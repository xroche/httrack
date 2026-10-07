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

/* Room past any capacity under test: the legacy readers write one byte beyond
   max at max == 1. */
enum { ST_LINE_SLACK = 16, ST_LINE_MAXCAP = 64 };

/* "\n", "\r", "\t", "\f", "\v", "\0", "\\" and "\xHH" in arg, into buf. */
static size_t st_lines_decode(const char *arg, char *buf, size_t size) {
  size_t n = 0;

  while (*arg != '\0' && n + 1 < size) {
    char c = *arg++;

    if (c == '\\' && *arg != '\0') {
      unsigned int byte;

      switch (c = *arg++) {
      case 'n':
        c = '\n';
        break;
      case 'r':
        c = '\r';
        break;
      case 't':
        c = '\t';
        break;
      case 'f':
        c = '\f';
        break;
      case 'v':
        c = '\v';
        break;
      case '0':
        c = '\0';
        break;
      case 'x':
        if (sscanf(arg, "%2x", &byte) == 1) {
          c = (char) byte;
          arg += 2;
        }
        break;
      default:
        break;
      }
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

/* readline CAP FLAGS TEXT: every line a caller's feof() loop gets, as [line],
   with "!" after a cut one. */
static int st_readline(httrackp *opt, int argc, char **argv) {
  char data[4096];
  char line[ST_LINE_MAXCAP + ST_LINE_SLACK];
  const int cap = argc >= 2 ? atoi(argv[0]) : 0;
  /* the engine drops an empty argument */
  const char *const text = argc >= 3 ? argv[2] : "";
  const hts_boolean cpp = argc >= 4 && strcmp(argv[3], "cpp") == 0;
  size_t len;
  FILE *fp;

  (void) opt;
  if (cap < 1 || cap > ST_LINE_MAXCAP) {
    fprintf(stderr, "readline: needs CAP (1..%d) FLAGS TEXT [cpp]\n",
            ST_LINE_MAXCAP);
    return 1;
  }
  len = st_lines_decode(text, data, sizeof(data));
  fp = st_lines_file(data, len);
  while (!feof(fp)) {
    size_t n;
    hts_boolean cut;

    memset(line, '#', sizeof(line));
    if (cpp) {
      cut = hts_readline_cpp(fp, line, (size_t) cap, st_lines_flags(argv[1]));
      n = strlen(line);
    } else {
      cut = hts_readline(fp, line, (size_t) cap, st_lines_flags(argv[1]), &n);
    }
    /* nothing written past the capacity */
    assertf(n < (size_t) cap && line[n] == '\0');
    assertf(line[cap] == '#');
    printf("[");
    st_lines_print(line, n);
    printf("]%s", cut ? "!" : "");
  }
  printf("\n");
  fclose(fp);
  return 0;
}

/* ------------------------------------------------------------ */
/* The readers this module replaced, kept to diff against.       */
/* ------------------------------------------------------------ */

/* htslib.c's linput(). */
static int legacy_linput(FILE *fp, char *s, int max) {
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
      case 9:
      case 12:
        break;
      default:
        s[j++] = (char) c;
        break;
      }
    }
  } while ((c != -1) && (c != EOF) && (j < (max - 1)));
  s[j] = '\0';
  return j;
}

/* htslib.c's rawlinput(), the cookies.txt reader. */
static void legacy_rawlinput(FILE *fp, char *s, int max) {
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
        s[j++] = (char) c;
        break;
      }
    }
  } while ((c != -1) && (c != EOF) && (j < (max - 1)));
  s[j++] = '\0';
}

/* htsserver.h's linput(), which also drops NUL. */
static int legacy_srv_linput(FILE *fp, char *s, int max) {
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
      case 0:
      case 9:
      case 12:
        break;
      default:
        s[j++] = (char) c;
        break;
      }
    }
  } while ((c != -1) && (c != EOF) && (j < (max - 1)));
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

    rlen = legacy_srv_linput(fp, ls, max);
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
/* Differential: legacy against new, and new against a model.    */
/* ------------------------------------------------------------ */

enum st_reader {
  ST_LINPUT,
  ST_RAWLINPUT,
  ST_SRV_LINPUT,
  ST_SRV_CPP,
  ST_NREADERS
};

static const char *const st_reader_names[ST_NREADERS] = {
    "linput", "rawlinput", "srv-linput", "srv-cpp"};

