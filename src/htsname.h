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
/*       savename routine (compute output filename)             */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTSNAME_DEFH
#define HTSNAME_DEFH

#include "htsglobal.h"

#define DELAYED_EXT "delayed"
#define IS_DELAYED_EXT(a) ( ((a) != NULL) && ((a)[0] != 0) && strendwith_(a, "." DELAYED_EXT) )
HTS_STATIC int strendwith_(const char *a, const char *b) {
  int i, j;

  for(i = 0; a[i] != 0; i++) ;
  for(j = 0; b[j] != 0; j++) ;
  while(i >= 0 && j >= 0 && a[i] == b[j]) {
    i--;
    j--;
  }
  return (j == -1);
}

#define CACHE_REFNAME "hts-cache/ref"

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE

/* Forward definitions */
#ifndef HTS_DEF_FWSTRUCT_httrackp
#define HTS_DEF_FWSTRUCT_httrackp
typedef struct httrackp httrackp;
#endif
#ifndef HTS_DEF_FWSTRUCT_lien_url
#define HTS_DEF_FWSTRUCT_lien_url
typedef struct lien_url lien_url;
#endif
#ifndef HTS_DEF_FWSTRUCT_struct_back
#define HTS_DEF_FWSTRUCT_struct_back
typedef struct struct_back struct_back;
#endif
#ifndef HTS_DEF_FWSTRUCT_cache_back
#define HTS_DEF_FWSTRUCT_cache_back
typedef struct cache_back cache_back;
#endif
#ifndef HTS_DEF_FWSTRUCT_hash_struct
#define HTS_DEF_FWSTRUCT_hash_struct
typedef struct hash_struct hash_struct;
#endif
#ifndef HTS_DEF_FWSTRUCT_lien_back
#define HTS_DEF_FWSTRUCT_lien_back
typedef struct lien_back lien_back;
#endif
#ifndef HTS_DEF_FWSTRUCT_lien_adrfil
#define HTS_DEF_FWSTRUCT_lien_adrfil
typedef struct lien_adrfil lien_adrfil;
#endif
#ifndef HTS_DEF_FWSTRUCT_lien_adrfilsave
#define HTS_DEF_FWSTRUCT_lien_adrfilsave
typedef struct lien_adrfilsave lien_adrfilsave;
#endif

// note: 'headers' can either be null, or incomplete (only r member filled)
int url_savename(lien_adrfilsave *const afs,
                 lien_adrfil *const former,
                 const char *referer_adr, const char *referer_fil, 
                 httrackp * opt, struct_back * sback, cache_back * cache,
                 hash_struct * hash, int ptr, int numero_passe,
                 const lien_back * headers);
void standard_name(char *b, const char *dot_pos, const char *nom_pos,
                   const char *fil_complete,
                   int short_ver);
void url_savename_addstr(char *d, const char *s);
char *url_md5(char *digest_buffer, const char *fil_complete);
void url_savename_refname(const char *adr, const char *fil, char *filename);
char *url_savename_refname_fullpath(httrackp * opt, const char *adr,
                                    const char *fil);
void url_savename_refname_remove(httrackp * opt, const char *adr,
                                 const char *fil);
#endif

#endif
