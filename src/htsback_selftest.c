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
/* File: htsback_selftest.c subroutines:                        */
/*       self-tests for the backlog and its worker threads      */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include <limits.h> /* INT_MAX, the backing count back_new() must refuse */

#include "htsselftest_int.h"
#include "htsstats.h"

/* hts_mirror_completed() answers HTS_DEFAULT until a mirror has run, so a GUI
   whose hts_main2() bailed out early does not read that as an abort. The
   verdict is live state, so copy_htsopt() must not carry it between opts. */
static int st_mirrorcompleted(httrackp *opt, int argc, char **argv) {
  httrackp *from = hts_create_opt();
  httrackp *to = hts_create_opt();
  int err = 0;

  ST_FAIL_IF(err, hts_mirror_completed(from) != HTS_DEFAULT);
  ST_FAIL_IF(err, hts_mirror_completed(to) != HTS_DEFAULT);

  /* A verdict never travels onto an opt that has none. */
  from->mirror_completed = HTS_TRUE;
  to->mirror_completed = HTS_DEFAULT;
  copy_htsopt(from, to);
  ST_FAIL_IF(err, hts_mirror_completed(to) != HTS_DEFAULT);
  /* A verdict never overwrites one the target earned itself, which is what a
     copy_htsopt change would silently break. */
  from->mirror_completed = HTS_FALSE;
  to->mirror_completed = HTS_TRUE;
  copy_htsopt(from, to);
  ST_FAIL_IF(err, hts_mirror_completed(to) != HTS_TRUE);

  hts_free_opt(from);
  hts_free_opt(to);
  return st_verdict("mirror-completed", err);
}

/* stat_errors reads the same for a 404 as for a timeout. A front end deciding
   whether a resume is owed needs the failed transfers apart, and per mirror, so
   copy_htsopt() must not carry the count between opts. */
static int st_transportfailures(httrackp *opt, int argc, char **argv) {
  httrackp *from = hts_create_opt();
  httrackp *to = hts_create_opt();
  const hts_stat_struct *stats;
  int err = 0;

  ST_FAIL_IF(err, from->transport_failures != 0 || to->transport_failures != 0);

  /* Published beside stat_errors, which is where a front end reads it. */
  to->transport_failures = 7;
  stats = hts_get_stats(to);
  ST_FAIL_IF(err, stats == NULL || stats->stat_transport_failures != 7);

  /* A count never travels onto another opt. */
  from->transport_failures = 3;
  to->transport_failures = 0;
  copy_htsopt(from, to);
  ST_FAIL_IF(err, to->transport_failures != 0);

  hts_free_opt(from);
  hts_free_opt(to);
  return st_verdict("transport-failures", err);
}

static int st_pause(httrackp *opt, int argc, char **argv) {
  int err = 0, i, seen_low = 0, seen_high = 0;

  /* Consecutive-ms seeds (production shape: launch timestamps a few ms apart)
     must stay in range and spread, not collapse to a bound -- worst case for a
     weak low-bit mixer. */
  for (i = 0; i < 10000; i++) {
    int t = hts_pause_target_ms((TStamp) (1719500000000LL + i), 5000, 10000);

    ST_FAIL_IF(err, t < 5000 || t > 10000);
    seen_low |= (t < 6000);
    seen_high |= (t > 9000);
  }
  ST_FAIL_IF(err, !seen_low || !seen_high);
  if (hts_pause_target_ms(12345, 8000, 8000) != 8000) /* equal bounds = fixed */
    err = 1;
  /* deterministic: a seed yields the same target even after an intervening call
     with another seed (no global PRNG state to perturb it) */
  {
    int a = hts_pause_target_ms(99, 5000, 10000);

    (void) hts_pause_target_ms(54321, 5000, 10000);
    ST_FAIL_IF(err, hts_pause_target_ms(99, 5000, 10000) != a);
  }
  return st_verdict("pause", err);
}

/* #747: a thread is outstanding from the moment hts_newthread() returns, not
   from the moment it starts running, or a wait right after the spawn joins
   nothing. One thread per round is what makes the old bug visible: the wait
   had to find the counter at zero, and a batch of spawns gives the earlier
   threads time to raise it. Unfixed, one round in two caught it, so the round
   count is what turns that into a reliable failure. */
#define THREADWAIT_N 8
#define THREADWAIT_ROUNDS 16
#define THREADWAIT_SLEEP_MS 50
#define THREADWAIT_GATE_MS 10000

static htsmutex threadwait_lock = HTSMUTEX_INIT;
static int threadwait_done = 0;
static hts_boolean threadwait_gated = HTS_FALSE;

static int threadwait_count(void) {
  int n;

  hts_mutexlock(&threadwait_lock);
  n = threadwait_done;
  hts_mutexrelease(&threadwait_lock);
  return n;
}

static void threadwait_thread(void *arg) {
  (void) arg;
  Sleep(THREADWAIT_SLEEP_MS);
  hts_mutexlock(&threadwait_lock);
  threadwait_done++;
  hts_mutexrelease(&threadwait_lock);
}

/* Stays outstanding until the gate clears, so wait_n() can be asked to leave a
   known number of live threads behind. Bounded: a wait_n() that wrongly drains
   them would otherwise never return, and hang the suite instead of failing. */
static void threadwait_gated_thread(void *arg) {
  int waited;

  (void) arg;
  for (waited = 0; waited < THREADWAIT_GATE_MS; waited += 10) {
    hts_boolean gated;

    hts_mutexlock(&threadwait_lock);
    gated = threadwait_gated;
    hts_mutexrelease(&threadwait_lock);
    if (!gated)
      break;
    Sleep(10);
  }
  hts_mutexlock(&threadwait_lock);
  threadwait_done++;
  hts_mutexrelease(&threadwait_lock);
}

static void st_statsrows_slot(lien_back *b, int status, LLint size,
                              LLint totalsize, const char *fil) {
  b->status = status;
  b->r.size = size;
  b->r.totalsize = totalsize;
  b->r.statuscode = HTTP_OK;
  strcpybuff(b->url_adr, "http://example.com");
  strcpybuff(b->url_fil, fil);
}

/* stats_collect_rows() survives an empty table and maps each slot to its row.
 */
static int st_statsrows(httrackp *opt, int argc, char **argv) {
  struct_back *sback = back_new(opt, 4);
  t_StatsBuffer rows[NStatsBuffer];
  lien_back *b;
  int i, err = 0;

  if (sback == NULL) {
    printf("statsrows: SKIP (no slot table)\n");
    return 77;
  }
  b = sback->lnk;
  for (i = 0; i < NStatsBuffer; i++)
    strcpybuff(rows[i].state, "stale");

  /* no backing table: no row, and no division by zero */
  ST_FAIL_IF(err,
             stats_collect_rows(opt, b, 0, 0, rows, NStatsBuffer, 40) != 0);
  ST_FAIL_IF(err, rows[0].state[0] != '\0');
  ST_FAIL_IF(err, stats_collect_rows(opt, b, 4, 0, rows, 0, 40) != 0);

  st_statsrows_slot(&b[0], STATUS_WAIT_HEADERS, 0, -1, "/a/b.html");
  st_statsrows_slot(&b[1], 50, 0, 0, "/empty");
  st_statsrows_slot(&b[2], STATUS_READY, 3, 3, "/done");
  st_statsrows_slot(&b[3], 50, 10000, -1, "/unknown");
  ST_FAIL_IF(err,
             stats_collect_rows(opt, b, 4, 0, rows, NStatsBuffer, 40) != 4);
  /* the current slot first, then transfers, then finished slots */
  ST_FAIL_IF(err, strcmp(rows[0].state, "request") != 0 || rows[0].back != 0);
  ST_FAIL_IF(err, strcmp(rows[0].name, "http://example.com/a") != 0);
  ST_FAIL_IF(err, strcmp(rows[0].file, "/b.html") != 0);
  ST_FAIL_IF(err, strcmp(rows[1].state, "receive") != 0 || rows[1].back != 1);
  /* a known size of 0 is shown as 0, not as an 8 KB placeholder */
  ST_FAIL_IF(err, rows[1].sizetot != 0 || rows[1].size != 0);
  ST_FAIL_IF(err, rows[2].back != 3 || rows[2].sizetot != 8192);
  ST_FAIL_IF(err, rows[2].size != 10000 % 8192);
  ST_FAIL_IF(err, strcmp(rows[3].state, "ready") != 0 || rows[3].back != 2);
  ST_FAIL_IF(err, rows[4].state[0] != '\0');

  /* a URL as wide as both fields is clipped and shortened, never aborted */
  memset(b[0].url_adr, 'h', sizeof(b[0].url_adr) - 1);
  b[0].url_adr[sizeof(b[0].url_adr) - 1] = '\0';
  memset(b[0].url_fil, 'f', sizeof(b[0].url_fil) - 1);
  b[0].url_fil[sizeof(b[0].url_fil) - 1] = '\0';
  ST_FAIL_IF(err, stats_collect_rows(opt, b, 4, 0, rows, 1, 40) != 1);
  ST_FAIL_IF(err, strlen(rows[0].name) >= 40);

  /* every label, from a rotated start, with the login and a secret hidden */
  for (i = 0; i < sback->count; i++)
    b[i].status = STATUS_FREE;
  back_free(&sback);
  sback = back_new(opt, 8);
  if (sback == NULL)
    return 1;
  b = sback->lnk;
  hts_secret_add(opt, "SECRETTOKEN1", 12, HTS_SECRET_HEADER);
  st_statsrows_slot(&b[0], STATUS_WAIT_DNS, 0, -1, "/dns");
  st_statsrows_slot(&b[1], STATUS_CONNECTING, 0, -1, "/conn");
  st_statsrows_slot(&b[2], STATUS_FTP_TRANSFER, 0, -1, "/pub/f");
  strcpybuff(b[2].url_adr, "FTP://up.example");
  strcpybuff(b[2].info, "RETR f");
  st_statsrows_slot(&b[3], STATUS_READY, 0, 0, "/missing");
  b[3].r.statuscode = 404;
  st_statsrows_slot(&b[4], STATUS_READY, 0, 0, "/odd");
  b[4].r.statuscode = 700;
  st_statsrows_slot(&b[5], STATUS_READY, 5, -1, "/short");
  st_statsrows_slot(&b[6], 50, 0, -1, "/in");
  strcpybuff(b[6].url_adr, "http://user:pw@example.com");
  st_statsrows_slot(&b[7], 98, 0, -1, "/q?t=SECRETTOKEN1");
  ST_FAIL_IF(err,
             stats_collect_rows(opt, b, 8, 2, rows, NStatsBuffer, 40) != 8);
  ST_FAIL_IF(err,
             strcmp(rows[0].state, "FTP: RETR f") != 0 || rows[0].back != 2);
  ST_FAIL_IF(err, strcmp(rows[1].state, "receive") != 0 || rows[1].back != 6);
  ST_FAIL_IF(err, strcmp(rows[1].name, "http://example.com") != 0);
  ST_FAIL_IF(err, strcmp(rows[2].state, "receive") != 0 || rows[2].back != 7);
  ST_FAIL_IF(err, strstr(rows[2].file, "SECRETTOKEN1") != NULL);
  ST_FAIL_IF(err, strcmp(rows[3].state, "search") != 0 || rows[3].back != 0);
  ST_FAIL_IF(err, strcmp(rows[4].state, "connect") != 0 || rows[4].back != 1);
  {
    char notfound[256];

    notfound[0] = '\0';
    infostatuscode(notfound, 404);
    ST_FAIL_IF(err, strcmp(rows[5].state, notfound) != 0 || rows[5].back != 3);
  }
  ST_FAIL_IF(err, strcmp(rows[6].state, "error") != 0 || rows[6].back != 4);
  ST_FAIL_IF(err, strcmp(rows[7].state, "ready") != 0 || rows[7].back != 5);
  /* a finished slot of unknown size shows what it got */
  ST_FAIL_IF(err, rows[7].sizetot != 5 || rows[7].size != 5);

  for (i = 0; i < sback->count; i++)
    b[i].status = STATUS_FREE;
  back_free(&sback);
  if (!err)
    printf("statsrows: OK\n");
  return err;
}

/* back_slot_fail() clips a reason too long for r.msg, where a copy aborted;
   back_slot_fail_nonfatal() closes and keeps a reason already set. */
static int st_slotfail(httrackp *opt, int argc, char **argv) {
  struct_back *sback = back_new(opt, 1);
  htsblk *r;
  char longmsg[sizeof(r->msg) + 1];
  int err = 0;

  if (sback == NULL) {
    printf("slotfail: SKIP (no slot table)\n");
    return 77;
  }
  r = &sback->lnk[0].r;
  r->soc = INVALID_SOCKET;
  /* poisoned, so a stray NUL past msg shows */
  memset(r->contenttype, '#', sizeof(r->contenttype));

  /* one byte past capacity: the copy keeps all but the last 'x' */
  memset(longmsg, 'x', sizeof(longmsg) - 1);
  longmsg[sizeof(longmsg) - 1] = '\0';
  sback->lnk[0].status = STATUS_CONNECTING;
  back_slot_fail(opt, sback, 0, STATUSCODE_TIMEOUT, longmsg);
  ST_FAIL_IF(err, r->statuscode != STATUSCODE_TIMEOUT);
  ST_FAIL_IF(err, sback->lnk[0].status != STATUS_READY);
  ST_FAIL_IF(err, strlen(r->msg) != sizeof(r->msg) - 1);
  ST_FAIL_IF(err, strncmp(r->msg, longmsg, sizeof(r->msg) - 1) != 0);
  ST_FAIL_IF(err, r->contenttype[0] != '#');

  /* NULL keeps the reason already set */
  strcpybuff(r->msg, "kept");
  sback->lnk[0].status = STATUS_CONNECTING;
  back_slot_fail(opt, sback, 0, STATUSCODE_NON_FATAL, NULL);
  ST_FAIL_IF(err, r->statuscode != STATUSCODE_NON_FATAL);
  ST_FAIL_IF(err, sback->lnk[0].status != STATUS_READY);
  ST_FAIL_IF(err, strcmp(r->msg, "kept") != 0);

  /* the nonfatal close keeps a set reason, else takes the fallback */
  r->soc = LOCAL_SOCKET_ID;
  sback->lnk[0].status = STATUS_CONNECTING;
  back_slot_fail_nonfatal(opt, sback, 0, "fallback");
  ST_FAIL_IF(err, r->soc != INVALID_SOCKET);
  ST_FAIL_IF(err, r->statuscode != STATUSCODE_NON_FATAL);
  ST_FAIL_IF(err, sback->lnk[0].status != STATUS_READY);
  ST_FAIL_IF(err, strcmp(r->msg, "kept") != 0);
  r->msg[0] = '\0';
  r->soc = LOCAL_SOCKET_ID;
  sback->lnk[0].status = STATUS_CONNECTING;
  back_slot_fail_nonfatal(opt, sback, 0, "fallback");
  ST_FAIL_IF(err, r->soc != INVALID_SOCKET);
  ST_FAIL_IF(err, strcmp(r->msg, "fallback") != 0);

  back_free(&sback);
  if (!err)
    printf("slotfail: OK\n");
  return err;
}

