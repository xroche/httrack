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
/* File: htscookie_selftest.c subroutines:                      */
/*       self-tests for cookie storage and matching             */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htsselftest_int.h"
#include "htscookie_selftest.h"

/* True if a Cookie header line is a whole RFC 6265 list of `count` pairs and
   nothing else. A dropped cookie must take its separator with it, because a
   trailing "a; " reads as part of the next value. */
static hts_boolean cookie_line_is_whole(const char *line, int count) {
  const char *end = strstr(line, H_CRLF);
  const char *p;
  int seen = 0;

  if (end == NULL || end[sizeof(H_CRLF) - 1] != '\0')
    return HTS_FALSE; // unterminated, or something follows the line
  if (strncmp(line, "Cookie: ", 8) != 0)
    return HTS_FALSE;
  p = line + 8;
  while (p < end) {
    const char *sep = strstr(p, "; ");
    const char *stop = (sep != NULL && sep < end) ? sep : end;
    const char *eq = memchr(p, '=', (size_t) (stop - p));

    if (eq == NULL || eq == p || eq + 1 == stop)
      return HTS_FALSE; // no '=', an empty name, or an empty value
    if (memchr(p, ';', (size_t) (stop - p)) != NULL)
      return HTS_FALSE; // a ';' inside a pair never came from this writer
    if (strncmp(p, "Cookie: ", 8) == 0)
      return HTS_FALSE; // the prefix again, so the line was rebuilt mid-way
    seen++;
    if (stop == end)
      break;
    p = sep + 2;
    if (p >= end)
      return HTS_FALSE; // a separator with no pair after it
  }
  return seen == count ? HTS_TRUE : HTS_FALSE;
}

static int st_cookies(httrackp *opt, int argc, char **argv) {
  static t_cookie cookie;
  char hdr[1024];
  char many_hdr[4096];
  /* RFC 6265: bare name=value pairs, no $Version/$Path (#151). */
  const char *expected = "Cookie: name=value; has_js=1" H_CRLF;
  const char *dom = "www.example.com";
  int err = 0;
  int added;

  (void) opt;
  (void) argc;
  (void) argv;
  cookie.max_len = sizeof(cookie.data);
  cookie.data[0] = '\0';
  added = cookie_add(&cookie, "name", "value", dom, "/");
  added |= cookie_add(&cookie, "has_js", "1", dom, "/");
  /* different domain: must be filtered out */
  added |= cookie_add(&cookie, "junk", "x", "other.org", "/");
  if (added) {
    printf("cookie-header: FAIL (cookie_add setup)\n");
    return 1;
  }

  http_cookie_header(&cookie, dom, "/", hdr, sizeof(hdr));
  if (strcmp(hdr, expected) != 0)
    err = 1;

  /* A hostile over-long request host must not overflow domain[] in
     treathead's default-domain copy (that would abort the mirror). */
  {
    static t_cookie ck2;
    htsblk r;
    char host[600];
    char line[64]; /* treathead NUL-cuts the header in place: never a literal */

    memset(&r, 0, sizeof(r));
    memset(host, 'a', sizeof(host) - 1);
    host[sizeof(host) - 1] = '\0';
    ck2.max_len = sizeof(ck2.data);
    ck2.data[0] = '\0';
    strcpybuff(line, "Set-Cookie: SID=1; path=/");
    treathead(&ck2, host, "/", &r, line);
    if (strnotempty(ck2.data)) // oversize-host cookie was not dropped
      err = 1;
    /* control: a normal host still yields a cookie through treathead */
    strcpybuff(line, "Set-Cookie: SID=1; path=/");
    treathead(&ck2, dom, "/", &r, line);
    if (strstr(ck2.data, "SID") == NULL) // guard wrongly dropped a valid cookie
      err = 1;
  }
  if (strstr(hdr, "$Version") != NULL || strstr(hdr, "$Path") != NULL)
    err = 1;
  if (strstr(hdr, "junk") != NULL) // wrong-domain cookie leaked
    err = 1;

  /* cookie_line_is_whole is the only barrier between a clipped line and a
     pass below, so what it refuses is asserted rather than assumed. */
  {
    static const struct {
      const char *line;
      int count;
    } bad[] = {
        {"Cookie: a=1; " H_CRLF, 1},                    // trailing separator
        {"Cookie: a=1", 1},                             // no CRLF
        {"Cookie: a=1" H_CRLF "x", 1},                  // trailing junk
        {"Cookie: a=1" H_CRLF "Cookie: b=2" H_CRLF, 2}, // a second line
        {"Cookie: Cookie: a=1" H_CRLF, 1},              // the prefix again
        {"Cookie: a=1; b=" H_CRLF, 2},                  // empty value
        {"Cookie: =1" H_CRLF, 1},                       // empty name
        {"Cookie: a=1;b=2" H_CRLF, 1},                  // ';' with no space
        {"Cookie: a=x; z=9" H_CRLF, 1}, // a value that forged a pair
        {"Cookie: a=1; b=2" H_CRLF, 1}, // count below the pairs present
        {"Cookie: a=1" H_CRLF, 2},      // count above them
        {"a=1" H_CRLF, 1},              // no prefix
    };

    size_t i;

    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
      if (cookie_line_is_whole(bad[i].line, bad[i].count)) {
        err = 1;
        printf("  accepted a bad line: %s\n", bad[i].line);
      }
    }
    if (!cookie_line_is_whole("Cookie: a=1; b=2" H_CRLF, 2))
      err = 1; // the control: a whole line must still pass
  }

  /* #101: every matching cookie is sent, not a fixed few. */
  {
    static t_cookie many;
    char tight[64];
    const int n = 20;
    hts_boolean clipped = HTS_FALSE;
    int i;
    int sent;

    many.max_len = sizeof(many.data);
    many.data[0] = '\0';
    for (i = 0; i < n; i++) {
      char name[16];
      char value[16];

      snprintf(name, sizeof(name), "c%02d", i);
      snprintf(value, sizeof(value), "v%02d", i);
      if (cookie_add(&many, name, value, dom, "/") != 0)
        err = 1;
    }
    sent = http_cookie_header(&many, dom, "/", many_hdr, sizeof(many_hdr));
    if (sent != n)
      err = 1;
    if (!cookie_line_is_whole(many_hdr, n))
      err = 1;
    for (i = 0; i < n; i++) {
      char pair[24];

      snprintf(pair, sizeof(pair), "c%02d=v%02d", i, i);
      if (strstr(many_hdr, pair) == NULL)
        err = 1;
    }

    /* A buffer too small for the whole set still ends clean, and the clip
       lands between a pair, on its separator or on the CRLF by the size. */
    for (i = 1; i <= (int) sizeof(tight); i++) {
      memset(tight, 'X', sizeof(tight)); /* a lost NUL must not read as empty */
      sent = http_cookie_header(&many, dom, "/", tight, (size_t) i);
      if (sent < 0 || sent > n)
        err = 1;
      else if (sent == 0) {
        if (tight[0] != '\0')
          err = 1;
      } else {
        if (sent < n)
          clipped = HTS_TRUE;
        if (!cookie_line_is_whole(tight, sent)) {
          err = 1;
          printf("  clipped at size %d: %s\n", i, tight);
        }
      }
    }
    if (!clipped) // a sweep that never clipped proves nothing about clipping
      err = 1;
  }
#ifndef _WIN32
  /* the jar holds live session cookies: cookie_save must keep it 0600 */
  {
    const char *jar = "st-cookies-jar.txt";
    struct stat st;
    FILE *fp;

    (void) UNLINK(jar);
    assertf(cookie_save(NULL, &cookie, jar) == 0);
    // check the opened file, not its name, so nothing can swap it in between
    assertf((fp = FOPEN(jar, "rb")) != NULL && fstat(fileno(fp), &st) == 0);
    fclose(fp);
    assertf((st.st_mode & 07777) == HTS_PROTECT_FILE);
    assertf(st.st_size > 0); /* mode-only checks would pass an empty jar */
    /* a pre-existing world-readable jar must be tightened, not kept */
    assertf((fp = FOPEN(jar, "ab")) != NULL && fchmod(fileno(fp), 0644) == 0);
    fclose(fp);
    assertf(cookie_save(NULL, &cookie, jar) == 0);
    assertf((fp = FOPEN(jar, "rb")) != NULL && fstat(fileno(fp), &st) == 0);
    fclose(fp);
    assertf((st.st_mode & 07777) == HTS_PROTECT_FILE);
    assertf(st.st_size > 0);
    (void) UNLINK(jar);
  }
#endif
  printf("cookie-header: %s\n", err ? "FAIL" : "OK");
  if (err) {
    printf("  got: %s\n", hdr);
    printf("  many: %s\n", many_hdr);
  }
  return err;
}

/* Store a Set-Cookie header as if it arrived from host, then report whether
   the jar sends it back to query. */
static hts_boolean cookie_roundtrip(const char *host, const char *header,
                                    const char *query, const char *needle) {
  static t_cookie cookie;
  htsblk r;
  char line[256];
  char hdr[1024];

  memset(&r, 0, sizeof(r));
  cookie.max_len = sizeof(cookie.data);
  cookie.data[0] = '\0';
  strcpybuff(line, header); // treathead NUL-cuts in place: never a literal
  treathead(&cookie, host, "/", &r, line);
  http_cookie_header(&cookie, query, "/", hdr, sizeof(hdr));
  return strstr(hdr, needle) != NULL ? HTS_TRUE : HTS_FALSE;
}

/* cookie_host must map ADR to WANT, or refuse ADR when WANT is NULL. */
static hts_boolean cookie_host_is(const char *adr, const char *want) {
  char host[COOKIE_MAX_DOMAIN];
  const hts_boolean ok = cookie_host(adr, host, sizeof(host));

  if (want == NULL)
    return ok ? HTS_FALSE : HTS_TRUE;
  return ok && strcmp(host, want) == 0 ? HTS_TRUE : HTS_FALSE;
}

/* Report a missed expectation under LABEL, and count it. */
static int cookie_expect_at(const char *label, hts_boolean got,
                            hts_boolean want, const char *what) {
  if (got == want)
    return 0;
  printf("%s: %s\n", label, what);
  return 1;
}

static int cookie_expect(hts_boolean got, hts_boolean want, const char *what) {
  return cookie_expect_at("cookie-port", got, want, what);
}

/* A cookie is scoped to a host, never to a port (RFC 6265), so a jar exported
   from a browser carries no port and must still match. */
static int st_cookieport(httrackp *opt, int argc, char **argv) {
  static const struct {
    const char *adr;
    const char *want; // NULL: cookie_host must refuse it
  } hosts[] = {
      {"example.com", "example.com"},   // the 99% path, untouched
      {".example.com", ".example.com"}, // a wildcard domain, untouched
      {"example.com:8080", "example.com"},
      {"user:pass@example.com:8080", "example.com"},
      {"[::1]:8080", "::1"}, // one representation, bracketed or bare
      {"[::1]", "::1"},
      {"::1", "::1"},
      {"", NULL}, // an empty domain is a suffix of every host
      {":8080", NULL},
      {"[]:80", NULL},
      // a URI spells a zone id's '%' as "%25" and a jar carries it bare
      {"[fe80::1%25eth0]:8080", "fe80::1%eth0"},
      {"[fe80::1%25eth0]", "fe80::1%eth0"},
      {"fe80::1%eth0", "fe80::1%eth0"},     // a browser export carries this
      {"fe80::1%25eth0", "fe80::1%25eth0"}, // a bare '%' is literal already
      {"fe80::1%250", "fe80::1%250"},       // scope index 250, not zone 0
      {"host%25name.example", "host%25name.example"}, // a name has no zone id
      // the rest of the zone id keeps whatever escapes it carries
      {"[fe80::1%25eth%250]", "fe80::1%eth%250"},
      {"[fe80::1%2525eth0]", "fe80::1%25eth0"}, // a zone id may open with "%25"
      {"[fe80::1%]:80", "fe80::1%"}, // truncated, so it decodes nothing
      {"[fe80::1%2]:80", "fe80::1%2"},
      {"[fe80::1%a5eth0]", "fe80::1%a5eth0"}, // 'a5' is not the introducer
      // an empty zone id is malformed, and still lands on one jar entry
      {"[fe80::1%25]:80", "fe80::1%"},
      // the request names an international host in its ASCII form
      {"WWW.B\303\274cher.example:81", "www.xn--bcher-kva.example"},
      {".b\303\274cher.example", ".xn--bcher-kva.example"},
      // IDNA would encode these as another host, or one holding a '/'
      {".\300\257.example", NULL},               // an overlong '/'
      {".\200.example", NULL},                   // a stray continuation byte
      {".\355\240\200.example", NULL},           // a surrogate
      {".\364\220\200\200.example", NULL},       // above U+10FFFF
      {".b\303.example", NULL},                  // a truncated sequence
      {".B\303\234CHER.example", NULL},          // a capital outside ASCII
      {".\320\220.example", NULL},               // a Cyrillic capital
      {".\320\260.example", ".xn--80a.example"}, // its small letter
      {".\304\200.example", NULL},               // U+0100, Latin Extended-A
      {".\305\271.example", NULL},               // U+0179, an odd capital there
      {".\316\221.example", NULL},               // U+0391, Greek
      {".\317\242.example", NULL},               // U+03E2, Coptic
      {".\323\220.example", NULL},               // U+04D0, Cyrillic extended
      {".\323\201.example", NULL},               // U+04C1, an odd capital there
      {".\324\261.example", NULL},               // U+0531, Armenian
      {".\341\270\200.example", NULL}, // U+1E00, Latin Extended Additional
      {".\357\274\241.example", NULL}, // U+FF21, fullwidth
      {".\303\227.example", ".xn--rca.example"}, // U+00D7 sits among capitals
      {".\316\261.example", ".xn--mxa.example"}, // a small Greek letter
      // fits as typed, but not once in its ASCII form
      {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
       "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
       "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
       "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\303\274",
       NULL},
  };

  static const struct {
    const char *host, *header, *query, *needle;
    hts_boolean want;
    const char *what;
  } trips[] = {
      // an explicit domain= never carries a port, so a port kept on the query
      // side loses the cookie outright
      {"localhost:8080", "Set-Cookie: withdom=D; path=/; domain=localhost",
       "localhost:8080", "withdom=D", HTS_TRUE, "domain=localhost off :8080"},
      // no domain=, so the default is the request host: stripping on one side
      // only breaks these two
      {"localhost:8080", "Set-Cookie: plain=P; path=/", "localhost:8080",
       "plain=P", HTS_TRUE, "no domain= off :8080"},
      {"localhost:8080", "Set-Cookie: plain=P; path=/", "localhost:9090",
       "plain=P", HTS_TRUE, ":8080 reaches :9090"},
      {"example.com", "Set-Cookie: plain=P; path=/", "other.com", "plain=P",
       HTS_FALSE, "example.com must not reach other.com"},
      // the wildcard match is by suffix, so the port must be off the query
      {"www.example.com:8080",
       "Set-Cookie: wild=W; path=/; domain=.example.com", "api.example.com",
       "wild=W", HTS_TRUE, "domain=.example.com must reach api.example.com"},
      // a bracketed IPv6 literal has a port to cut, a bare one has not, and
      // both spellings must reach the same jar entry either way round
      {"[::1]:8080", "Set-Cookie: six=6; path=/", "[::1]:9090", "six=6",
       HTS_TRUE, "[::1]:8080 reaches [::1]:9090"},
      {"::1", "Set-Cookie: six=6; path=/", "::1", "six=6", HTS_TRUE,
       "::1 reaches ::1"},
      {"[::1]:8080", "Set-Cookie: six=6; path=/", "::1", "six=6", HTS_TRUE,
       "[::1]:8080 reaches ::1"},
      {"::1", "Set-Cookie: six=6; path=/", "[::1]:8080", "six=6", HTS_TRUE,
       "::1 reaches [::1]:8080"},
      // truncating a bare IPv6 literal at its first colon would leave an empty
      // domain, and an empty domain is a suffix of every host
      {"::1", "Set-Cookie: six=6; path=/", "example.com", "six=6", HTS_FALSE,
       "::1 must not reach example.com"},
      {":8080", "Set-Cookie: pwn=OWNED; path=/", "victim.example", "pwn=OWNED",
       HTS_FALSE, "a host that is only a port must not reach victim.example"},
      // a bracketed single-colon literal is the one spelling whose normalised
      // form still holds a colon, so normalising it twice would file it under
      // "x" while every query still asks for "x:1"
      {"[x:1]:80", "Set-Cookie: odd=O; path=/", "[x:1]:80", "odd=O", HTS_TRUE,
       "the store side normalised the host twice"},
      // a link-local host reaches the zone id spelling a browser exports, and
      // stops at another zone
      {"[fe80::1%25eth0]:8080", "Set-Cookie: zone=Z; path=/", "fe80::1%eth0",
       "zone=Z", HTS_TRUE, "[fe80::1%25eth0] does not reach fe80::1%eth0"},
      {"fe80::1%eth0", "Set-Cookie: zone=Z; path=/", "[fe80::1%25eth0]:8080",
       "zone=Z", HTS_TRUE, "fe80::1%eth0 does not reach [fe80::1%25eth0]"},
      {"[fe80::1%25eth0]:8080", "Set-Cookie: zone=Z; path=/", "fe80::1%eth1",
       "zone=Z", HTS_FALSE, "a cookie on %eth0 reached %eth1"},
  };

  static t_cookie jar;
  char hdr[1024];
  size_t i;
  int err = 0;

  (void) opt;
  (void) argc;
  (void) argv;

  for (i = 0; i < sizeof(hosts) / sizeof(hosts[0]); i++) {
    char what[512];

    snprintf(what, sizeof(what), "'%s' is not scoped to %s", hosts[i].adr,
             hosts[i].want != NULL ? hosts[i].want : "(refused)");
    err |= cookie_expect(cookie_host_is(hosts[i].adr, hosts[i].want), HTS_TRUE,
                         what);
    if (hosts[i].want == NULL)
      continue;
    /* Idempotence: the store and send paths must survive a second pass, or a
       host normalised twice lands under a name no query ever asks for. */
    snprintf(what, sizeof(what), "'%s' does not survive a second pass",
             hosts[i].want);
    err |= cookie_expect(cookie_host_is(hosts[i].want, hosts[i].want), HTS_TRUE,
                         what);
  }
  for (i = 0; i < sizeof(trips) / sizeof(trips[0]); i++) {
    err |= cookie_expect(cookie_roundtrip(trips[i].host, trips[i].header,
                                          trips[i].query, trips[i].needle),
                         trips[i].want, trips[i].what);
  }

  /* A jar loaded from a browser file holds bare hosts; the port on the wire
     must not hide them. Control: another host stays filtered out. */
  jar.max_len = sizeof(jar.data);
  jar.data[0] = '\0';
  if (cookie_add(&jar, "fromfile", "F", "localhost", "/") != 0 ||
      cookie_add(&jar, "junk", "x", "example.org", "/") != 0) {
    printf("cookie-port: FAIL (cookie_add setup)\n");
    return 1;
  }
  http_cookie_header(&jar, "localhost:8080", "/", hdr, sizeof(hdr));
  err |= cookie_expect(strstr(hdr, "fromfile=F") != NULL, HTS_TRUE,
                       "a bare jar host is hidden by the port on the wire");
  err |= cookie_expect(strstr(hdr, "junk") != NULL, HTS_FALSE,
                       "example.org leaked to localhost");

  /* An older httrack, and a hand-edited jar, wrote the port into the domain
     field. cookie_find can never match a stored domain longer than the query,
     so the jar has to be normalised as it is read. */
  jar.data[0] = '\0';
  if (cookie_add(&jar, "sess", "PERSIST", "127.0.0.1:8080", "/") != 0 ||
      cookie_add(&jar, "sixjar", "V", "[::1]:8080", "/") != 0) {
    printf("cookie-port: FAIL (cookie_add setup, port form)\n");
    return 1;
  }
  http_cookie_header(&jar, "127.0.0.1", "/", hdr, sizeof(hdr));
  err |= cookie_expect(strstr(hdr, "sess=PERSIST") != NULL, HTS_TRUE,
                       "a jar storing 127.0.0.1:8080 lost its session");
  http_cookie_header(&jar, "::1", "/", hdr, sizeof(hdr));
  err |= cookie_expect(strstr(hdr, "sixjar=V") != NULL, HTS_TRUE,
                       "a jar storing [::1]:8080 lost its session");
  /* Only this case drives cookie_add with a zone id, which the host rows above
     never do. */
  if (cookie_add(&jar, "zonejar", "Z", "[fe80::1%25eth0]:8080", "/") != 0) {
    printf("cookie-port: FAIL (cookie_add setup, zone id form)\n");
    return 1;
  }
  http_cookie_header(&jar, "fe80::1%eth0", "/", hdr, sizeof(hdr));
  err |= cookie_expect(strstr(hdr, "zonejar=Z") != NULL, HTS_TRUE,
                       "a jar storing [fe80::1%25eth0] lost its session");
  err |= cookie_expect(cookie_add(&jar, "pwn", "OWNED", "", "/") == 0,
                       HTS_FALSE, "an empty domain entered the jar");

  /* cookie_remove normalises like cookie_add, so one string reaches the same
     entry through either. Control: the neighbour stays. */
  jar.data[0] = '\0';
  if (cookie_add(&jar, "gone", "G", "example.com:8080", "/") != 0 ||
      cookie_add(&jar, "kept", "K", "other.example", "/") != 0) {
    printf("cookie-port: FAIL (cookie_add setup, delete form)\n");
    return 1;
  }
  cookie_remove(&jar, "gone", "example.com:8080", "/");
  err |= cookie_expect(strstr(jar.data, "gone") != NULL, HTS_FALSE,
                       "a port-qualified delete missed its cookie");
  err |= cookie_expect(strstr(jar.data, "kept") != NULL, HTS_TRUE,
                       "the delete took its neighbour with it");

  printf("cookie-port: %s\n", err ? "FAIL" : "OK");
  return err;
}

