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
/*       wizard system (accept/refuse links)                    */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTSWIZARD_DEFH
#define HTSWIZARD_DEFH

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE

#include "htsglobal.h"
#include "htssafe.h"
#include "htsbuf.h"

/* Forward definitions */
#ifndef HTS_DEF_FWSTRUCT_httrackp
#define HTS_DEF_FWSTRUCT_httrackp
typedef struct httrackp httrackp;
#endif
#ifndef HTS_DEF_FWSTRUCT_lien_url
#define HTS_DEF_FWSTRUCT_lien_url
typedef struct lien_url lien_url;
#endif

/* The engine's robots.txt verdict for (adr,fil): HTS_TRUE when the fetch is
   forbidden. `filters_decided`/`filters_refused` carry the filter outcome,
   which overrides a ban under -s1 (HTS_ROBOTS_SOMETIMES). */
hts_boolean hts_robots_forbids(httrackp *opt, const char *adr, const char *fil,
                               hts_boolean filters_decided,
                               hts_boolean filters_refused);

/* Most filters one wizard answer can add. Slots must stay contiguous: the
   caller stops at the first empty one. */
#define HTS_WIZARD_MAX_FILTERS 2

/* The longest suffix an answer appends after the link (answer 7). */
#define HTS_WIZARD_FILTER_SUFFIX "*[file]"

/* Builds into `f` the `slot`-th filter answer `n` adds for the link (adr,fil),
   and leaves `f` empty past the last one. Only host-scope answers emit a
   second, because their starred form misses the apex. `seeker_up` is
   opt->seeker's HTS_SEEKER_UP bit. `f` needs HTS_FILTER_SLOT_SIZE bytes. */
void hts_wizard_answer_filter(htsbuff *f, int slot, int n, const char *adr,
                              const char *fil, hts_boolean seeker_up);

/* Writes into `dst` the "<host>/<path>" the wizard prompt asks about, clipped
   to fit. The answer still applies to the whole (adr,fil). */
void hts_wizard_prompt_url(hts_buf dst, const char *adr, const char *fil);
/* Returns the answer code in the front end's reply `s`: 0 for NULL or empty,
   the number for a leading digit run (trailing text is ignored), -1 for `*`,
   and -999 for `!`, any other first byte, or a number above INT_MAX. */
int hts_wizard_parse_reply(const char *s);

/* Records the filters answer `n` leaves behind for the link (adr,fil), on top
   of the wizard's block at the low indices of opt->filters. The last match
   wins, so the command-line filters above it outrank every answer. Returns
   the count inserted, which sit at the tail of the block ending at
   opt->wizard_filters. A rule too long for the matcher is dropped and not
   counted. */
int hts_wizard_insert_filters(httrackp *opt, int n, const char *adr,
                              const char *fil, hts_boolean seeker_up);

/* Which host-scope range answer `n` falls in: HTS_TRUE excludes the scope,
   HTS_FALSE includes it, HTS_DEFAULT for any answer outside both ranges. */
hts_tristate hts_wizard_scope_answer(int n);

/* Applies the verdict half of answer `n` for the link (adr,fil): refuses it,
   stops the questions, or bans recursion from it. It never overturns a
   computed verdict. An undecided (-1) verdict becomes refused or allowed. A
   new answer also needs hts_wizard_answer_filter(). */
void hts_wizard_apply_verdict(httrackp *opt, int n, const char *adr,
                              const char *fil, int *forbidden_url,
                              int *set_prio_to);

/* A (tag, attribute) pair naming a reference kind. */
#ifndef HTS_DEF_DEFSTRUCT_htspair_t
#define HTS_DEF_DEFSTRUCT_htspair_t

typedef struct htspair_t {
  const char *tag;
  const char *attr;
} htspair_t;
#endif

/* HTS_TRUE if tag starts with the whole token cmp; NULL tag never matches. */
hts_boolean hts_cmp_tag_token(const char *tag, const char *cmp);

/* HTS_TRUE when link `ptr` was taken for an asset on a host foreign to its
   referer: its URL does not look like hypertext, and the hosts differ. */
hts_boolean hts_link_is_foreign_asset(httrackp *opt, int ptr);

/* Writes into `dst` the end-of-mirror note that links above the start
   directory were refused, and what lifts the refusal. Returns HTS_FALSE with
   `dst` empty when no such link was refused. */
hts_boolean hts_upper_links_note(httrackp *opt, hts_buf dst);

/* `tag` and `attribute` name the markup the link came from, or are NULL where
   there is none. `in_css_file` says it came from a stylesheet, where url() has
   no tag to match. */
int hts_acceptlink(httrackp *opt, int ptr, const char *adr, const char *fil,
                   const char *tag, const char *attribute,
                   hts_boolean in_css_file, int *set_prio_to_0,
                   int *just_test_it);
int hts_testlinksize(httrackp * opt, const char *adr, const char *fil, LLint size);
int hts_acceptmime(httrackp * opt, int ptr,
                   const char *adr, const char *fil, const char *mime);
#endif

#endif
