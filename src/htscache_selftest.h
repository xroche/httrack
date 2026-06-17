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
/* File: htscache_selftest.h                                    */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTSCACHE_SELFTEST_DEFH
#define HTSCACHE_SELFTEST_DEFH

#ifdef HTS_INTERNAL_BYTECODE

#ifndef HTS_DEF_FWSTRUCT_httrackp
#define HTS_DEF_FWSTRUCT_httrackp
typedef struct httrackp httrackp;
#endif

/* Run the cache create/read/update self-test against a working directory.
   Returns the number of failed checks (0 == success). */
int cache_selftests(httrackp *opt, const char *dir);

/* Read a committed (frozen) cache fixture under <dir>/hts-cache/new.zip and
   assert a fixed set of entries decodes field- and byte-exact. Unlike
   cache_selftests (write-then-read with the same build, a round-trip), this
   reads bytes an earlier build froze, so it catches read-path / format drift.
   regen!=0 first rewrites the fixture from the same table (to regenerate the
   committed file, never by the test). Returns the failed-check count. */
int cache_golden_selftest(httrackp *opt, const char *dir, int regen);

#endif

#endif
