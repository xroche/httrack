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
/* File: Subroutines .h                                         */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

// Fichier librairie .h

#ifndef HTS_DEFH
#define HTS_DEFH

#include "httrack-library.h"
#include "htsfetch.h"

/* Forward definitions */
#ifndef HTS_DEF_FWSTRUCT_htsrequest
#define HTS_DEF_FWSTRUCT_htsrequest
typedef struct htsrequest htsrequest;
#endif
#ifndef HTS_DEF_FWSTRUCT_htsblk
#define HTS_DEF_FWSTRUCT_htsblk
typedef struct htsblk htsblk;
#endif
#ifndef HTS_DEF_FWSTRUCT_t_dnscache
#define HTS_DEF_FWSTRUCT_t_dnscache
typedef struct t_dnscache t_dnscache;
#endif
#ifndef HTS_DEF_FWSTRUCT_lien_adrfil
#define HTS_DEF_FWSTRUCT_lien_adrfil
typedef struct lien_adrfil lien_adrfil;
#endif
#ifndef HTS_DEF_FWSTRUCT_lien_adrfilsave
#define HTS_DEF_FWSTRUCT_lien_adrfilsave
typedef struct lien_adrfilsave lien_adrfilsave;
#endif

/* définitions globales */
#include "htsglobal.h"
#include "htsdate.h"
#include "htsurlport.h"
#include "htsbuf.h"

/* basic net definitions */
#include "htsbase.h"
#include "htsbasenet.h"
#include "htsnet.h"
#include "htsdefines.h"

#include <limits.h>
#ifndef _WIN32
#include <signal.h>
#include <time.h>
#if defined(THREADS)
#include <pthread.h>
#endif
#endif

/* readdir() */
#ifndef _WIN32
#include <sys/types.h>
#include <dirent.h>
#endif

/* cookies et auth */
#include "htsbauth.h"

// Attention, définition existante également dans le shell
// (à modifier avec celle-ci)
#define POSTTOK "?>post"

/* Is this Location free of the >post: tokens? A redirect naming one makes the
   engine send a local file back to whoever chose the value, so every place
   that fills a location has to refuse it, the cache and a resume ref
   included. */
HTS_CHECK_RESULT hts_boolean hts_location_is_safe(const char *location);

#include "htsopt.h"

/* Engine helpers libhttrack does not export. */

/** Load a plugin shared library and run its hts_plug(opt, argv) entry point. On
    success the handle is recorded in opt and unloaded by hts_free_opt().
    @return 1 if loaded and hts_plug succeeded; 0 if loaded but hts_plug was
    missing or refused; -1 if the library could not be loaded. */
int plug_wrapper(httrackp *opt, const char *moduleName, const char *argv);

/** va_list form of hts_log_print(). @p opt may be NULL (only the callback
   runs). Preserves errno. @p format must be non-NULL. */
void hts_log_vprint(httrackp *opt, int type,
                    HTS_FORMAT_STRING const char *format, va_list args)
    HTS_PRINTF_FUN(3, 0);

/** Human-readable build/feature string plus the names of plugged modules. The
    result is written into and aliases a 2048-byte scratch buffer inside opt: it
    is valid until that buffer is next used, and must not be freed. opt must be
    non-NULL. */
const char *hts_get_version_info(httrackp *opt);

/** Return the function pointer registered under @p name in opt as a uintptr_t,
   or 0 if none or unknown. */
uintptr_t htswrap_read(httrackp *opt, const char *name);

/** Read the percent-decoded `category=` value from a winprofile.ini file. The
    result is heap-allocated and owned by the caller (free with freet()), or
    NULL when the file is missing or has no category line. The bytes are not
    converted. They are UTF-8 if the file declares Charset=UTF-8, else the
    legacy encoding (the ANSI codepage on Windows). */
char *hts_getcategory(const char *filename);

/** Open a capture socket on 127.0.0.1, port *port (0 picks a free port). No
    other host can reach it, because the request it captures carries the
    browser's cookies for the target site. Writes the bound port back to *port
    and its dotted address into @p adr of @p adr_size bytes (left empty if the
    address does not fit), and returns the listening socket. Returns
    INVALID_SOCKET on failure. */
T_SOC catch_url_init(int *port, char *adr, size_t adr_size);

/** Whether the directory containing @p path exists. The basename is stripped
    first, so passing a file path tests its parent directory. @return 1 if it is
    a directory, 0 otherwise. */
hts_boolean dir_exists(const char *path);

/** Return the static reason-phrase string for @p statuscode, or NULL if
   unknown. The pointer is a string literal; do not free it. */
const char *infostatuscode_const(int statuscode);

/** Format @p n as a decimal string into @p strc and return it. */
char *int2char(strc_int2bytes2 *strc, int n);

/** Split byte count @p n into number and unit, returning a 2-element array
    {number, unit} stored inside @p strc. */
char **int2bytes2(strc_int2bytes2 *strc, LLint n);

/** Like jump_identification() and also strip a leading "www." or "www-4."-style
    host prefix, returning a pointer into the input to the normalized host. */
const char *jump_normalized_const(const char *);

/** Return a pointer (into the input) to the ":port" part of a URL's authority,
    or NULL if there is no explicit port. A ':' in the path, query or fragment
    is never returned. Handles bracketed IPv6 literals. */
char *jump_toport(char *);

const char *jump_toport_const(const char *);

/** Canonicalize a URL path into @p dest: collapse duplicate '/' and sort the
    query-string arguments, so "?b=2&a=1" and "?a=1&b=2" compare equal. Returns
    @p dest, a caller buffer of at least strlen(source)+1 bytes (the output is
    never longer than the input). */
char *fil_normalized(const char *source, char *dest);

/** Percent-escape only spaces (' ' becomes "%20"); copy everything else
 * verbatim. */
size_t escape_spc_url(const char *const src, char *const dest,
                      const size_t size);

/** Percent-escape @p src as a URI: the control and high bytes, plus space,
    '*', '\'', '"', '&' and '!'. Every other byte is kept as it is, '/' and the
    other reserved characters included. */
size_t escape_uri(const char *const src, char *const dest, const size_t size);

/** Percent-escape as escape_uri() does, plus the reserved, delimiter and
    unwise characters other than '/'. */
size_t escape_uri_utf(const char *const src, char *const dest,
                      const size_t size);

/** Percent-escape the C0 control bytes of @p src (0x00-0x1f) and copy every
    other byte verbatim, so bytes >= 0x7f keep the UTF-8 they carry. */
size_t escape_control_url(const char *const src, hts_buf dest);

/** Append-variant of escape_spc_url(): escapes @p src after the existing
    NUL-terminated content of @p dest. Returns the bytes appended (excluding the
    NUL). */
size_t append_escape_spc_url(const char *const src, hts_buf dest);

/** Append-variant of escape_in_url(). See append_escape_spc_url(). */
size_t append_escape_in_url(const char *const src, hts_buf dest);

/** Append-variant of escape_check_url(). See append_escape_spc_url(). */
size_t append_escape_check_url(const char *const src, hts_buf dest);

/** In-place variant of escape_spc_url(): escapes the NUL-terminated string in
    @p dest back into @p dest. */
size_t inplace_escape_spc_url(char *const dest, const size_t size);

/** In-place variant of escape_in_url(). See inplace_escape_spc_url(). */
size_t inplace_escape_in_url(char *const dest, const size_t size);

/** In-place variant of escape_uri(). See inplace_escape_spc_url(). */
size_t inplace_escape_uri(char *const dest, const size_t size);

/** In-place variant of escape_uri_utf(). See inplace_escape_spc_url(). */
size_t inplace_escape_uri_utf(char *const dest, const size_t size);

/** Same escaping as escape_check_url() but returns @p dest instead of the byte
    count. */
char *escape_check_url_addr(const char *const src, hts_buf dest);

/** Build one opaque MIME/MHTML content-id token in @p dest from @p adr and
    @p fil: escape_in_url() escaping with '-' for '%', so two URLs never share
    a token. */
