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
/* File: htscharset_selftest.c subroutines:                     */
/*       self-tests for charset conversion and escaping         */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htsselftest_int.h"

static int st_charset(httrackp *opt, int argc, char **argv) {
  char buf[512];
  size_t len;
  char *s;

  (void) opt;
  if (argc < 2) {
    fprintf(stderr, "charset: needs a charset and a string\n");
    return 1;
  }
  len = st_decode_body(argv[1], buf, sizeof(buf));
  s = hts_convertStringToUTF8(buf, len, argv[0]);
  if (s != NULL) {
    printf("%s\n", s);
    freet(s);
  } else {
    fprintf(stderr, "invalid string for charset %s\n", argv[0]);
  }
  return 0;
}

/* Oracle is the raw Win32 two-step this replaces, so a mis-wired direction or
   a plain copy fails whatever the machine's ACP. */
static int st_syscharset(httrackp *opt, int argc, char **argv) {
#ifdef _WIN32
  static const char *const utf8 = "caf\xC3\xA9 \xE2\x82\xAC"; /* "café €" */
  const UINT cp = GetACP();
  const int len = (int) strlen(utf8);
  /* the engine blocks best-fit; UTF-8 is the only ACP that check exempts */
  const DWORD flags = cp == CP_UTF8 ? 0 : WC_NO_BEST_FIT_CHARS;
  WCHAR wide[64], round[64];
  char want[64];
  char *sys, *back, *part;
  int wn, n, lossless;

  (void) opt;
  (void) argc;
  (void) argv;
  wn = MultiByteToWideChar(CP_UTF8, 0, utf8, len, wide,
                           (int) (sizeof(wide) / sizeof(wide[0])));
  assertf(wn > 0);
  n = WideCharToMultiByte(cp, flags, wide, wn, want, (int) sizeof(want) - 1,
                          NULL, NULL);
  assertf(n > 0);
  want[n] = '\0';
  /* the ACP holds it only if those bytes decode back to the same UTF-16 */
  lossless =
      MultiByteToWideChar(cp, 0, want, n, round,
                          (int) (sizeof(round) / sizeof(round[0]))) == wn &&
      memcmp(round, wide, (size_t) wn * sizeof(WCHAR)) == 0;

  sys = hts_convertStringUTF8ToSystem(utf8, (size_t) len);
  assertf(sys != NULL);
  assertf(strcmp(sys, want) == 0);
  assertf(strlen(sys) == (size_t) n); /* NUL-terminated, nothing past it */
  /* the copy the ASCII fast path returns is a pass on a UTF-8 ACP only */
  assertf(cp == CP_UTF8 || strcmp(sys, utf8) != 0);
  /* size bounds the read: a caller-given length, not strlen() */
  part = hts_convertStringUTF8ToSystem(utf8, 3);
  assertf(part != NULL && strcmp(part, "caf") == 0);
  freet(part);
  part = hts_convertStringUTF8ToSystem(utf8, 0);
  assertf(part != NULL && *part == '\0');
  freet(part);
  if (lossless) {
    back = hts_convertStringSystemToUTF8(sys, strlen(sys));
    assertf(back != NULL);
    assertf(strcmp(back, utf8) == 0);
    freet(back);
  }
  freet(sys);
  printf("syscharset: acp=%u %s: OK\n", (unsigned) cp,
         lossless ? "round-trip" : "one-way");
  return 0;
#else
  (void) opt;
  (void) argc;
  (void) argv;
  return 77; /* WIN32-only entry point */
#endif
}

/* Best-fit defeated the strict converter: CP932 lacks U+00A5 and quietly gave
   back a path separator for it. */
