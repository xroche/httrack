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
/*       filters ("regexp")                                     */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Internal engine bytecode */
#define HTS_INTERNAL_BYTECODE

// *.gif                  match all gif files 
// *[file]/*[file].exe    match all exe files with one folder structure
// *[A-Z,a-z,0-9,/,?]     match letters, nums, / and ?
// *[A-Z,a-z,0-9,/,?]

// *[>10,<100].gif        match all gif files larger than 10KB and smaller than 100KB
// *[file,>10,<100].gif   FORBIDDEN: you must not mix size test and pattern test

#include "htsfilters.h"

/* specific definitions */
#include "htsbase.h"
#include "htslib.h"
#include <ctype.h>
#include <stdint.h>
/* END specific definitions */

// à partir d'un tableau de {"+*.toto","-*.zip","+*.tata"} définit si nom est autorisé
// optionnel: taille à contrôller (ou numéro, etc) en pointeur
//            (en de détection de *size, la taille limite est écrite par dessus *size)
// exemple: +-*.gif*[<5] == supprimer GIF si <5KB
int fa_strjoker(int type, char **filters, int nfil, const char *nom, LLint * size,
                int *size_flag, int *depth) {
  int verdict = 0;              // on sait pas
  int i;
  LLint sizelimit = 0;

  if (size)
    sizelimit = *size;
  for(i = 0; i < nfil; i++) {
    LLint sz;
    int filteroffs = 1;

    if (strncmp(filters[i] + filteroffs, "mime:", 5) == 0) {
      if (type == 0)            // regular filters
        continue;
      filteroffs += 5;          // +mime:text/html
    } else {                    // mime filters
      if (type != 0)
        continue;
    }
    if (size)
      sz = *size;
    /* size unknown (scan time): no size pointer => size tests stay neutral */
    if (strjoker(nom, filters[i] + filteroffs, size ? &sz : NULL, size_flag)) {
      if (size)
        if (sz != *size)
          sizelimit = sz;
      if (filters[i][0] == '+')
        verdict = 1;            // autorisé
      else
        verdict = -1;           // interdit
      if (depth)
        *depth = i;
    }
  }
  if (size)
    *size = sizelimit;
  return verdict;
}

int fa_strjoker_dual(int type, char **filters, int nfil, const char *nom1,
                     const char *nom2, LLint *size, int *size_flag,
                     int *depth) {
  int depth1 = 0, depth2 = 0;
  int flag1 = 0, flag2 = 0;
  LLint sz1 = 0, sz2 = 0;
  int jok1, jok2, use1;

  if (size) {
    sz1 = *size;
    sz2 = *size;
  }
  jok1 = fa_strjoker(type, filters, nfil, nom1, size ? &sz1 : NULL,
                     size_flag ? &flag1 : NULL, &depth1);
  jok2 = fa_strjoker(type, filters, nfil, nom2, size ? &sz2 : NULL,
                     size_flag ? &flag2 : NULL, &depth2);
  use1 = jok2 == 0 || (jok1 != 0 && depth1 >= depth2);
  if (size)
    *size = use1 ? sz1 : sz2;
  if (size_flag)
    *size_flag = use1 ? flag1 : flag2;
  if (depth)
    *depth = use1 ? depth1 : depth2;
  return use1 ? jok1 : jok2;
}

/* Failure memo for the recursive matcher: one bit per (chaine, joker) offset
   pair keeps star-heavy patterns polynomial instead of exponential (#501). */
typedef struct strjoker_memo {
  const char *chaine0, *joker0; /* offsets are relative to these bases */
  size_t stride;                /* strlen(joker0) + 1 */
  unsigned char *failed;        /* failed-pair bitmap; NULL: no memo */
} strjoker_memo;

static const char *strjoker_impl(const strjoker_memo *memo, const char *chaine,
                                 const char *joker, LLint *size,
                                 int *size_flag);

/* A pair that failed once fails forever (*size is only written on the success
   path), so record NULL results and cut the re-exploration. */
static const char *strjoker_rec(const strjoker_memo *memo, const char *chaine,
                                const char *joker, LLint *size,
                                int *size_flag) {
  size_t bit = 0;
  const char *adr;

  if (memo->failed) {
    bit = (size_t) (chaine - memo->chaine0) * memo->stride +
          (size_t) (joker - memo->joker0);
    if (memo->failed[bit >> 3] & (unsigned char) (1u << (bit & 7)))
      return NULL;
  }
  adr = strjoker_impl(memo, chaine, joker, size, size_flag);
  if (adr == NULL && memo->failed)
    memo->failed[bit >> 3] |= (unsigned char) (1u << (bit & 7));
  return adr;
}

