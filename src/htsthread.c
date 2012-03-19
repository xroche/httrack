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
/* File: Threads                                                */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Internal engine bytecode */
#define HTS_INTERNAL_BYTECODE

#include "htsglobal.h"
#include "htsbase.h"
#include "htsthread.h"

#if USE_BEGINTHREAD
#if HTS_WIN
#include <process.h>
#endif
#endif

static int process_chain = 0;
static PTHREAD_LOCK_TYPE process_chain_mutex;

HTSEXT_API void htsthread_wait(void ) {
  htsthread_wait_n(0);
}

HTSEXT_API void htsthread_wait_n(int n_wait) {
#if USE_BEGINTHREAD
  int wait = 0;
  do {
    htsSetLock(&process_chain_mutex, 1);
    wait = (process_chain > n_wait );
    htsSetLock(&process_chain_mutex, 0);
    if (wait)
      Sleep(100);
  } while(wait);
#endif
}

HTSEXT_API void htsthread_init(void ) {
#if USE_BEGINTHREAD
  assertf(process_chain == 0);
  htsSetLock(&process_chain_mutex, -999);
#endif
}

HTSEXT_API void htsthread_uninit(void ) {
  htsthread_wait();
#if USE_BEGINTHREAD
  htsSetLock(&process_chain_mutex, -998);
#endif
}

typedef struct {
  PTHREAD_TYPE ( PTHREAD_TYPE_FNC *start_address )( void * );
  void** arglist;
} execth_args;
static PTHREAD_TYPE PTHREAD_TYPE_FNC execth( void * arg )
{
  execth_args* args = (execth_args*) arg;
  assertf(args != NULL);

  htsSetLock(&process_chain_mutex, 1);
  process_chain++;
  assertf(process_chain > 0);
  htsSetLock(&process_chain_mutex, 0);

  (void) args->start_address(args->arglist);

  htsSetLock(&process_chain_mutex, 1);
  process_chain--;
  assertf(process_chain >= 0);
  htsSetLock(&process_chain_mutex, 0);

  free(arg);
  return PTHREAD_RETURN;
}


HTSEXT_API int hts_newthread( PTHREAD_TYPE ( PTHREAD_TYPE_FNC *start_address )( void * ), unsigned stack_size, void *arglist )
{
  execth_args* args = (execth_args*) malloc(sizeof(execth_args));
  assertf(args != NULL);
  args->start_address = start_address;
  args->arglist = arglist;

  /* create a thread */
#ifdef _WIN32
  if (_beginthread(execth, stack_size, args) == -1) {
    free(args);
    return -1;
  }
#else
  {
    PTHREAD_HANDLE handle = 0;
    int retcode;
    retcode = pthread_create(&handle, NULL, execth, args);
    if (retcode != 0) {   /* error */
      free(args);
      return -1;
    } else {
      /* detach the thread from the main process so that is can be independent */
      pthread_detach(handle);
    }
  }
#endif
  return 0;
}


// Threads - emulate _beginthread under Linux/Unix using pthread_XX
// Some changes will have to be done, see PTHREAD_RETURN,PTHREAD_TYPE
#if USE_PTHREAD
#include <pthread.h>    /* _beginthread, _endthread */
#if 0
unsigned long _beginthread( void* ( *start_address )( void * ), unsigned stack_size, void *arglist )
{
  pthread_t th;
  int retcode;
  /* create a thread */
  retcode = pthread_create(&th, NULL, start_address, arglist);
  if (retcode != 0)     /* error */
    return -1;
  /* detach the thread from the main process so that is can be independent */
  pthread_detach(th);
  return 0;
}
#endif
#endif

#if USE_BEGINTHREAD
/* 
  Simple lock function

  Return value: always 0
  Parameter:
  1 wait for lock (mutex) available and lock it
  0 unlock the mutex
  [-1 check if locked (always return 0 with mutex)]
  -999 initialize
  -998 free
  */
HTSEXT_API int htsSetLock(PTHREAD_LOCK_TYPE* hMutex,int lock) {
#if HTS_WIN
  /* lock */
  switch(lock) {
   case 1:    /* lock */
     assertf(*hMutex != NULL);
     WaitForSingleObject(*hMutex,INFINITE);
     break;
   case 0:     /* unlock */
     assertf(*hMutex != NULL);
     ReleaseMutex(*hMutex);
     break;
   case -999:  /* create */
     *hMutex=CreateMutex(NULL,FALSE,NULL);
     break;
   case -998:  /* destroy */
     CloseHandle(*hMutex);
     *hMutex = NULL;
     break;
   default:
     assert(FALSE);
     break;
  }
#else
  switch(lock) {
   case 1:    /* lock */
     pthread_mutex_lock(hMutex);
     break;
   case 0:     /* unlock */
     pthread_mutex_unlock(hMutex);
     break;
   case -999:  /* create */
     pthread_mutex_init(hMutex,0);
     break;
   case -998:  /* destroy */
     pthread_mutex_destroy(hMutex);
     break;
   default:
     assert(0);
     break;
  }
#endif
  return 0;
}

#endif

