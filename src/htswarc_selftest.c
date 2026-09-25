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

/* Slurp a whole file into a malloc'd buffer; sets *len. NULL on error. */
static unsigned char *warc_slurp(const char *path, size_t *len) {
  char catbuff[CATBUFF_SIZE];
  FILE *f = FOPEN(fconv(catbuff, sizeof(catbuff), path), "rb");
  unsigned char *buf;
  long sz;
  if (f == NULL)
    return NULL;
  if (fseek(f, 0, SEEK_END) != 0 || (sz = ftell(f)) < 0) {
    fclose(f);
    return NULL;
  }
  rewind(f);
  buf = malloct((size_t) sz + 1);
  if (buf == NULL) {
    fclose(f);
    return NULL;
  }
  *len = fread(buf, 1, (size_t) sz, f);
  fclose(f);
  return buf;
}

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

/* Feed a synthetic transaction and validate the resulting .warc.gz against the
   WARC/1.1 spec: each record a self-standing gzip member starting WARC/1.,
   Content-Length == block length, the \r\n\r\n trailer intact, the response
   body round-trips, and the hop-by-hop Transfer-Encoding is dropped (a real
   Content-Encoding is kept verbatim; see warc-verbatim). */
/* Argument order kept for the existing call sites; the search itself is the
   shared hts_memstr. */
static const char *warc_memstr(const char *hay, const char *needle,
                               size_t haylen, size_t nlen) {
  return hts_memstr(hay, haylen, needle, nlen);
}

static int st_warc(httrackp *opt, int argc, char **argv) {
  char path[HTS_URLMAXSIZE];
  warc_writer *w;
  unsigned char *data;
  size_t data_len = 0;
  const unsigned char *p, *end;
  int err = 0, nrec = 0, nresp = 0, nreq = 0, nrevisit = 0, ninfo = 0;
  int seen_a_body = 0, body_occurrences = 0, a2_bodyless = 0, nm_cl_ok = 0;
  static const char a_body[] = "Hello, WARC!\n";

  if (argc < 1) {
    fprintf(stderr, "warc: needs a writable directory\n");
    return 1;
  }
  fconcat(path, sizeof(path), argv[0], "warc-selftest.warc.gz");

  w = warc_open(opt, path);
  assertf(w != NULL);

  /* 200 HTML, plaintext body: bogus Content-Length rewritten, hop-by-hop
     Transfer-Encoding dropped. The whitespace before its ':' exercises
     header_is tolerating "Name : value". */
  warc_write_transaction(
      w, "http://test.local/a.html", "127.0.0.1",
      "GET /a.html HTTP/1.1\r\nHost: test.local\r\n\r\n",
      "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n"
      "Transfer-Encoding : chunked\r\nContent-Length: 999\r\n\r\n",
      a_body, sizeof(a_body) - 1, NULL, NULL, 200, 0, 0);

  /* 302 redirect: header-only, no body. */
  warc_write_transaction(
      w, "http://test.local/r", "127.0.0.1",
      "GET /r HTTP/1.1\r\nHost: test.local\r\n\r\n",
      "HTTP/1.1 302 Found\r\nLocation: http://test.local/a.html\r\n\r\n", NULL,
      0, NULL, NULL, 302, 0, 0);

  /* 200 binary, chunked coding on the wire (already de-chunked here). */
  warc_write_transaction(
      w, "http://test.local/b.bin", "127.0.0.1",
      "GET /b.bin HTTP/1.1\r\nHost: test.local\r\n\r\n",
      "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\n"
      "Transfer-Encoding: chunked\r\n\r\n",
      "\x00\x01\x02\x03\x04", 5, NULL, NULL, 200, 0, 0);

  /* 200 with a body shorter than the declared Content-Length (rewritten). */
  warc_write_transaction(
      w, "http://test.local/trunc", "127.0.0.1",
      "GET /trunc HTTP/1.1\r\nHost: test.local\r\n\r\n",
      "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: "
      "100\r\n\r\n",
      "short", 5, NULL, NULL, 200, 0, 0);

  /* Same payload as a.html at a new URL: identical-payload-digest revisit
     (OpenSSL builds only; a plain build writes a second full response). */
  warc_write_transaction(w, "http://test.local/a2.html", "127.0.0.1",
                         "GET /a2.html HTTP/1.1\r\nHost: test.local\r\n\r\n",
                         "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n",
                         a_body, sizeof(a_body) - 1, NULL, NULL, 200, 0, 0);

  /* 304 revisit with an EMPTY response-header block: the block is just the
     2-byte separator, so declared Content-Length must be exactly 2 (F3). */
  warc_write_transaction(w, "http://test.local/nm", "127.0.0.1",
                         "GET /nm HTTP/1.1\r\nHost: test.local\r\n\r\n", "",
                         NULL, 0, NULL, NULL, 304, 1, 0);

  warc_close(w);

  data = warc_slurp(path, &data_len);
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
    nrec++;
    /* magic */
    if (rlen < 8 || memcmp(rec, "WARC/1.", 7) != 0)
      err = 1;
    /* record header ends at the first blank line */
    sep = warc_memstr((char *) rec, "\r\n\r\n", rlen, 4);
    if (sep == NULL) {
      err = 1;
      freet(rec);
      continue;
    }
    hdr_len = (size_t) ((const unsigned char *) sep - rec) + 4;
    /* Content-Length must equal the actual block length */
    cl = warc_memstr((char *) rec, "Content-Length:", hdr_len, 15);
    if (cl == NULL || sscanf(cl + 15, "%lld", &block_len) != 1)
      err = 1;
    else {
      if (hdr_len + (size_t) block_len + 4 != rlen)
        err = 1; /* header + block + trailing CRLFCRLF */
      else if (memcmp(rec + hdr_len + block_len, "\r\n\r\n", 4) != 0)
        err = 1; /* trailer intact */
    }
    if (warc_memstr((char *) rec, "WARC-Type: warcinfo", hdr_len, 19) != NULL)
      ninfo++;
    if (warc_memstr((char *) rec, "WARC-Type: request", hdr_len, 18) != NULL)
      nreq++;
    if (warc_memstr((char *) rec, "WARC-Type: response", hdr_len, 19) != NULL)
      nresp++;
    if (warc_memstr((char *) rec, "WARC-Type: revisit", hdr_len, 18) != NULL)
      nrevisit++;
    /* F1: the full body must appear exactly once across the whole file (a
       revisit must not re-embed it). */
    if (warc_memstr((char *) rec, a_body, rlen, sizeof(a_body) - 1) != NULL)
      body_occurrences++;
    /* F1: the a2.html identical-payload-digest revisit carries no body. */
    if (warc_memstr((char *) rec, "WARC-Target-URI: http://test.local/a2.html",
                    hdr_len, 42) != NULL &&
        warc_memstr((char *) rec, "WARC-Type: revisit", hdr_len, 18) != NULL)
      a2_bodyless =
          (warc_memstr((char *) rec, a_body, rlen, sizeof(a_body) - 1) == NULL);
    /* F3: the empty-header 304 revisit block is exactly the 2-byte separator
       (the request record shares this target URI, so match the revisit only).
     */
    if (warc_memstr((char *) rec, "WARC-Target-URI: http://test.local/nm",
                    hdr_len, 37) != NULL &&
        warc_memstr((char *) rec, "WARC-Type: revisit", hdr_len, 18) != NULL)
      nm_cl_ok = (block_len == 2);
    /* a.html response body round-trips; no Content-Encoding (plaintext) and the
       whitespaced Transfer-Encoding was dropped (header_is robustness). */
    if (warc_memstr((char *) rec, "WARC-Target-URI: http://test.local/a.html",
                    hdr_len, 41) != NULL &&
        warc_memstr((char *) rec, "msgtype=response", hdr_len, 16) != NULL) {
      const char *bsep = warc_memstr((char *) rec + hdr_len, "\r\n\r\n",
                                     (size_t) block_len, 4);
      if (bsep == NULL)
        err = 1;
      else {
        size_t bodyoff = (size_t) (bsep - (char *) rec) + 4;
        size_t got = rlen - 4 - bodyoff; /* minus record trailer */
        if (got != sizeof(a_body) - 1 ||
            memcmp(rec + bodyoff, a_body, got) != 0)
          err = 1;
        seen_a_body = 1;
      }
      if (warc_memstr((char *) rec, "Content-Encoding",
                      hdr_len + (size_t) block_len, 16) != NULL ||
          warc_memstr((char *) rec, "Transfer-Encoding",
                      hdr_len + (size_t) block_len, 17) != NULL)
        err = 1;
    }
    freet(rec);
  }
  freet(data);

  /* warcinfo + 6 transactions (response/revisit + request each) = 13 records.
   */
  if (ninfo != 1 || nreq != 6 || nrec != 13 || !seen_a_body || !nm_cl_ok)
    err = 1;