static int st_nobestfit(httrackp *opt, int argc, char **argv) {
#ifdef _WIN32
  /* internal, not in htscharset.h: ties cp below to cs by name, not by guess */
  extern UINT hts_getCodepage(const char *name);
  static const char *const yen = "\xC2\xA5";      /* U+00A5 */
  static const char *const micro = "\xC2\xB5";    /* U+00B5, best-fit 83 CA */
  static const char *const hira = "\xE3\x81\x82"; /* U+3042, CP932 82 A0 */
  /* yen + micro + hira + ASCII: a best-fit-shorter, a best-fit-longer, a
     native and an untouched code point in one string */
  static const char *const mixed = "\xC2\xA5\xC2\xB5\xE3\x81\x82"
                                   "A";
  const char *const cs = "shift_jis";
  const UINT cp = hts_getCodepage(cs);
  BOOL usedDefault = TRUE;
  BOOL gotCpInfo;
  CPINFO cpi;
  WCHAR wide[4];
  char raw[8];
  char *s;
  int wn, n;

  (void) opt;
  (void) argc;
  (void) argv;
  assertf(cp == 932);
  if (!IsValidCodePage(cp)) {
    printf("nobestfit: CP932 is not installed\n");
    return 77;
  }
  /* the trap, asserted rather than assumed: left to itself the codepage hands
     back a path separator and reports no substitution at all */
  wn = MultiByteToWideChar(CP_UTF8, 0, yen, (int) strlen(yen), wide,
                           (int) (sizeof(wide) / sizeof(wide[0])));
  assertf(wn == 1);
  n = WideCharToMultiByte(cp, 0, wide, wn, raw, (int) sizeof(raw), NULL,
                          &usedDefault);
  assertf(n == 1 && raw[0] == '\\' && !usedDefault);
  /* U+00B5 best-fits to two bytes where the substitute is one, so a flag
     carried by only one of the two calls shows up as a length disagreement */
  wn = MultiByteToWideChar(CP_UTF8, 0, micro, (int) strlen(micro), wide,
                           (int) (sizeof(wide) / sizeof(wide[0])));
  assertf(wn == 1);
  n = WideCharToMultiByte(cp, 0, wide, wn, raw, (int) sizeof(raw), NULL, NULL);
  assertf(n == 2);
  /* the sizing passes disagree, and the shorter one under-allocates: banning
     best-fit sizes to the substitute but converts into room for the best-fit */
  n = WideCharToMultiByte(cp, WC_NO_BEST_FIT_CHARS, wide, wn, NULL, 0, NULL,
                          NULL);
  assertf(n == 1);
  /* same disagreement, summed over 4 code points instead of 1: a flag gated
     on wsize==1 would leave yen/micro best-fit instead of substituted here */
  wn = MultiByteToWideChar(CP_UTF8, 0, mixed, (int) strlen(mixed), wide,
                           (int) (sizeof(wide) / sizeof(wide[0])));
  assertf(wn == 4);
  n = WideCharToMultiByte(cp, 0, wide, wn, NULL, 0, NULL, NULL);
  assertf(n == 6); /* yen 1 + micro best-fit 2 + hira 2 + 'A' 1 */
  n = WideCharToMultiByte(cp, WC_NO_BEST_FIT_CHARS, wide, wn, NULL, 0, NULL,
                          NULL);
  assertf(n == 5); /* yen 1 + micro default 1 + hira 2 + 'A' 1 */
  /* control: banning best-fit must not break what the codepage does hold */
  s = hts_convertStringFromUTF8Strict(hira, strlen(hira), cs);
  assertf(s != NULL);
  assertf(strcmp(s, "\x82\xA0") == 0);
  freet(s);
  s = hts_convertStringFromUTF8Strict(yen, strlen(yen), cs);
  assertf(s == NULL);
  /* the non-strict caller still substitutes, but with the codepage's own
     default character rather than a lookalike */
  gotCpInfo = GetCPInfo(cp, &cpi);
  assertf(gotCpInfo);
  s = hts_convertStringFromUTF8(yen, strlen(yen), cs);
  assertf(s != NULL);
  assertf(s[0] == (char) cpi.DefaultChar[0] && s[1] == '\0');
  freet(s);
  s = hts_convertStringFromUTF8Strict(micro, strlen(micro), cs);
  assertf(s == NULL);
  s = hts_convertStringFromUTF8(micro, strlen(micro), cs);
  assertf(s != NULL);
  assertf(s[0] == (char) cpi.DefaultChar[0] && s[1] == '\0');
  freet(s);
  /* the mixed string, asserted byte-exact: both mutants above would either
     mis-size the buffer (undersized usize) or mis-gate flags (best-fit
     leaking through for a >1 code point string), and either shows up here */
  s = hts_convertStringFromUTF8Strict(mixed, strlen(mixed), cs);
  assertf(s == NULL); /* yen and micro are both lossy */
  s = hts_convertStringFromUTF8(mixed, strlen(mixed), cs);
  assertf(s != NULL);
  assertf(s[0] == (char) cpi.DefaultChar[0]);
  assertf(s[1] == (char) cpi.DefaultChar[0]);
  assertf((unsigned char) s[2] == 0x82 && (unsigned char) s[3] == 0xA0);
  assertf(s[4] == 'A' && s[5] == '\0');
  freet(s);
  /* the other arm of the gate: a codepage that refuses a non-zero dwFlags must
     still convert, so UTF-7 encodes the yen instead of failing */
  s = hts_convertStringFromUTF8(yen, strlen(yen), "utf-7");
  assertf(s != NULL && s[0] == '+');
  freet(s);
  printf("nobestfit: OK\n");
  return 0;
#else
  (void) opt;
  (void) argc;
  (void) argv;
  return 77; /* WIN32-only entry point */
#endif
}

