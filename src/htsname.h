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
/*       savename routine (compute output filename)             */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTSNAME_DEFH
#define HTSNAME_DEFH

#include "htsglobal.h"
#include "htsbuf.h"

#define DELAYED_EXT "delayed"
#define IS_DELAYED_EXT(a) ( ((a) != NULL) && ((a)[0] != 0) && strendwith_(a, "." DELAYED_EXT) )
HTS_STATIC int strendwith_(const char *a, const char *b) {
  int i, j;

  for(i = 0; a[i] != 0; i++) ;
  for(j = 0; b[j] != 0; j++) ;
  while(i >= 0 && j >= 0 && a[i] == b[j]) {
    i--;
    j--;
  }
  return (j == -1);
}

#define CACHE_REFNAME "hts-cache/ref"

/* url_savename() keeps output path plus name under this many bytes, so an
   output path this long leaves it no room and is refused at startup. */
#define HTS_SAVENAME_CAP                                                       \
  (HTS_URLMAXSIZE * 2 - 64) /* collision suffix, .delayed, NUL */

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE

/* Forward definitions */
#ifndef HTS_DEF_FWSTRUCT_httrackp
#define HTS_DEF_FWSTRUCT_httrackp
typedef struct httrackp httrackp;
#endif
#ifndef HTS_DEF_FWSTRUCT_lien_url
#define HTS_DEF_FWSTRUCT_lien_url
typedef struct lien_url lien_url;
#endif
#ifndef HTS_DEF_FWSTRUCT_struct_back
#define HTS_DEF_FWSTRUCT_struct_back
typedef struct struct_back struct_back;
#endif
#ifndef HTS_DEF_FWSTRUCT_cache_back
#define HTS_DEF_FWSTRUCT_cache_back
typedef struct cache_back cache_back;
#endif
#ifndef HTS_DEF_FWSTRUCT_hash_struct
#define HTS_DEF_FWSTRUCT_hash_struct
typedef struct hash_struct hash_struct;
#endif
#ifndef HTS_DEF_FWSTRUCT_lien_back
#define HTS_DEF_FWSTRUCT_lien_back
typedef struct lien_back lien_back;
#endif
#ifndef HTS_DEF_FWSTRUCT_lien_adrfil
#define HTS_DEF_FWSTRUCT_lien_adrfil
typedef struct lien_adrfil lien_adrfil;
#endif
#ifndef HTS_DEF_FWSTRUCT_lien_adrfilsave
#define HTS_DEF_FWSTRUCT_lien_adrfilsave
typedef struct lien_adrfilsave lien_adrfilsave;
#endif

// note: 'headers' can either be null, or incomplete (only r member filled)
// 'sback' may be NULL, which skips the type-probing request
int url_savename(lien_adrfilsave *const afs,
                 lien_adrfil *const former,
                 const char *referer_adr, const char *referer_fil, 
                 httrackp * opt, struct_back * sback, cache_back * cache,
                 hash_struct * hash, int ptr, int numero_passe,
                 const lien_back * headers);
void standard_name(hts_buf b, const char *dot_pos, const char *nom_pos,
                   const char *fil_complete, int short_ver);
/* Does url's query have a field with an '=' whose key is exactly name? Its
   value, clipped to fit, goes to dst, of size >= 1. */
hts_boolean url_query_value(const char *url, const char *name, hts_buf dst);
/* Append s to d (mapping '\' to '/'), clipped to d.size bytes including the
   NUL. Clips rather than aborts, because s comes from a crawled page. */
void url_savename_addstr(hts_buf d, const char *s);
/* Escape in place each path component of save that is a DOS device or engine
   directory name, ignoring case and any extension: con.txt becomes con_.txt.
   Only a trailing ' ' or '.' run ends the first component (aux.example.com).
   save.size counts the NUL. A full buffer gets co_.txt, and nothing aborts. */
void url_savename_escape_reserved(hts_buf save);
/* Append sep+s to d whole, cutting d's tail (never inside a UTF-8 character)
   for room, so the suffix always survives; nothing happens if it alone fills
   d.size. */
void url_savename_addtail(hts_buf d, const char *sep, const char *s);
/* Read the first len bytes of path into buf. Return the count, 0 on failure. */
size_t hts_read_file_head(const char *path, void *buf, size_t len);

/* Is the wire-vs-ext verdict contested and settleable by a body sniff? */
int hts_ext_sniff_wanted(httrackp *opt, const char *wiremime, const char *file);
/* Write the hex MD5 of the query after '?' into digest, clipped. A URL with no
   query gets "". */
char *url_md5(hts_buf digest, const char *fil_complete);
/* Write the cache ref name of (adr,fil) into filename. When it does not fit,
   filename is "" and the result is HTS_FALSE. */
HTS_CHECK_RESULT hts_boolean url_savename_refname(const char *adr,
                                                  const char *fil,
                                                  hts_buf filename);
char *url_savename_refname_fullpath(httrackp * opt, const char *adr,
                                    const char *fil);
/* Remove the temp-ref for (adr,fil); HTS_TRUE if it was removed. */
hts_boolean url_savename_refname_remove(httrackp *opt, const char *adr,
                                        const char *fil);
/* Note that this mirror wrote the ref at filename. Engine thread. */
void url_savename_refname_written(httrackp *opt, const char *filename);
/* Could the ref at filename exist? False only when hts-cache/ref was absent at
   mirror start and this mirror has not written that ref. */
hts_boolean url_savename_refname_may_exist(httrackp *opt, const char *filename);
/* Reset the record at mirror start. Written refs are tracked only when
   hts-cache/ref is absent. Engine thread. */
void url_savename_refnames_start(httrackp *opt);
/* Drop the record, so every ref may exist again. */
void url_savename_refnames_free(httrackp *opt);
#endif

#endif
