/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998-2017 Xavier Roche and other contributors

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
/* File: HTTrack parameters block                               */
/*       Called by httrack.h and some other files               */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTTRACK_DEFOPT
#define HTTRACK_DEFOPT

#include <stdio.h>

#include "htsglobal.h"
#include "htsnet.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward definitions */
#ifndef HTS_DEF_FWSTRUCT_t_hts_htmlcheck_callbacks
#define HTS_DEF_FWSTRUCT_t_hts_htmlcheck_callbacks
typedef struct t_hts_htmlcheck_callbacks t_hts_htmlcheck_callbacks;
#endif
#ifndef HTS_DEF_FWSTRUCT_t_dnscache
#define HTS_DEF_FWSTRUCT_t_dnscache
typedef struct t_dnscache t_dnscache;
#endif
#ifndef HTS_DEF_FWSTRUCT_hash_struct
#define HTS_DEF_FWSTRUCT_hash_struct
typedef struct hash_struct hash_struct;
#endif
#ifndef HTS_DEF_FWSTRUCT_robots_wizard
#define HTS_DEF_FWSTRUCT_robots_wizard
typedef struct robots_wizard robots_wizard;
#endif
#ifndef HTS_DEF_FWSTRUCT_t_cookie
#define HTS_DEF_FWSTRUCT_t_cookie
typedef struct t_cookie t_cookie;
#endif

/** Forward definitions **/
#ifndef HTS_DEF_FWSTRUCT_String
#define HTS_DEF_FWSTRUCT_String
typedef struct String String;
#endif
#ifndef HTS_DEF_STRUCT_String
#define HTS_DEF_STRUCT_String
struct String {
  char *buffer_;
  size_t length_;
  size_t capacity_;
};
#endif

/* Defines */
#define CATBUFF_SIZE (STRING_SIZE*2*2)
#define STRING_SIZE 2048

/* Proxy structure */
#ifndef HTS_DEF_FWSTRUCT_t_proxy
#define HTS_DEF_FWSTRUCT_t_proxy
typedef struct t_proxy t_proxy;
#endif
struct t_proxy {
  int active;
  String name;
  int port;
  String bindhost;              // bind this host
};

/* Structure utile pour copier en bloc les paramètres */
#ifndef HTS_DEF_FWSTRUCT_htsfilters
#define HTS_DEF_FWSTRUCT_htsfilters
typedef struct htsfilters htsfilters;
#endif
struct htsfilters {
  char ***filters;
  int *filptr;
  //int*    filter_max;
};

/* User callbacks chain */
typedef int (*htscallbacksfncptr) (void);
typedef struct htscallbacks htscallbacks;
struct htscallbacks {
  void *moduleHandle;
  htscallbacksfncptr exitFnc;
  htscallbacks *next;
};

/* filenote() internal file structure */
#ifndef HTS_DEF_FWSTRUCT_filenote_strc
#define HTS_DEF_FWSTRUCT_filenote_strc
typedef struct filenote_strc filenote_strc;
#endif
struct filenote_strc {
  FILE *lst;
  char path[STRING_SIZE * 2];
};

/* concat() functions */
#ifndef HTS_DEF_FWSTRUCT_concat_strc
#define HTS_DEF_FWSTRUCT_concat_strc
typedef struct concat_strc concat_strc;
#endif
struct concat_strc {
  int index;
  char buff[16][STRING_SIZE * 2 * 2];
};

/* int2 functions */
#ifndef HTS_DEF_FWSTRUCT_strc_int2bytes2
#define HTS_DEF_FWSTRUCT_strc_int2bytes2
typedef struct strc_int2bytes2 strc_int2bytes2;
#endif
struct strc_int2bytes2 {
  char catbuff[CATBUFF_SIZE];
  char buff1[256];
  char buff2[32];
  char *buffadr[2];
};

