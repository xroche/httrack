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
/* File: Charset conversion functions                           */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTS_CHARSET_DEFH
#define HTS_CHARSET_DEFH

/** Standard includes. **/
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

/**
 * Convert the string "s" from charset "charset" to UTF-8.
 * Return NULL upon error.
 **/
extern char *hts_convertStringToUTF8(const char *s, size_t size, const char *charset);

/**
 * Convert the string "s" from UTF-8 to charset "charset".
 * Return NULL upon error.
 **/
extern char *hts_convertStringFromUTF8(const char *s, size_t size, const char *charset);

/**
 * Extract the charset from the HTML buffer "html"
 **/
extern char* hts_getCharsetFromMeta(const char *html, size_t size);

#ifdef _WIN32

/**
 * Convert UTF-8 to WCHAR.
 **/
extern LPWSTR hts_convertUTF8StringToUCS2(const char *s, int size, int* pwsize);

/**
 * Convert from WCHAR.
 **/
extern char *hts_convertUCS2StringToUTF8(LPWSTR woutput, int wsize);

/**
 * Convert current system codepage to UTF-8.
 **/
extern char *hts_convertStringSystemToUTF8(const char *s, size_t size);

#endif

#endif