size_t make_content_id(const char *const adr, const char *const fil,
                       hts_buf dest);

/** Low-level percent-escaper backing the escape_* family. @p mode selects the
    character class to escape: 0 check_url, 1 in_url, 2 spc_url, 3 uri,
    4 control_url, 30 uri_utf. @p max_size is the dest capacity including the
    NUL. */
size_t x_escape_http(const char *const s, char *const dest,
                     const size_t max_size, const int mode);

/** Strip all control characters (byte value < 32) from @p s in place. */
void escape_remove_control(char *const s);

/** HTML-escape for text output: rewrite '&' to "&amp;" and pass every other
   byte through unchanged. */
size_t escape_for_html_print(const char *const s, char *const dest,
                             const size_t size);

/** Like escape_for_html_print() but also convert every high byte (>= 128) to a
    numeric entity "&#xNN;". */
size_t escape_for_html_print_full(const char *const s, char *const dest,
                                  const size_t size);

/** Classify @p fil by its extension: 0 unknown, 1 known non-HTML, 2 known HTML.
    Consults the built-in table then user --assume rules. 0 for a NULL @p fil.
 */
int is_knowntype(httrackp *opt, const char *fil);

/** Like is_knowntype() but consults only the user --assume rules: 0 no rule,
    1 non-HTML, 2 HTML. */
int is_userknowntype(httrackp *opt, const char *fil);

/** Is @p ext, an extension such as "asp" or "php" (not a full filename), a
    known dynamic-page type? */
hts_boolean is_dyntype(hts_span ext);

/** 1 if MIME type @p st must not be reclassified or renamed (hypertext types
   and a built-in keep-list of commonly mislabeled types), else 0. */
hts_boolean may_unknown(httrackp *opt, const char *st);

/** Copy @p a into @p dst, converting every '\\' to '/' on all platforms. */
char *fslash(hts_buf dst, const char *a);

/** Set the process-global debug verbosity (0 is off); higher levels log more to
    stderr. Bit 0x80 redirects debug output to "hts-debug.txt". */
void hts_debug(int level);

#define READ_ERROR (-1)
#define READ_EOF (-2)
#define READ_TIMEOUT (-3)
#define READ_INTERNAL_ERROR (-4)

/* concat */

HTS_STATIC char *getHtsOptBuff_(httrackp *opt) {
  opt->state.concat.index = (opt->state.concat.index + 1) % 16;
  return opt->state.concat.buff[opt->state.concat.index];
}

#define OPT_GET_BUFF(OPT)      ( getHtsOptBuff_(OPT) )
#define OPT_GET_BUFF_SIZE(OPT) ( sizeof(opt->state.concat.buff[0]) )
#define OPT_GET_HTS_BUF(OPT)                                                   \
  hts_buf_at(OPT_GET_BUFF(OPT), OPT_GET_BUFF_SIZE(OPT))

// cache pour le dns, pour éviter de faire des gethostbyname sans arrêt
#ifndef HTS_DEF_FWSTRUCT_t_dnscache
#define HTS_DEF_FWSTRUCT_t_dnscache
typedef struct t_dnscache t_dnscache;
#endif
// One DNS cache record, stored as a coucal value keyed by hostname.
struct t_dnscache {
  // resolved addresses, in resolver (RFC 6724) order; host_count==0 means the
  // name does not resolve (negative cache). host_count<=HTS_MAXADDRNUM.
  int host_count;
  size_t host_length[HTS_MAXADDRNUM]; // sockaddr length of each (16 or 28)
  char host_addr[HTS_MAXADDRNUM][HTS_MAXADDRLEN];
  // mtime_local() past which a negative record must be resolved again, and
  // when it was stored; both 0 on a positive one, which never expires
  TStamp expiry;
  TStamp stored;
  int failures; // consecutive failed resolves, which lengthen the next wait
  int preferred; // index of the address a fallback last connected to
  int gai_code;  // getaddrinfo()'s code behind a negative record
};

/* Break t down as UTC into the caller's buffer, HTS_FALSE if that failed.
   gmtime()'s static is shared, and both the engine and ProxyTrack convert on
   worker threads. */
static HTS_INLINE HTS_UNUSED hts_boolean hts_gmtime(time_t t,
                                                    struct tm *tmbuf) {
#ifdef _WIN32
  /* Microsoft's gmtime_s takes the destination first, unlike C11 Annex K. */
  return gmtime_s(tmbuf, &t) == 0 ? HTS_TRUE : HTS_FALSE;
#else
  return gmtime_r(&t, tmbuf) != NULL ? HTS_TRUE : HTS_FALSE;
#endif
}

/* Break t down as local time into the caller's buffer, HTS_FALSE if that
   failed. localtime()'s static is shared, same rationale as hts_gmtime(). */
static HTS_INLINE HTS_UNUSED hts_boolean hts_localtime(time_t t,
                                                       struct tm *tmbuf) {
#ifdef _WIN32
  return localtime_s(tmbuf, &t) == 0 ? HTS_TRUE : HTS_FALSE;
#else
  return localtime_r(&t, tmbuf) != NULL ? HTS_TRUE : HTS_FALSE;
#endif
}

/* Capacity the hts_strerror() callers pass; a longer message is clipped. */
#define HTS_STRERROR_SIZE 128

/* 0 where the build found no reentrant form and kept plain strerror(). */
#if defined(_MSC_VER) || defined(HAVE_STRERROR_R)
#define HTS_STRERROR_REENTRANT 1
#else
#define HTS_STRERROR_REENTRANT 0
#endif

/* Write the message for errno value err into buf, and return buf. strerror()
   answers with a buffer the caller does not own: glibc reuses a per-thread one
   and frees it at thread exit, and POSIX allows one shared static. */
const char *hts_strerror(int err, char *buf, size_t size);

/* A modification time at the finest resolution this build can read. Compare it
   only against another value hts_file_mtime() produced, because the epoch is
   the platform's. */
typedef struct hts_filetime_t {
  int64_t sec;  /* not a time_t: Windows counts from 1601, so never print it */
  int32_t nsec; /* 0 where neither the build nor the filesystem is finer */
  /* S_ISREG, except on Windows, where every non-directory counts as one. */
  hts_boolean is_plain_file;
} hts_filetime_t;

/* Read FILE's modification time into WHEN. False if it can not be read, and
   WHEN is then untouched. */
hts_boolean hts_file_mtime(const char *file, hts_filetime_t *when);

/* Move FILE's modification time SECONDS back, keeping the fraction, which the
   utimes fallback rounds down to the microsecond. False if the stamp could not
   be read or written, and FILE then keeps the time it had. */
hts_boolean hts_file_backdate(const char *file, int seconds);

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE

/* Is path certainly missing? Any doubt answers HTS_FALSE. Utf-8; trailing
   separators are ignored. */
hts_boolean hts_path_absent(const char *path);

/* Record a loaded module in opt->libHandles, copying its name. HTS_FALSE when
   out of memory, and opt->libHandles is then unchanged. */
hts_boolean hts_libhandles_add(httrackp *opt, void *handle,
                               const char *moduleName);

/** An opt that hts_log_vprint drops every message for. Never dereferenced. */
extern char hts_log_quiet_tag;
#define HTS_LOG_QUIET ((httrackp *) (void *) &hts_log_quiet_tag)

// initialize an htsblk structure
void hts_init_htsblk(htsblk * r);

/* How many seconds should we wait, given a Retry-After value? -1 when it names
   no usable delay. `now` dates the HTTP-date form, and the caller caps. */
int hts_parse_retry_after(const char *value, time_t now);

/* Read the HTTP date S as Unix seconds, with a two-digit year up to PIVOT in
   20xx (see convert_time_rfc822_pivot). -1 when S is not a date. */
int64_t hts_parse_date(const char *s, int pivot);

// attach specific project log to hachtable logger
void hts_set_hash_handler(coucal hashtable, httrackp *opt);

// fonctions unix/winsock
int hts_read(htsblk *r, hts_buf buff);

