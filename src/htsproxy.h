/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 2026 Xavier Roche and other contributors

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
/* File: Proxy tunneling (HTTP CONNECT, SOCKS5) .h              */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#ifndef HTS_DEFPROXY
#define HTS_DEFPROXY

#include "htsglobal.h"

/* Forward definitions */
#ifndef HTS_DEF_FWSTRUCT_httrackp
#define HTS_DEF_FWSTRUCT_httrackp
typedef struct httrackp httrackp;
#endif
#ifndef HTS_DEF_FWSTRUCT_htsblk
#define HTS_DEF_FWSTRUCT_htsblk
typedef struct htsblk htsblk;
#endif

/* Open an HTTP CONNECT tunnel through the active proxy for an https request:
   `retour->soc` must already be TCP-connected to the proxy, and `adr` is the
   origin authority (url_adr, e.g. "https://host:port"). Sends the CONNECT
   request (with Proxy-Authorization when the proxy carries credentials) and
   reads the proxy's status line, so the caller's TLS handshake then runs
   end-to-end with the origin. Blocks up to `timeout` seconds. Returns 1 on a
   2xx tunnel, 0 on failure (retour->msg/statuscode set). */
int http_proxy_tunnel(httrackp *opt, htsblk *retour, const char *adr,
                      int timeout);

/* SOCKS5 (RFC 1928, RFC 1929 auth) handshake on the raw proxy socket, before
   any TLS: `retour->soc` must be TCP-connected to the proxy and `adr` is the
   origin (url_adr). The origin name is sent verbatim (ATYP=domain), so the
   proxy resolves it; a bracketed IPv6 literal origin is rejected. Blocks up to
   `timeout` seconds. Returns 1 with the stream positioned on the first origin
   byte, 0 on failure (retour->msg set). */
int socks5_handshake(httrackp *opt, htsblk *retour, const char *adr,
                     int timeout);

/* Self-test hook (-#test=socks5): run the handshake against `reply`, a scripted
   server byte stream, instead of a socket. `sent` captures the client frames,
   `consumed` the reply bytes eaten (the frame-desync guard). */
typedef struct socks5_test_io {
  const unsigned char *reply;
  size_t reply_len;
  size_t consumed;
  unsigned char sent[1024];
  size_t sent_len;
  char msg[256];
} socks5_test_io;

int socks5_handshake_scripted(httrackp *opt, const char *adr,
                              const char *proxy_name, socks5_test_io *io);

#endif
