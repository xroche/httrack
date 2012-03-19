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
/* File: httrack.c subroutines:                                 */
/*       basic authentication: password storage                 */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */


#include "htsbauth.h"

/* specific definitions */
#include "htsglobal.h"
#include "htslib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "htsnostatic.h"

/* END specific definitions */

// gestion des cookie
// ajoute, dans l'ordre
// !=0 : erreur
int cookie_add(t_cookie* cookie,char* cook_name,char* cook_value,char* domain,char* path) {
  char* a=cookie->data;
  char* insert;
  char cook[16384];
  // effacer éventuel cookie en double
  cookie_del(cookie,cook_name,domain,path);
  if ((int)strlen(cook_value)>1024) return -1;                              // trop long
  if ((int)strlen(cook_name)>256) return -1;                                // trop long
  if ((int)strlen(domain)>256) return -1;                                   // trop long
  if ((int)strlen(path)>256) return -1;                                     // trop long
  if ((int)(
    strlen(cookie->data)
    +strlen(cook_value)
    +strlen(cook_name)
    +strlen(domain)
    +strlen(path)
    +256
    ) > cookie->max_len) return -1;               // impossible d'ajouter

  insert=a;          // insérer ici
  while (*a) {
    if ( strlen(cookie_get(a,2)) <  strlen(path) )      // long. path (le + long est prioritaire)
      a=cookie->data+strlen(cookie->data);    // fin
    else {
      a=strchr(a,'\n');     // prochain champ
      if (a==NULL)
        a=cookie->data+strlen(cookie->data);    // fin
      else
        a++;
      while(*a=='\n') a++;
      insert=a;          // insérer ici
    }
  }
  // construction du cookie
  strcpy(cook,domain);
  strcat(cook,"\t");
  strcat(cook,"TRUE");
  strcat(cook,"\t");
  strcat(cook,path);
  strcat(cook,"\t");
  strcat(cook,"FALSE");
  strcat(cook,"\t");
  strcat(cook,"1999999999");
  strcat(cook,"\t");
  strcat(cook,cook_name);
  strcat(cook,"\t");
  strcat(cook,cook_value);
  strcat(cook,"\n");
  if (!( ((int) strlen(cookie->data) + (int) strlen(cook)) < cookie->max_len)) return -1;      // impossible d'ajouter
  cookie_insert(insert,cook);
#if DEBUG_COOK
  printf("add_new cookie: name=\"%s\" value=\"%s\" domain=\"%s\" path=\"%s\"\n",cook_name,cook_value,domain,path);
  //printf(">>>cook: %s<<<\n",cookie->data);
#endif
  return 0;
}

// effacer cookie si existe
int cookie_del(t_cookie* cookie,char* cook_name,char* domain,char* path) {
  char *a,*b;
  b=cookie_find(cookie->data,cook_name,domain,path);
  if (b) {
    a=cookie_nextfield(b);
    cookie_delete(b,(int) (a - b));
#if DEBUG_COOK
    printf("deleted old cookie: %s %s %s\n",cook_name,domain,path);
#endif
  }
  return 0;
}

// rechercher cookie à partir de la position s (par exemple s=cookie.data)
// renvoie pointeur sur ligne, ou NULL si introuvable
// path est aligné à droite et cook_name peut être vide (chercher alors tout cookie)
// .doubleclick.net	TRUE	/	FALSE	1999999999	id	A
char* cookie_find(char* s,char* cook_name,char* domain,char* path) {
  char* a=s;
  while (*a) {
    int t;
    if (strnotempty(cook_name)==0)
      t=1;                      // accepter par défaut
    else
      t=( strcmp(cookie_get(a,5),cook_name)==0 );     // tester si même nom
    if (t) {  // même nom ou nom qualconque
      //
      char* chk_dom=cookie_get(a,0);       // domaine concerné par le cookie
      if ((int) strlen(chk_dom) <= (int) strlen(domain)) {
        if ( strcmp(chk_dom,domain+strlen(domain)-strlen(chk_dom))==0 ) {  // même domaine
          //
          char* chk_path=cookie_get(a,2);       // chemin concerné par le cookie
          if ((int) strlen(chk_path) <= (int) strlen(path)) {
            if (strncmp(path,chk_path,strlen(chk_path))==0 ) { // même chemin
              return a;
            }
          }
        }
      }
    }
    a=cookie_nextfield(a);
  }
  return NULL;
}

