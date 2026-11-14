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

Please visit our Website: http://www.httrack.com
*/

/* Fuzz the Content-Encoding unpackers: a coded body in, a decoded file out,
   under the -m cap. */
#define ZLIB_CONST /* next_in may then point at the const fuzzer input */
#include "fuzz.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <zlib.h>

#include "htscodec.h"

/* A coded body past this size says nothing a smaller one cannot. */
#define FUZZ_CODEC_MAXSIZE (256 * 1024)
/* The unpacker's read and write chunk, which decides where a bomb is seen. */
#define FUZZ_CODEC_CHUNK 8192
/* fuzz_inflate_expect() result when zlib itself failed, so no verdict holds. */
#define FUZZ_CODEC_NO_ORACLE (-100)

static char codec_dir[256];
static char in_path[sizeof(codec_dir) + sizeof("/in.bin")];
static char out_path[sizeof(codec_dir) + sizeof("/out.bin")];

static void fuzz_codec_cleanup(void) {
  (void) unlink(in_path);
  (void) unlink(out_path);
  (void) rmdir(codec_dir);
}

static int fuzz_codec_setup(void) {
  if (in_path[0] == '\0') {
    const char *const tmp = getenv("TMPDIR");

    snprintf(codec_dir, sizeof(codec_dir), "%s/fuzz-codec-XXXXXX",
             tmp != NULL && *tmp != '\0' ? tmp : "/tmp");
    if (mkdtemp(codec_dir) == NULL) {
      return -1;
    }
    snprintf(in_path, sizeof(in_path), "%s/in.bin", codec_dir);
    snprintf(out_path, sizeof(out_path), "%s/out.bin", codec_dir);
    atexit(fuzz_codec_cleanup);
  }
  return 0;
}

/* Cap 0 is "no -m limit"; the others sit around the chunk size and the 1 MiB
   built-in floor, so the cap binds on some bodies and not others. */
static const LLint fuzz_codec_caps[8] = {0,    1,    100,   4096,
                                         8192, 8193, 65536, 1 << 20};

typedef enum {
  FUZZ_INFLATE_OK,
  FUZZ_INFLATE_BOMB,
  FUZZ_INFLATE_DATA_ERROR, /* the stream is not in this framing */
  FUZZ_INFLATE_FAIL,       /* truncated, or a preset dictionary */
  FUZZ_INFLATE_ENV_ERROR
} fuzz_inflate_result;

/* Inflate body into *out, grown as needed, stopping once past maxout. */
static fuzz_inflate_result fuzz_inflate(const uint8_t *body, size_t body_size,
                                        int window_bits, LLint maxout,
                                        unsigned char **out, size_t *len) {
  fuzz_inflate_result result = FUZZ_INFLATE_FAIL;
  size_t pos = 0, capacity = 0;
  z_stream strm;

  *len = 0;
  memset(&strm, 0, sizeof(strm));
  if (inflateInit2(&strm, window_bits) != Z_OK) {
    return FUZZ_INFLATE_ENV_ERROR;
  }
  do {
    const size_t navail =
        body_size - pos < FUZZ_CODEC_CHUNK ? body_size - pos : FUZZ_CODEC_CHUNK;
    int zerr;

    strm.next_in = (const Bytef *) (body + pos);
    strm.avail_in = (uInt) navail;
    pos += navail;
    do {
      if (capacity - *len < FUZZ_CODEC_CHUNK) {
        capacity = capacity * 2 + FUZZ_CODEC_CHUNK;
        *out = realloct(*out, capacity);
      }
      strm.next_out = *out + *len;
      strm.avail_out = FUZZ_CODEC_CHUNK;
      zerr = inflate(&strm, Z_NO_FLUSH);
      if (zerr == Z_DATA_ERROR || zerr == Z_NEED_DICT) {
        result = window_bits < 0 ? FUZZ_INFLATE_DATA_ERROR : FUZZ_INFLATE_FAIL;
        goto done;
      }
      if (zerr == Z_MEM_ERROR || zerr == Z_STREAM_ERROR) {
        result = FUZZ_INFLATE_ENV_ERROR;
        goto done;
      }
      *len += FUZZ_CODEC_CHUNK - strm.avail_out;
      if ((LLint) *len > maxout) {
        result = FUZZ_INFLATE_BOMB;
        goto done;
      }
    } while (strm.avail_out == 0);
    if (zerr == Z_STREAM_END) {
      result = FUZZ_INFLATE_OK;
      break;
    }
  } while (pos < body_size);
done:
  inflateEnd(&strm);
  return result;
}

/* The gzip/deflate verdict owed, by zlib alone: the framing the header
   suggests, the other one, then a verbatim copy. *out stays NULL when that copy
   is due. */
