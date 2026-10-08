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
/* File: command line splitter, shared by the engine and         */
/*       htsserver                                               */
/* Author: Xavier Roche                                          */
/* ------------------------------------------------------------ */

#ifndef HTSCMDLINE_DEFH
#define HTSCMDLINE_DEFH

#include "htsglobal.h"
#include "htsstrings.h"

/* The argument grammar: a space separates arguments, '"' toggles quoting, and
   inside quotes \\ and \" are the only escapes. */

/* hts_split_args() flags. */
/* TAB, CR and LF separate arguments too. */
#define HTS_SPLIT_FOLD_WS 1
/* Drop the quote characters, wherever they are in the argument. */
#define HTS_SPLIT_STRIP_QUOTES 2
/* Drop an empty argument, but keep a quoted "" (#106). */
#define HTS_SPLIT_DROP_EMPTY 4

/* Split "cmd" in place into a NULL-terminated vector of *nargs arguments, under
   the HTS_SPLIT_* flags. Returns a malloct'ed vector of pointers into cmd
   (freet the vector, never its entries), or NULL when it cannot be sized or
   allocated. */
char **hts_split_args(char *cmd, int *nargs, int flags);

/* hts_split_args() under HTS_SPLIT_FOLD_WS: the WebHTTrack filter list. */
char **hts_split_cmdline(char *cmd, int *nargs);

/* Append len bytes of arg to out with \\ and \" escaped, the body of a quoted
   argument without its quotes. */
void hts_escape_arg(String *out, const char *arg, size_t len);

/* Append arg to out as one argument: quoted when it holds a space, a quote or a
   backslash, "" when empty, verbatim otherwise. hts_split_args() with
   HTS_SPLIT_STRIP_QUOTES reads it back. */
void hts_quote_arg(String *out, const char *arg);

/* Does the len bytes at arg start and end with a quote? A URL or a path in
   that shape was quoted by mistake, since argv reaches the engine final. */
hts_boolean hts_is_quoted(const char *arg, size_t len);

/* Strip one surrounding quote pair from arg in place, if it starts with a
   quote. HTS_FALSE when that quote is not closed at its end. */
hts_boolean hts_unquote_arg(char *arg);

#endif
