/*
    How to build: (callback.so or callback.dll)
      With GNU-GCC:
        gcc -O -g3 -Wall -D_REENTRANT -shared -o mycallback.so callbacks-example.c -lhttrack2
      With MS-Visual C++:
        cl -LD -nologo -W3 -Zi -Zp4 -DWIN32 -Fe"mycallback.dll" callbacks-example.c libhttrack.lib

      Note: the httrack library linker option is only necessary when using libhttrack's functions inside the callback

    How to use:
      httrack --wrapper mycallback,string1,string2 ..
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Standard httrack module includes */
#include "httrack-library.h"
#include "htsopt.h"
#include "htsdefines.h"

/* Function definitions */
static int mysavename(t_hts_callbackarg *carg, httrackp *opt, const char* adr_complete, const char* fil_complete, const char* referer_adr, const char* referer_fil, char* save);
static int myend(t_hts_callbackarg *carg, httrackp *opt);

/* external functions */
EXTERNAL_FUNCTION int hts_plug(httrackp *opt, const char* argv);

/* TOLOWER */
#define TOLOWER_(a) (a >= 'A' && a <= 'Z') ? (a + ('a' - 'A')) : a
#define TOLOWER(a) ( TOLOWER_( (a) ) )

/*
  This sample just replaces all occurences of "string1" into "string2"
  string1 and string2 are passed in the callback string:
  httrack --wrapper save-name=callback:mysavename,string1,string2 ..
*/

typedef struct t_my_userdef {
  char string1[256];
  char string2[256];
} t_my_userdef;

/* 
module entry point 
*/
EXTERNAL_FUNCTION int hts_plug(httrackp *opt, const char* argv) {
  const char *arg = strchr(argv, ',');
  if (arg != NULL)
    arg++;

  /* Check args */
  if (arg == NULL || *arg == '\0' || strchr(arg, ',') == NULL) {
    fprintf(stderr, "** callback error: arguments expected or bad arguments\n");
    fprintf(stderr, "usage: httrack --wrapper save-name=callback:mysavename,string1,string2\n");
    fprintf(stderr, "example: httrack --wrapper save-name=callback:mysavename,foo,bar\n");
    return 0;   /* failed */
  } else {
    char *pos = strchr(arg, ',');
    t_my_userdef *userdef = (t_my_userdef*) malloc(sizeof(t_my_userdef));
    char * const string1 = userdef->string1;
    char * const string2 = userdef->string2;

    /* Split args */
    fprintf(stderr, "** info: wrapper_init(%s) called!\n", arg);
    fprintf(stderr, "** callback example: changing destination filename word by another one\n");
    string1[0] = string1[1] = '\0';
    strncat(string1, arg, pos - arg);
    strcpy(string2, pos + 1);
    fprintf(stderr, "** callback info: will replace %s by %s in filenames!\n", string1, string2);

    /* Plug callback functions */
    CHAIN_FUNCTION(opt, savename, mysavename, userdef);
    CHAIN_FUNCTION(opt, end, myend, userdef);
  }

  return 1;  /* success */
}

static int myend(t_hts_callbackarg *carg, httrackp *opt) {
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

static int mysavename(t_hts_callbackarg *carg, httrackp *opt, const char* adr_complete, const char* fil_complete, const char* referer_adr, const char* referer_fil, char* save) {
  t_my_userdef *userdef = (t_my_userdef*) CALLBACKARG_USERDEF(carg);
  char * const string1 = userdef->string1;
  char * const string2 = userdef->string2;
  /* */
  char *buff, *a, *b;

  /* Call parent functions if multiple callbacks are chained. */
  if (CALLBACKARG_PREV_FUN(carg, savename) != NULL) {
    if (!CALLBACKARG_PREV_FUN(carg, savename)(CALLBACKARG_PREV_CARG(carg), opt, adr_complete, fil_complete, referer_adr, referer_fil, save)) {
      return 0;  /* Abort */
    }
  }

  /* Process */
  buff = strdup(save);
  a = buff;
  b = save;
  *b = '\0';          /* the "save" variable points to a buffer with "sufficient" space */
  while(*a) {
    if (strncmp(a, string1, (int)strlen(string1)) == 0) {
      strcat(b, string2);
      b += strlen(b);
      a += strlen(string1);
    } else {
      *b++ = *a++;
      *b = '\0';
    }
  }
  free(buff);

  return 1;  /* success */
}
