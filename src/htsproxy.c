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
/* File: Proxy tunneling (HTTP CONNECT, SOCKS5)                 */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Internal engine bytecode */
#define HTS_INTERNAL_BYTECODE

#include "htscore.h"

#include "htslib.h"
#include "htsproxy.h"

#include <string.h>

// Read a CRLF line from a non-blocking socket (waits up to timeout per recv).
// Returns the line length (0 = empty), or -1 on timeout/EOF/error.
static int proxy_getline(T_SOC soc, char *s, int max, int timeout) {
  int j = 0;

  for (;;) {
    unsigned char ch;
    int n;

    if (!check_readinput_t(soc, timeout))
      return -1; // timed out waiting for data
    n = (int) recv(soc, &ch, 1, 0);
    if (n == 1) {
      if (ch == 13) // CR
        continue;
      if (ch == 10) // LF: end of line
        break;
      if (j >= max - 1)
        return -1; // line too long: bound the read against a hostile proxy
      s[j++] = (char) ch;
    } else if (n == 0) {
      return -1; // connection closed
    } else {
#ifdef _WIN32
      if (WSAGetLastError() == WSAEWOULDBLOCK)
        continue;
#else
      if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
        continue;
#endif
      return -1;
    }
  }
  s[j] = '\0';
  return j;
}

int http_proxy_tunnel(httrackp *opt, htsblk *retour, const char *adr,
                      int timeout) {
  const T_SOC soc = retour->soc;
  const char *const host = jump_identification_const(adr); // host[:port]
  const char *const portsep = jump_toport_const(adr);      // ":port" or NULL
  char BIGSTK authority[HTS_URLMAXSIZE * 2];
  char BIGSTK req[HTS_URLMAXSIZE * 4 + 1100];
  char line[1024];
  int code;

  if (soc == INVALID_SOCKET)
    return 0;

  // CONNECT needs an explicit host:port; default :80 for http, :443 for https
  authority[0] = '\0';
  if (portsep != NULL)
    strlcatbuff(authority, host, sizeof(authority)); // already host:port
  else {
    const int defport = (strncmp(adr, "https://", 8) == 0) ? 443 : 80;

    snprintf(authority, sizeof(authority), "%s:%d", host, defport);
  }

  // backstop: never let a stray CR/LF in the host smuggle a second line into
  // the CONNECT request (the host is already sanitized upstream)
  {
    const char *c;

    for (c = authority; *c != '\0'; c++) {
      if ((unsigned char) *c < ' ') {
        strcpybuff(retour->msg, "proxy CONNECT: invalid host");
        return 0;
      }
    }
  }

  snprintf(req, sizeof(req), "CONNECT %s HTTP/1.0" H_CRLF "Host: %s" H_CRLF,
           authority, authority);

  // creds go on the CONNECT, not the tunneled origin request
  if (link_has_authorization(retour->req.proxy.name)) {
    const char *a = jump_identification_const(retour->req.proxy.name);
    const char *astart = jump_protocol_const(retour->req.proxy.name);
    char autorisation[1100];
    char user_pass[256];

    autorisation[0] = user_pass[0] = '\0';
    strncatbuff(user_pass, astart, (int) (a - astart) - 1);
    strcpybuff(user_pass, unescape_http(OPT_GET_BUFF(opt),
                                        OPT_GET_BUFF_SIZE(opt), user_pass));
    code64((unsigned char *) user_pass, (int) strlen(user_pass),
           (unsigned char *) autorisation, 0);
    strlcatbuff(req, "Proxy-Authorization: Basic ", sizeof(req));
    strlcatbuff(req, autorisation, sizeof(req));
    strlcatbuff(req, H_CRLF, sizeof(req));
  }
  strlcatbuff(req, H_CRLF, sizeof(req)); // end of request headers

  // raw send(): sendc() would route to TLS when ssl is set (https tunnel)
  {
    const char *p = req;
    size_t remain = strlen(req);
    int stalls = 0;

    while (remain > 0) {
      const int n = (int) send(soc, p, (int) remain, 0);

      if (n > 0) {
        p += n;
        remain -= (size_t) n;
        stalls = 0;
      } else {
#ifdef _WIN32
        const int wouldblock = (WSAGetLastError() == WSAEWOULDBLOCK);
#else
        const int wouldblock =
            (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR);
#endif
        // don't spin forever on a fatal error or an unwritable socket
        if (!wouldblock || !check_writeinput_t(soc, timeout) ||
            ++stalls > 100) {
          strcpybuff(retour->msg, "proxy CONNECT: write error");
          return 0;
        }
      }
    }
  }

  // proxy status line: "HTTP/1.x <code> ..."
  if (proxy_getline(soc, line, sizeof(line), timeout) < 0) {
    strcpybuff(retour->msg, "proxy CONNECT: no response");
    return 0;
  }
  if (sscanf(line, "HTTP/%*d.%*d %d", &code) < 1)
    code = 0;
  if (code < 200 || code >= 300) {
    snprintf(retour->msg, sizeof(retour->msg), "proxy CONNECT refused: %s",
             strnotempty(line) ? line : "(no status)");
    return 0;
  }

  // drain headers to the blank line; cap the count so a flooding proxy can't
  // stall the crawl
  {
    int headers = 0;

    for (;;) {
      const int n = proxy_getline(soc, line, sizeof(line), timeout);

      if (n < 0) {
        strcpybuff(retour->msg, "proxy CONNECT: truncated response");
        return 0;
      }
      if (n == 0)
        break; // blank line: tunnel ready
      if (++headers > 64) {
        strcpybuff(retour->msg, "proxy CONNECT: too many response headers");
        return 0;
      }
    }
  }

  return 1;
}

