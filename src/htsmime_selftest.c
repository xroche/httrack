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
/* File: htsmime_selftest.c subroutines:                        */
/*       self-tests for content-type detection                  */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htsselftest_int.h"

static int st_mime(httrackp *opt, int argc, char **argv) {
  char mime[256];

  if (argc < 1) {
    fprintf(stderr, "mime: needs a filename\n");
    return 1;
  }
  if (get_httptype_sized(opt, mime, sizeof(mime), argv[0], 0)) {
    char ext[256];

    printf("%s is '%s'\n", argv[0], mime);
    if (give_mimext(ext, sizeof(ext), mime))
      printf("and its local type is '.%s'\n", ext);
  } else {
    printf("%s is of an unknown MIME type\n", argv[0]);
  }
  return 0;
}

/* Wider than the longest test value, so a byte written past any destination
   still lands in the canary rather than beyond it. */
enum { ST_ASSUME_CANARY = 1200 };

/* Build "\ncgi=<value_len bytes of 'B'>\n" into opt->mimedefs. */
static void st_assume_rule(httrackp *opt, size_t value_len) {
  size_t i;

  StringCopy(opt->mimedefs, "\ncgi=");
  for (i = 0; i < value_len; i++)
    StringAddchar(opt->mimedefs, 'B');
  StringCat(opt->mimedefs, "\n");
}

static unsigned st_assume_clips; /* clip warnings the callback has seen */

static HTS_PRINTF_FUN(3, 0) void st_assume_log(httrackp *opt, int type,
                                               const char *format,
                                               va_list args) {
  (void) opt;
  (void) format;
  (void) args;
  if ((type & 0xff) == LOG_WARNING)
    st_assume_clips++;
}

/* An empty selector runs every destination. */
static hts_boolean st_assume_wants(const char *only, const char *capacity) {
  return *only == '\0' || strcmp(only, capacity) == 0 ? HTS_TRUE : HTS_FALSE;
}

/* An --assume value is clipped to the buffer it lands in, and the clip is
   reported rather than fatal (#1276). Each value overshoots a different subset
   of the three destinations, so one shared bound cannot pass them all. */
static int st_assumemime(httrackp *opt, int argc, char **argv) {
  static const size_t values[] = {200, 300, 1100};
  /* pre-fix every destination smashes and only the first assertion to fire is
     visible, so each is selectable by its capacity */
  const char *const only = argc >= 1 ? argv[0] : "";
  const int save_debug = opt->debug;
  char canary[ST_ASSUME_CANARY];
  size_t v;

  if (!st_assume_wants(only, "128") && !st_assume_wants(only, "256") &&
      !st_assume_wants(only, "1024")) {
    fprintf(stderr, "assumemime: want one of 128, 256, 1024\n");
    return 1;
  }
  memset(canary, '#', sizeof(canary));
  /* the callback runs above the level filter, so the warnings can be counted
     while the log itself stays quiet and stdout carries one line */
  opt->debug = LOG_ERROR;
  hts_set_log_vprint_callback(st_assume_log);

  for (v = 0; v < sizeof(values) / sizeof(values[0]); v++) {
    const size_t len = values[v];

    st_assume_rule(opt, len);

    /* ishtml()'s mime[256] */
    if (st_assume_wants(only, "256")) {
      struct {
        char dst[256];
        char canary[ST_ASSUME_CANARY];
      } s;

      memset(&s, '#', sizeof(s));
      st_assume_clips = 0;
      assertf(get_userhttptype(opt, s.dst, sizeof(s.dst), "/x.cgi"));
      assertf(strlen(s.dst) == (len < sizeof(s.dst) ? len : sizeof(s.dst) - 1));
      assertf(memcmp(s.canary, canary, sizeof(s.canary)) == 0);
      assertf(st_assume_clips == (len < sizeof(s.dst) ? 0u : 1u));
      /* the frame ishtml() owns, which is where #1276 was reported */
      assertf(ishtml(opt, "/x.cgi") == 0);
    }

    /* url_savename()'s mime[1024] */
    if (st_assume_wants(only, "1024")) {
      struct {
        char dst[1024];
        char canary[ST_ASSUME_CANARY];
      } l;

      memset(&l, '#', sizeof(l));
      st_assume_clips = 0;
      assertf(get_userhttptype(opt, l.dst, sizeof(l.dst), "/x.cgi"));
      assertf(strlen(l.dst) == (len < sizeof(l.dst) ? len : sizeof(l.dst) - 1));
      assertf(memcmp(l.canary, canary, sizeof(l.canary)) == 0);
      assertf(st_assume_clips == (len < sizeof(l.dst) ? 0u : 1u));
    }

    /* htsblk.contenttype[128]: charset and contentencoding follow it in the
       same struct, so an overrun stays intra-object and neither ASan nor
       _FORTIFY_SOURCE reports it */
    if (st_assume_wants(only, "128")) {
      htsblk r;

      memset(&r, '#', sizeof(r));
      r.contenttype[0] = '\0';
      st_assume_clips = 0;
      assertf(get_httptype_sized(opt, r.contenttype, sizeof(r.contenttype),
                                 "/x.cgi", 0));
      assertf(strlen(r.contenttype) == sizeof(r.contenttype) - 1);
      assertf(memcmp(r.charset, canary, sizeof(r.charset)) == 0);
      assertf(memcmp(r.contentencoding, canary, sizeof(r.contentencoding)) ==
              0);
      assertf(st_assume_clips == 1);
    }
  }

  /* control: a value every destination holds is written whole, unclipped and
     unreported, and still reaches ishtml() as the type it names */
  StringCopy(opt->mimedefs, "\ncgi=text/html\n");
  {
    char mime[256];

    st_assume_clips = 0;
    assertf(get_userhttptype(opt, mime, sizeof(mime), "/x.cgi"));
    assertf(strcmp(mime, "text/html") == 0);
    assertf(st_assume_clips == 0);
    assertf(ishtml(opt, "/x.cgi") == 1);
  }

  hts_set_log_vprint_callback(NULL);
  opt->debug = save_debug;
  printf("assumemime ok\n");
  return 0;
}

static int st_sniff(httrackp *opt, int argc, char **argv) {
  char BIGSTK body[1024];
  size_t n;

  (void) opt;
  if (argc < 2) {
    fprintf(stderr, "sniff: needs a content-type and a body\n");
    return 1;
  }
  n = st_decode_body(argv[1], body, sizeof(body));
  printf("sniff: known=%d consistent=%d\n",
         hts_sniff_mime_known(argv[0]) == HTS_TRUE,
         hts_sniff_mime_consistent(body, n, argv[0]) == HTS_TRUE);
  return 0;
}

/* ------------------------------------------------------------ */
/* Registry: this module's tests, in the order -#test lists them. */
/* ------------------------------------------------------------ */

const struct selftest_entry selftests_mime[] = {
    {"mime", "<filename>", "MIME type for a filename", st_mime},
    {"assumemime", "[128|256|1024]",
     "--assume value clipped to each MIME destination", st_assumemime},
    {"sniff", "<content-type> <hex:..|text>", "MIME magic consistency",
     st_sniff},
    {NULL, NULL, NULL, NULL},
};
