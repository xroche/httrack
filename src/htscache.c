/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998 Xavier Roche and other contributors

SPDX-License-Identifier: GPL-3.0-or-later

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.

Ethical use: we kindly ask that you NOT use this software to harvest email
addresses or to collect any other private information about people. Doing so
would dishonor our work and waste the many hours we have spent on it.

Please visit our Website: http://www.httrack.com
*/

/* ------------------------------------------------------------ */
/* File: httrack.c subroutines:                                 */
/*       cache system (index and stores files in cache)         */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Internal engine bytecode */
#define HTS_INTERNAL_BYTECODE

#include "htscache.h"

/* specific definitions */
#include "htscore.h"
#include "htsbasenet.h"
#include "htsmd5.h"
#include <limits.h>
#include <time.h>

#include "htszlib.h"
/* END specific definitions */

/* Cache backend (zip-based since 3.31; the pre-3.31 .dat/.ndx import was
   removed, such caches are detected and refused in cache_init). */

void cache_mayadd(httrackp * opt, cache_back * cache, htsblk * r,
                  const char *url_adr, const char *url_fil,
                  const char *url_save) {
  hts_log_print(opt, LOG_DEBUG, "File checked by cache: %s", url_adr);
  // ---stockage en cache---
  // stocker dans le cache?
  if (opt->cache) {
    if (cache_writable(cache)) {
      // ensure not a temporary filename (should not happend ?!)
      if (IS_DELAYED_EXT(url_save)) {
        hts_log_print(opt, LOG_WARNING,
                      "aborted cache validation: %s%s still has temporary name %s",
                      url_adr, url_fil, url_save);
        return;
      }
      // c'est le seul endroit ou l'on ajoute des elements dans le cache (fichier entier ou header)
      // on stocke tout fichier "ok", mais également les réponses 404,301,302...
      if (
#if 1
           r->statuscode > 0
#else
           /* We don't store 5XX errors, because it might be a server problem */
           (r->statuscode == HTTP_OK)   /* stocker réponse standard, plus */
           ||(r->statuscode == 204)     /* no content */
           ||HTTP_IS_REDIRECT(r->statuscode)    /* redirect */
           ||(r->statuscode == 401)     /* authorization */
           ||(r->statuscode == 403)     /* unauthorized */
           ||(r->statuscode == 404)     /* not found */
           ||(r->statuscode == 410)     /* gone */
#endif
        ) {                     /* ne pas stocker si la page générée est une erreur */
        if (!r->is_file) {
          // stocker fichiers (et robots.txt)
          if (url_save == NULL || (strnotempty(url_save))
              || (strcmp(url_fil, "/robots.txt") == 0)) {
            // ajouter le fichier au cache
            cache_add(opt, cache, r, url_adr, url_fil, url_save,
                      opt->all_in_cache, StringBuff(opt->path_html_utf8));
            //
            // store a reference NOT to redo the same test zillions of times!
            // (problem reported by Lars Clausen)
            // we just store statuscode + location (if any)
            if (url_save == NULL && r->statuscode / 100 >= 3) {
              // cached "fast" header doesn't yet exists
              if (coucal_read
                  (cache->cached_tests,
                   concat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), url_adr, url_fil), NULL) == 0) {
                char BIGSTK tempo[HTS_URLMAXSIZE * 2];

                sprintf(tempo, "%d", (int) r->statuscode);
                if (r->location != NULL && r->location[0] != '\0') {
                  strcatbuff(tempo, "\n");
                  strcatbuff(tempo, r->location);
                }
                hts_log_print(opt, LOG_DEBUG,
                              "Cached fast-header response: %s%s is %d",
                              url_adr, url_fil, (int) r->statuscode);
                coucal_add(cache->cached_tests,
                            concat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), url_adr, url_fil),
                            (intptr_t) strdupt(tempo));
              }
            }
          }
        }
      }
    }
  }
  // ---fin stockage en cache---
}

#define ZIP_FIELD_STRING(headers, headersSize, field, value) do { \
  if ( (value != NULL) && (value)[0] != '\0') { \
    sprintf(headers + headersSize, "%s: %s\r\n", field, (value != NULL) ? (value) : ""); \
    (headersSize) += (int) strlen(headers + headersSize); \
  } \
} while(0)
#define ZIP_FIELD_INT(headers, headersSize, field, value) do { \
  if ( (value != 0) ) { \
    sprintf(headers + headersSize, "%s: "LLintP"\r\n", field, (LLint)(value)); \
    (headersSize) += (int) strlen(headers + headersSize); \
  } \
} while(0)
#define ZIP_FIELD_INT_FORCE(headers, headersSize, field, value) do { \
  sprintf(headers + headersSize, "%s: "LLintP"\r\n", field, (LLint)(value)); \
  (headersSize) += (int) strlen(headers + headersSize); \
} while(0)

struct cache_back_zip_entry {
  unsigned long int hdrPos;
  unsigned long int size;
  int compressionMethod;
};

#define ZIP_READFIELD_STRING(line, value, refline, refvalue, refvalue_size)    \
  do {                                                                         \
    if (line[0] != '\0' && strfield2(line, refline)) {                         \
      strlcpybuff(refvalue, value, refvalue_size);                             \
      line[0] = '\0';                                                          \
    }                                                                          \
  } while (0)
#define ZIP_READFIELD_INT(line, value, refline, refvalue) do { \
  if (line[0] != '\0' && strfield2(line, refline)) { \
    int intval = 0; \
    sscanf(value, "%d", &intval); \
    (refvalue) = intval; \
    line[0] = '\0'; \
	} \
} while(0)
#define ZIP_READFIELD_LLINT(line, value, refline, refvalue) do { \
  if (line[0] != '\0' && strfield2(line, refline)) { \
    LLint intval = 0; \
    sscanf(value, LLintP, &intval); \
    (refvalue) = intval; \
    line[0] = '\0'; \
	} \
} while(0)

/* Consecutive entry write failures before the cache stream is declared dead. */
#define CACHE_MAX_WRITE_FAILURES 8

/* Cache write failed: a fatal errno or a failure streak aborts the mirror
   (exit_xh); an isolated failure only drops the current entry. */
static void cache_zip_write_failed(httrackp *opt, cache_back *cache,
                                   const char *what, int zErr,
                                   hts_boolean entry_open, const char *url_adr,
                                   const char *url_fil) {
  const int fatal_errno = zErr == ZIP_ERRNO && check_fatal_io_errno();

  cache->zipWriteFailures++;
  if (fatal_errno || cache->zipWriteFailures >= CACHE_MAX_WRITE_FAILURES) {
    if (!cache->zipWriteFailed) {
      cache->zipWriteFailed = HTS_TRUE;
      if (fatal_errno) {
        hts_log_print(opt, LOG_ERROR,
                      "Mirror aborted: disk full or filesystem problems");
      } else {
        hts_log_print(opt, LOG_ERROR,
                      "Mirror aborted: cache write failed (%s): %s", what,
                      hts_get_zerror(zErr));
      }
    }
    opt->state.exit_xh = -1; /* fatal: stop the mirror, exit non-zero */
  } else {
    if (entry_open)
      zipCloseFileInZip((zipFile) cache->zipOutput); /* abandon, best-effort */
    hts_log_print(opt, LOG_WARNING,
                  "cache write failed (%s: %s), entry not cached: %s%s", what,
                  hts_get_zerror(zErr), url_adr, url_fil);
  }
}

