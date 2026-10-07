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
/* File: htslines_selftest.c subroutines:                       */
/*       self-tests for the config-file line readers            */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htsselftest_int.h"
#include "htslines.h"

/* The legacy readers write a byte past max at max == 1, hence the slack.
   ST_LINE_BIG holds any line of the corpus, joined or not. */
enum {
  ST_LINE_SLACK = 16,
  ST_LINE_MAXCAP = 64,
  ST_LINE_BIG = 128,
  ST_LINE_MAXREADS = 128
};

/* "\n", "\r", "\t", "\f", "\v", "\0" and "\\" in arg, into buf. */
static size_t st_lines_decode(const char *arg, char *buf, size_t size) {
  static const char from[] = "nrtfv0";
  static const char to[] = "\n\r\t\f\v\0";
  size_t n = 0;

  while (*arg != '\0' && n + 1 < size) {
    char c = *arg++;

    if (c == '\\' && *arg != '\0') {
      const char *const e = strchr(from, *arg);

      c = e != NULL ? to[e - from] : *arg;
      arg++;
    }
    buf[n++] = c;
  }
  buf[n] = '\0';
  return n;
}

/* "t" for HTS_LINE_DROP_TAB, "n" for HTS_LINE_DROP_NUL, "-" for neither. */
static int st_lines_flags(const char *arg) {
  return (strchr(arg, 't') != NULL ? HTS_LINE_DROP_TAB : 0) |
         (strchr(arg, 'n') != NULL ? HTS_LINE_DROP_NUL : 0);
}

static FILE *st_lines_file(const char *data, size_t len) {
  FILE *const fp = tmpfile();

  assertf(fp != NULL);
  assertf(fwrite(data, 1, len, fp) == len);
  rewind(fp);
  return fp;
}

/* Print len bytes of s, control bytes and the backslash escaped. */
static void st_lines_print(const char *s, size_t len) {
  size_t i;

  for (i = 0; i < len; i++) {
    const unsigned char c = (unsigned char) s[i];

    if (c == '\\')
      printf("\\\\");
    else if (c < 0x20 || c >= 0x7f)
      printf("\\x%02x", c);
    else
      putchar(c);
  }
}

/* readline CAP FLAGS TEXT [cpp]: every line a caller's feof() loop gets, as
   [line], or "!" for a cut one, whose content is unspecified. */
static int st_readline(httrackp *opt, int argc, char **argv) {
  char data[4096];
  char line[ST_LINE_MAXCAP + ST_LINE_SLACK];
  const int cap = argc >= 2 ? atoi(argv[0]) : 0;
  /* an empty TEXT reaches us as no argument at all */
  const char *const text = argc >= 3 ? argv[2] : "";
  const hts_boolean cpp = argc >= 4 && strcmp(argv[3], "cpp") == 0;
  FILE *fp;

  (void) opt;
  if (cap < 1 || cap > ST_LINE_MAXCAP) {
    fprintf(stderr, "readline: needs CAP (1..%d) FLAGS TEXT [cpp]\n",
            ST_LINE_MAXCAP);
    return 1;
  }
  fp = st_lines_file(data, st_lines_decode(text, data, sizeof(data)));
  while (!feof(fp)) {
    const int flags = st_lines_flags(argv[1]);
    hts_boolean cut;
    size_t n = (size_t) cap;

    memset(line, '#', sizeof(line));
    if (cpp) {
      cut = hts_readline_cpp(fp, line, (size_t) cap, flags);
      n = strlen(line);
    } else {
      cut = hts_readline(fp, line, (size_t) cap, flags);
      /* the terminator is the last NUL, as a kept NUL is data */
      while (n > 0 && line[n - 1] != '\0')
        n--;
      assertf(n > 0);
      n--;
    }
    /* nothing written past the capacity */
    assertf(line[cap] == '#');
    if (cut) {
      printf("!");
    } else {
      printf("[");
      st_lines_print(line, n);
      printf("]");
    }
  }
  printf("\n");
  fclose(fp);
  return 0;
}

/* ------------------------------------------------------------ */
/* Frozen copies of the old readers, for the differential.      */
/* ------------------------------------------------------------ */

/* Set when a legacy read stopped for want of room, not at the line's end. */
static hts_boolean st_legacy_full;

/* htslib.c's linput() (HTS_LINE_DROP_TAB), rawlinput() (0) and htsserver.h's
   linput() (both), which differ only in the bytes they drop. */
