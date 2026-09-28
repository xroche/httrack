/* A sendhead hook that refuses the request, which htsdefines.h says drops the
 * connection and fails the transfer. */

#include <stdio.h>

#include "httrack-library.h"

static int refuse(t_hts_callbackarg *carg, httrackp *opt, char *buff,
                  const char *adr, const char *fil, const char *referer_adr,
                  const char *referer_fil, htsblk *outgoing) {
  (void) carg;
  (void) opt;
  (void) buff;
  (void) referer_adr;
  (void) referer_fil;
  (void) outgoing;
  /* The test reads this back: the log assertions all hold on a build that never
   * called the hook. */
  fprintf(stderr, "sendheadrefuse: refusing %s%s\n", adr, fil);
  return 0;
}

EXTERNAL_FUNCTION int hts_plug(httrackp *opt, const char *argv) {
  (void) argv;
  CHAIN_FUNCTION(opt, sendhead, refuse, NULL);
  return 1;
}

EXTERNAL_FUNCTION int hts_unplug(httrackp *opt) {
  (void) opt;
  return 1;
}
