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
/* File: ZIP cache entry fields, shared by the engine and proxytrack */
/* Author: Xavier Roche                                          */
/* ------------------------------------------------------------ */

#ifndef HTSCACHECODEC_DEFH
#define HTSCACHECODEC_DEFH

/* No engine header here: proxytrack and the fuzzers build it without one. The
   READFIELD macros expand strfield2() from the caller's htslib.h. */
#include "htsglobal.h"
#include "htssafe.h"
#include "htsdate.h"
#include "minizip/zip.h"

#include <string.h>
#include <time.h>

/* Remote-controlled values (ETag, Location, the URL) can together outgrow the
   block, and a clipped field reads back as a valid shorter one, so one that
   does not fit is dropped whole and counted. `headers` must be an array. */
#define ZIP_FIELD_STRING(headers, headersSize, dropped, field, value)          \
  do {                                                                         \
    if ((value) != NULL && (value)[0] != '\0' &&                               \
        !slcatprintfbuff(headers, sizeof(headers), &(headersSize),             \
                         "%s: %s\r\n", field, value)) {                        \
      (dropped)++;                                                             \
    }                                                                          \
  } while (0)
#define ZIP_FIELD_INT(headers, headersSize, dropped, field, value)             \
  do {                                                                         \
    if ((value) != 0 &&                                                        \
        !slcatprintfbuff(headers, sizeof(headers), &(headersSize),             \
                         "%s: " LLintP "\r\n", field, (LLint) (value))) {      \
      (dropped)++;                                                             \
    }                                                                          \
  } while (0)
#define ZIP_FIELD_INT_FORCE(headers, headersSize, dropped, field, value)       \
  do {                                                                         \
    if (!slcatprintfbuff(headers, sizeof(headers), &(headersSize),             \
                         "%s: " LLintP "\r\n", field, (LLint) (value))) {      \
      (dropped)++;                                                             \
    }                                                                          \
  } while (0)

/* A corrupt cache can carry a field wider than ours; clipping it keeps the
   entry, where aborting would take the crawl down. */
#define ZIP_READFIELD_STRING(line, value, refline, refvalue, refvalue_size)    \
  do {                                                                         \
    if (line[0] != '\0' && strfield2(line, refline)) {                         \
      (void) strclipbuff(refvalue, refvalue_size, value);                      \
      line[0] = '\0';                                                          \
    }                                                                          \
  } while (0)
#define ZIP_READFIELD_INT(line, value, refline, refvalue)                      \
  do {                                                                         \
    if (line[0] != '\0' && strfield2(line, refline)) {                         \
      int intval = 0;                                                          \
      sscanf(value, "%d", &intval);                                            \
      (refvalue) = intval;                                                     \
      line[0] = '\0';                                                          \
    }                                                                          \
  } while (0)
#define ZIP_READFIELD_LLINT(line, value, refline, refvalue)                    \
  do {                                                                         \
    if (line[0] != '\0' && strfield2(line, refline)) {                         \
      LLint intval = 0;                                                        \
      sscanf(value, LLintP, &intval);                                          \
      (refvalue) = intval;                                                     \
      line[0] = '\0';                                                          \
    }                                                                          \
  } while (0)

/* Append an entry's first line to headers[*used], capacity size. A message
   too long for it points at X-StatusMessage instead. HTS_FALSE when it does
   not fit. */
static HTS_INLINE HTS_UNUSED hts_boolean hts_cache_status_line(
    char *headers, size_t size, size_t *used, int statuscode, const char *msg) {
  const char *const message = strlen(msg) < 32 ? msg : "(See X-StatusMessage)";

  return slcatprintfbuff(headers, size, used, "HTTP/1.%c %d %s\r\n", '1',
                         statuscode, message);
}

/* Reset fi, then date it from an RFC 822 Last-Modified value when it parses. */
static HTS_INLINE HTS_UNUSED void hts_cache_zip_date(zip_fileinfo *fi,
                                                     const char *lastmodified) {
  memset(fi, 0, sizeof(*fi));
  if (lastmodified[0] != '\0') {
    struct tm buffer;
    const struct tm *const tm_s = convert_time_rfc822(&buffer, lastmodified);

    if (tm_s != NULL) {
      fi->tmz_date.tm_sec = (uInt) tm_s->tm_sec;
      fi->tmz_date.tm_min = (uInt) tm_s->tm_min;
      fi->tmz_date.tm_hour = (uInt) tm_s->tm_hour;
      fi->tmz_date.tm_mday = (uInt) tm_s->tm_mday;
      fi->tmz_date.tm_mon = (uInt) tm_s->tm_mon;
      fi->tmz_date.tm_year = (uInt) tm_s->tm_year;
    }
  }
}

#endif
