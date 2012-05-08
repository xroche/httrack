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
/* File: httrack.c subroutines:                                 */
/*       savename routine (compute output filename)             */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Internal engine bytecode */
#define HTS_INTERNAL_BYTECODE

#include "htscore.h"
#include "htsname.h"
#include "md5.h"
#include "htsmd5.h"
#include "htstools.h"
#include "htscharset.h"
#include <ctype.h>

#undef test_flush
#define test_flush if (opt->flush) { fflush(opt->log); fflush(opt->log); }

#define ADD_STANDARD_PATH \
    {  /* ajout nom */\
      char BIGSTK buff[HTS_URLMAXSIZE*2];\
      buff[0]='\0';\
      strncatbuff(buff,start_pos,(int) (nom_pos - start_pos));\
      url_savename_addstr(save,buff);\
    }

#define ADD_STANDARD_NAME(shortname) \
    {  /* ajout nom */\
      char BIGSTK buff[HTS_URLMAXSIZE*2];\
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


#define URLSAVENAME_WAIT_FOR_AVAILABLE_SOCKET() do { \
  int prev = opt->state._hts_in_html_parsing; \
  while(back_pluggable_sockets_strict(sback, opt) <= 0) { \
    opt->state. _hts_in_html_parsing = 6; \
    /* Wait .. */ \
    back_wait(sback,opt,cache,0); \
    /* Transfer rate */ \
    engine_stats(); \
    /* Refresh various stats */ \
    HTS_STAT.stat_nsocket=back_nsoc(sback); \
    HTS_STAT.stat_errors=fspc(opt,NULL,"error"); \
    HTS_STAT.stat_warnings=fspc(opt,NULL,"warning"); \
    HTS_STAT.stat_infos=fspc(opt,NULL,"info"); \
    HTS_STAT.nbk=backlinks_done(sback,liens,lien_tot,ptr); \
    HTS_STAT.nb=back_transfered(HTS_STAT.stat_bytes,sback); \
    /* Check */ \
    { \
      if (!RUN_CALLBACK7(opt, loop, sback->lnk, sback->count,-1,ptr,lien_tot,(int) (time_local()-HTS_STAT.stat_timestart),&HTS_STAT)) { \
        return -1; \
      } \
    } \
  } \
  opt->state._hts_in_html_parsing = prev; \
} while(0)

/* Strip all // */
static void cleanDoubleSlash(char *s) {
  int i, j;
  for(i = 0, j = 0 ; s[i] != '\0' ; i++) {
    if (s[i] == '/' && i != 0 && s[i - 1] == '/') {
      continue;
    }
    if (i != j) {
      s[j] = s[i];
    }
    j++;
  }
  if (i != j) {
    s[j] = s[i];
  }
}

// legacy version, without page charset
int url_savename(char* adr_complete, char* fil_complete, char* save, 
                 char* former_adr, char* former_fil, 
                 char* referer_adr, char* referer_fil, 
                 httrackp* opt, 
                 lien_url** liens, int lien_tot, 
                 struct_back* sback, cache_back* cache, hash_struct* hash, 
                 int ptr, int numero_passe, const lien_back* headers) {
  return url_savename2(adr_complete, fil_complete, save, former_adr, former_fil, 
    referer_adr, referer_fil, opt, 
    liens, lien_tot, sback, cache, hash, ptr, numero_passe, headers, /* unknown */ NULL);
}

