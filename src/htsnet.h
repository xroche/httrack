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
/* File: Net definitions                                        */
/*       Used in .c files that needs connect() functions and so */
/* Note: includes htsbasenet.h                                  */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/** @file htsnet.h
    Socket/connection layer. Provides SOCaddr, an opaque IPv4/IPv6
   socket-address wrapper, plus accessor macros so callers never branch on
   address family. Builds on htsbasenet.h. */

#ifndef HTS_DEFNETH
#define HTS_DEFNETH

/* basic net definitions */
#include "htsglobal.h"
#include "htsbasenet.h"
#include "htssafe.h"

#include <string.h>
#include <ctype.h>
#ifdef _WIN32
// for read
#include <io.h>
// for FindFirstFile
#include <winbase.h>
typedef USHORT in_port_t;

typedef ADDRESS_FAMILY sa_family_t;
#else
#define INVALID_SOCKET -1
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
/* Force BSD_COMP for Sun environments. */
#ifndef BSD_COMP
#define BSD_COMP
#endif
#include <sys/ioctl.h>
/* gethostname & co */
#ifndef _WIN32
#include <unistd.h>
#endif
/* inet_addr */
#include <arpa/inet.h>
/* normally not needed; provide in_addr_t where the platform lacks it */
#ifndef HTS_DO_NOT_REDEFINE_in_addr_t
typedef unsigned long in_addr_t;
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Raw IP address type: in6_addr when IPv6 is enabled, else in_addr. */
#if HTS_INET6 != 0
typedef struct in6_addr INaddr;
#else
typedef struct in_addr INaddr;
#endif

/** Opaque socket address holding either an IPv4 or IPv6 endpoint. Use the
    SOCaddr_* accessors rather than touching m_addr; sa_family selects the
    active union member. */
#ifndef HTS_DEF_FWSTRUCT_SOCaddr
#define HTS_DEF_FWSTRUCT_SOCaddr
typedef struct SOCaddr SOCaddr;
#endif
struct SOCaddr {
  union {
    /* Generic version, for network functions such as getnameinfo() */
    struct sockaddr sa;
    /* IPv4 */
    struct sockaddr_in in;
#if HTS_INET6 != 0
    /* IPv6 */
    struct sockaddr_in6 in6;
#endif
  } m_addr;
};

/** Pointer to the port field (network byte order) for the active family.
    Asserts on NULL or an unset/unknown family. */
static HTS_INLINE HTS_UNUSED in_port_t *
SOCaddr_sinport_(SOCaddr *const addr, const char *file, const int line) {
  assertf_(addr != NULL, file, line);
  switch (addr->m_addr.sa.sa_family) {
  case AF_INET:
    return &addr->m_addr.in.sin_port;
    break;
#if HTS_INET6 != 0
  case AF_INET6:
    return &addr->m_addr.in6.sin6_port;
    break;
#endif
  default:
    assertf_(!"invalid structure", file, line);
    return 0;
    break;
  }
}

/** Length of the active sockaddr (sockaddr_in or sockaddr_in6), or 0 if the
    family is unset/unknown. The 0 case doubles as the "not valid" test. */
static HTS_INLINE HTS_UNUSED socklen_t SOCaddr_size_(const SOCaddr *const addr,
                                                     const char *file,
                                                     const int line) {
  assertf_(addr != NULL, file, line);
  switch (addr->m_addr.sa.sa_family) {
  case AF_INET:
    return sizeof(addr->m_addr.in);
    break;
#if HTS_INET6 != 0
  case AF_INET6:
    return sizeof(addr->m_addr.in6);
    break;
#endif
  default:
    return 0;
    break;
  }
}

/** Reset to the unset state (family AF_UNSPEC), making the address invalid. */
static HTS_INLINE HTS_UNUSED void
SOCaddr_clear_(SOCaddr *const addr, const char *file, const int line) {
  assertf_(addr != NULL, file, line);
  addr->m_addr.sa.sa_family = AF_UNSPEC;
}

