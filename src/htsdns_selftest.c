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
/* File: htsdns_selftest.c subroutines:                         */
/*       in-process self-test for the DNS resolver and cache    */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Routes the resolver through a scripted getaddrinfo (hts_resolver_backend)
   instead of the network, so resolution and the DNS cache are testable for a
   fixed set of scenarios (IPv4/IPv6/dual-stack, errors, family filter,
   cache reuse) with no live DNS. */

#define HTS_INTERNAL_BYTECODE

#include "htsdns_selftest.h"

#include "htscore.h"
#include "htslib.h"
#include "htsnet.h"

#include <stdio.h>
#include <string.h>

#if HTS_INET6 != 0

/* IPV6_resolver: 0 = v4+v6, 1 = v4 only, 2 = v6 only (htscoremain -@i). */
extern int IPV6_resolver;

/* One scripted host: either a getaddrinfo error, or an ordered address list. */
typedef struct mock_addr {
  int family;             /* AF_INET / AF_INET6 */
  unsigned char addr[16]; /* 4 (v4) or 16 (v6) meaningful bytes */
} mock_addr;

typedef struct mock_host {
  const char *name;
  int gai_err; /* non-zero: getaddrinfo returns this */
  int naddr;
  mock_addr addr[6];
  int calls;   /* times the backend resolved this host */
  int slow_ms; /* non-zero: block this long, as a black-hole resolver would */
} mock_host;

/* Long enough to outlast the 1s --timeout the bounded resolve is checked
   against, short enough to keep the self-test quick. */
#define MOCK_SLOW_MS 3000

static mock_host mock_hosts[] = {
    {"v4only.test", 0, 1, {{AF_INET, {1, 2, 3, 4}}}, 0},
    {"v6only.test", 0, 1, {{AF_INET6, {0x20, 0x01, 0x0d, 0xb8, [15] = 1}}}, 0},
    /* dual stack, IPv6 first (RFC 6724 order) then IPv4 */
    {"dual.test",
     0,
     2,
     {{AF_INET6, {0x20, 0x01, 0x0d, 0xb8, [15] = 2}}, {AF_INET, {5, 6, 7, 8}}},
     0},
    /* dual stack, IPv4 first: distinguishes "keep the first address" from
       "prefer a family", so the selection contract is actually pinned. */
    {"dual4.test",
     0,
     2,
     {{AF_INET, {9, 10, 11, 12}},
      {AF_INET6, {0x20, 0x01, 0x0d, 0xb8, [15] = 3}}},
     0},
    /* more addresses than HTS_MAXADDRNUM: the list must clamp to the cap. */
    {"many.test",
     0,
     6,
     {{AF_INET, {10, 0, 0, 1}},
      {AF_INET, {10, 0, 0, 2}},
      {AF_INET, {10, 0, 0, 3}},
      {AF_INET, {10, 0, 0, 4}},
      {AF_INET, {10, 0, 0, 5}},
      {AF_INET, {10, 0, 0, 6}}},
     0},
    {"nodns.test", EAI_NONAME, 0, {{0}}, 0},
    /* resolves, but only well after --timeout: the #606 wedge */
    {"slow.test", 0, 1, {{AF_INET, {127, 0, 0, 9}}}, 0, MOCK_SLOW_MS},
};

/* Serializes mock_host bookkeeping: a timed-out resolve is abandoned, so its
   worker is still inside the backend while the test reads the counters. */
static htsmutex mock_lock = HTSMUTEX_INIT;
static int mock_finished = 0; /* backend calls that have returned */

static mock_host *mock_find(const char *name) {
  for (size_t i = 0; i < sizeof(mock_hosts) / sizeof(mock_hosts[0]); i++) {
    if (strcmp(mock_hosts[i].name, name) == 0)
      return &mock_hosts[i];
  }
  return NULL;
}

static void mock_reset_calls(void) {
  hts_mutexlock(&mock_lock);
  for (size_t i = 0; i < sizeof(mock_hosts) / sizeof(mock_hosts[0]); i++)
    mock_hosts[i].calls = 0;
  mock_finished = 0;
  hts_mutexrelease(&mock_lock);
}

static int mock_read_calls(const char *name) {
  int calls;

  hts_mutexlock(&mock_lock);
  calls = mock_find(name)->calls;
  hts_mutexrelease(&mock_lock);
  return calls;
}

