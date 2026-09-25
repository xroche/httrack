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
/* File: htsselftest.c                                          */
/*       dispatch for the engine self-tests                     */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Each test was historically a single-letter `-#` arm in htscoremain.c; they
   now live behind one registry reached as `httrack -#test=NAME [args]` (and
   `-#test` lists them). A handler runs over the positional args
   (argv[0..argc-1]), prints one result line, and returns the process exit code.
   The tests themselves sit in hts<module>_selftest.c, one file per engine
   module, each publishing its own table. */

#include "htsselftest_int.h"

/* ------------------------------------------------------------ */
/* Batch runner: many self-tests in one process.                */
/* ------------------------------------------------------------ */

/* Framing byte between a case's output and its exit code, and between cases.
   Out of the ASCII control block, which no self-test prints. */
#define ST_BATCH_RS "\036"

/* Per case, so a bad length field cannot make us walk off the script. */
#define ST_BATCH_MAXARGS 1024

/* Read stdin to EOF, NUL-terminated; *size excludes that terminator. */
static char *st_batch_slurp(size_t *size) {
  size_t capa = 65536, len = 0;
  char *buff = malloct(capa);

  if (buff == NULL)
    return NULL;
  for (;;) {
    size_t n;

    if (len + 1 >= capa) {
      char *grown;

      if (capa > (size_t) -1 / 2) {
        freet(buff);
        return NULL;
      }
      grown = realloct(buff, capa * 2);
      if (grown == NULL) {
        freet(buff);
        return NULL;
      }
      buff = grown;
      capa *= 2;
    }
    n = fread(&buff[len], 1, capa - len - 1, stdin);
    if (n == 0) {
      /* Else a read error reads as EOF, and the tail of the script is dropped
         as silently as if it had never been written. */
      if (ferror(stdin)) {
        freet(buff);
        return NULL;
      }
      break;
    }
    len += n;
  }
  buff[len] = '\0';
  *size = len;
  return buff;
}

/* Case argument count, or -1 if the field is not one: atoi() would read a name
   as zero and swallow the next case's fields. */
static int st_batch_nargs(const char *field) {
  char *tail;
  long n;

  errno = 0;
  n = strtol(field, &tail, 10);
  if (field[0] == '\0' || *tail != '\0' || errno != 0 || n < 0 ||
      n > ST_BATCH_MAXARGS)
    return -1;
  return (int) n;
}

/* Next NUL-separated field, or NULL past the end of the script. */
static char *st_batch_field(char **pos, const char *end) {
  char *const field = *pos;

  if (field >= end)
    return NULL;
  *pos = field + strlen(field) + 1;
  return field;
}

/* A fresh option set per case: a handler's writes to opt must not reach the
   next one, as they never could across separate processes. It carries the seven
   fields htsmain() derives before the dispatch, out of the harness's own -O,
   the data directory and the tty probe; everything else is hts_create_opt's
   default on both routes, since the harness passes nothing else. */
static httrackp *st_batch_opt(const httrackp *from) {
  httrackp *const opt = hts_create_opt();

  StringCopy(opt->path_html, StringBuff(from->path_html));
  StringCopy(opt->path_html_utf8, StringBuff(from->path_html_utf8));
  StringCopy(opt->path_log, StringBuff(from->path_log));
  StringCopy(opt->path_bin, StringBuff(from->path_bin));
  opt->dir_topindex = from->dir_topindex;
  opt->quiet = from->quiet;
  opt->verbosedisplay = from->verbosedisplay;
  return opt;
}

/* Run a script of self-tests in one process, so a test file costs one fork
   rather than one per assertion (#1305). Cases must not travel in argv, where
   the engine would parse a `-*` as one of its own filters and rewrite the rest;
   stdin keeps them off it and out of htsmain()'s reach, so they arrive
   verbatim. The script is NUL-separated fields repeating <argc> <name> <arg>*,
   and each case prints its output, then RS, its exit code and RS again, which
   is what testlib.sh's selftest_run_queued splits on. */
