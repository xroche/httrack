/* A preprocess hook that answers 1 but stores no page, which must leave the
 * engine parsing the page it fetched. */

#include <stdio.h>

#include "httrack-library.h"

static int nopage(t_hts_callbackarg *carg, httrackp *opt, char **html, int *len,
                  const char *url_address, const char *url_file) {
  (void) carg;
  (void) opt;
  fprintf(stderr, "preprocessnopage: no page for %s%s\n", url_address,
          url_file);
  *html = NULL;
  *len = 0;
  return 1;
}

/* Prototypes first: the tree builds with -Wmissing-prototypes as an error, and
   no installed header declares a plugin's own entry points. */
EXTERNAL_FUNCTION int hts_plug(httrackp *opt, const char *argv);
EXTERNAL_FUNCTION int hts_unplug(httrackp *opt);

EXTERNAL_FUNCTION int hts_plug(httrackp *opt, const char *argv) {
  (void) argv;
  CHAIN_FUNCTION(opt, preprocess, nopage, NULL);
  return 1;
}

EXTERNAL_FUNCTION int hts_unplug(httrackp *opt) {
  (void) opt;
  return 1;
}
