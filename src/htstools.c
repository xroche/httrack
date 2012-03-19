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
/*       various tools (filename analyzing ..)                  */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htstools.h"

/* specific definitions */
#include "htsbase.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
/* END specific definitions */


// forme à partir d'un lien et du contexte (origin_fil et origin_adr d'où il est tiré) adr et fil
// [adr et fil sont des buffers de 1ko]
// 0 : ok
// -1 : erreur
// -2 : protocole non supporté (ftp)
int ident_url_relatif(char *lien,char* origin_adr,char* origin_fil,char* adr,char* fil) {
  int ok=0;
  int scheme=0;

  adr[0]='\0'; fil[0]='\0';    //effacer buffers

  // lien non vide!
  if (strnotempty(lien)==0) return -1;    // erreur!

  // Scheme?
  {
    char* a=lien;
    while (isalpha((unsigned char)*a))
      a++;
    if (*a == ':')
      scheme=1;
  }

  // filtrer les parazites (mailto & cie)
  // scheme+authority (//)
  if (
               (strfield(lien,"http://"))        // scheme+//
            || (strfield(lien,"file://"))   // scheme+//
            || (strncmp(lien,"//",2)==0)    // // sans scheme (-> default)
       ) {
    if (ident_url_absolute(lien,adr,fil)==-1) {        
      ok=-1;    // erreur URL
    }
  }
  else if (strfield(lien,"ftp://")) {
    // Note: ftp:foobar.gif is not valid
    if (ftp_available()) {     // ftp supporté
      if (ident_url_absolute(lien,adr,fil)==-1) {        
        ok=-1;    // erreur URL
      }
    } else {
      ok=-2;  // non supporté
    }
#if HTS_USEOPENSSL
  } else if (SSL_is_available && strfield(lien,"https://")) {
    // Note: ftp:foobar.gif is not valid
    if (ident_url_absolute(lien,adr,fil)==-1) {        
      ok=-1;    // erreur URL
    }
#endif
  } else if ((scheme) && (
    (!strfield(lien,"http:"))
    && (!strfield(lien,"https:"))
    && (!strfield(lien,"ftp:"))
    )) {
    ok=-1;      // unknown scheme
  } else {    // c'est un lien relatif
    char* a;
    
    // On forme l'URL complète à partie de l'url actuelle
    // et du chemin actuel si besoin est.
    
    // copier adresse
    if (((int) strlen(origin_adr)<HTS_URLMAXSIZE) && ((int) strlen(origin_fil)<HTS_URLMAXSIZE) && ((int) strlen(lien)<HTS_URLMAXSIZE)) {

      /* patch scheme if necessary */
      if (strfield(lien,"http:")) {
        lien+=5;
        strcpybuff(adr, jump_protocol(origin_adr));    // même adresse ; protocole vide (http)
      } else if (strfield(lien,"https:")) {
        lien+=6;
        strcpybuff(adr, "https://");   // même adresse forcée en https
        strcatbuff(adr, jump_protocol(origin_adr));
      } else if (strfield(lien,"ftp:")) {
        lien+=4;
        strcpybuff(adr, "ftp://");   // même adresse forcée en ftp
        strcatbuff(adr, jump_protocol(origin_adr));
      } else {
        strcpybuff(adr,origin_adr);    // même adresse ; et même éventuel protocole
      }
      
      if (*lien!='/') {  // sinon c'est un lien absolu
        if (*lien == '\0') {
          strcpybuff(fil,origin_fil);
        } else if (*lien == '?') {     // example: a href="?page=2"
          char* a;
          strcpybuff(fil,origin_fil);
          a=strchr(fil,'?');
          if (a) *a='\0';
          strcatbuff(fil,lien);
        } else {
          a=strchr(origin_fil,'?');
          if (a == NULL) a=origin_fil+strlen(origin_fil);
          while((*a!='/') && ( a > origin_fil) ) a--;
          if (*a=='/') {    // ok on a un '/'
            if ( (((int) (a - origin_fil))+1+strlen(lien)) < HTS_URLMAXSIZE) {
              // copier chemin
              strncpy(fil,origin_fil,((int) (a - origin_fil))+1);
              *(fil + ((int) (a - origin_fil))+1)='\0';
              
              // copier chemin relatif
              if (((int) strlen(fil)+(int) strlen(lien)) < HTS_URLMAXSIZE) {
                strcatbuff(fil,lien + ((*lien=='/')?1:0) );      
                // simplifier url pour les ../
                fil_simplifie(fil);
              } else
                ok=-1;    // erreur
            } else {    // erreur
              ok=-1;    // erreur URL
            }
          } else {    // erreur
            ok=-1;    // erreur URL
          }
        }
      } else { // chemin absolu
        // copier chemin directement
        strcatbuff(fil,lien);      
        fil_simplifie(fil);
      }  // *lien!='/'
    } else
      ok=-1;
    
  }  // test news: etc.

  // case insensitive pour adresse
  {
    char *a=jump_identification(adr);
    while(*a) {
      if ((*a>='A') && (*a<='Z'))
        *a+='a'-'A';       
      a++;
    }
  }
  
  return ok;
}





