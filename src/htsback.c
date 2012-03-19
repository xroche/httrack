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
/*       backing system (multiple socket download)              */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htsback.h"

/* specific definitions */
#include "htsbase.h"
#include "htsnet.h"
#include "htsthread.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
/* END specific definitions */

//#if HTS_WIN
#include "htsftp.h"
#if HTS_USEZLIB
#include "htszlib.h"
#endif
//#endif

#if HTS_WIN
#ifndef __cplusplus
// DOS
#include <process.h>    /* _beginthread, _endthread */
#endif
#else
#endif

#undef test_flush
#define test_flush if (opt->flush) { if (opt->log) { fflush(opt->log); } if (opt->errlog) { fflush(opt->errlog);  } }

#define VT_CLREOL       "\33[K"


// ---
// routines de backing
// retourne l'index d'un lien dans un tableau de backing
int back_index(lien_back* back,int back_max,char* adr,char* fil,char* sav) {
  int i=0;
  int index=-1;
  while( i<back_max ) {
    if (back[i].status>=0)    // réception OU prêt
      if (strfield2(back[i].url_adr,adr)) {
        if (strcmp(back[i].url_fil,fil)==0) {
          if (index==-1)    /* first time we meet, store it */
            index=i;
          else if (strcmp(back[i].url_sav,sav)==0) {  /* oops, check sav too */
            index=i;
            return index;
          }
        }
      }
    i++;
  }
  return index;
}

// nombre d'entrées libres dans le backing
int back_available(lien_back* back,int back_max) {
  int i;
  int nb=0;
  for(i=0;i<back_max;i++)
    if (back[i].status==-1)     /* libre */
      nb++;
  return nb;
}

// retourne estimation de la taille des html et fichiers stockés en mémoire
LLint back_incache(lien_back* back,int back_max) {
  int i;
  LLint sum=0;
  for(i=0;i<back_max;i++)
    if (back[i].status!=-1)
      if (back[i].r.adr)       // ne comptabilier que les blocs en mémoire
        sum+=max(back[i].r.size,back[i].r.totalsize);
  return sum;
}

// le lien a-t-il été mis en backing?
HTS_INLINE int back_exist(lien_back* back,int back_max,char* adr,char* fil,char* sav) {
  return (back_index(back,back_max,adr,fil,sav)>=0);
}

// nombre de sockets en tâche de fond
int back_nsoc(lien_back* back,int back_max) {
  int n=0;
  int i;
  for(i=0;i<back_max;i++)
    if (back[i].status > 0)    // only receive
      n++;

  return n;
}
int back_nsoc_overall(lien_back* back,int back_max) {
  int n=0;
  int i;
  for(i=0;i<back_max;i++)
    if (back[i].status > 0 || back[i].status == -103)
      n++;

  return n;
}

// objet (lien) téléchargé ou transféré depuis le cache
//
// fermer les paramètres de transfert,
// et notamment vérifier les fichiers compressés (décompresser), callback etc.
int back_finalize(httrackp* opt,cache_back* cache,lien_back* back,int p) {
  if (
      (back[p].status == 0)      // ready
      &&
      (!back[p].testmode)        // not test mode
      &&
      (back[p].r.statuscode>0)   // not internal error
      ) {
    char* state="unknown";
   
    /* décompression */
#if HTS_USEZLIB
    if (gz_is_available && back[p].r.compressed) {
      if (back[p].r.size > 0) {
        //if ( (back[p].r.adr) && (back[p].r.size>0) ) {
        // stats
        back[p].compressed_size=back[p].r.size;
        // en mémoire -> passage sur disque
        if (!back[p].r.is_write) {
          back[p].tmpfile_buffer[0]='\0';
          back[p].tmpfile=tmpnam(back[p].tmpfile_buffer);
          if (back[p].tmpfile != NULL && back[p].tmpfile[0] != '\0') {
            back[p].r.out=fopen(back[p].tmpfile,"wb");
            if (back[p].r.out) {
              if ((back[p].r.adr) && (back[p].r.size>0)) {
                if (fwrite(back[p].r.adr,1,(INTsys)back[p].r.size,back[p].r.out) != back[p].r.size) {
                  back[p].r.statuscode=-1;
                  strcpybuff(back[p].r.msg,"Write error when decompressing");
                }
              } else {
                back[p].tmpfile[0]='\0';
                back[p].r.statuscode=-1;
                strcpybuff(back[p].r.msg,"Empty compressed file");
              }
            } else {
              back[p].tmpfile[0]='\0';
              back[p].r.statuscode=-1;
              strcpybuff(back[p].r.msg,"Open error when decompressing");
            }
          }
        }
        // fermer fichier sortie
        if (back[p].r.out!=NULL) {
          fclose(back[p].r.out);
          back[p].r.out=NULL;
        }
        // décompression
        if (back[p].tmpfile != NULL && back[p].tmpfile[0] != '\0' && back[p].url_sav[0]) {
          LLint size;
          filecreateempty(back[p].url_sav);      // filenote & co
          if ((size = hts_zunpack(back[p].tmpfile,back[p].url_sav))>=0) {
            back[p].r.size=back[p].r.totalsize=size;
            // fichier -> mémoire
            if (!back[p].r.is_write) {
              deleteaddr(&back[p].r);
              back[p].r.adr=readfile(back[p].url_sav);
              if (!back[p].r.adr) {
                back[p].r.statuscode=-1;
                strcpybuff(back[p].r.msg,"Read error when decompressing");
              }
              remove(back[p].url_sav);
            }
          }
          remove(back[p].tmpfile);
        }
        // stats
        HTS_STAT.total_packed+=back[p].compressed_size;
        HTS_STAT.total_unpacked+=back[p].r.size;
        HTS_STAT.total_packedfiles++;
        // unflag
      }
    }
    back[p].r.compressed=0;
#endif
    
    /* Stats */
    if (cache->txt) {
      char flags[32];
      char s[256];
      time_t tt;
      struct tm* A;
      tt=time(NULL);
      A=localtime(&tt);
      if (A == NULL) {
        int localtime_returned_null=0;
        assert(localtime_returned_null);
      }
      strftime(s,250,"%H:%M:%S",A);
      
      flags[0]='\0';
      /* input flags */
      if (back[p].is_update)
        strcatbuff(flags, "U");   // update request
      else
        strcatbuff(flags, "-");
      if (back[p].range_req_size)
        strcatbuff(flags, "R");   // range request
      else
        strcatbuff(flags, "-");
      /* state flags */
      if (back[p].r.is_file)  // direct to disk
        strcatbuff(flags, "F");
      else
        strcatbuff(flags, "-");
      /* output flags */
      if (!back[p].r.notmodified)
        strcatbuff(flags, "M");   // modified
      else
        strcatbuff(flags, "-");
      if (back[p].r.is_chunk)  // chunked
        strcatbuff(flags, "C");
      else
        strcatbuff(flags, "-");
      if (back[p].r.compressed)
        strcatbuff(flags, "Z");   // gzip
      else
        strcatbuff(flags, "-");
      /* Err I had to split these.. */
      fprintf(cache->txt,"%s\t", s);
      fprintf(cache->txt,LLintP"/", (LLint)back[p].r.size);
      fprintf(cache->txt,LLintP,(LLint)back[p].r.totalsize);
      fprintf(cache->txt,"\t%s\t",flags);
    }
    if (back[p].r.statuscode==200) {
      if (back[p].r.size>=0) {
        if (strcmp(back[p].url_fil,"/robots.txt") !=0 ) {
          HTS_STAT.stat_bytes+=back[p].r.size;
          HTS_STAT.stat_files++;
        }
        if ( (!back[p].r.notmodified) && (opt->is_update) ) { 
          HTS_STAT.stat_updated_files++;       // page modifiée
          if (opt->log!=NULL) {
            fspc(opt->log,"info");
            if (back[p].is_update) {
              fprintf(opt->log,"engine: transfer-status: link updated: %s%s -> %s"LF,back[p].url_adr,back[p].url_fil,back[p].url_sav);
            } else {
              fprintf(opt->log,"engine: transfer-status: link added: %s%s -> %s"LF,back[p].url_adr,back[p].url_fil,back[p].url_sav);
            }
            test_flush;
          }
          if (cache->txt) {
            if (back[p].is_update) {
              state="updated";
            } else {
              state="added";
            }
          }
        } else {
          if ( (opt->debug>0) && (opt->log!=NULL) ) {
            fspc(opt->log,"info"); fprintf(opt->log,"engine: transfer-status: link recorded: %s%s -> %s"LF,back[p].url_adr,back[p].url_fil,back[p].url_sav);
            test_flush;
          }
          if (cache->txt) {
            if (opt->is_update)
              state="untouched";
            else
              state="added";
          }
        }
      } else {
        if ( (opt->debug>0) && (opt->log!=NULL) ) {
          fspc(opt->log,"info"); fprintf(opt->log,"engine: transfer-status: empty file? (%d, '%s'): %s%s"LF,back[p].r.statuscode,back[p].r.msg,back[p].url_adr,back[p].url_fil);
          test_flush;
        }
        if (cache->txt) {
          state="empty";
        }
      }
    } else {
      if ( (opt->debug>0) && (opt->log!=NULL) ) {
        fspc(opt->log,"info"); fprintf(opt->log,"engine: transfer-status: link error (%d, '%s'): %s%s"LF,back[p].r.statuscode,back[p].r.msg,back[p].url_adr,back[p].url_fil);
      }
      if (cache->txt) {
        state="error";
      }
    }
    if (cache->txt) {
      fprintf(cache->txt,
        "%d\t"
        "%s ('%s')\t"
        "%s\t"
        "%s%s\t"
        "%s%s\t%s\t"
        "(from %s%s)"
        LF,
        back[p].r.statuscode,
        state, escape_check_url_addr(back[p].r.msg),
        escape_check_url_addr(back[p].r.contenttype),
        ((back[p].r.etag[0])?"etag:":((back[p].r.lastmodified[0])?"date:":"")), escape_check_url_addr((back[p].r.etag[0])?back[p].r.etag:(back[p].r.lastmodified)),
        escape_check_url_addr(back[p].url_adr),escape_check_url_addr(back[p].url_fil),escape_check_url_addr(back[p].url_sav),
        escape_check_url_addr(back[p].referer_adr),escape_check_url_addr(back[p].referer_fil)
        );
      if (opt->flush)
        fflush(cache->txt);
    }
    
    /* Cache */
    cache_mayadd(opt,cache,&back[p].r,back[p].url_adr,back[p].url_fil,back[p].url_sav);

    // status finished callback
#if HTS_ANALYSTE
    hts_htmlcheck_xfrstatus(&back[p]);
#endif
    return 0;
  }
  return -1;
}

/* try to keep the connection alive */
int back_letlive(httrackp* opt, lien_back* back, int p) {
  htsblk* src = &back[p].r;
  if (src && !src->is_file 
    && src->soc != INVALID_SOCKET
    && src->statuscode >= 0           /* no timeout errors & co */
    && src->keep_alive_trailers == 0  /* not yet supported (chunk trailers) */
    && !check_sockerror(src->soc)
    /*&& !check_sockdata(src->soc)*/     /* no unexpected data */
    ) {
    htsblk tmp;
    memset(&tmp, 0, sizeof(tmp));
    /* clear everything but connection: switch, close, and reswitch */
    back_connxfr(src, &tmp);
    back_delete(opt, back, p);
    //deletehttp(src);
    back_connxfr(&tmp, src);
    src->req.flush_garbage=1;     /* ignore CRLF garbage */
    return 1;
  }
  return 0;
}

void back_connxfr(htsblk* src, htsblk* dst) {
  dst->soc = src->soc;
  src->soc = INVALID_SOCKET;
#if HTS_USEOPENSSL
  dst->ssl = src->ssl;
  src->ssl = 0;
  dst->ssl_con = src->ssl_con;
  src->ssl_con = NULL;
#endif
  dst->keep_alive = src->keep_alive;
  src->keep_alive = 0;
  dst->keep_alive_max = src->keep_alive_max;
  src->keep_alive_max = 0;
  dst->keep_alive_t = src->keep_alive_t;
  src->keep_alive_t = 0;
  dst->debugid = src->debugid;
  src->debugid = 0;
}

// clear, or leave for keep-alive
int back_maydelete(httrackp* opt,lien_back* back, int p) {
  if (p>=0) {    // on sait jamais..
    if (!opt->nokeepalive
      && back[p].r.keep_alive 
      && back[p].r.keep_alive_max > 1
      && back[p].ka_time_start 
      && time_local() < back[p].ka_time_start + back[p].r.keep_alive_t
      ) {
      lien_back tmp;
      strcpybuff(tmp.url_adr, back[p].url_adr);
      if (back_letlive(opt, back, p)) {
        strcpybuff(back[p].url_adr, tmp.url_adr);
        back[p].status = -103;  // alive & waiting
        if ((opt->debug>1) && (opt->log!=NULL)) {
          fspc(opt->log,"debug"); fprintf(opt->log,"(Keep-Alive): successfully saved #%d (%s)"LF, 
            back[p].r.debugid,
            back[p].url_adr); test_flush;
        }
        return 1;
      }
    }
    back_delete(opt,back, p);
  }
  return 0;
}

// clear, or leave for keep-alive
void back_maydeletehttp(httrackp* opt, lien_back* back, int back_max, int p) {
  if (back[p].r.soc!=INVALID_SOCKET) {
    int q;
    if (!opt->nokeepalive
      && back[p].r.keep_alive 
      && back[p].r.keep_alive_max > 1
      && back[p].ka_time_start 
      && time_local() < back[p].ka_time_start + back[p].r.keep_alive_t
      && ( q = back_search(opt, back, back_max) ) >= 0
      ) 
    {
      lien_back tmp;
      strcpybuff(tmp.url_adr, back[p].url_adr);
      deletehttp(&back[q].r);               // security check
      back_connxfr(&back[p].r, &back[q].r); // transfer live connection settings from p to q
      back[q].ka_time_start = back[p].ka_time_start;  // refresh
      back[p].r.soc = INVALID_SOCKET;
      strcpybuff(back[q].url_adr, tmp.url_adr); // address
      back[q].status = -103;  // alive & waiting
      if ((opt->debug>1) && (opt->log!=NULL)) {
        fspc(opt->log,"debug"); fprintf(opt->log,"(Keep-Alive): successfully preserved #%d (%s)"LF, 
          back[q].r.debugid,
          back[q].url_adr); test_flush;
      }
    } else {
      deletehttp(&back[p].r);
      back[p].r.soc = INVALID_SOCKET;
    }
  }
}


