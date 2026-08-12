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

Please visit our Website: http://www.httrack.com
*/

/* ------------------------------------------------------------ */
/* File: htsarena.h subroutines:                                */
/*       bump allocator whose allocations never move            */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTSARENA_DEFH
#define HTSARENA_DEFH

#include "htsglobal.h"
#include "htssafe.h"

#include <string.h>

/* A bump allocator over chunks that are never resized, so what it hands out
   keeps its address. That is what the realloc-based String and TypedArray
   cannot promise, and what the link records and the rebuilt command line need,
   both holding raw pointers into their own storage. Nothing is released until
   the whole arena is. */

/* Alignment fit for anything an arena is asked to hold. */
typedef union {
  void *p;
  long l;
  double d;
} hts_arena_align;

typedef struct hts_arena_chunk hts_arena_chunk;

struct hts_arena_chunk {
  hts_arena_chunk *next;
};

typedef struct {
  hts_arena_chunk *chunks; /* newest first; allocations are cut from it */
  size_t size;             /* the newest chunk, used of size bytes taken */
  size_t used;
} hts_arena;

#define HTS_ARENA_ALIGN sizeof(hts_arena_align)

/* Smallest chunk, then doubling up to the largest, so an arena holding a lot
   keeps few chunks and one holding little wastes none. */
#define HTS_ARENA_MIN 32768
#define HTS_ARENA_MAX (16 * 1024 * 1024)

/* First aligned offset past the chunk header, where its bytes begin. */
#define HTS_ARENA_HDR                                                          \
  ((sizeof(hts_arena_chunk) + HTS_ARENA_ALIGN - 1) &                           \
   ~(size_t) (HTS_ARENA_ALIGN - 1))

/* SIZE rounded up to the arena's alignment, or 0 if that would wrap. */
static HTS_INLINE HTS_UNUSED size_t hts_arena_round_(size_t size) {
  return size <= (size_t) -1 - (HTS_ARENA_ALIGN - 1)
             ? (size + HTS_ARENA_ALIGN - 1) & ~(size_t) (HTS_ARENA_ALIGN - 1)
             : 0;
}

/* Take a chunk with room for NEED bytes; ARENA is unchanged on failure. */
static HTS_UNUSED hts_boolean hts_arena_grow_(hts_arena *arena, size_t need) {
  size_t size = arena->size < HTS_ARENA_MAX / 2 ? arena->size * 2
                                                : (size_t) HTS_ARENA_MAX;
  hts_arena_chunk *chunk;

  if (size < HTS_ARENA_MIN)
    size = HTS_ARENA_MIN;
  if (size < need)
    size = need;
  if (size > (size_t) -1 - HTS_ARENA_HDR)
    return HTS_FALSE;
  chunk = (hts_arena_chunk *) malloct(HTS_ARENA_HDR + size);
  if (chunk == NULL)
    return HTS_FALSE;
  chunk->next = arena->chunks;
  arena->chunks = chunk;
  arena->size = size;
  arena->used = 0;
  return HTS_TRUE;
}

/* SIZE bytes from ARENA, aligned for any type, or NULL when out of memory.
   The address stays valid until hts_arena_free(). */
static HTS_UNUSED void *hts_arena_alloc(hts_arena *arena, size_t size) {
  const size_t need = hts_arena_round_(size);

  if (need == 0 && size != 0) /* the rounding wrapped */
    return NULL;
  /* need alone on one side: used never passes size, so the difference holds */
  if (arena->chunks == NULL || need > arena->size - arena->used) {
    if (!hts_arena_grow_(arena, need))
      return NULL;
  }
  arena->used += need;
  return (char *) arena->chunks + HTS_ARENA_HDR + arena->used - need;
}

/* A copy of S in ARENA, or NULL when out of memory. */
static HTS_UNUSED char *hts_arena_strdup(hts_arena *arena, const char *s) {
  const size_t len = strlen(s) + 1;
  char *const copy = (char *) hts_arena_alloc(arena, len);

  if (copy != NULL)
    memcpy(copy, s, len);
  return copy;
}

/* Release every chunk, and with them everything the arena handed out. */
static HTS_UNUSED void hts_arena_free(hts_arena *arena) {
  hts_arena_chunk *chunk = arena->chunks;

  while (chunk != NULL) {
    hts_arena_chunk *const next = chunk->next;

    freet(chunk);
    chunk = next;
  }
  memset(arena, 0, sizeof(*arena));
}

#endif
