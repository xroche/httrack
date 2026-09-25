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
/* File: htsopt_selftest.c subroutines:                         */
/*       self-tests for options, aliases and the log callback   */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htsselftest_int.h"

/* What the build's optional features came out as, so a test gates on the binary
   rather than on the platform it guessed from. */
static int st_features(httrackp *opt, int argc, char **argv) {
  (void) opt;
  (void) argc;
  (void) argv;
  printf("brotli %d\n", HTS_USEBROTLI ? 1 : 0);
  printf("zstd %d\n", HTS_USEZSTD ? 1 : 0);
#if (defined(__linux) && defined(HAVE_BACKTRACE))
  printf("backtrace 1\n");
#else
  printf("backtrace 0\n");
#endif
#if (defined(HTS_USEICONV) && (HTS_USEICONV == 0))
  printf("iconv 0\n");
#else
  printf("iconv 1\n");
#endif
#ifdef HTS_CRASH_TEST
  printf("crashtest 1\n");
#else
  printf("crashtest 0\n");
#endif
  /* The kernel, not the build, decides whether MPTCP can be opened. */
  printf("mptcp %d\n", hts_mptcp_available() ? 1 : 0);
  printf("mptcpinfo %d\n", hts_mptcp_reports() ? 1 : 0);
  return 0;
}

// -#test=footerfmt <template>: expand a -%F footer with fixed values (drives
// tests/01_engine-footerfmt.test). Also asserts the published field names and
// the overflow/zero-size returns the CLI cap keeps out of reach.
static int st_footerfmt(httrackp *opt, int argc, char **argv) {
  // Spelled out, not read back from the engine: these ten are the published
  // contract front ends validate their templates against.
  static const char *const names[] = {
      "addr",    "path", "url",     "date",   "lastmodified",
      "version", "mime", "charset", "status", "size"};
  // Filled by id, as htsparse.c does, so the .test's expected strings pin each
  // name to its enum slot; a positional list would not see them drift apart.
  const char *values[HTS_FOOTER_FIELD_COUNT] = {NULL};
  size_t i;
  char out[1024];
  char tiny[4];

  (void) opt;
  values[HTS_FOOTER_ADDR] = "host.example";
  values[HTS_FOOTER_PATH] = "/dir/page.html";
  values[HTS_FOOTER_URL] = "http://host.example/dir/page.html";
  values[HTS_FOOTER_DATE] = "DATE";
  values[HTS_FOOTER_LASTMODIFIED] = "LASTMOD";
  values[HTS_FOOTER_VERSION] = "VER";
  values[HTS_FOOTER_MIME] = "text/html";
  values[HTS_FOOTER_CHARSET] = "utf-8";
  values[HTS_FOOTER_STATUS] = "200";
  values[HTS_FOOTER_SIZE] = "1234";

  for (i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
    assertf(hts_footer_field_ok(names[i]) == HTS_TRUE);
  }
  assertf(!hts_footer_field_ok(NULL));
  assertf(!hts_footer_field_ok(""));
  assertf(!hts_footer_field_ok("nosuchfield"));
  // A prefix or a longer name must not match, or "{addr}" and "{addrx}" would
  // validate alike.
  assertf(!hts_footer_field_ok("add"));
  assertf(!hts_footer_field_ok("addrx"));
  // Matching is exact: the expander is case-sensitive, so a validator that
  // accepted "{ADDR}" would green-light a template the crawl emits verbatim.
  assertf(!hts_footer_field_ok("Addr"));
  // Overflow (named and legacy) and a zero-size buffer must return <0, never
  // truncate silently or write out of bounds.
  assertf(hts_footer_format(tiny, sizeof(tiny), "{addr}", values) < 0);
  assertf(hts_footer_format(tiny, sizeof(tiny), "a %s b", values) < 0);
  assertf(hts_footer_format(out, 0, "", values) < 0);
  // An empty template yields an empty, terminated string.
  assertf(hts_footer_format(out, sizeof(out), "", values) == 1 &&
          out[0] == '\0');
  if (argc < 1) {
    fprintf(stderr, "footerfmt: needs a template\n");
    return 1;
  }
  if (hts_footer_format(out, sizeof(out), argv[0], values) < 0) {
    fprintf(stderr, "footerfmt: overflow\n");
    return 1;
  }
  printf("%s\n", out);
  return 0;
}

// hts_split_cmdline(): the vector must grow with the argument count, and a
// quote inside a value must not end the argument and hand -V to the parser.
static int st_cmdlinesplit(httrackp *opt, int argc, char **argv) {
  char line[512];
  char **args;
  int nargs = 0;

  (void) opt;
  (void) argc;
  (void) argv;

  // control: every separator splits, and argv[0] is the program name
  strcpybuff(line, "httrack http://x/ --quiet\t-c8\n-O out");
  args = hts_split_cmdline(line, &nargs);
  assertf(args != NULL && nargs == 6);
  assertf(args[nargs] == NULL); // callers may walk to the terminator
  assertf(strcmp(args[0], "httrack") == 0);
  assertf(strcmp(args[1], "http://x/") == 0);
  assertf(strcmp(args[2], "--quiet") == 0);
  assertf(strcmp(args[3], "-c8") == 0);
  assertf(strcmp(args[4], "-O") == 0);
  assertf(strcmp(args[5], "out") == 0);
  freet(args);

  // the template pads with whitespace: empty arguments are kept (the engine
  // skips them), so the count is one per separator
  strcpybuff(line, "httrack  --quiet");
  args = hts_split_cmdline(line, &nargs);
  assertf(nargs == 3 && args[1][0] == '\0');
  assertf(strcmp(args[2], "--quiet") == 0);
  freet(args);

  // a quoted run keeps both its spaces and its quotes: the engine unquotes
  strcpybuff(line, "httrack --user-agent \"Mozilla 5.0\" -c8");
  args = hts_split_cmdline(line, &nargs);
  assertf(nargs == 4);
  assertf(strcmp(args[2], "\"Mozilla 5.0\"") == 0);
  assertf(strcmp(args[3], "-c8") == 0);
  freet(args);

  // an escaped quote is a literal quote, not the end of the argument: the
  // engine strips only the outer pair
  strcpybuff(line, "httrack --user-agent \"x\\\" -V \\\"touch /tmp/pwn\" -c8");
  args = hts_split_cmdline(line, &nargs);
  assertf(nargs == 4);
  assertf(strcmp(args[2], "\"x\" -V \"touch /tmp/pwn\"") == 0);
  assertf(strcmp(args[3], "-c8") == 0);
  freet(args);

  // \\ is a literal backslash, so a Windows path survives
  strcpybuff(line, "httrack --path \"C:\\\\dir\\\\sub\"");
  args = hts_split_cmdline(line, &nargs);
  assertf(nargs == 3);
  assertf(strcmp(args[2], "\"C:\\dir\\sub\"") == 0);
  freet(args);

  // outside a quoted run a backslash is literal: the url and wildcard-filter
  // fields, which the wizard cannot quote, read as before
  strcpybuff(line, "httrack -*\\** +*.png");
  args = hts_split_cmdline(line, &nargs);
  assertf(nargs == 3);
  assertf(strcmp(args[1], "-*\\**") == 0);
  assertf(strcmp(args[2], "+*.png") == 0);
  freet(args);

  // a quoted run leaves slots unused, so the terminator has to be written and
  // not inherited: size the vector from a full line first, so freeing it hands
  // the same chunk back with stale pointers in those slots
  strcpybuff(line, "httrack a b c d e");
  args = hts_split_cmdline(line, &nargs);
  assertf(nargs == 6);
  freet(args);
  strcpybuff(line, "httrack \"a b c d e\"");
  args = hts_split_cmdline(line, &nargs);
  assertf(nargs == 2);
  assertf(args[nargs] == NULL);
  freet(args);

  // an unterminated quote protects the rest of the line, as one argument
  strcpybuff(line, "httrack --footer \"unbalanced -V x");
  args = hts_split_cmdline(line, &nargs);
  assertf(nargs == 3);
  assertf(strcmp(args[2], "\"unbalanced -V x") == 0);
  freet(args);

  // past the 1024 entries the vector used to hold: distinct arguments, so a
  // write beyond the allocation cannot read back as the expected parse
  {
    const int n = 2000;
    const size_t size = 16 * (size_t) n + 16;
    char *big = malloct(size);
    size_t pos = 0;
    int i;

    assertf(big != NULL);
    pos = (size_t) snprintf(big, size, "httrack");
    assertf(pos < size);
    for (i = 0; i < n; i++) {
      // snprintf returns what it wanted to write, so accumulating it blind
      // would let the next size argument wrap
      const int len = snprintf(big + pos, size - pos, " a%d", i);

      assertf(len > 0 && (size_t) len < size - pos);
      pos += (size_t) len;
    }
    args = hts_split_cmdline(big, &nargs);
    assertf(args != NULL && nargs == n + 1);
    assertf(args[nargs] == NULL);
    for (i = 0; i < n; i++) {
      char expect[16];

      snprintf(expect, sizeof(expect), "a%d", i);
      assertf(strcmp(args[i + 1], expect) == 0);
    }
    freet(args);
    freet(big);
  }

  printf("cmdline-split self-test OK\n");
  return 0;
}

