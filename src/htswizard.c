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
/*       wizard system (accept/refuse links)                    */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Internal engine bytecode */
#define HTS_INTERNAL_BYTECODE

#include "htscore.h"
#include "htswizard.h"

HTS_STATIC_ASSERT(sizeof(HTS_WIZARD_FILTER_SUFFIX) <= HTS_FILTER_SUFFIX_MAX,
                  wizard_suffix_fits_filter_slot);

/* specific definitions */
#include "htsbase.h"
#include <ctype.h>
/* END specific definitions */

/* "embedded" */
htspair_t hts_detect_embed[] = {
  {"img", "src"},
  {"link", "href"},

  /* embedded script hack */
  {"script", ".src"},

  /* style */
  {"style", "import"},

  {NULL, NULL}
};

/* HTML5 media siblings of <img src>: same near-link treatment (#451) */
static const htspair_t hts_detect_embed_html5[] = {
    {"source", "src"}, {"source", "srcset"}, {"track", "src"}, {NULL, NULL}};

/* Internal */
static int hts_acceptlink_(httrackp * opt, int ptr, const char *adr,
                           const char *fil, const char *tag,
                           const char *attribute, int *set_prio_to,
                           int *just_test_it);

/*
httrackp opt	 bloc d'options
int ptr,int lien_tot,lien_url** liens
							 relatif aux liens
char* adr,char* fil
							 adresse/fichier à tester
char** filters,int filptr,int filter_max
							 relatif aux filtres
robots_wizard* robots
							 relatif aux robots
int* set_prio_to
							 callback obligatoire "capturer ce lien avec prio=N-1"
int* just_test_it
							 callback optionnel "ne faire que tester ce lien éventuellement"
retour:
0 accepté
1 refusé
-1 pas d'avis
*/

int hts_acceptlink(httrackp * opt, int ptr,
                   const char *adr, const char *fil,
                   const char *tag, const char *attribute,
                   int *set_prio_to, int *just_test_it) {
  int forbidden_url = hts_acceptlink_(opt, ptr,
                                      adr, fil, tag, attribute, set_prio_to,
                                      just_test_it);
  int prev_prio = set_prio_to ? *set_prio_to : 0;

  // -------------------- PHASE 6 --------------------
  {
    int test_url = RUN_CALLBACK3(opt, check_link, adr, fil, forbidden_url);

    if (test_url != -1) {
      forbidden_url = test_url;
      if (set_prio_to)
        *set_prio_to = prev_prio;
    }
  }

  return forbidden_url;
}

hts_boolean hts_cmp_tag_token(const char *tag, const char *cmp) {
  int p;

  return (tag != NULL && strncasecmp(tag, cmp, (p = (int) strlen(cmp))) == 0 &&
          !isalnum((unsigned char) tag[p]))
             ? HTS_TRUE
             : HTS_FALSE;
}

/* TRUE if (tag, attribute) matches an embedded-asset pair in the table */
static hts_boolean is_embed_pair(const htspair_t *table, const char *tag,
                                 const char *attribute) {
  int i;
  for (i = 0; table[i].tag != NULL; i++) {
    if (hts_cmp_tag_token(tag, table[i].tag) &&
        hts_cmp_tag_token(attribute, table[i].attr))
      return HTS_TRUE;
  }
  return HTS_FALSE;
}

/* The engine's robots.txt verdict for (adr,fil). Under HTS_ROBOTS_SOMETIMES an
   explicit filter acceptance overrides the ban, which is why the filter outcome
   is an input; the sitemap fetcher asks the same question outside the wizard.
 */
hts_boolean hts_robots_forbids(httrackp *opt, const char *adr, const char *fil,
                               hts_boolean filters_decided,
                               hts_boolean filters_refused) {
  if (!opt->robots || opt->robotsptr == NULL)
    return HTS_FALSE;
  if (checkrobots((robots_wizard *) opt->robotsptr, adr, fil) != -1)
    return HTS_FALSE;
  if (filters_decided && !filters_refused &&
      opt->robots == HTS_ROBOTS_SOMETIMES)
    return HTS_FALSE;
  return HTS_TRUE;
}

/* "<sign><host>", with any user:password@ stripped. */
static void wizard_cat_host(htsbuff *f, const char *sign, const char *adr) {
  htsbuff_cpy(f, sign);
  htsbuff_cat(f, jump_identification_const(adr));
}

/* "<sign><host>/" then the first len characters of fil ((size_t) -1: all). */
static void wizard_cat_path(htsbuff *f, const char *sign, const char *adr,
                            const char *fil, size_t len) {
  wizard_cat_host(f, sign, adr);
  if (*fil != '/')
    htsbuff_cat(f, "/");
  htsbuff_catn(f, fil, len);
}

void hts_wizard_prompt_url(char *dst, size_t dstsize, const char *adr,
                           const char *fil) {
  if (dst == NULL || dstsize == 0)
    return;
  /* clipped, never aborting: adr and fil are cut from a crawled link and
     together outgrow any prompt buffer */
  dst[0] = '\0';
  strlncatbuff(dst, adr, dstsize, dstsize - 1);
  if (*fil != '/')
    strlncatbuff(dst, "/", dstsize, dstsize - 1 - strlen(dst));
  strlncatbuff(dst, fil, dstsize, dstsize - 1 - strlen(dst));
}

