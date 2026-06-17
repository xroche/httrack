/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998 Xavier Roche and other contributors

SPDX-License-Identifier: GPL-3.0-or-later

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

Ethical use: we kindly ask that you NOT use this software to harvest email
addresses or to collect any other private information about people. Doing so
would dishonor our work and waste the many hours we have spent on it.

Please visit our Website: http://www.httrack.com
*/

/* ------------------------------------------------------------ */
/* File: HTTrack definition file for library usage              */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/**
 * @file httrack-library.h
 * @brief Public C API for embedding the HTTrack mirroring engine.
 *
 * Two ways to drive the engine, both supported and used by real consumers:
 *  - argv path: build an argv vector and call hts_main()/hts_main2(), exactly
 *    as the command-line tool is configured.
 *  - struct/callback path: hts_create_opt(), install callbacks with
 *    CHAIN_FUNCTION(), then hts_main2(), then hts_free_opt().
 *
 * Typical lifecycle: hts_init() once per process, then per mirror
 * hts_create_opt() -> CHAIN_FUNCTION() -> hts_main2() (blocking) ->
 * hts_get_stats()/hts_errmsg() -> hts_free_opt().
 *
 * Threading: hts_main2() blocks the calling thread. hts_request_stop() and
 * hts_has_stopped() are safe to call for the same opt from another thread while
 * the mirror runs. hts_free_opt() must not run until hts_has_stopped() is true.
 */

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
/** Log severity levels, most to least severe. A message is emitted only if its
    level is <= opt->debug. LOG_ERRNO is a flag OR'd into the level to append
    ": <strerror(errno)>" to the message. */
typedef enum hts_log_type {
  LOG_PANIC,         /**< Fatal condition. */
  LOG_ERROR,         /**< Error. */
  LOG_WARNING,       /**< Warning. */
  LOG_NOTICE,        /**< Notice; the default opt->debug level. */
  LOG_INFO,          /**< Informational. */
  LOG_DEBUG,         /**< Debug detail. */
  LOG_TRACE,         /**< Most verbose tracing. */
  LOG_ERRNO = 1 << 8 /**< Flag: append strerror(errno) to the message. */
} hts_log_type;
#endif
#ifndef HTS_DEF_FWSTRUCT_hts_stat_struct
#define HTS_DEF_FWSTRUCT_hts_stat_struct
typedef struct hts_stat_struct hts_stat_struct;
#endif

/** Assertion/error handler. Receives the failed expression text, source file,
    and line. The strings are valid only for the duration of the call; do not
    retain them. */
#ifndef HTS_DEF_FWSTRUCT_htsErrorCallback
#define HTS_DEF_FWSTRUCT_htsErrorCallback
typedef void (*htsErrorCallback) (const char *msg, const char *file, int line);
#endif

/* Helpers for plugging callbacks
requires: htsdefines.h */

/**
 * Install callback FUNCTION into OPT->callbacks_fun->MEMBER, chaining it ahead
 * of any callback already there (whose function and carg are saved for
 * CALLBACKARG_PREV_FUN/CALLBACKARG_PREV_CARG). ARGUMENT is an optional (may be
 * NULL) user pointer, later read inside the callback with
 * CALLBACKARG_USERDEF(). Allocates a t_hts_callbackarg with hts_malloc (not
 * checked for OOM); it is freed by hts_free_opt().
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

/** Inside a chained callback, return the ARGUMENT pointer originally passed to
    CHAIN_FUNCTION(), or NULL when CARG is NULL. */
#define CALLBACKARG_USERDEF(CARG) ( ( (CARG) != NULL ) ? (CARG)->userdef : NULL )

/** Return the callback of type NAME that this one chained over, cast to its
    function-pointer type, or NULL. Call it to forward to the prior handler. */
#define CALLBACKARG_PREV_FUN(CARG, NAME) ( (t_hts_htmlcheck_ ##NAME) ( ( (CARG) != NULL ) ? (CARG)->prev.fun : NULL ) )

/** Return the carg of the callback this one chained over (pass it when
   forwarding to the CALLBACKARG_PREV_FUN result), or NULL. */