/* cmd callback */
#ifndef HTS_DEF_FWSTRUCT_usercommand_strc
#define HTS_DEF_FWSTRUCT_usercommand_strc
typedef struct usercommand_strc usercommand_strc;
#endif
struct usercommand_strc {
  int exe;
  char cmd[2048];
};

/* error logging */
#ifndef HTS_DEF_FWSTRUCT_fspc_strc
#define HTS_DEF_FWSTRUCT_fspc_strc
typedef struct fspc_strc fspc_strc;
#endif
struct fspc_strc {
  int error;
  int warning;
  int info;
};

/* lien_url */
#ifndef HTS_DEF_FWSTRUCT_lien_url
#define HTS_DEF_FWSTRUCT_lien_url
typedef struct lien_url lien_url;
#endif

#ifndef HTS_DEF_DEFSTRUCT_hts_log_type
#define HTS_DEF_DEFSTRUCT_hts_log_type
typedef enum hts_log_type {
  LOG_PANIC,
  LOG_ERROR,
  LOG_WARNING,
  LOG_NOTICE,
  LOG_INFO,
  LOG_DEBUG,
  LOG_TRACE,
  LOG_ERRNO = 1 << 8
} hts_log_type;
#endif

/* Structure état du miroir */
#ifndef HTS_DEF_FWSTRUCT_htsoptstatecancel
#define HTS_DEF_FWSTRUCT_htsoptstatecancel
typedef struct htsoptstatecancel htsoptstatecancel;
#endif
struct htsoptstatecancel {
  char *url;
  htsoptstatecancel *next;
};

/* Mutexes */
#ifndef HTS_DEF_FWSTRUCT_htsmutex_s
#define HTS_DEF_FWSTRUCT_htsmutex_s
typedef struct htsmutex_s htsmutex_s, *htsmutex;
#endif

/* Hashtables */
#ifndef HTS_DEF_FWSTRUCT_struct_coucal
#define HTS_DEF_FWSTRUCT_struct_coucal
typedef struct struct_coucal struct_coucal, *coucal;
#endif

/* Structure état du miroir */
#ifndef HTS_DEF_FWSTRUCT_htsoptstate
#define HTS_DEF_FWSTRUCT_htsoptstate
typedef struct htsoptstate htsoptstate;
#endif
struct htsoptstate {
  htsmutex lock;                /* 3.41 */
  /* */
  int stop;
  int exit_xh;
  int back_add_stats;
  /* */
  int mimehtml_created;
  String mimemid;
  FILE *mimefp;
  int delayedId;
  /* */
  filenote_strc strc;
  /* Functions context (avoir thread variables!) */
  htscallbacks callbacks;
  concat_strc concat;
  usercommand_strc usercmd;
  fspc_strc fspc;
  char *userhttptype;
  int verif_backblue_done;
  int verif_external_status;
  t_dnscache *dns_cache;
  int dns_cache_nthreads;
  /* HTML parsing state */
  char _hts_errmsg[HTS_CDLMAXSIZE + 256];
  int _hts_in_html_parsing;
  int _hts_in_html_done;
  int _hts_in_html_poll;
  int _hts_setpause;
  int _hts_in_mirror;
  char **_hts_addurl;
  int _hts_cancel;
  htsoptstatecancel *cancel;    /* 3.41 */
  char HTbuff[2048];
  unsigned int debug_state;
  unsigned int tmpnameid;       /* 3.41 */
  int is_ended;                 /* 3.48-14 */
};

/* Library handles */
#ifndef HTS_DEF_FWSTRUCT_htslibhandles
#define HTS_DEF_FWSTRUCT_htslibhandles
typedef struct htslibhandles htslibhandles;
#endif
#ifndef HTS_DEF_FWSTRUCT_htslibhandle
#define HTS_DEF_FWSTRUCT_htslibhandle
typedef struct htslibhandle htslibhandle;
#endif
struct htslibhandle {
  char *moduleName;
  void *handle;
};
struct htslibhandles {
  int count;
  htslibhandle *handles;
};

