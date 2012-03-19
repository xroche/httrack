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
/* File: Basic definitions                                      */
/*       Used in .c files for basic (malloc() ..) definitions   */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTS_BASICH
#define HTS_BASICH

#include "htsglobal.h"

// size_t et mode_t
#include <stdio.h>
#if HTS_WIN
#else
#include <fcntl.h>
#endif

#if HTS_WIN
#else
 #define min(a,b) ((a)>(b)?(b):(a))
 #define max(a,b) ((a)>(b)?(a):(b))
#endif

// teste égalité de 2 chars, case insensitive
#define hichar(a) ((((a)>='a') && ((a)<='z')) ? ((a)-('a'-'A')) : (a))
#define streql(a,b) (hichar(a)==hichar(b))

// is this MIME an hypertext MIME (text/html), html/js-style or other script/text type?
#define HTS_HYPERTEXT_DEFAULT_MIME "text/html"
#define is_hypertext_mime(a) \
   ( (strfield2((a),"text/html")!=0)\
  || (strfield2((a),"application/x-javascript")!=0) \
  || (strfield2((a),"text/css")!=0) \
  || (strfield2((a),"image/svg+xml")!=0) \
  || (strfield2((a),"image/svg-xml")!=0) \
  /*|| (strfield2((a),"audio/x-pn-realaudio")!=0) */\
  )

#define may_be_hypertext_mime(a) \
   (\
     (strfield2((a),"audio/x-pn-realaudio")!=0) \
  )


// caractère maj
#define isUpperLetter(a) ( ((a) >= 'A') && ((a) <= 'Z') )

// conversion éventuelle / vers antislash
#if HTS_WIN
char* antislash(char* s);
#else
#define antislash(A) (A)
#endif


// functions
#if HTS_PLATFORM!=3
#ifdef __cplusplus
extern "C" {
#endif
#if HTS_PLATFORM!=2
#if HTS_PLATFORM!=1
 int   open      (const char *, int, ...);
#endif
 //int   read      (int,const char*,int);
 //int   write     (int,char*,int);
#endif
#if HTS_PLATFORM!=1
 int   close     (int);
 void* calloc    (size_t,size_t);
 void* malloc    (size_t);
 void* realloc   (void*,size_t);
 void  free      (void*);
#endif
#if HTS_WIN
#else
 int   mkdir     (const char*,mode_t);
#endif
#ifdef __cplusplus
}
#endif
#endif


// tracer malloc()
#if HTS_TRACE_MALLOC
#define malloct(A)    hts_malloc(A,0)
#define calloct(A,B)  hts_malloc(A,B)
#define freet(A)      hts_free(A)
#define realloct(A,B) hts_realloc(A,B)
void  hts_freeall();
void* hts_malloc    (size_t,size_t);
void  hts_free      (void*);
void* hts_realloc   (void*,size_t);
#else
#define malloct(A)    malloc(A)
#define calloct(A,B)  calloc(A,B)
#define freet(A)      free(A)
#define realloct(A,B) realloc(A,B)
#endif


#endif

