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

#include "htswarc.h"

#include "htscore.h"
#include "htslib.h"
#include "htstools.h"
#include "htssafe.h"
#include "htszlib.h"
#include "coucal/coucal.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#if HTS_USEOPENSSL
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

/* opt->state.warc value meaning "open failed once, do not retry". */
#define WARC_DISABLED ((void *) ~(uintptr_t) 0)

struct warc_writer {
  FILE *f;
  httrackp *opt;   /* kept for close-time logging (warc_wacz_package) */
  int gz;          /* 1: one gzip member per record; 0: raw */
  uint64_t offset; /* running byte offset (member starts, for a future index) */
  uint64_t counter; /* monotonic record counter */
  uint64_t rng;     /* PRNG state for the UUID fallback */
  char info_id[64]; /* warcinfo WARC-Record-ID, referenced by every record */
  coucal seen;      /* base32 payload digest -> "uri\001date" (revisit dedup) */
  /* --warc-max-size rotation: NAME-00000.warc.gz, -00001, ... (wget-style). */
  uint64_t max_size;   /* rotate once a segment reaches this; 0: single file */
  char *seg_base;      /* segment path without the .warc[.gz] suffix, or NULL */
  const char *seg_ext; /* ".warc.gz" or ".warc" */
  unsigned seg;        /* current segment number */
  char *info_fields;   /* warcinfo body, re-emitted at each new segment */
  /* --warc-cdx: accumulate one CDXJ line per response/revisit/resource record,
     sorted (LC_ALL=C) and written to <base>.cdx at close. */
  int cdx_on;       /* --warc-cdx enabled */
  char *cdx_path;   /* <base>.cdx output path, or NULL */
  char *cur_seg;    /* basename of the current segment file (CDXJ filename) */
  char **cdx_lines; /* NUL-terminated CDXJ lines (no newline), owned */
  size_t cdx_count; /* lines in use */
  size_t cdx_cap;   /* lines allocated */
  /* --wacz: at crawl end, package the segment(s) + .cdx + a generated
     pages.jsonl into <base>.wacz (WACZ 1.1.1). SHA-256 needs OpenSSL. */
  int wacz_on;          /* --wacz enabled (and OpenSSL present) */
  char *base_path;      /* resolved archive path minus .warc[.gz] suffix */
  const char *base_ext; /* ".warc.gz" or ".warc" (static) */
  char *arc_path;    /* full single-file archive path (NULL under rotation) */
  char **page_lines; /* one JSON page line per 200 text/html response, owned */
  size_t page_count;
  size_t page_cap;
  char *main_url;  /* first captured page URL (datapackage mainPageUrl) */
  char *main_date; /* its WARC-Date (mainPageDate) */
};

const char *warc_truncated_reason(int code) {
  switch (code) {
  case WARC_TRUNC_LENGTH:
    return "length";
  case WARC_TRUNC_TIME:
    return "time";
  case WARC_TRUNC_DISCONNECT:
    return "disconnect";
  default:
    return NULL;
  }
}

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

static int wbuf_printf(wbuf *b, const char *fmt, ...) HTS_PRINTF_FUN(2, 3);

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

/* ---- SHA-256 hex (WACZ digests; OpenSSL-only) ---- */

#if HTS_USEOPENSSL
static void md32_to_hex(const unsigned char md[32], char out[65]) {
  static const char hx[] = "0123456789abcdef";
  int i;
  for (i = 0; i < 32; i++) {
    out[i * 2] = hx[md[i] >> 4];
    out[i * 2 + 1] = hx[md[i] & 0x0F];
  }
  out[64] = '\0';
}

/* Lowercase-hex SHA-256 of n bytes at p into out[65]. Returns 1 on success. */
static int sha256_hex_mem(const void *p, size_t n, char out[65]) {
  EVP_MD_CTX *c = EVP_MD_CTX_new();
  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned int mdlen = 0;
  int ok;
  if (c == NULL)
    return 0;
  ok = EVP_DigestInit_ex(c, EVP_sha256(), NULL) == 1 &&
       (n == 0 || EVP_DigestUpdate(c, p, n) == 1) &&
       EVP_DigestFinal_ex(c, md, &mdlen) == 1 && mdlen == 32;
  EVP_MD_CTX_free(c);
  if (ok)
    md32_to_hex(md, out);
  return ok;
}

/* Lowercase-hex SHA-256 of the on-disk file at path into out[65]. */
static int sha256_hex_file(const char *path, char out[65]) {
  EVP_MD_CTX *c;
  FILE *fp;
  char catbuff[CATBUFF_SIZE];
  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned int mdlen = 0;
  int ok;
  fp = FOPEN(fconv(catbuff, sizeof(catbuff), path), "rb");
  if (fp == NULL)
    return 0;
  c = EVP_MD_CTX_new();
  if (c == NULL) {
    fclose(fp);
    return 0;
  }
  ok = EVP_DigestInit_ex(c, EVP_sha256(), NULL) == 1;
  while (ok) {
    unsigned char b[32768];
    size_t nl = fread(b, 1, sizeof(b), fp);
    if (nl > 0 && EVP_DigestUpdate(c, b, nl) != 1)
      ok = 0;
    if (nl < sizeof(b))
      break;
  }
  ok = ok && !ferror(fp) && EVP_DigestFinal_ex(c, md, &mdlen) == 1 &&
       mdlen == 32;
  EVP_MD_CTX_free(c);
  fclose(fp);
  if (ok)
    md32_to_hex(md, out);
  return ok;
}
#endif

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
   When set_cl>=0, drops the original Content-Length and appends
   "Content-Length: <set_cl>". Content-Encoding is dropped too, unless keep_ce
   (strategy A: set_cl is then the compressed length). Returns 0. */
