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
/* File: Main file .h                                           */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Core engine declarations. Not an installed header, but part of the de-facto
   API surface: external consumers (e.g. httrack-android) read these structs and
   constants and call functions declared here. */
#ifndef HTS_CORE_DEFH
#define HTS_CORE_DEFH

#include "htsglobal.h"

/* specific definitions */
#include "htsbase.h"
/* Includes and definitions */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <conio.h>
#include <direct.h>
#else
#ifndef _WIN32
#include <unistd.h>
#endif
#endif
/* END specific definitions */

/* Forward definitions */
#ifndef HTS_DEF_FWSTRUCT_lien_url
#define HTS_DEF_FWSTRUCT_lien_url
typedef struct lien_url lien_url;
#endif
#ifndef HTS_DEF_FWSTRUCT_lien_back
#define HTS_DEF_FWSTRUCT_lien_back
typedef struct lien_back lien_back;
#endif
#ifndef HTS_DEF_FWSTRUCT_struct_back
#define HTS_DEF_FWSTRUCT_struct_back
typedef struct struct_back struct_back;
#endif
#ifndef HTS_DEF_FWSTRUCT_cache_back
#define HTS_DEF_FWSTRUCT_cache_back
typedef struct cache_back cache_back;
#endif
#ifndef HTS_DEF_FWSTRUCT_hash_struct
#define HTS_DEF_FWSTRUCT_hash_struct
typedef struct hash_struct hash_struct;
#endif
#ifndef HTS_DEF_FWSTRUCT_filecreate_params
#define HTS_DEF_FWSTRUCT_filecreate_params
typedef struct filecreate_params filecreate_params;
#endif

// Include htslib.h for all types
#include "htslib.h"

// options
#include "htsopt.h"

// HTTrack engine sub-headers

// main entry point
#include "htscoremain.h"

// core routines
#include "htscore.h"

// misc tools for httrack.c
#include "htstools.h"

// command-line help
#include "htshelp.h"

// build the on-disk save filename
#include "htsname.h"

// FTP support
#include "htsftp.h"

// URL interception
#include "htscatchurl.h"

// robots.txt handling
#include "htsrobots.h"

// link-acceptance rules
#include "htswizard.h"

// regexp/filter routines
#include "htsfilters.h"

// download backing (the back[] slot ring)
#include "htsback.h"

// cache handling
#include "htscache.h"

// hashing
#include "htshash.h"
#include "coucal.h"

#include "htsdefines.h"

#include "hts-indextmpl.h"

/** A remote URL split into host and path, each a fixed inline buffer
    (HTS_URLMAXSIZE*2 bytes, NUL-terminated). */
#ifndef HTS_DEF_FWSTRUCT_lien_adrfil
#define HTS_DEF_FWSTRUCT_lien_adrfil
typedef struct lien_adrfil lien_adrfil;
#endif
struct lien_adrfil {
  char adr[HTS_URLMAXSIZE * 2]; /**< host (address) */
  char fil[HTS_URLMAXSIZE * 2]; /**< remote file path */
};

/** A remote URL plus the local on-disk path it is saved to. */
#ifndef HTS_DEF_FWSTRUCT_lien_adrfilsave
#define HTS_DEF_FWSTRUCT_lien_adrfilsave
typedef struct lien_adrfilsave lien_adrfilsave;
#endif
struct lien_adrfilsave {
  lien_adrfil af;
  char save[HTS_URLMAXSIZE * 2]; /**< local save path (with directory) */
};

/** Per-slot connect-fallback bookkeeping (parallel to struct_back.lnk).
    Tracks which resolved address the slot is currently connecting to so a
    stuck connect can be retried against the next one. */
typedef struct hts_connect_fallback {
  int addr_index;       /**< candidate being connected (0-based) */
  int addr_count;       /**< resolved addresses; -1 = not yet probed */
  TStamp connect_start; /**< when the current candidate's connect began */
} hts_connect_fallback;

/** The download-slot ring: the set of concurrent transfers in flight.
    Allocated/owned by the engine; consumers (status callbacks, the loop)
    read it but do not resize or free it. */