/* Javascript parser flags */
typedef enum htsparsejava_flags {
  HTSPARSE_NONE = 0,            // don't parse
  HTSPARSE_DEFAULT = 1,         // parse default (all)
  HTSPARSE_NO_CLASS = 2,        // don't parse .java
  HTSPARSE_NO_JAVASCRIPT = 4,   // don't parse .js
  HTSPARSE_NO_AGGRESSIVE = 8    // don't aggressively parse .js or .java
} htsparsejava_flags;

#ifndef HTS_DEF_FWSTRUCT_lien_buffers
#define HTS_DEF_FWSTRUCT_lien_buffers
typedef struct lien_buffers lien_buffers;
#endif

// paramètres httrack (options)
#ifndef HTS_DEF_FWSTRUCT_httrackp
#define HTS_DEF_FWSTRUCT_httrackp
typedef struct httrackp httrackp;
#endif
struct httrackp {
  size_t size_httrackp;         // size of this structure
  /* */
  int wizard;                   // wizard aucun/grand/petit
  int flush;                    // fflush sur les fichiers log
  int travel;                   // type de déplacements (same domain etc)
  int seeker;                   // up & down
  int depth;                    // nombre de niveaux de récursion
  int extdepth;                 // nombre de niveaux de récursion à l'éxtérieur
  int urlmode;                  // liens relatifs etc   
  int no_type_change;           // do not change file type according to MIME
  int debug;                    // mode débug log
  int getmode;                  // sauver html, images..
  FILE *log;                    // fichier log
  FILE *errlog;                 // et erreur
  LLint maxsite;                // taille max site
  LLint maxfile_nonhtml;        // taille max non html
  LLint maxfile_html;           // taille max html
  int maxsoc;                   // nbre sockets
  LLint fragment;               // fragmentation d'un site
  int nearlink;                 // prendre les images/data proche d'une page mais à l'extérieur
  int makeindex;                // faire un index
  int kindex;                   // et un index 'keyword'
  int delete_old;               // effacer anciens fichiers
  int timeout;                  // nombre de secondes de timeout
  int rateout;                  // nombre d'octets minium pour le transfert
  int maxtime;                  // temps max en secondes
  int maxrate;                  // taux de transfert max
  float maxconn;                // nombre max de connexions/s
  int waittime;                 // démarrage programmé
  int cache;                    // génération d'un cache
  //int aff_progress;     // barre de progression
  int shell;                    // gestion d'un shell par pipe stdin/stdout
  t_proxy proxy;                // configuration du proxy
  int savename_83;              // conversion 8-3 pour les noms de fichiers
  int savename_type;            // type de noms: structure originale/html-images en un seul niveau
  String savename_userdef;      // structure userdef (ex: %h%p/%n%q.%t)
  int savename_delayed;         // delayed type check
  int delayed_cached;           // delayed type check can be cached to speedup updates
  int mimehtml;                 // MIME-html
  int user_agent_send;          // user agent (ex: httrack/1.0 [sun])
  String user_agent;            //
  String referer;               // referer 
  String from;                  // from
  String path_log;              // chemin pour cache et log
  String path_html;             // chemin pour miroir
  String path_html_utf8;        // chemin pour miroir, UTF-8
  String path_bin;              // chemin pour templates
  int retry;                    // nombre d'essais supplémentaires en cas d'échec
  int makestat;                 // mettre à jour un fichier log de statistiques de transfert
  int maketrack;                // mettre à jour un fichier log de statistiques d'opérations
  int parsejava;                // parsing des classes java pour récupérer les class, gif & cie ; see htsparsejava_flags
  int hostcontrol;              // abandon d'un host trop lent etc.
  int errpage;                  // générer une page d'erreur en cas de 404 etc.
  int check_type;               // si type inconnu (cgi,asp,/) alors tester lien (et gérer moved éventuellement)
  int all_in_cache;             // tout mettre en cache!
  int robots;                   // traitement des robots
  int external;                 // pages externes->pages d'erreur
  int passprivacy;              // pas de mot de pass dans les liens externes?
  int includequery;             // include la query-string
  int mirror_first_page;        // miroir des liens
  String sys_com;               // commande système
  int sys_com_exec;             // executer commande 
  int accept_cookie;            // gestion des cookies
  t_cookie *cookie;
  int http10;                   // forcer http 1.0
  int nokeepalive;              // pas de keep-alive
  int nocompression;            // pas de compression
  int sizehack;                 // forcer réponse "mis à jour" si taille identique
  int urlhack;                  // force "url normalization" to avoid loops
  int tolerant;                 // accepter content-length incorrect
  int parseall;                 // essayer de tout parser (tags inconnus contenant des liens, par exemple)
  int parsedebug;               // débugger parser (debug!)
  int norecatch;                // ne pas reprendre les fichiers effacés localement par l'utilisateur
  int verbosedisplay;           // animation textuelle
  String footer;                // ligne d'infos
  int maxcache;                 // maximum en mémoire au niveau du cache (backing)
  //int maxcache_anticipate; // maximum de liens à anticiper (majorant)
  int ftp_proxy;                // proxy http pour ftp
  String filelist;              // fichier liste URL à inclure
  String urllist;               // fichier liste de filtres à inclure
  htsfilters filters;           // contient les pointeurs pour les filtres
  hash_struct *hash;            // hash structure
  lien_url **liens;             // links
  int lien_tot;                 // top index of "links" heap (always out-of-range)
  lien_buffers *liensbuf;       // links buffers
  robots_wizard *robotsptr;     // robots ptr
  String lang_iso;              // en, fr ..
  String accept;                // Accept:
  String headers;               // Additional headers
  String mimedefs;              // ext1=mimetype1\next2=mimetype2..
  String mod_blacklist;         // (3.41)
  int convert_utf8;             // filenames UTF-8 conversion (3.46)
  //
  int maxlink;                  // nombre max de liens
  int maxfilter;                // nombre max de filtres
  //
  const char *exec;             // adresse du nom de l'éxecutable
  //
  int quiet;                    // poser des questions autres que wizard?
  int keyboard;                 // vérifier stdin
  int bypass_limits;            // bypass built-in limits
  int background_on_suspend;    // background process on suspend signal
  //
  int is_update;                // c'est une update (afficher "File updated...")
  int dir_topindex;             // reconstruire top index par la suite
  //
  // callbacks
  t_hts_htmlcheck_callbacks *callbacks_fun;
  // store library handles
  htslibhandles libHandles;
  //
  htsoptstate state;            // state
};