/* attempt to attach a live connection to this slot */
int back_trylive(httrackp* opt,lien_back* back, int back_max, int p) {
  if (p>=0 && back[p].status != -103) {     // we never know..
    int i = back_searchlive(opt,back, back_max, back[p].url_adr);   // search slot
    if (i >= 0 && i != p) {
      deletehttp(&back[p].r);               // security check
      back_connxfr(&back[i].r, &back[p].r); // transfer live connection settings from i to p
      back_delete(opt,back, i);                 // delete old slot
      back[p].status=100;                   // ready to connect
      return 1;                             // success: will reuse live connection
    }
  }
  return 0;
}

/* search for a live position, or, if not possible, try to return a new one */
int back_searchlive(httrackp* opt, lien_back* back, int back_max, char* search_addr) {
  int i;

  /* search for a live socket */
  for(i = 0 ; i < back_max ; i++ ) {
    if (back[i].status == -103) {
      if (strfield2(back[i].url_adr, search_addr)) {   /* same location (xxc: check also virtual hosts?) */
        if (time_local() < back[i].ka_time_start + back[i].r.keep_alive_t) {
          return i;
        }
      }
    }
  }
  return -1;
}
  
int back_search(httrackp* opt,lien_back* back, int back_max) {
  int i;

  /* try to find an empty place */
  for(i = 0 ; i < back_max ; i++ ) {
    if (back[i].status == -1) {
      return i;
    }
  }

  /* couldn't find an empty place, try to requisition a keep-alive place */
  for(i = 0 ; i < back_max ; i++ ) {
    if (back[i].status == -103) {
      /* close this place */
      back_delete(opt,back, i);
      return i;
    }
  }

  /* oops, can't find a place */
  return -1;
}

// effacer entrée
int back_delete(httrackp* opt, lien_back* back, int p) {
  if (p>=0) {    // on sait jamais..
    // Vérificateur d'intégrité
    #if DEBUG_CHECKINT
    _CHECKINT(&back[p],"Appel back_delete")
    #endif
#if HTS_DEBUG_CLOSESOCK
    char info[256];
    sprintf(info,"back_delete: #%d\n",p);
    DEBUG_W2(info);
#endif

    // Libérer tous les sockets, handles, buffers..
    if (back[p].r.soc!=INVALID_SOCKET) {
#if HTS_DEBUG_CLOSESOCK
      DEBUG_W("back_delete: deletehttp\n");
#endif
      deletehttp(&back[p].r);
      back[p].r.soc=INVALID_SOCKET;
    }
    
    if (back[p].r.adr!=NULL) {  // reste un bloc à désallouer
      freet(back[p].r.adr);
      back[p].r.adr=NULL;
    }
    if (back[p].chunk_adr!=NULL) {  // reste un bloc à désallouer
      freet(back[p].chunk_adr);
      back[p].chunk_adr=NULL;
      back[p].chunk_size=0;
      back[p].chunk_blocksize=0;
      back[p].is_chunk=0;
    }
    // if (back[p].r.is_file) {  // fermer fichier entrée
    if (back[p].r.fp!=NULL) {
      fclose(back[p].r.fp);
      back[p].r.fp=NULL;
    }
    // }

    /* fichier de sortie */
    if (back[p].r.out!=NULL) {  // fermer fichier sortie
      fclose(back[p].r.out);
      back[p].r.out=NULL;
    }

    if (back[p].r.is_write) {     // ecriture directe
      /* écrire date "remote" */
      if (strnotempty(back[p].url_sav))          // normalement existe si on a un fichier de sortie
      if (strnotempty(back[p].r.lastmodified))   // last-modified existe
      if (fexist(back[p].url_sav))          // ainsi que le fichier
        set_filetime_rfc822(back[p].url_sav,back[p].r.lastmodified);

      /* executer commande utilisateur après chargement du fichier */
      //xx usercommand(opt,0,NULL,back[p].url_sav, back[p].url_adr, back[p].url_fil);
      back[p].r.is_write=0;
    }
    
    // Tout nettoyer
    memset(&back[p], 0, sizeof(lien_back));  
    back[p].r.soc=INVALID_SOCKET; back[p].r.location=back[p].location_buffer;
    
    // Le plus important: libérer le champ
    back[p].status=-1;
  }
  return 0;
}

/* Space left on backing stack */
int back_stack_available(lien_back* back,int back_max) {
  int p=0,n=0;
  for( ; p < back_max ; p++ )
    if ( back[p].status == -1 )
      n++;
  return n;
}

