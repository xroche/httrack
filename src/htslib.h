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

/* basic net definitions */
#include "htsbase.h"
#include "htsbasenet.h"
#include "htsnet.h"
#include "htsdefines.h"

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
hts_boolean hts_location_is_safe(const char *location);

#include "htsopt.h"

#define READ_ERROR (-1)
#define READ_EOF (-2)
#define READ_TIMEOUT (-3)
#define READ_INTERNAL_ERROR (-4)

/* concat */

#if ( defined(_WIN32) && defined(_MSC_VER) && ( _MSC_VER >= 1300 ) && (_MSC_VER <= 1310 ) )
/* NOTE: VC2003 inlining bug in optim mode not respecting function call sequence point */
#define MSVC2003INLINEBUG __declspec(noinline)
#else
#define MSVC2003INLINEBUG
#endif
MSVC2003INLINEBUG HTS_STATIC char *getHtsOptBuff_(httrackp * opt) {
  opt->state.concat.index = (opt->state.concat.index + 1) % 16;
  return opt->state.concat.buff[opt->state.concat.index];
}

#undef MSVC2003INLINEBUG
#define OPT_GET_BUFF(OPT)      ( getHtsOptBuff_(OPT) )
#define OPT_GET_BUFF_SIZE(OPT) ( sizeof(opt->state.concat.buff[0]) )

/* ANCIENNE STURCTURE pour cache 1.0 */
#ifndef HTS_DEF_FWSTRUCT_OLD_t_proxy
#define HTS_DEF_FWSTRUCT_OLD_t_proxy
typedef struct OLD_t_proxy OLD_t_proxy;
#endif
struct OLD_t_proxy {
  int active;
  char name[1024];
  int port;
};

#ifndef HTS_DEF_FWSTRUCT_OLD_htsblk
#define HTS_DEF_FWSTRUCT_OLD_htsblk
typedef struct OLD_htsblk OLD_htsblk;
#endif
struct OLD_htsblk {
  int statuscode;               // ANCIENNE STURCTURE - status-code, -1=erreur, 200=OK,201=..etc (cf RFC1945)
  int notmodified;              // ANCIENNE STURCTURE - page ou fichier NON modifié (transféré)
  int is_write;                 // ANCIENNE STURCTURE - sortie sur disque (out) ou en mémoire (adr)
  char *adr;                    // ANCIENNE STURCTURE - adresse du bloc de mémoire, NULL=vide
  FILE *out;                    // ANCIENNE STURCTURE - écriture directe sur disque (si is_write=1)
  int size;                     // ANCIENNE STURCTURE - taille fichier
  char msg[80];                 // ANCIENNE STURCTURE - message éventuel si échec ("\0"=non précisé)
  char contenttype[64];         // ANCIENNE STURCTURE - content-type ("text/html" par exemple)
  char *location;               // ANCIENNE STURCTURE - on copie dedans éventuellement la véritable 'location'
  int totalsize;                // ANCIENNE STURCTURE - taille totale à télécharger (-1=inconnue)
  int is_file;                  // ANCIENNE STURCTURE - ce n'est pas une socket mais un descripteur de fichier si 1
  T_SOC soc;                    // ANCIENNE STURCTURE - ID socket
  FILE *fp;                     // ANCIENNE STURCTURE - fichier pour file://
  OLD_t_proxy proxy;            // ANCIENNE STURCTURE - proxy
  int user_agent_send;          // ANCIENNE STURCTURE - user agent (ex: httrack/1.0 [sun])
  char user_agent[64];
  int http11;                   // ANCIENNE STURCTURE - l'en tête doit être signé HTTP/1.1 et non HTTP/1.0
};

/* fin ANCIENNE STURCTURE pour cache 1.0 */

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

// version
HTSEXT_API const char* hts_version(void);

// fonctions unix/winsock
int hts_read(htsblk * r, char *buff, int size);

/* Add to, read and reset the received-byte total, which an FTP worker counts on
   its own thread while the crawl thread counts the HTTP bytes: a lost += reads
   low, and -M gives up on that total (back_maxsize_reached()). A lock, not a
   64-bit atomic add, which needs libatomic where the target has no 8-byte
   compare-and-swap. hts_stat_recv_publish() copies the total to the
   HTS_STAT.HTS_TOTAL_RECV a front end reads, on the crawl thread beside every
   other field engine_stats() refreshes there. */
