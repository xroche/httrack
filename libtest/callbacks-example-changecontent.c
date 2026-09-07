/*
    HTTrack external callbacks example : rewrite the content of mirrored pages
    Example of a postprocess-html callback, in both shapes the reply may take
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
      httrack --wrapper mycallback,own,OLD=NEW ..

      Without "own" the page is edited inside the engine's buffer, which cannot
      grow, so NEW must not be longer than OLD. With it the callback answers
      with a buffer of its own, of any length, reused for every page and freed
      from uninit because the engine copies out of it and frees nothing.
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
  int own;      /* answer with a buffer of our own */
  char *buffer; /* that buffer, grown as needed and reused for every page */
  size_t capacity;
} t_my_userdef;

/* 
module entry point 
*/
EXTERNAL_FUNCTION int hts_plug(httrackp * opt, const char *argv) {
  const char *arg = strchr(argv, ',');
  const char *sep;
  int own = 0;
  t_my_userdef *userdef;

  if (arg != NULL && strncmp(++arg, "own,", 4) == 0) {
    own = 1;
    arg += 4;
  }
  sep = arg != NULL ? strchr(arg, '=') : NULL;

  if (sep == NULL || sep == arg) {
    fprintf(stderr,
            "changecontent: expected --wrapper changecontent,[own,]OLD=NEW\n");
    return 0; /* failure */
  }

  /* Create user-defined structure */
  userdef = (t_my_userdef *) hts_malloc(sizeof(t_my_userdef));
  if (userdef == NULL)
    return 0; /* failure */
  userdef->own = own;
  userdef->buffer = NULL;
  userdef->capacity = 0;
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
  if (!userdef->own && userdef->to_len > userdef->from_len) {
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
  hts_free(userdef->buffer);
  hts_free(userdef->pair);
  hts_free(userdef);
}

/* Write src, with every OLD replaced by NEW, into dst, and return what was
   written. dst may be src, because that caller keeps NEW no longer than OLD, so
   the write never passes the read. */
static size_t apply_pair(const t_my_userdef *userdef, char *dst,
                         const char *src, size_t size) {
  size_t in, out = 0;

  for (in = 0; in < size;) {
    if (size - in >= userdef->from_len &&
        memcmp(src + in, userdef->from, userdef->from_len) == 0) {
      memcpy(dst + out, userdef->to, userdef->to_len);
      out += userdef->to_len;
      in += userdef->from_len;
    } else {
      dst[out++] = src[in++];
    }
  }
  return out;
}

/* What apply_pair will write for these bytes, counting the matches rather than
   assuming the page can only shrink. */
static size_t apply_pair_size(const t_my_userdef *userdef, const char *src,
                              size_t size) {
  size_t in, matches = 0;

  for (in = 0; in + userdef->from_len <= size;) {
    if (memcmp(src + in, userdef->from, userdef->from_len) == 0) {
      matches++;
      in += userdef->from_len;
    } else {
      in++;
    }
  }
  return size - matches * userdef->from_len + matches * userdef->to_len;
}

static int postprocess(t_hts_callbackarg * carg, httrackp * opt, char **html,
                       int *len, const char *url_address,
                       const char *url_file) {
  t_my_userdef *const userdef = (t_my_userdef *) CALLBACKARG_USERDEF(carg);
  char *buffer;
  size_t size, out;

  /* Call parent functions if multiple callbacks are chained. */
  if (CALLBACKARG_PREV_FUN(carg, postprocess) != NULL) {
    if (!CALLBACKARG_PREV_FUN(carg, postprocess)(CALLBACKARG_PREV_CARG(carg),
                                                 opt, html, len, url_address,
                                                 url_file)) {
      return 0; /* Abort */
    }
  }

  /* Process: the engine's buffer is not a C string, so read exactly *len bytes
     of it and free nothing. */
  buffer = *html;
  if (buffer == NULL || *len <= 0)
    return 1;
  size = (size_t) *len;

  if (userdef->own) {
    const size_t need = apply_pair_size(userdef, buffer, size);

    if (need > userdef->capacity) {
      char *const grown = (char *) hts_realloc(userdef->buffer, need);

      if (grown == NULL)
        return 1; /* leave the page as the engine had it */
      userdef->buffer = grown;
      userdef->capacity = need;
    }
    out = apply_pair(userdef, userdef->buffer, buffer, size);
    *html = userdef->buffer;
  } else {
    out = apply_pair(userdef, buffer, buffer, size);
  }
  *len = (int) out;

  return 1;
}
