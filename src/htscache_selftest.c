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
#include "htsio.h"
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
  char locbuf[HTS_LOCATION_SIZE];
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
  char *locbuf = malloct(HTS_LOCATION_SIZE);
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
  if (!hts_fwrite_exact(body, body_len, fp)) {
    fprintf(stderr, "cache-selftest: disk-fallback: short write to '%s'\n",
            path);
    fail++;
  }
  fclose(fp);

  /* read it back: takes the X-In-Cache: 0 disk-fallback branch */
  selftest_open_for_read(&cache, opt);
  locbuf = malloct(HTS_LOCATION_SIZE);
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

/* A cache miss sends cache_read_including_broken() to the serialized reference
   of an interrupted transfer, whose entry it then frees: what it returns must
   not point into that entry (#826). */
static int broken_ref_selftest(httrackp *opt) {
  int fail = 0;
  cache_back cache;
  lien_back *entry;
  htsblk r;
  char save[HTS_URLMAXSIZE * 2];
  const char *const adr = "example.com";
  const char *const fil = "/interrupted.html";
  static const char body[] = "<html><body>half a p";
  static const char headers[] =
      "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";

  entry = calloct(1, sizeof(lien_back));
  strcpybuff(entry->url_adr, adr);
  strcpybuff(entry->url_fil, fil);
  concat(entry->url_sav, sizeof(entry->url_sav),
         StringBuff(opt->path_html_utf8), "example.com/interrupted.html");
  hts_init_htsblk(&entry->r);
  entry->r.statuscode = 200;
  entry->r.size = (LLint) (sizeof(body) - 1);
  strcpybuff(entry->r.msg, "OK");
  strcpybuff(entry->r.contenttype, "text/html");
  entry->r.location = entry->location_buffer;
  entry->r.adr = strdupt(body);
  entry->r.headers = strdupt(headers);
  if (back_serialize_ref(opt, entry) != 0) {
    fprintf(stderr, "%s: broken-ref: cannot write the reference\n",
            selftest_tag);
    fail++;
  }
  freet(entry->r.adr);
  freet(entry->r.headers);
  freet(entry);

  selftest_open_for_read(&cache, opt);
  save[0] = '\0';
  r = cache_read_including_broken(opt, &cache, adr, fil, save, NULL);
  selftest_close(&cache);

  if (r.statuscode != 200 || strcmp(r.contenttype, "text/html") != 0) {
    fprintf(stderr,
            "%s: broken-ref: statuscode %d type '%s', want 200/text/html"
            " (reference not read back)\n",
            selftest_tag, r.statuscode, r.contenttype);
    fail++;
  }
  if (strstr(save, "interrupted.html") == NULL) {
    fprintf(stderr, "%s: broken-ref: save name '%s' lost\n", selftest_tag,
            save);
    fail++;
  }
  /* the entry owned all three and is gone, so a non-NULL one is dangling */
  if (r.adr != NULL || r.headers != NULL || r.location != NULL) {
    fprintf(stderr,
            "%s: broken-ref: returned freed adr/headers/location"
            " (%p/%p/%p)\n",
            selftest_tag, (void *) r.adr, (void *) r.headers,
            (void *) r.location);
    fail++;
  }
  return fail;
}