static int st_metacharset(httrackp *opt, int argc, char **argv) {
  char *s;

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "metacharset: needs an html string\n");
    return 1;
  }
  s = hts_getCharsetFromMeta(argv[0], strlen(argv[0]));
  printf("%s\n", s != NULL ? s : "(none)");
  freet(s);
  return 0;
}

static int st_isutf8(httrackp *opt, int argc, char **argv) {
  char buf[512];
  size_t len;

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "isutf8: needs a string\n");
    return 1;
  }
  len = st_decode_body(argv[0], buf, sizeof(buf));
  printf("%d\n", hts_isStringUTF8(buf, len) ? 1 : 0);
  return 0;
}

static int st_idna_encode(httrackp *opt, int argc, char **argv) {
  char buf[512];
  size_t len;
  char *s;

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "idna-encode: needs a hostname\n");
    return 1;
  }
  len = st_decode_body(argv[0], buf, sizeof(buf));
  s = hts_convertStringUTF8ToIDNA(buf, len);
  if (s != NULL) {
    printf("%s\n", s);
    freet(s);
  } else {
    fprintf(stderr, "invalid string '%s'\n", argv[0]);
  }
  return 0;
}

static int st_idna_decode(httrackp *opt, int argc, char **argv) {
  char *s;

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "idna-decode: needs a hostname\n");
    return 1;
  }
  s = hts_convertStringIDNAToUTF8(argv[0], strlen(argv[0]));
  if (s != NULL) {
    printf("%s\n", s);
    freet(s);
  } else {
    fprintf(stderr, "invalid string '%s'\n", argv[0]);
  }
  return 0;
}

static int st_entities(httrackp *opt, int argc, char **argv) {
  char *s;
  const char *enc;

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "entities: needs a string\n");
    return 1;
  }
  s = strdupt(argv[0]);
  enc = argc >= 2 ? argv[1] : "UTF-8";
  if (s != NULL &&
      hts_unescapeEntitiesWithCharset(s, s, strlen(s) + 1, enc) == 0) {
    printf("%s\n", s);
    freet(s);
  } else {
    fprintf(stderr, "invalid string '%s'\n", argv[0]);
  }
  return 0;
}

/* The unescapers must reserve one byte for the trailing NUL: a 'max'-byte
   dest holding 'max' output chars pre-fix wrote dest[max] (1-byte OOB, caught
   by ASan). Both unescapeEntities and unescapeUrl share the guard. */
static int st_unescape_bounds(httrackp *opt, int argc, char **argv) {
  char dest[4];

  (void) opt;
  (void) argc;
  (void) argv;
  assertf(hts_unescapeEntities("abcd", dest, sizeof(dest)) == -1);
  assertf(hts_unescapeUrl("abcd", dest, sizeof(dest)) == -1);
  assertf(hts_unescapeEntities("abc", dest, sizeof(dest)) == 0);
  assertf(strcmp(dest, "abc") == 0);
  /* raw multi-byte UTF-8 flush path (bypasses the per-byte guard) */
  assertf(hts_unescapeUrl("ab\xC3\xA9", dest, sizeof(dest)) == -1);
  assertf(hts_unescapeUrl("a\xC3\xA9", dest, sizeof(dest)) == 0);
  assertf(strcmp(dest, "a\xC3\xA9") == 0);
  {
    /* %xx-encoded flush path (utfBufferJ = lastJ rollback) */
    char wide[8];

    assertf(hts_unescapeUrl("%C3%A9", wide, sizeof(wide)) == 0);
    assertf(strcmp(wide, "\xC3\xA9") == 0);
  }
  printf("unescape-bounds self-test OK\n");
  return 0;
}