/* Ajout d'un fichier en cache */
void cache_add(httrackp * opt, cache_back * cache, const htsblk * r,
               const char *url_adr, const char *url_fil, const char *url_save,
               int all_in_cache, const char *path_prefix) {
  char BIGSTK filename[HTS_URLMAXSIZE * 4];
  char catbuff[CATBUFF_SIZE];
  int dataincache = 0;          // put data in cache ?
  char BIGSTK headers[8192];
  int headersSize = 0;

  zip_fileinfo fi;
  const char *url_save_suffix = url_save;
  int zErr;

  /* already failed and aborting; don't touch the broken stream again */
  if (cache->zipWriteFailed)
    return;

  // robots.txt hack
  if (url_save == NULL) {
    dataincache = 0;            // testing links
  } else {
    if ((strnotempty(url_save) == 0)) {
      if (strcmp(url_fil, "/robots.txt") == 0)  // robots.txt
        dataincache = 1;
      else
        return;                 // error (except robots.txt)
    }

    /* Data in cache ? */
    if (is_hypertext_mime(opt, r->contenttype, url_fil)
        || (may_be_hypertext_mime(opt, r->contenttype, url_fil)
            && r->adr != NULL)
        /* store error messages! */
        || !HTTP_IS_OK(r->statuscode)
      ) {
      dataincache = 1;
    } else if (all_in_cache) {
      dataincache = 1;
    }
  }

  if (r->size < 0)              // error
    return;

  // data in cache: the body must fit the 32-bit zip write API
  if (dataincache && (LLint) (int) r->size != r->size) {
    if (r->is_write && url_save != NULL && strnotempty(url_save)) {
      hts_log_print(opt, LOG_WARNING,
                    "file too large for the cache, storing headers only: %s%s",
                    url_adr, url_fil);
      dataincache = 0;
    } else {
      hts_log_print(opt, LOG_WARNING,
                    "entry too large for the cache, not cached: %s%s", url_adr,
                    url_fil);
      return;
    }
  }

  /* Fields */
  headers[0] = '\0';
  headersSize = 0;
  /* */
  {
    const char *message;

    if (strlen(r->msg) < 32) {
      message = r->msg;
    } else {
      message = "(See X-StatusMessage)";
    }
    /* 64 characters MAX for first line */
    sprintf(headers + headersSize, "HTTP/1.%c %d %s\r\n", '1', r->statuscode,
            message);
  }
  headersSize += (int) strlen(headers + headersSize);

  if (path_prefix != NULL && path_prefix[0] != '\0' && url_save != NULL
      && url_save[0] != '\0') {
    int prefixLen = (int) strlen(path_prefix);

    if (strncmp(url_save, path_prefix, prefixLen) == 0) {
      url_save_suffix += prefixLen;
    }
  }

  /* Second line MUST ALWAYS be X-In-Cache */
  ZIP_FIELD_INT_FORCE(headers, headersSize, "X-In-Cache", dataincache);
  ZIP_FIELD_INT(headers, headersSize, "X-StatusCode", r->statuscode);
  ZIP_FIELD_STRING(headers, headersSize, "X-StatusMessage", r->msg);
  ZIP_FIELD_INT(headers, headersSize, "X-Size", r->size);       // size
  ZIP_FIELD_STRING(headers, headersSize, "Content-Type", r->contenttype);       // contenttype
  ZIP_FIELD_STRING(headers, headersSize, "X-Charset", r->charset);      // contenttype
  ZIP_FIELD_STRING(headers, headersSize, "Last-Modified", r->lastmodified);     // last-modified
  ZIP_FIELD_STRING(headers, headersSize, "Etag", r->etag);      // Etag
  ZIP_FIELD_STRING(headers, headersSize, "Location", r->location);      // 'location' pour moved
  ZIP_FIELD_STRING(headers, headersSize, "Content-Disposition", r->cdispo);     // Content-disposition
  ZIP_FIELD_STRING(headers, headersSize, "X-Addr", url_adr);    // Original address
  ZIP_FIELD_STRING(headers, headersSize, "X-Fil", url_fil);     // Original URI filename
  ZIP_FIELD_STRING(headers, headersSize, "X-Save", url_save_suffix);    // Original save filename

  /* Filename */
  if (!link_has_authority(url_adr)) {
    strcpybuff(filename, "http://");
  } else {
    strcpybuff(filename, "");
  }
  strcatbuff(filename, url_adr);
  strcatbuff(filename, url_fil);

  /* Time */
  memset(&fi, 0, sizeof(fi));
  if (r->lastmodified[0] != '\0') {
    struct tm buffer;
    struct tm *tm_s = convert_time_rfc822(&buffer, r->lastmodified);

    if (tm_s) {
      fi.tmz_date.tm_sec = (uInt) tm_s->tm_sec;
      fi.tmz_date.tm_min = (uInt) tm_s->tm_min;
      fi.tmz_date.tm_hour = (uInt) tm_s->tm_hour;
      fi.tmz_date.tm_mday = (uInt) tm_s->tm_mday;
      fi.tmz_date.tm_mon = (uInt) tm_s->tm_mon;
      fi.tmz_date.tm_year = (uInt) tm_s->tm_year;
    }
  }

  /* Open file - NOTE: headers in "comment" */
  if ((zErr = zipOpenNewFileInZip((zipFile) cache->zipOutput, filename, &fi,
                                  /* 
                                     Store headers in realtime in the local file directory as extra field
                                     In case of crash, we'll be able to recover the whole ZIP file by rescanning it
                                   */
                                  headers, (uInt) strlen(headers), NULL, 0, NULL,       /* comment */
                                  Z_DEFLATED, Z_DEFAULT_COMPRESSION)) != Z_OK) {
    cache_zip_write_failed(opt, cache, "opening a cache entry", zErr, HTS_FALSE,
                           url_adr, url_fil);
    return;
  }

  /* Write data in cache */
  if (dataincache) {
    if (r->is_write == 0) {
      if (r->size > 0 && r->adr != NULL) {
        if ((zErr =
             zipWriteInFileInZip((zipFile) cache->zipOutput, r->adr,
                                 (int) r->size)) != Z_OK) {
          cache_zip_write_failed(opt, cache, "writing to the cache", zErr,
                                 HTS_TRUE, url_adr, url_fil);
          return;
        }
      }
    } else {
      FILE *fp;

      // On recopie le fichier->.
      LLint file_size = fsize_utf8(fconv(catbuff, sizeof(catbuff), url_save));

      if (file_size >= 0) {
        fp = FOPEN(fconv(catbuff, sizeof(catbuff), url_save), "rb");
        if (fp != NULL) {
          char BIGSTK buff[32768];
          size_t nl;

          do {
            nl = fread(buff, 1, 32768, fp);
            if (nl > 0) {
              if ((zErr =
                   zipWriteInFileInZip((zipFile) cache->zipOutput, buff,
                                       (int) nl)) != Z_OK) {
                cache_zip_write_failed(opt, cache, "writing to the cache", zErr,
                                       HTS_TRUE, url_adr, url_fil);
                fclose(fp);
                return;
              }
            }
          } while(nl > 0);
          fclose(fp);
        } else {
          /* Err FIXME - lost file */
        }
      }                         /* Empty files are OK */
    }
  }

  /* Close */
  if ((zErr = zipCloseFileInZip((zipFile) cache->zipOutput)) != Z_OK) {
    cache_zip_write_failed(opt, cache, "closing a cache entry", zErr, HTS_FALSE,
                           url_adr, url_fil);
    return;
  }

  /* Flush */
  if ((zErr = zipFlush((zipFile) cache->zipOutput)) != 0) {
    cache_zip_write_failed(opt, cache, "flushing the cache", zErr, HTS_FALSE,
                           url_adr, url_fil);
    return;
  }

  cache->zipWriteFailures = 0; /* entry stored: reset the failure streak */
}

