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
/* File: htswizard_selftest.c subroutines:                      */
/*       self-tests for the interactive wizard                  */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htsselftest_int.h"

/* The pattern answer n owes (adr, fil): a second implementation of the builder,
   with no bound of its own, to compare it against. */
static void wizardfilter_want(htsbuff *w, int n, const char *adr,
                              const char *fil, hts_boolean up) {
  htsbuff_cpy(w, n <= 2 ? "-" : "+");
  htsbuff_cat(w, adr);
  if (n == 2 || n == 6 || (n == 5 && up)) {
    htsbuff_cat(w, "/*");
    return;
  }
  if (*fil != '/')
    htsbuff_cat(w, "/");
  htsbuff_cat(w, fil);
  if (n == 1 || n == 5)
    htsbuff_cat(w, "*");
  else if (n == 7)
    htsbuff_cat(w, HTS_WIZARD_FILTER_SUFFIX);
}

/* A link of the given lengths: adr all 'a', fil a directory of 'b' ending on a
   slash for answers 1, 5 and 7 to anchor on, led by one when `slash`. */
static void wizardfilter_link(char *adr, size_t adrlen, char *fil,
                              size_t fillen, hts_boolean slash) {
  memset(adr, 'a', adrlen);
  adr[adrlen] = '\0';
  memset(fil, 'b', fillen);
  fil[0] = slash ? '/' : 'b';
  fil[fillen - 1] = '/';
  fil[fillen] = '\0';
}

/* Asserts every answer that emits a filter for (adr, fil) emits exactly what
   wizardfilter_want() owes, and returns the longest pattern seen. */
static size_t wizardfilter_emits(htsbuff *f, char *expect, const char *adr,
                                 const char *fil) {
  static const int answers[] = {0, 1, 2, 5, 6, 7};
  size_t i, up, longest = 0;

  for (i = 0; i < sizeof(answers) / sizeof(answers[0]); i++) {
    for (up = 0; up < 2; up++) {
      const hts_boolean seek = up != 0 ? HTS_TRUE : HTS_FALSE;
      htsbuff w = htsbuff_ptr(expect, HTS_FILTER_SLOT_SIZE);

      wizardfilter_want(&w, answers[i], adr, fil, seek);
      hts_wizard_answer_filter(f, 0, answers[i], adr, fil, seek);
      assertf(strcmp(f->buf, expect) == 0);
      longest = f->len > longest ? f->len : longest;
    }
  }
  return longest;
}

/* Prints the filter answer <n> emits for (adr, fil) [up] in [slot]; with no
   arguments, asserts every answer against its expected pattern (#1119). */
