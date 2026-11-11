/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998 Xavier Roche and other contributors

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
/* File: htsdns.c subroutines:                                  */
/* DNS cache and host name resolution, declared in htslib.h     */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htscore.h"
#include "htsnet.h"
#include "htsthread.h"
#include "htscrashtest.h"

#include <string.h>

/* 0 = v4+v6, 1 = v4 only, 2 = v6 only (htscoremain -@i) */
int IPV6_resolver = 0;

// Free a DNS cache record (coucal value handler).
static void hts_cache_value_free(coucal_opaque arg, coucal_value value) {
  void *record = value.ptr;

  (void) arg;
  freet(record);
}

// opt's DNS cache hashtable, created on first use. Records (t_dnscache*) are
// owned by the table and freed by hts_cache_value_free on coucal_delete.
coucal hts_cache(httrackp *opt) {
  assertf(opt != NULL);
  if (opt->state.dns_cache == NULL) {
    coucal cache = coucal_new(0);

    coucal_set_name(cache, "dns_cache");
    coucal_value_set_value_handler(cache, hts_cache_value_free, NULL);
    opt->state.dns_cache = cache;
  }
  assertf(opt->state.dns_cache != NULL);
  return opt->state.dns_cache;
}

/* First lifetime of a negative DNS answer. Unlocked because only the
   self-test writes it, from the single thread that then resolves. */
static int hts_dns_negative_ttl_ms = HTS_DNS_NEGATIVE_TTL_MS;

void hts_dns_set_negative_ttl_ms(int ms) {
  hts_dns_negative_ttl_ms = ms > 0 ? ms : 1; /* the doubling needs a floor */
}

int hts_dns_negative_failures(httrackp *opt, const char *host) {
  void *ptr;
  int failures = 0;

  hts_mutexlock(&opt->state.lock);
  if (coucal_read_pvoid(hts_cache(opt), host, &ptr))
    failures = ((const t_dnscache *) ptr)->failures;
  hts_mutexrelease(&opt->state.lock);
  return failures;
}

void hts_dns_test_move_clock(httrackp *opt, const char *host, TStamp ms) {
  void *ptr;

  hts_mutexlock(&opt->state.lock);
  if (coucal_read_pvoid(hts_cache(opt), host, &ptr)) {
    t_dnscache *const record = (t_dnscache *) ptr;

    if (record->expiry != 0) { /* a positive record carries no stamps */
      record->expiry -= ms;
      record->stored -= ms;
    }
  }
  hts_mutexrelease(&opt->state.lock);
}

/* Doubles per consecutive failure up to the ceiling: an outage is retried
   soon, a host that is simply gone costs a handful of lookups per crawl. */
int hts_dns_negative_wait_ms(int failures) {
  int wait = hts_dns_negative_ttl_ms;
  int i;

  for (i = 1; i < failures && wait < HTS_DNS_NEGATIVE_TTL_MAX_MS; i++)
    wait *= 2;
  return wait < HTS_DNS_NEGATIVE_TTL_MAX_MS ? wait
                                            : HTS_DNS_NEGATIVE_TTL_MAX_MS;
}

// MUST BE LOCKED (coucal is not internally serialized vs FTP/web threads)
// Look up iadr in the DNS cache, filling out[0..min(count,max)-1].
// Returns: -1 not tested, or a negative answer past its expiry; 0
// negative-cached (not in DNS); >0 address count.
static int hts_ghbn_all(coucal cache, const char *const iadr,
                        SOCaddr *const out, const int max) {
  void *ptr;

  assertf(out != NULL);
  assertf(iadr != NULL);
  if (*iadr == '\0') {
    return -1;
  }
  if (coucal_read_pvoid(cache, iadr, &ptr)) { // found
    const t_dnscache *const record = (const t_dnscache *) ptr;
    int i;

    assertf(record->host_count <= HTS_MAXADDRNUM);
    /* An outage must not blacklist a host for the rest of the crawl, and the
       stamps are wall-clock: a clock now behind the store time expires the
       record too, or an NTP step delays the retry by the size of the step. */
    if (record->expiry != 0) {
      const TStamp now = mtime_local();

      if (now >= record->expiry || now < record->stored) {
        return -1;
      }
    }
    for (i = 0; i < record->host_count && i < max; i++) {
      assertf(record->host_length[i] <= sizeof(record->host_addr[i]));
      SOCaddr_copyaddr2(out[i], record->host_addr[i], record->host_length[i]);
    }
    return record->host_count;
  }
  return -1;
}

