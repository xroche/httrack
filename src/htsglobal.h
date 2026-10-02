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
 *  The portability layer nearly every other installed header includes: the
 *  version strings, the build switches that shape the exported structs, the
 *  HTSEXT_API export marker, the integer, time and socket typedefs with their
 *  printf formats, and the file-access mode constants. */

#ifndef HTTRACK_GLOBAL_DEFH
#define HTTRACK_GLOBAL_DEFH

/* Package version strings. VERSION is the dashed display form, VERSIONID the
   dotted numeric form hts_version() returns, AFF_VERSION a deliberately
   shape-free short form used in the User-Agent and the mirrored-page footer.
   None of them is the library ABI version, which is VERSION_INFO in
   configure.ac. */
#define HTTRACK_VERSION "3.50-5"
#define HTTRACK_VERSIONID "3.50.5"
#define HTTRACK_AFF_VERSION "3.x"
/* Nothing in the engine reads this one. */
#define HTTRACK_LIB_VERSION "2.0"

/* A consumer that defines HTS_NOINCLUDES includes <stdio.h> and <stdlib.h>
   itself, before this header. */
#ifndef HTS_NOINCLUDES
#include <stdio.h>
#include <stdlib.h>
#endif

/* Engine tuning constants: HTS_ACCESS, poll timings, default filenames. */
#include "htsconfig.h"

// Fixed-width integer types + PRI* format macros for the LLint/TStamp typedefs
#include <stdint.h>
#include <inttypes.h>

/* Type widths for a build with no configure. Nothing in the engine reads
   them any more. */
#ifdef _WIN32
#ifndef SIZEOF_LONG
#define SIZEOF_LONG 4
#define SIZEOF_LONG_LONG 8
#endif
#endif

/* Compiler-attribute markers, each expanding to nothing where the compiler has
   no such attribute. HTS_UNUSED says a symbol may go unused, and HTS_STATIC is
   a static that may. HTS_PRINTF_FUN(fmt, arg) checks the format string at
   argument index @p fmt against the varargs starting at @p arg.
   HTS_CHECK_RESULT says a caller must read the return value, and a (void) cast
   does not silence the warning. */
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

/* Where the build switches come from. Windows hard-codes them because MSVC
   runs no configure, and every other build reads the generated header below. */
#ifdef _WIN32

/*
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_STAT_H 1
*/
/* Windows can always load a shared library at run time. */
#ifndef DLLIB
#define DLLIB 1
#endif
#ifndef HTS_INET6
#define HTS_INET6 1
#endif
/* Stand-ins for the POSIX macros MSVC does not declare. Each answers non-zero
   for a match, not 1, so test them for truth and never against 1. */
#ifndef S_ISREG
#define S_ISREG(m) ((m) & _S_IFREG)

#define S_ISDIR(m) ((m) & _S_IFDIR)
#endif

#else

/* config.h is private: an autoconf consumer has one of its own, so only the
   switches the installed headers read are published, in htsfeatures.h. An
   internal build reads config.h and must not need the generated one. */
#ifdef HTS_INTERNAL_BUILD
#include "config.h"
#else
#include "htsfeatures.h"
#endif

/* Defined when the platform has no setuid(), and the engine then skips its
   "do not run as root" check. */
#ifndef SETUID
#define HTS_DO_NOT_USE_UID
#endif

/* 1 when the platform can load a shared library at run time. It does not gate
   the module hooks, which compile either way (htsmodules.c). */
#ifdef DLLIB
#define HTS_DLOPEN 1
#else
#define HTS_DLOPEN 0
#endif

#endif

/* Marks a local array big enough to matter for stack use. It expands to
   nothing. */
#ifndef BIGSTK
#define BIGSTK
#endif

/* 1 when local paths use a backslash separator, so the engine rewrites the
   '/' in a save name. 1 on Windows, 0 else. */
#ifdef _WIN32
#define HTS_DOSNAME 1
#else
#define HTS_DOSNAME 0
#endif

/* Always 1: zlib is mandatory, because the cache is a zip file. */
#ifndef HTS_USEZLIB
#define HTS_USEZLIB 1
#elif !HTS_USEZLIB
#error HTS_USEZLIB=0 is not a supported configuration
#endif

/* 1 when the engine can decode the br and zstd content codings, so it offers
   them in Accept-Encoding. Each defaults to off. */
