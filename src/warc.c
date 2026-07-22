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
/* HTTrack WARC/1.1 output writer (ISO 28500). See warc.h.
   Strategy B (see design): the response record stores the decoded body and a
   normalized header block (Content-Encoding/Transfer-Encoding stripped,
   Content-Length rewritten to the decoded length). Valid, replayable WARC. */
/* ------------------------------------------------------------ */

#define HTS_INTERNAL_BYTECODE

#include "warc.h"

#include "htscore.h"
#include "htslib.h"
#include "htstools.h"
#include "htssafe.h"
#include "htszlib.h"
#include "coucal/coucal.h"

#include <stdarg.h>
#include <stdint.h>
#include <time.h>

#if HTS_USEOPENSSL
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

/* opt->state.warc value meaning "open failed once, do not retry". */
#define WARC_DISABLED ((void *) ~(uintptr_t) 0)

struct warc_writer {
  FILE *f;
  int gz;          /* 1: one gzip member per record; 0: raw */
  uint64_t offset; /* running byte offset (member starts, for a future index) */
  uint64_t counter; /* monotonic record counter */
  uint64_t rng;     /* PRNG state for the UUID fallback */
  char info_id[64]; /* warcinfo WARC-Record-ID, referenced by every record */
  coucal seen;      /* base32 payload digest -> "uri\001date" (revisit dedup) */
};

/* ---- growable byte buffer (overflow-safe, project allocators) ---- */

typedef struct {
  char *data;
  size_t len;
  size_t cap;
} wbuf;

static void wbuf_free(wbuf *b) {
  freet(b->data);
  b->len = b->cap = 0;
}

/* Append n bytes; returns 0 on success, -1 on OOM/overflow. */
static int wbuf_add(wbuf *b, const void *p, size_t n) {
  if (n > (size_t) -1 - b->len)
    return -1;
  if (b->len + n > b->cap) {
    size_t ncap = b->cap ? b->cap : 256;
    char *nd;
    while (ncap < b->len + n) {
      if (ncap > (size_t) -1 / 2)
        return -1;
      ncap *= 2;
    }
    nd = realloct(b->data, ncap);
    if (nd == NULL)
      return -1;
    b->data = nd;
    b->cap = ncap;
  }
  memcpy(b->data + b->len, p, n);
  b->len += n;
  return 0;
}

static int wbuf_puts(wbuf *b, const char *s) {
  return wbuf_add(b, s, strlen(s));
}

static int wbuf_printf(wbuf *b, const char *fmt, ...) {
  char tmp[1024];
  int n;
  va_list ap;
  va_start(ap, fmt);
  n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
  va_end(ap);
  if (n < 0 || (size_t) n >= sizeof(tmp))
    return -1;
  return wbuf_add(b, tmp, (size_t) n);
}

/* ---- gzip-per-record member writer (mirrors ae_write_packed) ---- */

typedef struct {
  warc_writer *w;
  z_stream strm;
  int active; /* deflate stream initialized */
} member;

static int member_begin(member *m, warc_writer *w) {
  m->w = w;
  m->active = 0;
  if (w->gz) {
    memset(&m->strm, 0, sizeof(m->strm));
    /* windowBits=31 => full RFC1952 gzip member */
    if (deflateInit2(&m->strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 31, 8,
                     Z_DEFAULT_STRATEGY) != Z_OK)
      return -1;
    m->active = 1;
  }
  return 0;
}

static int member_write(member *m, const void *p, size_t n) {
  if (!m->w->gz)
    return (n == 0 || fwrite(p, 1, n, m->w->f) == n) ? 0 : -1;
  m->strm.next_in = (const Bytef *) p;
  while (n > 0) {
    unsigned char out[8192];
    size_t got;
    uInt chunk = (n > (uInt) -1) ? (uInt) -1 : (uInt) n;
    m->strm.avail_in = chunk;
    do {
      m->strm.next_out = out;
      m->strm.avail_out = sizeof(out);
      if (deflate(&m->strm, Z_NO_FLUSH) != Z_OK)
        return -1;
      got = sizeof(out) - m->strm.avail_out;
      if (got > 0 && fwrite(out, 1, got, m->w->f) != got)
        return -1;
    } while (m->strm.avail_out == 0);
    n -= chunk;
  }
  return 0;
}

