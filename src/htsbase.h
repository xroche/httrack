/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) Xavier Roche and other contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 3
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
/* File: Basic definitions                                      */
/*       Used in .c files for basic (malloc() ..) definitions   */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTS_BASICH
#define HTS_BASICH

#ifdef __cplusplus
extern "C" {
#endif

#include "htsglobal.h"
#include "htsstrings.h"

#include <string.h>
#include <time.h>

#ifndef _WIN32
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>

#ifndef _WIN32
#include <dlfcn.h>
#endif

#include <errno.h>

#ifdef _WIN32
#else
#include <fcntl.h>
#endif
#include <assert.h>

/* GCC extension */
#ifndef HTS_UNUSED
#ifdef __GNUC__
#define HTS_UNUSED __attribute__ ((unused))
#define HTS_STATIC static __attribute__ ((unused))
#else
#define HTS_UNUSED
#define HTS_STATIC static
#endif
#endif

#undef min
#undef max
#define min(a,b) ((a)>(b)?(b):(a))
#define max(a,b) ((a)>(b)?(a):(b))

#ifndef _WIN32
#undef Sleep
#define min(a,b) ((a)>(b)?(b):(a))
#define max(a,b) ((a)>(b)?(a):(b))
#define Sleep(a) { if (((a)*1000)%1000000) usleep(((a)*1000)%1000000); if (((a)*1000)/1000000) sleep(((a)*1000)/1000000); }
#endif

// teste égalité de 2 chars, case insensitive
#define hichar(a) ((((a)>='a') && ((a)<='z')) ? ((a)-('a'-'A')) : (a))
#define streql(a,b) (hichar(a)==hichar(b))

// caractère maj
#define isUpperLetter(a) ( ((a) >= 'A') && ((a) <= 'Z') )


/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE


// functions
#ifdef _WIN32
#define DynamicGet(handle, sym) GetProcAddress(handle, sym)
#else
#define DynamicGet(handle, sym) dlsym(handle, sym)
#endif

// emergency log
typedef void (*t_abortLog)(char* msg, char* file, int line);
extern HTSEXT_API t_abortLog abortLog__;
#define abortLog(a) abortLog__(a, __FILE__, __LINE__)
#define _ ,
#ifndef _WIN32_WCE
#define abortLogFmt(a) do { \
  FILE* fp = fopen("CRASH.TXT", "wb"); \
  if (!fp) fp = fopen("/tmp/CRASH.TXT", "wb"); \
  if (!fp) fp = fopen("C:\\CRASH.TXT", "wb"); \
  if (!fp) fp = fopen("\\Temp\\CRASH.TXT", "wb"); \
  if (!fp) fp = fopen("\\CRASH.TXT", "wb"); \
  if (!fp) fp = fopen("CRASH.TXT", "wb"); \
  if (fp) { \
    fprintf(fp, "HTTrack " HTTRACK_VERSIONID " closed at '" __FILE__ "', line %d\r\n", __LINE__); \
    fprintf(fp, "Reason:\r\n"); \
    fprintf(fp, a); \
    fprintf(fp, "\r\n"); \
    fflush(fp); \
    fclose(fp); \
  } \
} while(0)
#else
#define abortLogFmt(a) do { \
  XCEShowMessageA("HTTrack " HTTRACK_VERSIONID " closed at '" __FILE__ "', line %d\r\nReason:\r\n%s\r\n", __LINE__, a); \
} while(0)
#endif

