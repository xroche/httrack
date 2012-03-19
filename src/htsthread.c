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


#include "htsglobal.h"
#include "htsthread.h"

// Threads - emulate _beginthread under Linux/Unix using pthread_XX
// Some changes will have to be done, see PTHREAD_RETURN,PTHREAD_TYPE
#if USE_PTHREAD
#include <pthread.h>    /* _beginthread, _endthread */

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

#if USE_BEGINTHREAD
/* 
  Simple lock function

  Return value: always 0
  Parameter:
  1 wait for lock (mutex) available and lock it
  0 unlock the mutex
  [-1 check if locked (always return 0 with mutex)]
  -999 initialize
*/
int htsSetLock(PTHREAD_LOCK_TYPE* hMutex,int lock) {
#if HTS_WIN
  /* lock */
  if (lock==1)
    WaitForSingleObject(*hMutex,INFINITE);
  /* unlock */
  else if (lock==0)
    ReleaseMutex(*hMutex);
  /* create */
  else if (lock==-999)
    *hMutex=CreateMutex(NULL,FALSE,NULL);
#else
  /* lock */
  if (lock==1)
    pthread_mutex_lock(hMutex);
  /* unlock */
  else if (lock==0)
    pthread_mutex_unlock(hMutex);
  /* create */
  else if (lock==-999)
    pthread_mutex_init(hMutex,0);
#endif
  return 0;
}

#endif

