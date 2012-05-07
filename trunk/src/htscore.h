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
/* File: Main file .h                                           */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

// Fichier librairie .h
#ifndef HTS_CORE_DEFH
#define HTS_CORE_DEFH

#include "htsglobal.h"

/* specific definitions */
#include "htsbase.h"
// Includes & définitions
#include <sys/types.h>
#include <sys/stat.h>
#ifdef _WIN32
#ifndef  _WIN32_WCE
#include <conio.h>
#endif
#ifndef  _WIN32_WCE
#include <direct.h>
#endif
#else
#ifndef _WIN32
#include <unistd.h>
#endif
#endif
/* END specific definitions */

/* Forward definitions */
#ifndef HTS_DEF_FWSTRUCT_lien_url
#define HTS_DEF_FWSTRUCT_lien_url
typedef struct lien_url lien_url;
#endif
#ifndef HTS_DEF_FWSTRUCT_lien_back
#define HTS_DEF_FWSTRUCT_lien_back
typedef struct lien_back lien_back;
#endif
#ifndef HTS_DEF_FWSTRUCT_struct_back
#define HTS_DEF_FWSTRUCT_struct_back
typedef struct struct_back struct_back;
#endif
#ifndef HTS_DEF_FWSTRUCT_cache_back
#define HTS_DEF_FWSTRUCT_cache_back
typedef struct cache_back cache_back;
#endif
#ifndef HTS_DEF_FWSTRUCT_hash_struct
#define HTS_DEF_FWSTRUCT_hash_struct
typedef struct hash_struct hash_struct;
#endif
#ifndef HTS_DEF_FWSTRUCT_filecreate_params
#define HTS_DEF_FWSTRUCT_filecreate_params
typedef struct filecreate_params filecreate_params;
#endif

// Include htslib.h for all types
#include "htslib.h"

// options
#include "htsopt.h"

// INCLUDES .H PARTIES DE CODE HTTRACK

// routine main
#include "htscoremain.h"

// core routines
#include "htscore.h"

// divers outils pour httrack.c
#include "htstools.h"

// aide pour la version en ligne de commande
#include "htshelp.h"

// génération du nom de fichier à sauver
#include "htsname.h"

// gestion ftp
#include "htsftp.h"

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

#include "htsdefines.h"

#include "hts-indextmpl.h"

// structure d'un lien
#ifndef HTS_DEF_FWSTRUCT_lien_url
#define HTS_DEF_FWSTRUCT_lien_url
typedef struct lien_url lien_url;
#endif
struct lien_url {
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
  int hash_next[3];     // prochain lien avec même valeur hash
};

// chargement de fichiers en 'arrière plan'
#ifndef HTS_DEF_FWSTRUCT_lien_back
#define HTS_DEF_FWSTRUCT_lien_back
typedef struct lien_back lien_back;
#endif
struct lien_back {
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
  int locked;             // locked (to be used soon)
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
  //int links_index;        // to access liens[links_index]
  //
  char info[256];         // éventuel status pour le ftp
  int stop_ftp;           // flag stop pour ftp
  int finalized;          // finalized (optim memory)
  int early_add;          // was added before link heap saw it
#if DEBUG_CHECKINT
  char magic2;
#endif
};

#ifndef HTS_DEF_FWSTRUCT_struct_back
#define HTS_DEF_FWSTRUCT_struct_back
typedef struct struct_back struct_back;
#endif
struct struct_back {
  lien_back* lnk;
  int count;
  inthash ready;
	LLint ready_size_bytes;
};

typedef struct cache_back_zip_entry cache_back_zip_entry;

// cache
#ifndef HTS_DEF_FWSTRUCT_cache_back
#define HTS_DEF_FWSTRUCT_cache_back
typedef struct cache_back cache_back;
#endif
struct cache_back {
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
  inthash hashtable;
  // HASH for tests (naming subsystem)
  inthash cached_tests;
  // fichiers log optionnels
  FILE* log;
  FILE* errlog;
  // variables
  int ptr_ant;      // pointeur pour anticiper
  int ptr_last;     // pointeur pour anticiper
  //
  void *zipInput;
  void *zipOutput;
  cache_back_zip_entry* zipEntries;
  int zipEntriesOffs;
  int zipEntriesCapa;
};

#ifndef HTS_DEF_FWSTRUCT_hash_struct
#define HTS_DEF_FWSTRUCT_hash_struct
typedef struct hash_struct hash_struct;
#endif
struct hash_struct {
  lien_url** liens;                     // pointeur sur liens
  int max_lien;                         // indice le plus grand rencontré
  int hash[3][HTS_HASH_SIZE];           // tables pour sav/adr-fil/former_adr-former_fil
};

