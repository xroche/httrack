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
// type: numero enregistrement - 0 est case insensitive (sav) 1 (adr+fil) 2 (former_adr+former_fil)
// recherche dans la table selon nom1,nom2 et le no d'enregistrement

/* Key free handler (NOOP) ; addresses are kept */
static void key_freehandler(void *arg, void *value) {
}

/* Key strdup (pointer copy) */
static char* key_duphandler(void *arg, const char *name) {
  union {
    const char *roname;
    char *name;
  } u;
  u.roname = name;
  return u.name;
}

/* Key sav hashes are using case-insensitive version */
static inthash_keys key_sav_hashes(void *arg, const char *value) {
  hash_struct *const hash = (hash_struct*) arg;
  convtolower(hash->catbuff, value);
  return inthash_hash_value(hash->catbuff);
}

/* Key sav comparison is case-insensitive */
static int key_sav_equals(void *arg, const char *a, const char *b) {
  return strcasecmp(a, b) == 0;
}

/* Pseudo-key (lien_url structure) hash function */
static inthash_keys key_adrfil_hashes_generic(void *arg, const char *value_, 
                                              const int former) {
  hash_struct *const hash = (hash_struct*) arg;
  const lien_url*const lien = (lien_url*) value_;
  int i;
  const char *const adr = !former ? lien->adr : lien->former_adr;
  const char *const fil = !former ? lien->fil : lien->former_fil;
  const char *const adr_norm = adr != NULL ? 
    ( hash->normalized  ? jump_normalized(adr) : jump_identification(adr) )
    : NULL;

  // copy address
  assertf(adr_norm != NULL);
  strcpy(hash->normfil, adr_norm);

  // copy link
  assertf(fil != NULL);
  if (hash->normalized) {
    fil_normalized(fil, &hash->normfil[strlen(hash->normfil)]);
  } else {
    strcpy(&hash->normfil[i], fil);
  }

  // hash
  return inthash_hash_value(hash->normfil);
}

/* Pseudo-key (lien_url structure) comparison function */
static int key_adrfil_equals_generic(void *arg, const char *a_, const char *b_, 
                                     const int former) {
  hash_struct *const hash = (hash_struct*) arg;
  const int normalized = hash->normalized;
  const lien_url*const a = (lien_url*) a_;
  const lien_url*const b = (lien_url*) b_;
  const char *const a_adr = !former ? a->adr : a->former_adr;
  const char *const b_adr = !former ? b->adr : b->former_adr;
  const char *const a_fil = !former ? a->fil : a->former_fil;
  const char *const b_fil = !former ? b->fil : b->former_fil;
  const char *ja;
  const char *jb;

  // safety
  assertf(a_adr != NULL);
  assertf(b_adr != NULL);
  assertf(a_fil != NULL);
  assertf(b_fil != NULL);

  // skip scheme and authentication to the domain (possibly without www.)
  ja = normalized ? jump_normalized(a_adr) : jump_identification(a_adr);
  jb = normalized ? jump_normalized(b_adr) : jump_identification(b_adr);
  assertf(ja != NULL);
  assertf(jb != NULL);
  if (strcasecmp(ja, jb) != 0) {
    return 0;
  }

  // now compare pathes
  if (normalized) {
    fil_normalized(a_fil, hash->normfil);
    fil_normalized(b_fil, hash->normfil2);
    return strcmp(hash->normfil, hash->normfil2) == 0;
  } else {
    return strcmp(a_fil, b_fil) == 0;
  }
}

/* "adr"/"fil" lien_url structure members hashing function */
static inthash_keys key_adrfil_hashes(void *arg, const char *value_) {
  return key_adrfil_hashes_generic(arg, value_, 0);
}

/* "adr"/"fil" lien_url structure members comparison function */
static int key_adrfil_equals(void *arg, const char *a, const char *b) {
  return key_adrfil_equals_generic(arg, a, b, 0);
}

/* "former_adr"/"former_fil" lien_url structure members hashing function */
static inthash_keys key_former_adrfil_hashes(void *arg, const char *value_) {
  return key_adrfil_hashes_generic(arg, value_, 1);
}

/* "former_adr"/"former_fil" lien_url structure members comparison function */
static int key_former_adrfil_equals(void *arg, const char *a, const char *b) {
  return key_adrfil_equals_generic(arg, a, b, 1);
}

void hash_init(hash_struct * hash, int normalized) {
  hash->sav = inthash_new(0);
  hash->adrfil = inthash_new(0);
  hash->former_adrfil = inthash_new(0);
  hash->normalized = normalized;

  /* Case-insensitive comparison ; keys are direct char* filenames */
  inthash_value_set_key_handler(hash->sav,
                                key_duphandler,
                                key_freehandler,
                                key_sav_hashes,
                                key_sav_equals,
                                hash);

  /* URL-style comparison ; keys are lien_url structure pointers casted 
     to char* */
  inthash_value_set_key_handler(hash->adrfil,
                                key_duphandler,
                                key_freehandler,
                                key_adrfil_hashes,
                                key_adrfil_equals,
                                hash);
  inthash_value_set_key_handler(hash->former_adrfil,
                                key_duphandler,
                                key_freehandler,
                                key_former_adrfil_hashes,
                                key_former_adrfil_equals,
                                hash);
}

void hash_free(hash_struct *hash) {
  if (hash != NULL) {
    inthash_delete(&hash->sav);
    inthash_delete(&hash->adrfil);
    inthash_delete(&hash->former_adrfil);
  }
}

// retour: position ou -1 si non trouvé
int hash_read(const hash_struct * hash, const char *nom1, const char *nom2,
              hash_struct_type type) {
  intptr_t intvalue;
  lien_url lien;

  /* read */
  switch(type) {
  case HASH_STRUCT_FILENAME:
    if (inthash_read(hash->sav, nom1, &intvalue)) {
      return (int) intvalue;
    } else {
      return -1;
    }
    break;
  case HASH_STRUCT_ADR_PATH:
    memset(&lien, 0, sizeof(lien));
    lien.adr = key_duphandler(NULL, nom1);
    lien.fil = key_duphandler(NULL, nom2);
    if (inthash_read(hash->adrfil, (char*) &lien, &intvalue)) {
      return (int) intvalue;
    } else {
      return -1;
    }
    break;
  case HASH_STRUCT_ORIGINAL_ADR_PATH:
    memset(&lien, 0, sizeof(lien));
    lien.former_adr = key_duphandler(NULL, nom1);
    lien.former_fil = key_duphandler(NULL, nom2);
    if (inthash_read(hash->former_adrfil, (char*) &lien, &intvalue)) {
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
void hash_write(hash_struct * hash, int lpos) {
  /* first entry: destination filename (lowercased) */
  inthash_write(hash->sav, hash->liens[lpos]->sav, lpos);

  /* second entry: URL address and path */
  inthash_write(hash->adrfil, (char*) hash->liens[lpos], lpos);

  /* third entry: URL address and path before redirect */
  if (hash->liens[lpos]->former_adr) {        // former_adr existe?
    inthash_write(hash->former_adrfil, (char*) hash->liens[lpos], lpos);
  }
}