/* Wait for n backend calls to return, ordering their writes against ours. */
static void mock_wait_finished(int n) {
  for (;;) {
    hts_boolean done;

    hts_mutexlock(&mock_lock);
    done = (mock_finished >= n) ? HTS_TRUE : HTS_FALSE;
    hts_mutexrelease(&mock_lock);
    if (done)
      break;
    Sleep(10);
  }
}

/* Build one addrinfo node owning its sockaddr (freed by mock_freeaddrinfo). */
static struct addrinfo *mock_mkai(const mock_addr *a) {
  struct addrinfo *ai = calloct(1, sizeof(*ai));

  ai->ai_family = a->family;
  if (a->family == AF_INET) {
    struct sockaddr_in *sin = calloct(1, sizeof(*sin));

    sin->sin_family = AF_INET;
    memcpy(&sin->sin_addr, a->addr, 4);
    ai->ai_addr = (struct sockaddr *) sin;
    ai->ai_addrlen = sizeof(*sin);
  } else {
    struct sockaddr_in6 *sin6 = calloct(1, sizeof(*sin6));

    sin6->sin6_family = AF_INET6;
    memcpy(&sin6->sin6_addr, a->addr, 16);
    ai->ai_addr = (struct sockaddr *) sin6;
    ai->ai_addrlen = sizeof(*sin6);
  }
  return ai;
}

static int HTS_RESOLVER_CALL mock_getaddrinfo_(const char *node,
                                               const char *service,
                                               const struct addrinfo *hints,
                                               struct addrinfo **res) {
  mock_host *const h = mock_find(node);
  const int want = (hints != NULL) ? hints->ai_family : PF_UNSPEC;
  struct addrinfo *head = NULL, *tail = NULL;

  (void) service;
  *res = NULL;
  if (h == NULL)
    return EAI_NONAME;
  hts_mutexlock(&mock_lock);
  h->calls++; /* a real backend hit; a cached host skips this */
  hts_mutexrelease(&mock_lock);
  if (h->slow_ms != 0)
    Sleep(h->slow_ms);
  if (h->gai_err != 0)
    return h->gai_err;
  for (int i = 0; i < h->naddr; i++) {
    if (want != PF_UNSPEC && want != h->addr[i].family)
      continue; /* honor the requested family (v4/v6 only) */
    struct addrinfo *const ai = mock_mkai(&h->addr[i]);

    if (head == NULL)
      head = ai;
    else
      tail->ai_next = ai;
    tail = ai;
  }
  if (head == NULL)
    return EAI_NONAME; /* filtered to empty, as the libc resolver does */
  *res = head;
  return 0;
}

static int HTS_RESOLVER_CALL mock_getaddrinfo(const char *node,
                                              const char *service,
                                              const struct addrinfo *hints,
                                              struct addrinfo **res) {
  const int ret = mock_getaddrinfo_(node, service, hints, res);

  hts_mutexlock(&mock_lock);
  mock_finished++;
  hts_mutexrelease(&mock_lock);
  return ret;
}

static void HTS_RESOLVER_CALL mock_freeaddrinfo(struct addrinfo *res) {
  while (res != NULL) {
    struct addrinfo *const next = res->ai_next;

    freet(res->ai_addr);
    freet(res);
    res = next;
  }
}

static const hts_resolver_backend mock_backend = {mock_getaddrinfo,
                                                  mock_freeaddrinfo};

static int failures = 0;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      failures++;                                                              \
      fprintf(stderr, "dns-selftest: FAIL at %s:%d: %s\n", __FILE__, __LINE__, \
              #cond);                                                          \
    }                                                                          \
  } while (0)

/* Resolve via the uncached entry point; return the address family, or
   AF_UNSPEC if the host did not resolve. */
static int resolve_family_nocache(const char *host) {
  SOCaddr addr;
  const char *err = NULL;

  if (hts_dns_resolve_nocache2(host, &addr, &err) == NULL)
    return AF_UNSPEC;
  return SOCaddr_sinfamily(addr);
}