static int member_end(member *m) {
  int rc = 0;
  if (m->active) {
    unsigned char out[8192];
    int zerr;
    m->strm.avail_in = 0;
    do {
      m->strm.next_out = out;
      m->strm.avail_out = sizeof(out);
      zerr = deflate(&m->strm, Z_FINISH);
      {
        size_t got = sizeof(out) - m->strm.avail_out;
        if (got > 0 && fwrite(out, 1, got, m->w->f) != got)
          rc = -1;
      }
    } while (zerr == Z_OK);
    if (zerr != Z_STREAM_END)
      rc = -1;
    deflateEnd(&m->strm);
    m->active = 0;
  }
  return rc;
}

/* ---- SHA-1 + Base32 (digests are OpenSSL-only; omitted otherwise) ---- */

#if HTS_USEOPENSSL
/* Streaming SHA-1 over the block (all regions) and the payload (body only). */
typedef struct {
  EVP_MD_CTX *block;
  EVP_MD_CTX *payload;
} digester;

static void base32_20(const unsigned char in[20], char out[33]) {
  static const char a[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
  int i, o = 0;
  uint64_t buf = 0;
  int bits = 0;
  for (i = 0; i < 20; i++) {
    buf = (buf << 8) | in[i];
    bits += 8;
    while (bits >= 5) {
      bits -= 5;
      out[o++] = a[(buf >> bits) & 0x1F];
    }
  }
  out[o] = '\0'; /* 20 bytes => exactly 32 base32 chars, no padding */
}
#endif

/* Stream a block to a sink. When http_section, the HTTP header bytes (may be
   empty) plus their terminating CRLF are emitted first; the separator is bound
   to http_section, not to http_hdr being non-NULL, so an empty header still
   emits (and is counted in) the 2-byte separator (F3). The on-disk body is
   written as EXACTLY body_len octets — capped if the file grew, zero-padded if
   it shrank — so the declared Content-Length always equals the bytes written
   across every pass (F2). region 0=header, 1=body. Returns 0 on success. */
typedef int (*warc_sink)(void *ctx, int region, const void *p, size_t n);

static int stream_body_pad(warc_sink sink, void *ctx, size_t remaining) {
  static const char zeros[4096] = {0};
  while (remaining > 0) {
    size_t chunk = (remaining < sizeof(zeros)) ? remaining : sizeof(zeros);
    if (sink(ctx, 1, zeros, chunk) != 0)
      return -1;
    remaining -= chunk;
  }
  return 0;
}

static int stream_block(int http_section, const char *http_hdr,
                        size_t http_hdr_len, int has_body, const char *body,
                        size_t body_len, const char *body_path, warc_sink sink,
                        void *ctx) {
  if (http_section) {
    if (http_hdr != NULL && http_hdr_len > 0 &&
        sink(ctx, 0, http_hdr, http_hdr_len) != 0)
      return -1;
    if (sink(ctx, 0, "\r\n", 2) != 0)
      return -1;
  }
  if (has_body) {
    if (body != NULL) {
      if (body_len > 0 && sink(ctx, 1, body, body_len) != 0)
        return -1;
    } else if (body_path != NULL) {
      char catbuff[CATBUFF_SIZE];
      size_t remaining = body_len;
      FILE *fp = FOPEN(fconv(catbuff, sizeof(catbuff), body_path), "rb");
      if (fp == NULL)
        return -1;
      while (remaining > 0) {
        char b[32768];
        size_t want = (remaining < sizeof(b)) ? remaining : sizeof(b);
        size_t nl = fread(b, 1, want, fp);
        if (nl == 0)
          break; /* short file: pad below so written == declared */
        if (sink(ctx, 1, b, nl) != 0) {
          fclose(fp);
          return -1;
        }
        remaining -= nl;
      }
      fclose(fp);
      if (stream_body_pad(sink, ctx, remaining) != 0)
        return -1;
    }
  }
  return 0;
}

#if HTS_USEOPENSSL
static int digest_sink(void *ctx, int region, const void *p, size_t n) {
  digester *d = (digester *) ctx;
  if (d->block != NULL && EVP_DigestUpdate(d->block, p, n) != 1)
    return -1;
  if (region == 1 && d->payload != NULL &&
      EVP_DigestUpdate(d->payload, p, n) != 1)
    return -1;
  return 0;
}
#endif

static int write_sink(void *ctx, int region, const void *p, size_t n) {
  (void) region;
  return member_write((member *) ctx, p, n);
}

/* Base32 SHA-1 of a transaction payload (body only), for revisit dedup.
   Returns 1 and fills out[33] on success, 0 without OpenSSL or on error. */
static int payload_digest_b32(const char *body, size_t body_len,
                              const char *body_path, char out[33]) {
#if HTS_USEOPENSSL
  digester d;
  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned int mdlen = 0;
  int ok;
  d.block = NULL;
  d.payload = EVP_MD_CTX_new();
  if (d.payload == NULL)
    return 0;
  if (EVP_DigestInit_ex(d.payload, EVP_sha1(), NULL) != 1) {
    EVP_MD_CTX_free(d.payload);
    return 0;
  }
  ok = stream_block(0, NULL, 0, 1, body, body_len, body_path, digest_sink,
                    &d) == 0 &&
       EVP_DigestFinal_ex(d.payload, md, &mdlen) == 1 && mdlen == 20;
  EVP_MD_CTX_free(d.payload);
  if (!ok)
    return 0;
  base32_20(md, out);
  return 1;
#else
  (void) body;
  (void) body_len;
  (void) body_path;
  (void) out;
  return 0;
#endif
}

/* ---- misc record helpers ---- */

static void warc_fill_random(warc_writer *w, unsigned char *b, size_t n) {
  size_t i;
#if HTS_USEOPENSSL
  if (n <= (size_t) 0x7fffffff && RAND_bytes(b, (int) n) == 1)
    return;
#endif
  for (i = 0; i < n; i++) {
    w->rng ^= w->rng << 13;
    w->rng ^= w->rng >> 7;
    w->rng ^= w->rng << 17;
    b[i] = (unsigned char) (w->rng >> 24);
  }
}

/* urn:uuid: v4-shaped record id (uniqueness within the run is what matters). */
static void warc_make_id(warc_writer *w, char out[64]) {
  unsigned char b[16];
  w->counter++;
  warc_fill_random(w, b, sizeof(b));
  b[6] = (unsigned char) ((b[6] & 0x0F) | 0x40);
  b[8] = (unsigned char) ((b[8] & 0x3F) | 0x80);
  snprintf(out, 64,
           "<urn:uuid:%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-"
           "%02x%02x%02x%02x%02x%02x>",
           b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9], b[10],
           b[11], b[12], b[13], b[14], b[15]);
}

