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

#endif