typedef struct {
  size_t budget;  /**< bytes allowed through before writes start failing */
  int fail_errno; /**< errno set on the failing write (ENOSPC, EIO, ...) */
  int writes;     /**< zwrite call count, to detect re-entry into the stream */
  int fail_once;  /**< recover (unlimited budget) after the first failure */
  int truncates;  /**< ztruncate call count, to prove the rollback truncates */
  truncate64_file_func truncate; /**< the backend's own, NULL if it has none */
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

/* Count the truncate the rolled-back entry goes through, then run it. */
static int ZCALLBACK selftest_counting_ztruncate(voidpf opaque, voidpf stream,
                                                 ZPOS64_T size) {
  writefail_inject *inj = (writefail_inject *) opaque;

  inj->truncates++;
  if (inj->truncate == NULL)
    return -1;
  return inj->truncate(opaque, stream, size);
}

/* Open a ZIP whose writes fail past inj->budget, so cache_add() hits an error.
   Through the table the cache itself opens with (#1402), or with its truncate
   entry dropped, as the Win32 tables were: a rollback then only rewinds. */
static zipFile selftest_open_failing_zip(const char *path,
                                         writefail_inject *inj,
                                         hts_boolean truncatable) {
  zlib_filefunc64_def ff;

  hts_zip_filefunc64(&ff); /* real fopen/read/seek/close; ignores opaque */
  inj->truncate = truncatable ? ff.ztruncate64_file : NULL;
  ff.ztruncate64_file = truncatable ? selftest_counting_ztruncate : NULL;
  ff.zwrite_file = selftest_failing_zwrite;
  ff.opaque = inj;
  return zipOpen2_64(path, APPEND_STATUS_CREATE, NULL, &ff);
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
    inj.truncates = 0;
    inj.fail_once = 0;
    memset(&cache, 0, sizeof(cache));
    cache.type = 1;
    cache.log = stderr;
    cache.errlog = stderr;
    cache.hashtable = coucal_new(0);
    cache.zipOutput = selftest_open_failing_zip(path, &inj, HTS_TRUE);
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
    inj.truncates = 0;
    inj.fail_once = 0;
    memset(&cache, 0, sizeof(cache));
    cache.type = 1;
    cache.log = stderr;
    cache.errlog = stderr;
    cache.hashtable = coucal_new(0);
    cache.zipOutput = selftest_open_failing_zip(path, &inj, HTS_TRUE);
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
    inj.truncates = 0;
    inj.fail_once = 1;
    memset(&cache, 0, sizeof(cache));
    cache.type = 1;
    cache.log = stderr;
    cache.errlog = stderr;
    cache.hashtable = coucal_new(0);
    cache.zipOutput = selftest_open_failing_zip(path, &inj, HTS_TRUE);
    opt->state.exit_xh = 0;

    writefail_store(opt, &cache, "/blob.bin", body, body_len);
    if (cache.zipWriteFailed || opt->state.exit_xh != 0) {
      fprintf(stderr,
              "cache-writefail: skip: isolated failure aborted the mirror "
              "(flagged=%d, exit_xh=%d)\n",
              (int) cache.zipWriteFailed, opt->state.exit_xh);
      fail++;
    }
    /* the rolled-back entry is truncated away, not merely rewound: #1402,
       where a table with no truncate still reported the rollback as done */
    if (inj.truncate == NULL) {
      fprintf(stderr, "cache-writefail: skip: the table the cache opens with "
                      "carries no truncate, so the rollback only rewound\n");
      fail++;
    } else if (inj.truncates != 1) {
      fprintf(stderr,
              "cache-writefail: skip: abandoned entry truncated %d time(s), "
              "want 1\n",
              inj.truncates);
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

  /* a backend with no truncate rolls back by rewinding: the entry still drops
     and the mirror still lives, and the incomplete rollback is warned about */
  {
    cache_back cache;
    writefail_inject inj;
    char extra[8192];
    char rbody[64];
    int n;

    inj.budget = 4096;
    inj.fail_errno = EIO;
    inj.writes = 0;
    inj.truncates = 0;
    inj.fail_once = 1;
    memset(&cache, 0, sizeof(cache));
    cache.type = 1;
    cache.log = stderr;
    cache.errlog = stderr;
    cache.hashtable = coucal_new(0);
    cache.zipOutput = selftest_open_failing_zip(path, &inj, HTS_FALSE);
    opt->state.exit_xh = 0;

    writefail_store(opt, &cache, "/blob.bin", body, body_len);
    if (cache.zipWriteFailed || opt->state.exit_xh != 0) {
      fprintf(stderr,
              "cache-writefail: notrunc: a rewind-only rollback aborted the "
              "mirror (flagged=%d, exit_xh=%d)\n",
              (int) cache.zipWriteFailed, opt->state.exit_xh);
      fail++;
    }
    if (inj.truncates != 0) {
      fprintf(stderr,
              "cache-writefail: notrunc: %d truncate call(s) on a table that "
              "has none\n",
              inj.truncates);
      fail++;
    }
    writefail_store(opt, &cache, "/blob2.bin", body, 16);
    zipClose(cache.zipOutput, NULL);
    cache.zipOutput = NULL;
    n = writefail_read_entry(path, "http://example.com/blob2.bin", extra,
                             sizeof(extra), rbody, sizeof(rbody));
    if (n != 16 || memcmp(rbody, body, 16) != 0) {
      fprintf(stderr,
              "cache-writefail: notrunc: sibling entry lost after a rewind "
              "(%d)\n",
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
    inj.truncates = 0;
    inj.fail_once = 0;
    memset(&cache, 0, sizeof(cache));
    cache.type = 1;
    cache.log = stderr;
    cache.errlog = stderr;
    cache.hashtable = coucal_new(0);
    cache.zipOutput = selftest_open_failing_zip(path, &inj, HTS_TRUE);
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

/* cache_mayadd()'s shortcut for a 3xx with no save name records "<code>\n<url>"
   in cached_tests so the same URL is not re-tested. Its scratch must hold a
   location at the full contract; too small and the mirror aborts on a long
   redirect rather than storing it. */
static int fast_header_selftest(httrackp *opt, const char *location) {
  const char *const adr = "example.com";
  const char *const fil = "/moved-fast";
  cache_back cache;
  htsblk r;
  char *locbuf = malloct(HTS_LOCATION_SIZE);
  char *want = malloct(HTS_LOCATION_SIZE + 32);
  intptr_t stored = 0;
  int fail = 0;

  selftest_open_for_write(&cache, opt);
  cache.cached_tests = coucal_new(0);
  assertf(cache.cached_tests != NULL);
  coucal_value_is_malloc(cache.cached_tests, 1);

  hts_init_htsblk(&r);
  r.statuscode = 301;
  r.size = 0;
  strcpybuff(r.msg, "Moved Permanently");
  strcpybuff(r.contenttype, "text/html");
  strlcpybuff(locbuf, location, HTS_LOCATION_SIZE);
  r.location = locbuf;
  r.is_write = 0;
  cache_mayadd(opt, &cache, &r, adr, fil, NULL);

  snprintf(want, HTS_LOCATION_SIZE + 32, "%d\n%s", (int) r.statuscode,
           location);
  if (!coucal_read(cache.cached_tests,
                   concat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), adr, fil),
                   &stored)) {
    fprintf(stderr, "%s: fast-header: nothing recorded for %s%s\n",
            selftest_tag, adr, fil);
    fail++;
  } else if (strcmp((const char *) stored, want) != 0) {
    fprintf(stderr, "%s: fast-header: recorded %d bytes, expected %d\n",
            selftest_tag, (int) strlen((const char *) stored),
            (int) strlen(want));
    fail++;
  }

  selftest_close(&cache);
  freet(locbuf);
  freet(want);
  return fail;
}

int cache_selftests(httrackp *opt, const char *dir) {
  int failures = 0;
  cache_back cache;
  int i;

  /* near-limit field values: the etag stresses htsblk.etag[256], the location
     a redirect URL at the full HTS_LOCATION_SIZE contract */
  static char etag_long[251];
  static char location_long[HTS_LOCATION_SIZE];

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
    if (base[0] != '\0' && hts_lastchar(base) != '/') {
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

  /* pass 6: the broken-transfer reference fallback */
  failures += broken_ref_selftest(opt);

  /* pass 7: the save-less 3xx shortcut, whose scratch holds code + location */
  failures += fast_header_selftest(opt, location_long);

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

static void selftest_setup_dir(httrackp *opt, const char *dir) {
  char base[HTS_URLMAXSIZE];

  strcpybuff(base, dir);
  if (base[0] != '\0' && hts_lastchar(base) != '/') {
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
  selftest_setup_dir(opt, dir);

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
/* LLint, not size_t: the callers hold file sizes, which they also hand to
   reconcile_expect() and to fsize(). */
static void reconcile_put(httrackp *opt, const char *name, LLint size) {
  FILE *const fp = fopen(reconcile_st_path(opt, name), "wb");
  static const char filler[1024] = {'x'};

  assertf(fp != NULL);
  while (size > 0) {
    /* Narrowing is safe: the ternary bounds n by sizeof(filler) first. */
    const size_t n =
        size > (LLint) sizeof(filler) ? sizeof(filler) : (size_t) size;

    assertf(hts_fwrite_exact(filler, n, fp));
    size -= n;
  }
  fclose(fp);
}

/* Expect `name` to weigh `size` bytes, or be absent when size == -1. */
static int reconcile_expect(httrackp *opt, const char *name, LLint size,
                            const char *what) {
  const LLint got = fsize(reconcile_st_path(opt, name));

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
  static const LLint TINY = 1024, MID = 40000, SOLID = 131072;

  selftest_setup_dir(opt, dir);
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
  selftest_setup_dir(opt, dir);
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

/* 100 'A's: a placeholder header line long enough to be overwritten by a
   forged, over-long X-StatusMessage of the same byte length. */
#define CORRUPT_LONG_ETAG                                                      \
  "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"                         \
  "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"

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

/* Like corrupt_build, but the victim carries a 100-char Etag placeholder. */
static void corrupt_build_longetag(httrackp *opt) {
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
              "OK", "text/html", "utf-8", "", CORRUPT_LONG_ETAG, "", "",
              corrupt_body_b, strlen(corrupt_body_b));
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
  assertf(hts_fwrite_exact(data, n, fp));
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
  assertf(hts_fwrite_exact(data, n, fp));
  fclose(fp);
  freet(data);
}

/* Flip the victim's stored CRC in both headers, which must agree or the open
   fails before anything inflates. */
static void corrupt_victim_crc(httrackp *opt) {
  LLint fsz = 0;
  char *data = readfile2(reconcile_st_path(opt, "hts-cache/new.zip"), &fsz);
  const size_t n = (size_t) fsz;
  size_t k, local = 0, central = 0, hits = 0;
  FILE *fp;

  assertf(data != NULL);
  for (k = 0; k + 4 <= n; k++) {
    if (memcmp(data + k, "PK\x03\x04", 4) == 0 && ++hits == 2)
      local = k + 14;
  }
  assertf(hits == 2);
  hits = 0;
  for (k = 0; k + 4 <= n; k++) {
    if (memcmp(data + k, "PK\x01\x02", 4) == 0 && ++hits == 2)
      central = k + 16;
  }
  assertf(hits == 2);
  assertf(local != 0 && local + 4 <= n && central != 0 && central + 4 <= n);
  /* Not zeroed: an empty member's real CRC is 0, so 0 could match by luck. */
  memset(data + local, 0x5A, 4);
  memset(data + central, 0x5A, 4);
  fp = fopen(reconcile_st_path(opt, "hts-cache/new.zip"), "wb");
  assertf(fp != NULL);
  assertf(hts_fwrite_exact(data, n, fp));
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

/* An over-long field from a foreign cache must clip, not abort: the entry
   still reads, clipped to capacity, and the canary survives. */
static int corrupt_expect_victim_clipped(httrackp *opt, size_t wantmsg,
                                         size_t wantlastmod, const char *what) {
  cache_back cache;
  htsblk v, c;
  char BIGSTK lv[HTS_URLMAXSIZE * 2];
  char BIGSTK lc[HTS_URLMAXSIZE * 2];
  int fail = 0;

  selftest_open_for_read(&cache, opt);
  lv[0] = lc[0] = '\0';
  v = cache_readex(opt, &cache, CORRUPT_ADR, "/victim.html", "", lv, NULL, 1);
  if (v.statuscode != 200) {
    fprintf(stderr, "%s: %s: status %d, expected 200\n", selftest_tag, what,
            v.statuscode);
    fail++;
  }
  if (wantmsg != (size_t) -1 && strlen(v.msg) != wantmsg) {
    fprintf(stderr, "%s: %s: msg len %u, expected %u\n", selftest_tag, what,
            (unsigned) strlen(v.msg), (unsigned) wantmsg);
    fail++;
  }
  if (wantlastmod != (size_t) -1 && strlen(v.lastmodified) != wantlastmod) {
    fprintf(stderr, "%s: %s: lastmodified len %u, expected %u\n", selftest_tag,
            what, (unsigned) strlen(v.lastmodified), (unsigned) wantlastmod);
    fail++;
  }
  c = cache_readex(opt, &cache, CORRUPT_ADR, "/canary.html", "", lc, NULL, 1);
  if (c.statuscode != 200) {
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
  selftest_setup_dir(opt, dir);

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

  corrupt_build(opt);
  corrupt_victim_crc(opt);
  failures += corrupt_expect_victim(opt, "Cache Read Error : CRC",
                                    "body inflates, CRC does not match");

  /* A corrupt cache can hold a field wider than ours. Clipping keeps the
     entry; aborting would take the crawl down. Overwrite the placeholder Etag
     line in place, same byte length, so the zip offsets stay intact. */
  corrupt_build_longetag(opt);
  corrupt_patch(opt, "Etag: " CORRUPT_LONG_ETAG, 106,
                "X-StatusMessage: " /* 17 + 89 = 106 */
                "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
                1, 1);
  failures +=
      corrupt_expect_victim_clipped(opt, sizeof(((htsblk *) 0)->msg) - 1,
                                    (size_t) -1, "over-long X-StatusMessage");

  /* lastmodified[64] is narrower than msg[80]: one hardcoded clip length
     cannot satisfy both. */
  corrupt_build_longetag(opt);
  corrupt_patch(opt, "Etag: " CORRUPT_LONG_ETAG, 106,
                "Last-Modified: " /* 15 + 91 = 106 */
                "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
                1, 1);
  failures += corrupt_expect_victim_clipped(
      opt, (size_t) -1, sizeof(((htsblk *) 0)->lastmodified) - 1,
      "over-long Last-Modified");

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

/* Header block of the ZIP's first entry, from its local extra field. By
   position, not name: unzLocateFile compares through a 256-byte buffer and
   cannot find the multi-KB entry name this test writes. */
static int read_first_entry_extra(const char *path, char *extra,
                                  size_t extralen) {
  unzFile z = unzOpen(path);
  int elen = -1;

  if (z == NULL)
    return -1;
  if (unzGoToFirstFile(z) == UNZ_OK && unzOpenCurrentFile(z) == UNZ_OK) {
    elen = unzGetLocalExtrafield(z, extra, (unsigned) (extralen - 1));
    if (elen >= 0) {
      extra[elen] = '\0';
    }
    unzCloseCurrentFile(z);
  }
  unzClose(z);
  return elen;
}

static void fill_str(char *s, size_t size, size_t len, char c) {
  assertf(len < size);
  memset(s, c, len);
  s[len] = '\0';
}

/* A value read back must be a prefix of what was written, and no shorter than
   `least`: the reader clips a line to its parse buffer, but never splices,
   reorders, or drops more than that. */
static int check_prefix(const char *what, const char *got, const char *want,
                        size_t least) {
  const size_t len = strlen(got);

  if (len < least || strncmp(got, want, len) != 0) {
    fprintf(stderr,
            "cache-hdrbounds: %s reads back '%.32s...' (%d bytes), expected a "
            "prefix of the %d written, at least %d long\n",
            what, got, (int) len, (int) strlen(want), (int) least);
    return 1;
  }
  return 0;
}

int cache_header_bounds_selftest(httrackp *opt, const char *dir) {
  int failures = 0;
  cache_back cache;
  /* every htsblk field at the cap its declaration allows */
  static char msg[80], ctype[HTS_MIMETYPE_SIZE], charset[HTS_MIMETYPE_SIZE];
  static char etag[256], cdispo[256], location[HTS_LOCATION_SIZE];
  /* url_adr/url_fil/save are all lien_back-sized, so the three alone reach
     6 KB, past the header block the writer builds them in */
  static char big_adr[HTS_URLMAXSIZE * 2], big_fil[HTS_URLMAXSIZE * 2];
  static char big_save[HTS_URLMAXSIZE * 2];
  /* the control: same maxed fields, but a URL short enough to leave the header
     block room, so nothing is dropped and the round-trip must be exact */
  static char ok_fil[HTS_URLMAXSIZE];
  const char *const ok_adr = "example.com";
  const char *const ok_save = "example.com/big.html";
  const char *const body = "<html><body>maxed</body></html>";

  fill_str(msg, sizeof(msg), sizeof(msg) - 1, 'M');
  fill_str(ctype, sizeof(ctype), sizeof(ctype) - 1, 'C');
  fill_str(charset, sizeof(charset), sizeof(charset) - 1, 'H');
  fill_str(etag, sizeof(etag), sizeof(etag) - 1, 'E');
  fill_str(cdispo, sizeof(cdispo), sizeof(cdispo) - 1, 'D');
  fill_str(location, sizeof(location), sizeof(location) - 1, 'L');
  fill_str(big_adr, sizeof(big_adr), sizeof(big_adr) - 1, 'a');
  big_fil[0] = '/';
  fill_str(big_fil + 1, sizeof(big_fil) - 1, 2046, 'f');
  fill_str(big_save, sizeof(big_save), sizeof(big_save) - 1, 'S');
  ok_fil[0] = '/';
  fill_str(ok_fil + 1, sizeof(ok_fil) - 1, sizeof(ok_fil) - 2, 'k');

  {
    char base[HTS_URLMAXSIZE];

    strcpybuff(base, dir);
    if (base[0] != '\0' && hts_lastchar(base) != '/') {
      strcatbuff(base, "/");
    }
    StringCopy(opt->path_log, base);
    StringCopy(opt->path_html, base);
    StringCopy(opt->path_html_utf8, base);
  }
  opt->cache = HTS_CACHE_PRIORITY;

  selftest_open_for_write(&cache, opt);
  store_entry(opt, &cache, big_adr, big_fil, big_save, 200, msg, ctype, charset,
              "Mon, 01 Jan 2024 00:00:00 GMT", etag, location, cdispo, body,
              strlen(body));
  store_entry(opt, &cache, ok_adr, ok_fil, ok_save, 200, msg, ctype, charset,
              "Mon, 01 Jan 2024 00:00:00 GMT", etag, location, cdispo, body,
              strlen(body));
  selftest_close(&cache);

  /* Read the header straight from the ZIP, not through cache_readex: a runtime
     that does not trap the overrun would store the block and still pass. */
  {
    char zippath[HTS_URLMAXSIZE];
    static char extra[CACHE_HEADERS_SIZE * 4];
    size_t elen;

    slprintfbuff_clip(zippath, sizeof(zippath), "%shts-cache/new.zip",
                      StringBuff(opt->path_log));
    if (read_first_entry_extra(zippath, extra, sizeof(extra)) < 0) {
      fprintf(stderr, "cache-hdrbounds: maxed entry missing from the ZIP\n");
      failures++;
    } else if (strstr(extra, cdispo) == NULL) {
      /* an early-out writer would leave the control entry first, and every
         check below would then pass on the wrong block */
      fprintf(stderr,
              "cache-hdrbounds: first ZIP entry is not the maxed one\n");
      failures++;
    } else {
      elen = strlen(extra);
      if (elen >= CACHE_HEADERS_SIZE) {
        fprintf(stderr,
                "cache-hdrbounds: header block is %d bytes, past the writer's "
                "%d-byte buffer\n",
                (int) elen, (int) CACHE_HEADERS_SIZE);
        failures++;
      }
      if (elen < 2 || strcmp(extra + elen - 2, "\r\n") != 0) {
        fprintf(stderr, "cache-hdrbounds: header block ends mid-line\n");
        failures++;
      }
      /* X-Fil is written last, so its absence proves the block filled; the URL
         is already at its lien_back cap, so a surviving X-Fil means the case
         needs rebuilding, not silently weakening. */
      if (strstr(extra, "X-Fil: ") != NULL) {
        fprintf(stderr,
                "cache-hdrbounds: the maxed entry no longer fills the %d-byte "
                "block (%d bytes); this case has stopped testing the bound\n",
                (int) CACHE_HEADERS_SIZE, (int) elen);
        failures++;
      }
      /* X-Save is the only one of the three a reader acts on, so a full block
         must not be what drops it */
      if (strstr(extra, "X-Save: ") == NULL) {
        fprintf(stderr, "cache-hdrbounds: X-Save was dropped from a full "
                        "block; it must be written before X-Addr/X-Fil\n");
        failures++;
      }
    }
  }

  selftest_open_for_read(&cache, opt);
  {
    char *locbuf = malloct(HTS_LOCATION_SIZE);
    htsblk r;

    locbuf[0] = '\0';
    r = cache_readex(opt, &cache, ok_adr, ok_fil, "", locbuf, NULL, 1);
    if (r.statuscode != 200) {
      fprintf(stderr,
              "cache-hdrbounds: entry after the overflowing one reads "
              "statuscode %d, expected 200\n",
              r.statuscode);
      failures++;
    }
    /* a header block that fits loses nothing */
    if (strcmp(r.msg, msg) != 0 || strcmp(r.contenttype, ctype) != 0 ||
        strcmp(r.charset, charset) != 0 || strcmp(r.etag, etag) != 0 ||
        strcmp(r.cdispo, cdispo) != 0) {
      fprintf(stderr, "cache-hdrbounds: a maxed field did not round-trip\n");
      failures++;
    }
    /* the reader clips this to its line buffer (1014 of the 2047 bytes at the
       current HTS_URLMAXSIZE); the floor only has to rule out a real loss */
    failures += check_prefix("location", locbuf, location, 1000);
    if (r.adr == NULL || r.size != (LLint) strlen(body) ||
        memcmp(r.adr, body, strlen(body)) != 0) {
      fprintf(stderr, "cache-hdrbounds: body did not round-trip\n");
      failures++;
    }
    if (r.adr != NULL) {
      freet(r.adr);
    }
    freet(locbuf);
  }
  selftest_close(&cache);

  return failures;
}

/* A URL the cache cannot hold must read as a miss, never abort. */
static int expect_miss(httrackp *opt, cache_back *cache, const char *adr,
                       const char *fil) {
  htsblk r = cache_readex(opt, cache, adr, fil, "", NULL, NULL, 1);
  int failures = 0;

  if (r.statuscode != STATUSCODE_INVALID) {
    fprintf(stderr,
            "%s: an unstorable URL reads statuscode %d, expected a "
            "miss\n",
            selftest_tag, r.statuscode);
    failures++;
  }
  if (r.adr != NULL) {
    freet(r.adr);
  }
  return failures;
}

/* A cache ZIP whose first entry name is longer than the writer can emit,
   followed by a sane one; minizip hands such a name back unterminated. */
static int urlbounds_write_foreign_zip(const char *zippath, size_t namelen) {
  static const char hdr[] = "HTTP/1.1 200 OK\r\nX-In-Cache: 1\r\n"
                            "X-StatusCode: 200\r\nX-Size: 4\r\n"
                            "X-StatusMessage: OK\r\n"
                            "Content-Type: text/html\r\nX-Charset: utf-8\r\n";
  static char longname[CACHE_ENTRYNAME_SIZE * 2];
  const char *const body = "okay";
  zip_fileinfo fi;
  zipFile z = zipOpen(zippath, APPEND_STATUS_CREATE);
  int err = 0;
  int i;

  if (z == NULL) {
    return -1;
  }
  memset(&fi, 0, sizeof(fi));
  fill_str(longname, sizeof(longname), namelen, 'z');
  memcpy(longname, "http://", 7);
  for (i = 0; i < 2; i++) {
    const char *const name = i == 0 ? longname : "http://example.com/sane.html";

    err |= zipOpenNewFileInZip(z, name, &fi, hdr, (uInt) strlen(hdr), NULL, 0,
                               NULL, Z_DEFLATED, Z_DEFAULT_COMPRESSION);
    err |= zipWriteInFileInZip(z, body, (int) strlen(body));
    err |= zipCloseFileInZip(z);
  }
  err |= zipClose(z, NULL);
  return err;
}

int cache_url_bounds_selftest(httrackp *opt, const char *dir) {
  int failures = 0;
  cache_back cache;
  /* the largest pair lien_back can hold, and a twin differing only in its last
     byte: a key clipped anywhere on the path aliases the two together */
  static char big_adr[HTS_URLMAXSIZE * 2], big_fil[HTS_URLMAXSIZE * 2];
  static char twin_fil[HTS_URLMAXSIZE * 2];
  /* past every buffer on the path: no lien_back holds these, but the cache API
     takes plain pointers, and dropping the entry beats aborting the mirror.
     One overshoots url_fil, the other url_adr, so a bound on one destination
     only is not enough to pass */
  static char huge_fil[CACHE_ENTRYNAME_SIZE * 2];
  static char huge_adr[CACHE_ENTRYNAME_SIZE * 2];
  /* the key a writer that clipped huge_fil would have stored it under */
  static char decoy_fil[HTS_URLMAXSIZE * 2];
  const char *const short_adr = "example.com";
  const char *const short_fil = "/after.html";
  const char *const big_body = "<html><body>maxed</body></html>";
  const char *const twin_body = "<html><body>twin</body></html>";
  const char *const short_body = "<html><body>after</body></html>";
  const char *const decoy_body = "<html><body>decoy</body></html>";
  const char *const lm = "Mon, 01 Jan 2024 00:00:00 GMT";

  fill_str(big_adr, sizeof(big_adr), sizeof(big_adr) - 1, 'a');
  big_fil[0] = '/';
  fill_str(big_fil + 1, sizeof(big_fil) - 1, sizeof(big_fil) - 2, 'f');
  memcpy(twin_fil, big_fil, sizeof(twin_fil));
  twin_fil[sizeof(twin_fil) - 2] = 'g';
  huge_fil[0] = '/';
  fill_str(huge_fil + 1, sizeof(huge_fil) - 1, sizeof(huge_fil) - 2, 'h');
  fill_str(huge_adr, sizeof(huge_adr), sizeof(huge_adr) - 1, 'A');
  decoy_fil[0] = '/';
  fill_str(decoy_fil + 1, sizeof(decoy_fil) - 1, sizeof(decoy_fil) - 2, 'h');

  selftest_tag = "cache-urlbounds";
  selftest_setup_dir(opt, dir);

  selftest_open_for_write(&cache, opt);
  store_entry(opt, &cache, big_adr, big_fil, "example.com/big.html", 200, "OK",
              "text/html", "utf-8", lm, "", "", "", big_body, strlen(big_body));
  store_entry(opt, &cache, big_adr, twin_fil, "example.com/twin.html", 200,
              "OK", "text/html", "utf-8", lm, "", "", "", twin_body,
              strlen(twin_body));
  /* stored first, so a writer that clipped rather than dropped would overwrite
     it under its own key */
  store_entry(opt, &cache, big_adr, decoy_fil, "example.com/decoy.html", 200,
              "OK", "text/html", "utf-8", lm, "", "", "", decoy_body,
              strlen(decoy_body));
  store_entry(opt, &cache, big_adr, huge_fil, "example.com/huge.html", 200,
              "OK", "text/html", "utf-8", lm, "", "", "", "dropped", 7);
  store_entry(opt, &cache, huge_adr, "/short.html", "example.com/wide.html",
              200, "OK", "text/html", "utf-8", lm, "", "", "", "dropped", 7);
  /* written after the dropped one: the drop must cost nothing but itself */
  store_entry(opt, &cache, short_adr, short_fil, "example.com/after.html", 200,
              "OK", "text/html", "utf-8", lm, "", "", "", short_body,
              strlen(short_body));
  selftest_close(&cache);

  selftest_open_for_read(&cache, opt);
  /* the twin differs from the maxed entry only in its last byte: a key clipped
     anywhere on the path serves one for the other */
  failures += check_entry(opt, &cache, big_adr, big_fil, 200, "OK", "text/html",
                          "utf-8", lm, "", "", "", big_body, strlen(big_body));
  failures +=
      check_entry(opt, &cache, big_adr, twin_fil, 200, "OK", "text/html",
                  "utf-8", lm, "", "", "", twin_body, strlen(twin_body));
  failures +=
      check_entry(opt, &cache, short_adr, short_fil, 200, "OK", "text/html",
                  "utf-8", lm, "", "", "", short_body, strlen(short_body));
  failures +=
      check_entry(opt, &cache, big_adr, decoy_fil, 200, "OK", "text/html",
                  "utf-8", lm, "", "", "", decoy_body, strlen(decoy_body));
  failures += expect_miss(opt, &cache, big_adr, huge_fil);
  failures += expect_miss(opt, &cache, huge_adr, "/short.html");
  selftest_close(&cache);

  /* An entry name past every buffer on the path must be skipped, not indexed
     from an unterminated read -- which only the sanitizer legs see. */
  {
    char zippath[HTS_URLMAXSIZE];
    /* no pair of lien_back fields reaches this key, so look it up as one */
    static char foreign_fil[CACHE_KEY_SIZE];

    slprintfbuff_clip(zippath, sizeof(zippath), "%shts-cache/new.zip",
                      StringBuff(opt->path_log));
    if (urlbounds_write_foreign_zip(zippath, CACHE_ENTRYNAME_SIZE - 2) != 0) {
      fprintf(stderr, "cache-urlbounds: could not write the foreign cache\n");
      failures++;
    } else {
      /* the name the loader saw, minus the "http://" the index load strips */
      fill_str(foreign_fil, sizeof(foreign_fil), CACHE_ENTRYNAME_SIZE - 2 - 7,
               'z');
      selftest_open_for_read(&cache, opt);
      failures +=
          check_entry(opt, &cache, "example.com", "/sane.html", 200, "OK",
                      "text/html", "utf-8", "", "", "", "", "okay", 4);
      failures += expect_miss(opt, &cache, "", foreign_fil);
      selftest_close(&cache);
    }
  }

  return failures;
}

/* Buffer the reader rebuilds the save name into, so the longest name that fits
   is one byte less. */
#define SAVEBOUNDS_CAP (HTS_URLMAXSIZE * 2)
/* X-Save length used here, well inside the header line the reader can hold. */
#define SAVEBOUNDS_REL_LEN 1000
#define SAVEBOUNDS_GUARD 16
#define SAVEBOUNDS_GUARD_BYTE '\x5a'

/* A path of exactly len bytes, slash-bounded like an -O directory. */
static void savebounds_path(char *s, size_t size, size_t len) {
  assertf(len >= 2);
  fill_str(s, size, len, 'd');
  s[0] = s[len - 1] = '/';
}

/* Read the entry back with path_html_utf8 set to `path`, and check what the
   reader rebuilt. want/body are the save name and body it must return, or NULL
   for an entry whose rebuilt name cannot fit and must be refused. */
static int savebounds_expect(httrackp *opt, const char *path, const char *adr,
                             const char *fil, const char *want,
                             const char *body) {
  int failures = 0;
  cache_back cache;
  htsblk r;
  char *got = malloct(SAVEBOUNDS_CAP + SAVEBOUNDS_GUARD);
  size_t k;

  /* guard past the capacity the reader is handed, poisoned non-zero so a stray
     NUL shows up too */
  memset(got, SAVEBOUNDS_GUARD_BYTE, SAVEBOUNDS_CAP + SAVEBOUNDS_GUARD);
  got[0] = '\0';

  StringCopy(opt->path_html_utf8, path);
  selftest_open_for_read(&cache, opt);
  r = cache_readex(opt, &cache, adr, fil, "", NULL, got, 1);
  selftest_close(&cache);

  if (want != NULL) {
    if (r.statuscode != 200) {
      fprintf(stderr,
              "%s: html path of %d bytes: statuscode %d, expected 200 (%s)\n",
              selftest_tag, (int) strlen(path), r.statuscode, r.msg);
      failures++;
    }
    if (strcmp(got, want) != 0) {
      fprintf(stderr,
              "%s: html path of %d bytes: save name is %d bytes '%.48s...',"
              " expected %d bytes '%.48s...'\n",
              selftest_tag, (int) strlen(path), (int) strlen(got), got,
              (int) strlen(want), want);
      failures++;
    }
    if (r.adr == NULL || (size_t) r.size != strlen(body) ||
        memcmp(r.adr, body, strlen(body)) != 0) {
      fprintf(stderr, "%s: html path of %d bytes: body mismatch\n",
              selftest_tag, (int) strlen(path));
      failures++;
    }
  } else {
    if (r.statuscode != STATUSCODE_INVALID) {
      fprintf(stderr,
              "%s: html path of %d bytes: statuscode %d, expected the entry to"
              " be refused\n",
              selftest_tag, (int) strlen(path), r.statuscode);
      failures++;
    }
    /* the point of the refusal: a clipped name would name another file */
    if (got[0] != '\0') {
      fprintf(stderr,
              "%s: html path of %d bytes: refused entry still returns a %d-byte"
              " save name '%.48s...'\n",
              selftest_tag, (int) strlen(path), (int) strlen(got), got);
      failures++;
    }
    if (r.adr != NULL) {
      fprintf(stderr,
              "%s: html path of %d bytes: refused entry returns a body\n",
              selftest_tag, (int) strlen(path));
      failures++;
    }
  }
  for (k = SAVEBOUNDS_CAP; k < SAVEBOUNDS_CAP + SAVEBOUNDS_GUARD; k++) {
    if (got[k] != SAVEBOUNDS_GUARD_BYTE) {
      fprintf(stderr, "%s: html path of %d bytes: guard byte %d overwritten\n",
              selftest_tag, (int) strlen(path), (int) (k - SAVEBOUNDS_CAP));
      failures++;
      break;
    }
  }

  if (r.adr != NULL) {
    freet(r.adr);
  }
  freet(got);
  return failures;
}

/* The rebuilt name cannot fit under `path`: the entry must come back refused,
   with neither a save name nor a body. */
static int savebounds_expect_refused(httrackp *opt, const char *path,
                                     const char *adr, const char *fil) {
  return savebounds_expect(opt, path, adr, fil, NULL, NULL);
}

/* Store one entry, with path_prefix as a mirror passes it: non-NULL strips the
   html path off X-Save, NULL leaves the name absolute (the pre-3.40 shape). */
static void savebounds_store(httrackp *opt, cache_back *cache, const char *adr,
                             const char *fil, const char *save,
                             const char *path_prefix, const char *body) {
  htsblk w;
  char locw[4];
  char *bodycopy = strdupt(body);

  hts_init_htsblk(&w);
  w.statuscode = 200;
  w.size = (LLint) strlen(body);
  strcpybuff(w.msg, "OK");
  strcpybuff(w.contenttype, "text/html");
  strcpybuff(w.charset, "utf-8");
  locw[0] = '\0';
  w.location = locw;
  w.is_write = 0;
  w.adr = bodycopy;
  cache_add(opt, cache, &w, adr, fil, save, 1, path_prefix);
  freet(bodycopy);
}

int cache_savename_bounds_selftest(httrackp *opt, const char *dir) {
  int failures = 0;
  cache_back cache;
  static const char rel_head[] = "example.com/";
  static const char rel_tail[] = ".html";
  const char *const adr = "example.com";
  const char *const fil = "/deep.html";
  /* second entry, stored with an absolute X-Save: exercises the branch that
     copies the stored name verbatim instead of prepending */
  const char *const flat_fil = "/flat.html";
  static const char body[] = "<html><body>deep</body></html>";
  static const char flat_body[] = "<html><body>flat</body></html>";
  static char rel[SAVEBOUNDS_REL_LEN + 1];
  /* the rebuilt name ends on the last byte that fits, then one byte past it */
  static char path_fit[SAVEBOUNDS_CAP], path_over[SAVEBOUNDS_CAP];
  /* the html path the issue reported against */
  static char path_huge[SAVEBOUNDS_CAP];
  char BIGSTK writepath[HTS_URLMAXSIZE * 2];
  char BIGSTK save[HTS_URLMAXSIZE * 2];
  char BIGSTK flat_save[HTS_URLMAXSIZE * 2];
  /* concat() refuses a result that only just fits, and the longest expected
     name here is exactly SAVEBOUNDS_CAP - 1 bytes */
  char BIGSTK expected[SAVEBOUNDS_CAP + 8];

  selftest_tag = "cache-savebounds";
  selftest_setup_dir(opt, dir);
  strcpybuff(writepath, StringBuff(opt->path_html_utf8));

  memset(rel, 'p', SAVEBOUNDS_REL_LEN);
  rel[SAVEBOUNDS_REL_LEN] = '\0';
  memcpy(rel, rel_head, sizeof(rel_head) - 1);
  memcpy(rel + SAVEBOUNDS_REL_LEN - (sizeof(rel_tail) - 1), rel_tail,
         sizeof(rel_tail) - 1);
  savebounds_path(path_fit, sizeof(path_fit),
                  SAVEBOUNDS_CAP - 1 - SAVEBOUNDS_REL_LEN);
  savebounds_path(path_over, sizeof(path_over),
                  SAVEBOUNDS_CAP - SAVEBOUNDS_REL_LEN);
  savebounds_path(path_huge, sizeof(path_huge), 1500);
  concat(save, sizeof(save), writepath, rel);
  concat(flat_save, sizeof(flat_save), writepath, "example.com/flat.html");

  selftest_open_for_write(&cache, opt);
  savebounds_store(opt, &cache, adr, fil, save, writepath, body);
  savebounds_store(opt, &cache, adr, flat_fil, flat_save, NULL, flat_body);
  selftest_close(&cache);

  /* same html path as the write: the stripped prefix goes back on unchanged */
  failures += savebounds_expect(opt, writepath, adr, fil, save, body);
  /* an absolute stored name is used as-is, not prefixed a second time */
  failures +=
      savebounds_expect(opt, writepath, adr, flat_fil, flat_save, flat_body);

  /* deeper path, rebuilt name still fits to the last byte */
  concat(expected, sizeof(expected), path_fit, rel);
  failures += savebounds_expect(opt, path_fit, adr, fil, expected, body);

  /* one byte past: refuse and re-fetch rather than name another file */
  failures += savebounds_expect_refused(opt, path_over, adr, fil);
  failures += savebounds_expect_refused(opt, path_huge, adr, fil);

  return failures;
}

/* Poison bytes: CANARY fills the field a msg overrun would land in, FILL the
   oversized reason string. Non-zero, so a stray NUL terminator shows up. */
#define READFAIL_CANARY 'C'
#define READFAIL_FILL 'W'

/* Store one entry through cache_zip_store_stream, committing it only when the
   store succeeded (what cache_add does). Hands back the bytes the ZIP holds for
   it, or -1 when the entry is absent. */
static int readfail_roundtrip(const char *path, FILE *src, int want_status,
                              char *out, size_t outsz) {
  zipFile zf = hts_zipOpen_utf8(path, APPEND_STATUS_CREATE);
  zip_fileinfo fi;
  unzFile uf;
  int got, n;

  if (zf == NULL)
    return -1;
  memset(&fi, 0, sizeof(fi));
  if (zipOpenNewFileInZip(zf, "entry.bin", &fi, NULL, 0, NULL, 0, NULL,
                          Z_DEFLATED, Z_DEFAULT_COMPRESSION) != ZIP_OK) {
    zipClose(zf, NULL);
    return -1;
  }
  got = cache_zip_store_stream(zf, src);
  if (got == Z_OK)
    zipCloseFileInZip(zf);
  else
    zipAbandonFileInZip(zf);
  zipClose(zf, NULL);
  if (got != want_status) {
    fprintf(stderr, "cache-readfail: status %d, want %d\n", got, want_status);
    return -1;
  }
  uf = hts_unzOpen_utf8(path);
  if (uf == NULL || unzLocateFile(uf, "entry.bin", 1) != UNZ_OK ||
      unzOpenCurrentFile(uf) != UNZ_OK) {
    if (uf != NULL)
      unzClose(uf);
    return -1;
  }
  n = unzReadCurrentFile(uf, out, (unsigned) outsz);
  unzCloseCurrentFile(uf);
  unzClose(uf);
  return n;
}

/* Store one is_write entry: cache_add re-reads the body from `save` itself. */
static void readfail_store_file(httrackp *opt, cache_back *cache,
                                const char *fil, const char *save,
                                LLint declared) {
  htsblk r;
  char locbuf[4];

  hts_init_htsblk(&r);
  r.statuscode = 200;
  r.size = declared;
  strcpybuff(r.msg, "OK");
  strcpybuff(r.contenttype, "application/octet-stream");
  locbuf[0] = '\0';
  r.location = locbuf;
  r.is_write = 1;
  cache_add(opt, cache, &r, "example.com", fil, save, 1, NULL);
}

/* Write `len` bytes of body at the fconv()-resolved `save`. */
static int readfail_write_source(const char *save, const char *body,
                                 size_t len) {
  char catbuff[HTS_URLMAXSIZE * 2];
  char *path = fconv(catbuff, sizeof(catbuff), save);
  FILE *fp;

  (void) structcheck(path);
  fp = FOPEN(path, "wb");
  if (fp == NULL)
    return -1;
  if (len != 0 && !hts_fwrite_exact(body, len, fp)) {
    fclose(fp);
    return -1;
  }
  fclose(fp);
  return 0;
}

/* A source cache_add sizes and opens but cannot read: on Linux /proc/self/mem
   is a regular zero-length file whose reads return EIO. NULL elsewhere, so the
   case skips rather than assert on a branch it never reached. */
static const char *readfail_unreadable_source(void) {
  static const char path[] = "/proc/self/mem";
  char catbuff[HTS_URLMAXSIZE * 2];
  char probe[1];
  FILE *fp;
  int usable;

  if (fsize_utf8(fconv(catbuff, sizeof(catbuff), path)) < 0)
    return NULL;
  fp = FOPEN(fconv(catbuff, sizeof(catbuff), path), "rb");
  if (fp == NULL)
    return NULL;
  usable = fread(probe, 1, sizeof(probe), fp) == 0 && ferror(fp) != 0;
  fclose(fp);
  return usable ? path : NULL;
}

/* A short read is indistinguishable from EOF, so a failed read must abandon the
   entry: committing it would leave a truncated body under the X-Size the local
   header already declares, which the reader then has to catch. */
int cache_readfail_selftest(httrackp *opt, const char *dir) {
  static const char body[] = "cache-readfail body\x00\x01\x02 payload";
  const size_t body_len = sizeof(body) - 1;
  char zippath[HTS_URLMAXSIZE], srcpath[HTS_URLMAXSIZE];
  char catbuff[CATBUFF_SIZE];
  char out[256];
  const char *unreadable = readfail_unreadable_source();
  FILE *fp;
  int fail = 0, n;

  fconcat(zippath, sizeof(zippath), dir, "readfail.zip");
  fconcat(srcpath, sizeof(srcpath), dir, "readfail-src.bin");

  /* control: a readable source still copies byte for byte */
  fp = FOPEN(fconv(catbuff, sizeof(catbuff), srcpath), "wb");
  assertf(fp != NULL);
  assertf(fwrite(body, 1, body_len, fp) == body_len);
  fclose(fp);
  fp = FOPEN(fconv(catbuff, sizeof(catbuff), srcpath), "rb");
  assertf(fp != NULL);
  n = readfail_roundtrip(zippath, fp, Z_OK, out, sizeof(out));
  fclose(fp);
  if (n != (int) body_len || memcmp(out, body, body_len) != 0) {
    fprintf(stderr, "cache-readfail: readable source stored %d/%d byte(s)\n", n,
            (int) body_len);
    fail++;
  }

  /* control: an empty source is not an error */
  fp = FOPEN(fconv(catbuff, sizeof(catbuff), srcpath), "wb");
  assertf(fp != NULL);
  fclose(fp);
  fp = FOPEN(fconv(catbuff, sizeof(catbuff), srcpath), "rb");
  assertf(fp != NULL);
  n = readfail_roundtrip(zippath, fp, Z_OK, out, sizeof(out));
  fclose(fp);
  if (n != 0) {
    fprintf(stderr, "cache-readfail: empty source stored %d byte(s)\n", n);
    fail++;
  }

  /* a write-only stream fails every read: the entry must not reach the ZIP */
  fp = FOPEN(fconv(catbuff, sizeof(catbuff), srcpath), "wb");
  assertf(fp != NULL);
  n = readfail_roundtrip(zippath, fp, CACHE_ZIP_READ_ERROR, out, sizeof(out));
  fclose(fp);
  if (n != -1) {
    fprintf(stderr, "cache-readfail: unreadable source left an entry (%d)\n",
            n);
    fail++;
  }

  /* the real cache_add path: an unreadable body drops the whole entry, and the
     failure streak is charged once per entry, not twice */
  if (unreadable == NULL) {
    fprintf(stderr,
            "cache-readfail: no unreadable source here, case skipped\n");
  } else {
    cache_back cache;
    char extra[8192];
    char rbody[256];
    int i;

    memset(&cache, 0, sizeof(cache));
    cache.type = 1;
    cache.log = stderr;
    cache.errlog = stderr;
    cache.hashtable = coucal_new(0);
    cache.zipOutput = hts_zipOpen_utf8(zippath, APPEND_STATUS_CREATE);
    assertf(cache.zipOutput != NULL);
    opt->state.exit_xh = 0;

    if (readfail_write_source(srcpath, body, body_len) != 0) {
      fprintf(stderr, "cache-readfail: cannot create '%s'\n", srcpath);
      return fail + 1;
    }
    readfail_store_file(opt, &cache, "/good.bin", srcpath, (LLint) body_len);
    readfail_store_file(opt, &cache, "/unreadable.bin", unreadable,
                        (LLint) body_len);
    /* the rolled-back member must leave the stream usable */
    readfail_store_file(opt, &cache, "/good2.bin", srcpath, (LLint) body_len);
    /* CACHE_MAX_WRITE_FAILURES consecutive drops abort the mirror; charging
       one failed entry twice would trip it after half as many */
    for (i = 1; i <= CACHE_MAX_WRITE_FAILURES; i++) {
      char fil[32];

      if (cache.zipWriteFailed) {
        fprintf(stderr,
                "cache-readfail: aborted after %d dropped entr(ies), expected "
                "%d\n",
                i - 1, CACHE_MAX_WRITE_FAILURES);
        fail++;
        break;
      }
      snprintf(fil, sizeof(fil), "/u%d.bin", i);
      readfail_store_file(opt, &cache, fil, unreadable, (LLint) body_len);
    }
    if (!cache.zipWriteFailed || opt->state.exit_xh != -1) {
      fprintf(stderr,
              "cache-readfail: %d dropped entries did not abort the mirror "
              "(flagged=%d, exit_xh=%d)\n",
              CACHE_MAX_WRITE_FAILURES, (int) cache.zipWriteFailed,
              opt->state.exit_xh);
      fail++;
    }
    opt->state.exit_xh = 0; /* the abort is the assertion, not a real one */
    zipClose(cache.zipOutput, NULL);
    cache.zipOutput = NULL;

    if (writefail_read_entry(zippath, "http://example.com/unreadable.bin",
                             extra, sizeof(extra), rbody, sizeof(rbody)) >= 0) {
      fprintf(stderr, "cache-readfail: the unreadable entry was cached\n");
      fail++;
    }
    for (i = 0; i < 2; i++) {
      const char *const sibling = i == 0 ? "http://example.com/good.bin"
                                         : "http://example.com/good2.bin";

      n = writefail_read_entry(zippath, sibling, extra, sizeof(extra), rbody,
                               sizeof(rbody));
      if (n != (int) body_len || memcmp(rbody, body, body_len) != 0) {
        fprintf(stderr, "cache-readfail: '%s' stored %d byte(s)\n", sibling, n);
        fail++;
      }
    }
  }

  /* a truncated entry must not read back as a complete file */
  {
    cache_back cache;
    char save[HTS_URLMAXSIZE * 2];
    char *locbuf = malloct(HTS_LOCATION_SIZE);
    htsblk r;
    const LLint stored = (LLint) body_len;
    const LLint declared = stored + 4096;

    concat(save, sizeof(save), StringBuff(opt->path_html_utf8),
           "example.com/trunc.bin");
    if (readfail_write_source(save, body, body_len) != 0) {
      fprintf(stderr, "cache-readfail: cannot create '%s'\n", save);
      freet(locbuf);
      return fail + 1;
    }
    selftest_open_for_write(&cache, opt);
    /* the source is short of what the headers declare: exactly the entry a
       failed read used to commit */
    readfail_store_file(opt, &cache, "/trunc.bin", save, declared);
    readfail_store_file(opt, &cache, "/whole.bin", save, stored);
    selftest_close(&cache);

    selftest_open_for_read(&cache, opt);
    locbuf[0] = '\0';
    /* readonly=0: the direct-disk branch, which writes the body out again */
    r = cache_readex(opt, &cache, "example.com", "/trunc.bin", save, locbuf,
                     NULL, 0);
    if (r.statuscode != STATUSCODE_INVALID) {
      fprintf(stderr,
              "cache-readfail: truncated entry read back as %d ('%s')\n",
              r.statuscode, r.msg);
      fail++;
    }
    freet(r.adr);
    /* control: the same read on a whole entry succeeds */
    locbuf[0] = '\0';
    r = cache_readex(opt, &cache, "example.com", "/whole.bin", save, locbuf,
                     NULL, 0);
    if (r.statuscode != HTTP_OK) {
      fprintf(stderr, "cache-readfail: whole entry read back as %d ('%s')\n",
              r.statuscode, r.msg);
      fail++;
    } else if (fsize_utf8(fconv(catbuff, sizeof(catbuff), save)) != stored) {
      fprintf(stderr,
              "cache-readfail: whole entry restored " LLintP
              " byte(s), expected " LLintP "\n",
              (LLint) fsize_utf8(fconv(catbuff, sizeof(catbuff), save)),
              stored);
      fail++;
    }
    freet(r.adr);
    freet(locbuf);
    selftest_close(&cache);
  }

  /* r.msg is 80 bytes inside an installed-header struct, and both the reason
     and strerror() are unbounded, so the message must clip, not smash. */
  {
    static const int errs[] = {0, ENOENT, EIO, EACCES};
    char what[512];
    size_t i;

    memset(what, READFAIL_FILL, sizeof(what) - 1);
    what[sizeof(what) - 1] = '\0';
    for (i = 0; i < sizeof(errs) / sizeof(errs[0]); i++) {
      htsblk r;

      hts_init_htsblk(&r);
      memset(r.contenttype, READFAIL_CANARY, sizeof(r.contenttype));
      r.statuscode = HTTP_OK; /* poisoned: the failure must overwrite it */
      cache_read_failed(&r, what, errs[i]);
      if (r.msg[sizeof(r.msg) - 1] != '\0' || strlen(r.msg) >= sizeof(r.msg)) {
        fprintf(stderr, "cache-readfail: msg not terminated in %d bytes\n",
                (int) sizeof(r.msg));
        fail++;
      }
      if (memchr(r.contenttype, READFAIL_CANARY, sizeof(r.contenttype)) ==
              NULL ||
          r.contenttype[0] != READFAIL_CANARY) {
        fprintf(stderr,
                "cache-readfail: the message overran msg into "
                "contenttype (errno %d)\n",
                errs[i]);
        fail++;
      }
      if (r.statuscode != STATUSCODE_INVALID) {
        fprintf(stderr, "cache-readfail: statuscode left at %d\n",
                r.statuscode);
        fail++;
      }
    }
  }

  UNLINK(fconv(catbuff, sizeof(catbuff), srcpath));
  UNLINK(fconv(catbuff, sizeof(catbuff), zippath));
  return fail;
}

/* --- the portable resume reference (.ref) --------------------------------- */

/* The record ref_put_record() writes, one row per field, in order. The fixture
   is assembled from this table rather than captured from the writer, so a
   reviewer reads the format here instead of decoding 355 hex bytes. Every
   value is distinct, so no swap of two fields can hide, and each multi-byte
   number has all-different bytes so its order on disk is visible. */
typedef enum {
  REF_F_RAW, /* verbatim bytes, no length prefix (the magic) */
  REF_F_U32, /* 4 bytes, little-endian */
  REF_F_I32, /* 4 bytes, little-endian, two's complement */
  REF_F_I64, /* 8 bytes, little-endian, two's complement */
  REF_F_STR, /* u32 length, then that many bytes */
  REF_F_BLOB /* u64 length, then that many bytes */
} ref_field_kind;

typedef struct {
  const char *name;
  ref_field_kind kind;
  int64_t num;      /* value, for the numeric kinds */
  const char *text; /* bytes, for REF_F_RAW / STR / BLOB */
} ref_field;

#define REF_ST_ADR "example.com"
#define REF_ST_FIL "/partial.bin"
#define REF_ST_SAV "example.com/partial.bin"
#define REF_ST_BODY "twenty-three bytes here"
#define REF_ST_HEADERS "HTTP/1.1 206 Partial Content\r\n\r\n"
#define REF_ST_STATUS 0x11121314
#define REF_ST_NOTMODIFIED 0x0102
#define REF_ST_COMPRESSED 0x0304
#define REF_ST_EMPTY 0x0506
#define REF_ST_TOTALSIZE ((int64_t) 0x0102030405060708LL)
#define REF_ST_CRANGE ((int64_t) 0x1112131415161718LL)
#define REF_ST_CRANGE_START ((int64_t) -0x0e0d0c0b0a090808LL)
#define REF_ST_CRANGE_END ((int64_t) 0x2122232425262728LL)

static const ref_field ref_fields[] = {
    {"magic", REF_F_RAW, 0, HTS_REF_MAGIC},
    {"version", REF_F_U32, HTS_REF_VERSION, NULL},
    {"url_adr", REF_F_STR, 0, REF_ST_ADR},
    {"url_fil", REF_F_STR, 0, REF_ST_FIL},
    {"url_sav", REF_F_STR, 0, REF_ST_SAV},
    {"referer_adr", REF_F_STR, 0, "ref.example"},
    {"referer_fil", REF_F_STR, 0, "/from.html"},
    {"location", REF_F_STR, 0, "http://moved.example/elsewhere"},
    {"statuscode", REF_F_I32, REF_ST_STATUS, NULL},
    {"notmodified", REF_F_I32, REF_ST_NOTMODIFIED, NULL},
    {"compressed", REF_F_I32, REF_ST_COMPRESSED, NULL},
    {"empty", REF_F_I32, REF_ST_EMPTY, NULL},
    {"size", REF_F_I64, sizeof(REF_ST_BODY) - 1, NULL},
    {"totalsize", REF_F_I64, REF_ST_TOTALSIZE, NULL},
    {"crange", REF_F_I64, REF_ST_CRANGE, NULL},
    {"crange_start", REF_F_I64, REF_ST_CRANGE_START, NULL},
    {"crange_end", REF_F_I64, REF_ST_CRANGE_END, NULL},
    {"msg", REF_F_STR, 0, "Partial Content"},
    {"contenttype", REF_F_STR, 0, "application/octet-stream"},
    {"charset", REF_F_STR, 0, "utf-8"},
    {"contentencoding", REF_F_STR, 0, "gzip"},
    {"lastmodified", REF_F_STR, 0, "Wed, 03 Sep 2026 10:11:12 GMT"},
    {"etag", REF_F_STR, 0, "\"abc-123\""},
    {"cdispo", REF_F_STR, 0, "attachment; filename=partial.bin"},
    {"body", REF_F_BLOB, 0, REF_ST_BODY},
    {"headers", REF_F_STR, 0, REF_ST_HEADERS}};

#define REF_FIELD_COUNT (sizeof(ref_fields) / sizeof(ref_fields[0]))

/* Where each field's payload begins, filled by ref_st_build(): the length
   prefix for STR and BLOB, the number itself otherwise. */
static size_t ref_off[REF_FIELD_COUNT];
static unsigned char *ref_golden;
static size_t ref_golden_size;

/* Field index by name, so a reordered table moves the offsets with it. */
static size_t ref_st_field(const char *name) {
  size_t i;

  for (i = 0; i < REF_FIELD_COUNT; i++)
    if (strcmp(ref_fields[i].name, name) == 0)
      return i;
  assertf(!"no such field");
  return 0;
}

/* Low byte first, spelled out, so the fixture states the byte order rather
   than inheriting the host's. */
static size_t ref_st_num(unsigned char *dst, uint64_t v, size_t width) {
  size_t i;

  for (i = 0; i < width; i++)
    dst[i] = (unsigned char) ((v >> (8 * i)) & 0xff);
  return width;
}

/* Assemble the table into dst (NULL to measure), recording field offsets. */
static size_t ref_st_build(unsigned char *dst) {
  size_t off = 0;
  size_t i;

  for (i = 0; i < REF_FIELD_COUNT; i++) {
    const ref_field *const f = &ref_fields[i];
    const size_t len = f->text != NULL ? strlen(f->text) : 0;

    ref_off[i] = off;
    switch (f->kind) {
    case REF_F_RAW:
      if (dst != NULL)
        memcpy(dst + off, f->text, len);
      off += len;
      break;
    case REF_F_U32:
    case REF_F_I32:
      if (dst != NULL)
        ref_st_num(dst + off, (uint64_t) (uint32_t) f->num, 4);
      off += 4;
      break;
    case REF_F_I64:
      if (dst != NULL)
        ref_st_num(dst + off, (uint64_t) f->num, 8);
      off += 8;
      break;
    case REF_F_STR:
    case REF_F_BLOB: {
      const size_t width = f->kind == REF_F_STR ? 4 : 8;

      if (dst != NULL) {
        ref_st_num(dst + off, (uint64_t) len, width);
        memcpy(dst + off + width, f->text, len);
      }
      off += width + len;
      break;
    }
    }
  }
  return off;
}

/* Copy the reference's path out of the option scratch buffer, which the very
   next call reuses. */
static void ref_st_refpath(httrackp *opt, char *dst, size_t size) {
  strlcpybuff(dst, url_savename_refname_fullpath(opt, REF_ST_ADR, REF_ST_FIL),
              size);
}

static void ref_st_put(const char *path, const void *data, size_t size) {
  FILE *const fp = fopen(path, "wb");

  assertf(fp != NULL);
  assertf(hts_fwrite_exact(data, size, fp));
  fclose(fp);
}

static int ref_st_str(const char *got, const char *want, const char *field) {
  if (strcmp(got, want) != 0) {
    fprintf(stderr, "%s: %s is '%s', expected '%s'\n", selftest_tag, field, got,
            want);
    return 1;
  }
  return 0;
}

static int ref_st_int(LLint got, LLint want, const char *field) {
  if (got != want) {
    fprintf(stderr, "%s: %s is " LLintP ", expected " LLintP "\n", selftest_tag,
            field, got, want);
    return 1;
  }
  return 0;
}

/* The bytes a field must occupy on disk, spelled out rather than derived, so
   the check is independent of how the table was assembled. */
static int ref_st_bytes(const unsigned char *file, const char *field,
                        const unsigned char *want, size_t len) {
  const size_t off = ref_off[ref_st_field(field)];

  if (memcmp(file + off, want, len) != 0) {
    size_t i;

    fprintf(stderr, "%s: %s at offset %d reads", selftest_tag, field,
            (int) off);
    for (i = 0; i < len; i++)
      fprintf(stderr, " %02x", file[off + i]);
    fprintf(stderr, ", expected");
    for (i = 0; i < len; i++)
      fprintf(stderr, " %02x", want[i]);
    fprintf(stderr, "\n");
    return 1;
  }
  return 0;
}

/* Every field against the table, never against what the reader produced. */
static int ref_st_check(const lien_back *back) {
  int fail = 0;

  fail += ref_st_str(back->url_adr, REF_ST_ADR, "url_adr");
  fail += ref_st_str(back->url_fil, REF_ST_FIL, "url_fil");
  fail += ref_st_str(back->url_sav, REF_ST_SAV, "url_sav");
  fail += ref_st_str(back->referer_adr, "ref.example", "referer_adr");
  fail += ref_st_str(back->referer_fil, "/from.html", "referer_fil");
  fail += ref_st_str(back->r.location, "http://moved.example/elsewhere",
                     "location");
  fail += ref_st_int(back->r.statuscode, REF_ST_STATUS, "statuscode");
  fail += ref_st_int(back->r.notmodified, REF_ST_NOTMODIFIED, "notmodified");
  fail += ref_st_int(back->r.compressed, REF_ST_COMPRESSED, "compressed");
  fail += ref_st_int(back->r.empty, REF_ST_EMPTY, "empty");
  fail += ref_st_int(back->r.size, (LLint) (sizeof(REF_ST_BODY) - 1), "size");
  fail += ref_st_int(back->r.totalsize, REF_ST_TOTALSIZE, "totalsize");
  fail += ref_st_int(back->r.crange, REF_ST_CRANGE, "crange");
  fail += ref_st_int(back->r.crange_start, REF_ST_CRANGE_START, "crange_start");
  fail += ref_st_int(back->r.crange_end, REF_ST_CRANGE_END, "crange_end");
  fail += ref_st_str(back->r.msg, "Partial Content", "msg");
  fail += ref_st_str(back->r.contenttype, "application/octet-stream",
                     "contenttype");
  fail += ref_st_str(back->r.charset, "utf-8", "charset");
  fail += ref_st_str(back->r.contentencoding, "gzip", "contentencoding");
  fail += ref_st_str(back->r.lastmodified, "Wed, 03 Sep 2026 10:11:12 GMT",
                     "lastmodified");
  fail += ref_st_str(back->r.etag, "\"abc-123\"", "etag");
  fail +=
      ref_st_str(back->r.cdispo, "attachment; filename=partial.bin", "cdispo");
  if (back->r.location != back->location_buffer) {
    fprintf(stderr, "%s: location not restored into the entry's own buffer\n",
            selftest_tag);
    fail++;
  }
  if (back->r.adr == NULL ||
      memcmp(back->r.adr, REF_ST_BODY, sizeof(REF_ST_BODY)) != 0) {
    fprintf(stderr, "%s: body not restored\n", selftest_tag);
    fail++;
  }
  if (back->r.headers == NULL || strcmp(back->r.headers, REF_ST_HEADERS) != 0) {
    fprintf(stderr, "%s: headers not restored\n", selftest_tag);
    fail++;
  }
  /* nothing that only means something inside the writing process */
  if (back->r.soc != INVALID_SOCKET || back->r.out != NULL ||
      back->r.fp != NULL || back->tmpfile != NULL ||
      back->r.req.user_agent != NULL || back->send_too[0] != '\0') {
    fprintf(stderr, "%s: a live handle or option pointer came off the file\n",
            selftest_tag);
    fail++;
  }
  return fail;
}

/* A reference the reader must turn down: no entry comes back, and none is
   left half-built. */
static int ref_st_refuse(httrackp *opt, const char *path, const void *data,
                         size_t size, const char *what) {
  lien_back *back = NULL;

  ref_st_put(path, data, size);
  if (back_unserialize_ref(opt, REF_ST_ADR, REF_ST_FIL, &back) == 0) {
    fprintf(stderr, "%s: %s was accepted\n", selftest_tag, what);
    back_clear_entry(back);
    freet(back);
    return 1;
  }
  if (back != NULL) {
    fprintf(stderr, "%s: %s left an entry behind\n", selftest_tag, what);
    return 1;
  }
  return 0;
}

/* Read the reference back, or count a failure and return NULL. */
static lien_back *ref_st_accept(httrackp *opt, const char *what, int *fail) {
  lien_back *back = NULL;

  if (back_unserialize_ref(opt, REF_ST_ADR, REF_ST_FIL, &back) != 0) {
    fprintf(stderr, "%s: %s was refused\n", selftest_tag, what);
    (*fail)++;
    return NULL;
  }
  return back;
}

/* Rebuild the record with one string field's declared length raised to len,
   padded with 'A'. The caller frees. */
static unsigned char *ref_st_oversize(const char *field, size_t len,
                                      size_t *size) {
  const size_t i = ref_st_field(field);
  const size_t off = ref_off[i];
  const size_t tail = off + 4 + strlen(ref_fields[i].text);
  unsigned char *out;

  assertf(ref_fields[i].kind == REF_F_STR);

  *size = off + 4 + len + (ref_golden_size - tail);
  out = malloct(*size);
  memcpy(out, ref_golden, off);
  ref_st_num(out + off, (uint64_t) len, 4);
  memset(out + off + 4, 'A', len);
  memcpy(out + off + 4 + len, ref_golden + tail, ref_golden_size - tail);
  return out;
}

/* r.crange is the canary for an overflow of r.cdispo: the reader fills it
   earlier in the stream and never touches it again, so a stray byte or stray
   terminator past cdispo survives to the assertion. */
HTS_STATIC_ASSERT(offsetof(htsblk, crange) ==
                      offsetof(htsblk, cdispo) + sizeof(((htsblk *) 0)->cdispo),
                  ref_crange_follows_cdispo);

int ref_portable_selftest(httrackp *opt, const char *dir) {
  int fail = 0;
  char path[HTS_URLMAXSIZE * 2];
  lien_back *back;

  selftest_tag = "ref-portable";
  selftest_setup_dir(opt, dir);
#ifdef _WIN32
  mkdir(reconcile_st_path(opt, "hts-cache"));
  mkdir(reconcile_st_path(opt, CACHE_REFNAME));
#else
  mkdir(reconcile_st_path(opt, "hts-cache"), HTS_PROTECT_FOLDER);
  mkdir(reconcile_st_path(opt, CACHE_REFNAME), HTS_PROTECT_FOLDER);
#endif
  ref_st_refpath(opt, path, sizeof(path));

  ref_golden_size = ref_st_build(NULL);
  ref_golden = malloct(ref_golden_size);
  fail += ref_st_int((LLint) ref_st_build(ref_golden), (LLint) ref_golden_size,
                     "assembled record size");

  /* the writer emits the table, byte for byte */
  {
    lien_back *entry = calloct(1, sizeof(lien_back));
    unsigned char *got;
    LLint got_size;
    FILE *fp;

    hts_init_htsblk(&entry->r);
    entry->r.location = entry->location_buffer;
    strcpybuff(entry->url_adr, REF_ST_ADR);
    strcpybuff(entry->url_fil, REF_ST_FIL);
    strcpybuff(entry->url_sav, REF_ST_SAV);
    strcpybuff(entry->referer_adr, "ref.example");
    strcpybuff(entry->referer_fil, "/from.html");
    strcpybuff(entry->location_buffer, "http://moved.example/elsewhere");
    entry->r.statuscode = REF_ST_STATUS;
    entry->r.notmodified = REF_ST_NOTMODIFIED;
    entry->r.compressed = REF_ST_COMPRESSED;
    entry->r.empty = REF_ST_EMPTY;
    entry->r.size = (LLint) (sizeof(REF_ST_BODY) - 1);
    entry->r.totalsize = REF_ST_TOTALSIZE;
    entry->r.crange = REF_ST_CRANGE;
    entry->r.crange_start = REF_ST_CRANGE_START;
    entry->r.crange_end = REF_ST_CRANGE_END;
    strcpybuff(entry->r.msg, "Partial Content");
    strcpybuff(entry->r.contenttype, "application/octet-stream");
    strcpybuff(entry->r.charset, "utf-8");
    strcpybuff(entry->r.contentencoding, "gzip");
    strcpybuff(entry->r.lastmodified, "Wed, 03 Sep 2026 10:11:12 GMT");
    strcpybuff(entry->r.etag, "\"abc-123\"");
    strcpybuff(entry->r.cdispo, "attachment; filename=partial.bin");
    entry->r.adr = strdupt(REF_ST_BODY);
    entry->r.headers = strdupt(REF_ST_HEADERS);
    /* scaffolding that must not reach the file */
    entry->status = STATUS_TRANSFER;
    entry->r.soc = (T_SOC) 7;
    entry->r.req.user_agent = "must-not-be-serialized";
    strcpybuff(entry->send_too, "Range: bytes=11-\r\n");
    if (back_serialize_ref(opt, entry) != 0) {
      fprintf(stderr, "%s: cannot write the reference\n", selftest_tag);
      fail++;
    }
    freet(entry->r.adr);
    freet(entry->r.headers);
    freet(entry);

    got_size = fsize(path);
    got = malloct(ref_golden_size + 1);
    fp = fopen(path, "rb");
    assertf(fp != NULL);
    if (got_size != (LLint) ref_golden_size ||
        !hts_fread_exact(got, ref_golden_size, fp)) {
      fprintf(stderr, "%s: wrote " LLintP " bytes, expected %d\n", selftest_tag,
              got_size, (int) ref_golden_size);
      fail++;
    } else if (memcmp(got, ref_golden, ref_golden_size) != 0) {
      size_t i;

      for (i = 0; i < ref_golden_size && got[i] == ref_golden[i]; i++)
        ;
      fprintf(stderr, "%s: byte %d is 0x%02x, expected 0x%02x\n", selftest_tag,
              (int) i, got[i], ref_golden[i]);
      fail++;
    } else {
      /* the order those bytes go out in, one field per width and sign */
      static const unsigned char u32_le[4] = {0x14, 0x13, 0x12, 0x11};
      static const unsigned char i64_le[8] = {0x08, 0x07, 0x06, 0x05,
                                              0x04, 0x03, 0x02, 0x01};
      static const unsigned char neg_le[8] = {0xf8, 0xf7, 0xf6, 0xf5,
                                              0xf4, 0xf3, 0xf2, 0xf1};

      fail += ref_st_bytes(got, "statuscode", u32_le, sizeof(u32_le));
      fail += ref_st_bytes(got, "totalsize", i64_le, sizeof(i64_le));
      fail += ref_st_bytes(got, "crange_start", neg_le, sizeof(neg_le));
    }
    fclose(fp);
    freet(got);
  }

  /* and the reader takes them back, whatever host laid them down */
  ref_st_put(path, ref_golden, ref_golden_size);
  back = ref_st_accept(opt, "the golden reference", &fail);
  if (back != NULL) {
    fail += ref_st_check(back);
    back_clear_entry(back);
    freet(back);
  }

  /* exactly what 3.50.1 wrote: a host-native size_t, the raw structure, then
     the two data blocks, here both empty */
  {
    const size_t blit = sizeof(lien_back);
    const size_t size = sizeof(blit) * 3 + blit;
    unsigned char *legacy = calloct(1, size);
    lien_back *const old = (lien_back *) (legacy + sizeof(blit));

    memcpy(legacy, &blit, sizeof(blit));
    strcpybuff(old->url_adr, REF_ST_ADR);
    strcpybuff(old->url_fil, REF_ST_FIL);
    strcpybuff(old->url_sav, REF_ST_SAV);
    old->r.statuscode = 206;
    fail +=
        ref_st_refuse(opt, path, legacy, size, "a legacy host-native record");
    freet(legacy);
  }

  /* a wrong magic, a future version, and every truncation */
  {
    unsigned char *bad = malloct(ref_golden_size);
    const size_t cut[] = {0,
                          4,
                          8,
                          12,
                          ref_off[ref_st_field("statuscode")] + 2,
                          ref_off[ref_st_field("crange")] + 4,
                          ref_off[ref_st_field("body")] + 6,
                          ref_golden_size - 1};
    size_t i;

    memcpy(bad, ref_golden, ref_golden_size);
    memset(bad, 'X', HTS_REF_MAGIC_SIZE);
    fail += ref_st_refuse(opt, path, bad, ref_golden_size, "a wrong magic");

    memcpy(bad, ref_golden, ref_golden_size);
    bad[ref_off[ref_st_field("version")]] = HTS_REF_VERSION + 1;
    fail +=
        ref_st_refuse(opt, path, bad, ref_golden_size, "an unknown version");

    for (i = 0; i < sizeof(cut) / sizeof(cut[0]); i++)
      fail +=
          ref_st_refuse(opt, path, ref_golden, cut[i], "a truncated record");
    freet(bad);
  }

  /* the string cap, at its two boundary values, each file long enough that
     only the cap can decide it */
  {
    size_t size;
    unsigned char *at = ref_st_oversize("url_sav", HTS_REF_MAX_STR, &size);

    ref_st_put(path, at, size);
    freet(at);
    back = ref_st_accept(opt, "a name declaring exactly the cap", &fail);
    if (back != NULL) {
      fail += ref_st_int((LLint) strlen(back->url_sav),
                         (LLint) sizeof(back->url_sav) - 1, "clipped url_sav");
      fail += ref_st_int(back->r.statuscode, REF_ST_STATUS, "statuscode");
      back_clear_entry(back);
      freet(back);
    }
  }
  {
    size_t size;
    unsigned char *over =
        ref_st_oversize("url_sav", HTS_REF_MAX_STR + 1, &size);

    fail +=
        ref_st_refuse(opt, path, over, size, "a name one byte past the cap");
    freet(over);
  }

  /* a length past its destination clips, the reader stays in step with the
     stream, and the field the reader already filled next door is untouched */
  {
    size_t size;
    unsigned char *bad = ref_st_oversize("cdispo", 3000, &size);

    ref_st_put(path, bad, size);
    freet(bad);
    back = ref_st_accept(opt, "an over-long content-disposition", &fail);
    if (back != NULL) {
      const size_t want = sizeof(back->r.cdispo) - 1;

      if (strlen(back->r.cdispo) != want ||
          strspn(back->r.cdispo, "A") != want) {
        fprintf(stderr, "%s: cdispo clipped to %d bytes of '%s'\n",
                selftest_tag, (int) strlen(back->r.cdispo), back->r.cdispo);
        fail++;
      }
      fail += ref_st_int(back->r.crange, REF_ST_CRANGE, "crange canary");
      fail += ref_st_int(back->r.crange_start, REF_ST_CRANGE_START,
                         "crange_start canary");
      fail += ref_st_int(back->r.crange_end, REF_ST_CRANGE_END,
                         "crange_end canary");
      if (back->r.headers == NULL ||
          strcmp(back->r.headers, REF_ST_HEADERS) != 0) {
        fprintf(stderr, "%s: the clip lost the reader's place in the stream\n",
                selftest_tag);
        fail++;
      }
      back_clear_entry(back);
      freet(back);
    }
  }

  freet(ref_golden);
  UNLINK(path);
  return fail;
}