static int st_copyopt(httrackp *opt, int argc, char **argv) {
  httrackp *from = hts_create_opt();
  httrackp *to = hts_create_opt();
  int err = 0;

  (void) opt;
  (void) argc;
  (void) argv;
  /* from-values differ from both the to-values and the hts_create_opt()
     defaults (nearlink FALSE, errpage/parseall TRUE), so a copy that no-ops or
     just resets to defaults is caught too, not only the unsigned-guard bug. */
  from->retry = 7; /* int field: positive control */
  to->retry = 0;
  from->nearlink = HTS_TRUE;
  to->nearlink = HTS_FALSE;
  from->errpage = HTS_FALSE;
  to->errpage = HTS_TRUE;
  from->parseall = HTS_FALSE;
  to->parseall = HTS_TRUE;

  copy_htsopt(from, to);

  if (to->retry != 7)
    err = 1;
  if (to->nearlink != HTS_TRUE)
    err = 1;
  if (to->errpage != HTS_FALSE)
    err = 1;
  if (to->parseall != HTS_FALSE)
    err = 1;

  /* HTS_DEFAULT (-1) is "unspecified": copy_htsopt must skip it, leaving the
     target intact. Only a signed (int-backed) field can hold -1, so this also
     guards the type against regressing to an unsigned hts_boolean. */
  from->parseall = HTS_DEFAULT;
  to->parseall = HTS_TRUE;
  copy_htsopt(from, to);
  if (to->parseall != HTS_TRUE)
    err = 1;

  /* String field: a non-empty source deep-copies across, an empty source
     leaves the target intact (StringNotEmpty guard). Covers the exported
     copy_htsopt String path that no crawl test reaches. */
  StringCopy(from->cookies_file, "/tmp/jar.txt");
  StringCopy(to->cookies_file, "");
  copy_htsopt(from, to);
  if (strcmp(StringBuff(to->cookies_file), "/tmp/jar.txt") != 0)
    err = 1;
  StringCopy(from->cookies_file, "");
  copy_htsopt(from, to);
  if (strcmp(StringBuff(to->cookies_file), "/tmp/jar.txt") != 0)
    err = 1;

  /* warc_file: same String deep-copy path as cookies_file */
  StringCopy(from->warc_file, "run.warc.gz");
  StringCopy(to->warc_file, "");
  copy_htsopt(from, to);
  if (strcmp(StringBuff(to->warc_file), "run.warc.gz") != 0)
    err = 1;

  from->changes = HTS_TRUE;
  to->changes = HTS_FALSE;
  copy_htsopt(from, to);
  if (to->changes != HTS_TRUE)
    err = 1;

  /* single_file pair: the cap is guarded by >0, so an unset source must not
     overwrite the default the target already carries */
  from->single_file = HTS_TRUE;
  from->single_file_max_size = 4096;
  to->single_file = HTS_FALSE;
  to->single_file_max_size = SINGLEFILE_DEFAULT_MAX_SIZE;
  copy_htsopt(from, to);
  if (!to->single_file || to->single_file_max_size != 4096)
    err = 1;
  from->single_file_max_size = 0;
  copy_htsopt(from, to);
  if (to->single_file_max_size != 4096)
    err = 1;

  /* sitemap pair: the flag latches on, the URL takes the String deep copy */
  from->sitemap = HTS_TRUE;
  StringCopy(from->sitemap_url, "http://h.test/sitemap.xml");
  to->sitemap = HTS_FALSE;
  StringCopy(to->sitemap_url, "");
  copy_htsopt(from, to);
  if (!to->sitemap ||
      strcmp(StringBuff(to->sitemap_url), "http://h.test/sitemap.xml") != 0)
    err = 1;
  from->sitemap = HTS_FALSE;
  StringCopy(from->sitemap_url, "");
  copy_htsopt(from, to);
  if (!to->sitemap ||
      strcmp(StringBuff(to->sitemap_url), "http://h.test/sitemap.xml") != 0)
    err = 1;

  /* #185 pause pair: copied when enabled (max>0), the 0 sentinel skips */
  from->pause_min_ms = 5000;
  from->pause_max_ms = 10000;
  to->pause_min_ms = to->pause_max_ms = 0;
  copy_htsopt(from, to);
  if (to->pause_min_ms != 5000 || to->pause_max_ms != 10000)
    err = 1;
  from->pause_min_ms = from->pause_max_ms = 0;
  copy_htsopt(from, to);
  if (to->pause_min_ms != 5000 || to->pause_max_ms != 10000)
    err = 1;

  /* max_retry_after: 0 is a real setting (no wait), so only a negative skips */
  from->max_retry_after = 0;
  to->max_retry_after = 60;
  copy_htsopt(from, to);
  if (to->max_retry_after != 0)
    err = 1;
  from->max_retry_after = -1;
  copy_htsopt(from, to);
  if (to->max_retry_after != 0)
    err = 1;

  hts_free_opt(from);
  hts_free_opt(to);
  printf("copy-htsopt: %s\n", err ? "FAIL" : "OK");
  return err;
}

/* A run with no log file must still count its errors (#1681). */
static int st_logcounters(httrackp *opt, int argc, char **argv) {
  httrackp *o = hts_create_opt();
  const hts_stat_struct *stats;
  int err = 0;

  (void) opt;
  (void) argc;
  (void) argv;

  /* -Q sets no log file and default verbosity. */
  o->log = NULL;
  o->errlog = NULL;
  o->debug = LOG_NOTICE;

  hts_log_print(o, LOG_ERROR, "first");
  /* LOG_ERRNO rides in the high bits and must not shift the level. */
  hts_log_print(o, LOG_ERROR | LOG_ERRNO, "second");
  hts_log_print(o, LOG_WARNING, "third");
  hts_log_print(o, LOG_WARNING, "fourth");
  hts_log_print(o, LOG_WARNING, "fifth");
  hts_log_print(o, LOG_NOTICE, "sixth");

  /* Three distinct totals, so a swapped counter cannot pass. */
  stats = hts_get_stats(o);
  if (stats == NULL || stats->stat_errors != 2 || stats->stat_warnings != 3 ||
      stats->stat_infos != 1)
    err = 1;

  /* Polling hts_get_stats() must not increment the count. */
  stats = hts_get_stats(o);
  if (stats == NULL || stats->stat_errors != 2)
    err = 1;

  /* LOG_INFO is above the default verbosity, so it is not counted. */
  hts_log_print(o, LOG_INFO, "seventh");
  stats = hts_get_stats(o);
  if (stats == NULL || stats->stat_infos != 1)
    err = 1;

  /* Raising the verbosity lets the same level through. */
  o->debug = LOG_INFO;
  hts_log_print(o, LOG_INFO, "eighth");
  stats = hts_get_stats(o);
  if (stats == NULL || stats->stat_infos != 2)
    err = 1;

  /* A panic is fatal, so it counts as an error rather than as nothing. */
  hts_log_print(o, LOG_PANIC, "ninth");
  stats = hts_get_stats(o);
  if (stats == NULL || stats->stat_errors != 3)
    err = 1;

  hts_free_opt(o);
  printf("log-counters: %s\n", err ? "FAIL" : "OK");
  return err;
}

/* The note is built twice from one buffer, so a caller reusing it must never
   read the previous mirror's text back out of a silent run. */
static int st_upperlinksnote(httrackp *opt, int argc, char **argv) {
  httrackp *from = hts_create_opt();
  httrackp *to = hts_create_opt();
  char note[256];
  int err = 0;

  (void) opt;
  (void) argc;
  (void) argv;

  strcpybuff(note, "stale");
  if (hts_upper_links_note(to, note, sizeof(note)) || note[0] != '\0')
    err = 1; /* a fresh opt refused nothing, and must empty the buffer */

  to->upper_links_refused = HTS_TRUE;
  if (!hts_upper_links_note(to, note, sizeof(note)) ||
      strstr(note, "-B") == NULL)
    err = 1; /* the note must name the option that lifts the refusal */

  to->upper_links_refused = HTS_FALSE;
  if (hts_upper_links_note(to, note, sizeof(note)) || note[0] != '\0')
    err = 1;

  /* the flag never travels onto another opt */
  from->upper_links_refused = HTS_TRUE;
  copy_htsopt(from, to);
  if (to->upper_links_refused)
    err = 1;

  hts_free_opt(from);
  hts_free_opt(to);
  printf("upper-links-note: %s\n", err ? "FAIL" : "OK");
  return err;
}

/* The handler below pins the enumerator NAME; these pin the NUMBERS, which are
   ABI in the installed htsopt.h and are what -C prints. */
