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

/* GCC extension */
#ifndef HTS_UNUSED
#ifdef __GNUC__
#define HTS_UNUSED __attribute__ ((unused))
#define HTS_STATIC static __attribute__ ((unused))
#define HTS_INLINE __inline__
#define HTS_PRINTF_FUN(fmt, arg) __attribute__ ((format (printf, fmt, arg)))
#else
#define HTS_UNUSED
#define HTS_STATIC static
#define HTS_INLINE
#define HTS_PRINTF_FUN(fmt, arg)
#endif
#endif

#undef min
#undef max
#define min(a,b) ((a)>(b)?(b):(a))
#define max(a,b) ((a)>(b)?(a):(b))

#ifndef _WIN32
#undef Sleep
#define min(a,b) ((a)>(b)?(b):(a))
#define max(a,b) ((a)>(b)?(a):(b))
#define Sleep(a) { if (((a)*1000)%1000000) usleep(((a)*1000)%1000000); if (((a)*1000)/1000000) sleep(((a)*1000)/1000000); }
#endif

// teste égalité de 2 chars, case insensitive
#define hichar(a) ((((a)>='a') && ((a)<='z')) ? ((a)-('a'-'A')) : (a))
#define streql(a,b) (hichar(a)==hichar(b))

// caractère maj
#define isUpperLetter(a) ( ((a) >= 'A') && ((a) <= 'Z') )

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE

// functions
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