/* Write one raw jar record, the way a file written elsewhere reaches
   cookie_find, because cookie_add would fold the domain on the way in. */
static void cookie_seed(t_cookie *jar, const char *domain, const char *path) {
  jar->max_len = sizeof(jar->data);
  snprintf(jar->data, sizeof(jar->data),
           "%s\tTRUE\t%s\tFALSE\t1999999999\tid\tA\n", domain, path);
}

/* RFC 6265 matches a cookie domain without regard to case, because host names
   are case-insensitive. The name and the path beside it are byte-exact. */
static int st_cookiecase(httrackp *opt, int argc, char **argv) {
  static const char LABEL[] = "cookie-case";

  static const struct {
    const char *adr, *want;
  } hosts[] = {
      {"Example.COM", "example.com"},
      {"WWW.Example.COM:8080", "www.example.com"},
      {"[::FFFF:1]:80", "::ffff:1"},
      {"example.com", "example.com"}, // the 99% path, untouched
  };

  static const struct {
    const char *domain, *path; // as the jar on disk holds them
    const char *query, *qpath; // as the request asks for them
    hts_boolean want;
    const char *what;
  } jars[] = {
      // the shape #1625 was reported with
      {"Example.COM", "/", "example.com", "/", HTS_TRUE,
       "a jar domain 'Example.COM' never reaches example.com"},
      // shorter than the query, the one shape the wildcard compare refuses
      {"Example.COM", "/", "www.example.com", "/", HTS_TRUE,
       "'Example.COM' never reaches www.example.com"},
      {"Example.COM", "/", "other.example", "/", HTS_FALSE,
       "Example.COM leaked to other.example"},
      // longer than the query, so only the leading-dot strip can match it
      {".Example.COM", "/", "example.com", "/", HTS_TRUE,
       "a wildcard '.Example.COM' never reaches example.com"},
      {".Example.COM", "/", "notexample.com", "/", HTS_FALSE,
       ".Example.COM leaked to notexample.com"},
      // the fold stops at the domain, so /Dir/ and /dir/ are two paths
      {"example.com", "/Dir/", "example.com", "/Dir/page", HTS_TRUE,
       "an exact path stopped matching"},
      {"example.com", "/Dir/", "example.com", "/dir/page", HTS_FALSE,
       "the path match folded case"},
  };

  /* http_cookie_header folds the query through cookie_host before it asks, so
     only a direct call says what cookie_find promises a caller of its own. */
  static const struct {
    const char *name, *query;
    hts_boolean want;
    const char *what;
  } finds[] = {
      {"id", "WWW.EXAMPLE.com", HTS_TRUE,
       "cookie_find refused an unfolded query domain"},
      {"id", "www.example.com", HTS_TRUE, "cookie_find missed its own jar"},
      // RFC 6265 makes the cookie name byte-exact, like the path
      {"ID", "www.example.com", HTS_FALSE, "the name compare folded case"},
  };

  static t_cookie jar;
  char hdr[1024];
  size_t i;
  int err = 0;

  (void) opt;
  (void) argc;
  (void) argv;

  for (i = 0; i < sizeof(hosts) / sizeof(hosts[0]); i++) {
    char what[512];

    snprintf(what, sizeof(what), "'%s' is not scoped to %s", hosts[i].adr,
             hosts[i].want);
    err |= cookie_expect_at(LABEL, cookie_host_is(hosts[i].adr, hosts[i].want),
                            HTS_TRUE, what);
  }

  /* The jar is written back to cookies.txt for other tools to read, so the
     folded spelling has to be the one stored, not one the compare forgives. */
  jar.max_len = sizeof(jar.data);
  jar.data[0] = '\0';
  if (cookie_add(&jar, "id", "A", "Example.COM", "/") != 0) {
    printf("%s: FAIL (cookie_add setup)\n", LABEL);
    return 1;
  }
  err |= cookie_expect_at(LABEL, strstr(jar.data, "example.com\t") != NULL,
                          HTS_TRUE, "the jar did not store a folded domain");
  err |= cookie_expect_at(LABEL, strstr(jar.data, "Example") != NULL, HTS_FALSE,
                          "the jar kept the domain's original case");

  for (i = 0; i < sizeof(jars) / sizeof(jars[0]); i++) {
    cookie_seed(&jar, jars[i].domain, jars[i].path);
    http_cookie_header(&jar, jars[i].query, jars[i].qpath, hdr, sizeof(hdr));
    err |= cookie_expect_at(LABEL, strstr(hdr, "id=A") != NULL, jars[i].want,
                            jars[i].what);
  }

  cookie_seed(&jar, "example.com", "/");
  for (i = 0; i < sizeof(finds) / sizeof(finds[0]); i++) {
    err |= cookie_expect_at(
        LABEL,
        cookie_find(jar.data, finds[i].name, finds[i].query, "/") != NULL,
        finds[i].want, finds[i].what);
  }

  /* The store side of the same report, for example a server that names its
     own host in another case. */
  err |= cookie_expect_at(
      LABEL,
      cookie_roundtrip("example.com",
                       "Set-Cookie: sess=S; path=/; domain=Example.COM",
                       "example.com", "sess=S"),
      HTS_TRUE, "domain=Example.COM never came back to example.com");

  printf("%s: %s\n", LABEL, err ? "FAIL" : "OK");
  return err;
}

/* RFC 6265 section 5.1.3 lets a jar domain reach a host only when it names
   that host, or a parent domain ending a whole label of a name. A byte suffix
   is not enough: wwwexample.com and notevil.com are separately registrable. */
static int st_cookiedomain(httrackp *opt, int argc, char **argv) {
  static const char LABEL[] = "cookie-domain";

  static const struct {
    const char *jar_dom, *host;
    hts_boolean want;
    const char *what;
  } pairs[] = {
      /* The same host, the 99% path. An address names itself too, so the rule
         below must refuse a parent without refusing this. */
      {"example.com", "example.com", HTS_TRUE, "a jar lost its own host"},
      {"EXAMPLE.com", "example.com", HTS_TRUE,
       "an identical host stopped folding case"},
      {"localhost", "localhost", HTS_TRUE,
       "a single-label host lost its cookie"},
      {"127.0.0.1", "127.0.0.1", HTS_TRUE,
       "an IPv4 literal lost its own cookie"},
      {"::1", "::1", HTS_TRUE, "an IPv6 literal lost its own cookie"},
      {"example.com.", "example.com.", HTS_TRUE,
       "a trailing-dot FQDN lost its own cookie"},

      /* A parent domain, cut on a label boundary. */
      {"example.com", "www.example.com", HTS_TRUE,
       "example.com never reaches www.example.com"},
      {"example.com", "a.b.example.com", HTS_TRUE,
       "example.com never reaches a.b.example.com"},
      {".example.com", "example.com", HTS_TRUE,
       "'.example.com' never reaches example.com"},
      {".example.com", "www.example.com", HTS_TRUE,
       "'.example.com' never reaches www.example.com"},
      {".EXAMPLE.com", "WWW.example.com", HTS_TRUE,
       "a dotted jar domain stopped folding case"},

      /* #1638: a byte suffix that starts mid-label. Both hosts are separately
         registrable, so this is a cross-origin leak. */
      {"example.com", "wwwexample.com", HTS_FALSE,
       "example.com leaked to wwwexample.com"},
      {"evil.com", "notevil.com", HTS_FALSE, "evil.com leaked to notevil.com"},
      {"ample.com", "example.com", HTS_FALSE,
       "ample.com leaked to example.com"},
      {".example.com", "wwwexample.com", HTS_FALSE,
       "'.example.com' leaked to wwwexample.com"},
      {"e.com", "we.com", HTS_FALSE, "e.com leaked to we.com"},

      /* An empty domain is a suffix of every host. */
      {"", "example.com", HTS_FALSE,
       "an empty jar domain leaked to example.com"},
      {"", "", HTS_FALSE, "an empty jar domain matched an empty host"},

      /* RFC 6265 5.2.3 strips one leading dot, so a second one leaves an empty
         label. "..example.com" is no domain, and must therefore reach none. */
      {"..example.com", "www.example.com", HTS_FALSE,
       "'..example.com' reached www.example.com"},
      {"..example.com", "example.com", HTS_FALSE,
       "'..example.com' reached example.com"},

      /* An address has no parent domain, because every dot is inside it. */
      {"1.1", "192.168.1.1", HTS_FALSE,
       "the jar domain 1.1 leaked to 192.168.1.1"},
      {"0.1", "127.0.0.1", HTS_FALSE, "the jar domain 0.1 leaked to 127.0.0.1"},
      {".1.1", "192.168.1.1", HTS_FALSE,
       "a leading dot let 1.1 reach 192.168.1.1"},
      {"168.1.1", "192.168.1.1", HTS_FALSE, "168.1.1 leaked to 192.168.1.1"},
      /* cookie_host unbrackets IPv6, so this is the shape that arrives here */
      {"3.4", "::ffff:1.2.3.4", HTS_FALSE,
       "3.4 leaked to the IPv6 literal ::ffff:1.2.3.4"},
      {"1", "::1", HTS_FALSE, "the jar domain 1 leaked to ::1"},

      /* Longer than the host, and a trailing dot is a label of its own. These
         three document the dom_len guard rather than pin it, because dropping
         it leaves a read one byte short of the host that mismatches anyway. */
      {"www.example.com", "example.com", HTS_FALSE,
       "a child domain reached its parent"},
      {"example.com", "example.com.", HTS_FALSE,
       "example.com leaked to the FQDN example.com."},
      {"example.com.", "example.com", HTS_FALSE,
       "the FQDN example.com. leaked to example.com"},
  };

  /* The same rule as the jar sees it, so a caller reaching cookie_find through
     the request path gets the same verdict as a direct call. */
  static const struct {
    const char *jar_dom, *query;
    hts_boolean want;
    const char *what;
  } trips[] = {
      {"example.com", "wwwexample.com", HTS_FALSE,
       "the jar sent example.com's cookie to wwwexample.com"},
      {"example.com", "www.example.com", HTS_TRUE,
       "the jar withheld example.com's cookie from www.example.com"},
      {".example.com", "www.example.com", HTS_TRUE,
       "the jar withheld '.example.com' from www.example.com"},
      {"1.1", "192.168.1.1", HTS_FALSE,
       "the jar sent 1.1's cookie to 192.168.1.1"},
      {"192.168.1.1", "192.168.1.1", HTS_TRUE,
       "the jar withheld an address cookie from its own address"},
  };

  static t_cookie jar;
  char hdr[1024];
  size_t i;
  int err = 0;

  (void) opt;
  (void) argc;
  (void) argv;

  for (i = 0; i < sizeof(pairs) / sizeof(pairs[0]); i++) {
    err |= cookie_expect_at(
        LABEL, cookie_domain_match(pairs[i].jar_dom, pairs[i].host),
        pairs[i].want, pairs[i].what);
  }

  for (i = 0; i < sizeof(trips) / sizeof(trips[0]); i++) {
    cookie_seed(&jar, trips[i].jar_dom, "/");
    http_cookie_header(&jar, trips[i].query, "/", hdr, sizeof(hdr));
    err |= cookie_expect_at(LABEL, strstr(hdr, "id=A") != NULL, trips[i].want,
                            trips[i].what);
  }

  printf("%s: %s\n", LABEL, err ? "FAIL" : "OK");
  return err;
}

