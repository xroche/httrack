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
/* File: in-progress display rows, shared by httrack and htsserver .h */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTS_STATS_DEFH
#define HTS_STATS_DEFH

#include "htsglobal.h"

/* One row of the "in progress" display, shared so the CLI (httrack) and the
   web GUI (htsserver) cannot drift apart: each fills its own array. */

#define NStatsBuffer 14

#ifndef HTS_DEF_FWSTRUCT_t_StatsBuffer
#define HTS_DEF_FWSTRUCT_t_StatsBuffer
typedef struct t_StatsBuffer t_StatsBuffer;
#endif
struct t_StatsBuffer {
  char name[1024];
  char file[1024];
  char state[288];                         // a short label plus back->info[256]
  char BIGSTK url_sav[HTS_URLMAXSIZE * 2]; // pour cancel
  char BIGSTK url_adr[HTS_URLMAXSIZE * 2];
  char BIGSTK url_fil[HTS_URLMAXSIZE * 2];
  LLint size;
  LLint sizetot;
  int offset;
  //
  int back;
  //
  int actived; // pour disabled
};

#endif