void hts_stat_recv_add(LLint bytes);
LLint hts_stat_recv_get(void);
void hts_stat_recv_set(LLint bytes);
void hts_stat_recv_publish(void);

LLint check_downloadable_bytes(int rate);

int hts_uninit_module(void);

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
T_SOC http_xfopen(httrackp * opt, int mode, int treat, int waitconnect,
                  const char *xsend, const char *adr, const char *fil, htsblk * retour);
int http_sendhead(httrackp *opt, t_cookie *cookie, int mode, const char *xsend,
                  const char *adr, const char *fil, const char *referer_adr,
                  const char *referer_fil, const hts_fetch_meta *fetch,
                  htsblk *retour);
/* Build the request "Cookie:" header line for stored cookies matching
   adr/path into dst (NUL-terminated), wrapping the logic http_sendhead()
   uses. adr may carry its scheme, and only "https://" gets Secure cookies. A
   port in adr is ignored (see cookie_host). Returns cookies emitted. */
int http_cookie_header(t_cookie *cookie, const char *adr, const char *path,
                       char *dst, size_t dst_size);

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

/* Append the custom header block to dst (capacity dst_size), dropping any line
   whose field dst carries, a malformed or framing line, an earlier copy of
   Host, Cookie or Authorization, and a leading folded line. Field names ignore
   case and blanks before the colon. A line that does not fit is left out. */
void http_append_custom_headers(char *dst, size_t dst_size,
                                const char *headers);

/* Append the raw remainder of a >postfile: body into buffer (capacity
   buffer_size) at write position pos, and return the new position. Clips to
   the room left, and always leaves a NUL at the position returned: the request
   block is read back with strlen() by the WARC stash, the sendhead callback
   and sendc(). A NUL inside the file ends the request there. */
size_t http_postfile_body(char *buffer, size_t buffer_size, size_t pos,
                          FILE *fp);

/* Switch soc between blocking and non-blocking mode; false on failure, with
   errno (WSAGetLastError() on Windows) set. */
hts_boolean socket_set_nonblocking(T_SOC soc, hts_boolean nonblocking);
/* Pending-connect result for a non-blocking socket reported ready by select():
   0 = connected, >0 = the connect errno (refused, unreachable, ...), -1 if the
   probe itself failed. A failed connect is reported ready as well (writable on
   posix, exception set on winsock), so this is how success is told from failure
   without blocking. */
int connect_socket_error(T_SOC soc);

T_SOC newhttp(httrackp * opt, const char *iadr, htsblk * retour, int port,
              int waitconnect);
/* Like newhttp(), but connect to the addr_index-th resolved address of the host
   (0-based) instead of always the first; *addr_count, if non-NULL, is set to
   the total resolved addresses. newhttp() == newhttp_addr(...,0,NULL). Used by
   the slot scheduler to try the next address when a connect fails (dead IPv6
   etc.). */
T_SOC newhttp_addr(httrackp *opt, const char *iadr, htsblk *retour, int port,
                   int waitconnect, int addr_index, int *addr_count);
/* Clips the formatted failure reason into r->msg, which also round-trips
   through the cache as X-StatusMessage. Leaves r->statuscode to the caller. */
#define htsblk_failf(R, ...)                                                   \
  slprintfbuff_clip((R)->msg, sizeof((R)->msg), __VA_ARGS__)

HTS_INLINE void deletehttp(htsblk * r);
HTS_INLINE int deleteaddr(htsblk * r);
HTS_INLINE void deletesoc(T_SOC soc);
HTS_INLINE void deletesoc_r(htsblk *r);
int check_readinput(htsblk * r);
int check_readinput_t(T_SOC soc, int timeout);
int check_writeinput_t(T_SOC soc, int timeout);

/* TRUE if str[0..len) holds no byte below ' '. A control byte in a value the
   client puts on a protocol line (an FTP command, a CONNECT authority, a
   SOCKS5 host) smuggles a line or a field of its own. */
hts_boolean hts_is_control_free_sized(const char *str, size_t len);

/* Same over a NUL-terminated string. */
hts_boolean hts_is_control_free(const char *str);

/* TRUE if host[0..len) is an IPv4 literal: digits and dots, at least one dot.
   Such a host has no domain structure to reverse or to widen into. */
hts_boolean hts_host_is_ipv4(const char *host, size_t len);

