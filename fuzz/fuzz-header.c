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

/* Fuzz the HTTP response-header parser (htslib.c) the way back_wait drives it:
   binput_line() splits the block (2000-byte lines, over-long ones dropped),
   treatfirstline reads the status line, treathead_opt each header.
   The first input byte selects the state those calls see, see below. */
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

/* Selector byte:
   bit 0: no cookie jar        bits 1-2: host form (plain, https, userinfo,
   none) bit 3: keep-alive already on   bit 4: no location buffer bit 5: jar
   holds a Secure cookie for the host */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static const char *const hosts[] = {
      "www.example.com", "https://www.example.com", "u:p@[::1]:8080", NULL};
  static httrackp *opt = NULL;
  char *buf;
  htsblk r;
  t_cookie *cookie;
  char rcvd[2048];
  const char *end, *p;
  const char *host;
  int ptr = 0, adv = 0;
  unsigned sel;

  if (size < 1)
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
  host = hosts[(sel >> 1) & 3];

  cookie = calloct(1, sizeof(*cookie));
  cookie->max_len = sizeof(cookie->data);
  if (sel & 0x20)
    (void) cookie_set(cookie, "sid", "s", "www.example.com", "/",
                      &(cookie_attr) {.secure = HTS_TRUE});
  /* no host and a jar never meet: htscatchurl passes neither */
  opt->cookie = (sel & 1) || host == NULL ? NULL : cookie;

  memset(&r, 0, sizeof(r));
  if (sel & 0x08) {
    r.keep_alive = 1;
    r.keep_alive_max = 10;
    r.keep_alive_t = 15;
  }
  if (!(sel & 0x10)) {
    r.location = malloct(HTS_URLMAXSIZE * 2);
    r.location[0] = '\0';
  }

  /* as back_wait: status line, skipping one bogus blank line */
  p = buf;
  (void) binput_line(p + ptr, end, rcvd, 2000, &adv);
  ptr += adv;
  if (!strnotempty(rcvd)) {
    (void) binput_line(p + ptr, end, rcvd, 2000, &adv);
    ptr += adv;
  }
  treatfirstline(&r, rcvd);

  do {
    const hts_boolean cut = binput_line(p + ptr, end, rcvd, 2000, &adv);

    ptr += adv;
    if (cut)
      continue;
    if (strnotempty(rcvd))
      treathead_opt(opt, opt->cookie, host, "/", &r, rcvd);
    if (r.statuscode == HTTP_OK && strfield(rcvd, "content-range:") &&
        !(r.crange_start == 0 && r.crange_end == r.crange - 1))
      r.statuscode = HTTP_PARTIAL_CONTENT;
    (void) hts_header_is_read(rcvd);
  } while (strnotempty(rcvd));

  fuzz_header_param(buf);
  opt->cookie = NULL;
  freet(r.location);
  freet(cookie);
  freet(buf);
  return 0;
}
