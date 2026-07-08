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
/*       cache system (index and stores files in cache)         */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTSCACHE_DEFH
#define HTSCACHE_DEFH

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE

#include "htsglobal.h"

#include <stdlib.h>

/* Forward definitions */
#ifndef HTS_DEF_FWSTRUCT_httrackp
#define HTS_DEF_FWSTRUCT_httrackp
typedef struct httrackp httrackp;
#endif
#ifndef HTS_DEF_FWSTRUCT_cache_back
#define HTS_DEF_FWSTRUCT_cache_back
typedef struct cache_back cache_back;
#endif
#ifndef HTS_DEF_FWSTRUCT_htsblk
#define HTS_DEF_FWSTRUCT_htsblk
typedef struct htsblk htsblk;
#endif

// cache
void cache_mayadd(httrackp * opt, cache_back * cache, htsblk * r,
                  const char *url_adr, const char *url_fil,
                  const char *url_save);
void cache_add(httrackp * opt, cache_back * cache, const htsblk * r,
               const char *url_adr, const char *url_fil, const char *url_save,
               int all_in_cache, const char *path_prefix);
htsblk cache_read(httrackp * opt, cache_back * cache, const char *adr,
                  const char *fil, const char *save, char *location);
htsblk cache_read_ro(httrackp * opt, cache_back * cache, const char *adr,
                     const char *fil, const char *save, char *location);
/* Like cache_read, but also yields entries whose transfer broke; return_save
   (optional, HTS_URLMAXSIZE*2) receives the entry's recorded save name. */
htsblk cache_read_including_broken(httrackp *opt, cache_back *cache,
                                   const char *adr, const char *fil,
                                   char *return_save);
htsblk cache_readex(httrackp * opt, cache_back * cache, const char *adr,
                    const char *fil, const char *save, char *location,
                    char *return_save, int readonly);
htsblk *cache_header(httrackp * opt, cache_back * cache, const char *adr,
                     const char *fil, htsblk * r);
void cache_init(cache_back * cache, httrackp * opt);

/* Which hts-cache/ generation (new.* vs old.*) is authoritative. */
typedef enum {
  CACHE_RECONCILE_PROMOTE,     /* no new cache: promote the old generation */
  CACHE_RECONCILE_INTERRUPTED, /* aborted run: keep the larger generation */
  CACHE_RECONCILE_ROLLBACK     /* nothing transferred: restore the old one */
} hts_cache_reconcile_mode;

/* Reconcile the on-disk cache generations according to mode; a no-op when
   the involved files are absent. */
void hts_cache_reconcile(httrackp *opt, hts_cache_reconcile_mode mode);

int cache_writedata(FILE * cache_ndx, FILE * cache_dat, const char *str1,
                    const char *str2, char *outbuff, int len);
int cache_readdata(cache_back * cache, const char *str1, const char *str2,
                   char **inbuff, int *len);

void cache_rstr(FILE *fp, char *s, size_t s_size);
char *cache_rstr_addr(FILE * fp);
int cache_brstr(char *adr, char *s, size_t s_size);
/* binput over a NUL-terminated buffer, bounded: no read starts at/past end. */
int cache_binput(char *adr, const char *end, char *s, int max);
int cache_brint(char *adr, int *i);
void cache_rint(FILE * fp, int *i);
void cache_rLLint(FILE * fp, LLint * i);

int cache_wstr(FILE * fp, const char *s);
int cache_wint(FILE * fp, int i);
int cache_wLLint(FILE * fp, LLint i);

#endif

#endif