#if HTS_INET6 != 0
/* Active resolver backend; defaults to the libc resolver. The self-test
   reroutes it to script DNS answers in-process (see
   hts_dns_set_resolver_backend). */
static const hts_resolver_backend hts_resolver_libc = {getaddrinfo,
                                                       freeaddrinfo};
static const hts_resolver_backend *hts_resolver = &hts_resolver_libc;

void hts_dns_set_resolver_backend(const hts_resolver_backend *backend) {
  hts_resolver = (backend != NULL) ? backend : &hts_resolver_libc;
}

/* Debug/test hook: HTTRACK_DEBUG_RESOLVE="host:ip[,ip...]" pins the resolution
   of `host` to the listed addresses (curl --resolve style), so the connect
   fallback can be exercised deterministically (a dead address first, a live one
   next). Any other host resolves normally. Below: an addrinfo backend that owns
   its chain (its own freeaddrinfo), so a synthesized and a delegated result
   free the same way. */

static void HTS_RESOLVER_CALL override_freeaddrinfo(struct addrinfo *res);

/* Deep-copy a libc addrinfo chain into our own allocations, or NULL if one of
   them failed: the caller cannot hand a half-copied chain back as success. */
static struct addrinfo *resolver_dup_chain(const struct addrinfo *src) {
  struct addrinfo *head = NULL, *tail = NULL;

  for (; src != NULL; src = src->ai_next) {
    struct addrinfo *ai = calloct(1, sizeof(*ai)); /* freet() needs an lvalue */

    if (ai == NULL) {
      override_freeaddrinfo(head);
      return NULL;
    }
    ai->ai_family = src->ai_family;
    ai->ai_socktype = src->ai_socktype;
    ai->ai_protocol = src->ai_protocol;
    ai->ai_addrlen = src->ai_addrlen;
    ai->ai_addr = malloct(src->ai_addrlen);
    if (ai->ai_addr == NULL) {
      freet(ai);
      override_freeaddrinfo(head);
      return NULL;
    }
    memcpy(ai->ai_addr, src->ai_addr, src->ai_addrlen);
    if (head == NULL)
      head = ai;
    else
      tail->ai_next = ai;
    tail = ai;
  }
  return head;
}

/* Build one addrinfo node from an IPv4/IPv6 literal, or NULL if it does not
   parse or is filtered out by want_family (AF_INET/AF_INET6/PF_UNSPEC). */
static struct addrinfo *resolver_make_ai(const char *ip, int want_family) {
  struct addrinfo *ai;

  if (strchr(ip, ':') != NULL) { // IPv6 literal
    struct sockaddr_in6 sa6;

    if (want_family != PF_UNSPEC && want_family != AF_INET6)
      return NULL;
    memset(&sa6, 0, sizeof(sa6));
    if (inet_pton(AF_INET6, ip, &sa6.sin6_addr) != 1)
      return NULL;
    sa6.sin6_family = AF_INET6;
    ai = calloct(1, sizeof(*ai));
    if (ai == NULL)
      return NULL;
    ai->ai_family = AF_INET6;
    ai->ai_addrlen = sizeof(sa6);
    ai->ai_addr = malloct(sizeof(sa6));
    if (ai->ai_addr == NULL) {
      freet(ai);
      return NULL;
    }
    memcpy(ai->ai_addr, &sa6, sizeof(sa6));
  } else { // IPv4 literal
    struct sockaddr_in sa;

    if (want_family != PF_UNSPEC && want_family != AF_INET)
      return NULL;
    memset(&sa, 0, sizeof(sa));
    if (inet_pton(AF_INET, ip, &sa.sin_addr) != 1)
      return NULL;
    sa.sin_family = AF_INET;
    ai = calloct(1, sizeof(*ai));
    if (ai == NULL)
      return NULL;
    ai->ai_family = AF_INET;
    ai->ai_addrlen = sizeof(sa);
    ai->ai_addr = malloct(sizeof(sa));
    if (ai->ai_addr == NULL) {
      freet(ai);
      return NULL;
    }
    memcpy(ai->ai_addr, &sa, sizeof(sa));
  }
  return ai;
}