#ifndef HTS_DEF_FWSTRUCT_struct_back
#define HTS_DEF_FWSTRUCT_struct_back
typedef struct struct_back struct_back;
#endif
struct struct_back {
  lien_back *lnk;         /**< slot array, valid indices [0..count-1]
                               (count+1 entries allocated); a slot is
                               active iff lnk[i].status != STATUS_FREE.
                               See struct lien_back in htsopt.h and the
                               STATUS_* codes in htsbasenet.h. */
  int count;              /**< number of usable slots (back_max) */
  coucal ready;           /**< index of slots whose transfer completed */
  LLint ready_size_bytes; /**< total bytes buffered in completed slots */
  hts_connect_fallback *connect_fallback; /**< per-slot, count+1 entries */
};

typedef struct cache_back_zip_entry cache_back_zip_entry;

/** Open handle to the mirror cache (the read-from-old / write-to-new state
    used to resume and to avoid re-fetching unchanged files). Engine-owned. */
#ifndef HTS_DEF_FWSTRUCT_cache_back
#define HTS_DEF_FWSTRUCT_cache_back
typedef struct cache_back cache_back;
#endif
struct cache_back {
  int version; /**< cache-file format version being read */
  /* */
  int type;
  int ro;                   /**< read-only: no new cache is written */
  FILE *dat, *ndx, *olddat; /**< new data, new index, old data files */
  char *use;                /**< in-memory list of cached adr+fil keys */
  FILE *lst;                /**< file list, used for purge */
  FILE *txt;                /**< human-readable file list (info) */
  char lastmodified[256];
  // HASH
  coucal hashtable;
  // HASH for tests (naming subsystem)
  coucal cached_tests;
  /* optional log files */
  FILE *log;
  FILE *errlog;
  /* read-ahead cursors into the old cache */
  int ptr_ant;
  int ptr_last;
  /* ZIP-backed cache backend (newer format) */
  void *zipInput;
  void *zipOutput;
  cache_back_zip_entry *zipEntries;
  int zipEntriesOffs;
  int zipEntriesCapa;
  hts_boolean
      zipWriteFailed; /**< a cache write failed; stop touching the stream */
};

#ifndef HTS_DEF_FWSTRUCT_hash_struct
#define HTS_DEF_FWSTRUCT_hash_struct
typedef struct hash_struct hash_struct;
#endif
/** Lookup indexes over the link heap: map save-name / URL back to a link, so a
    URL seen twice resolves to one entry. The coucal tables index into liens;
    they do not own the links. */
struct hash_struct {
  /* points at the engine's link array (opt->liens); not owned */
  const lien_url *const*const*liens;
  /* save-name -> link index (case-insensitive: keys lowercased) */
  coucal sav;
  /* address+path -> link index */
  coucal adrfil;
  /* former address+path -> link index (renamed/moved entries) */
  coucal former_adrfil;
  /* effective urlhack sub-flags: www.==host / // collapse / query-arg sort */
  hts_boolean norm_host;
  hts_boolean norm_slash;
  hts_boolean norm_query;
  /* query-strip keys (not owned); set from opt->strip_query at hash_init */
  const char *strip_query;
  char normfil[HTS_URLMAXSIZE * 2];
  char normfil2[HTS_URLMAXSIZE * 2];
  char catbuff[CATBUFF_SIZE];
};

#ifndef HTS_DEF_FWSTRUCT_filecreate_params
#define HTS_DEF_FWSTRUCT_filecreate_params
typedef struct filecreate_params filecreate_params;
#endif
/** Parameters threaded through file-creation callbacks (filenote). */
struct filecreate_params {
  FILE *lst; /**< open file list to append created paths to */
  char path[HTS_URLMAXSIZE * 2];
};

/* Convenience accessors over the link heap; assume `opt` (and where used,
   `ptr`/`parent_relative`) are in scope. heap(N) is the Nth link;
   heap_top_index() is the last recorded link's index. */
#define heap(N)            (opt->liens[N])

#define heap_top_index()   (opt->lien_tot - 1)

#define heap_top()         (heap(heap_top_index()))

#define urladr()           (heap(ptr)->adr)

