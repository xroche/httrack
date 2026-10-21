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
/* File: RFC822 date parser, shared by the engine and proxytrack */
/* Author: Xavier Roche                                          */
/* ------------------------------------------------------------ */

#ifndef HTSDATE_DEFH
#define HTSDATE_DEFH

#include "htsglobal.h"

#include <stdint.h>
#include <time.h>

/* Parse an RFC822 date such as "Sun, 06 Nov 1994 08:49:37 GMT" into "result",
   which is returned, or NULL if "s" does not parse. The fields are GMT, so a
   caller wanting seconds uses hts_utc_seconds(), never mktime() (#1491). This
   file is separate so proxytrack, which does not link the library, can share
   it. */
struct tm *convert_time_rfc822(struct tm *result, const char *s);

/* As convert_time_rfc822, but a two-digit year up to PIVOT means 20xx and one
   above it 19xx. HTTP dates use 50, and cookies use 69 (RFC 6265 5.1.1). */
struct tm *convert_time_rfc822_pivot(struct tm *result, const char *s,
                                     int pivot);

/* Return the Unix time of the UTC date in tm, in 64 bits because a 32-bit
   time_t ends in 2038. Out-of-range fields carry over as with timegm(). */
int64_t hts_utc_seconds(const struct tm *tm);

/* Return t as a time_t, clipped to the range this platform's time_t holds. */
time_t hts_time_clip(int64_t t);

/* Does t fit this platform's time_t exactly? If so, store it in *out. */
hts_boolean hts_time_exact(int64_t t, time_t *out);

/* Store the file time for the UTC date in tm in *out, clipped to time_t. False
   when the date is the epoch or one second before it, which mean no date. */
hts_boolean hts_tm_filetime(const struct tm *tm, time_t *out);

#endif
