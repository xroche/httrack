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
/* File: htsparse.h parser                                      */
/*       html/javascript/css parser                             */
/*       and other parser routines                              */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htsglobal.h"

/* Forward definitions */
#ifndef HTS_DEF_FWSTRUCT_htsblk
#define HTS_DEF_FWSTRUCT_htsblk
typedef struct htsblk htsblk;
#endif
#ifndef HTS_DEF_FWSTRUCT_robots_wizard
#define HTS_DEF_FWSTRUCT_robots_wizard
typedef struct robots_wizard robots_wizard;
#endif
#ifndef HTS_DEF_FWSTRUCT_hash_struct
#define HTS_DEF_FWSTRUCT_hash_struct
typedef struct hash_struct hash_struct;
#endif

#ifndef HTS_DEF_FWSTRUCT_htsmoduleStructExtended
#define HTS_DEF_FWSTRUCT_htsmoduleStructExtended
typedef struct htsmoduleStructExtended htsmoduleStructExtended;
#endif
struct htsmoduleStructExtended {
  /* Main object */
  htsblk *r_;

  /* Error handling */
  int *error_;
  volatile int *exit_xh_;
  int *store_errpage_;

  /* Structural */
  int *filptr_;
  char ***filters_;
  robots_wizard *robots_;
  hash_struct *hash_;

  /* Base & codebase */
  char *base;
  char *codebase;

  /* Index */
  int *makeindex_done_;
  FILE **makeindex_fp_;
  int *makeindex_links_;
  char *makeindex_firstlink_;

  /* Html templates */
  char *template_header_;
  char *template_body_;
  char *template_footer_;

  /* Specific to downloads */
  LLint *stat_fragment_;
  TStamp makestat_time;
  FILE *makestat_fp;
  LLint *makestat_total_;
  int *makestat_lnk_;
  FILE *maketrack_fp;

  /* Function-dependant */
  char *loc_;
  TStamp *last_info_shell_;
  int *info_shell_;

};

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE

/*
  Main parser, attempt to scan links inside the html/css/js file
  Parameters: The public module structure, and the private module variables
*/
int htsparse(htsmoduleStruct * str, htsmoduleStructExtended * stre);

/* Strip a default ":80" (any spelling) from an absolute link's authority, in
   place into a buffer of the given size. */
void hts_strip_default_port(char *lien, size_t size);

/*
  Does a quoted string the dirty parser found look like a link? "str" and "len"
  are the string as the source spells it. "lastc" is the first non-blank byte
  after its closing quote, and "inscript" says it sits in JavaScript or CSS,
  not in a tag attribute. The verdict is taken on what the cut at '#' and '?'
  leaves, so "/#top" is judged as "/". A string of HTS_URLMAXSIZE bytes or more
  is refused, a length the parser never offers.
*/
hts_boolean hts_dirty_link_is_url(httrackp *opt, const char *str, size_t len,
                                  char lastc, hts_boolean inscript);

/* A link the script scanner found, as an offset and a length from the cursor
   it was given. "unquoted_end" is the byte an unquoted CSS url() operand stops
   at, and '\0' when the operand was quoted. All three fields are zero when the
   scanner found nothing. */
typedef struct hts_js_link {
  int offset;
  int length;
  char unquoted_end;
} hts_js_link;

/*
  Does the script or CSS at "cursor" hand a URL to .src, .location, .href,
  .open, .replace, .link, url() or import? "buffer" is the first byte of the
  document, which the keyword tests read backwards from. "in_tag" says the
  script is an attribute value such as onclick="...", and "tag_lastc" the quote
  that attribute is written with. "in_css" lets url() take an unquoted operand.
  Fills "link" and answers true when a URL was found.
*/
hts_boolean hts_js_scan_link(httrackp *opt, const char *cursor,
                             const char *buffer, hts_boolean in_tag,
                             char tag_lastc, hts_boolean in_css,
                             hts_js_link *link);

/*
  May the dirty parser take the in-tag quoted value at "quote" for a link?
  "tag_start" is the tag's first byte, the '<'. False when the quote is not an
  attribute value. Also false for the attribute names that never carry one:
  hts_nodetect (id, name and friends) and an xmlns declaration.
*/
hts_boolean hts_dirty_attr_detectable(const char *quote, const char *tag_start);

/*
  Finds the attribute name owning the quoted value at "quote" inside the tag
  starting at "tag_start", spanning [name, *nend). Returns NULL when the quote
  is not an attribute value, and leaves *nend unspecified when it does.
*/
const char *hts_dirty_attr_name(const char *quote, const char *tag_start,
                                const char **nend);