/* Add to, read and reset the received-byte total, which FTP workers update
   from their thread. A lock guards it, as a 64-bit atomic needs libatomic.
   hts_stat_recv_publish() copies it to HTS_STAT on the crawl thread. */
void hts_stat_recv_add(LLint bytes);
LLint hts_stat_recv_get(void);
void hts_stat_recv_set(LLint bytes);
LLint hts_stat_recv_publish(void);

LLint check_downloadable_bytes(int rate);

/* Client hints a Chromium browser sends by default (Sec-CH-UA*). */
typedef struct hts_client_hints {
  char ua[128];       /**< Sec-CH-UA brand list */
  char platform[16];  /**< Sec-CH-UA-Platform value, quoted */
  hts_boolean mobile; /**< Sec-CH-UA-Mobile is ?1 */
} hts_client_hints;

/* Fill ch with the hints the browser named by user_agent sends. Returns
   HTS_FALSE when that browser sends none: anything but Chrome or Edge from
   M108. */
hts_boolean hts_client_hints_of(const char *user_agent, hts_client_hints *ch);

/* Return the browser family whose request headers user_agent names: Chrome
   and Edge as hts_client_hints_of() tells them, or Firefox. */
hts_browser_family hts_browser_family_of(const char *user_agent);

/* Return http_header_slot_count() slot numbers: the order in which family
   sends the reorderable headers of a request to dest. */
const unsigned char *http_header_order(hts_browser_family family, int dest);

/* Return the header name of slot, or NULL past the last one. */
const char *http_header_slot_name(int slot);
int http_header_slot_count(void);

// fonctions principales
/* Start a non-blocking connect to adr (or to retour's proxy) for a GET, or open
   a file:// target. retour must not be NULL; it gets the socket and any error.
 */
T_SOC http_xfopen(httrackp *opt, const char *adr, const char *fil,
                  htsblk *retour);
int http_sendhead(httrackp *opt, t_cookie *cookie, int mode, const char *xsend,
                  const char *adr, const char *fil, const char *referer_adr,
                  const char *referer_fil, const hts_fetch_meta *fetch,
                  htsblk *retour);
/* Build the request "Cookie:" header line for stored cookies matching
   adr/path into dst (NUL-terminated), wrapping the logic http_sendhead()
   uses. adr may carry its scheme, and only "https://" gets Secure cookies. A
   port in adr is ignored (see cookie_host). Returns cookies emitted. */
int http_cookie_header(t_cookie *cookie, const char *adr, const char *path,
                       hts_buf dst);

/* True if the raw CRLF-separated header block, a request one we build or a
   response one we received, starts a line with `field_name`, given without the
   trailing colon (which is tolerated), ignoring case and blanks before the
   colon. headers must be NUL-terminated. */
hts_boolean http_headers_have_field(const char *headers,
                                    const char *field_name);

/* True when the response stored no body and no status excuses it: a redirect to
   follow, or the 412/416 whose stale resume partial the parser re-gets. */
hts_boolean hts_body_missing_unexpectedly(const htsblk *r);

/* Returns the warning to print for a -%X line, or NULL when it goes out as
   typed. headers is the block so far. */
const char *http_custom_header_warning(const char *headers, const char *line);

/* Why a bundle header line is refused, or NULL when allowed. headers is the
   block so far. */
const char *http_bundle_header_refusal(const char *headers, const char *line);

/* Append the custom header block to dst, dropping any line whose field dst
   carries, a malformed or framing line, an earlier copy of Host, Cookie or
   Authorization, and a leading folded line. Field names ignore case and blanks
   before the colon. A line that does not fit is left out. */
void http_append_custom_headers(hts_buf dst, const char *headers);

/* Append the raw remainder of a >postfile: body into buffer at write position
   pos, and return the new position. Clips to the room left, and always leaves
   a NUL at the position returned: the request block is read back with strlen()
   by the WARC stash, the sendhead callback and sendc(). A NUL inside the file
   ends the request there. */
size_t http_postfile_body(hts_buf buffer, size_t pos, FILE *fp);

/* Switch soc between blocking and non-blocking mode; false on failure, with
   errno (WSAGetLastError() on Windows) set. */
hts_boolean socket_set_nonblocking(T_SOC soc, hts_boolean nonblocking);
/* Pending-connect result for a non-blocking socket reported ready by select():
   0 = connected, >0 = the connect errno (refused, unreachable, ...), -1 if the
   probe itself failed. A failed connect is reported ready as well (writable on
   posix, exception set on winsock), so this is how success is told from failure
   without blocking. */
int connect_socket_error(T_SOC soc);

/* Start a non-blocking connect to iadr; port -1 takes it from iadr or the
   scheme default. */
T_SOC newhttp(httrackp *opt, const char *iadr, htsblk *retour, int port);
/* Like newhttp(), but connect to resolved address addr_index (0-based, or -1
   for the one hts_dns_set_preferred() last picked) and set *addr_count, if
   non-NULL, to the address count. */
T_SOC newhttp_addr(httrackp *opt, const char *iadr, htsblk *retour, int port,
                   int addr_index, int *addr_count);
/* Like newhttp_addr(), but try up to tries addresses from addr_index on (all
   of them for -1), skipping one -%b cannot bind for or whose connect fails at
   once. On success, *skipped (if non-NULL) gets how many were skipped. */
T_SOC newhttp_addrs(httrackp *opt, const char *iadr, htsblk *retour, int port,
                    int addr_index, int tries, int *addr_count, int *skipped);
/* Clips the formatted failure reason into r->msg, which also round-trips
   through the cache as X-StatusMessage. Leaves r->statuscode to the caller. */
#define htsblk_failf(R, ...)                                                   \
  slprintfbuff_clip((R)->msg, sizeof((R)->msg), __VA_ARGS__)

HTS_INLINE void deletehttp(htsblk * r);
HTS_INLINE int deleteaddr(htsblk * r);
HTS_INLINE void deletesoc(T_SOC soc);
/* Return 1 once soc is readable (writable) within timeout seconds, 0 otherwise.
   check_readinput polls r->soc without waiting. */
int check_readinput(htsblk * r);
int check_readinput_t(T_SOC soc, int timeout);
int check_writeinput_t(T_SOC soc, int timeout);

/* TRUE if S holds no byte below ' '. A control byte in a value the
   client puts on a protocol line (an FTP command, a CONNECT authority, a
   SOCKS5 host) smuggles a line or a field of its own. */
HTS_CHECK_RESULT hts_boolean hts_span_is_control_free(hts_span s);

/* Same over a NUL-terminated string. */
HTS_CHECK_RESULT hts_boolean hts_is_control_free(const char *str);

/* TRUE if HOST is an IPv4 literal: digits and dots, at least one dot. Such a
   host has no domain structure to reverse or to widen into. */
hts_boolean hts_host_is_ipv4(hts_span host);

/* Base-64 "user:pass" of the userinfo in a URL or proxy name, for a Basic
   Authorization header, into OUT (at least 1100 bytes). Returns 1 when filled,
   0 when there is no userinfo, -1 when it is too long (765 bytes max, as a
   clipped credential would authenticate as somebody else). */
int hts_basic_auth_value(httrackp *opt, const char *url, hts_buf out);

/* Copy the bare host name of a URL address into out for the TLS SNI
   extension: no scheme, userinfo or port. FALSE with out emptied when there is
   no DNS name to send (IP literal, empty, or too long), as RFC 6066 forbids
   literals. */
HTS_CHECK_RESULT hts_boolean hts_sni_hostname(const char *url_adr, hts_buf out);

#if HTS_USEOPENSSL
/* Make ssl's handshake fail unless the server's chain verifies against the
   system CA store and its certificate names url_adr's host (--check-cert).
   FALSE when the checks could not be armed: the caller must not connect. */
HTS_CHECK_RESULT hts_boolean hts_ssl_check_cert(SSL *ssl, const char *url_adr);
#endif

/* TRUE if HOST is an IPv4 or IPv6 address literal. */
hts_boolean hts_host_is_address(hts_span host);

