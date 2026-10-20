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

/* httrack mimics the headers of these browsers. */
typedef enum hts_browser_family {
  HTS_BROWSER_NONE = 0,
  HTS_BROWSER_CHROME, /* Chrome and Edge */
  HTS_BROWSER_FIREFOX
} hts_browser_family;

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

/* The attributes of one start tag that decide how its links are fetched.
   Zero it per page, and again when the page bytes change in place. */
typedef struct hts_fetch_tag {
  const char *name; /* just past the '<', or NULL before the first read */
  unsigned char link_dest; /* hts_fetch_dest of a <link href> */
  hts_boolean link_cors, download, crossorigin, module;
  const char *target; /* the target value, or NULL when it has none */
  size_t target_len;
} hts_fetch_tag;

/* Fill dest, cors and user for the attribute at attr, in the start tag just
   past '<' at tag_name, or for path fil in a stylesheet when tag_name is NULL.
   tag caches the attributes of tag_name, so a tag with many links is read
   once. page_frame is what a link with no target navigates, from
   hts_fetch_same_frame.
 */
void hts_fetch_classify(hts_fetch_meta *fm, hts_fetch_tag *tag,
                        const char *tag_name, const char *attr, const char *fil,
                        int page_frame);

/* Return the destination of a navigation that stays in the frame of a page
   fetched as page_dest: document, iframe or frame, else UNKNOWN. */
int hts_fetch_same_frame(int page_dest);

/* Does the NUL-terminated page html have a <base target>, which sends its
   links with no target to a frame we cannot tell? */
hts_boolean hts_fetch_has_base_target(const char *html);

/* Return how the URL at to_adr relates to the origin at from_adr: SAME_ORIGIN,
   SAME_SITE or CROSS_SITE. Same-site compares registrable domains from the
   Public Suffix List when it loaded, else the last two host labels. */
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

/* Write the "scheme://host[:port]" origin of adr into dst. Return HTS_FALSE
   when the host holds a byte a header line cannot carry or dst is too small. */
hts_boolean hts_fetch_origin(const char *adr, char *dst, size_t size);

/* httrack sends this Accept when neither -%a nor a browser table names one. */
#define HTS_ACCEPT_LEGACY                                                      \
  "text/html,image/png,image/jpeg,image/pjpeg,image/x-xbitmap,image/"          \
  "svg+xml,image/gif;q=0.9,*/*;q=0.1"

/* httrack sends HTS_ACCEPT_LANGUAGE_LEGACY without -%l, and a browser table
   sends HTS_ACCEPT_LANGUAGE_BROWSER instead. */
#define HTS_ACCEPT_LANGUAGE_LEGACY "en, *"
#define HTS_ACCEPT_LANGUAGE_BROWSER "en-US,en;q=0.9"

/* Return the kind index of the --accept argument arg when it starts with a
   kind word and '=', else -1. */
int hts_accept_kind_of(const char *arg);

/* Return the first Accept of: dest's kind line in list, the document line for
   a frame, user_default if user_given, family's table, user_default. A list
   line is "kind=value", the last one wins, and its value is copied into buf
   (size bytes). */
const char *hts_accept_lookup(const char *list, const char *user_default,
                              hts_boolean user_given, int dest,
                              hts_browser_family family, char *buf,
                              size_t size);

#endif

#endif
