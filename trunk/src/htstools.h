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
/* File: httrack.c subroutines:                                 */
/*       various tools (filename analyzing ..)                  */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */


#ifndef HTSTOOLS_DEFH
#define HTSTOOLS_DEFH 

/* specific definitions */
#include <stdio.h>
#include <stdlib.h>
#include "htsbase.h"
#include "htscore.h"

#ifdef _WIN32
#else
#include <dirent.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/stat.h>
#endif

int ident_url_relatif(char *lien,char* urladr,char* urlfil,char* adr,char* fil);
int lienrelatif(char* s,char* link,char* curr);
int link_has_authority(char* lien);
int link_has_authorization(char* lien);
void long_to_83(int mode,char* n83,char* save);
void longfile_to_83(int mode,char* n83,char* save);
HTS_INLINE int __rech_tageq(const char* adr,const char* s);
HTS_INLINE int __rech_tageqbegdigits(const char* adr,const char* s);
#define rech_tageq(adr,s) \
  ( \
    ( (*((adr)-1)=='<') || (is_space(*((adr)-1))) ) ? \
    ( \
      (streql(*(adr),*(s))) ?   \
      (__rech_tageq((adr),(s))) \
      : 0                       \
    ) \
    : 0\
  )
#define rech_tageqbegdigits(adr,s) \
  ( \
    ( (*((adr)-1)=='<') || (is_space(*((adr)-1))) ) ? \
    ( \
      (streql(*(adr),*(s))) ?   \
      (__rech_tageqbegdigits((adr),(s))) \
      : 0                       \
    ) \
    : 0\
  )
//HTS_INLINE int rech_tageq(const char* adr,const char* s);
HTS_INLINE int rech_sampletag(const char* adr,const char* s);
HTS_INLINE int check_tag(char* from,const char* tag);
int verif_backblue(httrackp* opt,char* base);
int verif_external(int nb,int test);

int istoobig(LLint size,LLint maxhtml,LLint maxnhtml,char* type);

#ifndef HTTRACK_DEFLIB
HTSEXT_API int hts_buildtopindex(httrackp* opt,char* path,char* binpath);
#endif


// Portable directory find functions

#ifndef HTTRACK_DEFLIB
#ifdef _WIN32
typedef struct {
  WIN32_FIND_DATA hdata;
  HANDLE handle;
} find_handle_struct;
#else
typedef struct {
  DIR * hdir;
  struct dirent* dirp;
  struct stat filestat;
  char path[2048];
} find_handle_struct;
#endif
typedef find_handle_struct* find_handle;
typedef struct topindex_chain {
  char name[2048];                    /* path */
  struct topindex_chain* next;        /* next element */
} topindex_chain  ;
// Directory find functions
HTSEXT_API find_handle hts_findfirst(char* path);
HTSEXT_API int hts_findnext(find_handle find);
HTSEXT_API int hts_findclose(find_handle find);
//
HTSEXT_API char* hts_findgetname(find_handle find);
HTSEXT_API int hts_findgetsize(find_handle find);
HTSEXT_API int hts_findisdir(find_handle find);
HTSEXT_API int hts_findisfile(find_handle find);
HTSEXT_API int hts_findissystem(find_handle find);
#endif

#endif
