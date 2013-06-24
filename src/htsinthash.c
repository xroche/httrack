/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) Xavier Roche and other contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 3
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
/* File: httrack.c subroutines:                                 */
/*       hash tables                                            */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Internal engine bytecode */
#define HTS_INTERNAL_BYTECODE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

#include "htsinthash.h"

#define USES_MD5

#ifdef USES_MD5
#include "htsmd5.h"
#endif

/** Hashtable. **/
struct struct_inthash {
  inthash_item *items;
  size_t nitems;
  t_inthash_freehandler free_handler;
  size_t hash_size_power;
  inthash_item stash[STASH_SIZE];
  size_t stash_size;
  char *string_pool;
  size_t string_pool_size;
  size_t string_pool_capacity;
  size_t string_pool_chars;
  unsigned char flag_valueismalloc;
};

#ifdef LIBHTTRACK_EXPORTS
#include "htsbase.h"
#define inthash_assert assertf
#else
static void inthash_fail(const char* exp, const char* file, int line) {
  fprintf(stderr, "assertion '%s' failed at %s:%d\n", exp, file, line);
  abort();
}
#define inthash_assert(EXP) (void)( (EXP) || (inthash_fail(#EXP, __FILE__, __LINE__), 0) )
#define HTS_PRINTF_FUN(FMT, ARGS)
#define HTS_INLINE
#endif

/* Numerical Recipes function. */
#define HASH_PRIME ( 1664525 )
#define HASH_CONST ( 1013904223 )
#define HASH_ADD(HASH, C) do {                  \
    (HASH) *= HASH_PRIME;                       \
    (HASH) += HASH_CONST;                       \
    (HASH) ^= ( (C) << 24);                     \
  } while(0)

/* 2**X */
#define POW2(X) ( (size_t) 1 << (X) )

static void inthash_log(inthash hashtable,
                        const char *format, ...)
                        HTS_PRINTF_FUN(2, 3) {
  va_list args;
  inthash_assert(format != NULL);
  va_start(args, format);
  fprintf(stderr, "[%p] ", (void*) hashtable);
  (void) vfprintf(stderr, format, args);
  putc('\n', stderr);
  va_end(args);
}

#ifndef USES_MD5
static unsigned int rev32(unsigned int v) {
  /* swap odd and even bits */
  v = ((v >> 1) & 0x55555555) | ((v & 0x55555555) << 1);
  /* swap consecutive pairs */
  v = ((v >> 2) & 0x33333333) | ((v & 0x33333333) << 2);
  /* swap nibbles ...  */
  v = ((v >> 4) & 0x0F0F0F0F) | ((v & 0x0F0F0F0F) << 4);
  /* swap bytes */
  v = ((v >> 8) & 0x00FF00FF) | ((v & 0x00FF00FF) << 8);
  /* swap 2-byte long pairs */
  v = ( v >> 16             ) | ( v               << 16);
  return v;
}
#endif

static inthash_keys inthash_calc_hashes(const char *value) {
#ifdef USES_MD5
  union {
    char md5digest[16];
    inthash_keys hashes;
  } u;
  domd5mem(value, strlen(value), u.md5digest, 0);
  return u.hashes;
#else
  /* compute two lcg hashes using different seeds */
  inthash_keys hash = { 0, ~0 };
  size_t i;
  for(i = 0 ; value[i] != '\0' ; i++) {
    const unsigned char c1 = value[i];
    const unsigned char c2 = 0xff - c1;
    HASH_ADD(hash.hash1, c1);
    HASH_ADD(hash.hash2, c2);
  }
  /* yes, it may happen */
  if (hash.hash1 == hash.hash2) {
    hash.hash2 = ~hash.hash2;
  }
  /* reverse bits: "higher-order bits have longer periods than the lower 
     order bits" (Wikipedia) */
  hash.hash1 ^= rev32(hash.hash1);
  hash.hash2 ^= rev32(hash.hash2);
  return hash;
#endif
}

static HTS_INLINE int inthash_is_free(const inthash hashtable, size_t pos) {
  return hashtable->items[pos].name == NULL;
}

