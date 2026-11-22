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
static coucal hts_cache(httrackp *opt) {
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

/* Double the wait per failure up to the ceiling, so dead hosts cost little. */
int hts_dns_negative_wait_ms(int failures) {
  int wait = hts_dns_negative_ttl_ms;
  int i;

  for (i = 1; i < failures && wait < HTS_DNS_NEGATIVE_TTL_MAX_MS; i++)
    wait *= 2;
  return wait < HTS_DNS_NEGATIVE_TTL_MAX_MS ? wait
                                            : HTS_DNS_NEGATIVE_TTL_MAX_MS;
}

/* Fixed texts, since gai_strerror() may share one buffer between threads. A
   table, because some systems give two codes one value. */
typedef struct dns_gai_text {
  int code;
  const char *text;
  const char *cached;
} dns_gai_text;

static const dns_gai_text dns_gai_texts[] = {
#ifdef EAI_AGAIN
    {EAI_AGAIN, "temporary DNS failure", "temporary DNS failure (cached)"},
#endif
#ifdef EAI_FAIL
    {EAI_FAIL, "permanent DNS failure", "permanent DNS failure (cached)"},
#endif
#ifdef EAI_NODATA
    {EAI_NODATA, "no address for host name",
     "no address for host name (cached)"},
#endif
#ifdef EAI_ADDRFAMILY
    {EAI_ADDRFAMILY, "no address in this family",
     "no address in this family (cached)"},
#endif
#ifdef EAI_MEMORY
    {EAI_MEMORY, "resolver out of memory", "resolver out of memory (cached)"},
#endif
#ifdef EAI_SYSTEM
    {EAI_SYSTEM, "resolver system error", "resolver system error (cached)"},
#endif
    {0, "host name resolution failed", "host name resolution failed (cached)"},
};

const char *hts_dns_strerror(const hts_dns_result *res) {
  size_t i;

  switch (res->status) {
  case HTS_DNS_OK:
    return "no error";
  case HTS_DNS_NONAME:
    return res->cached ? "host name does not exist (cached)"
                       : "host name does not exist";
  case HTS_DNS_TEMPFAIL:
    /* the last row, code 0, also answers for a code with no text */
    for (i = 0; i + 1 < sizeof(dns_gai_texts) / sizeof(dns_gai_texts[0]); i++)
      if (dns_gai_texts[i].code == res->gai_code)
        break;
    return res->cached ? dns_gai_texts[i].cached : dns_gai_texts[i].text;
  case HTS_DNS_TIMEOUT:
    return "host name resolution timed out";
  case HTS_DNS_CANCELLED:
    return "host name resolution cancelled";
  }
  return "unknown error";
}

/* Empty res, giving it status st and getaddrinfo() code gai. */
static void dns_result_fail(hts_dns_result *res, hts_dns_status st, int gai) {
  res->count = 0;
  res->status = st;
  res->gai_code = gai;
  res->cached = HTS_FALSE;
}

// MUST BE LOCKED (coucal is not internally serialized vs FTP/web threads)
// Look up iadr in the DNS cache into res. Returns HTS_FALSE when it is not
// cached, or is a negative answer past its expiry.
static hts_boolean hts_ghbn_all(coucal cache, const char *const iadr,
                                hts_dns_result *const res) {
  void *ptr;

  assertf(res != NULL);
  assertf(iadr != NULL);
  if (*iadr == '\0') {
    return HTS_FALSE;
  }
  if (coucal_read_pvoid(cache, iadr, &ptr)) { // found
    const t_dnscache *const record = (const t_dnscache *) ptr;
    int i;

    assertf(record->host_count <= HTS_MAXADDRNUM);
    /* Also expire a negative record when the wall clock went back. */
    if (record->expiry != 0) {
      const TStamp now = mtime_local();

      if (now >= record->expiry || now < record->stored) {
        return HTS_FALSE;
      }
    }
    for (i = 0; i < record->host_count; i++) {
      assertf(record->host_length[i] <= sizeof(record->host_addr[i]));
      SOCaddr_copyaddr2(res->addr[i], record->host_addr[i],
                        record->host_length[i]);
    }
    res->count = record->host_count;
    /* a negative record without an expiry is the name's own answer */
    res->status = record->host_count > 0 ? HTS_DNS_OK
                  : record->expiry == 0  ? HTS_DNS_NONAME
                                         : HTS_DNS_TEMPFAIL;
    res->gai_code = record->gai_code;
    res->cached = HTS_TRUE;
    return HTS_TRUE;
  }
  return HTS_FALSE;
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

/* Test hook: HTTRACK_DEBUG_RESOLVE="host:ip[,ip...]" pins host like curl
   --resolve. The backend owns its chains, so both kinds free the same way. */

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
    void *copy6;

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
    copy6 = malloct(sizeof(sa6));
    if (copy6 == NULL) {
      freet(ai);
      return NULL;
    }
    memcpy(copy6, &sa6, sizeof(sa6));
    ai->ai_addrlen = sizeof(sa6);
    ai->ai_addr = copy6;
  } else { // IPv4 literal
    struct sockaddr_in sa;
    void *copy4;

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
    copy4 = malloct(sizeof(sa));
    if (copy4 == NULL) {
      freet(ai);
      return NULL;
    }
    memcpy(copy4, &sa, sizeof(sa));
    ai->ai_addrlen = sizeof(sa);
    ai->ai_addr = copy4;
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

// Resolve hostname into res (resolver/RFC 6724 order, at most
// HTS_MAXADDRNUM), no cache. Returns res->status.
static hts_dns_status hts_dns_resolve_nocache_list_(const char *const hostname,
                                                    hts_dns_result *const res) {
  int count = 0;
#if HTS_INET6 == 0
  /* IPv4 resolver */
  const struct hostent *hp;

  dns_result_fail(res, HTS_DNS_TEMPFAIL, 0);
  hts_mutexlock(&dns_hostent_lock);
  hp = gethostbyname(hostname);
  if (hp == NULL && h_errno == HOST_NOT_FOUND)
    res->status = HTS_DNS_NONAME;
  if (hp != NULL) {
    char *const *h;

    for (h = hp->h_addr_list; count < HTS_MAXADDRNUM && h != NULL && *h != NULL;
         h++) {
      SOCaddr_clear(res->addr[count]);
      SOCaddr_copyaddr2(res->addr[count], *h, hp->h_length);
      if (SOCaddr_is_valid(res->addr[count]))
        count++;
    }
  }
  hts_mutexrelease(&dns_hostent_lock);
#else
  /* IPv6 resolver */
  struct addrinfo *res_ai = NULL, *cur;
  struct addrinfo hints;
  int gerr;

  dns_result_fail(res, HTS_DNS_TEMPFAIL, 0);
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
  if ((gerr = hts_resolver->getaddrinfo(hostname, NULL, &hints, &res_ai)) ==
      0) {
    for (cur = res_ai; cur != NULL && count < HTS_MAXADDRNUM;
         cur = cur->ai_next) {
      if (cur->ai_addr != NULL && cur->ai_addrlen != 0) {
        SOCaddr_clear(res->addr[count]);
        SOCaddr_copyaddr2(res->addr[count], cur->ai_addr, cur->ai_addrlen);
        if (SOCaddr_is_valid(res->addr[count]))
          count++;
      }
    }
  } else {
    res->gai_code = gerr;
    /* EAI_NONAME answers about the name; every other code says the resolver
       could not answer at all. */
    if (gerr == EAI_NONAME)
      res->status = HTS_DNS_NONAME;
  }
  if (res_ai) {
    hts_resolver->freeaddrinfo(res_ai);
  }
#endif

  if (count > 0) {
    res->count = count;
    res->status = HTS_DNS_OK;
    res->gai_code = 0;
  }
  return res->status;
}

// Strip [] around a literal IPv6 ([3ffe:b80:1234:1::1]) the resolver won't
// take, then resolve into res.
static hts_dns_status hts_dns_resolve_nocache_list(const char *const hostname,
                                                   hts_dns_result *const res) {
  if (!strnotempty(hostname)) {
    dns_result_fail(res, HTS_DNS_NONAME, 0);
    return res->status;
  }
  if ((hostname[0] == '[') && (hts_lastchar(hostname) == ']')) {
    size_t size = strlen(hostname);
    char *copy = malloct(size + 1);
    hts_dns_status st;

    assertf(copy != NULL);
    copy[0] = '\0';
    strncat(copy, hostname + 1, size - 2);
    st = hts_dns_resolve_nocache_list_(copy, res);
    freet(copy);
    return st;
  } else {
    return hts_dns_resolve_nocache_list_(hostname, res);
  }
}

SOCaddr *hts_dns_resolve_nocache(const char *const hostname,
                                 SOCaddr *const addr) {
  hts_dns_result res;

  (void) hts_dns_resolve_nocache_list(hostname, &res);
  SOCaddr_clear(*addr);
  if (res.count > 0)
    SOCaddr_copy_SOCaddr(*addr, res.addr[0]);
  return SOCaddr_is_valid(*addr) ? addr : NULL;
}

/* A timed-out resolve keeps running, so this job is refcounted and the last of
   caller and worker frees it. */
typedef struct dns_resolve_job {
  htsmutex lock;
  int refcount;
  hts_boolean done;
  char *hostname;
  hts_dns_result res; /* res.count is -1 when the worker gave no answer */
  hts_boolean stored; /* hts_dns_poll() put the answer in the cache */
} dns_resolve_job;

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
  hts_dns_result res;

  (void) hts_dns_resolve_nocache_list(job->hostname, &res);
  hts_crash_test_worker(HTS_CRASH_WORKER_DNS);
  hts_mutexlock(&job->lock);
  job->res = res;
  job->done = HTS_TRUE; /* published last: gates the caller's read of addr[] */
  hts_mutexrelease(&job->lock);
}

/* Releases the job whatever the body did (see hts_newthread_tail). A -1 count
   is what the caller reads as "no answer", so it caches nothing. */
static void dns_resolve_done(void *arg) {
  dns_resolve_job *const job = (dns_resolve_job *) arg;

  hts_mutexlock(&job->lock);
  if (!job->done) {
    dns_result_fail(&job->res, HTS_DNS_TEMPFAIL, 0);
    job->res.count = -1;
    job->done = HTS_TRUE;
  }
  hts_mutexrelease(&job->lock);
  dns_job_release(job);
}

/* Start resolving hostname on a worker thread, holding one reference for the
   caller. NULL when no thread could start. */
static dns_resolve_job *dns_job_start(const char *hostname) {
  dns_resolve_job *const job = calloct(1, sizeof(*job));

  assertf(job != NULL);
  hts_mutexinit(&job->lock);
  job->hostname = strdupt(hostname);
  job->refcount = 2; /* this caller + the worker */
  if (hts_newthread_tail(dns_resolve_thread, job, dns_resolve_done) != 0) {
    job->refcount = 1;
    dns_job_release(job);
    return NULL;
  }
  return job;
}

/* Resolve hostname on a worker thread, giving up after timeout seconds or as
   soon as *cancel (optional) is raised. Returns HTS_FALSE when no answer came,
   with res saying why: a timeout, a cancel or a worker cut short, none of which
   may be cached. A zero count is a real answer and gets negative-cached. */
static hts_boolean hts_dns_resolve_nocache_list_bounded(
    const char *hostname, hts_dns_result *const res, const int timeout,
    const volatile hts_boolean *cancel) {
  dns_resolve_job *job;
  TStamp deadline;
  hts_boolean answered = HTS_FALSE;
  int poll_ms = 1;

  /* no bound asked for (--timeout 0), and nobody to cut it short either */
  if (timeout <= 0 && cancel == NULL) {
    (void) hts_dns_resolve_nocache_list(hostname, res);
    return HTS_TRUE;
  }

  job = dns_job_start(hostname);
  if (job == NULL) { /* no worker: fall back to resolving inline */
    (void) hts_dns_resolve_nocache_list(hostname, res);
    return HTS_TRUE;
  }

  /* timeout <= 0 got here only for a cancellable resolve: no deadline then */
  deadline = timeout > 0 ? mtime_monotonic() + (TStamp) timeout * 1000 : 0;
  for (;;) {
    hts_boolean done;

    hts_mutexlock(&job->lock);
    done = job->done;
    if (done) {
      *res = job->res;
      answered = res->count >= 0 ? HTS_TRUE : HTS_FALSE;
      if (!answered)
        res->count = 0;
    }
    hts_mutexrelease(&job->lock);
    if (done)
      break;
    if (cancel != NULL && hts_load_acquire_int(cancel)) {
      dns_result_fail(res, HTS_DNS_CANCELLED, 0);
      break;
    }
    if (deadline != 0 && mtime_monotonic() >= deadline) {
      dns_result_fail(res, HTS_DNS_TIMEOUT, 0);
      break;
    }
    Sleep(poll_ms);
    if (poll_ms < 50) /* short first polls keep a fast resolve fast */
      poll_ms *= 2;
  }
  dns_job_release(job);
  return answered;
}

/* Cache key and resolver input: identification and any ":port" stripped. */
static void dns_cache_key(char *host, size_t size, const char *iadr) {
  char *a;

  strlcpybuff(host, jump_identification_const(iadr), size);
  if ((a = jump_toport(host)))
    *a = '\0';
}

static hts_boolean dns_same_addr(const void *a, size_t size, const SOCaddr *b) {
  return size != 0 && size == SOCaddr_size(*b) &&
         memcmp(a, &SOCaddr_sockaddr(*b), size) == 0;
}

int hts_dns_addr_index(const SOCaddr *list, int count, const SOCaddr *addr) {
  int i;

  for (i = 0; i < count; i++) {
    if (dns_same_addr(&SOCaddr_sockaddr(list[i]), SOCaddr_size(list[i]), addr))
      return i;
  }
  return -1;
}

int hts_dns_preferred(httrackp *opt, const char *iadr) {
  char BIGSTK host[HTS_URLMAXSIZE * 2];
  void *ptr;
  int preferred = 0;

  assertf(opt != NULL);
  if (!strnotempty(iadr))
    return 0;
  dns_cache_key(host, sizeof(host), iadr);
  hts_mutexlock(&opt->state.lock);
  if (coucal_read_pvoid(hts_cache(opt), host, &ptr))
    preferred = ((const t_dnscache *) ptr)->preferred;
  hts_mutexrelease(&opt->state.lock);
  return preferred;
}

void hts_dns_set_preferred(httrackp *opt, const char *iadr,
                           const SOCaddr *addr) {
  char BIGSTK host[HTS_URLMAXSIZE * 2];
  void *ptr;

  assertf(opt != NULL);
  if (!strnotempty(iadr))
    return;
  dns_cache_key(host, sizeof(host), iadr);
  hts_mutexlock(&opt->state.lock);
  if (coucal_read_pvoid(hts_cache(opt), host, &ptr)) {
    t_dnscache *const record = (t_dnscache *) ptr;
    int i;

    for (i = 0; i < record->host_count; i++) {
      if (dns_same_addr(record->host_addr[i], record->host_length[i], addr)) {
        record->preferred = i;
        break;
      }
    }
  }
  hts_mutexrelease(&opt->state.lock);
}

/* Store a resolve's answer for host. Must hold no lock. */
static void dns_cache_store(httrackp *opt, const char *host,
                            const hts_dns_result *res) {
  const int count = res->count;
  coucal cache;
  int i;

  hts_mutexlock(&opt->state.lock);
  cache = hts_cache(opt);
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
      record->gai_code = res->gai_code;
      if (count == 0 && res->status != HTS_DNS_NONAME) {
        record->stored = mtime_local();
        record->expiry = record->stored + hts_dns_negative_wait_ms(failures);
      }
      for (i = 0; i < count; i++) {
        record->host_length[i] = SOCaddr_size(res->addr[i]);
        assertf(record->host_length[i] <= sizeof(record->host_addr[i]));
        memcpy(record->host_addr[i], &SOCaddr_sockaddr(res->addr[i]),
               record->host_length[i]);
      }
      coucal_add_pvoid(cache, host, record);
    }
  }
  hts_mutexrelease(&opt->state.lock);
}