static int normalize_http_headers(const char *resp_hdr, long long set_cl,
                                  int keep_ce, wbuf *out) {
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
    } else if (set_cl >= 0 && header_is(p, len, "Content-Length")) {
      goto next;
    } else if (set_cl >= 0 && !keep_ce &&
               header_is(p, len, "Content-Encoding")) {
      /* keep_ce keeps every Content-Encoding line verbatim, duplicates and all:
         a faithful archive of what the server sent, as wget --warc. */
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

/* ---- CDXJ index (--warc-cdx) ---- */

/* Duplicate the last path component (basename), or NULL on OOM. */
static char *path_basename_dup(const char *path) {
  const char *b = path, *p;
  for (p = path; *p != '\0'; p++)
    if (*p == '/' || *p == '\\')
      b = p + 1;
  return strdupt(b);
}

/* A host made only of digits and dots is an IPv4 literal (never reversed). */
static int surt_host_is_ip(const char *h, size_t n) {
  size_t i;
  int dots = 0;
  for (i = 0; i < n; i++) {
    if (h[i] == '.')
      dots++;
    else if (h[i] < '0' || h[i] > '9')
      return 0;
  }
  return dots > 0;
}

/* SURT-canonicalize url into out (no newline): scheme and userinfo dropped,
   host lowercased with a leading www[digits] label stripped and the scheme
   default port removed, labels reversed and comma-joined then ')', path+query
   appended verbatim (case preserved, fragment dropped). IPv4 and [IPv6]
   literals keep their host form. A non-default port is kept as ":port" before
   the ')'. Returns 0 on success. */
static int surt_canon(const char *url, wbuf *out) {
  const char *p, *scheme_end, *authend, *host, *hostsep, *port = NULL;
  size_t portlen = 0, hlen, i;
  int def_port = -1;
  int is_ipv6 = 0, is_ip = 0;
  char hostbuf[1024];

  if (url == NULL)
    return -1;

  p = url;
  scheme_end = strstr(url, "://");
  if (scheme_end != NULL) {
    size_t sl = (size_t) (scheme_end - url);
    if (sl == 4 && strncasecmp(url, "http", 4) == 0)
      def_port = 80;
    else if (sl == 5 && strncasecmp(url, "https", 5) == 0)
      def_port = 443;
    p = scheme_end + 3;
  }

  authend = p;
  while (*authend != '\0' && *authend != '/' && *authend != '?' &&
         *authend != '#')
    authend++;

  { /* drop userinfo up to the last '@' inside the authority */
    const char *q, *at = NULL;
    for (q = p; q < authend; q++)
      if (*q == '@')
        at = q;
    if (at != NULL)
      p = at + 1;
  }
  host = p;

  if (host < authend && host[0] == '[') { /* [IPv6] literal */
    const char *rb = host;
    is_ipv6 = 1;
    while (rb < authend && *rb != ']')
      rb++;
    hostsep = (rb < authend) ? rb + 1 : authend;
  } else {
    const char *c = host;
    while (c < authend && *c != ':')
      c++;
    hostsep = c;
  }
  hlen = (size_t) (hostsep - host);
  if (hostsep < authend && *hostsep == ':') {
    port = hostsep + 1;
    portlen = (size_t) (authend - port);
  }

  if (hlen >= sizeof(hostbuf))
    return -1;
  for (i = 0; i < hlen; i++)
    hostbuf[i] = (char) tolower((unsigned char) host[i]);
  hostbuf[hlen] = '\0';

  if (!is_ipv6)
    is_ip = surt_host_is_ip(hostbuf, hlen);

  if (!is_ipv6 && !is_ip && hlen >= 4 && hostbuf[0] == 'w' &&
      hostbuf[1] == 'w' && hostbuf[2] == 'w') {
    size_t k = 3;
    while (k < hlen && hostbuf[k] >= '0' && hostbuf[k] <= '9')
      k++;
    if (k < hlen && hostbuf[k] == '.') {
      memmove(hostbuf, hostbuf + k + 1, hlen - (k + 1));
      hlen -= k + 1;
      hostbuf[hlen] = '\0';
    }
  }

  if (is_ipv6 || is_ip) {
    if (wbuf_add(out, hostbuf, hlen) != 0)
      return -1;
  } else { /* reverse the dot-separated labels, comma-joined */
    long idx;
    size_t seg_end = hlen;
    int first = 1;
    for (idx = (long) hlen; idx >= 0; idx--) {
      if (idx == 0 || hostbuf[idx - 1] == '.') {
        size_t lstart = (size_t) idx;
        size_t llen = seg_end - lstart;
        if (llen > 0) {
          if (!first && wbuf_add(out, ",", 1) != 0)
            return -1;
          if (wbuf_add(out, hostbuf + lstart, llen) != 0)
            return -1;
          first = 0;
        }
        seg_end = (idx > 0) ? (size_t) (idx - 1) : 0;
      }
    }
  }

  if (port != NULL && portlen > 0) { /* keep a non-default port */
    int pv = 0, ok = 1;
    size_t k;
    for (k = 0; k < portlen; k++) {
      if (port[k] < '0' || port[k] > '9') {
        ok = 0;
        break;
      }
      pv = pv * 10 + (port[k] - '0');
    }
    if (ok && pv != def_port &&
        (wbuf_add(out, ":", 1) != 0 || wbuf_add(out, port, portlen) != 0))
      return -1;
  }

  if (wbuf_add(out, ")", 1) != 0)
    return -1;

  { /* path + query, verbatim up to any fragment */
    const char *frag = authend;
    while (*frag != '\0' && *frag != '#')
      frag++;
    if (wbuf_add(out, authend, (size_t) (frag - authend)) != 0)
      return -1;
  }
  return 0;
}

/* 14-digit YYYYMMDDhhmmss from a WARC-Date "YYYY-MM-DDThh:mm:ssZ" (digits
 * only). */
static void iso8601_to_cdx14(const char *iso, char out[15]) {
  int o = 0;
  const char *p;
  for (p = iso; *p != '\0' && o < 14; p++)
    if (*p >= '0' && *p <= '9')
      out[o++] = *p;
  while (o < 14)
    out[o++] = '0';
  out[14] = '\0';
}

/* Append s as a JSON string (quoted, with " \ and control chars escaped). */
static int cdx_json_str(wbuf *b, const char *s) {
  if (wbuf_add(b, "\"", 1) != 0)
    return -1;
  for (; *s != '\0'; s++) {
    unsigned char c = (unsigned char) *s;
    if (c == '"' || c == '\\') {
      char e[2] = {'\\', (char) c};
      if (wbuf_add(b, e, 2) != 0)
        return -1;
    } else if (c < 0x20) {
      if (wbuf_printf(b, "\\u%04x", (unsigned) c) != 0)
        return -1;
    } else if (wbuf_add(b, s, 1) != 0) {
      return -1;
    }
  }
  return wbuf_add(b, "\"", 1);
}

/* Copy the media type of header "name" (up to ';'/space) from a raw HTTP header
   block into out; out is "" if absent. */
static void http_header_value(const char *hdr, const char *name, char *out,
                              size_t outsz) {
  size_t nl = strlen(name);
  const char *p = hdr;
  out[0] = '\0';
  if (hdr == NULL)
    return;
  while (*p != '\0') {
    const char *eol = strchr(p, '\n');
    size_t len = (eol != NULL) ? (size_t) (eol - p) : strlen(p);
    if (len > 0 && p[len - 1] == '\r')
      len--;
    if (len == 0)
      break; /* end of headers */
    if (len > nl && strncasecmp(p, name, nl) == 0 && p[nl] == ':') {
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
      if (vlen >= outsz)
        vlen = outsz - 1;
      memcpy(out, v, vlen);
      out[vlen] = '\0';
      return;
    }
    if (eol == NULL)
      break;
    p = eol + 1;
  }
}

/* Take ownership of a CDXJ line; frees it and returns -1 on OOM. */
static int cdx_lines_add(warc_writer *w, char *line) {
  if (w->cdx_count == w->cdx_cap) {
    size_t ncap = w->cdx_cap ? w->cdx_cap * 2 : 64;
    char **n;
    if (ncap > (size_t) -1 / sizeof(char *)) {
      freet(line);
      return -1;
    }
    n = realloct(w->cdx_lines, ncap * sizeof(char *));
    if (n == NULL) {
      freet(line);
      return -1;
    }
    w->cdx_lines = n;
    w->cdx_cap = ncap;
  }
  w->cdx_lines[w->cdx_count++] = line;
  return 0;
}

/* Build and stash one CDXJ line for a record. Best-effort: an OOM drops the
   line rather than failing the (already-written) record. */
static void warc_cdx_add(warc_writer *w, const char *target_uri,
                         const char *date_iso, const char *status,
                         const char *mime, const char *payload_digest,
                         uint64_t offset, uint64_t length) {
  wbuf line;
  char ts[15];
  memset(&line, 0, sizeof(line));
  iso8601_to_cdx14(date_iso, ts);
  if (surt_canon(target_uri, &line) != 0 ||
      wbuf_printf(&line, " %s {\"url\": ", ts) != 0 ||
      cdx_json_str(&line, target_uri) != 0)
    goto fail;
  if (mime != NULL && mime[0] != '\0' &&
      (wbuf_puts(&line, ", \"mime\": ") != 0 || cdx_json_str(&line, mime) != 0))
    goto fail;
  if (status != NULL && status[0] != '\0' &&
      (wbuf_puts(&line, ", \"status\": ") != 0 ||
       cdx_json_str(&line, status) != 0))
    goto fail;
  if (payload_digest != NULL && payload_digest[0] != '\0' &&
      wbuf_printf(&line, ", \"digest\": \"sha1:%s\"", payload_digest) != 0)
    goto fail;
  if (wbuf_printf(&line, ", \"length\": \"%llu\", \"offset\": \"%llu\"",
                  (unsigned long long) length,
                  (unsigned long long) offset) != 0)
    goto fail;
  if (w->cur_seg != NULL && (wbuf_puts(&line, ", \"filename\": ") != 0 ||
                             cdx_json_str(&line, w->cur_seg) != 0))
    goto fail;
  if (wbuf_puts(&line, "}") != 0 || wbuf_add(&line, "", 1) != 0) /* NUL */
    goto fail;
  if (cdx_lines_add(w, line.data) == 0)
    return; /* ownership transferred */
  return;   /* cdx_lines_add already freed on failure */
fail:
  wbuf_free(&line);
}

/* LC_ALL=C (unsigned byte) order over whole lines; the searchable key
   "<surt> <ts>" is the line prefix, so this yields sorted CDXJ. */
static int cdx_cmp(const void *a, const void *b) {
  const unsigned char *x = *(const unsigned char *const *) a;
  const unsigned char *y = *(const unsigned char *const *) b;
  while (*x != '\0' && *x == *y) {
    x++;
    y++;
  }
  return (int) *x - (int) *y;
}

/* Sort and write the accumulated CDXJ lines to <base>.cdx. */
static void warc_cdx_flush(warc_writer *w) {
  FILE *f;
  char catbuff[CATBUFF_SIZE];
  size_t i;
  if (!w->cdx_on || w->cdx_path == NULL || w->cdx_count == 0)
    return;
  qsort(w->cdx_lines, w->cdx_count, sizeof(char *), cdx_cmp);
  f = FOPEN(fconv(catbuff, sizeof(catbuff), w->cdx_path), "wb");
  if (f == NULL)
    return;
  for (i = 0; i < w->cdx_count; i++) {
    fputs(w->cdx_lines[i], f);
    fputc('\n', f);
  }
  fclose(f);
}

/* ---- WACZ pages + packaging (--wacz) ---- */

/* Record one pages.jsonl line for a top-level 200 text/html capture. First
   page also seeds datapackage mainPageUrl/mainPageDate. Best-effort. */
static void warc_page_add(warc_writer *w, const char *url, const char *date) {
  wbuf line;
  memset(&line, 0, sizeof(line));
  if (wbuf_printf(&line, "{\"id\": \"p%llu\", \"url\": ",
                  (unsigned long long) w->page_count) != 0 ||
      cdx_json_str(&line, url) != 0 || wbuf_puts(&line, ", \"ts\": ") != 0 ||
      cdx_json_str(&line, date) != 0 || wbuf_puts(&line, "}") != 0 ||
      wbuf_add(&line, "", 1) != 0) {
    wbuf_free(&line);
    return;
  }
  if (w->page_count == w->page_cap) {
    size_t ncap = w->page_cap ? w->page_cap * 2 : 32;
    char **n;
    if (ncap > (size_t) -1 / sizeof(char *)) {
      wbuf_free(&line);
      return;
    }
    n = realloct(w->page_lines, ncap * sizeof(char *));
    if (n == NULL) {
      wbuf_free(&line);
      return;
    }
    w->page_lines = n;
    w->page_cap = ncap;
  }
  w->page_lines[w->page_count++] = line.data;
  if (w->main_url == NULL) {
    w->main_url = strdupt(url);
    w->main_date = strdupt(date);
  }
}

#if HTS_USEOPENSSL
/* WACZ requires every ZIP entry stored, not deflated (spec 1.1.1). */
static int wacz_open_store(zipFile zf, const char *name) {
  zip_fileinfo zi;
  memset(&zi, 0, sizeof(zi));
  return zipOpenNewFileInZip(zf, name, &zi, NULL, 0, NULL, 0, NULL, 0 /*store*/,
                             0 /*level*/);
}

static int wacz_write_bytes(zipFile zf, const void *p, size_t n) {
  while (n > 0) {
    unsigned chunk = (n > 0x40000000u) ? 0x40000000u : (unsigned) n;
    if (zipWriteInFileInZip(zf, p, chunk) != ZIP_OK)
      return -1;
    p = (const char *) p + chunk;
    n -= chunk;
  }
  return 0;
}

/* Append one datapackage resource object; comma-prefixed unless first. */
static int wacz_resource_add(wbuf *res, int first, const char *name,
                             const char *path, const char *hex,
                             uint64_t bytes) {
  if (!first && wbuf_puts(res, ", ") != 0)
    return -1;
  if (wbuf_puts(res, "{\"name\": ") != 0 || cdx_json_str(res, name) != 0 ||
      wbuf_puts(res, ", \"path\": ") != 0 || cdx_json_str(res, path) != 0 ||
      wbuf_printf(res, ", \"hash\": \"sha256:%s\", \"bytes\": %llu}", hex,
                  (unsigned long long) bytes) != 0)
    return -1;
  return 0;
}

/* Store an on-disk file as zipname, hash it, and list it in res. */
static int wacz_add_disk(zipFile zf, const char *zipname, const char *diskpath,
                         const char *resname, wbuf *res, int first) {
  char hex[65];
  char catbuff[CATBUFF_SIZE];
  FILE *fp;
  uint64_t bytes = 0;
  int rc = -1;
  if (!sha256_hex_file(diskpath, hex))
    return -1;
  if (wacz_open_store(zf, zipname) != ZIP_OK)
    return -1;
  fp = FOPEN(fconv(catbuff, sizeof(catbuff), diskpath), "rb");
  if (fp != NULL) {
    rc = 0;
    for (;;) {
      char b[32768];
      size_t nl = fread(b, 1, sizeof(b), fp);
      if (nl > 0 && wacz_write_bytes(zf, b, nl) != 0) {
        rc = -1;
        break;
      }
      bytes += nl;
      if (nl < sizeof(b)) {
        if (ferror(fp))
          rc = -1;
        break;
      }
    }
    fclose(fp);
  }
  if (zipCloseFileInZip(zf) != ZIP_OK)
    rc = -1;
  if (rc == 0)
    rc = wacz_resource_add(res, first, resname, zipname, hex, bytes);
  return rc;
}

/* Store in-memory bytes as zipname; when res != NULL, hash and list them. */
static int wacz_add_mem(zipFile zf, const char *zipname, const void *data,
                        size_t n, const char *resname, wbuf *res, int first) {
  char hex[65];
  if (res != NULL && !sha256_hex_mem(data, n, hex))
    return -1;
  if (wacz_open_store(zf, zipname) != ZIP_OK)
    return -1;
  if (wacz_write_bytes(zf, data, n) != 0) {
    zipCloseFileInZip(zf);
    return -1;
  }
  if (zipCloseFileInZip(zf) != ZIP_OK)
    return -1;
  if (res != NULL)
    return wacz_resource_add(res, first, resname, zipname, hex, n);
  return 0;
}

static zipFile wacz_zip_open(const char *path) {
  zlib_filefunc64_def ff;
  fill_fopen64_filefunc(&ff);
  return zipOpen2_64(path, 0 /*create*/, NULL, &ff);
}

/* Move src onto dst (UTF-8/Windows-safe); RENAME won't clobber on Windows, so
   fall back to unlink+rename. Returns 0 on success. */
static int wacz_rename_over(const char *src, const char *dst) {
  char cs[CATBUFF_SIZE], cd[CATBUFF_SIZE];
  if (RENAME(fconv(cs, sizeof(cs), src), fconv(cd, sizeof(cd), dst)) == 0)
    return 0;
  (void) UNLINK(fconv(cd, sizeof(cd), dst));
  return RENAME(fconv(cs, sizeof(cs), src), fconv(cd, sizeof(cd), dst));
}

/* Package the segment(s) + .cdx + a generated pages.jsonl into <base>.wacz at
   crawl end (the archive file(s) and .cdx are already closed on disk). */
static void warc_wacz_package(warc_writer *w) {
  char waczpath[HTS_URLMAXSIZE * 2];
  char tmppath[HTS_URLMAXSIZE * 2];
  char segpath[HTS_URLMAXSIZE * 2];
  char catbuff[CATBUFF_SIZE];
  zipFile zf;
  wbuf pages, resources, dp, digest;
  char *seg_name;
  char dp_hex[65];
  char created[32];
  unsigned s, nseg;
  int err = 0, first = 1;
  size_t i;

  if (w->base_path == NULL || w->cdx_path == NULL)
    return;
  snprintf(waczpath, sizeof(waczpath), "%s.wacz", w->base_path);
  snprintf(tmppath, sizeof(tmppath), "%s.wacz.tmp", w->base_path);
  /* Build into a temp; only full success replaces <base>.wacz, so a zero-record
     re-run can't destroy a good archive (#522). */
  zf = wacz_zip_open(fconv(catbuff, sizeof(catbuff), tmppath));
  if (zf == NULL) {
    hts_log_print(w->opt, LOG_WARNING, "WACZ: could not create %s", tmppath);
    return;
  }
  memset(&resources, 0, sizeof(resources));

  /* archive/<name>.warc.gz for every segment (single file, or 0..seg). */
  nseg = (w->max_size > 0) ? w->seg + 1 : 1;
  for (s = 0; s < nseg && !err; s++) {
    char zipname[HTS_URLMAXSIZE];
    if (w->max_size > 0)
      snprintf(segpath, sizeof(segpath), "%s-%05u%s", w->base_path, s,
               w->base_ext);
    else
      strlcpybuff(segpath, w->arc_path != NULL ? w->arc_path : w->base_path,
                  sizeof(segpath));
    seg_name = path_basename_dup(segpath);
    if (seg_name == NULL) {
      err = 1;
      break;
    }
    snprintf(zipname, sizeof(zipname), "archive/%s", seg_name);
    if (wacz_add_disk(zf, zipname, segpath, seg_name, &resources, first) != 0)
      err = 1;
    freet(seg_name);
    first = 0;
  }

  /* indexes/index.cdx */
  if (!err && wacz_add_disk(zf, "indexes/index.cdx", w->cdx_path, "index.cdx",
                            &resources, first) != 0)
    err = 1;
  first = 0;

  /* pages/pages.jsonl: header line + one line per captured page. */
  memset(&pages, 0, sizeof(pages));
  if (!err &&
      wbuf_puts(&pages, "{\"format\": \"json-pages-1.0\", \"id\": \"pages\", "
                        "\"title\": \"All Pages\"}\n") != 0)
    err = 1;
  for (i = 0; i < w->page_count && !err; i++)
    if (wbuf_puts(&pages, w->page_lines[i]) != 0 ||
        wbuf_add(&pages, "\n", 1) != 0)
      err = 1;
  if (!err && wacz_add_mem(zf, "pages/pages.jsonl", pages.data, pages.len,
                           "pages.jsonl", &resources, first) != 0)
    err = 1;
  wbuf_free(&pages);

  /* datapackage.json listing every stored file with its sha256 + size. */
  warc_now_iso8601(created);
  memset(&dp, 0, sizeof(dp));
  if (!err &&
      (wbuf_printf(&dp,
                   "{\"profile\": \"data-package\", \"wacz_version\": "
                   "\"1.1.1\", \"software\": \"HTTrack/%s\", \"created\": ",
                   HTTRACK_VERSION) != 0 ||
       cdx_json_str(&dp, created) != 0))
    err = 1;
  if (!err && w->main_url != NULL &&
      (wbuf_puts(&dp, ", \"mainPageUrl\": ") != 0 ||
       cdx_json_str(&dp, w->main_url) != 0 ||
       wbuf_puts(&dp, ", \"mainPageDate\": ") != 0 ||
       cdx_json_str(&dp, w->main_date != NULL ? w->main_date : created) != 0))
    err = 1;
  if (!err && (wbuf_puts(&dp, ", \"resources\": [") != 0 ||
               wbuf_add(&dp, resources.data, resources.len) != 0 ||
               wbuf_puts(&dp, "]}") != 0))
    err = 1;
  if (!err &&
      wacz_add_mem(zf, "datapackage.json", dp.data, dp.len, NULL, NULL, 0) != 0)
    err = 1;

  /* datapackage-digest.json chains the integrity of datapackage.json. */
  memset(&digest, 0, sizeof(digest));
  if (!err && sha256_hex_mem(dp.data, dp.len, dp_hex)) {
    if (wbuf_printf(&digest,
                    "{\"path\": \"datapackage.json\", \"hash\": \"sha256:%s\"}",
                    dp_hex) != 0 ||
        wacz_add_mem(zf, "datapackage-digest.json", digest.data, digest.len,
                     NULL, NULL, 0) != 0)
      err = 1;
  } else {
    err = 1;
  }
  wbuf_free(&digest);
  wbuf_free(&dp);
  wbuf_free(&resources);

  zipClose(zf, NULL);
  if (err) {
    (void) UNLINK(fconv(catbuff, sizeof(catbuff), tmppath));
    hts_log_print(w->opt, LOG_WARNING,
                  "WACZ: packaging failed, kept existing %s untouched",
                  waczpath);
  } else if (wacz_rename_over(tmppath, waczpath) != 0) {
    (void) UNLINK(fconv(catbuff, sizeof(catbuff), tmppath));
    hts_log_print(w->opt, LOG_WARNING | LOG_ERRNO,
                  "WACZ: could not finalize %s", waczpath);
  }
}
#endif

/* Close the current segment and open the next; writes its warcinfo. */
static int warc_rotate(warc_writer *w);

/* Emit one full WARC record. When http_section, the block carries an HTTP
   header block (http_hdr, possibly empty) + a CRLF separator; body follows when
   has_body. block_len is derived here (single source of truth: separator and
   payload are counted exactly as stream_block emits them), so a declared
   Content-Length can never desync from the written bytes. The payload digest
   (body-only) is passed in when already known. truncated is a WARC-Truncated
   reason token or NULL. On success the record id is copied to out_id (may be
   NULL). */
static int warc_emit(warc_writer *w, const char *type, const char *content_type,
                     const char *target_uri, const char *ip,
                     const char *concurrent_to, const char *refers_uri,
                     const char *refers_date, const char *profile,
                     const char *payload_digest, const char *truncated,
                     const char *cdx_status, const char *cdx_mime,
                     int http_section, const char *http_hdr,
                     size_t http_hdr_len, int has_body, const char *body,
                     size_t body_len, const char *body_path, char out_id[64]) {
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

  /* Rotate to the next segment before this record when the current one is full;
     never split a record, and never rotate a warcinfo (it opens a segment). */
  if (w->max_size > 0 && w->seg_base != NULL && w->offset >= w->max_size &&
      strcmp(type, "warcinfo") != 0) {
    if (warc_rotate(w) != 0)
      return -1;
  }

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
  if (truncated != NULL &&
      wbuf_printf(&hdr, "WARC-Truncated: %s\r\n", truncated) != 0)
    goto done;
  if (wbuf_puts(&hdr, "\r\n") != 0)
    goto done;

  if (member_begin(&m, w) != 0)
    goto done;
  { /* member start (before writing): CDXJ offset for this record */
    uint64_t rec_offset = w->offset;
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
    /* Index response/revisit/resource records only (not warcinfo/request). */
    if (w->cdx_on && target_uri != NULL && target_uri[0] != '\0' &&
        (strcmp(type, "response") == 0 || strcmp(type, "revisit") == 0 ||
         strcmp(type, "resource") == 0))
      warc_cdx_add(w, target_uri, date, cdx_status, cdx_mime, payload_digest,
                   rec_offset, w->offset - rec_offset);
    /* WACZ pages: top-level 200 text/html captures. */
    if (w->wacz_on && target_uri != NULL && target_uri[0] != '\0' &&
        strcmp(type, "response") == 0 && cdx_status != NULL &&
        strcmp(cdx_status, "200") == 0 && cdx_mime != NULL &&
        strncasecmp(cdx_mime, "text/html", 9) == 0)
      warc_page_add(w, target_uri, date);
  }
  if (out_id != NULL)
    strlcpybuff(out_id, id, 64);
  rc = 0;
done:
  wbuf_free(&hdr);
  return rc;
}

/* ---- segment rotation (--warc-max-size) ---- */

/* Emit the warcinfo that heads a segment; sets w->info_id for its records. */
static int warc_write_warcinfo_record(warc_writer *w) {
  w->info_id[0] = '\0'; /* warcinfo itself carries no WARC-Warcinfo-ID */
  return warc_emit(
      w, "warcinfo", "application/warc-fields", NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL, 0, NULL, 0, 1, w->info_fields,
      w->info_fields != NULL ? strlen(w->info_fields) : 0, NULL, w->info_id);
}

static int warc_rotate(warc_writer *w) {
  char namebuf[HTS_URLMAXSIZE * 2];
  char catbuff[CATBUFF_SIZE];
  if (w->f != NULL) {
    fclose(w->f);
    w->f = NULL;
  }
  w->seg++;
  snprintf(namebuf, sizeof(namebuf), "%s-%05u%s", w->seg_base, w->seg,
           w->seg_ext);
  w->f = FOPEN(fconv(catbuff, sizeof(catbuff), namebuf), "wb");
  if (w->f == NULL)
    return -1;
  w->offset = 0;
  if (w->cdx_on) {
    freet(w->cur_seg);
    w->cur_seg = path_basename_dup(namebuf);
  }
  return warc_write_warcinfo_record(w);
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
    if (r->warc_rawpath != NULL) {
      (void) UNLINK(r->warc_rawpath); /* owns the strategy-A spool file */
      freet(r->warc_rawpath);
      r->warc_rawpath = NULL;
    }
  }
}

