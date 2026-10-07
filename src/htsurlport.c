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

#include "htsurlport.h"

#include <ctype.h>
#include <string.h>

static hts_boolean is_decimal_digit(char c) { return c >= '0' && c <= '9'; }

hts_boolean hts_parse_llint(const char *s, const char **end, LLint min,
                            LLint max, LLint *out) {
  const char *p;
  LLint value = 0;
  hts_boolean fits = HTS_TRUE;

  for (p = s; is_decimal_digit(*p); p++) {
    const int digit = *p - '0';

    // Checked before the multiply, so value * 10 + digit never passes max.
    if (!fits || digit > max || value > (max - digit) / 10)
      fits = HTS_FALSE;
    else
      value = value * 10 + digit;
  }
  if (end != NULL)
    *end = p;
  if (p == s || !fits || value < min)
    return HTS_FALSE;
  *out = value;
  return HTS_TRUE;
}

hts_scan_result hts_scan_llint(const char **s, LLint min, LLint max,
                               LLint *out) {
  const char *p = *s;
  const char *end;
  LLint ignored;

  while (isspace((unsigned char) *p))
    p++;
  if (hts_parse_llint(p, &end, min, max, out)) {
    *s = end;
    return HTS_SCAN_OK;
  }
  if ((*p == '-' || *p == '+') && is_decimal_digit(p[1]))
    (void) hts_parse_llint(p + 1, &end, min, max,
                           &ignored); // only to skip the refused digits
  if (end == p)
    return HTS_SCAN_NONE;
  *s = end;
  return HTS_SCAN_REFUSED;
}

hts_boolean hts_parse_url_port(const char *a, int *port) {
  const char *end;
  LLint p;

  if (!hts_parse_llint(a, &end, 1, 65535, &p) || *end != '\0')
    return HTS_FALSE;
  *port = (int) p;
  return HTS_TRUE;
}

hts_span hts_span_of(const char *s) {
  hts_span r;

  r.p = s != NULL ? s : "";
  r.len = strlen(r.p);
  return r;
}

hts_boolean hts_span_next(const char **cur, const char *end, char sep,
                          hts_span *out) {
  const char *const start = *cur;
  const char *stop;

  if (start >= end)
    return HTS_FALSE;
  stop = memchr(start, sep, (size_t) (end - start));
  out->p = start;
  out->len = (size_t) ((stop != NULL ? stop : end) - start);
  *cur = stop != NULL ? stop + 1 : end;
  return HTS_TRUE;
}

hts_boolean hts_span_field(hts_span s, char sep, size_t n, hts_span *out) {
  const char *const end = s.p + s.len;
  const char *p = s.p;
  const char *stop;

  for (; n > 0; n--) {
    const char *const at = memchr(p, sep, (size_t) (end - p));

    if (at == NULL)
      return HTS_FALSE;
    p = at + 1;
  }
  stop = memchr(p, sep, (size_t) (end - p));
  out->p = p;
  out->len = (size_t) ((stop != NULL ? stop : end) - p);
  return HTS_TRUE;
}

static hts_boolean span_in_set(const char *set, char c) {
  return c != '\0' && strchr(set, c) != NULL;
}

hts_span hts_span_trim(hts_span s, const char *left, const char *right) {
  while (s.len > 0 && span_in_set(left, s.p[0])) {
    s.p++;
    s.len--;
  }
  while (s.len > 0 && span_in_set(right, s.p[s.len - 1]))
    s.len--;
  return s;
}

hts_boolean hts_span_split(hts_span s, char c, hts_span *head, hts_span *tail) {
  const char *const at = s.len != 0 ? memchr(s.p, c, s.len) : NULL;

  if (at == NULL)
    return HTS_FALSE;
  head->p = s.p;
  head->len = (size_t) (at - s.p);
  tail->p = at + 1;
  tail->len = s.len - head->len - 1;
  return HTS_TRUE;
}

hts_boolean hts_span_copy(hts_span s, char *dst, size_t size) {
  size_t n;

  if (size == 0)
    return HTS_FALSE;
  n = s.len < size ? s.len : size - 1;
  memcpy(dst, s.p, n);
  dst[n] = '\0';
  return n == s.len;
}
