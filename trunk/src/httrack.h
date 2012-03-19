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
/* File: htsshow.c console progress info                        */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTSTOOLS_DEFH
#define HTSTOOLS_DEFH 

#if HTS_ANALYSTE_CONSOLE

#include "htsglobal.h"
#include "htscore.h"

typedef struct {
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

typedef struct {
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
int   __cdecl htsshow_checkhtml(char* html,int len,char* url_adresse,char* url_fichier);
int   __cdecl htsshow_loop(lien_back* back,int back_max,int back_index,int lien_n,int lien_tot,int stat_time,hts_stat_struct* stats);
char* __cdecl htsshow_query(char* question);
char* __cdecl htsshow_query2(char* question);
char* __cdecl htsshow_query3(char* question);
int   __cdecl htsshow_check(char* adr,char* fil,int status);
void  __cdecl htsshow_pause(char* lockfile);
void  __cdecl htsshow_filesave(char* file);
int   __cdecl htsshow_linkdetected(char* link);
int   __cdecl htsshow_xfrstatus(lien_back* back);
int   __cdecl htsshow_savename(char* adr_complete,char* fil_complete,char* referer_adr,char* referer_fil,char* save);
  
int main(int argc, char **argv);
void vt_color(int text,int back);
void vt_clear(void);
void vt_home(void);

#endif

#endif