// créer dans s, à partir du chemin courant curr_fil, le lien vers link (absolu)
// un ident_url_relatif a déja été fait avant, pour que link ne soit pas un chemin relatif
int lienrelatif(char* s,char* link,char* curr_fil) {
  char _curr[HTS_URLMAXSIZE*2];
  char newcurr_fil[HTS_URLMAXSIZE*2],newlink[HTS_URLMAXSIZE*2];
  char* curr;
  //int n=0;
  char* a;
  int slash=0;
  //
  newcurr_fil[0]='\0'; newlink[0]='\0';
  //

  // patch: éliminer les ? (paramètres) sinon bug
  if ( (a=strchr(curr_fil,'?')) ) {
    strncatbuff(newcurr_fil,curr_fil,(int) (a - curr_fil));
    curr_fil = newcurr_fil;
  }
  if ( (a=strchr(link,'?')) ) {
    strncatbuff(newlink,link,(int) (a - link));
    link = newlink;
  }

  // recopier uniquement le chemin courant
  curr=_curr;
  strcpybuff(curr,curr_fil);
  if ((a=strchr(curr,'?'))==NULL)  // couper au ? (params)
    a=curr+strlen(curr)-1;         // pas de params: aller à la fin
  while((*a!='/') && ( a> curr)) a--;       // chercher dernier / du chemin courant
  if (*a=='/') *(a+1)='\0';                           // couper dernier /
  
  // "effacer" s
  s[0]='\0';
  
  // sauter ce qui est commun aux 2 chemins
  {
    char *l,*c;
    if (*link=='/') link++;  // sauter slash
    if (*curr=='/') curr++;
    l=link;
    c=curr;
    // couper ce qui est commun
#if HTS_CASSE
    while ((*link==*curr) && (*link!=0)) {link++; curr++; }
#else
    while ((streql(*link,*curr)) && (*link!=0)) {link++; curr++; }
#endif
    // mais on veut un répertoirer entier!
    // si on a /toto/.. et /toto2/.. on ne veut pas sauter /toto !
    while(((*link!='/') || (*curr!='/')) && ( link > l)) { link--; curr--; }
    //if (*link=='/') link++;
    //if (*curr=='/') curr++;
  }
  
  // calculer la profondeur du répertoire courant et remonter
  // LES ../ ONT ETE SIMPLIFIES
  a=curr;
  if (*a=='/') a++;
  while(*a) if (*(a++)=='/') strcatbuff(s,"../");
  //if (strlen(s)==0) strcatbuff(s,"/");

  if (slash) strcatbuff(s,"/");    // garder absolu!!
  
  // on est dans le répertoire de départ, copier
  strcatbuff(s,link + ((*link=='/')?1:0) );

  /* Security check */
  if (strlen(s) >= HTS_URLMAXSIZE)
    return -1;

  // on a maintenant une chaine de la forme ../../test/truc.html  
  return 0;
}