/* SOCaddr accessors; server is an lvalue SOCaddr, not a pointer. */
#define SOCaddr_sinfamily(server)                                              \
  ((server).m_addr.sa.sa_family) /* AF_INET / AF_INET6 */

#define SOCaddr_sinport(server)                                                \
  (*SOCaddr_sinport_(&(server), __FILE__,                                      \
                     __LINE__)) /* port lvalue (network order) */

#define SOCaddr_size(server)                                                   \
  (SOCaddr_size_(&(server), __FILE__, __LINE__)) /* active sockaddr length */

#define SOCaddr_is_valid(server)                                               \
  (SOCaddr_size_(&(server), __FILE__, __LINE__) !=                             \
   0) /* nonzero if family is set */

#define SOCaddr_clear(server) SOCaddr_clear_(&(server), __FILE__, __LINE__)

#define SOCaddr_sockaddr(server)                                               \
  ((server).m_addr.sa) /* generic struct sockaddr view */

#define SOCaddr_capacity(server)                                               \
  sizeof((server).m_addr) /* full union size, for recvfrom() etc. */

/** Address family to bind/listen with: AF_INET6 when IPv6 is enabled (dual
    stack), else AF_INET. */
#if HTS_INET6 != 0
#define AFinet AF_INET6
#else
#define AFinet AF_INET
#endif

/** Set the port (host-order argument, stored network-order) on the active
 * family. */
#define SOCaddr_initport(server, port)                                         \
  do {                                                                         \
    SOCaddr_sinport(server) = htons((in_port_t) (port));                       \
  } while (0)

/** Initialize as an all-zero IPv4 wildcard (INADDR_ANY) address; returns its
    sockaddr length. */
static HTS_INLINE HTS_UNUSED socklen_t SOCaddr_initany_(SOCaddr *const addr,
                                                        const char *file,
                                                        const int line) {
  assertf_(addr != NULL, file, line);
  memset(&addr->m_addr.in, 0, sizeof(addr->m_addr.in));
  addr->m_addr.in.sin_family = AF_INET;
  return SOCaddr_size_(addr, file, line);
}

/** Initialize server as an IPv4 wildcard (INADDR_ANY) address. */
#define SOCaddr_initany(server)                                                \
  do {                                                                         \
    SOCaddr_initany_(&(server), __FILE__, __LINE__);                           \
  } while (0)

/** Populate server from data. data_size selects the source form: a full
    sockaddr_in / sockaddr_in6, or a raw 4-byte (IPv4) / 16-byte (IPv6) address
    with port zeroed. Any other size leaves an AF_INET shell. Returns the
    resulting sockaddr length. */
static HTS_UNUSED socklen_t SOCaddr_copyaddr_(SOCaddr *const server,
                                              const void *data,
                                              const size_t data_size,
                                              const char *file,
                                              const int line) {
  assertf_(server != NULL, file, line);
  assertf_(data != NULL, file, line);

  if (data_size == sizeof(struct sockaddr_in)) {
    memcpy(&server->m_addr.in, data, sizeof(struct sockaddr_in));
    assertf_(server->m_addr.sa.sa_family == AF_INET, file, line);
#if HTS_INET6 != 0
  } else if (data_size == sizeof(struct sockaddr_in6)) {
    memcpy(&server->m_addr.in6, data, sizeof(struct sockaddr_in6));
    assertf_(server->m_addr.sa.sa_family == AF_INET6, file, line);
#endif
  } else if (data_size == 4) {
    memset(&server->m_addr.in, 0, sizeof(server->m_addr.in));
    server->m_addr.in.sin_family = AF_INET;
    server->m_addr.in.sin_port = 0;
    memcpy(&server->m_addr.in.sin_addr, data, 4);
#if HTS_INET6 != 0
  } else if (data_size == 16) {
    memset(&server->m_addr.in6, 0, sizeof(server->m_addr.in6));
    server->m_addr.in6.sin6_family = AF_INET6;
    server->m_addr.in6.sin6_port = 0;
    memcpy(&server->m_addr.in6.sin6_addr, data, 16);
#endif
  } else {
    server->m_addr.in.sin_family = AF_INET;
  }
  return SOCaddr_size_(server, file, line);
}

