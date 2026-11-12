
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

/* ------------------------------------------------------------ */
/* File: htsmd5.c subroutines:                                  */
/*       generate a md5 hash                                    */
/*                                                              */
/* Written March 1993 by Branko Lankester                       */
/* Modified June 1993 by Colin Plumb for altered md5.c.         */
/* Modified October 1995 by Erik Troan for RPM                  */
/* Modified 2000 by Xavier Roche for domd5mem                   */
/* ------------------------------------------------------------ */

#ifndef HTS_INTERNAL_BYTECODE /* proxytrack and htsweb.c lack the flag */
#define HTS_INTERNAL_BYTECODE
#endif
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "htsmd5.h"
#include "md5.h"
#include "htssafe.h"

void hts_md5_init(struct MD5Context *ctx) {
  const int one = 1;
  const int little_endian = *(const char *) &one;

  /* brokenEndian=1 skips MD5's byte swap, right on a little-endian host */
  MD5Init(ctx, little_endian);
}

void hts_md5_hex(const unsigned char bindigest[16], char hex[33]) {
  static const char digits[] = "0123456789abcdef";
  int i;

  for (i = 0; i < 16; i++) {
    hex[2 * i] = digits[bindigest[i] >> 4];
    hex[2 * i + 1] = digits[bindigest[i] & 0x0f];
  }
  hex[32] = '\0';
}

int domd5mem(const char *buf, size_t len, char *digest, int asAscii) {
  unsigned char bindigest[16];
  struct MD5Context ctx;

  hts_md5_init(&ctx);
  MD5Update(&ctx, (const unsigned char *) buf, (unsigned int) len);
  MD5Final(bindigest, &ctx);

  if (!asAscii) {
    memcpy(digest, bindigest, 16);
  } else {
    hts_md5_hex(bindigest, digest);
  }

  return 0;
}

void md5selftest(void) {
  static const char str1[] = "The quick brown fox jumps over the lazy dog\n";
  static const char str1m[] = "37c4b87edffc5d198ff5a185cee7ee09";
  static const char str2[] = "Hello";
  static const char str2m[] = "8b1a9953c4611296a827abf8c47804d7";
  char digest[64];
#define MDCHECK(VAR, VARMD) do { \
  memset(digest, 0xCC, sizeof(digest)); \
  domd5mem(VAR, sizeof(VAR) - 1, digest, 1); \
  if (strcmp(digest, VARMD) != 0) { \
    fprintf(stderr, \
            "error: md5 selftest failed: '%s' => '%s' (!= '%s')\n", \
            VAR, digest, VARMD); \
    assert(! "md5 selftest failed"); \
  } \
} while(0)
MDCHECK(str1, str1m);
MDCHECK(str2, str2m);
#undef MDCHECK
  fprintf(stderr, "md5 selftest succeeded\n");
}
