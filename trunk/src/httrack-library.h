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
/* File: HTTrack definition file for library usage              */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */


#ifndef HTTRACK_DEFLIB
#define HTTRACK_DEFLIB

#include "htsglobal.h"
#include "htsopt.h"
#include "htswrap.h"

/* Main functions */
HTSEXT_API int hts_init(void);
HTSEXT_API int hts_uninit(void);
HTSEXT_API int hts_main(int argc, char **argv);

/* Wrapper functions */
HTSEXT_API int htswrap_init(void);
HTSEXT_API int htswrap_add(char* name,void* fct);
HTSEXT_API int htswrap_free(void);
HTSEXT_API unsigned long int htswrap_read(char* name);
HTSEXT_API const char* hts_is_available(void);

/* Other functions */
HTSEXT_API int hts_resetvar(void);
HTSEXT_API int hts_buildtopindex(httrackp* opt,char* path,char* binpath);
HTSEXT_API char* hts_getcategories(char* path, int type);
HTSEXT_API char* hts_getcategory(char* filename);

/* Catch-URL */
HTSEXT_API T_SOC catch_url_init_std(int* port_prox,char* adr_prox);
HTSEXT_API T_SOC catch_url_init(int* port,char* adr);
HTSEXT_API int catch_url(T_SOC soc,char* url,char* method,char* data);

/* State */
HTSEXT_API int hts_is_parsing(int flag);
HTSEXT_API int hts_is_testing(void);
HTSEXT_API int hts_is_exiting(void);
HTSEXT_API int hts_setopt(httrackp* opt);
HTSEXT_API int hts_addurl(char** url);
HTSEXT_API int hts_resetaddurl(void);
HTSEXT_API int copy_htsopt(httrackp* from, httrackp* to);
HTSEXT_API char* hts_errmsg(void);
HTSEXT_API int hts_setpause(int);      // pause transfer
HTSEXT_API int hts_request_stop(int force);
HTSEXT_API char* hts_cancel_file(char * s);
HTSEXT_API void hts_cancel_test(void);
HTSEXT_API void hts_cancel_parsing(void);
HTSEXT_API char* hts_cancel_file(char * s);
HTSEXT_API void hts_cancel_test(void);
HTSEXT_API void hts_cancel_parsing(void);

/* Tools */
HTSEXT_API int structcheck(char* s);
HTSEXT_API void infostatuscode(char* msg,int statuscode);
HTSEXT_API HTS_INLINE TStamp mtime_local(void);
HTSEXT_API void qsec2str(char *st,TStamp t);
HTSEXT_API char* int2char(int n);
HTSEXT_API char* int2bytes(LLint n);
HTSEXT_API char* int2bytessec(long int n);
HTSEXT_API char** int2bytes2(LLint n);
HTSEXT_API char* jump_identification(char*);
HTSEXT_API char* jump_normalized(char*);
HTSEXT_API char* jump_toport(char*);
HTSEXT_API char* fil_normalized(char* source, char* dest);
HTSEXT_API char* adr_normalized(char* source, char* dest);
HTSEXT_API char* hts_rootdir(char* file);

/* Escaping URLs */
HTSEXT_API void unescape_amp(char* s);
HTSEXT_API void escape_spc_url(char* s);
HTSEXT_API void escape_in_url(char* s);
HTSEXT_API void escape_uri(char* s);
HTSEXT_API void escape_uri_utf(char* s);
HTSEXT_API void escape_check_url(char* s);
HTSEXT_API char* escape_check_url_addr(char* s);
HTSEXT_API void  x_escape_http(char* s,int mode);
HTSEXT_API char* unescape_http(char* s);
HTSEXT_API char* unescape_http_unharm(char* s, int no_high);
HTSEXT_API char* antislash_unescaped(char* s);
HTSEXT_API void  escape_remove_control(char* s);

/* Debugging */
HTSEXT_API void hts_debug(int level);

/* Portable directory API */

typedef struct find_handle_struct find_handle_struct;
typedef find_handle_struct* find_handle;

typedef struct topindex_chain {
  int level;                          /* sort level */
  char* category;                     /* category */
  char name[2048];                    /* path */
  struct topindex_chain* next;        /* next element */
} topindex_chain  ;
HTSEXT_API find_handle hts_findfirst(char* path);
HTSEXT_API int hts_findnext(find_handle find);
HTSEXT_API int hts_findclose(find_handle find);
HTSEXT_API char* hts_findgetname(find_handle find);
HTSEXT_API int hts_findgetsize(find_handle find);
HTSEXT_API int hts_findisdir(find_handle find);
HTSEXT_API int hts_findisfile(find_handle find);
HTSEXT_API int hts_findissystem(find_handle find);

