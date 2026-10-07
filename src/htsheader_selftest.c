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
/* File: htsheader_selftest.c subroutines:                      */
/*       self-tests for HTTP headers and content codings        */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htsselftest_int.h"

#include <limits.h>
#include <stdint.h>

/* The launch gate: a hold withholds launches, the cap decides how long, and a
   stop outranks both. 77 to skip when no slot table could be allocated. */
static int st_retryafter_gate(httrackp *opt) {
  struct_back *sback = back_new(opt, 1);
  const hts_log_type quiet = opt->debug;
  int err = 0;

  if (sback == NULL) {
    printf("retry-after: SKIP (no slot table)\n");
    return 77;
  }
  /* the arming below logs, and the test driver exact-matches the output */
  opt->debug = LOG_PANIC;
  opt->maxsoc = 4;
  opt->maxconn = 0;
  opt->pause_max_ms = 0;
  opt->max_retry_after = 60;
  opt->state.stop = 0;

#define GATE_CHECK(cond)                                                       \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("retry-after: FAIL line %d: %s\n", __LINE__, #cond);              \
      err = 1;                                                                 \
    }                                                                          \
  } while (0)

  GATE_CHECK(back_pluggable_sockets_strict(sback, opt) > 0);

  back_set_retry_after(sback, opt, 30);
  GATE_CHECK(sback->retry_after_until != 0);
  /* the ask is in seconds: a hold scaled in milliseconds would be up already */
  GATE_CHECK(sback->retry_after_until - mtime_local() > 25 * 1000);
  GATE_CHECK(back_pluggable_sockets_strict(sback, opt) == 0);

  /* a shorter delay must not cut a hold already running */
  {
    const TStamp held = sback->retry_after_until;

    back_set_retry_after(sback, opt, 1);
    GATE_CHECK(sback->retry_after_until == held);
  }

  /* the cap, not the server, decides: 100000s clips to opt->max_retry_after.
     The upper bounds sit far above what was armed because mtime_local() is the
     wall clock: a scale error at least doubles the hold, so slack costs no kill
     and a backward clock step cannot red them. */
  GATE_CHECK(sback->retry_after_until - mtime_local() <= 45 * 1000);
  back_set_retry_after(sback, opt, 100000);
  GATE_CHECK(sback->retry_after_until - mtime_local() > 55 * 1000);
  GATE_CHECK(sback->retry_after_until - mtime_local() <= 90 * 1000);

  /* a stop outranks the hold, or the user's Ctrl-C waits out the delay */
  opt->state.stop = 1;
  GATE_CHECK(back_pluggable_sockets_strict(sback, opt) > 0);
  opt->state.stop = 0;
  GATE_CHECK(back_pluggable_sockets_strict(sback, opt) == 0);

  /* a cap of zero waives the wait entirely */
  sback->retry_after_until = 0;
  opt->max_retry_after = 0;
  back_set_retry_after(sback, opt, 30);
  GATE_CHECK(sback->retry_after_until == 0);
  GATE_CHECK(back_pluggable_sockets_strict(sback, opt) > 0);

  /* --pause is the gate's other delay, and a stop outranks it the same way */
  {
    const TStamp connect_was = HTS_STAT.last_connect;
    const TStamp request_was = HTS_STAT.last_request;
    const int pause_min_was = opt->pause_min_ms;

    opt->pause_min_ms = opt->pause_max_ms = 60 * 1000;
    HTS_STAT.last_connect = mtime_local();
    HTS_STAT.last_request = 0;
    GATE_CHECK(back_pluggable_sockets_strict(sback, opt) == 0);
    opt->state.stop = 1;
    GATE_CHECK(back_pluggable_sockets_strict(sback, opt) > 0);
    opt->state.stop = 0;
    GATE_CHECK(back_pluggable_sockets_strict(sback, opt) == 0);
    opt->pause_min_ms = pause_min_was;
    opt->pause_max_ms = 0;
    HTS_STAT.last_connect = connect_was;
    HTS_STAT.last_request = request_was;
  }

#undef GATE_CHECK
  opt->debug = quiet;
  back_free(&sback);
  return err;
}

static int st_retryafter(httrackp *opt, int argc, char **argv) {
  /* 1994-11-06 08:49:37 GMT, the RFC's own example date */
  const time_t ref = (time_t) 784111777;
  int err = 0;
  size_t i;

  static const struct {
    const char *value;
    int expect;
  } cases[] = {
      /* delta-seconds */
      {"120", 120},
      {" 120", 120},
      {"120\r\n", 120},
      {"0", 0},
      /* clipped, never wrapped negative; INT_MAX itself must survive intact */
      {"2147483640", 2147483640}, /* just under: clipping it is an off-by-one */
      {"2147483647", INT_MAX},
      {"2147483648", INT_MAX},
      {"4294967296", INT_MAX},
      {"99999999999999999999", INT_MAX},
      /* HTTP-date, relative to ref */
      {"Sun, 06 Nov 1994 08:50:37 GMT", 60},
      {"Sun, 06 Nov 1994 08:49:37 GMT", 0},
      {"Sun, 06 Nov 1994 08:48:37 GMT", 0}, /* already past, never negative */
      /* nothing usable: -1, which is what keeps a bare 503 fatal */
      {"", -1},
      {"   ", -1},
      {"soon", -1},
      {"12x", -1},
      {"-5", -1},
      {"1.5", -1},
  };

  (void) argc;
  (void) argv;
  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    const int got = hts_parse_retry_after(cases[i].value, ref);

    if (got != cases[i].expect) {
      printf("retry-after: \"%s\" gave %d, expected %d\n", cases[i].value, got,
             cases[i].expect);
      err = 1;
    }
  }
  if (hts_parse_retry_after(NULL, ref) != -1)
    err = 1;
  {
    const int gate = st_retryafter_gate(opt);

    if (gate == 77)
      return 77;
    err |= gate;
  }
  printf("retry-after: %s\n", err ? "FAIL" : "OK");
  return err;
}

/* Extra args are key=value: adr= cdispo= statuscode= status= strip= urlhack=
   no-www= no-slash= no-query= n83= type=, plus repeatable prior=adr|fil|sav
   registering an already-crawled link (dedup/collision paths). */
/* Parse raw response-header lines and print the naming-relevant fields. */
static int st_header(httrackp *opt, int argc, char **argv) {
  htsblk r;
  int i;

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "header: needs at least one raw header line\n");
    return 1;
  }
  memset(&r, 0, sizeof(r));
  for (i = 0; i < argc; i++) {
    char BIGSTK line[HTS_URLMAXSIZE * 2];

    strcpybuff(line, argv[i]);
    treathead(NULL, "www.example.com", "/", &r, line);
  }
  printf("contenttype=%s cdispo=%s\n", r.contenttype, r.cdispo);
  printf("contentencoding=%s\n", r.contentencoding);
  return 0;
}

/* Prints the code hts_status_line_code() reads from argv[0]. */
static int st_statusline(httrackp *opt, int argc, char **argv) {
  (void) opt;
  if (argc != 1) {
    fprintf(stderr, "statusline: needs one status line\n");
    return 1;
  }
  printf("code=%d\n", hts_status_line_code(argv[0]));
  return 0;
}

/* A header line that does not fit must be reported as cut and skipped whole,
   so the line after it is the server's next header and not its own tail. The
   block defaults to two headers and the blank line; \n and \r arrive escaped,
   since the command line filters both out. */
static int st_headerline(httrackp *opt, int argc, char **argv) {
  static const char fixture[] = "X-First: 0123456789abcdefghij\r\n"
                                "X-Second: ok\r\n"
                                "\r\n";
  char block[256], line[64];
  size_t blocklen;
  int max, ptr = 0, i;

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "headerline: needs a read size\n");
    return 1;
  }
  max = atoi(argv[0]);
  if (max < 2 || (size_t) max > sizeof(line)) {
    fprintf(stderr, "headerline: read size out of probe range\n");
    return 1;
  }
  if (argc < 2) {
    memcpy(block, fixture, sizeof(fixture));
    blocklen = sizeof(fixture) - 1;
  } else {
    const char *a = argv[1];

    if (strlen(a) >= sizeof(block)) {
      fprintf(stderr, "headerline: block too long\n");
      return 1;
    }
    for (blocklen = 0; *a != '\0'; a++) {
      if (*a == '\\' && a[1] == 'n')
        block[blocklen++] = '\n', a++;
      else if (*a == '\\' && a[1] == 'r')
        block[blocklen++] = '\r', a++;
      else
        block[blocklen++] = *a;
    }
    block[blocklen] = '\0';
  }
  /* one read past the block's lines, to show the walk stops at its end */
  for (i = 0; i < 4; i++) {
    int adv;
    const hts_boolean cut =
        binput_line(block + ptr, block + blocklen, line, max, &adv);

    ptr += adv;
    printf("cut=%d over=%d line=%s\n", cut != HTS_FALSE,
           ptr > (int) blocklen + 1, line);
  }
  return 0;
}

/* #1294: binput()'s advance must reach the next line even when it clipped the
   value, since the cache, robots and list parsers resume on it. */
static int st_binputline(httrackp *opt, int argc, char **argv) {
  static const char fixture[] = "Disallow: 0123456789abcdefghij\n"
                                "Allow: /open/\n";
  char block[256], line[64];
  size_t blocklen;
  int max, ptr = 0, i;

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "binputline: needs a read size\n");
    return 1;
  }
  max = atoi(argv[0]);
  if (max < 1 || (size_t) max >= sizeof(line)) {
    fprintf(stderr, "binputline: read size out of probe range\n");
    return 1;
  }
  if (argc < 2) {
    memcpy(block, fixture, sizeof(fixture));
    blocklen = sizeof(fixture) - 1;
  } else {
    const char *a = argv[1];

    if (strlen(a) >= sizeof(block)) {
      fprintf(stderr, "binputline: block too long\n");
      return 1;
    }
    for (blocklen = 0; *a != '\0'; a++) {
      if (*a == '\\' && a[1] == 'n')
        block[blocklen++] = '\n', a++;
      else if (*a == '\\' && a[1] == 'r')
        block[blocklen++] = '\r', a++;
      else
        block[blocklen++] = *a;
    }
    block[blocklen] = '\0';
  }
  /* one read past the block, to show the walk stops on its terminating NUL */
  for (i = 0; i < 3; i++) {
    const int adv = binput(block + ptr, line, max);

    printf("adv=%d line=%s\n", adv, line);
    ptr += adv;
    if (ptr > (int) blocklen)
      ptr = (int) blocklen;
  }
  return 0;
}

static int st_location_logged = 0;

static HTS_PRINTF_FUN(3, 0) void st_location_log(httrackp *opt, int type,
                                                 const char *format,
                                                 va_list args) {
  (void) opt;
  (void) type;
  (void) format;
  (void) args;
  st_location_logged++;
}

/* treathead's Location gate must match htsblk.location's buffer size: a
   redirect that fits is kept whole, a longer one refused and reported, and
   neither touches the canary behind the buffer. */
