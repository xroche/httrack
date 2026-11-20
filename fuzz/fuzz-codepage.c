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

/* Fuzz hts_codepageToUTF8() and the htscodepages.h tables, which iconv builds
   skip, so this links its own htscharset.c forced onto the tables. */
#include "fuzz.h"
#include "htscharset.h"

/* Aliases must resolve to their name's table, and the last two to none. */
static const char *const labels[] = {
    "iso88591", "iso885915", "cp1252",    "windows-1252", "windows1252",
    "IBM850",   "cp850",     "koi8r",     "koi8-r",       "cp037",
    "cp864",    "cp1256",    "iso885911", "utf-8",        "no-such-charset",
};

/* This selector takes the label from the input instead of labels[]. */
#define FUZZ_LABEL_FROM_INPUT 0xff

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  char *label = NULL;
  const char *charset;
  char *s;

  if (size == 0)
    return 0;
  if (data[0] == FUZZ_LABEL_FROM_INPUT) {
    /* The label ends at the first NUL, and any payload follows it */
    const uint8_t *const nul = memchr(data + 1, '\0', size - 1);
    const size_t labelLen =
        nul != NULL ? (size_t) (nul - (data + 1)) : size - 1;

    label = fuzz_strdup(data + 1, labelLen);
    charset = label;
    data += 1 + labelLen + (nul != NULL ? 1 : 0);
    size -= 1 + labelLen + (nul != NULL ? 1 : 0);
  } else {
    charset = labels[data[0] % (sizeof(labels) / sizeof(labels[0]))];
    data++, size--;
  }
  s = fuzz_strdup(data, size);

  /* the decoder is only reached for a non-ASCII string on a non-UTF-8 label */
  {
    char *utf8 = hts_convertStringToUTF8(s, size, charset);
    char *enc = hts_convertStringFromUTF8(s, size, charset);

    freet(utf8);
    freet(enc);
  }

  freet(s);
  freet(label);
  return 0;
}
