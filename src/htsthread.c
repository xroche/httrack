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

/* Internal engine bytecode */
#define HTS_INTERNAL_BYTECODE

#include "htsglobal.h"
#include "htsbase.h"
#include "htsthread.h"
#include "httrack-library.h"

#if USE_BEGINTHREAD
#ifdef _WIN32
#include <process.h>
#endif
#endif

/* Outstanding threads, counted at spawn rather than by the child at entry, so
   that a caller which spawns and immediately waits still joins them (#747). */
static int process_chain = 0;
static htsmutex process_chain_mutex = HTSMUTEX_INIT;

static void process_chain_add(int delta) {
  hts_mutexlock(&process_chain_mutex);
  process_chain += delta;
  assertf(process_chain >= 0);
  hts_mutexrelease(&process_chain_mutex);
}

HTSEXT_API void htsthread_wait(void) {
  htsthread_wait_n(0);
}

HTSEXT_API void htsthread_wait_n(int n_wait) {
#if USE_BEGINTHREAD
  int wait = 0;

  do {
    hts_mutexlock(&process_chain_mutex);
    wait = (process_chain > n_wait);
    hts_mutexrelease(&process_chain_mutex);
    if (wait)
      Sleep(100);
  } while(wait);
#endif
}

/* ensure initialized */
HTSEXT_API void htsthread_init(void) {
#if USE_BEGINTHREAD
#if (defined(_DEBUG) || defined(DEBUG))
  assertf(process_chain == 0);
#endif
  if (process_chain_mutex == HTSMUTEX_INIT) {
    hts_mutexinit(&process_chain_mutex);
  }
#endif
}

HTSEXT_API void htsthread_uninit(void) {
  htsthread_wait();
#if USE_BEGINTHREAD
  hts_mutexfree(&process_chain_mutex);
#endif
}

typedef struct hts_thread_s {
  void *arg;
  void (*fun) (void *arg);
  void (*tail)(void *arg);
} hts_thread_s;

/* A body and whether it reached its end. */
typedef struct hts_body_s {
  void *arg;
  void (*fun)(void *arg);
  hts_boolean returned;
} hts_body_s;

static void hts_run_body(void *arg) {
  hts_body_s *const body = (hts_body_s *) arg;

  body->fun(body->arg);
  body->returned = HTS_TRUE;
}

/* Set by a worker a fault recovery cut short, read and cleared by the crawl
   thread. Process-global, like the runner that does the recovering, and a plain
   flag because a worker thread must not touch opt: the mirror may already have
   freed it (see dns_resolve_thread). */
static volatile hts_boolean worker_faulted = HTS_FALSE;

hts_boolean hts_worker_faulted(void) { return worker_faulted; }

void hts_worker_fault_clear(void) { worker_faulted = HTS_FALSE; }

/* Set once before any thread is spawned, hence unlocked. */
static void *(*thread_enter)(void) = NULL;
static void (*thread_leave)(void *cookie) = NULL;
static hts_thread_runner thread_runner = NULL;

HTSEXT_API void hts_set_thread_hooks(void *(*enter)(void),
                                     void (*leave)(void *cookie)) {
  /* Never half a pair: 'leave' must not see a cookie no 'enter' produced. */
  const int paired = enter != NULL && leave != NULL;

  thread_enter = paired ? enter : NULL;
  thread_leave = paired ? leave : NULL;
}

HTSEXT_API hts_thread_runner hts_set_thread_runner(hts_thread_runner runner) {
  const hts_thread_runner previous = thread_runner;

  thread_runner = runner;
  return previous;
}

#ifdef _WIN32
static unsigned int __stdcall hts_entry_point(void *tharg)
#else
static void *hts_entry_point(void *tharg)
#endif
{
  hts_thread_s *s_args = (hts_thread_s *) tharg;
  void *const arg = s_args->arg;
  void (*const tail)(void *arg) = s_args->tail;
  hts_body_s body;
  void *cookie;

  body.fun = s_args->fun;
  body.arg = arg;
  body.returned = HTS_FALSE;
  freet(tharg);

  cookie = thread_enter != NULL ? thread_enter() : NULL;
  /* run */
  if (thread_runner != NULL)
    thread_runner(hts_run_body, &body);
  else
    hts_run_body(&body);
  /* Nothing can audit what the fault left behind, so the mirror gives up. The
     crawl thread performs it, being the one that holds opt. */
  if (!body.returned)
    worker_faulted = HTS_TRUE;
  /* Not at the end of the body, because a recovered fault never gets there. */
  if (tail != NULL)
    tail(arg);
  if (thread_leave != NULL)
    thread_leave(cookie);

  process_chain_add(-1);
#ifdef _WIN32
  return 0;
#else
  return NULL;
#endif
}

