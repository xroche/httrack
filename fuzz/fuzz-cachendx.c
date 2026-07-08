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

/* Fuzz the cache-index (.ndx) parser: a corrupt or truncated index must not
   walk the length-prefixed cache_brstr/cache_binput scan past the buffer.
   Mirrors the loader in cache_readex_new (htscache.c). */
#include "fuzz.h"
#include "htscache.h"
#include "htslib.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  char *buf = fuzz_strdup(data, size);
  const char *const end = buf + size;
  char firstline[256];
  char *a = buf;

  /* header: two length-prefixed fields (version, last-modified) */
  a += cache_brstr(a, firstline, sizeof(firstline));
  a += cache_brstr(a, firstline, sizeof(firstline));

  /* body: newline-delimited host/file/position triples. The coucal insert
     the real loader ends with is a separate library's concern; this harness
     targets the length-prefixed scan that must stay inside the buffer. */
  while (a != NULL && a < end) {
    char BIGSTK line[HTS_URLMAXSIZE * 2];
    char linepos[256];
    int pos;

    a = strchr(a + 1, '\n');
    if (a == NULL)
      break;
    a++;
    a += cache_binput(a, end, line, HTS_URLMAXSIZE);
    a += cache_binput(a, end, line + strlen(line), HTS_URLMAXSIZE);
    a += cache_binput(a, end, linepos, 200);
    sscanf(linepos, "%d", &pos);
    (void) pos;
  }

  freet(buf);
  return 0;
}
