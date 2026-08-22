/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998-2026 Xavier Roche and other contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

Important notes:

- We hereby ask people using this source NOT to use it in purpose of grabbing
emails addresses, or collecting any other private information on persons.
This would disgrace our work, and spoil the many hours we spent on it.

Please visit our Website: http://www.httrack.com
*/

/* ------------------------------------------------------------ */
/* File: percent-escape decoding shared by the engine and the server */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTS_ESCAPE_DEFH
#define HTS_ESCAPE_DEFH

#include "htsglobal.h"
#include "htsstrings.h"

/* Value of one hex digit, or -1 if 'c' is not one. */
HTS_UNUSED static HTS_INLINE int hts_ehexh(const char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  else if (c >= 'a' && c <= 'f')
    return (c - 'a' + 10);
  else if (c >= 'A' && c <= 'F')
    return (c - 'A' + 10);
  else
    return -1;
}

/* Value of the two-hex-digit sequence at 's', or -1 if either digit is not
   one. Rejecting is what keeps a malformed escape from decoding to a NUL. */
HTS_UNUSED static HTS_INLINE int hts_ehex(const char *s) {
  const int c1 = hts_ehexh(s[0]);

  if (c1 >= 0) {
    const int c2 = hts_ehexh(s[1]);

    if (c2 >= 0) {
      return 16 * c1 + c2;
    }
  }
  return -1;
}

/* Decode application/x-www-form-urlencoded 's' into 'tempo': "%%" is a literal
   '%', '+' is a space, and an escape that is truncated or not hex is copied
   through unchanged. */
extern void hts_unescapehttp(const char *s, String *tempo);

/* As hts_unescapehttp(), minus the '+' rule, and collapsing a decoded run of
   line separators so an escaped CRLF cannot forge an .ini line break. */
extern void hts_unescapeini(const char *s, String *tempo);

#endif
