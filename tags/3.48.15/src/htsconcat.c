/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998-2013 Xavier Roche and other contributors

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.

Important notes:

- We hereby ask people using this source NOT to use it in purpose of grabbing
emails addresses, or collecting any other private information on persons.
This would disgrace our work, and spoil the many hours we spent on it.

Please visit our Website: http://www.httrack.com
*/

/* ------------------------------------------------------------ */
/* File: Subroutines                                            */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "httrack.h"
#include "httrack-library.h"

// concat, concatène deux chaines et renvoi le résultat
// permet d'alléger grandement le code
#undef concat
HTSEXT_API char *concat(char *catbuff, size_t size, const char *a, const char *b) {
  size_t max = 0;

  RUNTIME_TIME_CHECK_SIZE(size);

  catbuff[0] = '\0';
  if (a != NULL && a[0] != '\0') {
    max += strlen(a);
    if (max + 1 >= size) {
      return catbuff;
    }
    strcat(catbuff, a);
  }
  if (b != NULL && b[0] != '\0') {
    max += strlen(b);
    if (max + 1 >= size) {
      return catbuff;
    }
    strcat(catbuff, b);
  }
  return catbuff;
}

// conversion fichier / -> antislash
static char *__fconv(char *a) {
#if HTS_DOSNAME
  int i;

  for(i = 0; a[i] != 0; i++)
    if (a[i] == '/')            // Unix-to-DOS style
      a[i] = '\\';
#endif
  return a;
}

#undef fconcat
#undef concat
HTSEXT_API char *fconcat(char *catbuff, size_t size, const char *a, const char *b) {
  RUNTIME_TIME_CHECK_SIZE(size);
  return __fconv(concat(catbuff, size, a, b));
}

#undef fconv
HTSEXT_API char *fconv(char *catbuff, size_t size, const char *a) {
  RUNTIME_TIME_CHECK_SIZE(size);
  return __fconv(concat(catbuff, size, a, ""));
}

/* / et \\ en / */
static char *__fslash(char *a) {
  int i;

  for(i = 0; a[i] != 0; i++)
    if (a[i] == '\\')           // convertir
      a[i] = '/';
  return a;
}

#undef fslash
char *fslash(char *catbuff, size_t size, const char *a) {
  RUNTIME_TIME_CHECK_SIZE(size);
  return __fslash(concat(catbuff, size, a, NULL));
}

// extension : html,gif..
HTSEXT_API const char *get_ext(char *catbuff, size_t size, const char *fil) {
  size_t i, last;

  RUNTIME_TIME_CHECK_SIZE(size);

  for(i = 0, last = 0 ; fil[i] != '\0' && fil[i] != '?' ; i++) {
    if (fil[i] == '.') {
      last = i + 1;
    }
  }

  if (last != 0 && i > last) {
    const size_t len = i - last;
    if (len < size) {
      catbuff[0] = '\0';
      strncat(catbuff, &fil[last], size);
      return catbuff;
    }
  }
  return "";
}
