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
/* File: percent-escape decoding, shared by the engine and       */
/*       htsserver                                              */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htsescape.h"
#include "htslib.h"

/* The bounds check must run before hts_ehex(), or a truncated escape is read
   past the terminator. */
void hts_unescapehttp(const char *s, String *tempo) {
  size_t i;

  for (i = 0; s[i] != '\0'; i++) {
    int h;

    if (s[i] == '%' && s[i + 1] == '%') {
      i++;
      StringAddchar(*tempo, '%');
    } else if (s[i] == '%' && s[i + 1] != '\0' && s[i + 2] != '\0' &&
               (h = hts_ehex(&s[i + 1])) >= 0) {
      StringAddchar(*tempo, (char) h);
      i += 2;
    } else if (s[i] == '+') {
      StringAddchar(*tempo, ' ');
    } else {
      StringAddchar(*tempo, s[i]);
    }
  }
}

void hts_unescapeini(const char *s, String *tempo) {
  size_t i;
  char lastc = 0;

  for (i = 0; s[i] != '\0'; i++) {
    int h;

    if (s[i] == '%' && s[i + 1] == '%') {
      i++;
      StringAddchar(*tempo, lastc = '%');
    } else if (s[i] == '%' && s[i + 1] != '\0' && s[i + 2] != '\0' &&
               (h = hts_ehex(&s[i + 1])) >= 0) {
      const char hc = (char) h;

      if (!is_retorsep(hc) || !is_retorsep(lastc)) {
        StringAddchar(*tempo, lastc = hc);
      }
      i += 2;
    } else {
      StringAddchar(*tempo, lastc = s[i]);
    }
  }
}