htsblk cache_read(httrackp * opt, cache_back * cache, const char *adr,
                  const char *fil, const char *save, char *location) {
  return cache_readex(opt, cache, adr, fil, save, location, NULL, 0);
}

htsblk cache_read_ro(httrackp * opt, cache_back * cache, const char *adr,
                     const char *fil, const char *save, char *location) {
  return cache_readex(opt, cache, adr, fil, save, location, NULL, 1);
}

htsblk cache_read_including_broken(httrackp *opt, cache_back *cache,
                                   const char *adr, const char *fil,
                                   char *return_save) {
  htsblk r = cache_readex(opt, cache, adr, fil, NULL, NULL, return_save, 0);

  if (r.statuscode == -1) {
    lien_back *itemback = NULL;

    if (back_unserialize_ref(opt, adr, fil, &itemback) == 0) {
      r = itemback->r;
      if (return_save != NULL)
        strlcpybuff(return_save, itemback->url_sav, HTS_URLMAXSIZE * 2);
      /* cleanup */
      back_clear_entry(itemback);       /* delete entry content */
      freet(itemback);          /* delete item */
      itemback = NULL;
      return r;
    }
  }
  return r;
}

static htsblk cache_readex_new(httrackp * opt, cache_back * cache,
                               const char *adr, const char *fil,
                               const char *save, char *location,
                               char *return_save, int readonly);

// lecture d'un fichier dans le cache
// si save==null alors test unqiquement
htsblk cache_readex(httrackp * opt, cache_back * cache, const char *adr,
                    const char *fil, const char *save, char *location,
                    char *return_save, int readonly) {
  if (cache->zipInput != NULL) {
    return cache_readex_new(opt, cache, adr, fil, save, location, return_save,
                            readonly);
  } else { /* no cache loaded */
    htsblk r;

    hts_init_htsblk(&r);
    strcpybuff(r.msg, "File Cache Entry Not Found");
    if (location != NULL) {
      r.location = location;
      r.location[0] = '\0';
    }
    return r;
  }
}

