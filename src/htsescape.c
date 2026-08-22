/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998-2026 Xavier Roche and other contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

Important notes:

- We hereby ask people using this source NOT to use it in purpose of grabbing
emails addresses, or collecting any other private information on persons.
This would disgrace our work, and spoil the many hours we spent on it.

Please visit our Website: http://www.httrack.com
*/

/* ------------------------------------------------------------ */
/* File: percent-escape decoding shared by the engine and the server */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htsescape.h"

/* CR, LF or TAB: htslib.h's is_retorsep() set, spelled out so this unit stays
   linkable into htsserver and proxytrack without the engine headers. */
static HTS_INLINE int hts_is_retorsep(const char c) {
  return c == 10 || c == 13 || c == 9;
}

/* An escape needs both digits before the terminator, or reading them runs past
   it; hts_ehex() then rejects anything that is not hex. Either way the '%' is
   copied through as itself rather than decoded to a NUL. */
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

      if (!hts_is_retorsep(hc) || !hts_is_retorsep(lastc)) {
        StringAddchar(*tempo, lastc = hc);
      }
      i += 2;
    } else {
      StringAddchar(*tempo, lastc = s[i]);
    }
  }
}