/* A malformed escape must survive as text. */
static int st_unescape_form(httrackp *opt, int argc, char **argv) {
  /* compared by length, so a decoded NUL is not mistaken for a short string */
  static const struct {
    const char *in;
    const char *expected;
    size_t len;
    int ini; /* run hts_unescapeini() rather than http */
  } cases[] = {
      {"", "", 0, 0},
      {"a%41b", "aAb", 3, 0},
      {"%c3%a9", "\xc3\xa9", 2, 0},
      {"%C3%A9", "\xc3\xa9", 2, 0}, /* hex case does not matter */
      {"a+b", "a b", 3, 0},
      {"100%%", "100%", 4, 0},
      {"%00", "\0", 1, 0},  /* well-formed, so it really does decode to NUL */
      {"%ZZ", "%ZZ", 3, 0}, /* not hex: kept literal, not decoded to NUL */
      {"%zz", "%zz", 3, 0}, /* lower case too, or a widened 'a'-'f' arm hides */
      {"%4", "%4", 2, 0},   /* truncated: no second digit to read */
      {"%", "%", 1, 0},
      {"%%%", "%%", 2, 0},
      {"a%2Gb", "a%2Gb", 5, 0}, /* second digit not hex */
      {"", "", 0, 1},
      {"a+b", "a+b", 3, 1},          /* the ini form has no '+' rule */
      {"a%0d%0ab", "a\rb", 3, 1},    /* a decoded separator run collapses */
      {"a%0d%0a%0db", "a\rb", 3, 1}, /* however long the run is */
      {"a\r\nb", "a\r\nb", 4, 1},    /* but raw separators pass through */
      {"a%ZZb", "a%ZZb", 5, 1},
  };

  size_t i;

  (void) opt;
  (void) argc;
  (void) argv;
  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    String out = STRING_EMPTY;
    const char *got;

    if (cases[i].ini) {
      hts_unescapeini(cases[i].in, &out);
    } else {
      hts_unescapehttp(cases[i].in, &out);
    }
    got = StringBuff(out);
    /* callers read the result with strcmp(), so the terminator is part of the
       contract */
    if (StringLength(out) != cases[i].len ||
        (cases[i].len != 0 &&
         memcmp(got, cases[i].expected, cases[i].len) != 0) ||
        (got != NULL && got[cases[i].len] != '\0')) {
      fprintf(stderr, "unescape-form: %s gave %d bytes, expected %d\n",
              cases[i].in, (int) StringLength(out), (int) cases[i].len);
      StringFree(out);
      return 1;
    }
    StringFree(out);
  }
  printf("unescape-form self-test OK\n");
  return 0;
}

/* escape_remove_control() compacts in place, so it has to terminate at the new
   end or the caller reads the compacted head plus the original tail (#974). */
/* append_escape_*() hands on what is LEFT of dest, so a remainder equal to
   sizeof(void *) used to trip the size heuristic meant for caller mistakes. */
static int st_escape_append_room(httrackp *opt, int argc, char **argv) {
  char buf[64];
  size_t room;

  (void) opt;
  (void) argc;
  (void) argv;
  for (room = 1; room <= 2 * sizeof(void *); room++) {
    const size_t used = sizeof(buf) - room;
    size_t ret;

    memset(buf, 'a', used);
    buf[used] = '\0';
    ret = append_escape_check_url("bc", buf, sizeof(buf));
    /* "bc" plus its NUL needs three bytes; the ambiguous size reports full */
    if (room < 3 || room == sizeof(void *)) {
      assertf(ret == room);
    } else {
      assertf(ret == 2);
      assertf(strlen(buf) == used + 2);
    }
    assertf(strlen(buf) <= used + 2);
  }
  printf("escape-append-room self-test OK\n");
  return 0;
}