#define CALLBACKARG_PREV_CARG(CARG) ( ( (CARG) != NULL ) ? (CARG)->prev.carg : NULL )

/* Functions */

/* Initialization */
/** Initialize the engine (lazy, idempotent, process-global): threading, the
    hashtable assert handler, modules, the MD5 self-test, and TLS when built
   with it. Only the first call does work. Honors $HTS_LOG for the debug level.
    Always returns 1. Call before hts_create_opt() or hts_main(). */
HTSEXT_API int hts_init(void);

/** No-op kept for API compatibility. Frees nothing (the process-global mutexes
    set up by hts_init() are never released) and always returns 1. */
HTSEXT_API int hts_uninit(void);

/** Block until all background mirror threads have finished. No-op unless built
    with threaded fetching. */
HTSEXT_API void htsthread_wait(void);

/* Main functions */
/** Run a full mirror from a command-line argv (argv[0] is ignored, as in
   main()). Creates a fresh option set, runs the engine, and frees it. Returns
   the engine exit code. Call hts_init() first. */
HTSEXT_API int hts_main(int argc, char **argv);

/** Run a full mirror using a caller-supplied option set. Use this instead of
    hts_main() to set options or plug callbacks on opt first. Blocks until the
    mirror ends and returns the engine exit code. The caller keeps ownership of
    opt and must release it with hts_free_opt(). */
HTSEXT_API int hts_main2(int argc, char **argv, httrackp * opt);

/* Options handling */
/** Allocate and default-initialize an option set, preloading the bundled parser
    modules. Returns a heap object the caller owns and must release with
    hts_free_opt(). Does not return NULL on allocation failure. */
HTSEXT_API httrackp *hts_create_opt(void);

/** Free an option set created by hts_create_opt() (callback chains, plugged
    modules, DNS cache, owned strings, and the structure). NULL is accepted. The
    pointer is invalid afterward. Do not call while a mirror is running on that
    opt; wait until hts_has_stopped() is true. */
HTSEXT_API void hts_free_opt(httrackp * opt);

/** Return sizeof(httrackp) as the library sees it, for caller-vs-library struct
    ABI mismatch checks. */
HTSEXT_API size_t hts_sizeof_opt(void);

/** Snapshot opt's error/warning/info counters and return a pointer to them.
    Returns NULL if opt is NULL. The result aliases a single process-global
    static: it is not thread-safe and is overwritten by the next call, so copy
    out the fields you need. */
HTSEXT_API const hts_stat_struct* hts_get_stats(httrackp * opt);

/** Legacy no-op retained for API compatibility. */
HTSEXT_API void set_wrappers(httrackp * opt);   /* LEGACY */

/** Load a plugin shared library and run its hts_plug(opt, argv) entry point. On
    success the handle is recorded in opt and unloaded by hts_free_opt().
    @return 1 if loaded and hts_plug succeeded; 0 if loaded but hts_plug was
    missing or refused; -1 if the library could not be loaded. */
HTSEXT_API int plug_wrapper(httrackp * opt, const char *moduleName,
                            const char *argv);

/** Install the process-global assertion/error callback (NULL clears it). Not
    per-opt, and not safe to change while a mirror runs. */
HTSEXT_API void hts_set_error_callback(htsErrorCallback handler);

/** Return the current process-global error callback, or NULL. */
HTSEXT_API htsErrorCallback hts_get_error_callback(void);

/* Logging */
/** Legacy: write prefix then msg to opt->log. Returns 0 if written, 1 if
    opt->log is NULL. Prefer hts_log_print(). */
HTSEXT_API int hts_log(httrackp * opt, const char *prefix, const char *msg);

/** printf-style log at level @p type (an hts_log_type, optionally |LOG_ERRNO).
    Forwards to the registered log callback, and when the level is <= opt->debug
    also to opt->log. @p format must be non-NULL. */
HTSEXT_API void hts_log_print(httrackp * opt, int type, const char *format,
                              ...) HTS_PRINTF_FUN(3, 4);

