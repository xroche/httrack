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

#include "stdio.h"
#include "htssystem.h"
#include "htscore.h"
#include "htsjava.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "htsnostatic.h"

//#include <math.h>

#ifndef HTS_LITTLE_ENDIAN
#define REVERSE_ENDIAN 1
#else
#define REVERSE_ENDIAN 0
#endif

/* big/little endian swap */
#define hts_swap16(A) ( (((A) & 0xFF)<<8) | (((A) & 0xFF00)>>8) )
#define hts_swap32(A) ( (( (hts_swap16(A)) & 0xFFFF)<<16) | (( (hts_swap16(A>>16)) & 0xFFFF)) )


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

int hts_parse_java(char *file,char* err_msg)
{
  FILE *fpout;
  JAVA_HEADER header;
  RESP_STRUCT *tab;
  
#if JAVADEBUG
  printf("fopen\n");
#endif
  if ((fpout = fopen(fconv(file), "r+b")) == NULL)
  {
    //fprintf(stderr, "Cannot open input file.\n");
    sprintf(err_msg,"Unable to open file %s",file);
    return 0;   // une erreur..
  }
    
#if JAVADEBUG
  printf("fread\n");
#endif
  //if (fread(&header,1,sizeof(JAVA_HEADER),fpout) != sizeof(JAVA_HEADER)) {   // pas complet..
  if (fread(&header,1,10,fpout) != 10) {   // pas complet..
    fclose(fpout);
    sprintf(err_msg,"File header too small (file len = "LLintP")",(LLint)fsize(file));
    return 0;
  }

#if JAVADEBUG
  printf("header\n");
#endif
  // tester en tête
#if REVERSE_ENDIAN
  header.magic = hts_swap32(header.magic);
  header.count = hts_swap16(header.count); 
#endif        
  if(header.magic!=0xCAFEBABE) {
    sprintf(err_msg,"non java file");
    if (fpout) { fclose(fpout); fpout=NULL; }
    return 0;
  }

  tab =(RESP_STRUCT*)calloct(header.count,sizeof(RESP_STRUCT));
  if (!tab) {
    sprintf(err_msg,"Unable to alloc %d bytes",(int)sizeof(RESP_STRUCT));
    if (fpout) { fclose(fpout); fpout=NULL; }
    return 0;    // erreur..
  }

#if JAVADEBUG
  printf("calchead\n");
#endif
  {
    int i;
    
    for (i = 1; i < header.count; i++) {
      int err=0;  // ++    
      tab[i]=readtable(fpout,tab[i],&err,err_msg);
      if (!err) {
        if ((tab[i].type == HTS_LONG) ||(tab[i].type == HTS_DOUBLE)) i++;  //2 element si double ou float
      } else {    // ++ une erreur est survenue!
        if (strnotempty(err_msg)==0)
          strcpy(err_msg,"Internal readtable error");
        freet(tab);
        if (fpout) { fclose(fpout); fpout=NULL; }
        return 0;
      }
    }
    
  }

  
#if JAVADEBUG
  printf("addfiles\n");
#endif
  {
    unsigned int acess;
    unsigned int Class;
    unsigned int SClass;
    int i;
    acess = readshort(fpout);
    Class = readshort(fpout);
    SClass = readshort(fpout);
    
    for (i = 1; i <header.count; i++) {
      
      if (tab[i].type == HTS_CLASS) {
        
        if ((tab[i].index1<header.count) && (tab[i].index1>=0)) {
          
          
          if((tab[i].index1!=SClass) && (tab[i].index1!=Class) && (tab[tab[i].index1].name[0]!='[')) {
            
            if(!strstr(tab[tab[i].index1].name,"java/")) {
              char tempo[1024];
              tempo[0]='\0';
              
              sprintf(tempo,"%s.class",tab[tab[i].index1].name);
#if JAVADEBUG
              printf("add %s\n",tempo);
#endif
              if (tab[tab[i].index1].file_position >= 0)
                hts_add_file(tempo,tab[tab[i].index1].file_position);
            }
            
          }
        } else { 
          i=header.count;  // exit 
        }
      }
      
    }
  }
  
 
#if JAVADEBUG
  printf("end\n");
#endif
  freet(tab);
  if (fpout) { fclose(fpout); fpout=NULL; }
  return 1;
}