HTS_STATIC_ASSERT(HTS_CACHE_NONE == 0, cache_none_is_0);
HTS_STATIC_ASSERT(HTS_CACHE_PRIORITY == 1, cache_priority_is_1);
HTS_STATIC_ASSERT(HTS_CACHE_TEST_UPDATE == 2, cache_test_update_is_2);

/* -C's own default (no -C given) is C1 cache-priority, not the C2 test-update
   the man page and this enum's comment claimed for a long time. */
static int st_cachedefault(httrackp *opt, int argc, char **argv) {
  httrackp *const fresh = hts_create_opt();
  int err = 0;

  (void) opt;
  (void) argc;
  (void) argv;
  if (fresh->cache != HTS_CACHE_PRIORITY)
    err = 1;
  hts_free_opt(fresh);
  printf("cache-default: %s\n", err ? "FAIL" : "OK");
  return err;
}

static char st_log_callback_seen[256];

static HTS_PRINTF_FUN(3, 0) void st_log_callback(httrackp *opt, int type,
                                                 const char *format,
                                                 va_list args) {
  (void) opt;
  (void) type;
  (void) vsnprintf(st_log_callback_seen, sizeof(st_log_callback_seen), format,
                   args);
}

/* The callback must not consume the va_list the log file's vfprintf() needs. */
static int st_logcallback(httrackp *opt, int argc, char **argv) {
  static const char want[] = "42 sentinel";
  static const char want_filtered[] = "7 filtered";
  char BIGSTK seen[sizeof(st_log_callback_seen)];
  char BIGSTK line[256];
  FILE *fp;
  int rc = 1;

  (void) argc;
  (void) argv;

  fp = tmpfile();
  if (fp == NULL) {
    fprintf(stderr, "logcallback: tmpfile() failed\n");
    return 1;
  }
  opt->log = fp;
  opt->debug = LOG_NOTICE;
  st_log_callback_seen[0] = '\0';
  hts_set_log_vprint_callback(st_log_callback);
  hts_log_print(opt, LOG_NOTICE, "%d %s", 42, "sentinel");
  hts_set_log_vprint_callback(NULL);
  opt->log = NULL;
  strcpybuff(seen, st_log_callback_seen);

  rewind(fp);
  if (fgets(line, (int) sizeof(line), fp) == NULL) {
    fprintf(stderr, "logcallback: log file is empty, nothing was written\n");
    fclose(fp);
    return 1;
  }
  fclose(fp);

  /* The callback runs above the level filter and without a log file at all;
     the front-ends that install one usually have no opt->log open. */
  st_log_callback_seen[0] = '\0';
  hts_set_log_vprint_callback(st_log_callback);
  hts_log_print(opt, LOG_DEBUG, "%d %s", 7, "filtered");
  hts_set_log_vprint_callback(NULL);

  /* Same arguments both ways; the file line carries a level prefix. */
  if (strcmp(seen, want) != 0)
    fprintf(stderr, "logcallback: callback got '%s' want '%s'\n", seen, want);
  else if (strstr(line, want) == NULL)
    fprintf(stderr, "logcallback: log file got '%s' want it to carry '%s'\n",
            line, want);
  else if (strcmp(st_log_callback_seen, want_filtered) != 0)
    fprintf(stderr, "logcallback: unfiltered callback got '%s' want '%s'\n",
            st_log_callback_seen, want_filtered);
  else
    rc = 0;

  if (rc == 0)
    printf("logcallback self-test OK\n");
  return rc;
}

#ifdef HTS_CRASH_TEST
static char st_crash_seen[256];
static int st_crash_level;
static hts_boolean st_crash_had_opt;

static HTS_PRINTF_FUN(3, 0) void st_crash_log(httrackp *opt, int type,
                                              const char *format,
                                              va_list args) {
  st_crash_had_opt = opt != NULL ? HTS_TRUE : HTS_FALSE;
  st_crash_level = type;
  (void) vsnprintf(st_crash_seen, sizeof(st_crash_seen), format, args);
}

static FILE *st_crash_fp;

static HTS_PRINTF_FUN(3, 0) void st_crash_file_log(httrackp *opt, int type,
                                                   const char *format,
                                                   va_list args) {
  (void) opt;
  (void) type;
  vfprintf(st_crash_fp, format, args);
  fputc('\n', st_crash_fp);
  /* Flushed line by line, because the caller below is about to fault. */
  fflush(st_crash_fp);
}

/* See htscrashtest.h for why both channels carry the marker. Given a file, arm
   a real -#c fault and write what the log callback receives there: that run
   crashes, which is the path httrack-android recovers. */
static int st_crashannounce(httrackp *opt, int argc, char **argv) {
  static const char want[] = "crash announce 42";
  int rc = 1;

  (void) opt;

  if (argc >= 1) {
    st_crash_fp = fopen(argv[0], "wb");
    if (st_crash_fp == NULL) {
      fprintf(stderr, "crashannounce: cannot write %s\n", argv[0]);
      return 1;
    }
    hts_set_log_vprint_callback(st_crash_file_log);
    (void) hts_crash_test("dnssegv");
    hts_crash_test_worker(HTS_CRASH_WORKER_DNS);
    /* Only a handler that swallowed the fault gets here, so hand the log
       channel back rather than funnelling a later case into this file. */
    hts_set_log_vprint_callback(NULL);
    fclose(st_crash_fp);
    st_crash_fp = NULL;
    fprintf(stderr, "crashannounce: the armed fault did not crash\n");
    return 1;
  }

  hts_set_log_vprint_callback(st_crash_log);
  /* The engine must not need an opt to reach the front end here. */
  hts_crash_test_announce("crash announce %d", 42);
  hts_set_log_vprint_callback(NULL);

  if (strcmp(st_crash_seen, want) != 0)
    fprintf(stderr, "crashannounce: callback got '%s' want '%s'\n",
            st_crash_seen, want);
  else if (st_crash_level != LOG_ERROR)
    fprintf(stderr, "crashannounce: logged at level %d, want %d (LOG_ERROR)\n",
            st_crash_level, (int) LOG_ERROR);
  else if (st_crash_had_opt)
    fprintf(stderr, "crashannounce: the callback was handed an opt\n");
  else
    rc = 0;

  if (rc == 0)
    printf("crashannounce self-test OK\n");
  return rc;
}
#endif

/* The short form optalias_check() emits for the option words, both joined by a
   space when it returns two, or NULL when it refuses them; *used counts the
   words consumed and warn takes the message an accepted option still drew. */
static const char *st_optalias_expand(char *dest, size_t dest_size,
                                      const char *word, const char *next,
                                      int *used, char *warn, size_t warn_size) {
  char BIGSTK out[2][HTS_CDLMAXSIZE];
  char *outv[2] = {out[0], out[1]};
  const char *argv[2];
  char error[256];
  const int argc = next != NULL ? 2 : 1;
  int outc = 0;

  argv[0] = word;
  argv[1] = next;
  out[0][0] = out[1][0] = dest[0] = warn[0] = '\0';
  *used = optalias_check(argc, argv, 0, &outc, outv, sizeof(out[0]), error,
                         sizeof(error));
  if (*used == 0) {
    assertf(error[0] != '\0'); /* a refusal has to say why */
    return NULL;
  }
  strlcpybuff(warn, error, warn_size);
  assertf(outc >= 1 && outc <= 2);
  strlcpybuff(dest, out[0], dest_size);
  if (outc == 2) {
    strlcatbuff(dest, " ", dest_size);
    strlcatbuff(dest, out[1], dest_size);
  }
  return dest;
}

/* Long-option value handling (#1195): a value the option's class did not take
   was dropped, so --index=0 read back as the enabling bare --index. */
