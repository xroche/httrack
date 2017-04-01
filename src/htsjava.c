/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998-2017 Xavier Roche and other contributors

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
/* File: Java classes parser                                    */
/* Author: Yann Philippot                                       */
/* ------------------------------------------------------------ */

/* Version: Oct/2000 */
/* Fixed: problems with class structure (10/2000) */

// htsjava.c - Parseur de classes java

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#if ( defined(_WIN32) ||defined(HAVE_SYS_TYPES_H) )
#include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/* Standard httrack module includes */
#include "httrack-library.h"
#include "htsopt.h"
#include "htsdefines.h"

/* Module structures */
#include "htsmodules.h"

/* We link to libhttrack, we can use its functions */
#include "httrack-library.h"

/* This file */
#include "htsjava.h"

static int reverse_endian(void) {
  int endian = 1;

  return (*((char *) &endian) == 1);
}

/* big/little endian swap */
#define hts_swap16(A) ( (((A) & 0xFF)<<8) | (((A) & 0xFF00)>>8) )
#define hts_swap32(A) ( (( (hts_swap16(A)) & 0xFFFF)<<16) | (( (hts_swap16(A>>16)) & 0xFFFF)) )

/* Static definitions */
static RESP_STRUCT readtable(htsmoduleStruct * str, FILE * fp, RESP_STRUCT,
                             int *);
static unsigned short int readshort(FILE * fp);
static int tris(httrackp * opt, char *);
static char *printname(char[1024], char[1024]);

// ** HTS_xx sinon pas pris par VC++
#define HTS_CLASS  7
#define HTS_FIELDREF  9
#define HTS_METHODREF  10
#define HTS_STRING  8
#define HTS_INTEGER  3
#define HTS_FLOAT  4
#define HTS_LONG  5
#define HTS_DOUBLE  6
#define HTS_INTERFACE  11
#define HTS_NAMEANDTYPE  12
#define HTS_ASCIZ  1
#define HTS_UNICODE  2

#define JAVADEBUG 0

static const char *libName = "htsjava";

#ifdef _WIN32
#define strcasecmp(a,b) stricmp(a,b)
#define strncasecmp(a,b,n) strnicmp(a,b,n)
#endif

static int detect_mime(htsmoduleStruct * str) {
  const char *savename = str->filename;

  if (savename) {
    int len = (int) strlen(savename);

    if (len > 6 && strcasecmp(savename + len - 6, ".class") == 0) {
      return 1;
    }
  }
  return 0;
}

static int hts_detect_java(t_hts_callbackarg * carg, httrackp * opt,
                           htsmoduleStruct * str) {
  /* Call parent functions if multiple callbacks are chained. */
  if (CALLBACKARG_PREV_FUN(carg, detect) != NULL) {
    if (CALLBACKARG_PREV_FUN(carg, detect)
        (CALLBACKARG_PREV_CARG(carg), opt, str)) {
      return 1;                 /* Found before us, let them have the priority */
    }
  }

  /* Check MIME */
  if (detect_mime(str)) {
    str->wrapper_name = libName;        /* Our ID */
    return 1;                   /* Known format, we take it */
  }

  return 0;                     /* Unknown format */
}

static off_t fsize(const char *s) {
  STRUCT_STAT st;

  if (STAT(s, &st) == 0 && S_ISREG(st.st_mode)) {
    return st.st_size;
  } else {
    return -1;
  }
}

