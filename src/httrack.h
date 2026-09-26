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
/* File: htsshow.c console progress info                        */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTTRACK_DEFH
#define HTTRACK_DEFH

#include "htsglobal.h"
#include "htscore.h"
#include "htssafe.h"
#include "htsstats.h"

#ifndef HTS_DEF_FWSTRUCT_t_InpInfo
#define HTS_DEF_FWSTRUCT_t_InpInfo
typedef struct t_InpInfo t_InpInfo;
#endif
struct t_InpInfo {
  int ask_refresh;
  int refresh;
  LLint stat_bytes;
  int stat_time;
  int lien_n;
  int lien_tot;
  int stat_nsocket;
  int rate;
  int irate;
  int ft;
  LLint stat_written;
  int stat_updated;
  int stat_errors;
  int stat_warnings;
  int stat_infos;
  TStamp stat_timestart;
  int stat_back;
};

int main(int argc, char **argv);

extern int _DEBUG_HEAD;
extern FILE *ioinfo;

#endif
