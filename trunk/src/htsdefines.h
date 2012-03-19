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
/* File: Some defines for httrack.c and others                  */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

// Fichier librairie .h
#ifndef HTS_DEFINES_DEFH
#define HTS_DEFINES_DEFH

typedef void  (* t_hts_htmlcheck_init)(void);
typedef void  (* t_hts_htmlcheck_uninit)(void);
typedef int   (* t_hts_htmlcheck_start)(httrackp* opt);
typedef int   (* t_hts_htmlcheck_end)(void);
typedef int   (* t_hts_htmlcheck_chopt)(httrackp* opt);
typedef int   (* t_hts_htmlcheck_process)(char** html,int* len,char* url_adresse,char* url_fichier);
typedef int   (* t_hts_htmlcheck)(char* html,int len,char* url_adresse,char* url_fichier);
typedef char* (* t_hts_htmlcheck_query)(char* question);
typedef char* (* t_hts_htmlcheck_query2)(char* question);
typedef char* (* t_hts_htmlcheck_query3)(char* question);
typedef int   (* t_hts_htmlcheck_loop)(lien_back* back,int back_max,int back_index,int lien_tot,int lien_ntot,int stat_time,hts_stat_struct* stats);
typedef int   (* t_hts_htmlcheck_check)(char* adr,char* fil,int status);
typedef int   (* t_hts_htmlcheck_check_mime)(char* adr,char* fil,char* mime,int status);
typedef void  (* t_hts_htmlcheck_pause)(char* lockfile);
typedef void  (* t_hts_htmlcheck_filesave)(char* file);
typedef void  (* t_hts_htmlcheck_filesave2)(char* hostname,char* filename,char* localfile,int is_new,int is_modified,int not_updated);
typedef int   (* t_hts_htmlcheck_linkdetected)(char* link);
typedef int   (* t_hts_htmlcheck_linkdetected2)(char* link, char* tag_start);
typedef int   (* t_hts_htmlcheck_xfrstatus)(lien_back* back);
typedef int   (* t_hts_htmlcheck_savename)(char* adr_complete,char* fil_complete,char* referer_adr,char* referer_fil,char* save);
typedef int   (* t_hts_htmlcheck_sendhead)(char* buff, char* adr, char* fil, char* referer_adr, char* referer_fil, htsblk* outgoing);
typedef int   (* t_hts_htmlcheck_receivehead)(char* buff, char* adr, char* fil, char* referer_adr, char* referer_fil, htsblk* incoming);

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE
// demande d'interaction avec le shell
#if HTS_ANALYSTE
extern char HTbuff[2048];
extern t_hts_htmlcheck_init       hts_htmlcheck_init;
extern t_hts_htmlcheck_uninit     hts_htmlcheck_uninit;
extern t_hts_htmlcheck_start      hts_htmlcheck_start;
extern t_hts_htmlcheck_end        hts_htmlcheck_end;
extern t_hts_htmlcheck_chopt      hts_htmlcheck_chopt;
extern t_hts_htmlcheck_process    hts_htmlcheck_preprocess;
extern t_hts_htmlcheck_process    hts_htmlcheck_postprocess;
extern t_hts_htmlcheck            hts_htmlcheck;
extern t_hts_htmlcheck_query      hts_htmlcheck_query;
extern t_hts_htmlcheck_query2     hts_htmlcheck_query2;
extern t_hts_htmlcheck_query3     hts_htmlcheck_query3;
extern t_hts_htmlcheck_loop       hts_htmlcheck_loop;
extern t_hts_htmlcheck_check      hts_htmlcheck_check;
extern t_hts_htmlcheck_check_mime hts_htmlcheck_check_mime;
extern t_hts_htmlcheck_pause      hts_htmlcheck_pause;
extern t_hts_htmlcheck_filesave   hts_htmlcheck_filesave;
extern t_hts_htmlcheck_filesave2  hts_htmlcheck_filesave2;
extern t_hts_htmlcheck_linkdetected hts_htmlcheck_linkdetected;
extern t_hts_htmlcheck_linkdetected2 hts_htmlcheck_linkdetected2;
extern t_hts_htmlcheck_xfrstatus  hts_htmlcheck_xfrstatus;
extern t_hts_htmlcheck_savename   hts_htmlcheck_savename;
extern t_hts_htmlcheck_sendhead   hts_htmlcheck_sendhead;
extern t_hts_htmlcheck_receivehead hts_htmlcheck_receivehead;
#endif

#if HTS_ANALYSTE
#define HT_PRINT(A) strcatbuff(HTbuff,A);
#define HT_REQUEST_START HTbuff[0]='\0';
#define HT_REQUEST_END 
#define HTT_REQUEST_START HTbuff[0]='\0';
#define HTT_REQUEST_END 
#define HTS_REQUEST_START HTbuff[0]='\0';
#define HTS_REQUEST_END 
#define HTS_PANIC_PRINTF(S) strcpybuff(_hts_errmsg,S);
#else
#define HT_PRINT(A) printf("%s",A);
#define HT_REQUEST_START /*printf("§\n");*/
#define HT_REQUEST_END   /*printf("§\n");*/
#define HTT_REQUEST_START /*if (httrack.shell) printf("§\n");*/
#define HTT_REQUEST_END   /*if (httrack.shell) printf("§\n");*/
#define HTS_REQUEST_START if (opt->shell) { HT_REQUEST_START }
#define HTS_REQUEST_END   if (opt->shell) { HT_REQUEST_END }
#define HTS_PANIC_PRINTF(S) printf("%s\n",S);
#endif

#endif

#endif

