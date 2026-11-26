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
/* File: htswarc_selftest.c subroutines:                        */
/*       self-tests for WARC and WACZ archive writing           */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htsselftest_int.h"

#include <stdint.h>

/* Inflate one gzip member at *in (limit end); returns the decompressed record
   in a malloc'd buffer (*out_len), advancing *in past the member. NULL at end
   or on error (*out_len distinguishes: 0 and NULL = clean end). */
static unsigned char *warc_next_member(const unsigned char **in,
                                       const unsigned char *end,
                                       size_t *out_len) {
  z_stream zs;
  unsigned char *out = NULL;
  size_t len = 0;
  int zerr;
  *out_len = 0;
  if (*in >= end)
    return NULL;
  memset(&zs, 0, sizeof(zs));
  if (inflateInit2(&zs, 15 + 32) != Z_OK)
    return NULL;
  zs.next_in = (const Bytef *) *in;
  zs.avail_in = (uInt) (end - *in);
  do {
    unsigned char tmp[8192];
    size_t got;
    zs.next_out = tmp;
    zs.avail_out = sizeof(tmp);
    zerr = inflate(&zs, Z_NO_FLUSH);
    if (zerr != Z_OK && zerr != Z_STREAM_END) {
      freet(out);
      inflateEnd(&zs);
      return NULL;
    }
    got = sizeof(tmp) - zs.avail_out;
    if (got > 0) {
      unsigned char *n = realloct(out, len + got + 1);
      if (n == NULL) {
        freet(out);
        inflateEnd(&zs);
        return NULL;
      }
      out = n;
      memcpy(out + len, tmp, got);
      len += got;
    }
  } while (zerr != Z_STREAM_END);
  *in = (const unsigned char *) zs.next_in; /* start of the next member */
  inflateEnd(&zs);
  if (out != NULL)
    out[len] = '\0';
  *out_len = len;
  return out;
}

/* Where does the first NEEDLE in HAY, which may hold NULs, end? NULL if absent.
 */
static const char *warc_after(hts_span hay, const char *needle) {
  const size_t n = strlen(needle);
  const char *const at = hts_memstr(hay, hts_span_at(needle, n));

  return at != NULL ? at + n : NULL;
}

/* Validate a synthetic transaction's .warc.gz against WARC/1.1. */
static int st_warc(httrackp *opt, int argc, char **argv) {
  char path[HTS_URLMAXSIZE];
  warc_writer *w;
  unsigned char *data;
  size_t data_len = 0;
  const unsigned char *p, *end;
  int err = 0, nrec = 0, nresp = 0, nreq = 0, nrevisit = 0, ninfo = 0;
  int seen_a_body = 0, body_occurrences = 0, a2_bodyless = 0, nm_cl_ok = 0;
  static const char a_body[] = "Hello, WARC!\n";

  fconcat(path, sizeof(path), argv[0], "warc-selftest.warc.gz");

  w = warc_open(opt, path);
  assertf(w != NULL);

  /* 200 HTML, plaintext body: bogus Content-Length rewritten, hop-by-hop
     Transfer-Encoding dropped. The whitespace before its ':' exercises
     the name match tolerating "Name : value". */
  warc_write_transaction(
      w, "http://test.local/a.html", "127.0.0.1",
      "GET /a.html HTTP/1.1\r\nHost: test.local\r\n\r\n",
      "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n"
      "Transfer-Encoding : chunked\r\nContent-Length: 999\r\n\r\n",
      warc_body_mem(hts_span_at(a_body, sizeof(a_body) - 1)), NULL, 200, 0, 0);

  /* 302 redirect: header-only, no body. */
  warc_write_transaction(
      w, "http://test.local/r", "127.0.0.1",
      "GET /r HTTP/1.1\r\nHost: test.local\r\n\r\n",
      "HTTP/1.1 302 Found\r\nLocation: http://test.local/a.html\r\n\r\n",
      warc_body_mem(hts_span_at(NULL, 0)), NULL, 302, 0, 0);

  /* 200 binary, chunked coding on the wire (already de-chunked here). */
  warc_write_transaction(
      w, "http://test.local/b.bin", "127.0.0.1",
      "GET /b.bin HTTP/1.1\r\nHost: test.local\r\n\r\n",
      "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\n"
      "Transfer-Encoding: chunked\r\n\r\n",
      warc_body_mem(hts_span_at("\x00\x01\x02\x03\x04", 5)), NULL, 200, 0, 0);

  /* 200 with a body shorter than the declared Content-Length (rewritten). */
  warc_write_transaction(
      w, "http://test.local/trunc", "127.0.0.1",
      "GET /trunc HTTP/1.1\r\nHost: test.local\r\n\r\n",
      "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: "
      "100\r\n\r\n",
      warc_body_mem(hts_span_at("short", 5)), NULL, 200, 0, 0);

  /* Same payload as a.html at a new URL: identical-payload-digest revisit
     (OpenSSL builds only; a plain build writes a second full response). */
  warc_write_transaction(w, "http://test.local/a2.html", "127.0.0.1",
                         "GET /a2.html HTTP/1.1\r\nHost: test.local\r\n\r\n",
                         "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n",
                         warc_body_mem(hts_span_at(a_body, sizeof(a_body) - 1)),
                         NULL, 200, 0, 0);

  /* 304 revisit with an EMPTY response-header block: the block is just the
     2-byte separator, so declared Content-Length must be exactly 2 (F3). */
  warc_write_transaction(w, "http://test.local/nm", "127.0.0.1",
                         "GET /nm HTTP/1.1\r\nHost: test.local\r\n\r\n", "",
                         warc_body_mem(hts_span_at(NULL, 0)), NULL, 304, 1, 0);

  warc_close(w);

  data = (unsigned char *) st_slurp(path, &data_len);
  assertf(data != NULL);
  p = data;
  end = data + data_len;

  while (p < end) {
    size_t rlen = 0;
    unsigned char *rec = warc_next_member(&p, end, &rlen);
    const char *sep, *cl;
    long long block_len = 0; /* 0 on a parse failure; err is already set */
    size_t hdr_len;
    if (rec == NULL) {
      if (rlen == 0)
        break; /* clean end */
      err = 1;
      break;
    }
    const hts_span whole = hts_span_at((const char *) rec, rlen);
    nrec++;
    /* magic */
    ST_FAIL_IF(err, rlen < 8 || memcmp(rec, "WARC/1.", 7) != 0);
    /* record header ends at the first blank line */
    sep = warc_after(whole, "\r\n\r\n");
    if (sep == NULL) {
      err = 1;
      freet(rec);
      continue;
    }
    hdr_len = (size_t) ((const unsigned char *) sep - rec);
    const hts_span head = hts_span_at((const char *) rec, hdr_len);
    /* Content-Length must equal the actual block length */
    cl = warc_after(head, "Content-Length:");
    if (cl == NULL || sscanf(cl, "%lld", &block_len) != 1)
      err = 1;
    else {
      if (hdr_len + (size_t) block_len + 4 != rlen)
        err = 1; /* header + block + trailing CRLFCRLF */
      else if (memcmp(rec + hdr_len + block_len, "\r\n\r\n", 4) != 0)
        err = 1; /* trailer intact */
    }
    if (warc_after(head, "WARC-Type: warcinfo") != NULL)
      ninfo++;
    if (warc_after(head, "WARC-Type: request") != NULL)
      nreq++;
    if (warc_after(head, "WARC-Type: response") != NULL)
      nresp++;
    if (warc_after(head, "WARC-Type: revisit") != NULL)
      nrevisit++;
    /* F1: the full body must appear exactly once across the whole file (a
       revisit must not re-embed it). */
    if (warc_after(whole, a_body) != NULL)
      body_occurrences++;
    /* F1: the a2.html identical-payload-digest revisit carries no body. */
    if (warc_after(head, "WARC-Target-URI: http://test.local/a2.html") !=
            NULL &&
        warc_after(head, "WARC-Type: revisit") != NULL)
      a2_bodyless = (warc_after(whole, a_body) == NULL);
    /* F3: the empty-header 304 revisit block is exactly the 2-byte separator
       (the request record shares this target URI, so match the revisit only).
     */
    if (warc_after(head, "WARC-Target-URI: http://test.local/nm") != NULL &&
        warc_after(head, "WARC-Type: revisit") != NULL)
      nm_cl_ok = (block_len == 2);
    /* a.html response body round-trips; no Content-Encoding (plaintext) and the
       whitespaced Transfer-Encoding was dropped. */
    if (warc_after(head, "WARC-Target-URI: http://test.local/a.html") != NULL &&
        warc_after(head, "msgtype=response") != NULL) {
      const hts_span msg =
          hts_span_at((const char *) rec, hdr_len + (size_t) block_len);
      const char *bsep = warc_after(hts_span_skip(msg, hdr_len), "\r\n\r\n");
      if (bsep == NULL)
        err = 1;
      else {
        size_t bodyoff = (size_t) (bsep - (char *) rec);
        size_t got = rlen - 4 - bodyoff; /* minus record trailer */
        ST_FAIL_IF(err, got != sizeof(a_body) - 1 ||
                            memcmp(rec + bodyoff, a_body, got) != 0);
        seen_a_body = 1;
      }
      ST_FAIL_IF(err, warc_after(msg, "Content-Encoding") != NULL ||
                          warc_after(msg, "Transfer-Encoding") != NULL);
    }
    freet(rec);
  }
  freet(data);

  /* warcinfo + 6 transactions (response/revisit + request each) = 13 records.
   */
  ST_FAIL_IF(err, ninfo != 1 || nreq != 6 || nrec != 13 || !seen_a_body ||
                      !nm_cl_ok);
#if HTS_USEOPENSSL
  /* a.html + b.bin + trunc + 302 are full responses; a2.html deduped to a
     revisit (bodyless), nm is the 304 revisit; the body appears exactly once.
   */
  ST_FAIL_IF(err, nrevisit != 2 || nresp != 4 || !a2_bodyless ||
                      body_occurrences != 1);
#else
  /* No digests: a2.html is a second full response, so the body appears twice
     and only the 304 nm is a revisit. */
  ST_FAIL_IF(err, nrevisit != 1 || nresp != 5 || body_occurrences != 2);
  (void) a2_bodyless; /* only meaningful with digests */
#endif

  printf("warc: %d records (%d response, %d request, %d revisit): %s\n", nrec,
         nresp, nreq, nrevisit, err ? "FAIL" : "OK");
  return err;
}

/* Parse a record's header/block split; sets *hdr_len and *block_len, returns 0
   when Content-Length matches the actual block bytes, -1 otherwise. */
static int warc_rec_split(hts_span rec, size_t *hdr_len, long long *block_len) {
  const char *sep = warc_after(rec, "\r\n\r\n");
  const char *cl;
  *block_len = 0;
  if (sep == NULL)
    return -1;
  *hdr_len = (size_t) (sep - rec.p);
  cl = warc_after(hts_span_at(rec.p, *hdr_len), "Content-Length:");
  if (cl == NULL || sscanf(cl, "%lld", block_len) != 1 ||
      *hdr_len + (size_t) *block_len + 4 != rec.len)
    return -1;
  return 0;
}

/* A cap-truncated body is archived as WARC-Truncated "length", or "time"
   when gzip-coded, which keeps Content-Encoding. */
static int st_warc_trunc(httrackp *opt, int argc, char **argv) {
  char path[HTS_URLMAXSIZE];
  warc_writer *w;
  unsigned char *data;
  size_t data_len = 0;
  const unsigned char *p, *end;
  int err = 0, trunc_len = 0, trunc_gz = 0, nresp = 0;
  static const char body[] = "partial body bytes\n";
  /* a valid gzip member (inflates to a known plaintext), as the coded partial
   */
  static const unsigned char gz[] = {
      0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xff, 0x0b, 0x2e,
      0x29, 0x4a, 0x2c, 0x49, 0x4d, 0xaf, 0xd4, 0x75, 0x54, 0x28, 0x4b, 0x2d,
      0x4a, 0x4a, 0x2c, 0xc9, 0xcc, 0x55, 0x08, 0x77, 0x0c, 0x72, 0x56, 0x48,
      0xca, 0x4f, 0xa9, 0xb4, 0x52, 0x28, 0xc9, 0x48, 0x55, 0x28, 0x2c, 0xcd,
      0x4c, 0xce, 0x56, 0x48, 0x2a, 0xca, 0x2f, 0xcf, 0x53, 0x48, 0xcb, 0xaf,
      0x50, 0xc8, 0x2a, 0xcd, 0x2d, 0x28, 0xd6, 0xe3, 0x02, 0x00, 0x5e, 0xb8,
      0xe7, 0x66, 0x3a, 0x00, 0x00, 0x00};

  fconcat(path, sizeof(path), argv[0], "warc-trunc.warc.gz");
  w = warc_open(opt, path);
  assertf(w != NULL);
  warc_write_transaction(
      w, "http://test.local/big.bin", "127.0.0.1",
      "GET /big.bin HTTP/1.1\r\nHost: test.local\r\n\r\n",
      "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\n\r\n",
      warc_body_mem(hts_span_at(body, sizeof(body) - 1)), NULL, 200, 0,
      WARC_TRUNC_LENGTH);
  warc_write_transaction(
      w, "http://test.local/big.gz", "127.0.0.1",
      "GET /big.gz HTTP/1.1\r\nHost: test.local\r\n\r\n",
      "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Encoding: "
      "gzip\r\n\r\n",
      warc_body_mem(hts_span_at((const char *) gz, sizeof(gz))), NULL, 200, 0,
      WARC_TRUNC_TIME);
  warc_close(w);

  data = (unsigned char *) st_slurp(path, &data_len);
  assertf(data != NULL);
  p = data;
  end = data + data_len;
  while (p < end) {
    size_t rlen = 0, hdr_len = 0;
    long long block_len = 0;
    unsigned char *rec = warc_next_member(&p, end, &rlen);
    if (rec == NULL) {
      ST_FAIL_IF(err, rlen != 0);
      break;
    }
    if (warc_rec_split(hts_span_at((const char *) rec, rlen), &hdr_len,
                       &block_len) != 0) {
      err = 1;
      freet(rec);
      continue;
    }
    const hts_span head = hts_span_at((const char *) rec, hdr_len);
    if (warc_after(head, "WARC-Type: response") != NULL) {
      nresp++;
      if (warc_after(head, "WARC-Target-URI: http://test.local/big.bin") !=
              NULL &&
          warc_after(head, "WARC-Truncated: length") != NULL)
        trunc_len = 1;
      if (warc_after(head, "WARC-Target-URI: http://test.local/big.gz") !=
          NULL) {
        const hts_span block =
            hts_span_at((const char *) rec + hdr_len, (size_t) block_len);
        const char *bsep = warc_after(block, "\r\n\r\n");
        size_t bodyoff = bsep ? (size_t) (bsep - (char *) rec) : 0;
        size_t got = bsep ? rlen - 4 - bodyoff : 0;
        /* WARC-Truncated: time, Content-Encoding kept, stored body == coded. */
        if (bsep != NULL && warc_after(head, "WARC-Truncated: time") != NULL &&
            warc_after(block, "Content-Encoding:") != NULL &&
            got == sizeof(gz) && memcmp(rec + bodyoff, gz, sizeof(gz)) == 0)
          trunc_gz = 1;
      }
    }
    freet(rec);
  }
  freet(data);
  ST_FAIL_IF(err, !trunc_len || !trunc_gz || nresp != 2);
  return st_verdict("warc-trunc", err);
}

