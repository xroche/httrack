/*
    HTTrack external callbacks example
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
              --wrapper loop=callback:check_loop
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

/*
  This sample just lists all links in documents with the parent link:
  <parent> -> <link>
  This sample can be improved, for example, to make a map of a website.
*/

static char currentURLBeingParsed[2048];

/*
"check-html" callback
typedef int   (* t_hts_htmlcheck)(char* html,int len,char* url_adresse,char* url_fichier);
*/
EXTERNAL_FUNCTION int process_file(char* html, int len, char* url_adresse, char* url_fichier) {
  printf("now parsing %s%s..\n", url_adresse, url_fichier);
  strcpy(currentURLBeingParsed, url_adresse);
  strcat(currentURLBeingParsed, url_fichier);
  return 1;  /* success */
}

/*
"link-detected" callback
typedef int   (* t_hts_htmlcheck_linkdetected)(char* link);
*/
EXTERNAL_FUNCTION int check_detectedlink(char* link) {
  printf("[%s] -> [%s]\n", currentURLBeingParsed, link);
  return 1;  /* success */
}

/*
"loop" callback
typedef int   (* t_hts_htmlcheck_loop)(void* back,int back_max,int back_index,int lien_tot,int lien_ntot,int stat_time,void* stats);
*/
EXTERNAL_FUNCTION int check_loop(void* back,int back_max,int back_index,int lien_tot,int lien_ntot,int stat_time,void* stats) {
  static int fun_animation=0;
  printf("%c\r", "/-\\|"[(fun_animation++)%4]);
  return 1;
}

/*
a default callback for testing purpose
*/
EXTERNAL_FUNCTION int check_void(void) {
  printf("\n* * * default callback function called! * * *\n\n");
  return 1;
}
