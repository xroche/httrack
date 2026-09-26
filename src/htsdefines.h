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
 *  The engine's extension surface: the function pointer type of every callback
 *  a wrapper or a parser module can implement, and the table the engine
 *  dispatches through. Install one by the string name in its field gloss below
 *  with htswrap_add(), or by its field name with CHAIN_FUNCTION(). */
#ifndef HTS_DEFINES_DEFH
#define HTS_DEFINES_DEFH

/* Forward declarations of engine structs, so this header is usable without
   pulling in their full definitions. Each is guarded so several public headers
   can repeat the typedef without clashing. */
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

/** Marks a symbol a wrapper module exports back to the engine. A plug-in must
    carry it on its hts_plug() and hts_unplug(), or -fvisibility=hidden keeps
    the engine from finding them. */
#ifndef EXTERNAL_FUNCTION
#ifdef _WIN32
#define EXTERNAL_FUNCTION __declspec(dllexport)
#elif ((defined(__GNUC__) && (__GNUC__ >= 4)) ||                               \
       (defined(HAVE_VISIBILITY) && HAVE_VISIBILITY))
#define EXTERNAL_FUNCTION __attribute__((visibility("default")))
#else
#define EXTERNAL_FUNCTION
#endif
#endif

/** Entry point of a --wrapper plug-in, run once as the module is loaded:
    install your callbacks on @p opt here. @p argv is the argument string given
    after the module name, and the module name itself for the parser modules the
    engine preloads. Return 1 for success. On any other value the engine calls
    hts_unplug() and unloads the module. */
typedef int (*t_hts_plug)(httrackp *opt, const char *argv);

/** Tear-down entry point of a --wrapper plug-in, run from hts_free_opt() and
    when hts_plug() refused. The return value is ignored. */
typedef int (*t_hts_unplug)(httrackp *opt);

/* Engine callback prototypes, one per hook the engine fires at a defined point
   of a mirror. carg is the argument node CHAIN_FUNCTION() built for this
   callback, read with CALLBACKARG_USERDEF(). Unless a hook says otherwise,
   every string a callback receives is valid for that call only, and the engine
   fires every hook from the thread running the mirror (the one blocked in
   hts_main2()), so no two callbacks run at once. What an int return means
   differs from hook to hook, so read each one. */

/** Fires once just before the mirror starts, after the command line is parsed.
    Allocate per-run state here. It gets no httrackp, so a wrapper keeps the one
    hts_plug() handed it. */
typedef void (*t_hts_htmlcheck_init)(t_hts_callbackarg *carg);

/** Fires once after the mirror has ended and the cache has been written.
    Release per-run state here. */
typedef void (*t_hts_htmlcheck_uninit)(t_hts_callbackarg *carg);

/** Fires when the mirror is about to walk its first link. Return 0 to give up
    before anything is fetched, and the engine still fires the end hook. */
typedef int (*t_hts_htmlcheck_start)(t_hts_callbackarg *carg, httrackp *opt);

/** Fires when the mirror's link loop is over, including after a start hook
    refused the run. The return value is ignored. */
typedef int (*t_hts_htmlcheck_end)(t_hts_callbackarg *carg, httrackp *opt);

/** Fires whenever the engine has changed the options: once before the mirror
    starts, and again each time the user toggles the display at the console.
    Read or adjust @p opt here. The return value is ignored. */
typedef int (*t_hts_htmlcheck_chopt)(t_hts_callbackarg *carg, httrackp *opt);

/** Shared prototype of the two page-rewriting hooks. On entry *@p html is the
    page and *@p len its length, and @p url_adresse with @p url_fichier name the
    host and path it came from. Store the new address and length back through
    the two pointers and return 1 for the engine to take them. Any other return
    means you changed nothing. The two hooks own that buffer differently, so
    read the aliases below. */
typedef int (*t_hts_htmlcheck_process)(t_hts_callbackarg *carg, httrackp *opt,
                                       char **html, int *len,
                                       const char *url_adresse,
                                       const char *url_fichier);

