/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998-2017 Xavier Roche and other contributors

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

Please visit our Website: http://www.httrack.com
*/

/* Parts (inside ARC format routines) by Lars Clausen (lc@statsbiblioteket.dk) */

/* ------------------------------------------------------------ */
/* File: Cache manager for ProxyTrack                           */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __ANDROID__
static long int  timezone = 0;
#endif

/* Locking */
#ifdef _WIN32
#include <process.h>            /* _beginthread, _endthread */
#else
#include <pthread.h>
#endif

#define HTSSAFE_ABORT_FUNCTION(A,B,C)
#include "htsglobal.h"

#define HTS_INTERNAL_BYTECODE
#include "coucal.h"
#include "htsmd5.h"
#undef HTS_INTERNAL_BYTECODE
#include "../minizip/mztools.h"
#include "../minizip/zip.h"

#include "htscore.h"
#include "htsback.h"

#include "store.h"
#include "proxystrings.h"
#include "proxytrack.h"

/* Unlocked functions */

static int PT_LookupCache__New_u(PT_Index index, const char *url);
static PT_Element PT_ReadCache__New_u(PT_Index index, const char *url,
                                      int flags);

static int PT_LookupCache__Old_u(PT_Index index, const char *url);
static PT_Element PT_ReadCache__Old_u(PT_Index index, const char *url,
                                      int flags);

static int PT_LookupCache__Arc_u(PT_Index index, const char *url);
static PT_Element PT_ReadCache__Arc_u(PT_Index index, const char *url,
                                      int flags);

/* Locking */

#ifdef _WIN32
void MutexInit(PT_Mutex * pMutex) {
  *pMutex = CreateMutex(NULL, FALSE, NULL);
}

void MutexLock(PT_Mutex * pMutex) {
  WaitForSingleObject(*pMutex, INFINITE);
}

void MutexUnlock(PT_Mutex * pMutex) {
  ReleaseMutex(*pMutex);
}

void MutexFree(PT_Mutex * pMutex) {
  CloseHandle(*pMutex);
  *pMutex = NULL;
}
#else
void MutexInit(PT_Mutex * pMutex) {
  (void) pthread_mutex_init(pMutex, 0);
}

void MutexLock(PT_Mutex * pMutex) {
  pthread_mutex_lock(pMutex);
}

void MutexUnlock(PT_Mutex * pMutex) {
  pthread_mutex_unlock(pMutex);
}

void MutexFree(PT_Mutex * pMutex) {
  pthread_mutex_destroy(pMutex);
}
#endif

/* Indexes */

typedef struct _PT_Index__New _PT_Index__New;
typedef struct _PT_Index__Old _PT_Index__Old;
typedef struct _PT_Index__Arc _PT_Index__Arc;
typedef struct _PT_Index_Functions _PT_Index_Functions;

typedef struct _PT_Index__New *PT_Index__New;
typedef struct _PT_Index__Old *PT_Index__Old;
typedef struct _PT_Index__Arc *PT_Index__Arc;
typedef struct _PT_Index_Functions *PT_Index_Functions;

enum {
  PT_CACHE_UNDEFINED = -1,
  PT_CACHE_MIN = 0,
  PT_CACHE__NEW = PT_CACHE_MIN,
  PT_CACHE__OLD,
  PT_CACHE__ARC,
  PT_CACHE_MAX = PT_CACHE__ARC
};

static int PT_LoadCache__New(PT_Index index, const char *filename);
static void PT_Index_Delete__New(PT_Index * pindex);
static PT_Element PT_ReadCache__New(PT_Index index, const char *url, int flags);
static int PT_LookupCache__New(PT_Index index, const char *url);
static int PT_SaveCache__New(PT_Indexes indexes, const char *filename);
 /**/ static int PT_LoadCache__Old(PT_Index index, const char *filename);
static void PT_Index_Delete__Old(PT_Index * pindex);
static PT_Element PT_ReadCache__Old(PT_Index index, const char *url, int flags);
static int PT_LookupCache__Old(PT_Index index, const char *url);
 /**/ static int PT_LoadCache__Arc(PT_Index index, const char *filename);
static void PT_Index_Delete__Arc(PT_Index * pindex);
static PT_Element PT_ReadCache__Arc(PT_Index index, const char *url, int flags);
static int PT_LookupCache__Arc(PT_Index index, const char *url);
static int PT_SaveCache__Arc(PT_Indexes indexes, const char *filename);

struct _PT_Index_Functions {
  /* Mandatory services */
  int (*PT_LoadCache) (PT_Index index, const char *filename);
  void (*PT_Index_Delete) (PT_Index * pindex);
    PT_Element(*PT_ReadCache) (PT_Index index, const char *url, int flags);
  int (*PT_LookupCache) (PT_Index index, const char *url);

  /* Optional services */
  int (*PT_SaveCache) (PT_Indexes indexes, const char *filename);
};

static _PT_Index_Functions _IndexFuncts[] = {
  {PT_LoadCache__New, PT_Index_Delete__New, PT_ReadCache__New,
   PT_LookupCache__New, PT_SaveCache__New},
  {PT_LoadCache__Old, PT_Index_Delete__Old, PT_ReadCache__Old,
   PT_LookupCache__Old, NULL},
  {PT_LoadCache__Arc, PT_Index_Delete__Arc, PT_ReadCache__Arc,
   PT_LookupCache__Arc, PT_SaveCache__Arc},
  {NULL, NULL, NULL, NULL}
};

#define PT_INDEX_COMMON_STRUCTURE \
	time_t timestamp;								\
	coucal hash;										\
	char startUrl[1024]

struct _PT_Index__New {
  PT_INDEX_COMMON_STRUCTURE;
  char path[1024];              /* either empty, or must include ending / */
  int fixedPath;
  int safeCache;
  unzFile zFile;
  PT_Mutex zFileLock;
};

struct _PT_Index__Old {
  PT_INDEX_COMMON_STRUCTURE;
  char filenameDat[1024];
  char filenameNdx[1024];
  FILE *dat, *ndx;
  PT_Mutex fileLock;
  int version;
  char lastmodified[1024];
  char path[1024];              /* either empty, or must include ending / */
  int fixedPath;
  int safeCache;
};

struct _PT_Index__Arc {
  PT_INDEX_COMMON_STRUCTURE;
  FILE *file;
  PT_Mutex fileLock;
  int version;
  char lastmodified[1024];
  char line[2048];
  char filenameIndexBuff[2048];
};

struct _PT_Index {
  int type;
  union {
    _PT_Index__New formatNew;
    _PT_Index__Old formatOld;
    _PT_Index__Arc formatArc;
    struct {
      PT_INDEX_COMMON_STRUCTURE;
    } common;
  } slots;
};

struct _PT_Indexes {
  coucal cil;
  struct _PT_Index **index;
  int index_size;
};

struct _PT_CacheItem {
  time_t lastUsed;
  size_t size;
  void *data;
};

struct _PT_Cache {
  coucal index;
  size_t maxSize;
  size_t totalSize;
  int count;
};

PT_Indexes PT_New(void) {
  PT_Indexes index = (PT_Indexes) calloc(sizeof(_PT_Indexes), 1);

  index->cil = coucal_new(0);
  coucal_set_name(index->cil, "index->cil");
  index->index_size = 0;
  index->index = NULL;
  return index;
}

void PT_Delete(PT_Indexes index) {
  if (index != NULL) {
    coucal_delete(&index->cil);
    free(index);
  }
}

int PT_RemoveIndex(PT_Indexes index, int indexId) {
  return 0;
}

static int binput(char *buff, char *s, int max) {
  int count = 0;
  int destCount = 0;

  // Note: \0 will return 1
  while(destCount < max && buff[count] != '\0' && buff[count] != '\n') {
    if (buff[count] != '\r') {
      s[destCount++] = buff[count];
    }
    count++;
  }
  s[destCount] = '\0';

  // then return the supplemental jump offset
  return count + 1;
}

static time_t file_timestamp(const char *file) {
  struct stat buf;

  if (stat(file, &buf) == 0) {
    time_t tt = buf.st_mtime;

    if (tt != (time_t) 0 && tt != (time_t) - 1) {
      return tt;
    }
  }
  return (time_t) 0;
}

static int PT_Index_Check__(PT_Index index, const char *file, int line) {
  if (index == NULL)
    return 0;
  if (index->type >= PT_CACHE_MIN && index->type <= PT_CACHE_MAX)
    return 1;
  proxytrack_print_log(CRITICAL, "index corrupted in memory at %s:%d", file,
                       line);
  return 0;
}

#define SAFE_INDEX(index) PT_Index_Check__(index, __FILE__, __LINE__)

/* ------------------------------------------------------------ */
/* Generic cache dispatch                                       */
/* ------------------------------------------------------------ */

void PT_Index_Delete(PT_Index * pindex) {
  if (pindex != NULL && (*pindex) != NULL) {
    PT_Index index = *pindex;

    if (SAFE_INDEX(index)) {
      _IndexFuncts[index->type].PT_Index_Delete(pindex);
    }
    free(index);
    *pindex = NULL;
  }
}

static void PT_Index_Delete__New(PT_Index * pindex) {
  if (pindex != NULL && (*pindex) != NULL) {
    PT_Index__New index = &(*pindex)->slots.formatNew;

    if (index->zFile != NULL) {
      unzClose(index->zFile);
      index->zFile = NULL;
    }
    if (index->hash != NULL) {
      coucal_delete(&index->hash);
      index->hash = NULL;
    }
    MutexFree(&index->zFileLock);
  }
}

