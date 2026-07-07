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

/* Fuzz the URL splitter and path normalizer (htslib.c): ident_url_absolute
   is the first parser to touch a raw URL; fil_simplifie collapses ./ and ../
   in place. */
#include "fuzz.h"
#include "htscore.h"
#include "htslib.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  char *s = fuzz_strdup(data, size);
  lien_adrfil *af = calloct(1, sizeof(*af));
  /* fil_simplifie rewrites in place and may grow an empty path to "./" */
  char *path = malloct(size + 3);

  (void) ident_url_absolute(s, af);
  memcpy(path, s, size + 1);
  fil_simplifie(path);

  freet(path);
  freet(af);
  freet(s);
  return 0;
}
