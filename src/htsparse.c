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
/* File: htsparse.c parser                                      */
/*       html/javascript/css parser                             */
/*       and other parser routines                              */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */


/* Internal engine bytecode */
#define HTS_INTERNAL_BYTECODE

#ifndef  _WIN32_WCE
#include <fcntl.h>
#endif
#include <ctype.h>

/* File defs */
#include "htscore.h"

/* specific definitions */
#include "htsbase.h"
#include "htsnet.h"
#include "htsbauth.h"
#include "htsmd5.h"
#include "htsindex.h"
#include "htscharset.h"

/* external modules */
#include "htsmodules.h"

// htswrap_add
#include "htswrap.h"

// parser
#include "htsparse.h"
#include "htsback.h"

// specific defines
#define urladr   (liens[ptr]->adr)
#define urlfil   (liens[ptr]->fil)
#define savename (liens[ptr]->sav)
#define parenturladr   (liens[liens[ptr]->precedent]->adr)
#define parenturlfil   (liens[liens[ptr]->precedent]->fil)
#define parentsavename (liens[liens[ptr]->precedent]->sav)
#define relativeurladr   ((!parent_relative)?urladr:parenturladr)
#define relativeurlfil   ((!parent_relative)?urlfil:parenturlfil)
#define relativesavename ((!parent_relative)?savename:parentsavename)

#define test_flush if (opt->flush) { if (opt->log) { fflush(opt->log); } if (opt->log) { fflush(opt->log);  } }

// does nothing
#define XH_uninit do {} while(0)

// version optimisée, qui permet de ne pas toucher aux html non modifiés (update)
#define REALLOC_SIZE 8192
#define HT_ADD_CHK(A) if (((int) (A)+ht_len+1) >= ht_size) { \
  ht_size=(A)+ht_len+REALLOC_SIZE; \
  ht_buff=(char*) realloct(ht_buff,ht_size); \
  if (ht_buff==NULL) { \
  printf("PANIC! : Not enough memory [%d]\n",__LINE__); \
  XH_uninit; \
  abortLogFmt("not enough memory for current html document in HT_ADD_CHK : realloct(%d) failed" _ ht_size); \
  abort(); \
  } \
} \
  ht_len+=A;
#define HT_ADD_ADR \
  if ((opt->getmode & 1) && (ptr>0)) { \
  size_t i = ((size_t) (adr - lastsaved)),j=ht_len; HT_ADD_CHK(i) \
  memcpy(ht_buff+j, lastsaved, i); \
  ht_buff[j+i]='\0'; \
  lastsaved=adr; \
  }
#define HT_ADD(A) \
  if ((opt->getmode & 1) && (ptr>0)) { \
  size_t i_ = strlen(A), j_ = ht_len; \
  if (i_) { \
  HT_ADD_CHK(i_) \
  memcpy(ht_buff+j_, A, i_); \
  ht_buff[j_+i_]='\0'; \
  } }
#define HT_ADD_HTMLESCAPED(A) \
  if ((opt->getmode & 1) && (ptr>0)) { \
    size_t i_, j_; \
    char BIGSTK tempo_[HTS_URLMAXSIZE*2]; \
    escape_for_html_print(A, tempo_); \
    i_=strlen(tempo_); \
    j_=ht_len; \
    if (i_) { \
    HT_ADD_CHK(i_) \
    memcpy(ht_buff+j_, tempo_, i_); \
    ht_buff[j_+i_]='\0'; \
  } }
#define HT_ADD_HTMLESCAPED_FULL(A) \
  if ((opt->getmode & 1) && (ptr>0)) { \
    size_t i_, j_; \
    char BIGSTK tempo_[HTS_URLMAXSIZE*2]; \
    escape_for_html_print_full(A, tempo_); \
    i_=strlen(tempo_); \
    j_=ht_len; \
    if (i_) { \
    HT_ADD_CHK(i_) \
    memcpy(ht_buff+j_, tempo_, i_); \
    ht_buff[j_+i_]='\0'; \
  } }
#define HT_ADD_START \
  size_t ht_size=(size_t)(r->size*5)/4+REALLOC_SIZE; \
  size_t ht_len=0; \
  char* ht_buff=NULL; \
  if ((opt->getmode & 1) && (ptr>0)) { \
  ht_buff=(char*) malloct(ht_size); \
  if (ht_buff==NULL) { \
  printf("PANIC! : Not enough memory [%d]\n",__LINE__); \
  XH_uninit; \
  abortLogFmt("not enough memory for current html document in HT_ADD_START : malloct(%d) failed" _ (int) ht_size); \
  abort(); \
  } \
  ht_buff[0]='\0'; \
  }
#define HT_ADD_END { \
  int ok=0;\
  if (ht_buff) { \
    char digest[32+2];\
    off_t fsize_old = fsize(fconv(OPT_GET_BUFF(opt),savename));\
    digest[0]='\0';\
    domd5mem(ht_buff,ht_len,digest,1);\
    if (fsize_old==ht_len) { \
      int mlen = 0;\
      char* mbuff;\
      cache_readdata(cache,"//[HTML-MD5]//",savename,&mbuff,&mlen);\
      if (mlen) \
        mbuff[mlen]='\0';\
      if ((mlen == 32) && (strcmp(((mbuff!=NULL)?mbuff:""),digest)==0)) {\
        ok=1;\
        if ( (opt->debug>1) && (opt->log!=NULL) ) {\
          HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"File not re-written (md5): %s"LF,savename);\
          test_flush;\
        }\
      } else {\
        ok=0;\
      } \
    }\
    if (!ok) { \
      file_notify(opt,urladr, urlfil, savename, 1, 1, r->notmodified); \
      fp=filecreate(&opt->state.strc, savename); \
      if (fp) { \
        if (ht_len>0) {\
        if (fwrite(ht_buff,1,ht_len,fp) != ht_len) { \
          int fcheck;\
          if ((fcheck=check_fatal_io_errno())) {\
            opt->state.exit_xh=-1;\
          }\
          if (opt->log) {   \
            int last_errno = errno; \
            HTS_LOG(opt,LOG_ERROR); fprintf(opt->log,"Unable to write HTML file %s: %s"LF, savename, strerror(last_errno));\
            if (fcheck) {\
              HTS_LOG(opt,LOG_ERROR);\
              fprintf(opt->log,"* * Fatal write error, giving up"LF);\
            }\
            test_flush;\
          }\
        }\
        }\
        fclose(fp); fp=NULL; \
        if (strnotempty(r->lastmodified)) \
        set_filetime_rfc822(savename,r->lastmodified); \
      } else {\
        int fcheck;\
        if ((fcheck=check_fatal_io_errno())) {\
  				HTS_LOG(opt,LOG_ERROR); fprintf(opt->log,"Mirror aborted: disk full or filesystem problems"LF); \
					test_flush; \
          opt->state.exit_xh=-1;\
        }\
        if (opt->log) { \
          int last_errno = errno; \
          HTS_LOG(opt,LOG_ERROR);\
          fprintf(opt->log,"Unable to save file %s : %s"LF, savename, strerror(last_errno));\
          if (fcheck) {\
            HTS_LOG(opt,LOG_ERROR);\
            fprintf(opt->log,"* * Fatal write error, giving up"LF);\
          }\
          test_flush;\
        }\
      }\
    } else {\
      file_notify(opt,urladr, urlfil, savename, 0, 0, r->notmodified); \
      filenote(&opt->state.strc, savename,NULL); \
    }\
    if (cache->ndx)\
      cache_writedata(cache->ndx,cache->dat,"//[HTML-MD5]//",savename,digest,(int)strlen(digest));\
  } \
  freet(ht_buff); ht_buff=NULL; \
}
#define HT_ADD_FOP 

// COPY IN HTSCORE.C
#define HT_INDEX_END do { \
  if (!makeindex_done) { \
  if (makeindex_fp) { \
  char BIGSTK tempo[1024]; \
  if (makeindex_links == 1) { \
  char BIGSTK link_escaped[HTS_URLMAXSIZE*2]; \
  strcpybuff(link_escaped, makeindex_firstlink); \
  escape_check_url(link_escaped); \
  sprintf(tempo,"<meta HTTP-EQUIV=\"Refresh\" CONTENT=\"0; URL=%s\">"CRLF,link_escaped); \
  } else \
  tempo[0]='\0'; \
  fprintf(makeindex_fp,template_footer, \
  "<!-- Mirror and index made by HTTrack Website Copier/"HTTRACK_VERSION" "HTTRACK_AFF_AUTHORS" -->", \
  tempo \
  ); \
  fflush(makeindex_fp); \
  fclose(makeindex_fp);  /* à ne pas oublier sinon on passe une nuit blanche */  \
  makeindex_fp=NULL; \
  usercommand(opt,0,NULL,fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_html_utf8),"index.html"),"primary","primary");  \
  } \
  } \
  makeindex_done=1;    /* ok c'est fait */  \
} while(0)

// Enregistrement d'un lien:
// on calcule la taille nécessaire: taille des 3 chaînes à stocker (taille forcée paire, plus 2 octets de sécurité)
// puis on vérifie qu'on a assez de marge dans le buffer - sinon on en réalloue un autre
// enfin on écrit à l'adresse courante du buffer, qu'on incrémente. on décrémente la taille dispo d'autant ensuite
// codebase: si non nul et si .class stockee on le note pour chemin primaire pour classes
// FA,FS: former_adr et former_fil, lien original
#define liens_record_sav_len(A) 

// COPIE DE HTSCORE.C

#define liens_record(A,F,S,FA,FF) { \
  int notecode=0; \
  size_t lienurl_len=((sizeof(lien_url)+HTS_ALIGN-1)/HTS_ALIGN)*HTS_ALIGN,\
  adr_len=strlen(A),\
  fil_len=strlen(F),\
  sav_len=strlen(S),\
  cod_len=0,\
  former_adr_len=strlen(FA),\
  former_fil_len=strlen(FF); \
  if (former_adr_len>0) {\
    former_adr_len=(former_adr_len/HTS_ALIGN)*HTS_ALIGN+HTS_ALIGN*2; \
    former_fil_len=(former_fil_len/HTS_ALIGN)*HTS_ALIGN+HTS_ALIGN*2; \
  } else \
    former_adr_len=former_fil_len=0;\
  if (strlen(F)>6) if (strnotempty(codebase)) if (strfield(F+strlen(F)-6,".class")) {\
    notecode=1; \
    cod_len=strlen(codebase); \
    cod_len=(cod_len/HTS_ALIGN)*HTS_ALIGN+HTS_ALIGN*2; \
  } \
  adr_len=(adr_len/HTS_ALIGN)*HTS_ALIGN+HTS_ALIGN*2; \
  fil_len=(fil_len/HTS_ALIGN)*HTS_ALIGN+HTS_ALIGN*2; \
  sav_len=(sav_len/HTS_ALIGN)*HTS_ALIGN+HTS_ALIGN*2; \
  if ((int) lien_size < (int) (adr_len+fil_len+sav_len+cod_len+former_adr_len+former_fil_len+lienurl_len)) { \
    lien_buffer=(char*) ((void*) calloct(add_tab_alloc,1)); \
    lien_size=add_tab_alloc; \
    if (lien_buffer!=NULL) { \
    liens[lien_tot]=(lien_url*) (void*) lien_buffer; lien_buffer+=lienurl_len; lien_size-=lienurl_len; \
    liens[lien_tot]->firstblock=1; \
    } \
  } else { \
    liens[lien_tot]=(lien_url*) (void*) lien_buffer; lien_buffer+=lienurl_len; lien_size-=lienurl_len; \
    liens[lien_tot]->firstblock=0; \
  } \
  if (liens[lien_tot]!=NULL) { \
    liens[lien_tot]->adr=lien_buffer; lien_buffer+=adr_len; lien_size-=adr_len; \
    liens[lien_tot]->fil=lien_buffer; lien_buffer+=fil_len; lien_size-=fil_len; \
    liens[lien_tot]->sav=lien_buffer; lien_buffer+=sav_len; lien_size-=sav_len; \
    liens[lien_tot]->cod=NULL; \
    if (notecode) { \
      liens[lien_tot]->cod=lien_buffer; \
      lien_buffer+=cod_len; \
      lien_size-=cod_len; \
      strcpybuff(liens[lien_tot]->cod,codebase); \
    } \
    if (former_adr_len>0) {\
      liens[lien_tot]->former_adr=lien_buffer; lien_buffer+=former_adr_len; lien_size-=former_adr_len; \
      liens[lien_tot]->former_fil=lien_buffer; lien_buffer+=former_fil_len; lien_size-=former_fil_len; \
      strcpybuff(liens[lien_tot]->former_adr,FA); \
      strcpybuff(liens[lien_tot]->former_fil,FF); \
    }\
    strcpybuff(liens[lien_tot]->adr,A); \
    strcpybuff(liens[lien_tot]->fil,F); \
    strcpybuff(liens[lien_tot]->sav,S); \
    liens_record_sav_len(liens[lien_tot]); \
    hash_write(hashptr,lien_tot,opt->urlhack);  \
  } \
}

#define ENGINE_DEFINE_CONTEXT() \
  ENGINE_DEFINE_CONTEXT_BASE(); \
  /* */ \
  htsblk* const r HTS_UNUSED = stre->r_; \
  hash_struct* const hash HTS_UNUSED = stre->hash_; \
  char* const codebase HTS_UNUSED = stre->codebase; \
  char* const base HTS_UNUSED = stre->base; \
  /* */ \
  const char * const template_header HTS_UNUSED = stre->template_header_; \
  const char * const template_body HTS_UNUSED = stre->template_body_; \
  const char * const template_footer HTS_UNUSED = stre->template_footer_; \
  /* */ \
  char* const makeindex_firstlink = stre->makeindex_firstlink_; \
  /* */ \
  /* */ \
  int error = * stre->error_; \
  int store_errpage = * stre->store_errpage_; \
  int lien_max = *stre->lien_max_; \
  /* */ \
  int makeindex_done = *stre->makeindex_done_; \
  FILE* makeindex_fp = *stre->makeindex_fp_; \
  int makeindex_links = *stre->makeindex_links_; \
  /* */ \
  LLint stat_fragment = *stre->stat_fragment_; \
  TStamp makestat_time = stre->makestat_time; \
  FILE* makestat_fp = stre->makestat_fp

#define ENGINE_SET_CONTEXT() \
  ENGINE_SET_CONTEXT_BASE(); \
  /* */ \
  error = * stre->error_; \
  store_errpage = * stre->store_errpage_; \
  lien_max = *stre->lien_max_; \
  /* */ \
  makeindex_done = *stre->makeindex_done_; \
  makeindex_fp = *stre->makeindex_fp_; \
  makeindex_links = *stre->makeindex_links_; \
  /* */ \
  stat_fragment = *stre->stat_fragment_; \
  makestat_time = stre->makestat_time; \
  makestat_fp = stre->makestat_fp

#define ENGINE_LOAD_CONTEXT() \
  ENGINE_DEFINE_CONTEXT()

#define ENGINE_SAVE_CONTEXT() \
  ENGINE_SAVE_CONTEXT_BASE(); \
  /* */ \
  * stre->error_ = error; \
  * stre->store_errpage_ = store_errpage; \
  * stre->lien_max_ = lien_max; \
  /* */ \
  *stre->makeindex_done_ = makeindex_done; \
  *stre->makeindex_fp_ = makeindex_fp; \
  *stre->makeindex_links_ = makeindex_links; \
  /* */ \
  *stre->stat_fragment_ = stat_fragment

#define _FILTERS     (*opt->filters.filters)
#define _FILTERS_PTR (opt->filters.filptr)
#define _ROBOTS      ((robots_wizard*)opt->robotsptr)

/* Apply current *adr character for the script automate */
#define AUTOMATE_LOOKUP_CURRENT_ADR() do { \
  if (inscript) { \
  int new_state_pos; \
  new_state_pos=inscript_state[inscript_state_pos][(unsigned char)*adr]; \
  if (new_state_pos < 0) { \
  new_state_pos=inscript_state[inscript_state_pos][INSCRIPT_DEFAULT]; \
  } \
  assertf(new_state_pos >= 0); \
  assertf(new_state_pos*sizeof(inscript_state[0]) < sizeof(inscript_state)); \
  inscript_state_pos=new_state_pos; \
  } \
} while(0)  

/* Increment current pointer to 'steps' characters, modifying automate if necessary */
#define INCREMENT_CURRENT_ADR(steps) do { \
  int steps__ = (int) ( steps ); \
  while(steps__ > 0) { \
  adr++; \
  AUTOMATE_LOOKUP_CURRENT_ADR(); \
  steps__ --; \
  } \
} while(0)


/* Main parser */
int htsparse(htsmoduleStruct* str, htsmoduleStructExtended* stre) {
	char catbuff[CATBUFF_SIZE];
  /* Load engine variables */
  ENGINE_LOAD_CONTEXT();

  {
    char* cAddr = r->adr;
    int cSize = (int) r->size;
    if ( (opt->debug>0) && (opt->log!=NULL) ) {
      HTS_LOG(opt,LOG_INFO); fprintf(opt->log,"engine: preprocess-html: %s%s"LF, urladr, urlfil);
    }
    if (RUN_CALLBACK4(opt, preprocess, &cAddr, &cSize, urladr, urlfil) == 1) {
      r->adr = cAddr;
      r->size = cSize;
    }
  }
  if (RUN_CALLBACK4(opt, check_html, r->adr,(int)r->size,urladr,urlfil)) {
    FILE* fp=NULL;      // fichier écrit localement 
    char* adr=r->adr;    // pointeur (on parcourt)
    char* lastsaved;    // adresse du dernier octet sauvé + 1
    if ( (opt->debug>1) && (opt->log!=NULL) ) {
      HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"scanning file %s%s (%s).."LF, urladr, urlfil, savename); test_flush;
    }


    // Indexing!
#if HTS_MAKE_KEYWORD_INDEX
    if (opt->kindex) {
      if (index_keyword(r->adr,r->size,r->contenttype,savename,StringBuff(opt->path_html_utf8))) {
        if ( (opt->debug>1) && (opt->log!=NULL) ) {
          HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"indexing file..done"LF); test_flush;
        }
      } else {
        if ( (opt->debug>1) && (opt->log!=NULL) ) {
          HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"indexing file..error!"LF); test_flush;
        }
      }
    }
