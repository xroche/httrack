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
   mirror driven through hts_main2(), and from inside the end callback, which
   is where WinHTTrack reads it. 465_local-mirror-completed.test runs it once
   per case named by argv[1]: finish, stop, why or refuse. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "httrack-library.h"
#include "htsopt.h"
#include "htsdefines.h"

static int stop_asked = 0;

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
  if (!stop_asked) {
    stop_asked = 1;
    printf("stop: requested\n"); /* the run really reached a progress tick */
  }
  hts_request_stop(opt, HTS_FALSE);
  return 1; /* keep going; the stop request is what ends the mirror */
}

/* The Refuse case: a start callback saying no makes httpmirror() give up
   before its crawl, which is a mirror that started and failed. */
static int refuse_start(t_hts_callbackarg *carg, httrackp *opt) {
  (void) carg;
  (void) opt;
  printf("start: refused\n");
  return 0; /* 0 aborts the mirror */
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

/* Where a front end really reads the verdict: WinHTTrack's end callback raises
   a flag, its polling loop then calls hts_mirror_completed(). So the engine
   must store the verdict before it fires this. */
static int end_verdict(t_hts_callbackarg *carg, httrackp *opt) {
  (void) carg;
  printf("end: %s\n", verdict_name(hts_mirror_completed(opt)));
  return 1;
}

int main(int argc, char **argv) {
  /* Arrays rather than string literals: hts_main2() takes a writable argv.
     av[0..2] are placed by hand below, the rest appended by the loop. */
  static char av[][16] = {"mirrorverdict", "-O",  "--why",         "--quiet",
                          "--robots=0",    "-c1", "--max-time=120"};
  const size_t av_loop_from = 3;
  httrackp *opt;
  char *args[16];
  size_t i;
  int argn = 0;
  int rc;

  if (argc != 4) {
    fprintf(stderr,
            "usage: mirrorverdict finish|stop|why|refuse <url> <outdir>\n");
    return 2;
  }
  hts_init();
  opt = hts_create_opt();
  if (opt == NULL) {
    fprintf(stderr, "hts_create_opt failed\n");
    return 2;
  }
  printf("before: %s\n", verdict_name(hts_mirror_completed(opt)));

  CHAIN_FUNCTION(opt, end, end_verdict, NULL);
  if (strcmp(argv[1], "stop") == 0)
    CHAIN_FUNCTION(opt, loop, stop_now, NULL);
  if (strcmp(argv[1], "refuse") == 0)
    CHAIN_FUNCTION(opt, start, refuse_start, NULL);

  args[argn++] = av[0];
  args[argn++] = argv[2];
  args[argn++] = av[1];
  args[argn++] = argv[3];
  if (strcmp(argv[1], "why") == 0) {
    args[argn++] = av[2];
    args[argn++] = argv[2]; /* --why asks about the URL it was given */
  }
  for (i = av_loop_from; i < sizeof(av) / sizeof(av[0]); i++)
    args[argn++] = av[i];
  args[argn] = NULL;
  rc = hts_main2(argn, args, opt);

  printf("rc: %d\n", rc);
  printf("after: %s\n", verdict_name(hts_mirror_completed(opt)));
  hts_free_opt(opt);
  hts_uninit();
  return 0;
}
