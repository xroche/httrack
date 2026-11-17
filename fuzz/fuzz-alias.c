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

/* Fuzz the option alias and command-line splitter (htsalias.c and
   htscmdline.c): hts_split_args()/hts_split_cmdline()/hts_split_webcmd()
   share one quoting grammar, optalias_check() maps the long-form aliases to
   real options the way the engine and the WebHTTrack server replay a
   command line, and optinclude_file() expands a config file into the same
   slots. A NUL byte in the input splits it: the part before is the command
   line, the part after is the config file. */
#include "fuzz.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "httrack-library.h"
#include "htsalias.h"
#include "htscmdline.h"
#include "htsglobal.h"

static char alias_dir[256];
static char alias_conf[sizeof(alias_dir) + sizeof("/fuzz-alias.conf")];

static void fuzz_alias_cleanup(void) {
  (void) unlink(alias_conf);
  (void) rmdir(alias_dir);
}

/* One option position through the alias layer: the check itself, the
   lookups, the cluster walk and the rate reader. */
static void fuzz_alias_token(int argc, const char *const *argv, int i) {
  /* return_argv is char[2][]: two buffers the check copies the option and
     its parameter into, return_argv_size each. */
  char ra0[HTS_CDLMAXSIZE];
  char ra1[HTS_CDLMAXSIZE];
  char *ra[2] = {ra0, ra1};
  int rac = 0;
  char err[HTS_CDLMAXSIZE + 64];
  const int treated =
      optalias_check(argc, argv, i, &rac, ra, HTS_CDLMAXSIZE, err,
                     sizeof(err));

  (void) treated;
  (void) optalias_find(argv[i]);
  (void) optreal_find(argv[i]);
  (void) optalias_help(argv[i]);
  /* the cluster walk is a short-option walk: htscoremain.c only clusters a
     token that starts with '-', where token[1] is at worst the NUL */
  if (argv[i][0] == '-') {
    size_t at = 1;
    char name[16];
    int steps = 0;

    while (optcluster_next(argv[i], &at, name, sizeof(name))
           && steps++ < HTS_CDLMAXSIZE)
      continue;
  }
  {
    double rate = 0;

    (void) optrate_read(argv[i], &rate);
  }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static int inited = 0;
  const int flags = size > 0 ? (data[0] & 7) : 0;
  const uint8_t *nul;
  char *cmd;
  char **args = NULL;
  int nargs = 0;
  int i;

  if (size == 0)
    return 0;
  if (!inited) {
    hts_init();
    if (!fuzz_tmpdir(alias_dir, sizeof(alias_dir), "fuzz-alias",
                     fuzz_alias_cleanup))
      return 0;
    snprintf(alias_conf, sizeof(alias_conf), "%s/fuzz-alias.conf", alias_dir);
    inited = 1;
  }
  data++, size--;
  nul = (const uint8_t *) memchr(data, 0, size);
  cmd = fuzz_strdup(data, nul != NULL ? (size_t) (nul - data) : size);

  /* The three splitters share the grammar; each edits cmd in place, so
     each works on its own copy. */
  args = hts_split_args(cmd, &nargs, flags);
  {
    char *copy = fuzz_strdup(data, nul != NULL ? (size_t) (nul - data) : size);
    int n2 = 0;
    char **a2 = hts_split_cmdline(copy, &n2);

    if (a2 != NULL)
      freet(a2);
    freet(copy);
  }
  {
    char *copy = fuzz_strdup(data, nul != NULL ? (size_t) (nul - data) : size);
    int n3 = 0;
    char **a3 = hts_split_webcmd(copy, &n3);

    if (a3 != NULL)
      freet(a3);
    freet(copy);
  }

  /* The caller (htscoremain.c) bounds every argument to HTS_CDLMAXSIZE;
     hold the same contract here so a too-long token is a truncation, not a
     contract the engine never makes. */
  if (args != NULL) {
    for (i = 0; i < nargs; i++) {
      if (args[i] != NULL && strlen(args[i]) >= HTS_CDLMAXSIZE)
        args[i][HTS_CDLMAXSIZE - 1] = '\0';
    }
    for (i = 0; i < nargs; i++) {
      if (args[i] == NULL)
        continue;
      fuzz_alias_token(nargs, (const char *const *) args, i);
      /* hts_quote_arg()/hts_split_args() round-trip. */
      {
        String out = STRING_EMPTY;
        int back_n = 0;
        char **back;

        hts_quote_arg(&out, args[i]);
        back = hts_split_args(StringBuff(out), &back_n,
                              HTS_SPLIT_STRIP_QUOTES);
        if (back != NULL)
          freet(back);
        StringFree(out);
      }
      {
        char *unq = fuzz_strdup((const uint8_t *) args[i], strlen(args[i]));

        (void) hts_is_quoted(unq, strlen(unq));
        (void) hts_unquote_arg(unq);
        freet(unq);
      }
    }
  }

  /* The config file half expands through the same slots. */
  if (nul != NULL && (size_t) (nul + 1 - data) < size) {
    const uint8_t *cfg = nul + 1;
    size_t cfg_size = size - (size_t) (cfg - data);
    FILE *fp = fopen(alias_conf, "wb");

    if (fp != NULL) {
      fwrite(cfg, 1, cfg_size, fp);
      fclose(fp);
      {
        cmdl_argv cmd;

        if (cmdl_init(&cmd, 8)) {
          if (cmdl_add(&cmd, "httrack")) {
            (void) optinclude_file(alias_conf, &cmd);
            if (cmd.argc > 1) {
              (void) cmdl_ins(&cmd, "--recurse", 1);
              if (cmd.argc > 2)
                cmdl_mark_param(&cmd, 2);
            }
          }
          cmdl_free(&cmd);
        }
      }
    }
  }

  if (args != NULL)
    freet(args);
  freet(cmd);
  return 0;
}
