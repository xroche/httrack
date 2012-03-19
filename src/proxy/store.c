/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) Xavier Roche and other contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

Please visit our Website: http://www.httrack.com
*/

/* ------------------------------------------------------------ */
/* File: Cache manager for ProxyTrack                           */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Locking */
#ifdef _WIN32
#include <process.h>    /* _beginthread, _endthread */
#else
#include <pthread.h>
#endif

#include "htsglobal.h"

#define HTS_INTERNAL_BYTECODE
#include "htsinthash.h"
#undef HTS_INTERNAL_BYTECODE
#include "../minizip/mztools.h"

#include "htscore.h"
#include "htsback.h"

#include "store.h"
#include "proxystrings.h"
#include "proxytrack.h"

/* Unlocked functions */

static int PT_LookupCache__New_u(PT_Index index, const char* url);
static PT_Element PT_ReadCache__New_u(PT_Index index, const char* url, int flags);

static int PT_LookupCache__Old_u(PT_Index index, const char* url);
static PT_Element PT_ReadCache__Old_u(PT_Index index, const char* url, int flags);


/* Locking */

#ifdef _WIN32
void MutexInit(PT_Mutex *pMutex) {
	*pMutex = CreateMutex(NULL,FALSE,NULL);
}

void MutexLock(PT_Mutex *pMutex) {
	WaitForSingleObject(*pMutex, INFINITE);
}

void MutexUnlock(PT_Mutex *pMutex) {
	ReleaseMutex(*pMutex);
}

void MutexFree(PT_Mutex *pMutex) {
	CloseHandle(*pMutex);
	*pMutex = NULL;
}
#else
void MutexInit(PT_Mutex *pMutex) {
	(void) pthread_mutex_init(pMutex, 0);
}

void MutexLock(PT_Mutex *pMutex) {
	pthread_mutex_lock(pMutex);
}

void MutexUnlock(PT_Mutex *pMutex) {
	pthread_mutex_unlock(pMutex);
}

void MutexFree(PT_Mutex *pMutex) {
	pthread_mutex_destroy(pMutex);
}
#endif

/* Indexes */

typedef struct _PT_Index__New _PT_Index__New;
typedef struct _PT_Index__Old _PT_Index__Old;
typedef struct _PT_Index_Functions _PT_Index_Functions;

typedef struct _PT_Index__New *PT_Index__New;
typedef struct _PT_Index__Old *PT_Index__Old;
typedef struct _PT_Index_Functions *PT_Index_Functions;

enum {
	PT_CACHE_UNDEFINED = -1,
	PT_CACHE_MIN = 0,
	PT_CACHE__NEW = PT_CACHE_MIN,
	PT_CACHE__OLD,
	PT_CACHE_MAX = PT_CACHE__OLD
};

static int PT_LoadCache__New(PT_Index index, const char *filename);
static void PT_Index_Delete__New(PT_Index *pindex);
static PT_Element PT_ReadCache__New(PT_Index index, const char* url, int flags);
static int PT_LookupCache__New(PT_Index index, const char* url);
/**/
static int PT_LoadCache__Old(PT_Index index, const char *filename);
static void PT_Index_Delete__Old(PT_Index *pindex);
static PT_Element PT_ReadCache__Old(PT_Index index, const char* url, int flags);
static int PT_LookupCache__Old(PT_Index index, const char* url);

struct _PT_Index_Functions {
	int (*PT_LoadCache)(PT_Index index, const char *filename);
	void (*PT_Index_Delete)(PT_Index *pindex);
	PT_Element (*PT_ReadCache)(PT_Index index, const char* url, int flags);
	int (*PT_LookupCache)(PT_Index index, const char* url);
};

static _PT_Index_Functions _IndexFuncts[] = {
  { PT_LoadCache__New, PT_Index_Delete__New, PT_ReadCache__New, PT_LookupCache__New },
  { PT_LoadCache__Old, PT_Index_Delete__Old, PT_ReadCache__Old, PT_LookupCache__Old },
	{ NULL, NULL, NULL, NULL }
};

#define PT_INDEX_COMMON_STRUCTURE \
	time_t timestamp;								\
	inthash hash;										\
	char startUrl[1024]

struct _PT_Index__New {
	PT_INDEX_COMMON_STRUCTURE;
	char path[1024];		/* either empty, or must include ending / */
	int fixedPath;
	int safeCache;
	unzFile zFile;
	PT_Mutex zFileLock;
};

struct _PT_Index__Old {
	PT_INDEX_COMMON_STRUCTURE;
	char filenameDat[1024];
	char filenameNdx[1024];
	FILE *dat,*ndx;
	PT_Mutex fileLock;
	int version;
	char lastmodified[1024];
	char path[1024];		/* either empty, or must include ending / */
	int fixedPath;
	int safeCache;
};

struct _PT_Index {
	int type;
	union {
		_PT_Index__New formatNew;
		_PT_Index__Old formatOld;
		struct {
			PT_INDEX_COMMON_STRUCTURE;
		} common;
	} slots;
};

struct _PT_Indexes {
	inthash cil;
	struct _PT_Index **index;
	int index_size;
};

struct _PT_CacheItem {
	time_t lastUsed;
	size_t size;
	void* data;
};

struct _PT_Cache {
	inthash index;
	size_t maxSize;
	size_t totalSize;
	int count;
};

PT_Indexes PT_New() {
	PT_Indexes index = (PT_Indexes) calloc(sizeof(_PT_Indexes), 1);
	index->cil = inthash_new(127);
	index->index_size = 0;
	index->index = NULL;
	return index;
}

