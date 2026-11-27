/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 2026 Xavier Roche and other contributors

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
/* File: HTTP content codings (gzip/deflate, brotli, zstd)      */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTS_DEFCODEC
#define HTS_DEFCODEC

#include "htsglobal.h"
#include "htsurlport.h"
#include "htsbuf.h"

/* Content codings we may meet in a Content-Encoding header. */
typedef enum {
  HTS_CODEC_IDENTITY = 0, /**< no coding, or a token we treat as none */
  HTS_CODEC_DEFLATE,      /**< gzip, x-gzip, deflate, x-deflate */
  HTS_CODEC_BROTLI,       /**< br */
  HTS_CODEC_ZSTD,         /**< zstd */
  HTS_CODEC_UNSUPPORTED   /**< a real coding we can not decode */
} hts_codec;

#ifdef HTS_INTERNAL_BYTECODE

/* Codec for a Content-Encoding token. A known coding we can not decode is
   UNSUPPORTED; an unrecognized token is IDENTITY (servers do emit junk). */
extern hts_codec hts_codec_parse(const char *encoding);

/* Accept-Encoding value to advertise; secure (TLS) also offers br and zstd. */
extern const char *hts_acceptencoding(hts_boolean compressible,
                                      hts_boolean secure);

/* Is ext the codec's archive extension, which stays packed on disk? */
extern HTS_CHECK_RESULT hts_boolean hts_codec_is_archive_ext(hts_codec codec,
                                                             hts_span ext);

/* Decode filename into newfile and return its size, or -1 on failure. On -1,
   errno is the local failure's, or 0 when the coded body was at fault. */
extern HTS_CHECK_RESULT int
hts_codec_unpack(hts_codec codec, const char *filename, const char *newfile);

/* Run hts_codec_unpack() with the output also capped at cap when > 0. */
extern HTS_CHECK_RESULT int hts_codec_unpack_max(hts_codec codec,
                                                 const char *filename,
                                                 const char *newfile,
                                                 LLint cap);
/* Returned when the caller's cap stopped the body. A bomb gives -1. */
#define HTS_CODEC_TOOBIG (-2)

/* Return hts_codec_maxout(in_size), lowered to cap if cap > 0. */
extern LLint hts_codec_budget(LLint in_size, LLint cap);

/* Decoded bodies cap at user_cap (-m) when > 0, else 256 MiB. */
extern LLint hts_codec_decode_cap(LLint user_cap);

/* Does cap, rather than the built-in limit, set the budget for in_size? */
extern hts_boolean hts_codec_cap_binds(LLint in_size, LLint cap);

/* Decode the leading bytes of a coded body into OUT for the MIME sniffer;
   0 when nothing could be decoded. */
extern size_t hts_codec_head(hts_codec codec, hts_span in, hts_buf out);

/* Return the decoded-size ceiling for in_size coded bytes. */
extern LLint hts_codec_maxout(LLint in_size);

/* Return the size of in and rewind it, or -1 if unknown. */
extern HTS_CHECK_RESULT LLint hts_codec_coded_size(FILE *in);

#endif

#endif
