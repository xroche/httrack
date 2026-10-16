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
/* File: htsfetch_selftest.c subroutines:                       */
/*       self-tests for the Sec-Fetch-* values                  */
/* ------------------------------------------------------------ */

#include "htsselftest_int.h"

#include "htsfetch.h"

/* -#test=fetchdest <tag> <attr> [fil [page]]: what a browser fetches for the
   attribute attr of the start tag <tag ...>, or, with tag "-", for a
   stylesheet reference to fil, in a page fetched as dest page (document). */
static int st_fetchdest(httrackp *opt, int argc, char **argv) {
  hts_fetch_meta fm;
  const char *tag = NULL, *attr;
  char needle[64];
  int page = HTS_FETCH_DEST_DOCUMENT;

  (void) opt;
  if (argc < 2 || argc > 4 || strlen(argv[1]) + 2 > sizeof(needle)) {
    fprintf(stderr, "fetchdest: takes <tag> <attr> [fil [page]]\n");
    return 1;
  }
  if (argc == 4) {
    for (page = HTS_FETCH_DEST_VIDEO;
         page > 0 && strcmp(hts_fetch_dest_name(page), argv[3]) != 0; page--)
      ;
  }
  attr = argv[1];
  if (strcmp(argv[0], "-") != 0) {
    tag = argv[0];
    snprintf(needle, sizeof(needle), " %s", argv[1]);
    attr = strstr(tag, needle);
    if (attr == NULL) {
      fprintf(stderr, "fetchdest: no %s in the tag\n", argv[1]);
      return 1;
    }
    attr++;
  }
  memset(&fm, 0, sizeof(fm));
  hts_fetch_classify(&fm, tag, attr, argc >= 3 ? argv[2] : "",
                     hts_fetch_same_frame(page));
  if (fm.dest == HTS_FETCH_DEST_UNKNOWN)
    printf("none\n");
  else
    printf("dest=%s mode=%s user=%s\n", hts_fetch_dest_name(fm.dest),
           hts_fetch_mode_name(&fm), fm.user ? "?1" : "-");
  return 0;
}

/* -#test=fetchsite <from> <to>: Sec-Fetch-Site for a request to <to> from a
   page at <from>. */
static int st_fetchsite(httrackp *opt, int argc, char **argv) {
  (void) opt;
  if (argc != 2) {
    fprintf(stderr, "fetchsite: takes <from> <to>\n");
    return 1;
  }
  printf("%s\n", hts_fetch_site_name(hts_fetch_site_of(argv[0], argv[1])));
  return 0;
}

/* -#test=fetchtrust <adr>: does a browser send Sec-Fetch-* to this host? */
static int st_fetchtrust(httrackp *opt, int argc, char **argv) {
  (void) opt;
  if (argc != 1) {
    fprintf(stderr, "fetchtrust: takes <adr>\n");
    return 1;
  }
  printf("%s\n", hts_fetch_trustworthy(argv[0]) ? "yes" : "no");
  return 0;
}

const struct selftest_entry selftests_fetch[] = {
    {"fetchdest", "<tag> <attr> [fil]",
     "Sec-Fetch-Dest and -Mode for a reference", st_fetchdest},
    {"fetchsite", "<from> <to>", "Sec-Fetch-Site between two URLs",
     st_fetchsite},
    {"fetchtrust", "<adr>", "is Sec-Fetch-* sent to this host", st_fetchtrust},
    {NULL, NULL, NULL, NULL},
};
