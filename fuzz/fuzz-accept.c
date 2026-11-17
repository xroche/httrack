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
   whether the engine mirrors the (adr, fil) a parsed page named, against the
   command-line filter rules, the wizard block and the robots level; the
   wizard-answer helpers build and record the filters an answer implies.
   The input is NUL-separated fields: knobs, adr, fil, tag, attribute, mime,
   reply, then up to six filter rules. */
#include "fuzz.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

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

/* The wizard-answer helpers over one reply: the parse, the scope, the two
   filter slots it implies, the block it records and the verdict it carries. */
static void fuzz_wizard_answer(httrackp *opt, const char *reply,
                               const char *adr, const char *fil,
                               hts_boolean seeker_up) {
  const int n = hts_wizard_parse_reply(reply);
  int slot;

  (void) hts_wizard_scope_answer(n);
  for (slot = 0; slot < HTS_WIZARD_MAX_FILTERS; slot++) {
    char buf[HTS_FILTER_SLOT_SIZE];
    htsbuff f = htsbuff_array(buf);

    hts_wizard_answer_filter(&f, slot, n, adr, fil, seeker_up);
  }
  {
    const int inserted = hts_wizard_insert_filters(opt, n, adr, fil, seeker_up);
    int forbidden_url = -1;
    int set_prio_to = 0;

    (void) inserted;
    hts_wizard_apply_verdict(opt, n, adr, fil, &forbidden_url, &set_prio_to);
  }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static int inited = 0;
  httrackp *opt;
  char adr[HTS_URLMAXSIZE];
  char fil[HTS_URLMAXSIZE];
  char tag[64];
  char attribute[64];
  char mime[64];
  char reply[64];
  char rule[FUZZ_ACCEPT_RULE_MAX];
  char note[HTS_URLMAXSIZE];
  uint8_t knobs;
  hts_boolean seeker_up;
  hts_boolean in_css_file;
  int set_prio_to = 0;
  int just_test_it = 0;
  const char *tag_arg;
  const char *attr_arg;
  int i;

  if (size == 0)
    return 0;
  if (!inited) {
    hts_init();
    inited = 1;
  }
  knobs = data[0];
  seeker_up = (knobs & 1) ? HTS_TRUE : HTS_FALSE;
  in_css_file = (knobs & 2) ? HTS_TRUE : HTS_FALSE;
  tag_arg = (knobs & 4) ? tag : NULL;
  attr_arg = (knobs & 4) ? attribute : NULL;
  data++, size--;

  if (!fuzz_field(&data, &size, adr, sizeof(adr)))
    return 0;
  if (!fuzz_field(&data, &size, fil, sizeof(fil)))
    snprintf(fil, sizeof(fil), "/");
  if (!fuzz_field(&data, &size, tag, sizeof(tag)))
    tag[0] = '\0';
  if (!fuzz_field(&data, &size, attribute, sizeof(attribute)))
    attribute[0] = '\0';
  if (!fuzz_field(&data, &size, mime, sizeof(mime)))
    snprintf(mime, sizeof(mime), "text/html");
  if (!fuzz_field(&data, &size, reply, sizeof(reply)))
    snprintf(reply, sizeof(reply), "1");

  /* Every decision is relative to the link's referer: urladr() is
     heap(ptr)->adr, so a link must be on the heap first, and opt->hash must
     outlive the calls that read it. */
  struct hash_struct hash;

  memset(&hash, 0, sizeof(hash));
  opt = hts_create_opt();
  opt->hash = &hash;
  (void) hts_record_init(opt);
  hash_init(opt, &hash, opt->urlhack);
  hash.liens = (const lien_url *const *const *) &opt->liens;
  (void) hts_record_link(opt, "fuzz.example", "/", "/dev/null", "", "",
                         NULL);
  opt->wizard = (knobs & 8) ? HTS_WIZARD_AUTO : HTS_WIZARD_NONE;
  opt->seeker = seeker_up ? (HTS_SEEKER_UP | HTS_SEEKER_DOWN) : HTS_SEEKER_DOWN;
  opt->robots = (hts_robots) ((knobs >> 5) & 3);

  /* The crawl's filter array, then the rules the input carries; the engine
     inserts a command-line rule at 0, so the last one read wins. */
  {
    char **filters = NULL;
    int filptr = 0;

    opt->maxfilter = opt->maxfilter < 128 ? 128 : opt->maxfilter;
    if (filters_init(&filters, opt->maxfilter, 0) != 0) {
      filters_bind(opt, &filters, &filptr);
      for (i = 0; i < FUZZ_ACCEPT_MAXRULES; i++) {
        if (!fuzz_field(&data, &size, rule, sizeof(rule)))
          break;
        (void) filters_insert(opt, 0, rule);
      }

      /* The decision the engine makes for every parsed link. */
      {
        const int forbidden =
            hts_acceptlink(opt, 0, adr, fil, tag_arg, attr_arg, in_css_file,
                           &set_prio_to, &just_test_it);

        (void) forbidden;
      }
      (void) hts_acceptmime(opt, 0, adr, fil, mime);
      {
        static const LLint sizes[4] = {0, 1024, (LLint) 1 << 20, -1};
        int k;

        for (k = 0; k < 4; k++)
          (void) hts_testlinksize(opt, adr, fil, sizes[k]);
      }

      /* The interactive answer layer over the same link. */
      (void) hts_wizard_parse_reply("");
      (void) hts_wizard_parse_reply("*");
      (void) hts_wizard_parse_reply("!");
      (void) hts_wizard_parse_reply("999999999999999");
      fuzz_wizard_answer(opt, reply, adr, fil, seeker_up);
      if (hts_upper_links_note(opt, note, sizeof(note)))
        (void) note[0];
    }
    /* opt owns the bound array; free_opt does not release it (the engine
       does at XH_extuninit), so release the slot block then the index. */
    if (filters[0] != NULL)
      freet(filters[0]);
    freet(filters);
  }
  hash_free(&hash);
  opt->hash = NULL;
  hts_record_free(opt);
  hts_free_opt(opt);
  return 0;
}