/* Is the link absolute (http://www..) or relative (/bar/foo.html) ? */
int link_has_authority(char* lien) {
  char* a=lien;
  if (isalpha((unsigned char)*a)) {
    // Skip scheme?
    while (isalpha((unsigned char)*a))
      a++;
    if (*a == ':')
      a++;
    else
      return 0;
  }
  if (strncmp(a,"//",2) == 0)
    return 1;
  return 0;
}

int link_has_authorization(char* lien) {
  char* adr = jump_protocol(lien);
  char* firstslash = strchr(adr, '/');
  char* detect = strchr(adr, '@');
  if (firstslash) {
    if (detect) {
      return (detect < firstslash);
    }
  } else {
    return (detect != NULL);
  }
  return 0;
}


// conversion chemin de fichier/dossier vers 8-3 ou ISO9660
void long_to_83(int mode,char* n83,char* save) {
  n83[0]='\0';

  while(*save) {
    char fn83[256],fnl[256];
    int i=0;
    fn83[0]=fnl[0]='\0';
    while((save[i]) && (save[i]!='/')) { fnl[i]=save[i]; i++; }
    fnl[i]='\0';
    // conversion
    longfile_to_83(mode,fn83,fnl);
    strcatbuff(n83,fn83);

    save+=i;
    if (*save=='/') { strcatbuff(n83,"/"); save++; }
  }
}


// conversion nom de fichier/dossier isolé vers 8-3 ou ISO9660
void longfile_to_83(int mode,char* n83,char* save) {
  int i=0,j=0,max=0;
  char nom[256];
  char ext[256];
  nom[0]=ext[0]='\0';
  
  switch(mode) {
  case 1:
    max=8;
    break;
  case 2:
    max=30;
    break;
  default:
    max=8;
    break;
  }

  /* No starting . */
  if (save[0] == '.') {
    save[0]='_';
  }
  /* No multiple dots */
  {
    char* last_dot=strrchr(save, '.');
    char* dot;
    while((dot=strchr(save, '.'))) {
      *dot = '_';
    }
    if (last_dot) {
      *last_dot='.';
    }
  }
  /* 
    Avoid: (ISO9660, but also suitable for 8-3)
    (Thanks to jonat@cellcast.com for te hint)
    /:;?\#*~
    0x00-0x1f and 0x80-0xff
  */
  for(i=0 ; i < (int) strlen(save) ; i++) {
    if (
      (strchr("/:;?\\#*~", save[i]))
      ||
      (save[i] < 32)
      ||
      (save[i] >= 127)
      ) {
      save[i]='_';
    }
  }

  i=j=0;
  while((i<max) && (save[j]) && (save[j]!='.')) {
    if (save[j]!=' ') {
      nom[i]=save[j]; 
      i++; 
    } 
    j++; 
  }  // recopier nom
  nom[i]='\0';
  if (save[j]) {  // il reste au moins un point
    i=strlen(save)-1;
    while((i>0) && (save[i]!='.') && (save[i]!='/')) i--;    // rechercher dernier .
    if (save[i]=='.') {  // point!
      int j=0;
      i++;
      while((j<3) && (save[i]) ) { if (save[i]!=' ') { ext[j]=save[i]; j++; } i++; }
      ext[j]='\0';
    }
  }
  // corriger vers 8-3
  n83[0]='\0';
  strncatbuff(n83,nom,8);
  if (strnotempty(ext)) {
    strcatbuff(n83,".");
    strncatbuff(n83,ext,3);    
  }
}

