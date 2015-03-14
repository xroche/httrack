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
/*       backing system (multiple socket download)              */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Internal engine bytecode */
#define HTS_INTERNAL_BYTECODE

/* specific definitions */
#include "htsnet.h"
#include "htscore.h"
#include "htsthread.h"
#include <time.h>
/* END specific definitions */

#include "htsback.h"

//#ifdef _WIN32
#include "htsftp.h"
#if HTS_USEZLIB
#include "htszlib.h"
#else
#error HTS_USEZLIB not defined
#endif
//#endif

#ifdef _WIN32
#ifndef __cplusplus
// DOS
#include <process.h>            /* _beginthread, _endthread */
#endif
#else
#endif

#define VT_CLREOL       "\33[K"

/* Slot operations */
static int slot_can_be_cached_on_disk(const lien_back * back);
static int slot_can_be_cleaned(const lien_back * back);
static int slot_can_be_finalized(httrackp * opt, const lien_back * back);

struct_back *back_new(httrackp *opt, int back_max) {
  int i;
  struct_back *sback = calloct(1, sizeof(struct_back));

  sback->count = back_max;
  sback->lnk = (lien_back *) calloct((back_max + 1), sizeof(lien_back));
  sback->ready = coucal_new(0);
  hts_set_hash_handler(sback->ready, opt);
  coucal_set_name(sback->ready, "back_new");
  sback->ready_size_bytes = 0;
  coucal_value_is_malloc(sback->ready, 1);
  // init
  for(i = 0; i < sback->count; i++) {
    sback->lnk[i].r.location = sback->lnk[i].location_buffer;
    sback->lnk[i].status = STATUS_FREE;
    sback->lnk[i].r.soc = INVALID_SOCKET;
  }
  return sback;
}

void back_free(struct_back ** sback) {
  if (sback != NULL && *sback != NULL) {
    if ((*sback)->lnk != NULL) {
      freet((*sback)->lnk);
      (*sback)->lnk = NULL;
    }
    if ((*sback)->ready != NULL) {
      coucal_delete(&(*sback)->ready);
      (*sback)->ready_size_bytes = 0;
    }
    freet(*sback);
    *sback = NULL;
  }
}

void back_delete_all(httrackp * opt, cache_back * cache, struct_back * sback) {
  if (sback != NULL) {
    int i;

    // delete live slots
    for(i = 0; i < sback->count; i++) {
      back_delete(opt, cache, sback, i);
    }
    // delete stored slots
    if (sback->ready != NULL) {
      struct_coucal_enum e = coucal_enum_new(sback->ready);
      coucal_item *item;

      while((item = coucal_enum_next(&e))) {
#ifndef HTS_NO_BACK_ON_DISK
        const char *filename = (char *) item->value.ptr;

        if (filename != NULL) {
          (void) UNLINK(filename);
        }
#else
        /* clear entry content (but not yet the entry) */
        lien_back *back = (lien_back *) item->value.ptr;

        back_clear_entry(back);
#endif
      }
      /* delete hashtable & content */
      coucal_delete(&sback->ready);
      sback->ready_size_bytes = 0;
    }
  }
}

// ---
// routines de backing

static int back_index_ready(httrackp * opt, struct_back * sback, const char *adr,
                            const char *fil, const char *sav, int getIndex);
static int back_index_fetch(httrackp * opt, struct_back * sback, const char *adr,
                            const char *fil, const char *sav, int getIndex);

// retourne l'index d'un lien dans un tableau de backing
int back_index(httrackp * opt, struct_back * sback, const char *adr, const char *fil,
               const char *sav) {
  return back_index_fetch(opt, sback, adr, fil, sav, 1);
}

static int back_index_fetch(httrackp * opt, struct_back * sback, const char *adr,
                            const char *fil, const char *sav, int getIndex) {
  lien_back *const back = sback->lnk;
  const int back_max = sback->count;
  int index = -1;
  int i;

  for(i = 0; i < back_max; i++) {
    if (back[i].status >= 0     /* not free or alive */
        && strfield2(back[i].url_adr, adr)
        && strcmp(back[i].url_fil, fil) == 0) {
      if (index == -1)          /* first time we meet, store it */
        index = i;
      else if (sav != NULL && strcmp(back[i].url_sav, sav) == 0) {      /* oops, check sav too */
        index = i;
        return index;
      }
    }
  }
  // not found in fast repository - search in the storage hashtable
  if (index == -1 && sav != NULL) {
    index = back_index_ready(opt, sback, adr, fil, sav, getIndex);
  }
  return index;
}

/* resurrect stored entry */
static int back_index_ready(httrackp * opt, struct_back * sback, const char *adr,
                            const char *fil, const char *sav, int getIndex) {
  lien_back *const back = sback->lnk;
  void *ptr = NULL;

  if (coucal_read_pvoid(sback->ready, sav, &ptr)) {
    if (!getIndex) {            /* don't "pagefault" the entry */
      if (ptr != NULL) {
        return sback->count;    /* (invalid but) positive result */
      } else {
        return -1;              /* not found */
      }
    } else if (ptr != NULL) {
      lien_back *itemback = NULL;

#ifndef HTS_NO_BACK_ON_DISK
      FILE *fp;
      const char *fileback = (char *) ptr;
      char catbuff[CATBUFF_SIZE];

      if ((fp = FOPEN(fconv(catbuff, sizeof(catbuff), fileback), "rb")) != NULL) {
        if (back_unserialize(fp, &itemback) != 0) {
          if (itemback != NULL) {
            back_clear_entry(itemback);
            freet(itemback);
            itemback = NULL;
          }
          hts_log_print(opt, LOG_WARNING | LOG_ERRNO,
                        "engine: warning: unserialize error for %s%s (%s)", adr,
                        fil, sav);
        }
        fclose(fp);
      } else {
        hts_log_print(opt, LOG_WARNING | LOG_ERRNO,
                      "engine: warning: unserialize error for %s%s (%s), file disappeared",
                      adr, fil, sav);
      }
      (void) UNLINK(fileback);
#else
      itemback = (lien_back *) ptr;
#endif
      if (itemback != NULL) {
        // move from hashtable to fast repository
        int q = back_search(opt, sback);

        if (q != -1) {
          deletehttp(&back[q].r);       // security check
          back_move(itemback, &back[q]);
          back_clear_entry(itemback);   /* delete entry content */
          freet(itemback);      /* delete item */
          itemback = NULL;
          coucal_remove(sback->ready, sav);    // delete item
          sback->ready_size_bytes -= back[q].r.size;    /* substract for stats */
          back_set_locked(sback, q);    /* locked */
          return q;
        } else {
          hts_log_print(opt, LOG_WARNING,
                        "engine: warning: unserialize error for %s%s (%s): no more space to wakeup frozen slots",
                        adr, fil, sav);
        }
      }
    }
  }
  return -1;
}

static int slot_can_be_cached_on_disk(const lien_back * back) {
  return (back->status == STATUS_READY && back->locked == 0
          && back->url_sav[0] != '\0'
          && strcmp(back->url_sav, BACK_ADD_TEST) != 0);
  /* Note: not checking !IS_DELAYED_EXT(back->url_sav) or it will quickly cause the slots to be filled! */
}

/* Put all backing entries that are ready in the storage hashtable to spare space and CPU */
int back_cleanup_background(httrackp * opt, cache_back * cache,
                            struct_back * sback) {
  lien_back *const back = sback->lnk;
  const int back_max = sback->count;
  int nclean = 0;
  int i;

  for(i = 0; i < back_max; i++) {
    // ready, not locked and suitable
    if (slot_can_be_cached_on_disk(&back[i])) {
#ifdef HTS_NO_BACK_ON_DISK
      lien_back *itemback;
#endif
      /* Security check */
      int checkIndex =
        back_index_ready(opt, sback, back[i].url_adr, back[i].url_fil,
                         back[i].url_sav, 1);
      if (checkIndex != -1) {
        hts_log_print(opt, LOG_WARNING,
                      "engine: unexpected duplicate file entry: %s%s -> %s (%d '%s') / %s%s -> %s (%d '%s')",
                      back[checkIndex].url_adr, back[checkIndex].url_fil,
                      back[checkIndex].url_sav, back[checkIndex].r.statuscode,
                      back[checkIndex].r.msg, back[i].url_adr, back[i].url_fil,
                      back[i].url_sav, back[i].r.statuscode, back[i].r.msg);
        back_delete(NULL, NULL, sback, checkIndex);
#ifdef _DEBUG
        /* This should NOT happend! */
        {
          int duplicateEntryInBacklog = 1;

          assertf(!duplicateEntryInBacklog);
        }
#endif
      }
#ifndef HTS_NO_BACK_ON_DISK
      /* temporarily serialize the entry on disk */
      {
        int fsz = (int) strlen(back[i].url_sav);
        char *filename = malloc(fsz + 8 + 1);

        if (filename != NULL) {
          FILE *fp;

          if (opt->getmode != 0) {
            sprintf(filename, "%s.tmp", back[i].url_sav);
          } else {
            sprintf(filename, "%stmpfile%d.tmp",
                    StringBuff(opt->path_html_utf8), opt->state.tmpnameid++);
          }
          /* Security check */
          if (fexist_utf8(filename)) {
            hts_log_print(opt, LOG_WARNING,
                          "engine: warning: temporary file %s already exists",
                          filename);
          }
          /* Create file and serialize slot */
          if ((fp = filecreate(NULL, filename)) != NULL) {
            if (back_serialize(fp, &back[i]) == 0) {
              coucal_add_pvoid(sback->ready, back[i].url_sav, filename);
              filename = NULL;
              sback->ready_size_bytes += back[i].r.size;        /* add for stats */
              nclean++;
              back_clear_entry(&back[i]);       /* entry is now recycled */
            } else {
              hts_log_print(opt, LOG_WARNING | LOG_ERRNO,
                            "engine: warning: serialize error for %s%s to %s: write error",
                            back[i].url_adr, back[i].url_fil, filename);
            }
            fclose(fp);
          } else {
            hts_log_print(opt, LOG_WARNING | LOG_ERRNO,
                          "engine: warning: serialize error for %s%s to %s: open error (%s, %s)",
                          back[i].url_adr, back[i].url_fil, filename,
                          dir_exists(filename) ? "directory exists" :
                          "directory does NOT exist!",
                          fexist_utf8(filename) ? "file already exists!" :
                          "file does not exist");
          }
          if (filename != NULL)
            free(filename);
        } else {
          hts_log_print(opt, LOG_WARNING | LOG_ERRNO,
                        "engine: warning: serialize error for %s%s to %s: memory full",
                        back[i].url_adr, back[i].url_fil, filename);
        }
      }
#else
      itemback = calloct(1, sizeof(lien_back));
      back_move(&back[i], itemback);
      coucal_add_pvoid(sback->ready, itemback->url_sav, itemback);
      nclean++;
#endif
    }
  }
  return nclean;
}

// nombre d'entrées libres dans le backing
int back_available(const struct_back * sback) {
  const lien_back *const back = sback->lnk;
  const int back_max = sback->count;
  int i;
  int nb = 0;

  for(i = 0; i < back_max; i++)
    if (back[i].status == STATUS_FREE)  /* libre */
      nb++;
  return nb;
}

// retourne estimation de la taille des html et fichiers stockés en mémoire
LLint back_incache(const struct_back * sback) {
  const lien_back *const back = sback->lnk;
  const int back_max = sback->count;
  int i;
  LLint sum = 0;

  for(i = 0; i < back_max; i++)
    if (back[i].status != -1)
      if (back[i].r.adr)        // ne comptabilier que les blocs en mémoire
        sum += max(back[i].r.size, back[i].r.totalsize);
  // stored (ready) slots
#ifdef HTS_NO_BACK_ON_DISK
  if (sback->ready != NULL) {
    struct_coucal_enum e = coucal_enum_new(sback->ready);
    coucal_item *item;

    while((item = coucal_enum_next(&e))) {
      lien_back *ritem = (lien_back *) item->value.ptr;

      if (ritem->status != -1)
        if (ritem->r.adr)       // ne comptabilier que les blocs en mémoire
          sum += max(ritem->r.size, ritem->r.totalsize);
    }
  }
#endif
  return sum;
}

// retourne estimation de la taille des html et fichiers stockés en mémoire
int back_done_incache(const struct_back * sback) {
  const lien_back *const back = sback->lnk;
  const int back_max = sback->count;
  int i;
  int n = 0;

  for(i = 0; i < back_max; i++)
    if (back[i].status == STATUS_READY)
      n++;
  // stored (ready) slots
  if (sback->ready != NULL) {
#ifndef HTS_NO_BACK_ON_DISK
    n += (int) coucal_nitems(sback->ready);
#else
    struct_coucal_enum e = coucal_enum_new(sback->ready);
    coucal_item *item;

    while((item = coucal_enum_next(&e))) {
      lien_back *ritem = (lien_back *) item->value.ptr;

      if (ritem->status == STATUS_READY)
        n++;
    }
#endif
  }
  return n;
}

// le lien a-t-il été mis en backing?
HTS_INLINE int back_exist(struct_back * sback, httrackp * opt, const char *adr,
                          const char *fil, const char *sav) {
  return (back_index_fetch(opt, sback, adr, fil, sav, /*don't fetch */ 0) >= 0);
}

// nombre de sockets en tâche de fond
int back_nsoc(const struct_back * sback) {
  const lien_back *const back = sback->lnk;
  const int back_max = sback->count;
  int n = 0;
  int i;

  for(i = 0; i < back_max; i++)
    if (back[i].status > 0)     // only receive
      n++;

  return n;
}
int back_nsoc_overall(const struct_back * sback) {
  const lien_back *const back = sback->lnk;
  const int back_max = sback->count;
  int n = 0;
  int i;

  for(i = 0; i < back_max; i++)
    if (back[i].status > 0 || back[i].status == STATUS_ALIVE)
      n++;

  return n;
}

/* generate temporary file on lien_back */
/* Note: utf-8 */
static int create_back_tmpfile(httrackp * opt, lien_back *const back) {
  // do not use tempnam() but a regular filename
  back->tmpfile_buffer[0] = '\0';
  if (back->url_sav != NULL && back->url_sav[0] != '\0') {
    snprintf(back->tmpfile_buffer, sizeof(back->tmpfile_buffer), "%s.z", 
             back->url_sav);
    back->tmpfile = back->tmpfile_buffer;
    if (structcheck(back->tmpfile) != 0) {
      hts_log_print(opt, LOG_WARNING, "can not create directory to %s", 
                    back->tmpfile);
      return -1;
    }
  } else {
    snprintf(back->tmpfile_buffer, sizeof(back->tmpfile_buffer),
             "%s/tmp%d.z", StringBuff(opt->path_html_utf8),
             opt->state.tmpnameid++);
    back->tmpfile = back->tmpfile_buffer;
  }
  /* OK */
  hts_log_print(opt, LOG_TRACE, "produced temporary name %s", back->tmpfile);
  return 0;
}