#if HTS_USEOPENSSL
  /* a.html + b.bin + trunc + 302 are full responses; a2.html deduped to a
     revisit (bodyless), nm is the 304 revisit; the body appears exactly once.
   */
  if (nrevisit != 2 || nresp != 4 || !a2_bodyless || body_occurrences != 1)
    err = 1;
#else
  /* No digests: a2.html is a second full response, so the body appears twice
     and only the 304 nm is a revisit. */
  if (nrevisit != 1 || nresp != 5 || body_occurrences != 2)
    err = 1;
  (void) a2_bodyless; /* only meaningful with digests */
#endif

  printf("warc: %d records (%d response, %d request, %d revisit): %s\n", nrec,
         nresp, nreq, nrevisit, err ? "FAIL" : "OK");
  return err;
}

/* Parse a record's header/block split; sets *hdr_len and *block_len, returns 0
   when Content-Length matches the actual block bytes, -1 otherwise. */
static int warc_rec_split(const unsigned char *rec, size_t rlen,
                          size_t *hdr_len, long long *block_len) {
  const char *sep = warc_memstr((const char *) rec, "\r\n\r\n", rlen, 4);
  const char *cl;
  *block_len = 0;
  if (sep == NULL)
    return -1;
  *hdr_len = (size_t) ((const unsigned char *) sep - rec) + 4;
  cl = warc_memstr((const char *) rec, "Content-Length:", *hdr_len, 15);
  if (cl == NULL || sscanf(cl + 15, "%lld", block_len) != 1 ||
      *hdr_len + (size_t) *block_len + 4 != rlen)
    return -1;
  return 0;
}