static void warc_now_iso8601(char out[32]) {
  time_t t = time(NULL);
  struct tm tmv;
#if defined(_WIN32)
  struct tm *g = gmtime(&t);
  if (g != NULL)
    tmv = *g;
  else
    memset(&tmv, 0, sizeof(tmv));
#else
  if (gmtime_r(&t, &tmv) == NULL)
    memset(&tmv, 0, sizeof(tmv));
#endif
  strftime(out, 32, "%Y-%m-%dT%H:%M:%SZ", &tmv);
}

/* Case-insensitive "does line start with name:" test. */
static int header_is(const char *line, size_t line_len, const char *name) {
  size_t nl = strlen(name);
  if (line_len < nl + 1)
    return 0;
  if (strncasecmp(line, name, nl) != 0)
    return 0;
  return line[nl] == ':';
}

/* Build the normalized HTTP header block from raw resp_hdr into out (no
   trailing CRLF terminator). Always drops Transfer-Encoding (hop-by-hop).
   When set_cl>=0, also drops Content-Encoding and the original Content-Length
   and appends "Content-Length: <set_cl>". Returns 0 on success. */
static int normalize_http_headers(const char *resp_hdr, long long set_cl,
                                  wbuf *out) {
  const char *p = resp_hdr;
  int first = 1;
  if (resp_hdr == NULL)
    return -1;
  while (*p != '\0') {
    const char *eol = strchr(p, '\n');
    size_t len = (eol != NULL) ? (size_t) (eol - p) : strlen(p);
    size_t raw = len;
    if (len > 0 && p[len - 1] == '\r')
      len--; /* strip CR; re-added as CRLF below */
    if (len == 0)
      break; /* blank line: end of headers */
    if (first) {
      first = 0; /* status line: keep verbatim */
    } else if (header_is(p, len, "Transfer-Encoding")) {
      goto next;
    } else if (set_cl >= 0 && (header_is(p, len, "Content-Encoding") ||
                               header_is(p, len, "Content-Length"))) {
      goto next;
    }
    if (wbuf_add(out, p, len) != 0 || wbuf_add(out, "\r\n", 2) != 0)
      return -1;
  next:
    if (eol == NULL)
      break;
    p = eol + 1;
    (void) raw;
  }
  if (set_cl >= 0 && wbuf_printf(out, "Content-Length: %lld\r\n", set_cl) != 0)
    return -1;
  return 0;
}

