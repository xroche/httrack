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
/*       cache system (index and stores files in cache)         */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */



#ifndef HTSCACHE_DEFH
#define HTSCACHE_DEFH 

#include "htscore.h"

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE

// cache
void cache_mayadd(httrackp* opt,cache_back* cache,htsblk* r,char* url_adr,char* url_fil,char* url_save);
void cache_add(cache_back* cache,htsblk r,char* url_adr,char* url_fil,char* url_save,int all_in_cache);
htsblk cache_read(httrackp* opt,cache_back* cache,char* adr,char* fil,char* save,char* location);
htsblk cache_read_ro(httrackp* opt,cache_back* cache,char* adr,char* fil,char* save,char* location);
htsblk cache_readex(httrackp* opt,cache_back* cache,char* adr,char* fil,char* save,char* location,char* return_save,int readonly);
htsblk* cache_header(httrackp* opt,cache_back* cache,char* adr,char* fil,htsblk* r);
void cache_init(cache_back* cache,httrackp* opt);

int cache_writedata(FILE* cache_ndx,FILE* cache_dat,char* str1,char* str2,char* outbuff,int len);
int cache_readdata(cache_back* cache,char* str1,char* str2,char** inbuff,int* len);

int cache_wstr(FILE* fp,char* s);
void cache_rstr(FILE* fp,char* s);
char* cache_rstr_addr(FILE* fp);
int  cache_brstr(char* adr,char* s);
int  cache_quickbrstr(char* adr,char* s);
int cache_brint(char* adr,int* i);
void cache_rint(FILE* fp,int* i);
int cache_wint(FILE* fp,int i);
void cache_rLLint(FILE* fp,LLint* i);
int cache_wLLint(FILE* fp,LLint i);

#endif

#endif