/* cookie_add must refuse rather than write past max_len, whatever the caller
   set that to, and must leave the bytes beyond it alone. */
static int st_cookiecap(httrackp *opt, int argc, char **argv) {
  static t_cookie cookie;
  /* Above the COOKIE_HEADROOM bytes cookie_add reserves, or it refuses all. */
  const size_t cap = 512;
  static const char NAME[] = "n", DOM[] = "www.example.com", PATH[] = "/";
  char big[1024];
  int err = 0;
  size_t i;

  (void) opt;
  (void) argc;
  (void) argv;

  /* A non-zero canary: a stray NUL from an off-by-one terminator is invisible
     against a zero fill, which is the exact bug a capacity check guards. */
  memset(cookie.data, 'C', sizeof(cookie.data));
  cookie.max_len = cap;
  cookie.data[0] = '\0';

  memset(big, 'v', sizeof(big) - 1);
  big[sizeof(big) - 1] = '\0';

  /* Far past the capacity: refused, and nothing written. */
  if (cookie_add(&cookie, NAME, big, DOM, PATH) == 0) {
    printf("cookie-cap: FAIL (a value %u bytes long was accepted into %u)\n",
           (unsigned) strlen(big), (unsigned) cap);
    err = 1;
  }
  if (cookie.data[0] != '\0') {
    printf("cookie-cap: FAIL (the store was written despite the refusal)\n");
    err = 1;
  }
  for (i = cap; i < sizeof(cookie.data); i++) {
    if (cookie.data[i] != 'C') {
      printf("cookie-cap: FAIL (byte %u past max_len was modified)\n",
             (unsigned) i);
      err = 1;
      break;
    }
  }

  /* The exact boundary, derived rather than hardcoded so it survives a change
     to the fixture strings: cookie_add reserves COOKIE_HEADROOM past them. */
  {
    const size_t fixed =
        strlen(NAME) + strlen(DOM) + strlen(PATH) + COOKIE_HEADROOM;
    const size_t fits = cap - fixed;

    /* Exactly filling max_len is accepted... */
    memset(big, 'v', fits);
    big[fits] = '\0';
    cookie.data[0] = '\0';
    if (cookie_add(&cookie, NAME, big, DOM, PATH) != 0) {
      printf("cookie-cap: FAIL (a value of %u, exactly max_len, was refused)\n",
             (unsigned) fits);
      err = 1;
    } else if (strlen(cookie.data) == 0 || strlen(cookie.data) >= cap) {
      printf("cookie-cap: FAIL (accepted, store is %u against max_len %u)\n",
             (unsigned) strlen(cookie.data), (unsigned) cap);
      err = 1;
    }

    /* ...and one byte more is not. */
    memset(big, 'v', fits + 1);
    big[fits + 1] = '\0';
    cookie.data[0] = '\0';
    if (cookie_add(&cookie, NAME, big, DOM, PATH) == 0) {
      printf(
          "cookie-cap: FAIL (a value of %u, one past max_len, was accepted)\n",
          (unsigned) (fits + 1));
      err = 1;
    } else if (cookie.data[0] != '\0') {
      printf("cookie-cap: FAIL (the store was written despite the refusal)\n");
      err = 1;
    }
  }

  /* The canary again, after the accepted write: an insert bounded by
     sizeof(data) rather than max_len would show up here. */
  for (i = cap; i < sizeof(cookie.data); i++) {
    if (cookie.data[i] != 'C') {
      printf("cookie-cap: FAIL (byte %u past max_len was modified)\n",
             (unsigned) i);
      err = 1;
      break;
    }
  }

  printf("cookie-cap: %s\n", err ? "FAIL" : "OK");
  return err;
}

/* -#test=cookieimport <dir>: load a jar (and, on Windows, copied IE cookies
   *@*.txt) from a long, non-ASCII folder via the UTF-8/long-path wrappers
   (#133). POSIX compiles the IE block out; there it is a positive control. */
static int st_cookieimport(httrackp *opt, int argc, char **argv) {
  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "cookieimport: needs a writable base dir\n");
    return 1;
  }
  char dir[HTS_URLMAXSIZE * 2];
  size_t base = 0;
  const size_t dirlen =
      st_mkdeep(dir, sizeof(dir), argv[0], "/" ST_NONASCII "-cookie-seg",
                "cookieimport", &base);

  if (dirlen == 0) {
    return 1;
  }

  char fpath[HTS_URLMAXSIZE * 2];
  char file[HTS_URLMAXSIZE * 2];

  assertf(sprintfbuff(fpath, "%s/", dir)); /* IE glob wants a trailing sep */

  /* cookies.txt: one Netscape record (host, _, path, _, _, name, value). */
  assertf(sprintfbuff(file, "%scookies.txt", fpath));
  {
    FILE *fp = FOPEN(file, "wb");

    assertf(fp != NULL);
    fprintf(fp, "www.example.com\tFALSE\t/\tFALSE\t0\tJARCOOK\tjarval\n");
    fclose(fp);
  }

  /* A copied IE cookie u@v.txt: name, value, url, then 6 unused fields. */
  assertf(sprintfbuff(file, "%su@v.txt", fpath));
  {
    FILE *fp = FOPEN(file, "wb");

    assertf(fp != NULL);
    fprintf(fp, "IECOOK\nieval\nwww.example.com/\n0\n0\n0\n0\n0\n*\n");
    fclose(fp);
  }

  static t_cookie ck;

  ck.max_len = sizeof(ck.data);
  ck.data[0] = '\0';
  assertf(cookie_load(NULL, &ck, fpath, "cookies.txt") == 0);
  assertf(strstr(ck.data, "JARCOOK") != NULL); /* jar read on a long path */
#ifdef _WIN32
  /* the IE scan merged the cookie and unlinked the consumed file */
  assertf(strstr(ck.data, "IECOOK") != NULL);
  assertf(FOPEN(file, "rb") == NULL);
#endif

  (void) UNLINK(file); /* u@v.txt (already gone on Windows) */
  assertf(sprintfbuff(file, "%scookies.txt", fpath));
  (void) UNLINK(file);
  dir[dirlen] = '\0';
  while (strlen(dir) > base) {
    char *const slash = strrchr(dir, '/');

    if (RMDIR(dir) != 0) {
      fprintf(stderr, "cookieimport: rmdir failed: %s\n", strerror(errno));
      return 1;
    }
    if (slash == NULL || (size_t) (slash - dir) < base) {
      break;
    }
    *slash = '\0';
  }

  printf("cookieimport: merged jar%s under a %u-char non-ASCII dir: OK\n",
#ifdef _WIN32
         " + IE cookie",
#else
         "",
#endif
         (unsigned) dirlen);
  return 0;
}

/* Compare cookie_get with a frozen copy. */

#define COOKIEDIFF_SIZE 8192

static const char *legacy_cookie_get(char *buffer, const char *cookie_base,
                                     int param) {
  const char *limit;

  while (*cookie_base == '\n')
    cookie_base++;
  limit = strchr(cookie_base, '\n');
  if (!limit)
    limit = cookie_base + strlen(cookie_base);
  if (limit) {
    if (param) {
      int i;

      for (i = 0; i < param; i++) {
        if (cookie_base) {
          cookie_base = strchr(cookie_base, '\t');
          if (cookie_base)
            cookie_base++;
        }
      }
    }
    if (cookie_base) {
      if (cookie_base < limit) {
        const char *a = cookie_base;
        htsbuff b = htsbuff_ptr(buffer, COOKIEDIFF_SIZE);

        while ((*a) && (*a != '\t') && (*a != '\n'))
          a++;
        htsbuff_catn(&b, cookie_base, (size_t) (a - cookie_base));
        return buffer;
      } else
        return "";
    } else
      return "";
  } else
    return "";
}

/* Both getters run on canary-filled buffers, which must come out equal. */
static void cookiediff_one(parsediff_stats *st, const char *line, char *o,
                           char *n) {
  int param;

  for (param = -2; param <= 8; param++) {
    const char *ro, *rn;
    char arg[16];

    memset(o, 'O', COOKIEDIFF_SIZE);
    memset(n, 'O', COOKIEDIFF_SIZE);
    st->cases++;
    ro = legacy_cookie_get(o, line, param);
    rn = cookie_get(n, line, param);
    if ((ro == o) != (rn == n) || memcmp(o, n, COOKIEDIFF_SIZE) != 0) {
      snprintf(arg, sizeof(arg), "%d", param);
      parsediff_fail(st, "cookie_get", line, arg, ro, rn);
    }
  }
}

static int st_cookiespandiff(httrackp *opt, int argc, char **argv) {
  static const char *const hand[] = {"",
                                     "\n",
                                     "\n\n",
                                     "a",
                                     "\t",
                                     "\t\t",
                                     "a\t",
                                     "\ta",
                                     "a\tb\tc",
                                     "a\t\tc",
                                     "a\tb\t",
                                     "a\tb\nc\td",
                                     "a\nb\tc\td",
                                     "\na\tb",
                                     "\n\na\tb\n",
                                     "a\tb\n",
                                     "a\r\n\tb",
                                     "a\t\n\tb",
                                     "\t\n\t",
                                     "a b\tc d",
                                     "h\tTRUE\t/\tFALSE\t0\tn\tv",
                                     "h\tTRUE\t/\tFALSE\t0\tn\tv\n",
                                     NULL};
  static const char *const alphabet[] = {"a", "\t",   "\n", "\r", "bc",
                                         " ", "\t\t", "/",  "x"};
  /* A field one byte under the 8192-byte buffer still fits. */
  static const size_t lengths[] = {8190, 8191};
  parsediff_stats st = {0, 0};
  char *o = malloct(COOKIEDIFF_SIZE);
  char *n = malloct(COOKIEDIFF_SIZE);
  uint32_t seed = 547;
  size_t i;
  int r;

  (void) opt;
  (void) argc;
  (void) argv;
  assertf(o != NULL && n != NULL);
  for (i = 0; hand[i] != NULL; i++)
    cookiediff_one(&st, hand[i], o, n);
  for (i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
    char *run = parsediff_run('x', lengths[i]);
    String line = STRING_EMPTY;

    cookiediff_one(&st, run, o, n);
    StringCat(line, "a\t");
    StringCat(line, run);
    StringCat(line, "\tb\nc");
    cookiediff_one(&st, StringBuff(line), o, n);
    StringFree(line);
    freet(run);
  }
  for (r = 0; r < 4000; r++) {
    String line = STRING_EMPTY;

    parsediff_random(&line, &seed, alphabet,
                     sizeof(alphabet) / sizeof(alphabet[0]), 16);
    cookiediff_one(&st, StringBuff(line) != NULL ? StringBuff(line) : "", o, n);
    StringFree(line);
  }
  freet(o);
  freet(n);
  printf("cookiespandiff: %lu cases, %lu failures\n", st.cases, st.failures);
  if (st.failures == 0)
    printf("cookiespandiff: OK\n");
  return st.failures != 0;
}

/* cookiequote HEADER: what the jar keeps of HEADER and what it then sends
   back, with the record's tabs shown as '|'. Both, because RFC 6265 5.4 sends
   a value as stored and only the stored side shows a cookie-av's own bytes. */
static int st_cookiequote(httrackp *opt, int argc, char **argv) {
  static t_cookie cookie;
  char line[512];
  char hdr[1024];
  htsblk r;
  size_t i, n;

  (void) opt;
  memset(&r, 0, sizeof(r));
  cookie.max_len = sizeof(cookie.data);
  cookie.data[0] = '\0';
  line[0] = '\0';
  strlncatbuff(line, argc >= 1 ? argv[0] : "", sizeof(line), sizeof(line) - 1);
  treathead(&cookie, "example.com", "/", &r, line);
  n = strlen(cookie.data);
  if (n != 0 && cookie.data[n - 1] == '\n')
    n--;
  printf("stored=");
  for (i = 0; i < n; i++)
    putchar(cookie.data[i] == '\t' ? '|' : cookie.data[i]);
  printf("\n");
  http_cookie_header(&cookie, "example.com", "/", hdr, sizeof(hdr));
  printf("sent=%s\n", hdr);
  return 0;
}

/* Is NAME=VALUE a whole pair of the Cookie line HDR, rather than the tail
   of a longer one? */
static hts_boolean cookie_sent(const char *hdr, const char *pair) {
  const size_t len = strlen(pair);
  const char *p;

  for (p = strstr(hdr, pair); p != NULL; p = strstr(p + 1, pair)) {
    if (p > hdr && p[-1] == ' ' &&
        (p[len] == ';' || p[len] == '\r' || p[len] == '\0'))
      return HTS_TRUE;
  }
  return HTS_FALSE;
}

/* RFC 6265 5.3: a Domain= the request host may not claim drops the cookie,
   and an expired Max-Age or Expires erases the stored one. */
