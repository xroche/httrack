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
/*       wizard system (accept/refuse links)                    */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Internal engine bytecode */
#define HTS_INTERNAL_BYTECODE

#include "htscore.h"
#include "htswizard.h"

/* specific definitions */
#include "htsbase.h"
#include <ctype.h>
/* END specific definitions */

// libérer filters[0] pour insérer un élément dans filters[0]
#define HT_INSERT_FILTERS0 do {\
  int i;\
  if (*opt->filters.filptr > 0) {\
    for(i = (*opt->filters.filptr)-1 ; i>=0 ; i--) {\
      strcpybuff((*opt->filters.filters)[i+1],(*opt->filters.filters)[i]);\
    }\
  }\
  (*opt->filters.filters)[0][0]='\0';\
  (*opt->filters.filptr)++;\
  assertf((*opt->filters.filptr) < opt->maxfilter); \
} while(0)

typedef struct htspair_t {
  const char *tag;
  const char *attr;
} htspair_t;

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

static int cmp_token(const char *tag, const char *cmp) {
  int p;

  return (strncasecmp(tag, cmp, (p = (int) strlen(cmp))) == 0
          && !isalnum((unsigned char) tag[p]));
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
    int i;

    for(i = 0; hts_detect_embed[i].tag != NULL; i++) {
      if (cmp_token(tag, hts_detect_embed[i].tag)
          && cmp_token(attribute, hts_detect_embed[i].attr)
        ) {
        embedded_triggered = 1;
        break;
      }
    }
  }

  // -------------------- PHASE 1 --------------------

  /* Doit-on traiter les non html? */
  if ((opt->getmode & 2) == 0) {        // non on ne doit pas
    if (!ishtml(opt, fil)) {    // non il ne faut pas
      //adr[0]='\0';    // ne pas traiter ce lien, pas traiter
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
  meme_adresse = strfield2(adr, urladr());
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

      if (lienrelatif(tempo, fil, heap(heap(ptr)->premier)->fil) == 0) {
        if (lienrelatif(tempo2, fil, heap(ptr)->fil) == 0) {
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
              if ((opt->seeker & 1) == 0) {     // interdiction de descendre
                forbidden_url = 1;
                hts_log_print(opt, LOG_DEBUG, "lower link canceled: %s%s", adr,
                              fil);
              } else {          // autorisé à priori - NEW
                if (!heap(ptr)->link_import) { // ne résulte pas d'un 'moved'
                  forbidden_url = 0;
                  hts_log_print(opt, LOG_DEBUG, "lower link authorized: %s%s",
                                adr, fil);
                }
              }
            } else if ((test1) || (test2)) {    // on peut descendre pour accéder au lien
              if ((opt->seeker & 1) != 0) {     // on peut descendre - NEW
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
            if ((opt->seeker & 2) == 0) {       // interdiction de monter
              forbidden_url = 1;
              hts_log_print(opt, LOG_DEBUG, "upper link canceled: %s%s", adr,
                            fil);
            } else {            // autorisé à monter - NEW
              if (!heap(ptr)->link_import) {   // ne résulte pas d'un 'moved'
                forbidden_url = 0;
                hts_log_print(opt, LOG_DEBUG, "upper link authorized: %s%s",
                              adr, fil);
              }
            }
          } else if ((!strncmp(tempo, "../", 3)) || (!strncmp(tempo2, "../", 3))) {     // Possible en montant
            if ((opt->seeker & 2) != 0) {       // autorisé à monter - NEW
              if (!heap(ptr)->link_import) {   // ne résulte pas d'un 'moved'
                forbidden_url = 0;
                hts_log_print(opt, LOG_DEBUG, "upper link authorized: %s%s",
                              adr, fil);
              }
            }                   // sinon autorisé en descente
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

    {                           // tester interdiction de monter
      char BIGSTK tempo[HTS_URLMAXSIZE * 2];
      char BIGSTK tempo2[HTS_URLMAXSIZE * 2];

      if (lienrelatif(tempo, fil, heap(heap(ptr)->premier)->fil) == 0) {
        if (lienrelatif(tempo2, fil, heap(ptr)->fil) == 0) {
        } else {
          hts_log_print(opt, LOG_ERROR,
                        "Error building relative link %s and %s", fil,
                        heap(ptr)->fil);
        }
      } else {
        hts_log_print(opt, LOG_ERROR, "Error building relative link %s and %s",
                      fil, heap(heap(ptr)->premier)->fil);

      }
    }                           // fin tester interdiction de monter

  } else {                      // adresse différente, sortir?

    //if (!opt->wizard) {    // mode non wizard
    // doit-on traiter ce lien?.. vérifier droits de sortie
    switch ((opt->travel & 255)) {
    case 0:
      if (!opt->wizard)         // mode non wizard
        forbidden_url = 1;
      break;                    // interdicton de sortir au dela de l'adresse
    case 1:{                   // sortie sur le même dom.xxx
        size_t i = strlen(adr) - 1;
        size_t j = strlen(urladr()) - 1;

        if ((i > 0) && (j > 0)) {
          while((i > 0) && (adr[i] != '.'))
            i--;
          while((j > 0) && (urladr()[j] != '.'))
            j--;
          if ((i > 0) && (j > 0)) {
            i--;
            j--;
            while((i > 0) && (adr[i] != '.'))
              i--;
            while((j > 0) && (urladr()[j] != '.'))
              j--;
          }
        }
        if ((i > 0) && (j > 0)) {
          if (!strfield2(adr + i, urladr() + j)) {        // !=
            if (!opt->wizard) { // mode non wizard
              //printf("refused: %s\n",adr);
              forbidden_url = 1;        // pas même domaine  
              hts_log_print(opt, LOG_DEBUG,
                            "foreign domain link canceled: %s%s", adr, fil);
            }

          } else {
            if (opt->wizard) {  // mode wizard
              forbidden_url = 0;        // même domaine  
              hts_log_print(opt, LOG_DEBUG, "same domain link authorized: %s%s",
                            adr, fil);
            }
          }

        } else
          forbidden_url = 1;
      }
      break;
    case 2:{                   // sortie sur le même .xxx
        size_t i = strlen(adr) - 1;
        size_t j = strlen(urladr()) - 1;

        while((i > 0) && (adr[i] != '.'))
          i--;
        while((j > 0) && (urladr()[j] != '.'))
          j--;
        if ((i > 0) && (j > 0)) {
          if (!strfield2(adr + i, urladr() + j)) {        // !-
            if (!opt->wizard) { // mode non wizard
              //printf("refused: %s\n",adr);
              forbidden_url = 1;        // pas même .xx  
              hts_log_print(opt, LOG_DEBUG,
                            "foreign location link canceled: %s%s", adr, fil);
            }
          } else {
            if (opt->wizard) {  // mode wizard
              forbidden_url = 0;        // même domaine  
              hts_log_print(opt, LOG_DEBUG,
                            "same location link authorized: %s%s", adr, fil);
            }
          }
        } else
          forbidden_url = 1;
      }
      break;
    case 7:                    // everywhere!!
      if (opt->wizard) {        // mode wizard
        forbidden_url = 0;
        break;
      }
    }                           // switch

    // ANCIENNE POS -- récupérer les liens à côtés d'un lien (nearlink)

  }                             // fin test adresse identique/différente

  // -------------------- PHASE 3 --------------------

  // récupérer les liens à côtés d'un lien (nearlink) (nvelle pos)
  if (forbidden_url != 0 && opt->nearlink) {
    if (!ishtml(opt, fil)) {    // non html
      //printf("ok %s%s\n",ad,fil);
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
          int jokDepth1 = 0, jokDepth2 = 0;
          int jok1 = 0, jok2 = 0;

          jok1 =
            fa_strjoker( /*url */ 0, _FILTERS, *_FILTERS_PTR, lfull, NULL, NULL,
                        &jokDepth1);
          jok2 =
            fa_strjoker( /*url */ 0, _FILTERS, *_FILTERS_PTR, l, NULL, NULL,
                        &jokDepth2);
          if (jok2 == 0) {      // #2 doesn't know
            jok = jok1;         // then, use #1
            mdepth = _FILTERS[jokDepth1];
          } else if (jok1 == 0) {       // #1 doesn't know
            jok = jok2;         // then, use #2
            mdepth = _FILTERS[jokDepth2];
          } else if (jokDepth1 >= jokDepth2) {  // #1 matching rule is "after" #2, then it is prioritary
            jok = jok1;
            mdepth = _FILTERS[jokDepth1];
          } else {              // #2 matching rule is "after" #1, then it is prioritary
            jok = jok2;
            mdepth = _FILTERS[jokDepth2];
          }
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
      if (opt->wizard == 2) {   // éliminer tous les liens non répertoriés comme autorisés (ou inconnus)
        question = 0;
        forbidden_url = 1;
        hts_log_print(opt, LOG_DEBUG,
                      "(wizard) ambiguous forbidden link: link %s at %s%s", l,
                      urladr(), urlfil());
      }
    }
    // vérifier robots.txt
    if (opt->robots) {
      int r = checkrobots(_ROBOTS, adr, fil);

      if (r == -1) {            // interdiction
#if DEBUG_ROBOTS
        printf("robots.txt forbidden: %s%s\n", adr, fil);
#endif
        // question résolue, par les filtres, et mode robot non strict
        if ((!question) && (filters_answer) && (opt->robots == 1)
            && (forbidden_url != 1)) {
          r = 0;                // annuler interdiction des robots
          if (!forbidden_url) {
            hts_log_print(opt, LOG_DEBUG,
                          "Warning link followed against robots.txt: link %s at %s%s",
                          l, adr, fil);
          }
        }
        if (r == -1) {          // interdire
          forbidden_url = 1;
          question = 0;
          hts_log_print(opt, LOG_DEBUG,
                        "(robots.txt) forbidden link: link %s at %s%s", l, adr,
                        fil);
        }
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
      const char *s;
      int n = 0;

      // si primaire (plus bas) alors ...
      if ((ptr != 0) && (force_mirror == 0)) {
        char BIGSTK tempo[HTS_URLMAXSIZE * 2];

        tempo[0] = '\0';
        strcatbuff(tempo, adr);
        strcatbuff(tempo, fil);
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
          if ((opt->seeker & 1) == 0) { // interdiction de descendre
            n = 7;
          } else {
            n = 5;              // autoriser miroir répertoires descendants (lien primaire)
          }
        } else                  // forcer valeur (sub-wizard)
          n = force_mirror;
      }

      /* sanity check - reallocate filters HERE */
      if ((*_FILTERS_PTR) + 1 >= opt->maxfilter) {
        opt->maxfilter += HTS_FILTERSINC;
        if (filters_init(&_FILTERS, opt->maxfilter, HTS_FILTERSINC) == 0) {
          printf("PANIC! : Too many filters : >%d [%d]\n", (*_FILTERS_PTR),
                 __LINE__);
          fflush(stdout);
          hts_log_print(opt, LOG_PANIC, "Too many filters, giving up..(>%d)",
                        (*_FILTERS_PTR));
          hts_log_print(opt, LOG_INFO,
                        "To avoid that: use #F option for more filters (example: -#F5000)");
          assertf("too many filters - giving up" == NULL);      // wild..
        }
      }
      // here we have enough room for a new filter if necessary
      switch (n) {
      case -1:                 // sauter tout le reste
        forbidden_url = 1;
        opt->wizard = 2;        // sauter tout le reste
        break;
      case 0:                  // interdire les mêmes liens: adr/fil
        forbidden_url = 1;
        HT_INSERT_FILTERS0;     // insérer en 0
        strcpybuff(_FILTERS[0], "-");
        strcatbuff(_FILTERS[0], jump_identification_const(adr));
        if (*fil != '/')
          strcatbuff(_FILTERS[0], "/");
        strcatbuff(_FILTERS[0], fil);
        break;

      case 1:                  // éliminer répertoire entier et sous rép: adr/path/ *
        forbidden_url = 1;
        {
          size_t i = strlen(fil) - 1;

          while((fil[i] != '/') && (i > 0))
            i--;
          if (fil[i] == '/') {
            HT_INSERT_FILTERS0; // insérer en 0
            strcpybuff(_FILTERS[0], "-");
            strcatbuff(_FILTERS[0], jump_identification_const(adr));
            if (*fil != '/')
              strcatbuff(_FILTERS[0], "/");
            strncatbuff(_FILTERS[0], fil, i);
            if (_FILTERS[0][strlen(_FILTERS[0]) - 1] != '/')
              strcatbuff(_FILTERS[0], "/");
            strcatbuff(_FILTERS[0], "*");
          }
        }

        // ** ...
        break;

      case 2:                  // adresse adr*
        forbidden_url = 1;
        HT_INSERT_FILTERS0;     // insérer en 0                                
        strcpybuff(_FILTERS[0], "-");
        strcatbuff(_FILTERS[0], jump_identification_const(adr));
        strcatbuff(_FILTERS[0], "*");
        break;

      case 3:                  // ** A FAIRE
        forbidden_url = 1;
        /*
           {
           int i=strlen(adr)-1;
           while((adr[i]!='/') && (i>0)) i--;
           if (i>0) {

           }

           } */

        break;
        //
      case 4:                  // same link
        // PAS BESOIN!!
        /*HT_INSERT_FILTERS0;    // insérer en 0                                
           strcpybuff(_FILTERS[0],"+");
           strcatbuff(_FILTERS[0],adr);
           if (*fil!='/') strcatbuff(_FILTERS[0],"/");
           strcatbuff(_FILTERS[0],fil); */

        // étant donné le renversement wizard/primary filter (les primary autorisent up/down ET interdisent)
        // il faut éviter d'un lien isolé effectue un miroir total..

        *set_prio_to = 0 + 1;   // niveau de récursion=0 (pas de miroir)

        break;

      case 5:                  // autoriser répertoire entier et fils
        if ((opt->seeker & 2) == 0) {   // interdiction de monter
          size_t i = strlen(fil) - 1;

          while((fil[i] != '/') && (i > 0))
            i--;
          if (fil[i] == '/') {
            HT_INSERT_FILTERS0; // insérer en 0                                
            strcpybuff(_FILTERS[0], "+");
            strcatbuff(_FILTERS[0], jump_identification_const(adr));
            if (*fil != '/')
              strcatbuff(_FILTERS[0], "/");
            strncatbuff(_FILTERS[0], fil, i + 1);
            strcatbuff(_FILTERS[0], "*");
          }
        } else {                // autoriser domaine alors!!
          HT_INSERT_FILTERS0;   // insérer en 0                                strcpybuff(filters[filptr],"+");
          strcpybuff(_FILTERS[0], "+");
          strcatbuff(_FILTERS[0], jump_identification_const(adr));
          strcatbuff(_FILTERS[0], "*");
        }
        break;

      case 6:                  // same domain
        HT_INSERT_FILTERS0;     // insérer en 0                                strcpybuff(filters[filptr],"+");
        strcpybuff(_FILTERS[0], "+");
        strcatbuff(_FILTERS[0], jump_identification_const(adr));
        strcatbuff(_FILTERS[0], "*");
        break;
        //
      case 7:                  // autoriser ce répertoire
        {
          size_t i = strlen(fil) - 1;

          while((fil[i] != '/') && (i > 0))
            i--;
          if (fil[i] == '/') {
            HT_INSERT_FILTERS0; // insérer en 0                                
            strcpybuff(_FILTERS[0], "+");
            strcatbuff(_FILTERS[0], jump_identification_const(adr));
            if (*fil != '/')
              strcatbuff(_FILTERS[0], "/");
            strncatbuff(_FILTERS[0], fil, i + 1);
            strcatbuff(_FILTERS[0], "*[file]");
          }
        }

        break;

      case 50:                 // on fait rien
        break;
      }                         // switch 

    }                           // test du wizard sur l'url
  }                             // fin du test wizard..

  // -------------------- PHASE 5 --------------------

  // lien non autorisé, peut-on juste le tester?
  if (just_test_it) {
    if (forbidden_url == 1) {
      if (opt->travel & 256) {  // tester tout de même
        if (strfield(adr, "ftp://") == 0) {                   // PAS ftp!
          forbidden_url = 1;    // oui oui toujours interdit (note: sert à rien car ==1 mais c pour comprendre)
          *just_test_it = 1;    // mais on teste
          hts_log_print(opt, LOG_DEBUG, "Testing link %s%s", adr, fil);
        }
      }
    }
    //adr[0]='\0';  // cancel
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

// tester taille
int hts_testlinksize(httrackp * opt, const char *adr, const char *fil, LLint size) {
  int jok = 0;

  if (size >= 0) {
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
        strcatbuff(l, "/");
      strcatbuff(lfull, fil);

      // filters, 0=sait pas 1=ok -1=interdit
      {
        int jokDepth1 = 0, jokDepth2 = 0;
        int jok1 = 0, jok2 = 0;
        LLint sz1 = size, sz2 = size;
        int size_flag1 = 0, size_flag2 = 0;

        jok1 =
          fa_strjoker( /*url */ 0, *opt->filters.filters, *opt->filters.filptr,
                      lfull, &sz1, &size_flag1, &jokDepth1);
        jok2 =
          fa_strjoker( /*url */ 0, *opt->filters.filters, *opt->filters.filptr,
                      l, &sz2, &size_flag2, &jokDepth2);
        if (jok2 == 0) {        // #2 doesn't know
          jok = jok1;           // then, use #1
          sz = sz1;
          size_flag = size_flag1;
        } else if (jok1 == 0) { // #1 doesn't know
          jok = jok2;           // then, use #2
          sz = sz2;
          size_flag = size_flag2;
        } else if (jokDepth1 >= jokDepth2) {    // #1 matching rule is "after" #2, then it is prioritary
          jok = jok1;
          sz = sz1;
          size_flag = size_flag1;
        } else {                // #2 matching rule is "after" #1, then it is prioritary
          jok = jok2;
          sz = sz2;
          size_flag = size_flag2;
        }
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

#undef HT_INSERT_FILTERS0
