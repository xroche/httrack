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

/* Copy the next NUL-separated field into dst (bounded), and advance past
   it. Returns HTS_FALSE at the end of the input. */
static hts_boolean fuzz_field(const uint8_t **data, size_t *size, char *dst,
                              size_t dst_size) {
  const uint8_t *nul;
  size_t len;

  if (*size == 0)
    return HTS_FALSE;
  nul = (const uint8_t *) memchr(*data, 0, *size);
  len = nul != NULL ? (size_t) (nul - *data) : *size;
  if (len >= dst_size)
    len = dst_size - 1;
  memcpy(dst, *data, len);
  dst[len] = '\0';
  *data += len;
  *size -= len;
  if (*size > 0) { /* skip the NUL itself */
    *data += 1;
    *size -= 1;
  }
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
  seeker_up = (knobs & 1) ? HTS_TRUE : HTS_FALSE;
  data++, size--;

  if (!fuzz_field(&data, &size, adr, sizeof(adr)))
    return 0;
  if (!fuzz_field(&data, &size, fil, sizeof(fil)))
    snprintf(fil, sizeof(fil), "/");
  if (!fuzz_field(&data, &size, mime, sizeof(mime)))
    snprintf(mime, sizeof(mime), "text/html");
  if (!fuzz_field(&data, &size, reply, sizeof(reply)))
    snprintf(reply, sizeof(reply), "1");

  memset(&hash, 0, sizeof(hash));
  opt = hts_create_opt();
  opt->log = opt->errlog = NULL;
  opt->hash = &hash;
  hash_init(opt, &hash, opt->urlhack);
  hash.liens = (const lien_url *const *const *) &opt->liens;
  opt->wizard = (knobs & 8) ? HTS_WIZARD_AUTO : HTS_WIZARD_NONE;
  opt->seeker = seeker_up ? (HTS_SEEKER_UP | HTS_SEEKER_DOWN) : HTS_SEEKER_DOWN;
  opt->travel = travels[(knobs >> 5) & 3];
  opt->maxfilter = opt->maxfilter < 128 ? 128 : opt->maxfilter;

  /* Link 0 is the start URL, link 1 the page holding the fuzzed link. */
  if (hts_record_init(opt) &&
      hts_record_link(opt, "fuzz.example", "/", "/dev/null", "", "", NULL) &&
      hts_record_link(opt, "fuzz.example", "/dir/page.html", "/dev/null", "",
                      "", NULL) &&
      filters_init(&filters, opt->maxfilter, 0) != 0) {
    /* depth 1 is the last level, whose links are refused */
    opt->liens[1]->depth = (knobs & 2) ? 1 : 2;
    filters_bind(opt, &filters, &filptr);
    /* appended like command-line rules, so the last match wins */
    for (i = 0; i < FUZZ_ACCEPT_MAXRULES; i++) {
      if (!fuzz_field(&data, &size, rule, sizeof(rule)))
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