static int st_cookiesetscope(httrackp *opt, int argc, char **argv) {
  static const char LABEL[] = "cookie-setscope";
#define PAST "Expires=Thu, 01 Jan 1970 00:00:00 GMT"
#define FUTURE "Expires=Fri, 01 Jan 2100 00:00:00 GMT"

  static const struct {
    const char *host, *header; // header NULL: only query
    const char *query, *pair;
    hts_boolean want;
  } steps[] = {
      // header (NULL: none) sent by host, then is pair sent to query?
      // a foreign Domain= is refused, and not stored for its sender either
      {"evil.test", "Set-Cookie: x=1; Domain=victim.test", "victim.test", "x=1",
       HTS_FALSE},
      {"evil.test", NULL, "evil.test", "x=1", HTS_FALSE},
      {"a.b.test", "Set-Cookie: p=1; Domain=b.test", "c.b.test", "p=1",
       HTS_TRUE},
      {"a.b.test", "Set-Cookie: pd=1; Domain=.B.Test", "c.b.test", "pd=1",
       HTS_TRUE},
      {"b.test", "Set-Cookie: child=1; Domain=a.b.test", "a.b.test", "child=1",
       HTS_FALSE},
      {"wwwexample.test", "Set-Cookie: lab=1; Domain=example.test",
       "example.test", "lab=1", HTS_FALSE},
      // a public suffix reaches every host under it
      {"a.foo.com", "Set-Cookie: ps=1; Domain=com", "b.bar.com", "ps=1",
       HTS_FALSE},
      {"a.foo.com", NULL, "a.foo.com", "ps=1", HTS_FALSE},
      {"a.b.co.uk", "Set-Cookie: uk=1; Domain=co.uk", "c.d.co.uk", "uk=1",
       HTS_FALSE},
      {"a.b.co.uk", "Set-Cookie: bk=1; Domain=b.co.uk", "c.b.co.uk", "bk=1",
       HTS_TRUE},
      {"a.b.co.uk", "Set-Cookie: ucu=1; Domain=CO.UK", "c.d.co.uk", "ucu=1",
       HTS_FALSE},
      // a public suffix may still name itself
      {"localhost:8080", "Set-Cookie: lh=1; Domain=localhost", "localhost",
       "lh=1", HTS_TRUE},
      // an address only ever names itself
      {"127.0.0.1", "Set-Cookie: ie=1; Domain=127.0.0.1", "127.0.0.1", "ie=1",
       HTS_TRUE},
      {"192.168.1.1", "Set-Cookie: ip=1; Domain=1.1", "192.168.1.1", "ip=1",
       HTS_FALSE},
      // no Domain= stays on its host
      {"del.test", "Set-Cookie: y=1", "del.test", "y=1", HTS_TRUE},
      // an empty or "." Domain=, and an empty Path=, are ignored
      {"del.test", "Set-Cookie: ed=1; Domain=", "del.test", "ed=1", HTS_TRUE},
      {"del.test", "Set-Cookie: dot=1; Domain=.", "del.test", "dot=1",
       HTS_TRUE},
      {"del.test", "Set-Cookie: ep=1; Path=", "del.test", "ep=1", HTS_TRUE},
      // a pair is matched whole, not as the tail or head of a longer one
      {"sub.test", "Set-Cookie: az=10", "sub.test", "z=1", HTS_FALSE},
      // an unusable first pair voids the header, it never promotes the next
      {"sub.test",
       "Set-Cookie: "
       "nnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnn"
       "n"
       "nnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnn"
       "n"
       "nnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnn"
       "n"
       "=1; Path=/; Max-Age=99",
       "sub.test", "Path=/", HTS_FALSE},
      {"sub.test", "Set-Cookie: =1; Path=/", "sub.test", "Path=/", HTS_FALSE},
      // the first pair is the cookie whatever its name
      {"sub.test", "Set-Cookie: Domain=x; a=1", "sub.test", "Domain=x",
       HTS_TRUE},
      {"sub.test", NULL, "sub.test", "a=1", HTS_FALSE},
      // an attribute after the first pair is never a cookie
      {"sub.test", "Set-Cookie: ss=1; SameSite=Lax", "sub.test", "SameSite=Lax",
       HTS_FALSE},
      {"sub.test", "Set-Cookie: pr=1; Priority=High", "sub.test",
       "Priority=High", HTS_FALSE},
      {"sub.test", "Set-Cookie: pt=1; Partitioned; Unknown=1", "sub.test",
       "Unknown=1", HTS_FALSE},
      {"sub.test", NULL, "sub.test", "pt=1", HTS_TRUE},
      // an expired cookie erases the stored one, an empty value included
      {"del.test", "Set-Cookie: z=1", "del.test", "z=1", HTS_TRUE},
      {"del.test", "Set-Cookie: z=gone; Max-Age=0", "del.test", "z=1",
       HTS_FALSE},
      {"del.test", NULL, "del.test", "z=gone", HTS_FALSE},
      {"del.test", "Set-Cookie: q=1", "del.test", "q=1", HTS_TRUE},
      {"del.test", "Set-Cookie: q=; Max-Age=0", "del.test", "q=1", HTS_FALSE},
      {"del.test", NULL, "del.test", "q=", HTS_FALSE},
      {"del.test", "Set-Cookie: n=1", "del.test", "n=1", HTS_TRUE},
      {"del.test", "Set-Cookie: n=1; Max-Age=-5", "del.test", "n=1", HTS_FALSE},
      {"del.test", "Set-Cookie: e=1", "del.test", "e=1", HTS_TRUE},
      {"del.test", "Set-Cookie: e=gone; " PAST, "del.test", "e=gone",
       HTS_FALSE},
      {"del.test", NULL, "del.test", "e=1", HTS_FALSE},
      {"del.test", "Set-Cookie: f=1; " FUTURE, "del.test", "f=1", HTS_TRUE},
      // Max-Age outranks Expires, either way round
      {"del.test", "Set-Cookie: m=1; Max-Age=3600; " PAST, "del.test", "m=1",
       HTS_TRUE},
      {"del.test", "Set-Cookie: m=; " FUTURE "; Max-Age=0", "del.test", "m=1",
       HTS_FALSE},
      {"del.test", NULL, "del.test", "m=", HTS_FALSE},
      // a Max-Age that is no number is ignored
      {"del.test", "Set-Cookie: g=1; Max-Age=soon", "del.test", "g=1",
       HTS_TRUE},
      // an empty value with no expiry replaces the old one
      {"del.test", "Set-Cookie: g=", "del.test", "g=1", HTS_FALSE},
      {"del.test", NULL, "del.test", "g=", HTS_TRUE},
      // a two-digit year below 70 is 20xx (RFC 6265 5.1.1)
      {"del.test", "Set-Cookie: y60=1; Expires=Sun, 01-Jan-60 00:00:00 GMT",
       "del.test", "y60=1", HTS_TRUE},
      {"del.test", "Set-Cookie: y69=1; Expires=Tue, 01-Jan-69 00:00:00 GMT",
       "del.test", "y69=1", HTS_TRUE},
      {"del.test", "Set-Cookie: y50=1; Expires=Sat, 01-Jan-50 00:00:00 GMT",
       "del.test", "y50=1", HTS_TRUE},
      {"del.test", "Set-Cookie: y70=1", "del.test", "y70=1", HTS_TRUE},
      {"del.test", "Set-Cookie: y70=1; Expires=Thu, 01-Jan-70 00:00:00 GMT",
       "del.test", "y70=1", HTS_FALSE},
      // a later invalid duplicate is ignored, not a reset
      {"del.test", "Set-Cookie: dm=1", "del.test", "dm=1", HTS_TRUE},
      {"del.test", "Set-Cookie: dm=1; Max-Age=0; Max-Age=soon", "del.test",
       "dm=1", HTS_FALSE},
      {"del.test", "Set-Cookie: de=1", "del.test", "de=1", HTS_TRUE},
      {"del.test", "Set-Cookie: de=1; " PAST "; Expires=garbage", "del.test",
       "de=1", HTS_FALSE},
      // a value's trailing whitespace is trimmed
      {"a.b.test", "Set-Cookie: tw=1; Domain=b.test ; Path=/", "c.b.test",
       "tw=1", HTS_TRUE},
      {"del.test", "Set-Cookie: mt=1", "del.test", "mt=1", HTS_TRUE},
      {"del.test", "Set-Cookie: mt=1; Max-Age=0 ", "del.test", "mt=1",
       HTS_FALSE},
      {"del.test", "Set-Cookie: ms=1", "del.test", "ms=1", HTS_TRUE},
      {"del.test", "Set-Cookie: ms=1; Max-Age=0 ; Path=/", "del.test", "ms=1",
       HTS_FALSE},
      // rotation still replaces
      {"del.test", "Set-Cookie: w=1", "del.test", "w=1", HTS_TRUE},
      {"del.test", "Set-Cookie: w=2", "del.test", "w=1", HTS_FALSE},
      {"del.test", NULL, "del.test", "w=2", HTS_TRUE},
      // a Domain= erase reaches a Domain= cookie
      {"www.del.test", "Set-Cookie: dd=1; Domain=del.test", "a.del.test",
       "dd=1", HTS_TRUE},
      {"www.del.test", "Set-Cookie: dd=; Domain=del.test; Max-Age=0",
       "a.del.test", "dd=1", HTS_FALSE},
      {"www.del.test", NULL, "a.del.test", "dd=", HTS_FALSE},
      // a host cannot erase another host's cookie either
      {"victim.test", "Set-Cookie: v=1", "victim.test", "v=1", HTS_TRUE},
      {"evil.test", "Set-Cookie: v=; Domain=victim.test; Max-Age=0",
       "victim.test", "v=1", HTS_TRUE},
  };

  static t_cookie jar;
  char hdr[4096];
  size_t i;
  int err = 0;

  (void) opt;
  (void) argc;
  (void) argv;
  jar.max_len = sizeof(jar.data);
  jar.data[0] = '\0';
  for (i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
    if (steps[i].header != NULL) {
      htsblk r;
      char line[256]; // treathead NUL-cuts the header in place

      memset(&r, 0, sizeof(r));
      strcpybuff(line, steps[i].header);
      treathead(&jar, steps[i].host, "/", &r, line);
    }
    http_cookie_header(&jar, steps[i].query, "/", hdr, sizeof(hdr));
    if (cookie_sent(hdr, steps[i].pair) != steps[i].want) {
      printf("%s: step %d: %s from %s, %s %s sent to %s\n", LABEL, (int) i,
             steps[i].header != NULL ? steps[i].header : "(query)",
             steps[i].host, steps[i].pair, steps[i].want ? "not" : "wrongly",
             steps[i].query);
      err = 1;
    }
  }
  // the jar file keeps "/" for an empty Path=, as a browser export does
  {
    char *const ep = cookie_find(jar.data, "ep", "del.test", "/");
    char field[8192]; // cookie_get's buffer size

    if (ep == NULL || strcmp(cookie_get(field, ep, 2), "/") != 0) {
      printf("%s: an empty Path= is not stored as /\n", LABEL);
      err = 1;
    }
  }
  printf("%s: %s\n", LABEL, err ? "FAIL" : "OK");
  return err;
#undef FUTURE
#undef PAST
}

/* -#test=cookiehttponly <dir>: a "#HttpOnly_" jar line is a record, and any
   other '#' line, even one shaped like a record, stays a comment. */
static int st_cookiehttponly(httrackp *opt, int argc, char **argv) {
  static t_cookie ck;
  char fpath[HTS_URLMAXSIZE * 2];
  char file[HTS_URLMAXSIZE * 2];
  char hdr[1024];
  FILE *fp;

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "cookiehttponly: needs a writable dir\n");
    return 1;
  }
  assertf(sprintfbuff(fpath, "%s/", argv[0]));
  assertf(sprintfbuff(file, "%scookies.txt", fpath));
  fp = FOPEN(file, "wb");
  assertf(fp != NULL);
  fprintf(fp, "# Netscape HTTP Cookie File\n"
              "#HttpOnly_.example.com\tTRUE\t/\tFALSE\t0\tsid\tabc\n"
              "#.example.com\tTRUE\t/\tFALSE\t0\tdead\tx\n"
              "#HttpOnly_#.example.com\tTRUE\t/\tFALSE\t0\tdead2\tx\n"
              "#httponly_.example.com\tTRUE\t/\tFALSE\t0\tdead3\tx\n"
              "#HttpOnlyX.example.com\tTRUE\t/\tFALSE\t0\tdead4\tx\n"
              ".example.com\tTRUE\t/\tFALSE\t0\tplain\t1\n"
              "#HttpOnly_.example.com\tTRUE\t/\tFALSE\t0\tsid2\tdef\n");
  fclose(fp);
  ck.max_len = sizeof(ck.data);
  ck.data[0] = '\0';
  // the four dead lines are comments a rewrite would drop
  assertf(cookie_load(NULL, &ck, fpath, "cookies.txt") == 4);
  (void) UNLINK(file);
  // each one counts on its own, so no two can trade places
  {
    static const char *const dead[] = {
        "#.example.com\tTRUE\t/\tFALSE\t0\tdead\tx\n",
        "#HttpOnly_#.example.com\tTRUE\t/\tFALSE\t0\tdead2\tx\n",
        "#httponly_.example.com\tTRUE\t/\tFALSE\t0\tdead3\tx\n",
        "#HttpOnlyX.example.com\tTRUE\t/\tFALSE\t0\tdead4\tx\n",
    };
    static t_cookie one;
    size_t i;

    one.max_len = sizeof(one.data);
    for (i = 0; i < sizeof(dead) / sizeof(dead[0]); i++) {
      fp = FOPEN(file, "wb");
      assertf(fp != NULL);
      fprintf(fp, "%s.example.com\tTRUE\t/\tFALSE\t0\tplain\t1\n", dead[i]);
      fclose(fp);
      one.data[0] = '\0';
      if (cookie_load(NULL, &one, fpath, "cookies.txt") != 1 ||
          strstr(one.data, "dead") != NULL ||
          strstr(one.data, "\tplain\t1") == NULL) {
        fprintf(stderr, "cookiehttponly: %s loaded as [%s]\n", dead[i],
                one.data);
        return 1;
      }
      (void) UNLINK(file);
    }
  }
  http_cookie_header(&ck, "www.example.com", "/", hdr, sizeof(hdr));
  if (strstr(hdr, "sid=abc") == NULL || strstr(hdr, "plain=1") == NULL ||
      strstr(hdr, "sid2=def") == NULL || strstr(hdr, "dead") != NULL ||
      strchr(ck.data, '#') != NULL) {
    fprintf(stderr, "cookiehttponly: sent [%s], jar [%s]\n", hdr, ck.data);
    return 1;
  }
  printf("cookiehttponly: OK\n");
  return 0;
}

/* This holds opt's log and level while a block captures its warnings. */
typedef struct cookie_logcap {
  FILE *saved, *log;
  int debug;
} cookie_logcap;

/* Send opt's warnings to a temporary file until cookie_log_stop. */
static void cookie_log_start(httrackp *opt, cookie_logcap *cap) {
  cap->saved = opt->log;
  cap->debug = opt->debug;
  cap->log = tmpfile();
  assertf(cap->log != NULL);
  opt->log = cap->log;
  opt->debug = LOG_WARNING;
}

/* Restore opt's log, and copy what was captured into buf. */
static void cookie_log_stop(httrackp *opt, cookie_logcap *cap, char *buf,
                            size_t size) {
  size_t n;

  rewind(cap->log);
  n = fread(buf, 1, size - 1, cap->log);
  buf[n] = '\0';
  fclose(cap->log);
  opt->log = cap->saved;
  opt->debug = cap->debug;
}

/* Read the whole file at path into buf, or "" when it cannot be opened. */
static void cookiejar_slurp(const char *path, char *buf, size_t size) {
  FILE *const fp = FOPEN(path, "rb");
  size_t n = 0;

  if (fp != NULL) {
    n = fread(buf, 1, size - 1, fp);
    fclose(fp);
  }
  buf[n] = '\0';
}

