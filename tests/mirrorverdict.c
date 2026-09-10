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

Please visit our Website: http://www.httrack.com
*/

/* Reads hts_mirror_completed() the way an embedding GUI does: around one real
   mirror driven through hts_main2(). 465_local-mirror-completed.test runs it
   once per case named by argv[1]. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "httrack-library.h"
#include "htsopt.h"
#include "htsdefines.h"

/* The Stop button, pushed at the engine's first progress tick. */
static int stop_now(t_hts_callbackarg *carg, httrackp *opt, lien_back *back,
                    int back_max, int back_index, int lien_tot, int lien_ntot,
                    int stat_time, hts_stat_struct *stats) {
  (void) carg;
  (void) back;
  (void) back_max;
  (void) back_index;
  (void) lien_tot;
  (void) lien_ntot;
  (void) stat_time;
  (void) stats;
  hts_request_stop(opt, HTS_FALSE);
  return 1;
}

static const char *verdict_name(hts_tristate verdict) {
  if (verdict == HTS_DEFAULT)
    return "default";
  if (verdict == HTS_TRUE)
    return "true";
  if (verdict == HTS_FALSE)
    return "false";
  return "bogus";
}

int main(int argc, char **argv) {
  static char prog[] = "mirrorverdict";
  static char opt_outdir[] = "-O";
  static char opt_quiet[] = "--quiet";
  static char opt_robots[] = "--robots=0";
  static char opt_conns[] = "-c1";
  static char opt_maxtime[] = "--max-time=120";
  httrackp *opt;
  char *args[16];
  int argn = 0;
  int rc;

  if (argc != 4) {
    fprintf(stderr, "usage: mirrorverdict finish|stop <url> <outdir>\n");
    return 2;
  }
  hts_init();
  opt = hts_create_opt();
  if (opt == NULL) {
    fprintf(stderr, "hts_create_opt failed\n");
    return 2;
  }
  printf("before: %s\n", verdict_name(hts_mirror_completed(opt)));

  if (strcmp(argv[1], "stop") == 0)
    CHAIN_FUNCTION(opt, loop, stop_now, NULL);

  /* Arrays rather than string literals: hts_main2() takes a writable argv. */
  args[argn++] = prog;
  args[argn++] = argv[2];
  args[argn++] = opt_outdir;
  args[argn++] = argv[3];
  args[argn++] = opt_quiet;
  args[argn++] = opt_robots;
  args[argn++] = opt_conns;
  args[argn++] = opt_maxtime;
  args[argn] = NULL;
  rc = hts_main2(argn, args, opt);

  printf("rc: %d\n", rc);
  printf("after: %s\n", verdict_name(hts_mirror_completed(opt)));
  hts_free_opt(opt);
  hts_uninit();
  return 0;
}