#endif

    // Now, parsing
    if ((opt->getmode & 1) && (ptr>0)) {  // récupérer les html sur disque       
      // créer le fichier html local
      HT_ADD_FOP;   // écrire peu à peu le fichier
    }

    if (!error) {
      time_t user_interact_timestamp = 0;
      int detect_title=0;  // détection  du title
      int back_add_stats = opt->state.back_add_stats;
      //
      char* in_media=NULL; // in other media type (real media and so..)
      int intag=0;         // on est dans un tag
      int incomment=0;     // dans un <!--
      int inscript=0;      // dans un scipt pour applets javascript)
      int inscript_locked=0;  // in locked script (ie. js file)
      signed char inscript_state[10][257];
      typedef enum { 
        INSCRIPT_START=0,
        INSCRIPT_ANTISLASH,
        INSCRIPT_INQUOTE,
        INSCRIPT_INQUOTE2,
        INSCRIPT_SLASH,
        INSCRIPT_SLASHSLASH,
        INSCRIPT_COMMENT,
        INSCRIPT_COMMENT2,
        INSCRIPT_ANTISLASH_IN_QUOTE,
        INSCRIPT_ANTISLASH_IN_QUOTE2,
        INSCRIPT_DEFAULT=256
      } INSCRIPT;
      INSCRIPT inscript_state_pos=INSCRIPT_START;
      char* inscript_name=NULL; // script tag name
      int inscript_tag=0;  // on est dans un <body onLoad="... terminé par >
      char inscript_tag_lastc='\0';
      // terminaison (" ou ') du "<body onLoad=.."
      int inscriptgen=0;     // on est dans un code générant, ex après obj.write("..
      //int inscript_check_comments=0, inscript_in_comments=0;    // javascript comments
      char scriptgen_q='\0'; // caractère faisant office de guillemet (' ou ")
      //int no_esc_utf=0;      // ne pas echapper chars > 127
      int nofollow=0;        // ne pas scanner
      //
      int parseall_lastc='\0';     // dernier caractère parsé pour parseall
      //int parseall_incomment=0;   // dans un /* */ (exemple: a = /* URL */ "img.gif";)
      //
      char* intag_start = adr;
			char* intag_name = NULL;
      char* intag_startattr=NULL;
      int intag_start_valid=0;
      int intag_ctype=0;
      //
      int   parent_relative=0;    // the parent is the base path (.js, .css..)
      HT_ADD_START;    // débuter
      lastsaved=adr;

      /* Initialize script automate for comments, quotes.. */
      memset(inscript_state, 0xff, sizeof(inscript_state));
      inscript_state[INSCRIPT_START][INSCRIPT_DEFAULT]=INSCRIPT_START;     /* by default, stay in START */
      inscript_state[INSCRIPT_START]['\\']=INSCRIPT_ANTISLASH;             /* #1: \ escapes the next character whatever it is */
      inscript_state[INSCRIPT_ANTISLASH][INSCRIPT_DEFAULT]=INSCRIPT_START;
      inscript_state[INSCRIPT_START]['\'']=INSCRIPT_INQUOTE;               /* #2: ' opens quote and only ' returns to 0 */
      inscript_state[INSCRIPT_INQUOTE][INSCRIPT_DEFAULT]=INSCRIPT_INQUOTE;
      inscript_state[INSCRIPT_INQUOTE]['\'']=INSCRIPT_START;
      inscript_state[INSCRIPT_INQUOTE]['\\']=INSCRIPT_ANTISLASH_IN_QUOTE;
      inscript_state[INSCRIPT_START]['\"']=INSCRIPT_INQUOTE2;              /* #3: " opens double-quote and only " returns to 0 */
      inscript_state[INSCRIPT_INQUOTE2][INSCRIPT_DEFAULT]=INSCRIPT_INQUOTE2;
      inscript_state[INSCRIPT_INQUOTE2]['\"']=INSCRIPT_START;
      inscript_state[INSCRIPT_INQUOTE2]['\\']=INSCRIPT_ANTISLASH_IN_QUOTE2;
      inscript_state[INSCRIPT_START]['/']=INSCRIPT_SLASH;                  /* #4: / state, default to #0 */
      inscript_state[INSCRIPT_SLASH][INSCRIPT_DEFAULT]=INSCRIPT_START;
      inscript_state[INSCRIPT_SLASH]['/']=INSCRIPT_SLASHSLASH;             /* #5: // with only LF to escape */
      inscript_state[INSCRIPT_SLASHSLASH][INSCRIPT_DEFAULT]=INSCRIPT_SLASHSLASH;
      inscript_state[INSCRIPT_SLASHSLASH]['\n']=INSCRIPT_START;
      inscript_state[INSCRIPT_SLASH]['*']=INSCRIPT_COMMENT;                /* #6: / * with only * / to escape */
      inscript_state[INSCRIPT_COMMENT][INSCRIPT_DEFAULT]=INSCRIPT_COMMENT;
      inscript_state[INSCRIPT_COMMENT]['*']=INSCRIPT_COMMENT2;             /* #7: closing comments */
      inscript_state[INSCRIPT_COMMENT2][INSCRIPT_DEFAULT]=INSCRIPT_COMMENT;
      inscript_state[INSCRIPT_COMMENT2]['/']=INSCRIPT_START;
      inscript_state[INSCRIPT_COMMENT2]['*']=INSCRIPT_COMMENT2;
      inscript_state[INSCRIPT_ANTISLASH_IN_QUOTE][INSCRIPT_DEFAULT]=INSCRIPT_INQUOTE;    /* #8: escape in '' */
      inscript_state[INSCRIPT_ANTISLASH_IN_QUOTE2][INSCRIPT_DEFAULT]=INSCRIPT_INQUOTE2;  /* #9: escape in "" */

      /* Primary list or URLs */
      if (ptr == 0) {
        intag=1;
        intag_start_valid=0;
				intag_name = NULL;
      }
      /* Check is the file is a .js file */
      else if (
        (compare_mime(opt,r->contenttype, str->url_file, "application/x-javascript")!=0)
        || (compare_mime(opt,r->contenttype, str->url_file, "text/css")!=0)
        ) {      /* JavaScript js file */
          inscript=1;
          inscript_locked=1;  /* Don't exit js space upon </script> */
          if (opt->parsedebug) { HT_ADD("<@@ inscript @@>"); }
          inscript_name="script";
          intag=1;     // because après <script> on y est .. - pas utile
          intag_start_valid=0;    // OUI car nous sommes dans du code, plus dans du "vrai" tag
          if ((opt->debug>1) && (opt->log!=NULL)) {
            HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"note: this file is a javascript file"LF); test_flush;
          }
          // for javascript only
          if (compare_mime(opt,r->contenttype, str->url_file, "application/x-javascript") != 0) {
            // all links must be checked against parent, not this link
            if (liens[ptr]->precedent != 0) {
              parent_relative=1;
            }
          }
        }
        /* Or a real audio */
      else if (compare_mime(opt,r->contenttype, str->url_file, "audio/x-pn-realaudio")!=0) {      /* realaudio link file */
        inscript=intag=0;
        inscript_name="media";
        intag_start_valid=0;
        in_media="LNK";       // real media! -> links
      } 
      /* Or a m3u playlist */
      else if (compare_mime(opt,r->contenttype, str->url_file, "audio/x-mpegurl")!=0) {      /* mp3 link file */
        inscript=intag=0;
        inscript_name="media";
        intag_start_valid=0;
        in_media="LNK";       // m3u! -> links
      } 
      else if (compare_mime(opt,r->contenttype, str->url_file, "application/x-authorware-map")!=0) {      /* macromedia aam file */
        inscript=intag=0;
        inscript_name="media";
        intag_start_valid=0;
        in_media="AAM";       // aam
      } 
      /* Or a RSS file */
      else if (
        compare_mime(opt,r->contenttype, str->url_file, "text/xml") != 0
        || compare_mime(opt,r->contenttype, str->url_file, "application/xml") != 0
        ) 
      {
        if (strstr(adr, "http://purl.org/rss/") != NULL) // Hmm, this is a bit lame ; will have to cleanup
        {      /* RSS file */
          inscript=intag=0;
          intag_start_valid=0;
          in_media=NULL;       // regular XML
        } else {   // cancel: write all
          adr = r->adr + r->size;
          HT_ADD_ADR;
          lastsaved=adr;
        }
      }

      // Detect UTF8 format
      //if (is_unicode_utf8((unsigned char*) r->adr, (unsigned int) r->size) == 1) {
      //  no_esc_utf=1;
      //} else {
      //  no_esc_utf=0;
      //}

			// Hack to prevent any problems with ram files of other files
      * ( r->adr + r->size ) = '\0';

      // ------------------------------------------------------------
      // analyser ce qu'il y a en mémoire (fichier html)
      // on scanne les balises
      // ------------------------------------------------------------
      opt->state._hts_in_html_done=0;     // 0% scannés
      opt->state._hts_in_html_parsing=1;  // flag pour indiquer un parsing

			base[0]='\0';    // effacer base-href
      do {
        int p=0;
        int valid_p=0;      // force to take p even if == 0
        int ending_p='\0';  // ending quote?
        int archivetag_p=0;  // avoid multiple-archives with commas
        int  unquoted_script=0;
        INSCRIPT inscript_state_pos_prev=inscript_state_pos;
        error=0;

        /* Break if we are done yet */
        if ( ( adr - r->adr ) >= r->size)
          break;

        /* Hack to avoid NULL char problems with C syntax */
        /* Yes, some bogus HTML pages can embed null chars
        and therefore can not be properly handled if this hack is not done
        */
        if ( ! (*adr) ) {
          if ( ((int) (adr - r->adr)) < r->size)
            *adr=' ';
        }

        /*
        index.html built here
        */
        // Construction index.html (sommaire)
        // Avant de tester les a href,
        // Ici on teste si l'on doit construire l'index vers le(s) site(s) miroir(s)
        if (!makeindex_done) {  // autoriation d'écrire un index
          if (!detect_title) {
            if (opt->depth == liens[ptr]->depth) {    // on note toujours les premiers liens
              if (!in_media) {
                if (opt->makeindex && (ptr>0)) {
                  if (opt->getmode & 1) {  // autorisation d'écrire
                    p=strfield(adr,"title");  
                    if (p) {
                      if (*(adr-1)=='/') p=0;    // /title
                    } else {
                      if (strfield(adr,"/html"))
                        p=-1;                    // noter, mais sans titre
                      else if (strfield(adr,"body"))
                        p=-1;                    // noter, mais sans titre
                      else if ( ((int) (adr - r->adr) ) >= (r->size-1) )
                        p=-1;                    // noter, mais sans titre
                      else if ( (int) (adr - r->adr) >= r->size - 2)   // we got to hurry
                        p=-1; // xxc xxc xxc
                    }
                  } else
                    p=0;

                  if (p) {    // ok center                            
                    if (makeindex_fp==NULL) {
                      file_notify(opt,"", "", fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_html_utf8),"index.html"), 1, 1, 0);
                      verif_backblue(opt,StringBuff(opt->path_html_utf8));    // générer gif
                      makeindex_fp=filecreate(&opt->state.strc, fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_html_utf8),"index.html"));
                      if (makeindex_fp!=NULL) {

                        // Header
                        fprintf(makeindex_fp,template_header,
                          "<!-- Mirror and index made by HTTrack Website Copier/"HTTRACK_VERSION" "HTTRACK_AFF_AUTHORS" -->"
                          );

                      } else makeindex_done=-1;    // fait, erreur
                    }

                    if (makeindex_fp!=NULL) {
                      char BIGSTK tempo[HTS_URLMAXSIZE*2];
                      char BIGSTK s[HTS_URLMAXSIZE*2];
                      char* a=NULL;
                      char* b=NULL;
                      s[0]='\0';
                      if (p>0) {
                        a=strchr(adr,'>');
                        if (a!=NULL) {
                          a++;
                          while(is_space(*a)) a++;    // sauter espaces & co
                          b=strchr(a,'<');   // prochain tag
                        }
                      }
                      if (lienrelatif(tempo,liens[ptr]->sav,concat(OPT_GET_BUFF(opt),StringBuff(opt->path_html_utf8),"index.html"))==0) {
                        detect_title=1;      // ok détecté pour cette page!
                        makeindex_links++;   // un de plus
                        strcpybuff(makeindex_firstlink,tempo);
                        //

                        /* Hack */
                        if (opt->mimehtml) {
                          strcpybuff(makeindex_firstlink, "cid:primary/primary");
                        }

                        if ((b==a) || (a==NULL) || (b==NULL)) {    // pas de titre
                          strcpybuff(s,tempo);
                        } else if ((b-a)<256) {
                          b--;
                          while(is_space(*b)) b--;
                          strncpy(s,a,b-a+1);
                          *(s+(b-a)+1)='\0';
                        }

                        // Body
                        fprintf(makeindex_fp,template_body,
                          tempo,
                          s
                          );

                      }
                    }
                  }
                }
              }

            } else if (liens[ptr]->depth<opt->depth) {   // on a sauté level1+1 et level1
              HT_INDEX_END;
            }
          } // if (opt->makeindex)
        }
        // FIN Construction index.html (sommaire)
        /*
        end -- index.html built here
        */



        /* Parse */
        if (
          (*adr=='<')    /* No starting tag */
          && (!inscript)    /* Not in (java)script */
          && (!incomment)   /* Not in comment (<!--) */
          && (!in_media)    /* Not in media */
          ) { 
            intag=1;
						intag_ctype=0;
						//parseall_incomment=0;
						//inquote=0;  // effacer quote
						intag_start = adr;
						for(intag_name = adr + 1 ; is_realspace(*intag_name) ; intag_name++ );
						intag_start_valid = 1;
						codebase[0]='\0';    // effacer éventuel codebase

						/* Meta ? */
						if (check_tag(intag_start, "meta")) {
							int pos;
							// <meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
							if ((pos = rech_tageq_all(adr, "http-equiv"))) {
								const char* token = NULL;
								int len = rech_endtoken(adr + pos, &token);
								if (len > 0) {
									if (strfield(token, "content-type")) {
										intag_ctype=1;
                    //NOPE-we do not convert the whole page actually
                    //intag_start[1] = 'X';
									}
									else if (strfield(token, "refresh")) {
										intag_ctype=2;
									}
								}
							}
						}

            if (opt->getmode & 1) {  // sauver html
              p=strfield(adr,"</html");
              if (p==0) p=strfield(adr,"<head>");
              // if (p==0) p=strfield(adr,"<doctype");
              if (p) {
                char* eol="\n";
                if (strchr(r->adr,'\r'))
                  eol="\r\n";
                if (StringNotEmpty(opt->footer) || opt->urlmode != 4) {  /* != preserve */
									if (StringNotEmpty(opt->footer)) {
	                  char BIGSTK tempo[1024+HTS_URLMAXSIZE*2];
										char gmttime[256];
										tempo[0]='\0';
										time_gmt_rfc822(gmttime);
										strcatbuff(tempo,eol);
	                  sprintf(tempo+strlen(tempo),StringBuff(opt->footer),jump_identification(urladr),urlfil,gmttime,HTTRACK_VERSIONID,"","","","","","","");
		                strcatbuff(tempo,eol);
										//fwrite(tempo,1,strlen(tempo),fp);
										HT_ADD(tempo);
									}
                  if (strnotempty(r->charset)) {
                    HT_ADD("<!-- Added by HTTrack --><meta http-equiv=\"content-type\" content=\"text/html;charset=");
                    HT_ADD(r->charset);
                    HT_ADD("\" /><!-- /Added by HTTrack -->");
                    HT_ADD(eol);
                  }
                }
              }
            }        

            // éliminer les <!-- (commentaires) : intag dévalidé
            if (*(adr+1)=='!')
              if (*(adr+2)=='-')
                if (*(adr+3)=='-') {
                  intag=0;
                  incomment=1;
                  intag_start_valid=0;
                }

          }
        else if (
          (*adr=='>')                        /* ending tag */
          && ( (!inscript && !in_media) || (inscript_tag) )  /* and in tag (or in script) */
          ) {
            if (inscript_tag) {
              inscript_tag=inscript=0;
              intag=0;
              incomment=0;
              intag_start_valid=0;
							intag_name = NULL;
              if (opt->parsedebug) { HT_ADD("<@@ /inscript @@>"); }
            } else if (!incomment) {
              intag=0; //inquote=0;

              // entrée dans du javascript?
              // on parse ICI car il se peut qu'on ait eu a parser les src=.. dedans
              //if (!inscript) {  // sinon on est dans un obj.write("..
              if ((intag_start_valid) && 
                (
                check_tag(intag_start,"script")
                ||
                check_tag(intag_start,"style")
                )
                ) {
                  char* a=intag_start;    // <
                  // ** while(is_realspace(*(--a)));
                  if (*a=='<') {  // sûr que c'est un tag?
                    if (check_tag(intag_start,"script"))
                      inscript_name="script";
                    else
                      inscript_name="style";
                    inscript=1;
                    inscript_state_pos=INSCRIPT_START;
                    intag=1;     // because après <script> on y est .. - pas utile
                    intag_start_valid=0;    // OUI car nous sommes dans du code, plus dans du "vrai" tag
                    if (opt->parsedebug) { HT_ADD("<@@ inscript @@>"); }
                  }
                }
            } else {                               /* end of comment? */
              // vérifier fermeture correcte
              if ( (*(adr-1)=='-') && (*(adr-2)=='-') ) {
                intag=0;
                incomment=0;
                intag_start_valid=0;
								intag_name = NULL;
              }
#if GT_ENDS_COMMENT
              /* wrong comment ending */
              else {
                /* check if correct ending does not exists
                <!-- foo > example <!-- bar > is sometimes accepted by browsers
                when no --> is used somewhere else.. darn those browsers are dirty
                */
                if (!strstr(adr,"-->")) {
                  intag=0;
                  incomment=0;
                  intag_start_valid=0;
									intag_name = NULL;
                }
              }
#endif
            }
            //}
          }
          //else if (*adr==34) {
          //  inquote=(inquote?0:1);
          //}
        else if (intag || inscript || in_media) {    // nous sommes dans un tag/commentaire, tester si on recoit un tag
          int p_type=0;
          int p_nocatch=0;
          int p_searchMETAURL=0;  // chercher ..URL=<url>
          int add_class=0;        // ajouter .class
          int add_class_dots_to_patch=0;   // number of '.' in code="x.y.z<realname>"
          char* p_flush=NULL;


          // ------------------------------------------------------------
          // parsing évolé
          // ------------------------------------------------------------
          if (((isalpha((unsigned char)*adr)) || (*adr=='/') || (inscript) || (in_media) || (inscriptgen))) {  // sinon pas la peine de tester..


            /* caractère de terminaison pour "miniparsing" javascript=.. ? 
            (ex: <a href="javascript:()" action="foo"> ) */
            if (inscript_tag) {
              if (inscript_tag_lastc) {
                if (*adr == inscript_tag_lastc) {
                  /* sortir */
                  inscript_tag=inscript=0;
                  incomment=0;
                  if (opt->parsedebug) { HT_ADD("<@@ /inscript @@>"); }
                }
              }
            }

            /* automate */
            AUTOMATE_LOOKUP_CURRENT_ADR();


            // Note:
            // Certaines pages ne respectent pas le html
            // notamment les guillements ne sont pas fixés
            // Nous sommes dans un tag, donc on peut faire un test plus
            // large pour pouvoi prendre en compte ces particularités

            // à vérifier: ACTION, CODEBASE, VRML

            if (in_media) {
              if (strcmp(in_media,"LNK")==0) { // real media
                p=0;
                valid_p=1;
              }
              else if (strcmp(in_media, "AAM")==0) { // AAM
                if (is_space((unsigned char)adr[0]) && ! is_space((unsigned char)adr[1])) {
                  char* a = adr + 1;
                  int n = 0;
                  int ok = 0;
                  int dot = 0;
                  while(n < HTS_URLMAXSIZE/2 && a[n] != '\0' &&
                    ( ! is_space((unsigned char)a[n]) || ! ( ok = 1) )
                    ) {
                      if (a[n] == '.') {
                        dot = n;
                      }
                      n++;
                    }
                    if (ok && dot > 0) {
                      char BIGSTK tmp[HTS_URLMAXSIZE/2 + 2];
                      tmp[0] = '\0';
                      strncat(tmp, a + dot + 1, n - dot - 1);
                      if (is_knowntype(opt,tmp) || ishtml_ext(tmp) != -1) {
                        adr++;
                        p = 0;
                        valid_p = 1;
                        unquoted_script = 1;
                      }
                    }
                }
              }
            } else if (ptr>0) {        /* pas première page 0 (primary) */
              p=0;  // saut pour le nom de fichier: adresse nom fichier=adr+p

              // ------------------------------
              // détection d'écriture JavaScript.
              // osons les obj.write et les obj.href=.. ! osons!
              // note: inscript==1 donc on sautera après les \"
              if (inscript) {
                if (inscriptgen) {          // on est déja dans un objet générant..
                  if (*adr==scriptgen_q) {  // fermeture des " ou '
                    if (*(adr-1)!='\\') {   // non
                      inscriptgen=0;        // ok parsing terminé
                    }
                  }
                } else {
                  char* a=NULL;
                  char check_this_fking_line=0;  // parsing code javascript..
                  char must_be_terminated=0;     // caractère obligatoire de terminaison!
                  int token_size;
                  if (!(token_size=strfield(adr,".writeln"))) // détection ...objet.write[ln]("code html")...
                    token_size=strfield(adr,".write");
                  if (token_size) {
                    a=adr+token_size;
                    while(is_realspace(*a)) a++; // sauter espaces
                    if (*a=='(') {  // début parenthèse
                      check_this_fking_line=2;  // à parser!
                      must_be_terminated=')';
                      a++;  // sauter (
                    }
                  }
                  // euhh ??? ???
                  /* else if (strfield(adr,".href")) {  // détection ...objet.href="...
                  a=adr+5;
                  while(is_realspace(*a)) a++; // sauter espaces
                  if (*a=='=') {  // ohh un égal
                  check_this_fking_line=1;  // à noter!
                  must_be_terminated=';';   // et si t'as oublié le ; tu sais pas coder
                  a++;   // sauter =
                  }

                  }*/

                  // on a un truc du genre instruction"code généré" dont on parse le code
                  if (check_this_fking_line) {
                    while(is_realspace(*a)) a++;
                    if ((*a=='\'') || (*a=='"')) {  // départ de '' ou ""
                      char *b;
                      scriptgen_q=*a;    // quote
                      b=a+1;      // départ de la chaîne
                      // vérifier forme ("code") et pas ("code"+var), ingérable
                      do {
                        if (*a==scriptgen_q && *(a-1)!='\\')  // quote non slash
                          break;            // sortie
                        else if (*a==10 && *(a-1) != '\\'  /* LF and no continue (\) character */
                          && ( *(a-1) != '\r' || *(a-2) != '\\' ) )  /* and not CRLF and no .. */
                          break;
                        else 
                          a++;  // caractère suivant
                      } while((a-b) < HTS_URLMAXSIZE / 2);
                      if (*a==scriptgen_q) {  // fin du quote
                        a++;
                        while(is_realspace(*a)) a++;
                        if (*a==must_be_terminated) {  // parenthèse fermante: ("..")

                          // bon, on doit parser une ligne javascript
                          // 1) si check.. ==1 alors c'est un nom de fichier direct, donc
                          // on fixe p sur le saut nécessaire pour atteindre le nom du fichier
                          // et le moteur se débrouillera ensuite tout seul comme un grand
                          // 2) si check==2 c'est un peu plus tordu car là on génére du
                          // code html au sein de code javascript au sein de code html
                          // dans ce cas on doit fixer un flag à un puis ensuite dans la boucle
                          // on devra parser les instructions standard comme <a href etc
                          // NOTE: le code javascript autogénéré n'est pas pris en compte!!
                          // (et ne marche pas dans 50% des cas de toute facon!)
                          if (check_this_fking_line==1) {
                            p=(int) (b - adr);    // calculer saut!
                          } else {
                            inscriptgen=1;        // SCRIPTGEN actif
                            adr=b;                // jump
                          }

                          if ((opt->debug>1) && (opt->log!=NULL)) {
                            char str[512];
                            str[0]='\0';
                            strncatbuff(str,b,minimum((int) (a - b + 1), 32));
                            HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"active code (%s) detected in javascript: %s"LF,(check_this_fking_line==2)?"parse":"pickup",str); test_flush;
                          }
                        }

                      }

                    }


                  }
                }
              }
              // fin detection code générant javascript vers html
              // ------------------------------


              // analyse proprement dite, A HREF=.. etc..
              if (!p) {
                // si dans un tag, et pas dans un script - sauf si on analyse un obj.write("..
                if ((intag && (!inscript)) || inscriptgen) {
                  if ( (*(adr-1)=='<') || (is_space(*(adr-1))) ) {   // <tag < tag etc
                    // <A HREF=.. pour les liens HTML
                    p=rech_tageq(adr,"href");
                    if (p) {    // href.. tester si c'est une bas href!
                      if ((intag_start_valid) && check_tag(intag_start, "base")) {  // oui!
                        // ** note: base href et codebase ne font pas bon ménage..
                        p_type=2;    // c'est un chemin
                      }
                    }

                    /* Tags supplémentaires à vérifier (<img src=..> etc) */
                    if (p==0) {
                      int i=0;
                      while( (p==0) && (strnotempty(hts_detect[i])) ) {
                        p=rech_tageq(adr,hts_detect[i]);
                        if (p) {
                          /* This is a temporary hack to avoid archive=foo.jar,bar.jar .. */
                          if (strcmp(hts_detect[i], "archive") == 0) {
                            archivetag_p = 1;
                          }
                        }
                        i++;
                      }
                    }

                    /* Tags supplémentaires en début à vérifier (<object .. hotspot1=..> etc) */
                    if (p==0) {
                      int i=0;
                      while( (p==0) && (strnotempty(hts_detectbeg[i])) ) {
                        p=rech_tageqbegdigits(adr,hts_detectbeg[i]);
                        i++;
                      }
                    }

                    /* Tags supplémentaires à vérifier : URL=.. */
                    if (p==0) {
                      int i=0;
                      while( (p==0) && (strnotempty(hts_detectURL[i])) ) {
                        p=rech_tageq(adr,hts_detectURL[i]);
                        i++;
                      }
                      if (p) {
                        if (intag_ctype == 1) {
                          p = 0;
#if 0
                          //if ((pos=rech_tageq(adr, "content"))) {
                          char temp[256];
                          char* token = NULL;
                          int len = rech_endtoken(adr + pos, &token);
                          if (len > 0 && len < sizeof(temp) - 2) {
                            char* chpos;
                            temp[0] = '\0';
                            strncat(temp, token, len);
                            if ((chpos = strstr(temp, "charset"))
                              &&
                              (chpos = strchr(chpos, '='))
                              ) {
                                chpos++;
                                while(is_space(*chpos)) chpod++;
                                //chpos
                              }
                          }
#endif
                        }
                        // <META HTTP-EQUIV="Refresh" CONTENT="3;URL=http://www.example.com">
                        else if (intag_ctype == 2) {
                          p_searchMETAURL=1;
                        } else {
                          p = 0;			/* cancel */
                        }
                      }


                    }

                    /* Tags supplémentaires à vérifier, mais à ne pas capturer */
                    if (p==0) {
                      int i=0;
                      while( (p==0) && (strnotempty(hts_detectandleave[i])) ) {
                        p=rech_tageq(adr,hts_detectandleave[i]);
                        i++;
                      }
                      if (p)
                        p_nocatch=1;      /* ne pas rechercher */
                    }

                    /* Evénements */
                    if (p==0 && 
                      ! inscript          /* we don't want events inside document.write */
                      ) {
                        int i=0;
                        /* détection onLoad etc */
                        while( (p==0) && (strnotempty(hts_detect_js[i])) ) {
                          p=rech_tageq(adr,hts_detect_js[i]);
                          i++;
                        }
                        /* non détecté - détecter également les onXxxxx= */
                        if (p==0) {
                          if ( (*adr=='o') && (*(adr+1)=='n') && isUpperLetter(*(adr+2)) ) {
                            p=0;
                            while(isalpha((unsigned char)adr[p]) && (p<64) ) p++;
                            if (p<64) {
                              while(is_space(adr[p])) p++;
                              if (adr[p]=='=')
                                p++;
                              else p=0;
                            } else p=0;
                          }
                        }
                        /* OK, événement repéré */
                        if (p) {
                          inscript_tag_lastc=*(adr+p);     /* à attendre à la fin */
                          adr+=p+1;   /* saut */
                          /*
                          On est désormais dans du code javascript
                          */
                          inscript_name="";
                          inscript=inscript_tag=1;
                          inscript_state_pos=INSCRIPT_START;
                          if (opt->parsedebug) { HT_ADD("<@@ inscript @@>"); }
                        }
                        p=0;        /* quoi qu'il arrive, ne rien démarrer ici */
                      }

                      // <APPLET CODE=.. pour les applet java.. [CODEBASE (chemin..) à faire]
                      if (p==0) {
                        p=rech_tageq(adr,"code");
                        if (p) {
                          if ((intag_start_valid) && check_tag(intag_start,"applet")) {  // dans un <applet !
                            p_type=-1;  // juste le nom de fichier+dossier, écire avant codebase 
                            add_class=1;   // ajouter .class au besoin                         

                            // vérifier qu'il n'y a pas de codebase APRES
                            // sinon on swappe les deux.
                            // pas très propre mais c'est ce qu'il y a de plus simple à faire!!

                            {
                              char *a;
                              a=adr;
                              while((*a) && (*a!='>') && (!rech_tageq(a,"codebase"))) a++;
                              if (rech_tageq(a,"codebase")) {  // banzai! codebase=
                                char* b;
                                b=strchr(a,'>');
                                if (b) {
                                  if (((int) (b - adr)) < 1000) {    // au total < 1Ko
                                    char BIGSTK tempo[HTS_URLMAXSIZE*2];
                                    tempo[0]='\0';
                                    strncatbuff(tempo,a,(int) (b - a) );
                                    strcatbuff( tempo," ");
                                    strncatbuff(tempo,adr,(int) (a - adr - 1));
                                    // éventuellement remplire par des espaces pour avoir juste la taille
                                    while((int) strlen(tempo)<((int) (b - adr)))
                                      strcatbuff(tempo," ");
                                    // pas d'erreur?
                                    if ((int) strlen(tempo) == ((int) (b - adr) )) {
                                      strncpy(adr,tempo,strlen(tempo));   // PAS d'octet nul à la fin!
                                      p=0;    // DEVALIDER!!
                                      p_type=0;
                                      add_class=0;
                                    }
                                  }
                                }
                              }
                            }

                          }
                        }
                      }

                      // liens à patcher mais pas à charger (ex: codebase)
                      if (p==0) {  // note: si non chargé (ex: ignorer .class) patché tout de même
                        p=rech_tageq(adr,"codebase");
                        if (p) {
                          if ((intag_start_valid) && check_tag(intag_start,"applet")) {  // dans un <applet !
                            p_type=-2;
                          } else p=-1;   // ne plus chercher
                        }
                      }


                      // Meta tags pour robots
                      if (p==0) {
                        if (opt->robots) {
                          if ((intag_start_valid) && check_tag(intag_start,"meta")) {
                            if (rech_tageq(adr,"name")) {    // name=robots.txt
                              char tempo[1100];
                              char* a;
                              tempo[0]='\0';
                              a=strchr(adr,'>');
#if DEBUG_ROBOTS
                              printf("robots.txt meta tag detected\n");
#endif
                              if (a) {
                                if (((int) (a - adr)) < 999 ) {
                                  strncatbuff(tempo,adr,(int) (a - adr));
                                  if (strstrcase(tempo,"content")) {
                                    if (strstrcase(tempo,"robots")) {
                                      if (strstrcase(tempo,"nofollow")) {
#if DEBUG_ROBOTS
                                        printf("robots.txt meta tag: nofollow in %s%s\n",urladr,urlfil);
#endif
                                        nofollow=1;       // NE PLUS suivre liens dans cette page
                                        if (opt->log) {
                                          HTS_LOG(opt,LOG_WARNING); fprintf(opt->log,"Link %s%s not scanned (follow robots meta tag)"LF,urladr,urlfil);
                                          test_flush;
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

											// entrée dans une applet javascript
                      /*if (!inscript) {  // sinon on est dans un obj.write("..
                      if (p==0)
                      if (rech_sampletag(adr,"script"))
                      if (check_tag(intag_start,"script")) {
                      inscript=1;
                      }
                      }*/

                      // Ici on procède à une analyse du code javascript pour tenter de récupérer
                      // certains fichiers évidents.
                      // C'est devenu obligatoire vu le nombre de pages qui intègrent
                      // des images réactives par exemple
                  }
                } else if (inscript) {

#if 0
                  /* Check // javascript comments */
                  if (*adr == 10 || *adr == 13) {
                    inscript_check_comments = 1;
                    inscript_in_comments = 0;
                  }
                  else if (inscript_check_comments) {
                    if (!is_realspace(*adr)) {
                      inscript_check_comments = 0;
                      if (adr[0] == '/' && adr[1] == '/') {
                        inscript_in_comments = 1;
                      }
                    }
                  }
#endif

                  /* Parse */
                  assertf(inscript_name != NULL);
                  if (
                    *adr == '/' &&
                    (
                    (strfield(adr,"/script") && strfield(inscript_name, "script"))
                    ||
                    (strfield(adr,"/style")  && strfield(inscript_name, "style"))
                    )
                    && inscript_locked == 0
                    ) {
                      char* a=adr;
                      //while(is_realspace(*(--a)));
                      while( is_realspace(*a) ) a--;
                      a--;
                      if (*a=='<') {  // sûr que c'est un tag?
                        inscript=0;
                        if (opt->parsedebug) { HT_ADD("<@@ /inscript @@>"); }
                      }
                    } else if (inscript_state_pos == INSCRIPT_START /*!inscript_in_comments*/) {
                      /*
                      Script Analyzing - different types supported:
                      foo="url"
                      foo("url") or foo(url)
                      foo "url"
                      */
                      char  expected     = '=';          // caractère attendu après
                      char* expected_end = ";";
                      int can_avoid_quotes=0;
                      char quotes_replacement='\0';
                      int ensure_not_mime=0;
                      if (inscript_tag)
                        expected_end=";\"\'";            // voir a href="javascript:doc.location='foo'"

                      /* Can we parse javascript ? */
                      if ( (opt->parsejava & HTSPARSE_NO_JAVASCRIPT) == 0) {
                        int nc;
                        nc = strfield(adr,".src");  // nom.src="image";
                        if (!nc) nc = strfield(adr,".location");  // document.location="doc"
                        if (!nc) nc = strfield(adr,":location");  // javascript:location="doc"
                        if (!nc) { // location="doc"
                          if ( ( nc = strfield(adr,"location") ) 
                            && !isspace(*(adr - 1))
                            )
                            nc = 0;
                        }
                        if (!nc) nc = strfield(adr,".href");  // document.location="doc"
                        if (!nc) if ( (nc = strfield(adr,".open")) ) { // window.open("doc",..
                          expected='(';    // parenthèse
                          expected_end="),";  // fin: virgule ou parenthèse
                          ensure_not_mime=1;  //* ensure the url is not a mime type */
                        }
                        if (!nc) if ( (nc = strfield(adr,".replace")) ) { // window.replace("url")
                          expected='(';    // parenthèse
                          expected_end=")";  // fin: parenthèse
                        }
                        if (!nc) if ( (nc = strfield(adr,".link")) ) { // window.link("url")
                          expected='(';    // parenthèse
                          expected_end=")";  // fin: parenthèse
                        }
                        if (!nc && (nc = strfield(adr,"url")) && (!isalnum(*(adr - 1))) && *(adr - 1) != '_') { // url(url)
                          expected='(';    // parenthèse
                          expected_end=")";  // fin: parenthèse
                          can_avoid_quotes=1;
                          quotes_replacement=')';
                        } else {
                          nc = 0;
                        }
                        if (!nc) if ( (nc = strfield(adr,"import")) ) { // import "url"
                          if (is_space(*(adr+nc))) {
                            expected=0;    // no char expected
                          } else
                            nc=0;
                        }
                        if (nc) {
                          char *a;
                          a=adr+nc;
                          while(is_realspace(*a)) a++;
                          if ((*a == expected) || (!expected)) {
                            if (expected)
                              a++;
                            while(is_realspace(*a)) a++;
                            if ((*a==34) || (*a=='\'') || (can_avoid_quotes)) {
                              char *b,*c;
                              int ndelim=1;
                              if ((*a==34) || (*a=='\''))
                                a++;
                              else
                                ndelim=0;
                              b=a;
                              if (ndelim) {
                                while((*b!=34) && (*b!='\'') && (*b!='\0')) b++;
                              }
                              else {
                                while((*b != quotes_replacement) && (*b!='\0')) b++;
                              }
                              c=b--; c+=ndelim;
                              while(*c==' ') c++;
                              if ((strchr(expected_end,*c)) || (*c=='\n') || (*c=='\r')) {
                                c-=(ndelim+1);
                                if ((int) (c - a + 1)) {
                                  if (ensure_not_mime) {
                                    int i = 0;
                                    while(a != NULL && hts_main_mime[i] != NULL && hts_main_mime[i][0] != '\0') {
                                      int p;
                                      if ((p=strfield(a, hts_main_mime[i])) && a[p] == '/') {
                                        a=NULL;
                                      }
                                      i++;
                                    }
                                  }
                                  // Check for bogus links (Vasiliy)
                                  if (a != NULL) {
                                    const size_t size = c - a + 1;
                                    int i;
                                    int first = 1;
                                    for(i = 0 ; i < size ; i++) {
                                      // Suspicious (in code ?), abort.
                                      if (a[i] == ',' || a[i] == ';') {
                                        if (first) {
                                          a = NULL;
                                          break;
                                        }
                                      }
                                      // Suspicious, abort.
                                      else if (a[i] == '"' || a[i] == '\'' || a[i] != '\t' || a[i] != '\r' || a[i] != '\n') {
                                        a = NULL;
                                        break;
                                      }
                                      else if (a[i] != ' ') {
                                        first = 0;
                                      }
                                    }
                                  }
                                  if (a != NULL) {
                                    if ((opt->debug>1) && (opt->log!=NULL)) {
                                      char str[512];
                                      str[0]='\0';
                                      strncatbuff(str,a,minimum((int) (c - a + 1),32));
                                      HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"link detected in javascript: %s"LF,str); test_flush;
                                    }
                                    p=(int) (a - adr);    // p non nul: TRAITER CHAINE COMME FICHIER
                                    if (can_avoid_quotes) {
                                      ending_p=quotes_replacement;
                                    }
                                  }
                                }
                              }


                            }
                          }
                        }

                      }  /* HTSPARSE_NO_JAVASCRIPT */

                    }
                }
              }

            } else {      // ptr == 0
              //p=rech_tageq(adr,"primary");    // lien primaire, yeah
              p=0;          // No stupid tag anymore, raw link
              valid_p=1;    // Valid even if p==0
              while ((adr[p] == '\r') || (adr[p] == '\n'))
                p++;
              //can_avoid_quotes=1;
              ending_p='\r';
            }       

          } else if (isspace((unsigned char)*adr)) {
            intag_startattr=adr+1;        // attribute in tag (for dirty parsing)
          }


          // ------------------------------------------------------------
          // dernier recours - parsing "sale" : détection systématique des .gif, etc.
          // risque: générer de faux fichiers parazites
          // fix: ne parse plus dans les commentaires
          // ------------------------------------------------------------
          if ( opt->parseall && (opt->parsejava & HTSPARSE_NO_AGGRESSIVE) == 0 
            && (ptr>0) && (!in_media) /* && (!inscript_in_comments)*/ ) {   // option parsing "brut"
            //int incomment_justquit=0;
            if (!is_realspace(*adr)) {
              int noparse=0;

              // Gestion des /* */
#if 0
              if (inscript) {
                if (parseall_incomment) {
                  if ((*adr=='/') && (*(adr-1)=='*'))
                    parseall_incomment=0;
                  incomment_justquit=1;       // ne pas noter dernier caractère
                } else {
                  if ((*adr=='/') && (*(adr+1)=='*'))
                    parseall_incomment=1;
                }
              } else
                parseall_incomment=0;
#endif
              /* ensure automate state  0 (not in comments, quotes..) */
              if (inscript && ( 
                inscript_state_pos != INSCRIPT_INQUOTE && inscript_state_pos != INSCRIPT_INQUOTE2
                ) ) {
                  noparse=1;
                }

                /* vérifier que l'on est pas dans un <!-- --> pur */
                if ( (!intag) && (incomment) && (!inscript))
                  noparse=1;        /* commentaire */

                // recherche d'URLs
                if (!noparse) {
                  //if ((!parseall_incomment) && (!noparse)) {
                  if (!p) {                   // non déja trouvé
                    if (adr != r->adr) {     // >1 caractère
                      // scanner les chaines
                      if ((*adr == '\"') || (*adr=='\'')) {         // "xx.gif" 'xx.gif'
                        if (strchr("=(,",parseall_lastc)) {    // exemple: a="img.gif.. (handles comments)
                          char *a=adr;
                          char stop=*adr;  // " ou '
                          int count=0;

                          // sauter caractères
                          a++;
                          // copier
                          while((*a) && (*a!='\'') && (*a!='\"') && (count<HTS_URLMAXSIZE)) { count++; a++; }

                          // ok chaine terminée par " ou '
                          if ((*a == stop) && (count<HTS_URLMAXSIZE) && (count>0)) {
                            char c;
                            char* aend;
                            //
                            aend=a;     // sauver début
                            a++;
                            while(is_taborspace(*a)) a++;
                            c=*a;
                            if (strchr("),;>/+\r\n",c)) {     // exemple: ..img.gif";
                              // le / est pour funct("img.gif" /* URL */);
                              char BIGSTK tempo[HTS_URLMAXSIZE*2];
                              char type[256];
                              int url_ok=0;      // url valide?
                              tempo[0]='\0'; type[0]='\0';
                              //
                              strncatbuff(tempo,adr+1,count);
                              //
                              if ((!strchr(tempo,' ')) || inscript) {   // espace dedans: méfiance! (sauf dans code javascript)
                                int invalid_url=0;

                                // escape                              
                                unescape_amp(tempo);

                                // Couper au # ou ? éventuel
                                {
                                  char* a=strchr(tempo,'#');
                                  if (a)
                                    *a='\0';
                                  a=strchr(tempo,'?');
                                  if (a)
                                    *a='\0';
                                }

                                // vérifier qu'il n'y a pas de caractères spéciaux
                                if (!strnotempty(tempo))
                                  invalid_url=1;
                                else if (strchr(tempo,'*')
                                  || strchr(tempo,'<')
                                  || strchr(tempo,'>')
                                  || strchr(tempo,',')    /* list of files ? */
                                  || strchr(tempo,'\"')    /* potential parsing bug */
                                  || strchr(tempo,'\'')    /* potential parsing bug */
                                  )
                                  invalid_url=1;
                                else if (tempo[0] == '.' && isalnum(tempo[1]))   // ".gif"
                                  invalid_url=1;

                                /* non invalide? */
                                if (!invalid_url) {
                                  // Un plus à la fin? Alors ne pas prendre sauf si extension ("/toto.html#"+tag)
                                  if (c!='+') {    // PAS de plus à la fin
#if 0
                                    char* a;
#endif
                                    // "Comparisons of scheme names MUST be case-insensitive" (RFC2616)                                  
                                    if (
                                      (strfield(tempo,"http:")) 
                                      || (strfield(tempo,"ftp:"))
#if HTS_USEOPENSSL
                                      || (
                                      SSL_is_available &&
                                      (strfield(tempo,"https:"))
                                      )
#endif
#if HTS_USEMMS
																			|| strfield(tempo,"mms:")
#endif
                                      )  // ok pas de problème
                                      url_ok=1;
                                    else if (tempo[strlen(tempo)-1]=='/') {        // un slash: ok..
                                      if (inscript)   // sinon si pas javascript, méfiance (répertoire style base?)
                                        url_ok=1;
                                    } 
#if 0
                                    else if ((a=strchr(tempo,'/'))) {        // un slash: ok..
                                      if (inscript) {    // sinon si pas javascript, méfiance (style "text/css")
                                        if (strchr(a+1,'/'))     // un seul / : abandon (STYLE type='text/css')
                                          if (!strchr(tempo,' '))  // avoid spaces (too dangerous for comments)
                                            url_ok=1;
                                      }
                                    }
#endif
                                  }
                                  // Prendre si extension reconnue
                                  if (!url_ok) {
                                    get_httptype(opt,type,tempo,0);
                                    if (strnotempty(type))     // type reconnu!
                                      url_ok=1;
                                    else if (is_dyntype(get_ext(OPT_GET_BUFF(opt),tempo)))  // reconnu php,cgi,asp..
                                      url_ok=1;
                                    // MAIS pas les foobar@aol.com !!
                                    if (strchr(tempo,'@'))
                                      url_ok=0;
                                  }
                                  //
                                  // Ok, cela pourrait être une URL
                                  if (url_ok) {

                                    // Check if not fodbidden tag (id,name..)
                                    if (intag_start_valid) {
                                      if (intag_start)
                                        if (intag_startattr)
                                          if (intag)
                                            if (!inscript)
                                              if (!incomment) {
                                                int i=0,nop=0;
                                                while( (nop==0) && (strnotempty(hts_nodetect[i])) ) {
                                                  nop=rech_tageq(intag_startattr,hts_nodetect[i]);
                                                  i++;
                                                }
                                                // Forbidden tag
                                                if (nop) {
                                                  url_ok=0;
                                                  if ((opt->debug>1) && (opt->log!=NULL)) {
                                                    HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"dirty parsing: bad tag avoided: %s"LF,hts_nodetect[i-1]); test_flush;
                                                  }
                                                }
                                              }
                                    }


                                    // Accepter URL, on la traitera comme une URL normale!!
                                    if (url_ok) {
                                      valid_p = 1;
                                      p = 0;
                                    }

                                  }
                                }
                              }
                            }
                          }
                        }
                      }
                    }
                  }  // p == 0               

                } // not in comment

                // plus dans un commentaire
                if ( inscript_state_pos == INSCRIPT_START 
                  && inscript_state_pos_prev == INSCRIPT_START) {
                    parseall_lastc=*adr;             // caractère avant le prochain
                  }


            }  // if realspace
          }  // if parseall


          // ------------------------------------------------------------
          // p!=0 : on a repéré un éventuel lien
          // ------------------------------------------------------------
          //
          if ((p>0) || (valid_p)) {    // on a repéré un lien
            //int lien_valide=0;
            char* eadr=NULL;          /* fin de l'URL */
            char* quote_adr=NULL;     /* adresse du ? dans l'adresse */
            int ok=1;
            char quote='\0';
            int quoteinscript=0;
            int  noquote=0;
						char *tag_attr_start = adr;

            // si nofollow ou un stop a été déclenché, réécrire tous les liens en externe
            if ((nofollow) 
              || (opt->state.stop && /* force follow not to lose previous cache data */ !opt->is_update)
              )
              p_nocatch=1;

            // écrire codebase avant, flusher avant code
            if ((p_type==-1) || (p_type==-2)) {
              if ((opt->getmode & 1) && (ptr>0)) {
                HT_ADD_ADR;    // refresh
              }
              lastsaved=adr;    // dernier écrit+1
            }

            // sauter espaces
            // adr+=p;
            INCREMENT_CURRENT_ADR(p);
            while( ( is_space(*adr) || (
              inscriptgen 
              && adr[0] == '\\' 
              && is_space(adr[1])
              )
              )
              && quote == '\0'
              ) {
                if (!quote)
                  if ((*adr=='\"') || (*adr=='\'')) {
                    quote=*adr;                     // on doit attendre cela à la fin
                    if (inscriptgen && *(adr - 1) == '\\') {
                      quoteinscript=1;  /* will wait for \" */
                    }
                  }
                  // puis quitter
                  // adr++;    // sauter les espaces, "" et cie
                  INCREMENT_CURRENT_ADR(1);
              }

              /* Stop at \n (LF) if primary links or link lists */
              if (ptr == 0 || (in_media && strcmp(in_media,"LNK")==0))
                quote='\n';
              /* s'arrêter que ce soit un ' ou un " : pour document.write('<img src="foo'+a); par exemple! */
              else if (inscript && ! unquoted_script)
                noquote=1;

              // sauter éventuel \" ou \' javascript
              if (inscript) {    // on est dans un obj.write("..
                if (*adr=='\\') {
                  if ((*(adr+1)=='\'') || (*(adr+1)=='"')) {  // \" ou \'
                    // adr+=2;    // sauter
                    INCREMENT_CURRENT_ADR(2);
                  }
                }
              }

              // sauter content="1;URL=http://..
              if (p_searchMETAURL) {
                int l=0;
                while(
                  (adr + l + 4 < r->adr + r->size)
                  && (!strfield(adr+l,"URL=")) 
                  && (l<128) ) l++;
                if (!strfield(adr+l,"URL="))
                  ok=-1;
                else
                  adr+=(l+4);
              }

              /* éviter les javascript:document.location=.. : les parser, plutôt */
              if (ok!=-1) {
                if (strfield(adr,"javascript:") 
                  && ! inscript       /* we don't want to parse 'javascript:' inside document.write inside scripts */
                  ) {
                    ok=-1;
                    /*
                    On est désormais dans du code javascript
                    */
                    inscript_name="";
                    inscript_tag=inscript=1;
                    inscript_state_pos=INSCRIPT_START;
                    inscript_tag_lastc=quote;     /* à attendre à la fin */
                    if (opt->parsedebug) { HT_ADD("<@@ inscript @@>"); }
                  }
              }

              if (p_type==1) {
                if (*adr=='#') {
                  adr++;           // sauter # pour usemap etc
                }
              }
              eadr=adr;

              // ne pas flusher après code si on doit écrire le codebase avant!
              if ((p_type!=-1) && (p_type!=2) && (p_type!=-2)) {
                if ((opt->getmode & 1) && (ptr>0)) {
                  HT_ADD_ADR;    // refresh
                }
                lastsaved=adr;    // dernier écrit+1
                // après on écrira soit les données initiales,
                // soir une URL/lien modifié!
              } else if (p_type==-1) p_flush=adr;    // flusher jusqu'à adr ensuite

              if (ok!=-1) {    // continuer
                // découper le lien
                do {
                  if ((* (unsigned char*) eadr)<32) {   // caractère de contrôle (ou \0)
                    if (!is_space(*eadr))
                      ok=0; 
                  }
                  if ( ( ((int) (eadr - adr)) ) > HTS_URLMAXSIZE)  // ** trop long, >HTS_URLMAXSIZE caractères (on prévoit HTS_URLMAXSIZE autres pour path)
                    ok=-1;    // ne pas traiter ce lien

                  if (ok > 0) {
                    //if (*eadr!=' ') {  
                    if (is_space(*eadr)) {   // guillemets,CR, etc
                      if ( 
                        ( *eadr == quote && ( !quoteinscript || *(eadr -1) == '\\') )  // end quote
                        || ( noquote && (*eadr == '\"' || *eadr == '\'') )       // end at any quote
                        || (!noquote && quote == '\0' && is_realspace(*eadr) )   // unquoted href
                        )     // si pas d'attente de quote spéciale ou si quote atteinte
                        ok=0; 
                    } else if (ending_p && (*eadr==ending_p))
                      ok=0;
                    else {
                      switch(*eadr) {
                    case '>': 
                      if (!quote) {
                        if (!inscript && !in_media) {
                          intag=0;    // PLUS dans un tag!
                          intag_start_valid=0;
													intag_name = NULL;
                        }
                        ok=0;
                      }
                      break;
                      /*case '<':*/ 
                    case '#': 
                      if (*(eadr-1) != '&')       // &#40;
                        ok=0; 
                      break;
                      // case '?': non!
                    case '\\': if (inscript) ok=0; break;     // \" ou \' point d'arrêt
                    case '?': quote_adr=adr; break;           // noter position query
                      }
                    }
                    //}
                  } 
                  eadr++;
                } while(ok==1);

                // Empty link detected
                if ( (((int) (eadr - adr))) <= 1) {       // link empty
                  ok=-1;        // No
                  if (*adr != '#') {        // Not empty+unique #
                    if ( (((int) (eadr - adr)) == 1)) {       // 1=link empty with delim (end_adr-start_adr)
                      if (quote) {
                        if ((opt->getmode & 1) && (ptr>0)) { 
                          HT_ADD("#");        // We add this for a <href="">
                        }
                      }
                    }
                  }
                }

                // This is a dirty and horrible hack to avoid parsing an Adobe GoLive bogus tag
                if (strfield(adr, "(Empty Reference!)")) {
                  ok=-1;        // No
                }

              }

              if (ok==0) {    // tester un lien
                char BIGSTK lien[HTS_URLMAXSIZE*2];
                int meme_adresse=0;      // 0 par défaut pour primary
                //char *copie_de_adr=adr;
                //char* p;

                // construire lien (découpage)
                if ( (((int) (eadr -  adr))-1) < HTS_URLMAXSIZE  ) {    // pas trop long?
                  strncpy(lien,adr,((int) (eadr - adr))-1);
                  *(lien+  (((int) (eadr -  adr)))-1  )='\0';
                  //printf("link: %s\n",lien);          
                  // supprimer les espaces
                  while((lien[strlen(lien)-1]==' ') && (strnotempty(lien))) lien[strlen(lien)-1]='\0';


                } else
                  lien[0]='\0';    // erreur


                // ------------------------------------------------------
                // Lien repéré et extrait
                if (strnotempty(lien)>0) {           // construction du lien
                  char BIGSTK adr[HTS_URLMAXSIZE*2],fil[HTS_URLMAXSIZE*2];          // ATTENTION adr cache le "vrai" adr
                  int forbidden_url=-1;              // lien non interdit (mais non autorisé..)
                  int just_test_it=0;                // mode de test des liens
                  int set_prio_to=0;                 // pour capture de page isolée
                  int import_done=0;                 // lien importé (ne pas scanner ensuite *à priori*)
                  //
                  adr[0]='\0'; fil[0]='\0';
                  //
                  // 0: autorisé
                  // 1: interdit (patcher tout de même adresse)

                  if ((opt->debug>1) && (opt->log!=NULL)) {
                    HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"link detected in html (tag): %s"LF,lien); test_flush;
                  }

                  // external check
                  if (!RUN_CALLBACK1(opt, linkdetected, lien) || !RUN_CALLBACK2(opt, linkdetected2, lien, intag_start)) {
                    error=1;    // erreur
                    if (opt->log) {
                      HTS_LOG(opt,LOG_ERROR); fprintf(opt->log,"Link %s refused by external wrapper"LF,lien);
                      test_flush;
                    }
                  }

#if HTS_STRIP_DOUBLE_SLASH
                  // supprimer les // en / (sauf pour http://)
                  if (opt->urlhack) {
                    char *a,*p,*q;
                    int done=0;
                    a=strchr(lien,':');    // http://
                    if (a) {
                      a++;
                      while(*a=='/') a++;    // position après http://
                    } else {
                      a=lien;                // début
                      while(*a=='/') a++;    // position après http://
                    }
                    q=strchr(a,'?');     // ne pas traiter après '?'
                    if (!q)
                      q=a+strlen(a)-1;
                    while(( p=strstr(a,"//")) && (!done) ) {    // remplacer // par /
                      if ((int) p>(int) q) {   // après le ? (toto.cgi?param=1//2.3)
                        done=1;    // stopper
                      } else {
                        char BIGSTK tempo[HTS_URLMAXSIZE*2];
                        tempo[0]='\0';
                        strncatbuff(tempo,a,(int) p - (int) a);
                        strcatbuff (tempo,p+1);
                        strcpybuff(a,tempo);    // recopier
                      }
                    }
                  }
#endif

                  // purger espaces de début et fin, CR,LF résiduels
                  // (IMG SRC="foo.<\n><\t>gif<\t>")
                  {
                    char* a = lien;
                    size_t llen;

                    // strip ending spaces
                    llen = ( *a != '\0' ) ? strlen(a) : 0;
                    while(llen > 0 && is_realspace(lien[llen - 1]) ) {
                      a[--llen]='\0';
                    } 
                    //  skip leading ones
                    while(is_realspace(*a)) a++;
                    // strip cr, lf, tab inside URL
                    llen = 0;
                    while(*a) {
                      if (*a != '\n' && *a != '\r' && *a != '\t') {
                        lien[llen++] = *a;
                      }
                      a++;
                    }
                    lien[llen] = '\0';
                  }

                  // commas are forbidden
                  if (archivetag_p) {
                    if (strchr(lien, ',')) {
                      error=1;    // erreur
                      if ((opt->debug>1) && (opt->log!=NULL)) {
                        HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"link rejected (multiple-archive) %s"LF,lien); test_flush;
                      }
                    }
                  }               

                  /* Unescape/escape %20 and other &nbsp; */
                  {
                    char BIGSTK query[HTS_URLMAXSIZE*2];
                    char* a=strchr(lien,'?');
                    if (a) {
                      strcpybuff(query,a);
                      *a='\0';
                    } else
                      query[0]='\0';
                    // conversion &amp; -> & et autres joyeusetés
                    unescape_amp(lien);
                    unescape_amp(query);
                    // décoder l'inutile (%2E par exemple) et coder espaces
                    // Bad: strcpybuff(lien,unescape_http(lien));
                    // Bad: strcpybuff(lien,unescape_http_unharm(lien, (no_esc_utf)?0:1));
										/* Never unescape high-chars (we don't know the encoding!!) */
                    strcpybuff(lien,unescape_http_unharm(catbuff,lien, 1));   /* note: '%' is still escaped */
                    escape_remove_control(lien);
                    // ???? No! escape_spc_url(lien);
                    strcatbuff(lien,query);     /* restore */
                  }

                  // convertir les éventuels \ en des / pour éviter des problèmes de reconnaissance!
                  {
                    char* a;
                    for(a = jump_identification(lien) ; *a != '\0' && *a != '?' ; a++) {
                      if (*a == '\\') {
                        *a = '/';
                      }
                    }
                  }

                  // supprimer le(s) ./
                  while ((lien[0]=='.') && (lien[1]=='/')) {
                    char BIGSTK tempo[HTS_URLMAXSIZE*2];
                    strcpybuff(tempo,lien+2);
                    strcpybuff(lien,tempo);
                  }
                  if (strnotempty(lien)==0)  // sauf si plus de nom de fichier
                    strcpybuff(lien,"./");

                  // vérifie les /~machin -> /~machin/
                  // supposition dangereuse?
                  // OUI!!
#if HTS_TILDE_SLASH
                  if (lien[strlen(lien)-1]!='/') {
                    char *a=lien+strlen(lien)-1;
                    // éviter aussi index~1.html
                    while (((int) a>(int) lien) && (*a!='~') && (*a!='/') && (*a!='.')) a--;
                    if (*a=='~') {
                      strcatbuff(lien,"/");    // ajouter slash
                    }
                  }
#endif

                  // APPLET CODE="mixer.MixerApplet.class" --> APPLET CODE="mixer/MixerApplet.class"
                  // yes, this is dirty
                  // but I'm so lazzy..
                  // and besides the java "code" convention is really a pain in html code
                  if (p_type==-1) {
                    char* a=strrchr(lien,'.');
                    add_class_dots_to_patch=0;
                    if (a) {
                      char* b;
                      do {
                        b=strchr(lien,'.');
                        if ((b != a) && (b)) {
                          add_class_dots_to_patch++;
                          *b='/';
                        }
                      } while((b != a) && (b));
                    }
                  }

                  // éliminer les éventuels :80 (port par défaut!)
                  if (link_has_authority(lien)) {
                    char * a;
                    a=strstr(lien,"//");    // "//" authority
                    if (a)
                      a+=2;
                    else
                      a=lien;
                    // while((*a) && (*a!='/') && (*a!=':')) a++;
                    a=jump_toport(a);
                    if (a) {  // port
                      int port=0;
                      int defport=80;
                      char* b=a+1;
#if HTS_USEOPENSSL
                      // FIXME
                      //if (strfield(adr, "https:")) {
                      //}
#endif
                      while(isdigit((unsigned char)*b)) { port*=10; port+=(int) (*b-'0'); b++; }
                      if (port==defport) {  // port 80, default - c'est débile
                        char BIGSTK tempo[HTS_URLMAXSIZE*2];
                        tempo[0]='\0';
                        strncatbuff(tempo,lien,(int) (a - lien));
                        strcatbuff(tempo,a+3);  // sauter :80
                        strcpybuff(lien,tempo);
                      }
                    }
                  }

                  // filtrer les parazites (mailto & cie)
                  /*
                  if (strfield(lien,"mailto:")) {  // ne pas traiter
                  error=1;
                  } else if (strfield(lien,"news:")) {  // ne pas traiter
                  error=1;
                  }
                  */

                  // vérifier que l'on ne doit pas ajouter de .class
                  if (!error) {
                    if (add_class) {
                      char *a = lien+strlen(lien)-1;
                      while(( a > lien) && (*a!='/') && (*a!='.')) a--;
                      if (*a != '.')
                        strcatbuff(lien,".class");    // ajouter .class
                      else if (!strfield2(a,".class"))
                        strcatbuff(lien,".class");    // idem
                    }
                  }

                  // si c'est un chemin, alors vérifier (toto/toto.html -> http://www/toto/)
                  if (!error) {
                    if ((opt->debug>1) && (opt->log!=NULL)) {
                      HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"position link check %s"LF,lien); test_flush;
                    }

                    if ((p_type==2) || (p_type==-2)) {   // code ou codebase                        
                      // Vérifier les codebase=applet (au lieu de applet/)
                      if (p_type==-2) {    // codebase
                        if (strnotempty(lien)) {
                          if (fil[strlen(lien)-1]!='/') {  // pas répertoire
                            strcatbuff(lien,"/");
                          }
                        }
                      }

                      /* base has always authority */
                      if (p_type==2 && !link_has_authority(lien)) {
                        char BIGSTK tmp[HTS_URLMAXSIZE*2];
                        strcpybuff(tmp, "http://");
                        strcatbuff(tmp, lien);
                        strcpybuff(lien, tmp);
                      }

                      /* only one ending / (bug on some pages) */
                      if ((int)strlen(lien)>2) {
                        int len = (int) strlen(lien);
                        while(len > 1 && lien[len-1] == '/' && lien[len-2] == '/' )    /* double // (bug) */
                          lien[--len]='\0';
                      }
                      // copier nom host si besoin est
                      if (!link_has_authority(lien)) {  // pas de http://
                        char BIGSTK adr2[HTS_URLMAXSIZE*2],fil2[HTS_URLMAXSIZE*2];  // ** euh ident_url_relatif??
                        if (ident_url_relatif(lien,urladr,urlfil,adr2,fil2)<0) {                        
                          error=1;
                        } else {
                          strcpybuff(lien,"http://");
                          strcatbuff(lien,adr2);
                          if (*fil2!='/')
                            strcatbuff(lien,"/");
                          strcatbuff(lien,fil2);
                          {
                            char* a;
                            a=lien+strlen(lien)-1;
                            while((*a) && (*a!='/') && ( a> lien)) a--;
                            if (*a=='/') {
                              *(a+1)='\0';
                            }
                          }
                          //char BIGSTK tempo[HTS_URLMAXSIZE*2];
                          //strcpybuff(tempo,"http://");
                          //strcatbuff(tempo,urladr);    // host
                          //if (*lien!='/')
                          //  strcatbuff(tempo,"/");
                          //strcatbuff(tempo,lien);
                          //strcpybuff(lien,tempo);
                        }
                      }

                      if (!error) {  // pas d'erreur?
                        if (p_type==2) {   // code ET PAS codebase      
                          char* a=lien+strlen(lien)-1;
                          char* start_of_filename = jump_identification(lien);
                          if (start_of_filename != NULL 
                            && (start_of_filename = strchr(start_of_filename, '/')) != NULL)
                            start_of_filename++;
                          if (start_of_filename == NULL)
                            strcatbuff(lien, "/");
                          while( (a > lien) && (*a) && (*a!='/')) a--;
                          if (*a=='/') {     // ok on a repéré le dernier /
                            if (start_of_filename != NULL && a + 1 >= start_of_filename) {
                              *(a+1)='\0';   // couper
                            }
                          } else {
                            *lien='\0';    // éliminer
                            error=1;   // erreur, ne pas poursuivre
                          }      
                        }

                        // stocker base ou codebase?
                        switch(p_type) {
                      case 2: { 
                        //if (*lien!='/') strcatbuff(base,"/");
                        strcpybuff(base,lien);
                              }
                              break;      // base
                      case -2: {
                        //if (*lien!='/') strcatbuff(codebase,"/");
                        strcpybuff(codebase,lien); 
                               }
                               break;  // base
                        }

                        if ((opt->debug>1) && (opt->log!=NULL)) {
                          HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"code/codebase link %s base %s"LF,lien,base); test_flush;
                        }
                        //printf("base code: %s - %s\n",lien,base);
                      }

                    } else {
                      char* _base;
                      if (p_type==-1)   // code (applet)
                        _base=codebase;
                      else
                        _base=base;


                      // ajouter chemin de base href..
                      if (strnotempty(_base)) {       // considérer base
                        if (!link_has_authority(lien)) {    // non absolue
                          if (*lien!='/') {           // non absolu sur le site (/)
                            if ( ((int) strlen(_base)+(int) strlen(lien))<HTS_URLMAXSIZE) {
                              // mailto: and co: do NOT add base
                              if (ident_url_relatif(lien,urladr,urlfil,adr,fil)>=0) {
                                char BIGSTK tempo[HTS_URLMAXSIZE*2];
                                // base est absolue
                                strcpybuff(tempo,_base);
                                strcatbuff(tempo,lien + ((*lien=='/')?1:0) );
                                strcpybuff(lien,tempo);        // patcher en considérant base
                                // ** vérifier que ../ fonctionne (ne doit pas arriver mais bon..)

                                if ((opt->debug>1) && (opt->log!=NULL)) {
                                  HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"link modified with code/codebase %s"LF,lien); test_flush;
                                }
                              }
                            } else {
                              error=1;    // erreur
                              if (opt->log) {
                                HTS_LOG(opt,LOG_ERROR); fprintf(opt->log,"Link %s too long with base href"LF,lien);
                                test_flush;
                              }
                            }
                          } else {
                            char BIGSTK badr[HTS_URLMAXSIZE*2], bfil[HTS_URLMAXSIZE*2];
                            if (ident_url_absolute(_base, badr, bfil) >=0 ) {
                              if ( ((int) strlen(badr)+(int) strlen(lien)) < HTS_URLMAXSIZE) {
                                char BIGSTK tempo[HTS_URLMAXSIZE*2];
                                // base est absolue
                                tempo[0] = '\0';
                                if (!link_has_authority(badr)) {
                                  strcatbuff(tempo, "http://");
                                }
                                strcatbuff(tempo,badr);
                                strcatbuff(tempo,lien);
                                strcpybuff(lien,tempo);        // patcher en considérant base

                                if ((opt->debug>1) && (opt->log!=NULL)) {
                                  HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"link modified with code/codebase %s"LF,lien); test_flush;
                                }
                              } else {
                                error=1;    // erreur
                                if (opt->log) {
                                  HTS_LOG(opt,LOG_ERROR); fprintf(opt->log,"Link %s too long with base href"LF,lien);
                                  test_flush;
                                }
                              }
                            }
                          }
                        }
                      }


                    }
                  }

                  // transformer lien quelconque (http, relatif, etc) en une adresse
                  // et un chemin+fichier (adr,fil)
                  if (!error) {
                    int reponse;
                    if ((opt->debug>1) && (opt->log!=NULL)) {
                      HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"build relative link %s with %s%s"LF,lien,relativeurladr,relativeurlfil); test_flush;
                    }
                    if ((reponse=ident_url_relatif(lien,relativeurladr,relativeurlfil,adr,fil))<0) {                        
                      adr[0]='\0';    // erreur
                      if (reponse==-2) {
                        if (opt->log) {
                          HTS_LOG(opt,LOG_WARNING); fprintf(opt->log,"Link %s not caught (unknown protocol)"LF,lien);
                          test_flush;
                        }
                      } else {
                        if ((opt->debug>1) && (opt->log!=NULL)) {
                          HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"ident_url_relatif failed for %s with %s%s"LF,lien,relativeurladr,relativeurlfil); test_flush;
                        }
                      }
                    } else {
                      if ((opt->debug>1) && (opt->log!=NULL)) {
                        HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"built relative link %s with %s%s -> %s%s"LF,lien,relativeurladr,relativeurlfil,adr,fil); test_flush;
                      }
                    }
                  } else {
                    if ((opt->debug>1) && (opt->log!=NULL)) {
                      HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"link %s not build, error detected before"LF,lien); test_flush;
                    }
                    adr[0]='\0';
                  }

#if HTS_CHECK_STRANGEDIR
                  // !ATTENTION!
                  // Ici on teste les exotiques du genre www.truc.fr/machin (sans slash à la fin)
                  // je n'ai pas encore trouvé le moyen de faire la différence entre un répertoire
                  // et un fichier en http A PRIORI : je fais donc un test
                  // En cas de moved xxx, on recalcule adr et fil, tout simplement
                  // DEFAUT: test effectué plusieurs fois! à revoir!!!
                  if ((adr[0]!='\0') && (strcmp(adr,"file://") && (p_type!=2) && (p_type!=-2)) {
                    //## if ((adr[0]!='\0') && (adr[0]!=lOCAL_CHAR) && (p_type!=2) && (p_type!=-2)) {
                    if (fil[strlen(fil)-1]!='/') {  // pas répertoire
                      if (ishtml(opt,fil)==-2) {    // pas d'extension
                        char BIGSTK loc[HTS_URLMAXSIZE*2];  // éventuelle nouvelle position
                        loc[0]='\0';
                        if ((opt->debug>1) && (opt->log!=NULL)) {
                          HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"link-check-directory: %s%s"LF,adr,fil);
                          test_flush;
                        }

                        // tester éventuelle nouvelle position
                        switch (http_location(adr,fil,loc).statuscode) {
                      case 200: // ok au final
                        if (strnotempty(loc)) {  // a changé d'adresse
                          if (opt->log) {
                            HTS_LOG(opt,LOG_WARNING); fprintf(opt->log,"Link %s%s has moved to %s for %s%s"LF,adr,fil,loc,urladr,urlfil);
                            test_flush;
                          }

                          // recalculer adr et fil!
                          if (ident_url_absolute(loc,adr,fil)==-1) {
                            adr[0]='\0';  // cancel
                            if ((opt->debug>1) && (opt->log!=NULL)) {
                              HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"link-check-dir: %s%s"LF,adr,fil);
                              test_flush;
                            }
                          }

                        }
                        break;
                      case -2: case -3:  // timeout ou erreur grave
                        if (opt->log) {
                          HTS_LOG(opt,LOG_WARNING); fprintf(opt->log,"Connection too slow for testing link %s%s (from %s%s)"LF,adr,fil,urladr,urlfil);
                          test_flush;
                        }

                        break;
                        }

                      }
                    } 
                  }
