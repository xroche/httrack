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

/* Fuzz the charset codecs: hts_convertStringToUTF8/FromUTF8 and the
   UTF-8/UCS4 primitives (htscharset.c). First input byte picks the charset. */
#include "fuzz.h"
#include "htscharset.h"

static const char *const charsets[] = {
    "utf-8",    "iso-8859-1", "iso-8859-2", "iso-8859-15", "windows-1252",
    "us-ascii", "shift_jis",  "euc-jp",     "iso-2022-jp", "gb2312",
    "big5",     "euc-kr",     "koi8-r",     "utf-16",      "unknown-charset",
};

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  const char *charset;
  char *s;

  if (size == 0)
    return 0;
  charset = charsets[data[0] % (sizeof(charsets) / sizeof(charsets[0]))];
  data++, size--;
  s = fuzz_strdup(data, size);

  {
    char *utf8 = hts_convertStringToUTF8(s, size, charset);
    freet(utf8);
  }
  {
    char *enc = hts_convertStringFromUTF8(s, size, charset);
    freet(enc);
  }
  {
    size_t nChars = 0;
    hts_UCS4 *ucs = hts_convertUTF8StringToUCS4(s, size, &nChars);

    if (ucs != NULL) {
      char *back = hts_convertUCS4StringToUTF8(ucs, nChars);
      freet(back);
      freet(ucs);
    }
  }
  {
    size_t i = 0;

    while (i < size) {
      hts_UCS4 uc = 0;
      const size_t nr = hts_readUTF8(s + i, size - i, &uc);
      char out[8];

      if (nr == 0)
        break;
      hts_writeUTF8(uc, out, sizeof(out));
      i += nr;
    }
  }

  freet(s);
  return 0;
}