HTSEXT_API hts_boolean hts_wizard_host_scope(const char *question, int k,
                                             char *dst, size_t dstsize) {
  const char *host, *port, *slash, *end, *scope;
  size_t len;

  if (dst == NULL || dstsize == 0)
    return HTS_FALSE;
  dst[0] = '\0';
  if (question == NULL || k < 0)
    return HTS_FALSE;

  host = jump_identification_const(question);
  port = jump_toport_const(question);
  slash = strchr(host, '/');
  end = host + strlen(host);
  scope = host;
  if (slash != NULL && slash < end)
    end = slash;
  /* the port belongs to the filter, so keep it and only bound the label walk */
  if (port != NULL && port < end)
    slash = port;
  else
    slash = end;
  /* a fully-qualified "foo.com." ends on the root label, which is not one */
  if (slash > host && slash[-1] == '.')
    slash--;
  if (slash == host || *host == '[') /* no host, or an IPv6 literal */
    return HTS_FALSE;
  if (hts_host_is_ipv4(host, (size_t) (slash - host)))
    return HTS_FALSE;

  /* widen by dropping one leading label per step, and never offer a bare TLD */
  for (; k > 0; k--) {
    const char *dot = memchr(scope, '.', (size_t) (slash - scope));

    if (dot == NULL)
      return HTS_FALSE;
    scope = dot + 1;
  }
  if (memchr(scope, '.', (size_t) (slash - scope)) == NULL)
    return HTS_FALSE;

  len = (size_t) (end - scope);
  if (len >= dstsize)
    return HTS_FALSE;
  memcpy(dst, scope, len);
  dst[len] = '\0';
  return HTS_TRUE;
}

hts_tristate hts_wizard_scope_answer(int n) {
  if (n >= HTS_WIZARD_SCOPE_EXCLUDE)
    return HTS_TRUE;
  if (n >= HTS_WIZARD_SCOPE_INCLUDE)
    return HTS_FALSE;
  return HTS_DEFAULT;
}

/* The domain a scope answer names, or HTS_FALSE for a host that has none. */
static hts_boolean wizard_answer_scope(const char *adr, int n, char *dst,
                                       size_t dstsize) {
  const int k =
      n - (hts_wizard_scope_answer(n) == HTS_TRUE ? HTS_WIZARD_SCOPE_EXCLUDE
                                                  : HTS_WIZARD_SCOPE_INCLUDE);

  return hts_wizard_host_scope(adr, k, dst, dstsize);
}

/* The subdomain form of the scope in slot 0, its apex in slot 1: the starred
   one does not match the apex, so a whole-domain answer needs both. */
static void wizard_cat_scope(htsbuff *f, const char *sign, const char *adr,
                             int n, int slot) {
  char scope[HTS_URLMAXSIZE];

  if (slot >= HTS_WIZARD_MAX_FILTERS)
    return;
  /* no domain lives below this host, so the host itself is the widest scope */
  if (!wizard_answer_scope(adr, n, scope, sizeof(scope))) {
    if (slot == 0) {
      wizard_cat_host(f, sign, adr);
      htsbuff_cat(f, "/*");
    }
    return;
  }
  htsbuff_cpy(f, sign);
  if (slot == 0)
    htsbuff_cat(f, "*.");
  htsbuff_cat(f, scope);
  htsbuff_cat(f, "/*");
}

void hts_wizard_answer_filter(htsbuff *f, int slot, int n, const char *adr,
                              const char *fil, hts_boolean seeker_up) {
  size_t dir = hts_lastcharoffset(fil);

  while (fil[dir] != '/' && dir > 0)
    dir--;

  /* a clipped pattern would be a wider rule than the answer asked for */
  assertf(f->cap >= HTS_FILTER_SLOT_SIZE);
  htsbuff_cpy(f, "");
  if (hts_wizard_scope_answer(n) != HTS_DEFAULT) {
    wizard_cat_scope(f, hts_wizard_scope_answer(n) == HTS_TRUE ? "-" : "+", adr,
                     n, slot);
    return;
  }
  if (slot != 0) /* every other answer emits a single filter */
    return;
  switch (n) {
  case 0:    /* this link only */
  case -999: /* an unreadable answer falls back to that same default */
    wizard_cat_path(f, "-", adr, fil, (size_t) -1);
    break;

  case 1: /* this directory and below */
    if (fil[dir] == '/') {
      /* stops before the last slash, so a doubled one collapses */
      wizard_cat_path(f, "-", adr, fil, dir);
      if (f->buf[f->len - 1] != '/')
        htsbuff_cat(f, "/");
      htsbuff_cat(f, "*");
    }
    break;

  case 2: /* the whole host */
    wizard_cat_host(f, "-", adr);
    htsbuff_cat(f, "/*");
    break;

  case 5: /* this directory and below, or the whole host */
    if (!seeker_up) {
      if (fil[dir] == '/') {
        wizard_cat_path(f, "+", adr, fil, dir + 1);
        htsbuff_cat(f, "*");
      }
    } else {
      wizard_cat_host(f, "+", adr);
      htsbuff_cat(f, "/*");
    }
    break;

  case 6: /* the whole host */
    wizard_cat_host(f, "+", adr);
    htsbuff_cat(f, "/*");
    break;

  case 7: /* this directory, files only */
    if (fil[dir] == '/') {
      wizard_cat_path(f, "+", adr, fil, dir + 1);
      htsbuff_cat(f, HTS_WIZARD_FILTER_SUFFIX);
    }
    break;

  default: /* the other answers add no filter */
    break;
  }
}

