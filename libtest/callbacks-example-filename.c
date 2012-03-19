/*
    HTTrack external callbacks example : changing the destination filename
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
static int mysavename(t_hts_callbackarg *carg, httrackp *opt, const char* adr_complete, const char* fil_complete, const char* referer_adr, const char* referer_fil, char* save);

/* external functions */
EXTERNAL_FUNCTION int hts_plug(httrackp *opt, const char* argv);

/* Options settings */
#include "htsopt.h"

/* TOLOWER */
#define TOLOWER_(a) (a >= 'A' && a <= 'Z') ? (a + ('a' - 'A')) : a
#define TOLOWER(a) ( TOLOWER_( (a) ) )

/*
  This sample just changes the destination filenames to ROT-13 equivalent ; that is,
  a -> n
  b -> o
  c -> p
  d -> q
  ..
  n -> a
  o -> b
  ..

  <parent> -> <link>
  This sample can be improved, for example, to make a map of a website.
*/

/* 
module entry point 
*/
EXTERNAL_FUNCTION int hts_plug(httrackp *opt, const char* argv) {
  const char *arg = strchr(argv, ',');
  if (arg != NULL)
    arg++;

  /* Plug callback functions */
  CHAIN_FUNCTION(opt, savename, mysavename, NULL);

  return 1;  /* success */
}

static int mysavename(t_hts_callbackarg *carg, httrackp *opt, const char* adr_complete, const char* fil_complete, const char* referer_adr, const char* referer_fil, char* save) {
  char* a;

  /* Call parent functions if multiple callbacks are chained. */
  if (CALLBACKARG_PREV_FUN(carg, savename) != NULL) {
    if (!CALLBACKARG_PREV_FUN(carg, savename)(CALLBACKARG_PREV_CARG(carg), opt, adr_complete, fil_complete, referer_adr, referer_fil, save)) {
      return 0;  /* Abort */
    }
  }

  /* Process */
  for(a = save ; *a != 0 ; a++) {
    char c = TOLOWER(*a);
    if (c >= 'a' && c <= 'z')
      *a = ( ( ( c - 'a' ) + 13 ) % 26 ) + 'a';     // ROT-13
  }
  
  return 1;  /* success */
}