/** va_list form of hts_log_print(). @p opt may be NULL (only the callback
   runs). Preserves errno. @p format must be non-NULL. */
HTSEXT_API void hts_log_vprint(httrackp * opt, int type, const char *format,
                               va_list args);

/** Install the process-global log callback invoked by hts_log_vprint() for
   every message, regardless of opt->debug (NULL clears it). Not per-opt. */
HTSEXT_API void
hts_set_log_vprint_callback(void (*callback)(httrackp *opt, int type,
                                             const char *format, va_list args));

/* Infos */
/** Human-readable build/feature string plus the names of plugged modules. The
    result is written into and aliases a 2048-byte scratch buffer inside opt: it
    is valid until that buffer is next used, and must not be freed. opt must be
    non-NULL. */
HTSEXT_API const char *hts_get_version_info(httrackp * opt);

/** Static build-features string (TLS, zlib, ipv6, and so on). Process-global
    storage; do not free or modify. */
HTSEXT_API const char *hts_is_available(void);

/** HTTrack version id string. Static storage; do not free. */
HTSEXT_API const char *hts_version(void);

/* Wrapper functions */
HTSEXT_API int htswrap_init(void);      // DEPRECATED - DUMMY FUNCTION

HTSEXT_API int htswrap_free(void);      // DEPRECATED - DUMMY FUNCTION

/** Register callback @p fct under @p name in opt's callback table (for example
    "start", "check-html", "linkdetected"). Returns 1 on success, 0 if @p name
   is not a known slot. Prefer CHAIN_FUNCTION(), which preserves any prior
   callback. */
HTSEXT_API int htswrap_add(httrackp * opt, const char *name, void *fct);

/** Return the function pointer registered under @p name in opt as a uintptr_t,
   or 0 if none or unknown. */
HTSEXT_API uintptr_t htswrap_read(httrackp * opt, const char *name);

/** @warning No implementation is linked into the library; calling this fails to
    link. For per-callback user data use the CHAIN_FUNCTION() ARGUMENT and
    CALLBACKARG_USERDEF() instead. */
HTSEXT_API int htswrap_set_userdef(httrackp * opt, void *userdef);

/** @warning No implementation is linked into the library; calling this fails to
    link. Read per-callback user data with CALLBACKARG_USERDEF() instead. */
HTSEXT_API void *htswrap_get_userdef(httrackp * opt);

/* Internal library allocators, if a different libc is being used by the client */
/** strdup() through the library allocator. Returns a heap copy freed with
    hts_free(), or NULL on failure. */
HTSEXT_API char *hts_strdup(const char *string);

/** malloc() through the library allocator. Free with hts_free(). NULL on OOM.
 */
HTSEXT_API void *hts_malloc(size_t size);

/** realloc() through the library allocator. NULL on failure, leaving the
   original block unchanged. */
HTSEXT_API void *hts_realloc(void *const data, const size_t size);

/** free() through the library allocator. NULL is accepted. */
HTSEXT_API void hts_free(void *data);

/* Other functions */
HTSEXT_API int hts_resetvar(void);      // DEPRECATED - DUMMY FUNCTION

/** (Re)build the top-level index.html aggregating every mirror project found
    under @p path. @p binpath is the data root used to locate the
    templates/topindex-*.html files, falling back to built-in templates. Writes
    <path>/index.html. @return 1 on success, 0 on failure. */
HTSEXT_API int hts_buildtopindex(httrackp * opt, const char *path,
                                 const char *binpath);

/** Scan every mirror project under @p path and return a CRLF-separated list:
    @p type==1 gives the distinct category names, any other value gives the
    project directory names. The result is heap-allocated and owned by the
   caller (free with freet()); it may be NULL. Not UTF-8. @p path is modified in
   place (a trailing '/' is stripped). */
HTSEXT_API char *hts_getcategories(char *path, int type);

/** Read the `category=` value from a winprofile.ini file. The result is
    heap-allocated and owned by the caller (free with freet()), or NULL when the
    file is missing or has no category line. Not UTF-8. */
HTSEXT_API char *hts_getcategory(const char *filename);

