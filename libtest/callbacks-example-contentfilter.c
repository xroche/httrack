/*
    HTTrack external callbacks example : crawling html pages depending on content
    Example of <wrappername>_init and <wrappername>_exit call (httrack >> 3.31)
    .c file

    How to build: (callback.so or callback.dll)
      With GNU-GCC:
        gcc -O -g3 -Wall -D_REENTRANT -shared -o mycallback.so callbacks-example.c -lhttrack2
      With MS-Visual C++:
        cl -LD -nologo -W3 -Zi -Zp4 -DWIN32 -Fe"mycallback.dll" callbacks-example.c libhttrack.lib

      Note: the httrack library linker option is only necessary when using libhttrack's functions inside the callback

    How to use:
      httrack --wrapper mycallback,stringtofind,stringtofind.. ..
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Standard httrack module includes */
#include "httrack-library.h"
#include "htsopt.h"
#include "htsdefines.h"

/* Local function definitions */
static int process(t_hts_callbackarg * carg, httrackp * opt, char *html,
                   int len, const char *address, const char *filename);
static int end(t_hts_callbackarg * carg, httrackp * opt);

/* external functions */
EXTERNAL_FUNCTION int hts_plug(httrackp * opt, const char *argv);

/* TOLOWER */
#define TOLOWER_(a) (a >= 'A' && a <= 'Z') ? (a + ('a' - 'A')) : a
#define TOLOWER(a) ( TOLOWER_( (a) ) )

/*
  This sample just crawls pages that contains certain keywords, and skips the other ones
*/

typedef struct t_my_userdef {
  char stringfilter[8192];
  char *stringfilters[128];
} t_my_userdef;

/* 
module entry point 
*/
EXTERNAL_FUNCTION int hts_plug(httrackp * opt, const char *argv) {
  const char *arg = strchr(argv, ',');

  if (arg != NULL)
    arg++;

  /* Check args */
  if (arg == NULL || *arg == '\0') {
    fprintf(stderr, "** callback error: arguments expected or bad arguments\n");
    fprintf(stderr,
            "usage: httrack --wrapper callback,stringtofind,stringtofind..\n");
    fprintf(stderr, "example: httrack --wrapper callback,apple,orange,lemon\n");
    return 0;
  } else {
    t_my_userdef *userdef = (t_my_userdef *) malloc(sizeof(t_my_userdef));      /* userdef */
    char *const stringfilter = userdef->stringfilter;
    char **const stringfilters = userdef->stringfilters;

    /* */
    char *a = stringfilter;
    int i = 0;

    fprintf(stderr, "** info: wrapper_init(%s) called!\n", arg);
    fprintf(stderr,
            "** callback example: crawling pages only if specific keywords are found\n");

    /* stringfilters = split(arg, ','); */
    strcpy(stringfilter, arg);
    while(a != NULL) {
      stringfilters[i] = a;
      a = strchr(a, ',');
      if (a != NULL) {
        *a = '\0';
        a++;
      }
      fprintf(stderr, "** callback info: will crawl pages with '%s' in them\n",
              stringfilters[i]);
      i++;
    }
    stringfilters[i++] = NULL;

    /* Plug callback functions */
    CHAIN_FUNCTION(opt, check_html, process, userdef);
    CHAIN_FUNCTION(opt, end, end, userdef);
  }

  return 1;                     /* success */
}

static int process(t_hts_callbackarg * carg, httrackp * opt, char *html,
                   int len, const char *address, const char *filename) {
  t_my_userdef *userdef = (t_my_userdef *) CALLBACKARG_USERDEF(carg);

  /*char * const stringfilter = userdef->stringfilter; */
  char **const stringfilters = userdef->stringfilters;

  /* */
  int i = 0;
  int getIt = 0;
  char *pos;

  /* Call parent functions if multiple callbacks are chained. */
  if (CALLBACKARG_PREV_FUN(carg, check_html) != NULL) {
    if (!CALLBACKARG_PREV_FUN(carg, check_html)
        (CALLBACKARG_PREV_CARG(carg), opt, html, len, address, filename)) {
      return 0;                 /* Abort */
    }
  }

  /* Process */
  if (strcmp(address, "primary") == 0 && strcmp(filename, "/primary") == 0)     /* primary page (list of links) */
    return 1;
  while(stringfilters[i] != NULL && !getIt) {
    if ((pos = strstr(html, stringfilters[i])) != NULL) {
      int j;

      getIt = 1;
      fprintf(stderr,
              "** callback info: found '%s' keyword in '%s%s', crawling this page!\n",
              stringfilters[i], address, filename);
      fprintf(stderr, "** details:\n(..)");
      for(j = 0; j < 72 && pos[j]; j++) {
        if (pos[j] > 32)
          fprintf(stderr, "%c", pos[j]);
        else
          fprintf(stderr, "?");
      }
      fprintf(stderr, "(..)\n");
    }
    i++;
  }
  if (getIt) {
    return 1;                   /* success */
  } else {
    fprintf(stderr,
            "** callback info: won't parse '%s%s' (no specified keywords found)\n",
            address, filename);
    return 0;                   /* this page sucks, don't parse it */
  }
}

static int end(t_hts_callbackarg * carg, httrackp * opt) {
  t_my_userdef *userdef = (t_my_userdef *) CALLBACKARG_USERDEF(carg);

  fprintf(stderr, "** info: wrapper_exit() called!\n");
  if (userdef != NULL) {
    free(userdef);
    userdef = NULL;
  }

  /* Call parent functions if multiple callbacks are chained. */
  if (CALLBACKARG_PREV_FUN(carg, end) != NULL) {
    return CALLBACKARG_PREV_FUN(carg, end) (CALLBACKARG_PREV_CARG(carg), opt);
  }

  return 1;                     /* success */
}
