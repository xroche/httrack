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

#include "htsglobal.h"
#include "htsbauth.h"

/* A start URL ending with this names the bundle file <name>.hts. */
#define HTS_BUNDLE_SUFFIX "#httrack-bundle="

/* A bundle name holds fewer bytes than this. */
#define HTS_BUNDLE_NAME_MAX 128

typedef enum {
  HTS_BUNDLE_NONE,  /* no suffix */
  HTS_BUNDLE_NAMED, /* a valid name, cut off the URL */
  HTS_BUNDLE_BAD    /* a suffix whose name is refused, URL left alone */
} hts_bundle_suffix;

/* Is name a bundle name: [A-Za-z0-9._-], not starting with '.' and no ".."? */
hts_boolean hts_bundle_name_valid(const char *name);

/* Cut the bundle suffix off url in place and copy its name into name. */
hts_bundle_suffix hts_bundle_cut(char *url, char *name, size_t name_size);

/* Forget the bundles and the --bundle-dir recorded for opt. */
void hts_bundle_reset(httrackp *opt);

/* Read names from dir rather than the project folder. HTS_FALSE when out of
   memory. */
hts_boolean hts_bundle_set_dir(httrackp *opt, const char *dir);

/* Record that the start URL url (suffix already cut) names bundle name. A
   name given twice is one bundle. HTS_FALSE when out of memory. */
hts_boolean hts_bundle_name(httrackp *opt, const char *name, const char *url);

/* Read every named bundle, then check scopes and identities. HTS_FALSE after
   writing why into err, naming the file: the run must stop. */
hts_boolean hts_bundle_load_all(httrackp *opt, char *err, size_t err_size);

/* The bundles' url lines, each followed by a LF, in a malloct'd string the
   caller freet's. NULL when there are none. */
char *hts_bundle_urls(httrackp *opt);

/* How many cookies the loaded bundles hold. */
int hts_bundle_cookie_count(httrackp *opt);

/* Store the bundles' cookies into jar. Returns how many it refused. */
int hts_bundle_cookies(httrackp *opt, t_cookie *jar);

/* When adr's host is in a bundle's scope, point *ua and *headers at that
   bundle's, for each it gives. Returns HTS_TRUE when it changed *ua. The
   strings live until hts_bundle_reset. */
hts_boolean hts_bundle_identity(httrackp *opt, const char *adr, const char **ua,
                                const char **headers);

/* Print each loaded bundle (UA, headers, scope, cookies) for the self-tests. */
void hts_bundle_dump(httrackp *opt);

#endif

#endif
