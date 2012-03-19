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
/*       cache system (index and stores files in cache)         */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htscache.h"

/* specific definitions */
#include "htsbase.h"
#include "htsbasenet.h"
#include "htsmd5.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "htsnostatic.h"
/* END specific definitions */

#undef test_flush
#define test_flush if (opt->flush) { fflush(opt->log); fflush(opt->errlog); }

// routines de mise en cache

/*
  VERSION 1.0 :
  -----------

.ndx file
 file with data
   <string>(date/time) [ <string>(hostname+filename) (datfile_position_ascii) ] * number_of_links
 file without data
   <string>(date/time) [ <string>(hostname+filename) (-datfile_position_ascii) ] * number_of_links

.dat file
 [ file ] * 
with
  file= (with data)
   [ bytes ] * sizeof(htsblk header) [ bytes ] * n(length of file given in htsblk header)
 file= (without data)
   [ bytes ] * sizeof(htsblk header)
with
 <string>(name) = <length in ascii>+<lf>+<data>


  VERSION 1.1/1.2 :
  ---------------

.ndx file
 file with data
   <string>("CACHE-1.1") <string>(date/time) [ <string>(hostname+filename) (datfile_position_ascii) ] * number_of_links
 file without data
   <string>("CACHE-1.1") <string>(date/time) [ <string>(hostname+filename) (-datfile_position_ascii) ] * number_of_links

.dat file
   <string>("CACHE-1.1") [ [Header_1.1] [bytes] * n(length of file given in header) ] *
with
 Header_1.1=
   <int>(statuscode)
   <int>(size)
   <string>(msg)
   <string>(contenttype)
   <string>(charset) [version 3]
   <string>(last-modified)
   <string>(Etag)
   <string>location
   <string>Content-disposition [version 2]
   <string>hostname [version 4]
   <string>URI filename [version 4]
   <string>local filename [version 4]
   [<string>"SD" <string>(supplemental data)]
   [<string>"SD" <string>(supplemental data)]
   ...
   <string>"HTS" (end of header)
   <int>(number of bytes of data) (0 if no data written)
*/

// Nouveau: si != text/html ne stocke que la taille


void cache_mayadd(httrackp* opt,cache_back* cache,htsblk* r,char* url_adr,char* url_fil,char* url_save) {
  if ((opt->debug>0) && (opt->log!=NULL)) {
    fspc(opt->log,"debug"); fprintf(opt->log,"File checked by cache: %s"LF,url_adr);
  }            
  // ---stockage en cache---
  // stocker dans le cache?
  if (opt->cache) {
    if (cache->dat!=NULL) {
      // c'est le seul endroit ou l'on ajoute des elements dans le cache (fichier entier ou header)
      // on stocke tout fichier "ok", mais également les réponses 404,301,302...
      if ((r->statuscode==200)         /* stocker réponse standard, plus */
        || (r->statuscode==204)     /* no content */
        || (r->statuscode==301)     /* moved perm */
        || (r->statuscode==302)     /* moved temp */
        || (r->statuscode==303)     /* moved temp */
        || (r->statuscode==307)     /* moved temp */
        || (r->statuscode==401)     /* authorization */
        || (r->statuscode==403)     /* unauthorized */
        || (r->statuscode==404)     /* not found */
        || (r->statuscode==410)     /* gone */
        )
      {        /* ne pas stocker si la page générée est une erreur */
        if (!r->is_file) {
          // stocker fichiers (et robots.txt)
          if ( (strnotempty(url_save)) || (strcmp(url_fil,"/robots.txt")==0)) {
            // ajouter le fichier au cache
            cache_add(*r,url_adr,url_fil,url_save,cache->ndx,cache->dat,opt->all_in_cache);
          }
        }
      }
    }
  }
  // ---fin stockage en cache---
}


