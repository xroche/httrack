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
/* File: Main file .h                                           */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

// Fichier librairie .h
#ifndef HTTRACK_DEFH
#define HTTRACK_DEFH


#include "htsglobal.h"

/* specific definitions */
#include "htsbase.h"
// Includes & définitions
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef _WIN32
#ifndef  _WIN32_WCE
#include <conio.h>
#endif
#ifndef  _WIN32_WCE
#include <signal.h>
#include <direct.h>
#else
#ifndef HTS_CECOMPAT
#include "signal.h"
#endif
#endif
#else
#include <signal.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#endif
/* END specific definitions */


// Include htslib.h for all types
#include "htslib.h"

#include "htsopt.h"

// structure d'un lien
typedef struct lien_url {
  char firstblock;      // flag 1=premier malloc 
  char link_import;     // lien importé à la suite d'un moved - ne pas appliquer les règles classiques up/down
  int depth;            // profondeur autorisée lien ; >0 forte 0=faible
  int pass2;            // traiter après les autres, seconde passe. si == -1, lien traité en background
  int premier;          // pointeur sur le premier lien qui a donné lieu aux autres liens du domaine
  int precedent;        // pointeur sur le lien qui a donné lieu à ce lien précis
  //int moved;          // pointeur sur moved
  int retry;            // nombre de retry restants
  int testmode;         // mode test uniquement, envoyer juste un head!
  char* adr;            // adresse
  char* fil;            // nom du fichier distant
  char* sav;            // nom à sauver sur disque (avec chemin éventuel)
  char* cod;            // chemin codebase éventuel si classe java
  char* former_adr;     // adresse initiale (avant éventuel moved), peut être nulle
  char* former_fil;     // nom du fichier distant initial (avant éventuel moved), peut être nul
  // pour optimisation:
#if HTS_HASH
  int hash_next[3];     // prochain lien avec même valeur hash
#else
  int sav_len;          // taille de sav
#endif
} lien_url;

// chargement de fichiers en 'arrière plan'
typedef struct lien_back {
#if DEBUG_CHECKINT
  char magic;
#endif
  char url_adr[HTS_URLMAXSIZE*2];     // adresse
  char url_fil[HTS_URLMAXSIZE*2];     // nom du fichier distant
  char url_sav[HTS_URLMAXSIZE*2];     // nom à sauver sur disque (avec chemin éventuel)
  char referer_adr[HTS_URLMAXSIZE*2]; // adresse host page referer
  char referer_fil[HTS_URLMAXSIZE*2]; // fichier page referer
  char location_buffer[HTS_URLMAXSIZE*2];  // "location" en cas de "moved" (302,..)
  char* tmpfile;                           // nom à sauver temporairement (compressé)
  char tmpfile_buffer[HTS_URLMAXSIZE*2];   // buffer pour le nom à sauver temporairement
  char send_too[1024];    // données à envoyer en même temps que le header
  int status;             // status (-1=non utilisé, 0: prêt, >0: opération en cours)
  int testmode;           // mode de test
  int timeout;            // gérer des timeouts? (!=0  : nombre de secondes)
  TStamp timeout_refresh; // si oui, time refresh
  int rateout;            // timeout refresh? (!=0 : taux minimum toléré en octets/s)
  TStamp rateout_time;    // si oui, date de départ
  LLint maxfile_nonhtml;  // taille max d'un fichier non html
  LLint maxfile_html;     // idem pour un ficheir html
  htsblk r;               // structure htsblk de chaque objet en background 
  int is_update;          // mode update
  int head_request;       // requète HEAD?
  LLint range_req_size;   // range utilisé
  TStamp ka_time_start;   // refresh time for KA 
  //
  int http11;             // L'en tête doit être signé HTTP/1.1 et non HTTP/1.0
  int is_chunk;           // chunk?
  char* chunk_adr;        // adresse chunk en cours de chargement
  LLint chunk_size;       // taille chunk en cours de chargement
  LLint chunk_blocksize;  // taille data declaree par le chunk
  LLint compressed_size;  // taille compressés (stats uniquement)
  //
  int* pass2_ptr;         // pointeur sur liens[ptr]->pass2
  //
  char info[256];         // éventuel status pour le ftp
  int stop_ftp;           // flag stop pour ftp
  int finalized;          // finalized (optim memory)
#if DEBUG_CHECKINT
  char magic2;
#endif
} lien_back;

