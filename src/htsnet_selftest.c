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
/* File: htsnet_selftest.c subroutines:                         */
/*       self-tests for sockets, transport and URL capture      */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htsselftest_int.h"

/* The two counters a front end polls, and the option beside them: published
   into the stats surface, and never carried onto another opt. Runs everywhere,
   because none of it needs a kernel that has Multipath TCP. */
static int st_mptcpstats(httrackp *opt, int argc, char **argv) {
  httrackp *from = hts_create_opt();
  httrackp *to = hts_create_opt();
  const hts_stat_struct *stats;
  int err = 0;

  (void) opt;
  (void) argc;
  (void) argv;

  if (from->mptcp != HTS_DEFAULT || from->mptcp_connections != 0 ||
      from->mptcp_fallbacks != 0) {
    fprintf(stderr, "mptcp-stats: a fresh opt is not unset and at zero\n");
    err = 1;
  }

  /* Published beside stat_transport_failures, which is where a front end
     reads it. */
  to->mptcp_connections = 7;
  to->mptcp_fallbacks = 3;
  stats = hts_get_stats(to);
  if (stats == NULL || stats->stat_mptcp_connections != 7 ||
      stats->stat_mptcp_fallbacks != 3) {
    fprintf(stderr, "mptcp-stats: the counters did not reach the stats\n");
    err = 1;
  }

  /* The option travels between opts. The counts are one mirror's own. */
  from->mptcp = HTS_TRUE;
  from->mptcp_connections = 5;
  from->mptcp_fallbacks = 5;
  to->mptcp = HTS_DEFAULT;
  to->mptcp_connections = 0;
  to->mptcp_fallbacks = 0;
  copy_htsopt(from, to);
  if (to->mptcp != HTS_TRUE) {
    fprintf(stderr, "mptcp-stats: the option did not travel\n");
    err = 1;
  }
  if (to->mptcp_connections != 0 || to->mptcp_fallbacks != 0) {
    fprintf(stderr, "mptcp-stats: a count travelled onto another opt\n");
    err = 1;
  }

  /* Off travels too: it is a decision, not the absence of one. */
  from->mptcp = HTS_FALSE;
  to->mptcp = HTS_TRUE;
  copy_htsopt(from, to);
  if (to->mptcp != HTS_FALSE) {
    fprintf(stderr, "mptcp-stats: the option off did not travel\n");
    err = 1;
  }

  /* HTS_DEFAULT is "unspecified", so it must leave a set target alone. */
  from->mptcp = HTS_DEFAULT;
  to->mptcp = HTS_FALSE;
  copy_htsopt(from, to);
  if (to->mptcp != HTS_FALSE) {
    fprintf(stderr, "mptcp-stats: an unset option overwrote a set one\n");
    err = 1;
  }

  hts_free_opt(from);
  hts_free_opt(to);
  printf("mptcp-stats: %s\n", err ? "FAIL" : "OK");
  return err;
}

/* Print the host/port the FTP path splits out of a URL address. */
static int st_ftpaddr(httrackp *opt, int argc, char **argv) {
  int i;

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "ftpaddr: needs a URL address\n");
    return 1;
  }
  for (i = 0; i < argc; i++) {
    char BIGSTK url[HTS_URLMAXSIZE * 2];
    char host[256];
    char err[128];
    int port = 21;

    strcpybuff(url, argv[i]);
    if (ftp_split_hostport(jump_identification(ftp_jump_authority(url)), host,
                           sizeof(host), &port, err, sizeof(err)))
      printf("host=%s port=%d\n", host, port);
    else
      printf("error=%s\n", err);
  }
  return 0;
}

static int st_proxyurl(httrackp *opt, int argc, char **argv) {
  char BIGSTK name[HTS_URLMAXSIZE * 2];
  int port = -1;

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "proxyurl: needs a proxy argument\n");
    return 1;
  }
  hts_parse_proxy(argv[0], name, sizeof(name), &port);
  // host= is the resolved host (scheme/userinfo stripped); kind= the transport
  printf("name=%s port=%d host=%s kind=%s\n", name, port,
         jump_identification_const(name),
         hts_proxy_is_socks(name)     ? "socks"
         : hts_proxy_is_connect(name) ? "connect"
                                      : "http");
  return 0;
}

/* Scripted SOCKS5 server: no-auth method reply, then a success CONNECT reply
   carrying the given ATYP address, then a sentinel origin byte the handshake
   must leave unread (an off-by-one in the reply framing eats it). */
static size_t socks5_reply(unsigned char *buf, unsigned char atyp,
                           const unsigned char *addr, size_t addrlen) {
  size_t n = 0;

  buf[n++] = 0x05;
  buf[n++] = 0x00; /* method: no authentication */
  buf[n++] = 0x05;
  buf[n++] = 0x00; /* REP: succeeded */
  buf[n++] = 0x00;
  buf[n++] = atyp;
  memcpy(buf + n, addr, addrlen);
  n += addrlen;
  buf[n++] = 0x1f;
  buf[n++] = 0x90; /* BND.PORT */
  buf[n++] = 0xAA; /* sentinel */
  return n;
}

#define SOCKS5_SENTINEL_LEFT(io, len) ((io).consumed == (len) - 1)