int hts_wizard_insert_filters(httrackp *opt, int n, const char *adr,
                              const char *fil, hts_boolean seeker_up) {
  char BIGSTK pattern[HTS_FILTER_SLOT_SIZE];
  htsbuff f = htsbuff_array(pattern);
  int slot, inserted = 0;

  /* grow first: a host-scope answer emits two filters */
  if ((*opt->filters.filptr) + 2 >= opt->maxfilter) {
    opt->maxfilter += HTS_FILTERSINC;
    if (filters_init(opt->filters.filters, opt->maxfilter, HTS_FILTERSINC) ==
        0) {
      printf("PANIC! : Too many filters : >%d [%d]\n", *opt->filters.filptr,
             __LINE__);
      fflush(stdout);
      hts_log_print(opt, LOG_PANIC, "Too many filters, giving up..(>%d)",
                    *opt->filters.filptr);
      hts_log_print(
          opt, LOG_INFO,
          "To avoid that: use #F option for more filters (example: -#F5000)");
      assertf("too many filters - giving up" == NULL); // wild..
    }
  }
  /* a counter outliving its array (an opt reused for a second crawl) would
     index past the end */
  if (opt->wizard_filters > *opt->filters.filptr)
    opt->wizard_filters = *opt->filters.filptr;
  for (slot = 0; slot < HTS_WIZARD_MAX_FILTERS; slot++) {
    hts_wizard_answer_filter(&f, slot, n, adr, fil, seeker_up);
    if (f.len == 0)
      break;
    /* a refused rule must not advance the block, or the caller reads back the
       neighbour's filter as this answer's */
    if (filters_insert(opt, opt->wizard_filters, pattern)) {
      opt->wizard_filters++;
      inserted++;
    }
  }
  return inserted;
}

void hts_wizard_apply_verdict(httrackp *opt, int n, const char *adr,
                              const char *fil, int *forbidden_url,
                              int *set_prio_to) {
  switch (n) {
  case -1: /* skip this link and every question after it */
    *forbidden_url = 1;
    opt->wizard = HTS_WIZARD_AUTO;
    break;

  case 0: /* this link */
  case 1: /* this directory and below */
  case 2: /* the whole host */
  case 3: /* the parent directory, which emits no filter yet */
    *forbidden_url = 1;
    break;

  case 4: /* wizard filters both allow and forbid, so an isolated link taken
             with no depth limit would mirror the whole site */
    *set_prio_to = 0 + 1; /* recursion level 0 */
    break;

  case -999: /* the "!" answer, and anything the front end could not parse */
    *forbidden_url = 1;
    hts_log_print(opt, LOG_WARNING,
                  "(wizard) could not read your answer at %s%s: link refused, "
                  "and the refusal recorded",
                  adr, fil);
    break;

  case 5:  /* this directory and below, or the whole host */
  case 6:  /* the whole host */
  case 7:  /* this directory, files only */
  case 50: /* nothing to do */
    break;

  default: /* a scope answer forbids like 2 or allows like 6; anything else is
              unknown */
    if (hts_wizard_scope_answer(n) == HTS_DEFAULT) {
      hts_log_print(opt, LOG_WARNING,
                    "(wizard) unknown answer %d at %s%s, keeping the computed "
                    "verdict",
                    n, adr, fil);
      break;
    }
    if (hts_wizard_scope_answer(n) == HTS_TRUE)
      *forbidden_url = 1;
    {
      char scope[HTS_URLMAXSIZE];

      if (!wizard_answer_scope(adr, n, scope, sizeof(scope)))
        hts_log_print(opt, LOG_WARNING,
                      "(wizard) %s has no domain above it, answer %d applied "
                      "to the whole host",
                      adr, n);
    }
    break;
  }

  /* the question is asked only while undecided; an answer that does not forbid
     authorizes the link */
  if (*forbidden_url == -1)
    *forbidden_url = 0;
}