/* -#test=cookiejar <dir>: the jar keeps every cookie attribute. */
static int st_cookiejar(httrackp *opt, int argc, char **argv) {
#define LABEL "cookie-jar"

  static const struct {
    const char *from, *header; // header NULL: only query
    const char *to, *path, *pair;
    hts_boolean want;
  } steps[] = {
      // header (NULL: none) sent by from, then is pair sent to to+path?
      // no Domain= is host-only, a Domain= reaches subdomains
      {"jar.test", "Set-Cookie: h=1", "jar.test", "/", "h=1", HTS_TRUE},
      {"jar.test", NULL, "www.jar.test", "/", "h=1", HTS_FALSE},
      {"jar.test", "Set-Cookie: d=1; Domain=jar.test", "www.jar.test", "/",
       "d=1", HTS_TRUE},
      // a longer path is another cookie, and its erasure leaves "/" alone
      {"jar.test", "Set-Cookie: h=2; Path=/a", "jar.test", "/", "h=1",
       HTS_TRUE},
      {"jar.test", NULL, "jar.test", "/", "h=2", HTS_FALSE},
      {"jar.test", NULL, "jar.test", "/a/x", "h=2", HTS_TRUE},
      {"jar.test", "Set-Cookie: h=; Path=/a; Max-Age=0", "jar.test", "/a/x",
       "h=1", HTS_TRUE},
      {"jar.test", NULL, "jar.test", "/a/x", "h=2", HTS_FALSE},
      // an erasure from another host cannot reach it
      {"www.jar.test", "Set-Cookie: d=; Max-Age=0", "jar.test", "/", "d=1",
       HTS_TRUE},
      // host-only is no part of the identity, so either form erases the other
      {"jar.test", "Set-Cookie: d=; Max-Age=0", "www.jar.test", "/", "d=1",
       HTS_FALSE},
      {"jar.test", "Set-Cookie: d=1; Domain=jar.test", "www.jar.test", "/",
       "d=1", HTS_TRUE},
      {"www.jar.test", "Set-Cookie: d=; Domain=jar.test; Max-Age=0", "jar.test",
       "/", "d=1", HTS_FALSE},
      // and either form replaces the other, taking its scope
      {"jar.test", "Set-Cookie: two=host", "jar.test", "/", "two=host",
       HTS_TRUE},
      {"jar.test", "Set-Cookie: two=dom; Domain=jar.test", "jar.test", "/",
       "two=host", HTS_FALSE},
      {"jar.test", NULL, "www.jar.test", "/", "two=dom", HTS_TRUE},
      {"jar.test", "Set-Cookie: two=back", "www.jar.test", "/", "two=dom",
       HTS_FALSE},
      {"jar.test", NULL, "www.jar.test", "/", "two=back", HTS_FALSE},
      {"jar.test", NULL, "jar.test", "/", "two=back", HTS_TRUE},
      // a dotted Domain= and an undotted one name the same cookie
      {"www.dot.test", "Set-Cookie: dt=1; Domain=.dot.test", "a.dot.test", "/",
       "dt=1", HTS_TRUE},
      {"www.dot.test", "Set-Cookie: dt=2; Domain=dot.test", "a.dot.test", "/",
       "dt=1", HTS_FALSE},
      {"www.dot.test", NULL, "a.dot.test", "/", "dt=2", HTS_TRUE},
      {"www.dot.test", "Set-Cookie: dt=3; Domain=.dot.test", "a.dot.test", "/",
       "dt=2", HTS_FALSE},
      {"www.dot.test", NULL, "a.dot.test", "/", "dt=3", HTS_TRUE},
      // Secure goes out over https only, as an attribute or as av=value
      {"https://jar.test", "Set-Cookie: s=1; Secure", "jar.test", "/", "s=1",
       HTS_FALSE},
      {"https://jar.test", NULL, "http://jar.test", "/", "s=1", HTS_FALSE},
      {"https://jar.test", NULL, "https://jar.test", "/", "s=1", HTS_TRUE},
      {"https://jar.test", "Set-Cookie: s2=1; SECURE=yes", "http://jar.test",
       "/", "s2=1", HTS_FALSE},
      {"https://jar.test", NULL, "https://jar.test", "/", "s2=1", HTS_TRUE},
      // a cookie named like an attribute is still a cookie
      {"jar.test", "Set-Cookie: secure=1", "jar.test", "/", "secure=1",
       HTS_TRUE},
      // an attribute that only starts with Secure is not Secure
      {"jar.test", "Set-Cookie: sx=1; Securex", "http://jar.test", "/", "sx=1",
       HTS_TRUE},
  };

  static t_cookie jar;
  const int64_t now = (int64_t) time(NULL);
  char buffer[8192];
  char BIGSTK hdr[8192];
  char fpath[HTS_URLMAXSIZE * 2];
  char file[HTS_URLMAXSIZE * 2];
  char saved[HTS_URLMAXSIZE * 2];
  char BIGSTK text[16384];
  size_t i;
  int err = 0;

  if (argc < 1) {
    fprintf(stderr, "cookiejar: needs a writable dir\n");
    return 1;
  }
  jar.max_len = sizeof(jar.data);
  jar.data[0] = '\0';
  for (i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
    if (steps[i].header != NULL) {
      htsblk r;
      char line[256]; // treathead NUL-cuts the header in place

      memset(&r, 0, sizeof(r));
      strcpybuff(line, steps[i].header);
      treathead(&jar, steps[i].from, "/", &r, line);
    }
    http_cookie_header(&jar, steps[i].to, steps[i].path, hdr, sizeof(hdr));
    if (cookie_sent(hdr, steps[i].pair) != steps[i].want) {
      printf("%s: step %d: %s from %s, %s %s sent to %s%s\n", LABEL, (int) i,
             steps[i].header != NULL ? steps[i].header : "(query)",
             steps[i].from, steps[i].pair, steps[i].want ? "not" : "wrongly",
             steps[i].to, steps[i].path);
      err = 1;
    }
  }

  /* Expiry is absolute: Max-Age outranks Expires, a session is 0, and a
     Set-Cookie lives 400 days at most. */
  {
    static const struct {
      const char *header, *name;
      int64_t low, high; // from now
    } lives[] = {
        {"Set-Cookie: ma=1; Expires=Fri, 01 Jan 2100 00:00:00 GMT; Max-Age=60",
         "ma", 60, 62},
        {"Set-Cookie: ex=1; Expires=Fri, 01 Jan 2100 00:00:00 GMT", "ex",
         400 * 86400, 400 * 86400 + 2},
        {"Set-Cookie: big=1; Max-Age=99999999999999999999", "big", 400 * 86400,
         400 * 86400 + 2},
        // 2^63 and 2^64 + 1, which an unguarded int64 wraps to < 0 and to 1
        {"Set-Cookie: b63=1; Max-Age=9223372036854775808", "b63", 400 * 86400,
         400 * 86400 + 2},
        {"Set-Cookie: b64=1; Max-Age=18446744073709551617", "b64", 400 * 86400,
         400 * 86400 + 2},
    };

    jar.data[0] = '\0';
    for (i = 0; i < sizeof(lives) / sizeof(lives[0]); i++) {
      htsblk r;
      char line[256];
      const char *rec;
      int64_t when;

      memset(&r, 0, sizeof(r));
      strcpybuff(line, lives[i].header);
      treathead(&jar, "jar.test", "/", &r, line);
      rec = cookie_find(jar.data, lives[i].name, "jar.test", "/");
      when = rec != NULL ? (int64_t) atoll(cookie_get(buffer, rec, 4)) : 0;
      if (when < now + lives[i].low || when > now + lives[i].high) {
        printf("%s: %s stored expiry " LLintP ", now " LLintP "\n", LABEL,
               lives[i].header, (LLint) when, (LLint) now);
        err = 1;
      }
    }
    // exact dates, the first two past a 32-bit time_t
    {
      static const struct {
        const char *date;
        int64_t unix_time;
      } dates[] = {
          {"Fri, 01 Jan 2100 00:00:00 GMT", 4102444800LL},
          {"Tue, 19 Jan 2038 03:14:08 GMT", 2147483648LL},
          {"Sun, 06 Nov 1994 08:49:37 GMT", 784111777LL},
          {"Wed, 01 Mar 2000 00:00:00 GMT", 951868800LL},
          {"Tue, 29 Feb 2000 23:59:59 GMT", 951868799LL},
          {"Wed, 15 Jul 2026 12:00:00 GMT", 1784116800LL},
          {"Thu, 31 Dec 2026 23:59:59 GMT", 1798761599LL},
      };

      for (i = 0; i < sizeof(dates) / sizeof(dates[0]); i++) {
        int64_t when = 0;

        if (!cookie_expires_time(dates[i].date, dates[i].unix_time - 10,
                                 &when) ||
            when != dates[i].unix_time) {
          printf("%s: %s read as " LLintP "\n", LABEL, dates[i].date,
                 (LLint) when);
          err = 1;
        }
      }
    }
    // out-of-range fields are capped, not overflowed
    {
      static const char *const huge[] = {
          "Thu, 2147483647 Jan 2027 00:00:00 GMT",
          "2147483647 Dec 2147483647 2147483647:2147483647:2147483647",
      };

      for (i = 0; i < sizeof(huge) / sizeof(huge[0]); i++) {
        int64_t when = 0;

        if (!cookie_expires_time(huge[i], 0, &when) ||
            when != (int64_t) 400 * 24 * 3600) {
          printf("%s: %s read as " LLintP "\n", LABEL, huge[i], (LLint) when);
          err = 1;
        }
      }
    }
    // a session cookie writes 0
    {
      htsblk r;
      char line[] = "Set-Cookie: sess=1";
      const char *rec;

      memset(&r, 0, sizeof(r));
      treathead(&jar, "jar.test", "/", &r, line);
      rec = cookie_find(jar.data, "sess", "jar.test", "/");
      if (rec == NULL || strcmp(cookie_get(buffer, rec, 4), "0") != 0) {
        printf("%s: a session cookie is not stored with expiry 0\n", LABEL);
        err = 1;
      }
    }
    // a record that has expired since it was stored is not sent
    {
      cookie_attr attr = {.host_only = HTS_FALSE,
                          .secure = HTS_FALSE,
                          .http_only = HTS_FALSE,
                          .expires = now - 10};

      jar.data[0] = '\0';
      assertf(cookie_set(&jar, "old", "1", "jar.test", "/", &attr) == 0);
      attr.expires = now + 3600;
      assertf(cookie_set(&jar, "young", "1", "jar.test", "/", &attr) == 0);
      http_cookie_header(&jar, "jar.test", "/", hdr, sizeof(hdr));
      if (cookie_sent(hdr, "old=1") || !cookie_sent(hdr, "young=1")) {
        printf("%s: expired record at send: [%s]\n", LABEL, hdr);
        err = 1;
      }
    }
  }

  /* A cookies.txt keeps host-only, Secure, HttpOnly and expiry through a
     load and a save, and an expired line is not loaded. */
  assertf(sprintfbuff(fpath, "%s/", argv[0]));
  assertf(sprintfbuff(file, "%scookies.txt", fpath));
  assertf(sprintfbuff(saved, "%ssaved.txt", fpath));
  {
    static const char *const kept[] = {
        ".load.test\tTRUE\t/\tFALSE\t0\tdom\t1" LF,
        "host.test\tFALSE\t/\tFALSE\t0\tonly\t1" LF,
        ".load.test\tTRUE\t/\tTRUE\t0\tsec\t1" LF,
        ".load.test\tTRUE\t/\tFALSE\t4102444800\tfuture\t1" LF,
        "#HttpOnly_.load.test\tTRUE\t/\tFALSE\t0\thid\t1" LF,
    };

    static const struct {
      const char *to, *pair;
      hts_boolean want;
    } sends[] = {
        {"www.load.test", "dom=1", HTS_TRUE},
        {"www.load.test", "future=1", HTS_TRUE},
        {"www.load.test", "hid=1", HTS_TRUE},
        {"www.load.test", "past=1", HTS_FALSE},
        {"www.load.test", "sec=1", HTS_FALSE},
        {"https://www.load.test", "sec=1", HTS_TRUE},
        {"host.test", "only=1", HTS_TRUE},
        {"sub.host.test", "only=1", HTS_FALSE},
    };

    FILE *fp = FOPEN(file, "wb");

    assertf(fp != NULL);
    fprintf(fp, "# Netscape HTTP Cookie File\n"
                ".load.test\tTRUE\t/\tFALSE\t1\tpast\t1\n");
    for (i = 0; i < sizeof(kept) / sizeof(kept[0]); i++)
      fprintf(fp, "%s", kept[i]);
    fclose(fp);
    jar.data[0] = '\0';
    assertf(cookie_load(NULL, &jar, fpath, "cookies.txt") == 0);
    for (i = 0; i < sizeof(sends) / sizeof(sends[0]); i++) {
      http_cookie_header(&jar, sends[i].to, "/", hdr, sizeof(hdr));
      if (cookie_sent(hdr, sends[i].pair) != sends[i].want) {
        printf("%s: loaded %s %s sent to %s\n", LABEL, sends[i].pair,
               sends[i].want ? "not" : "wrongly", sends[i].to);
        err = 1;
      }
    }
    assertf(cookie_save(NULL, &jar, saved) == 0);
    cookiejar_slurp(saved, text, sizeof(text));
    for (i = 0; i < sizeof(kept) / sizeof(kept[0]); i++) {
      if (strstr(text, kept[i]) == NULL) {
        printf("%s: the saved jar lost %s", LABEL, kept[i]);
        err = 1;
      }
    }
    if (strstr(text, "past") != NULL || strstr(text, "\tHttpOnly") != NULL) {
      printf("%s: the saved jar holds [%s]\n", LABEL, text);
      err = 1;
    }
    /* A Set-Cookie HttpOnly saves the way a loaded one does, in either form,
       and the saved domain is dotted exactly when subdomains get it. */
    {
      static const struct {
        const char *header, *saved;
      } saves[] = {
          {"Set-Cookie: web=1; HttpOnly",
           "\n#HttpOnly_jar.test\tFALSE\t/\tFALSE\t0\tweb\t1" LF},
          {"Set-Cookie: hv=1; HttpOnly=yes",
           "\n#HttpOnly_jar.test\tFALSE\t/\tFALSE\t0\thv\t1" LF},
          {"Set-Cookie: dm=1; Domain=jar.test",
           "\n.jar.test\tTRUE\t/\tFALSE\t0\tdm\t1" LF},
      };

      jar.data[0] = '\0';
      for (i = 0; i < sizeof(saves) / sizeof(saves[0]); i++) {
        htsblk r;
        char line[256];

        memset(&r, 0, sizeof(r));
        strcpybuff(line, saves[i].header);
        treathead(&jar, "jar.test", "/", &r, line);
      }
      // a dotted host-only record, from a hand-edited jar, saves undotted
      {
        cookie_attr attr = {.host_only = HTS_TRUE,
                            .secure = HTS_FALSE,
                            .http_only = HTS_FALSE,
                            .expires = 0};

        assertf(cookie_set(&jar, "hd", "1", ".jar.test", "/", &attr) == 0);
      }
      assertf(cookie_save(NULL, &jar, saved) == 0);
      cookiejar_slurp(saved, text, sizeof(text));
      for (i = 0; i < sizeof(saves) / sizeof(saves[0]); i++) {
        if (strstr(text, saves[i].saved) == NULL) {
          printf("%s: saved %s as [%s]\n", LABEL, saves[i].header, text);
          err = 1;
        }
      }
      if (strncmp(text, "# Netscape HTTP Cookie File" LF,
                  sizeof("# Netscape HTTP Cookie File" LF) - 1) != 0 ||
          strstr(text, "\njar.test\tFALSE\t/\tFALSE\t0\thd\t1" LF) == NULL) {
        printf("%s: the saved jar is [%s]\n", LABEL, text);
        err = 1;
      }
    }
    (void) UNLINK(file);
    (void) UNLINK(saved);
  }

  /* A cookie of RFC 6265's 4096 bytes loads, saves and goes out whole, and
     one byte more is refused with a warning rather than cut. */
  {
    cookie_logcap cap;
    char BIGSTK big[COOKIE_MAX_PAIR + 2];
    char BIGSTK pair[COOKIE_MAX_PAIR + 8];
    char logged[1024];
    FILE *fp = FOPEN(file, "wb");

    assertf(fp != NULL);
    memset(big, 'v', COOKIE_MAX_PAIR - 3);
    big[COOKIE_MAX_PAIR - 3] = '\0';
    fprintf(fp, "big.test\tFALSE\t/\tFALSE\t0\tfit\t%s\n", big);
    strcatbuff(big, "v"); // name and value now one byte over
    fprintf(fp, "big.test\tFALSE\t/\tFALSE\t0\tnot\t%s\n", big);
    fclose(fp);
    jar.data[0] = '\0';
    cookie_log_start(opt, &cap);
    assertf(cookie_load(opt, &jar, fpath, "cookies.txt") == 1);
    cookie_log_stop(opt, &cap, logged, sizeof(logged));
    big[COOKIE_MAX_PAIR - 3] = '\0';
    assertf(sprintfbuff(pair, "fit=%s", big));
    http_cookie_header(&jar, "big.test", "/", hdr, sizeof(hdr));
    if (!cookie_sent(hdr, pair) || strstr(jar.data, "\tnot\t") != NULL ||
        strstr(logged, "did not store 'not'") == NULL) {
      printf("%s: a 4096-byte cookie: sent %d bytes, log [%s]\n", LABEL,
             (int) strlen(hdr), logged);
      err = 1;
    }
    assertf(cookie_save(NULL, &jar, saved) == 0);
    cookiejar_slurp(saved, text, sizeof(text));
    if (strstr(text, big) == NULL || strstr(text, "\tnot\t") != NULL) {
      printf("%s: the saved jar lost the 4096-byte cookie\n", LABEL);
      err = 1;
    }
    (void) UNLINK(file);
    (void) UNLINK(saved);
  }

  /* A Set-Cookie the jar refuses is logged, once, and a stored one is not. */
  {
    cookie_logcap cap;
    char logged[1024];
    const char *p;
    int count = 0;
    htsblk r;
    char fits[] = "Set-Cookie: fits=1";
    char full[] = "Set-Cookie: full=1";

    cookie_log_start(opt, &cap);
    jar.data[0] = '\0';
    memset(&r, 0, sizeof(r));
    jar.max_len = 300; // room for one record and its headroom, not two
    treathead_opt(opt, &jar, "jar.test", "/", &r, fits);
    treathead_opt(opt, &jar, "jar.test", "/", &r, full);
    jar.max_len = sizeof(jar.data);
    cookie_log_stop(opt, &cap, logged, sizeof(logged));
    for (p = strstr(logged, "did not store"); p != NULL;
         p = strstr(p + 1, "did not store"))
      count++;
    if (count != 1 || strstr(logged, "'full'") == NULL ||
        strstr(jar.data, "fits") == NULL) {
      printf("%s: refusal log [%s], jar [%s]\n", LABEL, logged, jar.data);
      err = 1;
    }
  }
  /* An older build wrote TRUE in field 1 of every record, so a host-only
     Set-Cookie must still replace and erase a record loaded from its jar. */
  {
    static const struct {
      const char *header, *pair;
      hts_boolean want;
    } legacy[] = {
        {NULL, "SID=old", HTS_TRUE},
        {"Set-Cookie: SID=new", "SID=new", HTS_TRUE},
        {NULL, "SID=old", HTS_FALSE},
        {"Set-Cookie: SID=; Max-Age=0", "SID=new", HTS_FALSE},
        {NULL, "SID=", HTS_FALSE},
    };

    FILE *fp = FOPEN(file, "wb");

    assertf(fp != NULL);
    fprintf(fp, "127.0.0.1\tTRUE\t/\tFALSE\t1999999999\tSID\told\n");
    fclose(fp);
    jar.data[0] = '\0';
    assertf(cookie_load(NULL, &jar, fpath, "cookies.txt") == 0);
    for (i = 0; i < sizeof(legacy) / sizeof(legacy[0]); i++) {
      if (legacy[i].header != NULL) {
        htsblk r;
        char line[256];

        memset(&r, 0, sizeof(r));
        strcpybuff(line, legacy[i].header);
        treathead(&jar, "127.0.0.1", "/", &r, line);
      }
      http_cookie_header(&jar, "127.0.0.1", "/", hdr, sizeof(hdr));
      if (cookie_sent(hdr, legacy[i].pair) != legacy[i].want) {
        printf("%s: legacy jar row %d sent [%s]\n", LABEL, (int) i, hdr);
        err = 1;
      }
    }
    if (strstr(jar.data, "SID") != NULL) {
      printf("%s: the legacy jar kept [%s]\n", LABEL, jar.data);
      err = 1;
    }
    (void) UNLINK(file);
  }

  /* A line short of seven fields or with no name is skipped and counted, and a
     comment, a blank line or an expired cookie is not. */
  {
    FILE *fp = FOPEN(file, "wb");
    int skipped;

    assertf(fp != NULL);
    fprintf(fp, "# Netscape HTTP Cookie File\n"
                "\n"
                "short.test\tTRUE\t/\n"
                "short.test\tTRUE\t/\tFALSE\t0\t\tnoname\n"
                "short.test\tTRUE\t/\tFALSE\t1\tpast\t1\n"
                "short.test\tTRUE\t/\tFALSE\t0\tgood\t1\n");
    fclose(fp);
    jar.data[0] = '\0';
    skipped = cookie_load(NULL, &jar, fpath, "cookies.txt");
    if (skipped != 2 || strstr(jar.data, "\tgood\t1") == NULL ||
        strstr(jar.data, "noname") != NULL) {
      printf("%s: malformed lines: %d skipped, jar [%s]\n", LABEL, skipped,
             jar.data);
      err = 1;
    }
    (void) UNLINK(file);
  }

  /* A TAB or a line break in any field would forge a field or a record. */
  {
    htsblk r;
    char line[] = "Set-Cookie: tab=a\tb=c";
    char crlf[] = "Set-Cookie: nl=a\nb";

    memset(&r, 0, sizeof(r));
    jar.data[0] = '\0';
    treathead(&jar, "jar.test", "/", &r, line);
    treathead(&jar, "jar.test", "/", &r, crlf);
    if (cookie_set(&jar, "t\tn", "1", "jar.test", "/", NULL) == 0 ||
        cookie_set(&jar, "tn", "1", "jar.test", "/\tx", NULL) == 0 ||
        cookie_set(&jar, "tn", "1", "jar\t.test", "/", NULL) == 0 ||
        strnotempty(jar.data)) {
      printf("%s: a field with a TAB was stored: [%s]\n", LABEL, jar.data);
      err = 1;
    }
  }

  /* Each line a rewrite would lose is counted, and a line a known tool
     writes is not. */
  {
#define ROWS(text, skipped, rec, absent)                                       \
  {text, sizeof(text) - 1, skipped, rec, absent}
#define ROWR(text, skipped, rec) ROWS(text, skipped, rec, NULL)
#define ROWA(text, skipped, absent) ROWS(text, skipped, NULL, absent)
#define ROW(text, skipped) ROWS(text, skipped, NULL, NULL)

    static const struct {
      const char *bytes;
      size_t len;
      int skipped;
      const char *rec;    // NULL, or a record the jar must then hold
      const char *absent; // NULL, or a string the jar must not hold
    } loads[] = {
        // the same name, domain and path twice, whatever the host-only flag,
        // and the last one wins
        ROWS("d.test\tFALSE\t/\tFALSE\t0\tsid\t1\n"
             ".D.test\tTRUE\t/\tFALSE\t0\tsid\t2\n",
             1, "\tsid\t2\n", "\tsid\t1\n"),
        ROW("d.test\tTRUE\t/\tFALSE\t0\tsid\t1\n"
            "d.test\tTRUE\t/a\tFALSE\t0\tsid\t2\n"
            "d.test\tTRUE\t/\tFALSE\t0\tsid2\t3\n",
            0),
        // an expired twin is dropped on load, so a rewrite loses nothing
        ROW("d.test\tTRUE\t/\tFALSE\t0\tsid\t1\n"
            "d.test\tTRUE\t/\tFALSE\t1\tsid\t2\n",
            0),
        ROW("d.test\tTRUE\t/\tFALSE\t0\tn\tv1\tv2\n", 1),
        ROWA("x.test\tTRUE\t/\tFALSE\t0\tn\ta\tb\tc\n", 1, "\tn\t"),
        // an expiry a rewrite would turn into 0
        ROWA("d.test\tTRUE\t/\tFALSE\t12abc\tn\tv\n", 1, "\tn\t"),
        ROWA("d.test\tTRUE\t/\tFALSE\t1767225600.5\tn\tv\n", 1, "\tn\t"),
        // a UTF-8 BOM is skipped, before a header or a record
        ROWS("\xEF\xBB\xBF"
             "# Netscape HTTP Cookie File\nd.test\tTRUE\t/\tFALSE\t0\tn\tv\n",
             0, "d.test\tTRUE\t/\tFALSE\t0\tn\tv\n", "\xEF\xBB\xBF"),
        ROWS("\xEF\xBB\xBF"
             "d.test\tTRUE\t/\tFALSE\t0\tn\tv\n",
             0, "d.test\tTRUE\t/\tFALSE\t0\tn\tv\n", "\xEF\xBB\xBF"),
        // wget's own header
        ROWR("# HTTP cookie file.\n"
             "# Generated by Wget on 2026-10-19 12:00:00.\n"
             "# Edit at your own risk.\n\n"
             "d.test\tTRUE\t/\tFALSE\t0\tn\tv\n",
             0, "d.test\tTRUE\t/\tFALSE\t0\tn\tv\n"),
        // but a blank one is a session, which 0 also means
        ROWR("d.test\tTRUE\t/\tFALSE\t\tn\tv\n", 0, "\t0\tn\tv\n"),
        ROW("d.test\tTRUE\t/\tFALSE\t0\tn\tv\tHttpOnly\n", 0),
        ROW("d.test\tTRUE\t/\tFALSE\t0\tn\tv\0x\n", 1),
        // the jar refuses a CR in a name or a value, but not in a flag
        ROW("d.test\tTRUE\t/\tFALSE\r\t0\tn\tv\n", 1),
        // a CRLF ends the line, so it is no CR in the value
        ROWR("d.test\tTRUE\t/\tFALSE\t0\tn\tv\r\n", 0, "\tn\tv\n"),
        // a public suffix such as "com" loads host-only, so it reaches no other
        ROWR(".com\tTRUE\t/\tFALSE\t0\tsfx\t1\n", 0,
             ".com\tFALSE\t/\tFALSE\t0\tsfx\t1\n"),
        ROWR("CO.UK\tTRUE\t/\tFALSE\t0\tsfx\t1\n", 0,
             "co.uk\tFALSE\t/\tFALSE\t0\tsfx\t1\n"),
        ROWR("nas\tTRUE\t/\tFALSE\t0\tsfx\t1\n", 0,
             "nas\tFALSE\t/\tFALSE\t0\tsfx\t1\n"),
        ROWR("#HttpOnly_.com\tTRUE\t/\tFALSE\t0\tsfx\t1\n", 0,
             ".com\tFALSE\t/\tFALSE\t0\tsfx\t1\tHttpOnly\n"),
        ROWA(".com\tTRUE\t/\tFALSE\t1\tsfx\t1\n", 0, "sfx"),
        ROWR("com\tFALSE\t/\tFALSE\t0\tsfx\t1\n", 0,
             "com\tFALSE\t/\tFALSE\t0\tsfx\t1\n"),
        ROWR(".example.co.uk\tTRUE\t/\tFALSE\t0\tsfx\t1\n", 0,
             ".example.co.uk\tTRUE\t/\tFALSE\t0\tsfx\t1\n"),
        ROWR("example.com.\tTRUE\t/\tFALSE\t0\tsfx\t1\n", 0,
             "example.com.\tTRUE\t/\tFALSE\t0\tsfx\t1\n"),
        // a trailing-dot suffix stays host-only too
        ROWR("com.\tTRUE\t/\tFALSE\t0\tsfx\t1\n", 0,
             "com.\tFALSE\t/\tFALSE\t0\tsfx\t1\n"),
        ROWR("127.0.0.1\tTRUE\t/\tFALSE\t0\tsfx\t1\n", 0,
             "127.0.0.1\tTRUE\t/\tFALSE\t0\tsfx\t1\n"),
        ROWR("::1\tTRUE\t/\tFALSE\t0\tsfx\t1\n", 0,
             "::1\tTRUE\t/\tFALSE\t0\tsfx\t1\n"),
        ROW("# my own note\n", 1),
        ROW("# Netscape HTTP Cookie File (my note)\n", 1),
        ROW("# Edit at your own risk. Mine.\n", 1),
        // each line a known tool writes, then the spacing variants
        ROW("# Netscape HTTP Cookie File\n", 0),
        ROW("# HTTP Cookie File\n", 0),
        ROW("# Written by HTTrack Website Copier\n", 0),
        ROW("# https://curl.se/docs/http-cookies.html\n", 0),
        ROW("# https://curl.haxx.se/docs/http-cookies.html\n", 0),
        ROW("# http://curl.haxx.se/rfc/cookie_spec.html\n", 0),
        ROW("# https://curl.haxx.se/rfc/cookie_spec.html\n", 0),
        ROW("# This file was generated by libcurl! Edit at your own risk.\n",
            0),
        ROW("# http://www.netscape.com/newsref/std/cookie_spec.html\n", 0),
        ROW("# This is a generated file! Do not edit.\n", 0),
        ROW("# This is a generated file!  Do not edit.\n", 0),
        ROW("# This file is generated by yt-dlp.  Do not edit.\n", 0),
        ROW("# Generated by Wget on 2026-01-01 00:00:00.\n", 0),
        ROW("# Generated by hand: my login\n", 1),
        ROW("# Edit at your own risk.\n", 0),
        ROW("#  Netscape\tHTTP Cookie File \t\n", 0),
        ROW("# Netscape HTTP Cookie File\n"
            "# https://curl.se/docs/http-cookies.html\n"
            "# This file was generated by libcurl! Edit at your own risk.\n"
            "\n \t\n",
            0),
        ROW("# HTTP Cookie File\n"
            "# Generated by Wget on 2026-01-01 00:00:00.\n"
            "# Edit at your own risk.\n",
            0),
    };

#undef ROW
#undef ROWA
#undef ROWR
#undef ROWS

    for (i = 0; i < sizeof(loads) / sizeof(loads[0]); i++) {
      FILE *fp = FOPEN(file, "wb");
      int skipped;

      assertf(fp != NULL);
      assertf(fwrite(loads[i].bytes, 1, loads[i].len, fp) == loads[i].len);
      fclose(fp);
      jar.data[0] = '\0';
      skipped = cookie_load(NULL, &jar, fpath, "cookies.txt");
      if (skipped != loads[i].skipped ||
          (loads[i].rec != NULL && strstr(jar.data, loads[i].rec) == NULL) ||
          (loads[i].absent != NULL &&
           strstr(jar.data, loads[i].absent) != NULL)) {
        printf("%s: load row %d skipped %d lines, not %d, jar [%s]\n", LABEL,
               (int) i, skipped, loads[i].skipped, jar.data);
        err = 1;
      }
    }
    // of a repeated cookie, the last line is the one sent
    {
      FILE *fp = FOPEN(file, "wb");

      assertf(fp != NULL);
      fprintf(fp, "d.test\tTRUE\t/\tFALSE\t0\tsid\tfirst\n"
                  "d.test\tTRUE\t/\tFALSE\t0\tsid\tlast\n");
      fclose(fp);
      jar.data[0] = '\0';
      assertf(cookie_load(NULL, &jar, fpath, "cookies.txt") == 1);
      http_cookie_header(&jar, "www.d.test", "/", hdr, sizeof(hdr));
      if (!cookie_sent(hdr, "sid=last") || cookie_sent(hdr, "sid=first")) {
        printf("%s: a repeated cookie sent [%s]\n", LABEL, hdr);
        err = 1;
      }
    }
    // the eighth field we write marks HttpOnly
    {
      FILE *fp = FOPEN(file, "wb");

      assertf(fp != NULL);
      fprintf(fp, "d.test\tTRUE\t/\tFALSE\t0\tn\tv\tHttpOnly\n");
      fclose(fp);
      jar.data[0] = '\0';
      assertf(cookie_load(NULL, &jar, fpath, "cookies.txt") == 0);
      assertf(cookie_save(NULL, &jar, saved) == 0);
      cookiejar_slurp(saved, text, sizeof(text));
      if (strstr(text, "\n#HttpOnly_.d.test\tTRUE\t/\tFALSE\t0\tn\tv" LF) ==
          NULL) {
        printf("%s: an eighth HttpOnly field saved as [%s]\n", LABEL, text);
        err = 1;
      }
    }
    /* A Domain= naming a suffix host stays host-only through a save and a
       reload, and an old jar's TRUE line for one loads as host-only. */
    {
      static const struct {
        const char *line; // NULL: a Set-Cookie from host instead
        const char *host, *sub;
      } sfx[] = {
          {NULL, "localhost", "www.localhost"},
          {".localhost\tTRUE\t/\tFALSE\t0\tsfx\t1\n", "localhost",
           "www.localhost"},
          {"nas\tTRUE\t/\tFALSE\t0\tsfx\t1\n", "nas", "www.nas"},
      };

      for (i = 0; i < sizeof(sfx) / sizeof(sfx[0]); i++) {
        int skipped;

        jar.data[0] = '\0';
        if (sfx[i].line == NULL) {
          htsblk r;
          char line[64]; // treathead NUL-cuts the header in place

          memset(&r, 0, sizeof(r));
          strcpybuff(line, "Set-Cookie: sfx=1; Domain=localhost");
          treathead(&jar, sfx[i].host, "/", &r, line);
          assertf(cookie_save(NULL, &jar, file) == 0);
        } else {
          FILE *fp = FOPEN(file, "wb");

          assertf(fp != NULL);
          fprintf(fp, "%s", sfx[i].line);
          fclose(fp);
        }
        jar.data[0] = '\0';
        skipped = cookie_load(NULL, &jar, fpath, "cookies.txt");
        http_cookie_header(&jar, sfx[i].host, "/", hdr, sizeof(hdr));
        if (skipped != 0 || !cookie_sent(hdr, "sfx=1")) {
          printf("%s: suffix row %d: %d skipped, not sent to %s, jar [%s]\n",
                 LABEL, (int) i, skipped, sfx[i].host, jar.data);
          err = 1;
        }
        http_cookie_header(&jar, sfx[i].sub, "/", hdr, sizeof(hdr));
        if (cookie_sent(hdr, "sfx=1")) {
          printf("%s: suffix row %d: sent to %s\n", LABEL, (int) i, sfx[i].sub);
          err = 1;
        }
      }
    }
    // only a live line that loses its subdomains says so
    {
      static const struct {
        const char *line;
        hts_boolean warns;
      } logs[] = {
          {".com\tTRUE\t/\tFALSE\t0\tsfx\t1\n", HTS_TRUE},
          {"com\tFALSE\t/\tFALSE\t0\tsfx\t1\n", HTS_FALSE},
          {".com\tTRUE\t/\tFALSE\t1\tsfx\t1\n", HTS_FALSE},
      };

      for (i = 0; i < sizeof(logs) / sizeof(logs[0]); i++) {
        cookie_logcap cap;
        char logged[1024];
        FILE *fp = FOPEN(file, "wb");

        assertf(fp != NULL);
        fprintf(fp, "%s", logs[i].line);
        fclose(fp);
        jar.data[0] = '\0';
        cookie_log_start(opt, &cap);
        (void) cookie_load(opt, &jar, fpath, "cookies.txt");
        cookie_log_stop(opt, &cap, logged, sizeof(logged));
        if ((strstr(logged, "public suffix") != NULL) != logs[i].warns) {
          printf("%s: suffix log row %d logged [%s]\n", LABEL, (int) i, logged);
          err = 1;
        }
      }
    }
    (void) UNLINK(file);
    (void) UNLINK(saved);
    jar.data[0] = '\0';
    if (cookie_load(NULL, &jar, fpath, "cookies.txt") != -1) {
      printf("%s: a missing jar is not reported as missing\n", LABEL);
      err = 1;
    }
    // a missing jar beside its .bak is what a save that died leaves
    {
      char bak[HTS_URLMAXSIZE * 2];
      FILE *fp;

      assertf(sprintfbuff(bak, "%s.bak", file));
      fp = FOPEN(bak, "wb");
      assertf(fp != NULL);
      fclose(fp);
      if (cookie_load(NULL, &jar, fpath, "cookies.txt") != COOKIE_LOAD_KEEP) {
        printf("%s: a missing jar with a .bak is rewritten\n", LABEL);
        err = 1;
      }
      // an empty jar beside a non-empty .bak is what a crash before sync leaves
      fp = FOPEN(file, "wb");
      assertf(fp != NULL);
      fclose(fp);
      if (cookie_load(NULL, &jar, fpath, "cookies.txt") != 0) {
        printf("%s: an empty jar with an empty .bak is not loaded\n", LABEL);
        err = 1;
      }
      fp = FOPEN(bak, "wb");
      assertf(fp != NULL);
      fprintf(fp, "d.test\tTRUE\t/\tFALSE\t0\told\t1\n");
      fclose(fp);
      if (cookie_load(NULL, &jar, fpath, "cookies.txt") != COOKIE_LOAD_KEEP) {
        printf("%s: an empty jar with a non-empty .bak is rewritten\n", LABEL);
        err = 1;
      }
      (void) UNLINK(file);
      (void) UNLINK(bak);
    }
#ifndef _WIN32
    // a jar that exists but cannot be read would be replaced unread
    if (geteuid() != 0) {
      FILE *fp = FOPEN(file, "wb");

      assertf(fp != NULL);
      assertf(fchmod(fileno(fp), 0) == 0);
      fclose(fp);
      if (cookie_load(NULL, &jar, fpath, "cookies.txt") != COOKIE_LOAD_KEEP) {
        printf("%s: an unreadable jar is rewritten\n", LABEL);
        err = 1;
      }
      (void) UNLINK(file);
    }
#endif
  }

  /* A replacement too large for a full jar leaves the old record in place. */
  {
    char BIGSTK value[201];

    memset(value, 'v', sizeof(value) - 1);
    value[sizeof(value) - 1] = '\0';
    jar.data[0] = '\0';
    jar.max_len = 300;
    assertf(cookie_add(&jar, "keep", "1", "jar.test", "/") == 0);
    if (cookie_add(&jar, "keep", value, "jar.test", "/") == 0 ||
        strstr(jar.data, "\tkeep\t1\n") == NULL) {
      printf("%s: a refused replacement left [%s]\n", LABEL, jar.data);
      err = 1;
    }
    // the room the old record frees counts, so a same-size one replaces it
    jar.max_len = strlen(jar.data) + 260;
    if (cookie_add(&jar, "keep", "2", "jar.test", "/") != 0 ||
        strstr(jar.data, "\tkeep\t2\n") == NULL ||
        strstr(jar.data, "\tkeep\t1\n") != NULL) {
      printf("%s: a same-size replacement left [%s]\n", LABEL, jar.data);
      err = 1;
    }
    jar.max_len = sizeof(jar.data);
  }

  /* An empty jar empties an existing file and keeps the old one in .bak. It
     creates no missing file, but a jar with cookies does. */
  {
    char bak[HTS_URLMAXSIZE * 2];
    FILE *fp = FOPEN(file, "wb");

    assertf(sprintfbuff(bak, "%s.bak", file));
    assertf(fp != NULL);
    fprintf(fp, "d.test\tTRUE\t/\tFALSE\t0\tgone\t1\n");
    fclose(fp);
    jar.data[0] = '\0';
    cookie_save_replacing(opt, &jar, file);
    cookiejar_slurp(file, text, sizeof(text));
    if (strstr(text, "# Netscape HTTP Cookie File") != text ||
        strstr(text, "gone") != NULL) {
      printf("%s: an empty jar saved as [%s]\n", LABEL, text);
      err = 1;
    }
    cookiejar_slurp(bak, text, sizeof(text));
    if (strstr(text, "\tgone\t1") == NULL) {
      printf("%s: the emptied jar's .bak is [%s]\n", LABEL, text);
      err = 1;
    }
    (void) UNLINK(file);
    (void) UNLINK(bak);
    cookie_save_replacing(opt, &jar, file);
    if (fexist_utf8(file)) {
      printf("%s: an empty jar created a file\n", LABEL);
      err = 1;
    }
    assertf(cookie_add(&jar, "new", "1", "jar.test", "/") == 0);
    cookie_save_replacing(opt, &jar, file);
    cookiejar_slurp(file, text, sizeof(text));
    if (strstr(text, "\tnew\t1") == NULL || fexist_utf8(bak)) {
      printf("%s: a missing jar was saved as [%s]\n", LABEL, text);
      err = 1;
    }
    (void) UNLINK(file);
  }
#ifndef _WIN32
  /* A link to a missing file stays a link, even with cookies to save. */
  {
    char link[HTS_URLMAXSIZE * 2];
    struct stat st;

    assertf(sprintfbuff(link, "%sdangling.txt", fpath));
    assertf(symlink("missing-target.txt", link) == 0);
    cookie_save_replacing(NULL, &jar, link);
    if (lstat(link, &st) != 0 || !S_ISLNK(st.st_mode)) {
      printf("%s: a dangling link was replaced\n", LABEL);
      err = 1;
    }
    // the project jar refuses it too
    if (cookie_save(NULL, &jar, link) != -1 || lstat(link, &st) != 0 ||
        !S_ISLNK(st.st_mode)) {
      printf("%s: the project jar replaced a dangling link\n", LABEL);
      err = 1;
    }
    (void) UNLINK(link);
  }

  /* The project jar follows a link to its target, and stays a link. */
  {
    char link[HTS_URLMAXSIZE * 2];
    char target[HTS_URLMAXSIZE * 2];
    struct stat st;
    FILE *fp;

    assertf(sprintfbuff(link, "%sproject-link.txt", fpath));
    assertf(sprintfbuff(target, "%sproject-target.txt", fpath));
    fp = FOPEN(target, "wb");
    assertf(fp != NULL);
    fclose(fp);
    assertf(symlink("project-target.txt", link) == 0);
    jar.data[0] = '\0';
    assertf(cookie_add(&jar, "linked", "1", "jar.test", "/") == 0);
    assertf(cookie_save(NULL, &jar, link) == 0);
    cookiejar_slurp(target, text, sizeof(text));
    if (lstat(link, &st) != 0 || !S_ISLNK(st.st_mode) ||
        strstr(text, "\tlinked\t1") == NULL) {
      printf("%s: a project jar link left the target as [%s]\n", LABEL, text);
      err = 1;
    }
    (void) UNLINK(link);
    (void) UNLINK(target);
  }
#endif

  /* An empty jar empties an existing project jar, and creates none when it is
   * missing. */
  {
    FILE *fp = FOPEN(file, "wb");

    assertf(fp != NULL);
    fprintf(fp, "d.test\tTRUE\t/\tFALSE\t0\tgone\t1\n");
    fclose(fp);
    jar.data[0] = '\0';
    assertf(cookie_save(NULL, &jar, file) == 0);
    cookiejar_slurp(file, text, sizeof(text));
    if (strstr(text, "# Netscape HTTP Cookie File") != text ||
        strstr(text, "gone") != NULL) {
      printf("%s: an emptied project jar saved as [%s]\n", LABEL, text);
      err = 1;
    }
    (void) UNLINK(file);
    if (cookie_save(NULL, &jar, file) != 0 || fexist_utf8(file)) {
      printf("%s: an empty project jar created a file\n", LABEL);
      err = 1;
    }
  }

  /* A save names its temporaries after its own pid, and leaves another run's
     temporaries alone. */
  {
    char planted[4][HTS_URLMAXSIZE * 2];
    char bak[HTS_URLMAXSIZE * 2];
    const unsigned long other = back_self_pid() + 1;
    int pass;

    assertf(sprintfbuff(bak, "%s.bak", file));
    assertf(sprintfbuff(planted[0], "%s.tmp", file));
    assertf(sprintfbuff(planted[1], "%s.bak.tmp", file));
    assertf(sprintfbuff(planted[2], "%s.%lu.tmp", file, other));
    assertf(sprintfbuff(planted[3], "%s.bak.%lu.tmp", file, other));
    jar.data[0] = '\0';
    assertf(cookie_add(&jar, "pid", "1", "jar.test", "/") == 0);
    for (pass = 0; pass < 2; pass++) {
      size_t k;
      FILE *fp = FOPEN(file, "wb");

      assertf(fp != NULL);
      fclose(fp);
      for (k = 0; k < sizeof(planted) / sizeof(planted[0]); k++) {
        fp = FOPEN(planted[k], "wb");
        assertf(fp != NULL);
        fclose(fp);
      }
      if (pass == 0)
        cookie_save_replacing(NULL, &jar, file);
      else
        assertf(cookie_save(NULL, &jar, file) == 0);
      cookiejar_slurp(file, text, sizeof(text));
      if (strstr(text, "\tpid\t1") == NULL) {
        printf("%s: pass %d saved [%s]\n", LABEL, pass, text);
        err = 1;
      }
      for (k = 0; k < sizeof(planted) / sizeof(planted[0]); k++) {
        if (!fexist_utf8(planted[k])) {
          printf("%s: pass %d deleted %s\n", LABEL, pass, planted[k]);
          err = 1;
        }
        (void) UNLINK(planted[k]);
      }
      (void) UNLINK(file);
      (void) UNLINK(bak);
    }
  }

  /* The file holds the old or the new jar whatever step fails, and .bak gets
     the old one with or without hard links. A copy that fails leaves the
     previous .bak as it was. */
  {
    static const char old[] = "d.test\tTRUE\t/\tFALSE\t0\told\t1\n";
    static const char older[] = "d.test\tTRUE\t/\tFALSE\t0\tolder\t1\n";
    static const int faults[] = {
        0,
        COOKIE_FAULT_NOLINK,
        COOKIE_FAULT_REPLACE,
        COOKIE_FAULT_NOLINK | COOKIE_FAULT_REPLACE,
        COOKIE_FAULT_NOLINK | COOKIE_FAULT_COPY_WRITE,
        COOKIE_FAULT_NOLINK | COOKIE_FAULT_COPY_READ,
        COOKIE_FAULT_NOLINK | COOKIE_FAULT_COPY_CLOSE,
        COOKIE_FAULT_WRITE,
        COOKIE_FAULT_CLOSE,
    };
    const int copy_faults = COOKIE_FAULT_COPY_WRITE | COOKIE_FAULT_COPY_READ |
                            COOKIE_FAULT_COPY_CLOSE;
    char bak[HTS_URLMAXSIZE * 2];
    char baktmp[HTS_URLMAXSIZE * 2];
    char tmp[HTS_URLMAXSIZE * 2];

    assertf(sprintfbuff(bak, "%s.bak", file));
    assertf(sprintfbuff(baktmp, "%s.bak.%lu.tmp", file, back_self_pid()));
    assertf(sprintfbuff(tmp, "%s.%lu.tmp", file, back_self_pid()));
    jar.data[0] = '\0';
    assertf(cookie_add(&jar, "new", "1", "jar.test", "/") == 0);
    for (i = 0; i < sizeof(faults) / sizeof(faults[0]); i++) {
      const int fault = faults[i];
      const hts_boolean copy_fails = (fault & copy_faults) != 0;
      // a failed write stops before the backup, so .bak keeps the older jar
      const hts_boolean bak_kept =
          copy_fails ||
          (fault & (COOKIE_FAULT_WRITE | COOKIE_FAULT_CLOSE)) != 0;
      const hts_boolean fails = bak_kept || (fault & COOKIE_FAULT_REPLACE) != 0;
      FILE *fp = FOPEN(file, "wb");
#ifndef _WIN32
      struct stat before, after;
#endif

      assertf(fp != NULL);
      fprintf(fp, "%s", old);
#ifndef _WIN32
      assertf(fstat(fileno(fp), &before) == 0);
#endif
      fclose(fp);
      fp = FOPEN(bak, "wb");
      assertf(fp != NULL);
      fprintf(fp, "%s", older);
      fclose(fp);
      cookie_save_fault_selftest = fault;
      cookie_save_replacing(NULL, &jar, file);
      cookie_save_fault_selftest = 0;
      cookiejar_slurp(file, text, sizeof(text));
      if ((strcmp(text, old) == 0) != fails ||
          (!fails && strstr(text, "\tnew\t1") == NULL) || fexist_utf8(tmp) ||
          fexist_utf8(baktmp)) {
        printf("%s: fault %d left the jar as [%s]\n", LABEL, fault, text);
        err = 1;
      }
      cookiejar_slurp(bak, text, sizeof(text));
      if (strcmp(text, bak_kept ? older : old) != 0) {
        printf("%s: fault %d left .bak as [%s]\n", LABEL, fault, text);
        err = 1;
      }
#ifndef _WIN32
      // a link keeps the old file itself, a copy is owner-only
      fp = FOPEN(bak, "rb");
      assertf(fp != NULL);
      assertf(fstat(fileno(fp), &after) == 0);
      fclose(fp);
      if (!bak_kept &&
          ((fault == 0 && after.st_ino != before.st_ino) ||
           (after.st_mode & 077) !=
               ((fault & COOKIE_FAULT_NOLINK) ? 0 : (before.st_mode & 077)))) {
        printf("%s: fault %d made .bak inode %lu mode %o\n", LABEL, fault,
               (unsigned long) after.st_ino, (unsigned) after.st_mode);
        err = 1;
      }
#endif
      (void) UNLINK(file);
      (void) UNLINK(bak);
    }

    /* A file emptied or deleted after the load keeps the .bak as it was. */
    for (i = 0; i < 2; i++) {
      FILE *fp = FOPEN(file, "wb");

      assertf(fp != NULL);
      fprintf(fp, "%s", old);
      fclose(fp);
      jar.data[0] = '\0';
      assertf(cookie_load(NULL, &jar, fpath, "cookies.txt") == 0);
      assertf(cookie_add(&jar, "new", "1", "jar.test", "/") == 0);
      if (i == 0) {
        fp = FOPEN(file, "wb");
        assertf(fp != NULL);
        fclose(fp);
      } else {
        assertf(UNLINK(file) == 0);
      }
      fp = FOPEN(bak, "wb");
      assertf(fp != NULL);
      fprintf(fp, "%s", older);
      fclose(fp);
      cookie_save_replacing(NULL, &jar, file);
      cookiejar_slurp(bak, text, sizeof(text));
      if (strcmp(text, older) != 0 || (i == 0) != fexist_utf8(file) ||
          fsize_utf8(file) > 0 || fexist_utf8(tmp)) {
        printf("%s: a jar %s mid-crawl left .bak as [%s]\n", LABEL,
               i == 0 ? "emptied" : "deleted", text);
        err = 1;
      }
      (void) UNLINK(file);
      (void) UNLINK(bak);
    }

    /* A project jar whose rewrite fails keeps its old content. */
    for (i = 0; i < 3; i++) {
      static const int project_faults[] = {
          COOKIE_FAULT_WRITE, COOKIE_FAULT_CLOSE, COOKIE_FAULT_REPLACE};
      const int fault = project_faults[i];
      FILE *fp = FOPEN(file, "wb");

      assertf(fp != NULL);
      fprintf(fp, "%s", old);
      fclose(fp);
      cookie_save_fault_selftest = fault;
      if (cookie_save(NULL, &jar, file) != -1) {
        printf("%s: fault %d did not fail cookie_save\n", LABEL, fault);
        err = 1;
      }
      cookie_save_fault_selftest = 0;
      cookiejar_slurp(file, text, sizeof(text));
      if (strcmp(text, old) != 0 || fexist_utf8(tmp) || fexist_utf8(bak)) {
        printf("%s: fault %d left the project jar as [%s]\n", LABEL, fault,
               text);
        err = 1;
      }
      (void) UNLINK(file);
    }
  }

  printf("%s: %s\n", LABEL, err ? "FAIL" : "OK");
  return err;
#undef LABEL
}