int dns_selftests(httrackp *opt) {
  failures = 0;
  hts_dns_set_resolver_backend(&mock_backend);

  /* IPv4-only / IPv6-only hosts map to the right family. */
  IPV6_resolver = 0;
  CHECK(resolve_family_nocache("v4only.test") == AF_INET);
  CHECK(resolve_family_nocache("v6only.test") == AF_INET6);

  /* Dual-stack: the single-address API returns the *first* resolved address.
     Both orderings pin selection by position, not a family preference. The
     multi-address API (resolve_all, below) exposes the whole list. */
  CHECK(resolve_family_nocache("dual.test") == AF_INET6); /* v6 listed first */
  CHECK(resolve_family_nocache("dual4.test") == AF_INET); /* v4 listed first */

  /* Unknown host does not resolve. */
  CHECK(resolve_family_nocache("nodns.test") == AF_UNSPEC);

  /* Family filter (-@i4 / -@i6) selects v4 / v6 out of the dual-stack host. */
  IPV6_resolver = 1;
  CHECK(resolve_family_nocache("dual.test") == AF_INET);
  IPV6_resolver = 2;
  CHECK(resolve_family_nocache("dual.test") == AF_INET6);
  IPV6_resolver = 0;

  /* Cached driver resolves a host once and reuses the *same* address. */
  mock_reset_calls();
  {
    SOCaddr a1, a2;
    char ip1[64], ip2[64];
    const char *err = NULL;

    CHECK(hts_dns_resolve2(opt, "v4only.test", &a1, &err) != NULL);
    CHECK(hts_dns_resolve2(opt, "v4only.test", &a2, &err) != NULL);
    CHECK(mock_find("v4only.test")->calls == 1);
    /* the cache returns the right address, not merely a hit for the key */
    SOCaddr_inetntoa(ip1, sizeof(ip1), a1);
    SOCaddr_inetntoa(ip2, sizeof(ip2), a2);
    CHECK(strcmp(ip1, "1.2.3.4") == 0);
    CHECK(strcmp(ip1, ip2) == 0);
  }

  /* A negative result is cached too: a second lookup does not re-resolve. */
  {
    SOCaddr a1, a2;
    const char *err = NULL;

    CHECK(hts_dns_resolve2(opt, "nodns.test", &a1, &err) == NULL);
    CHECK(hts_dns_resolve2(opt, "nodns.test", &a2, &err) == NULL);
    CHECK(mock_find("nodns.test")->calls == 1); /* resolved once, then cached */
  }

  /* Multi-address resolution: count and order are the connect-fallback
     contract. A dead first address is retried against the next, so both must be
     exact. */
  mock_reset_calls();
  {
    SOCaddr addrs[HTS_MAXADDRNUM];
    char ip[64];
    const char *err = NULL;

    /* dual-stack, in resolver order: [0]=v6, [1]=v4 */
    CHECK(hts_dns_resolve_all(opt, "dual.test", addrs, HTS_MAXADDRNUM, &err) ==
          2);
    CHECK(SOCaddr_sinfamily(addrs[0]) == AF_INET6);
    CHECK(SOCaddr_sinfamily(addrs[1]) == AF_INET);
    SOCaddr_inetntoa(ip, sizeof(ip), addrs[1]);
    CHECK(strcmp(ip, "5.6.7.8") == 0);
    CHECK(mock_find("dual.test")->calls ==
          1); /* one backend hit for the list */

    /* single-address host: count 1 */
    CHECK(hts_dns_resolve_all(opt, "v4only.test", addrs, HTS_MAXADDRNUM,
                              &err) == 1);
    SOCaddr_inetntoa(ip, sizeof(ip), addrs[0]);
    CHECK(strcmp(ip, "1.2.3.4") == 0);

    /* does-not-resolve: count 0 (negative), no addresses */
    CHECK(hts_dns_resolve_all(opt, "nodns.test", addrs, HTS_MAXADDRNUM, &err) ==
          0);

    /* more than the cap: the kept list is clamped to HTS_MAXADDRNUM, keeping
       the FIRST addresses in resolver order (not some other window) */
    CHECK(hts_dns_resolve_all(opt, "many.test", addrs, HTS_MAXADDRNUM, &err) ==
          HTS_MAXADDRNUM);
    SOCaddr_inetntoa(ip, sizeof(ip), addrs[0]);
    CHECK(strcmp(ip, "10.0.0.1") == 0);
    SOCaddr_inetntoa(ip, sizeof(ip), addrs[HTS_MAXADDRNUM - 1]);
    CHECK(strcmp(ip, "10.0.0.4") == 0);

    /* family filter still applies through the list path */
    IPV6_resolver = 1;
    CHECK(hts_dns_resolve_all(opt, "dual4.test", addrs, HTS_MAXADDRNUM, &err) ==
          1);
    CHECK(SOCaddr_sinfamily(addrs[0]) == AF_INET);
    IPV6_resolver = 0;
  }

  /* "host:port" resolves as the bare host: the cache/connect path strips
     ":port" before both the backend and the cache key (#181). */
  mock_reset_calls();
  {
    SOCaddr a;
    char ip[64];
    const char *err = NULL;

    /* cache miss: the backend is hit with the stripped host, not "host:port" */
    CHECK(hts_dns_resolve2(opt, "v6only.test:8080", &a, &err) != NULL);
    CHECK(mock_find("v6only.test")->calls == 1);
    /* the bare host shares that one cache entry (stripped key) */
    CHECK(hts_dns_resolve2(opt, "v6only.test", &a, &err) != NULL);
    CHECK(mock_find("v6only.test")->calls == 1);
    /* a port variant of an already-cached host also hits, right address */
    CHECK(hts_dns_resolve2(opt, "v4only.test:1234", &a, &err) != NULL);
    SOCaddr_inetntoa(ip, sizeof(ip), a);
    CHECK(strcmp(ip, "1.2.3.4") == 0);
  }

  /* newhttp_addr() must connect to the addr_index-th address, not always the
     first: this is what back_connect_next relies on to reach the fallback. */
  {
    htsblk r;
    int count = -1;
    T_SOC s;

    hts_init_htsblk(&r);
    s = newhttp_addr(opt, "dual.test", &r, 80, 0, 0, &count);
    CHECK(count == 2);
    CHECK(SOCaddr_sinfamily(r.address) == AF_INET6); /* index 0 = v6 */
    if (s != INVALID_SOCKET)
      deletesoc(s);

    hts_init_htsblk(&r);
    count = -1;
    s = newhttp_addr(opt, "dual.test", &r, 80, 0, 1, &count);
    CHECK(count == 2);
    CHECK(SOCaddr_sinfamily(r.address) == AF_INET); /* index 1 = v4 */
    if (s != INVALID_SOCKET)
      deletesoc(s);

    /* out-of-range index: no address selected (address stays unset) */
    hts_init_htsblk(&r);
    s = newhttp_addr(opt, "dual.test", &r, 80, 0, 2, NULL);
    CHECK(s == INVALID_SOCKET);
    if (s != INVALID_SOCKET)
      deletesoc(s);
  }

  /* A URL's ":port" (port == -1, so newhttp_addr parses it) outside 1..65535
     must refuse the link, not fold into range and connect elsewhere (#614).
     *addr_count is the discriminator: 0 only if the port was refused before the
     resolve, still 2 for one merely truncated or defaulted to 80. */
  {
    /* "dual.test:" is an empty port, which WHATWG and curl both accept as
       meaning the default: it must keep resolving, not be refused as garbage */
    static const char *const good[] = {"dual.test:1",    "dual.test:80",
                                       "dual.test:8080", "dual.test:65535",
                                       "dual.test:080",  "dual.test:"};
    /* 65616 and 4294967376 are load-bearing: both wrap to a plausible 80 */
    static const char *const bad[] = {
        "dual.test:0",          "dual.test:65536",      "dual.test:65616",
        "dual.test:99999",      "dual.test:2147483648", "dual.test:4294967296",
        "dual.test:4294967376", "dual.test:-1",         "dual.test:-23437",
        "dual.test:80x",        "dual.test:+80",        "dual.test: 80",
        "dual.test:0x50"};
    size_t k;

    for (k = 0; k < sizeof(good) / sizeof(good[0]); k++) {
      htsblk r;
      int count = -1;
      T_SOC s;

      hts_init_htsblk(&r);
      s = newhttp_addr(opt, good[k], &r, -1, 0, 0, &count);
      CHECK(count == 2); /* accepted: reached the resolve */
      if (s != INVALID_SOCKET)
        deletesoc(s);
    }

    for (k = 0; k < sizeof(bad) / sizeof(bad[0]); k++) {
      htsblk r;
      int count = -1;
      T_SOC s;

      hts_init_htsblk(&r);
      s = newhttp_addr(opt, bad[k], &r, -1, 0, 0, &count);
      CHECK(s == INVALID_SOCKET);
      CHECK(count == 0); /* refused before resolving, not a failed connect */
      CHECK(strstr(r.msg, "Invalid port") != NULL);
      if (s != INVALID_SOCKET)
        deletesoc(s);
    }
  }

  /* Connect-fallback decision (consumer of the multi-address list): when a
     stuck connect should abandon the current address for the next one. */
  {
    /* no fallback for the last/only candidate, whatever the elapsed time */
    CHECK(back_connect_fallback_due(0, 1, 9999, 120) == 0);
    CHECK(back_connect_fallback_due(1, 2, 9999, 120) == 0);
    CHECK(back_connect_fallback_due(3, 4, 9999, 120) == 0);
    /* fallback available: wait the per-candidate deadline (cap 10s here) */
    CHECK(back_connect_fallback_due(0, 2, 9, 120) == 0);
    CHECK(back_connect_fallback_due(0, 2, 10, 120) == 1);
    CHECK(back_connect_fallback_due(2, 4, 10, 120) == 1);
    /* a shorter slot timeout shortens the deadline (min(timeout, cap)) */
    CHECK(back_connect_fallback_due(0, 2, 4, 5) == 0);
    CHECK(back_connect_fallback_due(0, 2, 5, 5) == 1);
    /* no timeout management: never force a fallback */
    CHECK(back_connect_fallback_due(0, 2, 9999, 0) == 0);
  }

  hts_dns_set_resolver_backend(NULL);
  return failures;
}

