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

/* Fuzz the command-line splitter and the option aliases (htscmdline.c and
   htsalias.c). Byte 0 holds the HTS_SPLIT_* flags. The rest, up to a NUL, is
   the command line; any bytes after the NUL are a config file for
   optinclude_file(). */
#include "fuzz.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "httrack-library.h"
#include "htsalias.h"
#include "htscmdline.h"
#include "htsglobal.h"
#include "htsbuf.h"

static char alias_dir[256];
static char alias_conf[sizeof(alias_dir) + sizeof("/fuzz-alias.conf")];

static void fuzz_alias_cleanup(void) {
  (void) unlink(alias_conf);
  (void) rmdir(alias_dir);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static int inited = 0;
  int flags;
  const uint8_t *nul;
  char *cmd;
  char **args;
  int nargs = 0;
  int i;

  if (size == 0)
    return 0;
  if (!inited) {
    hts_init();
    if (!fuzz_tmpdir(HTS_BUF(alias_dir), "fuzz-alias", fuzz_alias_cleanup))
      return 0;
    snprintf(alias_conf, sizeof(alias_conf), "%s/fuzz-alias.conf", alias_dir);
    inited = 1;
  }
  flags = data[0] &
          (HTS_SPLIT_FOLD_WS | HTS_SPLIT_STRIP_QUOTES | HTS_SPLIT_DROP_EMPTY);
  data++, size--;
  nul = (const uint8_t *) memchr(data, 0, size);
  cmd =
      hts_span_dup(fuzz_span(data, nul != NULL ? (size_t) (nul - data) : size));

  args = hts_split_args(cmd, &nargs, flags);
  if (args != NULL) {
    /* htscoremain.c bounds every argument to HTS_CDLMAXSIZE */
    for (i = 0; i < nargs; i++) {
      if (strlen(args[i]) >= HTS_CDLMAXSIZE)
        args[i][HTS_CDLMAXSIZE - 1] = '\0';
    }
    for (i = 0; i < nargs; i++) {
      char ra0[HTS_CDLMAXSIZE];
      char ra1[HTS_CDLMAXSIZE];
      char *ra[2] = {ra0, ra1};
      int rac = 0;
      char err[HTS_CDLMAXSIZE + 64];
      double rate = 0;
      String out = STRING_EMPTY;
      char **back;
      int back_n = 0;

      (void) optalias_check(nargs, (const char *const *) args, i, &rac, ra,
                            HTS_CDLMAXSIZE, HTS_BUF(err));
      (void) !optalias_help(args[i]); // fuzzes the lookup, not its answer
      (void) optrate_read(args[i], &rate);

      /* a quoted argument must split back to itself */
      hts_quote_arg(&out, args[i]);
      back = hts_split_args(StringBuffRW(out), &back_n, HTS_SPLIT_STRIP_QUOTES);
      if (back == NULL || back_n != 1 || strcmp(back[0], args[i]) != 0)
        abort();
      freet(back);
      StringFree(out);
    }
    freet(args);
  }

  if (nul != NULL && (size_t) (nul + 1 - data) < size) {
    const uint8_t *cfg = nul + 1;
    const size_t cfg_size = size - (size_t) (cfg - data);
    FILE *fp = fopen(alias_conf, "wb");

    if (fp != NULL) {
      cmdl_argv argv;
      const hts_boolean written = fwrite(cfg, 1, cfg_size, fp) == cfg_size;

      if (fclose(fp) == 0 && written && cmdl_init(&argv, 8)) {
        if (cmdl_add(&argv, "httrack"))
          (void) optinclude_file(alias_conf, &argv);
        cmdl_free(&argv);
      }
    }
  }

  freet(cmd);
  return 0;
}