// lecture d'un fichier dans le cache
// si save==null alors test unqiquement
static htsblk cache_readex_new(httrackp * opt, cache_back * cache,
                               const char *adr, const char *fil,
                               const char *target_save, char *location,
                               char *return_save, int readonly) {
  char BIGSTK location_default[HTS_URLMAXSIZE * 2];
  char BIGSTK buff[HTS_URLMAXSIZE * 2];
  char BIGSTK previous_save[HTS_URLMAXSIZE * 2];
  char BIGSTK previous_save_[HTS_URLMAXSIZE * 2];
  char catbuff[CATBUFF_SIZE];
  intptr_t hash_pos;
  int hash_pos_return;
  htsblk r;

  hts_init_htsblk(&r);
  //memset(&r, 0, sizeof(htsblk)); r.soc=INVALID_SOCKET;
  location_default[0] = '\0';
  previous_save[0] = previous_save_[0] = '\0';

  if (location) {
    r.location = location;
  } else {
    r.location = location_default;
  }
  r.location[0] = '\0';
  strcpybuff(buff, adr);
  strcatbuff(buff, fil);
  hash_pos_return = coucal_read(cache->hashtable, buff, &hash_pos);
  /* avoid errors on data entries */
  if (adr[0] == '/' && adr[1] == '/' && adr[2] == '[') {
    hash_pos_return = 0;
  }

  if (hash_pos_return != 0) {
    uLong posInZip;

    if (hash_pos > 0) {
      posInZip = (uLong) hash_pos;
    } else {
      posInZip = (uLong) - hash_pos;
    }
    if (unzSetOffset((unzFile) cache->zipInput, posInZip) == Z_OK) {
      /* Read header (Max 8KiB) */
      if (unzOpenCurrentFile((unzFile) cache->zipInput) == Z_OK) {
        char BIGSTK headerBuff[8192 + 2];
        int readSizeHeader;

        int dataincache = 0;

        /* For BIG comments */
        headerBuff[0]
          = headerBuff[sizeof(headerBuff) - 1]
          = headerBuff[sizeof(headerBuff) - 2]
          = headerBuff[sizeof(headerBuff) - 3] = '\0';

        if ((readSizeHeader =
             unzGetLocalExtrafield((unzFile) cache->zipInput, headerBuff,
                                   sizeof(headerBuff) - 2)) > 0)
          /*if (unzGetCurrentFileInfo((unzFile) cache->zipInput, NULL,
             NULL, 0, NULL, 0, headerBuff, sizeof(headerBuff) - 2) == Z_OK ) */
        {
          int offset = 0;
          char BIGSTK line[HTS_URLMAXSIZE + 2];
          int lineEof = 0;

          /*readSizeHeader = (int) strlen(headerBuff); */
          headerBuff[readSizeHeader] = '\0';
          do {
            char *value;

            line[0] = '\0';
            offset += binput(headerBuff + offset, line, sizeof(line) - 2);
            if (line[0] == '\0') {
              lineEof = 1;
            }
            value = strchr(line, ':');
            if (value != NULL) {
              *value++ = '\0';
              if (*value == ' ' || *value == '\t')
                value++;
              ZIP_READFIELD_INT(line, value, "X-In-Cache", dataincache);
              ZIP_READFIELD_INT(line, value, "X-Statuscode", r.statuscode);
              ZIP_READFIELD_STRING(line, value, "X-StatusMessage", r.msg,
                                   sizeof(r.msg));
              ZIP_READFIELD_LLINT(line, value, "X-Size", r.size);       // size
              ZIP_READFIELD_STRING(line, value, "Content-Type", r.contenttype,
                                   sizeof(r.contenttype));
              ZIP_READFIELD_STRING(line, value, "X-Charset", r.charset,
                                   sizeof(r.charset));
              ZIP_READFIELD_STRING(line, value, "Last-Modified", r.lastmodified,
                                   sizeof(r.lastmodified));
              ZIP_READFIELD_STRING(line, value, "Etag", r.etag, sizeof(r.etag));
              // r.location is a char* pointing into a HTS_URLMAXSIZE*2 buffer
              ZIP_READFIELD_STRING(line, value, "Location", r.location,
                                   HTS_URLMAXSIZE * 2);
              ZIP_READFIELD_STRING(line, value, "Content-Disposition", r.cdispo,
                                   sizeof(r.cdispo));
              ZIP_READFIELD_STRING(line, value, "X-Save", previous_save_,
                                   sizeof(previous_save_));
            }
          } while (offset < readSizeHeader && !lineEof);

          /* Previous entry */
          if (previous_save_[0] != '\0') {
            int pathLen = (int) strlen(StringBuff(opt->path_html_utf8));

            if (pathLen != 0 && strncmp(previous_save_, StringBuff(opt->path_html_utf8), pathLen) != 0) {       // old (<3.40) buggy format
              sprintf(previous_save, "%s%s", StringBuff(opt->path_html_utf8),
                      previous_save_);
            } else {
              strcpy(previous_save, previous_save_);
            }
          }
          if (return_save != NULL) {
            strlcpybuff(return_save, previous_save, HTS_URLMAXSIZE * 2);
          }

          /* A negative X-Size is corrupt; so is one >= INT_MAX when the data
             is in the zip (the write path asserts int-sized). Headers-only
             entries legitimately exceed INT_MAX (>2GB body on disk): keep
             them, or every update would re-fetch the file. */
          if (r.size < 0 || (dataincache && r.size >= INT_MAX)) {
            r.statuscode = STATUSCODE_INVALID;
            strcpybuff(r.msg, "Cache Read Error : Bad Size");
          }

          /* Complete fields */
          r.totalsize = r.size;
          r.adr = NULL;
          r.out = NULL;
          r.fp = NULL;

          /* Do not get data ? Do some final tests. */
          if (target_save == NULL) {
            // si save==null, ne rien charger (juste en tête)
            if (r.statuscode == HTTP_OK && !is_hypertext_mime(opt, r.contenttype, fil)) {       // pas HTML, écrire sur disk directement
              r.is_write = 1;   /* supposed to be on disk (informational) */
            }

            /* Ensure the file is present, because returning a reference to a missing file is useless! */
            if (!dataincache) { /* Data are supposed to be on disk */
              if (!fexist_utf8(fconv(catbuff, sizeof(catbuff), previous_save))) {        // un fichier existe déja
                if (!opt->norecatch) {
                  hts_log_print(opt, LOG_DEBUG, "Cache: could not find %s",
                                previous_save);
                  r.statuscode = STATUSCODE_INVALID;
                  strcpybuff(r.msg, "Previous cache file not found");
                }
              }
            }                   // otherwise, the ZIP file is supposed to be consistent with data.
          }
          /* Read data ? */
          else if (r.statuscode !=
                   STATUSCODE_INVALID) { /* ne pas lire uniquement header */
            int ok = 0;

#if HTS_DIRECTDISK
            // Not ro, and pure data (not HTML and friends) to be saved now.
            if (!readonly && r.statuscode == HTTP_OK && !is_hypertext_mime(opt, r.contenttype, fil) && strnotempty(target_save)) {      // pas HTML, écrire sur disk directement
              r.is_write = 1;   // écrire

              // Data is supposed to be on disk
              if (!dataincache) {
                r.msg[0] = '\0';

                // File exists on disk with declared cache name (this is expected!)
                if (fexist_utf8(fconv(catbuff, sizeof(catbuff), previous_save))) {       // un fichier existe déja
                  // Expected size ?
                  const size_t fsize =
                    fsize_utf8(fconv(catbuff, sizeof(catbuff), previous_save));
                  if (fsize == r.size) {
                    // Target name is the previous name, and the file looks good: nothing to do!
                    if (strcmp(previous_save, target_save) == 0) {
                      // So far so good
                      ok = 1;   // plus rien à faire
                    }
                    // Different filenames: rename now!
                    else {
                      char catbuff2[CATBUFF_SIZE];

                      if (RENAME
                          (fconv(catbuff, sizeof(catbuff), previous_save),
                           fconv(catbuff2, sizeof(catbuff2), target_save)) == 0) {
                        // So far so good
                        ok = 1; // plus rien à faire

                        hts_log_print(opt, LOG_DEBUG,
                                      "File '%s' has been renamed since last mirror to '%s' ; applying changes",
                                      previous_save, target_save);
                      } else {
                        r.statuscode = STATUSCODE_INVALID;
                        strcpybuff(r.msg, "Unable to rename file on disk");
                      }
                    }
                  } else {
                    hts_log_print(opt, LOG_WARNING,
                                  "warning: file size on disk (" LLintP
                                  ") does not have the expected size (" LLintP
                                  "))", (LLint) fsize, (LLint) r.size);
                  }
                }
                // File exists with the target name and not previous one ?
                // Suppose a broken mirror, with a file being renamed: OK
                else if (fexist_utf8(fconv(catbuff, sizeof(catbuff), target_save))) {
                  // Expected size ?
                  const size_t fsize = fsize_utf8(fconv(catbuff, sizeof(catbuff), target_save));

                  if (fsize == r.size) {
                    // So far so good
                    ok = 1;     // plus rien à faire
                  } else {
                    hts_log_print(opt, LOG_WARNING,
                                  "warning: renamed file size on disk (" LLintP
                                  ") does not have the expected size (" LLintP
                                  "))", (LLint) fsize, (LLint) r.size);
                  }
                }
                // File is present and sane ?
                if (ok) {
                  // Register file not to wipe it later
                  filenote(&opt->state.strc, target_save, NULL);        // noter comme connu
                  file_notify(opt, adr, fil, target_save, 0, 0, 1);     // data in cache
                }
                // Nope
                else {
                  // Default behavior
                  if (!opt->norecatch) {
                    r.statuscode = STATUSCODE_INVALID;
                    if (r.msg[0] == '\0') {
                      strcpybuff(r.msg, "Previous cache file not found");
                    }
                  }
                  // Do not recatch broken/erased files
                  else {
                    file_notify(opt, adr, fil, target_save, 1, 0, 0);
                    filecreateempty(&opt->state.strc, target_save);
                    //
                    r.statuscode = STATUSCODE_INVALID;
                    strcpybuff(r.msg, "File deleted by user not recaught");
                  }
                }
              }
              // Load data from cache (not from a disk file)
              else {
                file_notify(opt, adr, fil, target_save, 1, 1, 1);       // data in cache
                r.out = filecreate(&opt->state.strc, target_save);
#if HDEBUG
                printf("direct-disk: %s\n", save);
#endif
                if (r.out != NULL) {
                  char BIGSTK buff[32768 + 4];
                  LLint size = r.size;

                  if (size > 0) {
                    size_t nl;

                    do {
                      nl =
                        unzReadCurrentFile((unzFile) cache->zipInput, buff,
                                           (int) minimum(size, 32768));
                      if (nl > 0) {
                        size -= nl;
                        if (fwrite(buff, 1, nl, r.out) != nl) { // erreur
                          int last_errno = errno;

                          r.statuscode = STATUSCODE_INVALID;
                          sprintf(r.msg, "Cache Read Error : Read To Disk: %s",
                                  strerror(last_errno));
                        }
                      }
                    } while((nl > 0) && (size > 0) && (r.statuscode != -1));
                  }

                  fclose(r.out);
                  r.out = NULL;
#ifndef _WIN32
                  chmod(target_save, HTS_ACCESS_FILE);
#endif
                } else {
                  r.statuscode = STATUSCODE_INVALID;
                  strcpybuff(r.msg,
                             "Cache Write Error : Unable to Create File");
                }
              }

            } else
#endif
            {                   // lire en mémoire

              // We need to get bytes on memory, but the previous version is (supposed to be) on disk.
              if (!dataincache) {
                // Empty previous save name, or file does not exist ?
                if (!strnotempty(previous_save) || !fexist_utf8(previous_save)) {       // Pas de donnée en cache, bizarre car html!!!
                  // Hack: if error page data is missing (pre-3.45 releases), create one. We won't use it anyway.
                  if (!HTTP_IS_OK(r.statuscode)) {
                    const int size = 512;

                    r.adr = malloct(size);
                    sprintf(r.adr,
                            "<html><!-- Missing Error Page ; Generated by HTTrack Website Copier --><head><title>HTTP Error %u</title></head><body><h1>HTTP Error %u</h1></body></html>",
                            r.statuscode, r.statuscode);
                    r.size = strlen(r.adr);
                    assertf(r.size < size);
                    strcpy(r.contenttype, "text/html");
                    strcpy(r.charset, "utf-8");
                    // Fake total size, too.
                    r.totalsize = r.size;
                    // Not on disk anymore!
                    r.is_write = 0;
                  }
                  // Otherwise, this is a real error.
                  else {
                    r.statuscode = STATUSCODE_INVALID;
                    strcpybuff(r.msg,
                               "Previous cache file not found (empty filename)");
                  }
                } else if (r.size >= INT_MAX) { /* too big to read in memory */
                  r.statuscode = STATUSCODE_INVALID;
                  strcpybuff(r.msg, "Cache Read Error : Bad Size");
                } else { /* Read in memory from disk */
                  FILE *const fp = FOPEN(fconv(catbuff, sizeof(catbuff), previous_save), "rb");

                  if (fp != NULL) {
                    r.adr = (char *) malloct((int) r.size + 1);
                    if (r.adr != NULL) {
                      if (r.size > 0
                          && fread(r.adr, 1, (int) r.size, fp) != r.size) {
                        int last_errno = errno;

                        r.statuscode = STATUSCODE_INVALID;
                        sprintf(r.msg, "Read error in cache disk data: %s",
                                strerror(last_errno));
                      } else if (r.size >= 0)
                        *(r.adr + r.size) = '\0';
                    } else {
                      r.statuscode = STATUSCODE_INVALID;
                      strcpybuff(r.msg,
                                 "Read error (memory exhausted) from cache");
                    }
                    fclose(fp);
                  } else {
                    r.statuscode = STATUSCODE_INVALID;
                    strcpybuff(r.msg,
                               "Read error (unable to open disk file) from cache");
                  }
                }
              }
              // Data in cache.
              else {
                // lire fichier (d'un coup)
                r.adr = (char *) malloct((int) r.size + 1);
                if (r.adr != NULL) {
                  if (unzReadCurrentFile((unzFile) cache->zipInput, r.adr, (int) r.size) != r.size) {   // erreur
                    freet(r.adr);
                    r.adr = NULL;
                    r.statuscode = STATUSCODE_INVALID;
                    strcpybuff(r.msg, "Cache Read Error : Read Data");
                  } else
                    *(r.adr + r.size) = '\0';
                } else {        // erreur
                  r.statuscode = STATUSCODE_INVALID;
                  strcpybuff(r.msg, "Cache Memory Error");
                }
              }
            }
          }

        } else {
          r.statuscode = STATUSCODE_INVALID;
          strcpybuff(r.msg, "Cache Read Error : Read Header Data");
        }
        unzCloseCurrentFile((unzFile) cache->zipInput);
      } else {
        r.statuscode = STATUSCODE_INVALID;
        strcpybuff(r.msg, "Cache Read Error : Open File");
      }

    } else {
      r.statuscode = STATUSCODE_INVALID;
      strcpybuff(r.msg, "Cache Read Error : Bad Offset");
    }
  } else {
    r.statuscode = STATUSCODE_INVALID;
    strcpybuff(r.msg, "File Cache Entry Not Found");
  }
  if (!location) {              /* don't export internal buffer */
    r.location = NULL;
  }
  return r;
}

