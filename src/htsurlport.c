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
#include "htsctype.h"

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

  while (hts_isspace((unsigned char) *p))
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
  if (s == NULL)
    s = "";
  return hts_span_at(s, strlen(s));
}

hts_boolean hts_span_next_in(hts_span *rest, char sep, hts_span *field) {
  const char *stop;

  if (hts_span_empty(*rest))
    return HTS_FALSE;
  stop = memchr(rest->p, sep, rest->len);
  *field = stop != NULL ? hts_span_between(rest->p, stop) : *rest;
  *rest = hts_span_skip(*rest, field->len + (stop != NULL ? 1 : 0));
  return HTS_TRUE;
}

static hts_boolean span_in_set(const char *set, char c) {
  return c != '\0' && strchr(set, c) != NULL;
}

hts_boolean hts_span_empty(hts_span s) {
  return s.len == 0 ? HTS_TRUE : HTS_FALSE;
}

hts_span hts_span_trim(hts_span s, const char *left, const char *right) {
  while (!hts_span_empty(s) && span_in_set(left, s.p[0]))
    s = hts_span_skip(s, 1);
  while (!hts_span_empty(s) && span_in_set(right, s.p[s.len - 1]))
    s.len--;
  return s;
}

hts_boolean hts_span_split(hts_span s, char c, hts_span *head, hts_span *tail) {
  const char *const at = !hts_span_empty(s) ? memchr(s.p, c, s.len) : NULL;

  if (at == NULL)
    return HTS_FALSE;
  *head = hts_span_between(s.p, at);
  *tail = hts_span_skip(s, head->len + 1);
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

hts_span hts_span_at(const char *p, size_t len) {
  hts_span r;

  r.p = p;
  r.len = len;
  return r;
}

hts_span hts_span_between(const char *from, const char *to) {
  return hts_span_at(from, (size_t) (to - from));
}

hts_span hts_span_skip(hts_span s, size_t n) {
  if (n > s.len)
    n = s.len;
  if (n != 0) {
    s.p += n;
    s.len -= n;
  }
  return s;
}

hts_boolean hts_span_present(hts_span s) {
  return s.p != NULL ? HTS_TRUE : HTS_FALSE;
}

// Compare N bytes, where a NUL is a byte like any other.
static hts_boolean span_bytes_eq(const char *a, const char *b, size_t n,
                                 hts_boolean fold) {
  size_t i;

  if (n == 0)
    return HTS_TRUE; // either may be the NULL of an absent view field
  if (!fold)
    return memcmp(a, b, n) == 0;
  for (i = 0; i < n; i++)
    if (hts_tolower((unsigned char) a[i]) != hts_tolower((unsigned char) b[i]))
      return HTS_FALSE;
  return HTS_TRUE;
}

hts_boolean hts_span_eq(hts_span a, hts_span b) {
  return a.len == b.len && span_bytes_eq(a.p, b.p, a.len, HTS_FALSE);
}

hts_boolean hts_span_eq_ci(hts_span a, hts_span b) {
  return a.len == b.len && span_bytes_eq(a.p, b.p, a.len, HTS_TRUE);
}

hts_boolean hts_span_starts_with(hts_span s, hts_span prefix) {
  return s.len >= prefix.len &&
         span_bytes_eq(s.p, prefix.p, prefix.len, HTS_FALSE);
}

hts_boolean hts_span_starts_with_ci(hts_span s, hts_span prefix) {
  return s.len >= prefix.len &&
         span_bytes_eq(s.p, prefix.p, prefix.len, HTS_TRUE);
}

hts_boolean hts_span_ends_with(hts_span s, hts_span suffix) {
  // Check the length so the subtraction cannot wrap.
  return s.len >= suffix.len &&
         hts_span_starts_with(hts_span_skip(s, s.len - suffix.len), suffix);
}

hts_boolean hts_span_ends_with_ci(hts_span s, hts_span suffix) {
  // Check the length so the subtraction cannot wrap.
  return s.len >= suffix.len &&
         hts_span_starts_with_ci(hts_span_skip(s, s.len - suffix.len), suffix);
}

static int span_index(hts_span s, const char *const *words, hts_boolean fold) {
  int i;

  for (i = 0; words[i] != NULL; i++) {
    const hts_span w = hts_span_of(words[i]);

    if (s.len == w.len && span_bytes_eq(s.p, w.p, s.len, fold))
      return i;
  }
  return -1;
}

int hts_span_index(hts_span s, const char *const *words) {
  return span_index(s, words, HTS_FALSE);
}

int hts_span_index_ci(hts_span s, const char *const *words) {
  return span_index(s, words, HTS_TRUE);
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
    match = hts_span_eq_ci(hts_span_at(key, key_len), hts_span_of(name));
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

static const struct {
  int code;
  const char *text;
} status_reasons[] = {
    {100, "Continue"},
    {101, "Switching Protocols"},
    {200, "OK"},
    {201, "Created"},
    {202, "Accepted"},
    {203, "Non-Authoritative Information"},
    {204, "No Content"},
    {205, "Reset Content"},
    {206, "Partial Content"},
    {207, "Multi-Status"},
    {300, "Multiple Choices"},
    {301, "Moved Permanently"},
    {302, "Moved Temporarily"},
    {303, "See Other"},
    {304, "Not Modified"},
    {305, "Use Proxy"},
    {306, "Undefined 306 error"},
    {307, "Temporary Redirect"},
    {400, "Bad Request"},
    {401, "Unauthorized"},
    {402, "Payment Required"},
    {403, "Forbidden"},
    {404, "Not Found"},
    {405, "Method Not Allowed"},
    {406, "Not Acceptable"},
    {407, "Proxy Authentication Required"},
    {408, "Request Time-out"},
    {409, "Conflict"},
    {410, "Gone"},
    {411, "Length Required"},
    {412, "Precondition Failed"},
    {413, "Request Entity Too Large"},
    {414, "Request-URI Too Large"},
    {415, "Unsupported Media Type"},
    {416, "Requested Range Not Satisfiable"},
    {417, "Expectation Failed"},
    {429, "Too Many Requests"},
    {451, "Unavailable For Legal Reasons"},
    {500, "Internal Server Error"},
    {501, "Not Implemented"},
    {502, "Bad Gateway"},
    {503, "Service Unavailable"},
    {504, "Gateway Time-out"},
    {505, "HTTP Version Not Supported"},
};

const char *hts_status_reason(int code) {
  size_t i;

  for (i = 0; i < sizeof(status_reasons) / sizeof(status_reasons[0]); i++) {
    if (status_reasons[i].code == code)
      return status_reasons[i].text;
  }
  return NULL;
}
