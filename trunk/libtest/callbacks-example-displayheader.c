/*
    HTTrack external callbacks example : display all incoming request headers
    Example of <wrappername>_init and <wrappername>_exit call (httrack >> 3.31)
    .c file

    How to use:
    - compile this file as a module (callback.so or callback.dll)
      example:
      (with gcc)
      gcc -O -g3 -Wall -D_REENTRANT -DINET6 -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -shared -o callback.so callbacks-example-contentfilter.c
      or (with visual c++)
      cl -LD -nologo -W3 -Zi -Zp4 -DWIN32 -Fe"callback.dll" callbacks-example-displayheader.c
    - use the --wrapper option in httrack:
      httrack --wrapper save-name=callback:process,string[,string..]
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "httrack-library.h"

/* "External" */
#ifdef _WIN32
#define EXTERNAL_FUNCTION __declspec(dllexport)
#else
#define EXTERNAL_FUNCTION 
#endif

/* Function definitions */
EXTERNAL_FUNCTION int process(char* buff, char* adr, char* fil, char* referer_adr, char* referer_fil, void* incoming);
EXTERNAL_FUNCTION int wrapper_init(char* module, char* initString);
EXTERNAL_FUNCTION int wrapper_exit(void);

/*
"receive-header" callback
from htsdefines.h:
typedef int   (* t_hts_htmlcheck_receivehead)(char* buff, char* adr, char* fil, char* referer_adr, char* referer_fil, htsblk* incoming);
*/
EXTERNAL_FUNCTION int process(char* buff, char* adr, char* fil, char* referer_adr, char* referer_fil, void* incoming) {
  printf("[ %s%s ]\n%s\n", adr, fil, buff);
  return 1;   /* success */
}

/* <wrappername>_init() will be called, if exists, upon startup */
static char* thisModule = NULL;
EXTERNAL_FUNCTION int wrapper_init(char* module, char* initString) {
  fprintf(stderr, "Plugged %s\n", module);
  thisModule = module;
  return 1;  /* success */
}

/* <wrappername>_exit() will be called, if exists, upon exit */
EXTERNAL_FUNCTION int wrapper_exit(void) {
  fprintf(stderr, "Unplugged %s\n", thisModule);
  return 1;   /* success (result ignored anyway in xx_exit) */
}