static HTS_INLINE int inthash_matches_(const inthash_item *const item,
                                       const char *name,
                                       const inthash_keys *hashes) {
  return item->name != NULL
    && item->hashes.hash1 == hashes->hash1
    && item->hashes.hash2 == hashes->hash2
    && strcmp(item->name, name) == 0;
}

static HTS_INLINE int inthash_matches(const inthash hashtable, size_t pos,
                                      const char *name,
                                      const inthash_keys *hashes) {
  const inthash_item *const item = &hashtable->items[pos];
  return inthash_matches_(item, name, hashes);
}

/* realloc (expand) string pool ; does not change the compacity */
static void inthash_realloc_pool(inthash hashtable) {
  const size_t hash_size = POW2(hashtable->hash_size_power);
  char *const oldbase = hashtable->string_pool;
  size_t i;
  int count = 0;

  hashtable->string_pool = realloc(hashtable->string_pool,
    hashtable->string_pool_capacity);
  if (hashtable->string_pool == NULL) {
    fprintf(stderr, 
      "** hashtable string pool allocation error: could not allocate %u bytes", 
      (int) hashtable->string_pool_capacity);
    inthash_assert(! "hashtable string pool allocation error");
  }

  /* recompute string address */
#define RECOMPUTE_STRING(S) do {                                   \
    if (S != NULL) {                                               \
      const size_t offset = (S) - oldbase;                         \
      hashtable->items[i].name = &hashtable->string_pool[offset];  \
      count++;                                                     \
    }                                                              \
} while(0)

  /* recompute */
  for(i = 0 ; i < hash_size ; i++) {
    RECOMPUTE_STRING(hashtable->items[i].name);
  }
  for(i = 0 ; i < hashtable->stash_size ; i++) {
    RECOMPUTE_STRING(hashtable->stash[i].name);
  }

#undef RECOMPUTE_STRING

  inthash_log(hashtable, "reallocated string pool for %d strings: %d bytes",
              count, (int) hashtable->string_pool_capacity);
}

/* compact string pool ; does not change the capacity */
static void inthash_compact_pool(inthash hashtable) {
  const size_t hash_size = POW2(hashtable->hash_size_power);
  size_t i;
  char* old_pool = hashtable->string_pool;
  const size_t old_size = hashtable->string_pool_size;
  int count = 0;

  hashtable->string_pool = malloc(hashtable->string_pool_capacity);
  hashtable->string_pool_size = 0;
  hashtable->string_pool_chars = 0;
  if (hashtable->string_pool == NULL) {
    fprintf(stderr, 
      "** hashtable string pool compaction error: could not allocate %u bytes", 
      (int) hashtable->string_pool_capacity);
    inthash_assert(! "hashtable string pool compaction error");
  }

  /* relocate a string on a different pool */
#define RELOCATE_STRING(S) do {                                    \
    if (S != NULL) {                                               \
      const size_t len = strlen(S) + 1;                            \
      inthash_assert(hashtable->string_pool_size + len             \
        <= hashtable->string_pool_capacity);                       \
      memcpy(&hashtable->string_pool[hashtable->string_pool_size], \
        S, len);                                                   \
      S = &hashtable->string_pool[hashtable->string_pool_size];    \
      hashtable->string_pool_size += len;                          \
      count++;                                                     \
    }                                                              \
} while(0)

  /* relocate */
  for(i = 0 ; i < hash_size ; i++) {
    RELOCATE_STRING(hashtable->items[i].name);
  }
  for(i = 0 ; i < hashtable->stash_size ; i++) {
    RELOCATE_STRING(hashtable->stash[i].name);
  }

#undef RELOCATE_STRING

  /* compacted (used chars == current size) */
  hashtable->string_pool_chars = hashtable->string_pool_size;

  /* wipe previous pool */
  free(old_pool);

  inthash_log(hashtable,
              "compacted string pool for %d strings: %d bytes => %d bytes",
              count, (int) old_size, (int) hashtable->string_pool_size);
}

