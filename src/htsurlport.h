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
/* File: bounded decimal and TCP port parsers, byte spans, and  */
/*       header parameters, shared by the engine, htsserver and  */
/*       proxytrack                                              */
/* Author: Xavier Roche                                          */
/* ------------------------------------------------------------ */

#ifndef HTSURLPORT_DEFH
#define HTSURLPORT_DEFH

#include "htsglobal.h"

#include <string.h>

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

/* Return the span over the LEN bytes at P. */
static HTS_INLINE HTS_UNUSED hts_span hts_span_at(const char *p, size_t len) {
  hts_span r;

  r.p = p;
  r.len = len;
  return r;
}

/* Return the span over the C string S, or an empty span when S is NULL. Pass a
   literal to a compare as hts_span_of("lit"). */
static HTS_INLINE HTS_UNUSED hts_span hts_span_of(const char *s) {
  if (s == NULL)
    s = "";
  return hts_span_at(s, strlen(s));
}

/* Return the span over [FROM, TO), where TO must not precede FROM. */
hts_span hts_span_between(const char *from, const char *to);

/* Return S without its first N bytes, or the empty span at its end when N
   exceeds its length. */
static HTS_INLINE HTS_UNUSED hts_span hts_span_skip(hts_span s, size_t n) {
  if (n > s.len)
    n = s.len;
  if (n != 0) {
    s.p += n;
    s.len -= n;
  }
  return s;
}

/* Is S present? An absent hts_url_view part is not, but an empty one is. */
hts_boolean hts_span_present(hts_span s);

/* Return S without the leading bytes found in LEFT and the trailing ones found
   in RIGHT. A set may be empty, and it never matches a NUL. */
hts_span hts_span_trim(hts_span s, const char *left, const char *right);

/* Split S at its first C into *HEAD and *TAIL, C in neither. Return HTS_FALSE
   and leave both unchanged when S holds no C. */
hts_boolean hts_span_split(hts_span s, char c, hts_span *head, hts_span *tail);

/* Read the field of *REST up to SEP into *FIELD and shrink *REST past that SEP.
   Return FALSE once *REST is empty, so "" yields nothing and "a," yields only
   "a". After a TRUE return, [field->p, rest->p) is the field plus its SEP. */
HTS_CHECK_RESULT hts_boolean hts_span_next_in(hts_span *rest, char sep,
                                              hts_span *field);

/* Does S hold no bytes? Its pointer does not matter. */
hts_boolean hts_span_empty(hts_span s);

/* Do A and B hold the same bytes? */
hts_boolean hts_span_eq(hts_span a, hts_span b);

/* Do A and B hold the same bytes once A-Z are folded to a-z? */
hts_boolean hts_span_eq_ci(hts_span a, hts_span b);

/* Does S start with the bytes of PREFIX? */
hts_boolean hts_span_starts_with(hts_span s, hts_span prefix);

/* Does S start with PREFIX once A-Z are folded to a-z? */
hts_boolean hts_span_starts_with_ci(hts_span s, hts_span prefix);

/* Does S end with the bytes of SUFFIX? */
hts_boolean hts_span_ends_with(hts_span s, hts_span suffix);

/* Does S end with SUFFIX once A-Z are folded to a-z? */
hts_boolean hts_span_ends_with_ci(hts_span s, hts_span suffix);

/* The words of a list for hts_span_index, then the NULL it ends at. */
#define HTS_SPAN_WORDS(...) __VA_ARGS__, NULL

/* Return the index of S in the NULL-terminated WORDS, or -1. */
HTS_CHECK_RESULT int hts_span_index(hts_span s, const char *const *words);

/* hts_span_index with A-Z compared as a-z. */
HTS_CHECK_RESULT int hts_span_index_ci(hts_span s, const char *const *words);

/* Copy S into DST as a terminated string, clipped to SIZE - 1 bytes. Return
   FALSE if it was clipped, or if SIZE is 0, which writes nothing. */
hts_boolean hts_span_copy(hts_span s, char *dst, size_t size);

/* Does the SEP-separated LIST hold TOKEN, A-Z compared as a-z? Each item is
   trimmed of " \t\n\v\f\r" first, and an empty item never matches. */
HTS_CHECK_RESULT hts_boolean hts_span_list_has(hts_span list, char sep,
                                               hts_span token);

/* Read the next SEP-separated field of *REST as NAME=VALUE, split at its first
   '=' and not trimmed. VALUE is absent (see hts_span_present) when the field
   has no '='. Return FALSE once *REST is empty, as hts_span_next_in does. */
HTS_CHECK_RESULT hts_boolean hts_param_next(hts_span *rest, char sep,
                                            hts_span *name, hts_span *value);

/* One line of a header block. NAME is empty on the status line, a fold, or a
   line with no ':'. */
typedef struct {
  hts_span line;  /* without its LF and one CR */
  hts_span name;  /* without the blanks before the colon */
  hts_span value; /* trimmed of SP and HT */
} hts_header_field;

/* Split LINE at its first ':' into *NAME and *VALUE as hts_header_field has
   them. Both are empty for a fold (LINE opens with SP or HT) or a line with no
   ':'. */
void hts_header_split(hts_span line, hts_span *name, hts_span *value);

/* Read the next line of the header block *REST, ended by LF or CRLF, into *F.
   Return FALSE at the end of *REST or at the blank line ending the block. */
HTS_CHECK_RESULT hts_boolean hts_header_next(hts_span *rest,
                                             hts_header_field *f);

/* A malloct'ed copy of S as a C string, or NULL when out of memory. */
HTS_CHECK_RESULT char *hts_span_dup(hts_span s);

/* Is S one or more decimal digits? */
HTS_CHECK_RESULT hts_boolean hts_span_is_digits(hts_span s);

/* Is all of S decimal digits, with a value in [MIN, MAX]? On TRUE the value is
   in *OUT. An empty span, signs, blanks and overflow are refused, and *OUT is
   then untouched. */
HTS_CHECK_RESULT hts_boolean hts_span_parse_llint(hts_span s, LLint min,
                                                  LLint max, LLint *out);

/* Return a pointer to the '.' that starts the extension of PATH's last
   segment, or NULL. A byte of SEPS ends a segment, '?' is a plain byte, and a
   '.' at PATH[0] counts. */
HTS_CHECK_RESULT const char *hts_path_ext_dot(const char *path,
                                              const char *seps);

/* Return a span of the text after FIL's last '.' before the first '?', or an
   empty span. The search crosses directories, so "/v1.2/page" gives "2/page".
 */
hts_span hts_fil_ext(const char *fil);

/* Find parameter NAME (exact, any case) in a header VALUE whose fields are
   split by any byte of SEPS, as in "type; a=1; NAME=\"v\"". The first NAME
   wins. A quoted value unescapes only \" and \\, and a missing end quote
   runs to the end. Return TRUE with the value in OUT, or FALSE when NAME is
   absent or its value does not fit SIZE - 1 bytes. FALSE leaves OUT empty,
   unless SIZE is 0, which writes nothing. */
hts_boolean hts_header_param(const char *value, const char *seps,
                             const char *name, char *out, size_t size);

/* Return the static reason phrase of HTTP status CODE, or NULL if unknown.
   It sits here for proxytrack, and HTSEXT_API would break its MSVC build. */
const char *hts_status_reason(int code);

#endif