/* Catch-URL */
/** Open a local capture socket (a mini-proxy), trying a list of standard ports
    until one binds. Writes the chosen port to *port_prox and the local host
    address into adr_prox (a caller buffer of at least 128 bytes), and returns
    the listening socket. Returns INVALID_SOCKET if no port could be bound. */
HTSEXT_API T_SOC catch_url_init_std(int *port_prox, char *adr_prox);

/** Open a local capture socket bound to *port (0 picks a free port). Writes the
    effective port back to *port and the local dotted address into @p adr (a
    caller buffer of at least 128 bytes), and returns the listening socket.
    Returns INVALID_SOCKET on failure. */
HTSEXT_API T_SOC catch_url_init(int *port, char *adr);

/** Block on capture socket @p soc, accept one browser connection, and capture
    the proxied HTTP request: write the absolute URL to @p url, the upper-cased
    method to @p method, and the rebuilt request (request line, headers, and any
    POST body) to @p data, then send a canned response and close.
    @return 1 on success, 0 on error; on error @p url instead holds the peer's
    "ip:port". The buffers are caller-allocated and not bounds-checked: @p data
    must be CATCH_URL_DATA_SIZE bytes, and @p url / @p method must fit the
    captured request line. */
HTSEXT_API int catch_url(T_SOC soc, char *url, char *method, char *data);

/* State */
/** Whether the engine is parsing HTML. Returns 0 if not, otherwise the percent
    done (at least 1). @p flag >= 0 also requests a progress refresh; pass a
    negative value to query without side effects. */
HTSEXT_API int hts_is_parsing(httrackp * opt, int flag);

/** Current background phase: 0 none, 1 testing links, 2 purge, 3, 4 scheduling,
    5 waiting for a slot. */
HTSEXT_API int hts_is_testing(httrackp * opt);

/** Nonzero once the engine has begun its exit sequence. */
HTSEXT_API int hts_is_exiting(httrackp * opt);

/*HTSEXT_API int hts_setopt(httrackp* opt); DEPRECATED ; see copy_htsopt() */

/** Queue extra start URLs to inject into a running mirror. @p url is a
    caller-owned, NULL-terminated array of strings; the engine stores the
   pointer without copying, so the array and its strings must stay valid until
   the engine consumes them. @return nonzero if a list is now set. */
HTSEXT_API int hts_addurl(httrackp * opt, char **url);

/** Clear any pending add-URL list set by hts_addurl(). Always returns 0. */
HTSEXT_API int hts_resetaddurl(httrackp * opt);

/** Apply the runtime-tunable options from @p from onto @p to, to adjust a live
    mirror. Only fields set to a non-sentinel value are copied; the rest of @p
   to is left untouched. The user-agent string is deep-copied. @return 0. */
HTSEXT_API int copy_htsopt(const httrackp * from, httrackp * to);

/** Return the engine's last error message, or NULL. The string is owned by
    @p opt; do not free it, and use it only while @p opt lives. */
HTSEXT_API char *hts_errmsg(httrackp * opt);

/** Get or set the transfer-pause flag. @p p >= 0 sets it (nonzero means
   paused); a negative value queries. @return the current pause flag. */
HTSEXT_API int hts_setpause(httrackp * opt, int);

/** Ask the running mirror to terminate (sets the stop flag under the state
   lock, so it is safe to call from another thread). @p force is currently
   ignored.
    @return 0; no-op if @p opt is NULL. */
HTSEXT_API int hts_request_stop(httrackp * opt, int force);

/** Queue a single in-progress file, by URL, to be cancelled by the engine.
    @p url is copied internally. Takes the state lock, so it is thread-safe.
    @return the underlying push result. */
HTSEXT_API int hts_cancel_file_push(httrackp * opt, const char *url);

/** Cancel the in-progress link-testing phase. Effective only while a test runs.
 */
HTSEXT_API void hts_cancel_test(httrackp * opt);

/** Cancel the in-progress HTML parsing. Effective only while parsing is active.
 */
HTSEXT_API void hts_cancel_parsing(httrackp * opt);