static char* inthash_dup_name(inthash hashtable, const char *name) {
  const size_t len = strlen(name) + 1;
  char *s;

  if (hashtable->string_pool_capacity - hashtable->string_pool_size < len) {
    if (hashtable->string_pool_capacity == 0) {
      hashtable->string_pool_capacity = 16;
    }
    for( ; hashtable->string_pool_capacity - hashtable->string_pool_size < len
      ; hashtable->string_pool_capacity <<= 1) ;
    inthash_realloc_pool(hashtable);
  }

  s = &hashtable->string_pool[hashtable->string_pool_size];
  memcpy(s, name, len);
  hashtable->string_pool_size += len;
  hashtable->string_pool_chars += len;

  return s;
}

/* note: pointer must have been kicked from the pool first */
static void inthash_free_name(inthash hashtable, char *name) {
  const size_t len = strlen(name) + 1;
  hashtable->string_pool_chars -= len;
  if (hashtable->string_pool_chars < hashtable->string_pool_size / 2) {
    inthash_compact_pool(hashtable);
  }
}

static HTS_INLINE size_t inthash_hash_to_pos_(size_t hash_size_power,
                                              inthash_key hash) {
  const inthash_key mask = POW2(hash_size_power) - 1;
  return hash & mask;
}

static HTS_INLINE size_t inthash_hash_to_pos(const inthash hashtable,
                                             inthash_key hash) {
  return inthash_hash_to_pos_(hashtable->hash_size_power, hash);
}

int inthash_read_pvoid(inthash hashtable, const char *name, void **pvalue) {
  inthash_value value = INTHASH_VALUE_NULL;
  int ret =
    inthash_read_value(hashtable, name, (pvalue != NULL) ? &value : NULL);
  if (pvalue != NULL)
    *pvalue = value.ptr;
  return ret;
}

int inthash_write_pvoid(inthash hashtable, const char *name, void *pvalue) {
  inthash_value value = INTHASH_VALUE_NULL;

  value.ptr = pvalue;
  return inthash_write_value(hashtable, name, value);
}

void inthash_add_pvoid(inthash hashtable, const char *name, void *pvalue) {
  inthash_value value = INTHASH_VALUE_NULL;

  value.ptr = pvalue;
  inthash_write_value(hashtable, name, value);
}

int inthash_write(inthash hashtable, const char *name, intptr_t intvalue) {
  inthash_value value = INTHASH_VALUE_NULL;

  value.intg = intvalue;
  return inthash_write_value(hashtable, name, value);
}

static void inthash_default_free_handler(void *value) {
  if (value)
    free(value);
}

static void inthash_del_value_(inthash hashtable, inthash_value *pvalue) {
  if (hashtable->flag_valueismalloc) {
    if (hashtable->free_handler)
      hashtable->free_handler(pvalue->ptr);
    else
      inthash_default_free_handler(pvalue->ptr);
  }
  pvalue->ptr = NULL;
}

static void inthash_del_value(inthash hashtable, size_t pos) {
  inthash_del_value_(hashtable, &hashtable->items[pos].value);
}

static void inthash_del_name(inthash hashtable, inthash_item *item) {
  const inthash_keys nullHash = INTHASH_KEYS_NULL;
  char *const name = item->name;
  item->name = NULL;  /* there must be no reference remaining */
  item->hashes = nullHash;
  /* free after detach (we may compact the pool) */
  inthash_free_name(hashtable, name);
}

static void inthash_del_item(inthash hashtable, inthash_item *pitem) {
  inthash_del_value_(hashtable, &pitem->value);
  inthash_del_name(hashtable, pitem);
}