static const char *cookieflat_dir;
static char cookieflat_logged[16384];

/* Write len bytes of content to cookies.txt in cookieflat_dir, load it for
   start_url with the log in cookieflat_logged, and return the load's result. */
static int cookieflat_load(httrackp *opt, t_cookie *jar, const char *content,
                           size_t len, const char *start_url) {
  cookie_logcap cap;
  char fpath[HTS_URLMAXSIZE * 2];
  char file[HTS_URLMAXSIZE * 2];
  FILE *fp;
  int ret;

  assertf(sprintfbuff(fpath, "%s/", cookieflat_dir));
  assertf(sprintfbuff(file, "%scookies.txt", fpath));
  fp = FOPEN(file, "wb");
  assertf(fp != NULL);
  assertf(fwrite(content, 1, len, fp) == len);
  fclose(fp);
  jar->max_len = sizeof(jar->data);
  jar->data[0] = '\0';
  cookie_log_start(opt, &cap);
  ret = cookie_load_from(opt, jar, fpath, "cookies.txt", start_url);
  cookie_log_stop(opt, &cap, cookieflat_logged, sizeof(cookieflat_logged));
  (void) UNLINK(file);
  return ret;
}

static int st_cookieflat(httrackp *opt, int argc, char **argv) {
#define LABEL "cookie-flat"
#define SECURE "TRUE"
#define PLAIN "FALSE"
#define FLAT_DOMAIN(dom, sec, name, value)                                     \
  dom "\tTRUE\t/\t" sec "\t0\t" name "\t" value "\n"
#define FLAT_HOST(host, sec, name, value)                                      \
  host "\tFALSE\t/\t" sec "\t0\t" name "\t" value "\n"
#define BOM "\xEF\xBB\xBF"

  static const struct {
    const char *content, *start_url;
    int ret;
    const char *want[4]; // every record the jar then holds, NULL-ended
    const char *log;     // NULL: no check
  } cases[] = {
      // a registrable domain over https, with '=' in a value
      {"a=1; b=2=3",
       "https://www.example.com/x",
       COOKIE_LOAD_KEEP,
       {FLAT_DOMAIN("example.com", SECURE, "a", "1"),
        FLAT_DOMAIN("example.com", SECURE, "b", "2=3"), NULL},
       "loaded 2 from the document.cookie string in"},
      // a pasted request header, CRLF, blank lines, empty pairs and blanks
      {"\r\n \t \n Cookie:  a=1;; ;  b=2  \r\n",
       "WWW.Example.CO.UK",
       COOKIE_LOAD_KEEP,
       {FLAT_DOMAIN("example.co.uk", PLAIN, "a", "1"),
        FLAT_DOMAIN("example.co.uk", PLAIN, "b", "2"), NULL},
       "for example.co.uk and its subdomains"},
      // an address or a public suffix stays host-only
      {"a=1",
       "http://127.0.0.1:8080/",
       COOKIE_LOAD_KEEP,
       {FLAT_HOST("127.0.0.1", PLAIN, "a", "1"), NULL},
       "for 127.0.0.1 alone"},
      {"a=1",
       "http://[::ffff:1.2.3.4]:8080/",
       COOKIE_LOAD_KEEP,
       {FLAT_HOST("::ffff:1.2.3.4", PLAIN, "a", "1"), NULL},
       NULL},
      {"a=1",
       "http://localhost/",
       COOKIE_LOAD_KEEP,
       {FLAT_HOST("localhost", PLAIN, "a", "1"), NULL},
       NULL},
      {"a=1",
       "http://co.uk/",
       COOKIE_LOAD_KEEP,
       {FLAT_HOST("co.uk", PLAIN, "a", "1"), NULL},
       NULL},
      // the first of a repeated name wins, and a pair with no name is refused
      {"a=1; junk; =x; a=2; c=3\n",
       "http://x.example.org/",
       COOKIE_LOAD_KEEP,
       {FLAT_DOMAIN("example.org", PLAIN, "a", "1"),
        FLAT_DOMAIN("example.org", PLAIN, "c", "3"), NULL},
       "ignoring a repeated 'a'"},
      // a TAB on a later line is refused by the jar
      {"a=1\nb=2\tx\n",
       "http://example.net/",
       COOKIE_LOAD_KEEP,
       {FLAT_DOMAIN("example.net", PLAIN, "a", "1"), NULL},
       "ignoring 'b'"},
      // a '#' after the first byte is no comment
      {"a=1#x",
       "http://example.net/",
       COOKIE_LOAD_KEEP,
       {FLAT_DOMAIN("example.net", PLAIN, "a", "1#x"), NULL},
       NULL},
      // "Cookie:" is a header name only at the start of a line
      {"a=1; Cookie: b=2",
       "http://example.net/",
       COOKIE_LOAD_KEEP,
       {FLAT_DOMAIN("example.net", PLAIN, "a", "1"),
        FLAT_DOMAIN("example.net", PLAIN, "Cookie: b", "2"), NULL},
       NULL},
      // the quotes a browser console shows go, but an unmatched one stays
      {"'a=1; b=2'\n",
       "http://example.net/",
       COOKIE_LOAD_KEEP,
       {FLAT_DOMAIN("example.net", PLAIN, "a", "1"),
        FLAT_DOMAIN("example.net", PLAIN, "b", "2"), NULL},
       NULL},
      {" \"a=1; b=2\" \r\n",
       "http://example.net/",
       COOKIE_LOAD_KEEP,
       {FLAT_DOMAIN("example.net", PLAIN, "a", "1"),
        FLAT_DOMAIN("example.net", PLAIN, "b", "2"), NULL},
       NULL},
      // only the last pair of the quoted line loses a closing quote
      {"'a=x'; b=2'",
       "http://example.net/",
       COOKIE_LOAD_KEEP,
       {FLAT_DOMAIN("example.net", PLAIN, "a", "x'"),
        FLAT_DOMAIN("example.net", PLAIN, "b", "2"), NULL},
       NULL},
      {"\"a=1; b=2\"\nc=3\"",
       "http://example.net/",
       COOKIE_LOAD_KEEP,
       {FLAT_DOMAIN("example.net", PLAIN, "a", "1"),
        FLAT_DOMAIN("example.net", PLAIN, "b", "2"),
        FLAT_DOMAIN("example.net", PLAIN, "c", "3\""), NULL},
       NULL},
      {"'a=1; b=2",
       "http://example.net/",
       COOKIE_LOAD_KEEP,
       {FLAT_DOMAIN("example.net", PLAIN, "'a", "1"),
        FLAT_DOMAIN("example.net", PLAIN, "b", "2"), NULL},
       NULL},
      // a UTF-8 BOM is not part of the first name, nor hides a comment
      {BOM "a=1",
       "http://example.net/",
       COOKIE_LOAD_KEEP,
       {FLAT_DOMAIN("example.net", PLAIN, "a", "1"), NULL},
       NULL},
      {BOM "#see=http://x/?a=b\nx.test\tFALSE\t/\tFALSE\t0\tn\tv\n",
       "http://www.example.com/",
       1,
       {FLAT_HOST("x.test", PLAIN, "n", "v"), NULL},
       NULL},
      // a Netscape line spaced instead of TABbed has a blank in its first name
      {"127.0.0.1 FALSE / FALSE 0 sp v=1\n",
       "http://127.0.0.1/",
       1,
       {NULL},
       "fewer than 7"},
      // no start URL refuses it, and NULL reads a Netscape jar only
      {"a=1", "", COOKIE_LOAD_KEEP, {NULL}, "no start URL"},
      {"a=1", NULL, 1, {NULL}, "fewer than 7"},
      // a TAB, a comment or no '=' on the first line keeps the Netscape path
      {"x.test\tFALSE\t/\tFALSE\t0\tn\tv\n",
       "http://www.example.com/",
       0,
       {FLAT_HOST("x.test", PLAIN, "n", "v"), NULL},
       NULL},
      {"# a=b\n", "http://www.example.com/", 1, {NULL}, "unknown comment"},
      {"a=b\tc\n", "http://www.example.com/", 1, {NULL}, "fewer than 7"},
      {"junk\n", "http://www.example.com/", 1, {NULL}, "fewer than 7"},
  };

  static t_cookie jar;
  const char *const logged = cookieflat_logged;
  char BIGSTK text[COOKIE_MAX_PAIR * 3];
  char BIGSTK hdr[8192];
  size_t i, j;
  int ret;
  int err = 0;

  if (argc < 1) {
    fprintf(stderr, "%s: needs a writable dir\n", LABEL);
    return 1;
  }
  cookieflat_dir = argv[0];
  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    const char *const c = cases[i].content;
    size_t wanted = 0, lines = 0;
    hts_boolean missing = HTS_FALSE;

    ret = cookieflat_load(opt, &jar, c, strlen(c), cases[i].start_url);
    for (; cases[i].want[wanted] != NULL; wanted++) {
      if (strstr(jar.data, cases[i].want[wanted]) == NULL)
        missing = HTS_TRUE;
    }
    for (j = 0; jar.data[j] != '\0'; j++)
      lines += jar.data[j] == '\n' ? 1 : 0;
    if (ret != cases[i].ret || missing || lines != wanted ||
        (cases[i].log != NULL && strstr(logged, cases[i].log) == NULL)) {
      printf("%s: case %d returned %d, jar [%s], log [%s]\n", LABEL, (int) i,
             ret, jar.data, logged);
      err = 1;
    }
  }

  /* 4096 bytes of name and value load, one more is refused, a NUL is
     refused, and neither stops the pairs after it. */
  {
    size_t n = 0;

    memcpy(text + n, "v=", 2);
    n += 2;
    memset(text + n, 'x', COOKIE_MAX_PAIR - 1);
    n += COOKIE_MAX_PAIR - 1;
    memcpy(text + n, "; w=", 4);
    n += 4;
    memset(text + n, 'y', COOKIE_MAX_PAIR);
    n += COOKIE_MAX_PAIR;
    memcpy(text + n, "; n=a\0b; z=1", 12);
    n += 12;
    ret = cookieflat_load(opt, &jar, text, n, "http://big.test/");
    if (ret != COOKIE_LOAD_KEEP || strstr(jar.data, "\tv\txxx") == NULL ||
        strstr(jar.data, "\tw\t") != NULL ||
        strstr(jar.data, "\tn\t") != NULL ||
        strstr(jar.data, FLAT_DOMAIN("big.test", PLAIN, "z", "1")) == NULL ||
        strstr(logged, "ignoring a cookie over 4096 bytes") == NULL ||
        strstr(logged, "holding a NUL") == NULL ||
        strstr(logged, "loaded 2 from") == NULL) {
      printf("%s: long pairs: returned %d, log [%s]\n", LABEL, ret, logged);
      err = 1;
    }
  }

  /* Ignored cookies are logged ten times, then counted. */
  {
    const char *p;
    int count = 0;

    strcpybuff(text, "a=1;");
    for (i = 0; i < 30; i++)
      strcatbuff(text, "x;");
    ret = cookieflat_load(opt, &jar, text, strlen(text), "http://example.net/");
    for (p = strstr(logged, "no name"); p != NULL; p = strstr(p + 1, "no name"))
      count++;
    if (ret != COOKIE_LOAD_KEEP || count != 10 ||
        strstr(logged, "ignored 20 more cookies") == NULL) {
      printf("%s: 30 ignored cookies logged %d times, log [%s]\n", LABEL, count,
             logged);
      err = 1;
    }
  }

  /* The cookies go to the subdomains of a domain, not to another domain, and
     a Secure one not over http. */
  {
    static const struct {
      const char *to;
      hts_boolean want;
    } sends[] = {
        {"https://www.example.com", HTS_TRUE},
        {"https://example.com", HTS_TRUE},
        {"https://deep.www.example.com", HTS_TRUE},
        {"http://www.example.com", HTS_FALSE},
        {"https://example.org", HTS_FALSE},
    };

    (void) cookieflat_load(opt, &jar, "s=1", 3,
                           "https://www.example.com:8443/a?b");
    for (i = 0; i < sizeof(sends) / sizeof(sends[0]); i++) {
      http_cookie_header(&jar, sends[i].to, "/any", hdr, sizeof(hdr));
      if (cookie_sent(hdr, "s=1") != sends[i].want) {
        printf("%s: s=1 %s sent to %s\n", LABEL,
               sends[i].want ? "not" : "wrongly", sends[i].to);
        err = 1;
      }
    }
  }

  printf("%s: %s\n", LABEL, err ? "FAIL" : "OK");
  return err;
