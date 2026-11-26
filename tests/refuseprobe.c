/* A loop callback that aborts url_savename()'s type probe once a stop is
 * pending, so the probe returns -1 to hts_wait_delayed(). */

#include <stdio.h>

#include "httrack-library.h"
#include "htsopt.h"

static int refuse(t_hts_callbackarg *carg, httrackp *opt, lien_back *back,
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
  /* 2 is url_savename()'s probe, 6 a socket wait inside it */
  if (opt->state.stop && (opt->state._hts_in_html_parsing == 2 ||
                          opt->state._hts_in_html_parsing == 6)) {
    printf("refuseprobe: refused in state %d\n",
           opt->state._hts_in_html_parsing);
    fflush(stdout);
    return 0;
  }
  return 1;
}

/* Prototypes first: the tree builds with -Wmissing-prototypes as an error. */
EXTERNAL_FUNCTION int hts_plug(httrackp *opt, const char *argv);

EXTERNAL_FUNCTION int hts_plug(httrackp *opt, const char *argv) {
  (void) argv;
  CHAIN_FUNCTION(opt, loop, refuse, NULL);
  return 1;
}