static int st_reader_flags(enum st_reader r) {
  switch (r) {
  case ST_LINPUT:
    return HTS_LINE_DROP_TAB;
  case ST_RAWLINPUT:
    return 0;
  default:
    return HTS_LINE_DROP_TAB | HTS_LINE_DROP_NUL;
  }
}

static hts_boolean st_dropped(int c, int flags) {
  return c == '\r' ||
                 ((flags & HTS_LINE_DROP_TAB) != 0 &&
                  (c == '\t' || c == '\f')) ||
                 ((flags & HTS_LINE_DROP_NUL) != 0 && c == '\0')
             ? HTS_TRUE
             : HTS_FALSE;
}

/* Append one line as a caller sees it, up to its first NUL, then a separator.
   A cut line is "!" alone: what it keeps is the readline cases' business. */
static void st_lines_add(String *out, const char *s, hts_boolean cut) {
  if (cut)
    StringAddchar(*out, '!');
  else
    StringMemcat(*out, s, strlen(s));
  StringAddchar(*out, '\036');
}

/* Every line the legacy reader gives a feof() loop. */
static void st_legacy_all(FILE *fp, enum st_reader r, int cap, String *out) {
  char s[ST_LINE_MAXCAP + ST_LINE_SLACK];

  rewind(fp);
  StringClear(*out);
  while (!feof(fp)) {
    switch (r) {
    case ST_LINPUT:
      (void) legacy_linput(fp, s, cap);
      break;
    case ST_RAWLINPUT:
      legacy_rawlinput(fp, s, cap);
      break;
    case ST_SRV_LINPUT:
      (void) legacy_srv_linput(fp, s, cap);
      break;
    default:
      (void) legacy_srv_linput_cpp(fp, s, cap);
      break;
    }
    st_lines_add(out, s, HTS_FALSE);
  }
}

/* Same with the new reader. HTS_TRUE when any line was cut. */
static hts_boolean st_new_all(FILE *fp, enum st_reader r, int cap,
                              String *out) {
  char s[ST_LINE_MAXCAP + ST_LINE_SLACK];
  hts_boolean any = HTS_FALSE;

  rewind(fp);
  StringClear(*out);
  while (!feof(fp)) {
    hts_boolean cut;

    memset(s, '#', sizeof(s));
    if (r == ST_SRV_CPP)
      cut = hts_readline_cpp(fp, s, (size_t) cap, st_reader_flags(r));
    else
      cut = hts_readline(fp, s, (size_t) cap, st_reader_flags(r), NULL);
    assertf(s[cap] == '#');
    if (cut)
      any = HTS_TRUE;
    st_lines_add(out, s, cut);
  }
  return any;
}

/* Trailing whitespace off, then leading spaces and TABs, as linput_trim(). */
static void st_model_trim(String *seg) {
  size_t n = StringLength(*seg);
  size_t i = 0;
  const char *const b = StringBuff(*seg);

  while (n > 0 && is_realspace(b[n - 1]))
    n--;
  while (i < n && (b[i] == ' ' || b[i] == '\t'))
    i++;
  {
    String t = STRING_EMPTY;

    StringMemcat(t, b + i, n - i);
    StringClear(*seg);
    StringMemcat(*seg, StringBuff(t), StringLength(t));
    StringFree(t);
  }
}

static hts_boolean st_ends_backslash(const String *s) {
  return StringLength(*s) > 0 && StringBuff(*s)[StringLength(*s) - 1] == '\\'
             ? HTS_TRUE
             : HTS_FALSE;
}

/* What the new reader r must give a feof() loop over data at capacity cap,
   written without a fixed buffer. "at_size" is set when a read holds exactly
   cap - 1 bytes, where the legacy reader leaves the newline for the next read;
   "cut" when one does not fit. */