#endif

                  // Le lien doit juste être réécrit, mais ne doit pas générer un lien
                  // exemple: <FORM ACTION="url_cgi">
                  if (p_nocatch) {
                    forbidden_url=1;    // interdire récupération du lien
                    if ((opt->debug>1) && (opt->log!=NULL)) {
                      HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"link forced external at %s%s"LF,adr,fil);
                      test_flush;
                    }
                  }

                  // Tester si un lien doit être accepté ou refusé (wizard)
                  // forbidden_url=1 : lien refusé
                  // forbidden_url=0 : lien accepté
                  //if ((ptr>0) && (p_type!=2) && (p_type!=-2)) {    // tester autorisations?
                  if ((p_type!=2) && (p_type!=-2)) {    // tester autorisations?
                    if (!p_nocatch) {
                      if (adr[0]!='\0') {          
                        if ((opt->debug>1) && (opt->log!=NULL)) {
                          HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"wizard link test at %s%s.."LF,adr,fil);
                          test_flush;
                        }
                        forbidden_url=hts_acceptlink(opt,ptr,lien_tot,liens,
                          adr,fil,
													intag_name ? intag_name : NULL, intag_name ? tag_attr_start : NULL,
                          &set_prio_to,
                          &just_test_it);
                        if ((opt->debug>1) && (opt->log!=NULL)) {
                          HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"result for wizard link test: %d"LF,forbidden_url);
                          test_flush;
                        }
                      }
                    }
                  }

                  // calculer meme_adresse
                  meme_adresse=strfield2(jump_identification(adr),jump_identification(urladr));

                  // Début partie sauvegarde

                  // ici on forme le nom du fichier à sauver, et on patche l'URL
                  if (adr[0]!='\0') {
                    // savename: simplifier les ../ et autres joyeusetés
                    char BIGSTK save[HTS_URLMAXSIZE*2];
                    int r_sv=0;
                    // En cas de moved, adresse première
                    char BIGSTK former_adr[HTS_URLMAXSIZE*2];
                    char BIGSTK former_fil[HTS_URLMAXSIZE*2];
                    //
                    save[0]='\0'; former_adr[0]='\0'; former_fil[0]='\0';
                    //

                    // nom du chemin à sauver si on doit le calculer
                    // note: url_savename peut décider de tester le lien si il le trouve
                    // suspect, et modifier alors adr et fil
                    // dans ce cas on aura une référence directe au lieu des traditionnels
                    // moved en cascade (impossible à reproduire à priori en local, lorsque des fichiers
                    // gif sont impliqués par exemple)
                    if ((p_type!=2) && (p_type!=-2)) {  // pas base href ou codebase
                      if (forbidden_url!=1) {
                        char BIGSTK last_adr[HTS_URLMAXSIZE*2];

                        /* Calc */
                        last_adr[0]='\0';
                        //char last_fil[HTS_URLMAXSIZE*2]="";
                        strcpybuff(last_adr,adr);    // ancienne adresse
                        //strcpybuff(last_fil,fil);    // ancien chemin
                        r_sv=url_savename2(adr,fil,save,former_adr,former_fil,liens[ptr]->adr,liens[ptr]->fil,opt,liens,lien_tot,sback,cache,hash,ptr,numero_passe,NULL,str->page_charset_);
                        if (strcmp(jump_identification(last_adr),jump_identification(adr)) != 0) {  // a changé

                          // 2e test si moved

                          // Tester si un lien doit être accepté ou refusé (wizard)
                          // forbidden_url=1 : lien refusé
                          // forbidden_url=0 : lien accepté
                          if ((ptr>0) && (p_type!=2) && (p_type!=-2)) {    // tester autorisations?
                            if (!p_nocatch) {
                              if (adr[0]!='\0') {          
                                if ((opt->debug>1) && (opt->log!=NULL)) {
                                  HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"wizard moved link retest at %s%s.."LF,adr,fil);
                                  test_flush;
                                }
                                forbidden_url=hts_acceptlink(opt,ptr,lien_tot,liens,
                                  adr,fil,
                                  intag_name ? intag_name : NULL, intag_name ? tag_attr_start : NULL,
                                  &set_prio_to,
                                  &just_test_it);
                                if ((opt->debug>1) && (opt->log!=NULL)) {
                                  HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"result for wizard moved link retest: %d"LF,forbidden_url);
                                  test_flush;
                                }
                              }
                            }
                          }

                          //import_done=1;    // c'est un import!
                          meme_adresse=0;   // on a changé
                        }
                      } else {
                        strcpybuff(save,"");  // dummy
                      }
                    }

                    // resolve unresolved type
                    if (r_sv!=-1
                      && p_type != 2 && p_type != -2
                      && forbidden_url == 0
                      && IS_DELAYED_EXT(save)
                      )
                    {
                      time_t t;
                      
                      // pas d'erreur, on continue
                      r_sv = hts_wait_delayed(str, adr, fil, save, parenturladr, parenturlfil, former_adr, former_fil, &forbidden_url);

                      /* User interaction, because hts_wait_delayed can be slow.. (3.43) */
                      t = time(NULL);
                      if (user_interact_timestamp == 0 || t - user_interact_timestamp > 0) {
                        user_interact_timestamp = t;
                        ENGINE_SAVE_CONTEXT();
                        {
                          hts_mirror_process_user_interaction(str, stre);
                        }
                        ENGINE_SET_CONTEXT();
                      }
                    }

                    // record!
                    if (r_sv!=-1) {  // pas d'erreur, on continue
                      /* log */
                      if ((opt->debug>1) && (opt->log!=NULL)) {
                        HTS_LOG(opt,LOG_DEBUG);
                        if (forbidden_url!=1) {    // le lien va être chargé
                          if ((p_type==2) || (p_type==-2)) {  // base href ou codebase, pas un lien
                            fprintf(opt->log,"Code/Codebase: %s%s"LF,adr,fil);
                          } else if ((opt->getmode & 4)==0) {
                            fprintf(opt->log,"Record: %s%s -> %s"LF,adr,fil,save);
                          } else {
                            if (!ishtml(opt,fil))
                              fprintf(opt->log,"Record after: %s%s -> %s"LF,adr,fil,save);
                            else
                              fprintf(opt->log,"Record: %s%s -> %s"LF,adr,fil,save);
                          } 
                        } else
                          fprintf(opt->log,"External: %s%s"LF,adr,fil);
                        test_flush;
                      }
                      /* FIN log */

                      // écrire lien
                      if ((p_type==2) || (p_type==-2)) {  // base href ou codebase, sauter
                        lastsaved=eadr-1+1;  // sauter "
                      }
                      /* */
                      else if (opt->urlmode==0) {    // URL absolue dans tous les cas
                        if ((opt->getmode & 1) && (ptr>0)) {    // ecrire les html
                          if (!link_has_authority(adr)) {
                            HT_ADD("http://");
                          } else {
                            char* aut = strstr(adr, "//");
                            if (aut) {
                              char tmp[256];
                              tmp[0]='\0';
                              strncatbuff(tmp, adr, (int) (aut - adr));   // scheme
                              HT_ADD(tmp);          // Protocol
                              HT_ADD("//");
                            }
                          }

                          if (!opt->passprivacy) {
                            HT_ADD_HTMLESCAPED(jump_protocol(adr));           // Password
                          } else {
                            HT_ADD_HTMLESCAPED(jump_identification(adr));     // No Password
                          }
                          if (*fil!='/')
                            HT_ADD("/");
                          HT_ADD_HTMLESCAPED(fil);
                        }
                        lastsaved=eadr-1;    // dernier écrit+1 (enfin euh apres on fait un ++ alors hein)
                        /* */
                      } else if (opt->urlmode == 4) {    // ne rien faire!
                        /* */
                        /* leave the link 'as is' */
                        /* Sinon, dépend de interne/externe */
                      } else if (forbidden_url==1) {    // le lien ne sera pas chargé, référence externe!
                        if ((opt->getmode & 1) && (ptr>0)) {
                          if (p_type!=-1) {     // pas que le nom de fichier (pas classe java)
                            if (!opt->external) {
                              if (!link_has_authority(adr)) {
                                HT_ADD("http://");
                                if (!opt->passprivacy) {
                                  HT_ADD_HTMLESCAPED(adr);     // Password
                                } else {
                                  HT_ADD_HTMLESCAPED(jump_identification(adr));     // No Password
                                }
                                if (*fil!='/')
                                  HT_ADD("/");
                                HT_ADD_HTMLESCAPED(fil);
                              } else {
                                char* aut = strstr(adr, "//");
                                if (aut) {
                                  char tmp[256];
                                  tmp[0]='\0';
                                  strncatbuff(tmp, adr, (int) (aut - adr));   // scheme
                                  HT_ADD(tmp);          // Protocol
                                  HT_ADD("//");
                                  if (!opt->passprivacy) {
                                    HT_ADD_HTMLESCAPED(jump_protocol(adr));          // Password
                                  } else {
                                    HT_ADD_HTMLESCAPED(jump_identification(adr));     // No Password
                                  }
                                  if (*fil!='/')
                                    HT_ADD("/");
                                  HT_ADD_HTMLESCAPED(fil);
                                }
                              }
                              //
                            } else {    // fichier/page externe, mais on veut générer une erreur
                              //
                              int patch_it=0;
                              int add_url=0;
                              char* cat_name=NULL;
                              char* cat_data=NULL;
                              int cat_nb=0;
                              int cat_data_len=0;

                              // ajouter lien external
                              switch ( (link_has_authority(adr)) ? 1 : ( (fil[strlen(fil)-1]=='/')?1:(ishtml(opt,fil))  ) ) {
                            case 1: case -2:       // html ou répertoire
                              if (opt->getmode & 1) {  // sauver html
                                patch_it=1;   // redirect
                                add_url=1;    // avec link?
                                cat_name="external.html";
                                cat_nb=0;
                                cat_data=HTS_DATA_UNKNOWN_HTML;
                                cat_data_len=HTS_DATA_UNKNOWN_HTML_LEN;
                              }
                              break;
                            default:    // inconnu
                              // asp, cgi..
                              if ( (strfield2(fil+max(0,(int)strlen(fil)-4),".gif")) 
                                || (strfield2(fil+max(0,(int)strlen(fil)-4),".jpg")) 
                                || (strfield2(fil+max(0,(int)strlen(fil)-4),".xbm")) 
                                /*|| (ishtml(opt,fil)!=0)*/ ) {
                                patch_it=1;   // redirect
                              add_url=1;    // avec link aussi
                              cat_name="external.gif";
                              cat_nb=1;
                              cat_data=HTS_DATA_UNKNOWN_GIF;
                              cat_data_len=HTS_DATA_UNKNOWN_GIF_LEN;
                                } else /* if (is_dyntype(get_ext(fil))) */ {
                                  patch_it=1;   // redirect
                                  add_url=1;    // avec link?
                                  cat_name="external.html";
                                  cat_nb=0;
                                  cat_data=HTS_DATA_UNKNOWN_HTML;
                                  cat_data_len=HTS_DATA_UNKNOWN_HTML_LEN;
                                }
                                break;
                              }// html,gif

                              if (patch_it) {
                                char BIGSTK save[HTS_URLMAXSIZE*2];
                                char BIGSTK tempo[HTS_URLMAXSIZE*2];
                                strcpybuff(save,StringBuff(opt->path_html_utf8));
                                strcatbuff(save,cat_name);
                                if (lienrelatif(tempo,save, relativesavename)==0) {
																	/* Never escape high-chars (we don't know the encoding!!) */
                                  escape_uri_utf(tempo);     // escape with %xx
                                  //if (!no_esc_utf)
                                  //  escape_uri(tempo);     // escape with %xx
                                  //else
                                  //  escape_uri_utf(tempo);     // escape with %xx
                                  HT_ADD_HTMLESCAPED(tempo);    // page externe
                                  if (add_url) {
                                    HT_ADD("?link=");    // page externe

                                    // same as above
                                    if (!link_has_authority(adr)) {
                                      HT_ADD("http://");
                                      if (!opt->passprivacy) {
                                        HT_ADD_HTMLESCAPED(adr);     // Password
                                      } else {
                                        HT_ADD_HTMLESCAPED(jump_identification(adr));     // No Password
                                      }
                                      if (*fil!='/')
                                        HT_ADD("/");
                                      HT_ADD_HTMLESCAPED(fil);
                                    } else {
                                      char* aut = strstr(adr, "//");
                                      if (aut) {
                                        char tmp[256];
                                        tmp[0]='\0';
                                        strncatbuff(tmp, adr, (int) (aut - adr) + 2);   // scheme
                                        HT_ADD(tmp);
                                        if (!opt->passprivacy) {
                                          HT_ADD_HTMLESCAPED(jump_protocol(adr));          // Password
                                        } else {
                                          HT_ADD_HTMLESCAPED(jump_identification(adr));     // No Password
                                        }
                                        if (*fil!='/')
                                          HT_ADD("/");
                                        HT_ADD_HTMLESCAPED(fil);
                                      }
                                    }
                                    //

                                  }
                                }

                                // écrire fichier?
                                if (verif_external(opt,cat_nb,1)) {
                                  FILE* fp = filecreate(&opt->state.strc, fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_html_utf8),cat_name));
                                  if (fp) {
                                    if (cat_data_len==0) {   // texte
                                      verif_backblue(opt,StringBuff(opt->path_html_utf8));
                                      fprintf(fp,"%s%s","<!-- Created by HTTrack Website Copier/"HTTRACK_VERSION" "HTTRACK_AFF_AUTHORS" -->"LF,cat_data);
                                    } else {                    // data
                                      fwrite(cat_data,cat_data_len,1,fp);
                                    }
                                    fclose(fp);
                                    usercommand(opt,0,NULL,fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_html_utf8),cat_name),"","");
                                  }
                                }
                              }  else {    // écrire normalement le nom de fichier
                                HT_ADD("http://");
                                if (!opt->passprivacy) {
                                  HT_ADD_HTMLESCAPED(adr);       // Password
                                } else {
                                  HT_ADD_HTMLESCAPED(jump_identification(adr));       // No Password
                                }
                                if (*fil!='/')
                                  HT_ADD("/");
                                HT_ADD_HTMLESCAPED(fil);
                              }// patcher?
                            }  // external
                          } else {  // que le nom de fichier (classe java)
                            // en gros recopie de plus bas: copier codebase et base
                            if (p_flush) {
                              char BIGSTK tempo[HTS_URLMAXSIZE*2];    // <-- ajouté
                              char BIGSTK tempo_pat[HTS_URLMAXSIZE*2];

                              // Calculer chemin
                              tempo_pat[0]='\0';
                              strcpybuff(tempo,fil);  // <-- ajouté
                              {
                                char* a=strrchr(tempo,'/');

                                // Example: we converted code="x.y.z.foo.class" into "x/y/z/foo.class"
                                // we have to do the contrary now
                                if (add_class_dots_to_patch>0) {
                                  while( (add_class_dots_to_patch>0) && (a) ) {
                                    *a='.';     // convert "false" java / into .
                                    add_class_dots_to_patch--;
                                    a=strrchr(tempo,'/');
                                  }
                                  // if add_class_dots_to_patch, this is because there is a problem!!
                                  if (add_class_dots_to_patch) {
                                    if (opt->log) {
                                      HTS_LOG(opt,LOG_WARNING); fprintf(opt->log,"Error: can not rewind java path %s, check html code"LF,tempo);
                                      test_flush;
                                    }
                                  }
                                }

                                // Cut path/filename
                                if (a) {
                                  char BIGSTK tempo2[HTS_URLMAXSIZE*2];
                                  strcpybuff(tempo2,a+1);         // FICHIER
                                  strncatbuff(tempo_pat,tempo,(int) (a - tempo)+1);  // chemin
                                  strcpybuff(tempo,tempo2);                     // fichier
                                }
                              }

                              // érire codebase="chemin"
                              if ((opt->getmode & 1) && (ptr>0)) {
                                char BIGSTK tempo4[HTS_URLMAXSIZE*2];
                                tempo4[0]='\0';

                                if (strnotempty(tempo_pat)) {
                                  HT_ADD("codebase=\"http://");
                                  if (!opt->passprivacy) {
                                    HT_ADD_HTMLESCAPED(adr);  // Password
                                  } else {
                                    HT_ADD_HTMLESCAPED(jump_identification(adr));  // No Password
                                  }
                                  if (*tempo_pat!='/') HT_ADD("/");
                                  HT_ADD(tempo_pat);
                                  HT_ADD("\" ");
                                }

                                strncatbuff(tempo4,lastsaved,(int) (p_flush - lastsaved));
                                HT_ADD(tempo4);    // refresh code="
                                HT_ADD(tempo);
                              }
                            }
                          }
                        }
                        lastsaved=eadr-1;
                      }
                      /*
                      else if (opt->urlmode==1) {    // ABSOLU, c'est le cas le moins courant
                      //  NE FONCTIONNE PAS!!  (et est inutile)
                      if ((opt->getmode & 1) && (ptr>0)) {    // ecrire les html
                      // écrire le lien modifié, absolu
                      HT_ADD("file:");
                      if (*save=='/')
                      HT_ADD(save+1)
                      else
                      HT_ADD(save)
                      }
                      lastsaved=eadr-1;    // dernier écrit+1 (enfin euh apres on fait un ++ alors hein)
                      }
                      */
                      else if (opt->mimehtml) {
                        char BIGSTK buff[HTS_URLMAXSIZE*3];
                        HT_ADD("cid:");
                        strcpybuff(buff, adr);
                        strcatbuff(buff, fil);
                        escape_in_url(buff);
                        { char* a = buff; while((a = strchr(a, '%'))) { *a = 'X'; a++; } }
                        HT_ADD_HTMLESCAPED(buff);
                        lastsaved=eadr-1;    // dernier écrit+1 (enfin euh apres on fait un ++ alors hein)
                      }
                      else if (opt->urlmode==3) {    // URI absolue /
                        if ((opt->getmode & 1) && (ptr>0)) {    // ecrire les html
                          HT_ADD_HTMLESCAPED(fil);
                        }
                        lastsaved=eadr-1;    // dernier écrit+1 (enfin euh apres on fait un ++ alors hein)
                      }
                      else if (opt->urlmode==5) {    // transparent proxy URL
                        char BIGSTK tempo[HTS_URLMAXSIZE*2];
                        const char *uri;
                        int i;
                        char *pos;

                        if ((opt->getmode & 1) && (ptr>0)) {    // ecrire les html
                          if (!link_has_authority(adr)) {
                            HT_ADD("http://");
                          } else {
                            char* aut = strstr(adr, "//");
                            if (aut) {
                              char tmp[256];
                              tmp[0]='\0';
                              strncatbuff(tmp, adr, (int) (aut - adr));   // scheme
                              HT_ADD(tmp);          // Protocol
                              HT_ADD("//");
                            }
                          }

                          // filename is taken as URI (ex: "C:\My Website\www.example.com\foo4242.html)
                          uri = save;

                          // .. after stripping the path prefix (ex: "www.example.com\foo4242.html)
                          if (strnotempty(StringBuff(opt->path_html_utf8))) {
                            uri += StringLength(opt->path_html_utf8);
                            for( ; uri[0] == '/' || uri[0] == '\\' ; uri++) ;
                          }

                          // and replacing all \ by / (ex: "www.example.com/foo4242.html)
                          strcpybuff(tempo, uri);
                          for(i = 0 ; tempo[i] != '\0' ; i++) {
                            if (tempo[i] == '\\') {
                              tempo[i] = '/';
                            }
                          }

                          // convert to local codepage
                          if (str->page_charset_ != NULL && *str->page_charset_ != '\0') {
                            char *const local_save = hts_convertStringFromUTF8(tempo, strlen(tempo), str->page_charset_);
                            if (local_save != NULL) {
                              strcpybuff(tempo, local_save);
                              free(local_save);
                            } else {
                              if ((opt->debug>1) && (opt->log!=NULL)) {
                                HTS_LOG(opt,LOG_DEBUG); 
                                fprintf(opt->log, "Warning: could not build local charset representation of '%s' in '%s'"LF, tempo, str->page_charset_);
                                test_flush;
                              }
                            }
                          }

                          // put original query string if any (ex: "www.example.com/foo4242.html?q=45)
                          pos = strchr(fil, '?');
                          if (pos != NULL) {
                            strcatbuff(tempo, pos);
                          }

                          // write it
                          HT_ADD_HTMLESCAPED(tempo);
                        }
                        lastsaved=eadr-1;    // dernier écrit+1 (enfin euh apres on fait un ++ alors hein)
                      }
                      else if (opt->urlmode==2) {  // RELATIF
                        char BIGSTK tempo[HTS_URLMAXSIZE*2];
                        tempo[0]='\0';
                        // calculer le lien relatif

                        if (lienrelatif(tempo,save,relativesavename)==0) {
                          if (!in_media) {    // In media (such as real audio): don't patch
														/* Never escape high-chars (we don't know the encoding!!) */
														escape_uri_utf(tempo);

                            //if (!no_esc_utf)
                            //  escape_uri(tempo);     // escape with %xx
                            //else {
                            //  /* No escaping at all - remaining upper chars will be escaped below */
                            //  /* FIXME - Should be done in all local cases */
                            //  //x_escape_html(tempo);
                            //  //escape_uri_utf(tempo);     // FIXME - escape with %xx
                            //  //escape_uri(tempo);     // escape with %xx
                            //}
                          }
                          if ((opt->debug>1) && (opt->log!=NULL)) {
                            HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"relative link at %s build with %s and %s: %s"LF,adr,save,relativesavename,tempo);
                            test_flush;
                          }

                          // lien applet (code) - il faut placer un codebase avant
                          if (p_type==-1) {  // que le nom de fichier

                            if (p_flush) {
                              char BIGSTK tempo_pat[HTS_URLMAXSIZE*2];
                              tempo_pat[0]='\0';
                              {
                                char* a=strrchr(tempo,'/');

                                // Example: we converted code="x.y.z.foo.class" into "x/y/z/foo.class"
                                // we have to do the contrary now
                                if (add_class_dots_to_patch>0) {
                                  while( (add_class_dots_to_patch>0) && (a) ) {
                                    *a='.';     // convert "false" java / into .
                                    add_class_dots_to_patch--;
                                    a=strrchr(tempo,'/');
                                  }
                                  // if add_class_dots_to_patch, this is because there is a problem!!
                                  if (add_class_dots_to_patch) {
                                    if (opt->log) {
                                      HTS_LOG(opt,LOG_WARNING); fprintf(opt->log,"Error: can not rewind java path %s, check html code"LF,tempo);
                                      test_flush;
                                    }
                                  }
                                }

                                if (a) {
                                  char BIGSTK tempo2[HTS_URLMAXSIZE*2];
                                  strcpybuff(tempo2,a+1);
                                  strncatbuff(tempo_pat,tempo,(int) (a - tempo)+1);  // chemin
                                  strcpybuff(tempo,tempo2);                     // fichier
                                }
                              }

                              // érire codebase="chemin"
                              if ((opt->getmode & 1) && (ptr>0)) {
                                char BIGSTK tempo4[HTS_URLMAXSIZE*2];
                                tempo4[0]='\0';

                                if (strnotempty(tempo_pat)) {
                                  HT_ADD("codebase=\"");
                                  HT_ADD_HTMLESCAPED(tempo_pat);
                                  HT_ADD("\" ");
                                }

                                strncatbuff(tempo4,lastsaved,(int) (p_flush - lastsaved));
                                HT_ADD(tempo4);    // refresh code="
                              }
                            }
                            //lastsaved=adr;    // dernier écrit+1
                          }                              

                          if ((opt->getmode & 1) && (ptr>0)) {
                            // convert to local codepage - NOT, already converted into %NN, and passed to the remote server so we do not have anything to do
                            //if (str->page_charset_ != NULL && *str->page_charset_ != '\0') {
                            //  char *const local_save = hts_convertStringFromUTF8(tempo, strlen(tempo), str->page_charset_);
                            //  if (local_save != NULL) {
                            //    strcpybuff(tempo, local_save);
                            //    free(local_save);
                            //  } else {
                            //    if ((opt->debug>1) && (opt->log!=NULL)) {
                            //      HTS_LOG(opt,LOG_DEBUG); 
                            //      fprintf(opt->log, "Warning: could not build local charset representation of '%s' in '%s'"LF, tempo, str->page_charset_);
                            //      test_flush;
                            //    }
                            //  }
                            //}

                            // écrire le lien modifié, relatif
                            // Note: escape all chars, even >127 (no UTF)
                            HT_ADD_HTMLESCAPED_FULL(tempo);

                            // Add query-string, for informational purpose only
                            // Useless, because all parameters-pages are saved into different targets
                            if (opt->includequery) {
                              char* a=strchr(lien,'?');
                              if (a) {
                                HT_ADD_HTMLESCAPED(a);
                              }
                            }
                          }
                          lastsaved=eadr-1;    // dernier écrit+1 (enfin euh apres on fait un ++ alors hein)
                        } else {
                          if (opt->log) {
                            fprintf(opt->log,"Error building relative link %s and %s"LF,save,relativesavename);
                            test_flush;
                          }
                        }
                      }  // sinon le lien sera écrit normalement


