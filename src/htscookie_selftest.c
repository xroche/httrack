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

static int st_cookies(httrackp *opt, int argc, char **argv) {
  static t_cookie cookie;
  char hdr[1024];
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

  /* A hostile over-long request host must not overflow domain[256] in
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
#ifndef _WIN32
  /* the jar holds live session cookies: cookie_save must keep it 0600 */
  {
    const char *jar = "st-cookies-jar.txt";
    struct stat st;

    (void) UNLINK(jar);
    assertf(cookie_save(&cookie, jar) == 0);
    assertf(stat(jar, &st) == 0);
    assertf((st.st_mode & 07777) == HTS_PROTECT_FILE);
    assertf(st.st_size > 0); /* mode-only checks would pass an empty jar */
    /* a pre-existing world-readable jar must be tightened, not kept */
    assertf(chmod(jar, 0644) == 0);
    assertf(cookie_save(&cookie, jar) == 0);
    assertf(stat(jar, &st) == 0);
    assertf((st.st_mode & 07777) == HTS_PROTECT_FILE);
    assertf(st.st_size > 0);
    (void) UNLINK(jar);
  }
#endif
  printf("cookie-header: %s\n", err ? "FAIL" : "OK");
  if (err)
    printf("  got: %s\n", hdr);
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
  char host[256];
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

  /* cookie_del normalises like cookie_add, so one string reaches the same
     entry through either. Control: the neighbour stays. */
  jar.data[0] = '\0';
  if (cookie_add(&jar, "gone", "G", "example.com:8080", "/") != 0 ||
      cookie_add(&jar, "kept", "K", "other.example", "/") != 0) {
    printf("cookie-port: FAIL (cookie_add setup, delete form)\n");
    return 1;
  }
  cookie_del(&jar, "gone", "example.com:8080", "/");
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
  /* Above the 256 bytes of headroom cookie_add reserves, or it refuses all. */
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
     to the fixture strings: cookie_add reserves 256 bytes beyond the parts. */
  {
    const size_t fixed = strlen(NAME) + strlen(DOM) + strlen(PATH) + 256;
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

/* ------------------------------------------------------------ */
/* Registry: this module's tests, in the order -#test lists them. */
/* ------------------------------------------------------------ */

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
    {NULL, NULL, NULL, NULL},
};