// objet (lien) téléchargé ou transféré depuis le cache
//
// fermer les paramètres de transfert,
// et notamment vérifier les fichiers compressés (décompresser), callback etc.
int back_finalize(httrackp * opt, cache_back * cache, struct_back * sback,
                  const int p) {
  char catbuff[CATBUFF_SIZE];
  lien_back *const back = sback->lnk;
  const int back_max = sback->count;

  assertf(p >= 0 && p < back_max);

  /* Store ? */
  if (!back[p].finalized) {
    back[p].finalized = 1;

    /* Don't store broken files. Note: check is done before compression.
       If the file is partial, the next run will attempt to continue it with compression too.
     */
    if (back[p].r.totalsize >= 0 && back[p].r.statuscode > 0
        && back[p].r.size != back[p].r.totalsize && !opt->tolerant) {
      if (back[p].status == STATUS_READY) {
        hts_log_print(opt, LOG_WARNING,
                      "file not stored in cache due to bogus state (broken size, expected "
                      LLintP " got " LLintP "): %s%s", back[p].r.totalsize,
                      back[p].r.size, back[p].url_adr, back[p].url_fil);
      } else {
        hts_log_print(opt, LOG_INFO,
                      "incomplete file not yet stored in cache (expected "
                      LLintP " got " LLintP "): %s%s", back[p].r.totalsize,
                      back[p].r.size, back[p].url_adr, back[p].url_fil);
      }
      return -1;
    }

    if ((back[p].status == STATUS_READY)        // ready
        && (back[p].r.statuscode > 0)   // not internal error
      ) {
      if (!back[p].testmode) {  // not test mode
        const char *state = "unknown";

        /* décompression */
#if HTS_USEZLIB
        if (back[p].r.compressed) {
          if (back[p].r.size > 0) {
            //if ( (back[p].r.adr) && (back[p].r.size>0) ) {
            // stats
            back[p].compressed_size = back[p].r.size;
            // en mémoire -> passage sur disque
            if (!back[p].r.is_write) {
              // do not use tempnam() but a regular filename
              if (create_back_tmpfile(opt, &back[p]) == 0) {
                assertf(back[p].tmpfile != NULL);
                /* note: tmpfile is utf-8 */
                back[p].r.out = FOPEN(back[p].tmpfile, "wb");
                if (back[p].r.out) {
                  if ((back[p].r.adr) && (back[p].r.size > 0)) {
                    if (fwrite
                        (back[p].r.adr, 1, (size_t) back[p].r.size,
                         back[p].r.out) != back[p].r.size) {
                      back[p].r.statuscode = STATUSCODE_INVALID;
                      strcpybuff(back[p].r.msg,
                                 "Write error when decompressing");
                    }
                  } else {
                    back[p].tmpfile[0] = '\0';
                    back[p].r.statuscode = STATUSCODE_INVALID;
                    strcpybuff(back[p].r.msg, "Empty compressed file");
                  }
                } else {
                  snprintf(back[p].r.msg, sizeof(back[p].r.msg),
                           "Open error when decompressing (can not create temporary file %s)",
                           back[p].tmpfile);
                  back[p].tmpfile[0] = '\0';
                  back[p].r.statuscode = STATUSCODE_INVALID;
                }
              } else {
                snprintf(back[p].r.msg, sizeof(back[p].r.msg),
                         "Open error when decompressing (can not generate a temporary file)");
              }
            }
            // fermer fichier sortie
            if (back[p].r.out != NULL) {
              fclose(back[p].r.out);
              back[p].r.out = NULL;
            }
            // décompression
            if (back[p].tmpfile != NULL) {
              if (back[p].url_sav[0]) {
                LLint size;

                file_notify(opt, back[p].url_adr, back[p].url_fil,
                            back[p].url_sav, 1, 1, back[p].r.notmodified);
                filecreateempty(&opt->state.strc, back[p].url_sav);     // filenote & co
                if ((size = hts_zunpack(back[p].tmpfile, back[p].url_sav)) >= 0) {
                  back[p].r.size = back[p].r.totalsize = size;
                  // fichier -> mémoire
                  if (!back[p].r.is_write) {
                    deleteaddr(&back[p].r);
                    back[p].r.adr = readfile_utf8(back[p].url_sav);
                    if (!back[p].r.adr) {
                      back[p].r.statuscode = STATUSCODE_INVALID;
                      strcpybuff(back[p].r.msg,
                                 "Read error when decompressing");
                    }
                    UNLINK(back[p].url_sav);
                  }
                } else {
                  back[p].r.statuscode = STATUSCODE_INVALID;
                  strcpybuff(back[p].r.msg, "Error when decompressing");
                }
              }
              /* ensure that no remaining temporary file exists */
              unlink(back[p].tmpfile);
              back[p].tmpfile = NULL;
            }
            // stats
            HTS_STAT.total_packed += back[p].compressed_size;
            HTS_STAT.total_unpacked += back[p].r.size;
            HTS_STAT.total_packedfiles++;
            // unflag
          }
        }
#endif
        /* Write mode to disk */
        if (back[p].r.is_write && back[p].r.adr != NULL) {
          freet(back[p].r.adr);
          back[p].r.adr = NULL;
        }

        /* remove reference file, if any */
        if (back[p].r.is_write) {
          url_savename_refname_remove(opt, back[p].url_adr, back[p].url_fil);
        }

        /* ************************************************************************
           REAL MEDIA HACK
           Check if we have to load locally the file
           ************************************************************************ */
        if (back[p].r.statuscode == HTTP_OK) {  // OK (ou 304 en backing)
          if (back[p].r.is_write) {     // Written file
            if (may_be_hypertext_mime(opt, back[p].r.contenttype, back[p].url_fil)) {   // to parse!
              off_t sz;

              sz = fsize_utf8(back[p].url_sav);
              if (sz > 0) {     // ok, exists!
                if (sz < 8192) {        // ok, small file --> to parse!
                  FILE *fp = FOPEN(back[p].url_sav, "rb");

                  if (fp) {
                    back[p].r.adr = malloct((size_t) sz + 1);
                    if (back[p].r.adr) {
                      if (fread(back[p].r.adr, 1, sz, fp) == sz) {
                        back[p].r.size = sz;
                        back[p].r.adr[sz] = '\0';
                        back[p].r.is_write = 0; /* not anymore a direct-to-disk file */
                      } else {
                        freet(back[p].r.adr);
                        back[p].r.size = 0;
                        back[p].r.adr = NULL;
                        back[p].r.statuscode = STATUSCODE_INVALID;
                        strcpybuff(back[p].r.msg, ".RAM read error");
                      }
                      fclose(fp);
                      fp = NULL;
                      // remove (temporary) file!
                      UNLINK(fconv(catbuff, sizeof(catbuff), back[p].url_sav));
                    }
                    if (fp)
                      fclose(fp);
                  }
                }
              }
            }
          }
        }
        /* EN OF REAL MEDIA HACK */

        /* Stats */
        if (cache->txt) {
          char flags[32];
          char s[256];
          time_t tt;
          struct tm *A;

          tt = time(NULL);
          A = localtime(&tt);
          if (A == NULL) {
            int localtime_returned_null = 0;

            assertf(localtime_returned_null);
          }
          strftime(s, 250, "%H:%M:%S", A);

          flags[0] = '\0';
          /* input flags */
          if (back[p].is_update)
            strcatbuff(flags, "U");     // update request
          else
            strcatbuff(flags, "-");
          if (back[p].range_req_size)
            strcatbuff(flags, "R");     // range request
          else
            strcatbuff(flags, "-");
          /* state flags */
          if (back[p].r.is_file)        // direct to disk
            strcatbuff(flags, "F");
          else
            strcatbuff(flags, "-");
          /* output flags */
          if (!back[p].r.notmodified)
            strcatbuff(flags, "M");     // modified
          else
            strcatbuff(flags, "-");
          if (back[p].r.is_chunk)       // chunked
            strcatbuff(flags, "C");
          else
            strcatbuff(flags, "-");
          if (back[p].r.compressed)
            strcatbuff(flags, "Z");     // gzip
          else
            strcatbuff(flags, "-");
          /* Err I had to split these.. */
          fprintf(cache->txt, "%s\t", s);
          fprintf(cache->txt, LLintP "/", (LLint) back[p].r.size);
          fprintf(cache->txt, LLintP, (LLint) back[p].r.totalsize);
          fprintf(cache->txt, "\t%s\t", flags);
        }
#if HTS_USEZLIB
        back[p].r.compressed = 0;
#endif

        if (back[p].r.statuscode == HTTP_OK) {
          if (back[p].r.size >= 0) {
            if (strcmp(back[p].url_fil, "/robots.txt") != 0) {
              HTS_STAT.stat_bytes += back[p].r.size;
              HTS_STAT.stat_files++;
              hts_log_print(opt, LOG_TRACE, "added file %s%s => %s",
                            back[p].url_adr, back[p].url_fil,
                            back[p].url_sav != NULL ? back[p].url_sav : "");
            }
            if ((!back[p].r.notmodified) && (opt->is_update)) {
              HTS_STAT.stat_updated_files++;    // page modifiée
              if (back[p].is_update) {
                hts_log_print(opt, LOG_INFO,
                              "engine: transfer-status: link updated: %s%s -> %s",
                              back[p].url_adr, back[p].url_fil,
                              back[p].url_sav);
              } else {
                hts_log_print(opt, LOG_INFO,
                              "engine: transfer-status: link added: %s%s -> %s",
                              back[p].url_adr, back[p].url_fil,
                              back[p].url_sav);
              }
              if (cache->txt) {
                if (back[p].is_update) {
                  state = "updated";
                } else {
                  state = "added";
                }
              }
            } else {
              hts_log_print(opt, LOG_INFO,
                            "engine: transfer-status: link recorded: %s%s -> %s",
                            back[p].url_adr, back[p].url_fil, back[p].url_sav);
              if (cache->txt) {
                if (opt->is_update)
                  state = "untouched";
                else
                  state = "added";
              }
            }
          } else {
            hts_log_print(opt, LOG_INFO,
                          "engine: transfer-status: empty file? (%d, '%s'): %s%s",
                          back[p].r.statuscode, back[p].r.msg, back[p].url_adr,
                          back[p].url_fil);
            if (cache->txt) {
              state = "empty";
            }
          }
        } else {
          hts_log_print(opt, LOG_INFO,
                        "engine: transfer-status: link error (%d, '%s'): %s%s",
                        back[p].r.statuscode, back[p].r.msg, back[p].url_adr,
                        back[p].url_fil);
          if (cache->txt) {
            state = "error";
          }
        }
        if (cache->txt) {
#undef ESC_URL
#define ESC_URL(S) escape_check_url_addr(S, OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt))
          fprintf(cache->txt,
                  "%d\t" "%s ('%s')\t" "%s\t" "%s%s\t" "%s%s%s\t%s\t"
                  "(from %s%s%s)" LF, back[p].r.statuscode, state,
                  ESC_URL(back[p].r.msg),
                  ESC_URL(back[p].r.contenttype),
                  ((back[p].r.etag[0]) ? "etag:" : ((back[p].r.
                                           lastmodified[0]) ? "date:" : "")),
                  ESC_URL((back[p].r.etag[0]) ? back[p].r.
                                        etag : (back[p].r.lastmodified)),
                  (link_has_authority(back[p].url_adr) ? "" : "http://"),
                  ESC_URL(back[p].url_adr),
                  ESC_URL(back[p].url_fil),
                  ESC_URL(back[p].url_sav),
                  (link_has_authority(back[p].referer_adr)
                   || !back[p].referer_adr[0]) ? "" : "http://",
                  ESC_URL(back[p].referer_adr),
                  ESC_URL(back[p].referer_fil)
            );
#undef ESC_URL
          if (opt->flush)
            fflush(cache->txt);
        }

        /* Cache */
        if (!IS_DELAYED_EXT(back[p].url_sav)) {
          cache_mayadd(opt, cache, &back[p].r, back[p].url_adr, back[p].url_fil,
                       back[p].url_sav);
        } else {
          /* error */
          if (!HTTP_IS_OK(back[p].r.statuscode)) {
            hts_log_print(opt, LOG_DEBUG, "redirect to %s%s", back[p].url_adr,
                          back[p].url_fil);
            /* Store only header reference */
            cache_mayadd(opt, cache, &back[p].r, back[p].url_adr,
                         back[p].url_fil, NULL);
          } else {
            /* Partial file, but marked as "ok" ? */
            hts_log_print(opt, LOG_WARNING,
                          "file not stored in cache due to bogus state (incomplete type with %s (%d), size "
                          LLintP "): %s%s", back[p].r.msg, back[p].r.statuscode,
                          (LLint) back[p].r.size, back[p].url_adr,
                          back[p].url_fil);
          }
        }

        // status finished callback
        RUN_CALLBACK1(opt, xfrstatus, &back[p]);

        return 0;
      } else {                  // testmode
        if (back[p].r.statuscode / 100 >= 3) {  /* Store 3XX, 4XX, 5XX test response codes, but NOT 2XX */
          /* Cache */
          cache_mayadd(opt, cache, &back[p].r, back[p].url_adr, back[p].url_fil,
                       NULL);
        }
      }
    }
  }
  return -1;
}

/* try to keep the connection alive */
int back_letlive(httrackp * opt, cache_back * cache, struct_back * sback,
                 const int p) {
  lien_back *const back = sback->lnk;
  const int back_max = sback->count;
  int checkerror;
  htsblk *src = &back[p].r;

  assertf(p >= 0 && p < back_max);
  if (src && !src->is_file && src->soc != INVALID_SOCKET && src->statuscode >= 0        /* no timeout errors & co */
      && src->keep_alive_trailers == 0  /* not yet supported (chunk trailers) */
      && !(checkerror = check_sockerror(src->soc))
      /*&& !check_sockdata(src->soc) *//* no unexpected data */
    ) {
    htsblk tmp;

    memset(&tmp, 0, sizeof(tmp));
    /* clear everything but connection: switch, close, and reswitch */
    back_connxfr(src, &tmp);
    back_delete(opt, cache, sback, p);
    //deletehttp(src);
    back_connxfr(&tmp, src);
    src->req.flush_garbage = 1; /* ignore CRLF garbage */
    return 1;
  }
  return 0;
}

void back_connxfr(htsblk * src, htsblk * dst) {
  dst->soc = src->soc;
  src->soc = INVALID_SOCKET;
#if HTS_USEOPENSSL
  dst->ssl = src->ssl;
  src->ssl = 0;
  dst->ssl_con = src->ssl_con;
  src->ssl_con = NULL;
#endif
  dst->keep_alive = src->keep_alive;
  src->keep_alive = 0;
  dst->keep_alive_max = src->keep_alive_max;
  src->keep_alive_max = 0;
  dst->keep_alive_t = src->keep_alive_t;
  src->keep_alive_t = 0;
  dst->debugid = src->debugid;
  src->debugid = 0;
}

void back_move(lien_back * src, lien_back * dst) {
  memcpy(dst, src, sizeof(lien_back));
  memset(src, 0, sizeof(lien_back));
  src->r.soc = INVALID_SOCKET;
  src->status = STATUS_FREE;
  src->r.location = src->location_buffer;
  dst->r.location = dst->location_buffer;
}

void back_copy_static(const lien_back * src, lien_back * dst) {
  memcpy(dst, src, sizeof(lien_back));
  dst->r.soc = INVALID_SOCKET;
  dst->r.adr = NULL;
  dst->r.headers = NULL;
  dst->r.out = NULL;
  dst->r.location = dst->location_buffer;
  dst->r.fp = NULL;
#if HTS_USEOPENSSL
  dst->r.ssl_con = NULL;
#endif
}

static int back_data_serialize(FILE * fp, const void *data, size_t size) {
  if (fwrite(&size, 1, sizeof(size), fp) == sizeof(size)
      && (size == 0 || fwrite(data, 1, size, fp) == size)
    )
    return 0;
  return 1;                     /* error */
}

static int back_string_serialize(FILE * fp, const char *str) {
  size_t size = (str != NULL) ? (strlen(str) + 1) : 0;

  return back_data_serialize(fp, str, size);
}

static int back_data_unserialize(FILE * fp, void **str, size_t * size) {
  *str = NULL;
  if (fread(size, 1, sizeof(*size), fp) == sizeof(*size)) {
    if (*size == 0)             /* serialized NULL ptr */
      return 0;
    *str = malloct(*size + 1);
    if (*str == NULL)
      return 1;                 /* error */
    ((char *) *str)[*size] = 0; /* guard byte */
    if (fread(*str, 1, *size, fp) == *size)
      return 0;
  }
  return 1;                     /* error */
}

static int back_string_unserialize(FILE * fp, char **str) {
  size_t dummy;

  return back_data_unserialize(fp, (void **) str, &dummy);
}

int back_serialize(FILE * fp, const lien_back * src) {
  if (back_data_serialize(fp, src, sizeof(lien_back)) == 0
      && back_data_serialize(fp, src->r.adr,
                             src->r.adr ? (size_t) src->r.size : 0) == 0
      && back_string_serialize(fp, src->r.headers) == 0 && fflush(fp) == 0)
    return 0;
  return 1;
}

int back_unserialize(FILE * fp, lien_back ** dst) {
  size_t size;

  *dst = NULL;
  errno = 0;
  if (back_data_unserialize(fp, (void **) dst, &size) == 0
      && size == sizeof(lien_back)) {
    (*dst)->tmpfile = NULL;
    (*dst)->chunk_adr = NULL;
    (*dst)->r.adr = NULL;
    (*dst)->r.out = NULL;
    (*dst)->r.location = (*dst)->location_buffer;
    (*dst)->r.fp = NULL;
    (*dst)->r.soc = INVALID_SOCKET;
#if HTS_USEOPENSSL
    (*dst)->r.ssl_con = NULL;
#endif
    if (back_data_unserialize(fp, (void **) &(*dst)->r.adr, &size) == 0) {
      (*dst)->r.size = size;
      (*dst)->r.headers = NULL;
      if (back_string_unserialize(fp, &(*dst)->r.headers) == 0)
        return 0;               /* ok */
      if ((*dst)->r.headers != NULL)
        freet((*dst)->r.headers);
    }
    if ((*dst)->r.adr != NULL)
      freet((*dst)->r.adr);
  }
  if (dst != NULL) {
    freet(*dst);
    *dst = NULL;
  }
  return 1;                     /* error */
}

/* serialize a reference ; used to store references of files being downloaded in case of broken download */
/* Note: NOT utf-8 */
int back_serialize_ref(httrackp * opt, const lien_back * src) {
  const char *filename =
    url_savename_refname_fullpath(opt, src->url_adr, src->url_fil);
  FILE *fp = fopen(filename, "wb");

  if (fp == NULL) {
#ifdef _WIN32
    if (mkdir
        (fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log), CACHE_REFNAME))
        == 0)
#else
    if (mkdir
        (fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), StringBuff(opt->path_log), CACHE_REFNAME),
         S_IRWXU | S_IRWXG | S_IRWXO) == 0)
#endif
    {
      /* note: local filename */
      filename = url_savename_refname_fullpath(opt, src->url_adr, src->url_fil);
      fp = fopen(filename, "wb");
    }
  }
  if (fp != NULL) {
    int ser = back_serialize(fp, src);

    fclose(fp);
    return ser;
  }
  return 1;
}

/* unserialize a reference ; used to store references of files being downloaded in case of broken download */
int back_unserialize_ref(httrackp * opt, const char *adr, const char *fil,
                         lien_back ** dst) {
  const char *filename = url_savename_refname_fullpath(opt, adr, fil);
  FILE *fp = FOPEN(filename, "rb");

  if (fp != NULL) {
    int ser = back_unserialize(fp, dst);

    fclose(fp);
    if (ser != 0) {             /* back_unserialize_ref() != 0 does not need cleaning up */
      back_clear_entry(*dst);   /* delete entry content */
      freet(*dst);              /* delete item */
      *dst = NULL;
    }
    return ser;
  }
  return 1;
}

// clear, or leave for keep-alive
int back_maydelete(httrackp * opt, cache_back * cache, struct_back * sback,
                   const int p) {
  lien_back *const back = sback->lnk;
  const int back_max = sback->count;

  assertf(p >= 0 && p < back_max);
  if (p >= 0 && p < back_max) { // on sait jamais..
    if (
         /* Keep-alive authorized by user */
         !opt->nokeepalive
         /* Socket currently is keep-alive! */
         && back[p].r.keep_alive
         /* Remaining authorized requests */
         && back[p].r.keep_alive_max > 1
         /* Known keep-alive start (security) */
         && back[p].ka_time_start
         /* We're on time */
         && time_local() < back[p].ka_time_start + back[p].r.keep_alive_t
         /* Connection delay must not exceed keep-alive timeout */
         && (opt->maxconn <= 0
             || (back[p].r.keep_alive_t > (1.0 / opt->maxconn)))
      ) {
      lien_back tmp;

      strcpybuff(tmp.url_adr, back[p].url_adr);
      tmp.ka_time_start = back[p].ka_time_start;
      if (back_letlive(opt, cache, sback, p)) {
        strcpybuff(back[p].url_adr, tmp.url_adr);
        back[p].ka_time_start = tmp.ka_time_start;
        back[p].status = STATUS_ALIVE;  // alive & waiting
        assertf(back[p].ka_time_start != 0);
        hts_log_print(opt, LOG_DEBUG,
                      "(Keep-Alive): successfully saved #%d (%s)",
                      back[p].r.debugid, back[p].url_adr);
        return 1;
      }
    }
    back_delete(opt, cache, sback, p);
  }
  return 0;
}

// clear, or leave for keep-alive
void back_maydeletehttp(httrackp * opt, cache_back * cache, struct_back * sback,
                        const int p) {
  lien_back *const back = sback->lnk;
  const int back_max = sback->count;
  TStamp lt = 0;

  assertf(p >= 0 && p < back_max);
  if (back[p].r.soc != INVALID_SOCKET) {
    int q;

    if (back[p].r.soc != INVALID_SOCKET /* security check */
        && back[p].r.statuscode >= 0    /* no timeout errors & co */
        && back[p].r.keep_alive_trailers == 0   /* not yet supported (chunk trailers) */
        /* Socket not in I/O error status */
        && !back[p].r.is_file && !check_sockerror(back[p].r.soc)
        /* Keep-alive authorized by user */
        && !opt->nokeepalive
        /* Socket currently is keep-alive! */
        && back[p].r.keep_alive
        /* Remaining authorized requests */
        && back[p].r.keep_alive_max > 1
        /* Known keep-alive start (security) */
        && back[p].ka_time_start
        /* We're on time */
        && (lt = time_local()) < back[p].ka_time_start + back[p].r.keep_alive_t
        /* Connection delay must not exceed keep-alive timeout */
        && (opt->maxconn <= 0
            || (back[p].r.keep_alive_t > (1.0 / opt->maxconn)))
        /* Available slot in backing */
        && (q = back_search(opt, sback)) >= 0) {
      lien_back tmp;

      strcpybuff(tmp.url_adr, back[p].url_adr);
      tmp.ka_time_start = back[p].ka_time_start;
      deletehttp(&back[q].r);   // security check
      back_connxfr(&back[p].r, &back[q].r);     // transfer live connection settings from p to q
      back[q].ka_time_start = back[p].ka_time_start;    // refresh
      back[p].r.soc = INVALID_SOCKET;
      strcpybuff(back[q].url_adr, tmp.url_adr); // address
      back[q].ka_time_start = tmp.ka_time_start;
      back[q].status = STATUS_ALIVE;    // alive & waiting
      assertf(back[q].ka_time_start != 0);
      hts_log_print(opt, LOG_DEBUG,
                    "(Keep-Alive): successfully preserved #%d (%s)",
                    back[q].r.debugid, back[q].url_adr);
    } else {
      deletehttp(&back[p].r);
      back[p].r.soc = INVALID_SOCKET;
    }
  }
}