static void PT_Index_Delete__Old(PT_Index * pindex) {
  if (pindex != NULL && (*pindex) != NULL) {
    PT_Index__Old index = &(*pindex)->slots.formatOld;

    if (index->dat != NULL) {
      fclose(index->dat);
    }
    if (index->ndx != NULL) {
      fclose(index->ndx);
    }
    if (index->hash != NULL) {
      coucal_delete(&index->hash);
      index->hash = NULL;
    }
    MutexFree(&index->fileLock);
  }
}

static void PT_Index_Delete__Arc(PT_Index * pindex) {
  if (pindex != NULL && (*pindex) != NULL) {
    PT_Index__Arc index = &(*pindex)->slots.formatArc;

    if (index->file != NULL) {
      fclose(index->file);
    }
    MutexFree(&index->fileLock);
  }
}

int PT_AddIndex(PT_Indexes indexes, const char *path) {
  PT_Index index = PT_LoadCache(path);

  if (index != NULL) {
    int ret = PT_IndexMerge(indexes, &index);

    if (index != NULL) {
      PT_Index_Delete(&index);
    }
    return ret;
  }
  return -1;
}

PT_Element PT_Index_HTML_BuildRootInfo(PT_Indexes indexes) {
  if (indexes != NULL) {
    PT_Element elt = PT_ElementNew();
    int i;
    String html = STRING_EMPTY;

    StringClear(html);
    StringCat(html,
              "<html>" PROXYTRACK_COMMENT_HEADER
              DISABLE_IE_FRIENDLY_HTTP_ERROR_MESSAGES "<head>\r\n"
              "<title>ProxyTrack " PROXYTRACK_VERSION " Catalog</title>"
              "</head>\r\n" "<body>\r\n"
              "<h3>Available sites in this cache:</h3><br />" "<br />");
    StringCat(html, "<ul>\r\n");
    for(i = 0; i < indexes->index_size; i++) {
      if (indexes->index[i] != NULL
          && indexes->index[i]->slots.common.startUrl[0] != '\0') {
        const char *url = indexes->index[i]->slots.common.startUrl;

        StringCat(html, "<li>\r\n");
        StringCat(html, "<a href=\"");
        StringCat(html, url);
        StringCat(html, "\">");
        StringCat(html, url);
        StringCat(html, "</a>\r\n");
        StringCat(html, "</li>\r\n");
      }
    }
    StringCat(html, "</ul>\r\n");
    StringCat(html, "</body></html>\r\n");
    elt->size = StringLength(html);
    elt->adr = StringAcquire(&html);
    elt->statuscode = HTTP_OK;
    strcpy(elt->charset, "iso-8859-1");
    strcpy(elt->contenttype, "text/html");
    strcpy(elt->msg, "OK");
    StringFree(html);
    return elt;
  }
  return NULL;
}

static char *strchr_stop(char *str, char c, char stop) {
  for(; *str != 0 && *str != stop && *str != c; str++) ;
  if (*str == c)
    return str;
  return NULL;
}

char **PT_Enumerate(PT_Indexes indexes, const char *url, int subtree) {
  // should be cached!
  if (indexes != NULL && indexes->cil != NULL) {
    unsigned int urlSize;
    String list = STRING_EMPTY;
    String listindexes = STRING_EMPTY;
    String subitem = STRING_EMPTY;
    unsigned int listCount = 0;
    struct_coucal_enum en = coucal_enum_new(indexes->cil);
    coucal_item *chain;
    coucal hdupes = NULL;

    if (!subtree) {
      hdupes = coucal_new(0);
      coucal_set_name(hdupes, "hdupes");
    }
    StringClear(list);
    StringClear(listindexes);
    StringClear(subitem);
    if (strncmp(url, "http://", 7) == 0)
      url += 7;
    urlSize = (unsigned int) strlen(url);
    while((chain = coucal_enum_next(&en))) {
      long int index = (long int) chain->value.intg;

      if (urlSize == 0 || strncmp(chain->name, url, urlSize) == 0) {
        if (index >= 0 && index < indexes->index_size) {
          char *item = (char*) chain->name + urlSize;

          if (*item == '/')
            item++;
          {
            char *pos = subtree ? 0 : strchr_stop(item, '/', '?');
            unsigned int len =
              pos ? (unsigned int) (pos - item) : (unsigned int) strlen(item);
            if (len > 0 /* default document */  || *item == 0) {
              int isFolder = (item[len] == '/');

              StringClear(subitem);
              if (len > 0)
                StringMemcat(subitem, item, len);
              if (len == 0 || !coucal_exists(hdupes, StringBuff(subitem))) {
                char *ptr = NULL;

                ptr += StringLength(list);
                if (len > 0)
                  StringCat(list, StringBuff(subitem));
                if (isFolder)
                  StringCat(list, "/");
                StringMemcat(list, "\0", 1);    /* NULL terminated strings */
                StringMemcat(listindexes, (char*) &ptr, sizeof(ptr));
                listCount++;
                coucal_write(hdupes, StringBuff(subitem), 0);
              }
            }
          }
        } else {
          proxytrack_print_log(CRITICAL,
                               "PT_Enumerate:Corrupted central index locator");
        }
      }
    }
    StringFree(subitem);
    coucal_delete(&hdupes);
    if (listCount > 0) {
      unsigned int i;
      void *blk;
      char *nullPointer = NULL;
      char *startStrings;

      /* NULL terminated index */
      StringMemcat(listindexes, (char*) &nullPointer, sizeof(nullPointer));
      /* start of all strings (index) */
      startStrings = nullPointer + StringLength(listindexes);
      /* copy list of URLs after indexes */
      StringMemcat(listindexes, StringBuff(list), StringLength(list));
      /* ---- no reallocation beyond this point (fixed addresses) ---- */
      /* start of all strings (pointer) */
      startStrings = (startStrings - nullPointer) + StringBuffRW(listindexes);
      /* transform indexes into references */
      for(i = 0; i < listCount; i++) {
        char *ptr = NULL;
        unsigned int ndx;

        memcpy(&ptr, &StringBuff(listindexes)[i * sizeof(char *)],
               sizeof(char *));
        ndx = (unsigned int) (ptr - nullPointer);
        ptr = startStrings + ndx;
        memcpy(&StringBuffRW(listindexes)[i * sizeof(char *)], &ptr,
               sizeof(char *));
      }
      blk = StringAcquire(&listindexes);
      StringFree(list);
      StringFree(listindexes);
      return (char **) blk;
    }
  }
  return NULL;
}

void PT_Enumerate_Delete(char ***plist) {
  if (plist != NULL && *plist != NULL) {
    free(*plist);
    *plist = NULL;
  }
}

static int PT_GetType(const char *filename) {
  char *dot = strrchr(filename, '.');

  if (dot != NULL) {
    if (strcasecmp(dot, ".zip") == 0) {
      return PT_CACHE__NEW;
    } else if (strcasecmp(dot, ".ndx") == 0 || strcasecmp(dot, ".dat") == 0) {
      return PT_CACHE__OLD;
    } else if (strcasecmp(dot, ".arc") == 0) {
      return PT_CACHE__ARC;
    }
  }
  return PT_CACHE_UNDEFINED;
}

PT_Index PT_LoadCache(const char *filename) {
  int type = PT_GetType(filename);

  if (type != PT_CACHE_UNDEFINED) {
    PT_Index index = calloc(sizeof(_PT_Index), 1);

    if (index != NULL) {
      index->type = type;
      index->slots.common.timestamp = (time_t) time(NULL);
      index->slots.common.startUrl[0] = '\0';
      index->slots.common.hash = coucal_new(0);
      coucal_set_name(index->slots.common.hash, "index->slots.common.hash");
      if (!_IndexFuncts[type].PT_LoadCache(index, filename)) {
        proxytrack_print_log(DEBUG,
                             "reading httrack cache (format #%d) %s : error",
                             type, filename);
        free(index);
        index = NULL;
        return NULL;
      } else {
        proxytrack_print_log(DEBUG,
                             "reading httrack cache (format #%d) %s : success",
                             type, filename);
      }
      /* default starting URL is the first hash entry */
      if (index->slots.common.startUrl[0] == '\0') {
        struct_coucal_enum en = coucal_enum_new(index->slots.common.hash);
        coucal_item *chain;

        chain = coucal_enum_next(&en);
        if (chain != NULL && strstr(chain->name, "/robots.txt") != NULL) {
          chain = coucal_enum_next(&en);
        }
        if (chain != NULL) {
          if (!link_has_authority(chain->name))
            strcat(index->slots.common.startUrl, "http://");
          strcat(index->slots.common.startUrl, chain->name);
        }
      }
    }
    return index;
  }
  return NULL;
}

static long int filesize(const char *filename) {
  struct stat st;

  memset(&st, 0, sizeof(st));
  if (stat(filename, &st) == 0) {
    return (long int) st.st_size;
  }
  return -1;
}

int PT_LookupCache(PT_Index index, const char *url) {
  if (index != NULL && SAFE_INDEX(index)) {
    return _IndexFuncts[index->type].PT_LookupCache(index, url);
  }
  return 0;
}

int PT_SaveCache(PT_Indexes indexes, const char *filename) {
  int type = PT_GetType(filename);

  if (type != PT_CACHE_UNDEFINED) {
    if (_IndexFuncts[type].PT_SaveCache != NULL) {
      int ret = _IndexFuncts[type].PT_SaveCache(indexes, filename);

      if (ret == 0) {
        (void) set_filetime_time_t(filename, PT_GetTimeIndex(indexes));
        return 0;
      }
    }
  }
  return -1;
}

