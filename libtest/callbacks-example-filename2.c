/*
    HTTrack external callbacks example : changing the destination filename
    Example of <wrappername>_init and <wrappername>_exit call (httrack >> 3.31)
    .c file

    How to use:
    - compile this file as a module (callback.so or callback.dll)
      example:
      (with gcc)
      gcc -O -g3 -Wall -D_REENTRANT -DINET6 -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -shared -o callback.so callbacks-example-filename.c
      or (with visual c++)
      cl -LD -nologo -W3 -Zi -Zp4 -DWIN32 -Fe"callback.dll" callbacks-example-filename.c
    - use the --wrapper option in httrack:
      httrack --wrapper save-name=callback:mysavename,string1,string2
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* "External" */
#ifdef _WIN32
#define EXTERNAL_FUNCTION __declspec(dllexport)
#else
#define EXTERNAL_FUNCTION 
#endif

/* Function definitions */
EXTERNAL_FUNCTION int mysavename(char* adr_complete, char* fil_complete, char* referer_adr, char* referer_fil, char* save);
EXTERNAL_FUNCTION int wrapper_init(char* module, char* initString);
EXTERNAL_FUNCTION int wrapper_exit(void);

/* TOLOWER */
#define TOLOWER_(a) (a >= 'A' && a <= 'Z') ? (a + ('a' - 'A')) : a
#define TOLOWER(a) ( TOLOWER_( (a) ) )

/*
  This sample just replaces all occurences of "string1" into "string2"
  string1 and string2 are passed in the callback string:
  httrack --wrapper save-name=callback:mysavename,string1,string2 ..
*/

static char string1[256];
static char string2[256];
static int initialized = 0;

/*
"check-html" callback
from htsdefines.h:
typedef int   (* t_hts_htmlcheck_savename)(char* adr_complete,char* fil_complete,char* referer_adr,char* referer_fil,char* save);
*/
EXTERNAL_FUNCTION int mysavename(char* adr_complete, char* fil_complete, char* referer_adr, char* referer_fil, char* save) {
  char* buff = strdup(save);
  char* a = buff;
  char* b = save;
  if (!initialized) {
    fprintf(stderr, "** ERROR! mysavename_init() was not called by httrack - you are probably using an old version (<3.31)\n");
    fprintf(stderr, "** bailing out..\n");
    exit(1);
  }
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

/* <wrappername>_init() will be called, if exists, upon startup */
EXTERNAL_FUNCTION int wrapper_init(char* module, char* initString) {
  char* pos;
  fprintf(stderr, "** info: wrapper_init(%s, %s) called!\n", module, initString);
  fprintf(stderr, "** callback example: changing destination filename word by another one\n");
  if (initString == NULL || *initString == '\0' || (pos = strchr(initString, ',') ) == NULL) {
    fprintf(stderr, "** callback error: arguments expected or bad arguments\n");
    fprintf(stderr, "usage: httrack --wrapper save-name=callback:mysavename,string1,string2\n");
    fprintf(stderr, "example: httrack --wrapper save-name=callback:mysavename,foo,bar\n");
    return 0;
  }
  string1[0] = string1[1] = '\0';
  strncat(string1, initString, pos - initString);
  strcpy(string2, pos + 1);
  fprintf(stderr, "** callback info: will replace %s by %s in filenames!\n", string1, string2);
  initialized = 1;      /* we're ok */
  return 1;  /* success */
}

/* <wrappername>_exit() will be called, if exists, upon exit */
EXTERNAL_FUNCTION int wrapper_exit(void) {
  fprintf(stderr, "** info: wrapper_exit() called!\n");
  initialized = 0;
  return 1;   /* success (result ignored anyway in xx_exit) */
}
