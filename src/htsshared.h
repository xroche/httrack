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
/* File: file and URL helpers that htsserver and proxytrack     */
/*       compile in, because libhttrack does not export them    */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTSSHARED_DEFH
#define HTSSHARED_DEFH

#include "htsglobal.h"
#include "htsbasenet.h"

#include <stdio.h>
#include <time.h>

/* Not HTSEXT_API, because that breaks the MSVC proxytrack build. */

/* Read one line of fp into s (max bytes, NUL included) and return its length.
   CR, TAB and form feed are dropped, and NUL too with HTS_LINPUT_SKIP_NUL. A
   line cut at the bound has its newline consumed, so the next read is not "",
   and with HTS_LINPUT_DROP_TAIL the rest of the line is skipped. */
int hts_shared_linput(FILE *fp, char *s, int max, int flags);

/* hts_shared_linput() flags. */
#define HTS_LINPUT_SKIP_NUL 1
#define HTS_LINPUT_DROP_TAIL 2

/* Is s an existing regular file? A plain stat(), with no path conversion. */
hts_boolean hts_shared_fexist(const char *s);

/* Does the link carry an authority ("//host"), after an optional scheme? */
hts_boolean hts_shared_link_has_authority(const char *lien);

/* Wait until soc is readable or deadline passes; a deadline of 0 never
   passes. */
hts_boolean hts_shared_readable(T_SOC soc, time_t deadline);

/* Read one line off soc into s (max bytes, NUL included) and return its
   length, or -1 when no byte came before deadline (0: none). CR, TAB and form
   feed are dropped. A line longer than s is drained to its newline and sets
   *cut, so its tail is never read as a line of its own. */
int hts_shared_linputsoc(T_SOC soc, char *s, int max, time_t deadline,
                         hts_boolean *cut);

#endif
