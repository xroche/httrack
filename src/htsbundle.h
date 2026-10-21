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

/* A start URL ending with this names the bundle file <name>.hts. */
#define HTS_BUNDLE_SUFFIX "#httrack-bundle="

/* A bundle name holds fewer bytes than this. */
#define HTS_BUNDLE_NAME_MAX 128

typedef enum {
  HTS_BUNDLE_NONE,  /* no suffix */
  HTS_BUNDLE_NAMED, /* a valid name, cut off the URL */
  HTS_BUNDLE_BAD    /* a suffix whose name is refused, URL left alone */
} hts_bundle_suffix;

/* Cut the bundle suffix off url in place and copy its name into name. */
hts_bundle_suffix hts_bundle_cut(char *url, char *name, size_t name_size);

/* Record that the start URL url (suffix already cut) names bundle name. A
   name given twice is one bundle. HTS_FALSE when out of memory. */
hts_boolean hts_bundle_add(httrackp *opt, const char *name, const char *url);

/* Read every named bundle from opt->bundle_dir, or the project folder, and
   refuse two bundles that overlap with different identities or cookie values.
   The cookies go into opt->cookie, under the records already there when the
   jar file is newer than the bundle. jar is that file, or NULL. HTS_FALSE
   after writing why into err, naming the file: the run must stop. A second
   call reads the files again. */
hts_boolean hts_bundle_load_all(httrackp *opt, const char *jar, char *err,
                                size_t err_size);

/* The i-th url line of the loaded bundles, or NULL past the last one. */
const char *hts_bundle_url(httrackp *opt, size_t i);

/* Give req the UA and headers of the first bundle whose scope holds adr's
   host, where it has them. Its headers go over https alone when every URL
   naming it is https. */
void hts_bundle_identity(httrackp *opt, const char *adr, htsrequest *req);

/* Forget the bundles named for opt. */
void hts_bundle_free(httrackp *opt);

/* Print each loaded bundle (UA, url lines, scope, headers) for the
   self-tests. */
void hts_bundle_dump(httrackp *opt);

/* htsbauth.c internals the bundles share. */

/* Copy the host of url, with or without a scheme, into dst as cookie_host
   does, and set *https. */
hts_boolean cookie_url_host(const char *url, char *dst, size_t dst_size,
                            hts_boolean *https);

/* What cookie_load_stream lets into the jar, and the count of what it keeps
   out. */
typedef struct cookie_filter {
  hts_boolean (*covers)(const void *scope, const char *host);
  const void *scope;
  t_cookie *others;  /* every bundle's records so far */
  hts_boolean under; /* a record the jar holds already wins */
  int dropped;       /* records outside the scope */
  char clash[64];    /* a name others holds with another value, or "" */
  int lost;          /* records the jar could not take */
} cookie_filter;

/* Load Netscape jar records from fp's position to its end through filter,
   as cookie_load reads a file. name is the file name used in warnings. */
void cookie_load_stream(httrackp *opt, t_cookie *cookie, FILE *fp,
                        const char *name, cookie_filter *filter);

#endif

#endif