#undef BOM
#undef FLAT_HOST
#undef FLAT_DOMAIN
#undef PLAIN
#undef SECURE
#undef LABEL
}

/* ------------------------------------------------------------ */
/* Registry: this module's tests, in the order -#test lists them. */
/* ------------------------------------------------------------ */

static int cookiesverdict_logged;

static void cookiesverdict_log(httrackp *opt, int type, const char *format,
                               va_list args) {
  (void) opt;
  (void) type;
  (void) format;
  (void) args;
  cookiesverdict_logged++;
}

/* -#test=cookiesverdict <file> [start-url]. Prints the verdict, the lines
   lost, and the log counts of the verdict and of a real load. */
static int st_cookiesverdict(httrackp *opt, int argc, char **argv) {
  static const char *const names[] = {"rewritten",
                                      "created",
                                      "kept-lines-lost",
                                      "kept-unreadable",
                                      "kept-not-a-file",
                                      "kept-dangling-link",
                                      "kept-missing-beside-bak",
                                      "kept-empty-beside-bak",
                                      "kept-document-cookie",
                                      "kept-no-start-url"};
  const char *const start_url = argc > 1 ? argv[1] : NULL;
  hts_cookies_verdict v;
  t_cookie *jar;
  int lost = -1, quiet, loud, ret;

  (void) opt;
  if (argc < 1 || argc > 2) {
    fprintf(stderr, "cookiesverdict: takes a file, then a start URL\n");
    return 1;
  }
  jar = (t_cookie *) calloct(1, sizeof(t_cookie));
  assertf(jar != NULL);
  jar->max_len = COOKIE_MAX_JAR;
  cookiesverdict_logged = 0;
  hts_set_log_vprint_callback(cookiesverdict_log);
  v = hts_cookies_file_verdict(argv[0], start_url, &lost);
  quiet = cookiesverdict_logged;
  // a FIFO would block the real load
  ret = v == HTS_COOKIES_KEPT_NOT_A_FILE
            ? 0
            : cookie_load_from(NULL, jar, "", argv[0],
                               start_url != NULL ? start_url : "");
  loud = cookiesverdict_logged - quiet;
  hts_set_log_vprint_callback(NULL);
  freet(jar);
  if ((size_t) v >= sizeof(names) / sizeof(names[0]))
    return 1;
  printf("%s=%d lost=%d load=%d logged=%d/%d\n", names[v], (int) v, lost, ret,
         quiet, loud);
  return 0;
}

