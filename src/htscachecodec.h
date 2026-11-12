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
/* File: ZIP and ARC cache entry fields, shared by engine and proxytrack */
/* Author: Xavier Roche                                          */
/* ------------------------------------------------------------ */

#ifndef HTSCACHECODEC_DEFH
#define HTSCACHECODEC_DEFH

/* No engine header here: proxytrack and the fuzzers build it without one. The
   field readers expand strfield2() from the caller's htslib.h. */
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

typedef enum hts_cache_field_kind {
  HTS_CACHE_FIELD_INT,    /* value is an int */
  HTS_CACHE_FIELD_LLINT,  /* value is an LLint */
  HTS_CACHE_FIELD_STRING, /* value is a char[size], clipped to it */
  HTS_CACHE_FIELD_SKIP    /* consumed, value unused */
} hts_cache_field_kind;

typedef struct hts_cache_field {
  const char *name;
  hts_cache_field_kind kind;
  void *value;
  size_t size;
} hts_cache_field;

/* Store value into the field called name, matched case-insensitively.
   HTS_FALSE when no field has that name. */
static HTS_UNUSED hts_boolean
hts_cache_store_field(const hts_cache_field *fields, size_t count,
                      const char *name, const char *value) {
  size_t i;

  for (i = 0; i < count && !strfield2(name, fields[i].name); i++) {
  }
  if (i == count) {
    return HTS_FALSE;
  } else if (fields[i].kind == HTS_CACHE_FIELD_INT) {
    int intval = 0;

    sscanf(value, "%d", &intval);
    *(int *) fields[i].value = intval;
  } else if (fields[i].kind == HTS_CACHE_FIELD_LLINT) {
    LLint intval = 0;

    sscanf(value, LLintP, &intval);
    *(LLint *) fields[i].value = intval;
  } else if (fields[i].kind == HTS_CACHE_FIELD_STRING) {
    /* a corrupt cache can carry a field wider than ours; clipping keeps the
       entry, where aborting would take the crawl down */
    (void) strclipbuff((char *) fields[i].value, fields[i].size, value);
  }
  return HTS_TRUE;
}

/* Receives a header line no field names; may be NULL. */
typedef void (*hts_cache_field_other)(void *arg, const char *name,
                                      const char *value);

/* Read the header block buff[0..size) of one ZIP cache entry into fields,
   matching names case-insensitively, until the first empty line. HTS_FALSE
   when the block holds a NUL: the writers store strlen(headers), so it is
   damage and the entry must be refused. */
static HTS_UNUSED hts_boolean hts_cache_read_fields(
    const char *buff, size_t size, const hts_cache_field *fields, size_t count,
    hts_cache_field_other other, void *arg) {
  const char *p = buff;
  const char *const end = buff + size;
  /* the longest stored line is "Location: " plus a full-capacity URL;
     a shorter buffer truncates it into a valid-looking wrong target */
  char BIGSTK line[HTS_LOCATION_SIZE + 32];

  if (memchr(buff, '\0', size) != NULL) {
    return HTS_FALSE;
  }
  while (p < end) {
    size_t used = 0;
    hts_boolean cut = HTS_FALSE;
    char *value;

    /* a cut line is consumed whole, so its tail never reads as a field */
    for (; p < end && *p != '\n'; p++) {
      if (*p == '\r') {
        continue;
      } else if (used < sizeof(line) - 2) {
        line[used++] = *p;
      } else {
        cut = HTS_TRUE;
      }
    }
    if (p < end) {
      p++;
    }
    line[used] = '\0';
    if (used == 0) {
      break;
    }
    /* no line we write is this long, so a cut one is foreign or damaged and
       its prefix would parse as a wrong value */
    value = cut ? NULL : strchr(line, ':');
    if (value == NULL || value == line) {
      continue;
    }
    *value++ = '\0';
    if (*value == ' ' || *value == '\t') {
      value++;
    }
    if (!hts_cache_store_field(fields, count, line, value) && other != NULL) {
      other(arg, line, value);
    }
  }
  return HTS_TRUE;
}

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