// renvoie prochain champ
char* cookie_nextfield(char* a) {
  char* b=a;
  a=strchr(a,'\n');     // prochain champ
  if (a==NULL)
    a=b+strlen(b);    // fin
  else
    a++;
  while(*a=='\n') a++;
  return a;
}

// lire cookies.txt
// lire également (Windows seulement) les *@*.txt (cookies IE copiés)
// !=0 : erreur
int cookie_load(t_cookie* cookie,char* fpath,char* name) {
  cookie->data[0]='\0';

  // Fusionner d'abord les éventuels cookies IE
#if HTS_WIN
  {
    WIN32_FIND_DATA find;
    HANDLE h;
    char  pth[MAX_PATH + 32];
    strcpy(pth,fpath);
    strcat(pth,"*@*.txt");
    h = FindFirstFile(pth,&find);
    if (h != INVALID_HANDLE_VALUE) {
      do {
        if (!(find.dwFileAttributes  & FILE_ATTRIBUTE_DIRECTORY ))
          if (!(find.dwFileAttributes  & FILE_ATTRIBUTE_SYSTEM )) {
            FILE* fp=fopen(fconcat(fpath,find.cFileName),"rb");
            if (fp) {
              char cook_name[256];
              char cook_value[1000];
              char domainpathpath[512];
              //
              char domain[256];           // domaine cookie (.netscape.com)
              char path[256];             // chemin (/)
              int cookie_merged=0;
              linput(fp,cook_name,250);
              if (!feof(fp)) {
                linput(fp,cook_value,250);
                if ( (!feof(fp)) && (strnotempty(cook_value)) )  {
                  linput(fp,domainpathpath,500);
                  if (strnotempty(domainpathpath)) {
                    if (ident_url_absolute(domainpathpath,domain,path)>=0) {
                      cookie_add(cookie,cook_name,cook_value,domain,path);
                      cookie_merged=1;
                    }
                  }
                }
              }
              fclose(fp);
              if (cookie_merged)
                remove(fconcat(fpath,find.cFileName));
            }  // if fp
          }
      } while(FindNextFile(h,&find));
      FindClose(h);
    }
  }
#endif
  
  // Ensuite, cookies.txt
  {
    FILE* fp = fopen(fconcat(fpath,name),"rb");
    if (fp) {
      char line[8192];
      while( (!feof(fp)) && (((int) strlen(cookie->data)) < cookie->max_len)) {
        rawlinput(fp,line,8100);
        if (strnotempty(line)) {
          if (strlen(line)<8000) {
            if (line[0]!='#') {
              char domain[256];           // domaine cookie (.netscape.com)
              char path[256];             // chemin (/)
              char cook_name[256];        // nom cookie (MYCOOK)
              char cook_value[8192];      // valeur (ID=toto,S=1234)
              strcpy(domain,cookie_get(line,0));       // host
              strcpy(path,cookie_get(line,2));         // path
              strcpy(cook_name,cookie_get(line,5));    // name
              strcpy(cook_value,cookie_get(line,6));   // value
#if DEBUG_COOK
              printf("%s\n",line);
#endif
              cookie_add(cookie,cook_name,cook_value,domain,path);
            }
          }
        }
      }
      fclose(fp);
      return 0;
    }
  }
  return -1;
}

// écrire cookies.txt
// !=0 : erreur
int cookie_save(t_cookie* cookie,char* name) {
  if (strnotempty(cookie->data)) {
    char line[8192];
    FILE* fp = fopen(fconv(name),"wb");
    if (fp) {
      char* a=cookie->data;
      fprintf(fp,"# HTTrack Website Copier Cookie File"LF"# This file format is compatible with Netscape cookies"LF);
      do {
        a+=binput(a,line,8000);
        fprintf(fp,"%s"LF,line);
      } while(strnotempty(line));
      fclose(fp);
      return 0;
    }
  } else
    return 0;
  return -1;
}