const struct selftest_entry selftests_cookie[] = {
    {"cookies", "", "cookie request-header self-test", st_cookies},
    {"cookiecap", "", "cookie_add honours max_len", st_cookiecap},
    {"cookieport", "", "cookies are scoped to a host, not to a port",
     st_cookieport},
    {"cookiedomain", "", "a jar domain reaches a host only on a label boundary",
     st_cookiedomain},
    {"cookiecase", "", "cookie domains match without regard to case",
     st_cookiecase},
    {"cookieimport", "<dir>",
     "load a jar (and Windows IE cookies) from a long+non-ASCII folder",
     st_cookieimport},
    {"cookiehttponly", "<dir>",
     "a #HttpOnly_ jar line loads, other comments do not", st_cookiehttponly},
    {"cookiequote", "<Set-Cookie header>",
     "what the jar stores of a quoted cookie value and av", st_cookiequote},
    {"cookiejar", "<dir>",
     "the jar keeps host-only, Secure, HttpOnly and expiry, and logs a refusal",
     st_cookiejar},
    {"cookiesetscope", "",
     "Set-Cookie honours Domain= scope and Max-Age/Expires erasure",
     st_cookiesetscope},
    {"cookieflat", "<dir>",
     "a document.cookie string loads as cookies of the start URL's domain",
     st_cookieflat},
    {"cookiespandiff", "",
     "the cookie field getter matches a frozen copy of the old one",
     st_cookiespandiff},
    {"cookiesverdict", "<file> [start-url]",
     "what a crawl does to a -%K cookies file, beside a real load",
     st_cookiesverdict},
    {NULL, NULL, NULL, NULL},
};
