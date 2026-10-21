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

/* -#test=bundle <dir> <url>... [host:<adr>]... [jar:<file>]: load the bundles
   the URLs name from dir over the jar file, print them and the jar, then the
   UA and headers each adr gets. */
static int st_bundle(httrackp *opt, int argc, char **argv) {
  FILE *const log = opt->log;
  t_cookie *const prev = opt->cookie;
  t_cookie *jar = calloct(1, sizeof(t_cookie));
  const char *jar_file = NULL;
  char err[1024];
  char *rec;
  int i;

  if (argc < 2 || jar == NULL) {
    fprintf(stderr, "bundle: takes <dir> <url>... [host:<adr>]... "
                    "[jar:<file>]\n");
    freet(jar);
    return 1;
  }
  jar->max_len = 30000; /* httpmirror's */
  opt->log = stderr;    /* warnings stay out of the graded output */
  opt->cookie = jar;
  hts_bundle_free(opt);
  StringCopy(opt->bundle_dir, argv[0]);
  for (i = 1; i < argc; i++) {
    char url[HTS_URLMAXSIZE * 2];
    char name[HTS_BUNDLE_NAME_MAX];

    if (strncmp(argv[i], "jar:", 4) == 0) {
      jar_file = argv[i] + 4;
      (void) cookie_load(opt, jar, "", jar_file);
    } else if (strncmp(argv[i], "host:", 5) != 0 &&
               strlen(argv[i]) < sizeof(url)) {
      strcpybuff(url, argv[i]);
      if (hts_bundle_cut(url, name, sizeof(name)) == HTS_BUNDLE_NAMED)
        assertf(hts_bundle_add(opt, name, url));
    }
  }
  if (!hts_bundle_load_all(opt, jar_file, err, sizeof(err))) {
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
    goto done;
  }
  hts_bundle_dump(opt);
  for (rec = jar->data; *rec != '\0'; rec = cookie_nextfield(rec)) {
    char dom[8192], cname[8192], value[8192];

    printf("cookie %s %s=%s\n", cookie_get(dom, rec, 0),
           cookie_get(cname, rec, 5), cookie_get(value, rec, 6));
  }
  for (i = 1; i < argc; i++) {
    if (strncmp(argv[i], "host:", 5) == 0) {
      htsrequest req;
      const char *p;

      memset(&req, 0, sizeof(req));
      req.user_agent = req.headers = "global";
      hts_bundle_identity(opt, argv[i] + 5, &req);
      printf("%s ua=%s headers=", argv[i] + 5, req.user_agent);
      /* one line per host, so the block's line ends print as '|' */
      for (p = req.headers; *p != '\0'; p++) {
        if (*p != '\r')
          putchar(*p == '\n' ? '|' : *p);
      }
      putchar('\n');
    }
  }
done:
  hts_bundle_free(opt);
  StringCopy(opt->bundle_dir, "");
  opt->cookie = prev;
  opt->log = log;
  freet(jar);
  return 0;
}

const struct selftest_entry selftests_bundle[] = {
    {"bundlecut", "<url>", "the login bundle a start URL names", st_bundlecut},
    {"bundle", "<dir> <url>... [host:<adr>]... [jar:<file>]",
     "load login bundles and print the identity each host gets", st_bundle},
    {NULL, NULL, NULL, NULL},
};
