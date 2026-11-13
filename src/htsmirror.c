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
/* File: htsmirror.c mirror loop                                */
/*       redirect handling, user interaction, queue wait        */
/*       and delayed-type resolution                            */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include <fcntl.h>
#include <ctype.h>

/* File defs */
#include "htscore.h"

/* specific definitions */
#include "htsbase.h"
#include "htsio.h"
#include "htsnet.h"
#include "htsbauth.h"
#include "htsmd5.h"
#include "htsindex.h"
#include "htslines.h"
#include "htscharset.h"
#include "htsencoding.h"
#include "htssniff.h"
#include "htsname.h"
#include "htsthread.h"

/* external modules */
#include "htsmodules.h"

// htswrap_add
#include "htswrap.h"

// parser
#include "htsparse.h"
#include "htsmirror.h"
#include "htssinglefile.h"
#include "htsback.h"
#include "htsfetch.h"

// arrays
#include "htsarrays.h"

/* Drop a redirect Location's #fragment: a UA anchor, never part of the fetched
 * resource (#204). */
static void url_drop_fragment(char *const url) {
  char *const frag = strchr(url, '#');
  if (frag != NULL)
    *frag = '\0';
}

/* A copy httrack parsed as hypertext has no NUL left in it, the parse path
   blanks them on the way to disk. One surviving proves no run read links out of
   this file, whatever its recorded type and name claim (#1415). */
static hts_boolean hts_mirrored_copy_is_binary(const char *save) {
  char head[HTS_SNIFF_LEN];
  const size_t n = hts_read_file_head(save, head, sizeof(head));

  /* nothing readable reads as not-binary, so the guard still holds */
  return memchr(head, '\0', n) != NULL ? HTS_TRUE : HTS_FALSE;
}

/* Does this link have a mirrored subtree the purge could take? A previous run's
   hypertext page does, and so does a redirect, whose target and everything
   below it reached the mirror through here alone (#1395). */
static hts_boolean hts_link_may_carry_links(httrackp *opt, cache_back *cache,
                                            const char *adr, const char *fil,
                                            const char *save) {
  char BIGSTK prev_save[HTS_URLMAXSIZE * 2];
  char BIGSTK prev_location[HTS_LOCATION_SIZE];
  char guessed[256];
  htsblk prev;

  /* a cache miss leaves both untouched */
  prev_save[0] = prev_location[0] = '\0';
  prev = cache_read_including_broken(opt, cache, adr, fil, prev_save,
                                     prev_location);

  if (HTTP_IS_REDIRECT(prev.statuscode))
    return HTS_TRUE;
  if (prev.statuscode > 0) /* answered: an error mirrored nothing */
    return HTTP_IS_OK(prev.statuscode) && strnotempty(prev_save) &&
           is_hypertext_mime(opt, prev.contenttype, prev_save) &&
           !hts_mirrored_copy_is_binary(prev_save);

  /* No usable status left. A redirect is stored headers-only, so naming no
     local file invalidates it on read, but its Location survives. Below this
     arm only, or a stray Location on an answered blob would hold too. */
  if (strnotempty(prev_location))
    return HTS_TRUE;

  /* A run that failed the same way stored no entry at all, so the copy it kept
     (#746) is the only surviving record of what this page was. Both arms that
     type a file by its name check its bytes: the name can be the sentinel's. */
  guessed[0] = '\0';
  return save != NULL && strnotempty(save) && fexist_utf8(save) &&
         guess_httptype_sized(opt, guessed, sizeof(guessed), save) &&
         is_hypertext_mime__(guessed) && !hts_mirrored_copy_is_binary(save);
}

