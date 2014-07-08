/* ------------------------------------------------------------ */
/*
Coucal, Cuckoo hashing-based hashtable with stash area.
Copyright (C) 2013-2014 Xavier Roche (http://www.httrack.com/)
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

#include "coucal.h"

/* We use murmur hashing by default, even if md5 can be a good candidate,
   for its quality regarding diffusion and collisions.
   MD5 is slower than other hashing functions, but is known to be an excellent
   hashing function. FNV-1 is generally good enough for this purpose, too, but
   the performance gain is not sufficient to use it by default.

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
#if (!defined(HTS_INTHASH_USES_MD5) \
  && !defined(HTS_INTHASH_USES_OPENSSL_MD5) \
  && !defined(HTS_INTHASH_USES_MURMUR) \
  && !defined(HTS_INTHASH_USES_FNV1) \
  )
/* Temporry: fixing Invalid address alignment issues */
#if (defined(HAVE_ALIGNED_ACCESS_REQUIRED) \
  || defined(__sparc__) \
  || defined(mips) || defined(__mips__) || defined(MIPS) || defined(_MIPS_) \
  || defined(arm) || defined(__arm__) || defined(ARM) || defined(_ARM_) \
  )
#ifndef LIBHTTRACK_EXPORTS
#define HTS_INTHASH_USES_OPENSSL_MD5 1
#else
#define HTS_INTHASH_USES_MD5 1
#endif
#else
#define HTS_INTHASH_USES_MURMUR 1
#endif
#endif

/* Dispatch includes */
#if (defined(HTS_INTHASH_USES_MURMUR))
#include "murmurhash3.h"
#elif (defined(HTS_INTHASH_USES_MD5))
#include "md5.h"
#define HashMD5Init(CTX, FLAG) MD5Init(CTX, FLAG)
#define HashMD5Update(CTX, DATA, SIZE) MD5Update(CTX, DATA, SIZE)
#define HashMD5Final(DIGEST, CTX) MD5Final(DIGEST, CTX)
#define HashMD5Context MD5CTX
#elif (defined(HTS_INTHASH_USES_OPENSSL_MD5))
#include <openssl/md5.h>
#define HashMD5Init(CTX, FLAG) MD5_Init(CTX)
#define HashMD5Update(CTX, DATA, SIZE) MD5_Update(CTX, DATA, SIZE)
#define HashMD5Final(DIGEST, CTX) MD5_Final(DIGEST, CTX)
#define HashMD5Context MD5_CTX
#else
#error "No hash method defined"
#endif

/** Size of auxiliary stash. **/
#define STASH_SIZE 16

/** Minimum value for lg_size. **/
#define MIN_LG_SIZE 4

/** Minimum value for pool.capacity. **/
#define MIN_POOL_CAPACITY 256

/* 64-bit constant */
#if (defined(WIN32))
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
struct struct_coucal {
  /** Hashtable items. **/
  coucal_item *items;

  /** Log-2 of the hashtable size. **/
  size_t lg_size;

  /** Number of used items (<= POW2(lg_size)). **/
  size_t used;

  /** Stash area for collisions. **/
  struct {
    /** Stash items. **/
    coucal_item items[STASH_SIZE];

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
      t_coucal_value_freehandler free;
      /** opaque argument **/
      coucal_opaque arg;
    } value;

    /** How to handle names (might be NULL). **/
    struct {
      /** strdup() **/
      t_coucal_duphandler dup;
      /** free() **/
      t_coucal_key_freehandler free;
      /** hash **/
      t_coucal_hasheshandler hash;
      /** comparison **/
      t_coucal_cmphandler equals;
      /** opaque argument **/
      coucal_opaque arg;
    } key;

    /** How to handle fatal assertions (might be NULL). **/
    struct {
      /** logging **/
      t_coucal_loghandler log;
      /** abort() **/
      t_coucal_asserthandler fatal;
      /** opaque argument **/
      coucal_opaque arg;
      /** hashtable name for logging **/
      coucal_key_const name;
    } error;

    /** How to handle pretty-print (debug) (might be NULL). **/
    struct {
      /** key print() **/
      t_coucal_printkeyhandler key;
      /** value print() **/
      t_coucal_printvaluehandler value;
      /** opaque argument **/
      coucal_opaque arg;
    } print;
  } custom;
};