// écrire backblue.gif
int verif_backblue(httrackp* opt,char* base) {
  int* done;
  int ret=0;
  NOSTATIC_RESERVE(done, int, 1);
  //
  if (!base) {   // init
    *done=0;
    return 0;
  }
  if ( (!*done)
    || (fsize(fconcat(base,"backblue.gif")) != HTS_DATA_BACK_GIF_LEN)) {
    FILE* fp = filecreate(fconcat(base,"backblue.gif"));
    *done=1;
    if (fp) {
      if (fwrite(HTS_DATA_BACK_GIF,HTS_DATA_BACK_GIF_LEN,1,fp) != HTS_DATA_BACK_GIF_LEN)
        ret=1;
      fclose(fp);
      usercommand(opt,0,NULL,fconcat(base,"backblue.gif"),"","");
    } else
      ret=1;
    //
    fp = filecreate(fconcat(base,"fade.gif"));
    if (fp) {
      if (fwrite(HTS_DATA_FADE_GIF,HTS_DATA_FADE_GIF_LEN,1,fp) != HTS_DATA_FADE_GIF_LEN)
        ret=1;
      fclose(fp);
      usercommand(opt,0,NULL,fconcat(base,"fade.gif"),"","");
    } else
      ret=1;
  } 
  return ret;
}

// flag
int verif_external(int nb,int test) {
  int* status;
  NOSTATIC_RESERVE(status, int, 2);
  if (!test)
    status[nb]=0;   // reset
  else if (!status[nb]) {
    status[nb]=1;
    return 1;
  }
  return 0;
}


// recherche chaîne de type truc<espaces>=
// renvoi décalage à effectuer ou 0 si non trouvé
/* SECTION OPTIMISEE:
#define rech_tageq(adr,s) ( \
  ( (*(adr-1)=='<') || (is_space(*(adr-1))) ) ? \
    ( (streql(*adr,*s)) ? \
      (__rech_tageq(adr,s)) \
      : 0 \
    ) \
    : 0\
  )
*/
/*
HTS_INLINE int rech_tageq(const char* adr,const char* s) { 
  if ( (*(adr-1)=='<') || (is_space(*(adr-1))) ) {   // <tag < tag etc
    if (streql(*adr,*s)) {                           // tester premier octet (optimisation)
      return __rech_tageq(adr,s);
    }
  }
  return 0;
}
*/
// Deuxième partie
HTS_INLINE int __rech_tageq(const char* adr,const char* s) { 
  int p;
  p=strfield(adr,s);
  if (p) {
    while(is_space(adr[p])) p++;
    if (adr[p]=='=') {
      return p+1;
    }
  }
  return 0;
}
// same, but check begining of adr wirh s (for <object src="bar.mov" .. hotspot123="foo.html">)
HTS_INLINE int __rech_tageqbegdigits(const char* adr,const char* s) { 
  int p;
  p=strfield(adr,s);
  if (p) {
    while(isdigit((unsigned char)adr[p]))  p++;      // jump digits
    while(is_space(adr[p])) p++;
    if (adr[p]=='=') {
      return p+1;
    }
  }
  return 0;
}

// tag sans =
HTS_INLINE int rech_sampletag(const char* adr,const char* s) { 
  int p;
  if ( (*(adr-1)=='<') || (is_space(*(adr-1))) ) {   // <tag < tag etc
    p=strfield(adr,s);
    if (p) {
      if (!isalnum((unsigned char)adr[p])) {  // <srcbis n'est pas <src
        return 1;
      }
      return 0;
    }
  }
  return 0;
}

// teste si le tag contenu dans from est égal à "tag"
HTS_INLINE int check_tag(char* from,const char* tag) {
  char* a=from+1;
  int i=0;
  char s[256];
  while(is_space(*a)) a++;
  while((isalnum((unsigned char)*a) || (*a=='/')) && (i<250)) { s[i++]=*a; a++; }
  s[i++]='\0';
  return (strfield2(s,tag));  // comparer
}

// teste si un fichier dépasse le quota
int istoobig(LLint size,LLint maxhtml,LLint maxnhtml,char* type) {
  int ok=1;
  if (size>0) {
    if (is_hypertext_mime(type)) {
      if (maxhtml>0) {
        if (size>maxhtml)
          ok=0;
      }
    } else {
      if (maxnhtml>0) {
        if (size>maxnhtml)
          ok=0;
      }
    }
  }
  return (!ok);
}


