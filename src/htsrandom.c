/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 2026 Xavier Roche and other contributors

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
/* System CSPRNG. See htsrandom.h.                               */
/* ------------------------------------------------------------ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "htsrandom.h"

#ifdef _WIN32
#include "htswin32.h"
#else
#include <errno.h>
#include <stdio.h>
/* One macro gates both the include and the call: <sys/random.h> is the only
   declaration of getrandom(), so the symbol alone does not make it callable. */
#if defined(HAVE_SYS_RANDOM_H) && defined(HAVE_GETRANDOM)
#define HTS_USE_GETRANDOM
#include <sys/random.h>
#endif
#endif

#ifdef _WIN32
/* RtlGenRandom, resolved at runtime so no import library is needed. */
typedef BOOLEAN(WINAPI *hts_rtlgenrandom_t)(PVOID buffer, ULONG length);
#endif

hts_boolean hts_random_bytes(void *buf, size_t len) {
  if (len == 0) /* nothing to ask any source for, on every platform alike */
    return HTS_TRUE;
#ifdef _WIN32
  hts_boolean ok = HTS_FALSE;
  HMODULE dll = LoadLibraryA("advapi32.dll");

  if (dll != NULL) {
    hts_rtlgenrandom_t gen =
        (hts_rtlgenrandom_t) GetProcAddress(dll, "SystemFunction036");

    /* the ULONG cast must not truncate; callers ask for a few dozen bytes */
    if (gen != NULL && len <= 0x10000 && gen(buf, (ULONG) len)) {
      ok = HTS_TRUE;
    }
    FreeLibrary(dll);
  }
  return ok;
#else
  unsigned char *const p = (unsigned char *) buf;
  size_t got = 0;
  FILE *fp;

#ifdef HTS_USE_GETRANDOM
  /* Breaks the build if the two guards ever split again: an implicit
     declaration would return int, not ssize_t. */
  enum {
    getrandom_is_prototyped =
        1 / (sizeof(getrandom(p, len, 0)) == sizeof(ssize_t))
  };

  while (got < len) {
    const ssize_t n = getrandom(p + got, len - got, 0);

    if (n < 0) {
      if (errno == EINTR)
        continue;
      break; /* pre-3.17 kernel or a seccomp filter: try /dev/urandom */
    }
    got += (size_t) n;
  }
  if (got == len)
    return HTS_TRUE;
#endif
  fp = fopen("/dev/urandom", "rb");
  if (fp == NULL)
    return HTS_FALSE;
  while (got < len) {
    const size_t n = fread(p + got, 1, len - got, fp);

    if (n == 0) /* a short read is a hard failure, not partial credit */
      break;
    got += n;
  }
  fclose(fp);
  return got == len ? HTS_TRUE : HTS_FALSE;
#endif
}
