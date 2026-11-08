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

#include <string.h>
#include "htsglobal.h"
#include "htsbase.h"
#include "htsbasenet.h"
#include "htsopt.h"
#include "htsthread.h"
#include "httrack-library.h"

/* One row of the "in progress" display, shared so the CLI (httrack) and the
   web GUI (htsserver) cannot drift apart: both fill it with
   stats_collect_rows(). */

#define NStatsBuffer 14

#ifndef HTS_DEF_FWSTRUCT_t_StatsBuffer
#define HTS_DEF_FWSTRUCT_t_StatsBuffer
typedef struct t_StatsBuffer t_StatsBuffer;
#endif
struct t_StatsBuffer {
  char name[1024];
  char file[1024];
  char state[288];                         // a short label plus back->info[256]
  char BIGSTK url_sav[HTS_URLMAXSIZE * 2]; // for the cancel button
  LLint size;
  LLint sizetot;
  int back;
};

/* Returns where adr resumes past its login, which the display never shows,
   and sets *keep to the length of the scheme shown before it. */
HTS_STATIC const char *stats_skip_login(const char *adr, size_t *keep) {
  const char *const host = jump_identification_const(adr);
  const char *const sep = strstr(adr, "://");

  if (host == adr || host[-1] != '@') {
    *keep = 0;
    return adr;
  }
  *keep = sep != NULL && sep < host ? (size_t) (sep + 3 - adr) : 0;
  return host;
}

/* Fills rows[0..nrows) from the back_max slots, starting at back_index:
   transfers first, then requests, then finished slots. Clears the rest of
   rows[0..nrows), shortens a URL past maxurl bytes (16 to 1023), and
   returns the number of rows filled. Locking and printing stay with the
   caller. */
HTS_STATIC int stats_collect_rows(httrackp *opt, const lien_back *back,
                                  int back_max, int back_index,
                                  t_StatsBuffer *rows, int nrows, int maxurl) {
  int index = 0;
  int i, j, k;

  for (i = 0; i < nrows; i++) {
    rows[i].state[0] = '\0';
    rows[i].name[0] = '\0';
    rows[i].file[0] = '\0';
    rows[i].url_sav[0] = '\0';
    rows[i].back = 0;
    rows[i].size = 0;
    rows[i].sizetot = 0;
  }
  if (back == NULL || back_max <= 0 || back_index < 0)
    return 0;
  /* k = 0 is the current slot, k = 1 all of them */
  for (k = 0; k < 2; k++) {
    /* j is the priority pass: transfers, requests, finished */
    for (j = 0; j < 3 && index < nrows; j++) {
      int n;

      for (n = k; n < max(back_max * k, 1) && index < nrows; n++) {
        const int slot = (back_index + n) % back_max;
        const lien_back *const b = &back[slot];
        const int slot_status = hts_load_acquire_int(&b->status);
        t_StatsBuffer *const row = &rows[index];
        hts_boolean ok = HTS_TRUE;

        if (slot_status < 0)
          continue;
        if (j == 0 && slot_status > 0 && slot_status < 99) {
          strcpybuff(row->state, "receive");
        } else if (j == 1 && slot_status == STATUS_WAIT_HEADERS) {
          strcpybuff(row->state, "request");
        } else if (j == 1 && slot_status == STATUS_CONNECTING) {
          strcpybuff(row->state, "connect");
        } else if (j == 1 && slot_status == STATUS_WAIT_DNS) {
          strcpybuff(row->state, "search");
        } else if (j == 1 && slot_status == STATUS_FTP_TRANSFER) {
          snprintf(row->state, sizeof(row->state), "ftp: %s", b->info);
        } else if (j == 2 && slot_status == STATUS_READY) {
          if (b->r.statuscode == HTTP_OK) {
            strcpybuff(row->state, "ready");
          } else if (b->r.statuscode >= 100 && b->r.statuscode <= 599) {
            char tempo[256];

            tempo[0] = '\0';
            infostatuscode(tempo, b->r.statuscode);
            strcpybuff(row->state, tempo);
          } else {
            strcpybuff(row->state, "error");
          }
        } else {
          ok = HTS_FALSE;
        }

        if (ok) {
          char BIGSTK s[HTS_URLMAXSIZE * 2];
          char *a;
          int l;

          row->back = slot;
          s[0] = '\0';
          strcpybuff(row->url_sav, b->url_sav);
          if (strcmp(b->url_adr, "file://")) {
            size_t keep;
            const char *const host = stats_skip_login(b->url_adr, &keep);

            strncatbuff(s, b->url_adr, keep);
            strcatbuff(s, host);
          } else
            strcatbuff(s, "localhost");
          /* url_adr and url_fil are each as wide as s: clip, never abort */
          if (b->url_fil[0] != '/')
            strlncatbuff(s, "/", sizeof(s), sizeof(s) - 1 - strlen(s));
          strlncatbuff(s, b->url_fil, sizeof(s), sizeof(s) - 1 - strlen(s));
          hts_hide_secrets(opt, s, sizeof(s));

          a = strrchr(s, '/');
          if (a) {
            strncatbuff(row->file, a, 200);
            *a = '\0';
          }

          if ((l = (int) strlen(s)) < maxurl)
            strcpybuff(row->name, s);
          else {
            strncatbuff(row->name, s, maxurl / 2 - 2);
            strcatbuff(row->name, "...");
            strcatbuff(row->name, s + l - maxurl / 2 + 2);
          }

          /* An FTP worker may be writing the sizes right now. */
          {
            const LLint size = hts_load_relaxed_llint(&b->r.size);
            const LLint totalsize = hts_load_relaxed_llint(&b->r.totalsize);

            if (totalsize >= 0) { // known size, 0 included
              row->sizetot = totalsize;
              row->size = size;
            } else if (slot_status == STATUS_READY) {
              row->sizetot = size;
              row->size = size;
            } else {
              row->sizetot = 8192;
              row->size = size % 8192;
            }
          }
          index++;
        }
      }
    }
  }
  return index;
}

/* Console progress snapshot. */
typedef struct t_InpInfo {
  LLint stat_bytes;
  int stat_time;
  int lien_n;
  int lien_tot;
  int stat_nsocket;
  int rate;
  int irate;
  LLint stat_written;
  int stat_updated;
  int stat_errors;
  int stat_warnings;
  int stat_infos;
  int stat_back;
} t_InpInfo;

#endif
