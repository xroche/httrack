/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998-2015 Xavier Roche and other contributors

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.

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

#include "htsopt.h"
#include "htshash.h"

/* specific definitions */
#include "htsbase.h"
#include "htsglobal.h"
#include "htsmd5.h"
#include "htscore.h"
#include "coucal.h"
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
static void key_freehandler(void *arg, coucal_key value) {
}

/* Key strdup (pointer copy) */
static coucal_key key_duphandler(void *arg, coucal_key_const name) {
  union {
    coucal_key_const roname;
    coucal_key name;
  } u;
  u.roname = name;
  return u.name;
}

/* Key sav hashes are using case-insensitive version */
static coucal_hashkeys key_sav_hashes(void *arg, coucal_key_const key) {
  hash_struct *const hash = (hash_struct*) arg;
  convtolower(hash->catbuff, (const char*) key);
  return coucal_hash_string(hash->catbuff);
}

/* Key sav comparison is case-insensitive */
static int key_sav_equals(void *arg,
                          coucal_key_const a_,
                          coucal_key_const b_) {
  const char *const a = (const char*) a_;
  const char *const b = (const char*) b_;
  return strcasecmp(a, b) == 0;
}

static const char* key_sav_debug_print(void *arg,
                                       coucal_key_const a) {
  return (const char*) a;
}

static const char* value_sav_debug_print(void *arg, coucal_value_const a) {
  return (char*) a.ptr;
}

/* Pseudo-key (lien_url structure) hash function */
static coucal_hashkeys key_adrfil_hashes_generic(void *arg,
                                              coucal_key_const value, 
                                              const int former) {
  hash_struct *const hash = (hash_struct*) arg;
  const lien_url*const lien = (const lien_url*) value;
  const char *const adr = !former ? lien->adr : lien->former_adr;
  const char *const fil = !former ? lien->fil : lien->former_fil;
  const char *const adr_norm = adr != NULL ? 
    ( hash->normalized  ? jump_normalized_const(adr)
                        : jump_identification_const(adr) )
    : NULL;

  // copy address
  assertf(adr_norm != NULL);
  strcpy(hash->normfil, adr_norm);

  // copy link
  assertf(fil != NULL);
  if (hash->normalized) {
    fil_normalized(fil, &hash->normfil[strlen(hash->normfil)]);
  } else {
    strcpy(&hash->normfil[strlen(hash->normfil)], fil);
  }

  // hash
  return coucal_hash_string(hash->normfil);
}

