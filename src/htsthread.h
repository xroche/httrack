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
/* File: Threads                                                */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTS_DEFTHREAD
#define HTS_DEFTHREAD

#include "htsglobal.h"
#ifndef _WIN32
#include <pthread.h>
#endif
#ifdef _WIN32
#include "windows.h"
#endif
#ifndef USE_BEGINTHREAD
#error needs USE_BEGINTHREAD
#endif

/* Forward definition */
#ifndef HTS_DEF_FWSTRUCT_htsmutex_s
#define HTS_DEF_FWSTRUCT_htsmutex_s
typedef struct htsmutex_s htsmutex_s, *htsmutex;
#endif
#define HTSMUTEX_INIT NULL

#ifdef _WIN32
struct htsmutex_s {
  HANDLE handle;
};
#else /* #ifdef _WIN32 */
struct htsmutex_s {
  pthread_mutex_t handle;
};
#endif /* #ifdef _WIN32 */

/* Library internal definictions */
HTSEXT_API int hts_newthread(void (*fun) (void *arg), void *arg);

HTSEXT_API void htsthread_wait_n(int n_wait);

/* Locking functions */
HTSEXT_API void hts_mutexinit(htsmutex * mutex);
HTSEXT_API void hts_mutexfree(htsmutex * mutex);
HTSEXT_API void hts_mutexlock(htsmutex * mutex);
HTSEXT_API void hts_mutexrelease(htsmutex * mutex);

#ifdef HTS_INTERNAL_BYTECODE
/* Thread initialization */
HTSEXT_API void htsthread_init(void);
HTSEXT_API void htsthread_uninit(void);
#endif

#endif