/* An ftp:// capture is ONE resource record: WARC-Type: resource, the payload's
   own Content-Type, block == payload, and no request/response pair. */
static int st_warc_ftp(httrackp *opt, int argc, char **argv) {
  char path[HTS_URLMAXSIZE];
  warc_writer *w;
  unsigned char *data;
  size_t data_len = 0;
  const unsigned char *p, *end;
  int err = 0, nresource = 0, nresp = 0, nreq = 0;
  static const char body[] = "\x00\x01"
                             "FTP payload"
                             "\x02\x03";

  fconcat(path, sizeof(path), argv[0], "warc-ftp.warc.gz");
  w = warc_open(opt, path);
  assertf(w != NULL);
  warc_write_resource(w, "ftp://ftp.local/file.bin", "127.0.0.1",
                      "application/octet-stream",
                      warc_body_mem(hts_span_at(body, sizeof(body) - 1)), 0);
  warc_close(w);

  data = (unsigned char *) st_slurp(path, &data_len);
  assertf(data != NULL);
  p = data;
  end = data + data_len;
  while (p < end) {
    size_t rlen = 0, hdr_len = 0;
    long long block_len = 0;
    unsigned char *rec = warc_next_member(&p, end, &rlen);
    if (rec == NULL) {
      ST_FAIL_IF(err, rlen != 0);
      break;
    }
    ST_FAIL_IF(err, warc_rec_split(hts_span_at((const char *) rec, rlen),
                                   &hdr_len, &block_len) != 0);
    const hts_span head = hts_span_at((const char *) rec, hdr_len);
    if (warc_after(head, "WARC-Type: resource") != NULL) {
      nresource++;
      /* block is the raw payload, no HTTP envelope */
      ST_FAIL_IF(err, (size_t) block_len != sizeof(body) - 1 ||
                          memcmp(rec + hdr_len, body, sizeof(body) - 1) != 0);
      if (warc_after(head, "WARC-Target-URI: ftp://ftp.local/file.bin") ==
              NULL ||
          warc_after(head, "Content-Type: application/octet-stream") == NULL)
        err = 1;
    }
    if (warc_after(head, "WARC-Type: response") != NULL)
      nresp++;
    if (warc_after(head, "WARC-Type: request") != NULL)
      nreq++;
    freet(rec);
  }
  freet(data);
  ST_FAIL_IF(err, nresource != 1 || nresp != 0 || nreq != 0);
  printf("warc-ftp: resource=%d response=%d request=%d: %s\n", nresource, nresp,
         nreq, err ? "FAIL" : "OK");
  return err;
}

/* --warc-max-size rotates into <base>-00000.warc.gz, -00001, ...; each segment
   is independently valid and begins with its own warcinfo. */
static int st_warc_rotate(httrackp *opt, int argc, char **argv) {
  char path[HTS_URLMAXSIZE];
  char seg[HTS_URLMAXSIZE];
  warc_writer *w;
  LLint saved_max;
  unsigned char body[600];
  unsigned int rng = 0x12345678u;
  int err = 0, nseg = 0, i;
  size_t j;

  for (j = 0; j < sizeof(body);
       j++) { /* incompressible: gzip can't shrink it */
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    body[j] = (unsigned char) (rng >> 24);
  }
  fconcat(path, sizeof(path), argv[0], "warc-rot.warc.gz");
  saved_max = opt->warc_max_size;
  opt->warc_max_size =
      1000; /* a couple records per segment => several segments */
  w = warc_open(opt, path);
  assertf(w != NULL);
  for (i = 0; i < 8; i++) {
    char uri[64];
    snprintf(uri, sizeof(uri), "http://test.local/f%d.bin", i);
    warc_write_transaction(
        w, uri, "127.0.0.1", "GET / HTTP/1.1\r\nHost: test.local\r\n\r\n",
        "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\n\r\n",
        warc_body_mem(hts_span_at((const char *) body, sizeof(body))), NULL,
        200, 0, 0);
  }
  warc_close(w);
  opt->warc_max_size = saved_max;

  for (i = 0;; i++) {
    char fname[64];
    unsigned char *data;
    size_t data_len = 0;
    const unsigned char *p, *pend;
    int first = 1;
    snprintf(fname, sizeof(fname), "warc-rot-%05d.warc.gz", i);
    fconcat(seg, sizeof(seg), argv[0], fname);
    data = (unsigned char *) st_slurp(seg, &data_len);
    if (data == NULL)
      break; /* past the last segment */
    nseg++;
    p = data;
    pend = data + data_len;
    while (p < pend) {
      size_t rlen = 0, hdr_len = 0;
      long long block_len = 0;
      unsigned char *rec = warc_next_member(&p, pend, &rlen);
      if (rec == NULL) {
        ST_FAIL_IF(err, rlen != 0);
        break;
      }
      ST_FAIL_IF(err, warc_rec_split(hts_span_at((const char *) rec, rlen),
                                     &hdr_len, &block_len) != 0);
      if (first) { /* each segment leads with its own warcinfo */
        ST_FAIL_IF(err, warc_after(hts_span_at((const char *) rec, hdr_len),
                                   "WARC-Type: warcinfo") == NULL);
        first = 0;
      }
      freet(rec);
    }
    freet(data);
    if (first) /* empty segment */
      err = 1;
  }
  ST_FAIL_IF(err, nseg < 2);
  printf("warc-rotate: %d segments: %s\n", nseg, err ? "FAIL" : "OK");
  return err;
}

/* The default body storage: assert the stored WARC record is byte-verbatim gzip
   with Content-Encoding preserved and Content-Length = the coded length. */
static int st_warc_verbatim(httrackp *opt, int argc, char **argv) {
  char path[HTS_URLMAXSIZE];
  warc_writer *w;
  unsigned char *data;
  size_t data_len = 0;
  const unsigned char *p, *end;
  int err = 0, checked = 0;
  static const char a_plain[] =
      "Strategy-A verbatim WARC body: the quick brown fox jumps.\n";
  static const unsigned char a_gz[] = {
      0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xff, 0x0b, 0x2e,
      0x29, 0x4a, 0x2c, 0x49, 0x4d, 0xaf, 0xd4, 0x75, 0x54, 0x28, 0x4b, 0x2d,
      0x4a, 0x4a, 0x2c, 0xc9, 0xcc, 0x55, 0x08, 0x77, 0x0c, 0x72, 0x56, 0x48,
      0xca, 0x4f, 0xa9, 0xb4, 0x52, 0x28, 0xc9, 0x48, 0x55, 0x28, 0x2c, 0xcd,
      0x4c, 0xce, 0x56, 0x48, 0x2a, 0xca, 0x2f, 0xcf, 0x53, 0x48, 0xcb, 0xaf,
      0x50, 0xc8, 0x2a, 0xcd, 0x2d, 0x28, 0xd6, 0xe3, 0x02, 0x00, 0x5e, 0xb8,
      0xe7, 0x66, 0x3a, 0x00, 0x00, 0x00};

  fconcat(path, sizeof(path), argv[0], "warc-verbatim.warc.gz");

  w = warc_open(opt, path);
  assertf(w != NULL);
  /* the body is the coded (gzip) octets, stored verbatim. */
  warc_write_transaction(
      w, "http://test.local/z.html", "127.0.0.1",
      "GET /z.html HTTP/1.1\r\nHost: test.local\r\n\r\n",
      "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Encoding: "
      "gzip\r\nTransfer-Encoding: chunked\r\nContent-Length: 999\r\n\r\n",
      warc_body_mem(hts_span_at((const char *) a_gz, sizeof(a_gz))), NULL, 200,
      0, 0);
  warc_close(w);

  data = (unsigned char *) st_slurp(path, &data_len);
  assertf(data != NULL);
  p = data;
  end = data + data_len;

  while (p < end) {
    size_t rlen = 0, hdr_len = 0;
    long long block_len = 0;
    unsigned char *rec = warc_next_member(&p, end, &rlen);
    if (rec == NULL) {
      ST_FAIL_IF(err, rlen != 0);
      break;
    }
    const hts_span whole = hts_span_at((const char *) rec, rlen);
    if (warc_after(whole, "msgtype=response") == NULL) {
      freet(rec);
      continue;
    }
    if (warc_rec_split(whole, &hdr_len, &block_len) != 0) {
      err = 1;
      freet(rec);
      continue;
    }
    /* Assert: one Content-Encoding, no Transfer-Encoding, Content-Length =
       compressed size. */
    {
      const char *block = (char *) rec + hdr_len;
      const char *ce = warc_after(hts_span_at(block, (size_t) block_len),
                                  "Content-Encoding:");
      const char *hcl =
          warc_after(hts_span_at(block, (size_t) block_len), "Content-Length:");
      long long http_cl = -1;
      int nce = 0;
      const char *scan = ce;
      while (scan != NULL) {
        size_t rem = (size_t) block_len - (size_t) (scan - block);
        nce++;
        scan = warc_after(hts_span_at(scan, rem), "Content-Encoding:");
      }
      ST_FAIL_IF(err, ce == NULL || hts_strncasecmp(ce, " gzip", 5) != 0 ||
                          nce != 1);
      ST_FAIL_IF(err, warc_after(hts_span_at(block, (size_t) block_len),
                                 "Transfer-Encoding:") != NULL);
      ST_FAIL_IF(err, hcl == NULL || sscanf(hcl, "%lld", &http_cl) != 1 ||
                          http_cl != (long long) sizeof(a_gz));
    }
    /* Stored block bytes equal the gzip input, and inflate to the plaintext. */
    {
      const char *bsep = warc_after(
          hts_span_at((const char *) rec + hdr_len, (size_t) block_len),
          "\r\n\r\n");
      if (bsep == NULL)
        err = 1;
      else {
        size_t bodyoff = (size_t) (bsep - (char *) rec);
        size_t got = rlen - 4 - bodyoff; /* minus the record trailer */
        if (got != sizeof(a_gz) ||
            memcmp(rec + bodyoff, a_gz, sizeof(a_gz)) != 0)
          err = 1;
        else {
          const unsigned char *bp = rec + bodyoff;
          size_t plen = 0;
          unsigned char *plain = warc_next_member(&bp, bp + got, &plen);
          ST_FAIL_IF(err, plain == NULL || plen != sizeof(a_plain) - 1 ||
                              memcmp(plain, a_plain, plen) != 0);
          freet(plain);
        }
      }
    }
    checked = 1;
    freet(rec);
  }
  freet(data);
  ST_FAIL_IF(err, !checked);
  return st_verdict("warc-verbatim", err);
}

/* SURT canonicalization vectors (the CDXJ sort key: www-strip, default-port
   strip, host reversal, non-default port kept, IP/IPv6 verbatim). */
static int st_warc_surt(httrackp *opt, int argc, char **argv) {
  static const struct {
    const char *url, *want;
  } cases[] = {
      {"http://www.example.com/", "com,example)/"},
      {"http://example.com:80/a/b?q=1", "com,example)/a/b?q=1"},
      {"https://www.EXAMPLE.com/Path", "com,example)/Path"},
      {"https://example.com:443/", "com,example)/"},
      {"http://www2.example.com/x", "com,example)/x"},
      {"http://example.com:8080/p", "com,example:8080)/p"},
      {"HTTP://h:80/p", "h)/p"},
      {"http://h:8x/p", "h)/p"},
      {"http://example.com:99999999999999999999/p",
       "com,example:99999999999999999999)/p"},
      {"http://example.com:4294967376/", "com,example:4294967376)/"},
      {"http://h:0080/p", "h)/p"},
      {"http://h:65536/p", "h:65536)/p"},
      {"http://h:+80/p", "h)/p"},
      {"http://user:pass@www.example.com/y", "com,example)/y"},
      {"http://192.168.0.1/z", "192.168.0.1)/z"},
      {"http://[2001:db8::1]/w", "[2001:db8::1])/w"},
      {"http://sub.a.example.co.uk/deep?x=1#frag",
       "uk,co,example,a,sub)/deep?x=1"},
      {"http://example.com/caf\xe9", "com,example)/caf%E9"},
      {"http://example.com/caf\xc3\xa9", "com,example)/caf\xc3\xa9"},
      {"http://example.com/d\xed\xa0\x80z", "com,example)/d%ED%A0%80z"},
      {"http://example.com/tr\xe2\x82", "com,example)/tr%E2%82"},
      {"http://ex\xe9.com/", "com,ex%E9)/"},
      {"http://user:pass@www.Example.com:80/a?q=1#f", "com,example)/a?q=1"},
      {"https://[2001:db8::1]:443/x", "[2001:db8::1])/x"},
      {"ftp://1.2.3.4:21/p", "1.2.3.4:21)/p"},
      {"http://[::FFFF:1.2.3.4]:8080/v", "[::ffff:1.2.3.4]:8080)/v"},
      {"https://www3.a.b:8443/c", "b,a:8443)/c"},
      /* These split as the URL view does, which changed their SURT. */
      {"http://h\\x@y/z", "h)\\x@y/z"},
      {"http://a\\b/c", "a)\\b/c"},
      {"http://[::1]x/p", "[::1]x)/p"},
      {"h/x?u=http://a/b", "h)/x?u=http://a/b"},
      {"http:h/x", "h)/x"},
  };

  int err = 0;
  size_t i;
  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    char out[512];
    if (!warc_surt(cases[i].url, HTS_BUF(out)) ||
        strcmp(out, cases[i].want) != 0) {
      fprintf(stderr, "warc-surt: %s -> %s (want %s)\n", cases[i].url, out,
              cases[i].want);
      err = 1;
    }
  }
  return st_verdict("warc-surt", err);
}

