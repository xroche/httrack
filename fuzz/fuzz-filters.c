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

/* Fuzz the wildcard filter matcher (htsfilters.c; #148 bracket-range OOB was
   here). Input splits on the first NUL: pattern, then subject string. */
#include "fuzz.h"
#include "htsfilters.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  char *buf = fuzz_strdup(data, size);
  const char *joker = buf;
  const uint8_t *sep = memchr(data, '\0', size);
  /* subject in its own allocation so ASan bounds it apart from the pattern */
  char *nom = sep != NULL ? fuzz_strdup(sep + 1, (data + size) - (sep + 1))
                          : fuzz_strdup(data + size, 0);

  (void) strjoker(nom, joker, NULL, NULL);
  {
    LLint sz = (LLint) size;
    int size_flag = 0;

    (void) strjoker(nom, joker, &sz, &size_flag);
  }
  (void) strjokerfind(nom, joker);
  {
    char *filter = malloct(strlen(joker) + 2);
    char *filters[1];
    LLint sz = (LLint) size;
    int size_flag = 0, depth = 0;

    filter[0] = '-';
    memcpy(filter + 1, joker, strlen(joker) + 1);
    filters[0] = filter;
    (void) fa_strjoker(0, filters, 1, nom, &sz, &size_flag, &depth);
    freet(filter);
  }

  freet(nom);
  freet(buf);
  return 0;
}