/* Emit one full WARC record. When http_section, the block carries an HTTP
   header block (http_hdr, possibly empty) + a CRLF separator; body follows when
   has_body. block_len is derived here (single source of truth: separator and
   payload are counted exactly as stream_block emits them), so a declared
   Content-Length can never desync from the written bytes. The payload digest
   (body-only) is passed in when already known. On success the record id is
   copied to out_id (may be NULL). */
static int warc_emit(warc_writer *w, const char *type, const char *content_type,
                     const char *target_uri, const char *ip,
                     const char *concurrent_to, const char *refers_uri,
                     const char *refers_date, const char *profile,
                     const char *payload_digest, int http_section,
                     const char *http_hdr, size_t http_hdr_len, int has_body,
                     const char *body, size_t body_len, const char *body_path,
                     char out_id[64]) {
  wbuf hdr;
  member m;
  char id[64], date[32];
  size_t sep = http_section ? 2 : 0;
  size_t payload = has_body ? body_len : 0;
  size_t block_len;
  int rc = -1;
#if HTS_USEOPENSSL
  digester d;
  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned int mdlen = 0;
  char block_b32[33];
  int have_block_digest = 0;
#endif

  /* F4: overflow-safe block length; http_hdr_len+sep is provably small. */
  if (payload > (size_t) -1 - http_hdr_len - sep)
    return -1;
  block_len = http_hdr_len + sep + payload;

  memset(&hdr, 0, sizeof(hdr));
  warc_make_id(w, id);
  warc_now_iso8601(date);

#if HTS_USEOPENSSL
  /* Block digest over the whole block, in one streaming pass. */
  d.block = EVP_MD_CTX_new();
  d.payload = NULL;
  if (d.block != NULL && EVP_DigestInit_ex(d.block, EVP_sha1(), NULL) == 1 &&
      stream_block(http_section, http_hdr, http_hdr_len, has_body, body,
                   body_len, body_path, digest_sink, &d) == 0 &&
      EVP_DigestFinal_ex(d.block, md, &mdlen) == 1 && mdlen == 20) {
    base32_20(md, block_b32);
    have_block_digest = 1;
  }
  if (d.block != NULL)
    EVP_MD_CTX_free(d.block);
#endif

  if (wbuf_puts(&hdr, "WARC/1.1\r\n") != 0 ||
      wbuf_printf(&hdr, "WARC-Type: %s\r\n", type) != 0 ||
      wbuf_printf(&hdr, "WARC-Record-ID: %s\r\n", id) != 0 ||
      wbuf_printf(&hdr, "WARC-Date: %s\r\n", date) != 0)
    goto done;
  if (content_type != NULL &&
      wbuf_printf(&hdr, "Content-Type: %s\r\n", content_type) != 0)
    goto done;
  if (wbuf_printf(&hdr, "Content-Length: %llu\r\n",
                  (unsigned long long) block_len) != 0)
    goto done;
  if (w->info_id[0] != '\0' && strcmp(type, "warcinfo") != 0 &&
      wbuf_printf(&hdr, "WARC-Warcinfo-ID: %s\r\n", w->info_id) != 0)
    goto done;
  if (target_uri != NULL && target_uri[0] != '\0' &&
      wbuf_printf(&hdr, "WARC-Target-URI: %s\r\n", target_uri) != 0)
    goto done;
  if (ip != NULL && ip[0] != '\0' &&
      wbuf_printf(&hdr, "WARC-IP-Address: %s\r\n", ip) != 0)
    goto done;
  if (concurrent_to != NULL && concurrent_to[0] != '\0' &&
      wbuf_printf(&hdr, "WARC-Concurrent-To: %s\r\n", concurrent_to) != 0)
    goto done;
  if (profile != NULL &&
      wbuf_printf(&hdr, "WARC-Profile: %s\r\n", profile) != 0)
    goto done;
  if (refers_uri != NULL && refers_uri[0] != '\0' &&
      wbuf_printf(&hdr, "WARC-Refers-To-Target-URI: %s\r\n", refers_uri) != 0)
    goto done;
  if (refers_date != NULL && refers_date[0] != '\0' &&
      wbuf_printf(&hdr, "WARC-Refers-To-Date: %s\r\n", refers_date) != 0)
    goto done;
#if HTS_USEOPENSSL
  if (have_block_digest &&
      wbuf_printf(&hdr, "WARC-Block-Digest: sha1:%s\r\n", block_b32) != 0)
    goto done;
#endif
  if (payload_digest != NULL && payload_digest[0] != '\0' &&
      wbuf_printf(&hdr, "WARC-Payload-Digest: sha1:%s\r\n", payload_digest) !=
          0)
    goto done;
  if (wbuf_puts(&hdr, "\r\n") != 0)
    goto done;

  if (member_begin(&m, w) != 0)
    goto done;
  if (member_write(&m, hdr.data, hdr.len) != 0 ||
      stream_block(http_section, http_hdr, http_hdr_len, has_body, body,
                   body_len, body_path, write_sink, &m) != 0 ||
      member_write(&m, "\r\n\r\n", 4) != 0) {
    member_end(&m);
    goto done;
  }
  if (member_end(&m) != 0)
    goto done;

  {
    long pos = ftell(w->f);
    if (pos >= 0)
      w->offset = (uint64_t) pos;
  }
  if (out_id != NULL)
    strlcpybuff(out_id, id, 64);
  rc = 0;
done:
  wbuf_free(&hdr);
  return rc;
}

