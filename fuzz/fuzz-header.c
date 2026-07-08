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

/* Fuzz the HTTP response-header parser (htslib.c): treatfirstline on the
   status line, treathead on each following header. Both consume raw bytes
   off the wire and copy fields into fixed htsblk buffers; treathead also
   mutates its line in place and drives cookie parsing. */
#include "fuzz.h"
#include "htslib.h"
#include "htsbauth.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  char *buf = fuzz_strdup(data, size);
  htsblk r;
  t_cookie *cookie = calloct(1, sizeof(*cookie));
  char *line = malloct(size + 1);
  char *p = buf;
  int first = 1;

  memset(&r, 0, sizeof(r));
  cookie->max_len = (int) sizeof(cookie->data);
  r.location = malloct(HTS_URLMAXSIZE * 2);
  r.location[0] = '\0';

  /* feed one header line at a time, as the receive loop does */
  while (p != NULL && *p != '\0') {
    char *nl = strchr(p, '\n');
    size_t n = (nl != NULL) ? (size_t) (nl - p) : strlen(p);

    memcpy(line, p, n);
    line[n] = '\0';
    if (first) {
      treatfirstline(&r, line);
      first = 0;
    } else {
      treathead(cookie, "www.example.com", "/", &r, line);
    }
    p = (nl != NULL) ? nl + 1 : NULL;
  }

  freet(r.location);
  freet(line);
  freet(cookie);
  freet(buf);
  return 0;
}
