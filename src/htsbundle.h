/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 2026 Xavier Roche and other contributors

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
/* File: htsbundle.h subroutines:                               */
/*       login bundles named by a start URL                     */
/* ------------------------------------------------------------ */

#ifndef HTSBUNDLE_DEFH
#define HTSBUNDLE_DEFH

#ifdef HTS_INTERNAL_BYTECODE

#include <stddef.h>
#include <stdio.h>

#include "htsglobal.h"
#include "htsopt.h"
#include "htsbauth.h"

/* A start URL ending with this names the bundle file <name>.hts. */
#define HTS_BUNDLE_SUFFIX "#httrack-bundle="

/* A bundle name holds fewer bytes than this. */
#define HTS_BUNDLE_NAME_MAX 128

/* Cut the bundle suffix off url in place and record that url names that
   bundle. A name given twice is one bundle. NULL, or why the run must stop. */
const char *hts_bundle_take(httrackp *opt, char *url);

/* Read every named bundle from opt->bundle_dir, or the project folder, and
   refuse two bundles that overlap with different identities or cookie values.
   The cookies go into opt->cookie. A jar_file newer than the bundle keeps its
   records. A NULL one never does. HTS_FALSE after writing why into err, naming
   the file, and then the run must stop. A second call reads the files again. */
hts_boolean hts_bundle_load_all(httrackp *opt, const char *jar_file, char *err,
                                size_t err_size);

/* The i-th url line of the loaded bundles, or NULL past the last one. */
const char *hts_bundle_url(httrackp *opt, size_t i);

/* Give req the UA and the headers of the first bundle whose scope holds adr's
   host, when that bundle has them. Its headers go over https alone when any URL
   of the bundles merged with it is https. */
void hts_bundle_identity(httrackp *opt, const char *adr, htsrequest *req);

/* Forget the bundles named for opt. */
void hts_bundle_free(httrackp *opt);

/* Do the len bytes at s hold a control byte other than TAB? */
hts_boolean hts_bundle_has_control(const char *s, size_t len);

/* The bundles share these htsbauth.c internals. */

/* Copy the host of url, with or without a scheme, into dst as cookie_host
   does. */
hts_boolean cookie_url_host(const char *url, char *dst, size_t dst_size);

/* The record of s named cook_name for exactly domain and path, or NULL. */
char *cookie_find_exact(char *s, const char *cook_name, const char *domain,
                        const char *path);

/* Copy url's host into host, set attr, and return the domain that cookies
   pasted for url bind to: its registrable domain, or the host. NULL if none. */
const char *cookie_flat_bind(const char *url, char *host, size_t host_size,
                             cookie_attr *attr);

/* Store the pair "name=value" at pair as a cookie of domain. HTS_FALSE when it
   is not stored, counted in *ignored. */
hts_boolean cookie_flat_pair(httrackp *opt, t_cookie *cookie, char *pair,
                             const char *domain, const cookie_attr *attr,
                             const char *given, int *ignored);

/* Append the jar's records to out, as a cookies.txt file writes them. */
void cookie_records_text(const t_cookie *cookie, String *out);

/* This filter picks what cookie_load_stream lets into the jar. */
typedef struct cookie_filter {
  /* Should the record go into cookie? repeat says this load had it already.
     Setting why stops the load. */
  hts_boolean (*admit)(struct cookie_filter *filter, t_cookie *cookie,
                       hts_boolean repeat, const char *name, const char *value,
                       const char *domain, const char *path,
                       const cookie_attr *attr);
  void *ctx;
  int line;        /* the number of the last line read */
  const char *why; /* NULL, or why the load stopped at line */
  hts_boolean full_skips; /* Does a record the jar has no room for only warn? */
} cookie_filter;

/* Lines read from fp, or from the len bytes at buf when fp is NULL. */
typedef struct text_src {
  FILE *fp;
  const char *buf;
  size_t len;
  size_t pos;
  hts_boolean eof; /* a read went past the end, as feof() says */
  hts_boolean error;
} text_src;

/* A source reading fp from its position. */
text_src text_src_file(FILE *fp);

/* A source reading the len bytes at buf, which must outlive it. The last line
   needs no LF. */
text_src text_src_mem(const char *buf, size_t len);

/* Read a line into s, of size bytes, without its LF, and a CR before the LF
   when the whole line fit. *len counts every byte of the line, cut or not. */
void text_src_readline(text_src *src, char *s, size_t size, size_t *len);

/* Load Netscape jar records from src's position to its end through filter,
   as cookie_load reads a file. given names the file in warnings, which name
   the line and none of its bytes. A line with a control byte other than TAB,
   or one too long, stops the load. */
void cookie_load_stream(httrackp *opt, t_cookie *cookie, text_src *src,
                        const char *given, cookie_filter *filter);

/* Write the len bytes at data to a temporary beside file, owner-only, then
   move it over file, so a symbolic link at file is replaced, never followed.
   Did it succeed? On failure file is unchanged and errno says why. */
hts_boolean cookie_write_replace(const char *file, const char *data,
                                 size_t len);

/* Why can the len bytes of host, past any ASCII lowercasing, not take an
   ASCII form? IDNA needs strict UTF-8, and it encodes a capital outside ASCII
   as another host. NULL when they can. */
const char *cookie_host_refusal(const char *host, size_t len);

#endif

#endif