/** Runs on a fetched page before it is parsed, over a NUL-terminated buffer.
    The engine takes over whatever address you store back and frees it, so
    allocate a replacement with hts_malloc() or hts_strdup(), and hts_free() the
    buffer you were handed unless you grew it with hts_realloc(). */
typedef t_hts_htmlcheck_process t_hts_htmlcheck_preprocess;

/** Runs on the rewritten page before it is written to disk, over *@p len bytes
    with no terminator. The buffer belongs to the engine, so never free or
    realloc it. Either edit it where it is and return an address inside it,
    keeping or shrinking the length, or return a scratch buffer of your own that
    you reuse for every page. The engine skips the page when the address and
    length it gets back do not fit either shape. */
typedef t_hts_htmlcheck_process t_hts_htmlcheck_postprocess;

/** Runs on a fetched page just after the preprocess hook and before the parse.
    @p html and @p len are the page as the parser will see it, but a rewrite
    belongs in the preprocess hook rather than here. Return 0 and the engine
    drops the page: it takes no link from it and writes no local copy. */
typedef int (*t_hts_htmlcheck_check_html)(t_hts_callbackarg *carg,
                                          httrackp *opt, char *html, int len,
                                          const char *url_adresse,
                                          const char *url_fichier);

/** Declared and installable, but the engine never asks it anything. */
typedef const char *(*t_hts_htmlcheck_query)(t_hts_callbackarg *carg,
                                             httrackp *opt,
                                             const char *question);

/** Asks the user to confirm before the mirror starts, because the engine found
    a cache or a lock file in the output directory. Skipped under --quiet.
    @p question is the text to show. Answer "N", "NO" or "NON" in any case and
    hts_main2() returns 0 without mirroring. Any other answer, an empty one, or
    NULL continues. The engine reads the string before it asks again, so a
    static buffer will do. */
typedef const char *(*t_hts_htmlcheck_query2)(t_hts_callbackarg *carg,
                                              httrackp *opt,
                                              const char *question);

/** Asks the user what to do with a link that falls outside the mirror scope,
    while --wizard is on. @p question is that link, as "host/path". The answer
    is read as a number: 0 or an empty answer drops the link, 1 its directory
    and below, 2 its whole host, 4 takes the page but none of the links inside
    it, 5 takes the link, 6 every link on its host, and 7 the files of its
    directory. "*" drops the link and stops the questions. Anything the engine
    cannot read drops the link and records the refusal. The engine reads the
    string before it asks again, so a static buffer will do. */
typedef const char *(*t_hts_htmlcheck_query3)(t_hts_callbackarg *carg,
                                              httrackp *opt,
                                              const char *question);

/** query3 answer HTS_WIZARD_SCOPE_INCLUDE + k takes the k-th host scope
    hts_wizard_host_scope() enumerates for the question. */
#define HTS_WIZARD_SCOPE_INCLUDE 1000

/** query3 answer HTS_WIZARD_SCOPE_EXCLUDE + k drops that k-th host scope. */
#define HTS_WIZARD_SCOPE_EXCLUDE 2000

/** Fires on every tick of the mirror loop, often several times a second, so
    keep it cheap. @p back is the array of @p back_max transfer slots and
    @p back_index the slot this tick is about, or -1 for none. Despite their
    names, @p lien_tot carries the index of the link the engine is working on
    and @p lien_ntot the number of links queued. @p stat_time is the seconds
    since the mirror started, and @p stats the running totals. @p back and
    @p stats are NULL on the ticks fired before the first transfer and while
    --waittime waits. Return 0 to stop the mirror. */
typedef int (*t_hts_htmlcheck_loop)(t_hts_callbackarg *carg, httrackp *opt,
                                    lien_back *back, int back_max,
                                    int back_index, int lien_tot, int lien_ntot,
                                    int stat_time, hts_stat_struct *stats);