hts_dns_status hts_dns_resolve(httrackp *opt, const char *iadr, int timeout,
                               const volatile hts_boolean *cancel,
                               hts_dns_result *res) {
  char BIGSTK host[HTS_URLMAXSIZE * 2];
  hts_boolean hit;

  assertf(opt != NULL);
  assertf(res != NULL);
  if (!strnotempty(iadr)) {
    dns_result_fail(res, HTS_DNS_NONAME, 0);
    return res->status;
  }

  dns_cache_key(host, sizeof(host), iadr);

  hts_mutexlock(&opt->state.lock);
#if HTS_INET6 != 0
  hts_resolver_check_env(); /* settle the backend before a worker reads it */
#endif
  hit = hts_ghbn_all(hts_cache(opt), host, res);
  hts_mutexrelease(&opt->state.lock);
  if (hit)
    return res->status;

  /* Resolve with no lock held: getaddrinfo can block for a long time, and
     state.lock also gates the stop request (#606). */
  if (hts_dns_resolve_nocache_list_bounded(host, res, timeout, cancel))
    dns_cache_store(opt, host, res);
  return res->status;
}

int hts_dns_resolve_all(httrackp *opt, const char *iadr, SOCaddr *out,
                        int max) {
  hts_dns_result res;
  int i;

  assertf(opt != NULL);
  assertf(out != NULL);
  if (!strnotempty(iadr) || max <= 0)
    return 0;
  /* the mirror's stop flag cancels: a resolve behind a black hole otherwise
     holds a stopped crawl for the whole --timeout (#1073) */
  (void) hts_dns_resolve(opt, iadr, opt->timeout, &opt->state.stop, &res);
  for (i = 0; i < res.count && i < max; i++)
    SOCaddr_copy_SOCaddr(out[i], res.addr[i]);
  return res.count;
}

