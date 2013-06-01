/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) Xavier Roche and other contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 3
of the License, or any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

Important notes:

- We hereby ask people using this source NOT to use it in purpose of grabbing
emails addresses, or collecting any other private information on persons.
This would disgrace our work, and spoil the many hours we spent on it.

Please visit our Website: http://www.httrack.com
*/

/* ------------------------------------------------------------ */
/* File: Encoding conversion functions                          */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTS_ENCODING_DEFH
#define HTS_ENCODING_DEFH

/** Standard includes. **/
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

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
 * Unescape an URL-encoded string. The implicit charset is UTF-8.
 * In case of UTF-8 decoding error inside URL-encoded characters, 
 * the characters are left undecoded.
 * Note: source and destination MUST NOT be the same.
 * Returns 0 upon success, -1 upon overflow or error.
 **/
extern int hts_unescapeUrl(const char *src, char *dest, const size_t max);

#endif
