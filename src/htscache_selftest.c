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

#include <errno.h>
#include <stdio.h>
#include <string.h>

#define SELFTEST_VOLUME 3000 /* number of small entries in the scale pass */

/* prefix on assertion failures; set per entry point (-#A vs -#B) */
static const char *selftest_tag = "cache-selftest";

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

/* Store one entry; the body is copied so callers may pass const data. */
static void store_entry(httrackp *opt, cache_back *cache, const char *adr,
                        const char *fil, const char *save, int statuscode,
                        const char *msg, const char *contenttype,
                        const char *charset, const char *lastmodified,
                        const char *etag, const char *location,
                        const char *cdispo, const char *body, size_t body_len) {
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
  strcpybuff(r.cdispo, cdispo);
  strcpybuff(locbuf, location);
  r.location = locbuf;
  r.is_write = 0;
  /* an empty body must be NULL: cache_add rejects non-NULL with size 0 */
  if (body_len != 0) {
    bodycopy = malloct(body_len);
    memcpy(bodycopy, body, body_len);
    r.adr = bodycopy;
  }
  /* all_in_cache=1: body stays in the ZIP, so reads never need a disk file */
  cache_add(opt, cache, &r, adr, fil, save, 1, NULL);
  if (bodycopy != NULL) {
    freet(bodycopy);
  }
}

