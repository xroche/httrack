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
/* File: bounded decimal and TCP port parsers, shared by the    */
/*       engine, htsserver and proxytrack                        */
/* Author: Xavier Roche                                          */
/* ------------------------------------------------------------ */

#include "htsurlport.h"

#include <ctype.h>

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

int hts_scan_llint(const char **s, LLint min, LLint max, LLint *out) {
  const char *p = *s;
  const char *end;
  LLint ignored;

  while (isspace((unsigned char) *p))
    p++;
  if (hts_parse_llint(p, &end, min, max, out)) {
    *s = end;
    return 1;
  }
  if ((*p == '-' || *p == '+') && is_decimal_digit(p[1]))
    (void) hts_parse_llint(p + 1, &end, min, max,
                           &ignored); // only to skip the refused digits
  if (end == p)
    return 0;
  *s = end;
  return -1;
}

hts_boolean hts_parse_url_port(const char *a, int *port) {
  const char *end;
  LLint p;

  if (!hts_parse_llint(a, &end, 1, 65535, &p) || *end != '\0')
    return HTS_FALSE;
  *port = (int) p;
  return HTS_TRUE;
}