hts_boolean hts_dns_cached(httrackp *opt, const char *iadr) {
  char BIGSTK host[HTS_URLMAXSIZE * 2];
  hts_dns_result scratch;
  hts_boolean hit;

  assertf(opt != NULL);
  if (!strnotempty(iadr))
    return HTS_TRUE; /* nothing to resolve */
  dns_cache_key(host, sizeof(host), iadr);
  hts_mutexlock(&opt->state.lock);
  hit = hts_ghbn_all(hts_cache(opt), host, &scratch);
  hts_mutexrelease(&opt->state.lock);
  return hit;
}

hts_dns_job *hts_dns_start(httrackp *opt, const char *iadr) {
  char BIGSTK host[HTS_URLMAXSIZE * 2];

  assertf(opt != NULL);
  dns_cache_key(host, sizeof(host), iadr);
#if HTS_INET6 != 0
  hts_mutexlock(&opt->state.lock);
  hts_resolver_check_env(); /* settle the backend before a worker reads it */
  hts_mutexrelease(&opt->state.lock);
#endif
  return dns_job_start(host);
}

hts_boolean hts_dns_job_is(const hts_dns_job *job, const char *iadr) {
  char BIGSTK host[HTS_URLMAXSIZE * 2];

  dns_cache_key(host, sizeof(host), iadr);
  return strcmp(job->hostname, host) == 0 ? HTS_TRUE : HTS_FALSE;
}

hts_dns_job *hts_dns_job_ref(hts_dns_job *job) {
  hts_mutexlock(&job->lock);
  job->refcount++;
  hts_mutexrelease(&job->lock);
  return job;
}

void hts_dns_job_release(hts_dns_job *job) { dns_job_release(job); }

int hts_dns_poll(httrackp *opt, hts_dns_job *job, hts_dns_result *res) {
  hts_dns_result answer;
  hts_boolean store;

  hts_mutexlock(&job->lock);
  if (!job->done) {
    hts_mutexrelease(&job->lock);
    return HTS_DNS_PENDING;
  }
  answer = job->res;
  /* the first poller of a shared job stores it, so failures count once */
  store = answer.count >= 0 && !job->stored;
  job->stored = HTS_TRUE;
  hts_mutexrelease(&job->lock);
  if (store)
    dns_cache_store(opt, job->hostname, &answer);
  if (res != NULL) {
    *res = answer;
    if (res->count < 0) /* the worker was cut short */
      res->count = 0;
  }
  return answer.count;
}