static int st_backswap(httrackp *opt, int argc, char **argv) {
  lien_back back;
  int err = 0;

#define CHECK(want, why)                                                       \
  do {                                                                         \
    if (slot_can_be_cached_on_disk(&back) != (want)) {                         \
      fprintf(stderr, "backswap: expected %d for %s\n", (want), (why));        \
      err = 1;                                                                 \
    }                                                                          \
  } while (0)

  memset(&back, 0, sizeof(back));
  back.status = STATUS_READY;
  strcpybuff(back.url_sav, "/tmp/httrack-selftest.bin");
  CHECK(1, "a plain ready slot");

  back.tmpfile = back.tmpfile_buffer;
  strcpybuff(back.tmpfile_buffer, "/tmp/httrack-selftest.bin.bak");
  CHECK(0, "a slot still holding a re-fetch backup");

  /* Callers clear a spent temporary by emptying the name, not the pointer. */
  back.tmpfile_buffer[0] = '\0';
  CHECK(1, "a slot whose temporary was already dropped");

  back.tmpfile = NULL;
  back.locked = 1;
  CHECK(0, "a locked slot");
  back.locked = BACK_PINNED;
  CHECK(0, "a pinned slot");
  back.locked = 0;

  back.status = STATUS_TRANSFER;
  CHECK(0, "a slot still transferring");
  back.status = STATUS_READY;

  back.url_sav[0] = '\0';
  CHECK(0, "a slot with no save name");

  strcpybuff(back.url_sav, BACK_ADD_TEST);
  CHECK(0, "the dummy test slot");
#undef CHECK

  /* The swap round-trip must not lose the size of a slot whose body is already
     at url_sav, or the link writer blanks the file (#797). */
  {
    static const char body[] = "swapped body";
    int c;

    for (c = 0; c < 2; c++) {
      const hts_boolean inmemory = c == 0 ? HTS_TRUE : HTS_FALSE;
      FILE *const fp = tmpfile();
      lien_back *copy = NULL;

      memset(&back, 0, sizeof(back));
      back.status = STATUS_READY;
      strcpybuff(back.url_sav, "/tmp/httrack-selftest.bin");
      back.r.size = (LLint) sizeof(body) - 1;
      if (inmemory) {
        back.r.adr = strdupt(body);
      }
      if (fp == NULL || back_serialize(fp, &back) != 0 ||
          fseek(fp, 0, SEEK_SET) != 0 || back_unserialize(fp, &copy) != 0) {
        fprintf(stderr, "backswap: round-trip failed for a %s slot\n",
                inmemory ? "buffered" : "direct-to-disk");
        err = 1;
      } else {
        if (copy->r.size != back.r.size) {
          fprintf(stderr,
                  "backswap: %s slot came back with size " LLintP
                  ", expected " LLintP "\n",
                  inmemory ? "buffered" : "direct-to-disk", copy->r.size,
                  back.r.size);
          err = 1;
        }
        if (inmemory && (copy->r.adr == NULL ||
                         memcmp(copy->r.adr, body, sizeof(body) - 1) != 0)) {
          fprintf(stderr, "backswap: buffered slot lost its body\n");
          err = 1;
        }
        if (!inmemory && copy->r.adr != NULL) {
          fprintf(stderr, "backswap: direct-to-disk slot gained a body\n");
          err = 1;
        }
        back_clear_entry(copy);
        freet(copy);
      }
      if (fp != NULL)
        fclose(fp);
      freet(back.r.adr);
    }
  }

  /* Each buffer travels through a void *, so a truncated file must still come
     back as a NULL slot with nothing leaked. */
  {
    static const char body[] = "truncated body";
    static const char hdrs[] = "HTTP/1.1 200 OK\r\nX: 1\r\n";
    char *whole = NULL;
    long whole_len = 0;
    FILE *fp = tmpfile();

    memset(&back, 0, sizeof(back));
    back.status = STATUS_READY;
    strcpybuff(back.url_sav, "/tmp/httrack-selftest.bin");
    back.r.size = (LLint) sizeof(body) - 1;
    back.r.adr = strdupt(body);
    back.r.headers = strdupt(hdrs);
    if (fp == NULL || back_serialize(fp, &back) != 0 ||
        (whole_len = ftell(fp)) <= 0 || fseek(fp, 0, SEEK_SET) != 0) {
      fprintf(stderr, "backswap: could not serialize a slot with headers\n");
      err = 1;
    } else {
      whole = malloct((size_t) whole_len);
      if (whole == NULL ||
          fread(whole, 1, (size_t) whole_len, fp) != (size_t) whole_len) {
        fprintf(stderr, "backswap: could not read the serialized slot back\n");
        err = 1;
        whole_len = 0;
      }
    }
    if (fp != NULL)
      fclose(fp);

    if (whole_len > 0) {
      long cut;

      /* every cut, since each field of the record is a boundary */
      for (cut = 1; cut <= whole_len; cut++) {
        const hts_boolean complete = cut == whole_len;
        lien_back *copy = NULL;
        FILE *cfp;

        cfp = tmpfile();
        if (cfp == NULL ||
            fwrite(whole, 1, (size_t) cut, cfp) != (size_t) cut ||
            fseek(cfp, 0, SEEK_SET) != 0) {
          fprintf(stderr, "backswap: could not stage a %ld-byte slot\n", cut);
          err = 1;
        } else if (back_unserialize(cfp, &copy) == 0) {
          if (!complete) {
            fprintf(stderr, "backswap: %ld of %ld bytes unserialized anyway\n",
                    cut, whole_len);
            err = 1;
          } else if (copy->r.headers == NULL ||
                     strcmp(copy->r.headers, hdrs) != 0) {
            fprintf(stderr,
                    "backswap: the slot came back without its headers\n");
            err = 1;
          }
          back_clear_entry(copy);
          freet(copy);
        } else {
          if (complete) {
            fprintf(stderr, "backswap: the whole slot failed to unserialize\n");
            err = 1;
          }
          if (copy != NULL) {
            fprintf(stderr, "backswap: a failed unserialize kept a slot\n");
            err = 1;
          }
        }
        if (cfp != NULL)
          fclose(cfp);
      }
    }
    if (whole != NULL)
      freet(whole);
    freet(back.r.adr);
    freet(back.r.headers);
  }

  /* A ready table is a file, so its size headers are hostile input. */
  {
    /* SIZE_MAX wrapped the guard byte's allocation to zero, and 16 is a
       well-formed header of the wrong struct size, whose slot must be freed.
       A merely huge size is left out, since ASan aborts on it. */
    const size_t bad[] = {(size_t) -1, 16};
    size_t c;

    for (c = 0; c < sizeof(bad) / sizeof(bad[0]); c++) {
      FILE *const cfp = tmpfile();
      lien_back *copy = NULL;
      char pad[16];

      memset(pad, 0, sizeof(pad));
      if (cfp == NULL || fwrite(&bad[c], sizeof(bad[c]), 1, cfp) != 1 ||
          (bad[c] == sizeof(pad) &&
           fwrite(pad, 1, sizeof(pad), cfp) != sizeof(pad)) ||
          fseek(cfp, 0, SEEK_SET) != 0) {
        fprintf(stderr, "backswap: could not stage a bad size header\n");
        err = 1;
      } else if (back_unserialize(cfp, &copy) == 0) {
        fprintf(stderr, "backswap: a %lu-byte size header unserialized\n",
                (unsigned long) bad[c]);
        err = 1;
        back_clear_entry(copy);
        freet(copy);
      } else if (copy != NULL) {
        fprintf(stderr, "backswap: a rejected size header kept a slot\n");
        err = 1;
      }
      if (cfp != NULL)
        fclose(cfp);
    }
  }

  printf("backswap self-test: %s\n", err ? "FAIL" : "OK");
  return err;
}

/* Serialize a slot into a fresh buffer the caller frees, or NULL. */
static char *st_spool_record(const lien_back *back, long *len) {
  FILE *const fp = tmpfile();
  char *buf = NULL;

  *len = 0;
  if (fp != NULL && back_serialize(fp, back) == 0 && (*len = ftell(fp)) > 0 &&
      fseek(fp, 0, SEEK_SET) == 0 && (buf = malloct((size_t) *len)) != NULL &&
      fread(buf, 1, (size_t) *len, fp) != (size_t) *len) {
    freet(buf);
  }
  if (fp != NULL)
    fclose(fp);
  return buf;
}

static lien_back *st_spool_read(const char *buf, size_t len) {
  FILE *const fp = tmpfile();
  lien_back *copy = NULL;

  if (fp != NULL && fwrite(buf, 1, len, fp) == len &&
      fseek(fp, 0, SEEK_SET) == 0 && back_unserialize(fp, &copy) != 0 &&
      copy != NULL) {
    fprintf(stderr, "spoolrecord: a refused record kept a slot\n");
    copy = NULL; /* leaked, but the test fails anyway */
  }
  if (fp != NULL)
    fclose(fp);
  return copy;
}

/* Fill a string field with a run of one letter, len bytes long. */
#define ST_FILL(field, len, ch)                                                \
  do {                                                                         \
    memset((field), (ch), (len));                                              \
    (field)[(len)] = '\0';                                                     \
  } while (0)

/* The spool record carries only the used part of each string field, and gives
   back the slot byte for byte. */
static int st_spoolrecord(httrackp *opt, int argc, char **argv) {
  static const char body[] = "spooled body";
  static const char hdrs[] = "HTTP/1.1 200 OK\r\n";
  lien_back *back = calloct(1, sizeof(lien_back));
  lien_back *copy;
  char *rec = NULL;
  long len = 0;
  int err = 0;

  (void) opt;
  (void) argc;
  (void) argv;
  if (back == NULL)
    return 1;

  /* A small slot must spool far under the struct size. */
  strcpybuff(back->url_adr, "example.com");
  strcpybuff(back->url_fil, "/a.html");
  strcpybuff(back->url_sav, "/tmp/mirror/a.html");
  back->r.adr = strdupt(body);
  back->r.size = (LLint) sizeof(body) - 1;
  rec = st_spool_record(back, &len);
  if (rec == NULL) {
    fprintf(stderr, "spoolrecord: a small slot did not serialize\n");
    err = 1;
  } else if ((size_t) len >= sizeof(lien_back) / 8) {
    fprintf(stderr, "spoolrecord: a small slot took %ld bytes (struct: %lu)\n",
            len, (unsigned long) sizeof(lien_back));
    err = 1;
  }
  freet(rec);
  /* One more byte in any string field must cost exactly one byte. */
  {
#define ST_FIELD(f) {back->f, sizeof(back->f), #f}

    const struct {
      char *field;
      size_t size;
      const char *name;
    } fields[] = {ST_FIELD(url_adr),        ST_FIELD(url_fil),
                  ST_FIELD(url_sav),        ST_FIELD(referer_adr),
                  ST_FIELD(referer_fil),    ST_FIELD(location_buffer),
                  ST_FIELD(tmpfile_buffer), ST_FIELD(send_too),
                  ST_FIELD(r.msg),          ST_FIELD(r.contenttype),
                  ST_FIELD(r.charset),      ST_FIELD(r.contentencoding),
                  ST_FIELD(r.lastmodified), ST_FIELD(r.etag),
                  ST_FIELD(r.cdispo),       ST_FIELD(info)};

#undef ST_FIELD
    size_t i;

    for (i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
      long shorter = 0, longer = 0;
      char saved[64];

      strlcpybuff(saved, fields[i].field, sizeof(saved));
      strlcpybuff(fields[i].field, "x", fields[i].size);
      freet(rec);
      rec = st_spool_record(back, &shorter);
      strlcpybuff(fields[i].field, "xx", fields[i].size);
      freet(rec);
      rec = st_spool_record(back, &longer);
      strlcpybuff(fields[i].field, saved, fields[i].size);
      if (shorter == 0 || longer != shorter + 1) {
        fprintf(stderr, "spoolrecord: %s is not spooled by its length\n",
                fields[i].name);
        err = 1;
      }
    }
    freet(rec);
  }
  freet(back->r.adr);

  /* With every string field full, the copy must match exactly. Bytes past a
     terminator come back zero, so the slot starts zeroed. */
  memset(back, 0, sizeof(lien_back));
  back->r.adr = strdupt(body);
  back->r.size = (LLint) sizeof(body) - 1;
  ST_FILL(back->url_adr, sizeof(back->url_adr) - 1, 'a');
  ST_FILL(back->url_fil, 1, 'b');
  ST_FILL(back->url_sav, sizeof(back->url_sav) - 1, 'c');
  ST_FILL(back->referer_adr, 7, 'd');
  ST_FILL(back->referer_fil, sizeof(back->referer_fil) - 1, 'e');
  ST_FILL(back->location_buffer, sizeof(back->location_buffer) - 1, 'f');
  ST_FILL(back->tmpfile_buffer, 3, 'g');
  ST_FILL(back->send_too, sizeof(back->send_too) - 1, 'h');
  ST_FILL(back->r.msg, sizeof(back->r.msg) - 1, 'i');
  ST_FILL(back->r.contenttype, 9, 'j');
  ST_FILL(back->r.charset, sizeof(back->r.charset) - 1, 'k');
  ST_FILL(back->r.contentencoding, 4, 'l');
  ST_FILL(back->r.lastmodified, sizeof(back->r.lastmodified) - 1, 'm');
  ST_FILL(back->r.etag, sizeof(back->r.etag) - 1, 'n');
  ST_FILL(back->r.cdispo, 5, 'o');
  ST_FILL(back->info, sizeof(back->info) - 1, 'p');
  back->status = STATUS_READY;
  back->locked = 2;
  back->timeout = 31;
  back->maxfile_html = 1234567;
  back->r.statuscode = 206;
  back->r.notmodified = 1;
  back->r.totalsize = 98765;
  back->r.crange_end = 4321;
  back->r.retry_after = 17;
  memset(&back->r.req, 0x5a, sizeof(back->r.req)); /* every byte must reset */
  back->http11 = 1;
  back->chunk_blocksize = 77;
  back->early_add = 1;
  back->r.headers = strdupt(hdrs);
  rec = st_spool_record(back, &len);
  copy = rec != NULL ? st_spool_read(rec, (size_t) len) : NULL;
  if (copy == NULL) {
    fprintf(stderr, "spoolrecord: a full slot did not round-trip\n");
    err = 1;
  } else {
    char *adr = copy->r.adr;
    char *headers = copy->r.headers;

    if (adr == NULL || copy->r.size != back->r.size ||
        memcmp(adr, body, sizeof(body) - 1) != 0 || headers == NULL ||
        strcmp(headers, hdrs) != 0) {
      fprintf(stderr, "spoolrecord: the body or headers came back wrong\n");
      err = 1;
    }
    if (copy->r.location != copy->location_buffer ||
        copy->r.soc != INVALID_SOCKET) {
      fprintf(stderr, "spoolrecord: the in-process pointers were not reset\n");
      err = 1;
    }
    {
      htsrequest zero;

      memset(&zero, 0, sizeof(zero));
      if (memcmp(&copy->r.req, &zero, sizeof(zero)) != 0) {
        fprintf(stderr, "spoolrecord: the request block was not reset\n");
        err = 1;
      }
    }
    /* compare the rest byte for byte, the reset pointers aside */
    {
      char *const want_adr = back->r.adr;
      char *const want_headers = back->r.headers;
      htsrequest want_req;

      memcpy(&want_req, &back->r.req, sizeof(want_req));

      back->r.adr = copy->r.adr = NULL;
      back->r.headers = copy->r.headers = NULL;
      back->r.location = copy->r.location = NULL;
      back->r.soc = copy->r.soc;
      memcpy(&back->r.req, &copy->r.req, sizeof(back->r.req));
      if (memcmp(back, copy, sizeof(lien_back)) != 0) {
        fprintf(stderr, "spoolrecord: the slot came back changed\n");
        err = 1;
      }
      back->r.adr = want_adr;
      back->r.headers = want_headers;
      memcpy(&back->r.req, &want_req, sizeof(want_req));
    }
    freet(adr);
    freet(headers);
    freet(copy);
  }

  /* Forged first-field lengths: its room, one past it, and a wrapping one. */
  {
    const size_t room = sizeof(back->url_adr);
    const size_t head = 2 * sizeof(size_t);
    const size_t bad[] = {room, room + 1, (size_t) -1};
    size_t c;

    for (c = 0; rec != NULL && c < sizeof(bad) / sizeof(bad[0]); c++) {
      const size_t claim = bad[c] <= room + 1 ? bad[c] : 0;
      const size_t rest = (size_t) len - head - (room - 1);
      const size_t size = head + claim + rest;
      char *forged = malloct(size);

      if (forged == NULL) {
        err = 1;
        continue;
      }
      memcpy(forged, rec, sizeof(size_t));
      memcpy(forged + sizeof(size_t), &bad[c], sizeof(size_t));
      memset(forged + head, 'a', claim);
      memcpy(forged + head + claim, rec + head + (room - 1), rest);
      copy = st_spool_read(forged, size);
      if (copy != NULL) {
        fprintf(stderr, "spoolrecord: a %lu-byte field length was read\n",
                (unsigned long) bad[c]);
        err = 1;
        back_clear_entry(copy);
        freet(copy);
      }
      freet(forged);
    }
    /* a struct size of another build, the record otherwise intact */
    if (rec != NULL) {
      const size_t other = sizeof(lien_back) - 1;
      const size_t ours = sizeof(lien_back);

      memcpy(rec, &other, sizeof(other));
      copy = st_spool_read(rec, (size_t) len);
      if (copy != NULL) {
        fprintf(stderr, "spoolrecord: a foreign struct size was read\n");
        err = 1;
        back_clear_entry(copy);
        freet(copy);
      }
      memcpy(rec, &ours, sizeof(ours));
    }
    /* the control: the record as written, at the longest legal length */
    if (rec != NULL) {
      copy = st_spool_read(rec, (size_t) len);
      if (copy == NULL) {
        fprintf(stderr, "spoolrecord: the unforged record was refused\n");
        err = 1;
      } else {
        back_clear_entry(copy);
        freet(copy);
      }
    }
  }
  freet(rec);

  /* An unterminated field is refused on write, not read past. */
  memset(back->r.etag, 'z', sizeof(back->r.etag));
  {
    FILE *const fp = tmpfile();

    if (fp == NULL || back_serialize(fp, back) == 0 || ftell(fp) != 0) {
      fprintf(stderr, "spoolrecord: an unterminated field was serialized\n");
      err = 1;
    }
    if (fp != NULL)
      fclose(fp);
  }

  freet(back->r.adr);
  freet(back->r.headers);
  freet(back);
  printf("spoolrecord self-test: %s\n", err ? "FAIL" : "OK");
  return err;
}

#undef ST_FILL

