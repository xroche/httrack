/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998 Xavier Roche and other contributors

SPDX-License-Identifier: GPL-3.0-or-later

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

Ethical use: we kindly ask that you NOT use this software to harvest email
addresses or to collect any other private information about people. Doing so
would dishonor our work and waste the many hours we have spent on it.

Please visit our Website: http://www.httrack.com
*/

/* ------------------------------------------------------------ */
/* File: httrack.c subroutines:                                 */
/*       hash table system (fast index)                         */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTSHASH_DEFH
#define HTSHASH_DEFH

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE

/* Forward definitions */
#ifndef HTS_DEF_FWSTRUCT_hash_struct
#define HTS_DEF_FWSTRUCT_hash_struct
typedef struct hash_struct hash_struct;
#endif

/** Type of hash. **/
typedef enum hash_struct_type {
  HASH_STRUCT_FILENAME = 0,
  HASH_STRUCT_ADR_PATH,
  HASH_STRUCT_ORIGINAL_ADR_PATH
} hash_struct_type;

// tables de hachage
void hash_init(httrackp *opt, hash_struct *hash, hts_boolean normalized);
void hash_free(hash_struct *hash);
/* Test helper: HTS_TRUE if the two URLs dedupe together under opt's urlhack
   flags. */
hts_boolean hash_url_equals(httrackp *opt, const char *adra, const char *fila,
                            const char *adrb, const char *filb);
int hash_read(const hash_struct * hash, const char *nom1, const char *nom2,
              hash_struct_type type);
void hash_write(hash_struct * hash, size_t lpos);
int *hash_calc_chaine(hash_struct * hash, hash_struct_type type, int pos);
unsigned long int hash_cle(const char *nom1, const char *nom2);
#endif

#endif
