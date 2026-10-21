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
/* File: htsbundle_selftest.c subroutines:                      */
/*       self-tests for the login bundles                       */
/* ------------------------------------------------------------ */

#include "htsselftest_int.h"

#include "htsbundle.h"

/* -#test=bundlecut <url>: the bundle a start URL names, and the URL left. */
static int st_bundlecut(httrackp *opt, int argc, char **argv) {
  char url[HTS_URLMAXSIZE * 2];
  char name[HTS_BUNDLE_NAME_MAX];

  (void) opt;
  if (argc != 1 || strlen(argv[0]) >= sizeof(url)) {
    fprintf(stderr, "bundlecut: takes <url>\n");
    return 1;
  }
  strcpybuff(url, argv[0]);
  switch (hts_bundle_cut(url, name, sizeof(name))) {
  case HTS_BUNDLE_NONE:
    printf("none\n");
    break;
  case HTS_BUNDLE_BAD:
    printf("bad\n");
    break;
  case HTS_BUNDLE_NAMED:
    printf("%s %s\n", name, url);
    break;
  }
  return 0;
}

/* -#test=bundle <dir> <url>... [host:<host>]...: load the bundles the URLs
   name from dir, print them, then the UA and headers each host gets. */
static int st_bundle(httrackp *opt, int argc, char **argv) {
  FILE *const log = opt->log;
  char err[1024];
  int i;

  if (argc < 2) {
    fprintf(stderr, "bundle: takes <dir> <url>... [host:<host>]...\n");
    return 1;
  }
  hts_bundle_reset(opt);
  assertf(hts_bundle_set_dir(opt, argv[0]));
  opt->log = stderr; /* warnings stay out of the graded output */
  for (i = 1; i < argc; i++) {
    char url[HTS_URLMAXSIZE * 2];
    char name[HTS_BUNDLE_NAME_MAX];

    if (strncmp(argv[i], "host:", 5) == 0 || strlen(argv[i]) >= sizeof(url))
      continue;
    strcpybuff(url, argv[i]);
    if (hts_bundle_cut(url, name, sizeof(name)) == HTS_BUNDLE_NAMED)
      assertf(hts_bundle_name(opt, name, url));
  }
  if (!hts_bundle_load_all(opt, err, sizeof(err))) {
    /* the folder is the caller's, so the message is graded without it */
    const size_t dlen = strlen(argv[0]);
    const char *p = err;
    const char *at;

    printf("refused: ");
    while ((at = strstr(p, argv[0])) != NULL) {
      printf("%.*s", (int) (at - p), p);
      p = at + dlen;
    }
    printf("%s\n", p);
    hts_bundle_reset(opt);
    opt->log = log;
    return 0;
  }
  hts_bundle_dump(opt);
  for (i = 1; i < argc; i++) {
    if (strncmp(argv[i], "host:", 5) == 0) {
      const char *ua = "global";
      const char *headers = "global";
      const char *p;

      (void) hts_bundle_identity(opt, argv[i] + 5, &ua, &headers);
      printf("%s ua=%s headers=", argv[i] + 5, ua);
      /* one line per host, so the block's line ends print as '|' */
      for (p = headers; *p != '\0'; p++) {
        if (*p != '\r')
          putchar(*p == '\n' ? '|' : *p);
      }
      putchar('\n');
    }
  }
  hts_bundle_reset(opt);
  opt->log = log;
  return 0;
}

const struct selftest_entry selftests_bundle[] = {
    {"bundlecut", "<url>", "the login bundle a start URL names", st_bundlecut},
    {"bundle", "<dir> <url>... [host:<host>]...",
     "load login bundles and print the identity each host gets", st_bundle},
    {NULL, NULL, NULL, NULL},
};
