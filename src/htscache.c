/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998-2015 Xavier Roche and other contributors

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

Important notes:

- We hereby ask people using this source NOT to use it in purpose of grabbing
emails addresses, or collecting any other private information on persons.
This would disgrace our work, and spoil the many hours we spent on it.

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
#include <time.h>

#include "htszlib.h"
/* END specific definitions */

// routines de mise en cache

/*
  VERSION 1.0 :
  -----------

.ndx file
 file with data
   <string>(date/time) [ <string>(hostname+filename) (datfile_position_ascii) ] * number_of_links
 file without data
   <string>(date/time) [ <string>(hostname+filename) (-datfile_position_ascii) ] * number_of_links

.dat file
 [ file ] * 
with
  file= (with data)
   [ bytes ] * sizeof(htsblk header) [ bytes ] * n(length of file given in htsblk header)
 file= (without data)
   [ bytes ] * sizeof(htsblk header)
with
 <string>(name) = <length in ascii>+<lf>+<data>

  VERSION 1.1/1.2 :
  ---------------

.ndx file
 file with data
   <string>("CACHE-1.1") <string>(date/time) [ <string>(hostname+filename) (datfile_position_ascii) ] * number_of_links
 file without data
   <string>("CACHE-1.1") <string>(date/time) [ <string>(hostname+filename) (-datfile_position_ascii) ] * number_of_links

.dat file
   <string>("CACHE-1.1") [ [Header_1.1] [bytes] * n(length of file given in header) ] *
with
 Header_1.1=
   <int>(statuscode)
   <int>(size)
   <string>(msg)
   <string>(contenttype)
   <string>(charset) [version 3]
   <string>(last-modified)
   <string>(Etag)
   <string>location
   <string>Content-disposition [version 2]
   <string>hostname [version 4]
   <string>URI filename [version 4]
   <string>local filename [version 4]
   [<string>"SD" <string>(supplemental data)]
   [<string>"SD" <string>(supplemental data)]
   ...
   <string>"HTS" (end of header)
   <int>(number of bytes of data) (0 if no data written)
*/

// Nouveau: si != text/html ne stocke que la taille

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

#if 1

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

#define ZIP_READFIELD_STRING(line, value, refline, refvalue) do { \
  if (line[0] != '\0' && strfield2(line, refline)) { \
    strcpybuff(refvalue, value); \
    line[0] = '\0'; \
	} \
} while(0)
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