static void HTS_RESOLVER_CALL override_freeaddrinfo(struct addrinfo *res) {
  while (res != NULL) {
    struct addrinfo *const next = res->ai_next;

    freet(res->ai_addr);
    freet(res);
    res = next;
  }
}

static int HTS_RESOLVER_CALL override_getaddrinfo(const char *node,
                                                  const char *service,
                                                  const struct addrinfo *hints,
                                                  struct addrinfo **res) {
  const char *const spec = getenv("HTTRACK_DEBUG_RESOLVE");
  const int want = (hints != NULL) ? hints->ai_family : PF_UNSPEC;
  const char *colon;

  *res = NULL;
  if (spec != NULL && node != NULL && (colon = strchr(spec, ':')) != NULL &&
      (size_t) (colon - spec) == strlen(node) &&
      strncmp(spec, node, colon - spec) == 0) {
    struct addrinfo *head = NULL, *tail = NULL;
    char buf[256];
    char *p;

    buf[0] = '\0';
    strncatbuff(buf, colon + 1, sizeof(buf) - 1);
    for (p = strtok(buf, ","); p != NULL; p = strtok(NULL, ",")) {
      struct addrinfo *const ai = resolver_make_ai(p, want);

      if (ai != NULL) {
        if (head == NULL)
          head = ai;
        else
          tail->ai_next = ai;
        tail = ai;
      }
    }
    if (head == NULL)
      return EAI_NONAME;
    *res = head;
    return 0;
  }

  /* not overridden: delegate to libc, copying into our owned format */
  {
    struct addrinfo *sys = NULL;
    int gerr = getaddrinfo(node, service, hints, &sys);

    if (gerr != 0)
      return gerr;
    *res = resolver_dup_chain(sys);
    freeaddrinfo(sys);
    return *res != NULL ? 0 : EAI_MEMORY;
  }
}

static const hts_resolver_backend hts_resolver_override = {
    override_getaddrinfo, override_freeaddrinfo};

/* Install the env override once, unless a backend was already set (self-test).
 */
static void hts_resolver_check_env(void) {
  static int checked = 0;

  if (!checked) {
    checked = 1;
    if (hts_resolver == &hts_resolver_libc &&
        getenv("HTTRACK_DEBUG_RESOLVE") != NULL) {
      hts_resolver = &hts_resolver_override;
    }
  }
}
#endif

#if HTS_INET6 == 0
/* gethostbyname() answers into a shared static hostent, so the call and the
   copy out of it are one critical section, or two workers resolving at once
   read each other's addresses. Not getaddrinfo(), because configure also lands
   here when libc has none. */
static htsmutex dns_hostent_lock = HTSMUTEX_INIT;
#endif

// Resolve hostname into up to max addresses (resolver/RFC 6724 order), no
// cache. Returns the count copied into out[0..count-1]; 0 = does not resolve.
/* On a zero count, *permanent tells the two failures apart: the resolver
   saying the name does not exist, or it being unable to answer at all. */
