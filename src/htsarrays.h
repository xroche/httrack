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
/* File: Arrays                                                 */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTS_ARRAYS_DEFSTATIC
#define HTS_ARRAYS_DEFSTATIC

/* System definitions. */
#include <stdlib.h>
#include <string.h>

#include "htssafe.h"

/* Memory allocation assertion failure */
static void hts_record_assert_memory_failed(const size_t size) {
  fprintf(stderr, "memory allocation failed (%lu bytes)", \
          (long int) size); \
  assertf(! "memory allocation failed"); \
}

/** Dynamic array of T elements. **/
#define TypedArray(T) \
  struct {            \
    /** Elements. **/ \
    union {           \
      /** Typed. **/  \
      T* elts;        \
      /** Opaque. **/ \
      void* ptr;      \
    } data;           \
    /** Count. **/    \
    size_t size;      \
    /** Capacity. **/ \
    size_t capa;      \
  }
#define EMPTY_TYPED_ARRAY { { NULL }, 0, 0 }

/** Array size, in elements. **/
#define TypedArraySize(A)   ((A).size)

/** Array capacity, in elements. **/
#define TypedArrayCapa(A)   ((A).capa)

/**
 * Remaining free space, in elements. 
 * Macro, first element evaluated multiple times.
 **/
#define TypedArrayRoom(A)   ( TypedArrayCapa(A) - TypedArraySize(A) )

/** Array elements, of type T*. **/
#define TypedArrayElts(A)   ((A).data.elts)

/** Array pointer, of type void*. **/
#define TypedArrayPtr(A)    ((A).data.ptr)

/** Size of T. **/
#define TypedArrayWidth(A)  (sizeof(*TypedArrayElts(A)))

/** Nth element of the array. **/
#define TypedArrayNth(A, N) (TypedArrayElts(A)[N])

/**
 * Tail of the array (outside the array). 
 * The returned pointer points to the beginning of TypedArrayRoom(A)
 * free elements.
 **/
#define TypedArrayTail(A) (TypedArrayNth(A, TypedArraySize(A)))

/**
  * Ensure at least 'ROOM' elements can be put in the remaining space.
  * After a call to this macro, TypedArrayRoom(A) is guaranteed to be at 
  * least equal to 'ROOM'.
  **/
#define TypedArrayEnsureRoom(A, ROOM) do { \
  const size_t room_ = (ROOM); \
  while (TypedArrayRoom(A) < room_) { \
    TypedArrayCapa(A) = TypedArrayCapa(A) < 16 ? 16 : TypedArrayCapa(A) * 2; \
  } \
  TypedArrayPtr(A) = realloc(TypedArrayPtr(A), \
                             TypedArrayCapa(A)*TypedArrayWidth(A)); \
  if (TypedArrayPtr(A) == NULL) { \
    hts_record_assert_memory_failed(TypedArrayCapa(A)*TypedArrayWidth(A)); \
  } \
} while(0)

/** Add an element. Macro, first element evaluated multiple times. **/
#define TypedArrayAdd(A, E) do { \
  TypedArrayEnsureRoom(A, 1); \
  assertf(TypedArraySize(A) < TypedArrayCapa(A)); \
  TypedArrayTail(A) = (E); \
  TypedArraySize(A)++; \
} while(0)

/**
 * Add 'COUNT' elements from 'PTR'. 
 * Macro, first element evaluated multiple times.
 **/
#define TypedArrayAppend(A, PTR, COUNT) do { \
  const size_t count_ = (COUNT); \
  /* This 1-case is to benefit from type safety. */ \
  if (count_ == 1) { \
    TypedArrayAdd(A, *(PTR)); \
  } else { \
    const void *const source_ = (PTR); \
    TypedArrayEnsureRoom(A, count_); \
    assertf(count_ <= TypedArrayRoom(A)); \
    memcpy(&TypedArrayTail(A), source_, count_ * TypedArrayWidth(A)); \
    TypedArraySize(A) += count_; \
  } \
} while(0)

/** Clear an array, freeing memory and clearing size and capacity. **/
#define TypedArrayFree(A) do { \
  if (TypedArrayPtr(A) != NULL) { \
    TypedArrayCapa(A) = TypedArraySize(A) = 0; \
    free(TypedArrayPtr(A)); \
    TypedArrayPtr(A) = NULL; \
  } \
} while(0)

#endif