// ajouter un lien en backing
int back_add(lien_back* back,int back_max,httrackp* opt,cache_back* cache,char* adr,char* fil,char* save,char* referer_adr,char* referer_fil,int test,int* pass2_ptr) {
  int p=0;

  // vérifier cohérence de adr et fil (non vide!)
  if (strnotempty(adr)==0) {
    if ((opt->debug>1) && (opt->errlog!=NULL)) {
      fspc(opt->errlog,"debug"); fprintf(opt->errlog,"error: adr is empty for back_add"LF);
    }
    return -1;    // erreur!
  }
  if (strnotempty(fil)==0) {
    if ((opt->debug>1) && (opt->errlog!=NULL)) {
      fspc(opt->errlog,"debug"); fprintf(opt->errlog,"error: fil is empty for back_add"LF);
    }
    return -1;    // erreur!
  }
  // FIN vérifier cohérence de adr et fil (non vide!)

  // stats
  opt->state.back_add_stats++;

  // rechercher emplacement
  back_clean(opt, cache, back, back_max);
  if ( ( p = back_search(opt, back, back_max) ) >= 0) {
    back[p].send_too[0]='\0';  // éventuels paramètres supplémentaires à transmettre au serveur

    // clear r
    if (back[p].r.soc!=INVALID_SOCKET) {  /* we never know */
      deletehttp(&back[p].r);
    }
    memset(&(back[p].r), 0, sizeof(htsblk)); 
    back[p].r.soc=INVALID_SOCKET; 
    back[p].r.location=back[p].location_buffer;

    // créer entrée
    strcpybuff(back[p].url_adr,adr);
    strcpybuff(back[p].url_fil,fil);
    strcpybuff(back[p].url_sav,save);
    back[p].pass2_ptr=pass2_ptr;
    // copier referer si besoin
    strcpybuff(back[p].referer_adr,"");
    strcpybuff(back[p].referer_fil,"");
    if ((referer_adr) && (referer_fil)) {       // existe
      if ((strnotempty(referer_adr)) && (strnotempty(referer_fil))) {   // non vide
        if (referer_adr[0]!='!') {    // non détruit
          if (strcmp(referer_adr,"file://")) {      // PAS file://
            if (strcmp(referer_adr,"primary")) {      // pas referer 1er lien
              strcpybuff(back[p].referer_adr,referer_adr);
              strcpybuff(back[p].referer_fil,referer_fil);
            }
          }
        }
      }
    }
    // sav ne sert à rien pour le moment
    back[p].r.size=0;                   // rien n'a encore été chargé
    back[p].r.adr=NULL;                 // pas de bloc de mémoire
    back[p].r.is_write=0;               // à priori stockage en mémoire
    back[p].maxfile_html=opt->maxfile_html;
    back[p].maxfile_nonhtml=opt->maxfile_nonhtml;
    back[p].testmode=test;              // mode test?
    if (!opt->http10)                 // option "forcer 1.0" désactivée
      back[p].http11=1;               // autoriser http/1.1
    back[p].head_request=0;
    if (strcmp(back[p].url_sav,BACK_ADD_TEST)==0)    // HEAD
      back[p].head_request=1;
    else if (strcmp(back[p].url_sav,BACK_ADD_TEST2)==0)    // test en GET
      back[p].head_request=2;       // test en get

    /* Stop requested - abort backing */
    if (opt->state.stop) {
      back[p].r.statuscode=-1;        // fatal
      strcpybuff(back[p].r.msg,"mirror stopped by user");
      back[p].status=0;  // terminé
      if ((opt->debug>0) && (opt->log!=NULL)) {
        fspc(opt->log,"warning"); fprintf(opt->log,"File not added due to mirror cancel: %s%s"LF,adr,fil); test_flush;
      }            
      return 0;
    }

    // tester cache
    if ((strcmp(adr,"file://"))           /* pas fichier */
      && ( (!test) || (cache->type==1) )   /* cache prioritaire, laisser passer en test! */
      && ( (strnotempty(save)) || (strcmp(fil,"/robots.txt")==0) ) ) {  // si en test on ne doit pas utiliser le cache sinon telescopage avec le 302..
      //if ((!test) && (strcmp(adr,"file://")) 
      //if ((!test) && (strncmp(adr,"ftp://",6)) && (strcmp(adr,"file://")) 
#if HTS_FAST_CACHE
      long int hash_pos;
      int hash_pos_return=0;
#else
      char* a=NULL;
#endif
#if HTS_FAST_CACHE
      if (cache->hashtable) { 
#else
      if (cache->use) { 
#endif
        char buff[HTS_URLMAXSIZE*4];
#if HTS_FAST_CACHE
        strcpybuff(buff,adr); strcatbuff(buff,fil);
        hash_pos_return=inthash_read((inthash)cache->hashtable,buff,(long int*)&hash_pos);
#else
        buff[0]='\0'; strcatbuff(buff,"\n"); strcatbuff(buff,adr); strcatbuff(buff,"\n"); strcatbuff(buff,fil); strcatbuff(buff,"\n");
        a=strstr(cache->use,buff);
#endif
        
        // Ok, noté en cache->. mais bien présent dans le cache ou sur disque?
#if HTS_FAST_CACHE
        if (hash_pos_return) {
#else
        if (a) {
#endif
          if (!test) {      // non mode test
#if HTS_FAST_CACHE
            int pos=hash_pos;
#else
            int pos=-1;
            a+=strlen(buff);
            sscanf(a,"%d",&pos);    // lire position
#endif
            if (pos<0) {    // pas de mise en cache data, vérifier existence
              if (fsize(fconv(save)) <= 0) {  // fichier existe pas ou est vide!
                int found=0;

                /* It is possible that the file has been moved due to changes in build structure */
                {
                  char previous_save[HTS_URLMAXSIZE*2];
                  previous_save[0] = '\0';
                  back[p].r = cache_readex(opt, cache, adr, fil, NULL, back[p].location_buffer, previous_save, 0);
                  if (previous_save[0] != '\0' && fexist(fconv(previous_save))) {
                    rename(fconv(previous_save), fconv(save));
                    if (fexist(fconv(save))) {
                      found = 1;
                      if ((opt->debug>1) && (opt->log!=NULL)) {
                        fspc(opt->log,"debug"); fprintf(opt->log,"File '%s' has been renamed since last mirror to '%s' ; applying changes"LF, previous_save, save); test_flush;
                      }
                    } else {
                      if ((opt->debug>0) && (opt->log!=NULL)) {
                        fspc(opt->log,"error"); fprintf(opt->log,"Could not rename '%s' to '%s' ; will have to retransfer it"LF, previous_save, save); test_flush;
                      }
                    }
                  }
                }
                
                if (!found) {
#if HTS_FAST_CACHE
                  hash_pos_return=0;
#else
                  a=NULL;    
#endif
                  // dévalider car non présent sur disque dans structure originale!!!
                  // sinon, le fichier est ok à priori, mais on renverra un if-modified-since pour
                  // en être sûr
                  if (opt->norecatch) {              // tester norecatch
                    if (!fexist(fconv(save))) {  // fichier existe pas mais déclaré: on l'a effacé
                      FILE* fp=fopen(fconv(save),"wb");
                      if (fp) fclose(fp);
                      if (opt->log!=NULL) {
                        fspc(opt->log,"warning"); fprintf(opt->log,"File must have been erased by user, ignoring: %s%s"LF,back[p].url_adr,back[p].url_fil); test_flush;
                      }
                    }
                  }
                }
              }
            }
          }
        }
        //
      } else
#if HTS_FAST_CACHE
        hash_pos_return=0;
#else
        a=NULL;
#endif

      // Existe pas en cache, ou bien pas de cache présent
#if HTS_FAST_CACHE
      if (hash_pos_return) {  // OK existe en cache (et données aussi)!
#else
      if (a!=NULL) {  // OK existe en cache (et données aussi)!
#endif
        if (cache->type==1) {   // cache prioritaire (pas de test if-modified..)
                               // dans ce cas on peut également lire des réponses cachées comme 404,302...
          // lire dans le cache
          if (!test)
            back[p].r = cache_read(opt,cache,adr,fil,save, back[p].location_buffer);
          else
            back[p].r = cache_read(opt,cache,adr,fil,NULL, back[p].location_buffer);  // charger en tête uniquement du cache

          /* ensure correct location buffer set */
          back[p].r.location=back[p].location_buffer;

          /* Interdiction taille par le wizard? --> détruire */
          if (back[p].r.statuscode != -1) {  // pas d'erreur de lecture
            if (!back_checksize(opt,&back[p],0)) {
              back[p].status=0;  // FINI
              back[p].r.statuscode=-1;
              if (!back[p].testmode)
                strcpybuff(back[p].r.msg,"Cached file skipped (too big)");
              else
                strcpybuff(back[p].r.msg,"Test: Cached file skipped  (too big)");
              return 0;
            }
          }

          if (back[p].r.statuscode != -1) {  // pas d'erreur de lecture
            if ((opt->debug>0) && (opt->log!=NULL)) {
              if (!test) {
                fspc(opt->log,"debug"); fprintf(opt->log,"File immediately loaded from cache: %s%s"LF,back[p].url_adr,back[p].url_fil); test_flush;
              } else {
                fspc(opt->log,"debug"); fprintf(opt->log,"File immediately tested from cache: %s%s"LF,back[p].url_adr,back[p].url_fil); test_flush;
              }
            }
            back[p].r.notmodified=1;    // fichier non modifié
            back[p].status=0;  // OK prêt

            // finalize transfer
            if (!test) {
              if (back[p].r.statuscode>0) {
                back_finalize(opt,cache,back,p);
              }
            }

            return 0;
          } else {  // erreur
            // effacer r
            memset(&(back[p].r), 0, sizeof(htsblk)); back[p].r.soc=INVALID_SOCKET; back[p].r.location=back[p].location_buffer;
            // et continuer (chercher le fichier)
          }
          
        } else if (cache->type==2) {    // si en cache, demander de tester If-Modified-Since
          htsblk r;
          cache_header(opt,cache,adr,fil,&r);

          /* Interdiction taille par le wizard? */
          {
            LLint save_totalsize=back[p].r.totalsize;
            back[p].r.totalsize=r.totalsize;
            if (!back_checksize(opt,&back[p],1)) {
              r.statuscode = -1;
              //
              back[p].status=0;  // FINI
              deletehttp(&back[p].r); back[p].r.soc=INVALID_SOCKET;
              if (!back[p].testmode)
                strcpybuff(back[p].r.msg,"File too big");
              else
                strcpybuff(back[p].r.msg,"Test: File too big");
              return 0;
            }
            back[p].r.totalsize=save_totalsize;
          }
          
          if (r.statuscode != -1) {
            if (r.statuscode==200) {     // uniquement des 200 (OK)
              if (strnotempty(r.etag)) {  // ETag (RFC2616)
                /*
                - If both an entity tag and a Last-Modified value have been
                provided by the origin server, SHOULD use both validators in
                cache-conditional requests. This allows both HTTP/1.0 and
                HTTP/1.1 caches to respond appropriately.
                */
                if (strnotempty(r.lastmodified))
                  sprintf(back[p].send_too,"If-None-Match: %s\r\nIf-Modified-Since: %s\r\n",r.etag,r.lastmodified);
                else
                  sprintf(back[p].send_too,"If-None-Match: %s\r\n",r.etag);
              }
              else if (strnotempty(r.lastmodified))
                sprintf(back[p].send_too,"If-Modified-Since: %s\r\n",r.lastmodified);
              else if (strnotempty(cache->lastmodified))
                sprintf(back[p].send_too,"If-Modified-Since: %s\r\n",cache->lastmodified);
              
              /* this is an update of a file */
              if (strnotempty(back[p].send_too))
                back[p].is_update=1;
              back[p].r.req.nocompression=1;   /* Do not compress when updating! */
              
            }
            /* else if (strnotempty(cache->lastmodified))
            sprintf(back[p].send_too,"If-Modified-Since: %s\r\n",cache->lastmodified);
            */
          }
#if DEBUGCA
          printf("..is modified test %s\n",back[p].send_too);
#endif
        } 
        // Okay, pas trouvé dans le cache
        // Et si le fichier existe sur disque?
        // Pas dans le cache: fichier n'a pas été transféré du tout, donc pas sur disque?
      } else {
        if (fexist(save)) {    // fichier existe? aghl!
          LLint sz=fsize(save);
          // Bon, là il est possible que le fichier ait été partiellement transféré
          // (s'il l'avait été en totalité il aurait été inscrit dans le cache ET existerait sur disque)
          // PAS de If-Modified-Since, on a pas connaissance des données à la date du cache
          // On demande juste les données restantes si le date est valide (206), tout sinon (200)
          if ((ishtml(save) != 1) && (ishtml(back[p].url_fil)!=1)) {   // NON HTML (liens changés!!)
            if (sz>0) {    // Fichier non vide? (question bête, sinon on transfert tout!)
              if (strnotempty(cache->lastmodified)) {     /* pas de If-.. possible */
                /*if ( (!opt->http10) && (strnotempty(cache->lastmodified)) ) { */    /* ne pas forcer 1.0 */
#if DEBUGCA
                printf("..if unmodified since %s size "LLintP"\n",cache->lastmodified,(LLint)sz);
#endif
                if ((opt->debug>1) && (opt->log!=NULL)) {
                  fspc(opt->log,"debug"); fprintf(opt->log,"File partially present ("LLintP" bytes): %s%s"LF,(LLint)sz,back[p].url_adr,back[p].url_fil); test_flush;
                }
                
                /* impossible - don't have etag or date
                if (strnotempty(back[p].r.etag)) {  // ETag (RFC2616)
                sprintf(back[p].send_too,"If-None-Match: %s\r\n",back[p].r.etag);
                back[p].http11=1;    // En tête 1.1
                } else if (strnotempty(back[p].r.lastmodified)) {
                sprintf(back[p].send_too,"If-Unmodified-Since: %s\r\n",back[p].r.lastmodified);
                back[p].http11=1;    // En tête 1.1
                } else 
                */
                if (strlen(cache->lastmodified)) {
                  sprintf(back[p].send_too,
                    "If-Unmodified-Since: %s\r\nRange: bytes="LLintP"-\r\n"
                    ,cache->lastmodified,(LLint)sz);
                  back[p].http11=1;    // En tête 1.1
                  back[p].range_req_size=sz;
                  back[p].r.req.range_used=1;
                  back[p].r.req.nocompression=1;
                } else {
                  if ((opt->debug>0) && (opt->log!=NULL)) {
                    fspc(opt->log,"warning"); fprintf(opt->log,"Could not find timestamp for partially present file, restarting (lost "LLintP" bytes): %s%s"LF,(LLint)sz,back[p].url_adr,back[p].url_fil); test_flush;
                  }
                }
                
              } else { 
                if ((opt->debug>0) && (opt->errlog!=NULL)) {
                  fspc(opt->errlog,"warning");
                  /*
                  if (opt->http10)
                  fprintf(opt->errlog,"File partially present (%d bytes) retransfered due to HTTP/1.0 settings: %s%s"LF,sz,back[p].url_adr,back[p].url_fil);
                  else
                  */
                  fprintf(opt->errlog,"File partially present ("LLintP" bytes) retransfered due to lack of cache: %s%s"LF,(LLint)sz,back[p].url_adr,back[p].url_fil); 
                  test_flush;
                }
                /* Sinon requête normale... */
                back[p].http11=0;
              }
            } else if (opt->norecatch) {              // tester norecatch
              filenote(save,NULL);       // ne pas purger tout de même
              back[p].status=0;  // OK prêt
              back[p].r.statuscode=-1;  // erreur
              strcpybuff(back[p].r.msg,"Null-size file not recaught");
              return 0;
            }
          } else {
            if ((opt->debug>0) && (opt->errlog!=NULL)) {
              fspc(opt->errlog,"warning");
              fprintf(opt->errlog,"HTML file ("LLintP" bytes) retransfered due to lack of cache: %s%s"LF,(LLint)sz,back[p].url_adr,back[p].url_fil); 
              test_flush;
            }
            /* Sinon requête normale... */
            back[p].http11=0;
          }
        }
      }
    }


    {
      ///htsblk r;   non directement dans la structure-réponse!
      T_SOC soc;
      
      // ouvrir liaison, envoyer requète
      // ne pas traiter ou recevoir l'en tête immédiatement
      memset(&(back[p].r), 0, sizeof(htsblk)); back[p].r.soc=INVALID_SOCKET; back[p].r.location=back[p].location_buffer;
      // recopier proxy
      memcpy(&(back[p].r.req.proxy), &opt->proxy, sizeof(opt->proxy));
      // et user-agent
      strcpybuff(back[p].r.req.user_agent,opt->user_agent);
      strcpybuff(back[p].r.req.lang_iso,opt->lang_iso);
      back[p].r.req.user_agent_send=opt->user_agent_send;
      // et http11
      back[p].r.req.http11=back[p].http11;
      back[p].r.req.nocompression=opt->nocompression;
      back[p].r.req.nokeepalive=opt->nokeepalive;

      // mode ftp, court-circuit!
      if (strfield(back[p].url_adr,"ftp://")) {
        if (back[p].testmode) {
          if ((opt->debug>1) && (opt->errlog!=NULL)) {
            fspc(opt->errlog,"debug"); fprintf(opt->errlog,"error: forbidden test with ftp link for back_add"LF);
          }
          return -1;    // erreur pas de test permis
        }
        if (!(back[p].r.req.proxy.active && opt->ftp_proxy)) { // connexion directe, gérée en thread
          back[p].status=1000;   // connexion ftp
#if USE_BEGINTHREAD
          launch_ftp(&(back[p]));
#else
          {
            char nid[32];
            sprintf(nid,"htsftp%d-in_progress.lock",p);
            strcpybuff(back[p].location_buffer,fconcat(opt->path_log,nid));
          }
          launch_ftp(&(back[p]),back[p].location_buffer,opt->exec);
#endif
          return 0;
        }
      }
#if HTS_USEOPENSSL
      else if (SSL_is_available && strfield(back[p].url_adr,"https://")) {        // let's rock
        back[p].r.ssl = 1;
        // back[p].r.ssl_soc = NULL;
        back[p].r.ssl_con = NULL;
      }
#endif
      
      if (!back_trylive(opt,back, back_max, p)) {
#if HTS_XGETHOST
#if HDEBUG
        printf("back_solve..\n");
#endif
        back[p].status=101;    // tentative de résolution du nom de host
        soc=INVALID_SOCKET;    // pas encore ouverte
        back_solve(&back[p]);  // préparer
        if (host_wait(&back[p])) {  // prêt, par ex fichier ou dispo dans dns
#if HDEBUG
          printf("ok, dns cache ready..\n");
#endif
          soc=http_xfopen(0,0,0,back[p].send_too,adr,fil,&(back[p].r));
          if (soc==INVALID_SOCKET) {
            back[p].status=0;  // fini, erreur
          }
        }
        //
#else
        //
#if CNXDEBUG
        printf("XFopen..\n");
#endif
        
        if (strnotempty(back[p].send_too))    // envoyer un if-modified-since
#if HTS_XCONN
          soc=http_xfopen(0,0,0,back[p].send_too,adr,fil,&(back[p].r));
#else
        soc=http_xfopen(0,0,1,back[p].send_too,adr,fil,&(back[p].r));
#endif
        else
#if HTS_XCONN
          soc=http_xfopen(test,0,0,NULL,adr,fil,&(back[p].r));
#else
        soc=http_xfopen(test,0,1,NULL,adr,fil,&(back[p].r));
#endif
#endif
      } else {
        soc = back[p].r.soc;

        if ((opt->debug>1) && (opt->log!=NULL)) {
          fspc(opt->log,"debug"); fprintf(opt->log,"(Keep-Alive): successfully linked #%d (for %s%s)"LF, 
            back[p].r.debugid,
            back[p].url_adr, back[p].url_fil); test_flush;
        }
      }
      
      if (opt->timeout>0) {    // gestion du opt->timeout
        back[p].timeout=opt->timeout;
        back[p].timeout_refresh=time_local();
      } else {
        back[p].timeout=-1;    // pas de gestion (default)
      }
      
      if (opt->rateout>0) {    // gestion d'un taux minimum de transfert toléré
        back[p].rateout=opt->rateout;
        back[p].rateout_time=time_local();
      } else {
        back[p].rateout=-1;    // pas de gestion (default)
      }

      // Note: on charge les code-page erreurs (erreur 404, etc) dans le cas où cela est
      // rattrapable (exemple: 301,302 moved xxx -> refresh sur la page!)
      //if ((back[p].statuscode!=200) || (soc<0)) { // ERREUR HTTP/autre

#if CNXDEBUG
printf("Xfopen ok, poll..\n");
#endif

#if HTS_XGETHOST
    if (soc!=INVALID_SOCKET)
      if (back[p].status==101) {  // pas d'erreur
        if (!back[p].r.is_file)
          back[p].status=100;   // connexion en cours
        else
          back[p].status=1;     // fichier
      }

#else
      if (soc==INVALID_SOCKET) { // erreur socket
        back[p].status=0;    // FINI
        //if (back[p].soc!=INVALID_SOCKET) deletehttp(back[p].soc);
        back[p].r.soc=INVALID_SOCKET;
      } else {
        if (!back[p].r.is_file)
#if HTS_XCONN
          back[p].status=100;   // connexion en cours
#else
          back[p].status=99;    // chargement en tête en cours
#endif
        else
          back[p].status=1;     // chargement fichier
#if BDEBUG==1
        printf("..loading header\n");
#endif
      }
#endif
      
    }


    // note: si il y a erreur (404,etc) status=2 (terminé/échec) mais
    // le lien est considéré comme traité
    //if (back[p].soc<0)  // erreur
    //  return -1;

    return 0;
  } else {
    if ((opt->debug>1) && (opt->errlog!=NULL)) {
      fspc(opt->errlog,"debug"); fprintf(opt->errlog,"error: no space left in stack for back_add"LF);
    }
    return -1;    // plus de place
  }
}



#if HTS_XGETHOST
#if USE_BEGINTHREAD
// lancement multithread du robot
PTHREAD_TYPE Hostlookup(void* iadr_p) {
  char iadr[256];
  t_dnscache* cache=_hts_cache();  // adresse du cache
  t_hostent* hp;
  int error_found=0;

  // recopier (après id:pass)
#if DEBUGDNS 
  printf("resolv in background: %s\n",jump_identification(iadr_p));
#endif
  strcpybuff(iadr,jump_identification(iadr_p));
  // couper éventuel :
  {
    char *a;
    if ( (a=jump_toport(iadr)) )
      *a='\0';          // get rid of it
  }
  freet(iadr_p);

  // attendre que le cache dns soit prêt
  while(_hts_lockdns(-1));  // attendre libération
  _hts_lockdns(1);          // locker
  while(cache->n) {
    if (strcmp(cache->iadr,iadr)==0) {
      error_found=1;
    }
    cache=cache->n;    // calculer queue
  }
  if (strcmp(cache->iadr,iadr)==0) {
    error_found=1;
  }

  if (!error_found) {
    // en gros copie de hts_gethostbyname sans le return
    cache->n=(t_dnscache*) calloct(1,sizeof(t_dnscache));
    if (cache->n!=NULL) {
      t_fullhostent fullhostent_buffer;
      strcpybuff(cache->n->iadr,iadr);
      cache->n->host_length=0;        /* pour le moment rien */
      cache->n->n=NULL;
      _hts_lockdns(0);          // délocker
      
      /* resolve */
#if DEBUGDNS 
      printf("gethostbyname() in progress for %s\n",iadr);
#endif
      cache->n->host_length=-1;
      memset(cache->n->host_addr, 0, sizeof(cache->n->host_addr));
      hp=vxgethostbyname(iadr, &fullhostent_buffer);
      if (hp!=NULL) {
        memcpy(cache->n->host_addr, hp->h_addr, hp->h_length);
        cache->n->host_length = hp->h_length;
      }
    } else 
    _hts_lockdns(0);          // délocker
  } else {
#if DEBUGDNS 
    printf("aborting resolv for %s (found)\n",iadr);
#endif
    _hts_lockdns(0);          // délocker
  }
  // fin de copie de hts_gethostbyname

#if DEBUGDNS 
  printf("quitting resolv for %s (result: %d)\n",iadr,(cache->n!=NULL)?cache->n->host_length:(-999));
#endif

  return PTHREAD_RETURN;     /* _endthread implied  */
}
#endif

// attendre que le host (ou celui du proxy) ait été résolu
// si c'est un fichier, la résolution est immédiate
// idem pour ftp://
void back_solve(lien_back* back) {
  if ((!strfield(back->url_adr,"file://")) && (!strfield(back->url_adr,"ftp://"))) {
  //## if (back->url_adr[0]!=lOCAL_CHAR) {  // qq chose à préparer
    char* a;
    if (!(back->r.req.proxy.active))
      a=back->url_adr;
    else
      a=back->r.req.proxy.name;
    a = jump_protocol(a);
    if (!hts_dnstest(a)) {   // non encore testé!..
      // inscire en thread
#if HTS_WIN
      // Windows
#if USE_BEGINTHREAD
      {
        char* p = calloct(strlen(a)+2,1);
        if (p) {
          strcpybuff(p,a);
          _beginthread( Hostlookup , 0, p );
        }
      }
#else
      /*t_hostent* h=*/
      /*hts_gethostbyname(a);*/  // calcul
#endif
#else
#if USE_BEGINTHREAD
        char* p = calloct(strlen(a)+2,1);
        if (p) {
          strcpybuff(p,a);
          _beginthread( Hostlookup , 0, p );
        }
#else
      // Sous Unix, le gethostbyname() est bloquant..
      /*t_hostent* h=*/
      /*hts_gethostbyname(a);*/  // calcul
#endif
#endif
    }
  }
}

// détermine si le host a pu être résolu
int host_wait(lien_back* back) {
  if ((!strfield(back->url_adr,"file://")) && (!strfield(back->url_adr,"ftp://"))) {
  //## if (back->url_adr[0]!=lOCAL_CHAR) {
    if (!(back->r.req.proxy.active)) {
      return (hts_dnstest(back->url_adr));
    } else {
      return (hts_dnstest(back->r.req.proxy.name));      
    }
  } else return 1;    // prêt, fichier local
}
#endif


// élimine les fichiers non html en backing (anticipation)
// cleanup non-html files in backing to save backing space
// and allow faster "save in cache" operation
// also cleanup keep-alive sockets and ensure that not too many sockets are being opened
void back_clean(httrackp* opt,cache_back* cache,lien_back* back,int back_max) {
#if HTS_ANALYSTE
  int oneMore = ( (_hts_in_html_parsing == 2 && opt->maxsoc >= 2) || (_hts_in_html_parsing == 1 && opt->maxsoc >= 4) ) ? 1 : 0;  // testing links
#endif
  int i;
  for(i=0;i<back_max;i++) {
    if (back[i].status == 0) {                                   // ready
      /* Check autoclean */
      if (!back[i].testmode) {                                   // not test mode
        if (strnotempty(back[i].url_sav)) {                      // filename exists
          if (back[i].r.statuscode==200) {                   // HTTP "OK"
            if (back[i].r.size>0) {                              // size>0
              if (back[i].r.is_write                                // not in memory (on disk, ready)
                && !is_hypertext_mime(back[i].r.contenttype)        // not HTML/hypertext
                && !may_be_hypertext_mime(back[i].r.contenttype)    // may NOT be parseable mime type
                ) {
                if (back[i].pass2_ptr) {
                  // finalize
                  // // back_finalize(opt,cache,back,i);
                  // stats
                  //HTS_STAT.stat_bytes+=back[i].r.size;
                  //HTS_STAT.stat_files++;
                  //if ( (!back[i].r.notmodified) && (opt->is_update) ) { 
                  //  HTS_STAT.stat_updated_files++;       // page modifiée
                  //}
                  //xxxcache_mayadd(opt,cache,&back[i].r,back[i].url_adr,back[i].url_fil,back[i].url_sav);
                  usercommand(opt, 0, NULL, back[i].url_sav, back[i].url_adr, back[i].url_fil);
                  *back[i].pass2_ptr=-1;  // Done!
                  back_maydelete(opt,back,i);    // May delete backing entry
                  if ((opt->debug>0) && (opt->log!=NULL)) {
                    fspc(opt->log,"info"); fprintf(opt->log,"File successfully written in background: %s"LF,back[i].url_sav); test_flush;
                  }
                }
              } else {
                if (!back[i].finalized) {
                  if (1) {
                    /* Ensure deleted or recycled socket */
                    /* BUT DO NOT YET WIPE back[i].r.adr */
                    back_maydeletehttp(opt, back, back_max, i);
                    if ( (opt->debug>1) && (opt->log!=NULL) ) {
                      fspc(opt->log,"debug"); fprintf(opt->log,"file %s%s validated (cached, left in memory)"LF,back[i].url_adr,back[i].url_fil); test_flush;
                    }
                  } else {
                  /*
                  NOT YET HANDLED CORRECTLY (READ IN NEW CACHE TO DO)
                    */
                    /* Lock the entry but do not keep the html data in memory (in cache) */
                    if (opt->cache) {
                      htsblk r;
                      
                      /* Ensure deleted or recycled socket */
                      back_maydeletehttp(opt, back, back_max, i);
                      assertf(back[i].r.soc == INVALID_SOCKET);
                      
                      /* Check header */
                      cache_header(opt,cache,back[i].url_adr,back[i].url_fil,&r);
                      if (r.statuscode == 200) {
                        if (back[i].r.soc == INVALID_SOCKET) {
                          /* Delete buffer and sockets */
                          deleteaddr(&back[i].r);
                          deletehttp(&back[i].r);
                          back[i].finalized = 1;
                          if ( (opt->debug>1) && (opt->log!=NULL) ) {
                            fspc(opt->log,"debug"); fprintf(opt->log,"file %s%s temporarily left in cache to spare memory"LF,back[i].url_adr,back[i].url_fil); test_flush;
                          }
                        }
                      } else {
                        if ((opt->debug>0) && (opt->log!=NULL)) {
                          fspc(opt->log,"warning"); fprintf(opt->log,"Unexpected html cache lookup error during back clean"LF); test_flush;
                        }            
                      }
                      // xxc xxc
                    }
                  }
                }
              }
            }
          }
        }
      }
    } else if (back[i].status == -103) {                         // waiting (keep-alive)
      if (
        ! back[i].r.keep_alive
        || back[i].r.soc == INVALID_SOCKET
        || back[i].r.keep_alive_max < 1
        || time_local() >= back[i].ka_time_start + back[i].r.keep_alive_t
        ) {
        if ((opt->debug>0) && (opt->log!=NULL)) {
            fspc(opt->log,"debug"); fprintf(opt->log,"(Keep-Alive): live socket closed #%d (%s)"LF, 
            back[i].r.debugid,
            back[i].url_adr);
            test_flush;
        }
        back_delete(opt,back, i);    // delete backing entry
      }
    }
  }
  /* switch connections to live ones */
  for(i=0;i<back_max;i++) {
    if (back[i].status == 0) {                                   // ready
      if (back[i].r.soc != INVALID_SOCKET) {
        back_maydeletehttp(opt,back, back_max, i);
      }
      
    }
  }
  /* delete sockets if too many keep-alive'd sockets in background */
  if (opt->maxsoc > 0) {
    int max = opt->maxsoc + oneMore;
    int curr = back_nsoc_overall(back, back_max);
    if (curr > max) {
      if ((opt->debug>1) && (opt->log!=NULL)) {
        fspc(opt->log,"debug"); fprintf(opt->log,"(Keep-Alive): deleting #%d sockets"LF, 
          curr - max); test_flush;
      }
    }
    for(i = 0 ; i < back_max && curr > max ; i++) {
      if (back[i].status == -103) {
        back_delete(opt,back, i);    // delete backing entry
        curr--;
      }
    }
  }
}


// attente (gestion des buffers des sockets)
void back_wait(lien_back* back,int back_max,httrackp* opt,cache_back* cache,TStamp stat_timestart) {
  int i;
  T_SOC nfds=INVALID_SOCKET;
  fd_set fds,fds_c,fds_e;     // fds pour lecture, connect (write), et erreur
  int nsockets;     // nbre sockets
  LLint max_read_bytes;  // max bytes read per sockets
  struct timeval tv;
  int do_wait=0;
  int gestion_timeout=0;
  int busy_recv=0;     // pas de données pour le moment   
  int busy_state=0;    // pas de connexions
  int max_loop;  // nombre de boucles max à parcourir..
#if HTS_ANALYSTE
  int max_loop_chk=0;
#endif


  // max. number of loops
  max_loop=8;

#if 1
  // Cleanup the stack to save space!
  back_clean(opt,cache,back,back_max);
#endif

  // recevoir tant qu'il y a des données (avec un maximum de max_loop boucles)
  do_wait=0;
  gestion_timeout=0;
  do {
    int max_c;
    busy_state=busy_recv=0;

#if 0
    check_rate(stat_timestart,opt->maxrate);    // vérifier taux de transfert
#endif
    // inscrire les sockets actuelles, et rechercher l'ID la plus élevée
    FD_ZERO(&fds);
    FD_ZERO(&fds_c);
    FD_ZERO(&fds_e);
    nsockets=0;
    max_read_bytes=TAILLE_BUFFER;     // maximum bytes that can be read
    nfds=INVALID_SOCKET;

    max_c=1;
    for(i=0;i<back_max;i++) {

      // en cas de gestion du connect préemptif
#if HTS_XCONN
      if (back[i].status==100) {      // connexion
        do_wait=1;

        // noter socket write
        FD_SET(back[i].r.soc,&fds_c);
        
        // noter socket erreur
        FD_SET(back[i].r.soc,&fds_e);

        // calculer max
        if (max_c) {
          max_c=0;
          nfds=back[i].r.soc;
        } else if (back[i].r.soc>nfds) {
          // ID socket la plus élevée
          nfds=back[i].r.soc;
        }
        
      } else
#endif
#if HTS_XGETHOST
      if (back[i].status==101) {      // attente
        // rien à faire..
      } else
#endif
      // poll pour la lecture sur les sockets
      if ((back[i].status>0) && (back[i].status<100)) {  // en réception http
            
#if BDEBUG==1
        //printf("....socket in progress: %d\n",back[i].r.soc);
#endif
        // non local et non ftp
        if (!back[i].r.is_file) {
        //## if (back[i].url_adr[0]!=lOCAL_CHAR) {
          
          // vérification de sécurité
          if (back[i].r.soc!=INVALID_SOCKET) {  // hey, you never know..
            do_wait=1;
            
            // noter socket read
            FD_SET(back[i].r.soc,&fds);
            
            // noter socket error
            FD_SET(back[i].r.soc,&fds_e);
            
            // incrémenter nombre de sockets
            nsockets++;

            // calculer max
            if (max_c) {
              max_c=0;
              nfds=back[i].r.soc;
            } else if (back[i].r.soc>nfds) {
              // ID socket la plus élevée
              nfds=back[i].r.soc;
            }
          } else {
            back[i].r.statuscode=-4;
            if (back[i].status==100)
              strcpybuff(back[i].r.msg,"Connect Error");
            else
              strcpybuff(back[i].r.msg,"Receive Error");
            back[i].status=0;  // terminé
            if ((opt->debug>0) && (opt->log!=NULL)) {
              fspc(opt->log,"warning"); fprintf(opt->log,"Unexpected socket error during pre-loop"LF); test_flush;
            }            
          }
#if WIDE_DEBUG
          else {
            DEBUG_W("PANIC!!! Socket is invalid in a poll test!\n");
          }
#endif
          
        }
        
      }
    }    
    nfds++;
    
    if (do_wait) {  // attendre
      // temps d'attente max: 2.5 seconde
      tv.tv_sec=HTS_SOCK_SEC;
      tv.tv_usec=HTS_SOCK_MS;
      
#if BDEBUG==1
      printf("..select\n");
#endif
      
      // poller les sockets-attention au noyau sous Unix..
#if HTS_WIDE_DEBUG    
      DEBUG_W("select\n");
#endif
      select(nfds,&fds,&fds_c,&fds_e,&tv);
#if HTS_WIDE_DEBUG    
      DEBUG_W("select done\n");
#endif      
    }
    
    // maximum data which can be received for a socket, if limited
    if (nsockets) {
      if (opt->maxrate>0) {
        max_read_bytes = ( check_downloadable_bytes(opt->maxrate) / nsockets );
        if (max_read_bytes > TAILLE_BUFFER) {
          /* limit size */
          max_read_bytes = TAILLE_BUFFER;
        } else if (max_read_bytes < TAILLE_BUFFER) {
          /* a small pause */
          Sleep(10);
        }
      }
    }
    if (!max_read_bytes)
      busy_recv=0;
    
    // recevoir les données arrivées
    for(i=0;i<back_max;i++) {
      
      if (back[i].status>0) {
        if (!back[i].r.is_file) {  // not file..
          if (back[i].r.soc!=INVALID_SOCKET) {  // hey, you never know..
            int err=FD_ISSET(back[i].r.soc,&fds_e);
            if (err) {
              if (back[i].r.soc!=INVALID_SOCKET) {
#if HTS_DEBUG_CLOSESOCK
                DEBUG_W("back_wait: deletehttp\n");
#endif
                deletehttp(&back[i].r);
              }
              back[i].r.soc=INVALID_SOCKET;
              back[i].r.statuscode=-4;
              if (back[i].status==100)
                strcpybuff(back[i].r.msg,"Connect Error");
              else
                strcpybuff(back[i].r.msg,"Receive Error");
              if (back[i].status == -103) {     /* Keep-alive socket */
                back_delete(opt,back, i);
              } else {
                back[i].status=0;  // terminé
              }
            }
          }
        }
      }
      
      // ---- FLAG WRITE MIS A UN?: POUR LE CONNECT
      if (back[i].status==100) {   // attendre connect
        int dispo=0;
        // vérifier l'existance de timeout-check
        if (!gestion_timeout)
          if (back[i].timeout>0)
            gestion_timeout=1;
          
          // connecté?
          dispo=FD_ISSET(back[i].r.soc,&fds_c);
          if (dispo) {    // ok connected!!
            busy_state=1;
            
#if HTS_USEOPENSSL
            /* SSL mode */
            if (SSL_is_available && back[i].r.ssl) {
              // handshake not yet launched
              if (!back[i].r.ssl_con) {
                SSL_CTX_set_options(openssl_ctx, SSL_OP_ALL);
                // new session
                back[i].r.ssl_con = SSL_new(openssl_ctx);
                if (back[i].r.ssl_con) {
                  SSL_clear(back[i].r.ssl_con);
                  if (SSL_set_fd(back[i].r.ssl_con, back[i].r.soc) == 1) {
                    SSL_set_connect_state(back[i].r.ssl_con);
                    back[i].status = 102;         /* handshake wait */
                  } else
                    back[i].r.statuscode=-6;
                } else
                  back[i].r.statuscode=-6;
              }
              /* Error */
              if (back[i].r.statuscode == -6) {
                strcpybuff(back[i].r.msg, "bad SSL/TLS handshake");
                deletehttp(&back[i].r);
                back[i].r.soc=INVALID_SOCKET;
                back[i].r.statuscode=-5;
                back[i].status=0;
              }
            }
            
#endif

#if BDEBUG==1
          printf("..connect ok on socket %d\n",back[i].r.soc);
#endif
          
          if ((back[i].r.soc != INVALID_SOCKET) && (back[i].status==100)) {
            /* limit nb. connections/seconds to avoid server overload */
            if (opt->maxconn>0) {
              Sleep(1000/opt->maxconn);
            }
            
            back[i].ka_time_start=time_local();
            if (back[i].timeout>0) {    // refresh timeout si besoin est
              back[i].timeout_refresh=back[i].ka_time_start;
            }
            if (back[i].rateout>0) {    // le taux de transfert de base sur le début de la connexion
              back[i].rateout_time=back[i].ka_time_start;
            }
            // envoyer header
            //if (strcmp(back[i].url_sav,BACK_ADD_TEST)!=0)    // vrai get
            HTS_STAT.stat_nrequests++;
            if (!back[i].head_request)
              http_sendhead(opt->cookie,0,back[i].send_too,back[i].url_adr,back[i].url_fil,back[i].referer_adr,back[i].referer_fil,&back[i].r);         
            else if (back[i].head_request==2)  // test en GET!
              http_sendhead(opt->cookie,0,back[i].send_too,back[i].url_adr,back[i].url_fil,back[i].referer_adr,back[i].referer_fil,&back[i].r);         
            else        // test!
              http_sendhead(opt->cookie,1,back[i].send_too,back[i].url_adr,back[i].url_fil,back[i].referer_adr,back[i].referer_fil,&back[i].r);         
            back[i].status=99;  // attendre en tête maintenant
          }
        }
        
        // attente gethostbyname
      }
#if HTS_USEOPENSSL
      else if (SSL_is_available && back[i].status==102) {   // wait for SSL handshake
        /* SSL mode */
        if (back[i].r.ssl) {
          int conn_code;
          if ((conn_code = SSL_connect(back[i].r.ssl_con)) <= 0) {
            /* non blocking I/O, will retry */
            int err_code = SSL_get_error(back[i].r.ssl_con, conn_code);
            if (
              (err_code != SSL_ERROR_WANT_READ)
              &&
              (err_code != SSL_ERROR_WANT_WRITE)
              ) {
              char tmp[256];
              tmp[0]='\0';
              ERR_error_string(err_code, tmp);
              back[i].r.msg[0]='\0';
              strncatbuff(back[i].r.msg, tmp, sizeof(back[i].r.msg) - 2);
              if (!strnotempty(back[i].r.msg)) {
                sprintf(back[i].r.msg, "SSL/TLS error %d", err_code);
              }
              deletehttp(&back[i].r);
              back[i].r.soc=INVALID_SOCKET;
              back[i].r.statuscode=-5;
              back[i].status=0;
            }
          } else {        /* got it! */
            back[i].status=100;       // back to waitconnect
          }
        } else {
          strcpybuff(back[i].r.msg, "unexpected SSL/TLS error");
          deletehttp(&back[i].r);
          back[i].r.soc=INVALID_SOCKET;
          back[i].r.statuscode=-5;
          back[i].status=0;
        }
        
      }
#endif
#if HTS_XGETHOST
      else if (back[i].status==101) {  // attendre gethostbyname
#if DEBUGDNS 
        //printf("status 101 for %s\n",back[i].url_adr);
#endif

        if (!gestion_timeout)
          if (back[i].timeout>0)
            gestion_timeout=1;

        if (host_wait(&back[i])) {    // prêt
          back[i].status=100;        // attente connexion
          if (back[i].timeout>0) {    // refresh timeout si besoin est
            back[i].timeout_refresh=time_local();
          }
          if (back[i].rateout>0) {    // le taux de transfert de base sur le début de la connexion
            back[i].rateout_time=time_local();
          }

          back[i].r.soc=http_xfopen(0,0,0,back[i].send_too,back[i].url_adr,back[i].url_fil,&(back[i].r));
          if (back[i].r.soc==INVALID_SOCKET) {
            back[i].status=0;  // fini, erreur
            if (back[i].r.soc!=INVALID_SOCKET) {
#if HTS_DEBUG_CLOSESOCK
              DEBUG_W("back_wait(2): deletehttp\n");
#endif
              deletehttp(&back[i].r);
            }
            back[i].r.soc=INVALID_SOCKET;
            back[i].r.statuscode=-5;
            if (strnotempty(back[i].r.msg)==0) 
              strcpybuff(back[i].r.msg,"Unable to resolve host name");
          }
        }
        

      // ---- FLAG READ MIS A UN?: POUR LA RECEPTION
      }
#endif
#if USE_BEGINTHREAD
      // ..rien à faire, c'est magic les threads
#else
      else if (back[i].status==1000) {  // en réception ftp
        if (!fexist(back[i].location_buffer)) {    // terminé
          FILE* fp;
          fp=fopen(fconcat(back[i].location_buffer,".ok"),"rb");
          if (fp) {
            int j=0;
            fscanf(fp,"%d ",&(back[i].r.statuscode));
            while(!feof(fp)) {
              int c = fgetc(fp);
              if (c!=EOF)
                back[i].r.msg[j++]=c;
            }
            back[i].r.msg[j++]='\0';
            fclose(fp);
            remove(fconcat(back[i].location_buffer,".ok"));
            strcpybuff(fconcat(back[i].location_buffer,".ok"),"");
          } else {
            strcpybuff(back[i].r.msg,"Unknown ftp result, check if file is ok");
            back[i].r.statuscode=-1;
          }
          back[i].status=0;
          // finalize transfer
          if (back[i].r.statuscode>0) {
            back_finalize(opt,cache,back,i);
          }
        }
      }
#endif
      else if ((back[i].status>0) && (back[i].status<1000)) {  // en réception http
        int dispo=0;
        
        // vérifier l'existance de timeout-check
        if (!gestion_timeout)
          if (back[i].timeout>0)
            gestion_timeout=1;
          
          // données dispo?
          //## if (back[i].url_adr[0]!=lOCAL_CHAR)
          if (!back[i].r.is_file) {
            dispo=FD_ISSET(back[i].r.soc,&fds);
          }
          else
            dispo=1;

          // Check transfer rate!
          if (!max_read_bytes)
            dispo=0;                // limit transfer rate
          
          if (dispo) {    // données dispo
            LLint retour_fread;
            busy_recv=1;    // on récupère encore
#if BDEBUG==1
            printf("..data available on socket %d\n",back[i].r.soc);
#endif

            
            // range size hack old location

#if HTS_DIRECTDISK
            // Court-circuit:
            // Peut-on stocker le fichier directement sur disque?
            // Ahh que ca serait vachement mieux et que ahh que la mémoire vous dit merci!
            if (back[i].status) {
              if (back[i].r.is_write==0) {  // mode mémoire
                if (back[i].r.adr==NULL) {  // rien n'a été écrit
                  if (!back[i].testmode) {  // pas mode test
                    if (strnotempty(back[i].url_sav)) {
                      if (strcmp(back[i].url_fil,"/robots.txt")) {
                        if (back[i].r.statuscode==200) {  // 'OK'
                          if (!is_hypertext_mime(back[i].r.contenttype)) {    // pas HTML
                            if (opt->getmode&2) {    // on peut ecrire des non html
                              int fcheck=0;
                              back[i].r.is_write=1;    // écrire
                              if (back[i].r.compressed
                                &&
                                /* .gz are *NOT* depacked!! */
                                (strfield(get_ext(back[i].url_sav),"gz") == 0)
                                ) {
                                back[i].tmpfile_buffer[0]='\0';
                                back[i].tmpfile=tmpnam(back[i].tmpfile_buffer);
                                if (back[i].tmpfile != NULL && back[i].tmpfile[0])
                                  back[i].r.out=fopen(back[i].tmpfile,"wb");
                              } else {
                                back[i].r.compressed=0;
                                back[i].r.out=filecreate(back[i].url_sav);
                              }
                              if (back[i].r.out==NULL) {
                                if ((fcheck=check_fatal_io_errno())) {
                                  opt->state.exit_xh=-1;   /* fatal error */
                                }
                              }
#if HDEBUG
                              printf("direct-disk: %s\n",back[i].url_sav);
#endif
                              if ((opt->debug>1) && (opt->log!=NULL)) {
                                fspc(opt->log,"debug"); fprintf(opt->log,"File received from net to disk: %s%s"LF,back[i].url_adr,back[i].url_fil); test_flush;
                              }
                              
                              if (back[i].r.out==NULL) {
                                if (opt->errlog) {
                                  fspc(opt->errlog,"error");
                                  fprintf(opt->errlog,"Unable to save file %s : %s"LF,back[i].url_sav, strerror(errno));
                                  if (fcheck) {
                                    fspc(opt->errlog,"error");
                                    fprintf(opt->errlog,"* * Fatal write error, giving up"LF);
                                  }
                                  test_flush;
                                }
                                back[i].r.is_write=0;    // erreur, abandonner
#if HDEBUG
                                printf("..error!\n");
#endif
                              }
#if HTS_WIN==0
                              else chmod(back[i].url_sav,HTS_ACCESS_FILE);      
#endif          
                            } else {  // on coupe tout!
                              if ((opt->debug>1) && (opt->log!=NULL)) {
                                fspc(opt->log,"debug"); fprintf(opt->log,"File cancelled (non HTML): %s%s"LF,back[i].url_adr,back[i].url_fil); test_flush;
                              }
                              back[i].status=0;  // terminé
                              if (!back[i].testmode)
                                back[i].r.statuscode=-10;    // EUHH CANCEL
                              else
                                back[i].r.statuscode=-10;    // "TEST OK"
                              if (back[i].r.soc!=INVALID_SOCKET) {
#if HTS_DEBUG_CLOSESOCK
                                DEBUG_W("back_wait(3): deletehttp\n");
#endif
                                deletehttp(&back[i].r);
                              }
                              back[i].r.soc=INVALID_SOCKET;
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
#endif              

            // réception de données depuis socket ou fichier
            if (back[i].status) {
              if (back[i].status==99)  // recevoir par bloc de lignes
                retour_fread=http_xfread1(&(back[i].r),0);
              else if (back[i].status==98 || back[i].status==97) { // recevoir longueur chunk en hexa caractère par caractère
                // backuper pour lire dans le buffer chunk
                htsblk r;
                memcpy(&r, &(back[i].r), sizeof(htsblk));
                back[i].r.is_write=0;                   // mémoire
                back[i].r.adr=back[i].chunk_adr;        // adresse
                back[i].r.size=back[i].chunk_size;      // taille taille chunk
                back[i].r.totalsize=-1;                 // total inconnu
                back[i].r.out=NULL;
                back[i].r.is_file=0;
                //
                // ligne par ligne
                retour_fread=http_xfread1(&(back[i].r),-1);
                // modifier et restaurer
                back[i].chunk_adr=back[i].r.adr;        // adresse
                back[i].chunk_size=back[i].r.size;      // taille taille chunk
                memcpy(&(back[i].r), &r, sizeof(htsblk));    // restaurer véritable r
              }
              else if (back[i].is_chunk) {         // attention chunk, limiter taille à lire
#if CHUNKDEBUG==1
                printf("[%d] read %d bytes\n",(int)back[i].r.soc,(int)min(back[i].r.totalsize-back[i].r.size,max_read_bytes));
#endif
                retour_fread=(int) http_xfread1(&(back[i].r),(int) min(back[i].r.totalsize-back[i].r.size,max_read_bytes));
              } else              
                retour_fread=(int) http_xfread1(&(back[i].r),(int) max_read_bytes);
                // retour_fread=http_fread1(&(back[i].r));
            } else
              retour_fread=-1;                    // interruption ou annulation interne (peut ne pas être une erreur)
            
            // Si réception chunk, tester si on est pas à la fin!
            if (back[i].status==1) {
              if (back[i].is_chunk) {     // attendre prochain chunk
                if (back[i].r.size==back[i].r.totalsize) {      // fin chunk!
                  //printf("chunk end at %d\n",back[i].r.size);
                  back[i].status=97;  /* fetch ending CRLF */
                  if (back[i].chunk_adr!=NULL) { 
                    freet(back[i].chunk_adr); 
                    back[i].chunk_adr=NULL; 
                  } 
                  back[i].chunk_size=0;
                  retour_fread=0;       // pas d'erreur
#if CHUNKDEBUG==1
                  printf("[%d] waiting for current chunk CRLF..\n",(int)back[i].r.soc);
#endif
                }
              } else if (back[i].r.keep_alive) {
                if (back[i].r.size==back[i].r.totalsize) {      // fin!
                  retour_fread=-1;       // end
                }
              }
            }
            
            if (retour_fread < 0) {    // fin réception
              back[i].status=0;    // terminé
              if (back[i].r.soc!=INVALID_SOCKET) {
#if HTS_DEBUG_CLOSESOCK
                DEBUG_W("back_wait(4): deletehttp\n");
#endif
                /*KA deletehttp(&back[i].r);*/
                back_maydeletehttp(opt, back, back_max, i);
              }
              /*KA back[i].r.soc=INVALID_SOCKET; */
#if CHUNKDEBUG==1
              if (back[i].is_chunk)
                printf("[%d] must be the last chunk for %s (connection closed) - %d/%d\n",(int)back[i].r.soc,back[i].url_fil,back[i].r.size,back[i].r.totalsize);
#endif
              //if ((back[i].r.statuscode==-1) && (strnotempty(back[i].r.msg)==0)) {
              if ((back[i].r.statuscode <= 0) && (strnotempty(back[i].r.msg)==0)) {
#if HDEBUG
                printf("error interruped: %s\n",back[i].r.adr);
#endif        
                if (back[i].r.size>0)
                  strcatbuff(back[i].r.msg,"Interrupted transfer");
                else
                  strcatbuff(back[i].r.msg,"No data (connection closed)");
                back[i].r.statuscode=-4;
              }

              // finalize transfer
              if (back[i].r.statuscode>0) {
                back_finalize(opt,cache,back,i);
              }

              if (back[i].r.totalsize>0) {    // tester totalsize
              //if ((back[i].r.totalsize>0) && (back[i].status==99)) {    // tester totalsize
                if (back[i].r.totalsize!=back[i].r.size) {  // pas la même!
                  if (!opt->tolerant) {
                    //#if HTS_CL_IS_FATAL
                    deleteaddr(&back[i].r);
                    if (back[i].r.size<back[i].r.totalsize)
                      back[i].r.statuscode=-4;        // recatch
                    sprintf(back[i].r.msg,"Incorrect length ("LLintP" Bytes, "LLintP" expected)",(LLint)back[i].r.size,(LLint)back[i].r.totalsize);
                  } else {
                    //#else
                    // Un warning suffira..
                    if (cache->errlog!=NULL) {
                      fspc(cache->errlog,"warning"); fprintf(cache->errlog,"Incorrect length ("LLintP"!="LLintP" expected) for %s%s"LF,(LLint)back[i].r.size,(LLint)back[i].r.totalsize,back[i].url_adr,back[i].url_fil);
                    }
                    //#endif
                  }
                }
              }
#if BDEBUG==1
              printf("transfer ok\n");
#endif
            } else if (retour_fread > 0) {    // pas d'erreur de réception et data
              if (back[i].timeout>0) {    // refresh timeout si besoin est
                back[i].timeout_refresh=time_local();
              }

              // Traitement des en têtes chunks ou en têtes
              if (back[i].status==98 || back[i].status==97) {        // réception taille chunk en hexa (  après les en têtes, peut ne pas
                if (back[i].chunk_size > 0 && back[i].chunk_adr[back[i].chunk_size-1]==10) {
                  int chunk_size=-1;
                  char chunk_data[64];
                  if (back[i].chunk_size<32) {      // pas trop gros
                    char* chstrip=back[i].chunk_adr;
                    back[i].chunk_adr[ back[i].chunk_size-1]='\0';    // octet nul 
                    // skip leading spaces or cr
                    while(isspace(*chstrip)) chstrip++;
                    chunk_data[0] = '\0';
                    strncatbuff(chunk_data, chstrip, sizeof(chunk_data) - 2);
                    // strip chunk-extension
                    while( (chstrip = strchr(chunk_data, ';'))) *chstrip='\0';
                    while( (chstrip = strchr(chunk_data, ' '))) *chstrip='\0';
                    while( (chstrip = strchr(chunk_data, '\r'))) *chstrip='\0';
#if CHUNKDEBUG==1
                    printf("[%d] chunk received and read: %s\n",(int)back[i].r.soc,chunk_data);
#endif
                    if (back[i].r.totalsize<0)
                      back[i].r.totalsize=0;        // initialiser à 0
                    if (back[i].status==98) {   // "real" chunk
                      if (sscanf(chunk_data,"%x",&chunk_size) == 1) {
                        if (chunk_size > 0)
                          back[i].chunk_blocksize = chunk_size;  /* the data block chunk size */
                        else
                          back[i].chunk_blocksize = -1;  /* ending */
                        back[i].r.totalsize+=chunk_size;    // noter taille
                        back[i].r.adr=(char*) realloct(back[i].r.adr,(INTsys) back[i].r.totalsize + 1);
                        if (!back[i].r.adr) {
                          if (cache->errlog!=NULL) {
                            fprintf(cache->errlog,"Error: Not enough memory ("LLintP") for %s%s"LF,(LLint)back[i].r.totalsize,back[i].url_adr,back[i].url_fil);
                          }
                        }
#if CHUNKDEBUG==1
                        printf("[%d] chunk length: %d - next total "LLintP":\n",(int)back[i].r.soc,(int)chunk_size,(LLint)back[i].r.totalsize);
#endif
                      } else {
                        if (cache->errlog!=NULL) {
                          fprintf(cache->errlog,"Warning: Illegal chunk (%s) for %s%s"LF,back[i].chunk_adr,back[i].url_adr,back[i].url_fil);
                        }
                      }
                    } else {   /* back[i].status==97 : just receiving ending CRLF after data */
                      if (chunk_data[0] == '\0') {
                        if (back[i].chunk_blocksize > 0)
                          chunk_size=(int)back[i].chunk_blocksize;  /* recent data chunk size */
                        else if (back[i].chunk_blocksize == -1)
                          chunk_size=0;                        /* ending chunk */
                        else
                          chunk_size=1;                        /* fake positive size for 1st chunk history */
#if CHUNKDEBUG==1
                        printf("[%d] chunk CRLF seen\n", (int)back[i].r.soc);
#endif
                      } else {
                        if (cache->errlog!=NULL) {
                          fprintf(cache->errlog,"Warning: Illegal chunk CRLF (%s) for %s%s"LF,back[i].chunk_adr,back[i].url_adr,back[i].url_fil);
                        }
#if CHUNKDEBUG==1
                        printf("[%d] chunk CRLF ERROR!! : '%s'\n", (int)back[i].r.soc, chunk_data);
#endif
                      }
                    }
                  } else {                                  
                    if (cache->errlog!=NULL) {
                      fprintf(cache->errlog,"Warning: Chunk too big ("LLintP") for %s%s"LF,(LLint)back[i].chunk_size,back[i].url_adr,back[i].url_fil);
                    }
                  }
                    
                  // ok, continuer sur le body
                    
                  // si chunk non nul continuer (ou commencer)
                  if (back[i].status==97 && chunk_size > 0) {
                    back[i].status = 98;  /* waiting for next chunk (NN\r\n<data>\r\nNN\r\n<data>..\r\n0\r\n\r\n) */
#if CHUNKDEBUG==1
                    printf("[%d] waiting for next chunk\n", (int)back[i].r.soc);
#endif
                  } else if (back[i].status==98 && chunk_size == 0) {  /* final chunk */
                    back[i].status=97;  /* final CRLF */
#if CHUNKDEBUG==1
                    printf("[%d] waiting for final CRLF (chunk)\n", (int)back[i].r.soc);
#endif
                  } else if (back[i].status==98 && chunk_size >= 0) {  /* will fetch data now */
                    back[i].status=1;     // continuer body    
#if CHUNKDEBUG==1
                    printf("[%d] waiting for body (chunk)\n", (int)back[i].r.soc);
#endif
                  } else {                /* zero-size-chunk-CRLF (end) or error */
#if CHUNKDEBUG==1
                    printf("[%d] chunk end, total: %d\n",(int)back[i].r.soc,back[i].r.size);
#endif
                    /* End */
                    //if (back[i].status==97) {
                    back[i].status=0;     // fin  
                    //}

                    // finalize transfer
                    back_finalize(opt,cache,back,i);
                    if (back[i].r.soc!=INVALID_SOCKET) {
#if HTS_DEBUG_CLOSESOCK
                      DEBUG_W("back_wait(5): deletehttp\n");
#endif
                      /* Error */
                      if (chunk_size < 0) {
                        deletehttp(&back[i].r); back[i].r.soc=INVALID_SOCKET;
                        deleteaddr(&back[i].r);
                        back[i].r.statuscode=-1;
                        strcpybuff(back[i].r.msg,"Invalid chunk");
#if CHUNKDEBUG==1
                        printf("[%d] chunk error\n", (int)back[i].r.soc);
#endif
                      } else /* if chunk_size == 0 */ {
#if CHUNKDEBUG==1
                        printf("[%d] all chunks now received\n", (int)back[i].r.soc);
#endif
                          
                        /* Tester totalsize en fin de chunk */
                        if ((back[i].r.totalsize>0)) {    // tester totalsize
                          if (back[i].r.totalsize!=back[i].r.size) {  // pas la même!
#if HTS_CL_IS_FATAL
                            deleteaddr(&back[i].r);
                            back[i].r.statuscode=-1;
                            strcpybuff(back[i].r.msg,"Incorrect length");
#else
                            // Un warning suffira..
                            if (cache->errlog!=NULL) {
                              fspc(cache->errlog,"warning"); fprintf(cache->errlog,"Incorrect length ("LLintP"!="LLintP" expected) for %s%s"LF,(LLint)back[i].r.size,(LLint)back[i].r.totalsize,back[i].url_adr,back[i].url_fil);
                            }
#endif
                          }
                        }
                          
                        /* Oops, trailers! */
                        if (back[i].r.keep_alive_trailers) {
                          /* fixme (not yet supported) */
                        }
                          
                      }
                        
                        
                    }
                  }
                    
                  // effacer buffer (chunk en tete)
                  if (back[i].chunk_adr!=NULL) {
                    freet(back[i].chunk_adr);
                    back[i].chunk_adr=NULL;
                    back[i].chunk_size=0;
                    // NO! xxback[i].chunk_blocksize = 0;
                  }
                  
                } // taille buffer chunk > 1 && LF
                //
              } else if (back[i].status==99) {        // en têtes (avant le chunk si il est présent)
                //
                if (back[i].r.size>=2) {
                  // double LF
                  if (
                    ((back[i].r.adr[back[i].r.size-1]==10) && (back[i].r.adr[back[i].r.size-2]==10)) 
                    ||
                    (back[i].r.adr[0] == '<')    /* bogus server */
                    ) {
                    char rcvd[2048];
                    int ptr=0;
                    int noFreebuff=0;
                    
#if BDEBUG==1
                    printf("..ok, header received\n");
#endif
                    
                    
                    // Callback
#if HTS_ANALYSTE
                    if (hts_htmlcheck_receivehead != NULL) {
                      int test_head=hts_htmlcheck_receivehead(back[i].r.adr, back[i].url_adr, back[i].url_fil, back[i].referer_adr, back[i].referer_fil, &back[i].r);
                      if (test_head!=1) {
                        if ((opt->debug>0) && (opt->log!=NULL)) {
                          fspc(opt->log,"warning"); fprintf(opt->log,"External wrapper aborted transfer, breaking connection: %s%s"LF,back[i].url_adr,back[i].url_fil); test_flush;
                        }
                        back[i].status=0;  // FINI
                        deletehttp(&back[i].r); back[i].r.soc=INVALID_SOCKET;
                        strcpybuff(back[i].r.msg,"External wrapper aborted transfer");
                        back[i].r.statuscode = -1;
                      }
                    }
#endif
                    
                    /* Hack for zero-length headers */
                    if (back[i].status != 0 && back[i].r.adr[0] != '<') {
                      
                      // ----------------------------------------
                      // traiter en-tête!
                      // status-line à récupérer
                      ptr+=binput(back[i].r.adr+ptr,rcvd,2000);
                      if (strnotempty(rcvd)==0) {
                        /* Bogus CRLF, OR recycled connection and trailing chunk CRLF */
                        ptr+=binput(back[i].r.adr+ptr,rcvd,2000);
                      }
                      
                      // traiter status-line
                      treatfirstline(&back[i].r,rcvd);
                      
#if HDEBUG
                      printf("(Buffer) Status-Code=%d\n",back[i].r.statuscode);
#endif
                      if (_DEBUG_HEAD) {
                        if (ioinfo) {
                          fprintf(ioinfo,"[%d] response for %s%s:\r\ncode=%d\r\n",
                            back[i].r.debugid, jump_identification(back[i].url_adr),back[i].url_fil,back[i].r.statuscode);
                          fprintfio(ioinfo,back[i].r.adr,">>> ");
                          fprintf(ioinfo,"\r\n");
                          fflush(ioinfo);
                        }                    // en-tête
                      }
                      
                      // header // ** !attention! HTTP/0.9 non supporté
                      do {
                        ptr+=binput(back[i].r.adr+ptr,rcvd,2000);          
#if HDEBUG
                        printf("(buffer)>%s\n",rcvd);      
#endif
                        /*
                        if (_DEBUG_HEAD) {
                        if (ioinfo) {
                        fprintf(ioinfo,"(buffer)>%s\r\n",rcvd);      
                        fflush(ioinfo);
                        }
                        }
                        */
                        
                        if (strnotempty(rcvd))
                          treathead(opt->cookie,back[i].url_adr,back[i].url_fil,&back[i].r,rcvd);  // traiter
                        
                        // parfois les serveurs buggés renvoient un content-range avec un 200
                        if (back[i].r.statuscode==200)  // 'OK'
                          if (strfield(rcvd,"content-range:"))  // Avec un content-range: relisez les RFC..
                            back[i].r.statuscode=206;    // FORCER A 206 !!!!!
                          
                      } while(strnotempty(rcvd));
                      // ----------------------------------------                    

                      // libérer mémoire  -- après! --
                      deleteaddr(&back[i].r);
                    } else {
                      // assume text/html, OK
                      treatfirstline(&back[i].r, back[i].r.adr);
                      noFreebuff=1;
                    }
                      
                  
                    
                    /* 
                    Status code and header-response hacks
                    */

                    
                    // Check response : 203 == 200
                    if (back[i].r.statuscode==203) {  // 'Non-Authoritative Information'
                      back[i].r.statuscode=200;       // forcer "OK"
                    } else if (back[i].r.statuscode == 100) {
                      back[i].status=99;
                      back[i].r.size=0;
                      back[i].r.totalsize=0;
                      back[i].chunk_size=0;
                      back[i].r.statuscode=-1;
                      back[i].r.msg[0]='\0';
                      if ((opt->debug>1) && (opt->log!=NULL)) {
                        fspc(opt->log,"debug"); fprintf(opt->log,"Status 100 detected for %s%s, continuing headers"LF,back[i].url_adr,back[i].url_fil); test_flush;
                      }
                      continue;
                    }
                    
                    /*
                    Solve "false" 416 problems
                    */
                    if (back[i].r.statuscode==416) {  // 'Requested Range Not Satisfiable'
                      // Example:
                      // Range: bytes=2830-
                      // ->
                      // Content-Range: bytes */2830
                      if (back[i].range_req_size == back[i].r.crange) {
                        filenote(back[i].url_sav,NULL);
                        //xxusercommand(opt,0,NULL,back[i].url_sav,back[i].url_adr,back[i].url_fil);
                        deletehttp(&back[i].r); back[i].r.soc=INVALID_SOCKET;
                        back[i].status=0;    // READY
                        back[i].r.size=back[i].r.totalsize=back[i].range_req_size;
                        back[i].r.statuscode=304;     // NOT MODIFIED
                        if ((opt->debug>1) && (opt->log!=NULL)) {
                          fspc(opt->log,"debug"); fprintf(opt->log,"File seems complete (good 416 message), breaking connection: %s%s"LF,back[i].url_adr,back[i].url_fil); test_flush;
                        }
                      }
                    }
                    
                    // transform 406 into 200 ; we'll catch embedded links inside the choice page
                    if (back[i].r.statuscode==406) {  // 'Not Acceptable'
                      back[i].r.statuscode=200;
                    }

                    // Various hacks to limit re-transfers when updating a mirror
                    // Force update if same size detected
                    if (opt->sizehack) {
                      // We already have the file
                      // and ask the remote server for an update
                      // Some servers, especially dynamic pages severs, always
                      // answer that the page has been modified since last visit
                      // And answer with a 200 (OK) response, and the same page
                      // If the size is the same, and the option has been set, we assume
                      // that the file is identical - and therefore let's break the connection
                      if (back[i].is_update) {          // mise à jour
                        if (back[i].r.statuscode==200 && !back[i].testmode) {  // 'OK'
                          htsblk r = cache_read(opt,cache,back[i].url_adr,back[i].url_fil,NULL,NULL);    // lire entrée cache
                          if (r.statuscode == 200) {  // OK pas d'erreur cache
                            LLint len1,len2;
                            len1=r.totalsize;
                            len2=back[i].r.totalsize;
                            if (r.size>0)
                              len1=r.size;
                            if (len1>0) {
                              if (len1 == len2) {             // tailles identiques
                                back[i].r.statuscode=304;     // forcer NOT MODIFIED
                                deletehttp(&back[i].r); back[i].r.soc=INVALID_SOCKET;
                                if ((opt->debug>1) && (opt->log!=NULL)) {
                                  fspc(opt->log,"debug"); fprintf(opt->log,"File seems complete (same size), breaking connection: %s%s"LF,back[i].url_adr,back[i].url_fil); test_flush;
                                }
                              }
                            }
                          } else {
                            if (opt->errlog!=NULL) {
                              fspc(opt->errlog,"warning"); fprintf(opt->errlog,"File seems complete (same size), but there was a cache read error: %s%s"LF,back[i].url_adr,back[i].url_fil); test_flush;
                            }
                          }
                          if (r.adr) {
                            freet(r.adr);
                          }
                        }
                      }
                    }
                    
                    // Various hacks to limit re-transfers when updating a mirror
                    // Detect already downloaded file (with another browser, for example)
                    if (opt->sizehack) {
                      if (!back[i].is_update) {          // mise à jour
                        if (back[i].r.statuscode==200 && !back[i].testmode) {  // 'OK'
                          if (!is_hypertext_mime(back[i].r.contenttype)) {    // not HTML
                            if (strnotempty(back[i].url_sav)) {  // target found
                              int size = fsize(back[i].url_sav);  // target size
                              if (size >= 0) {
                                if (back[i].r.totalsize == size) {  // same size!
                                  deletehttp(&back[i].r); back[i].r.soc=INVALID_SOCKET;
                                  back[i].status=0;    // READY
                                  back[i].r.size=back[i].r.totalsize;
                                  filenote(back[i].url_sav,NULL);
                                  //xxusercommand(opt,0,NULL,back[i].url_sav,back[i].url_adr,back[i].url_fil);
                                  back[i].r.statuscode=304;     // NOT MODIFIED
                                  if ((opt->debug>1) && (opt->log!=NULL)) {
                                    fspc(opt->log,"debug"); fprintf(opt->log,"File seems complete (same size file discovered), breaking connection: %s%s"LF,back[i].url_adr,back[i].url_fil); test_flush;
                                  }
                                }
                              }
                            }
                          }
                        }
                      }
                    }
                    
                    // Various hacks to limit re-transfers when updating a mirror
                    // Detect bad range: header
                    if (opt->sizehack) {
                      // We have request for a partial file (with a 'Range: NNN-' header)
                      // and received a complete file notification (200), with 'Content-length: NNN'
                      // it might be possible that we had the complete file
                      // this is the case in *most* cases, so break the connection
                      if (back[i].r.is_write==0) {  // mode mémoire
                        if (back[i].r.adr==NULL) {  // rien n'a été écrit
                          if (!back[i].testmode) {  // pas mode test
                            if (strnotempty(back[i].url_sav)) {
                              if (strcmp(back[i].url_fil,"/robots.txt")) {
                                if (back[i].r.statuscode==200) {  // 'OK'
                                  if (!is_hypertext_mime(back[i].r.contenttype)) {    // pas HTML
                                    if (back[i].r.statuscode==200) {      // "OK"
                                      if (back[i].range_req_size>0) {     // but Range: requested
                                        if (back[i].range_req_size == back[i].r.totalsize) {    // And same size
#if HTS_DEBUG_CLOSESOCK
                                          DEBUG_W("back_wait(skip_range): deletehttp\n");
#endif
                                          deletehttp(&back[i].r); back[i].r.soc=INVALID_SOCKET;
                                          back[i].status=0;    // READY
                                          back[i].r.size=back[i].r.totalsize;
                                          filenote(back[i].url_sav,NULL);
                                          //xxusercommand(opt,0,NULL,back[i].url_sav,back[i].url_adr,back[i].url_fil);
                                          back[i].r.statuscode=304;     // NOT MODIFIED
                                          if ((opt->debug>1) && (opt->log!=NULL)) {
                                            fspc(opt->log,"debug"); fprintf(opt->log,"File seems complete (reget failed), breaking connection: %s%s"LF,back[i].url_adr,back[i].url_fil); test_flush;
                                          }
                                        }
                                      }
                                    }
                                    
                                  }
                                }
                              }
                            }
                          }
                        }
                      }
                    }
                    // END - Various hacks to limit re-transfers when updating a mirror

                    /* 
                    End of status code and header-response hacks
                    */

                    
                    
                    /* Interdiction taille par le wizard? */
                    if (back[i].r.soc!=INVALID_SOCKET) {
                      if (!back_checksize(opt,&back[i],1)) {
                        back[i].status=0;  // FINI
                        deletehttp(&back[i].r); back[i].r.soc=INVALID_SOCKET;
                        if (!back[i].testmode)
                          strcpybuff(back[i].r.msg,"File too big");
                        else
                          strcpybuff(back[i].r.msg,"Test: File too big");
                      }
                    }
                    
                    /* sinon, continuer */
                    /* if (back[i].r.soc!=INVALID_SOCKET) {   // ok récupérer body? */
                    // head: terminé
                    if (back[i].head_request) {
                      if ((opt->debug>1) && (opt->log!=NULL)) {
                        fspc(opt->log,"debug"); fprintf(opt->log,"Tested file: %s%s"LF,back[i].url_adr,back[i].url_fil); test_flush;
                      }
#if HTS_DEBUG_CLOSESOCK
                      DEBUG_W("back_wait(head request): deletehttp\n");
#endif
                      // Couper connexion
                      if (!back[i].http11) {    /* NO KA */
                        deletehttp(&back[i].r); back[i].r.soc=INVALID_SOCKET;
                      }
                      back[i].status=0;  // terminé
                    }
                    // traiter une éventuelle erreur 304 (cache à jour utilisable)
                    else if (back[i].r.statuscode==304) {  // document à jour dans le cache
                      // lire dans le cache
                      // ** NOTE: pas de vérif de la taille ici!!
#if HTS_DEBUG_CLOSESOCK
                      DEBUG_W("back_wait(file is not modified): deletehttp\n");
#endif
                      /* clear everything but connection: switch, close, and reswitch */
                      {
                        htsblk tmp;
                        memset(&tmp, 0, sizeof(tmp));
                        back_connxfr(&back[i].r, &tmp);
                        back[i].r=cache_read(opt,cache,back[i].url_adr,back[i].url_fil,back[i].url_sav,back[i].location_buffer);
                        back[i].r.location=back[i].location_buffer;
                        back_connxfr(&tmp,&back[i].r);
                      }

                      // hack:
                      // In case of 'if-unmodified-since' hack, a 304 status can be sent
                      // then, force 'ok' status
                      if (back[i].r.statuscode == -1) {
                        if (fexist(back[i].url_sav)) {
                          back[i].r.statuscode=200;     // OK
                          strcpybuff(back[i].r.msg, "OK (cached)");
                          back[i].r.is_file=1;
                          back[i].r.totalsize = back[i].r.size = fsize(back[i].url_sav);
                          get_httptype(back[i].r.contenttype, back[i].url_sav, 1);
                          if ((opt->debug>0) && (opt->log!=NULL)) {
                            fspc(opt->log,"debug"); fprintf(opt->log,"Not-modified status without cache guessed: %s%s"LF,back[i].url_adr,back[i].url_fil); test_flush;
                          }
                        }
                      }

                      // Status is okay?
                      if (back[i].r.statuscode!=-1) { // pas d'erreur de lecture
                        back[i].status=0;         // OK prêt
                        back[i].r.notmodified=1;  // NON modifié!
                        if ((opt->debug>0) && (opt->log!=NULL)) {
                          fspc(opt->log,"debug"); fprintf(opt->log,"File loaded after test from cache: %s%s"LF,back[i].url_adr,back[i].url_fil); test_flush;
                        }

                        // finalize
                        if (back[i].r.statuscode>0) {
                          back_finalize(opt,cache,back,i);
                        }
                        
#if DEBUGCA
                        printf("..document à jour après requète: %s%s\n",back[i].url_adr,back[i].url_fil);
#endif
                        
                        //printf(">%s status %d\n",back[p].r.contenttype,back[p].r.statuscode);
                      } else {  // erreur
                        back[i].status=0;  // terminé
                        //printf("erreur cache\n");
                        
                      } 
                      
/********** NO - must complete the body! ********** */
#if 0
                    } else if ((back[i].r.statuscode==301)
                      || (back[i].r.statuscode==302)
                      || (back[i].r.statuscode==303)
                      || (back[i].r.statuscode==307)
                      || (back[i].r.statuscode==412)
                      || (back[i].r.statuscode==416)
                      ) {   // Ne pas prendre le html, erreurs connues et gérées
#if HTS_DEBUG_CLOSESOCK
                      DEBUG_W("back_wait(301,302,303,307,412,416..): deletehttp\n");
#endif
                      // Couper connexion
                      /*KA deletehttp(&back[i].r); back[i].r.soc=INVALID_SOCKET;*/
                      back_maydeletehttp(opt, back, back_max, i);

                      back[i].status=0;  // terminé
                      // finalize
                      if (back[i].r.statuscode>0) {
                        back_finalize(opt,cache,back,i);
                      }
#endif
/********** **************************** ********** */
                    } else {    // il faut aller le chercher
                      
                      // effacer buffer (requète)
                      if (!noFreebuff) {
                        deleteaddr(&back[i].r);
                        back[i].r.size=0;
                      }
                      
                      // traiter 206 (partial content)
                      // xxc SI CHUNK VERIFIER QUE CA MARCHE??
                      if (back[i].r.statuscode==206) {  // on nous envoie un morceau (la fin) coz une partie sur disque!
                        LLint sz=fsize(back[i].url_sav);
#if HDEBUG
                        printf("partial content: "LLintP" on disk..\n",(LLint)sz);
#endif
                        if (sz>=0) {
                          if (!is_hypertext_mime(back[i].r.contenttype)) {    // pas HTML
                            if (opt->getmode&2) {    // on peut ecrire des non html  **sinon ben euhh sera intercepté plus loin, donc rap sur ce qui va sortir**
                              filenote(back[i].url_sav,NULL);    // noter fichier comme connu
                              back[i].r.out=fopen(fconv(back[i].url_sav),"ab");  // append
                              if (back[i].r.out) {
                                back[i].r.is_write=1;    // écrire
                                back[i].r.size=sz;    // déja écrit
                                back[i].r.statuscode=200;  // Forcer 'OK'
                                if (back[i].r.totalsize>0)
                                  back[i].r.totalsize+=sz;    // plus en fait
                                fseek(back[i].r.out,0,SEEK_END);  // à la fin
#if HDEBUG
                                printf("continue interrupted file\n");
#endif
                              } else {    // On est dans la m**
                                back[i].status=0;  // terminé (voir plus loin)
                                strcpybuff(back[i].r.msg,"Can not open partial file");
                              }
                            }
                          } else {    // mémoire
                            FILE* fp=fopen(fconv(back[i].url_sav),"rb");
                            if (fp) {
                              LLint alloc_mem=sz + 1;
                              if (back[i].r.totalsize>0)
                                alloc_mem+=back[i].r.totalsize;            // AJOUTER RESTANT!
                              if ( deleteaddr(&back[i].r) && (back[i].r.adr=(char*) malloct((INTsys) alloc_mem)) ) {
                                back[i].r.size=sz;
                                if (back[i].r.totalsize>0)
                                  back[i].r.totalsize+=sz;    // plus en fait
                                if (( fread(back[i].r.adr,1,(INTsys)sz,fp)) != sz) {
                                  back[i].status=0;  // terminé (voir plus loin)
                                  strcpybuff(back[i].r.msg,"Can not read partial file");
                                } else {
                                  back[i].r.statuscode=200;  // Forcer 'OK'
#if HDEBUG
                                  printf("continue in mem interrupted file\n");
#endif
                                }
                              } else {
                                back[i].status=0;  // terminé (voir plus loin)
                                strcpybuff(back[i].r.msg,"No memory for partial file");
                              }
                              fclose(fp);
                            } else {  // Argh.. 
                              back[i].status=0;  // terminé (voir plus loin)
                              strcpybuff(back[i].r.msg,"Can not open partial file");
                            }
                          }
                        } else {    // Non trouvé??
                          back[i].status=0;  // terminé (voir plus loin)
                          strcpybuff(back[i].r.msg,"Can not find partial file");
                        }
                        // Erreur?
                        if (back[i].status==0) {
                          if (back[i].r.soc!=INVALID_SOCKET) {
#if HTS_DEBUG_CLOSESOCK
                            DEBUG_W("back_wait(206 solve problems): deletehttp\n");
#endif
                            deletehttp(&back[i].r);
                          }
                          back[i].r.soc=INVALID_SOCKET;
                          //back[i].r.statuscode=206;  ????????
                          back[i].r.statuscode=-5;
                          if (strnotempty(back[i].r.msg))
                            strcpybuff(back[i].r.msg,"Error attempting to solve status 206 (partial file)");
                        }
                      }
                      
                      if (back[i].status!=0) {  // non terminé (erreur)
                        if (!back[i].testmode) {    // fichier normal
                          
                          if (back[i].r.empty && back[i].r.statuscode==200) {  // empty response
                            // Couper connexion
                            deletehttp(&back[i].r); back[i].r.soc=INVALID_SOCKET;
                            back[i].status=0;  // terminé
                            if ( deleteaddr(&back[i].r) && (back[i].r.adr=(char*) malloct((INTsys) 2)) ) {
                              back[i].r.adr[0] = 0;
                            }
                            back_finalize(opt,cache,back,i);
                          }
                          else if (!back[i].r.is_chunk) {    // pas de chunk
                            //if (back[i].r.http11!=2) {    // pas de chunk
                            back[i].is_chunk=0;
                            back[i].status=1;     // start body
                          } else {
#if CHUNKDEBUG==1
                            printf("[%d] chunk encoding detected %s..\n",(int)back[i].r.soc, back[i].url_fil);
#endif
                            back[i].is_chunk=1;
                            back[i].chunk_adr=NULL;
                            back[i].chunk_size=0;
                            back[i].chunk_blocksize=0;
                            back[i].status=98;    // start body wait chunk
                            back[i].r.totalsize=0;   /* devalidate size! (rfc) */
                          }
                          if (back[i].rateout>0) {
                            back[i].rateout_time=time_local();  // refresh pour transfer rate
                          }
#if HDEBUG
                          printf("(buffer) start body!\n");
#endif
                        } else {     // mode test, ne pas passer en 1!!
                          back[i].status=0;    // READY
#if HTS_DEBUG_CLOSESOCK
                          DEBUG_W("back_wait(test ok): deletehttp\n");
#endif
                          deletehttp(&back[i].r); back[i].r.soc=INVALID_SOCKET;
                          if (back[i].r.statuscode==200) {
                            strcpybuff(back[i].r.msg,"Test: OK");
                            back[i].r.statuscode=-10;    // test réussi
                          }
                          else {    // test a échoué, on ne change rien sauf que l'erreur est à titre indicatif
                            char tempo[1000];
                            strcpybuff(tempo,back[i].r.msg);
                            strcpybuff(back[i].r.msg,"Test: ");
                            strcatbuff(back[i].r.msg,tempo);
                          }
                          
                        }
                      }
                      
                      } 
                      
                      /*}*/
                      
                  }  // si LF
                }  // r.size>2
              }  // si == 99
              
            } // si pas d'erreurs
#if BDEBUG==1
            printf("bytes overall: %d\n",back[i].r.size);
#endif
          }  // données dispo
          
          // en cas d'erreur cl, supprimer éventuel fichier sur disque
#if HTS_REMOVE_BAD_FILES
          if (back[i].status<0) {
            if (!back[i].testmode) {    // pas en test
              remove(back[i].url_sav);    // éliminer fichier (endommagé)
              //printf("&& %s\n",back[i].url_sav);
            }
          }
#endif

          /* funny log for commandline users */
          //if (!opt->quiet) {  
          // petite animation
          if (opt->verbosedisplay==1) {
            if (back[i].status==0) {
              if (back[i].r.statuscode==200)
                printf("* %s%s ("LLintP" bytes) - OK"VT_CLREOL"\r",back[i].url_adr,back[i].url_fil,(LLint)back[i].r.size);
              else
                printf("* %s%s ("LLintP" bytes) - %d"VT_CLREOL"\r",back[i].url_adr,back[i].url_fil,(LLint)back[i].r.size,back[i].r.statuscode);
              fflush(stdout);
            }
          }
          //}
          

      } // status>0
    }  // for
    
    // vérifier timeouts
    if (gestion_timeout) {
      TStamp act;
      act=time_local();    // temps en secondes
      for(i=0;i<back_max;i++) {
        if (back[i].status>0) {  // réception/connexion/..
          if (back[i].timeout>0) {
            //printf("time check %d\n",((int) (act-back[i].timeout_refresh))-back[i].timeout);
            if (((int) (act-back[i].timeout_refresh))>=back[i].timeout) {
              if (back[i].r.soc!=INVALID_SOCKET) {
#if HTS_DEBUG_CLOSESOCK
                DEBUG_W("back_wait(timeout): deletehttp\n");
#endif
                deletehttp(&back[i].r);
              }
              back[i].r.soc=INVALID_SOCKET;
              back[i].r.statuscode=-2;
              if (back[i].status==100)
                strcpybuff(back[i].r.msg,"Connect Time Out");
              else if (back[i].status==101)
                strcpybuff(back[i].r.msg,"DNS Time Out");
              else
                strcpybuff(back[i].r.msg,"Receive Time Out");
              back[i].status=0;  // terminé
            } else if ((back[i].rateout>0) && (back[i].status<99)) {
              if (((int) (act-back[i].rateout_time))>=HTS_WATCHRATE) {   // checker au bout de 15s
                if ( (int) ((back[i].r.size)/(act-back[i].rateout_time)) < back[i].rateout ) {  // trop lent
                  back[i].status=0;  // terminé
                  if (back[i].r.soc!=INVALID_SOCKET) {
#if HTS_DEBUG_CLOSESOCK
                    DEBUG_W("back_wait(rateout): deletehttp\n");
#endif
                    deletehttp(&back[i].r);
                  }
                  back[i].r.soc=INVALID_SOCKET;
                  back[i].r.statuscode=-3;
                  strcpybuff(back[i].r.msg,"Transfer Rate Too Low");
                }
              }
            }
          }
        }
      }
    }
    max_loop--;
#if HTS_ANALYSTE
    max_loop_chk++;
#endif
  } while((busy_state) && (busy_recv) && (max_loop>0));
#if HTS_ANALYSTE
  if ((!busy_recv) && (!busy_state)) {
    if (max_loop_chk>=1) {
      Sleep(10);    // un tite pause pour éviter les lag..
    }
  }
#endif
}

int back_checksize(httrackp* opt,lien_back* eback,int check_only_totalsize) {
  LLint size_to_test;
  if (check_only_totalsize)
    size_to_test=eback->r.totalsize;
  else
    size_to_test=max(eback->r.totalsize,eback->r.size);
  if (size_to_test>=0) {
    
    /* Interdiction taille par le wizard? */
    if (hts_testlinksize(opt,eback->url_adr,eback->url_fil,eback->r.totalsize/1024)==-1) {
      return 0;     /* interdit */
    }                     
    
    /* vérifier taille classique (heml et non html) */
    if ((istoobig(size_to_test,eback->maxfile_html,eback->maxfile_nonhtml,eback->r.contenttype))) {
      return 0;     /* interdit */
    }
  }
  return 1;
}

int back_checkmirror(httrackp* opt) {
  // Check max time
  if ((opt->maxsite>0) && (HTS_STAT.stat_bytes >= opt->maxsite)) {
    if (opt->errlog) {
      fprintf(opt->errlog,"More than "LLintP" bytes have been transfered.. giving up"LF,(LLint)opt->maxsite);
      test_flush;
    } 
    return 0;
  } else if ((opt->maxtime>0) && ((time_local()-HTS_STAT.stat_timestart)>opt->maxtime)) {            
    if (opt->errlog) {
      fprintf(opt->errlog,"More than %d seconds passed.. giving up"LF,opt->maxtime);
      test_flush;
    } 
    return 0;
  }
  return 1;   /* Ok, go on */
}


// octets transférés + add
LLint back_transfered(LLint nb,lien_back* back,int back_max) {
  int i;
  // ajouter octets en instance
  for(i=0;i<back_max;i++)
    if ((back[i].status>0) && (back[i].status<99 || back[i].status>=1000))
      nb+=back[i].r.size;
  return nb;      
}

// infos backing
// j: 1 afficher sockets 2 afficher autres 3 tout afficher
void back_info(lien_back* back,int i,int j,FILE* fp) {
  if (back[i].status>=0) {
    char s[HTS_URLMAXSIZE*2+1024]; 
    s[0]='\0';
    back_infostr(back,i,j,s);
    strcatbuff(s,LF);
    fprintf(fp,"%s",s);
  }
}

// infos backing
// j: 1 afficher sockets 2 afficher autres 3 tout afficher
void back_infostr(lien_back* back,int i,int j,char* s) {
  if (back[i].status>=0) {
    int aff=0;
    if (j & 1) {
      if (back[i].status==100) {
        strcatbuff(s,"CONNECT ");
      } else if (back[i].status==99) {
        strcatbuff(s,"INFOS ");
        aff=1;
      } else if (back[i].status==98 || back[i].status==97) {
        strcatbuff(s,"INFOSC");             // infos chunk
        aff=1;
      }
      else if (back[i].status>0) {
#if HTS_ANALYSTE==2
        strcatbuff(s,"WAIT ");
#else
        strcatbuff(s,"RECEIVE "); 
#endif
        aff=1; 
      }
    } 
    if (j & 2) {
      if (back[i].status==0) {
        switch (back[i].r.statuscode) {
        case 200:
          strcatbuff(s,"READY ");
          aff=1;
          break;
#if HTS_ANALYSTE==2
        default:
          strcatbuff(s,"ERROR ");
          break;
#else
        case -1:
          strcatbuff(s,"ERROR ");
          aff=1;
          break;
        case -2:
          strcatbuff(s,"TIMEOUT ");
          aff=1;
          break;
        case -3:
          strcatbuff(s,"TOOSLOW ");
          aff=1;
          break;
        case 400:
          strcatbuff(s,"BADREQUEST ");
          aff=1;
          break;
        case 401: case 403:
          strcatbuff(s,"FORBIDDEN ");
          aff=1;
          break;
        case 404:
          strcatbuff(s,"NOT FOUND ");
          aff=1;
          break;
        case 500:
          strcatbuff(s,"SERVERROR ");
          aff=1;
          break;
        default:
          {
            char s2[256];
            sprintf(s2,"ERROR(%d)",back[i].r.statuscode);
            strcatbuff(s,s2);
          }
          aff=1;
#endif
        }
      }
    }
    
    if (aff) {
      {
        char s2[HTS_URLMAXSIZE*2+1024];
        sprintf(s2,"\"%s",back[i].url_adr); strcatbuff(s,s2);
        
        if (back[i].url_fil[0]!='/') strcatbuff(s,"/");
        sprintf(s2,"%s\" ",back[i].url_fil); strcatbuff(s,s2);
        sprintf(s,LLintP" "LLintP" ",(LLint)back[i].r.size,(LLint)back[i].r.totalsize); strcatbuff(s,s2);
      }
    }
  }
}

// -- backing --

#undef test_flush