static int st_location(httrackp *opt, int argc, char **argv) {
  struct {
    char loc[HTS_LOCATION_SIZE];
    char canary[64];
  } probe;

  char BIGSTK line[HTS_LOCATION_SIZE + 1024];
  const char *const prefix = "http://www.example.com/";
  const char *url, *want;
  htsblk r;
  size_t n, urllen, kept, i;
  int asked, smashed = 0;

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "location: needs a Location URL length\n");
    return 1;
  }
  /* subtract, never add: a negative argument must not wrap the range check */
  asked = atoi(argv[0]);
  if (asked <= (int) strlen(prefix) ||
      (size_t) asked > sizeof(line) - sizeof("Location: ")) {
    fprintf(stderr, "location: length out of probe range\n");
    return 1;
  }
  urllen = (size_t) asked;

  memset(probe.loc, 0, sizeof(probe.loc));
  memset(probe.canary, '#', sizeof(probe.canary));
  memset(&r, 0, sizeof(r));
  r.location = probe.loc;

  n = (size_t) snprintf(line, sizeof(line), "Location: %s", prefix);
  memset(line + n, 'a', urllen - strlen(prefix));
  line[n + urllen - strlen(prefix)] = '\0';
  url = line + n - strlen(prefix);

  st_location_logged = 0;
  hts_set_log_vprint_callback(st_location_log);
  treathead(NULL, "www.example.com", "/", &r, line);
  hts_set_log_vprint_callback(NULL);

  for (i = 0; i < sizeof(probe.canary); i++) {
    if (probe.canary[i] != '#')
      smashed++;
  }
  /* bounded: an unterminated buffer reports capacity, never reads past it */
  for (kept = 0; kept < sizeof(probe.loc) && probe.loc[kept] != '\0'; kept++)
    ;
  /* the wanted value comes from the contract, not from what treathead did */
  want = urllen < HTS_LOCATION_SIZE ? url : "";
  printf("asked=%d kept=%d value_ok=%d canary_smashed=%d logged=%d\n", asked,
         (int) kept, strcmp(probe.loc, want) == 0, smashed,
         st_location_logged != 0);
  return 0;
}

/* An over-long header value must not overflow treathead's tempo[1100]. */
static int st_headerlong(httrackp *opt, int argc, char **argv) {
  htsblk r;
  char BIGSTK line[HTS_URLMAXSIZE * 2];
  const char *const name = argc >= 1 ? argv[0] : "Content-Type:";
  const int pad = 1500; /* > tempo[1100] */
  size_t n;

  (void) opt;
  memset(&r, 0, sizeof(r));
  n = (size_t) snprintf(line, sizeof(line), "%s ", name);
  memset(line + n, 'a', pad);
  line[n + pad] = '\0';
  treathead(NULL, "www.example.com", "/", &r, line);
  printf("contenttype_len=%d contentencoding_len=%d\n",
         (int) strlen(r.contenttype), (int) strlen(r.contentencoding));
  return 0;
}

/* Does the custom request-header block already carry <field>? (#1337) */
static int st_headerfield(httrackp *opt, int argc, char **argv) {
  char *headers, *field;

  (void) opt;
  if (argc < 2) {
    fprintf(stderr, "headerfield: needs a header block and a field name\n");
    return 1;
  }
  /* exact-size heap copies so a sanitizer traps an over-read */
  headers = strdupt(argv[0]);
  field = strdupt(argv[1]);
  printf("%s\n",
         http_headers_have_field(headers, field) ? "present" : "absent");
  freet(headers);
  freet(field);
  return 0;
}

/* Drive one http_xfread1 call. entered is the buffer the case came in with, so
   a guard that allocates before refusing shows up. */
static void st_xfread_case(const char *name, htsblk *r, int bufl,
                           const char *entered) {
  /* Separate statements: as printf arguments a compiler may sample r->adr
     before the call, where no allocation can show. */
  const LLint ret = http_xfread1(r, bufl);
  const char *const adr = r->adr;
  const int code = r->statuscode;
  const char *state;

  if (adr == NULL)
    state = "null";
  else if (adr == entered)
    state = "kept";
  else
    state = "alloc";
  printf("%s: refused=%d adr=%s code=%d msg=%s\n", name,
         ret == READ_ERROR ? 1 : 0, state, code, r->msg);
}

/* Drive back_set_decoded_size() on a slot still holding its coded body. */
static void st_decode_size_case(const char *name, int is_write, LLint size) {
  htsblk r;
  hts_boolean ok;
  const char *state;
  LLint committed;

  memset(&r, 0, sizeof(r));
  r.soc = INVALID_SOCKET;
  r.is_write = (short int) is_write;
  r.adr = (char *) malloct(64);
  r.size = 64;
  ok = back_set_decoded_size(&r, size);
  state = r.adr != NULL ? "kept" : "dropped";
  committed = r.size;
  printf("%s: ok=%d body=%s size=" LLintP " code=%d msg=%s\n", name,
         ok == HTS_TRUE ? 1 : 0, state, committed, r.statuscode, r.msg);
  freet(r.adr);
}

/* http_xfread1 must refuse an in-memory buffer whose size would exceed a 32-bit
   index (hostile Content-Length or endless stream) rather than allocate it.
   The guard returns before any socket read, so no real connection is needed.
   The same bound must hold on the two paths that fill r->adr without it: a
   chunk stream, and a decoded content coding. */
static int st_xfread_limit(httrackp *opt, int argc, char **argv) {
  static const LLint sizes[] = {
      0, 1000, (LLint) INT32_MAX - 1, (LLint) INT32_MAX, (LLint) INT32_MAX + 1,
      -1};
  htsblk r;
  size_t i;

  (void) opt;

  // Content-Length just over 2 GiB.
  memset(&r, 0, sizeof(r));
  r.soc = INVALID_SOCKET;
  r.totalsize = (LLint) INT32_MAX + 1;
  st_xfread_case("bylen", &r, 8192, NULL);
  freet(r.adr);

  // 4 GiB: truncated to an int this length reads as 0.
  memset(&r, 0, sizeof(r));
  r.soc = INVALID_SOCKET;
  r.totalsize = (LLint) 1 << 32;
  st_xfread_case("bylen4g", &r, 8192, NULL);
  freet(r.adr);

  // Unknown length, buffer already at the limit: the next read would exceed it.
  memset(&r, 0, sizeof(r));
  r.soc = INVALID_SOCKET;
  r.totalsize = -1;
  r.size = (LLint) INT32_MAX;
  st_xfread_case("bygrow", &r, 8192, NULL);
  freet(r.adr);

  // Exactly at the 2 GiB index (size + bufl == INT32_MAX): must also be
  // refused, since the reallocs below add 1 (a `> INT32_MAX` check would let
  // this through and overflow the int realloc size).
  memset(&r, 0, sizeof(r));
  r.soc = INVALID_SOCKET;
  r.totalsize = -1;
  r.size = (LLint) INT32_MAX - 8192;
  st_xfread_case("boundary", &r, 8192, NULL);
  freet(r.adr);

  /* Same size, entering with a buffer already held: the guard runs per call,
     not on the first allocation only, or the realloc it lets through takes an
     int that has already wrapped negative. */
  memset(&r, 0, sizeof(r));
  r.soc = INVALID_SOCKET;
  r.totalsize = -1;
  r.size = (LLint) INT32_MAX - 8192;
  r.adr = (char *) malloct(64);
  r.adr[0] = '\0';
  st_xfread_case("grown", &r, 8192, r.adr);
  freet(r.adr);

  // A legitimate small size must NOT be refused, and must still be allocated.
  memset(&r, 0, sizeof(r));
  r.soc = INVALID_SOCKET;
  r.totalsize = 1000;
  st_xfread_case("accept", &r, 8192, NULL);
  freet(r.adr);

  /* Control for the unknown-length arm: an undeclared length is not itself a
     reason to refuse, only the size the stream has reached. */
  memset(&r, 0, sizeof(r));
  r.soc = INVALID_SOCKET;
  r.totalsize = -1;
  st_xfread_case("accept_grow", &r, 8192, NULL);
  freet(r.adr);

  // The bound itself, which the chunk and decode paths share.
  for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
    printf("fits(" LLintP ")=%d\n", sizes[i],
           hts_inmem_size_fits(sizes[i]) ? 1 : 0);

  /* hts_codec_maxout() caps a decoded body at INT_MAX inclusive, so a content
     coding reaches this with the one size the receive guard refuses. */
  st_decode_size_case("decode_mem_max", 0, (LLint) INT32_MAX);
  st_decode_size_case("decode_mem_ok", 0, (LLint) INT32_MAX - 1);
  st_decode_size_case("decode_disk_max", 1, (LLint) INT32_MAX);

  /* Given a file at or past the bound, the reader must refuse on the stat that
     would size its own allocation, so nothing appended after a caller's own
     check can carry the body past it. The second file is the control: a bound
     that refuses everything reads the same as one that works. */
  for (i = 0; (int) i < argc && i < 2; i++) {
    LLint got = -1;
    char *adr = readfile2_inmem(argv[i], &got);

    printf("%s: adr=%s size=" LLintP "\n", i == 0 ? "bigfile" : "smallfile",
           adr != NULL ? "alloc" : "null", got);
    freet(adr);
  }
  return 0;
}

/* Parse a Content-Range header and print the sanitized triple. A hostile value
   (negative or INT64 extreme) must clamp to 0 without signed-overflow UB. */
static int st_crange(httrackp *opt, int argc, char **argv) {
  int i;

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "crange: needs at least one raw Content-Range line\n");
    return 1;
  }
  for (i = 0; i < argc; i++) {
    htsblk r;
    char BIGSTK line[HTS_URLMAXSIZE * 2];

    memset(&r, 0, sizeof(r));
    strcpybuff(line, argv[i]);
    treathead(NULL, "www.example.com", "/", &r, line);
    printf("crange_start=" LLintP " crange_end=" LLintP " crange=" LLintP "\n",
           (LLint) r.crange_start, (LLint) r.crange_end, (LLint) r.crange);
  }
  return 0;
}

/* What the custom header block appends to a request already carrying <request>
   (#1340). CR and LF come back escaped, so a case stays one line. */
