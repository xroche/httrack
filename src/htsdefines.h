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
/* File: Some defines for httrack.c and others                  */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/** @file htsdefines.h
 *  Public callback prototypes and the wrapper/plug-in interface: the function
 *  pointer types a parser or wrapper module implements, and the callback table
 *  the engine dispatches through. */
#ifndef HTS_DEFINES_DEFH
#define HTS_DEFINES_DEFH

/* Forward declarations of engine structs, so this header is usable without
   pulling in their full definitions. Each is guarded so multiple public
   headers can repeat the typedef without clashing. */
#ifndef HTS_DEF_FWSTRUCT_httrackp
#define HTS_DEF_FWSTRUCT_httrackp
typedef struct httrackp httrackp;
#endif
#ifndef HTS_DEF_FWSTRUCT_lien_back
#define HTS_DEF_FWSTRUCT_lien_back
typedef struct lien_back lien_back;
#endif
#ifndef HTS_DEF_FWSTRUCT_htsblk
#define HTS_DEF_FWSTRUCT_htsblk
typedef struct htsblk htsblk;
#endif
#ifndef HTS_DEF_FWSTRUCT_hts_stat_struct
#define HTS_DEF_FWSTRUCT_hts_stat_struct
typedef struct hts_stat_struct hts_stat_struct;
#endif
#ifndef HTS_DEF_FWSTRUCT_htsmoduleStruct
#define HTS_DEF_FWSTRUCT_htsmoduleStruct
typedef struct htsmoduleStruct htsmoduleStruct;
#endif
#ifndef HTS_DEF_FWSTRUCT_t_hts_callbackarg
#define HTS_DEF_FWSTRUCT_t_hts_callbackarg
typedef struct t_hts_callbackarg t_hts_callbackarg;
#endif
#ifndef HTS_DEF_FWSTRUCT_t_hts_callbackarg
#define HTS_DEF_FWSTRUCT_t_hts_callbackarg
typedef struct t_hts_callbackarg t_hts_callbackarg;
#endif

/* Marks a symbol an external wrapper module exports back to the engine
   (dllexport on Windows, nothing elsewhere). */
#ifndef EXTERNAL_FUNCTION
#ifdef _WIN32
#define EXTERNAL_FUNCTION __declspec(dllexport)
#else
#define EXTERNAL_FUNCTION
#endif
#endif

/* Entry points of a --wrapper plug-in: hts_plug(opt, argv) is called once to
   install the wrapper (argv is the wrapper's argument string), hts_unplug(opt)
   once to tear it down. Both return non-zero on success. */
typedef int (*t_hts_plug) (httrackp * opt, const char *argv);

typedef int (*t_hts_unplug) (httrackp * opt);

/* Engine callback prototypes. Each is one hook the engine fires at a defined
   point of a mirror; a wrapper installs the ones it cares about in the
   callback table below. carg carries the user-defined argument chain; int
   returns are 1 to continue/accept, 0 to abort/refuse unless noted. */

/* Called once when the wrapper is installed; allocate per-run state here. */
typedef void (*t_hts_htmlcheck_init) (t_hts_callbackarg * carg);

/* Called once when the wrapper is removed; release per-run state here. */
typedef void (*t_hts_htmlcheck_uninit) (t_hts_callbackarg * carg);

/* Fired at the start of a mirror, after options are parsed. */
typedef int (*t_hts_htmlcheck_start) (t_hts_callbackarg * carg, httrackp * opt);

/* Fired at the end of a mirror. */
typedef int (*t_hts_htmlcheck_end) (t_hts_callbackarg * carg, httrackp * opt);

/* Fired while options are being changed, to validate or adjust them. */
typedef int (*t_hts_htmlcheck_chopt) (t_hts_callbackarg * carg, httrackp * opt);

/* Rewrite hook over an in-memory page: the html and len arguments point at the
   buffer and its length (the callback may reallocate and resize it),
   url_adresse and url_fichier name it. */
typedef int (*t_hts_htmlcheck_process) (t_hts_callbackarg * carg,
                                        httrackp * opt, char **html, int *len,
                                        const char *url_adresse,
                                        const char *url_fichier);

/* Same shape as process, run before HTML parsing. */
typedef t_hts_htmlcheck_process t_hts_htmlcheck_preprocess;

/* Same shape as process, run after HTML parsing. */
typedef t_hts_htmlcheck_process t_hts_htmlcheck_postprocess;