/* Assertion check. */
#define coucal_assert(HASHTABLE, EXP) \
  (void)( (EXP) || (coucal_assert_failed(HASHTABLE, #EXP, __FILE__, __LINE__), 0) )

/* Compiler-specific. */
#ifdef __GNUC__
#define INTHASH_PRINTF_FUN(fmt, arg) __attribute__ ((format (printf, fmt, arg)))
#define INTHASH_INLINE __inline__
#elif (defined(_MSC_VER))
#define INTHASH_PRINTF_FUN(FMT, ARGS)
#define INTHASH_INLINE __inline
#else
#define INTHASH_PRINTF_FUN(FMT, ARGS)
#define INTHASH_INLINE
#endif

/* Logging level. */
static void coucal_log(const coucal hashtable, coucal_loglevel level,
                        const char *format, va_list args);
#define DECLARE_LOG_FUNCTION(NAME, LEVEL) \
static void NAME(const coucal hashtable, const char *format, ...) \
  INTHASH_PRINTF_FUN(2, 3); \
static void NAME(const coucal hashtable, const char *format, ...) { \
  va_list args; \
  va_start(args, format); \
  coucal_log(hashtable, LEVEL, format, args); \
  va_end(args); \
}
#if 0
/* Verbose. */
DECLARE_LOG_FUNCTION(coucal_crit, coucal_log_critical)
DECLARE_LOG_FUNCTION(coucal_warning, coucal_log_warning)
DECLARE_LOG_FUNCTION(coucal_info, coucal_log_info)
DECLARE_LOG_FUNCTION(coucal_debug, coucal_log_debug)
DECLARE_LOG_FUNCTION(coucal_trace, coucal_log_trace)
#elif 0
/* Info. */
DECLARE_LOG_FUNCTION(coucal_crit, coucal_log_critical)
DECLARE_LOG_FUNCTION(coucal_warning, coucal_log_warning)
DECLARE_LOG_FUNCTION(coucal_info, coucal_log_info)
#define coucal_debug coucal_log
#define coucal_trace coucal_nolog
#else
/* No logging except stats and critical. */
DECLARE_LOG_FUNCTION(coucal_crit, coucal_log_critical)
DECLARE_LOG_FUNCTION(coucal_warning, coucal_log_warning)
DECLARE_LOG_FUNCTION(coucal_info, coucal_log_info)
#define coucal_debug coucal_nolog
#define coucal_trace coucal_nolog
#endif

/* 2**X */
#define POW2(X) ( (size_t) 1 << (X) )

/* the empty string for the string pool */
static char the_empty_string[1] = { 0 };

/* global assertion handler */
static t_coucal_asserthandler global_assert_handler = NULL;

/* global assertion handler */
static t_coucal_loghandler global_log_handler = NULL;

/* default assertion handler, if neither hashtable one nor global one 
   were defined */
static void coucal_fail(const char* exp, const char* file, int line) {
  fprintf(stderr, "assertion '%s' failed at %s:%d\n", exp, file, line);
  abort();
}

/* assert failed handler. */
static void coucal_assert_failed(const coucal hashtable, const char* exp, const char* file, int line) {
  const char *const name = coucal_get_name(hashtable);
  coucal_crit(hashtable, "hashtable %s: %s failed at %s:%d", 
    name != NULL ? name : "<unknown>", exp, file, line);
  if (hashtable != NULL && hashtable->custom.error.fatal != NULL) {
    hashtable->custom.error.fatal(hashtable->custom.error.arg, exp, file, line);
  } else if (global_assert_handler != NULL) {
    global_assert_handler(hashtable, exp, file, line);
  } else {
    coucal_fail(exp, file, line);
  }
  abort();
}

/* Logging */
static void coucal_log(const coucal hashtable, coucal_loglevel level,
                       const char *format, va_list args) {
  coucal_assert(hashtable, format != NULL);
  if (hashtable != NULL && hashtable->custom.error.log != NULL) {
    hashtable->custom.error.log(hashtable->custom.error.arg, level, format, args);
  } else if (global_log_handler != NULL) {
    global_log_handler(hashtable, level, format, args);
  } else {
    fprintf(stderr, "[%p] ", (void*) hashtable);
    (void) vfprintf(stderr, format, args);
    putc('\n', stderr);
  }
}

/* No logging (should be dropped by the compiler) */
static INTHASH_INLINE void coucal_nolog(const coucal hashtable, 
                                        const char *format, ...)
                                        INTHASH_PRINTF_FUN(2, 3);
static INTHASH_INLINE void coucal_nolog(const coucal hashtable, 
                                        const char *format, ...) {
  (void) hashtable;
  (void) format;
}

const char* coucal_get_name(coucal hashtable) {
  return hashtable->custom.error.name;
}

static void coucal_log_stats(coucal hashtable) {
  const char *const name = coucal_get_name(hashtable);
  coucal_info(hashtable, "hashtable %s%s%ssummary: "
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
               name != NULL ? "\"" : "",
               name != NULL ? name : "",
               name != NULL ? "\" " : "",
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
               (uint64_t) coucal_memory_size(hashtable)
               );
}

/* default hash function when key is a regular C-string */
coucal_hashkeys coucal_hash_data(const void *data_, size_t size) {
  const unsigned char *const data = (const unsigned char *) data_;
#if (defined(HTS_INTHASH_USES_MD5) || defined(HTS_INTHASH_USES_OPENSSL_MD5))
  /* compute a regular MD5 and extract two 32-bit integers */
  HashMD5Context ctx;
  union {
    unsigned char md5digest[16];
#if (COUCAL_HASH_SIZE == 32)
    coucal_hashkeys mhashes[2];
#endif
    coucal_hashkeys hashes;
  } u;

  /* compute MD5 */
  HashMD5Init(&ctx, 0);
  HashMD5Update(&ctx, data, (unsigned int) size);
  HashMD5Final(u.md5digest, &ctx);

#if (COUCAL_HASH_SIZE == 32)
  /* mix mix mix */
  u.mhashes[0].hash1 ^= u.mhashes[1].hash1;
  u.mhashes[0].hash2 ^= u.mhashes[1].hash2;
#endif

  /* do not keep identical hashes */
  if (u.hashes.hash1 == u.hashes.hash2) {
    u.hashes.hash2 = ~u.hashes.hash2;
  }

  return u.hashes;
#elif (defined(HTS_INTHASH_USES_MURMUR))
  union {
    uint32_t result[4];
    coucal_hashkeys hashes;
  } u;
  MurmurHash3_x86_128(data, (const int) size, 42, &u.result);

#if (COUCAL_HASH_SIZE == 32)
  /* mix mix mix */
  u.result[0] ^= u.result[2];
  u.result[1] ^= u.result[3];
#endif

  /* do not keep identical hashes */
  if (u.hashes.hash1 == u.hashes.hash2) {
    u.hashes.hash2 = ~u.hashes.hash2;
  }

  return u.hashes;
#elif (defined(HTS_INTHASH_USES_FNV1))
  /* compute two Fowler-Noll-Vo hashes (64-bit FNV-1 variant) ;
     each 64-bit hash being XOR-folded into a single 32-bit hash. */
  size_t i;
  coucal_hashkeys hashes;
  uint64_t h1, h2;

  /* FNV-1, 64-bit. */
#define FNV1_PRIME UINT_64_CONST(1099511628211)
#define FNV1_OFFSET_BASIS UINT_64_CONST(14695981039346656037)

  /* compute the hashes ; second variant is using xored data */
  h1 = FNV1_OFFSET_BASIS;
  h2 = ~FNV1_OFFSET_BASIS;
  for(i = 0 ; i < size ; i++) {
    const unsigned char c1 = data[i];
    const unsigned char c2 = ~c1;
    h1 = ( h1 * FNV1_PRIME ) ^ c1;
    h2 = ( h2 * FNV1_PRIME ) ^ c2;
  }

#if (COUCAL_HASH_SIZE == 32)
  /* XOR-folding to improve diffusion (Wikipedia) */
  hashes.hash1 = ( (uint32_t) h1 ^ (uint32_t) ( h1 >> 32 ) );
  hashes.hash2 = ( (uint32_t) h2 ^ (uint32_t) ( h2 >> 32 ) );
#elif (COUCAL_HASH_SIZE == 64)
  /* Direct hashes */
  hashes.hash1 = h1;
  hashes.hash2 = h2;
#else
#error "Unsupported COUCAL_HASH_SIZE"
#endif

#undef FNV1_PRIME
#undef FNV1_OFFSET_BASIS

  /* do not keep identical hashes */
  if (hashes.hash1 == hashes.hash2) {
    hashes.hash2 = ~hashes.hash2;
  }

  return hashes;

#else
#error "Undefined hashing method"
#endif
}

INTHASH_INLINE coucal_hashkeys coucal_hash_string(const char *name) {
  return coucal_hash_data(name, strlen(name));
}

INTHASH_INLINE coucal_hashkeys coucal_calc_hashes(coucal hashtable, 
                                                  coucal_key_const value) {
  return hashtable->custom.key.hash == NULL 
    ? coucal_hash_string(value)
    : hashtable->custom.key.hash(hashtable->custom.key.arg, value);
}

/* position 'pos' is free ? */
static INTHASH_INLINE int coucal_is_free(const coucal hashtable, size_t pos) {
  return hashtable->items[pos].name == NULL;
}

/* compare two keys ; by default using strcmp() */
static INTHASH_INLINE int coucal_equals(coucal hashtable,
                                        coucal_key_const a,
                                        coucal_key_const b) {
  return hashtable->custom.key.equals == NULL
    ? strcmp((const char*) a, (const char*) b) == 0
    : hashtable->custom.key.equals(hashtable->custom.key.arg, a, b);
}

static INTHASH_INLINE int coucal_matches_(coucal hashtable,
                                          const coucal_item *const item,
                                          coucal_key_const name,
                                          const coucal_hashkeys *hashes) {
  return item->name != NULL
    && item->hashes.hash1 == hashes->hash1
    && item->hashes.hash2 == hashes->hash2
    && coucal_equals(hashtable, item->name, name);
}

static INTHASH_INLINE int coucal_matches(coucal hashtable, size_t pos,
                                         coucal_key_const name,
                                         const coucal_hashkeys *hashes) {
  const coucal_item *const item = &hashtable->items[pos];
  return coucal_matches_(hashtable, item, name, hashes);
}

/* compact string pool ; does not necessarily change the capacity */
static void coucal_compact_pool(coucal hashtable, size_t capacity) {
  const size_t hash_size = POW2(hashtable->lg_size);
  size_t i;
  char*const old_pool = hashtable->pool.buffer;
  const size_t old_size = hashtable->pool.size;
  size_t count = 0;

  /* we manage the string pool */
  coucal_assert(hashtable, hashtable->custom.key.dup == NULL);

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
    coucal_debug(hashtable,
      "** hashtable string pool compaction error: could not allocate "
      "%"UINT_64_FORMAT" bytes", 
      (uint64_t) hashtable->pool.capacity);
    coucal_assert(hashtable, ! "hashtable string pool compaction error");
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
      coucal_assert(hashtable, dest < max_dest);           \
      dest[0] = src[0];                                     \
      {                                                     \
        size_t i;                                           \
        for(i = 1 ; src[i - 1] != '\0' ; i++) {             \
          coucal_assert(hashtable, &dest[i] < max_dest);   \
          dest[i] = src[i];                                 \
        }                                                   \
        /* update pool size */                              \
        hashtable->pool.size += i;                          \
        coucal_assert(hashtable,                           \
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

  coucal_debug(hashtable,
                "compacted string pool for %"UINT_64_FORMAT" strings: "
                "%"UINT_64_FORMAT" bytes => %"UINT_64_FORMAT" bytes",
                (uint64_t) count, (uint64_t) old_size,
                (uint64_t) hashtable->pool.size);
}

/* realloc (expand) string pool ; does not change the compacity */
static void coucal_realloc_pool(coucal hashtable, size_t capacity) {
  const size_t hash_size = POW2(hashtable->lg_size);
  char *const oldbase = hashtable->pool.buffer;
  size_t count = 0;

  /* we manage the string pool */
  coucal_assert(hashtable, hashtable->custom.key.dup == NULL);

  /* compact instead ? */
  if (hashtable->pool.used < ( hashtable->pool.size*3 ) / 4) {
    coucal_compact_pool(hashtable, capacity);
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
    coucal_crit(hashtable,
      "** hashtable string pool allocation error: could not allocate "
      "%"UINT_64_FORMAT" bytes", 
      (uint64_t) hashtable->pool.capacity);
    coucal_assert(hashtable, ! "hashtable string pool allocation error");
  }

  /* recompute string address */
#define RECOMPUTE_STRING(S) do {                                     \
    if (S != NULL && S != the_empty_string) {                        \
      const size_t offset = (const char*) (S) - oldbase;             \
      coucal_assert(hashtable, offset < hashtable->pool.capacity);  \
      S = &hashtable->pool.buffer[offset];                           \
      count++;                                                       \
    }                                                                \
} while(0)

  /* recompute string addresses */
  if (hashtable->pool.buffer != oldbase) {
    size_t i;
    for(i = 0 ; i < hash_size ; i++) {
      RECOMPUTE_STRING(hashtable->items[i].name);
    }
    for(i = 0 ; i < hashtable->stash.size ; i++) {
      RECOMPUTE_STRING(hashtable->stash.items[i].name);
    }
  }

#undef RECOMPUTE_STRING

  coucal_debug(hashtable, "reallocated string pool for "
                "%"UINT_64_FORMAT" strings: %"UINT_64_FORMAT" bytes",
                (uint64_t) count, (uint64_t) hashtable->pool.capacity);
}

static coucal_key coucal_dup_name_internal(coucal hashtable,
                                           coucal_key_const name_) {
  const char *const name = (const char*) name_;
  const size_t len = strlen(name) + 1;
  char *s;

  /* the pool does not allow empty strings for safety purpose ; handhe that
    (keys are being emptied when free'd to detect duplicate free) */
  if (len == 1) {
    coucal_assert(hashtable, the_empty_string[0] == '\0');
    return the_empty_string;
  }

  /* expand pool capacity */
  coucal_assert(hashtable, hashtable->pool.size <= hashtable->pool.capacity);
  if (hashtable->pool.capacity - hashtable->pool.size < len) {
    size_t capacity;
    for(capacity = MIN_POOL_CAPACITY ; capacity < hashtable->pool.size + len
      ; capacity <<= 1) ;
    coucal_assert(hashtable, hashtable->pool.size < capacity);
    coucal_realloc_pool(hashtable, capacity);
  }

  /* copy */
  coucal_assert(hashtable, len + hashtable->pool.size <= hashtable->pool.capacity);
  s = &hashtable->pool.buffer[hashtable->pool.size];
  memcpy(s, name, len);
  hashtable->pool.size += len;
  hashtable->pool.used += len;

  return s;
}

/* duplicate a key. default is to use the internal pool. */
static INTHASH_INLINE coucal_key coucal_dup_name(coucal hashtable,
                                                 coucal_key_const name) {
  return hashtable->custom.key.dup == NULL
    ? coucal_dup_name_internal(hashtable, name)
    : hashtable->custom.key.dup(hashtable->custom.key.arg, name);
}

/* internal pool free handler.
   note: pointer must have been kicked from the pool first */
static void coucal_free_key_internal(coucal hashtable, coucal_key name_) {
  char *const name = (char*) name_;
  const size_t len = strlen(name) + 1;

  /* see coucal_dup_name_internal() handling */
  if (len == 1 && name == the_empty_string) {
    coucal_assert(hashtable, the_empty_string[0] == '\0');
    return ;
  }

  coucal_assert(hashtable, *name != '\0' || !"duplicate or bad string pool release");
  hashtable->pool.used -= len;
  *name = '\0'; /* the string is now invalidated */

  /* compact the pool is too many holes  */
  if (hashtable->pool.used < hashtable->pool.size / 2) {
    size_t capacity = hashtable->pool.capacity;
    /* compact and shrink */
    if (hashtable->pool.used < capacity / 4) {
      capacity /= 2;
    }
    coucal_assert(hashtable, hashtable->pool.used < capacity);
    coucal_compact_pool(hashtable, capacity);
  }
}

/* free a key. default is to use the internal pool.
   note: pointer must have been kicked from the pool first */
static void coucal_free_key(coucal hashtable, coucal_key name) {
  if (hashtable->custom.key.free == NULL) {
    coucal_free_key_internal(hashtable, name);
  } else {
    hashtable->custom.key.free(hashtable->custom.key.arg, name);
  }
}

static INTHASH_INLINE size_t coucal_hash_to_pos_(size_t lg_size,
                                                 coucal_hashkey hash) {
  const coucal_hashkey mask = POW2(lg_size) - 1;
  return hash & mask;
}

static INTHASH_INLINE size_t coucal_hash_to_pos(const coucal hashtable,
                                                coucal_hashkey hash) {
  return coucal_hash_to_pos_(hashtable->lg_size, hash);
}

int coucal_read_pvoid(coucal hashtable, coucal_key_const name, void **pvalue) {
  coucal_value value = INTHASH_VALUE_NULL;
  const int ret =
    coucal_read_value(hashtable, name, (pvalue != NULL) ? &value : NULL);
  if (pvalue != NULL)
    *pvalue = value.ptr;
  return ret;
}

void* coucal_get_pvoid(coucal hashtable, coucal_key_const name) {
  void *value;
  if (!coucal_read_pvoid(hashtable, name, &value)) {
    return NULL;
  }
  return value;
}

int coucal_write_pvoid(coucal hashtable, coucal_key_const name, void *pvalue) {
  coucal_value value = INTHASH_VALUE_NULL;

  value.ptr = pvalue;
  return coucal_write_value(hashtable, name, value);
}

void coucal_add_pvoid(coucal hashtable, coucal_key_const name, void *pvalue) {
  coucal_value value = INTHASH_VALUE_NULL;

  value.ptr = pvalue;
  coucal_write_value(hashtable, name, value);
}

int coucal_write(coucal hashtable, coucal_key_const name, intptr_t intvalue) {
  coucal_value value = INTHASH_VALUE_NULL;

  value.intg = intvalue;
  return coucal_write_value(hashtable, name, value);
}

static void coucal_default_free_handler(coucal_opaque arg,
                                        coucal_value value) {
  (void) arg;
  if (value.ptr != NULL)
    free(value.ptr);
}

static INTHASH_INLINE void coucal_del_value_(coucal hashtable, coucal_value *pvalue) {
  if (pvalue->ptr != NULL) {
    if (hashtable->custom.value.free != NULL)
      hashtable->custom.value.free(hashtable->custom.value.arg, *pvalue);
    pvalue->ptr = NULL;
  }
}

static INTHASH_INLINE void coucal_del_value(coucal hashtable, size_t pos) {
  coucal_del_value_(hashtable, &hashtable->items[pos].value);
}

static void coucal_del_name(coucal hashtable, coucal_item *item) {
  const coucal_hashkeys nullHash = INTHASH_KEYS_NULL;
  char *const name = (char*) item->name;
  item->name = NULL;  /* there must be no reference remaining */
  item->hashes = nullHash;
  /* free after detach (we may compact the pool) */
  coucal_free_key(hashtable, name);
}

static void coucal_del_item(coucal hashtable, coucal_item *pitem) {
  coucal_del_value_(hashtable, &pitem->value);
  coucal_del_name(hashtable, pitem);
}

static int coucal_add_item_(coucal hashtable, coucal_item item);

/* Write (add or replace) a value in the hashtable. */
static int coucal_write_value_(coucal hashtable, coucal_key_const name,
                               coucal_value value) {
  coucal_item item;
  size_t pos;
  const coucal_hashkeys hashes = coucal_calc_hashes(hashtable, name);

  /* Statistics */
  hashtable->stats.write_count++;

  /* replace at position 1 ? */
  pos = coucal_hash_to_pos(hashtable, hashes.hash1);
  if (coucal_matches(hashtable, pos, name, &hashes)) {
    coucal_del_value(hashtable, pos);
    hashtable->items[pos].value = value;
    return 0;  /* replaced */
  }

  /* replace at position 2 ? */
  pos = coucal_hash_to_pos(hashtable, hashes.hash2);
  if (coucal_matches(hashtable, pos, name, &hashes)) {
    coucal_del_value(hashtable, pos);
    hashtable->items[pos].value = value;
    return 0;  /* replaced */
  }

  /* replace in the stash ? */
  if (hashtable->stash.size != 0) {
    size_t i;
    for(i = 0 ; i < hashtable->stash.size ; i++) {
      if (coucal_matches_(hashtable, &hashtable->stash.items[i], name, 
                           &hashes)) {
        coucal_del_value_(hashtable, &hashtable->stash.items[i].value);
        hashtable->stash.items[i].value = value;
        return 0;  /* replaced */
      }
    }
  }

  /* Statistics */
  hashtable->stats.add_count++;

  /* otherwise we need to create a new item */
  item.name = coucal_dup_name(hashtable, name);
  item.value = value;
  item.hashes = hashes;

  return coucal_add_item_(hashtable, item);
}

/* Return the string representation of a key */
static const char* coucal_print_key(coucal hashtable,
                                    coucal_key_const name) {
  return hashtable->custom.print.key != NULL
    ? hashtable->custom.print.key(hashtable->custom.print.arg, name)
    : (const char*) name;
}

/* Add a new item in the hashtable. The item SHALL NOT be already present. */
static int coucal_add_item_(coucal hashtable, coucal_item item) {
  coucal_hashkey cuckoo_hash, initial_cuckoo_hash;
  size_t loops;
  size_t pos;

  /* place at free position 1 ? */
  pos = coucal_hash_to_pos(hashtable, item.hashes.hash1);
  if (coucal_is_free(hashtable, pos)) {
    hashtable->items[pos] = item;
    return 1; /* added */
  } else {
    /* place at free position 2 ? */
    pos = coucal_hash_to_pos(hashtable, item.hashes.hash2);
    if (coucal_is_free(hashtable, pos)) {
      hashtable->items[pos] = item;
      return 1; /* added */
    }
    /* prepare cuckoo ; let's take position 1 */
    else {
      cuckoo_hash = initial_cuckoo_hash = item.hashes.hash1;
      coucal_trace(hashtable,
                    "debug:collision with '%s' at %"UINT_64_FORMAT" (%x)", 
                     coucal_print_key(hashtable, item.name),
                     (uint64_t) pos, cuckoo_hash);
    }
  }

  /* put 'item' in place with hash 'cuckoo_hash' */
  for(loops = POW2(hashtable->lg_size) ; loops != 0 ; --loops) {
    const size_t pos = coucal_hash_to_pos(hashtable, cuckoo_hash);

    coucal_trace(hashtable,
                  "\tdebug:placing cuckoo '%s' at %"UINT_64_FORMAT" (%x)", 
                  coucal_print_key(hashtable, item.name),
                  (uint64_t) pos, cuckoo_hash);

    /* place at alternate free position ? */
    if (coucal_is_free(hashtable, pos)) {
      coucal_trace(hashtable, "debug:free position");
      hashtable->items[pos] = item;
      return 1; /* added */
    }
    /* then cuckoo's place it is */
    else {
      /* replace */
      const coucal_item backup_item = hashtable->items[pos];
      hashtable->items[pos] = item;

      /* statistics */
      hashtable->stats.cuckoo_moved++;

      /* take care of new lost item */
      item = backup_item;

      /* we just kicked this item from its position 1 */
      if (pos == coucal_hash_to_pos(hashtable, item.hashes.hash1)) {
        /* then place it on position 2 on next run */
        coucal_trace(hashtable, "\tdebug:position 1");
        cuckoo_hash = item.hashes.hash2;
      }
      /* we just kicked this item from its position 2 */
      else if (pos == coucal_hash_to_pos(hashtable, item.hashes.hash2)) {
        /* then place it on position 1 on next run */
        coucal_trace(hashtable, "\tdebug:position 2");
        cuckoo_hash = item.hashes.hash1;
      }
      else {
        coucal_assert(hashtable, ! "hashtable internal error: unexpected position");
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
    coucal_debug(hashtable, "used stash because of collision (%d entries)",
                  (int) hashtable->stash.size);
    return 1; /* added */
  } else {
    /* debugging */
    if (hashtable->custom.print.key != NULL 
      && hashtable->custom.print.value != NULL) {
      size_t i;
      for(i = 0 ; i < hashtable->stash.size ; i++) {
        coucal_item *const item = &hashtable->stash.items[i];
        const size_t pos1 = coucal_hash_to_pos(hashtable, item->hashes.hash1);
        const size_t pos2 = coucal_hash_to_pos(hashtable, item->hashes.hash2);
        coucal_crit(hashtable, 
          "stash[%u]: key='%s' value='%s' pos1=%d pos2=%d hash1=%04x hash2=%04x",
          (int) i,
          hashtable->custom.print.key(hashtable->custom.print.arg, item->name),
          hashtable->custom.print.value(hashtable->custom.print.arg, item->value),
          (int) pos1, (int) pos2,
          item->hashes.hash1, item->hashes.hash2);
        if (!coucal_is_free(hashtable, pos1)) {
          coucal_item *const item = &hashtable->items[pos1];
          const size_t pos1 = coucal_hash_to_pos(hashtable, item->hashes.hash1);
          const size_t pos2 = coucal_hash_to_pos(hashtable, item->hashes.hash2);
          coucal_crit(hashtable, 
            "\t.. collisionning with key='%s' value='%s' pos1=%d pos2=%d hash1=%04x hash2=%04x",
            hashtable->custom.print.key(hashtable->custom.print.arg, item->name),
            hashtable->custom.print.value(hashtable->custom.print.arg, item->value),
            (int) pos1, (int) pos2,
            item->hashes.hash1, item->hashes.hash2);
        } else {
          coucal_crit(hashtable, "\t.. collisionning with a free slot (%d)!", (int) pos1);
        }
        if (!coucal_is_free(hashtable, pos2)) {
          coucal_item *const item = &hashtable->items[pos2];
          const size_t pos1 = coucal_hash_to_pos(hashtable, item->hashes.hash1);
          const size_t pos2 = coucal_hash_to_pos(hashtable, item->hashes.hash2);
          coucal_crit(hashtable, 
            "\t.. collisionning with key='%s' value='%s' pos1=%d pos2=%d hash1=%04x hash2=%04x",
            hashtable->custom.print.key(hashtable->custom.print.arg, item->name),
            hashtable->custom.print.value(hashtable->custom.print.arg, item->value),
            (int) pos1, (int) pos2,
            item->hashes.hash1, item->hashes.hash2);
        } else {
          coucal_crit(hashtable, "\t.. collisionning with a free slot (%d)!", (int) pos2);
        }
      }
    }

    /* we are doomed. hopefully the probability is lower than being killed
       by a wandering radioactive monkey */
    coucal_log_stats(hashtable);
    coucal_assert(hashtable, ! "hashtable internal error: cuckoo/stash collision");

    /* not reachable code */
    return -1;
  }
}

static INTHASH_INLINE int coucal_is_acceptable_pow2(size_t lg_size) {
  return lg_size <= COUCAL_HASH_SIZE && lg_size < sizeof(size_t)*8;
}

int coucal_write_value(coucal hashtable, coucal_key_const name,
                       coucal_value_const value) {
  /* replace of add item */
  const int ret = coucal_write_value_(hashtable, name, value);

  /* added ? */
  if (ret) {
    /* size of half of the table */
    const size_t half_size = POW2(hashtable->lg_size - 1);

    /* size of half of the stash */
    const size_t half_stash_size = STASH_SIZE / 2;

    /* item was added: increase count */
    hashtable->used++;

    /* table is more than half-full, or stash is more than half-full */
    if (hashtable->used >= half_size
      || hashtable->stash.size >= half_stash_size) {
      size_t i;

      /* size before  */
      const size_t prev_power = hashtable->lg_size;
      const size_t prev_size = half_size * 2;
      const size_t prev_alloc_size = prev_size*sizeof(coucal_item);

      /* size after doubling it */
      const size_t alloc_size = prev_alloc_size * 2;

      /* log stash issues */
      if (hashtable->stash.size >= half_stash_size 
        && half_size > POW2(16) 
        && hashtable->used < half_size / 4) {
          coucal_warning(hashtable, 
            "stash size still full despite %"UINT_64_FORMAT
            " elements used out of %"UINT_64_FORMAT,
            (uint64_t) hashtable->used, (uint64_t) half_size*2);
      }

      /* statistics */
      hashtable->stats.rehash_count++;

      /* realloc */
      hashtable->lg_size++;
      coucal_assert(hashtable, coucal_is_acceptable_pow2(hashtable->lg_size));
      hashtable->items = 
        (coucal_item *) realloc(hashtable->items, alloc_size);
      if (hashtable->items == NULL) {
        coucal_crit(hashtable,
          "** hashtable allocation error: "
          "could not allocate %"UINT_64_FORMAT" bytes", 
          (uint64_t) alloc_size);
        coucal_assert(hashtable, ! "hashtable allocation error");
      }

      /* clear upper half */
      memset(&hashtable->items[prev_size], 0, prev_alloc_size);

      /* relocate lower half items when needed */
      for(i = 0 ; i < prev_size ; i++) {
        if (!coucal_is_free(hashtable, i)) {
          const coucal_hashkeys *const hashes = &hashtable->items[i].hashes;

          /* currently at old position 1 */
          if (coucal_hash_to_pos_(prev_power, hashes->hash1) == i) {
            const size_t pos = coucal_hash_to_pos(hashtable, hashes->hash1);
            /* no more the expected position */
            if (pos != i) {
              coucal_assert(hashtable, pos >= prev_size);
              hashtable->items[pos] = hashtable->items[i];
              memset(&hashtable->items[i], 0, sizeof(hashtable->items[i]));
            }
          }
          else if (coucal_hash_to_pos_(prev_power, hashes->hash2) == i) {
            const size_t pos = coucal_hash_to_pos(hashtable, hashes->hash2);
            /* no more the expected position */
            if (pos != i) {
              coucal_assert(hashtable, pos >= prev_size);
              hashtable->items[pos] = hashtable->items[i];
              memset(&hashtable->items[i], 0, sizeof(hashtable->items[i]));
            }
          }
          else {
            coucal_assert(hashtable, ! "hashtable unexpected internal error (bad position)");
          }
        }
      }

      coucal_debug(hashtable,
                    "expanded hashtable to %"UINT_64_FORMAT" elements",
                    (uint64_t) POW2(hashtable->lg_size));

      /* attempt to merge the stash if present */
      if (hashtable->stash.size != 0) {
        const size_t old_size = hashtable->stash.size;
        size_t i;

        /* backup stash and reset it */
        coucal_item stash[STASH_SIZE];
        memcpy(&stash, hashtable->stash.items, sizeof(hashtable->stash.items));
        hashtable->stash.size = 0;

        /* insert all items */
        for(i = 0 ; i < old_size ; i++) {
          const int ret = coucal_add_item_(hashtable, stash[i]);
          if (ret == 0) {
            coucal_assert(hashtable, ! "hashtable duplicate key when merging the stash");
          }
        }

        /* logging */
        coucal_assert(hashtable, hashtable->stash.size <= old_size);
        if (hashtable->stash.size < old_size) {
          coucal_debug(hashtable, "reduced stash size from %"UINT_64_FORMAT" "
                        "to %"UINT_64_FORMAT,
                        (uint64_t) old_size, (uint64_t) hashtable->stash.size);
        } else {
          coucal_trace(hashtable, "stash has still %"UINT_64_FORMAT" elements",
                        (uint64_t) hashtable->stash.size);
        }
      }

    }
  }

  return ret;
}

void coucal_add(coucal hashtable, coucal_key_const name, intptr_t intvalue) {
  coucal_value value = INTHASH_VALUE_NULL;

  memset(&value, 0, sizeof(value));
  value.intg = intvalue;
  coucal_write_value(hashtable, name, value);
}

int coucal_read(coucal hashtable, coucal_key_const name, intptr_t * intvalue) {
  coucal_value value = INTHASH_VALUE_NULL;
  int ret =
    coucal_read_value(hashtable, name, (intvalue != NULL) ? &value : NULL);
  if (intvalue != NULL)
    *intvalue = value.intg;
  return ret;
}

coucal_value* coucal_fetch_value_hashes(coucal hashtable,
                                        coucal_key_const name,
                                        const coucal_hashkeys *hashes) {
  size_t pos;

  /* found at position 1 ? */
  pos = coucal_hash_to_pos(hashtable, hashes->hash1);
  if (coucal_matches(hashtable, pos, name, hashes)) {
    return &hashtable->items[pos].value;
  }

  /* found at position 2 ? */
  pos = coucal_hash_to_pos(hashtable, hashes->hash2);
  if (coucal_matches(hashtable, pos, name, hashes)) {
    return &hashtable->items[pos].value;
  }

  /* find in stash ? */
  if (hashtable->stash.size != 0) {
    size_t i;
    for(i = 0 ; i < hashtable->stash.size ; i++) {
      if (coucal_matches_(hashtable, &hashtable->stash.items[i], name,
                          hashes)) {
        return &hashtable->stash.items[i].value;
      }
    }
  }

  /* not found */
  return NULL;
}

INTHASH_INLINE coucal_value* coucal_fetch_value(coucal hashtable,
                                                coucal_key_const name) {
  const coucal_hashkeys hashes = coucal_calc_hashes(hashtable, name);
  return coucal_fetch_value_hashes(hashtable, name, &hashes);
}

int coucal_read_value(coucal hashtable, coucal_key_const name,
                      coucal_value * pvalue) {
  coucal_value* const value = coucal_fetch_value(hashtable, name);
  if (value != NULL) {
    if (pvalue != NULL) {
      *pvalue = *value;
    }
    return 1;
  }
  return 0;
}

static size_t coucal_inc_(coucal hashtable, coucal_key_const name,
                          size_t inc) {
  coucal_value* const value = coucal_fetch_value(hashtable, name);
  if (value != NULL) {
    value->uintg += inc;
    return value->uintg;
  } else {
    /* create a new value */
    const int ret = coucal_write(hashtable, name, inc);
    coucal_assert(hashtable, ret);
    return inc;
  }
}

int coucal_inc(coucal hashtable, coucal_key_const name) {
  return (int) coucal_inc_(hashtable, name, 1);
}

int coucal_dec(coucal hashtable, coucal_key_const name) {
  return (int) coucal_inc_(hashtable, name, (size_t) -1);
}

int coucal_exists(coucal hashtable, coucal_key_const name) {
  return coucal_read_value(hashtable, name, NULL);
}

static int coucal_remove_(coucal hashtable, coucal_key_const name,
                          const coucal_hashkeys *hashes, size_t *removed) {
  size_t pos;

  /* found at position 1 ? */
  pos = coucal_hash_to_pos(hashtable, hashes->hash1);
  if (coucal_matches(hashtable, pos, name, hashes)) {
    coucal_del_item(hashtable, &hashtable->items[pos]);
    *removed = pos;
    return 1;
  }

  /* found at position 2 ? */
  pos = coucal_hash_to_pos(hashtable, hashes->hash2);
  if (coucal_matches(hashtable, pos, name, hashes)) {
    coucal_del_item(hashtable, &hashtable->items[pos]);
    *removed = pos;
    return 1;
  }

  /* find in stash ? */
  if (hashtable->stash.size != 0) {
    size_t i;
    for(i = 0 ; i < hashtable->stash.size ; i++) {
      if (coucal_matches_(hashtable, &hashtable->stash.items[i], name,
                           hashes)) {
        coucal_del_item(hashtable, &hashtable->stash.items[i]);
        for( ; i + 1 < hashtable->stash.size ; i++) {
          hashtable->stash.items[i] = hashtable->stash.items[i + 1];
        }
        hashtable->stash.size--;
        *removed = (size_t) -1;
        coucal_debug(hashtable, "debug:deleted item in stash (%d entries)",
          (int) hashtable->stash.size);
        return 1;
      }
    }
  }

  /* not found */
  return 0;
}

int coucal_remove(coucal hashtable, coucal_key_const name) {
  const coucal_hashkeys hashes = coucal_calc_hashes(hashtable, name);
  size_t removed;
  const int ret = coucal_remove_(hashtable, name, &hashes, &removed);

  if (ret) {
    /* item was removed: decrease count */
    coucal_assert(hashtable, hashtable->used != 0);
    hashtable->used--;

    /* can we place stash entry back to the table ? */
    if (hashtable->stash.size != 0 && removed != (size_t) -1) {
      size_t i;
      for(i = 0 ; i < hashtable->stash.size ; i++) {
        const size_t pos1 =
          coucal_hash_to_pos(hashtable, hashtable->stash.items[i].hashes.hash1);
        const size_t pos2 =
          coucal_hash_to_pos(hashtable, hashtable->stash.items[i].hashes.hash2);
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
          coucal_debug(hashtable, "debug:moved item from stash (%d entries)",
            (int) hashtable->stash.size);
          break;
        }
      }
    }
  }

  return ret;
}

int coucal_readptr(coucal hashtable, coucal_key_const name, intptr_t * value) {
  int ret;

  *value = 0;
  ret = coucal_read(hashtable, name, value);
  if (*value == 0)
    ret = 0;
  return ret;
}

intptr_t coucal_get_intptr(coucal hashtable, coucal_key_const name) {
  intptr_t value;
  if (!coucal_read(hashtable, name, &value)) {
    return 0;
  }
  return value;
}

static INTHASH_INLINE size_t coucal_get_pow2(size_t initial_size) {
  size_t size;
  for(size = MIN_LG_SIZE 
    ; size <= COUCAL_HASH_SIZE && POW2(size) < initial_size
    ; size++) ;
  return size;
}

coucal coucal_new(size_t initial_size) {
  const size_t lg_size = coucal_get_pow2(initial_size);
  const int lg_valid = coucal_is_acceptable_pow2(lg_size);
  coucal hashtable = lg_valid 
    ? (coucal) calloc(1, sizeof(struct_coucal)) : NULL;
  coucal_item *const items = 
    (coucal_item *) calloc(POW2(lg_size), sizeof(coucal_item));

  if (lg_valid && items != NULL && hashtable != NULL) {
    hashtable->lg_size = lg_size;
    hashtable->items = items;
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
    hashtable->custom.error.log = NULL;
    hashtable->custom.error.fatal = NULL;
    hashtable->custom.error.name = NULL;
    hashtable->custom.error.arg = NULL;
    hashtable->custom.print.key = NULL;
    hashtable->custom.print.value = NULL;
    hashtable->custom.print.arg = NULL;
    return hashtable;
  }
  if (items != NULL) {
    free(items);
  }
  if (hashtable != NULL) {
    free(hashtable);
  }
  return NULL;
}

int coucal_created(coucal hashtable) {
  return hashtable != NULL && hashtable->items != NULL;
}

void coucal_value_is_malloc(coucal hashtable, int flag) {
  if (flag) {
    if (hashtable->custom.value.free == NULL) {
      hashtable->custom.value.free = coucal_default_free_handler;
      hashtable->custom.value.arg = NULL;
    }
  } else {
    hashtable->custom.value.free = NULL;
    hashtable->custom.value.arg = NULL;
  }
}

void coucal_set_name(coucal hashtable, coucal_key_const name) {
  hashtable->custom.error.name = name;
}

void coucal_value_set_value_handler(coucal hashtable,
                                    t_coucal_value_freehandler free,
                                    coucal_opaque arg) {
  hashtable->custom.value.free = free;
  hashtable->custom.value.arg = arg;
}

void coucal_value_set_key_handler(coucal hashtable,
                                  t_coucal_duphandler dup,
                                  t_coucal_key_freehandler free,
                                  t_coucal_hasheshandler hash,
                                  t_coucal_cmphandler equals,
                                  coucal_opaque arg) {
  /* dup and free must be consistent */
  coucal_assert(hashtable, ( dup == NULL ) == ( free == NULL ) );
  hashtable->custom.key.dup = dup;
  hashtable->custom.key.free = free;
  hashtable->custom.key.hash = hash;
  hashtable->custom.key.equals = equals;
  hashtable->custom.key.arg = arg;
}

void coucal_set_assert_handler(coucal hashtable,
                               t_coucal_loghandler log,
                               t_coucal_asserthandler fatal,
                               coucal_opaque arg) {
  hashtable->custom.error.log = log;
  hashtable->custom.error.fatal = fatal;
  hashtable->custom.error.arg = arg;
}

void coucal_set_print_handler(coucal hashtable,
                              t_coucal_printkeyhandler key,
                              t_coucal_printvaluehandler value,
                              coucal_opaque arg) {
  hashtable->custom.print.key = key;
  hashtable->custom.print.value = value;
  hashtable->custom.print.arg = arg;
}

size_t coucal_nitems(coucal hashtable) {
  if (hashtable != NULL)
    return hashtable->used;
  return 0;
}

size_t coucal_memory_size(coucal hashtable) {
  const size_t size_struct = sizeof(struct_coucal);
  const size_t hash_size = POW2(hashtable->lg_size)*sizeof(coucal_item);
  const size_t pool_size = hashtable->pool.capacity*sizeof(char);
  return size_struct + hash_size + pool_size;
}

size_t coucal_hash_size(void) {
  return COUCAL_HASH_SIZE;
}

void coucal_delete(coucal *phashtable) {
  if (phashtable != NULL) {
    coucal hashtable = *phashtable;
    if (hashtable != NULL) {
      coucal_log_stats(hashtable);
      if (hashtable->items != NULL) {
        /* we need to delete values */
        const size_t hash_size = POW2(hashtable->lg_size);
        size_t i;

        /* wipe hashtable values (not names) */
        for(i = 0 ; i < hash_size ; i++) {
          if (!coucal_is_free(hashtable, i)) {
            coucal_del_value(hashtable, i);
          }
        }

        /* wipe auxiliary stash values (not names) if any */
        for(i = 0 ; i < hashtable->stash.size ; i++) {
          coucal_del_value_(hashtable, &hashtable->stash.items[i].value);
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

struct_coucal_enum coucal_enum_new(coucal hashtable) {
  struct_coucal_enum e;

  e.index = 0;
  e.table = hashtable;
  return e;
}

coucal_item *coucal_enum_next(struct_coucal_enum * e) {
  const size_t hash_size = POW2(e->table->lg_size);
  for( ; e->index < hash_size 
    && coucal_is_free(e->table, e->index) ; e->index++) ;
  /* enumerate all table */
  if (e->index < hash_size) {
    coucal_item *const next = &e->table->items[e->index];
    e->index++;
    return next;
  }
  /* enumerate stash if present */
  else if (e->index < hash_size + e->table->stash.size) {
    const size_t index = e->index - hash_size;
    coucal_item *const next = &e->table->stash.items[index];
    e->index++;
    return next;
  }
  /* eof */
  else {
    return NULL;
  }
}

void coucal_set_global_assert_handler(t_coucal_loghandler log,
                                      t_coucal_asserthandler fatal) {
  global_log_handler = log;
  global_assert_handler = fatal;
}