// stats for httrack
#ifndef HTS_DEF_FWSTRUCT_hts_stat_struct
#define HTS_DEF_FWSTRUCT_hts_stat_struct
typedef struct hts_stat_struct hts_stat_struct;
#endif
struct hts_stat_struct {
  LLint HTS_TOTAL_RECV;         // flux entrant reçu
  LLint stat_bytes;             // octets écrits sur disque
  // int HTS_TOTAL_RECV_STATE;  // status: 0 tout va bien 1: ralentir un peu 2: ralentir 3: beaucoup
  TStamp stat_timestart;        // départ
  //
  LLint total_packed;           // flux entrant compressé reçu
  LLint total_unpacked;         // flux entrant compressé reçu
  int total_packedfiles;        // fichiers compressés
  //
  TStamp istat_timestart[2];    // départ pour calcul instantanné
  LLint istat_bytes[2];         // calcul pour instantanné
  TStamp istat_reference01;     // top départ donné par #0 à #1
  int istat_idlasttimer;        // id du timer qui a récemment donné une stat
  //
  int stat_files;               // nombre de fichiers écrits
  int stat_updated_files;       // nombre de fichiers mis à jour
  int stat_background;          // nombre de fichiers écrits en arrière plan
  //
  int stat_nrequests;           // nombre de requêtes sur socket
  int stat_sockid;              // nombre de sockets allouées au total
  int stat_nsocket;             // nombre de sockets
  int stat_errors;              // nombre d'erreurs
  int stat_errors_front;        // idem, mais au tout premier niveau
  int stat_warnings;            // '' warnings
  int stat_infos;               // '' infos
  int nbk;                      // fichiers anticipés en arrière plan et terminés
  LLint nb;                     // données transférées actuellement (estimation)
  //
  LLint rate;
  //
  TStamp last_connect;          // last connect() call
  TStamp last_request;          // last request issued
};