/** Nonzero once the mirror has fully ended. Read under the engine state lock,
   so safe to poll from another thread. Wait for this before hts_free_opt(). */
HTSEXT_API int hts_has_stopped(httrackp * opt);

/* Tools */
/** Ensure the directory chain leading to @p path exists, creating missing
    directories. @p path ends either with '/' (a directory) or a filename (its
    basename is ignored). A regular file blocking a needed directory is renamed
    to "<name>.txt". @p path is NOT UTF-8. @return 0 on success or if it already
    exists, -1 on error. */
HTSEXT_API int structcheck(const char *path);

/** Like structcheck() but @p path is UTF-8. @return 0 on success, -1 on error.
 */
HTSEXT_API int structcheck_utf8(const char *path);

/** Whether the directory containing @p path exists. The basename is stripped
    first, so passing a file path tests its parent directory. @return 1 if it is
    a directory, 0 otherwise. */
HTSEXT_API int dir_exists(const char *path);

/** Write the HTTP reason phrase for @p statuscode into @p msg, a caller buffer
   of at least 64 bytes. For an unknown code a non-empty @p msg is kept,
   otherwise it is set to "Unknown error". */
HTSEXT_API void infostatuscode(char *msg, int statuscode);

/** Return the static reason-phrase string for @p statuscode, or NULL if
   unknown. The pointer is a string literal; do not free it. */
HTSEXT_API const char *infostatuscode_const(int statuscode);

/** Current wall-clock time in milliseconds since the Unix epoch. */
HTSEXT_API TStamp mtime_local(void);

/** Format a duration @p t (in seconds) into a compact string in @p st, for
    example "3d,02h,04min05s". @p st is caller-allocated and not bounds-checked.
 */
HTSEXT_API void qsec2str(char *st, TStamp t);

/* The int2* helpers below write into the caller-supplied strc and return
   pointers into it. No allocation happens; the result is valid only until strc
   is reused, and a given strc is not reentrant. Use one strc per
   concurrently-live result. */
/** Format @p n as a decimal string into @p strc and return it. */
HTSEXT_API char *int2char(strc_int2bytes2 * strc, int n);

/** Format byte count @p n as "<num><unit>" (B/KiB/MiB/GiB and so on) into
    @p strc and return it. */
HTSEXT_API char *int2bytes(strc_int2bytes2 * strc, LLint n);

/** Format a transfer rate @p n as "<num><unit>/s" into @p strc and return it.
 */
HTSEXT_API char *int2bytessec(strc_int2bytes2 * strc, long int n);

/** Split byte count @p n into number and unit, returning a 2-element array
    {number, unit} stored inside @p strc. */
HTSEXT_API char **int2bytes2(strc_int2bytes2 * strc, LLint n);

/** Skip any "user[:pass]@" identification prefix in a URL, returning a pointer
    into the argument past it (or past the protocol if none). The result aliases
    the input string. */
HTSEXT_API char *jump_identification(char *);

HTSEXT_API const char *jump_identification_const(const char *);

/** Like jump_identification() and also strip a leading "www." host prefix,
    returning a pointer into the input to the normalized host. */
HTSEXT_API char *jump_normalized(char *);

HTSEXT_API const char *jump_normalized_const(const char *);

/** Return a pointer (into the input) to the ":port" part of a URL host, or NULL
    if there is no explicit port. Handles bracketed IPv6 literals. */
HTSEXT_API char *jump_toport(char *);

HTSEXT_API const char *jump_toport_const(const char *);

/** Canonicalize a URL path into @p dest: collapse duplicate '/' and sort the
    query-string arguments, so "?b=2&a=1" and "?a=1&b=2" compare equal. Returns
    @p dest, a caller buffer of at least strlen(source)+1 bytes (the output is
    never longer than the input). */
HTSEXT_API char *fil_normalized(const char *source, char *dest);

/** Write the normalized host of @p source (identification and "www." stripped)
    into @p dest, truncated to @p destsize. Returns @p dest. */
HTSEXT_API char *adr_normalized_sized(const char *source, char *dest,
                                      size_t destsize);

