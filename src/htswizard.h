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

/* Builds into `f` the `slot`-th filter answer `n` adds for the link (adr,fil),
   and leaves `f` empty past the last one. Only the host-scope answers emit a
   second, because their starred form misses the apex. `seeker_up` is the
   HTS_SEEKER_UP bit of opt->seeker, read by answer 5. */
void hts_wizard_answer_filter(htsbuff *f, int slot, int n, const char *adr,
                              const char *fil, hts_boolean seeker_up);

/* Writes into `dst` (capacity `dstsize`, NUL included) the "<host>/<path>" the
   wizard prompt asks about. Clips rather than aborting, so the prompt can lose
   its tail on a link longer than `dstsize`; the answer is still recorded
   against the whole (adr,fil). */
void hts_wizard_prompt_url(char *dst, size_t dstsize, const char *adr,
                           const char *fil);
/* Records the filters answer `n` leaves behind for the link (adr,fil), on top
   of the wizard's block at the low indices of opt->filters: last match wins, so
   a later answer outranks an earlier one and the command-line filters above the
   block outrank every answer. Returns the number inserted, which sit at the
   tail of the block, ending at opt->wizard_filters; a rule too long for the
   matcher is dropped, so that can fall short of what the answer emits. */
int hts_wizard_insert_filters(httrackp *opt, int n, const char *adr,
                              const char *fil, hts_boolean seeker_up);

/* Which host-scope range answer `n` falls in: HTS_TRUE excludes the scope,
   HTS_FALSE includes it, HTS_DEFAULT for any answer outside both ranges. */
hts_tristate hts_wizard_scope_answer(int n);

/* Applies the verdict half of answer `n` for the link (adr,fil): refuses it,
   stops the questions, or bans recursion from it. It never overturns a verdict
   already computed, but resolves an undecided (-1) link: refused, or
   authorized. The filter half is hts_wizard_answer_filter(), and a new answer
   needs both. */
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

int hts_acceptlink(httrackp * opt, int ptr,
                   const char *adr, const char *fil,
                   const char *tag, const char *attribute,
                   int *set_prio_to_0, int *just_test_it);
int hts_testlinksize(httrackp * opt, const char *adr, const char *fil, LLint size);
int hts_acceptmime(httrackp * opt, int ptr,
                   const char *adr, const char *fil, const char *mime);
#endif

#endif
