/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998 Xavier Roche and other contributors

SPDX-License-Identifier: GPL-3.0-or-later

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

Ethical use: we kindly ask that you NOT use this software to harvest email
addresses or to collect any other private information about people. Doing so
would dishonor our work and waste the many hours we have spent on it.

Please visit our Website: http://www.httrack.com
*/

/* ------------------------------------------------------------ */
/* File: Global #define file                                    */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/** @file htsglobal.h
 *  Foundational portability layer included by every other public header:
 *  version strings, platform/feature switches, the HTSEXT_API export marker,
 *  the integer/time/socket typedefs (LLint, TStamp, INTsys, T_SOC), printf
 *  format helpers, and the file-access mode constants. */

#ifndef HTTRACK_GLOBAL_DEFH
#define HTTRACK_GLOBAL_DEFH

/* Package version strings (the library ABI version is VERSION_INFO in
   configure.ac, decoupled from these). VERSION is the display form, VERSIONID
   the dotted numeric form, AFF_VERSION the short form shown in footers,
   LIB_VERSION the data/cache format generation. */
#define HTTRACK_VERSION "3.49-24"
#define HTTRACK_VERSIONID "3.49.24"
#define HTTRACK_AFF_VERSION "3.x"
#define HTTRACK_LIB_VERSION "2.0"

#ifndef HTS_NOINCLUDES
#include <stdio.h>
#include <stdlib.h>
#endif

// Platform detection (sizes, feature macros)
#include "htsconfig.h"

// Fixed-width integer types + PRI* format macros for the LLint/TStamp typedefs
#include <stdint.h>
#include <inttypes.h>

// WIN32 types
#ifdef _WIN32
#ifndef SIZEOF_LONG
#define SIZEOF_LONG 4
#define SIZEOF_LONG_LONG 8
#endif
#endif

/* Compiler-attribute helpers, no-ops where unsupported.
   HTS_UNUSED: suppress unused-symbol warnings. HTS_STATIC: an unused-safe
   static. HTS_PRINTF_FUN(fmt, arg): mark a printf-like function so the
   compiler type-checks the format string at argument index fmt against the
   varargs starting at arg. HTS_CHECK_RESULT: the return value carries the only
   error signal, so dropping it is a bug; a (void) cast does not silence it. */
#ifndef HTS_UNUSED
#ifdef __GNUC__
#define HTS_UNUSED __attribute__((unused))

#define HTS_STATIC static __attribute__((unused))

#define HTS_PRINTF_FUN(fmt, arg) __attribute__((format(printf, fmt, arg)))

#define HTS_CHECK_RESULT __attribute__((warn_unused_result))
#else
#define HTS_UNUSED
#define HTS_STATIC static
#define HTS_PRINTF_FUN(fmt, arg)
#define HTS_CHECK_RESULT
#endif
#endif

// config.h
#ifdef _WIN32

/*
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_STAT_H 1
*/
#ifndef DLLIB
#define DLLIB 1
#endif
#ifndef HTS_INET6
#define HTS_INET6 1
#endif
#ifndef S_ISREG
#define S_ISREG(m) ((m) & _S_IFREG)

#define S_ISDIR(m) ((m) & _S_IFDIR)
#endif

#else

#include "config.h"

#ifndef SETUID
#define HTS_DO_NOT_USE_UID
#endif

#ifdef DLLIB
#define HTS_DLOPEN 1
#else
#define HTS_DLOPEN 0
#endif

#endif

#ifndef BIGSTK
#define BIGSTK
#endif

// DOS-style 8.3 filenames? 1 on Windows, 0 elsewhere
#ifdef _WIN32
#define HTS_DOSNAME 1
#else
#define HTS_DOSNAME 0
#endif

// zlib is mandatory: the cache is a zip and minizip calls it regardless
#ifndef HTS_USEZLIB
#define HTS_USEZLIB 1
#elif !HTS_USEZLIB
#error HTS_USEZLIB=0 is not a supported configuration
#endif

// brotli and zstd content codings; off unless the build opted in (configure,
// or the Visual Studio projects, which link the vcpkg libraries)
#ifndef HTS_USEBROTLI
#define HTS_USEBROTLI 0
#endif
#ifndef HTS_USEZSTD
#define HTS_USEZSTD 0
#endif

#ifndef HTS_INET6
#define HTS_INET6 0
#endif

