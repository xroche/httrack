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
 * with a FNV hash function, with one additional auxiliary hash function.
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
  uintptr_t intg;

  /** Pointer value. **/
  void *ptr;
} inthash_value;

/** NULL Value. **/
#define INTHASH_VALUE_NULL { 0 }

#ifndef HTS_DEF_FWSTRUCT_inthash_item
#define HTS_DEF_FWSTRUCT_inthash_item
typedef struct inthash_item inthash_item;
#endif

/* Hash key (32-bit) */
typedef uint32_t inthash_key;

/* Pair of hashes */
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

/* Alias for legacy code */
typedef inthash_item inthash_chain;

/* Free handler */
typedef void (*t_inthash_freehandler) (void *value);

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

/* Hash functions: */
inthash inthash_new(size_t size);  /* Create a new hash table */
int inthash_created(inthash hashtable); /* Test if the hash table was successfully created */
size_t inthash_nitems(inthash hashtable); /* Number of items */
void inthash_delete(inthash * hashtable);       /* Delete an hash table */
void inthash_value_is_malloc(inthash hashtable, int flag);      /* Is the 'value' member a value that needs to be free()'ed ? */
void inthash_value_set_free_handler(inthash hashtable,  /* value free() handler (default one is 'free') */
                                    t_inthash_freehandler free_handler);

/* */
int inthash_read(inthash hashtable, const char *name, intptr_t * intvalue);     /* Read entry from the hash table */
int inthash_readptr(inthash hashtable, const char *name, intptr_t * intvalue);  /* Same function, but returns 0 upon null ptr */
int inthash_exists(inthash hashtable, const char *name);        /* Is the key existing ? */

/* */
int inthash_read_value(inthash hashtable, const char *name,
                       inthash_value * value);
int inthash_write_value(inthash hashtable, const char *name,
                        inthash_value value);
/* */
int inthash_read_pvoid(inthash hashtable, const char *name, void **value);
int inthash_write_pvoid(inthash hashtable, const char *name, void *value);
void inthash_add_pvoid(inthash hashtable, const char *name, void *value);

/* */
void inthash_add(inthash hashtable, const char *name, intptr_t value);  /* Add entry in the hash table */
int inthash_write(inthash hashtable, const char *name, intptr_t value); /* Overwrite/add entry in the hash table */
int inthash_inc(inthash hashtable, const char *name);   /* Increment entry in the hash table */
int inthash_dec(inthash hashtable, const char *name);   /* Decrement entry in the hash table */
int inthash_remove(inthash hashtable, const char *name);        /* Remove an entry from the hashtable */

/* */
struct_inthash_enum inthash_enum_new(inthash hashtable);        /* Start a new enumerator */
inthash_item *inthash_enum_next(struct_inthash_enum * e);      /* Fetch an item in the enumerator */

/* End of hash functions: */
#endif

#endif