static int inthash_write_value_(inthash hashtable, const char *name,
                                inthash_value value) {
  inthash_item item;
  size_t pos;
  const inthash_keys hashes = inthash_calc_hashes(name);
  inthash_key cuckoo_hash, initial_cuckoo_hash;
  size_t loops;

  /* replace at position 1 ? */
  pos = inthash_hash_to_pos(hashtable, hashes.hash1);
  if (inthash_matches(hashtable, pos, name, &hashes)) {
    inthash_del_value(hashtable, pos);
    hashtable->items[pos].value = value;
    return 0;  /* replaced */
  }

  /* replace at position 2 ? */
  pos = inthash_hash_to_pos(hashtable, hashes.hash2);
  if (inthash_matches(hashtable, pos, name, &hashes)) {
    inthash_del_value(hashtable, pos);
    hashtable->items[pos].value = value;
    return 0;  /* replaced */
  }

  /* replace in the stash ? */
  if (hashtable->stash_size != 0) {
    size_t i;
    for(i = 0 ; i < hashtable->stash_size ; i++) {
      if (inthash_matches_(&hashtable->stash[i], name, &hashes)) {
        inthash_del_value_(hashtable, &hashtable->stash[i].value);
        hashtable->stash[i].value = value;
        return 0;  /* replaced */
      }
    }
  }

  /* otherwise we need to create a new item */
  item.name = inthash_dup_name(hashtable, name);
  item.value = value;
  item.hashes = hashes;

  /* place at free position 1 ? */
  pos = inthash_hash_to_pos(hashtable, hashes.hash1);
  if (inthash_is_free(hashtable, pos)) {
    hashtable->items[pos] = item;
    return 1; /* added */
  } else {
    /* place at free position 2 ? */
    pos = inthash_hash_to_pos(hashtable, hashes.hash2);
    if (inthash_is_free(hashtable, pos)) {
      hashtable->items[pos] = item;
      return 1; /* added */
    }
    /* prepare cuckoo ; let's take position 1 */
    else {
      cuckoo_hash = initial_cuckoo_hash = hashes.hash1;
      inthash_log(hashtable, "debug:collision with '%s' at %d (%x)", 
        item.name, pos, cuckoo_hash);
    }
  }

  /* put 'item' in place with hash 'cuckoo_hash' */
  for(loops = POW2(hashtable->hash_size_power) ; loops != 0 ; --loops) {
    const size_t pos = inthash_hash_to_pos(hashtable, cuckoo_hash);

    inthash_log(hashtable, "\tdebug:placing cuckoo '%s' at %d (%x)", 
      item.name, pos, cuckoo_hash);

    /* place at alternate free position ? */
    if (inthash_is_free(hashtable, pos)) {
      inthash_log(hashtable, "debug:free position");
      hashtable->items[pos] = item;
      return 1; /* added */
    }
    /* then cuckoo's place it is */
    else {
      const inthash_item backup_item = hashtable->items[pos];
      hashtable->items[pos] = item;

      /* take care of new lost item */
      item = backup_item;

      /* we just kicked this item from its position 1 */
      if (pos == inthash_hash_to_pos(hashtable, item.hashes.hash1)) {
        /* then place it on position 2 on next run */
        inthash_log(hashtable, "\tdebug:position 1");
        cuckoo_hash = item.hashes.hash2;
      }
      /* we just kicked this item from its position 2 */
      else if (pos == inthash_hash_to_pos(hashtable, item.hashes.hash2)) {
        /* then place it on position 1 on next run */
        inthash_log(hashtable, "\tdebug:position 2");
        cuckoo_hash = item.hashes.hash1;
      }
      else {
        inthash_assert(! "hashtable internal error: unexpected position");
      }

      /* we are looping (back to same hash) */
      /* TODO FIXME: we should actually check the positions */
      if (cuckoo_hash == initial_cuckoo_hash && 0) {
        /* emergency stash */
        break;
      }
    }
  }

  /* emergency stashing */
  if (hashtable->stash_size < STASH_SIZE) {
    hashtable->stash[hashtable->stash_size] = item;
    hashtable->stash_size++;
    inthash_log(hashtable, "used stash because of collision (%d entries)",
                (int) hashtable->stash_size);
    inthash_log(hashtable, "debug:stash position");
    return 1; /* added */
  } else {
    /* we are doomed. hopefully the probability is lower than being killed
       by a wandering radioactive monkey */
    inthash_assert(! "hashtable internal error: cuckoo/stash collision");
    /* not reachable code */
    return -1;
  }
}

