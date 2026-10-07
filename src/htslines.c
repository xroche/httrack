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
/* File: htslines.c                                             */
/*       line readers for config files, shared by the engine    */
/*       and htsserver                                          */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htslines.h"

#include <string.h>

/* is_realspace(), which lives in htslib.h, out of htsserver's reach. */
static hts_boolean line_is_space(int c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' ||
                 c == '\r'
             ? HTS_TRUE
             : HTS_FALSE;
}

/* hts_readline(), also giving the line's last non-space byte, stored or not,
   or -1: a clipped line's continuation is decided by the byte the clip hid. */
static hts_boolean readline_tail(FILE *fp, char *s, size_t max, int flags,
                                 size_t *len, int *tail) {
  hts_boolean cut = HTS_FALSE;
  size_t j = 0;
  int c;

  *tail = -1;
  while ((c = fgetc(fp)) != EOF && c != '\n') {
    if (c == '\r' ||
        ((flags & HTS_LINE_DROP_TAB) != 0 && (c == '\t' || c == '\f')) ||
        ((flags & HTS_LINE_DROP_NUL) != 0 && c == '\0'))
      continue;
    if (!line_is_space(c))
      *tail = c;
    if (j + 1 < max)
      s[j++] = (char) c;
    else
      cut = HTS_TRUE;
  }
  s[j] = '\0';
  if (len != NULL)
    *len = j;
  return cut;
}

hts_boolean hts_readline(FILE *fp, char *s, size_t max, int flags,
                         size_t *len) {
  int tail;

  return readline_tail(fp, s, max, flags, len, &tail);
}

/* hts_readline() trimmed: trailing whitespace, then leading spaces and TABs. */
static hts_boolean readline_trim(FILE *fp, char *s, size_t max, int flags,
                                 size_t *len, int *tail) {
  const hts_boolean cut = readline_tail(fp, s, max, flags, len, tail);
  size_t n = *len;
  size_t i = 0;

  while (n > 0 && line_is_space((unsigned char) s[n - 1]))
    s[--n] = '\0';
  while (i < n && (s[i] == ' ' || s[i] == '\t'))
    i++;
  if (i > 0) {
    memmove(s, s + i, n - i);
    n -= i;
    s[n] = '\0';
  }
  *len = n;
  return cut;
}

hts_boolean hts_readline_cpp(FILE *fp, char *s, size_t max, int flags) {
  hts_boolean cut = HTS_FALSE;
  hts_boolean more;
  size_t rlen = 0;

  s[0] = '\0';
  do {
    size_t n;
    int tail;

    if (!cut && rlen > 0 && s[rlen - 1] == '\\')
      s[--rlen] = '\0';
    if (readline_trim(fp, s + rlen, max - rlen, flags, &n, &tail))
      cut = HTS_TRUE;
    rlen += n;
    /* once clipped, s no longer ends where the line does */
    more = cut ? tail == '\\' : rlen > 0 && s[rlen - 1] == '\\';
  } while (more);
  return cut;
}
