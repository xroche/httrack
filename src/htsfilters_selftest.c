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
/* File: htsfilters_selftest.c subroutines:                     */
/*       self-tests for the wildcard filter matcher             */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htsselftest_int.h"

/* ------------------------------------------------------------ */
/* The individual self-tests. Each runs over argv[0..argc-1] and returns the */
/* process exit code (0 == success); a result line goes to stdout. */
/* ------------------------------------------------------------ */

static int st_filter(httrackp *opt, int argc, char **argv) {
  char *str, *pat;
  int matched;

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "filter: needs a filter pattern and a string\n");
    return 1;
  }
  /* exact-size heap copies so a sanitizer traps an over-read; a missing
     subject means "" (not reachable as a CLI arg) */
  str = strdupt(argc >= 2 ? argv[1] : "");
  pat = strdupt(argv[0]);
  matched = strjoker(str, pat, NULL, NULL) != NULL;
  printf("%s does %s %s\n", str, matched ? "match" : "NOT match", argv[0]);
  freet(str);
  freet(pat);
  return 0;
}

/* Size-aware filter verdict via fa_strjoker: a negative <size> means the size
   is still unknown (scan time), so a size rule like -*.jpg*[<10] must stay
   neutral. */
static int st_filtersize(httrackp *opt, int argc, char **argv) {
  LLint sz;
  int size_flag = 0, verdict, known;

  (void) opt;
  if (argc < 3) {
    fprintf(stderr, "filtersize: needs <size> <string> <filter> [filter...]\n");
    return 1;
  }
  known = (argv[0][0] != '-'); /* "-1"/"-" => size unknown */
  sz = -1;
  if (known)
    sscanf(argv[0], LLintP, &sz);
  verdict = fa_strjoker(0, &argv[2], argc - 2, argv[1], known ? &sz : NULL,
                        known ? &size_flag : NULL, NULL);
  printf("verdict=%s size_flag=%d\n",
         verdict > 0   ? "allowed"
         : verdict < 0 ? "forbidden"
                       : "unknown",
         size_flag);
  return 0;
}

/* Mime-type filter verdict via fa_strjoker(type=1): only mime: rules apply. */
static int st_filtermime(httrackp *opt, int argc, char **argv) {
  int verdict;

  (void) opt;
  if (argc < 2) {
    fprintf(stderr, "filtermime: needs <mime> <filter> [filter...]\n");
    return 1;
  }
  verdict = fa_strjoker(1, &argv[1], argc - 1, argv[0], NULL, NULL, NULL);
  printf("verdict=%s\n", verdict > 0   ? "allowed"
                         : verdict < 0 ? "forbidden"
                                       : "unknown");
  return 0;
}

/* SplitMix64: deterministic, platform-independent case generator. */
static uint64_t st_mix64(uint64_t *state) {
  uint64_t z = (*state += UINT64_C(0x9E3779B97F4A7C15));

  z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4B5B9);
  z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
  return z ^ (z >> 31);
}

/* Differential test: memoized strjoker vs the no-memo oracle on seeded random
   pattern/subject/size cases must agree on result, *size and *size_flag. */
