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
/* File: bounded decimal, TCP port and separated-field parsers, */
/*       shared by the engine, htsserver and proxytrack          */
/* Author: Xavier Roche                                          */
/* ------------------------------------------------------------ */

#ifndef HTSURLPORT_DEFH
#define HTSURLPORT_DEFH

#include "htsglobal.h"

/* Read the unsigned decimal digits at s into *out if the value is in [min, max]
   and return TRUE, or return FALSE and leave *out alone. *end (if not NULL) is
   set past the digits, and overflow is refused before it can happen. */
hts_boolean hts_parse_llint(const char *s, const char **end, LLint min,
                            LLint max, LLint *out);

/* What hts_scan_llint() found. */
typedef enum {
  HTS_SCAN_REFUSED = -1, /* The number is signed or outside [min, max]. */
  HTS_SCAN_NONE = 0,     /* There is no number. */
  HTS_SCAN_OK = 1        /* The number is in *out. */
} hts_scan_result;

/* Skip white space, then hts_parse_llint() the number there. On HTS_SCAN_OK
   and HTS_SCAN_REFUSED, *s is moved past the number, so a caller can go on
   matching. On HTS_SCAN_NONE, *s is unchanged. */
hts_scan_result hts_scan_llint(const char **s, LLint min, LLint max,
                               LLint *out);

/* Parse the port text "a" (after the ':', up to the end of the string): TRUE
   and *port set for a bare decimal in 1..65535, else FALSE and *port left
   alone. Not sscanf("%d"), which range-checks nothing and wraps past INT_MAX.
   Its own file so proxytrack, which does not link the library, can share it. */
hts_boolean hts_parse_url_port(const char *a, int *port);

/* P points at LEN read-only bytes, which may hold NULs and lack an end NUL. */
typedef struct {
  const char *p;
  size_t len;
} hts_span;

/* Return the span over the C string S, or an empty span when S is NULL. */
hts_span hts_span_of(const char *s);

/* Read the next field of [*cur, end) up to SEP into *out, and move *cur past
   SEP. Return FALSE once *cur reaches END, so "a," yields only "a". After a
   TRUE return, [out->p, *cur) is the field plus the SEP it ended on, if any. */
hts_boolean hts_span_next(const char **cur, const char *end, char sep,
                          hts_span *out);

/* Return S without the leading bytes found in LEFT and the trailing ones found
   in RIGHT. A set may be empty, and it never matches a NUL. */
hts_span hts_span_trim(hts_span s, const char *left, const char *right);

/* Split S at its first C into *HEAD and *TAIL, C in neither. Return HTS_FALSE
   and leave both unchanged when S holds no C. */
hts_boolean hts_span_split(hts_span s, char c, hts_span *head, hts_span *tail);

/* Copy S into DST as a terminated string, clipped to SIZE - 1 bytes. Return
   FALSE if it was clipped, or if SIZE is 0, which writes nothing. */
hts_boolean hts_span_copy(hts_span s, char *dst, size_t size);

/* Find parameter NAME (exact, any case) in a header VALUE whose fields are
   split by any byte of SEPS, as in "type; a=1; NAME=\"v\"". The first NAME
   wins. A quoted value unescapes only \" and \\, and a missing end quote
   runs to the end. Return TRUE with the value in OUT, or FALSE when NAME is
   absent or its value does not fit SIZE - 1 bytes. FALSE leaves OUT empty,
   unless SIZE is 0, which writes nothing. */
hts_boolean hts_header_param(const char *value, const char *seps,
                             const char *name, char *out, size_t size);

#endif
