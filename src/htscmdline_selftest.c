/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998 Xavier Roche and other contributors

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
/* File: htscmdline_selftest.c subroutines:                     */
/*       self-tests for the argument quoting grammar            */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htsselftest_int.h"
#include "htscmdline.h"

#include <limits.h>
#include <stdint.h>

/* ------------------------------------------------------------ */
/* Frozen copies of the code hts_split_args() and hts_quote_arg() replaced. */
/* ------------------------------------------------------------ */

/* hts_split_cmdline() before it became a hts_split_args() flag set. */
static char **legacy_split_cmdline(char *cmd, int *nargs) {
  size_t nsep = 0;
  size_t capacity;
  size_t r;
  size_t w;
  int argc = 0;
  hts_boolean quoted = HTS_FALSE;
  char **argv;

  *nargs = 0;
  for (r = 0; cmd[r] != '\0'; r++) {
    if (cmd[r] == '\t' || cmd[r] == '\r' || cmd[r] == '\n') {
      cmd[r] = ' ';
    }
    if (cmd[r] == ' ') {
      nsep++;
    }
  }
  if (nsep > (size_t) INT_MAX - 1 || nsep > SIZE_MAX / sizeof(char *) - 2) {
    return NULL;
  }
  capacity = nsep + 2;
  argv = (char **) malloct(capacity * sizeof(char *));
  if (argv == NULL) {
    return NULL;
  }
  argv[argc++] = cmd;
  for (r = 0, w = 0; cmd[r] != '\0';) {
    if (quoted && cmd[r] == '\\' &&
        (cmd[r + 1] == '\\' || cmd[r + 1] == '\"')) {
      r++;
      cmd[w++] = cmd[r++];
    } else if (cmd[r] == '\"') {
      quoted = !quoted;
      cmd[w++] = cmd[r++];
    } else if (cmd[r] == ' ' && !quoted) {
      cmd[w++] = '\0';
      assertf((size_t) argc < capacity - 1);
      argv[argc++] = cmd + w;
      r++;
    } else {
      cmd[w++] = cmd[r++];
    }
  }
  cmd[w] = '\0';
  argv[argc] = NULL;
  *nargs = argc;
  return argv;
}

/* htscore.c's next_token(), the doit.log reader's scanner. */
static char *legacy_next_token(char *p, int flag) {
  int detect = 0;
  int quote = 0;

  p--;
  do {
    p++;
    if (flag && (*p == '\\')) {
      if (quote) {
        char c = '\0';

        if (*(p + 1) == '\\')
          c = '\\';
        else if (*(p + 1) == '"')
          c = '"';
        if (c) {
          *p = c;
          memmove(p + 1, p + 2, strlen(p + 2) + 1);
        }
      }
    } else if (*p == 34) {
      memmove(p, p + 1, strlen(p + 1) + 1);
      p--;
      quote = !quote;
    } else if (*p == 32) {
      if (!quote)
        detect = 1;
    } else if (*p == '\0') {
      p = NULL;
      detect = 1;
    }
  } while (!detect);
  return p;
}

/* htscoremain.c's doit.log reader loop, appending each token and a '\n'. */
static void legacy_doit_read(char *buff, String *out, int *ntok) {
  char *p = buff, *lastp;

  *ntok = 0;
  do {
    int quoted;

    lastp = p;
    quoted = (p != NULL && *p == '"');
    if (p) {
      p = legacy_next_token(p, 1);
      if (p) {
        *p = 0;
        p++;
      }
    }
    if (lastp) {
      if (strnotempty(lastp) || quoted) {
        StringCat(*out, lastp);
        StringAddchar(*out, '\n');
        (*ntok)++;
      }
    }
  } while (lastp != NULL);
}