static int st_wizardfilter(httrackp *opt, int argc, char **argv) {
  char pattern[HTS_FILTER_SLOT_SIZE];
  htsbuff f = htsbuff_array(pattern);

  (void) opt;
  if (argc >= 3) {
    hts_wizard_answer_filter(
        &f, argc >= 5 ? atoi(argv[4]) : 0, atoi(argv[0]), argv[1], argv[2],
        argc >= 4 && atoi(argv[3]) != 0 ? HTS_TRUE : HTS_FALSE);
    printf("%s\n", pattern);
    return 0;
  }
#define EMITS_SLOT(slot, n, adr, fil, up, expect)                              \
  do {                                                                         \
    hts_wizard_answer_filter(&f, (slot), (n), (adr), (fil), (up));             \
    assertf(strcmp(pattern, (expect)) == 0);                                   \
  } while (0)
#define EMITS(n, adr, fil, up, expect)                                         \
  EMITS_SLOT(0, (n), (adr), (fil), (up), (expect))

  /* the host-wide answers: 2 forbids, 5 (allowed to go up) and 6 authorize */
  EMITS(2, "foo.com", "/index.html", HTS_FALSE, "-foo.com/*");
  EMITS(5, "foo.com", "/dir/page.html", HTS_TRUE, "+foo.com/*");
  EMITS(6, "foo.com", "/dir/page.html", HTS_FALSE, "+foo.com/*");
  /* the port is part of the host, the credentials are not */
  EMITS(2, "foo.com:8080", "/x", HTS_FALSE, "-foo.com:8080/*");
  EMITS(2, "user:pass@foo.com", "/x", HTS_FALSE, "-foo.com/*");
  EMITS(5, "foo.com:8080", "/x", HTS_TRUE, "+foo.com:8080/*");
  EMITS(5, "user:pass@foo.com", "/x", HTS_TRUE, "+foo.com/*");
  EMITS(6, "foo.com:8080", "/x", HTS_FALSE, "+foo.com:8080/*");
  EMITS(6, "user:pass@foo.com", "/x", HTS_FALSE, "+foo.com/*");

  /* the trailing slash is what keeps a longer host out */
  assertf(strjoker("foo.com/index.html", pattern + 1, NULL, NULL) != NULL);
  assertf(strjoker("foo.com/", pattern + 1, NULL, NULL) != NULL);
  assertf(strjoker("foo.com.evil.org/x", pattern + 1, NULL, NULL) == NULL);
  assertf(strjoker("foo.com:8080/x", pattern + 1, NULL, NULL) == NULL);

  /* the link and directory answers */
  EMITS(0, "foo.com", "/dir/page.html", HTS_FALSE, "-foo.com/dir/page.html");
  EMITS(0, "foo.com", "index.html", HTS_FALSE, "-foo.com/index.html");
  /* #1251: an answer we could not read records that same default, separator
     included */
  EMITS(-999, "foo.com", "/dir/page.html", HTS_FALSE, "-foo.com/dir/page.html");
  EMITS(-999, "foo.com", "index.html", HTS_FALSE, "-foo.com/index.html");
  EMITS(1, "foo.com", "/dir/page.html", HTS_FALSE, "-foo.com/dir/*");
  EMITS(1, "foo.com", "/page.html", HTS_FALSE, "-foo.com/*");
  EMITS(5, "foo.com", "/dir/page.html", HTS_FALSE, "+foo.com/dir/*");
  EMITS(7, "foo.com", "/dir/page.html", HTS_FALSE, "+foo.com/dir/*[file]");
  /* answer 1 collapses a doubled trailing slash, answers 5 and 7 keep it */
  EMITS(1, "foo.com", "/dir//page.html", HTS_FALSE, "-foo.com/dir/*");
  EMITS(5, "foo.com", "/dir//page.html", HTS_FALSE, "+foo.com/dir//*");
  EMITS(7, "foo.com", "/dir//page.html", HTS_FALSE, "+foo.com/dir//*[file]");
  /* no directory to anchor on, so answers 1, 5 and 7 emit nothing */
  EMITS(1, "foo.com", "page.html", HTS_FALSE, "");
  EMITS(5, "foo.com", "page.html", HTS_FALSE, "");
  EMITS(7, "foo.com", "page.html", HTS_FALSE, "");

  /* A long link must emit the same pattern a short one does, byte for byte:
     clipping it would widen the rule (a cut "*[file]" becomes "*"), and the
     lengths come from the wire. Sweeps the old 2048-byte slot, then the real
     worst case, both link buffers full. */
  {
    const size_t urlmax = HTS_URLMAXSIZE * 2 - 1; /* the engine's adr and fil */
    char *adr = malloct(urlmax + 1);
    char *fil = malloct(urlmax + 1);
    char *expect = malloct(HTS_FILTER_SLOT_SIZE);
    size_t total;

    for (total = 2040; total <= 2048; total++) {
      const size_t adrlen = total / 2, fillen = total - 1 - adrlen;

      wizardfilter_link(adr, adrlen, fil, fillen, HTS_TRUE);
      wizardfilter_emits(&f, expect, adr, fil);
    }
    /* the real maximum: both buffers full, and no leading slash on fil to
       spare the separator. It fills the slot exactly, NUL included */
    wizardfilter_link(adr, urlmax, fil, urlmax, HTS_FALSE);
    assertf(wizardfilter_emits(&f, expect, adr, fil) ==
            HTS_FILTER_SLOT_SIZE - 1);
    freet(adr);
    freet(fil);
    freet(expect);
  }

  /* the answers that add no filter at all */
  EMITS(-1, "foo.com", "/x", HTS_FALSE, "");
  EMITS(3, "foo.com", "/x", HTS_FALSE, "");
  EMITS(4, "foo.com", "/x", HTS_FALSE, "");
  EMITS(50, "foo.com", "/x", HTS_FALSE, "");
  /* only slot 0 is ever filled outside the host-scope answers */
  EMITS_SLOT(1, 2, "foo.com", "/x", HTS_FALSE, "");
  EMITS_SLOT(1, 6, "foo.com", "/x", HTS_FALSE, "");

  /* the host-scope answers (#1117): both slots, the starred one missing the
     apex is why the second exists */
#define SCOPE_IN HTS_WIZARD_SCOPE_INCLUDE
#define SCOPE_EX HTS_WIZARD_SCOPE_EXCLUDE
  EMITS_SLOT(0, SCOPE_IN, "www.example.co.uk", "/x", HTS_FALSE,
             "+*.www.example.co.uk/*");
  EMITS_SLOT(1, SCOPE_IN, "www.example.co.uk", "/x", HTS_FALSE,
             "+www.example.co.uk/*");
  EMITS_SLOT(0, SCOPE_IN + 1, "www.example.co.uk", "/x", HTS_FALSE,
             "+*.example.co.uk/*");
  EMITS_SLOT(1, SCOPE_IN + 1, "www.example.co.uk", "/x", HTS_FALSE,
             "+example.co.uk/*");
  EMITS_SLOT(0, SCOPE_EX + 1, "www.example.co.uk", "/x", HTS_FALSE,
             "-*.example.co.uk/*");
  EMITS_SLOT(1, SCOPE_EX + 1, "www.example.co.uk", "/x", HTS_FALSE,
             "-example.co.uk/*");
  /* the port rides along, the credentials do not */
  EMITS_SLOT(0, SCOPE_IN + 1, "www.foo.com:8080", "/x", HTS_FALSE,
             "+*.foo.com:8080/*");
  EMITS_SLOT(1, SCOPE_IN, "user:pass@www.foo.com", "/x", HTS_FALSE,
             "+www.foo.com/*");
  /* #1251: a host with no domain below takes the answer on the host itself */
  EMITS_SLOT(0, SCOPE_IN + 2, "www.foo.com", "/x", HTS_FALSE, "+www.foo.com/*");
  EMITS_SLOT(1, SCOPE_IN + 2, "www.foo.com", "/x", HTS_FALSE, "");
  EMITS_SLOT(0, SCOPE_IN, "127.0.0.1:8080", "/x", HTS_FALSE,
             "+127.0.0.1:8080/*");
  EMITS_SLOT(0, SCOPE_IN, "user:pass@127.0.0.1", "/x", HTS_FALSE,
             "+127.0.0.1/*");
  EMITS_SLOT(1, SCOPE_IN, "127.0.0.1:8080", "/x", HTS_FALSE, "");
  EMITS_SLOT(0, SCOPE_EX, "localhost", "/x", HTS_FALSE, "-localhost/*");
  EMITS_SLOT(0, SCOPE_EX, "[::1]", "/x", HTS_FALSE, "-[::1]/*");
  /* slot 2 is past the pair either way */
  EMITS_SLOT(2, SCOPE_IN, "www.foo.com", "/x", HTS_FALSE, "");
  EMITS_SLOT(2, SCOPE_IN, "127.0.0.1", "/x", HTS_FALSE, "");

  /* what the pair must and must not catch */
  EMITS_SLOT(0, SCOPE_IN + 1, "www.example.co.uk", "/x", HTS_FALSE,
             "+*.example.co.uk/*");
  assertf(strjoker("a.b.example.co.uk/x", pattern + 1, NULL, NULL) != NULL);
  assertf(strjoker("example.co.uk/x", pattern + 1, NULL, NULL) == NULL);
  assertf(strjoker("notexample.co.uk/x", pattern + 1, NULL, NULL) == NULL);
  assertf(strjoker("example.co.uk.evil.com/x", pattern + 1, NULL, NULL) ==
          NULL);
  EMITS_SLOT(1, SCOPE_IN + 1, "www.example.co.uk", "/x", HTS_FALSE,
             "+example.co.uk/*");
  assertf(strjoker("example.co.uk/x", pattern + 1, NULL, NULL) != NULL);
  assertf(strjoker("notexample.co.uk/x", pattern + 1, NULL, NULL) == NULL);
#undef SCOPE_IN
#undef SCOPE_EX
#undef EMITS_SLOT
#undef EMITS
  printf("wizardfilter self-test OK\n");
  return 0;
}