/** @deprecated Use adr_normalized_sized(). This form has no destination size
   and assumes @p dest is the engine URL buffer of HTS_URLMAXSIZE*2 bytes; a
   smaller buffer can overflow. */
HTS_DEPRECATED("use adr_normalized_sized(source, dest, destsize)")

HTSEXT_API char *adr_normalized(const char *source, char *dest);

/** Get or set the process executable root directory (with trailing '/'). The
    first call with non-NULL @p file initializes it and returns NULL; later
    initialization calls are ignored. Call with NULL to query: returns the
   stored directory, or "" if never set. The result is a static internal buffer;
   do not free it, and do not set it from multiple threads. */
HTSEXT_API const char *hts_rootdir(char *file);

/* Escaping URLs */
/*
 * Size contract shared by the escape/unescape family below.
 * For the escape_* / append_escape_* / inplace_escape_* /
 * escape_for_html_print* / make_content_id / x_escape_http functions, `size` is
 * the total capacity of `dest` including the terminating NUL. The size_t return
 * is the number of bytes written, NOT counting the NUL; on overflow it returns
 * `size` and `dest` is still NUL-terminated (truncated). Passing sizeof(a
 * pointer) as the size trips a runtime assert. The unescape_http* functions
 * instead return `dest` (the catbuff pointer) and truncate to fit `size`.
 */
/** Decode HTML entities in @p s in place (for example "&amp;" becomes "&"). */
HTSEXT_API void unescape_amp(char *s);

/** Percent-escape only spaces (' ' becomes "%20"); copy everything else
 * verbatim. */
HTSEXT_API size_t escape_spc_url(const char *const src, char *const dest, const size_t size);

/** Aggressively percent-escape @p src for use as a single URL path segment
    (reserved, delimiter, unwise, special, avoid and mark characters). */
HTSEXT_API size_t escape_in_url(const char *const src, char *const dest, const size_t size);

/** Percent-escape @p src as a URI, escaping only what is necessary and keeping
    '/' and other reserved characters. */
HTSEXT_API size_t escape_uri(const char *const src, char *const dest, const size_t size);

/** Like escape_uri() for a UTF-8 URI: also escapes reserved characters other
    than '/'. */
HTSEXT_API size_t escape_uri_utf(const char *const src, char *const dest, const size_t size);

/** Minimal "make safe" escape: percent-escapes only '"', ' ' and control
    characters, leaving an already-formed URL otherwise intact. */
HTSEXT_API size_t escape_check_url(const char *const src, char *const dest, const size_t size);

/** Append-variant of escape_spc_url(): escapes @p src after the existing
    NUL-terminated content of @p dest. Returns the bytes appended (excluding the
    NUL). */
HTSEXT_API size_t append_escape_spc_url(const char *const src, char *const dest, const size_t size);

/** Append-variant of escape_in_url(). See append_escape_spc_url(). */
HTSEXT_API size_t append_escape_in_url(const char *const src, char *const dest, const size_t size);

/** Append-variant of escape_uri(). See append_escape_spc_url(). */
HTSEXT_API size_t append_escape_uri(const char *const src, char *const dest, const size_t size);

/** Append-variant of escape_uri_utf(). See append_escape_spc_url(). */
HTSEXT_API size_t append_escape_uri_utf(const char *const src, char *const dest, const size_t size);

/** Append-variant of escape_check_url(). See append_escape_spc_url(). */
HTSEXT_API size_t append_escape_check_url(const char *const src, char *const dest, const size_t size);

/** In-place variant of escape_spc_url(): escapes the NUL-terminated string in
    @p dest back into @p dest. */
HTSEXT_API size_t inplace_escape_spc_url(char *const dest, const size_t size);

/** In-place variant of escape_in_url(). See inplace_escape_spc_url(). */
HTSEXT_API size_t inplace_escape_in_url(char *const dest, const size_t size);

/** In-place variant of escape_uri(). See inplace_escape_spc_url(). */
HTSEXT_API size_t inplace_escape_uri(char *const dest, const size_t size);

/** In-place variant of escape_uri_utf(). See inplace_escape_spc_url(). */
HTSEXT_API size_t inplace_escape_uri_utf(char *const dest, const size_t size);