static void st_model_all(const char *data, size_t len, enum st_reader r,
                         size_t cap, String *out, hts_boolean *at_size,
                         hts_boolean *any_cut) {
  const int flags = st_reader_flags(r);
  String joined = STRING_EMPTY;
  String seg = STRING_EMPTY;
  hts_boolean cut = HTS_FALSE; /* the current line's */
  size_t pos = 0;

  StringClear(*out);
  StringClear(joined);
  *at_size = *any_cut = HTS_FALSE;
  for (;;) {
    hts_boolean more;
    int tail;
    size_t left;

    if (r == ST_SRV_CPP && !cut && st_ends_backslash(&joined))
      StringPopRight(joined);
    /* one physical line, or nothing once the data is spent */
    StringClear(seg);
    while (pos < len && data[pos] != '\n') {
      if (!st_dropped((unsigned char) data[pos], flags))
        StringAddchar(seg, data[pos]);
      pos++;
    }
    if (!cut) {
      left = cap - StringLength(joined);
      if (StringLength(seg) + 1 > left)
        cut = *any_cut = HTS_TRUE;
      else if (StringLength(seg) > 0 && StringLength(seg) + 1 == left)
        *at_size = HTS_TRUE;
    }
    if (r == ST_SRV_CPP)
      st_model_trim(&seg);
    tail = StringLength(seg) > 0
               ? (unsigned char) StringBuff(seg)[StringLength(seg) - 1]
               : -1;
    StringMemcat(joined, StringBuff(seg), StringLength(seg));
    more = r != ST_SRV_CPP ? HTS_FALSE
           : cut           ? tail == '\\'
                           : st_ends_backslash(&joined);
    if (!more) {
      st_lines_add(out, StringBuff(joined), cut);
      StringClear(joined);
      cut = HTS_FALSE;
      if (pos >= len)
        break;
    }
    if (pos < len)
      pos++; /* the '\n' */
  }
  StringFree(seg);
  StringFree(joined);
}

struct st_diff_counts {
  unsigned cases, same, at_size, split, failures;
};

static hts_boolean st_strings_equal(const String *a, const String *b) {
  return StringLength(*a) == StringLength(*b) &&
                 memcmp(StringBuff(*a), StringBuff(*b), StringLength(*a)) == 0
             ? HTS_TRUE
             : HTS_FALSE;
}

static void st_diff_one(const char *data, size_t len,
                        struct st_diff_counts *k) {
  static const int caps[] = {2, 3, 4, 5, 6, 8, 16, ST_LINE_MAXCAP};
  FILE *const fp = st_lines_file(data, len);
  String legacy = STRING_EMPTY;
  String fresh = STRING_EMPTY;
  String model = STRING_EMPTY;
  size_t c;
  int r;

  for (r = 0; r < ST_NREADERS; r++) {
    for (c = 0; c < sizeof(caps) / sizeof(caps[0]); c++) {
      const size_t cap = (size_t) caps[c];
      const char *why = NULL;
      hts_boolean at_size, cut;

      k->cases++;
      st_model_all(data, len, (enum st_reader) r, cap, &model, &at_size, &cut);
      st_legacy_all(fp, (enum st_reader) r, (int) cap, &legacy);
      (void) st_new_all(fp, (enum st_reader) r, (int) cap, &fresh);
      if (!st_strings_equal(&fresh, &model))
        why = "differs from the model";
      else if (st_strings_equal(&fresh, &legacy))
        k->same++;
      else if (!at_size && !cut)
        why = "differs from legacy where every line fits";
      else if (cut) {
        /* allowlisted: legacy reads the tail of a line as more lines */
        k->split++;
      } else {
        /* allowlisted: legacy reads an empty line after one of cap - 1 bytes */
        k->at_size++;
      }
      if (why != NULL) {
        k->failures++;
        if (k->failures <= 5) {
          printf("linediff: %s, %s cap %d input [", why, st_reader_names[r],
                 (int) cap);
          st_lines_print(data, len);
          printf("]\n");
        }
      }
    }
  }
  StringFree(legacy);
  StringFree(fresh);
  StringFree(model);
  fclose(fp);
}

/* The new readers against the legacy ones, on a hand corpus and a seeded
   random one: identical wherever every line fits, and otherwise only the two
   allowlisted differences. */
static int st_linediff(httrackp *opt, int argc, char **argv) {
  static const char *const corpus[] = {
      "",
      "\n",
      "a",
      "a\n",
      "ab\ncd",
      "ab\r\ncd\r\n",
      "a\rb\nc",
      "a\n\nb",
      "a\tb\fc",
      "a\\\nb\n",
      "a\\\\\n\nb",
      "  a  \\\n  b \n",
      "\\",
      "\\\n",
      "a \\ \nb",
      "abc",
      "abcd\n",
      "abcde\nx",
      "abcdefghijklmnopqrstuvwxyz\nnext",
      "\t\t\tab\n",
      "a\\\nb\\\nc\\\nd\\\ne\\\nf\nz",
  };
  static const char alphabet[] = "ab \t\r\n\\\f\v";
  struct st_diff_counts k = {0, 0, 0, 0, 0};
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
  st_diff_one("\0\0\0\0\0\0\n\0", 8, &k);
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
  printf("linediff: %u cases, %u same, %u at size, %u split, %u failures\n",
         k.cases, k.same, k.at_size, k.split, k.failures);
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
