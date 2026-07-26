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

/* Fuzz the sitemap <loc> scanner (htssitemap.c): raw XML, gzip-framed bodies
   and truncated streams all arrive here straight off the network. */
#include "fuzz.h"
#include "htssitemap.h"

static hts_boolean sm_count(void *arg, const char *url) {
  int *const n = (int *) arg;

  (void) url;
  (*n)++;
  return HTS_TRUE;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static const int caps[] = {0, 1, 16, HTS_SITEMAP_MAX_URLS_DOC};
  hts_boolean is_index;
  char *body;
  int n = 0, cap;

  if (size == 0)
    return 0;
  cap = caps[data[0] % (sizeof(caps) / sizeof(caps[0]))];
  data++, size--;
  /* A heap copy of exactly `size` bytes: the scanner must never rely on a
     terminator, and ASan turns any overread into a report. */
  body = malloct(size != 0 ? size : 1);
  memcpy(body, data, size);

  (void) hts_sitemap_scan(body, size, cap, &is_index, sm_count, &n);

  freet(body);
  return 0;
}
