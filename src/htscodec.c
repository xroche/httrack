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

/* Internal engine bytecode */
#define HTS_INTERNAL_BYTECODE

#include "htsbase.h"
#include "htscore.h"
#include "htscodec.h"
#include "htszlib.h"

#if HTS_USEBROTLI
#include <brotli/decode.h>
#endif

#if HTS_USEZSTD
#include <zstd.h>
/* 8 MB, the window an HTTP zstd decoder must honor (RFC 9659); libzstd
   defaults to 128 MB. */
#define HTS_ZSTD_WINDOWLOG_MAX 23
#endif

/* Decoded-size budget, bounding a bomb: brotli and zstd reach a million to one.
   Deflate cannot pass 1032x, so it only meets this at the 2 GiB ceiling. */
#define HTS_CODEC_MAX_RATIO 4096
#define HTS_CODEC_MIN_MAXOUT (1024 * 1024)

LLint hts_codec_maxout(LLint in_size) {
  LLint maxout;

  if (in_size <= 0 || in_size > INT_MAX / HTS_CODEC_MAX_RATIO)
    maxout = INT_MAX;
  else
    maxout = in_size * HTS_CODEC_MAX_RATIO;
  if (maxout < HTS_CODEC_MIN_MAXOUT)
    maxout = HTS_CODEC_MIN_MAXOUT;
  return maxout;
}

hts_codec hts_codec_parse(const char *encoding) {
  if (encoding == NULL || encoding[0] == '\0' ||
      strfield2(encoding, "identity"))
    return HTS_CODEC_IDENTITY;
  if (strfield2(encoding, "gzip") || strfield2(encoding, "x-gzip") ||
      strfield2(encoding, "deflate") || strfield2(encoding, "x-deflate"))
    return HTS_USEZLIB ? HTS_CODEC_DEFLATE : HTS_CODEC_UNSUPPORTED;
  if (strfield2(encoding, "br"))
    return HTS_USEBROTLI ? HTS_CODEC_BROTLI : HTS_CODEC_UNSUPPORTED;
  if (strfield2(encoding, "zstd"))
    return HTS_USEZSTD ? HTS_CODEC_ZSTD : HTS_CODEC_UNSUPPORTED;
  if (strfield2(encoding, "compress") || strfield2(encoding, "x-compress"))
    return HTS_CODEC_UNSUPPORTED; /* LZW: never advertised, no decoder here */
  /* Not a coding at all: broken servers put charsets and the like here, and
     the plain body they sent must survive it. */
  return HTS_CODEC_IDENTITY;
}

#if HTS_USEBROTLI
#define HTS_AE_BROTLI ", br"
#else
#define HTS_AE_BROTLI ""
#endif
#if HTS_USEZSTD
#define HTS_AE_ZSTD ", zstd"
#else
#define HTS_AE_ZSTD ""
#endif

const char *hts_acceptencoding(hts_boolean compressible, hts_boolean secure) {
  if (!compressible)
    return "identity";
#if HTS_USEZLIB
  /* br and zstd over TLS only, as browsers do: a cleartext intermediary that
     rewrites a coding it can not read would corrupt the mirror. */
  if (secure)
    return "gzip, deflate" HTS_AE_BROTLI HTS_AE_ZSTD ", identity;q=0.9";
  return "gzip, deflate, identity;q=0.9";
#else
  (void) secure;
  return "identity";
#endif
}

hts_boolean hts_codec_is_archive_ext(hts_codec codec, const char *ext) {
  if (ext == NULL || ext[0] == '\0')
    return HTS_FALSE;
  switch (codec) {
  case HTS_CODEC_DEFLATE:
    return strfield2(ext, "gz") || strfield2(ext, "tgz") ? HTS_TRUE : HTS_FALSE;
  case HTS_CODEC_BROTLI:
    return strfield2(ext, "br") ? HTS_TRUE : HTS_FALSE;
  case HTS_CODEC_ZSTD:
    return strfield2(ext, "zst") || strfield2(ext, "tzst") ? HTS_TRUE
                                                           : HTS_FALSE;
  default:
    return HTS_FALSE;
  }
}