/* A cap-truncated body is still archived, tagged WARC-Truncated (v1.1). A
   compressed body cut short by a cap keeps its Content-Encoding (the stored
   bytes are the coded partial), so the record's label matches its body: assert
   the plaintext response carries "WARC-Truncated: length", and the gzip-coded
   one carries "WARC-Truncated: time", keeps Content-Encoding, and stores the
   coded bytes verbatim. */
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

  if (argc < 1) {
    fprintf(stderr, "warc-trunc: needs a writable directory\n");
    return 1;
  }
  fconcat(path, sizeof(path), argv[0], "warc-trunc.warc.gz");
  w = warc_open(opt, path);
  assertf(w != NULL);
  warc_write_transaction(
      w, "http://test.local/big.bin", "127.0.0.1",
      "GET /big.bin HTTP/1.1\r\nHost: test.local\r\n\r\n",
      "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\n\r\n", body,
      sizeof(body) - 1, NULL, NULL, 200, 0, WARC_TRUNC_LENGTH);
  warc_write_transaction(
      w, "http://test.local/big.gz", "127.0.0.1",
      "GET /big.gz HTTP/1.1\r\nHost: test.local\r\n\r\n",
      "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Encoding: "
      "gzip\r\n\r\n",
      (const char *) gz, sizeof(gz), NULL, NULL, 200, 0, WARC_TRUNC_TIME);
  warc_close(w);

  data = warc_slurp(path, &data_len);
  assertf(data != NULL);
  p = data;
  end = data + data_len;
  while (p < end) {
    size_t rlen = 0, hdr_len = 0;
    long long block_len = 0;
    unsigned char *rec = warc_next_member(&p, end, &rlen);
    if (rec == NULL) {
      if (rlen != 0)
        err = 1;
      break;
    }
    if (warc_rec_split(rec, rlen, &hdr_len, &block_len) != 0) {
      err = 1;
      freet(rec);
      continue;
    }
    if (warc_memstr((char *) rec, "WARC-Type: response", hdr_len, 19) != NULL) {
      nresp++;
      if (warc_memstr((char *) rec,
                      "WARC-Target-URI: http://test.local/big.bin", hdr_len,
                      42) != NULL &&
          warc_memstr((char *) rec, "WARC-Truncated: length", hdr_len, 22) !=
              NULL)
        trunc_len = 1;
      if (warc_memstr((char *) rec, "WARC-Target-URI: http://test.local/big.gz",
                      hdr_len, 41) != NULL) {
        const char *bsep = warc_memstr((char *) rec + hdr_len, "\r\n\r\n",
                                       (size_t) block_len, 4);
        size_t bodyoff = bsep ? (size_t) (bsep - (char *) rec) + 4 : 0;
        size_t got = bsep ? rlen - 4 - bodyoff : 0;
        /* WARC-Truncated: time, Content-Encoding kept, stored body == coded. */
        if (bsep != NULL &&
            warc_memstr((char *) rec, "WARC-Truncated: time", hdr_len, 20) !=
                NULL &&
            warc_memstr((char *) rec + hdr_len,
                        "Content-Encoding:", (size_t) block_len, 17) != NULL &&
            got == sizeof(gz) && memcmp(rec + bodyoff, gz, sizeof(gz)) == 0)
          trunc_gz = 1;
      }
    }
    freet(rec);
  }
  freet(data);
  if (!trunc_len || !trunc_gz || nresp != 2)
    err = 1;
  printf("warc-trunc: %s\n", err ? "FAIL" : "OK");
  return err;
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

  if (argc < 1) {
    fprintf(stderr, "warc-ftp: needs a writable directory\n");
    return 1;
  }
  fconcat(path, sizeof(path), argv[0], "warc-ftp.warc.gz");
  w = warc_open(opt, path);
  assertf(w != NULL);
  warc_write_resource(w, "ftp://ftp.local/file.bin", "127.0.0.1",
                      "application/octet-stream", body, sizeof(body) - 1, NULL,
                      0);
  warc_close(w);

  data = warc_slurp(path, &data_len);
  assertf(data != NULL);
  p = data;
  end = data + data_len;
  while (p < end) {
    size_t rlen = 0, hdr_len = 0;
    long long block_len = 0;
    unsigned char *rec = warc_next_member(&p, end, &rlen);
    if (rec == NULL) {
      if (rlen != 0)
        err = 1;
      break;
    }
    if (warc_rec_split(rec, rlen, &hdr_len, &block_len) != 0)
      err = 1;
    if (warc_memstr((char *) rec, "WARC-Type: resource", hdr_len, 19) != NULL) {
      nresource++;
      if ((size_t) block_len != sizeof(body) - 1 ||
          memcmp(rec + hdr_len, body, sizeof(body) - 1) != 0)
        err = 1; /* block is the raw payload, no HTTP envelope */
      if (warc_memstr((char *) rec, "WARC-Target-URI: ftp://ftp.local/file.bin",
                      hdr_len, 41) == NULL ||
          warc_memstr((char *) rec, "Content-Type: application/octet-stream",
                      hdr_len, 38) == NULL)
        err = 1;
    }
    if (warc_memstr((char *) rec, "WARC-Type: response", hdr_len, 19) != NULL)
      nresp++;
    if (warc_memstr((char *) rec, "WARC-Type: request", hdr_len, 18) != NULL)
      nreq++;
    freet(rec);
  }
  freet(data);
  if (nresource != 1 || nresp != 0 || nreq != 0)
    err = 1;
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

  if (argc < 1) {
    fprintf(stderr, "warc-rotate: needs a writable directory\n");
    return 1;
  }
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
        (const char *) body, sizeof(body), NULL, NULL, 200, 0, 0);
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
    data = warc_slurp(seg, &data_len);
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
        if (rlen != 0)
          err = 1;
        break;
      }
      if (warc_rec_split(rec, rlen, &hdr_len, &block_len) != 0)
        err = 1;
      if (first) { /* each segment leads with its own warcinfo */
        if (warc_memstr((char *) rec, "WARC-Type: warcinfo", hdr_len, 19) ==
            NULL)
          err = 1;
        first = 0;
      }
      freet(rec);
    }
    freet(data);
    if (first) /* empty segment */
      err = 1;
  }
  if (nseg < 2)
    err = 1;
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

  if (argc < 1) {
    fprintf(stderr, "warc-verbatim: needs a writable directory\n");
    return 1;
  }
  fconcat(path, sizeof(path), argv[0], "warc-verbatim.warc.gz");

  w = warc_open(opt, path);
  assertf(w != NULL);
  /* the body is the coded (gzip) octets, stored verbatim. */
  warc_write_transaction(
      w, "http://test.local/z.html", "127.0.0.1",
      "GET /z.html HTTP/1.1\r\nHost: test.local\r\n\r\n",
      "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Encoding: "
      "gzip\r\nTransfer-Encoding: chunked\r\nContent-Length: 999\r\n\r\n",
      (const char *) a_gz, sizeof(a_gz), NULL, NULL, 200, 0, 0);
  warc_close(w);

  data = warc_slurp(path, &data_len);
  assertf(data != NULL);
  p = data;
  end = data + data_len;

  while (p < end) {
    size_t rlen = 0, hdr_len = 0;
    long long block_len = 0;
    unsigned char *rec = warc_next_member(&p, end, &rlen);
    if (rec == NULL) {
      if (rlen != 0)
        err = 1;
      break;
    }
    if (warc_memstr((char *) rec, "msgtype=response", rlen, 16) == NULL) {
      freet(rec);
      continue;
    }
    if (warc_rec_split(rec, rlen, &hdr_len, &block_len) != 0) {
      err = 1;
      freet(rec);
      continue;
    }
    /* Assert: one Content-Encoding, no Transfer-Encoding, Content-Length =
       compressed size. */
    {
      const char *block = (char *) rec + hdr_len;
      const char *ce =
          warc_memstr(block, "Content-Encoding:", (size_t) block_len, 17);
      const char *hcl =
          warc_memstr(block, "Content-Length:", (size_t) block_len, 15);
      long long http_cl = -1;
      int nce = 0;
      const char *scan = ce;
      while (scan != NULL) {
        size_t rem = (size_t) block_len - (size_t) (scan - block);
        nce++;
        scan = warc_memstr(scan + 17, "Content-Encoding:", rem - 17, 17);
      }
      if (ce == NULL || strncasecmp(ce + 17, " gzip", 5) != 0 || nce != 1)
        err = 1;
      if (warc_memstr(block, "Transfer-Encoding:", (size_t) block_len, 18) !=
          NULL)
        err = 1;
      if (hcl == NULL || sscanf(hcl + 15, "%lld", &http_cl) != 1 ||
          http_cl != (long long) sizeof(a_gz))
        err = 1;
    }
    /* Stored block bytes equal the gzip input, and inflate to the plaintext. */
    {
      const char *bsep = warc_memstr((char *) rec + hdr_len, "\r\n\r\n",
                                     (size_t) block_len, 4);
      if (bsep == NULL)
        err = 1;
      else {
        size_t bodyoff = (size_t) (bsep - (char *) rec) + 4;
        size_t got = rlen - 4 - bodyoff; /* minus the record trailer */
        if (got != sizeof(a_gz) ||
            memcmp(rec + bodyoff, a_gz, sizeof(a_gz)) != 0)
          err = 1;
        else {
          const unsigned char *bp = rec + bodyoff;
          size_t plen = 0;
          unsigned char *plain = warc_next_member(&bp, bp + got, &plen);
          if (plain == NULL || plen != sizeof(a_plain) - 1 ||
              memcmp(plain, a_plain, plen) != 0)
            err = 1;
          freet(plain);
        }
      }
    }
    checked = 1;
    freet(rec);
  }
  freet(data);
  if (!checked)
    err = 1;
  printf("warc-verbatim: %s\n", err ? "FAIL" : "OK");
  return err;
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
      {"http://user:pass@www.example.com/y", "com,example)/y"},
      {"http://192.168.0.1/z", "192.168.0.1)/z"},
      {"http://[2001:db8::1]/w", "[2001:db8::1])/w"},
      {"http://sub.a.example.co.uk/deep?x=1#frag",
       "uk,co,example,a,sub)/deep?x=1"},
  };

  int err = 0;
  size_t i;
  (void) opt;
  (void) argc;
  (void) argv;
  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    char out[512];
    if (warc_surt(cases[i].url, out, sizeof(out)) != 0 ||
        strcmp(out, cases[i].want) != 0) {
      fprintf(stderr, "warc-surt: %s -> %s (want %s)\n", cases[i].url, out,
              cases[i].want);
      err = 1;
    }
  }
  printf("warc-surt: %s\n", err ? "FAIL" : "OK");
  return err;
}

