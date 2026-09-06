/*
    HTTrack external callbacks example : rewrite the content of mirrored pages
    Example of a postprocess-html callback editing a page in place
    .c file

    How to build: (callback.so or callback.dll)
      With GNU-GCC:
        gcc -O -g3 -Wall -D_REENTRANT -shared -o mycallback.so \
            callbacks-example.c -lhttrack2
      With MS-Visual C++:
        cl -LD -nologo -W3 -Zi -Zp4 -DWIN32 -Fe"mycallback.dll" \
            callbacks-example.c libhttrack.lib

      Note: the httrack library linker option is only necessary when
      using libhttrack's functions inside the callback

    How to use:
      httrack --wrapper mycallback,OLD=NEW ..
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Standard httrack module includes */
#include "httrack-library.h"
#include "htsopt.h"
#include "htsdefines.h"

/* Local function definitions */
static void uninit(t_hts_callbackarg *carg);
static int postprocess(t_hts_callbackarg * carg, httrackp * opt, char **html,
                       int *len, const char *url_address, const char *url_file);

/* external functions */
EXTERNAL_FUNCTION int hts_plug(httrackp * opt, const char *argv);

/* The OLD=NEW pair given on the command line, held as one "OLD\0NEW" string. */
typedef struct t_my_userdef {
  char *pair;
  const char *from;
  size_t from_len;
  const char *to;
  size_t to_len;
} t_my_userdef;

/* 
module entry point 
*/
EXTERNAL_FUNCTION int hts_plug(httrackp * opt, const char *argv) {
  const char *arg = strchr(argv, ',');
  const char *sep = arg != NULL ? strchr(++arg, '=') : NULL;
  t_my_userdef *userdef;

  if (sep == NULL || sep == arg) {
    fprintf(stderr,
            "changecontent: expected --wrapper changecontent,OLD=NEW\n");
    return 0; /* failure */
  }

  /* Create user-defined structure */
  userdef = (t_my_userdef *) hts_malloc(sizeof(t_my_userdef));
  if (userdef == NULL)
    return 0; /* failure */
  userdef->pair = hts_strdup(arg);
  if (userdef->pair == NULL) {
    hts_free(userdef);
    return 0; /* failure */
  }
  userdef->pair[sep - arg] = '\0';
  userdef->from = userdef->pair;
  userdef->from_len = strlen(userdef->from);
  userdef->to = userdef->pair + (sep - arg) + 1;
  userdef->to_len = strlen(userdef->to);

  /* A reply left inside the engine's own buffer may shrink the page, never
     grow it, so a longer replacement has nowhere to go. */
  if (userdef->to_len > userdef->from_len) {
    fprintf(stderr, "changecontent: NEW cannot be longer than OLD\n");
    hts_free(userdef->pair);
    hts_free(userdef);
    return 0; /* failure */
  }

  /* Plug callback functions */
  CHAIN_FUNCTION(opt, uninit, uninit, userdef);
  CHAIN_FUNCTION(opt, postprocess, postprocess, userdef);

  return 1;                     /* success */
}

static void uninit(t_hts_callbackarg *carg) {
  t_my_userdef *userdef = (t_my_userdef *) CALLBACKARG_USERDEF(carg);

  /* Call parent functions if multiple callbacks are chained. */
  if (CALLBACKARG_PREV_FUN(carg, uninit) != NULL) {
    CALLBACKARG_PREV_FUN(carg, uninit)(CALLBACKARG_PREV_CARG(carg));
  }

  /* Process */
  hts_free(userdef->pair);
  hts_free(userdef);
}

static int postprocess(t_hts_callbackarg * carg, httrackp * opt, char **html,
                       int *len, const char *url_address,
                       const char *url_file) {
  const t_my_userdef *userdef =
      (const t_my_userdef *) CALLBACKARG_USERDEF(carg);
  char *buffer;
  size_t size, in, out = 0;

  /* Call parent functions if multiple callbacks are chained. */
  if (CALLBACKARG_PREV_FUN(carg, postprocess) != NULL) {
    if (!CALLBACKARG_PREV_FUN(carg, postprocess)(CALLBACKARG_PREV_CARG(carg),
                                                 opt, html, len, url_address,
                                                 url_file)) {
      return 0; /* Abort */
    }
  }

  /* Process: the engine keeps this buffer and it is not a C string, so read
     exactly *len bytes, edit them where they are, and free nothing. */
  buffer = *html;
  if (buffer == NULL || *len <= 0)
    return 1;
  size = (size_t) *len;
  for (in = 0; in < size;) {
    if (size - in >= userdef->from_len &&
        memcmp(buffer + in, userdef->from, userdef->from_len) == 0) {
      memcpy(buffer + out, userdef->to, userdef->to_len);
      out += userdef->to_len;
      in += userdef->from_len;
    } else {
      buffer[out++] = buffer[in++];
    }
  }
  *len = (int) out;

  return 1;
}
