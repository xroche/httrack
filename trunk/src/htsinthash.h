/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) Xavier Roche and other contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
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



#ifndef HTSINTHASH_DEFH
#define HTSINTHASH_DEFH 

// inthash -- simple hash table, using a key (char[]) and a value (ulong int)

// simple hash table for other routines
typedef struct inthash_chain {
  char* name;                    /* key (name) */
  union {
  unsigned long int intg;        /* integer value */
  void* ptr;                     /* ptr value */
  } value;
  struct inthash_chain* next;    /* next element */
} inthash_chain;

// structure behind inthash
typedef void (* t_inthash_freehandler)(void* value);
typedef struct {
  inthash_chain** hash;
  t_inthash_freehandler free_handler;
  unsigned int hash_size;
  unsigned short flag_valueismalloc;
} struct_inthash;

// main inthash type
typedef struct_inthash* inthash;

// subfunctions
unsigned long int inthash_key(char* value);
void inthash_init(inthash hashtable);
void inthash_delchain(inthash_chain* hash,t_inthash_freehandler free_handler);
void inthash_default_free_handler(void* value);

// main functions:


/* Hash functions: */
inthash inthash_new(int size);                                       /* Create a new hash table */
int     inthash_created(inthash hashtable);                          /* Test if the hash table was successfully created */
void    inthash_delete(inthash* hashtable);                          /* Delete an hash table */
void    inthash_value_is_malloc(inthash hashtable,int flag);         /* Is the 'value' member a value that needs to be free()'ed ? */
void    inthash_value_set_free_handler(inthash hashtable,             /* value free() handler (default one is 'free') */
                                    t_inthash_freehandler free_handler);
/* */
int     inthash_read(inthash hashtable,char* name,long int* value);    /* Read entry from the hash table */
int     inthash_readptr(inthash hashtable,char* name,long int* value); /* Same function, but returns 0 upon null ptr */
/* */
void    inthash_add(inthash hashtable,char* name,long int value);    /* Add entry in the hash table */
void*   inthash_addblk(inthash hashtable,char* name,int blksize);    /* Add entry in the hash table and set value to a new memory block */
int     inthash_write(inthash hashtable,char* name,long int value);  /* Overwrite/add entry in the hash table */
int     inthash_inc(inthash hashtable,char* name);                   /* Increment entry in the hash table */
/* End of hash functions: */


#endif
