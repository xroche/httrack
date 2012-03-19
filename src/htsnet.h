/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) Xavier Roche and other contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.


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
#include "htsbasenet.h"

#include <ctype.h>
#if HTS_WIN
 // pour read
 #include <io.h>
 // pour FindFirstFile
 #include <winbase.h>
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
#ifdef HAVE_UNISTD_H
 #include <unistd.h>
#endif
 /* inet_addr */
 #include <arpa/inet.h>
 // pas la peine normalement..
#ifndef HTS_DO_NOT_REDEFINE_in_addr_t
 typedef unsigned long in_addr_t;
#endif
#undef min
#undef max
#undef Sleep
#define min(a,b) ((a)>(b)?(b):(a))
#define max(a,b) ((a)>(b)?(a):(b))
#define Sleep(a) { if (((a)*1000)%1000000) usleep(((a)*1000)%1000000); if (((a)*1000)/1000000) sleep(((a)*1000)/1000000); }
#endif

/*
   ** ipV4 **
*/
#if HTS_INET6==0

/* Ipv4 structures */
typedef struct in_addr INaddr;
/* This should handle all cases */
typedef struct {
  union {
    struct sockaddr_in in;
    struct sockaddr sa;
    unsigned char v4data[4];
    unsigned char v6data[16];
    unsigned char pad[128];
  } m_addr;
} SOCaddr;

/* Ipv4 structure members */
#define SOCaddr_sinaddr(server) ((server).m_addr.in.sin_addr)
#define SOCaddr_sinfamily(server) ((server).m_addr.in.sin_family)
#define SOCaddr_sinport(server) ((server).m_addr.in.sin_port)

/* AF_xx */
#define AFinet AF_INET

/* Set port to sockaddr structure */
#define SOCaddr_initport(server, port) do { \
  SOCaddr_sinport(server) = htons((unsigned short int) (port)); \
} while(0)

#define SOCaddr_initany(server, server_len) do { \
  SOCaddr_sinfamily(server) = AF_INET; \
  memset(&SOCaddr_sinaddr(server), 0, sizeof(struct sockaddr_in)); \
  server_len=sizeof(struct sockaddr_in); \
} while(0)


/* Copy sockaddr to another one */
#define SOCaddr_copyaddr(server, server_len, hpaddr, hpsize) do { \
if (hpsize == sizeof(struct sockaddr_in)) { \
  server_len=sizeof(struct sockaddr_in); \
  SOCaddr_sinfamily(server) = (*(struct sockaddr_in*)(hpaddr)).sin_family; \
  memcpy(&SOCaddr_sinaddr(server), &(*(struct sockaddr_in*)(hpaddr)).sin_addr, sizeof(SOCaddr_sinaddr(server))); \
} else if (hpsize == 4) {\
  server_len=sizeof(struct sockaddr_in); \
  SOCaddr_sinfamily(server) = AF_INET; \
  memcpy(&SOCaddr_sinaddr(server), (hpaddr), sizeof(SOCaddr_sinaddr(server))); \
} else if ((hpsize > 0) && (hpsize <= sizeof(server))) { \
  server_len=hpsize; \
  memcpy(&(server), hpaddr, hpsize); \
} else { \
  server_len=0; \
} \
} while(0)

/* Get dotted address */
#define SOCaddr_inetntoa(namebuf, namebuflen, ss, sslen) do { \
char* dot = (char*) inet_ntoa(SOCaddr_sinaddr(ss)); \
(namebuf)[0]='\0'; \
if (dot) { \
strcpy(namebuf, dot); \
} \
} while(0)

/* Get protocol ID */
#define SOCaddr_getproto(ss, sslen) ('1')

/*
   ** ipV6 **
*/
#else

/* Ipv4 structures */
typedef struct in6_addr INaddr;
/* This should handle all cases */
typedef struct {
  union {
    struct sockaddr_in6 in6;
    struct sockaddr_in in;
    struct sockaddr sa;
    unsigned char v4data[4];
    unsigned char v6data[16];
    unsigned char pad[128];
  } m_addr;
} SOCaddr;