static int hts_dns_resolve_nocache_list_(const char *const hostname,
                                         SOCaddr *const out, const int max,
                                         const char **error,
                                         hts_boolean *permanent) {
  int count = 0;
  hts_boolean is_permanent = HTS_FALSE;

#if HTS_INET6 == 0
  /* IPv4 resolver */
  const struct hostent *hp;

  hts_mutexlock(&dns_hostent_lock);
  hp = gethostbyname(hostname);
  if (hp == NULL && h_errno == HOST_NOT_FOUND)
    is_permanent = HTS_TRUE;
  if (hp != NULL) {
    char *const *h;

    for (h = hp->h_addr_list; count < max && h != NULL && *h != NULL; h++) {
      SOCaddr_clear(out[count]);
      SOCaddr_copyaddr2(out[count], *h, hp->h_length);
      if (SOCaddr_is_valid(out[count]))
        count++;
    }
  }
  hts_mutexrelease(&dns_hostent_lock);
#else
  /* IPv6 resolver */
  struct addrinfo *res = NULL, *cur;
  struct addrinfo hints;
  int gerr;

  hts_resolver_check_env();
  memset(&hints, 0, sizeof(hints));
  if (IPV6_resolver == 1) // V4 only (for bogus V6 entries)
    hints.ai_family = PF_INET;
  else if (IPV6_resolver == 2) // V6 only (for testing V6 only)
    hints.ai_family = PF_INET6;
  else // V4 + V6
    hints.ai_family = PF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  if ((gerr = hts_resolver->getaddrinfo(hostname, NULL, &hints, &res)) == 0) {
    for (cur = res; cur != NULL && count < max; cur = cur->ai_next) {
      if (cur->ai_addr != NULL && cur->ai_addrlen != 0) {
        SOCaddr_clear(out[count]);
        SOCaddr_copyaddr2(out[count], cur->ai_addr, cur->ai_addrlen);
        if (SOCaddr_is_valid(out[count]))
          count++;
      }
    }
  } else {
    if (error != NULL)
      *error = gai_strerror(gerr);
    /* EAI_NONAME answers about the name; every other code says the resolver
       could not answer at all. */
    if (gerr == EAI_NONAME)
      is_permanent = HTS_TRUE;
  }
  if (res) {
    hts_resolver->freeaddrinfo(res);
  }
#endif

  if (permanent != NULL)
    *permanent = is_permanent;
  return count;
}

// Strip [] around a literal IPv6 ([3ffe:b80:1234:1::1]) the resolver won't
// take, then resolve into a list. Returns the count.
static int hts_dns_resolve_nocache_list(const char *const hostname,
                                        SOCaddr *const out, const int max,
                                        const char **error,
                                        hts_boolean *permanent) {
  if (!strnotempty(hostname) || max <= 0) {
    if (permanent != NULL)
      *permanent = HTS_FALSE;
    return 0;
  }
  if ((hostname[0] == '[') && (hts_lastchar(hostname) == ']')) {
    size_t size = strlen(hostname);
    char *copy = malloct(size + 1);
    int count;

    assertf(copy != NULL);
    copy[0] = '\0';
    strncat(copy, hostname + 1, size - 2);
    count = hts_dns_resolve_nocache_list_(copy, out, max, error, permanent);
    freet(copy);
    return count;
  } else {
    return hts_dns_resolve_nocache_list_(hostname, out, max, error, permanent);
  }
}

SOCaddr *hts_dns_resolve_nocache2(const char *const hostname,
                                  SOCaddr *const addr, const char **error) {
  SOCaddr_clear(*addr);
  if (hts_dns_resolve_nocache_list(hostname, addr, 1, error, NULL) > 0) {
    return SOCaddr_is_valid(*addr) ? addr : NULL;
  }
  return NULL;
}

SOCaddr *hts_dns_resolve_nocache(const char *const hostname,
                                 SOCaddr *const addr) {
  return hts_dns_resolve_nocache2(hostname, addr, NULL);
}

/* A resolve in flight. Refcounted: a timed-out resolve is abandoned, not
   cancelled, so the last of caller/worker to leave frees the job. */
typedef struct dns_resolve_job {
  htsmutex lock;
  int refcount;
  hts_boolean done;
  char *hostname;
  SOCaddr addr[HTS_MAXADDRNUM];
  int count;
  const char *error;
  hts_boolean permanent; /* a zero count that was about the name itself */
} dns_resolve_job;