/* htscoremain.c's doit.log writer, for one argument. */
static void legacy_doit_write(String *out, const char *arg) {
  if ((strchr(arg, ' ') != NULL) || (strchr(arg, '"') != NULL) ||
      (strchr(arg, '\\') != NULL)) {
    size_t j;

    StringAddchar(*out, '"');
    for (j = 0; arg[j] != '\0'; j++) {
      if (arg[j] == 34)
        StringCat(*out, "\\\"");
      else if (arg[j] == '\\')
        StringCat(*out, "\\\\");
      else
        StringAddchar(*out, arg[j]);
    }
    StringAddchar(*out, '"');
  } else if (strnotempty(arg) == 0) {
    StringCat(*out, "\"\"");
  } else {
    StringCat(*out, arg);
  }
}

/* htsserver.c's cat_cmdline_argn() without its HTML step, which runs after. */
static void legacy_cmdline_argn(String *out, const char *value, size_t len) {
  size_t i;

  for (i = 0; i < len; i++) {
    if (value[i] == '\\' || value[i] == '\"') {
      StringCat(*out, "\\");
    }
    StringMemcat(*out, &value[i], 1);
  }
}

/* htscoremain.c's argv quote strip, HTS_FALSE for "Missing quote". */
static hts_boolean legacy_unquote(char *arg) {
  if (arg[0] == '"') {
    char tempo[HTS_CDLMAXSIZE];

    strcpybuff(tempo, arg + 1);
    if (hts_lastchar(tempo) != '"') {
      return HTS_FALSE;
    }
    hts_choplastchar(tempo);
    strlcpybuff(arg, tempo, strlen(arg) + 1);
  }
  return HTS_TRUE;
}

/* ------------------------------------------------------------ */
/* Helpers */
/* ------------------------------------------------------------ */

/* Windows stdout is text mode, so control bytes are printed escaped. */
static void cat_shown(String *out, const char *s, size_t len) {
  size_t i;

  for (i = 0; i < len; i++) {
    const unsigned char c = (unsigned char) s[i];

    if (c == '\t') {
      StringCat(*out, "\\t");
    } else if (c == '\r') {
      StringCat(*out, "\\r");
    } else if (c == '\n') {
      StringCat(*out, "\\n");
    } else if (c < 0x20) {
      char hex[8];

      snprintf(hex, sizeof(hex), "\\x%02x", c);
      StringCat(*out, hex);
    } else {
      StringAddchar(*out, (char) c);
    }
  }
}

/* The vector as one string, one argument per '\n'-terminated line. */
static void cat_vector(String *out, char **argv, int argc) {
  int i;

  for (i = 0; i < argc; i++) {
    StringCat(*out, argv[i]);
    StringAddchar(*out, '\n');
  }
}

static uint32_t quote_rand(uint32_t *seed) {
  *seed = *seed * 1103515245u + 12345u;
  return *seed >> 16;
}

/* A random string over the bytes the grammar treats specially. */
static void quote_random(String *out, uint32_t *seed, size_t maxlen) {
  static const char alphabet[] = "ab \"\\\t\r\n,=<&'-\xe9";
  const size_t len = quote_rand(seed) % (maxlen + 1);
  size_t i;

  for (i = 0; i < len; i++) {
    StringAddchar(*out, alphabet[quote_rand(seed) % (sizeof(alphabet) - 1)]);
  }
}

typedef struct {
  unsigned long cases;
  unsigned long failures;
} quote_stats;

static void quote_fail(quote_stats *st, const char *site, const char *in,
                       const char *old, const char *new) {
  st->failures++;
  if (st->failures <= 20) {
    String msg = STRING_EMPTY;

    StringCat(msg, site);
    StringCat(msg, " differs on '");
    cat_shown(&msg, in, strlen(in));
    StringCat(msg, "': old '");
    cat_shown(&msg, old, strlen(old));
    StringCat(msg, "' new '");
    cat_shown(&msg, new, strlen(new));
    StringCat(msg, "'");
    printf("%s\n", StringBuff(msg));
    fflush(stdout); /* a later abort must not swallow it */
    StringFree(msg);
  }
}

