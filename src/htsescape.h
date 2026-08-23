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
/* File: percent-escape decoding, shared by the engine and       */
/*       htsserver                                              */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTSESCAPE_DEFH
#define HTSESCAPE_DEFH

#include "htsencoding.h"
#include "htsstrings.h"

/* Decode form-urlencoded 's' into 'tempo': "%%" is '%', '+' is space, and a
   bad escape stays literal. */
extern void hts_unescapehttp(const char *s, String *tempo);

/* As hts_unescapehttp(), minus the '+' rule, and collapsing a decoded run of
   line separators so an escaped CRLF cannot forge an .ini line break. */
extern void hts_unescapeini(const char *s, String *tempo);

#endif