static int st_headerdedup(httrackp *opt, int argc, char **argv) {
  const size_t half = 8192;
  char *req, *tmp, *headers;
  const char *appended;
  size_t i, reqlen, blocklen;
  /* Only an exact capacity puts a line's last byte on the buffer's last byte */
  size_t capacity = argc > 2 ? (size_t) atoi(argv[2]) : 2 * half;

  (void) opt;
  if (argc < 2) {
    fprintf(stderr, "headerdedup: needs a request and a header block\n");
    return 1;
  }
  req = malloct(capacity > 2 * half ? capacity : 2 * half);
  tmp = malloct(half);
  reqlen = st_decode_body(argv[0], req, half);
  blocklen = st_decode_body(argv[1], tmp, half);
  /* st_decode_body clips, and a clipped case grades an input nobody wrote */
  if (reqlen + 1 >= half || blocklen + 1 >= half || capacity <= reqlen) {
    fprintf(stderr, "headerdedup: argument longer than %d bytes\n",
            (int) half - 2);
    freet(tmp);
    freet(req);
    return 1;
  }
  appended = req + reqlen;
  /* exact-size copy so a sanitizer traps an over-read of the block */
  headers = strdupt(tmp);
  freet(tmp);
  http_append_custom_headers(req, capacity, headers);
  for (i = 0; appended[i] != '\0'; i++) {
    if (appended[i] == '\r')
      printf("\\r");
    else if (appended[i] == '\n')
      printf("\\n");
    else
      putchar(appended[i]);
  }
  printf("\n");
  freet(headers);
  freet(req);
  return 0;
}

/* Default User-Agent: honest HTTrack token, no resurrected Windows 98. */
static int st_useragent(httrackp *opt, int argc, char **argv) {
  const char *ua = StringBuff(opt->user_agent);
  (void) argc;
  (void) argv;
  assertf(ua != NULL);
  assertf(strcmp(ua, HTS_DEFAULT_USER_AGENT) == 0);
  /* Teeth independent of the macro: honest token + self-identifier, and no
     legacy Mozilla/4.x fake-browser string (rejects the whole relic family). */
  assertf(strstr(ua, "HTTrack/") != NULL);
  assertf(strstr(ua, "+https://www.httrack.com/") != NULL);
  assertf(strstr(ua, "Mozilla/4.") == NULL);
  printf("useragent self-test OK: %s\n", ua);
  return 0;
}

/* HTTP status code -> reason phrase, including the modern 429/451. */
static int st_status(httrackp *opt, int argc, char **argv) {
  const char *s;
  (void) opt;
  (void) argc;
  (void) argv;
  s = infostatuscode_const(429);
  assertf(s != NULL && strcmp(s, "Too Many Requests") == 0);
  s = infostatuscode_const(451);
  assertf(s != NULL && strcmp(s, "Unavailable For Legal Reasons") == 0);
  /* A spot-check of a long-standing code, and an unknown one. */
  s = infostatuscode_const(404);
  assertf(s != NULL && strcmp(s, "Not Found") == 0);
  assertf(infostatuscode_const(799) == NULL);
  printf("status self-test OK\n");
  return 0;
}

/* Which statuses excuse a response that stored no body. */
static int st_nobody(httrackp *opt, int argc, char **argv) {
  /* every status the engine excuses, and near misses that it must not */
  static const struct {
    int code;
    hts_boolean excused;
  } cases[] = {
      {301, HTS_TRUE},  {302, HTS_TRUE},  {303, HTS_TRUE},  {304, HTS_FALSE},
      {305, HTS_FALSE}, {306, HTS_FALSE}, {307, HTS_TRUE},  {308, HTS_TRUE},
      {309, HTS_FALSE}, {300, HTS_FALSE}, {411, HTS_FALSE}, {412, HTS_TRUE},
      {413, HTS_FALSE}, {415, HTS_FALSE}, {416, HTS_TRUE},  {417, HTS_FALSE},
      {200, HTS_FALSE}, {204, HTS_FALSE}, {404, HTS_FALSE}, {500, HTS_FALSE},
  };

  char body[] = "body";
  size_t i;
  htsblk r;
  (void) opt;
  (void) argc;
  (void) argv;

  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    memset(&r, 0, sizeof(r));
    r.statuscode = cases[i].code;
    if (hts_body_missing_unexpectedly(&r) == cases[i].excused) {
      fprintf(stderr, "status %d: expected %s\n", cases[i].code,
              cases[i].excused ? "excused" : "unexpected");
      return 1;
    }
  }

  /* a stored body is never missing, whatever the status */
  memset(&r, 0, sizeof(r));
  r.statuscode = 404;
  r.adr = body;
  assertf(!hts_body_missing_unexpectedly(&r));
  memset(&r, 0, sizeof(r));
  r.statuscode = 404;
  r.is_write = 1;
  assertf(!hts_body_missing_unexpectedly(&r));

  printf("nobody self-test OK\n");
  return 0;
}

/* Deflate src->path at windowBits (16+ gzip, + zlib, - raw); 0 on success. */
static int ae_write_packed(const char *path, int windowBits,
                           const unsigned char *src, size_t len) {
  unsigned char out[8192];
  z_stream strm;
  FILE *f;
  int zerr;

  memset(&strm, 0, sizeof(strm));
  if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, windowBits, 8,
                   Z_DEFAULT_STRATEGY) != Z_OK)
    return 1;
  if ((f = FOPEN(path, "wb")) == NULL) {
    deflateEnd(&strm);
    return 1;
  }
  strm.next_in = (const Bytef *) src;
  strm.avail_in = (uInt) len;
  do {
    size_t n;

    strm.next_out = out;
    strm.avail_out = sizeof(out);
    zerr = deflate(&strm, Z_FINISH);
    n = sizeof(out) - strm.avail_out;
    if (n > 0 && !hts_fwrite_exact(out, n, f)) {
      deflateEnd(&strm);
      fclose(f);
      return 1;
    }
  } while (zerr == Z_OK);
  deflateEnd(&strm);
  fclose(f);
  return (zerr == Z_STREAM_END) ? 0 : 1;
}

/* Forged raw deflate (08 1D) that misdetects as zlib; only fallback decodes */
static int ae_write_collision(const char *path, const unsigned char *src,
                              size_t len) {
  /* block-1 LEN low byte 0x1D: with 0x08, (0x081D)%31==0 */
  const size_t n1 = 29;
  size_t n2, p = 0;
  unsigned char *buf;
  FILE *f;
  int ok;

  if (len < n1 || len - n1 > 0xFFFF)
    return 1;
  n2 = len - n1;
  buf = malloct(10 + len);
  if (buf == NULL)
    return 1;
  buf[p++] = 0x08; /* BFINAL=0, BTYPE=00, forged padding -> zlib CMF nibble */
  buf[p++] = (unsigned char) (n1 & 0xff);
  buf[p++] = (unsigned char) (n1 >> 8);
  buf[p++] = (unsigned char) (~n1 & 0xff);
  buf[p++] = (unsigned char) ((~n1 >> 8) & 0xff);
  memcpy(buf + p, src, n1);
  p += n1;
  buf[p++] = 0x01; /* BFINAL=1, BTYPE=00 */
  buf[p++] = (unsigned char) (n2 & 0xff);
  buf[p++] = (unsigned char) (n2 >> 8);
  buf[p++] = (unsigned char) (~n2 & 0xff);
  buf[p++] = (unsigned char) ((~n2 >> 8) & 0xff);
  memcpy(buf + p, src + n1, n2);
  p += n2;
  f = FOPEN(path, "wb");
  ok = (f != NULL && hts_fwrite_exact(buf, p, f));
  if (f != NULL)
    fclose(f);
  freet(buf);
  return ok ? 0 : 1;
}

/* Write src[0..len) to path as-is; 0 on success. */
static int ae_write_raw(const char *path, const unsigned char *src,
                        size_t len) {
  FILE *const f = FOPEN(path, "wb");
  int ok;

  if (f == NULL)
    return 1;
  ok = hts_fwrite_exact(src, len, f);
  fclose(f);
  return ok ? 0 : 1;
}

/* Compare path's bytes to expect[0..len); 0 if equal. Streams (large files). */
static int ae_check_decoded(const char *path, const unsigned char *expect,
                            size_t len) {
  unsigned char buf[8192];
  FILE *f = FOPEN(path, "rb");
  size_t off = 0, n;

  if (f == NULL)
    return 1;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
    if (n > len - off || memcmp(buf, expect + off, n) != 0) {
      fclose(f);
      return 1;
    }
    off += n;
  }
  fclose(f);
  return (off == len) ? 0 : 1;
}

/* Accept-Encoding (#450): advertise gzip+deflate; both decode (hts_zunpack) */
static int st_acceptencoding(httrackp *opt, int argc, char **argv) {
  const char *off = hts_acceptencoding(HTS_FALSE, HTS_TRUE);
  const char *on = hts_acceptencoding(HTS_TRUE, HTS_FALSE);
  const char *tls = hts_acceptencoding(HTS_TRUE, HTS_TRUE);

  (void) opt;
  assertf(strcmp(off, "identity") == 0);
  assertf(strstr(on, "gzip") != NULL);
  assertf(strstr(on, "deflate") != NULL); /* fails on the old gzip-only list */
  /* br and zstd ride on TLS only, so a cleartext proxy can not be handed a
     coding it may try to rewrite */
  assertf(strstr(on, "br") == NULL && strstr(on, "zstd") == NULL);
  assertf((strstr(tls, ", br") != NULL) == (HTS_USEBROTLI != 0));
  assertf((strstr(tls, "zstd") != NULL) == (HTS_USEZSTD != 0));
  if (argc >= 1) {
    static const int windowBits[] = {16 + MAX_WBITS, MAX_WBITS, -MAX_WBITS};
    const unsigned char small[] =
        "deflate round-trip: HTTrack decodes gzip and deflate alike. "
        "deflate round-trip: HTTrack decodes gzip and deflate alike.";
    const size_t slen = sizeof(small) - 1;
    /* 64 KiB of varied (LCG) bytes: forces the multi-fread loop */
    const size_t blen = 64 * 1024;
    unsigned char *body = malloct(blen);
    uint32_t x = 0x1234567u;
    char inpath[HTS_URLMAXSIZE], outpath[HTS_URLMAXSIZE];
    size_t i;

    assertf(body != NULL);
    for (i = 0; i < blen; i++) {
      x = x * 1103515245u + 12345u;
      body[i] = (unsigned char) (x >> 16);
    }
    /* gzip, zlib (RFC1950) and raw deflate (RFC1951), both small and large. */
    for (i = 0; i < sizeof(windowBits) / sizeof(windowBits[0]); i++) {
      snprintf(inpath, sizeof(inpath), "%s/ae-in-%d.z", argv[0], windowBits[i]);
      snprintf(outpath, sizeof(outpath), "%s/ae-out-%d", argv[0],
               windowBits[i]);
      assertf(ae_write_packed(inpath, windowBits[i], small, slen) == 0);
      assertf(hts_zunpack(inpath, outpath) == (int) slen);
      assertf(ae_check_decoded(outpath, small, slen) == 0);
      assertf(ae_write_packed(inpath, windowBits[i], body, blen) == 0);
      assertf(hts_zunpack(inpath, outpath) == (int) blen);
      assertf(ae_check_decoded(outpath, body, blen) == 0);
    }
    /* Fallback teeth: raw deflate misdetected as zlib; -1 without the retry. */
    snprintf(inpath, sizeof(inpath), "%s/ae-collide.z", argv[0]);
    snprintf(outpath, sizeof(outpath), "%s/ae-collide.out", argv[0]);
    assertf(ae_write_collision(inpath, body, 64) == 0);
    assertf(hts_zunpack(inpath, outpath) == 64);
    assertf(ae_check_decoded(outpath, body, 64) == 0);
    /* Identity fallback (#47): a plain body mislabeled as compressed is kept
       verbatim, small and multi-chunk (> one 8 KiB fread). */
    assertf(ae_write_raw(inpath, small, slen) == 0);
    assertf(hts_zunpack(inpath, outpath) == (int) slen);
    assertf(ae_check_decoded(outpath, small, slen) == 0);
    {
      const size_t ilen = 16 * 1024;
      unsigned char *idbody = malloct(ilen);

      assertf(idbody != NULL);
      for (i = 0; i < ilen; i++)
        idbody[i] = small[i % slen];
      assertf(ae_write_raw(inpath, idbody, ilen) == 0);
      assertf(hts_zunpack(inpath, outpath) == (int) ilen);
      assertf(ae_check_decoded(outpath, idbody, ilen) == 0);
      freet(idbody);
    }
    /* Truncated gzip (CRC+ISIZE cut), zlib (ADLER32 cut) and raw deflate
       must all still fail, not fall back to a verbatim copy. */
    {
      static const struct {
        int wb;
        size_t cut;
      } tr[] = {{16 + MAX_WBITS, 8}, {MAX_WBITS, 4}, {-MAX_WBITS, 5}};

      for (i = 0; i < sizeof(tr) / sizeof(tr[0]); i++) {
        unsigned char z[512];
        size_t zlen;
        FILE *f;

        assertf(ae_write_packed(inpath, tr[i].wb, small, slen) == 0);
        f = FOPEN(inpath, "rb");
        assertf(f != NULL);
        zlen = fread(z, 1, sizeof(z), f);
        fclose(f);
        assertf(zlen > tr[i].cut && zlen < sizeof(z));
        assertf(ae_write_raw(inpath, z, zlen - tr[i].cut) == 0);
        assertf(hts_zunpack(inpath, outpath) < 0);
      }
    }
    freet(body);
  }
  printf("acceptencoding self-test OK: %s\n", on);
  return 0;
}

