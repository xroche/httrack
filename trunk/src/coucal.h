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

/**
 * Coucal, a Cuckoo-hashing-based hashtable with stash area C library.
 *
 * This hashtable library provides key/value hashtable, with by default a
 * string key, and integer/pointer value (with an associated optional
 * allocator). Both key and value can be of any type, using custom settings.
 * By default, the string key is dup'ed using a string pool, and the opaque
 * value is stored as-is (either the pointer, or an uintptr_t integer value).
 * It features O(1) average insertion, and guaranteed O(1) lookup, replace,
 * and delete operations.
 *
 * Implementation notes:
 * Implementation is auto-rehashable, and uses cuckoo hashing of size 2**n
 * with a Murmur or MD5 hash function, with one additional auxiliary hash
 * function.
 * It also uses a small stash area to handle rare cases of collisions.
 * Enumeration of all key/values is possible, deletion is also possible, but
 * currently without any auto-shrinking (ie. table will never shrink).
 * Overall, two main blocks are allocated: one for the items, and one for
 * the keys (pool).
 * If the string pool is being used (to store dup'ed keys), deleting is only
 * O(1) average, because the pool needs to be compacted time to time.
 * Currently the default maximum number of elements in the hashtable is
 * 2**31 - 1 (that is, 2,147,483,648 elements), but this default can be changed
 * by setting COUCAL_HASH_SIZE to a higher value (64 is the only higher value
 * currently supported), and rebuilding the library.
 *
 * References: 
 *
 * Cuckoo Hashing, by Rasmus Pagh and Flemming Friche Rodler
 * http://www.it-c.dk/people/pagh/papers/cuckoo-jour.pdf
 *
 * Cuckoo Stash, by Adam Kirsch, Michael Mitzenmacher and Udi Wieder
 * http://research.microsoft.com/pubs/73856/stash-full.9-30.pdf
 *
 * MurmurHash3, by Austin Appleby
 * http://en.wikipedia.org/wiki/MurmurHash
 *
 * MD5 http://en.wikipedia.org/wiki/MD5
 * FNV http://www.isthe.com/chongo/tech/comp/fnv/
 **/

#ifndef COUCAL_DEFH
#define COUCAL_DEFH

/* Includes */
#ifdef _WIN32
#include <stddef.h>
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;
#elif (defined(SOLARIS) || defined(sun) || defined(HAVE_INTTYPES_H) \
  || defined(BSD) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__FreeBSD_kernel__))
#include <inttypes.h>
#else
#include <stdint.h>
#endif
#include <stdarg.h>

/* External definitions */
#ifndef COUCAL_EXTERN
#ifdef _WIN32
#ifdef COUCAL_BUILDING
#define COUCAL_EXTERN __declspec(dllimport)
#else
#define COUCAL_EXTERN __declspec(dllexport)
#endif
#elif ( ( defined(__GNUC__) && ( __GNUC__ >= 4 ) ) \
        || ( defined(HAVE_VISIBILITY) && HAVE_VISIBILITY ) )
#define COUCAL_EXTERN extern __attribute__ ((visibility("default")))
#else
#define COUCAL_EXTERN extern
#endif
#endif

/** Key opaque type. May be a regular 'const char*'. **/
typedef void* coucal_key;

/** Key constant (can not be modified) opaque type. **/
typedef const void* coucal_key_const;

/** Opaque user-defined pointer. **/
typedef void* coucal_opaque;

/** Value (union of any value). **/
typedef union coucal_value {
  /** Integer value. **/
  intptr_t intg;

  /** Unsigned integer value. **/
  uintptr_t uintg;

  /** Pointer value. **/
  void *ptr;
} coucal_value;

/** Value constant. **/
typedef const coucal_value coucal_value_const;

/** NULL Value. **/
#define INTHASH_VALUE_NULL { 0 }

#ifndef HTS_DEF_FWSTRUCT_coucal_item
#define HTS_DEF_FWSTRUCT_coucal_item
typedef struct coucal_item coucal_item;
#endif

/** Coucal primary hash size. Default is 32-bit. **/
#ifndef COUCAL_HASH_SIZE
#define COUCAL_HASH_SIZE 32
#endif

/** Hash integer type. **/
#if (COUCAL_HASH_SIZE == 32)

/** 32-bit hash key. **/
typedef uint32_t coucal_hashkey;

