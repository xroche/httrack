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

/* HTS_TRUE if ext is the codec's own archive extension: a foo.gz served as
   Content-Encoding: gzip stays packed on disk. */
extern hts_boolean hts_codec_is_archive_ext(hts_codec codec, const char *ext);

/* Decode filename into newfile. Returns the decoded size, or -1 on a truncated
   or corrupt stream, an unsupported coding, a bomb over the budget, or I/O. */
extern int hts_codec_unpack(hts_codec codec, const char *filename,
                            const char *newfile);

/* Decode up to out_len leading bytes of a coded body for the MIME sniffer;
   0 when nothing could be decoded. */
extern size_t hts_codec_head(hts_codec codec, const void *in, size_t in_len,
                             void *out, size_t out_len);

/* Ceiling on the decoded size of a body of in_size coded bytes. */
extern LLint hts_codec_maxout(LLint in_size);

/* Coded size of an open stream, left rewound; -1 if unknown. */
extern LLint hts_codec_coded_size(FILE *in);

#endif

#endif