static int st_optalias(httrackp *opt, int argc, char **argv) {
  char got[HTS_CDLMAXSIZE * 2], warn[256];
  int i, used, params = 0;

  (void) opt;
  /* -list gives 282 the whole table: the rows to try against the engine's
     own parser, and the class of every name the wizard writes a value to */
  if (argc == 1 && strcmp(argv[0], "-list") == 0) {
    for (i = 0; optalias_value(i)[0] != '\0'; i++)
      printf("%s %s\n", opttype_value(i), optalias_value(i));
    return 0;
  }
  if (argc >= 1) {
    const char *const out = st_optalias_expand(got, sizeof(got), argv[0],
                                               argc >= 2 ? argv[1] : NULL,
                                               &used, warn, sizeof(warn));

    printf("%s\n", out != NULL ? out : "(refused)");
    return out != NULL ? 0 : 1;
  }
#define EXPANDS(want, word, next)                                              \
  do {                                                                         \
    const char *const out__ = st_optalias_expand(                              \
        got, sizeof(got), (word), (next), &used, warn, sizeof(warn));          \
    assertf(out__ != NULL && strcmp(out__, (want)) == 0);                      \
    assertf(warn[0] == '\0');                                                  \
  } while (0)
/* accepted, expanding to WANT, but drawing a warning on the way */
#define WARNS(want, word, next)                                                \
  do {                                                                         \
    const char *const out__ = st_optalias_expand(                              \
        got, sizeof(got), (word), (next), &used, warn, sizeof(warn));          \
    assertf(out__ != NULL && strcmp(out__, (want)) == 0);                      \
    assertf(warn[0] != '\0');                                                  \
  } while (0)
#define REFUSES(word, next)                                                    \
  assertf(st_optalias_expand(got, sizeof(got), (word), (next), &used, warn,    \
                             sizeof(warn)) == NULL)

  /* -I0 has always disabled the index; now the long form can say it too */
  EXPANDS("-I0", "--index=0", NULL);
  EXPANDS("-I0", "--index=off", NULL);
  EXPANDS("-I0", "--noindex", NULL);
  EXPANDS("-I", "--index=1", NULL);
  EXPANDS("-I", "--index=on", NULL);
  /* detached, as a config file writes it ("index off"), and never a URL */
  EXPANDS("-I0", "--index", "off");
  assertf(used == 2);
  EXPANDS("-I", "--index", "http://foo/");
  assertf(used == 1);
  EXPANDS("-I", "--index", NULL);
  assertf(used == 1);
  /* -I takes 0 alone, so an out-of-range value is an error, not a bare -I */
  REFUSES("--index=2", NULL);
  REFUSES("--index=yes", NULL);

  /* a numeric level reaches the short form whole */
  EXPANDS("-%I0", "--search-index=0", NULL);
  EXPANDS("-%I2", "--search-index=2", NULL);
  EXPANDS("-%v2", "--display=2", NULL);
  EXPANDS("-%N0", "--delayed-type-check=0", NULL);
  EXPANDS("-o0", "--generate-errors=0", NULL);
  REFUSES("--display=full", NULL);
  REFUSES("--display=99999999999999999999", NULL);

  /* three rows were "param", a class that demands a following token: the URL
     after --purge-old became its value, and =1 died on "invalid option 1" */
  EXPANDS("-X", "--purge-old", NULL);
  EXPANDS("-X", "--purge-old", "http://foo/");
  assertf(used == 1);
  EXPANDS("-X0", "--purge-old=0", NULL);
  EXPANDS("-X", "--purge-old=1", NULL);
  EXPANDS("-X0", "--purge-old", "0");
  assertf(used == 2);
  EXPANDS("-X", "--purge-old", "1");
  EXPANDS("-X0", "--nopurge-old", NULL);
  EXPANDS("-%f0", "--httpproxy-ftp=0", NULL);
  EXPANDS("-%f", "--httpproxy-ftp=1", NULL);
  EXPANDS("-%P0", "--extended-parsing=0", NULL);
  EXPANDS("-%P", "--extended-parsing=1", NULL);

  /* a flag that takes no value refuses one rather than dropping it */
  REFUSES("--mirror=0", NULL);
  REFUSES("--nomirror", NULL);
  REFUSES("--warc=0", NULL);
  REFUSES("--single-file=s", NULL);
  /* ...and so does a compound alias, whose value would land in the cluster */
  REFUSES("--spider=0", NULL);

  /* an alias whose own name begins with "no" is not a negation */
  EXPANDS("-%T0", "--no-utf8-conversion", NULL);
  EXPANDS("-y0", "--no-background-on-suspend", NULL);
  EXPANDS("-%T", "--utf8-conversion", NULL);

  /* the --wide-/--tiny- prefix glues the --wide/--tiny connection count onto
     the plain clusters, and is refused wherever the glue would be misread */
  EXPANDS("-wc32", "--wide-mirror", NULL);
  assertf(used == 1);
  EXPANDS("-wc1", "--tiny-mirror", NULL);
  EXPANDS("-p0C0I0tc32", "--wide-spider", NULL);
  EXPANDS("-qgc1", "--tiny-get", NULL);
  EXPANDS("-Xc32", "--wide-purge-old", NULL);
  EXPANDS("-o2c1", "--tiny-generate-errors", "2");
  assertf(used == 2);
  /* elsewhere the alias applies without the count, and says so */
  WARNS("-O /tmp", "--wide-path", "/tmp"); /* the value is a word of its own */
  WARNS("+*.gif", "--tiny-allow", "*.gif");
  WARNS("--clean", "--wide-clean", NULL); /* a long form */
  WARNS("-c8", "--wide-sockets", "8");    /* -c8 already: 8 or 32? */
  WARNS("-c32", "--tiny-wide", NULL);     /* -c32 already: 32 or 1? */
  WARNS("-N1", "--wide-structure", "1");  /* -N takes a template */
  WARNS("-%r", "--wide-warc", NULL);      /* -%rc is --warc-cdx, not -%r -c */
  WARNS("-#h", "--wide-version", NULL);   /* -#h is matched as a whole word */
  WARNS("-h", "--tiny-help", NULL);

  /* the value-taking classes are untouched */
  EXPANDS("-C0", "--cache=0", NULL);
  EXPANDS("-C0", "--nocache", NULL);
  EXPANDS("-C2", "--cache", "2");
  EXPANDS("-P proxy:8080", "--proxy", "proxy:8080");
  EXPANDS("+*.gif", "--allow", "*.gif");

  /* #1426, both directions and table-wide over the "param" rows */
  for (i = 0; optalias_value(i)[0] != '\0'; i++) {
    const int p = optalias_find(optalias_value(i));
    char word[HTS_CDLMAXSIZE], want[HTS_CDLMAXSIZE];

    if (strcmp(opttype_value(p), "param") != 0)
      continue;
    params++;
    snprintf(word, sizeof(word), "--%s=yes", optalias_value(i));
    REFUSES(word, NULL);
    snprintf(word, sizeof(word), "--%s=none", optalias_value(i));
    REFUSES(word, NULL);
    snprintf(word, sizeof(word), "--%s=OFF", optalias_value(i));
    REFUSES(word, NULL);
    snprintf(word, sizeof(word), "--%s", optalias_value(i));
    REFUSES(word, "http://foo/");
    /* control: a number, and the on/off mapping, still pass in silence */
    snprintf(word, sizeof(word), "--%s=2", optalias_value(i));
    snprintf(want, sizeof(want), "%s2", optreal_value(p));
    EXPANDS(want, word, NULL);
    snprintf(word, sizeof(word), "--%s=off", optalias_value(i));
    snprintf(want, sizeof(want), "%s0", optreal_value(p));
    EXPANDS(want, word, NULL);
    snprintf(word, sizeof(word), "--%s=on", optalias_value(i));
    EXPANDS(optreal_value(p), word, NULL);
    /* #1437, table-wide too: the bound is the destination's magnitude, so
       padding rides through and a run past every destination does not */
    snprintf(word, sizeof(word), "--%s=00000000000000000002",
             optalias_value(i));
    snprintf(want, sizeof(want), "%s00000000000000000002", optreal_value(p));
    EXPANDS(want, word, NULL);
    snprintf(word, sizeof(word), "--%s=2147483647", optalias_value(i));
    snprintf(want, sizeof(want), "%s2147483647", optreal_value(p));
    EXPANDS(want, word, NULL);
    snprintf(word, sizeof(word), "--%s=99999999999999999999",
             optalias_value(i));
    REFUSES(word, NULL);
    /* a sign is not a digit run: -2 would spill into the cluster loop */
    snprintf(word, sizeof(word), "--%s=-2", optalias_value(i));
    REFUSES(word, NULL);
    snprintf(word, sizeof(word), "--%s=+2", optalias_value(i));
    REFUSES(word, NULL);
  }
  assertf(params >= 27); /* the rows the issue enumerates were all walked */

  /* the separator -m and -%c read besides digits, anchored to one occurrence
     with a digit after it: 1.2.3 otherwise reached maxconn as 1.2 */
  EXPANDS("-m,5000", "--max-files=,5000", NULL);
  EXPANDS("-m100,5000", "--max-files=100,5000", NULL);
  EXPANDS("-%c0.5", "--connection-per-second=0.5", NULL);
  EXPANDS("-%c.5", "--connection-per-second=.5", NULL);
  REFUSES("--connection-per-second=1.2.3", NULL);
  REFUSES("--connection-per-second=0.", NULL);
  REFUSES("--max-files=,", NULL);
  REFUSES("--max-files=100,", NULL);
  REFUSES("--max-files=,,,5", NULL);
  REFUSES("--max-files=1,2,3", NULL);
  REFUSES("--sockets=8,4", NULL);
  REFUSES("--sockets=0.5", NULL);
  REFUSES("--advanced-maxlinks=99999999999999999999", NULL);
  /* --sockets=8I0 mirrored without a top index: the tail reached -I0 */
  REFUSES("--sockets=8I0", NULL);
  /* an empty value is the bare short form, as it has always been */
  EXPANDS("-c", "--sockets=", NULL);

  /* --structure glues a preset and detaches a template (#1380): the engine
     reads a detached -N value as a user template whatever it holds */
  EXPANDS("-N1", "--structure=1", NULL);
  EXPANDS("-N100", "--structure=100", NULL);
  EXPANDS("-N1", "--structure", "1");
  assertf(used == 2);
  EXPANDS("-N %h%p/%n%q.%t", "--structure=%h%p/%n%q.%t", NULL);
  assertf(used == 1);
  EXPANDS("-N %h%p/%n%q.%t", "--structure", "%h%p/%n%q.%t");
  assertf(used == 2);
  EXPANDS("-N %h%p/%n%q.%t", "--user-structure", "%h%p/%n%q.%t");
  /* an empty value is -N "", which the engine reads as "back to the default" */
  EXPANDS("-N ", "--structure=", NULL);
  /* a preset trailed by more short options is one value asking for two, so it
     is refused whatever the tail holds; a % in it buys no template either */
  REFUSES("--structure=1L0", NULL);
  REFUSES("--structure", "1L0");
  REFUSES("--structure=1c8", NULL);
  REFUSES("--structure=1%c8", NULL);
  REFUSES("--structure=8I0", NULL);
  /* on/off are whole values rather than tails, and still map onto the preset */
  EXPANDS("-N0", "--structure=off", NULL);
  /* both name the default preset: a bare -N would take the next word, and
     with a URL there it mirrored nothing (#1434) */
  EXPANDS("-N0", "--structure=on", NULL);
  EXPANDS("-N0", "--structure", "on");
  assertf(used == 2);
  /* a template may open with a digit, so the leading digits do not decide it */
  EXPANDS("-N 2col/%n.%t", "--structure=2col/%n.%t", NULL);
  EXPANDS("-N 2col/%n.%t", "--structure", "2col/%n.%t");
  /* past 9 digits sscanf("%d") wraps onto -1, and with no % to make it a
     template either the value has nowhere left to go */
  EXPANDS("-N999999999", "--structure=999999999", NULL);
  REFUSES("--structure=4294967295", NULL);
  /* leading zeros are not part of the number: this is the preset 1 */
  EXPANDS("-N0000000001", "--structure=0000000001", NULL);
  EXPANDS("-N0000000001", "-N", "0000000001");
  assertf(used == 2);
  /* a %-free value would map every URL onto one name, so it is a typo */
  REFUSES("--structure=OFF", NULL);
  REFUSES("--structure=flat", NULL);
  REFUSES("--structure", "none");
  REFUSES("--structure=-1", NULL);
  /* --user-structure is how to ask for such a value on purpose */
  EXPANDS("-N flat", "--user-structure", "flat");
  /* the short form agrees, rather than reading -N 1 as a template named 1 */
  EXPANDS("-N1", "-N", "1");
  assertf(used == 2);
  EXPANDS("-N", "-N", "%h%p/%n%q.%t");
  assertf(used == 1);
  /* and there a bare digit run is the only thing it takes: 1L0 and an overlong
     run stay templates, as they were before the class existed */
  EXPANDS("-N", "-N", "2col/%n.%t");
  assertf(used == 1);
  EXPANDS("-N", "-N", "4294967295");
  assertf(used == 1);
  EXPANDS("-N", "-N", "1L0");
  assertf(used == 1);

  /* a -# row means the arm its name and help name, not a neighbour: -#C lists
     the cache where -#E extracts its meta-data, -#T logs transfer ops where
     -#t is the autocheck, and -#0 went with the single-letter tests (#427) */
  EXPANDS("-#E", "--extract-cache", NULL);
  EXPANDS("-#C *.gif", "--debug-cache", "*.gif");
  EXPANDS("-#t", "--autotest", NULL);
  EXPANDS("-#T", "--debug-xfrstats", NULL);
  REFUSES("--debug-testfilters", NULL);
  REFUSES("--extract-cache=1", NULL);
  /* and the reverse lookup the generated help prints takes the first row */
  assertf(strcmp(optalias_value(optreal_find("-#E")), "extract-cache") == 0);
  assertf(strcmp(optalias_value(optreal_find("-#C")), "debug-cache") == 0);
  assertf(strcmp(optalias_value(optreal_find("-#t")), "autotest") == 0);
  assertf(strcmp(optalias_value(optreal_find("-#T")), "debug-xfrstats") == 0);

  /* invariant: every name's bare long form still emits its short form, and a
     param name still demands its own value. A duplicate name (test, continue)
     resolves to its first row */
  for (i = 0; optalias_value(i)[0] != '\0'; i++) {
    const int p = optalias_find(optalias_value(i));
    char word[HTS_CDLMAXSIZE];

    assertf(p >= 0);
    snprintf(word, sizeof(word), "--%s", optalias_value(i));
    if (strncmp(opttype_value(p), "param", 5) == 0) {
      REFUSES(word, NULL);
    } else {
      EXPANDS(optreal_value(p), word, NULL);
      assertf(used == 1);
    }
  }
  assertf(i > 100); /* the table was walked, not skipped */
#undef EXPANDS
#undef WARNS
#undef REFUSES

  printf("optalias self-test OK\n");
  return 0;
}