static int st_socks5(httrackp *opt, int argc, char **argv) {
  static const unsigned char v4[4] = {127, 0, 0, 1};
  static const unsigned char v6[16] = {0};
  static const unsigned char domain[6] = {5, 'p', 'r', 'o', 'x', 'y'};
  const char *const proxy = "socks5://127.0.0.1";
  unsigned char script[64];
  socks5_test_io io;
  size_t len;

  (void) argc;
  (void) argv;

  /* each reply address type is drained exactly, sentinel untouched */
  len = socks5_reply(script, 0x01, v4, sizeof(v4));
  io.reply = script;
  io.reply_len = len;
  assertf(socks5_handshake_scripted(opt, "origin.test", proxy, &io) == 1);
  assertf(SOCKS5_SENTINEL_LEFT(io, len));
  /* the greeting offers no-auth only, and the origin goes out by name, port 80
   */
  assertf(io.sent_len == 3 + 7 + 11);
  assertf(memcmp(io.sent, "\x05\x01\x00", 3) == 0);
  assertf(memcmp(io.sent + 3, "\x05\x01\x00\x03\x0borigin.test\x00\x50", 18) ==
          0);

  len = socks5_reply(script, 0x04, v6, sizeof(v6));
  io.reply = script;
  io.reply_len = len;
  assertf(socks5_handshake_scripted(opt, "origin.test", proxy, &io) == 1);
  assertf(SOCKS5_SENTINEL_LEFT(io, len));

  len = socks5_reply(script, 0x03, domain, sizeof(domain));
  io.reply = script;
  io.reply_len = len;
  assertf(socks5_handshake_scripted(opt, "origin.test", proxy, &io) == 1);
  assertf(SOCKS5_SENTINEL_LEFT(io, len));

  /* an unknown address type has no known length: fail, never guess */
  len = socks5_reply(script, 0x02, v4, sizeof(v4));
  io.reply = script;
  io.reply_len = len;
  assertf(socks5_handshake_scripted(opt, "origin.test", proxy, &io) == 0);

  /* truncated frames (here: no BND.PORT) fail instead of over-reading */
  len = socks5_reply(script, 0x01, v4, sizeof(v4)) - 3;
  io.reply = script;
  io.reply_len = len;
  assertf(socks5_handshake_scripted(opt, "origin.test", proxy, &io) == 0);

  /* explicit origin port is encoded big-endian (8443 = 0x20fb) */
  len = socks5_reply(script, 0x01, v4, sizeof(v4));
  io.reply = script;
  io.reply_len = len;
  assertf(socks5_handshake_scripted(opt, "origin.test:8443", proxy, &io) == 1);
  assertf(memcmp(io.sent + io.sent_len - 2, "\x20\xfb", 2) == 0);

  /* a bad origin port is refused before any byte goes out (#614). 4294967376 is
     the case the old range check could not see: it overflowed the sscanf("%d")
     into a plausible 80 and passed. 65616 would not prove anything here, since
     it fits an int and the old check already caught it. */
  {
    static const char *const bad[] = {"origin.test:4294967376",
                                      "origin.test:80x", "origin.test:+80",
                                      "origin.test: 80", "origin.test:8.0"};
    size_t k;

    for (k = 0; k < sizeof(bad) / sizeof(bad[0]); k++) {
      len = socks5_reply(script, 0x01, v4, sizeof(v4));
      io.reply = script;
      io.reply_len = len;
      assertf(socks5_handshake_scripted(opt, bad[k], proxy, &io) == 0);
      assertf(io.sent_len == 0);
    }
  }

  /* a control byte in the host would be a field of its own in the ATYP=domain
     request; the port that follows it must not hide it (#1010) */
  {
    static const char *const hostile[] = {"ori\rgin.test", "ori\rgin.test:80"};
    size_t k;

    for (k = 0; k < sizeof(hostile) / sizeof(hostile[0]); k++) {
      len = socks5_reply(script, 0x01, v4, sizeof(v4));
      io.reply = script;
      io.reply_len = len;
      assertf(socks5_handshake_scripted(opt, hostile[k], proxy, &io) == 0);
      assertf(io.sent_len == 0);
    }
  }

  /* credentials: split on the first colon of the escaped userinfo, so %3a stays
     inside the username and a colon in the password is not a delimiter */
  {
    static const unsigned char auth_script[] = {
        0x05, 0x02,             /* method: user/pass */
        0x01, 0x00,             /* auth: success */
        0x05, 0x00, 0x00, 0x01, /* REP: succeeded, ATYP ipv4 */
        127,  0,    0,    1,    0x1f, 0x90, 0xAA};

    io.reply = auth_script;
    io.reply_len = sizeof(auth_script);
    assertf(socks5_handshake_scripted(opt, "origin.test",
                                      "socks5://us%3aer:p:ass@127.0.0.1",
                                      &io) == 1);
    assertf(SOCKS5_SENTINEL_LEFT(io, sizeof(auth_script)));
    assertf(memcmp(io.sent, "\x05\x02\x00\x02", 4) == 0);
    assertf(memcmp(io.sent + 4, "\x01\x05us:er\x05p:ass", 13) == 0);
  }

  /* a proxy demanding auth we cannot provide, and one refusing every method */
  io.reply = (const unsigned char *) "\x05\x02";
  io.reply_len = 2;
  assertf(socks5_handshake_scripted(opt, "origin.test", proxy, &io) == 0);
  io.reply = (const unsigned char *) "\x05\xff";
  io.reply_len = 2;
  assertf(socks5_handshake_scripted(opt, "origin.test", proxy, &io) == 0);

  /* a refused CONNECT is an error, not a tunnel */
  io.reply = (const unsigned char
                  *) "\x05\x00\x05\x05\x00\x01\x7f\x00\x00\x01\x1f\x90";
  io.reply_len = 12;
  assertf(socks5_handshake_scripted(opt, "origin.test", proxy, &io) == 0);

  /* over-long host or credentials are rejected before anything is sent */
  {
    char host[512];
    char name[1024];
    size_t i;

    host[0] = '\0';
    for (i = 0; i < 256; i++)
      strcatbuff(host, "a");
    io.reply = script;
    io.reply_len = len;
    assertf(socks5_handshake_scripted(opt, host, proxy, &io) == 0);
    assertf(io.sent_len == 0);

    strcpybuff(name, "socks5://user:");
    for (i = 0; i < 256; i++)
      strcatbuff(name, "p");
    strcatbuff(name, "@127.0.0.1");
    io.reply = script;
    io.reply_len = len;
    assertf(socks5_handshake_scripted(opt, "origin.test", name, &io) == 0);
    assertf(io.sent_len == 0);
  }

  /* the request is always ATYP=domain, which cannot carry an IPv6 literal: a
     bracketed origin is rejected rather than sent as a bogus domain name. The
     msg check pins the reason: a stricter host validator would also reject
     these, but for the wrong cause. */
  io.reply = script;
  io.reply_len = len;
  assertf(socks5_handshake_scripted(opt, "[::1]", proxy, &io) == 0);
  assertf(io.sent_len == 0);
  assertf(strstr(io.msg, "IPv6") != NULL);
  io.reply = script;
  io.reply_len = len;
  assertf(socks5_handshake_scripted(opt, "[2001:db8::1]:8443", proxy, &io) ==
          0);
  assertf(io.sent_len == 0);
  assertf(strstr(io.msg, "IPv6") != NULL);

  printf("socks5 self-test OK\n");
  return 0;
}

/* Is the guard still the poison the case wrote? Poison, not zero: a stray
   terminator would read as untouched. */
static hts_boolean st_addrport_intact(const char *p, size_t n) {
  size_t i;

  for (i = 0; i < n; i++) {
    if (p[i] != 'Z')
      return HTS_FALSE;
  }
  return HTS_TRUE;
}

/* The longest text its family has: 255.255.255.255, or the IPv6 address
   inet_ntop cannot compress. */
static void st_addrport_longest(SOCaddr *addr, int family, unsigned int port) {
  (void) family;
  memset(addr, 0, sizeof(*addr));
#if HTS_INET6 != 0
  if (family == AF_INET6) {
    addr->m_addr.in6.sin6_family = AF_INET6;
    memset(&addr->m_addr.in6.sin6_addr, 0xff,
           sizeof(addr->m_addr.in6.sin6_addr));
  } else
#endif
  {
    addr->m_addr.in.sin_family = AF_INET;
    memset(&addr->m_addr.in.sin_addr, 0xff, sizeof(addr->m_addr.in.sin_addr));
  }
  SOCaddr_initport(*addr, port);
}

