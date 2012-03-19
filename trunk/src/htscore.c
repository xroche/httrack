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
/* File: Main source                                            */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <ctype.h>

/* File defs */
#include "htscore.h"

/* specific definitions */
#include "htsbase.h"
#include "htsnet.h"
#include "htsbauth.h"
#include "htsmd5.h"
#include "htsindex.h"

// htswrap_add
#include "htswrap.h"
/* END specific definitions */


/* HTML parsing */
#if HTS_ANALYSTE

t_hts_htmlcheck_init    hts_htmlcheck_init;
t_hts_htmlcheck_uninit  hts_htmlcheck_uninit;
t_hts_htmlcheck_start   hts_htmlcheck_start;
t_hts_htmlcheck_end     hts_htmlcheck_end;
t_hts_htmlcheck_chopt   hts_htmlcheck_chopt;
t_hts_htmlcheck         hts_htmlcheck;
t_hts_htmlcheck_query   hts_htmlcheck_query;
t_hts_htmlcheck_query2  hts_htmlcheck_query2;
t_hts_htmlcheck_query3  hts_htmlcheck_query3;
t_hts_htmlcheck_loop    hts_htmlcheck_loop;
t_hts_htmlcheck_check   hts_htmlcheck_check;
t_hts_htmlcheck_pause   hts_htmlcheck_pause;
t_hts_htmlcheck_filesave       hts_htmlcheck_filesave;
t_hts_htmlcheck_linkdetected   hts_htmlcheck_linkdetected;
t_hts_htmlcheck_xfrstatus hts_htmlcheck_xfrstatus;
t_hts_htmlcheck_savename  hts_htmlcheck_savename;

char _hts_errmsg[1100]="";
int _hts_in_html_parsing=0;
int _hts_in_html_done=0;  // % done
int _hts_in_html_poll=0;  // parsing
int _hts_setpause=0;
//httrackp* _hts_setopt=NULL;
char** _hts_addurl=NULL;

//
int _hts_cancel=0;
#endif



int exit_xh;          /* quick exit (fatal error or interrupt) */

/* debug */
#if DEBUG_SHOWTYPES
char REG[32768]="\n";
#endif
#if NSDEBUG
int nsocDEBUG=0;
#endif

//
#define _CLRSCR printf("\33[m\33[2J");
#define _GOTOXY(X,Y) printf("\33[" X ";" Y "f");

#if DEBUG_CHECKINT
 #define _CHECKINT_FAIL(a) printf("\n%s\n",a); fflush(stdout); exit(1);
 #define _CHECKINT(obj_ptr,message) \
   if (obj_ptr) {\
     if (( * ((char*) (obj_ptr)) != 0) || ( * ((char*) (((char*) (obj_ptr)) + sizeof(*(obj_ptr))-1)) != 0)) {\
       char msg[1100];\
       if (( * ((char*) (obj_ptr)) != 0) && ( * ((char*) (((char*) (obj_ptr)) + sizeof(*(obj_ptr))-1)) != 0))\
         sprintf(msg,"* PANIC: Integrity error (structure crushed)  in: %s",message);\
       else if ( * ((char*) (obj_ptr)) != 0)\
         sprintf(msg,"* PANIC: Integrity error (start of structure) in: %s",message);\
       else\
         sprintf(msg,"* PANIC: Integrity error (end of structure)   in: %s",message);\
       _CHECKINT_FAIL(msg);\
     }\
   } else {\
     char msg[1100];\
     sprintf(msg,"* PANIC: NULL pointer in: %s",message);\
     _CHECKINT_FAIL(msg);\
   }
#endif

#if DEBUG_HASH
  // longest hash chain?
  int longest_hash[3]={0,0,0},hashnumber=0;
#endif

// demande d'interaction avec le shell
#if HTS_ANALYSTE
char HTbuff[2048];
#endif



// Début de httpmirror, routines annexes

// version 1 pour httpmirror
// flusher si on doit lire peu à peu le fichier
#define test_flush if (opt.flush) { fflush(opt.log); fflush(opt.errlog); }

// pour alléger la syntaxe, des raccourcis sont créés
#define urladr   (liens[ptr]->adr)
#define urlfil   (liens[ptr]->fil)
#define savename (liens[ptr]->sav)
//#define level    (liens[ptr]->depth)

// au cas où nous devons quitter rapidement xhttpmirror (plus de mémoire, etc)
// note: partir de liens_max.. vers 0.. sinon erreur de violation de mémoire: les liens suivants
// ne sont plus à nous.. agh! [dur celui-là]
#if HTS_ANALYSTE
#define HTMLCHECK_UNINIT { \
if ( (opt.debug>0) && (opt.log!=NULL) ) { \
fspc(opt.log,"info"); fprintf(opt.log,"engine: end"LF); \
} \
hts_htmlcheck_end(); \
}
#else
 #define HTMLCHECK_UNINIT 
#endif

#define XH_extuninit { \
  int i; \
  HTMLCHECK_UNINIT \
  if (liens!=NULL) { \
  for(i=lien_max-1;i>=0;i--) { \
  if (liens[i]) { \
  if (liens[i]->firstblock==1) { \
  freet(liens[i]); \
  liens[i]=NULL; \
  } \
  } \
  } \
  freet(liens); \
  liens=NULL; \
  } \
  if (filters && filters[0]) { \
  freet(filters[0]); filters[0]=NULL; \
  } \
  if (filters) { \
  freet(filters); filters=NULL; \
  } \
  if (back) { \
  int i; \
  for(i=0;i<back_max;i++) { \
  back_delete(back,i); \
  } \
  freet(back); back=NULL;  \
  } \
  checkrobots_free(&robots);\
  if (cache.use) { freet(cache.use); cache.use=NULL; } \
  if (cache.dat) { fclose(cache.dat); cache.dat=NULL; }  \
  if (cache.ndx) { fclose(cache.ndx); cache.ndx=NULL; } \
  if (cache.olddat) { fclose(cache.olddat); cache.olddat=NULL; } \
  if (cache.lst) { fclose(cache.lst); cache.lst=NULL; } \
  if (cache.txt) { fclose(cache.txt); cache.txt=NULL; } \
  if (opt.log) fflush(opt.log); \
  if (opt.errlog) fflush(opt.errlog);\
  if (makestat_fp) { fclose(makestat_fp); makestat_fp=NULL; } \
  if (maketrack_fp){ fclose(maketrack_fp); maketrack_fp=NULL; } \
  if (opt.accept_cookie) cookie_save(opt.cookie,fconcat(opt.path_log,"cookies.txt")); \
  if (makeindex_fp) { fclose(makeindex_fp); makeindex_fp=NULL; } \
  if (cache_hashtable) { inthash_delete(&cache_hashtable); } \
  if (template_header) { freet(template_header); template_header=NULL; } \
  if (template_body)   { freet(template_body); template_body=NULL; } \
  if (template_footer) { freet(template_footer); template_footer=NULL; } \
  structcheck_init(-1); \
}
#define XH_uninit XH_extuninit if (r.adr) { freet(r.adr); r.adr=NULL; } 

// Enregistrement d'un lien:
// on calcule la taille nécessaire: taille des 3 chaînes à stocker (taille forcée paire, plus 2 octets de sécurité)
// puis on vérifie qu'on a assez de marge dans le buffer - sinon on en réalloue un autre
// enfin on écrit à l'adresse courante du buffer, qu'on incrémente. on décrémente la taille dispo d'autant ensuite
// codebase: si non nul et si .class stockee on le note pour chemin primaire pour classes
// FA,FS: former_adr et former_fil, lien original
#define REALLOC_SIZE 8192
#if HTS_HASH
#define liens_record_sav_len(A) 
#else
#define liens_record_sav_len(A) (A)->sav_len=strlen((A)->sav)
#endif

#define liens_record(A,F,S,FA,FF) { \
int notecode=0; \
int lienurl_len=((sizeof(lien_url)+HTS_ALIGN-1)/HTS_ALIGN)*HTS_ALIGN,\
  adr_len=strlen(A),\
  fil_len=strlen(F),\
  sav_len=strlen(S),\
  cod_len=0,\
  former_adr_len=strlen(FA),\
  former_fil_len=strlen(FF); \
if (former_adr_len>0) {\
  former_adr_len=(former_adr_len/HTS_ALIGN)*HTS_ALIGN+HTS_ALIGN*2; \
  former_fil_len=(former_fil_len/HTS_ALIGN)*HTS_ALIGN+HTS_ALIGN*2; \
} else former_adr_len=former_fil_len=0;\
if (strlen(F)>6) if (strnotempty(codebase)) if (strfield(F+strlen(F)-6,".class")) { notecode=1; \
cod_len=strlen(codebase); cod_len=(cod_len/HTS_ALIGN)*HTS_ALIGN+HTS_ALIGN*2; } \
adr_len=(adr_len/HTS_ALIGN)*HTS_ALIGN+HTS_ALIGN*2; fil_len=(fil_len/HTS_ALIGN)*HTS_ALIGN+HTS_ALIGN*2; sav_len=(sav_len/HTS_ALIGN)*HTS_ALIGN+HTS_ALIGN*2; \
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
if (notecode) { liens[lien_tot]->cod=lien_buffer; lien_buffer+=cod_len; lien_size-=cod_len; strcpy(liens[lien_tot]->cod,codebase); } \
if (former_adr_len>0) {\
liens[lien_tot]->former_adr=lien_buffer; lien_buffer+=former_adr_len; lien_size-=former_adr_len; \
liens[lien_tot]->former_fil=lien_buffer; lien_buffer+=former_fil_len; lien_size-=former_fil_len; \
strcpy(liens[lien_tot]->former_adr,FA); \
strcpy(liens[lien_tot]->former_fil,FF); \
}\
strcpy(liens[lien_tot]->adr,A); \
strcpy(liens[lien_tot]->fil,F); \
strcpy(liens[lien_tot]->sav,S); \
liens_record_sav_len(liens[lien_tot]); \
hash_write(&hash,lien_tot);  \
} \
}

/* - abandonné (simplifie) -
// Ajouter à un lien EXISTANT deux champs former_adr et former_fil pour indiquer le nom d'un fichier avant un "move"
// NOTE: si un alloc est fait ici il n'y aura pas de freet() à la fin, tant pis (firstbloc)
#define liens_add_former(index,A,F) { \
int adr_len=strlen(A),fil_len=strlen(F); \
adr_len=(adr_len/HTS_ALIGN)*HTS_ALIGN+HTS_ALIGN+4; fil_len=(fil_len/HTS_ALIGN)*HTS_ALIGN+HTS_ALIGN+4; \
if ((int) lien_size < (int) (adr_len+fil_len)) { \
lien_buffer=(char*) calloct(add_tab_alloc,1); \
lien_size=add_tab_alloc; \
} \
if (lien_buffer!=NULL) { \
if (liens[lien_tot]!=NULL) { \
liens[lien_tot]->former_adr=lien_buffer; lien_buffer+=adr_len; lien_size-=adr_len; \
liens[lien_tot]->former_fil=lien_buffer; lien_buffer+=fil_len; lien_size-=fil_len; \
strcpy(liens[lien_tot]->former_adr,A); \
strcpy(liens[lien_tot]->former_fil,F); \
} \
} \
}
*/

#if 0
#define HT_ADD_ADR { \
  fwrite(lastsaved,1,((int) (adr - lastsaved)),fp); \
  lastsaved=adr; }
#define HT_ADD(A) fwrite(A,1,(int) strlen(A),fp);
#define HT_ADD_START
#define HT_ADD_END if (fp) { fclose(fp); fp=NULL; }
#define HT_ADD_FOP { \
  fp=filecreate(savename); \
  if (fp==NULL) { \
  if (opt.errlog) { \
  fspc(opt.errlog,"error"); fprintf(opt.errlog,"Unable to create %s for %s%s"LF,savename,urladr,urlfil); \
  test_flush; \
  } \
  freet(r.adr); r.adr=NULL; \
  error=1; \
  } \
  }
#else
// version optimisée, qui permet de ne pas toucher aux html non modifiés (update)
#define HT_ADD_CHK(A) if (((int) (A)+ht_len+1) >= ht_size) { \
  ht_size=(A)+ht_len+REALLOC_SIZE; \
  ht_buff=(char*) realloct(ht_buff,ht_size); \
  if (ht_buff==NULL) { \
  printf("PANIC! : Not enough memory [%d]\n",__LINE__); \
  XH_uninit; \
  exit(1); \
  } \
  } \
  ht_len+=A;
/*
(Optimized)
#define HT_ADD_ADR { int i,j=ht_len; HT_ADD_CHK(((int) adr)- ((int) lastsaved)) \
  for(i=0;i<((int) adr)- ((int) lastsaved);i++) \
  ht_buff[j+i]=lastsaved[i]; \
  ht_buff[j+((int) adr)- ((int) lastsaved)]='\0'; \
  lastsaved=adr; }
*/
#define HT_ADD_ADR \
  if ((opt.getmode & 1) && (ptr>0)) { \
  int i=((int) (adr - lastsaved)),j=ht_len; HT_ADD_CHK(i) \
  memcpy(ht_buff+j, lastsaved, i); \
  ht_buff[j+i]='\0'; \
  lastsaved=adr; \
  }
/*
(Optimized)
#define HT_ADD(A) { HT_ADD_CHK(strlen(A)) strcat(ht_buff,A); }
*/
#define HT_ADD(A) \
  if ((opt.getmode & 1) && (ptr>0)) { \
  int i=strlen(A),j=ht_len; \
  if (i) { \
  HT_ADD_CHK(i) \
  memcpy(ht_buff+j, A, i); \
  ht_buff[j+i]='\0'; \
  } }
#define HT_ADD_START \
  int ht_size=(int)(r.size*5)/4+REALLOC_SIZE; \
  int ht_len=0; \
  char* ht_buff=NULL; \
  if ((opt.getmode & 1) && (ptr>0)) { \
  ht_buff=(char*) malloct(ht_size); \
  if (ht_buff==NULL) { \
  printf("PANIC! : Not enough memory [%d]\n",__LINE__); \
  XH_uninit; \
  exit(1); \
  } \
  ht_buff[0]='\0'; \
  }
#define HT_ADD_END { \
  int ok=0;\
  if (ht_buff) { \
  int file_len=(int) strlen(ht_buff);\
  char digest[32+2];\
  digest[0]='\0';\
  domd5mem(ht_buff,file_len,digest,1);\
  if (fsize(antislash(savename))==file_len) { \
  int mlen;\
  char* mbuff;\
  cache_readdata(&cache,"//[HTML-MD5]//",savename,&mbuff,&mlen);\
  if (mlen) mbuff[mlen]='\0';\
  if ((mlen == 32) && (strcmp(((mbuff!=NULL)?mbuff:""),digest)==0)) {\
  ok=1;\
  if ( (opt.debug>1) && (opt.log!=NULL) ) {\
  fspc(opt.log,"debug"); fprintf(opt.log,"File not re-written (md5): %s"LF,savename);\
  test_flush;\
  }\
  } else {\
  ok=0;\
  } \
  }\
  if (!ok) { \
  fp=filecreate(savename); \
  if (fp) { \
  if (file_len>0) {\
  if ((int)fwrite(ht_buff,1,file_len,fp) != file_len) { \
  if (opt.errlog) {   \
  fspc(opt.errlog,"error"); fprintf(opt.errlog,"Unable to write HTML file %s"LF,savename);\
  test_flush;\
  }\
  }\
  }\
  fclose(fp); fp=NULL; \
  if (strnotempty(r.lastmodified)) \
  set_filetime_rfc822(savename,r.lastmodified); \
  usercommand(0,NULL,antislash(savename)); \
  } else {\
  if (opt.errlog) { \
  fspc(opt.errlog,"error");\
  fprintf(opt.errlog,"Unable to save file %s"LF,savename);\
  test_flush;\
  }\
  }\
  } else {\
  filenote(savename,NULL); \
  }\
  if (cache.ndx)\
    cache_writedata(cache.ndx,cache.dat,"//[HTML-MD5]//",savename,digest,(int)strlen(digest));\
  } \
  freet(ht_buff); ht_buff=NULL; \
  }
#define HT_ADD_FOP 
#endif

// libérer filters[0] pour insérer un élément dans filters[0]
#define HT_INSERT_FILTERS0 {\
  int i;\
  if (filptr>0) {\
    for(i=filptr-1;i>=0;i--) {\
      strcpy(filters[i+1],filters[i]);\
    }\
  }\
  strcpy(filters[0],"");\
  filptr++;\
  filptr=minimum(filptr,filter_max);\
}

#define HT_INDEX_END do { \
if (!makeindex_done) { \
if (makeindex_fp) { \
  char tempo[1024]; \
  if (makeindex_links == 1) { \
    sprintf(tempo,"<meta HTTP-EQUIV=\"Refresh\" CONTENT=\"0; URL=%s\">"CRLF,makeindex_firstlink); \
  } else \
    tempo[0]='\0'; \
  fprintf(makeindex_fp,template_footer, \
    "<!-- Mirror and index made by HTTrack Website Copier/"HTTRACK_VERSION" "HTTRACK_AFF_AUTHORS" -->", \
    tempo \
    ); \
  fflush(makeindex_fp); \
  fclose(makeindex_fp);  /* à ne pas oublier sinon on passe une nuit blanche */  \
  makeindex_fp=NULL; \
  usercommand(0,NULL,fconcat(opt.path_html,"index.html"));  \
} \
} \
makeindex_done=1;    /* ok c'est fait */  \
} while(0)