#define assertf(exp) do { \
  if (! ( exp ) ) { \
    abortLog("assert failed: " #exp); \
    if (htsCallbackErr != NULL) { \
      htsCallbackErr("assert failed: " #exp, __FILE__ , __LINE__ ); \
    } \
    assert(exp); \
    abort(); \
  } \
} while(0)
/* non-fatal assert */
#define assertnf(exp) do { \
  if (! ( exp ) ) { \
    abortLog("assert failed: " #exp); \
    if (htsCallbackErr != NULL) { \
      htsCallbackErr("assert failed: " #exp, __FILE__ , __LINE__ ); \
    } \
  } \
} while(0)

/* logging */
typedef enum {
	LOG_DEBUG,
	LOG_INFO,
	LOG_WARNING,
	LOG_ERROR,
	LOG_PANIC
} HTS_LogType;
#define HTS_LOG(OPT,TYPE) do { \
  int last_errno = errno; \
	switch(TYPE) { \
	case LOG_DEBUG: \
		fspc(OPT,(OPT)->log, "debug"); \
		break; \
	case LOG_INFO: \
		fspc(OPT,(OPT)->log, "info"); \
		break; \
	case LOG_WARNING: \
		fspc(OPT,(OPT)->log, "warning"); \
		break; \
	case LOG_ERROR: \
		fspc(OPT,(OPT)->log, "error"); \
		break; \
	case LOG_PANIC: \
		fspc(OPT,(OPT)->log, "panic"); \
		break; \
	} \
  errno = last_errno; \
} while(0)

/* regular malloc's() */
#ifndef HTS_TRACE_MALLOC
#define malloct(A)          malloc(A)
#define calloct(A,B)        calloc((A), (B))
#define freet(A)            do { assertnf((A) != NULL); if ((A) != NULL) { free(A); (A) = NULL; } } while(0)
#define strdupt(A)          strdup(A)
#define realloct(A,B)       ( ((A) != NULL) ? realloc((A), (B)) : malloc(B) )
#define memcpybuff(A, B, N) memcpy((A), (B), (N))
#else
/* debug version */
#define malloct(A)    hts_malloc(A)
#define calloct(A,B)  hts_calloc(A,B)
#define strdupt(A)    hts_strdup(A)
#define freet(A)      do { hts_free(A); (A) = NULL; } while(0)
#define realloct(A,B) hts_realloc(A,B)
void  hts_freeall();
void* hts_malloc    (size_t);
void* hts_calloc(size_t,size_t);
char* hts_strdup(char*);
void* hts_xmalloc(size_t,size_t);
void  hts_free      (void*);
void* hts_realloc   (void*,size_t);
mlink* hts_find(char* adr);
/* protected memcpy */
#define memcpybuff(A, B, N) do { \
  mlink* lnk = hts_find((void*)(A)); \
  if (lnk != NULL) { \
    assertf(lnk != NULL); \
    assertf( * ( (t_htsboundary*) ( ((char*) lnk->adr) - sizeof(htsboundary) ) ) == htsboundary ); \
    assertf( * ( (t_htsboundary*) ( ((char*) lnk->adr) + lnk->len ) ) == htsboundary ); \
    assertf( ( ((char*)(A)) + (N)) < (char*) (lnk->adr + lnk->len) ); \
  } \
  memcpy(A, B, N); \
} while(0)

#endif

typedef void (* htsErrorCallback)(char* msg, char* file, int line);
extern HTSEXT_API htsErrorCallback htsCallbackErr;
extern HTSEXT_API int htsMemoryFastXfr;

/*
*/

#define stringdup()

#ifdef STRDEBUG