static int hts_acceptlink_(httrackp * opt, int ptr,
                           const char *adr, const char *fil, const char *tag,
                           const char *attribute, int *set_prio_to,
                           int *just_test_it) {
  int forbidden_url = -1;
  int meme_adresse;
  int embedded_triggered = 0;

#define _FILTERS     (*opt->filters.filters)
#define _FILTERS_PTR (opt->filters.filptr)
#define _ROBOTS      ((robots_wizard*)opt->robotsptr)
  int may_set_prio_to = 0;

  // -------------------- PHASE 0 --------------------

  /* Infos */
  hts_log_print(opt, LOG_DEBUG, "wizard test begins: %s%s", adr, fil);

  /* Already exists? Then, we know that we knew that this link had to be known */
  if (adr[0] != '\0' && fil[0] != '\0' && opt->hash != NULL
      && hash_read(opt->hash, adr, fil, 1) >= 0) {
    return 0;                   /* Yokai */
  }
  // -------------------- PRELUDE OF PHASE 3-BIS --------------------

  /* Built-in known tags (<img src=..>, ..) */
  if (forbidden_url != 0 && opt->nearlink && tag != NULL && attribute != NULL) {
    if (is_embed_pair(hts_detect_embed, tag, attribute) ||
        is_embed_pair(hts_detect_embed_html5, tag, attribute)) {
      embedded_triggered = 1;
    }
  }

  // -------------------- PHASE 1 --------------------

  /* Doit-on traiter les non html? */
  if ((opt->getmode & HTS_GETMODE_NONHTML) == 0) { // non on ne doit pas
    if (!ishtml(opt, fil)) {                       // non il ne faut pas
      forbidden_url = 1;        // interdire récupération du lien
      hts_log_print(opt, LOG_DEBUG, "non-html file ignored at %s : %s", adr,
                    fil);
    }
  }

  /* Niveau 1: ne pas parser suivant! */
  if (ptr > 0) {
    if ((heap(ptr)->depth <= 0)
        || (heap(ptr)->depth <= 1 && !embedded_triggered)) {
      forbidden_url = 1;        // interdire récupération du lien
      hts_log_print(opt, LOG_DEBUG,
                    "file from too far level ignored at %s : %s", adr, fil);
    }
  }

  /* en cas d'échec en phase 1, retour immédiat! */
  if (forbidden_url == 1) {
    return forbidden_url;
  }
  // -------------------- PHASE 2 --------------------

  // ------------------------------------------------------
  // doit-on traiter ce lien?.. vérifier droits de déplacement
  /* --host-alias makes the declared names one address, so a link to another
     name of the same site travels under the same-address rules */
  meme_adresse = strfield2(adr, urladr()) ||
                 hts_host_same_alias(hts_host_alias_rules(opt), adr, urladr(),
                                     hts_host_alias_collapse_www(opt));
  if (meme_adresse)
    hts_log_print(opt, LOG_DEBUG, "Compare addresses: %s=%s", adr, urladr());
  else
    hts_log_print(opt, LOG_DEBUG, "Compare addresses: %s!=%s", adr, urladr());
  if (meme_adresse) {           // même adresse 
    {                           // tester interdiction de descendre
      // MODIFIE : en cas de remontée puis de redescente, il se pouvait qu'on ne puisse pas atteindre certains fichiers
      // problème: si un fichier est virtuellement accessible via une page mais dont le lien est sur une autre *uniquement*..
      char BIGSTK tempo[HTS_URLMAXSIZE * 2];
      char BIGSTK tempo2[HTS_URLMAXSIZE * 2];

      tempo[0] = tempo2[0] = '\0';

      // note (up/down): on calcule à partir du lien primaire, ET du lien précédent.
      // ex: si on descend 2 fois on peut remonter 1 fois

      if (lienrelatif(tempo, sizeof(tempo), fil,
                      heap(heap(ptr)->premier)->fil) == 0) {
        if (lienrelatif(tempo2, sizeof(tempo2), fil, heap(ptr)->fil) == 0) {
          hts_log_print(opt, LOG_DEBUG,
                        "build relative links to test: %s %s (with %s and %s)",
                        tempo, tempo2, heap(heap(ptr)->premier)->fil,
                        heap(ptr)->fil);

          // si vient de primary, ne pas tester lienrelatif avec (car host "différent")
          /*if (heap(heap(ptr)->premier) == 0) {   // vient de primary
             }
           */

          // NEW: finalement OK, sauf pour les moved repérés par link_import
          // PROBLEME : annulé a cause d'un lien éventuel isolé accepté..qui entrainerait un miroir

          // (test même niveau (NOUVEAU à cause de certains problèmes de filtres non intégrés))
          // NEW
          if ((tempo[0] != '\0' && tempo[1] != '\0'
               && strchr(tempo + 1, '/') == 0)
              || (tempo2[0] != '\0' && tempo2[1] != '\0'
                  && strchr(tempo2 + 1, '/') == 0)
            ) {
            if (!heap(ptr)->link_import) {     // ne résulte pas d'un 'moved'
              forbidden_url = 0;
              hts_log_print(opt, LOG_DEBUG, "same level link authorized: %s%s",
                            adr, fil);
            }
          }
          // down
          if ((strncmp(tempo, "../", 3)) || (strncmp(tempo2, "../", 3))) {      // pas montée sinon ne nbous concerne pas
            int test1, test2;

            if (!strncmp(tempo, "../", 3))
              test1 = 0;
            else
              test1 = (strchr(tempo + ((*tempo == '/') ? 1 : 0), '/') != NULL);
            if (!strncmp(tempo2, "../", 3))
              test2 = 0;
            else
              test2 =
                (strchr(tempo2 + ((*tempo2 == '/') ? 1 : 0), '/') != NULL);
            if ((test1) && (test2)) {   // on ne peut que descendre
              if ((opt->seeker & HTS_SEEKER_DOWN) == 0) {
                forbidden_url = 1;
                hts_log_print(opt, LOG_DEBUG, "lower link canceled: %s%s", adr,
                              fil);
              } else {                         // autorisé à priori - NEW
                if (!heap(ptr)->link_import) { // ne résulte pas d'un 'moved'
                  forbidden_url = 0;
                  hts_log_print(opt, LOG_DEBUG, "lower link authorized: %s%s",
                                adr, fil);
                }
              }
            } else if ((test1) || (test2)) {    // on peut descendre pour accéder au lien
              if ((opt->seeker & HTS_SEEKER_DOWN) != 0) {
                if (!heap(ptr)->link_import) { // ne résulte pas d'un 'moved'
                  forbidden_url = 0;
                  hts_log_print(opt, LOG_DEBUG, "lower link authorized: %s%s",
                                adr, fil);
                }
              }
            }
          }

          // up
          if ((!strncmp(tempo, "../", 3)) && (!strncmp(tempo2, "../", 3))) {    // impossible sans monter
            if ((opt->seeker & HTS_SEEKER_UP) == 0) {
              forbidden_url = 1;
              hts_log_print(opt, LOG_DEBUG, "upper link canceled: %s%s", adr,
                            fil);
            } else {                           // autorisé à monter - NEW
              if (!heap(ptr)->link_import) {   // ne résulte pas d'un 'moved'
                forbidden_url = 0;
                hts_log_print(opt, LOG_DEBUG, "upper link authorized: %s%s",
                              adr, fil);
              }
            }
          } else if ((!strncmp(tempo, "../", 3)) || (!strncmp(tempo2, "../", 3))) {     // Possible en montant
            if ((opt->seeker & HTS_SEEKER_UP) != 0) {
              if (!heap(ptr)->link_import) {   // ne résulte pas d'un 'moved'
                forbidden_url = 0;
                hts_log_print(opt, LOG_DEBUG, "upper link authorized: %s%s",
                              adr, fil);
              }
            } // sinon autorisé en descente
          }

        } else {
          hts_log_print(opt, LOG_ERROR,
                        "Error building relative link %s and %s", fil,
                        heap(ptr)->fil);
        }
      } else {
        hts_log_print(opt, LOG_ERROR, "Error building relative link %s and %s",
                      fil, heap(heap(ptr)->premier)->fil);
      }

    }                           // tester interdiction de descendre?

  } else {                      // adresse différente, sortir?

    // doit-on traiter ce lien?.. vérifier droits de sortie
    switch ((opt->travel & HTS_TRAVEL_SCOPE_MASK)) {
    case HTS_TRAVEL_SAME_ADDRESS:
      if (!opt->wizard)         // mode non wizard
        forbidden_url = 1;
      break;                    // interdicton de sortir au dela de l'adresse
    case HTS_TRAVEL_SAME_DOMAIN: {
      size_t i = hts_lastcharoffset(adr);
      size_t j = hts_lastcharoffset(urladr());

      if ((i > 0) && (j > 0)) {
        while ((i > 0) && (adr[i] != '.'))
          i--;
        while ((j > 0) && (urladr()[j] != '.'))
          j--;
        if ((i > 0) && (j > 0)) {
          i--;
          j--;
          while ((i > 0) && (adr[i] != '.'))
            i--;
          while ((j > 0) && (urladr()[j] != '.'))
            j--;
        }
      }
      if ((i > 0) && (j > 0)) {
        if (!strfield2(adr + i, urladr() + j)) { // !=
          if (!opt->wizard) {                    // mode non wizard
            forbidden_url = 1; // pas même domaine
            hts_log_print(opt, LOG_DEBUG, "foreign domain link canceled: %s%s",
                          adr, fil);
          }

        } else {
          if (opt->wizard) {   // mode wizard
            forbidden_url = 0; // même domaine
            hts_log_print(opt, LOG_DEBUG, "same domain link authorized: %s%s",
                          adr, fil);
          }
        }

      } else
        forbidden_url = 1;
    } break;
    case HTS_TRAVEL_SAME_TLD: {
      size_t i = hts_lastcharoffset(adr);
      size_t j = hts_lastcharoffset(urladr());

      while ((i > 0) && (adr[i] != '.'))
        i--;
      while ((j > 0) && (urladr()[j] != '.'))
        j--;
      if ((i > 0) && (j > 0)) {
        if (!strfield2(adr + i, urladr() + j)) { // !-
          if (!opt->wizard) {                    // mode non wizard
            forbidden_url = 1; // pas même .xx
            hts_log_print(opt, LOG_DEBUG,
                          "foreign location link canceled: %s%s", adr, fil);
          }
        } else {
          if (opt->wizard) {   // mode wizard
            forbidden_url = 0; // même domaine
            hts_log_print(opt, LOG_DEBUG, "same location link authorized: %s%s",
                          adr, fil);
          }
        }
      } else
        forbidden_url = 1;
    } break;
    case HTS_TRAVEL_EVERYWHERE:
      if (opt->wizard) {        // mode wizard
        forbidden_url = 0;
        break;
      }
    } // switch

    // ANCIENNE POS -- récupérer les liens à côtés d'un lien (nearlink)

  }                             // fin test adresse identique/différente

  // -------------------- PHASE 3 --------------------

  // récupérer les liens à côtés d'un lien (nearlink) (nvelle pos)
  if (forbidden_url != 0 && opt->nearlink) {
    if (!ishtml(opt, fil)) {    // non html
      forbidden_url = 0;        // autoriser
      may_set_prio_to = 1 + 1;  // set prio to 1 (parse but skip urls) if near is the winner
      hts_log_print(opt, LOG_DEBUG, "near link authorized: %s%s", adr, fil);
    }
  }
  // -------------------- PHASE 3-BIS --------------------

  /* Built-in known tags (<img src=..>, ..) */
  if (forbidden_url != 0 && embedded_triggered) {
    forbidden_url = 0;          // autoriser
    may_set_prio_to = 1 + 1;    // set prio to 1 (parse but skip urls) if near is the winner
    hts_log_print(opt, LOG_DEBUG, "near link authorized (friendly tag): %s%s",
                  adr, fil);
  }

  // -------------------- PHASE 4 --------------------

  // ------------------------------------------------------
  // Si wizard, il se peut qu'on autorise ou qu'on interdise 
  // un lien spécial avant même de tester sa position, sa hiérarchie etc.
  // peut court-circuiter le forbidden_url précédent
  if (opt->wizard) {            // le wizard entre en action..
    //
    int question = 1;           // poser une question                            
    int force_mirror = 0;       // pour mirror links
    int filters_answer = 0;     // décision prise par les filtres
    /* Don't enlarge: the abort gates url_savename_addstr's append (#1269). */
    char BIGSTK l[HTS_URLMAXSIZE * 2];
    char BIGSTK lfull[HTS_URLMAXSIZE * 2];

    if (forbidden_url != -1)
      question = 0;             // pas de question, résolu

    // former URL complète du lien actuel
    strcpybuff(l, jump_identification_const(adr));
    if (*fil != '/')
      strcatbuff(l, "/");
    strcatbuff(l, fil);
    // full version (http://foo:bar@www.foo.com/bar.html)
    if (!link_has_authority(adr))
      strcpybuff(lfull, "http://");
    else
      lfull[0] = '\0';
    strcatbuff(lfull, adr);
    if (*fil != '/')
      strcatbuff(lfull, "/");
    strcatbuff(lfull, fil);

    // tester filters (URLs autorisées ou interdites explicitement)

    // si lien primaire on saute le joker, on est pas lémur
    if (ptr == 0) {             // lien primaire, autoriser
      question = 1;             // la question sera résolue automatiquement
      forbidden_url = 0;
      may_set_prio_to = 0;      // clear may-set flag
    } else {
      // eternal depth first
      // vérifier récursivité extérieure
      if (opt->extdepth > 0) {
        if ( /*question && */ (ptr > 0) && (!force_mirror)) {
          // well, this is kinda a hak
          // we don't want to mirror EVERYTHING, and we have to decide where to stop
          // there is no way yet to tag "external" links, and therefore links that are
          // "weak" (authorized depth < external depth) are just not considered for external
          // hack
          if (heap(ptr)->depth > opt->extdepth) {
            // *set_prio_to = opt->extdepth + 1;
            *set_prio_to = 1 + (opt->extdepth);
            may_set_prio_to = 0;        // clear may-set flag
            forbidden_url = 0;  // autorisé
            question = 0;       // résolution auto
            if (question) {
              hts_log_print(opt, LOG_DEBUG,
                            "(wizard) ambiguous link accepted (external depth): link %s at %s%s",
                            l, urladr(), urlfil());
            } else {
              hts_log_print(opt, LOG_DEBUG,
                            "(wizard) forced to accept link (external depth): link %s at %s%s",
                            l, urladr(), urlfil());
            }

          }
        }
      }
      // filters
      {
        int jok;
        const char *mdepth = "";

        // filters, 0=sait pas 1=ok -1=interdit
        {
          int jokDepth = 0;

          jok = fa_strjoker_dual(/*url */ 0, _FILTERS, *_FILTERS_PTR, lfull, l,
                                 NULL, NULL, &jokDepth);
          mdepth = _FILTERS[jokDepth];
        }

        if (jok == 1) {         // autorisé
          filters_answer = 1;   // décision prise par les filtres
          question = 0;         // ne pas poser de question, autorisé
          forbidden_url = 0;    // URL autorisée
          may_set_prio_to = 0;  // clear may-set flag
          hts_log_print(opt, LOG_DEBUG,
                        "(wizard) explicit authorized (%s) link: link %s at %s%s",
                        mdepth, l, urladr(), urlfil());
        } else if (jok == -1) { // forbidden
          filters_answer = 1;   // décision prise par les filtres
          question = 0;         // ne pas poser de question:
          forbidden_url = 1;    // URL interdite
          hts_log_print(opt, LOG_DEBUG,
                        "(wizard) explicit forbidden (%s) link: link %s at %s%s",
                        mdepth, l, urladr(), urlfil());
        }                       // sinon on touche à rien
      }
    }

    // vérifier mode mirror links
    if (question) {
      if (opt->mirror_first_page) {     // mode mirror links
        if (heap(ptr)->precedent == 0) {       // parent=primary!
          forbidden_url = 0;    // autorisé
          may_set_prio_to = 0;  // clear may-set flag
          question = 1;         // résolution auto
          force_mirror = 5;     // mirror (5)
          hts_log_print(opt, LOG_DEBUG,
                        "(wizard) explicit mirror link: link %s at %s%s", l,
                        urladr(), urlfil());
        }
      }
    }
    // on doit poser la question.. peut on la poser?
    // (oui je sais quel preuve de délicatesse, merci merci)      
    if ((question) && (ptr > 0) && (!force_mirror)) {
      if (opt->wizard == HTS_WIZARD_AUTO) {
        question = 0;
        forbidden_url = 1;
        hts_log_print(opt, LOG_DEBUG,
                      "(wizard) ambiguous forbidden link: link %s at %s%s", l,
                      urladr(), urlfil());
      }
    }
    // vérifier robots.txt
    if (opt->robots && checkrobots(_ROBOTS, adr, fil) == -1) {
#if DEBUG_ROBOTS
      printf("robots.txt forbidden: %s%s\n", adr, fil);
#endif
      if (!hts_robots_forbids(opt, adr, fil,
                              (!question && filters_answer) ? HTS_TRUE
                                                            : HTS_FALSE,
                              (forbidden_url == 1) ? HTS_TRUE : HTS_FALSE)) {
        if (!forbidden_url) {
          hts_log_print(
              opt, LOG_DEBUG,
              "Warning link followed against robots.txt: link %s at %s%s", l,
              adr, fil);
        }
      } else {
        forbidden_url = 1;
        question = 0;
        hts_log_print(opt, LOG_DEBUG,
                      "(robots.txt) forbidden link: link %s at %s%s", l, adr,
                      fil);
      }
    }

    if (!question) {
      if (!forbidden_url) {
        hts_log_print(opt, LOG_DEBUG,
                      "(wizard) shared foreign domain link: link %s at %s%s", l,
                      urladr(), urlfil());
      } else {
        hts_log_print(opt, LOG_DEBUG,
                      "(wizard) cancelled foreign domain link: link %s at %s%s",
                      l, urladr(), urlfil());
      }
#if BDEBUG==3
      printf("at %s in %s, wizard says: url %s ", urladr(), urlfil(), l);
      if (forbidden_url)
        printf("cancelled");
      else
        printf(">SHARED<");
      printf("\n");
#endif
    }

    /* en cas de question, ou lien primaire (enregistrer autorisations) */
    if (question || (ptr == 0)) {
      const char *s = NULL; /* the front end's raw reply, NULL if unasked */
      int n = 0;

      // si primaire (plus bas) alors ...
      if ((ptr != 0) && (force_mirror == 0)) {
        char BIGSTK tempo[HTS_URLMAXSIZE * 2];

        hts_wizard_prompt_url(tempo, sizeof(tempo), adr, fil);
        s = RUN_CALLBACK1(opt, query3, tempo);
        if (strnotempty(s) == 0)        // entrée
          n = 0;
        else if (isdigit((unsigned char) *s))
          sscanf(s, "%d", &n);
        else {
          switch (*s) {
          case '*':
            n = -1;
            break;
          case '!':
            n = -999; {
              /*char *a;
                 int i;                                    
                 a=copie_de_adr-128;
                 if (a<r.adr) a=r.adr;
                 for(i=0;i<256;i++) {
                 if (a==copie_de_adr) printf("\nHERE:\n");
                 printf("%c",*a++);
                 }
                 printf("\n\n");
               */
            }
            break;
          default:
            n = -999;
            printf("What did you say?\n");
            break;

          }
        }
        io_flush;
      } else {                  // lien primaire: autoriser répertoire entier       
        if (!force_mirror) {
          if ((opt->seeker & HTS_SEEKER_DOWN) == 0) {
            n = 7;
          } else {
            n = 5;              // autoriser miroir répertoires descendants (lien primaire)
          }
        } else                  // forcer valeur (sub-wizard)
          n = force_mirror;
      }

      hts_wizard_apply_verdict(opt, n, adr, fil, &forbidden_url, set_prio_to);
      {
        const int inserted = hts_wizard_insert_filters(
            opt, n, adr, fil,
            (opt->seeker & HTS_SEEKER_UP) != 0 ? HTS_TRUE : HTS_FALSE);

        /* the built-in query3 answers "" for nobody, so ask who replied */
        if (s != NULL && HAS_CALLBACK(opt, query3)) {
          char BIGSTK list[HTS_WIZARD_MAX_FILTERS * (HTS_FILTER_SLOT_SIZE + 1)];
          htsbuff added = htsbuff_array(list);
          int slot;

          /* read the slots back, so the log cannot drift from them */
          for (slot = opt->wizard_filters - inserted;
               slot < opt->wizard_filters; slot++) {
            if (added.len != 0)
              htsbuff_cat(&added, " ");
            htsbuff_cat(&added, _FILTERS[slot]);
          }
          hts_log_print(
              opt, LOG_NOTICE, "(wizard) answer '%s' (n=%d) for %s%s: %s%s%s",
              s, n, adr, fil,
              forbidden_url == 1   ? "forbidden"
              : forbidden_url == 0 ? "allowed"
                                   : "no verdict",
              added.len != 0 ? ", filters: " : "", htsbuff_str(&added));
        }
      }

    }                           // test du wizard sur l'url
  }                             // fin du test wizard..

  // -------------------- PHASE 5 --------------------

  // lien non autorisé, peut-on juste le tester?
  if (just_test_it) {
    if (forbidden_url == 1) {
      if (opt->travel & HTS_TRAVEL_TEST_ALL) { // tester tout de même
        if (strfield(adr, "ftp://") == 0) {                   // PAS ftp!
          forbidden_url = 1;    // oui oui toujours interdit (note: sert à rien car ==1 mais c pour comprendre)
          *just_test_it = 1;    // mais on teste
          hts_log_print(opt, LOG_DEBUG, "Testing link %s%s", adr, fil);
        }
      }
    }
  }
  // -------------------- FINAL PHASE --------------------
  // Test if the "Near" test won
  if (may_set_prio_to && forbidden_url == 0) {
    *set_prio_to = may_set_prio_to;
  }

  return forbidden_url;
#undef _FILTERS
#undef _FILTERS_PTR
#undef _ROBOTS
}

