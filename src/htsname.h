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
/* File: httrack.c subroutines:                                 */
/*       savename routine (compute output filename)             */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */



#ifndef HTSNAME_DEFH
#define HTSNAME_DEFH 

#include "htsglobal.h"

#define DELAYED_EXT "delayed"
#define IS_DELAYED_EXT(a) ( ((a) != NULL) && ((a)[0] != 0) && strendwith_(a, "." DELAYED_EXT) )
HTS_STATIC int strendwith_(const char* a, const char* b)  {
  int i, j;
  for(i = 0 ; a[i] != 0 ; i++);
  for(j = 0 ; b[j] != 0 ; j++);
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

// note: 'headers' can either be null, or incomplete (only r member filled)
int url_savename(char* adr_complete, char* fil_complete, char* save,
								 char* former_adr, char* former_fil, 
								 char* referer_adr, char* referer_fil, 
								 httrackp* opt, 
								 lien_url** liens, int lien_tot, 
								 struct_back* sback, 
								 cache_back* cache, 
								 hash_struct* hash, 
								 int ptr, int numero_passe, 
								 const lien_back* headers);
int url_savename2(char* adr_complete, char* fil_complete, char* save,
								  char* former_adr, char* former_fil, 
								  char* referer_adr, char* referer_fil, 
								  httrackp* opt, 
								  lien_url** liens, int lien_tot, 
								  struct_back* sback, 
								  cache_back* cache, 
								  hash_struct* hash, 
								  int ptr, int numero_passe, 
								  const lien_back* headers,
                  const char *charset);
void standard_name(char* b,char* dot_pos,char* nom_pos,char* fil_complete,int short_ver);
void url_savename_addstr(char* d,char* s);
char* url_md5(char* digest_buffer, char* fil_complete);
void url_savename_refname(const char *adr, const char *fil, char *filename);
char *url_savename_refname_fullpath(httrackp* opt, const char *adr, const char *fil);
void url_savename_refname_remove(httrackp* opt, const char *adr, const char *fil);
#endif

#endif
