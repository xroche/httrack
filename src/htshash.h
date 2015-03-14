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
void hash_init(httrackp *opt, hash_struct *hash, int normalized);
void hash_free(hash_struct *hash);
int hash_read(const hash_struct * hash, const char *nom1, const char *nom2,
              hash_struct_type type);
void hash_write(hash_struct * hash, size_t lpos);
int *hash_calc_chaine(hash_struct * hash, hash_struct_type type, int pos);
unsigned long int hash_cle(const char *nom1, const char *nom2);
#endif

#endif