#if HTS_USEBROTLI
static int codec_unpack_brotli(FILE *in, FILE *out, LLint maxout) {
  BrotliDecoderState *const state =
      BrotliDecoderCreateInstance(NULL, NULL, NULL);
  hts_boolean ok = HTS_TRUE, done = HTS_FALSE;
  LLint total = 0;

  if (state == NULL)
    return -1;
  while (ok && !done) {
    unsigned char BIGSTK inbuf[8192];
    size_t avail_in = fread(inbuf, 1, sizeof(inbuf), in);
    const uint8_t *next_in = inbuf;

    if (avail_in == 0)
      break; /* EOF; a stream still hungry for input is truncated */
    for (;;) {
      unsigned char BIGSTK outbuf[8192];
      uint8_t *next_out = outbuf;
      size_t avail_out = sizeof(outbuf);
      size_t produced;
      BrotliDecoderResult res;

      res = BrotliDecoderDecompressStream(state, &avail_in, &next_in,
                                          &avail_out, &next_out, NULL);
      produced = sizeof(outbuf) - avail_out;
      if (produced > 0) {
        total += (LLint) produced;
        if (total > maxout || fwrite(outbuf, 1, produced, out) != produced) {
          ok = HTS_FALSE;
          break;
        }
      }
      if (res == BROTLI_DECODER_RESULT_ERROR) {
        ok = HTS_FALSE;
        break;
      }
      if (res == BROTLI_DECODER_RESULT_SUCCESS) {
        done = HTS_TRUE;
        break;
      }
      if (res == BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT)
        break;
    }
    if (ferror(in))
      ok = HTS_FALSE;
  }
  BrotliDecoderDestroyInstance(state);
  return (ok && done && !ferror(in)) ? (int) total : -1;
}

static size_t codec_head_brotli(const void *in, size_t in_len, void *out,
                                size_t out_len) {
  BrotliDecoderState *const state =
      BrotliDecoderCreateInstance(NULL, NULL, NULL);
  const uint8_t *next_in = (const uint8_t *) in;
  uint8_t *next_out = (uint8_t *) out;
  size_t avail_in = in_len, avail_out = out_len;
  BrotliDecoderResult res;

  if (state == NULL)
    return 0;
  res = BrotliDecoderDecompressStream(state, &avail_in, &next_in, &avail_out,
                                      &next_out, NULL);
  BrotliDecoderDestroyInstance(state);
  return (res != BROTLI_DECODER_RESULT_ERROR) ? out_len - avail_out : 0;
}
#endif

#if HTS_USEZSTD
static int codec_unpack_zstd(FILE *in, FILE *out, LLint maxout) {
  ZSTD_DStream *const zds = ZSTD_createDStream();
  hts_boolean ok = HTS_TRUE, eof = HTS_FALSE;
  size_t zret = 1; /* 0 once a frame is fully flushed */
  LLint total = 0;

  if (zds == NULL)
    return -1;
  if (ZSTD_isError(ZSTD_DCtx_setParameter(zds, ZSTD_d_windowLogMax,
                                          HTS_ZSTD_WINDOWLOG_MAX))) {
    ZSTD_freeDStream(zds);
    return -1;
  }
  while (ok && !eof) {
    unsigned char BIGSTK inbuf[8192];
    ZSTD_inBuffer input;

    input.src = inbuf;
    input.size = fread(inbuf, 1, sizeof(inbuf), in);
    input.pos = 0;
    if (input.size == 0) {
      eof = HTS_TRUE;
      break;
    }
    while (input.pos < input.size) {
      unsigned char BIGSTK outbuf[8192];
      ZSTD_outBuffer output;

      output.dst = outbuf;
      output.size = sizeof(outbuf);
      output.pos = 0;
      zret = ZSTD_decompressStream(zds, &output, &input);
      if (ZSTD_isError(zret)) {
        ok = HTS_FALSE;
        break;
      }
      if (output.pos > 0) {
        total += (LLint) output.pos;
        if (total > maxout ||
            fwrite(outbuf, 1, output.pos, out) != output.pos) {
          ok = HTS_FALSE;
          break;
        }
      }
    }
    if (ferror(in))
      ok = HTS_FALSE;
  }
  ZSTD_freeDStream(zds);
  /* zret != 0 at EOF: the last frame never completed */
  return (ok && zret == 0 && !ferror(in)) ? (int) total : -1;
}