int hts_acceptmime(httrackp * opt, int ptr,
                   const char *adr, const char *fil, const char *mime) {
#define _FILTERS     (*opt->filters.filters)
#define _FILTERS_PTR (opt->filters.filptr)
#define _ROBOTS      ((robots_wizard*)opt->robotsptr)
  int forbidden_url = -1;
  const char *mdepth = "";
  int jokDepth = 0;
  int jok = 0;

  /* Authorized ? */
  jok =
    fa_strjoker( /*mime */ 1, _FILTERS, *_FILTERS_PTR, mime, NULL, NULL,
                &jokDepth);
  if (jok != 0) {
    mdepth = _FILTERS[jokDepth];
    if (jok == 1) {             // autorisé
      forbidden_url = 0;        // URL autorisée
      hts_log_print(opt, LOG_DEBUG,
                    "(wizard) explicit authorized (%s) link %s%s: mime '%s'",
                    mdepth, adr, fil, mime);
    } else if (jok == -1) {     // forbidden
      forbidden_url = 1;        // URL interdite
      hts_log_print(opt, LOG_DEBUG,
                    "(wizard) explicit forbidden (%s) link %s%s: mime '%s'",
                    mdepth, adr, fil, mime);
    }                           // sinon on touche à rien
  }
  /* userdef test */
  {
    int test_url =
      RUN_CALLBACK4(opt, check_mime, adr, fil, mime, forbidden_url);
    if (test_url != -1) {
      forbidden_url = test_url;
    }
  }
  return forbidden_url;
#undef _FILTERS
#undef _FILTERS_PTR
#undef _ROBOTS
}