/*
Check 301, 302, .. statuscodes (moved)
*/
int hts_mirror_check_moved(htsmoduleStruct *str,
                           htsmoduleStructExtended *stre) {
  /* Load engine variables */
  ENGINE_LOAD_CONTEXT();

  // DEBUT rattrapage des 301,302,307..
  // ------------------------------------------------------------
  if (!error) {

    // ------------------------------------------------------------
    // Rattrapage des 301,302,307 (moved) et 412,416 - les 304 le sont dans le
    // backing
    // ------------------------------------------------------------
    if (HTTP_IS_REDIRECT(r->statuscode)) {

      hts_log_print(opt, LOG_WARNING, "%s for %s%s", r->msg,
                    adr_nologin(opt, urladr()), urlfil());

      {
        char BIGSTK mov_url[HTS_URLMAXSIZE * 2];
        lien_adrfilsave savedmoved;
        lien_adrfil *const moved = &savedmoved.af;
        int get_it =
            0; // ne pas prendre le fichier à la même adresse par défaut
        int reponse = 0;

        mov_url[0] = '\0';
        moved->adr[0] = '\0';
        moved->fil[0] = '\0';
        savedmoved.save[0] = '\0';
        //

        strcpybuff(mov_url, r->location);
        url_drop_fragment(mov_url);

        // any url -> address+file
        if ((reponse = ident_url_relatif(mov_url, urladr(), urlfil(), moved)) >=
            0) {
          int set_prio_to = 0; // no priority set by the wizard
          // A same-file alias redirect must be followed, not stubbed (#159).
          const hts_boolean same_savefile = hts_redirect_same_savefile(
              opt, urladr(), urlfil(), moved->adr, moved->fil);
          // this is (roughly) the same URL..
          // a case problem in the host means the server is buggy
          // ("RFC says.." : host name IS case insensitive)
          // identical up to case, so we are going in circles
          if ((strfield2(moved->adr, urladr()) != 0) &&
              (strfield2(moved->fil, urlfil()) != 0)) {
            if (strcmp(moved->fil, urlfil()) == 0) {
              error = 1;
              get_it = -1; // do nothing
              hts_log_print(opt, LOG_WARNING,
                            "Can not bear crazy server (%s) for %s%s", r->msg,
                            adr_nologin(opt, urladr()), urlfil());
            } else { // wrong case: erase the entry in the stack and replay once
              get_it = 1;
            }
          } else { // different address
            // not the same address BUT a non-html file (no moved page possible)
            if (ishtml(opt, mov_url) == 0) {
              // -> take it at this address, the link is recorded with
              // lien_record() (hash)
              hts_log_print(opt, LOG_DEBUG,
                            "wizard link test for moved file at %s%s..",
                            adr_nologin(opt, moved->adr), moved->fil);
              // accepted? (new address not refused)
              if (hts_acceptlink(opt, ptr, moved->adr, moved->fil, NULL, NULL,
                                 HTS_FALSE, &set_prio_to, NULL) != 1) {
                get_it = 1;
                hts_log_print(opt, LOG_DEBUG, "moved link accepted: %s%s",
                              adr_nologin(opt, moved->adr), moved->fil);
              }
            } else if (same_savefile) {
              // A stub would point at itself; follow the redirect instead.
              if (hts_acceptlink(opt, ptr, moved->adr, moved->fil, NULL, NULL,
                                 HTS_FALSE, &set_prio_to, NULL) != 1) {
                get_it = 1;
                hts_log_print(opt, LOG_WARNING,
                              "Redirect to a same-file alias, fetching real "
                              "content: %s%s -> %s%s",
                              adr_nologin(opt, urladr()), urlfil(),
                              adr_nologin(opt, moved->adr), moved->fil);
              }
            } /* sinon traité normalement */
          }

          if (get_it == 1) {
            // court-circuiter le reste du traitement
            // et reculer pour mieux sauter
            hts_log_print(opt, LOG_WARNING,
                          "Warning moved treated for %s%s (real one is %s%s)",
                          adr_nologin(opt, urladr()), urlfil(),
                          adr_nologin(opt, moved->adr), moved->fil);
            // canceller lien actuel
            error = 1;
            hts_invalidate_link(opt, ptr); // invalidate hashtable entry
            // noter NOUVEAU lien
            {
              // calculer lien et éventuellement modifier addresse/fichier
              if (url_savename(&savedmoved, NULL,
                               heap(heap(ptr)->precedent)->adr,
                               heap(heap(ptr)->precedent)->fil, opt, sback,
                               cache, hash, ptr, numero_passe, NULL) != -1) {
                // Same-file alias: the reserved name is the invalidated source,
                // so record anyway.
                if (same_savefile ||
                    hash_read(hash, savedmoved.save, NULL,
                              HASH_STRUCT_FILENAME) < 0) { // n'existe pas déja
                  // enregistrer lien avec SAV IDENTIQUE
                  if (hts_record_link(opt, moved->adr, moved->fil,
                                      heap(ptr)->sav, "", "", NULL)) {
                    // mode test?
                    heap_top()->testmode = heap(ptr)->testmode;
                    heap_top()->link_import = 0; // mode normal
                    if (!set_prio_to)
                      heap_top()->depth = heap(ptr)->depth;
                    else
                      heap_top()->depth = max(
                          0,
                          min(set_prio_to - 1,
                              heap(ptr)->depth)); // PRIORITE NULLE (catch page)
                    heap_top()->pass2 = max(heap(ptr)->pass2, numero_passe);
                    heap_top()->retry = heap(ptr)->retry;
                    // same transfer under another name: carry the restart latch
                    // too, else a 206/alias alternation restarts for free
                    heap_top()->refetch_whole = heap(ptr)->refetch_whole;
                    // Sec-Fetch-Site takes the worst hop of the chain
                    heap_top()->fetch = heap(ptr)->fetch;
                    hts_fetch_through(opt, &heap_top()->fetch, heap(ptr)->adr);
                    heap_top()->premier = heap(ptr)->premier;
                    heap_top()->precedent = heap(ptr)->precedent;
                  } else { // oups erreur, plus de mémoire!!
                    return 0;
                  }
                } else {
                  hts_log_print(opt, LOG_INFO,
                                "moving %s to an existing file %s",
                                heap(ptr)->fil, urlfil());
                }
              }
            }

            // note métaphysique: il se peut qu'il y ait un index.html et un
            // INDEX.HTML sous DOS ca marche pas très bien... mais comme je suis
            // génial url_savename() est à même de régler ce problème
          }
        } // ident_url_xx

        if (get_it == 0) { // adresse vraiment différente et potentiellement en
                           // html (pas de possibilité de bouger la page tel
                           // quel à cause des <img src..> et cie)
          const size_t rn_size = 8192;
          char *const rn = (char *) malloct(rn_size);
          if (rn != NULL) {
            hts_log_print(opt, LOG_WARNING, "File has moved from %s%s to %s",
                          adr_nologin(opt, urladr()), urlfil(),
                          url_nologin(opt, mov_url));
            if (!opt->mimehtml) {
              inplace_escape_uri(mov_url, sizeof(mov_url));
            } else {
              char BIGSTK cid[HTS_URLMAXSIZE * 3];
              make_content_id(moved->adr, moved->fil, cid, sizeof(cid));
              strcpybuff(mov_url, "cid:");
              strcatbuff(mov_url, cid);
            }
            // On prépare une page qui sautera immédiatement sur la bonne URL
            // Le scanner re-changera, ensuite, cette URL, pour la mirrorer!
            snprintf(rn, rn_size,
                     "<HTML>" CRLF
                     "<!-- Created by HTTrack Website Copier/" HTTRACK_VERSION
                     " " HTTRACK_AFF_AUTHORS " -->" CRLF "<HEAD>" CRLF
                     "<META HTTP-EQUIV=\"Content-Type\" "
                     "CONTENT=\"text/html;charset=UTF-8\">"
                     "<META HTTP-EQUIV=\"Refresh\" CONTENT=\"0; URL=%s\">"
                     "<TITLE>Page has moved</TITLE>" CRLF "</HEAD>" CRLF
                     "<BODY>" CRLF
                     "<A HREF=\"%s\"><h3>Click here...</h3></A>" CRLF
                     "</BODY>" CRLF
                     "<!-- Created by HTTrack Website Copier/" HTTRACK_VERSION
                     " " HTTRACK_AFF_AUTHORS " -->" CRLF "</HTML>" CRLF,
                     mov_url, mov_url);

            // changer la page
            if (r->adr) {
              freet(r->adr);
              r->adr = NULL;
            }
            r->adr = rn;
            r->size = strlen(r->adr);
            strcpybuff(r->contenttype, "text/html");
          }
        } // get_it==0

      } // bloc
      // erreur HTTP (ex: 404, not found)
    } else if ((r->statuscode == HTTP_PRECONDITION_FAILED) ||
               (r->statuscode == HTTP_REQUESTED_RANGE_NOT_SATISFIABLE)) {
      // 412/416: the resume partial is stale; re-get the whole file (#206)
      lien_back *itemback = NULL;
      int had_partial = 0;
      int ref_existed = 0;
      int ref_gone;

      // Drop the temp-ref, its partial, and heap->sav so the re-get carries no
      // Range; else back_add rebuilds the same Range and loops.
      if (back_unserialize_ref(opt, heap(ptr)->adr, heap(ptr)->fil,
                               &itemback) == 0) {
        had_partial = 1;
        ref_existed = 1;
        // best-effort: an orphaned partial cannot re-Range once the ref is gone
        if (fexist_utf8(itemback->url_sav))
          (void) UNLINK(fconv(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                              itemback->url_sav));
        back_clear_entry(itemback);
        freet(itemback);
      }
      // don't re-record if the ref survived (it would re-Range and loop)
      ref_gone =
          url_savename_refname_remove(opt, heap(ptr)->adr, heap(ptr)->fil) ||
          !ref_existed;
      if (fexist_utf8(heap(ptr)->sav)) {
        had_partial = 1;
        UNLINK(heap(ptr)->sav);
      }

      // Re-get once, only if a partial existed and both Range triggers are
      // gone; a failed removal gives up rather than looping.
      if (had_partial && ref_gone && !fexist_utf8(heap(ptr)->sav)) {
        hts_log_print(opt, LOG_DEBUG, "Partial file reget (%s) for %s%s",
                      r->msg, adr_nologin(opt, urladr()), urlfil());
        if (hts_record_link(opt, heap(ptr)->adr, heap(ptr)->fil, heap(ptr)->sav,
                            "", "", NULL)) {
          heap_top()->testmode = heap(ptr)->testmode;
          heap_top()->link_import = 0;
          heap_top()->depth = heap(ptr)->depth;
          heap_top()->pass2 = max(heap(ptr)->pass2, numero_passe);
          heap_top()->retry = heap(ptr)->retry;
          heap_top()->refetch_whole = heap(ptr)->refetch_whole;
          heap_top()->fetch = heap(ptr)->fetch;
          heap_top()->premier = heap(ptr)->premier;
          heap_top()->precedent = heap(ptr)->precedent;
          error = 1;
          hts_invalidate_link(opt, ptr); // invalidate hashtable entry
        } else {                         // out of memory
          return 0;
        }
      } else {
        hts_log_print(opt, LOG_WARNING,
                      "Giving up on partial reget (%s) for %s%s", r->msg,
                      adr_nologin(opt, urladr()), urlfil());
        error = 1;
      }

      // Error ?
      if (error) {
        if (!opt->errpage) {
          if (r->adr) { // désalloc
            freet(r->adr);
            r->adr = NULL;
          }
        }
      }
    } else if (r->statuscode != HTTP_OK) {
      int can_retry = 0;

      // cas où l'on peut reessayer
      switch (r->statuscode) {
      case STATUSCODE_TIMEOUT:
        if (opt->hostcontrol) { // timeout et retry épuisés
          if ((opt->hostcontrol & HTS_HOSTCONTROL_BAN_TIMEOUT) &&
              (heap(ptr)->retry <= 0)) {
            hts_log_print(opt, LOG_DEBUG, "Link banned: %s%s",
                          adr_nologin(opt, urladr()), urlfil());
            host_ban(opt, ptr, sback, jump_identification_const(urladr()));
            hts_log_print(opt, LOG_DEBUG,
                          "Info: previous log - link banned: %s%s",
                          adr_nologin(opt, urladr()), urlfil());
          } else
            can_retry = 1;
        } else
          can_retry = 1;
        break;
      case STATUSCODE_SLOW:
        if ((opt->hostcontrol) && (heap(ptr)->retry <= 0)) { // too slow
          if (opt->hostcontrol & HTS_HOSTCONTROL_BAN_SLOW) {
            hts_log_print(opt, LOG_DEBUG, "Link banned: %s%s",
                          adr_nologin(opt, urladr()), urlfil());
            host_ban(opt, ptr, sback, jump_identification_const(urladr()));
            hts_log_print(opt, LOG_DEBUG,
                          "Info: previous log - link banned: %s%s",
                          adr_nologin(opt, urladr()), urlfil());
          } else
            can_retry = 1;
        } else
          can_retry = 1;
        break;
      case STATUSCODE_CONNERROR: // connect closed
        can_retry = 1;
        break;
      case STATUSCODE_NON_FATAL: // other (non fatal) error
        can_retry = 1;
        break;
      case STATUSCODE_SSL_HANDSHAKE: // bad SSL handskake
        can_retry = 1;
        break;
      case 408:
      case 409:
      case 500:
      case 502:
      case 504:
        can_retry = 1;
        break;
      case 429: // rate limited, so retry once the wait below is up
        can_retry = 1;
        break;
      case 503:
        /* A 503 naming no delay is as likely a host that is down for good */
        can_retry = r->retry_after >= 0;
        break;
      }

      /* Restarting a resume the server refused is not a retry of a failed
         transfer: free, but latched to one per link so an always-unusable
         server cannot loop (#1052). */
      const hts_boolean restart_whole =
          r->refetch_wholefile && !heap(ptr)->refetch_whole;

      if (strcmp(heap(ptr)->fil, "/primary") !=
          0) { // no primary (internal page 0)
        // give up: no free restart left, and no budget (or nothing to retry)
        if (!restart_whole && ((heap(ptr)->retry <= 0) || (!can_retry))) {
          if ((opt->retry > 0) && (can_retry)) {
            hts_log_print(
                opt, LOG_ERROR,
                "\"%s\" (%d) after %d retries at link %s%s (from %s%s)", r->msg,
                r->statuscode, opt->retry, adr_nologin(opt, urladr()), urlfil(),
                adr_nologin(opt, heap(heap(ptr)->precedent)->adr),
                heap(heap(ptr)->precedent)->fil);
          } else {
            if (r->statuscode == STATUSCODE_TEST_OK) { // test OK
              hts_log_print(opt, LOG_INFO, "Test OK at link %s%s (from %s%s)",
                            adr_nologin(opt, urladr()), urlfil(),
                            adr_nologin(opt, heap(heap(ptr)->precedent)->adr),
                            heap(heap(ptr)->precedent)->fil);
            } else {
              if (strcmp(urlfil(),
                         "/robots.txt")) { // ne pas afficher d'infos sur
                                           // robots.txt par défaut
                hts_log_print(
                    opt, LOG_ERROR, "\"%s\" (%d) at link %s%s (from %s%s)",
                    r->msg, r->statuscode, adr_nologin(opt, urladr()), urlfil(),
                    adr_nologin(opt, heap(heap(ptr)->precedent)->adr),
                    heap(heap(ptr)->precedent)->fil);
              } else {
                hts_log_print(opt, LOG_DEBUG, "No robots.txt rules at %s",
                              adr_nologin(opt, urladr()));
              }
            }
          }

          // NO error in trop level
          // due to the "no connection -> previous restored" hack
          // This prevent the engine from wiping all data if the website has
          // been deleted (or moved) since last time (which is quite annoying)
          if (heap(ptr)->precedent != 0) {
            // ici on teste si on doit enregistrer la page tout de même
            if (opt->errpage) {
              store_errpage = 1;
            }
          } else {
            if (strcmp(urlfil(), "/robots.txt") != 0) {
              /*
                 This is an error caused by a link entered by the user
                 That is, link(s) entered by user are invalid (404, 500, connect
                 error, proxy error->.) If all links entered are invalid, the
                 session failed and we will attempt to restore the previous one
                 Example: Try to update a website which has been deleted
                 remotely: this may delete the website locally, which is really
                 not desired (especially if the website disappeared!) With this
                 hack, the engine won't wipe local files (how clever)
               */
              HTS_STAT.stat_errors_front++;
            }
          }

          /* A failed transfer leaves a hole in the mirror where an answered
             error does not, a body cut off after its headers included, and
             robots.txt carries no content to lose. */
          if (back_transfer_failed(r->statuscode) && !heap(ptr)->testmode &&
              strcmp(urlfil(), "/robots.txt") != 0) {
            opt->transport_failures++;
          }

          /* No response at all, so nothing here says the site dropped the
             links this page carries. Same predicate as the one keeping the
             previous copy (#746), and it must stay so: every file this run
             preserves keeps its children. An answered error is not in it and
             needs nothing, being recovered from the cache and re-parsed. */
          if (back_transfer_failed(r->statuscode) && !heap(ptr)->testmode &&
              hts_link_may_carry_links(opt, cache, urladr(), urlfil(),
                                       savename())) {
            opt->links_unqueued = HTS_TRUE;
          }

        } else { // retry, or a refused-resume restart
          /* The gate is global, so the links already queued wait too rather
             than each earning its own 429 */
          if (r->retry_after >= 0) {
            back_set_retry_after(sback, opt, r->retry_after);
          }
          if (restart_whole) {
            hts_log_print(opt, LOG_NOTICE,
                          "Restarting whole file after error %d (%s) at link "
                          "%s%s (from %s%s)",
                          r->statuscode, r->msg, adr_nologin(opt, urladr()),
                          urlfil(),
                          adr_nologin(opt, heap(heap(ptr)->precedent)->adr),
                          heap(heap(ptr)->precedent)->fil);
          } else {
            hts_log_print(opt, LOG_NOTICE,
                          "Retry after error %d (%s) at link %s%s (from %s%s)",
                          r->statuscode, r->msg, adr_nologin(opt, urladr()),
                          urlfil(),
                          adr_nologin(opt, heap(heap(ptr)->precedent)->adr),
                          heap(heap(ptr)->precedent)->fil);
          }
          // re-request the file
          if (hts_record_link(opt, urladr(), urlfil(), savename(), "", "",
                              codebase)) {
            heap_top()->testmode = heap(ptr)->testmode; // mode test?
            heap_top()->link_import = 0;                // pas mode import
            heap_top()->depth = heap(ptr)->depth;
            heap_top()->pass2 = max(heap(ptr)->pass2, numero_passe);
            heap_top()->retry =
                restart_whole ? heap(ptr)->retry : heap(ptr)->retry - 1;
            heap_top()->fetch = heap(ptr)->fetch;
            heap_top()->premier = heap(ptr)->premier;
            heap_top()->precedent = heap(ptr)->precedent;
            // refetch whole with no Range, and latch out a second free restart
            heap_top()->refetch_whole = r->refetch_wholefile;
          } else { // oups erreur, plus de mémoire!!
            return 0;
          }
        }
      } else {
        hts_log_print(opt, LOG_DEBUG, "Info: no robots.txt at %s%s",
                      adr_nologin(opt, urladr()), urlfil());
      }
      if (!store_errpage) {
        if (r->adr) { // désalloc
          freet(r->adr);
          r->adr = NULL;
        }
        error = 1; // erreur!
      }
      // otherwise, consider this is not an error
    }
    // FIN rattrapage des 301,302,307..
    // ------------------------------------------------------------

  } // if !error

  /* Apply changes */
  ENGINE_SAVE_CONTEXT();

  return 0;
}

