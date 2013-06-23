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

// retour: position ou -1 si non trouvé
int hash_read(const hash_struct * hash, const char *nom1, const char *nom2,
              int type, int normalized) {
  char BIGSTK normfil_[HTS_URLMAXSIZE * 2];
  char catbuff[CATBUFF_SIZE];
  const char *normfil;
  intptr_t intvalue;
  char *name;

  /* dispatch type */
  switch(type) {
  case 0:
    /* first entry: destination filename (lowercased) */
    assertf(nom2 == NULL || *nom2 == '\0');
    name = convtolower(catbuff, nom1);
    break;
  case 1:
  case 2:
    /* second and third entries: URL address and path */
    if (!normalized)
      normfil = nom1;
    else
      normfil = fil_normalized(nom1, normfil_);
    if (!normalized) {
      strcpybuff(catbuff, jump_identification(nom2));
    } else {
      strcpybuff(catbuff, jump_normalized(nom2));
    }
    strcatbuff(catbuff, normfil);
    name = catbuff;
    break;
  default:
    assertf(! "unexpected case");
    break;
  }

  /* read */
  switch(type) {
  case 0:
    if (inthash_read(hash->sav, name, &intvalue)) {
      return (int) intvalue;
    } else {
      return -1;
    }
    break;
  case 1:
    if (inthash_read(hash->adrfil, name, &intvalue)) {
      return (int) intvalue;
    } else {
      return -1;
    }
    break;
  case 2:
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
  name = convtolower(catbuff, hash->liens[lpos]->sav);
  inthash_write(hash->sav, name, lpos);

  /* second entry: URL address and path */
  if (!normalized)
    normfil = hash->liens[lpos]->fil;
  else
    normfil = fil_normalized(hash->liens[lpos]->fil, normfil_);
  if (!normalized) {
    strcpybuff(catbuff, jump_identification(hash->liens[lpos]->adr));
  } else {
    strcpybuff(catbuff, jump_normalized(hash->liens[lpos]->adr));
  }
  strcatbuff(catbuff, normfil);
  inthash_write(hash->adrfil, name, lpos);

  /* third entry: URL address and path before redirect */
  if (hash->liens[lpos]->former_adr) {        // former_adr existe?
    if (!normalized) {
      normfil = hash->liens[lpos]->former_fil;
    } else {
      normfil = fil_normalized(hash->liens[lpos]->former_fil, normfil_);
    }
    if (!normalized)
      strcpybuff(catbuff, jump_identification(hash->liens[lpos]->former_adr));
    else
      strcpybuff(catbuff, jump_normalized(hash->liens[lpos]->former_adr));
    strcatbuff(catbuff, normfil);
    inthash_write(hash->former_adrfil, name, lpos);
  }
}
