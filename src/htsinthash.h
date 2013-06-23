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

// inthash -- simple hash table, using a key (char[]) and a value (uintptr_t)

#ifndef HTSINTHASH_DEFH
#define HTSINTHASH_DEFH

/* Includes */
#ifdef _WIN32
#include <stddef.h>
#elif (defined(SOLARIS) || defined(sun) || defined(HAVE_INTTYPES_H) \
  || defined(BSD) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__FreeBSD_kernel__))
#include <inttypes.h>
#else
#include <stdint.h>
#endif

// value
typedef union inthash_value {
  uintptr_t intg;               /* integer value */
  void *ptr;                    /* ptr value */
} inthash_value;

#define INTHASH_VALUE_NULL { 0 }

// simple hash table for other routines
#ifndef HTS_DEF_FWSTRUCT_inthash_item
#define HTS_DEF_FWSTRUCT_inthash_item
typedef struct inthash_item inthash_item;
#endif
struct inthash_item {
  char *name;                   /* key (name) */
  inthash_value value;          /* value */
};

typedef inthash_item inthash_chain;

typedef void (*t_inthash_freehandler) (void *value);

/* inthash structure */
#ifndef HTS_DEF_FWSTRUCT_struct_inthash
#define HTS_DEF_FWSTRUCT_struct_inthash
typedef struct struct_inthash struct_inthash, *inthash;
#endif
#define STASH_SIZE 16
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

// enumeration
#ifndef HTS_DEF_FWSTRUCT_struct_inthash_enum
#define HTS_DEF_FWSTRUCT_struct_inthash_enum
typedef struct struct_inthash_enum struct_inthash_enum;
#endif
struct struct_inthash_enum {
  inthash table;
  size_t index;
};

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE

// main functions:

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
int inthash_remove(inthash hashtable, const char *name);        /* Remove an entry from the hashtable */

/* */
struct_inthash_enum inthash_enum_new(inthash hashtable);        /* Start a new enumerator */
inthash_item *inthash_enum_next(struct_inthash_enum * e);      /* Fetch an item in the enumerator */

/* End of hash functions: */
#endif

#endif