/* Pseudo-key (lien_url structure) comparison function */
static int key_adrfil_equals_generic(void *arg,
                                     coucal_key_const a_,
                                     coucal_key_const b_, 
                                     const int former) {
  hash_struct *const hash = (hash_struct*) arg;
  const int normalized = hash->normalized;
  const lien_url*const a = (const lien_url*) a_;
  const lien_url*const b = (const lien_url*) b_;
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
  ja = normalized
    ? jump_normalized_const(a_adr) : jump_identification_const(a_adr);
  jb = normalized
    ? jump_normalized_const(b_adr) : jump_identification_const(b_adr);
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

static const char* key_adrfil_debug_print_(void *arg,
                                           coucal_key_const a_,
                                           const int former) {
  hash_struct *const hash = (hash_struct*) arg;
  const lien_url*const a = (const lien_url*) a_;
  const char *const a_adr = !former ? a->adr : a->former_adr;
  const char *const a_fil = !former ? a->fil : a->former_fil;
  snprintf(hash->normfil, sizeof(hash->normfil), "%s%s", a_adr, a_fil);
  return hash->normfil;
}

static const char* key_adrfil_debug_print(void *arg,
                                          coucal_key_const a_) {
  return key_adrfil_debug_print_(arg, a_, 0);
}

static const char* key_former_adrfil_debug_print(void *arg,
                                                 coucal_key_const a_) {
  return key_adrfil_debug_print_(arg, a_, 1);
}

static const char* value_adrfil_debug_print(void *arg,
                                            coucal_value_const value) {
  hash_struct *const hash = (hash_struct*) arg;
  snprintf(hash->normfil2, sizeof(hash->normfil2), "%d", (int) value.intg);
  return hash->normfil2;
}

/* "adr"/"fil" lien_url structure members hashing function */
static coucal_hashkeys key_adrfil_hashes(void *arg, coucal_key_const value_) {
  return key_adrfil_hashes_generic(arg, value_, 0);
}

/* "adr"/"fil" lien_url structure members comparison function */
static int key_adrfil_equals(void *arg,
                             coucal_key_const a,
                             coucal_key_const b) {
  return key_adrfil_equals_generic(arg, a, b, 0);
}

/* "former_adr"/"former_fil" lien_url structure members hashing function */
static coucal_hashkeys key_former_adrfil_hashes(void *arg, coucal_key_const value_) {
  return key_adrfil_hashes_generic(arg, value_, 1);
}

/* "former_adr"/"former_fil" lien_url structure members comparison function */
static int key_former_adrfil_equals(void *arg,
                                    coucal_key_const a,
                                    coucal_key_const b) {
  return key_adrfil_equals_generic(arg, a, b, 1);
}

void hash_init(httrackp *opt, hash_struct * hash, int normalized) {
  hash->sav = coucal_new(0);
  hash->adrfil = coucal_new(0);
  hash->former_adrfil = coucal_new(0);
  hash->normalized = normalized;

  hts_set_hash_handler(hash->sav, opt);
  hts_set_hash_handler(hash->adrfil, opt);
  hts_set_hash_handler(hash->former_adrfil, opt);

  coucal_set_name(hash->sav, "hash->sav");
  coucal_set_name(hash->adrfil, "hash->adrfil");
  coucal_set_name(hash->former_adrfil, "hash->former_adrfil");

  /* Case-insensitive comparison ; keys are direct char* filenames */
  coucal_value_set_key_handler(hash->sav,
                                key_duphandler,
                                key_freehandler,
                                key_sav_hashes,
                                key_sav_equals,
                                hash);

  /* URL-style comparison ; keys are lien_url structure pointers casted 
     to char* */
  coucal_value_set_key_handler(hash->adrfil,
                                key_duphandler,
                                key_freehandler,
                                key_adrfil_hashes,
                                key_adrfil_equals,
                                hash);
  coucal_value_set_key_handler(hash->former_adrfil,
                                key_duphandler,
                                key_freehandler,
                                key_former_adrfil_hashes,
                                key_former_adrfil_equals,
                                hash);

  /* pretty-printing */
  coucal_set_print_handler(hash->sav,
                            key_sav_debug_print,
                            value_sav_debug_print,
                            NULL);
  coucal_set_print_handler(hash->adrfil,
                            key_adrfil_debug_print,
                            value_adrfil_debug_print,
                            hash);
  coucal_set_print_handler(hash->former_adrfil,
                            key_former_adrfil_debug_print,
                            value_adrfil_debug_print,
                            hash);
}

void hash_free(hash_struct *hash) {
  if (hash != NULL) {
    coucal_delete(&hash->sav);
    coucal_delete(&hash->adrfil);
    coucal_delete(&hash->former_adrfil);
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
    if (coucal_read(hash->sav, nom1, &intvalue)) {
      return (int) intvalue;
    } else {
      return -1;
    }
    break;
  case HASH_STRUCT_ADR_PATH:
    memset(&lien, 0, sizeof(lien));
    lien.adr = key_duphandler(NULL, nom1);
    lien.fil = key_duphandler(NULL, nom2);
    if (coucal_read(hash->adrfil, (char*) &lien, &intvalue)) {
      return (int) intvalue;
    } else {
      return -1;
    }
    break;
  case HASH_STRUCT_ORIGINAL_ADR_PATH:
    memset(&lien, 0, sizeof(lien));
    lien.former_adr = key_duphandler(NULL, nom1);
    lien.former_fil = key_duphandler(NULL, nom2);
    if (coucal_read(hash->former_adrfil, (char*) &lien, &intvalue)) {
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
void hash_write(hash_struct * hash, size_t lpos) {
  /* first entry: destination filename (lowercased) */
  coucal_write(hash->sav, (*hash->liens)[lpos]->sav, lpos);

  /* second entry: URL address and path */
  coucal_write(hash->adrfil, (*hash->liens)[lpos], lpos);

  /* third entry: URL address and path before redirect */
  if ((*hash->liens)[lpos]->former_adr) {        // former_adr existe?
    coucal_write(hash->former_adrfil, (*hash->liens)[lpos], lpos);
  }
}
