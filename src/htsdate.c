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

#include "htsdate.h"

#include "htsglobal.h"
#include "htssafe.h"
#include "htsurlport.h"

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

/* hts_lowcase() lives in the library proxytrack does not link. */
static void lowcase(char *s) {
  size_t i;

  for (i = 0; s[i] != '\0'; i++)
    if ((s[i] >= 'A') && (s[i] <= 'Z'))
      s[i] += ('a' - 'A');
}

struct tm *convert_time_rfc822(struct tm *result, const char *s) {
  return convert_time_rfc822_pivot(result, s, 50);
}

struct tm *convert_time_rfc822_pivot(struct tm *result, const char *s,
                                     int pivot) {
  char months[] = "jan feb mar apr may jun jul aug sep oct nov dec";
  char str[256];
  char *a;

  /* The numbers in order of appearance, dd first. n1..n4 are then year, hour,
     min, sec, or hour, min, sec, year when n4 is four digits. */
  int result_mm = -1;
  int result_dd = -1;
  int result_n1 = -1;
  int result_n2 = -1;
  int result_n3 = -1;
  int result_n4 = -1;

  if ((int) strlen(s) > 200)
    return NULL;
  strcpybuff(str, s);
  lowcase(str);

  while ((a = strchr(str, '-')))
    *a = ' ';
  while ((a = strchr(str, ':')))
    *a = ' ';
  while ((a = strchr(str, ',')))
    *a = ' ';
  /* tokenise */
  a = str;
  while (*a) {
    char *first, *last;
    char tok[256];

    a += strspn(a, " ");
    first = a;
    a += strcspn(a, " ");
    last = a;
    tok[0] = '\0';
    if (first != last) {
      char *pos;

      strncatbuff(tok, first, (int) (last - first));
      /* classify it */
      if ((pos = strstr(months, tok))) { /* month always in letters */
        result_mm = ((int) (pos - months)) / 4;
      } else {
        const char *digits = tok;
        LLint value;

        /* A '+' zone such as "+0100" reads as its digits. */
        digits += strspn(digits, HTS_REALSPACES);
        if (digits[0] == '+' && isdigit((unsigned char) digits[1]))
          digits++;
        if (hts_scan_llint(&digits, 0, INT_MAX, &value) == HTS_SCAN_OK) {
          const int number = (int) value;

          if (result_dd < 0)                   /* day always first number */
            result_dd = number;
          else if (result_n1 < 0)
            result_n1 = number;
          else if (result_n2 < 0)
            result_n2 = number;
          else if (result_n3 < 0)
            result_n3 = number;
          else if (result_n4 < 0)
            result_n4 = number;
        } /* anything else is noise, "+1GMT" for example */
      }
    }
  }
  if ((result_n1 >= 0) && (result_mm >= 0) && (result_dd >= 0) &&
      (result_n2 >= 0) && (result_n3 >= 0) && (result_n4 >= 0)) {
    if (result_n4 >= 1000) { /* Sun Nov  6 08:49:37 1994 */
      result->tm_year = result_n4 - 1900;
      result->tm_hour = result_n1;
      result->tm_min = result_n2;
      result->tm_sec = result_n3 > 0 ? result_n3 : 0;
    } else { /* Sun, 06 Nov 1994 08:49:37 GMT or Sunday, 06-Nov-94 08:49:37 GMT
              */
      result->tm_hour = result_n2;
      result->tm_min = result_n3;
      result->tm_sec = result_n4 > 0 ? result_n4 : 0;
      if (result_n1 <= pivot) /* 00 means 2000 */
        result->tm_year = result_n1 + 100;
      else if (result_n1 < 1000) /* 99 means 1999 */
        result->tm_year = result_n1;
      else /* 2000 */
        result->tm_year = result_n1 - 1900;
    }
    result->tm_isdst = 0; /* assume GMT */
    result->tm_yday = -1; /* don't know */
    result->tm_wday = -1; /* don't know */
    result->tm_mon = result_mm;
    result->tm_mday = result_dd;
    return result;
  }
  return NULL;
}

/* See htsdate.h. */
int64_t hts_utc_seconds(const struct tm *tm) {
  // days from the civil date, counting years from March (Howard Hinnant)
  int64_t y = (int64_t) tm->tm_year + 1900 + tm->tm_mon / 12;
  int64_t m = tm->tm_mon % 12;
  int64_t era, yoe, doy, days;

  if (m < 0) {
    m += 12;
    y--;
  }
  if (m < 2)
    y--;
  era = (y >= 0 ? y : y - 399) / 400;
  yoe = y - era * 400;
  doy = (153 * ((m + 10) % 12) + 2) / 5 + (int64_t) tm->tm_mday - 1;
  days = era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
  return days * 86400 + (int64_t) tm->tm_hour * 3600 +
         (int64_t) tm->tm_min * 60 + tm->tm_sec;
}

/* See htsdate.h. */
time_t hts_time_clip(int64_t t) {
  if (sizeof(time_t) < sizeof(int64_t)) {
    if (t > INT32_MAX)
      return (time_t) INT32_MAX;
    if (t < INT32_MIN)
      return (time_t) INT32_MIN;
  }
  return (time_t) t;
}

/* See htsdate.h. */
hts_boolean hts_time_exact(int64_t t, time_t *out) {
  const time_t clipped = hts_time_clip(t);

  if ((int64_t) clipped != t)
    return HTS_FALSE;
  *out = clipped;
  return HTS_TRUE;
}

/* See htsdate.h. */
hts_boolean hts_tm_filetime(const struct tm *tm, time_t *out) {
  const int64_t t = hts_utc_seconds(tm);

  if (t == -1 || t == 0)
    return HTS_FALSE;
  // A date past what time_t holds stamps its last second, never 1901.
  *out = hts_time_clip(t);
  return HTS_TRUE;
}
