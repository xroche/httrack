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
    Socket address layer: SOCaddr wraps one IPv4 or IPv6 endpoint, and the
    SOCaddr_* accessors read and write it without the caller branching on
    address family. */

#ifndef HTS_DEFNETH
#define HTS_DEFNETH

/* htsglobal.h comes first, because it fixes HTS_INET6, which the SOCaddr
   layout below reads. */
#include "htsglobal.h"
#include "htsbasenet.h"
#include "htssafe.h"

#include <string.h>
#include <ctype.h>
/* We supply in_port_t, sa_family_t and in_addr_t where the platform declares
   none: the Winsock types below, or a uint16_t macro out of the generated
   htsfeatures.h. A consumer sees our spelling, so it must not add its own. */
#ifdef _WIN32
// for read
#include <io.h>
// for FindFirstFile
#include <winbase.h>
/** Port number type, unsigned 16-bit. */
typedef USHORT in_port_t;

/** Socket address family, as in AF_INET. */
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
#ifndef HTS_DO_NOT_REDEFINE_in_addr_t
/** IPv4 address for a platform that declares no in_addr_t. It may be wider
    than the 32 bits POSIX asks for, so do not assume a width. */
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

/** One IPv4 or IPv6 endpoint, address and port together. Use the SOCaddr_*
    accessors rather than m_addr, because sa_family picks the active union
    member. HTS_INET6 decides whether the IPv6 member exists, so it sets
    sizeof(SOCaddr) and the layout of every struct holding one, such as
    htsblk. Build a consumer with the value htsfeatures.h publishes. */
#ifndef HTS_DEF_FWSTRUCT_SOCaddr
#define HTS_DEF_FWSTRUCT_SOCaddr
typedef struct SOCaddr SOCaddr;
#endif
struct SOCaddr {
  union {
    struct sockaddr sa;    /**< generic view, for bind() and getnameinfo() */
    struct sockaddr_in in; /**< active when sa.sa_family is AF_INET */
#if HTS_INET6 != 0
    struct sockaddr_in6 in6; /**< active when sa.sa_family is AF_INET6 */
#endif
  } m_addr; /**< the endpoint, tagged by sa.sa_family */
};

/** Pointer to the port field of the active family, network byte order.
    Asserts on NULL, and on any family other than AF_INET and AF_INET6. Every
    helper here takes @p file and @p line, the caller's __FILE__ and __LINE__
    for that assert, and the matching SOCaddr_* macro passes them. */
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

/** Returns the active sockaddr's length, 0 for any other family. That 0
    doubles as the not-valid test. */
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

static HTS_INLINE HTS_UNUSED void
SOCaddr_clear_(SOCaddr *const addr, const char *file, const int line) {
  assertf_(addr != NULL, file, line);
  addr->m_addr.sa.sa_family = AF_UNSPEC;
}

/* Every macro below names a member of its server argument or takes its
   address, so server must be an lvalue SOCaddr and not a pointer. Each
   evaluates its arguments once unless its own comment says otherwise. */

/** Address family of an endpoint: AF_INET, AF_INET6, or AF_UNSPEC when
    unset. An lvalue. */
#define SOCaddr_sinfamily(server) ((server).m_addr.sa.sa_family)

/** Port of the active family, network byte order. An lvalue. Asserts on a
    family SOCaddr_sinport_() does not handle. */
#define SOCaddr_sinport(server)                                                \
  (*SOCaddr_sinport_(&(server), __FILE__, __LINE__))

/** Length of the active sockaddr, to hand to bind() or connect(). 0 when no
    family is set. */
#define SOCaddr_size(server) (SOCaddr_size_(&(server), __FILE__, __LINE__))

/** Does server hold an endpoint? False after SOCaddr_clear(). */
#define SOCaddr_is_valid(server)                                               \
  (SOCaddr_size_(&(server), __FILE__, __LINE__) != 0)

/** Reset server to the unset state, which makes it invalid. */
#define SOCaddr_clear(server) SOCaddr_clear_(&(server), __FILE__, __LINE__)

/** Generic struct sockaddr view of server, an lvalue, so a caller hands
    &SOCaddr_sockaddr(x) to the socket API. */
