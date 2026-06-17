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
    credentials) pairs, plus the cookie jar that holds it. */

#ifndef HTSBAUTH_DEFH
#define HTSBAUTH_DEFH

#include <sys/types.h>

/** One stored credential: the longest-prefix match against a request's
    host+path selects which auth header to send. */
#ifndef HTS_DEF_FWSTRUCT_bauth_chain
#define HTS_DEF_FWSTRUCT_bauth_chain
typedef struct bauth_chain bauth_chain;
#endif
struct bauth_chain {
  char prefix[1024]; /* host + path prefix, e.g. www.foo.com/secure/ */
  char auth[1024];   /* base-64 encoded user:pass (Authorization payload) */
  struct bauth_chain *next; /* next element, NULL-terminated list */
};

/** Per-session cookie jar; also holds the basic-auth list head (auth).
    The head node (auth) is embedded, not heap-allocated. */
#ifndef HTS_DEF_FWSTRUCT_t_cookie
#define HTS_DEF_FWSTRUCT_t_cookie
typedef struct t_cookie t_cookie;
#endif
struct t_cookie {
  int max_len;      /* capacity of data[] in use */
  char data[32768]; /* raw cookie store (NUL-terminated field list) */
  bauth_chain auth; /* embedded head of the basic-auth list */
};

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE

/* cookies */
int cookie_add(t_cookie * cookie, const  char *cook_name, const  char *cook_value,
               const  char *domain, const  char *path);

int cookie_del(t_cookie * cookie, const char *cook_name, const char *domain, const char *path);

int cookie_load(t_cookie * cookie, const char *path, const char *name);

int cookie_save(t_cookie * cookie, const char *name);

void cookie_insert(char *s, size_t s_size, const char *ins);

void cookie_delete(char *s, size_t s_size, size_t pos);

const char *cookie_get(char *buffer, const char *cookie_base, int param);

char *cookie_find(char *s, const char *cook_name, const char *domain, const char *path);

char *cookie_nextfield(char *a);

/* basic auth */

/** Register credentials (auth = base-64 user:pass) for the prefix derived from
    adr (host) and fil (path). No-op returning 0 if cookie is NULL, allocation
    fails, or a matching prefix is already stored; returns 1 on insertion. */
int bauth_add(t_cookie * cookie, const char *adr, const char *fil, const char *auth);

/** Return the stored base-64 credentials whose prefix matches adr+fil, or NULL
    if none (or cookie is NULL). Returned pointer aliases the jar's bauth_chain;
    caller must not free it. */
char *bauth_check(t_cookie * cookie, const char *adr, const char *fil);

/** Build the auth lookup key (host + path, query string stripped, truncated at
    the last '/') from adr and fil into prefix; returns prefix. Caller must
    supply a buffer of HTS_URLMAXSIZE * 2 bytes. */
char *bauth_prefix(char *buffer, const char *adr, const char *fil);

#endif

#endif