/* SOCKS5 client (RFC 1928, RFC 1929 auth), hostname mode only: the proxy
   resolves the origin name (remote DNS, curl's socks5h). The stream is the
   proxy socket, or a scripted buffer under -#test=socks5. */

#define SOCKS5_VERSION 0x05
#define SOCKS5_AUTH_VERSION 0x01
#define SOCKS5_METHOD_NONE 0x00
#define SOCKS5_METHOD_USERPASS 0x02
#define SOCKS5_METHOD_NOACCEPTABLE 0xFF
#define SOCKS5_CMD_CONNECT 0x01
#define SOCKS5_ATYP_IPV4 0x01
#define SOCKS5_ATYP_DOMAIN 0x03
#define SOCKS5_ATYP_IPV6 0x04
#define SOCKS5_MAXFIELD 255 /* one length byte: host, user and password */

typedef struct socks5_stream {
  T_SOC soc; /* INVALID_SOCKET when scripted (self-test) */
  int timeout;
  socks5_test_io *io;
} socks5_stream;

static int socks5_fail(char *msg, size_t msgsize, const char *text) {
  if (msgsize != 0)
    strlcpybuff(msg, text, msgsize);
  return 0;
}

/* Read exactly n bytes, or fail: SOCKS frames are not self-delimiting, so a
   short read would desync the stream shared with the origin traffic. */
static int socks5_read_n(socks5_stream *st, unsigned char *buf, size_t n) {
  size_t got = 0;
  int stalls = 0;

  if (st->soc == INVALID_SOCKET) { /* scripted */
    socks5_test_io *const io = st->io;

    if (n > io->reply_len - io->consumed)
      return 0;
    memcpy(buf, io->reply + io->consumed, n);
    io->consumed += n;
    return 1;
  }
  while (got < n) {
    const int r = (int) recv(st->soc, (char *) buf + got, (int) (n - got), 0);

    if (r > 0) {
      got += (size_t) r;
      stalls = 0;
    } else if (r == 0) {
      return 0; // proxy closed mid-frame
    } else {
#ifdef _WIN32
      const int wouldblock = (WSAGetLastError() == WSAEWOULDBLOCK);
#else
      const int wouldblock =
          (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR);
#endif
      if (!wouldblock || !check_readinput_t(st->soc, st->timeout) ||
          ++stalls > 100)
        return 0;
    }
  }
  return 1;
}

static int socks5_write_all(socks5_stream *st, const unsigned char *buf,
                            size_t len) {
  size_t remain = len;
  int stalls = 0;

  if (st->soc == INVALID_SOCKET) { /* scripted */
    socks5_test_io *const io = st->io;

    if (len > sizeof(io->sent) - io->sent_len)
      return 0;
    memcpy(io->sent + io->sent_len, buf, len);
    io->sent_len += len;
    return 1;
  }
  while (remain > 0) {
    // raw send: the socket is still plain here, sendc() would route to TLS
    const int n = (int) send(st->soc, (const char *) buf, (int) remain, 0);

    if (n > 0) {
      buf += n;
      remain -= (size_t) n;
      stalls = 0;
    } else {
#ifdef _WIN32
      const int wouldblock = (WSAGetLastError() == WSAEWOULDBLOCK);
#else
      const int wouldblock =
          (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR);
#endif
      if (!wouldblock || !check_writeinput_t(st->soc, st->timeout) ||
          ++stalls > 100)
        return 0;
    }
  }
  return 1;
}

static const char *socks5_rep_message(unsigned char rep) {
  switch (rep) {
  case 0x01:
    return "general failure";
  case 0x02:
    return "connection not allowed by ruleset";
  case 0x03:
    return "network unreachable";
  case 0x04:
    return "host unreachable";
  case 0x05:
    return "connection refused";
  case 0x06:
    return "TTL expired";
  case 0x07:
    return "command not supported";
  case 0x08:
    return "address type not supported";
  default:
    return "unknown error";
  }
}