/* Every old site against its replacement, on one input. */
static void quotediff_one(quote_stats *st, const char *in) {
  const size_t size = strlen(in) + 1;
  char *a = malloct(size), *b = malloct(size);
  String o = STRING_EMPTY, n = STRING_EMPTY;
  char **va, **vb;
  int na = 0, nb = 0;

  assertf(a != NULL && b != NULL);
  StringClear(o);
  StringClear(n);

  /* the WebHTTrack command line */
  memcpy(a, in, size);
  memcpy(b, in, size);
  va = legacy_split_cmdline(a, &na);
  vb = hts_split_cmdline(b, &nb);
  assertf(va != NULL && vb != NULL);
  cat_vector(&o, va, na);
  cat_vector(&n, vb, nb);
  st->cases++;
  if (na != nb || strcmp(StringBuff(o), StringBuff(n)) != 0)
    quote_fail(st, "hts_split_cmdline", in, StringBuff(o), StringBuff(n));
  freet(va);
  freet(vb);
  StringClear(o);
  StringClear(n);

  /* the doit.log line */
  memcpy(a, in, size);
  memcpy(b, in, size);
  legacy_doit_read(a, &o, &na);
  vb = hts_split_args(b, &nb, HTS_SPLIT_STRIP_QUOTES | HTS_SPLIT_DROP_EMPTY);
  assertf(vb != NULL);
  cat_vector(&n, vb, nb);
  st->cases++;
  if (na != nb || strcmp(StringBuff(o), StringBuff(n)) != 0)
    quote_fail(st, "doit.log reader", in, StringBuff(o), StringBuff(n));
  freet(vb);
  StringClear(o);
  StringClear(n);

  /* the doit.log writer */
  legacy_doit_write(&o, in);
  hts_quote_arg(&n, in);
  st->cases++;
  if (strcmp(StringBuff(o), StringBuff(n)) != 0)
    quote_fail(st, "doit.log writer", in, StringBuff(o), StringBuff(n));
  StringClear(o);
  StringClear(n);

  /* the WebHTTrack argument escape */
  legacy_cmdline_argn(&o, in, strlen(in));
  hts_escape_arg(&n, in, strlen(in));
  st->cases++;
  if (StringLength(o) != StringLength(n) ||
      memcmp(StringBuff(o), StringBuff(n), StringLength(o)) != 0)
    quote_fail(st, "argument escape", in, StringBuff(o), StringBuff(n));

  /* the argv quote strip */
  if (size <= HTS_CDLMAXSIZE) {
    hts_boolean ro, rn;

    memcpy(a, in, size);
    memcpy(b, in, size);
    ro = legacy_unquote(a);
    rn = hts_unquote_arg(b);
    st->cases++;
    if (ro != rn || (ro && strcmp(a, b) != 0))
      quote_fail(st, "argv unquote", in, ro ? a : "(missing quote)",
                 rn ? b : "(missing quote)");
  }

  StringFree(o);
  StringFree(n);
  freet(a);
  freet(b);
}

/* ------------------------------------------------------------ */
/* Self-tests */
/* ------------------------------------------------------------ */