typedef enum { ST_CAP_FAIL, ST_CAP_OK, ST_CAP_SKIPPED } st_cap_result;

/* Fail one spool write at a 4 KB file size cap, and check it left nothing. */
static st_cap_result st_spool_capped_write(httrackp *opt,
                                           const lien_back *back) {
#ifdef _WIN32
  (void) back;
  return ST_CAP_SKIPPED;
#else
  char name[HTS_URLMAXSIZE * 2];
  struct rlimit saved, tight;
  void (*oldxfsz)(int);
  hts_boolean written;

  if (!back_spoolname(opt, name, sizeof(name))) {
    fprintf(stderr, "spoolwrite: naming failed\n");
    return ST_CAP_FAIL;
  }
  if (getrlimit(RLIMIT_FSIZE, &saved) != 0)
    return ST_CAP_SKIPPED;
  tight = saved;
  tight.rlim_cur = 4096;
  /* a capped write then fails with EFBIG instead of killing us */
  oldxfsz = signal(SIGXFSZ, SIG_IGN);
  if (setrlimit(RLIMIT_FSIZE, &tight) != 0) {
    signal(SIGXFSZ, oldxfsz);
    return ST_CAP_SKIPPED;
  }
  written = back_spool_write(opt, name, back);
  (void) setrlimit(RLIMIT_FSIZE, &saved);
  signal(SIGXFSZ, oldxfsz);
  if (written) { /* the cap did not bite */
    (void) UNLINK(name);
    back_tmpdir_drop(name);
    return ST_CAP_SKIPPED;
  }
  if (fexist_utf8(name)) {
    fprintf(stderr, "spoolwrite: failed write left %s\n", name);
    return ST_CAP_FAIL;
  }
  if (dir_exists(name)) { /* checks name's parent directory */
    fprintf(stderr, "spoolwrite: failed write left the directory of %s\n",
            name);
    return ST_CAP_FAIL;
  }
  return ST_CAP_OK;
#endif
}

// -#test=spoolwrite <dir>: a spool write that fails leaves no partial file.
static int st_spoolwrite(httrackp *opt, int argc, char **argv) {
  char base[HTS_URLMAXSIZE];
  char name[HTS_URLMAXSIZE * 2];
  const size_t bodysize = 256 * 1024;
  lien_back back;
  int err = 0;

  snprintf(base, sizeof(base), "%s/", argv[0]);
  StringCopy(opt->path_html_utf8, base);
  memset(&back, 0, sizeof(back));
  back.status = STATUS_READY;
  strcpybuff(back.url_adr, "example.com");
  strcpybuff(back.url_fil, "/spool.html");
  back.r.adr = calloct(1, bodysize);
  back.r.size = (LLint) bodysize;
  if (back.r.adr == NULL) {
    fprintf(stderr, "spoolwrite: no memory\n");
    return 1;
  }

  switch (st_spool_capped_write(opt, &back)) {
  case ST_CAP_FAIL:
    err++;
    break;
  case ST_CAP_OK:
    printf("spoolwrite: failed write cleaned up\n");
    break;
  case ST_CAP_SKIPPED:
    printf("spoolwrite: no file size cap here, failure case skipped\n");
    break;
  }

  /* control: an unhindered write keeps its file */
  if (!back_spoolname(opt, name, sizeof(name)) ||
      !back_spool_write(opt, name, &back) || !fexist_utf8(name)) {
    fprintf(stderr, "spoolwrite: a good write did not keep %s\n", name);
    err++;
  }
  (void) UNLINK(name);
  back_tmpdir_drop(name);
  freet(back.r.adr);
  return st_verdict("spoolwrite", err);
}

/* A finished slot whose page is held in memory, as the spool takes it. */
static hts_boolean st_spooldir_slot(lien_back *back, const char *dir,
                                    const char *name) {
  back->status = STATUS_READY;
  strcpybuff(back->url_adr, "example.com");
  snprintf(back->url_fil, sizeof(back->url_fil), "/%s", name);
  snprintf(back->url_sav, sizeof(back->url_sav), "%s/%s", dir, name);
  back->r.adr = calloct(1, 100);
  back->r.size = 100;
  return back->r.adr != NULL ? HTS_TRUE : HTS_FALSE;
}

/* Spools a.html and b.html, then takes them back in that order. With vanish,
   b's spool file is deleted first, so taking a empties the directory early.
   Returns the number of failed checks. */
static int st_spooldir_round(httrackp *opt, const char *dir,
                             hts_boolean vanish) {
  FILE *const saved_log = opt->log;
  char tmpdir[HTS_URLMAXSIZE * 2];
  char sava[HTS_URLMAXSIZE * 2];
  char savb[HTS_URLMAXSIZE * 2];
  struct_back *sback = back_new(opt, 4);
  cache_back cache;
  void *spooled = NULL;
  int err = 0;
  int i;

#define CHECK(cond) ST_CHECK(err, cond)
  if (sback == NULL)
    return 1;
  sback->ready_mem_max = 0; /* this test is about the disk spool */
  /* dir_exists() tests the directory a name is in */
  snprintf(tmpdir, sizeof(tmpdir), "%s/~hts-tmp/x", dir);
  snprintf(sava, sizeof(sava), "%s/a.html", dir);
  snprintf(savb, sizeof(savb), "%s/b.html", dir);
  memset(&cache, 0, sizeof(cache));
  CHECK(st_spooldir_slot(&sback->lnk[0], dir, "a.html") &&
        st_spooldir_slot(&sback->lnk[1], dir, "b.html"));
  CHECK(back_cleanup_background(opt, &cache, sback) == 2);
  CHECK(dir_exists(tmpdir));
  if (vanish) {
    CHECK(coucal_read_pvoid(sback->ready, savb, &spooled) && spooled != NULL);
    if (back_spooled_file(spooled) != NULL)
      (void) UNLINK(back_spooled_file(spooled));
  }
  CHECK(back_index(opt, sback, "example.com", "/a.html", sava) >= 0);
  CHECK(dir_exists(tmpdir));
  opt->log = NULL; /* so a vanished file logs nothing */
  /* a vanished b is not found, but its lookup still removes the directory */
  CHECK((back_index(opt, sback, "example.com", "/b.html", savb) >= 0) ==
        !vanish);
  opt->log = saved_log;
  CHECK(!dir_exists(tmpdir));

  for (i = 0; i < sback->count; i++)
    back_clear_entry(&sback->lnk[i]);
  back_free(&sback);
#undef CHECK
  return err;
}

// -#test=spooldir <dir>: ~hts-tmp goes with the last spool entry, not before.
static int st_spooldir(httrackp *opt, int argc, char **argv) {
  char base[HTS_URLMAXSIZE];
  int err;

  if (argc < 1) {
    fprintf(stderr, "usage: -#test=spooldir <writable directory>\n");
    return 1;
  }
  snprintf(base, sizeof(base), "%s/", argv[0]);
  StringCopy(opt->path_html_utf8, base);
  err = st_spooldir_round(opt, argv[0], HTS_FALSE);
  err += st_spooldir_round(opt, argv[0], HTS_TRUE);
  return st_verdict("spooldir", err);
}

// -#test=spoolfull <dir>: a spooled entry waits for a free slot, kept whole.
static int st_spoolfull(httrackp *opt, int argc, char **argv) {
  static const char body[] = "spooled page";
  char base[HTS_URLMAXSIZE];
  FILE *const saved_log = opt->log;
  char sav[HTS_URLMAXSIZE * 2];
  char savc[HTS_URLMAXSIZE * 2];
  struct_back *sback;
  cache_back cache;
  void *spooled = NULL;
  void *again = NULL;
  int err = 0;
  int i, b;

#define CHECK(cond) ST_CHECK(err, cond)
  if (argc < 1) {
    fprintf(stderr, "usage: -#test=spoolfull <writable directory>\n");
    return 1;
  }
  snprintf(base, sizeof(base), "%s/", argv[0]);
  StringCopy(opt->path_html_utf8, base);
  snprintf(sav, sizeof(sav), "%s/a.html", argv[0]);
  snprintf(savc, sizeof(savc), "%s/c.html", argv[0]);
  memset(&cache, 0, sizeof(cache));
  if ((sback = back_new(opt, 4)) == NULL)
    return 1;
  sback->ready_mem_max = 0; /* the wait must keep a spool file whole */
  CHECK(st_spooldir_slot(&sback->lnk[0], argv[0], "a.html"));
  if (sback->lnk[0].r.adr != NULL)
    memcpy(sback->lnk[0].r.adr, body, sizeof(body));
  CHECK(back_cleanup_background(opt, &cache, sback) == 1);
  CHECK(coucal_read_pvoid(sback->ready, sav, &spooled) && spooled != NULL);
  /* every slot busy: the wake-up must fail and keep the spool file */
  while ((i = back_search_quick(sback)) != -1)
    sback->lnk[i].status = STATUS_CONNECTING;
  CHECK(back_index(opt, sback, "example.com", "/a.html", sav) < 0);
  CHECK(back_spooled_file(spooled) != NULL &&
        fexist_utf8(back_spooled_file(spooled)));
  /* a duplicate of it must stay in memory rather than replace it */
  back_clear_entry(&sback->lnk[2]);
  CHECK(st_spooldir_slot(&sback->lnk[2], argv[0], "a.html"));
  CHECK(back_cleanup_background(opt, &cache, sback) == 0);
  CHECK(coucal_nitems(sback->ready) == 1 &&
        coucal_read_pvoid(sback->ready, sav, &again) && again == spooled);
  back_clear_entry(&sback->lnk[2]);
  sback->lnk[2].status = STATUS_CONNECTING;
  sback->lnk[1].status = STATUS_FREE;
  b = back_index(opt, sback, "example.com", "/a.html", sav);
  CHECK(b == 1);
  CHECK(b >= 0 && sback->lnk[b].r.adr != NULL && sback->lnk[b].r.size == 100 &&
        strcmp(sback->lnk[b].r.adr, body) == 0);
  /* an unreadable spool file drops its entry, so nothing waits on it */
  back_clear_entry(&sback->lnk[3]);
  CHECK(st_spooldir_slot(&sback->lnk[3], argv[0], "c.html"));
  CHECK(back_cleanup_background(opt, &cache, sback) == 1);
  CHECK(coucal_read_pvoid(sback->ready, savc, &again) && again != NULL);
  if (back_spooled_file(again) != NULL)
    (void) UNLINK(back_spooled_file(again));
  opt->log = NULL; /* the vanished file logs a warning */
  CHECK(back_index(opt, sback, "example.com", "/c.html", savc) < 0);
  opt->log = saved_log;
  CHECK(coucal_nitems(sback->ready) == 0);

  for (i = 0; i < sback->count; i++)
    back_clear_entry(&sback->lnk[i]);
  back_free(&sback);
#undef CHECK
  return st_verdict("spoolfull", err);
}

/* A finished slot sK.html whose 100-byte body is all 'a' + k. */
static hts_boolean st_spoolmem_slot(lien_back *back, const char *dir, int k) {
  char name[32];

  snprintf(name, sizeof(name), "s%d.html", k);
  if (!st_spooldir_slot(back, dir, name))
    return HTS_FALSE;
  memset(back->r.adr, 'a' + k, 100);
  back->r.statuscode = 200;
  memset(&back->r.req, 0x5a, sizeof(back->r.req)); /* must come back zeroed */
  back->r.headers = strdupt("HTTP/1.1 200 OK\r\n");
  return back->r.headers != NULL ? HTS_TRUE : HTS_FALSE;
}

/* Wakes sK.html and checks it came back whole. Returns the failed checks. */
static int st_spoolmem_wake(httrackp *opt, struct_back *sback, const char *dir,
                            int k) {
  char fil[32];
  char sav[HTS_URLMAXSIZE * 2];
  char body[100];
  htsrequest zero;
  int err = 0;
  int q;

#define CHECK(cond) ST_CHECK(err, cond)
  snprintf(fil, sizeof(fil), "/s%d.html", k);
  snprintf(sav, sizeof(sav), "%s%s", dir, fil);
  memset(body, 'a' + k, sizeof(body));
  memset(&zero, 0, sizeof(zero));
  q = back_index(opt, sback, "example.com", fil, sav);
  CHECK(q >= 0);
  if (q >= 0) {
    const lien_back *const b = &sback->lnk[q];

    CHECK(strcmp(b->url_sav, sav) == 0 && b->r.statuscode == 200);
    CHECK(b->r.adr != NULL && b->r.size == 100 &&
          memcmp(b->r.adr, body, sizeof(body)) == 0);
    CHECK(b->r.headers != NULL &&
          strcmp(b->r.headers, "HTTP/1.1 200 OK\r\n") == 0);
    CHECK(memcmp(&b->r.req, &zero, sizeof(zero)) == 0);
    back_clear_entry(&sback->lnk[q]);
  }
#undef CHECK
  return err;
}

/* Spools s0..s2 under a cap of max bytes, of which the first mem must stay in
   memory, then wakes them all. unit is one entry's memory cost. */
static int st_spoolmem_round(httrackp *opt, const char *dir, size_t max,
                             int mem, size_t unit) {
  char tmpdir[HTS_URLMAXSIZE * 2];
  char sav[HTS_URLMAXSIZE * 2];
  struct_back *sback = back_new(opt, 4);
  cache_back cache;
  int err = 0;
  int k;

#define CHECK(cond) ST_CHECK(err, cond)
  if (sback == NULL)
    return 1;
  sback->ready_mem_max = max;
  snprintf(tmpdir, sizeof(tmpdir), "%s/~hts-tmp/x", dir);
  memset(&cache, 0, sizeof(cache));
  for (k = 0; k < 3; k++)
    CHECK(st_spoolmem_slot(&sback->lnk[k], dir, k));
  CHECK(back_cleanup_background(opt, &cache, sback) == 3);
  CHECK(coucal_nitems(sback->ready) == 3);
  CHECK(sback->ready_mem_bytes == (size_t) mem * unit);
  CHECK(sback->ready_mem_bytes <= max);
  CHECK(sback->ready_files == 3 - mem);
  CHECK(dir_exists(tmpdir) == (mem < 3));
  for (k = 0; k < 3; k++) {
    void *spooled = NULL;

    snprintf(sav, sizeof(sav), "%s/s%d.html", dir, k);
    CHECK(coucal_read_pvoid(sback->ready, sav, &spooled) && spooled != NULL);
    CHECK((back_spooled_file(spooled) == NULL) == (k < mem));
    CHECK(k < mem || fexist_utf8(back_spooled_file(spooled)));
  }
  for (k = 0; k < 3; k++)
    err += st_spoolmem_wake(opt, sback, dir, k);
  CHECK(coucal_nitems(sback->ready) == 0);
  CHECK(sback->ready_mem_bytes == 0 && sback->ready_files == 0);
  CHECK(!dir_exists(tmpdir));
  if (mem > 0) {
    /* the room a woken entry left takes the next one */
    for (k = 0; k < 3; k++)
      CHECK(st_spoolmem_slot(&sback->lnk[k], dir, k));
    CHECK(back_cleanup_background(opt, &cache, sback) == 3);
    err += st_spoolmem_wake(opt, sback, dir, 0);
    CHECK(st_spoolmem_slot(&sback->lnk[0], dir, 3));
    CHECK(back_cleanup_background(opt, &cache, sback) == 1);
    CHECK(sback->ready_mem_bytes == (size_t) mem * unit);
    CHECK(sback->ready_files == 3 - mem);
    /* the last file empties ~hts-tmp while entries wait in memory */
    for (k = mem; k < 3; k++)
      err += st_spoolmem_wake(opt, sback, dir, k);
    CHECK(coucal_nitems(sback->ready) == (size_t) mem);
    CHECK(!dir_exists(tmpdir));
  }
  /* the end of the mirror frees both kinds */
  for (k = mem; k < 3; k++)
    CHECK(st_spoolmem_slot(&sback->lnk[k], dir, k));
  CHECK(back_cleanup_background(opt, &cache, sback) == 3 - mem);
  CHECK(sback->ready_files == 3 - mem);
  back_delete_all(opt, &cache, sback);
  CHECK(sback->ready == NULL || coucal_nitems(sback->ready) == 0);
  CHECK(sback->ready_mem_bytes == 0 && sback->ready_files == 0);
  CHECK(!dir_exists(tmpdir));
  for (k = 0; k < sback->count; k++)
    back_clear_entry(&sback->lnk[k]);
  back_free(&sback);
#undef CHECK
  return err;
}