/* The formatter proxytrack's getip() relies on (#1493). */
static void st_addrport_case(int family, unsigned int port,
                             const char *expected) {
  const size_t guard = 16;
  const size_t reserved = sizeof(":65535") - 1;
  const size_t full = SOCADDR_INETNTOA_PORT_SIZE;
  char *arena = malloct(full + guard);
  char wantport[8];
  SOCaddr addr;
  size_t size;

  assertf(arena != NULL);
  st_addrport_longest(&addr, family, port);
  snprintf(wantport, sizeof(wantport), ":%u", port);

  /* the declared capacity holds the longest host AND the longest port */
  memset(arena, 'Z', full + guard);
  assertf(SOCaddr_inetntoa_port(arena, full, addr));
  assertf(strcmp(arena, expected) == 0);
  assertf(st_addrport_intact(arena + full, guard));

  /* no capacity is written past, and none silently drops the port, which is
     what a host let loose on the whole buffer does */
  for (size = 1; size <= full; size++) {
    const hts_boolean ok = SOCaddr_inetntoa_port(arena, size, addr);

    memset(arena, 'Z', full + guard);
    (void) SOCaddr_inetntoa_port(arena, size, addr);
    /* before strlen, or a run with no terminator reads off the allocation */
    assertf(memchr(arena, '\0', size) != NULL);
    assertf(strlen(arena) < size);
    assertf(st_addrport_intact(arena + size, full + guard - size));
    if (size <= reserved) {
      /* too small for the port at all: refuse, rather than clip it to "6553" */
      assertf(!ok);
      assertf(arena[0] == '\0');
    } else {
      assertf(strstr(arena, wantport) != NULL);
    }
  }
  freet(arena);
}

static int st_addrport(httrackp *opt, int argc, char **argv) {
  (void) opt;
  (void) argc;
  (void) argv;
  st_addrport_case(AF_INET, 65535, "255.255.255.255:65535");
  /* 65535 is byte-swap invariant, so a dropped ntohs() needs this one */
  st_addrport_case(AF_INET, 8080, "255.255.255.255:8080");
#if HTS_INET6 != 0
  st_addrport_case(AF_INET6, 65535,
                   "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:65535");
  st_addrport_case(AF_INET6, 8080,
                   "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:8080");
#endif
  printf("addrport self-test OK\n");
  return 0;
}