#if 0
                      if (fexist(save)) {    // le fichier existe..
                        adr[0]='\0';
                        //if ((opt->debug>0) && (opt->log!=NULL)) {
                        if (opt->log) {
                          HTS_LOG(opt,LOG_WARNING); fprintf(opt->log,"Link has already been written on disk, cancelled: %s"LF,save);
                          test_flush;
                        }
                      }
#endif                            

                      /* Security check */
                      if (strlen(save) >= HTS_URLMAXSIZE) {
                        adr[0]='\0';
                        if (opt->log) {
                          HTS_LOG(opt,LOG_WARNING); fprintf(opt->log,"Link is too long: %s"LF,save);
                          test_flush;
                        }
                      }

                      if ((adr[0]!='\0') && (p_type!=2) && (p_type!=-2) && (forbidden_url!=1) ) {  // si le fichier n'existe pas, ajouter à la liste                            
                        // n'y a-t-il pas trop de liens?
                        if (lien_tot+1 >= lien_max-4) {    // trop de liens!
                          printf("PANIC! : Too many URLs : >%d [%d]\n",lien_tot,__LINE__);
                          if (opt->log) {
                            fprintf(opt->log,LF"Too many URLs, giving up..(>%d)"LF,lien_max);
                            fprintf(opt->log,"To avoid that: use #L option for more links (example: -#L1000000)"LF);
                            test_flush;
                          }
                          if ((opt->getmode & 1) && (ptr>0)) { if (fp) { fclose(fp); fp=NULL; } }
                          XH_uninit;   // désallocation mémoire & buffers
                          return -1;

                        } else {    // noter le lien sur la listes des liens à charger
                          int pass_fix,dejafait=0;

                          // Calculer la priorité de ce lien
                          if ((opt->getmode & 4)==0) {    // traiter html après
                            pass_fix=0;
                          } else {    // vérifier que ce n'est pas un !html
                            if (!ishtml(opt,fil))
                              pass_fix=1;        // priorité inférieure (traiter après)
                            else
                              pass_fix=max(0,numero_passe);    // priorité normale
                          }

                          /* If the file seems to be an html file, get depth-1 */
                          /*
                          if (strnotempty(save)) {
                          if (ishtml(opt,save) == 1) {
                          // descore_prio = 2;
                          } else {
                          // descore_prio = 1;
                          }
                          }
                          */

                          // vérifier que le lien n'a pas déja été noté
                          // si c'est le cas, alors il faut s'assurer que la priorité associée
                          // au fichier est la plus grande des deux priorités
                          //
                          // On part de la fin et on essaye de se presser (économise temps machine)
                          {
                            int i=hash_read(hash,save,"",0,opt->urlhack);      // lecture type 0 (sav)
                            if (i>=0) {
                              if ((opt->debug>1) && (opt->log!=NULL)) {
                                if (
                                  strcmp(adr, liens[i]->adr) != 0 
                                  || strcmp(fil, liens[i]->fil) != 0
                                  ) {
                                    HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"merging similar links %s%s and %s%s"LF,adr,fil,liens[i]->adr,liens[i]->fil);
                                    test_flush;
                                  }
                              }
                              liens[i]->depth=maximum(liens[i]->depth,liens[ptr]->depth - 1);
                              dejafait=1;
                            }
                          }

                          // le lien n'a jamais été créé.
                          // cette fois ci, on le crée!
                          if (!dejafait) {                                
                            //
                            // >>>> CREER LE LIEN <<<<
                            //
                            // enregistrer lien à charger
                            //liens[lien_tot]->adr[0]=liens[lien_tot]->fil[0]=liens[lien_tot]->sav[0]='\0';
                            // même adresse: l'objet père est l'objet père de l'actuel

														// DEBUT ROBOTS.TXT AJOUT
														if (!just_test_it) {
															if ((!strfield(adr,"ftp://"))         // non ftp
																&& (!strfield(adr,"file://")) 
#if HTS_USEMMS
																&& (!strfield(adr,"mms://")) 
#endif
																) 
															{    // non file
																if (opt->robots) {    // récupérer robots
																	if (ishtml(opt,fil)!=0) {                       // pas la peine pour des fichiers isolés
																		if (checkrobots(_ROBOTS,adr,"") != -1) {    // robots.txt ?
																			checkrobots_set(_ROBOTS ,adr,"");          // ajouter entrée vide
																			if (checkrobots(_ROBOTS,adr,"") == -1) {    // robots.txt ?
																				// enregistrer robots.txt (MACRO)
																				liens_record(adr,"/robots.txt","","","");
																				if (liens[lien_tot]==NULL) {  // erreur, pas de place réservée
																					printf("PANIC! : Not enough memory [%d]\n",__LINE__);
																					if (opt->log) { 
																						fprintf(opt->log,"Not enough memory, can not re-allocate %d bytes"LF,(int)((add_tab_alloc+1)*sizeof(lien_url)));
																						test_flush;
																					}
																					if ((opt->getmode & 1) && (ptr>0)) { if (fp) { fclose(fp); fp=NULL; } }
																					XH_uninit;    // désallocation mémoire & buffers
																					return -1;
																				}  
																				liens[lien_tot]->testmode=0;          // pas mode test
																				liens[lien_tot]->link_import=0;       // pas mode import     
																				liens[lien_tot]->premier=lien_tot;
																				liens[lien_tot]->precedent=ptr;
																				liens[lien_tot]->depth=0;
																				liens[lien_tot]->pass2=max(0,numero_passe);
																				liens[lien_tot]->retry=0;
																				lien_tot++;  // UN LIEN DE PLUS
#if DEBUG_ROBOTS
																				printf("robots.txt: added file robots.txt for %s\n",adr);
#endif
																				if ((opt->debug>1) && (opt->log!=NULL)) {
																					HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"robots.txt added at %s"LF,adr);
																					test_flush;
																				}
																			} else {
																				if (opt->log) {   
																					fprintf(opt->log,"Unexpected robots.txt error at %d"LF,__LINE__);
																					test_flush;
																				}
																			}
																		}
																	}
																}
															}
														}
														// FIN ROBOTS.TXT AJOUT

														// enregistrer (MACRO)
														liens_record(adr,fil,save,former_adr,former_fil);
														if (liens[lien_tot]==NULL) {  // erreur, pas de place réservée
															printf("PANIC! : Not enough memory [%d]\n",__LINE__);
															if (opt->log) { 
																fprintf(opt->log,"Not enough memory, can not re-allocate %d bytes"LF,(int)((add_tab_alloc+1)*sizeof(lien_url)));
																test_flush;
															}
                              if ((opt->getmode & 1) && (ptr>0)) { if (fp) { fclose(fp); fp=NULL; } }
                              XH_uninit;    // désallocation mémoire & buffers
                              return -1;
                            }  

                            // mode test?
                            if (!just_test_it)
                              liens[lien_tot]->testmode=0;          // pas mode test
                            else
                              liens[lien_tot]->testmode=1;          // mode test
                            if (!import_done)
                              liens[lien_tot]->link_import=0;       // pas mode import
                            else
                              liens[lien_tot]->link_import=1;       // mode import
                            // écrire autres paramètres de la structure-lien
                            if ((meme_adresse) && (!import_done) && (liens[ptr]->premier != 0))
                              liens[lien_tot]->premier=liens[ptr]->premier;
                            else    // sinon l'objet père est le précédent lui même
                              liens[lien_tot]->premier=lien_tot;
                            // liens[lien_tot]->premier=ptr;

                            liens[lien_tot]->precedent=ptr;
                            // noter la priorité
                            if (!set_prio_to)
                              liens[lien_tot]->depth=liens[ptr]->depth - 1;
                            else
                              liens[lien_tot]->depth=max(0,min(liens[ptr]->depth-1,set_prio_to-1));         // PRIORITE NULLE (catch page)
                            // noter pass
                            liens[lien_tot]->pass2=pass_fix;
                            liens[lien_tot]->retry=opt->retry;

                            //strcpybuff(liens[lien_tot]->adr,adr);
                            //strcpybuff(liens[lien_tot]->fil,fil);
                            //strcpybuff(liens[lien_tot]->sav,save); 
                            if ((opt->debug>1) && (opt->log!=NULL)) {
                              if (!just_test_it) {
                                HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"OK, NOTE: %s%s -> %s"LF,liens[lien_tot]->adr,liens[lien_tot]->fil,liens[lien_tot]->sav);
                              } else {
                                HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"OK, TEST: %s%s"LF,liens[lien_tot]->adr,liens[lien_tot]->fil);
                              }
                              test_flush;
                            }

                            lien_tot++;  // UN LIEN DE PLUS
                          } else { // if !dejafait
                            if ((opt->debug>1) && (opt->log!=NULL)) {
                              HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"link has already been recorded, cancelled: %s"LF,save);
                              test_flush;
                            }

                          }


                        }   // si pas trop de liens
                      }   // si adr[0]!='\0'


                    }  // if adr[0]!='\0' 

                  }  // if adr[0]!='\0'

                }    // if strlen(lien)>0

              }   // if ok==0      

              assertf(eadr - adr >= 0);       // Should not go back
              if (eadr > adr) {
                INCREMENT_CURRENT_ADR(eadr - 1 - adr);
              }
              // adr=eadr-1;  // ** sauter

              /* We skipped bytes and skip the " : reset state */
              /*if (inscript) {
              inscript_state_pos = INSCRIPT_START;
              }*/

          }  // if (p) 

        }  // si '<' ou '>'

        // plus loin
        adr++;      // automate will be checked next loop


        /* Otimization: if we are scanning in HTML data (not in tag or script), 
        then jump to the next starting tag */
        if (ptr>0) {
          if ( (!intag)         /* Not in tag */
            && (!inscript)      /* Not in (java)script */
            && (!in_media)      /* Not in media */
            && (!incomment)     /* Not in comment (<!--) */
            && (!inscript_tag)  /* Not in tag with script inside */
            ) 
          {
            /* Not at the end */
            if (( ((int) (adr - r->adr)) ) < r->size) {
              /* Not on a starting tag yet */
              if (*adr != '<') {
                /* strchr does not well behave with null chrs.. */
                /* char* adr_next = strchr(adr,'<'); */
                char* adr_next = adr;
                while(*adr_next != '<' && (adr_next - r->adr) < r->size ) {
                  adr_next++;
                }
                /* Jump to near end (index hack) */
                if (!adr_next || *adr_next != '<') {
                  if (
                    ( (int)(adr - r->adr) < (r->size - 4)) 
                    &&
                    (r->size > 4)
                    ) {
                      adr = r->adr + r->size - 2;
                    }
                } else {
                  adr = adr_next;
                }
              }
            }
          }
        }

        // ----------
        // écrire peu à peu
        if ((opt->getmode & 1) && (ptr>0)) HT_ADD_ADR;
        lastsaved=adr;    // dernier écrit+1
        // ----------

        // Checks
        if (back_add_stats != opt->state.back_add_stats) {
          back_add_stats = opt->state.back_add_stats;

          // Check max time
          if (!back_checkmirror(opt)) {
            adr = r->adr + r->size;
          }
        }

        // pour les stats du shell si parsing trop long
        if (r->size)
          opt->state._hts_in_html_done=(100 * ((int) (adr - r->adr)) ) / (int)(r->size);
        if (opt->state._hts_in_html_poll) {
          opt->state._hts_in_html_poll=0;
          // temps à attendre, et remplir autant que l'on peut le cache (backing)
          back_wait(sback,opt,cache,HTS_STAT.stat_timestart);        
          back_fillmax(sback,opt,cache,liens,ptr,numero_passe,lien_tot);

          // Transfer rate
          engine_stats();

          // Refresh various stats
          HTS_STAT.stat_nsocket=back_nsoc(sback);
          HTS_STAT.stat_errors=fspc(opt, NULL,"error");
          HTS_STAT.stat_warnings=fspc(opt, NULL,"warning");
          HTS_STAT.stat_infos=fspc(opt, NULL,"info");
          HTS_STAT.nbk=backlinks_done(sback,liens,lien_tot,ptr);
          HTS_STAT.nb=back_transfered(HTS_STAT.stat_bytes,sback);

          if (!RUN_CALLBACK7(opt, loop, sback->lnk, sback->count, 0,ptr,lien_tot,(int) (time_local()-HTS_STAT.stat_timestart),&HTS_STAT)) {
            if (opt->log) {
              HTS_LOG(opt,LOG_INFO); fprintf(opt->log,"Exit requested by shell or user"LF);
              test_flush;
            } 
            *stre->exit_xh_=1;  // exit requested
            XH_uninit;
            return -1;
            //adr = r->adr + r->size;  // exit
          } else if (opt->state._hts_cancel == 1) {
            // adr = r->adr + r->size;  // exit
            nofollow=1;               // moins violent
            opt->state._hts_cancel = 0;
          }

        }

        // refresh the backing system each 2 seconds
        if (engine_stats()) {
          back_wait(sback,opt,cache,HTS_STAT.stat_timestart);        
          back_fillmax(sback,opt,cache,liens,ptr,numero_passe,lien_tot);
        }
      } while(( ((int) (adr - r->adr)) ) < r->size);

			opt->state._hts_in_html_parsing=0;  // flag
      opt->state._hts_cancel=0;           // pas de cancel

			if ((opt->getmode & 1) && (ptr>0)) {
        {
          char* cAddr = ht_buff;
          int cSize = (int) ht_len;
          if ( (opt->debug>0) && (opt->log!=NULL) ) {
            HTS_LOG(opt,LOG_INFO); fprintf(opt->log,"engine: postprocess-html: %s%s"LF, urladr, urlfil);
          }
          if (RUN_CALLBACK4(opt, postprocess, &cAddr, &cSize, urladr, urlfil) == 1) {
            ht_buff = cAddr;
            ht_len = cSize;
          }
        }

        /* Flush and save to disk */
        HT_ADD_END;    // achever
      }
      //
      //
      //
    }  // if !error


    if (opt->getmode & 1) { if (fp) { fclose(fp); fp=NULL; } }
    // sauver fichier
    //structcheck(savename);
    //filesave(opt,r->adr,r->size,savename);

  }  // analyse OK

  /* Apply changes */
  ENGINE_SAVE_CONTEXT();

  return 0;
}