// utiliser openssl?
#ifndef HTS_USEOPENSSL
// autoload
#define HTS_USEOPENSSL 1
#endif

#ifndef HTS_DLOPEN
#define HTS_DLOPEN 1
#endif

#ifndef HTS_USESWF
#define HTS_USESWF 1
#endif

#ifdef _WIN32
#else
#define __cdecl
#endif

/* Install paths and config-file names. HTTRACKRC is the per-user rc filename,
   HTTRACKCNF the system-wide config, HTTRACKDIR the shared data directory; the
   ETC/BIN/LIB/PREFIX paths are the defaults these derive from when not set by
   the build. */
#ifdef _WIN32
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
#define HTS_HTTRACKCNF HTS_ETCPATH "/httrack.conf"

#ifdef DATADIR
#define HTS_HTTRACKDIR DATADIR "/httrack/"
#else
#define HTS_HTTRACKDIR HTS_PREFIX "/share/httrack/"
#endif

#endif

/* Maximum URL length, in bytes. Callers size URL/path string buffers to this;
   anything longer is rejected. */
#define HTS_URLMAXSIZE 1024
/* Command-line argument cap, in bytes: an argument this long or longer is
   rejected. A buffer holding a message built around one adds +256. */
#define HTS_CDLMAXSIZE 1024
/* MIME-type buffer contract (htsblk.contenttype/charset/contentencoding); holds
   the longest registered MIME type, the Office OOXML ones reaching 73 chars */
#define HTS_MIMETYPE_SIZE 128
/* Capacity behind the htsblk.location pointer; the Location header is gated
   against this, not against HTS_URLMAXSIZE */
#define HTS_LOCATION_SIZE (HTS_URLMAXSIZE * 2)

/* Caps on single option arguments, in bytes, exclusive like HTS_CDLMAXSIZE.
   They bound the value, not a buffer: these option fields are dynamic Strings.
   Named so the front ends can check against them instead of copying them. */
#define HTS_FOOTER_MAXSIZE 254    /* -%F */
#define HTS_LANGISO_MAXSIZE 62    /* -%l */
#define HTS_REFERER_MAXSIZE 254   /* -%R */
#define HTS_FILELIST_MAXSIZE 254  /* -%L */
#define HTS_BINDHOST_MAXSIZE 254  /* -%b */
#define HTS_FROMEMAIL_MAXSIZE 254 /* -%E */

/* Copyright (C) 1998 Xavier Roche and other contributors */
#define HTTRACK_AFF_AUTHORS "[XR&CO]"
/* Named fields (hts_footer_format); a "%s" anywhere would switch the template
   back to the legacy positional model, a user's own additions included. */
#define HTS_DEFAULT_FOOTER                                                     \
  "<!-- Mirrored from {url} by HTTrack Website Copier/" HTTRACK_AFF_VERSION    \
  " " HTTRACK_AFF_AUTHORS ", {date} -->"
/* Honest crawler User-Agent; no fake OS/browser to go stale. */
#define HTS_DEFAULT_USER_AGENT                                                 \
  "Mozilla/5.0 (compatible; HTTrack/" HTTRACK_AFF_VERSION                      \
  "; +https://www.httrack.com/)"
#define HTTRACK_WEB "https://www.httrack.com"
/* Language=%s takes the catalog basename (LANGUAGE_FILE), an ASCII identifier;
   LANGUAGE_NAME is a localized display string in a legacy codepage (#1353). */
#define HTS_UPDATE_WEBSITE                                                     \
  "http://www.httrack.com/"                                                    \
  "update.php3?Product=HTTrack&Version=" HTTRACK_VERSIONID                     \
  "&VersionStr=" HTTRACK_VERSION "&Platform=%d&Language=%s"

#define H_CRLF "\x0d\x0a"
#define CRLF "\x0d\x0a"
#ifdef _WIN32
#define LF "\x0d\x0a"
#else
#define LF "\x0a"
#endif

/* Sentinel meaning "empty parameter", e.g. -F (none) */
#define HTS_NOPARAM "(none)"
#define HTS_NOPARAM2 "\"(none)\""

/* Boolean flag for option fields and API yes/no returns. Int-backed, not an
   enum: an enum makes C++ reject `field = 1` / `f(0)` on the exported fields
   and params. Int-sized, so the httrackp layout and the ABI are unchanged. */
