/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998-2014 Xavier Roche and other contributors

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
/* File: hash tables                                            */
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

/* We use md5 as hashing function for its quality regarding diffusion and
   collisions. MD5 is slower than other hashing functions, but is known to be
   an excellent hashing function. FNV-1 is generally good enough for this
   purpose, too, but the performance gain is not sufficient to use it by
   default.

   On several benchmarks, both MD5 and FNV were quite good (0.45 cuckoo moved
   on average for each new item inserted in the hashtable), but FNV-1 was more
   prone to mutual collisions (creating cycles requiring stash handling), and
   was causing the stash area to be more filled than the MD5 variant.

   Simpler hashing functions, such as rolling hashes (LCG) were also tested,
   but with collision rate and diffusion were terrible.

   [ On a 10M key tests, both variants acheived 0.45 cuckoo/add ration,
     but the FNV-1 variant collided 11 times with a maximum stash area
     filled with 4 entries ; whereas the MD5 variant did only collide
     once ]
*/
#if (!defined(HTS_INTHASH_USES_MD5) && !defined(HTS_INTHASH_USES_MURMUR))
#define HTS_INTHASH_USES_MD5 1
#endif

#if HTS_INTHASH_USES_MD5 == 1
#include "md5.h"
#elif (defined(HTS_INTHASH_USES_MURMUR))
#include "murmurhash3.h"
#else
/* use the Openssl implementation */
#if 0
#include <openssl/md5.h>
#define MD5Init MD5_Init
#define MD5Update MD5_Update
#define MD5Final MD5_Final
#endif
#endif

/** Size of auxiliary stash. **/
#define STASH_SIZE 16

/** Minimum value for lg_size. **/
#define MIN_LG_SIZE 8

/** Minimum value for pool.capacity. **/
#define MIN_POOL_CAPACITY 256

/* 64-bit constant */
#if defined(WIN32)
#define UINT_64_CONST(X) ((uint64_t) (X))
#define UINT_64_FORMAT "I64d"
#elif (defined(_LP64) || defined(__x86_64__) \
       || defined(__powerpc64__) || defined(__64BIT__))
#define UINT_64_CONST(X) ((uint64_t) X##UL)
#define UINT_64_FORMAT "ld"
#else
#define UINT_64_CONST(X) ((uint64_t) X##ULL)
#define UINT_64_FORMAT "lld"
#endif

/** Hashtable. **/
struct struct_inthash {
  /** Hashtable items. **/
  inthash_item *items;

  /** Log-2 of the hashtable size. **/
  size_t lg_size;

  /** Number of used items (<= POW2(lg_size)). **/
  size_t used;

  /** Stash area for collisions. **/
  struct {
    /** Stash items. **/
    inthash_item items[STASH_SIZE];

    /** Stash size (<= STASH_SIZE). **/
    size_t size;
  } stash;

  /** String pool. **/
  struct {
    /** String buffer. **/
    char *buffer;
    /** Buffer used size (high watermark). **/
    size_t size;
    /** Buffer capacity. **/
    size_t capacity;
    /** Used chars (== size if compacted). **/
    size_t used;
  } pool;

  /** Statistics **/
  struct {
    /** Highest stash.size seen. **/
    size_t max_stash_size;
    /** Number of writes. **/
    size_t write_count;
    /** Number of writes causing an add. **/
    size_t add_count;
    /** Number of cuckoo moved during adds. **/
    size_t cuckoo_moved;
    /** Number of items added to stash. **/
    size_t stash_added;
    /** Number of hashtable rehash/expand operations. **/
    size_t rehash_count;
    /** Number of pool compact operations. **/
    size_t pool_compact_count;
    /** Number of pool realloc operations. **/
    size_t pool_realloc_count;
  } stats;

  /** Settings. **/
  struct {
    /** How to handle values (might be NULL). **/
    struct {
      /** free() **/
      t_inthash_freehandler free;
      /** opaque argument **/
      void *arg;
    } value;

    /** How to handle names (might be NULL). **/
    struct {
      /** strdup() **/
      t_inthash_duphandler dup;
      /** free() **/
      t_inthash_freehandler free;
      /** hash **/
      t_inthash_hasheshandler hash;
      /** comparison **/
      t_inthash_cmphandler equals;
      /** opaque argument **/
      void *arg;
    } key;

    /** How to handle fatal assertions (might be NULL). **/
    struct {
      /** abort() **/
      t_inthash_asserthandler fatal;
      /** opaque argument **/
      void *arg;
    } error;
  } custom;
};