/* TRUE if host[0..len) is an IPv4 or IPv6 address literal. */
hts_boolean hts_host_is_address(const char *host, size_t len);

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

/* The three-digit HTTP status code at s, or -1 when s does not start with
   exactly three digits. */
int hts_status_code(const char *s);

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
/* Does an in-memory body of this size fit? r->adr is indexed with an int
   downstream and its allocations add a trailing NUL, so every producer of
   r->size must refuse where http_xfread1() does. */
hts_boolean hts_inmem_size_fits(LLint size);
/* Cached resolver: fill out[0..count-1] with up to max addresses for iadr (in
   resolver order), returning the count (0 = does not resolve, negative-cached).
   Resolves once per host; later calls read the DNS cache. Must hold no lock
   (brackets opt->state.lock itself, never across the resolve). A miss resolves
   on a worker thread bounded by opt->timeout and cut short by a mirror stop;
   either reports 0, uncached. */
int hts_dns_resolve_all(httrackp *opt, const char *iadr, SOCaddr *out, int max,
                        const char **error);
/* Like hts_dns_resolve_all(), with the wait bounded by timeout seconds (<= 0:
   unbounded) and cut short as soon as *cancel, if given, is raised. */
int hts_dns_resolve_all_bounded(httrackp *opt, const char *iadr, SOCaddr *out,
                                int max, int timeout,
                                const volatile hts_boolean *cancel,
                                const char **error);
HTS_INLINE SOCaddr *hts_dns_resolve2(httrackp *opt, const char *iadr,
                                     SOCaddr *const addr, const char **error);
SOCaddr *hts_dns_resolve_nocache2(const char *const hostname,
                                  SOCaddr *const addr, const char **error);
SOCaddr *hts_dns_resolve_nocache(const char *const hostname,
                                 SOCaddr *const addr);
int check_hostname_dns(const char *const hostname);

int ftp_available(void);

#if HTS_DNSCACHE
/* Return opt's DNS cache hashtable (hostname -> t_dnscache record), creating it
   on first use. Records are owned by the table and freed on coucal_delete. */
coucal hts_cache(httrackp *opt);
#endif

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

/* Test-only: move one cached negative record's stamps by ms, so a self-test
   outlasts a wait (positive) or steps behind them (negative), no sleeps. */
void hts_dns_test_move_clock(httrackp *opt, const char *host, TStamp ms);

// outils divers
HTS_INLINE TStamp time_local(void);

void sec2str(char *s, TStamp t);

void time_gmt_rfc822(char *s);
void time_local_rfc822(char *s);

/* Current UTC time as "YYYY-MM-DDThh:mm:ssZ". */
void hts_now_iso8601(char out[32]);

int set_filetime(const char *file, struct tm *tm_time);
int set_filetime_rfc822(const char *file, const char *date);
/* File mtime as a UTC time_t, or (time_t) -1 if it can not be read. */
time_t get_filetime(const char *file);
int get_filetime_rfc822(const char *file, char *date);
HTS_INLINE void time_rfc822(char *s, struct tm *A);
HTS_INLINE void time_rfc822_local(char *s, struct tm *A);

HTS_INLINE int sendc(htsblk * r, const char *s);
/* Returns the advance past the line's terminator. Unlike binput_line(), "max"
   excludes the NUL: "s" must hold max + 1 bytes. */
int binput(const char *buff, char *s, int max);
/* Read one line, from a buffer up to "end" or from a socket, consuming it whole
   even when it does not fit "s". HTS_TRUE means it was cut: the value is not
   what was written, so drop it rather than parse a prefix of it.
   binput_line() reports the advance through *offset. */
hts_boolean binput_line(const char *buff, const char *end, char *s, int max,
                        int *offset);
hts_boolean finput_line(T_SOC fd, char *s, int max);
/* Does treathead() read the header that "line" starts with? Keep in step. */
hts_boolean hts_header_is_read(const char *line);
int linput(FILE * fp, char *s, int max);
int linputsoc(T_SOC soc, char *s, int max);
int linputsoc_t(T_SOC soc, char *s, int max, int timeout);
const char *strstrcase(const char *s, const char *o);
int ident_url_absolute(const char *url, lien_adrfil *adrfil);
void fil_simplifie(char *f);
void map_characters(unsigned char *buffer, unsigned int size,
                    unsigned int *map);
int ishtml(httrackp * opt, const char *urlfil);
int ishtml_ext(const char *a);
int ishttperror(int err);