/* Copy the first min(count, max) addresses of src into dest. */
static void dns_copy_addrs(SOCaddr *dest, SOCaddr *src, int count, int max) {
  int i;

  for (i = 0; i < count && i < max; i++)
    SOCaddr_copy_SOCaddr(dest[i], src[i]);
}

static void dns_job_release(dns_resolve_job *job) {
  hts_boolean last;

  hts_mutexlock(&job->lock);
  last = (--job->refcount == 0) ? HTS_TRUE : HTS_FALSE;
  hts_mutexrelease(&job->lock);
  if (last) {
    hts_mutexfree(&job->lock);
    freet(job->hostname);
    freet(job);
  }
}

/* Outlives a timed-out resolve, so it writes only the job: never opt nor the
   DNS cache, which the crawl keeps using without it. */
static void dns_resolve_thread(void *arg) {
  dns_resolve_job *const job = (dns_resolve_job *) arg;
  SOCaddr resolved[HTS_MAXADDRNUM];
  const char *error = NULL;
  hts_boolean permanent = HTS_FALSE;
  const int count = hts_dns_resolve_nocache_list(
      job->hostname, resolved, HTS_MAXADDRNUM, &error, &permanent);

  hts_crash_test_worker(HTS_CRASH_WORKER_DNS);
  hts_mutexlock(&job->lock);
  dns_copy_addrs(job->addr, resolved, count, HTS_MAXADDRNUM);
  job->count = count;
  job->error = error;
  job->permanent = permanent;
  job->done = HTS_TRUE; /* published last: gates the caller's read of addr[] */
  hts_mutexrelease(&job->lock);
}

/* Releases the job whatever the body did (see hts_newthread_tail). A -1 count
   is what the caller reads as "no answer", so it caches nothing. */
static void dns_resolve_done(void *arg) {
  dns_resolve_job *const job = (dns_resolve_job *) arg;

  hts_mutexlock(&job->lock);
  if (!job->done) {
    job->count = -1;
    job->done = HTS_TRUE;
  }
  hts_mutexrelease(&job->lock);
  dns_job_release(job);
}

/* Resolve hostname on a worker thread, giving up after timeout seconds or as
   soon as *cancel (optional) is raised. Returns the address count, or -1 on
   timeout -- distinct from 0 ("does not resolve"), which is a real answer and
   gets negative-cached. */
static int hts_dns_resolve_nocache_list_bounded(
    const char *hostname, SOCaddr *const out, const int max, const int timeout,
    const volatile hts_boolean *cancel, const char **error,
    hts_boolean *permanent) {
  dns_resolve_job *job;
  TStamp deadline;
  int count = -1;
  int poll_ms = 1;

  /* no bound asked for (--timeout 0), and nobody to cut it short either */
  if (timeout <= 0 && cancel == NULL)
    return hts_dns_resolve_nocache_list(hostname, out, max, error, permanent);

  job = calloct(1, sizeof(*job));
  assertf(job != NULL);
  hts_mutexinit(&job->lock);
  job->hostname = strdupt(hostname);
  job->refcount = 2; /* this caller + the worker */
  if (hts_newthread_tail(dns_resolve_thread, job, dns_resolve_done) != 0) {
    job->refcount = 1; /* no worker: fall back to resolving inline */
    dns_job_release(job);
    return hts_dns_resolve_nocache_list(hostname, out, max, error, permanent);
  }

  /* timeout <= 0 got here only for a cancellable resolve: no deadline then */
  deadline = timeout > 0 ? mtime_monotonic() + (TStamp) timeout * 1000 : 0;
  for (;;) {
    hts_boolean done;

    hts_mutexlock(&job->lock);
    done = job->done;
    if (done) {
      count = job->count;
      dns_copy_addrs(out, job->addr, count, max);
      if (error != NULL)
        *error = job->error;
      if (permanent != NULL)
        *permanent = job->permanent;
    }
    hts_mutexrelease(&job->lock);
    if (done || (cancel != NULL && hts_load_acquire_int(cancel)) ||
        (deadline != 0 && mtime_monotonic() >= deadline))
      break;
    Sleep(poll_ms);
    if (poll_ms < 50) /* short first polls keep a fast resolve fast */
      poll_ms *= 2;
  }
  dns_job_release(job);
  return count;
}