static size_t codec_head_zstd(const void *in, size_t in_len, void *out,
                              size_t out_len) {
  ZSTD_DStream *const zds = ZSTD_createDStream();
  ZSTD_inBuffer input;
  ZSTD_outBuffer output;
  size_t zret;

  if (zds == NULL)
    return 0;
  if (ZSTD_isError(ZSTD_DCtx_setParameter(zds, ZSTD_d_windowLogMax,
                                          HTS_ZSTD_WINDOWLOG_MAX))) {
    ZSTD_freeDStream(zds);
    return 0;
  }
  input.src = in;
  input.size = in_len;
  input.pos = 0;
  output.dst = out;
  output.size = out_len;
  output.pos = 0;
  zret = ZSTD_decompressStream(zds, &output, &input);
  ZSTD_freeDStream(zds);
  return ZSTD_isError(zret) ? 0 : output.pos;
}
#endif

LLint hts_codec_coded_size(FILE *in) {
  long size;

  if (fseek(in, 0, SEEK_END) != 0)
    return -1;
  size = ftell(in);
  if (size < 0 || fseek(in, 0, SEEK_SET) != 0)
    return -1;
  return (LLint) size;
}

int hts_codec_unpack(hts_codec codec, const char *filename,
                     const char *newfile) {
  char catbuff[CATBUFF_SIZE];
  FILE *in, *out;
  LLint maxout;
  int ret = -1;

  if (filename == NULL || newfile == NULL || !filename[0] || !newfile[0])
    return -1;
  switch (codec) {
  case HTS_CODEC_DEFLATE:
#if HTS_USEZLIB
    return hts_zunpack(filename, newfile);
#else
    return -1;
#endif
  case HTS_CODEC_BROTLI:
  case HTS_CODEC_ZSTD:
    break;
  default: /* IDENTITY never reaches the unpacker; UNSUPPORTED must fail */
    return -1;
  }
  in = FOPEN(fconv(catbuff, sizeof(catbuff), filename), "rb");
  if (in == NULL)
    return -1;
  maxout = hts_codec_maxout(hts_codec_coded_size(in));
  out = FOPEN(fconv(catbuff, sizeof(catbuff), newfile), "wb");
  if (out == NULL) {
    fclose(in);
    return -1;
  }
#if HTS_USEBROTLI
  if (codec == HTS_CODEC_BROTLI)
    ret = codec_unpack_brotli(in, out, maxout);
#endif
#if HTS_USEZSTD
  if (codec == HTS_CODEC_ZSTD)
    ret = codec_unpack_zstd(in, out, maxout);
#endif
  fclose(out);
  fclose(in);
  return ret;
}

size_t hts_codec_head(hts_codec codec, const void *in, size_t in_len, void *out,
                      size_t out_len) {
  if (in == NULL || in_len == 0 || out == NULL || out_len == 0)
    return 0;
  switch (codec) {
#if HTS_USEZLIB
  case HTS_CODEC_DEFLATE:
    return hts_zhead(in, in_len, out, out_len);
#endif
#if HTS_USEBROTLI
  case HTS_CODEC_BROTLI:
    return codec_head_brotli(in, in_len, out, out_len);
#endif
#if HTS_USEZSTD
  case HTS_CODEC_ZSTD:
    return codec_head_zstd(in, in_len, out, out_len);
#endif
  default:
    return 0;
  }
}
