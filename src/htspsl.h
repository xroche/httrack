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
/* File: htspsl.h subroutines:                                  */
/*       Public Suffix List lookups                             */
/* ------------------------------------------------------------ */

#ifndef HTSPSL_DEFH
#define HTSPSL_DEFH

#ifdef HTS_INTERNAL_BYTECODE

#include <stddef.h>

#include "htsglobal.h"

/* The list compiled in: NUL-separated rules in chunks of hts_psl_builtin_size
   bytes, a NULL after the last chunk, and the upstream VERSION stamp. */
extern const char *const hts_psl_builtin[];
extern const size_t hts_psl_builtin_size[];
extern const char hts_psl_builtin_version[];

/* Load the list file configure named with --with-psl-file, else the built-in
   list. Call it from hts_init(), before any thread looks a host up. */
void hts_psl_init(void);
void hts_psl_uninit(void);

/* Load the list file at path, or the built-in list when path is NULL or the
   file holds fewer rules than a real list. */
void hts_psl_init_from(const char *path);

/* Replace the list with the rules in the file at path. Return HTS_FALSE, and
   keep the old list, when the file cannot be read or holds fewer than
   min_rules rules. */
hts_boolean hts_psl_load_file(const char *path, size_t min_rules);

/* Is a list loaded? */
hts_boolean hts_psl_loaded(void);

/* Return the VERSION stamp of the list in use, or "" when it has none. */
const char *hts_psl_version(void);

/* Return where the registrable domain starts in host, which is lowercase with
   no trailing dot. Return NULL when host is itself a public suffix, has an
   empty label (a trailing dot counts), or no list is loaded. */
const char *hts_psl_registrable_domain(const char *host);

#endif

#endif
