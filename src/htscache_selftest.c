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
#include <limits.h>
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

  /* a DOS-ified X-Save loses the path_html_utf8 prefix the reader matches on,
     and gets re-rooted as a pre-3.40 relative path; fconv() only on access */
  concat(save, sizeof(save), StringBuff(opt->path_html_utf8),
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
  int fail_once;  /**< recover (unlimited budget) after the first failure */
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
  if (inj->fail_once)
    inj->budget = (size_t) -1; /* the backend recovers after this failure */
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

/* Store an entry claiming a >2GB body; the degrade path never reads data. */
static void writefail_store_oversized(httrackp *opt, cache_back *cache,
                                      const char *fil, int is_write) {
  htsblk r;
  char locbuf[4];

  hts_init_htsblk(&r);
  r.statuscode = 200;
  r.size = (LLint) INT_MAX + 1;
  strcpybuff(r.msg, "OK");
  strcpybuff(r.contenttype, "application/octet-stream");
  locbuf[0] = '\0';
  r.location = locbuf;
  r.is_write = (short int) is_write;
  cache_add(opt, cache, &r, "example.com", fil, "example.com/big.bin", 1, NULL);
}

/* Read back `entryname`: extra field (cached headers) and body. Returns the
   body length, or -1 if the entry is absent or unreadable. */
static int writefail_read_entry(const char *path, const char *entryname,
                                char *extra, size_t extralen, char *body,
                                size_t bodylen) {
  unzFile z = unzOpen(path);
  int n = -1;

  if (z == NULL)
    return -1;
  if (unzLocateFile(z, entryname, 1) == UNZ_OK &&
      unzOpenCurrentFile(z) == UNZ_OK) {
    const int elen = unzGetLocalExtrafield(z, extra, (unsigned) (extralen - 1));

    if (elen >= 0) {
      extra[elen] = '\0';
      n = unzReadCurrentFile(z, body, (unsigned) bodylen);
    }
    unzCloseCurrentFile(z);
  }
  unzClose(z);
  return n;
}

/* Cache write-failure policy (#174/#219): fatal errno or a failure streak
   stops the mirror (exit_xh=-1, no crash); isolated/oversized drops the entry.
 */
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

  /* phase 0: fatal errno (ENOSPC) aborts at once; phase 1: persistent EIO
     drops entries until the streak caps out, then aborts. */
  for (phase = 0; phase < 2; phase++) {
    cache_back cache;
    writefail_inject inj;
    int writes_after_fail;

    inj.budget = (phase == 0) ? 4096 : 0;
    inj.fail_errno = (phase == 0) ? ENOSPC : EIO;
    inj.writes = 0;
    inj.fail_once = 0;
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
    if (phase == 0) {
      writefail_store(opt, &cache, "/blob.bin", body, body_len);
    } else {
      /* the abort must land exactly on the 8th consecutive failure */
      int i;

      for (i = 0; i < 7; i++) {
        char fil[32];

        snprintf(fil, sizeof(fil), "/b%d.bin", i);
        writefail_store(opt, &cache, fil, body, 16);
      }
      if (cache.zipWriteFailed) {
        fprintf(stderr, "cache-writefail: phase 1: aborted before the "
                        "8th consecutive failure\n");
        fail++;
      }
      writefail_store(opt, &cache, "/b7.bin", body, 16);
    }
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

  /* failures with successes in between reset the streak: never aborts */
  {
    cache_back cache;
    writefail_inject inj;
    int i;

    inj.budget = (size_t) -1;
    inj.fail_errno = EIO;
    inj.writes = 0;
    inj.fail_once = 0;
    memset(&cache, 0, sizeof(cache));
    cache.type = 1;
    cache.log = stderr;
    cache.errlog = stderr;
    cache.hashtable = coucal_new(0);
    cache.zipOutput = selftest_open_failing_zip(path, &inj);
    opt->state.exit_xh = 0;

    for (i = 0; i < 10; i++) {
      char fil[32];

      inj.budget = 0; /* this store fails */
      snprintf(fil, sizeof(fil), "/s%d.bin", i);
      writefail_store(opt, &cache, fil, body, 16);
      inj.budget = (size_t) -1; /* this one succeeds and resets the streak */
      snprintf(fil, sizeof(fil), "/ok%d.bin", i);
      writefail_store(opt, &cache, fil, body, 16);
    }
    if (cache.zipWriteFailed || opt->state.exit_xh != 0) {
      fprintf(stderr,
              "cache-writefail: scattered: non-consecutive failures aborted "
              "the mirror (flagged=%d, exit_xh=%d)\n",
              (int) cache.zipWriteFailed, opt->state.exit_xh);
      fail++;
    }
    zipClose(cache.zipOutput, NULL);
    cache.zipOutput = NULL;
  }

  /* isolated failure: only that entry drops; a later sibling round-trips */
  {
    cache_back cache;
    writefail_inject inj;
    char extra[8192];
    char rbody[64];
    int n;

    inj.budget = 4096;
    inj.fail_errno = EIO;
    inj.writes = 0;
    inj.fail_once = 1;
    memset(&cache, 0, sizeof(cache));
    cache.type = 1;
    cache.log = stderr;
    cache.errlog = stderr;
    cache.hashtable = coucal_new(0);
    cache.zipOutput = selftest_open_failing_zip(path, &inj);
    opt->state.exit_xh = 0;

    writefail_store(opt, &cache, "/blob.bin", body, body_len);
    if (cache.zipWriteFailed || opt->state.exit_xh != 0) {
      fprintf(stderr,
              "cache-writefail: skip: isolated failure aborted the mirror "
              "(flagged=%d, exit_xh=%d)\n",
              (int) cache.zipWriteFailed, opt->state.exit_xh);
      fail++;
    }
    writefail_store(opt, &cache, "/blob2.bin", body, 16);
    zipClose(cache.zipOutput, NULL);
    cache.zipOutput = NULL;
    n = writefail_read_entry(path, "http://example.com/blob2.bin", extra,
                             sizeof(extra), rbody, sizeof(rbody));
    if (n != 16 || memcmp(rbody, body, 16) != 0) {
      fprintf(stderr,
              "cache-writefail: skip: sibling entry lost after a skipped "
              "entry (%d)\n",
              n);
      fail++;
    }
  }

  /* >2GB bodies: in-memory drops the entry, on-disk degrades to headers-only */
  {
    cache_back cache;
    writefail_inject inj;
    char extra[8192];
    char rbody[64];
    int n;

    inj.budget = (size_t) -1; /* no injected failure */
    inj.fail_errno = 0;
    inj.writes = 0;
    inj.fail_once = 0;
    memset(&cache, 0, sizeof(cache));
    cache.type = 1;
    cache.log = stderr;
    cache.errlog = stderr;
    cache.hashtable = coucal_new(0);
    cache.zipOutput = selftest_open_failing_zip(path, &inj);
    opt->state.exit_xh = 0;

    writefail_store_oversized(opt, &cache, "/bigmem.bin", 0 /* in-memory */);
    writefail_store_oversized(opt, &cache, "/bigdisk.bin", 1 /* on-disk */);
    zipClose(cache.zipOutput, NULL);
    cache.zipOutput = NULL;

    if (cache.zipWriteFailed || opt->state.exit_xh != 0) {
      fprintf(stderr,
              "cache-writefail: oversize: mirror aborted (flagged=%d, "
              "exit_xh=%d)\n",
              (int) cache.zipWriteFailed, opt->state.exit_xh);
      fail++;
    }
    if (writefail_read_entry(path, "http://example.com/bigmem.bin", extra,
                             sizeof(extra), rbody, sizeof(rbody)) >= 0) {
      fprintf(stderr,
              "cache-writefail: oversize: in-memory entry was stored\n");
      fail++;
    }
    n = writefail_read_entry(path, "http://example.com/bigdisk.bin", extra,
                             sizeof(extra), rbody, sizeof(rbody));
    if (n != 0 || strstr(extra, "X-In-Cache: 0") == NULL) {
      fprintf(stderr,
              "cache-writefail: oversize: on-disk entry not stored "
              "headers-only (%d)\n",
              n);
      fail++;
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

/* --- hts_cache_reconcile() policies -------------------------------------- */

/* All reconcile inputs/outputs, wiped between cases. */
static const char *const reconcile_files[] = {
    "hts-cache/new.zip", "hts-cache/old.zip",   "hts-cache/new.dat",
    "hts-cache/old.dat", "hts-cache/new.ndx",   "hts-cache/old.ndx",
    "hts-cache/new.lst", "hts-cache/old.lst",   "hts-cache/new.txt",
    "hts-cache/old.txt", "hts-in_progress.lock"};

static char *reconcile_st_path(httrackp *opt, const char *name) {
  return fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                 StringBuff(opt->path_log), name);
}

static void reconcile_wipe(httrackp *opt) {
  size_t i;

  for (i = 0; i < sizeof(reconcile_files) / sizeof(reconcile_files[0]); i++)
    remove(reconcile_st_path(opt, reconcile_files[i]));
}

/* Create a filler file of exactly `size` bytes. */
static void reconcile_put(httrackp *opt, const char *name, size_t size) {
  FILE *const fp = fopen(reconcile_st_path(opt, name), "wb");
  static const char filler[1024] = {'x'};

  assertf(fp != NULL);
  while (size > 0) {
    const size_t n = size > sizeof(filler) ? sizeof(filler) : size;

    assertf(fwrite(filler, 1, n, fp) == n);
    size -= n;
  }
  fclose(fp);
}

/* Expect `name` to weigh `size` bytes, or be absent when size == -1. */
static int reconcile_expect(httrackp *opt, const char *name, off_t size,
                            const char *what) {
  const off_t got = fsize(reconcile_st_path(opt, name));

  if (got != size) {
    fprintf(stderr, "cache-reconcile: %s: %s is %d bytes, expected %d\n", what,
            name, (int) got, (int) size);
    return 1;
  }
  return 0;
}

int cache_reconcile_selftest(httrackp *opt, const char *dir) {
  int failures = 0;

  /* around the interrupted-run thresholds (new < 32768, old > 65536) */
  static const off_t TINY = 1024, MID = 40000, SOLID = 131072;

  golden_setup(opt, dir);
#ifdef _WIN32
  mkdir(reconcile_st_path(opt, "hts-cache"));
#else
  mkdir(reconcile_st_path(opt, "hts-cache"), HTS_PROTECT_FOLDER);
#endif

  /* PROMOTE: a zip old generation replaces a missing new one */
  reconcile_wipe(opt);
  reconcile_put(opt, "hts-cache/old.zip", SOLID);
  hts_cache_reconcile(opt, CACHE_RECONCILE_PROMOTE);
  failures += reconcile_expect(opt, "hts-cache/new.zip", SOLID, "promote-zip");
  failures += reconcile_expect(opt, "hts-cache/old.zip", -1, "promote-zip");

  /* PROMOTE: an existing new.zip is left alone */
  reconcile_wipe(opt);
  reconcile_put(opt, "hts-cache/new.zip", TINY);
  reconcile_put(opt, "hts-cache/old.zip", SOLID);
  hts_cache_reconcile(opt, CACHE_RECONCILE_PROMOTE);
  failures +=
      reconcile_expect(opt, "hts-cache/new.zip", TINY, "promote-zip-noop");
  failures +=
      reconcile_expect(opt, "hts-cache/old.zip", SOLID, "promote-zip-noop");

  /* PROMOTE: the removed pre-3.31 legacy pair is left alone */
  reconcile_wipe(opt);
  reconcile_put(opt, "hts-cache/old.dat", SOLID);
  reconcile_put(opt, "hts-cache/old.ndx", TINY);
  hts_cache_reconcile(opt, CACHE_RECONCILE_PROMOTE);
  failures += reconcile_expect(opt, "hts-cache/new.dat", -1, "promote-dat");
  failures += reconcile_expect(opt, "hts-cache/new.ndx", -1, "promote-dat");
  failures += reconcile_expect(opt, "hts-cache/old.dat", SOLID, "promote-dat");
  failures += reconcile_expect(opt, "hts-cache/old.ndx", TINY, "promote-dat");

  /* INTERRUPTED: no lock file, no action */
  reconcile_wipe(opt);
  reconcile_put(opt, "hts-cache/new.zip", TINY);
  reconcile_put(opt, "hts-cache/old.zip", SOLID);
  hts_cache_reconcile(opt, CACHE_RECONCILE_INTERRUPTED);
  failures +=
      reconcile_expect(opt, "hts-cache/new.zip", TINY, "interrupted-nolock");

  /* INTERRUPTED: an absent new.zip must NOT promote old.zip (fsize(-1) would
     spuriously pass "< TINY"); leave the solid old generation for ROLLBACK */
  reconcile_wipe(opt);
  reconcile_put(opt, "hts-in_progress.lock", 0);
  reconcile_put(opt, "hts-cache/old.zip", SOLID);
  hts_cache_reconcile(opt, CACHE_RECONCILE_INTERRUPTED);
  failures +=
      reconcile_expect(opt, "hts-cache/new.zip", -1, "interrupted-nonew");
  failures +=
      reconcile_expect(opt, "hts-cache/old.zip", SOLID, "interrupted-nonew");

  /* INTERRUPTED: stalled tiny new.zip loses to a solid old.zip (was dead for
     zip caches: the arm was gated on a legacy new.dat) */
  reconcile_wipe(opt);
  reconcile_put(opt, "hts-in_progress.lock", 0);
  reconcile_put(opt, "hts-cache/new.zip", TINY);
  reconcile_put(opt, "hts-cache/old.zip", SOLID);
  hts_cache_reconcile(opt, CACHE_RECONCILE_INTERRUPTED);
  failures +=
      reconcile_expect(opt, "hts-cache/new.zip", SOLID, "interrupted-zip");
  failures += reconcile_expect(opt, "hts-cache/old.zip", -1, "interrupted-zip");

  /* INTERRUPTED: old below the confidence threshold, keep new */
  reconcile_wipe(opt);
  reconcile_put(opt, "hts-in_progress.lock", 0);
  reconcile_put(opt, "hts-cache/new.zip", TINY);
  reconcile_put(opt, "hts-cache/old.zip", MID);
  hts_cache_reconcile(opt, CACHE_RECONCILE_INTERRUPTED);
  failures +=
      reconcile_expect(opt, "hts-cache/new.zip", TINY, "interrupted-smallold");

  /* INTERRUPTED: new big enough to trust, keep it */
  reconcile_wipe(opt);
  reconcile_put(opt, "hts-in_progress.lock", 0);
  reconcile_put(opt, "hts-cache/new.zip", MID);
  reconcile_put(opt, "hts-cache/old.zip", SOLID);
  hts_cache_reconcile(opt, CACHE_RECONCILE_INTERRUPTED);
  failures +=
      reconcile_expect(opt, "hts-cache/new.zip", MID, "interrupted-bignew");

  /* INTERRUPTED: the removed pre-3.31 legacy pair is left alone */
  reconcile_wipe(opt);
  reconcile_put(opt, "hts-in_progress.lock", 0);
  reconcile_put(opt, "hts-cache/new.dat", TINY);
  reconcile_put(opt, "hts-cache/new.ndx", TINY);
  reconcile_put(opt, "hts-cache/old.dat", SOLID);
  reconcile_put(opt, "hts-cache/old.ndx", MID);
  hts_cache_reconcile(opt, CACHE_RECONCILE_INTERRUPTED);
  failures +=
      reconcile_expect(opt, "hts-cache/new.dat", TINY, "interrupted-dat");
  failures +=
      reconcile_expect(opt, "hts-cache/new.ndx", TINY, "interrupted-dat");
  failures +=
      reconcile_expect(opt, "hts-cache/old.dat", SOLID, "interrupted-dat");
  failures +=
      reconcile_expect(opt, "hts-cache/old.ndx", MID, "interrupted-dat");

  /* ROLLBACK: the old zip generation is restored (a zip cache used to lose
     its only good generation here) */
  reconcile_wipe(opt);
  reconcile_put(opt, "hts-cache/new.zip", TINY);
  reconcile_put(opt, "hts-cache/old.zip", SOLID);
  hts_cache_reconcile(opt, CACHE_RECONCILE_ROLLBACK);
  failures += reconcile_expect(opt, "hts-cache/new.zip", SOLID, "rollback-zip");
  failures += reconcile_expect(opt, "hts-cache/old.zip", -1, "rollback-zip");

  /* ROLLBACK: sidecars are restored regardless of format */
  reconcile_wipe(opt);
  reconcile_put(opt, "hts-cache/new.lst", TINY);
  reconcile_put(opt, "hts-cache/old.lst", MID);
  reconcile_put(opt, "hts-cache/old.txt", MID);
  hts_cache_reconcile(opt, CACHE_RECONCILE_ROLLBACK);
  failures += reconcile_expect(opt, "hts-cache/new.lst", MID, "rollback-lst");
  failures += reconcile_expect(opt, "hts-cache/new.txt", MID, "rollback-txt");

  /* ROLLBACK: sidecars restored, the removed pre-3.31 pair left alone */
  reconcile_wipe(opt);
  reconcile_put(opt, "hts-cache/new.dat", TINY);
  reconcile_put(opt, "hts-cache/old.dat", SOLID);
  reconcile_put(opt, "hts-cache/old.ndx", MID);
  reconcile_put(opt, "hts-cache/old.lst", MID);
  reconcile_put(opt, "hts-cache/old.txt", MID);
  hts_cache_reconcile(opt, CACHE_RECONCILE_ROLLBACK);
  failures += reconcile_expect(opt, "hts-cache/new.dat", TINY, "rollback-dat");
  failures += reconcile_expect(opt, "hts-cache/new.ndx", -1, "rollback-dat");
  failures += reconcile_expect(opt, "hts-cache/old.dat", SOLID, "rollback-dat");
  failures += reconcile_expect(opt, "hts-cache/old.ndx", MID, "rollback-dat");
  failures += reconcile_expect(opt, "hts-cache/new.lst", MID, "rollback-dat");
  failures += reconcile_expect(opt, "hts-cache/new.txt", MID, "rollback-dat");

  /* ROLLBACK: nothing to restore, the new generation stays */
  reconcile_wipe(opt);
  reconcile_put(opt, "hts-cache/new.zip", TINY);
  hts_cache_reconcile(opt, CACHE_RECONCILE_ROLLBACK);
  failures += reconcile_expect(opt, "hts-cache/new.zip", TINY, "rollback-noop");

  reconcile_wipe(opt);
  return failures;
}

/* The pre-3.31 .dat/.ndx import was removed: cache_init must refuse the
   legacy pair, leave the files in place, and not flag an update run. */
int cache_legacy_refused_selftest(httrackp *opt, const char *dir) {
  int failures = 0;
  int variant;

  selftest_tag = "cache-legacy";
  golden_setup(opt, dir);
#ifdef _WIN32
  mkdir(reconcile_st_path(opt, "hts-cache"));
#else
  mkdir(reconcile_st_path(opt, "hts-cache"), HTS_PROTECT_FOLDER);
#endif

  /* variant 0: old.dat/old.ndx (--update layout); 1: new.dat/new.ndx (ro) */
  for (variant = 0; variant < 2; variant++) {
    cache_back cache;
    const char *const dat =
        variant == 0 ? "hts-cache/old.dat" : "hts-cache/new.dat";
    const char *const ndx =
        variant == 0 ? "hts-cache/old.ndx" : "hts-cache/new.ndx";

    reconcile_wipe(opt);
    reconcile_put(opt, dat, 4096);
    reconcile_put(opt, ndx, 4096);
    opt->is_update = 0;
    selftest_open(&cache, opt, /*ro*/ variant == 1);
    if (cache_readable(&cache)) {
      printf("cache-legacy: variant %d imported a removed format\n", variant);
      failures++;
    }
    if (coucal_nitems(cache.hashtable) != 0) { /* no phantom index entries */
      printf("cache-legacy: variant %d imported index entries\n", variant);
      failures++;
    }
    if (opt->is_update) {
      printf("cache-legacy: variant %d flagged an update\n", variant);
      failures++;
    }
    if (fsize(reconcile_st_path(opt, dat)) != 4096 ||
        fsize(reconcile_st_path(opt, ndx)) != 4096) {
      printf("cache-legacy: variant %d touched the legacy files\n", variant);
      failures++;
    }
    if (cache.lst != NULL) {
      fclose(cache.lst);
      cache.lst = NULL;
    }
    if (cache.txt != NULL) {
      fclose(cache.txt);
      cache.txt = NULL;
    }
    selftest_close(&cache);
  }

  /* a healthy zip must win over stray legacy files, with no refusal */
  {
    cache_back cache;
    const char *const body = "<html>zip wins</html>";

    reconcile_wipe(opt);
    selftest_open_for_write(&cache, opt);
    store_entry(opt, &cache, "example.com", "/", "example.com/index.html", 200,
                "OK", "text/html", "utf-8", "", "", "", "", body, strlen(body));
    selftest_close(&cache);
    reconcile_put(opt, "hts-cache/old.ndx", 4096);
    reconcile_put(opt, "hts-cache/old.dat", 4096);
    selftest_open_for_read(&cache, opt);
    if (!cache_readable(&cache)) {
      printf("cache-legacy: stray legacy files shadowed the zip cache\n");
      failures++;
    }
    failures +=
        check_entry(opt, &cache, "example.com", "/", 200, "OK", "text/html",
                    "utf-8", "", "", "", "", body, strlen(body));
    if (fsize(reconcile_st_path(opt, "hts-cache/old.ndx")) != 4096 ||
        fsize(reconcile_st_path(opt, "hts-cache/old.dat")) != 4096) {
      printf("cache-legacy: zip-wins pass touched the legacy files\n");
      failures++;
    }
    if (cache.lst != NULL) {
      fclose(cache.lst);
      cache.lst = NULL;
    }
    if (cache.txt != NULL) {
      fclose(cache.txt);
      cache.txt = NULL;
    }
    selftest_close(&cache);
  }

  reconcile_wipe(opt);
  return failures;
}

/* --- read-side corruption injection --------------------------------------- */

/* canary read back intact after each corruption; victim gets the byte surgery
 */
#define CORRUPT_ADR "corrupt.example.com"
static char corrupt_body_a[33 + 1];
static char corrupt_body_b[44 + 1];

/* Write a fresh two-entry cache: /canary.html then /victim.html. */
static void corrupt_build(httrackp *opt) {
  cache_back cache;

  memset(corrupt_body_a, 'a', sizeof(corrupt_body_a) - 1);
  memset(corrupt_body_b, 'b', sizeof(corrupt_body_b) - 1);
  remove(reconcile_st_path(opt, "hts-cache/new.zip"));
  remove(reconcile_st_path(opt, "hts-cache/old.zip"));
  selftest_open_for_write(&cache, opt);
  store_entry(opt, &cache, CORRUPT_ADR, "/canary.html", "canary.html", 200,
              "OK", "text/html", "utf-8", "", "", "", "", corrupt_body_a,
              strlen(corrupt_body_a));
  store_entry(opt, &cache, CORRUPT_ADR, "/victim.html", "victim.html", 200,
              "OK", "text/html", "utf-8", "", "", "", "", corrupt_body_b,
              strlen(corrupt_body_b));
  selftest_close(&cache);
}

/* Like corrupt_build, but the victim carries a 20-char Etag whose header line
   is later overwritten with a forged oversized X-Size (same byte length). */
static void corrupt_build_etag(httrackp *opt) {
  cache_back cache;

  memset(corrupt_body_a, 'a', sizeof(corrupt_body_a) - 1);
  memset(corrupt_body_b, 'b', sizeof(corrupt_body_b) - 1);
  remove(reconcile_st_path(opt, "hts-cache/new.zip"));
  remove(reconcile_st_path(opt, "hts-cache/old.zip"));
  selftest_open_for_write(&cache, opt);
  store_entry(opt, &cache, CORRUPT_ADR, "/canary.html", "canary.html", 200,
              "OK", "text/html", "utf-8", "", "", "", "", corrupt_body_a,
              strlen(corrupt_body_a));
  store_entry(opt, &cache, CORRUPT_ADR, "/victim.html", "victim.html", 200,
              "OK", "text/html", "utf-8", "", "AAAAAAAAAAAAAAAAAAAA", "", "",
              corrupt_body_b, strlen(corrupt_body_b));
  selftest_close(&cache);
}

/* Like corrupt_build_etag, but the victim is headers-only (X-In-Cache: 0,
   body on disk): the shape every non-html file is stored with. */
static void corrupt_build_disk(httrackp *opt) {
  cache_back cache;
  htsblk w;
  char locw[4];
  char BIGSTK save[HTS_URLMAXSIZE * 2];
  char BIGSTK catbuff[HTS_URLMAXSIZE * 2];
  char *path;
  FILE *fp;

  memset(corrupt_body_a, 'a', sizeof(corrupt_body_a) - 1);
  remove(reconcile_st_path(opt, "hts-cache/new.zip"));
  remove(reconcile_st_path(opt, "hts-cache/old.zip"));
  /* X-Save stored verbatim: see disk_fallback_selftest */
  concat(save, sizeof(save), StringBuff(opt->path_html_utf8),
         CORRUPT_ADR "/victim.bin");
  selftest_open_for_write(&cache, opt);
  store_entry(opt, &cache, CORRUPT_ADR, "/canary.html", "canary.html", 200,
              "OK", "text/html", "utf-8", "", "", "", "", corrupt_body_a,
              strlen(corrupt_body_a));
  hts_init_htsblk(&w);
  w.statuscode = 200;
  w.size = (LLint) sizeof(corrupt_body_b) - 1;
  strcpybuff(w.msg, "OK");
  strcpybuff(w.contenttype, "application/octet-stream");
  strcpybuff(w.etag, "AAAAAAAAAAAAAAAAAAAA");
  locw[0] = '\0';
  w.location = locw;
  w.is_write = 0;
  cache_add(opt, &cache, &w, CORRUPT_ADR, "/victim.bin", save,
            0 /* all_in_cache */, NULL);
  selftest_close(&cache);
  /* the reader only checks this file exists; it never reads it here */
  path = fconv(catbuff, sizeof(catbuff), save);
  (void) structcheck(path);
  fp = FOPEN(path, "wb");
  assertf(fp != NULL);
  fclose(fp);
}

/* Patch the nth of total occurrences of pat (same-length rep) in new.zip. */
static void corrupt_patch(httrackp *opt, const char *pat, size_t patlen,
                          const char *rep, size_t nth, size_t total) {
  LLint fsz = 0;
  char *data = readfile2(reconcile_st_path(opt, "hts-cache/new.zip"), &fsz);
  const size_t n = (size_t) fsz;
  size_t k, hits = 0, at = 0;
  FILE *fp;

  assertf(data != NULL);
  for (k = 0; k + patlen <= n; k++) {
    if (memcmp(data + k, pat, patlen) == 0) {
      hits++;
      if (hits == nth)
        at = k;
    }
  }
  assertf(hits == total);
  memcpy(data + at, rep, patlen);
  fp = fopen(reconcile_st_path(opt, "hts-cache/new.zip"), "wb");
  assertf(fp != NULL);
  assertf(fwrite(data, 1, n, fp) == n);
  fclose(fp);
  freet(data);
}

/* Garbage the first bytes of the victim's deflated data (2nd local header). */
static void corrupt_victim_body(httrackp *opt) {
  LLint fsz = 0;
  char *data = readfile2(reconcile_st_path(opt, "hts-cache/new.zip"), &fsz);
  const size_t n = (size_t) fsz;
  size_t k, hits = 0, off = 0;
  FILE *fp;

  assertf(data != NULL);
  for (k = 0; k + 4 <= n; k++) {
    if (memcmp(data + k, "PK\x03\x04", 4) == 0 && ++hits == 2) {
      const size_t namelen =
          (unsigned char) data[k + 26] | ((unsigned char) data[k + 27] << 8);
      const size_t extralen =
          (unsigned char) data[k + 28] | ((unsigned char) data[k + 29] << 8);

      off = k + 30 + namelen + extralen;
    }
  }
  assertf(hits == 2);
  assertf(off != 0 && off + 4 <= n);
  memset(data + off, 0xFF, 4);
  fp = fopen(reconcile_st_path(opt, "hts-cache/new.zip"), "wb");
  assertf(fp != NULL);
  assertf(fwrite(data, 1, n, fp) == n);
  fclose(fp);
  freet(data);
}

/* Read the corrupt /victim.html and, in the SAME read session, the intact
   /canary.html: the victim must be rejected (wantmsg pins which path) and the
   canary must still decode byte-exact, proving one bad entry never taints a
   sibling read. */
static int corrupt_expect_victim_fil(httrackp *opt, const char *fil,
                                     const char *wantmsg, const char *what) {
  cache_back cache;
  htsblk v, c;
  char BIGSTK lv[HTS_URLMAXSIZE * 2];
  char BIGSTK lc[HTS_URLMAXSIZE * 2];
  int fail = 0;

  selftest_open_for_read(&cache, opt);
  lv[0] = lc[0] = '\0';
  v = cache_readex(opt, &cache, CORRUPT_ADR, fil, "", lv, NULL, 1);
  if (v.statuscode != STATUSCODE_INVALID) {
    fprintf(stderr, "%s: %s: victim: statuscode is %d, expected %d\n",
            selftest_tag, what, v.statuscode, STATUSCODE_INVALID);
    fail++;
  }
  if (wantmsg != NULL && strcmp(v.msg, wantmsg) != 0) {
    fprintf(stderr, "%s: %s: victim: msg is '%s', expected '%s'\n",
            selftest_tag, what, v.msg, wantmsg);
    fail++;
  }
  c = cache_readex(opt, &cache, CORRUPT_ADR, "/canary.html", "", lc, NULL, 1);
  if (c.statuscode != 200 || c.adr == NULL ||
      c.size != (LLint) strlen(corrupt_body_a) ||
      memcmp(c.adr, corrupt_body_a, strlen(corrupt_body_a)) != 0) {
    fprintf(stderr, "%s: %s: canary tainted (status %d)\n", selftest_tag, what,
            c.statuscode);
    fail++;
  }
  if (v.adr != NULL)
    freet(v.adr);
  if (c.adr != NULL)
    freet(c.adr);
  selftest_close(&cache);
  return fail;
}

static int corrupt_expect_victim(httrackp *opt, const char *wantmsg,
                                 const char *what) {
  return corrupt_expect_victim_fil(opt, "/victim.html", wantmsg, what);
}

/* Headers-only probe of the disk victim: must parse OK with the size kept. */
static int corrupt_expect_disk_header(httrackp *opt, LLint wantsize,
                                      const char *what) {
  cache_back cache;
  htsblk v;
  char BIGSTK lv[HTS_URLMAXSIZE * 2];
  int fail = 0;

  selftest_open_for_read(&cache, opt);
  lv[0] = '\0';
  v = cache_readex(opt, &cache, CORRUPT_ADR, "/victim.bin", NULL, lv, NULL, 1);
  if (v.statuscode != 200 || v.size != wantsize) {
    fprintf(stderr,
            "%s: %s: statuscode %d size " LLintP ", expected 200/" LLintP "\n",
            selftest_tag, what, v.statuscode, (LLint) v.size, wantsize);
    fail++;
  }
  if (v.adr != NULL)
    freet(v.adr);
  selftest_close(&cache);
  return fail;
}

/* One zip corruption case: build, patch, then check victim+canary in-session.
 */
static int corrupt_case_zip(httrackp *opt, const char *pat, const char *rep,
                            size_t nth, size_t total, const char *wantmsg,
                            const char *what) {
  corrupt_build(opt);
  corrupt_patch(opt, pat, strlen(pat), rep, nth, total);
  return corrupt_expect_victim(opt, wantmsg, what);
}

int cache_corruption_selftest(httrackp *opt, const char *dir) {
  int failures = 0;

  selftest_tag = "cache-corrupt";
  golden_setup(opt, dir);

  failures +=
      corrupt_case_zip(opt, "X-Size: 44", "X-Size: 99", 1, 1,
                       "Cache Read Error : Read Data", "oversized X-Size");
  failures +=
      corrupt_case_zip(opt, "X-Size: 44", "X-Size: -4", 1, 1,
                       "Cache Read Error : Bad Size", "negative X-Size");
  /* both entries carry the line; the victim's is the second */
  failures += corrupt_case_zip(opt, "X-In-Cache: 1", "X-In-Cache: 0", 2, 2,
                               "Previous cache file not found (empty filename)",
                               "blanked X-In-Cache");
  /* smashed local file header: the entry is dropped at index load */
  failures +=
      corrupt_case_zip(opt, "PK\x03\x04", "XK\x03\x04", 2, 2,
                       "File Cache Entry Not Found", "smashed local header");

  corrupt_build(opt);
  corrupt_victim_body(opt);
  failures += corrupt_expect_victim(opt, "Cache Read Error : Read Data",
                                    "garbled deflate stream");

  /* An X-Size above INT_MAX is positive as int64 (slips a bare sign check) but
     truncates negative in the (int) cast the malloc uses: a wraparound alloc.
     cache_add asserts size fits an int, so such a value only reaches the reader
     from a corrupt/foreign cache; inject it by overwriting the victim's long
     Etag line with a same-length forged X-Size line (the parser keeps the last
     X-Size it sees), keeping the zip byte-length and offsets intact. */
  corrupt_build_etag(opt);
  corrupt_patch(opt, "Etag: AAAAAAAAAAAAAAAAAAAA", 26,
                "X-Size: 2147483648AAAAAAAA", 1, 1);
  failures += corrupt_expect_victim(opt, "Cache Read Error : Bad Size",
                                    "X-Size above INT_MAX");

  /* A headers-only entry (X-In-Cache: 0) may carry an X-Size >= INT_MAX: that
     is how every >2GB non-html file is stored. It must survive a header probe
     (or every update re-fetches the file); an in-memory read still rejects. */
  corrupt_build_disk(opt);
  corrupt_patch(opt, "Etag: AAAAAAAAAAAAAAAAAAAA", 26,
                "X-Size: 2147483648AAAAAAAA", 1, 1);
  failures += corrupt_expect_disk_header(opt, (LLint) 2147483648LL,
                                         "headers-only X-Size above INT_MAX");
  failures += corrupt_expect_victim_fil(opt, "/victim.bin",
                                        "Cache Read Error : Bad Size",
                                        "in-memory X-Size above INT_MAX");

  /* exactly INT_MAX pins the >= boundary: (int) r.size + 1 would overflow */
  corrupt_build_disk(opt);
  corrupt_patch(opt, "Etag: AAAAAAAAAAAAAAAAAAAA", 26,
                "X-Size: 2147483647AAAAAAAA", 1, 1);
  failures += corrupt_expect_victim_fil(opt, "/victim.bin",
                                        "Cache Read Error : Bad Size",
                                        "in-memory X-Size at INT_MAX");

  /* the negative check must stay global, headers-only included */
  corrupt_build_disk(opt);
  corrupt_patch(opt, "Etag: AAAAAAAAAAAAAAAAAAAA", 26,
                "X-Size: -2147483648AAAAAAA", 1, 1);
  failures += corrupt_expect_victim_fil(opt, "/victim.bin",
                                        "Cache Read Error : Bad Size",
                                        "headers-only negative X-Size");

  return failures;
}
