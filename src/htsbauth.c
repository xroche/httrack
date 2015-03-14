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
/*       basic authentication: password storage                 */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Internal engine bytecode */
#define HTS_INTERNAL_BYTECODE

#include "htsbauth.h"

/* specific definitions */
#include "htsglobal.h"
#include "htslib.h"
#include "htscore.h"

/* END specific definitions */

// gestion des cookie
// ajoute, dans l'ordre
// !=0 : erreur
int cookie_add(t_cookie * cookie, const char *cook_name, const char *cook_value,
               const char *domain, const char *path) {
  char buffer[8192];
  char *a = cookie->data;
  char *insert;
  char cook[16384];

  // effacer éventuel cookie en double
  cookie_del(cookie, cook_name, domain, path);
  if (strlen(cook_value) > 1024)
    return -1;                  // trop long
  if (strlen(cook_name) > 256)
    return -1;                  // trop long
  if (strlen(domain) > 256)
    return -1;                  // trop long
  if (strlen(path) > 256)
    return -1;                  // trop long
  if (strlen(cookie->data)
             + strlen(cook_value)
             + strlen(cook_name)
             + strlen(domain)
             + strlen(path)
             + 256 > cookie->max_len)
    return -1;                  // impossible d'ajouter

  insert = a;                   // insérer ici
  while(*a) {
    if (strlen(cookie_get(buffer, a, 2)) < strlen(path))        // long. path (le + long est prioritaire)
      a = cookie->data + strlen(cookie->data);  // fin
    else {
      a = strchr(a, '\n');      // prochain champ
      if (a == NULL)
        a = cookie->data + strlen(cookie->data);        // fin
      else
        a++;
      while(*a == '\n')
        a++;
      insert = a;               // insérer ici
    }
  }
  // construction du cookie
  strcpybuff(cook, domain);
  strcatbuff(cook, "\t");
  strcatbuff(cook, "TRUE");
  strcatbuff(cook, "\t");
  strcatbuff(cook, path);
  strcatbuff(cook, "\t");
  strcatbuff(cook, "FALSE");
  strcatbuff(cook, "\t");
  strcatbuff(cook, "1999999999");
  strcatbuff(cook, "\t");
  strcatbuff(cook, cook_name);
  strcatbuff(cook, "\t");
  strcatbuff(cook, cook_value);
  strcatbuff(cook, "\n");
  if (!((strlen(cookie->data) + strlen(cook)) < cookie->max_len))
    return -1;                  // impossible d'ajouter
  cookie_insert(insert, cook);
#if DEBUG_COOK
  printf("add_new cookie: name=\"%s\" value=\"%s\" domain=\"%s\" path=\"%s\"\n",
         cook_name, cook_value, domain, path);
  //printf(">>>cook: %s<<<\n",cookie->data);
#endif
  return 0;
}

// effacer cookie si existe
int cookie_del(t_cookie * cookie, const char *cook_name, const char *domain, const char *path) {
  char *a, *b;

  b = cookie_find(cookie->data, cook_name, domain, path);
  if (b) {
    a = cookie_nextfield(b);
    cookie_delete(b, a - b);
#if DEBUG_COOK
    printf("deleted old cookie: %s %s %s\n", cook_name, domain, path);
#endif
  }
  return 0;
}

// Matches wildcard cookie domains that start with a dot
// chk_dom: the domain stored in the cookie (potentially wildcard).
// domain: query domain
static int cookie_cmp_wildcard_domain(const char *chk_dom, const char *domain) {
  const size_t n = strlen(chk_dom);
  const size_t m = strlen(domain);
  const size_t l = n < m ? n : m;
  size_t i;
  for (i = l - 1; i >= 0; i--) {
    if (chk_dom[n - i - 1] != domain[m - i - 1]) {
      return 1;
    }
  }
  if (m < n && chk_dom[0] == '.') {
    return 0;
  }
  else if (m != n) {
    return 1;
  }
  return 0;
}