/** Decides whether a link may be fetched, as the engine picks it up from a page
    or follows a redirect, before any transfer. @p adr is the host, @p fil the
    path, and @p status the engine's own verdict so far, where 0 accepts,
    1 refuses and -1 means undecided. Return the verdict the engine should
    keep, in that same encoding, or -1 to leave the engine's own alone. */
typedef int (*t_hts_htmlcheck_check_link)(t_hts_callbackarg *carg,
                                          httrackp *opt, const char *adr,
                                          const char *fil, int status);

/** Decides whether a link may still be fetched now that its type is known,
    after the response headers arrive and before the body. @p mime is the
    content type and @p status the engine's verdict so far. Return 0 to accept,
    1 to refuse, then the engine closes the connection and marks the file
    excluded, or -1 to leave the engine's verdict alone. */
typedef int (*t_hts_htmlcheck_check_mime)(t_hts_callbackarg *carg,
                                          httrackp *opt, const char *adr,
                                          const char *fil, const char *mime,
                                          int status);

/** Fires when the mirror is paused, either by the hts-paused.lock request or by
    --fragment. The engine does not wait by itself, so block here until the
    mirror should go on. The built-in hook polls @p lockfile until it is
    gone. */
typedef void (*t_hts_htmlcheck_pause)(t_hts_callbackarg *carg, httrackp *opt,
                                      const char *lockfile);

/** Fires after a file has been written to disk. @p file is its local path. */
typedef void (*t_hts_htmlcheck_filesave)(t_hts_callbackarg *carg, httrackp *opt,
                                         const char *file);

/** Fires for every file the mirror decided about, whether it wrote it or left
    it alone. @p hostname and @p filename name the source, @p localfile the file
    on disk. @p is_new is set for a file that was not there before,
    @p is_modified for one whose content changed, @p not_updated for one the
    mirror kept as it was. An FTP transfer fires this hook from its own worker
    thread, so this is the one hook that must be thread-safe. */
typedef void (*t_hts_htmlcheck_filesave2)(t_hts_callbackarg *carg,
                                          httrackp *opt, const char *hostname,
                                          const char *filename,
                                          const char *localfile, int is_new,
                                          int is_modified, int not_updated);

/** Fires for each link found in a page, and for each link a parser module adds.
    @p link may be edited where it is, but nothing passes its size, so never
    make the string longer. Return 0 to drop the link. */
typedef int (*t_hts_htmlcheck_linkdetected)(t_hts_callbackarg *carg,
                                            httrackp *opt, char *link);

/** Same as the linkdetected hook, plus @p tag_start, the markup the link was
    found in, NULL when a parser module added the link. The engine skips this
    hook when the linkdetected one already refused the link. */
typedef int (*t_hts_htmlcheck_linkdetected2)(t_hts_callbackarg *carg,
                                             httrackp *opt, char *link,
                                             const char *tag_start);

/** Fires once per transfer slot, when its transfer has finished, whether it
    succeeded or failed. @p back holds that slot's result. The return value is
    ignored. */
typedef int (*t_hts_htmlcheck_xfrstatus)(t_hts_callbackarg *carg, httrackp *opt,
                                         lien_back *back);

/** Fires once the engine has computed the local path a URL saves to, so a
    wrapper can change it. @p adr_complete and @p fil_complete name it, and
    @p referer_adr with @p referer_fil the page that linked it. @p save holds
    the path and may be rewritten in place, within HTS_URLMAXSIZE*2 bytes and
    leaving room for the collision suffix and the ".delayed" marker the engine
    may still append. The engine strips "../" before this hook and not after,
    so a path written here can leave the mirror directory. The return value is
    ignored. */
typedef int (*t_hts_htmlcheck_savename)(t_hts_callbackarg *carg, httrackp *opt,
                                        const char *adr_complete,
                                        const char *fil_complete,
                                        const char *referer_adr,
                                        const char *referer_fil, char *save);

