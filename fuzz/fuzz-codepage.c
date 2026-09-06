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

/* Fuzz httrack's own codepage decoder: hts_codepageToUTF8() and the
   htscodepages.h tables. A build with iconv compiles none of that (htscharset.c
   hides it behind DISABLE_ICONV), yet it is what Windows and every iconv-less
   distro ship, so this harness links its own copy of htscharset.c forced onto
   the tables. fuzz-charset covers whichever path the build itself chose.

   The label reaches hts_equalsCodepageName(), which matches a table name
   loosely, so that "windows-1252" and "IBM850" name cp1252 and cp850. The
   input picks either a fixed label or one made of its own bytes. */
#include "fuzz.h"
#include "htscharset.h"

/* Table names, the IANA and IBM spellings that must resolve to the same table,
   and two that must resolve to none. */
static const char *const labels[] = {
    "iso88591", "iso885915", "cp1252",    "windows-1252", "windows1252",
    "IBM850",   "cp850",     "koi8r",     "koi8-r",       "cp037",
    "cp864",    "cp1256",    "iso885911", "utf-8",        "no-such-charset",
};

/* Selector asking for a label out of the input rather than out of labels[]. */
#define FUZZ_LABEL_FROM_INPUT 0xff

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  char *label = NULL;
  const char *charset;
  char *s;

  if (size == 0)
    return 0;
  if (data[0] == FUZZ_LABEL_FROM_INPUT) {
    /* label up to the first NUL, payload after it; no NUL means no payload */
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
