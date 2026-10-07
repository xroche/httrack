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

/* Flags for the readers below, which always drop CR. */
#define HTS_LINE_DROP_TAB 1 /* TAB and form feed */
#define HTS_LINE_DROP_NUL 2

/* Read a line into s, max bytes with the NUL, and consume its rest. HTS_TRUE
   when a non-space byte was dropped. At EOF it reads an empty line. */
hts_boolean hts_readline(FILE *fp, char *s, size_t max, int flags);

/* hts_readline() trimmed and joined after a trailing backslash. Leading spaces
   and backslashes count against max, so "abc\\\n\n" at 4 is cut. */
hts_boolean hts_readline_cpp(FILE *fp, char *s, size_t max, int flags);

/* The limit for files httrack writes itself. */
#define HTS_READLINE_ALLOC_MAX ((size_t) 1 << 20)

/* hts_readline() into a line growing to limit bytes. HTS_TRUE when the line
   was longer, whitespace included, and then line holds its start. */
hts_boolean hts_readline_alloc(FILE *fp, String *line, size_t limit, int flags);

#endif
