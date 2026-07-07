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

/* Fuzz the URL percent-decoders (htslib.c). First input byte picks the
   output buffer size, so the bounded-copy contract is exercised. */
#include "fuzz.h"
#include "httrack-library.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static const size_t bsizes[] = {1, 2, 16, 256, 8192};
  size_t bsize;
  char *s, *catbuff;

  if (size == 0)
    return 0;
  bsize = bsizes[data[0] % (sizeof(bsizes) / sizeof(bsizes[0]))];
  data++, size--;
  s = fuzz_strdup(data, size);
  catbuff = malloct(bsize);

  (void) unescape_http(catbuff, bsize, s);
  (void) unescape_http_unharm(catbuff, bsize, s, 0);
  (void) unescape_http_unharm(catbuff, bsize, s, 1);
  unescape_amp(s);

  freet(catbuff);
  freet(s);
  return 0;
}
