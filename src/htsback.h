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
/*       backing system (multiple socket download)              */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTSBACK_DEFH
#define HTSBACK_DEFH

#include "htsglobal.h"

#include "htsopt.h"
#include "htsthread.h"
#include "htsbuf.h"

/* Forward definitions */
#ifndef HTS_DEF_FWSTRUCT_httrackp
#define HTS_DEF_FWSTRUCT_httrackp
typedef struct httrackp httrackp;
#endif
#ifndef HTS_DEF_FWSTRUCT_struct_back
#define HTS_DEF_FWSTRUCT_struct_back
typedef struct struct_back struct_back;
#endif
#ifndef HTS_DEF_FWSTRUCT_cache_back
#define HTS_DEF_FWSTRUCT_cache_back
typedef struct cache_back cache_back;
#endif
#ifndef HTS_DEF_FWSTRUCT_lien_back
#define HTS_DEF_FWSTRUCT_lien_back
typedef struct lien_back lien_back;
#endif
#ifndef HTS_DEF_FWSTRUCT_htsblk
#define HTS_DEF_FWSTRUCT_htsblk
typedef struct htsblk htsblk;
#endif

/* An FTP worker publishes a slot's status, so read it with an acquire load. */
static HTS_INLINE HTS_UNUSED int back_status(const lien_back *b) {
  return hts_load_acquire_int(&b->status);
}

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE

// create/destroy
/* Slots for maxsoc connections, clamped to what back_new() can size. */
int back_max_slots(int maxsoc);
struct_back *back_new(httrackp *opt, int back_max);
void back_free(struct_back ** sback);

// backing
#define BACK_ADD_TEST "(dummy)"
#define BACK_ADD_TEST2 "(dummy2)"
int back_index(httrackp * opt, struct_back * sback, const char *adr, const char *fil,
               const char *sav);
int back_available(const struct_back * sback);
LLint back_incache(const struct_back * sback);
int back_done_incache(const struct_back * sback);
/* Like back_index(), but never loads a stored result into a slot. Returns
   sback->count for one. */
int back_index_peek(httrackp *opt, struct_back *sback, const char *adr,
                    const char *fil, const char *sav);
HTS_INLINE int back_exist(struct_back * sback, httrackp * opt, const char *adr,
                          const char *fil, const char *sav);
int back_nsoc(const struct_back *sback);
/* refetch_whole forces a whole-file GET and spends the free restart (#581). */
int back_add(struct_back *sback, httrackp *opt, cache_back *cache,
             const char *adr, const char *fil, const char *save,
             const char *referer_adr, const char *referer_fil, int test,
             hts_boolean refetch_whole, const hts_fetch_meta *fetch);
int back_add_if_not_exists(struct_back *sback, httrackp *opt, cache_back *cache,
                           const char *adr, const char *fil, const char *save,
                           const char *referer_adr, const char *referer_fil,
                           int test, const hts_fetch_meta *fetch);
int back_search_quick(struct_back * sback);
/* Does the busy bitmap cover every slot not STATUS_FREE? With exact, does it
   also hold no free slot (true right after back_clean())? */
hts_boolean back_busy_consistent(const struct_back *sback, hts_boolean exact);
/* Finds a live keep-alive slot for search_addr, or returns -1. */
int back_searchlive(httrackp *opt, struct_back *sback, const char *search_addr);
void back_clean(httrackp *opt, cache_back *cache, struct_back *sback);
void back_wait(struct_back *sback, httrackp *opt, cache_back *cache,
               TStamp stat_timestart);
void back_copy_static(const lien_back * src, lien_back * dst);
/* The .ref record's framing, shared with the self-test that pins it. */
#define HTS_REF_MAGIC "HTSREF1\n"
#define HTS_REF_MAGIC_SIZE 8
#define HTS_REF_VERSION 1
/* Caps a declared length must clear: one response's header block, and a body
   only an in-memory transfer ever carries. */
#define HTS_REF_MAX_STR (1024 * 1024)
#define HTS_REF_MAX_BLOB ((uint64_t) 1024 * 1024 * 1024)

/* The slot spool is host-native and valid only in the run that wrote it. */
hts_boolean back_serialize(FILE *fp, const lien_back *src);
hts_boolean back_unserialize(FILE *fp, lien_back **dst);
/* The resume state outlives the run, so its format is portable. */
int back_serialize_ref(httrackp * opt, const lien_back * src);
int back_unserialize_ref(httrackp * opt, const char *adr, const char *fil,
                         lien_back ** dst);
void back_set_finished(httrackp *opt, struct_back *sback, const int p);
/* Make slot p a live slot in state status, reading r's open socket as if
   back_add() had connected it, with no timeout. For back_wait() drivers with no
   network. The slot takes ownership of r's socket and buffers. */
void back_slot_attach(struct_back *sback, const int p, const int status,
                      const htsblk *r, const char *adr, const char *fil);
/* Fail slot p as statuscode and finish it. msg is clipped into r.msg, and NULL
   keeps the reason already there. The caller closes the connection. */
