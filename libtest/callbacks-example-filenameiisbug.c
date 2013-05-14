/*
    HTTrack external callbacks example : changing folder names ending with ".com"
    with ".c0m" as a workaround of IIS bug (see KB 275601)

    How to build: (callback.so or callback.dll)
      With GNU-GCC:
        gcc -O -g3 -Wall -D_REENTRANT -shared -o mycallback.so callbacks-example.c -lhttrack2
      With MS-Visual C++:
        cl -LD -nologo -W3 -Zi -Zp4 -DWIN32 -Fe"mycallback.dll" callbacks-example.c libhttrack.lib

      Note: the httrack library linker option is only necessary when using libhttrack's functions inside the callback
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

/* external functions */
EXTERNAL_FUNCTION int hts_plug(httrackp * opt, const char *argv);

/* 
module entry point 
*/
EXTERNAL_FUNCTION int hts_plug(httrackp * opt, const char *argv) {
  const char *arg = strchr(argv, ',');

  if (arg != NULL)
    arg++;
  CHAIN_FUNCTION(opt, savename, mysavename, NULL);
  return 1;                     /* success */
}

/*
  Replaces all "offending" IIS extensions (exe, dll..) with "nice" ones
*/
static int mysavename(t_hts_callbackarg * carg, httrackp * opt,
                      const char *adr_complete, const char *fil_complete,
                      const char *referer_adr, const char *referer_fil,
                      char *save) {
  static const char *iisBogus[] = { ".com", ".exe", ".dll", ".sh", NULL };
  static const char *iisBogusReplace[] = { ".c0m", ".ex3", ".dl1", ".5h", NULL };       /* MUST be the same sizes */
  char *a;

  /* Call parent functions if multiple callbacks are chained. */
  if (CALLBACKARG_PREV_FUN(carg, savename) != NULL) {
    if (!CALLBACKARG_PREV_FUN(carg, savename)
        (CALLBACKARG_PREV_CARG(carg), opt, adr_complete, fil_complete,
         referer_adr, referer_fil, save)) {
      return 0;                 /* Abort */
    }
  }

  /* Process */
  for(a = save; *a != '\0'; a++) {
    int i;

    for(i = 0; iisBogus[i] != NULL; i++) {
      int j;

      for(j = 0; iisBogus[i][j] == a[j] && iisBogus[i][j] != '\0'; j++) ;
      if (iisBogus[i][j] == '\0'
          && (a[j] == '\0' || a[j] == '/' || a[j] == '\\')) {
        strncpy(a, iisBogusReplace[i], strlen(iisBogusReplace[i]));
        break;
      }
    }
  }

  return 1;                     /* success */
}
