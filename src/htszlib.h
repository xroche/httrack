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
/* File: Unpacking subroutines using Jean-loup Gailly's Zlib    */
/*       for http compressed data                               */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTS_DEFZLIB
#define HTS_DEFZLIB

/* ZLib */
#include "zlib.h"

/* MiniZip */
#include "minizip/zip.h"
#include "minizip/unzip.h"
#include "minizip/mztools.h"

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE
/* Inflate filename into newfile (gzip, zlib or raw deflate); decoded size, or
   -1 on a corrupt/truncated stream, an over-budget body, or an I/O failure. A
   body provably in no deflate framing is copied verbatim. On -1, errno is the
   local failure's, and 0 when the coded body was the problem. */
extern int hts_zunpack(const char *filename, const char *newfile);
/* Inflate the head of a gzip/zlib stream, out_len max; 0 if undecodable */
extern size_t hts_zhead(const void *in, size_t in_len, void *out,
                        size_t out_len);
extern int hts_extract_meta(const char *path);
extern const char *hts_get_zerror(int err);
/* Open a ZIP for reading / writing through the UTF-8 file wrappers: the
   minizip default calls plain fopen, which mangles a non-ASCII path on Windows
   (#630). `append` takes the zipOpen2_64 APPEND_STATUS_* values. */
extern unzFile hts_unzOpen_utf8(const char *path);
extern zipFile hts_zipOpen_utf8(const char *path, int append);
/* The table both open with, for a caller that overrides one entry. */
extern void hts_zip_filefunc64(zlib_filefunc64_def *ff);
#endif

#endif
