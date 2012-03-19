/*
    HTTrack external callbacks example : changing the destination filename
    .c file

    How to use:
    - compile this file as a module (callback.so or callback.dll)
      example:
      (with gcc)
      gcc -O -g3 -Wall -D_REENTRANT -DINET6 -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -shared -o callback.so callbacks-example-filename.c
      or (with visual c++)
      cl -LD -nologo -W3 -Zi -Zp4 -DWIN32 -Fe"callback.dll" callbacks-example-filename.c
    - use the --wrapper option in httrack:
      httrack --wrapper save-name=callback:mysavename
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
"check-html" callback
from htsdefines.h:
typedef int   (* t_hts_htmlcheck_savename)(char* adr_complete,char* fil_complete,char* referer_adr,char* referer_fil,char* save);
*/
EXTERNAL_FUNCTION int mysavename(char* adr_complete, char* fil_complete, char* referer_adr, char* referer_fil, char* save) {
  char* a = save;
  while(*a) {
    char c = TOLOWER(*a);
    if (c >= 'a' && c <= 'z')
      *a = ( ( ( c - 'a' ) + 13 ) % 26 ) + 'a';     // ROT-13
    a++;
  }
  
  return 1;  /* success */
}