/* create a thread */
HTSEXT_API int hts_newthread(void (*fun) (void *arg), void *arg) {
  return hts_newthread_tail(fun, arg, NULL);
}

int hts_newthread_tail(void (*fun)(void *arg), void *arg,
                       void (*tail)(void *arg)) {
  hts_thread_s *s_args = malloct(sizeof(hts_thread_s));

  assertf(s_args != NULL);
  s_args->arg = arg;
  s_args->fun = fun;
  s_args->tail = tail;
  process_chain_add(1);
#ifdef _WIN32
  {
    unsigned int idt;
    HANDLE handle =
      (HANDLE) _beginthreadex(NULL, 0, hts_entry_point, s_args, 0, &idt);
    if (handle == 0) {
      process_chain_add(-1);
      freet(s_args);
      return -1;
    } else {
      /* detach the thread from the main process so that is can be independent */
      CloseHandle(handle);
    }
  }
#else
  {
    const size_t stackSize = 1024 * 1024 * 8;
    pthread_attr_t attr;
    pthread_t handle = 0;
    hts_boolean created;

    /* init kept apart: destroying an uninitialised attr is undefined (#772) */
    if (pthread_attr_init(&attr) == 0) {
      created = pthread_attr_setstacksize(&attr, stackSize) == 0 &&
                pthread_create(&handle, &attr, hts_entry_point, s_args) == 0;
      pthread_attr_destroy(&attr); /* create() copied what it needed */
    } else {
      created = HTS_FALSE;
    }
    if (!created) {
      process_chain_add(-1);
      freet(s_args);
      return -1;
    }
    /* detach the thread from the main process so that it can be independent */
    pthread_detach(handle);
  }
#endif
  return 0;
}

#if USE_BEGINTHREAD

/* Note: new 3.41 cleaned up functions. */

HTSEXT_API void hts_mutexinit(htsmutex * mutex) {
  htsmutex_s *smutex = malloct(sizeof(htsmutex_s));

#ifdef _WIN32
  smutex->handle = CreateMutex(NULL, FALSE, NULL);
#else
  pthread_mutex_init(&smutex->handle, 0);
#endif
  *mutex = smutex;
}

HTSEXT_API void hts_mutexfree(htsmutex * mutex) {
  if (mutex != NULL && *mutex != NULL) {
#ifdef _WIN32
    CloseHandle((*mutex)->handle);
#else
    pthread_mutex_destroy(&((*mutex)->handle));
#endif
    freet(*mutex);
    *mutex = NULL;
  }
}

HTSEXT_API void hts_mutexlock(htsmutex * mutex) {
  assertf(mutex != NULL);
  if (*mutex == HTSMUTEX_INIT) {        /* must be initialized */
    /* Initialize exactly once, even when several threads race to lock the same
       mutex for the first time. Build our own object, then publish it with a
       single atomic compare-and-swap; the threads that lose the race free the
       object they built (issue #297). No static guard is needed, which keeps
       this safe on Windows 2000 (no statically-initializable lock there). */
    htsmutex created = HTSMUTEX_INIT;

    hts_mutexinit(&created);
#ifdef _WIN32
    if (InterlockedCompareExchangePointer((PVOID volatile *) mutex, created,
                                          HTSMUTEX_INIT) != HTSMUTEX_INIT)
#else
    if (!__sync_bool_compare_and_swap(mutex, HTSMUTEX_INIT, created))
#endif
    {
      hts_mutexfree(&created);
    }
  }
  assertf(*mutex != NULL);
#ifdef _WIN32
  assertf((*mutex)->handle != NULL);
  WaitForSingleObject((*mutex)->handle, INFINITE);
#else
  pthread_mutex_lock(&(*mutex)->handle);
#endif
}

HTSEXT_API void hts_mutexrelease(htsmutex * mutex) {
  assertf(mutex != NULL && *mutex != NULL);
#ifdef _WIN32
  assertf((*mutex)->handle != NULL);
  ReleaseMutex((*mutex)->handle);
#else
  pthread_mutex_unlock(&(*mutex)->handle);
#endif
}

#endif
