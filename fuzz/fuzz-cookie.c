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

Please visit our Website: http://www.httrack.com
*/

/* Fuzz the cookie jar and basic-auth store (htsbauth.c) on everything the
   existing header harness does not reach: the Netscape cookies.txt loader
   (cookie_load, cookie_load_from) and the rewrite side (cookie_save,
   cookie_save_replacing), record storage and erasure (cookie_set/add/remove),
   the send-side lookups (cookie_find, cookie_find_exact, cookie_nextfield,
   cookie_get, cookie_sendable, cookie_domain_match, cookie_domain_allowed),
   the expiry parsers (cookie_max_age_time, cookie_expires_time), the jar text
   surgery (cookie_insert, cookie_delete), the flat-record helpers the login
   bundles share (cookie_url_host, cookie_flat_bind, cookie_flat_pair,
   cookie_records_text) and the basic-auth list (bauth_add, bauth_check).
   The first byte picks the operation; the rest is newline-separated fields,
   and from the second line on it is also the literal cookies.txt bytes for
   the load/save paths. Cookies decide what an authenticated mirror sends
   where, so both the parsers and the matching logic are attack surface. */
#include "fuzz.h"

#include "htsbauth.h"
#include "htsbundle.h"
#include "htscore.h"
#include "htsstrings.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static unsigned long cookie_sink;