/* No difference is intended: the shared grammar keeps each site's behavior. */
static int st_quotediff(httrackp *opt, int argc, char **argv) {
  static const char *const hand[] = {"",
                                     " ",
                                     "  ",
                                     "a",
                                     "a b",
                                     "a  b",
                                     " a",
                                     "a ",
                                     "\"\"",
                                     "\"\"\"\"",
                                     "a \"\" b",
                                     "\"a b\"",
                                     "a\"b c\"d",
                                     "\"a\\\"b\\\\c\"",
                                     "\"a\\",
                                     "a\\",
                                     "\\",
                                     "\\\"",
                                     "\"\\\"",
                                     "\"ab",
                                     "\"",
                                     "ab\"",
                                     "a\\ b",
                                     "\"a\\ b\"",
                                     "\"a\\nb\"",
                                     "a\tb",
                                     "a\rb\nc",
                                     "\"a\tb\"",
                                     "-F \"Mozilla 5.0\" -c8",
                                     "httrack -*\\** +*.png",
                                     "httrack \"C:\\\\dir\\\\sub\"",
                                     "-O \"/a b\" http://x/ \"\"",
                                     "\"(none)\"",
                                     "\"\"x\"\"",
                                     "a,=b <&'",
                                     "\xe9t\xe9",
                                     NULL};
  quote_stats st = {0, 0};
  uint32_t seed = 548;
  size_t i;
  int r;

  (void) opt;
  (void) argc;
  (void) argv;
  for (i = 0; hand[i] != NULL; i++)
    quotediff_one(&st, hand[i]);
  /* a run past the argv strip's stack buffer is skipped by that check only */
  {
    String big = STRING_EMPTY;

    StringAddchar(big, '"');
    for (i = 0; i < HTS_CDLMAXSIZE + 8; i++)
      StringAddchar(big, i % 7 == 0 ? ' ' : 'x');
    StringAddchar(big, '"');
    quotediff_one(&st, StringBuff(big));
    StringFree(big);
  }
  for (r = 0; r < 20000; r++) {
    String in = STRING_EMPTY;

    quote_random(&in, &seed, 24);
    quotediff_one(&st, StringLength(in) != 0 ? StringBuff(in) : "");
    StringFree(in);
  }
  printf("quotediff: %lu cases, %lu failures\n", st.cases, st.failures);
  if (st.failures != 0)
    return 1;
  printf("quotediff: OK\n");
  return 0;
}

/* split(quote(argv)) == argv, on both the doit.log and the WebHTTrack path. */
static int st_quoteprop(httrackp *opt, int argc, char **argv) {
  quote_stats st = {0, 0};
  uint32_t seed = 5480;
  int r;

  (void) opt;
  (void) argc;
  (void) argv;
  for (r = 0; r < 20000; r++) {
    const int n = (int) (quote_rand(&seed) % 6);
    String args[6];
    String want = STRING_EMPTY, wantweb = STRING_EMPTY;
    String line = STRING_EMPTY, web = STRING_EMPTY, got = STRING_EMPTY;
    char **v;
    int i, nv = 0;

    StringClear(want);
    StringClear(wantweb);
    StringClear(line);
    StringClear(web);
    StringClear(got);

    /* doit.log: hts_quote_arg() joined by spaces */
    for (i = 0; i < n; i++) {
      size_t k;

      StringInit(args[i]);
      StringClear(args[i]);
      quote_random(&args[i], &seed, 10);
      if (i != 0)
        StringAddchar(line, ' ');
      hts_quote_arg(&line, StringBuff(args[i]));
      StringCat(want, StringBuff(args[i]));
      StringAddchar(want, '\n');
      /* the command line folds TAB, CR and LF to a space, even quoted */
      StringCat(web, " \"");
      hts_escape_arg(&web, StringBuff(args[i]), StringLength(args[i]));
      StringAddchar(web, '\"');
      for (k = 0; k < StringLength(args[i]); k++) {
        const char c = StringBuff(args[i])[k];

        StringAddchar(wantweb, c == '\t' || c == '\r' || c == '\n' ? ' ' : c);
      }
      StringAddchar(wantweb, '\n');
    }
    v = hts_split_args(StringBuffRW(line), &nv,
                       HTS_SPLIT_STRIP_QUOTES | HTS_SPLIT_DROP_EMPTY);
    assertf(v != NULL);
    cat_vector(&got, v, nv);
    freet(v);
    st.cases++;
    if (nv != n || strcmp(StringBuff(got), StringBuff(want)) != 0)
      quote_fail(&st, "doit.log round trip", StringBuff(want), StringBuff(want),
                 StringBuff(got));

    /* WebHTTrack: "prog" then each argument quoted, unquoted by the engine */
    StringClear(got);
    {
      String cmd = STRING_EMPTY;

      StringCat(cmd, "prog");
      StringCat(cmd, StringBuff(web));
      v = hts_split_cmdline(StringBuffRW(cmd), &nv);
      assertf(v != NULL);
      for (i = 1; i < nv; i++) {
        if (!hts_unquote_arg(v[i]))
          StringCat(got, "(missing quote)");
        StringCat(got, v[i]);
        StringAddchar(got, '\n');
      }
      freet(v);
      StringFree(cmd);
    }
    st.cases++;
    if (nv != n + 1 || strcmp(StringBuff(got), StringBuff(wantweb)) != 0)
      quote_fail(&st, "command line round trip", StringBuff(wantweb),
                 StringBuff(wantweb), StringBuff(got));

    for (i = 0; i < n; i++)
      StringFree(args[i]);
    StringFree(want);
    StringFree(wantweb);
    StringFree(line);
    StringFree(web);
    StringFree(got);
  }
  printf("quoteprop: %lu cases, %lu failures\n", st.cases, st.failures);
  if (st.failures != 0)
    return 1;
  printf("quoteprop: OK\n");
  return 0;
}

