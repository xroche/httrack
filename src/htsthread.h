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
/* File: Threads                                                */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTS_DEFTHREAD
#define HTS_DEFTHREAD

#include "htsglobal.h"
#include "httrack-library.h"
#ifndef _WIN32
#include <pthread.h>
#endif
#include "htswin32.h"
#ifndef USE_BEGINTHREAD
#error needs USE_BEGINTHREAD
#endif

#ifdef _WIN32
struct htsmutex_s {
  HANDLE handle;
};
#else /* #ifdef _WIN32 */
struct htsmutex_s {
  pthread_mutex_t handle;
};
#endif /* #ifdef _WIN32 */

/* Publish an int another thread polls, and observe one. A plain store and load
   carry no ordering. The compiler, or arm64 hardware, may move the payload
   after the flag and leave a reader with stale data. An aligned 32-bit access
   never tears, so only the ordering needs saying. */
static HTS_INLINE HTS_UNUSED void hts_store_release_int(int *dst, int value) {
#ifdef _MSC_VER
  MemoryBarrier();
  *(volatile int *) dst = value;
#else
  __atomic_store_n(dst, value, __ATOMIC_RELEASE);
#endif
}

static HTS_INLINE HTS_UNUSED int hts_load_acquire_int(const int *src) {
#ifdef _MSC_VER
  const int value = *(const volatile int *) src;

  MemoryBarrier();
  return value;
#else
  return __atomic_load_n(src, __ATOMIC_ACQUIRE);
#endif
}

/* Also runs 'tail(arg)' on the worker once the body is over, and only when this
   returns 0. A thread runner (see hts_set_thread_runner()) that recovers from a
   fault returns without running the rest of the body, so cleanup the engine
   needs goes here. Not exported, because no caller outside the library spawns a
   worker. */
HTS_CHECK_RESULT int hts_newthread_tail(void (*fun)(void *arg), void *arg,
                                        void (*tail)(void *arg));

/* HTS_TRUE where a thread runner recovered from a fault, so a worker stopped
   halfway through its body. Read on the crawl thread, which turns it into a
   stop in back_check_worker_fault(). */
hts_boolean hts_worker_faulted(void);
/* Forget any fault, and take the next round so that a worker an earlier mirror
   abandoned cannot abort this one. Called by a mirror as it starts. */
void hts_worker_fault_clear(void);

#ifdef HTS_INTERNAL_BYTECODE
/* Thread initialization */
void htsthread_init(void);
void htsthread_uninit(void);
#endif

#endif
