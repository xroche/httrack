/* ------------------------------------------------------------ */
/*
Coucal, Cuckoo hashing-based hashtable with stash area.
Copyright (C) 2013-2014 Xavier Roche and other contributors
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

  Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

  Redistributions in binary form must reproduce the above copyright notice, this
  list of conditions and the following disclaimer in the documentation and/or
  other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/**
 * Library notes:
 * This small hashtable library provides key/value hashtable, with a string
 * key, and integer/pointer value (with an associated optional allocator)
 * It features O(1) average insertion, O(1) lookup, and O(1) delete.
 *
 * Implementation notes:
 * Implementation is auto-rehashable, and uses cuckoo hashing of size 2**n
 * with a MD5 or FNV-1 hash function, with one additional auxiliary hash
 * function.
 * It also uses a small stash area to handle rare cases of collisions.
 * Enumeration of all key/values is possible, deletion is also possible, but
 * currently without any auto-shrinking (ie. table will never shrink).
 * Overall, two main blocks are allocated: one for the items, and one for
 * the keys (pool).
 *
 * References: 
 * Cuckoo Hashing http://en.wikipedia.org/wiki/Cuckoo_hashing
 * Cuckoo Stash http://research.microsoft.com/pubs/73856/stash-full.9-30.pdf
 * FNV http://www.isthe.com/chongo/tech/comp/fnv/
 * MD5 http://en.wikipedia.org/wiki/MD5
 **/

#ifndef HTSINTHASH_DEFH
#define HTSINTHASH_DEFH

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

/** Key opaque type. May be a regular 'const char*'. **/
typedef void* inthash_key;

/** Key constant (can not be modified) opaque type. **/
typedef const void* inthash_key_const;

/** Opaque user-defined pointer. **/
typedef void* inthash_opaque;

/** Value (union of any value). **/
typedef union inthash_value {
  /** Integer value. **/
  intptr_t intg;

  /** Unsigned integer value. **/
  uintptr_t uintg;

  /** Pointer value. **/
  void *ptr;
} inthash_value;

/** Value constant. **/
typedef const inthash_value inthash_value_const;

/** NULL Value. **/
#define INTHASH_VALUE_NULL { 0 }

#ifndef HTS_DEF_FWSTRUCT_inthash_item
#define HTS_DEF_FWSTRUCT_inthash_item
typedef struct inthash_item inthash_item;
#endif

/** Hash key (32-bit) **/
typedef uint32_t inthash_hashkey;

/** Pair of hashes **/
typedef struct inthash_hashkeys {
  inthash_hashkey hash1;
  inthash_hashkey hash2;
} inthash_hashkeys;

/** NULL pair of hashes. **/
#define INTHASH_KEYS_NULL { 0, 0 }

/** Item holding a value. **/
struct inthash_item {
  /** Key. **/
  inthash_key name;

  /** Value. **/
  inthash_value value;

  /** Hashes of the key. **/
  inthash_hashkeys hashes;
};

/** Log level. **/
typedef enum inthash_loglevel {
  inthash_log_critical,
  inthash_log_warning,
  inthash_log_info,
  inthash_log_debug,
  inthash_log_trace
} inthash_loglevel;

/**  free handler. Only used when values are markes as xxc **/
typedef void (*t_inthash_key_freehandler)(inthash_opaque arg,
                                          inthash_key key);

/** Value free handler. Only used when values are markes as xxc **/
typedef void (*t_inthash_value_freehandler)(inthash_opaque arg,
                                            inthash_value value);

/** Key dup handler. **/
typedef inthash_key (*t_inthash_duphandler)(inthash_opaque arg,
                                            inthash_key_const name);

/** Key hash computation handler. **/
typedef inthash_hashkeys (*t_inthash_hasheshandler)(inthash_opaque arg,
                                                    inthash_key_const name);

/** Hashtable logging handler. **/
typedef void (*t_inthash_loghandler)(inthash_opaque arg, inthash_loglevel level, 
                                     const char* format, va_list args);

/** Hashtable fatal assertion failure. **/
typedef void (*t_inthash_asserthandler)(inthash_opaque arg, const char* exp,
                                        const char* file, int line);

/** Key printer (debug) **/
typedef const char* (*t_inthash_printkeyhandler)(inthash_opaque arg,
                                                 inthash_key_const name);

/** Value printer (debug) **/
typedef const char* (*t_inthash_printvaluehandler)(inthash_opaque arg,
                                                   inthash_value_const value);

/**
 * Value comparison handler (returns non-zero value if strings are equal).
 **/
typedef int (*t_inthash_cmphandler)(inthash_opaque arg,
                                    inthash_key_const a,
                                    inthash_key_const b);

/** Hashtable (opaque structure). **/
#ifndef HTS_DEF_FWSTRUCT_struct_inthash
#define HTS_DEF_FWSTRUCT_struct_inthash
typedef struct struct_inthash struct_inthash, *inthash;
#endif

/** Hashtable enumeration (opaque structure). **/
#ifndef HTS_DEF_FWSTRUCT_struct_inthash_enum
#define HTS_DEF_FWSTRUCT_struct_inthash_enum
typedef struct struct_inthash_enum struct_inthash_enum;
#endif

