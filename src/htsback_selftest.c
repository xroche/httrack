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

#include "htsselftest_int.h"

/* hts_mirror_completed() answers HTS_DEFAULT until a mirror has run, so a GUI
   whose hts_main2() bailed out early does not read that as an abort. The
   verdict is live state, so copy_htsopt() must not carry it between opts. */
static int st_mirrorcompleted(httrackp *opt, int argc, char **argv) {
  httrackp *from = hts_create_opt();
  httrackp *to = hts_create_opt();
  int err = 0;

  (void) opt;
  (void) argc;
  (void) argv;

  if (hts_mirror_completed(from) != HTS_DEFAULT)
    err = 1;
  if (hts_mirror_completed(to) != HTS_DEFAULT)
    err = 1;

  /* A verdict never travels onto an opt that has none. */
  from->mirror_completed = HTS_TRUE;
  to->mirror_completed = HTS_DEFAULT;
  copy_htsopt(from, to);
  if (hts_mirror_completed(to) != HTS_DEFAULT)
    err = 1;
  /* A verdict never overwrites one the target earned itself, which is what a
     copy_htsopt change would silently break. */
  from->mirror_completed = HTS_FALSE;
  to->mirror_completed = HTS_TRUE;
  copy_htsopt(from, to);
  if (hts_mirror_completed(to) != HTS_TRUE)
    err = 1;

  hts_free_opt(from);
  hts_free_opt(to);
  printf("mirror-completed: %s\n", err ? "FAIL" : "OK");
  return err;
}

/* stat_errors reads the same for a 404 as for a timeout. A front end deciding
   whether a resume is owed needs the failed transfers apart, and per mirror, so
   copy_htsopt() must not carry the count between opts. */
static int st_transportfailures(httrackp *opt, int argc, char **argv) {
  httrackp *from = hts_create_opt();
  httrackp *to = hts_create_opt();
  const hts_stat_struct *stats;
  int err = 0;

  (void) opt;
  (void) argc;
  (void) argv;

  if (from->transport_failures != 0 || to->transport_failures != 0)
    err = 1;

  /* Published beside stat_errors, which is where a front end reads it. */
  to->transport_failures = 7;
  stats = hts_get_stats(to);
  if (stats == NULL || stats->stat_transport_failures != 7)
    err = 1;

  /* A count never travels onto another opt. */
  from->transport_failures = 3;
  to->transport_failures = 0;
  copy_htsopt(from, to);
  if (to->transport_failures != 0)
    err = 1;

  hts_free_opt(from);
  hts_free_opt(to);
  printf("transport-failures: %s\n", err ? "FAIL" : "OK");
  return err;
}