/** Same prototype as the savename hook. Declared and installable, but the
    engine never calls it. */
typedef t_hts_htmlcheck_savename t_hts_htmlcheck_extsavename;

/** Fires with the request headers built and about to go out. @p buff is the
    whole NUL-terminated request block and may be edited where it is, and the
    engine sends whatever it holds on return and passes no size for it.
    @p outgoing is the connection being set up. Return 1 to send the request.
    Any other value drops the connection and fails the transfer. */
typedef int (*t_hts_htmlcheck_sendhead)(t_hts_callbackarg *carg, httrackp *opt,
                                        char *buff, const char *adr,
                                        const char *fil,
                                        const char *referer_adr,
                                        const char *referer_fil,
                                        htsblk *outgoing);

/** Fires with the response headers parsed and the body not read yet. @p buff is
    the raw header block, @p incoming the parsed response, whose fields (status
    code, content type) this hook may change. Return 1 to go on. Any other value
    closes the connection and fails this one transfer, not the mirror. */
typedef int (*t_hts_htmlcheck_receivehead)(t_hts_callbackarg *carg,
                                           httrackp *opt, char *buff,
                                           const char *adr, const char *fil,
                                           const char *referer_adr,
                                           const char *referer_fil,
                                           htsblk *incoming);

/** Asks a parser module whether it handles this document. @p str carries the
    document and its context. Name your module in str->wrapper_name. Return
    non-zero to claim the document, and the engine then calls the parse hook
    unless --mod-blacklist names that module. Return 0 to pass. */
typedef int (*t_hts_htmlcheck_detect)(t_hts_callbackarg *carg, httrackp *opt,
                                      htsmoduleStruct *str);

/** Extracts the links of a document the detect hook claimed, and hands each one
    to the engine through str->addLink. Return 1 on success, or 0 on failure
    with the reason in str->err_msg. */
typedef int (*t_hts_htmlcheck_parse)(t_hts_callbackarg *carg, httrackp *opt,
                                     htsmoduleStruct *str);

/* Callbacks */
#ifndef HTS_DEF_FWSTRUCT_t_hts_htmlcheck_callbacks
#define HTS_DEF_FWSTRUCT_t_hts_htmlcheck_callbacks
typedef struct t_hts_htmlcheck_callbacks t_hts_htmlcheck_callbacks;
#endif

/** Declares the callback slot NAME: its function pointer, typed
    t_hts_htmlcheck_<NAME>, beside the argument node handed to it. */
#define DEFCALLBACK(NAME)                                                      \
  struct NAME {                                                                \
    t_hts_htmlcheck_##NAME fun;                                                \
    t_hts_callbackarg *carg;                                                   \
  } NAME

/** Function pointer type of a slot reached without knowing which hook it is. */
typedef void *t_hts_htmlcheck_t_hts_htmlcheck_callbacks_item;

/** One slot, seen without its hook type. Every member of the table below is
    exactly this size. */
typedef DEFCALLBACK(t_hts_htmlcheck_callbacks_item);

/** The argument node a callback receives. CHAIN_FUNCTION() allocates one per
    installed callback and hts_free_opt() releases them all. Chaining lets a new
    hook wrap the one it displaced instead of replacing it. */
struct t_hts_callbackarg {
  void *userdef; /**< the pointer given to CHAIN_FUNCTION(), read back with
                      CALLBACKARG_USERDEF() */

  /** The callback this one displaced, so it can still be called. */
  struct prev {
    void *fun; /**< the displaced callback, NULL when there was none */
    t_hts_callbackarg *carg; /**< the argument node to pass to prev.fun */
  } prev;
};

/** The callback table, one slot per hook, held in an option set and dispatched
    by the engine. Install a slot either by the string name in its gloss with
    htswrap_add(), or by its field name with CHAIN_FUNCTION(). Every member must
    stay exactly one slot wide, because the engine walks the table as a flat
    array to release the chains. */