int hts_dns_resolve_all(httrackp *opt, const char *iadr, SOCaddr *out, int max,
                        const char **error) {
  assertf(opt != NULL);
  /* the mirror's stop flag cancels: a resolve behind a black hole otherwise
     holds a stopped crawl for the whole --timeout (#1073) */
  return hts_dns_resolve_all_bounded(opt, iadr, out, max, opt->timeout,
                                     &opt->state.stop, error);
}

int hts_dns_resolve_all_bounded(httrackp *opt, const char *iadr, SOCaddr *out,
                                int max, int timeout,
                                const volatile hts_boolean *cancel,
                                const char **error) {
  char BIGSTK host[HTS_URLMAXSIZE * 2];
  SOCaddr resolved[HTS_MAXADDRNUM];
  hts_boolean permanent = HTS_FALSE;
  coucal cache;
  int count, i;

  assertf(opt != NULL);
  assertf(out != NULL);
  if (!strnotempty(iadr) || max <= 0) {
    return 0;
  }

  /* cache key and resolver input: identification and any ":port" stripped */
  strcpybuff(host, jump_identification_const(iadr));
  {
    char *a;

    if ((a = jump_toport(host)))
      *a = '\0';
  }

  hts_mutexlock(&opt->state.lock);
  cache = hts_cache(opt);
#if HTS_INET6 != 0
  hts_resolver_check_env(); /* settle the backend before a worker reads it */
#endif
  count = hts_ghbn_all(cache, host, out, max);
  hts_mutexrelease(&opt->state.lock);
  if (count >= 0) { // cache hit (0 == negative-cached)
    return count;
  }

  /* Resolve with no lock held: getaddrinfo can block for a long time, and
     state.lock also gates the stop request (#606). */
  count = hts_dns_resolve_nocache_list_bounded(
      host, resolved, HTS_MAXADDRNUM, timeout, cancel, error, &permanent);

  if (count < 0) { /* timed out: no answer to cache, and none to report */
    if (error != NULL)
      *error = "host name resolution timed out";
    return 0;
  }

  hts_mutexlock(&opt->state.lock);
  { /* store the full list (coucal owns the record and dups the host key; a
       concurrent resolve of the same host replaces, and frees, this one) */
    t_dnscache *const record = malloct(sizeof(t_dnscache));
    void *previous;
    const int failures =
        (count == 0 && coucal_read_pvoid(cache, host, &previous))
            ? ((const t_dnscache *) previous)->failures + 1
            : 1;

    if (record != NULL) {
      memset(record, 0, sizeof(*record));
      record->host_count = count;
      record->failures = count == 0 ? failures : 0;
      /* The resolver saying the name does not exist stands for the crawl;
         it being unable to answer is only as good as the network it asked. */
      if (count == 0 && !permanent) {
        record->stored = mtime_local();
        record->expiry = record->stored + hts_dns_negative_wait_ms(failures);
      }
      for (i = 0; i < count; i++) {
        record->host_length[i] = SOCaddr_size(resolved[i]);
        assertf(record->host_length[i] <= sizeof(record->host_addr[i]));
        memcpy(record->host_addr[i], &SOCaddr_sockaddr(resolved[i]),
               record->host_length[i]);
      }
      coucal_add_pvoid(cache, host, record);
    }
  }
  hts_mutexrelease(&opt->state.lock);

  /* copy result to caller (cache store may have failed; result still valid) */
  dns_copy_addrs(out, resolved, count, max);
  return count;
}

SOCaddr *hts_dns_resolve2(httrackp *opt, const char *_iadr, SOCaddr *const addr,
                          const char **error) {
  SOCaddr_clear(*addr);
  if (hts_dns_resolve_all(opt, _iadr, addr, 1, error) > 0) {
    return SOCaddr_is_valid(*addr) ? addr : NULL;
  }
  return NULL;
}
