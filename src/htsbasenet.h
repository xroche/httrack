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
/* File: Basic net definitions                                  */
/*       Used in .c and .h files that needs hostent and so      */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/** @file htsbasenet.h
    Base networking definitions: the platform socket headers, the process-wide
    OpenSSL context, and the codes held by htsblk.statuscode and
    lien_back.status. A consumer that interprets either field needs the names
    here. */

#ifndef HTS_DEFBASENETH
#define HTS_DEFBASENETH

/* Must precede the HTS_INET6 and HTS_USEOPENSSL tests below, which
   HTS_DEFBASENETH lets only the first include reach. */
#include "htsglobal.h"

/* Windows needs its socket headers here, for hostent and SOCKET. On POSIX they
   come from htsnet.h and only INVALID_SOCKET is defined below. */
#ifdef _WIN32

#if HTS_INET6 == 0
#include <winsock2.h>
#else

#undef HTS_USESCOPEID
#define WIN32_LEAN_AND_MEAN
// KB955045 (http://support.microsoft.com/kb/955045)
// To execute an application using this function on earlier versions of Windows
// (Windows 2000, Windows NT, and Windows Me/98/95), then it is mandatary to
// #include Ws2tcpip.h and also Wspiapi.h. When the Wspiapi.h header file is
// included, the 'getaddrinfo' function is #defined to the 'WspiapiGetAddrInfo'
// inline function in Wspiapi.h.
#include <ws2tcpip.h>
#include <Wspiapi.h>

#endif

#else
/** Defined on the POSIX build and cleared on the Windows IPv6 build, but
    nothing in the tree reads it. */
#define HTS_USESCOPEID
/** POSIX's invalid socket, spelled with Winsock's name so one test serves
    both platforms. */
#define INVALID_SOCKET -1
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* HTS_USEOPENSSL adds two fields to htsblk, so a consumer must compile with
   the value the library was built with, and needs the OpenSSL headers too. */
#if HTS_USEOPENSSL
/*
   OpensSSL crypto routines by Eric Young (eay@cryptsoft.com)
   Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
   All rights reserved
*/
#ifndef HTS_OPENSSL_H_INCLUDED
#define HTS_OPENSSL_H_INCLUDED

/* OpenSSL definitions */
#include <openssl/ssl.h>
#include <openssl/crypto.h>
#include <openssl/err.h>

/* OpenSSL structure */
#include <openssl/bio.h>

/* Engine-only: not exported, so the installed header must not offer it. */
#ifdef HTS_INTERNAL_BYTECODE
/** Process-wide OpenSSL client context, shared by every TLS connection.
    hts_init() creates it, so it is NULL until then, and nothing frees it. */
extern SSL_CTX *openssl_ctx;
#endif

#endif
#endif

/** HTTP status codes as read off the wire, stored in htsblk.statuscode. */
typedef enum HTTPStatusCode {
  HTTP_CONTINUE = 100,
  HTTP_SWITCHING_PROTOCOLS = 101,
  HTTP_OK = 200,
  HTTP_CREATED = 201,
  HTTP_ACCEPTED = 202,
  HTTP_NON_AUTHORITATIVE_INFORMATION = 203,
  HTTP_NO_CONTENT = 204,
  HTTP_RESET_CONTENT = 205,
  HTTP_PARTIAL_CONTENT = 206,
  HTTP_MULTIPLE_CHOICES = 300,
  HTTP_MOVED_PERMANENTLY = 301,
  HTTP_FOUND = 302,
  HTTP_SEE_OTHER = 303,
  HTTP_NOT_MODIFIED = 304,
  HTTP_USE_PROXY = 305,
  HTTP_TEMPORARY_REDIRECT = 307,
  HTTP_BAD_REQUEST = 400,
  HTTP_UNAUTHORIZED = 401,
  HTTP_PAYMENT_REQUIRED = 402,
  HTTP_FORBIDDEN = 403,
  HTTP_NOT_FOUND = 404,
  HTTP_METHOD_NOT_ALLOWED = 405,
  HTTP_NOT_ACCEPTABLE = 406,
  HTTP_PROXY_AUTHENTICATION_REQUIRED = 407,
  HTTP_REQUEST_TIME_OUT = 408,
  HTTP_CONFLICT = 409,
  HTTP_GONE = 410,
  HTTP_LENGTH_REQUIRED = 411,
  HTTP_PRECONDITION_FAILED = 412,
  HTTP_REQUEST_ENTITY_TOO_LARGE = 413,
  HTTP_REQUEST_URI_TOO_LARGE = 414,
  HTTP_UNSUPPORTED_MEDIA_TYPE = 415,
  HTTP_REQUESTED_RANGE_NOT_SATISFIABLE = 416,
  HTTP_EXPECTATION_FAILED = 417,
  HTTP_TOO_MANY_REQUESTS = 429,
  HTTP_UNAVAILABLE_FOR_LEGAL_REASONS = 451,
  HTTP_INTERNAL_SERVER_ERROR = 500,
  HTTP_NOT_IMPLEMENTED = 501,
  HTTP_BAD_GATEWAY = 502,
  HTTP_SERVICE_UNAVAILABLE = 503,
  HTTP_GATEWAY_TIME_OUT = 504,
  HTTP_HTTP_VERSION_NOT_SUPPORTED = 505
} HTTPStatusCode;