/* Prints the domain scopes offered for <question>; with no argument, asserts
   the enumeration (#1117). */
static int st_wizardscope(httrackp *opt, int argc, char **argv) {
  char scope[HTS_URLMAXSIZE];
  int k;

  (void) opt;
  if (argc >= 1) {
    for (k = 0; hts_wizard_host_scope(argv[0], k, scope, sizeof(scope)); k++)
      printf("%d %s\n", k, scope);
    return 0;
  }
#define SCOPE(question, k, expect)                                             \
  do {                                                                         \
    assertf(hts_wizard_host_scope((question), (k), scope, sizeof(scope)));     \
    assertf(strcmp(scope, (expect)) == 0);                                     \
  } while (0)
/* poisoned first: comparing against '\0' cannot see a clear that never ran */
#define NOSCOPE_SIZED(question, k, size)                                       \
  do {                                                                         \
    memset(scope, 'X', sizeof(scope));                                         \
    assertf(!hts_wizard_host_scope((question), (k), scope, (size)));           \
    assertf(scope[0] == '\0');                                                 \
  } while (0)
#define NOSCOPE(question, k) NOSCOPE_SIZED((question), (k), sizeof(scope))

  /* k widens by one label at a time, starting at the host itself */
  SCOPE("download.example.co.uk/x", 0, "download.example.co.uk");
  SCOPE("download.example.co.uk/x", 1, "example.co.uk");
  SCOPE("download.example.co.uk/x", 2, "co.uk");
  NOSCOPE("download.example.co.uk/x", 3); /* "uk" is a bare TLD */
  SCOPE("example.com", 0, "example.com"); /* an adr with no fil works */
  NOSCOPE("example.com", 1);
  NOSCOPE("localhost/x", 0); /* nothing to widen into */
  NOSCOPE("download.example.co.uk/x", -1);

  /* protocol and credentials are stripped, the port kept */
  SCOPE("ftp://user:pass@www.foo.com/x", 1, "foo.com");
  SCOPE("www.foo.com:8080/x", 0, "www.foo.com:8080");
  SCOPE("www.foo.com:8080/x", 1, "foo.com:8080");
  NOSCOPE("www.foo.com:8080/x", 2);
  /* a path that carries dots or a colon must not be read as host labels */
  SCOPE("foo.com/a.b.c/d:e", 0, "foo.com");
  NOSCOPE("foo.com/a.b.c/d:e", 1);

  /* the shared predicate: a dotless run of digits is a hostname, not an IP */
  assertf(hts_host_is_ipv4("1.2.3.4", 7));
  assertf(!hts_host_is_ipv4("12345", 5));
  assertf(!hts_host_is_ipv4("foo.com", 7));

  /* an IP literal splits on dots without being a domain */
  NOSCOPE("192.168.1.1/x", 0);
  NOSCOPE("192.168.1.1:8080/x", 0);
  NOSCOPE("[3ffe:b80:1234::1]/x", 0);
  /* the dots inside this one reach the label walk unless brackets are refused
   */
  NOSCOPE("[::ffff:1.2.3.4]/x", 0);

  /* the root label of a fully-qualified host is not a label */
  SCOPE("www.foo.com./x", 0, "www.foo.com.");
  SCOPE("www.foo.com./x", 1, "foo.com.");
  NOSCOPE("www.foo.com./x", 2); /* "com." is still a bare TLD */
  NOSCOPE(".", 0);

  /* the destination must fit the scope and its terminator, and never truncate
   */
  {
    const char *q = "www.example.com/x";
    const size_t need = strlen("www.example.com");

    memset(scope, 'X', sizeof(scope));
    assertf(hts_wizard_host_scope(q, 0, scope, need + 1));
    assertf(strcmp(scope, "www.example.com") == 0);
    NOSCOPE_SIZED(q, 0, need);
    NOSCOPE_SIZED(q, 0, 4);
    NOSCOPE_SIZED(q, 0, 1);
  }
#undef SCOPE
#undef NOSCOPE
#undef NOSCOPE_SIZED
  printf("wizardscope self-test OK\n");
  return 0;
}

