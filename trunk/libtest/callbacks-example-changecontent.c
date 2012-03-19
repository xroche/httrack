/*
    HTTrack external callbacks example : display all incoming request headers
    Example of <wrappername>_init and <wrappername>_exit call (httrack >> 3.31)
    .c file

    How to build: (callback.so or callback.dll)
      With GNU-GCC:
        gcc -O -g3 -Wall -D_REENTRANT -shared -o mycallback.so callbacks-example.c -lhttrack2
      With MS-Visual C++:
        cl -LD -nologo -W3 -Zi -Zp4 -DWIN32 -Fe"mycallback.dll" callbacks-example.c libhttrack.lib

      Note: the httrack library linker option is only necessary when using libhttrack's functions inside the callback

    How to use:
      httrack --wrapper mycallback ..
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Standard httrack module includes */
#include "httrack-library.h"
#include "htsopt.h"
#include "htsdefines.h"

/* Local function definitions */
static int postprocess(t_hts_callbackarg *carg, httrackp *opt, 
                       char** html, int* len,
                       const char* url_address, const char* url_file);

/* external functions */
EXTERNAL_FUNCTION int hts_plug(httrackp *opt, const char* argv);

/* 
module entry point 
*/
EXTERNAL_FUNCTION int hts_plug(httrackp *opt, const char* argv) {
  const char *arg = strchr(argv, ',');
  if (arg != NULL)
    arg++;

  /* Plug callback functions */
  CHAIN_FUNCTION(opt, postprocess, postprocess, NULL);

  return 1;  /* success */
}

static int postprocess(t_hts_callbackarg *carg, httrackp *opt, char** html,int* len,const char* url_address,const char* url_file) {
  char *old = *html;

  /* Call parent functions if multiple callbacks are chained. */
  if (CALLBACKARG_PREV_FUN(carg, postprocess) != NULL) {
    if (CALLBACKARG_PREV_FUN(carg, postprocess)(CALLBACKARG_PREV_CARG(carg), opt, html, len, url_address, url_file)) {
      /* Modified *html */
      old = *html;
    }
  }

  /* Process */
  *html = strdup(*html);
  hts_free(old);

  return 1;
}
