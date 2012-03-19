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

#ifndef HTS_DEFTHREAD
#define HTS_DEFTHREAD

#include "htsglobal.h"
#if USE_PTHREAD
#include <pthread.h>    /* _beginthread, _endthread */
#endif
#if HTS_WIN
#include "windows.h"
#endif

#if USE_BEGINTHREAD
#if HTS_WIN

#define PTHREAD_RETURN
#define PTHREAD_TYPE void __cdecl
#define PTHREAD_LOCK_TYPE HANDLE

/* Useless - see '__declspec( thread )' */
/*
#define PTHREAD_KEY_TYPE void*
#define PTHREAD_KEY_CREATE(ptrkey, uninit)      do { *(ptrkey)=(void*)NULL; } while(0)
#define PTHREAD_KEY_DELETE(key)                 do { key=(void*)NULL; } while(0)
#define PTHREAD_KEY_SET(key, val, ptrtype)      do { key=(void*)(val); } while(0)
#define PTHREAD_KEY_GET(key, ptrval, ptrtype)   do { *(ptrval)=(ptrtype)(key); } while(0)
*/

#else

#define PTHREAD_RETURN NULL
#define PTHREAD_TYPE void*
#define PTHREAD_LOCK_TYPE pthread_mutex_t
#define PTHREAD_KEY_TYPE pthread_key_t
#define PTHREAD_KEY_CREATE(ptrkey, uninit)      pthread_key_create(ptrkey, uninit)
#define PTHREAD_KEY_DELETE(key)                 pthread_key_delete(key)
#define PTHREAD_KEY_SET(key, val, ptrtype)      pthread_setspecific(key, (void*)val)
#define PTHREAD_KEY_GET(key, ptrval, ptrtype)   do { *(ptrval)=(ptrtype)pthread_getspecific(key); } while(0)

#endif

#else

#define PTHREAD_LOCK_TYPE void*
#define PTHREAD_KEY_TYPE void*
#define PTHREAD_KEY_CREATE(ptrkey, uninit)      do { *(ptrkey)=(void*)NULL; } while(0)
#define PTHREAD_KEY_DELETE(key)                 do { key=(void*)NULL; } while(0)
#define PTHREAD_KEY_SET(key, val, ptrtype)      do { key=(void*)(val); } while(0)
#define PTHREAD_KEY_GET(key, ptrval, ptrtype)   do { *(ptrval)=(ptrtype)(key); } while(0)

#endif

int htsSetLock(PTHREAD_LOCK_TYPE * hMutex,int lock);

#if USE_PTHREAD
unsigned long _beginthread( void* ( *start_address )( void * ), unsigned stack_size, void *arglist );
#endif

#endif

