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
/*       main routine (first called)                            */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTSMAINHSR_DEFH
#define HTSMAINHSR_DEFH

// --assume standard
#define HTS_ASSUME_STANDARD \
  "php2 php3 php4 php cgi asp jsp pl cfm nsf=text/html"

#include "htsglobal.h"
#include "htsopt.h"

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE

int cmdl_opt(char *s);
int check_path(String * s, char *defaultname);

/* Absolute path of the running executable, or NULL where the OS will not say
   and argv[0] is the only clue left. Fills dst (dstsize bytes). */
const char *hts_self_path(char *dst, size_t dstsize);

/* Write the data directory holding the HTML templates into dst (dstsize bytes,
   NUL-terminated, trailing '/' included): builtin when it is there, else a
   layout under selfpath's directory. selfpath may be NULL, builtin empty. */
void hts_resolve_datadir(char *dst, size_t dstsize, const char *selfpath,
                         const char *builtin);

#endif

#endif