#define SOCaddr_sockaddr(server) ((server).m_addr.sa)

/** Size of the whole union, which is the room a recvfrom() or getsockname() may
    fill. A compile-time constant, and server is never evaluated. */
#define SOCaddr_capacity(server) sizeof((server).m_addr)

/** Address family to open a socket with: AF_INET6 when the build has IPv6,
    else AF_INET. No engine code uses it. */
#if HTS_INET6 != 0
#define AFinet AF_INET6
#else
#define AFinet AF_INET
#endif

/** Set the port of the active family from a host-order @p port. Asserts when
    the family is unset, so give server an address first. */
#define SOCaddr_initport(server, port)                                         \
  do {                                                                         \
    SOCaddr_sinport(server) = htons((in_port_t) (port));                       \
  } while (0)

/** Set @p addr to the IPv4 wildcard address (INADDR_ANY) with port 0, and
    return its sockaddr length. */
static HTS_INLINE HTS_UNUSED socklen_t SOCaddr_initany_(SOCaddr *const addr,
                                                        const char *file,
                                                        const int line) {
  assertf_(addr != NULL, file, line);
  memset(&addr->m_addr.in, 0, sizeof(addr->m_addr.in));
  addr->m_addr.in.sin_family = AF_INET;
  return SOCaddr_size_(addr, file, line);
}

#define SOCaddr_initany(server)                                                \
  do {                                                                         \
    SOCaddr_initany_(&(server), __FILE__, __LINE__);                           \
  } while (0)

/** Set @p addr to the IPv4 loopback address (127.0.0.1) with port 0, and
    return its sockaddr length. IPv4, so it binds whether or not the host has
    IPv6. */
static HTS_INLINE HTS_UNUSED socklen_t
SOCaddr_initloopback_(SOCaddr *const addr, const char *file, const int line) {
  assertf_(addr != NULL, file, line);
  memset(&addr->m_addr.in, 0, sizeof(addr->m_addr.in));
  addr->m_addr.in.sin_family = AF_INET;
  addr->m_addr.in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  return SOCaddr_size_(addr, file, line);
}

#define SOCaddr_initloopback(server)                                           \
  do {                                                                         \
    SOCaddr_initloopback_(&(server), __FILE__, __LINE__);                      \
  } while (0)

/** Fill @p server from @p data, whose @p data_size picks the source form: a
    full sockaddr_in, a full sockaddr_in6, or a bare 4-byte IPv4 address with
    the port zeroed. A full sockaddr must already carry the matching family,
    because the copy asserts on it. Any other size only stamps AF_INET on
    @p server and leaves the address bytes as they were. Returns the resulting
    sockaddr length. There is no bare IPv6 form, because 16 is already the size
    of a sockaddr_in, so pass a sockaddr_in6. */
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
  } else {
    server->m_addr.in.sin_family = AF_INET;
  }
  return SOCaddr_size_(server, file, line);
}

/** Copy hpaddr of hpsize bytes into server, and store the result length in
    the int lvalue server_len. SOCaddr_copyaddr_() lists the accepted forms. */
#define SOCaddr_copyaddr(server, server_len, hpaddr, hpsize)                   \
  do {                                                                         \
    server_len = (int) SOCaddr_copyaddr_(&(server), hpaddr, hpsize, __FILE__,  \
                                         __LINE__);                            \
  } while (0)

/** SOCaddr_copyaddr() without the length output. */
#define SOCaddr_copyaddr2(server, hpaddr, hpsize)                              \
  do {                                                                         \
    (void) SOCaddr_copyaddr_(&(server), hpaddr, hpsize, __FILE__, __LINE__);   \
  } while (0)

/** Copy the endpoint src into dest, family and port included. src is
    evaluated twice. */
#define SOCaddr_copy_SOCaddr(dest, src)                                        \
  do {                                                                         \
    SOCaddr_copyaddr_(&(dest), &(src).m_addr.sa, SOCaddr_size(src), __FILE__,  \
                      __LINE__);                                               \
  } while (0)

/* Export marker for the out-of-line helpers below. proxytrack compiles
   htsnet.c in rather than linking the library, and MSVC rejects a dllimport
   definition, so it sets HTS_NO_LIBHTTRACK. */
