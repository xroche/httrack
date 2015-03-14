/*
    HTTrack external callbacks example : dumy plugin, aimed to log for debugging purpose

    How to build: (callback.so or callback.dll)
      With GNU-GCC:
        gcc -O -g3 -Wall -D_REENTRANT -shared -o mycallback.so callbacks-example.c -lhttrack2
      With MS-Visual C++:
        cl -LD -nologo -W3 -Zi -Zp4 -DWIN32 -Fe"mycallback.dll" callbacks-example.c libhttrack.lib

      Note: the httrack library linker option is only necessary when using libhttrack's functions inside the callback

    How to use:
      httrack --wrapper mycallback ..
*/

/* system includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* standard httrack module includes */
#include "httrack-library.h"
#include "htsopt.h"
#include "htsdefines.h"

/* external functions */
EXTERNAL_FUNCTION int hts_plug(httrackp * opt, const char *argv);
EXTERNAL_FUNCTION int hts_unplug(httrackp * opt);

/* local function called as "check_html" callback */
static int process_file(t_hts_callbackarg * carg, httrackp * opt, char *html,
                        int len, const char *url_address,
                        const char *url_file) {
  void *ourDummyArg = (void *) CALLBACKARG_USERDEF(carg);       /*optional user-defined arg */
  char *fmt;

  (void) ourDummyArg;

  /* call parent functions if multiple callbacks are chained. you can skip this part, if you don't want previous callbacks to be called. */
  if (CALLBACKARG_PREV_FUN(carg, check_html) != NULL) {
    if (!CALLBACKARG_PREV_FUN(carg, check_html)
        (CALLBACKARG_PREV_CARG(carg), opt, html, len, url_address, url_file)) {
      return 0;                 /* abort */
    }
  }

  /* log */
  fprintf(stderr, "* parsing file %s%s\n", url_address, url_file);
  fmt = malloc(strlen(url_address) + strlen(url_file) + 128);
  sprintf(fmt, " parsing file %s%s", url_address, url_file);
  hts_log(opt, "log-wrapper-info", fmt);
  free(fmt);

  return 1;                     /* success */
}

static int start_of_mirror(t_hts_callbackarg * carg, httrackp * opt) {
  const char *arginfo = (char *) CALLBACKARG_USERDEF(carg);

  fprintf(stderr, "* mirror start\n");
  hts_log(opt, arginfo, "mirror started");

  /* call parent functions if multiple callbacks are chained. you can skip this part, if you don't want previous callbacks to be called. */
  if (CALLBACKARG_PREV_FUN(carg, end) != NULL) {
    /* status is ok on our side, return other callabck's status */
    return CALLBACKARG_PREV_FUN(carg, start) (CALLBACKARG_PREV_CARG(carg), opt);
  }

  return 1;                     /* success */
}

/* local function called as "end" callback */
static int end_of_mirror(t_hts_callbackarg * carg, httrackp * opt) {
  const char *arginfo = (char *) CALLBACKARG_USERDEF(carg);

  fprintf(stderr, "* mirror end\n");
  hts_log(opt, arginfo, "mirror ended");

  /* call parent functions if multiple callbacks are chained. you can skip this part, if you don't want previous callbacks to be called. */
  if (CALLBACKARG_PREV_FUN(carg, end) != NULL) {
    /* status is ok on our side, return other callabck's status */
    return CALLBACKARG_PREV_FUN(carg, end) (CALLBACKARG_PREV_CARG(carg), opt);
  }

  return 1;                     /* success */
}

/*
module entry point
the function name and prototype MUST match this prototype
*/
EXTERNAL_FUNCTION int hts_plug(httrackp * opt, const char *argv) {
  /* optional argument passed in the commandline we won't be using here */
  const char *arg = strchr(argv, ',');

  if (arg != NULL)
    arg++;

  /* plug callback functions */
  if (arg == NULL)
    arg = "log-wrapper-info";
  hts_log(opt, arg, "* plugging functions");
  CHAIN_FUNCTION(opt, check_html, process_file, (void *) (uintptr_t) arg);
  CHAIN_FUNCTION(opt, start, start_of_mirror, (void *) (uintptr_t) arg);
  CHAIN_FUNCTION(opt, end, end_of_mirror, (void *) (uintptr_t) arg);

  hts_log(opt, arg, "* module successfully plugged");
  return 1;                     /* success */
}

/*
module exit point
the function name and prototype MUST match this prototype
*/
EXTERNAL_FUNCTION int hts_unplug(httrackp * opt) {
  hts_log(opt, "log-wrapper-info", "* module successfully unplugged");
  return 1;
}