// insertion chaine ins avant s
void cookie_insert(char* s,char* ins) {
  char* buff;
  if (strnotempty(s)==0) {    // rien à faire, juste concat
    strcat(s,ins);
  } else {
    buff=(char*) malloc(strlen(s)+2);
    if (buff) {
      strcpy(buff,s);     // copie temporaire
      strcpy(s,ins);      // insérer
      strcat(s,buff);     // copier
      free(buff);
    }
  }
}
// destruction chaine dans s position pos
void cookie_delete(char* s,int pos) {
  char* buff;
  if (strnotempty(s+pos)==0) {    // rien à faire, effacer
    s[0]='\0';
  } else {
    buff=(char*) malloc(strlen(s+pos)+2);
    if (buff) {
      strcpy(buff,s+pos);     // copie temporaire
      strcpy(s,buff);         // copier
      free(buff);
    }
  }
}

// renvoie champ param de la chaine cookie_base
// ex: cookie_get("ceci est<tab>un<tab>exemple",1) renvoi "un"
char* cookie_get(char* cookie_base,int param) {
  char* buffer;
  //
  char * limit;
  NOSTATIC_RESERVE(buffer, char, 8192);

  while(*cookie_base=='\n') cookie_base++;
  limit = strchr(cookie_base,'\n');
  if (!limit) limit=cookie_base+strlen(cookie_base);
  if (limit) {
    if (param) {
      int i;
      for(i=0;i<param;i++) {
        if (cookie_base) {
          cookie_base=strchr(cookie_base,'\t');       // prochain tab
          if (cookie_base) cookie_base++;
        }
      }
    }
    if (cookie_base) {
      if ( cookie_base < limit) {
        char* a = cookie_base;
        while( (*a) && (*a!='\t') && (*a!='\n')) a++;
        buffer[0]='\0';
        strncat(buffer,cookie_base,(int) (a - cookie_base));
        return buffer;
      } else
        return "";
    } else
      return "";
  } else
    return "";
}
// fin cookies



// -- basic auth --

/* déclarer un répertoire comme possédant une authentification propre */
int bauth_add(t_cookie* cookie,char* adr,char* fil,char* auth) {
  if (cookie) {
    if (!bauth_check(cookie,adr,fil)) {       // n'existe pas déja
      bauth_chain* chain=&cookie->auth;
      char* prefix=bauth_prefix(adr,fil);
      /* fin de la chaine */
      while(chain->next)
        chain=chain->next;
      chain->next=(bauth_chain*) calloc(sizeof(bauth_chain),1);
      if (chain->next) {
        chain=chain->next;
        chain->next=NULL;
        strcpy(chain->auth,auth);
        strcpy(chain->prefix,prefix);
        return 1;
      }
    }
  }
  return 0;
}

/* tester adr et fil, et retourner authentification si nécessaire */
/* sinon, retourne NULL */
char* bauth_check(t_cookie* cookie,char* adr,char* fil) {
  if (cookie) {
    bauth_chain* chain=&cookie->auth;
    char* prefix=bauth_prefix(adr,fil);
    while(chain) {
      if (strnotempty(chain->prefix)) {
        if (strncmp(prefix,chain->prefix,strlen(chain->prefix))==0) {
          return chain->auth;
        }
      }
      chain=chain->next;
    }
  }
  return NULL;
}

char* bauth_prefix(char* adr,char* fil) {
  char* prefix;
  char* a;
  NOSTATIC_RESERVE(prefix, char, HTS_URLMAXSIZE*2);
  strcpy(prefix,jump_identification(adr));
  strcat(prefix,fil);
  a=strchr(prefix,'?');
  if (a) *a='\0';
  if (strchr(prefix,'/')) {
    a=prefix+strlen(prefix)-1;
    while(*a != '/') a--;
    *(a+1)='\0';
  }
  return prefix;
}
