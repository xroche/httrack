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

/* Fuzz the MIME magic-byte consistency check (htssniff.c): a wire MIME and
   a body leading window disagree, hts_sniff_mime_consistent() arbitrates on
   the magic rules. The first input byte picks the claimed MIME; the rest is
   the body the server sent. Every known MIME is then asked about the same
   body, so one input walks the whole rule table. */
#include "fuzz.h"

#include "htsglobal.h"
#include "htssniff.h"

static const char *const mimes[] = {
    "image/gif",
    "image/png",
    "image/jpeg",
    "application/pdf",
    "application/x-gzip",
    "application/zip",
    "audio/mpeg",
    "video/mp4",
    "text/html",
    "text/plain",
    "application/octet-stream",
};

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  const char *mime;
  size_t i;

  if (size == 0)
    return 0;
  mime = mimes[data[0] % (sizeof(mimes) / sizeof(mimes[0]))];
  data++, size--;
  (void) hts_sniff_mime_known(mime);
  (void) hts_sniff_mime_consistent(data, size, mime);
  for (i = 0; i < sizeof(mimes) / sizeof(mimes[0]); i++)
    (void) hts_sniff_mime_consistent(data, size, mimes[i]);
  return 0;
}
