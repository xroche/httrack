/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 2013 Xavier Roche and other contributors

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
/* File: Encoding conversion functions                          */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTS_ENCODING_DEFH
#define HTS_ENCODING_DEFH

#include "htsglobal.h"

/** Standard includes. **/
#include <stdlib.h>
#include <string.h>
#include "htswin32.h"

/**
 * Flags for hts_unescapeUrlSpecial().
 **/
typedef enum unescapeFlags {
  /** Do not decode ASCII. **/
  UNESCAPE_URL_NO_ASCII = 1
} unescapeFlags;

/* Value of one hex digit, or -1 if 'c' is not one. */
static HTS_INLINE HTS_UNUSED int hts_ehexh(const char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  else if (c >= 'a' && c <= 'f')
    return (c - 'a' + 10);
  else if (c >= 'A' && c <= 'F')
    return (c - 'A' + 10);
  else
    return -1;
}

/* Value of the two-hex-digit sequence at 's', or -1 if either digit is not one.
 */
static HTS_INLINE HTS_UNUSED int hts_ehex(const char *s) {
  const int c1 = hts_ehexh(s[0]);

  if (c1 >= 0) {
    const int c2 = hts_ehexh(s[1]);

    if (c2 >= 0) {
      return 16 * c1 + c2;
    }
  }
  return -1;
}

/**
 * Unescape HTML entities (as per HTML 4.0 Specification)
 * and replace them in-place by their UTF-8 equivalents.
 * Note: source and destination may be the same, and the destination only
 * needs to hold as space as the source.
 * Returns 0 upon success, -1 upon overflow or error.
 **/
extern int hts_unescapeEntities(const char *src,
                                char *dest, const size_t max);

/**
 * Unescape HTML entities (as per HTML 4.0 Specification)
 * and replace them in-place by their charset equivalents.
 * Note: source and destination may be the same, and the destination only
 * needs to hold as space as the source.
 * Returns 0 upon success, -1 upon overflow or error.
 **/
extern int hts_unescapeEntitiesWithCharset(const char *src,
                                           char *dest, const size_t max,
                                           const char *charset);

/**
 * Flags for hts_unescapeEntitiesWithCharsetSpecial(). Values stay distinct from
 * unescapeFlags above: both reach their function as a plain int.
 **/
typedef enum unescapeEntitiesFlags {
  /** The destination is a URL query string: write a reference the charset can
      not represent as %26%23<decimal>%3B (URL Standard), instead of source text
      whose '&' and '#' would re-split the query. **/
  UNESCAPE_ENTITIES_URL_QUERY = 2
} unescapeEntitiesFlags;

/**
 * Unescape HTML entities into their charset equivalents, "flags" being a mask
 * of UNESCAPE_ENTITIES_XXX constants.
 * Note: source and destination MUST NOT be the same with a flag that may grow
 * the string (UNESCAPE_ENTITIES_URL_QUERY).
 * Returns 0 upon success, -1 upon overflow or error.
 **/
extern int hts_unescapeEntitiesWithCharsetSpecial(const char *src, char *dest,
                                                  const size_t max,
                                                  const char *charset,
                                                  const int flags);

/**
 * Unescape an URL-encoded string. The implicit charset is UTF-8.
 * In case of UTF-8 decoding error inside URL-encoded characters, 
 * the characters are left undecoded.
 * Note: source and destination MUST NOT be the same.
 * Returns 0 upon success, -1 upon overflow or error.
 **/
extern int hts_unescapeUrl(const char *src, char *dest, const size_t max);

/**
 * Unescape an URL-encoded string. The implicit charset is UTF-8.
 * In case of UTF-8 decoding error inside URL-encoded characters,
 * the characters are left undecoded.
 * "flags" is a mask composed of UNESCAPE_URL_XXX constants.
 * Note: source and destination MUST NOT be the same.
 * Returns 0 upon success, -1 upon overflow or error.
 **/
extern int hts_unescapeUrlSpecial(const char *src, char *dest, const size_t max,
                                  const int flags);

#endif