void back_slot_fail(httrackp *opt, struct_back *sback, const int p,
                    const int statuscode, const char *msg);
/* Close slot p as non-fatal, keeping a reason already set over fallback. */
void back_slot_fail_nonfatal(httrackp *opt, struct_back *sback, const int p,
                             const char *fallback);
/* lien_back.locked: 1 keeps a slot in the table with its body in memory, and
   BACK_PINNED keeps it in the table but lets it write to disk.
   back_set_unlocked() releases both. */
#define BACK_PINNED 2
void back_set_locked(struct_back * sback, const int p);
void back_set_pinned(struct_back *sback, const int p);
void back_set_unlocked(struct_back * sback, const int p);
int back_delete(httrackp * opt, cache_back * cache, struct_back * sback,
                const int p);
/* Discard back's on-disk .delayed placeholder and its refname. */
void back_delayed_discard(httrackp *opt, lien_back *back);
/* Did back_add() settle this slot without a reply or a body, as a cache hit
   whose .delayed file is missing does? A reused socket still waiting is not. */
hts_boolean back_delayed_settled_empty(const lien_back *back);
/* Move back's .delayed placeholder (and open stream) to newname, or remove
   one an earlier run may have left; HTS_FALSE = file lost, slot flagged in
   error. */
hts_boolean back_delayed_rename(httrackp *opt, lien_back *back,
                                const char *newname);
int back_clear_entry(lien_back *back);
void back_delete_all(httrackp * opt, cache_back * cache, struct_back * sback);
/* Keep slot p alive for keep-alive when allowed, else delete it. */
void back_maydelete(httrackp *opt, cache_back *cache, struct_back *sback,
                    const int p);
/* Close slot p's transfer and store it; a refusal is logged here. */
void back_finalize(httrackp *opt, cache_back *cache, struct_back *sback,
                   const int p);
/* Did the fetch fail to produce a response, as opposed to the engine
   deliberately passing the resource over? Only the latter may be purged, so
   this also decides whether the previous copy survives the update (#746). */
hts_boolean back_transfer_failed(const int statuscode);
/* Commit a decoded body size onto r, refusing in memory what http_xfread1()
   refuses. A refusal drops the coded body and returns HTS_FALSE. */
hts_boolean back_set_decoded_size(htsblk *r, LLint size);
/* Back up url_sav to tmpfile for back_finalize() to restore on failure. Call
   right before truncating url_sav. */
void back_refetch_backup(httrackp *opt, lien_back *const back);
/* Commit or restore the re-fetch backup in back->tmpfile. HTS_FALSE means a
   commit had to restore, so the caller must not cache these validators. */
hts_boolean back_finalize_backup(httrackp *opt, lien_back *const back,
                                 hts_boolean commit);
/* Remove the reserved directory a temporary sat in, once the last slot sharing
   it is done; a non-empty one just refuses. No-op outside that directory. */
void back_tmpdir_drop(const char *tmp);
/* Name the spool file of a frozen backlog slot. Consumes an
   opt->state.tmpnameid. Returns HTS_FALSE (dest emptied) if the name does not
   fit. Note: utf-8. */
HTS_CHECK_RESULT hts_boolean back_spoolname(httrackp *opt, hts_buf dest);
/* Returns this process's id, as back_spoolname() writes it. */
unsigned long back_self_pid(void);
/* Write a slot to its spool file. Returns HTS_FALSE, leaving no file, on
   failure. */
hts_boolean back_spool_write(httrackp *opt, const char *filename,
                             const lien_back *back);
/* The spool file holding a ready-table entry, or NULL when it is in memory. */
const char *back_spooled_file(const void *spooled);
/* Moves every finished, unlocked slot to the spool and returns how many. */
int back_cleanup_background(httrackp *opt, cache_back *cache,
                            struct_back *sback);
/* Remove spool files whose process is gone or is this one. Call before this
   run spools anything. */
void back_spool_sweep(httrackp *opt);
/* Can this slot be swapped out to disk? Hidden, shared with the self-test. */
hts_boolean slot_can_be_cached_on_disk(const lien_back *back);
void back_info(struct_back * sback, int i, int j, FILE * fp);
void back_infostr(struct_back *sback, int i, int j, hts_buf s);
LLint back_transferred(LLint add, struct_back * sback);

/* Enforce -M/-E quotas: smooth-stops when reached; returns 0 once the -M cap
   or -E deadline overruns its grace period (callers must stop waiting). */
int back_checkmirror(httrackp * opt);
/* Give up the mirror after a worker thread faulted. Call it from the crawl
   thread. Idempotent. */
void back_check_worker_fault(httrackp *opt);

#endif

/* True if a connecting slot should give up on the current address and try the
   next one: a fallback address remains (addr_index+1 < addr_count) and the
   candidate has been connecting for at least its deadline, min(timeout, an
   internal cap). elapsed_ms in milliseconds, timeout in seconds. Exposed for
   the -#D self-test. */
int back_connect_fallback_due(int addr_index, int addr_count, TStamp elapsed_ms,
                              int timeout);

#endif
