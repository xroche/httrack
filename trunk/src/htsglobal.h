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
/* File: Global #define file                                    */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

// Fichier réunissant l'ensemble des defines

#ifndef HTTRACK_GLOBAL_DEFH
#define HTTRACK_GLOBAL_DEFH

// Version
#define HTTRACK_VERSION      "3.20-2"
#define HTTRACK_VERSIONID    "3.20.02"
#define HTTRACK_AFF_VERSION  "3.x"
//#define HTTRACK_AFF_WARNING  "This is a RELEASE CANDIDATE version of WinHTTrack Website Copier 3.0\nPlease report us any bug or problem"



// Définition plate-forme
#include "htssystem.h"
#include "htsconfig.h"

// Socket windows ou socket unix
#if HTS_PLATFORM==1
#define HTS_WIN 1
#else
#define HTS_WIN 0
#endif

// compatibilité DOS
#if HTS_WIN
#define HTS_DOSNAME 1
#else
#define HTS_DOSNAME 0
#endif

// utiliser zlib?
#if HTS_USEZLIB
#else
#ifdef _WINDOWS
#define HTS_USEZLIB 1
#endif
#endif

#ifndef HTS_INET6
#define HTS_INET6 0
#endif

// utiliser openssl?
#ifndef HTS_USEOPENSSL
#define HTS_USEOPENSSL 1
#endif

#if HTS_WIN
#else
#define __cdecl
#endif

/*
#if HTS_XGETHOST
#if HTS_PLATFORM==1
#ifndef __cplusplus
#undef HTS_XGMETHOD
#undef HTS_XGETHOST
#endif
#endif
#else
#undef HTS_XGMETHOD
#undef HTS_XGETHOST
#endif
*/


#if HTS_ANALYSTE
#else
#if HTS_WIN
#else
#undef HTS_ANALYSTE
// Analyste
#define HTS_ANALYSTE 1
#define HTS_ANALYSTE_CONSOLE 1
#endif
#endif


/* rc file */
#if HTS_WIN
#define HTS_HTTRACKRC "httrackrc"
#else

#ifndef HTS_ETCPATH
#define HTS_ETCPATH "/etc"
#endif
#ifndef HTS_BINPATH
#define HTS_BINPATH "/usr/bin"
#endif
#ifndef HTS_LIBPATH
#define HTS_LIBPATH "/usr/lib"
#endif
#ifndef HTS_PREFIX
#define HTS_PREFIX "/usr"
#endif

#define HTS_HTTRACKRC ".httrackrc"
#define HTS_HTTRACKCNF HTS_ETCPATH"/httrack.conf"
#define HTS_HTTRACKDIR HTS_PREFIX"/doc/httrack/"

#endif

/* Gestion des tables de hashage */
#define HTS_HASH_SIZE 20147
/* Taille max d'une URL */
#define HTS_URLMAXSIZE 512
/* Taille max ligne de commande (>=HTS_URLMAXSIZE*2) */
#define HTS_CDLMAXSIZE 1024
/* Copyright (C) Xavier Roche and other contributors */
#define HTTRACK_AFF_AUTHORS "[XR&CO'2002]"
#define HTS_DEFAULT_FOOTER "<!-- Mirrored from %s%s by HTTrack Website Copier/"HTTRACK_AFF_VERSION" "HTTRACK_AFF_AUTHORS", %s -->"
#define HTS_UPDATE_WEBSITE "http://www.httrack.com/update.php3?Product=HTTrack&Version="HTTRACK_VERSIONID"&VersionStr="HTTRACK_VERSION"&Platform=%d&Language=%s"

#define H_CRLF "\x0d\x0a"
#define CRLF   "\x0d\x0a"
#if HTS_WIN
#define LF "\x0d\x0a"
#else
#define LF "\x0a"
#endif

/* équivaut à "paramètre vide", par exemple -F (none) */
#define HTS_NOPARAM "(none)"
#define HTS_NOPARAM2 "\"(none)\""

/* maximum et minimum */
#define maximum(A,B) ( (A) > (B) ? (A) : (B) )
#define minimum(A,B) ( (A) < (B) ? (A) : (B) )

/* chaine vide? */
#define strnotempty(A) (((A)[0]!='\0') ? 1 : 0)

