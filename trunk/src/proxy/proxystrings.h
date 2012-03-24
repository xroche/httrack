/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) Xavier Roche and other contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 3
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

#ifndef HTS_PROXYSTRINGS_DEFSTATIC
#define HTS_PROXYSTRINGS_DEFSTATIC 

#include "htsstrings.h"


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
      StringCat(*tempo, "&amp;");
		else if (s[i] == '<')
      StringCat(*tempo, "&lt;");
		else if (s[i] == '>')
      StringCat(*tempo, "&gt;");
		else if (s[i] == '\"')
      StringCat(*tempo, "&quot;");
    else
      StringAddchar(*tempo, s[i]);
  }
}

static char* concat(char *catbuff,const char* a,const char* b) {
	if (a != NULL && a[0] != '\0') {
		strcpy(catbuff, a);
	} else {
		catbuff[0] = '\0';
	}
	if (b != NULL && b[0] != '\0') {
		strcat(catbuff, b);
	}
  return catbuff;
}

static char* __fconv(char* a) {
#ifdef WIN32
  int i;
  for(i = 0 ; a[i] != 0 ; i++)
    if (a[i] == '/')  // Unix-to-DOS style
      a[i] = '\\';
#endif
  return a;
}

static char* fconcat(char *catbuff, const char* a, const char* b) {
  return __fconv(concat(catbuff,a,b));
}

static char* fconv(char *catbuff, const char* a) {
  return __fconv(concat(catbuff,a,""));
}

#endif