int inthash_write_value(inthash hashtable, const char *name,
                        inthash_value value) {
  /* replace of add item */
  const int ret = inthash_write_value_(hashtable, name, value);

  /* added ? */
  if (ret) {
    /* size of half of the table */
    const size_t half_size = POW2(hashtable->hash_size_power - 1);

    /* item was added: increase count */
    hashtable->nitems++;

    /* table is more than half-full */
    if (hashtable->nitems >= half_size) {
      size_t i;
      char *data;

      /* size before  */
      const size_t prev_power = hashtable->hash_size_power;
      const size_t prev_size = half_size * 2;
      const size_t prev_alloc_size = prev_size*sizeof(inthash_item);

      /* size after doubling it */
      const size_t alloc_size = prev_alloc_size * 2;

      /* realloc */
      hashtable->hash_size_power++;
      hashtable->items = 
        (inthash_item *) realloc(hashtable->items, alloc_size);
      if (hashtable->items == NULL) {
        fprintf(stderr, 
          "** hashtable allocation error: could not allocate %u bytes", 
          (int) alloc_size);
        inthash_assert(! "hashtable allocation error");
      }

      /* clear upper half */
      data = (char*) hashtable->items;
      memset(data + prev_alloc_size, 0, prev_alloc_size);

      /* relocate lower half items when needed */
      for(i = 0 ; i < prev_size ; i++) {
        if (!inthash_is_free(hashtable, i)) {
          const inthash_keys *const hashes = &hashtable->items[i].hashes;

          /* currently at old position 1 */
          if (inthash_hash_to_pos_(prev_power, hashes->hash1) == i) {
            const size_t pos = inthash_hash_to_pos(hashtable, hashes->hash1);
            /* no more the expected position */
            if (pos != i) {
              inthash_assert(pos >= prev_size);
              hashtable->items[pos] = hashtable->items[i];
              memset(&hashtable->items[i], 0, sizeof(hashtable->items[i]));
            }
          }
          else if (inthash_hash_to_pos_(prev_power, hashes->hash2) == i) {
            const size_t pos = inthash_hash_to_pos(hashtable, hashes->hash2);
            /* no more the expected position */
            if (pos != i) {
              inthash_assert(pos >= prev_size);
              hashtable->items[pos] = hashtable->items[i];
              memset(&hashtable->items[i], 0, sizeof(hashtable->items[i]));
            }
          }
          else {
            inthash_assert(! "hashtable unexpected internal error (bad position)");
          }
        }
      }

      inthash_log(hashtable, "expanded hashtable to %d elements",
                  (int) POW2(hashtable->hash_size_power));

      /* attempt to merge the stash if present */
      if (hashtable->stash_size != 0) {
        const size_t old_size = hashtable->stash_size;
        size_t i;

        /* backup stash and reset it */
        inthash_item stash[STASH_SIZE];
        memcpy(&stash, hashtable->stash, sizeof(hashtable->stash));
        hashtable->stash_size = 0;

        /* insert all items */
        for(i = 0 ; i < old_size ; i++) {
          const int ret = inthash_write_value_(hashtable, stash[i].name,
                                               stash[i].value);
          if (ret == 0) {
            inthash_assert(! "hashtable duplicate key when merging the stash");
          }
        }

        /* delete old names (not values) */
        for(i = 0 ; i < hashtable->stash_size ; i++) {
          inthash_del_name(hashtable, &stash[i]);
        }

        /* logging */
        inthash_assert(hashtable->stash_size <= old_size);
        if (hashtable->stash_size < old_size) {
          inthash_log(hashtable, "reduced stash size from %d to %d",
                      (int) old_size, (int) hashtable->stash_size);
        } else {
          inthash_log(hashtable, "stash has still %d elements",
                      (int) hashtable->stash_size);
        }
      }

    }
  }

  return ret;
}

void inthash_add(inthash hashtable, const char *name, intptr_t intvalue) {
  inthash_value value = INTHASH_VALUE_NULL;

  memset(&value, 0, sizeof(value));
  value.intg = intvalue;
  inthash_write_value(hashtable, name, value);
}

int inthash_read(inthash hashtable, const char *name, intptr_t * intvalue) {
  inthash_value value = INTHASH_VALUE_NULL;
  int ret =
    inthash_read_value(hashtable, name, (intvalue != NULL) ? &value : NULL);
  if (intvalue != NULL)
    *intvalue = value.intg;
  return ret;
}