/* #1117: which host-scope range an answer falls in. */
static int st_wizardscopeanswer(httrackp *opt, int argc, char **argv) {
  (void) opt;
  (void) argc;
  (void) argv;
#define ANSWER(n, expect) assertf(hts_wizard_scope_answer(n) == (expect))
  /* the plain answers, and the boundary just below the first range */
  ANSWER(-999, HTS_DEFAULT);
  ANSWER(-1, HTS_DEFAULT);
  ANSWER(0, HTS_DEFAULT);
  ANSWER(7, HTS_DEFAULT);
  ANSWER(50, HTS_DEFAULT);
  ANSWER(HTS_WIZARD_SCOPE_INCLUDE - 1, HTS_DEFAULT);
  /* include runs up to the exclude base, and exclude has no upper end */
  ANSWER(HTS_WIZARD_SCOPE_INCLUDE, HTS_FALSE);
  ANSWER(HTS_WIZARD_SCOPE_EXCLUDE - 1, HTS_FALSE);
  ANSWER(HTS_WIZARD_SCOPE_EXCLUDE, HTS_TRUE);
  ANSWER(INT_MAX, HTS_TRUE);
#undef ANSWER
  printf("wizardscopeanswer self-test OK\n");
  return 0;
}

/* Poison: comparing the recursion cap against 0 would not see a stray write of
   the level the crawl uses. */
