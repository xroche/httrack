/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 2026 Xavier Roche and other contributors

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
/* File: htstools_selftest.c subroutines:                       */
/*       self-tests for the attribute scanner, winprofile.ini   */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htsselftest_int.h"

/* endtoken TAIL: what rech_endtoken() reads as the value at TAIL, which is a
   tag's bytes from just after `attr=`. An empty TAIL arrives as no argument. */
static int st_endtoken(httrackp *opt, int argc, char **argv) {
  const char *const tail = argc >= 1 ? argv[0] : "";
  const char *value = NULL;
  const int len = rech_endtoken(tail, &value);

  (void) opt;
  printf("len=%d value=[%.*s]\n", len, len, value);
  return 0;
}

/* profilecharset DIR: which winprofile.ini files declare UTF-8. */
static int st_profilecharset(httrackp *opt, int argc, char **argv) {
  static const struct {
    const char *content;
    const char *tail; /* without content: an over-long line ending in tail */
    hts_boolean utf8;
  } cases[] = {
      {"Category=x\r\n", NULL, HTS_FALSE},
      {"Category=x\r\nCharset=UTF-8\r\n", NULL, HTS_TRUE},
      {"charset=utf8\n", NULL, HTS_TRUE},
      {"CHARSET=Utf-8\r\n", NULL, HTS_TRUE},
      {"Charset=windows-1252\r\n", NULL, HTS_FALSE},
      {"Charset=\r\n", NULL, HTS_FALSE},
      {"Charset=UTF-8X\r\n", NULL, HTS_FALSE},
      {"Charset=UTF-16\r\n", NULL, HTS_FALSE},
      {"XCharset=UTF-8\r\n", NULL, HTS_FALSE},
      {"Charset=latin1\r\nCharset=UTF-8\r\n", NULL, HTS_FALSE},
      {"Charset:UTF-8\r\n", NULL, HTS_FALSE},
      {NULL, "Charset=UTF-8\r\nCategory=gr\xFCn\r\n", HTS_FALSE},
      {NULL, "x\nCharset=UTF-8\r\n", HTS_TRUE},
  };

  char path[HTS_URLMAXSIZE];
  size_t i;

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "profilecharset: needs a directory\n");
    return 1;
  }
  assertf(!hts_profile_declares_utf8(argv[0])); /* a directory, not a file */
  snprintf(path, sizeof(path), "%s/winprofile.ini", argv[0]);
  assertf(!hts_profile_declares_utf8(path)); /* missing file */
  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    FILE *const fp = fopen(path, "wb");

    assertf(fp != NULL);
    if (cases[i].content != NULL) {
      assertf(fputs(cases[i].content, fp) >= 0);
    } else {
      int k;

      assertf(fputs("Footer=", fp) >= 0);
      for (k = 0; k < 1014; k++)
        assertf(fputc('a', fp) != EOF);
      assertf(fputs(cases[i].tail, fp) >= 0);
    }
    assertf(fclose(fp) == 0);
    if (hts_profile_declares_utf8(path) != cases[i].utf8) {
      fprintf(stderr, "profilecharset: case %u read as %s\n", (unsigned) i,
              cases[i].utf8 ? "legacy" : "UTF-8");
      return 1;
    }
  }
  (void) unlink(path);
  printf("profilecharset self-test OK\n");
  return 0;
}

/* getcategory FILE: print the category hts_getcategory() reads. */
static int st_getcategory(httrackp *opt, int argc, char **argv) {
  char *categ;

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "getcategory: needs a file\n");
    return 1;
  }
  categ = hts_getcategory(argv[0]);
  printf("category=[%s]\n", categ != NULL ? categ : "(none)");
  freet(categ);
  return 0;
}

/* ------------------------------------------------------------ */
/* Registry: this module's tests, in the order -#test lists them. */
/* ------------------------------------------------------------ */

const struct selftest_entry selftests_tools[] = {
    {"endtoken", "<tag bytes after attr=>", "value rech_endtoken() reads",
     st_endtoken},
    {"profilecharset", "<dir>",
     "which winprofile.ini files declare Charset=UTF-8", st_profilecharset},
    {"getcategory", "<file>", "the category hts_getcategory() reads",
     st_getcategory},
    {NULL, NULL, NULL, NULL},
};