#ifndef HTS_DEF_DEFSTRUCT_hts_boolean
#define HTS_DEF_DEFSTRUCT_hts_boolean

typedef int hts_boolean;
#define HTS_FALSE 0
#define HTS_TRUE 1
#endif

#ifndef HTS_DEF_DEFSTRUCT_hts_tristate
#define HTS_DEF_DEFSTRUCT_hts_tristate
/* Tri-state hts_boolean: HTS_DEFAULT (-1) = "unspecified" (copy_htsopt leaves
   the target untouched); HTS_FALSE/HTS_TRUE = off/on. */
typedef int hts_tristate;
#define HTS_DEFAULT (-1)
#endif

/* Larger/smaller of two values. Macros: arguments are evaluated twice. */
#define maximum(A, B) ((A) > (B) ? (A) : (B))

#define minimum(A, B) ((A) < (B) ? (A) : (B))

/* True when A is a non-NULL, non-empty string. */
#define strnotempty(A) (((A) != NULL && (A)[0] != '\0'))

/* Compile-time check, usable as an expression. */
#define HTS_COMPILE_ASSERT(cond) ((void) sizeof(char[(cond) ? 1 : -1]))

/* The same where a declaration goes, which is where a rule tying constants
   together belongs. NAME is what the diagnostic points at. Takes a ';'. */
#define HTS_STATIC_ASSERT(cond, name)                                          \
  enum { hts_static_assert_##name = 1 / !!(cond) }

/* 'inline' where the dialect supports it (C++), nothing in plain C. */
#ifdef __cplusplus
#define HTS_INLINE inline
#else
#define HTS_INLINE
#endif

/* Marks a symbol as part of the library's public ABI: exported from
   libhttrack and visible to callers. Symbols without it stay internal (hidden
   under -fvisibility=hidden). Expands to dllexport when building the library,
   dllimport when consuming it, and the visibility("default") attribute on
   ELF. */
#ifdef _WIN32
#ifdef LIBHTTRACK_EXPORTS
#define HTSEXT_API __declspec(dllexport)
#else
#define HTSEXT_API __declspec(dllimport)
#endif
#else
/* See <http://gcc.gnu.org/wiki/Visibility> */
#if ((defined(__GNUC__) && (__GNUC__ >= 4)) ||                                 \
     (defined(HAVE_VISIBILITY) && HAVE_VISIBILITY))

#define HTSEXT_API __attribute__((visibility("default")))
#else
#define HTSEXT_API
#endif
#endif

/**
 * Mark a function deprecated, with a message pointing at the replacement.
 * Placed before the declaration so both the GCC/Clang attribute and the MSVC
 * __declspec sit in a position both accept. Degrades to nothing elsewhere.
 */
#if defined(__GNUC__) &&                                                       \
    (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5))

#define HTS_DEPRECATED(msg) __attribute__((deprecated(msg)))
#elif defined(__GNUC__)

#define HTS_DEPRECATED(msg) __attribute__((deprecated))
#elif defined(_MSC_VER) && (_MSC_VER >= 1400)

#define HTS_DEPRECATED(msg) __declspec(deprecated(msg))
#else
#define HTS_DEPRECATED(msg)
#endif

/* LLint/TStamp: signed exact-width 64-bit; -1 is a sentinel engine-wide. */
typedef int64_t LLint;
typedef int64_t TStamp;
/* Full printf conversion, '%' included (PRId64 has none): "X: " LLintP. */
#define LLintP "%" PRId64

/* Integer type for file offsets/sizes passed to the C library; INTsysP is its
   printf conversion. HTS_LFS is the large-file macro: LFS_FLAG is a configure
   make variable carrying the -D flags, never itself defined. */
#if defined(HTS_LFS) || defined(_MSC_VER)
typedef LLint INTsys;

#define INTsysP LLintP
#else
typedef int INTsys;

#define INTsysP "%d"
#endif

/* Socket-handle type. An unsigned integer wide enough for a Windows SOCKET;
   a plain int file descriptor on POSIX. T_SOCP is its printf conversion,
   '%' included: unsigned __int64 on Win64 must not be printed with "%d". */
#ifdef _WIN32
#if defined(_WIN64)

