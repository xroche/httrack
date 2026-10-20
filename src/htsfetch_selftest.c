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
  hts_fetch_tag ft;
  const char *tag = NULL, *attr;
  char needle[64];
  int page = HTS_FETCH_DEST_DOCUMENT;

  (void) opt;
  if (argc < 2 || argc > 4 || strlen(argv[1]) + 2 > sizeof(needle)) {
    fprintf(stderr, "fetchdest: takes <tag> <attr> [fil [page]]\n");
    return 1;
  }
  if (argc == 4 && !st_dest_of(argv[3], &page))
    return 1;
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
  memset(&ft, 0, sizeof(ft));
  hts_fetch_classify(&fm, &ft, tag, attr, argc >= 3 ? argv[2] : "",
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

/* -#test=fetchbasetarget <html>: does the page carry a <base target>? */
static int st_fetchbasetarget(httrackp *opt, int argc, char **argv) {
  (void) opt;
  if (argc != 1) {
    fprintf(stderr, "fetchbasetarget: takes <html>\n");
    return 1;
  }
  printf("%s\n", hts_fetch_has_base_target(argv[0]) ? "yes" : "no");
  return 0;
}

/* -#test=fetchscale: an unclosed tag repeated, and one tag of many links,
   scan in linear time. A quadratic scan runs into the test's time guard. */
static int st_fetchscale(httrackp *opt, int argc, char **argv) {
  static const char base[] = "<base a ", href[] = "href=a ";
  const size_t count = 150000;
  char *page = malloct(count * (sizeof(base) - 1) + 3);
  hts_fetch_meta fm;
  hts_fetch_tag ft;
  hts_boolean found;
  size_t i;

  (void) opt;
  (void) argv;
  if (argc != 0 || page == NULL)
    return 1;
  for (i = 0; i < count; i++)
    memcpy(page + i * (sizeof(base) - 1), base, sizeof(base) - 1);
  page[count * (sizeof(base) - 1)] = '\0';
  found = hts_fetch_has_base_target(page);
  memcpy(page, "a ", 2);
  for (i = 0; i < count; i++)
    memcpy(page + 2 + i * (sizeof(href) - 1), href, sizeof(href) - 1);
  page[2 + count * (sizeof(href) - 1)] = '\0';
  memset(&ft, 0, sizeof(ft));
  for (i = 0; i < count; i++)
    hts_fetch_classify(&fm, &ft, page, page + 2 + i * (sizeof(href) - 1), "",
                       HTS_FETCH_DEST_DOCUMENT);
  printf("%s %s\n", found ? "base" : "nobase", hts_fetch_dest_name(fm.dest));
  freet(page);
  return 0;
}

/* -#test=fetchorigin <adr|hex:..> prints the Origin of adr, or "(refused)". */
static int st_fetchorigin(httrackp *opt, int argc, char **argv) {
  char adr[HTS_URLMAXSIZE], origin[HTS_URLMAXSIZE * 2];

  (void) opt;
  if (argc != 1) {
    fprintf(stderr, "fetchorigin: takes an address\n");
    return 1;
  }
  st_decode_body(argv[0], adr, sizeof(adr));
  printf("%s\n",
         hts_fetch_origin(adr, origin, sizeof(origin)) ? origin : "(refused)");
  return 0;
}

/* -#test=acceptkind <arg> prints the kind an --accept argument names. */
static int st_acceptkind(httrackp *opt, int argc, char **argv) {
  int kind;

  (void) opt;
  if (argc != 1) {
    fprintf(stderr, "acceptkind: takes an --accept argument\n");
    return 1;
  }
  kind = hts_accept_kind_of(argv[0]);
  printf("%s\n", kind >= 0 ? hts_accept_kinds()[kind] : "default");
  return 0;
}

/* -#test=accept <dest> <none|chrome|firefox> <-%a|-> [kind=value...] prints
   the Accept a request to dest sends. "-" means no -%a was given. */
static int st_accept(httrackp *opt, int argc, char **argv) {
  char list[1024], buf[HTS_ACCEPT_MAXSIZE];
  const char *v;
  hts_browser_family family;
  int dest, i;

  (void) opt;
  if (argc < 3) {
    fprintf(stderr, "accept: takes <dest> <family> <-%%a|-> [kind=value...]\n");
    return 1;
  }
  if (!st_dest_of(argv[0], &dest) || !st_family_of(argv[1], &family))
    return 1;
  list[0] = '\0';
  for (i = 3; i < argc; i++) {
    if (list[0] != '\0')
      strlcatbuff(list, "\n", sizeof(list));
    strlcatbuff(list, argv[i], sizeof(list));
  }
  v = hts_accept_lookup(
      list, strcmp(argv[2], "-") == 0 ? HTS_ACCEPT_LEGACY : argv[2],
      strcmp(argv[2], "-") != 0, dest, family, buf, sizeof(buf));
  printf("%s\n", v == NULL                           ? "(null)"
                 : *v == '\0'                        ? "(empty)"
                 : strcmp(v, HTS_ACCEPT_LEGACY) == 0 ? "legacy"
                                                     : v);
  return 0;
}

/* -#test=acceptpreset [browser [kind]] prints hts_accept_preset(), or with no
   argument the kind list. */
static int st_acceptpreset(httrackp *opt, int argc, char **argv) {
  const char *v;

  (void) opt;
  if (argc == 0) {
    const char *const *k;
    int dest;

    /* Each kind must stay a Sec-Fetch-Dest name, so a renamed dest fails. */
    for (k = hts_accept_kinds(); *k != NULL; k++) {
      if (!st_dest_of(*k, &dest) || dest == HTS_FETCH_DEST_UNKNOWN)
        return 1;
      printf("%s%s", k != hts_accept_kinds() ? "," : "", *k);
    }
    printf("\n");
    return 0;
  }
  if (argc > 2) {
    fprintf(stderr, "acceptpreset: takes [browser [kind]]\n");
    return 1;
  }
  v = hts_accept_preset(argv[0], argc == 2 ? argv[1] : NULL);
  printf("%s\n", v != NULL ? v : "(null)");
  return 0;
}

const struct selftest_entry selftests_fetch[] = {
    {"fetchdest", "<tag> <attr> [fil]",
     "Sec-Fetch-Dest and -Mode for a reference", st_fetchdest},
    {"fetchsite", "<from> <to>", "Sec-Fetch-Site between two URLs",
     st_fetchsite},
    {"fetchtrust", "<adr>", "is Sec-Fetch-* sent to this host", st_fetchtrust},
    {"fetchbasetarget", "<html>", "does a page carry a <base target>",
     st_fetchbasetarget},
    {"fetchscale", "", "fetch metadata scans in linear time", st_fetchscale},
    {"fetchorigin", "<adr|hex:..>", "the Origin value of an address",
     st_fetchorigin},
    {"acceptkind", "<arg>", "the kind an --accept argument names",
     st_acceptkind},
    {"accept", "<dest> <family> <-%a|-> [kind=value...]",
     "the Accept a request sends", st_accept},
    {"acceptpreset", "[browser [kind]]", "the Accept a browser sends",
     st_acceptpreset},
    {NULL, NULL, NULL, NULL},
};