void PT_Delete(PT_Indexes index) {
	if (index != NULL) {
		inthash_delete(&index->cil);
		free(index);
	}
}

int PT_RemoveIndex(PT_Indexes index, int indexId) {
	return 0;
}

#define assertf(exp)

static int binput(char* buff,char* s,int max) {
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

static time_t file_timestamp(const char* file) {
  struct stat buf;
  if (stat(file, &buf) == 0) {
    time_t tt = buf.st_mtime;
		if (tt != (time_t) 0 && tt != (time_t) -1) {
			return tt;
		}
  }
  return (time_t) 0;
}

static int PT_Index_Check__(PT_Index index, const char* file, int line) {
	if (index == NULL)
		return 0;
	if (index->type >= PT_CACHE_MIN && index->type <= PT_CACHE_MAX)
		return 1;
	CRITICAL_("index corrupted in memory", file, line);
	return 0;
}
#define SAFE_INDEX(index) PT_Index_Check__(index, __FILE__, __LINE__)


/* ------------------------------------------------------------ */
/* Generic cache dispatch                                       */
/* ------------------------------------------------------------ */

void PT_Index_Delete(PT_Index *pindex) {
	if (pindex != NULL && (*pindex) != NULL) {
		PT_Index index = *pindex;
		if (SAFE_INDEX(index)) {
			_IndexFuncts[index->type].PT_Index_Delete(pindex);
		}
		free(index);
		*pindex = NULL;
	}
}

static void PT_Index_Delete__New(PT_Index *pindex) {
	if (pindex != NULL && (*pindex) != NULL) {
		PT_Index__New index = &(*pindex)->slots.formatNew;
		if (index->zFile != NULL) {
			unzClose(index->zFile);
			index->zFile = NULL;
		}
		if (index->hash != NULL) {
			inthash_delete(&index->hash);
			index->hash = NULL;
		}
		MutexFree(&index->zFileLock);
	}
}

static void PT_Index_Delete__Old(PT_Index *pindex) {
	if (pindex != NULL && (*pindex) != NULL) {
		PT_Index__Old index = &(*pindex)->slots.formatOld;
		if (index->dat != NULL) {
			fclose(index->dat);
		}
		if (index->ndx != NULL) {
			fclose(index->ndx);
		}
		if (index->hash != NULL) {
			inthash_delete(&index->hash);
			index->hash = NULL;
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
		StringStrcat(html, 
			"<html>"
			PROXYTRACK_COMMENT_HEADER
			DISABLE_IE_FRIENDLY_HTTP_ERROR_MESSAGES
			"<head>\r\n"
			"<title>ProxyTrack " PROXYTRACK_VERSION " Catalog</title>"
			"</head>\r\n"
			"<body>\r\n"
			"<h3>Available sites in this cache:</h3><br />"
			"<br />"
			);
		StringStrcat(html, "<ul>\r\n");
		for(i = 0 ; i < indexes->index_size ; i++) {
			if (indexes->index[i] != NULL
				&& indexes->index[i]->slots.common.startUrl[0] != '\0')
			{
				const char * url = indexes->index[i]->slots.common.startUrl;
				StringStrcat(html, "<li>\r\n");
				StringStrcat(html, "<a href=\"");
				StringStrcat(html, url);
				StringStrcat(html, "\">");
				StringStrcat(html, url);
				StringStrcat(html, "</a>\r\n");
				StringStrcat(html, "</li>\r\n");
			}
		}
		StringStrcat(html, "</ul>\r\n");
		StringStrcat(html, "</body></html>\r\n");
		elt->size = StringLength(html);
		elt->adr = StringAcquire(&html);
		elt->statuscode = 200;
		strcpy(elt->charset, "iso-8859-1");
		strcpy(elt->contenttype, "text/html");
		strcpy(elt->msg, "OK");
		StringFree(html);
		return elt;
	}
	return NULL;
}

static char* strchr_stop(char* str, char c, char stop) {
	for( ; *str != 0 && *str != stop && *str != c ; str++);
	if (*str == c)
		return str;
	return NULL;
}

char ** PT_Enumerate(PT_Indexes indexes, const char *url, int subtree) {
	// should be cached!
	if (indexes != NULL && indexes->cil != NULL) {
		unsigned int urlSize;
		String list = STRING_EMPTY;
		String listindexes = STRING_EMPTY;
		String subitem = STRING_EMPTY;
		unsigned int listCount = 0;
		struct_inthash_enum en = inthash_enum_new(indexes->cil);
		inthash_chain* chain;
		inthash hdupes = NULL;
		if (!subtree)
			hdupes= inthash_new(127);
		StringClear(list);
		StringClear(listindexes);
		StringClear(subitem);
		if (strncmp(url, "http://", 7) == 0)
			url += 7;
		urlSize = (unsigned int) strlen(url);
		while((chain = inthash_enum_next(&en))) {
			long int index = (long int)chain->value.intg;
			if (urlSize == 0 || strncmp(chain->name, url, urlSize) == 0) {
				if (index >= 0 && index < indexes->index_size) {
					char * item = chain->name + urlSize;
					if (*item == '/')
						item++;
					{
						char * pos = subtree ? 0 : strchr_stop(item, '/', '?');
						unsigned int len = pos ? (unsigned int)( pos - item ) : (unsigned int)strlen(item);
						if (len > 0 /* default document */ || *item == 0) {
							int isFolder = ( item[len] == '/' );
							StringClear(subitem);
							if (len > 0)
								StringMemcat(subitem, item, len);
							if (len == 0 || !inthash_exists(hdupes, StringBuff(subitem))) {
								char* ptr = NULL;
								ptr += StringLength(list);
								if (len > 0)
									StringStrcat(list, StringBuff(subitem));
								if (isFolder)
									StringStrcat(list, "/");
								StringMemcat(list, "\0", 1);		/* NULL terminated strings */
								StringMemcat(listindexes, &ptr, sizeof(ptr));
								listCount++;
								inthash_write(hdupes, StringBuff(subitem), 0);
							}
						}
					}
				} else {
					CRITICAL("PT_Enumerate:Corrupted central index locator");
				}
			}
		}
		StringFree(subitem);
		inthash_delete(&hdupes);
		if (listCount > 0) {
			unsigned int i;
			void* blk;
			char *nullPointer = NULL;
			char* startStrings;
			/* NULL terminated index */
			StringMemcat(listindexes, &nullPointer, sizeof(nullPointer));
			/* start of all strings (index) */
			startStrings = nullPointer + StringLength(listindexes);
			/* copy list of URLs after indexes */
			StringMemcat(listindexes, StringBuff(list), StringLength(list));
			/* ---- no reallocation beyond this point (fixed addresses) ---- */
			/* start of all strings (pointer) */
			startStrings = (startStrings - nullPointer) + StringBuff(listindexes);
			/* transform indexes into references */
			for(i = 0 ; i < listCount ; i++) {
				char *ptr = NULL;
				unsigned int ndx;
				memcpy(&ptr, &StringBuff(listindexes)[i*sizeof(char*)], sizeof(char*));
				ndx = (unsigned int) (ptr - nullPointer);
				ptr = startStrings + ndx;
				memcpy(&StringBuff(listindexes)[i*sizeof(char*)], &ptr, sizeof(char*));
			}
			blk = StringAcquire(&listindexes);
			StringFree(list);
			StringFree(listindexes);
			return (char **)blk;
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

PT_Index PT_LoadCache(const char *filename) {
	int type = PT_CACHE_UNDEFINED;
	char * dot = strrchr(filename, '.');
	if (dot != NULL) {
		if (strcasecmp(dot, ".zip") == 0) {
			type = PT_CACHE__NEW;
		} else if (strcasecmp(dot, ".ndx") == 0 || strcasecmp(dot, ".dat") == 0) {
			type = PT_CACHE__OLD;
		}
	}
	if (type != PT_CACHE_UNDEFINED) {
		PT_Index index = calloc(sizeof(_PT_Index), 1);
		if (index != NULL) {
			index->type = type;
			index->slots.common.timestamp = (time_t) time(NULL);
			index->slots.common.startUrl[0] = '\0';
			index->slots.common.hash = inthash_new(8191);
			if (!_IndexFuncts[type].PT_LoadCache(index, filename)) {
				DEBUG("reading httrack cache (format #%d) %s : error" _ type _ filename );
				free(index);
				index = NULL;
				return NULL;
			} else {
				DEBUG("reading httrack cache (format #%d) %s : success" _ type _ filename );
			}
			/* default starting URL is the first hash entry */
			if (index->slots.common.startUrl[0] == '\0') {
				struct_inthash_enum en = inthash_enum_new(index->slots.common.hash);
				inthash_chain* chain;
				chain = inthash_enum_next(&en);
				if (chain != NULL
					&& strstr(chain->name, "/robots.txt") != NULL) 
				{
					chain = inthash_enum_next(&en);
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


static long int filesize(const char* filename) {
  struct stat st;
  memset(&st, 0, sizeof(st));
  if (stat(filename, &st) == 0) {
		return (long int)st.st_size;
  }
  return -1;
} 

int PT_LookupCache(PT_Index index, const char* url) {
	if (index != NULL && SAFE_INDEX(index)) {
		return _IndexFuncts[index->type].PT_LookupCache(index, url);
	}
	return 0;
}

time_t PT_Index_Timestamp(PT_Index index) {
	return index->slots.common.timestamp;
}

static int PT_LookupCache__New(PT_Index index, const char* url) {
	int retCode;
	MutexLock(&index->slots.formatNew.zFileLock);
	{
		retCode = PT_LookupCache__New_u(index, url);
	}
	MutexUnlock(&index->slots.formatNew.zFileLock);
	return retCode;
}

static int PT_LookupCache__New_u(PT_Index index_, const char* url) {
	if (index_ != NULL) {
		PT_Index__New index = &index_->slots.formatNew;
		if (index->hash != NULL && index->zFile != NULL && url != NULL && *url != 0) {
			int hash_pos_return;
			if (strncmp(url, "http://", 7) == 0)
				url += 7;
			hash_pos_return = inthash_read(index->hash, url, NULL);
			if (hash_pos_return)
				return 1;
		}
	}
	return 0;
}

int PT_IndexMerge(PT_Indexes indexes, PT_Index *pindex)
{
	if (pindex != NULL && *pindex != NULL && (*pindex)->slots.common.hash != NULL
		&& indexes != NULL) 
	{
		PT_Index index = *pindex;
		struct_inthash_enum en = inthash_enum_new(index->slots.common.hash);
		inthash_chain* chain;
		int index_id = indexes->index_size++;
		int nMerged = 0;
		if ((indexes->index = realloc(indexes->index, sizeof(struct _PT_Index)*indexes->index_size)) != NULL) {
			indexes->index[index_id] = index;
			*pindex = NULL;
			while((chain = inthash_enum_next(&en)) != NULL) {
				const char * url = chain->name;
				if (url != NULL && url[0] != '\0') {
					long int previous_index_id = 0;
					if (inthash_read(indexes->cil, url, (long int*)&previous_index_id)) {
						if (previous_index_id >= 0 && previous_index_id < indexes->index_size) {
							if (indexes->index[previous_index_id]->slots.common.timestamp > index->slots.common.timestamp)			// existing entry is newer
								break;
						} else {
							CRITICAL("PT_IndexMerge:Corrupted central index locator");
						}
					}
					inthash_write(indexes->cil, chain->name, index_id);
					nMerged++;
				}
			}
		} else {
			CRITICAL("PT_IndexMerge:Memory exhausted");
		}
		return nMerged;
	}
	return -1;
}

void PT_Element_Delete(PT_Element *pentry) {
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

PT_Element PT_ReadIndex(PT_Indexes indexes, const char* url, int flags)
{
	if (indexes != NULL) 
	{
		long int index_id;
		if (strncmp(url, "http://", 7) == 0)
			url += 7;
		if (inthash_read(indexes->cil, url, &index_id)) {
			if (index_id >= 0 && index_id <= indexes->index_size) {
				PT_Element item =  PT_ReadCache(indexes->index[index_id], url, flags);
				if (item != NULL) {
					item->indexId = index_id;
					return item;
				}
			} else {
				CRITICAL("PT_ReadCache:Corrupted central index locator");
			}
		}
	}
	return NULL;
}

int PT_LookupIndex(PT_Indexes indexes, const char* url) {
	if (indexes != NULL) 
	{
		long int index_id;
		if (strncmp(url, "http://", 7) == 0)
			url += 7;
		if (inthash_read(indexes->cil, url, &index_id)) {
			if (index_id >= 0 && index_id <= indexes->index_size) {
				return 1;
			} else {
				CRITICAL("PT_ReadCache:Corrupted central index locator");
			}
		}
	}
	return 0;
}

PT_Index PT_GetIndex(PT_Indexes indexes, int indexId) {
	if (indexes != NULL && indexId >= 0 && indexId < indexes->index_size) 
	{
		return indexes->index[indexId];
	}
	return NULL;
}

PT_Element PT_ElementNew() {
	PT_Element r = NULL;
	if ((r = calloc(sizeof(_PT_Element), 1)) == NULL)
		return NULL;
	r->statuscode=STATUSCODE_INVALID;
	r->indexId = -1;
	return r;
}

PT_Element PT_ReadCache(PT_Index index, const char* url, int flags) {
	if (index != NULL && SAFE_INDEX(index)) {
		return _IndexFuncts[index->type].PT_ReadCache(index, url, flags);
	}
	return NULL;
}

static PT_Element PT_ReadCache__New(PT_Index index, const char* url, int flags) {
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
		if (zFile!=NULL) {
			const char * abpath;
			int slashes;
			inthash hashtable = index->hash;

			/* Compute base path for this index - the filename MUST be absolute! */
			for(slashes = 2, abpath = filename + (int)strlen(filename) - 1 
				; abpath > filename && ( ( *abpath != '/'&& *abpath != '\\' ) || --slashes > 0)
				; abpath--);
			index->path[0] = '\0';
			if (slashes == 0 && *abpath != 0) {
				int i;
				strncat(index->path, filename, (int) ( abpath - filename ) + 1 );
				for(i = 0 ; index->path[i] != 0 ; i++) {
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
				memset(comment, 0, sizeof(comment));       // for truncated reads
				do  {
					int readSizeHeader = 0;
					filename[0] = '\0';
					comment[0] = '\0';
					if (unzOpenCurrentFile(zFile) == Z_OK) {
						if (
							(readSizeHeader = unzGetLocalExtrafield(zFile, comment, sizeof(comment) - 2)) > 0
							&&
							unzGetCurrentFileInfo(zFile, NULL, filename, sizeof(filename) - 2, NULL, 0, NULL, 0) == Z_OK
							) 
						{
							long int pos = (long int) unzGetOffset(zFile);
							assertf(readSizeHeader < sizeof(comment));
							comment[readSizeHeader] = '\0';
							entries++;
							if (pos > 0) {
								int dataincache = 0;    // data in cache ?
								char* filenameIndex = filename;
								if (strncmp(filenameIndex, "http://", 7) == 0) {
									filenameIndex += 7;
								}
								if (comment[0] != '\0') {
									int maxLine = 2;
									char* a = comment;
									while(*a && maxLine-- > 0) {      // parse only few first lines
										char line[1024];
										line[0] = '\0';
										a+=binput(a, line, sizeof(line) - 2);
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
									inthash_add(hashtable, filenameIndex, pos);
								else
									inthash_add(hashtable, filenameIndex, -pos);

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
								fprintf(stderr, "Corrupted cache meta entry #%d"LF, (int)entries);
							}
						} else {
							fprintf(stderr, "Corrupted cache entry #%d"LF, (int)entries);
						}
						unzCloseCurrentFile(zFile);
					} else {
						fprintf(stderr, "Corrupted cache entry #%d"LF, (int)entries);
					}
				} while( unzGoToNextFile(zFile) == Z_OK );
				return 1;
			} else {
				inthash_delete(&index->hash);
				index = NULL;
			}
		} else {
			index = NULL;
		}
	}
	return 0;
}

static PT_Element PT_ReadCache__New_u(PT_Index index_, const char* url, int flags)
{
	PT_Index__New index = (PT_Index__New) &index_->slots.formatNew;
  char location_default[HTS_URLMAXSIZE*2];
  char previous_save[HTS_URLMAXSIZE*2];
  char previous_save_[HTS_URLMAXSIZE*2];
  long int hash_pos;
  int hash_pos_return;
	PT_Element r = NULL;
	if (index == NULL || index->hash == NULL || index->zFile == NULL || url == NULL || *url == 0)
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
  hash_pos_return = inthash_read(index->hash, url, (long int*)&hash_pos);

  if (hash_pos_return) {
    uLong posInZip;
    if (hash_pos > 0) {
      posInZip = (uLong) hash_pos;
    } else {
      posInZip = (uLong) -hash_pos;
    }
		if (unzSetOffset(index->zFile, posInZip) == Z_OK) {
      /* Read header (Max 8KiB) */
      if (unzOpenCurrentFile(index->zFile) == Z_OK) {
        char headerBuff[8192 + 2];
        int readSizeHeader;
        int totalHeader = 0;
        int dataincache = 0;
        
        /* For BIG comments */
        headerBuff[0] 
          = headerBuff[sizeof(headerBuff) - 1] 
          = headerBuff[sizeof(headerBuff) - 2] 
          = headerBuff[sizeof(headerBuff) - 3] = '\0';

        if ( (readSizeHeader = unzGetLocalExtrafield(index->zFile, headerBuff, sizeof(headerBuff) - 2)) > 0) 
        {
          int offset = 0;
          char line[HTS_URLMAXSIZE + 2];
          int lineEof = 0;
          headerBuff[readSizeHeader] = '\0';
          do {
            char* value;
            line[0] = '\0';
            offset += binput(headerBuff + offset, line, sizeof(line) - 2);
            if (line[0] == '\0') {
              lineEof = 1;
            }
            value = strchr(line, ':');
            if (value != NULL) {
              *value++ = '\0';
              if (*value == ' ' || *value == '\t') value++;
              ZIP_READFIELD_INT(line, value, "X-In-Cache", dataincache);
              ZIP_READFIELD_INT(line, value, "X-Statuscode", r->statuscode);
              ZIP_READFIELD_STRING(line, value, "X-StatusMessage", r->msg);              // msg
              ZIP_READFIELD_INT(line, value, "X-Size", r->size);           // size
              ZIP_READFIELD_STRING(line, value, "Content-Type", r->contenttype);      // contenttype
              ZIP_READFIELD_STRING(line, value, "X-Charset", r->charset);          // contenttype
              ZIP_READFIELD_STRING(line, value, "Last-Modified", r->lastmodified);     // last-modified
              ZIP_READFIELD_STRING(line, value, "Etag", r->etag);             // Etag
              ZIP_READFIELD_STRING(line, value, "Location", r->location);         // 'location' pour moved
              ZIP_READFIELD_STRING(line, value, "Content-Disposition", r->cdispo);           // Content-disposition
              //ZIP_READFIELD_STRING(line, value, "X-Addr", ..);            // Original address
              //ZIP_READFIELD_STRING(line, value, "X-Fil", ..);            // Original URI filename
              ZIP_READFIELD_STRING(line, value, "X-Save", previous_save_);           // Original save filename
            }
          } while(offset < readSizeHeader && !lineEof);
          totalHeader = offset;

					/* Previous entry */
					if (previous_save_[0] != '\0') {
						int pathLen = (int) strlen(index->path);
						if (pathLen > 0 && strncmp(previous_save_, index->path, pathLen) == 0) {			// old (<3.40) buggy format
							strcpy(previous_save, previous_save_);
						}
						// relative ? (hack)
						else if (index->safeCache
							|| (previous_save_[0] != '/'																	// /home/foo/bar.gif
							&& ( !isalpha(previous_save_[0]) || previous_save_[1] != ':' ) )	// c:/home/foo/bar.gif
							)
						{
							index->safeCache = 1;
							sprintf(previous_save, "%s%s", index->path, previous_save_);
						}
						// bogus format (includes buggy absolute path)
						else {
							/* guess previous path */
							if (index->fixedPath == 0) {
								const char * start = jump_protocol_and_auth(url);
								const char * end = start ? strchr(start, '/') : NULL;
								int len = (int) (end - start);
								if (start != NULL && end != NULL && len > 0 && len < 128) {
									char piece[128 + 2];
									const char * where;
									piece[0] = '\0';
									strncat(piece, start, len);
									if ((where = strstr(previous_save_, piece)) != NULL) {
										index->fixedPath = (int) (where - previous_save_);		// offset to relative path
									}
								}
							}
							if (index->fixedPath > 0) {
								int saveLen = (int) strlen(previous_save_);
								if (index->fixedPath < saveLen) {
									sprintf(previous_save, "%s%s", index->path, previous_save_ + index->fixedPath);
								} else {
									sprintf(r->msg, "Bogus fixePath prefix for %s (prefixLen=%d)", previous_save_, (int)index->fixedPath);
									r->statuscode = STATUSCODE_INVALID;
								}
							} else {
								sprintf(previous_save, "%s%s", index->path, previous_save_);
							}
						}
					}

          /* Complete fields */
          r->adr=NULL;
          if (r->statuscode != STATUSCODE_INVALID) {			/* Can continue */
            int ok = 0;
                       
            // Court-circuit:
            // Peut-on stocker le fichier directement sur disque?
            if (ok) {
              if (r->msg[0] == '\0') {
                strcpy(r->msg,"Cache Read Error : Unexpected error");
              }
            } else { // lire en mémoire
              
              if (!dataincache) {
								/* Read in memory from cache */
								if (flags & FETCH_BODY) {
									if (strnotempty(previous_save)) {
										FILE* fp = fopen(fconv(previous_save), "rb");
										if (fp != NULL) {
											r->adr = (char*) malloc(r->size + 4);
											if (r->adr != NULL) {
												if (r->size > 0 && fread(r->adr, 1,  r->size, fp) != r->size) {
													r->statuscode=STATUSCODE_INVALID;
													sprintf(r->msg,"Read error in cache disk data: %s", strerror(errno));
												}
											} else {
												r->statuscode=STATUSCODE_INVALID;
												strcpy(r->msg,"Read error (memory exhausted) from cache");
											}
											fclose(fp);
										} else {
											r->statuscode=STATUSCODE_INVALID;
											sprintf(r->msg, "Read error (can't open '%s') from cache", fconv(previous_save));
										}
									} else {
										r->statuscode=STATUSCODE_INVALID;
										strcpy(r->msg,"Cached file name is invalid");
									}
								}
              } else {
								// lire fichier (d'un coup)
								if (flags & FETCH_BODY) {
									r->adr=(char*) malloc(r->size+1);
									if (r->adr!=NULL) {
										if (unzReadCurrentFile(index->zFile, r->adr, r->size) != r->size) {  // erreur
											free(r->adr);
											r->adr=NULL;
											r->statuscode=STATUSCODE_INVALID;
											strcpy(r->msg,"Cache Read Error : Read Data");
										} else
											*(r->adr+r->size)='\0';
										//printf(">%s status %d\n",back[p].r->contenttype,back[p].r->statuscode);
									} else {  // erreur
										r->statuscode=STATUSCODE_INVALID;
										strcpy(r->msg,"Cache Memory Error");
									}
								}
							}
            }
          }    // si save==null, ne rien charger (juste en tête)
        } else {
          r->statuscode=STATUSCODE_INVALID;
          strcpy(r->msg,"Cache Read Error : Read Header Data");
        }
        unzCloseCurrentFile(index->zFile);
      } else {
        r->statuscode=STATUSCODE_INVALID;
        strcpy(r->msg,"Cache Read Error : Open File");
      }

    } else {
      r->statuscode=STATUSCODE_INVALID;
      strcpy(r->msg,"Cache Read Error : Bad Offset");
    }
  } else {
    r->statuscode=STATUSCODE_INVALID;
    strcpy(r->msg,"File Cache Entry Not Found");
  }
	if (r->location[0] != '\0') {
		r->location = strdup(r->location);
	} else {
		r->location = NULL;
	}
  return r;
}


/* ------------------------------------------------------------ */
/* Old HTTrack cache (dat/ndx) format                           */
/* ------------------------------------------------------------ */

static int cache_brstr(char* adr,char* s) {
  int i;
  int off;
  char buff[256 + 1];
  off=binput(adr,buff,256);
  adr+=off;
  sscanf(buff,"%d",&i);
  if (i>0)
    strncpy(s,adr,i);
  *(s+i)='\0';
  off+=i;
  return off;
}

static void cache_rstr(FILE* fp,char* s) {
  INTsys i;
  char buff[256+4];
  linput(fp,buff,256);
  sscanf(buff,INTsysP,&i);
  if (i < 0 || i > 32768)    /* error, something nasty happened */
    i=0;
  if (i>0) {
    if ((int) fread(s,1,i,fp) != i) {
      int fread_cache_failed = 0;
      assertf(fread_cache_failed);
    }
  }
  *(s+i)='\0';
}

static char* cache_rstr_addr(FILE* fp) {
  INTsys i;
  char* addr = NULL;
  char buff[256+4];
  linput(fp,buff,256);
  sscanf(buff,"%d",&i);
  if (i < 0 || i > 32768)    /* error, something nasty happened */
    i=0;
  if (i > 0) {
    addr = malloc(i + 1);
    if (addr != NULL) {
      if ((int) fread(addr,1,i,fp) != i) {
        int fread_cache_failed = 0;
        assertf(fread_cache_failed);
      }
      *(addr+i)='\0';
    }
  }
  return addr;
}

static void cache_rint(FILE* fp,int* i) {
  char s[256];
  cache_rstr(fp,s);
  sscanf(s,"%d",i);
}

static void cache_rLLint(FILE* fp,unsigned long* i) {
	int l;
  char s[256];
  cache_rstr(fp,s);
  sscanf(s,"%d",&l);
	*i = (unsigned long)l;
}

static int PT_LoadCache__Old(PT_Index index_, const char *filename) {
	if (index_ != NULL && filename != NULL) {
		char * pos = strrchr(filename, '.');
		PT_Index__Old cache = &index_->slots.formatOld;
		long int ndxSize;
		cache->filenameDat[0] = '\0';
		cache->filenameNdx[0] = '\0';
		cache->path[0] = '\0';

		{
			PT_Index__Old index = cache;
			const char * abpath;
			int slashes;
			/* -------------------- COPY OF THE __New() CODE -------------------- */
			/* Compute base path for this index - the filename MUST be absolute! */
			for(slashes = 2, abpath = filename + (int)strlen(filename) - 1 
				; abpath > filename && ( ( *abpath != '/'&& *abpath != '\\' ) || --slashes > 0)
				; abpath--);
			index->path[0] = '\0';
			if (slashes == 0 && *abpath != 0) {
				int i;
				strncat(index->path, filename, (int) ( abpath - filename ) + 1 );
				for(i = 0 ; index->path[i] != 0 ; i++) {
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
			char * use = malloc(ndxSize + 1);
			if (fread(use, 1, ndxSize, cache->ndx) == ndxSize) {
				char firstline[256];
				char* a=use;
				use[ndxSize] = '\0';
				a += cache_brstr(a, firstline);
				if (strncmp(firstline,"CACHE-",6)==0) {       // Nouvelle version du cache
					if (strncmp(firstline,"CACHE-1.",8)==0) {      // Version 1.1x
						cache->version=(int)(firstline[8]-'0');           // cache 1.x
						if (cache->version <= 5) {
							a+=cache_brstr(a,firstline);
							strcpy(cache->lastmodified,firstline);
						} else {
							// fprintf(opt->errlog,"Cache: version 1.%d not supported, ignoring current cache"LF,cache->version);
							fclose(cache->dat);
							cache->dat=NULL;
							free(use);
							use=NULL;
						}
					} else {        // non supporté
						// fspc(opt->errlog,"error"); fprintf(opt->errlog,"Cache: %s not supported, ignoring current cache"LF,firstline);
						fclose(cache->dat);
						cache->dat=NULL;
						free(use);
						use=NULL;
					}
					/* */
				} else {              // Vieille version du cache
					/* */
					// fspc(opt->log,"warning"); fprintf(opt->log,"Cache: importing old cache format"LF);
					cache->version=0;        // cache 1.0
					strcpy(cache->lastmodified,firstline); 
				}

				/* Create hash table for the cache (MUCH FASTER!) */
				if (use) {
					char line[HTS_URLMAXSIZE*2];
					char linepos[256];
					int  pos;
					int firstSeen = 0;
					while ( (a!=NULL) && (a < (use + ndxSize) ) ) {
						a=strchr(a+1,'\n');     /* start of line */
						if (a) {
							a++;
							/* read "host/file" */
							a+=binput(a,line,HTS_URLMAXSIZE);
							a+=binput(a,line+strlen(line),HTS_URLMAXSIZE);
							/* read position */
							a+=binput(a,linepos,200);
							sscanf(linepos,"%d",&pos);

							/* Add entry */
							inthash_add(cache->hash,line,pos);

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
					use=NULL;
					return 1;
				}
			}
		}
	}
	return 0;
}

static String DecodeUrl(const char * url) {
	int i;
	String s = STRING_EMPTY;
	StringClear(s);
	for(i = 0 ; url[i] != '\0' ; i++) {
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
					StringAddchar(s, (char)codepoint);
				}
				i += 2;
			}
		} else {
			StringAddchar(s, url[i]);
		}
	}
	return s;
}

static PT_Element PT_ReadCache__Old(PT_Index index, const char* url, int flags) {
	PT_Element retCode;
	MutexLock(&index->slots.formatOld.fileLock);
	{
		retCode = PT_ReadCache__Old_u(index, url, flags);
	}
	MutexUnlock(&index->slots.formatOld.fileLock);
	return retCode;
}

static PT_Element PT_ReadCache__Old_u(PT_Index index_, const char* url, int flags) {
	PT_Index__Old cache = (PT_Index__Old) &index_->slots.formatOld;
  long int hash_pos;
  int hash_pos_return;
  char location_default[HTS_URLMAXSIZE*2];
  char previous_save[HTS_URLMAXSIZE*2];
  char previous_save_[HTS_URLMAXSIZE*2];
  PT_Element r;
  int ok=0;

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
  hash_pos_return=inthash_read(cache->hash, url, (long int*)&hash_pos);

  if (hash_pos_return) {
    int pos = (int) hash_pos;     /* simply */

    if (fseek(cache->dat, (pos>0) ? pos : (-pos), SEEK_SET) == 0) {
			/* Importer cache1.0 */
			if (cache->version==0) {
				OLD_htsblk old_r;
				if (fread((char*) &old_r,1,sizeof(old_r),cache->dat) == sizeof(old_r)) { // lire tout (y compris statuscode etc)
					int i;
					String urlDecoded;
					r->statuscode = old_r.statuscode;
					r->size = old_r.size;        // taille fichier
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
					for(i = 0 ; previous_save_[i] != '\0' && previous_save_[i] != '?' ; i++) {
						if (FORBIDDEN_CHAR(previous_save_[i])) {
							previous_save_[i] = '_';
						}
					}
					previous_save_[i] = '\0';
#undef FORBIDDEN_CHAR
				ok = 1;     /* import  ok */
			}
      /* */
      /* Cache 1.1 */
      } else {
        char check[256];
        unsigned long size_read;
        check[0]='\0';
        //
        cache_rint(cache->dat,&r->statuscode);
        cache_rLLint(cache->dat,&r->size);
        cache_rstr(cache->dat,r->msg);
        cache_rstr(cache->dat,r->contenttype);
        if (cache->version >= 3)
          cache_rstr(cache->dat,r->charset);
        cache_rstr(cache->dat,r->lastmodified);
        cache_rstr(cache->dat,r->etag);
        cache_rstr(cache->dat,r->location);
        if (cache->version >= 2)
          cache_rstr(cache->dat,r->cdispo);
        if (cache->version >= 4) {
          cache_rstr(cache->dat, previous_save_);  // adr
          cache_rstr(cache->dat, previous_save_);  // fil
          previous_save[0] = '\0';
          cache_rstr(cache->dat, previous_save_);  // save
        }
        if (cache->version >= 5) {
          r->headers = cache_rstr_addr(cache->dat);
        }
        //
        cache_rstr(cache->dat,check);
        if (strcmp(check,"HTS")==0) {           /* intégrité OK */
          ok=1;
        }
        cache_rLLint(cache->dat, &size_read);       /* lire size pour être sûr de la taille déclarée (réécrire) */
        if (size_read > 0) {                         /* si inscrite ici */
          r->size = size_read;
        } else {                              /* pas de données directement dans le cache, fichier présent? */
					r->size = 0;
        }
      }

			/* Check destination filename */

			{
				PT_Index__Old index = cache;
				/* -------------------- COPY OF THE __New() CODE -------------------- */
				if (previous_save_[0] != '\0') {
					int pathLen = (int) strlen(index->path);
					if (pathLen > 0 && strncmp(previous_save_, index->path, pathLen) == 0) {			// old (<3.40) buggy format
						strcpy(previous_save, previous_save_);
					}
					// relative ? (hack)
					else if (index->safeCache
						|| (previous_save_[0] != '/'																	// /home/foo/bar.gif
						&& ( !isalpha(previous_save_[0]) || previous_save_[1] != ':' ) )	// c:/home/foo/bar.gif
						)
					{
						index->safeCache = 1;
						sprintf(previous_save, "%s%s", index->path, previous_save_);
					}
					// bogus format (includes buggy absolute path)
					else {
						/* guess previous path */
						if (index->fixedPath == 0) {
							const char * start = jump_protocol_and_auth(url);
							const char * end = start ? strchr(start, '/') : NULL;
							int len = (int) (end - start);
							if (start != NULL && end != NULL && len > 0 && len < 128) {
								char piece[128 + 2];
								const char * where;
								piece[0] = '\0';
								strncat(piece, start, len);
								if ((where = strstr(previous_save_, piece)) != NULL) {
									index->fixedPath = (int) (where - previous_save_);		// offset to relative path
								}
							}
						}
						if (index->fixedPath > 0) {
							int saveLen = (int) strlen(previous_save_);
							if (index->fixedPath < saveLen) {
								sprintf(previous_save, "%s%s", index->path, previous_save_ + index->fixedPath);
							} else {
								sprintf(r->msg, "Bogus fixePath prefix for %s (prefixLen=%d)", previous_save_, (int)index->fixedPath);
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
				if ( (r->statuscode>=0) && (r->statuscode<=999)) {
					r->adr = NULL;
					if (pos<0) {
						if (flags & FETCH_BODY) {
							FILE* fp = fopen(previous_save, "rb");
							if (fp != NULL) {
								r->adr = (char*) malloc(r->size + 1);
								if (r->adr != NULL) {
									if (r->size > 0 && fread(r->adr, 1, r->size, fp) != r->size) {
										r->statuscode=STATUSCODE_INVALID;
										strcpy(r->msg,"Read error in cache disk data");
									}
									r->adr[r->size] = '\0';
								} else {
									r->statuscode=STATUSCODE_INVALID;
									strcpy(r->msg,"Read error (memory exhausted) from cache");
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
							r->adr=(char*) malloc(r->size + 1);
							if (r->adr!=NULL) {
								if (fread(r->adr, 1, r->size,cache->dat) != r->size) {  // erreur
									free(r->adr);
									r->adr=NULL;
									r->statuscode=STATUSCODE_INVALID;
									strcpy(r->msg,"Cache Read Error : Read Data");
								} else
									r->adr[r->size] = '\0';
							} else {  // erreur
								r->statuscode=STATUSCODE_INVALID;
								strcpy(r->msg,"Cache Memory Error");
							}
						}
					}
				} else {
          r->statuscode=STATUSCODE_INVALID;
          strcpy(r->msg,"Cache Read Error : Bad Data");
        }
      } else {  // erreur
        r->statuscode=STATUSCODE_INVALID;
        strcpy(r->msg,"Cache Read Error : Read Header");
      }
    } else {
      r->statuscode=STATUSCODE_INVALID;
      strcpy(r->msg,"Cache Read Error : Seek Failed");
    }
  } else {
    r->statuscode=STATUSCODE_INVALID;
    strcpy(r->msg,"File Cache Entry Not Found");
  }
	if (r->location[0] != '\0') {
		r->location = strdup(r->location);
	} else {
		r->location = NULL;
	}
  return r;
}

static int PT_LookupCache__Old(PT_Index index, const char* url) {
	int retCode;
	MutexLock(&index->slots.formatOld.fileLock);
	{
		retCode = PT_LookupCache__Old_u(index, url);
	}
	MutexUnlock(&index->slots.formatOld.fileLock);
	return retCode;
}

static int PT_LookupCache__Old_u(PT_Index index_, const char* url) {
	if (index_ != NULL) {
		PT_Index__New cache = (PT_Index__New) &index_->slots.formatNew;
		if (cache == NULL || cache->hash == NULL || url == NULL || *url == 0)
			return 0;
		if (strncmp(url, "http://", 7) == 0)
			url += 7;
		if (inthash_read(cache->hash, url, NULL))
			return 1;
	}
	return 0;
}