/*
  Check for 301,302.. errors ("moved") and handle them; re-isuue requests, make
  rediretc file, handle filters considerations..
  Parameters: The public module structure, and the private module variables
  Returns 0 upon success
*/
int hts_mirror_check_moved(htsmoduleStruct * str,
                           htsmoduleStructExtended * stre);

/*
  Non-zero if a redirect (cur_adr,cur_fil)->(moved_adr,moved_fil) saves to the
  same local file, so it must be followed rather than turned into a
  self-pointing "moved" stub (#159). Mirrors the savename: scheme+userinfo
  stripped, www kept (www dedup is the crawl layer's job), path
  slash/query-normalized per the URL-hack flags. Not hash_url_equals: that keys
  on the dedup hash, which folds www and never collapses http<->https.
*/
hts_boolean hts_redirect_same_savefile(httrackp *opt, const char *cur_adr,
                                       const char *cur_fil,
                                       const char *moved_adr,
                                       const char *moved_fil);

/*
  Did a postprocess-html callback's reply come out of the engine's own storage?
  "html" is what it returned, "buffer" and "capa" the storage. Any pointer
  inside it edited that storage in place, at offset "html - buffer": a reply
  skipping a BOM comes back at buffer+3 and is no less in place than one at
  buffer itself. The engine must move such bytes down rather than append them
  onto themselves.
*/
hts_boolean hts_postprocess_reply_inplace(const char *html, const char *buffer,
                                          size_t capa);

/*
  Can a postprocess-html callback's reply be applied? "html" and "len" are what
  it returned, "buffer", "size" and "capa" what it was handed. An in-place reply
  holds only the bytes the engine gave it, counted from its own offset. A reply
  carrying its own buffer is bounded by that buffer instead. Neither may report
  a negative count, which an int "len" allows.
*/
hts_boolean hts_postprocess_reply_ok(const char *html, int len,
                                     const char *buffer, size_t size,
                                     size_t capa);

/*
  Take the pending request NAME (HTS_ABORT_LOCKNAME or HTS_PAUSE_LOCKNAME) in
  the output directory, deleting the file, and answer whether the mirror must
  act on it. True only for a request newer than this run's own
  hts-in_progress.lock and deletable, so neither one inherited from an earlier
  run nor one the engine cannot remove ever reaches the mirror.
*/
hts_boolean hts_take_lock_request(httrackp *opt, const char *name);

/*
  Process user intercations: pause, add link, delete link..
*/
void hts_mirror_process_user_interaction(htsmoduleStruct * str,
                                         htsmoduleStructExtended * stre);

/*
  Get the next file on the queue, waiting for it, handling other files in background..
  Parameters: The public module structure, and the private module variables
  Returns 0 upon success
*/
int hts_mirror_wait_for_next_file(htsmoduleStruct * str,
                                  htsmoduleStructExtended * stre);

/*
  Wait for (adr, fil, save) to be started, that is, 
  to be ready for naming, having its header MIME type
  If the final URL is to be forbidden, sets 'forbidden_url' to the corresponding value
*/
int hts_wait_delayed(htsmoduleStruct * str, lien_adrfilsave *afs,
                     char *parent_adr, char *parent_fil, lien_adrfil *former,
                     int *forbidden_url);

/* Context state */

#define ENGINE_DEFINE_CONTEXT_BASE() \
  httrackp* const opt HTS_UNUSED = (httrackp*) str->opt; \
  struct_back* const sback HTS_UNUSED = (struct_back*) str->sback; \
  lien_back* const back HTS_UNUSED = sback->lnk; \
  const int back_max HTS_UNUSED = sback->count; \
  cache_back* const cache HTS_UNUSED = (cache_back*) str->cache; \
  hash_struct* const hashptr HTS_UNUSED = (hash_struct*) str->hashptr; \
  const int numero_passe HTS_UNUSED = str->numero_passe; \
  /* variable */ \
  int ptr = *str->ptr_

#define ENGINE_SET_CONTEXT_BASE() \
  ptr = *str->ptr_

#define ENGINE_LOAD_CONTEXT_BASE() \
  ENGINE_DEFINE_CONTEXT_BASE()

#define ENGINE_SAVE_CONTEXT_BASE() \
  /* Apply changes */ \
  * str->ptr_ = ptr

#endif
