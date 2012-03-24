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

#include "htshash.h"

/* specific definitions */
#include "htsbase.h"
#include "htsopt.h"
#include "htsglobal.h"
#include "htsmd5.h"
#include "htscore.h"
/* END specific definitions */

/* Specific macros */
#ifndef malloct
#define malloct malloc
#define freet free
#define calloct calloc
#define strcpybuff strcpy
#endif

// GESTION DES TABLES DE HACHAGE
// Méthode à 2 clés (adr+fil), 2e cle facultative
// hash[no_enregistrement][pos]->hash est un index dans le tableau général liens
// #define HTS_HASH_SIZE 8191  (premier si possible!)
// type: numero enregistrement - 0 est case insensitive (sav) 1 (adr+fil) 2 (former_adr+former_fil)
// recherche dans la table selon nom1,nom2 et le no d'enregistrement
// retour: position ou -1 si non trouvé
int hash_read(const hash_struct* hash,char* nom1,char* nom2,int type,int normalized) {
  char BIGSTK normfil_[HTS_URLMAXSIZE*2];
	char catbuff[CATBUFF_SIZE];
  char* normfil;
  char* normadr;
  unsigned int cle;
  int pos; 
  // calculer la clé de recherche, non modulée
  if (type)
    cle = hash_cle(nom1,nom2);
  else
    cle = hash_cle(convtolower(catbuff,nom1),nom2);         // case insensitive
  // la position se calcule en modulant
  pos = (int) (cle%HTS_HASH_SIZE);
  // entrée trouvée?
  if (hash->hash[type][pos] >= 0) {             // un ou plusieurs enregistrement(s) avec une telle clé existe..
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
        {
          if (!normalized)
            normfil=hash->liens[pos]->fil;
          else
            normfil=fil_normalized(hash->liens[pos]->fil,normfil_);
          if (!normalized)
            normadr = jump_identification(hash->liens[pos]->adr);
          else
            normadr = jump_normalized(hash->liens[pos]->adr);
          if ((strfield2(nom1,normadr)!=0) && (strcmp(nom2,normfil)==0)) {
#if DEBUG_HASH==2
            printf("hash: found shortcut at %d\n",pos);
#endif
            return pos;
          }
        }
        break;
      case 2:         // former_adr+former_fil
        {
          if (hash->liens[pos]->former_adr) {
            if (!normalized)
              normfil=hash->liens[pos]->former_fil;
            else
              normfil=fil_normalized(hash->liens[pos]->former_fil,normfil_);
            if (!normalized)
              normadr = jump_identification(hash->liens[pos]->former_adr);
            else
              normadr = jump_normalized(hash->liens[pos]->former_adr);
            
            if ((strfield2(nom1,normadr)!=0) && (strcmp(nom2,normfil)==0)) {
#if DEBUG_HASH==2
              printf("hash: found shortcut at %d\n",pos);
#endif
              return pos;
            }
          }
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
void hash_write(hash_struct* hash,int lpos,int normalized) {
  char BIGSTK normfil_[HTS_URLMAXSIZE*2];
	char catbuff[CATBUFF_SIZE];
  char* normfil;
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
    cle = hash_cle(convtolower(catbuff,hash->liens[lpos]->sav),"");    // CASE INSENSITIVE
    pos = (int) (cle%HTS_HASH_SIZE);
    ptr = hash_calc_chaine(hash,0,pos);         // calculer adresse chaine
    *ptr = lpos;                   // noter dernier enregistré
#if DEBUG_HASH==3
    printf("[%d",pos);
#endif
    //
    if (!normalized)
      normfil=hash->liens[lpos]->fil;
    else
      normfil=fil_normalized(hash->liens[lpos]->fil,normfil_);
    if (!normalized)
      cle = hash_cle(jump_identification(hash->liens[lpos]->adr),normfil);
    else
      cle = hash_cle(jump_normalized(hash->liens[lpos]->adr),normfil);
    pos = (int) (cle%HTS_HASH_SIZE);
    ptr = hash_calc_chaine(hash,1,pos);         // calculer adresse chaine
    *ptr = lpos;                   // noter dernier enregistré
#if DEBUG_HASH==3
    printf(",%d",pos);
#endif
    //
    if (hash->liens[lpos]->former_adr) {         // former_adr existe?
      if (!normalized)
        normfil=hash->liens[lpos]->former_fil;
      else
        normfil=fil_normalized(hash->liens[lpos]->former_fil,normfil_);
      if (!normalized)
        cle = hash_cle(jump_identification(hash->liens[lpos]->former_adr),normfil);
      else
        cle = hash_cle(jump_normalized(hash->liens[lpos]->former_adr),normfil);
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
    abortLogFmt("unexpected error in hash_write (pos=%d)" _ pos);
    abort();
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
// FIN GESTION DES TABLES DE HACHAGE