/* ---- request stash (engine hooks) ---- */

void warc_stash_request(htsblk *r, const char *reqhdr) {
  if (r == NULL)
    return;
  freet(r->warc_reqhdr);
  if (reqhdr != NULL)
    r->warc_reqhdr = strdupt(reqhdr);
}

void warc_stash_response(htsblk *r, const char *resphdr) {
  if (r == NULL)
    return;
  freet(r->warc_resphdr);
  if (resphdr != NULL)
    r->warc_resphdr = strdupt(resphdr);
}

void warc_free_request(htsblk *r) {
  if (r != NULL) {
    freet(r->warc_reqhdr);
    freet(r->warc_resphdr);
  }
}

/* ---- open / close ---- */

warc_writer *warc_open(httrackp *opt, const char *path) {
  warc_writer *w;
  char namebuf[HTS_URLMAXSIZE * 2];
  char catbuff[CATBUFF_SIZE];
  wbuf info;
  const char *robots;
  size_t plen;

  if (path == NULL)
    return NULL;

  /* --warc with no name: <output>/httrack-<timestamp>.warc.gz */
  if (strcmp(path, WARC_AUTONAME) == 0) {
    char ts[32];
    time_t t = time(NULL);
    struct tm tmv;
#if defined(_WIN32)
    struct tm *g = gmtime(&t);
    if (g != NULL)
      tmv = *g;
    else
      memset(&tmv, 0, sizeof(tmv));
#else
    if (gmtime_r(&t, &tmv) == NULL)
      memset(&tmv, 0, sizeof(tmv));
#endif
    strftime(ts, sizeof(ts), "%Y%m%d%H%M%S", &tmv);
    snprintf(catbuff, sizeof(catbuff), "httrack-%s.warc.gz", ts);
    path =
        fconcat(namebuf, sizeof(namebuf), StringBuff(opt->path_html), catbuff);
  } else {
    /* --warc-file NAME: append .warc.gz unless already a .warc/.warc.gz name;
       place bare basenames under the output directory (like the auto name). */
    size_t l = strlen(path);
    int has_warc = (l >= 5 && strcasecmp(path + l - 5, ".warc") == 0);
    int has_gz = (l >= 3 && strcasecmp(path + l - 3, ".gz") == 0);
    char named[HTS_URLMAXSIZE];
    if (has_warc || has_gz)
      strlcpybuff(named, path, sizeof(named));
    else
      snprintf(named, sizeof(named), "%s.warc.gz", path);
    if (strchr(named, '/') == NULL && strchr(named, '\\') == NULL) {
      path =
          fconcat(namebuf, sizeof(namebuf), StringBuff(opt->path_html), named);
    } else {
      strlcpybuff(namebuf, named, sizeof(namebuf));
      path = namebuf;
    }
  }

  w = calloct(1, sizeof(*w));
  if (w == NULL)
    return NULL;
  plen = strlen(path);
  w->gz = (plen >= 3 && strcasecmp(path + plen - 3, ".gz") == 0);
  w->rng = (uint64_t) time(NULL) ^ ((uint64_t) (uintptr_t) w << 16) ^
           0x9e3779b97f4a7c15ULL;
  w->seen = coucal_new(0);
  if (w->seen != NULL)
    coucal_value_is_malloc(w->seen, 1);

  w->f = FOPEN(fconv(catbuff, sizeof(catbuff), path), "wb");
  if (w->f == NULL) {
    if (w->seen != NULL)
      coucal_delete(&w->seen);
    freet(w);
    return NULL;
  }

  robots = (opt->robots == HTS_ROBOTS_NEVER) ? "ignore" : "obey";
  memset(&info, 0, sizeof(info));
  if (wbuf_printf(&info,
                  "software: HTTrack/%s (+https://www.httrack.com/)\r\n"
                  "format: WARC file version 1.1\r\n"
                  "conformsTo: http://iipc.github.io/warc-specifications/"
                  "specifications/warc-format/warc-1.1/\r\n"
                  "robots: %s\r\n",
                  HTTRACK_VERSION, robots) != 0 ||
      (StringNotEmpty(opt->path_html) &&
       wbuf_printf(&info, "isPartOf: %s\r\n", StringBuff(opt->path_html)) !=
           0)) {
    wbuf_free(&info);
    warc_close(w);
    return NULL;
  }
  w->info_id[0] = '\0'; /* not yet known: warcinfo omits WARC-Warcinfo-ID */
  if (warc_emit(w, "warcinfo", "application/warc-fields", NULL, NULL, NULL,
                NULL, NULL, NULL, NULL, 0, NULL, 0, 1, info.data, info.len,
                NULL, w->info_id) != 0) {
    wbuf_free(&info);
    warc_close(w);
    return NULL;
  }
  wbuf_free(&info);
  return w;
}

