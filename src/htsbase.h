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
/* File: Basic definitions                                      */
/*       Used in .c files for basic (malloc() ..) definitions   */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTS_BASICH
#define HTS_BASICH

#ifdef __cplusplus
extern "C" {
#endif

#include "htsglobal.h"
#include "htsstrings.h"
#include "htssafe.h"

#include <string.h>
#include <time.h>

#ifndef _WIN32
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>

#ifndef _WIN32
#include <dlfcn.h>
#endif

#include <errno.h>

#ifdef _WIN32
#else
#include <fcntl.h>
#endif
#include <assert.h>

/* Compiler-portability attribute macros (no-ops on non-GCC). */
#ifndef HTS_UNUSED
#ifdef __GNUC__
#define HTS_UNUSED __attribute__ ((unused))

#define HTS_STATIC static __attribute__ ((unused))

#define HTS_INLINE __inline__
/* printf-style format check; fmt/arg are 1-based argument positions. */
#define HTS_PRINTF_FUN(fmt, arg) __attribute__ ((format (printf, fmt, arg)))
#else
#define HTS_UNUSED
#define HTS_STATIC static
#define HTS_INLINE
#define HTS_PRINTF_FUN(fmt, arg)
#endif
#endif

/* min/max evaluate their arguments twice; pass side-effect-free expressions. */
#undef min
#undef max
#define min(a,b) ((a)>(b)?(b):(a))

#define max(a,b) ((a)>(b)?(a):(b))

#ifndef _WIN32
#undef Sleep
#define min(a,b) ((a)>(b)?(b):(a))

#define max(a,b) ((a)>(b)?(a):(b))

/* Win32 Sleep() shim for POSIX; argument is milliseconds. */
#define Sleep(a) { if (((a)*1000)%1000000) usleep(((a)*1000)%1000000); if (((a)*1000)/1000000) sleep(((a)*1000)/1000000); }
#endif

/* hichar: ASCII uppercasing of one char. streql: case-insensitive equality of
   two chars. ASCII only; not locale-aware. */
#define hichar(a) ((((a)>='a') && ((a)<='z')) ? ((a)-('a'-'A')) : (a))

#define streql(a,b) (hichar(a)==hichar(b))

/* True if c is an ASCII uppercase letter. */
#define isUpperLetter(a) ( ((a) >= 'A') && ((a) <= 'Z') )

/* Library-internal only (engine translation units that define
   HTS_INTERNAL_BYTECODE); not part of the consumer surface. */
#ifdef HTS_INTERNAL_BYTECODE

/* Resolve a symbol in an already-loaded dynamic module. */
#ifdef _WIN32
#define DynamicGet(handle, sym) GetProcAddress(handle, sym)
#else
#define DynamicGet(handle, sym) dlsym(handle, sym)
#endif

#endif

#ifdef __cplusplus
}
#endif

#endif
