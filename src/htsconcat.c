/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 2014 Xavier Roche and other contributors

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
/* File: Subroutines                                            */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "htssafe.h"
#include "htslib.h"
#include "htsbuf.h"

/* Concatenate a and b into catbuff; an overflow stops before the part that
   does not fit. */
#undef concat
HTSEXT_API char *concat(char *catbuff, size_t size, const char *a, const char *b) {
  const hts_buf out = hts_buf_at(catbuff, size);

  RUNTIME_TIME_CHECK_SIZE(size);

  catbuff[0] = '\0';
  if (a != NULL && !hts_buf_try_cat(out, a))
    return catbuff;
  if (b != NULL)
    (void) !hts_buf_try_cat(out, b);
  return catbuff;
}

// conversion fichier / -> antislash
static char *__fconv(char *a) {
#if HTS_DOSNAME
  int i;

  for(i = 0; a[i] != 0; i++)
    if (a[i] == '/')            // Unix-to-DOS style
      a[i] = '\\';
#endif
  return a;
}

#undef fconcat
#undef concat
HTSEXT_API char *fconcat(char *catbuff, size_t size, const char *a, const char *b) {
  RUNTIME_TIME_CHECK_SIZE(size);
  return __fconv(concat(catbuff, size, a, b));
}

#undef fconv
HTSEXT_API char *fconv(char *catbuff, size_t size, const char *a) {
  RUNTIME_TIME_CHECK_SIZE(size);
  return __fconv(concat(catbuff, size, a, ""));
}

static hts_boolean is_path_sep(char c) {
  return c == '/' || (HTS_DOSNAME && c == '\\');
}

hts_boolean hts_path_join(hts_buf b, const char *dir, const char *name) {
  const size_t dlen = strlen(dir);
  const size_t nlen = strlen(name);
  const size_t slash = dlen != 0 && nlen != 0 && !is_path_sep(dir[dlen - 1]) &&
                       !is_path_sep(name[0]);

  if (b.size == 0)
    return HTS_FALSE;
  /* b.size - dlen - slash is at least 0 once dlen < b.size */
  if (dlen >= b.size || nlen >= b.size - dlen - slash) {
    b.p[0] = '\0';
    return HTS_FALSE;
  }
  memcpy(b.p, dir, dlen);
  if (slash)
    b.p[dlen] = '/';
  memcpy(b.p + dlen + slash, name, nlen + 1);
  return HTS_TRUE;
}

hts_boolean hts_fpath_join(hts_buf b, const char *dir, const char *name) {
  if (!hts_path_join(b, dir, name))
    return HTS_FALSE;
  __fconv(b.p);
  return HTS_TRUE;
}

/* / et \\ en / */
static char *__fslash(char *a) {
  int i;

  for(i = 0; a[i] != 0; i++)
    if (a[i] == '\\')           // convertir
      a[i] = '/';
  return a;
}

char *fslash(hts_buf dst, const char *a) {
  return __fslash(concat(dst.p, dst.size, a, NULL));
}
