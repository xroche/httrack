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
/*       savename routine (compute output filename)             */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htsname.h"

/* specific definitions */
#include "htsbase.h"
#include "htstools.h"
#include "htsmd5.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
/* END specific definitions */

#undef test_flush
#define test_flush if (opt->flush) { fflush(opt->log); fflush(opt->errlog); }

#define ADD_STANDARD_PATH \
    {  /* ajout nom */\
      char buff[HTS_URLMAXSIZE*2];\
      buff[0]='\0';\
      strncat(buff,start_pos,(int) (nom_pos - start_pos));\
      url_savename_addstr(save,buff);\
    }

#define ADD_STANDARD_NAME(shortname) \
    {  /* ajout nom */\
      char buff[HTS_URLMAXSIZE*2];\
      standard_name(buff,dot_pos,nom_pos,fil_complete,(shortname));\
      url_savename_addstr(save,buff);\
    }


/* Avoid stupid DOS system folders/file such as 'nul' */
/* Based on linux/fs/umsdos/mangle.c */
static const char *hts_tbdev[] =
{
    "/prn", "/con", "/aux", "/nul",
    "/lpt1", "/lpt2", "/lpt3", "/lpt4",
    "/com1", "/com2", "/com3", "/com4",
    "/clock$",
    "/emmxxxx0", "/xmsxxxx0", "/setverxx",
    ""
};



