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
/*       backing system (multiple socket download)              */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */


#ifndef HTSBACK_DEFH
#define HTSBACK_DEFH 

#include "htsglobal.h"
#include "htsbasenet.h"
#include "htscore.h"

// backing
#define BACK_ADD_TEST "(dummy)"
#define BACK_ADD_TEST2 "(dummy2)"
int  back_index(lien_back* back,int back_max,char* adr,char* fil,char* sav);
int back_available(lien_back* back,int back_max);
LLint back_incache(lien_back* back,int back_max);
HTS_INLINE int  back_exist(lien_back* back,int back_max,char* adr,char* fil,char* sav);
int  back_nsoc(lien_back* back,int back_max);
int  back_add(lien_back* back,int back_max,httrackp* opt,cache_back* cache,char* adr,char* fil,char* save,char* referer_adr,char* referer_fil,int test,short int* pass2_ptr);
int  back_stack_available(lien_back* back,int back_max);
void back_clean(httrackp* opt,cache_back* cache,lien_back* back,int back_max);
void back_wait(lien_back* back,int back_max,httrackp* opt,cache_back* cache,TStamp stat_timestart);
int  back_delete(lien_back* back,int p);
int  back_finalize(httrackp* opt,cache_back* cache,lien_back* back,int p);
void back_info(lien_back* back,int i,int j,FILE* fp);
void back_infostr(lien_back* back,int i,int j,char* s);
LLint  back_transfered(LLint add,lien_back* back,int back_max);
// hostback
#if HTS_XGETHOST
void back_solve(lien_back* back);
int host_wait(lien_back* back);
#endif
int back_checksize(httrackp* opt,lien_back* eback,int check_only_totalsize);

#if HTS_XGETHOST
#if USE_BEGINTHREAD
PTHREAD_TYPE Hostlookup(void* iadr_p);
#endif
#endif

#endif
