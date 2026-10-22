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
/* File: httrack.c subroutines:                                 */
/*       basic authentication: password storage                 */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/** @file htsbauth.h
    HTTP Basic authentication storage: a per-session list of (URL-prefix,
    credentials) pairs, plus the cookie jar that holds it. Nothing here writes a
    credential to a file or to a log. */

#ifndef HTSBAUTH_DEFH
#define HTSBAUTH_DEFH

#include <sys/types.h>

#include "htsglobal.h" /* hts_boolean */

/** One stored credential. bauth_check() takes the first stored prefix that
    starts the request key, not the longest, so insertion order decides. */
#ifndef HTS_DEF_FWSTRUCT_bauth_chain
#define HTS_DEF_FWSTRUCT_bauth_chain
typedef struct bauth_chain bauth_chain;
#endif
struct bauth_chain {
  char prefix[1024]; /**< host and path, cut after the last '/', for example
                          www.foo.com/secure/ */
  char auth[1024];   /**< base-64 "user:pass", the value sent after
                          "Authorization: Basic " */
  struct bauth_chain *next; /**< next entry, NULL at the end */
};

/** Per-session cookie jar, which also carries the basic-auth list head. */
#ifndef HTS_DEF_FWSTRUCT_t_cookie
#define HTS_DEF_FWSTRUCT_t_cookie
typedef struct t_cookie t_cookie;
#endif
struct t_cookie {
  size_t max_len;   /**< bytes of data[] the jar may fill, set by the owner and
                         not always sizeof(data) */
  char data[32768]; /**< Netscape-format records, one tab-separated line each */
  bauth_chain auth; /**< head of the basic-auth list, embedded so bauth_free()
                         keeps it */
};

/* Library internal definitions */
#ifdef HTS_INTERNAL_BYTECODE

#include <time.h>

#ifndef HTS_DEF_FWSTRUCT_httrackp
#define HTS_DEF_FWSTRUCT_httrackp
typedef struct httrackp httrackp;
#endif

/* cookies */

/** Copy ADR's host into DST, lowercased and without identification, IPv6
    brackets or port. RFC 6265 scopes a cookie to a host, matches that host
    without case, and a browser jar records no port. A bracketed literal's
    zone id keeps the bare '%' that a jar carries, not the URI's "%25".
    HTS_FALSE means the caller sends no cookie. */
hts_boolean cookie_host(const char *adr, char *dst, size_t dst_size);

/** A jar record holds this many bytes of name plus value. */
#define COOKIE_MAX_PAIR 4096
/** A cookie domain buffer holds this many bytes, the NUL included. */
#define COOKIE_MAX_DOMAIN 256
/** A cookie path buffer holds this many bytes, the NUL included. */
#define COOKIE_MAX_PATH 256
/** A cookies.txt line must be shorter than this many bytes. */
#define COOKIE_MAX_LINE 8000
/** The max_len httpmirror gives the jar, below sizeof(t_cookie.data). */
#define COOKIE_MAX_JAR 30000
/** cookie_set keeps this many bytes of the jar free beyond a record's parts. */
#define COOKIE_HEADROOM 256

/** A jar record keeps these beside name, value, domain and path. */
typedef struct cookie_attr {
  hts_boolean host_only; /**< Is it sent to its domain alone? */
  hts_boolean secure;    /**< Is it sent over https only? */
  hts_boolean http_only; /**< Is it HttpOnly? It is kept for the save. */
  int64_t expires;       /**< absolute unix time, 0 for a session cookie */
} cookie_attr;

/** Store cook_name=cook_value for domain/path, with domain normalised by
    cookie_host, replacing the record of the same name, domain and path.
    attr NULL means a session cookie that subdomains also get, except for a
    public suffix, which stays host-only. !=0 if the jar refused it, an empty
    domain included. */
int cookie_set(t_cookie *cookie, const char *cook_name, const char *cook_value,
               const char *domain, const char *path, const cookie_attr *attr);

/** This is cookie_set with attr NULL. */
int cookie_add(t_cookie *cookie, const char *cook_name, const char *cook_value,
               const char *domain, const char *path);

/** Erase the record of cook_name with exactly this domain and path, with
    domain normalised by cookie_host as cookie_set does. Always 0: a domain
    nothing can be stored under holds nothing to erase. */
int cookie_remove(t_cookie *cookie, const char *cook_name, const char *domain,
                  const char *path);

/** Is the record at rec sendable at NOW, over https when https is HTS_TRUE?
    An expired record never is. A Secure one needs https. */
hts_boolean cookie_sendable(const char *rec, hts_boolean https, int64_t now);

/** Load the Netscape jar <path>/<name> into cookie (plus the copied IE jars in
    <path> on Windows). Each line a rewrite would lose is skipped, counted and
    reported through opt, which may be NULL: a malformed or unreadable line, a
    field that does not fit, a cookie the jar refuses, a repeated cookie, and
    a comment no known tool writes. Returns the count, or -1 if the file does
    not exist and has no <name>.bak. Returns COOKIE_LOAD_KEEP after a warning
    when the crawl must not rewrite the file: it cannot be opened, it is a
    dangling symbolic link, it is missing while its .bak exists, it is empty
    while its .bak is not, or memory ran out. A symbolic link is followed, so
    the .bak is the one beside its target. Blank lines, known header comments
    and expired cookies are not counted. An empty expiry field is a session
    cookie, and only the Netscape format is read. */