#if HTS_USEBROTLI
/* No brotli encoder is linked, so the coded bytes are canned: brotli quality 9
   over cc_text, and over 4 MiB of 'A' (14 bytes, ~300000x). */
static const unsigned char cc_br_text[] = {
    0x1b, 0x43, 0x00, 0x00, 0x44, 0xdd, 0x96, 0xea, 0xe8, 0x22, 0xdd, 0x90,
    0xa4, 0x1b, 0x8a, 0xf7, 0x47, 0x0e, 0xc2, 0xc5, 0x3d, 0x09, 0x1b, 0x70,
    0xe0, 0x1e, 0x60, 0xa0, 0x8b, 0xcc, 0xbe, 0xcb, 0xb0, 0x31, 0x76, 0x9e,
    0xcf, 0x6e, 0x41, 0xb5, 0xe8, 0x2e, 0x56, 0x78, 0x08, 0x1b, 0xfa, 0x08,
    0x8a, 0x50, 0x83, 0x4e, 0x62, 0x7f, 0xbf, 0x05, 0xf2, 0x22, 0x8f, 0xdf,
    0x28, 0xdc, 0x9f, 0xa9, 0x90, 0x50, 0x37, 0x62, 0x56, 0x4f, 0xa8};
static const unsigned char cc_br_bomb[] = {0x9b, 0xff, 0xff, 0x3f, 0x00,
                                           0x24, 0x82, 0xe2, 0xb1, 0x40,
                                           0x72, 0xef, 0x7f, 0x00};
#endif

#if HTS_USEBROTLI || HTS_USEZSTD
static const unsigned char cc_text[] =
    "content codings: HTTrack decodes gzip, brotli and zstd bodies alike.";
#endif

/* Content codings: br and zstd decode, junk tokens stay identity, a coding we
   can not undo fails the fetch, and a bomb never lands on disk. */
static int st_contentcodings(httrackp *opt, int argc, char **argv) {
#if HTS_USEBROTLI || HTS_USEZSTD
  const size_t tlen = sizeof(cc_text) - 1;
#endif
  char inpath[HTS_URLMAXSIZE], outpath[HTS_URLMAXSIZE];

  (void) opt;
  assertf(hts_codec_parse("gzip") == HTS_CODEC_DEFLATE);
  assertf(hts_codec_parse("x-deflate") == HTS_CODEC_DEFLATE);
  assertf(hts_codec_parse("") == HTS_CODEC_IDENTITY);
  assertf(hts_codec_parse("identity") == HTS_CODEC_IDENTITY);
  /* servers do label plain bodies with junk; the page must survive that */
  assertf(hts_codec_parse("utf-8") == HTS_CODEC_IDENTITY);
  /* a real coding with no decoder here: fail, never save the coded bytes */
  assertf(hts_codec_parse("compress") == HTS_CODEC_UNSUPPORTED);
  assertf(hts_codec_parse("br") ==
          (HTS_USEBROTLI ? HTS_CODEC_BROTLI : HTS_CODEC_UNSUPPORTED));
  assertf(hts_codec_parse("zstd") ==
          (HTS_USEZSTD ? HTS_CODEC_ZSTD : HTS_CODEC_UNSUPPORTED));
  assertf(hts_codec_is_archive_ext(HTS_CODEC_DEFLATE, "tgz"));
  assertf(!hts_codec_is_archive_ext(HTS_CODEC_DEFLATE, "html"));
  assertf(hts_codec_is_archive_ext(HTS_CODEC_BROTLI, "br"));
  assertf(hts_codec_is_archive_ext(HTS_CODEC_ZSTD, "zst"));
  /* decoded-size budget: 4096x, floor 1 MiB, ceiling INT_MAX */
  assertf(hts_codec_maxout(1) == 1024 * 1024);
  assertf(hts_codec_maxout(1024) == 4096 * 1024);
  assertf(hts_codec_maxout(1024 * 1024) == INT_MAX);

  if (argc >= 1) {
    snprintf(inpath, sizeof(inpath), "%s/cc-in", argv[0]);
    snprintf(outpath, sizeof(outpath), "%s/cc-out", argv[0]);
#if HTS_USEBROTLI
    {
      unsigned char head[16];

      assertf(ae_write_raw(inpath, cc_br_text, sizeof(cc_br_text)) == 0);
      assertf(hts_codec_unpack(HTS_CODEC_BROTLI, inpath, outpath) ==
              (int) tlen);
      assertf(ae_check_decoded(outpath, cc_text, tlen) == 0);
      assertf(hts_codec_head(HTS_CODEC_BROTLI, cc_br_text, sizeof(cc_br_text),
                             head, sizeof(head)) == sizeof(head));
      assertf(memcmp(head, cc_text, sizeof(head)) == 0);
      /* truncated: must fail, not fall back to a verbatim copy */
      assertf(ae_write_raw(inpath, cc_br_text, sizeof(cc_br_text) - 4) == 0);
      assertf(hts_codec_unpack(HTS_CODEC_BROTLI, inpath, outpath) < 0);
      /* cc_br_bomb is a valid stream that expands to 4 MiB; the budget, not a
         corrupt frame, is what must reject it. */
      assertf((LLint) (4 * 1024 * 1024) >
              hts_codec_maxout((LLint) sizeof(cc_br_bomb)));
      assertf(ae_write_raw(inpath, cc_br_bomb, sizeof(cc_br_bomb)) == 0);
      assertf(hts_codec_unpack(HTS_CODEC_BROTLI, inpath, outpath) < 0);
    }
#endif
#if HTS_USEZSTD
    {
      const size_t bomblen = 4 * 1024 * 1024;
      const size_t bound = ZSTD_compressBound(bomblen);
      unsigned char *bomb = malloct(bomblen);
      unsigned char *zbuf = malloct(bound);
      unsigned char head[16];
      size_t zlen;

      assertf(bomb != NULL && zbuf != NULL);
      zlen = ZSTD_compress(zbuf, bound, cc_text, tlen, 6);
      assertf(!ZSTD_isError(zlen));
      assertf(ae_write_raw(inpath, zbuf, zlen) == 0);
      assertf(hts_codec_unpack(HTS_CODEC_ZSTD, inpath, outpath) == (int) tlen);
      assertf(ae_check_decoded(outpath, cc_text, tlen) == 0);
      assertf(hts_codec_head(HTS_CODEC_ZSTD, zbuf, zlen, head, sizeof(head)) ==
              sizeof(head));
      assertf(memcmp(head, cc_text, sizeof(head)) == 0);
      assertf(ae_write_raw(inpath, zbuf, zlen - 4) == 0);
      assertf(hts_codec_unpack(HTS_CODEC_ZSTD, inpath, outpath) < 0);
      memset(bomb, 'A', bomblen);
      zlen = ZSTD_compress(zbuf, bound, bomb, bomblen, 6);
      assertf(!ZSTD_isError(zlen));
      /* the fixture must really be past the budget, whatever the ratio is */
      assertf((LLint) bomblen > hts_codec_maxout((LLint) zlen));
      assertf(ae_write_raw(inpath, zbuf, zlen) == 0);
      assertf(hts_codec_unpack(HTS_CODEC_ZSTD, inpath, outpath) < 0);
      freet(bomb);
      freet(zbuf);
    }
#endif
    /* a coding we can not undo yields no file at all */
    assertf(hts_codec_unpack(HTS_CODEC_UNSUPPORTED, inpath, outpath) < 0);
  }
  printf("contentcodings self-test OK\n");
  return 0;
}

/* The errno contract the disk-full classification rests on: a decode that
   cannot write leaves the write's errno, a body the decoder refuses leaves 0
   (#1398). Both are poisoned first, so a caller that never sets errno fails. */
