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

/* Fuzz the MIME magic-byte check (htssniff.c). The input is the claimed MIME,
   a NUL, then the body the server sent. */
#include "fuzz.h"

#include "htsglobal.h"
#include "htssniff.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  const uint8_t *const nul = memchr(data, '\0', size);
  const char *const mime = (const char *) data;
  size_t skip;

  if (nul == NULL)
    return 0;
  skip = (size_t) (nul - data) + 1;
  (void) hts_sniff_mime_known(mime);
  (void) hts_sniff_mime_consistent(
      hts_span_at((const char *) data + skip, size - skip), mime);
  return 0;
}
