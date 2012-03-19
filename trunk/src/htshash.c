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

#include "htshash.h"

/* specific definitions */
#include "htsbase.h"
#include "htsmd5.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* END specific definitions */

// GESTION DES TABLES DE HACHAGE
// Méthode à 2 clés (adr+fil), 2e cle facultative
// hash[no_enregistrement][pos]->hash est un index dans le tableau général liens
// #define HTS_HASH_SIZE 8191  (premier si possible!)
// type: numero enregistrement - 0 est case insensitive (sav) 1 (adr+fil) 2 (former_adr+former_fil)
#if HTS_HASH
// recherche dans la table selon nom1,nom2 et le no d'enregistrement
// retour: position ou -1 si non trouvé
int hash_read(hash_struct* hash,char* nom1,char* nom2,int type) {
  unsigned int cle;
  int pos; 
  // calculer la clé de recherche, non modulée
  if (type)
    cle = hash_cle(nom1,nom2);
  else
    cle = hash_cle(convtolower(nom1),nom2);         // case insensitive
  // la position se calcule en modulant
  pos = (int) (cle%HTS_HASH_SIZE);
  // entrée trouvée?
  if (hash->hash[type][pos] >= 0) {             // un enregistrement avec une telle clé existe..
    // tester table de raccourcis (hash)
    // pos est maintenant la position recherchée dans liens
    pos = hash->hash[type][pos];
    while (pos>=0) {              // parcourir la chaine
      switch (type) {
      case 0:         // sav
        if (strfield2(nom1,hash->liens[pos]->sav)) {  // case insensitive
#if DEBUG_HASH==2
          printf("hash: found shortcut at %d\n",pos);
#endif
          return pos;
        }
        break;
      case 1:         // adr+fil
        if ((strcmp(nom1,jump_identification(hash->liens[pos]->adr))==0) && (strcmp(nom2,hash->liens[pos]->fil)==0)) {
#if DEBUG_HASH==2
          printf("hash: found shortcut at %d\n",pos);
#endif
          return pos;
        }
        break;
      case 2:         // former_adr+former_fil
        if (hash->liens[pos]->former_adr)
        if ((strcmp(nom1,jump_identification(hash->liens[pos]->former_adr))==0) && (strcmp(nom2,hash->liens[pos]->former_fil)==0)) {
#if DEBUG_HASH==2
          printf("hash: found shortcut at %d\n",pos);
#endif
          return pos;
        }
        break;
      }
      // calculer prochaine position dans la chaine
      {
        int old=pos;
        pos=hash->liens[pos]->hash_next[type];   // sinon prochain dans la chaine
        if (old==pos)
          pos=-1;         // erreur de bouclage (ne devrait pas arriver)
      }
    }
    
    // Ok va falloir chercher alors..
    /*pos=hash->max_lien;    // commencer à max_lien
    switch (type) {
    case 0:         // sav
      while(pos>=0) {
        if (hash->liens[pos]->hash_sav == cle ) {
          if (strcmp(nom1,hash->liens[pos]->sav)==0) {
            hash->hash[type][(int) (cle%HTS_HASH_SIZE)] = pos;    // noter plus récent dans shortcut table
#if DEBUG_HASH==2
            printf("hash: found long search at %d\n",pos);
#endif
            return pos;
          }
        }
        pos--;
      }
      break;
    case 1:         // adr+fil
      while(pos>=0) {
        if (hash->liens[pos]->hash_adrfil == cle ) {
          if ((strcmp(nom1,hash->liens[pos]->adr)==0) && (strcmp(nom2,hash->liens[pos]->fil)==0)) {
            hash->hash[type][(int) (cle%HTS_HASH_SIZE)] = pos;    // noter plus récent dans shortcut table
#if DEBUG_HASH==2
            printf("hash: found long search at %d\n",pos);
#endif
            return pos;
          }
        }
        pos--;
      }
      break;
    case 2:         // former_adr+former_fil
      while(pos>=0) {
        if (hash->liens[pos]->hash_fadrfil == cle ) {
          if (hash->liens[pos]->former_adr)
            if ((strcmp(nom1,hash->liens[pos]->former_adr)==0) && (strcmp(nom2,hash->liens[pos]->former_fil)==0)) {
            hash->hash[type][(int) (cle%HTS_HASH_SIZE)] = pos;    // noter plus récent dans shortcut table
#if DEBUG_HASH==2
            printf("hash: found long search at %d\n",pos);
#endif
            return pos;
          }
        }
        pos--;
      }
    }*/
#if DEBUG_HASH==1
    printf("hash: not found after test %s%s\n",nom1,nom2);
#endif
    return -1;    // non trouvé
  } else {
#if DEBUG_HASH==2
    printf("hash: not found %s%s\n",nom1,nom2);
#endif
    return -1;    // non trouvé : clé non entrée (même une fois)
  }
}

