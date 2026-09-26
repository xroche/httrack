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
    credentials) pairs, plus the cookie jar that holds it. The credentials stay
    in memory, because nothing here writes them to a file or to a log. */

#ifndef HTSBAUTH_DEFH
#define HTSBAUTH_DEFH

#include <sys/types.h>

#include "htsglobal.h" /* hts_boolean */

/** One stored credential. bauth_check() returns the first entry whose prefix
    starts the request's host and path, in insertion order, so a wider prefix
    stored first wins over a narrower one stored later. */
#ifndef HTS_DEF_FWSTRUCT_bauth_chain
#define HTS_DEF_FWSTRUCT_bauth_chain
typedef struct bauth_chain bauth_chain;
#endif
struct bauth_chain {
  char prefix[1024]; /**< host and path, cut after the last '/', for example
                          www.foo.com/secure/ */
  char auth[1024];   /**< base-64 "user:pass", the value sent after
                          "Authorization: Basic " */
  struct bauth_chain *next; /**< next entry, NULL at the end of the list */
};

/** Per-session cookie jar, which also carries the head of the basic-auth
    list. */
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

/** Store cook_name=cook_value for domain/path, with domain normalised by
    cookie_host. !=0 if the jar refused it, an empty domain included. */
int cookie_add(t_cookie *cookie, const char *cook_name, const char *cook_value,
               const char *domain, const char *path);

/** Erase cook_name for domain/path, with domain normalised by cookie_host as
    cookie_add does. Always 0: a domain nothing can be stored under holds
    nothing to erase. */
int cookie_del(t_cookie *cookie, const char *cook_name, const char *domain,
               const char *path);

/** Load the Netscape jar <path>/<name> into cookie (plus the copied IE jars in
    <path> on Windows). A line whose field does not fit is refused, not clipped,
    and reported through opt, which may be NULL. Returns 0 if the jar was
    opened, -1 otherwise. */
int cookie_load(httrackp *opt, t_cookie *cookie, const char *path,
                const char *name);

/** Write the jar's cookies to name in Netscape format, owner-only on Unix
    because they are live session cookies. The stored basic-auth credentials are
    not written. Returns 0 when the file was written or the jar was empty, -1 if
    the file could not be opened. */
int cookie_save(t_cookie *cookie, const char *name);

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

/** First jar record at or after s whose name (an empty cook_name accepts any),
    domain and path match, or NULL. The domain match folds case, and the
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
    adr+fil, or NULL if none matches (or cookie is NULL). The result points into
    the jar, so the caller must not free it, and bauth_free() invalidates it. */
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