/* A 32-bit ftell, as on 64-bit Windows, misplaces every record past 2 GB. */
static int st_warc_offset(httrackp *opt, int argc, char **argv) {
  static const uint64_t cases[] = {
      0,
      2147483647ULL, /* LONG_MAX where long is 32 bits */
      2147483648ULL, /* first offset a 32-bit signed tell cannot hold */
      4294967295ULL,
      4294967296ULL,
      1099511627775ULL, /* 1TB - 1: a plausible archive on a big mirror */
  };
  char path[HTS_URLMAXSIZE];
  char catbuff[CATBUFF_SIZE];
  FILE *fp;
  int err = 0;
  size_t i;
  size_t wide = 0; /* offsets measured past what a 32-bit tell could hold */

  fconcat(path, sizeof(path), argv[0], "warc-offset.bin");
  fp = FOPEN(fconv(catbuff, sizeof(catbuff), path), "wb");
  if (fp == NULL) {
    fprintf(stderr, "warc-offset: cannot create '%s': %s\n", path,
            strerror(errno));
    return 1;
  }
  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    const uint64_t want = cases[i];
    uint64_t got;

    if (fseeko(fp, (LLint) want, SEEK_SET) != 0) {
      /* EFBIG: the filesystem caps file size below the offset (GNU/Hurd ext2fs)
       */
      if (errno == EFBIG) {
        fprintf(stderr,
                "warc-offset: %" PRIu64 " is past this filesystem's"
                " file-size cap, skipped\n",
                want);
        continue;
      }
      fprintf(stderr, "warc-offset: cannot seek to %" PRIu64 ": %s\n", want,
              strerror(errno));
      err = 1;
      continue;
    }
    /* the sentinel must never survive: a failed tell would leak it back */
    got = warc_stream_offset(fp, 0xdeadbeefULL);
    if (got != want) {
      fprintf(stderr, "warc-offset: tell at %" PRIu64 " reported %" PRIu64 "\n",
              want, got);
      err = 1;
    } else if (want > 2147483647ULL) {
      wide++;
    }
  }
  fclose(fp);
  /* fail here, or a filesystem refusing every wide offset proves nothing */
  if (wide == 0) {
    fprintf(stderr, "warc-offset: no offset past 2GB was measurable here\n");
    err = 1;
  }
  /* seeking never wrote: a several-GB file here would mean a real write */
  if (fsize_utf8(path) != 0) {
    fprintf(stderr, "warc-offset: the probe file is not empty (%" PRIu64 ")\n",
            (uint64_t) fsize_utf8(path));
    err = 1;
  }
  UNLINK(fconv(catbuff, sizeof(catbuff), path));

  /* The width has to survive all the way to the CDXJ text: 01_zlib-warc-cdx
     covers the wiring, but only at offsets a 32-bit field would also hold. */
  {
    static const struct {
      uint64_t length, offset;
      const char *want;
    } extents[] = {
        {0, 0, ", \"length\": \"0\", \"offset\": \"0\""},
        {4096, 4294967295ULL,
         ", \"length\": \"4096\", \"offset\": \"4294967295\""},
        {4294967296ULL, 4294967296ULL,
         ", \"length\": \"4294967296\", \"offset\": \"4294967296\""},
        {18446744073709551615ULL, 1099511627776ULL,
         ", \"length\": \"18446744073709551615\", \"offset\": "
         "\"1099511627776\""},
    };

    for (i = 0; i < sizeof(extents) / sizeof(extents[0]); i++) {
      char got[WARC_CDX_EXTENT_SIZE];

      got[0] = '\0';
      if (!warc_cdx_extent(HTS_BUF(got), extents[i].length,
                           extents[i].offset) ||
          strcmp(got, extents[i].want) != 0) {
        fprintf(stderr, "warc-offset: CDXJ extent is '%s', want '%s'\n", got,
                extents[i].want);
        err = 1;
      }
    }

    /* the truncation return, which no WARC_CDX_EXTENT_SIZE buffer reaches: the
       widest pair fits it, and a clipped extent must be refused, not indexed */
    {
      struct {
        char buf[24];
        char canary[8];
      } tight;

      char full[WARC_CDX_EXTENT_SIZE];

      memset(&tight, 'C', sizeof(tight));
      if (warc_cdx_extent(HTS_BUF(tight.buf), 4294967296ULL,
                          1099511627776ULL)) {
        fprintf(stderr, "warc-offset: a clipped CDXJ extent was not refused\n");
        err = 1;
      }
      if (memcmp(tight.canary, "CCCCCCCC", sizeof(tight.canary)) != 0) {
        fprintf(stderr, "warc-offset: the CDXJ extent overran its buffer\n");
        err = 1;
      }
      if (!warc_cdx_extent(HTS_BUF(full), UINT64_MAX, UINT64_MAX)) {
        fprintf(stderr,
                "warc-offset: the widest CDXJ extent does not fit %d bytes\n",
                (int) sizeof(full));
        err = 1;
      }
    }
  }
  return st_verdict("warc-offset", err);
}

/* A URL longer than 1024 bytes must still reach the archive (#785). */
static int st_warc_longurl(httrackp *opt, int argc, char **argv) {
  /* "WARC-Target-URI: " + CRLF costs 19 bytes, so the old buffer failed at
     1005; 9000 forces several reallocs within one record. */
  static const size_t lengths[] = {100, 1003, 1004, 1005, 1006, 2000, 9000};
  static const char resp_hdr[] =
      "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
  char path[HTS_URLMAXSIZE * 2];
  char body[64];
  warc_writer *w;
  char *blob;
  LLint fsz;
  const char *at2;
  size_t i, nrec = 0;
  int err = 0;

  snprintf(path, sizeof(path), "%s/longurl.warc", argv[0]);

  w = warc_open(opt, path);
  if (w == NULL) {
    fprintf(stderr, "warc-longurl: could not create %s\n", path);
    return 1;
  }
  for (i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
    const size_t len = lengths[i];
    char *uri = malloct(len + 1);

    if (uri == NULL) {
      warc_close(w);
      return 1;
    }
    /* A distinct tail per URI so a truncated one cannot match another. */
    snprintf(uri, len + 1, "http://example.com/%04d/", (int) len);
    memset(uri + strlen(uri), 'a', len - strlen(uri));
    uri[len] = '\0';
    /* Distinct payloads: identical ones dedupe into revisit records. */
    snprintf(body, sizeof(body), "<html><body>%04d</body></html>\n", (int) len);
    if (warc_write_transaction(w, uri, NULL, NULL, resp_hdr,
                               warc_body_mem(hts_span_at(body, strlen(body))),
                               NULL, 200, 0, 0) != 0) {
      fprintf(stderr, "warc-longurl: write failed at length %d\n", (int) len);
      err = 1;
    }
    freet(uri);
  }
  warc_close(w);

  blob = readfile2_utf8(path, &fsz);
  if (blob == NULL || fsz <= 0) {
    fprintf(stderr, "warc-longurl: no archive written\n");
    freet(blob);
    return 1;
  }

  for (i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
    const size_t len = lengths[i];
    char want[64];
    const char *at;

    snprintf(want, sizeof(want), "WARC-Target-URI: http://example.com/%04d/",
             (int) len);
    at = strstr(blob, want);
    if (at == NULL) {
      fprintf(stderr, "warc-longurl: length %d lost its record\n", (int) len);
      err = 1;
    } else if (strlen(at) < strlen("WARC-Target-URI: ") + len ||
               at[strlen("WARC-Target-URI: ") + len] != '\r') {
      fprintf(stderr, "warc-longurl: length %d truncated\n", (int) len);
      err = 1;
    }
  }
  for (at2 = blob; (at2 = strstr(at2, "WARC-Type: response")) != NULL; at2++)
    nrec++;
  if (nrec != sizeof(lengths) / sizeof(lengths[0])) {
    fprintf(stderr, "warc-longurl: %d response records, want %d\n", (int) nrec,
            (int) (sizeof(lengths) / sizeof(lengths[0])));
    err = 1;
  }

  freet(blob);
  return st_verdict("warc-longurl", err);
}

/* Checks that each sorted .cdx line inflates to the record it names. */
static int st_warc_cdx(httrackp *opt, int argc, char **argv) {
  char wpath[HTS_URLMAXSIZE], cpath[HTS_URLMAXSIZE];
  warc_writer *w;
  unsigned char *warc = NULL, *cdx = NULL;
  size_t warc_len = 0, cdx_len = 0;
  hts_boolean saved_cdx;
  int err = 0, nlines = 0, nm_lines = 0;
  const char *lp, *cend;
  char prev[2048];

  fconcat(wpath, sizeof(wpath), argv[0], "warc-cdx.warc.gz");
  fconcat(cpath, sizeof(cpath), argv[0], "warc-cdx.cdx");
  saved_cdx = opt->warc_cdx;
  opt->warc_cdx = 1;
  w = warc_open(opt, wpath);
  assertf(w != NULL);
  warc_write_transaction(w, "http://www.example.com/one", "127.0.0.1",
                         "GET /one HTTP/1.1\r\nHost: www.example.com\r\n\r\n",
                         "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n",
                         warc_body_mem(hts_span_at("one body\n", 9)), NULL, 200,
                         0, 0);
  warc_write_resource(w, "ftp://files.example.com/data.bin", "127.0.0.1",
                      "application/octet-stream",
                      warc_body_mem(hts_span_at("\x00\x01\x02\x03", 4)), 0);
  warc_write_transaction(w, "http://alpha.example.com/two", "127.0.0.1",
                         "GET /two HTTP/1.1\r\nHost: alpha.example.com\r\n\r\n",
                         "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n",
                         warc_body_mem(hts_span_at("two body\n", 9)), NULL, 200,
                         0, 0);
  /* Same payload as /one at a new URL: identical-payload-digest revisit under
     OpenSSL, a full response otherwise; either way one index line. */
  warc_write_transaction(w, "http://zeta.example.com/dup", "127.0.0.1",
                         "GET /dup HTTP/1.1\r\nHost: zeta.example.com\r\n\r\n",
                         "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n",
                         warc_body_mem(hts_span_at("one body\n", 9)), NULL, 200,
                         0, 0);
  /* A 304 declares no type, so the index line takes the caller's (#826). */
  warc_write_transaction(w, "http://nm.example.com/kept", "127.0.0.1",
                         "GET /kept HTTP/1.1\r\nHost: nm.example.com\r\n"
                         "If-Modified-Since: Mon, 01 Jan 2024 00:00:00 GMT\r\n"
                         "\r\n",
                         "HTTP/1.1 304 Not Modified\r\n\r\n",
                         warc_body_mem(hts_span_at(NULL, 0)), "text/html", 200,
                         1, 0);
  warc_close(w);
  opt->warc_cdx = saved_cdx;

  warc = (unsigned char *) st_slurp(wpath, &warc_len);
  cdx = (unsigned char *) st_slurp(cpath, &cdx_len);
  assertf(warc != NULL);
  assertf(cdx != NULL);

  prev[0] = '\0';
  lp = (const char *) cdx;
  cend = (const char *) cdx + cdx_len;
  while (lp < cend) {
    const char *eol = memchr(lp, '\n', (size_t) (cend - lp));
    size_t llen = eol ? (size_t) (eol - lp) : (size_t) (cend - lp);
    char line[2048];
    const char *j, *us, *ue, *o, *l;
    char url[1024];
    unsigned long long off = 0, len = 0;
    const unsigned char *mp, *mend;
    unsigned char *rec;
    size_t rlen = 0, urllen;
    if (llen == 0) {
      lp = eol ? eol + 1 : cend;
      continue;
    }
    if (llen >= sizeof(line)) {
      err = 1;
      break;
    }
    memcpy(line, lp, llen);
    line[llen] = '\0';
    nlines++;
    /* must be sorted */
    ST_FAIL_IF(err, prev[0] != '\0' && strcmp(prev, line) > 0);
    strlcpybuff(prev, line, sizeof(prev));
    j = warc_after(hts_span_of(line), "\"url\": \"");
    o = warc_after(hts_span_of(line), "\"offset\": \"");
    l = warc_after(hts_span_of(line), "\"length\": \"");
    if (j == NULL || o == NULL || l == NULL || sscanf(o, "%llu", &off) != 1 ||
        sscanf(l, "%llu", &len) != 1) {
      err = 1;
      goto nextline;
    }
    us = j;
    ue = strchr(us, '"');
    if (ue == NULL || (urllen = (size_t) (ue - us)) >= sizeof(url)) {
      err = 1;
      goto nextline;
    }
    memcpy(url, us, urllen);
    url[urllen] = '\0';
    if (strstr(url, "nm.example.com") != NULL) {
      nm_lines++;
      ST_FAIL_IF(err, strstr(line, "\"mime\": \"text/html\"") == NULL);
    }
    if (len == 0 || off > warc_len || len > warc_len - off) {
      err = 1;
      goto nextline;
    }
    mp = warc + off;
    mend = warc + off + len;
    rec = warc_next_member(&mp, mend, &rlen);
    if (rec == NULL) {
      err = 1;
      goto nextline;
    }
    {
      char needle[1100];
      snprintf(needle, sizeof(needle), "WARC-Target-URI: %s\r\n", url);
      ST_FAIL_IF(err, warc_after(hts_span_at((const char *) rec, rlen),
                                 needle) == NULL);
    }
    freet(rec);
  nextline:
    lp = eol ? eol + 1 : cend;
  }
  freet(warc);
  freet(cdx);
  /* 4 responses/revisits + 1 resource; no warcinfo/request */
  ST_FAIL_IF(err, nlines != 5 || nm_lines != 1);
  printf("warc-cdx: %d index lines: %s\n", nlines, err ? "FAIL" : "OK");
  return err;
}

