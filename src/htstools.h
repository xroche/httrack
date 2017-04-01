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
#include "htsglobal.h"

/* Forward definitions */
#ifndef HTS_DEF_FWSTRUCT_httrackp
#define HTS_DEF_FWSTRUCT_httrackp
typedef struct httrackp httrackp;
#endif
#ifndef HTS_DEF_FWSTRUCT_find_handle_struct
#define HTS_DEF_FWSTRUCT_find_handle_struct
typedef struct find_handle_struct find_handle_struct;
typedef find_handle_struct *find_handle;
#endif
#ifndef HTS_DEF_FWSTRUCT_lien_adrfil
#define HTS_DEF_FWSTRUCT_lien_adrfil
typedef struct lien_adrfil lien_adrfil;
#endif
#ifndef HTS_DEF_FWSTRUCT_lien_adrfilsave
#define HTS_DEF_FWSTRUCT_lien_adrfilsave
typedef struct lien_adrfilsave lien_adrfilsave;
#endif

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE
int ident_url_relatif(const char *lien, const char *origin_adr,
                      const char *origin_fil,
                      lien_adrfil* const adrfil);
int lienrelatif(char *s, const char *link, const char *curr);
int link_has_authority(const char *lien);
int link_has_authorization(const char *lien);
void long_to_83(int mode, char *n83, char *save);
void longfile_to_83(int mode, char *n83, char *save);
HTS_INLINE int __rech_tageq(const char *adr, const char *s);
HTS_INLINE int __rech_tageqbegdigits(const char *adr, const char *s);
HTS_INLINE int rech_tageq_all(const char *adr, const char *s);

int hts_template_format(FILE *const out, const char *format, ...);
int hts_template_format_str(char *buffer, size_t size, const char *format, ...);

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
HTS_INLINE int rech_sampletag(const char *adr, const char *s);
HTS_INLINE int rech_endtoken(const char *adr, const char **start);
HTS_INLINE int check_tag(const char *from, const char *tag);
int verif_backblue(httrackp * opt, const char *base);
int verif_external(httrackp * opt, int nb, int test);

int istoobig(httrackp * opt, LLint size, LLint maxhtml, LLint maxnhtml,
             char *type);
HTSEXT_API int hts_buildtopindex(httrackp * opt, const char *path,
                                 const char *binpath);

// Portable directory find functions
// Directory find functions
HTSEXT_API find_handle hts_findfirst(char *path);
HTSEXT_API int hts_findnext(find_handle find);
HTSEXT_API int hts_findclose(find_handle find);

//
HTSEXT_API char *hts_findgetname(find_handle find);
HTSEXT_API int hts_findgetsize(find_handle find);
HTSEXT_API int hts_findisdir(find_handle find);
HTSEXT_API int hts_findisfile(find_handle find);
HTSEXT_API int hts_findissystem(find_handle find);

#endif

#endif
