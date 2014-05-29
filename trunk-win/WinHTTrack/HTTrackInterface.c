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


Important notes:

- We hereby ask people using this source NOT to use it in purpose of grabbing
emails addresses, or collecting any other private information on persons.
This would disgrace our work, and spoil the many hours we spent on it.


Please visit our Website: http://www.httrack.com
*/


/* ------------------------------------------------------------ */
/* File: Interface                                              */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

//#include "htsglobal.h"
//#include "htsbase.h"

#include "htsopt.h"
#include "HTTrackInterface.h"

// Lecture d'une ligne (peut être unicode à priori)
int linput(FILE* fp,char* s,int max) {
  int c;
  int j=0;
  do {
    c=fgetc(fp);
    if (c!=EOF) {
      switch(c) {
        case 13: break;  // sauter CR
        case 10: c=-1; break;
        case 9: case 12: break;  // sauter ces caractères
        default: s[j++]=(char) c; break;
      }
    }
  }  while((c!=-1) && (c!=EOF) && (j<(max-1)));
  s[j]='\0';
  return j;
}
int linput_trim(FILE* fp,char* s,int max) {
  int rlen=0;
  char* ls=(char*) malloct(max+2);
  s[0]='\0';
  if (ls) {
    char* a;
    // lire ligne
    rlen=linput(fp,ls,max);
    if (rlen) {
      // sauter espaces et tabs en fin
      while( (rlen>0) && ((ls[max(rlen-1,0)]==' ') || (ls[max(rlen-1,0)]=='\t')) )
        ls[--rlen]='\0';
      // sauter espaces en début
      a=ls;
      while((rlen>0) && ((*a==' ') || (*a=='\t'))) {
        a++;
        rlen--;
      }
      if (rlen>0) {
        memcpy(s,a,rlen);      // can copy \0 chars
        s[rlen]='\0';
      }
    }
    //
    freet(ls);
  }
  return rlen;
}
int linput_cpp(FILE* fp,char* s,int max) {
  int rlen=0;
  s[0]='\0';
  do {
    int ret;
    if (rlen>0)
    if (s[rlen-1]=='\\')
      s[--rlen]='\0';      // couper \ final
    // lire ligne
    ret=linput_trim(fp,s+rlen,max-rlen);
    if (ret>0)
      rlen+=ret;
  } while((s[max(rlen-1,0)]=='\\') && (rlen<max));
  return rlen;
}

// idem avec les car spéciaux
void rawlinput(FILE* fp,char* s,int max) {
  int c;
  int j=0;
  do {
    c=fgetc(fp);
    if (c!=EOF) {
      switch(c) {
        case 13: break;  // sauter CR
        case 10: c=-1; break;
        default: s[j++]=(char) c; break;
      }
    }
  }  while((c!=-1) && (c!=EOF) && (j<(max-1)));
  s[j++]='\0';
}

// Like linput, but in memory (optimized)
int binput(char* buff,char* s,int max) {
  int count = 0;
  int destCount = 0;

  // Note: \0 will return 1
  while(destCount < max && buff[count] != '\0' && buff[count] != '\n') {
    if (buff[count] != '\r') {
      s[destCount++] = buff[count];
    }
		count++;
  }
  s[destCount] = '\0';

  // then return the supplemental jump offset
  return count + 1;
} 

int fexist(const char* s) {
  struct stat st;
  memset(&st, 0, sizeof(st));
  if (stat(s, &st) == 0) {
    if (S_ISREG(st.st_mode)) {
      return 1;
    }
  }
  return 0;
} 

size_t fsize(const char* s) {
  FILE* fp;
  fp=fopen(s,"rb");
  if (fp!=NULL) {
    size_t i;
    fseek(fp,0,SEEK_END);
    i = ftell(fp);
    fclose(fp);
    return i;
  } else
    return -1;
}

TStamp time_local(void) {
  return ((TStamp) time(NULL));
}

/* / et \\ en / */
//static char* __fslash(char* a) {
//  int i;
//  for(i=0;i<(int) strlen(a);i++)
//    if (a[i]=='\\')  // convertir
//      a[i]='/';
//  return a;
//}
//
//char* fslash(char *catbuff,const char* a) {
//  return __fslash(concat(catbuff,a,""));
//}

// conversion minuscules, avec buffer
char* convtolower(char* catbuff,const char* a) {
  strcpybuff(catbuff, a);
  hts_lowcase(catbuff);  // lower case
	return catbuff;
}

// conversion en minuscules
void hts_lowcase(char* s) {
  int i;
  for(i=0;i<(int) strlen(s);i++)
    if ((s[i]>='A') && (s[i]<='Z'))
      s[i]+=('a'-'A');
}
