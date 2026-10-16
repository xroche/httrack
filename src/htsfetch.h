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
/* File: htsfetch.h subroutines:                               */
/*       Fetch Metadata request headers (Sec-Fetch-*)           */
/* ------------------------------------------------------------ */

#ifndef HTSFETCH_DEFH
#define HTSFETCH_DEFH

#ifdef HTS_INTERNAL_BYTECODE

#include "htsglobal.h"
#include "htsopt.h"

/* A destination the parser cannot name stays UNKNOWN, because a wrong value
   can draw a 403 where none passes. */
typedef enum hts_fetch_dest {
  HTS_FETCH_DEST_UNKNOWN = 0,
  HTS_FETCH_DEST_DOCUMENT,
  HTS_FETCH_DEST_IFRAME,
  HTS_FETCH_DEST_FRAME,
  HTS_FETCH_DEST_OBJECT,
  HTS_FETCH_DEST_EMBED,
  HTS_FETCH_DEST_IMAGE,
  HTS_FETCH_DEST_SCRIPT,
  HTS_FETCH_DEST_STYLE,
  HTS_FETCH_DEST_FONT,
  HTS_FETCH_DEST_AUDIO,
  HTS_FETCH_DEST_VIDEO
} hts_fetch_dest;

/* Sec-Fetch-Site values run from best to worst, so the worst hop wins. */
typedef enum hts_fetch_site {
  HTS_FETCH_SITE_UNSET = 0, /* not computed yet, which sends nothing */
  HTS_FETCH_SITE_NONE,
  HTS_FETCH_SITE_SAME_ORIGIN,
  HTS_FETCH_SITE_SAME_SITE,
  HTS_FETCH_SITE_CROSS_SITE
} hts_fetch_site;

/* Fill dest, cors and user for the attribute at attr, in the start tag just
   past '<' at tag_name, or for path fil in a stylesheet when tag_name is NULL.
   page_frame is what a link with no target navigates, from
   hts_fetch_same_frame.
 */
void hts_fetch_classify(hts_fetch_meta *fm, const char *tag_name,
                        const char *attr, const char *fil, int page_frame);

/* Return the destination of a navigation that stays in the frame of a page
   fetched as page_dest: document, iframe or frame, else UNKNOWN. */
int hts_fetch_same_frame(int page_dest);

/* Does the NUL-terminated page html have a <base target>, which sends its
   links with no target to a frame we cannot tell? */
hts_boolean hts_fetch_has_base_target(const char *html);

/* Return how the URL at to_adr relates to the origin at from_adr: SAME_ORIGIN,
   SAME_SITE or CROSS_SITE. Same-site compares the last two host labels, so it
   never says cross-site where the Public Suffix List says same-site. */
hts_fetch_site hts_fetch_site_of(const char *from_adr, const char *to_adr);

/* Merge the hop to adr into fm->site. Clears fm->dest when fm->initiator names
   no page in opt. */
void hts_fetch_through(httrackp *opt, hts_fetch_meta *fm, const char *adr);

/* Is adr potentially trustworthy (https, or a loopback host), the only URLs a
   browser sends Sec-Fetch-* to? */
hts_boolean hts_fetch_trustworthy(const char *adr);

/* Is dest a navigation (Sec-Fetch-Mode: navigate)? */
hts_boolean hts_fetch_is_navigation(int dest);

/* These return the header value for a dest, a mode or a site. */
const char *hts_fetch_dest_name(int dest);
const char *hts_fetch_mode_name(const hts_fetch_meta *fm);
const char *hts_fetch_site_name(int site);

#endif

#endif
