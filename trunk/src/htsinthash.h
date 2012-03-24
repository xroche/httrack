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
  uintptr_t intg;                /* integer value */
  void* ptr;                     /* ptr value */
} inthash_value;

#define INTHASH_VALUE_NULL { 0 }

// simple hash table for other routines
#ifndef HTS_DEF_FWSTRUCT_inthash_chain
#define HTS_DEF_FWSTRUCT_inthash_chain
typedef struct inthash_chain inthash_chain;
#endif
struct inthash_chain {
  char* name;                    /* key (name) */
  inthash_value value;           /* value */
  struct inthash_chain* next;    /* next element */
};

typedef void (* t_inthash_freehandler)(void* value);

/* inthash structure */
#ifndef HTS_DEF_FWSTRUCT_struct_inthash
#define HTS_DEF_FWSTRUCT_struct_inthash
typedef struct struct_inthash struct_inthash, *inthash;
#endif
struct struct_inthash {
  inthash_chain** hash;
  unsigned int nitems;
  t_inthash_freehandler free_handler;
  unsigned int hash_size;
  unsigned short flag_valueismalloc;
};

// enumeration
#ifndef HTS_DEF_FWSTRUCT_struct_inthash_enum
#define HTS_DEF_FWSTRUCT_struct_inthash_enum
typedef struct struct_inthash_enum struct_inthash_enum;
#endif
struct struct_inthash_enum {
  inthash table;
  int index;
  inthash_chain* item;
};

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE

// main functions:

/* Hash functions: */
inthash inthash_new(int size);                                            /* Create a new hash table */
int     inthash_created(inthash hashtable);                               /* Test if the hash table was successfully created */
unsigned int inthash_nitems(inthash hashtable);                           /* Number of items */
void    inthash_delete(inthash* hashtable);                               /* Delete an hash table */
void    inthash_value_is_malloc(inthash hashtable,int flag);              /* Is the 'value' member a value that needs to be free()'ed ? */
void    inthash_value_set_free_handler(inthash hashtable,                 /* value free() handler (default one is 'free') */
                                       t_inthash_freehandler free_handler);
/* */
int     inthash_read(inthash hashtable,const char* name,intptr_t* intvalue);    /* Read entry from the hash table */
int     inthash_readptr(inthash hashtable,const char* name,intptr_t* intvalue); /* Same function, but returns 0 upon null ptr */
int     inthash_exists(inthash hashtable, const char* name);                    /* Is the key existing ? */
/* */
int     inthash_read_value(inthash hashtable,const char* name,inthash_value* value);
int     inthash_write_value(inthash hashtable,const char* name,inthash_value value);
void    inthash_add_value(inthash hashtable, const char* name, inthash_value value);
/* */
int     inthash_read_pvoid(inthash hashtable,const char* name, void** value);
int     inthash_write_pvoid(inthash hashtable,const char* name, void* value);
void    inthash_add_pvoid(inthash hashtable, const char* name, void* value);
/* */
void    inthash_add(inthash hashtable,const char* name,intptr_t value);    /* Add entry in the hash table */
void*   inthash_addblk(inthash hashtable,const char* name,int blksize);    /* Add entry in the hash table and set value to a new memory block */
int     inthash_write(inthash hashtable,const char* name,intptr_t value);  /* Overwrite/add entry in the hash table */
int     inthash_inc(inthash hashtable,const char* name);                   /* Increment entry in the hash table */
int     inthash_remove(inthash hashtable,const char* name);                /* Remove an entry from the hashtable */
/* */
struct_inthash_enum inthash_enum_new(inthash hashtable);             /* Start a new enumerator */
inthash_chain* inthash_enum_next(struct_inthash_enum* e);           /* Fetch an item in the enumerator */
/* End of hash functions: */
#endif

#endif
