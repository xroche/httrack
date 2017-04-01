/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998-2017 Xavier Roche and other contributors

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

/** UCS4 type. **/
typedef unsigned int hts_UCS4;

/** Leading character (ASCII or leading UTF-8 sequence) **/
#define HTS_IS_LEADING_UTF8(C) ((unsigned char)(C) < 0x80 || (unsigned char)(C) >= 0xc0)

/**
 * Convert the string "s" from charset "charset" to UTF-8.
 * Return NULL upon error.
 **/
extern char *hts_convertStringToUTF8(const char *s, size_t size,
                                     const char *charset);

/**
 * Convert the string "s" from UTF-8 to charset "charset".
 * Return NULL upon error.
 **/
extern char *hts_convertStringFromUTF8(const char *s, size_t size,
                                       const char *charset);

/**
 * Convert an UTF-8 string to an IDNA (RFC 3492) string.
 **/
extern char *hts_convertStringUTF8ToIDNA(const char *s, size_t size);

/**
 * Convert an IDNA (RFC 3492) string to an UTF-8 string.
 **/
extern char *hts_convertStringIDNAToUTF8(const char *s, size_t size);

/**
 * Has the given string any IDNA segments ?
 **/
extern int hts_isStringIDNA(const char *s, size_t size);

/**
 * Extract the charset from the HTML buffer "html"
 **/
extern char *hts_getCharsetFromMeta(const char *html, size_t size);

/**
 * Is the given string an ASCII string ?
 **/
extern int hts_isStringAscii(const char *s, size_t size);

/**
 * Is the given string an UTF-8 string ?
 **/
extern int hts_isStringUTF8(const char *s, size_t size);

/**
 * Is the given charset the UTF-8 charset ?
 **/
extern int hts_isCharsetUTF8(const char *charset);

/**
 * Get an UTF-8 string length in characters.
 **/
extern size_t hts_stringLengthUTF8(const char *s);

/**
 * Copy at most 'nBytes' bytes from src to dest, not truncating UTF-8
 * sequences.
 * Returns the number of bytes copied, not including the terminating \0.
 **/
extern size_t hts_copyStringUTF8(char *dest, const char *src, 
                                 size_t nBytes);

/**
 * Append at most 'nBytes' bytes from src to dest, not truncating UTF-8
 * sequences.
 * Returns the number of bytes appended, not including the terminating \0.
 **/
extern size_t hts_appendStringUTF8(char *dest, const char *src, 
                                   size_t nBytes);

/**
 * Convert an UTF-8 string into an Unicode string (0-terminated).
 **/
extern hts_UCS4* hts_convertUTF8StringToUCS4(const char *s, size_t size, 
                                             size_t *nChars);

/**
 * Convert an Unicode string into an UTF-8 string.
 **/
extern char *hts_convertUCS4StringToUTF8(const hts_UCS4 *s, size_t nChars);

/**
 * Return the length (in characters) of an UCS4 string terminated by 0.
 **/
extern size_t hts_stringLengthUCS4(const hts_UCS4 *s);

/**
 * Write the Unicode character 'uc' in 'dest' of maximum size 'size'.
 * Return the number of bytes written, or 0 upon error.
 * Note: does not \0-terminate the destination buffer.
 **/
extern size_t hts_writeUTF8(hts_UCS4 uc, char *dest, size_t size);

/**
 * Read the next Unicode character within 'src' of size 'size' and, upon
 * successful reading, return the number of bytes read and place the
 * character is 'puc'.
 * Return 0 upon error.
 **/
extern size_t hts_readUTF8(const char *src, size_t size, hts_UCS4 *puc);

/**
 * Given the first UTF-8 sequence character, get the total number of
 * characters in the sequence (1 for ASCII). 
 * Return 0 upon error (not a leading character).
 **/
extern size_t hts_getUTF8SequenceLength(const char lead);

/** WIN32 specific functions. **/
#ifdef _WIN32
/**
 * Convert UTF-8 to WCHAR.
 * This function is WIN32 specific.
 **/
extern LPWSTR hts_convertUTF8StringToUCS2(const char *s, int size, int *pwsize);

/**
 * Convert from WCHAR.
 * This function is WIN32 specific.
 **/
extern char *hts_convertUCS2StringToUTF8(LPWSTR woutput, int wsize);

/**
 * Convert current system codepage to UTF-8.
 * This function is WIN32 specific.
 **/
extern char *hts_convertStringSystemToUTF8(const char *s, size_t size);
#endif

#endif