#elif (COUCAL_HASH_SIZE == 64)

/** 64-bit hash key. **/
typedef uint64_t coucal_hashkey;

#else
#error "Unsupported COUCAL_HASH_SIZE"
#endif

/** Pair of hashes **/
typedef struct coucal_hashkeys {
  coucal_hashkey hash1;
  coucal_hashkey hash2;
} coucal_hashkeys;

/** NULL pair of hashes. **/
#define INTHASH_KEYS_NULL { 0, 0 }

/** Item holding a value. **/
struct coucal_item {
  /** Key. **/
  coucal_key name;

  /** Value. **/
  coucal_value value;

  /** Hashes of the key. **/
  coucal_hashkeys hashes;
};

/** Log level. **/
typedef enum coucal_loglevel {
  coucal_log_critical,
  coucal_log_warning,
  coucal_log_info,
  coucal_log_debug,
  coucal_log_trace
} coucal_loglevel;

/**  free handler. Only used when values are markes as xxc **/
typedef void (*t_coucal_key_freehandler)(coucal_opaque arg,
                                         coucal_key key);

/** Value free handler. Only used when values are markes as xxc **/
typedef void (*t_coucal_value_freehandler)(coucal_opaque arg,
                                           coucal_value value);

/** Key dup handler. **/
typedef coucal_key (*t_coucal_duphandler)(coucal_opaque arg,
                                          coucal_key_const name);

/** Key hash computation handler. **/
typedef coucal_hashkeys (*t_coucal_hasheshandler)(coucal_opaque arg,
                                                  coucal_key_const name);

/** Hashtable logging handler. **/
typedef void (*t_coucal_loghandler)(coucal_opaque arg, coucal_loglevel level, 
                                    const char* format, va_list args);

/** Hashtable fatal assertion failure. **/
typedef void (*t_coucal_asserthandler)(coucal_opaque arg, const char* exp,
                                       const char* file, int line);

/** Key printer (debug) **/
typedef const char* (*t_coucal_printkeyhandler)(coucal_opaque arg,
                                                coucal_key_const name);

/** Value printer (debug) **/
typedef const char* (*t_coucal_printvaluehandler)(coucal_opaque arg,
                                                  coucal_value_const value);

/**
 * Value comparison handler (returns non-zero value if strings are equal).
 **/
typedef int (*t_coucal_cmphandler)(coucal_opaque arg,
                                   coucal_key_const a,
                                   coucal_key_const b);

/** Hashtable (opaque structure). **/
#ifndef HTS_DEF_FWSTRUCT_struct_coucal
#define HTS_DEF_FWSTRUCT_struct_coucal
typedef struct struct_coucal struct_coucal, *coucal;
#endif

/** Hashtable enumeration (opaque structure). **/
#ifndef HTS_DEF_FWSTRUCT_struct_coucal_enum
#define HTS_DEF_FWSTRUCT_struct_coucal_enum
typedef struct struct_coucal_enum struct_coucal_enum;
#endif

