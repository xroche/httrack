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
/* File: htsparse_selftest.h                                    */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTSPARSE_SELFTEST_DEFH
#define HTSPARSE_SELFTEST_DEFH

#include "htsglobal.h"

#ifdef HTS_INTERNAL_BYTECODE

#ifndef HTS_DEF_FWSTRUCT_httrackp
#define HTS_DEF_FWSTRUCT_httrackp
typedef struct httrackp httrackp;
#endif

/* Sweep hts_dirty_link_is_url over every string of up to four bytes across the
   alphabet it branches on, plus token combinations reaching the scheme and
   extension branches, against a model of its rules. "dump" prints the whole
   verdict table instead of judging it. Returns 0 when all agree. */
int parse_selftest_dirtylink(httrackp *opt, hts_boolean dump);

/* Sweep hts_js_scan_link over the shapes a script statement takes, one
   representative per class it branches on, against a model of its rules, and
   pin a table of named statements. "dump" prints the verdicts instead.
   Returns 0 when all agree. */
int parse_selftest_jsscan(httrackp *opt, hts_boolean dump);

/* Check hts_dirty_attr_name and hts_dirty_attr_detectable twice: against tags
   built from a known attribute, and against malformed ones where only what the
   walk may claim can be checked. Returns 0 when both hold. */
int parse_selftest_tagattr(httrackp *opt);

#endif

#endif