static int st_filtermemo(httrackp *opt, int argc, char **argv) {
  static const char *const pieces[] = {
      "a",       "b",        "A",    "c",      ".",       "/",
      "?",       "*",        "*[a]", "*[ab]",  "*[a-c]",  "*[A-Z]",
      "*[<5]",   "*[>5]",    "*(a)", "*(a,b)", "*[file]", "*[path]",
      "*[name]", "*[param]", "*[]",  "*[\\a]", "*[a-"};
  static const char subject_chars[] = "abAc./?";
  uint64_t rng = UINT64_C(0x501);
  int iters = 20000, matched = 0, unmatched = 0;
  int it;

  (void) opt;
  if (argc >= 1)
    sscanf(argv[0], "%d", &iters);
  for (it = 0; it < iters; it++) {
    char pat[64], str[16];
    size_t pl = 0;
    const int npieces = (int) (st_mix64(&rng) % 5);
    const int slen = (int) (st_mix64(&rng) % 13);
    const int szsel = (int) (st_mix64(&rng) % 4); /* none/unknown/3/20 */
    LLint sz1, sz2, off1, off2;
    int i, flag1 = 0, flag2 = 0;
    char *hpat, *hstr;
    const char *adr;

    for (i = 0; i < npieces; i++) {
      const char *p =
          pieces[st_mix64(&rng) % (sizeof(pieces) / sizeof(pieces[0]))];
      const size_t len = strlen(p);

      if (len >= sizeof(pat) - pl)
        break;
      memcpy(pat + pl, p, len);
      pl += len;
    }
    pat[pl] = '\0';
    for (i = 0; i < slen; i++)
      str[i] = subject_chars[st_mix64(&rng) % (sizeof(subject_chars) - 1)];
    str[slen] = '\0';
    sz1 = sz2 = (szsel <= 1) ? -1 : (szsel == 2) ? 3 : 20;
    /* exact-size heap copies per run so a sanitizer traps any over-read */
    hpat = strdupt(pat);
    hstr = strdupt(str);
    adr = strjoker(hstr, hpat, szsel ? &sz1 : NULL, szsel ? &flag1 : NULL);
    off1 = adr != NULL ? (LLint) (adr - hstr) : -1;
    freet(hpat);
    freet(hstr);
    hpat = strdupt(pat);
    hstr = strdupt(str);
    adr =
        strjoker_nomemo(hstr, hpat, szsel ? &sz2 : NULL, szsel ? &flag2 : NULL);
    off2 = adr != NULL ? (LLint) (adr - hstr) : -1;
    freet(hpat);
    freet(hstr);
    if (off1 != off2 || sz1 != sz2 || flag1 != flag2) {
      printf("filtermemo MISMATCH pat=[%s] str=[%s] szsel=%d: "
             "off %d/%d size %d/%d flag %d/%d\n",
             pat, str, szsel, (int) off1, (int) off2, (int) sz1, (int) sz2,
             flag1, flag2);
      return 1;
    }
    if (off1 >= 0)
      matched++;
    else
      unmatched++;
  }
  /* both polarities must actually occur or the test proves nothing */
  assertf(matched > 0 && unmatched > 0);
  printf("filtermemo: %d cases OK\n", iters);
  return 0;
}

/* Merged two-form filter verdict via fa_strjoker_dual (see htsfilters.h). */
static int st_filterdual(httrackp *opt, int argc, char **argv) {
  int depth = -1, verdict;

  (void) opt;
  if (argc < 3) {
    fprintf(stderr,
            "filterdual: needs <string1> <string2> <filter> [filter...]\n");
    return 1;
  }
  verdict = fa_strjoker_dual(0, &argv[2], argc - 2, argv[0], argv[1], NULL,
                             NULL, &depth);
  printf("verdict=%s rule=%d\n",
         verdict > 0   ? "allowed"
         : verdict < 0 ? "forbidden"
                       : "unknown",
         depth);
  return 0;
}

/* Length/work caps stop a hostile pattern stack-overflowing or hanging the
   process (OSS-Fuzz 5060751291908096 / 5745936014573568). */
