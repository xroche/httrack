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

#ifndef HTS_DEF_FWSTRUCT_httrackp
#define HTS_DEF_FWSTRUCT_httrackp
typedef struct httrackp httrackp;
#endif
#ifndef HTS_DEF_FWSTRUCT_strc_int2bytes2
#define HTS_DEF_FWSTRUCT_strc_int2bytes2
typedef struct strc_int2bytes2 strc_int2bytes2;
#endif

/* Helpers for plugging callbacks
requires: htsdefines.h */

/*
Add a function callback 'FUNCTION' to the option structure 'OPT' callback member 'MEMBER', 
with an optional (may be NULL) argument 'ARGUMENT'
*/
#define CHAIN_FUNCTION(OPT, MEMBER, FUNCTION, ARGUMENT) do { \
  t_hts_callbackarg *carg = (t_hts_callbackarg*) hts_malloc(sizeof(t_hts_callbackarg)); \
  carg->userdef = ( ARGUMENT ); \
  carg->prev.fun = (void*) ( OPT )->callbacks_fun-> MEMBER .fun; \
  carg->prev.carg = ( OPT )->callbacks_fun-> MEMBER .carg; \
  ( OPT )->callbacks_fun-> MEMBER .fun = ( FUNCTION ); \
  ( OPT )->callbacks_fun-> MEMBER .carg = carg; \
} while(0)

/* The following helpers are useful only if you know that an existing callback migh be existing before before the call to CHAIN_FUNCTION()
If your functions were added just after hts_create_opt(), no need to make the previous function check */

/* Get the user-defined pointer initially passed to CHAIN_FUNCTION(), given the callback's carg argument */
#define CALLBACKARG_USERDEF(CARG) ( ( (CARG) != NULL ) ? (CARG)->userdef : NULL )

/* Get the previously existing function before the call to CHAIN_FUNCTION(), given the callback's carg argument */
#define CALLBACKARG_PREV_FUN(CARG, NAME) ( (t_hts_htmlcheck_ ##NAME) ( ( (CARG) != NULL ) ? (CARG)->prev.fun : NULL ) )

/* Get the previously existing function argument before the call to CHAIN_FUNCTION(), given the callback's carg argument */
#define CALLBACKARG_PREV_CARG(CARG) ( ( (CARG) != NULL ) ? (CARG)->prev.carg : NULL )

/* Functions */

/* Initialization */
HTSEXT_API int hts_init(void);
HTSEXT_API int hts_uninit(void);
HTSEXT_API void htsthread_wait(void);

/* Main functions */
HTSEXT_API int hts_main(int argc, char **argv);
HTSEXT_API int hts_main2(int argc, char **argv, httrackp *opt);

/* Options handling */
HTSEXT_API httrackp* hts_create_opt(void);
HTSEXT_API void hts_free_opt(httrackp *opt);
HTSEXT_API void set_wrappers(httrackp *opt);	// DEPRECATED - DUMMY FUNCTION
HTSEXT_API int plug_wrapper(httrackp *opt, const char *moduleName, const char* argv);

/* Logging */
HTSEXT_API int hts_log(httrackp *opt, const char* prefix, const char *msg);

/* Infos */
HTSEXT_API const char* hts_get_version_info(httrackp *opt);
HTSEXT_API const char* hts_is_available(void);

/* Wrapper functions */
HTSEXT_API int htswrap_init(void);	// DEPRECATED - DUMMY FUNCTION
HTSEXT_API int htswrap_free(void);	// DEPRECATED - DUMMY FUNCTION
HTSEXT_API int htswrap_add(httrackp *opt, const char* name, void* fct);
HTSEXT_API unsigned long int htswrap_read(httrackp *opt, const char* name);
HTSEXT_API int htswrap_set_userdef(httrackp *opt, void *userdef);
HTSEXT_API void* htswrap_get_userdef(httrackp *opt);

/* Internal library allocators, if a different libc is being used by the client */
HTSEXT_API char* hts_strdup(const char* string);
HTSEXT_API void* hts_malloc(size_t size);
HTSEXT_API void* hts_realloc(void* data, size_t size);
HTSEXT_API void hts_free(void* data);

/* Other functions */
HTSEXT_API int hts_resetvar(void);				// DEPRECATED - DUMMY FUNCTION
HTSEXT_API int hts_buildtopindex(httrackp* opt,const char* path,const char* binpath);
HTSEXT_API const char* hts_getcategories(const char* path, int type);
HTSEXT_API const char* hts_getcategory(const char* filename);

/* Catch-URL */
HTSEXT_API T_SOC catch_url_init_std(int* port_prox,char* adr_prox);
HTSEXT_API T_SOC catch_url_init(int* port,char* adr);
HTSEXT_API int catch_url(T_SOC soc,char* url,char* method,char* data);