/* Wrapper functions types (commented) : */
/*
typedef void  (* t_hts_htmlcheck_init)(void);
typedef void  (* t_hts_htmlcheck_uninit)(void);
typedef int   (* t_hts_htmlcheck_start)(httrackp* opt);
typedef int   (* t_hts_htmlcheck_end)(void);
typedef int   (* t_hts_htmlcheck_chopt)(httrackp* opt);
typedef int   (* t_hts_htmlcheck)(char* html,int len,char* url_adresse,char* url_fichier);
typedef char* (* t_hts_htmlcheck_query)(char* question);
typedef char* (* t_hts_htmlcheck_query2)(char* question);
typedef char* (* t_hts_htmlcheck_query3)(char* question);
typedef int   (* t_hts_htmlcheck_loop)(lien_back* back,int back_max,int back_index,int lien_tot,int lien_ntot,int stat_time,hts_stat_struct* stats);
typedef int   (* t_hts_htmlcheck_check)(char* adr,char* fil,int status);
typedef void  (* t_hts_htmlcheck_pause)(char* lockfile);
typedef void  (* t_hts_htmlcheck_filesave)(char* file);
typedef int   (* t_hts_htmlcheck_linkdetected)(char* link);
typedef int   (* t_hts_htmlcheck_xfrstatus)(lien_back* back);
typedef int   (* t_hts_htmlcheck_savename)(char* adr_complete,char* fil_complete,char* referer_adr,char* referer_fil,char* save);
typedef int   (* t_hts_htmlcheck_sendhead)(char* buff, char* adr, char* fil, char* referer_adr, char* referer_fil, htsblk* outgoing);
typedef int   (* t_hts_htmlcheck_receivehead)(char* buff, char* adr, char* fil, char* referer_adr, char* referer_fil, htsblk* incoming);
*/

/* Wrapper functions names : */
/*
  hts_htmlcheck_init         = (t_hts_htmlcheck_init)           htswrap_read("init");
Log: "engine: init"

  hts_htmlcheck_uninit       = (t_hts_htmlcheck_uninit)         htswrap_read("free");
Log: "engine: free"

  hts_htmlcheck_start        = (t_hts_htmlcheck_start)          htswrap_read("start");
Log: "engine: start"

  hts_htmlcheck_end          = (t_hts_htmlcheck_end)            htswrap_read("end");
Log: "engine: end"

  hts_htmlcheck_chopt        = (t_hts_htmlcheck_chopt)          htswrap_read("change-options");
Log: "engine: change-options"

  hts_htmlcheck              = (t_hts_htmlcheck)                htswrap_read("check-html");
Log: "check-html: <url>"

  hts_htmlcheck_query        = (t_hts_htmlcheck_query)          htswrap_read("query");
  hts_htmlcheck_query2       = (t_hts_htmlcheck_query2)         htswrap_read("query2");
  hts_htmlcheck_query3       = (t_hts_htmlcheck_query3)         htswrap_read("query3");
  hts_htmlcheck_loop         = (t_hts_htmlcheck_loop)           htswrap_read("loop");
  hts_htmlcheck_check        = (t_hts_htmlcheck_check)          htswrap_read("check-link");
Log: none

  hts_htmlcheck_pause        = (t_hts_htmlcheck_pause)          htswrap_read("pause");
Log: "pause: <lockfile>"

  hts_htmlcheck_filesave     = (t_hts_htmlcheck_filesave)       htswrap_read("save-file");
  hts_htmlcheck_linkdetected = (t_hts_htmlcheck_linkdetected)   htswrap_read("link-detected");
Log: none

  hts_htmlcheck_xfrstatus    = (t_hts_htmlcheck_xfrstatus)      htswrap_read("transfer-status");
Log: 
    "engine: transfer-status: link updated: <url> -> <file>"
  | "engine: transfer-status: link added: <url> -> <file>"
  | "engine: transfer-status: link recorded: <url> -> <file>"
  | "engine: transfer-status: link link error (<errno>, '<err_msg>'): <url>"
  hts_htmlcheck_savename     = (t_hts_htmlcheck_savename  )     htswrap_read("save-name");
Log: 
    "engine: save-name: local name: <url> -> <file>"
*/

#endif
