/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998-2017 Xavier Roche and other contributors

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
/* File: Basic net definitions                                  */
/*       Used in .c and .h files that needs hostent and so      */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTS_DEFBASENETH
#define HTS_DEFBASENETH

#ifdef _WIN32

#if HTS_INET6==0
#include <winsock2.h>
#else

#undef HTS_USESCOPEID
#define WIN32_LEAN_AND_MEAN
// KB955045 (http://support.microsoft.com/kb/955045)
// To execute an application using this function on earlier versions of Windows
// (Windows 2000, Windows NT, and Windows Me/98/95), then it is mandatary to #include Ws2tcpip.h
// and also Wspiapi.h. When the Wspiapi.h header file is included, the 'getaddrinfo' function is
// #defined to the 'WspiapiGetAddrInfo' inline function in Wspiapi.h. 
#include <ws2tcpip.h>
#include <Wspiapi.h>
//#include <winsock2.h>
//#include <tpipv6.h>

#endif

#else
#define HTS_USESCOPEID
#define INVALID_SOCKET -1
#endif

#ifdef __cplusplus
extern "C" {
#endif

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

/* Global SSL context */
extern SSL_CTX *openssl_ctx;

#endif
#endif

/** RFC2616 status-codes ('statuscode' member of htsblk) **/
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
  HTTP_INTERNAL_SERVER_ERROR = 500,
  HTTP_NOT_IMPLEMENTED = 501,
  HTTP_BAD_GATEWAY = 502,
  HTTP_SERVICE_UNAVAILABLE = 503,
  HTTP_GATEWAY_TIME_OUT = 504,
  HTTP_HTTP_VERSION_NOT_SUPPORTED = 505
} HTTPStatusCode;

/** Internal HTTrack status-codes ('statuscode' member of htsblk) **/
typedef enum BackStatusCode {
  STATUSCODE_INVALID = -1,
  STATUSCODE_TIMEOUT = -2,
  STATUSCODE_SLOW = -3,
  STATUSCODE_CONNERROR = -4,
  STATUSCODE_NON_FATAL = -5,
  STATUSCODE_SSL_HANDSHAKE = -6,
  STATUSCODE_TOO_BIG = -7,
  STATUSCODE_TEST_OK = -10
} BackStatusCode;

/** HTTrack status ('status' member of of 'lien_back') **/
typedef enum HTTrackStatus {
  STATUS_ALIVE = -103,
  STATUS_FREE = -1,
  STATUS_READY = 0,
  STATUS_TRANSFER = 1,
  STATUS_CHUNK_CR = 97,
  STATUS_CHUNK_WAIT = 98,
  STATUS_WAIT_HEADERS = 99,
  STATUS_CONNECTING = 100,
  STATUS_WAIT_DNS = 101,
  STATUS_SSL_WAIT_HANDSHAKE = 102,
  STATUS_FTP_TRANSFER = 1000,
  STATUS_FTP_READY = 1001
} HTTrackStatus;

#ifdef __cplusplus
}
#endif

#endif