/* Read one entry back and check every field. Returns the mismatch count. */
static int check_entry(httrackp *opt, cache_back *cache, const char *adr,
                       const char *fil, int statuscode, const char *msg,
                       const char *contenttype, const char *charset,
                       const char *lastmodified, const char *etag,
                       const char *location, const char *cdispo,
                       const char *body, size_t body_len) {
  int fail = 0;
  char *locbuf = malloct(HTS_URLMAXSIZE * 2);
  htsblk r;

  locbuf[0] = '\0';
  /* readonly=1: pure read, no rename/disk-write decision logic */
  r = cache_readex(opt, cache, adr, fil, "", locbuf, NULL, 1);

#define CHECK_STR(field, want)                                                 \
  do {                                                                         \
    if (strcmp((field), (want)) != 0) {                                        \
      fprintf(stderr, "%s: %s%s: " #field " is '%s', expected '%s'\n",         \
              selftest_tag, adr, fil, (field), (want));                        \
      fail++;                                                                  \
    }                                                                          \
  } while (0)

  if (r.statuscode != statuscode) {
    fprintf(stderr, "%s: %s%s: statuscode is %d, expected %d\n", selftest_tag,
            adr, fil, r.statuscode, statuscode);
    fail++;
  }
  CHECK_STR(r.msg, msg);
  CHECK_STR(r.contenttype, contenttype);
  CHECK_STR(r.charset, charset);
  CHECK_STR(r.lastmodified, lastmodified);
  CHECK_STR(r.etag, etag);
  CHECK_STR(locbuf, location);
  CHECK_STR(r.cdispo, cdispo);

  if (r.size != (LLint) body_len) {
    fprintf(stderr, "%s: %s%s: size is " LLintP ", expected %d\n", selftest_tag,
            adr, fil, (LLint) r.size, (int) body_len);
    fail++;
  } else if (body_len != 0 &&
             (r.adr == NULL || memcmp(r.adr, body, body_len) != 0)) {
    fprintf(stderr, "%s: %s%s: body mismatch\n", selftest_tag, adr, fil);
    fail++;
  }

  /* loaded body must be NUL-terminated at [size] for cache_readex's strlen()
     consumers; buffer is malloc(size + slack) so [size] is in bounds */
  if (r.adr != NULL && r.adr[r.size] != '\0') {
    fprintf(stderr, "%s: %s%s: body not NUL-terminated at [size]\n",
            selftest_tag, adr, fil);
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

/* Exercise the disk-fallback read path: a record stored with X-In-Cache: 0
   keeps its body on disk (not in the ZIP), and cache_readex must load it from
   there. The one-shot crawl tests never re-read such a body into memory, so
   this path otherwise has no runtime coverage. We store the header with
   all_in_cache=0 and a non-hypertext content-type (-> X-In-Cache: 0), create
   the body at the exact fconv()-resolved path the reader uses, then read it
   back and assert it round-trips and is NUL-terminated. */
static int disk_fallback_selftest(httrackp *opt) {
  int fail = 0;
  cache_back cache;
  htsblk r;
  char catbuff[HTS_URLMAXSIZE * 2];
  char *path;
  char *locbuf;
  FILE *fp;
  const char *const adr = "example.com";
  const char *const fil = "/blob.bin";
  char save[HTS_URLMAXSIZE * 2];
  /* no embedded NUL: were the read to leave this un-terminated, a later
     strlen() would run off the end (the bug this guards) */
  static const char body[] = "BINARY-on-disk-body-0123456789-no-trailing-nul";
  const size_t body_len = sizeof(body) - 1;

  /* X-Save must start with path_html_utf8 so the reader resolves it verbatim
     (otherwise it re-roots it as a pre-3.40 relative path); then the body we
     create at fconv(save) is exactly where cache_readex looks for it. */
  fconcat(save, sizeof(save), StringBuff(opt->path_html_utf8),
          "example.com/blob.bin");

  /* write only the header (X-In-Cache: 0); the body stays on disk */
  selftest_open_for_write(&cache, opt);
  {
    htsblk w;
    char locw[4];
    char *bodycopy = malloct(body_len);

    hts_init_htsblk(&w);
    w.statuscode = 200;
    w.size = (LLint) body_len;
    strcpybuff(w.msg, "OK");
    strcpybuff(w.contenttype, "application/octet-stream");
    locw[0] = '\0';
    w.location = locw;
    w.is_write = 0;
    memcpy(bodycopy, body, body_len);
    w.adr = bodycopy;
    cache_add(opt, &cache, &w, adr, fil, save, 0 /* all_in_cache */, NULL);
    freet(bodycopy);
  }
  selftest_close(&cache);

  /* create the on-disk body where the reader will look for it */
  path = fconv(catbuff, sizeof(catbuff), save);
  (void) structcheck(path);
  fp = FOPEN(path, "wb");
  if (fp == NULL) {
    fprintf(stderr, "cache-selftest: disk-fallback: cannot create '%s'\n",
            path);
    return 1;
  }
  if (fwrite(body, 1, body_len, fp) != body_len) {
    fprintf(stderr, "cache-selftest: disk-fallback: short write to '%s'\n",
            path);
    fail++;
  }
  fclose(fp);

  /* read it back: takes the X-In-Cache: 0 disk-fallback branch */
  selftest_open_for_read(&cache, opt);
  locbuf = malloct(HTS_URLMAXSIZE * 2);
  locbuf[0] = '\0';
  r = cache_readex(opt, &cache, adr, fil, "", locbuf, NULL, 1);
  if (r.statuscode != 200) {
    fprintf(stderr,
            "cache-selftest: disk-fallback: statuscode %d, expected 200"
            " (path not taken or read failed)\n",
            r.statuscode);
    fail++;
  }
  if (r.size != (LLint) body_len) {
    fprintf(stderr,
            "cache-selftest: disk-fallback: size " LLintP ", expected %d\n",
            (LLint) r.size, (int) body_len);
    fail++;
  } else if (r.adr == NULL || memcmp(r.adr, body, body_len) != 0) {
    fprintf(stderr, "cache-selftest: disk-fallback: body mismatch\n");
    fail++;
  }
  /* the loaded body must be NUL-terminated at [size] */
  if (r.adr != NULL && r.adr[r.size] != '\0') {
    fprintf(stderr, "cache-selftest: disk-fallback: body not NUL-terminated\n");
    fail++;
  }
  if (r.adr != NULL) {
    freet(r.adr);
  }
  freet(locbuf);
  selftest_close(&cache);
  return fail;
}

typedef struct {
  size_t budget;  /**< bytes allowed through before writes start failing */
  int fail_errno; /**< errno set on the failing write (ENOSPC, EIO, ...) */
  int writes;     /**< zwrite call count, to detect re-entry into the stream */
} writefail_inject;

/* zwrite that copies until the budget runs out, then fails with inj->fail_errno
   (the #174/#219 condition). Counts calls so the test can prove a flagged cache
   never re-enters the stream. */
static uLong selftest_failing_zwrite(voidpf opaque, voidpf stream,
                                     const void *buf, uLong size) {
  writefail_inject *inj = (writefail_inject *) opaque;

  inj->writes++;
  if (inj->budget >= (size_t) size) {
    inj->budget -= (size_t) size;
    return (uLong) fwrite(buf, 1, (size_t) size, (FILE *) stream);
  }
  errno = inj->fail_errno;
  return 0; /* short write -> the minizip op returns an error */
}

/* Open a ZIP whose writes fail past inj->budget, so cache_add() hits an error.
 */
static zipFile selftest_open_failing_zip(const char *path,
                                         writefail_inject *inj) {
  zlib_filefunc_def ff;

  fill_fopen_filefunc(&ff); /* real fopen/read/seek/close; ignores opaque */
  ff.zwrite_file = selftest_failing_zwrite;
  ff.opaque = inj;
  return zipOpen2(path, APPEND_STATUS_CREATE, NULL, &ff);
}

/* Store one octet-stream body into `cache` (all-in-cache, body in the ZIP). */
static void writefail_store(httrackp *opt, cache_back *cache, const char *fil,
                            const char *body, size_t body_len) {
  htsblk r;
  char locbuf[4];
  char *bodycopy = malloct(body_len);

  hts_init_htsblk(&r);
  r.statuscode = 200;
  r.size = (LLint) body_len;
  strcpybuff(r.msg, "OK");
  strcpybuff(r.contenttype, "application/octet-stream");
  locbuf[0] = '\0';
  r.location = locbuf;
  r.is_write = 0;
  memcpy(bodycopy, body, body_len);
  r.adr = bodycopy;
  cache_add(opt, cache, &r, "example.com", fil, "example.com/blob.bin", 1,
            NULL);
  freet(bodycopy);
}

/* #174/#219: a failing cache write used to crash via assertf(); it must instead
   stop the mirror (exit_xh = -1) without crashing. Assert that, plus the cache
   is flagged and a sibling write doesn't re-enter the broken stream. */
int cache_write_failure_selftest(httrackp *opt, const char *dir) {
  int fail = 0;
  char path[HTS_URLMAXSIZE];
  /* incompressible + big, so deflate flushes (and fails) mid-write, before
   * close */
  static const size_t body_len = 256 * 1024;
  char *body = malloct(body_len);
  int phase;

  gen_body(body, body_len, 1 /* incompressible */);
  fconcat(path, sizeof(path), dir, "/wfail.zip");

  /* phase 0: fail on the body write, fatal errno (ENOSPC, the disk-full
     branch). phase 1: fail on the open, non-fatal errno (EIO, dropped-share
     branch). Both must abort the mirror. */
  for (phase = 0; phase < 2; phase++) {
    cache_back cache;
    writefail_inject inj;
    int writes_after_fail;

    inj.budget = (phase == 0) ? 4096 : 0;
    inj.fail_errno = (phase == 0) ? ENOSPC : EIO;
    inj.writes = 0;
    memset(&cache, 0, sizeof(cache));
    cache.type = 1;
    cache.log = stderr;
    cache.errlog = stderr;
    cache.hashtable = coucal_new(0);
    cache.zipOutput = selftest_open_failing_zip(path, &inj);
    if (cache.zipOutput == NULL) {
      fprintf(stderr, "cache-writefail: could not open injected ZIP\n");
      fail++;
      continue;
    }

    opt->state.exit_xh = 0; /* clear; the failing write must set it to -1 */
    writefail_store(opt, &cache, "/blob.bin", body, body_len);
    if (!cache.zipWriteFailed) {
      fprintf(stderr, "cache-writefail: phase %d: write error not caught\n",
              phase);
      fail++;
    }
    if (opt->state.exit_xh != -1) {
      fprintf(stderr,
              "cache-writefail: phase %d: mirror not aborted (exit_xh=%d)\n",
              phase, opt->state.exit_xh);
      fail++;
    }

    /* a flagged cache must no-op a sibling write: no further backend write */
    writes_after_fail = inj.writes;
    writefail_store(opt, &cache, "/blob2.bin", body, 16);
    if (inj.writes != writes_after_fail) {
      fprintf(stderr,
              "cache-writefail: phase %d: sibling write re-entered the broken "
              "stream (%d extra backend writes)\n",
              phase, inj.writes - writes_after_fail);
      fail++;
    }

    if (cache.zipOutput != NULL) {
      zipClose(cache.zipOutput,
               NULL); /* best-effort; may fail on the backend */
      cache.zipOutput = NULL;
    }
  }

  freet(body);
  return fail;
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
    /* the disk-fallback pass resolves on-disk body paths through fconv(), which
       is rooted at path_html; keep it inside the test directory too */
    StringCopy(opt->path_html, base);
    StringCopy(opt->path_html_utf8, base);
  }
  opt->cache = HTS_CACHE_PRIORITY;

  /* pass 1: create everything in a single write session */
  selftest_open_for_write(&cache, opt);

  /* edge cases (cdispo "" except where noted): normal HTML page */
  store_entry(opt, &cache, "example.com", "/", "example.com/index.html", 200,
              "OK", "text/html", "utf-8", "Mon, 01 Jan 2024 00:00:00 GMT",
              "etag-normal", "", "", body_index, strlen(body_index));
  /* redirect: empty body, empty optional fields, near-limit location */
  store_entry(opt, &cache, "example.com", "/moved", "example.com/moved.html",
              301, "Moved Permanently", "text/html", "", "", "", location_long,
              "", NULL, 0);
  /* non-HTML content-type, near-limit etag */
  store_entry(opt, &cache, "example.com", "/api", "example.com/api.json", 200,
              "OK", "application/json", "utf-8",
              "Tue, 02 Jan 2024 12:00:00 GMT", etag_long, "", "", body_api,
              strlen(body_api));
  /* binary body, with a Content-Disposition */
  store_entry(opt, &cache, "example.com", "/logo", "example.com/logo.png", 200,
              "OK", "image/png", "", "", "etag-bin", "",
              "attachment; filename=\"logo.png\"", binary_body,
              sizeof(binary_body));
  /* error status with a body and a location */
  store_entry(opt, &cache, "example.com", "/gone", "example.com/gone.html", 404,
              "Not Found", "text/html", "utf-8", "", "etag-404",
              "https://example.com/where-it-went", "", body_404,
              strlen(body_404));

  /* scale: a few thousand small entries */
  for (i = 0; i < SELFTEST_VOLUME; i++) {
    char fil[64], save[128], body[64];

    sprintf(fil, "/v/%05d", i);
    sprintf(save, "example.com/v/%05d.html", i);
    sprintf(body, "<html>volume entry %d</html>", i);
    store_entry(opt, &cache, "example.com", fil, save, 200, "OK", "text/html",
                "utf-8", "", "", "", "", body, strlen(body));
  }

  /* compression: a few large bodies */
  for (i = 0; i < large_count; i++) {
    char fil[64], save[128];

    sprintf(fil, "/big/%d.bin", i);
    sprintf(save, "example.com/big/%d.bin", i);
    store_entry(opt, &cache, "example.com", fil, save, 200, "OK",
                "application/octet-stream", "", "", "", "", "", large_body[i],
                large_size[i]);
  }

  selftest_close(&cache);

  /* pass 2: read back and verify everything round-tripped */
  selftest_open_for_read(&cache, opt);

  failures +=
      check_entry(opt, &cache, "example.com", "/", 200, "OK", "text/html",
                  "utf-8", "Mon, 01 Jan 2024 00:00:00 GMT", "etag-normal", "",
                  "", body_index, strlen(body_index));
  failures += check_entry(opt, &cache, "example.com", "/moved", 301,
                          "Moved Permanently", "text/html", "", "", "",
                          location_long, "", NULL, 0);
  failures +=
      check_entry(opt, &cache, "example.com", "/api", 200, "OK",
                  "application/json", "utf-8", "Tue, 02 Jan 2024 12:00:00 GMT",
                  etag_long, "", "", body_api, strlen(body_api));
  failures +=
      check_entry(opt, &cache, "example.com", "/logo", 200, "OK", "image/png",
                  "", "", "etag-bin", "", "attachment; filename=\"logo.png\"",
                  binary_body, sizeof(binary_body));
  failures += check_entry(opt, &cache, "example.com", "/gone", 404, "Not Found",
                          "text/html", "utf-8", "", "etag-404",
                          "https://example.com/where-it-went", "", body_404,
                          strlen(body_404));

  for (i = 0; i < SELFTEST_VOLUME; i++) {
    char fil[64], body[64];

    sprintf(fil, "/v/%05d", i);
    sprintf(body, "<html>volume entry %d</html>", i);
    failures +=
        check_entry(opt, &cache, "example.com", fil, 200, "OK", "text/html",
                    "utf-8", "", "", "", "", body, strlen(body));
  }

  for (i = 0; i < large_count; i++) {
    char fil[64];

    sprintf(fil, "/big/%d.bin", i);
    failures += check_entry(opt, &cache, "example.com", fil, 200, "OK",
                            "application/octet-stream", "", "", "", "", "",
                            large_body[i], large_size[i]);
  }

  selftest_close(&cache);

  /* pass 3: update one edge entry with new body and headers */
  selftest_open_for_write(&cache, opt);
  store_entry(opt, &cache, "example.com", "/", "example.com/index.html", 200,
              "OK", "text/html", "iso-8859-1", "Wed, 03 Jan 2024 09:30:00 GMT",
              "etag-updated", "", "", body_updated, strlen(body_updated));
  selftest_close(&cache);

  /* pass 4: re-read and confirm the updated value, not the old one */
  selftest_open_for_read(&cache, opt);
  failures +=
      check_entry(opt, &cache, "example.com", "/", 200, "OK", "text/html",
                  "iso-8859-1", "Wed, 03 Jan 2024 09:30:00 GMT", "etag-updated",
                  "", "", body_updated, strlen(body_updated));
  selftest_close(&cache);

  /* pass 5: the disk-fallback read path (X-In-Cache: 0, body on disk) */
  failures += disk_fallback_selftest(opt);

  for (i = 0; i < large_count; i++) {
    freet(large_body[i]);
  }

  return failures;
}

/* Golden fixture: a small frozen cache read back to guard the read path and ZIP
   format. The table is the contract; tests/fixtures/cache-golden/.../new.zip is
   a witness written once via `httrack -#B <dir> regen`. Bodies stay in the ZIP
   (all_in_cache=1), so a read needs only new.zip -- fully portable. */

/* embedded NUL + high bytes: the binary-safe read path */
static const char golden_binary[] = {
    'P',  'N',  'G', '\0', '\r', '\n',        (char) 0xFF, (char) 0x80,
    '\0', '\0', 'e', 'n',  'd',  (char) 0xCA, (char) 0xFE, '\n'};

typedef struct {
  const char *adr, *fil, *save, *msg, *contenttype, *charset, *lastmodified,
      *etag, *location, *cdispo, *body;
  size_t body_len;
  int statuscode;
} golden_entry;

/* string-literal body + length (drops the terminator); the binary array passes
   its length explicitly, every byte counts */
#define GBODY(s) (s), (sizeof(s) - 1)

static const golden_entry golden_entries[] = {
    /* normal HTML page */
    {"example.com", "/", "example.com/index.html", "OK", "text/html", "utf-8",
     "Mon, 01 Jan 2024 00:00:00 GMT", "etag-normal", "", "",
     GBODY("<html><body>hello</body></html>"), 200},
    /* redirect: empty body and optionals, a Location */
    {"example.com", "/moved", "example.com/moved.html", "Moved Permanently",
     "text/html", "", "", "", "https://example.com/new-home", "", NULL, 0, 301},
    /* non-HTML content */
    {"example.com", "/api", "example.com/api.json", "OK", "application/json",
     "utf-8", "Tue, 02 Jan 2024 12:00:00 GMT", "etag-api", "", "",
     GBODY("{\"k\":\"v\"}"), 200},
    /* binary body with a Content-Disposition */
    {"example.com", "/logo", "example.com/logo.png", "OK", "image/png", "", "",
     "etag-bin", "", "attachment; filename=\"logo.png\"", golden_binary,
     sizeof(golden_binary), 200},
    /* error status with a body and a Location */
    {"example.com", "/gone", "example.com/gone.html", "Not Found", "text/html",
     "utf-8", "", "etag-404", "https://example.com/where-it-went", "",
     GBODY("<html><body>404 Not Found</body></html>"), 404},
};

#define GOLDEN_COUNT (sizeof(golden_entries) / sizeof(golden_entries[0]))

static void golden_setup(httrackp *opt, const char *dir) {
  char base[HTS_URLMAXSIZE];

  strcpybuff(base, dir);
  if (base[0] != '\0' && base[strlen(base) - 1] != '/') {
    strcatbuff(base, "/");
  }
  StringCopy(opt->path_log, base);
  StringCopy(opt->path_html, base);
  StringCopy(opt->path_html_utf8, base);
  opt->cache = HTS_CACHE_PRIORITY;
}

int cache_golden_selftest(httrackp *opt, const char *dir, int regen) {
  int failures = 0;
  size_t k;
  cache_back cache;

  selftest_tag = "cache-golden";
  golden_setup(opt, dir);

  /* regen rewrites the fixture from the table; the test never passes it, so the
     read pass verifies bytes a previous build froze */
  if (regen) {
    selftest_open_for_write(&cache, opt);
    for (k = 0; k < GOLDEN_COUNT; k++) {
      const golden_entry *e = &golden_entries[k];

      store_entry(opt, &cache, e->adr, e->fil, e->save, e->statuscode, e->msg,
                  e->contenttype, e->charset, e->lastmodified, e->etag,
                  e->location, e->cdispo, e->body, e->body_len);
    }
    selftest_close(&cache);
  }

  selftest_open_for_read(&cache, opt);
  for (k = 0; k < GOLDEN_COUNT; k++) {
    const golden_entry *e = &golden_entries[k];

    failures +=
        check_entry(opt, &cache, e->adr, e->fil, e->statuscode, e->msg,
                    e->contenttype, e->charset, e->lastmodified, e->etag,
                    e->location, e->cdispo, e->body, e->body_len);
  }
  selftest_close(&cache);

  return failures;
}