void warc_close(warc_writer *w) {
  if (w == NULL)
    return;
  if (w->f != NULL)
    fclose(w->f);
  if (w->seen != NULL)
    coucal_delete(&w->seen);
  freet(w);
}

void warc_close_opt(httrackp *opt) {
  if (opt->state.warc != NULL && opt->state.warc != WARC_DISABLED) {
    warc_close((warc_writer *) opt->state.warc);
  }
  opt->state.warc = NULL;
}

/* ---- one transaction ---- */

int warc_write_transaction(warc_writer *w, const char *target_uri,
                           const char *ip, const char *req_hdr,
                           const char *resp_hdr, const char *body,
                           size_t body_len, const char *body_path,
                           int statuscode, int is_update_unchanged) {
  wbuf http;
  char resp_id[64];
  char pdig[33];
  int have_pdig;
  int is_revisit = 0;
  const char *profile = NULL;
  const char *refers_uri = NULL;
  const char *refers_date = NULL;
  char refers_buf[HTS_URLMAXSIZE * 2 + 64];
  int has_payload;
  int emit_body;
  int rc = -1;

  if (resp_hdr == NULL)
    return -1;

  /* A payload exists (for digesting) unless this is a bodyless 304. */
  has_payload = (body_len > 0 && (body != NULL || body_path != NULL) &&
                 !is_update_unchanged);

  /* Payload digest drives identical-payload-digest dedup (OpenSSL only). */
  have_pdig =
      has_payload ? payload_digest_b32(body, body_len, body_path, pdig) : 0;

  if (is_update_unchanged) {
    is_revisit = 1;
    profile = "http://netpreserve.org/warc/1.1/revisit/server-not-modified";
  } else if (have_pdig && w->seen != NULL) {
    void *prev = NULL;
    if (coucal_read_pvoid(w->seen, pdig, &prev) && prev != NULL) {
      char *slot = (char *) prev;
      char *sep = strchr(slot, '\001');
      is_revisit = 1;
      profile =
          "http://netpreserve.org/warc/1.1/revisit/identical-payload-digest";
      if (sep != NULL) {
        size_t n = (size_t) (sep - slot);
        if (n < sizeof(refers_buf)) {
          memcpy(refers_buf, slot, n);
          refers_buf[n] = '\0';
          refers_uri = refers_buf;
          refers_date = sep + 1;
        }
      }
    }
  }

  /* Both revisit kinds (server-304 and identical-payload-digest) are bodyless;
     only a full response carries the payload (F1). */
  emit_body = has_payload && !is_revisit;

  /* Normalize headers: full response rewrites Content-Length to the decoded
     body length and strips Content-Encoding; a revisit keeps them (no body). */
  memset(&http, 0, sizeof(http));
  if (normalize_http_headers(resp_hdr, emit_body ? (long long) body_len : -1,
                             &http) != 0) {
    wbuf_free(&http);
    return -1;
  }

  /* Response first: its id links the request via WARC-Concurrent-To. */
  resp_id[0] = '\0';
  if (warc_emit(w, is_revisit ? "revisit" : "response",
                "application/http;msgtype=response", target_uri, ip, NULL,
                refers_uri, refers_date, profile, have_pdig ? pdig : NULL, 1,
                http.data, http.len, emit_body, body, body_len, body_path,
                resp_id) != 0) {
    wbuf_free(&http);
    return -1;
  }
  wbuf_free(&http);

  if (req_hdr != NULL && req_hdr[0] != '\0') {
    size_t rlen = strlen(req_hdr);
    if (warc_emit(w, "request", "application/http;msgtype=request", target_uri,
                  NULL, resp_id, NULL, NULL, NULL, NULL, 0, NULL, 0, 1, req_hdr,
                  rlen, NULL, NULL) != 0)
      return -1;
  }

  /* Record this payload for later identical-payload-digest revisits. */
  if (!is_revisit && have_pdig && w->seen != NULL && target_uri != NULL) {
    char date[32];
    char *slot;
    size_t need;
    warc_now_iso8601(date);
    need = strlen(target_uri) + 1 + strlen(date) + 1;
    slot = malloct(need);
    if (slot != NULL) {
      snprintf(slot, need, "%s\001%s", target_uri, date);
      if (coucal_write_pvoid(w->seen, pdig, slot) == 0) {
        /* replaced an existing entry: coucal freed the old value */
      }
    }
  }

  (void) statuscode;
  rc = 0;
  return rc;
}