/** HTTrack's own status codes, stored in htsblk.statuscode beside the HTTP
    ones. A fresh htsblk starts at STATUSCODE_INVALID. */
typedef enum BackStatusCode {
  STATUSCODE_INVALID = -1,       /**< no usable response, and no retry */
  STATUSCODE_TIMEOUT = -2,       /**< the slot ran out of --timeout */
  STATUSCODE_SLOW = -3,          /**< the transfer stayed under --min-rate */
  STATUSCODE_CONNERROR = -4,     /**< the connection failed, or died later */
  STATUSCODE_NON_FATAL = -5,     /**< another error, and a retry may follow */
  STATUSCODE_SSL_HANDSHAKE = -6, /**< the TLS handshake failed */
  STATUSCODE_TOO_BIG = -7,       /**< the body was too big, and no retry */
  STATUSCODE_TEST_OK = -10,      /**< --test found the link alive */
  STATUSCODE_EXCLUDED = -11,     /**< a -mime: filter refused the type */
  STATUSCODE_IO_FATAL = -12,     /**< a write failed, and the mirror stops */
  STATUSCODE_IO_ERROR = -13      /**< a write failed, but the mirror goes on */
} BackStatusCode;

/** Is code one of the two write-error classes? r.size counts bytes read, so it
    cannot tell a complete body from one a failed write cut short. */
static HTS_INLINE HTS_UNUSED hts_boolean statuscode_is_write_error(int code) {
  return code == STATUSCODE_IO_FATAL || code == STATUSCODE_IO_ERROR ? HTS_TRUE
                                                                    : HTS_FALSE;
}

/** Connection state of a backing slot, the 'status' member of lien_back. The
    numbers are ordered on purpose, because the engine tests ranges: above zero
    is a slot in flight, and 1000 or more an FTP one. A transfer ends at
    STATUS_READY, then the slot returns to STATUS_FREE once the crawler has
    taken the result. An HTTPS slot reaches STATUS_CONNECTING twice, once for
    connect() and once after the handshake, so that branch runs twice. */
typedef enum HTTrackStatus {
  STATUS_ALIVE = -103,             /**< keep-alive socket, no request on it */
  STATUS_FREE = -1,                /**< slot unused, and free to take */
  STATUS_READY = 0,                /**< transfer over, result not taken yet */
  STATUS_TRANSFER = 1,             /**< reading the body, or a local file */
  STATUS_CHUNK_CR = 97,            /**< reading the CRLF ending a chunk */
  STATUS_CHUNK_WAIT = 98,          /**< reading the next chunk's hex size */
  STATUS_WAIT_HEADERS = 99,        /**< request sent, headers still coming */
  STATUS_CONNECTING = 100,         /**< waiting for connect() to finish */
  STATUS_WAIT_DNS = 101,           /**< resolving the host name */
  STATUS_SSL_WAIT_HANDSHAKE = 102, /**< running the TLS handshake */
  STATUS_FTP_TRANSFER = 1000,      /**< an FTP worker owns slot and socket */
  STATUS_FTP_READY = 1001          /**< the FTP worker is done, so reap it */
} HTTrackStatus;

#ifdef __cplusplus
}
#endif

#endif
