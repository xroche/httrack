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

/* Internal engine bytecode */
#define HTS_INTERNAL_BYTECODE

#include "htsinthash.h"

/* specific definitions */
#include "htsbase.h"
#include "htsglobal.h"
#include "htsmd5.h"
/* END specific definitions */

/* Specific macros */
#ifndef malloct
#define malloct malloc
#define freet free
#define calloct calloc
#define strcpybuff strcpy
#endif

// inthash -- simple hash table, using a key (char[]) and a value (ulong int)

unsigned long int inthash_key(char* value) {
  return md5sum32(value);
}

// Check for duplicate entry (==1 : added)
int inthash_write(inthash hashtable,char* name,long int value) {
  int pos = (inthash_key(name) % hashtable->hash_size);
  inthash_chain* h=hashtable->hash[pos];
  while (h) {
    if (strcmp(h->name,name)==0) {
      /* Delete element */
      if (hashtable->flag_valueismalloc) {
        void* ptr = (void*)h->value.intg;
        if (ptr != NULL) {
          if (hashtable->free_handler)
            hashtable->free_handler(ptr);
          else
            freet(ptr);
        }
      }
      /* Insert */
      h->value.intg=value;
      return 0;
    }
    h=h->next;
  }
  // Not found, add it!
  inthash_add(hashtable,name,value);
  return 1;
}

// Increment pos value, create one if necessary (=0)
// (==1 : created)
int inthash_inc(inthash hashtable,char* name) {
  long int value=0;
  int r=0;
  if (inthash_read(hashtable,name,&value)) {
    value++;
  }
  else {    /* create new value */
    value=0;
    r=1;
  }
  inthash_write(hashtable,name,value);
  return (r);
}


// Does not check for duplicate entry
void inthash_add(inthash hashtable,char* name,long int value) {
  int pos = (inthash_key(name) % hashtable->hash_size);
  inthash_chain** h=&hashtable->hash[pos];

  while (*h)
    h=&((*h)->next);
  *h=(inthash_chain*)calloct(1,
                              sizeof(inthash_chain)
                              +
                              strlen(name)+2
                            );
  if (*h) {
    (*h)->name=((char*)(*h)) + sizeof(inthash_chain);
    (*h)->next=NULL;
    strcpybuff((*h)->name,name);
    (*h)->value.intg=value;
  }
}

void* inthash_addblk(inthash hashtable,char* name,int blksize) {
  int pos = (inthash_key(name) % hashtable->hash_size);
  inthash_chain** h=&hashtable->hash[pos];

  while (*h)
    h=&((*h)->next);
  *h=(inthash_chain*)calloct(1,
                              sizeof(inthash_chain)
                              +
                              strlen(name)+2
                              +
                              blksize
                            );
  if (*h) {
    (*h)->name = ((char*)(*h)) + sizeof(inthash_chain);
    (*h)->next=NULL;
    strcpybuff((*h)->name,name);
    (*h)->value.intg = (unsigned long) (char*) ((char*)(*h)) + sizeof(inthash_chain) + strlen(name) + 2;
    return (void*)(*h)->value.intg;
  }
  return NULL;
}

int inthash_read(inthash hashtable,char* name,long int* value) {
  int pos = (inthash_key(name) % hashtable->hash_size);
  inthash_chain* h=hashtable->hash[pos];
  while (h) {
    if (strcmp(h->name,name)==0) {
      if (value != NULL)
        *value=h->value.intg;
      return 1;
    }
    h=h->next;
  }
  return 0;
}

int inthash_readptr(inthash hashtable,char* name,long int* value) {
  int ret;
  *value = 0;
  ret = inthash_read(hashtable, name, value);
  if (*value == 0)
    ret = 0;
  return ret;
}

void inthash_init(inthash hashtable) {
  unsigned int i;
  for(i=0;i<hashtable->hash_size;i++) {
    hashtable->hash[i]=NULL;
  }
}

void inthash_delchain(inthash_chain* hash,t_inthash_freehandler free_handler) {
  if (hash) {
    inthash_delchain(hash->next,free_handler);
    if (free_handler) {     // pos is a malloc() block, delete it!
      if (hash->value.intg) {
        void* ptr = (void*)hash->value.intg;
        if (free_handler)
          free_handler(ptr);
        else
          freet(ptr);
        hash->value.intg=0;
      }
    }
    freet(hash);
  }
}

void inthash_default_free_handler(void* value) {
  if (value)
    freet(value);
}

// --

inthash inthash_new(int size) {
  inthash hashtable=(inthash)calloct(1,sizeof(struct_inthash));
  if (hashtable) {
    hashtable->hash_size=0;
    hashtable->flag_valueismalloc=0;
    if ((hashtable->hash=(inthash_chain**)calloct(size,sizeof(inthash_chain*)))) {
      hashtable->hash_size=size;
      inthash_init(hashtable);
    }
  }
  return hashtable;
}

int inthash_created(inthash hashtable) {
  if (hashtable)
    if (hashtable->hash)
      return 1;
  return 0;
}

void inthash_value_is_malloc(inthash hashtable,int flag) {
  hashtable->flag_valueismalloc=flag;
}

void inthash_value_set_free_handler(inthash hashtable, t_inthash_freehandler free_handler) {
  hashtable->free_handler = free_handler;
}

void inthash_delete(inthash* hashtable) {
  if (hashtable) {
    if (*hashtable) {
      if ((*hashtable)->hash) {
        unsigned int i;
        t_inthash_freehandler free_handler=NULL;
        if ( (*hashtable)->flag_valueismalloc ) {
          if ( (*hashtable)->free_handler )
            free_handler=(*hashtable)->free_handler;
          else
            free_handler=inthash_default_free_handler;
        }
        for(i=0;i<(*hashtable)->hash_size;i++) {
          inthash_delchain((*hashtable)->hash[i],(*hashtable)->free_handler);
          (*hashtable)->hash[i]=NULL;
        }
        freet((*hashtable)->hash);
        (*hashtable)->hash = NULL;
      }
      freet(*hashtable);
      *hashtable=NULL;
    }
  }
}
