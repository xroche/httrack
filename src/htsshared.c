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
/* File: file and URL helpers that htsserver and proxytrack     */
/*       compile in, because libhttrack does not export them    */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htsshared.h"

#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

int hts_shared_linput(FILE *fp, char *s, int max, hts_boolean skip_nul) {
  int c;
  int j = 0;

  do {
    c = fgetc(fp);
    if (c != EOF) {
      switch (c) {
      case 13:
        break;
      case 10:
        c = -1;
        break;
      case 9:
      case 12:
        break;
      case 0:
        if (skip_nul)
          break;
        s[j++] = (char) c;
        break;
      default:
        s[j++] = (char) c;
        break;
      }
    }
  } while ((c != -1) && (c != EOF) && (j < (max - 1)));
  if (j == max - 1 && c != -1 && c != EOF) {
    /* Stopped at the bound: consume the newline, or the next read returns "" */
    while ((c = fgetc(fp)) == 13)
      ;
    if (c != 10 && c != EOF)
      ungetc(c, fp);
  }
  s[j] = '\0';
  return j;
}

hts_boolean hts_shared_fexist(const char *s) {
  struct stat st;

  memset(&st, 0, sizeof(st));
  return stat(s, &st) == 0 && S_ISREG(st.st_mode);
}

hts_boolean hts_shared_link_has_authority(const char *lien) {
  const char *a = lien;

  if (isalpha((const unsigned char) *a)) {
    while (isalpha((const unsigned char) *a))
      a++;
    if (*a == ':')
      a++;
    else
      return HTS_FALSE;
  }
  return strncmp(a, "//", 2) == 0;
}
