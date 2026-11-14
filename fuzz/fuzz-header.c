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

/* Fuzz the HTTP response-header parser (htslib.c) the way back_wait drives it,
   after a first byte that selects the parser state (SEL_* below). */
#include "fuzz.h"
#include "htslib.h"
#include "htsbauth.h"
#include "htsurlport.h"

/* A small out makes long values hit the too-long path. */
static void fuzz_header_param(const char *value) {
  static const struct {
    const char *name, *seps;
  } params[] = {{"charset", ";"}, {"filename", ";"}, {"max", ",;"}};

  char out[16];
  size_t i;

  for (i = 0; i < sizeof(params) / sizeof(params[0]); i++) {
    const hts_boolean found = hts_header_param(
        value, params[i].seps, params[i].name, out, sizeof(out));

    if (strlen(out) >= sizeof(out) || (!found && out[0] != '\0'))
      abort();
  }
}

enum {
  /* no cookie jar */
  SEL_NO_JAR = 0x01,
  /* host form: plain, https, userinfo, none */
  SEL_HOST_SHIFT = 1,
  SEL_HOST_MASK = 3,
  /* keep-alive already on */
  SEL_KEEPALIVE = 0x08,
  /* no location buffer */
  SEL_NO_LOCATION = 0x10,
  /* the jar holds a Secure cookie for the host */
  SEL_SECURE_COOKIE = 0x20
};

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static const char *const hosts[] = {
      "www.example.com", "https://www.example.com", "u:p@[::1]:8080", NULL};
  static httrackp *opt = NULL;
  char *buf;
  htsblk r;
  t_cookie *jar;
  char rcvd[2048];
  const char *end;
  const char *host;
  int ptr = 0, adv = 0;
  unsigned sel;

  /* http_xfread1 stops a header block there */
  if (size < 1 || size - 1 > HTS_LINE_BLOCK_SIZE - 2)
    return 0;
  sel = data[0];
  data++, size--;

  if (opt == NULL) {
    hts_init();
    opt = hts_create_opt();
    opt->log = opt->errlog = NULL;
  }

  buf = fuzz_strdup(data, size);
  end = buf + size;
  host = hosts[(sel >> SEL_HOST_SHIFT) & SEL_HOST_MASK];

  jar = calloct(1, sizeof(*jar));
  jar->max_len = sizeof(jar->data);
  if (sel & SEL_SECURE_COOKIE)
    (void) cookie_set(jar, "sid", "s", "www.example.com", "/",
                      &(cookie_attr) {.secure = HTS_TRUE});
  /* htscatchurl passes neither a host nor a jar, so never pair them */
  opt->cookie = (sel & SEL_NO_JAR) || host == NULL ? NULL : jar;

  memset(&r, 0, sizeof(r));
  if (sel & SEL_KEEPALIVE) {
    r.keep_alive = 1;
    r.keep_alive_max = 10;
    r.keep_alive_t = 15;
  }
  if (!(sel & SEL_NO_LOCATION)) {
    r.location = malloct(HTS_URLMAXSIZE * 2);
    r.location[0] = '\0';
  }

  /* Mirror of back_wait's header loop (src/htsback.c); keep in sync. */
  if (buf[0] != '<') {
    /* status line, skipping one bogus blank line */
    (void) binput_line(buf + ptr, end, rcvd, 2000, &adv);
    ptr += adv;
    if (!strnotempty(rcvd)) {
      (void) binput_line(buf + ptr, end, rcvd, 2000, &adv);
      ptr += adv;
    }
    treatfirstline(&r, rcvd);

    do {
      const hts_boolean cut = binput_line(buf + ptr, end, rcvd, 2000, &adv);

      ptr += adv;
      if (cut) {
        (void) hts_header_is_read(rcvd);
        continue;
      }
      if (strnotempty(rcvd))
        treathead_opt(opt, opt->cookie, host, "/", &r, rcvd);
      if (r.statuscode == HTTP_OK && strfield(rcvd, "content-range:") &&
          !(r.crange_start == 0 && r.crange_end == r.crange - 1))
        r.statuscode = HTTP_PARTIAL_CONTENT;
    } while (strnotempty(rcvd));
  } else {
    /* a bogus server: assume text/html */
    treatfirstline(&r, buf);
  }
  /* end of mirror */

  fuzz_header_param(buf);
  opt->cookie = NULL;
  freet(r.location);
  freet(jar);
  freet(buf);
  return 0;
}