// forme le nom du fichier à sauver (save) à partir de fil et adr
// système intelligent, qui renomme en cas de besoin (exemple: deux INDEX.HTML et index.html)
int url_savename(char* adr_complete,char* fil_complete,char* save,char* former_adr,char* former_fil,char* referer_adr,char* referer_fil,httrackp* opt,lien_url** liens,int lien_tot,lien_back* back,int back_max,cache_back* cache,hash_struct* hash,int ptr,int numero_passe) {
  char newfil[HTS_URLMAXSIZE*2];   /* ="" */
  char* fil;
  char* adr;
  char* print_adr;
  char *start_pos=NULL,*nom_pos=NULL,*dot_pos=NULL;  // Position nom et point
  // pour changement d'extension ou de nom (content-disposition)
  int ext_chg=0;
  char ext[256];
  int max_char=0;
  //CLEAR
  newfil[0]=ext[0]='\0';

  /* 8-3 ? */
  switch(opt->savename_83) {
  case 1:
    max_char=8;
    break;
  case 2:
    max_char=30;
    break;
  default:
    max_char=8;
    break;
  }

  // effacer save
  save[0]='\0';
  // fil
  fil = fil_complete;
  // et adr (sauter user/pass)
  // on prend le parti de mettre les fichiers avec login/pass au même endroit que si ils
  // étaient capturés sans ces paramètres
  // c'est pour cette raison qu'on ignore totalement adr_complete (même pour la recherche en table de hachage)
  adr=jump_identification(adr_complete);

  // à afficher sans ftp://
  print_adr=jump_protocol(adr);

  // court-circuit pour lien primaire
  if (strnotempty(adr)==0) {
    if (strcmp(fil,"primary")==0) {
      strcat(save,"primary.html");
      return 0;
    }
  }


  // vérifier que le nom n'a pas déja été calculé (si oui le renvoyer tel que)
  // vérifier que le nom n'est pas déja pris...
  // NOTE: si on cherche /toto/ et que /toto est trouvé on le prend (et réciproquqment) ** // **
  if (liens!=NULL) { 
    int i;

#if HTS_HASH
    i=hash_read(hash,adr,fil_complete,1);      // recherche table 1 (adr+fil)
    if (i>=0) {    // ok, trouvé
      strcpy(save,liens[i]->sav);
      return 0;
    }
    i=hash_read(hash,adr,fil_complete,2);      // recherche table 2 (former_adr+former_fil)
    if (i>=0) {    // ok, trouvé
      // copier location moved!
      strcpy(adr_complete,liens[i]->adr);
      strcpy(fil_complete,liens[i]->fil);
      // et save
      strcpy(save,liens[i]->sav);  // copier (formé à partir du nouveau lien!)
      return 0;
    }
#else
    for(i=lien_tot-1;i>=0;i--) {        
#if HTS_CASSE
      if ((strcmp(liens[i]->adr,adr)==0) && (strcmp(liens[i]->fil,fil_complete)==0))
#else
      if ((strfield2(liens[i]->adr,adr)) && (strfield2(liens[i]->fil,fil_complete)))
#endif
      {    // ok c'est le même lien, adresse déja définie
        strcpy(save,liens[i]->sav);
        return 0;
      }
      if (liens[i]->former_adr) {     // tester ancienne loc?
#if HTS_CASSE
        if ((strcmp(liens[i]->former_adr,adr)==0) && (strcmp(liens[i]->former_fil,fil_complete)==0))
#else
        if ((strfield2(liens[i]->former_adr,adr)) && (strfield2(liens[i]->former_fil,fil_complete)))
#endif
        {
          // copier location moved!
          strcpy(adr_complete,liens[i]->adr);
          strcpy(fil_complete,liens[i]->fil);
          // et save
          strcpy(save,liens[i]->sav);  // copier (formé à partir du nouveau lien!)
          return 0;
        }
      }
    }
#endif

    // chercher sans / ou avec / dans former
    {
      char fil_complete_patche[HTS_URLMAXSIZE*2];
      strcpy(fil_complete_patche,fil_complete);
      // Version avec ou sans /
      if (fil_complete_patche[strlen(fil_complete_patche)-1]=='/')
        fil_complete_patche[strlen(fil_complete_patche)-1]='\0';
      else
        strcat(fil_complete_patche,"/");
#if HTS_HASH
      i=hash_read(hash,adr,fil_complete_patche,2);      // recherche table 2 (former_adr+former_fil)
      if (i>=0) {
        // écraser fil et adr (pas former_fil?????)
        strcpy(adr_complete,liens[i]->adr);
        strcpy(fil_complete,liens[i]->fil);
        // écrire save
        strcpy(save,liens[i]->sav);
        return 0;
      }
#else
      // même boucle en gros
      for(i=lien_tot-1;i>=0;i--) {        
        if (liens[i]->former_adr) {    // former-adr?
#if HTS_CASSE
          if ((strcmp(liens[i]->former_adr,adr)==0) && (strcmp(liens[i]->former_fil,fil_complete_patche)==0))
#else
          if ((strfield2(liens[i]->former_adr,adr)) && (strfield2(liens[i]->former_fil,fil_complete_patche)))
#endif
          {    // ok c'est le même lien, adresse déja définie
            // écraser fil et adr (pas former_fil?????)
            strcpy(adr_complete,liens[i]->adr);
            strcpy(fil_complete,liens[i]->fil);
            // écrire save
            strcpy(save,liens[i]->sav);
            return 0;
          }
        }
      }
#endif
    }
  }

  // vérifier la non présence de paramètres dans le nom de fichier
  // si il y en a, les supprimer (ex: truc.cgi?subj=aspirateur)
  // néanmoins, gardé pour vérifier la non duplication (voir après)
  {
    char* a;
    a=strchr(fil,'?');
    if (a!=NULL) {
      strncat(newfil,fil,(int) (a - fil));
    } else {
      strcpy(newfil,fil);
    }
    fil=newfil;
  }
  // décoder %
  strcpy(fil,unescape_http(fil));
  /*
  {
    char tempo[HTS_URLMAXSIZE*2];
    int i,j=0;
    for (i=0;i<(int) strlen(fil);i++) {
      if (fil[i]=='%') {
        i++;
        tempo[j++]=(char) ehex(fil+i);
        i++;    // sauter 2 caractères finalement
      } else
        tempo[j++]=fil[i];
    }
    tempo[j++]='\0';
    strcpy(fil,tempo);
  }
  */
  
  
  /* replace shtml to html.. */
  switch (ishtml(fil)) {       /* .html,.shtml,.. */
  case 1:
    if ( 
      (strcmp(get_ext(fil),"html") != 0)
      && (strcmp(get_ext(fil),"htm") != 0)
      ) {
      strcpy(ext,"html");
      ext_chg=1;
    }
    break;
    case 0:
      if (!strnotempty(ext)) {
        if (is_userknowntype(get_ext(fil))) {      // mime known by user
          char mime[1024];
          mime[0]=ext[0]='\0';
          get_userhttptype(0,mime,get_ext(fil));
          if (strnotempty(mime)) {
            give_mimext(ext,mime);
            if (strnotempty(ext)) {
              ext_chg=1;
            }
          }
        }
      }
      break;
  }
  

  // si option check_type activée
  if ((opt->check_type) && (!ext_chg)) {
    if ( (!strfield(adr_complete,"file://")) 
      && (!strfield(adr_complete,"ftp://")) 
      ) {
      // tester type avec requète HEAD si on ne connait pas le type du fichier
      if (!(   (opt->check_type==1) && (fil[strlen(fil)-1]=='/')   ))    // slash doit être html?
      if (ishtml(fil)<0) { // on ne sait pas si c'est un html ou un fichier..
        // lire dans le cache
        htsblk r = cache_read(opt,cache,adr,fil,NULL);              // test uniquement
        if (r.statuscode != -1) {  // pas d'erreur de lecture cache
          char s[16]; s[0]='\0';
          if ( (opt->debug>1) && (opt->log!=NULL) ) {
            fspc(opt->log,"debug"); fprintf(opt->log,"Testing link type (from cache) %s%s"LF,adr_complete,fil_complete);
            test_flush;
          }
          if (strnotempty(r.cdispo)) {        /* filename given */
            ext_chg=2;      /* change filename */
            strcpy(ext,r.cdispo);
          }
          else if (!may_unknown(r.contenttype)) {  // on peut patcher à priori?
            give_mimext(s,r.contenttype);  // obtenir extension
            if (strnotempty(s)>0) {        // on a reconnu l'extension
              ext_chg=1;
              strcpy(ext,s);
            }
          }
          //
        } else {          // test imposible dans le cache, faire une requête
          //
#if HTS_ANALYSTE
          int hihp=_hts_in_html_parsing;
#endif
          int has_been_moved=0;
          char curr_adr[HTS_URLMAXSIZE*2],curr_fil[HTS_URLMAXSIZE*2];
          curr_adr[0]=curr_fil[0]='\0';
#if HTS_ANALYSTE
          _hts_in_html_parsing=2;  // test
#endif
          if ( (opt->debug>1) && (opt->log!=NULL) ) {
            fspc(opt->log,"debug"); fprintf(opt->log,"Testing link type %s%s"LF,adr_complete,fil_complete);
            test_flush;
          }
          strcpy(curr_adr,adr_complete);
          strcpy(curr_fil,fil_complete);
          // ajouter dans le backing le fichier en mode test
          // savename: rien car en mode test
          if (back_add(back,back_max,opt,cache,curr_adr,curr_fil,BACK_ADD_TEST,referer_adr,referer_fil,1,NULL)!=-1) {
            int b;
            b=back_index(back,back_max,curr_adr,curr_fil,BACK_ADD_TEST);         
            if (b>=0) {
              int petits_tours=0;
              int get_test_request=0;       // en cas de bouclage sur soi même avec HEAD, tester avec GET.. parfois c'est la cause des problèmes
              do {
                // temps à attendre, et remplir autant que l'on peut le cache (backing)
                if (back[b].status>0) back_wait(back,back_max,opt,cache,0);        
                if (ptr>=0)
                  back_fillmax(back,back_max,opt,cache,liens,ptr,numero_passe,lien_tot);
  
                // on est obligé d'appeler le shell pour le refresh..
#if HTS_ANALYSTE
                {
                  
                  // Transfer rate
                  engine_stats();
                  
                  // Refresh various stats
                  HTS_STAT.stat_nsocket=back_nsoc(back,back_max);
                  HTS_STAT.stat_errors=fspc(NULL,"error");
                  HTS_STAT.stat_warnings=fspc(NULL,"warning");
                  HTS_STAT.stat_infos=fspc(NULL,"info");
                  HTS_STAT.nbk=backlinks_done(liens,lien_tot,ptr);
                  HTS_STAT.nb=back_transfered(HTS_STAT.stat_bytes,back,back_max);

                  if (!hts_htmlcheck_loop(back,back_max,b,ptr,lien_tot,(int) (time_local()-HTS_STAT.stat_timestart),&HTS_STAT)) {
                    return -1;
                  } else if (_hts_cancel) {    // cancel 2 ou 1 (cancel parsing)
                    back_delete(back,b);       // cancel test
                  }
                }
#endif
                
                
                // traitement des 304,303..
                if (back[b].status<=0) {
                  if (    (back[b].r.statuscode==301)
                       || (back[b].r.statuscode==302)
                       || (back[b].r.statuscode==303)
                       || (back[b].r.statuscode==307)
                     ) {    // agh moved.. un tit tour de plus
                    if ((petits_tours<5) && (former_adr) && (former_fil)) { // on va pas tourner en rond non plus!
                      if ((int) strnotempty(back[b].r.location)) {    // location existe!
                        char mov_url[HTS_URLMAXSIZE*2],mov_adr[HTS_URLMAXSIZE*2],mov_fil[HTS_URLMAXSIZE*2];
                        mov_url[0]=mov_adr[0]=mov_fil[0]='\0';
                        //
                        strcpy(mov_url,back[b].r.location);    // copier URL
                        if (ident_url_relatif(mov_url,curr_adr,curr_fil,mov_adr,mov_fil)>=0) {                        
                          // si non bouclage sur soi même, ou si test avec GET non testé
                          if ((strcmp(mov_adr,curr_adr)) || (strcmp(mov_fil,curr_fil)) || (get_test_request==0)) {
                            // bouclage?
                            if ((!strcmp(mov_adr,curr_adr)) && (!strcmp(mov_fil,curr_fil)))
                              get_test_request=1;     // faire requète avec GET

                            // recopier former_adr/fil?
                            if ((former_adr) && (former_fil)) {
                              if (strnotempty(former_adr)==0) {    // Pas déja noté
                                strcpy(former_adr,curr_adr);
                                strcpy(former_fil,curr_fil);
                              }
                            }

                            // check explicit forbidden - don't follow 3xx in this case
                            {
                              int set_prio_to=0;
                              robots_wizard* robots = (robots_wizard*) opt->robotsptr;
                              if (hts_acceptlink(opt,ptr,lien_tot,liens,
                                mov_adr,mov_fil,
                                opt->filters.filters,opt->filters.filptr,opt->maxfilter,
                                robots,
                                &set_prio_to,
                                NULL) == 1) 
                              {  /* forbidden */
                                has_been_moved = 1;
                                back_delete(back,b);    // ok
                                strcpy(curr_adr,mov_adr);
                                strcpy(curr_fil,mov_fil);
                                mov_url[0]='\0';
                              }
                            }
                            
                            // ftp: stop!
                            if (strfield(mov_url,"ftp://")) {    // ftp, ok on arrête
                              has_been_moved = 1;
                              back_delete(back,b);    // ok
                              strcpy(curr_adr,mov_adr);
                              strcpy(curr_fil,mov_fil);
                            } else if (*mov_url) {
                              char* methode;
                              if (!get_test_request)
                                methode=BACK_ADD_TEST;      // tester avec HEAD
                              else {
                                methode=BACK_ADD_TEST2;     // tester avec GET
                                if ( opt->errlog!=NULL ) {
                                  fspc(opt->errlog,"warning"); fprintf(opt->errlog,"Loop with HEAD request (during prefetch) at %s%s"LF,curr_adr,curr_fil);
                                  test_flush;
                                }                    
                              }
                              // Ajouter
                              if (back_add(back,back_max,opt,cache,mov_adr,mov_fil,methode,referer_adr,referer_fil,1,NULL)!=-1) {    // OK
                                if ( (opt->debug>1) && (opt->errlog!=NULL) ) {
                                  fspc(opt->errlog,"warning"); fprintf(opt->errlog,"(during prefetch) %s (%d) to link %s at %s%s"LF,back[b].r.msg,back[b].r.statuscode,back[b].r.location,curr_adr,curr_fil);
                                  test_flush;
                                }
                                
                                // libérer emplacement backing actuel et attendre le prochain
                                back_delete(back,b);
                                strcpy(curr_adr,mov_adr);
                                strcpy(curr_fil,mov_fil);
                                b=back_index(back,back_max,curr_adr,curr_fil,methode);         
                                if (!get_test_request)
                                  has_been_moved = 1;       // sinon ne pas forcer has_been_moved car non déplacé
                                petits_tours++;
                                //
                              } else {// sinon on fait rien et on s'en va.. (ftp etc)
                                if ( (opt->debug>1)  && (opt->errlog)) {
                                  fspc(opt->errlog,"debug"); fprintf(opt->errlog,"Warning: Savename redirect backing error at %s%s"LF,mov_adr,mov_fil);
                                  test_flush;
                                } 
                              }
                            }
                          } else {
                            if ( opt->errlog!=NULL ) {
                              fspc(opt->errlog,"warning"); fprintf(opt->errlog,"Unable to test %s%s (loop to same filename)"LF,adr_complete,fil_complete);
                              test_flush;
                            }
                          }
                          
                        }
                      }
                    } else{  // arrêter les frais
                      if ( opt->errlog!=NULL ) {
                        fspc(opt->errlog,"warning"); fprintf(opt->errlog,"Unable to test %s%s (loop)"LF,adr_complete,fil_complete);
                        test_flush;
                      }
                    }
                  }  // ok, leaving
                }
                
              } while(back[b].status>0);
              
              // Si non déplacé, forcer type?
              if (!has_been_moved) {
                if (back[b].r.statuscode!=-10) {    // erreur
                  if (strnotempty(back[b].r.contenttype)==0)
                    strcpy(back[b].r.contenttype,"text/html");    // message d'erreur en html
                  // Finalement on, renvoie un erreur, pour ne toucher à rien dans le code
                  // libérer emplacement backing
                  /*if (opt->errlog!=NULL) {
                    fspc(opt->errlog,0); fprintf(opt->errlog,"Error: (during prefetch) %s (%d) to link %s at %s%s"LF,back[b].r.msg,back[b].r.statuscode,back[b].r.location,curr_adr,curr_fil);
                    test_flush;
                  }                    
                  back_delete(back,b);
                  return -1;        // ERREUR (404 par exemple)
                  */
                } 
                
                {            // pas d'erreur, changer type?
                  char s[16];
                  s[0]='\0';
                  if (strnotempty(back[b].r.cdispo)) {        /* filename given */
                    ext_chg=2;      /* change filename */
                    strcpy(ext,back[b].r.cdispo);
                  }
                  else if ((!may_unknown(back[b].r.contenttype)) || (!get_ext(back[b].url_fil)) ) {  // on peut patcher à priori? (pas interdit ou pas de type)
                    give_mimext(s,back[b].r.contenttype);  // obtenir extension
                    if (strnotempty(s)>0) {    // on a reconnu l'extension
                      ext_chg=1;
                      strcpy(ext,s);
                    }
                  }
                }
              }
              // FIN Si non déplacé, forcer type?

              // libérer emplacement backing
              back_delete(back,b);
              
              // --- --- ---
              // oops, a été déplacé.. on recalcule en récursif (osons!)
              if (has_been_moved) {
                // copier adr, fil (optionnel, mais sinon marche pas pour le rip)
                strcpy(adr_complete,curr_adr);
                strcpy(fil_complete,curr_fil);
                // copier adr, fil
                
                return url_savename(curr_adr,curr_fil,save,NULL,NULL,referer_adr,referer_fil,opt,liens,lien_tot,back,back_max,cache,hash,ptr,numero_passe);
              }
              // --- --- ---
              
            }
            
          } else {
            printf("PANIC! : Savename Crash adding error, unexpected error found.. [%d]\n",__LINE__);
#if BDEBUG==1
            printf("error while savename crash adding\n");
#endif
            if (opt->errlog) {
              fspc(opt->errlog,"error"); fprintf(opt->errlog,"Unexpected savename backing error at %s%s"LF,adr,fil_complete);
              test_flush;
            } 
            
          }
          // restaurer
#if HTS_ANALYSTE
          _hts_in_html_parsing=hihp;
#endif
        }  // caché?
      }
    }
  }



  // - - - DEBUT NOMMAGE - - -

  // Donner nom par défaut?
  if (fil[strlen(fil)-1]=='/')  {
    if (!strfield(adr_complete,"ftp://"))
      strcat(fil,DEFAULT_HTML);     // nommer page par défaut!!
    else {
      if (!opt->proxy.active)
        strcat(fil,DEFAULT_FTP);     // nommer page par défaut (texte)
      else
        strcat(fil,DEFAULT_HTML);     // nommer page par défaut (à priori ici html depuis un proxy http)
    }
  }
  // Changer extension?
  // par exemple, php3 sera sauvé en html, cgi en html ou gif, xbm etc.. selon les cas
  if (ext_chg) {  // changer ext
    char* a=fil+strlen(fil)-1;
    if ( (opt->debug>1) && (opt->log!=NULL) ) {
      fspc(opt->log,"debug");
      if (ext_chg==1)
        fprintf(opt->log,"Changing link extension %s%s to .%s"LF,adr_complete,fil_complete,ext);
      else
        fprintf(opt->log,"Changing link name %s%s to %s"LF,adr_complete,fil_complete,ext);
      test_flush;
    }
    if (ext_chg==1) {
      while((a > fil) && (*a!='.') && (*a!='/')) a--;
      if (*a=='.') *a='\0';  // couper
      strcat(fil,".");      // recopier point
    } else {
      while(( a > fil) && (*a!='/')) a--;
      if (*a=='/') a++;
      *a='\0';
   }
    strcat(fil,ext);    // copier ext/nom
  }

  // Rechercher premier / et dernier .
  {  
    char* a=fil+strlen(fil)-1;

    // passer structures
    start_pos=fil;
    while(( a > fil) && (*a != '/') && (*a != '\\')) {
      if (*a == '.')    // point? noter position
        if (!dot_pos)
          dot_pos=a;
        a--;
    }
    if ((*a=='/') || (*a=='\\')) a++;
    nom_pos = a;
  }

  
  // un nom de fichier est généré
  // s'il existe déja, alors on le mofifie légèrement

  // ajouter nom du site éventuellement en premier
  if (opt->savename_type == -1) {    // utiliser savename_userdef! (%h%p/%n%q.%t)
    char* a = opt->savename_userdef;
    char* b = save;
    /*char *nom_pos=NULL,*dot_pos=NULL;  // Position nom et point */
    char tok;

    /*
    {  // Rechercher premier /
      char* a=fil+strlen(fil)-1;
      // passer structures
      while(((int) a>(int) fil) && (*a != '/') && (*a != '\\')) {
        if (*a == '.')    // point? noter position
        if (!dot_pos)
          dot_pos=a;
        a--;
      }
      if ((*a=='/') || (*a=='\\')) a++;
      nom_pos = a;
    }
    */

    // Construire nom
    while ((*a) && (((int) (b - save)) < HTS_URLMAXSIZE ) ) {    // parser, et pas trop long..
      if (*a == '%') {
        int short_ver=0;
        a++;
        if (*a == 's') {
          short_ver=1;
          a++;
        }
        *b='\0';
        switch(tok=*a++) {
          case '[':            // %[param]
            if (strchr(a,']')) {
              char name[256];
              char* c=name;
              while(*a!=']') {
                *c++=*a++;
              }
              a++;
              *c++='\0';
              strcat(name,"=");           /* param=.. */
              c=strchr(fil_complete,'?');
              /* parameters exists */
              if (c) {
                c=strstr(c,name);     /* finds param= */
                if (c) {
                  c+=strlen(name);    /* jumps "param=" */
                  while( (*c) && (*c!='&'))
                    *b++=*c++;
                }
              }
            }
          break;
          case '%': *b++='%'; break;
          case 'n':    // nom sans ext
            if (dot_pos) {
              if (!short_ver)    // Noms longs
                strncat(b,nom_pos,(int) (dot_pos - nom_pos));
              else
                strncat(b,nom_pos,min((int) (dot_pos - nom_pos),8));
            } else {
              if (!short_ver)    // Noms longs
                strcpy(b,nom_pos);
              else
                strncat(b,nom_pos,8);
            }
            b+=strlen(b);   // pointer à la fin
            break;
          case 'N':    // nom avec ext
            // RECOPIE NOM + EXT
            *b='\0';
            if (dot_pos) {
              if (!short_ver)    // Noms longs
                strncat(b,nom_pos,(int) (dot_pos - nom_pos));
              else
                strncat(b,nom_pos,min((int) (dot_pos - nom_pos),8));
            } else {
              if (!short_ver)    // Noms longs
                strcpy(b,nom_pos);
              else
                strncat(b,nom_pos,8);
            }
            b+=strlen(b);   // pointer à la fin
            // RECOPIE NOM + EXT
            *b='\0';
            if (dot_pos) {
              if (!short_ver)    // Noms longs
                strcpy(b,dot_pos+1);
              else
                strncat(b,dot_pos+1,3);
            } else {
              if (!short_ver)    // Noms longs
                strcpy(b,DEFAULT_EXT);    // pas de..
              else
                strcpy(b,DEFAULT_EXT_SHORT);    // pas de..
            }
            b+=strlen(b);   // pointer à la fin
            //
            break;
          case 't':    // ext
            *b='\0';
            if (dot_pos) {
              if (!short_ver)    // Noms longs
                strcpy(b,dot_pos+1);
              else
                strncat(b,dot_pos+1,3);
            } else {
              if (!short_ver)    // Noms longs
                strcpy(b,DEFAULT_EXT);    // pas de..
              else
                strcpy(b,DEFAULT_EXT_SHORT);    // pas de..
            }
            b+=strlen(b);   // pointer à la fin
            break;
          case 'p':    // path sans dernier /
            *b='\0';
            if (nom_pos != fil + 1) { // pas: /index.html (chemin nul)
              if (!short_ver) {   // Noms longs
                strncat(b,fil,(int) (nom_pos - fil) - 1);
              } else {
                char pth[HTS_URLMAXSIZE*2],n83[HTS_URLMAXSIZE*2];
                pth[0]=n83[0]='\0';
                //
                strncat(pth,fil,(int) (nom_pos - fil) - 1);
                long_to_83(opt->savename_83,n83,pth);
                strcpy(b,n83);
              }
            }
            b+=strlen(b);   // pointer à la fin
            break;
          case 'h':    // host
            *b='\0';
            if (strcmp(adr_complete,"file://")==0) {
              if (!short_ver)    // Noms longs
                strcpy(b,"localhost");
              else
                strcpy(b,"local");
            } else {
              if (!short_ver)    // Noms longs
                strcpy(b,print_adr);
              else
                strncat(b,print_adr,8);
            }
            b+=strlen(b);   // pointer à la fin
            break;
          case 'M':         /* host/address?query MD5 (128-bits) */
            *b='\0';
            {
              char digest[32+2];
              char buff[HTS_URLMAXSIZE*2];
              digest[0]=buff[0]='\0';
              strcpy(buff,adr);
              strcat(buff,fil_complete);
              domd5mem(buff,strlen(buff),digest,1);
              strcpy(b,digest);
            }
            b+=strlen(b);   // pointer à la fin
            break;
          case 'Q': case 'q':         /* query MD5 (128-bits/16-bits) 
                                         GENERATED ONLY IF query string exists! */
            *b='\0';
            strncat(b,url_md5(fil_complete),(tok == 'Q')?32:4);
            b+=strlen(b);   // pointer à la fin
            break;
        }
      } else
        *b++=*a++;
    }
    *b++='\0';
    //
    // Types prédéfinis
    //

  } 
  //
  // Structure originale
  else if (opt->savename_type%100==0) { 
    /* recopier www.. */
    if (opt->savename_type!=100) {  
      if (((opt->savename_type/1000)%2)==0) {    // >1000 signifie "pas de www/"
        if (strcmp(adr_complete,"file://")==0) {
          //## if (*adr==lOCAL_CHAR) {
          if (opt->savename_83 != 1)  // noms longs
            strcat(save,"localhost");
          else
            strcat(save,"local");
        } else {
          // adresse url
          if (!opt->savename_83) {  // noms longs (et pas de .)
            strcat(save,print_adr);
          } else {  // noms 8-3
            if (strlen(print_adr)>4) {
              if (strfield(print_adr,"www."))
                strncat(save,print_adr+4,max_char);
              else
                strncat(save,print_adr,8);
            } else strncat(save,print_adr,max_char);
          }
        }
        if (*fil!='/') strcat(save,"/");
      }
    }
  
#if HTS_CASSE==0
    hts_lowcase(save);
#endif  
        
    /*
    // ne sert à rien car a déja été filtré normalement
    if ((*fil=='.') && (*(fil+1)=='/'))          // ./index.html ** //
      url_savename_addstr(save,fil+2);
    else                                               // index.html ou /index.html
      url_savename_addstr(save,fil);
    if (save[strlen(save)-1]=='/') 
      strcat(save,DEFAULT_HTML);     // nommer page par défaut!!
*/

    /* add name */
    ADD_STANDARD_PATH;
    ADD_STANDARD_NAME(0);

  }
  //
  // Structure html/image
  else {    
    // dossier "web" ou "www.xxx" ?
    if (((opt->savename_type/1000)%2)==0) {    // >1000 signifie "pas de www/"
      if ((opt->savename_type/100)%2) {
        if (strcmp(adr_complete,"file://")==0) {
        //## if (*adr==lOCAL_CHAR) {
          if (opt->savename_83 != 1)  // noms longs
            strcat(save,"localhost/");
          else
            strcat(save,"local/");
        } else {
          // adresse url
          if (!opt->savename_83) {  // noms longs
            strcat(save,print_adr); strcat(save,"/");
          } else {  // noms 8-3
            if (strlen(print_adr)>4) {
              if (strfield(print_adr,"www."))
                strncat(save,print_adr+4,max_char);
              else
                strncat(save,print_adr,max_char);
              strcat(save,"/");
            } else { 
              strncat(save,print_adr,max_char); strcat(save,"/");
            }
          }
        }
      } else {
        strcat(save,"web/");    // répertoire général
      }
    } 

    // si un html à coup sûr
    if ( (ext_chg!=0) ? (ishtml_ext(ext)==1) : (ishtml(fil)==1) ) {
      if (opt->savename_type%100==2) {  // html/
        strcat(save,"html/");
      }
    } else {
      if ((opt->savename_type%100==1) || (opt->savename_type%100==2)) {  // html & images
        strcat(save,"images/");
      }
    }
    
    switch (opt->savename_type%100) {
    case 4: case 5: {           // séparer par types
      char* a=fil+strlen(fil)-1;
      // passer structures
      while(( a > fil) && (*a != '/') && (*a != '\\')) a--;      
      if ((*a=='/') || (*a=='\\')) a++;

      // html?
      if ( (ext_chg!=0) ? (ishtml_ext(ext)==1) : (ishtml(fil)==1) ) {
        if (opt->savename_type%100==5)
          strcat(save,"html/");
      } else {
        char* a=fil+strlen(fil)-1;
        while(( a> fil) && (*a != '/') && (*a != '.')) a--;      
        if (*a!='.')
          strcat(save,"other");
        else
          strcat(save,a+1);
        strcat(save,"/");
      }
      /*strcat(save,a);*/
      /* add name */
      ADD_STANDARD_NAME(0);
            }
      break;
    case 99: {                  // 'codé' .. c'est un gadget
      int i;
      int j;
      char* a;
      char C[]="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";
      int L;
      // pseudo-CRC sur fil et adr pour initialiser générateur aléatoire..
      unsigned int s=0;
      L=strlen(C);
      for(i=0;i<(int) strlen(fil_complete);i++) {
        s+=(unsigned int) fil_complete[i];
      }
      for(i=0;i<(int) strlen(adr_complete);i++) {
        s+=(unsigned int) adr_complete[i];
      }
      srand(s);
      
      j=strlen(save);
      for(i=0;i<8;i++) {
        char c=C[(rand()%L)];
        save[i+j]=c;
      }
      save[i+j]='\0';
      // ajouter extension
      a=fil+strlen(fil)-1;
      while(( a > fil) && (*a != '/') && (*a != '.')) a--;
      if (*a=='.') {
        strcat(save,a);    // ajouter
      }
             } 
      break;
    default: {   // noms sans les noms des répertoires
      // ne garder que le nom, pas la structure
      /*
      char* a=fil+strlen(fil)-1;
      while(((int) a>(int) fil) && (*a != '/') && (*a != '\\')) a--;      
      if ((*a=='/') || (*a=='\\')) a++;
      strcat(save,a);
      */

      /* add name */
      ADD_STANDARD_NAME(0);
            }
      break;
    }

#if HTS_CASSE==0
    hts_lowcase(save);
#endif  

    if (save[strlen(save)-1]=='/') 
      strcat(save,DEFAULT_HTML);     // nommer page par défaut!!
  }


  // vérifier qu'on ne doit pas forcer l'extension
  // par exemple, asp sera sauvé en html, cgi en html ou gif, xbm etc.. selon les cas
  /*if (ext_chg) {
    char* a=save+strlen(save)-1;
    while(((int) a>(int) save) && (*a!='.') && (*a!='/')) a--;
    if (*a=='.') *a='\0';  // couper
    // recopier extension
    strcat(save,".");
    strcat(save,ext);    // copier ext
  }*/
  // de même en cas de manque d'extension on en place une de manière forcée..
  // cela évite les /chez/toto et les /chez/toto/index.html incompatibles
  if (opt->savename_type != -1) {
    char* a=save+strlen(save)-1;
    while(( a > save) && (*a!='.') && (*a!='/')) a--;
    if (*a!='.') {   // agh pas de point
      //strcat(save,".none");                 // a éviter
      strcat(save,".html");                   // préférable!
      if ( (opt->debug>1) && (opt->errlog!=NULL) ) {
        fspc(opt->errlog,"warning"); fprintf(opt->errlog,"Default HTML type set for %s%s"LF,adr_complete,fil_complete);
        test_flush;
      }
    }
  }

  // effacer pass au besoin pour les autentifications
  // (plus la peine : masqué au début)
/*
  {
    char* a=jump_identification(save);
    if (a!=save) {
      char tempo[HTS_URLMAXSIZE*2];
      char *b;
      tempo[0]='\0';
      strcpy(tempo,"[");
      b=strchr(save,':');
      if (!b) b=strchr(save,'@');
      if (b)
        strncat(tempo,save,(int) b-(int) a);
      strcat(tempo,"]");
      strcat(tempo,a);
      strcpy(save,a);
    }
  }
*/

  // éviter les / au début (cause: N100)
  if (save[0]=='/') {
    char tempo[HTS_URLMAXSIZE*2];
    strcpy(tempo,save+1);
    strcpy(save,tempo);
  }

  // changer les ~,:,",*,? en _ pour sauver sur disque
  hts_replace(save,'~','_');  // interdit sous unix (~foo)
  //
  hts_replace(save,'\\','_');
  hts_replace(save,':','_');  // interdit sous windows
  hts_replace(save,'*','_');  // interdit sous windows
  hts_replace(save,'?','_');  // doit pas arriver!!
  hts_replace(save,'\"','_');  // interdit sous windows
  hts_replace(save,'<','_');  // interdit sous windows
  hts_replace(save,'>','_');  // interdit sous windows
  hts_replace(save,'|','_');  // interdit sous windows
  //
  hts_replace(save,'@','_');
  //
  { // éliminer les // (comme ftp://)
    char* a;
    while( (a=strstr(save,"//")) ) *a='_';
    // Eliminer chars spéciaux
    a=save -1 ;
    while(*(++a))
      if ( ((unsigned char)(*a) <= 31)
          || ((unsigned char)(*a) == 127) )
        *a='_';
  }

   
#if HTS_OVERRIDE_DOS_FOLDERS
  /* Replace /foo/nul/bar by /foo/nul-/bar */
  {
    int i=0;
    while(hts_tbdev[i][0]) {
      char* a=save;
      while((a=strstr(a,hts_tbdev[i]))) {
        switch ( (int) a[strlen(hts_tbdev[i])] ) {
        case '\0':
        case '/':  {
          char tempo[HTS_URLMAXSIZE*2]; tempo[0]='\0';
          strncat(tempo,save,(int) (a - save) + strlen(hts_tbdev[i]));
          strcat(tempo,"-");
          strcat(tempo,a+strlen(hts_tbdev[i]));
          strcpy(save,tempo);
                   }
          break;
        }
        a+=strlen(hts_tbdev[i]);
      }
      i++;
    }
  }
#endif

  // conversion 8-3 .. y compris pour les répertoires
  if (opt->savename_83) {
    char n83[HTS_URLMAXSIZE*2];
    long_to_83(opt->savename_83,n83,save);
    strcpy(save,n83);
  }


  /* ensure that there is no ../ (potential vulnerability) */
  fil_simplifie(save);

#if HTS_ANALYSTE
  {
    hts_htmlcheck_savename(adr_complete,fil_complete,referer_adr,referer_fil,save);
    if ( (opt->debug>0) && (opt->log!=NULL) ) {
      fspc(opt->log,"info"); fprintf(opt->log,"engine: save-name: local name: %s%s -> %s"LF,adr,fil,save);
      test_flush;
    }
  }
#endif

  // chemin primaire éventuel A METTRE AVANT
  if (strnotempty(opt->path_html)) {
    char tempo[HTS_URLMAXSIZE*2];
    strcpy(tempo,opt->path_html);
    strcat(tempo,save);
    strcpy(save,tempo);
  }


  // vérifier que le nom n'est pas déja pris...
  if (liens!=NULL) { 
    int nom_ok;
    do {
      int i;
      int len;
      len=strlen(save);    // taille
      //
      nom_ok=1;  // à priori bon
      // on part de la fin pour optimiser, plus les opti de taille pour aller encore plus vite..
#if DEBUG_SAVENAME
printf("\nStart search\n");
#endif

#if HTS_HASH
      i=hash_read(hash,save,"",0);      // lecture type 0 (sav)
      if (i>=0)
#else
      for(i=lien_tot-1;i>=0;i--) {
#if DEBUG_SAVENAME
printf("%cParse: %d",13,i);
#endif
        
        if (liens[i]->sav_len==len) {    // même taille de chaîne          
#if HTS_CASSE
          if (strcmp(liens[i]->sav,save)==0)    // existe déja
#else
          if (strfield2(liens[i]->sav,save))    // un tel nom existe déja
#endif
#endif
          {
#if HTS_CASSE
            if ((strcmp(liens[i]->adr,adr)==0) && (strcmp(liens[i]->fil,fil_complete)==0))
#else
            if ((strfield2(liens[i]->adr,adr)) && (strfield2(liens[i]->fil,fil_complete)))
#endif
            {    // ok c'est le même lien, adresse déja définie
              //printf("Ok, %s\n",save);
              //i=lien_tot;    // sortir
              i=0;
#if DEBUG_SAVENAME
printf("\nOK ALREADY DEFINED\n",13,i);
#endif
            } else {  // utilisé par un AUTRE, changer de nom
              char tempo[HTS_URLMAXSIZE*2];
              char* a=save+strlen(save)-1;
              char* b;
              int n=2;       
              tempo[0]='\0';

#if DEBUG_SAVENAME
printf("\nWRONG CASE UNMATCH : \n%s\n%s, REDEFINE\n",liens[i]->fil,fil_complete);
#endif
              nom_ok=0;
              i=0;
              
              while(( a > save) && (*a!='.') && (*a!='\\') && (*a!='/')) a--;
              if (*a=='.')
                strncat(tempo,save,(int) (a - save));
              else
                strcat(tempo,save);
              
              // tester la présence d'un -xx (ex: index-2.html -> index-3.html)
              b=tempo+strlen(tempo)-1;
              while (isdigit((unsigned char)*b)) b--;
              if (*b=='-') {
                sscanf(b+1,"%d",&n);
                *b='\0';    // couper
                n++;  // plus un
              }
              
              // en plus il faut gérer le 8-3 .. pas facile le client
              if (opt->savename_83) {
                int max;
                char* a=tempo+strlen(tempo)-1;
                while(( a > tempo) && (*a!='/')) a--;
                if (*a=='/') a++;
                max=max_char-1-nombre_digit(n);
                if ((int) strlen(a)>max)
                  *(a+max)='\0';  // couper sinon il n'y aura pas la place!
              }
              
              // ajouter -xx (ex: index.html -> index-2.html)
              sprintf(tempo+strlen(tempo),"-%d",n);
              
              // ajouter extension
              if (*a=='.')
                strcat(tempo,a);
              
              strcpy(save,tempo);
              
              //printf("switched: %s\n",save);
              
            }  // if
#if HTS_HASH
          }
#else
          }  // if
        }  // if sav_len
      }  // for
#endif
#if DEBUG_SAVENAME
printf("\nEnd search, %s\n",fil_complete);
#endif
    } while(!nom_ok);
    
  }
  
  //printf("'%s' %s %s\n",save,adr,fil);
      
  return 0;
}

