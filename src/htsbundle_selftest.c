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
#include "htsthread.h"

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

/* -#test=regdomain <host>...: each host's registrable domain, or "none". */
static int st_regdomain(httrackp *opt, int argc, char **argv) {
  int i;

  (void) opt;
  for (i = 0; i < argc; i++) {
    char reg[256];

    printf("%s %s\n", argv[i],
           hts_registrable_domain(argv[i], reg, sizeof(reg)) ? reg : "none");
  }
  return 0;
}

/* -#test=bundlescope <cookie domain> <url>...: "keep" when a bundle naming the
   URLs keeps a cookie set for the domain, else "drop". */
static int st_bundlescope(httrackp *opt, int argc, char **argv) {
  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "bundlescope: takes <cookie domain> <url>...\n");
    return 1;
  }
  printf("%s\n", hts_bundle_cookie_in_scope((const char *const *) &argv[1],
                                            (size_t) (argc - 1), argv[0])
                     ? "keep"
                     : "drop");
  return 0;
}

/* -#test=bundlenames <urls> [<list file>]: the count and the bundle names the
   addresses use, or "refused: <why>". */
static int st_bundlenames(httrackp *opt, int argc, char **argv) {
  char *stale[1] = {NULL};
  char **names;
  char error[64];
  int n;
  int i;

  (void) opt;
  if (argc < 1 || argc > 2) {
    fprintf(stderr, "bundlenames: takes <urls> [<list file>]\n");
    return 1;
  }
  /* so a failure must reset them */
  names = stale;
  strcpybuff(error, "stale");
  n = hts_bundle_names_in_use(argv[0], argc == 2 ? argv[1] : NULL, &names,
                              error, sizeof(error));
  if (n < 0) {
    assertf(names == NULL);
    printf("refused: %s\n", error);
    return 0;
  }
  assertf(names[n] == NULL);
  printf("%d:", n);
  for (i = 0; i < n; i++)
    printf(" %s", names[i]);
  printf("\n");
  hts_bundle_names_free(names);
  return 0;
}

/* -#test=bundlename <name>: "ok", or why the name is refused. */
static int st_bundlename(httrackp *opt, int argc, char **argv) {
  const char *why;

  (void) opt;
  if (argc != 1) {
    fprintf(stderr, "bundlename: takes <name>\n");
    return 1;
  }
  why = hts_bundle_name_refusal(argv[0]);
  printf("%s\n", why != NULL ? why : "ok");
  return 0;
}

/* -#test=paste <dir> <file> [agent:<ua>] [url:<url>] [dry] [keep]: turn the
   paste held in file into a bundle in dir, and print what came back. keep
   passes HTS_PASTE_KEEP_EMPTY. */
