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
/* File: Basic net definitions                                  */
/*       Used in .c and .h files that needs T_SOC and so        */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTS_DEFBASENETH
#define HTS_DEFBASENETH

#if HTS_WIN

#if HTS_INET6==0
 #include <winsock.h>
#else
#undef HTS_USESCOPEID
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <tpipv6.h>
#endif
 typedef SOCKET T_SOC;
 typedef struct hostent FAR t_hostent;

#else
#define HTS_USESCOPEID
 #define INVALID_SOCKET -1
 typedef int T_SOC;
 typedef struct hostent t_hostent;
#endif

#if HTS_USEOPENSSL
/*
   OpensSSL crypto routines by Eric Young (eay@cryptsoft.com)
   Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
   All rights reserved
*/
#ifndef HTS_OPENSSL_H_INCLUDED
#define HTS_OPENSSL_H_INCLUDED
#ifdef __cplusplus
extern "C" {
#endif
#include <openssl/ssl.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
//#include <openssl/bio.h>
#ifdef __cplusplus
 };
#endif
/* OpenSSL structure */
extern SSL_CTX *openssl_ctx;

#endif
#endif

#endif