/* TRUE if this -P proxy name (which keeps its scheme) is a SOCKS5 proxy. */
hts_boolean hts_proxy_is_socks(const char *name);

/* TRUE if this -P proxy name is a "connect://" CONNECT-only proxy (#564). */
hts_boolean hts_proxy_is_connect(const char *name);

void treathead(t_cookie * cookie, const char *adr, const char *fil, htsblk * retour,
               char *rcvd);
/* Run treathead, logging any refused cookie through opt, which may be NULL. */
void treathead_opt(httrackp *opt, t_cookie *cookie, const char *adr,
                   const char *fil, htsblk *retour, char *rcvd);
void treatfirstline(htsblk * retour, const char *rcvd);

/* Does Content-Length value s, a list such as "42, 42", agree with itself and
   with *seen, the length an earlier field gave (-1 if none)? Stores the length
   in *seen, or -2 for a signed or oversized one. Text that is no number is
   skipped. */
hts_boolean hts_content_length_agrees(const char *s, LLint *seen);

/* The status code of an "HTTP/x.y NNN reason" line, or -1. */
int hts_status_line_code(const char *line);

// sous-fonctions
/* Buffer http_xfread1() fills in its line modes, and so the ceiling on any
   blank-line-terminated block it reads: a header section or a chunk trailer
   section. Overrunning it fails the transfer. */
#define HTS_LINE_BLOCK_SIZE 8192
/* http_xfread1() read modes: a positive bufl reads at most that many raw bytes,
   anything else reads CR-stripped lines into the HTS_LINE_BLOCK_SIZE buffer. */
#define HTS_XFREAD_LINE_BLOCK 0 /* lines up to a blank one (header/trailer) */
#define HTS_XFREAD_LINE (-1)    /* one line, stopping at the first LF */
LLint http_xfread1(htsblk * r, int bufl);
/* Self-test hook: read the line modes bytewise, as on an MPTCP socket where
   peeking is unsafe. */
void hts_peek_force_off(hts_boolean off);
/* Self-test hook: how many line-mode calls took the bytewise path? */
int hts_xfread_bytewise_count(void);
/* Does an in-memory body of this size fit? r->adr is indexed with an int
   downstream and its allocations add a trailing NUL, so every producer of
   r->size must refuse where http_xfread1() does. */
HTS_CHECK_RESULT hts_boolean hts_inmem_size_fits(LLint size);

/* How did a resolve end? */
typedef enum hts_dns_status {
  HTS_DNS_OK = 0,
  HTS_DNS_NONAME,   /* the resolver says the name does not exist */
  HTS_DNS_TEMPFAIL, /* the resolver could not answer, so ask again later */
  HTS_DNS_TIMEOUT,  /* the wait ran out before the resolver answered */
  HTS_DNS_CANCELLED /* the cancel flag was raised before it answered */
} hts_dns_status;

/* The caller owns this answer to a resolve. */
typedef struct hts_dns_result {
  int count; /* addr[0..count-1], in resolver order; 0 unless status is OK */
  SOCaddr addr[HTS_MAXADDRNUM];
  hts_dns_status status;
  int gai_code;       /* getaddrinfo()'s code behind a failure, else 0 */
  hts_boolean cached; /* answered by the DNS cache, negatively or not */
} hts_dns_result;

/* Static English text for res's outcome ("no error" when it resolved), safe
   to keep and share. */
const char *hts_dns_strerror(const hts_dns_result *res);
/* Log prefixes for hts_dns_strerror(): with the longest text, both fit in
   htsblk.msg. */
#define HTS_DNS_FAIL_PREFIX "Unable to get server's address: "
#define HTS_DNS_BIND_PREFIX "Unable to bind the local address: "

/* Cached resolver: fill res with iadr's addresses and return res->status.
   Resolves once per host; later calls read the DNS cache. Must hold no lock
   (brackets opt->state.lock itself, never across the resolve). A miss resolves
   on a worker thread bounded by timeout seconds (<= 0: unbounded) and cut short
   as soon as *cancel, if given, is raised; neither outcome is cached. */
hts_dns_status hts_dns_resolve(httrackp *opt, const char *iadr, int timeout,
                               const volatile hts_boolean *cancel,
                               hts_dns_result *res);
/* hts_dns_resolve() bounded by opt->timeout and cut by a mirror stop, copying
   up to max addresses into out. Returns the address count, 0 on failure. */
int hts_dns_resolve_all(httrackp *opt, const char *iadr, SOCaddr *out, int max);
/* A resolve running on a worker thread, so the crawl need not wait for it. */
typedef struct dns_resolve_job hts_dns_job;
/* hts_dns_poll() result while the worker has not answered: a count, not an
   hts_dns_status. */
#define HTS_DNS_PENDING (-2)
/* Does the DNS cache answer for iadr now, positively or not? */
hts_boolean hts_dns_cached(httrackp *opt, const char *iadr);
/* Start resolving iadr's host, returning one reference to the job, or NULL
   when no worker could start. Must hold no lock. */
hts_dns_job *hts_dns_start(httrackp *opt, const char *iadr);
/* Is job resolving iadr's host? */
hts_boolean hts_dns_job_is(const hts_dns_job *job, const char *iadr);
/* Take one more reference to job. */
hts_dns_job *hts_dns_job_ref(hts_dns_job *job);
/* Drop one reference; the worker holds its own. */
void hts_dns_job_release(hts_dns_job *job);
/* HTS_DNS_PENDING while job runs. Then the address count, stored in the DNS
   cache, and the answer in *res if given; -1 if the worker ended without an
   answer. Must hold no lock. */
int hts_dns_poll(httrackp *opt, hts_dns_job *job, hts_dns_result *res);
/* Returns the index of addr in list[0..count-1], port included, or -1. */
int hts_dns_addr_index(const SOCaddr *list, int count, const SOCaddr *addr);
/* Index in iadr's cached list of the address to try first, 0 by default. */
int hts_dns_preferred(httrackp *opt, const char *iadr);
/* Make addr, if it is in iadr's cached list, the one new connects try first.
   The caller must not hold opt->state.lock. */
void hts_dns_set_preferred(httrackp *opt, const char *iadr,
                           const SOCaddr *addr);
/* Uncached and unbounded: first address of hostname into *addr, or NULL. */
SOCaddr *hts_dns_resolve_nocache(const char *const hostname,
                                 SOCaddr *const addr);

/* How long a name that did not resolve stays negative-cached, and the ceiling
   the wait doubles up to. */
#define HTS_DNS_NEGATIVE_TTL_MS 60000
#define HTS_DNS_NEGATIVE_TTL_MAX_MS 900000
/* Wait before asking again, after this many consecutive failures (>= 1). */
int hts_dns_negative_wait_ms(int failures);
/* Test-only: shorten the first wait so a self-test need not wait a minute. */
void hts_dns_set_negative_ttl_ms(int ms);
/* Consecutive failed resolves recorded for host, 0 if it is not cached or
   resolves. Test-only: nothing in the engine reads it back. */
int hts_dns_negative_failures(httrackp *opt, const char *host);
/* Milliseconds off a clock NTP cannot step, for an elapsed time or a
   deadline. Its zero is arbitrary, so it measures an interval and never names
   a date; anything stored or sent still needs mtime_local(). */
TStamp mtime_monotonic(void);
/* Has a timeout of timeout_s seconds run out between the readings
   since_ms and now_ms? Both must come from mtime_monotonic(). A timeout_s <= 0
   has always run out. */
hts_boolean hts_timeout_elapsed(TStamp since_ms, TStamp now_ms, int timeout_s);

/* Test-only: move one cached negative record's stamps by ms, so a self-test
   outlasts a wait (positive) or steps behind them (negative), no sleeps. */
void hts_dns_test_move_clock(httrackp *opt, const char *host, TStamp ms);

// outils divers
HTS_INLINE TStamp time_local(void);

/* Writes duration t in seconds as long text, clipped to the buffer size. */
void sec2str(hts_buf s, TStamp t);

/* The current time as an RFC 822 date, GMT or local, clipped to the buffer
   size. */