/* Ajout d'un fichier en cache */
void cache_add(htsblk r,char* url_adr,char* url_fil,char* url_save,FILE* cache_ndx,FILE* cache_dat,int all_in_cache) {
  int pos;
  char s[256];
  char buff[HTS_URLMAXSIZE*4];
  int ok=1;
  int dataincache=0;    // donnée en cache?
  /*char digest[32+2];*/
  /*digest[0]='\0';*/

  // Longueur url_save==0?
  if ( (strnotempty(url_save)==0) ) {
    if (strcmp(url_fil,"/robots.txt")==0)        // robots.txt
      dataincache=1;
    else
      return;   // erreur (sauf robots.txt)
  }

  if (r.size <= 0)   // taille <= 0 
    return;          // refusé..

  // Mettre les *donées* en cache ?
  if (is_hypertext_mime(r.contenttype))    // html, mise en cache des données et 
    dataincache=1;                               // pas uniquement de l'en tête
  else if (all_in_cache)
    dataincache=1;                               // forcer tout en cache

  /* calcul md5 ? */
  /*
  if (is_hypertext_mime(r.contenttype)) {    // html, calcul MD5
    if (r.adr) {
      domd5mem(r.adr,r.size,digest,1);
    }
  }*/

  // Position
  fflush(cache_dat); fflush(cache_ndx);
  pos=ftell(cache_dat);
  // écrire pointeur seek, adresse, fichier
  if (dataincache)   // patcher
    sprintf(s,"%d\n",pos);    // ecrire tel que (eh oui évite les \0..)
  else
    sprintf(s,"%d\n",-pos);   // ecrire tel que (eh oui évite les \0..)

  // data
  // écrire données en-tête, données fichier
  /*if (!dataincache) {   // patcher
    r.size=-r.size;  // négatif
  }*/

  // Construction header
  ok=0;
  if (cache_wint(cache_dat,r.statuscode) != -1       // statuscode
  && cache_wLLint(cache_dat,r.size) != -1           // size
  && cache_wstr(cache_dat,r.msg) != -1              // msg
  && cache_wstr(cache_dat,r.contenttype) != -1      // contenttype
  && cache_wstr(cache_dat,r.charset) != -1          // contenttype
  && cache_wstr(cache_dat,r.lastmodified) != -1     // last-modified
  && cache_wstr(cache_dat,r.etag) != -1             // Etag
  && cache_wstr(cache_dat,(r.location!=NULL)?r.location:"") != -1         // 'location' pour moved
  && cache_wstr(cache_dat,r.cdispo) != -1           // Content-disposition
  && cache_wstr(cache_dat,url_adr) != -1            // Original address
  && cache_wstr(cache_dat,url_fil) != -1            // Original URI filename
  && cache_wstr(cache_dat,url_save) != -1           // Original save filename
  && cache_wstr(cache_dat,"HTS") != -1              // end of header
  ) {
    ok=1;       /* ok */
  }
  // Fin construction header

  /*if ((int) fwrite((char*) &r,1,sizeof(htsblk),cache_dat) == sizeof(htsblk)) {*/
  if (ok) {
    if (dataincache) {    // mise en cache?
      if (!r.adr) {       /* taille nulle (parfois en cas de 301 */
        if (cache_wLLint(cache_dat,0)==-1)          /* 0 bytes */
          ok=0;
      } else if (r.is_write==0) {  // en mémoire, recopie directe
        if (cache_wLLint(cache_dat,r.size)!=-1) {
          if (r.size>0) {   // taille>0
            if (fwrite(r.adr,1,(INTsys)r.size,cache_dat)!=r.size)
              ok=0;
          } else    // taille=0, ne rien écrire
            ok=0;
        } else
          ok=0;
      } else {  // recopier fichier dans cache
        FILE* fp;
        // On recopie le fichier..
        LLint file_size=fsize(fconv(url_save));
        if (file_size>=0) {
          if (cache_wLLint(cache_dat,file_size)!=-1) {
            fp=fopen(fconv(url_save),"rb");
            if (fp!=NULL) {
              char buff[32768];
              INTsys nl;
              do {
                nl=fread(buff,1,32768,fp);
                if (nl>0) { 
                  if ((INTsys)fwrite(buff,1,(INTsys)nl,cache_dat)!=nl) {  // erreur
                    nl=-1;
                    ok=0;
                  }
                }
              } while(nl>0);
              fclose(fp);
            } else ok=0;
          } else ok=0;
        } else ok=0;
      }
    } else {
      if (cache_wLLint(cache_dat,0)==-1)          /* 0 bytes */
        ok=0;
    }
  } else ok=0;
  /*if (!dataincache) {   // dépatcher
    r.size=-r.size;
  }*/

  // index
  // adresse+cr+fichier+cr
  if (ok) {
    buff[0]='\0'; strcatbuff(buff,url_adr); strcatbuff(buff,"\n"); strcatbuff(buff,url_fil); strcatbuff(buff,"\n");
    cache_wstr(cache_ndx,buff);
    fwrite(s,1,strlen(s),cache_ndx);
  }  // si ok=0 on a peut être écrit des données pour rien mais on s'en tape
  
  // en cas de plantage, on aura au moins le cache!
  fflush(cache_dat); fflush(cache_ndx);
}