int cookie_load(httrackp *opt, t_cookie *cookie, const char *path,
                const char *name);

/** This is cookie_load, but when start_url is not NULL, a file whose first
    non-blank line is a document.cookie string ("a=1; b=2", no TAB, no blank
    in the first name, not a comment, with an optional UTF-8 BOM, outer quotes
    and "Cookie:") is read as session cookies for path "/" of the registrable
    domain of start_url. An address or a public suffix gets them for that host
    alone. They are Secure iff start_url is https. Such a file, or a start_url
    with no host, returns COOKIE_LOAD_KEEP, because a save would replace it. */
int cookie_load_from(httrackp *opt, t_cookie *cookie, const char *path,
                     const char *name, const char *start_url);
#define COOKIE_LOAD_KEEP (-2)

/** Write the jar's cookies to name in Netscape format, owner-only on Unix.
    The new jar goes to a temporary file first, which then replaces name in one
    step, so name holds the old or the new jar whatever fails. A symbolic link
    is followed and stays a link, and a dangling one is refused. An empty jar
    writes a header alone, or nothing when name does not exist. Returns 0 when
    the file was written or needed no write, and -1 after a warning logged
    through opt, which may be NULL. */
int cookie_save(httrackp *opt, t_cookie *cookie, const char *name);

/** Save the jar as cookie_save does, but first keep the old content as a
    .bak file, beside the target of a symbolic link. A missing file beside a
    .bak, or an empty one beside a non-empty .bak, is not rewritten. */
void cookie_save_replacing(httrackp *opt, t_cookie *cookie, const char *name);

/** Insert ins in front of the string at s, whose buffer holds s_size bytes. */
void cookie_insert(char *s, size_t s_size, const char *ins);

/** Drop the first pos bytes of the string at s, whose buffer holds s_size
    bytes. */
void cookie_delete(char *s, size_t s_size, size_t pos);

/** Return tab-separated field param (0-based) of the jar record at
    cookie_base, copied into buffer, or "" when the record has no such field.
    buffer must hold 8192 bytes. */
const char *cookie_get(char *buffer, const char *cookie_base, int param);

/** Does the jar domain JAR_DOM cover the host HOST? RFC 6265 5.1.3 says it
    does when the two name the same host, or when JAR_DOM ends a whole label of
    HOST and HOST is not an address literal. A leading dot on JAR_DOM, the
    Netscape jar form, names the same domain. Case folds on both sides, but
    nothing is normalised here, so pass HOST through cookie_host first. */
hts_boolean cookie_domain_match(const char *jar_dom, const char *host);

/** May a Set-Cookie from HOST, normalised by cookie_host, carry the Domain
    attribute DOMAIN? RFC 6265 5.3 says yes when DOMAIN is HOST itself, or a
    parent of it that is not a public suffix. A leading '.' on DOMAIN is
    accepted. When no suffix list is loaded, every parent domain is allowed. */
hts_boolean cookie_domain_allowed(const char *domain, const char *host);

/** Read a Set-Cookie Max-Age VALUE received at NOW into *when, the absolute
    expiry, capped 400 days ahead and never below 1 (RFC 6265 5.2.2).
    HTS_FALSE when VALUE is not a whole number, so the attribute is ignored. */
hts_boolean cookie_max_age_time(const char *value, int64_t now, int64_t *when);

/** Read a Set-Cookie Expires VALUE received at NOW into *when, bounded as
    cookie_max_age_time is (RFC 6265 5.2.1). HTS_FALSE when VALUE is not a
    date, so the attribute is ignored. */
hts_boolean cookie_expires_time(const char *value, int64_t now, int64_t *when);

/** Return the first jar record at or after s whose name, domain and path
    match, or NULL. An empty cook_name matches any name. The domain match folds
    case. It reaches a subdomain only when the record is not host-only. The
    record's own path must be a byte-exact prefix of path. The result points
    into s. */
char *cookie_find(char *s, const char *cook_name, const char *domain,
                  const char *path);

/** Start of the jar record after the one at a, or the trailing NUL when a
    holds the last one. */
char *cookie_nextfield(char *a);

/* basic auth */

/** Register credentials (auth = base-64 user:pass) for the prefix derived from
    adr (host) and fil (path). No-op returning 0 if cookie is NULL, allocation
    fails, a matching prefix is already stored, bauth_prefix builds no key, or
    either string is too long for bauth_chain; returns 1 on insertion. */
int bauth_add(t_cookie *cookie, const char *adr, const char *fil,
              const char *auth);

/** Return the base-64 credentials of the first stored prefix that starts
    adr+fil, or NULL when none does or cookie is NULL. The result points into
    the jar, so never free it, and bauth_free() invalidates it. */
char *bauth_check(t_cookie *cookie, const char *adr, const char *fil);

/** Drop every stored credential, leaving the jar's embedded head empty. Safe on
    a NULL jar and on a jar already freed; the jar itself is not released. */
void bauth_free(t_cookie *cookie);

/** Build the auth lookup key (host + path, query string stripped, truncated at
    the last '/') from adr and fil into prefix; returns prefix, or NULL when
    adr+fil does not fit, since a clipped key would match URLs nobody
    authenticated. Caller must supply a buffer of HTS_URLMAXSIZE * 2 bytes. */
char *bauth_prefix(char *buffer, const char *adr, const char *fil);

#endif

#endif