void time_gmt_rfc822(hts_buf s);
void time_local_rfc822(hts_buf s);

/* Current UTC time as "YYYY-MM-DDThh:mm:ssZ". */
void hts_now_iso8601(char out[32]);

int set_filetime_rfc822(const char *file, const char *date);
/* File mtime as a UTC time_t, or (time_t) -1 if it can not be read. */
time_t get_filetime(const char *file);
/* Writes file mtime as an RFC 822 GMT date into DATE, or returns 0. */
int get_filetime_rfc822(const char *file, hts_buf date);
/* Writes A as an RFC 822 GMT date, clipped to size bytes. */
HTS_INLINE void time_rfc822(char *s, size_t size, struct tm *A);

HTS_INLINE int sendc(htsblk * r, const char *s);
/* Returns the advance past the line's terminator. */
int binput(const char *buff, hts_buf s);
/* Read one line from a buffer up to "end", consuming it whole even when it
   does not fit S. HTS_TRUE means it was cut: the value is not what was
   written, so drop it rather than parse a prefix of it. The advance is reported
   through *offset. */
hts_boolean binput_line(const char *buff, const char *end, hts_buf s,
                        int *offset);
/* Does treathead() read the header that "line" starts with? Keep in step. */
hts_boolean hts_header_is_read(const char *line);
const char *strstrcase(const char *s, const char *o);
int ident_url_absolute(const char *url, lien_adrfil *adrfil);
/* Collapse "./" and "../" segments of @p f in place. An empty path becomes
   "./", so @p f needs at least 3 bytes. */
void fil_simplifie(char *f);
/* Count each byte value of S into map[256]. */
void map_characters(hts_span s, unsigned int *map);
int ishtml(httrackp * opt, const char *urlfil);
/* Return 1 if EXT is an HTML extension such as "htm", else -1. */
int ishtml_ext(hts_span ext);

/* Write fil's --assume type into s (capacity ssize, NUL included); too long a
   value is clipped, not fatal. True if a rule matched, s then holding its
   value, which "--assume cgi=" leaves empty. */
hts_boolean get_userhttptype(httrackp *opt, char *s, size_t ssize,
                             const char *fil);
int give_mimext(hts_buf s, const char *st);

int may_unknown2(httrackp * opt, const char *mime, const char *filename);

const char *strrchr_limit(const char *s, char c, const char *limit);
/* Returns the end of the authority at s, which must be past the scheme: its
   first '/', '?', '#' or '\'. */
const char *hts_authority_end(const char *s);

/* Spans into one adr or URL string. An absent part has a NULL p, a present
   but empty one ("h:" has an empty port) a non-NULL p and a zero len. */
typedef struct hts_url_view {
  hts_span scheme;   /* without ':'; absent when the input has no scheme */
  hts_span userinfo; /* before the authority's last '@' */
  hts_span host;     /* always present, brackets kept for IPv6 */
  hts_span port;     /* without ':' */
  hts_span path;     /* always present, from the authority's end */
  hts_span query;    /* without '?' */
  hts_span fragment; /* without '#' */
} hts_url_view;

/* Split URL, NULs included, into *V without copying. Input with
   no scheme starts at its authority, as an adr does. The scheme is an engine
   one ("//" optional) or any other before "://". The authority ends at '/',
   '?', '#' or '\'. Syntax only: return FALSE, *V still filled, on
   - an unclosed '[', or bytes after ']' that are not ":port" (no port then);
   - a port with a non-digit or above 65535. */
hts_boolean hts_url_view_parse(hts_span url, hts_url_view *v);

char *jump_protocol(char *source);
const char *jump_protocol_const(const char *source);

/* Split a -P proxy argument "[scheme://][user:pass@]host[:port]" into the proxy
   host string (scheme and any user:pass kept, for later stripping and auth),
   written NUL-terminated into NAME (truncated to fit), and the port in *port.
   The port defaults by scheme: 1080 for socks5/socks5h, else 8080. */
void hts_parse_proxy(const char *arg, hts_buf name, int *port);
/* RFC 2045 caps a base64 line at 76 characters, which encode 57 input bytes. */
#define HTS_BASE64_LINE_BYTES 57
/* Base-64 IN into OUT, with a CRLF every 76 output bytes when CRLF is set. */
void code64(hts_span in, hts_buf out, hts_boolean crlf);

char *convtolower(hts_buf catbuff, const char *a);
int multipleStringMatch(const char *s, const char *match);

/* Writes a header block to fp, each line after prefix, with the value of a
   field that carries a login replaced by the fprint_redacted() marker, and
   each secret opt knows as ***. opt may be NULL. */
void fprintfio(FILE *fp, const char *buff, const char *prefix, httrackp *opt);
/* The first line of buff as fprintfio() shows it, written into dst, clipped:
   for one write, which keeps the line whole on a merged stdout and stderr.
   Returns dst.p. */
const char *hts_header_shown(httrackp *opt, const char *line, hts_buf dst);

/* The backstop behind the per-site masking: opt keeps the first
   HTS_SECRET_MAX passwords and tokens it sees, each up to HTS_SECRET_MAXLEN
   bytes, and the log writers print each as ***. A URL password is hidden only
   as a login, before an '@', so a password such as "index" leaves index.html
   alone. A header token of HTS_SECRET_TOKEN bytes or more is hidden anywhere.
 */
#define HTS_SECRET_MAX 64
#define HTS_SECRET_MAXLEN 1024
#define HTS_SECRET_TOKEN 8

typedef enum hts_secret_kind {
  HTS_SECRET_LOGIN, /* a URL password */
  HTS_SECRET_HEADER /* a header token, kept only from HTS_SECRET_TOKEN bytes */
} hts_secret_kind;

/* Allocates and frees the set of opt->secrets. */
void hts_secret_init(httrackp *opt);
void hts_secret_free(httrackp *opt);

/* Adds s to the set of opt. Thread-safe. Once the set is full, or for a
   secret over HTS_SECRET_MAXLEN bytes, logs one warning and adds nothing. */
void hts_secret_add(httrackp *opt, hts_span s, hts_secret_kind kind);

/* Adds the password of the login of url, in any scheme or none, raw and
   unescaped. A user alone is not added. url may be NULL. */
void hts_secret_add_login(httrackp *opt, const char *url);

/* Adds the values of the Authorization, Proxy-Authorization and Cookie lines
   of a -%X header block, which may be NULL. */
void hts_secret_add_headers(httrackp *opt, const char *headers);

/* fprintf() to fp, with each secret of opt as ***. */
void hts_secret_fprintf(httrackp *opt, FILE *fp,
                        HTS_FORMAT_STRING const char *format, ...)
    HTS_PRINTF_FUN(3, 4);

/* vfprintf() to fp, with each secret of opt as ***. */
void secret_vfprintf(httrackp *opt, FILE *fp,
                     HTS_FORMAT_STRING const char *format, va_list args)
    HTS_PRINTF_FUN(3, 0);

/* Skip adr's blanks and scheme, then its login. */
const char *login_start(const char *adr);
const char *login_end(const char *adr);
/* Copies url, a URL a user typed, to dst with each '?', '#'
   and '\' before the last '@' preceding the first '/' written as %XX, so its
   login reads as it always has. Returns HTS_FALSE when dst was too small. */
hts_boolean hts_escape_typed_login(const char *url, hts_buf dst);
/* Copies src to dst, writing each byte of set that lies inside escaped, a
   part of src, as %XX. An escape that does not fit is dropped whole. Returns
   HTS_FALSE when dst was too small. */
hts_boolean hts_escape_span(hts_span src, hts_span escaped, const char *set,
                            hts_buf dst);

/* Is the name opening LINE one of the NULL-terminated NAMES, in any case? */
hts_boolean header_name_in(const char *line, const char *const *names);

/* What hts_mask_passwords() reads: */
typedef enum {
  HTS_MASK_LIST, /* blank-separated start URLs and filters */
  HTS_MASK_ADDR  /* one start URL, or a -P value */
} hts_mask_mode;