void warc_adopt_rawspool(htsblk *r, const char *tmpfile_path) {
  if (r != NULL) {
    LLint rawsize;
    freet(r->warc_rawpath);
    r->warc_rawpath = NULL;
    r->warc_rawsize = 0;
    if (strnotempty(tmpfile_path) && (rawsize = fsize_utf8(tmpfile_path)) > 0 &&
        (r->warc_rawpath = strdupt(tmpfile_path)) != NULL) {
      r->warc_rawsize = rawsize;
    }
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
  w->opt = opt;
  plen = strlen(path);
  w->gz = (plen >= 3 && strcasecmp(path + plen - 3, ".gz") == 0);
  w->rng = (uint64_t) time(NULL) ^ ((uint64_t) (uintptr_t) w << 16) ^
           0x9e3779b97f4a7c15ULL;
  w->seen = coucal_new(0);
  if (w->seen != NULL)
    coucal_value_is_malloc(w->seen, 1);
  w->max_size = (opt->warc_max_size > 0) ? (uint64_t) opt->warc_max_size : 0;

  /* Build the warcinfo body once; each segment re-emits it. */
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
           0) ||
      wbuf_add(&info, "", 1) != 0) { /* NUL-terminate for info_fields */
    wbuf_free(&info);
    warc_close(w);
    return NULL;
  }
  w->info_fields = strdupt(info.data);
  wbuf_free(&info);
  if (w->info_fields == NULL) {
    warc_close(w);
    return NULL;
  }

  /* --warc-cdx: <base>.cdx next to the resolved archive path (pre-rotation). */
  if (opt->warc_cdx) {
    size_t l = strlen(path);
    size_t baselen = l;
    if (l >= 8 && strcasecmp(path + l - 8, ".warc.gz") == 0) {
      baselen = l - 8;
      w->base_ext = ".warc.gz";
    } else if (l >= 5 && strcasecmp(path + l - 5, ".warc") == 0) {
      baselen = l - 5;
      w->base_ext = ".warc";
    } else {
      w->base_ext = w->gz ? ".warc.gz" : ".warc";
    }
    w->cdx_on = 1;
    w->cdx_path = malloct(baselen + 5); /* ".cdx" + NUL */
    w->base_path = malloct(baselen + 1);
    if (w->cdx_path == NULL || w->base_path == NULL) {
      warc_close(w);
      return NULL;
    }
    memcpy(w->cdx_path, path, baselen);
    memcpy(w->cdx_path + baselen, ".cdx", 5);
    memcpy(w->base_path, path, baselen);
    w->base_path[baselen] = '\0';
  }

  /* --wacz packages archive+cdx+pages at close; SHA-256 needs OpenSSL. */
  if (opt->warc_wacz) {
#if HTS_USEOPENSSL
    w->wacz_on = 1;
#else
    hts_log_print(opt, LOG_WARNING,
                  "WACZ requires an OpenSSL-enabled build for SHA-256 digests; "
                  "--wacz disabled (WARC and CDXJ still written)");
#endif
  }

  /* Rotation on: the first segment is <base>-00000<ext> (wget-style); split the
     resolved path into base + suffix so later segments reuse the base. */
  if (w->max_size > 0) {
    size_t l = strlen(path);
    size_t baselen;
    if (l >= 8 && strcasecmp(path + l - 8, ".warc.gz") == 0) {
      baselen = l - 8;
      w->seg_ext = ".warc.gz";
    } else if (l >= 5 && strcasecmp(path + l - 5, ".warc") == 0) {
      baselen = l - 5;
      w->seg_ext = ".warc";
    } else {
      baselen = l;
      w->seg_ext = w->gz ? ".warc.gz" : ".warc";
    }
    w->seg_base = malloct(baselen + 1);
    if (w->seg_base == NULL) {
      warc_close(w);
      return NULL;
    }
    memcpy(w->seg_base, path, baselen);
    w->seg_base[baselen] = '\0';
    w->seg = 0;
    snprintf(namebuf, sizeof(namebuf), "%s-%05u%s", w->seg_base, w->seg,
             w->seg_ext);
    path = namebuf;
  }

  w->f = FOPEN(fconv(catbuff, sizeof(catbuff), path), "wb");
  if (w->f == NULL) {
    warc_close(w);
    return NULL;
  }
  if (w->cdx_on)
    w->cur_seg = path_basename_dup(path);
  if (w->wacz_on && w->max_size == 0)
    w->arc_path = strdupt(path); /* single-file: package this exact path */

  if (warc_write_warcinfo_record(w) != 0) {
    warc_close(w);
    return NULL;
  }
  return w;
}