struct t_hts_htmlcheck_callbacks {
  /* v3.41 */
  DEFCALLBACK(init);          /**< "init" */
  DEFCALLBACK(uninit);        /**< "free" */
  DEFCALLBACK(start);         /**< "start" */
  DEFCALLBACK(end);           /**< "end" */
  DEFCALLBACK(chopt);         /**< "change-options" */
  DEFCALLBACK(preprocess);    /**< "preprocess-html" */
  DEFCALLBACK(postprocess);   /**< "postprocess-html" */
  DEFCALLBACK(check_html);    /**< "check-html" */
  DEFCALLBACK(query);         /**< "query", never called */
  DEFCALLBACK(query2);        /**< "query2" */
  DEFCALLBACK(query3);        /**< "query3" */
  DEFCALLBACK(loop);          /**< "loop" */
  DEFCALLBACK(check_link);    /**< "check-link" */
  DEFCALLBACK(check_mime);    /**< "check-mime" */
  DEFCALLBACK(pause);         /**< "pause" */
  DEFCALLBACK(filesave);      /**< "save-file" */
  DEFCALLBACK(filesave2);     /**< "save-file2" */
  DEFCALLBACK(linkdetected);  /**< "link-detected" */
  DEFCALLBACK(linkdetected2); /**< "link-detected2" */
  DEFCALLBACK(xfrstatus);     /**< "transfer-status" */
  DEFCALLBACK(savename);      /**< "save-name" */
  DEFCALLBACK(sendhead);      /**< "send-header" */
  DEFCALLBACK(receivehead);   /**< "receive-header" */
  DEFCALLBACK(detect);        /**< no string name, CHAIN_FUNCTION() only */
  DEFCALLBACK(parse);         /**< no string name, CHAIN_FUNCTION() only */
  /* >3.41 */
  DEFCALLBACK(extsavename); /**< no string name, and never called */
};

/* Library-internal helpers, compiled only inside the engine. */
#ifdef HTS_INTERNAL_BYTECODE

#include <stddef.h>

/** A callback slot's name beside its offset in the table. Declared here, and
    used by nothing. */
#ifndef HTS_DEF_FWSTRUCT_t_hts_callback_ref
#define HTS_DEF_FWSTRUCT_t_hts_callback_ref
typedef struct t_hts_callback_ref t_hts_callback_ref;
#endif
struct t_hts_callback_ref {
  const char *name; /**< the slot's string name */
  size_t offset;    /**< its byte offset in t_hts_htmlcheck_callbacks */
};

#ifdef __cplusplus
extern "C" {
#endif

/** The no-op callbacks the engine falls back to for each slot a consumer left
    empty. */
extern const t_hts_htmlcheck_callbacks default_callbacks;

#ifdef __cplusplus
}
#endif

/** Append A to the message being built in opt->state.HTbuff, so opt must be in
    scope. The buffer holds 2048 bytes and aborts on overflow. */
#define HT_PRINT(A) strcatbuff(opt->state.HTbuff, A);

/** Start a fresh message in opt->state.HTbuff, which is what the query2 hook is
    later asked about. */
#define HT_REQUEST_START opt->state.HTbuff[0] = '\0';

/** Closes an HT_REQUEST_START block, and does nothing. */
#define HT_REQUEST_END
/** Another spelling of HT_REQUEST_START. */
#define HTT_REQUEST_START opt->state.HTbuff[0] = '\0';

/** Another spelling of HT_REQUEST_END. */
#define HTT_REQUEST_END
/** Another spelling of HT_REQUEST_START, used by nothing. */
#define HTS_REQUEST_START opt->state.HTbuff[0] = '\0';

/** Another spelling of HT_REQUEST_END, used by nothing. */
#define HTS_REQUEST_END
/** Record S as the engine's error message, which hts_errmsg() returns. Needs
    opt in scope, and aborts when S does not fit. */
#define HTS_PANIC_PRINTF(S) strcpybuff(opt->state._hts_errmsg, S);

#endif

#endif
