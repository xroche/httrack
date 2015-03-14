
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998-2015 Xavier Roche and other contributors

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

Important notes:

- We hereby ask people using this source NOT to use it in purpose of grabbing
emails addresses, or collecting any other private information on persons.
This would disgrace our work, and spoil the many hours we spent on it.

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

/* Internal engine bytecode */
#define HTS_INTERNAL_BYTECODE

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "htsmd5.h"
#include "md5.h"
#include "htssafe.h"

int domd5mem(const char *buf, size_t len, char *digest, int asAscii) {
  int endian = 1;
  unsigned char bindigest[16];
  struct MD5Context ctx;

  MD5Init(&ctx, *((char *) &endian));
  MD5Update(&ctx, (const unsigned char *) buf, (unsigned int) len);
  MD5Final(bindigest, &ctx);

  if (!asAscii) {
    memcpy(digest, bindigest, 16);
  } else {
    sprintf(digest,
            "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"
            "%02x%02x%02x%02x%02x", bindigest[0], bindigest[1], bindigest[2],
            bindigest[3], bindigest[4], bindigest[5], bindigest[6],
            bindigest[7], bindigest[8], bindigest[9], bindigest[10],
            bindigest[11], bindigest[12], bindigest[13], bindigest[14],
            bindigest[15]);
  }

  return 0;
}

unsigned long int md5sum32(const char *buff) {
  union {
    char md5digest[16];
    unsigned long int hash;
  } u;

  domd5mem(buff, strlen(buff), u.md5digest, 0);
  return u.hash;
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