/* Print hts_split_args(<line>) under FLAGS, a mix of f(old), s(trip) and
   d(rop), or "-" for none. */
static int st_splitargs(httrackp *opt, int argc, char **argv) {
  String out = STRING_EMPTY;
  char *line;
  char **v;
  int flags = 0, nv = 0, i;

  (void) opt;
  if (argc != 2) {
    fprintf(stderr, "usage: -#test=splitargs <flags> <line>\n");
    return 1;
  }
  flags |= strchr(argv[0], 'f') != NULL ? HTS_SPLIT_FOLD_WS : 0;
  flags |= strchr(argv[0], 's') != NULL ? HTS_SPLIT_STRIP_QUOTES : 0;
  flags |= strchr(argv[0], 'd') != NULL ? HTS_SPLIT_DROP_EMPTY : 0;
  line = strdupt(argv[1]);
  assertf(line != NULL);
  v = hts_split_args(line, &nv, flags);
  assertf(v != NULL && v[nv] == NULL);
  for (i = 0; i < nv; i++) {
    StringAddchar(out, '[');
    cat_shown(&out, v[i], strlen(v[i]));
    StringAddchar(out, ']');
  }
  printf("%d:%s\n", nv, StringLength(out) != 0 ? StringBuff(out) : "");
  freet(v);
  freet(line);
  StringFree(out);
  return 0;
}

/* Print the arguments as hts_quote_arg() writes them, space-separated. */
static int st_quotearg(httrackp *opt, int argc, char **argv) {
  String line = STRING_EMPTY, out = STRING_EMPTY;
  int i;

  (void) opt;
  for (i = 0; i < argc; i++) {
    if (i != 0)
      StringAddchar(line, ' ');
    hts_quote_arg(&line, argv[i]);
  }
  cat_shown(&out, StringLength(line) != 0 ? StringBuff(line) : "",
            StringLength(line));
  printf("[%s]\n", StringLength(out) != 0 ? StringBuff(out) : "");
  StringFree(line);
  StringFree(out);
  return 0;
}

/* Print hts_unquote_arg(<arg>), or "missing quote". */
static int st_unquotearg(httrackp *opt, int argc, char **argv) {
  char *arg;

  (void) opt;
  if (argc != 1) {
    fprintf(stderr, "usage: -#test=unquotearg <arg>\n");
    return 1;
  }
  arg = strdupt(argv[0]);
  assertf(arg != NULL);
  if (hts_unquote_arg(arg))
    printf("[%s]\n", arg);
  else
    printf("missing quote\n");
  freet(arg);
  return 0;
}

/* ------------------------------------------------------------ */
/* Registry: this module's tests, in the order -#test lists them. */
/* ------------------------------------------------------------ */

const struct selftest_entry selftests_cmdline[] = {
    {"quotediff", "", "argument quoting against the code it replaced",
     st_quotediff},
    {"quoteprop", "", "split(quote(argv)) == argv over random argv",
     st_quoteprop},
    {"splitargs", "<flags> <line>", "split a line into arguments",
     st_splitargs},
    {"quotearg", "[arg...]", "quote arguments for a command line", st_quotearg},
    {"unquotearg", "<arg>", "strip an argument's quote pair", st_unquotearg},
    {NULL, NULL, NULL, NULL},
};
