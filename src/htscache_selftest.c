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
/* File: htscache_selftest.c subroutines:                       */
/*       in-process self-test for the (ZIP) cache subsystem      */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Drives the public cache API (cache_init / cache_add / cache_readex)
   through a create -> read -> update cycle on a real on-disk ZIP cache,
   asserting every header field and the (binary-safe) body round-trips.
   Besides a few hand-crafted edge cases it stores a few thousand entries
   (index/lookup scale) and a handful of large compressible/incompressible
   bodies (zlib deflate/inflate). Reached via `httrack -#A <dir>`. */

#define HTS_INTERNAL_BYTECODE

#include "htscache_selftest.h"

#include "htscache.h"
#include "htscore.h"
#include "htslib.h"
#include "htszlib.h"

#include <stdio.h>
#include <string.h>

#define SELFTEST_VOLUME 3000 /* number of small entries in the scale pass */

/* Open a cache session. A write session (ro=0) rotates new.zip -> old.zip and
   opens a fresh new.zip; a read session (ro=1) opens new.zip in place. */
static void selftest_open(cache_back *cache, httrackp *opt, int ro) {
  memset(cache, 0, sizeof(*cache));
  cache->type = 1;
  cache->log = stderr;
  cache->errlog = stderr;
  cache->hashtable = coucal_new(0);
  cache->ro = ro;
  cache_init(cache, opt);
}

static void selftest_open_for_write(cache_back *cache, httrackp *opt) {
  selftest_open(cache, opt, 0);
}

static void selftest_open_for_read(cache_back *cache, httrackp *opt) {
  selftest_open(cache, opt, 1);
}

static void selftest_close(cache_back *cache) {
  if (cache->dat != NULL) {
    fclose(cache->dat);
    cache->dat = NULL;
  }
  if (cache->ndx != NULL) {
    fclose(cache->ndx);
    cache->ndx = NULL;
  }
  if (cache->zipOutput != NULL) {
    zipClose(cache->zipOutput,
             "Created by HTTrack Website Copier (cache self-test)");
    cache->zipOutput = NULL;
  }
  if (cache->zipInput != NULL) {
    unzClose(cache->zipInput);
    cache->zipInput = NULL;
  }
  /* hashtable is intentionally not coucal_delete()d: it would dump a stats
     summary to stderr on every call, and this is a one-shot CLI subcommand
     that exits right after (same choice as the other -# cache subcommands) */
}

/* Store one entry. The body is copied into a private buffer (any size), so
   callers may pass const data and cache_add never sees a cast-away qualifier;
   it consumes everything synchronously, so the copy is freed on return. */
static void store_entry(httrackp *opt, cache_back *cache, const char *adr,
                        const char *fil, const char *save, int statuscode,
                        const char *msg, const char *contenttype,
                        const char *charset, const char *lastmodified,
                        const char *etag, const char *location,
                        const char *body, size_t body_len) {
  htsblk r;
  char locbuf[HTS_URLMAXSIZE * 2];
  char *bodycopy = NULL;

  hts_init_htsblk(&r);
  r.statuscode = statuscode;
  r.size = (LLint) body_len;
  strcpybuff(r.msg, msg);
  strcpybuff(r.contenttype, contenttype);
  strcpybuff(r.charset, charset);
  strcpybuff(r.lastmodified, lastmodified);
  strcpybuff(r.etag, etag);
  strcpybuff(locbuf, location);
  r.location = locbuf;
  r.is_write = 0;
  /* an empty body must be a NULL pointer: cache_add rejects a non-NULL
     pointer with size 0 */
  if (body_len != 0) {
    bodycopy = malloct(body_len);
    memcpy(bodycopy, body, body_len);
    r.adr = bodycopy;
  }
  /* all_in_cache=1: keep the body in the ZIP whatever the content-type,
     so the read path never depends on a file on disk */
  cache_add(opt, cache, &r, adr, fil, save, 1, NULL);
  if (bodycopy != NULL) {
    freet(bodycopy);
  }
}

/* Read one entry back and check every field. Returns the number of
   mismatches (0 == success). */