hts_boolean hts_link_is_foreign_asset(httrackp *opt, int ptr) {
  int parent;

  if (ptr <= 0)
    return HTS_FALSE;
  parent = heap(ptr)->precedent;
  /* Seeds hang off the synthetic primary link 0: the user asked for those. */
  if (parent <= 0 || parent >= opt->lien_tot)
    return HTS_FALSE;
  if (ishtml(opt, heap(ptr)->fil) != 0) /* hypertext, or no telling */
    return HTS_FALSE;
  /* adr carries the scheme and any user:pw@, so compare bare authorities. */
  return strfield2(jump_identification_const(heap(ptr)->adr),
                   jump_identification_const(heap(parent)->adr))
             ? HTS_FALSE
             : HTS_TRUE;
}

// tester taille
int hts_testlinksize(httrackp * opt, const char *adr, const char *fil, LLint size) {
  int jok = 0;

  if (size >= 0) {
    /* Don't enlarge: the abort gates url_savename_addstr's append (#1269). */
    char BIGSTK l[HTS_URLMAXSIZE * 2];
    char BIGSTK lfull[HTS_URLMAXSIZE * 2];

    if (size >= 0) {
      LLint sz = size;
      int size_flag = 0;

      // former URL complète du lien actuel
      strcpybuff(l, jump_identification_const(adr));
      if (*fil != '/')
        strcatbuff(l, "/");
      strcatbuff(l, fil);
      //
      if (!link_has_authority(adr))
        strcpybuff(lfull, "http://");
      else
        lfull[0] = '\0';
      strcatbuff(lfull, adr);
      if (*fil != '/')
        strcatbuff(lfull, "/");
      strcatbuff(lfull, fil);

      // filters, 0=sait pas 1=ok -1=interdit
      {
        sz = size;
        jok = fa_strjoker_dual(/*url */ 0, *opt->filters.filters,
                               *opt->filters.filptr, lfull, l, &sz, &size_flag,
                               NULL);
      }

      // log
      if (jok == 1) {
        hts_log_print(opt, LOG_DEBUG,
                      "File confirmed (size test): %s%s (" LLintP ")", adr, fil,
                      (LLint) (size));
      } else if (jok == -1) {
        if (size_flag) {        /* interdit à cause de la taille */
          hts_log_print(opt, LOG_DEBUG,
                        "File cancelled due to its size: %s%s (" LLintP
                        ", limit: " LLintP ")", adr, fil, (LLint) (size),
                        (LLint) (sz));
        } else {
          jok = 1;
        }
      }
    }
  }
  return jok;
}
