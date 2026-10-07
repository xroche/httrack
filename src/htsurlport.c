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

// Not strncasecmp, which MSVC lacks and this file has no shim for.
static hts_boolean param_name_is(const char *key, size_t len,
                                 const char *name) {
  size_t i;

  for (i = 0; i < len; i++)
    if (tolower((unsigned char) key[i]) != tolower((unsigned char) name[i]))
      return HTS_FALSE;
  return name[len] == '\0';
}

// Only these two are escapes, so a raw Windows path keeps its backslashes.
static hts_boolean param_escape(const char *p) {
  return p[0] == '\\' && (p[1] == '"' || p[1] == '\\');
}

// Skip to the next separator outside a quoted string.
static const char *param_skip(const char *p, const char *seps) {
  hts_boolean quoted = HTS_FALSE;

  for (; *p != '\0' && (quoted || !span_in_set(seps, *p)); p++) {
    if (*p == '"')
      quoted = !quoted;
    else if (quoted && param_escape(p))
      p++;
  }
  return p;
}

hts_boolean hts_header_param(const char *value, const char *seps,
                             const char *name, char *out, size_t size) {
  const char *p = value;

  if (size == 0)
    return HTS_FALSE;
  out[0] = '\0';
  while (*p != '\0') {
    const char *key, *v;
    size_t key_len, n = 0;
    hts_boolean match;

    while (span_in_set(" \t", *p) || span_in_set(seps, *p))
      p++;
    for (key = p; *p != '\0' && *p != '=' && !span_in_set(seps, *p); p++)
      ;
    for (key_len = (size_t) (p - key);
         key_len != 0 && span_in_set(" \t", key[key_len - 1]); key_len--)
      ;
    if (*p != '=')
      continue; // a field with no value, such as the media type
    match = param_name_is(key, key_len, name);
    for (p++; span_in_set(" \t", *p); p++)
      ;
    if (*p == '"') {
      for (p++; *p != '\0' && *p != '"'; p++) {
        if (param_escape(p))
          p++;
        if (match) {
          if (n >= size - 1) {
            out[0] = '\0';
            return HTS_FALSE;
          }
          out[n++] = *p;
        }
      }
      if (match) {
        out[n] = '\0';
        return HTS_TRUE;
      }
      if (*p == '"')
        p = param_skip(p + 1, seps); // junk after the closing quote
    } else {
      for (v = p; *p != '\0' && !span_in_set(seps, *p); p++)
        ;
      for (n = (size_t) (p - v); n != 0 && span_in_set(" \t", v[n - 1]); n--)
        ;
      if (match) {
        if (n > size - 1)
          return HTS_FALSE;
        memcpy(out, v, n);
        out[n] = '\0';
        return HTS_TRUE;
      }
    }
  }
  return HTS_FALSE;
}