HTSEXT_API int hts_buildtopindex(httrackp* opt,char* path,char* binpath) {
  FILE* fpo;
  int retval=0;
  char rpath[1024*2];
  char *toptemplate_header=NULL,*toptemplate_body=NULL,*toptemplate_footer=NULL;
  
  // et templates html
  toptemplate_header=readfile_or(fconcat(binpath,"templates/topindex-header.html"),HTS_INDEX_HEADER);
  toptemplate_body=readfile_or(fconcat(binpath,"templates/topindex-body.html"),HTS_INDEX_BODY);
  toptemplate_footer=readfile_or(fconcat(binpath,"templates/topindex-footer.html"),HTS_INDEX_FOOTER);
  
  if (toptemplate_header && toptemplate_body && toptemplate_footer) {
    
    strcpybuff(rpath,path);
    if (rpath[0]) {
      if (rpath[strlen(rpath)-1]=='/')
        rpath[strlen(rpath)-1]='\0';
    }
    
    fpo=fopen(fconcat(rpath,"/index.html"),"wb");
    if (fpo) {
      find_handle h;
      verif_backblue(opt,concat(rpath,"/"));    // générer gif
      // Header
      fprintf(fpo,toptemplate_header,
        "<!-- Mirror and index made by HTTrack Website Copier/"HTTRACK_VERSION" "HTTRACK_AFF_AUTHORS" -->"
        );
      
      /* Find valid project names */
      h = hts_findfirst(rpath);
      if (h) {
        struct topindex_chain * chain=NULL;
        struct topindex_chain * startchain=NULL;
        do {
          if (hts_findisdir(h)) {
            char iname[HTS_URLMAXSIZE*2];
            strcpybuff(iname,rpath);
            strcatbuff(iname,"/");
            strcatbuff(iname,hts_findgetname(h));
            strcatbuff(iname,"/index.html");
            if (fexist(iname)) {
              struct topindex_chain * oldchain=chain;
              chain=calloc(sizeof(struct topindex_chain), 1);
              if (!startchain) {
                startchain=chain;
              }
              if (chain) {
                if (oldchain) {
                  oldchain->next=chain;
                }
                chain->next=NULL;
                strcpybuff(chain->name, hts_findgetname(h));
              }
            }
            
          }
        } while(hts_findnext(h));
        hts_findclose(h);

        /* Build sorted index */
        chain=startchain;
        while(chain) {
          char hname[HTS_URLMAXSIZE*2];
          strcpybuff(hname,chain->name);
          escape_check_url(hname);
          fprintf(fpo,toptemplate_body,
            hname,
            chain->name
            );

          chain=chain->next;
        }


        retval=1;
      }
      
      // Footer
      fprintf(fpo,toptemplate_footer,
        "<!-- Mirror and index made by HTTrack Website Copier/"HTTRACK_VERSION" "HTTRACK_AFF_AUTHORS" -->"
        );
      
      fclose(fpo);
      
    }
    
  }

  if (toptemplate_header)
    freet(toptemplate_header);
  if (toptemplate_body)
    freet(toptemplate_body);
  if (toptemplate_footer)
    freet(toptemplate_footer);
  
  return retval;
}