int PT_EnumCache(PT_Indexes indexes,
                 int (*callback) (void *, const char *url, PT_Element),
                 void *arg) {
  if (indexes != NULL && indexes->cil != NULL) {
    struct_coucal_enum en = coucal_enum_new(indexes->cil);
    coucal_item *chain;

    while((chain = coucal_enum_next(&en))) {
      const long int index_id = (long int) chain->value.intg;
      const char *const url = chain->name;

      if (index_id >= 0 && index_id <= indexes->index_size) {
        PT_Element item =
          PT_ReadCache(indexes->index[index_id], url,
                       FETCH_HEADERS | FETCH_BODY);
        if (item != NULL) {
          int ret = callback(arg, url, item);

          PT_Element_Delete(&item);
          if (ret != 0)
            return ret;
        }
      } else {
        proxytrack_print_log(CRITICAL,
                             "PT_ReadCache:Corrupted central index locator");
        return -1;
      }
    }
  }
  return 0;
}

time_t PT_Index_Timestamp(PT_Index index) {
  return index->slots.common.timestamp;
}

static int PT_LookupCache__New(PT_Index index, const char *url) {
  int retCode;

  MutexLock(&index->slots.formatNew.zFileLock);
  {
    retCode = PT_LookupCache__New_u(index, url);
  }
  MutexUnlock(&index->slots.formatNew.zFileLock);
  return retCode;
}

static int PT_LookupCache__New_u(PT_Index index_, const char *url) {
  if (index_ != NULL) {
    PT_Index__New index = &index_->slots.formatNew;

    if (index->hash != NULL && index->zFile != NULL && url != NULL && *url != 0) {
      int hash_pos_return;

      if (strncmp(url, "http://", 7) == 0)
        url += 7;
      hash_pos_return = coucal_read(index->hash, url, NULL);
      if (hash_pos_return)
        return 1;
    }
  }
  return 0;
}

int PT_IndexMerge(PT_Indexes indexes, PT_Index * pindex) {
  if (pindex != NULL && *pindex != NULL && (*pindex)->slots.common.hash != NULL
      && indexes != NULL) {
    PT_Index index = *pindex;
    struct_coucal_enum en = coucal_enum_new(index->slots.common.hash);
    coucal_item *chain;
    int index_id = indexes->index_size++;
    int nMerged = 0;

    if ((indexes->index =
         realloc(indexes->index,
                 sizeof(struct _PT_Index) * indexes->index_size)) != NULL) {
      indexes->index[index_id] = index;
      *pindex = NULL;
      while((chain = coucal_enum_next(&en)) != NULL) {
        const char *url = chain->name;

        if (url != NULL && url[0] != '\0') {
          intptr_t previous_index_id = 0;

          if (coucal_read(indexes->cil, url, &previous_index_id)) {
            if (previous_index_id >= 0
                && previous_index_id < indexes->index_size) {
              if (indexes->index[previous_index_id]->slots.common.timestamp > index->slots.common.timestamp)    // existing entry is newer
                break;
            } else {
              proxytrack_print_log(CRITICAL,
                                   "PT_IndexMerge:Corrupted central index locator");
            }
          }
          coucal_write(indexes->cil, chain->name, index_id);
          nMerged++;
        }
      }
    } else {
      proxytrack_print_log(CRITICAL, "PT_IndexMerge:Memory exhausted");
    }
    return nMerged;
  }
  return -1;
}

void PT_Element_Delete(PT_Element * pentry) {
  if (pentry != NULL) {
    PT_Element entry = *pentry;

    if (entry != NULL) {
      if (entry->adr != NULL) {
        free(entry->adr);
        entry->adr = NULL;
      }
      if (entry->headers != NULL) {
        free(entry->headers);
        entry->headers = NULL;
      }
      if (entry->location != NULL) {
        free(entry->location);
        entry->location = NULL;
      }
      free(entry);
    }
    *pentry = NULL;
  }
}

PT_Element PT_ReadIndex(PT_Indexes indexes, const char *url, int flags) {
  if (indexes != NULL) {
    intptr_t index_id;

    if (strncmp(url, "http://", 7) == 0)
      url += 7;
    if (coucal_read(indexes->cil, url, &index_id)) {
      if (index_id >= 0 && index_id <= indexes->index_size) {
        PT_Element item = PT_ReadCache(indexes->index[index_id], url, flags);

        if (item != NULL) {
          item->indexId = (int) index_id;
          return item;
        }
      } else {
        proxytrack_print_log(CRITICAL,
                             "PT_ReadCache:Corrupted central index locator");
      }
    }
  }
  return NULL;
}

int PT_LookupIndex(PT_Indexes indexes, const char *url) {
  if (indexes != NULL) {
    intptr_t index_id;

    if (strncmp(url, "http://", 7) == 0)
      url += 7;
    if (coucal_read(indexes->cil, url, &index_id)) {
      if (index_id >= 0 && index_id <= indexes->index_size) {
        return 1;
      } else {
        proxytrack_print_log(CRITICAL,
                             "PT_ReadCache:Corrupted central index locator");
      }
    }
  }
  return 0;
}

time_t PT_GetTimeIndex(PT_Indexes indexes) {
  if (indexes != NULL && indexes->index_size > 0) {
    int i;
    time_t maxt = indexes->index[0]->slots.common.timestamp;

    for(i = 1; i < indexes->index_size; i++) {
      const time_t currt = indexes->index[i]->slots.common.timestamp;

      if (currt > maxt) {
        maxt = currt;
      }
    }
    return maxt;
  }
  return (time_t) - 1;
}

PT_Index PT_GetIndex(PT_Indexes indexes, int indexId) {
  if (indexes != NULL && indexId >= 0 && indexId < indexes->index_size) {
    return indexes->index[indexId];
  }
  return NULL;
}

PT_Element PT_ElementNew(void) {
  PT_Element r = NULL;

  if ((r = calloc(sizeof(_PT_Element), 1)) == NULL)
    return NULL;
  r->statuscode = STATUSCODE_INVALID;
  r->indexId = -1;
  return r;
}

PT_Element PT_ReadCache(PT_Index index, const char *url, int flags) {
  if (index != NULL && SAFE_INDEX(index)) {
    return _IndexFuncts[index->type].PT_ReadCache(index, url, flags);
  }
  return NULL;
}

static PT_Element PT_ReadCache__New(PT_Index index, const char *url, int flags) {
  PT_Element retCode;

  MutexLock(&index->slots.formatNew.zFileLock);
  {
    retCode = PT_ReadCache__New_u(index, url, flags);
  }
  MutexUnlock(&index->slots.formatNew.zFileLock);
  return retCode;
}