/* attempt to attach a live connection to this slot */
int back_trylive(httrackp * opt, cache_back * cache, struct_back * sback,
                 const int p) {
  lien_back *const back = sback->lnk;
  const int back_max = sback->count;

  assertf(p >= 0 && p < back_max);
  if (p >= 0 && back[p].status != STATUS_ALIVE) {       // we never know..
    int i = back_searchlive(opt, sback, back[p].url_adr);       // search slot

    if (i >= 0 && i != p) {
      deletehttp(&back[p].r);   // security check
      back_connxfr(&back[i].r, &back[p].r);     // transfer live connection settings from i to p
      back[p].ka_time_start = back[i].ka_time_start;
      back_delete(opt, cache, sback, i);        // delete old slot
      back[p].status = STATUS_CONNECTING;       // ready to connect
      return 1;                 // success: will reuse live connection
    }
  }
  return 0;
}

/* search for a live position, or, if not possible, try to return a new one */
int back_searchlive(httrackp * opt, struct_back * sback, const char *search_addr) {
  lien_back *const back = sback->lnk;
  const int back_max = sback->count;
  int i;

  /* search for a live socket */
  for(i = 0; i < back_max; i++) {
    if (back[i].status == STATUS_ALIVE) {
      if (strfield2(back[i].url_adr, search_addr)) {    /* same location (xxc: check also virtual hosts?) */
        if (time_local() < back[i].ka_time_start + back[i].r.keep_alive_t) {
          return i;
        }
      }
    }
  }
  return -1;
}

int back_search_quick(struct_back * sback) {
  lien_back *const back = sback->lnk;
  const int back_max = sback->count;
  int i;

  /* try to find an empty place */
  for(i = 0; i < back_max; i++) {
    if (back[i].status == STATUS_FREE) {
      return i;
    }
  }

  /* oops, can't find a place */
  return -1;
}

int back_search(httrackp * opt, struct_back * sback) {
  lien_back *const back = sback->lnk;
  const int back_max = sback->count;
  int i;

  /* try to find an empty place */
  if ((i = back_search_quick(sback)) != -1)
    return i;

  /* couldn't find an empty place, try to requisition a keep-alive place */
  for(i = 0; i < back_max; i++) {
    if (back[i].status == STATUS_ALIVE) {
      lien_back *const back = sback->lnk;

      /* close this place */
      back_clear_entry(&back[i]);       /* Already finalized (this is the night of the living dead) */
      /*back_delete(opt,cache,sback, i); */
      return i;
    }
  }

  /* oops, can't find a place */
  return -1;
}

void back_set_finished(struct_back * sback, const int p) {
  lien_back *const back = sback->lnk;
  const int back_max = sback->count;

  assertf(p >= 0 && p < back_max);
  if (p >= 0 && p < sback->count) {     // we never know..
    /* status: finished (waiting to be validated) */
    back[p].status = STATUS_READY;      /* finished */
    /* close open r/w streams, if any */
    if (back[p].r.fp != NULL) {
      fclose(back[p].r.fp);
      back[p].r.fp = NULL;
    }
    if (back[p].r.out != NULL) {        // fermer fichier sortie
      fclose(back[p].r.out);
      back[p].r.out = NULL;
    }
  }
}

void back_set_locked(struct_back * sback, const int p) {
  lien_back *const back = sback->lnk;
  const int back_max = sback->count;

  assertf(p >= 0 && p < back_max);
  if (p >= 0 && p < sback->count) {
    /* status: locked (in process, do not swap on disk) */
    back[p].locked = 1;         /* locked */
  }
}

void back_set_unlocked(struct_back * sback, const int p) {
  lien_back *const back = sback->lnk;
  const int back_max = sback->count;

  assertf(p >= 0 && p < back_max);
  if (p >= 0 && p < sback->count) {
    /* status: unlocked (can be swapped on disk) */
    back[p].locked = 0;         /* unlocked */
  }
}

int back_flush_output(httrackp * opt, cache_back * cache, struct_back * sback,
                      const int p) {
  lien_back *const back = sback->lnk;
  const int back_max = sback->count;

  assertf(p >= 0 && p < back_max);
  if (p >= 0 && p < sback->count) {     // on sait jamais..
    /* close input file */
    if (back[p].r.fp != NULL) {
      fclose(back[p].r.fp);
      back[p].r.fp = NULL;
    }
    /* fichier de sortie */
    if (back[p].r.out != NULL) {        // fermer fichier sortie
      fclose(back[p].r.out);
      back[p].r.out = NULL;
    }
    /* set file time */
    if (back[p].r.is_write) {   // ecriture directe
      /* écrire date "remote" */
      if (strnotempty(back[p].url_sav)
          && strnotempty(back[p].r.lastmodified)
          && fexist_utf8(back[p].url_sav))      // normalement existe si on a un fichier de sortie
      {
        set_filetime_rfc822(back[p].url_sav, back[p].r.lastmodified);
      }
      /* executer commande utilisateur après chargement du fichier */
      //xx usercommand(opt,0,NULL,back[p].url_sav, back[p].url_adr, back[p].url_fil);
      back[p].r.is_write = 0;
    }
    return 1;
  }
  return 0;
}

// effacer entrée
int back_delete(httrackp * opt, cache_back * cache, struct_back * sback,
                const int p) {
  lien_back *const back = sback->lnk;
  const int back_max = sback->count;

  assertf(p >= 0 && p < back_max);
  if (p >= 0 && p < sback->count) {     // on sait jamais..
    // Vérificateur d'intégrité
#if DEBUG_CHECKINT
    _CHECKINT(&back[p], "Appel back_delete")
#endif
#if HTS_DEBUG_CLOSESOCK
      DEBUG_W("back_delete: #%d\n" _(int) p);
#endif

    // Finalize
    if (!back[p].finalized) {
      if ((back[p].status == STATUS_READY)      // ready
          && (!back[p].testmode)        // not test mode
          && (back[p].r.statuscode > 0) // not internal error
        ) {
        hts_log_print(opt, LOG_DEBUG,
                      "File '%s%s' -> %s not yet saved in cache - saving now",
                      back[p].url_adr, back[p].url_fil, back[p].url_sav);
      }
      if (cache != NULL) {
        //hts_log_print(opt, LOG_TRACE, "finalizing from back_delete");
        back_finalize(opt, cache, sback, p);
      }
    }
    back[p].finalized = 0;

    // flush output buffers
    (void) back_flush_output(opt, cache, sback, p);

    return back_clear_entry(&back[p]);
  }
  return 0;
}

/* ensure that the entry is not locked */
void back_index_unlock(struct_back * sback, const int p) {
  lien_back *const back = sback->lnk;

  if (back[p].locked) {
    back[p].locked = 0;         /* not locked anymore */
  }
}

/* the entry is available again */
static void back_set_free(lien_back * back) {
  back->locked = 0;
  back->status = STATUS_FREE;
}

/* delete entry content (clear the entry), but don't unallocate the entry itself */
int back_clear_entry(lien_back * back) {
  if (back != NULL) {
    // Libérer tous les sockets, handles, buffers..
    if (back->r.soc != INVALID_SOCKET) {
#if HTS_DEBUG_CLOSESOCK
      DEBUG_W("back_delete: deletehttp\n");
#endif
      deletehttp(&back->r);
      back->r.soc = INVALID_SOCKET;
    }

    if (back->r.adr != NULL) {  // reste un bloc à désallouer
      freet(back->r.adr);
      back->r.adr = NULL;
    }
    if (back->chunk_adr != NULL) {      // reste un bloc à désallouer
      freet(back->chunk_adr);
      back->chunk_adr = NULL;
      back->chunk_size = 0;
      back->chunk_blocksize = 0;
      back->is_chunk = 0;
    }
    // only for security
    if (back->tmpfile && back->tmpfile[0] != '\0') {
      (void) unlink(back->tmpfile);
      back->tmpfile = NULL;
    }
    // headers
    if (back->r.headers != NULL) {
      freet(back->r.headers);
      back->r.headers = NULL;
    }
    // Tout nettoyer
    memset(back, 0, sizeof(lien_back));
    back->r.soc = INVALID_SOCKET;
    back->r.location = back->location_buffer;

    // Le plus important: libérer le champ
    back_set_free(back);

    return 1;
  }
  return 0;
}

/* Space left on backing stack */
int back_stack_available(struct_back * sback) {
  lien_back *const back = sback->lnk;
  const int back_max = sback->count;
  int p = 0, n = 0;

  for(; p < back_max; p++)
    if (back[p].status == STATUS_FREE)
      n++;
  return n;
}

// ajouter un lien en backing
int back_add_if_not_exists(struct_back * sback, httrackp * opt,
                           cache_back * cache, const char *adr, const char *fil, const char *save,
                           const char *referer_adr, const char *referer_fil, int test) {
  back_clean(opt, cache, sback);        /* first cleanup the backlog to ensure that we have some entry left */
  if (!back_exist(sback, opt, adr, fil, save)) {
    return back_add(sback, opt, cache, adr, fil, save, referer_adr, referer_fil,
                    test);
  }
  return 0;
}