/* Using httrack code */
static void inthash_fail(const char* exp, const char* file, int line) {
  fprintf(stderr, "assertion '%s' failed at %s:%d\n", exp, file, line);
  abort();
}
#define inthash_assert(HASHTABLE, EXP) \
  (void)( (EXP) || (inthash_assert_failed(HASHTABLE, #EXP, __FILE__, __LINE__), 0) )

/* Compiler-specific. */
#ifndef HTS_PRINTF_FUN
#ifdef __GNUC__
#define HTS_PRINTF_FUN(fmt, arg) __attribute__ ((format (printf, fmt, arg)))
#else
#define HTS_PRINTF_FUN(FMT, ARGS)
#endif
#endif
#ifndef HTS_INLINE
#ifdef __GNUC__
#define HTS_INLINE __inline__
#else
#define HTS_INLINE
#endif
#endif

/* Logging level. */
#if 0
/* Verbose. */
#define inthash_crit inthash_log
#define inthash_info inthash_log
#define inthash_debug inthash_log
#define inthash_trace inthash_log
#elif 0
/* Info. */
#define inthash_crit inthash_log
#define inthash_info inthash_log
#define inthash_debug inthash_log
#define inthash_trace inthash_nolog
#else
/* No logging except stats and critical. */
#define inthash_crit inthash_log
#define inthash_info inthash_log
#define inthash_debug inthash_nolog
#define inthash_trace inthash_nolog
#endif

/* 2**X */
#define POW2(X) ( (size_t) 1 << (X) )

/* the empty string for the string pool */
static char the_empty_string[1] = { 0 };

/* global assertion handler */
static t_inthash_asserthandler global_assert_handler = NULL;

/* Assert failed handler. */
static void inthash_assert_failed(const inthash hashtable, const char* exp, const char* file, int line) {
  if (hashtable != NULL && hashtable->custom.error.fatal != NULL) {
    hashtable->custom.error.fatal(hashtable->custom.error.arg, exp, file, line);
  } else if (global_assert_handler != NULL) {
    global_assert_handler(hashtable, exp, file, line);
  } else {
    inthash_fail(exp, file, line);
  }
  abort();
}

/* Logging */
static void inthash_log(const inthash hashtable, const char *format, ...)
                        HTS_PRINTF_FUN(2, 3);
static void inthash_log(const inthash hashtable, const char *format, ...) {
  va_list args;
  inthash_assert(hashtable, format != NULL);
  fprintf(stderr, "[%p] ", (void*) hashtable);
  va_start(args, format);
  (void) vfprintf(stderr, format, args);
  va_end(args);
  putc('\n', stderr);
}

/* No logging (should be dropped by the compiler) */
static void inthash_nolog(const inthash hashtable, const char *format, ...)
                          HTS_PRINTF_FUN(2, 3);
static void inthash_nolog(const inthash hashtable, const char *format, ...) {
}

static void inthash_log_stats(inthash hashtable) {
  inthash_info(hashtable, "hashtable summary: "
               "size=%"UINT_64_FORMAT" (lg2=%"UINT_64_FORMAT") "
               "used=%"UINT_64_FORMAT" "
               "stash-size=%"UINT_64_FORMAT" "
               "pool-size=%"UINT_64_FORMAT" "
               "pool-capacity=%"UINT_64_FORMAT" "
               "pool-used=%"UINT_64_FORMAT" "
               "writes=%"UINT_64_FORMAT" "
               "(new=%"UINT_64_FORMAT") "
               "moved=%"UINT_64_FORMAT " "
               "stashed=%"UINT_64_FORMAT" "
               "max-stash-size=%"UINT_64_FORMAT" "
               "avg-moved=%g "
               "rehash=%"UINT_64_FORMAT" "
               "pool-compact=%"UINT_64_FORMAT" "
               "pool-realloc=%"UINT_64_FORMAT" "
               "memory=%"UINT_64_FORMAT,
               (uint64_t) POW2(hashtable->lg_size),
               (uint64_t) hashtable->lg_size,
               (uint64_t) hashtable->used,
               (uint64_t) hashtable->stash.size,
               (uint64_t) hashtable->pool.size,
               (uint64_t) hashtable->pool.capacity,
               (uint64_t) hashtable->pool.used,
               (uint64_t) hashtable->stats.write_count,
               (uint64_t) hashtable->stats.add_count,
               (uint64_t) hashtable->stats.cuckoo_moved,
               (uint64_t) hashtable->stats.stash_added,
               (uint64_t) hashtable->stats.max_stash_size,
               (double) hashtable->stats.cuckoo_moved / (double) hashtable->stats.add_count,
               (uint64_t) hashtable->stats.rehash_count,
               (uint64_t) hashtable->stats.pool_compact_count,
               (uint64_t) hashtable->stats.pool_realloc_count,
               (uint64_t) inthash_memory_size(hashtable)
               );
}

inthash_keys inthash_hash_value(const char *value) {
#if HTS_INTHASH_USES_MD5 == 1
  /* compute a regular MD5 and extract two 32-bit integers */
  MD5_CTX ctx;
  union {
    unsigned char md5digest[16];
    inthash_keys mhashes[2];
    inthash_keys hashes;
  } u;

  /* compute MD5 */
  MD5Init(&ctx, 0);
  MD5Update(&ctx, (const unsigned char *) value,
    (unsigned int) strlen(value));
  MD5Final(u.md5digest, &ctx);

  /* mix mix mix */
  u.mhashes[0].hash1 ^= u.mhashes[1].hash1;
  u.mhashes[0].hash2 ^= u.mhashes[1].hash2;

  /* do not keep identical hashes */
  if (u.hashes.hash1 == u.hashes.hash2) {
    u.hashes.hash2 = ~u.hashes.hash2;
  }

  return u.hashes;
#elif (defined(HTS_INTHASH_USES_MURMUR))
  union {
    uint32_t result[4];
    inthash_keys hashes;
  } u;
  MurmurHash3_x86_128(value, (const int) strlen(value),
                      42, &u.result) ;

  /* mix mix mix */
  u.result[0] ^= u.result[2];
  u.result[1] ^= u.result[3];

  /* do not keep identical hashes */
  if (u.hashes.hash1 == u.hashes.hash2) {
    u.hashes.hash2 = ~u.hashes.hash2;
  }

  return u.hashes;
#else
  /* compute two Fowler-Noll-Vo hashes (64-bit FNV-1 variant) ;
     each 64-bit hash being XOR-folded into a single 32-bit hash. */
  size_t i;
  inthash_keys hashes;
  uint64_t h1, h2;

  /* FNV-1, 64-bit. */
#define FNV1_PRIME UINT_64_CONST(1099511628211)
#define FNV1_OFFSET_BASIS UINT_64_CONST(14695981039346656037)

  /* compute the hashes ; second variant is using xored data */
  h1 = FNV1_OFFSET_BASIS;
  h2 = ~FNV1_OFFSET_BASIS;
  for(i = 0 ; value[i] != '\0' ; i++) {
    const unsigned char c1 = value[i];
    const unsigned char c2 = ~c1;
    h1 = ( h1 * FNV1_PRIME ) ^ c1;
    h2 = ( h2 * FNV1_PRIME ) ^ c2;
  }

  /* XOR-folding to improve diffusion (Wikipedia) */
  hashes.hash1 = ( (uint32_t) h1 ^ (uint32_t) ( h1 >> 32 ) );
  hashes.hash2 = ( (uint32_t) h2 ^ (uint32_t) ( h2 >> 32 ) );

#undef FNV1_PRIME
#undef FNV1_OFFSET_BASIS

  /* do not keep identical hashes */
  if (hashes.hash1 == hashes.hash2) {
    hashes.hash2 = ~hashes.hash2;
  }

  return hashes;
#endif
}

static HTS_INLINE inthash_keys inthash_calc_hashes(inthash hashtable, 
                                                   const char *value) {
  return hashtable->custom.key.hash == NULL 
    ? inthash_hash_value(value)
    : hashtable->custom.key.hash(hashtable->custom.key.arg, value);
}

/* position 'pos' is free ? */
static HTS_INLINE int inthash_is_free(const inthash hashtable, size_t pos) {
  return hashtable->items[pos].name == NULL;
}

/* compare two keys ; by default using strcmp() */
static HTS_INLINE int inthash_equals(inthash hashtable,
                                     const char *a, const char *b) {
  return hashtable->custom.key.equals == NULL
    ? strcmp(a, b) == 0
    : hashtable->custom.key.equals(hashtable->custom.key.arg, a, b);
}

static HTS_INLINE int inthash_matches_(inthash hashtable,
                                       const inthash_item *const item,
                                       const char *name,
                                       const inthash_keys *hashes) {
  return item->name != NULL
    && item->hashes.hash1 == hashes->hash1
    && item->hashes.hash2 == hashes->hash2
    && inthash_equals(hashtable, item->name, name);
}

static HTS_INLINE int inthash_matches(inthash hashtable, size_t pos,
                                      const char *name,
                                      const inthash_keys *hashes) {
  const inthash_item *const item = &hashtable->items[pos];
  return inthash_matches_(hashtable, item, name, hashes);
}

/* compact string pool ; does not change the capacity */
static void inthash_compact_pool(inthash hashtable, size_t capacity) {
  const size_t hash_size = POW2(hashtable->lg_size);
  size_t i;
  char*const old_pool = hashtable->pool.buffer;
  const size_t old_size = hashtable->pool.size;
  size_t count = 0;

  /* we manage the string pool */
  inthash_assert(hashtable, hashtable->custom.key.dup == NULL);

  /* statistics */
  hashtable->stats.pool_compact_count++;

  /* change capacity now */
  if (hashtable->pool.capacity != capacity) {
    hashtable->pool.capacity = capacity;
  }

  /* realloc */
  hashtable->pool.buffer = malloc(hashtable->pool.capacity);
  hashtable->pool.size = 0;
  hashtable->pool.used = 0;
  if (hashtable->pool.buffer == NULL) {
    inthash_debug(hashtable,
      "** hashtable string pool compaction error: could not allocate "
      "%"UINT_64_FORMAT" bytes", 
      (uint64_t) hashtable->pool.capacity);
    inthash_assert(hashtable, ! "hashtable string pool compaction error");
  }

  /* relocate a string on a different pool */
#define RELOCATE_STRING(S) do {                             \
    if (S != NULL && S != the_empty_string) {               \
      const char *const src = (S);                          \
      char *const dest =                                    \
        &hashtable->pool.buffer[hashtable->pool.size];      \
      const size_t capacity = hashtable->pool.capacity;     \
      char *const max_dest =                                \
        &hashtable->pool.buffer[capacity];                  \
      /* copy string */                                     \
      inthash_assert(hashtable, dest < max_dest);           \
      dest[0] = src[0];                                     \
      {                                                     \
        size_t i;                                           \
        for(i = 1 ; src[i - 1] != '\0' ; i++) {             \
          inthash_assert(hashtable, &dest[i] < max_dest);   \
          dest[i] = src[i];                                 \
        }                                                   \
        /* update pool size */                              \
        hashtable->pool.size += i;                          \
        inthash_assert(hashtable,                           \
                       hashtable->pool.size <= capacity);   \
      }                                                     \
      /* update source */                                   \
      S = dest;                                             \
      count++;                                              \
    }                                                       \
} while(0)

  /* relocate */
  for(i = 0 ; i < hash_size ; i++) {
    RELOCATE_STRING(hashtable->items[i].name);
  }
  for(i = 0 ; i < hashtable->stash.size ; i++) {
    RELOCATE_STRING(hashtable->stash.items[i].name);
  }

#undef RELOCATE_STRING

  /* compacted (used chars == current size) */
  hashtable->pool.used = hashtable->pool.size;

  /* wipe previous pool */
  free(old_pool);

  inthash_debug(hashtable,
                "compacted string pool for %"UINT_64_FORMAT" strings: "
                "%"UINT_64_FORMAT" bytes => %"UINT_64_FORMAT" bytes",
                (uint64_t) count, (uint64_t) old_size,
                (uint64_t) hashtable->pool.size);
}

/* realloc (expand) string pool ; does not change the compacity */
static void inthash_realloc_pool(inthash hashtable, size_t capacity) {
  const size_t hash_size = POW2(hashtable->lg_size);
  char *const oldbase = hashtable->pool.buffer;
  size_t i;
  size_t count = 0;

  /* we manage the string pool */
  inthash_assert(hashtable, hashtable->custom.key.dup == NULL);

  /* compact instead ? */
  if (hashtable->pool.used < ( hashtable->pool.size*3 ) / 4) {
    inthash_compact_pool(hashtable, capacity);
    return ;
  }

  /* statistics */
  hashtable->stats.pool_realloc_count++;

  /* change capacity now */
  hashtable->pool.capacity = capacity;

  /* realloc */
  hashtable->pool.buffer = realloc(hashtable->pool.buffer,
    hashtable->pool.capacity);
  if (hashtable->pool.buffer == NULL) {
    inthash_crit(hashtable,
      "** hashtable string pool allocation error: could not allocate "
      "%"UINT_64_FORMAT" bytes", 
      (uint64_t) hashtable->pool.capacity);
    inthash_assert(hashtable, ! "hashtable string pool allocation error");
  }

  /* recompute string address */
#define RECOMPUTE_STRING(S) do {                                     \
    if (S != NULL && S != the_empty_string) {                        \
      const size_t offset = (S) - oldbase;                           \
      inthash_assert(hashtable, offset < hashtable->pool.capacity);  \
      S = &hashtable->pool.buffer[offset];                           \
      count++;                                                       \
    }                                                                \
} while(0)

  /* recompute */
  for(i = 0 ; i < hash_size ; i++) {
    RECOMPUTE_STRING(hashtable->items[i].name);
  }
  for(i = 0 ; i < hashtable->stash.size ; i++) {
    RECOMPUTE_STRING(hashtable->stash.items[i].name);
  }

#undef RECOMPUTE_STRING

  inthash_debug(hashtable, "reallocated string pool for "
                "%"UINT_64_FORMAT" strings: %"UINT_64_FORMAT" bytes",
                (uint64_t) count, (uint64_t) hashtable->pool.capacity);
}

static char* inthash_dup_name_internal(inthash hashtable, const char *name) {
  const size_t len = strlen(name) + 1;
  char *s;

  /* the pool does not allow empty strings for safety purpose ; handhe that
    (keys are being emptied when free'd to detect duplicate free) */
  if (len == 1) {
    inthash_assert(hashtable, the_empty_string[0] == '\0');
    return the_empty_string;
  }

  /* expand pool capacity */
  inthash_assert(hashtable, hashtable->pool.size <= hashtable->pool.capacity);
  if (hashtable->pool.capacity - hashtable->pool.size < len) {
    size_t capacity;
    for(capacity = MIN_POOL_CAPACITY ; capacity < hashtable->pool.size + len
      ; capacity <<= 1) ;
    inthash_assert(hashtable, hashtable->pool.size < capacity);
    inthash_realloc_pool(hashtable, capacity);
  }

  /* copy */
  inthash_assert(hashtable, len + hashtable->pool.size <= hashtable->pool.capacity);
  s = &hashtable->pool.buffer[hashtable->pool.size];
  memcpy(s, name, len);
  hashtable->pool.size += len;
  hashtable->pool.used += len;

  return s;
}

/* duplicate a key. default is to use the internal pool. */
static HTS_INLINE char* inthash_dup_name(inthash hashtable, const char *name) {
  return hashtable->custom.key.dup == NULL
    ? inthash_dup_name_internal(hashtable, name)
    : hashtable->custom.key.dup(hashtable->custom.key.arg, name);
}

/* internal pool free handler.
   note: pointer must have been kicked from the pool first */
static void inthash_free_key_internal(inthash hashtable, char *name) {
  const size_t len = strlen(name) + 1;

  /* see inthash_dup_name_internal() handling */
  if (len == 1 && name == the_empty_string) {
    inthash_assert(hashtable, the_empty_string[0] == '\0');
    return ;
  }

  inthash_assert(hashtable, *name != '\0' || !"duplicate or bad string pool release");
  hashtable->pool.used -= len;
  *name = '\0'; /* the string is now invalidated */

  /* compact the pool is too many holes  */
  if (hashtable->pool.used < hashtable->pool.size / 2) {
    size_t capacity = hashtable->pool.capacity;
    /* compact and shrink */
    if (hashtable->pool.used < capacity / 4) {
      capacity /= 2;
    }
    inthash_assert(hashtable, hashtable->pool.used < capacity);
    inthash_compact_pool(hashtable, capacity);
  }
}

/* free a key. default is to use the internal pool.
   note: pointer must have been kicked from the pool first */
static void inthash_free_key(inthash hashtable, char *name) {
  if (hashtable->custom.key.free == NULL) {
    inthash_free_key_internal(hashtable, name);
  } else {
    hashtable->custom.key.free(hashtable->custom.key.arg, name);
  }
}

static HTS_INLINE size_t inthash_hash_to_pos_(size_t lg_size,
                                              inthash_key hash) {
  const inthash_key mask = POW2(lg_size) - 1;
  return hash & mask;
}

static HTS_INLINE size_t inthash_hash_to_pos(const inthash hashtable,
                                             inthash_key hash) {
  return inthash_hash_to_pos_(hashtable->lg_size, hash);
}

int inthash_read_pvoid(inthash hashtable, const char *name, void **pvalue) {
  inthash_value value = INTHASH_VALUE_NULL;
  const int ret =
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

static void inthash_default_free_handler(void *arg, void *value) {
  if (value)
    free(value);
}

static void inthash_del_value_(inthash hashtable, inthash_value *pvalue) {
  if (hashtable->custom.value.free != NULL)
    hashtable->custom.value.free(hashtable->custom.value.arg, pvalue->ptr);
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
  inthash_free_key(hashtable, name);
}

static void inthash_del_item(inthash hashtable, inthash_item *pitem) {
  inthash_del_value_(hashtable, &pitem->value);
  inthash_del_name(hashtable, pitem);
}

static int inthash_add_item_(inthash hashtable, inthash_item item);

/* Write (add or replace) a value in the hashtable. */
static int inthash_write_value_(inthash hashtable, const char *name,
                                inthash_value value) {
  inthash_item item;
  size_t pos;
  const inthash_keys hashes = inthash_calc_hashes(hashtable, name);

  /* Statistics */
  hashtable->stats.write_count++;

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
  if (hashtable->stash.size != 0) {
    size_t i;
    for(i = 0 ; i < hashtable->stash.size ; i++) {
      if (inthash_matches_(hashtable, &hashtable->stash.items[i], name, 
                           &hashes)) {
        inthash_del_value_(hashtable, &hashtable->stash.items[i].value);
        hashtable->stash.items[i].value = value;
        return 0;  /* replaced */
      }
    }
  }

  /* Statistics */
  hashtable->stats.add_count++;

  /* otherwise we need to create a new item */
  item.name = inthash_dup_name(hashtable, name);
  item.value = value;
  item.hashes = hashes;

  return inthash_add_item_(hashtable, item);
}

/* Add a new item in the hashtable. The item SHALL NOT be alreasy present. */
static int inthash_add_item_(inthash hashtable, inthash_item item) {
  inthash_key cuckoo_hash, initial_cuckoo_hash;
  size_t loops;
  size_t pos;

  /* place at free position 1 ? */
  pos = inthash_hash_to_pos(hashtable, item.hashes.hash1);
  if (inthash_is_free(hashtable, pos)) {
    hashtable->items[pos] = item;
    return 1; /* added */
  } else {
    /* place at free position 2 ? */
    pos = inthash_hash_to_pos(hashtable, item.hashes.hash2);
    if (inthash_is_free(hashtable, pos)) {
      hashtable->items[pos] = item;
      return 1; /* added */
    }
    /* prepare cuckoo ; let's take position 1 */
    else {
      cuckoo_hash = initial_cuckoo_hash = item.hashes.hash1;
      inthash_trace(hashtable,
                    "debug:collision with '%s' at %"UINT_64_FORMAT" (%x)", 
                     item.name, (uint64_t) pos, cuckoo_hash);
    }
  }

  /* put 'item' in place with hash 'cuckoo_hash' */
  for(loops = POW2(hashtable->lg_size) ; loops != 0 ; --loops) {
    const size_t pos = inthash_hash_to_pos(hashtable, cuckoo_hash);

    inthash_trace(hashtable,
                  "\tdebug:placing cuckoo '%s' at %"UINT_64_FORMAT" (%x)", 
                  item.name, (uint64_t) pos, cuckoo_hash);

    /* place at alternate free position ? */
    if (inthash_is_free(hashtable, pos)) {
      inthash_trace(hashtable, "debug:free position");
      hashtable->items[pos] = item;
      return 1; /* added */
    }
    /* then cuckoo's place it is */
    else {
      /* replace */
      const inthash_item backup_item = hashtable->items[pos];
      hashtable->items[pos] = item;

      /* statistics */
      hashtable->stats.cuckoo_moved++;

      /* take care of new lost item */
      item = backup_item;

      /* we just kicked this item from its position 1 */
      if (pos == inthash_hash_to_pos(hashtable, item.hashes.hash1)) {
        /* then place it on position 2 on next run */
        inthash_trace(hashtable, "\tdebug:position 1");
        cuckoo_hash = item.hashes.hash2;
      }
      /* we just kicked this item from its position 2 */
      else if (pos == inthash_hash_to_pos(hashtable, item.hashes.hash2)) {
        /* then place it on position 1 on next run */
        inthash_trace(hashtable, "\tdebug:position 2");
        cuckoo_hash = item.hashes.hash1;
      }
      else {
        inthash_assert(hashtable, ! "hashtable internal error: unexpected position");
      }

      /* we are looping (back to same hash) */
      /* TODO FIXME: we should actually check the positions */
      if (cuckoo_hash == initial_cuckoo_hash) {
        /* emergency stash */
        break;
      }
    }
  }

  /* emergency stashing for the rare cases of collisions */
  if (hashtable->stash.size < STASH_SIZE) {
    hashtable->stash.items[hashtable->stash.size] = item;
    hashtable->stash.size++;
    /* for statistics */
    hashtable->stats.stash_added++;
    if (hashtable->stash.size > hashtable->stats.max_stash_size) {
      hashtable->stats.max_stash_size = hashtable->stash.size;
    }
    inthash_debug(hashtable, "used stash because of collision (%d entries)",
                  (int) hashtable->stash.size);
    return 1; /* added */
  } else {
    /* we are doomed. hopefully the probability is lower than being killed
       by a wandering radioactive monkey */
    inthash_log_stats(hashtable);
    inthash_assert(hashtable, ! "hashtable internal error: cuckoo/stash collision");
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
    const size_t half_size = POW2(hashtable->lg_size - 1);

    /* item was added: increase count */
    hashtable->used++;

    /* table is more than half-full */
    if (hashtable->used >= half_size) {
      size_t i;

      /* size before  */
      const size_t prev_power = hashtable->lg_size;
      const size_t prev_size = half_size * 2;
      const size_t prev_alloc_size = prev_size*sizeof(inthash_item);

      /* size after doubling it */
      const size_t alloc_size = prev_alloc_size * 2;

      /* statistics */
      hashtable->stats.rehash_count++;

      /* realloc */
      hashtable->lg_size++;
      hashtable->items = 
        (inthash_item *) realloc(hashtable->items, alloc_size);
      if (hashtable->items == NULL) {
        inthash_crit(hashtable,
          "** hashtable allocation error: "
          "could not allocate %"UINT_64_FORMAT" bytes", 
          (uint64_t) alloc_size);
        inthash_assert(hashtable, ! "hashtable allocation error");
      }

      /* clear upper half */
      memset(&hashtable->items[prev_size], 0, prev_alloc_size);

      /* relocate lower half items when needed */
      for(i = 0 ; i < prev_size ; i++) {
        if (!inthash_is_free(hashtable, i)) {
          const inthash_keys *const hashes = &hashtable->items[i].hashes;

          /* currently at old position 1 */
          if (inthash_hash_to_pos_(prev_power, hashes->hash1) == i) {
            const size_t pos = inthash_hash_to_pos(hashtable, hashes->hash1);
            /* no more the expected position */
            if (pos != i) {
              inthash_assert(hashtable, pos >= prev_size);
              hashtable->items[pos] = hashtable->items[i];
              memset(&hashtable->items[i], 0, sizeof(hashtable->items[i]));
            }
          }
          else if (inthash_hash_to_pos_(prev_power, hashes->hash2) == i) {
            const size_t pos = inthash_hash_to_pos(hashtable, hashes->hash2);
            /* no more the expected position */
            if (pos != i) {
              inthash_assert(hashtable, pos >= prev_size);
              hashtable->items[pos] = hashtable->items[i];
              memset(&hashtable->items[i], 0, sizeof(hashtable->items[i]));
            }
          }
          else {
            inthash_assert(hashtable, ! "hashtable unexpected internal error (bad position)");
          }
        }
      }

      inthash_debug(hashtable,
                    "expanded hashtable to %"UINT_64_FORMAT" elements",
                    (uint64_t) POW2(hashtable->lg_size));

      /* attempt to merge the stash if present */
      if (hashtable->stash.size != 0) {
        const size_t old_size = hashtable->stash.size;
        size_t i;

        /* backup stash and reset it */
        inthash_item stash[STASH_SIZE];
        memcpy(&stash, hashtable->stash.items, sizeof(hashtable->stash.items));
        hashtable->stash.size = 0;

        /* insert all items */
        for(i = 0 ; i < old_size ; i++) {
          const int ret = inthash_add_item_(hashtable, stash[i]);
          if (ret == 0) {
            inthash_assert(hashtable, ! "hashtable duplicate key when merging the stash");
          }
        }

        /* logging */
        inthash_assert(hashtable, hashtable->stash.size <= old_size);
        if (hashtable->stash.size < old_size) {
          inthash_debug(hashtable, "reduced stash size from %"UINT_64_FORMAT" "
                        "to %"UINT_64_FORMAT,
                        (uint64_t) old_size, (uint64_t) hashtable->stash.size);
        } else {
          inthash_trace(hashtable, "stash has still %"UINT_64_FORMAT" elements",
                        (uint64_t) hashtable->stash.size);
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
  const inthash_keys hashes = inthash_calc_hashes(hashtable, name);
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
  if (hashtable->stash.size != 0) {
    size_t i;
    for(i = 0 ; i < hashtable->stash.size ; i++) {
      if (inthash_matches_(hashtable, &hashtable->stash.items[i], name,
                           &hashes)) {
        return &hashtable->stash.items[i].value;
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
    value->uintg += inc;
    return value->uintg;
  } else {
    /* create a new value */
    const int ret = inthash_write(hashtable, name, inc);
    inthash_assert(hashtable, ret);
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
  if (hashtable->stash.size != 0) {
    size_t i;
    for(i = 0 ; i < hashtable->stash.size ; i++) {
      if (inthash_matches_(hashtable, &hashtable->stash.items[i], name,
                           hashes)) {
        inthash_del_item(hashtable, &hashtable->stash.items[i]);
        for( ; i + 1 < hashtable->stash.size ; i++) {
          hashtable->stash.items[i] = hashtable->stash.items[i + 1];
        }
        hashtable->stash.size--;
        *removed = (size_t) -1;
        inthash_debug(hashtable, "debug:deleted item in stash (%d entries)",
          (int) hashtable->stash.size);
        return 1;
      }
    }
  }

  /* not found */
  return 0;
}

int inthash_remove(inthash hashtable, const char *name) {
  const inthash_keys hashes = inthash_calc_hashes(hashtable, name);
  size_t removed;
  const int ret = inthash_remove_(hashtable, name, &hashes, &removed);

  if (ret) {
    /* item was removed: decrease count */
    inthash_assert(hashtable, hashtable->used != 0);
    hashtable->used--;

    /* can we place stash entry back to the table ? */
    if (hashtable->stash.size != 0 && removed != (size_t) -1) {
      size_t i;
      for(i = 0 ; i < hashtable->stash.size ; i++) {
        const size_t pos1 =
          inthash_hash_to_pos(hashtable, hashtable->stash.items[i].hashes.hash1);
        const size_t pos2 =
          inthash_hash_to_pos(hashtable, hashtable->stash.items[i].hashes.hash2);
        if (pos1 == removed || pos2 == removed) {
          if (pos1 == removed) {
            hashtable->items[pos1] = hashtable->stash.items[i];
          } else if (pos2 == removed) {
            hashtable->items[pos2] = hashtable->stash.items[i];
          }
          for( ; i + 1 < hashtable->stash.size ; i++) {
            hashtable->stash.items[i] = hashtable->stash.items[i + 1];
          }
          hashtable->stash.size--;
          inthash_debug(hashtable, "debug:moved item from stash (%d entries)",
            (int) hashtable->stash.size);
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
    for(size = MIN_LG_SIZE ; POW2(size) < initial_size ; size++) ;
    if ((hashtable->items =
         (inthash_item *) calloc(POW2(size), sizeof(inthash_item)))) {
      hashtable->lg_size = size;
    }
    hashtable->used = 0;
    hashtable->stash.size = 0;
    hashtable->pool.buffer = NULL;
    hashtable->pool.size = 0;
    hashtable->pool.capacity = 0;
    hashtable->pool.used = 0;
    hashtable->stats.max_stash_size = 0;
    hashtable->stats.write_count = 0;
    hashtable->stats.add_count = 0;
    hashtable->stats.cuckoo_moved = 0;
    hashtable->stats.stash_added= 0;
    hashtable->stats.pool_compact_count = 0;
    hashtable->stats.pool_realloc_count = 0;
    hashtable->stats.rehash_count = 0;
    hashtable->custom.value.free = NULL;
    hashtable->custom.value.arg = NULL;
    hashtable->custom.key.dup = NULL;
    hashtable->custom.key.free = NULL;
    hashtable->custom.key.hash = NULL;
    hashtable->custom.key.equals = NULL;
    hashtable->custom.key.arg = NULL;
  }
  return hashtable;
}

int inthash_created(inthash hashtable) {
  return hashtable != NULL && hashtable->items != NULL;
}

void inthash_value_is_malloc(inthash hashtable, int flag) {
  if (flag) {
    if (hashtable->custom.value.free == NULL) {
      hashtable->custom.value.free = inthash_default_free_handler;
      hashtable->custom.value.arg = NULL;
    }
  } else {
    hashtable->custom.value.free = NULL;
    hashtable->custom.value.arg = NULL;
  }
}

void inthash_value_set_value_handler(inthash hashtable,
                                     t_inthash_freehandler free,
                                     void *arg) {
  hashtable->custom.value.free = free;
  hashtable->custom.value.arg = arg;
}

void inthash_value_set_key_handler(inthash hashtable,
                                   t_inthash_duphandler dup,
                                   t_inthash_freehandler free,
                                   t_inthash_hasheshandler hash,
                                   t_inthash_cmphandler equals,
                                   void *arg) {
  /* dup and free must be consistent */
  inthash_assert(hashtable, ( dup == NULL ) == ( free == NULL ) );
  hashtable->custom.key.dup = dup;
  hashtable->custom.key.free = free;
  hashtable->custom.key.hash = hash;
  hashtable->custom.key.equals = equals;
  hashtable->custom.key.arg = arg;
}

void inthash_set_assert_handler(inthash hashtable,
                                t_inthash_asserthandler fatal,
                                void *arg) {
  inthash_assert(hashtable, fatal != NULL);
  hashtable->custom.error.fatal = fatal;
  hashtable->custom.error.arg = arg;
}

size_t inthash_nitems(inthash hashtable) {
  if (hashtable != NULL)
    return hashtable->used;
  return 0;
}

size_t inthash_memory_size(inthash hashtable) {
  const size_t size_struct = sizeof(struct_inthash);
  const size_t hash_size = POW2(hashtable->lg_size)*sizeof(inthash_item);
  const size_t pool_size = hashtable->pool.capacity*sizeof(char);
  return size_struct + hash_size + pool_size;
}

void inthash_delete(inthash *phashtable) {
  if (phashtable != NULL) {
    inthash hashtable = *phashtable;
    if (hashtable != NULL) {
      inthash_log_stats(hashtable);
      if (hashtable->items != NULL) {
        /* we need to delete values */
        const size_t hash_size = POW2(hashtable->lg_size);
        size_t i;

        /* wipe hashtable values (not names) */
        for(i = 0 ; i < hash_size ; i++) {
          if (!inthash_is_free(hashtable, i)) {
            inthash_del_value(hashtable, i);
          }
        }

        /* wipe auxiliary stash values (not names) if any */
        for(i = 0 ; i < hashtable->stash.size ; i++) {
          inthash_del_value_(hashtable, &hashtable->stash.items[i].value);
        }
      }
      /* wipe top-level */
      hashtable->lg_size = 0;
      hashtable->used = 0;
      free(hashtable->pool.buffer);
      hashtable->pool.buffer = NULL;
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
  const size_t hash_size = POW2(e->table->lg_size);
  for( ; e->index < hash_size 
    && inthash_is_free(e->table, e->index) ; e->index++) ;
  /* enumerate all table */
  if (e->index < hash_size) {
    inthash_item *const next = &e->table->items[e->index];
    e->index++;
    return next;
  }
  /* enumerate stash if present */
  else if (e->index < hash_size + e->table->stash.size) {
    const size_t index = e->index - hash_size;
    inthash_item *const next = &e->table->stash.items[index];
    e->index++;
    return next;
  }
  /* eof */
  else {
    return NULL;
  }
}

void inthash_set_global_assert_handler(t_inthash_asserthandler fatal) {
  global_assert_handler = fatal;
}
