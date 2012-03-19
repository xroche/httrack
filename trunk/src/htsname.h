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
/*       savename routine (compute output filename)             */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */



#ifndef HTSNAME_DEFH
#define HTSNAME_DEFH 

#include "htscore.h"

#define DELAYED_EXT "delayed"
#define IS_DELAYED_EXT(a) ( ((a) != NULL) && ((a)[0] != 0) && strendwith_(a, "." DELAYED_EXT) )
static int strendwith_(const char* a, const char* b) {
  int i, j;
  for(i = 0 ; a[i] != 0 ; i++);
  for(j = 0 ; b[j] != 0 ; j++);
  while(i >= 0 && j >= 0 && a[i] == b[j]) {
    i--;
    j--;
  }
  return (j == -1);
}


/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE
// note: 'headers' can either be null, or incomplete (only r member filled)
int url_savename(char* adr_complete,char* fil_complete,char* save,char* former_adr,char* former_fil,char* referer_adr,char* referer_fil,httrackp* opt,lien_url** liens,int lien_tot,struct_back* sback,cache_back* cache,hash_struct* hash,int ptr,int numero_passe,const lien_back* headers);
void standard_name(char* b,char* dot_pos,char* nom_pos,char* fil_complete,int short_ver);
void url_savename_addstr(char* d,char* s);
char* url_md5(char* fil_complete);
#endif

#endif