/* nom avec md5 urilisé partout */
void standard_name(char* b,char* dot_pos,char* nom_pos,char* fil_complete,int short_ver) {
  b[0]='\0';
  /* Nom */
  if (dot_pos) {
    if (!short_ver)    // Noms longs
      strncat(b,nom_pos,(int) (dot_pos - nom_pos));
    else
      strncat(b,nom_pos,min((int) (dot_pos - nom_pos),8));
  } else {
    if (!short_ver)    // Noms longs
      strcat(b,nom_pos);
    else
      strncat(b,nom_pos,8);
  }
  /* MD5 - 16 bits */
  strncat(b,url_md5(fil_complete),4);
  /* Ext */
  if (dot_pos) {
    strcat(b,".");
    if (!short_ver)    // Noms longs
      strcat(b,dot_pos+1);
    else
      strncat(b,dot_pos+1,3);
  } else {
    if (!short_ver)    // Noms longs
      strcat(b,DEFAULT_EXT);    // pas de..
    else
      strcat(b,DEFAULT_EXT_SHORT);    // pas de..
  }
}


/* Petit md5 */
char* url_md5(char* fil_complete) {
  char* digest;
  char* a;
  NOSTATIC_RESERVE(digest, char, 32+2);
  digest[0]='\0';
  a=strchr(fil_complete,'?');
  if (a) {
    if (strlen(a)) {
      char buff[HTS_URLMAXSIZE*2];
      a++;
      digest[0]=buff[0]='\0';
      strcat(buff,a);         /* query string MD5 */
      domd5mem(buff,strlen(buff),digest,1);
    }
  }
  return digest;
}

// interne à url_savename: ajoute une chaîne à une autre avec \ -> /
void url_savename_addstr(char* d,char* s) {
  int i=strlen(d);
  while(*s) {
    if (*s=='\\')  // remplacer \ par des /
      d[i++]='/';
    else
      d[i++]=*s;
    s++;
  }
  d[i]='\0';
}

#undef test_flush