/** In-place variant of escape_check_url(). See inplace_escape_spc_url(). */
HTSEXT_API size_t inplace_escape_check_url(char *const dest, const size_t size);

/** Same escaping as escape_check_url() but returns @p dest instead of the byte
    count. */
HTSEXT_API char *escape_check_url_addr(const char *const src, char *const dest, const size_t size);

/** Build a MIME/MHTML content-id token in @p dest from @p adr and @p fil:
    escape_in_url() both, then replace every '%' with 'X' so the result is one
    opaque token. */
HTSEXT_API size_t make_content_id(const char *const adr, const char *const fil, char *const dest, const size_t size);

/** Low-level percent-escaper backing the escape_* family. @p mode selects the
    character class to escape: 0 check_url, 1 in_url, 2 spc_url, 3 uri,
    30 uri_utf. @p max_size is the dest capacity including the NUL. */
HTSEXT_API size_t x_escape_http(const char *const s, char *const dest, const size_t max_size, const int mode);

/** Strip all control characters (byte value < 32) from @p s in place. */
HTSEXT_API void escape_remove_control(char *const s);

/** HTML-escape for text output: rewrite '&' to "&amp;" and pass every other
   byte through unchanged. */
HTSEXT_API size_t escape_for_html_print(const char *const s, char *const dest, const size_t size);

/** Like escape_for_html_print() but also convert every high byte (>= 128) to a
    numeric entity "&#xNN;". */
HTSEXT_API size_t escape_for_html_print_full(const char *const s, char *const dest, const size_t size);

/** Percent-decode @p s into @p catbuff (capacity @p size) and return @p
   catbuff. Decodes every "%xx" hex escape. */
HTSEXT_API char *unescape_http(char *const catbuff, const size_t size, const char *const s);

/** Percent-decode @p s into @p catbuff, but only the escapes that are safe to
    decode while keeping a valid URI (reserved, delimiter, unwise, control and
    must-avoid escapes are kept encoded, and %25 is never decoded). @p no_high &
   1 also decodes high (>= 128) bytes; @p no_high & 2 also decodes an escaped
    space. Returns @p catbuff. */
HTSEXT_API char *unescape_http_unharm(char *const catbuff, const size_t size, const char *s, const int no_high);

/** @warning No implementation is linked into the library; calling this fails to
    link. */
HTSEXT_API char *antislash_unescaped(char *catbuff, const char *s);

HTSEXT_API void escape_remove_control(char *s);

/** Determine the MIME type of local file name @p fil into @p s (capacity
    @p ssize): user --assume rules, then ".html", then the built-in extension
    table. @p flag != 0 forces a fallback type. @return 1 if a type was written,
    0 otherwise. */
HTSEXT_API int get_httptype_sized(httrackp *opt, char *s, size_t ssize,
                                  const char *fil, int flag);

/** @deprecated Use get_httptype_sized(). Assumes @p s has at least
    HTS_MIMETYPE_SIZE capacity. */
HTS_DEPRECATED("use get_httptype_sized(opt, s, ssize, fil, flag)")

HTSEXT_API void get_httptype(httrackp * opt, char *s, const char *fil,
                             int flag);

/** Classify @p fil by its extension: 0 unknown, 1 known non-HTML, 2 known HTML.
    Consults the built-in table then user --assume rules. 0 for a NULL @p fil.
 */
HTSEXT_API int is_knowntype(httrackp * opt, const char *fil);

/** Like is_knowntype() but consults only the user --assume rules: 0 no rule,
    1 non-HTML, 2 HTML. */
HTSEXT_API int is_userknowntype(httrackp * opt, const char *fil);

/** 1 if @p fil, an extension such as "asp" or "php" (not a full filename), is a
    known dynamic-page type, else 0. */
HTSEXT_API int is_dyntype(const char *fil);

/** Extract the extension of @p fil (text after the last '.', stopping at '?')
    into caller scratch @p catbuff (capacity @p size) and return it. Returns ""
    (a literal, not @p catbuff) when there is no extension or it does not fit.
 */