/* Write fil's --assume type into s (capacity ssize, NUL included); too long a
   value is clipped, not fatal. True if a rule matched, s then holding its
   value, which "--assume cgi=" leaves empty. */
hts_boolean get_userhttptype(httrackp *opt, char *s, size_t ssize,
                             const char *fil);
int give_mimext(char *s, size_t ssize, const char *st);

int may_bogus_multiple(httrackp * opt, const char *mime, const char *filename);
int may_unknown2(httrackp * opt, const char *mime, const char *filename);

const char *strrchr_limit(const char *s, char c, const char *limit);
char *jump_protocol(char *source);
const char *jump_protocol_const(const char *source);

/* Split a -P proxy argument "[scheme://][user:pass@]host[:port]" into the proxy
   host string (scheme and any user:pass kept, for later stripping and auth),
   written NUL-terminated into name[name_size] (truncated to fit), and the port
   in *port. The port defaults by scheme: 1080 for socks5/socks5h, else 8080. */
void hts_parse_proxy(const char *arg, char *name, size_t name_size, int *port);
void code64(unsigned char *a, int size_a, unsigned char *b, int crlf);

char *convtolower(char *catbuff, size_t catbuffsize, const char *a);
void hts_lowcase(char *s);
int multipleStringMatch(const char *s, const char *match);

/* Writes a header block to fp, each line after prefix, with the value of a
   field that carries a login replaced by the fprint_redacted() marker, and
   each secret opt knows as ***. opt may be NULL. */
void fprintfio(FILE *fp, const char *buff, const char *prefix, httrackp *opt);
/* The first line of buff as fprintfio() shows it, written into dst (size
   bytes, clipped): for one write, which keeps the line whole on a merged
   stdout and stderr. Returns dst. */
const char *hts_header_shown(httrackp *opt, const char *line, char *dst,
                             size_t size);

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

/* Adds the len bytes at s to the set of opt. Thread-safe. Once the set is
   full, or for a secret over HTS_SECRET_MAXLEN bytes, logs one warning and
   adds nothing. */
void hts_secret_add(httrackp *opt, const char *s, size_t len,
                    hts_secret_kind kind);

/* Adds the password of the login of url, in any scheme or none, raw and
   unescaped. A user alone is not added. url may be NULL. */
void hts_secret_add_login(httrackp *opt, const char *url);

/* Adds the values of the Authorization, Proxy-Authorization and Cookie lines
   of a -%X header block, which may be NULL. */
void hts_secret_add_headers(httrackp *opt, const char *headers);

/* Writes the len bytes at s to fp, with each secret of opt as ***. */
void hts_secret_fwrite(httrackp *opt, FILE *fp, const char *s, size_t len);

/* fprintf() to fp, with each secret of opt as ***. */
void hts_secret_fprintf(httrackp *opt, FILE *fp, const char *format, ...)
    HTS_PRINTF_FUN(3, 4);

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
   itself when it has no login, else a copy clipped to size bytes in buf. */
const char *adr_nologin_r(char *buf, size_t size, const char *adr);

/* Same as adr_nologin_r(), but only a URL with a scheme or "//" has a login.
   Any "scheme://" counts, not only the ones the engine fetches. */
const char *url_nologin_r(char *buf, size_t size, const char *url);

/* adr_nologin_r() and url_nologin_r() into a rotating buffer of opt, which
   they take only when there is a login. A NULL opt drops the login. */
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

/* A send() carrying whatever protection this platform actually applies. The
   flag alone is not enough: GNU/Hurd defines MSG_NOSIGNAL and still kills the
   process, which is how a library takes down whoever embedded it (#1689). */
static HTS_INLINE HTS_UNUSED int hts_send_nosignal(T_SOC soc, const char *s,
                                                   int len) {
  sigpipe_mask m;
  int n, err;

  sigpipe_hold(&m);
  n = (int) send(soc, s, len, HTS_MSG_NOSIGNAL);
  /* Saved across the release, whose sigtimedwait() reports EAGAIN whenever
     nothing was pending. Callers read errno on the next line to tell a would
     block from a dead socket, and EAGAIN is the answer that means keep going.
   */
  err = errno;
  sigpipe_release(&m);
  errno = err;
  return n;
}

void cut_path(char *fullpath, char *path, size_t path_size, char *pname,
              size_t pname_size);