static int st_filterbounds(httrackp *opt, int argc, char **argv) {
  const size_t big = 100000; /* well past the length cap */
  const size_t stars = 1023; /* pattern len 2047, under the length cap */
  const size_t subjlen = 2048;
  char *subj = malloct(big + 1);
  char *pat = malloct(2 * stars + 2);
  size_t steps = 0, maxsteps = 0, depth = 0, maxdepth = 0, i;

  (void) opt;
  (void) argc;
  (void) argv;
  memset(subj, 'a', big);
  subj[big] = '\0';
  /* '*' matches anything, but an over-length subject trips the length cap */
  assertf(strjoker(subj, "*", NULL, NULL) == NULL);
  assertf(strjokerfind(subj, "*") == NULL);
  /* Star-heavy dead-end at the length cap: unbounded it runs ~1.26e9 memo-steps
     (~6s). */
  for (i = 0; i < stars; i++) {
    pat[2 * i] = '*';
    pat[2 * i + 1] = 'a';
  }
  pat[2 * stars] = 'b'; /* never matches an all-'a' subject */
  pat[2 * stars + 1] = '\0';
  subj[subjlen] = '\0';
  /* Budget must fire and hold: steps > cap (deleting the budget zeroes the
     counter that is the enforcement), steps < 10*cap (unbudgeted ~1.26e9). */
  assertf(strjoker_bounds(subj, pat, &steps, &maxsteps, &depth, &maxdepth) ==
          NULL);
  assertf(steps > maxsteps && steps < 10 * maxsteps);
  /* Depth caps the stack: uncapped this recurses 2046 frames, ~900KB (#574). */
  assertf(depth == maxdepth);
  assertf(strjokerfind(subj, pat) == NULL);
  /* Pin the cap from below: 32 segments must still match, so a cap set so low
     it would break real multi-segment filters (which use far fewer) fails. */
  for (i = 0; i < 32; i++) {
    pat[2 * i] = '*';
    pat[2 * i + 1] = 'a';
  }
  pat[64] = '\0';
  memset(subj, 'a', 32);
  subj[32] = '\0';
  assertf(strjoker(subj, pat, NULL, NULL) != NULL);
  /* Same pin for the class-branch shape users actually write (*[..]), against a
     long subject: it must match with room to spare under the work cap. */
  {
    const char *seg = "*[A-Z,a-z,0-9]";
    const size_t seglen = strlen(seg), nseg = 16;

    for (i = 0; i < (int) nseg; i++)
      memcpy(pat + i * seglen, seg, seglen);
    pat[nseg * seglen] = '\0';
    memset(subj, 'a', 512);
    subj[512] = '\0';
    assertf(strjoker_bounds(subj, pat, &steps, &maxsteps, NULL, NULL) != NULL);
    assertf(steps < maxsteps);
  }
  freet(pat);
  freet(subj);
  printf("filterbounds: OK\n");
  return 0;
}

/* #1270: filters_insert() refuses a rule the matcher would never read, and
   says so, instead of storing one that can never fire. */