typedef struct cache_back_zip_entry cache_back_zip_entry;

// cache
typedef struct cache_back {
  int version;        // 0 ou 1
  /* */
  int type;
  int ro;
  FILE *dat,*ndx,*olddat;
  char *use;      // liste des adr+fil
  FILE *lst;      // liste des fichiers pour la "purge"
  FILE *txt;      // liste des fichiers (info)
  char lastmodified[256];
  // HASH
  void* hashtable;
  // HASH for tests (naming subsystem)
  void* cached_tests;
  // fichiers log optionnels
  FILE* log;
  FILE* errlog;
  // variables
  int ptr_ant;      // pointeur pour anticiper
  int ptr_last;     // pointeur pour anticiper
  //
  void* zipInput;
  void* zipOutput;
  cache_back_zip_entry* zipEntries;
  int zipEntriesOffs;
  int zipEntriesCapa;
} cache_back;

typedef struct hash_struct {
  lien_url** liens;                     // pointeur sur liens
  int max_lien;                         // indice le plus grand rencontré
  int hash[3][HTS_HASH_SIZE];           // tables pour sav/adr-fil/former_adr-former_fil
} hash_struct;

#if HTS_HASH
#else
#define hash_write(A,B)
#endif

typedef struct filecreate_params {
  FILE* lst;
  char path[HTS_URLMAXSIZE*2];
} filecreate_params;

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE

static int cache_writable(cache_back* cache) {
  return (cache != NULL && ( cache->dat != NULL || cache->zipOutput != NULL ) );
}

static int cache_readable(cache_back* cache) {
  return (cache != NULL && ( cache->olddat != NULL || cache->zipInput != NULL ) );
}

#endif

// Fonctions

// INCLUDES .H PARTIES DE CODE HTTRACK

// routine main
#include "htscoremain.h"

// divers outils pour httrack.c
#include "htstools.h"

// aide pour la version en ligne de commande
#include "htshelp.h"

// génération du nom de fichier à sauver
#include "htsname.h"

// gestion ftp
#include "htsftp.h"

// routine parser java
#include "htsjava.h"

// gestion interception d'URL
#include "htscatchurl.h"

// gestion robots.txt
#include "htsrobots.h"

// routines d'acceptation de liens
#include "htswizard.h"

// routines de regexp
#include "htsfilters.h"

// gestion backing
#include "htsback.h"

// gestion cache
#include "htscache.h"

// gestion hashage
#include "htshash.h"
#include "htsinthash.h"

// gestion réentrance
#include "htsnostatic.h"

// infos console
#if HTS_ANALYSTE_CONSOLE
#include "httrack.h"
#endif

#include "htsdefines.h"

#include "hts-indextmpl.h"

// INCLUDES .H PARTIES DE CODE HTTRACK

//