#ifndef HTS_DEF_FWSTRUCT_filecreate_params
#define HTS_DEF_FWSTRUCT_filecreate_params
typedef struct filecreate_params filecreate_params;
#endif
struct filecreate_params {
  FILE* lst;
  char path[HTS_URLMAXSIZE*2];
};

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE

HTS_STATIC int cache_writable(cache_back* cache) {
  return (cache != NULL && ( cache->dat != NULL || cache->zipOutput != NULL ) );
}

HTS_STATIC int cache_readable(cache_back* cache) {
  return (cache != NULL && ( cache->olddat != NULL || cache->zipInput != NULL ) );
}

#endif

// Fonctions

// INCLUDES .H PARTIES DE CODE HTTRACK

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE

//
#ifndef HTTRACK_DEFLIB
HTSEXT_API int hts_is_parsing(httrackp *opt, int flag);
HTSEXT_API int hts_is_testing(httrackp *opt);
HTSEXT_API int hts_addurl(httrackp *opt, char** url);
HTSEXT_API int hts_resetaddurl(httrackp *opt);
HTSEXT_API int copy_htsopt(const httrackp* from,httrackp* to);
HTSEXT_API char* hts_errmsg(httrackp *opt);
HTSEXT_API int hts_setpause(httrackp *opt, int);
//
HTSEXT_API int hts_is_exiting(httrackp *opt);
HTSEXT_API int hts_request_stop(httrackp* opt, int force);
//
HTSEXT_API int hts_cancel_file_push(httrackp *opt, const char *url);
HTSEXT_API void hts_cancel_test(httrackp *opt);
HTSEXT_API void hts_cancel_parsing(httrackp *opt);
#endif

char* hts_cancel_file_pop(httrackp *opt);

#endif

//


//int httpmirror(char* url,int level,httrackp opt);
int httpmirror(char* url1,httrackp* opt);
int filesave(httrackp* opt,const char* adr,int len,const char* s,const char* url_adr /* = NULL */,const char* url_fil /* = NULL */);
char* hts_cancel_file_pop(httrackp *opt);
int check_fatal_io_errno(void);
int engine_stats(void);
void host_ban(httrackp* opt,lien_url** liens,int ptr,int lien_tot,struct_back* sback,char* host);
FILE* filecreate(filenote_strc *strct,const char* s);
FILE* fileappend(filenote_strc *strct,const char* s);
int filecreateempty(filenote_strc *strct, const char* filename);
int filenote(filenote_strc *strct,const char* s,filecreate_params* params);
void file_notify(httrackp* opt,const char* adr,const char* fil,const char* save,int create,int modify,int wasupdated);
HTS_INLINE void usercommand(httrackp* opt,int exe,const char* cmd,const char* file,const char* adr,const char* fil);
void usercommand_exe(const char* cmd,const char* file);
int filters_init(char*** ptrfilters, int maxfilter, int filterinc);
#ifndef HTTRACK_DEFLIB
HTSEXT_API int structcheck(const char* path);
HTSEXT_API int structcheck_utf8(const char* path);
HTSEXT_API int dir_exists(const char* path);
#endif
HTS_INLINE int fspc(httrackp *opt,FILE* fp,const char* type);
char* next_token(char* p,int flag);
//
char* readfile(char* fil);
char* readfile2(char* fil, LLint* size);
char* readfile_utf8(char* fil);
char* readfile_or(char* fil,char* defaultdata);
#if 0
void check_rate(TStamp stat_timestart,int maxrate);
#endif

// liens
int liens_record(char* adr,char* fil,char* save,char* former_adr,char* former_fil,char* codebase);


// backing, routines externes
int back_pluggable_sockets(struct_back* sback, httrackp* opt);
int back_pluggable_sockets_strict(struct_back* sback, httrackp* opt);
int back_fill(struct_back* sback,httrackp* opt,cache_back* cache,lien_url** liens,int ptr,int numero_passe,int lien_tot);
int backlinks_done(struct_back* sback,lien_url** liens,int lien_tot,int ptr);
int back_fillmax(struct_back* sback,httrackp* opt,cache_back* cache,lien_url** liens,int ptr,int numero_passe,int lien_tot);

// cancel file
#ifndef HTTRACK_DEFLIB
HTSEXT_API int hts_cancel_file_push(httrackp *opt, const char *url);
HTSEXT_API void hts_cancel_test(httrackp *opt);
HTSEXT_API void hts_cancel_parsing(httrackp *opt);
#endif

int ask_continue(httrackp *opt);
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

/* external modules */
int htsAddLink(htsmoduleStruct* str, char* link);

// Void
void voidf(void);

#define HTS_TOPINDEX "TOP_INDEX_HTTRACK"

#endif
