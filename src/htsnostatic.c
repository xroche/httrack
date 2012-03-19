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


Important notes:

- We hereby ask people using this source NOT to use it in purpose of grabbing
emails addresses, or collecting any other private information on persons.
This would disgrace our work, and spoil the many hours we spent on it.


Please visit our Website: http://www.httrack.com
*/


/* ------------------------------------------------------------ */
/* File: htsnostatic.c subroutines:                             */
/*       thread-safe routines for reentrancy                    */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htsnostatic.h"

#include "htsbase.h"
#include "htshash.h"

typedef struct {
  /*
  inthash values;
  */
  inthash blocks;
} hts_varhash;

#if USE_BEGINTHREAD
static PTHREAD_LOCK_TYPE hts_static_Mutex;
#endif
static int hts_static_Mutex_init=0;
#if HTS_WIN
#else
static PTHREAD_KEY_TYPE hts_static_key;
#endif

int hts_initvar() {
  if (!hts_static_Mutex_init) {
    /* Init done */
    hts_static_Mutex_init=1;
#if USE_BEGINTHREAD
    /* Init mutex */
    htsSetLock(&hts_static_Mutex, -999);
    
#if HTS_WIN
#else
    /* Init hash */
    PTHREAD_KEY_CREATE(&hts_static_key, hts_destroyvar);
#endif
#endif
  }

  /* Set specific thread value */
#if USE_BEGINTHREAD
#if HTS_WIN
#else
  {
    void* thread_val;
    hts_varhash* hts_static_hash = (hts_varhash*) malloc(sizeof(hts_static_hash));
    if (!hts_static_hash)
      return 0;
      /*
      hts_static_hash->values = inthash_new(HTS_VAR_MAIN_HASH);
      if (!hts_static_hash->values)
      return 0;
    */
    hts_static_hash->blocks = inthash_new(HTS_VAR_MAIN_HASH);
    if (!hts_static_hash->blocks)
      return 0;
    /* inthash_value_is_malloc(hts_static_hash->values, 0);  */ /* Regular values */
    inthash_value_is_malloc(hts_static_hash->blocks, 1);     /* We'll have to free them upon term! */
    inthash_value_set_free_handler(hts_static_hash->blocks, hts_destroyvar_key);  /* free handler */
    thread_val = (void*) hts_static_hash;
    
    PTHREAD_KEY_SET(hts_static_key, thread_val, inthash);
  }
#endif
#endif

  return 1;
}

/*
  hash table free handler to free all keys
*/
void hts_destroyvar_key(void* adr) {
#if HTS_WIN
#else
  hts_NostaticComplexKey* cKey = (hts_NostaticComplexKey*) adr;
  if (cKey) {
    void* block_address = NULL;
    PTHREAD_KEY_GET(cKey->localKey, &block_address, void*);
    /* Free block */
    if (block_address) {
      free(block_address);
    }
    cKey->localInit = 0;
  }
#endif
}

void hts_destroyvar(void* ptrkey) {
#if HTS_WIN
#else
  if (ptrkey) {
    hts_varhash* hashtables = (hts_varhash*) ptrkey;
    PTHREAD_KEY_SET(hts_static_key, NULL, inthash);  /* unregister */

    /* Destroy has table */
    inthash_delete(&(hashtables->blocks));  /* will magically call hts_destroyvar_key(), too */
    /*
    inthash_delete(&(hashtables->values));
    */
    free(ptrkey);
  }
#endif
}

/*
  destroy all key values (for the current thread)
*/
int hts_freevar() {
#if HTS_WIN
#if 0
  void* thread_val = NULL;
  PTHREAD_KEY_GET(hts_static_key, &thread_val, inthash);
  hts_destroyvar(thread_val);
  PTHREAD_KEY_SET(hts_static_key, NULL, inthash);  /* unregister */
  /*
  PTHREAD_KEY_DELETE(hts_static_key); NO
  */
#endif
#endif
  return 1;
}

int hts_resetvar() {
  int r;
  hts_lockvar();
  {
    hts_freevar();
    r = hts_initvar();
  }
  hts_unlockvar();
  return r;
}

int hts_maylockvar() {
  return hts_static_Mutex_init;
}

int hts_lockvar() {
#if USE_BEGINTHREAD
  htsSetLock(&hts_static_Mutex, 1);
#endif
  return 1;
}

int hts_unlockvar() {
#if USE_BEGINTHREAD
  htsSetLock(&hts_static_Mutex, 0);
#endif
  return 1;
}

int hts_setvar(char* name, long int value) {
  return hts_setextvar(name, (long int)value, 0);
}

int hts_setblkvar(char* name, void* value) {
  return hts_setextvar(name, (long int)value, 1);
}

int hts_setextvar(char* name, long int value, int flag) {
#if HTS_WIN
#else
  void* thread_val = NULL;
  hts_varhash* hashtables;
  
  /*
  hts_lockvar();   // NO - MUST be protected by caller
  {
  */
  PTHREAD_KEY_GET(hts_static_key, &thread_val, inthash);
  hashtables = (hts_varhash*) thread_val;
  if (hashtables) { // XXc XXC hack for win version
    inthash_write(hashtables->blocks, name, value);
  }
#endif
  
  return 1;
}


int hts_getvar(char* name, long int* ptrvalue) {
  return hts_getextvar(name, (long int*)ptrvalue, 0);
}

int hts_getblkvar(char* name, void** ptrvalue) {
  return hts_getextvar(name, (long int*)ptrvalue, 1);
}

int hts_getextvar(char* name, long int* ptrvalue, int flag) {
#if HTS_WIN
#else
  void* thread_val = NULL;
  hts_varhash* hashtables;
  
  hts_lockvar();
  {
    PTHREAD_KEY_GET(hts_static_key, &thread_val, inthash);
    hashtables = (hts_varhash*) thread_val;
    /*  if (flag) {
    */
    inthash_read(hashtables->blocks, name, ptrvalue);
    /*
    } else {
    inthash_read(hashtables->values, name, ptrvalue);
    }
    */
  }
  hts_unlockvar();
#endif
  
  return 1;
}

long int hts_directgetvar(char* name) {
  long int value=0;
  hts_getvar(name, &value);
  return value;
}

void* hts_directgetblkvar(char* name) {
  void* value=NULL;
  hts_getblkvar(name, &value);
  return value;
}