/* ------------------------------------------------------------ */
/* New HTTrack cache (new.zip) format                           */
/* ------------------------------------------------------------ */

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
#define ZIP_READFIELD_STRING(line, value, refline, refvalue) do { \
  if (line[0] != '\0' && strfield2(line, refline)) { \
    strcpy(refvalue, value); \
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

int PT_LoadCache__New(PT_Index index_, const char *filename) {
  if (index_ != NULL && filename != NULL) {
    PT_Index__New index = &index_->slots.formatNew;
    unzFile zFile = index->zFile = unzOpen(filename);

    index->timestamp = file_timestamp(filename);
    MutexInit(&index->zFileLock);

    // Opened ?
    if (zFile != NULL) {
      const char *abpath;
      int slashes;
      coucal hashtable = index->hash;

      /* Compute base path for this index - the filename MUST be absolute! */
      for(slashes = 2, abpath = filename + (int) strlen(filename) - 1;
          abpath > filename && ((*abpath != '/' && *abpath != '\\')
                                || --slashes > 0);
          abpath--) ;
      index->path[0] = '\0';
      if (slashes == 0 && *abpath != 0) {
        int i;

        strncat(index->path, filename, (int) (abpath - filename) + 1);
        for(i = 0; index->path[i] != 0; i++) {
          if (index->path[i] == '\\') {
            index->path[i] = '/';
          }
        }
      }

      /* Ready directory entries */
      if (unzGoToFirstFile(zFile) == Z_OK) {
        char comment[128];
        char filename[HTS_URLMAXSIZE * 4];
        int entries = 0;
        int firstSeen = 0;

        memset(comment, 0, sizeof(comment));    // for truncated reads
        do {
          int readSizeHeader = 0;

          filename[0] = '\0';
          comment[0] = '\0';
          if (unzOpenCurrentFile(zFile) == Z_OK) {
            if ((readSizeHeader =
                 unzGetLocalExtrafield(zFile, comment, sizeof(comment) - 2)) > 0
                && unzGetCurrentFileInfo(zFile, NULL, filename,
                                         sizeof(filename) - 2, NULL, 0, NULL,
                                         0) == Z_OK) {
              long int pos = (long int) unzGetOffset(zFile);

              assertf(readSizeHeader < sizeof(comment));
              comment[readSizeHeader] = '\0';
              entries++;
              if (pos > 0) {
                int dataincache = 0;    // data in cache ?
                char *filenameIndex = filename;

                if (strncmp(filenameIndex, "http://", 7) == 0) {
                  filenameIndex += 7;
                }
                if (comment[0] != '\0') {
                  int maxLine = 2;
                  char *a = comment;

                  while(*a && maxLine-- > 0) {  // parse only few first lines
                    char line[1024];

                    line[0] = '\0';
                    a += binput(a, line, sizeof(line) - 2);
                    if (strncmp(line, "X-In-Cache:", 11) == 0) {
                      if (strcmp(line, "X-In-Cache: 1") == 0) {
                        dataincache = 1;
                      } else {
                        dataincache = 0;
                      }
                      break;
                    }
                  }
                }
                if (dataincache)
                  coucal_add(hashtable, filenameIndex, pos);
                else
                  coucal_add(hashtable, filenameIndex, -pos);

                /* First link as starting URL */
                if (!firstSeen) {
                  if (strstr(filenameIndex, "/robots.txt") == NULL) {
                    firstSeen = 1;
                    if (!link_has_authority(filenameIndex))
                      strcat(index->startUrl, "http://");
                    strcat(index->startUrl, filenameIndex);
                  }
                }
              } else {
                fprintf(stderr, "Corrupted cache meta entry #%d" LF,
                        (int) entries);
              }
            } else {
              fprintf(stderr, "Corrupted cache entry #%d" LF, (int) entries);
            }
            unzCloseCurrentFile(zFile);
          } else {
            fprintf(stderr, "Corrupted cache entry #%d" LF, (int) entries);
          }
        } while(unzGoToNextFile(zFile) == Z_OK);
        return 1;
      } else {
        coucal_delete(&index->hash);
        index = NULL;
      }
    } else {
      index = NULL;
    }
  }
  return 0;
}

static PT_Element PT_ReadCache__New_u(PT_Index index_, const char *url,
                                      int flags) {
  PT_Index__New index = (PT_Index__New) & index_->slots.formatNew;
  char location_default[HTS_URLMAXSIZE * 2];
  char previous_save[HTS_URLMAXSIZE * 2];
  char previous_save_[HTS_URLMAXSIZE * 2];
  char catbuff[CATBUFF_SIZE];
  intptr_t hash_pos;
  int hash_pos_return;
  PT_Element r = NULL;

  if (index == NULL || index->hash == NULL || index->zFile == NULL
      || url == NULL || *url == 0)
    return NULL;
  if ((r = PT_ElementNew()) == NULL)
    return NULL;
  location_default[0] = '\0';
  previous_save[0] = previous_save_[0] = '\0';
  memset(r, 0, sizeof(_PT_Element));
  r->location = location_default;
  strcpy(r->location, "");
  if (strncmp(url, "http://", 7) == 0)
    url += 7;
  hash_pos_return = coucal_read(index->hash, url, &hash_pos);

  if (hash_pos_return) {
    uLong posInZip;

    if (hash_pos > 0) {
      posInZip = (uLong) hash_pos;
    } else {
      posInZip = (uLong) - hash_pos;
    }
    if (unzSetOffset(index->zFile, posInZip) == Z_OK) {
      /* Read header (Max 8KiB) */
      if (unzOpenCurrentFile(index->zFile) == Z_OK) {
        char headerBuff[8192 + 2];
        int readSizeHeader;
        //int totalHeader = 0;
        int dataincache = 0;

        /* For BIG comments */
        headerBuff[0]
          = headerBuff[sizeof(headerBuff) - 1]
          = headerBuff[sizeof(headerBuff) - 2]
          = headerBuff[sizeof(headerBuff) - 3] = '\0';

        if ((readSizeHeader =
             unzGetLocalExtrafield(index->zFile, headerBuff,
                                   sizeof(headerBuff) - 2)) > 0) {
          int offset = 0;
          char line[HTS_URLMAXSIZE + 2];
          int lineEof = 0;

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
              ZIP_READFIELD_INT(line, value, "X-Statuscode", r->statuscode);
              ZIP_READFIELD_STRING(line, value, "X-StatusMessage", r->msg);     // msg
              ZIP_READFIELD_INT(line, value, "X-Size", r->size);        // size
              ZIP_READFIELD_STRING(line, value, "Content-Type", r->contenttype);        // contenttype
              ZIP_READFIELD_STRING(line, value, "X-Charset", r->charset);       // contenttype
              ZIP_READFIELD_STRING(line, value, "Last-Modified", r->lastmodified);      // last-modified
              ZIP_READFIELD_STRING(line, value, "Etag", r->etag);       // Etag
              ZIP_READFIELD_STRING(line, value, "Location", r->location);       // 'location' pour moved
              ZIP_READFIELD_STRING(line, value, "Content-Disposition", r->cdispo);      // Content-disposition
              //ZIP_READFIELD_STRING(line, value, "X-Addr", ..);            // Original address
              //ZIP_READFIELD_STRING(line, value, "X-Fil", ..);            // Original URI filename
              ZIP_READFIELD_STRING(line, value, "X-Save", previous_save_);      // Original save filename
              if (line[0] != '\0') {
                int len = r->headers ? ((int) strlen(r->headers)) : 0;
                int nlen =
                  (int) (strlen(line) + 2 + strlen(value) + sizeof("\r\n") + 1);
                r->headers = realloc(r->headers, len + nlen);
                r->headers[len] = '\0';
                strcat(r->headers, line);
                strcat(r->headers, ": ");
                strcat(r->headers, value);
                strcat(r->headers, "\r\n");
              }
            }
          } while(offset < readSizeHeader && !lineEof);
          //totalHeader = offset;

          /* Previous entry */
          if (previous_save_[0] != '\0') {
            int pathLen = (int) strlen(index->path);

            if (pathLen > 0 && strncmp(previous_save_, index->path, pathLen) == 0) {    // old (<3.40) buggy format
              strcpy(previous_save, previous_save_);
            }
            // relative ? (hack)
            else if (index->safeCache || (previous_save_[0] != '/'      // /home/foo/bar.gif
                                          && (!isalpha(previous_save_[0]) || previous_save_[1] != ':')) // c:/home/foo/bar.gif
              ) {
              index->safeCache = 1;
              sprintf(previous_save, "%s%s", index->path, previous_save_);
            }
            // bogus format (includes buggy absolute path)
            else {
              /* guess previous path */
              if (index->fixedPath == 0) {
                const char *start = jump_protocol_and_auth(url);
                const char *end = start ? strchr(start, '/') : NULL;
                int len = (int) (end - start);

                if (start != NULL && end != NULL && len > 0 && len < 128) {
                  char piece[128 + 2];
                  const char *where;

                  piece[0] = '\0';
                  strncat(piece, start, len);
                  if ((where = strstr(previous_save_, piece)) != NULL) {
                    index->fixedPath = (int) (where - previous_save_);  // offset to relative path
                  }
                }
              }
              if (index->fixedPath > 0) {
                int saveLen = (int) strlen(previous_save_);

                if (index->fixedPath < saveLen) {
                  sprintf(previous_save, "%s%s", index->path,
                          previous_save_ + index->fixedPath);
                } else {
                  sprintf(r->msg, "Bogus fixePath prefix for %s (prefixLen=%d)",
                          previous_save_, (int) index->fixedPath);
                  r->statuscode = STATUSCODE_INVALID;
                }
              } else {
                sprintf(previous_save, "%s%s", index->path, previous_save_);
              }
            }
          }

          /* Complete fields */
          r->adr = NULL;
          if (r->statuscode != STATUSCODE_INVALID) {    /* Can continue */
            int ok = 0;

            // Court-circuit:
            // Peut-on stocker le fichier directement sur disque?
            if (ok) {
              if (r->msg[0] == '\0') {
                strcpy(r->msg, "Cache Read Error : Unexpected error");
              }
            } else {            // lire en mémoire

              if (!dataincache) {
                /* Read in memory from cache */
                if (flags & FETCH_BODY) {
                  if (strnotempty(previous_save)) {
                    FILE *fp = fopen(file_convert(catbuff, sizeof(catbuff), previous_save), "rb");

                    if (fp != NULL) {
                      r->adr = (char *) malloc(r->size + 4);
                      if (r->adr != NULL) {
                        if (r->size > 0
                            && fread(r->adr, 1, r->size, fp) != r->size) {
                          int last_errno = errno;

                          r->statuscode = STATUSCODE_INVALID;
                          sprintf(r->msg, "Read error in cache disk data: %s",
                                  strerror(last_errno));
                        }
                      } else {
                        r->statuscode = STATUSCODE_INVALID;
                        strcpy(r->msg,
                               "Read error (memory exhausted) from cache");
                      }
                      fclose(fp);
                    } else {
                      r->statuscode = STATUSCODE_INVALID;
                      sprintf(r->msg, "Read error (can't open '%s') from cache",
                              file_convert(catbuff, sizeof(catbuff), previous_save));
                    }
                  } else {
                    r->statuscode = STATUSCODE_INVALID;
                    strcpy(r->msg, "Cached file name is invalid");
                  }
                }
              } else {
                // lire fichier (d'un coup)
                if (flags & FETCH_BODY) {
                  r->adr = (char *) malloc(r->size + 1);
                  if (r->adr != NULL) {
                    if (unzReadCurrentFile(index->zFile, r->adr, (unsigned int) r->size) != r->size) {  // erreur
                      free(r->adr);
                      r->adr = NULL;
                      r->statuscode = STATUSCODE_INVALID;
                      strcpy(r->msg, "Cache Read Error : Read Data");
                    } else
                      *(r->adr + r->size) = '\0';
                    //printf(">%s status %d\n",back[p].r->contenttype,back[p].r->statuscode);
                  } else {      // erreur
                    r->statuscode = STATUSCODE_INVALID;
                    strcpy(r->msg, "Cache Memory Error");
                  }
                }
              }
            }
          }                     // si save==null, ne rien charger (juste en tête)
        } else {
          r->statuscode = STATUSCODE_INVALID;
          strcpy(r->msg, "Cache Read Error : Read Header Data");
        }
        unzCloseCurrentFile(index->zFile);
      } else {
        r->statuscode = STATUSCODE_INVALID;
        strcpy(r->msg, "Cache Read Error : Open File");
      }

    } else {
      r->statuscode = STATUSCODE_INVALID;
      strcpy(r->msg, "Cache Read Error : Bad Offset");
    }
  } else {
    r->statuscode = STATUSCODE_INVALID;
    strcpy(r->msg, "File Cache Entry Not Found");
  }
  if (r->location[0] != '\0') {
    r->location = strdup(r->location);
  } else {
    r->location = NULL;
  }
  return r;
}

