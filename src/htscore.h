/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998-2015 Xavier Roche and other contributors

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.

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
#include <conio.h>
#include <direct.h>
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
#include "coucal.h"

#include "htsdefines.h"

#include "hts-indextmpl.h"

// adr, fil
#ifndef HTS_DEF_FWSTRUCT_lien_adrfil
#define HTS_DEF_FWSTRUCT_lien_adrfil
typedef struct lien_adrfil lien_adrfil;
#endif
struct lien_adrfil {
  char adr[HTS_URLMAXSIZE * 2];      // adresse
  char fil[HTS_URLMAXSIZE * 2];      // nom du fichier distant
};

// adr, fil, save
#ifndef HTS_DEF_FWSTRUCT_lien_adrfilsave
#define HTS_DEF_FWSTRUCT_lien_adrfilsave
typedef struct lien_adrfilsave lien_adrfilsave;
#endif
struct lien_adrfilsave {
  lien_adrfil af;
  char save[HTS_URLMAXSIZE * 2];     // nom à sauver sur disque (avec chemin éventuel)
};

#ifndef HTS_DEF_FWSTRUCT_struct_back
#define HTS_DEF_FWSTRUCT_struct_back
typedef struct struct_back struct_back;
#endif
struct struct_back {
  lien_back *lnk;
  int count;
  coucal ready;
  LLint ready_size_bytes;
};

typedef struct cache_back_zip_entry cache_back_zip_entry;

// cache
#ifndef HTS_DEF_FWSTRUCT_cache_back
#define HTS_DEF_FWSTRUCT_cache_back
typedef struct cache_back cache_back;
#endif
struct cache_back {
  int version;                  // 0 ou 1
  /* */
  int type;
  int ro;
  FILE *dat, *ndx, *olddat;
  char *use;                    // liste des adr+fil
  FILE *lst;                    // liste des fichiers pour la "purge"
  FILE *txt;                    // liste des fichiers (info)
  char lastmodified[256];
  // HASH
  coucal hashtable;
  // HASH for tests (naming subsystem)
  coucal cached_tests;
  // fichiers log optionnels
  FILE *log;
  FILE *errlog;
  // variables
  int ptr_ant;                  // pointeur pour anticiper
  int ptr_last;                 // pointeur pour anticiper
  //
  void *zipInput;
  void *zipOutput;
  cache_back_zip_entry *zipEntries;
  int zipEntriesOffs;
  int zipEntriesCapa;
};

#ifndef HTS_DEF_FWSTRUCT_hash_struct
#define HTS_DEF_FWSTRUCT_hash_struct
typedef struct hash_struct hash_struct;
#endif
struct hash_struct {
  /* Links big array reference */
  const lien_url *const*const*liens;
  /* Savename (case insensitive ; lowercased) */
  coucal sav;
  /* Address and path */
  coucal adrfil;
  /* Former address and path */
  coucal former_adrfil;
  /** Buffers **/
  int normalized;
  char normfil[HTS_URLMAXSIZE * 2];
  char normfil2[HTS_URLMAXSIZE * 2];
  char catbuff[CATBUFF_SIZE];
};

#ifndef HTS_DEF_FWSTRUCT_filecreate_params
#define HTS_DEF_FWSTRUCT_filecreate_params
typedef struct filecreate_params filecreate_params;
#endif
struct filecreate_params {
  FILE *lst;
  char path[HTS_URLMAXSIZE * 2];
};

/* Access macros. */
#define heap(N)            (opt->liens[N])
#define heap_top_index()   (opt->lien_tot - 1)
#define heap_top()         (heap(heap_top_index()))
#define urladr()           (heap(ptr)->adr)
#define urlfil()           (heap(ptr)->fil)
#define savename()         (heap(ptr)->sav)
#define parenturladr()     (heap(heap(ptr)->precedent)->adr)
#define parenturlfil()     (heap(heap(ptr)->precedent)->fil)
#define parentsavename()   (heap(heap(ptr)->precedent)->sav)
#define relativeurladr()   ((!parent_relative)?urladr():parenturladr())
#define relativeurlfil()   ((!parent_relative)?urlfil():parenturlfil())
#define relativesavename() ((!parent_relative)?savename():parentsavename())

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE

