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

Please visit our Website: http://www.httrack.com
*/

/* ------------------------------------------------------------ */
/* File: Strings                                                */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

// Strings a bit safer than static buffers

#ifndef HTS_PROXYSTRINGS_DEFSTATIC
#define HTS_PROXYSTRINGS_DEFSTATIC

#include "htsstrings.h"

/* Tools */

HTS_UNUSED static void escapexml(const char *s, String * tempo) {
  int i;

  for(i = 0; s[i] != '\0'; i++) {
    if (s[i] == '&')
      StringCat(*tempo, "&amp;");
    else if (s[i] == '<')
      StringCat(*tempo, "&lt;");
    else if (s[i] == '>')
      StringCat(*tempo, "&gt;");
    else if (s[i] == '\"')
      StringCat(*tempo, "&quot;");
    else
      StringAddchar(*tempo, s[i]);
  }
}

HTS_UNUSED static char* file_convert(char *dest, size_t size, const char *src) {
  size_t i;
  for(i = 0 ; src[i] != '\0' && i + 1 < size ; i++) {
#ifdef _WIN32
    if (src[i] == '/') {
      dest[i] = '\\';
    } else {
#endif
    dest[i] = src[i];
#ifdef _WIN32
    }
#endif
  }
  dest[i] = '\0';
  return dest;
}

#endif