/* Returns a copy of s, allocated with malloct, in which the password, or the
   user alone, of every start URL or proxy address in s is *** . The rest stays
   byte for byte. NULL when out of memory. */
char *hts_mask_passwords(const char *s, hts_mask_mode mode);

/* Log calls never print an address raw: pass adr_nologin() or url_nologin(),
   which test 660 checks. */

/* Returns adr with its login masked as user:*** or ***, for log lines: adr
   itself when it has no login, else a copy clipped to BUF. */
const char *adr_nologin_r(hts_buf buf, const char *adr);

/* Same as adr_nologin_r(), but only a URL with a scheme or "//" has a login.
   Any "scheme://" counts, not only the ones the engine fetches. */
const char *url_nologin_r(char *buf, size_t size, const char *url);

/* adr_nologin_r() and url_nologin_r() into a rotating buffer of opt, which
   they take only when there is a login. A NULL opt drops the login, and a
   NULL adr returns "". */
const char *adr_nologin(httrackp *opt, const char *adr);
const char *url_nologin(httrackp *opt, const char *url);

/* Writes the marker that stands for a hidden secret of len bytes. */
void fprint_redacted(FILE *fp, size_t len);

/* A write to a socket whose peer is gone raises SIGPIPE, and the default
   action kills the process. The library must not install a handler, because
   the process belongs to whoever embedded us. Three steps leave the host's
   signals alone, and each covers what the others cannot:
     - HTS_MSG_NOSIGNAL on every send() we issue. Absent on macOS, and
       defined but ignored on GNU/Hurd, where the signal still arrives.
     - SO_NOSIGPIPE on every socket we write to. Absent on Linux, and the only
       one that reaches OpenSSL, which writes with write(2).
     - the mask below, around every write the other two may not cover. */
#ifdef MSG_NOSIGNAL
#define HTS_MSG_NOSIGNAL MSG_NOSIGNAL
#else
#define HTS_MSG_NOSIGNAL 0
#endif

void socket_set_nosigpipe(T_SOC soc);

/* Multipath TCP (RFC 8684). On Linux it is a protocol on an ordinary stream
   socket; on macOS it is an AF_MULTIPATH socket connected with connectx().
   configure decides this, and a build without it, such as MSVC, has none. */
#if !defined(HTS_INET_MPTCP) || defined(_WIN32)
#undef HTS_INET_MPTCP
#define HTS_INET_MPTCP 0
#endif

/* Create a stream socket for an outgoing connection, with SIGPIPE disarmed.
   Uses Multipath TCP when `opt` asks for it and the system has it, and plain
   TCP otherwise. Pass `will_bind` when the caller pins a source address, which
   rules Multipath TCP out. Returns INVALID_SOCKET on failure. */
T_SOC hts_socket_client(int family, const httrackp *opt, hts_boolean will_bind);

/* Can this build, on this system, open a Multipath TCP socket at all? */
hts_boolean hts_mptcp_available(void);

/* Can this build tell a negotiated connection from a fallen-back one? */
hts_boolean hts_mptcp_reports(void);

/* Connect a socket hts_socket_client() returned. macOS reaches Multipath TCP
   through connectx() rather than a protocol, so the connect differs there too.
   Returns 0, or -1 with errno set, exactly as connect() does. */
int hts_socket_connect(T_SOC soc, const struct sockaddr *addr, SOClen len,
                       const httrackp *opt);

/* Start a non-blocking connect to *server. Returns HTS_TRUE when it succeeded
   or is in progress, and sets *pending (if non-NULL) when it is in progress. */
hts_boolean hts_socket_connect_started(T_SOC soc, const SOCaddr *server,
                                       const httrackp *opt,
                                       hts_boolean *pending);

/* Did Multipath TCP survive this established connection's handshake? */
hts_boolean hts_socket_is_mptcp(T_SOC soc);

/* Count an established connection as MPTCP or as fallen back to TCP. Counts
   nothing when this run did not ask for MPTCP, and covers the HTTP backend
   only, because an FTP socket is established on its own worker thread. */
void hts_mptcp_account(httrackp *opt, T_SOC soc);

/* Windows raises no SIGPIPE. Elsewhere this arm is Darwin, which has no
   sigtimedwait and does have SO_NOSIGPIPE, so the socket already covers it. */
#if defined(_WIN32) || !defined(HAVE_SIGTIMEDWAIT)
typedef struct {
  int unused;
} sigpipe_mask;

static HTS_INLINE HTS_UNUSED void sigpipe_hold(sigpipe_mask *m) { (void) m; }

static HTS_INLINE HTS_UNUSED void sigpipe_release(sigpipe_mask *m) { (void) m; }
#else
#if defined(THREADS)
#define HTS_SIGMASK(how, set, old) pthread_sigmask(how, set, old)
#else
#define HTS_SIGMASK(how, set, old) sigprocmask(how, set, old)
#endif

/* Thread mask only, so the host's handler and disposition are untouched. */
typedef struct {
  sigset_t mask;   /* what to put back */
  int held;        /* the mask above is ours to restore */
  int was_pending; /* the host had one queued already, so leave it alone */
} sigpipe_mask;

static HTS_INLINE HTS_UNUSED void sigpipe_hold(sigpipe_mask *m) {
  sigset_t block, pending;

  m->held = 0;
  m->was_pending =
      sigpending(&pending) == 0 && sigismember(&pending, SIGPIPE) == 1;
  if (sigemptyset(&block) == 0 && sigaddset(&block, SIGPIPE) == 0 &&
      HTS_SIGMASK(SIG_BLOCK, &block, &m->mask) == 0)
    m->held = 1;
}

static HTS_INLINE HTS_UNUSED void sigpipe_release(sigpipe_mask *m) {
  sigset_t ours;
  struct timespec now = {0, 0};

  if (!m->held)
    return;
  /* Take back the one our own write queued, or nothing. sigtimedwait() with a
     zero timeout cannot block, where sigwait() hangs for good if another
     thread takes a process-directed SIGPIPE first. */
  if (!m->was_pending && sigemptyset(&ours) == 0 &&
      sigaddset(&ours, SIGPIPE) == 0)
    (void) sigtimedwait(&ours, NULL, &now);
  (void) HTS_SIGMASK(SIG_SETMASK, &m->mask, NULL);
}
#endif

/* A send() that cannot raise SIGPIPE. Linux honours the flag alone, but
   GNU/Hurd defines MSG_NOSIGNAL and still kills the process, which is how a
   library takes down whoever embedded it (#1689). A length past INT_MAX is
   clipped, which the caller sees as a short write. */
static HTS_INLINE HTS_UNUSED int hts_send_nosignal(T_SOC soc, const char *s,
                                                   size_t len) {
#ifdef __clang_analyzer__
  // the analyzer reads the INT_MAX clip as a huge send
  const int ilen = (int) len;
#else
  const int ilen = len > INT_MAX ? INT_MAX : (int) len;
#endif
#if defined(__linux__) && defined(MSG_NOSIGNAL)
  return (int) send(soc, s, ilen, HTS_MSG_NOSIGNAL);
#else
  sigpipe_mask m;
  int n, err;

  sigpipe_hold(&m);
  n = (int) send(soc, s, ilen, HTS_MSG_NOSIGNAL);
  /* Save errno across the release, whose sigtimedwait() sets EAGAIN when
     nothing was pending, which callers would read as a would-block. */
  err = errno;
  sigpipe_release(&m);
  errno = err;
  return n;
#endif
}

int fexist(const char *s);
int fexist_utf8(const char *s);

/* File size in bytes, -1 if absent or not a regular file. LLint, not off_t:
   the latter is a 32-bit long on MSVC, truncating any file of 2GB or more. */
LLint fpsize(FILE *fp);
LLint fsize(const char *s);
LLint fsize_utf8(const char *s);

/* variables globales */
extern int _DEBUG_HEAD;
extern FILE *ioinfo;

// htsmodule.c definitions
extern void *openFunctionLib(const char *file_);
extern void *getFunctionPtr(void *handle, const char *fncname);
extern void closeFunctionLib(void *handle);