static int fuzz_inflate_expect(const uint8_t *body, size_t body_size, LLint cap,
                               unsigned char **out, size_t *len) {
  const hts_boolean wrapped =
      body_size >= 2 && ((body[0] == 0x1f && body[1] == 0x8b) ||
                         ((body[0] & 0x0f) == Z_DEFLATED &&
                          (((unsigned) body[0] << 8 | body[1]) % 31) == 0));
  const LLint maxout = hts_codec_maxout((LLint) body_size);
  const hts_boolean binds = cap > 0 && cap < maxout;
  const LLint budget = binds ? cap : maxout;
  const int bomb = binds ? HTS_CODEC_TOOBIG : -1;
  hts_boolean raw_data_error = HTS_FALSE;
  int attempt;

  for (attempt = 0; attempt < 2; attempt++) {
    const int window_bits =
        (attempt == 0) == wrapped ? 32 + MAX_WBITS : -MAX_WBITS;

    switch (fuzz_inflate(body, body_size, window_bits, budget, out, len)) {
    case FUZZ_INFLATE_OK:
      return (int) *len;
    case FUZZ_INFLATE_BOMB:
      return bomb;
    case FUZZ_INFLATE_DATA_ERROR:
      raw_data_error = HTS_TRUE;
      break;
    case FUZZ_INFLATE_FAIL:
      break;
    case FUZZ_INFLATE_ENV_ERROR:
      return FUZZ_CODEC_NO_ORACLE;
    }
  }
  freet(*out);
  if (wrapped || !raw_data_error) {
    return -1;
  }
  return (LLint) body_size > budget ? bomb : (int) body_size;
}

/* Contents of the decoded file, which must be exactly size bytes. */
static unsigned char *fuzz_read_out(int size) {
  unsigned char *const buf = malloct((size_t) size + 1);
  FILE *const fp = fopen(out_path, "rb");

  if (fp == NULL || fread(buf, 1, (size_t) size + 1, fp) != (size_t) size) {
    abort();
  }
  fclose(fp);
  return buf;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static const hts_codec codecs[4] = {HTS_CODEC_DEFLATE, HTS_CODEC_BROTLI,
                                      HTS_CODEC_ZSTD, HTS_CODEC_UNSUPPORTED};
  const uint8_t *body;
  size_t body_size;
  hts_codec codec;
  LLint cap;
  hts_boolean sniff, written;
  FILE *fp;
  unsigned char head[64];
  unsigned char *expect_buf = NULL;
  size_t expect_len = 0;
  int ret, expect = FUZZ_CODEC_NO_ORACLE;

  if (size < 1 || size - 1 > FUZZ_CODEC_MAXSIZE || fuzz_codec_setup() != 0) {
    return 0;
  }
  codec = codecs[data[0] & 3];               /* bits 0-1 */
  cap = fuzz_codec_caps[(data[0] >> 2) & 7]; /* bits 2-4 */
  sniff = (data[0] & 0x20) != 0;             /* bit 5; bits 6-7 unused */
  body = data + 1;
  body_size = size - 1;
  if ((fp = fopen(in_path, "wb")) == NULL) {
    return 0;
  }
  written = fwrite(body, 1, body_size, fp) == body_size;
  if (fclose(fp) != 0 || !written) {
    return 0;
  }

  /* the MIME sniffer's prefix decoder reads the same body from memory */
  if (sniff && hts_codec_head(codec, body, body_size, head, sizeof(head)) >
                   sizeof(head)) {
    abort();
  }
  ret = hts_codec_unpack_max(codec, in_path, out_path, cap);
  /* A size within budget, -1, or the cap verdict for a codec we decode. On a
     healthy disk a failure is the body's, which errno 0 says. */
  if (ret < HTS_CODEC_TOOBIG || (ret == HTS_CODEC_TOOBIG && cap == 0) ||
      (codec == HTS_CODEC_UNSUPPORTED && ret != -1) ||
      (ret < 0 && errno != 0) ||
      ret > hts_codec_budget((LLint) body_size, cap)) {
    abort();
  }
  if (codec == HTS_CODEC_DEFLATE) {
    expect =
        fuzz_inflate_expect(body, body_size, cap, &expect_buf, &expect_len);
    if (expect != FUZZ_CODEC_NO_ORACLE && ret != expect) {
      abort();
    }
  }
  if (ret >= 0) {
    unsigned char *got = fuzz_read_out(ret);

    if (expect >= 0 && memcmp(got, expect_buf != NULL ? expect_buf : body,
                              (size_t) ret) != 0) {
      abort();
    }
    freet(got);
  }
  freet(expect_buf);

  /* a body decoding past the cap without one owes the cap verdict */
  if (cap > 0 && ret < 0 && codec != HTS_CODEC_UNSUPPORTED) {
    const int uncapped = hts_codec_unpack_max(codec, in_path, out_path, 0);

    if ((uncapped < 0 && errno != 0) ||
        (uncapped > cap && ret != HTS_CODEC_TOOBIG)) {
      abort();
    }
  }
  return 0;
}
