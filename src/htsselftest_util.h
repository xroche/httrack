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
/* File: htsselftest_util.h                                     */
/*       fixtures several self-test files share                 */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTSSELFTEST_UTIL_DEFH
#define HTSSELFTEST_UTIL_DEFH

#include "htsglobal.h"
#include "htscore.h"

#ifdef HTS_INTERNAL_BYTECODE

size_t st_decode_body(const char *arg, char *buf, size_t size);

// é and 中 in UTF-8: the charset axis of the long-path tests (#630).
#define ST_NONASCII "\xC3\xA9\xE4\xB8\xAD"

// Headroom st_mkdeep keeps for one more segment plus the caller's leaf name,
// the longest of which is direnum's "/\xE4\xB8\xAD-deux.bin".
#define ST_LEAF_ROOM 64

/* The wiring hts_mirror() gives the naming path, and the teardown it owes. */
void st_mirror_wiring(httrackp *opt, struct_back **sback, hash_struct *hash,
                      hts_boolean backing);
void st_mirror_wiring_free(httrackp *opt, cache_back *cache,
                           struct_back **sback, hash_struct *hash);

/* Everything a cache_back holds, freed; safe to call twice. */
void st_cache_close(httrackp *opt, cache_back *cache);

/* mkdir `path`, tolerating one that exists; HTS_FALSE after complaining. */
hts_boolean st_mkdir_at(const char *path, size_t n, const char *who);

/* UTF-16 units s[0..n) costs, which is what Windows measures MAX_PATH in. */
size_t st_utf16_units(const char *s, size_t n);

/* Build a tree under `dir` whose deepest path clears MAX_PATH; 0 on failure. */
size_t st_mkdeep(char *buf, size_t bufsize, const char *dir, const char *nseg,
                 const char *who, size_t *baselen);

#endif

#endif