/** Copy hpaddr (length hpsize) into server, writing the result length into the
    lvalue server_len (int). See SOCaddr_copyaddr_ for accepted forms. */
#define SOCaddr_copyaddr(server, server_len, hpaddr, hpsize)                   \
  do {                                                                         \
    server_len = (int) SOCaddr_copyaddr_(&(server), hpaddr, hpsize, __FILE__,  \
                                         __LINE__);                            \
  } while (0)

/** Like SOCaddr_copyaddr but discards the result length. */
#define SOCaddr_copyaddr2(server, hpaddr, hpsize)                              \
  do {                                                                         \
    (void) SOCaddr_copyaddr_(&(server), hpaddr, hpsize, __FILE__, __LINE__);   \
  } while (0)

/** Copy one SOCaddr (src) into another (dest), preserving family and port. */
#define SOCaddr_copy_SOCaddr(dest, src)                                        \
  do {                                                                         \
    SOCaddr_copyaddr_(&(dest), &(src).m_addr.sa, SOCaddr_size(src), __FILE__,  \
                      __LINE__);                                               \
  } while (0)

/** Write the numeric (dotted/colon) host of ss into namebuf (capacity
    namebuflen), scope id stripped. On failure namebuf becomes "". */
static HTS_UNUSED void SOCaddr_inetntoa_(char *namebuf, size_t namebuflen,
                                         SOCaddr *const ss, const char *file,
                                         const int line) {
  assertf_(namebuf != NULL, file, line);
  assertf_(ss != NULL, file, line);

  if (getnameinfo(&ss->m_addr.sa, sizeof(ss->m_addr), namebuf, namebuflen, NULL,
                  0, NI_NUMERICHOST) == 0) {
    /* remove scope id(s) */
    char *const pos = strchr(namebuf, '%');
    if (pos != NULL) {
      *pos = '\0';
    }
  } else {
    namebuf[0] = '\0';
  }
}

/** Numeric host of ss into namebuf (capacity namebuflen); "" on failure. */
#define SOCaddr_inetntoa(namebuf, namebuflen, ss)                              \
  SOCaddr_inetntoa_(namebuf, namebuflen, &(ss), __FILE__, __LINE__)

/** Single-char family tag: '1' for IPv4, '2' otherwise (used in the cache). */
#define SOCaddr_getproto(ss)                                                   \
  (SOCaddr_size(ss) == sizeof(struct sockaddr_in) ? '1' : '2')

/** Length type for socket APIs (getsockname, accept, ...). */
typedef socklen_t SOClen;

#if HTS_INET6 != 0
/** Resolver backend: getaddrinfo/freeaddrinfo as a swappable pair, so the
    self-test can script DNS answers (families, multiplicity, errors)
    in-process. The free function must match its getaddrinfo (a fake allocates
    its own chain), hence the pair. */
/* Winsock's resolver is __stdcall; a plain pointer only compiles on x64, where
   there is one convention. Backend implementations must carry this too. */
#ifdef _WIN32
#define HTS_RESOLVER_CALL WSAAPI
#else
#define HTS_RESOLVER_CALL
#endif

typedef struct hts_resolver_backend {
  int(HTS_RESOLVER_CALL *getaddrinfo)(const char *node, const char *service,
                                      const struct addrinfo *hints,
                                      struct addrinfo **res);
  void(HTS_RESOLVER_CALL *freeaddrinfo)(struct addrinfo *res);
} hts_resolver_backend;

/** Install a resolver backend for the process; NULL restores the libc default.
    Test-only seam, not thread-safe; callers must serialize against resolves. */
void hts_dns_set_resolver_backend(const hts_resolver_backend *backend);
#endif

#ifdef __cplusplus
}
#endif

#endif