// structure pour paramètres supplémentaires lors de la requête
#ifndef HTS_DEF_FWSTRUCT_htsrequest_proxy
#define HTS_DEF_FWSTRUCT_htsrequest_proxy
typedef struct htsrequest_proxy htsrequest_proxy;
#endif
struct htsrequest_proxy {
  int active;
  const char* name;
  int port;
  const char* bindhost;           // bind this host
};

#ifndef HTS_DEF_FWSTRUCT_htsrequest
#define HTS_DEF_FWSTRUCT_htsrequest
typedef struct htsrequest htsrequest;
#endif
struct htsrequest {
  short int user_agent_send;    // user agent (ex: httrack/1.0 [sun])
  short int http11;             // l'en tête peut (doit) être signé HTTP/1.1 et non HTTP/1.0
  short int nokeepalive;        // pas de keep-alive
  short int range_used;         // Range utilisé
  short int nocompression;      // Pas de compression
  short int flush_garbage;      // recycled
  const char* user_agent;
  const char* referer;
  const char* from;
  const char* lang_iso;
  const char* accept;
  const char* headers;
  htsrequest_proxy proxy;       // proxy
};

// structure pour retour d'une connexion/prise d'en tête
#ifndef HTS_DEF_FWSTRUCT_htsblk
#define HTS_DEF_FWSTRUCT_htsblk
typedef struct htsblk htsblk;
#endif
struct htsblk {
  int statuscode;               // status-code, -1=erreur, 200=OK,201=..etc (cf RFC1945)
  short int notmodified;        // page ou fichier NON modifié (transféré)
  short int is_write;           // sortie sur disque (out) ou en mémoire (adr)
  short int is_chunk;           // mode chunk
  short int compressed;         // compressé?
  short int empty;              // vide?
  short int keep_alive;         // Keep-Alive?
  short int keep_alive_trailers;        // ..with trailers extension
  int keep_alive_t;             // KA timeout
  int keep_alive_max;           // KA number of requests
  char *adr;                    // adresse du bloc de mémoire, NULL=vide
  char *headers;                // adresse des en têtes si présents
  FILE *out;                    // écriture directe sur disque (si is_write=1)
  LLint size;                   // taille fichier
  char msg[80];                 // message éventuel si échec ("\0"=non précisé)
  char contenttype[64];         // content-type ("text/html" par exemple)
  char charset[64];             // charset ("iso-8859-1" par exemple)
  char contentencoding[64];     // content-encoding ("gzip" par exemple)
  char *location;               // on copie dedans éventuellement la véritable 'location'
  LLint totalsize;              // taille totale à télécharger (-1=inconnue)
  short int is_file;            // ce n'est pas une socket mais un descripteur de fichier si 1
  T_SOC soc;                    // ID socket
  SOCaddr address;              // IP address
  int address_size;             // IP address structure length (unused internally)
  FILE *fp;                     // fichier pour file://
#if HTS_USEOPENSSL
  short int ssl;                // is this connection a SSL one? (https)
  // BIO* ssl_soc;          // SSL structure
  SSL *ssl_con;                 // connection structure
#endif
  char lastmodified[64];        // Last-Modified
  char etag[256];               // Etag
  char cdispo[256];             // Content-Disposition coupé
  LLint crange;                 // Content-Range
  LLint crange_start;           // Content-Range
  LLint crange_end;             // Content-Range
  int debugid;                  // debug connection
  /* */
  htsrequest req;               // paramètres pour la requête
  /*char digest[32+2];   // digest md5 généré par le moteur ("" si non généré) */
};