static int PT_SaveCache__New_Fun(void *arg, const char *url, PT_Element element) {
  zipFile zFileOut = (zipFile) arg;
  char headers[8192];
  int headersSize;
  zip_fileinfo fi;
  int zErr;
  const char *url_adr = "";
  const char *url_fil = "";

  headers[0] = '\0';
  headersSize = 0;

  /* Fields */
  headers[0] = '\0';
  headersSize = 0;
  /* */
  {
    const char *message;

    if (strlen(element->msg) < 32) {
      message = element->msg;
    } else {
      message = "(See X-StatusMessage)";
    }
    /* 64 characters MAX for first line */
    sprintf(headers + headersSize, "HTTP/1.%c %d %s\r\n", '1',
            element->statuscode, message);
  }
  headersSize += (int) strlen(headers + headersSize);

  /* Second line MUST ALWAYS be X-In-Cache */
  ZIP_FIELD_INT_FORCE(headers, headersSize, "X-In-Cache", 1);
  ZIP_FIELD_INT(headers, headersSize, "X-StatusCode", element->statuscode);
  ZIP_FIELD_STRING(headers, headersSize, "X-StatusMessage", element->msg);
  ZIP_FIELD_INT(headers, headersSize, "X-Size", element->size); // size
  ZIP_FIELD_STRING(headers, headersSize, "Content-Type", element->contenttype); // contenttype
  ZIP_FIELD_STRING(headers, headersSize, "X-Charset", element->charset);        // contenttype
  ZIP_FIELD_STRING(headers, headersSize, "Last-Modified", element->lastmodified);       // last-modified
  ZIP_FIELD_STRING(headers, headersSize, "Etag", element->etag);        // Etag
  ZIP_FIELD_STRING(headers, headersSize, "Location", element->location);        // 'location' pour moved
  ZIP_FIELD_STRING(headers, headersSize, "Content-Disposition", element->cdispo);       // Content-disposition
  ZIP_FIELD_STRING(headers, headersSize, "X-Addr", url_adr);    // Original address
  ZIP_FIELD_STRING(headers, headersSize, "X-Fil", url_fil);     // Original URI filename
  ZIP_FIELD_STRING(headers, headersSize, "X-Save", ""); // Original save filename

  /* Time */
  memset(&fi, 0, sizeof(fi));
  if (element->lastmodified[0] != '\0') {
    struct tm buffer;
    struct tm *tm_s = convert_time_rfc822(&buffer, element->lastmodified);

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
  if ((zErr = zipOpenNewFileInZip(zFileOut, url, &fi,
                                  /* 
                                     Store headers in realtime in the local file directory as extra field
                                     In case of crash, we'll be able to recover the whole ZIP file by rescanning it
                                   */
                                  headers, (uInt) strlen(headers), NULL, 0, NULL,       /* comment */
                                  Z_DEFLATED, Z_DEFAULT_COMPRESSION)) != Z_OK) {
    assertf(! "zip_zipOpenNewFileInZip_failed");
  }

  /* Write data in cache */
  if (element->size > 0 && element->adr != NULL) {
    if ((zErr =
         zipWriteInFileInZip(zFileOut, element->adr,
                             (int) element->size)) != Z_OK) {
      assertf(! "zip_zipWriteInFileInZip_failed");
    }
  }

  /* Close */
  if ((zErr = zipCloseFileInZip(zFileOut)) != Z_OK) {
    assertf(! "zip_zipCloseFileInZip_failed");
  }

  /* Flush */
  if ((zErr = zipFlush(zFileOut)) != 0) {
    assertf(! "zip_zipFlush_failed");
  }

  return 0;
}

static int PT_SaveCache__New(PT_Indexes indexes, const char *filename) {
  zipFile zFileOut = zipOpen(filename, 0);

  if (zFileOut != NULL) {
    int ret = PT_EnumCache(indexes, PT_SaveCache__New_Fun, (void *) zFileOut);

    zipClose(zFileOut,
             "Created by HTTrack Website Copier/ProxyTrack "
             PROXYTRACK_VERSION);
    zFileOut = NULL;
    if (ret != 0)
      (void) unlink(filename);
    return ret;
  }
  return -1;
}

/* ------------------------------------------------------------ */
/* Old HTTrack cache (dat/ndx) format                           */
/* ------------------------------------------------------------ */

static int cache_brstr(char *adr, char *s) {
  int i;
  int off;
  char buff[256 + 1];

  off = binput(adr, buff, 256);
  adr += off;
  sscanf(buff, "%d", &i);
  if (i > 0)
    strncpy(s, adr, i);
  *(s + i) = '\0';
  off += i;
  return off;
}

static void cache_rstr(FILE * fp, char *s) {
  INTsys i;
  char buff[256 + 4];

  linput(fp, buff, 256);
  sscanf(buff, INTsysP, &i);
  if (i < 0 || i > 32768)       /* error, something nasty happened */
    i = 0;
  if (i > 0) {
    if ((int) fread(s, 1, i, fp) != i) {
      assertf(! "fread_cache_failed");
    }
  }
  *(s + i) = '\0';
}

static char *cache_rstr_addr(FILE * fp) {
  INTsys i;
  char *addr = NULL;
  char buff[256 + 4];

  linput(fp, buff, 256);
  sscanf(buff, "%d", &i);
  if (i < 0 || i > 32768)       /* error, something nasty happened */
    i = 0;
  if (i > 0) {
    addr = malloc(i + 1);
    if (addr != NULL) {
      if ((int) fread(addr, 1, i, fp) != i) {
        assertf(! "fread_cache_failed");
      }
      *(addr + i) = '\0';
    }
  }
  return addr;
}

static void cache_rint(FILE * fp, int *i) {
  char s[256];

  cache_rstr(fp, s);
  sscanf(s, "%d", i);
}

static void cache_rLLint(FILE * fp, unsigned long *i) {
  int l;
  char s[256];

  cache_rstr(fp, s);
  sscanf(s, "%d", &l);
  *i = (unsigned long) l;
}

static int PT_LoadCache__Old(PT_Index index_, const char *filename) {
  if (index_ != NULL && filename != NULL) {
    char *pos = strrchr(filename, '.');
    PT_Index__Old cache = &index_->slots.formatOld;
    long int ndxSize;

    cache->filenameDat[0] = '\0';
    cache->filenameNdx[0] = '\0';
    cache->path[0] = '\0';

    {
      PT_Index__Old index = cache;
      const char *abpath;
      int slashes;

      /* -------------------- COPY OF THE __New() CODE -------------------- */
      /* Compute base path for this index - the filename MUST be absolute! */
      for(slashes = 2, abpath = filename + (int) strlen(filename) - 1;
          abpath > filename && ((*abpath != '/' && *abpath != '\\')
                                || --slashes > 0);
          abpath--) ;
      index->path[0] = '\0';
      if (slashes == 0 && *abpath != 0) {
        int i;

        strncat(index->path, filename, (int) (abpath - filename) + 1);
        for(i = 0; index->path[i] != 0; i++) {
          if (index->path[i] == '\\') {
            index->path[i] = '/';
          }
        }
      }
      /* -------------------- END OF COPY OF THE __New() CODE -------------------- */
    }

    /* Index/data filenames */
    if (pos != NULL) {
      int nLen = (int) (pos - filename);

      strncat(cache->filenameDat, filename, nLen);
      strncat(cache->filenameNdx, filename, nLen);
      strcat(cache->filenameDat, ".dat");
      strcat(cache->filenameNdx, ".ndx");
    }
    ndxSize = filesize(cache->filenameNdx);
    cache->timestamp = file_timestamp(cache->filenameDat);
    cache->dat = fopen(cache->filenameDat, "rb");
    cache->ndx = fopen(cache->filenameNdx, "rb");
    if (cache->dat != NULL && cache->ndx != NULL && ndxSize > 0) {
      char *use = malloc(ndxSize + 1);

      if (fread(use, 1, ndxSize, cache->ndx) == ndxSize) {
        char firstline[256];
        char *a = use;

        use[ndxSize] = '\0';
        a += cache_brstr(a, firstline);
        if (strncmp(firstline, "CACHE-", 6) == 0) {     // Nouvelle version du cache
          if (strncmp(firstline, "CACHE-1.", 8) == 0) { // Version 1.1x
            cache->version = (int) (firstline[8] - '0');        // cache 1.x
            if (cache->version <= 5) {
              a += cache_brstr(a, firstline);
              strcpy(cache->lastmodified, firstline);
            } else {
              // fprintf(opt->errlog,"Cache: version 1.%d not supported, ignoring current cache"LF,cache->version);
              fclose(cache->dat);
              cache->dat = NULL;
              free(use);
              use = NULL;
            }
          } else {              // non supporté
            // fspc(opt->errlog,"error"); fprintf(opt->errlog,"Cache: %s not supported, ignoring current cache"LF,firstline);
            fclose(cache->dat);
            cache->dat = NULL;
            free(use);
            use = NULL;
          }
          /* */
        } else {                // Vieille version du cache
          /* */
          // hts_log_print(opt, LOG_WARNING, "Cache: importing old cache format");
          cache->version = 0;   // cache 1.0
          strcpy(cache->lastmodified, firstline);
        }

        /* Create hash table for the cache (MUCH FASTER!) */
        if (use) {
          char line[HTS_URLMAXSIZE * 2];
          char linepos[256];
          int pos;
          int firstSeen = 0;

          while((a != NULL) && (a < (use + ndxSize))) {
            a = strchr(a + 1, '\n');    /* start of line */
            if (a) {
              a++;
              /* read "host/file" */
              a += binput(a, line, HTS_URLMAXSIZE);
              a += binput(a, line + strlen(line), HTS_URLMAXSIZE);
              /* read position */
              a += binput(a, linepos, 200);
              sscanf(linepos, "%d", &pos);

              /* Add entry */
              coucal_add(cache->hash, line, pos);

              /* First link as starting URL */
              if (!firstSeen) {
                if (strstr(line, "/robots.txt") == NULL) {
                  PT_Index__Old index = cache;

                  firstSeen = 1;
                  if (!link_has_authority(line))
                    strcat(index->startUrl, "http://");
                  strcat(index->startUrl, line);
                }
              }

            }
          }
          /* Not needed anymore! */
          free(use);
          use = NULL;
          return 1;
        }
      }
    }
  }
  return 0;
}

static String DecodeUrl(const char *url) {
  int i;
  String s = STRING_EMPTY;

  StringClear(s);
  for(i = 0; url[i] != '\0'; i++) {
    if (url[i] == '+') {
      StringAddchar(s, ' ');
    } else if (url[i] == '%') {
      if (url[i + 1] == '%') {
        StringAddchar(s, '%');
        i++;
      } else if (url[i + 1] != 0 && url[i + 2] != 0) {
        char tmp[3];
        int codepoint = 0;

        tmp[0] = url[i + 1];
        tmp[1] = url[i + 2];
        tmp[2] = 0;
        if (sscanf(tmp, "%x", &codepoint) == 1) {
          StringAddchar(s, (char) codepoint);
        }
        i += 2;
      }
    } else {
      StringAddchar(s, url[i]);
    }
  }
  return s;
}

static PT_Element PT_ReadCache__Old(PT_Index index, const char *url, int flags) {
  PT_Element retCode;

  MutexLock(&index->slots.formatOld.fileLock);
  {
    retCode = PT_ReadCache__Old_u(index, url, flags);
  }
  MutexUnlock(&index->slots.formatOld.fileLock);
  return retCode;
}

static PT_Element PT_ReadCache__Old_u(PT_Index index_, const char *url,
                                      int flags) {
  PT_Index__Old cache = (PT_Index__Old) & index_->slots.formatOld;
  intptr_t hash_pos;
  int hash_pos_return;
  char location_default[HTS_URLMAXSIZE * 2];
  char previous_save[HTS_URLMAXSIZE * 2];
  char previous_save_[HTS_URLMAXSIZE * 2];
  PT_Element r;
  int ok = 0;

  if (cache == NULL || cache->hash == NULL || url == NULL || *url == 0)
    return NULL;
  if ((r = PT_ElementNew()) == NULL)
    return NULL;
  location_default[0] = '\0';
  previous_save[0] = previous_save_[0] = '\0';
  memset(r, 0, sizeof(_PT_Element));
  r->location = location_default;
  strcpy(r->location, "");
  if (strncmp(url, "http://", 7) == 0)
    url += 7;
  hash_pos_return = coucal_read(cache->hash, url, &hash_pos);

  if (hash_pos_return) {
    int pos = (int) hash_pos;   /* simply */

    if (fseek(cache->dat, (pos > 0) ? pos : (-pos), SEEK_SET) == 0) {
      /* Importer cache1.0 */
      if (cache->version == 0) {
        OLD_htsblk old_r;

        if (fread((char *) &old_r, 1, sizeof(old_r), cache->dat) == sizeof(old_r)) {    // lire tout (y compris statuscode etc)
          int i;
          String urlDecoded;

          r->statuscode = old_r.statuscode;
          r->size = old_r.size; // taille fichier
          strcpy(r->msg, old_r.msg);
          strcpy(r->contenttype, old_r.contenttype);

          /* Guess the destination filename.. this sucks, because this method is not reliable.
             Yes, the old 1.0 cache format was *that* bogus. /rx */
#define FORBIDDEN_CHAR(c) (c == '~' \
	|| c == '\\' \
	|| c == ':' \
	|| c == '*' \
	|| c == '?' \
	|| c == '\"' \
	|| c == '<' \
	|| c == '>' \
	|| c == '|' \
	|| c == '@' \
	|| ((unsigned char) c ) <= 31 \
	|| ((unsigned char) c ) == 127 \
	)
          urlDecoded = DecodeUrl(jump_protocol_and_auth(url));
          strcpy(previous_save_, StringBuff(urlDecoded));
          StringFree(urlDecoded);
          for(i = 0; previous_save_[i] != '\0' && previous_save_[i] != '?'; i++) {
            if (FORBIDDEN_CHAR(previous_save_[i])) {
              previous_save_[i] = '_';
            }
          }
          previous_save_[i] = '\0';
#undef FORBIDDEN_CHAR
          ok = 1;               /* import  ok */
        }
        /* */
        /* Cache 1.1 */
      } else {
        char check[256];
        unsigned long size_read;
        unsigned long int size_;

        check[0] = '\0';
        //
        cache_rint(cache->dat, &r->statuscode);
        cache_rLLint(cache->dat, &size_);
        r->size = (size_t) size_;
        cache_rstr(cache->dat, r->msg);
        cache_rstr(cache->dat, r->contenttype);
        if (cache->version >= 3)
          cache_rstr(cache->dat, r->charset);
        cache_rstr(cache->dat, r->lastmodified);
        cache_rstr(cache->dat, r->etag);
        cache_rstr(cache->dat, r->location);
        if (cache->version >= 2)
          cache_rstr(cache->dat, r->cdispo);
        if (cache->version >= 4) {
          cache_rstr(cache->dat, previous_save_);       // adr
          cache_rstr(cache->dat, previous_save_);       // fil
          previous_save[0] = '\0';
          cache_rstr(cache->dat, previous_save_);       // save
        }
        if (cache->version >= 5) {
          r->headers = cache_rstr_addr(cache->dat);
        }
        //
        cache_rstr(cache->dat, check);
        if (strcmp(check, "HTS") == 0) {        /* intégrité OK */
          ok = 1;
        }
        cache_rLLint(cache->dat, &size_read);   /* lire size pour être sûr de la taille déclarée (réécrire) */
        if (size_read > 0) {    /* si inscrite ici */
          r->size = size_read;
        } else {                /* pas de données directement dans le cache, fichier présent? */
          r->size = 0;
        }
      }

      /* Check destination filename */

      {
        PT_Index__Old index = cache;

        /* -------------------- COPY OF THE __New() CODE -------------------- */
        if (previous_save_[0] != '\0') {
          int pathLen = (int) strlen(index->path);

          if (pathLen > 0 && strncmp(previous_save_, index->path, pathLen) == 0) {      // old (<3.40) buggy format
            strcpy(previous_save, previous_save_);
          }
          // relative ? (hack)
          else if (index->safeCache || (previous_save_[0] != '/'        // /home/foo/bar.gif
                                        && (!isalpha(previous_save_[0]) || previous_save_[1] != ':'))   // c:/home/foo/bar.gif
            ) {
            index->safeCache = 1;
            sprintf(previous_save, "%s%s", index->path, previous_save_);
          }
          // bogus format (includes buggy absolute path)
          else {
            /* guess previous path */
            if (index->fixedPath == 0) {
              const char *start = jump_protocol_and_auth(url);
              const char *end = start ? strchr(start, '/') : NULL;
              int len = (int) (end - start);

              if (start != NULL && end != NULL && len > 0 && len < 128) {
                char piece[128 + 2];
                const char *where;

                piece[0] = '\0';
                strncat(piece, start, len);
                if ((where = strstr(previous_save_, piece)) != NULL) {
                  index->fixedPath = (int) (where - previous_save_);    // offset to relative path
                }
              }
            }
            if (index->fixedPath > 0) {
              int saveLen = (int) strlen(previous_save_);

              if (index->fixedPath < saveLen) {
                sprintf(previous_save, "%s%s", index->path,
                        previous_save_ + index->fixedPath);
              } else {
                sprintf(r->msg, "Bogus fixePath prefix for %s (prefixLen=%d)",
                        previous_save_, (int) index->fixedPath);
                r->statuscode = STATUSCODE_INVALID;
              }
            } else {
              sprintf(previous_save, "%s%s", index->path, previous_save_);
            }
          }
        }
        /* -------------------- END OF COPY OF THE __New() CODE -------------------- */
      }

      /* Read data */
      if (ok) {
        r->adr = NULL;
        if ((r->statuscode >= 0) && (r->statuscode <= 999)) {
          r->adr = NULL;
          if (pos < 0) {
            if (flags & FETCH_BODY) {
              FILE *fp = fopen(previous_save, "rb");

              if (fp != NULL) {
                r->adr = (char *) malloc(r->size + 1);
                if (r->adr != NULL) {
                  if (r->size > 0 && fread(r->adr, 1, r->size, fp) != r->size) {
                    r->statuscode = STATUSCODE_INVALID;
                    strcpy(r->msg, "Read error in cache disk data");
                  }
                  r->adr[r->size] = '\0';
                } else {
                  r->statuscode = STATUSCODE_INVALID;
                  strcpy(r->msg, "Read error (memory exhausted) from cache");
                }
                fclose(fp);
              } else {
                r->statuscode = STATUSCODE_INVALID;
                strcpy(r->msg, "Previous cache file not found (2)");
              }
            }
          } else {
            // lire fichier (d'un coup)
            if (flags & FETCH_BODY) {
              r->adr = (char *) malloc(r->size + 1);
              if (r->adr != NULL) {
                if (fread(r->adr, 1, r->size, cache->dat) != r->size) { // erreur
                  free(r->adr);
                  r->adr = NULL;
                  r->statuscode = STATUSCODE_INVALID;
                  strcpy(r->msg, "Cache Read Error : Read Data");
                } else
                  r->adr[r->size] = '\0';
              } else {          // erreur
                r->statuscode = STATUSCODE_INVALID;
                strcpy(r->msg, "Cache Memory Error");
              }
            }
          }
        } else {
          r->statuscode = STATUSCODE_INVALID;
          strcpy(r->msg, "Cache Read Error : Bad Data");
        }
      } else {                  // erreur
        r->statuscode = STATUSCODE_INVALID;
        strcpy(r->msg, "Cache Read Error : Read Header");
      }
    } else {
      r->statuscode = STATUSCODE_INVALID;
      strcpy(r->msg, "Cache Read Error : Seek Failed");
    }
  } else {
    r->statuscode = STATUSCODE_INVALID;
    strcpy(r->msg, "File Cache Entry Not Found");
  }
  if (r->location[0] != '\0') {
    r->location = strdup(r->location);
  } else {
    r->location = NULL;
  }
  return r;
}