/* Connected stream pair over loopback; Windows has no socketpair(). */
static int st_socketpair(T_SOC sv[2]) {
  struct sockaddr_in sa;
  socklen_t len = sizeof(sa);
  T_SOC srv, cli, acc;

  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if ((srv = (T_SOC) socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
    return -1;
  if (bind(srv, (struct sockaddr *) &sa, sizeof(sa)) != 0 ||
      listen(srv, 1) != 0 ||
      getsockname(srv, (struct sockaddr *) &sa, &len) != 0) {
    deletesoc(srv);
    return -1;
  }
  if ((cli = (T_SOC) socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
    deletesoc(srv);
    return -1;
  }
  if (connect(cli, (struct sockaddr *) &sa, sizeof(sa)) != 0 ||
      (acc = (T_SOC) accept(srv, NULL, NULL)) == INVALID_SOCKET) {
    deletesoc(cli);
    deletesoc(srv);
    return -1;
  }
  deletesoc(srv);
  sv[0] = acc;
  sv[1] = cli;
  return 0;
}

#ifndef _WIN32
/* Drop the peer so a write gets EPIPE, because SO_LINGER with a zero timeout
   asks for a RST rather than a FIN. GNU/Hurd's pfinet sends a FIN anyway. */
static void st_kill_peer(T_SOC soc) {
  struct linger lg;

  lg.l_onoff = 1;
  lg.l_linger = 0;
  /* Checked, so a host that refuses the option says so here rather than
     through the writes further down. */
  assertf(setsockopt(soc, SOL_SOCKET, SO_LINGER, (const char *) &lg,
                     sizeof(lg)) == 0);
  deletesoc(soc);
}

/* A library must not kill the process that embedded it (#1689). httrack's own
   main() swallows SIGPIPE, so this puts the default action back first: without
   that, every assertion below passes over a broken library. */
static int st_sigpipe(httrackp *opt, int argc, char **argv) {
  void (*inherited)(int);
  T_SOC sv[2];
  htsblk r;
  char discard[1];
  hts_boolean fin_only;
  int i, n;

  (void) opt;
  (void) argc;
  (void) argv;
  inherited = signal(SIGPIPE, SIG_DFL);
  assertf(inherited != SIG_ERR);

  /* The write path: sendc() to a peer that is gone must report the failure
     rather than end the process. */
  assertf(st_socketpair(sv) == 0);
  st_kill_peer(sv[0]);
  memset(&r, 0, sizeof(r));
  r.soc = sv[1];
  /* What newhttp_addr does to a real one. On macOS this is the whole cover,
     because HTS_MSG_NOSIGNAL is 0 there. */
  socket_set_nosigpipe(sv[1]);
  /* Wait for the peer to be gone rather than assume a write count outlasts it,
     because Darwin hands loopback input to another thread. Nothing was ever
     sent on this socket, so the read takes the reset, or the end of file. */
  assertf(check_readinput_t(sv[1], 10) == 1);
  n = (int) recv(sv[1], discard, sizeof(discard), 0);
  assertf(n <= 0);
  fin_only = n == 0 ? HTS_TRUE : HTS_FALSE;
  /* A FIN leaves this half of the connection open, so the write still succeeds
     and only the peer's answer breaks the pipe. GNU/Hurd's pfinet never
     answers, so end this half ourselves and take the EPIPE locally (#1717). */
  if (fin_only)
    assertf(shutdown(sv[1], SHUT_WR) == 0);
  /* Twice, because a platform that re-reports ECONNRESET on the first still
     owes EPIPE on the second. */
  for (i = 0; i < 2; i++) {
    errno = 0;
    assertf(sendc(&r, "GET / HTTP/1.0\r\n\r\n") < 0);
#ifndef _WIN32
    /* The write reports why it failed, and htsproxy.c reads exactly this to
       tell a dead socket from one that would block. Anything that runs between
       the send and the caller has to put errno back. */
    assertf(errno == EPIPE || errno == ECONNRESET || errno == ENOTCONN);
#endif
  }
  deletesoc(sv[1]);

#ifdef HAVE_SIGTIMEDWAIT
  /* Darwin has no sigtimedwait, so the mask is a no-op there and the socket
     option is what covers the writes. */
  {
    sigset_t pipeset, pending, saved, now;
    sigpipe_mask m;
    int caught;

    assertf(sigemptyset(&pipeset) == 0 && sigaddset(&pipeset, SIGPIPE) == 0);
    /* A second signal the host blocked, so the restore below is caught
       putting the whole mask back rather than just unblocking SIGPIPE. */
    assertf(sigemptyset(&now) == 0 && sigaddset(&now, SIGUSR2) == 0);
    assertf(HTS_SIGMASK(SIG_BLOCK, &now, &saved) == 0);

    /* It holds SIGPIPE off this thread and puts the mask back. */
    sigpipe_hold(&m);
    assertf(m.held == 1 && m.was_pending == 0);
    assertf(raise(SIGPIPE) == 0);
    assertf(sigpending(&pending) == 0 && sigismember(&pending, SIGPIPE) == 1);
    sigpipe_release(&m);
    assertf(sigpending(&pending) == 0 && sigismember(&pending, SIGPIPE) == 0);
    assertf(HTS_SIGMASK(SIG_SETMASK, NULL, &now) == 0);
    assertf(sigismember(&now, SIGPIPE) == 0 && sigismember(&now, SIGUSR2) == 1);

    /* One the host queued is the host's, so the mask leaves it alone. */
    assertf(HTS_SIGMASK(SIG_BLOCK, &pipeset, NULL) == 0);
    assertf(raise(SIGPIPE) == 0);
    sigpipe_hold(&m);
    assertf(m.was_pending == 1);
    sigpipe_release(&m);
    assertf(sigpending(&pending) == 0 && sigismember(&pending, SIGPIPE) == 1);
    assertf(sigwait(&pipeset, &caught) == 0 && caught == SIGPIPE);
    assertf(HTS_SIGMASK(SIG_SETMASK, &saved, NULL) == 0);
  }
#endif

  /* Read it back, or a sibling self-test inherits whatever we left. */
  assertf(signal(SIGPIPE, inherited) != SIG_ERR);
  assertf(signal(SIGPIPE, SIG_DFL) == inherited);
  assertf(signal(SIGPIPE, inherited) == SIG_DFL);
  printf("sigpipe self-test OK\n");
  return 0;
}
#endif

/* get_ftp_line must bound a hostile, CRLF-less reply into its internal
   1024-byte buffer; ASan turns the pre-fix overflow into an abort here. */
static int st_ftpline(httrackp *opt, int argc, char **argv) {
  T_SOC sv[2];
  char line[2048];
  char flood[4096];

  (void) opt;
  (void) argc;
  (void) argv;
  memset(flood, 'x', sizeof(flood));
  assertf(st_socketpair(sv) == 0);
  // the 4102-byte reply fits the loopback send buffer, so no reader is needed
  assertf(send(sv[1], "220 ", 4, 0) == 4); // valid 3-digit code
  assertf(send(sv[1], flood, (int) sizeof(flood), 0) == (int) sizeof(flood));
  assertf(send(sv[1], "\r\n", 2, 0) == 2); // end the line so we return
  deletesoc(sv[1]);
  line[0] = '\0';
  get_ftp_line(NULL, sv[0], line, sizeof(line), 5, NULL);
  deletesoc(sv[0]);
  printf("ftp-line self-test OK (bounded %d-byte reply)\n",
         (int) sizeof(flood));
  return 0;
}

/* ftp_split_userpass: the split itself, and userinfo refused rather than
   clipped into another account's name (#1032). */
static int st_ftpuser(httrackp *opt, int argc, char **argv) {
  static const size_t caps[] = {16, 256}; /* asymmetric: a shared bound shows */
  char ubuf[256 + 32], pbuf[sizeof(ubuf)], poison[sizeof(ubuf)];
  char in[2 * 256 + 8];
  size_t c, over;

  (void) opt;
  (void) argc;
  (void) argv;
  memset(poison, '#', sizeof(poison));
  {
    const char ok[] = "bob:secret@host/f"; // '@' at index 10

    assertf(ftp_split_userpass(ok, ok + 11, ubuf, sizeof(ubuf), pbuf,
                               sizeof(pbuf)) == HTS_TRUE);
    assertf(strcmp(ubuf, "bob") == 0);
    assertf(strcmp(pbuf, "secret") == 0);
  }
  {
    const char ok[] = "bob@host/f"; // no password: the '@' still ends the user

    assertf(ftp_split_userpass(ok, ok + 4, ubuf, sizeof(ubuf), pbuf,
                               sizeof(pbuf)) == HTS_TRUE);
    assertf(strcmp(ubuf, "bob") == 0);
    assertf(pbuf[0] == '\0');
  }
  {
    const char ok[] = "u@relay:pw@gw/f"; // only the last '@' ends the userinfo

    assertf(ftp_split_userpass(ok, ok + 11, ubuf, sizeof(ubuf), pbuf,
                               sizeof(pbuf)) == HTS_TRUE);
    assertf(strcmp(ubuf, "u@relay") == 0);
    assertf(strcmp(pbuf, "pw") == 0);
  }
  for (c = 0; c < sizeof(caps) / sizeof(caps[0]); c++) {
    const size_t ucap = caps[c], pcap = caps[1 - c];

    /* overshoot the user, the pass, then a bare name bounded only by '@' */
    for (over = 0; over <= 2; over++) {
      const size_t cap = over == 1 ? pcap : ucap;
      size_t len;

      for (len = cap - 2; len <= cap + 1; len++) {
        const size_t user_len = over == 1 ? 1 : len;
        const size_t pass_len = over == 0 ? 1 : (over == 1 ? len : 0);
        const size_t total = user_len + pass_len + (over == 2 ? 1 : 2);
        const hts_boolean fits = len < cap ? HTS_TRUE : HTS_FALSE;

        memset(in, 'u', user_len);
        if (over != 2) {
          in[user_len] = ':';
          memset(in + user_len + 1, 'p', pass_len);
        }
        in[total - 1] = '@';
        in[total] = '\0';
        memcpy(ubuf, poison, sizeof(ubuf)); /* a zero canary would hide a NUL */
        memcpy(pbuf, poison, sizeof(pbuf));
        assertf(ftp_split_userpass(in, in + total, ubuf, ucap, pbuf, pcap) ==
                fits);
        if (fits) {
          assertf(strlen(ubuf) == user_len && strlen(pbuf) == pass_len);
          assertf(ubuf[user_len - 1] == 'u');
          assertf(pass_len == 0 || pbuf[pass_len - 1] == 'p');
        } else {
          assertf(ubuf[0] == '\0' && pbuf[0] == '\0'); /* fail safe */
        }
        /* the whole tail: one canary byte misses a write just past it */
        assertf(memcmp(ubuf + ucap, poison, sizeof(ubuf) - ucap) == 0);
        assertf(memcmp(pbuf + pcap, poison, sizeof(pbuf) - pcap) == 0);
      }
    }
  }
  printf("ftp-userpass self-test OK\n");
  return 0;
}

/* Both quoting forms at two capacities: the quoted form is two bytes wider
   (#1019). */
static int st_ftpcmdlen(httrackp *opt, int argc, char **argv) {
  static const size_t caps[] = {32, FTP_LINE_SIZE};
  char BIGSTK buf[FTP_LINE_SIZE + 32];
  char BIGSTK poison[FTP_LINE_SIZE + 32];
  char BIGSTK path[FTP_LINE_SIZE + 2];
  char BIGSTK wire[FTP_LINE_SIZE * 2];
  size_t c, got = 0;
  T_SOC sv[2];

  (void) opt;
  (void) argc;
  (void) argv;
  memset(poison, '#', sizeof(poison));
  for (c = 0; c < sizeof(caps) / sizeof(caps[0]); c++) {
    const size_t cap = caps[c];
    int quoted;

    for (quoted = 0; quoted <= 1; quoted++) {
      const size_t verb = 5 + 2 * (size_t) quoted; /* "RETR " plus quotes */
      const size_t fit = cap - 1 - verb; /* longest path still fitting */
      size_t len;

      for (len = fit - 1; len <= fit + 1; len++) {
        memset(path, 'p', len);
        path[len] = '\0';
        if (quoted)
          path[0] = ' '; /* any of these forces the quoted form */
        memcpy(buf, poison, sizeof(buf)); /* a zero canary would hide a NUL */
        if (len > fit) {
          assertf(ftp_command(buf, cap, "RETR", path) == HTS_FALSE);
          assertf(buf[0] == '\0'); /* fail-safe for an ignored result */
        } else {
          assertf(ftp_command(buf, cap, "RETR", path) == HTS_TRUE);
          assertf(strlen(buf) == verb + len);
          assertf(strncmp(buf, "RETR ", 5) == 0);
          assertf(buf[verb + len - 1] == (quoted ? '\"' : 'p'));
        }
        /* the whole tail: one canary byte misses a write just past it */
        assertf(memcmp(buf + cap, poison, sizeof(buf) - cap) == 0);
      }
    }
  }

  /* send_line() adds the CRLF and drops, rather than truncates, an over-length
     line. */
  memset(path, 'q', FTP_LINE_SIZE);
  path[FTP_LINE_SIZE] = '\0';
  assertf(st_socketpair(sv) == 0);
  assertf(send_line(sv[0], path) == 0); /* one byte too long: never sent */
  path[FTP_LINE_SIZE - 1] = '\0';
  assertf(send_line(sv[0], path) != 0);
  deletesoc(sv[0]);
  for (;;) {
    const int n = (int) recv(sv[1], wire + got, (int) (sizeof(wire) - got), 0);

    if (n <= 0)
      break;
    got += (size_t) n;
  }
  deletesoc(sv[1]);
  assertf(got == FTP_LINE_SIZE + 1); /* the maximal command alone */
  assertf(memcmp(wire, path, FTP_LINE_SIZE - 1) == 0);
  assertf(memcmp(wire + FTP_LINE_SIZE - 1, "\r\n", 2) == 0);
  printf("ftp-cmdlen self-test OK (%d bytes sent)\n", (int) got);
  return 0;
}

/* send_line() must drop a command line carrying a control byte (#1010). */
static int st_ftpctrl(httrackp *opt, int argc, char **argv) {
  /* Verb and URL path as run_launch_ftp() hands them over, then the line the
     wire must carry; NULL for a command that must never leave. */
  static const struct {
    const char *verb;
    const char *path;
    const char *sent;
  } cases[] = {
      {"RETR", "/f.txt%0d%0aDELE%20secret.txt", NULL},
      {"RETR", "/f.txt%0dDELE%20secret.txt", NULL},
      {"RETR", "/f.txt%0aDELE%20secret.txt", NULL},
      {"LIST -A", "/d%0d%0aDELE%20secret.txt/", NULL},
      {"RETR", "/plain.txt", "RETR /plain.txt"},
      {"RETR", "/a%20b.txt", "RETR \"/a b.txt\""},
      {"RETR", "%2Fa%25b.txt", "RETR /a%b.txt"},
      /* High bytes must still go out: a plain-char check reads them negative
         and rejects them. */
      {"RETR", "/caf%e9.txt", "RETR /caf\xe9.txt"},
      /* Bare, these two would hand a server that shells out to ls a flag. */
      {"LIST -A", "/x%20-la/", "LIST -A \"/x -la/\""},
      {"LIST -A", "-la", "LIST -A \"-la\""},
  };

  char BIGSTK catbuff[CATBUFF_SIZE];
  char cmd[512];
  char expect[512];
  char wire[512];
  T_SOC sv[2];
  size_t got = 0, dropped = 1, i;

  (void) opt;
  (void) argc;
  (void) argv;
  expect[0] = '\0';
  assertf(st_socketpair(sv) == 0);
  assertf(send_line(sv[0], "USER bob\001") == 0); // any field, not just a path
  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    ftp_command(cmd, sizeof(cmd), cases[i].verb,
                unescape_http(catbuff, sizeof(catbuff), cases[i].path));
    if (cases[i].sent == NULL) {
      assertf(strstr(cmd, "DELE") != NULL); // the payload did reach the builder
      assertf(send_line(sv[0], cmd) == 0);
      dropped++;
    } else {
      assertf(send_line(sv[0], cmd) != 0);
      strcatbuff(expect, cases[i].sent);
      strcatbuff(expect, "\r\n");
    }
  }
  deletesoc(sv[0]); // EOF, so the read below sees the whole wire
  for (;;) {
    const int n = (int) recv(sv[1], wire + got, (int) (sizeof(wire) - got), 0);

    if (n <= 0)
      break;
    got += (size_t) n;
  }
  deletesoc(sv[1]);
  assertf(got == strlen(expect));
  assertf(memcmp(wire, expect, got) == 0);
  printf("ftp-ctrlchars self-test OK (%d bytes sent, %d rejected)\n", (int) got,
         (int) dropped);
  return 0;
}

/* One header line socinput() keeps whole. */
#define ST_CATCHURL_PAD 990
/* One it cannot: the capture is refused on the line, not on a count. */
#define ST_CATCHURL_LONG (CATCH_URL_LINE_SIZE + 808)
/* More such lines than any budget the request block could hold. */
#define ST_CATCHURL_FLOOD 4000
#define ST_CATCHURL_REQ "GET http://example.com/ HTTP/1.0\r\n"
/* what data[] receives for that request, before any header */
#define ST_CATCHURL_LINE "GET / HTTP/1.0\r\n"

typedef struct st_catchurl_arg {
  int port;
  int pads; /* ST_CATCHURL_PAD-byte header lines */
  int tail; /* length of one last header line */
} st_catchurl_arg;

/* Loopback client connected to what st_catchurl_listen() made. */
static T_SOC st_catchurl_connect(int port) {
  struct sockaddr_in sa;
  T_SOC cli;

  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  sa.sin_port = htons((unsigned short) port);
  cli = (T_SOC) socket(AF_INET, SOCK_STREAM, 0);
  assertf(cli != INVALID_SOCKET);
  assertf(connect(cli, (struct sockaddr *) &sa, sizeof(sa)) == 0);
  return cli;
}

/* Listener on an ephemeral loopback port, reported in "port". */
static T_SOC st_catchurl_listen(int *port) {
  struct sockaddr_in sa;
  SOClen len = sizeof(sa);
  T_SOC srv;

  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  srv = (T_SOC) socket(AF_INET, SOCK_STREAM, 0);
  assertf(srv != INVALID_SOCKET);
  assertf(bind(srv, (struct sockaddr *) &sa, sizeof(sa)) == 0);
  assertf(listen(srv, 4) == 0);
  assertf(getsockname(srv, (struct sockaddr *) &sa, &len) == 0);
  *port = ntohs(sa.sin_port);
  return srv;
}

/* The browser side of catch_url(): "pads" padding lines, then one of "tail". */
static void st_catchurl_client(void *varg) {
  const st_catchurl_arg *const arg = (const st_catchurl_arg *) varg;
  char hdr[ST_CATCHURL_PAD + 2];
  T_SOC cli = st_catchurl_connect(arg->port);
  int i;

  memset(hdr, 'a', sizeof(hdr));
  memcpy(hdr, "X-Pad: ", 7);
  hdr[ST_CATCHURL_PAD] = '\r';
  hdr[ST_CATCHURL_PAD + 1] = '\n';
  assertf(send(cli, ST_CATCHURL_REQ, (int) strlen(ST_CATCHURL_REQ), 0) > 0);
  for (i = 0; i < arg->pads; i++)
    assertf(send(cli, hdr, (int) sizeof(hdr), 0) == (int) sizeof(hdr));
  assertf(arg->tail >= 7 && arg->tail <= ST_CATCHURL_PAD);
  hdr[arg->tail] = '\r';
  hdr[arg->tail + 1] = '\n';
  assertf(send(cli, hdr, arg->tail + 2, 0) == arg->tail + 2);
  assertf(send(cli, "\r\n", 2, 0) == 2);
  (void) recv(cli, hdr, (int) sizeof(hdr), 0);
  deletesoc(cli);
}

/* The browser feeds catch_url() a header block of whatever size it likes. The
   assertions sit on the last block data[] holds and the first it does not,
   since a bound one byte out is the bug. */
static int st_catchurl(httrackp *opt, int argc, char **argv) {
  /* the request line, then each header with its CRLF, then the empty line */
  const size_t line = strlen(ST_CATCHURL_LINE);
  const int pads = (int) ((CATCH_URL_DATA_SIZE - line) / (ST_CATCHURL_PAD + 2));
  const size_t used = line + (size_t) pads * (ST_CATCHURL_PAD + 2);
  /* what is left for a last header, its CRLF, and the empty line's CRLF */
  const int fits = (int) (CATCH_URL_DATA_SIZE - 1 - used) - 4;
  char BIGSTK url[HTS_URLMAXSIZE * 2];
  char method[32];
  char *data = malloct(CATCH_URL_DATA_SIZE);
  st_catchurl_arg arg;
  T_SOC srv;

  (void) opt;
  (void) argc;
  (void) argv;
  assertf(data != NULL);
  srv = st_catchurl_listen(&arg.port);
  arg.pads = pads;

  /* the largest block data[] holds: it comes back filled to the last byte */
  arg.tail = fits;
  url[0] = method[0] = data[0] = '\0';
  assertf(hts_newthread(st_catchurl_client, &arg) == 0);
  assertf(catch_url_capture(srv, url, method, data) == CATCH_URL_OK);
  htsthread_wait();
  assertf(strcmp(method, "GET") == 0);
  assertf(strcmp(url, "http://example.com/") == 0);
  assertf(strncmp(data, ST_CATCHURL_LINE "X-Pad: aaa", line + 10) == 0);
  assertf(strlen(data) == CATCH_URL_DATA_SIZE - 1);

  /* one byte more: refused on the block, and no prefix handed back */
  arg.tail = fits + 1;
  url[0] = method[0] = data[0] = '\0';
  assertf(hts_newthread(st_catchurl_client, &arg) == 0);
  assertf(catch_url_capture(srv, url, method, data) == CATCH_URL_ERR_BLOCK);
  htsthread_wait();
  deletesoc(srv);
  assertf(data[0] == '\0');
  printf("catchurl self-test OK (%d headers plus %d bytes kept, %d refused)\n",
         pads, fits, fits + 1);
  freet(data);
  return 0;
}

/* One over-long header line in a block small enough to leave in a single
   send(), so no later write can fail once the capture has hung up. */
static void st_catchurl_over_client(void *varg) {
  const st_catchurl_arg *const arg = (const st_catchurl_arg *) varg;
  char *blk = malloct(ST_CATCHURL_LONG + 128);
  const size_t req = strlen(ST_CATCHURL_REQ);
  T_SOC cli = st_catchurl_connect(arg->port);
  size_t n = req + ST_CATCHURL_LONG;

  assertf(blk != NULL);
  memcpy(blk, ST_CATCHURL_REQ, req);
  memcpy(blk + req, "X-Pad: ", 7);
  memset(blk + req + 7, 'a', ST_CATCHURL_LONG - 7);
  /* the CRLF the long line ends on, then a header that does fit, then the
     empty line: what a line-count or byte-count bound would still accept */
  memcpy(blk + n, "\r\nX-Ok: 1\r\n\r\n", 13);
  n += 13;
  assertf(send(cli, blk, (int) n, 0) == (int) n);
  (void) recv(cli, blk, ST_CATCHURL_LONG, 0);
  deletesoc(cli);
  freet(blk);
}

/* A peer sending nothing but over-long header lines. Each one used to charge
   the block budget nothing, so the capture read them for as long as they came;
   the send loop now ends on the hang-up instead of on its own cap. */
static void st_catchurl_flood_client(void *varg) {
  const st_catchurl_arg *const arg = (const st_catchurl_arg *) varg;
  const int len = ST_CATCHURL_LONG + 2;
  char *hdr = malloct((size_t) len);
  T_SOC cli = st_catchurl_connect(arg->port);
  int i;

  assertf(hdr != NULL);
  memset(hdr, 'a', (size_t) len);
  memcpy(hdr, "X-Pad: ", 7);
  hdr[ST_CATCHURL_LONG] = '\r';
  hdr[ST_CATCHURL_LONG + 1] = '\n';
  assertf(send(cli, ST_CATCHURL_REQ, (int) strlen(ST_CATCHURL_REQ), 0) > 0);
  for (i = 0; i < ST_CATCHURL_FLOOD; i++) {
    if (send(cli, hdr, len, 0) != len)
      break;
  }
  assertf(i < ST_CATCHURL_FLOOD);
  deletesoc(cli);
  freet(hdr);
}

/* A header line the line buffer cannot hold fails the capture. Skipping one
   left the block missing a header the browser sent, and charged the 32 KB
   budget nothing, so a peer sending only such lines held catch_url() open. */
static int st_catchurl_overlong(httrackp *opt, int argc, char **argv) {
  char BIGSTK url[HTS_URLMAXSIZE * 2];
  char method[32];
  char *data = malloct(CATCH_URL_DATA_SIZE);
  st_catchurl_arg arg;
  T_SOC srv;

  (void) opt;
  (void) argc;
  (void) argv;
  assertf(data != NULL);
  srv = st_catchurl_listen(&arg.port);
  arg.pads = arg.tail = 0;

  /* one over-long line, far under any block bound: refused, nothing kept */
  url[0] = method[0] = data[0] = '\0';
  assertf(hts_newthread(st_catchurl_over_client, &arg) == 0);
  assertf(catch_url_capture(srv, url, method, data) == CATCH_URL_ERR_HEADER);
  htsthread_wait();
  assertf(data[0] == '\0');

  /* nothing but over-long lines: the capture ends on the first one */
  url[0] = method[0] = data[0] = '\0';
  assertf(hts_newthread(st_catchurl_flood_client, &arg) == 0);
  assertf(catch_url_capture(srv, url, method, data) == CATCH_URL_ERR_HEADER);
  htsthread_wait();
  deletesoc(srv);
  assertf(data[0] == '\0');
  printf("catchurl over-long header self-test OK (%d bytes refused: %s)\n",
         ST_CATCHURL_LONG, catch_url_strerror(CATCH_URL_ERR_HEADER));
  freet(data);
  return 0;
}

/* What one catch_url_init* listener must hold. Consumes srv. */
static void st_catchurl_check_bind(T_SOC srv, int port, const char *adr) {
  char BIGSTK url[HTS_URLMAXSIZE * 2];
  char method[32];
  char *data = malloct(CATCH_URL_DATA_SIZE);
  struct sockaddr_in sa;
  SOClen len = sizeof(sa);
  st_catchurl_arg arg;

  assertf(data != NULL);
  assertf(srv != INVALID_SOCKET);

  /* what a peer on the network could reach */
  memset(&sa, 0, sizeof(sa));
  assertf(getsockname(srv, (struct sockaddr *) &sa, &len) == 0);
  assertf(sa.sin_family == AF_INET);
  assertf(ntohl(sa.sin_addr.s_addr) == INADDR_LOOPBACK);

  /* what the user is told to enter in the browser */
  assertf(strcmp(adr, "127.0.0.1") == 0);
  assertf(port != 0 && port == ntohs(sa.sin_port));

  /* and a capture over that very socket still succeeds */
  arg.port = port;
  arg.pads = 0;
  arg.tail = 7;
  url[0] = method[0] = data[0] = '\0';
  assertf(hts_newthread(st_catchurl_client, &arg) == 0);
  assertf(catch_url_capture(srv, url, method, data) == CATCH_URL_OK);
  htsthread_wait();
  deletesoc(srv);
  assertf(strcmp(method, "GET") == 0);
  assertf(strcmp(url, "http://example.com/") == 0);
  freet(data);
}

/* Both entry points, because catch_url_init_std() is the one the CLI calls and
   it asks for 8080 before falling back to an ephemeral port: a bind that is
   loopback only for the ephemeral case would leave the CLI on the LAN. */
static int st_catchurl_bind(httrackp *opt, int argc, char **argv) {
  char adr[128];
  T_SOC srv;
  int port = 0;

  (void) opt;
  (void) argc;
  (void) argv;

  /* poisoned, so a listener that wrote nothing is not read as "127.0.0.1" */
  memset(adr, 'x', sizeof(adr) - 1);
  adr[sizeof(adr) - 1] = '\0';
  srv = catch_url_init(&port, adr);
  st_catchurl_check_bind(srv, port, adr);

  memset(adr, 'x', sizeof(adr) - 1);
  srv = catch_url_init_std(&port, adr);
  st_catchurl_check_bind(srv, port, adr);

  printf("catchurl bind self-test OK (%s, both entry points)\n", adr);
  return 0;
}

// -#test=mptcp: the Multipath TCP switch - which protocol hts_socket_client()
// asks the kernel for, and whether a handshake that fell back to plain TCP is
// counted as a fallback rather than as a multipath connection.
#if HTS_INET_MPTCP
/* Open a loopback listener speaking `proto` and write its address back. */
static T_SOC st_mptcp_listen(int proto, struct sockaddr_in *addr) {
  const T_SOC srv = (T_SOC) socket(AF_INET, SOCK_STREAM, proto);
  socklen_t len = (socklen_t) sizeof(*addr);

  if (srv == INVALID_SOCKET)
    return INVALID_SOCKET;
  /* Bounded, or a connect that silently never connects wedges accept() until
     the suite's own watchdog, which reads as a hang and not as a failure. */
  {
    struct timeval tv;

    tv.tv_sec = 10;
    tv.tv_usec = 0;
    (void) setsockopt(srv, SOL_SOCKET, SO_RCVTIMEO, (const char *) &tv,
                      sizeof(tv));
  }
  memset(addr, 0, sizeof(*addr));
  addr->sin_family = AF_INET;
  addr->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (bind(srv, (struct sockaddr *) addr, sizeof(*addr)) != 0 ||
      listen(srv, 4) != 0 ||
      getsockname(srv, (struct sockaddr *) addr, &len) != 0) {
    close(srv);
    return INVALID_SOCKET;
  }
  return srv;
}

#ifdef IPPROTO_MPTCP
/* The protocol a socket was created with. SO_PROTOCOL is a Linux name, and so
   is IPPROTO_MPTCP, so every caller of this sits behind the same guard. */
static int st_mptcp_protocol(T_SOC soc) {
  int proto = -1;
  socklen_t len = (socklen_t) sizeof(proto);

  if (getsockopt(soc, SOL_SOCKET, SO_PROTOCOL, (char *) &proto, &len) != 0)
    return -1;
  return proto;
}
#endif

#ifdef IPPROTO_MPTCP
/* Connect a hand-made MPTCP socket to an MPTCP listener, bypassing the engine.
   Whether that negotiates is a property of the kernel and not of the diff: an
   emulated runner falls back between two MPTCP sockets. Returns -1 on a setup
   failure, otherwise whether it negotiated. */
static int st_mptcp_reference(void) {
  struct sockaddr_in addr;
  const T_SOC srv = st_mptcp_listen(IPPROTO_MPTCP, &addr);
  T_SOC cli, acc;
  int verdict = -1;

  if (srv == INVALID_SOCKET)
    return -1;
  cli = (T_SOC) socket(AF_INET, SOCK_STREAM, IPPROTO_MPTCP);
  if (cli == INVALID_SOCKET ||
      connect(cli, (struct sockaddr *) &addr, sizeof(addr)) != 0) {
    if (cli != INVALID_SOCKET)
      close(cli);
    close(srv);
    return -1;
  }
  acc = accept(srv, NULL, NULL);
  if (acc != INVALID_SOCKET) {
    verdict = hts_socket_is_mptcp(cli) ? 1 : 0;
    close(acc);
  }
  close(cli);
  close(srv);
  return verdict;
}
#endif

/* Make one connection to a listener speaking `server_proto`, through the same
   factory and connect the crawler uses, and prove a byte crosses it. Returns
   -1 when the connection could not be set up, otherwise whether Multipath TCP
   survived the handshake (1) or not (0). */
static int st_mptcp_pair(httrackp *opt, const char *tag, int server_proto) {
  struct sockaddr_in addr;
  const T_SOC srv = st_mptcp_listen(server_proto, &addr);
  T_SOC cli, acc;
  int verdict = -1;
  char byte = 0;

  if (srv == INVALID_SOCKET) {
    fprintf(stderr, "mptcp: cannot listen for %s\n", tag);
    return -1;
  }
  cli = hts_socket_client(AF_INET, opt, HTS_FALSE);
  if (cli == INVALID_SOCKET ||
      hts_socket_connect(cli, (struct sockaddr *) &addr, sizeof(addr), opt) !=
          0) {
    fprintf(stderr, "mptcp: cannot connect for %s\n", tag);
    if (cli != INVALID_SOCKET)
      close(cli);
    close(srv);
    return -1;
  }
  acc = accept(srv, NULL, NULL);
  /* A socket that connects but carries nothing is the failure a connect-only
     check cannot see. */
  if (acc == INVALID_SOCKET || send(cli, "x", 1, 0) != 1 ||
      recv(acc, &byte, 1, 0) != 1 || byte != 'x') {
    fprintf(stderr, "mptcp: %s connected but carried no data\n", tag);
  } else {
    verdict = hts_socket_is_mptcp(cli) ? 1 : 0;
    /* Which counter the crawler would reach for. Swapping the two arms of
       hts_mptcp_account() is invisible without this. */
    opt->mptcp_connections = opt->mptcp_fallbacks = 0;
    hts_mptcp_account(opt, cli);
    if (hts_mptcp_reports() &&
        (opt->mptcp_connections != (verdict == 1 ? 1 : 0) ||
         opt->mptcp_fallbacks != (verdict == 1 ? 0 : 1))) {
      fprintf(stderr, "mptcp: %s is negotiated=%d but counted %d and %d\n", tag,
              verdict, opt->mptcp_connections, opt->mptcp_fallbacks);
      verdict = -1;
    }
  }
  if (acc != INVALID_SOCKET)
    close(acc);
  close(cli);
  close(srv);
  return verdict;
}
#endif

static int st_mptcp(httrackp *opt, int argc, char **argv) {
#if HTS_INET_MPTCP
  const hts_tristate saved = opt->mptcp;
  int err = 0;
  int verdict;
  T_SOC soc;

  (void) argc;
  (void) argv;
  if (!hts_mptcp_available()) {
    printf("mptcp: SKIP (no Multipath TCP on this system)\n");
    return 77;
  }

  /* Off means a plain socket, and a run that did not ask counts nothing. */
  opt->mptcp = HTS_FALSE;
  soc = hts_socket_client(AF_INET, opt, HTS_FALSE);
  if (soc == INVALID_SOCKET) {
    fprintf(stderr, "mptcp: no socket with the option off\n");
    err = 1;
  } else {
#ifdef IPPROTO_MPTCP
    if (st_mptcp_protocol(soc) == IPPROTO_MPTCP) {
      fprintf(stderr, "mptcp: the option was off and MPTCP was asked for\n");
      err = 1;
    }
#endif
    if (hts_socket_is_mptcp(soc)) {
      fprintf(stderr, "mptcp: a plain TCP socket reported multipath\n");
      err = 1;
    }
    opt->mptcp_connections = opt->mptcp_fallbacks = 0;
    hts_mptcp_account(opt, soc);
    if (opt->mptcp_connections != 0 || opt->mptcp_fallbacks != 0) {
      fprintf(stderr, "mptcp: a run that did not ask for MPTCP was counted\n");
      err = 1;
    }
    close(soc);
  }

  opt->mptcp = HTS_TRUE;
#ifdef IPPROTO_MPTCP
  /* Only Linux names a protocol, so only there can the request be read back. */
  soc = hts_socket_client(AF_INET, opt, HTS_FALSE);
  if (soc == INVALID_SOCKET) {
    fprintf(stderr, "mptcp: no socket with the option on\n");
    err = 1;
  } else {
    const int proto = st_mptcp_protocol(soc);

    if (proto != IPPROTO_MPTCP) {
      fprintf(stderr,
              "mptcp: the option was on and protocol %d was asked for\n",
              proto);
      err = 1;
    }
    close(soc);
  }
#endif

  /* The factory and the connect seam end to end, against a peer that does not
     speak it. Runs wherever the feature is built, which is what covers the
     macOS connectx() path. */
  verdict = st_mptcp_pair(opt, "tcp peer", IPPROTO_TCP);
  if (verdict < 0) {
    err = 1;
  } else if (hts_mptcp_reports() && verdict != 0) {
    fprintf(stderr, "mptcp: a plain TCP peer was reported as multipath\n");
    err = 1;
  }

#ifdef IPPROTO_MPTCP
  /* The other direction, against a peer that speaks it. Compared with a socket
     made by hand rather than with 1, because whether two MPTCP sockets actually
     negotiate is the kernel's business: an emulated runner falls back, and that
     is not this engine being wrong. */
  if (hts_mptcp_reports()) {
    const int reference = st_mptcp_reference();

    verdict = st_mptcp_pair(opt, "mptcp peer", IPPROTO_MPTCP);
    if (verdict < 0 || reference < 0) {
      err = 1;
    } else if (verdict != reference) {
      fprintf(stderr,
              "mptcp: the engine's socket negotiated %d where one made by hand "
              "negotiated %d\n",
              verdict, reference);
      err = 1;
    }
  }
#endif

  opt->mptcp = saved;
  opt->mptcp_connections = opt->mptcp_fallbacks = 0;
  printf("mptcp: %s\n", err ? "FAIL" : "OK");
  return err;
#else
  (void) opt;
  (void) argc;
  (void) argv;
  printf("mptcp: SKIP (not built with Multipath TCP)\n");
  return 77;
#endif
}

/* ------------------------------------------------------------ */
/* Registry: this module's tests, in the order -#test lists them. */
/* ------------------------------------------------------------ */

const struct selftest_entry selftests_net[] = {
    {"mptcp", "",
     "which protocol an outgoing socket asks for, and whether a "
     "fallback to plain TCP is seen as one",
     st_mptcp},
    {"mptcpstats", "",
     "the Multipath TCP counters reach the stats and stay off another opt",
     st_mptcpstats},
    {"addrport", "",
     "\"host:port\" of a peer address is bounded and complete (#1493)",
     st_addrport},
    {"catchurl", "",
     "an over-long request header block fails the capture, not the process",
     st_catchurl},
    {"catchurl-overlong", "",
     "a request header line the line buffer cannot hold fails the capture",
     st_catchurl_overlong},
    {"catchurl-bind", "",
     "the capture proxy binds loopback and advertises the address it bound",
     st_catchurl_bind},
#ifndef _WIN32
    {"sigpipe", "", "a write to a vanished peer must not kill the process",
     st_sigpipe},
#endif
    {"ftpaddr", "<url-address>...", "host/port the FTP path splits out",
     st_ftpaddr},
    {"proxyurl", "<proxy-arg>", "parse a -P proxy URL into host/port",
     st_proxyurl},
    {"socks5", "", "SOCKS5 handshake framing and credential self-test",
     st_socks5},
    {"ftp-line", "", "get_ftp_line bounds a hostile FTP reply line",
     st_ftpline},
    {"ftp-userpass", "", "ftp_split_userpass bounds URL userinfo", st_ftpuser},
    {"ftp-ctrlchars", "", "send_line rejects a control byte in an FTP command",
     st_ftpctrl},
    {"ftp-cmdlen", "",
     "an FTP command too long for its control line is refused", st_ftpcmdlen},
    {NULL, NULL, NULL, NULL},
};