/* State */
HTSEXT_API int hts_is_parsing(httrackp *opt, int flag);
HTSEXT_API int hts_is_testing(httrackp *opt);
HTSEXT_API int hts_is_exiting(httrackp *opt);
/*HTSEXT_API int hts_setopt(httrackp* opt); DEPRECATED ; see copy_htsopt() */
HTSEXT_API int hts_addurl(httrackp *opt, char** url);
HTSEXT_API int hts_resetaddurl(httrackp *opt);
HTSEXT_API int copy_htsopt(httrackp* from, httrackp* to);
HTSEXT_API char* hts_errmsg(httrackp *opt);
HTSEXT_API int hts_setpause(httrackp *opt, int);      // pause transfer
HTSEXT_API int hts_request_stop(httrackp* opt, int force);
HTSEXT_API int hts_cancel_file_push(httrackp *opt, const char *url);
HTSEXT_API void hts_cancel_test(httrackp *opt);
HTSEXT_API void hts_cancel_parsing(httrackp *opt);
HTSEXT_API void hts_cancel_test(httrackp *opt);
HTSEXT_API void hts_cancel_parsing(httrackp *opt);

/* Tools */
HTSEXT_API int structcheck(const char* path);
HTSEXT_API int dir_exists(const char* path);
HTSEXT_API void infostatuscode(char* msg,int statuscode);
HTSEXT_API HTS_INLINE TStamp mtime_local(void);
HTSEXT_API void qsec2str(char *st,TStamp t);
HTSEXT_API char* int2char(strc_int2bytes2* strc, int n);
HTSEXT_API char* int2bytes(strc_int2bytes2* strc, LLint n);
HTSEXT_API char* int2bytessec(strc_int2bytes2* strc, long int n);
HTSEXT_API char** int2bytes2(strc_int2bytes2* strc, LLint n);
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
HTSEXT_API char* unescape_http(char *catbuff, const char* s);
HTSEXT_API char* unescape_http_unharm(char *catbuff, const char* s, int no_high);
HTSEXT_API char* antislash_unescaped(char *catbuff, const char* s);
HTSEXT_API void  escape_remove_control(char* s);
HTSEXT_API void  get_httptype(httrackp *opt,char *s,const char *fil,int flag);
HTSEXT_API int is_knowntype(httrackp *opt,const char *fil);
HTSEXT_API int is_userknowntype(httrackp *opt,const char *fil);
HTSEXT_API int is_dyntype(const char *fil);
HTSEXT_API char* get_ext(char *catbuff, const char *fil);

/* Ugly string tools */
HTSEXT_API char* concat(char *catbuff,const char* a,const char* b) ;
HTSEXT_API char* fconcat(char *catbuff, const char* a, const char* b);
HTSEXT_API char* fconv(char *catbuff, const char* a);

/* Debugging */
HTSEXT_API void hts_debug(int level);

/* Portable directory API */

#ifndef HTS_DEF_FWSTRUCT_find_handle_struct
#define HTS_DEF_FWSTRUCT_find_handle_struct
typedef struct find_handle_struct find_handle_struct;
typedef find_handle_struct* find_handle;
#endif

#ifndef HTS_DEF_FWSTRUCT_topindex_chain
#define HTS_DEF_FWSTRUCT_topindex_chain
typedef struct topindex_chain topindex_chain;
#endif
struct topindex_chain {
  int level;                          /* sort level */
  char* category;                     /* category */
  char name[2048];                    /* path */
  struct topindex_chain* next;        /* next element */
};
HTSEXT_API find_handle hts_findfirst(char* path);
HTSEXT_API int hts_findnext(find_handle find);
HTSEXT_API int hts_findclose(find_handle find);
HTSEXT_API char* hts_findgetname(find_handle find);
HTSEXT_API int hts_findgetsize(find_handle find);
HTSEXT_API int hts_findisdir(find_handle find);
HTSEXT_API int hts_findisfile(find_handle find);
HTSEXT_API int hts_findissystem(find_handle find);

/* UTF-8 aware FILE API */
#ifndef HTS_DEF_FILEAPI
#ifdef _WIN32
#define FOPEN hts_fopen_utf8
HTSEXT_API FILE* hts_fopen_utf8(const char *path, const char *mode);
#define STAT hts_stat_utf8
typedef struct _stat STRUCT_STAT;
HTSEXT_API int hts_stat_utf8(const char *path, STRUCT_STAT *buf);
#define UNLINK hts_unlink_utf8
HTSEXT_API int hts_unlink_utf8(const char *pathname);
#define RENAME hts_rename_utf8
HTSEXT_API int hts_rename_utf8(const char *oldpath, const char *newpath);
#define MKDIR(F) hts_mkdir_utf8(F)
HTSEXT_API int hts_mkdir_utf8(const char *pathname);
#define UTIME(A,B) hts_utime_utf8(A,B)
typedef struct _utimbuf STRUCT_UTIMBUF;
HTSEXT_API int hts_utime_utf8(const char *filename, const STRUCT_UTIMBUF *times);
#else
#define FOPEN fopen
#define STAT stat
typedef struct stat STRUCT_STAT;
#define UNLINK unlink
#define RENAME rename
#define MKDIR(F) mkdir(F, HTS_ACCESS_FOLDER)
typedef struct utimbuf STRUCT_UTIMBUF;
#define UTIME(A,B) utime(A,B)
#endif
#define HTS_DEF_FILEAPI
#endif

#endif