/*
typedef void  (* t_hts_htmlcheck_init)(void);
typedef void  (* t_hts_htmlcheck_uninit)(void);
typedef int   (* t_hts_htmlcheck_start)(httrackp* opt);
typedef int   (* t_hts_htmlcheck_end)(void);
typedef int   (* t_hts_htmlcheck_chopt)(httrackp* opt);
typedef int   (* t_hts_htmlcheck_process)(char** html,int* len,char* url_adresse,char* url_fichier);
typedef int   (* t_hts_htmlcheck)(char* html,int len,char* url_adresse,char* url_fichier);
typedef char* (* t_hts_htmlcheck_query)(char* question);
typedef char* (* t_hts_htmlcheck_query2)(char* question);
typedef char* (* t_hts_htmlcheck_query3)(char* question);
typedef int   (* t_hts_htmlcheck_loop)(lien_back* back,int back_max,int back_index,int lien_tot,int lien_ntot,int stat_time,hts_stat_struct* stats);
typedef int   (* t_hts_htmlcheck_check)(char* adr,char* fil,int status);
typedef void  (* t_hts_htmlcheck_pause)(char* lockfile);
typedef void  (* t_hts_htmlcheck_filesave)(char* file);
typedef int   (* t_hts_htmlcheck_linkdetected)(char* link);
typedef int   (* t_hts_htmlcheck_linkdetected2)(char* link, char* tag_start);
typedef int   (* t_hts_htmlcheck_xfrstatus)(lien_back* back);
typedef int   (* t_hts_htmlcheck_savename)(char* adr_complete,char* fil_complete,char* referer_adr,char* referer_fil,char* save);
typedef int   (* t_hts_htmlcheck_sendhead)(char* buff, char* adr, char* fil, char* referer_adr, char* referer_fil, htsblk* outgoing);
typedef int   (* t_hts_htmlcheck_receivehead)(char* buff, char* adr, char* fil, char* referer_adr, char* referer_fil, htsblk* incoming);
*/

// demande d'interaction avec le shell
#if HTS_ANALYSTE
//char HTbuff[1024];
/*
extern t_hts_htmlcheck_init       hts_htmlcheck_init;
extern t_hts_htmlcheck_uninit     hts_htmlcheck_uninit;
extern t_hts_htmlcheck_start      hts_htmlcheck_start;
extern t_hts_htmlcheck_end        hts_htmlcheck_end;
extern t_hts_htmlcheck_chopt      hts_htmlcheck_chopt;
extern t_hts_htmlcheck_process    hts_htmlcheck_preprocess;
extern t_hts_htmlcheck_process    hts_htmlcheck_postprocess;
extern t_hts_htmlcheck            hts_htmlcheck;
extern t_hts_htmlcheck_query      hts_htmlcheck_query;
extern t_hts_htmlcheck_query2     hts_htmlcheck_query2;
extern t_hts_htmlcheck_query3     hts_htmlcheck_query3;
extern t_hts_htmlcheck_loop       hts_htmlcheck_loop;
extern t_hts_htmlcheck_check      hts_htmlcheck_check;
extern t_hts_htmlcheck_pause      hts_htmlcheck_pause;
extern t_hts_htmlcheck_filesave   hts_htmlcheck_filesave;
extern t_hts_htmlcheck_linkdetected hts_htmlcheck_linkdetected;
extern t_hts_htmlcheck_linkdetected2 hts_htmlcheck_linkdetected2;
extern t_hts_htmlcheck_xfrstatus  hts_htmlcheck_xfrstatus;
extern t_hts_htmlcheck_savename   hts_htmlcheck_savename;
extern t_hts_htmlcheck_sendhead   hts_htmlcheck_sendhead;
extern t_hts_htmlcheck_receivehead hts_htmlcheck_receivehead;
*/

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE

//
#ifndef HTTRACK_DEFLIB
HTSEXT_API int hts_is_parsing(int flag);
HTSEXT_API int hts_is_testing(void);
HTSEXT_API int hts_is_exiting(void);
HTSEXT_API int hts_setopt(httrackp* opt);
HTSEXT_API int hts_addurl(char** url);
HTSEXT_API int hts_resetaddurl(void);
HTSEXT_API int copy_htsopt(httrackp* from,httrackp* to);
HTSEXT_API char* hts_errmsg(void);
HTSEXT_API int hts_setpause(int);      // pause transfer
HTSEXT_API int hts_request_stop(int force);
//
HTSEXT_API char* hts_cancel_file(char * s);
HTSEXT_API void hts_cancel_test(void);
HTSEXT_API void hts_cancel_parsing(void);
#endif
//
// Variables globales
extern int _hts_in_html_parsing;
extern int _hts_in_html_done;  // % réalisés
extern int _hts_in_html_poll;  // parsing
extern char _hts_errmsg[1100];
extern int _hts_setpause;
//extern httrackp* _hts_setopt;
extern char** _hts_addurl;
extern int _hts_cancel;
#endif

