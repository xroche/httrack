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

/* Fuzz the HTML entity decoder (htsencoding.c). First input byte picks the
   destination size, so truncation bounds get exercised too. */
#include "fuzz.h"
#include "htsencoding.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static const size_t dsizes[] = {1, 2, 8, 64, 4096};
  size_t dsize;
  char *src, *dest;

  if (size == 0)
    return 0;
  dsize = dsizes[data[0] % (sizeof(dsizes) / sizeof(dsizes[0]))];
  data++, size--;
  src = fuzz_strdup(data, size);
  dest = malloct(dsize);

  (void) hts_unescapeEntities(src, dest, dsize);
  (void) hts_unescapeEntitiesWithCharset(src, dest, dsize, "iso-8859-1");

  freet(dest);
  freet(src);
  return 0;
}