/* The CDXJ offset is a uint64_t, and the tell behind it must carry the same
   width: `long`/ftell caps at 2GB, and is 32-bit even on 64-bit Windows, so
   every record past the cap would be indexed at a wrong offset (or at the last
   good one). Seeking alone costs no disk: the file stays empty. */
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

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "warc-offset: needs a writable directory\n");
    return 1;
  }
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
      if (warc_cdx_extent(got, sizeof(got), extents[i].length,
                          extents[i].offset) < 0 ||
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
      int n;

      memset(&tight, 'C', sizeof(tight));
      if (warc_cdx_extent(tight.buf, sizeof(tight.buf), 4294967296ULL,
                          1099511627776ULL) != -1) {
        fprintf(stderr, "warc-offset: a clipped CDXJ extent was not refused\n");
        err = 1;
      }
      if (memcmp(tight.canary, "CCCCCCCC", sizeof(tight.canary)) != 0) {
        fprintf(stderr, "warc-offset: the CDXJ extent overran its buffer\n");
        err = 1;
      }
      n = warc_cdx_extent(full, sizeof(full), UINT64_MAX, UINT64_MAX);
      if (n < 0 || (size_t) n != strlen(full)) {
        fprintf(stderr,
                "warc-offset: the widest CDXJ extent does not fit %d bytes\n",
                (int) sizeof(full));
        err = 1;
      }
    }
  }
  printf("warc-offset: %s\n", err ? "FAIL" : "OK");
  return err;
}

/* A URL longer than the old 1024-byte header-format buffer must still reach the
   archive: the record used to be abandoned whole, silently (#785). The sweep
   straddles the boundary so both the stack-buffer and the grow path run. */
static int st_warc_longurl(httrackp *opt, int argc, char **argv) {
  /* "WARC-Target-URI: " + CRLF costs 19 bytes, so the old buffer failed at
     1005; 9000 forces several reallocs within one record. */
  static const size_t lengths[] = {100, 1003, 1004, 1005, 1006, 2000, 9000};
  static const char resp_hdr[] =
      "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
  char path[HTS_URLMAXSIZE * 2];
  char body[64];
  warc_writer *w;
  FILE *fp;
  char *blob;
  LLint fsz;
  const char *at2;
  size_t i, n, nrec = 0;
  int err = 0;

  if (argc < 1) {
    fprintf(stderr, "warc-longurl: need a writable directory\n");
    return 1;
  }
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
    if (warc_write_transaction(w, uri, NULL, NULL, resp_hdr, body, strlen(body),
                               NULL, NULL, 200, 0, 0) != 0) {
      fprintf(stderr, "warc-longurl: write failed at length %d\n", (int) len);
      err = 1;
    }
    freet(uri);
  }
  warc_close(w);

  fsz = fsize_utf8(path);
  blob = (fsz > 0) ? malloct((size_t) fsz + 1) : NULL;
  if (blob == NULL) {
    fprintf(stderr, "warc-longurl: no archive written\n");
    return 1;
  }
  fp = FOPEN(path, "rb");
  n = (fp != NULL) ? fread(blob, 1, (size_t) fsz, fp) : 0;
  if (fp != NULL)
    fclose(fp);
  blob[n] = '\0';

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
  printf("warc-longurl: %s\n", err ? "FAIL" : "OK");
  return err;
}

/* End-to-end CDXJ: crawl a handful of records with --warc-cdx, then verify the
   .cdx is sorted, has exactly one line per response/revisit/resource (none for
   warcinfo/request), and each offset/length points at a gzip member that
   independently inflates to a record whose WARC-Target-URI matches the line. */