#define PRIO_UNSET 42

/* Prints what answer `n` does to the crawl; with no arguments, asserts every
   answer, on an undecided, an allowed and an already refused link. */
static int st_wizardverdict(httrackp *opt, int argc, char **argv) {
  const hts_wizard asked = opt->wizard;
  FILE *const projectlog = opt->log;
  char line[HTS_URLMAXSIZE];
  FILE *log;
  int url, depth;

  url = -1; /* the undecided verdict the wizard is asked about */
  depth = PRIO_UNSET;
  opt->wizard = HTS_WIZARD_ASK;
  if (argc >= 1) {
    hts_wizard_apply_verdict(opt, atoi(argv[0]), "foo.com", "/a/b.html", &url,
                             &depth);
    printf("forbidden=%d stop=%d prio=%d\n", url,
           opt->wizard == HTS_WIZARD_AUTO, depth);
    opt->wizard = asked;
    return 0;
  }
  opt->log = NULL; /* the battery walks the answers that warn */
/* answer `n` over a link the crawl had left at `in`: the verdict it must leave,
   whether it stops the questions, and the recursion cap it must set. */
#define APPLIES(n, in, forbidden, stop, prio)                                  \
  do {                                                                         \
    url = (in);                                                                \
    depth = PRIO_UNSET;                                                        \
    opt->wizard = HTS_WIZARD_ASK;                                              \
    hts_wizard_apply_verdict(opt, (n), "foo.com", "/a/b.html", &url, &depth);  \
    assertf(url == (forbidden));                                               \
    assertf(opt->wizard == ((stop) ? HTS_WIZARD_AUTO : HTS_WIZARD_ASK));       \
    assertf(depth == (prio));                                                  \
  } while (0)
  /* '*' refuses and stops the questions */
  APPLIES(-1, 0, 1, 1, PRIO_UNSET);
  APPLIES(-1, 1, 1, 1, PRIO_UNSET);
  /* the refusing answers, 3 included although it emits no filter yet */
  APPLIES(0, 0, 1, 0, PRIO_UNSET);
  APPLIES(1, 0, 1, 0, PRIO_UNSET);
  APPLIES(2, 0, 1, 0, PRIO_UNSET);
  APPLIES(3, 0, 1, 0, PRIO_UNSET);
  /* 4 caps the recursion, and takes the link like any accepting answer */
  APPLIES(4, 0, 0, 0, 1);
  APPLIES(4, 1, 1, 0, 1);
  /* an accepting answer never clears a refusal the crawl already computed */
  APPLIES(5, 1, 1, 0, PRIO_UNSET);
  APPLIES(6, 1, 1, 0, PRIO_UNSET);
  APPLIES(7, 1, 1, 0, PRIO_UNSET);
  APPLIES(50, 1, 1, 0, PRIO_UNSET);
  APPLIES(6, 0, 0, 0, PRIO_UNSET);
  /* #1251: an answer we could not read refuses, like the empty one */
  APPLIES(-999, 0, 1, 0, PRIO_UNSET);
  APPLIES(-999, 1, 1, 0, PRIO_UNSET);
  /* both ends of each scope range: include allows, exclude forbids */
  APPLIES(HTS_WIZARD_SCOPE_INCLUDE, 0, 0, 0, PRIO_UNSET);
  APPLIES(HTS_WIZARD_SCOPE_EXCLUDE - 1, 0, 0, 0, PRIO_UNSET);
  APPLIES(HTS_WIZARD_SCOPE_EXCLUDE, 0, 1, 0, PRIO_UNSET);
  APPLIES(INT_MAX, 0, 1, 0, PRIO_UNSET);
  /* an answer in no range never overturns an accept or a refusal */
  APPLIES(8, 0, 0, 0, PRIO_UNSET);
  APPLIES(8, 1, 1, 0, PRIO_UNSET);
  APPLIES(999, 0, 0, 0, PRIO_UNSET);
  APPLIES(-2, 0, 0, 0, PRIO_UNSET);
  APPLIES(-1000, 0, 0, 0, PRIO_UNSET);
  APPLIES(INT_MIN, 0, 0, 0, PRIO_UNSET);
  APPLIES(HTS_WIZARD_SCOPE_INCLUDE - 1, 0, 0, 0, PRIO_UNSET);
  /* an answer that does not refuse leaves the link allowed, never undecided */
  APPLIES(4, -1, 0, 0, 1);
  APPLIES(5, -1, 0, 0, PRIO_UNSET);
  APPLIES(6, -1, 0, 0, PRIO_UNSET);
  APPLIES(7, -1, 0, 0, PRIO_UNSET);
  APPLIES(50, -1, 0, 0, PRIO_UNSET);
  APPLIES(8, -1, 0, 0, PRIO_UNSET); /* -999 is #1259's, not asserted here */
  APPLIES(HTS_WIZARD_SCOPE_INCLUDE, -1, 0, 0, PRIO_UNSET);
  /* and one that does refuse must still refuse it */
  APPLIES(-1, -1, 1, 1, PRIO_UNSET);
  APPLIES(0, -1, 1, 0, PRIO_UNSET);
  APPLIES(1, -1, 1, 0, PRIO_UNSET);
  APPLIES(2, -1, 1, 0, PRIO_UNSET);
  APPLIES(3, -1, 1, 0, PRIO_UNSET);
  APPLIES(HTS_WIZARD_SCOPE_EXCLUDE, -1, 1, 0, PRIO_UNSET);
#undef APPLIES

/* the one log line answer `n` leaves for `adr`, "" for none */
#define WARNS(n, adr, expect)                                                  \
  do {                                                                         \
    log = tmpfile();                                                           \
    assertf(log != NULL);                                                      \
    opt->log = log;                                                            \
    url = 0;                                                                   \
    depth = PRIO_UNSET;                                                        \
    hts_wizard_apply_verdict(opt, (n), (adr), "/a/b.html", &url, &depth);      \
    rewind(log);                                                               \
    if (fgets(line, (int) sizeof(line), log) == NULL)                          \
      line[0] = '\0';                                                          \
    /* the wanted text, an empty log where there is none, nothing after it */  \
    assertf(strstr(line, (expect)) != NULL &&                                  \
            ((expect)[0] != '\0') == (line[0] != '\0') &&                      \
            fgets(line, (int) sizeof(line), log) == NULL);                     \
    fclose(log);                                                               \
  } while (0)
  /* an answer the engine can honour in full says nothing */
  WARNS(6, "foo.com", "");
  WARNS(8, "foo.com", "unknown answer 8");
  WARNS(HTS_WIZARD_SCOPE_INCLUDE, "www.foo.com", "");
  /* #1251: the answers the engine cannot honour as asked */
  WARNS(-999, "foo.com", "could not read your answer");
  WARNS(HTS_WIZARD_SCOPE_INCLUDE, "127.0.0.1", "has no domain above it");
  WARNS(HTS_WIZARD_SCOPE_EXCLUDE + 5, "www.foo.com", "has no domain above it");
#undef WARNS

  opt->log = projectlog;
  opt->wizard = asked;
  printf("wizardverdict self-test OK\n");
  return 0;
}

