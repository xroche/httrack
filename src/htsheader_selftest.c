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

const struct selftest_entry selftests_header[] = {
    {"pubheaders", "",
     "layout of the installed structs configure's switches decide",
     st_pubheaders},
    {"retry-after", "", "Retry-After header parser self-test", st_retryafter},
    {"header", "<raw-header-line> ...", "response header-line parsing",
     st_header},
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