static int st_warc_cdx(httrackp *opt, int argc, char **argv) {
  char wpath[HTS_URLMAXSIZE], cpath[HTS_URLMAXSIZE];
  warc_writer *w;
  unsigned char *warc = NULL, *cdx = NULL;
  size_t warc_len = 0, cdx_len = 0;
  hts_boolean saved_cdx;
  int err = 0, nlines = 0, nm_lines = 0;
  const char *lp, *cend;
  char prev[2048];

  if (argc < 1) {
    fprintf(stderr, "warc-cdx: needs a writable directory\n");
    return 1;
  }
  fconcat(wpath, sizeof(wpath), argv[0], "warc-cdx.warc.gz");
  fconcat(cpath, sizeof(cpath), argv[0], "warc-cdx.cdx");
  saved_cdx = opt->warc_cdx;
  opt->warc_cdx = 1;
  w = warc_open(opt, wpath);
  assertf(w != NULL);
  warc_write_transaction(w, "http://www.example.com/one", "127.0.0.1",
                         "GET /one HTTP/1.1\r\nHost: www.example.com\r\n\r\n",
                         "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n",
                         "one body\n", 9, NULL, NULL, 200, 0, 0);
  warc_write_resource(w, "ftp://files.example.com/data.bin", "127.0.0.1",
                      "application/octet-stream", "\x00\x01\x02\x03", 4, NULL,
                      0);
  warc_write_transaction(w, "http://alpha.example.com/two", "127.0.0.1",
                         "GET /two HTTP/1.1\r\nHost: alpha.example.com\r\n\r\n",
                         "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n",
                         "two body\n", 9, NULL, NULL, 200, 0, 0);
  /* Same payload as /one at a new URL: identical-payload-digest revisit under
     OpenSSL, a full response otherwise; either way one index line. */
  warc_write_transaction(w, "http://zeta.example.com/dup", "127.0.0.1",
                         "GET /dup HTTP/1.1\r\nHost: zeta.example.com\r\n\r\n",
                         "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n",
                         "one body\n", 9, NULL, NULL, 200, 0, 0);
  /* A 304 declares no type, so the index line takes the caller's (#826). */
  warc_write_transaction(w, "http://nm.example.com/kept", "127.0.0.1",
                         "GET /kept HTTP/1.1\r\nHost: nm.example.com\r\n"
                         "If-Modified-Since: Mon, 01 Jan 2024 00:00:00 GMT\r\n"
                         "\r\n",
                         "HTTP/1.1 304 Not Modified\r\n\r\n", NULL, 0, NULL,
                         "text/html", 200, 1, 0);
  warc_close(w);
  opt->warc_cdx = saved_cdx;

  warc = warc_slurp(wpath, &warc_len);
  cdx = warc_slurp(cpath, &cdx_len);
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
    if (prev[0] != '\0' && strcmp(prev, line) > 0)
      err = 1; /* must be sorted */
    strlcpybuff(prev, line, sizeof(prev));
    j = strstr(line, "\"url\": \"");
    o = strstr(line, "\"offset\": \"");
    l = strstr(line, "\"length\": \"");
    if (j == NULL || o == NULL || l == NULL ||
        sscanf(o + 11, "%llu", &off) != 1 ||
        sscanf(l + 11, "%llu", &len) != 1) {
      err = 1;
      goto nextline;
    }
    us = j + 8;
    ue = strchr(us, '"');
    if (ue == NULL || (urllen = (size_t) (ue - us)) >= sizeof(url)) {
      err = 1;
      goto nextline;
    }
    memcpy(url, us, urllen);
    url[urllen] = '\0';
    if (strstr(url, "nm.example.com") != NULL) {
      nm_lines++;
      if (strstr(line, "\"mime\": \"text/html\"") == NULL)
        err = 1;
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
      if (warc_memstr((char *) rec, needle, rlen, strlen(needle)) == NULL)
        err = 1;
    }
    freet(rec);
  nextline:
    lp = eol ? eol + 1 : cend;
  }
  freet(warc);
  freet(cdx);
  if (nlines != 5 || nm_lines != 1)
    err = 1; /* 4 responses/revisits + 1 resource; no warcinfo/request */
  printf("warc-cdx: %d index lines: %s\n", nlines, err ? "FAIL" : "OK");
  return err;
}

static char st_cdx_log[2048];

/* Collect the "WARC:" diagnostics only, so an unrelated message cannot satisfy
   (or break) a silence assertion. */
static HTS_PRINTF_FUN(3, 0) void st_cdx_log_cb(httrackp *opt, int type,
                                               const char *format,
                                               va_list args) {
  char line[512];
  (void) opt;
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
                           "body\n", 5, NULL, NULL, 200, 0, 0);
  }
}

/* Every way warc_cdx_flush can fail to leave a usable index beside the archive
   it just replaced. The empty-index warning must stay silent for a first run,
   which has no previous index to invalidate (#1041). */