// Portable directory find functions
/*
// Example:
find_handle h = hts_findfirst("/tmp");
if (h) {
  do {
    if (hts_findisfile(h))
      printf("File: %s (%d octets)\n",hts_findgetname(h),hts_findgetsize(h));
    else if (hts_findisdir(h))
      printf("Dir: %s\n",hts_findgetname(h));
  } while(hts_findnext(h));
  hts_findclose(h);
}
*/
HTSEXT_API find_handle hts_findfirst(char* path) {
  if (path) {
    if (strnotempty(path)) {
      find_handle_struct* find = (find_handle_struct*) calloc(1,sizeof(find_handle_struct));
      if (find) {
        memset(find, 0, sizeof(find_handle_struct));
#if HTS_WIN
        {
          char rpath[1024*2];
          strcpybuff(rpath,path);
          if (rpath[0]) {
            if (rpath[strlen(rpath)-1]!='\\')
              strcatbuff(rpath,"\\");
          }
          strcatbuff(rpath,"*.*");
          find->handle = FindFirstFile(rpath,&find->hdata);
          if (find->handle != INVALID_HANDLE_VALUE)
            return find;
        }
#else
        strcpybuff(find->path,path);
        {
          if (find->path[0]) {
            if (find->path[strlen(find->path)-1]!='/')
              strcatbuff(find->path,"/");
          }
        }
        find->hdir=opendir(path);
        if (find->hdir != NULL) {
          if (hts_findnext(find) == 1)
            return find;
        }
#endif
        free((void*)find);
      }
    }
  }
  return NULL;   
}

HTSEXT_API int hts_findnext(find_handle find) {
  if (find) {
#if HTS_WIN
    if ( (FindNextFile(find->handle,&find->hdata)))
      return 1;
#else
    memset(&(find->filestat), 0, sizeof(find->filestat));
    if ((find->dirp=readdir(find->hdir)))
      if (find->dirp->d_name)
        if (!stat(concat(find->path,find->dirp->d_name),&find->filestat))
          return 1;
#endif
  }
  return 0;
}

HTSEXT_API int hts_findclose(find_handle find) {
  if (find) {
#if HTS_WIN
    if (find->handle) {
      FindClose(find->handle);
      find->handle=NULL;
    }
#else
    if (find->hdir) {
      closedir (find->hdir);
      find->hdir=NULL;
    }
#endif
    free((void*)find);
  }
  return 0;
}

HTSEXT_API char* hts_findgetname(find_handle find) {
  if (find) {
#if HTS_WIN
    return find->hdata.cFileName;
#else
    if (find->dirp)
      return find->dirp->d_name;
#endif
  }
  return NULL;
}

HTSEXT_API int hts_findgetsize(find_handle find) {
  if (find) {
#if HTS_WIN
    return find->hdata.nFileSizeLow;
#else
    return find->filestat.st_size;
#endif
  }
  return -1;
}

HTSEXT_API int hts_findisdir(find_handle find) {
  if (find) {
    if (!hts_findissystem(find)) {
#if HTS_WIN
      if (find->hdata.dwFileAttributes  & FILE_ATTRIBUTE_DIRECTORY)
        return 1;
#else
      if (S_ISDIR(find->filestat.st_mode))
        return 1;
#endif
    }
  }
  return 0;
}
HTSEXT_API int hts_findisfile(find_handle find) {
  if (find) {
    if (!hts_findissystem(find)) {
#if HTS_WIN
      if (!(find->hdata.dwFileAttributes  & FILE_ATTRIBUTE_DIRECTORY))
        return 1;
#else
      if (S_ISREG(find->filestat.st_mode))
        return 1;
#endif
    }
  }
  return 0;
}
HTSEXT_API int hts_findissystem(find_handle find) {
  if (find) {
#if HTS_WIN
    if (find->hdata.dwFileAttributes  & (FILE_ATTRIBUTE_SYSTEM|FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_TEMPORARY))
      return 1;
    else if ( (!strcmp(find->hdata.cFileName,"..")) || (!strcmp(find->hdata.cFileName,".")) )
      return 1;
#else
    if (
      (S_ISCHR(find->filestat.st_mode))
      || 
      (S_ISBLK(find->filestat.st_mode))
      || 
      (S_ISFIFO(find->filestat.st_mode))
      || 
      (S_ISSOCK(find->filestat.st_mode))
      )
      return 1;
    else if ( (!strcmp(find->dirp->d_name,"..")) || (!strcmp(find->dirp->d_name,".")) )
      return 1;
#endif
  }
  return 0;
}