/*
Check 301, 302, .. statuscodes (moved)
*/
int hts_mirror_check_moved(htsmoduleStruct* str, htsmoduleStructExtended* stre) {
  /* Load engine variables */
  ENGINE_LOAD_CONTEXT();  

  // DEBUT rattrapage des 301,302,307..
  // ------------------------------------------------------------
  if (!error) {
    ////////{
    // on a chargé un fichier en plus
    // if (!error) stat_loaded+=r.size;

    // ------------------------------------------------------------
    // Rattrapage des 301,302,307 (moved) et 412,416 - les 304 le sont dans le backing 
    // ------------------------------------------------------------
    if (HTTP_IS_REDIRECT(r->statuscode)) {          
        //if (r->adr!=NULL) {   // adr==null si fichier direct. [catch: davename normalement si cgi]
        //int i=0;
        char *rn=NULL;
        // char* p;

        if ( (opt->debug>0) && (opt->log!=NULL) ) {
          //if (opt->log) {
          HTS_LOG(opt,LOG_WARNING); fprintf(opt->log,"%s for %s%s"LF,r->msg,urladr,urlfil);
          test_flush;
        }


        {
          char BIGSTK mov_url[HTS_URLMAXSIZE*2],mov_adr[HTS_URLMAXSIZE*2],mov_fil[HTS_URLMAXSIZE*2];
          int get_it=0;         // ne pas prendre le fichier à la même adresse par défaut
          int reponse=0;
          mov_url[0]='\0'; mov_adr[0]='\0'; mov_fil[0]='\0';
          //

          strcpybuff(mov_url,r->location);

          // url qque -> adresse+fichier
          if ((reponse=ident_url_relatif(mov_url,urladr,urlfil,mov_adr,mov_fil))>=0) {                        
            int set_prio_to=0;    // pas de priotité fixéd par wizard

            // check whether URLHack is harmless or not
            if (opt->urlhack) {
              char BIGSTK n_adr[HTS_URLMAXSIZE*2], n_fil[HTS_URLMAXSIZE*2];
              char BIGSTK pn_adr[HTS_URLMAXSIZE*2], pn_fil[HTS_URLMAXSIZE*2];
              n_adr[0] = n_fil[0] = '\0';
              (void) adr_normalized(mov_adr, n_adr);
              (void) fil_normalized(mov_fil, n_fil);
              (void) adr_normalized(urladr, pn_adr);
              (void) fil_normalized(urlfil, pn_fil);
              if (strcasecmp(n_adr, pn_adr) == 0 && strcasecmp(n_fil, pn_fil) == 0) {
                if (opt->log) {
                  HTS_LOG(opt,LOG_WARNING); fprintf(opt->log,"Redirected link is identical because of 'URL Hack' option: %s%s and %s%s"LF, urladr, urlfil, mov_adr, mov_fil);
                  test_flush;
                }
              }
            }

            //if (ident_url_absolute(mov_url,mov_adr,mov_fil)!=-1) {    // ok URL reconnue
            // c'est (en gros) la même URL..
            // si c'est un problème de casse dans le host c'est que le serveur est buggé
            // ("RFC says.." : host name IS case insensitive)
            if ((strfield2(mov_adr,urladr)!=0) && (strfield2(mov_fil,urlfil)!=0)) {  // identique à casse près
              // on tourne en rond
              if (strcmp(mov_fil,urlfil)==0) {
                error=1;
                get_it=-1;        // ne rien faire
                if (opt->log) {
                  HTS_LOG(opt,LOG_WARNING); fprintf(opt->log,"Can not bear crazy server (%s) for %s%s"LF,r->msg,urladr,urlfil);
                  test_flush;
                }
              } else {    // mauvaise casse, effacer entrée dans la pile et rejouer une fois
                get_it=1;
              }
            } else {        // adresse différente
              if (ishtml(opt,mov_url)==0) {   // pas même adresse MAIS c'est un fichier non html (pas de page moved possible)
                // -> on prend à cette adresse, le lien sera enregistré avec lien_record() (hash)
                if ((opt->debug>1) && (opt->log!=NULL)) {
                  HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"wizard link test for moved file at %s%s.."LF,mov_adr,mov_fil);
                  test_flush;
                }
                // accepté?
                if (hts_acceptlink(opt,ptr,lien_tot,liens,
                  mov_adr,mov_fil,
                  NULL, NULL,
                  &set_prio_to,
                  NULL) != 1) {                /* nouvelle adresse non refusée ? */
                    get_it=1;
                    if ((opt->debug>1) && (opt->log!=NULL)) {
                      HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"moved link accepted: %s%s"LF,mov_adr,mov_fil);
                      test_flush;
                    }
                  }
              } /* sinon traité normalement */
            }

            //if ((strfield2(mov_adr,urladr)!=0) && (strfield2(mov_fil,urlfil)!=0)) {  // identique à casse près
            if (get_it==1) {
              // court-circuiter le reste du traitement
              // et reculer pour mieux sauter
              if (opt->log) {
                HTS_LOG(opt,LOG_WARNING); fprintf(opt->log,"Warning moved treated for %s%s (real one is %s%s)"LF,urladr,urlfil,mov_adr,mov_fil);
                test_flush;
              }          
              // canceller lien actuel
              error=1;
              strcpybuff(liens[ptr]->adr,"!");  // caractère bidon (invalide hash)
              // noter NOUVEAU lien
              //xxc xxc
              //  set_prio_to=0+1;  // protection if the moved URL is an html page!!
              //xxc xxc
              {
                char BIGSTK mov_sav[HTS_URLMAXSIZE*2];
                // calculer lien et éventuellement modifier addresse/fichier
                if (url_savename2(mov_adr,mov_fil,mov_sav,NULL,NULL,liens[liens[ptr]->precedent]->adr,liens[liens[ptr]->precedent]->fil,opt,liens,lien_tot,sback,cache,hash,ptr,numero_passe,NULL,str->page_charset_)!=-1) { 
                  if (hash_read(hash,mov_sav,"",0,0)<0) {      // n'existe pas déja
                    // enregistrer lien (MACRO) avec SAV IDENTIQUE
                    liens_record(mov_adr,mov_fil,liens[ptr]->sav,"","");
                    //liens_record(mov_adr,mov_fil,mov_sav,"","");
                    if (liens[lien_tot]!=NULL) {    // OK, pas d'erreur
                      // mode test?
                      liens[lien_tot]->testmode=liens[ptr]->testmode;
                      liens[lien_tot]->link_import=0;       // mode normal
                      if (!set_prio_to)
                        liens[lien_tot]->depth=liens[ptr]->depth;
                      else
                        liens[lien_tot]->depth=max(0,min(set_prio_to-1,liens[ptr]->depth));       // PRIORITE NULLE (catch page)
                      liens[lien_tot]->pass2=max(liens[ptr]->pass2,numero_passe);
                      liens[lien_tot]->retry=liens[ptr]->retry;
                      liens[lien_tot]->premier=liens[ptr]->premier;
                      liens[lien_tot]->precedent=liens[ptr]->precedent;
                      lien_tot++;
                    } else {  // oups erreur, plus de mémoire!!
                      printf("PANIC! : Not enough memory [%d]\n",__LINE__);
                      if (opt->log) {
                        fprintf(opt->log,"Not enough memory, can not re-allocate %d bytes"LF,(int)((add_tab_alloc+1)*sizeof(lien_url)));
                        test_flush;
                      }
                      //if (opt->getmode & 1) { if (fp) { fclose(fp); fp=NULL; } }
                      XH_uninit;    // désallocation mémoire & buffers
                      return 0;
                    }
                  } else {
                    if ( (opt->debug>0) && (opt->log!=NULL) ) {
                      HTS_LOG(opt,LOG_WARNING); fprintf(opt->log,"moving %s to an existing file %s"LF,liens[ptr]->fil,urlfil);
                      test_flush;
                    }
                  }

                }
              }

              //printf("-> %s %s %s\n",liens[lien_tot-1]->adr,liens[lien_tot-1]->fil,liens[lien_tot-1]->sav);

              // note métaphysique: il se peut qu'il y ait un index.html et un INDEX.HTML
              // sous DOS ca marche pas très bien... mais comme je suis génial url_savename()
              // est à même de régler ce problème
            }
          } // ident_url_xx

          if (get_it==0) {    // adresse vraiment différente et potentiellement en html (pas de possibilité de bouger la page tel quel à cause des <img src..> et cie)
            rn=(char*) calloct(8192,1);
            if (rn!=NULL) {
              if (opt->log) {
                HTS_LOG(opt,LOG_WARNING); fprintf(opt->log,"File has moved from %s%s to %s"LF,urladr,urlfil,mov_url);
                test_flush;
              }
              if (!opt->mimehtml) {
                escape_uri(mov_url);
              } else {
                char BIGSTK buff[HTS_URLMAXSIZE*3];
                strcpybuff(buff, mov_adr);
                strcatbuff(buff, mov_fil);
                escape_in_url(buff);
                { char* a = buff; while((a = strchr(a, '%'))) { *a = 'X'; a++; } }
                strcpybuff(mov_url, "cid:");
                strcatbuff(mov_url, buff);
              }
              // On prépare une page qui sautera immédiatement sur la bonne URL
              // Le scanner re-changera, ensuite, cette URL, pour la mirrorer!
              strcpybuff(rn,"<HTML>"CRLF);
              strcatbuff(rn,"<!-- Created by HTTrack Website Copier/"HTTRACK_VERSION" "HTTRACK_AFF_AUTHORS" -->"CRLF);
              strcatbuff(rn,"<HEAD>"CRLF"<TITLE>Page has moved</TITLE>"CRLF"</HEAD>"CRLF"<BODY>"CRLF);
              strcatbuff(rn,"<META HTTP-EQUIV=\"Refresh\" CONTENT=\"0; URL=");
              strcatbuff(rn,mov_url);    // URL
              strcatbuff(rn,"\">"CRLF);
              strcatbuff(rn,"<A HREF=\"");
              strcatbuff(rn,mov_url);
              strcatbuff(rn,"\">");
              strcatbuff(rn,"<B>Click here...</B></A>"CRLF);
              strcatbuff(rn,"</BODY>"CRLF);
              strcatbuff(rn,"<!-- Created by HTTrack Website Copier/"HTTRACK_VERSION" "HTTRACK_AFF_AUTHORS" -->"CRLF);
              strcatbuff(rn,"</HTML>"CRLF);

              // changer la page
              if (r->adr) { 
                freet(r->adr); 
                r->adr=NULL; 
              }
              r->adr=rn;
              r->size=strlen(r->adr);
              strcpybuff(r->contenttype, "text/html");
            }
          }  // get_it==0

        }     // bloc
        // erreur HTTP (ex: 404, not found)
      } else if (
        (r->statuscode==HTTP_PRECONDITION_FAILED)
        || (r->statuscode==HTTP_REQUESTED_RANGE_NOT_SATISFIABLE)
        ) {    // Precondition Failed, c'est à dire pour nous redemander TOUT le fichier
          if (fexist_utf8(liens[ptr]->sav)) {
            remove(liens[ptr]->sav);    // Eliminer
            if (!fexist_utf8(liens[ptr]->sav)) {  // Bien éliminé? (sinon on boucle..)
#if HDEBUG
              printf("Partial content NOT up-to-date, reget all file for %s\n",liens[ptr]->sav);
#endif
              if ( (opt->debug>1) && (opt->log!=NULL) ) {
                //if (opt->log) {
                HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"Partial file reget (%s) for %s%s"LF,r->msg,urladr,urlfil);
                test_flush;
              }
              // enregistrer le MEME lien (MACRO)
              liens_record(liens[ptr]->adr,liens[ptr]->fil,liens[ptr]->sav,"","");
              if (liens[lien_tot]!=NULL) {    // OK, pas d'erreur
                liens[lien_tot]->testmode=liens[ptr]->testmode;          // mode test?
                liens[lien_tot]->link_import=0;       // pas mode import
                liens[lien_tot]->depth=liens[ptr]->depth;
                liens[lien_tot]->pass2=max(liens[ptr]->pass2,numero_passe);
                liens[lien_tot]->retry=liens[ptr]->retry;
                liens[lien_tot]->premier=liens[ptr]->premier;
                liens[lien_tot]->precedent=ptr;
                lien_tot++;
                //
                // canceller lien actuel
                error=1;
                strcpybuff(liens[ptr]->adr,"!");  // caractère bidon (invalide hash)
                //
              } else {  // oups erreur, plus de mémoire!!
                printf("PANIC! : Not enough memory [%d]\n",__LINE__);
                if (opt->log) {
                  fprintf(opt->log,"Not enough memory, can not re-allocate %d bytes"LF,(int)((add_tab_alloc+1)*sizeof(lien_url)));
                  test_flush;
                }
                //if (opt->getmode & 1) { if (fp) { fclose(fp); fp=NULL; } }
                XH_uninit;    // désallocation mémoire & buffers
                return 0;
              } 
            } else {
              if (opt->log!=NULL) {
                HTS_LOG(opt,LOG_ERROR); fprintf(opt->log,"Can not remove old file %s"LF,urlfil);
                test_flush;
                error = 1;
              }
            }
          } else {
            if (opt->log!=NULL) {
              HTS_LOG(opt,LOG_WARNING); fprintf(opt->log,"Unexpected 412/416 error (%s) for %s%s, '%s' could not be found on disk"LF,r->msg,urladr,urlfil,liens[ptr]->sav != NULL ? liens[ptr]->sav : "");
              test_flush;
              error = 1;
            }
          }

          // Error ?
          if (error) {
            if (!opt->errpage) {
              if (r->adr) {     // désalloc
                freet(r->adr); 
                r->adr=NULL; 
              }
            }
          }
        } else if (r->statuscode!=HTTP_OK) {
          int can_retry=0;

          // cas où l'on peut reessayer
          switch(r->statuscode) {
            //case -1: can_retry=1; break;
          case STATUSCODE_TIMEOUT:
            if (opt->hostcontrol) {    // timeout et retry épuisés
              if ((opt->hostcontrol & 1) && (liens[ptr]->retry<=0)) {
                if ((opt->debug>1) && (opt->log!=NULL)) {
                  HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"Link banned: %s%s"LF,urladr,urlfil); test_flush;
                }
                host_ban(opt,liens,ptr,lien_tot,sback,jump_identification(urladr));
                if ((opt->debug>1) && (opt->log!=NULL)) {
                  HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"Info: previous log - link banned: %s%s"LF,urladr,urlfil); test_flush;
                }
              } else can_retry=1;
            } else can_retry=1;
            break;
          case STATUSCODE_SLOW:
            if ((opt->hostcontrol) && (liens[ptr]->retry<=0)) {    // too slow
              if (opt->hostcontrol & 2) {
                if ((opt->debug>1) && (opt->log!=NULL)) {
                  HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"Link banned: %s%s"LF,urladr,urlfil); test_flush;
                }
                host_ban(opt,liens,ptr,lien_tot,sback,jump_identification(urladr));
                if ((opt->debug>1) && (opt->log!=NULL)) {
                  HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"Info: previous log - link banned: %s%s"LF,urladr,urlfil); test_flush;
                }
              } else can_retry=1;
            } else can_retry=1;
            break;
          case STATUSCODE_CONNERROR:            // connect closed
            can_retry=1;
            break;
          case STATUSCODE_NON_FATAL:            // other (non fatal) error
            can_retry=1;
            break;
          case STATUSCODE_SSL_HANDSHAKE:            // bad SSL handskake
            can_retry=1;
            break;
          case 408: case 409: case 500: case 502: case 504: 
            can_retry=1;
            break;
          }

          if ( strcmp(liens[ptr]->fil,"/primary") != 0 ) {  // no primary (internal page 0)
            if ((liens[ptr]->retry<=0) || (!can_retry) ) {  // retry épuisés (ou retry impossible)
              if (opt->log) {
                if ((opt->retry>0) && (can_retry)){
                  HTS_LOG(opt,LOG_ERROR); 
                  fprintf(opt->log,"\"%s\" (%d) after %d retries at link %s%s (from %s%s)"LF,r->msg,r->statuscode,opt->retry,urladr,urlfil,liens[liens[ptr]->precedent]->adr,liens[liens[ptr]->precedent]->fil);
                } else {
                  if (r->statuscode==STATUSCODE_TEST_OK) {    // test OK
                    if ((opt->debug>0) && (opt->log!=NULL)) {
                      HTS_LOG(opt,LOG_INFO); 
                      fprintf(opt->log,"Test OK at link %s%s (from %s%s)"LF,urladr,urlfil,liens[liens[ptr]->precedent]->adr,liens[liens[ptr]->precedent]->fil);
                    }
                  } else {
                    if (strcmp(urlfil,"/robots.txt")) {       // ne pas afficher d'infos sur robots.txt par défaut
                      HTS_LOG(opt,LOG_ERROR); 
                      fprintf(opt->log,"\"%s\" (%d) at link %s%s (from %s%s)"LF,r->msg,r->statuscode,urladr,urlfil,liens[liens[ptr]->precedent]->adr,liens[liens[ptr]->precedent]->fil);
                    } else {
                      if (opt->debug>1) {
                        HTS_LOG(opt,LOG_INFO); fprintf(opt->log,"No robots.txt rules at %s"LF,urladr);
                        test_flush;
                      }
                    }
                  }
                }
                test_flush;
              }

              // NO error in trop level
              // due to the "no connection -> previous restored" hack
              // This prevent the engine from wiping all data if the website has been deleted (or moved)
              // since last time (which is quite annoying)
              if (liens[ptr]->precedent != 0) {
                // ici on teste si on doit enregistrer la page tout de même
                if (opt->errpage) {
                  store_errpage=1;
                }
              } else {
                if (strcmp(urlfil,"/robots.txt") != 0) {
                  /*
                  This is an error caused by a link entered by the user
                  That is, link(s) entered by user are invalid (404, 500, connect error, proxy error->.)
                  If all links entered are invalid, the session failed and we will attempt to restore
                  the previous one
                  Example: Try to update a website which has been deleted remotely: this may delete
                  the website locally, which is really not desired (especially if the website disappeared!)
                  With this hack, the engine won't wipe local files (how clever)
                  */
                  HTS_STAT.stat_errors_front++;
                }
              }

            } else {    // retry!!
              if (opt->debug>0 && opt->log != NULL) {  // on fera un alert si le retry échoue               
                HTS_LOG(opt,LOG_WARNING); fprintf(opt->log,"Retry after error %d (%s) at link %s%s (from %s%s)"LF,r->statuscode,r->msg,urladr,urlfil,liens[liens[ptr]->precedent]->adr,liens[liens[ptr]->precedent]->fil);
                test_flush;
              }
              // redemander fichier
              liens_record(urladr,urlfil,savename,"","");
              if (liens[lien_tot]!=NULL) {    // OK, pas d'erreur
                liens[lien_tot]->testmode=liens[ptr]->testmode;          // mode test?
                liens[lien_tot]->link_import=0;       // pas mode import
                liens[lien_tot]->depth=liens[ptr]->depth;
                liens[lien_tot]->pass2=max(liens[ptr]->pass2,numero_passe);
                liens[lien_tot]->retry=liens[ptr]->retry-1;    // moins 1 retry!
                liens[lien_tot]->premier=liens[ptr]->premier;
                liens[lien_tot]->precedent=liens[ptr]->precedent;
                lien_tot++;
              } else {  // oups erreur, plus de mémoire!!
                printf("PANIC! : Not enough memory [%d]\n",__LINE__);
                if (opt->log) {
                  HTS_LOG(opt,LOG_PANIC); 
                  fprintf(opt->log,"Not enough memory, can not re-allocate %d bytes"LF,(int)((add_tab_alloc+1)*sizeof(lien_url)));
                  test_flush;
                }
                //if (opt->getmode & 1) { if (fp) { fclose(fp); fp=NULL; } }
                XH_uninit;    // désallocation mémoire & buffers
                return 0;
              } 
            }
          } else {
            if (opt->log) {
              if (opt->debug>1) {
                HTS_LOG(opt,LOG_INFO); 
                fprintf(opt->log,"Info: no robots.txt at %s%s"LF,urladr,urlfil);
              }
            }
          }
          if (!store_errpage) {
            if (r->adr) {     // désalloc
              freet(r->adr); 
              r->adr=NULL; 
            }
            error=1;  // erreur!
          }
          // otherwise, consider this is not an error
        }
        // FIN rattrapage des 301,302,307..
        // ------------------------------------------------------------

  }  // if !error


  /* Apply changes */
  ENGINE_SAVE_CONTEXT();

  return 0;


}