// -#test=spoolmem <dir>: spooled slots stay in memory up to the cap.
static int st_spoolmem(httrackp *opt, int argc, char **argv) {
  char base[HTS_URLMAXSIZE];
  struct_back *sback;
  cache_back cache;
  size_t unit = 0;
  int err = 0;

#define CHECK(cond) ST_CHECK(err, cond)
  if (argc < 1) {
    fprintf(stderr, "usage: -#test=spoolmem <writable directory>\n");
    return 1;
  }
  snprintf(base, sizeof(base), "%s/", argv[0]);
  StringCopy(opt->path_html_utf8, base);
  memset(&cache, 0, sizeof(cache));
  /* one entry's cost, with the default cap */
  if ((sback = back_new(opt, 4)) == NULL)
    return 1;
  CHECK(st_spoolmem_slot(&sback->lnk[0], argv[0], 0));
  CHECK(back_cleanup_background(opt, &cache, sback) == 1);
  unit = sback->ready_mem_bytes;
  CHECK(unit > 100 && sback->ready_files == 0);
  err += st_spoolmem_wake(opt, sback, argv[0], 0);
  back_free(&sback);
  if (unit == 0)
    return st_verdict("spoolmem", err + 1);
  err += st_spoolmem_round(opt, argv[0], (size_t) 64 * 1024 * 1024, 3, unit);
  err += st_spoolmem_round(opt, argv[0], 2 * unit, 2, unit);
  err += st_spoolmem_round(opt, argv[0], 2 * unit - 1, 1, unit);
  err += st_spoolmem_round(opt, argv[0], 0, 0, unit);
#undef CHECK
  return st_verdict("spoolmem", err);
}

static hts_boolean st_spool_plant(const char *path) {
  FILE *const fp = filecreate(NULL, path);

  if (fp == NULL) {
    fprintf(stderr, "spoolsweep: could not plant %s\n", path);
    return HTS_FALSE;
  }
  return fclose(fp) == 0 ? HTS_TRUE : HTS_FALSE;
}

/* As pid 1, init's file is our own, so the sweep rightly removes it. */
static hts_boolean st_spool_keep_applies(const char *name) {
#ifndef _WIN32
  if (getpid() == 1 && strcmp(name, "tmpfile1-0.tmp") == 0)
    return HTS_FALSE;
#endif
  (void) name;
  return HTS_TRUE;
}

// -#test=spoolsweep <dir>: startup removes the spool of a dead process only.
static int st_spoolsweep(httrackp *opt, int argc, char **argv) {
  /* no process has this pid on any platform */
  static const char *const dead = "tmpfile2147483646-3.tmp";
  /* the sweep must keep each of these */
  static const char *const keep[] = {
      "tmpfile2147483646-3.tmpx", /* suffix */
      "tmpfile7.tmp",             /* no pid */
      "tmp2147483646-1.tmp",      /* prefix */
      "tmpfile2147483646-.tmp",   /* no counter */
#if ULONG_MAX == 0xfffffffful
      "tmpfile6442450942-1.tmp", /* would wrap to 2147483646 */
#else
      "tmpfile18446744075857035262-1.tmp", /* would wrap to 2147483646 */
#endif
#ifdef _WIN32
      "tmpfile4-0.tmp", /* System: always running */
#else
      "tmpfile1-0.tmp",          /* init: alive, and EPERM unless root */
      "tmpfile3000000000-1.tmp", /* a negative pid_t */
#endif
  };
  char base[HTS_URLMAXSIZE];
  char path[HTS_URLMAXSIZE * 2];
  char own[HTS_URLMAXSIZE * 2];
  char live[HTS_URLMAXSIZE * 2];
  lien_back back;
  size_t i;
  int err = 0;

  snprintf(base, sizeof(base), "%s/", argv[0]);
  StringCopy(opt->path_html_utf8, base);
  /* a file of our own pid predates this run, so it is stale */
  memset(&back, 0, sizeof(back));
  back.status = STATUS_READY;
  if (!back_spoolname(opt, own, sizeof(own)) ||
      !back_spool_write(opt, own, &back)) {
    fprintf(stderr, "spoolsweep: could not plant %s\n", own);
    return 1;
  }
  live[0] = '\0';
#ifndef _WIN32
  snprintf(live, sizeof(live), "%s~hts-tmp/tmpfile%lu-0.tmp", base,
           (unsigned long) getppid());
  if (!st_spool_plant(live))
    return 1;
#endif
  snprintf(path, sizeof(path), "%s~hts-tmp/%s", base, dead);
  if (!st_spool_plant(path))
    return 1;
  for (i = 0; i < sizeof(keep) / sizeof(keep[0]); i++) {
    if (!st_spool_keep_applies(keep[i]))
      continue;
    snprintf(path, sizeof(path), "%s~hts-tmp/%s", base, keep[i]);
    if (!st_spool_plant(path))
      return 1;
  }

  back_spool_sweep(opt);

  snprintf(path, sizeof(path), "%s~hts-tmp/%s", base, dead);
  if (fexist_utf8(path)) {
    fprintf(stderr, "spoolsweep: dead process's %s survived\n", dead);
    err++;
  }
  if (fexist_utf8(own)) {
    fprintf(stderr, "spoolsweep: earlier run's %s survived\n", own);
    err++;
  }
  if (live[0] != '\0' && !fexist_utf8(live)) {
    fprintf(stderr, "spoolsweep: live parent's %s was removed\n", live);
    err++;
  }
  for (i = 0; i < sizeof(keep) / sizeof(keep[0]); i++) {
    if (!st_spool_keep_applies(keep[i]))
      continue;
    snprintf(path, sizeof(path), "%s~hts-tmp/%s", base, keep[i]);
    if (!fexist_utf8(path)) {
      fprintf(stderr, "spoolsweep: %s was removed\n", keep[i]);
      err++;
    }
    (void) UNLINK(path);
  }
  if (live[0] != '\0')
    (void) UNLINK(live);

  /* the empty directory goes too */
  snprintf(path, sizeof(path), "%s~hts-tmp/%s", base, dead);
  if (!st_spool_plant(path))
    return 1;
  back_spool_sweep(opt);
  if (dir_exists(path)) {
    fprintf(stderr, "spoolsweep: emptied ~hts-tmp was kept\n");
    err++;
  }
  return st_verdict("spoolsweep", err);
}

/* TEST-NET-1: routed nowhere, so a connect to it stays pending. */
#define ST_BACKSTOP_DEADHOST "192.0.2.1"

/* Open a connect that is still in flight, or INVALID_SOCKET where the network
   answers for the blackhole instead of dropping. */
static T_SOC st_backstop_pending(httrackp *opt, htsblk *r) {
  T_SOC soc;

  hts_init_htsblk(r);
  soc = newhttp_addr(opt, ST_BACKSTOP_DEADHOST, r, 80, 0, NULL);
  if (soc == INVALID_SOCKET)
    return INVALID_SOCKET;
  r->soc = soc;
  if (check_socket_connect(soc) != 0) {
    strcpybuff(r->msg, "the network answers for this address");
    deletehttp(r);
    return INVALID_SOCKET;
  }
  return soc;
}

/* Is the descriptor still an open socket? Clearing r.soc proves nothing about
   the connection: an abort that forgets deletehttp() leaks one fd per slot. */
static hts_boolean st_backstop_soc_open(T_SOC soc) {
  int type = 0;
  socklen_t len = (socklen_t) sizeof(type);

  return getsockopt(soc, SOL_SOCKET, SO_TYPE, (char *) &type, &len) == 0
             ? HTS_TRUE
             : HTS_FALSE;
}

static void st_backstop_slot(struct_back *sback, int p, int status,
                             const htsblk *r) {
  lien_back *const back = &sback->lnk[p];

  /* no timeout, so only the stop may end these slots */
  back_slot_attach(sback, p, status, r, ST_BACKSTOP_DEADHOST, "/stalled.html");
  /* poisoned, so the expected outcome cannot be the value the slot came with */
  back->r.statuscode = STATUSCODE_TIMEOUT;
  strcpybuff(back->r.msg, "untouched");
}

/* A network that rejects TEST-NET-1 instead of dropping it answers in ms. */
#define ST_BACKSTOP_SETTLE_MS 500

/* Bytes the size cap allows. back_maxsize_grace() spares running transfers a
   further tenth, so the past-grace case below doubles it. */
#define ST_BACKSTOP_CAP 1000

/* Refill every slot: a fresh pending connect for each state that owns a socket,
   plus the poison the assertions read back. False if the blackhole answered. */
static hts_boolean st_backstop_arm(httrackp *opt, struct_back *sback,
                                   const int *status, htsblk *r, int slots) {
  int i;

  for (i = 0; i < slots; i++) {
    deletehttp(&sback->lnk[i].r); /* whatever the previous round left */
    if (st_backstop_pending(opt, &r[i]) == INVALID_SOCKET) {
      printf("backstop: SKIP (no stalled connect to " ST_BACKSTOP_DEADHOST
             ": %s)\n",
             r[i].msg);
      return HTS_FALSE;
    }
    st_backstop_slot(sback, i, status[i], &r[i]);
  }
  /* The readback above is too early to see a refusal, and back_wait() then
     ends slots the caller asserts are intact (the powerpc buildds). */
  Sleep(ST_BACKSTOP_SETTLE_MS);
  for (i = 0; i < slots; i++) {
    if (check_socket_connect(sback->lnk[i].r.soc) != 0) {
      printf("backstop: SKIP (the network answered for " ST_BACKSTOP_DEADHOST
             " within %d ms)\n",
             ST_BACKSTOP_SETTLE_MS);
      return HTS_FALSE;
    }
  }
  return HTS_TRUE;
}

/* back_delete_all(), the shutdown every exit reaches, on its own slot table. A
   capped mirror can end through the link loop before either sweep runs
   (htscore.c, on back_checkmirror), and the partial is on disk just the same.
   Returns 77 on an allocation failure, as st_backstop() does. */
static int st_backstop_check_shutdown(httrackp *opt) {
  /* back_delete_all() raises the flag only for a live slot writing a file of
     its own. back_is_live() is a RANGE, 0 < status < STATUS_FTP_TRANSFER, so
     the rows walk it: one on each side, and two inside it far enough apart that
     narrowing the range to its first status fails. */
  static const struct {
    const char *what;
    int status;
    hts_boolean is_write;
    const char *url_sav;
    hts_boolean kept;
  } shutdown[] = {{"a free slot", STATUS_FREE, HTS_TRUE,
                   "hts-backstop-selftest.tmp", HTS_FALSE},
                  {"a slot already finished", STATUS_READY, HTS_TRUE,
                   "hts-backstop-selftest.tmp", HTS_FALSE},
                  {"an FTP slot", STATUS_FTP_TRANSFER, HTS_TRUE,
                   "hts-backstop-selftest.tmp", HTS_FALSE},
                  {"a slot writing nothing to disk", STATUS_TRANSFER, HTS_FALSE,
                   "", HTS_FALSE},
                  {"a .delayed placeholder", STATUS_TRANSFER, HTS_TRUE,
                   "hts-backstop-selftest.tmp." DELAYED_EXT, HTS_FALSE},
                  {"a partial file", STATUS_TRANSFER, HTS_TRUE,
                   "hts-backstop-selftest.tmp", HTS_TRUE},
                  {"a partial file awaiting its next chunk", STATUS_CHUNK_WAIT,
                   HTS_TRUE, "hts-backstop-selftest.tmp", HTS_TRUE}};

  int err = 0;
  size_t c;

  for (c = 0; c < sizeof(shutdown) / sizeof(shutdown[0]) && !err; c++) {
    struct_back *shut = back_new(opt, 1);
    cache_back shutcache;

    if (shut == NULL) {
      printf("backstop: SKIP (no slot table for the shutdown cases)\n");
      return 77;
    }
    memset(&shutcache, 0, sizeof(shutcache));
    shutcache.hashtable = coucal_new(0);
    shut->lnk[0].status = shutdown[c].status;
    shut->lnk[0].r.soc = INVALID_SOCKET;
    shut->lnk[0].r.is_write = shutdown[c].is_write;
    strcpybuff(shut->lnk[0].url_sav, shutdown[c].url_sav);
    opt->abort_left_partial = HTS_FALSE;

    back_delete_all(opt, &shutcache, shut);

    if (opt->abort_left_partial != shutdown[c].kept) {
      printf("  FAIL line %d: %s must %s the resume data at shutdown\n",
             __LINE__, shutdown[c].what, shutdown[c].kept ? "keep" : "drop");
      err = 1;
    }
    back_free(&shut);
    coucal_delete(&shutcache.hashtable);
  }
  return err;
}

/* A user stop must drop every slot but the FTP one (#1073, #1110). */
static int st_backstop(httrackp *opt, int argc, char **argv) {
  /* the states a stop sweeps, pre-connect then receive, and last the FTP one
     whose socket another thread owns */
  enum {
    SLOT_CONNECT = 0,
    SLOT_SSL,
    SLOT_HEADERS,
    SLOT_XFER,
    SLOT_CHUNK,
    SLOT_FTP,
    SLOTS
  };

  static const int status[SLOTS] = {
      STATUS_CONNECTING, STATUS_SSL_WAIT_HANDSHAKE, STATUS_WAIT_HEADERS,
      STATUS_TRANSFER,   STATUS_CHUNK_WAIT,         STATUS_FTP_TRANSFER};
  T_SOC swept[SLOTS];
  htsblk r[SLOTS];
  struct_back *sback = NULL;
  cache_back cache;
  lien_back *back;
  int err = 0;
  int skipped = 0;
  int round;
  int i;

  /* no quota may fire instead of the stop, and no slot may time out */
  opt->maxtime = 0;
  opt->maxsite = 0;
  opt->timeout = 0;
  opt->state.stop = 0;

  memset(&cache, 0, sizeof(cache));

#define CHECK(cond) ST_CHECK(err, cond)

  /* before the cleanup label has anything to free */
  sback = back_new(opt, SLOTS);
  if (sback == NULL)
    return 77;
  cache.hashtable = coucal_new(0);
  back = sback->lnk;
  if (!st_backstop_arm(opt, sback, status, r, SLOTS)) {
    skipped = 1;
    goto cleanup;
  }

  /* Control: with the mirror running, back_wait ends nothing. A sweep that
     fired unconditionally would strand every connect the crawl opens. Only the
     states back_wait leaves alone are checked; it advances the resolution and
     handshake waits by itself, the first to connect and the second to an error
     for want of a TLS session this fixture cannot build. */
  back_wait(sback, opt, &cache, 0);
  CHECK(back[SLOT_CONNECT].status == STATUS_CONNECTING);
  for (i = SLOT_HEADERS; i < SLOTS; i++)
    CHECK(back[i].status == status[i]);
  for (i = SLOT_CONNECT; i < SLOTS; i++) {
    if (i == SLOT_SSL)
      continue;
    CHECK(back[i].r.statuscode == STATUSCODE_TIMEOUT);
    CHECK(strcmp(back[i].r.msg, "untouched") == 0);
  }

  /* Twice, re-armed in between: the sweep runs on every wait, not once. A slot
     that was connecting when the user hit ^C is the case under test, so the
     flag goes up after the slots are in place. */
  for (round = 0; round < 2 && !err; round++) {
    opt->state.stop = 0;
    if (!st_backstop_arm(opt, sback, status, r, SLOTS)) {
      skipped = 1;
      goto cleanup;
    }
    for (i = 0; i < SLOTS; i++)
      swept[i] = r[i].soc;
    hts_request_stop(opt, 0);

    back_wait(sback, opt, &cache, 0);

    /* every slot but the FTP one is gone, its socket closed rather than merely
       forgotten, and reported as fatal so the link is not queued again */
    for (i = SLOT_CONNECT; i < SLOT_FTP; i++) {
      CHECK(back[i].status == STATUS_READY);
      CHECK(back[i].r.soc == INVALID_SOCKET);
      CHECK(back[i].r.statuscode == STATUSCODE_INVALID);
      CHECK(strcmp(back[i].r.msg, "mirror stopped by user") == 0);
      if (swept[i] != INVALID_SOCKET)
        CHECK(!st_backstop_soc_open(swept[i]));
    }
    /* control: an FTP worker is still writing through this slot, so a sweep
       reaching it frees a socket and a buffer under a live thread */
    CHECK(back[SLOT_FTP].status == status[SLOT_FTP]);
    CHECK(back[SLOT_FTP].r.soc == swept[SLOT_FTP]);
    CHECK(st_backstop_soc_open(back[SLOT_FTP].r.soc));
    CHECK(back[SLOT_FTP].r.statuscode == STATUSCODE_TIMEOUT);
    CHECK(strcmp(back[SLOT_FTP].r.msg, "untouched") == 0);
  }

  /* back_abort_stopped(), still in grace. A cap raises the stop flag itself and
     leaves the transfers already running a grace (#77, #481), so the sweep
     takes the pre-connect slots only and back_abort_limit() ends the rest
     later. */
  if (!err) {
    const LLint recv_was = hts_stat_recv_get();

    opt->state.stop = 0;
    if (!st_backstop_arm(opt, sback, status, r, SLOTS)) {
      skipped = 1;
      goto cleanup;
    }
    for (i = 0; i < SLOTS; i++)
      swept[i] = r[i].soc;
    /* cap reached, its whole grace still to overrun before the hard stop */
    hts_stat_recv_set(ST_BACKSTOP_CAP);
    opt->maxsite = ST_BACKSTOP_CAP;
    /* already false here, but it keeps the check below local to this block */
    opt->abort_left_partial = HTS_FALSE;
    hts_request_stop(opt, 0);

    back_wait(sback, opt, &cache, 0);

    /* nothing it swept was writing, so there is no partial to describe */
    CHECK(!opt->abort_left_partial);
    for (i = SLOT_CONNECT; i < SLOT_HEADERS; i++) {
      CHECK(back[i].status == STATUS_READY);
      CHECK(back[i].r.statuscode == STATUSCODE_INVALID);
    }
    for (i = SLOT_HEADERS; i < SLOTS; i++) {
      CHECK(back[i].status == status[i]);
      CHECK(back[i].r.soc == swept[i]);
      CHECK(st_backstop_soc_open(back[i].r.soc));
      CHECK(back[i].r.statuscode == STATUSCODE_TIMEOUT);
      CHECK(strcmp(back[i].r.msg, "untouched") == 0);
    }
    opt->maxsite = 0;
    hts_stat_recv_set(recv_was);
  }

  /* back_abort_stopped() keeps hts-cache/ref for a partial that outlives the
     stop, not for every slot it killed (#1595). A .delayed placeholder goes
     with its own ref, so it is not one. */
  {
    static const struct {
      const char *what;
      hts_boolean is_write;
      const char *url_sav;
      hts_boolean kept;
    } partial[] = {
        {"a slot writing nothing to disk", HTS_FALSE, "", HTS_FALSE},
        {"a .delayed placeholder", HTS_TRUE,
         "hts-backstop-selftest.tmp." DELAYED_EXT, HTS_FALSE},
        {"a partial file", HTS_TRUE, "hts-backstop-selftest.tmp", HTS_TRUE}};

    int c;

    for (c = 0; c < (int) (sizeof(partial) / sizeof(partial[0])) && !err; c++) {
      opt->state.stop = 0;
      if (!st_backstop_arm(opt, sback, status, r, SLOTS)) {
        skipped = 1;
        goto cleanup;
      }
      back[SLOT_XFER].r.is_write = partial[c].is_write;
      strcpybuff(back[SLOT_XFER].url_sav, partial[c].url_sav);
      opt->abort_left_partial = HTS_FALSE;
      hts_request_stop(opt, 0);

      back_wait(sback, opt, &cache, 0);

      if (opt->abort_left_partial != partial[c].kept) {
        printf("  FAIL line %d: %s must %s the resume data\n", __LINE__,
               partial[c].what, partial[c].kept ? "keep" : "drop");
        err = 1;
      }
      back[SLOT_XFER].r.is_write = 0;
      back[SLOT_XFER].url_sav[0] = '\0';
    }
  }

  /* back_abort_limit(), past the grace. It tears down the transfers the grace
     spared, through the same back_abort_slot(), so the partial it cuts keeps
     its resume data like any other (#1595). */
  if (!err) {
    const LLint recv_was = hts_stat_recv_get();

    opt->state.stop = 0;
    if (!st_backstop_arm(opt, sback, status, r, SLOTS)) {
      skipped = 1;
      goto cleanup;
    }
    back[SLOT_XFER].r.is_write = 1;
    strcpybuff(back[SLOT_XFER].url_sav, "hts-backstop-selftest.tmp");
    opt->abort_left_partial = HTS_FALSE;
    /* well past any grace, so the limit block ends every live slot and tuning
       back_maxsize_grace() cannot quietly put this case back inside it */
    opt->maxsite = ST_BACKSTOP_CAP;
    hts_stat_recv_set(ST_BACKSTOP_CAP * 2);
    hts_request_stop(opt, 0);

    back_wait(sback, opt, &cache, 0);

    CHECK(back[SLOT_XFER].status == STATUS_READY);
    /* not the statuscode: the arming poison is STATUSCODE_TIMEOUT too, so it
       cannot tell an untouched slot from a swept one */
    CHECK(strcmp(back[SLOT_XFER].r.msg, "Mirror Size Limit") == 0);
    CHECK(opt->abort_left_partial);
    back[SLOT_XFER].r.is_write = 0;
    back[SLOT_XFER].url_sav[0] = '\0';
    opt->maxsite = 0;
    hts_stat_recv_set(recv_was);
  }
  if (!err) {
    const int rc = st_backstop_check_shutdown(opt);

    if (rc == 77) {
      skipped = 1;
      goto cleanup;
    }
    err = rc;
  }
#undef CHECK

cleanup:
  opt->maxsite = 0;
  opt->state.stop = 0;
  opt->abort_left_partial = HTS_FALSE;
  for (i = 0; i < SLOTS; i++)
    deletehttp(&back[i].r);
  back_free(&sback);
  coucal_delete(&cache.hashtable);
  if (skipped)
    return 77;

  return st_verdict("backstop self-test", err);
}