static int st_codecerrno(httrackp *opt, int argc, char **argv) {
  static const unsigned char body[] = "codec errno contract, round-tripped";
  const size_t len = sizeof(body) - 1;
  char inpath[HTS_URLMAXSIZE], outpath[HTS_URLMAXSIZE];
  FILE *fp;

  (void) opt;
  if (argc < 2) {
    fprintf(stderr, "usage: -#test=codecerrno <dir> <doomed-out>\n");
    return 1;
  }
  snprintf(inpath, sizeof(inpath), "%s/ce-in.z", argv[0]);
  snprintf(outpath, sizeof(outpath), "%s/ce-out", argv[0]);
  assertf(ae_write_packed(inpath, 16 + MAX_WBITS, body, len) == 0);
  errno = ENOSPC;
  assertf(hts_zunpack(inpath, outpath) == (int) len);
  errno = 0;
  assertf(hts_zunpack(inpath, argv[1]) == -1);
  assertf(errno == ENOSPC);
  errno = 0;
  assertf(hts_codec_unpack(HTS_CODEC_DEFLATE, inpath, argv[1]) == -1);
  assertf(errno == ENOSPC);
  errno = ENOSPC;
  assertf(hts_codec_unpack(HTS_CODEC_UNSUPPORTED, inpath, outpath) == -1);
  assertf(errno == 0);
  /* Past the gzip magic, so the header still parses and the identity fallback
     stays out of it: the stream itself is what fails. */
  fp = FOPEN(inpath, "r+b");
  assertf(fp != NULL);
  assertf(fseek(fp, 12, SEEK_SET) == 0);
  assertf(hts_fwrite_exact("\xff\xff\xff\xff\xff\xff\xff\xff", 8, fp));
  assertf(fclose(fp) == 0);
  errno = ENOSPC;
  assertf(hts_zunpack(inpath, outpath) == -1);
  assertf(errno == 0);
  errno = ENOSPC;
  assertf(hts_codec_unpack(HTS_CODEC_DEFLATE, inpath, outpath) == -1);
  assertf(errno == 0);
#if HTS_USEBROTLI
  /* and a backend of its own, where the write goes through codec_sink() */
  assertf(ae_write_raw(inpath, cc_br_text, sizeof(cc_br_text)) == 0);
  errno = 0;
  assertf(hts_codec_unpack(HTS_CODEC_BROTLI, inpath, argv[1]) == -1);
  assertf(errno == ENOSPC);
  assertf(ae_write_raw(inpath, cc_br_text, sizeof(cc_br_text) - 4) == 0);
  errno = ENOSPC;
  assertf(hts_codec_unpack(HTS_CODEC_BROTLI, inpath, outpath) == -1);
  assertf(errno == 0);
#endif
  printf("codecerrno: write failure kept, refused stream cleared\n");
  return 0;
}

/* The library's own view of the structs HTS_INET6 and HTS_USEOPENSSL shape.
   381 diffs it against a consumer compiled from the installed headers alone. */