/* Runs hts_scan_token() into an arena of destsize bytes poisoned past its end,
   and reports whether anything past that moved. */
static hts_boolean st_scantoken_run(char *src, size_t destsize, char *arena,
                                    size_t guard, hts_boolean *fit,
                                    size_t *advance) {
  char *p = src;
  size_t i;

  /* poisoned with '#', not 0, or the stray NUL of an off-by-one would read as
     untouched */
  memset(arena, '#', destsize + guard);
  *fit = hts_scan_token(&p, arena, destsize);
  *advance = (size_t) (p - src);
  for (i = destsize; i < destsize + guard; i++) {
    if (arena[i] != '#')
      return HTS_FALSE;
  }
  return HTS_TRUE;
}

/* A token longer than the buffer it is copied into must be refused, never
   written past the end (#1271). */
static int st_scantoken(httrackp *opt, int argc, char **argv) {
  /* the engine's own filter bound, and a small one the same code must honour */
  const size_t caps[] = {8, HTS_URLMAXSIZE * 2 - 1};
  const size_t guard = 64;
  size_t c;

  (void) opt;
  (void) argc;
  (void) argv;
  for (c = 0; c < sizeof(caps) / sizeof(caps[0]); c++) {
    const size_t destsize = caps[c];
    const size_t room = destsize - 1; /* characters, the NUL aside */
    /* empty, short, and the four lengths that straddle the bound */
    const size_t lens[] = {0, 1, room - 1, room, room + 1, room + guard};
    char *arena = malloct(destsize + guard);
    char *src = malloct(room + guard + 8);
    size_t k;

    assertf(arena != NULL && src != NULL);
    for (k = 0; k < sizeof(lens) / sizeof(lens[0]); k++) {
      const size_t len = lens[k];
      const size_t kept = len < room ? len : room;
      hts_boolean fit;
      size_t advance, i;

      /* the token, the whitespace run after it, and the token to land on. Its
         bytes vary, or a copy landing on the wrong index would still match */
      for (i = 0; i < len; i++)
        src[i] = (char) ('a' + i % 26);
      memcpy(src + len, " \tb", 4);
      assertf(st_scantoken_run(src, destsize, arena, guard, &fit, &advance));
      assertf(fit == (len <= room ? HTS_TRUE : HTS_FALSE));
      assertf(strlen(arena) == kept && memcmp(arena, src, kept) == 0);
      assertf(advance == len + 2);

      /* the same token ending the string: the cursor stops on its NUL */
      src[len] = '\0';
      assertf(st_scantoken_run(src, destsize, arena, guard, &fit, &advance));
      assertf(fit == (len <= room ? HTS_TRUE : HTS_FALSE));
      assertf(strlen(arena) == kept && memcmp(arena, src, kept) == 0);
      assertf(advance == len);
    }
    freet(arena);
    freet(src);
  }
  printf("scantoken self-test OK\n");
  return 0;
}

/* One http_postfile_body() case, run into a request block followed by a
   poisoned guard. A missing terminator shows up as a returned position that
   walked into the poison instead of stopping inside the block. */
static int st_postfile_case(const char *path, size_t start, size_t expect,
                            const char *what) {
  enum { capacity = 64, guard = 16 };

  char *arena = malloct(capacity + guard);
  FILE *fp = FOPEN(path, "rb");
  size_t pos;
  size_t i;
  int err = 0;

  if (arena == NULL || fp == NULL) {
    printf("  FAIL %s: cannot open %s\n", what, path);
    freet(arena);
    if (fp != NULL)
      fclose(fp);
    return 1;
  }
  memset(arena, '#', capacity + guard);
  arena[capacity + guard - 1] = '\0'; /* a runaway strlen ends here, not past */
  memset(arena, 'R', start);          /* what the request line already wrote */
  arena[start] = '\0';

  pos = http_postfile_body(arena, capacity, start, fp);
  fclose(fp);

  if (pos != expect) {
    printf("  FAIL %s: position %d, expected %d\n", what, (int) pos,
           (int) expect);
    err = 1;
  } else if (arena[pos] != '\0') {
    printf("  FAIL %s: no terminator at %d\n", what, (int) pos);
    err = 1;
  }
  for (i = capacity; i + 1 < capacity + guard; i++) {
    if (arena[i] != '#') {
      printf("  FAIL %s: guard byte %d written\n", what, (int) (i - capacity));
      err = 1;
      break;
    }
  }
  freet(arena);
  return err;
}

