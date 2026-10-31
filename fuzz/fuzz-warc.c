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

/* Fuzz the WARC/1.1 serializer (htswarc.c): whole transactions through the
   direct writer API the engine hook uses, plus the pure helpers it is built
   from (SURT canonicalization, header-block normalization and value picking,
   CDXJ extent rendering, truncation reason tokens).

   Threat model: the serializer's inputs derive from the wire - the raw
   request and response header blocks a hostile server answered with, the
   body it sent, the URL it was reached at - and the archive is written back
   out (gzip member framing, digests, CDXJ index lines). A mistake here
   corrupts the archival format or overflows while copying hostile header
   text; the CDXJ offsets are 64-bit arithmetic on file positions.

   Offline by construction: the writer only ever touches a private temp dir.

   Input format: three control bytes, then an op sheet "op args" (LF-separated;
   unknown ops are skipped so the format can grow).

     byte 0  response status code pick: 200 206 301 304 404 0 -1 999
     byte 1  0x01 .warc.gz (gzip members) 0x02 CDXJ index 0x04 write a
             second, revisit transaction 0x08 write a resource record too
     byte 2  low 2 bits WARC_TRUNC_* reason for the body; middle 2 bits
             unchanged_kind pick (none / server-304 / engine-forced) for the
             revisit; high bits set_cl pick for the normalization probe

     uri <url>            target_uri (default http://example.com/a.html)
     ip <addr>            peer IP (empty omits)
     ctype <mime>         content_type fallback for the CDXJ line
     reqhdr <hex>         request header block
     resphdr <hex>        response header block (status line + headers)
     body <hex>           body (max 2048 bytes)
     surt <url>           warc_surt() probe
     hdrvalue <hex>|<nm>  warc_http_header_value() probe
 */
#include "fuzz.h"

#include "httrack-library.h"
#include "htswarc.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* A transaction set past this size says nothing a smaller one cannot. */
#define FUZZ_WC_MAXSIZE (64 * 1024)
/* Longest hex blob a header block or body decodes to. */
#define FUZZ_WC_BLOB 2048

static char wc_dir[256];
static char wc_path[sizeof(wc_dir) + sizeof("/a.warc.gz")];
static char wc_cdx[sizeof(wc_dir) + sizeof("/a.cdx")];

/* keep probe results and the archive bytes alive past the calls */
static volatile size_t wc_sink;

static void wc_cleanup(void) {
  if (wc_path[0] != '\0')
    (void) UNLINK(wc_path);
  if (wc_cdx[0] != '\0')
    (void) UNLINK(wc_cdx);
  if (wc_dir[0] != '\0')
    (void) rmdir(wc_dir);
}

static int wc_setup(void) {
  if (wc_dir[0] == '\0') {
    const char *const tmp = getenv("TMPDIR");

    snprintf(wc_dir, sizeof(wc_dir), "%s/fuzz-warc-XXXXXX",
             tmp != NULL && *tmp != '\0' ? tmp : "/tmp");
    if (mkdtemp(wc_dir) == NULL) {
      return -1;
    }
    atexit(wc_cleanup);
  }
  return 0;
}

/* Clip-copy: header blocks and bodies stay inside the buffer the op line
   gave them, so an abort in the serializer names a module bug, never a
   harness one. */
static char *wc_field(char *dst, size_t dsize, const char *src, size_t n) {
  if (n >= dsize) {
    n = dsize - 1;
  }
  if (n > 0) {
    memcpy(dst, src, n);
  }
  dst[n] = '\0';
  return dst;
}

static int wc_hex(const char *s, char *dst, size_t dsize) {
  size_t n = 0;

  while (s[0] != '\0' && s[1] != '\0' && n + 1 < dsize) {
    unsigned int byte;
    int hi, lo;

    hi = s[0] >= '0' && s[0] <= '9' ? s[0] - '0'
         : s[0] >= 'a' && s[0] <= 'f' ? s[0] - 'a' + 10
         : s[0] >= 'A' && s[0] <= 'F' ? s[0] - 'A' + 10 : -1;
    lo = s[1] >= '0' && s[1] <= '9' ? s[1] - '0'
         : s[1] >= 'a' && s[1] <= 'f' ? s[1] - 'a' + 10
         : s[1] >= 'A' && s[1] <= 'F' ? s[1] - 'A' + 10 : -1;
    if (hi < 0 || lo < 0) {
      break;
    }
    byte = (unsigned int) ((hi << 4) | lo);
    dst[n++] = (char) byte;
    s += 2;
  }
  dst[n] = '\0';
  return (int) n;
}