hts_boolean hts_take_lock_request(httrackp *opt, const char *name) {
  /* OPT_GET_BUFF rotates, so both paths stay valid at once. */
  const char *const request = hts_logpath(opt, name);
  const char *const progress = hts_logpath(opt, "hts-in_progress.lock");

  if (!fexist_utf8(request))
    return HTS_FALSE;
  /* A request no newer than this run's progress lock was aimed at an earlier
     mirror, and is neither ours to act on nor to delete. A clock stepping back
     still makes the file inert meanwhile. */
  if (!hts_file_is_newer(request, progress))
    return HTS_FALSE;
  UNLINK(request);
  /* A lock still there is one we may never be able to remove, and acting on it
     would fire on this poll and on every one after it. */
  if (fexist_utf8(request))
    return HTS_FALSE;
  return HTS_TRUE;
}

/*
  Process pause, link adding..
*/
void hts_mirror_process_user_interaction(htsmoduleStruct *str,
                                         htsmoduleStructExtended *stre) {
  int b;

  /* Load engine variables */
  ENGINE_LOAD_CONTEXT();

  /* Windows has no cross-process SIGTERM: a stop request arrives as a file. */
  if (hts_take_lock_request(opt, HTS_ABORT_LOCKNAME)) {
    hts_log_print(opt, LOG_ERROR, "Exit requested by shell or user");
    hts_store_release_int(stre->exit_xh_, 1);
    return;
  }

  // pause/lock files
  {
    int do_pause = 0;

    if (hts_take_lock_request(opt, HTS_PAUSE_LOCKNAME)) {
      do_pause = 1;
    }
    // after receving N bytes, pause
    if (opt->fragment > 0) {
      if ((HTS_STAT.stat_bytes - stat_fragment) > opt->fragment) {
        do_pause = 1;
      }
    }
    /* A run already winding down would pause with nothing left to wake it. */
    if (do_pause && !hts_run_is_ending(opt)) {
      hts_log_print(opt, LOG_INFO, "engine: pause requested..");
      /* Drain the transfers, unless the run is ending meanwhile. */
      while (back_nsoc(sback) > 0 && !hts_run_is_ending(opt)) {
        back_wait(sback, opt, cache, HTS_STAT.stat_timestart);
        Sleep(200);
        {
          back_wait(sback, opt, cache, HTS_STAT.stat_timestart);

          b = 0;
          if (!hts_loop_tick(sback, opt, b, ptr) || !back_checkmirror(opt)) {
            hts_log_print(opt, LOG_ERROR, "Exit requested by shell or user");
            hts_store_release_int(stre->exit_xh_, 1); // exit requested
            return;
          }
        }
      }
      /* A stop during the drain must not pause either. */
      if (!hts_run_is_ending(opt)) {
        FILE *fp = FOPEN(hts_logpath(opt, "hts-paused.lock"), "wb");
        if (fp) {
          fspc(NULL, fp, "info"); // timestamp
          fprintf(fp,
                  "Pause" LF "HTTrack is paused after retreiving " LLintP
                  " bytes" LF "Delete this file to continue the mirror->.." LF
                  "" LF "",
                  (LLint) HTS_STAT.stat_bytes);
          fclose(fp);
        }
        stat_fragment = HTS_STAT.stat_bytes;
        /* Info for wrappers */
        hts_log_print(opt, LOG_INFO, "engine: pause: %s",
                      hts_logpath(opt, "hts-paused.lock"));
        RUN_CALLBACK1(opt, pause, hts_logpath(opt, "hts-paused.lock"));
      }
    }
    //
  }
  // end of pause/lock files

  // changement dans les préférences
  {
    char **const addurl = hts_addurl_take(opt);
    char **next;
    lien_adrfilsave add;

    for (next = addurl; next != NULL && *next != NULL; next++) {
      char BIGSTK add_url[HTS_URLMAXSIZE * 2];

      add.af.adr[0] = add.af.fil[0] = add_url[0] = '\0';
      if (!link_has_authority(*next))
        strcpybuff(add_url, "http://"); // ajouter http://
      strcatbuff(add_url, *next);
      if (ident_url_absolute(add_url, &add.af) >= 0) {
        // ----Ajout----
        lien_adrfil former;
        int forbidden_url = 0;
        int r_sv;

        former.adr[0] = former.fil[0] = '\0';

        // resolve the link, possibly rewriting address/file
        r_sv = url_savename(&add, NULL, NULL, NULL, opt, sback, cache, hash,
                            ptr, numero_passe, NULL);
        /* No headers were passed, so the name may still be a placeholder: the
           parser resolves it before recording, and so must we (#1253). */
        if (r_sv != -1 && IS_DELAYED_EXT(add.save)) {
          r_sv = hts_wait_delayed(str, &add, NULL, NULL, &former,
                                  &forbidden_url, NULL);
        }
        if (r_sv != -1) {
          /* A refused type, or a stop that left the placeholder: recording
             that name is the bug. */
          if (forbidden_url == 1 || IS_DELAYED_EXT(add.save)) {
            hts_log_print(opt, LOG_NOTICE,
                          "Link %s%s not added after user request",
                          adr_nologin(opt, add.af.adr), add.af.fil);
          } else if (hash_read(hash, add.save, NULL, HASH_STRUCT_FILENAME) <
                     0) { // no existing record
            // record the link
            if (hts_record_link(opt, add.af.adr, add.af.fil, add.save,
                                former.adr, former.fil, NULL)) {
              hts_link_set_self_parented(opt, opt->depth, max(0, numero_passe));
              //
              hts_log_print(opt, LOG_INFO, "Link added by user: %s%s",
                            adr_nologin(opt, add.af.adr), add.af.fil);
              //
            } else if (hts_maxlinks_no_room(opt)) {
              /* Give the parser's verdict rather than dropping the rest of
                 the user's URLs in silence. */
              maxlinks_give_up(opt, stre->exit_xh_);
              break;
            } else { // out of memory
              hts_addurl_free(addurl);
              return;
            }
          } else {
            hts_log_print(opt, LOG_NOTICE,
                          "Existing link %s%s not added after user request",
                          adr_nologin(opt, add.af.adr), add.af.fil);
          }
        }
      } else {
        hts_log_print(opt, LOG_ERROR, "Error during URL decoding for %s",
                      url_nologin(opt, add_url));
      }
      // ----Fin Ajout----
    }
    hts_addurl_free(addurl);
  }
  hts_apply_live_filters(opt, sback, ptr);
  // si une pause a été demandée
  if (opt->state._hts_setpause ||
      back_pluggable_sockets_strict(sback, opt) <= 0) {
    // index du lien actuel
    int b = back_index(opt, sback, urladr(), urlfil(), savename());
    int prev = opt->state._hts_in_html_parsing;

    if (b < 0)
      b = 0; // forcer pour les stats
    while (opt->state._hts_setpause ||
           back_pluggable_sockets_strict(sback, opt) <=
               0) { // on fait la pause..
      opt->state._hts_in_html_parsing = 6;
      /* a rule sent during a pause applies now, not on resume */
      hts_apply_live_filters(opt, sback, ptr);
      back_wait(sback, opt, cache, HTS_STAT.stat_timestart);
      /* time limit (-E) exceeded: stop waiting for a socket (#481) */
      if (!back_checkmirror(opt))
        break;

      if (!hts_loop_tick(sback, opt, b, ptr)) {
        hts_log_print(opt, LOG_ERROR, "Exit requested by shell or user");
        hts_store_release_int(stre->exit_xh_, 1); // exit requested
        return;
      }
      /* Read here as well, or a mirror down to its last socket sees no stop
         request until the transfer it waits on has ended. */
      if (hts_take_lock_request(opt, HTS_ABORT_LOCKNAME)) {
        hts_log_print(opt, LOG_ERROR, "Exit requested by shell or user");
        hts_store_release_int(stre->exit_xh_, 1);
      }
      /* Same omission as the wait for the current link: with every slot busy
         the mirror parks here instead, and the exit never lands (#1096). */
      if (hts_load_acquire_int(stre->exit_xh_)) {
        return;
      }
      Sleep(100); // pause
    }
    opt->state._hts_in_html_parsing = prev;
  }
  ENGINE_SAVE_CONTEXT();
  return;
}

