/*
    HTTrack external callbacks example : changing folder names ending with ".com"
    with ".c0m" as a workaround of IIS bug (see KB 275601)

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

/*
  Replaces all "offending" IIS extensions (exe, dll..) with "nice" ones
*/
/*
"check-html" callback
from htsdefines.h:
typedef int   (* t_hts_htmlcheck_savename)(char* adr_complete,char* fil_complete,char* referer_adr,char* referer_fil,char* save);
*/
EXTERNAL_FUNCTION int mysavename(char* adr_complete, char* fil_complete, char* referer_adr, char* referer_fil, char* save) {
  static const char* iisBogus[]        = { ".com", ".exe", ".dll", ".sh", NULL };
  static const char* iisBogusReplace[] = { ".c0m", ".ex3", ".dl1", ".5h", NULL }; /* MUST be the same sizes */
  char* a;
  for(a = save ; *a != '\0' ; a++) {
    int i;
    for(i = 0 ; iisBogus[i] != NULL ; i++) {
      int j;
      for(j = 0 ; iisBogus[i][j] == a[j] && iisBogus[i][j] != '\0' ; j++);
      if (iisBogus[i][j] == '\0' && ( a[j] == '\0' || a[j] == '/' || a[j] == '\\' ) ) {
        strncpy(a, iisBogusReplace[i], strlen(iisBogusReplace[i]));
        break;
      }
    }
  }
  return 1;  /* success */
}