int hts_set_callback(t_hts_htmlcheck_callbacks * callbacks,
                     const char *name, void *function);
void *hts_get_callback(t_hts_htmlcheck_callbacks * callbacks,
                       const char *name);

#define CBSTRUCT(OPT) ((t_hts_htmlcheck_callbacks*) ((OPT)->callbacks_fun))
#define GET_USERCALLBACK(OPT, NAME) ( CBSTRUCT(OPT)-> NAME .fun )
#define GET_USERARG(OPT, NAME) ( CBSTRUCT(OPT)-> NAME .carg )
/* True when a front end registered its own NAME callback */
#define HAS_CALLBACK(OPT, NAME)                                                \
  (CBSTRUCT(OPT) != NULL && CBSTRUCT(OPT)->NAME.fun != NULL &&                 \
   CBSTRUCT(OPT)->NAME.fun != default_callbacks.NAME.fun)
#define GET_CALLBACK(OPT, NAME)                                                \
  (HAS_CALLBACK(OPT, NAME) ? (GET_USERCALLBACK(OPT, NAME))                     \
                           : (default_callbacks.NAME.fun))

/* Predefined macros */
#define RUN_CALLBACK_NOARG(OPT, NAME) GET_CALLBACK(OPT, NAME)(GET_USERARG(OPT, NAME))
#define RUN_CALLBACK0(OPT, NAME) GET_CALLBACK(OPT, NAME)(GET_USERARG(OPT, NAME), OPT)
#define RUN_CALLBACK1(OPT, NAME, ARG1) GET_CALLBACK(OPT, NAME)(GET_USERARG(OPT, NAME), OPT, ARG1)
#define RUN_CALLBACK2(OPT, NAME, ARG1, ARG2) GET_CALLBACK(OPT, NAME)(GET_USERARG(OPT, NAME), OPT, ARG1, ARG2)
#define RUN_CALLBACK3(OPT, NAME, ARG1, ARG2, ARG3) GET_CALLBACK(OPT, NAME)(GET_USERARG(OPT, NAME), OPT, ARG1, ARG2, ARG3)
#define RUN_CALLBACK4(OPT, NAME, ARG1, ARG2, ARG3, ARG4) GET_CALLBACK(OPT, NAME)(GET_USERARG(OPT, NAME), OPT, ARG1, ARG2, ARG3, ARG4)
#define RUN_CALLBACK5(OPT, NAME, ARG1, ARG2, ARG3, ARG4, ARG5) GET_CALLBACK(OPT, NAME)(GET_USERARG(OPT, NAME), OPT, ARG1, ARG2, ARG3, ARG4, ARG5)
#define RUN_CALLBACK6(OPT, NAME, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6) GET_CALLBACK(OPT, NAME)(GET_USERARG(OPT, NAME), OPT, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6)
#define RUN_CALLBACK7(OPT, NAME, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7)     \
  GET_CALLBACK(OPT, NAME)(GET_USERARG(OPT, NAME), OPT, ARG1, ARG2, ARG3, ARG4, \
                          ARG5, ARG6, ARG7)

#endif // internals

#undef PATH_SEPARATOR
#ifdef _WIN32
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

     /* clang-format off */
/* One byte per column, so the four classes read side by side. Nothing down
   to the next marker survives clang-format either. */
#define  is_space(c)      ( ((c)==' ') || ((c)=='\"') || ((c)==10) || ((c)==13) || ((c)==9) || ((c)==12) || ((c)==11) || ((c)=='\'') )
#define  is_realspace(c)  hts_isspace(c)
#define  is_taborspace(c) ( ((c)==' ')                                          || ((c)==9)                             )
#define  is_retorsep(c)   (                              ((c)==10) || ((c)==13) || ((c)==9)                                          )
//HTS_INLINE int is_space(char);
//HTS_INLINE int is_realspace(char);

#define HTTP_IS_REDIRECT(code) ( \
     (code) == 301               \
  || (code) == 302               \
  || (code) == 303               \
  || (code) == 307               \
  || (code) == 308               \
  )
#define HTTP_IS_NOTMODIFIED(code) ( \
     (code) == 304               \
  )
#define HTTP_IS_OK(code) ( ( (code) / 100 ) == 2 )
#define HTTP_IS_ERROR(code) ( !HTTP_IS_OK(code) && !HTTP_IS_REDIRECT(code) && !HTTP_IS_NOTMODIFIED(code) )

// 0 when f does not start with s, else the index in f just past the match.
// Compares without regard to case.
HTS_STATIC int strfield(const char *f, const char *s) {
  int r = 0;

  while(streql(*f, *s) && ((*f) != 0) && ((*s) != 0)) {
    f++;
    s++;
    r++;
  }
  if (*s == 0)
    return r;
  else
    return 0;
}

/* clang-format on */

/* MSVC ships these POSIX functions under other names. Kept out of the installed
   headers: they would rewrite the same identifiers in a consumer's own code. */
#ifdef _MSC_VER
#define fseeko _fseeki64
#define ftello _ftelli64
#endif

#define strfield2(f,s) ( (strlen(f)!=strlen(s)) ? 0 : (strfield(f,s)) )

// is this MIME an hypertext MIME (text/html), html/js-style or other script/text type?
#define HTS_HYPERTEXT_DEFAULT_MIME "text/html"
/* Cached when the server sent no Content-Type. Type tests treat it as html,
   but wire_patches_ext() keeps the URL extension. */
#define HTS_UNKNOWN_MIME "unknown/unknown"
/* Map the no-declared-type sentinel back to a real type for any header or
   record we EMIT or PERSIST, so "unknown/unknown" never reaches a consumer
   (a served Content-Type, a ProxyTrack .arc record, ...). */
#define hts_effective_mime(m)                                                  \
  (strfield2((m), HTS_UNKNOWN_MIME) ? HTS_HYPERTEXT_DEFAULT_MIME : (m))

#define is_html_mime_type(a)                                                   \
  ((strfield2((a), "text/html") != 0) ||                                       \
   (strfield2((a), "application/xhtml+xml") != 0) ||                           \
   (strfield2((a), HTS_UNKNOWN_MIME) !=                                        \
    0) /* no declared type: treat as html */                                   \
  )
/* Every JavaScript type we link-scan: IANA's text/javascript, which is what
   servers send and what our own table emits for .mjs, plus the legacy
   application spellings .js still maps to. */
#define is_javascript_mime_type(a)                                             \
  ((strfield2((a), "text/javascript") != 0) ||                                 \
   (strfield2((a), "application/javascript") != 0) ||                          \
   (strfield2((a), "application/x-javascript") != 0) ||                        \
   (strfield2((a), "application/ecmascript") != 0))
#define is_hypertext_mime__(a)                                                 \
  (is_html_mime_type(a) || is_javascript_mime_type(a) ||                       \
   (strfield2((a), "text/css") !=                                              \
    0) /*|| (strfield2((a),"text/vnd.wap.wml")!=0)*/                           \
   || (strfield2((a), "image/svg+xml") != 0) ||                                \
   (strfield2((a), "image/svg-xml") !=                                         \
    0) /*|| (strfield2((a),"audio/x-pn-realaudio")!=0) */                      \
   || (strfield2((a), "application/x-authorware-map") != 0) ||                 \
   (strfield2((a), "application/rss+xml") != 0) ||                             \
   (strfield2((a), "application/atom+xml") != 0))
/* Every spelling servers use for an m3u playlist, HLS's included. */
#define is_playlist_mime_type(a)                                               \
  ((strfield2((a), "audio/x-mpegurl") != 0) ||                                 \
   (strfield2((a), "audio/mpegurl") != 0) ||                                   \
   (strfield2((a), "application/x-mpegurl") != 0) ||                           \
   (strfield2((a), "application/vnd.apple.mpegurl") != 0))
/* TODO: text/xml and application/xml, once their content is checked. */
#define may_be_hypertext_mime__(a)                                             \
  ((strfield2((a), "audio/x-pn-realaudio") != 0) || is_playlist_mime_type(a))

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE

