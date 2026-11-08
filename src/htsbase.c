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
/* File: libc-only helpers declared in htslib.h                 */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htslib.h"

#include <stdio.h>
#include <string.h>

/* user abort callback */
static htsErrorCallback htsCallbackErr = NULL;

HTSEXT_API void hts_set_error_callback(htsErrorCallback handler) {
  htsCallbackErr = handler;
}

HTSEXT_API htsErrorCallback hts_get_error_callback(void) {
  return htsCallbackErr;
}

const char *hts_strerror(int err, char *buf, size_t size) {
  assertf(buf != NULL && size != 0);
  buf[0] = '\0';
  /* Only the terminator fits, and the libcs disagree on that boundary. */
  if (size == 1)
    return buf;
#if !HTS_STRERROR_REENTRANT
  /* No reentrant form here, so this build stays as racy as it was. */
  (void) strclipbuff(buf, size, strerror(err));
#elif defined(_MSC_VER)
  (void) strerror_s(buf, size, err);
#elif defined(__USE_GNU)
  {
    /* The GNU variant's return is the message and buf may stay untouched,
       where the XSI one's return is a status. Only __USE_GNU tells them apart
       here, since configure's probe sets _GNU_SOURCE and this tree does not. */
    const char *const msg = strerror_r(err, buf, size);

    if (msg != NULL && msg != buf)
      (void) strclipbuff(buf, size, msg);
  }
#else
  /* Nonzero is ERANGE or EINVAL, and buf may still hold a partial message. */
  (void) strerror_r(err, buf, size);
#endif
  buf[size - 1] = '\0';
  if (buf[0] == '\0')
    (void) strclipbuff(buf, size, "unknown error");
  return buf;
}

// lower-case conversion into caller buffer (capacity catbuffsize)
char *convtolower(char *catbuff, size_t catbuffsize, const char *a) {
  strlcpybuff(catbuff, a, catbuffsize);
  hts_lowcase(catbuff); // lower case
  return catbuff;
}

// lower-case s in place
void hts_lowcase(char *s) {
  size_t i;

  for (i = 0; s[i] != '\0'; i++)
    if ((s[i] >= 'A') && (s[i] <= 'Z'))
      s[i] += ('a' - 'A');
}

/* File size, -1 if absent */
/* Note: NOT utf-8 */
LLint fsize(const char *s) {
  STRUCT_STAT st;
  int err;

  if (!strnotempty(s)) // empty name: error
    return -1;
  /* _stat64, not stat(): the latter is 32-bit on MSVC */
#ifdef _WIN32
  err = _stat64(s, &st);
#else
  err = stat(s, &st);
#endif
  return err == 0 && S_ISREG(st.st_mode) ? st.st_size : -1;
}

/* fseeko/ftello, never fseek/ftell: ftell returns long, which cannot carry an
   offset past 2GB on a 32-bit platform and fails with EOVERFLOW there. */
LLint fpsize(FILE *fp) {
  LLint oldpos, size;

  if (!fp)
    return -1;
  oldpos = ftello(fp);
  fseeko(fp, 0, SEEK_END);
  size = ftello(fp);
  fseeko(fp, oldpos, SEEK_SET);
  return size;
}

int multipleStringMatch(const char *s, const char *match) {
  int ret = 0;
  String name = STRING_EMPTY;

  if (match == NULL || s == NULL || *s == 0)
    return 0;
  for (; *match != 0; match++) {
    StringClear(name);
    for (; *match != 0 && *match != '\n'; match++) {
      StringAddchar(name, *match);
    }
    if (StringLength(name) > 0 && strstr(s, StringBuff(name)) != NULL) {
      ret = 1;
      break;
    }
  }
  StringFree(name);
  return ret;
}
