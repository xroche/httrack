/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 2014 Xavier Roche and other contributors

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
/* File: Arrays                                                 */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/** @file htsarrays.h
 *  Header-only typed dynamic array over the lvalue A, counted in elements.
 *  Most macros evaluate A several times, so A must have no side effect. */
#ifndef HTS_ARRAYS_DEFSTATIC
#define HTS_ARRAYS_DEFSTATIC

/* System definitions. */
#include <stdlib.h>
#include <string.h>

#include "htssafe.h"

/** Report a failed allocation of @p size bytes and abort. Call it where the
    caller has no way to fail gracefully. The array macros below never call it,
    because they leave the array as it was and let the caller notice. */
static HTS_UNUSED void hts_record_assert_memory_failed(const size_t size) {
  fprintf(stderr, "memory allocation failed (%lu bytes)", (long int) size);
  assertf(!"memory allocation failed");
}

/** Dynamic array of T elements. **/
#define TypedArray(T)                                                          \
  struct {                                                                     \
    /** Elements. **/                                                          \
    union {                                                                    \
      /** Typed. **/                                                           \
      T *elts;                                                                 \
      /** Opaque. **/                                                          \
      void *ptr;                                                               \
    } data;                                                                    \
    /** Element count. **/                                                     \
    size_t size;                                                               \
    /** Capacity, in elements. **/                                             \
    size_t capa;                                                               \
  }

/** Initializer for an empty array (no backing store, size and capacity 0). **/
#define EMPTY_TYPED_ARRAY {{NULL}, 0, 0}

/** Array size, in elements. **/
#define TypedArraySize(A) ((A).size)

/** Array capacity, in elements. **/
#define TypedArrayCapa(A) ((A).capa)

/** Remaining free space, in elements. **/
#define TypedArrayRoom(A) (TypedArrayCapa(A) - TypedArraySize(A))

/** Array elements, of type T*. A grow can move the store, so do not keep this
    pointer across an Add, Push, Append, Reserve or EnsureRoom. **/
#define TypedArrayElts(A) ((A).data.elts)

/** Array pointer, of type void*. **/
#define TypedArrayPtr(A) ((A).data.ptr)

/** Size of one element, in bytes. **/
#define TypedArrayWidth(A) (sizeof(*TypedArrayElts(A)))

/** Nth element of the array, as an lvalue. No bounds check; N must be
    < TypedArraySize(A). **/
#define TypedArrayNth(A, N) (TypedArrayElts(A)[N])

/** First free slot past the end, as an lvalue of the element type. It starts a
    run of TypedArrayRoom(A) free elements, so it is unusable when that room is
    0. **/
#define TypedArrayTail(A) (TypedArrayNth(A, TypedArraySize(A)))

/**
 * Can 'ROOM' more elements be put in the array?
 * This is how a growth failure is reported: TypedArrayEnsureRoom(A, ROOM)
 * leaves the array untouched when it cannot allocate, so the caller asks again.
 **/
#define TypedArrayHasRoom(A, ROOM) (TypedArrayRoom(A) >= (ROOM))

/** Make room for ROOM more elements of WIDTH bytes past SIZE in the store
    *PTR of *CAPA elements. HTS_FALSE leaves both unchanged: the allocation
    failed, or the room would not fit a size_t. Use TypedArrayReserve(). **/
static HTS_UNUSED HTS_CHECK_RESULT hts_boolean
hts_array_reserve_(void **const ptr, size_t *const capa, const size_t size,
                   const size_t room, const size_t width) {
  /* Largest element count whose byte size still fits a size_t. */
  const size_t maxCapa = (size_t) -1 / width;
  const size_t minCapa = maxCapa < 16 ? maxCapa : 16;
  size_t newCapa = *capa;
  void *newPtr;

  /* ASan copies the whole store on a same-size realloc. */
  if (*capa - size >= room)
    return HTS_TRUE;
  if (room > maxCapa - size)
    return HTS_FALSE;
  /* Saturating: a plain newCapa*2 wraps to 0 and the loop never ends. */
  while (newCapa - size < room) {
    newCapa = newCapa < minCapa       ? minCapa
              : newCapa > maxCapa / 2 ? maxCapa
                                      : newCapa * 2;
  }
  /* Via a temporary: realloc keeps the old block on failure. */
  newPtr = realloct(*ptr, newCapa * width);
  if (newPtr == NULL)
    return HTS_FALSE;
  *ptr = newPtr;
  *capa = newCapa;
  return HTS_TRUE;
}

