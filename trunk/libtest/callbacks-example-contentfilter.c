/*
    HTTrack external callbacks example : crawling html pages depending on content
    Example of <wrappername>_init and <wrappername>_exit call (httrack >> 3.31)
    .c file

    How to use:
    - compile this file as a module (callback.so or callback.dll)
      example:
      (with gcc)
      gcc -O -g3 -Wall -D_REENTRANT -DINET6 -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -shared -o callback.so callbacks-example-contentfilter.c
      or (with visual c++)
      cl -LD -nologo -W3 -Zi -Zp4 -DWIN32 -Fe"callback.dll" callbacks-example-contentfilter.c
    - use the --wrapper option in httrack:
      httrack --wrapper save-name=callback:process,string[,string..]
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
EXTERNAL_FUNCTION int process(char* html, int len, char* address, char* filename);
EXTERNAL_FUNCTION int wrapper_init(char* module, char* initString);
EXTERNAL_FUNCTION int wrapper_exit(void);

/* TOLOWER */
#define TOLOWER_(a) (a >= 'A' && a <= 'Z') ? (a + ('a' - 'A')) : a
#define TOLOWER(a) ( TOLOWER_( (a) ) )

/*
  This sample just crawls pages that contains certain keywords, and skips the other ones
*/

static char stringfilter[8192];
static char* stringfilters[128];
static int initialized = 0;

/*
"check-html" callback
from htsdefines.h:
typedef int   (* t_hts_htmlcheck)(char* html,int len,char* address,char* filename);
*/
EXTERNAL_FUNCTION int process(char* html, int len, char* address, char* filename) {
  int i = 0;
  int getIt = 0;
  char* pos;
  if (!initialized) {
    fprintf(stderr, "** ERROR! process_init() was not called by httrack - you are probably using an old version (<3.31)\n");
    fprintf(stderr, "** bailing out..\n");
    exit(1);
  }
  if (strcmp(address, "primary") == 0 && strcmp(filename, "/primary") == 0)      /* primary page (list of links) */
    return 1;
  while(stringfilters[i] != NULL && ! getIt) {
    if ( ( pos = strstr(html, stringfilters[i]) ) != NULL) {
      int j;
      getIt = 1;
      fprintf(stderr, "** callback info: found '%s' keyword in '%s%s', crawling this page!\n", stringfilters[i], address, filename);
      fprintf(stderr, "** details:\n(..)");
      for(j = 0; j < 72 && pos[j] ; j++) {
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
    return 1;  /* success */
  } else {
    fprintf(stderr, "** callback info: won't parse '%s%s' (no specified keywords found)\n", address, filename);
    return 0;  /* this page sucks, don't parse it */
  }
}

/* <wrappername>_init() will be called, if exists, upon startup */
EXTERNAL_FUNCTION int wrapper_init(char* module, char* initString) {
  char* a = stringfilter;
  int i = 0;
  fprintf(stderr, "** info: wrapper_init(%s, %s) called!\n", module, initString);
  fprintf(stderr, "** callback example: crawling pages only if specific keywords are found\n");
  if (initString == NULL || *initString == '\0') {
    fprintf(stderr, "** callback error: arguments expected or bad arguments\n");
    fprintf(stderr, "usage: httrack --wrapper save-name=callback:process,stringtofind,stringtofind..\n");
    fprintf(stderr, "example: httrack --wrapper save-name=callback:process,apple,orange,lemon\n");
    return 0;
  }

  /* stringfilters = split(initString, ','); */
  strcpy(stringfilter, initString);
  while(a != NULL) {
    stringfilters[i] = a;
    a = strchr(a, ',');
    if (a != NULL) {
      *a = '\0';
      a ++;
    }
    fprintf(stderr, "** callback info: will crawl pages with '%s' in them\n", stringfilters[i]);
    i++;
  }
  stringfilters[i++] = NULL;
  initialized = 1;      /* we're ok */
  return 1;  /* success */
}

/* <wrappername>_exit() will be called, if exists, upon exit */
EXTERNAL_FUNCTION int wrapper_exit(void) {
  fprintf(stderr, "** info: wrapper_exit() called!\n");
  initialized = 0;
  return 1;   /* success (result ignored anyway in xx_exit) */
}
