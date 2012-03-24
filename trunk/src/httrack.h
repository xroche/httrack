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
/* File: htsshow.c console progress info                        */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTSTOOLS_DEFH
#define HTSTOOLS_DEFH 

#include "htsglobal.h"
#include "htscore.h"

#ifndef HTS_DEF_FWSTRUCT_t_StatsBuffer
#define HTS_DEF_FWSTRUCT_t_StatsBuffer
typedef struct t_StatsBuffer t_StatsBuffer;
#endif
struct t_StatsBuffer {
  char name[1024];
  char file[1024];
  char state[256];
  char BIGSTK url_sav[HTS_URLMAXSIZE*2];    // pour cancel
  char BIGSTK url_adr[HTS_URLMAXSIZE*2];
  char BIGSTK url_fil[HTS_URLMAXSIZE*2];
  LLint size;
  LLint sizetot;
  int offset;
  //
  int back;
  //
  int actived;    // pour disabled
};

#ifndef HTS_DEF_FWSTRUCT_t_InpInfo
#define HTS_DEF_FWSTRUCT_t_InpInfo
typedef struct t_InpInfo t_InpInfo;
#endif
struct t_InpInfo {
  int ask_refresh;
  int refresh;
  LLint stat_bytes;
  int stat_time;
  int lien_n;
  int lien_tot;
  int stat_nsocket;
  int rate;
  int irate;
  int ft;
  LLint stat_written;
  int stat_updated;
  int stat_errors;
  int stat_warnings;
  int stat_infos;
  TStamp stat_timestart;
  int stat_back;
};

int main(int argc, char **argv);
#endif

/* */

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