// enregistrement lien lpos dans les 3 tables hash1..3
void hash_write(hash_struct* hash,int lpos) {
  unsigned int cle;
  int pos; 
  int* ptr;
  //
  if (hash->liens[lpos]) {                       // on sait jamais..
    hash->max_lien = max(hash->max_lien,lpos);
#if DEBUG_HASH
    hashnumber=hash->max_lien;
#endif
    // élément actuel sur -1 (fin de chaine)
    hash->liens[lpos]->hash_next[0]=hash->liens[lpos]->hash_next[1]=hash->liens[lpos]->hash_next[2]=-1;
    //
    cle = hash_cle(convtolower(hash->liens[lpos]->sav),"");    // CASE INSENSITIVE
    pos = (int) (cle%HTS_HASH_SIZE);
    ptr = hash_calc_chaine(hash,0,pos);         // calculer adresse chaine
    *ptr = lpos;                   // noter dernier enregistré
#if DEBUG_HASH==3
    printf("[%d",pos);
#endif
    //
    cle = hash_cle(jump_identification(hash->liens[lpos]->adr),hash->liens[lpos]->fil);
    pos = (int) (cle%HTS_HASH_SIZE);
    ptr = hash_calc_chaine(hash,1,pos);         // calculer adresse chaine
    *ptr = lpos;                   // noter dernier enregistré
#if DEBUG_HASH==3
    printf(",%d",pos);
#endif
    //
    if (hash->liens[lpos]->former_adr) {         // former_adr existe?
      cle = hash_cle(jump_identification(hash->liens[lpos]->former_adr),hash->liens[lpos]->former_fil);
      pos = (int) (cle%HTS_HASH_SIZE);
      ptr = hash_calc_chaine(hash,2,pos);         // calculer adresse chaine
      *ptr = lpos;                   // noter dernier enregistré
#if DEBUG_HASH==3
      printf(",%d",pos);
#endif
    }
#if DEBUG_HASH==3
    printf("] "); fflush(stdout);
#endif
  }
#if DEBUT_HASH
  else {
    printf("* hash_write=0!!\n");
    exit(1);
  }
#endif
  //
}

// calcul clé
// il n'y a pas de formule de hashage universelle, celle-ci semble acceptable..
unsigned long int hash_cle(char* nom1,char* nom2) {
  /*
  unsigned int sum=0;
  int i=0;
  while(*nom1) {
    sum += 1;
    sum += (unsigned int) *(nom1);
    sum *= (unsigned int) *(nom1++);
    sum += (unsigned int) i;
    i++;
  }
  while(*nom2) {
    sum += 1;
    sum += (unsigned int) *(nom2);
    sum *= (unsigned int) *(nom2++);
    sum += (unsigned int) i;
    i++;
  }
  */
  return md5sum32(nom1)
        +md5sum32(nom2);
}

// calcul de la position finale dans la chaine des elements ayant la même clé
int* hash_calc_chaine(hash_struct* hash,int type,int pos) {
#if DEBUG_HASH
  int count=0;
#endif
  if (hash->hash[type][pos] == -1)
    return &(hash->hash[type][pos]);    // premier élément dans la chaine
  pos=hash->hash[type][pos];
  while(hash->liens[pos]->hash_next[type] != -1) {
    pos = hash->liens[pos]->hash_next[type];
#if DEBUG_HASH
    count++;
#endif
  }
#if DEBUG_HASH
  count++;
  longest_hash[type]=max(longest_hash[type],count);
#endif
  return &(hash->liens[pos]->hash_next[type]);
}
#endif
// FIN GESTION DES TABLES DE HACHAGE












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
  *h=(inthash_chain*)calloc(1,
                              sizeof(inthash_chain)
                              +
                              strlen(name)+2
                            );
  if (*h) {
    (*h)->name=((char*)(*h)) + sizeof(inthash_chain);
    (*h)->next=NULL;
    strcpy((*h)->name,name);
    (*h)->value.intg=value;
  }
}

void* inthash_addblk(inthash hashtable,char* name,int blksize) {
  int pos = (inthash_key(name) % hashtable->hash_size);
  inthash_chain** h=&hashtable->hash[pos];

  while (*h)
    h=&((*h)->next);
  *h=(inthash_chain*)calloc(1,
                              sizeof(inthash_chain)
                              +
                              strlen(name)+2
                              +
                              blksize
                            );
  if (*h) {
    (*h)->name = ((char*)(*h)) + sizeof(inthash_chain);
    (*h)->next=NULL;
    strcpy((*h)->name,name);
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
      *value=h->value.intg;
      return 1;
    }
    h=h->next;
  }
  return 0;
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
        if (free_handler)
          free_handler((void*)hash->value.intg);
        else
          free((void*)hash->value.intg);
      }
      hash->value.intg=0;
    }
    free(hash);
  }
}

void inthash_default_free_handler(void* value) {
  if (value)
    free(value);
}

// --

inthash inthash_new(int size) {
  inthash hashtable=(inthash)calloc(1,sizeof(struct_inthash));
  if (hashtable) {
    hashtable->hash_size=0;
    hashtable->flag_valueismalloc=0;
    if ((hashtable->hash=(inthash_chain**)calloc(size,sizeof(inthash_chain*)))) {
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
      }
      free(*hashtable);
      *hashtable=NULL;
    }
  }
}