// lecture d'un fichier dans le cache
// si save==null alors test unqiquement
static int hts_rename(httrackp * opt, const char *a, const char *b) {
  hts_log_print(opt, LOG_DEBUG, "Cache: rename %s -> %s (%p %p)", a, b, a, b);
  return RENAME(a, b);
}

/* minizip callbacks that open through hts_fopen_utf8, so the cache ZIP follows
   a non-ASCII path_log instead of landing in an ANSI-mangled twin (#630). On
   POSIX hts_fopen_utf8 is fopen, so this is a no-op off Windows. */
static voidpf ZCALLBACK hts_zip_fopen_utf8(voidpf opaque, const char *filename,
                                           int mode) {
  const char *mode_fopen = NULL;

  (void) opaque;
  if ((mode & ZLIB_FILEFUNC_MODE_READWRITEFILTER) == ZLIB_FILEFUNC_MODE_READ)
    mode_fopen = "rb";
  else if (mode & ZLIB_FILEFUNC_MODE_EXISTING)
    mode_fopen = "r+b";
  else if (mode & ZLIB_FILEFUNC_MODE_CREATE)
    mode_fopen = "wb";
  if (filename == NULL || mode_fopen == NULL)
    return NULL;
  return (voidpf) FOPEN(filename, mode_fopen);
}

static unzFile hts_unzOpen_utf8(const char *path) {
  zlib_filefunc_def ff;

  fill_fopen_filefunc(&ff);
  ff.zopen_file = hts_zip_fopen_utf8;
  return unzOpen2(path, &ff);
}