// Début de httpmirror, robot
// url1 peut être multiple
int httpmirror(char* url1,httrackp* ptropt) {
  httrackp opt = *ptropt;      // structure d'options
  char* primary=NULL;          // première page, contenant les liens à scanner
  int lien_tot=0;              // nombre de liens pour le moment
  lien_url** liens=NULL;       // les pointeurs sur les liens
  hash_struct hash;            // système de hachage, accélère la recherche dans les liens
  t_cookie cookie;             // gestion des cookies
  int lien_max=0;
  int lien_size=0;        // octets restants dans buffer liens dispo
  char* lien_buffer=NULL; // buffer liens actuel
  int add_tab_alloc=256000;    // +256K de liens à chaque fois
  //char* tab_alloc=NULL;
  int ptr;             // pointeur actuel sur les liens
  //
  int numero_passe=0;  // deux passes pour html puis images
  int back_max=0;      // fichiers qui peuvent être en local
  lien_back* back=NULL; // backing en local
  htsblk r;            // retour de certaines fonctions
  TStamp lastime=0;    // pour affichage infos de tmp en tmp
  // pour les stats, nombre de fichiers & octets écrits
  LLint stat_fragment=0;  // pour la fragmentation
  //TStamp istat_timestart;   // départ pour calcul instantanné
  //
  TStamp last_info_shell=0;
  int info_shell=0;
  // filtres
  char** filters = NULL;
  //int filter_max=0;
  int filptr=0;
  //
  int makeindex_done=0;  // lorsque l'index sera fait
  FILE* makeindex_fp=NULL;
  int makeindex_links=0;
  char makeindex_firstlink[HTS_URLMAXSIZE*2];
  // statistiques (mode #Z)
  FILE* makestat_fp=NULL;    // fichier de stats taux transfert
  FILE* maketrack_fp=NULL;   // idem pour le tracking
  TStamp makestat_time=0;    // attente (secondes)
  LLint makestat_total=0;    // repère du nombre d'octets transférés depuis denrière stat
  int makestat_lnk=0;        // idem, pour le nombre de liens
  //
  char codebase[HTS_URLMAXSIZE*2];  // base pour applet java
  char base[HTS_URLMAXSIZE*2];      // base pour les autres fichiers
  //
  cache_back cache;
  robots_wizard robots;    // gestion robots.txt
  inthash cache_hashtable=NULL;
  int cache_hash_size=0;
  //
  char *template_header=NULL,*template_body=NULL,*template_footer=NULL;
  //
  codebase[0]='\0'; base[0]='\0';
  //
  cookie.auth.next=NULL;
  cookie.auth.auth[0]=cookie.auth.prefix[0]='\0';
  //

  // noter heure actuelle de départ en secondes
  memset(&HTS_STAT, 0, sizeof(HTS_STAT));
  HTS_STAT.stat_timestart=time_local();
  //istat_timestart=stat_timestart;
  HTS_STAT.istat_timestart[0]=HTS_STAT.istat_timestart[1]=mtime_local();
  /* reset stats */
  HTS_STAT.HTS_TOTAL_RECV=0;
  HTS_STAT.istat_bytes[0]=HTS_STAT.istat_bytes[1]=0;
  if (opt.aff_progress)
    lastime=HTS_STAT.stat_timestart;
  if (opt.shell) {
    last_info_shell=HTS_STAT.stat_timestart;
  }
  if ((opt.makestat) || (opt.maketrack)){
    makestat_time=HTS_STAT.stat_timestart;
  }
  // initialiser compteur erreurs
  fspc(NULL,NULL);

  // initialiser cookie
  if (opt.accept_cookie) {
    opt.cookie=&cookie;
    cookie.max_len=30000;       // max len
    strcpy(cookie.data,"");
    // Charger cookies.txt par défaut ou cookies.txt du miroir
    if (fexist(fconcat(opt.path_log,"cookies.txt")))
      cookie_load(opt.cookie,opt.path_log,"cookies.txt");
    else if (fexist("cookies.txt"))
      cookie_load(opt.cookie,"","cookies.txt");
  } else
    opt.cookie=NULL;

  // initialiser exit_xh
  exit_xh=0;          // sortir prématurément (var globale)

  // initialiser usercommand
  usercommand(opt.sys_com_exec,opt.sys_com,"");

  // initialiser structcheck
  structcheck_init(1);

  // initialiser tableau options accessible par d'autres fonctions (signal)
  hts_declareoptbuffer(&opt);

  // initialiser verif_backblue
  verif_backblue(NULL);
  verif_external(0,0);
  verif_external(1,0);

  // et templates html
  template_header=readfile_or(fconcat(opt.path_bin,"templates/index-header.html"),HTS_INDEX_HEADER);
  template_body=readfile_or(fconcat(opt.path_bin,"templates/index-body.html"),HTS_INDEX_BODY);
  template_footer=readfile_or(fconcat(opt.path_bin,"templates/index-footer.html"),HTS_INDEX_FOOTER);

  // initialiser mimedefs
  get_userhttptype(1,opt.mimedefs,NULL);

  // Initialiser indexation
  if (opt.kindex)
    index_init(opt.path_html);

  // effacer bloc cache
  memset(&cache, 0, sizeof(cache_back));
  cache.type=opt.cache;  // cache?
  cache.errlog=opt.errlog;  // err log?
  cache.ptr_ant=cache.ptr_last=0;   // pointeur pour anticiper

  // initialiser hash cache
  if (!cache_hash_size) 
    cache_hash_size=HTS_HASH_SIZE;
  cache_hashtable=inthash_new(cache_hash_size);
  if (cache_hashtable==NULL) {
    printf("PANIC! : Not enough memory [%d]\n",__LINE__);
    filters[0]=NULL; back_max=0;    // uniquement a cause du warning de XH_extuninit
    XH_extuninit;
    return 0;
  }
  cache.hashtable=(void*)cache_hashtable;      /* copy backcache hash */

  // initialiser cache DNS
  _hts_lockdns(-999);
  
  // robots.txt
  strcpy(robots.adr,"!");    // dummy
  robots.token[0]='\0';
  robots.next=NULL;          // suivant
  opt.robotsptr = &robots;
  
  // effacer filters
  opt.maxfilter = maximum(opt.maxfilter, 128);
  if (filters_init(&filters, opt.maxfilter, 0) == 0) {
    printf("PANIC! : Not enough memory [%d]\n",__LINE__);
    back_max=0;    // uniquement a cause du warning de XH_extuninit
    XH_extuninit;
    return 0;
  }
  opt.filters.filters=&filters;
  //
  opt.filters.filptr=&filptr;
  //opt.filters.filter_max=&filter_max;
  
  // tableau de pointeurs sur les liens
  lien_max=maximum(opt.maxlink,32);
  liens=(lien_url**) malloct(lien_max*sizeof(lien_url*));   // tableau de pointeurs sur les liens
  if (liens==NULL) {
    printf("PANIC! : Not enough memory [%d]\n",__LINE__);
    //XH_uninit;
    return 0;
  } else {
    int i;
    for(i=0;i<lien_max;i++) {
      liens[i]=NULL;     
    }
  }
  // initialiser ptr et lien_tot
  ptr=0;
  lien_tot=0;
#if HTS_HASH
  // initialiser hachage
  {
    int i;
    for(i=0;i<HTS_HASH_SIZE;i++)
      hash.hash[0][i]=hash.hash[1][i]=hash.hash[2][i] = -1;    // pas d'entrées
    hash.liens = liens;
    hash.max_lien=0;
  }
#endif

  
  // copier adresse(s) dans liste des adresses
  {
    char *a=url1;
    int primary_len=8192;
    if (strnotempty(opt.filelist)) {
      primary_len+=max(0,fsize(opt.filelist)*2);
    }
    primary_len+=strlen(url1)*2;

    // création de la première page, qui contient les liens de base à scanner
    // c'est plus propre et plus logique que d'entrer à la main les liens dans la pile
    // on bénéficie ainsi des vérifications et des tests du robot pour les liens "primaires"
    primary=(char*) malloct(primary_len); 
    if (primary) {
      primary[0]='\0';
    } else {
      printf("PANIC! : Not enough memory [%d]\n",__LINE__);
      back_max=0;    // uniquement a cause du warning de XH_extuninit
      XH_extuninit;
      return 0;
    }
    
    while(*a) {
      int i;
      int joker=0;

      // vérifier qu'il n'y a pas de * dans l'url
      if (*a=='+')
        joker=1;
      else if (*a=='-')
        joker=1;
      /* NON, certaines URL ont des * (!)
      else {
        int i=0;
        while((a[i]!=0) && (a[i]!=' ')) if (a[i++]=='*') joker=1;
      }
      */
      
      if (joker) {    // joker ou filters
        //char* p;
        char tempo[HTS_URLMAXSIZE*2];
        int type; int plus=0;

        // noter joker (dans b)
        if (*a=='+') {  // champ +
          type=1; plus=1; a++;
        } else if (*a=='-') {  // champ forbidden[]
          type=0; a++;
        } else {  // champ + avec joker sans doute
          type=1;
        }

        // recopier prochaine chaine (+ ou -)
        i=0;
        while((*a!=0) && (*a!=' ')) { tempo[i++]=*a; a++; }  
        tempo[i++]='\0';
        while(*a==' ') { a++; }

        // sauter les + sans rien après..
        if (strnotempty(tempo)) {
          if ((plus==0) && (type==1)) {  // implicite: *www.edf.fr par exemple
            if (tempo[strlen(tempo)-1]!='*') {
              strcat(tempo,"*");  // ajouter un *
            }
          }
          if (type)
            strcpy(filters[filptr],"+");
          else
            strcpy(filters[filptr],"-");
          /*
          if (strfield(tempo,"http://"))
            strcat(filters[filptr],tempo+7);        // ignorer http://
          else if (strfield(tempo,"ftp://"))
            strcat(filters[filptr],tempo+6);        // ignorer ftp://
          else
          */
          strcat(filters[filptr],tempo);
          filptr++;
          
          /* sanity check */
          if (filptr + 1 >= opt.maxfilter) {
            opt.maxfilter += HTS_FILTERSINC;
            if (filters_init(&filters, opt.maxfilter, HTS_FILTERSINC) == 0) {
              printf("PANIC! : Too many filters : >%d [%d]\n",filptr,__LINE__);
              if (opt.errlog) {
                fprintf(opt.errlog,LF"Too many filters, giving up..(>%d)"LF,filptr);
                fprintf(opt.errlog,"To avoid that: use #F option for more filters (example: -#F5000)"LF);
                test_flush;
              }
              back_max=0;    // uniquement a cause du warning de XH_extuninit
              XH_extuninit;
              return 0;
            }
            //opt.filters.filters=filters;
          }

        }
        
      } else {    // adresse normale
        char url[HTS_URLMAXSIZE*2];
        // prochaine adresse
        i=0;
        while((*a!=0) && (*a!=' ')) { url[i++]=*a; a++; }  
        while(*a==' ') { a++; }
        url[i++]='\0';

        //strcat(primary,"<PRIMARY=\"");
        if (strstr(url,":/")==NULL)
          strcat(primary,"http://");
        strcat(primary,url);
        //strcat(primary,"\">");
        strcat(primary,"\n");
      }
    }  // while

    /* load URL file list */
    /* OPTIMIZED for fast load */
    if (strnotempty(opt.filelist)) {
      char* filelist_buff=NULL;
      int filelist_sz=fsize(opt.filelist);
      if (filelist_sz>0) {
        FILE* fp=fopen(opt.filelist,"rb");
        if (fp) {
          filelist_buff=malloct(filelist_sz + 2);
          if (filelist_buff) {
            if ((int)fread(filelist_buff,1,filelist_sz,fp) != filelist_sz) {
              freet(filelist_buff);
              filelist_buff=NULL;
            } else {
              *(filelist_buff + filelist_sz) = '\0';
            }
          }
          fclose(fp);
        }
      }
      
      if (filelist_buff) {
        int filelist_ptr=0;
        int n=0;
        char line[HTS_URLMAXSIZE*2];
        char* primary_ptr = primary + strlen(primary);
        while( filelist_ptr < filelist_sz ) {
          int count=binput(filelist_buff+filelist_ptr,line,HTS_URLMAXSIZE);
          filelist_ptr+=count;
          if (count && line[0]) {
            n++;
            if (strstr(line,":/")==NULL) {
              strcpy(primary_ptr, "http://");
              primary_ptr += strlen(primary_ptr);
            }
            strcpy(primary_ptr, line);
            primary_ptr += strlen(primary_ptr);
            strcpy(primary_ptr, "\n");
            primary_ptr += 1;
          }
        }
        // fclose(fp);
        if (opt.log!=NULL) {
          fspc(opt.log,"info"); fprintf(opt.log,"%d links added from %s"LF,n,opt.filelist); test_flush;
        }

        // Free buffer
        freet(filelist_buff);
      } else {
        if (opt.errlog!=NULL) {
          fspc(opt.errlog,"error"); fprintf(opt.errlog,"Could not include URL list: %s"LF,opt.filelist); test_flush;
        }
      }
    }


    // lien primaire
    liens_record("primary","/primary","primary.html","","");
    if (liens[lien_tot]==NULL) {  // erreur, pas de place réservée
      printf("PANIC! : Not enough memory [%d]\n",__LINE__);
      if (opt.errlog) {
        fprintf(opt.errlog,"Not enough memory, can not re-allocate %d bytes"LF,(int)((add_tab_alloc+1)*sizeof(lien_url)));
        test_flush;
      }
      back_max=0;    // uniquement a cause du warning de XH_extuninit
      XH_extuninit;    // désallocation mémoire & buffers
      return 0;
    }    
    liens[lien_tot]->testmode=0;          // pas mode test
    liens[lien_tot]->link_import=0;       // pas mode import
    liens[lien_tot]->depth=opt.depth+1;   // lien de priorité maximale
    liens[lien_tot]->pass2=0;             // 1ère passe
    liens[lien_tot]->retry=opt.retry;     // lien de priorité maximale
    liens[lien_tot]->premier=lien_tot;    // premier lien, objet-père=objet              
    liens[lien_tot]->precedent=lien_tot;  // lien précédent
    lien_tot++;  

    // Initialiser cache
    cache_init(&cache,&opt);
  }
  
#if BDEBUG==3
  {
    int i;
    for(i=0;i<lien_tot;i++) {
      printf("%d>%s%s as %s\n",i,liens[i]->adr,liens[i]->fil,liens[i]->sav);
    }
    for(i=0;i<filptr;i++) {
      printf("%d>filters=%s\n",i,filters[i]);
    }
  }
#endif
   
  // backing
  //soc_max=opt.maxsoc;
  if (opt.maxsoc>0) {
#if BDEBUG==2
    _CLRSCR;
#endif
    // Nombre de fichiers HTML pouvant être présents en mémoire de manière simultannée
    // On prévoit large: les fichiers HTML ne prennent que peu de place en mémoire, et les
    // fichiers non html sont sauvés en direct sur disque.
    // --> 1024 entrées + 32 entrées par socket en supplément
    back_max=opt.maxsoc*32+1024;
    //back_max=opt.maxsoc*8+32;
    back=(lien_back*) calloct((back_max+1),sizeof(lien_back));
    if (back==NULL) {
      if (opt.errlog)
        fprintf(opt.errlog,"Not enough memory, can not allocate %d bytes"LF,(int)((opt.maxsoc+1)*sizeof(lien_back)));
      return 0;
    } else {    // copier buffer-location & effacer
      int i;
      for(i=0;i<back_max;i++){
        back[i].r.location=back[i].location_buffer;
        back[i].status=-1;
        back[i].r.soc=INVALID_SOCKET;
      }
    }
  }


  // flush
  test_flush;

  // statistiques
  if (opt.makestat) {
    makestat_fp=fopen(fconcat(opt.path_log,"hts-stats.txt"),"wb");
    if (makestat_fp != NULL) {
      fprintf(makestat_fp,"HTTrack statistics report, every minutes"LF LF);
    }
  }

  // tracking -- débuggage
  if (opt.maketrack) {
    maketrack_fp=fopen(fconcat(opt.path_log,"hts-track.txt"),"wb");
    if (maketrack_fp != NULL) {
      fprintf(maketrack_fp,"HTTrack tracking report, every minutes"LF LF);
    }
  }

  // on n'a pas de liens!! (exemple: httrack www.* impossible sans départ..)
  if (lien_tot<=0) {
    if (opt.errlog) {
      fprintf(opt.errlog,"Error! You MUST specify at least one complete URL, and not only wildcards!"LF);
    }
  }


  // attendre une certaine heure..
  if (opt.waittime>0) {
    int rollover=0;
    int ok=0;
    {
      TStamp tl=0;
      time_t tt;
      struct tm* A;
      tt=time(NULL);
      A=localtime(&tt);
      tl+=A->tm_sec;
      tl+=A->tm_min*60;
      tl+=A->tm_hour*60*60;
      if (tl>opt.waittime)  // attendre minuit
        rollover=1;
    }

    // attendre..
    do {
      TStamp tl=0;
      time_t tt;
      struct tm* A;
      tt=time(NULL);
      A=localtime(&tt);
      tl+=A->tm_sec;
      tl+=A->tm_min*60;
      tl+=A->tm_hour*60*60;

      if (rollover) {
        if (tl<=opt.waittime)
          rollover=0;  // attendre heure
      } else {
        if (tl>opt.waittime)
          ok=1;  // ok!
      }
      
#if HTS_ANALYSTE
      {  
        int r;
        if (rollover)
          r=hts_htmlcheck_loop(back,back_max,0,0,lien_tot,(int) (opt.waittime-tl+24*3600),NULL);
        else
          r=hts_htmlcheck_loop(back,back_max,0,0,lien_tot,(int) (opt.waittime-tl),NULL);
        if (!r) {
          exit_xh=1;  // exit requested
          ok=1;          
        } else
          Sleep(100);
      }
#endif
    } while(!ok);    
    
    // note: recopie de plus haut
    // noter heure actuelle de départ en secondes
    HTS_STAT.stat_timestart=time_local();
    if (opt.aff_progress)
      lastime=HTS_STAT.stat_timestart;
    if (opt.shell) {
      last_info_shell=HTS_STAT.stat_timestart;
    }
    if ((opt.makestat) || (opt.maketrack)){
      makestat_time=HTS_STAT.stat_timestart;
    }


  }  
  /* Info for wrappers */
  if ( (opt.debug>0) && (opt.log!=NULL) ) {
    fspc(opt.log,"info"); fprintf(opt.log,"engine: start"LF);
  }
#if HTS_ANALYSTE
  if (!hts_htmlcheck_start(&opt)) {
    XH_extuninit;
    return 1;
  }
#endif
  

  // ------------------------------------------------------------

  // ------------------------------------------------------------
  // Boucle générale de parcours des liens
  // ------------------------------------------------------------
  do {
    int error=0;          // si error alors sauter
    int store_errpage=0;  // c'est une erreur mais on enregistre le html
    char loc[HTS_URLMAXSIZE*2];    // adresse de relocation

    // Ici on charge le fichier (html, gif..) en mémoire
    // Les HTMLs sont traités (si leur priorité est suffisante)

    // effacer r
    memset(&r, 0, sizeof(htsblk)); r.soc=INVALID_SOCKET;
    r.location=loc;    // en cas d'erreur 3xx (moved)
    // recopier proxy
    memcpy(&(r.req.proxy), &opt.proxy, sizeof(opt.proxy));
    // et user-agent
    strcpy(r.req.user_agent,opt.user_agent);
    r.req.user_agent_send=opt.user_agent_send;

    if (!error) {
      
      // Skip empty/invalid/done in background
      if (liens[ptr]) {
        while (  (liens[ptr]) && (
                    ( ((urladr != NULL)?(urladr):(" "))[0]=='!') ||
                    ( ((urlfil != NULL)?(urlfil):(" "))[0]=='\0') ||
                    ( (liens[ptr]->pass2 == -1) )
                 )
               ) {  // sauter si lien annulé (ou fil vide)
          if ((opt.debug>1) && (opt.log!=NULL)) {
            fspc(opt.log,"debug"); fprintf(opt.log,"link #%d seems ready, skipping: %s%s.."LF,ptr,((urladr != NULL)?(urladr):(" ")),((urlfil != NULL)?(urlfil):(" ")));
            test_flush;
          }
          ptr++;
        }
      }
      if (liens[ptr]) {    // on a qq chose à récupérer?

        if ( (opt.debug>1) && (opt.log!=NULL) ) {
          fspc(opt.log,"debug"); fprintf(opt.log,"Wait get: %s%s"LF,urladr,urlfil);
          test_flush;
#if DEBUG_ROBOTS
          if (strcmp(urlfil,"/robots.txt") == 0) {
            printf("robots.txt detected\n");
          }
#endif
        }    
        // ------------------------------------------------------------
        // DEBUT --RECUPERATION LIEN---
        if (ptr==0) {              // premier lien à parcourir: lien primaire construit avant
          r.adr=primary; primary=NULL;
          r.statuscode=200;
          r.size=strlen(r.adr);
          r.soc=INVALID_SOCKET;
          strcpy(r.contenttype,"text/html");
        /*} else if (opt.maxsoc<=0) {   // fichiers 1 à 1 en attente (pas de backing)
          // charger le fichier en mémoire tout bêtement
          r=xhttpget(urladr,urlfil);
          //
        */
        } else {    // backing, multiples sockets
          //
          int b;
          int n;
          
#if BDEBUG==1
          printf("\nBack test..\n");
#endif

          // pause/lock files
          {
            int do_pause=0;

            // user pause lockfile : create hts-paused.lock --> HTTrack will be paused
            if (fexist(fconcat(opt.path_log,"hts-stop.lock"))) {
              // remove lockfile
              remove(fconcat(opt.path_log,"hts-stop.lock"));
              if (!fexist(fconcat(opt.path_log,"hts-stop.lock"))) {
                do_pause=1;
              }
            }
            
            // after receving N bytes, pause
            if (opt.fragment>0) {
              if ((HTS_STAT.stat_bytes-stat_fragment) > opt.fragment) {
                do_pause=1;
              }
            }
            
            // pause?
            if (do_pause) {
              if ( (opt.debug>0) && (opt.log!=NULL) ) {
                fspc(opt.log,"info"); fprintf(opt.log,"engine: pause requested.."LF);
              }
              while (back_nsoc(back,back_max)>0) {                  // attendre fin des transferts
                back_wait(back,back_max,&opt,&cache,HTS_STAT.stat_timestart);
                Sleep(200);
#if HTS_ANALYSTE
                {
                  back_wait(back,back_max,&opt,&cache,HTS_STAT.stat_timestart);
                  
                  // Transfer rate
                  engine_stats();
                  
                  // Refresh various stats
                  HTS_STAT.stat_nsocket=back_nsoc(back,back_max);
                  HTS_STAT.stat_errors=fspc(NULL,"error");
                  HTS_STAT.stat_warnings=fspc(NULL,"warning");
                  HTS_STAT.stat_infos=fspc(NULL,"info");
                  HTS_STAT.nbk=backlinks_done(liens,lien_tot,ptr);
                  HTS_STAT.nb=back_transfered(HTS_STAT.stat_bytes,back,back_max);
                  
                  b=0;
                  if (!hts_htmlcheck_loop(back,back_max,b,ptr,lien_tot,(int) (time_local()-HTS_STAT.stat_timestart),&HTS_STAT)) {
                    if (opt.errlog) {
                      fspc(opt.errlog,"info"); fprintf(opt.errlog,"Exit requested by shell or user"LF);
                      test_flush;
                    }
                    exit_xh=1;  // exit requested
                    XH_uninit;
                    return 0;
                  }
                }
#endif
              }
              // On désalloue le buffer d'enregistrement des chemins créée, au cas où pendant la pause
              // l'utilisateur ferait un rm -r après avoir effectué un tar
              structcheck_init(1);
              {
                FILE* fp = fopen(fconcat(opt.path_log,"hts-paused.lock"),"wb");
                if (fp) {
                  fspc(fp,"info");  // dater
                  fprintf(fp,"Pause"LF"HTTrack is paused after retreiving "LLintP" bytes"LF"Delete this file to continue the mirror..."LF""LF"",HTS_STAT.stat_bytes);
                  fclose(fp);
                }
              }
              stat_fragment=HTS_STAT.stat_bytes;
              /* Info for wrappers */
              if ( (opt.debug>0) && (opt.log!=NULL) ) {
                fspc(opt.log,"info"); fprintf(opt.log,"engine: pause: %s"LF,fconcat(opt.path_log,"hts-paused.lock"));
              }
#if HTS_ANALYSTE
              hts_htmlcheck_pause(fconcat(opt.path_log,"hts-paused.lock"));
#else
              while (fexist(fconcat(opt.path_log,"hts-paused.lock"))) {
                //back_wait(back,back_max,&opt,&cache,HTS_STAT.stat_timestart);   inutile!! (plus de sockets actives)
                Sleep(1000);
              }
#endif
            }
            //
          }
          // end of pause/lock files
          
#if HTS_ANALYSTE
          // changement dans les préférences
/*
          if (_hts_setopt) {
            copy_htsopt(_hts_setopt,&opt);    // copier au besoin
            _hts_setopt=NULL;                 // effacer callback
          }
*/
          if (_hts_addurl) {
            char add_adr[HTS_URLMAXSIZE*2];
            char add_fil[HTS_URLMAXSIZE*2];
            while(*_hts_addurl) {
              char add_url[HTS_URLMAXSIZE*2];
              add_adr[0]=add_fil[0]=add_url[0]='\0';
              if (!link_has_authority(*_hts_addurl))
                strcpy(add_url,"http://");          // ajouter http://
              strcat(add_url,*_hts_addurl);
              if (ident_url_absolute(add_url,add_adr,add_fil)>=0) {
                // ----Ajout----
                // noter NOUVEAU lien
                char add_sav[HTS_URLMAXSIZE*2];
                // calculer lien et éventuellement modifier addresse/fichier
                if (url_savename(add_adr,add_fil,add_sav,NULL,NULL,NULL,NULL,&opt,liens,lien_tot,back,back_max,&cache,&hash,ptr,numero_passe)!=-1) { 
                  if (hash_read(&hash,add_sav,"",0)<0) {      // n'existe pas déja
                    // enregistrer lien (MACRO)
                    liens_record(add_adr,add_fil,add_sav,"","");
                    if (liens[lien_tot]!=NULL) {    // OK, pas d'erreur
                      liens[lien_tot]->testmode=0;          // mode test?
                      liens[lien_tot]->link_import=0;       // mode normal
                      liens[lien_tot]->depth=opt.depth;
                      liens[lien_tot]->pass2=max(0,numero_passe);
                      liens[lien_tot]->retry=opt.retry;
                      liens[lien_tot]->premier=lien_tot;
                      liens[lien_tot]->precedent=lien_tot;
                      lien_tot++;
                      //
                      if ((opt.debug>0) && (opt.log!=NULL)) {
                        fspc(opt.log,"info"); fprintf(opt.log,"Link added by user: %s%s"LF,add_adr,add_fil); test_flush;
                      }
                      //
                    } else {  // oups erreur, plus de mémoire!!
                      printf("PANIC! : Not enough memory [%d]\n",__LINE__);
                      if (opt.errlog) {
                        fprintf(opt.errlog,"Not enough memory, can not re-allocate %d bytes"LF,(int)((add_tab_alloc+1)*sizeof(lien_url)));
                        test_flush;
                      }
                      //if (opt.getmode & 1) { if (fp) { fclose(fp); fp=NULL; } }
                      XH_uninit;    // désallocation mémoire & buffers
                      return 0;
                    }
                  } else {
                    if ( (opt.debug>0) && (opt.errlog!=NULL) ) {
                      fspc(opt.errlog,"warning"); fprintf(opt.errlog,"Existing link %s%s not added after user request"LF,add_adr,add_fil);
                      test_flush;
                    }
                  }
                  
                }
              } else {
                if (opt.errlog) {
                  fspc(opt.errlog,"error");
                  fprintf(opt.errlog,"Error during URL decoding for %s"LF,add_url);
                  test_flush;
                }
              }
              // ----Fin Ajout----
              _hts_addurl++;                  // suivante
            }
            _hts_addurl=NULL;           // libérer _hts_addurl
          }
          // si une pause a été demandée
          if (_hts_setpause) {
            // index du lien actuel
            int b=back_index(back,back_max,urladr,urlfil,savename);
            if (b<0) b=0;    // forcer pour les stats
            while(_hts_setpause) {    // on fait la pause..
              back_wait(back,back_max,&opt,&cache,HTS_STAT.stat_timestart);

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
                if (opt.errlog) {
                  fspc(opt.errlog,"info"); fprintf(opt.errlog,"Exit requested by shell or user"LF);
                  test_flush;
                }
                exit_xh=1;  // exit requested
                XH_uninit;
                return 0;
              }
              if (back_nsoc(back,back_max)==0)
                Sleep(250);  // tite pause
            }
          }
#endif
          
          // si le fichier n'est pas en backing, le mettre..
          if (!back_exist(back,back_max,urladr,urlfil,savename)) {
#if BDEBUG==1
            printf("crash backing: %s%s\n",liens[ptr]->adr,liens[ptr]->fil);
#endif
            if (back_add(back,back_max,&opt,&cache,urladr,urlfil,savename,liens[liens[ptr]->precedent]->adr,liens[liens[ptr]->precedent]->fil,liens[ptr]->testmode,&liens[ptr]->pass2)==-1) {
              printf("PANIC! : Crash adding error, unexpected error found.. [%d]\n",__LINE__);
#if BDEBUG==1
              printf("error while crash adding\n");
#endif
              if (opt.errlog) {
                fspc(opt.errlog,"error"); fprintf(opt.errlog,"Unexpected backing error for %s%s"LF,urladr,urlfil);
                test_flush;
              } 
              
            }
          }
          
#if BDEBUG==1
          printf("test number of socks\n");
#endif
          
          // ajouter autant de socket qu'on peut ajouter
          n=opt.maxsoc-back_nsoc(back,back_max);
#if BDEBUG==1
          printf("%d sockets available for backing\n",n);
#endif

#if HTS_ANALYSTE
          if ((n>0) && (!_hts_setpause)) {   // si sockets libre et pas en pause, ajouter
#else
          if (n>0) {                         // si sockets libre
#endif
            // remplir autant que l'on peut le cache (backing)
            back_fillmax(back,back_max,&opt,&cache,liens,ptr,numero_passe,lien_tot);
          }
          
          // index du lien actuel
/*
          b=back_index(back,back_max,urladr,urlfil,savename);
          
          if (b>=0) 
*/
          {
            // ------------------------------------------------------------
            // attendre que le fichier actuel soit prêt - BOUCLE D'ATTENTE
            do {

              // index du lien actuel
              b=back_index(back,back_max,urladr,urlfil,savename);
#if BDEBUG==1
              printf("back index %d, waiting\n",b);
#endif
              // Continue to the loop if link still present
              if (b<0)
                continue;
              
              // Receive data
              if (back[b].status>0)
                back_wait(back,back_max,&opt,&cache,HTS_STAT.stat_timestart);
              
              // Continue to the loop if link still present
              b=back_index(back,back_max,urladr,urlfil,savename);
              if (b<0)
                continue;
              
              // And fill the backing stack
              if (back[b].status>0)
                back_fillmax(back,back_max,&opt,&cache,liens,ptr,numero_passe,lien_tot);
              
              // Continue to the loop if link still present
              b=back_index(back,back_max,urladr,urlfil,savename);
              if (b<0)
                continue;

              // autres occupations de HTTrack: statistiques, boucle d'attente, etc.
              if ((opt.makestat) || (opt.maketrack)) {
                TStamp l=time_local();
                if ((int) (l-makestat_time) >= 60) {   
                  if (makestat_fp != NULL) {
                    fspc(makestat_fp,"info");
                    fprintf(makestat_fp,"Rate= %d (/"LLintP") \11NewLinks= %d (/%d)"LF,(int) ((HTS_STAT.HTS_TOTAL_RECV-makestat_total)/(l-makestat_time)), HTS_STAT.HTS_TOTAL_RECV,(int) lien_tot-makestat_lnk,(int) lien_tot);
                    fflush(makestat_fp);
                    makestat_total=HTS_STAT.HTS_TOTAL_RECV;
                    makestat_lnk=lien_tot;
                  }
                  if (maketrack_fp!=NULL) {
                    int i;
                    fspc(maketrack_fp,"info"); fprintf(maketrack_fp,LF);
                    for(i=0;i<back_max;i++) {
                      back_info(back,i,3,maketrack_fp);
                    }
                    fprintf(maketrack_fp,LF);
                    
                  }
                  makestat_time=l;
                }
              }
#if HTS_ANALYSTE
              {
                int i;
                {
                  char* s=hts_cancel_file("");
                  if (strnotempty(s)) {    // fichier à canceller
                    for(i=0;i<back_max;i++) {
                      if ((back[i].status>0)) {
                        if (strcmp(back[i].url_sav,s)==0) {  // ok trouvé
                          if (back[i].status != 1000) {
#if HTS_DEBUG_CLOSESOCK
                            DEBUG_W("user cancel: deletehttp\n");
#endif
                            if (back[i].r.soc!=INVALID_SOCKET) deletehttp(&back[i].r);
                            back[i].r.soc=INVALID_SOCKET;
                            back[i].r.statuscode=-1;
                            strcpy(back[i].r.msg,"Cancelled by User");
                            back[i].status=0;  // terminé
                          } else    // cancel ftp.. flag à 1
                            back[i].stop_ftp = 1;
                        }
                      }
                    }
                    s[0]='\0';
                  }
                }
                
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
                  if (opt.errlog) {
                    fspc(opt.errlog,"info"); fprintf(opt.errlog,"Exit requested by shell or user"LF);
                    test_flush;
                  } 
                  exit_xh=1;  // exit requested
                  XH_uninit;
                  return 0;
                }
              }
              
#endif
#if HTS_POLL
              if ((opt.shell) || (opt.keyboard) || (opt.verbosedisplay) || (!opt.quiet)) {
                TStamp tl;
                info_shell=1;

                /* Toggle with ENTER */
                if (!opt.quiet) {
                  if (check_stdin()) {
                    char com[256];
                    linput(stdin,com,200);
                    if (opt.verbosedisplay==2)
                      opt.verbosedisplay=1;
                    else
                      opt.verbosedisplay=2;
                    /* Info for wrappers */
                    if ( (opt.debug>0) && (opt.log!=NULL) ) {
                      fspc(opt.log,"info"); fprintf(opt.log,"engine: change-options"LF);
                    }
#if HTS_ANALYSTE
                    hts_htmlcheck_chopt(&opt);
#endif
                  }
                }

                /*
                ..useless..
                while (check_stdin()) {  // données disponibles
                  char com[256];
                  com[0]='\0';
                  
                  if (!rcvd) rcvd=1;
                  linput(stdin,com,256);

                  if (strnotempty(com)) {
                    if (strlen(com)<=2) {
                      switch(*com) {
                      case '?': {    // Status?
                        if (back[b].status>0) printf("WAIT\n"); 
                        else printf("READY\n");
                                }
                        break;
                      case 'f': {    // Fichier en attente?
                        if (back[b].status>0) printf("WAIT %s\n",back[b].url_fil); 
                        else printf("READY %s\n",back[b].url_fil);
                                }
                        break;
                      case 'A': case 'F': {    // filters
                        int i;
                        for(i=0;i<filptr;i++) {
                          printf("%s ",filters[i]);
                        }
                        printf("\n");
                                }
                        break;
                      case '#': {    // Afficher statistique sur le nombre de liens, etc
                        switch(*(com+1)) {
                        case 'l': printf("%d\n",lien_tot); break;  // nombre de liens enregistrés
                        case 's': printf("%d\n",back_nsoc(back,back_max)); break;  // nombre de sockets
                        case 'r': printf("%d\n",(int) (HTS_STAT.HTS_TOTAL_RECV/(time_local()-HTS_STAT.stat_timestart))); break;  // taux de transfert
                        }
                                }
                        break;
                      case 'K': if (*(com+1)=='!') {  // Kill
                        XH_uninit;
                        return -1;
                                }
                        break;
                      case 'X': if (*(com+1)=='!') {  // exit
                        exit_xh=1;
                                }
                        break;
                      case 'I': if (*(com+1)=='+') info_shell=1; else info_shell=0;
                        break;
                      }
                      io_flush;
                    } else if (*com=='@') {
                      printf("%s\n",com+1);
                      io_flush;
                    }
                  } 
                  
                }  // while
                */
                tl=time_local();
                
                // générer un message d'infos sur l'état actuel
                if (opt.shell) {    // si shell
                  if ((tl-last_info_shell)>0) {    // toute les 1 sec
                    FILE* fp=stdout;
                    int a=0;
                    last_info_shell=tl;
                    if (fexist(fconcat(opt.path_log,"hts-autopsy"))) {  // débuggage: teste si le robot est vivant
                      // (oui je sais un robot vivant.. mais bon.. il a le droit de vivre lui aussi)
                      // (libérons les robots esclaves de l'internet!)
                      remove(fconcat(opt.path_log,"hts-autopsy"));
                      fp=fopen(fconcat(opt.path_log,"hts-isalive"),"wb");
                      a=1;
                    }
                    if ((info_shell) || a) {
                      int i,j;
                      
                      fprintf(fp,"TIME %d"LF,(int) (tl-HTS_STAT.stat_timestart));
                      fprintf(fp,"TOTAL %d"LF,(int) HTS_STAT.stat_bytes);
                      fprintf(fp,"RATE %d"LF,(int) (HTS_STAT.HTS_TOTAL_RECV/(tl-HTS_STAT.stat_timestart)));
                      fprintf(fp,"SOCKET %d"LF,back_nsoc(back,back_max));
                      fprintf(fp,"LINK %d"LF,lien_tot);
                      {
                        LLint mem=0;
                        for(i=0;i<back_max;i++)
                          if (back[i].r.adr!=NULL)
                            mem+=back[i].r.size;
                          fprintf(fp,"INMEM "LLintP""LF,mem);
                      }
                      for(j=0;j<2;j++) {  // passes pour ready et wait
                        for(i=0;i<back_max;i++) {
                          back_info(back,i,j+1,stdout);    // maketrack_fp a la place de stdout ?? // **
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
              if ((opt.debug>1) && (opt.log!=NULL)) {
                fspc(opt.log,"debug"); fprintf(opt.log,"link #%d is ready, no more on the stack, skipping: %s%s.."LF,ptr,urladr,urlfil);
                test_flush;
              }

              // prochain lien
              // ptr++;

              // Jump to 'continue'
              // This is one of the very very rare cases where goto
              // is acceptable
              // A supplemental flag and if( ) { } would be really messy
              goto jump_if_done;
            }
            

#if HTS_ANALYSTE==2
#else
            //if (!opt.quiet) {  // petite animation
            if (!opt.verbosedisplay) {
              if (!opt.quiet) {
                static int roll=0;  /* static: ok */
                roll=(roll+1)%4;
                printf("%c\x0d",("/-\\|")[roll]);
                fflush(stdout);
              }
            } else if (opt.verbosedisplay==1) {
              if (back[b].r.statuscode==200)
                printf("%d/%d: %s%s ("LLintP" bytes) - OK\33[K\r",ptr,lien_tot,back[b].url_adr,back[b].url_fil,back[b].r.size);
              else
                printf("%d/%d: %s%s ("LLintP" bytes) - %d\33[K\r",ptr,lien_tot,back[b].url_adr,back[b].url_fil,back[b].r.size,back[b].r.statuscode);
              fflush(stdout);
            }
            //}
#endif
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
            memcpy(&r, &(back[b].r), sizeof(htsblk));
            r.location=loc;    // ne PAS copier location!! adresse, pas de buffer
            if (back[b].r.location) 
              strcpy(r.location,back[b].r.location);
            back[b].r.adr=NULL;    // ne pas faire de desalloc ensuite

            // libérer emplacement backing
            back_delete(back,b);
            
            // progression
            if (opt.aff_progress) {
              TStamp tl=time_local();
              if ((tl-HTS_STAT.stat_timestart)>0) {
                char s[32];
                int i=0;
                lastime=tl;
                _CLRSCR; _GOTOXY("1","1");
                printf("Rate=%d B/sec\n",(int) (HTS_STAT.HTS_TOTAL_RECV/(tl-HTS_STAT.stat_timestart)));
                while(i<minimum(back_max,99)) {  // **
                  if (back[i].status>=0) {  // loading..
                    s[0]='\0';
                    if (strlen(back[i].url_fil)>16)
                      strcat(s,back[i].url_fil+strlen(back[i].url_fil)-16);       
                    else
                      strncat(s,back[i].url_fil,16);
                    printf("%s : ",s);
                    
                    printf("[");
                    if (back[i].r.totalsize>0) {
                      int p;
                      int j;
                      p=(int)((back[i].r.size*10)/back[i].r.totalsize);
                      p=minimum(10,p);
                      for(j=0;j<p;j++) printf("*");
                      for(j=0;j<(10-p);j++) printf("-");
                    } else { 
                      printf(LLintP,back[i].r.size);                      
                    }
                    printf("]");
                    
                    //} else if (back[i].status==0) {
                    //  strcpy(s,"ENDED");
                  } 
                  printf("\n");
                  i++;
                }
                io_flush;
              }
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
                } else if (back[i].status==0) {
                  strcpy(s,"ENDED");
                } else 
                  strcpy(s,"   -   ");
                while(strlen(s)<8) strcat(s," ");
                printf("%s",s); io_flush;
                i++;
              }
            }
#endif
            
            
#if BDEBUG==1
            printf("statuscode=%d with %s / msg=%s\n",r.statuscode,r.contenttype,r.msg);
#endif
            
          }
          /*else {
#if BDEBUG==1
            printf("back index error\n");
#endif
          }
          */
          
        }
        // FIN --RECUPERATION LIEN--- 
        // ------------------------------------------------------------



      } else {    // lien vide..
        if (opt.errlog) {
          fspc(opt.errlog,"warning"); fprintf(opt.errlog,"Warning, link #%d empty"LF,ptr); test_flush;
          error=1;
        }         
      }  // test si url existe (non vide!)
      


      // ---tester taille a posteriori---
      // tester r.adr
      if (!error) {
        // erreur, pas de fichier chargé:
        if ((!r.adr) && (r.is_write==0) 
          && (r.statuscode!=301) 
          && (r.statuscode!=302) 
          && (r.statuscode!=303) 
          && (r.statuscode!=307) 
          && (r.statuscode!=412)
          && (r.statuscode!=416)
         ) { 
          // error=1;
          
          // peut être que le fichier était trop gros?
          if ((istoobig(r.totalsize,opt.maxfile_html,opt.maxfile_nonhtml,r.contenttype))
           || (istoobig(r.totalsize,opt.maxfile_html,opt.maxfile_nonhtml,r.contenttype))) {
            error=0;
            if (opt.errlog) {
              fspc(opt.errlog,"warning"); fprintf(opt.errlog,"Big file cancelled according to user's preferences: %s%s"LF,urladr,urlfil);
              test_flush;
            }
          }
          // // // error=1;    // ne pas traiter la suite -- euhh si finalement..
        }
      }
      // ---fin tester taille a posteriori---    


      // -------------------- 
      // BOGUS MIME TYPE HACK
      // Check if we have a bogus MIME type
      // example: 
      // Content-type="text/html"
      // and 
      // Content-disposition="foo.jpg"
      // --------------------
      if (!error) {
        if (r.statuscode == 200) {    // OK (ou 304 en backing)
          if (r.adr) {    // Written file
            if ( (is_hypertext_mime(r.contenttype))   /* Is HTML or Js, .. */
              || (may_be_hypertext_mime(r.contenttype) && (r.adr) )  /* Is real media, .. */
              ) {
              if (strnotempty(r.cdispo)) {      // Content-disposition set!
                if (ishtml(savename) == 0) {    // Non HTML!!
                  // patch it!
                  strcpy(r.contenttype,"application/octet-stream");
                }
              }
            }
          }
        }
      }
      
      // ------------------------------------
      // BOGUS MIME TYPE HACK II (the revenge)
      // Check if we have a bogus MIME type
      if ( (is_hypertext_mime(r.contenttype))   /* Is HTML or Js, .. */
        || (may_be_hypertext_mime(r.contenttype))  /* Is real media, .. */
        ) {
        if ((r.adr) && (r.size)) {
          unsigned int map[256];
          int i;
          unsigned int nspec = 0;
          map_characters((unsigned char*)r.adr, (unsigned int)r.size, (unsigned int*)map);
          for(i = 1 ; i < 32 ; i++) {   //  null chars ignored..
            if (!is_realspace(i) 
              && i != 27        /* Damn you ISO2022-xx! */
              ) {
              nspec += map[i];
            }
          }
          if ((nspec > r.size / 100) && (nspec > 10)) {    // too many special characters
            strcpy(r.contenttype,"application/octet-stream");
            if (opt.errlog) {
              fspc(opt.errlog,"warning"); fprintf(opt.errlog,"File not parsed, looks like binary: %s%s"LF,urladr,urlfil);
              test_flush;
            }
          }
        }
      }
      
      // -------------------- 
      // REAL MEDIA HACK
      // Check if we have to load locally the file
      // --------------------
      if (!error) {
        if (r.statuscode == 200) {    // OK (ou 304 en backing)
          if (r.adr==NULL) {    // Written file
            if (may_be_hypertext_mime(r.contenttype)) {   // to parse!
              LLint sz;
              sz=fsize(savename);
              if (sz>0) {   // ok, exists!
                if (sz < 1024) {   // ok, small file --> to parse!
                  FILE* fp=fopen(savename,"rb");
                  if (fp) {
                    r.adr=malloct((int)sz + 2);
                    if (r.adr) {
                      fread(r.adr,(int)sz,1,fp);
                      r.size=sz;
                      fclose(fp);
                      fp=NULL;
                      // remove (temporary) file!
                      remove(savename);
                    }
                    if (fp)
                      fclose(fp);
                  }
                }
              }
            }
          }
        }
      }
      // EN OF REAL MEDIA HACK
      

      // ---stockage en cache---
      // stocker dans le cache?
      /*
      if (!error) {
        if (ptr>0) {
          if (liens[ptr]) {
            cache_mayadd(&opt,&cache,&r,urladr,urlfil,savename);
          } else
            error=1;
        }
      }
      */
      // ---fin stockage en cache---
      


      // DEBUT rattrapage des 301,302,307..
      // ------------------------------------------------------------
      if (!error) {
        ////////{
        // on a chargé un fichier en plus
        // if (!error) stat_loaded+=r.size;
        
        // ------------------------------------------------------------
        // Rattrapage des 301,302,307 (moved) et 412,416 - les 304 le sont dans le backing 
        // ------------------------------------------------------------
        if ( (r.statuscode==301) 
          || (r.statuscode==302)
          || (r.statuscode==303)
          || (r.statuscode==307)
          ) {          
          //if (r.adr!=NULL) {   // adr==null si fichier direct. [catch: davename normalement si cgi]
          //int i=0;
          char *rn=NULL;
          // char* p;
          
          if ( (opt.debug>0) && (opt.errlog!=NULL) ) {
            //if (opt.errlog) {
            fspc(opt.errlog,"warning"); fprintf(opt.errlog,"%s for %s%s"LF,r.msg,urladr,urlfil);
            test_flush;
          }
          
          
          {
            char mov_url[HTS_URLMAXSIZE*2],mov_adr[HTS_URLMAXSIZE*2],mov_fil[HTS_URLMAXSIZE*2];
            int get_it=0;         // ne pas prendre le fichier à la même adresse par défaut
            int reponse=0;
            mov_url[0]='\0'; mov_adr[0]='\0'; mov_fil[0]='\0';
            //
            
            strcpy(mov_url,r.location);
            
            // url qque -> adresse+fichier
            if ((reponse=ident_url_relatif(mov_url,urladr,urlfil,mov_adr,mov_fil))>=0) {                        
              int set_prio_to=0;    // pas de priotité fixéd par wizard
              
              //if (ident_url_absolute(mov_url,mov_adr,mov_fil)!=-1) {    // ok URL reconnue
              // c'est (en gros) la même URL..
              // si c'est un problème de casse dans le host c'est que le serveur est buggé
              // ("RFC says.." : host name IS case insensitive)
              if ((strfield2(mov_adr,urladr)!=0) && (strfield2(mov_fil,urlfil)!=0)) {  // identique à casse près
                // on tourne en rond
                if (strcmp(mov_fil,urlfil)==0) {
                  error=1;
                  get_it=-1;        // ne rien faire
                  if (opt.errlog) {
                    fspc(opt.errlog,"warning"); fprintf(opt.errlog,"Can not bear crazy server (%s) for %s%s"LF,r.msg,urladr,urlfil);
                    test_flush;
                  }
                } else {    // mauvaise casse, effacer entrée dans la pile et rejouer une fois
                  get_it=1;
                }
              } else {        // adresse différente
                if (ishtml(mov_url)==0) {   // pas même adresse MAIS c'est un fichier non html (pas de page moved possible)
                  // -> on prend à cette adresse, le lien sera enregistré avec lien_record() (hash)
                  if ((opt.debug>1) && (opt.log!=NULL)) {
                    fspc(opt.log,"debug"); fprintf(opt.log,"wizard link test for moved file at %s%s.."LF,mov_adr,mov_fil);
                    test_flush;
                  }
                  // accepté?
                  if (hts_acceptlink(&opt,ptr,lien_tot,liens,
                    mov_adr,mov_fil,
                    &filters,&filptr,opt.maxfilter,
                    &robots,
                    &set_prio_to,
                    NULL) != 1) {                /* nouvelle adresse non refusée ? */
                    get_it=1;
                    if ((opt.debug>1) && (opt.log!=NULL)) {
                      fspc(opt.log,"debug"); fprintf(opt.log,"moved link accepted: %s%s"LF,mov_adr,mov_fil);
                      test_flush;
                    }
                  }
                } /* sinon traité normalement */
              }
              
              //if ((strfield2(mov_adr,urladr)!=0) && (strfield2(mov_fil,urlfil)!=0)) {  // identique à casse près
              if (get_it==1) {
                // court-circuiter le reste du traitement
                // et reculer pour mieux sauter
                if (opt.errlog) {
                  fspc(opt.errlog,"warning"); fprintf(opt.errlog,"Warning moved treated for %s%s (real one is %s%s)"LF,urladr,urlfil,mov_adr,mov_fil);
                  test_flush;
                }          
                // canceller lien actuel
                error=1;
                strcpy(liens[ptr]->adr,"!");  // caractère bidon (invalide hash)
#if HTS_HASH
#else
                liens[ptr]->sav_len=-1;       // taille invalide
#endif
                // noter NOUVEAU lien
                {
                  char mov_sav[HTS_URLMAXSIZE*2];
                  // calculer lien et éventuellement modifier addresse/fichier
                  if (url_savename(mov_adr,mov_fil,mov_sav,NULL,NULL,liens[liens[ptr]->precedent]->adr,liens[liens[ptr]->precedent]->fil,&opt,liens,lien_tot,back,back_max,&cache,&hash,ptr,numero_passe)!=-1) { 
                    if (hash_read(&hash,mov_sav,"",0)<0) {      // n'existe pas déja
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
                        if (opt.errlog) {
                          fprintf(opt.errlog,"Not enough memory, can not re-allocate %d bytes"LF,(int)((add_tab_alloc+1)*sizeof(lien_url)));
                          test_flush;
                        }
                        //if (opt.getmode & 1) { if (fp) { fclose(fp); fp=NULL; } }
                        XH_uninit;    // désallocation mémoire & buffers
                        return 0;
                      }
                    } else {
                      if ( (opt.debug>0) && (opt.errlog!=NULL) ) {
                        fspc(opt.errlog,"warning"); fprintf(opt.errlog,"moving %s to an existing file %s"LF,liens[ptr]->fil,urlfil);
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
                if (opt.errlog) {
                  fspc(opt.errlog,"warning"); fprintf(opt.errlog,"File has moved from %s%s to %s"LF,urladr,urlfil,mov_url);
                  test_flush;
                }
                escape_uri(mov_url);
                // On prépare une page qui sautera immédiatement sur la bonne URL
                // Le scanner re-changera, ensuite, cette URL, pour la mirrorer!
                strcpy(rn,"<HTML>"CRLF);
                strcat(rn,"<!-- Created by HTTrack Website Copier/"HTTRACK_VERSION" "HTTRACK_AFF_AUTHORS" -->"CRLF);
                strcat(rn,"<HEAD>"CRLF"<TITLE>Page has moved</TITLE>"CRLF"</HEAD>"CRLF"<BODY>"CRLF);
                strcat(rn,"<META HTTP-EQUIV=\"Refresh\" CONTENT=\"0; URL=");
                strcat(rn,mov_url);    // URL
                strcat(rn,"\">"CRLF);
                strcat(rn,"<A HREF=\"");
                strcat(rn,mov_url);
                strcat(rn,"\">");
                strcat(rn,"<B>Click here...</B></A>"CRLF);
                strcat(rn,"</BODY>"CRLF);
                strcat(rn,"<!-- Created by HTTrack Website Copier/"HTTRACK_VERSION" "HTTRACK_AFF_AUTHORS" -->"CRLF);
                strcat(rn,"</HTML>"CRLF);
                
                // changer la page
                if (r.adr) { freet(r.adr); r.adr=NULL; }
                r.adr=rn;
                r.size=strlen(r.adr);
                strcpy(r.contenttype,"text/html");
              }
            }  // get_it==0
            
          }     // bloc
        // erreur HTTP (ex: 404, not found)
        } else if (
             (r.statuscode==412)
          || (r.statuscode==416)
          ) {    // Precondition Failed, c'est à dire pour nous redemander TOUT le fichier
          if (fexist(liens[ptr]->sav)) {
            remove(liens[ptr]->sav);    // Eliminer
            if (!fexist(liens[ptr]->sav)) {  // Bien éliminé? (sinon on boucle..)
#if HDEBUG
              printf("Partial content NOT up-to-date, reget all file for %s\n",liens[ptr]->sav);
#endif
              if ( (opt.debug>1) && (opt.errlog!=NULL) ) {
                //if (opt.errlog) {
                fspc(opt.errlog,"debug"); fprintf(opt.errlog,"Partial file reget (%s) for %s%s"LF,r.msg,urladr,urlfil);
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
                strcpy(liens[ptr]->adr,"!");  // caractère bidon (invalide hash)
#if HTS_HASH
#else
                liens[ptr]->sav_len=-1;       // taille invalide
#endif
                //
              } else {  // oups erreur, plus de mémoire!!
                printf("PANIC! : Not enough memory [%d]\n",__LINE__);
                if (opt.errlog) {
                  fprintf(opt.errlog,"Not enough memory, can not re-allocate %d bytes"LF,(int)((add_tab_alloc+1)*sizeof(lien_url)));
                  test_flush;
                }
                //if (opt.getmode & 1) { if (fp) { fclose(fp); fp=NULL; } }
                XH_uninit;    // désallocation mémoire & buffers
                return 0;
              } 
            } else {
              if (opt.errlog!=NULL) {
                fspc(opt.errlog,"error"); fprintf(opt.errlog,"Can not remove old file %s"LF,urlfil);
                test_flush;
              }
            }
          } else {
            if (opt.errlog!=NULL) {
              fspc(opt.errlog,"warning"); fprintf(opt.errlog,"Unexpected 412/416 error (%s) for %s%s"LF,r.msg,urladr,urlfil);
              test_flush;
            }
          }
        } else if (r.statuscode!=200) {
          int can_retry=0;
          
          // cas où l'on peut reessayer
          // -2=timeout -3=rateout (interne à httrack)
          switch(r.statuscode) {
            //case -1: can_retry=1; break;
          case -2: if (opt.hostcontrol) {    // timeout et retry épuisés
            if ((opt.hostcontrol & 1) && (liens[ptr]->retry<=0)) {
              if ((opt.debug>1) && (opt.log!=NULL)) {
                fspc(opt.log,"debug"); fprintf(opt.log,"Link banned: %s%s"LF,urladr,urlfil); test_flush;
              }
              host_ban(&opt,liens,ptr,lien_tot,back,back_max,filters,opt.maxfilter,&filptr,jump_identification(urladr));
              if ((opt.debug>1) && (opt.log!=NULL)) {
                fspc(opt.log,"debug"); fprintf(opt.log,"Info: previous log - link banned: %s%s"LF,urladr,urlfil); test_flush;
              }
            } else can_retry=1;
                   } else can_retry=1;
            break;
          case -3: if ((opt.hostcontrol) && (liens[ptr]->retry<=0)) {    // too slow
            if (opt.hostcontrol & 2) {
              if ((opt.debug>1) && (opt.log!=NULL)) {
                fspc(opt.log,"debug"); fprintf(opt.log,"Link banned: %s%s"LF,urladr,urlfil); test_flush;
              }
              host_ban(&opt,liens,ptr,lien_tot,back,back_max,filters,opt.maxfilter,&filptr,jump_identification(urladr));
              if ((opt.debug>1) && (opt.log!=NULL)) {
                fspc(opt.log,"debug"); fprintf(opt.log,"Info: previous log - link banned: %s%s"LF,urladr,urlfil); test_flush;
              }
            } else can_retry=1;
                   } else can_retry=1;
            break;
          case -4:            // connect closed
            can_retry=1;
            break;
          case -5:            // other (non fatal) error
            can_retry=1;
            break;
          case -6:            // bad SSL handskake
            can_retry=1;
            break;
          case 408: case 409: case 500: case 502: case 504: can_retry=1;
            break;
          }
          
          if ( strcmp(liens[ptr]->fil,"/primary") != 0 ) {  // no primary (internal page 0)
            if ((liens[ptr]->retry<=0) || (!can_retry) ) {  // retry épuisés (ou retry impossible)
              if (opt.errlog) {
                if ((opt.retry>0) && (can_retry)){
                  fspc(opt.errlog,"error"); 
                  fprintf(opt.errlog,"\"%s\" (%d) after %d retries at link %s%s (from %s%s)"LF,r.msg,r.statuscode,opt.retry,urladr,urlfil,liens[liens[ptr]->precedent]->adr,liens[liens[ptr]->precedent]->fil);
                } else {
                  if (r.statuscode==-10) {    // test OK
                    if ((opt.debug>0) && (opt.errlog!=NULL)) {
                      fspc(opt.errlog,"info"); 
                      fprintf(opt.errlog,"Test OK at link %s%s (from %s%s)"LF,urladr,urlfil,liens[liens[ptr]->precedent]->adr,liens[liens[ptr]->precedent]->fil);
                    }
                  } else {
                    if (strcmp(urlfil,"/robots.txt")) {       // ne pas afficher d'infos sur robots.txt par défaut
                      fspc(opt.errlog,"error"); 
                      fprintf(opt.errlog,"\"%s\" (%d) at link %s%s (from %s%s)"LF,r.msg,r.statuscode,urladr,urlfil,liens[liens[ptr]->precedent]->adr,liens[liens[ptr]->precedent]->fil);
                    } else {
                      if (opt.debug>1) {
                        fspc(opt.errlog,"info"); fprintf(opt.errlog,"No robots.txt rules at %s"LF,urladr);
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
                if (opt.errpage) {
                  store_errpage=1;
                }
              } else {
                if (strcmp(urlfil,"/robots.txt") != 0) {
                  /*
                    This is an error caused by a link entered by the user
                    That is, link(s) entered by user are invalid (404, 500, connect error, proxy error..)
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
              if (opt.debug>0 && opt.errlog != NULL) {  // on fera un alert si le retry échoue               
                fspc(opt.errlog,"warning"); fprintf(opt.errlog,"Retry after error %d (%s) at link %s%s (from %s%s)"LF,r.statuscode,r.msg,urladr,urlfil,liens[liens[ptr]->precedent]->adr,liens[liens[ptr]->precedent]->fil);
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
                if (opt.errlog) {
                  fspc(opt.errlog,"panic"); 
                  fprintf(opt.errlog,"Not enough memory, can not re-allocate %d bytes"LF,(int)((add_tab_alloc+1)*sizeof(lien_url)));
                  test_flush;
                }
                //if (opt.getmode & 1) { if (fp) { fclose(fp); fp=NULL; } }
                XH_uninit;    // désallocation mémoire & buffers
                return 0;
              } 
            }
          } else {
            if (opt.errlog) {
              if (opt.debug>1) {
                fspc(opt.errlog,"info"); 
                fprintf(opt.errlog,"Info: no robots.txt at %s%s"LF,urladr,urlfil);
              }
            }
          }
          if (!store_errpage) {
            if (r.adr) { freet(r.adr); r.adr=NULL; }  // désalloc
            error=1;  // erreur!
          }
        }
        // FIN rattrapage des 301,302,307..
        // ------------------------------------------------------------
        
        

      }  // if !error
    }  // if !error
    
    if (!error) {
#if DEBUG_SHOWTYPES
      if (strstr(REG,r.contenttype)==NULL) {
        strcat(REG,r.contenttype);
        strcat(REG,"\n");
        printf("%s\n",r.contenttype);
        io_flush;
      }
#endif
      

      // ------------------------------------------------------
      // ok, fichier chargé localement
      // ------------------------------------------------------
      
      // Vérificateur d'intégrité
      #if DEBUG_CHECKINT
      {
        int i;
        for(i=0;i<back_max;i++) {
          char si[256];
          sprintf(si,"Test global après back_wait, index %d",i);
          _CHECKINT(&back[i],si)
        }
      }
      #endif


      /* info: updated */
      /*
      if (ptr>0) {
        // "mis à jour"
        if ((!r.notmodified) && (opt.is_update) && (!store_errpage)) {    // page modifiée
          if (strnotempty(savename)) {
            HTS_STAT.stat_updated_files++;
            if (opt.log!=NULL) {
              //if ((opt.debug>0) && (opt.log!=NULL)) {
              fspc(opt.log,"info"); fprintf(opt.log,"File updated: %s%s"LF,urladr,urlfil);
              test_flush;
            }
          }
        } else {
          if (!store_errpage) {
            if ( (opt.debug>0) && (opt.log!=NULL) ) {
              fspc(opt.log,"info"); fprintf(opt.log,"File recorded: %s%s"LF,urladr,urlfil);
              test_flush;
            }
          }
        }
      }
      */
      
      // ------------------------------------------------------
      // traitement (parsing)
      // ------------------------------------------------------

      // traiter
      if (
           ( (is_hypertext_mime(r.contenttype))   /* Is HTML or Js, .. */
             || (may_be_hypertext_mime(r.contenttype) && (r.adr) )  /* Is real media, .. */
           )
        && (liens[ptr]->depth>0)            /* Depth > 0 (recurse depth) */
        && (r.adr!=NULL)        /* HTML Data exists */
        && (r.size>0)           /* And not empty */
        && (!store_errpage)     /* Not an html error page */
        && (savename[0]!='\0')  /* Output filename exists */
        ) {    // ne traiter que le html si autorisé
        // -- -- -- --
        // Parsing HTML
        if (!error) {
          /* Info for wrappers */
          if ( (opt.debug>0) && (opt.log!=NULL) ) {
            fspc(opt.log,"info"); fprintf(opt.log,"engine: check-html: %s%s"LF,urladr,urlfil);
          }
          {
          // I'll have to segment this part
#include "htsparse.c"
          }
        }
        // Fin parsing HTML
        // -- -- -- --


      }  // si text/html
      // -- -- --
      else {    // sauver fichier quelconque
        // -- -- --
        // sauver fichier


        /* En cas d'erreur, vérifier que fichier d'erreur existe */
        if (strnotempty(savename) == 0) {           // chemin de sauvegarde existant
          if (strcmp(urlfil,"/robots.txt")==0) {    // pas robots.txt
            if (store_errpage) {                    // c'est une page d'erreur
              int create_html_warning=0;
              int create_gif_warning=0;
              switch (ishtml(urlfil)) {      /* pas fichier html */
              case 0:                        /* non html */
                {
                  char buff[256];
                  guess_httptype(buff,urlfil);
                  if (strcmp(buff,"image/gif")==0)
                    create_gif_warning=1;
                }
                break;
              case 1:                        /* html */
                if (!r.adr) {
                }
                break;
              default:                       /* don't know.. */
                break;    
              }
              /* Créer message d'erreur ? */
              if (create_html_warning) {
                char* adr=(char*)malloct(strlen(HTS_DATA_ERROR_HTML)+1100);
                if ( (opt.debug>0) && (opt.log!=NULL) ) {
                  fspc(opt.log,"info"); fprintf(opt.log,"Creating HTML warning file (%s)"LF,r.msg);
                  test_flush;
                }
                if (adr) {
                  if (r.adr) {
                    freet(r.adr);
                    r.adr=NULL;
                  }
                  sprintf(adr,HTS_DATA_ERROR_HTML,r.msg);
                  r.adr=adr;
                }
              } else if (create_gif_warning) {
                char* adr=(char*)malloct(HTS_DATA_UNKNOWN_GIF_LEN);
                if ( (opt.debug>0) && (opt.log!=NULL) ) {
                  fspc(opt.log,"info"); fprintf(opt.log,"Creating GIF dummy file (%s)"LF,r.msg);
                  test_flush;
                }
                if (r.adr) {
                  freet(r.adr);
                  r.adr=NULL;
                }
                memcpy(adr, HTS_DATA_UNKNOWN_GIF, HTS_DATA_UNKNOWN_GIF_LEN);
                r.adr=adr;
              }
            }
          }
        }

        if (strnotempty(savename) == 0) {    // pas de chemin de sauvegarde
          if (strcmp(urlfil,"/robots.txt")==0) {    // robots.txt
            if (r.adr) {
              int bptr=0;
              char line[1024];
              char buff[8192];
              char infobuff[8192];
              int record=0;
              line[0]='\0'; buff[0]='\0'; infobuff[0]='\0';
              //
#if DEBUG_ROBOTS
              printf("robots.txt dump:\n%s\n",r.adr);
#endif
              do {
                bptr+=binput(r.adr+bptr, line, sizeof(line) - 2);
                if (strfield(line,"user-agent:")) {
                  char* a;
                  a=line+11;
                  while(*a==' ') a++;    // sauter espace(s)
                  if (*a == '*') {
                    if (record != 2)
                      record=1;    // c pour nous
                  } else if (strfield(a,"httrack")) {
                    buff[0]='\0';      // re-enregistrer
                    infobuff[0]='\0';
                    record=2;          // locked
#if DEBUG_ROBOTS
                    printf("explicit disallow for httrack\n");
#endif
                  }
                  else record=0;
                } else if (record) {
                  if (strfield(line,"disallow:")) {
                    char* a;
                    a=strchr(line,'#');
                    if (a) *a='\0';
                    while((line[strlen(line)-1]==' ') 
                      || (line[strlen(line)-1]==10) 
                      || (line[strlen(line)-1]==13))
                      line[strlen(line)-1]='\0';   // supprimer espaces
                    a=line+9;
                    while((*a==' ') || (*a==10) || (*a==13))
                      a++;    // sauter espace(s)
                    if (strnotempty(a)) {
                      if (strcmp(a,"/") != 0) {      /* ignoring disallow: / */
                        if ( (strlen(buff) + strlen(a) + 8) < sizeof(buff)) {
                          strcat(buff,a);
                          strcat(buff,"\n");
                          if (strnotempty(infobuff)) strcat(infobuff,", ");
                          strcat(infobuff,a);
                        }
                      } else {
                        if (opt.errlog!=NULL) {
                          fspc(opt.errlog,"info"); fprintf(opt.errlog,"Note: %s robots.txt rules are too restrictive, ignoring /"LF,urladr);
                          test_flush;
                        }
                      }
                    }
                  }
                }
              } while( (bptr<r.size) && (strlen(buff) < (sizeof(buff) - 32) ) );
              if (strnotempty(buff)) {
                checkrobots_set(&robots,urladr,buff);
                if (opt.log!=NULL) {
                  if (opt.log != opt.errlog) {
                    fspc(opt.log,"info"); fprintf(opt.log,"Note: robots.txt forbidden links for %s are: %s"LF,urladr,infobuff);
                    test_flush;
                  } 
                }
                if (opt.errlog!=NULL) {
                  fspc(opt.errlog,"info"); fprintf(opt.errlog,"Note: due to %s remote robots.txt rules, links begining with these path will be forbidden: %s (see in the options to disable this)"LF,urladr,infobuff);
                  test_flush;
                }
              }
            }
          }
        } else if (r.is_write) {    // déja sauvé sur disque
          /*
          if (!ishttperror(r.statuscode))
            HTS_STAT.stat_files++;
          HTS_STAT.stat_bytes+=r.size;
          */
          //printf("ok......\n");
        } else {
          // Si on doit sauver une page HTML sans la scanner, cela signifie que le niveau de
          // récursion nous en empêche
          // Dans ce cas on met un fichier indiquant ce fait
          // Si par la suite on doit retraiter ce fichier avec un niveau de récursion plus
          // fort, on supprimera le readme, et on scannera le fichier html!
          // note: sauté si store_errpage (càd si page d'erreur, non à scanner!)
          if ( (is_hypertext_mime(r.contenttype)) && (!store_errpage) && (r.size>0)) {  // c'est du html!!
            char tempo[HTS_URLMAXSIZE*2];
            FILE* fp;
            tempo[0]='\0';
            strcpy(tempo,savename);
            strcat(tempo,".readme");
            
#if HTS_DOSNAME
            // remplacer / par des slash arrière
            {
              int i=0;
              while(tempo[i]) {
                if (tempo[i]=='/')
                  tempo[i]='\\';
                i++;
              } 
            } 
            // a partir d'ici le slash devient antislash
#endif
            
            if ((fp=fopen(tempo,"wb"))!=NULL) {
              fprintf(fp,"Info-file generated by HTTrack Website Copier "HTTRACK_VERSION""CRLF""CRLF);
              fprintf(fp,"The file %s has not been scanned by HTS"CRLF,savename);
              fprintf(fp,"Some links contained in it may be unreachable locally."CRLF);
              fprintf(fp,"If you want to get these files, you have to set an upper recurse level, ");
              fprintf(fp,"and to rescan the URL."CRLF);
              fclose(fp);
#if HTS_WIN==0
              chmod(tempo,HTS_ACCESS_FILE);      
#endif
              usercommand(0,NULL,antislash(tempo));
            }
            
            
            if ( (opt.debug>0) && (opt.errlog!=NULL) ) {
              fspc(opt.errlog,"warning"); fprintf(opt.errlog,"Warning: store %s without scan: %s"LF,r.contenttype,savename);
              test_flush;
            }
          } else {
            if ((opt.getmode & 2)!=0) {    // ok autorisé
              if ( (opt.debug>1) && (opt.log!=NULL) ) {
                fspc(opt.log,"debug"); fprintf(opt.log,"Store %s: %s"LF,r.contenttype,savename);
                test_flush;
              }
            } else {    // lien non autorisé! (ex: cgi-bin en html)
              if ((opt.debug>1) && (opt.log!=NULL)) {
                fspc(opt.log,"debug"); fprintf(opt.log,"non-html file ignored after upload at %s : %s"LF,urladr,urlfil);
                test_flush;
              } 
              freet(r.adr); r.adr=NULL;
            }
          }
          
          //printf("extern=%s\n",r.contenttype);

          // ATTENTION C'EST ICI QU'ON SAUVE LE FICHIER!!          
          if (r.adr) {
            if (filesave(r.adr,(int)r.size,savename)!=0) {
              if (opt.errlog) {   
                fspc(opt.errlog,"error"); fprintf(opt.errlog,"Unable to save file %s"LF,savename);
                test_flush;
              }
            } else {
              /*
              if (!ishttperror(r.statuscode))
                HTS_STAT.stat_files++;
              HTS_STAT.stat_bytes+=r.size;
              */
            }
          }
          
        }
  

        /* Parsing of other media types (java, ram..) */
        /*
        if (strfield2(r.contenttype,"audio/x-pn-realaudio")) {
          if ((opt.debug>1) && (opt.log!=NULL)) {
            fspc(opt.log,"debug"); fprintf(opt.log,"(Real Media): parsing %s"LF,savename); test_flush;
          }
          if (fexist(savename)) {   // ok, existe bien!
            FILE* fp=fopen(savename,"r+b");
            if (fp) {
              if (!fseek(fp,0,SEEK_SET)) {
                char line[HTS_URLMAXSIZE*2];
                linput(fp,line,HTS_URLMAXSIZE);
                if (strnotempty(line)) {
                  if ((opt.debug>1) && (opt.log!=NULL)) {
                    fspc(opt.log,"debug"); fprintf(opt.log,"(Real Media): detected %s"LF,line); test_flush;
                  }
                }
              }
              fclose(fp);
            }
          }
        } else */
        if (opt.parsejava) {
          if (strlen(savename)>6) {  // fichier.class
            if (strfield(savename+strlen(savename)-6,".class")) {  // ok c'est une classe
              if (fexist(savename)) {   // ok, existe bien!
                char err_msg[1100];
                int r;
                err_msg[0]='\0';
                
                //##char* buffer;
                // JavaParsing f34R!
                if ((opt.debug>1) && (opt.log!=NULL)) {
                  fspc(opt.log,"debug"); fprintf(opt.log,"(JavaClass catch file): parsing %s"LF,savename); test_flush;
                }
                
                //##buffer=(char*) malloct(32768);
                //##if (buffer) {
                //
                //##strcpy(buffer,"$BUFFER$");
                //##hts_add_file(buffer);    // déclarer buffer
                while(hts_add_file(NULL,-1) >= 0);   // clear chain
                
                r=hts_parse_java(savename,(char*) &err_msg);  // parsing
                if (!r) {    // error
                  if (opt.errlog) {   
                    fspc(opt.errlog,"error"); fprintf(opt.errlog,"Unable to parse java file %s : %s"LF,savename,err_msg);
                    test_flush;
                  }
                } else {  // ok
                  char adr[HTS_URLMAXSIZE*2],fil[HTS_URLMAXSIZE*2],save[HTS_URLMAXSIZE*2];  // nom du fichier à sauver dans la boucle
                  char codebase[HTS_URLMAXSIZE*2];                  // codebase classe java
                  char lien[HTS_URLMAXSIZE*2];
                  //##char* a;
                  int file_position;
                  int pass_fix,prio_fix;
                  codebase[0]='\0';
                  //
                  
                  if ((opt.debug>1) && (opt.log!=NULL)) {
                    fspc(opt.log,"debug"); fprintf(opt.log,"(JavaClass catch file): parsing finished, now copying links.."LF); test_flush;
                  }
                  // recopie de "creer le lien"
                  //
                  
                  // adr = c'est la même
                  // fil et save: save2 et fil2
                  prio_fix=maximum(liens[ptr]->depth-1,0);
                  pass_fix=max(liens[ptr]->pass2,numero_passe);
                  if (liens[ptr]->cod) strcpy(codebase,liens[ptr]->cod);       // codebase valable pour tt les classes descendantes
                  if (strnotempty(codebase)==0) {    // pas de codebase, construire
                    char* a;
                    strcpy(codebase,liens[ptr]->fil);
                    a=codebase+strlen(codebase)-1;
                    while((*a) && (*a!='/') && ( a > codebase)) a--;
                    if (*a=='/')
                      *(a+1)='\0';    // couper
                  } else {    // couper http:// éventuel
                    if (strfield(codebase,"http://")) {
                      char tempo[HTS_URLMAXSIZE*2];
                      char* a=codebase+7;
                      a=strchr(a,'/');    // après host
                      if (a) {  // ** msg erreur et vérifier?
                        strcpy(tempo,a);
                        strcpy(codebase,tempo);    // couper host
                      } else {
                        if (opt.errlog) {   
                          fprintf(opt.errlog,"Unexpected strstr error in base %s"LF,codebase);
                          test_flush;
                        }
                      }
                    }
                  }
                  //##a=buffer;
                  //##strcat(buffer,"&");  // fin du buffer
                  if (!((int) strlen(codebase)<HTS_URLMAXSIZE)) {    // trop long
                    if (opt.errlog) {   
                      fprintf(opt.errlog,"Codebase too long, parsing skipped (%s)"LF,codebase);
                      test_flush;
                    }
                    //##a=NULL;
                    while(hts_add_file(NULL,-1) >= 0);   // clear chain
                  }
                  while ( (file_position=hts_add_file(lien,-1)) >= 0 ) {
                    int dejafait=0;
                    /* //##
                    char* b;
                    
                     // prochain fichier à noter!
                     lien[0]='\0';
                     b=strchr(a,'&');  // marqueur de fin de chaine (voir hts_add_file)
                     if (b) {
                     if ( ( ((int) b-(int) a) + strlen(codebase)) < HTS_URLMAXSIZE)
                     strncat(lien,a,(int) b-(int) a);    // nom du fichier
                     else {
                     if (opt.errlog) {   
                     fprintf(opt.errlog,"Error: Java-Parser generated link that exceeds %d bytes"LF,HTS_URLMAXSIZE);
                     test_flush;
                     }
                     }
                     } else a=NULL;
                     
                      if (strnotempty(lien)==0) a=NULL;  // fin
                      if (a)
                      a=b+1;
                    */
                    
                    if (strnotempty(lien)) {
                      
                      // calculer les chemins et noms de sauvegarde
                      if (ident_url_relatif(lien,urladr,codebase,adr,fil)>=0) { // reformage selon chemin
                        int r;
                        
                        //  patcher opt pour garder structure originale!! (on ne patche pas les noms dans la classe java!)
                        //##if (!strstr(lien,"://")) {         // PAS tester les http://.. inutile (on ne va pas patcher le binaire :-( )
                        if (1) {
                          char tempo[HTS_URLMAXSIZE*2];
                          int a,b;
                          tempo[0]='\0';
                          a=opt.savename_type;
                          b=opt.savename_83;
                          opt.savename_type=0;
                          opt.savename_83=0;
                          // note: adr,fil peuvent être patchés
                          r=url_savename(adr,fil,save,NULL,NULL,NULL,NULL,&opt,liens,lien_tot,back,back_max,&cache,&hash,ptr,numero_passe);
                          opt.savename_type=a;
                          opt.savename_83=b;
                          if (r != -1) {
                            if (savename) {
                              if (lienrelatif(tempo,save,savename)==0) {
                                if ((opt.debug>1) && (opt.log!=NULL)) {
                                  fspc(opt.log,"debug"); fprintf(opt.log,"(JavaClass catch file): relative link at %s build with %s and %s: %s"LF,adr,save,savename,tempo);
                                  test_flush;
                                }
                                //
                                // xxc xxc xxc xxc TODO java:
                                // rebuild the java class with patched strings...
                                //
                                if (strlen(tempo)<=strlen(lien)) {
                                  FILE* fp=fopen(savename,"r+b");
                                  if (fp) {
                                    if (!fseek(fp,file_position,SEEK_SET)) {
                                      //unsigned short int string_length=strlen(tempo);
                                      //fwrite(&valint,sizeof(string_length),1,fp);
                                      // xxc xxc ARGH! SI la taille est <, décaler le code ?!
                                    } else {
                                      if (opt.log!=NULL) {
                                        fspc(opt.log,"debug"); fprintf(opt.log,"(JavaClass catch file): unable to patch: %s"LF,savename);
                                        test_flush;
                                      }
                                    }
                                    fclose(fp);
                                  } else {
                                    if (opt.log!=NULL) {
                                      fspc(opt.log,"debug"); fprintf(opt.log,"(JavaClass catch file): unable to open: %s"LF,savename);
                                      test_flush;
                                    }
                                  }
                                } else {
                                  if (opt.log!=NULL) {
                                    fspc(opt.log,"debug"); fprintf(opt.log,"(JavaClass catch file): link too long, unable to write it: %s"LF,tempo);
                                    test_flush;
                                  }
                                }
                              }
                            }
                          }
                        } else {
                          if ((opt.debug>1) && (opt.log!=NULL)) {
                            fspc(opt.log,"debug"); fprintf(opt.log,"(JavaClass catch file): file not caught: %s"LF,lien); test_flush;
                          }
                          r=-1;
                        }
                        //
                        if (r != -1) {
                          if ((opt.debug>1) && (opt.log!=NULL)) {
                            fspc(opt.log,"debug"); fprintf(opt.log,"(JavaClass catch file): %s%s -> %s (base %s)"LF,adr,fil,save,codebase); test_flush;
                          }
                          
                          // modifié par rapport à l'autre version (cf prio_fix notamment et save2)
                          
                          // vérifier que le lien n'a pas déja été noté
                          // si c'est le cas, alors il faut s'assurer que la priorité associée
                          // au fichier est la plus grande des deux priorités
                          //
                          // On part de la fin et on essaye de se presser (économise temps machine)
#if HTS_HASH
                          {
                            int i=hash_read(&hash,save,"",0);      // lecture type 0 (sav)
                            if (i>=0) {
                              liens[i]->depth=maximum(liens[i]->depth,prio_fix);
                              dejafait=1;
                            }
                          }
#else
                          {
                            int l;
                            int i;
                            l=strlen(save);
                            for(i=lien_tot-1;(i>=0) && (dejafait==0);i--) {
                              if (liens[i]->sav_len==l) {    // même taille de chaîne
                                if (strcmp(liens[i]->sav,save)==0) {    // existe déja
                                  liens[i]->depth=maximum(liens[i]->depth,prio_fix);
                                  dejafait=1;
                                }
                              }
                            }
                          }
#endif
                          
                          
                          if (!dejafait) {
                            //
                            // >>>> CREER LE LIEN JAVA <<<<
                            
                            // enregistrer fichier de java (MACRO)
                            liens_record(adr,fil,save,"","");
                            if (liens[lien_tot]==NULL) {  // erreur, pas de place réservée
                              printf("PANIC! : Not enough memory [%d]\n",__LINE__);
                              if (opt.errlog) { 
                                fprintf(opt.errlog,"Not enough memory, can not re-allocate %d bytes"LF,(int)((add_tab_alloc+1)*sizeof(lien_url)));
                                test_flush;
                              }
                              // if ((opt.getmode & 1) && (ptr>0)) { if (fp) { fclose(fp); fp=NULL; } }
                              XH_extuninit;    // désallocation mémoire & buffers
                              return 0;
                            }  
                            
                            // mode test?                          
                            liens[lien_tot]->testmode=0;          // pas mode test
                            
                            liens[lien_tot]->link_import=0;       // pas mode import
                            
                            // écrire autres paramètres de la structure-lien
                            //if (meme_adresse)                                 
                            liens[lien_tot]->premier=liens[ptr]->premier;
                            //else    // sinon l'objet père est le précédent lui même
                            //  liens[lien_tot]->premier=ptr;
                            
                            liens[lien_tot]->precedent=ptr;
                            // noter la priorité
                            liens[lien_tot]->depth=prio_fix;
                            liens[lien_tot]->pass2=max(pass_fix,numero_passe);
                            liens[lien_tot]->retry=opt.retry;
                            
                            //strcpy(liens[lien_tot]->adr,adr);
                            //strcpy(liens[lien_tot]->fil,fil);
                            //strcpy(liens[lien_tot]->sav,save); 
                            if ((opt.debug>1) && (opt.log!=NULL)) {
                              fspc(opt.log,"debug"); fprintf(opt.log,"(JavaClass catch file): OK, NOTE: %s%s -> %s"LF,liens[lien_tot]->adr,liens[lien_tot]->fil,liens[lien_tot]->sav);
                              test_flush;
                            }
                            
                            lien_tot++;  // UN LIEN DE PLUS
                          }
                        }      
                        }  
                        
                      }  
                    }
                    
                  }
                  //##// effacer buffer temporaire
                  //##if (buffer) freet(buffer); buffer=NULL;
                  //##}  // if buffer
              }  // if exist
            }  // if .class
          }  // if strlen-savename
        }  // if opt.parsejava
        
        
        
      }  // text/html ou autre
      
    }  // if !error
    

jump_if_done:
    // libérer les liens
    if (r.adr) { freet(r.adr); r.adr=NULL; }   // libérer la mémoire!
    
    // prochain lien
    ptr++;
    
    // faut-il sauter le(s) lien(s) suivant(s)? (fichiers images à passer après les html)
    if (opt.getmode & 4) {    // sauver les non html après
      // sauter les fichiers selon la passe
      if (!numero_passe) {
        while((ptr<lien_tot)?(   liens[ptr]->pass2):0) ptr++;
      } else {
        while((ptr<lien_tot)?( ! liens[ptr]->pass2):0) ptr++;
      }
      if (ptr>=lien_tot) {     // fin de boucle
        if (!numero_passe) { // première boucle
          if ((opt.debug>1) && (opt.log!=NULL)) {
            fprintf(opt.log,LF"Now getting non-html files..."LF);
            test_flush;
          }
          numero_passe=1;   // seconde boucle
          ptr=0;
          // prochain pass2
          while((ptr<lien_tot)?(!liens[ptr]->pass2):0) ptr++;
          
          //printf("first link==%d\n");
          
        }
      }  
    }
        
    // a-t-on dépassé le quota?
    if ((opt.maxsite>0) && (HTS_STAT.stat_bytes>=opt.maxsite)) {
      if (opt.errlog) {
        fprintf(opt.errlog,"More than "LLintP" bytes have been transfered.. giving up"LF,opt.maxsite);
        test_flush;
      } 
      ptr=lien_tot;
    } else if ((opt.maxtime>0) && ((time_local()-HTS_STAT.stat_timestart)>opt.maxtime)) {            
      if (opt.errlog) {
        fprintf(opt.errlog,"More than %d seconds passed.. giving up"LF,opt.maxtime);
        test_flush;
      } 
      ptr=lien_tot;
    } else if (exit_xh) {  // sortir
      if (opt.errlog) {
        fspc(opt.errlog,"info"); fprintf(opt.errlog,"Exit requested by shell or user"LF);
        test_flush;
      } 
      ptr=lien_tot;
    }
  } while(ptr<lien_tot);
  //
  //
  //
  
  /*
  Ensure the index is being closed
  */
  HT_INDEX_END;
  
  /* 
    updating-a-remotely-deteted-website hack
    no much data transfered, no data saved
    <no files successfulyl saved>
    we assume that something was bad (no connection)
    just backup old cache and restore everything
  */
  if (
    (HTS_STAT.stat_files <= 0) 
    && 
    (HTS_STAT.HTS_TOTAL_RECV < 32768)    /* should be fine */
    ) {
    if (opt.errlog) {
      fspc(opt.errlog,"info"); fprintf(opt.errlog,"No data seems to have been transfered during this session! : restoring previous one!"LF);
      test_flush;
    } 
    XH_uninit;
    if ( (fexist(fconcat(opt.path_log,"hts-cache/old.dat"))) && (fexist(fconcat(opt.path_log,"hts-cache/old.ndx"))) ) {
      remove(fconcat(opt.path_log,"hts-cache/new.dat"));
      remove(fconcat(opt.path_log,"hts-cache/new.ndx"));
      remove(fconcat(opt.path_log,"hts-cache/new.lst"));
      remove(fconcat(opt.path_log,"hts-cache/new.txt"));
      rename(fconcat(opt.path_log,"hts-cache/old.dat"),fconcat(opt.path_log,"hts-cache/new.dat"));
      rename(fconcat(opt.path_log,"hts-cache/old.ndx"),fconcat(opt.path_log,"hts-cache/new.ndx"));
      rename(fconcat(opt.path_log,"hts-cache/old.lst"),fconcat(opt.path_log,"hts-cache/new.lst"));
      rename(fconcat(opt.path_log,"hts-cache/old.txt"),fconcat(opt.path_log,"hts-cache/new.txt"));
    }
    exit_xh=2;        /* interrupted (no connection detected) */
    return 1;
  }

  // info text  
  if (cache.txt) {
    fclose(cache.txt); cache.txt=NULL;
  }

  // purger!
  if (cache.lst) {
    fclose(cache.lst); cache.lst=NULL;
    if (opt.delete_old) {
      FILE *old_lst,*new_lst;
      //
#if HTS_ANALYSTE
      _hts_in_html_parsing=3;
#endif
      //
      old_lst=fopen(fconcat(opt.path_log,"hts-cache/old.lst"),"rb");
      if (old_lst) {
        LLint sz=fsize(fconcat(opt.path_log,"hts-cache/new.lst"));
        new_lst=fopen(fconcat(opt.path_log,"hts-cache/new.lst"),"rb");
        if ((new_lst) && (sz>0)) {
          char* adr=(char*) malloct((INTsys)sz);
          if (adr) {
            if ((int) fread(adr,1,(INTsys)sz,new_lst) == sz) {
              char line[1100];
              int purge=0;
              while(!feof(old_lst)) {
                linput(old_lst,line,1000);
                if (!strstr(adr,line)) {    // fichier non trouvé dans le nouveau?
                  char file[HTS_URLMAXSIZE*2];
                  strcpy(file,opt.path_html);
                  strcat(file,line+1);
                  file[strlen(file)-1]='\0';
                  if (fexist(file)) {       // toujours sur disque: virer
                    if (opt.log) {
                      fspc(opt.log,"info"); fprintf(opt.log,"Purging %s"LF,file);
                    }
                    remove(file); purge=1;
                  }
                }
              }
              {
                fseek(old_lst,0,SEEK_SET);
                while(!feof(old_lst)) {
                  linput(old_lst,line,1000);
                  while(strnotempty(line) && (line[strlen(line)-1]!='/') && (line[strlen(line)-1]!='\\')) {
                    line[strlen(line)-1]='\0';
                  }
                  if (strnotempty(line))
                    line[strlen(line)-1]='\0';
                  if (strnotempty(line))
                    if (!strstr(adr,line)) {    // non trouvé?
                      char file[HTS_URLMAXSIZE*2];
                      strcpy(file,opt.path_html);
                      strcat(file,line+1);
                      while ((strnotempty(file)) && (rmdir(file)==0)) {    // ok, éliminé (existait)
                        purge=1;
                        if (opt.log) {
                          fspc(opt.log,"info"); fprintf(opt.log,"Purging directory %s/"LF,file);
                          while(strnotempty(file) && (file[strlen(file)-1]!='/') && (file[strlen(file)-1]!='\\')) {
                            file[strlen(file)-1]='\0';
                          }
                          if (strnotempty(file))
                            file[strlen(file)-1]='\0';
                        }
                      }
                    }
                }
              }
              //
              if (!purge) {
                if (opt.log) {
                  fprintf(opt.log,"No files purged"LF);
                }
              }
            }
            freet(adr);
          }
          fclose(new_lst);
        }
        fclose(old_lst);
      }
      //
#if HTS_ANALYSTE
      _hts_in_html_parsing=0;
#endif
    }
  }
  // fin purge!

  // Indexation
  if (opt.kindex)
    index_finish(opt.path_html,opt.kindex);

  // afficher résumé dans log
  if (opt.log!=NULL) {
    int error   = fspc(NULL,"error");
    int warning = fspc(NULL,"warning");
    int info    = fspc(NULL,"info");
    char htstime[256];
    // int n=(int) (stat_loaded/(time_local()-HTS_STAT.stat_timestart));
    int n=(int) (HTS_STAT.HTS_TOTAL_RECV/(max(1,time_local()-HTS_STAT.stat_timestart)));
    
    sec2str(htstime,time_local()-HTS_STAT.stat_timestart);
    //fprintf(opt.log,LF"HTS-mirror complete in %s : %d links scanned, %d files written (%d bytes overall) [%d bytes received at %d bytes/sec]"LF,htstime,lien_tot-1,HTS_STAT.stat_files,stat_bytes,stat_loaded,n);
    fprintf(opt.log,LF"HTTrack mirror complete in %s : %d links scanned, %d files written (%d bytes overall) [%d bytes received at %d bytes/sec]",htstime,(int)lien_tot-1,(int)HTS_STAT.stat_files,(int)HTS_STAT.stat_bytes,(int)HTS_STAT.HTS_TOTAL_RECV,(int)n);
    if (HTS_STAT.total_packed) {
      int packed_ratio=(int)((LLint)(HTS_STAT.total_packed*100)/HTS_STAT.total_unpacked);
      fprintf(opt.log,", "LLintP" bytes transfered using HTTP compression in %d files, ratio %d%%",HTS_STAT.total_unpacked,HTS_STAT.total_packedfiles,packed_ratio);
    }
    fprintf(opt.log,LF);
    if (error)
      fprintf(opt.log,"(%d errors, %d warnings, %d messages)"LF,error,warning,info);
    else
      fprintf(opt.log,"(No errors, %d warnings, %d messages)"LF,warning,info);
    test_flush;
  }
#if DEBUG_HASH
  // noter les collisions
  {
    int i;
    int empty1=0,empty2=0,empty3=0;
    for(i=0;i<HTS_HASH_SIZE;i++) {
      if (hash.hash[0][i] == -1)
        empty1++;
      if (hash.hash[1][i] == -1)
        empty2++;
      if (hash.hash[2][i] == -1)
        empty3++;
    }
    printf("\n");
    printf("Debug info: Hash-table report\n");
    printf("Number of files entered:   %d\n",hashnumber);
    printf("Table size:                %d\n",HTS_HASH_SIZE);
    printf("\n");
    printf("Longest chain sav:              %d, empty: %d\n",longest_hash[0],empty1);
    printf("Longest chain adr,fil:          %d, empty: %d\n",longest_hash[1],empty2);
    printf("Longest chain former_adr/fil:   %d, empty: %d\n",longest_hash[2],empty3);
    printf("\n");
  }
#endif    
  // fin afficher résumé dans log
  
  // désallocation mémoire & buffers  

  XH_uninit    

  return 1;    // OK
}
// version 2 pour le reste
// flusher si on doit lire peu à peu le fichier
#undef test_flush
#define test_flush if (opt->flush) { fflush(opt->log); fflush(opt->errlog); }


// Estimate transfer rate
// a little bit complex, but not too much
/*
  .. : idle
  ^  : event

  ----|----|----|----|----|----|----|----|---->
   1    2    3    4    5    6    7    8    9   time (seconds)
  ----|----|----|----|----|----|----|----|---->
  ^........^.........^.........^.........^.... timer 0
  ----^.........^.........^.........^......... timer 1
           0    1    0    1    0    1    0     timer N sets its statistics
      *         *         *         *          timer 0 resync timer 1

  Therefore, each seconds, we resync the transfer rate with 2-seconds

*/
int engine_stats(void) {
#if 0
  static FILE* debug_fp=NULL; /* ok */
  if (!debug_fp)
    debug_fp=fopen("esstat.txt","wb");
#endif
  HTS_STAT.stat_nsocket=HTS_STAT.stat_errors=HTS_STAT.nbk==0;
  HTS_STAT.nb=0;
  if (HTS_STAT.HTS_TOTAL_RECV>2048) {
    TStamp cdif=mtime_local();
    int i;

    for(i=0;i<2;i++) {
      if ( (cdif - HTS_STAT.istat_timestart[i]) >= 2000) {
        TStamp dif;
#if 0
fprintf(debug_fp,"set timer %d\n",i); fflush(debug_fp);
#endif
        dif=cdif - HTS_STAT.istat_timestart[i];
        if ((TStamp)(dif/1000)>0) {
          LLint byt=(HTS_STAT.HTS_TOTAL_RECV - HTS_STAT.istat_bytes[i]);
          HTS_STAT.rate=(LLint)((TStamp) ((TStamp)byt/(dif/1000)));
          HTS_STAT.istat_idlasttimer=i;      // this timer recently sets the stats
          //
          HTS_STAT.istat_bytes[i]=HTS_STAT.HTS_TOTAL_RECV;
          HTS_STAT.istat_timestart[i]=cdif;
        }
        return 1;       /* refreshed */
      }
    }

    // resynchronization between timer 0 (master) and 1 (slave)
    // timer #0 resync timer #1 when reaching 1 second limit
    if (HTS_STAT.istat_reference01 != HTS_STAT.istat_timestart[0]) {
      if ( (cdif - HTS_STAT.istat_timestart[0]) >= 1000) {
#if 0
fprintf(debug_fp,"resync timer 1\n"); fflush(debug_fp);
#endif
        HTS_STAT.istat_bytes[1]=HTS_STAT.HTS_TOTAL_RECV;
        HTS_STAT.istat_timestart[1]=cdif;
        HTS_STAT.istat_reference01=HTS_STAT.istat_timestart[0];
      }
    }

  }
  return 0;
}


// bannir host (trop lent etc)
void host_ban(httrackp* opt,lien_url** liens,int ptr,int lien_tot,lien_back* back,int back_max,char** filters,int filter_max,int* filptr,char* host) {
  //int l;
  int i;

  if (host[0]=='!')
    return;    // erreur.. déja cancellé.. bizarre.. devrait pas arriver

  /* sanity check */
  if (*filptr + 1 >= opt->maxfilter) {
    opt->maxfilter += HTS_FILTERSINC;
    if (filters_init(&filters, opt->maxfilter, HTS_FILTERSINC) == 0) {
      printf("PANIC! : Too many filters : >%d [%d]\n",*filptr,__LINE__);
      if (opt->errlog) {
        fprintf(opt->errlog,LF"Too many filters, giving up..(>%d)"LF,*filptr);
        fprintf(opt->errlog,"To avoid that: use #F option for more filters (example: -#F5000)"LF);
        fflush(opt->errlog);
      }
      abort();
    }
    //opt->filters.filters=&filters;
  }

  // interdire host
  if (*filptr < filter_max) {
    strcpy(filters[*filptr],"-");
    strcat(filters[*filptr],host);
    strcat(filters[*filptr],"/*");     // host/ * interdit
    (*filptr)++; *filptr=minimum(*filptr,filter_max);  
  }
  
  // oups
  if (strlen(host)<=1) {    // euhh?? longueur <= 1
    if (strcmp(host,"file://")) {
      //## if (host[0]!=lOCAL_CHAR) {  // pas local
      if (opt->log!=NULL) {
        fprintf(opt->log,"PANIC! HostCancel detected memory leaks [char %d]"LF,host[0]); test_flush;
      }          
      return;  // purée
    }
  }
  
  // couper connexion
  for(i=0;i<back_max;i++) {
    if (back[i].status>=0)    // réception OU prêt
      if (strfield2(back[i].url_adr,host)) {
#if HTS_DEBUG_CLOSESOCK
        DEBUG_W("host control: deletehttp\n");
#endif
        back[i].status=0;  // terminé
        if (back[i].r.soc!=INVALID_SOCKET) deletehttp(&back[i].r);
        back[i].r.soc=INVALID_SOCKET;
        back[i].r.statuscode=-2;    // timeout (peu importe si c'est un traffic jam)
        strcpy(back[i].r.msg,"Link Cancelled by host control");
        
        if ((opt->debug>1) && (opt->log!=NULL)) {
          fprintf(opt->log,"Shutdown: %s%s"LF,back[i].url_adr,back[i].url_fil); test_flush;
        }          
      }
  }
  
  // effacer liens
  //l=strlen(host);
  for(i=0;i<lien_tot;i++) {
    //if (liens[i]->adr_len==l) {    // même taille de chaîne
    // Calcul de taille sécurisée
    if (liens[i]) {
      if (liens[i]->adr) {
        int l = 0;
        while((liens[i]->adr[l]) && (l<1020)) l++;
        if ((l > 0) && (l<1020)) {   // sécurité
          if (strfield2(jump_identification(liens[i]->adr),host)) {    // host
            if ((opt->debug>1) && (opt->log!=NULL)) {
              fprintf(opt->log,"Cancel: %s%s"LF,liens[i]->adr,liens[i]->fil); test_flush;
            }
            strcpy(liens[i]->adr,"!");    // cancel (invalide hash)
#if HTS_HASH
#else
            liens[i]->sav_len=-1;         // taille invalide
#endif
            // on efface pas le hash, because si on rencontre le lien, reverif sav..
          }
        } else {
          if (opt->log!=NULL) {
            char dmp[1040];
            dmp[0]='\0';
            strncat(dmp,liens[i]->adr,1024);
            fprintf(opt->log,"WARNING! HostCancel detected memory leaks [len %d at %d]"LF,l,i); test_flush;
            fprintf(opt->log,"dump 1024 bytes (address %p): "LF"%s"LF,liens[i]->adr,dmp); test_flush;
          }          
        }
      } else {
        if (opt->log!=NULL) {
          fprintf(opt->log,"WARNING! HostCancel detected memory leaks [adr at %d]"LF,i); test_flush;
        }  
      }
    } else {
      if (opt->log!=NULL) {
        fprintf(opt->log,"WARNING! HostCancel detected memory leaks [null at %d]"LF,i); test_flush;
      }  
    }
    //}
  }
}


/* Init structure */
/* 1 : init */
/* -1 : off */
char* structcheck_init(int init) {
  char** structcheck_buff;
  int* structcheck_buff_size;
  NOSTATIC_RESERVE(structcheck_buff, char*, 1);
  NOSTATIC_RESERVE(structcheck_buff_size, int, 1);
  if (init < 2) {
    if (init) {
      if (*structcheck_buff)
        freet(*structcheck_buff);
      *structcheck_buff=NULL;
    }
    if (init != -1) {
      if (*structcheck_buff==NULL) {
        *structcheck_buff_size = 65536;
        *structcheck_buff=(char*) malloct(*structcheck_buff_size);  // désalloué xh_xx
        if (*structcheck_buff)
          strcpy(*structcheck_buff,"#");
      }
    }
  } else {   /* Ensure enough room */
    if (*structcheck_buff_size < init) {
      *structcheck_buff_size = init + 65536;
      *structcheck_buff=(char*) realloct(*structcheck_buff, *structcheck_buff_size);
      if (*structcheck_buff == NULL) {   /* Reset :( */
        *structcheck_buff_size = 65536;
        *structcheck_buff=(char*) malloct(*structcheck_buff_size);  // désalloué xh_xx
        if (*structcheck_buff)
          strcpy(*structcheck_buff,"#");
      }
    }
  }
  return *structcheck_buff;
}

int filters_init(char*** ptrfilters, int maxfilter, int filterinc) {
  char** filters = *ptrfilters;
  int filter_max=maximum(maxfilter, 128);
  if (filters == NULL) {
    filters=(char**) malloct( sizeof(char*) * (filter_max+2) );
    memset(filters, 0, sizeof(char*) * (filter_max+2));  // filters[0] == 0
  } else {
    filters=(char**) realloct(filters, sizeof(char*) * (filter_max+2) );
  }
  if (filters) {
    if (filters[0] == NULL) {
      filters[0]=(char*) malloct( sizeof(char) * (filter_max+2) * (HTS_URLMAXSIZE*2) );
      memset(filters[0], 0, sizeof(char) * (filter_max+2) * (HTS_URLMAXSIZE*2) );
    } else {
      filters[0]=(char*) realloct(filters[0], sizeof(char) * (filter_max+2) * (HTS_URLMAXSIZE*2) );
    }
    if (filters[0] == NULL) {
      freet(filters);
      filters = NULL;
    }
  }
  if (filters != NULL) {
    int i;
    int from;
    if (filterinc == 0)
      from = 0;
    else
      from = filter_max - filterinc;
    for(i=0 ; i<=filter_max ; i++) {    // PLUS UN (sécurité)
      filters[i]=filters[0]+i*(HTS_URLMAXSIZE*2);
    }
    for(i=from ; i<=filter_max ; i++) {    // PLUS UN (sécurité)
      filters[i][0]='\0';    // clear
    }
  }
  *ptrfilters = filters;
  return (filters != NULL) ? filter_max : 0;
}

// vérifier présence de l'arbo
int structcheck(char* s) {
  // vérifier la présence des dossier(s)
  char *a=s;
  char nom[HTS_URLMAXSIZE*2];
  char *b;
  char* structcheck_buff=NULL;
  if (strnotempty(s)==0) return 0;
  if (strlen(s)>HTS_URLMAXSIZE) return 0;

  // Get buffer address
  structcheck_buff=structcheck_init(0);
  if (!structcheck_buff)
    return -1;

  if (strlen(structcheck_buff) > 65000) {
    strcpy(structcheck_buff,"#");  // réinit.. c'est idiot ** **
  }
  
  if (structcheck_buff) {
    b=nom;
    do {    
      if (*a) *b++=*a++;
      while((*a!='/') && (*a!='\0')) *b++=*a++;
      *b='\0';    // pas de ++ pour boucler
      if (*a=='/') {    // toujours dossier
        if (strnotempty(nom)) {
          char tempo[HTS_URLMAXSIZE*2];
          
          strcpy(tempo,"#"); strcat(tempo,nom); strcat(tempo,"#");
          if (strstr(structcheck_buff,tempo)==NULL) {    // non encore créé

            /* Check room */
            structcheck_init(strlen(structcheck_buff) + strlen(nom) + 8192);
            if (!structcheck_buff)
              return -1;

            strcat(structcheck_buff,"#"); strcat(structcheck_buff,nom); strcat(structcheck_buff,"#");  // ajouter à la liste
                        
#if HTS_WIN
            if (mkdir(fconv(nom))!=0)
#else    
              if (mkdir(fconv(nom),HTS_ACCESS_FOLDER)!=0)
#endif
              {
#if HTS_REMOVE_ANNOYING_INDEX
                // might be a filename with same name than this folder
                // then, remove it to allow folder creation
                // it happends when servers gives a folder index while
                // requesting / page
                // -> if the file can be opened (not a folder) then rename it
                FILE* fp=fopen(fconv(nom),"ab");
                if (fp) {
                  fclose(fp);
                  rename(fconv(nom),fconcat(fconv(nom),".txt"));
                }
                // if it fails, that's too bad
#if HTS_WIN
                mkdir(fconv(nom));
#else    
                mkdir(fconv(nom),HTS_ACCESS_FOLDER);
#endif
#endif
                // Si existe déja renvoie une erreur.. tant pis
              }
#if HTS_WIN==0
              chmod(fconv(nom),HTS_ACCESS_FOLDER);
#endif
          }
        }
        *b++=*a++;    // slash
      } 
    } while(*a);
  }
  return 0;
}


// sauver un fichier
int filesave(char* adr,int len,char* s) {
  FILE* fp;
  // écrire le fichier
  if ((fp=filecreate(s))!=NULL) {
    int nl=0;
    if (len>0) {
      nl=(int) fwrite(adr,1,len,fp);
    }
    fclose(fp);
    usercommand(0,NULL,antislash(s));
    if (nl!=len)  // erreur
      return -1;
  } else
    return -1;
  
  return 0;
}


// ouvrir un fichier (avec chemin Un*x)
FILE* filecreate(char* s) {
  char fname[HTS_URLMAXSIZE*2];
  FILE* fp;
  fname[0]='\0';

  // noter lst
  filenote(s,NULL);
  
  // if (*s=='/') strcpy(fname,s+1); else strcpy(fname,s);    // pas de / (root!!) // ** SIIIIIII!!! à cause de -O <path>
  strcpy(fname,s);

#if HTS_DOSNAME
  // remplacer / par des slash arrière
  {
    int i=0;
    while(fname[i]) {
      if (fname[i]=='/')
        fname[i]='\\';
      i++;
    } 
  } 
  // a partir d'ici le slash devient antislash
#endif
  
  // construite le chemin si besoin est
  if (structcheck(s)!=0) {
    return NULL;
  }
  
  // ouvrir
  fp=fopen(fname,"wb");
#if HTS_WIN==0
  if (fp!=NULL) chmod(fname,HTS_ACCESS_FILE);
#endif

  return fp;
}

// create an empty file
int filecreateempty(char* filename) {
  FILE* fp;
  fp=filecreate(filename);      // filenote & co
  if (fp) {
    fclose(fp);
    return 1;
  } else
    return 0; 
}

// noter fichier
typedef struct {
  FILE* lst;
  char path[HTS_URLMAXSIZE*2];
} filenote_strc;
int filenote(char* s,filecreate_params* params) {
  filenote_strc* strc;
  NOSTATIC_RESERVE(strc, filenote_strc, 1);
  
  // gestion du fichier liste liste
  if (params) {
    //filecreate_params* p = (filecreate_params*) params;
    strcpy(strc->path,params->path);
    strc->lst=params->lst;
    return 0;
  } else if (strc->lst) {
    char savelst[HTS_URLMAXSIZE*2];
    strcpy(savelst,fslash(s));
    // couper chemin?
    if (strnotempty(strc->path)) {
      if (strncmp(fslash(strc->path),savelst,strlen(strc->path))==0) {  // couper
        strcpy(savelst,s+strlen(strc->path));
      }
    }
    fprintf(strc->lst,"[%s]"LF,savelst);
    fflush(strc->lst);
  }
  return 1;
}

// executer commande utilisateur
typedef struct {
  int exe;
  char cmd[2048];
} usercommand_strc;
HTS_INLINE void usercommand(int _exe,char* _cmd,char* file) {
  usercommand_strc* strc;
  NOSTATIC_RESERVE(strc, usercommand_strc, 1);

  if (_exe) {
    strcpy(strc->cmd,_cmd);
    if (strnotempty(strc->cmd))
      strc->exe=_exe;
    else
      strc->exe=0;
  }

#if HTS_ANALYSTE
  if (hts_htmlcheck_filesave)
  if (strnotempty(file))
    hts_htmlcheck_filesave(file);
#endif

  if (strc->exe) {
    if (strnotempty(file)) {
      if (strnotempty(strc->cmd)) {
        usercommand_exe(strc->cmd,file);
      }
    }
  }
}
void usercommand_exe(char* cmd,char* file) {
  char temp[8192];
  char c[2]="";
  int i;
  temp[0]='\0';
  //
  for(i=0;i<(int) strlen(cmd);i++) {
    if ((cmd[i]=='$') && (cmd[i+1]=='0')) {
      strcat(temp,file);
      i++;
    } else {
      c[0]=cmd[i]; c[1]='\0';
      strcat(temp,c);
    }
  }
  system(temp);
}

// écrire n espaces dans fp
typedef struct {
  int error;
  int warning;
  int info;
} fspc_strc;
HTS_INLINE int fspc(FILE* fp,char* type) {
  fspc_strc* strc;
  NOSTATIC_RESERVE(strc, fspc_strc, 1); // log..

  //
  if (fp) {
    char s[256];
    time_t tt;
    struct tm* A;
    tt=time(NULL);
    A=localtime(&tt);
    strftime(s,250,"%H:%M:%S",A);
    if (strnotempty(type))
      fprintf(fp,"%s\t%c%s: \t",s,hichar(*type),type+1);
    else
      fprintf(fp,"%s\t \t",s);
    if (strcmp(type,"warning")==0)
      strc->warning++;
    else if (strcmp(type,"error")==0)
      strc->error++;
    else if (strcmp(type,"info")==0)
      strc->info++;
  } 
  else if (!type)
    strc->error=strc->warning=strc->info=0;     // reset
  else if (strcmp(type,"warning")==0)
    return strc->warning;
  else if (strcmp(type,"error")==0)
    return strc->error;
  else if (strcmp(type,"info")==0)
    return strc->info;
  return 0;
}


// vérifier taux de transfert
#if 0
void check_rate(TStamp stat_timestart,int maxrate) {
  // vérifier taux de transfert (pas trop grand?)
  /*
  if (maxrate>0) {
    int r = (int) (HTS_STAT.HTS_TOTAL_RECV/(time_local()-stat_timestart));    // taux actuel de transfert
    HTS_STAT.HTS_TOTAL_RECV_STATE=0;
    if (r>maxrate) {    // taux>taux autorisé
      int taux = (int) (((TStamp) (r - maxrate) * 100) / (TStamp) maxrate);
      if (taux<15)
        HTS_STAT.HTS_TOTAL_RECV_STATE=1;   // ralentir un peu (<15% dépassement)
      else if (taux<50)
        HTS_STAT.HTS_TOTAL_RECV_STATE=2;   // beaucoup (<50% dépassement)
      else
        HTS_STAT.HTS_TOTAL_RECV_STATE=3;   // énormément (>50% dépassement)
    }
  }
  */
}
#endif

// ---
// sous routines liées au moteur et au backing

// supplemental links ready (done) after ptr
int backlinks_done(lien_url** liens,int lien_tot,int ptr) {
  int n=0;
  int i;
  //Links done and stored in cache
  for(i=ptr+1;i<lien_tot;i++) {
    if (liens[i]) {
      if (liens[i]->pass2 == -1) {
        n++;
      }
    }
  }
  return n;
}

// remplir backing si moins de max_bytes en mémoire
HTS_INLINE int back_fillmax(lien_back* back,int back_max,httrackp* opt,cache_back* cache,lien_url** liens,int ptr,int numero_passe,int lien_tot) {
  if (!opt->state.stop) {
    if (back_incache(back,back_max)<opt->maxcache) {  // pas trop en mémoire?
      return back_fill(back,back_max,opt,cache,liens,ptr,numero_passe,lien_tot);
    }
  }
  return -1;                /* plus de place */
}

// remplir backing
int back_fill(lien_back* back,int back_max,httrackp* opt,cache_back* cache,lien_url** liens,int ptr,int numero_passe,int lien_tot) {
  int n;

  // ajouter autant de socket qu'on peut ajouter
  n=opt->maxsoc-back_nsoc(back,back_max);

  // vérifier qu'il restera assez de place pour les tests ensuite (en théorie, 1 entrée libre restante suffirait)
  n=min( n, back_available(back,back_max) - 8 );

  // no space left on backing stack - do not back anymore
  if (back_stack_available(back,back_max) <= 2)
    n=0;

  if (n>0) {
    int p;

    if (ptr<cache->ptr_last) {      /* restart (2 scans: first html, then non html) */
      cache->ptr_ant=0;
    }

    p=ptr+1;
    /* on a déja parcouru */
    if (p<cache->ptr_ant)
      p=cache->ptr_ant;
    while( (p<lien_tot) && (n>0) ) {
    //while((p<lien_tot) && (n>0) && (p < ptr+opt->maxcache_anticipate)) {
      int ok=1;
      
      // on ne met pas le fichier en backing si il doit être traité après
      if (liens[p]->pass2) {  // 2è passe
        if (numero_passe!=1)
          ok=0;
      } else {
        if (numero_passe!=0)
          ok=0;
      }
      
      // note: si un backing est fini, il reste en mémoire jusqu'à ce que
      // le ptr l'atteigne
      if (ok) {
        if (!back_exist(back,back_max,liens[p]->adr,liens[p]->fil,liens[p]->sav)) {
          if (back_add(back,back_max,opt,cache,liens[p]->adr,liens[p]->fil,liens[p]->sav,liens[liens[p]->precedent]->adr,liens[liens[p]->precedent]->fil,liens[p]->testmode,&liens[p]->pass2)==-1) {
            if ( (opt->debug>1) && (opt->errlog!=NULL) ) {
              fspc(opt->errlog,"debug"); fprintf(opt->errlog,"error: unable to add more links through back_add for back_fill"LF);
              test_flush;
            }                    
#if BDEBUG==1
            printf("error while adding\n");
#endif                  
            n=0;    // sortir
          } else {
            n--;
#if BDEBUG==1
            printf("backing: %s%s\n",liens[p]->adr,liens[p]->fil);          
#endif
          } 
        }
      }
      p++;
    }  // while
    /* sauver position dernière anticipation */
    cache->ptr_ant=p;
    cache->ptr_last=ptr;
  }
  return 0;
}
// ---


















// routines de détournement de SIGHUP & co (Unix)
//
httrackp* hts_declareoptbuffer(httrackp* optdecl) {
  static httrackp* opt=NULL; /* OK */
  if (optdecl) opt=optdecl;
  return opt;
}
//
void sig_finish( int code ) {       // finir et quitter
  signal(code,sig_term);  // quitter si encore
  exit_xh=1;
  fprintf(stderr,"\nExit requested to engine (signal %d)\n",code);
}
void sig_term( int code ) {       // quitter brutalement
  fprintf(stderr,"\nProgram terminated (signal %d)\n",code);
  exit(0);
}
#if HTS_WIN
void sig_ask( int code ) {        // demander
  char s[256];
  signal(code,sig_term);  // quitter si encore
  printf("\nQuit program/Interrupt/Cancel? (Q/I/C) ");
  fflush(stdout);
  scanf("%s",s);
  if ( (s[0]=='y') || (s[0]=='Y') || (s[0]=='o') || (s[0]=='O') || (s[0]=='q') || (s[0]=='Q'))
    exit(0);     // quitter
  else if ( (s[0]=='i') || (s[0]=='I') ) {
    httrackp* opt=hts_declareoptbuffer(NULL);
    if (opt) {
      // ask for stop
      opt->state.stop=1;
    }
  }
  signal(code,sig_ask);  // remettre signal
}
#else
void sig_back( int code ) {       // ignorer et mettre en backing 
  signal(code,sig_ignore);
  sig_doback(0);
}
void sig_ask( int code ) {        // demander
  char s[256];
  signal(code,sig_term);  // quitter si encore
  printf("\nQuit program/Interrupt/Background/bLind background/Cancel? (Q/I/B/L/C) ");
  fflush(stdout);
  scanf("%s",s);
  if ( (s[0]=='y') || (s[0]=='Y') || (s[0]=='o') || (s[0]=='O') || (s[0]=='q') || (s[0]=='Q'))
    exit(0);     // quitter
  else if ( (s[0]=='b') || (s[0]=='B') || (s[0]=='a') || (s[0]=='A') )
    sig_doback(0);  // arrière plan
  else if ( (s[0]=='l') || (s[0]=='L') )
    sig_doback(1);  // arrière plan
  else if ( (s[0]=='i') || (s[0]=='I') ) {
    httrackp* opt=hts_declareoptbuffer(NULL);
    if (opt) {
      // ask for stop
      opt->state.stop=1;
    }
    signal(code,sig_ask);  // remettre signal
  }
  else {
    printf("cancel..\n");
    signal(code,sig_ask);  // remettre signal
  }
}
void sig_ignore( int code ) {     // ignorer signal
}
void sig_brpipe( int code ) {     // treat if necessary
  if (!sig_ignore_flag(-1)) {
    sig_term(code);
  }
}
void sig_doback(int blind) {       // mettre en backing 
  int out=-1;
  //
  printf("\nMoving to background to complete the mirror...\n"); fflush(stdout);

  {
    httrackp* opt=hts_declareoptbuffer(NULL);
    if (opt) {
      // suppress logging and asking lousy questions
      opt->quiet=1;
      opt->verbosedisplay=0;
    }
  }

  if (!blind)
    out = open("hts-nohup.out",O_CREAT|O_WRONLY,S_IRUSR|S_IWUSR);
  if (out == -1)
    out = open("/dev/null",O_WRONLY,S_IRUSR|S_IWUSR);
  close(0);
  close(1);
  dup(out);
  close(2);
  dup(out);
  //
  switch (fork()) {
  case 0: 
    break;
  case -1:
    fprintf(stderr,"Error: can not fork process\n");
    break;
  default:            // pere
    usleep(100000);   // pause 1/10s "A  microsecond  is  .000001s"
    _exit(0);
    break;  
  }
}
#endif
// fin routines de détournement de SIGHUP & co

// Poll stdin.. si besoin
#if HTS_POLL
// lecture stdin des caractères disponibles
int read_stdin(char* s,int max) {
  int i=0;
  while((check_stdin()) && (i<(max-1)) )
    s[i++]=fgetc(stdin);
  s[i]='\0';
  return i;
}
#ifdef _WIN32
HTS_INLINE int check_stdin(void) {
  return (_kbhit());
}
#else
HTS_INLINE int check_flot(T_SOC s) {
  fd_set fds;
  struct timeval tv;
  FD_ZERO(&fds);
  FD_SET((T_SOC) s,&fds);
  tv.tv_sec=0;
  tv.tv_usec=0;
  select(s+1,&fds,NULL,NULL,&tv);
  return FD_ISSET(s,&fds);
}
HTS_INLINE int check_stdin(void) {
  fflush(stdout); fflush(stdin);
  if (check_flot(0))
    return 1;
  return 0;
}
#endif
#endif

// Attente de touche
#if HTS_ANALYSTE
int ask_continue(void) {
  char* s;
  s=hts_htmlcheck_query2(HTbuff);
  if (s) {
    if (strnotempty(s)) {
      if ((strfield2(s,"N")) || (strfield2(s,"NO")) || (strfield2(s,"NON")))
        return 0;
    }
    return 1;
  }
  return 1;
}
#else
int ask_continue(void) {
  char s[12];
  s[0]='\0';
  printf("Press <Y><Enter> to confirm, <N><Enter> to abort\n");
  io_flush; linput(stdin,s,4);
  if (strnotempty(s)) {
    if ((strfield2(s,"N")) || (strfield2(s,"NO")) || (strfield2(s,"NON")))
      return 0;
  }
  return 1;
}
#endif

// nombre de digits dans un nombre
int nombre_digit(int n) {
  int i=1;
  while(n >= 10) { n/=10; i++; }
  return i;
}


// renvoi adresse de la fin du token dans p
// renvoi NULL si la chaine est un token unique
// (PATCHE également la chaine)
// ex: "test" "test2" renvoi adresse sur espace
// flag==1 si chaine comporte des echappements comme \"
char* next_token(char* p,int flag) {
  int detect=0;
  int quote=0;
  p--;
  do {
    p++;
    if (flag && (*p=='\\')) {   // sauter \x ou \"
      if (quote) {
        char c='\0';
        if (*(p+1)=='\\')
          c='\\';
        else if (*(p+1)=='"')
          c='"';
        if (c) {
          char tempo[8192];
          tempo[0]=c; tempo[1]='\0';
          strcat(tempo,p+2);
          strcpy(p,tempo);
        }
      }
    }
    else if (*p==34) {  // guillemets (de fin)
      quote=!quote;
    }
    else if (*p==32) {
      if (!quote)
        detect=1;
    }
    else if (*p=='\0') {
      p=NULL;
      detect=1;
    }
  } while(!detect);
  return p;
}

// routines annexes 
#if HTS_ANALYSTE
// canceller un fichier (noter comme cancellable)
// !!NOT THREAD SAFE!!
char* hts_cancel_file(char * s) {
  static char sav[HTS_URLMAXSIZE*2]="";
  if (s[0]!='\0')
  if (sav[0]=='\0')
    strcpy(sav,s);
  return sav;
}
void hts_cancel_test(void) {
  if (_hts_in_html_parsing==2)
    _hts_cancel=2;
}
void hts_cancel_parsing(void) {
  if (_hts_in_html_parsing)
   _hts_cancel=1;
}
#endif
//        for(_i=0;(_i<back_max) && (index<NStatsBuffer);_i++) {
//          i=(back_index+_i)%back_max;    // commencer par le "premier" (l'actuel)
//          if (back[i].status>=0) {     // signifie "lien actif"


/*  
hts_add_file, add/get elements in the add chain for java parsing
if file_position >= 0
  push 'file/file_position'
  return 1 (return 0 if exists)
else
  pop file -> 'file'
  return 'file_position'
else if empty/error
  return -1;
*/
typedef struct addfile_chain {
  char name[1024];
  int pos;
  struct addfile_chain* next;
} addfile_chain;
typedef addfile_chain* addfile_chain_ptr;
int hts_add_file(char* file,int file_position) {
  addfile_chain** chain;
  NOSTATIC_RESERVE(chain, addfile_chain_ptr, 1);

  if (file_position>=0) {         /* copy file to the chain */
    struct addfile_chain** current;
    current=chain;                     /* start from */
    while(*current) {
      if (strcmp((*current)->name,file)==0)
        return 0;                       /* already exists */
      current=&( (*current)->next );    /* 'next' address */
    }
    *current=calloct(1,sizeof(addfile_chain));
    if (*current) {
      (*current)->next=NULL;
      (*current)->pos=-1;
      (*current)->name[0]='\0';
    }
    if (*current) {
      strcpy((*current)->name,file);
      (*current)->pos=file_position;
      return 1;
    } else {
      printf("PANIC! Too many Java files during parsing [1]\n");
      return -1;
    }
  } else {                      /* copy last element in file and delete it */
    if (file)
      file[0]='\0';
    if (*chain) {
      struct addfile_chain** current;
      int pos=-1;
      current=chain;                     /* start from */
      while( (*current)->next ) {
        current=&( (*current)->next );    /* 'next' address */
      }
      if (file)
        strcpy(file,(*current)->name);
      pos=(*current)->pos;
      freet(*current);
      *current=NULL;
      return pos;
    }
    return -1;                            /* no more elements */
  }

  return 0;
}

#if HTS_ANALYSTE
// en train de parser un fichier html? réponse: % effectués
// flag>0 : refresh demandé
int hts_is_parsing(int flag) {
  if (_hts_in_html_parsing) {  // parsing?
    if (flag>=0) _hts_in_html_poll=1;  // faudrait un tit refresh
    return max(_hts_in_html_done,1); // % effectués
  } else {
    return 0;                 // non
  }
}
int hts_is_testing(void) {            // 0 non 1 test 2 purge
  if (_hts_in_html_parsing==2)
    return 1;
  else if (_hts_in_html_parsing==3)
    return 2;
  return 0;
}
// message d'erreur?
char* hts_errmsg(void) {
  return _hts_errmsg;
}
// mode pause transfer
int hts_setpause(int p) {
  if (p>=0) _hts_setpause=p;
  return _hts_setpause;
}
// ask for termination
int hts_request_stop(int force) {
  httrackp* opt=hts_declareoptbuffer(NULL);
  if (opt) {
    opt->state.stop=1;
  }
  return 0;
}
// régler en cours de route les paramètres réglables..
// -1 : erreur
int hts_setopt(httrackp* set_opt) {
  if (set_opt) {
    httrackp* engine_opt=hts_declareoptbuffer(NULL);
    if (engine_opt) {
      //_hts_setopt=opt;
      copy_htsopt(set_opt,engine_opt);
    }
  }
  return 0;
}
// ajout d'URL
// -1 : erreur
int hts_addurl(char** url) {
  if (url) _hts_addurl=url;
  return (_hts_addurl!=NULL);
}
int hts_resetaddurl(void) {
  _hts_addurl=NULL;
  return (_hts_addurl!=NULL);
}
// copier nouveaux paramètres si besoin
int copy_htsopt(httrackp* from,httrackp* to) {
  if (from->maxsite > -1) 
    to->maxsite = from->maxsite;
  
  if (from->maxfile_nonhtml > -1) 
    to->maxfile_nonhtml = from->maxfile_nonhtml;
  
  if (from->maxfile_html > -1) 
    to->maxfile_html = from->maxfile_html;
  
  if (from->maxsoc > 0) 
    to->maxsoc = from->maxsoc;
  
  if (from->nearlink > -1) 
    to->nearlink = from->nearlink;
  
  if (from->timeout > -1) 
    to->timeout = from->timeout;
  
  if (from->rateout > -1)
    to->rateout = from->rateout;
  
  if (from->maxtime > -1) 
    to->maxtime = from->maxtime;
  
  if (from->maxrate > -1)
    to->maxrate = from->maxrate;
  
  if (strnotempty(from->user_agent)) 
    strcpy(to->user_agent , from->user_agent);
  
  if (from->retry > -1) 
    to->retry = from->retry;
  
  if (from->hostcontrol > -1) 
    to->hostcontrol = from->hostcontrol;
  
  if (from->errpage > -1) 
    to->errpage = from->errpage;

  if (from->parseall > -1) 
    to->parseall = from->parseall;


  // test all: bit 8 de travel
  if (from->travel > -1)  {
    if (from->travel & 256)
      to->travel|=256;
    else
      to->travel&=255;
  }


  return 0;
}

#endif
//





// message copyright interne
void voidf(void) {
  char* a;
  a=""CRLF""CRLF;
  a="+-----------------------------------------------+"CRLF;
  a="|HyperTextTRACKer, Offline Browser Utility      |"CRLF;
  a="|                      HTTrack Website Copier   |"CRLF;
  a="|Code:         Windows Interface Xavier Roche   |"CRLF;
  a="|                    HTS/HTTrack Xavier Roche   |"CRLF;
  a="|                .class Parser Yann Philippot   |"CRLF;
  a="|                                               |"CRLF;
  a="|Tested on:                 Windows95,98,NT,2K  |"CRLF;
  a="|                           Linux PC            |"CRLF;
  a="|                           Sun-Solaris 5.6     |"CRLF;
  a="|                           AIX 4               |"CRLF;
  a="|                                               |"CRLF;
  a="|Copyright (C) Xavier Roche and other           |"CRLF;
  a="|contributors                                   |"CRLF;
  a="|                                               |"CRLF;
  a="|Use this program at your own risks!            |"CRLF;    
  a="+-----------------------------------------------+"CRLF;
  a=""CRLF;
}


// HTTrack Website Copier Copyright (C) Xavier Roche and other contributors
//