htsblk cache_read(httrackp* opt,cache_back* cache,char* adr,char* fil,char* save,char* location) {
  return cache_readex(opt,cache,adr,fil,save,location,NULL,0);
}

htsblk cache_read_ro(httrackp* opt,cache_back* cache,char* adr,char* fil,char* save,char* location) {
  return cache_readex(opt,cache,adr,fil,save,location,NULL,1);
}

// lecture d'un fichier dans le cache
// si save==null alors test unqiquement
htsblk cache_readex(httrackp* opt,cache_back* cache,char* adr,char* fil,char* save,char* location,
                    char* return_save, int readonly) {
#if HTS_FAST_CACHE
  long int hash_pos;
  int hash_pos_return;
#else
  char* a;
#endif
  char buff[HTS_URLMAXSIZE*2];
  char location_default[HTS_URLMAXSIZE*2];
  char previous_save[HTS_URLMAXSIZE*2];
  htsblk r;
  int ok=0;
  int header_only=0;

  memset(&r, 0, sizeof(htsblk)); r.soc=INVALID_SOCKET;
  if (location) {
    r.location = location;
  } else {
    r.location = location_default;
  }
  strcpybuff(r.location, ""); 
#if HTS_FAST_CACHE
  strcpybuff(buff,adr); strcatbuff(buff,fil);
  hash_pos_return=inthash_read((inthash)cache->hashtable,buff,(long int*)&hash_pos);
#else
  buff[0]='\0'; strcatbuff(buff,"\n"); strcatbuff(buff,adr); strcatbuff(buff,"\n"); strcatbuff(buff,fil); strcatbuff(buff,"\n");
  if (cache->use)
    a=strstr(cache->use,buff);
  else
    a=NULL;       // forcer erreur
#endif

  /* avoid errors on data entries */
  if (adr[0] == '/' && adr[1] == '/' && adr[2] == '[') {
#if HTS_FAST_CACHE
    hash_pos_return = 0;
#else
    a = NULL;
#endif
  }

  // en cas de succès
#if HTS_FAST_CACHE
  if (hash_pos_return) {
#else
  if (a!=NULL) {  // OK existe en cache!
#endif
    INTsys pos;
#if DEBUGCA
    fprintf(stdout,"..cache: %s%s at ",adr,fil);
#endif
    
#if HTS_FAST_CACHE
    pos=hash_pos;     /* simply */
#else
    a+=strlen(buff);
    sscanf(a,"%d",&pos);    // lire position
#endif
#if DEBUGCA
    printf("%d\n",pos);
#endif

    fflush(cache->olddat); 
    if (fseek(cache->olddat,((pos>0)?pos:(-pos)),SEEK_SET) == 0) {
      /* Importer cache1.0 */
      if (cache->version==0) {
        OLD_htsblk old_r;
        if (fread((char*) &old_r,1,sizeof(old_r),cache->olddat)==sizeof(old_r)) { // lire tout (y compris statuscode etc)
          r.statuscode=old_r.statuscode;
          r.size=old_r.size;        // taille fichier
          strcpybuff(r.msg,old_r.msg);
          strcpybuff(r.contenttype,old_r.contenttype);
          ok=1;     /* import  ok */
        }
      /* */
      /* Cache 1.1 */
      } else {
        char check[256];
        LLint size_read;
        check[0]='\0';
        //
        cache_rint(cache->olddat,&r.statuscode);
        cache_rLLint(cache->olddat,&r.size);
        cache_rstr(cache->olddat,r.msg);
        cache_rstr(cache->olddat,r.contenttype);
        if (cache->version >= 3)
          cache_rstr(cache->olddat,r.charset);
        cache_rstr(cache->olddat,r.lastmodified);
        cache_rstr(cache->olddat,r.etag);
        cache_rstr(cache->olddat,r.location);
        if (cache->version >= 2)
          cache_rstr(cache->olddat,r.cdispo);
        if (cache->version >= 4) {
          cache_rstr(cache->olddat, previous_save);  // adr
          cache_rstr(cache->olddat, previous_save);  // fil
          previous_save[0] = '\0';
          cache_rstr(cache->olddat, previous_save);  // save
          if (return_save != NULL) {
            strcpybuff(return_save, previous_save);
          }
        }
        //
        cache_rstr(cache->olddat,check);
        if (strcmp(check,"HTS")==0) {           /* intégrité OK */
          ok=1;
        }
        cache_rLLint(cache->olddat,&size_read);       /* lire size pour être sûr de la taille déclarée (réécrire) */
        if (size_read>0) {                         /* si inscrite ici */
          r.size=size_read;
        } else {                              /* pas de données directement dans le cache, fichier présent? */
          if (r.statuscode!=200)
            header_only=1;          /* que l'en tête ici! */
        }
      }

      /* Remplir certains champs */
      r.totalsize=r.size;

      // lecture du header (y compris le statuscode)
      /*if (fread((char*) &r,1,sizeof(htsblk),cache->olddat)==sizeof(htsblk)) { // lire tout (y compris statuscode etc)*/
      if (ok) {
        // sécurité
        r.adr=NULL;
        r.out=NULL;
        ////r.location=NULL;  non, fixée lors des 301 ou 302
        r.fp=NULL;
        
        if ( (r.statuscode>=0) && (r.statuscode<=999)
          && (r.notmodified>=0)  && (r.notmodified<=9) ) {   // petite vérif intégrité
          if ((save) && (!header_only) ) {     /* ne pas lire uniquement header */
            //int to_file=0;
            
            r.adr=NULL; r.soc=INVALID_SOCKET; 
            // // r.location=NULL;
            
#if HTS_DIRECTDISK
            // Court-circuit:
            // Peut-on stocker le fichier directement sur disque?
            if (!readonly && r.statuscode==200 && !is_hypertext_mime(r.contenttype) && strnotempty(save)) {    // pas HTML, écrire sur disk directement
              int ok=0;
              
              r.is_write=1;    // écrire
              if (fexist(fconv(save))) {  // un fichier existe déja
                //if (fsize(fconv(save))==r.size) {  // même taille -- NON tant pis (taille mal declaree)
                ok=1;    // plus rien à faire
                filenote(save,NULL);        // noter comme connu
                //xxusercommand(opt,0,NULL,save,adr,fil);
                //}
              }
              
              if ((pos<0) && (!ok)) { // Pas de donnée en cache et fichier introuvable : erreur!
                if (opt->norecatch) {
                  filecreateempty(save);
                  //
                  r.statuscode=-1;
                  strcpybuff(r.msg,"File deleted by user not recaught");
                  ok=1;     // ne pas récupérer (et pas d'erreur)
                } else {
                  r.statuscode=-1;
                  strcpybuff(r.msg,"Previous cache file not found");
                  ok=1;    // ne pas récupérer
                }
              }
              
              if (!ok) {  
                r.out=filecreate(save);
#if HDEBUG
                printf("direct-disk: %s\n",save);
#endif
                if (r.out!=NULL) {
                  char buff[32768+4];
                  LLint size = r.size;
                  if (size > 0) {
                    INTsys nl;
                    do {
                      nl=fread(buff,1,(INTsys) minimum(size,32768),cache->olddat);
                      if (nl>0) {
                        size-=nl; 
                        if ((INTsys)fwrite(buff,1,(INTsys)nl,r.out)!=nl) {  // erreur
                          r.statuscode=-1;
                          strcpybuff(r.msg,"Cache Read Error : Read To Disk");
                        }
                      }
                    } while((nl>0) && (size>0) && (r.statuscode!=-1));
                  }
                  
                  fclose(r.out);
                  r.out=NULL;
#if HTS_WIN==0
                  chmod(save,HTS_ACCESS_FILE);      
#endif          
                  //xxusercommand(opt,0,NULL,fconv(save), adr, fil);
                } else {
                  r.statuscode=-1;
                  strcpybuff(r.msg,"Cache Write Error : Unable to Create File");
                  //printf("%s\n",save);
                }
              }
              
            } else
#endif
            { // lire en mémoire
              
              if (pos<0) {
                if (strnotempty(save)) { // Pas de donnée en cache, bizarre car html!!!
                  r.statuscode=-1;
                  strcpybuff(r.msg,"Previous cache file not found (2)");
                } else {                 /* Read in memory from cache */
                  if (strnotempty(return_save) && fexist(return_save)) {
                    FILE* fp = fopen(fconv(return_save), "rb");
                    if (fp != NULL) {
                      r.adr=(char*) malloct((INTsys)r.size + 4);
                      if (adr != NULL) {
                        if (r.size > 0 && fread(r.adr, 1, (INTsys) r.size, fp) != r.size) {
                          r.statuscode=-1;
                          strcpybuff(r.msg,"Read error in cache disk data");
                        }
                      } else {
                        r.statuscode=-1;
                        strcpybuff(r.msg,"Read error (memory exhausted) from cache");
                      }
                      fclose(fp);
                    }
                  } else {
                    r.statuscode=-1;
                    strcpybuff(r.msg,"Cache file not found on disk");
                  }
                }
              } else {
                // lire fichier (d'un coup)
                r.adr=(char*) malloct((INTsys)r.size+4);
                if (r.adr!=NULL) {
                  if (fread(r.adr,1,(INTsys)r.size,cache->olddat)!=r.size) {  // erreur
                    freet(r.adr);
                    r.adr=NULL;
                    r.statuscode=-1;
                    strcpybuff(r.msg,"Cache Read Error : Read Data");
                  } else
                    *(r.adr+r.size)='\0';
                  //printf(">%s status %d\n",back[p].r.contenttype,back[p].r.statuscode);
                } else {  // erreur
                  r.statuscode=-1;
                  strcpybuff(r.msg,"Cache Memory Error");
                }
              }
            }
          }    // si save==null, ne rien charger (juste en tête)
        } else {
#if DEBUGCA
          printf("Cache Read Error : Bad Data");
#endif
          r.statuscode=-1;
          strcpybuff(r.msg,"Cache Read Error : Bad Data");
        }
      } else {  // erreur
#if DEBUGCA
        printf("Cache Read Error : Read Header");
#endif
        r.statuscode=-1;
        strcpybuff(r.msg,"Cache Read Error : Read Header");
      }
    } else {
#if DEBUGCA
      printf("Cache Read Error : Seek Failed");
#endif
      r.statuscode=-1;
      strcpybuff(r.msg,"Cache Read Error : Seek Failed");
    }
  } else {
#if DEBUGCA
    printf("File Cache Not Found");
#endif
    r.statuscode=-1;
    strcpybuff(r.msg,"File Cache Entry Not Found");
  }
  if (!location) {   /* don't export internal buffer */
    r.location = NULL;
  }
  return r;
}

/* write (string1-string2)-data in cache */
/* 0 if failed */
int cache_writedata(FILE* cache_ndx,FILE* cache_dat,char* str1,char* str2,char* outbuff,int len) {
  if (cache_dat) {
    char buff[HTS_URLMAXSIZE*4];
    char s[256];
    int pos;
    fflush(cache_dat); fflush(cache_ndx);
    pos=ftell(cache_dat);
    /* first write data */
    if (cache_wint(cache_dat,len)!=-1) {       // length
      if ((INTsys)fwrite(outbuff,1,(INTsys)len,cache_dat) == (INTsys) len) {   // data
        /* then write index */
        sprintf(s,"%d\n",pos);
        buff[0]='\0'; strcatbuff(buff,str1); strcatbuff(buff,"\n"); strcatbuff(buff,str2); strcatbuff(buff,"\n");
        cache_wstr(cache_ndx,buff);
        if (fwrite(s,1,(INTsys)strlen(s),cache_ndx) == strlen(s)) {
          fflush(cache_dat); fflush(cache_ndx);
          return 1;
        }
      }
    }
  }
  return 0;
}

/* read the data corresponding to (string1-string2) in cache */
/* 0 if failed */
int cache_readdata(cache_back* cache,char* str1,char* str2,char** inbuff,int* inlen) {
#if HTS_FAST_CACHE
  if (cache->hashtable) {
    char buff[HTS_URLMAXSIZE*4];
    long int pos;
    strcpybuff(buff,str1); strcatbuff(buff,str2);
    if (inthash_read((inthash)cache->hashtable,buff,(long int*)&pos)) {
      if (fseek(cache->olddat,((pos>0)?pos:(-pos)),SEEK_SET) == 0) {
        INTsys len;
        cache_rint(cache->olddat,&len);
        if (len>0) {
          char* mem_buff=(char*)malloct(len+4);    /* Plus byte 0 */
          if (mem_buff) {
            if ((INTsys)fread(mem_buff,1,len,cache->olddat)==len) { // lire tout (y compris statuscode etc)*/
              *inbuff=mem_buff;
              *inlen=len;
              return 1;
            } else
              freet(mem_buff);
          }
        }
      }
    }
  }
#endif
  *inbuff=NULL;
  *inlen=0;
  return 0;
}

// renvoyer uniquement en tête, ou NULL si erreur
// return NULL upon error, and set -1 to r.statuscode
htsblk* cache_header(httrackp* opt,cache_back* cache,char* adr,char* fil,htsblk* r) {
  *r=cache_read(opt,cache,adr,fil,NULL,NULL);              // test uniquement
  if (r->statuscode != -1)
    return r;
  else
    return NULL;
}


// Initialisation du cache: créer nouveau, renomer ancien, charger..
void cache_init(cache_back* cache,httrackp* opt) {
  // ---
  // utilisation du cache: renommer ancien éventuel et charger index
  if (opt->cache) {
#if DEBUGCA
    printf("cache init: ");
#endif
    if (!cache->ro) {
#if HTS_WIN
      mkdir(fconcat(opt->path_log,"hts-cache"));
#else
      mkdir(fconcat(opt->path_log,"hts-cache"),HTS_PROTECT_FOLDER);
#endif
      if ((fexist(fconcat(opt->path_log,"hts-cache/new.dat"))) && (fexist(fconcat(opt->path_log,"hts-cache/new.ndx")))) {  // il existe déja un cache précédent.. renommer
#if DEBUGCA
        printf("work with former cache\n");
#endif
        if (fexist(fconcat(opt->path_log,"hts-cache/old.dat")))
          remove(fconcat(opt->path_log,"hts-cache/old.dat"));
        if (fexist(fconcat(opt->path_log,"hts-cache/old.ndx")))
          remove(fconcat(opt->path_log,"hts-cache/old.ndx"));
        
        rename(fconcat(opt->path_log,"hts-cache/new.dat"),fconcat(opt->path_log,"hts-cache/old.dat"));
        rename(fconcat(opt->path_log,"hts-cache/new.ndx"),fconcat(opt->path_log,"hts-cache/old.ndx"));
      } else {  // un des deux (ou les deux) fichiers cache absents: effacer l'autre éventuel
#if DEBUGCA
        printf("new cache\n");
#endif
        if (fexist(fconcat(opt->path_log,"hts-cache/new.dat")))
          remove(fconcat(opt->path_log,"hts-cache/new.dat"));
        if (fexist(fconcat(opt->path_log,"hts-cache/new.ndx")))
          remove(fconcat(opt->path_log,"hts-cache/new.ndx"));
      }
    }
    
    // charger index cache précédent
    if (
      (
      !cache->ro &&
      fsize(fconcat(opt->path_log,"hts-cache/old.dat")) >=0 && fsize(fconcat(opt->path_log,"hts-cache/old.ndx")) >0
      )
      ||
      (
      cache->ro &&
      fsize(fconcat(opt->path_log,"hts-cache/new.dat")) >=0 && fsize(fconcat(opt->path_log,"hts-cache/new.ndx")) > 0
      )
      ) {
      FILE* oldndx=NULL;
#if DEBUGCA
      printf("..load cache\n");
#endif
      if (!cache->ro) {
        cache->olddat=fopen(fconcat(opt->path_log,"hts-cache/old.dat"),"rb");        
        oldndx=fopen(fconcat(opt->path_log,"hts-cache/old.ndx"),"rb");        
      } else {
        cache->olddat=fopen(fconcat(opt->path_log,"hts-cache/new.dat"),"rb");        
        oldndx=fopen(fconcat(opt->path_log,"hts-cache/new.ndx"),"rb");        
      }
      // les deux doivent être ouvrables
      if ((cache->olddat==NULL) && (oldndx!=NULL)) {
        fclose(oldndx);
        oldndx=NULL;
      }
      if ((cache->olddat!=NULL) && (oldndx==NULL)) {
        fclose(cache->olddat);
        cache->olddat=NULL;
      }
      // lire index
      if (oldndx!=NULL) {
        int buffl;
        fclose(oldndx); oldndx=NULL;
        // lire ndx, et lastmodified
        if (!cache->ro) {
          buffl=fsize(fconcat(opt->path_log,"hts-cache/old.ndx"));
          cache->use=readfile(fconcat(opt->path_log,"hts-cache/old.ndx"));
        } else {
          buffl=fsize(fconcat(opt->path_log,"hts-cache/new.ndx"));
          cache->use=readfile(fconcat(opt->path_log,"hts-cache/new.ndx"));
        }
        if (cache->use!=NULL) {
          char firstline[256];
          char* a=cache->use;
          a+=cache_brstr(a,firstline);
          if (strncmp(firstline,"CACHE-",6)==0) {       // Nouvelle version du cache
            if (strncmp(firstline,"CACHE-1.",8)==0) {      // Version 1.1x
              cache->version=(int)(firstline[8]-'0');           // cache 1.x
              if (cache->version <= 4) {
                a+=cache_brstr(a,firstline);
                strcpybuff(cache->lastmodified,firstline);
              } else {
                if (opt->errlog) {
                  fspc(opt->errlog,"error"); fprintf(opt->errlog,"Cache: version 1.%d not supported, ignoring current cache"LF,cache->version);
                  fflush(opt->errlog);
                }
                fclose(cache->olddat);
                cache->olddat=NULL;
                freet(cache->use);
                cache->use=NULL;
              }
            } else {        // non supporté
              if (opt->errlog) {
                fspc(opt->errlog,"error"); fprintf(opt->errlog,"Cache: %s not supported, ignoring current cache"LF,firstline);
                fflush(opt->errlog);
              }
              fclose(cache->olddat);
              cache->olddat=NULL;
              freet(cache->use);
              cache->use=NULL;
            }
            /* */
          } else {              // Vieille version du cache
            /* */
            if (opt->log) {
              fspc(opt->log,"warning"); fprintf(opt->log,"Cache: importing old cache format"LF);
              fflush(opt->log);
            }
            cache->version=0;        // cache 1.0
            strcpybuff(cache->lastmodified,firstline); 
          }
          opt->is_update=1;        // signaler comme update
          
          /* Create hash table for the cache (MUCH FASTER!) */
#if HTS_FAST_CACHE
          if (cache->use) {
            char line[HTS_URLMAXSIZE*2];
            char linepos[256];
            int  pos;
            while ( (a!=NULL) && (a < (cache->use+buffl) ) ) {
              a=strchr(a+1,'\n');     /* start of line */
              if (a) {
                a++;
                /* read "host/file" */
                a+=binput(a,line,HTS_URLMAXSIZE);
                a+=binput(a,line+strlen(line),HTS_URLMAXSIZE);
                /* read position */
                a+=binput(a,linepos,200);
                sscanf(linepos,"%d",&pos);
                inthash_add((inthash)cache->hashtable,line,pos);
              }
            }
            /* Not needed anymore! */
            freet(cache->use);
            cache->use=NULL;
          }
#endif
        }
      }
      }  // taille cache>0
      
#if DEBUGCA
      printf("..create cache\n");
#endif
      if (!cache->ro) {
        // ouvrir caches actuels
        structcheck(fconcat(opt->path_log, "hts-cache/"));
        cache->dat=fopen(fconcat(opt->path_log,"hts-cache/new.dat"),"wb");        
        cache->ndx=fopen(fconcat(opt->path_log,"hts-cache/new.ndx"),"wb");        
        // les deux doivent être ouvrables
        if ((cache->dat==NULL) && (cache->ndx!=NULL)) {
          fclose(cache->ndx);
          cache->ndx=NULL;
        }
        if ((cache->dat!=NULL) && (cache->ndx==NULL)) {
          fclose(cache->dat);
          cache->dat=NULL;
        }
        
        if (cache->ndx!=NULL) {
          char s[256];
          
          cache_wstr(cache->dat,"CACHE-1.4");
          fflush(cache->dat);
          cache_wstr(cache->ndx,"CACHE-1.4");
          fflush(cache->ndx);
          //
          time_gmt_rfc822(s);   // date et heure actuelle GMT pour If-Modified-Since..
          cache_wstr(cache->ndx,s);        
          fflush(cache->ndx);    // un petit fflush au cas où
          
          // supprimer old.lst
          if (fexist(fconcat(opt->path_log,"hts-cache/old.lst")))
            remove(fconcat(opt->path_log,"hts-cache/old.lst"));
          // renommer
          if (fexist(fconcat(opt->path_log,"hts-cache/new.lst")))
            rename(fconcat(opt->path_log,"hts-cache/new.lst"),fconcat(opt->path_log,"hts-cache/old.lst"));
          // ouvrir
          cache->lst=fopen(fconcat(opt->path_log,"hts-cache/new.lst"),"wb");
          {
            filecreate_params tmp;
            strcpybuff(tmp.path,opt->path_html);    // chemin
            tmp.lst=cache->lst;                 // fichier lst
            filenote("",&tmp);        // initialiser filecreate
          }
          
          // supprimer old.txt
          if (fexist(fconcat(opt->path_log,"hts-cache/old.txt")))
            remove(fconcat(opt->path_log,"hts-cache/old.txt"));
          // renommer
          if (fexist(fconcat(opt->path_log,"hts-cache/new.txt")))
            rename(fconcat(opt->path_log,"hts-cache/new.txt"),fconcat(opt->path_log,"hts-cache/old.txt"));
          // ouvrir
          cache->txt=fopen(fconcat(opt->path_log,"hts-cache/new.txt"),"wb");
          if (cache->txt) {
            fprintf(cache->txt,"date\tsize'/'remotesize\tflags(request:Update,Range state:File response:Modified,Chunked,gZipped)\t");
            fprintf(cache->txt,"statuscode\tstatus ('servermsg')\tMIME\tEtag|Date\tURL\tlocalfile\t(from URL)"LF);
          }
          
          // test
          // cache_writedata(cache->ndx,cache->dat,"//[TEST]//","test1","TEST PIPO",9);
        }
        
      } else {
        cache->lst = cache->dat = cache->ndx = NULL;
      }
      
  }
  
}




// lire un fichier.. (compatible \0)
char* readfile(char* fil) {
  char* adr=NULL;
  INTsys len=0;
  len=fsize(fil);
  if (len >= 0) {  // exists
    FILE* fp;
    fp=fopen(fconv(fil),"rb");
    if (fp!=NULL) {  // n'existe pas (!)
      adr=(char*) malloct(len+1);
      if (adr!=NULL) {
        if (len > 0 && (INTsys)fread(adr,1,len,fp) != len) {    // fichier endommagé ?
          freet(adr);
          adr=NULL;
        } else
          *(adr+len)='\0';
      }
      fclose(fp);
    }
  }
  return adr;
}

char* readfile_or(char* fil,char* defaultdata) {
  char* realfile=fil;
  char* ret;
  if (!fexist(fil))
    realfile=fconcat(hts_rootdir(NULL),fil);
  ret=readfile(realfile);
  if (ret)
    return ret;
  else {
    char *adr=malloct(strlen(defaultdata)+2);
    if (adr) {
      strcpybuff(adr,defaultdata);
      return adr;
    }
  }
  return NULL;
}

// écriture/lecture d'une chaîne sur un fichier
// -1 : erreur, sinon 0
int cache_wstr(FILE* fp,char* s) {
  INTsys i;
  char buff[256+4];
  i=strlen(s);
  sprintf(buff,INTsysP "\n",i);
  if (fwrite(buff,1,(INTsys)strlen(buff),fp) != strlen(buff))
    return -1;
  if (i>0)
  if ((INTsys)fwrite(s,1,i,fp) != i)
    return -1;
  return 0;
}
void cache_rstr(FILE* fp,char* s) {
  INTsys i;
  char buff[256+4];
  linput(fp,buff,256);
  sscanf(buff,INTsysP,&i);
  if (i < 0 || i > 32768)    /* error, something nasty happened */
    i=0;
  if (i>0)
    fread(s,1,i,fp);
  *(s+i)='\0';
}
int cache_brstr(char* adr,char* s) {
  int i;
  int off;
  char buff[256+4];
  off=binput(adr,buff,256);
  adr+=off;
  sscanf(buff,"%d",&i);
  if (i>0)
    strncpy(s,adr,i);
  *(s+i)='\0';
  off+=i;
  return off;
}
int cache_quickbrstr(char* adr,char* s) {
  int i;
  int off;
  char buff[256+4];
  off=binput(adr,buff,256);
  adr+=off;
  sscanf(buff,"%d",&i);
  if (i>0)
    strncpy(s,adr,i);
  *(s+i)='\0';
  off+=i;
  return off;
}
/* idem, mais en int */
int cache_brint(char* adr,int* i) {
  char s[256];
  int r=cache_brstr(adr,s);
  if (r!=-1)
    sscanf(s,"%d",i);
  return r;
}
void cache_rint(FILE* fp,int* i) {
  char s[256];
  cache_rstr(fp,s);
  sscanf(s,"%d",i);
}
int cache_wint(FILE* fp,int i) {
  char s[256];
  sprintf(s,"%d",(int) i);
  return cache_wstr(fp,s);
}
void cache_rLLint(FILE* fp,LLint* i) {
  char s[256];
  cache_rstr(fp,s);
  sscanf(s,LLintP,i);
}
int cache_wLLint(FILE* fp,LLint i) {
  char s[256];
  sprintf(s,LLintP,(LLint) i);
  return cache_wstr(fp,s);
}
// -- cache --