// rechercher cookie à partir de la position s (par exemple s=cookie.data)
// renvoie pointeur sur ligne, ou NULL si introuvable
// path est aligné à droite et cook_name peut être vide (chercher alors tout cookie)
// .doubleclick.net     TRUE    /       FALSE   1999999999      id      A
char *cookie_find(char *s, const char *cook_name, const char *domain, const char *path) {
  char buffer[8192];
  char *a = s;

  while(*a) {
    int t;

    if (strnotempty(cook_name) == 0)
      t = 1;                    // accepter par défaut
    else
      t = (strcmp(cookie_get(buffer, a, 5), cook_name) == 0);   // tester si même nom
    if (t) {                    // même nom ou nom qualconque
      //
      const char *chk_dom = cookie_get(buffer, a, 0); // domaine concerné par le cookie

      if ((strlen(chk_dom) <= strlen(domain) &&
        strcmp(chk_dom, domain + strlen(domain) - strlen(chk_dom)) == 0) ||
        !cookie_cmp_wildcard_domain(chk_dom, domain)) {  // même domaine
          //
        const char *chk_path = cookie_get(buffer, a, 2);    // chemin concerné par le cookie

        if (strlen(chk_path) <= strlen(path)) {
          if (strncmp(path, chk_path, strlen(chk_path)) == 0) {       // même chemin
            return a;
          }
        }
      }
    }
    a = cookie_nextfield(a);
  }
  return NULL;
}

// renvoie prochain champ
char *cookie_nextfield(char *a) {
  char *b = a;

  a = strchr(a, '\n');          // prochain champ
  if (a == NULL)
    a = b + strlen(b);          // fin
  else
    a++;
  while(*a == '\n')
    a++;
  return a;
}

// lire cookies.txt
// lire également (Windows seulement) les *@*.txt (cookies IE copiés)
// !=0 : erreur
int cookie_load(t_cookie * cookie, const char *fpath, const char *name) {
  char catbuff[CATBUFF_SIZE];
  char buffer[8192];

  //  cookie->data[0]='\0';

  // Fusionner d'abord les éventuels cookies IE
#ifdef _WIN32
  {
    WIN32_FIND_DATAA find;
    HANDLE h;
    char pth[MAX_PATH + 32];

    strcpybuff(pth, fpath);
    strcatbuff(pth, "*@*.txt");
    h = FindFirstFileA((char *) pth, &find);
    if (h != INVALID_HANDLE_VALUE) {
      do {
        if (!(find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
          if (!(find.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM)) {
            FILE *fp = fopen(fconcat(catbuff, sizeof(catbuff), fpath, find.cFileName), "rb");

            if (fp) {
              char cook_name[256];
              char cook_value[1000];
              char domainpathpath[512];
              char dummy[512];

              //
              lien_adrfil af;   // chemin (/)
              int cookie_merged = 0;

              //
              // Read all cookies
              while(!feof(fp)) {
                cook_name[0] = cook_value[0] = domainpathpath[0]
                = dummy[0] = af.adr[0] = af.fil[0] = '\0';
                linput(fp, cook_name, 250);
                if (!feof(fp)) {
                  linput(fp, cook_value, 250);
                  if (!feof(fp)) {
                    int i;

                    linput(fp, domainpathpath, 500);
                    /* Read 6 other useless values */
                    for(i = 0; !feof(fp) && i < 6; i++) {
                      linput(fp, dummy, 500);
                    }
                    if (strnotempty(cook_name)
                        && strnotempty(cook_value)
                        && strnotempty(domainpathpath)) {
                      if (ident_url_absolute(domainpathpath, &af) >= 0) {
                        cookie_add(cookie, cook_name, cook_value, af.adr, af.fil);
                        cookie_merged = 1;
                      }
                    }
                  }
                }
              }
              fclose(fp);
              if (cookie_merged)
                remove(fconcat(catbuff, sizeof(catbuff), fpath, find.cFileName));
            }                   // if fp
          }
      } while(FindNextFileA(h, &find));
      FindClose(h);
    }
  }
#endif

  // Ensuite, cookies.txt
  {
    FILE *fp = fopen(fconcat(catbuff, sizeof(catbuff), fpath, name), "rb");

    if (fp) {
      char BIGSTK line[8192];

      while((!feof(fp)) && (((int) strlen(cookie->data)) < cookie->max_len)) {
        rawlinput(fp, line, 8100);
        if (strnotempty(line)) {
          if (strlen(line) < 8000) {
            if (line[0] != '#') {
              char domain[256]; // domaine cookie (.netscape.com)
              char path[256];   // chemin (/)
              char cook_name[1024];     // nom cookie (MYCOOK)
              char BIGSTK cook_value[8192];     // valeur (ID=toto,S=1234)

              strcpybuff(domain, cookie_get(buffer, line, 0));  // host
              strcpybuff(path, cookie_get(buffer, line, 2));    // path
              strcpybuff(cook_name, cookie_get(buffer, line, 5));       // name
              strcpybuff(cook_value, cookie_get(buffer, line, 6));      // value
#if DEBUG_COOK
              printf("%s\n", line);
#endif
              cookie_add(cookie, cook_name, cook_value, domain, path);
            }
          }
        }
      }
      fclose(fp);
      return 0;
    }
  }
  return -1;
}

// écrire cookies.txt
// !=0 : erreur
int cookie_save(t_cookie * cookie, const char *name) {
  char catbuff[CATBUFF_SIZE];

  if (strnotempty(cookie->data)) {
    char BIGSTK line[8192];
    FILE *fp = fopen(fconv(catbuff, sizeof(catbuff), name), "wb");

    if (fp) {
      char *a = cookie->data;

      fprintf(fp,
              "# HTTrack Website Copier Cookie File" LF
              "# This file format is compatible with Netscape cookies" LF);
      do {
        a += binput(a, line, 8000);
        fprintf(fp, "%s" LF, line);
      } while(strnotempty(line));
      fclose(fp);
      return 0;
    }
  } else
    return 0;
  return -1;
}

// insertion chaine ins avant s
void cookie_insert(char *s, const char *ins) {
  char *buff;

  if (strnotempty(s) == 0) {    // rien à faire, juste concat
    strcatbuff(s, ins);
  } else {
    buff = (char *) malloct(strlen(s) + 1);
    if (buff) {
      strcpybuff(buff, s);      // copie temporaire
      strcpybuff(s, ins);       // insérer
      strcatbuff(s, buff);      // copier
      freet(buff);
    }
  }
}

// destruction chaine dans s position pos
void cookie_delete(char *s, size_t pos) {
  char *buff;

  if (strnotempty(s + pos) == 0) {      // rien à faire, effacer
    s[0] = '\0';
  } else {
    buff = (char *) malloct(strlen(s + pos) + 1);
    if (buff) {
      strcpybuff(buff, s + pos);        // copie temporaire
      strcpybuff(s, buff);      // copier
      freet(buff);
    }
  }
}

// renvoie champ param de la chaine cookie_base
// ex: cookie_get("ceci est<tab>un<tab>exemple",1) renvoi "un"
const char *cookie_get(char *buffer, const char *cookie_base, int param) {
  const char *limit;

  while(*cookie_base == '\n')
    cookie_base++;
  limit = strchr(cookie_base, '\n');
  if (!limit)
    limit = cookie_base + strlen(cookie_base);
  if (limit) {
    if (param) {
      int i;

      for(i = 0; i < param; i++) {
        if (cookie_base) {
          cookie_base = strchr(cookie_base, '\t');      // prochain tab
          if (cookie_base)
            cookie_base++;
        }
      }
    }
    if (cookie_base) {
      if (cookie_base < limit) {
        const char *a = cookie_base;

        while((*a) && (*a != '\t') && (*a != '\n'))
          a++;
        buffer[0] = '\0';
        strncatbuff(buffer, cookie_base, (int) (a - cookie_base));
        return buffer;
      } else
        return "";
    } else
      return "";
  } else
    return "";
}

// fin cookies

// -- basic auth --

/* déclarer un répertoire comme possédant une authentification propre */
int bauth_add(t_cookie * cookie, const char *adr, const char *fil, const char *auth) {
  char buffer[HTS_URLMAXSIZE * 2];

  if (cookie) {
    if (!bauth_check(cookie, adr, fil)) {       // n'existe pas déja
      bauth_chain *chain = &cookie->auth;
      char *prefix = bauth_prefix(buffer, adr, fil);

      /* fin de la chaine */
      while(chain->next)
        chain = chain->next;
      chain->next = (bauth_chain *) calloc(sizeof(bauth_chain), 1);
      if (chain->next) {
        chain = chain->next;
        chain->next = NULL;
        strcpybuff(chain->auth, auth);
        strcpybuff(chain->prefix, prefix);
        return 1;
      }
    }
  }
  return 0;
}

/* tester adr et fil, et retourner authentification si nécessaire */
/* sinon, retourne NULL */
char *bauth_check(t_cookie * cookie, const char *adr, const char *fil) {
  char buffer[HTS_URLMAXSIZE * 2];

  if (cookie) {
    bauth_chain *chain = &cookie->auth;
    char *prefix = bauth_prefix(buffer, adr, fil);

    while(chain) {
      if (strnotempty(chain->prefix)) {
        if (strncmp(prefix, chain->prefix, strlen(chain->prefix)) == 0) {
          return chain->auth;
        }
      }
      chain = chain->next;
    }
  }
  return NULL;
}

char *bauth_prefix(char *prefix, const char *adr, const char *fil) {
  char *a;

  strcpybuff(prefix, jump_identification_const(adr));
  strcatbuff(prefix, fil);
  a = strchr(prefix, '?');
  if (a)
    *a = '\0';
  if (strchr(prefix, '/')) {
    a = prefix + strlen(prefix) - 1;
    while(*a != '/')
      a--;
    *(a + 1) = '\0';
  }
  return prefix;
}