static int st_warc_cdx_errors(httrackp *opt, int argc, char **argv) {
  char path[HTS_URLMAXSIZE], cdx[HTS_URLMAXSIZE];
  static const char stale[] = "no record was indexed";
  static const char unwritable[] = "could not write the index";
  hts_boolean saved_cdx;
  void *saved_state;
  warc_writer *w;
  int err = 0;

  if (argc < 1) {
    fprintf(stderr, "warc-cdx-errors: needs a writable directory\n");
    return 1;
  }
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

  /* A previous archive and its index: this run swapped over the first, so the
     second now describes an archive that is gone. */
  fconcat(path, sizeof(path), argv[0], "cdxerr-prev.warc.gz");
  fconcat(cdx, sizeof(cdx), argv[0], "cdxerr-prev.cdx");
  st_cdx_leave(path, "previous archive");
  st_cdx_leave(cdx, "com,example)/ 20240101000000 {}\n");
  w = warc_open(opt, path);
  assertf(w != NULL);
  warc_close(w);
  err |= st_cdx_logged("swap over a previous archive", stale);

  /* Mirror case: written in place, then abandoned, so the archive is clobbered
     and the index beside it describes what used to be there. */
  fconcat(path, sizeof(path), argv[0], "cdxerr-abort.warc.gz");
  fconcat(cdx, sizeof(cdx), argv[0], "cdxerr-abort.cdx");
  st_cdx_leave(cdx, "com,example)/ 20240101000000 {}\n");
  w = warc_open(opt, path);
  assertf(w != NULL);
  opt->state.warc = w;
  warc_abort_opt(opt);
  err |= st_cdx_logged("abandoned in-place run", stale);

  /* Same, with no index left behind: the message would name a file that never
     existed, which is the false alarm the gate is there to avoid. */
  fconcat(path, sizeof(path), argv[0], "cdxerr-abort-noidx.warc.gz");
  w = warc_open(opt, path);
  assertf(w != NULL);
  opt->state.warc = w;
  warc_abort_opt(opt);
  err |= st_cdx_logged("abandoned run with no index", NULL);

  /* An archive that never opened replaced nothing, so the index beside it
     still describes what is there: the failed open is the only error. */
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
  /* /dev/full fails every write with ENOSPC. An index below one stdio buffer
     never reaches the device until fclose, and a larger one errors before it,
     which is the difference the two messages carry. */
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
  printf("warc-cdx-errors: %s\n", err ? "FAIL" : "OK");
  return err;
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

/* memmem(): a NUL byte anywhere in the archive would cut strstr() short. */
static hts_boolean st_blob_has(const unsigned char *blob, size_t len,
                               const char *s) {
  const size_t n = strlen(s);
  size_t i;

  for (i = 0; n <= len && i <= len - n; i++) {
    if (memcmp(blob + i, s, n) == 0)
      return HTS_TRUE;
  }
  return HTS_FALSE;
}

/* The archive on disk must hold `want`'s run, and no trace of the others. */
static int st_warc_holds(const char *name, const char *path, const char *want,
                         const char *absent1, const char *absent2) {
  const char *const absent[2] = {absent1, absent2};
  size_t len = 0, i;
  unsigned char *blob = warc_slurp(path, &len);
  int err = 0;

  if (blob == NULL) {
    fprintf(stderr, "warc-teardown: %s: cannot read %s\n", name, path);
    return 1;
  }
  /* shape, not validity: the first record's version line, nothing more */
  if (len < 8 || memcmp(blob, "WARC/1.1", 8) != 0) {
    fprintf(stderr, "warc-teardown: %s: %s does not open on a WARC record\n",
            name, path);
    err++;
  }
  if (!st_blob_has(blob, len, want)) {
    fprintf(stderr, "warc-teardown: %s: %s lost the run holding \"%s\"\n", name,
            path, want);
    err++;
  }
  for (i = 0; i < 2; i++) {
    if (st_blob_has(blob, len, absent[i])) {
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

  /* stands in for the per-mirror memset in hts_create_opt(): this test reuses
     the dispatcher's opt where the engine always has a fresh one */
  opt->state.warc = NULL;
  st_warc_emit(opt, run1);
  warc_close_opt(opt);
  if (fsize_utf8(path) <= 0) {
    fprintf(stderr, "warc-teardown: %s: the hook opened no archive\n", name);
    return 1;
  }

  /* a previous archive is now there, so this run goes through the temporary */
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
  /* abort keeps run 1, close commits run 2; the late body guards against an
     append, no reopen can reach the archive without losing that run first */
  err += abort_run ? st_warc_holds(name, path, run1, run2, late)
                   : st_warc_holds(name, path, run2, run1, late);
  return err;
}

// -#test=warc-teardown <dir>: teardown finalizes the slots left in flight, so
// back_finalize emits after warc_close_opt/warc_abort_opt has run (#1060).
static int st_warc_teardown(httrackp *opt, int argc, char **argv) {
  char path[HTS_URLMAXSIZE], saved_file[HTS_URLMAXSIZE];
  void *saved_state;
  int err = 0;

  if (argc < 1) {
    fprintf(stderr, "warc-teardown: needs a writable directory\n");
    return 1;
  }
  saved_state = opt->state.warc;
  strlcpybuff(saved_file, StringBuff(opt->warc_file), sizeof(saved_file));

  fconcat(path, sizeof(path), argv[0], "teardown-close.warc");
  err += st_warc_teardown_case(opt, "close", path, HTS_FALSE);
  fconcat(path, sizeof(path), argv[0], "teardown-abort.warc");
  err += st_warc_teardown_case(opt, "abort", path, HTS_TRUE);

  StringCopy(opt->warc_file, saved_file);
  opt->state.warc = saved_state;
  printf("warc-teardown: %s\n", err ? "FAIL" : "OK");
  return err;
}

#if HTS_USEOPENSSL
/* Lowercase-hex SHA-256 of n bytes into out[65]; 1 on success. */
static int wacz_test_sha256(const void *p, size_t n, char out[65]) {
  EVP_MD_CTX *c = EVP_MD_CTX_new();
  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned int mdlen = 0, i;
  static const char hx[] = "0123456789abcdef";
  int ok;
  if (c == NULL)
    return 0;
  ok = EVP_DigestInit_ex(c, EVP_sha256(), NULL) == 1 &&
       (n == 0 || EVP_DigestUpdate(c, p, n) == 1) &&
       EVP_DigestFinal_ex(c, md, &mdlen) == 1 && mdlen == 32;
  EVP_MD_CTX_free(c);
  if (!ok)
    return 0;
  for (i = 0; i < 32; i++) {
    out[i * 2] = hx[md[i] >> 4];
    out[i * 2 + 1] = hx[md[i] & 0x0F];
  }
  out[64] = '\0';
  return 1;
}

/* One unzipped WACZ member: name, raw bytes, and the ZIP compression method. */
typedef struct {
  char name[256];
  unsigned char *data;
  size_t len;
  int method;
} wacz_entry;

/* Package a 2-record WARC as a WACZ, then unzip it in-process and assert the
   fixed layout, STORE-mode entries, recomputing sha256 digests, the digest
   chain, and the pages.jsonl header. */
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

  if (argc < 1) {
    fprintf(stderr, "warc-wacz: needs a writable directory\n");
    return 1;
  }
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
                         "<html>home</html>\n", 18, NULL, NULL, 200, 0, 0);
  warc_write_transaction(
      w, "http://www.example.com/data.bin", "127.0.0.1",
      "GET /data.bin HTTP/1.1\r\nHost: www.example.com\r\n\r\n",
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/octet-stream\r\n\r\n",
      "\x00\x01\x02\x03\x04", 5, NULL, NULL, 200, 0, 0);
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
        err = 1;
        break;
      }
      if (e->len > 0 &&
          unzReadCurrentFile(uf, e->data, (unsigned) e->len) != (int) e->len)
        err = 1;
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
  if (!have_archive || !have_index || pages == NULL || dp == NULL ||
      dig == NULL || !all_store)
    err = 1;

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
      if (body == NULL || blen == 0 ||
          warc_memstr(body, "\"url\": ", blen, 7) == NULL ||
          warc_memstr(body, "\"ts\": ", blen, 6) == NULL)
        err = 1;
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
      if (strstr(json, "\"profile\": \"data-package\"") == NULL ||
          strstr(json, "\"wacz_version\": \"") == NULL)
        err = 1;
      p = json;
      while ((p = strstr(p, "\"path\": \"")) != NULL) {
        char path[256], want[80], got[65];
        const char *pe, *h;
        size_t plen;
        p += 9;
        pe = strchr(p, '"');
        if (pe == NULL || (plen = (size_t) (pe - p)) >= sizeof(path)) {
          err = 1;
          break;
        }
        memcpy(path, p, plen);
        path[plen] = '\0';
        h = strstr(pe, "\"hash\": \"sha256:");
        if (h == NULL || sscanf(h + 16, "%79[0-9a-f]", want) != 1) {
          err = 1;
          break;
        }
        for (i = 0; i < nent; i++)
          if (strcmp(ent[i].name, path) == 0)
            break;
        if (i == nent || !wacz_test_sha256(ent[i].data, ent[i].len, got) ||
            strcmp(got, want) != 0)
          err = 1;
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
    if (djson == NULL || !wacz_test_sha256(dp->data, dp->len, dphex)) {
      err = 1;
    } else {
      memcpy(djson, dig->data, dig->len);
      djson[dig->len] = '\0';
      if (strstr(djson, "\"path\": \"datapackage.json\"") == NULL)
        err = 1;
      h = strstr(djson, "\"hash\": \"sha256:");
      if (h == NULL || sscanf(h + 16, "%79[0-9a-f]", want) != 1 ||
          strcmp(want, dphex) != 0)
        err = 1;
    }
    freet(djson);
  }

  for (i = 0; i < nent; i++)
    freet(ent[i].data);

  /* #522-class: a failed re-package must leave the existing .wacz untouched.
     Drop the .cdx and re-run empty so packaging fails on the missing index. */
  good_size = fsize(waczpath);
  if (good_size <= 0)
    err = 1;
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
static hts_boolean sf_try_put(const char *dir, const char *rel,
                              const void *data, size_t len) {
  char BIGSTK path[HTS_URLMAXSIZE * 2];
  char catbuff[CATBUFF_SIZE];
  FILE *fp;

  fconcat(path, sizeof(path), dir, rel);
  structcheck_utf8(path);
  fp = FOPEN(fconv(catbuff, sizeof(catbuff), path), "wb");
  if (fp == NULL)
    return HTS_FALSE;
  assertf(len == 0 || hts_fwrite_exact(data, len, fp));
  fclose(fp);
  return HTS_TRUE;
}

/* Expand a fixture: \001<ref>\002 becomes <ref> plus this run's mark for it.
   The secret is random per run, so a fixture cannot spell a mark itself. */
static void sf_expand_fixture(httrackp *opt, const char *in, size_t len,
                              String *out) {
  size_t i, start = 0;

  StringClear(*out);
  for (i = 0; i < len; i++) {
    if (in[i] == '\001') {
      start = StringLength(*out);
    } else if (in[i] == '\002') {
      char mark[SINGLEFILE_MARK_MAX];

      StringCat(*out,
                singlefile_mark(opt, mark, sizeof(mark), SINGLEFILE_CLASS_ANY,
                                StringLength(*out) - start));
    } else {
      StringAddchar(*out, in[i]);
    }
  }
}

static void sf_put(const char *dir, const char *rel, const void *data,
                   size_t len) {
  assertf(sf_try_put(dir, rel, data, len));
}

/* sf_put() for a text fixture, expanding its \001<ref>\002 delimiters. Binary
   fixtures must not go through here: sf_png carries those very bytes. */
static size_t sf_put_marked(httrackp *opt, const char *dir, const char *rel,
                            const void *data, size_t len) {
  String body = STRING_EMPTY;
  size_t written;

  sf_expand_fixture(opt, (const char *) data, len, &body);
  assertf(sf_try_put(dir, rel, StringBuff(body), StringLength(body)));
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
  while (p[n] != '\0' && (isalnum((unsigned char) p[n]) || p[n] == '+' ||
                          p[n] == '/' || p[n] == '='))
    n++;
  *len = n;
  return p;
}

/* Independent base64 decoder: the round-trip check must not lean on code64().
 */
static unsigned char *sf_unb64(const char *s, size_t len, size_t *outlen) {
  static const char alpha[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  unsigned char *out = (unsigned char *) malloct(len / 4 * 3 + 4);
  unsigned int acc = 0;
  size_t i, n = 0;
  int bits = 0;

  if (out == NULL)
    return NULL;
  for (i = 0; i < len; i++) {
    const char *const p = s[i] != '\0' ? strchr(alpha, s[i]) : NULL;

    if (s[i] == '=')
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
  raw = sf_unb64(b64, len, &dlen);
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
    /* A fragment is document text the mark never covers, so it reaches the
       data: URI exactly as written, in the quoting the author gave it. */
    "b { background: url('\001img/sprite.svg\002#w x'); }\n"
    "i { background: url('\001img/sprite.svg\002#lt<s>gt'); }\n"
    "u { background: url(\"\001img/sprite.svg\002#z'(\\\xC3\xA9\"); }</style>\n"
    "</head><body>\n"
    "<img src=\"\001img/a%20b.png\002\" srcset=\"\001img/a%20b.png\002 "
    "1x, \001img/big.png\002 2x\">\n"
    "<link rel=\"icon\" href=\"\001icon.png\002\">\n"
    "<link rel=\"preload\" as=\"font\" href=\"\001font/f.woff2\002\">\n"
    "<img src=\"data:image/gif;base64,QUJD\">\n"
    /* Each has a real file where its guard's removal would land it; without
       that they stay links either way, the target merely being absent. */
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
    /* The second is what bites: drop the clamp and its leading ".." lands it
       back on <root>/img/a b.png. The first can only 404 either way. */
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
  /* The over-cap url() is what drives the rebase fallback: a reference an
     inlined stylesheet could not embed has to come out relative to the page,
     not to the stylesheet, or it dangles. */
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
  sf_put_marked(opt, root, "page.html", sf_page, sizeof(sf_page) - 1);
  sf_put_marked(opt, root, "deep/sub/page.html", deep, sizeof(deep) - 1);
  sf_put(root, "other.html", "<html>o</html>", 14);
  sf_put_marked(opt, root, "css/main.css", css, sizeof(css) - 1);
  sf_put_marked(opt, root, "css/sub/nested.css", nested, sizeof(nested) - 1);
  sf_put_marked(opt, root, "css/sub/two.css", two, sizeof(two) - 1);
  sf_put(root, "js/app.js", js, sizeof(js) - 1);
  sf_put(root, "img/a b.png", sf_png, SF_PNG_LEN);
  sf_put(root, "img/big.png", big, sizeof(big));
  sf_put(root, "img/sprite.svg", sf_svg, sizeof(sf_svg) - 1);
  sf_put(root, "img/big-sprite.svg", big, sizeof(big));
  sf_put(root, "img/b&c%d#e.png", big, sizeof(big));
  sf_put(root, "img/in.png", sf_png, SF_PNG_LEN);
  sf_put(root, "img/po.png", sf_png, SF_PNG_LEN);
  sf_put(root, "img/sv.png", sf_png, SF_PNG_LEN);
  sf_put(root, "img/bg.png", sf_png, SF_PNG_LEN);
  sf_put(root, "img/ph.png", sf_png, SF_PNG_LEN);
  sf_put(root, "img/lz.png", sf_png, SF_PNG_LEN);
  sf_put(root, "img/lz2.png", sf_png, SF_PNG_LEN);
  sf_put(root, "img/low.png", sf_png, SF_PNG_LEN);
  sf_put(root, "img/ob.png", sf_png, SF_PNG_LEN);
  sf_put(root, "img/em.png", sf_png, SF_PNG_LEN);
  sf_put(root, "icon.png", sf_png, SF_PNG_LEN);
  sf_put(root, "font/f.woff2", "wOF2\x00\x01", 6);
  /* Where a guard's removal would land each reference that has to stay a
     link. The colon-bearing two are impossible on Windows, where the scheme
     guard then only gets the weaker "the link survived" check. */
  sf_put(root, "img/never.png", sf_png, SF_PNG_LEN);
  sf_put(root, "example.com/x.png", sf_png, SF_PNG_LEN);
  sf_colon_ok =
      sf_try_put(root, "http:/example.com/x.png", sf_png, SF_PNG_LEN) &&
      sf_try_put(root, "data:text/css;base64,QUJD", "p{}", 3);
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
    sf_put_marked(opt, root, "wide.html", StringBuff(wide), StringLength(wide));
    StringFree(wide);
  }
  sf_put(root, "v.mp4",
         "\x00\x00\x00\x18"
         "ftypisom",
         12);
}

/* -#test=singlefile <dir>: rewrite a hand-built mirror and check what gets
   inlined, what must keep its link, the per-asset cap, and idempotence. */
static int st_singlefile(httrackp *opt, int argc, char **argv) {
  char BIGSTK root[HTS_URLMAXSIZE];
  char BIGSTK page[HTS_URLMAXSIZE * 2];
  const LLint saved_cap = opt->single_file_max_size;
  char *out, *css, *nested;
  size_t outlen = 0, len = 0;

  if (argc < 1) {
    fprintf(stderr, "singlefile: needs a writable directory\n");
    return 1;
  }
  sf_err = 0;
  sf_put(argv[0], "escape.png", sf_png,
         SF_PNG_LEN); /* just outside the mirror */
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
  /* The class gate, not the attribute name, is what keeps a page out. */
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

  /* Fragments: the mark covers the reference only, so what followed it comes
     back byte-identical -- including the escapes the document already carried
     -- while the query rode inside the mark and went with it. */
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

  /* Nothing inlines, so the rewriter must be byte transparent. Assert on its
     output: the file it declined to write could not have changed regardless. */
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

    sf_expand_fixture(opt, sf_page, sizeof(sf_page) - 1, &marked);
    (void) singlefile_rewrite_html(opt, root, page, StringBuff(marked),
                                   StringLength(marked),
                                   SINGLEFILE_MAX_PAGE_SIZE, &verbatim);
    /* Mark-transparent, not byte-transparent: a reference that cannot be
       inlined loses its mark and keeps everything else. */
    sf_check(StringLength(verbatim) < StringLength(marked),
             "a page with nothing to inline kept its marks");
    sf_check(strstr(StringBuff(verbatim), singlefile_intro(opt)) == NULL,
             "an un-inlinable reference kept its mark");
    StringFree(marked);
    StringFree(verbatim);
  }
  (void) outlen;

  /* A mark the pass cannot parse stays in the page as text, so whatever
     singlefile_mark writes it has to read back. emit_max restates htsparse's
     worst case for one reference independently of SINGLEFILE_MAX_SPAN, and the
     spans straddle it. */
  {
    const size_t emit_max =
        HTS_URLMAXSIZE * 2 *
        (HTS_HTMLESCAPE_FULL_MAXEXP + HTS_HTMLESCAPE_MAXEXP);
    const size_t spans[] = {4097, emit_max, emit_max + 1, emit_max * 4};
    char mark[SINGLEFILE_MARK_MAX];
    size_t k;

    sf_check(singlefile_mark(opt, mark, sizeof(mark), SINGLEFILE_CLASS_ANY,
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
      marklen = strlen(singlefile_mark(opt, mark, sizeof(mark),
                                       SINGLEFILE_CLASS_ANY, spans[k]));
      StringCat(marked, mark);
      StringCat(marked, "\">\n");
      StringClear(got);
      (void) singlefile_rewrite_html(opt, root, page, StringBuff(marked),
                                     StringLength(marked),
                                     SINGLEFILE_MAX_PAGE_SIZE, &got);
      sf_check(strstr(StringBuff(got), singlefile_intro(opt)) == NULL,
               "a mark the emitter wrote stayed in the page as text");
      sf_check(hts_memstr(StringBuff(got), StringLength(got), pad, spans[k]) !=
                   NULL,
               "the reference the mark measured was lost");
      /* Nothing resolves here, so the mark is the only thing that may go: a
         length check catches what a presence check cannot. */
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
        (void) singlefile_rewrite_html(opt, root, page, StringBuff(marked),
                                       StringLength(marked),
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
        (void) singlefile_rewrite_html(opt, root, page, StringBuff(marked),
                                       StringLength(marked),
                                       SINGLEFILE_MAX_PAGE_SIZE, &got);
        sf_check(StringLength(got) == StringLength(marked),
                 "a wrapping digit run was read as a mark");
      }
      freet(pad);
      StringFree(marked);
      StringFree(got);
    }
  }

  /* singlefile_may_mark searches with hts_memstr, which finds nothing for an
     empty needle; the intro is fixed-width, so that case cannot arise. */
  {
    String probe = STRING_EMPTY;

    sf_check(strlen(singlefile_intro(opt)) == SINGLEFILE_INTRO_LEN,
             "the intro is not the fixed-width string may_mark assumes");
    sf_check(singlefile_may_mark(opt, "", 0), "an empty body was refused");
    StringClear(probe);
    StringMemcat(probe, "x\0", 2); /* bodies carry NULs; strstr would stop */
    StringCat(probe, singlefile_intro(opt));
    sf_check(!singlefile_may_mark(opt, StringBuff(probe), StringLength(probe)),
             "an intro ending the body, past a NUL, was missed");
    sf_check(
        singlefile_may_mark(opt, StringBuff(probe), StringLength(probe) - 1),
        "a truncated intro was read as one");
    StringFree(probe);
  }

  /* The per-page budget against a self-importing stylesheet. The large-budget
     run is the control: it proves the fan-out is real, so the small one was
     cut short by the budget and not by the fixture. */
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
    css_len = sf_put_marked(opt, root, "b.css", bomb_css, sizeof(bomb_css) - 1);
    fconcat(page, sizeof(page), root, "page.html");
    opt->single_file_max_size = 1024 * 1024;
    StringClear(small);
    StringClear(large);
    sf_expand_fixture(opt, bomb_html, sizeof(bomb_html) - 1, &bomb);
    (void) singlefile_rewrite_html(opt, root, page, StringBuff(bomb),
                                   StringLength(bomb), (LLint) css_len * 3,
                                   &small);
    (void) singlefile_rewrite_html(opt, root, page, StringBuff(bomb),
                                   StringLength(bomb), SINGLEFILE_MAX_PAGE_SIZE,
                                   &large);
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
  printf("singlefile: %s\n", sf_err ? "FAIL" : "OK");
  return sf_err;
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
    {"warc-teardown", "<dir>",
     "a closed or abandoned archive is not reopened by a late transaction",
     st_warc_teardown},
#if HTS_USEOPENSSL
    {"warc-wacz", "<dir>", "--wacz package: layout, STORE mode, sha256 digests",
     st_warc_wacz},
#endif
    {"singlefile", "<dir>",
     "--single-file: what is inlined, the per-asset cap, idempotence",
     st_singlefile},
    {NULL, NULL, NULL, NULL},
};