#define urlfil()           (heap(ptr)->fil)

#define savename()         (heap(ptr)->sav)

#define parenturladr()     (heap(heap(ptr)->precedent)->adr)

#define parenturlfil()     (heap(heap(ptr)->precedent)->fil)

#define parentsavename()   (heap(heap(ptr)->precedent)->sav)

#define relativeurladr()   ((!parent_relative)?urladr():parenturladr())

#define relativeurlfil()   ((!parent_relative)?urlfil():parenturlfil())

#define relativesavename() ((!parent_relative)?savename():parentsavename())

/* Library-internal helpers (engine-only, HTS_INTERNAL_BYTECODE). */
#ifdef HTS_INTERNAL_BYTECODE

/* True if a new cache is being written (plain or zip backend). */
HTS_STATIC int cache_writable(cache_back * cache) {
  return (cache != NULL && (cache->dat != NULL || cache->zipOutput != NULL));
}

/* True if an old cache is available to read (plain or zip backend). */
HTS_STATIC int cache_readable(cache_back * cache) {
  return (cache != NULL && (cache->olddat != NULL || cache->zipInput != NULL));
}

#endif

// Functions

/* Library-internal only (engine TUs). */
#ifdef HTS_INTERNAL_BYTECODE

char *hts_cancel_file_pop(httrackp * opt);

#endif

/* Record a link on the heap. All strings are copied (caller keeps ownership).
   Returns 1 on success, 0 if the link limit (opt->maxlink) is reached. */
int hts_record_link(httrackp * opt,
                    const char *address, const char *file, const char *save,
                    const char *ref_address, const char *ref_file,
                    const char *codebase);

/* Index of the most recently recorded link. */
size_t hts_record_link_latest(httrackp *opt);

/* Mark link at index lpos as not to be processed (sets pass2 = -1). */
void hts_invalidate_link(httrackp * opt, int lpos);

/* Reset / free the engine's link heap. */
void hts_record_init(httrackp *opt);

void hts_record_free(httrackp *opt);

/* Run the mirror for the given start URL(s) under opt. Top-level engine entry.
 */
int httpmirror(char *url1, httrackp * opt);

/* Write len bytes of adr to local path s. url_adr/url_fil (may be NULL) name
   the source URL for logging/notification. */
int filesave(httrackp * opt, const char *adr, int len, const char *s,
             const char *url_adr /* = NULL */ ,
             const char *url_fil /* = NULL */ );

char *hts_cancel_file_pop(httrackp * opt);

int check_fatal_io_errno(void);

int engine_stats(void);

void host_ban(httrackp * opt, int ptr, struct_back * sback, const char *host);

/* Open local file s for writing (filecreate, truncate) or appending
   (fileappend), creating parent directories as needed. Return an open FILE*
   the caller must fclose(), or NULL on failure. */
FILE *filecreate(filenote_strc * strct, const char *s);

FILE *fileappend(filenote_strc * strct, const char *s);

/* Create an empty file, return 1 on success, 0 on failure. */
int filecreateempty(filenote_strc * strct, const char *filename);

int filenote(filenote_strc * strct, const char *s, filecreate_params * params);

void file_notify(httrackp * opt, const char *adr, const char *fil,
                 const char *save, int create, int modify, int wasupdated);

void usercommand(httrackp * opt, int exe, const char *cmd, const char *file,
                 const char *adr, const char *fil);

void usercommand_exe(const char *cmd, const char *file);

// Finish the makeindex index.html (footer + refresh meta), run usercommand.
// Updates *makeindex_done/*makeindex_fp in place; adr/fil are the mode strings.
void hts_finish_makeindex(httrackp *opt, int *makeindex_done,
                          FILE **makeindex_fp, int makeindex_links,
                          const char *makeindex_firstlink,
                          const char *template_footer, const char *adr,
                          const char *fil);

int filters_init(char ***ptrfilters, int maxfilter, int filterinc);

int fspc(httrackp * opt, FILE * fp, const char *type);

char *next_token(char *p, int flag);

/* Like fil_normalized(), but first drops query keys in STRIP (comma-separated,
   "*" = all); STRIP NULL/empty behaves exactly like fil_normalized(). */