static const int wc_codes[] = { 200, 206, 301, 304, 404, 0, -1, 999 };
static const int wc_unchanged[] = { WARC_UNCHANGED_NONE,
                                    WARC_UNCHANGED_SERVER_304,
                                    WARC_UNCHANGED_ENGINE_FORCED };

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static int inited = 0;

  const uint8_t *p, *end;
  httrackp *opt;
  warc_writer *w;
  int statuscode, truncated, unchanged, set_cl;
  int statuscode2 = 0;
  char uri[HTS_URLMAXSIZE * 2] = "http://example.com/a.html";
  char ip[64] = "";
  char ctype[HTS_URLMAXSIZE * 2] = "text/html";
  char reqhdr[FUZZ_WC_BLOB] = "";
  char resphdr[FUZZ_WC_BLOB] = "";
  char body[FUZZ_WC_BLOB] = "A";
  int reqhdr_len = 0, resphdr_len = 0, body_len = 1;
  int have_cdx = 0;

  if (size < 3 || size > FUZZ_WC_MAXSIZE) {
    return 0;
  }
  if (wc_setup() != 0) {
    return 0;
  }
  if (!inited) {
    hts_init();
    inited = 1;
  }

  statuscode = wc_codes[data[0] & 7];
  truncated = (int) (data[2] & 3);
  unchanged = wc_unchanged[(data[2] >> 2) % 3];
  set_cl = (int[]) { -1, 0, 5, 1048576 }[(data[2] >> 4) & 3];

  p = data + 3;
  end = data + size;
  while (p < end) {
    const uint8_t *eol = memchr(p, '\n', (size_t) (end - p));
    const uint8_t *line_end = eol != NULL ? eol : end;
    const uint8_t *sep = memchr(p, ' ', (size_t) (line_end - p));
    const uint8_t *val = sep != NULL ? sep + 1 : line_end;
    size_t keylen = sep != NULL ? (size_t) (sep - p) : (size_t) (line_end - p);
    size_t vallen = (size_t) (line_end - val);
    char line[FUZZ_WC_BLOB];

    if (sep == NULL || vallen == 0 || vallen >= sizeof(line)) {
      if (eol == NULL) {
        break;
      }
      p = eol + 1;
      continue;
    }
    memcpy(line, val, vallen);
    line[vallen] = '\0';

    if (keylen == 3 && memcmp(p, "uri", 3) == 0) {
      wc_field(uri, sizeof(uri), line, vallen);
    } else if (keylen == 2 && memcmp(p, "ip", 2) == 0) {
      wc_field(ip, sizeof(ip), line, vallen);
    } else if (keylen == 5 && memcmp(p, "ctype", 5) == 0) {
      wc_field(ctype, sizeof(ctype), line, vallen);
    } else if (keylen == 6 && memcmp(p, "reqhdr", 6) == 0) {
      reqhdr_len = wc_hex(line, reqhdr, sizeof(reqhdr));
    } else if (keylen == 7 && memcmp(p, "resphdr", 7) == 0) {
      resphdr_len = wc_hex(line, resphdr, sizeof(resphdr));
    } else if (keylen == 4 && memcmp(p, "body", 4) == 0) {
      body_len = wc_hex(line, body, sizeof(body));
      if (body_len <= 0) {
        body[0] = 'A';
        body_len = 1;
      }
    } else if (keylen == 4 && memcmp(p, "surt", 4) == 0) {
      char out[HTS_URLMAXSIZE * 2];

      out[0] = '\0';
      (void) warc_surt(line, out, sizeof(out));
      wc_sink += (size_t) out[0] + strlen(out);
    } else if (keylen == 8 && memcmp(p, "hdrvalue", 8) == 0) {
      char *bar = strchr(line, '|');

      if (bar != NULL) {
        char hdr[FUZZ_WC_BLOB];
        char out[128];
        size_t k;

        *bar++ = '\0';
        (void) wc_hex(line, hdr, sizeof(hdr));
        memset(out, 0, sizeof(out));
        warc_http_header_value(hdr, bar, out, sizeof(out));
        for (k = 0; k < sizeof(out); k++)
          wc_sink += (size_t) out[k];
      }
    }
    /* unknown ops are skipped: the format can grow without breaking old
       corpora */
    if (eol == NULL) {
      break;
    }
    p = eol + 1;
  }

  opt = hts_create_opt();
  opt->log = opt->errlog = NULL;
  if ((data[1] & 0x02) != 0) {
    opt->warc_cdx = HTS_TRUE;
    have_cdx = 1;
  }
  snprintf(wc_path, sizeof(wc_path), "%s/a.warc%s", wc_dir,
           (data[1] & 0x01) != 0 ? ".gz" : "");
  snprintf(wc_cdx, sizeof(wc_cdx), "%s/a.cdx", wc_dir);
  (void) UNLINK(wc_path);
  (void) UNLINK(wc_cdx);

  /* One finished transaction through the direct writer API the engine hook
     wraps: wire-derived header blocks, body, status, revisit and truncation
     tags, then whatever second transaction the sheet asks for. */
  if ((w = warc_open(opt, wc_path)) != NULL) {
    (void) warc_write_transaction(w, uri, ip[0] != '\0' ? ip : NULL,
                                  reqhdr_len > 0 ? reqhdr : NULL,
                                  resphdr_len > 0 ? resphdr : NULL, body,
                                  (size_t) body_len, NULL,
                                  ctype[0] != '\0' ? ctype : NULL, statuscode,
                                  WARC_UNCHANGED_NONE,
                                  truncated != 0 ? truncated : 0);
    if ((data[1] & 0x04) != 0) {
      statuscode2 = statuscode;
      (void) warc_write_transaction(w, uri, ip[0] != '\0' ? ip : NULL,
                                    reqhdr_len > 0 ? reqhdr : NULL,
                                    resphdr_len > 0 ? resphdr : NULL, body,
                                    (size_t) body_len, NULL,
                                    ctype[0] != '\0' ? ctype : NULL,
                                    statuscode2, unchanged, 0);
    }
    if ((data[1] & 0x08) != 0) {
      (void) warc_write_resource(w, uri, ip[0] != '\0' ? ip : NULL,
                                 ctype[0] != '\0' ? ctype : NULL, body,
                                 (size_t) body_len, NULL, truncated);
    }
    warc_close(w);
  }

  /* The archive is the serializer's output; touch it so the writes stay
     honest, then drop it to keep runs bounded. */
  {
    FILE *const fp = fopen(wc_path, "rb");

    if (fp != NULL) {
      int c;

      while ((c = getc(fp)) != EOF)
        wc_sink += (size_t) c;
      (void) fclose(fp);
    }
  }
  if (have_cdx) {
    FILE *const fp = fopen(wc_cdx, "rb");

    if (fp != NULL) {
      int c;

      while ((c = getc(fp)) != EOF)
        wc_sink += (size_t) c;
      (void) fclose(fp);
    }
  }
  (void) UNLINK(wc_path);
  (void) UNLINK(wc_cdx);

  /* The pure helpers, on the same hostile text. */
  {
    char out[HTS_URLMAXSIZE * 2];
    char norm[512];
    char hv[128];
    char ext[WARC_CDX_EXTENT_SIZE];

    out[0] = '\0';
    (void) warc_surt(uri, out, sizeof(out));
    wc_sink += (size_t) out[0];
    memset(norm, 0, sizeof(norm));
    if (resphdr_len > 0) {
      char *nh = warc_normalized_headers(resphdr, set_cl);

      if (nh != NULL) {
        wc_sink += (size_t) nh[0] + strlen(nh);
        freet(nh);
      }
    }
    memset(hv, 0, sizeof(hv));
    if (resphdr_len > 0) {
      warc_http_header_value(resphdr, "Content-Type", hv, sizeof(hv));
      wc_sink += (size_t) hv[0];
    }
    memset(ext, 0, sizeof(ext));
    wc_sink += (size_t) warc_cdx_extent(ext, sizeof(ext), 4294967296ULL,
                                        1099511627776ULL);
    wc_sink += (size_t) (warc_truncated_reason(truncated) != NULL);
    /* warc_stream_offset() tells f and falls back on a tell failure; f must
       be a real stream, so probe it on the just-written archive. */
    {
      FILE *const fp = fopen(wc_path, "rb");

      if (fp != NULL) {
        wc_sink += (size_t) warc_stream_offset(fp, 12345);
        (void) fclose(fp);
      }
    }
  }

  hts_free_opt(opt);
  return 0;
}