#ifndef HTS_USEBROTLI
#define HTS_USEBROTLI 0
#endif
#ifndef HTS_USEZSTD
#define HTS_USEZSTD 0
#endif

/* 1 when the build has IPv6. It picks the sockaddr_in6 arm of SOCaddr
   (htsnet.h), so it changes the layout of every struct holding one: a consumer
   must take the value from htsfeatures.h and never guess it. */
#ifndef HTS_INET6
#define HTS_INET6 0
#endif

/* 1 when the build speaks https. It adds the ssl fields to htsblk (htsopt.h),
   so it too changes struct layout and must come from htsfeatures.h. */
#ifndef HTS_USEOPENSSL
#define HTS_USEOPENSSL 1
#endif

#ifndef HTS_DLOPEN
#define HTS_DLOPEN 1
#endif

/* 1 when the build advertises Flash (.swf) link support. Only the WebHTTrack
   feature list reads it. */
#ifndef HTS_USESWF
#define HTS_USESWF 1
#endif

/* The MSVC calling-convention keyword the engine's callback declarations
   carry. Defined away elsewhere, so they compile everywhere. */
#ifdef _WIN32
#else
#define __cdecl
#endif

/* Install paths and config-file names, all string literals with no trailing
   slash except HTS_HTTRACKDIR, which has one. configure passes the directories
   in (PREFIX, SYSCONFDIR, BINDIR, LIBDIR, DATADIR), and the literals here are
   the fallback for a build that passes none. Only HTS_HTTRACKRC exists on
   Windows, so test the others with #ifdef before you use them. */
#ifdef _WIN32
#define HTS_HTTRACKRC "httrackrc"
#else

#ifndef HTS_ETCPATH
#ifdef SYSCONFDIR
#define HTS_ETCPATH SYSCONFDIR
#else
#define HTS_ETCPATH "/etc"
#endif
#endif
#ifndef HTS_BINPATH
#ifdef BINDIR
#define HTS_BINPATH BINDIR
#else
#define HTS_BINPATH "/usr/bin"
#endif
#endif
#ifndef HTS_LIBPATH
#ifdef LIBDIR
#define HTS_LIBPATH LIBDIR
#else
#define HTS_LIBPATH "/usr/lib"
#endif
#endif
#ifndef HTS_PREFIX
#ifdef PREFIX
#define HTS_PREFIX PREFIX
#else
#define HTS_PREFIX "/usr"
#endif
#endif

#define HTS_HTTRACKRC ".httrackrc"
/* System-wide config file, read only when no HTS_HTTRACKRC was found. */
#define HTS_HTTRACKCNF HTS_ETCPATH "/httrack.conf"

/* Data directory holding the HTML templates and the language files. */
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
/* Retry-After is the server's number, so the engine caps it (seconds). */
#define HTS_DEFAULT_MAX_RETRY_AFTER 60
#define HTS_MAX_RETRY_AFTER_LIMIT 3600
#define HTTRACK_WEB "https://www.httrack.com"
/* Language=%s takes the catalog basename (LANGUAGE_FILE), an ASCII identifier;
   LANGUAGE_NAME is a localized display string in a legacy codepage (#1353). */
#define HTS_UPDATE_WEBSITE                                                     \
  "http://www.httrack.com/"                                                    \
  "update.php3?Product=HTTrack&Version=" HTTRACK_VERSIONID                     \
  "&VersionStr=" HTTRACK_VERSION "&Platform=%d&Language=%s"

/* CR LF. H_CRLF ends an HTTP header line, and CRLF the lines of the HTML pages
   the engine writes itself. */
#define H_CRLF "\x0d\x0a"
#define CRLF "\x0d\x0a"
/* The local text-file and console line terminator, despite the name: CR LF on
   Windows and LF elsewhere. */
#ifdef _WIN32
#define LF "\x0d\x0a"
#else
#define LF "\x0a"
#endif

/* Sentinel meaning "empty parameter", for example -F (none). HTS_NOPARAM2 is
   the same word with the quote characters still around it. */
#define HTS_NOPARAM "(none)"
#define HTS_NOPARAM2 "\"(none)\""

/* Boolean for option fields and yes/no returns. Assign HTS_FALSE or HTS_TRUE.
   It is an int rather than an enum, so C++ still accepts `field = 1`. */
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

/* Larger and smaller of two values. One argument is evaluated twice, so pass
   no side effect. */
#define maximum(A, B) ((A) > (B) ? (A) : (B))

