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

/* Internal engine bytecode */
#define HTS_INTERNAL_BYTECODE

#include "htsinthash.h"

/* specific definitions */
#include "htsbase.h"
#include "htsmd5.h"
/* END specific definitions */

/* Specific macros */
#ifdef NO_MALLOCT
#undef malloct
#undef freet
#undef calloct
#undef strcpybuff
#endif
#ifndef malloct
#define malloct malloc
#define freet free
#define calloct calloc
#define strcpybuff strcpy
#endif

// static functions

static void inthash_delchain(inthash_chain* hash,t_inthash_freehandler free_handler);
static void inthash_default_free_handler(void* value);
static unsigned long int inthash_key(const char* value);
static void inthash_init(inthash hashtable);


// inthash -- simple hash table, using a key (char[]) and a value (ulong int)

static unsigned long int inthash_key(const char* value) {
  return md5sum32(value);
}

int inthash_read_pvoid(inthash hashtable,const char* name, void** pvalue) {
  inthash_value value = INTHASH_VALUE_NULL;
  int ret = inthash_read_value(hashtable, name, (pvalue != NULL) ? &value : NULL);
  if (pvalue != NULL)
    *pvalue = value.ptr;
  return ret;
}

int inthash_write_pvoid(inthash hashtable,const char* name, void* pvalue) {
  inthash_value value = INTHASH_VALUE_NULL;
  value.ptr = pvalue;
  return inthash_write_value(hashtable, name, value);
}

void inthash_add_pvoid(inthash hashtable, const char* name, void* pvalue) {
  inthash_value value = INTHASH_VALUE_NULL;
  value.ptr = pvalue;
  inthash_add_value(hashtable, name, value);
}

// Check for duplicate entry (==1 : added)
int inthash_write(inthash hashtable,const char* name,intptr_t intvalue) {
  inthash_value value = INTHASH_VALUE_NULL;
  value.intg = intvalue;
  return inthash_write_value(hashtable, name, value);
}

int inthash_write_value(inthash hashtable,const char* name,inthash_value value) {
  int pos = (inthash_key(name) % hashtable->hash_size);
  inthash_chain* h=hashtable->hash[pos];
  while (h) {
    if (strcmp(h->name,name)==0) {
      /* Delete element */
      if (hashtable->flag_valueismalloc) {
        void* ptr = h->value.ptr;
        if (ptr != NULL) {
          if (hashtable->free_handler)
            hashtable->free_handler(ptr);
          else
            freet(ptr);
        }
      }
      /* Insert */
      h->value=value;
      return 0;
    }
    h=h->next;
  }
  // Not found, add it!
  inthash_add_value(hashtable,name,value);
  return 1;
}

// Increment pos value, create one if necessary (=0)
// (==1 : created)
int inthash_inc(inthash hashtable,const char* name) {
  intptr_t value=0;
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
void inthash_add(inthash hashtable, const char* name, intptr_t intvalue) {
  inthash_value value = INTHASH_VALUE_NULL;
  memset(&value, 0, sizeof(value));
  value.intg = intvalue;
  inthash_add_value(hashtable, name, value);
}

void inthash_add_value(inthash hashtable, const char* name, inthash_value value) {
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
    (*h)->value=value;
    hashtable->nitems++;
  }
}

void* inthash_addblk(inthash hashtable,const char* name,int blksize) {
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
    (*h)->value.ptr = (void*) ( ((char*)(*h)) + sizeof(inthash_chain) + strlen(name) + 2 );
    hashtable->nitems++;
    return (*h)->value.ptr;
  }
  return NULL;
}

int inthash_read(inthash hashtable,const char* name,intptr_t* intvalue) {
  inthash_value value = INTHASH_VALUE_NULL;
  int ret = inthash_read_value(hashtable, name, (intvalue != NULL) ? &value : NULL);
  if (intvalue != NULL)
    *intvalue = value.intg;
  return ret;
}

int inthash_read_value(inthash hashtable,const char* name,inthash_value* value) {
  int pos = (inthash_key(name) % hashtable->hash_size);
  inthash_chain* h=hashtable->hash[pos];
  while (h) {
    if (strcmp(h->name,name)==0) {
      if (value != NULL)
        *value=h->value;
      return 1;
    }
    h=h->next;
  }
  return 0;
}

int inthash_exists(inthash hashtable, const char* name) {
  return inthash_read_value(hashtable, name, NULL);
}

int inthash_remove(inthash hashtable,const char* name) {
  int pos = (inthash_key(name) % hashtable->hash_size);
  inthash_chain** h=&hashtable->hash[pos];
  t_inthash_freehandler free_handler=NULL;
  if ( hashtable->flag_valueismalloc ) {
    if ( hashtable->free_handler )
      free_handler=hashtable->free_handler;
    else
      free_handler=inthash_default_free_handler;
  }
  while (*h) {
    if (strcmp((*h)->name,name)==0) {
      inthash_chain* next;
      if (free_handler) {
        if ((*h)->value.ptr) {
          void* ptr = (*h)->value.ptr;
          if (free_handler)
            free_handler(ptr);
          else
            freet(ptr);
          (*h)->value.ptr=0;
        }
      }
      next=(*h)->next;
      freet(*h);
      *h=next;
      hashtable->nitems--;
      return 1;
    }
    h=&((*h)->next);
  }
  return 0;
}

int inthash_readptr(inthash hashtable,const char* name,intptr_t* value) {
  int ret;
  *value = 0;
  ret = inthash_read(hashtable, name, value);
  if (*value == 0)
    ret = 0;
  return ret;
}

static void inthash_init(inthash hashtable) {
  unsigned int i;
  for(i=0;i<hashtable->hash_size;i++) {
    hashtable->hash[i]=NULL;
  }
}

static void inthash_delchain(inthash_chain* hash,t_inthash_freehandler free_handler) {
  while(hash != NULL) {
    inthash_chain* next=hash->next;
    if (free_handler) {     // pos is a malloc() block, delete it!
      if (hash->value.ptr) {
        void* ptr = hash->value.ptr;
        if (free_handler)
          free_handler(ptr);
        else
          freet(ptr);
        hash->value.ptr=0;
      }
    }
    freet(hash);
    hash=next;
  }
}

static void inthash_default_free_handler(void* value) {
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
    hashtable->nitems = 0;
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

unsigned int inthash_nitems(inthash hashtable) {
  if (hashtable!= NULL)
    return hashtable->nitems;
  return 0;
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

// Enumerators

struct_inthash_enum inthash_enum_new(inthash hashtable) {
  struct_inthash_enum e;
  memset(&e, 0, sizeof(e));
  e.index = 0;
  e.item = NULL;
  e.table = hashtable;
  return e;
}

inthash_chain* inthash_enum_next(struct_inthash_enum* e) {
  inthash_chain* item = NULL;
  if (e != NULL) {
    while(e->item == NULL && e->index < (int) e->table->hash_size) {
      e->item = e->table->hash[e->index];
      e->index++;
    }
    if (e->item != NULL) {
      item = e->item;
      e->item = e->item->next;
    }
  }
  return item;
}