/* Ipv4 structure members */
#define SOCaddr_sinaddr(server)     ((server).m_addr.in6.sin6_addr)
#define SOCaddr_sinfamily(server)   ((server).m_addr.in6.sin6_family)
#define SOCaddr_sinport(server)     ((server).m_addr.in6.sin6_port)
#define SOCaddr_sinflowinfo(server) ((server).m_addr.in6.sin6_flowinfo)
/* #define SOCaddr_sinscopeid(a)     ((a).m_addr.in6.sin6_scope_id) */

/* AF_xx */
#define AFinet AF_INET6

/* Set port to sockaddr structure */
#define SOCaddr_initport(server, port) do { \
    SOCaddr_sinport(server) = htons((unsigned short int) (port)); \
} while(0)

#define SOCaddr_initany(server, server_len) do { \
  SOCaddr_sinfamily(server) = AF_INET; \
  memset(&SOCaddr_sinaddr(server), 0, sizeof(struct sockaddr_in)); \
  server_len=sizeof(struct sockaddr_in); \
} while(0)

/*
  Copy sockaddr to SOCaddr
 
  Note;
  The '> sizeof(struct sockaddr_in6)' hack if for the VC6 structure which
  lacks the scope id
*/
#define SOCaddr_copyaddr(server, server_len, hpaddr, hpsize) do { \
if (hpsize == sizeof(struct sockaddr_in6)) { \
  server_len=sizeof(struct sockaddr_in6); \
  SOCaddr_sinfamily(server) = (*(struct sockaddr_in6*)(hpaddr)).sin6_family; \
  SOCaddr_sinflowinfo(server) = (*(struct sockaddr_in6*)(hpaddr)).sin6_flowinfo; \
  memcpy(&SOCaddr_sinaddr(server), &(*(struct sockaddr_in6*)(hpaddr)).sin6_addr, sizeof(SOCaddr_sinaddr(server))); \
} else if (hpsize > sizeof(struct sockaddr_in6)) { \
  server_len=hpsize; \
  memcpy(&(server), hpaddr, hpsize); \
} else if (hpsize == sizeof(struct sockaddr_in)) { \
  server_len=sizeof(struct sockaddr_in); \
  (*(struct sockaddr_in*)(&server)).sin_family = AF_INET; \
  memcpy(&(*(struct sockaddr_in*)&(server)).sin_addr, &(*(struct sockaddr_in*)(hpaddr)).sin_addr, sizeof((*(struct sockaddr_in*)(hpaddr)).sin_addr)); \
} else if (hpsize == 4) {\
  server_len=sizeof(struct sockaddr_in); \
  (*(struct sockaddr_in*)(&server)).sin_family = AF_INET; \
  memcpy(&(*(struct sockaddr_in*)&(server)).sin_addr, hpaddr, 4); \
} else if (hpsize == 16) {\
  server_len=sizeof(struct sockaddr_in6); \
  SOCaddr_sinfamily(server) = AF_INET6; \
  memcpy(&SOCaddr_sinaddr(server), (hpaddr), 16); \
} else if ((hpsize > 0) && (hpsize <= sizeof(server))) { \
  server_len=hpsize; \
  memcpy(&(server), hpaddr, hpsize); \
} else { \
  server_len=0; \
} \
} while(0)

/* Get dotted address */
#define SOCaddr_inetntoa(namebuf, namebuflen, ss, sslen) do { \
(namebuf)[0]='\0'; \
getnameinfo((struct sockaddr *)&(ss), sslen, \
            (namebuf), namebuflen, NULL, 0, NI_NUMERICHOST); \
} while(0)

/* Get protocol ID */
#define SOCaddr_getproto(ss, sslen) ((sslen == sizeof(struct sockaddr_in6))?('2'):('1'))

#endif

/* Buffer structure to copy various hostent structures */
typedef struct {
  t_hostent hp;
  char* list[2];
  char addr[HTS_MAXADDRLEN];    /* various struct sockaddr structures */
  unsigned int addr_maxlen;
} t_fullhostent;

/* Initialize a t_fullhostent structure */
#define fullhostent_init(h) do { \
memset((h), 0, sizeof(t_fullhostent)); \
(h)->hp.h_addr_list = (char **) & ((h)->list); \
(h)->list[0] = (char *) & ((h)->addr); \
(h)->list[1] = NULL; \
(h)->addr_maxlen = HTS_MAXADDRLEN; \
} while(0)


#endif