static zipFile hts_zipOpen_utf8(const char *path, int append) {
  zlib_filefunc_def ff;

  fill_fopen_filefunc(&ff);
  ff.zopen_file = hts_zip_fopen_utf8;
  return zipOpen2(path, append, NULL, &ff);
}

/* Pathname of a file inside the mirror dir (rotating concat buffer). */
static char *reconcile_path(httrackp *opt, const char *name) {
  return fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                 StringBuff(opt->path_log), name);
}

/* Interrupted-run heuristic: prefer the old generation when the new cache
   stalled below NEW_TINY while the old one grew past OLD_SOLID (historical
   arbitrary thresholds). */
#define CACHE_RECONCILE_NEW_TINY 32768
#define CACHE_RECONCILE_OLD_SOLID 65536

/* Replace the new-generation file by the old one, when the old one exists. */
static void reconcile_promote(httrackp *opt, const char *oldname,
                              const char *newname) {
  if (fexist_utf8(reconcile_path(opt, oldname))) {
    UNLINK(reconcile_path(opt, newname));
    RENAME(reconcile_path(opt, oldname), reconcile_path(opt, newname));
  }
}

void hts_cache_reconcile(httrackp *opt, hts_cache_reconcile_mode mode) {
  switch (mode) {
  case CACHE_RECONCILE_PROMOTE:
    /* Previous run rotated new.* to old.* then died before writing: promote
       the old generation back, whichever format it uses. */
    if (!fexist_utf8(reconcile_path(opt, "hts-cache/new.zip")))
      reconcile_promote(opt, "hts-cache/old.zip", "hts-cache/new.zip");
    break;
  case CACHE_RECONCILE_INTERRUPTED:
    /* Aborted run: keep the larger generation when the new cache is
       suspiciously small next to the old one. The new file must exist: fsize()
       is -1 for a missing file, which would spuriously pass the "< TINY" test
       and overwrite a solid old generation that PROMOTE/ROLLBACK should keep.
     */
    if (!opt->cache ||
        !fexist_utf8(reconcile_path(opt, "hts-in_progress.lock")))
      break;
    if (fexist_utf8(reconcile_path(opt, "hts-cache/new.zip")) &&
        fexist_utf8(reconcile_path(opt, "hts-cache/old.zip")) &&
        fsize_utf8(reconcile_path(opt, "hts-cache/new.zip")) <
            CACHE_RECONCILE_NEW_TINY &&
        fsize_utf8(reconcile_path(opt, "hts-cache/old.zip")) >
            CACHE_RECONCILE_OLD_SOLID &&
        fsize_utf8(reconcile_path(opt, "hts-cache/old.zip")) >
            fsize_utf8(reconcile_path(opt, "hts-cache/new.zip")))
      reconcile_promote(opt, "hts-cache/old.zip", "hts-cache/new.zip");
    break;
  case CACHE_RECONCILE_ROLLBACK:
    /* Nothing transferred: restore the previous generation and sidecars. */
    reconcile_promote(opt, "hts-cache/old.zip", "hts-cache/new.zip");
    reconcile_promote(opt, "hts-cache/old.lst", "hts-cache/new.lst");
    reconcile_promote(opt, "hts-cache/old.txt", "hts-cache/new.txt");
    break;
  }
}

// renvoyer uniquement en tête, ou NULL si erreur
// return NULL upon error, and set -1 to r.statuscode
htsblk *cache_header(httrackp * opt, cache_back * cache, const char *adr,
                     const char *fil, htsblk * r) {
  *r = cache_read(opt, cache, adr, fil, NULL, NULL);    // test uniquement
  if (r->statuscode != -1)
    return r;
  else
    return NULL;
}

