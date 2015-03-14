/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998-2015 Xavier Roche and other contributors

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

Important notes:

- We hereby ask people using this source NOT to use it in purpose of grabbing
emails addresses, or collecting any other private information on persons.
This would disgrace our work, and spoil the many hours we spent on it.

Please visit our Website: http://www.httrack.com
*/

/* ------------------------------------------------------------ */
/* File: Net definitions                                        */
/*       Used in .c files that needs connect() functions and so */
/* Note: includes htsbasenet.h                                  */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTS_DEFNETH
#define HTS_DEFNETH

/* basic net definitions */
#include "htsglobal.h"
#include "htsbasenet.h"
#include "htssafe.h"

#include <string.h>
#include <ctype.h>
#ifdef _WIN32
 // pour read
#include <io.h>
 // pour FindFirstFile
#include <winbase.h>
typedef USHORT in_port_t;
typedef ADDRESS_FAMILY sa_family_t;
#else
 //typedef int T_SOC;
#define INVALID_SOCKET -1
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
 /* Force for sun env. */
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
 // pas la peine normalement..
#ifndef HTS_DO_NOT_REDEFINE_in_addr_t
typedef unsigned long in_addr_t;
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Ipv4 structures */
#if HTS_INET6 != 0
typedef struct in6_addr INaddr;
#else
typedef struct in_addr INaddr;
#endif

/* This should handle all cases */
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

static HTS_INLINE HTS_UNUSED in_port_t* SOCaddr_sinport_(SOCaddr *const addr,
                                                         const char *file, const int line) {
  assertf_(addr != NULL, file, line);
  switch(addr->m_addr.sa.sa_family) {
  case AF_INET:
    return &addr->m_addr.in.sin_port;
    break;
#if HTS_INET6 != 0
  case AF_INET6:
    return &addr->m_addr.in6.sin6_port;
    break;
#endif
  default:
    assertf_(! "invalid structure", file, line);
    return 0;
    break;
  }
}

static HTS_INLINE HTS_UNUSED socklen_t SOCaddr_size_(const SOCaddr*const addr,
                                                     const char *file, const int line) {
  assertf_(addr != NULL, file, line);
  switch(addr->m_addr.sa.sa_family) {
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

static HTS_INLINE HTS_UNUSED void SOCaddr_clear_(SOCaddr*const addr,
                                                 const char *file, const int line) {
  assertf_(addr != NULL, file, line);
  addr->m_addr.sa.sa_family = AF_UNSPEC;
}

/* Ipv4/6 structure members */
#define SOCaddr_sinfamily(server)   ((server).m_addr.sa.sa_family)
#define SOCaddr_sinport(server)     (*SOCaddr_sinport_(&(server), __FILE__, __LINE__))
#define SOCaddr_size(server)        (SOCaddr_size_(&(server), __FILE__, __LINE__))
#define SOCaddr_is_valid(server)    (SOCaddr_size_(&(server), __FILE__, __LINE__) != 0 )
#define SOCaddr_clear(server)       SOCaddr_clear_(&(server), __FILE__, __LINE__)
#define SOCaddr_sockaddr(server)    ((server).m_addr.sa)
#define SOCaddr_capacity(server)    sizeof((server).m_addr)

/* AF_xx */
#if HTS_INET6 != 0
#define AFinet AF_INET6
#else
#define AFinet AF_INET
#endif

/* Set port to sockaddr structure */
#define SOCaddr_initport(server, port) do { \
  SOCaddr_sinport(server) = htons((in_port_t) (port)); \
} while(0)

static HTS_INLINE HTS_UNUSED socklen_t SOCaddr_initany_(SOCaddr*const addr,
                                                        const char *file, const int line) {
  assertf_(addr != NULL, file, line);
  memset(&addr->m_addr.in, 0, sizeof(addr->m_addr.in));
  addr->m_addr.in.sin_family = AF_INET;
  return SOCaddr_size_(addr, file, line);
}

#define SOCaddr_initany(server) do { \
  SOCaddr_initany_(&(server), __FILE__, __LINE__); \
} while(0)

/*
  Copy sockaddr_in/sockaddr_in6/raw IPv4/raw IPv6 to our opaque SOCaddr
*/
static HTS_UNUSED socklen_t SOCaddr_copyaddr_(SOCaddr*const server, 
                                              const void *data, const size_t data_size,
                                              const char *file, const int line) {
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

#define SOCaddr_copyaddr(server, server_len, hpaddr, hpsize) do { \
  server_len = (int) SOCaddr_copyaddr_(&(server), hpaddr, hpsize, __FILE__, __LINE__); \
} while(0)

#define SOCaddr_copyaddr2(server, hpaddr, hpsize) do { \
  (void) SOCaddr_copyaddr_(&(server), hpaddr, hpsize, __FILE__, __LINE__); \
} while(0)

#define SOCaddr_copy_SOCaddr(dest, src) do { \
  SOCaddr_copyaddr_(&(dest), &(src).m_addr.sa, SOCaddr_size(src), __FILE__, __LINE__); \
} while(0)

/* Get dotted address */

static HTS_UNUSED void SOCaddr_inetntoa_(char *namebuf, size_t namebuflen, 
                                         SOCaddr *const ss,
                                         const char *file, const int line) {
  assertf_(namebuf != NULL, file, line);
  assertf_(ss != NULL, file, line);

  if (getnameinfo(&ss->m_addr.sa, sizeof(ss->m_addr),
                  namebuf, namebuflen, 
                  NULL, 0, 
                  NI_NUMERICHOST) == 0) {
    /* remove scope id(s) */
    char *const pos = strchr(namebuf, '%');
    if (pos != NULL) {
      *pos = '\0';
    }
  } else {
    namebuf[0] = '\0';
  }
}

#define SOCaddr_inetntoa(namebuf, namebuflen, ss) \
  SOCaddr_inetntoa_(namebuf, namebuflen, &(ss), __FILE__, __LINE__)

/* Get protocol ID */
#define SOCaddr_getproto(ss) ( SOCaddr_size(ss) == sizeof(struct sockaddr_in) ? '1' : '2')

/* Socket length type */
typedef socklen_t SOClen;

#ifdef __cplusplus
}
#endif

#endif
