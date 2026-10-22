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
  static const char nodir[] = "/nonexistent-bundlecut/";
  char url[HTS_URLMAXSIZE * 2];
  char err[1024];
  const char *why;

  if (argc != 1 || strlen(argv[0]) >= sizeof(url)) {
    fprintf(stderr, "bundlecut: takes <url>\n");
    return 1;
  }
  strcpybuff(url, argv[0]);
  hts_bundle_free(opt);
  why = hts_bundle_take(opt, url);
  if (why != NULL) {
    printf("bad: %s\n", why);
  } else if (opt->bundle_state == NULL) {
    printf("none\n");
  } else {
    /* the name shows in the file a load from a missing folder cannot open */
    StringCopy(opt->bundle_dir, nodir);
    assertf(!hts_bundle_load_all(opt, NULL, err, sizeof(err)) &&
            strncmp(err, nodir, sizeof(nodir) - 1) == 0 &&
            strstr(err, ".hts: ") != NULL);
    printf("%.*s %s\n",
           (int) (strstr(err, ".hts: ") - err) - (int) (sizeof(nodir) - 1),
           err + sizeof(nodir) - 1, url);
    StringCopy(opt->bundle_dir, "");
    hts_bundle_free(opt);
  }
  return 0;
}

/* Print err with each "<dir>/" cut to "/", since the folder is the caller's. */
static void print_without_dir(const char *err, const char *dir) {
  const size_t dlen = strlen(dir);
  const char *p = err;
  const char *at;

  while ((at = strstr(p, dir)) != NULL) {
    const hts_boolean whole = at[dlen] == '/' || at[dlen] == '\\';

    printf("%.*s", (int) (at - p) + (whole ? 0 : (int) dlen), p);
    p = at + dlen;
  }
  printf("%s\n", p);
}

/* -#test=bundle <dir> <url>... [host:<adr>]... [hide:<text>]... [jar:<file>]
   [twice] [b0]: load the bundles the URLs name from dir over the jar file,
   twice when asked, with cookies off for b0, then print their url lines, the
   jar, the UA and headers each adr gets, and each text as the logs show it. */
static int st_bundle(httrackp *opt, int argc, char **argv) {
  FILE *const log = opt->log;
  t_cookie *const prev = opt->cookie;
  t_cookie *jar = calloct(1, sizeof(t_cookie));
  const char *jar_file = NULL;
  int loads = 1;
  char err[1024];
  const char *url;
  char *rec;
  int i;

  if (argc < 2 || jar == NULL) {
    fprintf(stderr, "bundle: takes <dir> <url>... [host:<adr>]... "
                    "[hide:<text>]... [jar:<file>] [twice] [b0]\n");
    freet(jar);
    return 1;
  }
  jar->max_len = COOKIE_MAX_JAR; /* httpmirror's */
  opt->log = stderr;    /* warnings stay out of the graded output */
  opt->cookie = jar;
  hts_bundle_free(opt);
  StringCopy(opt->bundle_dir, argv[0]);
  for (i = 1; i < argc; i++) {
    char buff[HTS_URLMAXSIZE * 2];

    if (strncmp(argv[i], "jar:", 4) == 0) {
      jar_file = argv[i] + 4;
      (void) cookie_load(opt, jar, "", jar_file);
    } else if (strcmp(argv[i], "twice") == 0) {
      loads = 2;
    } else if (strcmp(argv[i], "b0") == 0) {
      opt->cookie = NULL;
    } else if (strncmp(argv[i], "host:", 5) != 0 &&
               strncmp(argv[i], "hide:", 5) != 0 &&
               strlen(argv[i]) < sizeof(buff)) {
      strcpybuff(buff, argv[i]);
      assertf(hts_bundle_take(opt, buff) == NULL);
    }
  }
  for (i = 0; i < loads; i++) {
    if (!hts_bundle_load_all(opt, jar_file, err, sizeof(err))) {
      printf("refused: ");
      print_without_dir(err, argv[0]);
      goto done;
    }
  }
  for (i = 0; (url = hts_bundle_url(opt, i)) != NULL; i++)
    printf("url %s\n", url);
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
    } else if (strncmp(argv[i], "hide:", 5) == 0) {
      char shown[HTS_URLMAXSIZE * 2];

      (void) strclipbuff(shown, sizeof(shown), argv[i] + 5);
      hts_hide_secrets(opt, shown, sizeof(shown));
      printf("hide %s\n", shown);
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
    {"bundle",
     "<dir> <url>... [host:<adr>]... [hide:<text>]... [jar:<file>] [twice] "
     "[b0]",
     "load login bundles and print the identity each host gets", st_bundle},
    {NULL, NULL, NULL, NULL},
};