/* >postfile: bodies, read raw into the request block. Fixture paths in argv:
   shorter than the room left, longer than it, one holding a NUL, and empty. */
static int st_postfile(httrackp *opt, int argc, char **argv) {
  int err = 0;

  (void) opt;
  if (argc < 4) {
    printf("usage: -#test=postfile <short> <long> <embedded-nul> <empty>\n");
    return 1;
  }
  /* room left is 64 - 8 - 1 = 55 bytes */
  err |= st_postfile_case(argv[0], 8, 8 + 3, "short");
  err |= st_postfile_case(argv[1], 8, 8 + 55, "long");
  err |= st_postfile_case(argv[2], 8, 8 + 2, "embedded-nul");
  err |= st_postfile_case(argv[3], 8, 8, "empty");

  printf("postfile self-test: %s\n", err ? "FAIL" : "OK");
  return err;
}

// -#test=abortlock|stoplock <dir>: hts_take_lock_request() takes a request only
// when it is stamped after this run's hts-in_progress.lock and the engine could
// also delete it. It leaves every other lock file alone.
static hts_boolean st_lockrule_touch(const char *path) {
  FILE *const fp = FOPEN(path, "wb");

  if (fp == NULL)
    return HTS_FALSE;
  fclose(fp);
  return HTS_TRUE;
}

static hts_boolean st_lockrule_stamp(const char *path, time_t when) {
  STRUCT_UTIMBUF times;

  times.actime = times.modtime = when;
  return UTIME(path, &times) == 0 ? HTS_TRUE : HTS_FALSE;
}

/* Stamp PATH at SEC plus NSEC, both in the units hts_file_mtime() reports, so
   the caller reads SEC back rather than taking it from a clock. False on
   failure, and NSEC is dropped where this build has no sub-second call. */
static hts_boolean st_lockrule_stamp_ns(const char *path, int64_t sec,
                                        int32_t nsec) {
#if defined(_WIN32)
  LPWSTR wpath = hts_pathToUCS2(path);
  ULARGE_INTEGER ticks;
  FILETIME ft;
  HANDLE h;
  hts_boolean ok;

  if (wpath == NULL)
    return HTS_FALSE;
  h = CreateFileW(wpath, FILE_WRITE_ATTRIBUTES,
                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  freet(wpath);
  if (h == INVALID_HANDLE_VALUE)
    return HTS_FALSE;
  ticks.QuadPart = (ULONGLONG) sec * 10000000ULL + (ULONGLONG) (nsec / 100);
  ft.dwLowDateTime = ticks.LowPart;
  ft.dwHighDateTime = ticks.HighPart;
  ok = SetFileTime(h, NULL, NULL, &ft) ? HTS_TRUE : HTS_FALSE;
  CloseHandle(h);
  return ok;
#elif defined(HAVE_UTIMENSAT)
  struct timespec times[2];

  times[0].tv_sec = times[1].tv_sec = (time_t) sec;
  times[0].tv_nsec = times[1].tv_nsec = (long) nsec;
  return utimensat(AT_FDCWD, path, times, 0) == 0 ? HTS_TRUE : HTS_FALSE;
#else
  return st_lockrule_stamp(path, (time_t) sec);
#endif
}

/* Does hts_file_mtime() report the seconds and nanoseconds the filesystem
   holds? This is what tells a reader that invents or drops either apart from a
   filesystem that rounds. Vacuously true where STAT() has no nanosecond field,
   and on Windows, where it offers nothing independent of the reader's own
   GetFileAttributesExW. */
static hts_boolean st_lockrule_time_matches_stat(const char *path, int64_t sec,
                                                 int32_t nsec) {
#if !defined(_WIN32) && (defined(HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC) ||          \
                         defined(HAVE_STRUCT_STAT_ST_MTIMESPEC_TV_NSEC))
  STRUCT_STAT buf;
  long raw;

  if (STAT(path, &buf) != 0)
    return HTS_FALSE;
#if defined(HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC)
  raw = (long) buf.st_mtim.tv_nsec;
#else
  raw = (long) buf.st_mtimespec.tv_nsec;
#endif
  return sec == (int64_t) buf.st_mtime && nsec == (int32_t) raw ? HTS_TRUE
                                                                : HTS_FALSE;
#else
  (void) path;
  (void) sec;
  (void) nsec;
  return HTS_TRUE;
#endif
}

/* The sub-second arm of the rule. A script that asks as soon as it sees
   hts-in_progress.lock lands in the second the mirror started, which is the
   boundary here. Reports what covered it, because a filesystem keeping whole
   seconds holds none of these stamps. */
static int st_lockrule_subsecond(httrackp *opt, const char *tag,
                                 const char *name, const char *lock,
                                 const char *progress) {
  /* A microsecond apart, because 436 and 459 write their request as soon as
     the progress lock appears and nothing spaces the two writes. */
  const int32_t later = 500001000, same = 500000000, earlier = 499999000;
  hts_filetime_t started, probe;
  int err = 0;

  if (!hts_file_mtime(progress, &started)) {
    fprintf(stderr, "%s: %s has no timestamp\n", tag, progress);
    return 1;
  }
  if (!st_lockrule_touch(lock) ||
      !st_lockrule_stamp_ns(lock, started.sec, later) ||
      !hts_file_mtime(lock, &probe)) {
    fprintf(stderr, "%s: cannot stamp %s\n", tag, lock);
    return 1;
  }
  /* Before believing the read back, pin it to what the filesystem holds. */
  if (!st_lockrule_time_matches_stat(lock, probe.sec, probe.nsec)) {
    fprintf(stderr,
            "%s: hts_file_mtime() does not report %s's own seconds and"
            " nanoseconds\n",
            tag, lock);
    (void) UNLINK(lock);
    return 1;
  }
  if (probe.sec != started.sec || probe.nsec != later) {
    /* The stamp did not come back as asked, so none of the cases below can be
       placed. A FAT or exFAT volume rounds it to whole, even seconds. */
    printf("%s: same-second request: NOT COVERED (asked for %d ns, read back"
           " %d%s)\n",
           tag, (int) later, (int) probe.nsec,
           probe.sec != started.sec ? ", in another second" : "");
    (void) UNLINK(lock);
    return err;
  }

  if (!st_lockrule_stamp_ns(progress, started.sec, same)) {
    fprintf(stderr, "%s: cannot stamp %s\n", tag, progress);
    return 1;
  }
  if (!hts_take_lock_request(opt, name)) {
    fprintf(stderr,
            "%s: a request made in the second the mirror started was"
            " ignored\n",
            tag);
    err = 1;
  }
  if (fexist_utf8(lock)) {
    fprintf(stderr, "%s: the request outlived the action it caused\n", tag);
    err = 1;
  }

  /* Equal to the nanosecond is not later, so this is still an earlier run's. */
  if (!st_lockrule_touch(lock) ||
      !st_lockrule_stamp_ns(lock, started.sec, same)) {
    fprintf(stderr, "%s: cannot stamp %s\n", tag, lock);
    return 1;
  }
  if (hts_take_lock_request(opt, name)) {
    fprintf(stderr,
            "%s: a request stamped exactly when the mirror started was"
            " taken\n",
            tag);
    err = 1;
  }

  /* Same second, earlier nanosecond: the progress lock's own sub-second part
     has to be read, or this one reads as newer than a zero. Re-created,
     because the case above fails by deleting it. */
  if (!st_lockrule_touch(lock) ||
      !st_lockrule_stamp_ns(lock, started.sec, earlier)) {
    fprintf(stderr, "%s: cannot stamp %s\n", tag, lock);
    return 1;
  }
  if (hts_take_lock_request(opt, name)) {
    fprintf(stderr,
            "%s: a request older than the mirror by a fraction was taken\n",
            tag);
    err = 1;
  }
  if (!fexist_utf8(lock)) {
    fprintf(stderr, "%s: a request the engine refused was deleted\n", tag);
    err = 1;
  }

  /* An earlier second is earlier despite the fraction, or a request left
     by the run before would be taken for one aimed at this mirror. */
  if (!st_lockrule_stamp_ns(lock, started.sec - 1, later)) {
    fprintf(stderr, "%s: cannot stamp %s\n", tag, lock);
    return 1;
  }
  if (hts_take_lock_request(opt, name)) {
    fprintf(stderr,
            "%s: a request from the second before the mirror started was"
            " taken\n",
            tag);
    err = 1;
  }
  if (!fexist_utf8(lock)) {
    fprintf(stderr, "%s: a request the engine refused was deleted\n", tag);
    err = 1;
  }
  (void) UNLINK(lock);

  /* Backdating keeps the fraction, or the second it buys the engine would be
     short by up to another one. */
  if (!hts_file_backdate(progress, 1) || !hts_file_mtime(progress, &probe)) {
    fprintf(stderr, "%s: cannot backdate %s\n", tag, progress);
    return 1;
  }
  if (probe.sec != started.sec - 1 || probe.nsec != same) {
    fprintf(stderr,
            "%s: backdating %s moved it by something other than one second\n",
            tag, progress);
    err = 1;
  }
  printf("%s: same-second request: covered (stamps a microsecond apart)\n",
         tag);
  return err;
}