/* optimisation inline si possible */
#ifdef __cplusplus
#define HTS_INLINE inline
#else
#define HTS_INLINE
#endif

#ifdef HTS_NO_64_BIT
#define HTS_LONGLONG 0
#else
#define HTS_LONGLONG 1
#endif

// long long int? (or int)
// (and int cast for system functions like malloc() )
#if HTS_LONGLONG
 #if HTS_WIN
  typedef __int64 LLint;
  typedef __int64 TStamp;
  typedef int INTsys;
  #define LLintP "%I64d"
 #else
 #if HTS_PLATFORM==0
  typedef long long int LLint;
  typedef long long int TStamp;
  typedef int INTsys;
  #define LLintP "%lld"
 #else
  typedef long long int LLint;
  typedef long long int TStamp;
  typedef int INTsys;
  #define LLintP "%Ld"
 #endif
 #endif
#else
 typedef int LLint;
 typedef int INTsys;
 typedef double TStamp;
 #define LLintP "%d"
#endif

/* Alignement */
#ifndef HTS_ALIGN
#define HTS_ALIGN 4
#endif

/* IPV4, IPV6 and various unified structures */
#define HTS_MAXADDRLEN 64

#if HTS_WIN
#else
#define __cdecl 
#endif

/* mode pour mkdir ET chmod (accès aux fichiers) */
#define HTS_PROTECT_FOLDER (S_IRUSR|S_IWUSR|S_IXUSR)
#if HTS_ACCESS
#define HTS_ACCESS_FILE (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)
#define HTS_ACCESS_FOLDER (S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH)
#else
#define HTS_ACCESS_FILE (S_IRUSR|S_IWUSR)
#define HTS_ACCESS_FOLDER (S_IRUSR|S_IWUSR|S_IXUSR)
#endif

/* vérifier la déclaration des variables préprocesseur */
#ifndef HTS_DOSNAME
#error | HTS_DOSNAME Has not been defined.
#error | Set it to 1 if you are under DOS, 0 under Unix.
#error | Example: place this line in you source, before includes:
#error | #define HTS_DOSNAME 0
#error
#error
#endif
#ifndef HTS_ACCESS
/* Par défaut, accès à tous les utilisateurs */
#define HTS_ACCESS 1
#endif

/* fflush sur stdout */
#define io_flush { fflush(stdout); fflush(stdin); }



/* HTSLib */

// Cache DNS, accélère les résolution d'adresses
#define HTS_DNSCACHE 1

// ID d'une pseudo-socket locale pour les file://
#define LOCAL_SOCKET_ID -500000

// taille de chaque buffer (10 sockets 650 ko)
#define TAILLE_BUFFER 65535

#if HTS_WIN
#else
// use pthreads.h
#ifdef HTS_DO_NOT_USE_PTHREAD
#define USE_PTHREAD 0
#else
#define USE_PTHREAD 1
#endif
#endif

#if HTS_WIN
#define USE_BEGINTHREAD 1
#else
#if USE_PTHREAD
#define USE_BEGINTHREAD 1
#else
/* sh*t.. */
#define USE_BEGINTHREAD 0
#endif
#endif

/* ------------------------------------------------------------ */
/* Debugging                                                    */
/* ------------------------------------------------------------ */

// débuggage types
#define DEBUG_SHOWTYPES 0
// backing debug
#define BDEBUG 0
// chunk receive
#define CHUNKDEBUG 0
// realloc links debug
#define MDEBUG 0
// cache debug
#define DEBUGCA 0
// DNS debug
#define DEBUGDNS 0
// savename debug
#define DEBUG_SAVENAME 0
// debug robots
#define DEBUG_ROBOTS 0
// debug hash
#define DEBUG_HASH 0
// Vérification d'intégrité
#define DEBUG_CHECKINT 0
// nbr sockets debug
#define NSDEBUG 0
// tracer mallocs
#define HTS_TRACE_MALLOC 0

// débuggage HTSLib
#define HDEBUG 0
// surveillance de la connexion
#define CNXDEBUG 0
// debuggage cookies
#define DEBUG_COOK 0
// débuggage hard..
#define HTS_WIDE_DEBUG 0
// debuggage deletehttp et cie
#define HTS_DEBUG_CLOSESOCK 0
// debug tracage mémoire
#define MEMDEBUG 0

// htsmain
#define DEBUG_STEPS 0

#endif