// Initialisation du cache: créer nouveau, renomer ancien, charger..
void cache_init(cache_back * cache, httrackp * opt) {
  // ---
  // utilisation du cache: renommer ancien éventuel et charger index
  hts_log_print(opt, LOG_DEBUG, "Cache: enabled=%d, base=%s, ro=%d",
                (int) opt->cache, fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                                          StringBuff(opt->path_log),
                                          "hts-cache/"), (int) cache->ro);
  if (opt->cache) {
#if DEBUGCA
    printf("cache init: ");
#endif
    if (!cache->ro) {
      MKDIR(fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                    StringBuff(opt->path_log), "hts-cache"));
      if ((fexist_utf8(fconcat(
              OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
              StringBuff(opt->path_log),
              "hts-cache/new.zip")))) { // a previous cache exists.. rename it
        /* Remove OLD cache */
        if (fexist_utf8(fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                                StringBuff(opt->path_log),
                                "hts-cache/old.zip"))) {
          if (UNLINK(fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                             StringBuff(opt->path_log), "hts-cache/old.zip")) !=
              0) {
            hts_log_print(opt, LOG_WARNING | LOG_ERRNO,
                          "Cache: error while moving previous cache");
          }
        }

        /* Rename */
        if (hts_rename
            (opt,
             fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                     "hts-cache/new.zip"), fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                                                   StringBuff(opt->path_log),
                                                   "hts-cache/old.zip")) != 0) {
          hts_log_print(opt, LOG_WARNING | LOG_ERRNO,
                        "Cache: error while moving previous cache");
        } else {
          hts_log_print(opt, LOG_DEBUG, "Cache: successfully renamed");
        }
      }
    } else {
      hts_log_print(opt, LOG_DEBUG, "Cache: no cache found");
    }
    hts_log_print(opt, LOG_DEBUG, "Cache: size %d",
                  (int) fsize_utf8(
                      fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                              StringBuff(opt->path_log), "hts-cache/old.zip")));

    // charger index cache précédent
    if ((!cache->ro &&
         fsize_utf8(fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                            StringBuff(opt->path_log), "hts-cache/old.zip")) >
             0) ||
        (cache->ro &&
         fsize_utf8(fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                            StringBuff(opt->path_log), "hts-cache/new.zip")) >
             0)) {
      if (!cache->ro) {
        cache->zipInput = hts_unzOpen_utf8(
            fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                    StringBuff(opt->path_log), "hts-cache/old.zip"));
      } else {
        cache->zipInput = hts_unzOpen_utf8(
            fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                    StringBuff(opt->path_log), "hts-cache/new.zip"));
      }

      // Corrupted ZIP file ? Try to repair!
      if (cache->zipInput == NULL && !cache->ro) {
        char *name;
        uLong repaired = 0;
        uLong repairedBytes = 0;

        if (!cache->ro) {
          name =
            fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                    "hts-cache/old.zip");
        } else {
          name =
            fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                    "hts-cache/new.zip");
        }
        hts_log_print(opt, LOG_WARNING,
                      "Cache: damaged cache, trying to repair");
        /* mztools has no UTF-8 hook: repairing a corrupt cache under a
           non-ASCII path_log fails cleanly on Windows (re-crawl), it never
           forks a twin. */
        if (unzRepair
            (name,
             fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                     "hts-cache/repair.zip"), fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                                                      StringBuff(opt->path_log),
                                                      "hts-cache/repair.tmp"),
             &repaired, &repairedBytes) == Z_OK) {
          UNLINK(name);
          RENAME(fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                         StringBuff(opt->path_log), "hts-cache/repair.zip"),
                 name);
          cache->zipInput = hts_unzOpen_utf8(name);
          hts_log_print(opt, LOG_WARNING,
                        "Cache: %d bytes successfully recovered in %d entries",
                        (int) repairedBytes, (int) repaired);
        } else {
          hts_log_print(opt, LOG_WARNING, "Cache: could not repair the cache");
        }
      }
      // Opened ?
      if (cache->zipInput != NULL) {
        int zErr;

        /* Ready directory entries */
        if ((zErr = unzGoToFirstFile((unzFile) cache->zipInput)) == Z_OK) {
          char comment[128];
          char BIGSTK filename[HTS_URLMAXSIZE * 4];
          int entries = 0;

          memset(comment, 0, sizeof(comment));  // for truncated reads
          do {
            int readSizeHeader = 0;

            filename[0] = '\0';
            comment[0] = '\0';
            if (unzOpenCurrentFile((unzFile) cache->zipInput) == Z_OK) {
              if ((readSizeHeader =
                   unzGetLocalExtrafield((unzFile) cache->zipInput, comment,
                                         sizeof(comment) - 2)) > 0
                  && unzGetCurrentFileInfo((unzFile) cache->zipInput, NULL,
                                           filename, sizeof(filename) - 2, NULL,
                                           0, NULL, 0) == Z_OK) {
                long int pos =
                  (long int) unzGetOffset((unzFile) cache->zipInput);
                assertf(readSizeHeader < sizeof(comment));
                comment[readSizeHeader] = '\0';
                entries++;
                if (pos > 0) {
                  int dataincache = 0;  // data in cache ?
                  char *filenameIndex = filename;

                  if (strfield(filenameIndex, "http://")) {
                    filenameIndex += 7;
                  }
                  if (comment[0] != '\0') {
                    int maxLine = 2;
                    char *a = comment;

                    while(*a && maxLine-- > 0) {        // parse only few first lines
                      char BIGSTK line[1024];

                      line[0] = '\0';
                      a += binput(a, line, sizeof(line) - 2);
                      if (strfield(line, "X-In-Cache:")) {
                        if (strfield2(line, "X-In-Cache: 1")) {
                          dataincache = 1;
                        } else {
                          dataincache = 0;
                        }
                        break;
                      }
                    }
                  }
                  if (dataincache)
                    coucal_add(cache->hashtable, filenameIndex, pos);
                  else
                    coucal_add(cache->hashtable, filenameIndex, -pos);
                } else {
                  hts_log_print(opt, LOG_WARNING,
                                "Corrupted cache meta entry #%d",
                                (int) entries);
                }
              } else {
                hts_log_print(opt, LOG_WARNING, "Corrupted cache entry #%d",
                              (int) entries);
              }
              unzCloseCurrentFile((unzFile) cache->zipInput);
            } else {
              hts_log_print(opt, LOG_WARNING, "Corrupted cache entry #%d",
                            (int) entries);
            }
          } while(unzGoToNextFile((unzFile) cache->zipInput) == Z_OK);
          hts_log_print(opt, LOG_DEBUG, "Cache index loaded: %d entries loaded",
                        (int) entries);
          opt->is_update = 1;   // signaler comme update

        } else {
          hts_log_print(opt, LOG_WARNING,
                        "Cache: error trying to read the cache: %s",
                        hts_get_zerror(zErr));
        }

      } else {
        hts_log_print(opt, LOG_WARNING,
                      "Cache: error trying to open the cache");
      }

    } else if (fsize_utf8(reconcile_path(opt, "hts-cache/old.ndx")) > 0 ||
               fsize_utf8(reconcile_path(opt, "hts-cache/new.ndx")) > 0) {
      /* pre-3.31 (2003) .dat/.ndx cache: import support removed */
      hts_log_print(opt, LOG_ERROR,
                    "Cache: the pre-3.31 .dat/.ndx cache format is no longer "
                    "supported; ignoring it (the site will be re-crawled)");
    } else {
      hts_log_print(opt, LOG_DEBUG, "Cache: no cache found in %s",
                    fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                            "hts-cache/"));
    }

#if DEBUGCA
    printf("..create cache\n");
#endif
    if (!cache->ro) {
      // ouvrir caches actuels
      structcheck_utf8(fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                               StringBuff(opt->path_log), "hts-cache/"));

      {
        /* Create ZIP file cache */
        cache->zipOutput = (void *) hts_zipOpen_utf8(
            fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                    StringBuff(opt->path_log), "hts-cache/new.zip"),
            0);

        if (cache->zipOutput != NULL) {
          // supprimer old.lst
          if (fexist_utf8(fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                                  StringBuff(opt->path_log),
                                  "hts-cache/old.lst")))
            UNLINK(fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                           StringBuff(opt->path_log), "hts-cache/old.lst"));
          // renommer
          if (fexist_utf8(fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                                  StringBuff(opt->path_log),
                                  "hts-cache/new.lst")))
            RENAME(fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                           StringBuff(opt->path_log), "hts-cache/new.lst"),
                   fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                           StringBuff(opt->path_log), "hts-cache/old.lst"));
          // ouvrir
          cache->lst =
              FOPEN(fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                            StringBuff(opt->path_log), "hts-cache/new.lst"),
                    "wb");
          strcpybuff(opt->state.strc.path, StringBuff(opt->path_html));
          opt->state.strc.lst = cache->lst;

          // supprimer old.txt
          if (fexist_utf8(fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                                  StringBuff(opt->path_log),
                                  "hts-cache/old.txt")))
            UNLINK(fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                           StringBuff(opt->path_log), "hts-cache/old.txt"));
          // renommer
          if (fexist_utf8(fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                                  StringBuff(opt->path_log),
                                  "hts-cache/new.txt")))
            RENAME(fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                           StringBuff(opt->path_log), "hts-cache/new.txt"),
                   fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                           StringBuff(opt->path_log), "hts-cache/old.txt"));
          // ouvrir
          cache->txt =
              FOPEN(fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                            StringBuff(opt->path_log), "hts-cache/new.txt"),
                    "wb");
          if (cache->txt) {
            fprintf(cache->txt,
                    "date\tsize'/'remotesize\tflags(request:Update,Range state:File response:Modified,Chunked,gZipped)\t");
            fprintf(cache->txt,
                    "statuscode\tstatus ('servermsg')\tMIME\tEtag|Date\tURL\tlocalfile\t(from URL)"
                    LF);
          }
        }
      }

    } else {
      cache->lst = NULL;
    }

  } else {
    hts_log_print(opt, LOG_DEBUG, "Cache: no cache enabled");
  }

}

// lire un fichier.. (compatible \0)
/* Note: NOT utf-8 */
char *readfile(const char *fil) {
  return readfile2(fil, NULL);
}

