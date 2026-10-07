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
/* File: htslines.h                                             */
/*       line readers for config files, shared by the engine    */
/*       and htsserver                                          */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTSLINES_DEFH
#define HTSLINES_DEFH

#include "htsglobal.h"
#include "htsstrings.h"

#include <stddef.h>
#include <stdio.h>

/* Flags for the readers below. CR is always dropped. */
/* Drop TAB and form feed. */
#define HTS_LINE_DROP_TAB 1
/* Drop NUL. */
#define HTS_LINE_DROP_NUL 2

/* Read one line from fp into s, which holds max >= 1 bytes with the NUL. The
   rest of a line too long for s is consumed, so its tail is never read back as
   the next line. Returns HTS_TRUE when a byte other than whitespace was
   dropped, and a cut line's content is unspecified. EOF is not reported:
   callers loop on feof(), and a read at EOF gives an empty line. */
hts_boolean hts_readline(FILE *fp, char *s, size_t max, int flags);

/* hts_readline() with whitespace trimmed at the end, spaces and TABs at the
   start, and a line ending with a backslash joined to the next one. Which
   lines are joined does not depend on max. Returns HTS_TRUE when any joined
   line was cut. */
hts_boolean hts_readline_cpp(FILE *fp, char *s, size_t max, int flags);

/* The limit for files httrack writes itself. A command line or a URL list is
   far smaller, and a corrupt file still cannot claim unbounded memory. */
#define HTS_READLINE_ALLOC_MAX ((size_t) 1 << 20)

/* hts_readline() into line, which grows to hold up to limit bytes. Returns
   HTS_TRUE when the line was longer, whitespace included, and then line holds
   only its start. */
hts_boolean hts_readline_alloc(FILE *fp, String *line, size_t limit, int flags);

#endif
