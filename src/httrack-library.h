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
/* File: HTTrack definition file for library usage              */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTTRACK_DEFLIB
#define HTTRACK_DEFLIB

#ifdef __cplusplus
extern "C" {
#endif

#include "htsglobal.h"

#ifndef _WIN32
#include <inttypes.h>
#endif
#include <stdarg.h>

#ifndef HTS_DEF_FWSTRUCT_httrackp
#define HTS_DEF_FWSTRUCT_httrackp
typedef struct httrackp httrackp;
#endif
#ifndef HTS_DEF_FWSTRUCT_strc_int2bytes2
#define HTS_DEF_FWSTRUCT_strc_int2bytes2
typedef struct strc_int2bytes2 strc_int2bytes2;
#endif
#ifndef HTS_DEF_DEFSTRUCT_hts_log_type
#define HTS_DEF_DEFSTRUCT_hts_log_type
typedef enum hts_log_type {
  LOG_PANIC,
  LOG_ERROR,
  LOG_WARNING,
  LOG_NOTICE,
  LOG_INFO,
  LOG_DEBUG,
  LOG_TRACE,
  LOG_ERRNO = 1 << 8
} hts_log_type;
#endif
#ifndef HTS_DEF_FWSTRUCT_hts_stat_struct
#define HTS_DEF_FWSTRUCT_hts_stat_struct
typedef struct hts_stat_struct hts_stat_struct;
#endif

/** Assert error callback. **/
#ifndef HTS_DEF_FWSTRUCT_htsErrorCallback
#define HTS_DEF_FWSTRUCT_htsErrorCallback
typedef void (*htsErrorCallback) (const char *msg, const char *file, int line);
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
HTSEXT_API int hts_main2(int argc, char **argv, httrackp * opt);

/* Options handling */
HTSEXT_API httrackp *hts_create_opt(void);
HTSEXT_API void hts_free_opt(httrackp * opt);
HTSEXT_API size_t hts_sizeof_opt(void);
HTSEXT_API const hts_stat_struct* hts_get_stats(httrackp * opt);
HTSEXT_API void set_wrappers(httrackp * opt);   /* LEGACY */
HTSEXT_API int plug_wrapper(httrackp * opt, const char *moduleName,
                            const char *argv);
HTSEXT_API void hts_set_error_callback(htsErrorCallback handler);
HTSEXT_API htsErrorCallback hts_get_error_callback(void);

/* Logging */
HTSEXT_API int hts_log(httrackp * opt, const char *prefix, const char *msg);
HTSEXT_API void hts_log_print(httrackp * opt, int type, const char *format,
                              ...) HTS_PRINTF_FUN(3, 4);
HTSEXT_API void hts_log_vprint(httrackp * opt, int type, const char *format,
                               va_list args);
HTSEXT_API void hts_set_log_vprint_callback(void (*callback)(httrackp * opt, 
                                           int type, 
                                           const char *format, va_list args));

/* Infos */
HTSEXT_API const char *hts_get_version_info(httrackp * opt);
HTSEXT_API const char *hts_is_available(void);
HTSEXT_API const char* hts_version(void);
HTSEXT_API const hts_stat_struct* hts_get_stats(httrackp * opt);

/* Wrapper functions */
HTSEXT_API int htswrap_init(void);      // DEPRECATED - DUMMY FUNCTION
HTSEXT_API int htswrap_free(void);      // DEPRECATED - DUMMY FUNCTION
HTSEXT_API int htswrap_add(httrackp * opt, const char *name, void *fct);
HTSEXT_API uintptr_t htswrap_read(httrackp * opt, const char *name);
HTSEXT_API int htswrap_set_userdef(httrackp * opt, void *userdef);
HTSEXT_API void *htswrap_get_userdef(httrackp * opt);

/* Internal library allocators, if a different libc is being used by the client */
HTSEXT_API char *hts_strdup(const char *string);
HTSEXT_API void *hts_malloc(size_t size);
HTSEXT_API void *hts_realloc(void *const data, const size_t size);
HTSEXT_API void hts_free(void *data);

/* Other functions */
HTSEXT_API int hts_resetvar(void);      // DEPRECATED - DUMMY FUNCTION
HTSEXT_API int hts_buildtopindex(httrackp * opt, const char *path,
                                 const char *binpath);
HTSEXT_API char *hts_getcategories(char *path, int type);
HTSEXT_API char *hts_getcategory(const char *filename);

/* Catch-URL */
HTSEXT_API T_SOC catch_url_init_std(int *port_prox, char *adr_prox);
HTSEXT_API T_SOC catch_url_init(int *port, char *adr);
HTSEXT_API int catch_url(T_SOC soc, char *url, char *method, char *data);

/* State */
HTSEXT_API int hts_is_parsing(httrackp * opt, int flag);
HTSEXT_API int hts_is_testing(httrackp * opt);
HTSEXT_API int hts_is_exiting(httrackp * opt);

/*HTSEXT_API int hts_setopt(httrackp* opt); DEPRECATED ; see copy_htsopt() */

HTSEXT_API int hts_addurl(httrackp * opt, char **url);
HTSEXT_API int hts_resetaddurl(httrackp * opt);
HTSEXT_API int copy_htsopt(const httrackp * from, httrackp * to);
HTSEXT_API char *hts_errmsg(httrackp * opt);
HTSEXT_API int hts_setpause(httrackp * opt, int);
HTSEXT_API int hts_request_stop(httrackp * opt, int force);
HTSEXT_API int hts_cancel_file_push(httrackp * opt, const char *url);
HTSEXT_API void hts_cancel_test(httrackp * opt);
HTSEXT_API void hts_cancel_parsing(httrackp * opt);
HTSEXT_API void hts_cancel_test(httrackp * opt);
HTSEXT_API void hts_cancel_parsing(httrackp * opt);
HTSEXT_API int hts_has_stopped(httrackp * opt);

/* Tools */
HTSEXT_API int structcheck(const char *path);
HTSEXT_API int structcheck_utf8(const char *path);
HTSEXT_API int dir_exists(const char *path);
HTSEXT_API void infostatuscode(char *msg, int statuscode);
HTSEXT_API TStamp mtime_local(void);
HTSEXT_API void qsec2str(char *st, TStamp t);
HTSEXT_API char *int2char(strc_int2bytes2 * strc, int n);
HTSEXT_API char *int2bytes(strc_int2bytes2 * strc, LLint n);
HTSEXT_API char *int2bytessec(strc_int2bytes2 * strc, long int n);
HTSEXT_API char **int2bytes2(strc_int2bytes2 * strc, LLint n);
HTSEXT_API char *jump_identification(char *);
HTSEXT_API const char *jump_identification_const(const char *);
HTSEXT_API char *jump_normalized(char *);
HTSEXT_API const char *jump_normalized_const(const char *);
HTSEXT_API char *jump_toport(char *);
HTSEXT_API const char *jump_toport_const(const char *);
HTSEXT_API char *fil_normalized(const char *source, char *dest);
HTSEXT_API char *adr_normalized(const char *source, char *dest);
HTSEXT_API const char *hts_rootdir(char *file);

/* Escaping URLs */
HTSEXT_API void unescape_amp(char *s);

HTSEXT_API size_t escape_spc_url(const char *const src, char *const dest, const size_t size);
HTSEXT_API size_t escape_in_url(const char *const src, char *const dest, const size_t size);
HTSEXT_API size_t escape_uri(const char *const src, char *const dest, const size_t size);
HTSEXT_API size_t escape_uri_utf(const char *const src, char *const dest, const size_t size);
HTSEXT_API size_t escape_check_url(const char *const src, char *const dest, const size_t size);

HTSEXT_API size_t append_escape_spc_url(const char *const src, char *const dest, const size_t size);
HTSEXT_API size_t append_escape_in_url(const char *const src, char *const dest, const size_t size);
HTSEXT_API size_t append_escape_uri(const char *const src, char *const dest, const size_t size);
HTSEXT_API size_t append_escape_uri_utf(const char *const src, char *const dest, const size_t size);
HTSEXT_API size_t append_escape_check_url(const char *const src, char *const dest, const size_t size);

HTSEXT_API size_t inplace_escape_spc_url(char *const dest, const size_t size);
HTSEXT_API size_t inplace_escape_in_url(char *const dest, const size_t size);
HTSEXT_API size_t inplace_escape_uri(char *const dest, const size_t size);
HTSEXT_API size_t inplace_escape_uri_utf(char *const dest, const size_t size);
HTSEXT_API size_t inplace_escape_check_url(char *const dest, const size_t size);

HTSEXT_API char *escape_check_url_addr(const char *const src, char *const dest, const size_t size);
HTSEXT_API size_t make_content_id(const char *const adr, const char *const fil, char *const dest, const size_t size);

HTSEXT_API size_t x_escape_http(const char *const s, char *const dest, const size_t max_size, const int mode);
HTSEXT_API void escape_remove_control(char *const s);
HTSEXT_API size_t escape_for_html_print(const char *const s, char *const dest, const size_t size);
HTSEXT_API size_t escape_for_html_print_full(const char *const s, char *const dest, const size_t size);

HTSEXT_API char *unescape_http(char *const catbuff, const size_t size, const char *const s);
HTSEXT_API char *unescape_http_unharm(char *const catbuff, const size_t size, const char *s, const int no_high);
HTSEXT_API char *antislash_unescaped(char *catbuff, const char *s);

HTSEXT_API void escape_remove_control(char *s);
HTSEXT_API void get_httptype(httrackp * opt, char *s, const char *fil,
                             int flag);
HTSEXT_API int is_knowntype(httrackp * opt, const char *fil);
HTSEXT_API int is_userknowntype(httrackp * opt, const char *fil);
HTSEXT_API int is_dyntype(const char *fil);
HTSEXT_API const char *get_ext(char *catbuff, size_t size, const char *fil);
HTSEXT_API int may_unknown(httrackp * opt, const char *st);
HTSEXT_API void guess_httptype(httrackp * opt, char *s, const char *fil);

/* Ugly string tools */
HTSEXT_API char *concat(char *catbuff, size_t size, const char *a, const char *b);
HTSEXT_API char *fconcat(char *catbuff, size_t size, const char *a, const char *b);
HTSEXT_API char *fconv(char *catbuff, size_t size, const char *a);
HTSEXT_API char *fslash(char *catbuff, size_t size, const char *a);

/* Debugging */
HTSEXT_API void hts_debug(int level);

/* Portable directory API */

#ifndef HTS_DEF_FWSTRUCT_find_handle_struct
#define HTS_DEF_FWSTRUCT_find_handle_struct
typedef struct find_handle_struct find_handle_struct;
typedef find_handle_struct *find_handle;
#endif

#ifndef HTS_DEF_FWSTRUCT_topindex_chain
#define HTS_DEF_FWSTRUCT_topindex_chain
typedef struct topindex_chain topindex_chain;
#endif
struct topindex_chain {
  int level;                    /* sort level */
  char *category;               /* category */
  char name[2048];              /* path */
  struct topindex_chain *next;  /* next element */
};
HTSEXT_API find_handle hts_findfirst(char *path);
HTSEXT_API int hts_findnext(find_handle find);
HTSEXT_API int hts_findclose(find_handle find);
HTSEXT_API char *hts_findgetname(find_handle find);
HTSEXT_API int hts_findgetsize(find_handle find);
HTSEXT_API int hts_findisdir(find_handle find);
HTSEXT_API int hts_findisfile(find_handle find);
HTSEXT_API int hts_findissystem(find_handle find);

/* UTF-8 aware FILE API */
#ifndef HTS_DEF_FILEAPI
#ifdef _WIN32
#define FOPEN hts_fopen_utf8
HTSEXT_API FILE *hts_fopen_utf8(const char *path, const char *mode);

#define STAT hts_stat_utf8
typedef struct _stat STRUCT_STAT;
HTSEXT_API int hts_stat_utf8(const char *path, STRUCT_STAT * buf);

#define UNLINK hts_unlink_utf8
HTSEXT_API int hts_unlink_utf8(const char *pathname);

#define RENAME hts_rename_utf8
HTSEXT_API int hts_rename_utf8(const char *oldpath, const char *newpath);

#define MKDIR(F) hts_mkdir_utf8(F)
HTSEXT_API int hts_mkdir_utf8(const char *pathname);

#define UTIME(A,B) hts_utime_utf8(A,B)
typedef struct _utimbuf STRUCT_UTIMBUF;
HTSEXT_API int hts_utime_utf8(const char *filename,
                              const STRUCT_UTIMBUF * times);
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

/** Macro aimed to break at build-time if a size is not a sizeof() strictly 
 *  greater than sizeof(char*). **/
#undef COMPILE_TIME_CHECK_SIZE
#define COMPILE_TIME_CHECK_SIZE(A) (void) ((void (*)(char[A - sizeof(char*) - 1])) NULL)

/** Macro aimed to break at compile-time if a size is not a sizeof() strictly 
 *  greater than sizeof(char*). **/
#undef RUNTIME_TIME_CHECK_SIZE
#define RUNTIME_TIME_CHECK_SIZE(A) assertf((A) != sizeof(void*))

#define fconv(A,B,C) (COMPILE_TIME_CHECK_SIZE(B), fconv(A,B,C))
#define concat(A,B,C,D) (COMPILE_TIME_CHECK_SIZE(B), concat(A,B,C,D))
#define fconcat(A,B,C,D) (COMPILE_TIME_CHECK_SIZE(B), fconcat(A,B,C,D))
#define fslash(A,B,C) (COMPILE_TIME_CHECK_SIZE(B), fslash(A,B,C))

#ifdef __cplusplus
}
#endif

#endif
