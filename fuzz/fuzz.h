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

/* Shared helpers for the libFuzzer harnesses. */
#ifndef FUZZ_H
#define FUZZ_H

#define HTS_INTERNAL_BYTECODE

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "htsbase.h"

/* Heap NUL-terminated copy of the fuzzer input, so ASan bounds every read. */
static char *fuzz_strdup(const uint8_t *data, size_t size) {
  char *s = malloct(size + 1);

  memcpy(s, data, size);
  s[size] = '\0';
  return s;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

#endif