char *fil_normalized_filtered(const char *source, char *dest,
                              const char *strip);

/* As fil_normalized_filtered(), but DO_SLASH/DO_QUERY gate the // collapse and
   the query-argument sort independently (the urlhack sub-flags). */
char *fil_normalized_filtered_ex(const char *source, char *dest,
                                 const char *strip, int do_slash, int do_query);

/* For URL ADR/FIL, return (in DEST) the comma keylist to strip from the
   '\n'-separated "[pattern=]keys" RULES (patterns matched on host/path via
   strjoker, last wins); NULL if none match. Feeds fil_normalized_filtered(). */
const char *hts_query_strip_keys(const char *rules, const char *adr,
                                 const char *fil, char *dest, size_t destsize);

/* Read a whole file into a freshly malloc'd, NUL-terminated buffer; the caller
   owns it and must release it with freet(). Return NULL on missing/unreadable
   file (readfile_or substitutes defaultdata instead). The byte content is NOT
   transcoded except readfile_utf8, which expects a UTF-8 path. readfile2
   reports the byte size (excluding the NUL) via *size when non-NULL. */
char *readfile(const char *fil);

char *readfile2(const char *fil, LLint * size);

char *readfile_utf8(const char *fil);

char *readfile_or(const char *fil, const char *defaultdata);

#if 0
void check_rate(TStamp stat_timestart, int maxrate);
#endif

/* Backing (download-slot) scheduler. Operate on the back[] ring (struct_back).
   Not thread-safe; call from the single crawl loop. */

/* True if a connecting slot should give up on the current address and try the
   next one: a fallback address remains (addr_index+1 < addr_count) and the
   candidate has been connecting for at least its deadline, min(timeout, an
   internal cap). elapsed/timeout in seconds. Exposed for the -#D self-test. */
int back_connect_fallback_due(int addr_index, int addr_count, int elapsed,
                              int timeout);

/* How many new sockets may be opened now, honoring maxsoc and the maxconn rate
   limit (>=0). _strict ignores reserved-slot headroom; the plain form leaves
   room for naming tests and stops at 0 when the stack is nearly full. */
int back_pluggable_sockets(struct_back * sback, httrackp * opt);

int back_pluggable_sockets_strict(struct_back * sback, httrackp * opt);

/* Randomized inter-file pause target in [min_ms,max_ms] (#185), derived from a
   timestamp seed so it is stable within one gap and rerolls per launch. */
int hts_pause_target_ms(TStamp seed, int min_ms, int max_ms);

/* Schedule more links from the heap into free slots. Returns the number queued,
   or <=0 if none could be added (no free slot / paused / stopped). */
int back_fill(struct_back * sback, httrackp * opt, cache_back * cache,
              int ptr, int numero_passe);

/* Count of links already finished (in background or served from cache). */
int backlinks_done(const struct_back *sback, lien_url **liens, int lien_tot,
                   int ptr);

/* Like back_fill, but a no-op (returns -1) when in-memory buffered data already
   exceeds opt->maxcache. */
int back_fillmax(struct_back * sback, httrackp * opt, cache_back * cache,
                 int ptr, int numero_passe);

/* Interactive prompt: continue an interrupted mirror? Returns nonzero to go on.
 */
int ask_continue(httrackp * opt);

/* Number of decimal digits in n. */
int nombre_digit(int n);

// Java
#if 0
int hts_add_file(char *file, int file_position);
#endif

// Polling
#if HTS_POLL
int check_flot(T_SOC s);

int check_stdin(void);

int read_stdin(char *s, int max);
#endif
/* Socket readiness probes: nonzero if the socket has an error / has data. */
int check_sockerror(T_SOC s);

int check_sockdata(T_SOC s);

/* external modules: register a link discovered by a parser plugin. */
int htsAddLink(htsmoduleStruct * str, char *link);

/* No-op function (used as a do-nothing callback / to defeat optimizers). */
void voidf(void);

/* HTML marker comment marking where the top index is spliced. */
#define HTS_TOPINDEX "TOP_INDEX_HTTRACK"

#endif
