/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998-2015 Xavier Roche and other contributors

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

Important notes:

- We hereby ask people using this source NOT to use it in purpose of grabbing
emails addresses, or collecting any other private information on persons.
This would disgrace our work, and spoil the many hours we spent on it.

Please visit our Website: http://www.httrack.com
*/

/* ------------------------------------------------------------ */
/* File: htsshow.c console progress info                        */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTSTOOLS_DEFH
#define HTSTOOLS_DEFH

#include "htsglobal.h"
#include "htscore.h"
#include "htssafe.h"

#ifndef HTS_DEF_FWSTRUCT_t_StatsBuffer
#define HTS_DEF_FWSTRUCT_t_StatsBuffer
typedef struct t_StatsBuffer t_StatsBuffer;
#endif
struct t_StatsBuffer {
  char name[1024];
  char file[1024];
  char state[256];
  char BIGSTK url_sav[HTS_URLMAXSIZE * 2];      // pour cancel
  char BIGSTK url_adr[HTS_URLMAXSIZE * 2];
  char BIGSTK url_fil[HTS_URLMAXSIZE * 2];
  LLint size;
  LLint sizetot;
  int offset;
  //
  int back;
  //
  int actived;                  // pour disabled
};

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
#endif

extern HTSEXT_API hts_stat_struct HTS_STAT;
extern int _DEBUG_HEAD;
extern FILE *ioinfo;