/* back_new() sizes its slot table from -cN, so a user can ask for one that
   does not fit. The contract is a NULL return, which httpmirror() turns into a
   log line and a stopped mirror, where an abort loses that message. */
static int st_backnew(httrackp *opt, int argc, char **argv) {
  /* Too big to serve without the allocator asking the kernel for more. */
  enum { slots = 4096 };

  struct_back *sback;

  /* Control: with memory available the same call hands back a whole table, so
     a NULL below cannot be back_new refusing every size. */
  sback = back_new(opt, slots);
  if (sback == NULL || sback->count != slots || sback->lnk == NULL ||
      sback->connect_fallback == NULL || sback->ready == NULL) {
    printf("backnew: FAILED (control table incomplete)\n");
    back_free(&sback);
    return 1;
  }
  back_free(&sback);

  /* A count whose back_max + 1 is not an int is refused before any allocation:
     the sum used to be signed UB. */
  if (back_new(opt, INT_MAX) != NULL || back_new(opt, -1) != NULL) {
    printf("backnew: FAILED (a count it cannot express was accepted)\n");
    return 1;
  }

  /* What -cN is turned into must always be something back_new() can express, so
     check the clamp itself rather than an allocator's answer to an absurd
     request: -cN is unclamped under --bypass-limits, maxsoc * 32 + 1024 used to
     overflow, and how a platform refuses a 37 TB table is its own business. */
  {
    static const int maxsocs[] = {
        -1, 0, 1, 8, 67108830, 67108831, 67108832, 2000000000, INT_MAX};
    size_t i;

    for (i = 0; i < sizeof(maxsocs) / sizeof(maxsocs[0]); i++) {
      const int slots = back_max_slots(maxsocs[i]);

      if (slots <= 0 || slots == INT_MAX) {
        printf("backnew: FAILED (-c%d yields %d slots)\n", maxsocs[i], slots);
        return 1;
      }
    }
    /* Narrowing control: an ordinary -c is still sized from it, not capped. */
    if (back_max_slots(8) != 8 * 32 + 1024) {
      printf("backnew: FAILED (-c8 is capped: %d)\n", back_max_slots(8));
      return 1;
    }
  }
#ifndef _WIN32
  {
    struct rlimit saved, tight;

    if (getrlimit(RLIMIT_AS, &saved) != 0) {
      printf("backnew: cannot cap memory, skipped\n");
      return 0;
    }
    tight = saved;
    tight.rlim_cur = 1024 * 1024;
    if (setrlimit(RLIMIT_AS, &tight) != 0) {
      printf("backnew: cannot cap memory, skipped\n");
      return 0;
    }
    sback = back_new(opt, slots);
    (void) setrlimit(RLIMIT_AS, &saved);
    if (sback != NULL) {
      /* The cap is advisory here, so the run says nothing either way. */
      back_free(&sback);
      printf("backnew: cap did not bite, skipped\n");
      return 0;
    }
    printf("backnew: oom OK\n");
    return 0;
  }
#else
  printf("backnew: cannot cap memory, skipped\n");
  return 0;
#endif
}

/* A link whose strings cannot be copied is dropped whole, not left half-built.
 */
static int st_recordoom(httrackp *opt, int argc, char **argv) {
  static const size_t huge = 64 * 1024 * 1024;
  struct_back *sback = NULL;
  hash_struct hash;
  cache_back cache;
  char *big = NULL;
  int rc = 1;
  int before;
  int recorded = 0, recorded_cod = 0, after = -1, after_cod = -1;

  memset(&cache, 0, sizeof(cache));
  st_mirror_wiring(opt, &sback, &hash, HTS_FALSE);
  if (!hts_record_link(opt, "www.example.com", "/a.html", "/p/a.html", "", "",
                       NULL)) {
    printf("recordoom: FAILED (no first link)\n");
    goto done;
  }
  before = opt->lien_tot;
  big = malloct(huge);
  assertf(big != NULL);
  memset(big, 'x', huge - 1);
  big[huge - 1] = '\0';
#ifndef _WIN32
  {
    struct rlimit saved, tight;

    if (getrlimit(RLIMIT_AS, &saved) != 0) {
      printf("recordoom: cannot cap memory, skipped\n");
      rc = 0;
      goto done;
    }
    tight = saved;
    tight.rlim_cur = 1024 * 1024;
    if (setrlimit(RLIMIT_AS, &tight) != 0) {
      printf("recordoom: cannot cap memory, skipped\n");
      rc = 0;
      goto done;
    }
    /* the address, then the codebase of a .class: the first and last copy */
    recorded = hts_record_link(opt, big, "/b.html", "/p/b.html", "", "", NULL);
    after = opt->lien_tot;
    recorded_cod = hts_record_link(opt, "www.example.com", "/d.class",
                                   "/p/d.class", "", "", big);
    after_cod = opt->lien_tot;
    (void) setrlimit(RLIMIT_AS, &saved);
  }
#else
  printf("recordoom: cannot cap memory, skipped\n");
  rc = 0;
  goto done;
#endif
  if (recorded || recorded_cod) {
    printf("recordoom: cap did not bite, skipped\n");
    rc = 0;
    goto done;
  }
  if (after != before || after_cod != before || opt->liens[before] != NULL) {
    printf("recordoom: FAILED (refused links left %d then %d entries, want "
           "%d)\n",
           after, after_cod, before);
    goto done;
  }
  if (!hts_record_link(opt, "www.example.com", "/c.html", "/p/c.html", "", "",
                       NULL) ||
      opt->lien_tot != before + 1 ||
      strcmp(opt->liens[before]->fil, "/c.html") != 0) {
    printf("recordoom: FAILED (no link recorded after the refusal)\n");
    goto done;
  }
  printf("recordoom: oom OK\n");
  rc = 0;

done:
  freet(big);
  st_mirror_wiring_free(opt, &cache, &sback, &hash);
  return rc;
}

/* Both 32-bit halves carry the step, so a torn read has unequal halves. */
#define RELAXED_STEPS 100000
#define RELAXED_VALUE(i) ((LLint) (i) * (LLint) 0x100000001)

static volatile LLint relaxed_counter = 0;

static void relaxed_writer(void *arg) {
  int i;

  (void) arg;
  for (i = 1; i <= RELAXED_STEPS; i++)
    hts_store_relaxed_llint(&relaxed_counter, RELAXED_VALUE(i));
}

static int st_relaxedcounter(httrackp *opt, int argc, char **argv) {
  LLint last = 0;
  long spins;
  int err = 0;

  hts_store_relaxed_llint(&relaxed_counter, 0);
  if (hts_newthread(relaxed_writer, NULL) != 0) {
    fprintf(stderr, "relaxedcounter: cannot spawn\n");
    return 1;
  }
  /* Read while the writer runs, as the progress display reads an FTP slot. */
  for (spins = 0; last != RELAXED_VALUE(RELAXED_STEPS) && spins < 100000000L;
       spins++) {
    const LLint v = hts_load_relaxed_llint(&relaxed_counter);

#ifdef HTS_LLINT_ATOMIC
    const hts_boolean bad = v < last || (v >> 32) != (v & 0xffffffff);
#else
    const hts_boolean bad = HTS_FALSE; /* This host's plain access may tear. */
#endif

    if (bad) {
      fprintf(stderr, "relaxedcounter: read " LLintP " after " LLintP "\n", v,
              last);
      err = 1;
      break;
    }
    last = v;
  }
  htsthread_wait();
  if (hts_load_relaxed_llint(&relaxed_counter) !=
      RELAXED_VALUE(RELAXED_STEPS)) {
    fprintf(stderr, "relaxedcounter: final value " LLintP "\n",
            hts_load_relaxed_llint(&relaxed_counter));
    err = 1;
  }
  return st_verdict("relaxedcounter self-test", err);
}

static int st_threadwait(httrackp *opt, int argc, char **argv) {
  int err = 0;
  int i, round;

  /* htsthread_wait() joins a thread spawned just before it */
  for (round = 0; round < THREADWAIT_ROUNDS && !err; round++) {
    hts_mutexlock(&threadwait_lock);
    threadwait_done = 0;
    hts_mutexrelease(&threadwait_lock);
    if (hts_newthread(threadwait_thread, NULL) != 0) {
      fprintf(stderr, "threadwait: cannot spawn\n");
      return 1;
    }
    htsthread_wait();
    if (threadwait_count() != 1) {
      fprintf(stderr, "threadwait: round %d returned before the thread ran\n",
              round);
      err = 1;
    }
  }

  /* htsthread_wait_n(n) leaves n behind rather than draining everything */
  hts_mutexlock(&threadwait_lock);
  threadwait_done = 0;
  threadwait_gated = HTS_TRUE;
  hts_mutexrelease(&threadwait_lock);
  for (i = 0; i < THREADWAIT_N; i++) {
    if (hts_newthread(threadwait_gated_thread, NULL) != 0) {
      fprintf(stderr, "threadwait: cannot spawn a gated thread\n");
      return 1;
    }
  }
  htsthread_wait_n(THREADWAIT_N);
  if (threadwait_count() != 0) {
    fprintf(stderr, "threadwait: wait_n(%d) joined %d gated threads\n",
            THREADWAIT_N, threadwait_count());
    err = 1;
  }
  hts_mutexlock(&threadwait_lock);
  threadwait_gated = HTS_FALSE;
  hts_mutexrelease(&threadwait_lock);
  htsthread_wait();
  if (threadwait_count() != THREADWAIT_N) {
    fprintf(stderr, "threadwait: wait left %d/%d gated threads running\n",
            THREADWAIT_N - threadwait_count(), THREADWAIT_N);
    err = 1;
  }

  return st_verdict("threadwait self-test", err);
}

/* Confirms the runner encloses the body exactly once, on the worker. */
#ifdef _WIN32
typedef DWORD threadrunner_id;
#define threadrunner_self() GetCurrentThreadId()
#define threadrunner_same(a, b) ((a) == (b))
#else
typedef pthread_t threadrunner_id;
#define threadrunner_self() pthread_self()
#define threadrunner_same(a, b) (pthread_equal((a), (b)) != 0)
#endif

static htsmutex threadrunner_lock = HTSMUTEX_INIT;
static int threadrunner_before = 0;
static int threadrunner_after = 0;
static int threadrunner_body = 0;
/* What the runner had done when the body ran, 1 and 0 from inside it. */
static int threadrunner_seen_before = 0;
static int threadrunner_seen_after = 0;
static threadrunner_id threadrunner_body_thread;
static threadrunner_id threadrunner_runner_thread;
static int threadrunner_tail = 0;
static int threadrunner_body_at_tail = 0;
static threadrunner_id threadrunner_tail_thread;
/* Its address is the worker's arg, so a tail handed another pointer shows. */
static int threadrunner_owner;
static void *threadrunner_tail_arg = NULL;
static jmp_buf threadrunner_jmp;

static void threadrunner_count(int *what) {
  hts_mutexlock(&threadrunner_lock);
  (*what)++;
  hts_mutexrelease(&threadrunner_lock);
}

static void threadrunner_reset(void) {
  threadrunner_before = 0;
  threadrunner_after = 0;
  threadrunner_body = 0;
  threadrunner_seen_before = 0;
  threadrunner_seen_after = 0;
  threadrunner_tail = 0;
  threadrunner_body_at_tail = 0;
  threadrunner_tail_arg = NULL;
}

static void threadrunner_body_fn(void *arg) {
  (void) arg;
  hts_mutexlock(&threadrunner_lock);
  threadrunner_seen_before = threadrunner_before;
  threadrunner_seen_after = threadrunner_after;
  threadrunner_body_thread = threadrunner_self();
  threadrunner_body++;
  hts_mutexrelease(&threadrunner_lock);
}

/* Leaves the body the way a fault recovery does, so nothing after the jump
   runs. A real runner gets there through siglongjmp(). */
static void threadrunner_cut_fn(void *arg) {
  (void) arg;
  hts_mutexlock(&threadrunner_lock);
  threadrunner_body_thread = threadrunner_self();
  threadrunner_body++;
  hts_mutexrelease(&threadrunner_lock);
  longjmp(threadrunner_jmp, 1);
}

/* A worker an earlier mirror abandoned: the round moves on before the fault, so
   the fault belongs to a mirror that is over. */