static int legacy_linput(FILE *fp, char *s, int max, int flags) {
  int c;
  int j = 0;

  do {
    c = fgetc(fp);
    if (c != EOF) {
      switch (c) {
      case 13:
        break;
      case 10:
        c = -1;
        break;
      default:
        if (!(((flags & HTS_LINE_DROP_TAB) != 0 && (c == 9 || c == 12)) ||
              ((flags & HTS_LINE_DROP_NUL) != 0 && c == 0)))
          s[j++] = (char) c;
        break;
      }
    }
  } while ((c != -1) && (c != EOF) && (j < (max - 1)));
  if (c != -1 && c != EOF)
    st_legacy_full = HTS_TRUE;
  s[j] = '\0';
  return j;
}

/* htsserver.h's linput_trim(). */
static int legacy_srv_linput_trim(FILE *fp, char *s, int max) {
  int rlen = 0;
  char *ls = (char *) malloct(max + 1);

  s[0] = '\0';
  if (ls) {
    char *a;

    rlen = legacy_linput(fp, ls, max, HTS_LINE_DROP_TAB | HTS_LINE_DROP_NUL);
    if (rlen) {
      while ((rlen > 0) && is_realspace(ls[max(rlen - 1, 0)]))
        ls[--rlen] = '\0';
      a = ls;
      while ((rlen > 0) && ((*a == ' ') || (*a == '\t'))) {
        a++;
        rlen--;
      }
      if (rlen > 0) {
        memcpy(s, a, rlen);
        s[rlen] = '\0';
      }
    }
    freet(ls);
  }
  return rlen;
}

/* htsserver.h's linput_cpp(), the lang catalog reader. */
static int legacy_srv_linput_cpp(FILE *fp, char *s, int max) {
  int rlen = 0;

  s[0] = '\0';
  do {
    int ret;

    if (rlen > 0)
      if (s[rlen - 1] == '\\')
        s[--rlen] = '\0';
    ret = legacy_srv_linput_trim(fp, s + rlen, max - rlen);
    if (ret > 0)
      rlen += ret;
  } while ((s[max(rlen - 1, 0)] == '\\') && (rlen < max));
  return rlen;
}

/* ------------------------------------------------------------ */
/* Differential and cap independence.                           */
/* ------------------------------------------------------------ */

enum { ST_NREADERS = 4, ST_CPP = 3 };

static const char *const st_reader_names[ST_NREADERS] = {
    "linput", "rawlinput", "srv-linput", "srv-cpp"};

static const int st_reader_flags[ST_NREADERS] = {
    HTS_LINE_DROP_TAB, 0, HTS_LINE_DROP_TAB | HTS_LINE_DROP_NUL,
    HTS_LINE_DROP_TAB | HTS_LINE_DROP_NUL};

/* One read by reader r, old or new, at cap into s. Returns the offset after. */
static long st_read(FILE *fp, int r, size_t cap, hts_boolean legacy, char *s,
                    hts_boolean *cut) {
  *cut = HTS_FALSE;
  if (legacy && r == ST_CPP)
    (void) legacy_srv_linput_cpp(fp, s, (int) cap);
  else if (legacy)
    (void) legacy_linput(fp, s, (int) cap, st_reader_flags[r]);
  else if (r == ST_CPP)
    *cut = hts_readline_cpp(fp, s, cap, st_reader_flags[r]);
  else
    *cut = hts_readline(fp, s, cap, st_reader_flags[r]);
  return ftell(fp);
}

struct st_diff_counts {
  unsigned cases, fit, other, failures;
};

static void st_diff_fail(struct st_diff_counts *k, const char *why, int r,
                         size_t cap, const char *data, size_t len) {
  if (++k->failures <= 5) {
    printf("linediff: %s, %s cap %d input [", why, st_reader_names[r],
           (int) cap);
    st_lines_print(data, len);
    printf("]\n");
  }
}

/* Walk data with reader r at cap, as a feof() loop does, into ends[]. Each
   line the old reader read without running out of room must come back
   identical. Returns the number of reads. */