// structure d'un lien
#ifndef HTS_DEF_FWSTRUCT_lien_url
#define HTS_DEF_FWSTRUCT_lien_url
typedef struct lien_url lien_url;
#endif
struct lien_url {
  char *adr;                    // adresse
  char *fil;                    // nom du fichier distant
  char *sav;                    // nom à sauver sur disque (avec chemin éventuel)
  char *cod;                    // chemin codebase éventuel si classe java
  char *former_adr;             // adresse initiale (avant éventuel moved), peut être nulle
  char *former_fil;             // nom du fichier distant initial (avant éventuel moved), peut être nul

  int premier;                  // pointeur sur le premier lien qui a donné lieu aux autres liens du domaine
  int precedent;                // pointeur sur le lien qui a donné lieu à ce lien précis
  int depth;                    // profondeur autorisée lien ; >0 forte 0=faible
  int pass2;                    // traiter après les autres, seconde passe. si == -1, lien traité en background
  char link_import;             // lien importé à la suite d'un moved - ne pas appliquer les règles classiques up/down
  //int moved;          // pointeur sur moved
  int retry;                    // nombre de retry restants
  int testmode;                 // mode test uniquement, envoyer juste un head!
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
  char url_adr[HTS_URLMAXSIZE * 2];     // adresse
  char url_fil[HTS_URLMAXSIZE * 2];     // nom du fichier distant
  char url_sav[HTS_URLMAXSIZE * 2];     // nom à sauver sur disque (avec chemin éventuel)
  char referer_adr[HTS_URLMAXSIZE * 2]; // adresse host page referer
  char referer_fil[HTS_URLMAXSIZE * 2]; // fichier page referer
  char location_buffer[HTS_URLMAXSIZE * 2];     // "location" en cas de "moved" (302,..)
  char *tmpfile;                // nom à sauver temporairement (compressé)
  char tmpfile_buffer[HTS_URLMAXSIZE * 2];      // buffer pour le nom à sauver temporairement
  char send_too[1024];          // données à envoyer en même temps que le header
  int status;                   // status (-1=non utilisé, 0: prêt, >0: opération en cours)
  int locked;                   // locked (to be used soon)
  int testmode;                 // mode de test
  int timeout;                  // gérer des timeouts? (!=0  : nombre de secondes)
  TStamp timeout_refresh;       // si oui, time refresh
  int rateout;                  // timeout refresh? (!=0 : taux minimum toléré en octets/s)
  TStamp rateout_time;          // si oui, date de départ
  LLint maxfile_nonhtml;        // taille max d'un fichier non html
  LLint maxfile_html;           // idem pour un ficheir html
  htsblk r;                     // structure htsblk de chaque objet en background 
  int is_update;                // mode update
  int head_request;             // requète HEAD?
  LLint range_req_size;         // range utilisé
  TStamp ka_time_start;         // refresh time for KA 
  //
  int http11;                   // L'en tête doit être signé HTTP/1.1 et non HTTP/1.0
  int is_chunk;                 // chunk?
  char *chunk_adr;              // adresse chunk en cours de chargement
  LLint chunk_size;             // taille chunk en cours de chargement
  LLint chunk_blocksize;        // taille data declaree par le chunk
  LLint compressed_size;        // taille compressés (stats uniquement)
  //
  //int links_index;        // to access liens[links_index]
  //
  char info[256];               // éventuel status pour le ftp
  int stop_ftp;                 // flag stop pour ftp
  int finalized;                // finalized (optim memory)
  int early_add;                // was added before link heap saw it
#if DEBUG_CHECKINT
  char magic2;
#endif
};

#ifdef __cplusplus
}
#endif

#endif
