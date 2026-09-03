/* The plugin entry point html/plug.html documents, over httrack-library.h
 * alone. */

#include "httrack-library.h"

static int process_file(t_hts_callbackarg *carg, httrackp *opt, char *html,
                        int len, const char *url_address,
                        const char *url_file) {
  (void) CALLBACKARG_USERDEF(carg);
  if (CALLBACKARG_PREV_FUN(carg, check_html) != NULL) {
    return CALLBACKARG_PREV_FUN(carg, check_html)(
        CALLBACKARG_PREV_CARG(carg), opt, html, len, url_address, url_file);
  }
  return 1;
}

static int end_of_mirror(t_hts_callbackarg *carg, httrackp *opt) {
  if (CALLBACKARG_PREV_FUN(carg, end) != NULL) {
    return CALLBACKARG_PREV_FUN(carg, end)(CALLBACKARG_PREV_CARG(carg), opt);
  }
  return 1;
}

EXTERNAL_FUNCTION int hts_plug(httrackp *opt, const char *argv) {
  (void) argv;
  CHAIN_FUNCTION(opt, check_html, process_file, NULL);
  CHAIN_FUNCTION(opt, end, end_of_mirror, NULL);
  return 1;
}

EXTERNAL_FUNCTION int hts_unplug(httrackp *opt) {
  (void) opt;
  return 1;
}