//


//int httpmirror(char* url,int level,httrackp opt);
int httpmirror(char* url1,httrackp* opt);
int filesave(httrackp* opt,char* adr,int len,char* s,char* url_adr /* = NULL */,char* url_fil /* = NULL */);
int check_fatal_io_errno(void);
int engine_stats(void);
void host_ban(httrackp* opt,lien_url** liens,int ptr,int lien_tot,lien_back* back,int back_max,char* host);
FILE* filecreate(char* s);
int filecreateempty(char* filename);
int filenote(char* s,filecreate_params* params);
HTS_INLINE void usercommand(httrackp* opt,int exe,char* cmd,char* file,char* adr,char* fil);
void usercommand_exe(char* cmd,char* file);
//void* structcheck_init(int init);
int filters_init(char*** ptrfilters, int maxfilter, int filterinc);
#ifndef HTTRACK_DEFLIB
HTSEXT_API int structcheck(char* s);
#endif
HTS_INLINE int fspc(FILE* fp,char* type);
char* next_token(char* p,int flag);
//
char* readfile(char* fil);
char* readfile_or(char* fil,char* defaultdata);
#if 0
void check_rate(TStamp stat_timestart,int maxrate);
#endif

// liens
int liens_record(char* adr,char* fil,char* save,char* former_adr,char* former_fil,char* codebase);


// backing, routines externes
int back_pluggable_sockets(lien_back* back, int back_max, httrackp* opt);
int back_pluggable_sockets_strict(lien_back* back, int back_max, httrackp* opt);
int back_fill(lien_back* back,int back_max,httrackp* opt,cache_back* cache,lien_url** liens,int ptr,int numero_passe,int lien_tot);
int backlinks_done(lien_url** liens,int lien_tot,int ptr);
int back_fillmax(lien_back* back,int back_max,httrackp* opt,cache_back* cache,lien_url** liens,int ptr,int numero_passe,int lien_tot);

// cancel file
#if HTS_ANALYSTE
#ifndef HTTRACK_DEFLIB
HTSEXT_API char* hts_cancel_file(char * s);
HTSEXT_API void hts_cancel_test(void);
HTSEXT_API void hts_cancel_parsing(void);
#endif
#endif

int ask_continue(void);
int nombre_digit(int n);

// Java
#if 0
int hts_add_file(char* file,int file_position);
#endif

// Polling
#if HTS_POLL
HTS_INLINE int check_flot(T_SOC s);
HTS_INLINE int check_stdin(void);
int read_stdin(char* s,int max);
#endif
HTS_INLINE int check_sockerror(T_SOC s);
HTS_INLINE int check_sockdata(T_SOC s);

httrackp* hts_declareoptbuffer(httrackp* optdecl);
void sig_finish( int code );     // finir et quitter
void sig_term( int code );       // quitter
#if HTS_WIN
void sig_ask( int code );        // demander
#else
void sig_back( int code );       // ignorer et mettre en backing 
void sig_ask( int code );        // demander
void sig_ignore( int code );     // ignorer signal
void sig_brpipe( int code );     // treat if necessary
void sig_doback(int);            // mettre en arrière plan
#endif

/* external modules */
int htsAddLink(htsmoduleStruct* str, char* link);

// Void
void voidf(void);

#define HTS_TOPINDEX "TOP_INDEX_HTTRACK"

#endif

#endif


