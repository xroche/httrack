/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) Xavier Roche and other contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 3
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
/*       Used in .c and .h files that needs hostent and so      */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTS_DEFBASENETH
#define HTS_DEFBASENETH

#ifdef _WIN32

#if HTS_INET6==0
 #include <winsock2.h>
#else

#ifndef _WIN32_WCE
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
#else
 #include <winsock2.h>
 #include <socket.h>
#endif

#endif

typedef struct hostent FAR t_hostent;

#else
#define HTS_USESCOPEID
 #define INVALID_SOCKET -1
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

/*
#include <openssl/ssl.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
*/

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE

/* OpenSSL definitions */
#define SSL_shutdown hts_ptrfunc_SSL_shutdown
#define SSL_free hts_ptrfunc_SSL_free
#define SSL_new hts_ptrfunc_SSL_new
#define SSL_clear hts_ptrfunc_SSL_clear
#define SSL_set_fd hts_ptrfunc_SSL_set_fd
#define SSL_set_connect_state hts_ptrfunc_SSL_set_connect_state
#define SSL_connect hts_ptrfunc_SSL_connect
#define SSL_get_error hts_ptrfunc_SSL_get_error
#define SSL_write hts_ptrfunc_SSL_write
#define SSL_read hts_ptrfunc_SSL_read
#define SSL_library_init hts_ptrfunc_SSL_library_init
#define ERR_load_crypto_strings hts_ptrfunc_ERR_load_crypto_strings
#define ERR_load_SSL_strings hts_ptrfunc_ERR_load_SSL_strings
#define SSLv23_client_method hts_ptrfunc_SSLv23_client_method
#define SSL_CTX_new hts_ptrfunc_SSL_CTX_new
#define ERR_error_string hts_ptrfunc_ERR_error_string
#define SSL_load_error_strings hts_ptrfunc_SSL_load_error_strings
#define SSL_CTX_ctrl hts_ptrfunc_SSL_CTX_ctrl

#endif

/* */
typedef void SSL_CTX;
typedef void* SSL;
typedef void SSL_METHOD;
typedef int (*t_SSL_shutdown)(SSL *ssl);
typedef void (*t_SSL_free)(SSL *ssl);
typedef SSL (*t_SSL_new)(SSL_CTX *ctx);
typedef int (*t_SSL_clear)(SSL *ssl);
typedef int (*t_SSL_set_fd)(SSL *ssl, int fd);
typedef void (*t_SSL_set_connect_state)(SSL *ssl);
typedef int (*t_SSL_connect)(SSL *ssl);
typedef int (*t_SSL_get_error)(SSL *ssl, int ret);
typedef int (*t_SSL_write)(SSL *ssl, const void *buf, int num);
typedef int (*t_SSL_read)(SSL *ssl, void *buf, int num);
typedef int (*t_SSL_library_init)(void);
typedef void (*t_ERR_load_crypto_strings)(void);
typedef void (*t_ERR_load_SSL_strings)(void);
typedef SSL_METHOD * (*t_SSLv23_client_method)(void);
typedef SSL_CTX * (*t_SSL_CTX_new)(SSL_METHOD *method);
typedef char * (*t_ERR_error_string)(unsigned long e, char *buf);
typedef void (*t_SSL_load_error_strings)(void);
typedef long (*t_SSL_CTX_ctrl)(SSL_CTX *ctx, int cmd, long larg, char *parg);

/* Library internal definictions */
#ifdef HTS_INTERNAL_BYTECODE

extern int SSL_is_available;
extern t_SSL_shutdown SSL_shutdown;
extern t_SSL_free SSL_free;
extern t_SSL_new SSL_new;
extern t_SSL_clear SSL_clear;
extern t_SSL_set_fd SSL_set_fd;
extern t_SSL_set_connect_state SSL_set_connect_state;
extern t_SSL_connect SSL_connect;
extern t_SSL_get_error SSL_get_error;
extern t_SSL_write SSL_write;
extern t_SSL_read SSL_read;
extern t_SSL_library_init SSL_library_init;
extern t_ERR_load_crypto_strings ERR_load_crypto_strings;
extern t_ERR_load_SSL_strings ERR_load_SSL_strings;
extern t_SSLv23_client_method SSLv23_client_method;
extern t_SSL_CTX_new SSL_CTX_new;
extern t_ERR_error_string ERR_error_string;
extern t_SSL_load_error_strings SSL_load_error_strings;
extern t_SSL_CTX_ctrl SSL_CTX_ctrl;

#endif

/*
From /usr/include/openssl/ssl.h
*/
#define SSL_ERROR_NONE                  0
#define SSL_ERROR_SSL                   1
#define SSL_ERROR_WANT_READ             2
#define SSL_ERROR_WANT_WRITE            3
#define SSL_ERROR_WANT_X509_LOOKUP      4
#define SSL_ERROR_SYSCALL               5 /* look at error stack/return value/errno */
#define SSL_ERROR_ZERO_RETURN           6
#define SSL_ERROR_WANT_CONNECT          7
#define SSL_OP_ALL                                      0x000FFFFFL
#define SSL_CTRL_OPTIONS                        32
#define SSL_CTX_set_options(ctx,op) \
        SSL_CTX_ctrl(ctx,SSL_CTRL_OPTIONS,op,NULL)

//#include <openssl/bio.h>
/* OpenSSL structure */
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

#endif