static int st_pubheaders(httrackp *opt, int argc, char **argv) {
  (void) opt;
  (void) argc;
  (void) argv;
#define PUBH_SIZE(t) printf("sizeof %s = %lu\n", #t, (unsigned long) sizeof(t))
#define PUBH_OFF(t, f)                                                         \
  printf("offsetof %s.%s = %lu\n", #t, #f, (unsigned long) offsetof(t, f))
  PUBH_SIZE(SOCaddr);
  PUBH_SIZE(INTsys);
  PUBH_SIZE(htsblk);
  PUBH_OFF(htsblk, soc);
  PUBH_OFF(htsblk, address);
  PUBH_OFF(htsblk, fp);
  PUBH_OFF(htsblk, lastmodified);
  PUBH_OFF(htsblk, etag);
  PUBH_OFF(htsblk, debugid);
  PUBH_SIZE(httrackp);
  printf("HTS_INET6 = %d\n", (int) HTS_INET6);
  printf("HTS_USEOPENSSL = %d\n", (int) HTS_USEOPENSSL);
#undef PUBH_SIZE
#undef PUBH_OFF
  return 0;
}

/* ------------------------------------------------------------ */
/* Registry: this module's tests, in the order -#test lists them. */
/* ------------------------------------------------------------ */

/* <parse|scan> <text> <min> <max>: what hts_parse_llint() or hts_scan_llint()
   reads from text, and how many bytes it moves past. */
static int st_intparse(httrackp *opt, int argc, char **argv) {
  const char *end = argv[1];
  LLint min, max, value = -1;

  (void) opt;
  if (argc != 4) {
    fprintf(stderr, "int-parse: needs parse|scan <text> <min> <max>\n");
    return 1;
  }
  min = (LLint) strtoll(argv[2], NULL, 10);
  max = (LLint) strtoll(argv[3], NULL, 10);
  if (strcmp(argv[0], "parse") == 0) {
    const hts_boolean ok = hts_parse_llint(argv[1], &end, min, max, &value);

    printf("%s value=" LLintP " end=%d\n", ok ? "ok" : "refused", value,
           (int) (end - argv[1]));
  } else {
    const hts_scan_result got = hts_scan_llint(&end, min, max, &value);

    printf("got=%d value=" LLintP " end=%d\n", (int) got, value,
           (int) (end - argv[1]));
  }
  return 0;
}

/* The differential below: each converted site, as it parsed before (legacy_*,
   a frozen copy using sscanf) and as it parses now, rendered into out. */
typedef void (*st_int_site_fn)(const char *in, char *out, size_t size);

static void st_int_treathead(const char *field, const char *in, htsblk *r) {
  char BIGSTK line[HTS_URLMAXSIZE * 2];

  memset(r, 0, sizeof(*r));
  r->totalsize = -1;
  line[0] = '\0';
  strlncatbuff(line, field, sizeof(line), sizeof(line) - 1);
  strlncatbuff(line, in, sizeof(line), sizeof(line) - 1);
  treathead(NULL, "www.example.com", "/", r, line);
}

static void legacy_clen(const char *in, char *out, size_t size) {
  LLint totalsize = -1;
  int empty = 0;

  if (sscanf(in, LLintP, &totalsize) == 1 && totalsize == 0)
    empty = 1;
  snprintf(out, size, "totalsize=" LLintP " empty=%d", totalsize, empty);
}

static void current_clen(const char *in, char *out, size_t size) {
  htsblk r;

  st_int_treathead("Content-Length:", in, &r);
  snprintf(out, size, "totalsize=" LLintP " empty=%d", (LLint) r.totalsize,
           (int) r.empty);
}

static void legacy_crange(const char *in, char *out, size_t size) {
  LLint crange_start = 0, crange_end = 0, crange = 0;
  const char *a;

  for (a = in; is_space(*a); a++)
    ;
  if (strncasecmp(a, "bytes ", 6) == 0) {
    for (a += 6; is_space(*a); a++)
      ;
    if (sscanf(a, LLintP "-" LLintP "/" LLintP, &crange_start, &crange_end,
               &crange) != 3) {
      crange_start = 0;
      crange_end = 0;
      crange = 0;
      a = strchr(in, '/');
      if (a != NULL) {
        a++;
        if (sscanf(a, LLintP, &crange) == 1 && crange >= 0) {
          crange_start = 0;
          crange_end = crange - 1;
        } else {
          crange = 0;
        }
      }
    }
    if (crange_start < 0 || crange_end < 0 || crange < 0)
      crange_start = crange_end = crange = 0;
  }
  snprintf(out, size, LLintP " " LLintP " " LLintP, crange_start, crange_end,
           crange);
}

static void current_crange(const char *in, char *out, size_t size) {
  htsblk r;

  st_int_treathead("Content-Range:", in, &r);
  snprintf(out, size, LLintP " " LLintP " " LLintP, (LLint) r.crange_start,
           (LLint) r.crange_end, (LLint) r.crange);
}

static void legacy_keepalive(const char *in, char *out, size_t size) {
  int keep_alive = 0, keep_alive_t = 0, keep_alive_max = 0;
  const char *a = in;

  while (is_space(*a))
    a++;
  if (*a) {
    const char *p;

    keep_alive = 1;
    keep_alive_max = 10;
    keep_alive_t = 15;
    if ((p = strstr(a, "timeout="))) {
      p += strlen("timeout=");
      sscanf(p, "%d", &keep_alive_t);
    }
    if ((p = strstr(a, "max="))) {
      p += strlen("max=");
      sscanf(p, "%d", &keep_alive_max);
    }
    if (keep_alive_max <= 1 || keep_alive_t < 1)
      keep_alive = 0;
  }
  snprintf(out, size, "keep_alive=%d timeout=%d max=%d", keep_alive,
           keep_alive_t, keep_alive_max);
}

static void current_keepalive(const char *in, char *out, size_t size) {
  htsblk r;

  st_int_treathead("Keep-Alive:", in, &r);
  snprintf(out, size, "keep_alive=%d timeout=%d max=%d", r.keep_alive,
           r.keep_alive_t, r.keep_alive_max);
}

static void legacy_ftpsize(const char *in, char *out, size_t size) {
  const char *szstr = strchr(in, ' ');
  LLint value = 0;

  if (szstr != NULL && sscanf(szstr + 1, LLintP, &value) == HTS_SCAN_OK)
    snprintf(out, size, "size=" LLintP, value);
  else
    snprintf(out, size, "none");
}

static void current_ftpsize(const char *in, char *out, size_t size) {
  LLint value = 0;

  if (ftp_parse_size(in, &value))
    snprintf(out, size, "size=" LLintP, value);
  else
    snprintf(out, size, "none");
}

static struct tm *legacy_convert_time_rfc822(struct tm *result, const char *s) {
  char months[] = "jan feb mar apr may jun jul aug sep oct nov dec";
  char str[256];
  char *a;
  int result_mm = -1, result_dd = -1, result_n1 = -1, result_n2 = -1;
  int result_n3 = -1, result_n4 = -1;

  if ((int) strlen(s) > 200)
    return NULL;
  strcpybuff(str, s);
  hts_lowcase(str);
  while ((a = strchr(str, '-')))
    *a = ' ';
  while ((a = strchr(str, ':')))
    *a = ' ';
  while ((a = strchr(str, ',')))
    *a = ' ';
  a = str;
  while (*a) {
    char *first, *last;
    char tok[256];

    while (*a == ' ')
      a++;
    first = a;
    while ((*a) && (*a != ' '))
      a++;
    last = a;
    tok[0] = '\0';
    if (first != last) {
      char *pos;

      strncatbuff(tok, first, (int) (last - first));
      if ((pos = strstr(months, tok))) {
        result_mm = ((int) (pos - months)) / 4;
      } else {
        int number;

        if (sscanf(tok, "%d", &number) == 1) {
          if (result_dd < 0)
            result_dd = number;
          else if (result_n1 < 0)
            result_n1 = number;
          else if (result_n2 < 0)
            result_n2 = number;
          else if (result_n3 < 0)
            result_n3 = number;
          else if (result_n4 < 0)
            result_n4 = number;
        }
      }
    }
  }
  if ((result_n1 >= 0) && (result_mm >= 0) && (result_dd >= 0) &&
      (result_n2 >= 0) && (result_n3 >= 0) && (result_n4 >= 0)) {
    memset(result, 0, sizeof(*result));
    if (result_n4 >= 1000) {
      result->tm_year = result_n4 - 1900;
      result->tm_hour = result_n1;
      result->tm_min = result_n2;
      result->tm_sec = result_n3 > 0 ? result_n3 : 0;
    } else {
      result->tm_hour = result_n2;
      result->tm_min = result_n3;
      result->tm_sec = result_n4 > 0 ? result_n4 : 0;
      if (result_n1 <= 50)
        result->tm_year = result_n1 + 100;
      else if (result_n1 < 1000)
        result->tm_year = result_n1;
      else
        result->tm_year = result_n1 - 1900;
    }
    result->tm_mon = result_mm;
    result->tm_mday = result_dd;
    return result;
  }
  return NULL;
}

static void st_int_print_tm(const struct tm *tm, char *out, size_t size) {
  if (tm == NULL)
    snprintf(out, size, "none");
  else
    snprintf(out, size, "y=%d m=%d d=%d %d:%d:%d", tm->tm_year, tm->tm_mon,
             tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
}

static void legacy_date(const char *in, char *out, size_t size) {
  struct tm tm;

  st_int_print_tm(legacy_convert_time_rfc822(&tm, in), out, size);
}

static void current_date(const char *in, char *out, size_t size) {
  struct tm tm;

  st_int_print_tm(convert_time_rfc822(&tm, in), out, size);
}

/* htsserver's and proxytrack's Content-length, which live outside the
   library: their parse, then what it decides, copied from each program. */
static void st_int_server_read(LLint length, char *out, size_t size) {
  const LLint buffer = 1000; // stands for buffer_size - 2

  snprintf(out, size, "read=" LLintP,
           length > buffer ? buffer
           : length > 0    ? length
                           : 0);
}

static void legacy_server(const char *in, char *out, size_t size) {
  LLint length = 1000;

  sscanf(in, LLintP, &length);
  st_int_server_read(length, out, size);
}

static void current_server(const char *in, char *out, size_t size) {
  const char *a = in;
  LLint length = 1000;

  if (hts_scan_llint(&a, 0, INT64_MAX, &length) == HTS_SCAN_REFUSED)
    length = 0;
  st_int_server_read(length, out, size);
}

static void legacy_proxytrack(const char *in, char *out, size_t size) {
  int length = 0;

  if (sscanf(in, "%d", &length) != 1)
    snprintf(out, size, "error");
  else
    snprintf(out, size, "length=%d", length);
}

static void current_proxytrack(const char *in, char *out, size_t size) {
  const char *a = in;
  LLint value;

  if (hts_scan_llint(&a, 0, INT_MAX, &value) == HTS_SCAN_OK)
    snprintf(out, size, "length=%d", (int) value);
  else
    snprintf(out, size, "error");
}

static void legacy_port(const char *in, char *out, size_t size) {
  char *end;
  long p;

  if (!isdigit((unsigned char) *in) ||
      (p = strtol(in, &end, 10), *end != '\0') || p < 1 || p > 65535)
    snprintf(out, size, "refused");
  else
    snprintf(out, size, "port=%ld", p);
}

static void current_port(const char *in, char *out, size_t size) {
  int port = -1;

  if (hts_parse_url_port(in, &port))
    snprintf(out, size, "port=%d", port);
  else
    snprintf(out, size, "refused");
}

struct st_int_site {
  const char *name;
  st_int_site_fn legacy, current;
  LLint max;                // the site's bound, for the generated allowlist
  hts_boolean sign_read;    // the date parser reads either sign as no sign
  hts_boolean exact;        // no intended change at all
  const char *templates[5]; // generated inputs: one "%s" for the number
};

static const struct st_int_site st_int_sites[] = {
    {"content-length",
     legacy_clen,
     current_clen,
     INT64_MAX,
     HTS_FALSE,
     HTS_FALSE,
     {"%s", " %s", NULL}},
    {"content-range",
     legacy_crange,
     current_crange,
     INT64_MAX,
     HTS_FALSE,
     HTS_FALSE,
     {" bytes %s-9/10", " bytes 0-%s/10", " bytes 0-9/%s", " bytes */%s",
      NULL}},
    {"keep-alive",
     legacy_keepalive,
     current_keepalive,
     INT_MAX,
     HTS_FALSE,
     HTS_FALSE,
     {" timeout=%s, max=100", " timeout=5, max=%s", NULL}},
    {"ftp-size",
     legacy_ftpsize,
     current_ftpsize,
     INT64_MAX,
     HTS_FALSE,
     HTS_FALSE,
     {"213 %s", "213%s", NULL}},
    {"date",
     legacy_date,
     current_date,
     INT_MAX,
     HTS_TRUE,
     HTS_FALSE,
     {"Sun, 06 Nov 1994 08:49:%s GMT", "%s Nov 1994 08:49:37 GMT",
      "Sun Nov  6 08:49:37 %s", "06 Nov 94 08:49 %s", NULL}},
    {"server-length",
     legacy_server,
     current_server,
     INT64_MAX,
     HTS_FALSE,
     HTS_FALSE,
     {" %s", NULL}},
    {"proxytrack-length",
     legacy_proxytrack,
     current_proxytrack,
     INT_MAX,
     HTS_FALSE,
     HTS_FALSE,
     {" %s", NULL}},
    {"url-port",
     legacy_port,
     current_port,
     65535,
     HTS_FALSE,
     HTS_TRUE,
     {"%s", NULL}},
};

/* A NULL want means the case parses as before, and a want names a change. */
static const struct {
  const char *site, *in, *want;
} st_int_cases[] = {
    {"content-length", " 0", NULL},
    {"content-length", " 00123x", NULL},
    {"content-length", "\t123 456", NULL},
    {"content-length", " abc", NULL},
    {"content-length", "", NULL},
    {"content-length", " 9223372036854775807", NULL},
    {"content-length", " -5", "totalsize=-1 empty=0"},
    {"content-length", " +5", "totalsize=-1 empty=0"},
    {"content-length", " -0", "totalsize=-1 empty=0"},
    {"content-length", " 9223372036854775808", "totalsize=-1 empty=0"},
    {"content-range", " bytes 0-70870/70871", NULL},
    {"content-range", " bytes */70871", NULL},
    {"content-range", " bytes 0 - 5/10", NULL},
    {"content-range", " bytes 0- 5/ 10", NULL},
    {"content-range", " bytes -5/10", NULL},
    {"content-range", " bytes -5-10/20", NULL},
    {"content-range", " bytes 0-5", NULL},
    {"content-range", " bytes */0", NULL},
    {"content-range", " bytes */-3", NULL},
    {"content-range", " items 1-2/3", NULL},
    {"content-range", " bytes 0-9223372036854775807/9223372036854775807", NULL},
    {"content-range", " bytes +0-5/10", "0 0 0"},
    {"content-range", " bytes 0-5/9223372036854775808", "0 0 0"},
    {"content-range", " bytes */+10", "0 0 0"},
    {"content-range", " bytes */9223372036854775808", "0 0 0"},
    {"keep-alive", " timeout=5, max=100", NULL},
    {"keep-alive", " timeout=0", NULL},
    {"keep-alive", " max=1", NULL},
    {"keep-alive", " timeout=abc", NULL},
    {"keep-alive", " timeout= 7", NULL},
    {"keep-alive", " xmax=50",
     "keep_alive=1 timeout=15 max=10"}, // whole names only
    {"keep-alive", " timeout=007x", NULL},
    {"keep-alive", " timeout=-1", "keep_alive=0 timeout=0 max=10"},
    {"keep-alive", " timeout=+30", "keep_alive=0 timeout=0 max=10"},
    {"keep-alive", " timeout=4294967311", "keep_alive=0 timeout=0 max=10"},
    {"keep-alive", " max=-1", "keep_alive=0 timeout=15 max=0"},
    {"ftp-size", "213 1234", NULL},
    {"ftp-size", "213  1234", NULL},
    {"ftp-size", "213 12ab", NULL},
    {"ftp-size", "213", NULL},
    {"ftp-size", "213 x", NULL},
    {"ftp-size", "213 9223372036854775807", NULL},
    {"ftp-size", "213 -1", "none"},
    {"ftp-size", "213 +1", "none"},
    {"ftp-size", "213 99999999999999999999", "none"},
    {"date", "Sun, 06 Nov 1994 08:49:37 GMT", NULL},
    {"date", "Sunday, 06-Nov-94 08:49:37 GMT", NULL},
    {"date", "Sun Nov  6 08:49:37 1994", NULL},
    {"date", "Sun, 06 Nov 1994 08:49:37 +0100", NULL},
    {"date", "\t06 Nov 1994 08:49:37", NULL},
    {"date", "garbage", NULL},
    {"date", "06 Nov 94 08:49 +0000", NULL},
    {"date", "Sun, 06 Nov 1994 08:49 +0100", NULL},
    {"date", "Sun, 06 Nov 1994 08:49:37 GMT+1", NULL},
    {"date", "Sun, 06 Nov 1994 08:49:37 \t+1", NULL},
    {"date", "Sun, 06 Nov 1994 08:49:37 ++1", NULL},
    {"date", "4294967302 Nov 1994 08:49:37", "none"},
    {"server-length", " 50", NULL},
    {"server-length", " abc", NULL},
    {"server-length", " -5", NULL},
    {"server-length", " +5", "read=0"},
    {"server-length", " 9223372036854775808", "read=0"},
    {"proxytrack-length", " 50", NULL},
    {"proxytrack-length", " abc", NULL},
    {"proxytrack-length", " -5", "error"},
    {"proxytrack-length", " +5", "error"},
    {"proxytrack-length", " 2147483648", "error"},
    {"url-port", "80", NULL},
    {"url-port", "00080", NULL},
    {"url-port", "65536", NULL},
    {"url-port", "+80", NULL},
    {"url-port", " 80", NULL},
    {"url-port", "80x", NULL},
    {"url-port", "", NULL},
    {"url-port", "99999999999999999999", NULL},
};

/* Copy pattern into out with its one "%s" replaced by value. */
static void st_int_fill(char *out, size_t size, const char *pattern,
                        const char *value) {
  const char *hole = strstr(pattern, "%s");

  out[0] = '\0';
  strlncatbuff(out, pattern, size, (size_t) (hole - pattern));
  strlncatbuff(out, value, size, size - 1);
  strlncatbuff(out, hole + 2, size, size - 1);
}

/* Is the digit run at s larger than max? */
static hts_boolean st_int_oversized(const char *s, LLint max) {
  LLint v;

  return *s != '\0' && !hts_parse_llint(s, NULL, 0, max, &v);
}

/* Every converted site against its legacy copy: the hand cases, then each
   template filled with every space x sign x digits x suffix combination. Only
   a signed or out-of-range number may differ, and the hand cases pin how. */
static int st_intparsediff(httrackp *opt, int argc, char **argv) {
  static const char *const spaces[] = {"", " ", "\t", "  "};
  static const char *const signs[] = {"", "+", "-"};
  static const char *const digits[] = {"",
                                       "0",
                                       "00",
                                       "7",
                                       "42",
                                       "0042",
                                       "65535",
                                       "65536",
                                       "2147483647",
                                       "2147483648",
                                       "4294967295",
                                       "4294967311",
                                       "9223372036854775807",
                                       "9223372036854775808",
                                       "99999999999999999999"};
  static const char *const suffixes[] = {"", "x", " 7", ".5", "/3"};
  size_t i, s, t, w, g, d, x;
  int compared = 0, changed = 0, failures = 0;
  char in[256], old[256], cur[256], gen[128];

  (void) opt;
  if (argc == 2) { // One input through one site, as it parses now.
    for (s = 0; s < sizeof(st_int_sites) / sizeof(st_int_sites[0]); s++)
      if (strcmp(st_int_sites[s].name, argv[0]) == 0) {
        st_int_sites[s].current(argv[1], cur, sizeof(cur));
        printf("%s\n", cur);
        return 0;
      }
    fprintf(stderr, "int-parse-diff: no site \"%s\"\n", argv[0]);
    return 1;
  }
  for (i = 0; i < sizeof(st_int_cases) / sizeof(st_int_cases[0]); i++) {
    for (s = 0; strcmp(st_int_sites[s].name, st_int_cases[i].site) != 0; s++)
      ;
    st_int_sites[s].legacy(st_int_cases[i].in, old, sizeof(old));
    st_int_sites[s].current(st_int_cases[i].in, cur, sizeof(cur));
    // An overflow's legacy result is up to the libc, so a want pins only ours.
    if (strcmp(cur, st_int_cases[i].want != NULL ? st_int_cases[i].want
                                                 : old) != 0) {
      printf("FAIL %s \"%s\": legacy \"%s\" now \"%s\" want \"%s\"\n",
             st_int_sites[s].name, st_int_cases[i].in, old, cur,
             st_int_cases[i].want != NULL ? st_int_cases[i].want : old);
      failures++;
    }
    compared++;
    changed += st_int_cases[i].want != NULL;
  }
  for (s = 0; s < sizeof(st_int_sites) / sizeof(st_int_sites[0]); s++) {
    const struct st_int_site *site = &st_int_sites[s];

    for (t = 0; site->templates[t] != NULL; t++)
      for (w = 0; w < sizeof(spaces) / sizeof(spaces[0]); w++)
        for (g = 0; g < sizeof(signs) / sizeof(signs[0]); g++)
          for (d = 0; d < sizeof(digits) / sizeof(digits[0]); d++)
            for (x = 0; x < sizeof(suffixes) / sizeof(suffixes[0]); x++) {
              const hts_boolean is_signed =
                  *signs[g] != '\0' && *digits[d] != '\0' && !site->sign_read;

              snprintf(gen, sizeof(gen), "%s%s%s%s", spaces[w], signs[g],
                       digits[d], suffixes[x]);
              st_int_fill(in, sizeof(in), site->templates[t], gen);
              if (!site->exact &&
                  (is_signed || st_int_oversized(digits[d], site->max))) {
                changed++;
                continue;
              }
              site->legacy(in, old, sizeof(old));
              site->current(in, cur, sizeof(cur));
              compared++;
              if (strcmp(old, cur) != 0) {
                printf("FAIL %s \"%s\": legacy \"%s\" now \"%s\"\n", site->name,
                       in, old, cur);
                failures++;
              }
            }
  }
  printf("int-parse-diff: %d compared, %d intended changes, %d failures\n",
         compared, changed, failures);
  return failures != 0;
}

/* hts_header_param() on one value: found or not, and what it copied. */
static int st_headerparam(httrackp *opt, int argc, char **argv) {
  char out[256];
  int size;
  hts_boolean found;

  (void) opt;
  if (argc != 4) {
    fprintf(stderr, "header-param: needs <value> <seps> <name> <size>\n");
    return 1;
  }
  size = atoi(argv[3]);
  if (size < 0 || (size_t) size > sizeof(out)) {
    fprintf(stderr, "header-param: size out of probe range\n");
    return 1;
  }
  memset(out, 'X', sizeof(out));
  found = hts_header_param(argv[0], argv[1], argv[2], out, (size_t) size);
  printf("%s [%s]\n", found ? "yes" : "no", size != 0 ? out : "-");
  return 0;
}

/* Each converted treathead site as it parsed before (frozen) and now. */
static void legacy_ctype(const char *in, char *out, size_t size) {
  char line[1024], tempo[1100];
  char contenttype[HTS_MIMETYPE_SIZE] = "", charset[HTS_MIMETYPE_SIZE] = "";
  char *a;

  line[0] = '\0';
  strlncatbuff(line, in, sizeof(line), sizeof(line) - 1);
  a = strchr(line, ';');
  if (a) {
    *a = '\0';
    a++;
    while (is_space(*a))
      a++;
    if (strfield(a, "charset")) {
      a += 7;
      while (is_space(*a))
        a++;
      if (*a == '=') {
        a++;
        while (is_space(*a))
          a++;
        if (*a == '\"')
          a++;
        while (is_space(*a))
          a++;
        if (*a) {
          char *chs = a;

          while (*a && !is_space(*a) && *a != '\"' && *a != ';')
            a++;
          *a = '\0';
          if (*chs && strlen(chs) < sizeof(charset) - 2)
            strcpybuff(charset, chs);
        }
      }
    }
  }
  if (sscanf(line, "%1099s", tempo) == 1 &&
      strlen(tempo) < sizeof(contenttype) - 2)
    strcpybuff(contenttype, tempo);
  snprintf(out, size, "contenttype=%s charset=%s", contenttype, charset);
}

static void current_ctype(const char *in, char *out, size_t size) {
  htsblk r;

  st_int_treathead("Content-Type:", in, &r);
  snprintf(out, size, "contenttype=%s charset=%s", r.contenttype, r.charset);
}

static void legacy_cdispo(const char *in, char *out, size_t size) {
  char cdispo[256] = "";
  const char *v = in;

  while (is_realspace(*v))
    v++;
  if (strlen(v) < 250) {
    char tmp[256];
    char *a;

    strcpybuff(tmp, v);
    a = strstr(tmp, "filename=");
    if (a) {
      char *c;

      a += strlen("filename=");
      while (is_space(*a))
        a++;
      while ((c = strchr(a, '/')))
        a = c + 1;
      hts_rtrim(a, HTS_SPACES);
      if (strlen(a) < 200)
        strcpybuff(cdispo, a);
    }
  }
  snprintf(out, size, "cdispo=%s", cdispo);
}

static void current_cdispo(const char *in, char *out, size_t size) {
  htsblk r;

  st_int_treathead("Content-Disposition:", in, &r);
  snprintf(out, size, "cdispo=%s", r.cdispo);
}

static void legacy_ka_param(const char *s, int *value) {
  LLint v;
  const hts_scan_result got = hts_scan_llint(&s, 0, INT_MAX, &v);

  if (got != HTS_SCAN_NONE)
    *value = got == HTS_SCAN_OK ? (int) v : 0;
}

static void legacy_keepalive_scan(const char *in, char *out, size_t size) {
  int keep_alive = 0, keep_alive_t = 0, keep_alive_max = 0;
  const char *a = in;

  while (is_space(*a))
    a++;
  if (*a) {
    const char *p;

    keep_alive = 1;
    keep_alive_max = 10;
    keep_alive_t = 15;
    if ((p = strstr(a, "timeout=")))
      legacy_ka_param(p + strlen("timeout="), &keep_alive_t);
    if ((p = strstr(a, "max=")))
      legacy_ka_param(p + strlen("max="), &keep_alive_max);
    if (keep_alive_max <= 1 || keep_alive_t < 1)
      keep_alive = 0;
  }
  snprintf(out, size, "keep_alive=%d timeout=%d max=%d", keep_alive,
           keep_alive_t, keep_alive_max);
}

/* Why a generated input may parse differently from before. */
enum {
  HP_LATER = 1,  // the parameter is not the first one
  HP_TRAIL = 2,  // more parameters follow the file name
  HP_SUFFIX = 4, // a longer name ends with the wanted one
  HP_QUOTE = 8,  // a quoted value holds a ';' or a '\' escape
  HP_SPACE = 16, // white space around the '='
  HP_CASE = 32,  // the name is not in lower case
  HP_COUNT = 6
};

static const char *const st_hp_reasons[HP_COUNT] = {"later", "trail", "suffix",
                                                    "quote", "space", "case"};

/* One parameter of the generated corpus. Key is the name it must match as, and
   want what the site then reports, or NULL for a parameter it must skip. */
struct st_hp_item {
  const char *text, *key, *want;
  int why;
};

/* Prefix is what the site prints before the value, or NULL for Keep-Alive. */
struct st_hp_site {
  const char *name, *head, *param, *prefix;
  st_int_site_fn legacy, current;
  const char *seps[4];
  struct st_hp_item items[10];
};

static const struct st_hp_site st_hp_sites[] = {
    {"content-type",
     " text/html",
     "charset",
     "contenttype=text/html charset=",
     legacy_ctype,
     current_ctype,
     {";", "; ", " ; ", NULL},
     {{"charset=utf-8", "charset", "utf-8", 0},
      {"charset=\"iso-8859-1\"", "charset", "iso-8859-1", 0},
      {"CharSet = koi8-r", "charset", "koi8-r", 0},
      {"charset=", "charset", "", 0},
      {"xcharset=bad", NULL, NULL, 0},
      {"q=\"a;charset=bad\"", NULL, NULL, HP_QUOTE},
      {"format=flowed", NULL, NULL, 0},
      {NULL, NULL, NULL, 0}}},
    {"content-disposition",
     " attachment",
     "filename",
     "cdispo=",
     legacy_cdispo,
     current_cdispo,
     {";", "; ", " ; ", NULL},
     {{"filename=a.txt", "filename", "a.txt", 0},
      {"filename=\"b c.txt\"", "filename", "b c.txt", 0},
      {"filename=\"dir/d.txt\"", "filename", "d.txt", 0},
      {"FILENAME=e.txt", "filename", "e.txt", HP_CASE},
      {"filename=\"g\\\"h.txt\"", "filename", "g\"h.txt", HP_QUOTE},
      {"filename*=UTF-8''f.txt", NULL, NULL, 0},
      {"xfilename=x.txt", NULL, NULL, HP_SUFFIX},
      {"name=\"q;filename=bad\"", NULL, NULL, HP_QUOTE},
      {"size=3", NULL, NULL, 0},
      {NULL, NULL, NULL, 0}}},
    {"keep-alive",
     "",
     NULL,
     NULL,
     legacy_keepalive_scan,
     current_keepalive,
     {",", ", ", ";", NULL},
     {{"timeout=5", "timeout", "5", 0},
      {"max=100", "max", "100", 0},
      {"max=1", "max", "1", 0},
      {"timeout = 7", "timeout", "7", HP_SPACE},
      {"max=\"20\"", "max", "20", HP_QUOTE},
      {"xmax=50", NULL, NULL, HP_SUFFIX},
      {"xtimeout=9", NULL, NULL, HP_SUFFIX},
      {"foo=bar", NULL, NULL, 0},
      {NULL, NULL, NULL, 0}}},
};

/* First item of seq whose key is key, or NULL. */
static const struct st_hp_item *st_hp_first(const struct st_hp_item *const *seq,
                                            size_t n, const char *key,
                                            size_t *at) {
  size_t i;

  for (i = 0; i < n; i++)
    if (seq[i]->key != NULL && strcmp(seq[i]->key, key) == 0) {
      *at = i;
      return seq[i];
    }
  return NULL;
}

/* What the site must report now for the parameters in seq. */
static void st_hp_expect(const struct st_hp_site *site,
                         const struct st_hp_item *const *seq, size_t n,
                         char *out, size_t size, int *why) {
  size_t at = 0, i;
  const struct st_hp_item *hit;

  *why = 0;
  for (i = 0; i < n; i++)
    *why |= seq[i]->why;
  if (site->prefix == NULL) {
    const struct st_hp_item *t = st_hp_first(seq, n, "timeout", &at);
    const struct st_hp_item *m = st_hp_first(seq, n, "max", &at);
    const int timeout = t != NULL ? atoi(t->want) : 15;
    const int max = m != NULL ? atoi(m->want) : 10;

    snprintf(out, size, "keep_alive=%d timeout=%d max=%d",
             !(max <= 1 || timeout < 1), timeout, max);
    return;
  }
  hit = st_hp_first(seq, n, site->param, &at);
  if (hit != NULL && at != 0)
    *why |= HP_LATER;
  if (hit != NULL && at + 1 < n)
    *why |= HP_TRAIL;
  snprintf(out, size, "%s%s", site->prefix, hit != NULL ? hit->want : "");
}

/* Hand cases: a NULL want means the input parses as before. */
static const struct {
  const char *site, *in, *want;
} st_hp_cases[] = {
    {"content-type", " text/html", NULL},
    {"content-type", " text/html;charset=utf-8", NULL},
    {"content-type", " text/html; charset='utf-8'", NULL},
    {"content-type", " text/html; charset=\" utf-8 \"", NULL},
    {"content-type", " text/html; charset=utf-8 junk", NULL},
    {"content-type", " text/html; charset=utf-8\"", NULL},
    {"content-type", " text/html; charset", NULL},
    {"content-type", " text/html; charset=\"utf-8", NULL},
    {"content-type", " charset=utf-8",
     "contenttype=charset=utf-8 charset=utf-8"},
    {"content-disposition", " attachment", NULL},
    {"content-disposition", " attachment; filename=a.txt", NULL},
    {"content-disposition", " attachment; filename='a.txt'", NULL},
    {"content-disposition", " attachment; filename= a.txt ", NULL},
    {"content-disposition", " attachment; filename=\"a.txt", NULL},
    {"content-disposition", " attachment; filename=../../etc/passwd", NULL},
    {"content-disposition", " attachment; filename=", NULL},
    {"content-disposition", " filename=a.txt", NULL},
    {"content-disposition", " attachment; filename=\"a.txt\"; size=3",
     "cdispo=a.txt"},
    {"content-disposition", " attachment; filename=\"a;b.txt\"", NULL},
    {"content-disposition", " attachment; filename=\"C:\\dir\\a.txt\"", NULL},
    {"content-disposition", " attachment; filename=\"C:\\\\dir\\\\a.txt\"",
     "cdispo=C:\\dir\\a.txt"},
    {"content-disposition", " attachment; filename = a.txt", "cdispo=a.txt"},
    {"keep-alive", " timeout=5, max=100", NULL},
    {"keep-alive", " timeout=5; max=100", NULL},
    {"keep-alive", " timeout=5,max=100", NULL},
    {"keep-alive", " timeout= 7", NULL},
    {"keep-alive", " timeout=007x", NULL},
    {"keep-alive", " timeout=-1", NULL},
    {"keep-alive", " max", NULL},
    {"keep-alive", " xmax=50", "keep_alive=1 timeout=15 max=10"},
    {"keep-alive", " max=\"50\"", "keep_alive=1 timeout=15 max=50"},
};

/* Hand cases whose input is HEAD, then N copies of FILL. */
static const struct {
  const char *site, *head;
  char fill;
  size_t n;
  const char *want;
} st_hp_runs[] = {
    {"content-type", " text/html; charset=\"utf-8 ", 'x', 300,
     "contenttype=text/html charset="},
    {"content-type", " text/html; charset=", 'x', 100, NULL},
    {"content-type", " text/html; charset=", 'x', 125, NULL},
    {"content-type", " text/html; charset=", 'x', 126, NULL},
    {"content-disposition", " attachment; filename=", 'x', 150, NULL},
    {"content-disposition", " attachment; filename=", 'x', 199, NULL},
    {"content-disposition", " attachment; filename=", 'x', 200, NULL},
    {"content-disposition", " attachment; filename=a.txt; filename*=UTF-8''",
     'x', 240, "cdispo=a.txt"},
    {"keep-alive", " timeout=5, max=", '0', 64,
     "keep_alive=1 timeout=5 max=10"},
};

/* One hand case against the legacy copy of its site. */
static int st_hp_case(const char *name, const char *in, const char *want) {
  const struct st_hp_site *site = st_hp_sites;
  char old[512], cur[512];

  while (strcmp(site->name, name) != 0)
    site++;
  site->legacy(in, old, sizeof(old));
  site->current(in, cur, sizeof(cur));
  if (strcmp(cur, want != NULL ? want : old) == 0)
    return 0;
  printf("FAIL %s \"%s\": legacy \"%s\" now \"%s\"\n", name, in, old, cur);
  return 1;
}

/* Generated input number I: N items under one separator, also put in seq. */
static void st_hp_build(const struct st_hp_site *site, size_t items,
                        const char *sep, size_t n, size_t i,
                        const struct st_hp_item **seq, char *in, size_t size) {
  size_t k;

  snprintf(in, size, "%s", site->head);
  for (k = 0; k < n; k++, i /= items) {
    seq[k] = &site->items[i % items];
    strlncatbuff(in, k != 0 || *site->head != '\0' ? sep : " ", size, size - 1);
    strlncatbuff(in, seq[k]->text, size, size - 1);
  }
}

/* Hand cases, then every 1-3 item sequence: a change needs a reason. */
static int st_headerparamdiff(httrackp *opt, int argc, char **argv) {
  size_t i, s, k, n;
  int compared = 0, failures = 0, by[HP_COUNT] = {0};
  char in[512], old[512], cur[512], want[512];

  (void) opt;
  (void) argc;
  (void) argv;
  for (i = 0; i < sizeof(st_hp_cases) / sizeof(st_hp_cases[0]); i++, compared++)
    failures +=
        st_hp_case(st_hp_cases[i].site, st_hp_cases[i].in, st_hp_cases[i].want);
  for (i = 0; i < sizeof(st_hp_runs) / sizeof(st_hp_runs[0]); i++, compared++) {
    const size_t head = strlen(st_hp_runs[i].head);

    assertf(head + st_hp_runs[i].n < sizeof(in));
    memcpy(in, st_hp_runs[i].head, head);
    memset(in + head, st_hp_runs[i].fill, st_hp_runs[i].n);
    in[head + st_hp_runs[i].n] = '\0';
    failures += st_hp_case(st_hp_runs[i].site, in, st_hp_runs[i].want);
  }
  for (s = 0; s < sizeof(st_hp_sites) / sizeof(st_hp_sites[0]); s++) {
    const struct st_hp_site *site = &st_hp_sites[s];
    size_t items, sep;

    for (items = 0; site->items[items].text != NULL; items++)
      ;
    for (sep = 0; site->seps[sep] != NULL; sep++)
      for (n = 1; n <= 3; n++) {
        size_t total = 1;

        for (k = 0; k < n; k++)
          total *= items;
        for (i = 0; i < total; i++) {
          const struct st_hp_item *seq[3];
          int why, r;

          st_hp_build(site, items, site->seps[sep], n, i, seq, in, sizeof(in));
          site->legacy(in, old, sizeof(old));
          site->current(in, cur, sizeof(cur));
          st_hp_expect(site, seq, n, want, sizeof(want), &why);
          compared++;
          if (strcmp(cur, want) != 0) {
            printf("FAIL %s \"%s\": now \"%s\" want \"%s\"\n", site->name, in,
                   cur, want);
            failures++;
          } else if (strcmp(cur, old) != 0) {
            if (why == 0) {
              printf("FAIL %s \"%s\": legacy \"%s\" now \"%s\", no reason\n",
                     site->name, in, old, cur);
              failures++;
            }
            for (r = 0; r < HP_COUNT; r++)
              if ((why & (1 << r)) != 0) {
                by[r]++;
                break;
              }
          }
        }
      }
  }
  printf("header-param-diff: %d compared, %d failures, changed by", compared,
         failures);
  for (i = 0; i < HP_COUNT; i++)
    printf(" %s=%d", st_hp_reasons[i], by[i]);
  printf("\n");
  return failures != 0;
}

const struct selftest_entry selftests_header[] = {
    {"pubheaders", "",
     "layout of the installed structs configure's switches decide",
     st_pubheaders},
    {"retry-after", "", "Retry-After header parser self-test", st_retryafter},
    {"header", "<raw-header-line> ...", "response header-line parsing",
     st_header},
    {"statusline", "<status-line>", "status code of an HTTP status line",
     st_statusline},
    {"headerfield", "<headers> <field>",
     "is <field> already present in the custom request-header block",
     st_headerfield},
    {"headerdedup", "<request> <headers> [capacity]",
     "what the custom header block adds to a request that has some (#1340)",
     st_headerdedup},
    {"headerlong", "[header-name:]",
     "over-long header value must not overflow the parse scratch",
     st_headerlong},
    {"location", "<url-length>", "Location gate matches the buffer it protects",
     st_location},
    {"headerline", "<read-size>",
     "a header line that does not fit is reported and skipped whole",
     st_headerline},
    {"binputline", "<read-size> [block]",
     "binput() consumes a clipped line whole (#1294)", st_binputline},
    {"crange", "<raw-content-range-line> ...",
     "Content-Range parse integer safety", st_crange},
    {"int-parse", "parse|scan <text> <min> <max>",
     "bounded decimal parse: value and end offset", st_intparse},
    {"int-parse-diff", "[<site> <input>]",
     "integer header and reply fields parse as before, out-of-range refused",
     st_intparsediff},
    {"header-param", "<value> <seps> <name> <size>",
     "one parameter of a header value, unquoted, refused if too long",
     st_headerparam},
    {"header-param-diff", "",
     "header parameters parse as before, but for the named changes",
     st_headerparamdiff},
    {"xfread-limit", "[oversized-file small-file]",
     "in-memory receive buffer size bound", st_xfread_limit},
    {"useragent", "", "default User-Agent self-test", st_useragent},
    {"codecerrno", "<dir> <doomed-out>",
     "hts_codec_unpack() errno: kept on a failed write, cleared on a bad "
     "stream",
     st_codecerrno},
    {"status", "", "HTTP status code -> reason phrase self-test", st_status},
    {"nobody", "", "which statuses excuse a response that stored no body",
     st_nobody},
    {"acceptencoding", "[dir]",
     "Accept-Encoding advertises gzip+deflate, both decode", st_acceptencoding},
    {"contentcodings", "[dir]",
     "brotli and zstd bodies decode; bombs and unknown codings are refused",
     st_contentcodings},
    {NULL, NULL, NULL, NULL},
};