/* protected strcat, strncat and strcpy - definitely useful */
#define strcatbuff(A, B) do { \
  assertf( (A) != NULL ); \
  if ( ! (B) ) { assertf( 0 ); } \
  if (htsMemoryFastXfr) { \
    if (sizeof(A) != sizeof(char*)) { \
      (A)[sizeof(A) - 1] = '\0'; \
    } \
    strcat(A, B); \
    if (sizeof(A) != sizeof(char*)) { \
      assertf((A)[sizeof(A) - 1] == '\0'); \
    } \
  } else { \
    unsigned int sz = (unsigned int) strlen(A); \
    unsigned int szf = (unsigned int) strlen(B); \
    if (sizeof(A) != sizeof(char*)) { \
      assertf(sz + szf + 1 < sizeof(A)); \
      if (szf > 0) { \
        if (sz + szf + 1 < sizeof(A)) { \
          memcpy((A) + sz, (B), szf + 1); \
        } \
      } \
    } else if (szf > 0) { \
      memcpybuff((A) + sz, (B), szf + 1); \
    } \
  } \
} while(0)
#define strncatbuff(A, B, N) do { \
  assertf( (A) != NULL ); \
  if ( ! (B) ) { assertf( 0 ); } \
  if (htsMemoryFastXfr) { \
    if (sizeof(A) != sizeof(char*)) { \
      (A)[sizeof(A) - 1] = '\0'; \
    } \
    strncat(A, B, N); \
    if (sizeof(A) != sizeof(char*)) { \
      assertf((A)[sizeof(A) - 1] == '\0'); \
    } \
  } else { \
    unsigned int sz = (unsigned int) strlen(A); \
    unsigned int szf = (unsigned int) strlen(B); \
    if (szf > (unsigned int) (N)) szf = (unsigned int) (N); \
    if (sizeof(A) != sizeof(char*)) { \
      assertf(sz + szf + 1 < sizeof(A)); \
      if (szf > 0) { \
        if (sz + szf + 1 < sizeof(A)) { \
          memcpy((A) + sz, (B), szf); \
          * ( (A) + sz + szf) = '\0'; \
        } \
      } \
    } else if (szf > 0) { \
      memcpybuff((A) + sz, (B), szf); \
      * ( (A) + sz + szf) = '\0'; \
    } \
  } \
} while(0)
#define strcpybuff(A, B) do { \
  assertf( (A) != NULL ); \
  if ( ! (B) ) { assertf( 0 ); } \
  if (htsMemoryFastXfr) { \
    if (sizeof(A) != sizeof(char*)) { \
      (A)[sizeof(A) - 1] = '\0'; \
    } \
    strcpy(A, B); \
    if (sizeof(A) != sizeof(char*)) { \
      assertf((A)[sizeof(A) - 1] == '\0'); \
    } \
  } else { \
    unsigned int szf = (unsigned int) strlen(B); \
    if (sizeof(A) != sizeof(char*)) { \
      assertf(szf + 1 < sizeof(A)); \
      if (szf > 0) { \
        if (szf + 1 < sizeof(A)) { \
          memcpy((A), (B), szf + 1); \
        } else { \
          * (A) = '\0'; \
        } \
      } else { \
        * (A) = '\0'; \
      } \
    } else { \
      memcpybuff((A), (B), szf + 1); \
    } \
  } \
} while(0)

#else

#ifdef STRDEBUGFAST

/* protected strcat, strncat and strcpy - definitely useful */
#define strcatbuff(A, B) do { \
  assertf( (A) != NULL ); \
  if ( ! (B) ) { assertf( 0 ); } \
  if (sizeof(A) != sizeof(char*)) { \
    (A)[sizeof(A) - 1] = '\0'; \
  } \
  strcat(A, B); \
  if (sizeof(A) != sizeof(char*)) { \
    assertf((A)[sizeof(A) - 1] == '\0'); \
  } \
} while(0)
#define strncatbuff(A, B, N) do { \
  assertf( (A) != NULL ); \
  if ( ! (B) ) { assertf( 0 ); } \
  if (sizeof(A) != sizeof(char*)) { \
    (A)[sizeof(A) - 1] = '\0'; \
  } \
  strncat(A, B, N); \
  if (sizeof(A) != sizeof(char*)) { \
    assertf((A)[sizeof(A) - 1] == '\0'); \
  } \
} while(0)
#define strcpybuff(A, B) do { \
  assertf( (A) != NULL ); \
  if ( ! (B) ) { assertf( 0 ); } \
  if (sizeof(A) != sizeof(char*)) { \
    (A)[sizeof(A) - 1] = '\0'; \
  } \
  strcpy(A, B); \
  if (sizeof(A) != sizeof(char*)) { \
    assertf((A)[sizeof(A) - 1] == '\0'); \
  } \
} while(0)

#else

#define strcatbuff strcat
#define strncatbuff strncat
#define strcpybuff strcpy

#endif

#endif

#endif

#ifdef __cplusplus
}
#endif

#endif