static int PT_LookupCache__Old(PT_Index index, const char *url) {
  int retCode;

  MutexLock(&index->slots.formatOld.fileLock);
  {
    retCode = PT_LookupCache__Old_u(index, url);
  }
  MutexUnlock(&index->slots.formatOld.fileLock);
  return retCode;
}

static int PT_LookupCache__Old_u(PT_Index index_, const char *url) {
  if (index_ != NULL) {
    PT_Index__New cache = (PT_Index__New) & index_->slots.formatNew;

    if (cache == NULL || cache->hash == NULL || url == NULL || *url == 0)
      return 0;
    if (strncmp(url, "http://", 7) == 0)
      url += 7;
    if (coucal_read(cache->hash, url, NULL))
      return 1;
  }
  return 0;
}

/* ------------------------------------------------------------ */
/* Internet Archive Arc 1.0 (arc) format                        */
/* Xavier Roche (roche@httrack.com)                             */
/* Lars Clausen (lc@statsbiblioteket.dk)                        */
/* ------------------------------------------------------------ */

#define ARC_SP ' '

static const char *getArcField(const char *line, int pos) {
  int i;

  for(i = 0; line[i] != '\0' && pos > 0; i++) {
    if (line[i] == ARC_SP)
      pos--;
  }
  if (pos == 0)
    return &line[i];
  return NULL;
}