/* The other request file, which taking NAME must leave untouched. */
static const char *st_lockrule_other(const char *name) {
  return strcmp(name, HTS_ABORT_LOCKNAME) == 0 ? HTS_PAUSE_LOCKNAME
                                               : HTS_ABORT_LOCKNAME;
}

/* Set up <dir>/, its progress lock and both requests, NAME stamped at WHEN. */
static hts_boolean st_lockrule_setup(const char *dir, const char *name,
                                     char *lock, size_t locksz, char *progress,
                                     size_t progresssz, char *other,
                                     size_t othersz, time_t when) {
  strlcpybuff(lock, dir, locksz);
  strlcatbuff(lock, name, locksz);
  strlcpybuff(progress, dir, progresssz);
  strlcatbuff(progress, "hts-in_progress.lock", progresssz);
  strlcpybuff(other, dir, othersz);
  strlcatbuff(other, st_lockrule_other(name), othersz);
  structcheck(lock);
  return st_lockrule_touch(progress) && st_lockrule_touch(other) &&
         st_lockrule_touch(lock) && st_lockrule_stamp(lock, when);
}

/* A request the engine cannot remove must never be taken. Only a real failed
   unlink proves that, and nothing portable makes one happen, so this names
   what covered it rather than skipping in silence. */
static int st_lockrule_undeletable(httrackp *opt, const char *base,
                                   const char *tag, const char *name) {
#ifdef _WIN32
  (void) opt;
  (void) base;
  (void) name;
  /* The real case is a sharing violation, which needs a second process. */
  printf("%s: undeletable request: NOT COVERED (no portable way to make"
         " DeleteFile fail here)\n",
         tag);
  return 0;
#else
  char BIGSTK rodir[HTS_URLMAXSIZE];
  char BIGSTK rolock[HTS_URLMAXSIZE * 2];
  char BIGSTK roprogress[HTS_URLMAXSIZE * 2];
  char BIGSTK roother[HTS_URLMAXSIZE * 2];
  const uid_t self = geteuid();
  uid_t drop = self;
  int rofd;
  int poll;
  int err = 0;

  strlcpybuff(rodir, base, sizeof(rodir));
  strlcatbuff(rodir, "readonly/", sizeof(rodir));
  if (!st_lockrule_setup(rodir, name, rolock, sizeof(rolock), roprogress,
                         sizeof(roprogress), roother, sizeof(roother),
                         time(NULL) + 60)) {
    fprintf(stderr, "%s: cannot set up %s\n", tag, rolock);
    return 1;
  }
  /* One descriptor for both mode changes, so the name is resolved once. */
  rofd = open(rodir, O_RDONLY | O_DIRECTORY);
  if (rofd == -1) {
    fprintf(stderr, "%s: cannot open %s\n", tag, rodir);
    return 1;
  }
  /* Still readable, because an engine that saw no request at all would pass
     this for the wrong reason. */
  if (fchmod(rofd, 0555) != 0) {
    fprintf(stderr, "%s: cannot make %s read-only\n", tag, rodir);
    close(rofd);
    return 1;
  }
  /* Root may write to a read-only directory, so borrow an unprivileged euid. */
  if (self == 0) {
    const struct passwd *const pw = getpwnam("nobody");

    drop = pw != NULL ? pw->pw_uid : (uid_t) 65534;
    if (seteuid(drop) != 0) {
      printf("%s: undeletable request: NOT COVERED (root, and euid %d "
             "is not reachable)\n",
             tag, (int) drop);
      (void) fchmod(rofd, 0700);
      close(rofd);
      return 0;
    }
  }

  StringCopy(opt->path_log, rodir);
  /* Probing by removal keeps no stat that could go stale before the engine's
     own unlink, and ENOENT means the euid cannot reach the request at all. */
  errno = 0;
  if (UNLINK(rolock) == 0) {
    printf("%s: undeletable request: NOT COVERED (euid %d could still"
           " delete %s)\n",
           tag, (int) drop, rolock);
  } else if (errno == ENOENT) {
    printf("%s: undeletable request: NOT COVERED (%s is out of reach of"
           " euid %d)\n",
           tag, rodir, (int) drop);
  } else {
    for (poll = 0; poll < 2; poll++) {
      if (hts_take_lock_request(opt, name)) {
        fprintf(stderr,
                "%s: a request that cannot be deleted was taken (poll %d)\n",
                tag, poll);
        err = 1;
      }
    }
    errno = 0;
    if (UNLINK(rolock) == 0 || errno == ENOENT) {
      fprintf(stderr, "%s: the undeletable request went away\n", tag);
      err = 1;
    }
    printf("%s: undeletable request: covered (unwritable directory,"
           " euid %d)\n",
           tag, (int) drop);
  }

  if (self == 0 && seteuid(self) != 0) {
    fprintf(stderr, "%s: cannot get euid %d back\n", tag, (int) self);
    err = 1;
  }
  (void) fchmod(rofd, 0700);
  close(rofd);
  return err;
#endif
}

static int st_lockrule(httrackp *opt, int argc, char **argv, const char *tag,
                       const char *name) {
  char BIGSTK base[HTS_URLMAXSIZE];
  char BIGSTK lock[HTS_URLMAXSIZE * 2];
  char BIGSTK progress[HTS_URLMAXSIZE * 2];
  char BIGSTK other[HTS_URLMAXSIZE * 2];
  String saved = STRING_EMPTY;
  time_t started;
  int err = 0;

  if (argc < 1) {
    fprintf(stderr, "usage: -#test=%s <writable directory>\n", tag);
    return 1;
  }
  strcpybuff(base, argv[0]);
  if (base[0] != '\0' && hts_lastchar(base) != '/')
    strcatbuff(base, "/");
  /* The engine still has to shut down through its own path_log. */
  StringCopy(saved, StringBuff(opt->path_log));
  StringCopy(opt->path_log, base);

  strlcpybuff(lock, base, sizeof(lock));
  strlcatbuff(lock, name, sizeof(lock));
  structcheck(lock);
  if (hts_take_lock_request(opt, name)) {
    fprintf(stderr, "%s: a mirror with no request took one\n", tag);
    err = 1;
  }

  if (!st_lockrule_setup(base, name, lock, sizeof(lock), progress,
                         sizeof(progress), other, sizeof(other), time(NULL))) {
    fprintf(stderr, "%s: cannot set up %s\n", tag, base);
    err = 1;
  }
  started = get_filetime(progress);
  if (started == (time_t) -1) {
    fprintf(stderr, "%s: %s has no timestamp\n", tag, progress);
    err = 1;
  }

  /* Left behind by an earlier mirror, in the run's own start second. On a
     filesystem keeping fractions this reads as the earlier-fraction case, so it
     pins the boundary only where the stamps are whole seconds. */
  if (!st_lockrule_stamp(lock, started)) {
    fprintf(stderr, "%s: cannot stamp %s\n", tag, lock);
    err = 1;
  }
  if (hts_take_lock_request(opt, name)) {
    fprintf(stderr,
            "%s: a request no newer than the mirror it found was taken\n", tag);
    err = 1;
  }
  if (!fexist_utf8(lock)) {
    fprintf(stderr, "%s: a request the engine refused was deleted\n", tag);
    err = 1;
  }

  /* The engine backdates its own progress lock by a second, so a request
     written the instant that lock appears is newer than it. */
  if (!hts_file_backdate(progress, 1)) {
    fprintf(stderr, "%s: cannot backdate %s\n", tag, progress);
    err = 1;
  }
  if (!hts_take_lock_request(opt, name)) {
    fprintf(stderr,
            "%s: with the progress lock backdated, a request stamped when the"
            " mirror started was still refused\n",
            tag);
    err = 1;
  }
  /* A minute earlier is an earlier run's, backdating or not. */
  if (!st_lockrule_touch(lock) || !st_lockrule_stamp(lock, started - 60)) {
    fprintf(stderr, "%s: cannot stamp %s\n", tag, lock);
    err = 1;
  }
  if (hts_take_lock_request(opt, name)) {
    fprintf(stderr, "%s: a request a minute older than the mirror was taken\n",
            tag);
    err = 1;
  }
  if (!fexist_utf8(lock)) {
    fprintf(stderr, "%s: a request the engine refused was deleted\n", tag);
    err = 1;
  }
  if (!st_lockrule_stamp(progress, started)) {
    fprintf(stderr, "%s: cannot put %s back to when the mirror started\n", tag,
            progress);
    err = 1;
  }

  /* Stamped after the mirror started: aimed at this run. */
  if (!st_lockrule_touch(lock) || !st_lockrule_stamp(lock, started + 1)) {
    fprintf(stderr, "%s: cannot stamp %s\n", tag, lock);
    err = 1;
  }
  if (!hts_take_lock_request(opt, name)) {
    fprintf(stderr, "%s: a request the engine can delete was ignored\n", tag);
    err = 1;
  }
  if (fexist_utf8(lock)) {
    fprintf(stderr, "%s: the request outlived the action it caused\n", tag);
    err = 1;
  }
  if (!fexist_utf8(progress) || !fexist_utf8(other)) {
    fprintf(stderr, "%s: taking a request deleted a neighbouring lock\n", tag);
    err = 1;
  }
  if (hts_take_lock_request(opt, name)) {
    fprintf(stderr, "%s: one request was taken twice\n", tag);
    err = 1;
  }

  err |= st_lockrule_subsecond(opt, tag, name, lock, progress);

  /* No progress lock means no mirror to reach, whatever the request says. */
  (void) UNLINK(progress);
  if (!st_lockrule_touch(lock) || !st_lockrule_stamp(lock, time(NULL) + 60)) {
    fprintf(stderr, "%s: cannot re-create %s\n", tag, lock);
    err = 1;
  }
  if (hts_take_lock_request(opt, name)) {
    fprintf(stderr, "%s: a request reached a mirror that never ran\n", tag);
    err = 1;
  }

  err |= st_lockrule_undeletable(opt, base, tag, name);

  StringCopy(opt->path_log, StringBuff(saved));
  StringFree(saved);
  printf("%s self-test: %s\n", tag, err ? "FAIL" : "OK");
  return err;
}