/** Enumeration. **/
struct struct_coucal_enum {
  coucal table;
  size_t index;
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create a new hashtable, with initial bucket size of 'size'.
 * If size is 0, use the default minimal bucket size.
 * Return a non-NULL pointer upon success.
 *
 * By default, keys are supposed to be '\0'-terminated strings, which are
 * duplicated by the library (the passed pointer does not need to be
 * persistent), and values are opaque pointers (or integers) which are copied
 * "as is", without further processing. Use coucal_value_set_key_handler()
 * and coucal_value_set_value_handler() to alter this default behavior.
 **/
COUCAL_EXTERN coucal coucal_new(size_t size);

/**
 * Was the hashtable successfully created ?
 * Return non-zero value if the hashtable is valid.
 **/
COUCAL_EXTERN int coucal_created(coucal hashtable);

/**
 * Delete a hashtable, freeing all entries.
 **/
COUCAL_EXTERN void coucal_delete(coucal * hashtable);

/**
 * Return the number of items in the hashtable.
 **/
COUCAL_EXTERN size_t coucal_nitems(coucal hashtable);

/**
 * Return the memory size taken by the hashtable.
 * (This does not take account of the possible memory taken by values)
 **/
COUCAL_EXTERN size_t coucal_memory_size(coucal hashtable);

/**
 * Return the library hash size compiled.
 * The returned value MUST match COUCAL_HASH_SIZE.
 **/
COUCAL_EXTERN size_t coucal_hash_size(void);

/**
 * If 'flag' is non-zero, calls coucal_value_set_value_handler() with
 * default system free() handler function, otherwise, free the value handlers.
 **/
COUCAL_EXTERN void coucal_value_is_malloc(coucal hashtable, int flag);

/**
 * Set handlers for values.
 * free: this handler will be called when a value is to be removed from
 * the hashtable. if NULL, values won't be free'd.
 * arg: opaque custom argument to be used by functions.
 * Handler(s) MUST NOT be changed once elements have been added.
 **/
COUCAL_EXTERN void coucal_value_set_value_handler(coucal hashtable,
                                                  t_coucal_value_freehandler free,
                                                  coucal_opaque arg);

/**
 * Set handlers for keys.
 * dup: handler called to duplicate a key. if NULL, the internal pool is used.
 * free: handler called to free a key. if NULL, the internal pool is used.
 * hash: hashing handler, called to hash a key. if NULL, the default hash
 * function is used.
 * equals: comparison handler, returning non-zero value when two keys are
 * identical. if NULL, the default comparison function is used.
 * arg: opaque custom argument to be used by functions.
 * Handler(s) MUST NOT be changed once elements have been added.
 **/
COUCAL_EXTERN void coucal_value_set_key_handler(coucal hashtable,
                                                t_coucal_duphandler dup,
                                                t_coucal_key_freehandler free,
                                                t_coucal_hasheshandler hash,
                                                t_coucal_cmphandler equals,
                                                coucal_opaque arg);

/**
 * Set assertion failure handler.
 * log: handler called upon serious programming error
 * fatal: handler called upon serious programming error
 **/
COUCAL_EXTERN void coucal_set_assert_handler(coucal hashtable,
                                             t_coucal_loghandler log,
                                             t_coucal_asserthandler fatal,
                                             coucal_opaque arg);

/**
 * Set pretty print loggers (debug). Both handlers must return a string
 * pointer which shall be valid until the next call. Both key and value
 * pointers shall be valid at the same time.
 * name: handler called to print the string representation of the name
 * value: handler called to print the string representation of the value
 **/
COUCAL_EXTERN void coucal_set_print_handler(coucal hashtable,
                                            t_coucal_printkeyhandler key,
                                            t_coucal_printvaluehandler value,
                                            coucal_opaque arg);

/**
 * Set the hashtable name, for degugging purpose.
 * name: the hashtable name (ASCII or UTF-8)
 */
COUCAL_EXTERN void coucal_set_name(coucal hashtable, coucal_key_const name);

/**
 * Get the hashtable name, for degugging purpose.
 * Return NULL if no name was defined.
 **/
COUCAL_EXTERN const char* coucal_get_name(coucal hashtable);

/**
 * Read an integer entry from the hashtable.
 * Return non-zero value upon success and sets intvalue.
 **/
COUCAL_EXTERN int coucal_read(coucal hashtable, coucal_key_const name,
                              intptr_t * intvalue);

/**
 * Same as coucal_read(), but return 0 is the value was zero.
 **/
COUCAL_EXTERN int coucal_readptr(coucal hashtable, coucal_key_const name,
                                 intptr_t * intvalue);

/**
 * Read an integer entry from the hashtable.
 * Return 0 if the entry could not be found.
 **/
COUCAL_EXTERN intptr_t coucal_get_intptr(coucal hashtable,
                                         coucal_key_const name);

/**
 * Return non-zero value if the given entry exists.
 **/
COUCAL_EXTERN int coucal_exists(coucal hashtable, coucal_key_const name);

/**
 * Read an entry from the hashtable.
 * Return non-zero value upon success and sets value.
 **/
COUCAL_EXTERN int coucal_read_value(coucal hashtable, coucal_key_const name,
                                    coucal_value *value);

/**
 * Write an entry to the hashtable.
 * Return non-zero value if the entry was added, zero if it was replaced.
 **/
COUCAL_EXTERN int coucal_write_value(coucal hashtable, coucal_key_const name,
                                     coucal_value_const value);

/**
 * Read a pointer entry from the hashtable.
 * Return non-zero value upon success and sets value.
 **/
COUCAL_EXTERN int coucal_read_pvoid(coucal hashtable, coucal_key_const name,
                                    void **value);

/**
 * Read a pointer entry from the hashtable and returns its value.
 * Return NULL if the entry could not be found.
 **/
COUCAL_EXTERN void* coucal_get_pvoid(coucal hashtable, coucal_key_const name);

/**
 * Write a pointer entry to the hashtable.
 * Return non-zero value if the entry was added, zero if it was replaced.
 **/
COUCAL_EXTERN int coucal_write_pvoid(coucal hashtable, coucal_key_const name,
                                     void *value);

/**
 * Alias to coucal_write_pvoid()
 **/
COUCAL_EXTERN void coucal_add_pvoid(coucal hashtable, coucal_key_const name,
                                    void *value);

/**
 * Write an integer entry to the hashtable.
 * Return non-zero value if the entry was added, zero if it was replaced.
 **/
COUCAL_EXTERN int coucal_write(coucal hashtable, coucal_key_const name,
                               intptr_t value);

/**
 * Alias to coucal_write()
 **/
COUCAL_EXTERN void coucal_add(coucal hashtable, coucal_key_const name,
                              intptr_t value);

/**
 * Increment an entry value in the hashtable
 * (or create a new entry with value 1 if it does not yet exist)
 * Return non-zero value if the entry was added, zero if it was changed.
 **/
COUCAL_EXTERN int coucal_inc(coucal hashtable, coucal_key_const name);

/**
 * Decrement an entry value in the hashtable 
 * (or create a new entry with value -1 if it does not yet exist)
 * Return non-zero value if the entry was added, zero if it was changed.
 **/
COUCAL_EXTERN int coucal_dec(coucal hashtable, coucal_key_const name);

/**
 * Fetch an entry value from the hashtable.
 * Returns NULL if the entry could not be found.
 * The returned pointer is only valid until next call to this library, and can
 * be used for read or write operations.
 **/
COUCAL_EXTERN coucal_value* coucal_fetch_value(coucal hashtable,
                                               coucal_key_const name);

/**
 * Fetch an entry value from the hashtable, given a name, and its hashes.
 * Returns NULL if the entry could not be found.
 * The returned pointer is only valid until next call to this library, and can
 * be used for read or write operations.
 * The hashes MUST have been computed using coucal_calc_hashes(), or by
 * copying an existing hash during an enumeration.
 * The use of a non-matching hash is safe, but will return NULL.
 **/
COUCAL_EXTERN coucal_value* coucal_fetch_value_hashes(coucal hashtable,
                                                      coucal_key_const name,
                                                      const coucal_hashkeys
                                                      *hashes);

/**
 * Remove an entry from the hashtable
 * Return non-zero value if the entry was removed, zero otherwise.
 **/
COUCAL_EXTERN int coucal_remove(coucal hashtable, coucal_key_const name);

/**
 * Return a new enumerator.
 * Note: deleting entries is safe while enumerating, but adding entries 
 * lead to undefined enumeration behavior (yet safe).
 **/
COUCAL_EXTERN struct_coucal_enum coucal_enum_new(coucal hashtable);

/**
 * Enumerate the next entry.
 **/
COUCAL_EXTERN coucal_item *coucal_enum_next(struct_coucal_enum * e);

/**
 * Compute a hash of a key for the hashtable 'hashtable'.
 * The returned hash is suitable for use with coucal_fetch_value_hashes()
 * Note: the default implementation uses coucal_hash_string()
 **/
COUCAL_EXTERN coucal_hashkeys coucal_calc_hashes(coucal hashtable, 
                                                 coucal_key_const value);

/**
 * Compute a hash, given a string. This is the default function used for
 * hashing keys, which are by default strings. This function uses
 * coucal_hash_data() as backend.
 **/
COUCAL_EXTERN coucal_hashkeys coucal_hash_string(const char *value);

/**
 * Compute a hash, given an opaque buffer.
 **/
COUCAL_EXTERN coucal_hashkeys coucal_hash_data(const void *data, size_t size);

/**
 * Set default global assertion failure handler.
 * The handler will be used if no specific handler was defined in the
 * hashtable itself.
 * log: handler called upon serious error log (opaque argument 
 * is the hashtable itself)
 * fatal: handler called upon serious programming error (opaque argument 
 * is the hashtable itself)
 **/
COUCAL_EXTERN void coucal_set_global_assert_handler(t_coucal_loghandler log,
                                                    t_coucal_asserthandler fatal);

#ifdef __cplusplus
}
#endif

#endif
