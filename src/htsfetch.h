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
/* File: Fetch Metadata request headers (Sec-Fetch-*)          */
/* ------------------------------------------------------------ */

#ifndef HTSFETCH_DEFH
#define HTSFETCH_DEFH

#ifdef HTS_INTERNAL_BYTECODE

#include "htsglobal.h"
#include "htsopt.h"

/* The request destination; anything the parser cannot name stays UNKNOWN,
   because a wrong value can draw a 403 where none passes. */
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

/* Sec-Fetch-Site, the cross values ordered from best to worst. */
typedef enum hts_fetch_site {
  HTS_FETCH_SITE_UNSET = 0,
  HTS_FETCH_SITE_NONE,
  HTS_FETCH_SITE_SAME_ORIGIN,
  HTS_FETCH_SITE_SAME_SITE,
  HTS_FETCH_SITE_CROSS_SITE
} hts_fetch_site;

/* Fill fm for the reference whose attribute name is at attr. tag_name points
   just past the '<' of its start tag, or is NULL inside a stylesheet body,
   where fil (the resolved path) tells a font from an image. Leaves site and
   initiator alone. */
void hts_fetch_classify(hts_fetch_meta *fm, const char *tag_name,
                        const char *attr, const char *fil);

/* The relation of the URL at to_adr to the origin at from_adr: SAME_ORIGIN,
   SAME_SITE or CROSS_SITE. Same-site compares the last two host labels, so it
   never says cross-site where the Public Suffix List says same-site. */
hts_fetch_site hts_fetch_site_of(const char *from_adr, const char *to_adr);

/* fm as sent for a hop to adr: site becomes the worst of the earlier hops and
   this one. opt names the links fm->initiator indexes. */
void hts_fetch_through(httrackp *opt, hts_fetch_meta *fm, const char *adr);

/* Is adr potentially trustworthy (https, or a loopback host), the only URLs a
   browser sends Sec-Fetch-* to? */
hts_boolean hts_fetch_trustworthy(const char *adr);

/* Is dest a navigation (Sec-Fetch-Mode: navigate)? */
hts_boolean hts_fetch_is_navigation(int dest);

/* Header values for fm, whose site went through hts_fetch_through. */
const char *hts_fetch_dest_name(int dest);
const char *hts_fetch_mode_name(const hts_fetch_meta *fm);
const char *hts_fetch_site_name(int site);

#endif

#endif
