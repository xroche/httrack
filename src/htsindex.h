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
/* File: htsindex.h                                             */
/*       keyword indexing system (search index)                 */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTSKINDEX_DEFH
#define HTSKINDEX_DEFH

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE

#include "htsglobal.h"
#include "htsopt.h"
#include "htsurlport.h"

/* Add the words of the page HTML, of type MIME, saved as FILENAME. 1 when
   indexed, 0 when skipped. */
HTS_CHECK_RESULT int index_keyword(hts_span html, const char *mime,
                                   const char *filename, const char *indexpath);
void index_init(const char *indexpath);
void index_finish(const char *indexpath, hts_kindex mode);
#endif

#endif
