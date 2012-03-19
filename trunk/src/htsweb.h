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
/* File: webhttrack.c routines                                  */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef WEBHTTRACK_WBC
#define WEBHTTRACK_WBC

#include "htsglobal.h"
#include "htscore.h"

#define NStatsBuffer     14
#define MAX_LEN_INPROGRESS 40

typedef struct t_StatsBuffer {
  char name[1024];
  char file[1024];
  char state[256];
  char url_sav[HTS_URLMAXSIZE*2];    // pour cancel
  char url_adr[HTS_URLMAXSIZE*2];
  char url_fil[HTS_URLMAXSIZE*2];
  LLint size;
  LLint sizetot;
  int offset;
  //
  int back;
  //
  int actived;    // pour disabled
} t_StatsBuffer;

typedef struct t_InpInfo {
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
} t_InpInfo;

// wrappers
void  __cdecl htsshow_init(void);
void  __cdecl htsshow_uninit(void);
int   __cdecl htsshow_start(httrackp* opt);
int   __cdecl htsshow_chopt(httrackp* opt);
int   __cdecl htsshow_end(void);
int   __cdecl htsshow_preprocesshtml(char** html,int* len,char* url_adresse,char* url_fichier);
int   __cdecl htsshow_checkhtml(char* html,int len,char* url_adresse,char* url_fichier);
int   __cdecl htsshow_loop(lien_back* back,int back_max,int back_index,int lien_n,int lien_tot,int stat_time,hts_stat_struct* stats);
char* __cdecl htsshow_query(char* question);
char* __cdecl htsshow_query2(char* question);
char* __cdecl htsshow_query3(char* question);
int   __cdecl htsshow_check(char* adr,char* fil,int status);
int   __cdecl htsshow_check_mime(char* adr,char* fil,char* mime,int status);
void  __cdecl htsshow_pause(char* lockfile);
void  __cdecl htsshow_filesave(char* file);
void  __cdecl htsshow_filesave2(char* adr, char* fil, char* save, int is_new, int is_modified, int not_updated);
int   __cdecl htsshow_linkdetected(char* link);
int   __cdecl htsshow_linkdetected2(char* link, char* start_tag);
int   __cdecl htsshow_xfrstatus(lien_back* back);
int   __cdecl htsshow_savename(char* adr_complete,char* fil_complete,char* referer_adr,char* referer_fil,char* save);
int   __cdecl htsshow_sendheader(char* buff, char* adr, char* fil, char* referer_adr, char* referer_fil, htsblk* outgoing);
int   __cdecl htsshow_receiveheader(char* buff, char* adr, char* fil, char* referer_adr, char* referer_fil, htsblk* incoming);
  
int main(int argc, char **argv);
void webhttrack_main(char* cmd);
void webhttrack_lock(int lock);

#endif