// error: !=0 si erreur fatale
RESP_STRUCT readtable(FILE *fp,RESP_STRUCT trans,int* error,char* err_msg)
{
  unsigned short int length;
  int j;
  *error = 0;  // pas d'erreur
  trans.file_position=-1;
  trans.type = (int)(unsigned char)fgetc(fp);
  switch (trans.type) {
  case HTS_CLASS:
    strcpy(trans.name,"Class");
    trans.index1 = readshort(fp);
    break;
    
  case HTS_FIELDREF:
    strcpy(trans.name,"Field Reference");
    trans.index1 = readshort(fp);
    readshort(fp);
    break;
    
  case HTS_METHODREF:
    strcpy(trans.name,"Method Reference");
    trans.index1 = readshort(fp);
    readshort(fp);
    break;
    
  case HTS_INTERFACE:
    strcpy(trans.name,"Interface Method Reference");
    trans.index1 =readshort(fp);
    readshort(fp);
    break;
  case HTS_NAMEANDTYPE:
    strcpy(trans.name,"Name and Type");
    trans.index1 = readshort(fp);
    readshort(fp);
    break;
    
  case HTS_STRING:                // CONSTANT_String
    strcpy(trans.name,"String");
    trans.index1 = readshort(fp);
    break;
    
  case HTS_INTEGER:
    strcpy(trans.name,"Integer");
    for(j=0;j<4;j++) fgetc(fp);
    break;
    
  case HTS_FLOAT:
    strcpy(trans.name,"Float");
    for(j=0;j<4;j++) fgetc(fp);
    break;
    
  case HTS_LONG:
    strcpy(trans.name,"Long");
    for(j=0;j<8;j++) fgetc(fp);
    break;
  case HTS_DOUBLE:
    strcpy(trans.name,"Double");
    for(j=0;j<8;j++) fgetc(fp);
    break;
    
  case HTS_ASCIZ:
  case HTS_UNICODE:
    
    if (trans.type == HTS_ASCIZ)
      strcpy(trans.name,"HTS_ASCIZ");
    else
      strcpy(trans.name,"HTS_UNICODE");
    
    {
      char buffer[1024]; 
      char *p;
      
      p=&buffer[0];

      //fflush(fp); 
      trans.file_position=ftell(fp);
      length = readshort(fp);
      if (length<HTS_URLMAXSIZE) {
        // while ((length > 0) && (length<500)) {
        while (length > 0) {
          *p++ =fgetc(fp);
          
          length--;
        }
        *p='\0';
        
        //#if JDEBUG
        //      if(tris(buffer)==1) printf("%s\n ",buffer);
        //      if(tris(buffer)==2)  printf("%s\n ",printname(buffer));
        //#endif
        if(tris(buffer)==1)  hts_add_file(buffer,trans.file_position);       
        else if(tris(buffer)==2)  hts_add_file(printname(buffer),trans.file_position);

        strcpy(trans.name,buffer);
      } else {    // gros pb
        while ( (length > 0) && (!feof(fp))) {
          fgetc(fp);
          length--;
        }
        if (!feof(fp)) {
          trans.type=-1;
        } else {
          sprintf(err_msg,"Internal stucture error (ASCII)");
          *error = 1;
        }
        return(trans);
      }
    }
    break;
  default:
    // printf("Type inconnue\n");
    // on arrête tout 
    sprintf(err_msg,"Internal structure unknown (type %d)",trans.type);
    *error = 1;
    return(trans);
    break;
  }  
  return(trans);
}


unsigned short int readshort(FILE *fp)
{
  unsigned short int valint;
  fread(&valint,sizeof(valint),1,fp);

#if REVERSE_ENDIAN
  return hts_swap16(valint);
#else
  return valint;
#endif
  
}

int tris(char * buffer)
{
  //
  // Java
  if((buffer[0]=='[') && buffer[1]=='L' && (!strstr(buffer,"java/")) ) 
    return 2;
  if (strstr(buffer,".gif") || strstr(buffer,".jpg") || strstr(buffer,".jpeg") || strstr(buffer,".au") ) 
    return 1;
  // Ajouts R.X: test type
  // Autres fichiers
  {
    char type[256];
    type[0]='\0';
    get_httptype(type,buffer,0);
    if (strnotempty(type))     // type reconnu!
      return 1;
    // ajout RX 05/2001
    else if (is_dyntype(get_ext(buffer)))   // asp,cgi...
      return 1;
  }
  return 0;
}


char * printname(char  name[1024])
{
  char* rname;
  //char *rname;
  char *p;
  char *p1;
  int j;
  NOSTATIC_RESERVE(rname, char, 1024);
  rname[0]='\0';
  //
 
  p=&name[0];
  
  if(*p!='[')  return "";
  p+=2;
  //rname=(char*)calloct(strlen(name)+8,sizeof(char));
  p1=rname;
  for (j = 0; j < (int) strlen(name); j++,p++) {
    if (*p == '/') *p1='.'; 
    if (*p==';'){*p1='\0';
    strcat(rname,".class");
    return (rname);}
    else *p1=*p;
    p1++;
  }
  p1-=3;
  *p1='\0';
  return (rname);
  
}