HTS_STATIC int cache_writable(cache_back * cache) {
  return (cache != NULL && (cache->dat != NULL || cache->zipOutput != NULL));
}

HTS_STATIC int cache_readable(cache_back * cache) {
  return (cache != NULL && (cache->olddat != NULL || cache->zipInput != NULL));
}

#endif

// Fonctions

// INCLUDES .H PARTIES DE CODE HTTRACK

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE

char *hts_cancel_file_pop(httrackp * opt);

#endif

// add a link on the heap
int hts_record_link(httrackp * opt,
                    const char *address, const char *file, const char *save,
                    const char *ref_address, const char *ref_file,
                    const char *codebase);

// index of the latest added link
size_t hts_record_link_latest(httrackp *opt);

// invalidate an entry
void hts_invalidate_link(httrackp * opt, int lpos);

// wipe all records
void hts_record_init(httrackp *opt);
void hts_record_free(httrackp *opt);

//int httpmirror(char* url,int level,httrackp opt);
int httpmirror(char *url1, httrackp * opt);
int filesave(httrackp * opt, const char *adr, int len, const char *s,
             const char *url_adr /* = NULL */ ,
             const char *url_fil /* = NULL */ );
char *hts_cancel_file_pop(httrackp * opt);
int check_fatal_io_errno(void);
int engine_stats(void);
void host_ban(httrackp * opt, int ptr, struct_back * sback, const char *host);
FILE *filecreate(filenote_strc * strct, const char *s);
FILE *fileappend(filenote_strc * strct, const char *s);
int filecreateempty(filenote_strc * strct, const char *filename);
int filenote(filenote_strc * strct, const char *s, filecreate_params * params);
void file_notify(httrackp * opt, const char *adr, const char *fil,
                 const char *save, int create, int modify, int wasupdated);
void usercommand(httrackp * opt, int exe, const char *cmd, const char *file,
                 const char *adr, const char *fil);
void usercommand_exe(const char *cmd, const char *file);
int filters_init(char ***ptrfilters, int maxfilter, int filterinc);

int fspc(httrackp * opt, FILE * fp, const char *type);
char *next_token(char *p, int flag);

//
char *readfile(const char *fil);
char *readfile2(const char *fil, LLint * size);
char *readfile_utf8(const char *fil);
char *readfile_or(const char *fil, const char *defaultdata);

#if 0
void check_rate(TStamp stat_timestart, int maxrate);
#endif

// liens
int liens_record(char *adr, char *fil, char *save, char *former_adr,
                 char *former_fil, char *codebase);

// backing, routines externes
int back_pluggable_sockets(struct_back * sback, httrackp * opt);
int back_pluggable_sockets_strict(struct_back * sback, httrackp * opt);
int back_fill(struct_back * sback, httrackp * opt, cache_back * cache,
              int ptr, int numero_passe);
int backlinks_done(const struct_back * sback, lien_url ** liens, 
                   int lien_tot, int ptr);
int back_fillmax(struct_back * sback, httrackp * opt, cache_back * cache,
                 int ptr, int numero_passe);

int ask_continue(httrackp * opt);
int nombre_digit(int n);

// Java
#if 0
int hts_add_file(char *file, int file_position);
#endif

// Polling
#if HTS_POLL
int check_flot(T_SOC s);
int check_stdin(void);
int read_stdin(char *s, int max);
#endif
int check_sockerror(T_SOC s);
int check_sockdata(T_SOC s);

/* external modules */
int htsAddLink(htsmoduleStruct * str, char *link);

// Void
void voidf(void);
#define HTS_TOPINDEX "TOP_INDEX_HTTRACK"

#endif