static int st_batch(httrackp *opt, int argc, char **argv) {
  char *buff, *pos;
  const char *end;
  char *field;
  size_t len = 0;
  int err = 0;

  (void) argc;
  (void) argv;
  buff = st_batch_slurp(&len);
  if (buff == NULL) {
    fprintf(stderr, "batch: could not read the script on stdin\n");
    return 1;
  }
  pos = buff;
  end = &buff[len];
  while (!err && (field = st_batch_field(&pos, end)) != NULL) {
    const int nargs = st_batch_nargs(field);
    char *const name = st_batch_field(&pos, end);
    char **args;
    int i, code;

    if (nargs < 0 || name == NULL) {
      fprintf(stderr, "batch: malformed case header\n");
      err = 1;
      break;
    }
    args = calloct((size_t) nargs + 1, sizeof(char *));
    if (args == NULL) {
      fprintf(stderr, "batch: not enough memory\n");
      err = 1;
      break;
    }
    for (i = 0; i < nargs; i++) {
      args[i] = st_batch_field(&pos, end);
      if (args[i] == NULL) {
        fprintf(stderr, "batch: case '%s' is missing arguments\n", name);
        err = 1;
        break;
      }
    }
    if (err) {
      freet(args);
      break;
    }
    /* Nesting would re-read a stdin already at EOF and silently run nothing. */
    if (strcmp(name, "batch") == 0) {
      fprintf(stderr, "batch: cannot nest\n");
      code = 1;
    } else {
      httrackp *const copt = st_batch_opt(opt);

      code = hts_selftest(copt, name, nargs, args);
      hts_free_opt(copt);
    }
    freet(args);
    fflush(stdout);
    printf(ST_BATCH_RS "%d" ST_BATCH_RS, code);
    fflush(stdout);
  }
  freet(buff);
  return err;
}

/* ------------------------------------------------------------ */
/* Registry: this module's tests, in the order -#test lists them. */
/* ------------------------------------------------------------ */

static const struct selftest_entry selftests_core[] = {
    {"batch", "", "run a NUL-separated script of self-tests read from stdin",
     st_batch},
    {NULL, NULL, NULL, NULL},
};

/* Every module's tests, in the order -#test lists them. */
static const struct selftest_entry *const selftest_tables[] = {
    selftests_core,    selftests_back,   selftests_base,  selftests_cache,
    selftests_charset, selftests_cookie, selftests_dns,   selftests_filters,
    selftests_header,  selftests_io,     selftests_mime,  selftests_name,
    selftests_net,     selftests_opt,    selftests_parse, selftests_warc,
    selftests_wizard,
};

static void list_selftests(void) {
  size_t t;

  fprintf(stderr, "Engine self-tests (httrack -#test=NAME [args]):\n");
  for (t = 0; t < sizeof(selftest_tables) / sizeof(selftest_tables[0]); t++) {
    const struct selftest_entry *e;

    for (e = selftest_tables[t]; e->name != NULL; e++) {
      fprintf(stderr, "  %-16s %-32s %s\n", e->name, e->args, e->desc);
    }
  }
}

int hts_selftest(httrackp *opt, const char *name, int argc, char **argv) {
  size_t t;

  if (name == NULL || name[0] == '\0' || strcmp(name, "list") == 0) {
    list_selftests();
    return 0;
  }
  for (t = 0; t < sizeof(selftest_tables) / sizeof(selftest_tables[0]); t++) {
    const struct selftest_entry *e;

    for (e = selftest_tables[t]; e->name != NULL; e++) {
      if (strcmp(name, e->name) == 0)
        return e->fn(opt, argc, argv);
    }
  }
  fprintf(stderr, "Unknown self-test '%s'\n", name);
  list_selftests();
  return 1;
}
