/*
    HTTrack external callbacks example
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

/* Function definitions */
static int process_file(t_hts_callbackarg *carg, httrackp *opt, char* html, int len, const char* url_address, const char* url_file);
static int check_detectedlink(t_hts_callbackarg *carg, httrackp *opt, char* link);
static int check_loop(t_hts_callbackarg *carg, httrackp *opt, void* back,int back_max,int back_index,int lien_tot,int lien_ntot,int stat_time,void* stats);
static int end(t_hts_callbackarg *carg, httrackp *opt);

/* external functions */
EXTERNAL_FUNCTION int hts_plug(httrackp *opt, const char* argv);

/*
  This sample just lists all links in documents with the parent link:
  <parent> -> <link>
  This sample can be improved, for example, to make a map of a website.
*/

typedef struct t_my_userdef {
  char currentURLBeingParsed[2048];
} t_my_userdef;

/* 
module entry point 
*/
EXTERNAL_FUNCTION int hts_plug(httrackp *opt, const char* argv) {
  t_my_userdef *userdef;
  /* */
  const char *arg = strchr(argv, ',');
  if (arg != NULL)
    arg++;

  /* Create user-defined structure */
  userdef = (t_my_userdef*) malloc(sizeof(t_my_userdef));    /* userdef */
  userdef->currentURLBeingParsed[0] = '\0';

  /* Plug callback functions */
  CHAIN_FUNCTION(opt, check_html, process_file, userdef);
  CHAIN_FUNCTION(opt, end, end, userdef);
  CHAIN_FUNCTION(opt, linkdetected, check_detectedlink, userdef);
  CHAIN_FUNCTION(opt, loop, check_loop, userdef);

  return 1;  /* success */
}

static int process_file(t_hts_callbackarg *carg, httrackp *opt, char* html, int len, const char* url_address, const char* url_file) {
  t_my_userdef *userdef = (t_my_userdef*) CALLBACKARG_USERDEF(carg);
  char * const currentURLBeingParsed = userdef->currentURLBeingParsed;

  /* Call parent functions if multiple callbacks are chained. */
  if (CALLBACKARG_PREV_FUN(carg, check_html) != NULL) {
    if (!CALLBACKARG_PREV_FUN(carg, check_html)(CALLBACKARG_PREV_CARG(carg), opt, html, len, url_address, url_file)) {
      return 0;  /* Abort */
    }
  }

  /* Process */
  printf("now parsing %s%s..\n", url_address, url_file);
  strcpy(currentURLBeingParsed, url_address);
  strcat(currentURLBeingParsed, url_file);

  return 1;  /* success */
}

static int check_detectedlink(t_hts_callbackarg *carg, httrackp *opt, char* link) {
  t_my_userdef *userdef = (t_my_userdef*) CALLBACKARG_USERDEF(carg);
  char * const currentURLBeingParsed = userdef->currentURLBeingParsed;

  /* Call parent functions if multiple callbacks are chained. */
  if (CALLBACKARG_PREV_FUN(carg, linkdetected) != NULL) {
    if (!CALLBACKARG_PREV_FUN(carg, linkdetected)(CALLBACKARG_PREV_CARG(carg), opt, link)) {
      return 0;  /* Abort */
    }
  }

  /* Process */
  printf("[%s] -> [%s]\n", currentURLBeingParsed, link);

  return 1;  /* success */
}

static int check_loop(t_hts_callbackarg *carg, httrackp *opt, void* back,int back_max,int back_index,int lien_tot,int lien_ntot,int stat_time,void* stats) {
  static int fun_animation=0;

  /* Call parent functions if multiple callbacks are chained. */
  if (CALLBACKARG_PREV_FUN(carg, loop) != NULL) {
    if (!CALLBACKARG_PREV_FUN(carg, loop)(CALLBACKARG_PREV_CARG(carg), opt, back, back_max, back_index, lien_tot, lien_ntot, stat_time, stats)) {
      return 0;  /* Abort */
    }
  }

  /* Process */
  printf("%c\r", "/-\\|"[(fun_animation++)%4]);
  return 1;
}

static int end(t_hts_callbackarg *carg, httrackp *opt) {
  t_my_userdef *userdef = (t_my_userdef*) CALLBACKARG_USERDEF(carg);
  fprintf(stderr, "** info: wrapper_exit() called!\n");
  if (userdef != NULL) {
    free(userdef);
    userdef = NULL;
  }

  /* Call parent functions if multiple callbacks are chained. */
  if (CALLBACKARG_PREV_FUN(carg, end) != NULL) {
    return CALLBACKARG_PREV_FUN(carg, end)(CALLBACKARG_PREV_CARG(carg), opt);
  }

  return 1;  /* success */
}