static int st_abortlock(httrackp *opt, int argc, char **argv) {
  return st_lockrule(opt, argc, argv, "abortlock", HTS_ABORT_LOCKNAME);
}

static int st_stoplock(httrackp *opt, int argc, char **argv) {
  return st_lockrule(opt, argc, argv, "stoplock", HTS_PAUSE_LOCKNAME);
}

/* What a postprocess-html callback may claim, and out of whose storage. Two
   capacities and every offset, or a bound that ignored "size" or looked only at
   the base pointer would pass. */
static int st_postprocsize(httrackp *opt, int argc, char **argv) {
  char buf[16] = {0}, own[16] = {0};
  const size_t capa = sizeof(buf);
  size_t size;

  (void) opt;
  (void) argc;
  (void) argv;
  /* the engine's own storage, whatever the reply points at inside it */
  assertf(hts_postprocess_reply_inplace(buf, buf, capa));
  assertf(hts_postprocess_reply_inplace(buf + capa - 1, buf, capa));
  assertf(!hts_postprocess_reply_inplace(buf + capa, buf, capa));
  assertf(!hts_postprocess_reply_inplace(own, buf, capa));
  assertf(!hts_postprocess_reply_inplace(buf, buf, 0));
  for (size = 4; size <= capa; size *= 2) {
    size_t at;

    for (at = 0; at <= size; at++) {
      /* in place, a reply owns what is left of "size" past its own offset */
      assertf(hts_postprocess_reply_ok(buf + at, (int) (size - at), buf, size,
                                       capa));
      /* one past the storage is another object's business, not ours to bound */
      if (at < capa)
        assertf(!hts_postprocess_reply_ok(buf + at, (int) (size - at) + 1, buf,
                                          size, capa));
    }
    /* in the buffer's slack, past every byte the engine handed out */
    if (size < capa)
      assertf(!hts_postprocess_reply_ok(buf + size + 1, 0, buf, size, capa));
    /* a buffer of the callback's own is bounded by the callback */
    assertf(hts_postprocess_reply_ok(own, (int) capa + 1, buf, size, capa));
    /* neither may report a negative count */
    assertf(!hts_postprocess_reply_ok(own, -1, buf, size, capa));
    assertf(!hts_postprocess_reply_ok(buf, -1, buf, size, capa));
  }
  printf("postprocess reply self-test OK\n");
  return 0;
}

/* Print the verdict for a -V template, then the exact vector the engine would
   hand the shell, one "ARG <index> <value>" line each. The save name is a
   vector entry rather than part of the command, so a caller that wants to
   replay it has to pass it the same way. */
static int st_usercmd(httrackp *opt, int argc, char **argv) {
  char BIGSTK dest[8192];
  /* An optional third argument caps the room. The command line cannot reach
     the real cap any more: a -V template stops at HTS_CDLMAXSIZE, and the save
     name no longer lands in the command. */
  size_t room = sizeof(dest);
  const char *why = "unnamed";

  (void) opt;
  if (argc < 2) {
    fprintf(stderr, "usercmd: needs a -V template and a filename\n");
    return 1;
  }
  if (argc > 2) {
    const int asked = atoi(argv[2]);

    if (asked <= 0 || (size_t) asked > sizeof(dest)) {
      fprintf(stderr, "usercmd: room must be within 1..%d\n",
              (int) sizeof(dest));
      return 1;
    }
    room = (size_t) asked;
  }
  switch (usercommand_expand(dest, room, argv[0], argv[1], &why)) {
  case USERCOMMAND_EXPAND_OK:
    break;
  case USERCOMMAND_EXPAND_REFUSED:
    printf("REFUSED %s\n", why);
    return 0;
  default:
    printf("TOOLONG\n");
    return 0;
  }
  printf("OK\n");
#ifndef _WIN32
  {
    char *vector[USERCOMMAND_ARGV_MAX];
    const size_t n = usercommand_argv(vector, dest, argv[1]);
    size_t i;

    for (i = 0; i < n; i++)
      printf("ARG %d %s\n", (int) i, vector[i]);
  }
#else
  /* cmd.exe takes no positional parameters, so the name is inside the text */
  printf("ARG 0 cmd\nARG 1 /c\nARG 2 %s\n", dest);
#endif
  return 0;
}

/* Run a -V template for real, through the engine's own spawn path, and report
   what the shell exited with. The child's output goes to our stdout. */
static int st_usercmdrun(httrackp *opt, int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usercmdrun: needs a -V template and a filename\n");
    return 1;
  }
  fflush(stdout);
  printf("EXIT %d\n", usercommand_exe(opt, argv[0], argv[1]));
  return 0;
}

/* ------------------------------------------------------------ */
/* Registry: this module's tests, in the order -#test lists them. */
/* ------------------------------------------------------------ */

const struct selftest_entry selftests_opt[] = {
    {"usercmd", "<-V template> <filename> [room]",
     "rewrite a -V template's $0 and print the shell vector it would run",
     st_usercmd},
    {"usercmdrun", "<-V template> <filename>",
     "run a -V template through the engine's own spawn path", st_usercmdrun},
    {"log-counters", "",
     "error and warning counts survive a run with no log file (#1681)",
     st_logcounters},
    {"scantoken", "",
     "option-string token copy is bounded and reports a refusal (#1271)",
     st_scantoken},
    {"abortlock", "<writable directory>",
     "an abort request stops the mirror only if it is fresh and deletable",
     st_abortlock},
    {"stoplock", "<writable directory>",
     "a pause request pauses the mirror only if it is fresh and deletable",
     st_stoplock},
    {"postprocsize", "",
     "a postprocess-html callback cannot claim bytes it was not handed",
     st_postprocsize},
    {"postfile", "<short> <long> <embedded-nul> <empty>",
     "a >postfile: body is clipped to the request block and terminated",
     st_postfile},
    {"optalias", "[-list | <option> [<value>]]",
     "long-option alias expansion (--index=0 and friends)", st_optalias},
    {"features", "", "which optional features this build has", st_features},
    {"footerfmt", "<template>", "-%F footer positional/named expansion",
     st_footerfmt},
    {"cmdline-split", "",
     "webhttrack command-line to argv split (bounds, quoting)",
     st_cmdlinesplit},
    {"copyopt", "", "copy_htsopt option-copy self-test", st_copyopt},
    {"upperlinksnote", "",
     "the above-the-start-directory note empties its buffer when silent",
     st_upperlinksnote},
    {"cachedefault", "", "-C default is C1 cache-priority, not C2",
     st_cachedefault},
    {"logcallback", "", "log callback must not consume the log file's va_list",
     st_logcallback},
#ifdef HTS_CRASH_TEST
    {"crashannounce", "[logfile]",
     "a crash-test marker reaches the log callback too", st_crashannounce},
#endif
    {NULL, NULL, NULL, NULL},
};