#ifdef HTS_NO_LIBHTTRACK
#define HTSNET_API
#else
#define HTSNET_API HTSEXT_API
#endif

/* Out of line because getnameinfo() is not declared to a strict-ISO
   translation unit (#1001). */

/** Write the numeric host of @p ss, dotted for IPv4 and colon-separated for
    IPv6, into @p namebuf of @p namebuflen bytes, with any scope id stripped.
    @p namebuf becomes "" on failure. SOCADDR_INETNTOA_SIZE always fits. */
HTSNET_API void SOCaddr_inetntoa_(char *namebuf, size_t namebuflen,
                                  SOCaddr *const ss, const char *file,
                                  const int line);

#define SOCaddr_inetntoa(namebuf, namebuflen, ss)                              \
  SOCaddr_inetntoa_(namebuf, namebuflen, &(ss), __FILE__, __LINE__)

/** Capacity that always holds the numeric host of a SOCaddr: the longest IPv6
    text, plus the scope id getnameinfo() needs room for even though
    SOCaddr_inetntoa() strips it. */
#define SOCADDR_INETNTOA_SIZE                                                  \
  sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255%4294967295")

/** Capacity that always holds "host:port". The host alone can fill whatever
    it is given, so the port needs room of its own on top. */
#define SOCADDR_INETNTOA_PORT_SIZE                                             \
  (SOCADDR_INETNTOA_SIZE + sizeof(":65535") - 1)

/** Write "host:port" for @p ss into @p namebuf of @p namebuflen bytes,
    capping the host so the port always fits. Clips rather than aborting,
    because the address comes from a peer. Returns HTS_FALSE when @p namebuflen
    left no room for the port, which a caller sizing on
    SOCADDR_INETNTOA_PORT_SIZE never sees. Asserts when @p namebuflen is 0, or
    when @p ss carries no family. */
HTSNET_API hts_boolean SOCaddr_inetntoa_port_(char *namebuf, size_t namebuflen,
                                              SOCaddr *const ss,
                                              const char *file, const int line);

#define SOCaddr_inetntoa_port(namebuf, namebuflen, ss)                         \
  SOCaddr_inetntoa_port_(namebuf, namebuflen, &(ss), __FILE__, __LINE__)

/** FTP address-family number of ss, the one EPRT carries: '1' for IPv4, '2'
    otherwise. */
#define SOCaddr_getproto(ss)                                                   \
  (SOCaddr_size(ss) == sizeof(struct sockaddr_in) ? '1' : '2')

/** Length type for socket APIs (getsockname, accept, ...). */
typedef socklen_t SOClen;

#if HTS_INET6 != 0
/* Engine-only: not exported, and the type is useless without the setter. */
#ifdef HTS_INTERNAL_BYTECODE
/** Calling convention of the resolver entry points. Winsock's resolver is
    __stdcall, and a plain pointer only compiles on x64, where there is one
    convention. A backend implementation must carry it too. */
#ifdef _WIN32
#define HTS_RESOLVER_CALL WSAAPI
#else
#define HTS_RESOLVER_CALL
#endif

/* File scope, or the tag below is a fresh type scoped to its own prototype
   wherever <netdb.h> has not already declared it. */
struct addrinfo;

/** getaddrinfo and freeaddrinfo as a swappable pair, so a self-test can
    script DNS answers in-process: families, multiplicity and errors. */
typedef struct hts_resolver_backend {
  /** Resolves a name, with getaddrinfo()'s arguments and return codes. */
  int(HTS_RESOLVER_CALL *getaddrinfo)(const char *node, const char *service,
                                      const struct addrinfo *hints,
                                      struct addrinfo **res);
  /** Frees a chain this same backend's getaddrinfo returned, which is why the
      two travel as a pair. */
  void(HTS_RESOLVER_CALL *freeaddrinfo)(struct addrinfo *res);
} hts_resolver_backend;

/** Install a resolver backend for the whole process, or pass NULL to restore
    the libc pair. Only the pointer is stored, so @p backend must outlive the
    setting. Test-only seam, and not thread-safe, so serialize it against
    resolves. */
void hts_dns_set_resolver_backend(const hts_resolver_backend *backend);
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif
