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
#ifdef _WIN32
#include "htscharset.h" /* hts_pathToUCS2, hts_convertUCS2StringToUTF8 */
#endif

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
  cookie_insert(insert, cookie->max_len - (size_t) (insert - cookie->data),
                cook);
#if DEBUG_COOK
  printf("add_new cookie: name=\"%s\" value=\"%s\" domain=\"%s\" path=\"%s\"\n",
         cook_name, cook_value, domain, path);
#endif
  return 0;
}

// effacer cookie si existe
int cookie_del(t_cookie * cookie, const char *cook_name, const char *domain, const char *path) {
  char *a, *b;

  b = cookie_find(cookie->data, cook_name, domain, path);
  if (b) {
    a = cookie_nextfield(b);
    cookie_delete(b, cookie->max_len - (size_t) (b - cookie->data), a - b);
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
  int i;
  for (i = (int) l - 1; i >= 0; i--) {
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

// Read cookies.txt, plus (Windows only) any copied IE cookies *@*.txt.
// Returns !=0 on error.
int cookie_load(t_cookie * cookie, const char *fpath, const char *name) {
  char catbuff[CATBUFF_SIZE];
  char buffer[8192];

  // Merge any IE cookies first
#ifdef _WIN32
  {
    WIN32_FIND_DATAW find;
    HANDLE h;
    char pth[HTS_URLMAXSIZE];
    LPWSTR wpth;

    strcpybuff(pth, fpath);
    strcatbuff(pth, "*@*.txt");
    // Wide glob so a long or non-ASCII IE-cookie folder is scanned (#133).
    wpth = hts_pathToUCS2(pth);
    h = wpth != NULL ? FindFirstFileW(wpth, &find) : INVALID_HANDLE_VALUE;
    freet(wpth);
    if (h != INVALID_HANDLE_VALUE) {
      do {
        if (!(find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
          if (!(find.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM)) {
            // cFileName is UTF-16: convert to UTF-8 so the mirror path and
            // hts_fopen_utf8/_unlink stay UTF-8 end to end (no CP_ACP
            // mojibake).
            char *u = hts_convertUCS2StringToUTF8(find.cFileName, -1);
            FILE *fp =
                u != NULL
                    ? FOPEN(fconcat(catbuff, sizeof(catbuff), fpath, u), "rb")
                    : NULL;

            if (fp) {
              char cook_name[256];
              char cook_value[1000];
              char domainpathpath[512];
              char dummy[512];

              lien_adrfil af; // host + path (/)
              int cookie_merged = 0;

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
                UNLINK(fconcat(catbuff, sizeof(catbuff), fpath, u));
            }                   // if fp
            freet(u);
          }
      } while (FindNextFileW(h, &find));
      FindClose(h);
    }
  }
#endif

  // Ensuite, cookies.txt
  {
    FILE *fp = FOPEN(fconcat(catbuff, sizeof(catbuff), fpath, name), "rb");

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

/* Write cookies.txt; returns 0 on success. The jar holds live session
   cookies, so keep it owner-only on Unix (Windows inherits folder ACLs). */
int cookie_save(t_cookie * cookie, const char *name) {
  char catbuff[CATBUFF_SIZE];

  if (strnotempty(cookie->data)) {
    char BIGSTK line[8192];
#ifdef _WIN32
    FILE *fp = FOPEN(fconv(catbuff, sizeof(catbuff), name), "wb");
#else
    const int fd = open(fconv(catbuff, sizeof(catbuff), name),
                        O_WRONLY | O_CREAT | O_TRUNC, HTS_PROTECT_FILE);
    FILE *fp = fd != -1 ? fdopen(fd, "wb") : NULL;

    if (fd != -1 && fp == NULL)
      close(fd);    /* fdopen failed: don't leak the descriptor */
    if (fp != NULL) /* O_CREAT's mode skips pre-existing jars: tighten those */
      (void) fchmod(fd, HTS_PROTECT_FILE);
#endif

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

// Insert string ins before s. s_size is the capacity of the buffer at s.
void cookie_insert(char *s, size_t s_size, const char *ins) {
  char *buff;

  if (strnotempty(s) == 0) { // nothing there yet: just concatenate
    strlcatbuff(s, ins, s_size);
  } else {
    buff = (char *) malloct(strlen(s) + 1);
    if (buff) {
      strlcpybuff(buff, s, strlen(s) + 1); // temporary copy of s
      strlcpybuff(s, ins, s_size);         // write ins
      strlcatbuff(s, buff, s_size);        // then the saved content
      freet(buff);
    }
  }
}

// Delete the substring of s at position pos. s_size is the capacity at s.
void cookie_delete(char *s, size_t s_size, size_t pos) {
  char *buff;

  if (strnotempty(s + pos) == 0) { // nothing after pos: truncate
    s[0] = '\0';
  } else {
    buff = (char *) malloct(strlen(s + pos) + 1);
    if (buff) {
      strlcpybuff(buff, s + pos, strlen(s + pos) + 1); // temporary copy
      strlcpybuff(s, buff, s_size);                    // overwrite from start
      freet(buff);
    }
  }
}

// Return field <param> (0-based, tab-separated) of the cookie line cookie_base,
// into buffer. ex: cookie_get("ceci est<tab>un<tab>exemple", 1) returns "un".
// buffer must hold at least COOKIE_FIELD_BUFFER_SIZE bytes (all callers use
// char[8192]).
#define COOKIE_FIELD_BUFFER_SIZE 8192
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
        htsbuff b = htsbuff_ptr(buffer, COOKIE_FIELD_BUFFER_SIZE);

        while((*a) && (*a != '\t') && (*a != '\n'))
          a++;
        htsbuff_catn(&b, cookie_base, (size_t) (a - cookie_base));
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

/* Build the auth prefix (host + path, query stripped) into prefix.
   Callers pass a buffer of HTS_URLMAXSIZE * 2 bytes. */
char *bauth_prefix(char *prefix, const char *adr, const char *fil) {
  char *a;

  strlcpybuff(prefix, jump_identification_const(adr), HTS_URLMAXSIZE * 2);
  strlcatbuff(prefix, fil, HTS_URLMAXSIZE * 2);
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