/* Ajout d'un fichier en cache */
void cache_add(httrackp * opt, cache_back * cache, const htsblk * r,
               const char *url_adr, const char *url_fil, const char *url_save,
               int all_in_cache, const char *path_prefix) {
  char BIGSTK filename[HTS_URLMAXSIZE * 4];
  char catbuff[CATBUFF_SIZE];
  int dataincache = 0;          // put data in cache ?
  char BIGSTK headers[8192];
  int headersSize = 0;

  //int entryBodySize = 0;
  //int entryFilenameSize = 0;
  zip_fileinfo fi;
  const char *url_save_suffix = url_save;
  int zErr;

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

  // data in cache
  if (dataincache) {
    assertf(((int) r->size) == r->size);
    //entryBodySize = (int) r->size;
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

  //entryFilenameSize = (int) ( strlen(url_adr) + strlen(url_fil));

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
    int zip_zipOpenNewFileInZip_failed = 0;

    assertf(zip_zipOpenNewFileInZip_failed);
  }

  /* Write data in cache */
  if (dataincache) {
    if (r->is_write == 0) {
      if (r->size > 0 && r->adr != NULL) {
        if ((zErr =
             zipWriteInFileInZip((zipFile) cache->zipOutput, r->adr,
                                 (int) r->size)) != Z_OK) {
          int zip_zipWriteInFileInZip_failed = 0;

          assertf(zip_zipWriteInFileInZip_failed);
        }
      }
    } else {
      FILE *fp;

      // On recopie le fichier->.
      off_t file_size = fsize_utf8(fconv(catbuff, sizeof(catbuff), url_save));

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
                int zip_zipWriteInFileInZip_failed = 0;

                assertf(zip_zipWriteInFileInZip_failed);
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
    int zip_zipCloseFileInZip_failed = 0;

    assertf(zip_zipCloseFileInZip_failed);
  }

  /* Flush */
  if ((zErr = zipFlush((zipFile) cache->zipOutput)) != 0) {
    int zip_zipFlush_failed = 0;

    assertf(zip_zipFlush_failed);
  }
}

#else

/* Ajout d'un fichier en cache */
void cache_add(httrackp * opt, cache_back * cache, const htsblk * r,
               char *url_adr, char *url_fil, char *url_save, int all_in_cache) {
  int pos;
  char s[256];
  char BIGSTK buff[HTS_URLMAXSIZE * 4];
  int ok = 1;
  int dataincache = 0;          // donnée en cache?
  FILE *cache_ndx = cache->ndx;
  FILE *cache_dat = cache->dat;

  /*char digest[32+2]; */
  /*digest[0]='\0'; */

  // Longueur url_save==0?
  if ((strnotempty(url_save) == 0)) {
    if (strcmp(url_fil, "/robots.txt") == 0)    // robots.txt
      dataincache = 1;
    else if (strcmp(url_fil, "/test") == 0)     // testing links
      dataincache = 0;
    else
      return;                   // erreur (sauf robots.txt)
  }

  /*
     if (r->size <= 0)   // taille <= 0 
     return;          // refusé..
   */

  // Mettre les *donées* en cache ?
  if (is_hypertext_mime(opt, r->contenttype, url_fil))  // html, mise en cache des données et 
    dataincache = 1;            // pas uniquement de l'en tête
  else if (all_in_cache)
    dataincache = 1;            // forcer tout en cache

  /* calcul md5 ? */
  /*
     if (is_hypertext_mime(opt,r->contenttype)) {    // html, calcul MD5
     if (r->adr) {
     domd5mem(r->adr,r->size,digest,1);
     }
     } */

  // Position
  fflush(cache_dat);
  fflush(cache_ndx);
  pos = ftell(cache_dat);
  // écrire pointeur seek, adresse, fichier
  if (dataincache)              // patcher
    sprintf(s, "%d\n", pos);    // ecrire tel que (eh oui évite les \0..)
  else
    sprintf(s, "%d\n", -pos);   // ecrire tel que (eh oui évite les \0..)

  // data
  // écrire données en-tête, données fichier
  /*if (!dataincache) {   // patcher
     r->size=-r->size;  // négatif
     } */

  // Construction header
  ok = 0;
  if (cache_wint(cache_dat, r->statuscode) != -1        // statuscode
      && cache_wLLint(cache_dat, r->size) != -1 // size
      && cache_wstr(cache_dat, r->msg) != -1    // msg
      && cache_wstr(cache_dat, r->contenttype) != -1    // contenttype
      && cache_wstr(cache_dat, r->charset) != -1        // contenttype
      && cache_wstr(cache_dat, r->lastmodified) != -1   // last-modified
      && cache_wstr(cache_dat, r->etag) != -1   // Etag
      && cache_wstr(cache_dat, (r->location != NULL) ? r->location : "") != -1  // 'location' pour moved
      && cache_wstr(cache_dat, r->cdispo) != -1 // Content-disposition
      && cache_wstr(cache_dat, url_adr) != -1   // Original address
      && cache_wstr(cache_dat, url_fil) != -1   // Original URI filename
      && cache_wstr(cache_dat, url_save) != -1  // Original save filename
      && cache_wstr(cache_dat, r->headers) != -1        // Full HTTP Headers
      && cache_wstr(cache_dat, "HTS") != -1     // end of header
    ) {
    ok = 1;                     /* ok */
  }
  // Fin construction header

  /*if ((int) fwrite((char*) &r,1,sizeof(htsblk),cache_dat) == sizeof(htsblk)) { */
  if (ok) {
    if (dataincache) {          // mise en cache?
      if (!r->adr) {            /* taille nulle (parfois en cas de 301 */
        if (cache_wLLint(cache_dat, 0) == -1)   /* 0 bytes */
          ok = 0;
      } else if (r->is_write == 0) {    // en mémoire, recopie directe
        if (cache_wLLint(cache_dat, r->size) != -1) {
          if (r->size > 0) {    // taille>0
            if (fwrite(r->adr, 1, r->size, cache_dat) != r->size)
              ok = 0;
          } else                // taille=0, ne rien écrire
            ok = 0;
        } else
          ok = 0;
      } else {                  // recopier fichier dans cache
        FILE *fp;

        // On recopie le fichier->.
        off_t file_size = fsize_utf8(fconv(catbuff, url_save));

        if (file_size >= 0) {
          if (cache_wLLint(cache_dat, file_size) != -1) {
            fp = FOPEN(fconv(catbuff, url_save), "rb");
            if (fp != NULL) {
              char BIGSTK buff[32768];
              ssize_t nl;

              do {
                nl = fread(buff, 1, 32768, fp);
                if (nl > 0) {
                  if (fwrite(buff, 1, nl, cache_dat) != nl) {   // erreur
                    nl = -1;
                    ok = 0;
                  }
                }
              } while(nl > 0);
              fclose(fp);
            } else
              ok = 0;
          } else
            ok = 0;
        } else
          ok = 0;
      }
    } else {
      if (cache_wLLint(cache_dat, 0) == -1)     /* 0 bytes */
        ok = 0;
    }
  } else
    ok = 0;
  /*if (!dataincache) {   // dépatcher
     r->size=-r->size;
     } */

  // index
  // adresse+cr+fichier+cr
  if (ok) {
    buff[0] = '\0';
    strcatbuff(buff, url_adr);
    strcatbuff(buff, "\n");
    strcatbuff(buff, url_fil);
    strcatbuff(buff, "\n");
    cache_wstr(cache_ndx, buff);
    fwrite(s, 1, strlen(s), cache_ndx);
  }                             // si ok=0 on a peut être écrit des données pour rien mais on s'en tape

  // en cas de plantage, on aura au moins le cache!
  fflush(cache_dat);
  fflush(cache_ndx);
}

#endif

htsblk cache_read(httrackp * opt, cache_back * cache, const char *adr,
                  const char *fil, const char *save, char *location) {
  return cache_readex(opt, cache, adr, fil, save, location, NULL, 0);
}

htsblk cache_read_ro(httrackp * opt, cache_back * cache, const char *adr,
                     const char *fil, const char *save, char *location) {
  return cache_readex(opt, cache, adr, fil, save, location, NULL, 1);
}

htsblk cache_read_including_broken(httrackp * opt, cache_back * cache,
                                   const char *adr, const char *fil) {
  htsblk r = cache_read(opt, cache, adr, fil, NULL, NULL);

  if (r.statuscode == -1) {
    lien_back *itemback = NULL;

    if (back_unserialize_ref(opt, adr, fil, &itemback) == 0) {
      r = itemback->r;
      /* cleanup */
      back_clear_entry(itemback);       /* delete entry content */
      freet(itemback);          /* delete item */
      itemback = NULL;
      return r;
    }
  }
  return r;
}

static htsblk cache_readex_old(httrackp * opt, cache_back * cache,
                               const char *adr, const char *fil,
                               const char *save, char *location,
                               char *return_save, int readonly);

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
  } else {
    return cache_readex_old(opt, cache, adr, fil, save, location, return_save,
                            readonly);
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
  strcpybuff(r.location, "");
  strcpybuff(buff, adr);
  strcatbuff(buff, fil);
  hash_pos_return = coucal_read(cache->hashtable, buff, &hash_pos);
  /* avoid errors on data entries */
  if (adr[0] == '/' && adr[1] == '/' && adr[2] == '[') {
#if HTS_FAST_CACHE
    hash_pos_return = 0;
#else
    a = NULL;
#endif
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

        //int totalHeader = 0;
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
              ZIP_READFIELD_STRING(line, value, "X-StatusMessage", r.msg);      // msg
              ZIP_READFIELD_LLINT(line, value, "X-Size", r.size);       // size
              ZIP_READFIELD_STRING(line, value, "Content-Type", r.contenttype); // contenttype
              ZIP_READFIELD_STRING(line, value, "X-Charset", r.charset);        // contenttype
              ZIP_READFIELD_STRING(line, value, "Last-Modified", r.lastmodified);       // last-modified
              ZIP_READFIELD_STRING(line, value, "Etag", r.etag);        // Etag
              ZIP_READFIELD_STRING(line, value, "Location", r.location);        // 'location' pour moved
              ZIP_READFIELD_STRING(line, value, "Content-Disposition", r.cdispo);       // Content-disposition
              //ZIP_READFIELD_STRING(line, value, "X-Addr", ..);            // Original address
              //ZIP_READFIELD_STRING(line, value, "X-Fil", ..);            // Original URI filename
              ZIP_READFIELD_STRING(line, value, "X-Save", previous_save_);      // Original save filename
            }
          } while(offset < readSizeHeader && !lineEof);
          //totalHeader = offset;

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
            strcpybuff(return_save, previous_save);
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
          else {                /* ne pas lire uniquement header */
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
                  //printf("%s\n",save);
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
                } else {        /* Read in memory from disk */
                  FILE *const fp = FOPEN(fconv(catbuff, sizeof(catbuff), previous_save), "rb");

                  if (fp != NULL) {
                    r.adr = (char *) malloct((int) r.size + 4);
                    if (r.adr != NULL) {
                      if (r.size > 0
                          && fread(r.adr, 1, (int) r.size, fp) != r.size) {
                        int last_errno = errno;

                        r.statuscode = STATUSCODE_INVALID;
                        sprintf(r.msg, "Read error in cache disk data: %s",
                                strerror(last_errno));
                      }
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
                r.adr = (char *) malloct((int) r.size + 4);
                if (r.adr != NULL) {
                  if (unzReadCurrentFile((unzFile) cache->zipInput, r.adr, (int) r.size) != r.size) {   // erreur
                    freet(r.adr);
                    r.adr = NULL;
                    r.statuscode = STATUSCODE_INVALID;
                    strcpybuff(r.msg, "Cache Read Error : Read Data");
                  } else
                    *(r.adr + r.size) = '\0';
                  //printf(">%s status %d\n",back[p].r.contenttype,back[p].r.statuscode);
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
static htsblk cache_readex_old(httrackp * opt, cache_back * cache,
                               const char *adr, const char *fil,
                               const char *save, char *location,
                               char *return_save, int readonly) {
#if HTS_FAST_CACHE
  intptr_t hash_pos;
  int hash_pos_return;
#else
  char *a;
#endif
  char BIGSTK buff[HTS_URLMAXSIZE * 2];
  char BIGSTK location_default[HTS_URLMAXSIZE * 2];
  char BIGSTK previous_save[HTS_URLMAXSIZE * 2];
  char catbuff[CATBUFF_SIZE];
  htsblk r;
  int ok = 0;
  int header_only = 0;

  hts_init_htsblk(&r);
  //memset(&r, 0, sizeof(htsblk)); r.soc=INVALID_SOCKET;
  if (location) {
    r.location = location;
  } else {
    r.location = location_default;
  }
  strcpybuff(r.location, "");
#if HTS_FAST_CACHE
  strcpybuff(buff, adr);
  strcatbuff(buff, fil);
  hash_pos_return = coucal_read(cache->hashtable, buff, &hash_pos);
#else
  buff[0] = '\0';
  strcatbuff(buff, "\n");
  strcatbuff(buff, adr);
  strcatbuff(buff, "\n");
  strcatbuff(buff, fil);
  strcatbuff(buff, "\n");
  if (cache->use)
    a = strstr(cache->use, buff);
  else
    a = NULL;                   // forcer erreur
#endif

  /* avoid errors on data entries */
  if (adr[0] == '/' && adr[1] == '/' && adr[2] == '[') {
#if HTS_FAST_CACHE
    hash_pos_return = 0;
#else
    a = NULL;
#endif
  }
  // en cas de succès
#if HTS_FAST_CACHE
  if (hash_pos_return != 0) {
#else
  if (a != NULL) {              // OK existe en cache!
#endif
    intptr_t pos;

#if DEBUGCA
    fprintf(stdout, "..cache: %s%s at ", adr, fil);
#endif

#if HTS_FAST_CACHE
    pos = hash_pos;             /* simply */
#else
    a += strlen(buff);
    sscanf(a, "%d", &pos);      // lire position
#endif
#if DEBUGCA
    printf("%d\n", pos);
#endif

    fflush(cache->olddat);
    if (fseek(cache->olddat, (long) ((pos > 0) ? pos : (-pos)), SEEK_SET) == 0) {
      /* Importer cache1.0 */
      if (cache->version == 0) {
        OLD_htsblk old_r;

        if (fread((char *) &old_r, 1, sizeof(old_r), cache->olddat) == sizeof(old_r)) { // lire tout (y compris statuscode etc)
          r.statuscode = old_r.statuscode;
          r.size = old_r.size;  // taille fichier
          strcpybuff(r.msg, old_r.msg);
          strcpybuff(r.contenttype, old_r.contenttype);
          ok = 1;               /* import  ok */
        }
        /* */
        /* Cache 1.1 */
      } else {
        char check[256];
        LLint size_read;

        check[0] = '\0';
        //
        cache_rint(cache->olddat, &r.statuscode);
        cache_rLLint(cache->olddat, &r.size);
        cache_rstr(cache->olddat, r.msg);
        cache_rstr(cache->olddat, r.contenttype);
        if (cache->version >= 3)
          cache_rstr(cache->olddat, r.charset);
        cache_rstr(cache->olddat, r.lastmodified);
        cache_rstr(cache->olddat, r.etag);
        cache_rstr(cache->olddat, r.location);
        if (cache->version >= 2)
          cache_rstr(cache->olddat, r.cdispo);
        if (cache->version >= 4) {
          cache_rstr(cache->olddat, previous_save);     // adr
          cache_rstr(cache->olddat, previous_save);     // fil
          previous_save[0] = '\0';
          cache_rstr(cache->olddat, previous_save);     // save
          if (return_save != NULL) {
            strcpybuff(return_save, previous_save);
          }
        }
        if (cache->version >= 5) {
          r.headers = cache_rstr_addr(cache->olddat);
        }
        //
        cache_rstr(cache->olddat, check);
        if (strcmp(check, "HTS") == 0) {        /* intégrité OK */
          ok = 1;
        }
        cache_rLLint(cache->olddat, &size_read);        /* lire size pour être sûr de la taille déclarée (réécrire) */
        if (size_read > 0) {    /* si inscrite ici */
          r.size = size_read;
        } else {                /* pas de données directement dans le cache, fichier présent? */
          if (r.statuscode != HTTP_OK)
            header_only = 1;    /* que l'en tête ici! */
        }
      }

      /* Remplir certains champs */
      r.totalsize = r.size;

      // lecture du header (y compris le statuscode)
      /*if (fread((char*) &r,1,sizeof(htsblk),cache->olddat)==sizeof(htsblk)) { // lire tout (y compris statuscode etc) */
      if (ok) {
        // sécurité
        r.adr = NULL;
        r.out = NULL;
        ////r.location=NULL;  non, fixée lors des 301 ou 302
        r.fp = NULL;

        if ((r.statuscode >= 0) && (r.statuscode <= 999)
            && (r.notmodified >= 0) && (r.notmodified <= 9)) {  // petite vérif intégrité
          if ((save) && (!header_only)) {       /* ne pas lire uniquement header */
            //int to_file=0;

            r.adr = NULL;
            r.soc = INVALID_SOCKET;
            // // r.location=NULL;

#if HTS_DIRECTDISK
            // Court-circuit:
            // Peut-on stocker le fichier directement sur disque?
            if (!readonly && r.statuscode == HTTP_OK && !is_hypertext_mime(opt, r.contenttype, fil) && strnotempty(save)) {     // pas HTML, écrire sur disk directement
              int ok = 0;

              r.is_write = 1;   // écrire
              if (fexist_utf8(fconv(catbuff, sizeof(catbuff), save))) {  // un fichier existe déja
                //if (fsize_utf8(fconv(save))==r.size) {  // même taille -- NON tant pis (taille mal declaree)
                ok = 1;         // plus rien à faire
                filenote(&opt->state.strc, save, NULL); // noter comme connu
                file_notify(opt, adr, fil, save, 0, 0, 0);
                //}
              }

              if ((pos < 0) && (!ok)) { // Pas de donnée en cache et fichier introuvable : erreur!
                if (opt->norecatch) {
                  file_notify(opt, adr, fil, save, 1, 0, 0);
                  filecreateempty(&opt->state.strc, save);
                  //
                  r.statuscode = STATUSCODE_INVALID;
                  strcpybuff(r.msg, "File deleted by user not recaught");
                  ok = 1;       // ne pas récupérer (et pas d'erreur)
                } else {
                  r.statuscode = STATUSCODE_INVALID;
                  strcpybuff(r.msg, "Previous cache file not found");
                  ok = 1;       // ne pas récupérer
                }
              }

              if (!ok) {
                r.out = filecreate(&opt->state.strc, save);
#if HDEBUG
                printf("direct-disk: %s\n", save);
#endif
                if (r.out != NULL) {
                  char BIGSTK buff[32768 + 4];
                  size_t size = (size_t) r.size;

                  if (size > 0) {
                    size_t nl;

                    do {
                      nl = fread(buff, 1, minimum(size, 32768), cache->olddat);
                      if (nl > 0) {
                        size -= nl;
                        if (fwrite(buff, 1, nl, r.out) != nl) { // erreur
                          r.statuscode = STATUSCODE_INVALID;
                          strcpybuff(r.msg, "Cache Read Error : Read To Disk");
                        }
                      }
                    } while((nl > 0) && (size > 0) && (r.statuscode != -1));
                  }

                  fclose(r.out);
                  r.out = NULL;
#ifndef _WIN32
                  chmod(save, HTS_ACCESS_FILE);
#endif
                } else {
                  r.statuscode = STATUSCODE_INVALID;
                  strcpybuff(r.msg,
                             "Cache Write Error : Unable to Create File");
                  //printf("%s\n",save);
                }
              }

            } else
#endif
            {                   // lire en mémoire

              if (pos < 0) {
                if (strnotempty(save)) {        // Pas de donnée en cache, bizarre car html!!!
                  r.statuscode = STATUSCODE_INVALID;
                  strcpybuff(r.msg, "Previous cache file not found (2)");
                } else {        /* Read in memory from cache */
                  if (strnotempty(return_save) && fexist_utf8(return_save)) {
                    FILE *fp = FOPEN(fconv(catbuff, sizeof(catbuff), return_save), "rb");

                    if (fp != NULL) {
                      r.adr = (char *) malloct((size_t) r.size + 4);
                      if (r.adr != NULL) {
                        if (r.size > 0
                            && fread(r.adr, 1, (size_t) r.size, fp) != r.size) {
                          r.statuscode = STATUSCODE_INVALID;
                          strcpybuff(r.msg, "Read error in cache disk data");
                        }
                      } else {
                        r.statuscode = STATUSCODE_INVALID;
                        strcpybuff(r.msg,
                                   "Read error (memory exhausted) from cache");
                      }
                      fclose(fp);
                    }
                  } else {
                    r.statuscode = STATUSCODE_INVALID;
                    strcpybuff(r.msg, "Cache file not found on disk");
                  }
                }
              } else {
                // lire fichier (d'un coup)
                r.adr = (char *) malloct((size_t) r.size + 4);
                if (r.adr != NULL) {
                  if (fread(r.adr, 1, (size_t) r.size, cache->olddat) != r.size) {      // erreur
                    freet(r.adr);
                    r.adr = NULL;
                    r.statuscode = STATUSCODE_INVALID;
                    strcpybuff(r.msg, "Cache Read Error : Read Data");
                  } else
                    *(r.adr + r.size) = '\0';
                  //printf(">%s status %d\n",back[p].r.contenttype,back[p].r.statuscode);
                } else {        // erreur
                  r.statuscode = STATUSCODE_INVALID;
                  strcpybuff(r.msg, "Cache Memory Error");
                }
              }
            }
          }                     // si save==null, ne rien charger (juste en tête)
        } else {
#if DEBUGCA
          printf("Cache Read Error : Bad Data");
#endif
          r.statuscode = STATUSCODE_INVALID;
          strcpybuff(r.msg, "Cache Read Error : Bad Data");
        }
      } else {                  // erreur
#if DEBUGCA
        printf("Cache Read Error : Read Header");
#endif
        r.statuscode = STATUSCODE_INVALID;
        strcpybuff(r.msg, "Cache Read Error : Read Header");
      }
    } else {
#if DEBUGCA
      printf("Cache Read Error : Seek Failed");
#endif
      r.statuscode = STATUSCODE_INVALID;
      strcpybuff(r.msg, "Cache Read Error : Seek Failed");
    }
  } else {
#if DEBUGCA
    printf("File Cache Not Found");
#endif
    r.statuscode = STATUSCODE_INVALID;
    strcpybuff(r.msg, "File Cache Entry Not Found");
  }
  if (!location) {              /* don't export internal buffer */
    r.location = NULL;
  }
  return r;
}

/* write (string1-string2)-data in cache */
/* 0 if failed */
int cache_writedata(FILE * cache_ndx, FILE * cache_dat, const char *str1,
                    const char *str2, char *outbuff, int len) {
  if (cache_dat) {
    char BIGSTK buff[HTS_URLMAXSIZE * 4];
    char s[256];
    int pos;

    fflush(cache_dat);
    fflush(cache_ndx);
    pos = ftell(cache_dat);
    /* first write data */
    if (cache_wint(cache_dat, len) != -1) {     // length
      if (fwrite(outbuff, 1, len, cache_dat) == len) {  // data
        /* then write index */
        sprintf(s, "%d\n", pos);
        buff[0] = '\0';
        strcatbuff(buff, str1);
        strcatbuff(buff, "\n");
        strcatbuff(buff, str2);
        strcatbuff(buff, "\n");
        cache_wstr(cache_ndx, buff);
        if (fwrite(s, 1, strlen(s), cache_ndx) == strlen(s)) {
          fflush(cache_dat);
          fflush(cache_ndx);
          return 1;
        }
      }
    }
  }
  return 0;
}

/* read the data corresponding to (string1-string2) in cache */
/* 0 if failed */
int cache_readdata(cache_back * cache, const char *str1, const char *str2,
                   char **inbuff, int *inlen) {
#if HTS_FAST_CACHE
  if (cache->hashtable) {
    char BIGSTK buff[HTS_URLMAXSIZE * 4];
    intptr_t pos;

    strcpybuff(buff, str1);
    strcatbuff(buff, str2);
    if (coucal_read(cache->hashtable, buff, &pos)) {
      if (fseek(cache->olddat, (long) ((pos > 0) ? pos : (-pos)), SEEK_SET) ==
          0) {
        INTsys len;

        cache_rint(cache->olddat, &len);
        if (len > 0) {
          char *mem_buff = (char *) malloct(len + 4);   /* Plus byte 0 */

          if (mem_buff) {
            if (fread(mem_buff, 1, len, cache->olddat) == len) {        // lire tout (y compris statuscode etc)*/
              *inbuff = mem_buff;
              *inlen = len;
              return 1;
            } else
              freet(mem_buff);
          }
        }
      }
    }
  }
#endif
  *inbuff = NULL;
  *inlen = 0;
  return 0;
}

static int hts_rename(httrackp * opt, const char *a, const char *b) {
  hts_log_print(opt, LOG_DEBUG, "Cache: rename %s -> %s (%p %p)", a, b, a, b);
  return rename(a, b);
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
#ifdef _WIN32
      mkdir(fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log), "hts-cache"));
#else
      mkdir(fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log), "hts-cache"),
            HTS_PROTECT_FOLDER);
#endif
      if ((fexist(fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log), "hts-cache/new.zip")))) {       // il existe déja un cache précédent.. renommer
        /* Previous cache from the previous cache version */
#if 0
        /* No.. reuse with old httrack releases! */
        if (fexist
            (fconcat
             (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
              "hts-cache/old.dat")))
          remove(fconcat
                 (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                  "hts-cache/old.dat"));
        if (fexist
            (fconcat
             (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
              "hts-cache/old.ndx")))
          remove(fconcat
                 (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                  "hts-cache/old.ndx"));
#endif
        /* Previous cache version */
        if ((fexist(fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log), "hts-cache/new.dat"))) && (fexist(fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log), "hts-cache/new.ndx")))) {     // il existe déja un cache précédent.. renommer
          rename(fconcat
                 (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                  "hts-cache/new.dat"), fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                                                StringBuff(opt->path_log),
                                                "hts-cache/old.dat"));
          rename(fconcat
                 (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                  "hts-cache/new.ndx"), fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                                                StringBuff(opt->path_log),
                                                "hts-cache/old.ndx"));
        }

        /* Remove OLD cache */
        if (fexist
            (fconcat
             (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
              "hts-cache/old.zip"))) {
          if (remove
              (fconcat
               (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                "hts-cache/old.zip")) != 0) {
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
      } else if ((fexist(fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log), "hts-cache/new.dat"))) && (fexist(fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log), "hts-cache/new.ndx")))) {        // il existe déja un cache précédent.. renommer
#if DEBUGCA
        printf("work with former cache\n");
#endif
        if (fexist
            (fconcat
             (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
              "hts-cache/old.dat")))
          remove(fconcat
                 (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                  "hts-cache/old.dat"));
        if (fexist
            (fconcat
             (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
              "hts-cache/old.ndx")))
          remove(fconcat
                 (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                  "hts-cache/old.ndx"));

        rename(fconcat
               (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                "hts-cache/new.dat"), fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                                              StringBuff(opt->path_log),
                                              "hts-cache/old.dat"));
        rename(fconcat
               (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                "hts-cache/new.ndx"), fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                                              StringBuff(opt->path_log),
                                              "hts-cache/old.ndx"));
      } else {                  // un des deux (ou les deux) fichiers cache absents: effacer l'autre éventuel
#if DEBUGCA
        printf("new cache\n");
#endif
        if (fexist
            (fconcat
             (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
              "hts-cache/new.dat")))
          remove(fconcat
                 (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                  "hts-cache/new.dat"));
        if (fexist
            (fconcat
             (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
              "hts-cache/new.ndx")))
          remove(fconcat
                 (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                  "hts-cache/new.ndx"));
      }
    } else {
      hts_log_print(opt, LOG_DEBUG, "Cache: no cache found");
    }
    hts_log_print(opt, LOG_DEBUG, "Cache: size %d",
                  (int)
                  fsize(fconcat
                        (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                         "hts-cache/old.zip")));

    // charger index cache précédent
    if ((!cache->ro
         &&
         fsize(fconcat
               (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                "hts-cache/old.zip")) > 0)
        || (cache->ro
            &&
            fsize(fconcat
                  (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                   "hts-cache/new.zip")) > 0)
      ) {
      if (!cache->ro) {
        cache->zipInput =
          unzOpen(fconcat
                  (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                   "hts-cache/old.zip"));
      } else {
        cache->zipInput =
          unzOpen(fconcat
                  (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                   "hts-cache/new.zip"));
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
        if (unzRepair
            (name,
             fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                     "hts-cache/repair.zip"), fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                                                      StringBuff(opt->path_log),
                                                      "hts-cache/repair.tmp"),
             &repaired, &repairedBytes) == Z_OK) {
          unlink(name);
          rename(fconcat
                 (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                  "hts-cache/repair.zip"), name);
          cache->zipInput = unzOpen(name);
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

    } else
      if ((!cache->ro
           &&
           fsize(fconcat
                 (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                  "hts-cache/old.dat")) >= 0
           &&
           fsize(fconcat
                 (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                  "hts-cache/old.ndx")) > 0)
          || (cache->ro
              &&
              fsize(fconcat
                    (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                     "hts-cache/new.dat")) >= 0
              &&
              fsize(fconcat
                    (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                     "hts-cache/new.ndx")) > 0)
      ) {
      FILE *oldndx = NULL;

#if DEBUGCA
      printf("..load cache\n");
#endif
      if (!cache->ro) {
        cache->olddat =
          fopen(fconcat
                (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                 "hts-cache/old.dat"), "rb");
        oldndx =
          fopen(fconcat
                (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                 "hts-cache/old.ndx"), "rb");
      } else {
        cache->olddat =
          fopen(fconcat
                (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                 "hts-cache/new.dat"), "rb");
        oldndx =
          fopen(fconcat
                (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                 "hts-cache/new.ndx"), "rb");
      }
      // les deux doivent être ouvrables
      if ((cache->olddat == NULL) && (oldndx != NULL)) {
        fclose(oldndx);
        oldndx = NULL;
      }
      if ((cache->olddat != NULL) && (oldndx == NULL)) {
        fclose(cache->olddat);
        cache->olddat = NULL;
      }
      // lire index
      if (oldndx != NULL) {
        int buffl;

        fclose(oldndx);
        oldndx = NULL;
        // lire ndx, et lastmodified
        if (!cache->ro) {
          buffl =
            fsize(fconcat
                  (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                   "hts-cache/old.ndx"));
          cache->use =
            readfile(fconcat
                     (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                      "hts-cache/old.ndx"));
        } else {
          buffl =
            fsize(fconcat
                  (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                   "hts-cache/new.ndx"));
          cache->use =
            readfile(fconcat
                     (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                      "hts-cache/new.ndx"));
        }
        if (cache->use != NULL) {
          char firstline[256];
          char *a = cache->use;

          a += cache_brstr(a, firstline);
          if (strncmp(firstline, "CACHE-", 6) == 0) {   // Nouvelle version du cache
            if (strncmp(firstline, "CACHE-1.", 8) == 0) {       // Version 1.1x
              cache->version = (int) (firstline[8] - '0');      // cache 1.x
              if (cache->version <= 5) {
                a += cache_brstr(a, firstline);
                strcpybuff(cache->lastmodified, firstline);
              } else {
                hts_log_print(opt, LOG_ERROR,
                              "Cache: version 1.%d not supported, ignoring current cache",
                              cache->version);
                fclose(cache->olddat);
                cache->olddat = NULL;
                freet(cache->use);
                cache->use = NULL;
              }
            } else {            // non supporté
              hts_log_print(opt, LOG_ERROR,
                            "Cache: %s not supported, ignoring current cache",
                            firstline);
              fclose(cache->olddat);
              cache->olddat = NULL;
              freet(cache->use);
              cache->use = NULL;
            }
            /* */
          } else {              // Vieille version du cache
            /* */
            hts_log_print(opt, LOG_WARNING,
                          "Cache: importing old cache format");
            cache->version = 0; // cache 1.0
            strcpybuff(cache->lastmodified, firstline);
          }
          opt->is_update = 1;   // signaler comme update

          /* Create hash table for the cache (MUCH FASTER!) */
#if HTS_FAST_CACHE
          if (cache->use) {
            char BIGSTK line[HTS_URLMAXSIZE * 2];
            char linepos[256];
            int pos;

            while((a != NULL) && (a < (cache->use + buffl))) {
              a = strchr(a + 1, '\n');  /* start of line */
              if (a) {
                a++;
                /* read "host/file" */
                a += binput(a, line, HTS_URLMAXSIZE);
                a += binput(a, line + strlen(line), HTS_URLMAXSIZE);
                /* read position */
                a += binput(a, linepos, 200);
                sscanf(linepos, "%d", &pos);
                coucal_add(cache->hashtable, line, pos);
              }
            }
            /* Not needed anymore! */
            freet(cache->use);
            cache->use = NULL;
          }
#endif
        }
      }
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
      structcheck(fconcat
                  (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log), "hts-cache/"));

      if (1) {
        /* Create ZIP file cache */
        cache->zipOutput =
          (void *)
          zipOpen(fconcat
                  (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                   "hts-cache/new.zip"), 0);

        if (cache->zipOutput != NULL) {
          // supprimer old.lst
          if (fexist
              (fconcat
               (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                "hts-cache/old.lst")))
            remove(fconcat
                   (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                    "hts-cache/old.lst"));
          // renommer
          if (fexist
              (fconcat
               (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                "hts-cache/new.lst")))
            rename(fconcat
                   (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                    "hts-cache/new.lst"), fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                                                  StringBuff(opt->path_log),
                                                  "hts-cache/old.lst"));
          // ouvrir
          cache->lst =
            fopen(fconcat
                  (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                   "hts-cache/new.lst"), "wb");
          strcpybuff(opt->state.strc.path, StringBuff(opt->path_html));
          opt->state.strc.lst = cache->lst;
          //{
          //filecreate_params tmp;
          //strcpybuff(tmp.path,StringBuff(opt->path_html));    // chemin
          //tmp.lst=cache->lst;                 // fichier lst
          //filenote("",&tmp);        // initialiser filecreate
          //}

          // supprimer old.txt
          if (fexist
              (fconcat
               (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                "hts-cache/old.txt")))
            remove(fconcat
                   (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                    "hts-cache/old.txt"));
          // renommer
          if (fexist
              (fconcat
               (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                "hts-cache/new.txt")))
            rename(fconcat
                   (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                    "hts-cache/new.txt"), fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                                                  StringBuff(opt->path_log),
                                                  "hts-cache/old.txt"));
          // ouvrir
          cache->txt =
            fopen(fconcat
                  (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                   "hts-cache/new.txt"), "wb");
          if (cache->txt) {
            fprintf(cache->txt,
                    "date\tsize'/'remotesize\tflags(request:Update,Range state:File response:Modified,Chunked,gZipped)\t");
            fprintf(cache->txt,
                    "statuscode\tstatus ('servermsg')\tMIME\tEtag|Date\tURL\tlocalfile\t(from URL)"
                    LF);
          }
        }
      } else {
        cache->dat =
          fopen(fconcat
                (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                 "hts-cache/new.dat"), "wb");
        cache->ndx =
          fopen(fconcat
                (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                 "hts-cache/new.ndx"), "wb");
        // les deux doivent être ouvrables
        if ((cache->dat == NULL) && (cache->ndx != NULL)) {
          fclose(cache->ndx);
          cache->ndx = NULL;
        }
        if ((cache->dat != NULL) && (cache->ndx == NULL)) {
          fclose(cache->dat);
          cache->dat = NULL;
        }

        if (cache->ndx != NULL) {
          char s[256];

          cache_wstr(cache->dat, "CACHE-1.5");
          fflush(cache->dat);
          cache_wstr(cache->ndx, "CACHE-1.5");
          fflush(cache->ndx);
          //
          time_gmt_rfc822(s);   // date et heure actuelle GMT pour If-Modified-Since..
          cache_wstr(cache->ndx, s);
          fflush(cache->ndx);   // un petit fflush au cas où

          // supprimer old.lst
          if (fexist
              (fconcat
               (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                "hts-cache/old.lst")))
            remove(fconcat
                   (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                    "hts-cache/old.lst"));
          // renommer
          if (fexist
              (fconcat
               (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                "hts-cache/new.lst")))
            rename(fconcat
                   (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                    "hts-cache/new.lst"), fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                                                  StringBuff(opt->path_log),
                                                  "hts-cache/old.lst"));
          // ouvrir
          cache->lst =
            fopen(fconcat
                  (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                   "hts-cache/new.lst"), "wb");
          strcpybuff(opt->state.strc.path, StringBuff(opt->path_html));
          opt->state.strc.lst = cache->lst;
          //{
          //  filecreate_params tmp;
          //  strcpybuff(tmp.path,StringBuff(opt->path_html));    // chemin
          //  tmp.lst=cache->lst;                 // fichier lst
          //  filenote("",&tmp);        // initialiser filecreate
          //}

          // supprimer old.txt
          if (fexist
              (fconcat
               (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                "hts-cache/old.txt")))
            remove(fconcat
                   (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                    "hts-cache/old.txt"));
          // renommer
          if (fexist
              (fconcat
               (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                "hts-cache/new.txt")))
            rename(fconcat
                   (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                    "hts-cache/new.txt"), fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                                                  StringBuff(opt->path_log),
                                                  "hts-cache/old.txt"));
          // ouvrir
          cache->txt =
            fopen(fconcat
                  (OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log),
                   "hts-cache/new.txt"), "wb");
          if (cache->txt) {
            fprintf(cache->txt,
                    "date\tsize'/'remotesize\tflags(request:Update,Range state:File response:Modified,Chunked,gZipped)\t");
            fprintf(cache->txt,
                    "statuscode\tstatus ('servermsg')\tMIME\tEtag|Date\tURL\tlocalfile\t(from URL)"
                    LF);
          }
          // test
          // cache_writedata(cache->ndx,cache->dat,"//[TEST]//","test1","TEST PIPO",9);
        }                       // cache->ndx!=NULL
      }                         //cache->zipOutput != NULL

    } else {
      cache->lst = cache->dat = cache->ndx = NULL;
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
  INTsys len = 0;

  len = fsize(fil);
  if (len >= 0) {               // exists
    FILE *fp;

    fp = fopen(fconv(catbuff, sizeof(catbuff), fil), "rb");
    if (fp != NULL) {           // n'existe pas (!)
      adr = (char *) malloct(len + 1);
      if (size != NULL)
        *size = len;
      if (adr != NULL) {
        if (len > 0 && fread(adr, 1, len, fp) != len) { // fichier endommagé ?
          freet(adr);
          adr = NULL;
        } else
          *(adr + len) = '\0';
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
  const off_t len = fsize_utf8(fil);

  if (len >= 0) {               // exists
    FILE *const fp = FOPEN(fconv(catbuff, sizeof(catbuff), fil), "rb");

    if (fp != NULL) {           // n'existe pas (!)
      adr = (char *) malloct(len + 1);
      if (adr != NULL) {
        if (len > 0 && fread(adr, 1, len, fp) != len) { // fichier endommagé ?
          freet(adr);
          adr = NULL;
        } else {
          adr[len] = '\0';
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
      strcpybuff(adr, defaultdata);
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
void cache_rstr(FILE * fp, char *s) {
  INTsys i;
  char buff[256 + 4];

  linput(fp, buff, 256);
  sscanf(buff, INTsysP, &i);
  if (i < 0 || i > 32768)       /* error, something nasty happened */
    i = 0;
  if (i > 0) {
    if ((int) fread(s, 1, i, fp) != i) {
      int fread_cache_failed = 0;

      assertf(fread_cache_failed);
    }
  }
  *(s + i) = '\0';
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
int cache_brstr(char *adr, char *s) {
  int i;
  int off;
  char buff[256 + 4];

  off = binput(adr, buff, 256);
  adr += off;
  sscanf(buff, "%d", &i);
  if (i > 0)
    strncpy(s, adr, i);
  *(s + i) = '\0';
  off += i;
  return off;
}
int cache_quickbrstr(char *adr, char *s) {
  int i;
  int off;
  char buff[256 + 4];

  off = binput(adr, buff, 256);
  adr += off;
  sscanf(buff, "%d", &i);
  if (i > 0)
    strncpy(s, adr, i);
  *(s + i) = '\0';
  off += i;
  return off;
}

/* idem, mais en int */
int cache_brint(char *adr, int *i) {
  char s[256];
  int r = cache_brstr(adr, s);

  if (r != -1)
    sscanf(s, "%d", i);
  return r;
}
void cache_rint(FILE * fp, int *i) {
  char s[256];

  cache_rstr(fp, s);
  sscanf(s, "%d", i);
}
int cache_wint(FILE * fp, int i) {
  char s[256];

  sprintf(s, "%d", (int) i);
  return cache_wstr(fp, s);
}
void cache_rLLint(FILE * fp, LLint * i) {
  char s[256];

  cache_rstr(fp, s);
  sscanf(s, LLintP, i);
}
int cache_wLLint(FILE * fp, LLint i) {
  char s[256];

  sprintf(s, LLintP, (LLint) i);
  return cache_wstr(fp, s);
}

// -- cache --