/*
Wait for next file and
check 301, 302, .. statuscodes (moved)
*/
int hts_mirror_wait_for_next_file(htsmoduleStruct *str,
                                  htsmoduleStructExtended *stre) {
  /* Load engine variables */
  ENGINE_DEFINE_CONTEXT();
  int b;
  int n;

  /* This is not supposed to hapen. */
  if (heap(ptr)->pass2 == -1) {
    hts_log_print(opt, LOG_WARNING, "Link is already ready %s%s",
                  adr_nologin(opt, urladr()), urlfil());
  }

  /* User interaction */
  ENGINE_SAVE_CONTEXT();
  {
    hts_mirror_process_user_interaction(str, stre);
  }
  ENGINE_SET_CONTEXT();

  /* Done while processing user interactions ? */
  if (heap(ptr)->pass2 == -1) {
    hts_log_print(opt, LOG_DEBUG, "Link is now ready %s%s",
                  adr_nologin(opt, urladr()), urlfil());
    // We are ready
    return 2; // goto jump_if_done;
  }
  // si le fichier n'est pas en backing, le mettre..
  if (!back_exist(str->sback, str->opt, urladr(), urlfil(), savename())) {
    if (back_add(sback, opt, cache, urladr(), urlfil(), savename(),
                 heap(heap(ptr)->precedent)->adr,
                 heap(heap(ptr)->precedent)->fil, heap(ptr)->testmode,
                 heap(ptr)->refetch_whole, &heap(ptr)->fetch) == -1) {
      printf("PANIC! : Crash adding error, unexpected error found.. [%d]\n",
             __LINE__);
      hts_log_print(opt, LOG_ERROR, "Unexpected backing error for %s%s",
                    adr_nologin(opt, urladr()), urlfil());
    }
  }

  // ajouter autant de socket qu'on peut ajouter
  n = opt->maxsoc - back_nsoc(sback);

  if ((n > 0) &&
      (!opt->state
            ._hts_setpause)) { // si sockets libre et pas en pause, ajouter
    // remplir autant que l'on peut le cache (backing)
    back_fillmax(sback, opt, cache, ptr, numero_passe);
  }
  // index du lien actuel
  {
    // ------------------------------------------------------------
    // attendre que le fichier actuel soit prêt - BOUCLE D'ATTENTE
    do {
      /* User interaction */
      ENGINE_SAVE_CONTEXT();
      {
        hts_mirror_process_user_interaction(str, stre);
      }
      ENGINE_SET_CONTEXT();

      // index du lien actuel
      b = back_index(opt, sback, urladr(), urlfil(), savename());
      // Continue to the loop if link still present
      if (b < 0)
        break;

      // Receive data
      if (back_status(&back[b]) > 0)
        back_wait(sback, opt, cache, HTS_STAT.stat_timestart);

      // Continue to the loop if link still present
      b = back_index(opt, sback, urladr(), urlfil(), savename());
      if (b < 0)
        break;

      // Stop the mirror
      if (!back_checkmirror(opt)) {
        hts_log_print(opt, LOG_ERROR, "Exit requested by shell or user");
        hts_store_release_int(stre->exit_xh_, 1); // exit requested
        return 0;
      }
      /* An exit asked of the engine must leave this wait too: only teardown
         stops a live FTP worker, so waiting on its slot never ends (#1096). */
      if (hts_load_acquire_int(stre->exit_xh_)) {
        return 0;
      }
      // And fill the backing stack
      if (hts_load_acquire_int(&back[b].status) > 0)
        back_fillmax(sback, opt, cache, ptr, numero_passe);

      // Continue to the loop if link still present
      b = back_index(opt, sback, urladr(), urlfil(), savename());
      if (b < 0)
        break;

      // autres occupations de HTTrack: statistiques, boucle d'attente, etc.
      if ((opt->makestat) || (opt->maketrack)) {
        TStamp l = time_local();

        if ((int) (l - makestat_time) >= 60) {
          if (makestat_fp != NULL) {
            const LLint recv = hts_stat_recv_get();

            fspc(NULL, makestat_fp, "info");
            fprintf(
                makestat_fp, "Rate= %d (/" LLintP ") \11NewLinks= %d (/%d)" LF,
                (int) ((recv - *stre->makestat_total_) / (l - makestat_time)),
                (LLint) recv, (int) opt->lien_tot - *stre->makestat_lnk_,
                (int) opt->lien_tot);
            fflush(makestat_fp);
            *stre->makestat_total_ = recv;
            *stre->makestat_lnk_ = heap_top_index();
          }
          if (stre->maketrack_fp != NULL) {
            int i;

            fspc(NULL, stre->maketrack_fp, "info");
            fprintf(stre->maketrack_fp, LF);
            for (i = 0; i < back_max; i++) {
              back_info(sback, i, 3, stre->maketrack_fp);
            }
            fprintf(stre->maketrack_fp, LF);
            fflush(stre->maketrack_fp);
          }
          makestat_time = l;
        }
      }

      /* cancel links */
      {
        int i;
        char *s;

        while ((s = hts_cancel_file_pop(opt)) != NULL) {
          if (strnotempty(s)) { // fichier à canceller
            hts_boolean matched = HTS_FALSE;

            for (i = 0; i < back_max; i++) {
              /* Once, and with the acquire matching ftp_worker_release(): two
                 reads can straddle a worker publishing STATUS_FTP_READY and
                 take the teardown arm for a slot it still owned. */
              const int status = hts_load_acquire_int(&back[i].status);

              if (status > 0) {
                if (strcmp(back[i].url_sav, s) == 0) { // ok trouvé
                  matched = HTS_TRUE;
                  if (status != 1000) {
                    if (back[i].r.soc != INVALID_SOCKET)
                      deletehttp(&back[i].r);
                    back[i].r.soc = INVALID_SOCKET;
                    back[i].r.statuscode = STATUSCODE_INVALID;
                    strcpybuff(back[i].r.msg, "Cancelled by User");
                    back[i].status = 0; // terminé
                    back_set_finished(opt, sback, i);
                  } else // cancel ftp, so flag it
                    hts_store_release_int(&back[i].stop_ftp, 1);
                }
              }
            }
            if (!matched) {
              hts_log_print(opt, LOG_WARNING,
                            "Cancel ignored, no transfer in progress has this "
                            "save path (finished, or a URL was passed): %s",
                            s);
            }
            s[0] = '\0';
          }
          freet(s);
        }

        if (!hts_loop_tick(sback, opt, b, ptr)) {
          hts_log_print(opt, LOG_ERROR, "Exit requested by shell or user");
          hts_store_release_int(stre->exit_xh_, 1); // exit requested
          return 0;
        }
      }

#if HTS_POLL
      if ((opt->shell) || (opt->keyboard) || (opt->verbosedisplay) ||
          (!opt->quiet)) {
        TStamp tl;

        *stre->info_shell_ = 1;

        /* Toggle with ENTER */
        if (!opt->quiet) {
          if (check_stdin()) {
            char com[256];

            linput(stdin, com, 200);
            if (opt->verbosedisplay == HTS_VERBOSE_FULL)
              opt->verbosedisplay = HTS_VERBOSE_SIMPLE;
            else
              opt->verbosedisplay = HTS_VERBOSE_FULL;
            /* Info for wrappers */
            hts_log_print(opt, LOG_INFO, "engine: change-options");
            RUN_CALLBACK0(opt, chopt);
          }
        }

        tl = time_local();

        // générer un message d'infos sur l'état actuel
        if (opt->shell) {                           // si shell
          if ((tl - *stre->last_info_shell_) > 0) { // toute les 1 sec
            FILE *fp = stdout;
            int a = 0;

            *stre->last_info_shell_ = tl;
            // Debug hook: is the robot still alive?
            if (fexist_utf8(hts_logpath(opt, "hts-autopsy"))) {
              // (oui je sais un robot vivant.. mais bon.. il a le droit de
              // vivre lui aussi) (libérons les robots esclaves de l'internet!)
              FILE *fpa;

              UNLINK(hts_logpath(opt, "hts-autopsy"));
              fpa = FOPEN(hts_logpath(opt, "hts-isalive"), "wb");
              /* on failure the dump stays on stdout, as when untriggered */
              if (fpa != NULL) {
                fp = fpa;
                a = 1;
              }
            }
            if ((*stre->info_shell_) || a) {
              int i, j;

              fprintf(fp, "TIME %d" LF, (int) (tl - HTS_STAT.stat_timestart));
              fprintf(fp, "TOTAL %d" LF, (int) HTS_STAT.stat_bytes);
              /* divisor non-zero only via htscore.c's last_info_shell seed */
              fprintf(fp, "RATE %d" LF,
                      (int) (hts_stat_recv_get() /
                             max(1, tl - HTS_STAT.stat_timestart)));
              fprintf(fp, "SOCKET %d" LF, back_nsoc(sback));
              fprintf(fp, "LINK %d" LF, opt->lien_tot);
              {
                LLint mem = 0;

                for (i = 0; i < back_max; i++)
                  if (back[i].r.adr != NULL)
                    mem += back[i].r.size;
                fprintf(fp, "INMEM " LLintP "" LF, (LLint) mem);
              }
              for (j = 0; j < 2; j++) { // passes pour ready et wait
                for (i = 0; i < back_max; i++) {
                  back_info(
                      sback, i, j + 1,
                      stdout); // maketrack_fp a la place de stdout ?? // **
                }
              }
              fprintf(fp, LF);
              if (a)
                fclose(fp);
              io_flush;
            }
          }
        } // si shell

      } // si shell ou keyboard (option)
      //
#endif
    } while ((b >= 0) && (back_status(&back[max(b, 0)]) > 0));

    // If link not found on the stack, it's because it has already been
    // downloaded in background Then, skip it and go to the next one
    if (b < 0) {
      hts_log_print(opt, LOG_DEBUG,
                    "link #%d is ready, no more on the stack, skipping: %s%s..",
                    ptr, adr_nologin(opt, urladr()), urlfil());

      return 2; // goto jump_if_done;
    }

    if (!opt->verbosedisplay) {
      /* The spinner overwrites itself; nothing to overwrite off a terminal. */
      if (!opt->quiet && hts_stdout_isterminal()) {
        static int roll = 0; /* static: ok */

        roll = (roll + 1) % 4;
        printf("%c\x0d", ("/-\\|")[roll]);
        fflush(stdout);
      }
    } else if (opt->verbosedisplay == HTS_VERBOSE_SIMPLE) {
      if (b >= 0) {
        if (back[b].r.statuscode == HTTP_OK)
          printf("%d/%d: %s%s (" LLintP " bytes) - OK\n", ptr, opt->lien_tot,
                 adr_nologin(opt, back[b].url_adr), back[b].url_fil,
                 (LLint) back[b].r.size);
        else
          printf("%d/%d: %s%s (" LLintP " bytes) - %d\n", ptr, opt->lien_tot,
                 adr_nologin(opt, back[b].url_adr), back[b].url_fil,
                 (LLint) back[b].r.size, back[b].r.statuscode);
      } else {
        hts_log_print(opt, LOG_ERROR, "Link disappeared");
      }
      fflush(stdout);
    }

    // ------------------------------------------------------------

    // copier structure réponse htsblk
    if (b >= 0) {
      memcpy(r, &(back[b].r), sizeof(htsblk));
      r->location =
          stre->loc_; // ne PAS copier location!! adresse, pas de buffer
      if (back[b].r.location)
        strlcpybuff(r->location, back[b].r.location, HTS_LOCATION_SIZE);
      back[b].r.adr = NULL; // ne pas faire de desalloc ensuite
      r->headers = NULL;    // back_maydelete() frees the block this aliases

      // libérer emplacement backing
      back_maydelete(opt, cache, sback, b);
    }
  }

  ENGINE_SAVE_CONTEXT();
  return 0;
}