static void threadrunner_stale_fn(void *arg) {
  (void) arg;
  hts_mutexlock(&threadrunner_lock);
  threadrunner_body_thread = threadrunner_self();
  threadrunner_body++;
  hts_mutexrelease(&threadrunner_lock);
  hts_worker_fault_clear(); /* as the next mirror does when it starts */
  longjmp(threadrunner_jmp, 1);
}

static void threadrunner_tail_fn(void *arg) {
  hts_mutexlock(&threadrunner_lock);
  threadrunner_tail_arg = arg;
  threadrunner_body_at_tail = threadrunner_body;
  threadrunner_tail_thread = threadrunner_self();
  threadrunner_tail++;
  hts_mutexrelease(&threadrunner_lock);
}

static void threadrunner_runner(void (*fun)(void *arg), void *arg) {
  threadrunner_runner_thread = threadrunner_self();
  threadrunner_count(&threadrunner_before);
  /* Stands in for a fault handler's own jump buffer. */
  if (setjmp(threadrunner_jmp) == 0)
    fun(arg);
  threadrunner_count(&threadrunner_after);
}

static hts_boolean threadrunner_spawn_body(void (*fun)(void *arg),
                                           void (*tail)(void *arg)) {
  if (hts_newthread_tail(fun, &threadrunner_owner, tail) != 0) {
    fprintf(stderr, "threadrunner: cannot spawn\n");
    return HTS_FALSE;
  }
  htsthread_wait();
  return HTS_TRUE;
}

static hts_boolean threadrunner_spawn(void) {
  return threadrunner_spawn_body(threadrunner_body_fn, NULL);
}

static int st_threadrunner(httrackp *opt, int argc, char **argv) {
  const threadrunner_id caller = threadrunner_self();
  hts_boolean spawned;
  int err = 0;

  /* Unlocked, because htsthread_wait() published the worker's writes. */
  threadrunner_reset();
  if (hts_set_thread_runner(threadrunner_runner) != NULL) {
    fprintf(stderr, "threadrunner: a runner was installed already\n");
    return 1;
  }
  spawned = threadrunner_spawn();
  /* Process-global, so clear it before any check can return early. */
  if (hts_set_thread_runner(NULL) != threadrunner_runner) {
    fprintf(stderr, "threadrunner: the setter did not hand back the runner\n");
    err = 1;
  }
  if (!spawned)
    return 1;

  if (threadrunner_before != 1 || threadrunner_after != 1) {
    fprintf(stderr, "threadrunner: runner entered %d time(s), left %d\n",
            threadrunner_before, threadrunner_after);
    err = 1;
  }
  if (threadrunner_body != 1) {
    fprintf(stderr, "threadrunner: the body ran %d time(s), expected once\n",
            threadrunner_body);
    err = 1;
  }
  if (threadrunner_seen_before != 1 || threadrunner_seen_after != 0) {
    fprintf(stderr, "threadrunner: the body saw the runner at %d/%d, not 1/0\n",
            threadrunner_seen_before, threadrunner_seen_after);
    err = 1;
  }
  /* A sigsetjmp on the caller's stack would catch nothing the worker does. */
  if (threadrunner_same(threadrunner_body_thread, caller) ||
      !threadrunner_same(threadrunner_body_thread,
                         threadrunner_runner_thread)) {
    fprintf(stderr,
            "threadrunner: the body did not run on the runner's worker\n");
    err = 1;
  }

  /* NULL restores the plain call. */
  threadrunner_reset();
  if (!threadrunner_spawn())
    return 1;
  if (threadrunner_body != 1) {
    fprintf(stderr, "threadrunner: no runner, the body ran %d time(s)\n",
            threadrunner_body);
    err = 1;
  }
  if (threadrunner_before != 0 || threadrunner_after != 0) {
    fprintf(stderr, "threadrunner: a cleared runner still entered %d time(s)\n",
            threadrunner_before);
    err = 1;
  }
  if (threadrunner_same(threadrunner_body_thread, caller)) {
    fprintf(stderr, "threadrunner: the body ran on the caller's thread\n");
    err = 1;
  }

  /* A tail runs on the worker once the body is over, whether or not the body
     reached its own end. The FTP worker list is released there. */
  threadrunner_reset();
  if (!threadrunner_spawn_body(threadrunner_body_fn, threadrunner_tail_fn))
    return 1;
  if (threadrunner_body != 1 || threadrunner_tail != 1) {
    fprintf(stderr, "threadrunner: body %d time(s) and tail %d, expected 1/1\n",
            threadrunner_body, threadrunner_tail);
    err = 1;
  }
  if (threadrunner_body_at_tail != 1) {
    fprintf(stderr, "threadrunner: the tail ran before the body\n");
    err = 1;
  }
  if (threadrunner_tail_arg != &threadrunner_owner) {
    fprintf(stderr, "threadrunner: the tail got another worker's arg\n");
    err = 1;
  }
  if (hts_worker_faulted()) {
    fprintf(stderr, "threadrunner: a body that finished reads as a fault\n");
    err = 1;
  }

  /* The body jumps out of threadrunner_cut_fn, so only the tail can reap the
     worker. */
  threadrunner_reset();
  if (hts_set_thread_runner(threadrunner_runner) != NULL) {
    fprintf(stderr, "threadrunner: a runner was installed already\n");
    return 1;
  }
  spawned = threadrunner_spawn_body(threadrunner_cut_fn, threadrunner_tail_fn);
  hts_set_thread_runner(NULL);
  if (!spawned)
    return 1;
  /* The runner left normally, so the engine saw a worker whose body stopped
     halfway and nothing else. */
  if (threadrunner_body != 1 || threadrunner_after != 1) {
    fprintf(stderr, "threadrunner: recovery ran the body %d time(s), left %d\n",
            threadrunner_body, threadrunner_after);
    err = 1;
  }
  if (threadrunner_tail != 1 || threadrunner_body_at_tail != 1) {
    fprintf(stderr,
            "threadrunner: a cut-short body ran the tail %d time(s), at %d\n",
            threadrunner_tail, threadrunner_body_at_tail);
    err = 1;
  } else if (!threadrunner_same(threadrunner_tail_thread,
                                threadrunner_body_thread)) {
    fprintf(stderr, "threadrunner: the tail ran off the worker thread\n");
    err = 1;
  }
  if (threadrunner_tail_arg != &threadrunner_owner) {
    fprintf(stderr,
            "threadrunner: a cut-short body gave the tail another arg\n");
    err = 1;
  }

  /* The mirror gives up on it, and says so through the exit status rather than
     reading as a stop the user asked for. */
  if (!hts_worker_faulted()) {
    fprintf(stderr, "threadrunner: a cut-short body raised no fault\n");
    err = 1;
  }
  {
    FILE *const saved_log = opt->log;

    opt->log = NULL; /* the abort logs, and this test's own output is exact */
    opt->state.stop = 0;
    opt->state.exit_xh = 0;
    back_checkmirror(opt);
    if (opt->state.stop != 1 || opt->state.exit_xh != -1) {
      fprintf(stderr, "threadrunner: the mirror read the fault as stop %d/%d\n",
              opt->state.stop, opt->state.exit_xh);
      err = 1;
    }
    /* a stop the user asked for exits 0, so the fault's verdict outranks it */
    opt->state.stop = 1;
    opt->state.exit_xh = 1;
    back_check_worker_fault(opt);
    if (opt->state.exit_xh != -1) {
      fprintf(stderr, "threadrunner: a user stop swallowed the fault (%d)\n",
              opt->state.exit_xh);
      err = 1;
    }
    opt->state.stop = 0;
    opt->state.exit_xh = 0;
    hts_worker_fault_clear();
    back_checkmirror(opt);
    if (opt->state.exit_xh != 0) {
      fprintf(stderr,
              "threadrunner: a cleared fault still aborted the mirror\n");
      err = 1;
    }
    opt->log = saved_log;
  }

  /* Round 0 is the only round the cases above ran in, and no mirror uses it:
     each one takes the next round as it starts. */
  hts_worker_fault_clear();
  threadrunner_reset();
  if (hts_set_thread_runner(threadrunner_runner) != NULL) {
    fprintf(stderr, "threadrunner: a runner was installed already\n");
    return 1;
  }
  spawned = threadrunner_spawn_body(threadrunner_cut_fn, threadrunner_tail_fn);
  hts_set_thread_runner(NULL);
  if (!spawned)
    return 1;
  if (!hts_worker_faulted()) {
    fprintf(stderr, "threadrunner: no fault past the first round\n");
    err = 1;
  }
  hts_worker_fault_clear();

  /* A fault raised after its own mirror ended aborts nothing. */
  threadrunner_reset();
  if (hts_set_thread_runner(threadrunner_runner) != NULL) {
    fprintf(stderr, "threadrunner: a runner was installed already\n");
    return 1;
  }
  spawned =
      threadrunner_spawn_body(threadrunner_stale_fn, threadrunner_tail_fn);
  hts_set_thread_runner(NULL);
  if (!spawned)
    return 1;
  if (threadrunner_body != 1 || threadrunner_tail != 1) {
    fprintf(stderr, "threadrunner: the stale round ran %d body and %d tail\n",
            threadrunner_body, threadrunner_tail);
    err = 1;
  }
  if (hts_worker_faulted()) {
    fprintf(stderr, "threadrunner: a fault from a finished mirror was kept\n");
    err = 1;
  }

  return st_verdict("threadrunner self-test", err);
}

static int st_ftpworker(httrackp *opt, int argc, char **argv) {
  const int err = ftp_worker_selftests();

  return st_verdict("ftp-worker-selftest", err);
}

static int st_ftphandoff(httrackp *opt, int argc, char **argv) {
  const int err = ftp_handoff_selftests(opt);

  return st_verdict("ftp-handoff-selftest", err);
}

/* #1697: hts_strerror() must own its output. strerror() answers with a buffer
   the next call in that thread reuses and thread exit frees, and POSIX lets it
   be one static shared by every thread. */
#define STRERROR_THREADS 8
#define STRERROR_ROUNDS 20000

/* Four errno values every libc names, then four codes none of them do: the
   second group is what a libc formats into that buffer instead of answering
   with a constant string of its own. */
static const int strerror_codes[STRERROR_THREADS] = {
    EACCES, ENOENT, EINVAL, EPERM, 4001, 4002, 4003, 4004};

static char strerror_expected[STRERROR_THREADS][HTS_STRERROR_SIZE];
static htsmutex strerror_lock = HTSMUTEX_INIT;
static int strerror_bad = 0;

static void strerror_thread(void *arg) {
  const int i = *(const int *) arg;
  int bad = 0, round;

  for (round = 0; round < STRERROR_ROUNDS; round++) {
    char buf[HTS_STRERROR_SIZE];

    if (strcmp(hts_strerror(strerror_codes[i], buf, sizeof(buf)),
               strerror_expected[i]) != 0)
      bad++;
  }
  hts_mutexlock(&strerror_lock);
  strerror_bad += bad;
  hts_mutexrelease(&strerror_lock);
}

static int st_strerror(httrackp *opt, int argc, char **argv) {
  static int idx[STRERROR_THREADS];
  char first[HTS_STRERROR_SIZE], second[HTS_STRERROR_SIZE];
  const char *kept;
  int err = 0, i;

  for (i = 0; i < STRERROR_THREADS; i++) {
    const char *const msg = hts_strerror(
        strerror_codes[i], strerror_expected[i], sizeof(strerror_expected[i]));

    if (msg != strerror_expected[i] || *msg == '\0') {
      fprintf(stderr, "strerror: code %d gave no message of its own\n",
              strerror_codes[i]);
      err = 1;
    }
  }

  /* A caller can hold two messages at once, where a shared buffer would have
     lost the first to the second call. Codes 0 and 1 because musl answers
     every unknown one alike, and strerror_expected[] because kept is first. */
  kept = hts_strerror(strerror_codes[0], first, sizeof(first));
  (void) hts_strerror(strerror_codes[1], second, sizeof(second));
  if (strcmp(kept, strerror_expected[0]) != 0) {
    fprintf(stderr,
            "strerror: the next call changed an earlier message to \"%s\"\n",
            kept);
    err = 1;
  }
  if (strcmp(first, second) == 0) {
    fprintf(stderr, "strerror: two different codes gave one message \"%s\"\n",
            first);
    err = 1;
  }

  /* Every capacity fills, terminates, and writes nothing past its end. The
     bytes above it are poisoned non-zero, so a stray NUL shows up too. */
  {
    static const size_t sizes[] = {1, 2, 4, 20};

    struct {
      char dst[20];
      char tail[8];
    } s;

    char ref[sizeof(s)];
    size_t k;

    memset(ref, '#', sizeof(ref));
    for (k = 0; k < sizeof(sizes) / sizeof(sizes[0]); k++) {
      const size_t cap = sizes[k];

      memset(&s, '#', sizeof(s));
      if (hts_strerror(EACCES, s.dst, cap) != s.dst ||
          memchr(s.dst, '\0', cap) == NULL || (cap > 1 && s.dst[0] == '\0')) {
        fprintf(stderr, "strerror: capacity %d was not filled and terminated\n",
                (int) cap);
        err = 1;
      }
      if (memcmp((const char *) &s + cap, ref, sizeof(s) - cap) != 0) {
        fprintf(stderr, "strerror: capacity %d wrote past its end\n",
                (int) cap);
        err = 1;
      }
    }
  }

#if !HTS_STRERROR_REENTRANT
  printf("strerror: this build kept plain strerror(), so the concurrent phase "
         "below proves nothing here\n");
#endif
  for (i = 0; i < STRERROR_THREADS; i++) {
    idx[i] = i;
    if (hts_newthread(strerror_thread, &idx[i]) != 0) {
      fprintf(stderr, "strerror: cannot spawn\n");
      return 1;
    }
  }
  htsthread_wait();
  if (strerror_bad != 0) {
    fprintf(stderr, "strerror: %d/%d concurrent messages were wrong\n",
            strerror_bad, STRERROR_THREADS * STRERROR_ROUNDS);
    err = 1;
  }

  return st_verdict("strerror self-test", err);
}

/* hts_mutexlock() builds a lock on first use, so threads meet an empty slot. */
#define MUTEXLAZY_THREADS 8
#define MUTEXLAZY_LOCKS 64
#define MUTEXLAZY_ROUNDS 40

/* Never initialized here: the first pass of every thread is what races. */
static htsmutex mutexlazy_lock[MUTEXLAZY_LOCKS];

/* Bumped under the lock, so a plain int is what proves the lock excluded. */
static int mutexlazy_count[MUTEXLAZY_LOCKS];

static void mutexlazy_thread(void *arg) {
  int round, i;

  (void) arg;
  for (round = 0; round < MUTEXLAZY_ROUNDS; round++) {
    for (i = 0; i < MUTEXLAZY_LOCKS; i++) {
      hts_mutexlock(&mutexlazy_lock[i]);
      mutexlazy_count[i]++;
      hts_mutexrelease(&mutexlazy_lock[i]);
    }
  }
}

/* The ordering half only shows where loads are reordered: arm64, or any build
   under ThreadSanitizer. What it pins everywhere is that the race yields one
   working lock. */
static int st_mutexlazyinit(httrackp *opt, int argc, char **argv) {
  const int want = MUTEXLAZY_THREADS * MUTEXLAZY_ROUNDS;
  int err = 0;
  int i;

  for (i = 0; i < MUTEXLAZY_THREADS; i++) {
    if (hts_newthread(mutexlazy_thread, NULL) != 0) {
      fprintf(stderr, "mutexlazyinit: cannot spawn\n");
      return 1;
    }
  }
  htsthread_wait();

  for (i = 0; i < MUTEXLAZY_LOCKS; i++) {
    if (mutexlazy_lock[i] == HTSMUTEX_INIT) {
      fprintf(stderr, "mutexlazyinit: lock %d was never built\n", i);
      err = 1;
    } else if (mutexlazy_count[i] != want) {
      fprintf(stderr, "mutexlazyinit: lock %d counted %d of %d\n", i,
              mutexlazy_count[i], want);
      err = 1;
    }
    hts_mutexfree(&mutexlazy_lock[i]);
    if (mutexlazy_lock[i] != HTSMUTEX_INIT) {
      fprintf(stderr, "mutexlazyinit: lock %d survived the free\n", i);
      err = 1;
    }
    mutexlazy_count[i] = 0;
  }

  return st_verdict("mutexlazyinit self-test", err);
}

/* ------------------------------------------------------------ */
/* Registry: this module's tests, in the order -#test lists them. */
/* ------------------------------------------------------------ */

/* host_ban() must never end an FTP slot, which its worker thread is still
 * writing. */
#define ST_HOSTBAN_HOST "ftp.example.invalid"
#define ST_HOSTBAN_POISON 4242

static void st_hostban_slot(struct_back *sback, int p, int status,
                            const char *adr) {
  lien_back *const back = &sback->lnk[p];

  back->status = status;
  back->timeout = -1;
  back->rateout = -1;
  back->stop_ftp = HTS_FALSE;
  back->r.soc = INVALID_SOCKET; /* or host_ban() would deletehttp() fd 0 */
  /* open, so a wrong teardown is caught by the fclose it performs */
  back->r.fp = tmpfile();
  assertf(back->r.fp != NULL);
  back->r.statuscode = ST_HOSTBAN_POISON;
  strcpybuff(back->r.msg, "untouched");
  strcpybuff(back->url_adr, adr);
  strcpybuff(back->url_fil, "/pub/big.iso");
}

