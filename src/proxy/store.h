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

Please visit our Website: http://www.httrack.com
*/

/* ------------------------------------------------------------ */
/* File: Cache manager for ProxyTrack                           */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef WEBHTTRACK_PROXYTRACK_STORE
#define WEBHTTRACK_PROXYTRACK_STORE

/* Includes */
#ifndef _WIN32
#include <pthread.h>
#else
#include "windows.h"
#endif

/* Proxy */

typedef struct _PT_Index _PT_Index;
typedef struct _PT_Indexes _PT_Indexes;

typedef struct _PT_Index *PT_Index;
typedef struct _PT_Indexes *PT_Indexes;

typedef struct _PT_Cache _PT_Cache;
typedef struct _PT_Cache *PT_Cache;

typedef struct _PT_CacheItem _PT_CacheItem;
typedef struct _PT_CacheItem *PT_CacheItem;

typedef struct _PT_Element {
  int indexId;                  // index identifier, if suitable (!= -1)
  //
  int statuscode;               // status-code, -1=erreur, 200=OK,201=..etc (cf RFC1945)
  char *adr;                    // adresse du bloc de mémoire, NULL=vide
  char *headers;                // adresse des en têtes si présents (RFC822 format)
  size_t size;                  // taille fichier
  char msg[1024];               // error message ("\0"=undefined)
  char contenttype[64];         // content-type ("text/html" par exemple)
  char charset[64];             // charset ("iso-8859-1" par exemple)
  char *location;               // on copie dedans éventuellement la véritable 'location'
  char lastmodified[64];        // Last-Modified
  char etag[64];                // Etag
  char cdispo[256];             // Content-Disposition coupé
} _PT_Element;
typedef struct _PT_Element *PT_Element;

typedef enum PT_Fetch_Flags {
  FETCH_HEADERS,                // fetch headers
  FETCH_BODY                    // fetch body
} PT_Fetch_Flags;

/* Locking */
#ifdef _WIN32
typedef void *PT_Mutex;
#else
typedef pthread_mutex_t PT_Mutex;
#endif

void MutexInit(PT_Mutex * pMutex);
void MutexLock(PT_Mutex * pMutex);
void MutexUnlock(PT_Mutex * pMutex);
void MutexFree(PT_Mutex * pMutex);

/* Indexes */
PT_Indexes PT_New(void);
void PT_Delete(PT_Indexes index);
PT_Element PT_ReadIndex(PT_Indexes indexes, const char *url, int flags);
int PT_LookupIndex(PT_Indexes indexes, const char *url);
int PT_AddIndex(PT_Indexes index, const char *path);
int PT_RemoveIndex(PT_Indexes index, int indexId);
int PT_IndexMerge(PT_Indexes indexes, PT_Index * pindex);
PT_Index PT_GetIndex(PT_Indexes indexes, int indexId);
time_t PT_GetTimeIndex(PT_Indexes indexes);

/* Indexes list */
PT_Element PT_Index_HTML_BuildRootInfo(PT_Indexes indexes);
char **PT_Enumerate(PT_Indexes indexes, const char *url, int subtree);
void PT_Enumerate_Delete(char ***plist);
int PT_EnumCache(PT_Indexes indexes,
                 int (*callback) (void *, const char *url, PT_Element),
                 void *arg);
int PT_SaveCache(PT_Indexes indexes, const char *filename);

/* Index */
PT_Index PT_LoadCache(const char *filename);
void PT_Index_Delete(PT_Index * pindex);
PT_Element PT_ReadCache(PT_Index index, const char *url, int flags);
int PT_LookupCache(PT_Index index, const char *url);
time_t PT_Index_Timestamp(PT_Index index);

/* Elements*/
PT_Element PT_ElementNew(void);
void PT_Element_Delete(PT_Element * pentry);

#endif
