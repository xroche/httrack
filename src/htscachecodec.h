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
   readers expand strfield2() and CACHE_ENTRYNAME_SIZE from the caller's. */
#include "htsglobal.h"
#include "htssafe.h"
#include "htsdate.h"
#include "minizip/zip.h"
#include "minizip/unzip.h"

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

/* Copy the line at p, without its CRs, into line[size] and return where the
   next one starts. Stops at end or a NUL. A line too long for line is consumed
   whole, so its tail never reads as another line, and *cut is set. */
static HTS_UNUSED const char *hts_cache_line(const char *p, const char *end,
                                             char *line, size_t size,
                                             hts_boolean *cut) {
  size_t used = 0;

  *cut = HTS_FALSE;
  for (; p < end && *p != '\0' && *p != '\n'; p++) {
    if (*p == '\r') {
      continue;
    } else if (used < size - 1) {
      line[used++] = *p;
    } else {
      *cut = HTS_TRUE;
    }
  }
  line[used] = '\0';
  return p < end && *p == '\n' ? p + 1 : p;
}

typedef enum hts_cache_field_kind {
  HTS_CACHE_FIELD_INT,    /* dest is an int */
  HTS_CACHE_FIELD_LLINT,  /* dest is an LLint */
  HTS_CACHE_FIELD_STRING, /* dest is a char[dest_size], clipped to it */
  HTS_CACHE_FIELD_SKIP    /* consumed, dest unused */
} hts_cache_field_kind;

typedef struct hts_cache_field {
  const char *name;
  hts_cache_field_kind kind;
  void *dest;
  size_t dest_size;
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
    *(int *) fields[i].dest = intval;
  } else if (fields[i].kind == HTS_CACHE_FIELD_LLINT) {
    LLint intval = 0;

    sscanf(value, LLintP, &intval);
    *(LLint *) fields[i].dest = intval;
  } else if (fields[i].kind == HTS_CACHE_FIELD_STRING) {
    /* a corrupt cache can carry a field wider than ours; clipping keeps the
       entry, where aborting would take the crawl down */
    (void) strclipbuff(fields[i].dest, fields[i].dest_size, value);
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
    hts_boolean cut;
    char *value;

    p = hts_cache_line(p, end, line, sizeof(line) - 1, &cut);
    if (line[0] == '\0') {
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

/* Receives each indexed entry: its name without "http://", and its offset,
   negated when the data is not in the zip. */
typedef void (*hts_cache_index_entry)(void *arg, const char *name, long pos);
/* Receives a damaged entry: what is wrong, and how many entries read before. */
typedef void (*hts_cache_index_bad)(void *arg, const char *what, int entry);

/* Is the X-In-Cache line among the first two of a header block? Matched
   case-insensitively; a NUL ends the block. */
static HTS_UNUSED hts_boolean hts_cache_header_in_cache(const char *buff,
                                                        size_t size) {
  const char *p = buff;
  const char *const end = buff + size;
  int lines;

  for (lines = 0; lines < 2 && p < end && *p != '\0'; lines++) {
    char line[1024];
    hts_boolean cut;

    p = hts_cache_line(p, end, line, sizeof(line), &cut);
    if (strfield(line, "X-In-Cache:")) {
      return strfield2(line, "X-In-Cache: 1") ? HTS_TRUE : HTS_FALSE;
    }
  }
  return HTS_FALSE;
}

/* Walk every entry of an open ZIP cache, passing each to entry and each damaged
   one to bad. Returns unzGoToFirstFile()'s error, Z_OK when the walk ran, and
   the number of entries read through *entries. */
static HTS_UNUSED int hts_cache_walk_index(unzFile zFile,
                                           hts_cache_index_entry entry,
                                           hts_cache_index_bad bad, void *arg,
                                           int *entries) {
  char comment[128];
  char BIGSTK filename[CACHE_ENTRYNAME_SIZE];
  unz_file_info zfi;
  const int err = unzGoToFirstFile(zFile);

  *entries = 0;
  if (err != Z_OK) {
    return err;
  }
  memset(comment, 0, sizeof(comment)); // for truncated reads
  do {
    int readSizeHeader;

    filename[0] = '\0';
    comment[0] = '\0';
    memset(&zfi, 0, sizeof(zfi));
    if (unzOpenCurrentFile(zFile) != Z_OK) {
      bad(arg, "Corrupted cache entry", *entries);
      continue;
    }
    if ((readSizeHeader =
             unzGetLocalExtrafield(zFile, comment, sizeof(comment) - 2)) > 0 &&
        unzGetCurrentFileInfo(zFile, &zfi, filename, sizeof(filename) - 2, NULL,
                              0, NULL, 0) == Z_OK
        /* minizip leaves a name this long unterminated, and a clipped one
           would index as another URL's key */
        && zfi.size_filename < sizeof(filename) - 2) {
      const long pos = (long) unzGetOffset(zFile);

      assertf((size_t) readSizeHeader < sizeof(comment));
      comment[readSizeHeader] = '\0';
      (*entries)++;
      if (pos > 0) {
        const char *const name =
            filename + (strfield(filename, "http://") ? 7 : 0);

        entry(arg, name,
              hts_cache_header_in_cache(comment, (size_t) readSizeHeader)
                  ? pos
                  : -pos);
      } else {
        bad(arg, "Corrupted cache meta entry", *entries);
      }
    } else {
      bad(arg, "Corrupted cache entry", *entries);
    }
    unzCloseCurrentFile(zFile);
  } while (unzGoToNextFile(zFile) == Z_OK);
  return Z_OK;
}

#endif