// forme le nom du fichier à sauver (save) à partir de fil et adr
// système intelligent, qui renomme en cas de besoin (exemple: deux INDEX.HTML et index.html)
int url_savename2(char* adr_complete, char* fil_complete, char* save, 
								  char* former_adr, char* former_fil, 
								  char* referer_adr, char* referer_fil, 
								  httrackp* opt, 
								  lien_url** liens, int lien_tot, 
								  struct_back* sback, cache_back* cache, hash_struct* hash, 
								  int ptr, int numero_passe, const lien_back* headers, const char *charset) {
	char catbuff[CATBUFF_SIZE];
  const char* mime_type = ( headers && !HTTP_IS_REDIRECT(headers->r.statuscode) ) ? headers->r.contenttype : NULL;
  /*const char* mime_type = ( headers && HTTP_IS_OK(headers->r.statuscode) ) ? headers->r.contenttype : NULL;*/
  lien_back* const back = sback->lnk;
  /* */
  char BIGSTK newfil[HTS_URLMAXSIZE*2];   /* ="" */
  /*char BIGSTK normadr_[HTS_URLMAXSIZE*2];*/
  char BIGSTK normadr_[HTS_URLMAXSIZE*2], normfil_[HTS_URLMAXSIZE*2];
	enum { PROTOCOL_HTTP, PROTOCOL_HTTPS, PROTOCOL_FTP, PROTOCOL_FILE, PROTOCOL_MMS, PROTOCOL_UNKNOWN };
  static const char* protocol_str[] = {"http", "https", "ftp", "file", "mms", "unknown"};
  int protocol = PROTOCOL_HTTP;
  char* normadr;
  char* normfil;
  char* fil;
  char* adr;
  char* print_adr;
  char *start_pos=NULL,*nom_pos=NULL,*dot_pos=NULL;  // Position nom et point
  // pour changement d'extension ou de nom (content-disposition)
  int ext_chg=0, ext_chg_delayed=0;
  int is_html=0;
  char ext[256];
  int max_char=0;
  //CLEAR
  newfil[0]=ext[0]='\0';

  /* 8-3 ? */
  switch(opt->savename_83) {
  case 1:     // 8-3
    max_char=8;
    break;
  case 2:     // Level 2 File names may be up to 31 characters.
    max_char=31;
    break;
  default:
    max_char=8;
    break;
  }

  // effacer save
  save[0]='\0';
  // fil
  fil = fil_complete;
  // copy of fil, used for lookups (see urlhack)
  normfil = fil;
  // et adr (sauter user/pass)
  // on prend le parti de mettre les fichiers avec login/pass au même endroit que si ils
  // étaient capturés sans ces paramètres
  // c'est pour cette raison qu'on ignore totalement adr_complete (même pour la recherche en table de hachage)
  adr = jump_identification(adr_complete);
  // copy of adr, used for lookups (see urlhack)
  normadr = adr;

  // normalize the URL:
  // www.foo.com -> foo.com
  // www-42.foo.com -> foo.com
  // foo.com/bar//foobar -> foo.com/bar/foobar
  if (opt->urlhack) {
    // copy of adr (without protocol), used for lookups (see urlhack)
    normadr=adr_normalized(adr, normadr_);
    normfil=fil_normalized(fil,normfil_);
  } else {
    if (link_has_authority(adr_complete)) {     // https or other protocols : in "http/" subfolder
      char* pos = strchr(adr_complete, ':');
      if (pos != NULL) {
        normadr_[0] = '\0';
        strncatbuff(normadr_, adr_complete, (int)(pos - adr_complete));
        strcatbuff(normadr_, "://");
        strcatbuff(normadr_, normadr);
        normadr=normadr_;
      }
    }
  }

  // à afficher sans ftp://
  print_adr=jump_protocol(adr);
  if (strfield(adr_complete, "https:")) {
	  protocol = PROTOCOL_HTTPS;
  } else if (strfield(adr_complete, "ftp:")) {
	  protocol = PROTOCOL_FTP;
  } else if (strfield(adr_complete, "file:")) {
	  protocol = PROTOCOL_FILE;
  } else if (strfield(adr_complete, "mms:")) {
	  protocol = PROTOCOL_MMS;
  } else {
	  protocol = PROTOCOL_HTTP;
  }

  // court-circuit pour lien primaire
  if (strnotempty(adr)==0) {
    if (strcmp(fil,"primary")==0) {
      strcatbuff(save,"primary.html");
      return 0;
    }
  }


  // vérifier que le nom n'a pas déja été calculé (si oui le renvoyer tel que)
  // vérifier que le nom n'est pas déja pris...
  // NOTE: si on cherche /toto/ et que /toto est trouvé on le prend (et réciproquqment) ** // **
  if (liens!=NULL) { 
    int i;

    i=hash_read(hash,normadr,normfil,1,opt->urlhack);      // recherche table 1 (adr+fil)
    if (i>=0) {    // ok, trouvé
      strcpybuff(save,liens[i]->sav);
      return 0;
    }
    i=hash_read(hash,normadr,normfil,2,opt->urlhack);      // recherche table 2 (former_adr+former_fil)
    if (i>=0) {    // ok, trouvé
      // copier location moved!
      strcpybuff(adr_complete,liens[i]->adr);
      strcpybuff(fil_complete,liens[i]->fil);
      // et save
      strcpybuff(save,liens[i]->sav);  // copier (formé à partir du nouveau lien!)
      return 0;
    }

    // chercher sans / ou avec / dans former
    {
      char BIGSTK fil_complete_patche[HTS_URLMAXSIZE*2];
      strcpybuff(fil_complete_patche,normfil);
      // Version avec ou sans /
      if (fil_complete_patche[strlen(fil_complete_patche)-1]=='/')
        fil_complete_patche[strlen(fil_complete_patche)-1]='\0';
      else
        strcatbuff(fil_complete_patche,"/");
      i=hash_read(hash,normadr,fil_complete_patche,2,opt->urlhack);      // recherche table 2 (former_adr+former_fil)
      if (i>=0) {
        // écraser fil et adr (pas former_fil?????)
        strcpybuff(adr_complete,liens[i]->adr);
        strcpybuff(fil_complete,liens[i]->fil);
        // écrire save
        strcpybuff(save,liens[i]->sav);
        return 0;
      }
    }
  }

  // vérifier la non présence de paramètres dans le nom de fichier
  // si il y en a, les supprimer (ex: truc.cgi?subj=aspirateur)
  // néanmoins, gardé pour vérifier la non duplication (voir après)
  {
    char* a;
    a=strchr(fil,'?');
    if (a!=NULL) {
      strncatbuff(newfil,fil,(int) (a - fil));
    } else {
      strcpybuff(newfil,fil);
    }
    fil=newfil;
  }
  // Decode remaining %
  strcpybuff(fil,unescape_http(catbuff,fil));
	// , BUT do not decode high chars
  //strcpybuff(fil,unescape_http_unharm(fil, 1));
	// YES (not server side, but fs/client side)

#if HTS_USEMMS
	/* .asx hack */
	if (headers != NULL && headers->r.cdispo[0] != 0 
		&& strfield(headers->r.contenttype, "video/")
		&& strfield2(get_ext(OPT_GET_BUFF(opt),headers->r.cdispo), "asx") == 0) 
	{
		ext_chg = 1;
		strcpybuff(ext, "asx");
	}
	else if (headers != NULL && headers->r.contenttype[0] != 0 
		&& strfield2(headers->r.contenttype, "video/x-ms-asf"))
	{
		char *exts = get_ext(OPT_GET_BUFF(opt),headers->url_fil);
		if (strfield2(exts, "wmv") == 0)
		{
			ext_chg = 1;
			strcpybuff(ext, "wmv");
		}
		else if (strfield2(exts, "asf") == 0)
		{
			ext_chg = 1;
			strcpybuff(ext, "asf");
		}
		else if (strfield2(exts, "avi") == 0)
		{
			ext_chg = 1;
			strcpybuff(ext, "avi");
		}
		else if (strfield2(exts, "asx") == 0)
		{
			ext_chg = 1;
			strcpybuff(ext, "asx");
		}
	}
#endif

  /* replace shtml to html.. */
  if (opt->savename_delayed == 2)
    is_html = -1;            /* ALWAYS delay type */
  else
    is_html = ishtml(opt,fil);
  switch ( is_html ) {       /* .html,.shtml,.. */
  case 1:
    if ( 
      (strfield2(get_ext(OPT_GET_BUFF(opt),fil),"html") == 0)
      && (strfield2(get_ext(OPT_GET_BUFF(opt),fil),"htm") == 0)
      )
    {
      strcpybuff(ext,"html");
      ext_chg=1;
    }
    break;
  case 0:
    if (!strnotempty(ext)) {
      if (is_userknowntype(opt,fil)) {      // mime known by user
        char BIGSTK mime[1024];
        mime[0]=ext[0]='\0';
        get_userhttptype(opt,mime,fil);
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
  if (is_html < 0 && opt->check_type && !ext_chg) {
    int ishtest=0;
    if ( (!strfield(adr_complete,"file://")) 
      && (!strfield(adr_complete,"ftp://")) 
#if HTS_USEMMS
      && (!strfield(adr_complete,"mms://")) 
#endif
      ) {
      // tester type avec requète HEAD si on ne connait pas le type du fichier
      if (!(   (opt->check_type==1) && (fil[strlen(fil)-1]=='/')   ))    // slash doit être html?
      if ( opt->savename_delayed == 2 || (ishtest=ishtml(opt,fil)) < 0) { // on ne sait pas si c'est un html ou un fichier..
        // lire dans le cache
        htsblk r = cache_read_including_broken(opt,cache,adr,fil);              // test uniquement
        if (r.statuscode != -1) {  // pas d'erreur de lecture cache
          char s[16]; s[0]='\0';
          if ( (opt->debug>1) && (opt->log!=NULL) ) {
            HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"Testing link type (from cache) %s%s"LF,adr_complete,fil_complete);
            test_flush;
          }
          if (strnotempty(r.cdispo)) {        /* filename given */
            ext_chg=2;      /* change filename */
            strcpybuff(ext,r.cdispo);
          }
          else if (!may_unknown2(opt,r.contenttype,fil) || ishtest == -2) {  // on peut patcher à priori?
            give_mimext(s,r.contenttype);  // obtenir extension
            if (strnotempty(s)>0) {        // on a reconnu l'extension
              ext_chg=1;
              strcpybuff(ext,s);
            }
          }
          //
        } else if ( opt->savename_delayed != 2 && is_userknowntype(opt,fil)) {   /* PATCH BY BRIAN SCHRÖDER. 
                                                                            Lookup mimetype not only by extension, 
                                                                            but also by filename */
          /* Note: "foo.cgi => text/html" means that foo.cgi shall have the text/html MIME file type,
          that is, ".html" */
          char BIGSTK mime[1024];
          mime[0]=ext[0]='\0';
          get_userhttptype(opt, mime, fil);
          if (strnotempty(mime)) {
            give_mimext(ext, mime);
            if (strnotempty(ext)) {
              ext_chg=1;
            }
          }
        }
        // note: if savename_delayed is enabled, the naming will be temporary (and slightly invalid!)
        // note: if we are about to stop (opt->state.stop), back_add() will fail later
        else if (opt->savename_delayed != 0 && !opt->state.stop) {
          if (mime_type != NULL)  {
            ext[0] = '\0';
            if (*mime_type) {
              give_mimext(ext, mime_type);
            }
            if (strnotempty(ext)) {
              char mime_from_file[128];
              mime_from_file[0] = 0;
              get_httptype(opt, mime_from_file, fil, 1);
              if (!strnotempty(mime_from_file) || strcasecmp(mime_type, mime_from_file) != 0) {    /* different mime for this type */
                if (!may_unknown2(opt, mime_type, fil)) {
                  ext_chg = 1;
                }
              } else {
                ext_chg = 0;
              }
            }
          } else {
            /* Avoid collisions (no collisionning detection) */
            sprintf(ext, "%x.%s", opt->state.delayedId++, DELAYED_EXT);
            ext_chg = 1;
						ext_chg_delayed = 1;		/* due to naming system */
          }
        }
        // test imposible dans le cache, faire une requête
        else {
          //
          int hihp = opt->state._hts_in_html_parsing;
          int has_been_moved=0;
          char BIGSTK curr_adr[HTS_URLMAXSIZE*2],curr_fil[HTS_URLMAXSIZE*2];
          
          /* Ensure we don't use too many sockets by using a "testing" one
             If we have only 1 simultaneous connection authorized, wait for pending download
             Wait for an available slot 
          */
          URLSAVENAME_WAIT_FOR_AVAILABLE_SOCKET();

          /* Rock'in */
          curr_adr[0]=curr_fil[0]='\0';
         opt->state. _hts_in_html_parsing=2;  // test
          if ( (opt->debug>1) && (opt->log!=NULL) ) {
            HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"Testing link type %s%s"LF,adr_complete,fil_complete);
            test_flush;
          }
          strcpybuff(curr_adr,adr_complete);
          strcpybuff(curr_fil,fil_complete);
          // ajouter dans le backing le fichier en mode test
          // savename: rien car en mode test
          if (back_add(sback,opt,cache,curr_adr,curr_fil,BACK_ADD_TEST,referer_adr,referer_fil,1)!=-1) {
            int b;
            b=back_index(opt,sback,curr_adr,curr_fil,BACK_ADD_TEST);         
            if (b>=0) {
              int stop_looping=0;
              int petits_tours=0;
              int get_test_request=0;       // en cas de bouclage sur soi même avec HEAD, tester avec GET.. parfois c'est la cause des problèmes
              do {
                // temps à attendre, et remplir autant que l'on peut le cache (backing)
                if (back[b].status>0) {
                  back_wait(sback,opt,cache,0);        
                }
                if (ptr>=0) {
									back_fillmax(sback,opt,cache,liens,ptr,numero_passe,lien_tot);
								}

								// on est obligé d'appeler le shell pour le refresh..
								// Transfer rate
								engine_stats();

								// Refresh various stats
								HTS_STAT.stat_nsocket=back_nsoc(sback);
								HTS_STAT.stat_errors=fspc(opt,NULL,"error");
								HTS_STAT.stat_warnings=fspc(opt,NULL,"warning");
								HTS_STAT.stat_infos=fspc(opt,NULL,"info");
								HTS_STAT.nbk=backlinks_done(sback,liens,lien_tot,ptr);
								HTS_STAT.nb=back_transfered(HTS_STAT.stat_bytes,sback);

                if (!RUN_CALLBACK7(opt, loop, sback->lnk, sback->count,b,ptr,lien_tot,(int) (time_local()-HTS_STAT.stat_timestart),&HTS_STAT)) {
                  return -1;
                } else if (opt->state._hts_cancel || !back_checkmirror(opt)) {    // cancel 2 ou 1 (cancel parsing)
                  back_delete(opt,cache,sback,b);       // cancel test
                  stop_looping = 1;
                }

                // traitement des 304,303..
                if (back[b].status<=0) {
                  if (HTTP_IS_REDIRECT(back[b].r.statuscode)) {    // agh moved.. un tit tour de plus
                    if ((petits_tours<5) && (former_adr) && (former_fil)) { // on va pas tourner en rond non plus!
                      if ((int) strnotempty(back[b].r.location)) {    // location existe!
                        char BIGSTK mov_url[HTS_URLMAXSIZE*2],mov_adr[HTS_URLMAXSIZE*2],mov_fil[HTS_URLMAXSIZE*2];
                        mov_url[0]=mov_adr[0]=mov_fil[0]='\0';
                        //
                        strcpybuff(mov_url,back[b].r.location);    // copier URL
                        if (ident_url_relatif(mov_url,curr_adr,curr_fil,mov_adr,mov_fil)>=0) {                        
                          // si non bouclage sur soi même, ou si test avec GET non testé
                          if ((strcmp(mov_adr,curr_adr)) || (strcmp(mov_fil,curr_fil)) || (get_test_request==0)) {
                            // bouclage?
                            if ((!strcmp(mov_adr,curr_adr)) && (!strcmp(mov_fil,curr_fil)))
                              get_test_request=1;     // faire requète avec GET

                            // recopier former_adr/fil?
                            if ((former_adr) && (former_fil)) {
                              if (strnotempty(former_adr)==0) {    // Pas déja noté
                                strcpybuff(former_adr,curr_adr);
                                strcpybuff(former_fil,curr_fil);
                              }
                            }

                            // check explicit forbidden - don't follow 3xx in this case
                            {
                              int set_prio_to=0;
                              if (hts_acceptlink(opt,ptr,lien_tot,liens,
                                mov_adr,mov_fil,
                                NULL, NULL,
                                &set_prio_to,
                                NULL) == 1) 
                              {  /* forbidden */
                                has_been_moved = 1;
                                back_maydelete(opt,cache,sback,b);    // ok
                                strcpybuff(curr_adr,mov_adr);
                                strcpybuff(curr_fil,mov_fil);
                                mov_url[0]='\0';
                                stop_looping = 1;
                              }
                            }
                            
                            // ftp: stop!
                            if (strfield(mov_url,"ftp://")
#if HTS_USEMMS
															|| strfield(mov_url,"mms://")
#endif
															) 
														{    // ftp, ok on arrête
                              has_been_moved = 1;
                              back_maydelete(opt,cache,sback,b);    // ok
                              strcpybuff(curr_adr,mov_adr);
                              strcpybuff(curr_fil,mov_fil);
                              stop_looping = 1;
                            } else if (*mov_url) {
                              char* methode;
                              if (!get_test_request)
                                methode=BACK_ADD_TEST;      // tester avec HEAD
                              else {
                                methode=BACK_ADD_TEST2;     // tester avec GET
                                if ( opt->log!=NULL ) {
                                  HTS_LOG(opt,LOG_WARNING); fprintf(opt->log,"Loop with HEAD request (during prefetch) at %s%s"LF,curr_adr,curr_fil);
                                  test_flush;
                                }                    
                              }
                              // Ajouter
                              URLSAVENAME_WAIT_FOR_AVAILABLE_SOCKET();
                              if (back_add(sback,opt,cache,mov_adr,mov_fil,methode,referer_adr,referer_fil,1)!=-1) {    // OK
                                if ( (opt->debug>1) && (opt->log!=NULL) ) {
                                  HTS_LOG(opt,LOG_WARNING); fprintf(opt->log,"(during prefetch) %s (%d) to link %s at %s%s"LF,back[b].r.msg,back[b].r.statuscode,back[b].r.location,curr_adr,curr_fil);
                                  test_flush;
                                }
                                
                                // libérer emplacement backing actuel et attendre le prochain
                                back_maydelete(opt,cache,sback,b);
                                strcpybuff(curr_adr,mov_adr);
                                strcpybuff(curr_fil,mov_fil);
                                b=back_index(opt,sback,curr_adr,curr_fil,methode);         
                                if (!get_test_request)
                                  has_been_moved = 1;       // sinon ne pas forcer has_been_moved car non déplacé
                                petits_tours++;
                                //
                              } else {// sinon on fait rien et on s'en va.. (ftp etc)
                                if ( (opt->debug>1)  && (opt->log)) {
                                  HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"Warning: Savename redirect backing error at %s%s"LF,mov_adr,mov_fil);
                                  test_flush;
                                } 
                              }
                            }
                          } else {
                            if ( opt->log!=NULL ) {
                              HTS_LOG(opt,LOG_WARNING); fprintf(opt->log,"Unable to test %s%s (loop to same filename)"LF,adr_complete,fil_complete);
                              test_flush;
                            }
                          }
                          
                        }
                      }
                    } else{  // arrêter les frais
                      if ( opt->log!=NULL ) {
                        HTS_LOG(opt,LOG_WARNING); fprintf(opt->log,"Unable to test %s%s (loop)"LF,adr_complete,fil_complete);
                        test_flush;
                      }
                    }
                  }  // ok, leaving
                }
              } while(!stop_looping && back[b].status > 0 && back[b].status < 1000);
              
              // Si non déplacé, forcer type?
              if (!has_been_moved) {
                if (back[b].r.statuscode!=-10) {    // erreur
                  if (strnotempty(back[b].r.contenttype)==0)
                    strcpybuff(back[b].r.contenttype,"text/html");    // message d'erreur en html
                  // Finalement on, renvoie un erreur, pour ne toucher à rien dans le code
                  // libérer emplacement backing
                  /*if (opt->log!=NULL) {
                    fspc(opt->log,0); fprintf(opt->log,"Error: (during prefetch) %s (%d) to link %s at %s%s"LF,back[b].r.msg,back[b].r.statuscode,back[b].r.location,curr_adr,curr_fil);
                    test_flush;
                  }                    
                  back_delete(opt,cache,sback,b);
                  return -1;        // ERREUR (404 par exemple)
                  */
                } 
                
                {            // pas d'erreur, changer type?
                  char s[16];
                  s[0]='\0';
                  if (strnotempty(back[b].r.cdispo)) {        /* filename given */
                    ext_chg=2;      /* change filename */
                    strcpybuff(ext,back[b].r.cdispo);
                  }
                  else if (!may_unknown2(opt, back[b].r.contenttype, back[b].url_fil) || ishtest == -2 ) {  // on peut patcher à priori? (pas interdit ou pas de type)
                    give_mimext(s,back[b].r.contenttype);  // obtenir extension
                    if (strnotempty(s)>0) {    // on a reconnu l'extension
                      ext_chg=1;
                      strcpybuff(ext,s);
                    }
                  }
                }
              }
              // FIN Si non déplacé, forcer type?

              // libérer emplacement backing
              back_maydelete(opt,cache,sback,b);
              
              // --- --- ---
              // oops, a été déplacé.. on recalcule en récursif (osons!)
              if (has_been_moved) {
                // copier adr, fil (optionnel, mais sinon marche pas pour le rip)
                strcpybuff(adr_complete,curr_adr);
                strcpybuff(fil_complete,curr_fil);
                // copier adr, fil
                
                return url_savename(curr_adr,curr_fil,save,NULL,NULL,referer_adr,referer_fil,opt,liens,lien_tot,sback,cache,hash,ptr,numero_passe,NULL);
              }
              // --- --- ---
              
            }
            
          } else {
            printf("PANIC! : Savename Crash adding error, unexpected error found.. [%d]\n",__LINE__);
#if BDEBUG==1
            printf("error while savename crash adding\n");
#endif
            if (opt->log) {
              HTS_LOG(opt,LOG_ERROR); fprintf(opt->log,"Unexpected savename backing error at %s%s"LF,adr,fil_complete);
              test_flush;
            } 
            
          }
          // restaurer
          opt->state._hts_in_html_parsing=hihp;
        }  // caché?
      }
    }
  }



  // - - - DEBUT NOMMAGE - - -

  // Donner nom par défaut?
  if (fil[strlen(fil)-1]=='/')  {
    if (!strfield(adr_complete,"ftp://")
#if HTS_USEMMS
			&& !strfield(adr_complete,"mms://")
#endif
			)
		{
      strcatbuff(fil,DEFAULT_HTML);     // nommer page par défaut!!
#if HTS_USEMMS
		} else if (strfield(adr_complete,"mms://")) {
			strcatbuff(fil,DEFAULT_MMS);
#endif
		} else {
      if (!opt->proxy.active)
        strcatbuff(fil,DEFAULT_FTP);     // nommer page par défaut (texte)
      else
        strcatbuff(fil,DEFAULT_HTML);     // nommer page par défaut (à priori ici html depuis un proxy http)
    }
  }
  // Changer extension?
  // par exemple, php3 sera sauvé en html, cgi en html ou gif, xbm etc.. selon les cas
  if (ext_chg) {  // changer ext
    char* a=fil+strlen(fil)-1;
    if ( (opt->debug>1) && (opt->log!=NULL) ) {
      HTS_LOG(opt,LOG_DEBUG);
      if (ext_chg==1)
        fprintf(opt->log,"Changing link extension %s%s to .%s"LF,adr_complete,fil_complete,ext);
      else
        fprintf(opt->log,"Changing link name %s%s to %s"LF,adr_complete,fil_complete,ext);
      test_flush;
    }
    if (ext_chg==1) {
      while((a > fil) && (*a!='.') && (*a!='/')) a--;
      if (*a=='.') *a='\0';  // couper
      strcatbuff(fil,".");      // recopier point
    } else {
      while(( a > fil) && (*a!='/')) a--;
      if (*a=='/') a++;
      *a='\0';
   }
    strcatbuff(fil,ext);    // copier ext/nom
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
    const char* a = StringBuff(opt->savename_userdef);
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
          case '[':            // %[param:prefix_if_not_empty:suffix_if_not_empty:empty_replacement:notfound_replacement]
            if (strchr(a,']')) {
              int pos=0;
              char name[5][256];
              char* c=name[0];
              for(pos = 0 ; pos < 5 ; pos++) {
                name[pos][0]='\0';
              }
              pos=0;
              while(*a!=']') {
                if (pos < 5) {
                  if (*a == ':') {  // next token
                    c=name[++pos];
                    a++;
                  } else {
                    *c++=*a++;
                    *c='\0';
                  }
                }
              }
              a++;
              strcatbuff(name[0],"=");           /* param=.. */
              c=strchr(fil_complete,'?');
              /* parameters exists */
              if (c) {
                char* cp;
                while((cp = strstr(c+1, name[0])) && *(cp-1) != '?' && *(cp-1) != '&') {     /* finds [?&]param= */
                  c = cp;
                }
                if (cp) {
                  c = cp + strlen(name[0]);    /* jumps "param=" */
                  strcpybuff(b, name[1]);    /* prefix */
                  b += strlen(b);
                  if (*c != '\0' && *c != '&') {
                    char* d = name[0];
                    /* */
                    while(*c != '\0' && *c != '&') {
                      *d++ = *c++;
                    }
                    *d = '\0';
                    d = unescape_http(catbuff,name[0]);
                    if (d && *d) {
                      strcpybuff(b, d);         /* value */
                      b += strlen(b);
                    } else {
                      strcpybuff(b, name[3]);    /* empty replacement if any */
                      b += strlen(b);
                    }
                  } else {
                    strcpybuff(b, name[3]);    /* empty replacement if any */
                    b += strlen(b);
                  }
                  strcpybuff(b, name[2]);    /* suffix */
                  b += strlen(b);
                } else {
                  strcpybuff(b, name[4]);    /* not found replacement if any */
                  b += strlen(b);
                }
							} else {
								strcpybuff(b, name[4]);    /* not found replacement if any */
								b += strlen(b);
							}
            }
          break;
          case '%': *b++='%'; break;
          case 'n':    // nom sans ext
            *b='\0';
            if (dot_pos) {
              if (!short_ver)    // Noms longs
                strncatbuff(b,nom_pos,(int) (dot_pos - nom_pos));
              else
                strncatbuff(b,nom_pos,min((int) (dot_pos - nom_pos),8));
            } else {
              if (!short_ver)    // Noms longs
                strcpybuff(b,nom_pos);
              else
                strncatbuff(b,nom_pos,8);
            }
            b+=strlen(b);   // pointer à la fin
            break;
          case 'N':    // nom avec ext
            // RECOPIE NOM + EXT
            *b='\0';
            if (dot_pos) {
              if (!short_ver)    // Noms longs
                strncatbuff(b,nom_pos,(int) (dot_pos - nom_pos));
              else
                strncatbuff(b,nom_pos,min((int) (dot_pos - nom_pos),8));
            } else {
              if (!short_ver)    // Noms longs
                strcpybuff(b,nom_pos);
              else
                strncatbuff(b,nom_pos,8);
            }
            b+=strlen(b);   // pointer à la fin
            // RECOPIE NOM + EXT
            *b='\0';
            if (dot_pos) {
              if (!short_ver)    // Noms longs
                strcpybuff(b,dot_pos+1);
              else
                strncatbuff(b,dot_pos+1,3);
            } else {
              if (!short_ver)    // Noms longs
                strcpybuff(b,DEFAULT_EXT + 1);    // pas de..
              else
                strcpybuff(b,DEFAULT_EXT_SHORT + 1);    // pas de..
            }
            b+=strlen(b);   // pointer à la fin
            //
            break;
          case 't':    // ext
            *b='\0';
            if (dot_pos) {
              if (!short_ver)    // Noms longs
                strcpybuff(b,dot_pos+1);
              else
                strncatbuff(b,dot_pos+1,3);
            } else {
              if (!short_ver)    // Noms longs
                strcpybuff(b,DEFAULT_EXT + 1);    // pas de..
              else
                strcpybuff(b,DEFAULT_EXT_SHORT + 1);    // pas de..
            }
            b+=strlen(b);   // pointer à la fin
            break;
          case 'p':    // path sans dernier /
            *b='\0';
            if (nom_pos != fil + 1) { // pas: /index.html (chemin nul)
              if (!short_ver) {   // Noms longs
                strncatbuff(b,fil,(int) (nom_pos - fil) - 1);
              } else {
                char BIGSTK pth[HTS_URLMAXSIZE*2],n83[HTS_URLMAXSIZE*2];
                pth[0]=n83[0]='\0';
                //
                strncatbuff(pth,fil,(int) (nom_pos - fil) - 1);
                long_to_83(opt->savename_83,n83,pth);
                strcpybuff(b,n83);
              }
            }
            b+=strlen(b);   // pointer à la fin
            break;
          case 'h':    // host
            *b='\0';
            if (strcmp(adr_complete,"file://")==0) {
              if (!short_ver)    // Noms longs
                strcpybuff(b,"localhost");
              else
                strcpybuff(b,"local");
            } else {
              if (!short_ver)    // Noms longs
                strcpybuff(b,print_adr);
              else
                strncatbuff(b,print_adr,8);
            }
            b+=strlen(b);   // pointer à la fin
            break;
          case 'M':         /* host/address?query MD5 (128-bits) */
            *b='\0';
            {
              char digest[32+2];
              char BIGSTK buff[HTS_URLMAXSIZE*2];
              digest[0]=buff[0]='\0';
              strcpybuff(buff,adr);
              strcatbuff(buff,fil_complete);
              domd5mem(buff,strlen(buff),digest,1);
              strcpybuff(b,digest);
            }
            b+=strlen(b);   // pointer à la fin
            break;
          case 'Q': case 'q':         /* query MD5 (128-bits/16-bits) 
                                         GENERATED ONLY IF query string exists! */
						{
							char md5[32 + 2];
							*b='\0';
							strncatbuff(b,url_md5(md5, fil_complete),(tok == 'Q')?32:4);
							b+=strlen(b);   // pointer à la fin
						}
            break;
		  case 'r': case 'R':		// protocol
            *b='\0';
            strcatbuff(b, protocol_str[protocol]);
            b+=strlen(b);   // pointer à la fin
		  break;

      /* Patch by Juan Fco Rodriguez to get the full query string */
      case 'k':
        {
          char *d = strchr(fil_complete,'?');
          if( d != NULL )
          {
            strcatbuff(b, d);
            b+=strlen(b);
          }
        }
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
            strcatbuff(save,"localhost");
          else
            strcatbuff(save,"local");
        } else {
          // adresse url
          if (!opt->savename_83) {  // noms longs (et pas de .)
            strcatbuff(save,print_adr);
          } else {  // noms 8-3
            if (strlen(print_adr)>4) {
              if (strfield(print_adr,"www."))
                strncatbuff(save,print_adr+4,max_char);
              else
                strncatbuff(save,print_adr,8);
            } else strncatbuff(save,print_adr,max_char);
          }
        }
        if (*fil!='/') strcatbuff(save,"/");
      }
    }
  
    hts_lowcase(save);
        
    /*
    // ne sert à rien car a déja été filtré normalement
    if ((*fil=='.') && (*(fil+1)=='/'))          // ./index.html ** //
      url_savename_addstr(save,fil+2);
    else                                               // index.html ou /index.html
      url_savename_addstr(save,fil);
    if (save[strlen(save)-1]=='/') 
      strcatbuff(save,DEFAULT_HTML);     // nommer page par défaut!!
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
            strcatbuff(save,"localhost/");
          else
            strcatbuff(save,"local/");
        } else {
          // adresse url
          if (!opt->savename_83) {  // noms longs
            strcatbuff(save,print_adr); strcatbuff(save,"/");
          } else {  // noms 8-3
            if (strlen(print_adr)>4) {
              if (strfield(print_adr,"www."))
                strncatbuff(save,print_adr+4,max_char);
              else
                strncatbuff(save,print_adr,max_char);
              strcatbuff(save,"/");
            } else { 
              strncatbuff(save,print_adr,max_char); strcatbuff(save,"/");
            }
          }
        }
      } else {
        strcatbuff(save,"web/");    // répertoire général
      }
    } 

    // si un html à coup sûr
    if ( (ext_chg!=0) ? (ishtml_ext(ext) == 1) : (ishtml(opt,fil)==1) ) {
      if (opt->savename_type%100==2) {  // html/
        strcatbuff(save, "html/");
      }
    } else {
      if ((opt->savename_type%100==1) || (opt->savename_type%100==2)) {  // html & images
        strcatbuff(save, "images/");
      }
    }
    
    switch (opt->savename_type%100) {
    case 4: case 5: {           // séparer par types
      char* a=fil+strlen(fil)-1;
      // passer structures
      while(( a > fil) && (*a != '/') && (*a != '\\')) a--;      
      if ((*a=='/') || (*a=='\\')) a++;

      // html?
      if ( (ext_chg!=0) ? (ishtml_ext(ext)==1) : (ishtml(opt,fil)==1) ) {
        if (opt->savename_type%100==5)
          strcatbuff(save,"html/");
      } else {
        char* a=fil+strlen(fil)-1;
        while(( a> fil) && (*a != '/') && (*a != '.')) a--;      
        if (*a!='.')
          strcatbuff(save,"other");
        else
          strcatbuff(save,a+1);
        strcatbuff(save,"/");
      }
      /*strcatbuff(save,a);*/
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
      L = (int) strlen(C);
      for(i=0;i<(int) strlen(fil_complete);i++) {
        s+=(unsigned int) fil_complete[i];
      }
      for(i=0;i<(int) strlen(adr_complete);i++) {
        s+=(unsigned int) adr_complete[i];
      }
      srand(s);
      
      j = (int) strlen(save);
      for(i=0;i<8;i++) {
        char c=C[(rand()%L)];
        save[i+j]=c;
      }
      save[i+j]='\0';
      // ajouter extension
      a = fil + strlen(fil) - 1;
      while(( a > fil) && (*a != '/') && (*a != '.')) a--;
      if (*a=='.') {
        strcatbuff(save,a);    // ajouter
      }
             } 
      break;
    default: {   // noms sans les noms des répertoires
      // ne garder que le nom, pas la structure
      /*
      char* a=fil+strlen(fil)-1;
      while(((int) a>(int) fil) && (*a != '/') && (*a != '\\')) a--;      
      if ((*a=='/') || (*a=='\\')) a++;
      strcatbuff(save,a);
      */

      /* add name */
      ADD_STANDARD_NAME(0);
            }
      break;
    }

    hts_lowcase(save);

    if (save[strlen(save)-1]=='/') 
      strcatbuff(save,DEFAULT_HTML);     // nommer page par défaut!!
  }


  // vérifier qu'on ne doit pas forcer l'extension
  // par exemple, asp sera sauvé en html, cgi en html ou gif, xbm etc.. selon les cas
  /*if (ext_chg) {
    char* a=save+strlen(save)-1;
    while(((int) a>(int) save) && (*a!='.') && (*a!='/')) a--;
    if (*a=='.') *a='\0';  // couper
    // recopier extension
    strcatbuff(save,".");
    strcatbuff(save,ext);    // copier ext
  }*/
  // de même en cas de manque d'extension on en place une de manière forcée..
  // cela évite les /chez/toto et les /chez/toto/index.html incompatibles
  if (opt->savename_type != -1) {
    char* a=save+strlen(save)-1;
    while(( a > save) && (*a!='.') && (*a!='/')) a--;
    if (*a!='.') {   // agh pas de point
      //strcatbuff(save,".none");                 // a éviter
      strcatbuff(save,".html");                   // préférable!
      if ( (opt->debug>1) && (opt->log!=NULL) ) {
        HTS_LOG(opt,LOG_WARNING); fprintf(opt->log,"Default HTML type set for %s%s"LF,adr_complete,fil_complete);
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
      char BIGSTK tempo[HTS_URLMAXSIZE*2];
      char *b;
      tempo[0]='\0';
      strcpybuff(tempo,"[");
      b=strchr(save,':');
      if (!b) b=strchr(save,'@');
      if (b)
        strncatbuff(tempo,save,(int) b-(int) a);
      strcatbuff(tempo,"]");
      strcatbuff(tempo,a);
      strcpybuff(save,a);
    }
  }
*/

  // éviter les / au début (cause: N100)
  if (save[0]=='/') {
    char BIGSTK tempo[HTS_URLMAXSIZE*2];
    strcpybuff(tempo,save+1);
    strcpybuff(save,tempo);
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
  if (opt->savename_83 == 2) { // CDROM
    // maybe other ones?
    hts_replace(save,'-','_');
    hts_replace(save,'=','_');
    hts_replace(save,'+','_');
  }
  //
  { // éliminer les // (comme ftp://)
    char* a;
    //while( (a=strstr(save,"//")) ) *a='_';
    cleanDoubleSlash(save);
    // Eliminer chars spéciaux
    a=save -1 ;
    while(*(++a))
      if ( ((unsigned char)(*a) <= 31)
          || ((unsigned char)(*a) == 127) )
        *a='_';
  }

   
#if HTS_OVERRIDE_DOS_FOLDERS
  /* Replace /foo/nul/bar by /foo/nul_/bar */
  {
    int i=0;
    while(hts_tbdev[i][0]) {
      char* a=save;
      while((a=strstrcase(a,(char*)hts_tbdev[i]))) {
        switch ( (int) a[strlen(hts_tbdev[i])] ) {
        case '\0':
        case '/':
        case '.': 
        {
          char BIGSTK tempo[HTS_URLMAXSIZE*2]; tempo[0]='\0';
          strncatbuff(tempo,save,(int) (a - save) + strlen(hts_tbdev[i]));
          strcatbuff(tempo,"_");
          strcatbuff(tempo,a+strlen(hts_tbdev[i]));
          strcpybuff(save,tempo);
                   }
          break;
        }
        a+=strlen(hts_tbdev[i]);
      }
      i++;
    }
  }
  /* Strip ending . or ' ' forbidden on windoz */
  {
    int len;
    char* a=save;
    while((a=strstr(a,"./"))) {
      *a = '_';
    }
    a=save;
    while((a=strstr(a," /"))) {
      *a = '_';
    }
    len = (int) strlen(save);
    if (len > 0 && ( save[len - 1] == '.' || save[len - 1] == ' ') ) {
      save[len - 1] = '_';
    }
  }
#endif

  // conversion 8-3 .. y compris pour les répertoires
  if (opt->savename_83) {
    char BIGSTK n83[HTS_URLMAXSIZE*2];
    long_to_83(opt->savename_83,n83,save);
    strcpybuff(save,n83);
  }

  // enforce stricter ISO9660 compliance (bug reported by Steffo Carlsson)
  // Level 1 File names are restricted to 8 characters with a 3 character extension, 
  // upper case letters, numbers and underscore; maximum depth of directories is 8.
  // This will be our "DOS mode"
  // L2: 31 characters
  // A-Z,0-9,_
  if (opt->savename_83 > 0) {
    char *a, *last;
    for(last = save + strlen(save) - 1 ; last != save && *last != '/' && *last != '\\' && *last != '.' ; last--);
    if (*last != '.') {
      last = NULL;
    }
    for(a = save ; *a != '\0' ; a++) {
      if (*a >= 'a' && *a <= 'z') {
        *a -= 'a' - 'A';
      }
      else if (*a == '.') {
        if (a != last) {
          *a = '_';
        }
      }
      else if ( ! ( (*a >= 'A' && *a <= 'Z') || (*a >= '0' && *a <= '9') || *a == '_' || *a == '/' || *a == '\\') ) {
        *a = '_';
      }
    }
  }

  /* ensure that there is no ../ (potential vulnerability) */
  fil_simplifie(save);

  /* convert name to UTF-8 ? */
  if (charset != NULL && charset[0] != '\0') {
    char *const s = hts_convertStringToUTF8(save, (int) strlen(save), charset);
    if (s != NULL) {
      strcpy(save, s);
      free(s);
    }
  }

	/* callback */
  RUN_CALLBACK5(opt, savename, adr_complete,fil_complete,referer_adr,referer_fil,save);

  if ( (opt->debug>0) && (opt->log!=NULL) ) {
		HTS_LOG(opt,LOG_INFO); fprintf(opt->log,"engine: save-name: local name: %s%s -> %s"LF,adr,fil,save);
		test_flush;
	}

	/* Ensure that the MANDATORY "temporary" extension is set */
	if (ext_chg_delayed) {
		char *ptr;
		char *lastDot = NULL;
		for(ptr = save ; *ptr != 0 ; ptr++) {
			if (*ptr == '.') {
				lastDot = ptr;
			} else if (*ptr == '/' || *ptr == '\\') {
				lastDot = NULL;
			}
		}
		if (lastDot == NULL) {
			strcatbuff(save, "." DELAYED_EXT);
		} else if (!IS_DELAYED_EXT(save)) {
			strcatbuff(lastDot, "." DELAYED_EXT);
		}
	}

  // chemin primaire éventuel A METTRE AVANT
  if (strnotempty(StringBuff(opt->path_html_utf8))) {
    char BIGSTK tempo[HTS_URLMAXSIZE*2];
    strcpybuff(tempo,StringBuff(opt->path_html_utf8));
    strcatbuff(tempo,save);
    strcpybuff(save,tempo);
  }


  // vérifier que le nom n'est pas déja pris...
  if (liens!=NULL) { 
    int nom_ok;
    do {
      int i;
      //
      nom_ok=1;  // à priori bon
      // on part de la fin pour optimiser, plus les opti de taille pour aller encore plus vite..
#if DEBUG_SAVENAME
printf("\nStart search\n");
#endif

      i=hash_read(hash,save,"",0,0);      // lecture type 0 (sav)
      if (i>=0)
          {
            int sameAdr = ( strfield2(liens[i]->adr, normadr) != 0 );
            int sameFil;
						// NO - URL hack is only for stripping // and www.
            //if (opt->urlhack != 0)
            //  sameFil = ( strfield2(liens[i]->fil, normfil) != 0);
            //else
            sameFil = ( strcmp(liens[i]->fil, normfil) == 0);
            if (sameAdr && sameFil)
            {    // ok c'est le même lien, adresse déja définie
              /* Take the existing name not to screw up with cAsE sEnSiTiViTy of Linux/Unix */
              if (strcmp(liens[i]->sav, save) != 0) {
                strcpybuff(save, liens[i]->sav);
              }
              i=0;
#if DEBUG_SAVENAME
printf("\nOK ALREADY DEFINED\n",13,i);
#endif
            } else {  // utilisé par un AUTRE, changer de nom
              char BIGSTK tempo[HTS_URLMAXSIZE*2];
              char* a=save+strlen(save)-1;
              char* b;
              int n=2;       
              char collisionSeparator = ( (opt->savename_83 != 2) ? '-' : '_');
              tempo[0]='\0';

#if DEBUG_SAVENAME
printf("\nWRONG CASE UNMATCH : \n%s\n%s, REDEFINE\n",liens[i]->fil,fil_complete);
#endif
              nom_ok=0;
              i=0;
              
              while(( a > save) && (*a!='.') && (*a!='\\') && (*a!='/')) a--;
              if (*a=='.')
                strncatbuff(tempo,save,(int) (a - save));
              else
                strcatbuff(tempo,save);
              
              // tester la présence d'un -xx (ex: index-2.html -> index-3.html)
              b=tempo+strlen(tempo)-1;
              while (isdigit((unsigned char)*b)) b--;
              if (*b == collisionSeparator) {
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
              sprintf(tempo+strlen(tempo),"%c%d", collisionSeparator, n);
              
              // ajouter extension
              if (*a=='.')
                strcatbuff(tempo,a);
              
              strcpybuff(save,tempo);
              
              //printf("switched: %s\n",save);
              
            }  // if
          }
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
	char md5[32 + 2];

  b[0]='\0';
  /* Nom */
  if (dot_pos) {
    if (!short_ver)    // Noms longs
      strncatbuff(b,nom_pos,(int) (dot_pos - nom_pos));
    else
      strncatbuff(b,nom_pos,min((int) (dot_pos - nom_pos),8));
  } else {
    if (!short_ver)    // Noms longs
      strcatbuff(b,nom_pos);
    else
      strncatbuff(b,nom_pos,8);
  }
  /* MD5 - 16 bits */
  strncatbuff(b,url_md5(md5,fil_complete),4);
  /* Ext */
  if (dot_pos) {
    strcatbuff(b,".");
    if (!short_ver)    // Noms longs
      strcatbuff(b,dot_pos+1);
    else
      strncatbuff(b,dot_pos+1,3);
  } else {
    if (!short_ver)    // Noms longs
      strcatbuff(b,DEFAULT_EXT + 1);    // pas de..
    else
      strcatbuff(b,DEFAULT_EXT_SHORT + 1);    // pas de..
  }
}


/* Petit md5 */
char* url_md5(char* digest, char* fil_complete) {
  char* a;
  digest[0]='\0';
  a=strchr(fil_complete,'?');
  if (a) {
    if (strlen(a)) {
      char BIGSTK buff[HTS_URLMAXSIZE*2];
      a++;
      digest[0]=buff[0]='\0';
      strcatbuff(buff,a);         /* query string MD5 */
      domd5mem(buff,strlen(buff),digest,1);
    }
  }
  return digest;
}

// interne à url_savename: ajoute une chaîne à une autre avec \ -> /
void url_savename_addstr(char* d,char* s) {
  int i = (int) strlen(d);
  while(*s) {
    if (*s=='\\')  // remplacer \ par des /
      d[i++]='/';
    else
      d[i++]=*s;
    s++;
  }
  d[i]='\0';
}

/* "filename" should be at least 64 bytes. */
void url_savename_refname(const char *adr, const char *fil, char *filename) {
  unsigned char bindigest[16];
  MD5_CTX ctx;
  MD5Init(&ctx, 0);
  MD5Update(&ctx, adr, (unsigned int) strlen(adr));
  MD5Update(&ctx, ",", 1);
  MD5Update(&ctx, fil, (unsigned int) strlen(fil));
  MD5Final(bindigest, &ctx);
  sprintf(filename, CACHE_REFNAME "/"
    "%02x%02x%02x%02x%02x%02x%02x%02x"
    "%02x%02x%02x%02x%02x%02x%02x%02x"
    ".ref",
    bindigest[0],  bindigest[1],  bindigest[2],  bindigest[3],
    bindigest[4],  bindigest[5],  bindigest[6],  bindigest[7],
    bindigest[8],  bindigest[9],  bindigest[10], bindigest[11],
    bindigest[12], bindigest[13], bindigest[14], bindigest[15]);
}

/* note: return a local filename */
char *url_savename_refname_fullpath(httrackp* opt, const char *adr, const char *fil) {
  char digest_filename[64];
  url_savename_refname(adr, fil, digest_filename);
  return fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log), digest_filename);
}

/* remove refname if any */
void url_savename_refname_remove(httrackp* opt, const char *adr, const char *fil) {
  char *filename = url_savename_refname_fullpath(opt, adr, fil);
  (void) UNLINK(filename);
}

#undef test_flush