static int st_paste(httrackp *opt, int argc, char **argv) {
  const char *agent = NULL, *url = NULL;
  hts_boolean dry = HTS_FALSE, keep = HTS_FALSE;
  hts_paste_result *res = calloct(1, sizeof(*res));
  String paste = STRING_EMPTY;
  char buf[4096];
  size_t n;
  FILE *fp;
  int i;

  (void) opt;
  if (res != NULL)
    res->size = sizeof(*res);
  fp = argc >= 2 ? FOPEN(argv[1], "rb") : NULL;
  if (fp == NULL || res == NULL) {
    fprintf(stderr, "paste: takes <dir> <file> [agent:<ua>] [url:<url>] "
                    "[dry] [keep]\n");
    if (fp != NULL)
      fclose(fp);
    freet(res);
    return 1;
  }
  while ((n = fread(buf, 1, sizeof(buf), fp)) > 0)
    StringMemcat(paste, buf, n);
  fclose(fp);
  for (i = 2; i < argc; i++) {
    if (strncmp(argv[i], "agent:", 6) == 0)
      agent = argv[i] + 6;
    else if (strncmp(argv[i], "url:", 4) == 0)
      url = argv[i] + 4;
    else if (strcmp(argv[i], "dry") == 0)
      dry = HTS_TRUE;
    else if (strcmp(argv[i], "keep") == 0)
      keep = HTS_TRUE;
  }
  if (!StringNotEmpty(paste))
    StringCat(paste, "");
  if (strlen(StringBuff(paste)) != StringLength(paste)) {
    printf("refused: the file holds a NUL\n");
  } else if (!(keep ? hts_bundle_from_paste_flags(
                          StringBuff(paste), argv[0], agent, url,
                          HTS_PASTE_KEEP_EMPTY | (dry ? HTS_PASTE_DRY_RUN : 0),
                          res)
                    : hts_bundle_from_paste(StringBuff(paste), argv[0], agent,
                                            url, dry, res))) {
    assertf(res->name[0] == '\0' && res->url[0] == '\0' &&
            res->agent[0] == '\0' && res->hosts[0] == '\0' &&
            res->cookies == 0 && res->error[0] != '\0');
    printf("refused: %s\n", res->error);
  } else {
    assertf(res->error[0] == '\0');
    printf("name=%s cookies=%d hosts=%s\nurl=%s\nagent=%s\n", res->name,
           res->cookies, res->hosts, res->url, res->agent);
  }
  StringFree(paste);
  freet(res);
  return 0;
}

typedef struct paste_race {
  const char *dir;
  int rounds;
  volatile int failed;
  char why[HTS_PASTE_ERROR_SIZE]; /* the first failure */
} paste_race;

static void paste_race_worker(void *arg) {
  paste_race *const race = (paste_race *) arg;
  hts_paste_result res;
  int i;

  for (i = 0; i < race->rounds; i++) {
    res.size = sizeof(res);
    if (!hts_bundle_from_paste("curl 'https://race.test/' -b a=1", race->dir,
                               NULL, NULL, HTS_FALSE, &res) &&
        hts_increment_int(&race->failed) == 1)
      strcpybuff(race->why, res.error);
  }
}

/* Two threads of one process write the same bundle at once. */
static int st_pasterace(httrackp *opt, int argc, char **argv) {
  paste_race race;

  (void) opt;
  if (argc < 2) {
    fprintf(stderr, "pasterace: takes <dir> <rounds>\n");
    return 1;
  }
  race.dir = argv[0];
  race.rounds = atoi(argv[1]);
  race.failed = 0;
  race.why[0] = '\0';
  if (hts_newthread(paste_race_worker, &race) != 0 ||
      hts_newthread(paste_race_worker, &race) != 0) {
    fprintf(stderr, "pasterace: cannot start a thread\n");
    return 1;
  }
  htsthread_wait_n(0);
  printf("failed=%d\n", hts_load_acquire_int(&race.failed));
  if (race.why[0] != '\0')
    printf("first: %s\n", race.why);
  return 0;
}

const struct selftest_entry selftests_bundle[] = {
    {"bundlecut", "<url>", "the login bundle a start URL names", st_bundlecut},
    {"bundle",
     "<dir> <url>... [host:<adr>]... [hide:<text>]... [jar:<file>] [twice] "
     "[b0]",
     "load login bundles and print the identity each host gets", st_bundle},
    {"regdomain", "<host>...", "the registrable domain of each host",
     st_regdomain},
    {"bundlename", "<name>", "is name a login bundle name?", st_bundlename},
    {"bundlescope", "<cookie domain> <url>...",
     "does a login bundle naming the URLs keep a cookie of that domain?",
     st_bundlescope},
    {"bundlenames", "<urls> [<list file>]",
     "the login bundle names the start addresses use", st_bundlenames},
    {"paste", "<dir> <file> [agent:<ua>] [url:<url>] [dry] [keep]",
     "turn a pasted cURL command or cookies.txt into a login bundle", st_paste},
    {"pasterace", "<dir> <rounds>",
     "two threads write the same login bundle at once", st_pasterace},
    {NULL, NULL, NULL, NULL},
};