/** Make room for 'ROOM' more elements. Returns HTS_TRUE when there is room,
    HTS_FALSE with the array unchanged when there is not, overflow included. **/
#define TypedArrayReserve(A, ROOM)                                             \
  hts_array_reserve_(&TypedArrayPtr(A), &TypedArrayCapa(A), TypedArraySize(A), \
                     (ROOM), TypedArrayWidth(A))

/** Add element 'E' at the end. Returns HTS_TRUE, or HTS_FALSE with the array
    unchanged when the store cannot grow. 'E' is read after any grow. **/
#define TypedArrayPush(A, E)                                                   \
  (TypedArrayReserve(A, 1)                                                     \
       ? (TypedArrayTail(A) = (E), TypedArraySize(A)++, HTS_TRUE)              \
       : HTS_FALSE)

/** Return N, aborting unless N < SIZE. Use TypedArrayAt(). **/
static HTS_UNUSED size_t hts_array_index_(const size_t n, const size_t size) {
  assertf(n < size);
  return n;
}

/** Nth element of the array, as an lvalue. Aborts unless N < size. **/
#define TypedArrayAt(A, N)                                                     \
  TypedArrayNth(A, hts_array_index_((N), TypedArraySize(A)))

/**
 * Ensure at least 'ROOM' more elements can be put in the array.
 * A failed allocation leaves the array unchanged (check TypedArrayHasRoom()).
 * A byte-count overflow aborts.
 **/
#define TypedArrayEnsureRoom(A, ROOM)                                          \
  do {                                                                         \
    const size_t room_ = (ROOM);                                               \
    /* Largest element count whose byte size still fits a size_t. */           \
    const size_t maxCapa_ = (size_t) -1 / TypedArrayWidth(A);                  \
    assertf(room_ <= maxCapa_ - TypedArraySize(A));                            \
    /* Callers learn of a failed grow from TypedArrayHasRoom(). */             \
    if (!TypedArrayReserve(A, room_))                                          \
      break;                                                                   \
  } while (0)

/** Add element 'E' at the end. The array is left untouched when the store
    cannot grow. **/
#define TypedArrayAdd(A, E)                                                    \
  do {                                                                         \
    TypedArrayEnsureRoom(A, 1);                                                \
    if (TypedArrayHasRoom(A, 1)) {                                             \
      TypedArrayTail(A) = (E);                                                 \
      TypedArraySize(A)++;                                                     \
    }                                                                          \
  } while (0)

/**
 * Add 'COUNT' elements, not bytes, read from 'PTR'.
 * A 'COUNT' of 0 does nothing. The array is left untouched when the store
 * cannot grow. 'PTR' must not point into the array, because a grow frees the
 * old store. A 'COUNT' the array could never address aborts, see
 * TypedArrayEnsureRoom().
 **/
#define TypedArrayAppend(A, PTR, COUNT)                                        \
  do {                                                                         \
    const size_t count_ = (COUNT);                                             \
    /* An array that never grew has no store, and a zero offset off a NULL     \
       pointer is undefined even where nothing is copied. */                   \
    if (count_ == 0) {                                                         \
      break;                                                                   \
    }                                                                          \
    /* This 1-case is to benefit from type safety. */                          \
    if (count_ == 1) {                                                         \
      TypedArrayAdd(A, *(PTR));                                                \
    } else {                                                                   \
      const void *const source_ = (PTR);                                       \
      TypedArrayEnsureRoom(A, count_);                                         \
      if (TypedArrayHasRoom(A, count_)) {                                      \
        memcpy(&TypedArrayTail(A), source_, count_ *TypedArrayWidth(A));       \
        TypedArraySize(A) += count_;                                           \
      }                                                                        \
    }                                                                          \
  } while (0)

/** Free the store and set size and capacity back to 0. **/
#define TypedArrayFree(A)                                                      \
  do {                                                                         \
    if (TypedArrayPtr(A) != NULL) {                                            \
      TypedArrayCapa(A) = TypedArraySize(A) = 0;                               \
      freet(TypedArrayPtr(A));                                                 \
    }                                                                          \
  } while (0)

#endif
