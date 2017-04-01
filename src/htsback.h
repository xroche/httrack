/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998-2017 Xavier Roche and other contributors

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
/* File: httrack.c subroutines:                                 */
/*       backing system (multiple socket download)              */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTSBACK_DEFH
#define HTSBACK_DEFH

#include "htsglobal.h"

#if HTS_XGETHOST
#if USE_BEGINTHREAD
#include "htsthread.h"
#endif
#endif

/* Forward definitions */
#ifndef HTS_DEF_FWSTRUCT_httrackp
#define HTS_DEF_FWSTRUCT_httrackp
typedef struct httrackp httrackp;
#endif
#ifndef HTS_DEF_FWSTRUCT_struct_back
#define HTS_DEF_FWSTRUCT_struct_back
typedef struct struct_back struct_back;
#endif
#ifndef HTS_DEF_FWSTRUCT_cache_back
#define HTS_DEF_FWSTRUCT_cache_back
typedef struct cache_back cache_back;
#endif
#ifndef HTS_DEF_FWSTRUCT_lien_back
#define HTS_DEF_FWSTRUCT_lien_back
typedef struct lien_back lien_back;
#endif
#ifndef HTS_DEF_FWSTRUCT_htsblk
#define HTS_DEF_FWSTRUCT_htsblk
typedef struct htsblk htsblk;
#endif

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE

// create/destroy
struct_back *back_new(httrackp *opt, int back_max);
void back_free(struct_back ** sback);

// backing
#define BACK_ADD_TEST "(dummy)"
#define BACK_ADD_TEST2 "(dummy2)"
int back_index(httrackp * opt, struct_back * sback, const char *adr, const char *fil,
               const char *sav);
int back_available(const struct_back * sback);
LLint back_incache(const struct_back * sback);
int back_done_incache(const struct_back * sback);
HTS_INLINE int back_exist(struct_back * sback, httrackp * opt, const char *adr,
                          const char *fil, const char *sav);
int back_nsoc(const struct_back * sback);
int back_nsoc_overall(const struct_back * sback);
int back_add(struct_back * sback, httrackp * opt, cache_back * cache, const char *adr,
             const char *fil, const char *save, const char *referer_adr, const char *referer_fil,
             int test);
int back_add_if_not_exists(struct_back * sback, httrackp * opt,
                           cache_back * cache, const char *adr, const char *fil, const char *save,
                           const char *referer_adr, const char *referer_fil, int test);
int back_stack_available(struct_back * sback);
int back_search(httrackp * opt, struct_back * sback);
int back_search_quick(struct_back * sback);
void back_clean(httrackp * opt, cache_back * cache, struct_back * sback);
int back_cleanup_background(httrackp * opt, cache_back * cache,
                            struct_back * sback);
void back_wait(struct_back * sback, httrackp * opt, cache_back * cache,
               TStamp stat_timestart);
int back_letlive(httrackp * opt, cache_back * cache, struct_back * sback,
                 const int p);
int back_searchlive(httrackp * opt, struct_back * sback, const char *search_addr);
void back_connxfr(htsblk * src, htsblk * dst);
void back_move(lien_back * src, lien_back * dst);
void back_copy_static(const lien_back * src, lien_back * dst);
int back_serialize(FILE * fp, const lien_back * src);
int back_unserialize(FILE * fp, lien_back ** dst);
int back_serialize_ref(httrackp * opt, const lien_back * src);
int back_unserialize_ref(httrackp * opt, const char *adr, const char *fil,
                         lien_back ** dst);
void back_set_finished(struct_back * sback, const int p);
void back_set_locked(struct_back * sback, const int p);
void back_set_unlocked(struct_back * sback, const int p);
int back_delete(httrackp * opt, cache_back * cache, struct_back * sback,
                const int p);
void back_index_unlock(struct_back * sback, const int p);
int back_clear_entry(lien_back * back);
int back_flush_output(httrackp * opt, cache_back * cache, struct_back * sback,
                      const int p);
void back_delete_all(httrackp * opt, cache_back * cache, struct_back * sback);
int back_maydelete(httrackp * opt, cache_back * cache, struct_back * sback,
                   const int p);
void back_maydeletehttp(httrackp * opt, cache_back * cache, struct_back * sback,
                        const int p);
int back_trylive(httrackp * opt, cache_back * cache, struct_back * sback,
                 const int p);
int back_finalize(httrackp * opt, cache_back * cache, struct_back * sback,
                  const int p);
void back_info(struct_back * sback, int i, int j, FILE * fp);
void back_infostr(struct_back * sback, int i, int j, char *s);
LLint back_transferred(LLint add, struct_back * sback);

// hostback
#if HTS_XGETHOST
void back_solve(httrackp * opt, lien_back * sback);
int host_wait(httrackp * opt, lien_back * sback);
#endif
int back_checksize(httrackp * opt, lien_back * eback, int check_only_totalsize);
int back_checkmirror(httrackp * opt);

#endif

#endif