int fexist(const char *s);
int fexist_utf8(const char *s);

/* File size in bytes, -1 if absent or not a regular file. LLint, not off_t:
   the latter is a 32-bit long on MSVC, truncating any file of 2GB or more. */
LLint fpsize(FILE *fp);
LLint fsize(const char *s);
LLint fsize_utf8(const char *s);

// Threads
typedef void *(*beginthread_type) (void *);

/*unsigned long _beginthread( beginthread_type start_address, unsigned stack_size, void *arglist );*/

/* variables globales */
extern int _DEBUG_HEAD;
extern FILE *ioinfo;

/* constantes */
extern const char *hts_mime_keep[];
extern const char *hts_mime[][2];
extern const char *hts_main_mime[];
extern const char *hts_detect[];
extern const char *hts_detectbeg[];
extern const char *hts_nodetect[];
extern const char *hts_detectURL[];
extern const char *hts_detectandleave[];
extern const char *hts_detect_js[];

// htsmodule.c definitions
extern void *openFunctionLib(const char *file_);
extern void *getFunctionPtr(void *handle, const char *fncname);
extern void closeFunctionLib(void *handle);

extern void clearCallbacks(htscallbacks * chain);
int hts_set_callback(t_hts_htmlcheck_callbacks * callbacks,
                     const char *name, void *function);
void *hts_get_callback(t_hts_htmlcheck_callbacks * callbacks,
                       const char *name);

#define CBSTRUCT(OPT) ((t_hts_htmlcheck_callbacks*) ((OPT)->callbacks_fun))
#define GET_USERCALLBACK(OPT, NAME) ( CBSTRUCT(OPT)-> NAME .fun )
#define GET_USERARG(OPT, NAME) ( CBSTRUCT(OPT)-> NAME .carg )
/* True when a front end registered its own NAME callback */
#define HAS_CALLBACK(OPT, NAME)                                                \
  (CBSTRUCT(OPT) != NULL && CBSTRUCT(OPT)->NAME.fun != NULL)
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

/* UTF-8 aware FILE API */
#ifndef HTS_DEF_FILEAPI
#ifdef _WIN32
#define FOPEN hts_fopen_utf8
     HTSEXT_API FILE *hts_fopen_utf8(const char *path, const char *mode);

#define STAT hts_stat_utf8
     /* _stat64: _stat's st_size is a 32-bit long, even in x64 builds */
     typedef struct _stat64 STRUCT_STAT;
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
/* The underlying filesystem charset is supposed to be UTF-8 */
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
#define  is_realspace(c)  ( ((c)==' ')                || ((c)==10) || ((c)==13) || ((c)==9) || ((c)==12) || ((c)==11)                )
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

HTS_STATIC int strcmpnocase(const char *a, const char *b) {
  while(*a) {
    int cmp = hichar(*a) - hichar(*b);

    if (cmp != 0)
      return cmp;
    a++;
    b++;
  }
  return 0;
}

#ifdef _WIN32
#define strcasecmp(a,b) stricmp(a,b)
#define strncasecmp(a,b,n) strnicmp(a,b,n)
/* No snprintf alias: _snprintf leaves a truncated string unterminated. */
#endif

/* MSVC ships these POSIX functions under other names. Kept out of the installed
   headers: they would rewrite the same identifiers in a consumer's own code. */
#ifdef _MSC_VER
#define fseeko _fseeki64
#define ftello _ftelli64
#endif

#define strfield2(f,s) ( (strlen(f)!=strlen(s)) ? 0 : (strfield(f,s)) )

// is this MIME an hypertext MIME (text/html), html/js-style or other script/text type?
#define HTS_HYPERTEXT_DEFAULT_MIME "text/html"
/* Sentinel stored when the server declared no Content-Type. It is html-ish
   for every type test (so a typeless response still parses/stores as today),
   but the naming code (wire_patches_ext) treats it as "no declared type" and
   keeps the URL extension. It rides the cache, so updates name consistently. */
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
   || (strfield2((a), "application/x-authorware-map") != 0))
#define may_be_hypertext_mime__(a) \
   (\
     (strfield2((a),"audio/x-pn-realaudio")!=0) \
     || (strfield2((a),"audio/x-mpegurl")!=0) \
     /*|| (strfield2((a),"text/xml")!=0) || (strfield2((a),"application/xml")!=0) : TODO: content check */ \
  )

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

#endif