/* Split buf in place at '\n'; up to max fields. */
static size_t fuzz_lines(char *buf, char *lines[], size_t max) {
  size_t n = 0;
  char *p = buf;

  while (p != NULL && *p != '\0' && n < max) {
    char *nl = strchr(p, '\n');

    if (nl != NULL)
      *nl = '\0';
    lines[n++] = p;
    p = (nl != NULL) ? nl + 1 : NULL;
  }
  return n;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  t_cookie jar;
  char *buf;
  char *lines[8] = {NULL};
  size_t nlines;
  size_t i;
  char *file_bytes;
  size_t file_size;

  if (size < 2)
    return 0;
  buf = fuzz_strdup(data + 1, size - 1);
  nlines = fuzz_lines(buf, lines, sizeof(lines) / sizeof(lines[0]));
  /* Missing fields read as empty strings, never as wild pointers. */
  for (i = 0; i < sizeof(lines) / sizeof(lines[0]); i++)
    if (lines[i] == NULL)
      lines[i] = (char *) "";

  /* From the second line on: the literal jar-file bytes for the loaders,
     i.e. the rest of the buffer from lines[1] to its NUL (in bounds). */
  file_bytes = buf;
  file_size = strlen(buf);
  if (nlines > 1 && lines[1] >= buf && lines[1] <= buf + file_size) {
    file_bytes = lines[1];
    file_size = file_size - (size_t) (file_bytes - buf);
  }

  memset(&jar, 0, sizeof(jar));
  jar.max_len = COOKIE_MAX_JAR;
  /* Seed records, so the lookups always have material. */
  (void) cookie_add(&jar, "sid", "s1", "fuzz.example", "/");
  (void) cookie_add(&jar, "pref", "p1", ".example.com", "/");

  switch (data[0] % 8) {
  case 0: {                       /* store and erase, with attributes */
    cookie_attr attr;

    memset(&attr, 0, sizeof(attr));
    attr.secure = (nlines > 4 && (lines[4][0] & 1)) ? HTS_TRUE : HTS_FALSE;
    attr.http_only = (nlines > 4 && (lines[4][0] & 2)) ? HTS_TRUE : HTS_FALSE;
    attr.host_only = (nlines > 4 && (lines[4][0] & 4)) ? HTS_TRUE : HTS_FALSE;
    attr.expires = (nlines > 5) ? (int64_t) atol(lines[5]) : 0;
    (void) cookie_set(&jar, lines[0], nlines > 1 ? lines[1] : "v",
                      nlines > 2 ? lines[2] : "fuzz.example",
                      nlines > 3 ? lines[3] : "/", &attr);
    (void) cookie_set(&jar, lines[0], nlines > 1 ? lines[1] : "v",
                      nlines > 2 ? lines[2] : "fuzz.example",
                      nlines > 3 ? lines[3] : "/", NULL);
    (void) cookie_remove(&jar, lines[0],
                         nlines > 2 ? lines[2] : "fuzz.example",
                         nlines > 3 ? lines[3] : "/");
    break;
  }
  case 1: {                       /* send-side lookups */
    const char *dom = nlines > 0 ? lines[0] : "fuzz.example";
    const char *path = nlines > 1 ? lines[1] : "/";
    char *rec = cookie_find(jar.data, "", dom, path);
    int https = (nlines > 2 && (lines[2][0] & 1));
    int64_t now = (int64_t) time(NULL);
    char field[8192];
    int i;

    /* cookie_nextfield stops at the jar's trailing NUL, which it keeps
       returning: end the walk on it, not on NULL. */
    for (; rec != NULL && *rec != '\0'; rec = cookie_nextfield(rec)) {
      (void) cookie_sendable(rec, https ? HTS_TRUE : HTS_FALSE, now);
      for (i = 0; i < 6; i++) {
        (void) cookie_get(field, rec, i);
        cookie_sink += (unsigned long) strlen(field);
      }
    }
    rec = cookie_find_exact(jar.data, lines[0], dom, path);
    cookie_sink += (rec != NULL) ? (unsigned long) strlen(rec) : 0;
    (void) cookie_domain_match(dom, nlines > 2 ? lines[2] : "www.fuzz.example");
    (void) cookie_domain_allowed(dom,
                                 nlines > 2 ? lines[2] : "www.fuzz.example");
    break;
  }
  case 2: {                       /* jar file load, save and reload */
    char dir[] = "/tmp/fuzz-cookie-XXXXXX";
    char path[256];

    if (mkdtemp(dir) != NULL) {
      FILE *fp;

      snprintf(path, sizeof(path), "%s/cookies.txt", dir);
      fp = fopen(path, "wb");
      if (fp != NULL) {
        fwrite(file_bytes, 1, file_size, fp);
        fclose(fp);
      }
      (void) cookie_load(NULL, &jar, dir, "cookies.txt");
      (void) cookie_load_from(NULL, &jar, dir, "cookies.txt",
                              nlines > 0 ? lines[0] : "https://fuzz.example/");
      (void) cookie_save(NULL, &jar, path);
      (void) cookie_load(NULL, &jar, dir, "cookies.txt");
      (void) cookie_save_replacing(NULL, &jar, path);
      snprintf(path, sizeof(path), "%s/cookies.txt", dir);
      (void) unlink(path);
      (void) rmdir(dir);
    }
    break;
  }
  case 3: {                       /* basic auth */
    (void) bauth_add(&jar, nlines > 0 ? lines[0] : "fuzz.example",
                     nlines > 1 ? lines[1] : "/secure/",
                     nlines > 2 ? lines[2] : "dXNlcjpwYXNz");
    {
      char *auth = bauth_check(&jar, nlines > 0 ? lines[0] : "fuzz.example",
                               nlines > 1 ? lines[1] : "/secure/x.html");

      cookie_sink += (auth != NULL) ? (unsigned long) strlen(auth) : 0;
    }
    bauth_free(&jar);
    break;
  }
  case 4: {                       /* expiry parsing */
    int64_t when = 0;
    int64_t now = (int64_t) time(NULL);

    if (cookie_max_age_time(lines[0], now, &when))
      cookie_sink += (unsigned long) when;
    when = 0;
    if (cookie_expires_time(lines[0], now, &when))
      cookie_sink += (unsigned long) when;
    break;
  }
  case 5: {                       /* jar text surgery */
    char s[1024];

    strlcpybuff(s, lines[0], sizeof(s));
    if (nlines > 1)
      cookie_insert(s, sizeof(s), lines[1]);
    if (nlines > 2) {
      /* Callers guarantee pos <= strlen(s) (both pass within-string
         offsets), so the harness clamps to the same contract. */
      size_t len = strlen(s);
      size_t pos = (size_t) atol(lines[2]);

      cookie_delete(s, sizeof(s), pos > len ? len : pos);
    }
    cookie_sink += (unsigned long) strlen(s);
    break;
  }
  case 6: {                       /* the flat helpers the bundles share */
    char host[COOKIE_MAX_DOMAIN];
    cookie_attr attr;
    char pair[512];
    const char *bind;
    String text;

    memset(&attr, 0, sizeof(attr));
    bind = cookie_flat_bind(nlines > 0 ? lines[0] : "https://a.fuzz.example/x",
                            host, sizeof(host), &attr);
    if (bind != NULL) {
      int ignored = 0;

      strlcpybuff(pair, lines[0], sizeof(pair));
      (void) cookie_flat_pair(NULL, &jar, pair, bind, &attr, lines[0],
                              &ignored);
      cookie_sink += (unsigned long) ignored;
    }
    StringInit(text);
    cookie_records_text(&jar, &text);
    cookie_sink += (unsigned long) StringLength(text);
    StringFree(text);
    break;
  }
  default: {                      /* host scoping and a field sweep */
    char host[COOKIE_MAX_DOMAIN];
    char field[8192];
    int i;

    (void) cookie_url_host(nlines > 0 ? lines[0] : "https://[IPv6:::1]:80/",
                           host, sizeof(host));
    cookie_sink += (unsigned long) strlen(host);
    for (i = 0; i < 8; i++) {
      (void) cookie_get(field, jar.data, i);
      cookie_sink += (unsigned long) strlen(field);
    }
    break;
  }
  }

  freet(buf);
  return 0;
}