static inthash_value* inthash_read_value_(inthash hashtable,
                                          const char *name) {
  const inthash_keys hashes = inthash_calc_hashes(name);
  size_t pos;

  /* found at position 1 ? */
  pos = inthash_hash_to_pos(hashtable, hashes.hash1);
  if (inthash_matches(hashtable, pos, name, &hashes)) {
    return &hashtable->items[pos].value;
  }

  /* found at position 2 ? */
  pos = inthash_hash_to_pos(hashtable, hashes.hash2);
  if (inthash_matches(hashtable, pos, name, &hashes)) {
    return &hashtable->items[pos].value;
  }

  /* find in stash ? */
  if (hashtable->stash_size != 0) {
    size_t i;
    for(i = 0 ; i < hashtable->stash_size ; i++) {
      if (inthash_matches_(&hashtable->stash[i], name, &hashes)) {
        return &hashtable->stash[i].value;
      }
    }
  }

  /* not found */
  return NULL;
}

int inthash_read_value(inthash hashtable, const char *name,
                       inthash_value * pvalue) {
  inthash_value* const value = inthash_read_value_(hashtable, name);
  if (value != NULL) {
    if (pvalue != NULL) {
      *pvalue = *value;
    }
    return 1;
  }
  return 0;
}

static size_t inthash_inc_(inthash hashtable, const char *name,
                           size_t inc) {
  inthash_value* const value = inthash_read_value_(hashtable, name);
  if (value != NULL) {
    value->intg += inc;
    return value->intg;
  } else {
    /* create a new value */
    const int ret = inthash_write(hashtable, name, inc);
    inthash_assert(ret);
    return inc;
  }
}

int inthash_inc(inthash hashtable, const char *name) {
  return (int) inthash_inc_(hashtable, name, 1);
}

int inthash_dec(inthash hashtable, const char *name) {
  return (int) inthash_inc_(hashtable, name, (size_t) -1);
}

int inthash_exists(inthash hashtable, const char *name) {
  return inthash_read_value(hashtable, name, NULL);
}

static int inthash_remove_(inthash hashtable, const char *name,
                           const inthash_keys *hashes, size_t *removed) {
  size_t pos;

  /* found at position 1 ? */
  pos = inthash_hash_to_pos(hashtable, hashes->hash1);
  if (inthash_matches(hashtable, pos, name, hashes)) {
    inthash_del_item(hashtable, &hashtable->items[pos]);
    *removed = pos;
    return 1;
  }

  /* found at position 2 ? */
  pos = inthash_hash_to_pos(hashtable, hashes->hash2);
  if (inthash_matches(hashtable, pos, name, hashes)) {
    inthash_del_item(hashtable, &hashtable->items[pos]);
    *removed = pos;
    return 1;
  }

  /* find in stash ? */
  if (hashtable->stash_size != 0) {
    size_t i;
    for(i = 0 ; i < hashtable->stash_size ; i++) {
      if (inthash_matches_(&hashtable->stash[i], name, hashes)) {
        inthash_del_item(hashtable, &hashtable->stash[i]);
        for( ; i + 1 < hashtable->stash_size ; i++) {
          hashtable->stash[i] = hashtable->stash[i + 1];
        }
        hashtable->stash_size--;
        *removed = (size_t) -1;
        inthash_log(hashtable, "debug:deleted item in stash (%d entries)",
          (int) hashtable->stash_size);
        return 1;
      }
    }
  }

  /* not found */
  return 0;
}

