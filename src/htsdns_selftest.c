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
  int calls; /* times the backend resolved this host */
} mock_host;

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
};

static mock_host *mock_find(const char *name) {
  for (size_t i = 0; i < sizeof(mock_hosts) / sizeof(mock_hosts[0]); i++) {
    if (strcmp(mock_hosts[i].name, name) == 0)
      return &mock_hosts[i];
  }
  return NULL;
}

static void mock_reset_calls(void) {
  for (size_t i = 0; i < sizeof(mock_hosts) / sizeof(mock_hosts[0]); i++)
    mock_hosts[i].calls = 0;
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

static int mock_getaddrinfo(const char *node, const char *service,
                            const struct addrinfo *hints,
                            struct addrinfo **res) {
  mock_host *const h = mock_find(node);
  const int want = (hints != NULL) ? hints->ai_family : PF_UNSPEC;
  struct addrinfo *head = NULL, *tail = NULL;

  (void) service;
  *res = NULL;
  if (h == NULL)
    return EAI_NONAME;
  h->calls++; /* a real backend hit; a cached host skips this */
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

static void mock_freeaddrinfo(struct addrinfo *res) {
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

    /* more than the cap: the kept list is clamped to HTS_MAXADDRNUM */
    CHECK(hts_dns_resolve_all(opt, "many.test", addrs, HTS_MAXADDRNUM, &err) ==
          HTS_MAXADDRNUM);

    /* family filter still applies through the list path */
    IPV6_resolver = 1;
    CHECK(hts_dns_resolve_all(opt, "dual4.test", addrs, HTS_MAXADDRNUM, &err) ==
          1);
    CHECK(SOCaddr_sinfamily(addrs[0]) == AF_INET);
    IPV6_resolver = 0;
  }

  hts_dns_set_resolver_backend(NULL);
  return failures;
}

#else

int dns_selftests(httrackp *opt) {
  (void) opt;
  return 0; /* resolver seam only exists in the IPv6 build */
}

#endif