/* The reply's BND.ADDR is variable-length: consume exactly what ATYP says, or
   the leftover bytes prepend to the first origin (or TLS) read. */
static int socks5_read_reply(socks5_stream *st, char *msg, size_t msgsize) {
  unsigned char head[4];
  unsigned char addr[SOCKS5_MAXFIELD + 2];
  size_t addrlen;

  if (!socks5_read_n(st, head, sizeof(head)))
    return socks5_fail(msg, msgsize, "SOCKS5: no reply from proxy");
  if (head[0] != SOCKS5_VERSION)
    return socks5_fail(msg, msgsize, "SOCKS5: bad version in reply");
  if (head[1] != 0x00) {
    snprintf(msg, msgsize, "SOCKS5 connect failed: %s",
             socks5_rep_message(head[1]));
    return 0;
  }
  switch (head[3]) {
  case SOCKS5_ATYP_IPV4:
    addrlen = 4;
    break;
  case SOCKS5_ATYP_IPV6:
    addrlen = 16;
    break;
  case SOCKS5_ATYP_DOMAIN: {
    unsigned char len;

    if (!socks5_read_n(st, &len, 1))
      return socks5_fail(msg, msgsize, "SOCKS5: truncated reply");
    addrlen = len; // <= 255, always fits addr[]
    break;
  }
  default: // unknown length: the stream cannot be resynchronized
    return socks5_fail(msg, msgsize, "SOCKS5: unknown address type in reply");
  }
  if (addrlen != 0 && !socks5_read_n(st, addr, addrlen))
    return socks5_fail(msg, msgsize, "SOCKS5: truncated reply address");
  if (!socks5_read_n(st, addr, 2)) // BND.PORT, unused
    return socks5_fail(msg, msgsize, "SOCKS5: truncated reply port");
  return 1;
}

static int socks5_negotiate(socks5_stream *st, const char *host, size_t hostlen,
                            int port, const char *user, size_t userlen,
                            const char *pass, size_t passlen, int want_auth,
                            char *msg, size_t msgsize) {
  unsigned char frame[3 + SOCKS5_MAXFIELD + SOCKS5_MAXFIELD];
  unsigned char rep[2];

  /* greeting */
  frame[0] = SOCKS5_VERSION;
  frame[1] = (unsigned char) (want_auth ? 2 : 1);
  frame[2] = SOCKS5_METHOD_NONE;
  frame[3] = SOCKS5_METHOD_USERPASS;
  if (!socks5_write_all(st, frame, (size_t) 2 + frame[1]))
    return socks5_fail(msg, msgsize, "SOCKS5: write error");
  if (!socks5_read_n(st, rep, sizeof(rep)))
    return socks5_fail(msg, msgsize, "SOCKS5: no method reply from proxy");
  if (rep[0] != SOCKS5_VERSION)
    return socks5_fail(msg, msgsize, "SOCKS5: bad version in method reply");
  switch (rep[1]) {
  case SOCKS5_METHOD_NONE:
    break;
  case SOCKS5_METHOD_USERPASS:
    if (!want_auth)
      return socks5_fail(msg, msgsize,
                         "SOCKS5: proxy requires authentication, none given");
    /* RFC 1929 sub-negotiation; its version byte is 0x01, not 0x05 */
    frame[0] = SOCKS5_AUTH_VERSION;
    frame[1] = (unsigned char) userlen;
    memcpy(frame + 2, user, userlen);
    frame[2 + userlen] = (unsigned char) passlen;
    memcpy(frame + 3 + userlen, pass, passlen);
    if (!socks5_write_all(st, frame, 3 + userlen + passlen))
      return socks5_fail(msg, msgsize, "SOCKS5: write error");
    if (!socks5_read_n(st, rep, sizeof(rep)))
      return socks5_fail(msg, msgsize, "SOCKS5: no authentication reply");
    if (rep[1] != 0x00)
      return socks5_fail(msg, msgsize, "SOCKS5: authentication failed");
    break;
  case SOCKS5_METHOD_NOACCEPTABLE:
    return socks5_fail(
        msg, msgsize,
        "SOCKS5: proxy accepts no authentication method we offer");
  default:
    return socks5_fail(msg, msgsize,
                       "SOCKS5: proxy selected an unknown method");
  }

  /* CONNECT to the origin, by name */
  frame[0] = SOCKS5_VERSION;
  frame[1] = SOCKS5_CMD_CONNECT;
  frame[2] = 0x00; // RSV
  frame[3] = SOCKS5_ATYP_DOMAIN;
  frame[4] = (unsigned char) hostlen;
  memcpy(frame + 5, host, hostlen);
  frame[5 + hostlen] = (unsigned char) (port >> 8);
  frame[6 + hostlen] = (unsigned char) (port & 0xFF);
  if (!socks5_write_all(st, frame, 7 + hostlen))
    return socks5_fail(msg, msgsize, "SOCKS5: write error");

  return socks5_read_reply(st, msg, msgsize);
}

