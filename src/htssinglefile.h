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
   Internal, not installed. Unlike -%M (one MHT archive for the whole mirror)
   it rewrites the saved pages in place and keeps page-to-page links relative,
   so the mirror stays browsable and every page also stands alone.
   Limitation: an inlined stylesheet becomes a data: URL, whose path is opaque,
   so an asset inside it that stayed a link (over the cap) no longer resolves.
   Raise --single-file-max-size past it to inline it too. */
/* ------------------------------------------------------------ */

#ifndef HTS_SINGLEFILE_DEFH
#define HTS_SINGLEFILE_DEFH

#include "htsopt.h"
#include "htsstrings.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --single-file-max-size default: a bigger asset stays an ordinary link. */
#define SINGLEFILE_DEFAULT_MAX_SIZE (10 * 1024 * 1024)

/* Never base64 more than this, whatever the cap: code64() sizes in int. */
#define SINGLEFILE_HARD_MAX_SIZE (256 * 1024 * 1024)

/* Total bytes one page may inline. Nested @import fans out multiplicatively,
   so a few hundred bytes of hostile CSS can otherwise ask for gigabytes. */
#define SINGLEFILE_MAX_PAGE_SIZE (64 * 1024 * 1024)

/* Rewrite every HTML page the mirror produced. No-op unless opt->single_file;
   call once the tree is final, after the update purge. */
void singlefile_process_mirror(httrackp *opt);

/* Rewrite one HTML document held in memory, appending the result to out.
   root is the mirror directory that references may not escape; page_path is
   the document's own path under it (both UTF-8, '/' or native separators).
   page_budget caps the total inlined bytes, since nested @import fans out
   multiplicatively; the mirror pass passes SINGLEFILE_MAX_PAGE_SIZE.
   Returns HTS_TRUE if at least one reference was replaced; out may still
   differ from the input when that is HTS_FALSE, since a style or srcset value
   is re-serialized in place. */
hts_boolean singlefile_rewrite_html(httrackp *opt, const char *root,
                                    const char *page_path, const char *html,
                                    size_t html_len, LLint page_budget,
                                    String *out);

/* singlefile_rewrite_html() over the file at page_path, rewritten in place.
   Returns HTS_TRUE if the file changed. */
hts_boolean singlefile_rewrite_file(httrackp *opt, const char *root,
                                    const char *page_path);

#ifdef __cplusplus
}
#endif

#endif