/* Inspect a page (read-only html/len) without rewriting it. */
typedef int (*t_hts_htmlcheck_check_html) (t_hts_callbackarg * carg,
                                           httrackp * opt, char *html, int len,
                                           const char *url_adresse,
                                           const char *url_fichier);

/* Answer an engine query identified by 'question'; returns the answer string
   (owned by the callback, must stay valid until the next call). */
typedef const char *(*t_hts_htmlcheck_query) (t_hts_callbackarg * carg,
                                              httrackp * opt,
                                              const char *question);

/* Second query channel, same contract as query. */
typedef const char *(*t_hts_htmlcheck_query2) (t_hts_callbackarg * carg,
                                               httrackp * opt,
                                               const char *question);

/* Third query channel, same contract as query. */
typedef const char *(*t_hts_htmlcheck_query3) (t_hts_callbackarg * carg,
                                               httrackp * opt,
                                               const char *question);

/* Per-tick progress hook: 'back' is the transfer slot array of 'back_max'
   entries, back_index the active one; lien_tot/lien_ntot and stats report
   queue size and running totals, stat_time the elapsed time. */
typedef int (*t_hts_htmlcheck_loop) (t_hts_callbackarg * carg, httrackp * opt,
                                     lien_back * back, int back_max,
                                     int back_index, int lien_tot,
                                     int lien_ntot, int stat_time,
                                     hts_stat_struct * stats);

/* Veto a link (adr host, fil path) after its transfer; status is the result.
   Return 0 to drop the link. */
typedef int (*t_hts_htmlcheck_check_link) (t_hts_callbackarg * carg,
                                           httrackp * opt, const char *adr,
                                           const char *fil, int status);

/* Veto a link by its MIME type before download; return 0 to skip it. */
typedef int (*t_hts_htmlcheck_check_mime) (t_hts_callbackarg * carg,
                                           httrackp * opt, const char *adr,
                                           const char *fil, const char *mime,
                                           int status);

/* Fired when the mirror pauses, waiting on 'lockfile' to be removed. */
typedef void (*t_hts_htmlcheck_pause) (t_hts_callbackarg * carg, httrackp * opt,
                                       const char *lockfile);

/* Fired after a file is written to disk; 'file' is the local path. */
typedef void (*t_hts_htmlcheck_filesave) (t_hts_callbackarg * carg,
                                          httrackp * opt, const char *file);

/* Richer file-saved notification: source host/filename, local path, and flags
   telling whether the file is new, modified, or left unchanged. */
typedef void (*t_hts_htmlcheck_filesave2) (t_hts_callbackarg * carg,
                                           httrackp * opt, const char *hostname,
                                           const char *filename,
                                           const char *localfile, int is_new,
                                           int is_modified, int not_updated);

/* Fired for each link parsed out of a page; 'link' may be edited in place. */
typedef int (*t_hts_htmlcheck_linkdetected) (t_hts_callbackarg * carg,
                                             httrackp * opt, char *link);

/* As linkdetected, plus tag_start, the markup the link was found in. */
typedef int (*t_hts_htmlcheck_linkdetected2) (t_hts_callbackarg * carg,
                                              httrackp * opt, char *link,
                                              const char *tag_start);

/* Fired on each transfer-status change of slot 'back'. */
typedef int (*t_hts_htmlcheck_xfrstatus) (t_hts_callbackarg * carg,
                                          httrackp * opt, lien_back * back);

/* Choose the local save path for a URL; write it into 'save'. adr/fil name the
   target, referer_adr/referer_fil the page that linked it. */
typedef int (*t_hts_htmlcheck_savename) (t_hts_callbackarg * carg,
                                         httrackp * opt,
                                         const char *adr_complete,
                                         const char *fil_complete,
                                         const char *referer_adr,
                                         const char *referer_fil, char *save);

/* Extended save-name hook, same signature as savename. */
typedef t_hts_htmlcheck_savename t_hts_htmlcheck_extsavename;

/* Inspect or edit the outgoing request headers in 'buff' before they are sent.
 */
typedef int (*t_hts_htmlcheck_sendhead) (t_hts_callbackarg * carg,
                                         httrackp * opt, char *buff,
                                         const char *adr, const char *fil,
                                         const char *referer_adr,
                                         const char *referer_fil,
                                         htsblk * outgoing);

/* Inspect the incoming response headers in 'buff' after they are received. */
typedef int (*t_hts_htmlcheck_receivehead) (t_hts_callbackarg * carg,
                                            httrackp * opt, char *buff,
                                            const char *adr, const char *fil,
                                            const char *referer_adr,
                                            const char *referer_fil,
                                            htsblk * incoming);

