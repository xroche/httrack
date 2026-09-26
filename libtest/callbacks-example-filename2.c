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
static int mysavename(t_hts_callbackarg * carg, httrackp * opt,
                      const char *adr_complete, const char *fil_complete,
                      const char *referer_adr, const char *referer_fil,
                      char *save);
static int myend(t_hts_callbackarg * carg, httrackp * opt);

/* external functions */
EXTERNAL_FUNCTION int hts_plug(httrackp * opt, const char *argv);

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
EXTERNAL_FUNCTION int hts_plug(httrackp * opt, const char *argv) {
  const char *arg = strchr(argv, ',');

  if (arg != NULL)
    arg++;

  /* Check args */
  if (arg == NULL || *arg == '\0' || strchr(arg, ',') == NULL) {
    fprintf(stderr, "** callback error: arguments expected or bad arguments\n");
    fprintf(stderr,
            "usage: httrack --wrapper save-name=callback:mysavename,string1,string2\n");
    fprintf(stderr,
            "example: httrack --wrapper save-name=callback:mysavename,foo,bar\n");
    return 0;                   /* failed */
  } else {
    const char *pos = strchr(arg, ',');
    t_my_userdef *userdef;
    char *string1, *string2;

    /* An empty string1 would make the rewrite loop below match without ever
       advancing, and neither half may outgrow its own field. */
    if (pos == arg || (size_t) (pos - arg) >= sizeof(userdef->string1) ||
        strlen(pos + 1) >= sizeof(userdef->string2)) {
      fprintf(stderr,
              "** callback error: empty or over-long arguments (max %d each)\n",
              (int) sizeof(userdef->string1) - 1);
      return 0; /* failed */
    }
    userdef = (t_my_userdef *) malloc(sizeof(t_my_userdef));
    if (userdef == NULL)
      return 0; /* failed */
    string1 = userdef->string1;
    string2 = userdef->string2;

    /* Split args */
    fprintf(stderr, "** info: wrapper_init(%s) called!\n", arg);
    fprintf(stderr,
            "** callback example: changing destination filename word by another one\n");
    string1[0] = string1[1] = '\0';
    strncat(string1, arg, (size_t) (pos - arg));
    strcpy(string2, pos + 1);
    fprintf(stderr, "** callback info: will replace %s by %s in filenames!\n",
            string1, string2);

    /* Plug callback functions */
    CHAIN_FUNCTION(opt, savename, mysavename, userdef);
    CHAIN_FUNCTION(opt, end, myend, userdef);
  }

  return 1;                     /* success */
}

static int myend(t_hts_callbackarg * carg, httrackp * opt) {
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

static int mysavename(t_hts_callbackarg * carg, httrackp * opt,
                      const char *adr_complete, const char *fil_complete,
                      const char *referer_adr, const char *referer_fil,
                      char *save) {
  t_my_userdef *userdef = (t_my_userdef *) CALLBACKARG_USERDEF(carg);
  char *const string1 = userdef->string1;
  char *const string2 = userdef->string2;

  /* All "save" promises is HTS_URLMAXSIZE bytes, and its real size is not
     passed in, so build into a buffer of that size and refuse a name that
     would not fit: a clipped save name collides with another file. */
  char out[HTS_URLMAXSIZE];
  size_t used = 0;
  char *buff, *a;

  /* Call parent functions if multiple callbacks are chained. */
  if (CALLBACKARG_PREV_FUN(carg, savename) != NULL) {
    if (!CALLBACKARG_PREV_FUN(carg, savename)
        (CALLBACKARG_PREV_CARG(carg), opt, adr_complete, fil_complete,
         referer_adr, referer_fil, save)) {
      return 0;                 /* Abort */
    }
  }

  /* Process */
  buff = strdup(save);
  if (buff == NULL)
    return 1; /* leave the name as it is */
  for (a = buff; *a != '\0';) {
    const char *add;
    size_t addlen;

    if (strncmp(a, string1, strlen(string1)) == 0) {
      add = string2;
      addlen = strlen(string2);
      a += strlen(string1);
    } else {
      add = a;
      addlen = 1;
      a++;
    }
    /* overflow-safe: the grown length stands alone, and used < sizeof(out) */
    if (addlen >= sizeof(out) - used) {
      free(buff);
      return 0; /* Abort */
    }
    memcpy(out + used, add, addlen);
    used += addlen;
  }
  out[used] = '\0';
  memcpy(save, out, used + 1);
  free(buff);

  return 1;                     /* success */
}
