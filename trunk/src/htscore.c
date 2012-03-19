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

/* external modules */
#include "htsmodules.h"

// htswrap_add
#include "htswrap.h"

// parser
#include "htsparse.h"

/* Cache */
#include "htszlib.h"


/* END specific definitions */


/* HTML parsing */
#if HTS_ANALYSTE

t_hts_htmlcheck_init    hts_htmlcheck_init = NULL;
t_hts_htmlcheck_uninit  hts_htmlcheck_uninit = NULL;
t_hts_htmlcheck_start   hts_htmlcheck_start = NULL;
t_hts_htmlcheck_end     hts_htmlcheck_end = NULL;
t_hts_htmlcheck_chopt   hts_htmlcheck_chopt = NULL;
t_hts_htmlcheck_process hts_htmlcheck_preprocess = NULL;
t_hts_htmlcheck_process hts_htmlcheck_postprocess = NULL;
t_hts_htmlcheck         hts_htmlcheck = NULL;
t_hts_htmlcheck_query   hts_htmlcheck_query = NULL;
t_hts_htmlcheck_query2  hts_htmlcheck_query2 = NULL;
t_hts_htmlcheck_query3  hts_htmlcheck_query3 = NULL;
t_hts_htmlcheck_loop    hts_htmlcheck_loop = NULL;
t_hts_htmlcheck_check   hts_htmlcheck_check = NULL;
t_hts_htmlcheck_pause   hts_htmlcheck_pause = NULL;
t_hts_htmlcheck_filesave       hts_htmlcheck_filesave = NULL;
t_hts_htmlcheck_linkdetected   hts_htmlcheck_linkdetected = NULL;
t_hts_htmlcheck_linkdetected2  hts_htmlcheck_linkdetected2 = NULL;
t_hts_htmlcheck_xfrstatus hts_htmlcheck_xfrstatus = NULL;
t_hts_htmlcheck_savename  hts_htmlcheck_savename = NULL;
t_hts_htmlcheck_sendhead  hts_htmlcheck_sendhead = NULL;
t_hts_htmlcheck_receivehead  hts_htmlcheck_receivehead = NULL;

extern void set_wrappers(void);

char _hts_errmsg[1100]="";
int _hts_in_html_parsing=0;
int _hts_in_html_done=0;  // % done
int _hts_in_html_poll=0;  // parsing
int _hts_setpause=0;
//httrackp* _hts_setopt=NULL;
char** _hts_addurl=NULL;

/* external modules */
extern int hts_parse_externals(htsmoduleStruct* str);
extern void htspe_init(void);

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

#define XH_extuninit do { \
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
  back_delete(&opt,&cache,back,i); \
  } \
  freet(back); back=NULL;  \
  } \
  checkrobots_free(&robots);\
  if (cache.use) { freet(cache.use); cache.use=NULL; } \
  if (cache.dat) { fclose(cache.dat); cache.dat=NULL; }  \
  if (cache.ndx) { fclose(cache.ndx); cache.ndx=NULL; } \
  if (cache.zipOutput) { \
    zipClose(cache.zipOutput, "Created by HTTrack Website Copier/"HTTRACK_VERSION); \
    cache.zipOutput = NULL; \
  } \
  if (cache.zipInput) { \
    unzClose(cache.zipInput); \
    cache.zipInput = NULL; \
  } \
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
  if (cache_tests)     { inthash_delete(&cache_tests); } \
  if (template_header) { freet(template_header); template_header=NULL; } \
  if (template_body)   { freet(template_body); template_body=NULL; } \
  if (template_footer) { freet(template_footer); template_footer=NULL; } \
  clearCallbacks(&opt.state.callbacks); \
  /*structcheck_init(-1);*/ \
} while(0)
#define XH_uninit do { XH_extuninit; if (r.adr) { freet(r.adr); r.adr=NULL; } } while(0)

// Enregistrement d'un lien:
// on calcule la taille nécessaire: taille des 3 chaînes à stocker (taille forcée paire, plus 2 octets de sécurité)
// puis on vérifie qu'on a assez de marge dans le buffer - sinon on en réalloue un autre
// enfin on écrit à l'adresse courante du buffer, qu'on incrémente. on décrémente la taille dispo d'autant ensuite
// codebase: si non nul et si .class stockee on le note pour chemin primaire pour classes
// FA,FS: former_adr et former_fil, lien original
#if HTS_HASH
#define liens_record_sav_len(A) 
#else
#define liens_record_sav_len(A) (A)->sav_len=strlen((A)->sav)
#endif

#define liens_record(A,F,S,FA,FF,NORM) { \
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
if (notecode) { liens[lien_tot]->cod=lien_buffer; lien_buffer+=cod_len; lien_size-=cod_len; strcpybuff(liens[lien_tot]->cod,codebase); } \
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
hash_write(hashptr,lien_tot,NORM);  \
} \
}


#define HT_INDEX_END do { \
if (!makeindex_done) { \
if (makeindex_fp) { \
  char BIGSTK tempo[1024]; \
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
  usercommand(&opt,0,NULL,fconcat(opt.path_html,"index.html"),"","");  \
} \
} \
makeindex_done=1;    /* ok c'est fait */  \
} while(0)