static size_t st_walk(FILE *fp, int r, size_t cap, long *ends,
                      struct st_diff_counts *k, const char *data, size_t len) {
  char s[ST_LINE_BIG + ST_LINE_SLACK];
  char old[ST_LINE_BIG + ST_LINE_SLACK];
  size_t n = 0;
  hts_boolean eof = HTS_FALSE;

  rewind(fp);
  while (!eof && n < ST_LINE_MAXREADS) {
    const long start = ftell(fp);
    hts_boolean cut, unused;
    long old_end;

    memset(s, '#', sizeof(s));
    ends[n++] = st_read(fp, r, cap, HTS_FALSE, s, &cut);
    assertf(s[cap] == '#');
    eof = feof(fp) ? HTS_TRUE : HTS_FALSE;
    fseek(fp, start, SEEK_SET);
    st_legacy_full = HTS_FALSE;
    old_end = st_read(fp, r, cap, HTS_TRUE, old, &unused);
    if (st_legacy_full) {
      k->other++;
    } else {
      k->fit++;
      if (cut || old_end != ends[n - 1] || strcmp(s, old) != 0)
        st_diff_fail(k, "differs from legacy on a line that fits", r, cap, data,
                     len);
    }
    fseek(fp, ends[n - 1], SEEK_SET);
  }
  return n;
}

/* Against the old readers line by line, and the same line boundaries at every
   cap: only a line's content and its cut flag may depend on the cap. */
static void st_diff_one(const char *data, size_t len,
                        struct st_diff_counts *k) {
  FILE *const fp = st_lines_file(data, len);
  int r;

  for (r = 0; r < ST_NREADERS; r++) {
    long big[ST_LINE_MAXREADS];
    const size_t nbig = st_walk(fp, r, ST_LINE_BIG, big, k, data, len);
    size_t cap;

    for (cap = 1; cap <= 24; cap++) {
      long ends[ST_LINE_MAXREADS];
      const size_t n = st_walk(fp, r, cap, ends, k, data, len);

      k->cases++;
      if (n != nbig || memcmp(ends, big, n * sizeof(ends[0])) != 0)
        st_diff_fail(k, "line boundaries depend on the cap", r, cap, data, len);
    }
  }
  fclose(fp);
}

/* The new readers against the old ones, on a hand corpus and a seeded random
   one. */
static int st_linediff(httrackp *opt, int argc, char **argv) {
  static const char *const corpus[] = {
      "",
      "\n",
      "a",
      "ab\r\ncd\r\n",
      "a\n\nb",
      "a\tb\fc",
      "a\\\nb\n",
      "a\\\\\n\nb",
      "abcdef\\\\\n\nghi\njk",
      "  a  \\\n  b \n",
      "\\",
      "a \\ \nb",
      "abcd   \n",
      "abcdefghijklmnopqrstuvwxyz\nnext",
      "\t\t\tab\n",
      "\va\\\v\nb\f\n",
      "a\\\nb\\\nc\\\nd\\\ne\\\nf\nz",
  };
  static const char alphabet[] = "ab \t\r\n\\\f\v";
  struct st_diff_counts k = {0, 0, 0, 0};
  uint32_t seed = 1551;
  size_t i;
  int n;

  (void) opt;
  (void) argc;
  (void) argv;
  for (i = 0; i < sizeof(corpus) / sizeof(corpus[0]); i++)
    st_diff_one(corpus[i], strlen(corpus[i]), &k);
  /* NUL is data too, and the one byte only some readers drop */
  st_diff_one("a\0b\nc", 5, &k);
  for (n = 0; n < 400; n++) {
    char data[96];
    size_t len;

    seed = seed * 1103515245u + 12345u;
    len = (seed >> 16) % sizeof(data);
    for (i = 0; i < len; i++) {
      seed = seed * 1103515245u + 12345u;
      /* one byte in ten a NUL, the rest from the alphabet */
      data[i] = (seed >> 16) % 10 == 0
                    ? '\0'
                    : alphabet[(seed >> 20) % (sizeof(alphabet) - 1)];
    }
    st_diff_one(data, len, &k);
  }
  printf("linediff: %u cases, %u lines fit, %u do not, %u failures\n", k.cases,
         k.fit, k.other, k.failures);
  printf("linediff: %s\n", k.failures == 0 ? "OK" : "FAIL");
  return k.failures == 0 ? 0 : 1;
}

/* ------------------------------------------------------------ */
/* Registry: this module's tests, in the order -#test lists them. */
/* ------------------------------------------------------------ */

const struct selftest_entry selftests_lines[] = {
    {"readline", "<cap> <t|n|tn|-> <text> [cpp]",
     "config-file line reader: [line] per read, ! when cut", st_readline},
    {"linediff", "", "line readers against the legacy ones they replaced",
     st_linediff},
    {NULL, NULL, NULL, NULL},
};