static int hts_parse_java(t_hts_callbackarg * carg, httrackp * opt,
                          htsmoduleStruct * str) {
  /* The wrapper_name memebr has changed: not for us anymore */
  if (str->wrapper_name == NULL || strcmp(str->wrapper_name, libName) != 0) {
    /* Call parent functions if multiple callbacks are chained. */
    if (CALLBACKARG_PREV_FUN(carg, parse) != NULL) {
      return CALLBACKARG_PREV_FUN(carg, parse) (CALLBACKARG_PREV_CARG(carg),
                                                opt, str);
    }
    strcpy(str->err_msg,
           "unexpected error: bad wrapper_name and no previous wrapper");
    return 0;                   /* Unexpected error */
  } else {
    if (detect_mime(str)) {

      /* (Legacy code) */
      char catbuff[CATBUFF_SIZE];
      FILE *fpout;
      JAVA_HEADER header;
      RESP_STRUCT *tab;
      const char *file = str->filename;

      str->relativeToHtmlLink = 1;

#if JAVADEBUG
      printf("fopen\n");
#endif
      if ((fpout = FOPEN(fconv(catbuff, sizeof(catbuff), file), "r+b")) == NULL) {
        //fprintf(stderr, "Cannot open input file.\n");
        sprintf(str->err_msg, "Unable to open file %s", file);
        return 0;               // une erreur..
      }
#if JAVADEBUG
      printf("fread\n");
#endif
      //if (fread(&header,1,sizeof(JAVA_HEADER),fpout) != sizeof(JAVA_HEADER)) {   // pas complet..
      if (fread(&header, 1, 10, fpout) != 10) { // pas complet..
        fclose(fpout);
        sprintf(str->err_msg, "File header too small (file len = " LLintP ")",
                (LLint) fsize(file));
        return 0;
      }
#if JAVADEBUG
      printf("header\n");
#endif
      // tester en tête
      if (reverse_endian()) {
        header.magic = hts_swap32(header.magic);
        header.count = hts_swap16(header.count);
      }
      if (header.magic != 0xCAFEBABE) {
        sprintf(str->err_msg, "non java file");
        if (fpout) {
          fclose(fpout);
          fpout = NULL;
        }
        return 0;
      }

      tab = (RESP_STRUCT *) calloc(header.count, sizeof(RESP_STRUCT));
      if (!tab) {
        sprintf(str->err_msg, "Unable to alloc %d bytes",
                (int) sizeof(RESP_STRUCT));
        if (fpout) {
          fclose(fpout);
          fpout = NULL;
        }
        return 0;               // erreur..
      }
#if JAVADEBUG
      printf("calchead\n");
#endif
      {
        int i;

        for(i = 1; i < header.count; i++) {
          int err = 0;          // ++    

          tab[i] = readtable(str, fpout, tab[i], &err);
          if (!err) {
            if ((tab[i].type == HTS_LONG) || (tab[i].type == HTS_DOUBLE))
              i++;              //2 element si double ou float
          } else {              // ++ une erreur est survenue!
            if (strnotempty(str->err_msg) == 0)
              strcpy(str->err_msg, "Internal readtable error");
            free(tab);
            if (fpout) {
              fclose(fpout);
              fpout = NULL;
            }
            return 0;
          }
        }

      }

#if JAVADEBUG
      printf("addfiles\n");
#endif
      {
        //unsigned int acess;
        unsigned int Class;
        unsigned int SClass;
        int i;

        //acess = readshort(fpout);
        Class = readshort(fpout);
        SClass = readshort(fpout);

        for(i = 1; i < header.count; i++) {

          if (tab[i].type == HTS_CLASS) {

            if ((tab[i].index1 < header.count) && (tab[i].index1 >= 0)) {

              if ((tab[i].index1 != SClass) && (tab[i].index1 != Class)
                  && (tab[tab[i].index1].name[0] != '[')) {

                if (!strstr(tab[tab[i].index1].name, "java/")) {
                  char BIGSTK tempo[1024];

                  tempo[0] = '\0';

                  sprintf(tempo, "%s.class", tab[tab[i].index1].name);
#if JAVADEBUG
                  printf("add %s\n", tempo);
#endif
                  if (tab[tab[i].index1].file_position >= 0)
                    str->addLink(str, tempo);   /* tab[tab[i].index1].file_position */
                }

              }
            } else {
              i = header.count; // exit 
            }
          }

        }
      }

#if JAVADEBUG
      printf("end\n");
#endif
      free(tab);
      if (fpout) {
        fclose(fpout);
        fpout = NULL;
      }
      return 1;

    } else {
      strcpy(str->err_msg, "bad MIME type");
    }
  }
  return 0;                     /* Error */
}

/*
module entry point 
*/
EXTERNAL_FUNCTION int hts_plug(httrackp * opt, const char *argv);
EXTERNAL_FUNCTION int hts_plug(httrackp * opt, const char *argv) {
  /* Plug callback functions */
  CHAIN_FUNCTION(opt, detect, hts_detect_java, NULL);
  CHAIN_FUNCTION(opt, parse, hts_parse_java, NULL);

  return 1;                     /* success */
}