static int check_entry(httrackp *opt, cache_back *cache, const char *adr,
                       const char *fil, int statuscode, const char *msg,
                       const char *contenttype, const char *charset,
                       const char *lastmodified, const char *etag,
                       const char *location, const char *body,
                       size_t body_len) {
  int fail = 0;
  char *locbuf = malloct(HTS_URLMAXSIZE * 2);
  htsblk r;

  locbuf[0] = '\0';
  /* readonly=1: pure read, no rename/disk-write decision logic */
  r = cache_readex(opt, cache, adr, fil, "", locbuf, NULL, 1);

#define CHECK_STR(field, want)                                                 \
  do {                                                                         \
    if (strcmp((field), (want)) != 0) {                                        \
      fprintf(stderr,                                                          \
              "cache-selftest: %s%s: " #field " is '%s', expected '%s'\n",     \
              adr, fil, (field), (want));                                      \
      fail++;                                                                  \
    }                                                                          \
  } while (0)

  if (r.statuscode != statuscode) {
    fprintf(stderr, "cache-selftest: %s%s: statuscode is %d, expected %d\n",
            adr, fil, r.statuscode, statuscode);
    fail++;
  }
  CHECK_STR(r.msg, msg);
  CHECK_STR(r.contenttype, contenttype);
  CHECK_STR(r.charset, charset);
  CHECK_STR(r.lastmodified, lastmodified);
  CHECK_STR(r.etag, etag);
  CHECK_STR(locbuf, location);

  if (r.size != (LLint) body_len) {
    fprintf(stderr, "cache-selftest: %s%s: size is " LLintP ", expected %d\n",
            adr, fil, (LLint) r.size, (int) body_len);
    fail++;
  } else if (body_len != 0 &&
             (r.adr == NULL || memcmp(r.adr, body, body_len) != 0)) {
    fprintf(stderr, "cache-selftest: %s%s: body mismatch\n", adr, fil);
    fail++;
  }

#undef CHECK_STR

  if (r.adr != NULL) {
    freet(r.adr);
  }
  freet(locbuf);
  return fail;
}

/* Fill a body of the requested size. kind 0 is highly compressible (a short
   repeating pattern), kind 1 is incompressible (a deterministic PRNG), kind 2
   alternates the two -- together they exercise both deflate outcomes. */
static void gen_body(char *buf, size_t len, int kind) {
  unsigned int seed = 0x9e3779b1u ^ (unsigned int) len;
  size_t j;

  for (j = 0; j < len; j++) {
    if (kind == 0 || (kind == 2 && (j & 1) == 0)) {
      buf[j] = (char) ('A' + (j % 26));
    } else {
      seed = seed * 1103515245u + 12345u;
      buf[j] = (char) (seed >> 16);
    }
  }
}