static int st_hostban(httrackp *opt, int argc, char **argv) {
  enum { SLOT_FTP = 0, SLOT_HTTP, SLOTS };

  struct_back *sback;
  char **filters = NULL;
  int filptr = 0;
  int err = 0;

  opt->maxfilter = 128;
  if (filters_init(&filters, opt->maxfilter, 0) == 0) {
    fprintf(stderr, "hostban: cannot allocate filters\n");
    return 77;
  }
  filters_bind(opt, &filters, &filptr);
  sback = back_new(opt, SLOTS);
  if (sback == NULL) {
    fprintf(stderr, "hostban: cannot allocate the backing table\n");
    return 77;
  }
  /* An FTP slot's url_adr carries its scheme (htsback.c, on "ftp://"), an
     http one does not, and host_ban() is handed a bare host. */
  st_hostban_slot(sback, SLOT_FTP, STATUS_FTP_TRANSFER,
                  "ftp://" ST_HOSTBAN_HOST);
  st_hostban_slot(sback, SLOT_HTTP, STATUS_TRANSFER, ST_HOSTBAN_HOST);

  host_ban(opt, 0, sback, ST_HOSTBAN_HOST);

  /* The worker's slot, exactly as it was: poison intact, file still open. */
  if (sback->lnk[SLOT_FTP].status != STATUS_FTP_TRANSFER) {
    fprintf(stderr, "hostban: the worker's slot went to status %d\n",
            sback->lnk[SLOT_FTP].status);
    err++;
  }
  if (sback->lnk[SLOT_FTP].r.fp == NULL) {
    fprintf(stderr, "hostban: the worker's file was closed under it\n");
    err++;
  }
  if (sback->lnk[SLOT_FTP].r.statuscode != ST_HOSTBAN_POISON ||
      strcmp(sback->lnk[SLOT_FTP].r.msg, "untouched") != 0) {
    fprintf(stderr, "hostban: the worker's slot was rewritten as %d '%s'\n",
            sback->lnk[SLOT_FTP].r.statuscode, sback->lnk[SLOT_FTP].r.msg);
    err++;
  }
  /* The control: an http slot of the same host IS ended, so a test that stopped
     ending anything fails rather than passing quietly. */
  if (sback->lnk[SLOT_HTTP].status != STATUS_READY) {
    fprintf(stderr, "hostban: the http slot was left at status %d\n",
            sback->lnk[SLOT_HTTP].status);
    err++;
  }
  if (sback->lnk[SLOT_HTTP].r.fp != NULL) {
    fprintf(stderr, "hostban: the http slot's file was left open\n");
    err++;
  }
  if (strcmp(sback->lnk[SLOT_HTTP].r.msg, "Link Cancelled by host control") !=
      0) {
    fprintf(stderr, "hostban: the http slot reads '%s'\n",
            sback->lnk[SLOT_HTTP].r.msg);
    err++;
  }

  if (sback->lnk[SLOT_FTP].r.fp != NULL) {
    fclose(sback->lnk[SLOT_FTP].r.fp);
    sback->lnk[SLOT_FTP].r.fp = NULL;
  }
  back_free(&sback);
  freet(filters[0]);
  freet(filters);
  return st_verdict("hostban self-test", err);
}

/* host_ban() cancels every link of the host, whatever its address length. */
#define ST_HOSTBAN_LONG 1100

static char *st_hostban_long(const char *prefix, char fill,
                             const char *suffix) {
  const size_t lp = strlen(prefix), ls = strlen(suffix);
  char *const s = malloct(lp + ST_HOSTBAN_LONG + ls + 1);

  assertf(s != NULL);
  memcpy(s, prefix, lp);
  memset(s + lp, fill, ST_HOSTBAN_LONG);
  memcpy(s + lp + ST_HOSTBAN_LONG, suffix, ls + 1);
  return s;
}

static int st_hostbanlinks(httrackp *opt, int argc, char **argv) {
  enum {
    LNK_SHORT = 0,
    LNK_LONGLOGIN,
    LNK_LONGHOST,
    LNK_OTHER,
    LNK_OTHERLONGHOST,
    LINKS
  };

  static const int banned[LINKS] = {1, 1, 1, 0, 0};
  FILE *const saved_log = opt->log;
  struct_back *sback = NULL;
  hash_struct hash;
  cache_back cache;
  char **filters = NULL;
  int filptr = 0;
  int err = 0;
  int i;
  char *login = st_hostban_long("user:", 'p', "@" ST_HOSTBAN_HOST);
  char *longhost = st_hostban_long("", 'h', ".example.invalid");
  char *otherlong = st_hostban_long("", 'o', ".example.invalid");
  const char *const adr[LINKS] = {ST_HOSTBAN_HOST, login, longhost,
                                  "other.example.invalid", otherlong};

  opt->log = NULL;
  memset(&cache, 0, sizeof(cache));
  st_mirror_wiring(opt, &sback, &hash, HTS_TRUE);
  opt->maxfilter = 128;
  assertf(filters_init(&filters, opt->maxfilter, 0) != 0);
  filters_bind(opt, &filters, &filptr);
  for (i = 0; i < LINKS; i++)
    assertf(hts_record_link(opt, adr[i], "/a.html", "/p/a.html", "", "", NULL));

  host_ban(opt, 0, sback, ST_HOSTBAN_HOST);
  /* a host of 1020+ bytes, banned by its own name */
  host_ban(opt, 0, sback, longhost);

  for (i = 0; i < LINKS; i++) {
    if ((opt->liens[i]->pass2 == -1) != banned[i]) {
      fprintf(stderr, "hostbanlinks: link %d (adr length %d) %s\n", i,
              (int) strlen(adr[i]),
              banned[i] ? "was left valid" : "was cancelled");
      err++;
    }
  }

  opt->log = saved_log;
  freet(filters[0]);
  freet(filters);
  st_mirror_wiring_free(opt, &cache, &sback, &hash);
  freet(otherlong);
  freet(longhost);
  freet(login);
  return st_verdict("hostbanlinks self-test", err);
}

/* The clock starts just before a wall-clock second ends, so a whole-second
   -E count would stop 200ms in. */
static int st_maxtime(httrackp *opt, int argc, char **argv) {
  FILE *const saved_log = opt->log;
  int err = 0;
  int tries;
  TStamp t0 = 0, elapsed = 0;

  opt->log = NULL; /* the stop logs, and this test's own output is exact */
  opt->maxtime = 1;
  /* A starved host can oversleep past the cap, so retry. The window is timed
     here, never through the clock under test. */
  for (tries = 0; tries < 5; tries++) {
    while (mtime_local() % 1000 < 850 || mtime_local() % 1000 >= 900)
      Sleep(5);
    hts_mirror_clock_start();
    t0 = mtime_monotonic();
    Sleep(200);
    opt->state.stop = 0;
    back_checkmirror(opt);
    elapsed = mtime_monotonic() - t0;
    if (elapsed < 1000)
      break;
  }
  if (opt->state.stop && elapsed < 1000) {
    fprintf(stderr, "maxtime: -E1 stopped %d ms in\n", (int) elapsed);
    err = 1;
  }
  while (mtime_monotonic() - t0 < 1000)
    Sleep(10);
  back_checkmirror(opt);
  if (!opt->state.stop) {
    fprintf(stderr, "maxtime: -E1 had not stopped %d ms in\n",
            (int) (mtime_monotonic() - t0));
    err = 1;
  }
  opt->state.stop = 0;
  opt->maxtime = 0;
  opt->log = saved_log;
  return st_verdict("maxtime self-test", err);
}

/* A disk-full abort logs its line and stops the mirror. */
static int st_iofatal(httrackp *opt, int argc, char **argv) {
  FILE *const saved_log = opt->log;
  int err = 0;

  if (argc < 1) {
    fprintf(stderr, "iofatal: needs a log file\n");
    return 1;
  }
  opt->log = fopen(argv[0], "wb");
  if (opt->log == NULL) {
    opt->log = saved_log;
    fprintf(stderr, "iofatal: cannot open %s\n", argv[0]);
    return 1;
  }
  hts_store_release_int(&opt->state.exit_xh, 0);
  hts_io_fatal_abort(opt);
  fclose(opt->log);
  opt->log = saved_log;
  if (hts_load_acquire_int(&opt->state.exit_xh) != -1) {
    fprintf(stderr, "iofatal: the mirror was not stopped\n");
    err = 1;
  }
  return st_verdict("iofatal self-test", err);
}

/* Make slot i one back_clean() finalizes and frees: READY, on disk, not HTML.
 */
static void st_busy_finalizable(lien_back *slot, int i) {
  slot->status = STATUS_READY;
  slot->finalized = 1;
  slot->locked = 0;
  slot->testmode = 0;
  slot->early_add = 0;
  slot->r.is_write = 1;
  slot->r.statuscode = HTTP_OK;
  slot->r.size = 0;
  slot->r.keep_alive = 0;
  strcpybuff(slot->r.contenttype, "image/png");
  snprintf(slot->url_fil, sizeof(slot->url_fil), "/busy%d.png", i);
  snprintf(slot->url_sav, sizeof(slot->url_sav), "busyslots-none/%d.png", i);
}

/* Which live slot holds adr/fil, found by a scan of every slot? */
static int st_busy_rescan_index(const struct_back *sback, const char *adr,
                                const char *fil) {
  int i;

  for (i = 0; i < sback->count; i++) {
    const lien_back *const slot = &sback->lnk[i];

    if (slot->status >= 0 && strcmp(slot->url_adr, adr) == 0 &&
        strcmp(slot->url_fil, fil) == 0)
      return i;
  }
  return -1;
}

/* The busy bitmap covers every slot in use through each status change, and
   back_wait() still reaches a slot past a bitmap word boundary. */
static int st_busyslots(httrackp *opt, int argc, char **argv) {
  /* three bitmap words, with slots on both sides of each boundary */
  enum { SLOTS = 130 };

  static const int probe[] = {0, 1, 63, 64, 65, 127, 128, SLOTS - 1};
  const int nprobe = (int) (sizeof(probe) / sizeof(probe[0]));
  struct_back *sback = back_new(opt, SLOTS);
  cache_back cache;
  hash_struct hash;
  htsblk r;
  int err = 0;
  int i, k, round;

#define CHECK(cond) ST_CHECK(err, cond)
  if (sback == NULL)
    return 77;
  opt->maxtime = 0;
  opt->maxsite = 0;
  opt->timeout = 0;
  opt->state.stop = 0;
  memset(&cache, 0, sizeof(cache));
  cache.hashtable = coucal_new(0);
  memset(&r, 0, sizeof(r));
  r.soc = INVALID_SOCKET;
  /* back_clean() looks a finalized slot up on the heap */
  hash_init(opt, &hash, opt->urlhack);
  opt->hash = &hash;

  CHECK(back_busy_consistent(sback, HTS_FALSE));
  back_clean(opt, &cache, sback);
  CHECK(back_busy_consistent(sback, HTS_TRUE));
  CHECK(back_available(sback) == SLOTS);

  for (round = 0; round < 4 && !err; round++) {
    /* take every slot, by search or by attach, then free the probes */
    for (i = 0; i < SLOTS; i++) {
      if ((i + round) % 2 == 0) {
        const int p = back_search_quick(sback);

        CHECK(p == i);
        sback->lnk[p].status = STATUS_WAIT_HEADERS;
      } else {
        back_slot_attach(sback, i, STATUS_CONNECTING, &r, "example.com", "/");
      }
      CHECK(back_busy_consistent(sback, HTS_FALSE));
    }
    CHECK(back_search_quick(sback) == -1);
    CHECK(back_available(sback) == 0);
    for (i = 0; i < SLOTS; i++)
      (void) back_clear_entry(&sback->lnk[i]);
    /* freed slots keep their bit until the next prune, and count as free */
    CHECK(back_available(sback) == SLOTS);
    back_clean(opt, &cache, sback);
    CHECK(back_busy_consistent(sback, HTS_TRUE));
    CHECK(back_available(sback) == SLOTS);
    CHECK(back_nsoc(sback) == 0);

    /* receiving slots with no socket: back_wait() must end each one */
    for (k = 0; k < nprobe; k++) {
      back_slot_attach(sback, probe[k], STATUS_TRANSFER, &r, "example.com",
                       "/");
    }
    CHECK(back_busy_consistent(sback, HTS_FALSE));
    CHECK(back_nsoc(sback) == nprobe);
    CHECK(back_available(sback) == SLOTS - nprobe);
    back_wait(sback, opt, &cache, 0);
    for (k = 0; k < nprobe; k++)
      CHECK(sback->lnk[probe[k]].status == STATUS_READY);
    CHECK(back_busy_consistent(sback, HTS_FALSE));
    for (k = 0; k < nprobe; k++)
      st_busy_finalizable(&sback->lnk[probe[k]], probe[k]);
    back_clean(opt, &cache, sback);
    CHECK(back_available(sback) == SLOTS);
    CHECK(back_busy_consistent(sback, HTS_TRUE));

    /* past an empty first word, the scan lands on the first slot of the next */
    back_slot_attach(sback, 64, STATUS_TRANSFER, &r, "example.com", "/");
    back_slot_attach(sback, 128, STATUS_TRANSFER, &r, "example.com", "/");
    CHECK(back_nsoc(sback) == 2);
    (void) back_clear_entry(&sback->lnk[64]);
    (void) back_clear_entry(&sback->lnk[128]);
    back_clean(opt, &cache, sback);
  }

  /* Each scan below must reach the last slot and the last bitmap word. */
  if (!err) {
    char fil[32];
    const int maxsoc = opt->maxsoc;
    const int background = HTS_STAT.stat_background;

    /* back_index_peek() finds every live slot, as a full rescan does */
    for (k = 0; k < nprobe; k++) {
      snprintf(fil, sizeof(fil), "/p%d", probe[k]);
      back_slot_attach(sback, probe[k], STATUS_TRANSFER, &r, "example.com",
                       fil);
    }
    for (k = 0; k < nprobe; k++) {
      snprintf(fil, sizeof(fil), "/p%d", probe[k]);
      CHECK(back_index_peek(opt, sback, "example.com", fil, "") == probe[k]);
      CHECK(st_busy_rescan_index(sback, "example.com", fil) == probe[k]);
    }
    CHECK(back_index_peek(opt, sback, "example.com", "/none", "") == -1);

    /* back_wait() times out each expired connect, and arms that check from
       the last slot alone */
    for (k = 0; k < nprobe; k++) {
      lien_back *const slot = &sback->lnk[probe[k]];

      slot->status = STATUS_CONNECTING;
      slot->timeout = probe[k] == SLOTS - 1 ? 1 : -1;
      slot->timeout_refresh = time_local() - 10;
    }
    back_wait(sback, opt, &cache, 0);
    for (k = 0; k < nprobe; k++) {
      CHECK(sback->lnk[probe[k]].status ==
            (probe[k] == SLOTS - 1 ? STATUS_READY : STATUS_CONNECTING));
      sback->lnk[probe[k]].timeout = 1;
    }
    back_wait(sback, opt, &cache, 0);
    for (k = 0; k < nprobe; k++) {
      CHECK(sback->lnk[probe[k]].status == STATUS_READY);
      CHECK(sback->lnk[probe[k]].r.statuscode == STATUSCODE_TIMEOUT);
    }

    /* back_clean() finalizes every finished slot */
    for (k = 0; k < nprobe; k++)
      st_busy_finalizable(&sback->lnk[probe[k]], probe[k]);
    back_clean(opt, &cache, sback);
    CHECK(HTS_STAT.stat_background == background + nprobe);
    CHECK(back_available(sback) == SLOTS);
    CHECK(back_busy_consistent(sback, HTS_TRUE));

    /* back_clean() ends every keep-alive slot with no connection */
    opt->maxsoc = 0;
    for (k = 0; k < nprobe; k++) {
      back_slot_attach(sback, probe[k], STATUS_ALIVE, &r, "example.com", "/");
      sback->lnk[probe[k]].finalized = 1; /* nothing to store */
    }
    back_clean(opt, &cache, sback);
    CHECK(back_available(sback) == SLOTS);

    /* back_clean() closes the connection of every ready slot */
    for (k = 0; k < nprobe; k++) {
      htsblk live = r;

      live.soc = (T_SOC) socket(AF_INET, SOCK_STREAM, 0);
      CHECK(live.soc != INVALID_SOCKET);
      back_slot_attach(sback, probe[k], STATUS_READY, &live, "example.com",
                       "/");
    }
    back_clean(opt, &cache, sback);
    for (k = 0; k < nprobe; k++) {
      CHECK(sback->lnk[probe[k]].status == STATUS_READY);
      CHECK(sback->lnk[probe[k]].r.soc == INVALID_SOCKET);
      st_busy_finalizable(&sback->lnk[probe[k]], probe[k]);
    }
    back_clean(opt, &cache, sback);
    CHECK(back_available(sback) == SLOTS);

    /* Past -c2, back_clean() drops keep-alive slots, here only the last two,
       until two connections are left: back_nsoc_overall() must count all
       four, and the cull must reach the last slot. */
    opt->maxsoc = 2;
    back_slot_attach(sback, 0, STATUS_TRANSFER, &r, "example.com", "/");
    back_slot_attach(sback, 64, STATUS_TRANSFER, &r, "example.com", "/");
    for (i = SLOTS - 2; i < SLOTS; i++) {
      htsblk live = r;
      lien_back *const slot = &sback->lnk[i];

      live.soc = (T_SOC) socket(AF_INET, SOCK_STREAM, 0);
      CHECK(live.soc != INVALID_SOCKET);
      live.keep_alive = 1;
      live.keep_alive_max = 10;
      live.keep_alive_t = 1000;
      back_slot_attach(sback, i, STATUS_ALIVE, &live, "example.com", "/");
      slot->ka_time_start = time_local();
      slot->finalized = 1;
    }
    back_clean(opt, &cache, sback);
    CHECK(sback->lnk[SLOTS - 2].status == STATUS_FREE);
    CHECK(sback->lnk[SLOTS - 1].status == STATUS_FREE);
    CHECK(back_nsoc(sback) == 2);
    st_busy_finalizable(&sback->lnk[0], 0);
    st_busy_finalizable(&sback->lnk[64], 64);
    back_clean(opt, &cache, sback);
    CHECK(back_available(sback) == SLOTS);
    CHECK(back_busy_consistent(sback, HTS_TRUE));
    opt->maxsoc = maxsoc;
  }
#undef CHECK
  back_free(&sback);
  hash_free(&hash);
  opt->hash = NULL;
  coucal_delete(&cache.hashtable);
  printf("busyslots self-test: %s\n", err ? "FAILED" : "OK");
  return err ? 1 : 0;
}

