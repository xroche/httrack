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

static int process_chain = 0;
static htsmutex process_chain_mutex = HTSMUTEX_INIT;

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
} hts_thread_s;

#ifdef _WIN32
static unsigned int __stdcall hts_entry_point(void *tharg)
#else
static void *hts_entry_point(void *tharg)
#endif
{
  hts_thread_s *s_args = (hts_thread_s *) tharg;
  void *const arg = s_args->arg;
  void (*fun) (void *arg) = s_args->fun;

  free(tharg);

  hts_mutexlock(&process_chain_mutex);
  process_chain++;
  assertf(process_chain > 0);
  hts_mutexrelease(&process_chain_mutex);

  /* run */
  fun(arg);

  hts_mutexlock(&process_chain_mutex);
  process_chain--;
  assertf(process_chain >= 0);
  hts_mutexrelease(&process_chain_mutex);
#ifdef _WIN32
  return 0;
#else
  return NULL;
#endif
}

/* create a thread */
HTSEXT_API int hts_newthread(void (*fun) (void *arg), void *arg) {
  hts_thread_s *s_args = malloc(sizeof(hts_thread_s));

  assertf(s_args != NULL);
  s_args->arg = arg;
  s_args->fun = fun;
#ifdef _WIN32
  {
    unsigned int idt;
    HANDLE handle =
      (HANDLE) _beginthreadex(NULL, 0, hts_entry_point, s_args, 0, &idt);
    if (handle == 0) {
      free(s_args);
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
    int retcode;

    if (pthread_attr_init(&attr) != 0
        || pthread_attr_setstacksize(&attr, stackSize) != 0
        || (retcode =
            pthread_create(&handle, &attr, hts_entry_point, s_args)) != 0) {
      free(s_args);
      return -1;
    } else {
      /* detach the thread from the main process so that is can be independent */
      pthread_detach(handle);
      pthread_attr_destroy(&attr);
    }
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
    hts_mutexinit(mutex);
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