/* A URL carrying a raw Latin-1 byte still gives a CDXJ line whose JSON is
   valid UTF-8, while a well-formed UTF-8 URL passes through unchanged. */
static int st_warc_cdx_utf8(httrackp *opt, int argc, char **argv) {
  static const char *const urls[] = {"http://www.example.com/caf\xe9",
                                     "http://www.example.com/caf\xc3\xa9"};
  char wpath[HTS_URLMAXSIZE], cpath[HTS_URLMAXSIZE];
  warc_writer *w;
  unsigned char *cdx;
  size_t cdx_len = 0, i;
  hts_boolean saved_cdx;
  int err = 0, nlines = 0, nescaped = 0, nverbatim = 0;
  const char *lp, *cend;

  fconcat(wpath, sizeof(wpath), argv[0], "warc-cdx-utf8.warc.gz");
  fconcat(cpath, sizeof(cpath), argv[0], "warc-cdx-utf8.cdx");
  saved_cdx = opt->warc_cdx;
  opt->warc_cdx = HTS_TRUE;
  w = warc_open(opt, wpath);
  assertf(w != NULL);
  for (i = 0; i < sizeof(urls) / sizeof(urls[0]); i++)
    warc_write_transaction(
        w, urls[i], "127.0.0.1",
        "GET /caf HTTP/1.1\r\nHost: www.example.com\r\n\r\n",
        "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n",
        warc_body_mem(
            hts_span_at(i == 0 ? "first\n" : "second\n", i == 0 ? 6 : 7)),
        NULL, 200, 0, 0);
  warc_close(w);
  opt->warc_cdx = saved_cdx;

  cdx = (unsigned char *) st_slurp(cpath, &cdx_len);
  assertf(cdx != NULL);
  lp = (const char *) cdx;
  cend = (const char *) cdx + cdx_len;
  while (lp < cend) {
    const char *eol = memchr(lp, '\n', (size_t) (cend - lp));
    const char *end = eol != NULL ? eol : cend;
    const char *json = memchr(lp, '{', (size_t) (end - lp));

    if (end != lp) {
      nlines++;
      if (json == NULL || !hts_isStringUTF8(hts_span_between(lp, end))) {
        fprintf(stderr, "warc-cdx-utf8: line %d is not UTF-8\n", nlines);
        err = 1;
      } else if (warc_after(hts_span_at(json, (size_t) (end - json)),
                            "/caf\\ufffd\"") != NULL) {
        nescaped++;
      } else if (warc_after(hts_span_at(json, (size_t) (end - json)),
                            "/caf\xc3\xa9\"") != NULL) {
        nverbatim++;
      }
    }
    lp = end < cend ? end + 1 : cend;
  }
  freet(cdx);
  if (nlines != 2 || nescaped != 1 || nverbatim != 1) {
    fprintf(stderr, "warc-cdx-utf8: %d lines, %d escaped, %d verbatim\n",
            nlines, nescaped, nverbatim);
    err = 1;
  }
  return st_verdict("warc-cdx-utf8", err);
}

static char st_cdx_log[2048];

/* Collect the "WARC:" diagnostics only, so an unrelated message cannot satisfy
   (or break) a silence assertion. */
static HTS_PRINTF_FUN(3, 0) void st_cdx_log_cb(httrackp *opt, int type,
                                               const char *format,
                                               va_list args) {
  char line[512];
  (void) type;
  (void) vsnprintf(line, sizeof(line), format, args);
  if (strncmp(line, "WARC:", 5) == 0)
    strlncatbuff(st_cdx_log, line, sizeof(st_cdx_log),
                 sizeof(st_cdx_log) - strlen(st_cdx_log) - 1);
}

/* 1 on mismatch; want == NULL asks for silence. Resets the capture. */
static int st_cdx_logged(const char *name, const char *want) {
  const int bad =
      want != NULL ? strstr(st_cdx_log, want) == NULL : st_cdx_log[0] != '\0';
  if (bad)
    fprintf(stderr, "warc-cdx-errors: %s logged \"%s\", want \"%s\"\n", name,
            st_cdx_log, want != NULL ? want : "(nothing)");
  st_cdx_log[0] = '\0';
  return bad;
}

/* Leave a previous run's file behind for this one to find. */
static void st_cdx_leave(const char *path, const char *content) {
  FILE *const fp = FOPEN(path, "wb");

  assertf(fp != NULL);
  fputs(content, fp);
  fclose(fp);
}

/* Feed n index lines whose URLs are `pad` characters long. */
static void st_cdx_fill(warc_writer *w, int n, size_t pad) {
  char url[1024], req[1100];
  int i;

  for (i = 0; i < n; i++) {
    size_t l =
        (size_t) snprintf(url, sizeof(url), "http://e%d.example.com/", i);

    while (l + 1 < sizeof(url) && l < pad)
      url[l++] = 'p';
    url[l] = '\0';
    snprintf(req, sizeof(req),
             "GET / HTTP/1.1\r\nHost: e%d.example.com\r\n\r\n", i);
    warc_write_transaction(w, url, "127.0.0.1", req,
                           "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n",
                           warc_body_mem(hts_span_at("body\n", 5)), NULL, 200,
                           0, 0);
  }
}

/* These are the ways warc_cdx_flush can fail to leave a usable index. A
   first run has no old index, so the empty-index warning is quiet (#1041). */
static int st_warc_cdx_errors(httrackp *opt, int argc, char **argv) {
  char path[HTS_URLMAXSIZE], cdx[HTS_URLMAXSIZE];
  static const char stale[] = "no record was indexed";
  static const char unwritable[] = "could not write the index";
  hts_boolean saved_cdx;
  void *saved_state;
  warc_writer *w;
  int err = 0;

  saved_state = opt->state.warc;
  saved_cdx = opt->warc_cdx;
  opt->warc_cdx = 1;
  st_cdx_log[0] = '\0';
  hts_set_log_vprint_callback(st_cdx_log_cb);

  /* No previous archive and nothing indexed: nothing on disk went stale. */
  fconcat(path, sizeof(path), argv[0], "cdxerr-first.warc.gz");
  w = warc_open(opt, path);
  assertf(w != NULL);
  warc_close(w);
  err |= st_cdx_logged("first run", NULL);

  /* This run replaced an archive, so the old index is stale. */
  fconcat(path, sizeof(path), argv[0], "cdxerr-prev.warc.gz");
  fconcat(cdx, sizeof(cdx), argv[0], "cdxerr-prev.cdx");
  st_cdx_leave(path, "previous archive");
  st_cdx_leave(cdx, "com,example)/ 20240101000000 {}\n");
  w = warc_open(opt, path);
  assertf(w != NULL);
  warc_close(w);
  err |= st_cdx_logged("swap over a previous archive", stale);

  /* The mirror case clobbers the archive in place, so its index is stale. */
  fconcat(path, sizeof(path), argv[0], "cdxerr-abort.warc.gz");
  fconcat(cdx, sizeof(cdx), argv[0], "cdxerr-abort.cdx");
  st_cdx_leave(cdx, "com,example)/ 20240101000000 {}\n");
  w = warc_open(opt, path);
  assertf(w != NULL);
  opt->state.warc = w;
  warc_abort_opt(opt);
  err |= st_cdx_logged("abandoned in-place run", stale);

  /* With no index left behind, no message may name it. */
  fconcat(path, sizeof(path), argv[0], "cdxerr-abort-noidx.warc.gz");
  w = warc_open(opt, path);
  assertf(w != NULL);
  opt->state.warc = w;
  warc_abort_opt(opt);
  err |= st_cdx_logged("abandoned run with no index", NULL);

  /* An archive that never opened leaves its index valid. */
  fconcat(path, sizeof(path), argv[0], "cdxerr-noopen.warc.gz");
  fconcat(cdx, sizeof(cdx), argv[0], "cdxerr-noopen.cdx");
  st_cdx_leave(cdx, "com,example)/ 20240101000000 {}\n");
  if (MKDIR(path) != 0 && errno != EEXIST) {
    fprintf(stderr, "warc-cdx-errors: mkdir %s failed: %s\n", path,
            strerror(errno));
    err = 1;
  } else {
    assertf(warc_open(opt, path) == NULL);
    err |= st_cdx_logged("archive that never opened", NULL);
  }

  /* The index path cannot be opened at all. */
  fconcat(path, sizeof(path), argv[0], "cdxerr-fopen.warc.gz");
  fconcat(cdx, sizeof(cdx), argv[0], "cdxerr-fopen.cdx");
  if (MKDIR(cdx) != 0 && errno != EEXIST) {
    fprintf(stderr, "warc-cdx-errors: mkdir %s failed: %s\n", cdx,
            strerror(errno));
    err = 1;
  } else {
    w = warc_open(opt, path);
    assertf(w != NULL);
    st_cdx_fill(w, 1, 0);
    warc_close(w);
    err |= st_cdx_logged("unopenable index path", unwritable);
  }

#ifndef _WIN32
  /* /dev/full fails a small index at fclose and a large one earlier. */
  if (access("/dev/full", W_OK) == 0) {
    fconcat(path, sizeof(path), argv[0], "cdxerr-short.warc.gz");
    fconcat(cdx, sizeof(cdx), argv[0], "cdxerr-short.cdx");
    (void) UNLINK(cdx);
    if (symlink("/dev/full", cdx) != 0) {
      fprintf(stderr, "warc-cdx-errors: symlink %s failed: %s\n", cdx,
              strerror(errno));
      err = 1;
    } else {
      w = warc_open(opt, path);
      assertf(w != NULL);
      st_cdx_fill(w, 1, 0);
      warc_close(w);
      err |= st_cdx_logged("index lost at fclose", unwritable);

      fconcat(path, sizeof(path), argv[0], "cdxerr-partial.warc.gz");
      fconcat(cdx, sizeof(cdx), argv[0], "cdxerr-partial.cdx");
      (void) UNLINK(cdx);
      if (symlink("/dev/full", cdx) != 0) {
        fprintf(stderr, "warc-cdx-errors: symlink %s failed: %s\n", cdx,
                strerror(errno));
        err = 1;
      } else {
        w = warc_open(opt, path);
        assertf(w != NULL);
        st_cdx_fill(w, 64, 512); /* well over any stdio buffer */
        warc_close(w);
        err |= st_cdx_logged("index truncated mid-write", "is incomplete");
      }
    }
  } else {
    printf("warc-cdx-errors: no /dev/full, skipping the write-failure cases\n");
  }
#endif

  hts_set_log_vprint_callback(NULL);
  opt->warc_cdx = saved_cdx;
  opt->state.warc = saved_state;
  return st_verdict("warc-cdx-errors", err);
}

/* One finished transaction through the engine hook. No stashed headers, so the
   hook synthesizes the status line the real caller supplies. */
static void st_warc_emit(httrackp *opt, const char *body) {
  lien_back *back = calloct(1, sizeof(lien_back));

  assertf(back != NULL);
  strcpybuff(back->url_adr, "example.com");
  strcpybuff(back->url_fil, "/teardown.html");
  back->r.statuscode = 200;
  strcpybuff(back->r.msg, "OK");
  strcpybuff(back->r.contenttype, "text/html");
  back->r.adr = strdupt(body);
  back->r.size = (LLint) strlen(body);
  warc_write_backtransaction(opt, back);
  freet(back->r.adr);
  freet(back);
}

/* The archive on disk must hold `want`'s run, and no trace of the others. */
static int st_warc_holds(const char *name, const char *path, const char *want,
                         const char *absent1, const char *absent2) {
  const char *const absent[2] = {absent1, absent2};
  size_t len = 0, i;
  char *blob = st_slurp(path, &len);
  int err = 0;

  if (blob == NULL) {
    fprintf(stderr, "warc-teardown: %s: cannot read %s\n", name, path);
    return 1;
  }
  /* this checks only the first record's version line */
  if (len < 8 || memcmp(blob, "WARC/1.1", 8) != 0) {
    fprintf(stderr, "warc-teardown: %s: %s does not open on a WARC record\n",
            name, path);
    err++;
  }
  if (warc_after(hts_span_at(blob, len), want) == NULL) {
    fprintf(stderr, "warc-teardown: %s: %s lost the run holding \"%s\"\n", name,
            path, want);
    err++;
  }
  for (i = 0; i < 2; i++) {
    if (warc_after(hts_span_at(blob, len), absent[i]) != NULL) {
      fprintf(stderr, "warc-teardown: %s: %s took \"%s\"\n", name, path,
              absent[i]);
      err++;
    }
  }
  freet(blob);
  return err;
}

/* One archive, three writes: two runs, then the emit teardown makes. */
static int st_warc_teardown_case(httrackp *opt, const char *name,
                                 const char *path, hts_boolean abort_run) {
  static const char run1[] = "teardown-run-1-body";
  static const char run2[] = "teardown-run-2-body-longer";
  static const char late[] = "teardown-late-body";
  char tmp[HTS_URLMAXSIZE * 2 + 8];
  char catbuff[CATBUFF_SIZE];
  int err = 0;

  snprintf(tmp, sizeof(tmp), "%s.tmp", path);
  (void) UNLINK(fconv(catbuff, sizeof(catbuff), path));
  (void) UNLINK(fconv(catbuff, sizeof(catbuff), tmp));
  StringCopy(opt->warc_file, path);

  /* the dispatcher's opt is reused, so reset it as hts_create_opt() would */
  opt->state.warc = NULL;
  st_warc_emit(opt, run1);
  warc_close_opt(opt);
  if (fsize_utf8(path) <= 0) {
    fprintf(stderr, "warc-teardown: %s: the hook opened no archive\n", name);
    return 1;
  }

  /* a previous archive exists, so this run writes the temporary */
  opt->state.warc = NULL;
  st_warc_emit(opt, run2);
  if (!fexist_utf8(tmp)) {
    fprintf(stderr, "warc-teardown: %s: no temporary beside %s\n", name, path);
    return 1;
  }
  if (abort_run)
    warc_abort_opt(opt);
  else
    warc_close_opt(opt);

  st_warc_emit(opt, late); /* what back_finalize emits during teardown */
  if (fexist_utf8(tmp)) {
    fprintf(stderr, "warc-teardown: %s: orphan temporary %s\n", name, tmp);
    err++;
  }
  /* abort keeps run 1, close commits run 2, and the late body catches an
     append */
  err += abort_run ? st_warc_holds(name, path, run1, run2, late)
                   : st_warc_holds(name, path, run2, run1, late);
  return err;
}

