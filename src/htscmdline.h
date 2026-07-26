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

/* Split the command line "cmd" in place into an argv vector of *nargs entries,
   argv[0] being the program name: a space, tab, CR or LF separates arguments, a
   double quote protects a run of them, and inside a quoted run \" and \\ stand
   for a literal " and \, so a quote in a value can no longer end the argument
   and turn the rest into flags. Quotes are kept, the engine unquotes each
   argument itself. Returns a malloct'ed vector of pointers into cmd (freet the
   vector, never its entries), or NULL when it cannot be sized or allocated. */
char **hts_split_cmdline(char *cmd, int *nargs);

#endif