static int st_escape_control(httrackp *opt, int argc, char **argv) {
  static const struct {
    const char *in;
    const char *out;
  } cases[] = {
      /* VT and FF are the ones that reach here: is_space() passes them */
      {"/a\013bc", "/abc"},
      {"\014abc", "abc"},
      /* nothing moves, but the end does */
      {"abc\013", "abc"},
      {"abc\001def", "abcdef"},
      {"\001\002\003", ""},
      /* untouched inputs: the terminator must stay where it was */
      {"abc", "abc"},
      {"", ""},
      /* only bytes below 32 go: DEL and high bytes are not control here */
      {"a\177\303\251", "a\177\303\251"},
      /* both sides of the >= 32 cut, and a shrink of more than one byte */
      {"a b", "a b"},
      {"a\037b\036c", "abc"},
  };

  const size_t ncases = sizeof(cases) / sizeof(cases[0]);
  char buf[1024];
  size_t k, m;

  (void) opt;
  if (argc > 0) {
    const size_t n = st_decode_body(argv[0], buf, sizeof(buf));

    assertf(n < sizeof(buf));
    escape_remove_control(buf);
    printf("escape-control: len=%d out=hex:", (int) strlen(buf));
    for (k = 0; buf[k] != '\0'; k++) {
      printf("%02x", (unsigned char) buf[k]);
    }
    printf("\n");
    return 0;
  }

  for (k = 0; k < ncases; k++) {
    const size_t inlen = strlen(cases[k].in);
    const size_t outlen = strlen(cases[k].out);

    /* poison, so a stray write shows up as a byte a zeroed buffer would hide */
    memset(buf, '#', sizeof(buf));
    memcpy(buf, cases[k].in, inlen + 1);
    escape_remove_control(buf);
    assertf(strlen(buf) == outlen);
    assertf(memcmp(buf, cases[k].out, outlen + 1) == 0);
    /* the terminator belongs inside the original string, never past its NUL */
    for (m = inlen + 1; m < sizeof(buf); m++) {
      assertf(buf[m] == '#');
    }
  }
  printf("escape-control self-test OK\n");
  return 0;
}

/* Each inplace_escape_*() must equal escape_*() on a copy. */
static int st_inplace_escape(httrackp *opt, int argc, char **argv) {
  /* >255 bytes forces the helper's malloct path, not the stack buffer */
  static char longstr[600];
  static const char *const samples[] = {
      "",          "abc",           "a b/c?d=e&f", "h\x8ello w\x94rld",
      "a%b\"c<d>", "/path to/file", longstr};
  static size_t (*const inplace[])(char *, size_t) = {
      inplace_escape_in_url, inplace_escape_spc_url, inplace_escape_uri_utf,
      inplace_escape_check_url, inplace_escape_uri};
  static size_t (*const plain[])(const char *, char *, size_t) = {
      escape_in_url, escape_spc_url, escape_uri_utf, escape_check_url,
      escape_uri};
  size_t i, f;

  (void) opt;
  (void) argc;
  (void) argv;

  memset(longstr, 'a', sizeof(longstr) - 1);
  for (f = 0; f < sizeof(inplace) / sizeof(inplace[0]); f++) {
    for (i = 0; i < sizeof(samples) / sizeof(samples[0]); i++) {
      char ref[4096], work[4096];
      size_t rret, iret;
      rret = plain[f](samples[i], ref, sizeof(ref));
      strcpybuff(work, samples[i]);
      iret = inplace[f](work, sizeof(work));
      assertf(iret == rret);
      assertf(strcmp(work, ref) == 0);
    }
  }
  printf("inplace-escape self-test OK\n");
  return 0;
}