/*
  Process pause, link adding..
*/
void hts_mirror_process_user_interaction(htsmoduleStruct* str, htsmoduleStructExtended* stre) {
  int b;
  /* Load engine variables */
  ENGINE_LOAD_CONTEXT();

#if BDEBUG==1
  printf("\nBack test..\n");
#endif

  // pause/lock files
  {
    int do_pause=0;

    // user pause lockfile : create hts-paused.lock --> HTTrack will be paused
    if (fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-stop.lock"))) {
      // remove lockfile
      remove(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-stop.lock"));
      if (!fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-stop.lock"))) {
        do_pause=1;
      }
    }

    // after receving N bytes, pause
    if (opt->fragment>0) {
      if ((HTS_STAT.stat_bytes-stat_fragment) > opt->fragment) {
        do_pause=1;
      }
    }

    // pause?
    if (do_pause) {
      if ( (opt->debug>0) && (opt->log!=NULL) ) {
        HTS_LOG(opt,LOG_INFO); fprintf(opt->log,"engine: pause requested.."LF);
      }
      while (back_nsoc(sback)>0) {                  // attendre fin des transferts
        back_wait(sback,opt,cache,HTS_STAT.stat_timestart);
        Sleep(200);
        {
          back_wait(sback,opt,cache,HTS_STAT.stat_timestart);

          // Transfer rate
          engine_stats();

          // Refresh various stats
          HTS_STAT.stat_nsocket=back_nsoc(sback);
          HTS_STAT.stat_errors=fspc(opt,NULL,"error");
          HTS_STAT.stat_warnings=fspc(opt,NULL,"warning");
          HTS_STAT.stat_infos=fspc(opt,NULL,"info");
          HTS_STAT.nbk=backlinks_done(sback,liens,lien_tot,ptr);
          HTS_STAT.nb=back_transfered(HTS_STAT.stat_bytes,sback);

          b=0;
          if (!RUN_CALLBACK7(opt, loop, sback->lnk, sback->count, b,ptr,lien_tot,(int) (time_local()-HTS_STAT.stat_timestart),&HTS_STAT)
            || !back_checkmirror(opt)) {
              if (opt->log) {
                HTS_LOG(opt,LOG_INFO); fprintf(opt->log,"Exit requested by shell or user"LF);
                test_flush;
              }
              *stre->exit_xh_=1;  // exit requested
              XH_uninit;
              return ;
            }
        }
      }
      // On désalloue le buffer d'enregistrement des chemins créée, au cas où pendant la pause
      // l'utilisateur ferait un rm -r après avoir effectué un tar
      // structcheck_init(1);
      {
        FILE* fp = fopen(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-paused.lock"),"wb");
        if (fp) {
          fspc(NULL,fp,"info");  // dater
          fprintf(fp,"Pause"LF"HTTrack is paused after retreiving "LLintP" bytes"LF"Delete this file to continue the mirror->.."LF""LF"",(LLint)HTS_STAT.stat_bytes);
          fclose(fp);
        }
      }
      stat_fragment=HTS_STAT.stat_bytes;
      /* Info for wrappers */
      if ( (opt->debug>0) && (opt->log!=NULL) ) {
        HTS_LOG(opt,LOG_INFO); fprintf(opt->log,"engine: pause: %s"LF,fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-paused.lock"));
      }
      RUN_CALLBACK1(opt, pause, fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-paused.lock"));
    }
    //
  }
  // end of pause/lock files

  // changement dans les préférences
  if (opt->state._hts_addurl) {
    char BIGSTK add_adr[HTS_URLMAXSIZE*2];
    char BIGSTK add_fil[HTS_URLMAXSIZE*2];
    while(*opt->state._hts_addurl) {
      char BIGSTK add_url[HTS_URLMAXSIZE*2];
      add_adr[0]=add_fil[0]=add_url[0]='\0';
      if (!link_has_authority(*opt->state._hts_addurl))
        strcpybuff(add_url,"http://");          // ajouter http://
      strcatbuff(add_url,*opt->state._hts_addurl);
      if (ident_url_absolute(add_url,add_adr,add_fil)>=0) {
        // ----Ajout----
        // noter NOUVEAU lien
        char BIGSTK add_sav[HTS_URLMAXSIZE*2];
        // calculer lien et éventuellement modifier addresse/fichier
        if (url_savename2(add_adr,add_fil,add_sav,NULL,NULL,NULL,NULL,opt,liens,lien_tot,sback,cache,hash,ptr,numero_passe,NULL,str->page_charset_)!=-1) { 
          if (hash_read(hash,add_sav,"",0,0)<0) {      // n'existe pas déja
            // enregistrer lien (MACRO)
            liens_record(add_adr,add_fil,add_sav,"","");
            if (liens[lien_tot]!=NULL) {    // OK, pas d'erreur
              liens[lien_tot]->testmode=0;          // mode test?
              liens[lien_tot]->link_import=0;       // mode normal
              liens[lien_tot]->depth=opt->depth;
              liens[lien_tot]->pass2=max(0,numero_passe);
              liens[lien_tot]->retry=opt->retry;
              liens[lien_tot]->premier=lien_tot;
              liens[lien_tot]->precedent=lien_tot;
              lien_tot++;
              //
              if ((opt->debug>0) && (opt->log!=NULL)) {
                HTS_LOG(opt,LOG_INFO); fprintf(opt->log,"Link added by user: %s%s"LF,add_adr,add_fil); test_flush;
              }
              //
            } else {  // oups erreur, plus de mémoire!!
              printf("PANIC! : Not enough memory [%d]\n",__LINE__);
              if (opt->log) {
                fprintf(opt->log,"Not enough memory, can not re-allocate %d bytes"LF,(int)((add_tab_alloc+1)*sizeof(lien_url)));
                test_flush;
              }
              //if (opt->getmode & 1) { if (fp) { fclose(fp); fp=NULL; } }
              XH_uninit;    // désallocation mémoire & buffers
              return ;
            }
          } else {
            if ( (opt->debug>0) && (opt->log!=NULL) ) {
              HTS_LOG(opt,LOG_WARNING); fprintf(opt->log,"Existing link %s%s not added after user request"LF,add_adr,add_fil);
              test_flush;
            }
          }

        }
      } else {
        if (opt->log) {
          HTS_LOG(opt,LOG_ERROR);
          fprintf(opt->log,"Error during URL decoding for %s"LF,add_url);
          test_flush;
        }
      }
      // ----Fin Ajout----
      opt->state._hts_addurl++;                  // suivante
    }
    opt->state._hts_addurl=NULL;           // libérer _hts_addurl
  }
  // si une pause a été demandée
  if (opt->state._hts_setpause || back_pluggable_sockets_strict(sback, opt) <= 0) {
    // index du lien actuel
    int b=back_index(opt,sback,urladr,urlfil,savename);
    int prev = opt->state._hts_in_html_parsing;
    if (b<0) b=0;    // forcer pour les stats
    while(opt->state._hts_setpause || back_pluggable_sockets_strict(sback, opt) <= 0) {    // on fait la pause..
      opt->state._hts_in_html_parsing = 6;
      back_wait(sback,opt,cache,HTS_STAT.stat_timestart);

      // Transfer rate
      engine_stats();

      // Refresh various stats
      HTS_STAT.stat_nsocket=back_nsoc(sback);
      HTS_STAT.stat_errors=fspc(opt,NULL,"error");
      HTS_STAT.stat_warnings=fspc(opt,NULL,"warning");
      HTS_STAT.stat_infos=fspc(opt,NULL,"info");
      HTS_STAT.nbk=backlinks_done(sback,liens,lien_tot,ptr);
      HTS_STAT.nb=back_transfered(HTS_STAT.stat_bytes,sback);

      if (!RUN_CALLBACK7(opt, loop, sback->lnk, sback->count, b,ptr,lien_tot,(int) (time_local()-HTS_STAT.stat_timestart),&HTS_STAT)) {
        if (opt->log) {
          HTS_LOG(opt,LOG_INFO); fprintf(opt->log,"Exit requested by shell or user"LF);
          test_flush;
        }
        *stre->exit_xh_=1;  // exit requested
        XH_uninit;
        return ;
      }
      Sleep(100);  // pause
    }
    opt->state._hts_in_html_parsing = prev;
  }
  ENGINE_SAVE_CONTEXT();
  return ;
}

/*
Wait for next file and
check 301, 302, .. statuscodes (moved)
*/
int hts_mirror_wait_for_next_file(htsmoduleStruct* str, htsmoduleStructExtended* stre) {
  /* Load engine variables */
  ENGINE_DEFINE_CONTEXT();
  int b;
  int n;

  /* User interaction */
  ENGINE_SAVE_CONTEXT();
  {
    hts_mirror_process_user_interaction(str, stre);
  }
  ENGINE_SET_CONTEXT();

  // si le fichier n'est pas en backing, le mettre..
  if (!back_exist(str->sback,str->opt,urladr,urlfil,savename)) {
#if BDEBUG==1
    printf("crash backing: %s%s\n",liens[ptr]->adr,liens[ptr]->fil);
#endif
    if (back_add(sback,opt,cache,urladr,urlfil,savename,liens[liens[ptr]->precedent]->adr,liens[liens[ptr]->precedent]->fil,liens[ptr]->testmode)==-1) {
      printf("PANIC! : Crash adding error, unexpected error found.. [%d]\n",__LINE__);
#if BDEBUG==1
      printf("error while crash adding\n");
#endif
      if (opt->log) {
        HTS_LOG(opt,LOG_ERROR); fprintf(opt->log,"Unexpected backing error for %s%s"LF,urladr,urlfil);
        test_flush;
      } 

    }
  }

#if BDEBUG==1
  printf("test number of socks\n");
#endif

  // ajouter autant de socket qu'on peut ajouter
  n=opt->maxsoc-back_nsoc(sback);
#if BDEBUG==1
  printf("%d sockets available for backing\n",n);
#endif

  if ((n>0) && (!opt->state._hts_setpause)) {   // si sockets libre et pas en pause, ajouter
    // remplir autant que l'on peut le cache (backing)
    back_fillmax(sback,opt,cache,liens,ptr,numero_passe,lien_tot);
  }

  // index du lien actuel
  {
    // ------------------------------------------------------------
    // attendre que le fichier actuel soit prêt - BOUCLE D'ATTENTE
    do {
      /* User interaction */
      ENGINE_SAVE_CONTEXT();
      {
        hts_mirror_process_user_interaction(str, stre);
      }
      ENGINE_SET_CONTEXT();

      // index du lien actuel
      b=back_index(opt,sback,urladr,urlfil,savename);
#if BDEBUG==1
      printf("back index %d, waiting\n",b);
#endif
      // Continue to the loop if link still present
      if (b<0)
        break;

      // Receive data
      if (back[b].status>0)
        back_wait(sback,opt,cache,HTS_STAT.stat_timestart);

      // Continue to the loop if link still present
      b=back_index(opt,sback,urladr,urlfil,savename);
      if (b<0)
        break;

      // Stop the mirror
      if (!back_checkmirror(opt)) {
        *stre->exit_xh_=1;  // exit requested
        XH_uninit;
        return 0;
      }

      // And fill the backing stack
      if (back[b].status>0)
        back_fillmax(sback,opt,cache,liens,ptr,numero_passe,lien_tot);

      // Continue to the loop if link still present
      b=back_index(opt,sback,urladr,urlfil,savename);
      if (b<0)
        break;

      // autres occupations de HTTrack: statistiques, boucle d'attente, etc.
      if ((opt->makestat) || (opt->maketrack)) {
        TStamp l=time_local();
        if ((int) (l-makestat_time) >= 60) {   
          if (makestat_fp != NULL) {
            fspc(NULL,makestat_fp,"info");
            fprintf(makestat_fp,"Rate= %d (/"LLintP") \11NewLinks= %d (/%d)"LF,(int) ((HTS_STAT.HTS_TOTAL_RECV-*stre->makestat_total_)/(l-makestat_time)), (LLint)HTS_STAT.HTS_TOTAL_RECV,(int) lien_tot-*stre->makestat_lnk_,(int) lien_tot);
            fflush(makestat_fp);
            *stre->makestat_total_=HTS_STAT.HTS_TOTAL_RECV;
            *stre->makestat_lnk_=lien_tot;
          }
          if (stre->maketrack_fp != NULL) {
            int i;
            fspc(NULL,stre->maketrack_fp,"info"); fprintf(stre->maketrack_fp,LF);
            for(i=0;i<back_max;i++) {
              back_info(sback,i,3,stre->maketrack_fp);
            }
            fprintf(stre->maketrack_fp,LF);
            fflush(stre->maketrack_fp);

          }
          makestat_time=l;
        }
      }

      /* cancel links */
			{
        int i;
        char* s;
        while(( s = hts_cancel_file_pop(opt) ) != NULL) {
          if (strnotempty(s)) {    // fichier à canceller
            for(i = 0 ; i < back_max ; i++) {
              if ((back[i].status > 0)) {
                if (strcmp(back[i].url_sav,s) == 0) {  // ok trouvé
                  if (back[i].status != 1000) {
#if HTS_DEBUG_CLOSESOCK
                    DEBUG_W("user cancel: deletehttp\n");
#endif
                    if (back[i].r.soc!=INVALID_SOCKET)
                      deletehttp(&back[i].r);
                    back[i].r.soc=INVALID_SOCKET;
                    back[i].r.statuscode=STATUSCODE_INVALID;
                    strcpybuff(back[i].r.msg,"Cancelled by User");
                    back[i].status=0;  // terminé
                    back_set_finished(sback, i);
                  } else    // cancel ftp.. flag à 1
                    back[i].stop_ftp = 1;
                }
              }
            }
            s[0]='\0';
          }
          freet(s);
        }

        // Transfer rate
        engine_stats();

        // Refresh various stats
        HTS_STAT.stat_nsocket=back_nsoc(sback);
        HTS_STAT.stat_errors=fspc(opt,NULL,"error");
        HTS_STAT.stat_warnings=fspc(opt,NULL,"warning");
        HTS_STAT.stat_infos=fspc(opt,NULL,"info");
        HTS_STAT.nbk=backlinks_done(sback,liens,lien_tot,ptr);
        HTS_STAT.nb=back_transfered(HTS_STAT.stat_bytes,sback);

        if (!RUN_CALLBACK7(opt, loop, sback->lnk, sback->count, b,ptr,lien_tot,(int) (time_local()-HTS_STAT.stat_timestart),&HTS_STAT)) {
          if (opt->log) {
            HTS_LOG(opt,LOG_INFO); fprintf(opt->log,"Exit requested by shell or user"LF);
            test_flush;
          } 
          *stre->exit_xh_=1;  // exit requested
          XH_uninit;
          return 0;
        }

      }

#if HTS_POLL
      if ((opt->shell) || (opt->keyboard) || (opt->verbosedisplay) || (!opt->quiet)) {
        TStamp tl;
        *stre->info_shell_=1;

        /* Toggle with ENTER */
        if (!opt->quiet) {
          if (check_stdin()) {
            char com[256];
            linput(stdin,com,200);
            if (opt->verbosedisplay==2)
              opt->verbosedisplay=1;
            else
              opt->verbosedisplay=2;
            /* Info for wrappers */
            if ( (opt->debug>0) && (opt->log!=NULL) ) {
              HTS_LOG(opt,LOG_INFO); fprintf(opt->log,"engine: change-options"LF);
            }
            RUN_CALLBACK0(opt, chopt);
          }
        }

        tl=time_local();

        // générer un message d'infos sur l'état actuel
        if (opt->shell) {    // si shell
          if ((tl-*stre->last_info_shell_)>0) {    // toute les 1 sec
            FILE* fp=stdout;
            int a=0;
            *stre->last_info_shell_=tl;
            if (fexist(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-autopsy"))) {  // débuggage: teste si le robot est vivant
              // (oui je sais un robot vivant.. mais bon.. il a le droit de vivre lui aussi)
              // (libérons les robots esclaves de l'internet!)
              remove(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-autopsy"));
              fp=fopen(fconcat(OPT_GET_BUFF(opt), StringBuff(opt->path_log),"hts-isalive"),"wb");
              a=1;
            }
            if ((*stre->info_shell_) || a) {
              int i,j;

              fprintf(fp,"TIME %d"LF,(int) (tl-HTS_STAT.stat_timestart));
              fprintf(fp,"TOTAL %d"LF,(int) HTS_STAT.stat_bytes);
              fprintf(fp,"RATE %d"LF,(int) (HTS_STAT.HTS_TOTAL_RECV/(tl-HTS_STAT.stat_timestart)));
              fprintf(fp,"SOCKET %d"LF,back_nsoc(sback));
              fprintf(fp,"LINK %d"LF,lien_tot);
              {
                LLint mem=0;
                for(i=0;i<back_max;i++)
                  if (back[i].r.adr!=NULL)
                    mem+=back[i].r.size;
                fprintf(fp,"INMEM "LLintP""LF,(LLint)mem);
              }
              for(j=0;j<2;j++) {  // passes pour ready et wait
                for(i=0;i<back_max;i++) {
                  back_info(sback,i,j+1,stdout);    // maketrack_fp a la place de stdout ?? // **
                }
              }
              fprintf(fp,LF);
              if (a)
                fclose(fp);
              io_flush;
            }
          }
        }  // si shell

      }  // si shell ou keyboard (option)
      //
#endif
    } while((b>=0) && (back[max(b,0)].status>0));


    // If link not found on the stack, it's because it has already been downloaded
    // in background
    // Then, skip it and go to the next one
    if (b<0) {
      if ((opt->debug>1) && (opt->log!=NULL)) {
        HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"link #%d is ready, no more on the stack, skipping: %s%s.."LF,ptr,urladr,urlfil);
        test_flush;
      }

      // prochain lien
      // ptr++;

      return 2; // goto jump_if_done;

    }
#if 0
    /* FIXME - finalized HAS NO MORE THIS MEANING */
    /* link put in cache by the backing system for memory spare - reclaim */
    else if (back[b].finalized) {
      assertf(back[b].r.adr == NULL);
      /* read file in cache */
      back[b].r = cache_read_ro(opt,cache,back[b].url_adr,back[b].url_fil,back[b].url_sav, back[b].location_buffer);
      /* ensure correct location buffer set */
      back[b].r.location=back[b].location_buffer;
      if (back[b].r.statuscode == STATUSCODE_INVALID) {
        if (opt->log) {
          HTS_LOG(opt,LOG_ERROR); fprintf(opt->log,"Unexpected error: %s%s not found anymore in cache"LF,back[b].url_adr,back[b].url_fil);
          test_flush;
        }
      } else {
        if ( (opt->debug>1) && (opt->log!=NULL) ) {
          HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"reclaim file %s%s (%d)"LF,back[b].url_adr,back[b].url_fil,back[b].r.statuscode); test_flush;
        }
      }
    }
#endif

    if (!opt->verbosedisplay) {
      if (!opt->quiet) {
        static int roll=0;  /* static: ok */
        roll=(roll+1)%4;
        printf("%c\x0d",("/-\\|")[roll]);
        fflush(stdout);
      }
    } else if (opt->verbosedisplay==1) {
      if (b >= 0) {
        if (back[b].r.statuscode==HTTP_OK)
          printf("%d/%d: %s%s ("LLintP" bytes) - OK\33[K\r",ptr,lien_tot,back[b].url_adr,back[b].url_fil,(LLint)back[b].r.size);
        else
          printf("%d/%d: %s%s ("LLintP" bytes) - %d\33[K\r",ptr,lien_tot,back[b].url_adr,back[b].url_fil,(LLint)back[b].r.size,back[b].r.statuscode);
      } else {
        HTS_LOG(opt,LOG_ERROR); fprintf(opt->log,"Link disappeared");
      }
      fflush(stdout);
    }
    //}

		// ------------------------------------------------------------
    // Vérificateur d'intégrité
#if DEBUG_CHECKINT
    _CHECKINT(&back[b],"Retour de back_wait, après le while")
    {
      int i;
      for(i=0;i<back_max;i++) {
        char si[256];
        sprintf(si,"Test global après back_wait, index %d",i);
        _CHECKINT(&back[i],si)
      }
    }
#endif

    // copier structure réponse htsblk
    if (b >= 0) {
      memcpy(r, &(back[b].r), sizeof(htsblk));
      r->location=stre->loc_;    // ne PAS copier location!! adresse, pas de buffer
      if (back[b].r.location) 
        strcpybuff(r->location,back[b].r.location);
      back[b].r.adr=NULL;    // ne pas faire de desalloc ensuite

      // libérer emplacement backing
      back_maydelete(opt,cache,sback,b);
    }

    // débug graphique
#if BDEBUG==2
    {
      char s[12];
      int i=0;
      _GOTOXY(1,1);
      printf("Rate=%d B/sec\n",(int) (HTS_STAT.HTS_TOTAL_RECV/(time_local()-HTS_STAT.stat_timestart)));
      while(i<minimum(back_max,160)) {
        if (back[i].status>0) {
          sprintf(s,"%d",back[i].r.size);
        } else if (back[i].status==STATUS_READY) {
          strcpybuff(s,"ENDED");
        } else 
          strcpybuff(s,"   -   ");
        while(strlen(s)<8) strcatbuff(s," ");
        printf("%s",s); io_flush;
        i++;
      }
    }
#endif


#if BDEBUG==1
    printf("statuscode=%d with %s / msg=%s\n",r->statuscode,r->contenttype,r->msg);
#endif

  }

  ENGINE_SAVE_CONTEXT();
  return 0;
}

/* Wait for delayed types */
int hts_wait_delayed(htsmoduleStruct* str, 
                     char* adr, char* fil, char* save, 
                     char* parent_adr, char* parent_fil,
                     char* former_adr, char* former_fil, 
                     int* forbidden_url) {
  ENGINE_LOAD_CONTEXT_BASE();
  hash_struct* const hash = hashptr;

  int r_sv=0;
  int in_error = 0;
  LLint in_error_size = 0;
  char in_error_msg[32];

  // resolve unresolved type
  if (opt->savename_delayed != 0
    && *forbidden_url == 0 
    && IS_DELAYED_EXT(save) 
    && !opt->state.stop
    )
  {
    int loops;
    int continue_loop;
    if ((opt->debug>1) && (opt->log!=NULL)) {
      HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"Waiting for type to be known: %s%s"LF, adr, fil);
      test_flush;
    }

    /* Follow while type is unknown and redirects occurs */
    for( loops = 0, continue_loop = 1 ; IS_DELAYED_EXT(save) && continue_loop && loops < 7 ; loops++  ) {
      continue_loop = 0;

      /*
      Wait for an available slot 
      */
      WAIT_FOR_AVAILABLE_SOCKET();

      /* We can lookup directly in the cache to speedup this mess */
      if (opt->delayed_cached) {
				lien_back back;
				memset(&back, 0, sizeof(back));
				back.r = cache_read(opt, cache, adr, fil, NULL, NULL);              // test uniquement
        if (back.r.statuscode == HTTP_OK && strnotempty(back.r.contenttype)) {      // cache found, and aswer is 'OK'
          if ((opt->debug>1) && (opt->log!=NULL)) {
            HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"Direct type lookup in cache (-%%D1): %s"LF, back.r.contenttype);
            test_flush;
          }

          /* Recompute filename with MIME type */
          save[0] = '\0';
          r_sv=url_savename2(adr,fil,save,former_adr,former_fil,liens[ptr]->adr,liens[ptr]->fil,opt,liens,lien_tot,sback,cache,hash,ptr,numero_passe,&back,str->page_charset_);

          /* Recompute authorization with MIME type */
          {
            int new_forbidden_url = hts_acceptmime(opt, ptr, lien_tot, liens, adr,fil, back.r.contenttype);
            if (new_forbidden_url != -1) {
              if ((opt->debug>1) && (opt->log!=NULL)) {
                HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"result for wizard mime test: %d"LF,new_forbidden_url);
                test_flush;
              }
              if (new_forbidden_url == 1) {
                *forbidden_url = new_forbidden_url;
                if ((opt->debug>1) && (opt->log!=NULL)) {
                  HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"link forbidden because of MIME types restrictions: %s%s"LF, adr, fil);
                  test_flush;
                }
                break;        // exit loop
              }
            }
          }

          /* And exit loop */
          break;
        }
      }

      /* Check if the file was recorded already (necessary for redirects) */
      if (hash_read(hash,save,"",0,opt->urlhack) >= 0) {
        if (loops == 0) {   /* Should not happend */
          if ( opt->log!=NULL ) {
            HTS_LOG(opt,LOG_ERROR); fprintf(opt->log, "Duplicate entry in hts_wait_delayed() cancelled: %s%s -> %s"LF,adr,fil,save);
            test_flush;
          }
        }
        /* Exit loop (we're done) */
        continue_loop = 0;
        break ;
      }

      /* Add in backing (back_index() will respond correctly) */
      if (back_add_if_not_exists(sback,opt,cache,adr,fil,save,parent_adr,parent_fil,0) != -1) {
        int b;
        b=back_index(opt,sback,adr,fil,save); 
        if (b<0) {
          printf("PANIC! : Crash adding error, unexpected error found.. [%d]\n",__LINE__);
          XH_uninit;    // désallocation mémoire & buffers
          return -1;
        }

        /* We added the link before the parser recorded it -- the background download MUST NOT clean silently this entry! (Petr Gajdusek) */
        back[b].early_add = 1;

        /* Cache read failed because file does not exists (bad delayed name!)
        Just re-add with the correct name, as we know the MIME now!
        */
        if (back[b].r.statuscode == STATUSCODE_INVALID && back[b].r.adr == NULL) {
					lien_back delayed_back;
          //char BIGSTK delayed_ctype[128];
          // delayed_ctype[0] = '\0';
          // strncatbuff(delayed_ctype, back[b].r.contenttype, sizeof(delayed_ctype) - 1);    // copier content-type
					back_copy_static(&back[b], &delayed_back);

          /* Delete entry */
          back_delete(opt,cache,sback,b);       // cancel
          b = -1;

          /* Recompute filename with MIME type */
          save[0] = '\0';
          r_sv=url_savename2(adr,fil,save,former_adr,former_fil,liens[ptr]->adr,liens[ptr]->fil,opt,liens,lien_tot,sback,cache,hash,ptr,numero_passe,&delayed_back,str->page_charset_);

          /* Recompute authorization with MIME type */
          {
            int new_forbidden_url = hts_acceptmime(opt, ptr, lien_tot, liens, adr,fil, delayed_back.r.contenttype);
            if (new_forbidden_url != -1) {
              if ((opt->debug>1) && (opt->log!=NULL)) {
                HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"result for wizard mime test: %d"LF,*forbidden_url);
                test_flush;
              }
              if (new_forbidden_url == 1) {
                *forbidden_url = new_forbidden_url;
                if ((opt->debug>1) && (opt->log!=NULL)) {
                  HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"link forbidden because of MIME types restrictions: %s%s"LF, adr, fil);
                  test_flush;
                }
                break;        // exit loop
              }
            }
          }

          /* Re-Add wiht correct type */
          if (back_add_if_not_exists(sback,opt,cache,adr,fil,save,parent_adr,parent_fil,0) != -1) {
            b=back_index(opt,sback,adr,fil,save); 
          }
          if (b<0) {
            printf("PANIC! : Crash adding error, unexpected error found.. [%d]\n",__LINE__);
            XH_uninit;    // désallocation mémoire & buffers
            return -1;
          }

          /* We added the link before the parser recorded it -- the background download MUST NOT clean silently this entry! (Petr Gajdusek) */
          back[b].early_add = 1;

          if ((opt->debug>1) && (opt->log!=NULL)) {
            HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"Type immediately loaded from cache: %s"LF, delayed_back.r.contenttype);
            test_flush;
          }
        }

        /* Wait for headers to be received */
        if (b >= 0) {
          back_set_locked(sback, b);    // Locked entry
        }
        do {
          if (b < 0)
            break;

          // temps à attendre, et remplir autant que l'on peut le cache (backing)
          if (back[b].status>0) {
            back_wait(sback,opt,cache,0);
          }
          if (ptr>=0) {
            back_fillmax(sback,opt,cache,liens,ptr,numero_passe,lien_tot);
          }

          // on est obligé d'appeler le shell pour le refresh..
          {

            // Transfer rate
            engine_stats();

            // Refresh various stats
            HTS_STAT.stat_nsocket=back_nsoc(sback);
            HTS_STAT.stat_errors=fspc(opt,NULL,"error");
            HTS_STAT.stat_warnings=fspc(opt,NULL,"warning");
            HTS_STAT.stat_infos=fspc(opt,NULL,"info");
            HTS_STAT.nbk=backlinks_done(sback,liens,lien_tot,ptr);
            HTS_STAT.nb=back_transfered(HTS_STAT.stat_bytes,sback);

            if (!RUN_CALLBACK7(opt, loop, sback->lnk, sback->count, b,ptr,lien_tot,(int) (time_local()-HTS_STAT.stat_timestart),&HTS_STAT)) {
              return -1;
            } else if (opt->state._hts_cancel || !back_checkmirror(opt)) {    // cancel 2 ou 1 (cancel parsing)
              back_delete(opt,cache,sback,b);       // cancel test
              break;
            }
          }
        } while(
          /* dns/connect/request */ 
					( back[b].status >= 99 && back[b].status <= 101 )
          ||
          /* For redirects, wait for request to be terminated */
          ( HTTP_IS_REDIRECT(back[b].r.statuscode) && back[b].status > 0 )
          ||
          /* Same for errors */
          ( HTTP_IS_ERROR(back[b].r.statuscode) && back[b].status > 0 )
          );
        if (b >= 0) {
          back_set_unlocked(sback, b);    // Unlocked entry
        }
        /* ready (chunked) or ready (regular download) or ready (completed) */

        // Note: filename NOT in hashtable yet - liens_record will do it, with the correct ext!
        if (b >= 0) {
					lien_back delayed_back;
          //char BIGSTK delayed_ctype[128];
          //delayed_ctype[0] = '\0';
          //strncatbuff(delayed_ctype, back[b].r.contenttype, sizeof(delayed_ctype) - 1);    // copier content-type
					back_copy_static(&back[b], &delayed_back);

          /* Error */
          if (HTTP_IS_ERROR(back[b].r.statuscode))
          {
            /* seen as in error */
            in_error = back[b].r.statuscode;
            in_error_msg[0] = 0;
            strncat(in_error_msg, back[b].r.msg, sizeof(in_error_msg) - 1);
            in_error_size = back[b].r.totalsize;
            /* don't break, even with "don't take error pages" switch, because we need to process the slot anyway (and cache the error) */
          }
          /* Moved! */
          else if (HTTP_IS_REDIRECT(back[b].r.statuscode))
          {
            char BIGSTK mov_url[HTS_URLMAXSIZE*2];
            mov_url[0] = '\0';
            strcpybuff(mov_url, back[b].r.location);    // copier URL

            /* Remove (temporarily created) file if it was created */
            UNLINK(fconv(OPT_GET_BUFF(opt),back[b].url_sav));

            /* Remove slot! */
            if (back[b].status == STATUS_READY) {
              back_maydelete(opt, cache, sback, b);
            } else {    /* should not happend */
              back_delete(opt, cache, sback, b);
            }
            b = -1;

            /* Handle redirect */
            if ((int) strnotempty(mov_url)) {    // location existe!
              char BIGSTK mov_adr[HTS_URLMAXSIZE*2],mov_fil[HTS_URLMAXSIZE*2];
              mov_adr[0]=mov_fil[0]='\0';
              //
              if (ident_url_relatif(mov_url,adr,fil,mov_adr,mov_fil)>=0) {                        
                if ((opt->debug>1) && (opt->log!=NULL)) {
                  HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"Redirect while resolving type: %s%s -> %s%s"LF, adr, fil, mov_adr, mov_fil);
                  test_flush;
                }
                // si non bouclage sur soi même, ou si test avec GET non testé
                if (strcmp(mov_adr,adr) != 0 || strcmp(mov_fil,fil) != 0) {

                  // recopier former_adr/fil?
                  if ((former_adr) && (former_fil)) {
                    if (strnotempty(former_adr)==0) {    // Pas déja noté
                      strcpybuff(former_adr,adr);
                      strcpybuff(former_fil,fil);
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
                      /* Note: the cache 'cached_tests' system will remember this error, and we'll only issue ONE request */
                      *forbidden_url = 1;          /* Forbidden! */
                      if ((opt->debug>1) && (opt->log!=NULL)) {
                        HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"link forbidden because of redirect beyond the mirror scope at %s%s -> %s%s"LF,adr,fil,mov_adr,mov_fil);
                        test_flush;
                      }
                      strcpybuff(adr,mov_adr);
                      strcpybuff(fil,mov_fil);
                      mov_url[0]='\0';
                      break;
                    }
                  }

                  // ftp: stop!
                  if (strfield(mov_url,"ftp://")
#if HTS_USEMMS
										|| strfield(mov_url,"mms://")
#endif
										) {
                    strcpybuff(adr,mov_adr);
                    strcpybuff(fil,mov_fil);
                    break;
                  }

                  /* ok, continue */
                  strcpybuff(adr,mov_adr);
                  strcpybuff(fil,mov_fil);
                  continue_loop = 1;

                  /* Recompute filename for hash lookup */
                  save[0] = '\0';
                  r_sv=url_savename2(adr,fil,save,former_adr,former_fil,liens[ptr]->adr,liens[ptr]->fil,opt,liens,lien_tot,sback,cache,hash,ptr,numero_passe,&delayed_back,str->page_charset_);
                } else {
                  if ( opt->log!=NULL ) {
                    HTS_LOG(opt,LOG_WARNING); fprintf(opt->log,"Unable to test %s%s (loop to same filename)"LF,adr,fil);
                    test_flush;
                  }
                }  // loop to same location
              }  // ident_url_relatif()
            }  // location
          }  // redirect
          if ((opt->debug>1) && (opt->log!=NULL)) {
						HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"Final type for %s%s: '%s'"LF, adr, fil, delayed_back.r.contenttype);
            test_flush;
          }

					/* If we are done, do additional checks with final type and authorizations */
					if (!continue_loop) {
						/* Recompute filename with MIME type */
						save[0] = '\0';
						r_sv=url_savename(adr,fil,save,former_adr,former_fil,liens[ptr]->adr,liens[ptr]->fil,opt,liens,lien_tot,sback,cache,hash,ptr,numero_passe,&delayed_back);

						/* Recompute authorization with MIME type */
						{
							int new_forbidden_url = hts_acceptmime(opt, ptr, lien_tot, liens, adr,fil, delayed_back.r.contenttype);
							if (new_forbidden_url != -1) {
								if ((opt->debug>1) && (opt->log!=NULL)) {
									HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"result for wizard mime test: %d"LF,*forbidden_url);
									test_flush;
								}
								if (new_forbidden_url == 1) {
									*forbidden_url = new_forbidden_url;
									if ((opt->debug>1) && (opt->log!=NULL)) {
										HTS_LOG(opt,LOG_DEBUG); fprintf(opt->log,"link forbidden because of MIME types restrictions: %s%s"LF, adr, fil);
										test_flush;
									}
									break;        // exit loop
								}
							}
						}
					}

          /* Still have a back reference */
          if (b >= 0) {
            /* Finalize now as we have the type */
						if (back[b].status == STATUS_READY) {
							if (!back[b].finalized) {
								back_finalize(opt,cache,sback,b);
							}
						}
            /* Patch destination filename for direct-to-disk mode */
            strcpybuff(back[b].url_sav, save);
          }

        }  // b >= 0
      } else {
        printf("PANIC! : Crash adding error, unexpected error found.. [%d]\n",__LINE__);
        XH_uninit;    // désallocation mémoire & buffers
        return -1;
      }

    } // while(IS_DELAYED_EXT(save))

    if (in_error != 0) {
      /* 'no error page' selected or file discarded by size rules! */
      if (!opt->errpage || ( in_error == STATUSCODE_TOO_BIG ) ) {
        /* Note: the cache 'cached_tests' system will remember this error, and we'll only issue ONE request */
#if 0
        /* No (3.43) - don't do that. We must not post-exclude an authorized link, because this will prevent the cache
        system from processing it, leading to refetch it endlessly. Just accept it, and handle the error as
        usual during parsing.
        */
        *forbidden_url = 1;          /* Forbidden! */
#endif
        if (opt->log != NULL && opt->debug > 0) {
          if (in_error == STATUSCODE_TOO_BIG) {
            HTS_LOG(opt, LOG_INFO); fprintf(opt->log,"link not taken because of its size (%d bytes) at %s%s"LF,(int)in_error_size,adr,fil);
          } else {
            HTS_LOG(opt, LOG_INFO); fprintf(opt->log,"link not taken because of error (%d '%s') at %s%s"LF,in_error,in_error_msg,adr,fil);
          }
          test_flush;
        }
      }
    }

    // error
    if (*forbidden_url != 1
      && IS_DELAYED_EXT(save)) {
      *forbidden_url = 1;
      if (opt->log!=NULL) {
        if (in_error) {
          HTS_LOG(opt,LOG_WARNING); fprintf(opt->log,"link in error (%d '%s'), type unknown, aborting: %s%s"LF, in_error, in_error_msg, adr, fil);
        } else {
          HTS_LOG(opt,LOG_WARNING); fprintf(opt->log,"link is probably looping, type unknown, aborting: %s%s"LF, adr, fil);
        }
        test_flush;
      }
    }

  }  // delayed type check ?

  ENGINE_SAVE_CONTEXT_BASE();

  return 0;
}

