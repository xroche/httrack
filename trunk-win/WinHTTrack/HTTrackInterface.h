
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>

#include "httrack-library.h"
/**/
//#include "htsglobal.h"
//#include "htsbase.h"
#include "htsopt.h"
#include "htsdefines.h"
#include "htsstrings.h"

extern int linput(FILE* fp,char* s,int max);
extern int linput_trim(FILE* fp,char* s,int max);
extern int linput_cpp(FILE* fp,char* s,int max);
extern void rawlinput(FILE* fp,char* s,int max);
extern int binput(char* buff,char* s,int max);
extern int fexist(const char* s);
extern off_t fsize(const char* s);
extern TStamp time_local(void);

extern char* __fslash(char* a);
extern char* fslash(char* catbuff,const char* a);
extern char* convtolower(char* catbuff,const char* a);
extern void hts_lowcase(char* s);

extern char* next_token(char* p,int flag);

// Engine internal variables
typedef void (* htsErrorCallback)(char* msg, char* file, int line);
extern HTSEXT_API htsErrorCallback htsCallbackErr;
extern HTSEXT_API int htsMemoryFastXfr;
/* */
extern HTSEXT_API hts_stat_struct HTS_STAT;
extern int _DEBUG_HEAD;
extern FILE* ioinfo;




// from htsbase.h

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


// emergency log
typedef void (*t_abortLog)(char* msg, char* file, int line);
extern HTSEXT_API t_abortLog abortLog__;
#define abortLog(a) abortLog__(a, __FILE__, __LINE__)
#define abortLogFmt(a) do { \
  FILE* fp = fopen("CRASH.TXT", "wb"); \
  if (!fp) fp = fopen("/tmp/CRASH.TXT", "wb"); \
  if (!fp) fp = fopen("C:\\CRASH.TXT", "wb"); \
  if (fp) { \
    fprintf(fp, "HTTrack " HTTRACK_VERSIONID " closed at '" __FILE__ "', line %d\r\n", __LINE__); \
    fprintf(fp, "Reason:\r\n"); \
    fprintf(fp, a); \
    fprintf(fp, "\r\n"); \
    fflush(fp); \
    fclose(fp); \
  } \
} while(0)

#define _ ,
#define abortLogFmt(a) do { \
  FILE* fp = fopen("CRASH.TXT", "wb"); \
  if (!fp) fp = fopen("/tmp/CRASH.TXT", "wb"); \
  if (!fp) fp = fopen("C:\\CRASH.TXT", "wb"); \
  if (fp) { \
    fprintf(fp, "HTTrack " HTTRACK_VERSIONID " closed at '" __FILE__ "', line %d\r\n", __LINE__); \
    fprintf(fp, "Reason:\r\n"); \
    fprintf(fp, a); \
    fprintf(fp, "\r\n"); \
    fflush(fp); \
    fclose(fp); \
  } \
} while(0)
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

//

#define malloct(A)          malloc(A)
#define calloct(A,B)        calloc((A), (B))
#define freet(A)            do { assertnf((A) != NULL); if ((A) != NULL) { free(A); (A) = NULL; } } while(0)
#define strdupt(A)          strdup(A)
#define realloct(A,B)       ( ((A) != NULL) ? realloc((A), (B)) : malloc(B) )
#define memcpybuff(A, B, N) memcpy((A), (B), (N))


// various
#define copychar(a) concat(catbuff,(a),NULL)
