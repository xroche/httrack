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
/* File: percent-escape decoding and '&'-separated queries,     */
/*       shared by the engine and htsserver                     */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTSESCAPE_DEFH
#define HTSESCAPE_DEFH

#include "htsencoding.h"
#include "htsstrings.h"

/* Decode form-urlencoded 's' into 'tempo': "%%" is '%', '+' is space, and a
   bad escape stays literal. */
extern void hts_unescapehttp(const char *s, String *tempo);

/* As hts_unescapehttp(), minus the '+' rule, and collapsing a decoded run of
   line separators so an escaped CRLF cannot forge an .ini line break. */
extern void hts_unescapeini(const char *s, String *tempo);

/* Next '&'-separated "key[=value]" field at *cur, empty fields included.
   key and val point into the query and are not NUL-terminated.
   val is NULL, and vallen 0, when the field has no '='; the key ends at the
   first '='. *cur moves to the next field, and is NULL after the last one.
   Returns HTS_FALSE, setting nothing, once *cur is NULL. */
extern hts_boolean hts_query_next(const char **cur, const char **key,
                                  size_t *keylen, const char **val,
                                  size_t *vallen);

/* First field of query that has an '=' and whose key is exactly name. */
extern hts_boolean hts_query_find(const char *query, const char *name,
                                  const char **val, size_t *vallen);

/* Copy name's alphanumeric value into dst of capacity size. True when a
   non-empty one fit whole; dst is left empty otherwise. */
extern hts_boolean hts_query_alnum_value(char *dst, size_t size,
                                         const char *query, const char *name);

/* True when name appears at least once, and every occurrence is shorter than
   maxlen and decodes to expected. */
extern hts_boolean hts_query_all_match(const char *query, const char *name,
                                       const char *expected, size_t maxlen);

/* Split query in place: for each field with an '=', NUL-terminate its key and
   value and pass them to emit. Fields without '=' are skipped. */
typedef void (*hts_query_emit)(void *arg, char *key, char *value);
extern void hts_query_split(char *query, hts_query_emit emit, void *arg);

#endif