/* The hook reads an on-disk body at its file size, neither short nor long. */
static int st_warc_disk_case(httrackp *opt, const char *dir) {
  static const char body[] = "warc-disk-body-END";
  lien_back *back = calloct(1, sizeof(lien_back));
  char path[HTS_URLMAXSIZE];
  char catbuff[CATBUFF_SIZE];
  char *blob;
  size_t len = 0;
  FILE *fp;
  int err = 0;

  assertf(back != NULL);
  fconcat(path, sizeof(path), dir, "teardown-disk.warc");
  (void) UNLINK(fconv(catbuff, sizeof(catbuff), path));
  StringCopy(opt->warc_file, path);
  fconcat(back->url_sav, sizeof(back->url_sav), dir, "teardown-disk.html");
  fp = FOPEN(fconv(catbuff, sizeof(catbuff), back->url_sav), "wb");
  assertf(fp != NULL);
  assertf(fwrite(body, 1, sizeof(body) - 1, fp) == sizeof(body) - 1);
  fclose(fp);
  strcpybuff(back->url_adr, "example.com");
  strcpybuff(back->url_fil, "/disk.html");
  back->r.statuscode = 200;
  strcpybuff(back->r.msg, "OK");
  strcpybuff(back->r.contenttype, "text/html");
  back->r.is_write = 1;
  opt->state.warc = NULL;
  warc_write_backtransaction(opt, back);
  warc_close_opt(opt);
  freet(back);
  blob = st_slurp(path, &len);
  if (blob == NULL || warc_after(hts_span_at(blob, len),
                                 "warc-disk-body-END\r\n\r\n") == NULL) {
    fprintf(stderr, "warc-teardown: disk: %s lacks the whole on-disk body\n",
            path);
    err++;
  }
  freet(blob);
  return err;
}

// -#test=warc-teardown <dir>: teardown finalizes the slots left in flight, so
// back_finalize emits after warc_close_opt/warc_abort_opt has run (#1060).
static int st_warc_teardown(httrackp *opt, int argc, char **argv) {
  char path[HTS_URLMAXSIZE], saved_file[HTS_URLMAXSIZE];
  void *saved_state;
  int err = 0;

  saved_state = opt->state.warc;
  strlcpybuff(saved_file, StringBuff(opt->warc_file), sizeof(saved_file));

  fconcat(path, sizeof(path), argv[0], "teardown-close.warc");
  err += st_warc_teardown_case(opt, "close", path, HTS_FALSE);
  fconcat(path, sizeof(path), argv[0], "teardown-abort.warc");
  err += st_warc_teardown_case(opt, "abort", path, HTS_TRUE);
  err += st_warc_disk_case(opt, argv[0]);

  StringCopy(opt->warc_file, saved_file);
  opt->state.warc = saved_state;
  return st_verdict("warc-teardown", err);
}

#if HTS_USEOPENSSL
/* Lowercase-hex SHA-256 of DATA into OUT, of at least 65 bytes; 1 on success.
 */
static int wacz_test_sha256(hts_span data, hts_buf out) {
  EVP_MD_CTX *c = EVP_MD_CTX_new();
  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned int mdlen = 0, i;
  static const char hx[] = "0123456789abcdef";
  int ok;
  assertf(out.size >= 65);
  if (c == NULL)
    return 0;
  ok = EVP_DigestInit_ex(c, EVP_sha256(), NULL) == 1 &&
       (data.len == 0 || EVP_DigestUpdate(c, data.p, data.len) == 1) &&
       EVP_DigestFinal_ex(c, md, &mdlen) == 1 && mdlen == 32;
  EVP_MD_CTX_free(c);
  if (!ok)
    return 0;
  for (i = 0; i < 32; i++) {
    out.p[i * 2] = hx[md[i] >> 4];
    out.p[i * 2 + 1] = hx[md[i] & 0x0F];
  }
  out.p[64] = '\0';
  return 1;
}

/* One unzipped WACZ member: name, raw bytes, and the ZIP compression method. */
typedef struct {
  char name[256];
  unsigned char *data;
  size_t len;
  int method;
} wacz_entry;

/* Package a 2-record WARC as a WACZ, then unzip it in-process and check it. */
static int st_warc_wacz(httrackp *opt, int argc, char **argv) {
  char wpath[HTS_URLMAXSIZE], waczpath[HTS_URLMAXSIZE], cdxpath[HTS_URLMAXSIZE];
  warc_writer *w;
  hts_boolean saved_cdx, saved_wacz;
  wacz_entry ent[16];
  int nent = 0, err = 0, i;
  unzFile uf;
  const wacz_entry *dp = NULL, *dig = NULL, *pages = NULL;
  int have_archive = 0, have_index = 0, all_store = 1;
  LLint good_size;

  fconcat(wpath, sizeof(wpath), argv[0], "warc-wacz.warc.gz");
  fconcat(waczpath, sizeof(waczpath), argv[0], "warc-wacz.wacz");
  fconcat(cdxpath, sizeof(cdxpath), argv[0], "warc-wacz.cdx");
  saved_cdx = opt->warc_cdx;
  saved_wacz = opt->warc_wacz;
  opt->warc_cdx = 1;
  opt->warc_wacz = 1;
  w = warc_open(opt, wpath);
  assertf(w != NULL);
  warc_write_transaction(w, "http://www.example.com/", "127.0.0.1",
                         "GET / HTTP/1.1\r\nHost: www.example.com\r\n\r\n",
                         "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n",
                         warc_body_mem(hts_span_at("<html>home</html>\n", 18)),
                         NULL, 200, 0, 0);
  warc_write_transaction(
      w, "http://www.example.com/data.bin", "127.0.0.1",
      "GET /data.bin HTTP/1.1\r\nHost: www.example.com\r\n\r\n",
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/octet-stream\r\n\r\n",
      warc_body_mem(hts_span_at("\x00\x01\x02\x03\x04", 5)), NULL, 200, 0, 0);
  warc_close(w);

  /* Unzip every member in-process. */
  uf = hts_unzOpen_utf8(waczpath);
  assertf(uf != NULL);
  if (unzGoToFirstFile(uf) == UNZ_OK) {
    do {
      unz_file_info info;
      wacz_entry *e;
      if (nent >= (int) (sizeof(ent) / sizeof(ent[0]))) {
        err = 1;
        break;
      }
      e = &ent[nent];
      if (unzGetCurrentFileInfo(uf, &info, e->name, sizeof(e->name), NULL, 0,
                                NULL, 0) != UNZ_OK) {
        err = 1;
        break;
      }
      e->method = (int) info.compression_method;
      e->len = (size_t) info.uncompressed_size;
      e->data = malloct(e->len ? e->len : 1);
      if (e->data == NULL || unzOpenCurrentFile(uf) != UNZ_OK) {
        freet(e->data);
        err = 1;
        break;
      }
      ST_FAIL_IF(err, e->len > 0 &&
                          unzReadCurrentFile(uf, e->data, (unsigned) e->len) !=
                              (int) e->len);
      unzCloseCurrentFile(uf);
      nent++;
    } while (unzGoToNextFile(uf) == UNZ_OK);
  }
  unzClose(uf);

  /* Classify members and assert STORE mode (WACZ spec requirement). */
  for (i = 0; i < nent; i++) {
    const wacz_entry *e = &ent[i];
    if (e->method != 0)
      all_store = 0;
    if (strncmp(e->name, "archive/", 8) == 0)
      have_archive = 1;
    else if (strcmp(e->name, "indexes/index.cdx") == 0)
      have_index = 1;
    else if (strcmp(e->name, "pages/pages.jsonl") == 0)
      pages = e;
    else if (strcmp(e->name, "datapackage.json") == 0)
      dp = e;
    else if (strcmp(e->name, "datapackage-digest.json") == 0)
      dig = e;
  }
  ST_FAIL_IF(err, !have_archive || !have_index || pages == NULL || dp == NULL ||
                      dig == NULL || !all_store);

  /* pages.jsonl: header line, then >= 1 body row carrying url + ts. */
  if (pages != NULL) {
    if (pages->len < 27 ||
        memcmp(pages->data, "{\"format\": \"json-pages-1.0\"", 27) != 0)
      err = 1;
    else {
      const char *nl = memchr(pages->data, '\n', pages->len);
      const char *body = nl ? nl + 1 : NULL;
      size_t blen =
          body ? pages->len - (size_t) (body - (char *) pages->data) : 0;
      ST_FAIL_IF(err,
                 body == NULL || blen == 0 ||
                     warc_after(hts_span_at(body, blen), "\"url\": ") == NULL ||
                     warc_after(hts_span_at(body, blen), "\"ts\": ") == NULL);
    }
  }

  /* Every datapackage resource hash recomputes from the stored member bytes. */
  if (dp != NULL) {
    char *json = malloct(dp->len + 1);
    if (json == NULL) {
      err = 1;
    } else {
      const char *p;
      memcpy(json, dp->data, dp->len);
      json[dp->len] = '\0';
      ST_FAIL_IF(err, strstr(json, "\"profile\": \"data-package\"") == NULL ||
                          strstr(json, "\"wacz_version\": \"") == NULL);
      p = json;
      while ((p = warc_after(hts_span_of(p), "\"path\": \"")) != NULL) {
        char path[256], want[80], got[65];
        const char *pe, *h;
        size_t plen;
        pe = strchr(p, '"');
        if (pe == NULL || (plen = (size_t) (pe - p)) >= sizeof(path)) {
          err = 1;
          break;
        }
        memcpy(path, p, plen);
        path[plen] = '\0';
        h = warc_after(hts_span_of(pe), "\"hash\": \"sha256:");
        if (h == NULL || sscanf(h, "%79[0-9a-f]", want) != 1) {
          err = 1;
          break;
        }
        for (i = 0; i < nent; i++)
          if (strcmp(ent[i].name, path) == 0)
            break;
        ST_FAIL_IF(err,
                   i == nent ||
                       !wacz_test_sha256(
                           hts_span_at((const char *) ent[i].data, ent[i].len),
                           HTS_BUF(got)) ||
                       strcmp(got, want) != 0);
        if (i < nent) {
          const char *bytes = warc_after(hts_span_of(pe), "\"bytes\": ");
          ST_FAIL_IF(err, bytes == NULL || strtoull(bytes, NULL, 10) !=
                                               (unsigned long long) ent[i].len);
        }
        p = pe;
      }
      freet(json);
    }
  }

  /* datapackage-digest.json chains sha256(datapackage.json). */
  if (dp != NULL && dig != NULL) {
    char dphex[65], *djson = malloct(dig->len + 1);
    const char *h;
    char want[80];
    if (djson == NULL ||
        !wacz_test_sha256(hts_span_at((const char *) dp->data, dp->len),
                          HTS_BUF(dphex))) {
      err = 1;
    } else {
      memcpy(djson, dig->data, dig->len);
      djson[dig->len] = '\0';
      ST_FAIL_IF(err, strstr(djson, "\"path\": \"datapackage.json\"") == NULL);
      h = warc_after(hts_span_of(djson), "\"hash\": \"sha256:");
      ST_FAIL_IF(err, h == NULL || sscanf(h, "%79[0-9a-f]", want) != 1 ||
                          strcmp(want, dphex) != 0);
    }
    freet(djson);
  }

  for (i = 0; i < nent; i++)
    freet(ent[i].data);

  /* #522-class: a failed re-package must leave the existing .wacz untouched.
     Drop the .cdx and re-run empty so packaging fails on the missing index. */
  good_size = fsize(waczpath);
  ST_FAIL_IF(err, good_size <= 0);
  (void) UNLINK(cdxpath);
  w = warc_open(opt, wpath);
  assertf(w != NULL);
  warc_close(w);
  if (fsize(waczpath) != good_size) /* destroyed or rewritten = data loss */
    err = 1;

  opt->warc_cdx = saved_cdx;
  opt->warc_wacz = saved_wacz;
  printf("warc-wacz: %d members (store=%d): %s\n", nent, all_store,
         err ? "FAIL" : "OK");
  return err;
}
#endif

/* ------------------------------------------------------------ */
/* --single-file                                                 */
/* ------------------------------------------------------------ */

static int sf_err = 0;

/* Set when the filesystem accepted a ':' in a name; Windows never does. */
static hts_boolean sf_colon_ok = HTS_FALSE;

static void sf_check(int ok, const char *what) {
  if (!ok) {
    fprintf(stderr, "singlefile: %s\n", what);
    sf_err++;
  }
}

/* Write rel (a '/'-separated path under dir), creating the directories.
   Returns HTS_FALSE if the name is one the filesystem will not take. */
static hts_boolean sf_try_put(const char *dir, const char *rel, hts_span data) {
  char BIGSTK path[HTS_URLMAXSIZE * 2];
  char catbuff[CATBUFF_SIZE];
  FILE *fp;

  fconcat(path, sizeof(path), dir, rel);
  structcheck_utf8(path);
  fp = FOPEN(fconv(catbuff, sizeof(catbuff), path), "wb");
  if (fp == NULL)
    return HTS_FALSE;
  assertf(hts_fwrite_span(data, fp));
  fclose(fp);
  return HTS_TRUE;
}

/* Expand a fixture: \001<ref>\002 becomes <ref> plus this run's mark for it.
   The secret is random per run, so a fixture cannot spell a mark itself. */
static void sf_expand_fixture(httrackp *opt, hts_span in, String *out) {
  size_t i, start = 0;

  StringClear(*out);
  for (i = 0; i < in.len; i++) {
    if (in.p[i] == '\001') {
      start = StringLength(*out);
    } else if (in.p[i] == '\002') {
      char mark[SINGLEFILE_MARK_MAX];

      StringCat(*out, singlefile_mark(opt, HTS_BUF(mark), SINGLEFILE_CLASS_ANY,
                                      StringLength(*out) - start));
    } else {
      StringAddchar(*out, in.p[i]);
    }
  }
}

static void sf_put(const char *dir, const char *rel, hts_span data) {
  assertf(sf_try_put(dir, rel, data));
}

/* sf_put() for a text fixture, expanding its \001<ref>\002 delimiters. Binary
   fixtures must not go through here: sf_png carries those very bytes. */