/* Decode the proxy userinfo into the two RFC 1929 fields. Split on the first
   colon of the still-escaped string, so a %3A stays inside the username. */
static int socks5_credentials(httrackp *opt, const char *proxy_name, char *user,
                              size_t user_size, size_t *userlen, char *pass,
                              size_t pass_size, size_t *passlen, char *msg,
                              size_t msgsize) {
  const char *const a = jump_identification_const(proxy_name); // past the '@'
  const char *const astart = jump_protocol_const(proxy_name);
  char userinfo[1024];
  char *colon;

  if (a <= astart || (size_t) (a - astart) - 1 >= sizeof(userinfo))
    return socks5_fail(msg, msgsize, "SOCKS5: credentials too long");
  userinfo[0] = '\0';
  strncatbuff(userinfo, astart, (int) (a - astart) - 1);
  colon = strchr(userinfo, ':');
  if (colon != NULL)
    *colon++ = '\0';
  strlcpybuff(
      user, unescape_http(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), userinfo),
      user_size);
  strlcpybuff(pass,
              colon != NULL ? unescape_http(OPT_GET_BUFF(opt),
                                            OPT_GET_BUFF_SIZE(opt), colon)
                            : "",
              pass_size);
  *userlen = strlen(user);
  *passlen = strlen(pass);
  // reject, never truncate: a clipped secret would authenticate as another one
  if (*userlen > SOCKS5_MAXFIELD || *passlen > SOCKS5_MAXFIELD)
    return socks5_fail(msg, msgsize, "SOCKS5: credentials too long");
  if (*userlen == 0)
    return socks5_fail(msg, msgsize, "SOCKS5: empty proxy username");
  return 1;
}

static int socks5_handshake_stream(httrackp *opt, socks5_stream *st,
                                   const char *adr, const char *proxy_name,
                                   int ssl, char *msg, size_t msgsize) {
  const char *const host = jump_identification_const(adr);
  const char *const portsep = jump_toport_const(adr);
  const size_t hostlen =
      portsep != NULL ? (size_t) (portsep - host) : strlen(host);
  // sized for the whole userinfo: decoding shrinks, so an over-long credential
  // lands intact and is rejected below rather than silently clipped
  char user[1024];
  char pass[1024];
  size_t userlen = 0, passlen = 0;
  int want_auth = 0;
  int port = ssl ? 443 : 80;
  size_t i;

  if (hostlen == 0 || hostlen > SOCKS5_MAXFIELD)
    return socks5_fail(msg, msgsize, "SOCKS5: invalid origin host");
  if (host[0] == '[') // ATYP=domain cannot carry an IPv6 literal
    return socks5_fail(msg, msgsize,
                       "SOCKS5: IPv6 literal origin is not supported");
  for (i = 0; i < hostlen; i++) {
    if ((unsigned char) host[i] < ' ')
      return socks5_fail(msg, msgsize, "SOCKS5: invalid origin host");
  }
  if (portsep != NULL) {
    int p = -1;

    sscanf(portsep + 1, "%d", &p);
    if (p <= 0 || p > 65535)
      return socks5_fail(msg, msgsize, "SOCKS5: invalid origin port");
    port = p;
  }
  if (link_has_authorization(proxy_name)) {
    if (!socks5_credentials(opt, proxy_name, user, sizeof(user), &userlen, pass,
                            sizeof(pass), &passlen, msg, msgsize))
      return 0;
    want_auth = 1;
  }
  return socks5_negotiate(st, host, hostlen, port, user, userlen, pass, passlen,
                          want_auth, msg, msgsize);
}

int socks5_handshake(httrackp *opt, htsblk *retour, const char *adr,
                     int timeout) {
  socks5_stream st;
#if HTS_USEOPENSSL
  const int ssl = retour->ssl;
#else
  const int ssl = 0;
#endif

  if (retour->soc == INVALID_SOCKET)
    return 0;
  st.soc = retour->soc;
  st.timeout = timeout;
  st.io = NULL;
  return socks5_handshake_stream(opt, &st, adr, retour->req.proxy.name, ssl,
                                 retour->msg, sizeof(retour->msg));
}

int socks5_handshake_scripted(httrackp *opt, const char *adr,
                              const char *proxy_name, socks5_test_io *io) {
  socks5_stream st;

  st.soc = INVALID_SOCKET;
  st.timeout = 0;
  st.io = io;
  io->consumed = 0;
  io->sent_len = 0;
  io->msg[0] = '\0';
  return socks5_handshake_stream(opt, &st, adr, proxy_name, 0, io->msg,
                                 sizeof(io->msg));
}
