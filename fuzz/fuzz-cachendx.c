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

/* Fuzz the legacy .ndx/.dat cache reader, which is proxytrack's (store.c) and
   is now the only one left: #1551 moved -#C off the .ndx onto the ZIP index,
   and htscache.c's cache_brstr/cache_binput have had no caller since. The
   loader walks length-prefixed index entries and seeks the .dat on offsets read
   out of them, so both files come from the input:

     bytes 0..1   big-endian length of the .ndx part
     then         the .ndx, then the rest as the .dat

   PT_SaveCache() then reads every entry the loader indexed back out. */
#include "fuzz.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "coucal.h"
#include "proxy/store.h"

/* A cache pair past this size says nothing a smaller one cannot. */
#define FUZZ_NDX_MAXSIZE (1024 * 1024)

static char ndx_dir[256];
static char ndx_in[sizeof(ndx_dir) + sizeof("/in.ndx")];
static char ndx_dat[sizeof(ndx_dir) + sizeof("/in.dat")];
static char ndx_out[sizeof(ndx_dir) + sizeof("/out.arc")];

static void fuzz_ndx_cleanup(void) {
  (void) unlink(ndx_in);
  (void) unlink(ndx_dat);
  (void) unlink(ndx_out);
  (void) rmdir(ndx_dir);
}

/* proxytrack's main() installs one; without it coucal logs stats per free */
static void fuzz_ndx_coucal_log(coucal_opaque arg, coucal_loglevel level,
                                const char *format, va_list args) {
  (void) arg;
  (void) level;
  (void) format;
  (void) args;
}

/* PT_GetType() picks the format from the extension, so the names decide it */
static int fuzz_ndx_setup(void) {
  if (ndx_in[0] == '\0') {
    const char *const tmp = getenv("TMPDIR");

    coucal_set_global_assert_handler(fuzz_ndx_coucal_log, NULL);

    snprintf(ndx_dir, sizeof(ndx_dir), "%s/fuzz-cachendx-XXXXXX",
             tmp != NULL && *tmp != '\0' ? tmp : "/tmp");
    if (mkdtemp(ndx_dir) == NULL) {
      return -1;
    }
    snprintf(ndx_in, sizeof(ndx_in), "%s/in.ndx", ndx_dir);
    snprintf(ndx_dat, sizeof(ndx_dat), "%s/in.dat", ndx_dir);
    snprintf(ndx_out, sizeof(ndx_out), "%s/out.arc", ndx_dir);
    atexit(fuzz_ndx_cleanup);
  }
  return 0;
}

static hts_boolean fuzz_ndx_write(const char *path, const uint8_t *data,
                                  size_t size) {
  FILE *const fp = fopen(path, "wb");
  hts_boolean written;

  if (fp == NULL) {
    return HTS_FALSE;
  }
  written = fwrite(data, 1, size, fp) == size ? HTS_TRUE : HTS_FALSE;
  if (fclose(fp) != 0) {
    return HTS_FALSE;
  }
  return written;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  PT_Indexes indexes;
  size_t ndxSize;

  if (size < 2 || size > FUZZ_NDX_MAXSIZE || fuzz_ndx_setup() != 0) {
    return 0;
  }
  ndxSize = ((size_t) data[0] << 8) | data[1];
  data += 2, size -= 2;
  if (ndxSize > size) {
    ndxSize = size;
  }
  /* the loader wants both files present, so an empty .dat is still written */
  if (!fuzz_ndx_write(ndx_in, data, ndxSize) ||
      !fuzz_ndx_write(ndx_dat, data + ndxSize, size - ndxSize)) {
    return 0;
  }

  indexes = PT_New();
  if (indexes != NULL) {
    if (PT_AddIndex(indexes, ndx_in) > 0) {
      /* the writer reads every indexed entry back through the .dat */
      (void) PT_SaveCache(indexes, ndx_out);
    }
    PT_Delete(indexes);
  }
  return 0;
}
