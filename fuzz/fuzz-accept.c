/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998 Xavier Roche and other contributors

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

/* Fuzz the link-acceptance wizard (htswizard.c): hts_acceptlink() decides
   whether the engine mirrors the (adr, fil) a page links to, against the
   referer's depth, the travel scope and the command-line filter rules. Then
   the wizard records the filters a reply implies.
   The input is a knob byte, then NUL-separated fields: adr, fil, mime, reply,
   then up to six filter rules. */
#include "fuzz.h"

#include <stdio.h>
#include <string.h>

#include "htscore.h"
#include "htshash.h"
#include "htsglobal.h"
#include "htsopt.h"
#include "htswizard.h"

#define FUZZ_ACCEPT_MAXRULES 6
#define FUZZ_ACCEPT_RULE_MAX 256

/* Knob byte bits. Bits 2, 4 and 7 are unused. */
#define KNOB_SEEKER_UP 0x01  /* the crawl may go up a directory */
#define KNOB_LAST_LEVEL 0x02 /* the linking page sits at the last depth */
#define KNOB_WIZARD 0x08     /* run the wizard, which applies the rules */
#define KNOB_TRAVEL_SHIFT 5  /* two bits: index into travels[] */
#define KNOB_TRAVEL_MASK 0x03

/* Copy the next NUL-separated field of *rest into dst, clipped, and advance
   past it. Returns HTS_FALSE at the end of the input. */
static hts_boolean fuzz_field(hts_span *rest, hts_buf dst) {
  hts_span field;

  if (!hts_span_next_in(rest, '\0', &field))
    return HTS_FALSE;
  (void) hts_buf_put_span_clip(dst, field);
  return HTS_TRUE;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static const int travels[4] = {HTS_TRAVEL_SAME_ADDRESS,
                                 HTS_TRAVEL_SAME_DOMAIN, HTS_TRAVEL_SAME_TLD,
                                 HTS_TRAVEL_EVERYWHERE};
  static int inited = 0;
  httrackp *opt;
  struct hash_struct hash;
  char **filters = NULL;
  int filptr = 0;
  char adr[HTS_URLMAXSIZE];
  char fil[HTS_URLMAXSIZE];
  char mime[64];
  char reply[64];
  char rule[FUZZ_ACCEPT_RULE_MAX];
  uint8_t knobs;
  hts_boolean seeker_up;
  hts_span rest;
  int set_prio_to = 0;
  int just_test_it = 0;
  int i;

  if (size == 0)
    return 0;
  if (!inited) {
    hts_init();
    inited = 1;
  }
  knobs = data[0];
  seeker_up = (knobs & KNOB_SEEKER_UP) ? HTS_TRUE : HTS_FALSE;
  rest = fuzz_span(data + 1, size - 1);

  if (!fuzz_field(&rest, HTS_BUF(adr)))
    return 0;
  if (!fuzz_field(&rest, HTS_BUF(fil)))
    snprintf(fil, sizeof(fil), "/");
  if (!fuzz_field(&rest, HTS_BUF(mime)))
    snprintf(mime, sizeof(mime), "text/html");
  if (!fuzz_field(&rest, HTS_BUF(reply)))
    snprintf(reply, sizeof(reply), "1");

  memset(&hash, 0, sizeof(hash));
  opt = hts_create_opt();
  opt->log = opt->errlog = NULL;
  opt->hash = &hash;
  hash_init(opt, &hash, opt->urlhack);
  hash.liens = (const lien_url *const *const *) &opt->liens;
  opt->wizard = (knobs & KNOB_WIZARD) ? HTS_WIZARD_AUTO : HTS_WIZARD_NONE;
  opt->seeker = seeker_up ? (HTS_SEEKER_UP | HTS_SEEKER_DOWN) : HTS_SEEKER_DOWN;
  opt->travel = travels[(knobs >> KNOB_TRAVEL_SHIFT) & KNOB_TRAVEL_MASK];
  opt->maxfilter = opt->maxfilter < 128 ? 128 : opt->maxfilter;

  /* Link 0 is the start URL, link 1 the page holding the fuzzed link. */
  if (hts_record_init(opt) &&
      hts_record_link(opt, "fuzz.example", "/", "/dev/null", "", "", NULL) &&
      hts_record_link(opt, "fuzz.example", "/dir/page.html", "/dev/null", "",
                      "", NULL) &&
      filters_init(&filters, opt->maxfilter, 0) != 0) {
    /* depth 1 is the last level, whose links are refused */
    opt->liens[1]->depth = (knobs & KNOB_LAST_LEVEL) ? 1 : 2;
    filters_bind(opt, &filters, &filptr);
    /* appended like command-line rules, so the last match wins */
    for (i = 0; i < FUZZ_ACCEPT_MAXRULES; i++) {
      if (!fuzz_field(&rest, HTS_BUF(rule)))
        break;
      (void) filters_insert(opt, filptr, rule);
    }

    (void) hts_acceptlink(opt, 1, adr, fil, NULL, NULL, HTS_FALSE, &set_prio_to,
                          &just_test_it);
    (void) hts_acceptmime(opt, 1, adr, fil, mime);
    {
      static const LLint sizes[4] = {0, 1024, (LLint) 1 << 20, -1};
      int k;

      for (k = 0; k < 4; k++)
        (void) hts_testlinksize(opt, adr, fil, sizes[k]);
    }

    /* The wizard's answer to a reply about the same link. */
    {
      const int n = hts_wizard_parse_reply(reply);
      int forbidden_url = -1;

      (void) hts_wizard_insert_filters(opt, n, adr, fil, seeker_up);
      hts_wizard_apply_verdict(opt, n, adr, fil, &forbidden_url, &set_prio_to);
    }
  }
  /* free_opt leaves the bound filter array to its owner */
  if (filters != NULL) {
    freet(filters[0]);
    freet(filters);
  }
  hash_free(&hash);
  opt->hash = NULL;
  hts_record_free(opt);
  hts_free_opt(opt);
  return 0;
}
