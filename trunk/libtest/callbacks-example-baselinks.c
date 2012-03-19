/*
    HTTrack external callbacks example : enforce a constant base href
	Can be useful to make copies of site's archives using site's URL base href as root reference
    .c file

    How to use:
    - compile this file as a module (callback.so or callback.dll)
      example:
      (with gcc)
      gcc -O -g3 -Wall -D_REENTRANT -DINET6 -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -shared -o callback.so callbacks-example.c
      or (with visual c++)
      cl -LD -nologo -W3 -Zi -Zp4 -DWIN32 -Fe"callback.dll" callbacks-example.c
    - use the --wrapper option in httrack:
      httrack --wrapper check-html=callback:process_file
              --wrapper link-detected=callback:check_detectedlink
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
EXTERNAL_FUNCTION int process_file(char* html, int len, char* url_adresse, char* url_fichier);
EXTERNAL_FUNCTION int check_detectedlink(char* link);
EXTERNAL_FUNCTION int check_detectedlink_init(char* initString);
EXTERNAL_FUNCTION int check_detectedlink_exit(void);

static char base[HTS_URLMAXSIZE + 2] = "";

/*
"check-html" callback
typedef int   (* t_hts_htmlcheck)(char* html,int len,char* url_adresse,char* url_fichier);
*/
EXTERNAL_FUNCTION int process_file(char* html, int len, char* url_adresse, char* url_fichier) {
  /* Disable base href, if any */
  char* prevBase = strstr(html, "<BASE HREF=\"");
  if (prevBase != NULL) {
    prevBase[1] = 'X';
  }
  return 1;  /* success */
}

/*
"link-detected" callback
typedef int   (* t_hts_htmlcheck_linkdetected)(char* link);
*/
EXTERNAL_FUNCTION int check_detectedlink(char* link) {
  if (!base[0]) {
    fprintf(stderr, "** ERROR! check_detectedlink_init() was not called by httrack - you are probably using an old version (<3.31) or called the wrapper with bad arguments\n");
    fprintf(stderr, "** bailing out..\n");
    exit(1);
  }
  /* The incoming (read/write) buffer is at least HTS_URLMAXSIZE bytes long */
  if (strncmp(link, "http://", 7) == 0 || strncmp(link, "https://", 8) == 0) {
    char temp[HTS_URLMAXSIZE * 2];
	strcpy(temp, base);
	strcat(temp, link);
	strcpy(link, temp);
  }
  return 1;  /* success */
}

/* <wrappername>_init() will be called, if exists, upon startup */
EXTERNAL_FUNCTION int check_detectedlink_init(char* initString) {
  fprintf(stderr, "Plugged..\n");
  if (initString != NULL && *initString != '\0' && strlen(initString) < HTS_URLMAXSIZE / 2) {
    strcpy(base, initString);
    fprintf(stderr, "Using root '%s'\n", base);
    return 1;  /* success */
  } else {
    fprintf(stderr, "** callback error: arguments expected or bad arguments\n");
    fprintf(stderr, "usage: httrack --wrapper save-name=callback:mysavename,base\n");
	fprintf(stderr, "example: httrack --wrapper save-name=callback:mysavename,http://www.example.com/\n");
    return 0;  /* failed */
  }
}

/* <wrappername>_exit() will be called, if exists, upon exit */
EXTERNAL_FUNCTION int check_detectedlink_exit(void) {
  fprintf(stderr, "Unplugged ..\n");
  return 1;   /* success (result ignored anyway in xx_exit) */
}
