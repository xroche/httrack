/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 2026 Xavier Roche and other contributors

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
/* --single-file: post-mirror pass embedding each page's assets as data: URIs.
   Unlike -%M, it rewrites pages in place and keeps page links relative. An
   asset that stayed a link (over the cap) no longer resolves inside an inlined
   stylesheet, because a data: URL's path is opaque. Raise
   --single-file-max-size to inline it. */
/* ------------------------------------------------------------ */

#ifndef HTS_SINGLEFILE_DEFH
#define HTS_SINGLEFILE_DEFH

#include "htscore.h" /* HTS_HTMLESCAPE_*_MAXEXP */
#include "htsbuf.h"
#include "htsopt.h"
#include "htsstrings.h"
#include "htsurlport.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --single-file-max-size default: a bigger asset stays an ordinary link. */
#define SINGLEFILE_DEFAULT_MAX_SIZE (10 * 1024 * 1024)

/* Never base64 more than this, whatever the cap, to bound an asset's memory. */
#define SINGLEFILE_HARD_MAX_SIZE (256 * 1024 * 1024)

/* Total bytes one page may inline. Nested @import fans out multiplicatively,
   so a few hundred bytes of hostile CSS can otherwise ask for gigabytes. */
#define SINGLEFILE_MAX_PAGE_SIZE (64 * 1024 * 1024)

/* The mark htsparse appends to a saved reference the pass may inline:

     #!<16-hex secret>.<class>.<len>

   It is a fragment, so a half-marked mirror still browses. The secret is 64
   CSPRNG bits per run, never written to disk, so a site cannot forge a mark.
   <len> is the byte length of the reference before the mark, and <class> the
   context htsparse saw. */
#define SINGLEFILE_MARK_INTRO "#!"
#define SINGLEFILE_SECRET_HEX 16

/* Bytes before the ".<class>.<len>" tail. */
#define SINGLEFILE_INTRO_LEN                                                   \
  (sizeof(SINGLEFILE_MARK_INTRO) - 1 + SINGLEFILE_SECRET_HEX)

/* <class>: what the referencing context expects. */
#define SINGLEFILE_CLASS_ANY '-'
#define SINGLEFILE_CLASS_CSS 'c'
#define SINGLEFILE_CLASS_JS 'j'

/* Enough for the intro, the secret, both separators and a 20-digit length. */
#define SINGLEFILE_MARK_MAX 64

/* The longest <len> a mark may carry is HTS_URLMAXSIZE*2 of savename through
   escape_for_html_print_full, plus as much query through
   escape_for_html_print. The emit site asserts this against its buffers. */
#define SINGLEFILE_MAX_SPAN                                                    \
  (HTS_URLMAXSIZE * 2 * (HTS_HTMLESCAPE_FULL_MAXEXP + HTS_HTMLESCAPE_MAXEXP))

/* Release this run's secret. */
void singlefile_free(httrackp *opt);

/* "#!<secret>" for this run, or NULL if no CSPRNG was available (in which case
   nothing may be marked). Draws the secret on first use. */
HTS_CHECK_RESULT const char *singlefile_intro(httrackp *opt);

/* HTS_FALSE if [body,len) already holds this run's intro, so the document
   must not be marked. Only a 2^-64 coincidence trips this. */
HTS_CHECK_RESULT hts_boolean singlefile_may_mark(httrackp *opt, hts_span body);

/* Write the mark for a reference of reflen bytes into buf; returns buf, left
   empty for a reflen the pass could not read back. */
const char *singlefile_mark(httrackp *opt, hts_buf buf, char cls,
                            size_t reflen);

/* The class a reference in this context may inline as, or 0 to leave it alone.
   tag_name points just past the '<' of the enclosing start tag, or NULL when
   there is none (inside a stylesheet or a script); attr at the attribute name,
   and body at the document's first byte, which bounds every read.
   Everything htsparse detects is inlinable unless it names a page. */
char singlefile_ref_class(const char *tag_name, const char *attr,
                          const char *body);

/* Rewrite every HTML page the mirror produced, then strip the marks left in
   the assets. No-op unless opt->single_file; call once the tree is final,
   after the update purge. */
void singlefile_process_mirror(httrackp *opt);

/* Expand the marks in the document held in memory, appending the result to
   out. root is the mirror directory that references may not escape; page_path
   is the document's own path under it (both UTF-8, '/' or native separators).
   page_budget caps the total inlined bytes, since a nested @import fans out
   multiplicatively; the mirror pass passes SINGLEFILE_MAX_PAGE_SIZE.
   Returns HTS_TRUE if at least one reference was replaced. */
hts_boolean singlefile_rewrite_html(httrackp *opt, const char *root,
                                    const char *page_path, hts_span html,
                                    LLint page_budget, String *out);

/* singlefile_rewrite_html() over the file at page_path, rewritten in place.
   Returns HTS_TRUE if the file changed. */
HTS_CHECK_RESULT hts_boolean singlefile_rewrite_file(httrackp *opt,
                                                     const char *root,
                                                     const char *page_path);

#ifdef __cplusplus
}
#endif

#endif