/* ---- engine emit hook ---- */

void warc_write_backtransaction(httrackp *opt, lien_back *back) {
  warc_writer *w;
  char uri[HTS_URLMAXSIZE * 4 + 16];
  char ip[128];
  const char *body;
  size_t body_len;
  const char *body_path;
  const char *resp_hdr;
  char synth[512];
  int is_unchanged;

  if (opt->state.warc == WARC_DISABLED)
    return;
  if (opt->state.warc == NULL) {
    w = warc_open(opt, StringBuff(opt->warc_file));
    if (w == NULL) {
      opt->state.warc = WARC_DISABLED;
      hts_log_print(opt, LOG_ERROR, "could not create WARC archive %s",
                    StringBuff(opt->warc_file));
      return;
    }
    opt->state.warc = w;
  }
  w = (warc_writer *) opt->state.warc;

  if (back->r.statuscode <= 0)
    return;

  snprintf(uri, sizeof(uri), "%s%s%s",
           link_has_authority(back->url_adr) ? "" : "http://", back->url_adr,
           back->url_fil);

  ip[0] = '\0';
  SOCaddr_inetntoa(ip, sizeof(ip), back->r.address);

  if (!back->r.is_write) {
    body = back->r.adr;
    body_len = (back->r.size > 0) ? (size_t) back->r.size : 0;
    body_path = NULL;
  } else {
    LLint fs;
    body = NULL;
    body_path = back->url_sav;
    fs = fsize_utf8(body_path);
    /* F4: an on-disk body past size_t (LLP32/ILP32, >4GB) would wrap the length
       used as Content-Length; drop the body rather than desync the record. */
    if (fs > 0 && (uint64_t) fs <= (uint64_t) (size_t) -1)
      body_len = (size_t) fs;
    else
      body_len = 0;
  }

  is_unchanged = (back->r.notmodified && opt->is_update) ? 1 : 0;

  /* Prefer the stashed raw headers; synthesize a minimal status line for the
     header-less (HTTP/0.9-style) responses that never carried a header block.
   */
  resp_hdr = back->r.warc_resphdr;
  if (resp_hdr == NULL) {
    snprintf(synth, sizeof(synth), "HTTP/1.1 %d %s\r\nContent-Type: %s\r\n\r\n",
             back->r.statuscode, back->r.msg[0] ? back->r.msg : "OK",
             back->r.contenttype[0] ? back->r.contenttype
                                    : "application/octet-stream");
    resp_hdr = synth;
  }

  warc_write_transaction(w, uri, ip, back->r.warc_reqhdr, resp_hdr, body,
                         body_len, body_path, back->r.statuscode, is_unchanged);
}
