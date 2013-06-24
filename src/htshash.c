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
#include "htsinthash.h"
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

void hash_init(hash_struct * hash) {
  hash->sav = inthash_new(0);
  hash->adrfil = inthash_new(0);
  hash->former_adrfil = inthash_new(0);
}

static char * normalize_key(const char *nom1, const char *nom2,
                            hash_struct_type type, int normalized, 
                            char *normfil_, char *catbuff) {
  /* dispatch type */
  const char *normfil;
  switch(type) {
  case HASH_STRUCT_FILENAME:
    /* first entry: destination filename (lowercased) */
    assertf(nom2 == NULL || *nom2 == '\0');
    return convtolower(catbuff, nom1);
    break;
  case HASH_STRUCT_ADR_PATH:
  case HASH_STRUCT_ORIGINAL_ADR_PATH:
    /* second and third entries: URL address and path */
    if (!normalized)
      normfil = nom2;
    else
      normfil = fil_normalized(nom2, normfil_);
    if (!normalized) {
      strcpybuff(catbuff, jump_identification(nom1));
    } else {
      strcpybuff(catbuff, jump_normalized(nom1));
    }
    strcatbuff(catbuff, normfil);
    return catbuff;
    break;
  default:
    assertf(! "unexpected case");
    return NULL;
    break;
  }
}

// retour: position ou -1 si non trouvé
int hash_read(const hash_struct * hash, const char *nom1, const char *nom2,
              hash_struct_type type, int normalized) {
  char BIGSTK normfil_[HTS_URLMAXSIZE * 2];
  char catbuff[CATBUFF_SIZE];
  intptr_t intvalue;
  char *const name = normalize_key(nom1, nom2, type, normalized, 
    normfil_, catbuff);

  /* read */
  switch(type) {
  case HASH_STRUCT_FILENAME:
    if (inthash_read(hash->sav, name, &intvalue)) {
      return (int) intvalue;
    } else {
      return -1;
    }
    break;
  case HASH_STRUCT_ADR_PATH:
    if (inthash_read(hash->adrfil, name, &intvalue)) {
      return (int) intvalue;
    } else {
      return -1;
    }
    break;
  case HASH_STRUCT_ORIGINAL_ADR_PATH:
    if (inthash_read(hash->former_adrfil, name, &intvalue)) {
      return (int) intvalue;
    } else {
      return -1;
    }
    break;
  default:
    assertf(! "unexpected case");
    return -1;
    break;
  }
}

// enregistrement lien lpos dans les 3 tables hash1..3
void hash_write(hash_struct * hash, int lpos, int normalized) {
  char BIGSTK normfil_[HTS_URLMAXSIZE * 2];
  char catbuff[CATBUFF_SIZE];
  const char *normfil;
  const char *name;

  /* first entry: destination filename (lowercased) */
  name = normalize_key(hash->liens[lpos]->sav, NULL, HASH_STRUCT_FILENAME,
    normalized, normfil_, catbuff);
  inthash_write(hash->sav, name, lpos);

  /* second entry: URL address and path */
  name = normalize_key(hash->liens[lpos]->adr, hash->liens[lpos]->fil, 
    HASH_STRUCT_ADR_PATH, normalized, normfil_, catbuff);
  inthash_write(hash->adrfil, name, lpos);

  /* third entry: URL address and path before redirect */
  if (hash->liens[lpos]->former_adr) {        // former_adr existe?
    name = normalize_key(hash->liens[lpos]->former_adr, 
      hash->liens[lpos]->former_fil, HASH_STRUCT_ORIGINAL_ADR_PATH,
      normalized, normfil_, catbuff);
    inthash_write(hash->former_adrfil, name, lpos);
  }
}
