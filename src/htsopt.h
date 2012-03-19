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
/* File: HTTrack parameters block                               */
/*       Called by httrack.h and some other files               */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */


#ifndef HTTRACK_DEFOPT
#define HTTRACK_DEFOPT

#include <stdio.h>
#include "htsbasenet.h"
#include "htsbauth.h"

// structure proxy
typedef struct {
  int active;
  char name[1024];
  int port;
  char bindhost[256];   // bind this host
} t_proxy; 

/* Structure utile pour copier en bloc les paramètres */
typedef struct {
  char***  filters;
  int*     filptr;
  //int*    filter_max;
} htsfilters;

/* Structure état du miroir */
typedef struct {
  int stop;
  int exit_xh;
  int back_add_stats;
  /* */
  int mimehtml_created;
  char mimemid[256];
  FILE* mimefp;
} htsoptstate;


// paramètres httrack (options)
typedef struct {
  int wizard;       // wizard aucun/grand/petit
  int flush;        // fflush sur les fichiers log
  int travel;       // type de déplacements (same domain etc)
  int seeker;       // up & down
  int depth;        // nombre de niveaux de récursion
  int extdepth;     // nombre de niveaux de récursion à l'éxtérieur
  int urlmode;      // liens relatifs etc   
  int debug;        // mode débug log
  int getmode;      // sauver html, images..
  FILE* log;        // fichier log
  FILE* errlog;     // et erreur
  LLint maxsite;        // taille max site
  LLint maxfile_nonhtml; // taille max non html
  LLint maxfile_html;   // taille max html
  int maxsoc;           // nbre sockets
  LLint fragment;       // fragmentation d'un site
  int nearlink;         // prendre les images/data proche d'une page mais à l'extérieur
  int makeindex;        // faire un index
  int kindex;           // et un index 'keyword'
  int delete_old;       // effacer anciens fichiers
  int timeout;          // nombre de secondes de timeout
  int rateout;          // nombre d'octets minium pour le transfert
  int maxtime;          // temps max en secondes
  int maxrate;          // taux de transfert max
  int maxconn;          // nombre max de connexions/s
  int waittime;         // démarrage programmé
  int cache;            // génération d'un cache
  //int aff_progress;     // barre de progression
  int shell;            // gestion d'un shell par pipe stdin/stdout
  t_proxy proxy;        // configuration du proxy
  int savename_83;      // conversion 8-3 pour les noms de fichiers
  int savename_type;    // type de noms: structure originale/html-images en un seul niveau
  char savename_userdef[256];  // structure userdef (ex: %h%p/%n%q.%t)
  int mimehtml;         // MIME-html
  int user_agent_send;  // user agent (ex: httrack/1.0 [sun])
  char user_agent[128];
  char path_log[1024];  // chemin pour cache et log
  char path_html[1024]; // chemin pour miroir
  char path_bin[1024];  // chemin pour templates
  int retry;            // nombre d'essais supplémentaires en cas d'échec
  int makestat;         // mettre à jour un fichier log de statistiques de transfert
  int maketrack;        // mettre à jour un fichier log de statistiques d'opérations
  int parsejava;        // parsing des classes java pour récupérer les class, gif & cie
  int hostcontrol;      // abandon d'un host trop lent etc.
  int errpage;          // générer une page d'erreur en cas de 404 etc.
  int check_type;       // si type inconnu (cgi,asp,/) alors tester lien (et gérer moved éventuellement)
  int all_in_cache;     // tout mettre en cache!
  int robots;           // traitement des robots
  int external;         // pages externes->pages d'erreur
  int passprivacy;      // pas de mot de pass dans les liens externes?
  int includequery;     // include la query-string
  int mirror_first_page; // miroir des liens
  char sys_com[2048];   // commande système
  int sys_com_exec;     // executer commande 
  int accept_cookie;    // gestion des cookies
  t_cookie* cookie;
  int http10;           // forcer http 1.0
  int nokeepalive;      // pas de keep-alive
  int nocompression;    // pas de compression
  int sizehack;         // forcer réponse "mis à jour" si taille identique
  int urlhack;          // force "url normalization" to avoid loops
  int tolerant;         // accepter content-length incorrect
  int parseall;         // essayer de tout parser (tags inconnus contenant des liens, par exemple)
  int norecatch;        // ne pas reprendre les fichiers effacés localement par l'utilisateur
  int verbosedisplay;   // animation textuelle
  char footer[256];     // ligne d'infos
  int maxcache;         // maximum en mémoire au niveau du cache (backing)
  //int maxcache_anticipate; // maximum de liens à anticiper (majorant)
  int ftp_proxy;        // proxy http pour ftp
  char filelist[1024];  // fichier liste URL à inclure
  char urllist[1024];   // fichier liste de filtres à inclure
  htsfilters filters;   // contient les pointeurs pour les filtres
  void* hash;           // hash structure
  void* robotsptr;         // robots ptr
  char lang_iso[64];    // en, fr ..
  char mimedefs[2048];  // ext1=mimetype1\next2=mimetype2..
  //
  int maxlink;          // nombre max de liens
  int maxfilter;        // nombre max de filtres
  //
  char* exec;           // adresse du nom de l'éxecutable
  //
  int quiet;            // poser des questions autres que wizard?
  int keyboard;         // vérifier stdin
  //
  int is_update;        // c'est une update (afficher "File updated...")
  int dir_topindex;     // reconstruire top index par la suite
  //
  htsoptstate state;    // état
} httrackp;

// stats for httrack
typedef struct {
  LLint HTS_TOTAL_RECV;      // flux entrant reçu
  LLint stat_bytes;          // octets écrits sur disque
  // int HTS_TOTAL_RECV_STATE;  // status: 0 tout va bien 1: ralentir un peu 2: ralentir 3: beaucoup
  TStamp stat_timestart;     // départ
  //
  LLint total_packed;        // flux entrant compressé reçu
  LLint total_unpacked;      // flux entrant compressé reçu
  int   total_packedfiles;   // fichiers compressés
  //
  TStamp istat_timestart[2];   // départ pour calcul instantanné
  LLint  istat_bytes[2];       // calcul pour instantanné
  TStamp istat_reference01;    // top départ donné par #0 à #1
  int    istat_idlasttimer;    // id du timer qui a récemment donné une stat
  //
  int stat_files;            // nombre de fichiers écrits
  int stat_updated_files;    // nombre de fichiers mis à jour
  //
  int stat_nrequests;        // nombre de requêtes sur socket
  int stat_sockid;           // nombre de sockets allouées au total
  int stat_nsocket;          // nombre de sockets
  int stat_errors;           // nombre d'erreurs
  int stat_errors_front;     // idem, mais au tout premier niveau
  int stat_warnings;         // '' warnings
  int stat_infos;            // '' infos
  int nbk;                   // fichiers anticipés en arrière plan et terminés
  LLint nb;                  // données transférées actuellement (estimation)
  //
  LLint rate;
} hts_stat_struct;


#endif