void warc_close(warc_writer *w) {
  size_t i;
  if (w == NULL)
    return;
  warc_cdx_flush(w); /* sort + write <base>.cdx before tearing down */
  if (w->f != NULL)
    fclose(w->f);
  w->f = NULL;
#if HTS_USEOPENSSL
  if (w->wacz_on) /* package once the segment(s) + .cdx are closed on disk */
    warc_wacz_package(w);
#endif
  if (w->seen != NULL)
    coucal_delete(&w->seen);
  for (i = 0; i < w->cdx_count; i++)
    freet(w->cdx_lines[i]);
  freet(w->cdx_lines);
  freet(w->cdx_path);
  for (i = 0; i < w->page_count; i++)
    freet(w->page_lines[i]);
  freet(w->page_lines);
  freet(w->base_path);
  freet(w->arc_path);
  freet(w->main_url);
  freet(w->main_date);
  freet(w->cur_seg);
  freet(w->seg_base);
  freet(w->info_fields);
  freet(w);
}

int warc_surt(const char *url, char *out, size_t outsz) {
  wbuf b;
  int rc = -1;
  memset(&b, 0, sizeof(b));
  if (surt_canon(url, &b) == 0 && wbuf_add(&b, "", 1) == 0 && b.len <= outsz) {
    strlcpybuff(out, b.data, outsz);
    rc = 0;
  }
  wbuf_free(&b);
  return rc;
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
                           int statuscode, int is_update_unchanged,
                           int truncated, int keep_content_encoding) {
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
  char statusbuf[16];
  char mimebuf[256];

  if (resp_hdr == NULL)
    return -1;

  /* CDXJ status/mime (from the caller's status and the response Content-Type).
   */
  snprintf(statusbuf, sizeof(statusbuf), "%d", statuscode);
  http_header_value(resp_hdr, "Content-Type", mimebuf, sizeof(mimebuf));

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

  /* Full response rewrites Content-Length; strategy A/B decide Content-Encoding
     (see normalize_http_headers). Revisit keeps both. */
  memset(&http, 0, sizeof(http));
  if (normalize_http_headers(resp_hdr, emit_body ? (long long) body_len : -1,
                             keep_content_encoding, &http) != 0) {
    wbuf_free(&http);
    return -1;
  }

  /* Response first: its id links the request via WARC-Concurrent-To. A revisit
     is a deliberate dedup, not a truncation, so tag WARC-Truncated only on a
     full (body-carrying) response. */
  resp_id[0] = '\0';
  if (warc_emit(w, is_revisit ? "revisit" : "response",
                "application/http;msgtype=response", target_uri, ip, NULL,
                refers_uri, refers_date, profile, have_pdig ? pdig : NULL,
                emit_body ? warc_truncated_reason(truncated) : NULL, statusbuf,
                mimebuf, 1, http.data, http.len, emit_body, body, body_len,
                body_path, resp_id) != 0) {
    wbuf_free(&http);
    return -1;
  }
  wbuf_free(&http);

  if (req_hdr != NULL && req_hdr[0] != '\0') {
    size_t rlen = strlen(req_hdr);
    if (warc_emit(w, "request", "application/http;msgtype=request", target_uri,
                  NULL, resp_id, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0,
                  NULL, 0, 1, req_hdr, rlen, NULL, NULL) != 0)
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

/* ---- one non-HTTP capture ---- */

int warc_write_resource(warc_writer *w, const char *target_uri, const char *ip,
                        const char *content_type, const char *body,
                        size_t body_len, const char *body_path, int truncated) {
  char pdig[33];
  int has_body = (body_len > 0 && (body != NULL || body_path != NULL));
  int have_pdig =
      has_body ? payload_digest_b32(body, body_len, body_path, pdig) : 0;
  /* resource: the block is the raw payload, its own MIME is the record's
     Content-Type, and there is no HTTP request/response envelope. */
  return warc_emit(w, "resource",
                   (content_type != NULL && content_type[0] != '\0')
                       ? content_type
                       : "application/octet-stream",
                   target_uri, ip, NULL, NULL, NULL, NULL,
                   have_pdig ? pdig : NULL, warc_truncated_reason(truncated),
                   NULL, content_type, 0, NULL, 0, has_body, body, body_len,
                   body_path, NULL);
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
  int is_ftp;
  int keep_ce = 0;

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

  is_ftp = strfield(back->url_adr, "ftp://") != 0;
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

  /* FTP has no HTTP envelope: one resource record carrying the payload. */
  if (is_ftp) {
    warc_write_resource(w, uri, ip, back->r.contenttype, body, body_len,
                        body_path, back->r.warc_truncated);
    return;
  }

  /* Strategy A: use the spooled coded body; the digest below is then over the
     coded payload. */
  if (opt->warc_verbatim && back->r.warc_rawpath != NULL &&
      back->r.warc_rawsize > 0 &&
      (uint64_t) back->r.warc_rawsize <= (uint64_t) (size_t) -1) {
    body = NULL;
    body_path = back->r.warc_rawpath;
    body_len = (size_t) back->r.warc_rawsize;
    keep_ce = 1;
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
                         body_len, body_path, back->r.statuscode, is_unchanged,
                         back->r.warc_truncated, keep_ce);
}