static char *copyArcField(const char *line, int npos, char *dest, int destMax) {
  const char *pos;

  if ((pos = getArcField(line, npos)) != NULL) {
    int i;

    for(i = 0; pos[i] != '\0' && pos[i] != ARC_SP && (--destMax) > 0; i++) {
      dest[i] = pos[i];
    }
    dest[i] = 0;
    return dest;
  }
  dest[0] = 0;
  return NULL;
}

static int getArcLength(const char *line) {
  const char *pos;

  if ((pos = getArcField(line, 9)) != NULL
      || (pos = getArcField(line, 4)) != NULL
      || (pos = getArcField(line, 2)) != NULL) {
    int length;

    if (sscanf(pos, "%d", &length) == 1) {
      return length;
    }
  }
  return -1;
}

static int skipArcNl(FILE * file) {
  if (fgetc(file) == 0x0a) {
    return 0;
  }
  return -1;
}

static int skipArcData(FILE * file, const char *line) {
  int jump = getArcLength(line);

  if (jump != -1) {
    if (fseek(file, jump, SEEK_CUR) == 0 /* && skipArcNl(file) == 0 */ ) {
      return 0;
    }
  }
  return -1;
}

static int getDigit(const char digit) {
  return (int) (digit - '0');
}

static int getDigit2(const char *const pos) {
  return getDigit(pos[0]) * 10 + getDigit(pos[1]);
}

static int getDigit4(const char *const pos) {
  return getDigit(pos[0]) * 1000 + getDigit(pos[1]) * 100 +
    getDigit(pos[2]) * 10 + getDigit(pos[3]);
}

static time_t getGMT(struct tm *tm) {   /* hey, time_t is local! */
  time_t t = mktime(tm);

  if (t != (time_t) - 1 && t != (time_t) 0) {
    /* BSD does not have static "timezone" declared */
#if (defined(BSD) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__FreeBSD_kernel__))
    time_t now = time(NULL);
    time_t timezone = -localtime(&now)->tm_gmtoff;
#endif
    return (time_t) (t - timezone);
  }
  return (time_t) - 1;
}

static time_t getArcTimestamp(const char *const line) {
  const char *pos;

  if ((pos = getArcField(line, 2)) != NULL) {
    int i;

    /* date == YYYYMMDDhhmmss (Greenwich Mean Time) */
    /* example: 20050405154029  */
    for(i = 0; pos[i] >= '0' && pos[i] <= '9'; i++) ;
    if (i == 14) {
      struct tm tm;

      memset(&tm, 0, sizeof(tm));
      tm.tm_year = getDigit4(pos + 0) - 1900;   /* current year minus 1900 */
      tm.tm_mon = getDigit2(pos + 4) - 1;       /* 0 – 11 */
      tm.tm_mday = getDigit2(pos + 6);  /* 1 – 31 */
      tm.tm_hour = getDigit2(pos + 8);  /* 0 – 23 */
      tm.tm_min = getDigit2(pos + 10);  /* 0 – 59 */
      tm.tm_sec = getDigit2(pos + 12);  /* 0 – 59 */
      tm.tm_isdst = 0;
      return getGMT(&tm);
    }
  }
  return (time_t) - 1;
}

static int readArcURLRecord(PT_Index__Arc index) {
  index->line[0] = '\0';
  if (linput(index->file, index->line, sizeof(index->line) - 1)) {
    return 0;
  }
  return -1;
}

#define str_begins(str, sstr) ( strncmp(str, sstr, sizeof(sstr) - 1) == 0 )
static int PT_CompatibleScheme(const char *url) {
  return (str_begins(url, "http:")
          || str_begins(url, "https:")
          || str_begins(url, "ftp:")
          || str_begins(url, "file:"));
}

int PT_LoadCache__Arc(PT_Index index_, const char *filename) {
  if (index_ != NULL && filename != NULL) {
    PT_Index__Arc index = &index_->slots.formatArc;

    index->timestamp = file_timestamp(filename);
    MutexInit(&index->fileLock);
    index->file = fopen(filename, "rb");

    // Opened ?
    if (index->file != NULL) {
      coucal hashtable = index->hash;

      if (readArcURLRecord(index) == 0) {
        int entries = 0;

        /* Read first line */
        if (strncmp(index->line, "filedesc://", sizeof("filedesc://") - 1) != 0) {
          fprintf(stderr, "Unexpected bad signature #%s" LF, index->line);
          fclose(index->file);
          index->file = NULL;
          return 0;
        }
        /* Timestamp */
        index->timestamp = getArcTimestamp(index->line);
        /* Skip first entry */
        if (skipArcData(index->file, index->line) != 0
            || skipArcNl(index->file) != 0) {
          fprintf(stderr, "Unexpected bad data offset size first entry" LF);
          fclose(index->file);
          index->file = NULL;
          return 0;
        }
        /* Read all meta-entries (not data) */
        while(!feof(index->file)) {
          unsigned long int fpos = ftell(index->file);

          if (skipArcNl(index->file) == 0 && readArcURLRecord(index) == 0) {
            int length = getArcLength(index->line);

            if (length >= 0) {
              const char *filenameIndex = copyArcField(index->line, 0,
                                                       index->filenameIndexBuff, sizeof(index->filenameIndexBuff) - 1); /* can not be NULL */

              if (strncmp(filenameIndex, "http://", 7) == 0) {
                filenameIndex += 7;
              }
              if (*filenameIndex != 0) {
                if (skipArcData(index->file, index->line) != 0) {
                  fprintf(stderr,
                          "Corrupted cache data entry #%d (truncated file?), aborting read"
                          LF, (int) entries);
                }
                /*fprintf(stdout, "adding %s [%d]\n", filenameIndex, (int)fpos); */
                if (PT_CompatibleScheme(index->filenameIndexBuff)) {
                  coucal_add(hashtable, filenameIndex, fpos);  /* position of meta-data */
                  entries++;
                }
              } else {
                fprintf(stderr, "Corrupted cache meta entry #%d" LF,
                        (int) entries);
              }
            } else {
              fprintf(stderr,
                      "Corrupted cache meta entry #%d, aborting read" LF,
                      (int) entries);
              break;
            }
          } else {
            break;
          }
        }

        /* OK */
        return 1;
      } else {
        fprintf(stderr, "Bad file (empty ?)" LF);
      }
    } else {
      fprintf(stderr, "Unable to open file" LF);
      index = NULL;
    }
  } else {
    fprintf(stderr, "Bad arguments" LF);
  }
  return 0;
}

#define HTTP_READFIELD_STRING(line, value, refline, refvalue) do { \
  if (line[0] != '\0' && strfield2(line, refline)) { \
    strcpy(refvalue, value); \
    line[0] = '\0'; \
	} \
} while(0)
#define HTTP_READFIELD_INT(line, value, refline, refvalue) do { \
  if (line[0] != '\0' && strfield2(line, refline)) { \
    int intval = 0; \
    sscanf(value, "%d", &intval); \
    (refvalue) = intval; \
    line[0] = '\0'; \
	} \
} while(0)

static PT_Element PT_ReadCache__Arc(PT_Index index, const char *url, int flags) {
  PT_Element retCode;

  MutexLock(&index->slots.formatArc.fileLock);
  {
    retCode = PT_ReadCache__Arc_u(index, url, flags);
  }
  MutexUnlock(&index->slots.formatArc.fileLock);
  return retCode;
}