HTSEXT_API const char *get_ext(char *catbuff, size_t size, const char *fil);

/** 1 if MIME type @p st must not be reclassified or renamed (hypertext types
   and a built-in keep-list of commonly mislabeled types), else 0. */
HTSEXT_API int may_unknown(httrackp * opt, const char *st);

/** Guess the MIME type of local file @p fil into @p s (capacity @p ssize),
    always producing a type. @return 1 if a type was written. */
HTSEXT_API int guess_httptype_sized(httrackp *opt, char *s, size_t ssize,
                                    const char *fil);

/** @deprecated Use guess_httptype_sized(). Assumes @p s has at least
    HTS_MIMETYPE_SIZE capacity. */
HTS_DEPRECATED("use guess_httptype_sized(opt, s, ssize, fil)")

HTSEXT_API void guess_httptype(httrackp * opt, char *s, const char *fil);

/* Ugly string tools */
/* These take a caller scratch buffer catbuff of capacity size and return it. On
   overflow they stop without writing past size and return the truncated buffer.
   size must be a real array sizeof (the macros below check this at compile
   time), not a pointer. */
/** Concatenate @p a and @p b into @p catbuff (NULL or empty operands are
 * skipped). */
HTSEXT_API char *concat(char *catbuff, size_t size, const char *a, const char *b);

/** Like concat(a, b) but convert '/' to the platform path separator (Windows).
 */
HTSEXT_API char *fconcat(char *catbuff, size_t size, const char *a, const char *b);

/** Copy @p a into @p catbuff, converting '/' to the platform path separator
    (Windows). */
HTSEXT_API char *fconv(char *catbuff, size_t size, const char *a);

/** Copy @p a into @p catbuff, converting every '\\' to '/' on all platforms. */
HTSEXT_API char *fslash(char *catbuff, size_t size, const char *a);

/* Debugging */
/** Set the process-global debug verbosity (0 is off); higher levels log more to
    stderr. Bit 0x80 redirects debug output to "hts-debug.txt". */
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
/** One node of the index/category listing built when generating the top index.
 */
struct topindex_chain {
  int level;                   /**< sort level */
  char *category;              /**< category (heap string) */
  char name[2048];             /**< path */
  struct topindex_chain *next; /**< next element */
};

/** Open directory @p path for iteration, positioned on the first entry. Returns
    an opaque handle to free with hts_findclose(), or NULL on empty path or open
    failure. */
HTSEXT_API find_handle hts_findfirst(char *path);

/** Advance to the next directory entry. Returns 1 if an entry is available, 0
   at end of directory. */
HTSEXT_API int hts_findnext(find_handle find);

/** Close the iteration and free @p find. Always returns 0; NULL is accepted. */
HTSEXT_API int hts_findclose(find_handle find);

/** Name of the current entry, or NULL. Points into the handle's storage; valid
    only until the next hts_findnext()/hts_findclose(). */
HTSEXT_API char *hts_findgetname(find_handle find);

/** Size in bytes of the current entry, or -1. Truncated to int, so unreliable
    for files larger than 2 GB. */
HTSEXT_API int hts_findgetsize(find_handle find);

/** 1 if the current entry is a directory, else 0 (a system/special entry, see
    hts_findissystem(), reports 0). */
HTSEXT_API int hts_findisdir(find_handle find);

/** 1 if the current entry is a regular file, else 0 (a system/special entry,
   see hts_findissystem(), reports 0). */
HTSEXT_API int hts_findisfile(find_handle find);

/** 1 if the current entry is a special/system entry to skip: "." or "..", on
    POSIX also device/fifo/socket nodes, on Windows also system, hidden or
    temporary entries. Else 0. */
HTSEXT_API int hts_findissystem(find_handle find);

/* UTF-8 aware FILE API */
/* On non-Windows these macros resolve directly to the POSIX calls. On Windows
   they map to the hts_*_utf8 wrappers below, which convert the UTF-8 path to
   UTF-16 and call the wide CRT, falling back to the narrow CRT if conversion
   fails. Always pass UTF-8 paths through these. */
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