#undef PRIO_UNSET

/* Prints the prompt the wizard asks about <adr> <fil>; with no argument,
   asserts it. */
static int st_wizardprompt(httrackp *opt, int argc, char **argv) {
  char prompt[HTS_URLMAXSIZE * 2];
  char adr[HTS_URLMAXSIZE], fil[HTS_URLMAXSIZE * 2];

  (void) opt;
  if (argc >= 2) {
    hts_wizard_prompt_url(prompt, sizeof(prompt), argv[0], argv[1]);
    printf("%s\n", prompt);
    return 0;
  }
#define ASKS(adr_, fil_, expect)                                               \
  do {                                                                         \
    hts_wizard_prompt_url(prompt, sizeof(prompt), (adr_), (fil_));             \
    assertf(strcmp(prompt, (expect)) == 0);                                    \
  } while (0)
  ASKS("foo.com", "/dir/page.html", "foo.com/dir/page.html");
  ASKS("foo.com:8080", "/", "foo.com:8080/");
  ASKS("user:pass@foo.com", "/x", "user:pass@foo.com/x");
  /* the separator, for the slash-less forms the parser does not emit today */
  ASKS("foo.com", "index.html", "foo.com/index.html");
  ASKS("foo.com", "", "foo.com/");
#undef ASKS

  /* #1251: a link too long for the prompt is clipped, not fatal, and what is
     left still names the host it came from */
  memset(adr, 'a', sizeof(adr) - 1);
  adr[sizeof(adr) - 1] = '\0';
  memset(fil, 'b', sizeof(fil) - 1);
  fil[sizeof(fil) - 1] = '\0';
  fil[0] = '/';
  hts_wizard_prompt_url(prompt, sizeof(prompt), adr, fil);
  assertf(strlen(prompt) == sizeof(prompt) - 1);
  assertf(strncmp(prompt, adr, sizeof(adr) - 1) == 0);
  assertf(prompt[sizeof(adr) - 1] == '/');
  /* and again with the separator to insert, which eats one more byte */
  fil[0] = 'b';
  hts_wizard_prompt_url(prompt, sizeof(prompt), adr, fil);
  assertf(strlen(prompt) == sizeof(prompt) - 1);
  assertf(strncmp(prompt, adr, sizeof(adr) - 1) == 0);
  assertf(prompt[sizeof(adr) - 1] == '/');
  assertf(prompt[sizeof(adr)] == 'b');
  /* a host alone long enough to fill it leaves no room for either */
  memset(fil, 'b', sizeof(fil) - 1);
  fil[0] = '/';
  hts_wizard_prompt_url(prompt, sizeof(prompt), fil, fil);
  assertf(strlen(prompt) == sizeof(prompt) - 1);
  assertf(strncmp(prompt, fil, sizeof(prompt) - 1) == 0);

  printf("wizardprompt self-test OK\n");
  return 0;
}