// Début de httpmirror, robot
// url1 peut être multiple
int httpmirror(char* url1,httrackp* ptropt) {
  httrackp BIGSTK opt;         // structure d'options
  char* primary=NULL;          // première page, contenant les liens à scanner
  int lien_tot=0;              // nombre de liens pour le moment
  lien_url** liens=NULL;       // les pointeurs sur les liens
  hash_struct hash;            // système de hachage, accélère la recherche dans les liens
  hash_struct* hashptr = &hash;
  t_cookie BIGSTK cookie;             // gestion des cookies
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
  htsblk BIGSTK r;            // retour de certaines fonctions
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
  char BIGSTK makeindex_firstlink[HTS_URLMAXSIZE*2];
  // statistiques (mode #Z)
  FILE* makestat_fp=NULL;    // fichier de stats taux transfert
  FILE* maketrack_fp=NULL;   // idem pour le tracking
  TStamp makestat_time=0;    // attente (secondes)
  LLint makestat_total=0;    // repère du nombre d'octets transférés depuis denrière stat
  int makestat_lnk=0;        // idem, pour le nombre de liens
  //
  char BIGSTK codebase[HTS_URLMAXSIZE*2];  // base pour applet java
  char BIGSTK base[HTS_URLMAXSIZE*2];      // base pour les autres fichiers
  //
  cache_back BIGSTK cache;
  robots_wizard BIGSTK robots;    // gestion robots.txt
  inthash cache_hashtable=NULL;
  inthash cache_tests=NULL;
  int cache_hash_size=0;
  //
  char *template_header=NULL,*template_body=NULL,*template_footer=NULL;
  //
  opt = *ptropt;
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
  /*
  if (opt.aff_progress)
    lastime=HTS_STAT.stat_timestart;
    */
  if (opt.shell) {
    last_info_shell=HTS_STAT.stat_timestart;
  }
  if ((opt.makestat) || (opt.maketrack)){
    makestat_time=HTS_STAT.stat_timestart;
  }
  // initialiser compteur erreurs
  fspc(NULL,NULL);

  // init external modules
  htspe_init();

  // initialiser cookie
  if (opt.accept_cookie) {
    opt.cookie=&cookie;
    cookie.max_len=30000;       // max len
    strcpybuff(cookie.data,"");
    // Charger cookies.txt par défaut ou cookies.txt du miroir
    cookie_load(opt.cookie,opt.path_log,"cookies.txt");
    cookie_load(opt.cookie,"","cookies.txt");
  } else
    opt.cookie=NULL;

  // initialiser exit_xh
  exit_xh=0;          // sortir prématurément (var globale)

  // initialiser usercommand
  usercommand(&opt,opt.sys_com_exec,opt.sys_com,"","","");

  // initialiser structcheck
  // structcheck_init(1);

  // initialiser tableau options accessible par d'autres fonctions (signal)
  hts_declareoptbuffer(&opt);

  // initialiser verif_backblue
  verif_backblue(&opt,NULL);
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
  cache_tests=inthash_new(cache_hash_size);
  if (cache_hashtable==NULL || cache_tests==NULL) {
    printf("PANIC! : Not enough memory [%d]\n",__LINE__);
    filters[0]=NULL; back_max=0;    // uniquement a cause du warning de XH_extuninit
    XH_extuninit;
    return 0;
  }
  inthash_value_is_malloc(cache_tests, 1);     /* malloc */
  cache.hashtable=(void*)cache_hashtable;      /* copy backcache hash */
  cache.cached_tests=(void*)cache_tests;      /* copy of cache_tests */

  // initialiser cache DNS
  _hts_lockdns(-999);
  
  // robots.txt
  strcpybuff(robots.adr,"!");    // dummy
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
  
  // hash table
  opt.hash = &hash;

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
      
      if (joker) {    // joker ou filters
        //char* p;
        char BIGSTK tempo[HTS_URLMAXSIZE*2];
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
        while((*a!=0) && (!isspace((unsigned char)*a))) { tempo[i++]=*a; a++; }  
        tempo[i++]='\0';
        while(isspace((unsigned char)*a)) { a++; }

        // sauter les + sans rien après..
        if (strnotempty(tempo)) {
          if ((plus==0) && (type==1)) {  // implicite: *www.edf.fr par exemple
            if (tempo[strlen(tempo)-1]!='*') {
              strcatbuff(tempo,"*");  // ajouter un *
            }
          }
          if (type)
            strcpybuff(filters[filptr],"+");
          else
            strcpybuff(filters[filptr],"-");
          /*
          if (strfield(tempo,"http://"))
            strcatbuff(filters[filptr],tempo+7);        // ignorer http://
          else if (strfield(tempo,"ftp://"))
            strcatbuff(filters[filptr],tempo+6);        // ignorer ftp://
          else
          */
          strcatbuff(filters[filptr],tempo);
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
        char BIGSTK url[HTS_URLMAXSIZE*2];
        // prochaine adresse
        i=0;
        while((*a!=0) && (!isspace((unsigned char)*a))) { url[i++]=*a; a++; }  
        while(isspace((unsigned char)*a)) { a++; }
        url[i++]='\0';

        //strcatbuff(primary,"<PRIMARY=\"");
        if (strstr(url,":/")==NULL)
          strcatbuff(primary,"http://");
        strcatbuff(primary,url);
        //strcatbuff(primary,"\">");
        strcatbuff(primary,"\n");
      }
    }  // while

    /* load URL file list */
    /* OPTIMIZED for fast load */
    if (strnotempty(opt.filelist)) {
      char* filelist_buff=NULL;
      INTsys filelist_sz=fsize(opt.filelist);
      if (filelist_sz>0) {
        FILE* fp=fopen(opt.filelist,"rb");
        if (fp) {
          filelist_buff=malloct(filelist_sz + 2);
          if (filelist_buff) {
            if ((INTsys)fread(filelist_buff,1,filelist_sz,fp) != filelist_sz) {
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
        char BIGSTK line[HTS_URLMAXSIZE*2];
        char* primary_ptr = primary + strlen(primary);
        while( filelist_ptr < filelist_sz ) {
          int count=binput(filelist_buff+filelist_ptr,line,HTS_URLMAXSIZE);
          filelist_ptr+=count;
          if (count && line[0]) {
            n++;
            if (strstr(line,":/")==NULL) {
              strcpybuff(primary_ptr, "http://");
              primary_ptr += strlen(primary_ptr);
            }
            strcpybuff(primary_ptr, line);
            primary_ptr += strlen(primary_ptr);
            strcpybuff(primary_ptr, "\n");
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
    liens_record("primary","/primary",fslash(fconcat(opt.path_html,"index.html")),"","",opt.urlhack);
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
    {
      int backupXFR = htsMemoryFastXfr;
#if HTS_ANALYSTE
      _hts_in_html_parsing=4;
#endif
      if (!hts_htmlcheck_loop(NULL,0,0,0,lien_tot,0,NULL)) {
        exit_xh=1;  // exit requested
      }
      htsMemoryFastXfr = 1;               /* fast load */
      cache_init(&cache,&opt);
      htsMemoryFastXfr = backupXFR;
#if HTS_ANALYSTE
      _hts_in_html_parsing=0;
#endif
    }

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
	  fflush(makestat_fp);
    }
  }

  // tracking -- débuggage
  if (opt.maketrack) {
    maketrack_fp=fopen(fconcat(opt.path_log,"hts-track.txt"),"wb");
    if (maketrack_fp != NULL) {
      fprintf(maketrack_fp,"HTTrack tracking report, every minutes"LF LF);
	  fflush(maketrack_fp);
    }
  }

  // on n'a pas de liens!! (exemple: httrack www.* impossible sans départ..)
  if (lien_tot<=0) {
    if (opt.errlog) {
      fprintf(opt.errlog,"Error! You MUST specify at least one complete URL, and not only wildcards!"LF);
    }
  }

  /* Send options to callback functions */
#if HTS_ANALYSTE
  hts_htmlcheck_chopt(&opt);
#endif

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
    _hts_in_html_parsing=5;
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
    _hts_in_html_parsing=0;
    
    // note: recopie de plus haut
    // noter heure actuelle de départ en secondes
    HTS_STAT.stat_timestart=time_local();
    /*
    if (opt.aff_progress)
      lastime=HTS_STAT.stat_timestart;
      */
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
  set_wrappers();   // _start() is allowed to set other wrappers
#endif
  

  // ------------------------------------------------------------

  // ------------------------------------------------------------
  // Boucle générale de parcours des liens
  // ------------------------------------------------------------
  do {
    int error=0;          // si error alors sauter
    int store_errpage=0;  // c'est une erreur mais on enregistre le html
    char BIGSTK loc[HTS_URLMAXSIZE*2];    // adresse de relocation

    // Ici on charge le fichier (html, gif..) en mémoire
    // Les HTMLs sont traités (si leur priorité est suffisante)

    // effacer r
    memset(&r, 0, sizeof(htsblk)); r.soc=INVALID_SOCKET;
    r.location=loc;    // en cas d'erreur 3xx (moved)
    // recopier proxy
    memcpy(&(r.req.proxy), &opt.proxy, sizeof(opt.proxy));
    // et user-agent
    strcpybuff(r.req.user_agent,opt.user_agent);
    strcpybuff(r.req.referer,opt.referer);
    strcpybuff(r.req.from,opt.from);
    strcpybuff(r.req.lang_iso,opt.lang_iso);
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
          strcpybuff(r.contenttype,"text/html");
        /*} else if (opt.maxsoc<=0) {   // fichiers 1 à 1 en attente (pas de backing)
          // charger le fichier en mémoire tout bêtement
          r=xhttpget(urladr,urlfil);
          //
        */
        } else {    // backing, multiples sockets
          
          
          /*
            **************************************
            Get the next link, waiting for other files, handling external callbacks
          */
          {
            char BIGSTK buff_err_msg[1024];
            htsmoduleStruct BIGSTK str;
            htsmoduleStructExtended BIGSTK stre;
            buff_err_msg[0] = '\0';
            memset(&str, 0, sizeof(str));
            memset(&stre, 0, sizeof(stre));
            /* */
            str.err_msg = buff_err_msg;
            str.filename = savename;
            str.mime = r.contenttype;
            str.url_host = urladr;
            str.url_file = urlfil;
            str.size = (int) r.size;
            /* */
            str.addLink = htsAddLink;
            /* */
            str.liens = liens;
            str.opt = &opt;
            str.back = back;
            str.back_max = back_max;
            str.cache = &cache;
            str.hashptr = hashptr;
            str.numero_passe = numero_passe;
            str.add_tab_alloc = add_tab_alloc;
            /* */
            str.lien_tot_ = &lien_tot;
            str.ptr_ = &ptr;
            str.lien_size_ = &lien_size;
            str.lien_buffer_ = &lien_buffer;
            /* */
            /* */
            stre.r_ = &r;
            /* */
            stre.error_ = &error;
            stre.exit_xh_ = &exit_xh;
            stre.store_errpage_ = &store_errpage;
            /* */
            stre.base = base;
            stre.codebase = codebase;
            /* */
            stre.filters_ = &filters;
            stre.filptr_ = &filptr;
            stre.robots_ = &robots;
            stre.hash_ = &hash;
            stre.lien_max_ = &lien_max;
            /* */
            stre.makeindex_done_ = &makeindex_done;
            stre.makeindex_fp_ = &makeindex_fp;
            stre.makeindex_links_ = &makeindex_links;
            stre.makeindex_firstlink_ = makeindex_firstlink;
            /* */
            stre.template_header_ = template_header;
            stre.template_body_ = template_body;
            stre.template_footer_ = template_footer;
            /* */
            stre.stat_fragment_ = &stat_fragment;
            stre.makestat_time = makestat_time;
            stre.makestat_fp = makestat_fp;
            stre.makestat_total_ = &makestat_total;
            stre.makestat_lnk_ = &makestat_lnk;
            stre.maketrack_fp = maketrack_fp;
            /* FUNCTION DEPENDANT */
            stre.loc_ = loc;
            stre.last_info_shell_ = &last_info_shell;
            stre.info_shell_ = &info_shell;

            /* Parse */
            switch(hts_mirror_wait_for_next_file(&str, &stre)) {
            case -1:
              XH_uninit;
              return -1;
              break;
            case 2:
              // Jump to 'continue'
              // This is one of the very very rare cases where goto
              // is acceptable
              // A supplemental flag and if( ) { } would be really messy
              goto jump_if_done;
            }
            
          }
          
          
        }
        // FIN --RECUPERATION LIEN--- 
        // ------------------------------------------------------------
        
        
        
      } else {    // lien vide..
        if (opt.errlog && opt.debug > 0) {
          fspc(opt.errlog,"warning"); fprintf(opt.errlog,"Warning, link #%d empty"LF,ptr); test_flush;
        }         
        error=1;
        goto jump_if_done;
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
            if ( (is_hypertext_mime(r.contenttype, urlfil))   /* Is HTML or Js, .. */
              || (may_be_hypertext_mime(r.contenttype, urlfil) && (r.adr) )  /* Is real media, .. */
              ) {
              if (strnotempty(r.cdispo)) {      // Content-disposition set!
                if (ishtml(savename) == 0) {    // Non HTML!!
                  // patch it!
                  strcpybuff(r.contenttype,"application/octet-stream");
                }
              }
            }
          }
        }
        
        // ------------------------------------
        // BOGUS MIME TYPE HACK II (the revenge)
        // Check if we have a bogus MIME type
        if ( (is_hypertext_mime(r.contenttype, urlfil))   /* Is HTML or Js, .. */
          || (may_be_hypertext_mime(r.contenttype, urlfil))  /* Is real media, .. */
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
            /* On-the-fly UCS2 to ISO-8859-1 conversion (note: UCS2 should never be used on the net) */
            if (
              map[0] > r.size/10
              &&
              r.size % 2 == 0
              &&
              (
              ( ((unsigned char) r.adr[0]) == 0xff && ((unsigned char) r.adr[1]) == 0xfe)
              ||
              ( ((unsigned char) r.adr[0]) == 0xfe && ((unsigned char) r.adr[1]) == 0xff)
              )
              ) {
              int lost=0;
              int i;
              int swap = (r.adr[0] == 0xff);
              for(i = 0 ; i < r.size / 2 - 1 ; i++) {
                unsigned int unic = 0;
                if (swap)
                  unic = (r.adr[i*2 + 2] << 8) + r.adr[i*2 + 2 + 1];
                else
                  unic = r.adr[i*2 + 2] + (r.adr[i*2 + 2 + 1] << 8);
                if (unic <= 255)
                  r.adr[i] = (char) unic;
                else {
                  r.adr[i] = '?';
                  lost++;
                }
              }
              r.size = r.size / 2 - 1;
              r.adr[r.size] = '\0';

              if (opt.errlog) {
                fspc(opt.errlog,"warning"); fprintf(opt.errlog,"File %s%s converted from UCS2 to 8-bit, %d characters lost during conversion (better to use UTF-8)"LF, urladr, urlfil, lost);
                test_flush;
              }
            } else if ((nspec > r.size / 100) && (nspec > 10)) {    // too many special characters
              strcpybuff(r.contenttype,"application/octet-stream");
              if (opt.errlog) {
                fspc(opt.errlog,"warning"); fprintf(opt.errlog,"File not parsed, looks like binary: %s%s"LF,urladr,urlfil);
                test_flush;
              }
            }

            /* This hack allows to avoid problems with parsing '\0' characters  */
            for(i = 0 ; i < r.size ; i++) {
              if (r.adr[i] == '\0') r.adr[i] = ' ';
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
            if (may_be_hypertext_mime(r.contenttype, urlfil)) {   // to parse!
              LLint sz;
              sz=fsize(savename);
              if (sz>0) {   // ok, exists!
                if (sz < 8192) {   // ok, small file --> to parse!
                  FILE* fp=fopen(savename,"rb");
                  if (fp) {
                    r.adr=malloct((int)sz + 2);
                    if (r.adr) {
                      if (fread(r.adr,1,(INTsys)sz,fp) == sz) {
                        r.size=sz;
                      } else {
                        freet(r.adr);
                        r.size=0;
                        r.adr = NULL;
                        r.statuscode=-1;
                        strcpybuff(r.msg, ".RAM read error");
                      }
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
            xxcache_mayadd(&opt,&cache,&r,urladr,urlfil,savename);
          } else
            error=1;
        }
      }
      */
      // ---fin stockage en cache---
      
      
      
      /*
         **************************************
         Check "Moved permanently" and other similar errors, retrying URLs if necessary and handling
         redirect pages.
      */
      if (!error) {
        char BIGSTK buff_err_msg[1024];
        htsmoduleStruct BIGSTK str;
        htsmoduleStructExtended BIGSTK stre;
        buff_err_msg[0] = '\0';
        memset(&str, 0, sizeof(str));
        memset(&stre, 0, sizeof(stre));
        /* */
        str.err_msg = buff_err_msg;
        str.filename = savename;
        str.mime = r.contenttype;
        str.url_host = urladr;
        str.url_file = urlfil;
        str.size = (int) r.size;
        /* */
        str.addLink = htsAddLink;
        /* */
        str.liens = liens;
        str.opt = &opt;
        str.back = back;
        str.back_max = back_max;
        str.cache = &cache;
        str.hashptr = hashptr;
        str.numero_passe = numero_passe;
        str.add_tab_alloc = add_tab_alloc;
        /* */
        str.lien_tot_ = &lien_tot;
        str.ptr_ = &ptr;
        str.lien_size_ = &lien_size;
        str.lien_buffer_ = &lien_buffer;
        /* */
        /* */
        stre.r_ = &r;
        /* */
        stre.error_ = &error;
        stre.exit_xh_ = &exit_xh;
        stre.store_errpage_ = &store_errpage;
        /* */
        stre.base = base;
        stre.codebase = codebase;
        /* */
        stre.filters_ = &filters;
        stre.filptr_ = &filptr;
        stre.robots_ = &robots;
        stre.hash_ = &hash;
        stre.lien_max_ = &lien_max;
        /* */
        stre.makeindex_done_ = &makeindex_done;
        stre.makeindex_fp_ = &makeindex_fp;
        stre.makeindex_links_ = &makeindex_links;
        stre.makeindex_firstlink_ = makeindex_firstlink;
        /* */
        stre.template_header_ = template_header;
        stre.template_body_ = template_body;
        stre.template_footer_ = template_footer;
        /* */
        stre.stat_fragment_ = &stat_fragment;
        stre.makestat_time = makestat_time;
        stre.makestat_fp = makestat_fp;
        stre.makestat_total_ = &makestat_total;
        stre.makestat_lnk_ = &makestat_lnk;
        stre.maketrack_fp = maketrack_fp;
        
        /* Parse */
        if (hts_mirror_check_moved(&str, &stre) != 0) {
          XH_uninit;
          return -1;
        }
        
      }

    }  // if !error
    
    if (!error) {
#if DEBUG_SHOWTYPES
      if (strstr(REG,r.contenttype)==NULL) {
        strcatbuff(REG,r.contenttype);
        strcatbuff(REG,"\n");
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
           ( (is_hypertext_mime(r.contenttype, urlfil))   /* Is HTML or Js, .. */
             || (may_be_hypertext_mime(r.contenttype, urlfil) && (r.adr) )  /* Is real media, .. */
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
            char BIGSTK buff_err_msg[1024];
            htsmoduleStruct BIGSTK str;
            htsmoduleStructExtended BIGSTK stre;
            buff_err_msg[0] = '\0';
            memset(&str, 0, sizeof(str));
            memset(&stre, 0, sizeof(stre));
            /* */
            str.err_msg = buff_err_msg;
            str.filename = savename;
            str.mime = r.contenttype;
            str.url_host = urladr;
            str.url_file = urlfil;
            str.size = (int) r.size;
            /* */
            str.addLink = htsAddLink;
            /* */
            str.liens = liens;
            str.opt = &opt;
            str.back = back;
            str.back_max = back_max;
            str.cache = &cache;
            str.hashptr = hashptr;
            str.numero_passe = numero_passe;
            str.add_tab_alloc = add_tab_alloc;
            /* */
            str.lien_tot_ = &lien_tot;
            str.ptr_ = &ptr;
            str.lien_size_ = &lien_size;
            str.lien_buffer_ = &lien_buffer;
            /* */
            /* */
            stre.r_ = &r;
            /* */
            stre.error_ = &error;
            stre.exit_xh_ = &exit_xh;
            stre.store_errpage_ = &store_errpage;
            /* */
            stre.base = base;
            stre.codebase = codebase;
            /* */
            stre.filters_ = &filters;
            stre.filptr_ = &filptr;
            stre.robots_ = &robots;
            stre.hash_ = &hash;
            stre.lien_max_ = &lien_max;
            /* */
            stre.makeindex_done_ = &makeindex_done;
            stre.makeindex_fp_ = &makeindex_fp;
            stre.makeindex_links_ = &makeindex_links;
            stre.makeindex_firstlink_ = makeindex_firstlink;
            /* */
            stre.template_header_ = template_header;
            stre.template_body_ = template_body;
            stre.template_footer_ = template_footer;
            /* */
            stre.stat_fragment_ = &stat_fragment;
            stre.makestat_time = makestat_time;
            stre.makestat_fp = makestat_fp;
            stre.makestat_total_ = &makestat_total;
            stre.makestat_lnk_ = &makestat_lnk;
            stre.maketrack_fp = maketrack_fp;
            
            /* Parse */
            if (htsparse(&str, &stre) != 0) {
              XH_uninit;
              return -1;
            }


          // I'll have to segment this part
// #include "htsparse.c"


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
              char BIGSTK line[1024];
              char BIGSTK buff[8192];
              char BIGSTK infobuff[8192];
              int record=0;
              line[0]='\0'; buff[0]='\0'; infobuff[0]='\0';
              //
#if DEBUG_ROBOTS
              printf("robots.txt dump:\n%s\n",r.adr);
#endif
              do {
                char* comm;
                int llen;
                bptr+=binput(r.adr+bptr, line, sizeof(line) - 2);
                /* strip comment */
                comm=strchr(line, '#');
                if (comm != NULL) {
                  *comm = '\0';
                }
                /* strip spaces */
                llen=strlen(line);
                while(llen > 0 && is_realspace(line[llen - 1])) {
                  line[llen - 1] = '\0';
                  llen--;
                }
                if (strfield(line,"user-agent:")) {
                  char* a;
                  a=line+11;
                  while(is_realspace(*a)) a++;    // sauter espace(s)
                  if ( *a == '*') {
                    if (record != 2)
                      record=1;    // c pour nous
                  } else if (strfield(a,"httrack") || strfield(a,"winhttrack") || strfield(a,"webhttrack")) {
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
                    char* a=line+9;
                    while(is_realspace(*a))
                      a++;    // sauter espace(s)
                    if (strnotempty(a)) {
                      if (strcmp(a,"/") != 0 || opt.robots >= 3) {      /* ignoring disallow: / */
                        if ( (strlen(buff) + strlen(a) + 8) < sizeof(buff)) {
                          strcatbuff(buff,a);
                          strcatbuff(buff,"\n");
                          if ( (strlen(infobuff) + strlen(a) + 8) < sizeof(infobuff)) {
                            if (strnotempty(infobuff)) strcatbuff(infobuff,", ");
                            strcatbuff(infobuff,a);
                          }
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
          if ( (is_hypertext_mime(r.contenttype, urlfil)) && (!store_errpage) && (r.size>0)) {  // c'est du html!!
            char BIGSTK tempo[HTS_URLMAXSIZE*2];
            FILE* fp;
            tempo[0]='\0';
            strcpybuff(tempo,savename);
            strcatbuff(tempo,".readme");
            
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
              fprintf(fp,"Info-file generated by HTTrack Website Copier "HTTRACK_VERSION"%s"CRLF""CRLF, WHAT_is_available);
              fprintf(fp,"The file %s has not been scanned by HTS"CRLF,savename);
              fprintf(fp,"Some links contained in it may be unreachable locally."CRLF);
              fprintf(fp,"If you want to get these files, you have to set an upper recurse level, ");
              fprintf(fp,"and to rescan the URL."CRLF);
              fclose(fp);
#if HTS_WIN==0
              chmod(tempo,HTS_ACCESS_FILE);      
#endif
              usercommand(&opt,0,NULL,fconv(tempo),"","");
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
              if (r.adr) {
                freet(r.adr); r.adr=NULL;
              }
            }
          }
          
          //printf("extern=%s\n",r.contenttype);

          // ATTENTION C'EST ICI QU'ON SAUVE LE FICHIER!!          
          if (r.adr) {
            if (filesave(&opt,r.adr,(int)r.size,savename,urladr,urlfil)!=0) {
              int fcheck;
              if ((fcheck=check_fatal_io_errno())) {
                exit_xh=-1;   /* fatal error */
              }
              if (opt.errlog) {   
                fspc(opt.errlog,"error"); fprintf(opt.errlog,"Unable to save file %s : %s"LF, savename, strerror(errno));
                if (fcheck) {
                  fspc(opt.errlog,"error");
                  fprintf(opt.errlog,"* * Fatal write error, giving up"LF);
                }
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
                char BIGSTK line[HTS_URLMAXSIZE*2];
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


        /* External modules */
        if (opt.parsejava && fexist(savename)) {
          char BIGSTK buff_err_msg[1024];
          htsmoduleStruct BIGSTK str;
          buff_err_msg[0] = '\0';
          memset(&str, 0, sizeof(str));
          /* */
          str.err_msg = buff_err_msg;
          str.filename = savename;
          str.mime = r.contenttype;
          str.url_host = urladr;
          str.url_file = urlfil;
          str.size = (int) r.size;
          /* */
          str.addLink = htsAddLink;
          /* */
          str.liens = liens;
          str.opt = &opt;
          str.back = back;
          str.back_max = back_max;
          str.cache = &cache;
          str.hashptr = hashptr;
          str.numero_passe = numero_passe;
          str.add_tab_alloc = add_tab_alloc;
          /* */
          str.lien_tot_ = &lien_tot;
          str.ptr_ = &ptr;
          str.lien_size_ = &lien_size;
          str.lien_buffer_ = &lien_buffer;
          /* Parse if recognized */
          switch(hts_parse_externals(&str)) {
          case 1:
            if ((opt.debug>1) && (opt.log!=NULL)) {
              fspc(opt.log,"debug"); fprintf(opt.log,"(External module): parsed successfully %s"LF,savename); test_flush;
            }
            break;
          case 0:
            if ((opt.debug>1) && (opt.log!=NULL)) {
              fspc(opt.log,"debug"); fprintf(opt.log,"(External module): couldn't parse successfully %s : %s"LF,savename, str.err_msg); test_flush;
            }
            break;
          }
        }
             
        
      }  // text/html ou autre
      

      /* Post-processing */
      if (fexist(savename)) {
        usercommand(&opt, 0, NULL, savename, urladr, urlfil);
      }


    }  // if !error
    

jump_if_done:
    // libérer les liens
    if (r.adr) { 
      freet(r.adr); 
      r.adr=NULL; 
    }   // libérer la mémoire!
    
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

    // copy abort state if necessary from outside
    if (!exit_xh && opt.state.exit_xh) {
      exit_xh=opt.state.exit_xh;
    }
    // a-t-on dépassé le quota?
    if (!back_checkmirror(&opt)) {
      ptr=lien_tot;
    } else if (exit_xh) {  // sortir
      if (opt.errlog) {
        fspc(opt.errlog,"info"); 
        if (exit_xh==1) {
          fprintf(opt.errlog,"Exit requested by shell or user"LF);
        } else {
          fprintf(opt.errlog,"Exit requested by engine"LF);
        }
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
            if (fread(adr,1,(INTsys)sz,new_lst) == sz) {
              char line[1100];
              int purge=0;
              while(!feof(old_lst)) {
                linput(old_lst,line,1000);
                if (!strstr(adr,line)) {    // fichier non trouvé dans le nouveau?
                  char BIGSTK file[HTS_URLMAXSIZE*2];
                  strcpybuff(file,opt.path_html);
                  strcatbuff(file,line+1);
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
                      char BIGSTK file[HTS_URLMAXSIZE*2];
                      strcpybuff(file,opt.path_html);
                      strcatbuff(file,line+1);
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
    char BIGSTK finalInfo[8192];
    int error   = fspc(NULL,"error");
    int warning = fspc(NULL,"warning");
    int info    = fspc(NULL,"info");
    char BIGSTK htstime[256];
    char BIGSTK infoupdated[256];
    // int n=(int) (stat_loaded/(time_local()-HTS_STAT.stat_timestart));
    LLint n=(LLint) (HTS_STAT.HTS_TOTAL_RECV/(max(1,time_local()-HTS_STAT.stat_timestart)));
    
    sec2str(htstime,time_local()-HTS_STAT.stat_timestart);
    //sprintf(finalInfo + strlen(finalInfo),LF"HTS-mirror complete in %s : %d links scanned, %d files written (%d bytes overall) [%d bytes received at %d bytes/sec]"LF,htstime,lien_tot-1,HTS_STAT.stat_files,stat_bytes,stat_loaded,n);
    infoupdated[0] = '\0';
    if (opt.is_update) {
      if (HTS_STAT.stat_updated_files > 0) {
        sprintf(infoupdated, ", %d files updated", (int)HTS_STAT.stat_updated_files);
      } else {
        sprintf(infoupdated, ", no files updated");
      }
    }
    finalInfo[0] = '\0';
    sprintf(finalInfo + strlen(finalInfo),
      "HTTrack Website Copier/"HTTRACK_VERSION" mirror complete in %s : "
      "%d links scanned, %d files written ("LLintP" bytes overall)%s "
      "["LLintP" bytes received at "LLintP" bytes/sec]",
      htstime,
      (int)lien_tot-1,
      (int)HTS_STAT.stat_files,
      (LLint)HTS_STAT.stat_bytes,
      infoupdated,
      (LLint)HTS_STAT.HTS_TOTAL_RECV,
      (LLint)n
      );

    if (HTS_STAT.total_packed > 0 && HTS_STAT.total_unpacked > 0) {
      int packed_ratio=(int)((LLint)(HTS_STAT.total_packed*100)/HTS_STAT.total_unpacked);
      sprintf(finalInfo + strlen(finalInfo),", "LLintP" bytes transfered using HTTP compression in %d files, ratio %d%%",(LLint)HTS_STAT.total_unpacked,HTS_STAT.total_packedfiles,(int)packed_ratio);
    }
    if (!opt.nokeepalive && HTS_STAT.stat_sockid > 0 && HTS_STAT.stat_nrequests > HTS_STAT.stat_sockid) {
      int rq = (HTS_STAT.stat_nrequests * 10) / HTS_STAT.stat_sockid;
      sprintf(finalInfo + strlen(finalInfo),", %d.%d requests per connection", rq/10, rq%10);
    }
    sprintf(finalInfo + strlen(finalInfo),LF);
    if (error)
      sprintf(finalInfo + strlen(finalInfo),"(%d errors, %d warnings, %d messages)"LF,error,warning,info);
    else
      sprintf(finalInfo + strlen(finalInfo),"(No errors, %d warnings, %d messages)"LF,warning,info);

    // Log
    fprintf(opt.log,LF"%s", finalInfo);

    // Close ZIP
    if (cache.zipOutput) {
      zipClose(cache.zipOutput, finalInfo);
      cache.zipOutput = NULL;
    }
    
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

  // ending
  usercommand(&opt,0,NULL,NULL,NULL,NULL);

  // désallocation mémoire & buffers
  XH_uninit;

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


#define _FILTERS     (*opt->filters.filters)
#define _FILTERS_PTR (opt->filters.filptr)
#define _ROBOTS      ((robots_wizard*)opt->robotsptr)

// bannir host (trop lent etc)
void host_ban(httrackp* opt,lien_url** liens,int ptr,int lien_tot,lien_back* back,int back_max,char* host) {
  //int l;
  int i;

  if (host[0]=='!')
    return;    // erreur.. déja cancellé.. bizarre.. devrait pas arriver

  /* sanity check */
  if (*_FILTERS_PTR + 1 >= opt->maxfilter) {
    opt->maxfilter += HTS_FILTERSINC;
    if (filters_init(&_FILTERS, opt->maxfilter, HTS_FILTERSINC) == 0) {
      printf("PANIC! : Too many filters : >%d [%d]\n",*_FILTERS_PTR,__LINE__);
      if (opt->errlog) {
        fprintf(opt->errlog,LF"Too many filters, giving up..(>%d)"LF,*_FILTERS_PTR);
        fprintf(opt->errlog,"To avoid that: use #F option for more filters (example: -#F5000)"LF);
        fflush(opt->errlog);
      }
      assertf("too many filters - giving up" == NULL);
    }
  }

  // interdire host
  assertf((*_FILTERS_PTR) < opt->maxfilter);
  if (*_FILTERS_PTR < opt->maxfilter) {
    strcpybuff(_FILTERS[*_FILTERS_PTR],"-");
    strcatbuff(_FILTERS[*_FILTERS_PTR],host);
    strcatbuff(_FILTERS[*_FILTERS_PTR],"/*");     // host/ * interdit
    (*_FILTERS_PTR)++; 
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
        strcpybuff(back[i].r.msg,"Link Cancelled by host control");
        
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
            strcpybuff(liens[i]->adr,"!");    // cancel (invalide hash)
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
            strncatbuff(dmp,liens[i]->adr,1024);
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


#if 0
/* Init structure */
/* 1 : init */
/* -1 : off */
/* 0 : query */
/* 2 : LOCK */
/* -2 : UNLOCK */
void* structcheck_init(int init) {
  int structcheck_size = 1024;
  inthash structcheck_hash=NULL;
  /* */
  static PTHREAD_LOCK_TYPE structcheck_init_mutex;
  static int structcheck_init_mutex_init=0;

  if (init == 1 || init == -1) {
    if (init) {
      if (structcheck_hash)
        inthash_delete(&structcheck_hash);
      structcheck_hash=NULL;
    }
    if (init != -1) {
      if (structcheck_init_mutex_init == 0) {
        htsSetLock(&structcheck_init_mutex, -999);
        structcheck_init_mutex_init=1;
      }
      if (structcheck_hash==NULL) {
        structcheck_hash=inthash_new(structcheck_size);  // désalloué xh_xx
      }
    }
  }

  /* Lock / Unlock */
  if (init == 2) {  // Lock
    htsSetLock(&structcheck_init_mutex, 1);
  } else if (init == -2) {  // Unlock
    htsSetLock(&structcheck_init_mutex, 0);
  }
  return structcheck_hash;
}
#endif

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
HTSEXT_API int structcheck(char* s) {
  // vérifier la présence des dossier(s)
  char *a=s;
  char BIGSTK nom[HTS_URLMAXSIZE*2];
  char *b;
  //inthash structcheck_hash=NULL;
  if (strnotempty(s)==0) return 0;
  if (strlen(s)>HTS_URLMAXSIZE) return 0;

  // Get buffer address
  /*
  structcheck_hash = (inthash)structcheck_init(0);
  if (structcheck_hash == NULL) {
    return -1;
  }
  */

  b=nom;
  do {    
    if (*a) *b++=*a++;
    while((*a!='/') && (*a!='\0')) *b++=*a++;
    *b='\0';    // pas de ++ pour boucler
    if (*a=='/') {    // toujours dossier
      if (strnotempty(nom)) {         
        //if (inthash_write(structcheck_hash, nom, 1)) {    // non encore créé                       
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
            if (fexist(fconv(nom))) {
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
          /*chmod(fconv(nom),HTS_ACCESS_FOLDER);*/
#endif
          //}
      }
      *b++=*a++;    // slash
    } 
  } while(*a);
  return 0;
}


// sauver un fichier
int filesave(httrackp* opt,char* adr,int len,char* s,char* url_adr,char* url_fil) {
  FILE* fp;
  // écrire le fichier
  if ((fp=filecreate(s))!=NULL) {
    int nl=0;
    if (len>0) {
      nl=(int) fwrite(adr,1,(INTsys)len,fp);
    }
    fclose(fp);
    //xxusercommand(opt,0,NULL,fconv(s),url_adr,url_fil);
    if (nl!=len)  // erreur
      return -1;
  } else
    return -1;
  
  return 0;
}

/* We should stop */
int check_fatal_io_errno(void) {
  switch(errno) {
#ifdef EMFILE
  case EMFILE: /* Too many open files */
#endif
#ifdef ENOSPC
  case ENOSPC: /* No space left on device */
#endif
#ifdef EROFS
  case EROFS:  /* Read-only file system */
#endif
    return 1;
    break;
  }
  return 0;
}


// ouvrir un fichier (avec chemin Un*x)
FILE* filecreate(char* s) {
  char BIGSTK fname[HTS_URLMAXSIZE*2];
  FILE* fp;
  fname[0]='\0';

  // noter lst
  filenote(s,NULL);
  
  // if (*s=='/') strcpybuff(fname,s+1); else strcpybuff(fname,s);    // pas de / (root!!) // ** SIIIIIII!!! à cause de -O <path>
  strcpybuff(fname,s);

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
  
  // ouvrir
  fp=fopen(fname,"wb");
  if (fp == NULL) {
    // construire le chemin si besoin est
    (void)structcheck(s);
    fp=fopen(fname,"wb");
  }
  
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
    strcpybuff(strc->path,params->path);
    strc->lst=params->lst;
    return 0;
  } else if (strc->lst) {
    char BIGSTK savelst[HTS_URLMAXSIZE*2];
    strcpybuff(savelst,fslash(s));
    // couper chemin?
    if (strnotempty(strc->path)) {
      if (strncmp(fslash(strc->path),savelst,strlen(strc->path))==0) {  // couper
        strcpybuff(savelst,s+strlen(strc->path));
      }
    }
    fprintf(strc->lst,"[%s]"LF,savelst);
    fflush(strc->lst);
  }
  return 1;
}

// executer commande utilisateur
static void postprocess_file(httrackp* opt,char* save, char* adr, char* fil);
typedef struct {
  int exe;
  char cmd[2048];
} usercommand_strc;
HTS_INLINE void usercommand(httrackp* opt,int _exe,char* _cmd,char* file,char* adr,char* fil) {
  usercommand_strc* strc;
  NOSTATIC_RESERVE(strc, usercommand_strc, 1);
  
  /* Callback */
  if (_exe) {
    strcpybuff(strc->cmd,_cmd);
    if (strnotempty(strc->cmd))
      strc->exe=_exe;
    else
      strc->exe=0;
  }

  /* post-processing */
  postprocess_file(opt, file, adr, fil);

#if HTS_ANALYSTE
  if (hts_htmlcheck_filesave != NULL)
  if (file != NULL && strnotempty(file))
    hts_htmlcheck_filesave(file);
#endif

  if (strc->exe) {
    if (file != NULL && strnotempty(file)) {
      if (strnotempty(strc->cmd)) {
        usercommand_exe(strc->cmd,file);
      }
    }
  }
}
void usercommand_exe(char* cmd,char* file) {
  char BIGSTK temp[8192];
  char c[2]="";
  int i;
  temp[0]='\0';
  //
  for(i=0;i<(int) strlen(cmd);i++) {
    if ((cmd[i]=='$') && (cmd[i+1]=='0')) {
      strcatbuff(temp,file);
      i++;
    } else {
      c[0]=cmd[i]; c[1]='\0';
      strcatbuff(temp,c);
    }
  }
  system(temp);
}


static void postprocess_file(httrackp* opt,char* save, char* adr, char* fil) {
  int first = 0;
  /* MIME-html archive to build */
  if (opt != NULL && opt->mimehtml) {
    if (adr != NULL && strcmp(adr, "primary") == 0) {
      adr = NULL;
    }
    if (save != NULL && opt != NULL && adr != NULL && adr[0] && strnotempty(save) && fexist(save)) {
      char* rsc_save = save;
      char* rsc_fil = strrchr(fil, '/');
      int n;
      if (rsc_fil == NULL)
        rsc_fil = fil;
      if (strncmp(fslash(save), fslash(opt->path_html), (n = (int)strlen(opt->path_html))) == 0) {
        rsc_save += n;
      }

      if (!opt->state.mimehtml_created) {
        first = 1;
        opt->state.mimefp = fopen(fconcat(opt->path_html,"index.mht"), "wb");
        if (opt->state.mimefp != NULL) {
          char BIGSTK rndtmp[1024], currtime[256];
          srand(time(NULL));
          time_gmt_rfc822(currtime);
          sprintf(rndtmp, "%d_%d", (int)time(NULL), (int) rand());
          sprintf(opt->state.mimemid, "----=_MIMEPart_%s_=----", rndtmp);
          fprintf(opt->state.mimefp, "From: HTTrack Website Copier <nobody@localhost>\r\n"
            "Subject: Local mirror\r\n"
            "Date: %s\r\n"
            "Message-ID: <httrack_%s@localhost>\r\n"
            "Content-Type: multipart/related;\r\n"
            "\tboundary=\"%s\";\r\n"
            "\ttype=\"text/html\"\r\n"
            "MIME-Version: 1.0\r\n"
            "\r\nThis message is a RFC MIME-compliant multipart message.\r\n"
            "\r\n"
            , currtime, rndtmp, opt->state.mimemid);
          opt->state.mimehtml_created = 1;
        } else {
          opt->state.mimehtml_created = -1;
          if ( opt->errlog != NULL ) {
            fspc(opt->errlog,"error"); fprintf(opt->log,"unable to create index.mht"LF);
          }
        }
      }
      if (opt->state.mimehtml_created == 1 && opt->state.mimefp != NULL) {
        FILE* fp = fopen(save, "rb");
        if (fp != NULL) {
          char buff[60*100 + 2];
          char mimebuff[256];
          char BIGSTK cid[HTS_URLMAXSIZE*3];
          int len;
          int isHtml = ( ishtml(save) == 1 );
          mimebuff[0] = '\0';

          /* CID */
          strcpybuff(cid, adr);
          strcatbuff(cid, fil);
          escape_in_url(cid);
          { char* a = cid; while((a = strchr(a, '%'))) { *a = 'X'; a++; } }
          
          guess_httptype(mimebuff, save);
          fprintf(opt->state.mimefp, "--%s\r\n", opt->state.mimemid);
          /*if (first)
          fprintf(opt->state.mimefp, "Content-disposition: inline\r\n");
          else*/
          fprintf(opt->state.mimefp, "Content-disposition: attachment; filename=\"%s\"\r\n", rsc_save);
          fprintf(opt->state.mimefp, 
            "Content-Type: %s\r\n"
            "Content-Transfer-Encoding: %s\r\n"
            /*"Content-Location: http://localhost/%s\r\n"*/
            "Content-ID: <%s>\r\n"
            "\r\n"
            , mimebuff
            , isHtml ? "8bit" : "base64"
            /*, rsc_save*/
            , cid);
          while((len = fread(buff, 1, sizeof(buff) - 2, fp)) > 0) {
            buff[len] = '\0';
            if (!isHtml) {
              char base64buff[60*100*2];
              code64((unsigned char*)buff, len, (unsigned char*)base64buff, 1);
              fprintf(opt->state.mimefp, "%s", base64buff);
            } else {
              fprintf(opt->state.mimefp, "%s", buff);
            }
          }
          fclose(fp);
          fprintf(opt->state.mimefp, "\r\n\r\n");
        }
      }
    } else if (save == NULL) {
      if (opt->state.mimehtml_created == 1 && opt->state.mimefp != NULL) {
        fprintf(opt->state.mimefp, 
          "--%s--\r\n", opt->state.mimemid);
        fclose(opt->state.mimefp);
        opt->state.mimefp = NULL;
      }
    }
  }
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
    if (A == NULL) {
      int localtime_returned_null=0;
      assert(localtime_returned_null);
    }
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

int back_pluggable_sockets_strict(lien_back* back, int back_max, httrackp* opt) {
  int n = opt->maxsoc - back_nsoc(back, back_max);

  // connect limiter
  if (n > 0 && opt->maxconn > 0 && HTS_STAT.last_connect > 0) {
    TStamp opTime = HTS_STAT.last_request ? HTS_STAT.last_request : HTS_STAT.last_connect;
    TStamp cTime = mtime_local();
    TStamp lap = ( cTime - opTime );
    TStamp minLap = (TStamp) ( 1000.0 / opt->maxconn );
    if (lap < minLap) {
      n = 0;
    } else {
      int nMax = (int) ( lap / minLap );
      n = min(n, nMax);
    }
  }

  return n;
}

int back_pluggable_sockets(lien_back* back, int back_max, httrackp* opt) {
  int n;

  // ajouter autant de socket qu'on peut ajouter
  n=back_pluggable_sockets_strict(back, back_max, opt);

  // vérifier qu'il restera assez de place pour les tests ensuite (en théorie, 1 entrée libre restante suffirait)
  n=min( n, back_available(back,back_max) - 8 );

  // no space left on backing stack - do not back anymore
  if (back_stack_available(back,back_max) <= 2)
    n=0;

  return n;
}

// remplir backing
int back_fill(lien_back* back,int back_max,httrackp* opt,cache_back* cache,lien_url** liens,int ptr,int numero_passe,int lien_tot) {
  int n = back_pluggable_sockets(back, back_max, opt);
  if (n>0) {
    int p;

    if (ptr<cache->ptr_last) {      /* restart (2 scans: first html, then non html) */
      cache->ptr_ant=0;
    }

    p=ptr+1;
    /* on a déja parcouru */
    if (p<cache->ptr_ant)
      p=cache->ptr_ant;
    while( (p<lien_tot) && (n>0) && back_checkmirror(opt)) {
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
      printf("finishing pending transfers.. please wait\n");
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
  signal(code, sig_brpipe);
}
void sig_doback(int blind) {       // mettre en backing 
  int out=-1;
  //
  printf("\nMoving into background to complete the mirror...\n"); fflush(stdout);

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
#ifndef _WIN32_WCE
  return (_kbhit());
#else
  return 0;
#endif
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

HTS_INLINE int check_sockerror(T_SOC s) {
  fd_set fds;
  struct timeval tv;
  FD_ZERO(&fds);
  FD_SET((T_SOC) s,&fds);
  tv.tv_sec=0;
  tv.tv_usec=0;
  select(s+1,NULL,NULL,&fds,&tv);
  return FD_ISSET(s,&fds);
}

/* check incoming data */
HTS_INLINE int check_sockdata(T_SOC s) {
  fd_set fds;
  struct timeval tv;
  FD_ZERO(&fds);
  FD_SET((T_SOC) s,&fds);
  tv.tv_sec=0;
  tv.tv_usec=0;
  select(s+1,&fds,NULL,NULL,&tv);
  return FD_ISSET(s,&fds);
}

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
          char BIGSTK tempo[8192];
          tempo[0]=c; tempo[1]='\0';
          strcatbuff(tempo,p+2);
          strcpybuff(p,tempo);
        }
      }
    }
    else if (*p==34) {  // guillemets (de fin)
      char BIGSTK tempo[8192];
      tempo[0]='\0';
      strcatbuff(tempo,p+1);
      strcpybuff(p,tempo);   /* wipe "" */
      p--;
      /* */
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
HTSEXT_API char* hts_cancel_file(char * s) {
  static char sav[HTS_URLMAXSIZE*2]="";
  if (s[0]!='\0')
  if (sav[0]=='\0')
    strcpybuff(sav,s);
  return sav;
}
HTSEXT_API void hts_cancel_test(void) {
  if (_hts_in_html_parsing==2)
    _hts_cancel=2;
}
HTSEXT_API void hts_cancel_parsing(void) {
  if (_hts_in_html_parsing)
   _hts_cancel=1;
}
#endif
//        for(_i=0;(_i<back_max) && (index<NStatsBuffer);_i++) {
//          i=(back_index+_i)%back_max;    // commencer par le "premier" (l'actuel)
//          if (back[i].status>=0) {     // signifie "lien actif"

#if 0
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
int opt->(char* file,int file_position) {
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
      strcpybuff((*current)->name,file);
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
        strcpybuff(file,(*current)->name);
      pos=(*current)->pos;
      freet(*current);
      *current=NULL;
      return pos;
    }
    return -1;                            /* no more elements */
  }

  return 0;
}
#endif

#if HTS_ANALYSTE
// en train de parser un fichier html? réponse: % effectués
// flag>0 : refresh demandé
HTSEXT_API int hts_is_parsing(int flag) {
  if (_hts_in_html_parsing) {  // parsing?
    if (flag>=0) _hts_in_html_poll=1;  // faudrait un tit refresh
    return max(_hts_in_html_done,1); // % effectués
  } else {
    return 0;                 // non
  }
}
HTSEXT_API int hts_is_testing(void) {            // 0 non 1 test 2 purge
  if (_hts_in_html_parsing==2)
    return 1;
  else if (_hts_in_html_parsing==3)
    return 2;
  else if (_hts_in_html_parsing==4)
    return 3;
  else if (_hts_in_html_parsing==5)   // scheduling
    return 4;
  else if (_hts_in_html_parsing==6)   // wait for slot
    return 5;
  return 0;
}
HTSEXT_API int hts_is_exiting(void) {
  return exit_xh;
}
// message d'erreur?
char* hts_errmsg(void) {
  return _hts_errmsg;
}
// mode pause transfer
HTSEXT_API int hts_setpause(int p) {
  if (p>=0) _hts_setpause=p;
  return _hts_setpause;
}
// ask for termination
HTSEXT_API int hts_request_stop(int force) {
  httrackp* opt=hts_declareoptbuffer(NULL);
  if (opt) {
    opt->state.stop=1;
  }
  return 0;
}
// régler en cours de route les paramètres réglables..
// -1 : erreur
HTSEXT_API int hts_setopt(httrackp* set_opt) {
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
HTSEXT_API int hts_addurl(char** url) {
  if (url) _hts_addurl=url;
  return (_hts_addurl!=NULL);
}
HTSEXT_API int hts_resetaddurl(void) {
  _hts_addurl=NULL;
  return (_hts_addurl!=NULL);
}
// copier nouveaux paramètres si besoin
HTSEXT_API int copy_htsopt(httrackp* from,httrackp* to) {
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
  
  if (from->maxconn > 0)
    to->maxconn = from->maxconn;

  if (strnotempty(from->user_agent)) 
    strcpybuff(to->user_agent , from->user_agent);
  
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

/* External modules callback */
int htsAddLink(htsmoduleStruct* str, char* link) {
  if (link != NULL && str != NULL && link[0] != '\0') {
    lien_url** liens = (lien_url**) str->liens;
    httrackp* opt = (httrackp*) str->opt;
    lien_back* back = (lien_back*) str->back;
    cache_back* cache = (cache_back*) str->cache;
    hash_struct* hashptr = (hash_struct*) str->hashptr;
    int back_max = str->back_max;
    int numero_passe = str->numero_passe;
    int add_tab_alloc = str->add_tab_alloc;
    /* */
    int lien_tot = * ( (int*) (str->lien_tot_) );
    int ptr = * ( (int*) (str->ptr_) );
    int lien_size = * ( (int*) (str->lien_size_) );
    char* lien_buffer = * ( (char**) (str->lien_buffer_) );
    /* */
    /* */
    char BIGSTK adr[HTS_URLMAXSIZE*2],
      fil[HTS_URLMAXSIZE*2],
      save[HTS_URLMAXSIZE*2];
    char BIGSTK codebase[HTS_URLMAXSIZE*2];
    /* */
    int pass_fix, prio_fix;
    /* */
    int forbidden_url = 1;
    
    codebase[0]='\0';
    
    if ((opt->debug>1) && (opt->log!=NULL)) {
      fspc(opt->log,"debug"); fprintf(opt->log,"(module): adding link : '%s'"LF, link); test_flush;
    }
    // recopie de "creer le lien"
    //    

#if HTS_ANALYSTE
  if (!hts_htmlcheck_linkdetected(link) || !hts_htmlcheck_linkdetected2(link, NULL)) {
    if (opt->errlog) {
      fspc(opt->errlog,"error"); fprintf(opt->errlog,"Link %s refused by external wrapper"LF, link);
      test_flush;
    }
    return 0;
  }
#endif

    // adr = c'est la même
    // fil et save: save2 et fil2
    prio_fix=maximum(liens[ptr]->depth-1,0);
    pass_fix=max(liens[ptr]->pass2,numero_passe);
    if (liens[ptr]->cod) strcpybuff(codebase,liens[ptr]->cod);       // codebase valable pour tt les classes descendantes
    if (strnotempty(codebase)==0) {    // pas de codebase, construire
      char* a;
      if (str->relativeToHtmlLink == 0)
        strcpybuff(codebase,liens[ptr]->fil);
      else
        strcpybuff(codebase,liens[liens[ptr]->precedent]->fil);
      a=codebase+strlen(codebase)-1;
      while((*a) && (*a!='/') && ( a > codebase)) a--;
      if (*a=='/')
        *(a+1)='\0';    // couper
    } else {    // couper http:// éventuel
      if (strfield(codebase,"http://")) {
        char BIGSTK tempo[HTS_URLMAXSIZE*2];
        char* a=codebase+7;
        a=strchr(a,'/');    // après host
        if (a) {  // ** msg erreur et vérifier?
          strcpybuff(tempo,a);
          strcpybuff(codebase,tempo);    // couper host
        } else {
          if (opt->errlog) {   
            fprintf(opt->errlog,"Unexpected strstr error in base %s"LF,codebase);
            test_flush;
          }
        }
      }
    }
    
    if (!((int) strlen(codebase)<HTS_URLMAXSIZE)) {    // trop long
      if (opt->errlog) {   
        fprintf(opt->errlog,"Codebase too long, parsing skipped (%s)"LF,codebase);
        test_flush;
      }
    }
    
    {
      char* lien = link;
      int dejafait=0;
      
      if (strnotempty(lien) && strlen(lien) < HTS_URLMAXSIZE) {
        
        // calculer les chemins et noms de sauvegarde
        if (ident_url_relatif(lien,urladr,codebase,adr,fil)>=0) { // reformage selon chemin
          int r;
          int set_prio_to = 0;
          int just_test_it = 0;
          forbidden_url = hts_acceptlink(opt, ptr, lien_tot, liens,
            adr,fil,
            NULL, NULL,
            &set_prio_to,
            &just_test_it);
          if ((opt->debug>1) && (opt->log!=NULL)) {
            fspc(opt->log,"debug"); fprintf(opt->log,"result for wizard external module link: %d"LF,forbidden_url);
            test_flush;
          }

          /* Link accepted */
          if (!forbidden_url) {
            char BIGSTK tempo[HTS_URLMAXSIZE*2];
            int a,b;
            tempo[0]='\0';
            a=opt->savename_type;
            b=opt->savename_83;
            opt->savename_type=0;
            opt->savename_83=0;
            // note: adr,fil peuvent être patchés
            r=url_savename(adr,fil,save,NULL,NULL,NULL,NULL,opt,liens,lien_tot,back,back_max,cache,hashptr,ptr,numero_passe);
            opt->savename_type=a;
            opt->savename_83=b;
            if (r != -1) {
              if (savename) {
                if (lienrelatif(tempo,save,savename)==0) {
                  if ((opt->debug>1) && (opt->log!=NULL)) {
                    fspc(opt->log,"debug"); fprintf(opt->log,"(module): relative link at %s build with %s and %s: %s"LF,adr,save,savename,tempo);
                    test_flush;
                    if (str->localLink && str->localLinkSize > (int) strlen(tempo) + 1) {
                      strcpybuff(str->localLink, tempo);
                    }
                  }
                }
              }
            }
          } else {
            if ((opt->debug>1) && (opt->log!=NULL)) {
              fspc(opt->log,"debug"); fprintf(opt->log,"(module): file not caught: %s"LF,lien); test_flush;
            }
            if (str->localLink && str->localLinkSize > (int) ( strlen(adr) + strlen(fil) +  8 ) ) {
              str->localLink[0] = '\0';
              if (!link_has_authority(adr))
                strcpybuff(str->localLink,"http://");
              strcatbuff(str->localLink, adr);
              strcatbuff(str->localLink, fil);
            }
            r=-1;
          }
          //
          if (r != -1) {
            if ((opt->debug>1) && (opt->log!=NULL)) {
              fspc(opt->log,"debug"); fprintf(opt->log,"(module): %s%s -> %s (base %s)"LF,adr,fil,save,codebase); test_flush;
            }
            
            // modifié par rapport à l'autre version (cf prio_fix notamment et save2)
            
            // vérifier que le lien n'a pas déja été noté
            // si c'est le cas, alors il faut s'assurer que la priorité associée
            // au fichier est la plus grande des deux priorités
            //
            // On part de la fin et on essaye de se presser (économise temps machine)
#if HTS_HASH
            {
              int i=hash_read(hashptr,save,"",0,opt->urlhack);      // lecture type 0 (sav)
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
              
              // enregistrer fichier (MACRO)
              liens_record(adr,fil,save,"","",opt->urlhack);
              if (liens[lien_tot]==NULL) {  // erreur, pas de place réservée
                printf("PANIC! : Not enough memory [%d]\n",__LINE__);
                if (opt->errlog) { 
                  fprintf(opt->errlog,"Not enough memory, can not re-allocate %d bytes"LF,(int)((add_tab_alloc+1)*sizeof(lien_url)));
                  test_flush;
                }
                exit_xh=-1;    /* fatal error -> exit */
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
              if (!set_prio_to)
                liens[lien_tot]->depth=prio_fix;
              else
                liens[lien_tot]->depth=max(0,min(liens[ptr]->depth-1,set_prio_to-1));         // PRIORITE NULLE (catch page)
              liens[lien_tot]->pass2=max(pass_fix,numero_passe);
              liens[lien_tot]->retry=opt->retry;
              
              //strcpybuff(liens[lien_tot]->adr,adr);
              //strcpybuff(liens[lien_tot]->fil,fil);
              //strcpybuff(liens[lien_tot]->sav,save); 
              if ((opt->debug>1) && (opt->log!=NULL)) {
                fspc(opt->log,"debug"); fprintf(opt->log,"(module): OK, NOTE: %s%s -> %s"LF,liens[lien_tot]->adr,liens[lien_tot]->fil,liens[lien_tot]->sav);
                test_flush;
              }
              
              lien_tot++;  // UN LIEN DE PLUS
            }
          }
        }
      }
    }
    
    /* Apply changes */
    * ( (int*) (str->lien_tot_) ) = lien_tot;
    * ( (int*) (str->ptr_) ) = ptr;
    * ( (int*) (str->lien_size_) ) = lien_size;
    * ( (char**) (str->lien_buffer_) ) = lien_buffer;
    return (forbidden_url == 0);
  }
  return 0;
}





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

