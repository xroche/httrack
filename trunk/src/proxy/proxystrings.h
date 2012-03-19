/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) Xavier Roche and other contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

Please visit our Website: http://www.httrack.com
*/


/* ------------------------------------------------------------ */
/* File: Strings                                                */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

// Strings a bit safer than static buffers

#ifndef HTS_STRINGS_DEFSTATIC
#define HTS_STRINGS_DEFSTATIC 

typedef struct String {
  char* buff;
  int len;
  int capa;
} String;

#define STRING_EMPTY {NULL, 0, 0}
#define STRING_BLK_SIZE 256
#define StringBuff(blk) ((blk).buff)
#define StringLength(blk) ((blk).len)
#define StringCapacity(blk) ((blk).capa)
#define StringRoom(blk, size) do { \
  if ((blk).len + (int)(size) + 1 > (blk).capa) { \
    (blk).capa = ((blk).len + (size) + 1) * 2; \
    (blk).buff = (char*) realloc((blk).buff, (blk).capa); \
  } \
} while(0)
#define StringBuffN(blk, size) StringBuffN_(&(blk), size) 
static char* StringBuffN_(String* blk, int size) {
  StringRoom(*blk, (blk->len) + size);
  return StringBuff(*blk);
}
#define StringClear(blk) do { \
  StringRoom(blk, 0); \
  (blk).buff[0] = '\0'; \
  (blk).len = 0; \
} while(0)
#define StringFree(blk) do { \
  if ((blk).buff != NULL) { \
    free((blk).buff); \
    (blk).buff = NULL; \
  } \
  (blk).capa = 0; \
  (blk).len = 0; \
} while(0)
#define StringMemcat(blk, str, size) do { \
  StringRoom(blk, size); \
  if ((int)(size) > 0) { \
    memcpy((blk).buff + (blk).len, (str), (size)); \
    (blk).len += (size); \
  } \
  *((blk).buff + (blk).len) = '\0'; \
} while(0)
#define StringAddchar(blk, c) do { \
  char __c = (c); \
  StringMemcat(blk, &__c, 1); \
} while(0)
static void* StringAcquire(String* blk) {
  void* buff = blk->buff;
  blk->buff = NULL;
  blk->capa = 0;
  blk->len = 0;
  return buff;
}
static StringAttach(String* blk, char** str) {
	StringFree(*blk);
	if (str != NULL && *str != NULL) {
		blk->buff = *str;
		blk->capa = (int)strlen(blk->buff);
		blk->len = blk->capa;
		*str = NULL;
	}
}
#define StringStrcat(blk, str) StringMemcat(blk, str, ((str) != NULL) ? (int)strlen(str) : 0)
#define StringStrcpy(blk, str) do { \
  StringClear(blk); \
  StringStrcat(blk, str); \
} while(0)

/* Tools */

static int ehexh(char c) {
  if ((c>='0') && (c<='9')) return c-'0';
  if ((c>='a') && (c<='f')) c-=('a'-'A');
  if ((c>='A') && (c<='F')) return (c-'A'+10);
  return 0;
}

static int ehex(const char* s) {
  return 16*ehexh(*s)+ehexh(*(s+1));
}

static void unescapehttp(const char* s, String* tempo) {
  int i;
  for (i = 0; s[i] != '\0' ; i++) {
    if (s[i]=='%' && s[i+1]=='%') {
      i++;
      StringAddchar(*tempo, '%');
    } else if (s[i]=='%') {
      char hc;
      i++;
      hc = (char) ehex(s+i);
      StringAddchar(*tempo, (char) hc);
      i++;    // sauter 2 caractères finalement
    }
    else if (s[i]=='+') {
      StringAddchar(*tempo, ' ');
    }
    else
      StringAddchar(*tempo, s[i]);
  }
}

static void escapexml(const char* s, String* tempo) {
  int i;
  for (i=0 ; s[i] != '\0' ; i++) {
    if (s[i] == '&')
      StringStrcat(*tempo, "&amp;");
		else if (s[i] == '<')
      StringStrcat(*tempo, "&lt;");
		else if (s[i] == '>')
      StringStrcat(*tempo, "&gt;");
		else if (s[i] == '\"')
      StringStrcat(*tempo, "&quot;");
    else
      StringAddchar(*tempo, s[i]);
  }
}

#endif