/* Resets the wizard's filter array to the command-line filters `cmd`
   (NULL-terminated). */
static void wz_seed(httrackp *opt, char **filters, int *filptr,
                    const char *const *cmd) {
  int i;

  *filptr = 0;
  opt->wizard_filters = 0;
  for (i = 0; cmd != NULL && cmd[i] != NULL; i++)
    strlcpybuff(filters[(*filptr)++], cmd[i], HTS_FILTER_SLOT_SIZE);
}

/* Asserts the array holds exactly `want`, in order, naming the slot that
   differs. */
static void wz_holds(char **filters, int filptr, const char *const *want) {
  int i;

  for (i = 0; want[i] != NULL; i++) {
    if (i >= filptr || strcmp(filters[i], want[i]) != 0)
      fprintf(stderr, "filter %d: got [%s], want [%s]\n", i,
              i < filptr ? filters[i] : "<past the end>", want[i]);
    assertf(i < filptr);
    assertf(strcmp(filters[i], want[i]) == 0);
  }
  if (filptr != i)
    fprintf(stderr, "filter %d: got [%s], want nothing\n", i,
            i < filptr ? filters[i] : "<past the end>");
  assertf(filptr == i);
}

/* Drives hts_wizard_insert_filters(): prints the array the given answers build
   over the command-line filters, or asserts the precedence rules. */
