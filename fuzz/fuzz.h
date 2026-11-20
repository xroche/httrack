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

/* Shared helpers for the libFuzzer harnesses. */
#ifndef FUZZ_H
#define FUZZ_H

#define HTS_INTERNAL_BYTECODE

#include <dirent.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "htsbase.h"

/* Heap NUL-terminated copy of the fuzzer input, so ASan bounds every read. */
HTS_UNUSED static char *fuzz_strdup(const uint8_t *data, size_t size) {
  char *s = malloct(size + 1);

  memcpy(s, data, size);
  s[size] = '\0';
  return s;
}

/* Copy the next NUL-separated field into dst (bounded). Returns HTS_FALSE at
   the end of the input. */
HTS_UNUSED static hts_boolean fuzz_field(const uint8_t **data, size_t *size,
                                         char *dst, size_t dst_size) {
  const uint8_t *nul;
  size_t len;

  if (*size == 0)
    return HTS_FALSE;
  nul = (const uint8_t *) memchr(*data, 0, *size);
  len = nul != NULL ? (size_t) (nul - *data) : *size;
  if (len >= dst_size)
    len = dst_size - 1;
  memcpy(dst, *data, len);
  dst[len] = '\0';
  *data += len;
  *size -= len;
  if (*size > 0) { /* skip the NUL itself */
    *data += 1;
    *size -= 1;
  }
  return HTS_TRUE;
}

/* Remove everything under dir, keeping dir itself. Unbounded depth so deep
   mirrored paths do not leak across iterations. */
HTS_UNUSED static void fuzz_wipe_dir(const char *dir) {
  DIR *const d = opendir(dir);
  struct dirent *e;

  if (d == NULL)
    return;
  while ((e = readdir(d)) != NULL) {
    char path[4096];
    struct stat st;

    if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
      continue;
    snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
    if (lstat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
      fuzz_wipe_dir(path);
      (void) rmdir(path);
    } else {
      (void) unlink(path);
    }
  }
  closedir(d);
}

/* Create <$TMPDIR or /tmp>/<prefix>-XXXXXX in dir, and run cleanup at exit.
   Returns HTS_FALSE and registers nothing when mkdtemp fails. */
HTS_UNUSED static hts_boolean
fuzz_tmpdir(char *dir, size_t size, const char *prefix, void (*cleanup)(void)) {
  const char *const tmp = getenv("TMPDIR");

  snprintf(dir, size, "%s/%s-XXXXXX",
           tmp != NULL && *tmp != '\0' ? tmp : "/tmp", prefix);
  if (mkdtemp(dir) == NULL)
    return HTS_FALSE;
  atexit(cleanup);
  return HTS_TRUE;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

#endif