// wildcard comparator: match chaine against joker (pattern), case-insensitive
// returns the address of the first matched letter past any leading joker
// (ie. *[..]toto.. returns the address of toto within chaine), NULL on mismatch
HTS_INLINE const char *strjoker(const char *chaine, const char *joker,
                                LLint *size, int *size_flag) {
  strjoker_memo memo = {chaine, joker, 0, NULL};
  unsigned char stackbits[2048];
  hts_boolean onheap = HTS_FALSE;
  const char *adr;

  if (chaine != NULL && joker != NULL) {
    const size_t n1 = strlen(chaine) + 1;

    memo.stride = strlen(joker) + 1;
    if (n1 <= (SIZE_MAX - 7) / memo.stride) { // overflow-safe bitmap size
      const size_t bytes = (n1 * memo.stride + 7) / 8;

      if (bytes <= sizeof(stackbits)) {
        memset(stackbits, 0, bytes);
        memo.failed = stackbits;
      } else {
        memo.failed = (unsigned char *) calloct(bytes, 1);
        onheap = memo.failed != NULL ? HTS_TRUE : HTS_FALSE;
      }
    }
  }
  adr = strjoker_rec(&memo, chaine, joker, size, size_flag);
  if (onheap)
    freet(memo.failed);
  return adr;
}