/** Enumeration. **/
struct struct_inthash_enum {
  inthash table;
  size_t index;
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create a new hashtable, with initial bucket size of 'size'.
 * If size is 0, use the default minimal bucket size.
 * Return a non-NULL pointer upon success.
 **/
inthash inthash_new(size_t size);

/**
 * Was the hashtable successfully created ?
 * Return non-zero value if the hashtable is valid.
 **/
int inthash_created(inthash hashtable);

/**
 * Delete a hashtable, freeing all entries.
 **/
void inthash_delete(inthash * hashtable);

/**
 * Return the number of items in the hashtable.
 **/
size_t inthash_nitems(inthash hashtable);

/**
 * Return the memory size taken by the hashtable.
 * (This does not take account of the possible memory taken by values)
 **/
size_t inthash_memory_size(inthash hashtable);

/**
 * If 'flag' is non-zero, calls inthash_value_set_value_handler() with
 * default system free() handler function, otherwise, free the value handlers.
 **/
void inthash_value_is_malloc(inthash hashtable, int flag);

/**
 * Set handlers for values.
 * free: this handler will be called when a value is to be removed from
 * the hashtable. if NULL, values won't be free'd.
 * arg: opaque custom argument to be used by functions.
 * Handler(s) MUST NOT be changed once elements have been added.
 **/
void inthash_value_set_value_handler(inthash hashtable,
                                     t_inthash_value_freehandler free,
                                     inthash_opaque arg);

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
void inthash_value_set_key_handler(inthash hashtable,
                                   t_inthash_duphandler dup,
                                   t_inthash_key_freehandler free,
                                   t_inthash_hasheshandler hash,
                                   t_inthash_cmphandler equals,
                                   inthash_opaque arg);

/**
 * Set assertion failure handler.
 * log: handler called upon serious programming error
 * fatal: handler called upon serious programming error
 **/
void inthash_set_assert_handler(inthash hashtable,
                                t_inthash_loghandler log,
                                t_inthash_asserthandler fatal,
                                inthash_opaque arg);

/**
 * Set pretty print loggers (debug). Both handlers must return a string
 * pointer which shall be valid until the next call. Both key and value
 * pointers shall be valid at the same time.
 * name: handler called to print the string representation of the name
 * value: handler called to print the string representation of the value
 **/
void inthash_set_print_handler(inthash hashtable,
                               t_inthash_printkeyhandler key,
                               t_inthash_printvaluehandler value,
                               inthash_opaque arg);

/**
 * Set the hashtable name, for degugging purpose.
 * name: the hashtable name (ASCII or UTF-8)
 */
void inthash_set_name(inthash hashtable, inthash_key_const name);

/**
 * Get the hashtable name, for degugging purpose.
 * Return NULL if no name was defined.
 **/
const char* inthash_get_name(inthash hashtable);

/**
 * Read an integer entry from the hashtable.
 * Return non-zero value upon success and sets intvalue.
 **/
int inthash_read(inthash hashtable, inthash_key_const name, intptr_t * intvalue);

/**
 * Same as inthash_read(), but return 0 is the value was zero.
 **/
int inthash_readptr(inthash hashtable, inthash_key_const name, intptr_t * intvalue);

/**
 * Return non-zero value if the given entry exists.
 **/
int inthash_exists(inthash hashtable, inthash_key_const name);

/**
 * Read an entry from the hashtable.
 * Return non-zero value upon success and sets value.
 **/
int inthash_read_value(inthash hashtable, inthash_key_const name,
                       inthash_value *value);

/**
 * Write an entry to the hashtable.
 * Return non-zero value if the entry was added, zero if it was replaced.
 **/
int inthash_write_value(inthash hashtable, inthash_key_const name,
                        inthash_value_const value);
/**
 * Read a pointer entry from the hashtable.
 * Return non-zero value upon success and sets value.
 **/
int inthash_read_pvoid(inthash hashtable, inthash_key_const name, void **value);

/**
 * Write a pointer entry to the hashtable.
 * Return non-zero value if the entry was added, zero if it was replaced.
 **/
int inthash_write_pvoid(inthash hashtable, inthash_key_const name, void *value);

/**
 * Alias to inthash_write_pvoid()
 **/
void inthash_add_pvoid(inthash hashtable, inthash_key_const name, void *value);

/**
 * Write an integer entry to the hashtable.
 * Return non-zero value if the entry was added, zero if it was replaced.
 **/
int inthash_write(inthash hashtable, inthash_key_const name, intptr_t value);

/**
 * Alias to inthash_write()
 **/
void inthash_add(inthash hashtable, inthash_key_const name, intptr_t value);

/**
 * Increment an entry value in the hashtable
 * (or create a new entry with value 1 if it does not yet exist)
 * Return non-zero value if the entry was added, zero if it was changed.
 **/
int inthash_inc(inthash hashtable, inthash_key_const name);

/**
 * Decrement an entry value in the hashtable 
 * (or create a new entry with value -1 if it does not yet exist)
 * Return non-zero value if the entry was added, zero if it was changed.
 **/
int inthash_dec(inthash hashtable, inthash_key_const name);

/**
 * Remove an entry from the hashtable
 * Return non-zero value if the entry was removed, zero otherwise.
 **/
int inthash_remove(inthash hashtable, inthash_key_const name);

/**
 * Return a new enumerator.
 * Note: deleting entries is safe while enumerating, but adding entries 
 * lead to undefined enumeration behavior (yet safe).
 **/
struct_inthash_enum inthash_enum_new(inthash hashtable);

/**
 * Enumerate the next entry.
 **/
inthash_item *inthash_enum_next(struct_inthash_enum * e);

/**
 * Compute a hash, given a string. This is the default function used for
 * hashing keys, which are by default strings.
 **/
inthash_hashkeys inthash_hash_string(const char *value);

/**
 * Set default global assertion failure handler.
 * The handler will be used if no specific handler was defined in the
 * hashtable itself.
 * log: handler called upon serious error log (opaque argument 
 * is the hashtable itself)
 * fatal: handler called upon serious programming error (opaque argument 
 * is the hashtable itself)
 **/
void inthash_set_global_assert_handler(t_inthash_loghandler log,
                                       t_inthash_asserthandler fatal);

#ifdef __cplusplus
}
#endif

#endif
