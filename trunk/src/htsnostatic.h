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

/*
  Okay, with these routines, the engine should be fully reentrant (thread-safe)
  All static references have been changed:

  from
  function foo() {
    static bartype bar;
  }
  to:
  function foo() {
    bartype* bar;
    NOSTATIC_RESERVE(bar, bartype, 1);
  }
*/

#ifndef HTSNOSTATIC_DEFH
#define HTSNOSTATIC_DEFH 

#include "htscore.h"
#include "htsthread.h"

/*
#if USE_PTHREAD
#if HTS_WIN
#undef HTS_REENTRANT
#else
#define HTS_REENTRANT
#endif
#else
#undef HTS_REENTRANT
#endif
*/

#define HTS_VAR_MAIN_HASH 127

/*
  MutEx
*/


/* Magic per-thread variables functions 

  Example:
  hts_lockvar();
  hts_setvar("MyFoo", (long int)(void*)&foo);
  hts_unlockvar();
  ..
  foo=(void*)(long int)hts_directgetvar("MyFoo");

  Do not forget to initialize (hts_initvar()) the library once per thread
*/
int hts_initvar(void);
int hts_freevar(void);
#ifndef HTTRACK_DEFLIB
HTSEXT_API int hts_resetvar(void);
#endif
int hts_maylockvar(void);
int hts_lockvar(void);
int hts_unlockvar(void);

int hts_setvar(char* name, long int value);
int hts_getvar(char* name, long int* ptrvalue);
long int hts_directgetvar(char* name);

int hts_setblkvar(char* name, void* value);
int hts_getblkvar(char* name, void** ptrvalue);
void* hts_directgetblkvar(char* name);

/* Internal */
int hts_setextvar(char* name, long int value, int flag);
int hts_getextvar(char* name, long int* ptrvalue, int flag);
void hts_destroyvar(void* ptrkey);
void hts_destroyvar_key(void* adr);

/* 
  Ensure that the variable 'name' has 'nelts' of type 'type' reserved 
  fnc is an UNIQUE function name
*/
#define NOSTATIC_RESERVE(name, type, nelt) NOSTATIC_XRESERVE(name, type, nelt)

/*
  Note:
  Yes, we first read the localInit flag variable without MutEx protection,
  for optimization purpose, because the flag is set once initialization DONE.
  If the first read fails, we *securely* re-check and initialize *if* necessary.
  The abort() things should NEVER be called, and are here for safety reasons
*/
/*
  function-specific static cKey:
  cKey = { localKey, localInit }
              ||        \
              \/          \ ==1 upon initialization
         thread variable
              ||
              \/
             void*
              ||
              \/
      'thread-static' value

  the function-specific static cKey is also referenced in the global 
  hashtable for free() purpose: (see hts_destroyvar())

      global static key variable
        'hts_static_key'
               ||
               \/
         thread variable
               ||
               \/
              void*
               ||
               \/
            hashtable
               ||
               \/
     function-specific hash key
               ||
               \/
              &cKey

*/
#if HTS_WIN

/* Windows: handled by the compiler */
#define NOSTATIC_XRESERVE(name, type, nelt) do { \
  __declspec( thread ) static type thValue[nelt]; \
  __declspec( thread ) int  static initValue = 0; \
  name = thValue; \
  if (!initValue) { \
    initValue = 1; \
    memset(&thValue, 0, sizeof(thValue)); \
  } \
} while(0)

#else

/* Un*x : slightly more complex, we have to create a thread-key */
typedef struct {
  PTHREAD_KEY_TYPE localKey;
  unsigned char localInit;
} hts_NostaticComplexKey;
#define NOSTATIC_XRESERVE(name, type, nelt) do { \
static hts_NostaticComplexKey cKey={0,0}; \
name = NULL; \
if ( cKey.localInit ) { \
  PTHREAD_KEY_GET(cKey.localKey, &name, type*); \
} \
if ( ( ! cKey.localInit ) || ( name == NULL ) ) { \
  if (!hts_maylockvar()) { \
    abortLog("unable to lock mutex (not initialized?!)"); \
    abort(); \
  } \
  hts_lockvar(); \
  { \
    { \
      name = (type *) calloc((nelt), sizeof(type)); \
      if (name == NULL) { \
        abortLog("unable to allocate memory for variable!"); \
        abort(); \
      } \
      { \
        char elt_name[64+8]; \
        sprintf(elt_name, #name "_%d", (int) __LINE__); \
        PTHREAD_KEY_CREATE(&(cKey.localKey), NULL); \
        hts_setblkvar(elt_name, &cKey); \
      } \
      PTHREAD_KEY_SET(cKey.localKey, name, type*); \
      name = NULL; \
      PTHREAD_KEY_GET(cKey.localKey, &name, type*); \
      if (name == NULL) { \
        abortLog("unable to load thread key!"); \
        abort(); \
      } \
      if ( ! cKey.localInit ) { \
        cKey.localInit = 1; \
      } \
    } \
  } \
  hts_unlockvar(); \
} \
else { \
  PTHREAD_KEY_GET(cKey.localKey, &name, type*); \
  if (name == NULL) { \
    abortLog("unable to load thread key! (2)"); \
    abort(); \
  } \
} \
} while(0)
#endif

#endif