static size_t sf_put_marked(httrackp *opt, const char *dir, const char *rel,
                            hts_span text) {
  String body = STRING_EMPTY;
  size_t written;

  sf_expand_fixture(opt, text, &body);
  assertf(
      sf_try_put(dir, rel, hts_span_at(StringBuff(body), StringLength(body))));
  written = StringLength(body);
  StringFree(body);
  return written;
}

/* Number of times needle occurs in hay. */
static int sf_count(const char *hay, const char *needle) {
  const size_t l = strlen(needle);
  int n = 0;
  const char *p = hay;

  while ((p = strstr(p, needle)) != NULL) {
    n++;
    p += l;
  }
  return n;
}

/* The base64 payload following the first occurrence of prefix, up to the first
   byte outside the base64 alphabet. NULL if prefix is absent. */
static const char *sf_payload(const char *hay, const char *prefix,
                              size_t *len) {
  const char *p = strstr(hay, prefix);
  size_t n = 0;

  if (p == NULL)
    return NULL;
  p += strlen(prefix);
  while (p[n] != '\0' && (hts_isalnum((unsigned char) p[n]) || p[n] == '+' ||
                          p[n] == '/' || p[n] == '='))
    n++;
  *len = n;
  return p;
}

/* Independent base64 decoder: the round-trip check must not lean on code64().
 */
static unsigned char *sf_unb64(hts_span s, size_t *outlen) {
  static const char alpha[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  unsigned char *out = (unsigned char *) malloct(s.len / 4 * 3 + 4);
  unsigned int acc = 0;
  size_t i, n = 0;
  int bits = 0;

  if (out == NULL)
    return NULL;
  for (i = 0; i < s.len; i++) {
    const char *const p = s.p[i] != '\0' ? strchr(alpha, s.p[i]) : NULL;

    if (s.p[i] == '=')
      break;
    if (p == NULL) {
      freet(out);
      return NULL;
    }
    acc = (acc << 6) | (unsigned int) (p - alpha);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out[n++] = (unsigned char) ((acc >> bits) & 0xff);
    }
  }
  *outlen = n;
  return out;
}

/* Decoded payload of the first data: URI with that MIME, as a NUL-terminated
   buffer the caller freet()s. NULL when absent or undecodable. */
static char *sf_decode(const char *hay, const char *mime, size_t *outlen) {
  char prefix[128];
  size_t len = 0, dlen = 0;
  const char *b64;
  unsigned char *raw;

  snprintf(prefix, sizeof(prefix), "data:%s;base64,", mime);
  b64 = sf_payload(hay, prefix, &len);
  if (b64 == NULL)
    return NULL;
  raw = sf_unb64(hts_span_at(b64, len), &dlen);
  if (raw == NULL)
    return NULL;
  raw[dlen] = '\0';
  if (outlen != NULL)
    *outlen = dlen;
  return (char *) raw;
}

/* How many data: URIs of that MIME nest inside each other, starting at hay. */
static int sf_nesting(const char *hay, const char *mime) {
  char *cur = strdupt(hay);
  int n = 0;

  while (cur != NULL) {
    char *const inner = sf_decode(cur, mime, NULL);

    freet(cur);
    cur = inner;
    if (inner != NULL)
      n++;
  }
  return n;
}

/* Above the tag parser's attribute limit, so the fixture crosses it. */
#define SF_ST_MAX_ATTRS 64

/* The 12-byte asset: high bytes and an embedded NUL, so a text-shaped copy
   would be caught. */
static const char sf_png[] = "\x89PNG\r\n\x1a\n\x00\x01\x02\xff";
#define SF_PNG_LEN 12

static const char sf_svg[] = "<svg><g id=\"icon-a\"/></svg>";

static const char sf_page[] =
    "<html><head>\n"
    "<link rel=\"stylesheet\" href=\"\001css/main.css\002\">\n"
    "<link rel=\"canonical\" href=\"\001other.html\002\">\n"
    "<title>t</title>\n"
    "<style>body { background: url(\"\001img/a%20b.png\002\"); }\n"
    /* The mark never covers a fragment, so it reaches the URI as written. */
    "b { background: url('\001img/sprite.svg\002#w x'); }\n"
    "i { background: url('\001img/sprite.svg\002#lt<s>gt'); }\n"
    "u { background: url(\"\001img/sprite.svg\002#z'(\\\xC3\xA9\"); }</style>\n"
    "</head><body>\n"
    "<img src=\"\001img/a%20b.png\002\" srcset=\"\001img/a%20b.png\002 "
    "1x, \001img/big.png\002 2x\">\n"
    "<link rel=\"icon\" href=\"\001icon.png\002\">\n"
    "<link rel=\"preload\" as=\"font\" href=\"\001font/f.woff2\002\">\n"
    "<img src=\"data:image/gif;base64,QUJD\">\n"
    /* Each has a file where its guard's removal would land it. */
    "<img src=\"http://example.com/x.png\">\n"
    "<img src=\"//example.com/x.png\">\n"
    "<input type=\"image\" src=\"\001img/in.png\002\">\n"
    /* Lazy loading: src is the placeholder, the real image rides data-src. */
    "<img src=\"\001img/ph.png\002\" data-src=\"\001img/lz.png\002\" "
    "data-srcset=\"\001img/lz2.png\002 2x\" "
    "lowsrc=\"\001img/low.png\002\">\n"
    "<object data=\"\001img/ob.png\002\"></object>\n"
    "<embed src=\"\001img/em.png\002\">\n"
    "<img data-src=\"\001other.html\002\">\n"
    /* What a first pass emits: re-resolving it is what a second pass must not
       do, and the fallback type would inline whatever the walk found. */
    "<link rel=\"stylesheet\" href=\"data:text/css;base64,QUJD\">\n"
    "<video poster=\"\001img/po.png\002\" controls>"
    "<source src=\"\001v.mp4\002\" type=\"video/mp4\"></video>\n"
    "<svg><image href=\"\001img/sv.png\002\"/></svg>\n"
    "<table background=\"\001img/bg.png\002\"><tr><td>x</td></tr></table>\n"
    /* Dropping the clamp lands the second on <root>/img/a b.png. */
    "<img src=\"\001../escape.png\002\">\n"
    "<img src=\"\001../img/a%20b.png\002\">\n"
    "<a href=\"img/a%20b.png\">link</a>\n"
    "<script src=\"\001js/app.js\002\"></script>\n"
    "<script>var s = \"</scripting>\"; var t = \"<img src='img/a%20b.png'>\";"
    "</script>\n"
    /* A fragment selects inside the asset, so it has to survive onto the
       data: URI; the query named the remote resource and must not. */
    "<img src=\"\001img/sprite.svg\002#icon-a\">\n"
    "<img srcset=\"\001img/sprite.svg\002#icon-b 2x\">\n"
    "<svg><image xlink:href=\"\001img/sprite.svg\002#icon-c\"/></svg>\n"
    "<div style=\"background:url(\001img/sprite.svg\002#icon-d)\"></div>\n"
    "<div style=\"background:url('\001img/sprite.svg\002#i)e')\"></div>\n"
    "<img src=\"\001img/sprite.svg?v=1\002#icon-f\">\n"
    "<img src=\"\001img/sprite.svg\002#g&amp;h%2Di\">\n"
    "<img src='\001img/sprite.svg\002#q\"z'>\n"
    "<img src=\"\001missing.png\002\" >\n"
    "<!--><img src=\"\001img/a%20b.png\002\">\n"
    "<div style=\"background:url(\001img/a%20b.png\002)\"></div>\n"
    "<div style='content:\"x\"; "
    "background:url(\001img/a%20b.png\002)'></div>\n"
    "</body></html>\n";

/* Lay a small mirror down under root. */
static void sf_fixture(httrackp *opt, const char *root) {
  /* A url() an inlined stylesheet could not embed must be page-relative. */
  static const char css[] =
      "@import \"\001sub/nested.css\002\";\n"
      "@import url(\"\001sub/two.css\002\");\n"
      "@import \"a\\\"url(../img/a b.png)b.css\";\n"
      "@font-face { font-family: f; src: url(\001../font/f.woff2\002); }\n"
      "body { background: url(\001../img/a%20b.png\002); }\n"
      "div { background: url(\001../img/big.png\002); }\n"
      "div.s { background: url(\001../img/big-sprite.svg\002#icon-g); }\n"
      /* A name whose escapes the rebase has to put back, unlike a fragment's;
         the '#' has to come back encoded or it reads as one. */
      "div.h { background: url(\001../img/b&amp;c%25d%23e.png\002); }\n"
      "/* url(../img/never.png) */\n";
  static const char nested[] =
      "div { background: url(\001../../img/a%20b.png\002); }\n";
  static const char two[] =
      "p { background: url(\001../../img/a%20b.png\002); }\n";
  static const char deep[] = "<html><head><link rel=\"stylesheet\" "
                             "href=\"\001../../css/main.css\002\">\n"
                             "</head><body>d</body></html>\n";
  static const char js[] = "var app = 1;\n";
  char big[4096];

  memset(big, 'B', sizeof(big));
  sf_put_marked(opt, root, "page.html",
                hts_span_at(sf_page, sizeof(sf_page) - 1));
  sf_put_marked(opt, root, "deep/sub/page.html",
                hts_span_at(deep, sizeof(deep) - 1));
  sf_put(root, "other.html", hts_span_of("<html>o</html>"));
  sf_put_marked(opt, root, "css/main.css", hts_span_at(css, sizeof(css) - 1));
  sf_put_marked(opt, root, "css/sub/nested.css",
                hts_span_at(nested, sizeof(nested) - 1));
  sf_put_marked(opt, root, "css/sub/two.css",
                hts_span_at(two, sizeof(two) - 1));
  sf_put(root, "js/app.js", hts_span_at(js, sizeof(js) - 1));
  sf_put(root, "img/a b.png", hts_span_at(sf_png, SF_PNG_LEN));
  sf_put(root, "img/big.png", hts_span_at(big, sizeof(big)));
  sf_put(root, "img/sprite.svg", hts_span_at(sf_svg, sizeof(sf_svg) - 1));
  sf_put(root, "img/big-sprite.svg", hts_span_at(big, sizeof(big)));
  sf_put(root, "img/b&c%d#e.png", hts_span_at(big, sizeof(big)));
  sf_put(root, "img/in.png", hts_span_at(sf_png, SF_PNG_LEN));
  sf_put(root, "img/po.png", hts_span_at(sf_png, SF_PNG_LEN));
  sf_put(root, "img/sv.png", hts_span_at(sf_png, SF_PNG_LEN));
  sf_put(root, "img/bg.png", hts_span_at(sf_png, SF_PNG_LEN));
  sf_put(root, "img/ph.png", hts_span_at(sf_png, SF_PNG_LEN));
  sf_put(root, "img/lz.png", hts_span_at(sf_png, SF_PNG_LEN));
  sf_put(root, "img/lz2.png", hts_span_at(sf_png, SF_PNG_LEN));
  sf_put(root, "img/low.png", hts_span_at(sf_png, SF_PNG_LEN));
  sf_put(root, "img/ob.png", hts_span_at(sf_png, SF_PNG_LEN));
  sf_put(root, "img/em.png", hts_span_at(sf_png, SF_PNG_LEN));
  sf_put(root, "icon.png", hts_span_at(sf_png, SF_PNG_LEN));
  sf_put(root, "font/f.woff2", hts_span_at("wOF2\x00\x01", 6));
  /* Each path is where a guard's removal would land a reference. Windows
     cannot create the two with a colon, so it checks the scheme guard less. */
  sf_put(root, "img/never.png", hts_span_at(sf_png, SF_PNG_LEN));
  sf_put(root, "example.com/x.png", hts_span_at(sf_png, SF_PNG_LEN));
  sf_colon_ok =
      sf_try_put(root, "http:/example.com/x.png",
                 hts_span_at(sf_png, SF_PNG_LEN)) &&
      sf_try_put(root, "data:text/css;base64,QUJD", hts_span_of("p{}"));
  { /* More attributes than the tag parser records, with a '>' inside a quoted
       value: the give-up path must not rescan quote-blind. */
    String wide = STRING_EMPTY;
    int n;

    StringCopy(wide, "<html><body><p");
    for (n = 0; n <= SF_ST_MAX_ATTRS; n++) {
      char one[32];

      assertf(sprintfbuff(one, " a%d=1", n));
      StringCat(wide, one);
    }
    StringCat(wide,
              " title=\"> <img src=img/a%20b.png> \">end</p></body></html>");
    sf_put_marked(opt, root, "wide.html",
                  hts_span_at(StringBuff(wide), StringLength(wide)));
    StringFree(wide);
  }
  sf_put(root, "v.mp4",
         hts_span_at("\x00\x00\x00\x18"
                     "ftypisom",
                     12));
}

/* singlefileclass <tag>: the class single-file gives TAG's href, where TAG
   starts at its '<'. 'c' is a stylesheet, '-' any other asset, '0' a reference
   left alone. */
static int st_singlefileclass(httrackp *opt, int argc, char **argv) {
  const char *const tag = argc >= 1 ? argv[0] : "<";
  const char *const href = strstr(tag, "href=");
  const char cls =
      href != NULL ? singlefile_ref_class(tag + 1, href, tag) : '\0';

  printf("class=%c\n", cls != '\0' ? cls : '0');
  return 0;
}

/* -#test=singlefile <dir>: rewrite a hand-built mirror and check what gets
   inlined, what must keep its link, the per-asset cap, and idempotence. */