static int st_filtercap(httrackp *opt, int argc, char **argv) {
  const htsfilters saved = opt->filters;
  const int savedwizard = opt->wizard_filters;
  const int saveddebug = opt->debug;
  FILE *const projectlog = opt->log;
  char BIGSTK atcap[HTS_FILTER_MAXLEN + 1];   /* "+a..a*", the longest rule */
  char BIGSTK overcap[HTS_FILTER_MAXLEN + 2]; /* the same, one byte too long */
  char BIGSTK toolong[STRJOKER_MAXLEN + 3];   /* and one the matcher skips */
  char BIGSTK subject[HTS_FILTER_MAXLEN];     /* a URL all three would match */
  char BIGSTK line[STRJOKER_MAXLEN + 256]; /* room for a warning quoting one */
  char **filters = NULL;
  int filptr = 0;
  int taken, verdict, warned;

  (void) argc;
  (void) argv;
  memset(atcap, 'a', sizeof(atcap) - 1);
  atcap[0] = '+';
  atcap[sizeof(atcap) - 2] = '*';
  atcap[sizeof(atcap) - 1] = '\0';
  memset(overcap, 'a', sizeof(overcap) - 1);
  overcap[0] = '-';
  overcap[sizeof(overcap) - 2] = '*';
  overcap[sizeof(overcap) - 1] = '\0';
  memset(toolong, 'a', sizeof(toolong) - 1);
  toolong[0] = '-';
  toolong[sizeof(toolong) - 2] = '*';
  toolong[sizeof(toolong) - 1] = '\0';
  memset(subject, 'a', sizeof(subject) - 1);
  subject[sizeof(subject) - 1] = '\0';
  assertf(strlen(atcap) == HTS_FILTER_MAXLEN);
  assertf(strlen(overcap) == HTS_FILTER_MAXLEN + 1);
  assertf(strlen(toolong) > STRJOKER_MAXLEN);
  assertf(strlen(subject) <= STRJOKER_MAXLEN); /* else none could match */
  assertf(strjoker(subject, toolong + 1, NULL, NULL) == NULL);

  assertf(filters_init(&filters, opt->maxfilter, 0) != 0);
  opt->filters.filters = &filters;
  opt->filters.filptr = &filptr;
  opt->wizard_filters = 0;
  opt->debug = LOG_NOTICE;
/* non-zero, so a stray write into the slot above the last rule shows up */
#define POISON "-poison/*"
/* offer `pattern` to the array, and report what it did with it */
#define TRY(label, pattern)                                                    \
  do {                                                                         \
    FILE *const log = tmpfile();                                               \
    char want[64];                                                             \
    assertf(log != NULL);                                                      \
    opt->log = log;                                                            \
    taken = filters_insert(opt, filptr, (pattern));                            \
    verdict = fa_strjoker(0, filters, filptr, subject, NULL, NULL, NULL);      \
    rewind(log);                                                               \
    if (fgets(line, (int) sizeof(line), log) == NULL)                          \
      line[0] = '\0';                                                          \
    /* the reason, this rule's own length, and the rule itself */              \
    snprintf(want, sizeof(want), "%d bytes", (int) strlen(pattern));           \
    warned = strstr(line, "could never match") != NULL &&                      \
             strstr(line, want) != NULL && strstr(line, (pattern)) != NULL;    \
    assertf(line[0] == '\0' || warned); /* nothing else may be logged */       \
    fclose(log);                                                               \
    printf("%s: stored=%d rules=%d verdict=%d warned=%d\n", (label),           \
           taken != 0, filptr, verdict, warned);                               \
  } while (0)
/* a refusal leaves the count, the verdict and the slot above it untouched */
#define REFUSED()                                                              \
  do {                                                                         \
    assertf(!taken && filptr == 1 && verdict == 1 && warned);                  \
    assertf(strcmp(filters[0], atcap) == 0);                                   \
    assertf(strcmp(filters[1], POISON) == 0);                                  \
  } while (0)

  /* a rule at the cap is stored, silently, and still matches; refusing one byte
     early would be silent too */
  TRY("at the cap", atcap);
  assertf(taken && filptr == 1 && verdict == 1 && !warned);
  assertf(strcmp(filters[0], atcap) == 0);

  /* last match wins, so a stored overcap rule would forbid what the first
     allows: the verdict below is what proves it is really absent */
  strlcpybuff(filters[1], POISON, HTS_FILTER_SLOT_SIZE);
  TRY("one past the cap", overcap);
  REFUSED();
  TRY("past the matcher", toolong);
  REFUSED();

#undef REFUSED
#undef TRY
#undef POISON
  opt->log = projectlog;
  opt->debug = saveddebug;
  freet(filters[0]);
  freet(filters);
  opt->filters = saved;
  opt->wizard_filters = savedwizard;
  /* the two independent caps a rule meets, so a test reads them instead of
     hardcoding a number that drifts from the engine (#1288) */
  printf("filtercap: rule=%d argv=%d\n", (int) HTS_FILTER_MAXLEN,
         (int) HTS_CDLMAXSIZE);
  printf("filtercap self-test OK\n");
  return 0;
}

#undef POISON

/* ------------------------------------------------------------ */
/* Registry: this module's tests, in the order -#test lists them. */
/* ------------------------------------------------------------ */

const struct selftest_entry selftests_filters[] = {
    {"filter", "<pattern> <string>", "match a string against a wildcard filter",
     st_filter},
    {"filtersize", "<size> <string> <filter>...",
     "size-aware filter verdict (negative size = unknown/scan time)",
     st_filtersize},
    {"filtermime", "<mime> <filter>...",
     "mime-type filter verdict (fa_strjoker type=1)", st_filtermime},
    {"filtermemo", "[iterations]",
     "memoized vs unmemoized matcher differential", st_filtermemo},
    {"filterdual", "<string1> <string2> <filter>...",
     "merged two-form filter verdict (fa_strjoker_dual)", st_filterdual},
    {"filterbounds", "", "matcher length/work caps reject hostile patterns",
     st_filterbounds},
    {"filtercap", "", "an over-long filter rule is refused, not stored dead",
     st_filtercap},
    {NULL, NULL, NULL, NULL},
};