int cache_selftests(httrackp *opt, const char *dir) {
  int failures = 0;
  cache_back cache;
  int i;

  /* near-limit field values. The etag stresses htsblk.etag[256]; the location
     stresses a long redirect URL. Each cached header line is read back through
     a HTS_URLMAXSIZE-sized parse buffer ("<field>: <value>\r\n"), so the
     round-trippable value is shorter than HTS_URLMAXSIZE: 1000 stays safely
     under that real limit. */
  static char etag_long[251];
  static char location_long[1001];

  /* a body with embedded NUL and high bytes, to prove binary safety */
  static const char binary_body[] = {
      'P',  'N',  'G', '\0', '\r', '\n',        (char) 0xFF, (char) 0x80,
      '\0', '\0', 'e', 'n',  'd',  (char) 0xCA, (char) 0xFE, '\n'};

  /* large bodies for the compression pass; kept alive across the write and
     read passes so the read can compare against them */
  static const size_t large_size[] = {200000, 200000, 50000};
  const int large_count = (int) (sizeof(large_size) / sizeof(large_size[0]));
  char *large_body[3];

  /* edge-case bodies, named so store and read assert the exact same bytes */
  const char *const body_index = "<html><body>hello</body></html>";
  const char *const body_api = "{\"k\":\"v\"}";
  const char *const body_updated = "<html><body>UPDATED CONTENT</body></html>";
  const char *const body_404 = "<html><body>404 Not Found</body></html>";

  memset(etag_long, 'E', sizeof(etag_long) - 1);
  etag_long[sizeof(etag_long) - 1] = '\0';
  memset(location_long, 'L', sizeof(location_long) - 1);
  location_long[sizeof(location_long) - 1] = '\0';

  for (i = 0; i < large_count; i++) {
    large_body[i] = malloct(large_size[i]);
    gen_body(large_body[i], large_size[i], i);
  }

  /* set up an isolated cache directory */
  {
    char base[HTS_URLMAXSIZE];

    strcpybuff(base, dir);
    if (base[0] != '\0' && base[strlen(base) - 1] != '/') {
      strcatbuff(base, "/");
    }
    StringCopy(opt->path_log, base);
  }
  opt->cache = 1;

  /* pass 1: create everything in a single write session */
  selftest_open_for_write(&cache, opt);

  /* edge cases: normal HTML page */
  store_entry(opt, &cache, "example.com", "/", "example.com/index.html", 200,
              "OK", "text/html", "utf-8", "Mon, 01 Jan 2024 00:00:00 GMT",
              "etag-normal", "", body_index, strlen(body_index));
  /* redirect: empty body, empty optional fields, near-limit location */
  store_entry(opt, &cache, "example.com", "/moved", "example.com/moved.html",
              301, "Moved Permanently", "text/html", "", "", "", location_long,
              NULL, 0);
  /* non-HTML content-type kept in cache via all_in_cache, near-limit etag */
  store_entry(opt, &cache, "example.com", "/api", "example.com/api.json", 200,
              "OK", "application/json", "utf-8",
              "Tue, 02 Jan 2024 12:00:00 GMT", etag_long, "", body_api,
              strlen(body_api));
  /* binary body */
  store_entry(opt, &cache, "example.com", "/logo", "example.com/logo.png", 200,
              "OK", "image/png", "", "", "etag-bin", "", binary_body,
              sizeof(binary_body));
  /* error status with a body and a location (non-2xx codes are cached too) */
  store_entry(opt, &cache, "example.com", "/gone", "example.com/gone.html", 404,
              "Not Found", "text/html", "utf-8", "", "etag-404",
              "https://example.com/where-it-went", body_404, strlen(body_404));

  /* scale: a few thousand small entries */
  for (i = 0; i < SELFTEST_VOLUME; i++) {
    char fil[64], save[128], body[64];

    sprintf(fil, "/v/%05d", i);
    sprintf(save, "example.com/v/%05d.html", i);
    sprintf(body, "<html>volume entry %d</html>", i);
    store_entry(opt, &cache, "example.com", fil, save, 200, "OK", "text/html",
                "utf-8", "", "", "", body, strlen(body));
  }

  /* compression: a few large bodies */
  for (i = 0; i < large_count; i++) {
    char fil[64], save[128];

    sprintf(fil, "/big/%d.bin", i);
    sprintf(save, "example.com/big/%d.bin", i);
    store_entry(opt, &cache, "example.com", fil, save, 200, "OK",
                "application/octet-stream", "", "", "", "", large_body[i],
                large_size[i]);
  }

  selftest_close(&cache);

  /* pass 2: read back and verify everything round-tripped */
  selftest_open_for_read(&cache, opt);

  failures += check_entry(opt, &cache, "example.com", "/", 200, "OK",
                          "text/html", "utf-8", "Mon, 01 Jan 2024 00:00:00 GMT",
                          "etag-normal", "", body_index, strlen(body_index));
  failures += check_entry(opt, &cache, "example.com", "/moved", 301,
                          "Moved Permanently", "text/html", "", "", "",
                          location_long, NULL, 0);
  failures +=
      check_entry(opt, &cache, "example.com", "/api", 200, "OK",
                  "application/json", "utf-8", "Tue, 02 Jan 2024 12:00:00 GMT",
                  etag_long, "", body_api, strlen(body_api));
  failures +=
      check_entry(opt, &cache, "example.com", "/logo", 200, "OK", "image/png",
                  "", "", "etag-bin", "", binary_body, sizeof(binary_body));
  failures += check_entry(opt, &cache, "example.com", "/gone", 404, "Not Found",
                          "text/html", "utf-8", "", "etag-404",
                          "https://example.com/where-it-went", body_404,
                          strlen(body_404));

  for (i = 0; i < SELFTEST_VOLUME; i++) {
    char fil[64], body[64];

    sprintf(fil, "/v/%05d", i);
    sprintf(body, "<html>volume entry %d</html>", i);
    failures +=
        check_entry(opt, &cache, "example.com", fil, 200, "OK", "text/html",
                    "utf-8", "", "", "", body, strlen(body));
  }

  for (i = 0; i < large_count; i++) {
    char fil[64];

    sprintf(fil, "/big/%d.bin", i);
    failures += check_entry(opt, &cache, "example.com", fil, 200, "OK",
                            "application/octet-stream", "", "", "", "",
                            large_body[i], large_size[i]);
  }

  selftest_close(&cache);

  /* pass 3: update one edge entry with new body and headers */
  selftest_open_for_write(&cache, opt);
  store_entry(opt, &cache, "example.com", "/", "example.com/index.html", 200,
              "OK", "text/html", "iso-8859-1", "Wed, 03 Jan 2024 09:30:00 GMT",
              "etag-updated", "", body_updated, strlen(body_updated));
  selftest_close(&cache);

  /* pass 4: re-read and confirm the updated value, not the old one */
  selftest_open_for_read(&cache, opt);
  failures +=
      check_entry(opt, &cache, "example.com", "/", 200, "OK", "text/html",
                  "iso-8859-1", "Wed, 03 Jan 2024 09:30:00 GMT", "etag-updated",
                  "", body_updated, strlen(body_updated));
  selftest_close(&cache);

  for (i = 0; i < large_count; i++) {
    freet(large_body[i]);
  }

  return failures;
}