static int st_wizardinsert(httrackp *opt, int argc, char **argv) {
  const htsfilters saved = opt->filters;
  const int savedwizard = opt->wizard_filters;
  const int savedmax = opt->maxfilter;
  char **filters = NULL;
  int filptr = 0;
  int i;

  assertf(filters_init(&filters, opt->maxfilter, 0) != 0);
  opt->filters.filters = &filters;
  opt->filters.filptr = &filptr;
  opt->wizard_filters = 0;
  /* Answers are httrack.c's query3 codes; seeker_up is pinned off, so answer 5
     takes its directory branch (tests/264 covers the host one). */
#define INSERT(n, adr, fil)                                                    \
  hts_wizard_insert_filters(opt, (n), (adr), (fil), HTS_FALSE)
#define HOLDS(...)                                                             \
  do {                                                                         \
    const char *const want[] = {__VA_ARGS__, NULL};                            \
    wz_holds(filters, filptr, want);                                           \
  } while (0)
#define SEED(...)                                                              \
  do {                                                                         \
    const char *const cmd[] = {__VA_ARGS__, NULL};                             \
    wz_seed(opt, filters, &filptr, cmd);                                       \
  } while (0)
#define RESET() wz_seed(opt, filters, &filptr, NULL)
/* what the array as it stands decides for `url` */
#define VERDICT(url) fa_strjoker(0, filters, filptr, (url), NULL, NULL, NULL)

  if (argc >= 2) {
    int sep = argc;

    for (i = 2; i < argc; i++) {
      if (strcmp(argv[i], "@") == 0) {
        sep = i;
        break;
      }
    }
    for (i = sep + 1;
         i < argc && filptr + HTS_WIZARD_MAX_FILTERS < opt->maxfilter; i++)
      strlcpybuff(filters[filptr++], argv[i], HTS_FILTER_SLOT_SIZE);
    for (i = 2; i < sep; i++)
      INSERT(atoi(argv[i]), argv[0], argv[1]);
    for (i = 0; i < filptr; i++)
      printf("%s%s", i != 0 ? " " : "", filters[i]);
    printf("\n");
    goto done;
  }

  /* wizard_filters indexes one crawl's array, so binding another resets it */
  {
    char **other = NULL;
    int otherptr = 7;

    opt->wizard_filters = 3;
    filters_bind(opt, &other, &otherptr);
    assertf(opt->wizard_filters == 0);
    assertf(opt->filters.filters == &other && opt->filters.filptr == &otherptr);
    filters_bind(opt, &filters, &filptr);
  }

  /* a counter that outlived its array still cannot index past the end */
  SEED("-*.zip");
  opt->wizard_filters = 99;
  INSERT(6, "h", "/dir/two.html");
  assertf(opt->wizard_filters <= filptr);

  /* the correction case: "ignore this link", then "mirror the whole host" */
  RESET();
  assertf(INSERT(0, "h", "/dir/one.html") == 1);
  HOLDS("-h/dir/one.html");
  assertf(opt->wizard_filters == 1);
  assertf(VERDICT("h/dir/one.html") == -1);
  INSERT(6, "h", "/dir/two.html");
  HOLDS("-h/dir/one.html", "+h/*");
  assertf(opt->wizard_filters == 2);
  assertf(VERDICT("h/dir/one.html") == 1);

  /* reversing the answers reverses the outcome, or the block is not ordered */
  RESET();
  INSERT(6, "h", "/dir/two.html");
  INSERT(0, "h", "/dir/one.html");
  HOLDS("+h/*", "-h/dir/one.html");
  assertf(VERDICT("h/dir/one.html") == -1);
  assertf(VERDICT("h/dir/two.html") == 1);

  /* no answer reaches above the command line */
  SEED("-h/*.zip", "+h/keep/*");
  INSERT(6, "h", "/dir/two.html");
  HOLDS("+h/*", "-h/*.zip", "+h/keep/*");
  assertf(opt->wizard_filters == 1);
  assertf(VERDICT("h/a.zip") == -1);
  assertf(VERDICT("h/dir/two.html") == 1);

  /* the primary link records its scope first, so a later answer overrides it */
  RESET();
  INSERT(7, "h", "/dir/index.html");
  INSERT(0, "h", "/dir/one.html");
  HOLDS("+h/dir/*[file]", "-h/dir/one.html");
  assertf(VERDICT("h/dir/one.html") == -1);

  /* both halves of a host-scope answer land in the block, in emission order.
     The count returned is what the caller slices the block by, so assert it. */
  SEED("-*.zip");
  assertf(INSERT(HTS_WIZARD_SCOPE_INCLUDE + 1, "www.example.co.uk",
                 "/x.html") == 2);
  HOLDS("+*.example.co.uk/*", "+example.co.uk/*", "-*.zip");
  assertf(opt->wizard_filters == 2);

  /* an answer that emits nothing leaves the block and the array unchanged */
  SEED("-*.zip");
  assertf(INSERT(4, "h", "/dir/one.html") == 0);
  assertf(INSERT(50, "h", "/dir/one.html") == 0);
  assertf(INSERT(3, "h", "/dir/one.html") == 0);
  HOLDS("-*.zip");
  assertf(opt->wizard_filters == 0);

  /* answers 1, 5 and 7 emit nothing when the file part has no directory */
  RESET();
  INSERT(1, "h", "one.html");
  INSERT(5, "h", "one.html");
  INSERT(7, "h", "one.html");
  assertf(filptr == 0 && opt->wizard_filters == 0);

  printf("wizardinsert self-test OK\n");
done:
#undef INSERT
#undef HOLDS
#undef SEED
#undef RESET
#undef VERDICT
  freet(filters[0]);
  freet(filters);
  opt->filters = saved;
  opt->wizard_filters = savedwizard;
  opt->maxfilter = savedmax;
  return 0;
}

/* ------------------------------------------------------------ */
/* Registry: this module's tests, in the order -#test lists them. */
/* ------------------------------------------------------------ */

const struct selftest_entry selftests_wizard[] = {
    {"wizardfilter", "[<answer> <adr> <fil> [up [slot]]]",
     "filter emitted by a wizard answer", st_wizardfilter},
    {"wizardscope", "[<question>]",
     "domain scopes the wizard can offer for a host", st_wizardscope},
    {"wizardscopeanswer", "", "host-scope range of a wizard answer",
     st_wizardscopeanswer},
    {"wizardverdict", "[<answer>]", "what a wizard answer applies",
     st_wizardverdict},
    {"wizardprompt", "[<adr> <fil>]", "URL the wizard prompt asks about",
     st_wizardprompt},
    {"wizardinsert", "[<adr> <fil> [answer...] [@ filter...]]",
     "where a wizard answer lands in the filter array", st_wizardinsert},
    {NULL, NULL, NULL, NULL},
};