/* Pin HTS_HTMLESCAPE*_MAXEXP to each escaper's true max byte expansion. */
static int st_escape_room(httrackp *opt, int argc, char **argv) {
  /* N > 1023: where 6n outgrows the old 5n+1024 reservation */
  enum { N = 2000 };

  char *src = malloct(N + 1);
  char *dst;
  size_t room, got;
  (void) opt;
  (void) argc;
  (void) argv;

  /* _full worst case: a high byte expands to "&#xHH;" (6 bytes) */
  memset(src, 0xE9, N);
  src[N] = '\0';
  room = (size_t) N * HTS_HTMLESCAPE_FULL_MAXEXP + 1024;
  dst = malloct(room);
  got = escape_for_html_print_full(src, dst, room);
  assertf(got == (size_t) N * HTS_HTMLESCAPE_FULL_MAXEXP);
  assertf(strlen(dst) == got);
  freet(dst);

  /* one factor short overflows (returns size), truncating the page: the bug */
  room = (size_t) N * (HTS_HTMLESCAPE_FULL_MAXEXP - 1) + 1024;
  dst = malloct(room);
  got = escape_for_html_print_full(src, dst, room);
  assertf(got == room);
  freet(dst);

  /* plain escaper worst case: '&' -> "&amp;" (5); high bytes stay verbatim */
  memset(src, '&', N);
  src[N] = '\0';
  room = (size_t) N * HTS_HTMLESCAPE_MAXEXP + 1024;
  dst = malloct(room);
  got = escape_for_html_print(src, dst, room);
  assertf(got == (size_t) N * HTS_HTMLESCAPE_MAXEXP);
  assertf(strlen(dst) == got);
  freet(dst);

  freet(src);
  printf("escape-room self-test OK\n");
  return 0;
}

/* A '?' starts the query wherever it sits, the first byte included, so a '+'
   after it decodes to a space there too. */
static int st_unescape_plus(httrackp *opt, int argc, char **argv) {
  static const struct {
    const char *src;
    const char *want;
  } cases[] = {
      {"?a+b", "?a b"},     /* the query opens on the first byte */
      {"x?a+b", "x?a b"},   /* and anywhere after it */
      {"a+b", "a+b"},       /* no query, so '+' stays literal */
      {"%3Fa+b", "?a+b"},   /* an escaped '?' does not open the query */
      {"?%41+%42", "?A B"}, /* %xx decoding still runs inside the query */
      {"%41+%42", "A+B"},   /* and outside it */
      /* opening the query also cancels a pending UTF-8 pair at the '+' */
      {"?\xC3%+%\xA9", "?\xC3% %\xA9"},
  };

  size_t i;

  (void) opt;
  (void) argc;
  (void) argv;
  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    char dst[64];

    assertf(hts_unescapeUrlSpecial(cases[i].src, dst, sizeof(dst), 0) == 0);
    assertf(strcmp(dst, cases[i].want) == 0);
  }
  printf("unescape-plus self-test OK\n");
  return 0;
}

/* ------------------------------------------------------------ */
/* Registry: this module's tests, in the order -#test lists them. */
/* ------------------------------------------------------------ */

const struct selftest_entry selftests_charset[] = {
    {"charset", "<charset> <hex:..|string>",
     "convert a string to UTF-8 from a charset", st_charset},
    {"syscharset", "", "UTF-8 <-> system codepage conversion (WIN32 only)",
     st_syscharset},
    {"nobestfit", "",
     "no best-fit substitute when converting from UTF-8 (WIN32 only)",
     st_nobestfit},
    {"metacharset", "<html>", "extract the <meta> charset from an HTML page",
     st_metacharset},
    {"isutf8", "<hex:..|string>", "is the string valid UTF-8 (1/0)", st_isutf8},
    {"idna-encode", "<host>", "encode a hostname to IDNA/punycode",
     st_idna_encode},
    {"idna-decode", "<host>", "decode an IDNA/punycode hostname",
     st_idna_decode},
    {"entities", "<string> [encoding]", "unescape HTML entities", st_entities},
    {"unescape-bounds", "", "unescapers reserve the NUL byte (no 1-byte OOB)",
     st_unescape_bounds},
    {"unescape-plus", "",
     "a '?' opens the query even as the first byte, so a later '+' is a space",
     st_unescape_plus},
    {"unescape-form", "", "form/ini percent-decoding keeps a malformed escape",
     st_unescape_form},
    {"escape-append-room", "",
     "append_escape_*() tolerates a pointer-sized remainder",
     st_escape_append_room},
    {"escape-control", "[hex:..|string]",
     "escape_remove_control() terminates at the compacted end",
     st_escape_control},
    {"inplace-escape", "", "inplace_escape_* vs escape_* equivalence self-test",
     st_inplace_escape},
    {"escape-room", "", "HT_ADD_HTMLESCAPED* reservation-factor self-test",
     st_escape_room},
    {NULL, NULL, NULL, NULL},
};