#define minimum(A, B) ((A) < (B) ? (A) : (B))

/* True when @p A is a non-NULL, non-empty string. @p A is evaluated twice. */
#define strnotempty(A) (((A) != NULL && (A)[0] != '\0'))

/* Compile-time check, usable as an expression. */
#define HTS_COMPILE_ASSERT(cond) ((void) sizeof(char[(cond) ? 1 : -1]))

/* The same where a declaration goes, which is where a rule tying constants
   together belongs. NAME is what the diagnostic points at. Takes a ';'. */
#define HTS_STATIC_ASSERT(cond, name)                                          \
  enum { hts_static_assert_##name = 1 / !!(cond) }

/* Expands to `inline` when the header is compiled as C++, and to nothing in
   plain C. */
#ifdef __cplusplus
#define HTS_INLINE inline
#else
#define HTS_INLINE
#endif

/* Marks a symbol as part of the library's public ABI, so a consumer may link
   against it. A symbol without it is internal and may change or vanish. It
   expands to nothing on a compiler with no way to say this. */
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

/** Marks a function deprecated, and @p msg names the replacement. It goes
 *  before the declaration, and expands to nothing where the compiler cannot
 *  warn. */
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

/* Says the function never returns. It goes before the declaration, and
   expands to nothing where the compiler has no such attribute. */
#if defined(__GNUC__)
#define HTS_NORETURN __attribute__((noreturn))
#elif defined(_MSC_VER)
#define HTS_NORETURN __declspec(noreturn)
#else
#define HTS_NORETURN
#endif

/* Marks a deliberate switch fallthrough. Write it as a statement before the
   next case label. It expands to nothing where the compiler has no such
   attribute. */
#if defined(__has_attribute)

#if __has_attribute(fallthrough)

#define HTS_FALLTHROUGH __attribute__((fallthrough))
#endif
#endif
#ifndef HTS_FALLTHROUGH
#define HTS_FALLTHROUGH
#endif

/* Byte counts, file sizes and offsets, exactly 64 bits and signed. -1 means
   "unknown" or "no limit" in the fields and returns that accept it. */
typedef int64_t LLint;
/* A time value, exactly 64 bits and signed. The producer fixes the unit: some
   fields hold seconds since the Unix epoch, mtime_local() milliseconds since
   it, and mtime_monotonic() milliseconds from an unspecified origin, comparable
   only against itself. */
typedef int64_t TStamp;
/* printf conversion for an LLint or a TStamp, '%' included, because PRId64 has
   none: "X: " LLintP. */
#define LLintP "%" PRId64

/* 64 bits where the build has large-file support (HTS_LFS) or under MSVC, and
   a plain int otherwise, so a consumer must not assume a width. INTsysP is its
   printf conversion, '%' included. */
#if defined(HTS_LFS) || defined(_MSC_VER)
typedef LLint INTsys;

#define INTsysP LLintP
#else
typedef int INTsys;

#define INTsysP "%d"
#endif

/* Socket handle: an unsigned integer as wide as a Windows SOCKET (64 bits on
   Win64, 32 on Win32), and a plain int file descriptor on POSIX. T_SOCP is its
   printf conversion, '%' included, so never print a T_SOC with "%d". */
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

/* See __cdecl above. */
#ifdef _WIN32
#else
#define __cdecl
#endif

/* POSIX permission bits for created folders and files (mkdir and chmod).
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

/* Flushes stdout and stdin. It expands to a brace block, so an `if` around it
   needs braces. */
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
/* Type of the guard word written on both sides of a traced block. */
typedef unsigned long int t_htsboundary;

#ifndef HTS_DEF_FWSTRUCT_mlink
#define HTS_DEF_FWSTRUCT_mlink
typedef struct mlink mlink;
#endif
struct mlink {
  char *adr;
  int len;
  int id; /**< serial number of the allocation */
  struct mlink *next;
};

static const t_htsboundary htsboundary = 0xDEADBEEF;
#endif
#endif

/* Nothing reads this any more. */
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
// connection monitoring debug
#define CNXDEBUG 0
// cookie debug
#define DEBUG_COOK 0
// heavy/low-level debug
#define HTS_WIDE_DEBUG 0
// socket close debug
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
/* Lets DEBUG_W("a" _ b) pass several fprintf arguments, by redefining the
   identifier _. Debug builds only. */
#undef _
#define _ ,
#endif

#endif