static int st_pause(httrackp *opt, int argc, char **argv) {
  int err = 0, i, seen_low = 0, seen_high = 0;

  (void) opt;
  (void) argc;
  (void) argv;
  /* Consecutive-ms seeds (production shape: launch timestamps a few ms apart)
     must stay in range and spread, not collapse to a bound -- worst case for a
     weak low-bit mixer. */
  for (i = 0; i < 10000; i++) {
    int t = hts_pause_target_ms((TStamp) (1719500000000LL + i), 5000, 10000);

    if (t < 5000 || t > 10000)
      err = 1;
    seen_low |= (t < 6000);
    seen_high |= (t > 9000);
  }
  if (!seen_low || !seen_high)
    err = 1;
  if (hts_pause_target_ms(12345, 8000, 8000) != 8000) /* equal bounds = fixed */
    err = 1;
  /* deterministic: a seed yields the same target even after an intervening call
     with another seed (no global PRNG state to perturb it) */
  {
    int a = hts_pause_target_ms(99, 5000, 10000);

    (void) hts_pause_target_ms(54321, 5000, 10000);
    if (hts_pause_target_ms(99, 5000, 10000) != a)
      err = 1;
  }
  printf("pause: %s\n", err ? "FAIL" : "OK");
  return err;
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

static int st_backswap(httrackp *opt, int argc, char **argv) {
  (void) opt;
  (void) argc;
  (void) argv;
  return back_selftest_slot_swap();
}

/* TEST-NET-1: routed nowhere, so a connect to it stays pending. */
#define ST_BACKSTOP_DEADHOST "192.0.2.1"

/* Open a connect that is still in flight, or INVALID_SOCKET where the network
   answers for the blackhole instead of dropping. */
static T_SOC st_backstop_pending(httrackp *opt, htsblk *r) {
  T_SOC soc;

  hts_init_htsblk(r);
  soc = newhttp_addr(opt, ST_BACKSTOP_DEADHOST, r, 80, 0, 0, NULL);
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

  back->r = *r;
  back->r.location = back->location_buffer;
  back->status = status;
  back->timeout = -1; /* only the stop may end these slots */
  back->rateout = -1;
  /* poisoned, so the expected outcome cannot be the value the slot came with */
  back->r.statuscode = STATUSCODE_TIMEOUT;
  strcpybuff(back->r.msg, "untouched");
  strcpybuff(back->url_adr, ST_BACKSTOP_DEADHOST);
  strcpybuff(back->url_fil, "/stalled.html");
  /* address list already probed, as it is for a live connect: re-probing it
     would resolve, and the fd churn would spoil the closed-socket check */
  sback->connect_fallback[p].addr_count = 1;
}

/* A network that rejects TEST-NET-1 instead of dropping it answers in ms. */
#define ST_BACKSTOP_SETTLE_MS 500

/* Bytes the size cap allows. back_maxsize_grace() spares running transfers a
   further tenth, so the past-grace case below doubles it. */
#define ST_BACKSTOP_CAP 1000

/* Refill every slot: a fresh pending connect for each state that owns a socket,
   plus the poison the assertions read back. False if the blackhole answered. */
static hts_boolean st_backstop_arm(httrackp *opt, struct_back *sback,
                                   const int *status, htsblk *r, int slots,
                                   int dnsslot) {
  int i;

  for (i = 0; i < slots; i++) {
    deletehttp(&sback->lnk[i].r); /* whatever the previous round left */
    if (i == dnsslot) {
      hts_init_htsblk(&r[i]);
    } else if (st_backstop_pending(opt, &r[i]) == INVALID_SOCKET) {
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
    if (i != dnsslot && check_socket_connect(sback->lnk[i].r.soc) != 0) {
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
    SLOT_DNS = 0,
    SLOT_CONNECT,
    SLOT_SSL,
    SLOT_HEADERS,
    SLOT_XFER,
    SLOT_CHUNK,
    SLOT_FTP,
    SLOTS
  };

  static const int status[SLOTS] = {
      STATUS_WAIT_DNS,     STATUS_CONNECTING, STATUS_SSL_WAIT_HANDSHAKE,
      STATUS_WAIT_HEADERS, STATUS_TRANSFER,   STATUS_CHUNK_WAIT,
      STATUS_FTP_TRANSFER};
  T_SOC swept[SLOTS];
  htsblk r[SLOTS];
  struct_back *sback = NULL;
  cache_back cache;
  lien_back *back;
  int err = 0;
  int skipped = 0;
  int round;
  int i;

  (void) argc;
  (void) argv;

  /* no quota may fire instead of the stop, and no slot may time out */
  opt->maxtime = 0;
  opt->maxsite = 0;
  opt->timeout = 0;
  opt->state.stop = 0;

  memset(&cache, 0, sizeof(cache));

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("  FAIL line %d: %s\n", __LINE__, #cond);                         \
      err = 1;                                                                 \
    }                                                                          \
  } while (0)

  /* before the cleanup label has anything to free */
  sback = back_new(opt, SLOTS);
  if (sback == NULL)
    return 77;
  cache.hashtable = coucal_new(0);
  back = sback->lnk;
  if (!st_backstop_arm(opt, sback, status, r, SLOTS, SLOT_DNS)) {
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
    if (!st_backstop_arm(opt, sback, status, r, SLOTS, SLOT_DNS)) {
      skipped = 1;
      goto cleanup;
    }
    for (i = 0; i < SLOTS; i++)
      swept[i] = r[i].soc;
    hts_request_stop(opt, 0);

    back_wait(sback, opt, &cache, 0);

    /* every slot but the FTP one is gone, its socket closed rather than merely
       forgotten, and reported as fatal so the link is not queued again */
    for (i = SLOT_DNS; i < SLOT_FTP; i++) {
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
    if (!st_backstop_arm(opt, sback, status, r, SLOTS, SLOT_DNS)) {
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
    for (i = SLOT_DNS; i < SLOT_HEADERS; i++) {
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
      if (!st_backstop_arm(opt, sback, status, r, SLOTS, SLOT_DNS)) {
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
    if (!st_backstop_arm(opt, sback, status, r, SLOTS, SLOT_DNS)) {
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

  printf("backstop self-test: %s\n", err ? "FAIL" : "OK");
  return err;
}

/* back_new() sizes its slot table from -cN, so a user can ask for one that
   does not fit. The contract is a NULL return, which httpmirror() turns into a
   log line and a stopped mirror, where an abort loses that message. */
static int st_backnew(httrackp *opt, int argc, char **argv) {
  /* Too big to serve without the allocator asking the kernel for more. */
  enum { slots = 4096 };

  struct_back *sback;

  (void) argc;
  (void) argv;

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

static int st_threadwait(httrackp *opt, int argc, char **argv) {
  int err = 0;
  int i, round;

  (void) opt;
  (void) argc;
  (void) argv;

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

  printf("threadwait self-test: %s\n", err ? "FAIL" : "OK");
  return err;
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

  (void) argc;
  (void) argv;

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

  printf("threadrunner self-test: %s\n", err ? "FAIL" : "OK");
  return err;
}

static int st_ftpworker(httrackp *opt, int argc, char **argv) {
  const int err = ftp_worker_selftests();

  (void) opt;
  (void) argc;
  (void) argv;
  printf("ftp-worker-selftest: %s\n", err ? "FAIL" : "OK");
  return err;
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

  (void) opt;
  (void) argc;
  (void) argv;

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

  printf("strerror self-test: %s\n", err ? "FAIL" : "OK");
  return err;
}

/* ------------------------------------------------------------ */
/* Registry: this module's tests, in the order -#test lists them. */
/* ------------------------------------------------------------ */

const struct selftest_entry selftests_back[] = {
    {"strerror", "",
     "a thread's error message is its own, never another thread's (#1697)",
     st_strerror},
    {"threadrunner", "",
     "a registered thread runner encloses each worker body exactly once",
     st_threadrunner},
    {"ftpworker", "", "an FTP worker's tail hands its backlog slot back",
     st_ftpworker},
    {"backnew", "",
     "a backing table too big to allocate is a NULL, not an abort", st_backnew},
    {"mirrorcompleted", "",
     "a fresh opt has no mirror verdict, and copy_htsopt carries none",
     st_mirrorcompleted},
    {"transportfailures", "",
     "a failed transfer is counted apart from an answered error",
     st_transportfailures},
    {"threadwait", "", "htsthread_wait() joins threads spawned just before it",
     st_threadwait},
    {"backswap", "", "which backlog slots may be swapped to the ready table",
     st_backswap},
    {"backstop", "",
     "a user stop drops the slots still waiting to connect (#1073)",
     st_backstop},
    {"pause", "", "randomized inter-file pause target self-test", st_pause},
    {NULL, NULL, NULL, NULL},
};