/* Recheck a link against its known MIME type; true when filters forbid it. */
static hts_boolean mime_forbids_link(httrackp *opt, int ptr,
                                     const lien_adrfilsave *afs,
                                     const char *mime, int *forbidden_url) {
  const int verdict = hts_acceptmime(opt, ptr, afs->af.adr, afs->af.fil, mime);

  if (verdict == -1)
    return HTS_FALSE;
  hts_log_print(opt, LOG_DEBUG, "result for wizard mime test: %d", verdict);
  if (verdict != 1)
    return HTS_FALSE;
  *forbidden_url = verdict;
  hts_log_print(opt, LOG_DEBUG,
                "link forbidden because of MIME types restrictions: %s%s",
                adr_nologin(opt, afs->af.adr), afs->af.fil);
  return HTS_TRUE;
}

/* Wait for delayed types */
int hts_wait_delayed(htsmoduleStruct *str, lien_adrfilsave *afs,
                     char *parent_adr, char *parent_fil, lien_adrfil *former,
                     int *forbidden_url, hts_fetch_meta *fetch) {
  ENGINE_LOAD_CONTEXT_BASE();
  hash_struct *const hash = hashptr;

  int in_error = 0;
  hts_boolean cancelled = HTS_FALSE;
  LLint in_error_size = 0;
  char in_error_msg[32];

  // resolve unresolved type
  if (opt->savename_delayed != HTS_SAVENAME_DELAYED_NONE &&
      *forbidden_url == 0 && IS_DELAYED_EXT(afs->save) &&
      !hts_load_acquire_int(&opt->state.stop)) {
    int loops;
    int continue_loop;
    char BIGSTK
        cookie_before[16384]; /* #15: this URL's Cookie header, pre-request */

    hts_log_print(opt, LOG_DEBUG, "Waiting for type to be known: %s%s",
                  adr_nologin(opt, afs->af.adr), afs->af.fil);

    /* Follow while type is unknown and redirects occurs */
    for (loops = 0, continue_loop = 1;
         IS_DELAYED_EXT(afs->save) && continue_loop && loops < 7; loops++) {
      continue_loop = 0;
      /* #15: snapshot the Cookie header THIS url would send, so only its own
         self-redirect Set-Cookie trips the retry, not a concurrent slot's. */
      cookie_before[0] = '\0';
      if (opt->accept_cookie && opt->cookie != NULL)
        http_cookie_header(opt->cookie, afs->af.adr, afs->af.fil, cookie_before,
                           sizeof(cookie_before));

      /* Wait for an available slot */
      if (!hts_wait_available_socket(sback, opt, cache, ptr))
        return -1;

      /* We can lookup directly in the cache to speedup this mess */
      if (opt->delayed_cached) {
        lien_back back;

        memset(&back, 0, sizeof(back));
        back.r = cache_read(opt, cache, afs->af.adr, afs->af.fil, NULL,
                            NULL); // test uniquement
        if (back.r.statuscode == HTTP_OK &&
            strnotempty(back.r.contenttype)) { // cache found, and aswer is 'OK'
          hts_log_print(opt, LOG_DEBUG,
                        "Direct type lookup in cache (-%%D1): %s",
                        back.r.contenttype);

          /* Recompute filename with MIME type */
          afs->save[0] = '\0';
          url_savename(afs, former, heap(ptr)->adr, heap(ptr)->fil, opt, sback,
                       cache, hash, ptr, numero_passe, &back);

          /* Recompute authorization with MIME type */
          (void) mime_forbids_link(opt, ptr, afs, back.r.contenttype,
                                   forbidden_url);
          break; /* exit loop, forbidden or not */
        }
      }

      /* Check if the file was recorded already (necessary for redirects) */
      if (hash_read(hash, afs->save, NULL, HASH_STRUCT_FILENAME) >= 0) {
        if (loops == 0) { /* Should not happend */
          hts_log_print(
              opt, LOG_ERROR,
              "Duplicate entry in hts_wait_delayed() cancelled: %s%s -> %s",
              adr_nologin(opt, afs->af.adr), afs->af.fil, afs->save);
        }
        /* Exit loop (we're done) */
        continue_loop = 0;
        break;
      }

      /* Add in backing (back_index() will respond correctly) */
      if (back_add_if_not_exists(sback, opt, cache, afs->af.adr, afs->af.fil,
                                 afs->save, parent_adr, parent_fil, 0,
                                 fetch) != -1) {
        int b;

        b = back_index(opt, sback, afs->af.adr, afs->af.fil, afs->save);
        if (b < 0) {
          printf("PANIC! : Crash adding error, unexpected error found.. [%d]\n",
                 __LINE__);
          return -1;
        }

        /* We added the link before the parser recorded it -- the background
         * download MUST NOT clean silently this entry! (Petr Gajdusek) */
        back[b].early_add = 1;

        /* Cache read failed because file does not exist (bad delayed name!)
           Just re-add with the correct name, as we know the MIME now!
         */
        if (back[b].r.statuscode == STATUSCODE_INVALID &&
            back[b].r.adr == NULL) {
          lien_back delayed_back;

          back_copy_static(&back[b], &delayed_back);

          /* Delete entry */
          back[b].r.statuscode =
              0; /* TEMPORARY INVESTIGATE WHY WE FETCHED A SOCKET HERE */
          back_maydelete(opt, cache, sback, b); // cancel
          b = -1;

          /* the cancel may leave the now-unreferenced placeholder on disk
           * (#483) */
          if (fexist_utf8(delayed_back.url_sav)) {
            back_delayed_discard(opt, &delayed_back);
          }

          /* Recompute filename with MIME type */
          afs->save[0] = '\0';
          url_savename(afs, former, heap(ptr)->adr, heap(ptr)->fil, opt, sback,
                       cache, hash, ptr, numero_passe, &delayed_back);

          /* Recompute authorization with MIME type */
          if (mime_forbids_link(opt, ptr, afs, delayed_back.r.contenttype,
                                forbidden_url))
            break; // exit loop

          /* Re-Add wiht correct type */
          if (back_add_if_not_exists(sback, opt, cache, afs->af.adr,
                                     afs->af.fil, afs->save, parent_adr,
                                     parent_fil, 0, fetch) != -1) {
            b = back_index(opt, sback, afs->af.adr, afs->af.fil, afs->save);
          }
          if (b < 0) {
            printf(
                "PANIC! : Crash adding error, unexpected error found.. [%d]\n",
                __LINE__);
            return -1;
          }

          /* We added the link before the parser recorded it -- the background
           * download MUST NOT clean silently this entry! (Petr Gajdusek) */
          back[b].early_add = 1;

          hts_log_print(opt, LOG_DEBUG,
                        "Type immediately loaded from cache: %s",
                        delayed_back.r.contenttype);
        }

        /* Wait for headers to be received */
        if (b >= 0) {
          back_set_locked(sback, b); // Locked entry
        }
        int st;
        do {
          if (b < 0)
            break;

          // temps à attendre, et remplir autant que l'on peut le cache
          // (backing)
          if (back_status(&back[b]) > 0) {
            back_wait(sback, opt, cache, 0);
          }
          if (ptr >= 0) {
            back_fillmax(sback, opt, cache, ptr, numero_passe);
          }
          if (!hts_loop_tick(sback, opt, b, ptr)) {
            back_set_unlocked(sback, b);
            return -1;
          } else if (opt->state._hts_cancel ||
                     !back_checkmirror(
                         opt)) { // cancel level 2 or 1 (cancel parsing)
            back_delete(opt, cache, sback, b); // cancel test
            b = -1;
            cancelled = HTS_TRUE;
            break;
          }
          st = back_status(&back[b]);
        } while (
            /* dns/connect/request */
            (st >= 99 && st <= 101) ||
            /* For redirects, wait for request to be terminated */
            (HTTP_IS_REDIRECT(back[b].r.statuscode) && st > 0) ||
            /* Same for errors */
            (HTTP_IS_ERROR(back[b].r.statuscode) && st > 0) ||
            /* Contested type: wait for a sniffable body head (or EOF) */
            (back[b].r.statuscode == HTTP_OK && st > 0 &&
             strnotempty(back[b].r.cdispo) == 0 &&
             back[b].r.size < HTS_SNIFF_LEN &&
             hts_ext_sniff_wanted(opt, back[b].r.contenttype,
                                  back[b].url_fil)));
        if (b >= 0) {
          back_set_unlocked(sback, b); // Unlocked entry
        }
        /* ready (chunked) or ready (regular download) or ready (completed) */

        // Note: filename NOT in hashtable yet - liens_record will do it, with
        // the correct ext!
        if (b >= 0) {
          lien_back delayed_back;

          back_copy_static(&back[b], &delayed_back);

          /* Error */
          if (HTTP_IS_ERROR(back[b].r.statuscode)) {
            /* seen as in error */
            in_error = back[b].r.statuscode;
            in_error_msg[0] = 0;
            strncatbuff(in_error_msg, back[b].r.msg, sizeof(in_error_msg) - 1);
            in_error_size = back[b].r.totalsize;
            /* don't break, even with "don't take error pages" switch, because
             * we need to process the slot anyway (and cache the error) */
          }
          /* Moved! */
          else if (HTTP_IS_REDIRECT(back[b].r.statuscode)) {
            char BIGSTK mov_url[HTS_URLMAXSIZE * 2];
            char BIGSTK cookie_after[16384];
            hts_boolean cookies_changed;

            /* #15: cookie-wall signal: this URL's own Cookie header changed
               across its self-redirect (a Set-Cookie it just set). */
            cookie_after[0] = '\0';
            if (opt->accept_cookie && opt->cookie != NULL)
              http_cookie_header(opt->cookie, afs->af.adr, afs->af.fil,
                                 cookie_after, sizeof(cookie_after));
            cookies_changed = strcmp(cookie_before, cookie_after) != 0;

            mov_url[0] = '\0';
            strcpybuff(mov_url, back[b].r.location); // copier URL
            url_drop_fragment(mov_url);

            /* Remove (temporarily created) file if it was created */
            UNLINK(fconv(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                         back[b].url_sav));

            /* Remove slot! */
            if (back_status(&back[b]) == STATUS_READY) {
              back_maydelete(opt, cache, sback, b);
            } else { /* should not happend */
              back_delete(opt, cache, sback, b);
            }
            b = -1;

            /* Handle redirect */
            if (strnotempty(mov_url)) { // location existe!
              lien_adrfil moved;
              moved.adr[0] = moved.fil[0] = '\0';
              if (fetch != NULL)
                hts_fetch_through(opt, fetch, afs->af.adr);
              //
              if (ident_url_relatif(mov_url, afs->af.adr, afs->af.fil,
                                    &moved) >= 0) {
                hts_log_print(opt, LOG_DEBUG,
                              "Redirect while resolving type: %s%s -> %s%s",
                              adr_nologin(opt, afs->af.adr), afs->af.fil,
                              adr_nologin(opt, moved.adr), moved.fil);
                // si non bouclage sur soi même, ou si test avec GET non testé
                if (strcmp(moved.adr, afs->af.adr) != 0 ||
                    strcmp(moved.fil, afs->af.fil) != 0) {

                  // recopier former->adr/fil?
                  if (former != NULL) {
                    if (strnotempty(former->adr) == 0) { // Pas déja noté
                      strcpybuff(former->adr, afs->af.adr);
                      strcpybuff(former->fil, afs->af.fil);
                    }
                  }
                  // check explicit forbidden - don't follow 3xx in this case
                  {
                    int set_prio_to = 0;

                    if (hts_acceptlink(opt, ptr, moved.adr, moved.fil, NULL,
                                       NULL, HTS_FALSE, &set_prio_to,
                                       NULL) == 1) { /* forbidden */
                      /* Note: the cache 'cached_tests' system will remember
                       * this error, and we'll only issue ONE request */
                      *forbidden_url = 1; /* Forbidden! */
                      hts_log_print(opt, LOG_DEBUG,
                                    "link forbidden because of redirect beyond "
                                    "the mirror scope at %s%s -> %s%s",
                                    adr_nologin(opt, afs->af.adr), afs->af.fil,
                                    adr_nologin(opt, moved.adr), moved.fil);
                      strcpybuff(afs->af.adr, moved.adr);
                      strcpybuff(afs->af.fil, moved.fil);
                      mov_url[0] = '\0';
                      break;
                    }
                  }

                  // ftp: stop!
                  if (strfield(mov_url, "ftp://")) {
                    strcpybuff(afs->af.adr, moved.adr);
                    strcpybuff(afs->af.fil, moved.fil);
                    break;
                  }

                  /* ok, continue */
                  strcpybuff(afs->af.adr, moved.adr);
                  strcpybuff(afs->af.fil, moved.fil);
                  continue_loop = 1;

                  /* Recompute filename for hash lookup */
                  afs->save[0] = '\0';
                  url_savename(afs, former, heap(ptr)->adr, heap(ptr)->fil, opt,
                               sback, cache, hash, ptr, numero_passe,
                               &delayed_back);
                } else if (cookies_changed) {
                  // #15: cookie-wall self-redirect; evict the cached
                  // fast-header so the re-issue refetches with the cookie.
                  if (cache->cached_tests != NULL)
                    coucal_remove(cache->cached_tests,
                                  concat(OPT_GET_BUFF(opt),
                                         OPT_GET_BUFF_SIZE(opt), afs->af.adr,
                                         afs->af.fil));
                  afs->save[0] = '\0';
                  url_savename(afs, former, heap(ptr)->adr, heap(ptr)->fil, opt,
                               sback, cache, hash, ptr, numero_passe,
                               &delayed_back);
                  continue_loop = 1;
                } else {
                  hts_log_print(opt, LOG_WARNING,
                                "Unable to test %s%s (loop to same filename)",
                                adr_nologin(opt, afs->af.adr), afs->af.fil);
                } // loop to same location
              } // ident_url_relatif()
            } // location
          } // redirect
          hts_log_print(opt, LOG_DEBUG, "Final type for %s%s: '%s'",
                        adr_nologin(opt, afs->af.adr), afs->af.fil,
                        delayed_back.r.contenttype);

          /* If we are done, do additional checks with final type and
           * authorizations */
          if (!continue_loop) {
            /* Recompute filename with MIME type. After a stop, url_savename()
               pumps sockets and could move this slot out, so pin it. */
            const hts_boolean pin =
                b >= 0 && back_status(&back[b]) != STATUS_FREE;

            afs->save[0] = '\0';
            if (pin)
              back_set_pinned(sback, b);
            url_savename(afs, former, heap(ptr)->adr, heap(ptr)->fil, opt,
                         sback, cache, hash, ptr, numero_passe, &delayed_back);
            if (pin)
              back_set_unlocked(sback, b);

            /* Recompute authorization with MIME type */
            if (mime_forbids_link(opt, ptr, afs, delayed_back.r.contenttype,
                                  forbidden_url))
              break; // exit loop
          }

          /* Still have a back reference */
          if (b >= 0) {
            /* move a still-writing placeholder before the url_sav patch
               blinds every cleanup to it (#483) */
            back_delayed_rename(opt, &back[b], afs->save);
            /* patch url_sav BEFORE finalize: it records/caches under this name
             */
            strcpybuff(back[b].url_sav, afs->save);
            /* Finalize now as we have the type */
            if (back_status(&back[b]) == STATUS_READY) {
              if (!back[b].finalized) {
                hts_log_print(opt, LOG_TRACE, "finalizing as we have the type");
                back_finalize(opt, cache, sback, b);
              }
            }
          }

        } // b >= 0
      } else {
        printf("PANIC! : Crash adding error, unexpected error found.. [%d]\n",
               __LINE__);
        return -1;
      }

    } // while(IS_DELAYED_EXT(save))

    if (in_error != 0) {
      /* 'no error page' selected or file discarded by size rules! */
      if (!opt->errpage || (in_error == STATUSCODE_TOO_BIG)) {
        /* Note: the cache 'cached_tests' system will remember this error, and
         * we'll only issue ONE request */
        /* Do not post-exclude the link here (*forbidden_url): the cache system
           would never process it again, and it would be refetched endlessly */
        if (in_error == STATUSCODE_TOO_BIG) {
          hts_log_print(opt, LOG_INFO,
                        "link not taken because of its size (%d bytes) at %s%s",
                        (int) in_error_size, adr_nologin(opt, afs->af.adr),
                        afs->af.fil);
        } else {
          hts_log_print(opt, LOG_INFO,
                        "link not taken because of error (%d '%s') at %s%s",
                        in_error, in_error_msg, adr_nologin(opt, afs->af.adr),
                        afs->af.fil);
        }
      }
    }
    // error
    if (*forbidden_url != 1 && IS_DELAYED_EXT(afs->save)) {
      *forbidden_url = 1;
      if (cancelled) {
        hts_log_print(opt, LOG_DEBUG,
                      "link cancelled, type unknown, left online: %s%s",
                      adr_nologin(opt, afs->af.adr), afs->af.fil);
      } else if (in_error) {
        hts_log_print(opt, LOG_WARNING,
                      "link in error (%d '%s'), type unknown, aborting: %s%s",
                      in_error, in_error_msg, adr_nologin(opt, afs->af.adr),
                      afs->af.fil);
      } else {
        hts_log_print(opt, LOG_WARNING,
                      "link is probably looping, type unknown, aborting: %s%s",
                      adr_nologin(opt, afs->af.adr), afs->af.fil);
      }
    }

  } // delayed type check ?

  ENGINE_SAVE_CONTEXT_BASE();

  return 0;
}