typedef unsigned __int64 T_SOC;
#define T_SOCP "%" PRIu64
#else
typedef unsigned __int32 T_SOC;
#define T_SOCP "%" PRIu32
#endif
#else
typedef int T_SOC;
#define T_SOCP "%d"
#endif

/* Buffer size for a printed network address (IPv4 or IPv6, NUL included). */
#define HTS_MAXADDRLEN 64

/* Max resolved addresses kept per host for connect fallback (dead IPv6 etc.).
 */
#define HTS_MAXADDRNUM 4

#ifdef _WIN32
#else
#define __cdecl
#endif

/* Permission bits for created folders and files (mkdir and chmod).
   PROTECT_FOLDER/FILE are owner-only. With HTS_ACCESS set (the default) the
   ACCESS_ modes also grant group/other read; otherwise they stay owner-only. */
#define HTS_PROTECT_FOLDER (S_IRUSR | S_IWUSR | S_IXUSR)
#define HTS_PROTECT_FILE (S_IRUSR | S_IWUSR)

#if HTS_ACCESS
#define HTS_ACCESS_FILE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

#define HTS_ACCESS_FOLDER                                                      \
  (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)
#else
#define HTS_ACCESS_FILE (S_IRUSR | S_IWUSR)

#define HTS_ACCESS_FOLDER (S_IRUSR | S_IWUSR | S_IXUSR)
#endif

/* Sanity-check that the required preprocessor switches are defined */
#ifndef HTS_DOSNAME
#error | HTS_DOSNAME Has not been defined.
#error | Set it to 1 if you are under DOS, 0 under Unix.
#error | Example: place this line in you source, before includes:
#error | #define HTS_DOSNAME 0
#error
#error
#endif
#ifndef HTS_ACCESS
/* Default: files readable by all users */
#define HTS_ACCESS 1
#endif

/* fflush sur stdout */
#define io_flush                                                               \
  {                                                                            \
    fflush(stdout);                                                            \
    fflush(stdin);                                                             \
  }

/* HTSLib */

// Enable the DNS cache (speeds up address resolution)
#define HTS_DNSCACHE 1

// Pseudo-socket id standing in for a local file:// transfer
#define LOCAL_SOCKET_ID -2

// Per-connection transfer buffer size, in bytes
#define TAILLE_BUFFER 65536

#ifdef HTS_DO_NOT_USE_PTHREAD
#error needs threads support
#endif
#define USE_BEGINTHREAD 1

#ifdef _DEBUG
// trace mallocs
// #define HTS_TRACE_MALLOC
#ifdef HTS_TRACE_MALLOC
typedef unsigned long int t_htsboundary;

#ifndef HTS_DEF_FWSTRUCT_mlink
#define HTS_DEF_FWSTRUCT_mlink
typedef struct mlink mlink;
#endif
struct mlink {
  char *adr;
  int len;
  int id;
  struct mlink *next;
};

static const t_htsboundary htsboundary = 0xDEADBEEF;
#endif
#endif

/* strxxx debugging */
#ifndef NOSTRDEBUG
#define STRDEBUG 1
#endif

/* ------------------------------------------------------------ */
/* Debugging                                                    */
/* ------------------------------------------------------------ */

// type-detection debug
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
// integrity-check debug
#define DEBUG_CHECKINT 0
// nbr sockets debug
#define NSDEBUG 0

// HTSLib debug
#define HDEBUG 0
// surveillance de la connexion
#define CNXDEBUG 0
// debuggage cookies
#define DEBUG_COOK 0
// heavy/low-level debug
#define HTS_WIDE_DEBUG 0
// debuggage deletehttp et cie
#define HTS_DEBUG_CLOSESOCK 0
// memory-tracing debug
#define MEMDEBUG 0

// htsmain
#define DEBUG_STEPS 0

// Derived debug control switches
#if HTS_DEBUG_CLOSESOCK
#define _HTS_WIDE 1
#endif
#if HTS_WIDE_DEBUG
#define _HTS_WIDE 1
#endif
#if _HTS_WIDE
extern FILE *DEBUG_fp;

#define DEBUG_W(A)                                                             \
  {                                                                            \
    if (DEBUG_fp == NULL)                                                      \
      DEBUG_fp = fopen("bug.out", "wb");                                       \
    fprintf(DEBUG_fp, ":>" A);                                                 \
    fflush(DEBUG_fp);                                                          \
  }
#undef _
#define _ ,
#endif

#endif
