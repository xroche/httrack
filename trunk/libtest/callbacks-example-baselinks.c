/*
    HTTrack external callbacks example : enforce a constant base href
	Can be useful to make copies of site's archives using site's URL base href as root reference
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
static int process_file(t_hts_callbackarg *carg, httrackp* opt, char* html, int len, const char* url_address, const char* url_file);
static int check_detectedlink(t_hts_callbackarg *carg, httrackp* opt, char* link);
static int check_detectedlink_end(t_hts_callbackarg *carg, httrackp *opt);

/* external functions */
EXTERNAL_FUNCTION int hts_plug(httrackp *opt, const char* argv);

/* 
module entry point 
*/
EXTERNAL_FUNCTION int hts_plug(httrackp *opt, const char* argv) {
  const char *arg = strchr(argv, ',');
  if (arg != NULL)
    arg++;

  /* Check args */
  fprintf(stderr, "Plugged..\n");
  if (arg == NULL || *arg == '\0' || strlen(arg) >= HTS_URLMAXSIZE / 2) {
    fprintf(stderr, "** callback error: arguments expected or bad arguments\n");
    fprintf(stderr, "usage: httrack --wrapper modulename,base\n");
    fprintf(stderr, "example: httrack --wrapper callback,http://www.example.com/\n");
    return 0;  /* failed */
  } else {
    char *callbacks_userdef = strdup(arg);      /* userdef */

    /* Plug callback functions */
    CHAIN_FUNCTION(opt, check_html, process_file, callbacks_userdef);
    CHAIN_FUNCTION(opt, linkdetected, check_detectedlink, callbacks_userdef);
    CHAIN_FUNCTION(opt, end, check_detectedlink_end, callbacks_userdef);

    fprintf(stderr, "Using root '%s'\n", callbacks_userdef);
  }

  return 1;  /* success */
}

static int process_file(t_hts_callbackarg *carg, httrackp* opt, char* html, int len, const char* url_address, const char* url_file) {
  char* prevBase;

  /* Call parent functions if multiple callbacks are chained. */
  if (CALLBACKARG_PREV_FUN(carg, check_html) != NULL) {
    if (!CALLBACKARG_PREV_FUN(carg, check_html)(CALLBACKARG_PREV_CARG(carg), opt, html, len, url_address, url_file)) {
      return 0;  /* Abort */
    }
  }

  /* Disable base href, if any */
  if ( ( prevBase = strstr(html, "<BASE HREF=\"") ) != NULL) {
    prevBase[1] = 'X';
  }

  return 1;  /* success */
}

static int check_detectedlink(t_hts_callbackarg *carg, httrackp* opt, char* link) {
  const char *base = (char*) CALLBACKARG_USERDEF(carg);

  /* Call parent functions if multiple callbacks are chained. */
  if (CALLBACKARG_PREV_FUN(carg, linkdetected) != NULL) {
    if (!CALLBACKARG_PREV_FUN(carg, linkdetected)(CALLBACKARG_PREV_CARG(carg), opt, link)) {
      return 0;  /* Abort */
    }
  }

  /* The incoming (read/write) buffer is at least HTS_URLMAXSIZE bytes long */
  if (strncmp(link, "http://", 7) == 0 || strncmp(link, "https://", 8) == 0) {
    char temp[HTS_URLMAXSIZE * 2];
    strcpy(temp, base);
    strcat(temp, link);
    strcpy(link, temp);
  }

  return 1;  /* success */
}

static int check_detectedlink_end(t_hts_callbackarg *carg, httrackp *opt) {
  char *base = (char*) CALLBACKARG_USERDEF(carg);

  fprintf(stderr, "Unplugged ..\n");
  if (base != NULL) {
    free(base);
    base = NULL;
  }

  /* Call parent functions if multiple callbacks are chained. */
  if (CALLBACKARG_PREV_FUN(carg, end) != NULL) {
    return CALLBACKARG_PREV_FUN(carg, end)(CALLBACKARG_PREV_CARG(carg), opt);
  }

  return 1;  /* success */
}