// check if (mime, file) is hypertext
HTS_STATIC int is_hypertext_mime(httrackp * opt, const char *mime,
                                 const char *file) {
  if (is_hypertext_mime__(mime))
    return 1;
  if (may_unknown(opt, mime)) {
    char guessed[256];

    guessed[0] = '\0';
    if (!guess_httptype_sized(opt, guessed, sizeof(guessed), file))
      return 0;
    return is_hypertext_mime__(guessed);
  }
  return 0;
}

// check if (mime, file) is JavaScript, guessing from the extension as above
HTS_STATIC int is_javascript_mime(httrackp *opt, const char *mime,
                                  const char *file) {
  if (is_javascript_mime_type(mime))
    return 1;
  if (file != NULL && file[0] != '\0' && may_unknown(opt, mime)) {
    char guessed[256];

    guessed[0] = '\0';
    if (!guess_httptype_sized(opt, guessed, sizeof(guessed), file))
      return 0;
    return is_javascript_mime_type(guessed);
  }
  return 0;
}

// check if (mime, file) is an m3u playlist, guessing from the extension
HTS_STATIC int is_playlist_mime(httrackp *opt, const char *mime,
                                const char *file) {
  if (is_playlist_mime_type(mime))
    return 1;
  if (file != NULL && file[0] != '\0' && may_unknown(opt, mime)) {
    char guessed[256];

    guessed[0] = '\0';
    if (!guess_httptype_sized(opt, guessed, sizeof(guessed), file))
      return 0;
    return is_playlist_mime_type(guessed);
  }
  return 0;
}

// check if (mime, file) might be "false" hypertext
HTS_STATIC int may_be_hypertext_mime(httrackp * opt, const char *mime,
                                     const char *file) {
  if (may_be_hypertext_mime__(mime))
    return 1;
  if (file != NULL && file[0] != '\0' && may_unknown(opt, mime)) {
    char guessed[256];

    guessed[0] = '\0';
    if (!guess_httptype_sized(opt, guessed, sizeof(guessed), file))
      return 0;
    return may_be_hypertext_mime__(guessed);
  }
  return 0;
}

// compare (mime, file) with reference
HTS_STATIC int compare_mime(httrackp * opt, const char *mime, const char *file,
                            const char *reference) {
  if (is_hypertext_mime__(mime) || may_be_hypertext_mime__(mime))
    return strfield2(mime, reference);
  if (file != NULL && file[0] != '\0' && may_unknown(opt, mime)) {
    char guessed[256];

    guessed[0] = '\0';
    if (!guess_httptype_sized(opt, guessed, sizeof(guessed), file))
      return 0;
    return strfield2(guessed, reference);
  }
  return 0;
}

#endif

// returns (size_t) -1 upon error
static HTS_UNUSED size_t llint_to_size_t(LLint o) {
  const size_t so = (size_t) o;
  if ((LLint) so == o) {
    return so;
  } else {
    return (size_t) -1;
  }
}

/* Capacity for @p used bytes plus @p extra more plus @p slack spare;
   (size_t) -1 if the total exceeds (size_t) -2 or @p extra is negative
   (llint_to_size_t() would map that to a huge valid-looking size). */
static HTS_UNUSED size_t llint_grow_size_t(size_t used, LLint extra,
                                           size_t slack) {
  const size_t max = (size_t) -2; /* (size_t) -1 is the error value */
  const size_t e = extra >= 0 ? llint_to_size_t(extra) : (size_t) -1;

  if (e == (size_t) -1 || used > max || slack > max - used ||
      e > max - used - slack) {
    return (size_t) -1;
  }
  return used + e + slack;
}

/* dirent() compatibility */
#ifdef _WIN32
/* Holds a UTF-8 d_name: MAX_PATH (260) UTF-16 units expand to <=3 bytes each.
   Windows-only struct, ABI free to break. */
#define HTS_DIRENT_SIZE 1024
struct dirent {
  ino_t d_ino;                  /* ignored */
  off_t d_off;                  /* ignored */
  unsigned short d_reclen;      /* ignored */
  unsigned char d_type;         /* ignored */
  char d_name[HTS_DIRENT_SIZE]; /* filename */
};
typedef struct DIR DIR;
struct DIR {
  HANDLE h;
  struct dirent entry;
  char *name;
};
DIR *opendir(const char *name);
struct dirent *readdir(DIR * dir);
int closedir(DIR * dir);
#endif

/* Like fil_normalized(), but first drops query keys in STRIP (comma-separated,
   "*" = all); STRIP NULL/empty behaves exactly like fil_normalized(). */
char *fil_normalized_filtered(const char *source, char *dest,
                              const char *strip);

/* As fil_normalized_filtered(), but DO_SLASH/DO_QUERY gate the // collapse and
   the query-argument sort independently (the urlhack sub-flags). */
char *fil_normalized_filtered_ex(const char *source, char *dest,
                                 const char *strip, int do_slash, int do_query);

/* For URL ADR/FIL, return (in DEST) the comma keylist to strip from the
   '\n'-separated "[pattern=]keys" RULES (patterns matched on host/path via
   strjoker, last wins); NULL if none match. Feeds fil_normalized_filtered(). */
const char *hts_query_strip_keys(const char *rules, const char *adr,
                                 const char *fil, hts_buf dest);

/* Is KEY in the comma list STRIP? Tokens are space-trimmed and
   case-sensitive, and "*" matches any key. */
hts_boolean hts_query_key_stripped(hts_span key, const char *strip);

/* Return the canonical host (length *CANONLEN) of the last RULES entry whose
   aliases match HOST or FULL ("scheme://host"), or NULL. COLLAPSE_WWW ignores
   a "www." prefix on both sides. */
const char *hts_host_alias_match(const char *rules, const char *host,
                                 const char *full, hts_boolean collapse_www,
                                 size_t *canonlen);

/* The --host-alias rules of OPT, or NULL when the option was not given.
   While a mirror runs it returns the hash's snapshot, so names match keys. */
const char *hts_host_alias_rules(httrackp *opt);

/* Return the query-strip keys of OPT or NULL, like hts_host_alias_rules(). */
const char *hts_strip_query_rules(httrackp *opt);

/* ADR with its host replaced by the canonical one, from the '\n'-separated
   "alias[,alias...]=canonical" RULES (aliases matched with strjoker, last
   wins). NOWWW matches the host with its www. prefix collapsed, as -%u keys it.
   Chains are followed to their end, so the result is a fixpoint. Scheme and
   credentials are kept. NULL when no rule applies, the mapping is a no-op, the
   rules loop, or the result would not fit DEST. */
const char *hts_host_alias(const char *rules, const char *adr,
                           hts_boolean collapse_www, hts_buf dest);

/* Resolve like hts_host_alias, and set *SETTLED, when SETTLED is not NULL, to
   whether the chain ended within the hop limit. Exposed for the -#test=spandiff
   test. */
const char *hts_host_alias_resolve(const char *rules, const char *adr,
                                   hts_boolean collapse_www, hts_buf dest,
                                   hts_boolean *settled);

/* HTS_TRUE when OPT's url hacks collapse www.host onto host. */
hts_boolean hts_host_alias_collapse_www(httrackp *opt);

/* Rewrite AF's host to its --host-alias canonical form, in place, and return
   it. Idempotent; returns af->adr unchanged when no rule applies. */
const char *hts_host_alias_fold(httrackp *opt, lien_adrfil *af);

/* HTS_TRUE if ADRA and ADRB name the same host once RULES are applied. */
hts_boolean hts_host_same_alias(const char *rules, const char *adra,
                                const char *adrb, hts_boolean collapse_www);

/* First canonical host in RULES whose chain never ends ("a=b" plus "b=a"),
   copied into DEST, or NULL if every chain settles. Those hosts are left
   unaliased, so the engine reports them rather than picking a hop. */
const char *hts_host_alias_looping(const char *rules, hts_boolean collapse_www,
                                   hts_buf dest);

#endif