int back_add(struct_back * sback, httrackp * opt, cache_back * cache, const char *adr,
             const char *fil, const char *save, const char *referer_adr, const char *referer_fil,
             int test) {
  lien_back *const back = sback->lnk;
  const int back_max = sback->count;
  int p = 0;
  char catbuff[CATBUFF_SIZE];
  char catbuff2[CATBUFF_SIZE];
  lien_back *itemback = NULL;

#if (defined(_DEBUG) || defined(DEBUG))
  if (!test && back_exist(sback, opt, adr, fil, save)) {
    int already_there = 0;

    hts_log_print(opt, LOG_ERROR, "error: back_add(%s,%s,%s) duplicate", adr,
                  fil, save);
  }
#endif

  // vérifier cohérence de adr et fil (non vide!)
  if (strnotempty(adr) == 0) {
    hts_log_print(opt, LOG_WARNING, "error: adr is empty for back_add");
    return -1;                  // erreur!
  }
  if (strnotempty(fil) == 0) {
    hts_log_print(opt, LOG_WARNING, "error: fil is empty for back_add");
    return -1;                  // erreur!
  }
  // FIN vérifier cohérence de adr et fil (non vide!)

  // stats
  opt->state.back_add_stats++;

  // rechercher emplacement
  back_clean(opt, cache, sback);
  if ((p = back_search(opt, sback)) >= 0) {
    back[p].send_too[0] = '\0'; // éventuels paramètres supplémentaires à transmettre au serveur

    // clear r
    if (back[p].r.soc != INVALID_SOCKET) {      /* we never know */
      deletehttp(&back[p].r);
    }
    //memset(&(back[p].r), 0, sizeof(htsblk)); 
    hts_init_htsblk(&back[p].r);
    back[p].r.location = back[p].location_buffer;

    // créer entrée
    strcpybuff(back[p].url_adr, adr);
    strcpybuff(back[p].url_fil, fil);
    strcpybuff(back[p].url_sav, save);
    //back[p].links_index = links_index;
    // copier referer si besoin
    strcpybuff(back[p].referer_adr, "");
    strcpybuff(back[p].referer_fil, "");
    if ((referer_adr) && (referer_fil)) {       // existe
      if ((strnotempty(referer_adr)) && (strnotempty(referer_fil))) {   // non vide
        if (referer_adr[0] != '!') {    // non détruit
          if (strcmp(referer_adr, "file://")) { // PAS file://
            if (strcmp(referer_adr, "primary")) {       // pas referer 1er lien
              strcpybuff(back[p].referer_adr, referer_adr);
              strcpybuff(back[p].referer_fil, referer_fil);
            }
          }
        }
      }
    }
    // sav ne sert à rien pour le moment
    back[p].r.size = 0;         // rien n'a encore été chargé
    back[p].r.adr = NULL;       // pas de bloc de mémoire
    back[p].r.is_write = 0;     // à priori stockage en mémoire
    back[p].maxfile_html = opt->maxfile_html;
    back[p].maxfile_nonhtml = opt->maxfile_nonhtml;
    back[p].testmode = test;    // mode test?
    if (!opt->http10)           // option "forcer 1.0" désactivée
      back[p].http11 = 1;       // autoriser http/1.1
    back[p].head_request = 0;
    if (strcmp(back[p].url_sav, BACK_ADD_TEST) == 0)    // HEAD
      back[p].head_request = 1;
    else if (strcmp(back[p].url_sav, BACK_ADD_TEST2) == 0)      // test en GET
      back[p].head_request = 2; // test en get

    /* Stop requested - abort backing */
    /* For update mode: second check after cache lookup not to lose all previous cache data ! */
    if (opt->state.stop && !opt->is_update) {
      back[p].r.statuscode = STATUSCODE_INVALID;        // fatal
      strcpybuff(back[p].r.msg, "mirror stopped by user");
      back[p].status = STATUS_READY;    // terminé
      back_set_finished(sback, p);
      hts_log_print(opt, LOG_WARNING,
                    "File not added due to mirror cancel: %s%s", adr, fil);
      return 0;
    }
    // test "fast header" cache ; that is, tests we did that lead to 3XX/4XX/5XX response codes
    if (cache->cached_tests != NULL) {
      intptr_t ptr = 0;

      if (coucal_read(cache->cached_tests,
        concat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), adr, fil), &ptr)) {       // gotcha
        if (ptr != 0) {
          char *text = (char *) ptr;
          char *lf = strchr(text, '\n');
          int code = 0;

          if (sscanf(text, "%d", &code) == 1) { // got code
            back[p].r.statuscode = code;
            back[p].status = STATUS_READY;      // terminé
            if (lf != NULL && *lf != '\0') {    // got location ?
              strcpybuff(back[p].r.location, lf + 1);
            }
            return 0;
          }
        }
      }
    }
    // tester cache
    if ((strcmp(adr, "file://"))        /* pas fichier */
        &&((!test) || (cache->type == 1))       /* cache prioritaire, laisser passer en test! */
        &&((strnotempty(save)) || (strcmp(fil, "/robots.txt") == 0))) { // si en test on ne doit pas utiliser le cache sinon telescopage avec le 302..
#if HTS_FAST_CACHE
      intptr_t hash_pos;
      int hash_pos_return = 0;
#else
      char *a = NULL;
#endif
#if HTS_FAST_CACHE
      if (cache->hashtable) {
#else
      if (cache->use) {
#endif
        char BIGSTK buff[HTS_URLMAXSIZE * 4];

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
        a = strstr(cache->use, buff);
#endif

        // Ok, noté en cache->. mais bien présent dans le cache ou sur disque?
#if HTS_FAST_CACHE
        // negative values when data is not in cache
        if (hash_pos_return < 0) {
#else
        if (a) {
#endif
          if (!test) {          // non mode test
#if HTS_FAST_CACHE==0
            int pos = -1;

            a += strlen(buff);
            sscanf(a, "%d", &pos);      // lire position
#endif

#if HTS_FAST_CACHE==0
            if (pos < 0) {      // pas de mise en cache data, vérifier existence
#endif
              /* note: no check with IS_DELAYED_EXT() enabled - postcheck by client please! */
              if (save[0] != '\0' && !IS_DELAYED_EXT(save) && fsize_utf8(fconv(catbuff, sizeof(catbuff), save)) <= 0) {  // fichier final n'existe pas ou est vide!
                int found = 0;

                /* It is possible that the file has been moved due to changes in build structure */
                {
                  char BIGSTK previous_save[HTS_URLMAXSIZE * 2];
                  htsblk r;

                  previous_save[0] = '\0';
                  r =
                    cache_readex(opt, cache, adr, fil, /*head */ NULL,
                                 /*bound to back[p] (temporary) */
                                 back[p].location_buffer, previous_save, /*ro */
                                 1);
                  /* Is supposed to be on disk only */
                  if (r.is_write && previous_save[0] != '\0') {
                    /* Exists, but with another (old) filename: rename (almost) silently */
                    if (strcmp(previous_save, save) != 0
                        && fexist_utf8(fconv(catbuff, sizeof(catbuff), previous_save))) {
                      rename(fconv(catbuff, sizeof(catbuff), previous_save),
                             fconv(catbuff2, sizeof(catbuff2), save));
                      if (fexist_utf8(fconv(catbuff, sizeof(catbuff), save))) {
                        found = 1;
                        hts_log_print(opt, LOG_DEBUG,
                                      "File '%s' has been renamed since last mirror to '%s' ; applying changes",
                                      previous_save, save);
                      } else {
                        hts_log_print(opt, LOG_ERROR,
                                      "Could not rename '%s' to '%s' ; will have to retransfer it",
                                      previous_save, save);
                      }
                    }
                  }
                  back[p].location_buffer[0] = '\0';
                }

                /* Not found ? */
                if (!found) {
#if HTS_FAST_CACHE
                  hash_pos_return = 0;
#else
                  a = NULL;
#endif
                  // dévalider car non présent sur disque dans structure originale!!!
                  // sinon, le fichier est ok à priori, mais on renverra un if-modified-since pour
                  // en être sûr
                  if (opt->norecatch) { // tester norecatch
                    if (!fexist_utf8(fconv(catbuff, sizeof(catbuff), save))) {   // fichier existe pas mais déclaré: on l'a effacé
                      FILE *fp = FOPEN(fconv(catbuff, sizeof(catbuff), save), "wb");

                      if (fp)
                        fclose(fp);
                      hts_log_print(opt, LOG_WARNING,
                                    "Previous file '%s' not found (erased by user ?), ignoring: %s%s",
                                    save, back[p].url_adr, back[p].url_fil);
                    }
                  } else {
                    hts_log_print(opt, LOG_WARNING,
                                  "Previous file '%s' not found (erased by user ?), recatching: %s%s",
                                  save, back[p].url_adr, back[p].url_fil);
                  }
                }
              }                 // fsize() <= 0
#if HTS_FAST_CACHE==0
            }
#endif
          }
        }
        //
      } else {
#if HTS_FAST_CACHE
        hash_pos_return = 0;
#else
        a = NULL;
#endif
      }

      // Existe pas en cache, ou bien pas de cache présent
#if HTS_FAST_CACHE
      if (hash_pos_return) {    // OK existe en cache (et données aussi)!
#else
      if (a != NULL) {          // OK existe en cache (et données aussi)!
#endif
        const int cache_is_prioritary = cache->type == 1
          || opt->state.stop != 0;
        if (cache_is_prioritary) {      // cache prioritaire (pas de test if-modified..)
          // dans ce cas on peut également lire des réponses cachées comme 404,302...
          // lire dans le cache
          if (!test)
            back[p].r =
              cache_read(opt, cache, adr, fil, save, back[p].location_buffer);
          else
            back[p].r = cache_read(opt, cache, adr, fil, NULL, back[p].location_buffer);        // charger en tête uniquement du cache

          /* ensure correct location buffer set */
          back[p].r.location = back[p].location_buffer;

          /* Interdiction taille par le wizard? --> détruire */
          if (back[p].r.statuscode != -1) {     // pas d'erreur de lecture
            if (!back_checksize(opt, &back[p], 0)) {
              back[p].status = STATUS_READY;    // FINI
              back_set_finished(sback, p);
              back[p].r.statuscode = STATUSCODE_TOO_BIG;
              if (!back[p].testmode)
                strcpybuff(back[p].r.msg, "Cached file skipped (too big)");
              else
                strcpybuff(back[p].r.msg,
                           "Test: Cached file skipped  (too big)");
              return 0;
            }
          }

          if (back[p].r.statuscode != -1 || IS_DELAYED_EXT(save)) {     // pas d'erreur de lecture ou test retardé
            if (!test) {
              hts_log_print(opt, LOG_DEBUG,
                            "File immediately loaded from cache: %s%s",
                            back[p].url_adr, back[p].url_fil);
            } else {
              hts_log_print(opt, LOG_DEBUG,
                            "File immediately tested from cache: %s%s",
                            back[p].url_adr, back[p].url_fil);
            }
            back[p].r.notmodified = 1;  // fichier non modifié
            back[p].status = STATUS_READY;      // OK prêt
            //file_notify(back[p].url_adr, back[p].url_fil, back[p].url_sav, 0, 0, back[p].r.notmodified);        // not modified
            back_set_finished(sback, p);

            // finalize transfer
            if (!test) {
              if (back[p].r.statuscode > 0) {
                hts_log_print(opt, LOG_TRACE, "finalizing in back_add");
                back_finalize(opt, cache, sback, p);
              }
            }

            return 0;
          } else {              // erreur
            // effacer r
            hts_init_htsblk(&back[p].r);
            //memset(&(back[p].r), 0, sizeof(htsblk)); 
            back[p].r.location = back[p].location_buffer;
            // et continuer (chercher le fichier)
          }

        } else if (cache->type == 2) {  // si en cache, demander de tester If-Modified-Since
          htsblk r;

          cache_header(opt, cache, adr, fil, &r);

          /* Interdiction taille par le wizard? */
          {
            LLint save_totalsize = back[p].r.totalsize;

            back[p].r.totalsize = r.totalsize;
            if (!back_checksize(opt, &back[p], 1)) {
              r.statuscode = STATUSCODE_INVALID;
              //
              back[p].status = STATUS_READY;    // FINI
              back_set_finished(sback, p);
              back[p].r.statuscode = STATUSCODE_TOO_BIG;
              deletehttp(&back[p].r);
              back[p].r.soc = INVALID_SOCKET;
              if (!back[p].testmode)
                strcpybuff(back[p].r.msg, "File too big");
              else
                strcpybuff(back[p].r.msg, "Test: File too big");
              return 0;
            }
            back[p].r.totalsize = save_totalsize;
          }

          if (r.statuscode != -1) {
            if (r.statuscode == HTTP_OK) {      // uniquement des 200 (OK)
              if (strnotempty(r.etag)) {        // ETag (RFC2616)
                /*
                   - If both an entity tag and a Last-Modified value have been
                   provided by the origin server, SHOULD use both validators in
                   cache-conditional requests. This allows both HTTP/1.0 and
                   HTTP/1.1 caches to respond appropriately.
                 */
                if (strnotempty(r.lastmodified))
                  sprintf(back[p].send_too,
                          "If-None-Match: %s\r\nIf-Modified-Since: %s\r\n",
                          r.etag, r.lastmodified);
                else
                  sprintf(back[p].send_too, "If-None-Match: %s\r\n", r.etag);
              } else if (strnotempty(r.lastmodified))
                sprintf(back[p].send_too, "If-Modified-Since: %s\r\n",
                        r.lastmodified);
              else if (strnotempty(cache->lastmodified))
                sprintf(back[p].send_too, "If-Modified-Since: %s\r\n",
                        cache->lastmodified);

              /* this is an update of a file */
              if (strnotempty(back[p].send_too))
                back[p].is_update = 1;
              back[p].r.req.nocompression = 1;  /* Do not compress when updating! */

            }
          }
#if DEBUGCA
          printf("..is modified test %s\n", back[p].send_too);
#endif
        }
      }
      /* Not in cache ; maybe in temporary cache ? Warning: non-movable "url_sav" */
      else if (back_unserialize_ref(opt, adr, fil, &itemback) == 0) {
        const off_t file_size = fsize_utf8(itemback->url_sav);

        /* Found file on disk */
        if (file_size > 0) {
          char *send_too = back[p].send_too;

          sprintf(send_too, "Range: bytes=" LLintP "-\r\n", (LLint) file_size);
          send_too += strlen(send_too);
          /* add etag information */
          if (strnotempty(itemback->r.etag)) {
            sprintf(send_too, "If-Match: %s\r\n", itemback->r.etag);
            send_too += strlen(send_too);
          }
          /* add date information */
          if (strnotempty(itemback->r.lastmodified)) {
            sprintf(send_too, "If-Unmodified-Since: %s\r\n",
                    itemback->r.lastmodified);
            send_too += strlen(send_too);
          }
          back[p].http11 = 1;   /* 1.1 */
          back[p].range_req_size = (LLint) file_size;
          back[p].r.req.range_used = 1;
          back[p].is_update = 1;        /* this is an update of a file */
          back[p].r.req.nocompression = 1;      /* Do not compress when updating! */
        } else {
          /* broken ; remove */
          url_savename_refname_remove(opt, adr, fil);
        }
        /* cleanup */
        back_clear_entry(itemback);     /* delete entry content */
        freet(itemback);        /* delete item */
        itemback = NULL;
      }
      /* Not in cache or temporary cache ; found on disk ? (hack) */
      else if (fexist_utf8(save)) {
        const off_t sz = fsize_utf8(save);

        // Bon, là il est possible que le fichier ait été partiellement transféré
        // (s'il l'avait été en totalité il aurait été inscrit dans le cache ET existerait sur disque)
        // PAS de If-Modified-Since, on a pas connaissance des données à la date du cache
        // On demande juste les données restantes si le date est valide (206), tout sinon (200)
        if ((ishtml(opt, save) != 1) && (ishtml(opt, back[p].url_fil) != 1)) {  // NON HTML (liens changés!!)
          if (sz > 0) {         // Fichier non vide? (question bête, sinon on transfert tout!)
            char lastmodified[256];

            get_filetime_rfc822(save, lastmodified);
            if (strnotempty(lastmodified)) {    /* pas de If-.. possible */
#if DEBUGCA
              printf("..if unmodified since %s size " LLintP "\n", lastmodified,
                     (LLint) sz);
#endif
              hts_log_print(opt, LOG_DEBUG,
                            "File partially present (" LLintP " bytes): %s%s",
                            (LLint) sz, back[p].url_adr, back[p].url_fil);

              /* impossible - don't have etag or date
                 if (strnotempty(back[p].r.etag)) {  // ETag (RFC2616)
                 sprintf(back[p].send_too,"If-None-Match: %s\r\n",back[p].r.etag);
                 back[p].http11=1;    // En tête 1.1
                 } else if (strnotempty(back[p].r.lastmodified)) {
                 sprintf(back[p].send_too,"If-Unmodified-Since: %s\r\n",back[p].r.lastmodified);
                 back[p].http11=1;    // En tête 1.1
                 } else 
               */
              if (strlen(lastmodified)) {
                sprintf(back[p].send_too,
                        "If-Unmodified-Since: %s\r\nRange: bytes=" LLintP
                        "-\r\n", lastmodified, (LLint) sz);
                back[p].http11 = 1;     // En tête 1.1
                back[p].is_update = 1;  /* this is an update of a file */
                back[p].range_req_size = sz;
                back[p].r.req.range_used = 1;
                back[p].r.req.nocompression = 1;
              } else {
                hts_log_print(opt, LOG_WARNING,
                              "Could not find timestamp for partially present file, restarting (lost "
                              LLintP " bytes): %s%s", (LLint) sz,
                              back[p].url_adr, back[p].url_fil);
              }

            } else {
              hts_log_print(opt, LOG_NOTICE,
                            "File partially present (" LLintP
                            " bytes) retransferred due to lack of cache: %s%s",
                            (LLint) sz, back[p].url_adr, back[p].url_fil);
              /* Sinon requête normale... */
              back[p].http11 = 0;
            }
          } else if (opt->norecatch) {  // tester norecatch
            filenote(&opt->state.strc, save, NULL);     // ne pas purger tout de même
            file_notify(opt, back[p].url_adr, back[p].url_fil, back[p].url_sav,
                        0, 0, back[p].r.notmodified);
            back[p].status = STATUS_READY;      // OK prêt
            back_set_finished(sback, p);
            back[p].r.statuscode = STATUSCODE_INVALID;  // erreur
            strcpybuff(back[p].r.msg, "Null-size file not recaught");
            return 0;
          }
        } else {
          hts_log_print(opt, LOG_NOTICE,
                        "HTML file (" LLintP
                        " bytes) retransferred due to lack of cache: %s%s",
                        (LLint) sz, back[p].url_adr, back[p].url_fil);
          /* Sinon requête normale... */
          back[p].http11 = 0;
        }
      }
    }

    /* Stop requested - abort backing */
    if (opt->state.stop) {
      back[p].r.statuscode = STATUSCODE_INVALID;        // fatal
      strcpybuff(back[p].r.msg, "mirror stopped by user");
      back[p].status = STATUS_READY;    // terminé
      back_set_finished(sback, p);
      hts_log_print(opt, LOG_WARNING,
                    "File not added due to mirror cancel: %s%s", adr, fil);
      return 0;
    }

    {
      ///htsblk r;   non directement dans la structure-réponse!
      T_SOC soc;

      // ouvrir liaison, envoyer requète
      // ne pas traiter ou recevoir l'en tête immédiatement
      hts_init_htsblk(&back[p].r);
      //memset(&(back[p].r), 0, sizeof(htsblk)); 
      back[p].r.location = back[p].location_buffer;
      // recopier proxy
      if ((back[p].r.req.proxy.active = opt->proxy.active)) {
        if (StringBuff(opt->proxy.bindhost) != NULL)
          back[p].r.req.proxy.bindhost = StringBuff(opt->proxy.bindhost);
        if (StringBuff(opt->proxy.name) != NULL)
          back[p].r.req.proxy.name = StringBuff(opt->proxy.name);
        back[p].r.req.proxy.port = opt->proxy.port;
      }
      // et user-agent
      back[p].r.req.user_agent = StringBuff(opt->user_agent);
      back[p].r.req.referer = StringBuff(opt->referer);
      back[p].r.req.from = StringBuff(opt->from);
      back[p].r.req.lang_iso = StringBuff(opt->lang_iso);
      back[p].r.req.accept = StringBuff(opt->accept);
      back[p].r.req.headers = StringBuff(opt->headers);
      back[p].r.req.user_agent_send = opt->user_agent_send;
      // et http11
      back[p].r.req.http11 = back[p].http11;
      back[p].r.req.nocompression = opt->nocompression;
      back[p].r.req.nokeepalive = opt->nokeepalive;

      // mode ftp, court-circuit!
      if (strfield(back[p].url_adr, "ftp://")) {
        if (back[p].testmode) {
          hts_log_print(opt, LOG_DEBUG,
                        "error: forbidden test with ftp link for back_add");
          return -1;            // erreur pas de test permis
        }
        if (!(back[p].r.req.proxy.active && opt->ftp_proxy)) {  // connexion directe, gérée en thread
          FTPDownloadStruct *str =
            (FTPDownloadStruct *) malloc(sizeof(FTPDownloadStruct));
          str->pBack = &back[p];
          str->pOpt = opt;
          /* */
          back[p].status = STATUS_FTP_TRANSFER; // connexion ftp
#if USE_BEGINTHREAD
          launch_ftp(str);
#else
#error Must have pthreads
#endif
          return 0;
        }
      }
#if HTS_USEOPENSSL
      else if (strfield(back[p].url_adr, "https://")) {     // let's rock
        back[p].r.ssl = 1;
        // back[p].r.ssl_soc = NULL;
        back[p].r.ssl_con = NULL;
      }
#endif

      if (!back_trylive(opt, cache, sback, p)) {
#if HTS_XGETHOST
#if HDEBUG
        printf("back_solve..\n");
#endif
        back[p].status = STATUS_WAIT_DNS;       // tentative de résolution du nom de host
        soc = INVALID_SOCKET;   // pas encore ouverte
        back_solve(opt, &back[p]);      // préparer
        if (host_wait(opt, &back[p])) { // prêt, par ex fichier ou dispo dans dns
#if HDEBUG
          printf("ok, dns cache ready..\n");
#endif
          soc =
            http_xfopen(opt, 0, 0, 0, back[p].send_too, adr, fil, &back[p].r);
          if (soc == INVALID_SOCKET) {
            back[p].status = STATUS_READY;      // fini, erreur
            back_set_finished(sback, p);
          }
        }
        //
#else
        //
#if CNXDEBUG
        printf("XFopen..\n");
#endif

        if (strnotempty(back[p].send_too))      // envoyer un if-modified-since
#if HTS_XCONN
          soc = http_xfopen(0, 0, 0, back[p].send_too, adr, fil, &(back[p].r));
#else
          soc = http_xfopen(0, 0, 1, back[p].send_too, adr, fil, &(back[p].r));
#endif
        else
#if HTS_XCONN
          soc = http_xfopen(test, 0, 0, NULL, adr, fil, &(back[p].r));
#else
          soc = http_xfopen(test, 0, 1, NULL, adr, fil, &(back[p].r));
#endif
#endif
      } else {
        soc = back[p].r.soc;

        hts_log_print(opt, LOG_DEBUG,
                      "(Keep-Alive): successfully linked #%d (for %s%s)",
                      back[p].r.debugid, back[p].url_adr, back[p].url_fil);
      }

      if (opt->timeout > 0) {   // gestion du opt->timeout
        back[p].timeout = opt->timeout;
        back[p].timeout_refresh = time_local();
      } else {
        back[p].timeout = -1;   // pas de gestion (default)
      }

      if (opt->rateout > 0) {   // gestion d'un taux minimum de transfert toléré
        back[p].rateout = opt->rateout;
        back[p].rateout_time = time_local();
      } else {
        back[p].rateout = -1;   // pas de gestion (default)
      }

      // Note: on charge les code-page erreurs (erreur 404, etc) dans le cas où cela est
      // rattrapable (exemple: 301,302 moved xxx -> refresh sur la page!)
      //if ((back[p].statuscode!=HTTP_OK) || (soc<0)) { // ERREUR HTTP/autre

#if CNXDEBUG
      printf("Xfopen ok, poll..\n");
#endif

#if HTS_XGETHOST
      if (soc != INVALID_SOCKET)
        if (back[p].status == STATUS_WAIT_DNS) {        // pas d'erreur
          if (!back[p].r.is_file)
            back[p].status = STATUS_CONNECTING; // connexion en cours
          else
            back[p].status = 1; // fichier
        }
#else
      if (soc == INVALID_SOCKET) {      // erreur socket
        back[p].status = STATUS_READY;  // FINI
        back_set_finished(sback, p);
        //if (back[p].soc!=INVALID_SOCKET) deletehttp(back[p].soc);
        back[p].r.soc = INVALID_SOCKET;
      } else {
        if (!back[p].r.is_file)
#if HTS_XCONN
          back[p].status = STATUS_CONNECTING;   // connexion en cours
#else
          back[p].status = STATUS_WAIT_HEADERS;  // chargement en tête en cours
#endif
        else
          back[p].status = 1;   // chargement fichier
#if BDEBUG==1
        printf("..loading header\n");
#endif
      }
#endif

    }

    // note: si il y a erreur (404,etc) status=2 (terminé/échec) mais
    // le lien est considéré comme traité
    //if (back[p].soc<0)  // erreur
    //  return -1;

    return 0;
  } else {
    if (opt->log != NULL) {
      hts_log_print(opt, LOG_WARNING,
                    "error: no space left in stack for back_add");
      if ((opt->state.debug_state & 1) == 0) {  /* debug_state<0> == debug 'no space left in stack' */
        int i;

        hts_log_print(opt, LOG_WARNING, "debug: DUMPING %d BLOCKS", back_max);
        opt->state.debug_state |= 1;    /* once */
        /* OUTPUT FULL DEBUG INFORMATION THE FIRST TIME WE SEE THIS VERY ANNOYING BUG,
           HOPING THAT SOME USER REPORT WILL QUICKLY SOLVE THIS PROBLEM :p */
        for(i = 0; i < back_max; i++) {
          if (back[i].status != -1) {
            int may_clean = slot_can_be_cleaned(&back[i]);
            int may_finalize = may_clean
              && slot_can_be_finalized(opt, &back[i]);
            int may_serialize = slot_can_be_cached_on_disk(&back[i]);

            hts_log_print(opt, LOG_DEBUG,
                          "back[%03d]: may_clean=%d, may_finalize_disk=%d, may_serialize=%d:"
                          LF "\t"
                          "finalized(%d), status(%d), locked(%d), delayed(%d), test(%d), "
                          LF "\t"
                          "statuscode(%d), size(%d), is_write(%d), may_hypertext(%d), "
                          LF "\t" "contenttype(%s), url(%s%s), save(%s)", i,
                          may_clean, may_finalize, may_serialize,
                          back[i].finalized, back[i].status, back[i].locked,
                          IS_DELAYED_EXT(back[i].url_sav), back[i].testmode,
                          back[i].r.statuscode, (int) back[i].r.size,
                          back[i].r.is_write, may_be_hypertext_mime(opt,
                                                                    back[i].r.
                                                                    contenttype,
                                                                    back[i].
                                                                    url_fil),
                          /* */
                          back[i].r.contenttype, back[i].url_adr,
                          back[i].url_fil,
                          back[i].url_sav ? back[i].url_sav : "<null>");
          }
        }
      }

    }
    return -1;                  // plus de place
  }
}

#if HTS_XGETHOST
// attendre que le host (ou celui du proxy) ait été résolu
// si c'est un fichier, la résolution est immédiate
// idem pour ftp://
void back_solve(httrackp * opt, lien_back * back) {
  assertf(opt != NULL);
  assertf(back != NULL);
  if ((!strfield(back->url_adr, "file://"))
      && !strfield(back->url_adr, "ftp://")
    ) {
    const char *a;

    if (!(back->r.req.proxy.active))
      a = back->url_adr;
    else
      a = back->r.req.proxy.name;
    assertf(a != NULL);
    a = jump_protocol_const(a);
    if (check_hostname_dns(a)) {
      hts_log_print(opt, LOG_DEBUG, "resolved: %s", a);
    } else {
      hts_log_print(opt, LOG_DEBUG, "failed to resolve: %s", a);
    }
    //if (hts_dnstest(opt, a, 1) == 2) { // non encore testé!..
    //  hts_log_print(opt, LOG_DEBUG, "resolving in background: %s", a);
    //}
  }
}

// détermine si le host a pu être résolu
int host_wait(httrackp * opt, lien_back * back) {
  // Always synchronous. No more background DNS resolution
  // (does not really improve performances)
  return 1;
}
#endif

// élimine les fichiers non html en backing (anticipation)
// cleanup non-html files in backing to save backing space
// and allow faster "save in cache" operation
// also cleanup keep-alive sockets and ensure that not too many sockets are being opened

static int slot_can_be_cleaned(const lien_back * back) {
  return (back->status == STATUS_READY) // ready
    /* Check autoclean */
    && (!back->testmode)        // not test mode
    && (strnotempty(back->url_sav))     // filename exists
    && (HTTP_IS_OK(back->r.statuscode)) // HTTP "OK"
    && (back->r.size >= 0)      // size>=0
    ;
}

static int slot_can_be_finalized(httrackp * opt, const lien_back * back) {
  return back->r.is_write       // not in memory (on disk, ready)
    && !is_hypertext_mime(opt, back->r.contenttype, back->url_fil)      // not HTML/hypertext
    && !may_be_hypertext_mime(opt, back->r.contenttype, back->url_fil)  // may NOT be parseable mime type
    /* Has not been added before the heap saw the link, or now exists on heap */
    && (!back->early_add
        || hash_read(opt->hash, back->url_sav, NULL, HASH_STRUCT_FILENAME) >= 0);
}

void back_clean(httrackp * opt, cache_back * cache, struct_back * sback) {
  lien_back *const back = sback->lnk;
  const int back_max = sback->count;
  int oneMore = ((opt->state._hts_in_html_parsing == 2 && opt->maxsoc >= 2) || (opt->state._hts_in_html_parsing == 1 && opt->maxsoc >= 4)) ? 1 : 0;     // testing links
  int i;

  for(i = 0; i < back_max; i++) {
    if (slot_can_be_cleaned(&back[i])) {
      if (slot_can_be_finalized(opt, &back[i])) {
        (void) back_flush_output(opt, cache, sback, i); // flush output buffers
        usercommand(opt, 0, NULL, back[i].url_sav, back[i].url_adr,
                    back[i].url_fil);
        //if (back[i].links_index >= 0) {
        //  assertf(back[i].links_index < opt->hash->max_lien);
        //  opt->hash->liens[back[i].links_index]->pass2 = -1;
        //  // *back[i].pass2_ptr=-1;  // Done!
        //}
        /* MANDATORY if we don't want back_fill() to endlessly put the same file on download! */
        {
          int index = hash_read(opt->hash, back[i].url_sav, NULL, HASH_STRUCT_FILENAME );       // lecture type 0 (sav)

          if (index >= 0) {
            opt->liens[index]->pass2 = -1;        /* DONE! */
          } else {
            hts_log_print(opt, LOG_INFO,
                          "engine: warning: entry cleaned up, but no trace on heap: %s%s (%s)",
                          back[i].url_adr, back[i].url_fil, back[i].url_sav);
          }
        }
        HTS_STAT.stat_background++;
        hts_log_print(opt, LOG_INFO,
                      "File successfully written in background: %s",
                      back[i].url_sav);
        back_maydelete(opt, cache, sback, i);   // May delete backing entry
      } else {
        if (!back[i].finalized) {
          if (1) {
            /* Ensure deleted or recycled socket */
            /* BUT DO NOT YET WIPE back[i].r.adr */
            hts_log_print(opt, LOG_DEBUG,
                          "file %s%s validated (cached, left in memory)",
                          back[i].url_adr, back[i].url_fil);
            back_maydeletehttp(opt, cache, sback, i);
          } else {
            /*
               NOT YET HANDLED CORRECTLY (READ IN NEW CACHE TO DO)
             */
            /* Lock the entry but do not keep the html data in memory (in cache) */
            if (opt->cache) {
              htsblk r;

              /* Ensure deleted or recycled socket */
              back_maydeletehttp(opt, cache, sback, i);
              assertf(back[i].r.soc == INVALID_SOCKET);

              /* Check header */
              cache_header(opt, cache, back[i].url_adr, back[i].url_fil, &r);
              if (r.statuscode == HTTP_OK) {
                if (back[i].r.soc == INVALID_SOCKET) {
                  /* Delete buffer and sockets */
                  deleteaddr(&back[i].r);
                  deletehttp(&back[i].r);
                  hts_log_print(opt, LOG_DEBUG,
                                "file %s%s temporarily left in cache to spare memory",
                                back[i].url_adr, back[i].url_fil);
                }
              } else {
                hts_log_print(opt, LOG_WARNING,
                              "Unexpected html cache lookup error during back clean");
              }
              // xxc xxc
            }
          }
        }
      }
    } else if (back[i].status == STATUS_ALIVE) {        // waiting (keep-alive)
      if (!back[i].r.keep_alive || back[i].r.soc == INVALID_SOCKET
          || back[i].r.keep_alive_max < 1
          || time_local() >= back[i].ka_time_start + back[i].r.keep_alive_t) {
        const char *reason = "unknown";
        char buffer[128];
        if (!back[i].r.keep_alive) {
          reason = "not keep-alive";
        } else if (back[i].r.soc == INVALID_SOCKET) {
          reason = "closed";
        } else if (back[i].r.keep_alive_max < 1) {
          reason = "keep-alive-max reached";
        } else if (time_local() >= back[i].ka_time_start + back[i].r.keep_alive_t) {
          assertf(back[i].ka_time_start != 0);
          snprintf(buffer, sizeof(buffer), "keep-alive timeout = %ds)",
                   (int) back[i].r.keep_alive_t);
          reason = buffer;
        }
        hts_log_print(opt, LOG_DEBUG,
                      "(Keep-Alive): live socket #%d (%s) closed (%s)",
                      back[i].r.debugid, back[i].url_adr, reason);
        back_delete(opt, cache, sback, i);      // delete backing entry
      }
    }
  }
  /* switch connections to live ones */
  for(i = 0; i < back_max; i++) {
    if (back[i].status == STATUS_READY) {       // ready
      if (back[i].r.soc != INVALID_SOCKET) {
        back_maydeletehttp(opt, cache, sback, i);
      }
    }
  }
  /* delete sockets if too many keep-alive'd sockets in background */
  if (opt->maxsoc > 0) {
    int max = opt->maxsoc + oneMore;
    int curr = back_nsoc_overall(sback);

    if (curr > max) {
      hts_log_print(opt, LOG_DEBUG, "(Keep-Alive): deleting #%d sockets",
                    curr - max);
    }
    for(i = 0; i < back_max && curr > max; i++) {
      if (back[i].status == STATUS_ALIVE) {
        back_delete(opt, cache, sback, i);      // delete backing entry
        curr--;
      }
    }
  }
  /* transfer ready slots to the storage hashtable */
  {
    int nxfr = back_cleanup_background(opt, cache, sback);

    if (nxfr > 0) {
      hts_log_print(opt, LOG_DEBUG,
                    "(htsback): %d slots ready moved to background", nxfr);
    }
  }
}

// attente (gestion des buffers des sockets)
void back_wait(struct_back * sback, httrackp * opt, cache_back * cache,
               TStamp stat_timestart) {
  char catbuff[CATBUFF_SIZE];
  lien_back *const back = sback->lnk;
  const int back_max = sback->count;
  unsigned int i_mod;
  T_SOC nfds = INVALID_SOCKET;
  fd_set fds, fds_c, fds_e;     // fds pour lecture, connect (write), et erreur
  int nsockets;                 // nbre sockets
  LLint max_read_bytes;         // max bytes read per sockets
  struct timeval tv;
  int do_wait = 0;
  int gestion_timeout = 0;
  int busy_recv = 0;            // pas de données pour le moment   
  int busy_state = 0;           // pas de connexions
  int max_loop;                 // nombre de boucles max à parcourir..
  int max_loop_chk = 0;
  unsigned int mod_random =
    (unsigned int) (time_local() + HTS_STAT.HTS_TOTAL_RECV);

  // max. number of loops
  max_loop = 8;

#if 1
  // Cleanup the stack to save space!
  back_clean(opt, cache, sback);
#endif

  // recevoir tant qu'il y a des données (avec un maximum de max_loop boucles)
  do_wait = 0;
  gestion_timeout = 0;
  do {
    int max_c;

    busy_state = busy_recv = 0;

#if 0
    check_rate(stat_timestart, opt->maxrate);   // vérifier taux de transfert
#endif
    // inscrire les sockets actuelles, et rechercher l'ID la plus élevée
    FD_ZERO(&fds);
    FD_ZERO(&fds_c);
    FD_ZERO(&fds_e);
    nsockets = 0;
    max_read_bytes = TAILLE_BUFFER;     // maximum bytes that can be read
    nfds = INVALID_SOCKET;

    max_c = 1;
    for(i_mod = 0; i_mod < (unsigned int) back_max; i_mod++) {
      // for(i=0;i<back_max;i++) {
      unsigned int i = (i_mod + mod_random) % (back_max);

      // en cas de gestion du connect préemptif
#if HTS_XCONN
      if (back[i].status == STATUS_CONNECTING) {        // connexion
        do_wait = 1;

        // noter socket write
        FD_SET(back[i].r.soc, &fds_c);

        // noter socket erreur
        FD_SET(back[i].r.soc, &fds_e);

        // calculer max
        if (max_c) {
          max_c = 0;
          nfds = back[i].r.soc;
        } else if (back[i].r.soc > nfds) {
          // ID socket la plus élevée
          nfds = back[i].r.soc;
        }

      } else
#endif
#if HTS_XGETHOST
      if (back[i].status == STATUS_WAIT_DNS) {  // attente
        // rien à faire..
      } else
#endif
        // poll pour la lecture sur les sockets
      if ((back[i].status > 0) && (back[i].status < 100)) {     // en réception http

#if BDEBUG==1
        //printf("....socket in progress: %d\n",back[i].r.soc);
#endif
        // non local et non ftp
        if (!back[i].r.is_file) {
          //## if (back[i].url_adr[0]!=lOCAL_CHAR) {

          // vérification de sécurité
          if (back[i].r.soc != INVALID_SOCKET) {        // hey, you never know..
            if (
                // Do not endlessly wait when receiving SSL http data (Patrick Pfeifer)
#if HTS_USEOPENSSL
                !back[i].r.ssl && 
#endif
                back[i].status > 0 && back[i].status < 1000) {
              do_wait = 1;

              // noter socket read
              FD_SET(back[i].r.soc, &fds);

              // noter socket error
              FD_SET(back[i].r.soc, &fds_e);

              // incrémenter nombre de sockets
              nsockets++;

              // calculer max
              if (max_c) {
                max_c = 0;
                nfds = back[i].r.soc;
              } else if (back[i].r.soc > nfds) {
                // ID socket la plus élevée
                nfds = back[i].r.soc;
              }
            }
          } else {
            back[i].r.statuscode = STATUSCODE_CONNERROR;
            if (back[i].status == STATUS_CONNECTING)
              strcpybuff(back[i].r.msg, "Connect Error");
            else
              strcpybuff(back[i].r.msg, "Receive Error");
            back[i].status = STATUS_READY;      // terminé
            back_set_finished(sback, i);
            hts_log_print(opt, LOG_WARNING,
                          "Unexpected socket error during pre-loop");
          }
        }

      }
    }
    nfds++;

    if (do_wait) {              // attendre
      // temps d'attente max: 2.5 seconde
      tv.tv_sec = HTS_SOCK_SEC;
      tv.tv_usec = HTS_SOCK_MS;

#if BDEBUG==1
      printf("..select\n");
#endif

      // poller les sockets-attention au noyau sous Unix..
#if HTS_WIDE_DEBUG
      DEBUG_W("select\n");
#endif
      select((int) nfds, &fds, &fds_c, &fds_e, &tv);
#if HTS_WIDE_DEBUG
      DEBUG_W("select done\n");
#endif
    }
    // maximum data which can be received for a socket, if limited
    if (nsockets) {
      if (opt->maxrate > 0) {
        max_read_bytes = (check_downloadable_bytes(opt->maxrate) / nsockets);
        if (max_read_bytes > TAILLE_BUFFER) {
          /* limit size */
          max_read_bytes = TAILLE_BUFFER;
        } else if (max_read_bytes < TAILLE_BUFFER) {
          /* a small pause */
          Sleep(10);
        }
      }
    }
    if (!max_read_bytes)
      busy_recv = 0;

    // recevoir les données arrivées
    for(i_mod = 0; i_mod < (unsigned int) back_max; i_mod++) {
      // for(i=0;i<back_max;i++) {
      unsigned int i = (i_mod + mod_random) % (back_max);

      if (back[i].status > 0) {
        if (!back[i].r.is_file) {       // not file..
          if (back[i].r.soc != INVALID_SOCKET) {        // hey, you never know..
            int err = FD_ISSET(back[i].r.soc, &fds_e);

            if (err) {
              if (back[i].r.soc != INVALID_SOCKET) {
#if HTS_DEBUG_CLOSESOCK
                DEBUG_W("back_wait: deletehttp\n");
#endif
                deletehttp(&back[i].r);
              }
              back[i].r.soc = INVALID_SOCKET;
              back[i].r.statuscode = STATUSCODE_CONNERROR;
              if (back[i].status == STATUS_CONNECTING)
                strcpybuff(back[i].r.msg, "Connect Error");
              else
                strcpybuff(back[i].r.msg, "Receive Error");
              if (back[i].status == STATUS_ALIVE) {     /* Keep-alive socket */
                back_delete(opt, cache, sback, i);
              } else {
                back[i].status = STATUS_READY;  // terminé
                back_set_finished(sback, i);
              }
            }
          }
        }
      }
      // ---- FLAG WRITE MIS A UN?: POUR LE CONNECT
      if (back[i].status == STATUS_CONNECTING) {        // attendre connect
        int dispo = 0;

        // vérifier l'existance de timeout-check
        if (!gestion_timeout)
          if (back[i].timeout > 0)
            gestion_timeout = 1;

        // connecté?
        dispo = FD_ISSET(back[i].r.soc, &fds_c);
        if (dispo) {            // ok connected!!
          busy_state = 1;

#if HTS_USEOPENSSL
          /* SSL mode */
          if (back[i].r.ssl) {
            // handshake not yet launched
            if (!back[i].r.ssl_con) {
              SSL_CTX_set_options(openssl_ctx, SSL_OP_ALL);
              // new session
              back[i].r.ssl_con = SSL_new(openssl_ctx);
              if (back[i].r.ssl_con) {
                SSL_clear(back[i].r.ssl_con);
                if (SSL_set_fd(back[i].r.ssl_con, (int) back[i].r.soc) == 1) {
                  SSL_set_connect_state(back[i].r.ssl_con);
                  back[i].status = STATUS_SSL_WAIT_HANDSHAKE;   /* handshake wait */
                } else
                  back[i].r.statuscode = STATUSCODE_SSL_HANDSHAKE;
              } else
                back[i].r.statuscode = STATUSCODE_SSL_HANDSHAKE;
            }
            /* Error */
            if (back[i].r.statuscode == STATUSCODE_SSL_HANDSHAKE) {
              strcpybuff(back[i].r.msg, "bad SSL/TLS handshake");
              deletehttp(&back[i].r);
              back[i].r.soc = INVALID_SOCKET;
              back[i].r.statuscode = STATUSCODE_NON_FATAL;
              back[i].status = STATUS_READY;
              back_set_finished(sback, i);
            }
          }
#endif

#if BDEBUG==1
          printf("..connect ok on socket %d\n", back[i].r.soc);
#endif

          if ((back[i].r.soc != INVALID_SOCKET)
              && (back[i].status == STATUS_CONNECTING)) {
            /* limit nb. connections/seconds to avoid server overload */
            /*if (opt->maxconn>0) {
               Sleep(1000/opt->maxconn);
               } */

            back[i].ka_time_start = time_local();
            if (back[i].timeout > 0) {  // refresh timeout si besoin est
              back[i].timeout_refresh = back[i].ka_time_start;
            }
            if (back[i].rateout > 0) {  // le taux de transfert de base sur le début de la connexion
              back[i].rateout_time = back[i].ka_time_start;
            }
            // envoyer header
            //if (strcmp(back[i].url_sav,BACK_ADD_TEST)!=0)    // vrai get
            HTS_STAT.stat_nrequests++;
            if (!back[i].head_request)
              http_sendhead(opt, opt->cookie, 0, back[i].send_too,
                            back[i].url_adr, back[i].url_fil,
                            back[i].referer_adr, back[i].referer_fil,
                            &back[i].r);
            else if (back[i].head_request == 2) // test en GET!
              http_sendhead(opt, opt->cookie, 0, back[i].send_too,
                            back[i].url_adr, back[i].url_fil,
                            back[i].referer_adr, back[i].referer_fil,
                            &back[i].r);
            else                // test!
              http_sendhead(opt, opt->cookie, 1, back[i].send_too,
                            back[i].url_adr, back[i].url_fil,
                            back[i].referer_adr, back[i].referer_fil,
                            &back[i].r);
            back[i].status = STATUS_WAIT_HEADERS;        // attendre en tête maintenant
          }
        }
        // attente gethostbyname
      }
#if HTS_USEOPENSSL
      else if (back[i].status == STATUS_SSL_WAIT_HANDSHAKE) {       // wait for SSL handshake
        /* SSL mode */
        if (back[i].r.ssl) {
          int conn_code;

          if ((conn_code = SSL_connect(back[i].r.ssl_con)) <= 0) {
            /* non blocking I/O, will retry */
            int err_code = SSL_get_error(back[i].r.ssl_con, conn_code);

            if ((err_code != SSL_ERROR_WANT_READ)
                && (err_code != SSL_ERROR_WANT_WRITE)
              ) {
              char tmp[256];

              tmp[0] = '\0';
              ERR_error_string(err_code, tmp);
              back[i].r.msg[0] = '\0';
              strncatbuff(back[i].r.msg, tmp, sizeof(back[i].r.msg) - 2);
              if (!strnotempty(back[i].r.msg)) {
                sprintf(back[i].r.msg, "SSL/TLS error %d", err_code);
              }
              deletehttp(&back[i].r);
              back[i].r.soc = INVALID_SOCKET;
              back[i].r.statuscode = STATUSCODE_NON_FATAL;
              back[i].status = STATUS_READY;
              back_set_finished(sback, i);
            }
          } else {              /* got it! */
            back[i].status = STATUS_CONNECTING; // back to waitconnect
          }
        } else {
          strcpybuff(back[i].r.msg, "unexpected SSL/TLS error");
          deletehttp(&back[i].r);
          back[i].r.soc = INVALID_SOCKET;
          back[i].r.statuscode = STATUSCODE_NON_FATAL;
          back[i].status = STATUS_READY;
          back_set_finished(sback, i);
        }

      }
#endif
#if HTS_XGETHOST
      else if (back[i].status == STATUS_WAIT_DNS) {     // attendre gethostbyname
#if DEBUGDNS
        //printf("status 101 for %s\n",back[i].url_adr);
#endif

        if (!gestion_timeout)
          if (back[i].timeout > 0)
            gestion_timeout = 1;

        if (host_wait(opt, &back[i])) { // prêt
          back[i].status = STATUS_CONNECTING;   // attente connexion
          if (back[i].timeout > 0) {    // refresh timeout si besoin est
            back[i].timeout_refresh = time_local();
          }
          if (back[i].rateout > 0) {    // le taux de transfert de base sur le début de la connexion
            back[i].rateout_time = time_local();
          }

          back[i].r.soc =
            http_xfopen(opt, 0, 0, 0, back[i].send_too, back[i].url_adr,
                        back[i].url_fil, &(back[i].r));
          if (back[i].r.soc == INVALID_SOCKET) {
            back[i].status = STATUS_READY;      // fini, erreur
            back_set_finished(sback, i);
            if (back[i].r.soc != INVALID_SOCKET) {
#if HTS_DEBUG_CLOSESOCK
              DEBUG_W("back_wait(2): deletehttp\n");
#endif
              deletehttp(&back[i].r);
            }
            back[i].r.soc = INVALID_SOCKET;
            back[i].r.statuscode = STATUSCODE_NON_FATAL;
            if (strnotempty(back[i].r.msg) == 0)
              strcpybuff(back[i].r.msg, "Unable to resolve host name");
          }
        }

        // ---- FLAG READ MIS A UN?: POUR LA RECEPTION
      }
#endif
#if USE_BEGINTHREAD
      // ..rien à faire, c'est magic les threads
#else
      else if (back[i].status == STATUS_FTP_TRANSFER) { // en réception ftp
        if (!fexist(back[i].location_buffer)) { // terminé
          FILE *fp;

          fp =
            FOPEN(fconcat(OPT_GET_BUFF(opt), back[i].location_buffer, ".ok"),
                  "rb");
          if (fp) {
            int j = 0;

            fscanf(fp, "%d ", &(back[i].r.statuscode));
            while(!feof(fp)) {
              int c = fgetc(fp);

              if (c != EOF)
                back[i].r.msg[j++] = c;
            }
            back[i].r.msg[j++] = '\0';
            fclose(fp);
            UNLINK(fconcat(OPT_GET_BUFF(opt), back[i].location_buffer, ".ok"));
            strcpybuff(fconcat
                       (OPT_GET_BUFF(opt), back[i].location_buffer, ".ok"), "");
          } else {
            strcpybuff(back[i].r.msg,
                       "Unknown ftp result, check if file is ok");
            back[i].r.statuscode = STATUSCODE_INVALID;
          }
          back[i].status = STATUS_READY;
          back_set_finished(sback, i);
          // finalize transfer
          if (back[i].r.statuscode > 0) {
            hts_log_print(opt, LOG_TRACE, "finalizing ftp");
            back_finalize(opt, cache, sback, i);
          }
        }
      }
#endif
      else if (back[i].status == STATUS_FTP_READY) {    // ftp ready
        back[i].status = STATUS_READY;
        back_set_finished(sback, i);
        // finalize transfer
        if (back[i].r.statuscode > 0) {
          hts_log_print(opt, LOG_TRACE, "finalizing ftp");
          back_finalize(opt, cache, sback, i);
        }
      } else if ((back[i].status > 0) && (back[i].status < 1000)) {     // en réception http
        int dispo = 0;

        // vérifier l'existance de timeout-check
        if (!gestion_timeout)
          if (back[i].timeout > 0)
            gestion_timeout = 1;

        // données dispo?
        //## if (back[i].url_adr[0]!=lOCAL_CHAR)
        if (back[i].r.is_file)
          dispo = 1;
#if HTS_USEOPENSSL
        else if (back[i].r.ssl)
          dispo = 1;
#endif
        else
          dispo = FD_ISSET(back[i].r.soc, &fds);

        // Check transfer rate!
        if (!max_read_bytes)
          dispo = 0;            // limit transfer rate

        if (dispo) {            // données dispo
          LLint retour_fread;

          busy_recv = 1;        // on récupère encore
#if BDEBUG==1
          printf("..data available on socket %d\n", back[i].r.soc);
#endif

          // range size hack old location

#if HTS_DIRECTDISK
          // Court-circuit:
          // Peut-on stocker le fichier directement sur disque?
          // Ahh que ca serait vachement mieux et que ahh que la mémoire vous dit merci!
          if (back[i].status) {
            if (back[i].r.is_write == 0) {      // mode mémoire
              if (back[i].r.adr == NULL) {      // rien n'a été écrit
                if (!back[i].testmode) {        // pas mode test
                  if (strnotempty(back[i].url_sav)) {
                    if (strcmp(back[i].url_fil, "/robots.txt")) {
                      if (back[i].r.statuscode == HTTP_OK) {    // 'OK'
                        if (!is_hypertext_mime(opt, back[i].r.contenttype, back[i].url_fil)) {  // pas HTML
                          if (opt->getmode & 2) {       // on peut ecrire des non html
                            int fcheck = 0;
                            int last_errno = 0;

                            back[i].r.is_write = 1;     // écrire
                            if (back[i].r.compressed &&
                                /* .gz are *NOT* depacked!! */
                                strfield(get_ext(catbuff, sizeof(catbuff), back[i].url_sav), "gz") == 0
                                && strfield(get_ext(catbuff, sizeof(catbuff), back[i].url_sav), "tgz") == 0
                              ) {
                              if (create_back_tmpfile(opt, &back[i]) == 0) {
                                assertf(back[i].tmpfile != NULL);
                                /* note: tmpfile is utf-8 */
                                if ((back[i].r.out =
                                     FOPEN(back[i].tmpfile, "wb")) == NULL) {
                                  last_errno = errno;
                                }
                              }
                            } else {
                              file_notify(opt, back[i].url_adr, back[i].url_fil,
                                          back[i].url_sav, 1, 1,
                                          back[i].r.notmodified);
                              back[i].r.compressed = 0;
                              if ((back[i].r.out =
                                   filecreate(&opt->state.strc,
                                              back[i].url_sav)) == NULL) {
                                last_errno = errno;
                              }
                            }
                            if (back[i].r.out == NULL) {
                              errno = last_errno;
                              if ((fcheck = check_fatal_io_errno())) {
                                hts_log_print(opt, LOG_ERROR,
                                              "Mirror aborted: disk full or filesystem problems");
                                opt->state.exit_xh = -1;        /* fatal error */
                              }
                            }
#if HDEBUG
                            printf("direct-disk: %s\n", back[i].url_sav);
#endif
                            hts_log_print(opt, LOG_DEBUG,
                                          "File received from net to disk: %s%s",
                                          back[i].url_adr, back[i].url_fil);

                            if (back[i].r.out == NULL) {
                              hts_log_print(opt, LOG_ERROR | LOG_ERRNO,
                                            "Unable to save file %s",
                                            back[i].url_sav);
                              if (fcheck) {
                                hts_log_print(opt, LOG_ERROR,
                                              "* * Fatal write error, giving up");
                              }
                              back[i].r.is_write = 0;   // erreur, abandonner
                              back[i].status = STATUS_READY;      // terminé
                              back_set_finished(sback, i);
                              if (back[i].r.soc != INVALID_SOCKET) {
                                deletehttp(&back[i].r);
                                back[i].r.soc = INVALID_SOCKET;
                              }
                            } else {
#ifndef _WIN32
                              chmod(back[i].url_sav, HTS_ACCESS_FILE);
#endif
                              /* create a temporary reference file in case of broken mirror */
                              if (back[i].r.out != NULL && opt->cache != 0) {
                                if (back_serialize_ref(opt, &back[i]) != 0) {
                                  hts_log_print(opt, LOG_WARNING,
                                                "Could not create temporary reference file for %s%s",
                                                back[i].url_adr,
                                                back[i].url_fil);
                                }
                              }
                            }
                          } else {      // on coupe tout!
                            hts_log_print(opt, LOG_DEBUG,
                                          "File cancelled (non HTML): %s%s",
                                          back[i].url_adr, back[i].url_fil);
                            back[i].status = STATUS_READY;      // terminé
                            back_set_finished(sback, i);
                            if (!back[i].testmode)
                              back[i].r.statuscode = STATUSCODE_INVALID;        // EUHH CANCEL
                            else
                              back[i].r.statuscode = STATUSCODE_TEST_OK;        // "TEST OK"
                            if (back[i].r.soc != INVALID_SOCKET) {
#if HTS_DEBUG_CLOSESOCK
                              DEBUG_W("back_wait(3): deletehttp\n");
#endif
                              deletehttp(&back[i].r);
                            }
                            back[i].r.soc = INVALID_SOCKET;
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
#endif

          // réception de données depuis socket ou fichier
          if (back[i].status) {
            if (back[i].status == STATUS_WAIT_HEADERS)  // recevoir par bloc de lignes
              retour_fread = http_xfread1(&(back[i].r), 0);
            else if (back[i].status == STATUS_CHUNK_WAIT || back[i].status == STATUS_CHUNK_CR) {        // recevoir longueur chunk en hexa caractère par caractère
              // backuper pour lire dans le buffer chunk
              htsblk r;

              memcpy(&r, &(back[i].r), sizeof(htsblk));
              back[i].r.is_write = 0;   // mémoire
              back[i].r.adr = back[i].chunk_adr;        // adresse
              back[i].r.size = back[i].chunk_size;      // taille taille chunk
              back[i].r.totalsize = -1; // total inconnu
              back[i].r.out = NULL;
              back[i].r.is_file = 0;
              //
              // ligne par ligne
              retour_fread = http_xfread1(&(back[i].r), -1);
              // modifier et restaurer
              back[i].chunk_adr = back[i].r.adr;        // adresse
              back[i].chunk_size = back[i].r.size;      // taille taille chunk
              memcpy(&(back[i].r), &r, sizeof(htsblk)); // restaurer véritable r
            } else if (back[i].is_chunk) {      // attention chunk, limiter taille à lire
#if CHUNKDEBUG==1
              printf("[%d] read %d bytes\n", (int) back[i].r.soc,
                     (int) min(back[i].r.totalsize - back[i].r.size,
                               max_read_bytes));
#endif
              retour_fread =
                (int) http_xfread1(&(back[i].r),
                                   (int) min(back[i].r.totalsize -
                                             back[i].r.size, max_read_bytes));
            } else
              retour_fread =
                (int) http_xfread1(&(back[i].r), (int) max_read_bytes);
          } else
            retour_fread = READ_EOF;    // interruption ou annulation interne (peut ne pas être une erreur)

          // Si réception chunk, tester si on est pas à la fin!
          if (back[i].status == 1) {
            if (back[i].is_chunk) {     // attendre prochain chunk
              if (back[i].r.size == back[i].r.totalsize) {      // fin chunk!
                //printf("chunk end at %d\n",back[i].r.size);
                back[i].status = STATUS_CHUNK_CR;       /* fetch ending CRLF */
                if (back[i].chunk_adr != NULL) {
                  freet(back[i].chunk_adr);
                  back[i].chunk_adr = NULL;
                }
                back[i].chunk_size = 0;
                retour_fread = 0;       // pas d'erreur
#if CHUNKDEBUG==1
                printf("[%d] waiting for current chunk CRLF..\n",
                       (int) back[i].r.soc);
#endif
              }
            } else if (back[i].r.keep_alive) {
              if (back[i].r.size == back[i].r.totalsize) {      // fin!
                retour_fread = READ_EOF;        // end
              }
            }
          }

          if (retour_fread < 0) {       // fin réception
            back[i].status = STATUS_READY;      // terminé
            back_set_finished(sback, i);
            /*KA back[i].r.soc=INVALID_SOCKET; */
#if CHUNKDEBUG==1
            if (back[i].is_chunk)
              printf
                ("[%d] must be the last chunk for %s (connection closed) - %d/%d\n",
                 (int) back[i].r.soc, back[i].url_fil, back[i].r.size,
                 back[i].r.totalsize);
#endif
            if (retour_fread < 0 && retour_fread != READ_EOF) {
              if (back[i].r.size > 0)
                strcpybuff(back[i].r.msg, "Interrupted transfer");
              else
                strcpybuff(back[i].r.msg, "No data (connection closed)");
              back[i].r.statuscode = STATUSCODE_CONNERROR;
            } else if ((back[i].r.statuscode <= 0)
                       && (strnotempty(back[i].r.msg) == 0)) {
#if HDEBUG
              printf("error interruped: %s\n", back[i].r.adr);
#endif
              if (back[i].r.size > 0)
                strcpybuff(back[i].r.msg, "Interrupted transfer");
              else
                strcpybuff(back[i].r.msg, "No data (connection closed)");
              back[i].r.statuscode = STATUSCODE_CONNERROR;
            }
            // Close socket
            if (back[i].r.soc != INVALID_SOCKET) {
#if HTS_DEBUG_CLOSESOCK
              DEBUG_W("back_wait(4): deletehttp\n");
#endif
              /*KA deletehttp(&back[i].r); */
              back_maydeletehttp(opt, cache, sback, i);
            }
            // finalize transfer
            if (back[i].r.statuscode > 0 && !IS_DELAYED_EXT(back[i].url_sav)
              ) {
              hts_log_print(opt, LOG_TRACE, "finalizing regular file");
              back_finalize(opt, cache, sback, i);
            }

            if (back[i].r.totalsize >= 0) {     // tester totalsize
              //if ((back[i].r.totalsize>=0) && (back[i].status==STATUS_WAIT_HEADERS)) {    // tester totalsize
              if (back[i].r.totalsize != back[i].r.size) {      // pas la même!
                if (!opt->tolerant) {
                  //#if HTS_CL_IS_FATAL
                  deleteaddr(&back[i].r);
                  if (back[i].r.size < back[i].r.totalsize)
                    back[i].r.statuscode = STATUSCODE_CONNERROR;        // recatch
                  sprintf(back[i].r.msg,
                          "Incorrect length (" LLintP " Bytes, " LLintP
                          " expected)", (LLint) back[i].r.size,
                          (LLint) back[i].r.totalsize);
                } else {
                  //#else
                  // Un warning suffira..
                  hts_log_print(opt, LOG_WARNING,
                                "Incorrect length (" LLintP "!=" LLintP
                                " expected) for %s%s", (LLint) back[i].r.size,
                                (LLint) back[i].r.totalsize, back[i].url_adr,
                                back[i].url_fil);
                  //#endif
                }
              }
            }
#if BDEBUG==1
            printf("transfer ok\n");
#endif
          } else if (retour_fread > 0) {        // pas d'erreur de réception et data
            if (back[i].timeout > 0) {  // refresh timeout si besoin est
              back[i].timeout_refresh = time_local();
            }
            // Traitement des en têtes chunks ou en têtes
            if (back[i].status == STATUS_CHUNK_WAIT || back[i].status == STATUS_CHUNK_CR) {     // réception taille chunk en hexa (  après les en têtes, peut ne pas
              if (back[i].chunk_size > 0
                  && back[i].chunk_adr[back[i].chunk_size - 1] == 10) {
                int chunk_size = -1;
                char chunk_data[64];

                if (back[i].chunk_size < 32) {  // pas trop gros
                  char *chstrip = back[i].chunk_adr;

                  back[i].chunk_adr[back[i].chunk_size - 1] = '\0';     // octet nul 
                  // skip leading spaces or cr
                  while(isspace(*chstrip))
                    chstrip++;
                  chunk_data[0] = '\0';
                  strncatbuff(chunk_data, chstrip, sizeof(chunk_data) - 2);
                  // strip chunk-extension
                  while((chstrip = strchr(chunk_data, ';')))
                    *chstrip = '\0';
                  while((chstrip = strchr(chunk_data, ' ')))
                    *chstrip = '\0';
                  while((chstrip = strchr(chunk_data, '\r')))
                    *chstrip = '\0';
#if CHUNKDEBUG==1
                  printf("[%d] chunk received and read: %s\n",
                         (int) back[i].r.soc, chunk_data);
#endif
                  if (back[i].r.totalsize < 0)
                    back[i].r.totalsize = 0;    // initialiser à 0 (-1 == unknown)
                  if (back[i].status == STATUS_CHUNK_WAIT) {    // "real" chunk
                    if (sscanf(chunk_data, "%x", &chunk_size) == 1) {
                      if (chunk_size > 0)
                        back[i].chunk_blocksize = chunk_size;   /* the data block chunk size */
                      else
                        back[i].chunk_blocksize = -1;   /* ending */
                      back[i].r.totalsize += chunk_size;        // noter taille
                      if (back[i].r.adr != NULL || !back[i].r.is_write) {       // Not to disk
                        back[i].r.adr =
                          (char *) realloct(back[i].r.adr,
                                            (size_t) back[i].r.totalsize + 1);
                        if (!back[i].r.adr) {
                          if (cache->log != NULL) {
                            hts_log_print(opt, LOG_ERROR,
                                          "not enough memory (" LLintP
                                          ") for %s%s",
                                          (LLint) back[i].r.totalsize,
                                          back[i].url_adr, back[i].url_fil);
                          }
                        }
                      }
#if CHUNKDEBUG==1
                      printf("[%d] chunk length: %d - next total " LLintP ":\n",
                             (int) back[i].r.soc, (int) chunk_size,
                             (LLint) back[i].r.totalsize);
#endif
                    } else {
                      hts_log_print(opt, LOG_WARNING,
                                    "Illegal chunk (%s) for %s%s",
                                    back[i].chunk_adr, back[i].url_adr,
                                    back[i].url_fil);
                    }
                  } else {      /* back[i].status==STATUS_CHUNK_CR : just receiving ending CRLF after data */
                    if (chunk_data[0] == '\0') {
                      if (back[i].chunk_blocksize > 0)
                        chunk_size = (int) back[i].chunk_blocksize;     /* recent data chunk size */
                      else if (back[i].chunk_blocksize == -1)
                        chunk_size = 0; /* ending chunk */
                      else
                        chunk_size = 1; /* fake positive size for 1st chunk history */
#if CHUNKDEBUG==1
                      printf("[%d] chunk CRLF seen\n", (int) back[i].r.soc);
#endif
                    } else {
                      hts_log_print(opt, LOG_WARNING,
                                    "illegal chunk CRLF (%s) for %s%s",
                                    back[i].chunk_adr, back[i].url_adr,
                                    back[i].url_fil);
#if CHUNKDEBUG==1
                      printf("[%d] chunk CRLF ERROR!! : '%s'\n",
                             (int) back[i].r.soc, chunk_data);
#endif
                    }
                  }
                } else {
                  hts_log_print(opt, LOG_WARNING,
                                "chunk too big (" LLintP ") for %s%s",
                                (LLint) back[i].chunk_size, back[i].url_adr,
                                back[i].url_fil);
                }

                // ok, continuer sur le body

                // si chunk non nul continuer (ou commencer)
                if (back[i].status == STATUS_CHUNK_CR && chunk_size > 0) {
                  back[i].status = STATUS_CHUNK_WAIT;   /* waiting for next chunk (NN\r\n<data>\r\nNN\r\n<data>..\r\n0\r\n\r\n) */
#if CHUNKDEBUG==1
                  printf("[%d] waiting for next chunk\n", (int) back[i].r.soc);
#endif
                } else if (back[i].status == STATUS_CHUNK_WAIT && chunk_size == 0) {    /* final chunk */
                  back[i].status = STATUS_CHUNK_CR;     /* final CRLF */
#if CHUNKDEBUG==1
                  printf("[%d] waiting for final CRLF (chunk)\n",
                         (int) back[i].r.soc);
#endif
                } else if (back[i].status == STATUS_CHUNK_WAIT && chunk_size >= 0) {    /* will fetch data now */
                  back[i].status = 1;   // continuer body    
#if CHUNKDEBUG==1
                  printf("[%d] waiting for body (chunk)\n",
                         (int) back[i].r.soc);
#endif
                } else {        /* zero-size-chunk-CRLF (end) or error */
#if CHUNKDEBUG==1
                  printf("[%d] chunk end, total: %d\n", (int) back[i].r.soc,
                         back[i].r.size);
#endif
                  /* End */
                  //if (back[i].status==STATUS_CHUNK_CR) {
                  back[i].status = STATUS_READY;        // fin  
                  back_set_finished(sback, i);
                  //}

                  // finalize transfer if not temporary
                  if (!IS_DELAYED_EXT(back[i].url_sav)) {
                    hts_log_print(opt, LOG_TRACE, "finalizing at chunk end");
                    back_finalize(opt, cache, sback, i);
                  } else {
                    if (back[i].r.statuscode == HTTP_OK) {
                      hts_log_print(opt, LOG_WARNING,
                                    "unexpected incomplete type with 200 code at %s%s",
                                    back[i].url_adr, back[i].url_fil);
                    }
                  }
                  if (back[i].r.soc != INVALID_SOCKET) {
#if HTS_DEBUG_CLOSESOCK
                    DEBUG_W("back_wait(5): deletehttp\n");
#endif
                    /* Error */
                    if (chunk_size < 0) {
                      deletehttp(&back[i].r);
                      back[i].r.soc = INVALID_SOCKET;
                      deleteaddr(&back[i].r);
                      back[i].r.statuscode = STATUSCODE_INVALID;
                      strcpybuff(back[i].r.msg, "Invalid chunk");
#if CHUNKDEBUG==1
                      printf("[%d] chunk error\n", (int) back[i].r.soc);
#endif
                    } else {    /* if chunk_size == 0 */

#if CHUNKDEBUG==1
                      printf("[%d] all chunks now received\n",
                             (int) back[i].r.soc);
#endif

                      /* Tester totalsize en fin de chunk */
                      if ((back[i].r.totalsize >= 0)) { // tester totalsize
                        if (back[i].r.totalsize != back[i].r.size) {    // pas la même!
                          if (!opt->tolerant) {
                            deleteaddr(&back[i].r);
                            back[i].r.statuscode = STATUSCODE_INVALID;
                            strcpybuff(back[i].r.msg, "Incorrect length");
                          } else {
                            // Un warning suffira..
                            hts_log_print(opt, LOG_WARNING,
                                          "Incorrect length (" LLintP "!="
                                          LLintP " expected) for %s%s",
                                          (LLint) back[i].r.size,
                                          (LLint) back[i].r.totalsize,
                                          back[i].url_adr, back[i].url_fil);
                          }
                        }
                      }

                      /* Oops, trailers! */
                      if (back[i].r.keep_alive_trailers) {
                        /* fixme (not yet supported) */
                      }

                    }

                  }
                }

                // effacer buffer (chunk en tete)
                if (back[i].chunk_adr != NULL) {
                  freet(back[i].chunk_adr);
                  back[i].chunk_adr = NULL;
                  back[i].chunk_size = 0;
                  // NO! xxback[i].chunk_blocksize = 0;
                }

              }                 // taille buffer chunk > 1 && LF
              //
            } else if (back[i].status == STATUS_WAIT_HEADERS) { // en têtes (avant le chunk si il est présent)
              //
              if (back[i].r.size >= 2) {
                // double LF
                if (((back[i].r.adr[back[i].r.size - 1] == 10)
                     && (back[i].r.adr[back[i].r.size - 2] == 10))
                    || (back[i].r.adr[0] == '<')        /* bogus server */
                  ) {
                  char rcvd[2048];
                  int ptr = 0;
                  int noFreebuff = 0;

#if BDEBUG==1
                  printf("..ok, header received\n");
#endif

                  /* Hack for zero-length headers */
                  if (back[i].status != 0 && back[i].r.adr[0] != '<') {

                    // ----------------------------------------
                    // traiter en-tête!
                    // status-line à récupérer
                    ptr += binput(back[i].r.adr + ptr, rcvd, 2000);
                    if (strnotempty(rcvd) == 0) {
                      /* Bogus CRLF, OR recycled connection and trailing chunk CRLF */
                      ptr += binput(back[i].r.adr + ptr, rcvd, 2000);
                    }
                    // traiter status-line
                    treatfirstline(&back[i].r, rcvd);

#if HDEBUG
                    printf("(Buffer) Status-Code=%d\n", back[i].r.statuscode);
#endif
                    if (_DEBUG_HEAD) {
                      if (ioinfo) {
                        fprintf(ioinfo,
                                "[%d] response for %s%s:\r\ncode=%d\r\n",
                                back[i].r.debugid,
                                jump_identification_const(back[i].url_adr),
                                back[i].url_fil, back[i].r.statuscode);
                        fprintfio(ioinfo, back[i].r.adr, ">>> ");
                        fprintf(ioinfo, "\r\n");
                        fflush(ioinfo);
                      }         // en-tête
                    }
                    // header // ** !attention! HTTP/0.9 non supporté
                    do {
                      ptr += binput(back[i].r.adr + ptr, rcvd, 2000);
#if HDEBUG
                      printf("(buffer)>%s\n", rcvd);
#endif
                      /*
                         if (_DEBUG_HEAD) {
                         if (ioinfo) {
                         fprintf(ioinfo,"(buffer)>%s\r\n",rcvd);      
                         fflush(ioinfo);
                         }
                         }
                       */

                      if (strnotempty(rcvd))
                        treathead(opt->cookie, back[i].url_adr, back[i].url_fil, &back[i].r, rcvd);     // traiter

                      // parfois les serveurs buggés renvoient un content-range avec un 200
                      if (back[i].r.statuscode == HTTP_OK)      // 'OK'
                        if (strfield(rcvd, "content-range:")) { // Avec un content-range: relisez les RFC..
                          // Fake range (the file is complete)
                          if (!
                              (back[i].r.crange_start == 0
                               && back[i].r.crange_end ==
                               back[i].r.crange - 1)) {
                            back[i].r.statuscode = HTTP_PARTIAL_CONTENT;        // FORCER A 206 !!!!!
                          }
                        }

                    } while(strnotempty(rcvd));
                    // ----------------------------------------                    

                  } else {
                    // assume text/html, OK
                    treatfirstline(&back[i].r, back[i].r.adr);
                    noFreebuff = 1;
                  }

                  // Callback
                  {
                    int test_head = RUN_CALLBACK6(opt, receivehead,
                                                  back[i].r.adr,
                                                  back[i].url_adr,
                                                  back[i].url_fil,
                                                  back[i].referer_adr,
                                                  back[i].referer_fil,
                                                  &back[i].r);
                    if (test_head != 1) {
                      hts_log_print(opt, LOG_WARNING,
                                    "External wrapper aborted transfer, breaking connection: %s%s",
                                    back[i].url_adr, back[i].url_fil);
                      back[i].status = STATUS_READY;    // FINI
                      back_set_finished(sback, i);
                      deletehttp(&back[i].r);
                      back[i].r.soc = INVALID_SOCKET;
                      strcpybuff(back[i].r.msg,
                                 "External wrapper aborted transfer");
                      back[i].r.statuscode = STATUSCODE_INVALID;
                    }
                  }

                  // Free headers memory now
                  // Actually, save them for informational purpose
                  if (!noFreebuff) {
                    char *block = back[i].r.adr;

                    back[i].r.adr = NULL;
                    deleteaddr(&back[i].r);
                    back[i].r.headers = block;
                  }

                  /* 
                     Status code and header-response hacks
                   */

                  // Check response : 203 == 200
                  if (back[i].r.statuscode ==
                      HTTP_NON_AUTHORITATIVE_INFORMATION) {
                    back[i].r.statuscode = HTTP_OK;     // forcer "OK"
                  } else if (back[i].r.statuscode == HTTP_CONTINUE) {
                    back[i].status = STATUS_WAIT_HEADERS;
                    back[i].r.size = 0;
                    back[i].r.totalsize = -1;
                    back[i].chunk_size = 0;
                    back[i].r.statuscode = STATUSCODE_INVALID;
                    back[i].r.msg[0] = '\0';
                    hts_log_print(opt, LOG_DEBUG,
                                  "Status 100 detected for %s%s, continuing headers",
                                  back[i].url_adr, back[i].url_fil);
                    continue;
                  }

                  /*
                     Solve "false" 416 problems
                   */
                  if (back[i].r.statuscode == HTTP_REQUESTED_RANGE_NOT_SATISFIABLE) {   // 'Requested Range Not Satisfiable'
                    // Example:
                    // Range: bytes=2830-
                    // ->
                    // Content-Range: bytes */2830
                    if (back[i].range_req_size == back[i].r.crange) {
                      filenote(&opt->state.strc, back[i].url_sav, NULL);
                      file_notify(opt, back[i].url_adr, back[i].url_fil,
                                  back[i].url_sav, 0, 0, back[i].r.notmodified);
                      deletehttp(&back[i].r);
                      back[i].r.soc = INVALID_SOCKET;
                      back[i].status = STATUS_READY;    // READY
                      back_set_finished(sback, i);
                      back[i].r.size = back[i].r.totalsize =
                        back[i].range_req_size;
                      back[i].r.statuscode = HTTP_NOT_MODIFIED; // NOT MODIFIED
                      hts_log_print(opt, LOG_DEBUG,
                                    "File seems complete (good 416 message), breaking connection: %s%s",
                                    back[i].url_adr, back[i].url_fil);
                    }
                  }
                  // transform 406 into 200 ; we'll catch embedded links inside the choice page
                  if (back[i].r.statuscode == 406) {    // 'Not Acceptable'
                    back[i].r.statuscode = HTTP_OK;
                  }
                  // 'do not erase already downloaded file'
                  // on an updated file
                  // with an error : consider a 304 error
                  if (!opt->delete_old) {
                    if (HTTP_IS_ERROR(back[i].r.statuscode) && back[i].is_update
                        && !back[i].testmode) {
                      if (back[i].url_sav[0] && fexist_utf8(back[i].url_sav)) {
                        hts_log_print(opt, LOG_DEBUG,
                                      "Error ignored %d (%s) because of 'no purge' option for %s%s",
                                      back[i].r.statuscode, back[i].r.msg,
                                      back[i].url_adr, back[i].url_fil);
                        back[i].r.statuscode = HTTP_NOT_MODIFIED;
                        deletehttp(&back[i].r);
                        back[i].r.soc = INVALID_SOCKET;
                      }
                    }
                  }
                  // Various hacks to limit re-transfers when updating a mirror
                  // Force update if same size detected
                  if (opt->sizehack) {
                    // We already have the file
                    // and ask the remote server for an update
                    // Some servers, especially dynamic pages severs, always
                    // answer that the page has been modified since last visit
                    // And answer with a 200 (OK) response, and the same page
                    // If the size is the same, and the option has been set, we assume
                    // that the file is identical - and therefore let's break the connection
                    if (back[i].is_update) {    // mise à jour
                      if (back[i].r.statuscode == HTTP_OK && !back[i].testmode) {       // 'OK'
                        htsblk r = cache_read(opt, cache, back[i].url_adr, back[i].url_fil, NULL, NULL);        // lire entrée cache

                        if (r.statuscode == HTTP_OK) {  // OK pas d'erreur cache
                          LLint len1, len2;

                          len1 = r.totalsize;
                          len2 = back[i].r.totalsize;
                          if (r.size > 0)
                            len1 = r.size;
                          if (len1 >= 0) {
                            if (len1 == len2) { // tailles identiques
                              back[i].r.statuscode = HTTP_NOT_MODIFIED; // forcer NOT MODIFIED
                              deletehttp(&back[i].r);
                              back[i].r.soc = INVALID_SOCKET;
                              hts_log_print(opt, LOG_DEBUG,
                                            "File seems complete (same size), breaking connection: %s%s",
                                            back[i].url_adr, back[i].url_fil);
                            }
                          }
                        } else {
                          hts_log_print(opt, LOG_WARNING,
                                        "File seems complete (same size), but there was a cache read error (%u): %s%s",
                                        r.statuscode, back[i].url_adr,
                                        back[i].url_fil);
                        }
                        if (r.adr) {
                          freet(r.adr);
                          r.adr = NULL;
                        }
                      }
                    }
                  }
                  // Various hacks to limit re-transfers when updating a mirror
                  // Detect already downloaded file (with another browser, for example)
                  if (opt->sizehack) {
                    if (!back[i].is_update) {   // mise à jour
                      if (back[i].r.statuscode == HTTP_OK && !back[i].testmode) {       // 'OK'
                        if (!is_hypertext_mime(opt, back[i].r.contenttype, back[i].url_fil)) {  // not HTML
                          if (strnotempty(back[i].url_sav)) {   // target found
                            int size = fsize_utf8(back[i].url_sav);     // target size

                            if (size >= 0) {
                              if (back[i].r.totalsize == size) {        // same size!
                                deletehttp(&back[i].r);
                                back[i].r.soc = INVALID_SOCKET;
                                back[i].status = STATUS_READY;  // READY
                                back_set_finished(sback, i);
                                back[i].r.size = back[i].r.totalsize;
                                filenote(&opt->state.strc, back[i].url_sav,
                                         NULL);
                                file_notify(opt, back[i].url_adr,
                                            back[i].url_fil, back[i].url_sav, 0,
                                            0, back[i].r.notmodified);
                                back[i].r.statuscode = HTTP_NOT_MODIFIED;       // NOT MODIFIED
                                hts_log_print(opt, LOG_DEBUG,
                                              "File seems complete (same size file discovered), breaking connection: %s%s",
                                              back[i].url_adr, back[i].url_fil);
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                  // Various hacks to limit re-transfers when updating a mirror
                  // Detect bad range: header
                  if (opt->sizehack) {
                    // We have request for a partial file (with a 'Range: NNN-' header)
                    // and received a complete file notification (200), with 'Content-length: NNN'
                    // it might be possible that we had the complete file
                    // this is the case in *most* cases, so break the connection
                    if (back[i].r.is_write == 0) {      // mode mémoire
                      if (back[i].r.adr == NULL) {      // rien n'a été écrit
                        if (!back[i].testmode) {        // pas mode test
                          if (strnotempty(back[i].url_sav)) {
                            if (strcmp(back[i].url_fil, "/robots.txt")) {
                              if (back[i].r.statuscode == HTTP_OK) {    // 'OK'
                                if (!is_hypertext_mime(opt, back[i].r.contenttype, back[i].url_fil)) {  // pas HTML
                                  if (back[i].r.statuscode == HTTP_OK) {        // "OK"
                                    if (back[i].range_req_size > 0) {   // but Range: requested
                                      if (back[i].range_req_size == back[i].r.totalsize) {      // And same size
#if HTS_DEBUG_CLOSESOCK
                                        DEBUG_W
                                          ("back_wait(skip_range): deletehttp\n");
#endif
                                        deletehttp(&back[i].r);
                                        back[i].r.soc = INVALID_SOCKET;
                                        back[i].status = STATUS_READY;  // READY
                                        back_set_finished(sback, i);
                                        back[i].r.size = back[i].r.totalsize;
                                        filenote(&opt->state.strc,
                                                 back[i].url_sav, NULL);
                                        file_notify(opt, back[i].url_adr,
                                                    back[i].url_fil,
                                                    back[i].url_sav, 0, 0,
                                                    back[i].r.notmodified);
                                        back[i].r.statuscode = HTTP_NOT_MODIFIED;       // NOT MODIFIED
                                        hts_log_print(opt, LOG_DEBUG,
                                                      "File seems complete (reget failed), breaking connection: %s%s",
                                                      back[i].url_adr,
                                                      back[i].url_fil);
                                      }
                                    }
                                  }

                                }
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                  // END - Various hacks to limit re-transfers when updating a mirror

                  /* 
                     End of status code and header-response hacks
                   */

                  /* Interdiction taille par le wizard? */
                  if (back[i].r.soc != INVALID_SOCKET) {
                    if (!back_checksize(opt, &back[i], 1)) {
                      back[i].status = STATUS_READY;    // FINI
                      back_set_finished(sback, i);
                      back[i].r.statuscode = STATUSCODE_TOO_BIG;
                      deletehttp(&back[i].r);
                      back[i].r.soc = INVALID_SOCKET;
                      if (!back[i].testmode)
                        strcpybuff(back[i].r.msg, "File too big");
                      else
                        strcpybuff(back[i].r.msg, "Test: File too big");
                    }
                  }

                  /* sinon, continuer */
                  /* if (back[i].r.soc!=INVALID_SOCKET) {   // ok récupérer body? */
                  // head: terminé
                  if (back[i].head_request) {
                    hts_log_print(opt, LOG_DEBUG, "Tested file: %s%s",
                                  back[i].url_adr, back[i].url_fil);
#if HTS_DEBUG_CLOSESOCK
                    DEBUG_W("back_wait(head request): deletehttp\n");
#endif
                    // Couper connexion
                    if (!back[i].http11) {      /* NO KA */
                      deletehttp(&back[i].r);
                      back[i].r.soc = INVALID_SOCKET;
                    }
                    back[i].status = STATUS_READY;      // terminé
                    back_set_finished(sback, i);
                  }
                  // traiter une éventuelle erreur 304 (cache à jour utilisable)
                  else if (back[i].r.statuscode == HTTP_NOT_MODIFIED) { // document à jour dans le cache
                    // lire dans le cache
                    // ** NOTE: pas de vérif de la taille ici!!
#if HTS_DEBUG_CLOSESOCK
                    DEBUG_W("back_wait(file is not modified): deletehttp\n");
#endif
                    /* clear everything but connection: switch, close, and reswitch */
                    {
                      htsblk tmp;

                      memset(&tmp, 0, sizeof(tmp));
                      back_connxfr(&back[i].r, &tmp);
                      back[i].r =
                        cache_read(opt, cache, back[i].url_adr, back[i].url_fil,
                                   back[i].url_sav, back[i].location_buffer);
                      back[i].r.location = back[i].location_buffer;
                      back_connxfr(&tmp, &back[i].r);
                    }

                    // hack:
                    // In case of 'if-unmodified-since' hack, a 304 status can be sent
                    // then, force 'ok' status
                    if (back[i].r.statuscode == STATUSCODE_INVALID) {
                      if (fexist_utf8(back[i].url_sav)) {
                        back[i].r.statuscode = HTTP_OK; // OK
                        strcpybuff(back[i].r.msg, "OK (cached)");
                        back[i].r.is_file = 1;
                        back[i].r.totalsize = back[i].r.size =
                          fsize_utf8(back[i].url_sav);
                        get_httptype(opt, back[i].r.contenttype,
                                     back[i].url_sav, 1);
                        hts_log_print(opt, LOG_DEBUG,
                                      "Not-modified status without cache guessed: %s%s",
                                      back[i].url_adr, back[i].url_fil);
                      }
                    }
                    // Status is okay?
                    if (back[i].r.statuscode != -1) {   // pas d'erreur de lecture
                      back[i].status = STATUS_READY;    // OK prêt
                      back_set_finished(sback, i);
                      back[i].r.notmodified = 1;        // NON modifié!
                      hts_log_print(opt, LOG_DEBUG,
                                    "File loaded after test from cache: %s%s",
                                    back[i].url_adr, back[i].url_fil);

                      // finalize
                      //file_notify(back[i].url_adr, back[i].url_fil, back[i].url_sav, 0, 0, back[i].r.notmodified);        // not modified
                      if (back[i].r.statuscode > 0) {
                        hts_log_print(opt, LOG_TRACE, "finalizing after cache load");
                        back_finalize(opt, cache, sback, i);
                      }
#if DEBUGCA
                      printf("..document à jour après requète: %s%s\n",
                             back[i].url_adr, back[i].url_fil);
#endif

                      //printf(">%s status %d\n",back[p].r.contenttype,back[p].r.statuscode);
                    } else {    // erreur
                      back[i].status = STATUS_READY;    // terminé
                      back_set_finished(sback, i);
                      //printf("erreur cache\n");

                    }

/********** NO - must complete the body! ********** */
#if 0
                  } else if (HTTP_IS_REDIRECT(back[i].r.statuscode)
                             || (back[i].r.statuscode == 412)
                             || (back[i].r.statuscode == 416)
                    ) {         // Ne pas prendre le html, erreurs connues et gérées
#if HTS_DEBUG_CLOSESOCK
                    DEBUG_W
                      ("back_wait(301,302,303,307,412,416..): deletehttp\n");
#endif
                    // Couper connexion
                    /*KA deletehttp(&back[i].r); back[i].r.soc=INVALID_SOCKET; */
                    back_maydeletehttp(opt, cache, sback, i);

                    back[i].status = STATUS_READY;      // terminé
                    back_set_finished(sback, i);
                    // finalize
                    if (back[i].r.statuscode > 0) {
                      hts_log_print(opt, LOG_TRACE, "finalizing redirect & 4xx");
                      back_finalize(opt, cache, sback, i);
                    }
#endif
/********** **************************** ********** */
                  } else {      // il faut aller le chercher

                    // effacer buffer (requète)
                    if (!noFreebuff) {
                      deleteaddr(&back[i].r);
                      back[i].r.size = 0;
                    }
                    // traiter 206 (partial content)
                    // xxc SI CHUNK VERIFIER QUE CA MARCHE??
                    if (back[i].r.statuscode == 206) {  // on nous envoie un morceau (la fin) coz une partie sur disque!
                      off_t sz = fsize_utf8(back[i].url_sav);

#if HDEBUG
                      printf("partial content: " LLintP " on disk..\n",
                             (LLint) sz);
#endif
                      if (sz >= 0) {
                        if (!is_hypertext_mime(opt, back[i].r.contenttype, back[i].url_sav)) {  // pas HTML
                          if (opt->getmode & 2) {       // on peut ecrire des non html  **sinon ben euhh sera intercepté plus loin, donc rap sur ce qui va sortir**
                            filenote(&opt->state.strc, back[i].url_sav, NULL);  // noter fichier comme connu
                            file_notify(opt, back[i].url_adr, back[i].url_fil,
                                        back[i].url_sav, 0, 1,
                                        back[i].r.notmodified);
                            back[i].r.out = FOPEN(fconv(catbuff, sizeof(catbuff), back[i].url_sav), "ab");       // append
                            if (back[i].r.out && opt->cache != 0) {
                              back[i].r.is_write = 1;   // écrire
                              back[i].r.size = sz;      // déja écrit
                              back[i].r.statuscode = HTTP_OK;   // Forcer 'OK'
                              if (back[i].r.totalsize >= 0)
                                back[i].r.totalsize += sz;      // plus en fait
                              fseek(back[i].r.out, 0, SEEK_END);        // à la fin
                              /* create a temporary reference file in case of broken mirror */
                              if (back_serialize_ref(opt, &back[i]) != 0) {
                                hts_log_print(opt, LOG_WARNING,
                                              "Could not create temporary reference file for %s%s",
                                              back[i].url_adr, back[i].url_fil);
                              }
#if HDEBUG
                              printf("continue interrupted file\n");
#endif
                            } else {    // On est dans la m**
                              back[i].status = STATUS_READY;    // terminé (voir plus loin)
                              back_set_finished(sback, i);
                              strcpybuff(back[i].r.msg,
                                         "Can not open partial file");
                            }
                          }
                        } else {        // mémoire
                          FILE *fp =
                            FOPEN(fconv(catbuff, sizeof(catbuff), back[i].url_sav), "rb");
                          if (fp) {
                            LLint alloc_mem = sz + 1;

                            if (back[i].r.totalsize >= 0)
                              alloc_mem += back[i].r.totalsize; // AJOUTER RESTANT!
                            if (deleteaddr(&back[i].r)
                                && (back[i].r.adr =
                                    (char *) malloct((size_t) alloc_mem))) {
                              back[i].r.size = sz;
                              if (back[i].r.totalsize >= 0)
                                back[i].r.totalsize += sz;      // plus en fait
                              if ((fread(back[i].r.adr, 1, sz, fp)) != sz) {
                                back[i].status = STATUS_READY;  // terminé (voir plus loin)
                                back_set_finished(sback, i);
                                strcpybuff(back[i].r.msg,
                                           "Can not read partial file");
                              } else {
                                back[i].r.statuscode = HTTP_OK; // Forcer 'OK'
#if HDEBUG
                                printf("continue in mem interrupted file\n");
#endif
                              }
                            } else {
                              back[i].status = STATUS_READY;    // terminé (voir plus loin)
                              back_set_finished(sback, i);
                              strcpybuff(back[i].r.msg,
                                         "No memory for partial file");
                            }
                            fclose(fp);
                          } else {      // Argh.. 
                            back[i].status = STATUS_READY;      // terminé (voir plus loin)
                            back_set_finished(sback, i);
                            strcpybuff(back[i].r.msg,
                                       "Can not open partial file");
                          }
                        }
                      } else {  // Non trouvé??
                        back[i].status = STATUS_READY;  // terminé (voir plus loin)
                        back_set_finished(sback, i);
                        strcpybuff(back[i].r.msg, "Can not find partial file");
                      }
                      // Erreur?
                      if (back[i].status == STATUS_READY) {
                        if (back[i].r.soc != INVALID_SOCKET) {
#if HTS_DEBUG_CLOSESOCK
                          DEBUG_W
                            ("back_wait(206 solve problems): deletehttp\n");
#endif
                          deletehttp(&back[i].r);
                        }
                        back[i].r.soc = INVALID_SOCKET;
                        //back[i].r.statuscode=206;  ????????
                        back[i].r.statuscode = STATUSCODE_NON_FATAL;
                        if (strnotempty(back[i].r.msg))
                          strcpybuff(back[i].r.msg,
                                     "Error attempting to solve status 206 (partial file)");
                      }
                    }

                    if (back[i].status != 0) {  // non terminé (erreur)
                      if (!back[i].testmode) {  // fichier normal

                        if (back[i].r.empty /* ?? && back[i].r.statuscode==HTTP_OK */ ) {       // empty response
                          // Couper connexion
                          back_maydeletehttp(opt, cache, sback, i);
                          /* KA deletehttp(&back[i].r); back[i].r.soc=INVALID_SOCKET; */
                          back[i].status = STATUS_READY;        // terminé
                          back_set_finished(sback, i);
                          if (deleteaddr(&back[i].r)
                              && (back[i].r.adr = (char *) malloct(2))) {
                            back[i].r.adr[0] = 0;
                          }
                          hts_log_print(opt, LOG_TRACE, "finalizing empty");
                          back_finalize(opt, cache, sback, i);
                        } else if (!back[i].r.is_chunk) {       // pas de chunk
                          //if (back[i].r.http11!=2) {    // pas de chunk
                          back[i].is_chunk = 0;
                          back[i].status = 1;   // start body
                        } else {
#if CHUNKDEBUG==1
                          printf("[%d] chunk encoding detected %s..\n",
                                 (int) back[i].r.soc, back[i].url_fil);
#endif
                          back[i].is_chunk = 1;
                          back[i].chunk_adr = NULL;
                          back[i].chunk_size = 0;
                          back[i].chunk_blocksize = 0;
                          back[i].status = STATUS_CHUNK_WAIT;   // start body wait chunk
                          back[i].r.totalsize = -1;     /* devalidate size! (rfc) */
                        }
                        if (back[i].rateout > 0) {
                          back[i].rateout_time = time_local();  // refresh pour transfer rate
                        }
#if HDEBUG
                        printf("(buffer) start body!\n");
#endif
                      } else {  // mode test, ne pas passer en 1!!
                        back[i].status = STATUS_READY;  // READY
                        back_set_finished(sback, i);
#if HTS_DEBUG_CLOSESOCK
                        DEBUG_W("back_wait(test ok): deletehttp\n");
#endif
                        deletehttp(&back[i].r);
                        back[i].r.soc = INVALID_SOCKET;
                        if (back[i].r.statuscode == HTTP_OK) {
                          strcpybuff(back[i].r.msg, "Test: OK");
                          back[i].r.statuscode = STATUSCODE_TEST_OK;    // test réussi
                        } else {        // test a échoué, on ne change rien sauf que l'erreur est à titre indicatif
                          char tempo[1000];

                          strcpybuff(tempo, back[i].r.msg);
                          strcpybuff(back[i].r.msg, "Test: ");
                          strcatbuff(back[i].r.msg, tempo);
                        }

                      }
                    }

                  }

                  /*} */

                }               // si LF
              }                 // r.size>2
            }                   // si == 99

          }                     // si pas d'erreurs
#if BDEBUG==1
          printf("bytes overall: %d\n", back[i].r.size);
#endif
        }                       // données dispo

        // en cas d'erreur cl, supprimer éventuel fichier sur disque
#if HTS_REMOVE_BAD_FILES
        if (back[i].status < 0) {
          if (!back[i].testmode) {      // pas en test
            UNLINK(back[i].url_sav);    // éliminer fichier (endommagé)
            //printf("&& %s\n",back[i].url_sav);
          }
        }
#endif

        /* funny log for commandline users */
        //if (!opt->quiet) {  
        // petite animation
        if (opt->verbosedisplay == 1) {
          if (back[i].status == STATUS_READY) {
            if (back[i].r.statuscode == HTTP_OK)
              printf("* %s%s (" LLintP " bytes) - OK" VT_CLREOL "\r",
                     back[i].url_adr, back[i].url_fil, (LLint) back[i].r.size);
            else
              printf("* %s%s (" LLintP " bytes) - %d" VT_CLREOL "\r",
                     back[i].url_adr, back[i].url_fil, (LLint) back[i].r.size,
                     back[i].r.statuscode);
            fflush(stdout);
          }
        }
        //}

      }                         // status>0
    }                           // for

    // vérifier timeouts
    if (gestion_timeout) {
      TStamp act;

      act = time_local();       // temps en secondes
      for(i_mod = 0; i_mod < (unsigned int) back_max; i_mod++) {
        // for(i=0;i<back_max;i++) {
        unsigned int i = (i_mod + mod_random) % (back_max);

        if (back[i].status > 0) {       // réception/connexion/..
          if (back[i].timeout > 0) {
            //printf("time check %d\n",((int) (act-back[i].timeout_refresh))-back[i].timeout);
            if (((int) (act - back[i].timeout_refresh)) >= back[i].timeout) {
              hts_log_print(opt, LOG_DEBUG, "connection timed out for %s%s", back[i].url_adr,
                back[i].url_fil);
              if (back[i].r.soc != INVALID_SOCKET) {
#if HTS_DEBUG_CLOSESOCK
                DEBUG_W("back_wait(timeout): deletehttp\n");
#endif
                deletehttp(&back[i].r);
              }
              back[i].r.soc = INVALID_SOCKET;
              back[i].r.statuscode = STATUSCODE_TIMEOUT;
              if (back[i].status == STATUS_CONNECTING)
                strcpybuff(back[i].r.msg, "Connect Time Out");
              else if (back[i].status == STATUS_WAIT_DNS)
                strcpybuff(back[i].r.msg, "DNS Time Out");
              else
                strcpybuff(back[i].r.msg, "Receive Time Out");
              back[i].status = STATUS_READY;    // terminé
              back_set_finished(sback, i);
            } else if ((back[i].rateout > 0) && (back[i].status < 99)) {
              if (((int) (act - back[i].rateout_time)) >= HTS_WATCHRATE) {      // checker au bout de 15s
                if ((int) ((back[i].r.size) / (act - back[i].rateout_time)) < back[i].rateout) {        // trop lent
                  back[i].status = STATUS_READY;        // terminé
                  back_set_finished(sback, i);
                  if (back[i].r.soc != INVALID_SOCKET) {
#if HTS_DEBUG_CLOSESOCK
                    DEBUG_W("back_wait(rateout): deletehttp\n");
#endif
                    deletehttp(&back[i].r);
                  }
                  back[i].r.soc = INVALID_SOCKET;
                  back[i].r.statuscode = STATUSCODE_SLOW;
                  strcpybuff(back[i].r.msg, "Transfer Rate Too Low");
                }
              }
            }
          }
        }
      }
    }
    max_loop--;
    max_loop_chk++;
  } while((busy_state) && (busy_recv) && (max_loop > 0));
  if ((!busy_recv) && (!busy_state)) {
    if (max_loop_chk >= 1) {
      Sleep(10);                // un tite pause pour éviter les lag..
    }
  }
}

int back_checksize(httrackp * opt, lien_back * eback, int check_only_totalsize) {
  LLint size_to_test;

  if (check_only_totalsize)
    size_to_test = eback->r.totalsize;
  else
    size_to_test = max(eback->r.totalsize, eback->r.size);
  if (size_to_test >= 0) {

    /* Interdiction taille par le wizard? */
    if (hts_testlinksize(opt, eback->url_adr, eback->url_fil,
                         size_to_test / 1024) == -1) {
      return 0;                 /* interdit */
    }

    /* vérifier taille classique (heml et non html) */
    if ((istoobig
         (opt, size_to_test, eback->maxfile_html, eback->maxfile_nonhtml,
          eback->r.contenttype))) {
      return 0;                 /* interdit */
    }
  }
  return 1;
}

int back_checkmirror(httrackp * opt) {
  // Check max size
  if ((opt->maxsite > 0) && (HTS_STAT.stat_bytes >= opt->maxsite)) {
    if (!opt->state.stop) {     /* not yet stopped */
      hts_log_print(opt, LOG_ERROR,
                    "More than " LLintP
                    " bytes have been transferred.. giving up",
                    (LLint) opt->maxsite);
      /* cancel mirror smoothly */
      hts_request_stop(opt, 0);
    }
    return 1;                   /* don'k break mirror too sharply for size limits, but stop requested */
    /*return 0;
     */
  }
  // Check max time
  if ((opt->maxtime > 0)
      && ((time_local() - HTS_STAT.stat_timestart) >= opt->maxtime)) {
    if (!opt->state.stop) {     /* not yet stopped */
      hts_log_print(opt, LOG_ERROR, "More than %d seconds passed.. giving up",
                    opt->maxtime);
      /* cancel mirror smoothly */
      hts_request_stop(opt, 0);
    }
  }
  return 1;                     /* Ok, go on */
}

// octets transférés + add
LLint back_transferred(LLint nb, struct_back * sback) {
  lien_back *const back = sback->lnk;
  const int back_max = sback->count;
  int i;

  // ajouter octets en instance
  for(i = 0; i < back_max; i++)
    if ((back[i].status > 0) && (back[i].status < 99 || back[i].status >= 1000))
      nb += back[i].r.size;
  // stored (ready) slots
  if (sback->ready != NULL) {
#ifndef HTS_NO_BACK_ON_DISK
    nb += sback->ready_size_bytes;
#else
    struct_coucal_enum e = coucal_enum_new(sback->ready);
    coucal_item *item;

    while((item = coucal_enum_next(&e))) {
      lien_back *ritem = (lien_back *) item->value.ptr;

      if ((ritem->status > 0) && (ritem->status < 99 || ritem->status >= 1000))
        nb += ritem->r.size;
    }
#endif
  }
  return nb;
}

// infos backing
// j: 1 afficher sockets 2 afficher autres 3 tout afficher
void back_info(struct_back * sback, int i, int j, FILE * fp) {
  lien_back *const back = sback->lnk;
  const int back_max = sback->count;

  assertf(i >= 0 && i < back_max);
  if (back[i].status >= 0) {
    char BIGSTK s[HTS_URLMAXSIZE * 2 + 1024];

    s[0] = '\0';
    back_infostr(sback, i, j, s);
    strcatbuff(s, LF);
    fprintf(fp, "%s", s);
  }
}

// infos backing
// j: 1 afficher sockets 2 afficher autres 3 tout afficher
void back_infostr(struct_back * sback, int i, int j, char *s) {
  lien_back *const back = sback->lnk;
  const int back_max = sback->count;

  assertf(i >= 0 && i < back_max);
  if (back[i].status >= 0) {
    int aff = 0;

    if (j & 1) {
      if (back[i].status == STATUS_CONNECTING) {
        strcatbuff(s, "CONNECT ");
      } else if (back[i].status == STATUS_WAIT_HEADERS) {
        strcatbuff(s, "INFOS ");
        aff = 1;
      } else if (back[i].status == STATUS_CHUNK_WAIT
                 || back[i].status == STATUS_CHUNK_CR) {
        strcatbuff(s, "INFOSC");        // infos chunk
        aff = 1;
      } else if (back[i].status > 0) {
        strcatbuff(s, "RECEIVE ");
        aff = 1;
      }
    }
    if (j & 2) {
      if (back[i].status == STATUS_READY) {
        switch (back[i].r.statuscode) {
        case 200:
          strcatbuff(s, "READY ");
          aff = 1;
          break;
        case -1:
          strcatbuff(s, "ERROR ");
          aff = 1;
          break;
        case -2:
          strcatbuff(s, "TIMEOUT ");
          aff = 1;
          break;
        case -3:
          strcatbuff(s, "TOOSLOW ");
          aff = 1;
          break;
        case 400:
          strcatbuff(s, "BADREQUEST ");
          aff = 1;
          break;
        case 401:
        case 403:
          strcatbuff(s, "FORBIDDEN ");
          aff = 1;
          break;
        case 404:
          strcatbuff(s, "NOT FOUND ");
          aff = 1;
          break;
        case 500:
          strcatbuff(s, "SERVERROR ");
          aff = 1;
          break;
        default:
          {
            char s2[256];

            sprintf(s2, "ERROR(%d)", back[i].r.statuscode);
            strcatbuff(s, s2);
          }
          aff = 1;
        }
      }
    }

    if (aff) {
      {
        char BIGSTK s2[HTS_URLMAXSIZE * 2 + 1024];

        sprintf(s2, "\"%s", back[i].url_adr);
        strcatbuff(s, s2);

        if (back[i].url_fil[0] != '/')
          strcatbuff(s, "/");
        sprintf(s2, "%s\" ", back[i].url_fil);
        strcatbuff(s, s2);
        sprintf(s, LLintP " " LLintP " ", (LLint) back[i].r.size,
                (LLint) back[i].r.totalsize);
        strcatbuff(s, s2);
      }
    }
  }
}

// -- backing --