/* Note: NOT utf-8 */
char *readfile2(const char *fil, LLint * size) {
  char *adr = NULL;
  char catbuff[CATBUFF_SIZE];
  const LLint len = fsize(fil);

  /* a size too large for size_t (32-bit Windows/i386) must fail closed: it
     would wrap malloct() short while fread() still read the untruncated len */
  const size_t buflen = len >= 0 ? llint_to_size_t(len) : (size_t) -1;

  if (buflen != (size_t) -1) { // exists, and is addressable
    FILE *fp;

    fp = fopen(fconv(catbuff, sizeof(catbuff), fil), "rb");
    if (fp != NULL) {           // n'existe pas (!)
      adr = (char *) malloct(buflen + 1);
      if (size != NULL)
        *size = len;
      if (adr != NULL) {
        if (buflen > 0 &&
            fread(adr, 1, buflen, fp) != buflen) { // fichier endommagé ?
          freet(adr);
          adr = NULL;
        } else
          *(adr + buflen) = '\0';
      }
      fclose(fp);
    }
  }
  return adr;
}

/* Note: utf-8 */
char *readfile_utf8(const char *fil) {
  char *adr = NULL;
  char catbuff[CATBUFF_SIZE];
  const LLint len = fsize_utf8(fil);
  const size_t buflen = len >= 0 ? llint_to_size_t(len) : (size_t) -1;

  if (buflen != (size_t) -1) { // exists, and is addressable (see readfile2)
    FILE *const fp = FOPEN(fconv(catbuff, sizeof(catbuff), fil), "rb");

    if (fp != NULL) {           // n'existe pas (!)
      adr = (char *) malloct(buflen + 1);
      if (adr != NULL) {
        if (buflen > 0 &&
            fread(adr, 1, buflen, fp) != buflen) { // fichier endommagé ?
          freet(adr);
          adr = NULL;
        } else {
          adr[buflen] = '\0';
        }
      }
      fclose(fp);
    }
  }
  return adr;
}

/* Note: NOT utf-8 */
char *readfile_or(const char *fil, const char *defaultdata) {
  const char *realfile = fil;
  char *ret;
  char catbuff[CATBUFF_SIZE];

  if (!fexist(fil))
    realfile = fconcat(catbuff, sizeof(catbuff), hts_rootdir(NULL), fil);
  ret = readfile(realfile);
  if (ret)
    return ret;
  else {
    char *adr = malloct(strlen(defaultdata) + 1);

    if (adr) {
      strlcpybuff(adr, defaultdata, strlen(defaultdata) + 1);
      return adr;
    }
  }
  return NULL;
}

// écriture/lecture d'une chaîne sur un fichier
// -1 : erreur, sinon 0
int cache_wstr(FILE * fp, const char *s) {
  INTsys i;
  char buff[256 + 4];

  i = (s != NULL) ? ((INTsys) strlen(s)) : 0;
  sprintf(buff, INTsysP "\n", i);
  if (fwrite(buff, 1, strlen(buff), fp) != strlen(buff))
    return -1;
  if (i > 0 && fwrite(s, 1, i, fp) != i)
    return -1;
  return 0;
}
void cache_rstr(FILE *fp, char *s, size_t s_size) {
  INTsys i;
  char buff[256 + 4];

  linput(fp, buff, 256);
  sscanf(buff, INTsysP, &i);
  if (i < 0 || i > 32768)       /* error, something nasty happened */
    i = 0;
  if (i > 0) {
    /* Store at most s_size-1 bytes into s, but consume all i bytes from the
       stream so the next field stays aligned (the field may be longer than the
       destination in a tampered/old cache). */
    const size_t want = (size_t) i;
    const size_t store = want < s_size ? want : s_size - 1;

    if (fread(s, 1, store, fp) != store) {
      int fread_cache_failed = 0;

      assertf(fread_cache_failed);
    }
    if (want > store && fseek(fp, (long) (want - store), SEEK_CUR) != 0) {
      int fseek_cache_failed = 0;

      assertf(fseek_cache_failed);
    }
    s[store] = '\0';
  } else {
    s[0] = '\0';
  }
}
char *cache_rstr_addr(FILE * fp) {
  INTsys i;
  char *addr = NULL;
  char buff[256 + 4];

  linput(fp, buff, 256);
  sscanf(buff, INTsysP, &i);
  if (i < 0 || i > 32768)       /* error, something nasty happened */
    i = 0;
  if (i > 0) {
    addr = malloct(i + 1);
    if (addr != NULL) {
      if ((int) fread(addr, 1, i, fp) != i) {
        int fread_cache_failed = 0;

        assertf(fread_cache_failed);
      }
      *(addr + i) = '\0';
    }
  }
  return addr;
}
int cache_brstr(char *adr, char *s, size_t s_size) {
  int i;
  int off;
  char buff[256 + 4];

  off = binput(adr, buff, 256);
  /* binput stops at the buffer's terminating NUL; a value can only follow a
     real line terminator, so never step past end-of-buffer. */
  if (adr[off - 1] == '\0') {
    s[0] = '\0';
    return off - 1;
  }
  /* an empty/non-numeric field leaves i unset: treat as length 0 */
  if (sscanf(buff, "%d", &i) != 1 || i < 0 || i > 32768)
    i = 0;
  if (i > 0) {
    /* A corrupt/truncated cache may declare a length past the buffer end;
       bound both the copy and the advance to the bytes actually present. */
    const size_t avail = strnlen(adr + off, (size_t) i);
    const size_t store = avail < s_size ? avail : s_size - 1;

    memcpy(s, adr + off, store);
    s[store] = '\0';
    off += (int) avail;
  } else {
    s[0] = '\0';
  }
  return off;
}

/* binput bounded to a NUL-terminated buffer: refuse to start a read at or
   past `end`, so a prior over-advance can't walk a cache-index parse OOB. */
int cache_binput(char *adr, const char *end, char *s, int max) {
  if (adr >= end) {
    s[0] = '\0';
    return 0;
  }
  return binput(adr, s, max);
}

/* idem, mais en int */
int cache_brint(char *adr, int *i) {
  char s[256];
  int r = cache_brstr(adr, s, sizeof(s));

  if (r != -1)
    sscanf(s, "%d", i);
  return r;
}
void cache_rint(FILE * fp, int *i) {
  char s[256];

  cache_rstr(fp, s, sizeof(s));
  sscanf(s, "%d", i);
}
int cache_wint(FILE * fp, int i) {
  char s[256];

  sprintf(s, "%d", (int) i);
  return cache_wstr(fp, s);
}
void cache_rLLint(FILE * fp, LLint * i) {
  char s[256];

  cache_rstr(fp, s, sizeof(s));
  sscanf(s, LLintP, i);
}
int cache_wLLint(FILE * fp, LLint i) {
  char s[256];

  sprintf(s, LLintP, (LLint) i);
  return cache_wstr(fp, s);
}

// -- cache --
