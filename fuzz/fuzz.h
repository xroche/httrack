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

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "htsbase.h"
#include "htsbuf.h"

/* Return the span over the SIZE input bytes at DATA. */
static HTS_INLINE HTS_UNUSED hts_span fuzz_span(const uint8_t *data,
                                                size_t size) {
  return hts_span_at((const char *) data, size);
}

/* Create <$TMPDIR or /tmp>/<prefix>-XXXXXX in dir, and run cleanup at exit.
   Returns HTS_FALSE and registers nothing when mkdtemp fails. */
HTS_UNUSED static hts_boolean fuzz_tmpdir(hts_buf dir, const char *prefix,
                                          void (*cleanup)(void)) {
  const char *const tmp = getenv("TMPDIR");

  hts_buf_printf_clip(dir, "%s/%s-XXXXXX",
                      tmp != NULL && *tmp != '\0' ? tmp : "/tmp", prefix);
  if (mkdtemp(dir.p) == NULL)
    return HTS_FALSE;
  atexit(cleanup);
  return HTS_TRUE;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

#endif
