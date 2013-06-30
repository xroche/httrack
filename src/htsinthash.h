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
/*       hash table system (fast index)                         */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

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

/** Value. **/
typedef union inthash_value {
  /** Integer value. **/
  intptr_t intg;

  /** Unsigned integer value. **/
  uintptr_t uintg;

  /** Pointer value. **/
  void *ptr;
} inthash_value;

/** NULL Value. **/
#define INTHASH_VALUE_NULL { 0 }

#ifndef HTS_DEF_FWSTRUCT_inthash_item
#define HTS_DEF_FWSTRUCT_inthash_item
typedef struct inthash_item inthash_item;
#endif

/** Hash key (32-bit) **/
typedef uint32_t inthash_key;

/** Pair of hashes **/
typedef struct inthash_keys {
  inthash_key hash1;
  inthash_key hash2;
} inthash_keys;

/** NULL pair of hashes. **/
#define INTHASH_KEYS_NULL { 0, 0 }

/** Item holding a value. **/
struct inthash_item {
  /** Key. **/
  char *name;

  /** Value. **/
  inthash_value value;

  /** Hashes of the key. **/
  inthash_keys hashes;
};

/** Alias for legacy code. **/
typedef inthash_item inthash_chain;

/** Value free handler **/
typedef void (*t_inthash_freehandler) (void *arg, void *value);

/** Name dup handler. **/
typedef char* (*t_inthash_duphandler) (void *arg, const char *name);

/** Hash computation handler. **/
typedef inthash_keys (*t_inthash_hasheshandler)(void *arg, const char *value);

/**
 * Value comparison handler (returns non-zero value if strings are equal).
 **/
typedef int (*t_inthash_cmphandler)(void *arg, const char *a, const char *b);

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

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE

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
                                     t_inthash_freehandler free,
                                     void *arg);

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
                                   t_inthash_freehandler free,
                                   t_inthash_hasheshandler hash,
                                   t_inthash_cmphandler equals,
                                   void *arg);

/**
 * Read an integer entry from the hashtable.
 * Return non-zero value upon success and sets intvalue.
 **/
int inthash_read(inthash hashtable, const char *name, intptr_t * intvalue);

/**
 * Same as inthash_read(), but return 0 is the value was zero.
 **/
int inthash_readptr(inthash hashtable, const char *name, intptr_t * intvalue);

/**
 * Return non-zero value if the given entry exists.
 **/
int inthash_exists(inthash hashtable, const char *name);

/**
 * Read an entry from the hashtable.
 * Return non-zero value upon success and sets value.
 **/
int inthash_read_value(inthash hashtable, const char *name,
                       inthash_value * value);

/**
 * Write an entry to the hashtable.
 * Return non-zero value if the entry was added, zero if it was replaced.
 **/
int inthash_write_value(inthash hashtable, const char *name,
                        inthash_value value);
/**
 * Read a pointer entry from the hashtable.
 * Return non-zero value upon success and sets value.
 **/
int inthash_read_pvoid(inthash hashtable, const char *name, void **value);

/**
 * Write a pointer entry to the hashtable.
 * Return non-zero value if the entry was added, zero if it was replaced.
 **/
int inthash_write_pvoid(inthash hashtable, const char *name, void *value);

/**
 * Alias to inthash_write_pvoid()
 **/
void inthash_add_pvoid(inthash hashtable, const char *name, void *value);

/**
 * Write an integer entry to the hashtable.
 * Return non-zero value if the entry was added, zero if it was replaced.
 **/
int inthash_write(inthash hashtable, const char *name, intptr_t value);

/**
 * Alias to inthash_write()
 **/
void inthash_add(inthash hashtable, const char *name, intptr_t value);

/**
 * Increment an entry value in the hashtable
 * (or create a new entry with value 1 if it does not yet exist)
 * Return non-zero value if the entry was added, zero if it was changed.
 **/
int inthash_inc(inthash hashtable, const char *name);

/**
 * Decrement an entry value in the hashtable 
 * (or create a new entry with value -1 if it does not yet exist)
 * Return non-zero value if the entry was added, zero if it was changed.
 **/
int inthash_dec(inthash hashtable, const char *name);

/**
 * Remove an entry from the hashtable
 * Return non-zero value if the entry was removed, zero otherwise.
 **/
int inthash_remove(inthash hashtable, const char *name);

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
 * Compute a hash, given a string value.
 **/
inthash_keys inthash_hash_value(const char *value);

#endif

#endif