static PT_Element PT_ReadCache__Arc_u(PT_Index index_, const char *url,
                                      int flags) {
  PT_Index__Arc index = (PT_Index__Arc) & index_->slots.formatArc;
  char location_default[HTS_URLMAXSIZE * 2];
  intptr_t hash_pos;
  int hash_pos_return;
  PT_Element r = NULL;

  if (index == NULL || index->hash == NULL || url == NULL || *url == 0)
    return NULL;
  if ((r = PT_ElementNew()) == NULL)
    return NULL;
  location_default[0] = '\0';
  memset(r, 0, sizeof(_PT_Element));
  r->location = location_default;
  strcpy(r->location, "");
  if (strncmp(url, "http://", 7) == 0)
    url += 7;
  hash_pos_return = coucal_read(index->hash, url, &hash_pos);

  if (hash_pos_return) {
    if (fseek(index->file, (long) hash_pos, SEEK_SET) == 0) {
      if (skipArcNl(index->file) == 0 && readArcURLRecord(index) == 0) {
        long int fposMeta = ftell(index->file);
        int dataLength = getArcLength(index->line);
        const char *pos;

        /* Read HTTP headers */
        /* HTTP/1.1 404 Not Found */
        if (linput(index->file, index->line, sizeof(index->line) - 1)) {
          if ((pos = getArcField(index->line, 1)) != NULL) {
            if (sscanf(pos, "%d", &r->statuscode) != 1) {
              r->statuscode = STATUSCODE_INVALID;
            }
          }
          if ((pos = getArcField(index->line, 2)) != NULL) {
            r->msg[0] = '\0';
            strncat(r->msg, pos, sizeof(pos) - 1);
          }
          while(linput(index->file, index->line, sizeof(index->line) - 1)
                && index->line[0] != '\0') {
            char *const line = index->line;
            char *value = strchr(line, ':');

            if (value != NULL) {
              *value = '\0';
              for(value++; *value == ' ' || *value == '\t'; value++) ;
              HTTP_READFIELD_INT(line, value, "Content-Length", r->size);       // size
              HTTP_READFIELD_STRING(line, value, "Content-Type", r->contenttype);       // contenttype
              HTTP_READFIELD_STRING(line, value, "Last-Modified", r->lastmodified);     // last-modified
              HTTP_READFIELD_STRING(line, value, "Etag", r->etag);      // Etag
              HTTP_READFIELD_STRING(line, value, "Location", r->location);      // 'location' pour moved
              HTTP_READFIELD_STRING(line, value, "Content-Disposition", r->cdispo);     // Content-disposition
              if (line[0] != '\0') {
                int len = r->headers ? ((int) strlen(r->headers)) : 0;
                int nlen =
                  (int) (strlen(line) + 2 + strlen(value) + sizeof("\r\n") + 1);
                r->headers = realloc(r->headers, len + nlen);
                r->headers[len] = '\0';
                strcat(r->headers, line);
                strcat(r->headers, ": ");
                strcat(r->headers, value);
                strcat(r->headers, "\r\n");
              }
            }
          }

          /* FIXME charset */
          if (r->contenttype[0] != '\0') {
            char *pos = strchr(r->contenttype, ';');

            if (pos != NULL) {
              /*char *chs = strchr(pos, "charset="); */
              /*HTTP_READFIELD_STRING(line, value, "X-Charset", r->charset); */
              *pos = 0;
              if ((pos = strchr(r->contenttype, ' ')) != NULL) {
                *pos = 0;
              }
            }
          }

          /* Read data */
          if (r->statuscode != STATUSCODE_INVALID) {    /* Can continue */
            if (flags & FETCH_BODY) {
              long int fposCurrent = ftell(index->file);
              long int metaSize = fposCurrent - fposMeta;
              long int fetchSize = (long int) r->size;

              if (fetchSize <= 0) {
                fetchSize = dataLength - metaSize;
              } else if (fetchSize > dataLength - metaSize) {
                r->statuscode = STATUSCODE_INVALID;
                strcpy(r->msg, "Cache Read Error : Truncated Data");
              }
              r->size = 0;
              if (r->statuscode != STATUSCODE_INVALID) {
                r->adr = (char *) malloc(fetchSize);
                if (r->adr != NULL) {
                  if (fetchSize > 0
                      && (r->size =
                          (int) fread(r->adr, 1, fetchSize,
                                      index->file)) != fetchSize) {
                    int last_errno = errno;

                    r->statuscode = STATUSCODE_INVALID;
                    sprintf(r->msg, "Read error in cache disk data: %s",
                            strerror(last_errno));
                  }
                } else {
                  r->statuscode = STATUSCODE_INVALID;
                  strcpy(r->msg, "Read error (memory exhausted) from cache");
                }
              }
            }
          }

        } else {
          r->statuscode = STATUSCODE_INVALID;
          strcpy(r->msg, "Cache Read Error : Read Header Error");
        }

      } else {
        r->statuscode = STATUSCODE_INVALID;
        strcpy(r->msg, "Cache Read Error : Read Header Error");
      }
    } else {
      r->statuscode = STATUSCODE_INVALID;
      strcpy(r->msg, "Cache Read Error : Seek Error");
    }

  } else {
    r->statuscode = STATUSCODE_INVALID;
    strcpy(r->msg, "File Cache Entry Not Found");
  }
  if (r->location[0] != '\0') {
    r->location = strdup(r->location);
  } else {
    r->location = NULL;
  }
  return r;
}

static int PT_LookupCache__Arc(PT_Index index, const char *url) {
  int retCode;

  MutexLock(&index->slots.formatArc.fileLock);
  {
    retCode = PT_LookupCache__Arc_u(index, url);
  }
  MutexUnlock(&index->slots.formatArc.fileLock);
  return retCode;
}

static int PT_LookupCache__Arc_u(PT_Index index_, const char *url) {
  if (index_ != NULL) {
    PT_Index__New cache = (PT_Index__New) & index_->slots.formatNew;

    if (cache == NULL || cache->hash == NULL || url == NULL || *url == 0)
      return 0;
    if (strncmp(url, "http://", 7) == 0)
      url += 7;
    if (coucal_read(cache->hash, url, NULL))
      return 1;
  }
  return 0;
}

typedef struct PT_SaveCache__Arc_t {
  PT_Indexes indexes;
  FILE *fp;
  time_t t;
  char filename[64];
  struct tm buff;
  char headers[8192];
  char md5[32 + 2];
} PT_SaveCache__Arc_t;

static int PT_SaveCache__Arc_Fun(void *arg, const char *url, PT_Element element) {
  PT_SaveCache__Arc_t *st = (PT_SaveCache__Arc_t *) arg;
  FILE *const fp = st->fp;
  struct tm *tm = convert_time_rfc822(&st->buff, element->lastmodified);
  int size_headers;

  sprintf(st->headers,
          "HTTP/1.0 %d %s" "\r\n" "X-Server: ProxyTrack " PROXYTRACK_VERSION
          "\r\n" "Content-type: %s%s%s%s" "\r\n" "Last-modified: %s" "\r\n"
          "Content-length: %d" "\r\n", element->statuscode, element->msg,
          /**/ element->contenttype,
          (element->charset[0] ? "; charset=\"" : ""),
          (element->charset[0] ? element->charset : ""),
          (element->charset[0] ? "\"" : ""), /**/ element->lastmodified,
          (int) element->size);
  if (element->location != NULL && element->location[0] != '\0') {
    sprintf(st->headers + strlen(st->headers), "Location: %s" "\r\n",
            element->location);
  }
  if (element->headers != NULL) {
    if (strlen(element->headers) <
        sizeof(st->headers) - strlen(element->headers) - 1) {
      strcat(st->headers, element->headers);
    }
  }
  strcat(st->headers, "\r\n");
  size_headers = (int) strlen(st->headers);

  /* doc == <nl><URL-record><nl><network_doc> */

  /* Format: URL IP date mime result checksum location offset filename length */
  if (element->adr != NULL) {
    domd5mem(element->adr, element->size, st->md5, 1);
  } else {
    strcpy(st->md5, "-");
  }
  fprintf(fp,
          /* nl */
          "\n"
          /* URL-record */
          "%s%s %s %04d%02d%02d%02d%02d%02d %s %d %s %s %ld %s %ld"
          /* nl */
          "\n",
          /* args */
          (link_has_authority(url) ? "" : "http://"), url, "0.0.0.0",
          tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour,
          tm->tm_min, tm->tm_sec, element->contenttype, element->statuscode,
          st->md5, (element->location ? element->location : "-"),
          (long int) ftell(fp), st->filename,
          (long int) (size_headers + element->size));
  /* network_doc */
  if (fwrite(st->headers, 1, size_headers, fp) != size_headers
      || (element->size > 0
          && fwrite(element->adr, 1, element->size, fp) != element->size)
    ) {
    return 1;                   /* Error */
  }

  return 0;
}

static int PT_SaveCache__Arc(PT_Indexes indexes, const char *filename) {
  FILE *fp = fopen(filename, "wb");

  if (fp != NULL) {
    PT_SaveCache__Arc_t st;
    int ret;
    time_t t = PT_GetTimeIndex(indexes);
    struct tm tm = PT_GetTime(t);

    /* version-2-block ==
       filedesc://<path><sp><ip_address><sp><date><sp>text/plain<sp>200<sp>-<sp>-<sp>0<sp><filename><sp><length><nl>
       2<sp><reserved><sp><origin-code><nl>
       URL<sp>IP-address<sp>Archive-date<sp>Content-type<sp>Result-code<sp>Checksum<sp>Location<sp> Offset<sp>Filename<sp>Archive-length<nl>
       <nl> */
    const char *prefix =
      "2 0 HTTrack Website Copier" "\n"
      "URL IP-address Archive-Date Content-Type Result-code Checksum Location Offset Filename Archive-length"
      "\n" "\n";
    sprintf(st.filename, "httrack_%d.arc", (int) t);
    fprintf(fp,
            "filedesc://%s 0.0.0.0 %04d%02d%02d%02d%02d%02d text/plain 200 - - 0 %s %d"
            "\n" "%s", st.filename, tm.tm_year + 1900, tm.tm_mon + 1,
            tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, st.filename,
            (int) strlen(prefix), prefix);
    st.fp = fp;
    st.indexes = indexes;
    st.t = t;
    ret = PT_EnumCache(indexes, PT_SaveCache__Arc_Fun, (void *) &st);
    fclose(fp);
    if (ret != 0)
      (void) unlink(filename);
    return ret;
  }
  return -1;
}