static int st_singlefile(httrackp *opt, int argc, char **argv) {
  char BIGSTK root[HTS_URLMAXSIZE];
  char BIGSTK page[HTS_URLMAXSIZE * 2];
  const LLint saved_cap = opt->single_file_max_size;
  char *out, *css, *nested;
  size_t outlen = 0, len = 0;

  sf_err = 0;
  /* just outside the mirror */
  sf_put(argv[0], "escape.png", hts_span_at(sf_png, SF_PNG_LEN));
  fconcat(root, sizeof(root), argv[0], "mirror/");
  sf_fixture(opt, root);
  fconcat(page, sizeof(page), root, "page.html");

  /* Cap between the small assets and big.png. */
  opt->single_file_max_size = 1024;
  sf_check(singlefile_rewrite_file(opt, root, page),
           "first pass changed nothing");
  out = readfile_utf8(page);
  assertf(out != NULL);

  /* Inlined, and the payload is the file's exact bytes. */
  {
    char *img = sf_decode(out, "image/png", &len);

    sf_check(img != NULL && len == SF_PNG_LEN &&
                 memcmp(img, sf_png, SF_PNG_LEN) == 0,
             "image payload does not round-trip");
    freet(img);
  }
  {
    char *js = sf_decode(out, "application/x-javascript", &len);

    sf_check(js != NULL && len == 13 && memcmp(js, "var app = 1;\n", 13) == 0,
             "script payload does not round-trip");
    freet(js);
  }
  sf_check(strstr(out, "href=\"data:text/css;base64,") != NULL,
           "stylesheet not inlined into the link");
  {
    char *font = sf_decode(out, "font/woff2", &len);

    sf_check(font != NULL && len == 6 && memcmp(font, "wOF2\x00\x01", 6) == 0,
             "rel=preload font payload does not round-trip");
    freet(font);
  }

  /* Every other (tag, attribute) rule in the table. */
  sf_check(strstr(out, "icon.png") == NULL, "rel=icon not inlined");
  sf_check(strstr(out, "img/in.png") == NULL, "input src not inlined");
  sf_check(strstr(out, "img/po.png") == NULL, "video poster not inlined");
  sf_check(strstr(out, "img/sv.png") == NULL, "svg image href not inlined");
  sf_check(strstr(out, "img/bg.png") == NULL,
           "legacy background attribute not inlined");
  sf_check(strstr(out, "img/ob.png") == NULL, "object data not inlined");
  sf_check(strstr(out, "img/em.png") == NULL, "embed src not inlined");
  sf_check(strstr(out, "img/ph.png") == NULL, "lazy placeholder not inlined");
  sf_check(strstr(out, "img/lz.png") == NULL, "data-src not inlined");
  sf_check(strstr(out, "img/lz2.png") == NULL, "data-srcset not inlined");
  sf_check(strstr(out, "img/low.png") == NULL, "lowsrc not inlined");
  /* The class gate keeps a page out, whatever the attribute name. */
  sf_check(strstr(out, "data-src=\"other.html\"") != NULL,
           "data-src pointing at a page was inlined");

  sf_check(strstr(out, "<a href=\"img/a%20b.png\">") != NULL, "anchor inlined");
  sf_check(strstr(out, "href=\"other.html\"") != NULL, "rel=canonical inlined");
  sf_check(strstr(out, "src=\"v.mp4\"") != NULL, "video source inlined");
  sf_check(strstr(out, "src=\"http://example.com/x.png\"") != NULL,
           "absolute URL rewritten");
  sf_check(strstr(out, "src=\"//example.com/x.png\"") != NULL,
           "site-root-relative URL rewritten");
  sf_check(sf_count(out, "QUJD") == 2, "existing data: URI not preserved");
  sf_check(strstr(out, "src=\"../escape.png\"") != NULL,
           "a reference outside the mirror was resolved");
  sf_check(strstr(out, "src=\"../img/a%20b.png\"") != NULL,
           "a leading .. was dropped instead of rejected");
  sf_check(strstr(out, "var t = \"<img src='img/a%20b.png'>\";") != NULL,
           "script body rewritten past a </scripting> lookalike");

  /* Only the marked reference is touched: the value keeps its own quoting, so
     nothing can be emitted that the attribute could not already hold. */
  sf_check(strstr(out, "style='content:\"x\"; background:url(data:") != NULL,
           "style attribute re-quoted instead of substituted in place");

  /* The mark covers only the reference, so what followed it is unchanged. */
  sf_check(strstr(out, "img/sprite.svg") == NULL,
           "a fragment-bearing reference was left a link");
  sf_check(sf_count(out, "#icon-a\"") == 1, "img src fragment dropped");
  sf_check(sf_count(out, "#icon-b 2x\"") == 1, "srcset fragment dropped");
  sf_check(sf_count(out, "#icon-c\"") == 1, "xlink:href fragment dropped");
  sf_check(sf_count(out, "#icon-d)") == 1, "style url() fragment dropped");
  sf_check(sf_count(out, "#i)e')") == 1,
           "a fragment closing the url() token was rewritten");
  sf_check(sf_count(out, "#icon-f\"") == 1, "fragment after a query dropped");
  sf_check(strstr(out, "?v=1") == NULL, "query carried onto the data: URI");
  sf_check(sf_count(out, "#g&amp;h%2Di\"") == 1,
           "an escape the document already carried was encoded again");
  sf_check(sf_count(out, "#q\"z'") == 1, "a quote in a fragment was rewritten");
  sf_check(sf_count(out, "#w x')") == 1,
           "whitespace in a fragment was rewritten");
  sf_check(sf_count(out, "#lt<s>gt')") == 1,
           "'<'/'>' in a fragment were rewritten");
  sf_check(sf_count(out, "#z'(\\\xC3\xA9\")") == 1,
           "a quote, paren, backslash or high byte was rewritten");

  sf_check(strstr(out, "img/big.png 2x") != NULL, "over-cap asset inlined");
  sf_check(strstr(out, " 1x") != NULL, "srcset descriptor lost");
  sf_check(sf_count(out, "img/a%20b.png") ==
               3, /* the anchor, the script body, and the ".." one */
           "an inlinable reference was left as a link");

  /* The inlined stylesheet carries its own @import and url() inlined. */
  css = sf_decode(out, "text/css", NULL);
  sf_check(css != NULL, "stylesheet payload undecodable");
  if (css != NULL) {
    sf_check(strstr(css, "@import \"data:text/css;base64,") != NULL,
             "@import not inlined");
    sf_check(strstr(css, "url(data:image/png;base64,") != NULL,
             "url() in stylesheet not inlined");
    sf_check(strstr(css, "url(../img/never.png)") != NULL,
             "url() inside a CSS comment was rewritten");
    sf_check(strstr(css, "url(data:font/woff2;base64,") != NULL,
             "@font-face src not inlined");
    sf_check(strstr(css, "@import url(\"data:text/css;base64,") != NULL,
             "@import url() form not inlined");
    sf_check(strstr(css, "url(../img/a b.png)b.css") != NULL,
             "url() inside a string with an escaped quote was rewritten");
    /* The over-cap url() read ../img/big.png from css/; this page sits at the
       root, so it has to come back out as img/big.png or it dangles. */
    sf_check(strstr(css, "url(img/big.png)") != NULL,
             "over-cap url() not rebased onto the page's directory");
    sf_check(strstr(css, "url(img/big-sprite.svg#icon-g)") != NULL,
             "a rebased url() lost its fragment");
    sf_check(strstr(css, "url(img/b%26c%25d%23e.png)") != NULL,
             "a rebased name came back unescaped");
    nested = sf_decode(css, "text/css", NULL);
    sf_check(nested != NULL &&
                 strstr(nested, "url(data:image/png;base64,") != NULL,
             "url() in the @import'ed stylesheet not inlined");
    freet(nested);
    freet(css);
  }

  /* A tag with more attributes than the parser records comes back byte for
     byte, quoted '>' and all, instead of being re-scanned as markup. */
  {
    char BIGSTK wide[HTS_URLMAXSIZE * 2];
    char *before, *after;

    fconcat(wide, sizeof(wide), root, "wide.html");
    before = readfile_utf8(wide);
    assertf(before != NULL);
    (void) singlefile_rewrite_file(opt, root, wide);
    after = readfile_utf8(wide);
    sf_check(after != NULL && strcmp(before, after) == 0,
             "an over-wide tag was rewritten");
    freet(after);
    freet(before);
  }

  /* The same stylesheet from two directories down, where the rebase has to
     climb: the emitted path must stay inside the mirror and name the file. */
  {
    char BIGSTK deep[HTS_URLMAXSIZE * 2];
    char *dout, *dcss;

    fconcat(deep, sizeof(deep), root, "deep/sub/page.html");
    sf_check(singlefile_rewrite_file(opt, root, deep),
             "deep page not rewritten");
    dout = readfile_utf8(deep);
    assertf(dout != NULL);
    dcss = sf_decode(dout, "text/css", NULL);
    sf_check(dcss != NULL && strstr(dcss, "url(../../img/big.png)") != NULL,
             "over-cap url() not rebased from a nested page");
    freet(dcss);
    freet(dout);
  }

  /* Idempotence: a second pass must find nothing and leave the bytes alone. */
  sf_check(!singlefile_rewrite_file(opt, root, page), "second pass rewrote");
  {
    char *again = readfile_utf8(page);

    sf_check(again != NULL && strcmp(again, out) == 0,
             "second pass changed the page");
    freet(again);
  }
  freet(out);

  /* Same page, a cap above big.png: it now inlines. */
  fconcat(root, sizeof(root), argv[0], "mirror2/");
  sf_fixture(opt, root);
  fconcat(page, sizeof(page), root, "page.html");
  opt->single_file_max_size = 1024 * 1024;
  sf_check(singlefile_rewrite_file(opt, root, page),
           "raised-cap pass changed nothing");
  out = readfile_utf8(page);
  assertf(out != NULL);
  sf_check(strstr(out, "img/big.png 2x") == NULL,
           "asset under the raised cap still a link");
  freet(out);

  /* Nothing inlines, so the rewriter's output must be byte transparent. */
  fconcat(root, sizeof(root), argv[0], "mirror3/");
  sf_fixture(opt, root);
  fconcat(page, sizeof(page), root, "page.html");
  opt->single_file_max_size = 1;
  /* The page is still rewritten: nothing inlines, but the marks must go. */
  sf_check(singlefile_rewrite_file(opt, root, page),
           "one-byte cap left the marks in place");
  {
    char *capped = readfile_utf8(page);

    /* Only the two data: URIs the fixture itself ships. */
    sf_check(capped != NULL && sf_count(capped, ";base64,") == 2,
             "one-byte cap inlined");
    freet(capped);
  }
  {
    String verbatim = STRING_EMPTY;

    StringClear(verbatim);
    String marked = STRING_EMPTY;

    sf_expand_fixture(opt, hts_span_at(sf_page, sizeof(sf_page) - 1), &marked);
    (void) singlefile_rewrite_html(
        opt, root, page, hts_span_at(StringBuff(marked), StringLength(marked)),
        SINGLEFILE_MAX_PAGE_SIZE, &verbatim);
    /* A reference that cannot inline loses its mark and nothing else. */
    sf_check(StringLength(verbatim) < StringLength(marked),
             "a page with nothing to inline kept its marks");
    sf_check(strstr(StringBuff(verbatim), singlefile_intro(opt)) == NULL,
             "an un-inlinable reference kept its mark");
    StringFree(marked);
    StringFree(verbatim);
  }
  /* A mark ending the body is read with no byte after it. */
  {
    static const char tail[] = "\001nothere.png\002";
    String marked = STRING_EMPTY;
    String got = STRING_EMPTY;

    sf_expand_fixture(opt, hts_span_at(tail, sizeof(tail) - 1), &marked);
    StringAddchar(marked, '9'); /* a digit past the span must not be read */
    StringClear(got);
    (void) singlefile_rewrite_html(
        opt, root, page,
        hts_span_at(StringBuff(marked), StringLength(marked) - 1),
        SINGLEFILE_MAX_PAGE_SIZE, &got);
    sf_check(StringLength(got) == 11 &&
                 memcmp(StringBuff(got), "nothere.png", 11) == 0,
             "a mark ending the body was not read");
    StringFree(marked);
    StringFree(got);
  }
  (void) outlen;

  /* The pass must read back any mark singlefile_mark writes. emit_max derives
     the worst case apart from SINGLEFILE_MAX_SPAN. */
  {
    const size_t emit_max =
        HTS_URLMAXSIZE * 2 *
        (HTS_HTMLESCAPE_FULL_MAXEXP + HTS_HTMLESCAPE_MAXEXP);
    const size_t spans[] = {4097, emit_max, emit_max + 1, emit_max * 4};
    char mark[SINGLEFILE_MARK_MAX];
    size_t k;

    sf_check(singlefile_mark(opt, HTS_BUF(mark), SINGLEFILE_CLASS_ANY,
                             emit_max)[0] != '\0',
             "a span htsparse can emit was refused a mark");
    for (k = 0; k < sizeof(spans) / sizeof(spans[0]); k++) {
      String marked = STRING_EMPTY, got = STRING_EMPTY;
      char *pad = (char *) malloct(spans[k]);
      size_t marklen;

      assertf(pad != NULL);
      memset(pad, 'a', spans[k]);
      StringClear(marked);
      StringCat(marked, "<img src=\"");
      StringMemcat(marked, pad, spans[k]);
      marklen = strlen(
          singlefile_mark(opt, HTS_BUF(mark), SINGLEFILE_CLASS_ANY, spans[k]));
      StringCat(marked, mark);
      StringCat(marked, "\">\n");
      StringClear(got);
      (void) singlefile_rewrite_html(
          opt, root, page,
          hts_span_at(StringBuff(marked), StringLength(marked)),
          SINGLEFILE_MAX_PAGE_SIZE, &got);
      sf_check(strstr(StringBuff(got), singlefile_intro(opt)) == NULL,
               "a mark the emitter wrote stayed in the page as text");
      sf_check(hts_memstr(hts_span_at(StringBuff(got), StringLength(got)),
                          hts_span_at(pad, spans[k])) != NULL,
               "the reference the mark measured was lost");
      /* Nothing resolves here, so only the mark may go. */
      sf_check(StringLength(got) == StringLength(marked) - marklen,
               "the pass removed something other than the mark");
      if (spans[k] == emit_max + 1) {
        char tail[32];

        /* Hand-built at cap+1, which singlefile_mark now refuses: without the
           cap sf_parse_mark reads it and eats the padding behind it. */
        snprintf(tail, sizeof(tail), ".%c.%d", SINGLEFILE_CLASS_ANY,
                 (int) spans[k]);
        StringClear(marked);
        StringCat(marked, "<img src=\"");
        StringMemcat(marked, pad, spans[k]);
        StringCat(marked, singlefile_intro(opt));
        StringCat(marked, tail);
        StringCat(marked, "\">\n");
        StringClear(got);
        (void) singlefile_rewrite_html(
            opt, root, page,
            hts_span_at(StringBuff(marked), StringLength(marked)),
            SINGLEFILE_MAX_PAGE_SIZE, &got);
        sf_check(strstr(StringBuff(got), singlefile_intro(opt)) != NULL,
                 "an over-cap length was read as a mark");
        /* 2^64 + 10: unsaturated, the digits wrap to 10 and the mark eats ten
           bytes of padding instead of being refused. */
        StringClear(marked);
        StringCat(marked, "<img src=\"");
        StringMemcat(marked, pad, spans[k]);
        StringCat(marked, singlefile_intro(opt));
        StringCat(marked, ".-.18446744073709551626");
        StringCat(marked, "\">\n");
        StringClear(got);
        (void) singlefile_rewrite_html(
            opt, root, page,
            hts_span_at(StringBuff(marked), StringLength(marked)),
            SINGLEFILE_MAX_PAGE_SIZE, &got);
        sf_check(StringLength(got) == StringLength(marked),
                 "a wrapping digit run was read as a mark");
      }
      freet(pad);
      StringFree(marked);
      StringFree(got);
    }
  }

  /* The fixed-width intro keeps singlefile_may_mark off an empty needle. */
  {
    String probe = STRING_EMPTY;

    sf_check(strlen(singlefile_intro(opt)) == SINGLEFILE_INTRO_LEN,
             "the intro is not the fixed-width string may_mark assumes");
    sf_check(singlefile_may_mark(opt, hts_span_of("")),
             "an empty body was refused");
    StringClear(probe);
    StringMemcat(probe, "x\0", 2); /* bodies carry NULs; strstr would stop */
    StringCat(probe, singlefile_intro(opt));
    sf_check(!singlefile_may_mark(
                 opt, hts_span_at(StringBuff(probe), StringLength(probe))),
             "an intro ending the body, past a NUL, was missed");
    sf_check(singlefile_may_mark(
                 opt, hts_span_at(StringBuff(probe), StringLength(probe) - 1)),
             "a truncated intro was read as one");
    StringFree(probe);
  }

  /* The budget stops a self-importing stylesheet, and the large run is the
     control. */
  {
    static const char bomb_css[] =
        "@import \"\001b.css\002\";@import \"\001b.css\002\";"
        "@import \"\001b.css\002\";@import \"\001b.css\002\";\n";
    static const char bomb_html[] =
        "<html><head>"
        "<link rel=\"stylesheet\" href=\"\001b.css\002\">"
        "</head></html>\n";
    size_t css_len;
    String small = STRING_EMPTY, large = STRING_EMPTY, bomb = STRING_EMPTY;

    fconcat(root, sizeof(root), argv[0], "bomb/");
    css_len = sf_put_marked(opt, root, "b.css",
                            hts_span_at(bomb_css, sizeof(bomb_css) - 1));
    fconcat(page, sizeof(page), root, "page.html");
    opt->single_file_max_size = 1024 * 1024;
    StringClear(small);
    StringClear(large);
    sf_expand_fixture(opt, hts_span_at(bomb_html, sizeof(bomb_html) - 1),
                      &bomb);
    (void) singlefile_rewrite_html(
        opt, root, page, hts_span_at(StringBuff(bomb), StringLength(bomb)),
        (LLint) css_len * 3, &small);
    (void) singlefile_rewrite_html(
        opt, root, page, hts_span_at(StringBuff(bomb), StringLength(bomb)),
        SINGLEFILE_MAX_PAGE_SIZE, &large);
    sf_check(StringLength(large) > 4096, "the @import bomb did not fan out");
    sf_check(StringLength(small) < StringLength(large) / 8,
             "the per-page budget did not cut the fan-out short");
    /* Three files fit in three file-lengths only if the budget is charged
       before each nested rewrite; charging after measured five levels. */
    sf_check(sf_nesting(StringBuff(small), "text/css") == 3,
             "the per-page budget was not charged as each asset was taken");
    StringFree(small);
    StringFree(large);
    StringFree(bomb);
  }

  if (!sf_colon_ok)
    printf("singlefile: ':' is not a legal filename here, so the scheme guard "
           "only gets the weaker check\n");
  opt->single_file_max_size = saved_cap;
  return st_verdict("singlefile", sf_err);
}

