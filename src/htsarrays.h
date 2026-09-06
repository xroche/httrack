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
 *  Header-only generic dynamic array (a typed growable vector). All operations
 *  are macros parameterized by the array lvalue A; the element type T is fixed
 *  by the struct TypedArray(T) declares. Counts and capacities are in
 *  elements, not bytes. The array owns its backing store: grow it via the Add/
 *  Append/EnsureRoom macros and release it with TypedArrayFree. */
#ifndef HTS_ARRAYS_DEFSTATIC
#define HTS_ARRAYS_DEFSTATIC

/* System definitions. */
#include <stdlib.h>
#include <string.h>

#include "htssafe.h"

/* Abort on a NULL the caller has no way to fail gracefully. Not what the array
   macros do: they leave the array as it was and let the caller notice. */
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
    /** Count. **/                                                             \
    size_t size;                                                               \
    /** Capacity. **/                                                          \
    size_t capa;                                                               \
  }

/** Initializer for an empty array (no backing store, size and capacity 0). **/
#define EMPTY_TYPED_ARRAY {{NULL}, 0, 0}

/** Array size, in elements. **/
#define TypedArraySize(A) ((A).size)

/** Array capacity, in elements. **/
#define TypedArrayCapa(A) ((A).capa)

/**
 * Remaining free space, in elements.
 * Macro, first element evaluated multiple times.
 **/
#define TypedArrayRoom(A) (TypedArrayCapa(A) - TypedArraySize(A))

/** Array elements, of type T*. **/
#define TypedArrayElts(A) ((A).data.elts)

/** Array pointer, of type void*. **/
#define TypedArrayPtr(A) ((A).data.ptr)

/** Size of T. **/
#define TypedArrayWidth(A) (sizeof(*TypedArrayElts(A)))

/** Nth element of the array, as an lvalue. No bounds check; N must be
    < TypedArraySize(A). **/
#define TypedArrayNth(A, N) (TypedArrayElts(A)[N])

/**
 * Tail of the array (outside the array).
 * The returned pointer points to the beginning of TypedArrayRoom(A)
 * free elements.
 **/
#define TypedArrayTail(A) (TypedArrayNth(A, TypedArraySize(A)))

/**
 * Can 'ROOM' more elements be put in the array?
 * This is how a growth failure is reported: TypedArrayEnsureRoom(A, ROOM)
 * leaves the array untouched when it cannot allocate, so the caller asks
 * again. Macro, first element evaluated multiple times.
 **/
#define TypedArrayHasRoom(A, ROOM) (TypedArrayRoom(A) >= (ROOM))

/**
 * Ensure at least 'ROOM' elements can be put in the remaining space.
 * On success TypedArrayRoom(A) is at least 'ROOM'. An allocation failure
 * leaves contents, size and capacity as they were, for the caller to notice
 * with TypedArrayHasRoom(). Aborts only if the total would not fit a size_t,
 * which is a caller bug.
 **/
#define TypedArrayEnsureRoom(A, ROOM)                                          \
  do {                                                                         \
    const size_t room_ = (ROOM);                                               \
    /* Largest element count whose byte size still fits a size_t. */           \
    const size_t maxCapa_ = (size_t) -1 / TypedArrayWidth(A);                  \
    const size_t minCapa_ = maxCapa_ < 16 ? maxCapa_ : 16;                     \
    size_t capa_ = TypedArrayCapa(A);                                          \
    void *newPtr_;                                                             \
    assertf(room_ <= maxCapa_ - TypedArraySize(A));                            \
    /* Saturating: a plain capa_*2 wraps to 0 and the loop never ends. */      \
    while (capa_ - TypedArraySize(A) < room_) {                                \
      capa_ = capa_ < minCapa_       ? minCapa_                                \
              : capa_ > maxCapa_ / 2 ? maxCapa_                                \
                                     : capa_ * 2;                              \
    }                                                                          \
    /* Via a temporary: realloc keeps the old block on failure. */             \
    newPtr_ = realloct(TypedArrayPtr(A), capa_ * TypedArrayWidth(A));          \
    if (newPtr_ != NULL) {                                                     \
      TypedArrayPtr(A) = newPtr_;                                              \
      TypedArrayCapa(A) = capa_;                                               \
    }                                                                          \
  } while (0)

/** Add an element, if the array can be grown to hold it. Macro, first element
    evaluated multiple times. **/
#define TypedArrayAdd(A, E)                                                    \
  do {                                                                         \
    TypedArrayEnsureRoom(A, 1);                                                \
    if (TypedArrayHasRoom(A, 1)) {                                             \
      TypedArrayTail(A) = (E);                                                 \
      TypedArraySize(A)++;                                                     \
    }                                                                          \
  } while (0)

/**
 * Add 'COUNT' elements from 'PTR', if the array can be grown to hold them.
 * Macro, first element evaluated multiple times.
 **/
#define TypedArrayAppend(A, PTR, COUNT)                                        \
  do {                                                                         \
    const size_t count_ = (COUNT);                                             \
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

/** Clear an array, freeing memory and clearing size and capacity. **/
#define TypedArrayFree(A)                                                      \
  do {                                                                         \
    if (TypedArrayPtr(A) != NULL) {                                            \
      TypedArrayCapa(A) = TypedArraySize(A) = 0;                               \
      freet(TypedArrayPtr(A));                                                 \
    }                                                                          \
  } while (0)

#endif
