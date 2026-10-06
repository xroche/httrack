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

/* This holds one "key[=value]" field of an '&'-separated query. The spans
   point into the query and are not NUL-terminated. */
typedef struct hts_query_field {
  const char *key;
  size_t keylen;
  const char *val; /* NULL when the field has no '=' */
  size_t vallen;   /* 0 when val is NULL */
  size_t len;      /* the whole field: key, '=' and value */
} hts_query_field;

/* Reads the next '&'-separated field at *cur, empty fields included. The key
   ends at the first '='. *cur then moves to the next field, and becomes NULL
   after the last one. Returns HTS_FALSE, leaving *field alone, once *cur is
   NULL. */
extern hts_boolean hts_query_next(const char **cur, hts_query_field *field);

/* Finds the first field of query with an '=' whose key is exactly name. */
extern hts_boolean hts_query_find(const char *query, const char *name,
                                  hts_query_field *field);

#endif