/* Attach slot p, with file and save name derived from p. */
static lien_back *st_busyscans_attach(struct_back *sback, int p, int status,
                                      const htsblk *r) {
  char fil[32];

  snprintf(fil, sizeof(fil), "/s%d.html", p);
  back_slot_attach(sback, p, status, r, "example.com", fil);
  snprintf(sback->lnk[p].url_sav, sizeof(sback->lnk[p].url_sav),
           "busyscans/s%d.html", p);
  return &sback->lnk[p];
}

/* Free every slot and prune the bitmap. */
static void st_busyscans_reset(httrackp *opt, cache_back *cache,
                               struct_back *sback) {
  int i;

  for (i = 0; i < sback->count; i++)
    (void) back_clear_entry(&sback->lnk[i]);
  back_clean(opt, cache, sback);
}

/* Attach a keep-alive slot for host, still within its keep-alive time. */
static lien_back *st_busyscans_idle(struct_back *sback, int p, const char *host,
                                    const htsblk *r) {
  lien_back *const slot = st_busyscans_attach(sback, p, STATUS_ALIVE, r);

  strcpybuff(slot->url_adr, host);
  slot->r.keep_alive_t = 1000;
  slot->ka_time_start = time_local();
  return slot;
}

/* -#test=busyscans <dir>: every scan the busy bitmap drives reaches each slot
   in use, through the last slot and the last bitmap word. */
static int st_busyscans(httrackp *opt, int argc, char **argv) {
  /* three bitmap words, with slots on both sides of each boundary */
  enum { SLOTS = 130 };

  /* a busy slot outside probe[] that each scan must skip by its status */
  enum { BYSTANDER = 2 };

  static const int probe[] = {0, 1, 63, 64, 65, 127, 128, SLOTS - 1};
  const int nprobe = (int) (sizeof(probe) / sizeof(probe[0]));
  static char body[] = "x";
  char base[HTS_URLMAXSIZE];
  char host[32];
  struct_back *sback;
  cache_back cache;
  hash_struct hash;
  htsblk r;
  LLint want = 0;
  int err = 0;
  int i, k;

#define CHECK(cond) ST_CHECK(err, cond)
  if (argc < 1)
    return 1;
  snprintf(base, sizeof(base), "%s/", argv[0]);
  StringCopy(opt->path_html_utf8, base);
  sback = back_new(opt, SLOTS);
  if (sback == NULL)
    return 77;
  opt->maxtime = 0;
  opt->maxsite = 0;
  opt->timeout = 0;
  opt->state.stop = 0;
  memset(&cache, 0, sizeof(cache));
  cache.hashtable = coucal_new(0);
  memset(&r, 0, sizeof(r));
  r.soc = INVALID_SOCKET;
  hash_init(opt, &hash, opt->urlhack);
  opt->hash = &hash;

  /* back_done_incache(), back_incache() and back_transferred() add up every
     slot, each weighted by its index so a missed one shows */
  for (k = 0; k < nprobe; k++) {
    lien_back *const slot =
        st_busyscans_attach(sback, probe[k], STATUS_READY, &r);

    slot->r.adr = body;
    slot->r.size = probe[k] + 1;
    want += probe[k] + 1;
  }
  CHECK(back_done_incache(sback) == nprobe);
  CHECK(back_incache(sback) == want);
  CHECK(back_transferred(0, sback) == 0);
  for (k = 0; k < nprobe; k++)
    sback->lnk[probe[k]].status = STATUS_TRANSFER;
  CHECK(back_done_incache(sback) == 0);
  CHECK(back_transferred(0, sback) == want);
  for (k = 0; k < nprobe; k++)
    sback->lnk[probe[k]].r.adr = NULL;
  CHECK(back_incache(sback) == 0);
  st_busyscans_reset(opt, &cache, sback);

  /* back_searchlive() finds the keep-alive slot of each host */
  for (k = 0; k < nprobe; k++) {
    snprintf(host, sizeof(host), "h%d.example", probe[k]);
    (void) st_busyscans_idle(sback, probe[k], host, &r);
  }
  st_busyscans_idle(sback, BYSTANDER, "h63.example", &r)->status =
      STATUS_TRANSFER;
  for (k = 0; k < nprobe; k++) {
    snprintf(host, sizeof(host), "h%d.example", probe[k]);
    CHECK(back_searchlive(opt, sback, host) == probe[k]);
  }
  CHECK(back_searchlive(opt, sback, "none.example") == -1);
  st_busyscans_reset(opt, &cache, sback);

  /* back_clean() spools every finished page it may not finalize */
  for (k = 0; k < nprobe; k++) {
    (void) st_busyscans_attach(sback, probe[k], STATUS_READY, &r);
  }
  back_clean(opt, &cache, sback);
  CHECK(coucal_nitems(sback->ready) == (size_t) nprobe);
  CHECK(back_done_incache(sback) == nprobe);

  /* When every slot is busy, back_index() takes the keep-alive slot at the end
   */
  for (i = 0; i < SLOTS - 1; i++) {
    CHECK(back_search_quick(sback) == i);
    sback->lnk[i].status = STATUS_WAIT_HEADERS;
  }
  st_busyscans_attach(sback, SLOTS - 1, STATUS_ALIVE, &r);
  CHECK(back_index(opt, sback, "example.com", "/s0.html",
                   "busyscans/s0.html") == SLOTS - 1);
  st_busyscans_reset(opt, &cache, sback);

  /* a user stop aborts every transfer */
  for (k = 0; k < nprobe; k++)
    st_busyscans_attach(sback, probe[k], STATUS_TRANSFER, &r);
  st_busyscans_attach(sback, BYSTANDER, STATUS_READY, &r)->locked = 1;
  hts_request_stop(opt, 0);
  back_wait(sback, opt, &cache, 0);
  opt->state.stop = 0;
  for (k = 0; k < nprobe; k++)
    CHECK(strcmp(sback->lnk[probe[k]].r.msg, "mirror stopped by user") == 0);
  CHECK(sback->lnk[BYSTANDER].r.msg[0] == '\0');
  st_busyscans_reset(opt, &cache, sback);

  /* a size cap past its grace aborts every transfer */
  {
    const LLint recv_was = hts_stat_recv_get();

    for (k = 0; k < nprobe; k++)
      st_busyscans_attach(sback, probe[k], STATUS_TRANSFER, &r);
    st_busyscans_attach(sback, BYSTANDER, STATUS_READY, &r)->locked = 1;
    opt->maxsite = 1000;
    hts_stat_recv_set(100000);
    back_wait(sback, opt, &cache, 0);
    opt->maxsite = 0;
    hts_stat_recv_set(recv_was);
    opt->state.stop = 0;
    for (k = 0; k < nprobe; k++)
      CHECK(strcmp(sback->lnk[probe[k]].r.msg, "Mirror Size Limit") == 0);
    CHECK(sback->lnk[BYSTANDER].r.msg[0] == '\0');
    st_busyscans_reset(opt, &cache, sback);
  }

  /* back_delete_all() sees a partial still being written in the last slot */
  {
    lien_back *const slot =
        st_busyscans_attach(sback, SLOTS - 1, STATUS_TRANSFER, &r);

    slot->r.is_write = 1;
    opt->abort_left_partial = HTS_FALSE;
    back_delete_all(opt, &cache, sback);
    CHECK(opt->abort_left_partial);
    opt->abort_left_partial = HTS_FALSE;
  }
#undef CHECK
  back_free(&sback);
  hash_free(&hash);
  opt->hash = NULL;
  coucal_delete(&cache.hashtable);
  printf("busyscans self-test: %s\n", err ? "FAILED" : "OK");
  return err ? 1 : 0;
}

/* -#test=delayedsettled: only a finished, empty slot counts as settled. */
static int st_delayedsettled(httrackp *opt, int argc, char **argv) {
  enum { SLOTS = 8, LIVE = 5 };

  static const int busy[] = {
      STATUS_ALIVE,        STATUS_FREE,       STATUS_TRANSFER,
      STATUS_CHUNK_CR,     STATUS_CHUNK_WAIT, STATUS_WAIT_HEADERS,
      STATUS_CONNECTING,   STATUS_WAIT_DNS,   STATUS_SSL_WAIT_HANDSHAKE,
      STATUS_FTP_TRANSFER, STATUS_FTP_READY};
  struct_back *sback;
  cache_back cache;
  hash_struct hash;
  htsblk live;
  lien_back *slot;
  int b, k;
  int err = 0;

#define CHECK(cond) ST_CHECK(err, cond)
  (void) argc;
  (void) argv;
  sback = back_new(opt, SLOTS);
  if (sback == NULL)
    return 77;
  opt->maxsoc = SLOTS;
  opt->timeout = 0;
  opt->rateout = 0;
  opt->state.stop = 0;
  memset(&cache, 0, sizeof(cache));
  cache.hashtable = coucal_new(0);
  hash_init(opt, &hash, opt->urlhack);
  opt->hash = &hash;

  /* a reused socket: the slot waits for its reply, which is no cache hit */
  memset(&live, 0, sizeof(live));
  live.soc = (T_SOC) socket(AF_INET, SOCK_STREAM, 0);
  CHECK(live.soc != INVALID_SOCKET);
  live.keep_alive = 1;
  live.keep_alive_max = 10;
  live.keep_alive_t = 1000;
  back_slot_attach(sback, LIVE, STATUS_ALIVE, &live, "example.com", "/");
  sback->lnk[LIVE].ka_time_start = time_local();
  sback->lnk[LIVE].finalized = 1;
  CHECK(back_add(sback, opt, &cache, "example.com", "/ka.png",
                 "ka.png.1.delayed", NULL, NULL, 0, HTS_FALSE, NULL) == 0);
  b = back_index(opt, sback, "example.com", "/ka.png", "ka.png.1.delayed");
  CHECK(b >= 0 && b != LIVE);
  if (b >= 0) {
    slot = &sback->lnk[b];
    CHECK(back_status(&sback->lnk[LIVE]) == STATUS_FREE);
    CHECK(slot->status == STATUS_CONNECTING);
    CHECK(slot->r.soc != INVALID_SOCKET);
    CHECK(slot->r.statuscode == STATUSCODE_INVALID);
    CHECK(!back_delayed_settled_empty(slot));
    back_delete(opt, &cache, sback, b);
  }

  /* a slot back_add() settled with no reply and no body */
  hts_request_stop(opt, 0);
  CHECK(back_add(sback, opt, &cache, "example.com", "/st.png",
                 "st.png.2.delayed", NULL, NULL, 0, HTS_FALSE, NULL) == 0);
  opt->state.stop = 0;
  b = back_index(opt, sback, "example.com", "/st.png", "st.png.2.delayed");
  CHECK(b >= 0);
  if (b >= 0) {
    static char body[] = "x";

    slot = &sback->lnk[b];
    CHECK(slot->status == STATUS_READY);
    CHECK(back_delayed_settled_empty(slot));

    slot->r.adr = body;
    CHECK(!back_delayed_settled_empty(slot));
    slot->r.adr = NULL;
    slot->r.statuscode = HTTP_OK;
    CHECK(!back_delayed_settled_empty(slot));
    slot->r.statuscode = STATUSCODE_INVALID;
    for (k = 0; k < (int) (sizeof(busy) / sizeof(busy[0])); k++) {
      slot->status = busy[k];
      CHECK(!back_delayed_settled_empty(slot));
    }
    slot->status = STATUS_READY;
    back_delete(opt, &cache, sback, b);
  }
#undef CHECK
  back_free(&sback);
  hash_free(&hash);
  opt->hash = NULL;
  coucal_delete(&cache.hashtable);
  printf("delayedsettled self-test: %s\n", err ? "FAILED" : "OK");
  return err ? 1 : 0;
}

const struct selftest_entry selftests_back[] = {
    {"iofatal", "<logfile>",
     "a disk-full abort logs its line and stops the mirror", st_iofatal},
    {"strerror", "",
     "a thread's error message is its own, never another thread's (#1697)",
     st_strerror},
    {"maxtime", "",
     "-E stops at its cap to the millisecond, not up to 1s early", st_maxtime},
    {"threadrunner", "",
     "a registered thread runner encloses each worker body exactly once",
     st_threadrunner},
    {"ftpworker", "", "an FTP worker's tail hands its backlog slot back",
     st_ftpworker},
    {"ftphandoff", "",
     "an FTP worker's slot handoff is ordered against the crawl thread",
     st_ftphandoff},
    {"backnew", "",
     "a backing table too big to allocate is a NULL, not an abort", st_backnew},
    {"recordoom", "",
     "a link whose strings cannot be allocated is dropped, not an abort",
     st_recordoom},
    {"mirrorcompleted", "",
     "a fresh opt has no mirror verdict, and copy_htsopt carries none",
     st_mirrorcompleted},
    {"transportfailures", "",
     "a failed transfer is counted apart from an answered error",
     st_transportfailures},
    {"threadwait", "", "htsthread_wait() joins threads spawned just before it",
     st_threadwait},
    {"relaxedcounter", "",
     "a 64-bit counter another thread writes reads back whole and in order",
     st_relaxedcounter},
    {"mutexlazyinit", "",
     "a lock built on first use serves every thread that raced for it",
     st_mutexlazyinit},
    {"backswap", "", "which backlog slots may be swapped to the ready table",
     st_backswap},
    {"delayedsettled", "",
     "a link on a reused keep-alive socket is not re-added as a cache hit",
     st_delayedsettled},
    {"busyscans", "<dir>",
     "each slot-table scan reaches every slot in use, up to the last one",
     st_busyscans},
    {"busyslots", "",
     "slot scans see every slot in use, through each status change",
     st_busyslots},
    {"spoolrecord", "",
     "a spooled slot keeps only its used bytes and refuses a bad length",
     st_spoolrecord},
    {"slotfail", "", "a failed slot clips a reason too long for its message",
     st_slotfail},
    {"statsrows", "",
     "the progress rows survive an empty table and show a known size of 0",
     st_statsrows},
    {"spoolwrite", "<dir>", "a failed spool write leaves no partial file",
     st_spoolwrite},
    {"spooldir", "<dir>", "~hts-tmp goes with the last spool entry, not before",
     st_spooldir},
    {"spoolfull", "<dir>", "a spooled entry waits for a free slot, kept whole",
     st_spoolfull},
    {"spoolmem", "<dir>", "spooled slots stay in memory up to a byte cap",
     st_spoolmem},
    {"spoolsweep", "<dir>", "startup removes the spool of a dead process only",
     st_spoolsweep},
    {"backstop", "",
     "a user stop drops the slots still waiting to connect (#1073)",
     st_backstop},
    {"pause", "", "randomized inter-file pause target self-test", st_pause},
    {"hostban", "",
     "a banned host's FTP slot is left to its worker, never ended here",
     st_hostban},
    {"hostbanlinks", "",
     "a banned host's links are cancelled, even with an address of 1020+ bytes",
     st_hostbanlinks},
    {NULL, NULL, NULL, NULL},
};
