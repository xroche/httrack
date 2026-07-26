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
/* HTTrack change report (--changes). Internal, not installed.
   Accumulates what the crawl did to each mirrored file, compares the bytes
   against the copy the previous run left behind, and writes
   hts-changes.json next to the log. */
/* ------------------------------------------------------------ */

#ifndef HTS_CHANGES_DEFH
#define HTS_CHANGES_DEFH

#include "htsopt.h"
#include "htsstrings.h"

#ifndef HTS_DEF_FWSTRUCT_cache_back
#define HTS_DEF_FWSTRUCT_cache_back
typedef struct cache_back cache_back;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Report file name, written under the project's log directory. */
#define HTS_CHANGES_FILE "hts-changes.json"

/* Schema version carried by the report; bump on an incompatible change. */
#define HTS_CHANGES_SCHEMA 1

/* Which side of the comparison a mirrored file ended up on. */
typedef enum {
  HTS_CHANGE_NEW = 0,   /* no local copy before this run */
  HTS_CHANGE_CHANGED,   /* rewritten, and the bytes differ */
  HTS_CHANGE_UNCHANGED, /* the bytes are those of the previous mirror */
  HTS_CHANGE_GONE,      /* in the previous mirror, absent from this one */
  HTS_CHANGE_BUCKETS
} hts_change_bucket;

/* Record what this crawl is doing to the local file `save` (an absolute path;
   adr/fil form the URL it comes from, either may be empty for an engine
   generated file). `rewritten` means the copy on disk is being written over,
   `not_updated` is the 200-versus-304 transfer signal, used only when no
   digest can be taken. Only the first call for a given file samples the
   previous copy, so a retried or twice-notified resource is counted once.
   No-op unless --changes is on. */
void hts_changes_notify(httrackp *opt, const char *adr, const char *fil,
                        const char *save, hts_boolean rewritten,
                        hts_boolean not_updated);

/* Refine the entry for a parsed HTML file, whose local copy is the payload
   after link rewriting plus a footer carrying the crawl date: its bytes differ
   on every run, so the comparison uses the payload `r` holds against the body
   the previous run left in the cache. Called once the page is ready to be
   written, from hts_finish_html_file(). */
void hts_changes_html(httrackp *opt, cache_back *cache, const htsblk *r,
                      const char *adr, const char *fil, const char *save);

/* Record `file` (mirror-relative, as listed in new.lst) as present in the
   previous mirror and absent from this one. Whether it is also deleted from
   disk is the purge option's business, not this one's. */
void hts_changes_gone(httrackp *opt, const char *file);

/* Record that the previous mirror's index listed `file`. That index, not the
   file's presence on disk, decides what counts as already mirrored: a partial
   left by this crawl's own failed attempt is on disk but was never part of the
   previous mirror. Calling it at all marks the run as having one. */
void hts_changes_previous(httrackp *opt, const char *file);

/* Resolve every entry against the bytes now on disk and serialize the report
   into `out` (replaced). Exposed for the `changes` self-test; the engine goes
   through hts_changes_close_opt(). */
void hts_changes_report(httrackp *opt, String *out);

/* Write the report and log a one-line summary, then free the accumulator.
   Null-safe and idempotent. */
void hts_changes_close_opt(httrackp *opt);

/* Drop the accumulator without writing anything, for a run that never reached
   its end. Null-safe and idempotent. */
void hts_changes_free_opt(httrackp *opt);

/* Bucket for one entry, from what was observed. No filesystem access:
   `have_digests` says both sides could be hashed, `digests_equal` compares
   them, and `not_updated` is the fallback signal when they could not. */
hts_change_bucket hts_changes_classify(hts_boolean rewritten,
                                       hts_boolean existed,
                                       hts_boolean not_updated,
                                       hts_boolean have_digests,
                                       hts_boolean digests_equal);

/* Append `s` to `out` as a quoted JSON string. Byte sequences that are not
   valid UTF-8 become U+FFFD, so a mirror carrying legacy-charset URLs still
   produces parseable JSON. */
void hts_changes_json_string(String *out, const char *s);

#ifdef __cplusplus
}
#endif

#endif