/* Compare the WARC header walks with frozen copies. */

static int legacy_header_is(const char *line, size_t line_len,
                            const char *name) {
  size_t nl = strlen(name), i;
  if (line_len < nl || hts_strncasecmp(line, name, nl) != 0)
    return 0;
  for (i = nl; i < line_len && (line[i] == ' ' || line[i] == '\t'); i++)
    ;
  return i < line_len && line[i] == ':';
}

static int legacy_normalize_http_headers(const char *resp_hdr, long long set_cl,
                                         String *out) {
  const char *p = resp_hdr;
  int first = 1;
  char cl[64];
  if (resp_hdr == NULL)
    return -1;
  while (*p != '\0') {
    const char *eol = strchr(p, '\n');
    size_t len = (eol != NULL) ? (size_t) (eol - p) : strlen(p);
    if (len > 0 && p[len - 1] == '\r')
      len--;
    if (len == 0)
      break;
    if (first) {
      first = 0;
    } else if (legacy_header_is(p, len, "Transfer-Encoding")) {
      goto next;
    } else if (set_cl >= 0 && legacy_header_is(p, len, "Content-Length")) {
      goto next;
    }
    StringMemcat(*out, p, len);
    StringCat(*out, "\r\n");
  next:
    if (eol == NULL)
      break;
    p = eol + 1;
  }
  if (set_cl >= 0) {
    snprintf(cl, sizeof(cl), "Content-Length: %lld\r\n", set_cl);
    StringCat(*out, cl);
  }
  return 0;
}

static void legacy_http_header_value(const char *hdr, const char *name,
                                     hts_buf out) {
  size_t nl = strlen(name);
  const char *p = hdr;
  out.p[0] = '\0';
  if (hdr == NULL)
    return;
  while (*p != '\0') {
    const char *eol = strchr(p, '\n');
    size_t len = (eol != NULL) ? (size_t) (eol - p) : strlen(p);
    if (len > 0 && p[len - 1] == '\r')
      len--;
    if (len == 0)
      break;
    if (len > nl && hts_strncasecmp(p, name, nl) == 0 && p[nl] == ':') {
      const char *v = p + nl + 1;
      size_t vlen, k;
      while (v < p + len && (*v == ' ' || *v == '\t'))
        v++;
      vlen = (size_t) (p + len - v);
      for (k = 0; k < vlen; k++)
        if (v[k] == ';' || v[k] == ' ' || v[k] == '\t') {
          vlen = k;
          break;
        }
      if (vlen >= out.size)
        vlen = out.size - 1;
      memcpy(out.p, v, vlen);
      out.p[vlen] = '\0';
      return;
    }
    if (eol == NULL)
      break;
    p = eol + 1;
  }
}

static void warcdiff_one(parsediff_stats *st, const char *hdr) {
  static const long long cls[] = {-1, 0, 42};
  static const char *const names[] = {"Content-Type", "content-type",
                                      "Content-Length", "X", ""};
  static const size_t sizes[] = {1, 2, 8, 256};
  size_t i, j;

  for (i = 0; i < sizeof(cls) / sizeof(cls[0]); i++) {
    String o = STRING_EMPTY;
    const hts_boolean fo = legacy_normalize_http_headers(hdr, cls[i], &o) == 0;
    /* An output that stayed empty has no buffer, and reads as "". */
    const char *const ob = StringBuff(o) != NULL ? StringBuff(o) : "";
    char *n = warc_normalized_headers(hdr, cls[i]);

    st->cases++;
    if (fo != (n != NULL) ||
        (n != NULL &&
         (strlen(n) != StringLength(o) || memcmp(n, ob, StringLength(o)) != 0)))
      parsediff_fail(st, "normalize_http_headers", hdr != NULL ? hdr : "(null)",
                     "", ob, n != NULL ? n : "(null)");
    StringFree(o);
    freet(n);
  }
  for (i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
    for (j = 0; j < sizeof(sizes) / sizeof(sizes[0]); j++) {
      char o[256], n[256];

      memset(o, 'O', sizeof(o));
      memset(n, 'O', sizeof(n));
      st->cases++;
      legacy_http_header_value(hdr, names[i], hts_buf_at(o, sizes[j]));
      warc_http_header_value(hdr, names[i], hts_buf_at(n, sizes[j]));
      if (memcmp(o, n, sizeof(o)) != 0)
        parsediff_fail(st, "http_header_value", hdr != NULL ? hdr : "(null)",
                       names[i], o, n);
    }
  }
}

static int st_warcspandiff(httrackp *opt, int argc, char **argv) {
  static const char *const hand[] = {
      "",
      "\n",
      "\r\n",
      "\r",
      "\r\r\n",
      "HTTP/1.1 200 OK",
      "HTTP/1.1 200 OK\r\n",
      "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\nbody",
      "HTTP/1.1 200 OK\nContent-Type: text/html; charset=x\nX: y\n",
      "HTTP/1.1 200 OK\r\nContent-Type:text/html\r\r\nX: y",
      "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nContent-Length: 5\r\n",
      "HTTP/1.1 200 OK\r\nContent-Length : 5\r\ntransfer-encoding\t: x\r\n",
      "HTTP/1.1 200 OK\r\ncontent-length: 5\r\nX: y\r\n",
      "HTTP/1.1 200 OK\r\n\r\nContent-Type: late\r\n",
      "\nContent-Type: text/html\n",
      "HTTP/1.1 200 OK\r\nContent-Type:\r\n",
      "HTTP/1.1 200 OK\r\nContent-Type: \t \r\n",
      "HTTP/1.1 200 OK\r\nContent-Type: a\rb\r\n",
      "HTTP/1.1 200 OK\r\nContent-Typex: a\r\nContent-Type: b\r\n",
      "HTTP/1.1 200 OK\r\nContent-Type : a\r\nContent-Type: b\r\n",
      "HTTP/1.1 200 OK\r\n:\r\n\r\n",
      NULL};
  static const char *const alphabet[] = {"HTTP/1.1 200 OK",
                                         "\r",
                                         "\n",
                                         "\r\n",
                                         "Content-Type",
                                         ":",
                                         "Content-Length",
                                         "Transfer-Encoding",
                                         " ",
                                         "\t",
                                         "text/html",
                                         ";",
                                         "x",
                                         "chunked",
                                         "5",
                                         "content-type"};
  /* These header values sit around the 256-byte output buffer. */
  static const size_t lengths[] = {254, 255, 256, 257};
  parsediff_stats st = {0, 0};
  uint32_t seed = 547;
  size_t i;
  int r;

  /* An OUTSZ of 0 leaves OUT alone. */
  {
    char z = 'Z';

    warc_http_header_value("HTTP/1.1 200 OK\r\nContent-Type: a\r\n",
                           "Content-Type", hts_buf_at(&z, 0));
    warc_http_header_value(NULL, "Content-Type", hts_buf_at(&z, 0));
    assertf(z == 'Z');
  }
  warcdiff_one(&st, NULL);
  for (i = 0; hand[i] != NULL; i++)
    warcdiff_one(&st, hand[i]);
  for (i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
    char *run = parsediff_run('x', lengths[i]);
    String hdr = STRING_EMPTY;

    StringCat(hdr, "HTTP/1.1 200 OK\r\nContent-Type: ");
    StringCat(hdr, run);
    StringCat(hdr, "\r\nX: ");
    StringCat(hdr, run);
    warcdiff_one(&st, StringBuff(hdr));
    StringFree(hdr);
    freet(run);
  }
  for (r = 0; r < 6000; r++) {
    String hdr = STRING_EMPTY;

    parsediff_random(&hdr, &seed, alphabet,
                     sizeof(alphabet) / sizeof(alphabet[0]), 20);
    warcdiff_one(&st, StringBuff(hdr) != NULL ? StringBuff(hdr) : "");
    StringFree(hdr);
  }
  printf("warcspandiff: %lu cases, %lu failures\n", st.cases, st.failures);
  if (st.failures == 0)
    printf("warcspandiff: OK\n");
  return st.failures != 0;
}

/* ------------------------------------------------------------ */
/* Registry: this module's tests, in the order -#test lists them. */
/* ------------------------------------------------------------ */

const struct selftest_entry selftests_warc[] = {
    {"warc", "<dir>", "WARC/1.1 writer: framing, digests, revisit dedup",
     st_warc},
    {"warc-trunc", "<dir>", "WARC-Truncated on a cap-truncated body",
     st_warc_trunc},
    {"warc-ftp", "<dir>", "ftp resource record (no HTTP envelope)",
     st_warc_ftp},
    {"warc-rotate", "<dir>", "--warc-max-size segment rotation",
     st_warc_rotate},
    {"warc-verbatim", "<dir>", "verbatim compressed response body (default)",
     st_warc_verbatim},
    {"warc-surt", "", "SURT canonicalization of the CDXJ sort key",
     st_warc_surt},
    {"warc-offset", "<dir>", "the CDXJ record offset stays 64-bit past 2GB",
     st_warc_offset},
    {"warc-longurl", "<dir>",
     "a URL past the header-format buffer still reaches the archive",
     st_warc_longurl},
    {"warc-cdx", "<dir>", "--warc-cdx CDXJ index: sorted, offsets inflate",
     st_warc_cdx},
    {"warc-cdx-errors", "<dir>",
     "--warc-cdx diagnostics when the index cannot be written or is empty",
     st_warc_cdx_errors},
    {"warc-cdx-utf8", "<dir>",
     "--warc-cdx JSON stays valid UTF-8 for a raw Latin-1 URL",
     st_warc_cdx_utf8},
    {"warc-teardown", "<dir>",
     "a closed or abandoned archive is not reopened by a late transaction",
     st_warc_teardown},
#if HTS_USEOPENSSL
    {"warc-wacz", "<dir>", "--wacz package: layout, STORE mode, sha256 digests",
     st_warc_wacz},
#endif
    {"singlefileclass", "[link tag]",
     "class single-file gives a <link href> reference", st_singlefileclass},
    {"singlefile", "<dir>",
     "--single-file: what is inlined, the per-asset cap, idempotence",
     st_singlefile},
    {"warcspandiff", "",
     "the WARC header walks match frozen copies of the old ones",
     st_warcspandiff},
    {NULL, NULL, NULL, NULL},
};