// error: !=0 si erreur fatale
static RESP_STRUCT readtable(htsmoduleStruct * str, FILE * fp,
                             RESP_STRUCT trans, int *error) {
  char rname[1024];
  unsigned short int length;
  int j;

  *error = 0;                   // pas d'erreur
  trans.file_position = -1;
  trans.type = (int) (unsigned char) fgetc(fp);
  switch (trans.type) {
  case HTS_CLASS:
    strcpy(trans.name, "Class");
    trans.index1 = readshort(fp);
    break;

  case HTS_FIELDREF:
    strcpy(trans.name, "Field Reference");
    trans.index1 = readshort(fp);
    readshort(fp);
    break;

  case HTS_METHODREF:
    strcpy(trans.name, "Method Reference");
    trans.index1 = readshort(fp);
    readshort(fp);
    break;

  case HTS_INTERFACE:
    strcpy(trans.name, "Interface Method Reference");
    trans.index1 = readshort(fp);
    readshort(fp);
    break;
  case HTS_NAMEANDTYPE:
    strcpy(trans.name, "Name and Type");
    trans.index1 = readshort(fp);
    readshort(fp);
    break;

  case HTS_STRING:             // CONSTANT_String
    strcpy(trans.name, "String");
    trans.index1 = readshort(fp);
    break;

  case HTS_INTEGER:
    strcpy(trans.name, "Integer");
    for(j = 0; j < 4; j++)
      fgetc(fp);
    break;

  case HTS_FLOAT:
    strcpy(trans.name, "Float");
    for(j = 0; j < 4; j++)
      fgetc(fp);
    break;

  case HTS_LONG:
    strcpy(trans.name, "Long");
    for(j = 0; j < 8; j++)
      fgetc(fp);
    break;
  case HTS_DOUBLE:
    strcpy(trans.name, "Double");
    for(j = 0; j < 8; j++)
      fgetc(fp);
    break;

  case HTS_ASCIZ:
  case HTS_UNICODE:

    if (trans.type == HTS_ASCIZ)
      strcpy(trans.name, "HTS_ASCIZ");
    else
      strcpy(trans.name, "HTS_UNICODE");

    {
      char BIGSTK buffer[1024];
      char *p;

      p = &buffer[0];

      //fflush(fp); 
      trans.file_position = ftell(fp);
      length = readshort(fp);
      if (length < HTS_URLMAXSIZE) {
        // while ((length > 0) && (length<500)) {
        while(length > 0) {
          *p++ = fgetc(fp);

          length--;
        }
        *p = '\0';

        //#if JDEBUG
        //      if(tris(buffer)==1) printf("%s\n ",buffer);
        //      if(tris(buffer)==2)  printf("%s\n ",printname(buffer));
        //#endif
        if (tris(str->opt, buffer) == 1)
          str->addLink(str, buffer);    /* trans.file_position */
        else if (tris(str->opt, buffer) == 2)
          str->addLink(str, printname(rname, buffer));

        strcpy(trans.name, buffer);
      } else {                  // gros pb
        while((length > 0) && (!feof(fp))) {
          fgetc(fp);
          length--;
        }
        if (!feof(fp)) {
          trans.type = -1;
        } else {
          sprintf(str->err_msg, "Internal stucture error (ASCII)");
          *error = 1;
        }
        return (trans);
      }
    }
    break;
  default:
    // printf("Type inconnue\n");
    // on arrête tout 
    sprintf(str->err_msg, "Internal structure unknown (type %d)", trans.type);
    *error = 1;
    return (trans);
    break;
  }
  return (trans);
}

static unsigned short int readshort(FILE * fp) {
  unsigned short int valint;

  if (fread(&valint, sizeof(valint), 1, fp) == 1) {
    if (reverse_endian())
      return hts_swap16(valint);
    else
      return valint;
  } else {
    return 0;
  }

}

static int tris(httrackp * opt, char *buffer) {
  char catbuff[CATBUFF_SIZE];

  //
  // Java
  if ((buffer[0] == '[') && buffer[1] == 'L' && (!strstr(buffer, "java/")))
    return 2;
  if (strstr(buffer, ".gif") || strstr(buffer, ".jpg")
      || strstr(buffer, ".jpeg") || strstr(buffer, ".au"))
    return 1;
  // Ajouts R.X: test type
  // Autres fichiers
  {
    char type[256];

    type[0] = '\0';
    get_httptype(opt, type, buffer, 0);
    if (strnotempty(type))      // type reconnu!
      return 1;
    // ajout RX 05/2001
    else if (is_dyntype(get_ext(catbuff, sizeof(catbuff), buffer)))      // asp,cgi...
      return 1;
  }
  return 0;
}

static char *printname(char rname[1024], char name[1024]) {
  char *p;
  char *p1;
  size_t j;

  rname[0] = '\0';
  //

  p = &name[0];

  if (*p != '[') {
    return rname;  // ""
  }
  p += 2;
  //rname=(char*)calloct(strlen(name)+8,sizeof(char));
  p1 = rname;
  for(j = 0; name[j] != '\0'; j++, p++) {
    if (*p == '/')
      *p1 = '.';
    if (*p == ';') {
      *p1 = '\0';
      strcat(rname, ".class");
      return rname;
    } else
      *p1 = *p;
    p1++;
  }
  p1 -= 3;
  *p1 = '\0';
  return rname;

}