/* Probes how long acquiring opt->state.lock takes while a resolve is in
   flight. hts_has_stopped() takes that same lock and mutates nothing, and the
   API promises it stays callable from another thread during a mirror. */
typedef struct lock_probe {
  httrackp *opt;
  htsmutex lock;
  hts_boolean done;
  TStamp blocked_ms;
} lock_probe;

static void lock_probe_thread(void *arg) {
  lock_probe *const p = (lock_probe *) arg;
  TStamp start;

  Sleep(MOCK_SLOW_MS / 10); /* let the resolve get under way first */
  start = mtime_local();
  (void) hts_has_stopped(p->opt);
  hts_mutexlock(&p->lock);
  p->blocked_ms = mtime_local() - start;
  p->done = HTS_TRUE;
  hts_mutexrelease(&p->lock);
}

int dns_timeout_selftests(httrackp *opt) {
  SOCaddr addrs[HTS_MAXADDRNUM];
  const char *err = NULL;
  lock_probe probe;
  TStamp start, elapsed;
  int count;

  failures = 0;
  hts_dns_set_resolver_backend(&mock_backend);
  IPV6_resolver = 0;
  mock_reset_calls();
  opt->timeout = 1; /* the bound under test */

  memset(&probe, 0, sizeof(probe));
  probe.opt = opt;
  probe.lock = HTSMUTEX_INIT;
  hts_mutexinit(&probe.lock);
  CHECK(hts_newthread(lock_probe_thread, &probe) == 0);

  start = mtime_local();
  count = hts_dns_resolve_all(opt, "slow.test", addrs, HTS_MAXADDRNUM, &err);
  elapsed = mtime_local() - start;

  /* the resolve returns on opt->timeout, not when the resolver deigns to
     answer: this is what lets --max-time and --timeout fire (#606). The bound
     is derived from opt->timeout, never from the mock's sleep, or a resolve
     that ignored opt->timeout would still pass under the mock. */
  CHECK(elapsed < (TStamp) opt->timeout * 1000 + 500);
  CHECK(count == 0); /* a timeout is reported as "does not resolve" */

  /* state.lock is not held across the resolve; a concurrent stop query, which
     the mirror API promises stays live, is not blocked behind it */
  for (;;) {
    hts_boolean done;
    TStamp blocked;

    hts_mutexlock(&probe.lock);
    done = probe.done;
    blocked = probe.blocked_ms;
    hts_mutexrelease(&probe.lock);
    if (done) {
      CHECK(blocked < 500);
      break;
    }
    Sleep(20);
  }
  hts_mutexfree(&probe.lock);

  /* a timeout is not an answer, so it must not be negative-cached: the host is
     resolved again rather than written off for the rest of the crawl */
  CHECK(mock_read_calls("slow.test") == 1);
  count = hts_dns_resolve_all(opt, "slow.test", addrs, HTS_MAXADDRNUM, &err);
  CHECK(count == 0);
  CHECK(mock_read_calls("slow.test") == 2); /* re-resolved, not cached */

  /* Both resolves were abandoned mid-backend; wait for their workers to leave
     it before returning. The backend stays installed: an abandoned worker
     still reads it (to free its addrinfo) after the last call returns. */
  mock_wait_finished(2);
  return failures;
}

#else

int dns_selftests(httrackp *opt) {
  (void) opt;
  return 0; /* resolver seam only exists in the IPv6 build */
}

#endif