/* External parser module hooks: detect claims a document type (return 1 to
   take it), parse then extracts its links. 'str' carries the document. */
typedef int (*t_hts_htmlcheck_detect) (t_hts_callbackarg * carg, httrackp * opt,
                                       htsmoduleStruct * str);

typedef int (*t_hts_htmlcheck_parse) (t_hts_callbackarg * carg, httrackp * opt,
                                      htsmoduleStruct * str);

/* Callbacks */
#ifndef HTS_DEF_FWSTRUCT_t_hts_htmlcheck_callbacks
#define HTS_DEF_FWSTRUCT_t_hts_htmlcheck_callbacks
typedef struct t_hts_htmlcheck_callbacks t_hts_htmlcheck_callbacks;
#endif

/* Declares one named callback slot: its function pointer (typed
   t_hts_htmlcheck_<NAME>) paired with the carg passed to it. */
#define DEFCALLBACK(NAME)         \
  struct NAME {                   \
    t_hts_htmlcheck_ ##NAME fun;  \
    t_hts_callbackarg *carg;      \
  } NAME

/* Generic, type-erased callback slot used where the hook type is opaque. */
typedef void *t_hts_htmlcheck_t_hts_htmlcheck_callbacks_item;

typedef DEFCALLBACK(t_hts_htmlcheck_callbacks_item);

/* Per-callback argument node. Wrappers chain these so a new hook can wrap an
   existing one: userdef is the wrapper's own data, prev points back to the
   function and carg it displaced (call it to keep the previous behavior). */
struct t_hts_callbackarg {
  /* User-defined argument for the called function */
  void *userdef;

  /* Previous function, if any (fun != NULL) */
  struct prev {
    void *fun;
    t_hts_callbackarg *carg;
  } prev;
};

/* The full callback table, one slot per hook; installed in httrackp options
   and dispatched by the engine. The trailing comments mark the API version a
   slot first appeared in. */
struct t_hts_htmlcheck_callbacks {
  /* v3.41 */
  DEFCALLBACK(init);
  DEFCALLBACK(uninit);
  DEFCALLBACK(start);
  DEFCALLBACK(end);
  DEFCALLBACK(chopt);
  DEFCALLBACK(preprocess);
  DEFCALLBACK(postprocess);
  DEFCALLBACK(check_html);
  DEFCALLBACK(query);
  DEFCALLBACK(query2);
  DEFCALLBACK(query3);
  DEFCALLBACK(loop);
  DEFCALLBACK(check_link);
  DEFCALLBACK(check_mime);
  DEFCALLBACK(pause);
  DEFCALLBACK(filesave);
  DEFCALLBACK(filesave2);
  DEFCALLBACK(linkdetected);
  DEFCALLBACK(linkdetected2);
  DEFCALLBACK(xfrstatus);
  DEFCALLBACK(savename);
  DEFCALLBACK(sendhead);
  DEFCALLBACK(receivehead);
  DEFCALLBACK(detect);
  DEFCALLBACK(parse);
  /* >3.41 */
  DEFCALLBACK(extsavename);
};

/* Library-internal helpers, compiled only inside the engine. */
#ifdef HTS_INTERNAL_BYTECODE

/* Maps a callback slot's name to its byte offset in the callback table, so a
   slot can be installed by name. */
#ifndef HTS_DEF_FWSTRUCT_t_hts_callback_ref
#define HTS_DEF_FWSTRUCT_t_hts_callback_ref
typedef struct t_hts_callback_ref t_hts_callback_ref;
#endif
struct t_hts_callback_ref {
  const char *name;
  size_t offset;
};

#ifdef __cplusplus
extern "C" {
#endif

/* Default (no-op) callback table the engine starts from. */
extern const t_hts_htmlcheck_callbacks default_callbacks;

#ifdef __cplusplus
}
#endif

/* Internal helpers for building an HTTP request/response into the engine's
   scratch buffer (opt->state.HTbuff): START resets it, PRINT appends; the
   PANIC variant records a fatal error message. */
#define HT_PRINT(A) strcatbuff(opt->state.HTbuff,A);

#define HT_REQUEST_START opt->state.HTbuff[0]='\0';

#define HT_REQUEST_END
#define HTT_REQUEST_START opt->state.HTbuff[0]='\0';

#define HTT_REQUEST_END
#define HTS_REQUEST_START opt->state.HTbuff[0]='\0';

#define HTS_REQUEST_END
#define HTS_PANIC_PRINTF(S) strcpybuff(opt->state._hts_errmsg,S);

#endif

#endif