static const char *strjoker_impl(const strjoker_memo *memo, const char *chaine,
                                 const char *joker, LLint *size,
                                 int *size_flag) {
  if (strnotempty(joker) == 0) {        // fin de chaine joker
    if (strnotempty(chaine) == 0)       // fin aussi pour la chaine: ok
      return chaine;
    else if (chaine[0] == '?')
      return chaine;            // --?-- pour les index.html?Choix=2
    else
      return NULL;              // non trouvé
  }
  // on va progresser en suivant les 'mots' contenus dans le joker
  // un mot peut être un * ou bien toute autre séquence de lettres

  if (strcmp(joker, "*") == 0) {        // ok, rien après
    return chaine;
  }
  // 1er cas: jokers * ou jokers multiples *[..]
  if (joker[0] == '*') {        // comparer joker+reste (*toto/..)
    int jmp;                    // nombre de caractères pour le prochain mot dans joker
    int cut = 0;                // interdire tout caractère superflu
    char pass[256];
    char LEFT = '[', RIGHT = ']';
    int unique = 0;

    switch (joker[1]) {
    case '[':
      LEFT = '[';
      RIGHT = ']';
      unique = 0;
      break;
    case '(':
      LEFT = '(';
      RIGHT = ')';
      unique = 1;
      break;
    }

    if ((joker[1] == LEFT) && (joker[2] != LEFT)) {     // multijoker (tm)
      int i;

      for(i = 0; i < 256; i++)
        pass[i] = 0;

      // noms réservés
      if ((strfield(joker + 2, "file")) || (strfield(joker + 2, "name"))) {
        for(i = 0; i < 256; i++)
          pass[i] = 1;
        pass[(int) '?'] = 0;
        //pass[(int) ';'] = 0;
        pass[(int) '/'] = 0;
        i = 2;
        {
          int len = (int) strlen(joker);

          while((joker[i] != RIGHT) && (joker[i]) && (i < len))
            i++;
        }
      } else if (strfield(joker + 2, "path")) {
        for(i = 0; i < 256; i++)
          pass[i] = 1;
        pass[(int) '?'] = 0;
        //pass[(int) ';'] = 0;
        i = 2;
        {
          int len = (int) strlen(joker);

          while((joker[i] != RIGHT) && (joker[i]) && (i < len))
            i++;
        }
      } else if (strfield(joker + 2, "param")) {
        if (chaine[0] == '?') { // il y a un paramètre juste là
          for(i = 0; i < 256; i++)
            pass[i] = 1;
        }                       // sinon synonyme de 'rien'
        i = 2;
        {
          int len = (int) strlen(joker);

          while((joker[i] != RIGHT) && (joker[i]) && (i < len))
            i++;
        }
      } else {
        // décode les directives comme *[A-Z,âêîôû,0-9]
        i = 2;
        if (joker[i] == RIGHT) {        // *[] signifie "plus rien après"
          cut = 1;              // caractère supplémentaire interdit
        } else {
          int len = (int) strlen(joker);

          while((joker[i] != RIGHT) && (joker[i]) && (i < len)) {
            // '\' escapes the next char as a literal member, e.g. *[\[\]]
            if (joker[i] == '\\' && joker[i + 1] != '\0') {
              i++;
              pass[(int) (unsigned char) joker[i]] = 1;
              i++;
            } else if ((joker[i] == '<') || (joker[i] == '>')) { // *[<10]
              int lsize = 0;
              int lverdict;

              i++;
              if (sscanf(joker + i, "%d", &lsize) == 1) {
                if (size) {
                  if (*size >= 0) {
                    if (size_flag)
                      *size_flag = 1;   /* a joué */
                    if (joker[i - 1] == '<')
                      lverdict = (*size < lsize);
                    else
                      lverdict = (*size > lsize);
                    if (!lverdict) {
                      return NULL;      // ne correspond pas
                    } else {
                      *size = lsize;
                      return chaine;    // ok
                    }
                  } else
                    return NULL;        // ne correspond pas
                } else
                  return NULL;  // ne correspond pas (test impossible)
                // jump
                while(isdigit((unsigned char) joker[i]))
                  i++;
              }
            } else if (joker[i + 1] == '-' && joker[i + 2] != '\0') {
              // range *[A-Z]; the '\0' guard rejects a truncated *[a- (else
              // i+=3 overshoots the NUL)
              if ((int) (unsigned char) joker[i + 2] >
                  (int) (unsigned char) joker[i]) {
                int j;

                for(j = (int) (unsigned char) joker[i];
                    j <= (int) (unsigned char) joker[i + 2]; j++)
                  pass[j] = 1;
              }
              i += 3;
            } else { // 1 car, ex: *[ ]
              pass[(int) (unsigned char) joker[i]] = 1;
              i++;
            }
            if ((joker[i] == ',') || (joker[i] == ';'))
              i++;
          }
        }
      }
      // à sauter dans joker
      jmp = i;
      if (joker[i])
        jmp++;

      //
    } else {                    // tout autoriser
      //
      int i;

      for(i = 0; i < 256; i++)
        pass[i] = 1;            // tout autoriser
      jmp = 1;
    }

    {
      int i, max;
      const char *adr;

      // la chaine doit se terminer exactement
      if (cut) {
        if (strnotempty(chaine))
          return NULL;          // perdu
        else
          return chaine;        // ok
      }
      // comparaison en boucle, c'est ca qui consomme huhu..
      // le tableau pass[256] indique les caractères ASCII autorisés

      // tester sans le joker (pas ()+ mais ()*)
      if (!unique) {
        if ((adr = strjoker_rec(memo, chaine, joker + jmp, size, size_flag))) {
          return adr;
        }
      }
      // tester
      i = 0;
      if (!unique)
        max = (int) strlen(chaine);
      else                      /* *(a) only match a (not aaaaa) */
        max = strnotempty(chaine) ? 1 : 0; /* empty chaine: no char to eat */
      while(i < (int) max) {
        if (pass[(int) (unsigned char) chaine[i]]) {    // caractère autorisé
          if ((adr = strjoker_rec(memo, chaine + i + 1, joker + jmp, size,
                                  size_flag))) {
            return adr;
          }
          i++;
        } else
          i = max + 2;          // sortir
      }

      // tester chaîne vide
      if (i != max + 2)         // avant c'est ok
        if ((adr = strjoker_rec(memo, chaine + max, joker + jmp, size,
                                size_flag)))
          return adr;

      return NULL;              // perdu
    }

  } else {                      // comparer mot+reste (toto*..)
    if (strnotempty(chaine)) {
      int jmp = 0, ok = 1;

      // comparer début de joker et début de chaine
      while((joker[jmp] != '*') && (joker[jmp]) && (ok)) {
        // CI : remplacer streql par une comparaison !=
        if (!streql(chaine[jmp], joker[jmp])) {
          ok = 0;               // quitter
        }
        jmp++;
      }

      // comparaison ok?
      if (ok) {
        // continuer la comparaison.
        if (strjoker_rec(memo, chaine + jmp, joker + jmp, size, size_flag))
          return chaine;        // retourner 1e lettre
      }

    }                           // strlen(a)
    return NULL;
  }                             // * ou mot

  return NULL;
}

// recherche multiple
// exemple: find dans un texte de strcpybuff(*[A-Z,a-z],"*[0-9]"); va rechercher la première occurence
// d'un strcpy sur une variable ayant un nom en lettres et copiant une chaine de chiffres
// ATTENTION!! Eviter les jokers en début, où gare au temps machine!
const char *strjokerfind(const char *chaine, const char *joker) {
  const char *adr;

  while(*chaine) {
    if ((adr = strjoker(chaine, joker, NULL, NULL))) {  // ok trouvé
      return adr;
    }
    chaine++;
  }
  return NULL;
}
