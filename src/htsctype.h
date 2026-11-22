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
/* File: Locale-independent character helpers                  */
/* ------------------------------------------------------------ */

/* <ctype.h> and strcasecmp() follow the locale a host may set (Qt does), and
   <ctype.h> is undefined for a negative char. These helpers act as the "C"
   locale on any char. tests/01_engine-locale.test bans the libc forms. */

#ifndef HTS_CTYPE_DEFH
#define HTS_CTYPE_DEFH

#include "htsglobal.h"

#include <stddef.h>

static HTS_INLINE HTS_UNUSED int hts_isdigit(int c) {
  return c >= '0' && c <= '9';
}

static HTS_INLINE HTS_UNUSED int hts_isupper(int c) {
  return c >= 'A' && c <= 'Z';
}

static HTS_INLINE HTS_UNUSED int hts_islower(int c) {
  return c >= 'a' && c <= 'z';
}

static HTS_INLINE HTS_UNUSED int hts_isalpha(int c) {
  return hts_isupper(c) || hts_islower(c);
}

static HTS_INLINE HTS_UNUSED int hts_isalnum(int c) {
  return hts_isalpha(c) || hts_isdigit(c);
}

static HTS_INLINE HTS_UNUSED int hts_isxdigit(int c) {
  return hts_isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/* Space, \t, \n, \v, \f and \r, as isspace() in the "C" locale. */
static HTS_INLINE HTS_UNUSED int hts_isspace(int c) {
  return c == ' ' || (c >= '\t' && c <= '\r');
}

/* Any other value, a negative char included, comes back unchanged. */
static HTS_INLINE HTS_UNUSED int hts_tolower(int c) {
  return hts_isupper(c) ? c + ('a' - 'A') : c;
}

static HTS_INLINE HTS_UNUSED int hts_toupper(int c) {
  return hts_islower(c) ? c - ('a' - 'A') : c;
}

/* strncasecmp() that folds only A-Z and compares bytes as unsigned char. */
static HTS_INLINE HTS_UNUSED int hts_strncasecmp(const char *a, const char *b,
                                                 size_t n) {
  for (; n != 0; a++, b++, n--) {
    const int ca = hts_tolower((unsigned char) *a);
    const int cb = hts_tolower((unsigned char) *b);

    if (ca != cb || ca == '\0')
      return ca - cb;
  }
  return 0;
}

static HTS_INLINE HTS_UNUSED int hts_strcasecmp(const char *a, const char *b) {
  return hts_strncasecmp(a, b, (size_t) -1);
}

/* Lower-case s in place. */
static HTS_INLINE HTS_UNUSED void hts_lowcase(char *s) {
  for (; *s != '\0'; s++)
    *s = (char) hts_tolower((unsigned char) *s);
}

#endif