int inthash_remove(inthash hashtable, const char *name) {
  const inthash_keys hashes = inthash_calc_hashes(name);
  size_t removed;
  const int ret = inthash_remove_(hashtable, name, &hashes, &removed);

  if (ret) {
    /* item was removed: decrease count */
    inthash_assert(hashtable->nitems != 0);
    hashtable->nitems--;

    /* can we place stash entry back to the table ? */
    if (hashtable->stash_size != 0 && removed != (size_t) -1) {
      size_t i;
      for(i = 0 ; i < hashtable->stash_size ; i++) {
        const size_t pos1 =
          inthash_hash_to_pos(hashtable, hashtable->stash[i].hashes.hash1);
        const size_t pos2 =
          inthash_hash_to_pos(hashtable, hashtable->stash[i].hashes.hash2);
        if (pos1 == removed || pos2 == removed) {
          if (pos1 == removed) {
            hashtable->items[pos1] = hashtable->stash[i];
          } else if (pos2 == removed) {
            hashtable->items[pos2] = hashtable->stash[i];
          }
          for( ; i + 1 < hashtable->stash_size ; i++) {
            hashtable->stash[i] = hashtable->stash[i + 1];
          }
          hashtable->stash_size--;
          inthash_log(hashtable, "debug:moved item from stash (%d entries)",
            (int) hashtable->stash_size);
          break;
        }
      }
    }
  }

  return ret;
}

int inthash_readptr(inthash hashtable, const char *name, intptr_t * value) {
  int ret;

  *value = 0;
  ret = inthash_read(hashtable, name, value);
  if (*value == 0)
    ret = 0;
  return ret;
}

inthash inthash_new(size_t initial_size) {
  inthash hashtable = (inthash) calloc(1, sizeof(struct_inthash));

  if (hashtable) {
    size_t size;
    for(size = 8 ; POW2(size) < initial_size ; size++) ;
    hashtable->flag_valueismalloc = 0;
    if ((hashtable->items =
         (inthash_item *) calloc(POW2(size), sizeof(inthash_item)))) {
      hashtable->hash_size_power = size;
    }
    hashtable->nitems = 0;
    hashtable->stash_size = 0;
    hashtable->string_pool = NULL;
    hashtable->string_pool_size = 0;
    hashtable->string_pool_capacity = 0;
    hashtable->string_pool_chars = 0;
  }
  return hashtable;
}

int inthash_created(inthash hashtable) {
  return hashtable != NULL && hashtable->items != NULL;
}

void inthash_value_is_malloc(inthash hashtable, int flag) {
  hashtable->flag_valueismalloc = flag;
}

void inthash_value_set_free_handler(inthash hashtable,
                                    t_inthash_freehandler free_handler) {
  hashtable->free_handler = free_handler;
}

size_t inthash_nitems(inthash hashtable) {
  if (hashtable != NULL)
    return hashtable->nitems;
  return 0;
}

void inthash_delete(inthash *phashtable) {
  if (phashtable != NULL) {
    inthash hashtable = *phashtable;
    if (hashtable != NULL) {
      if (hashtable->items != NULL) {
        /* we need to delete values */
        const size_t hash_size = POW2(hashtable->hash_size_power);
        size_t i;

        /* wipe hashtable values (not names) */
        for(i = 0 ; i < hash_size ; i++) {
          if (!inthash_is_free(hashtable, i)) {
            inthash_del_value(hashtable, i);
          }
        }

        /* wipe auxiliary stash values (not names) if any */
        for(i = 0 ; i < hashtable->stash_size ; i++) {
          inthash_del_value_(hashtable, &hashtable->stash[i].value);
        }
      }
      /* wipe top-level */
      hashtable->hash_size_power = 0;
      hashtable->nitems = 0;
      free(hashtable->string_pool);
      hashtable->string_pool = NULL;
      free(hashtable->items);
      hashtable->items = NULL;
      free(hashtable);
      *phashtable = NULL;
    }
  }
}

/* Enumerator */

struct_inthash_enum inthash_enum_new(inthash hashtable) {
  struct_inthash_enum e;

  e.index = 0;
  e.table = hashtable;
  return e;
}

inthash_item *inthash_enum_next(struct_inthash_enum * e) {
  const size_t hash_size = POW2(e->table->hash_size_power);
  for( ; e->index < hash_size 
    && inthash_is_free(e->table, e->index) ; e->index++) ;
  /* enumerate all table */
  if (e->index < hash_size) {
    inthash_item *const next = &e->table->items[e->index];
    e->index++;
    return next;
  }
  /* enumerate stash if present */
  else if (e->index < hash_size + e->table->stash_size) {
    const size_t index = e->index - hash_size;
    inthash_item *const next = &e->table->stash[index];
    e->index++;
    return next;
  }
  /* eof */
  else {
    return NULL;
  }
}
