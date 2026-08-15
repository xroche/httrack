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
/*       robots.txt (website robot file)                        */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTSROBOTS_DEFH
#define HTSROBOTS_DEFH

// robots wizard
#ifndef HTS_DEF_FWSTRUCT_robots_wizard
#define HTS_DEF_FWSTRUCT_robots_wizard
typedef struct robots_wizard robots_wizard;
#endif

#ifndef HTS_DEF_FWSTRUCT_httrackp
#define HTS_DEF_FWSTRUCT_httrackp
typedef struct httrackp httrackp;
#endif

/* Rule blob ceiling. RFC 9309 §2.5 asks crawlers to honour at least the first
   500 KiB of a robots.txt, and the blob only ever holds part of that file. */
#define HTS_ROBOTS_MAX_TOKEN_SIZE (500 * 1024)

/* Longest robots.txt line read; a stored rule is marker + pattern + LF. */
#define HTS_ROBOTS_LINE_SIZE 1024

struct robots_wizard {
  char *adr;   /* host; NULL on the list head, which holds no rules */
  char *token; /* per-host blob, one rule per line: 'A'/'D' then the pattern */
  struct robots_wizard *next;
};

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE
/* -1 if `fil` disallowed for `adr` (RFC 9309); empty: -1 if rules exist. */
int checkrobots(const robots_wizard *robots, const char *adr, const char *fil);
/* Free every node's strings, plus every node below `robots` but not `robots`.
 */
void checkrobots_free(robots_wizard * robots);
/* Store `data` as the rule blob of `adr`, adding the host if new. -1 on
   success, 0 if the allocation failed. */
int checkrobots_set(robots_wizard * robots, const char *adr, const char *data);
/* Parse robots.txt `body` for `adr`, storing the HTTrack group's rules; `info`
   gets a summary of the disallows actually kept, `keep_root_disallow` FALSE
   drops "Disallow: /", and `sitemaps` (optional) collects the Sitemap: URLs,
   one per line. Anything the parser has to leave out is logged through `opt`,
   which may be NULL. */
void robots_parse(httrackp *opt, robots_wizard *robots, const char *adr,
                  const char *body, size_t bodysize, char *info,
                  size_t infosize, hts_boolean keep_root_disallow,
                  char *sitemaps, size_t sitemapsize);
#endif

#endif
