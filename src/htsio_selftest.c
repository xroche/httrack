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
/* File: htsio_selftest.c subroutines:                          */
/*       self-tests for file and directory I/O                  */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htsselftest_int.h"

/* fsize()/fsize_utf8()/fpsize() must report a size past 4GB: 32-bit wraps both
   ways there (MSVC's off_t and struct _stat st_size are long, 32-bit even on
   x64), and a size under 4GB would survive an *unsigned* 32-bit truncation. */
static int st_fsize(httrackp *opt, int argc, char **argv) {
  const LLint expected = 5 * 1024 * 1024 * 1024LL;
  char BIGSTK path[HTS_URLMAXSIZE * 2];
  char BIGSTK absent[HTS_URLMAXSIZE * 2];
  /* both variants: only fsize() feeds the >2GB readers (file://, --list) */
  const int width = (int) sizeof(fsize(""));
  const int width_utf8 = (int) sizeof(fsize_utf8(""));
  FILE *fp;
  LLint got, got_utf8, gotp, gone;
  int rc = 0;

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "fsize: needs a directory\n");
    return 1;
  }
  concat(path, sizeof(path), argv[0], "/sparse-5g.bin");
  concat(absent, sizeof(absent), argv[0], "/no-such-file.bin");

  /* sparse: seek past 4GB and write the last byte */
  fp = FOPEN(path, "wb");
  if (fp == NULL) {
    fprintf(stderr, "fsize: cannot create '%s': %s\n", path, strerror(errno));
    return 1;
  }
#ifdef _WIN32
  {
    /* NTFS allocates the hole unless asked not to; POSIX gives it for free.
       Best-effort: a non-sparse file still measures the same, just costs 5GB.
     */
    HANDLE h = (HANDLE) _get_osfhandle(_fileno(fp));
    DWORD ret;

    if (h != INVALID_HANDLE_VALUE)
      (void) DeviceIoControl(h, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &ret, NULL);
  }
#endif
  if (fseeko(fp, expected - 1, SEEK_SET) != 0 || fputc(0, fp) == EOF ||
      fclose(fp) != 0) {
    const int err = errno;

    fprintf(stderr, "fsize: cannot extend '%s' to " LLintP ": %s\n", path,
            expected, strerror(err));
    UNLINK(path);
    /* EFBIG: file-size cap below 5GB (GNU/Hurd ext2fs); skip, don't fail. */
    return err == EFBIG ? 77 : 1;
  }

  got = fsize(path);
  got_utf8 = fsize_utf8(path);
  fp = FOPEN(path, "rb");
  if (fp == NULL) {
    fprintf(stderr, "fsize: cannot reopen '%s': %s\n", path, strerror(errno));
    gotp = -1;
  } else {
    gotp = fpsize(fp);
    fclose(fp);
  }
  UNLINK(path);
  gone = fsize(absent); /* contract: -1, not 0, when absent */

  printf("fsize: width=%d,%d size=" LLintP "," LLintP " psize=" LLintP
         " absent=" LLintP "\n",
         width, width_utf8, got, got_utf8, gotp, gone);
  if (width != 8 || width_utf8 != 8) {
    fprintf(stderr, "fsize: return types are %d/%d bytes, expected 8\n", width,
            width_utf8);
    rc = 1;
  }
  if (got != expected || got_utf8 != expected) {
    fprintf(stderr,
            "fsize: fsize/fsize_utf8 are " LLintP "/" LLintP
            ", expected " LLintP "\n",
            got, got_utf8, expected);
    rc = 1;
  }
  if (gotp != expected) {
    fprintf(stderr, "fsize: fpsize is " LLintP ", expected " LLintP "\n", gotp,
            expected);
    rc = 1;
  }
  if (gone != -1) {
    fprintf(stderr, "fsize: absent file is " LLintP ", expected -1\n", gone);
    rc = 1;
  }
  return rc;
}

/* The deprecated form is what this test measures, so silence it here only. */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
static int st_findsize_legacy(find_handle find) {
  return hts_findgetsize(find);
}

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

/* Sparse file of exactly `size` bytes; 77 where the host cannot hold it. */
static int st_findsize_make(const char *path, LLint size) {
  FILE *fp = FOPEN(path, "wb");
  hts_boolean ok;
  int err;

  if (fp == NULL) {
    fprintf(stderr, "findsize: cannot create '%s': %s\n", path,
            strerror(errno));
    return 1;
  }
#ifdef _WIN32
  {
    /* NTFS allocates the hole unless asked not to; POSIX gives it for free. */
    HANDLE fh = (HANDLE) _get_osfhandle(_fileno(fp));
    DWORD ret;

    if (fh != INVALID_HANDLE_VALUE)
      (void) DeviceIoControl(fh, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &ret,
                             NULL);
  }
#endif
  ok = fseeko(fp, size - 1, SEEK_SET) == 0 && fputc(0, fp) != EOF ? HTS_TRUE
                                                                  : HTS_FALSE;
  err = errno;
  if (fclose(fp) != 0 && ok) {
    ok = HTS_FALSE;
    err = errno;
  }
  if (ok)
    return 0;
  fprintf(stderr, "findsize: cannot extend '%s' to " LLintP ": %s\n", path,
          size, strerror(err));
  UNLINK(path);
  /* the host cannot hold the probe (small file cap, full disk): skip */
  return err == EFBIG || err == ENOSPC ? 77 : 1;
}

/* Three entries measured through both find-size forms. 6GB is the one that
   catches a sign-extended low dword: bit 31 of 0x1_8000_0000 is set, where
   5GB's 0x4000_0000 and 1234 both leave it clear. */
static int st_findsize(httrackp *opt, int argc, char **argv) {
  const LLint big_expected = 5 * 1024 * 1024 * 1024LL;
  const LLint sign_expected = 6 * 1024 * 1024 * 1024LL;
  const LLint small_expected = 1234;
  char BIGSTK big[HTS_URLMAXSIZE * 2];
  char BIGSTK sign[HTS_URLMAXSIZE * 2];
  char BIGSTK small[HTS_URLMAXSIZE * 2];
  const int width = (int) sizeof(hts_findgetsize64(NULL));
  LLint big_got = -2, sign_got = -2, small_got = -2;
  int big_got32 = -2, sign_got32 = -2, small_got32 = -2;
  find_handle h;
  int rc;

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "findsize: needs a directory\n");
    return 1;
  }
  concat(big, sizeof(big), argv[0], "/find-5g.bin");
  concat(sign, sizeof(sign), argv[0], "/find-6g.bin");
  concat(small, sizeof(small), argv[0], "/find-small.bin");

  rc = st_findsize_make(big, big_expected);
  if (rc == 0)
    rc = st_findsize_make(sign, sign_expected);
  if (rc == 0)
    rc = st_findsize_make(small, small_expected);
  if (rc != 0) {
    UNLINK(big);
    UNLINK(sign);
    UNLINK(small);
    return rc;
  }

  h = hts_findfirst(argv[0]);
  if (h == NULL) {
    fprintf(stderr, "findsize: cannot enumerate '%s'\n", argv[0]);
    rc = 1;
  } else {
    do {
      const char *const name = hts_findgetname(h);

      if (name == NULL)
        continue;
      if (strcmp(name, "find-5g.bin") == 0) {
        big_got = hts_findgetsize64(h);
        big_got32 = st_findsize_legacy(h);
      } else if (strcmp(name, "find-6g.bin") == 0) {
        sign_got = hts_findgetsize64(h);
        sign_got32 = st_findsize_legacy(h);
      } else if (strcmp(name, "find-small.bin") == 0) {
        small_got = hts_findgetsize64(h);
        small_got32 = st_findsize_legacy(h);
      }
    } while (hts_findnext(h));
    hts_findclose(h);
  }
  UNLINK(big);
  UNLINK(sign);
  UNLINK(small);
  if (rc != 0)
    return rc;

  printf("findsize: width=%d big=" LLintP ",%d signbit=" LLintP
         ",%d small=" LLintP ",%d\n",
         width, big_got, big_got32, sign_got, sign_got32, small_got,
         small_got32);
  if (width != 8) {
    fprintf(stderr, "findsize: return type is %d bytes, expected 8\n", width);
    rc = 1;
  }
  if (big_got != big_expected || sign_got != sign_expected) {
    fprintf(stderr,
            "findsize: 5GB/6GB files are " LLintP "/" LLintP
            ", expected " LLintP "/" LLintP "\n",
            big_got, sign_got, big_expected, sign_expected);
    rc = 1;
  }
  if (big_got32 != -1 || sign_got32 != -1) {
    fprintf(stderr,
            "findsize: deprecated form is %d/%d on the 5GB/6GB files, "
            "expected the -1 sentinel\n",
            big_got32, sign_got32);
    rc = 1;
  }
  if (small_got != small_expected || small_got32 != (int) small_expected) {
    fprintf(stderr, "findsize: " LLintP "-byte file is " LLintP ",%d\n",
            small_expected, small_got, small_got32);
    rc = 1;
  }
  return rc;
}

/* 4GB+100KB wraps to ~108KB through an int, and needs 33 unsigned bits. A
   macro, not a static const: MSVC's C mode (/TC) rejects a const object
   used inside another object's static initializer below (C2099). */
#define HTS_ST_GROWSIZE_OVER32 (4LL * 1024 * 1024 * 1024 + 100 * 1024)

/* llint_grow_size_t() sizes the buffer holding a whole -%S list file: the
   result must be the exact 64-bit sum or a clean refusal, never a short one. */
static int st_growsize(httrackp *opt, int argc, char **argv) {
  enum { REFUSE, ACCEPT, WIDTH };

  static const struct {
    size_t used;
    LLint extra;
    size_t slack;
    int want;
  } cases[] = {
      {0, 0, 0, ACCEPT},
      {10, 100, 8192, ACCEPT},
      {(size_t) -2 - 8, 4, 4, ACCEPT}, /* exact fit, no room to spare */
      {(size_t) -2, 0, 0, ACCEPT},     /* largest representable capacity */
      {0, -1, 0, REFUSE},              /* fsize() failure */
      /* -1 already maps to SIZE_MAX; only this exercises the negative guard */
      {0, -4096, 0, REFUSE},
      {(size_t) -1, 1, 0, REFUSE},
      {(size_t) -2, 0, 1, REFUSE},     /* slack alone overruns */
      {(size_t) -1 - 8, 4, 4, REFUSE}, /* total would be the error value */
      {(size_t) -1 - 8, 4, 8, REFUSE},
      {0, HTS_ST_GROWSIZE_OVER32, 8192,
       WIDTH}, /* 32-bit size_t can't hold these */
      {10, HTS_ST_GROWSIZE_OVER32, 8192, WIDTH},
  };

  /* 0xffffffff doubles as a 32-bit size_t's error value, so it stays out */
  static const LLint narrowing[] = {0,  1,    8192, HTS_ST_GROWSIZE_OVER32,
                                    -1, -4096};

  size_t k;
  int rc = 0;

  (void) opt;
  (void) argc;
  (void) argv;
  for (k = 0; k < sizeof(cases) / sizeof(cases[0]); k++) {
    const size_t used = cases[k].used, slack = cases[k].slack;
    const LLint extra = cases[k].extra;
    const size_t got = llint_grow_size_t(used, extra, slack);
    const hts_boolean refused = got == (size_t) -1 ? HTS_TRUE : HTS_FALSE;
    const hts_boolean exact =
        !refused && extra >= 0 && got - used - slack == (size_t) extra;
    hts_boolean ok;

    switch (cases[k].want) {
    case ACCEPT:
      ok = exact;
      break;
    case REFUSE:
      ok = refused;
      break;
    default:
      ok = sizeof(size_t) >= sizeof(LLint) ? exact : refused;
      break;
    }
    if (!ok) {
      fprintf(stderr,
              "growsize: grow(" LLintP ", " LLintP ", " LLintP ") = " LLintP
              " (want %s)\n",
              (LLint) used, extra, (LLint) slack, (LLint) got,
              cases[k].want == REFUSE ? "refusal" : "exact sum");
      rc = 1;
    }
  }

  /* llint_to_size_t() refuses only what size_t cannot hold: a negative size
     round-trips intact on LP64, so callers check the sign themselves. */
  for (k = 0; k < sizeof(narrowing) / sizeof(narrowing[0]); k++) {
    const LLint v = narrowing[k];
    const hts_boolean representable =
        sizeof(size_t) >= sizeof(LLint) || (v >= 0 && v <= 0xffffffffLL);
    const size_t want = representable ? (size_t) v : (size_t) -1;
    const size_t got = llint_to_size_t(v);

    if (got != want) {
      fprintf(stderr,
              "growsize: narrow(" LLintP ") = " LLintP " (want " LLintP ")\n",
              v, (LLint) got, (LLint) want);
      rc = 1;
    }
  }

  printf("growsize self-test %s\n", rc == 0 ? "OK" : "FAILED");
  return rc;
}

/* an empty fil started htsAddLink's codebase walk before the buffer (#730) */
static int st_addlink(httrackp *opt, int argc, char **argv) {
  htsmoduleStruct BIGSTK str;
  cache_back cache;
  struct_back *sback;
  hash_struct hash;
  int ptr = 0;
  int rc = 0;
  int i;

  (void) argc;
  (void) argv;

  memset(&cache, 0, sizeof(cache));
  cache.hashtable = (void *) coucal_new(0);
  st_mirror_wiring(opt, &sback, &hash, HTS_TRUE);

  memset(&str, 0, sizeof(str));
  str.opt = opt;
  str.sback = sback;
  str.cache = &cache;
  str.hashptr = &hash;
  str.ptr_ = &ptr;
  str.addLink = htsAddLink;

  /* [0] is the underflow; [1] and [2] are controls that the trim is unchanged.
     A query-only link is the one that notices the trim at all: for the others
     ident_url_relatif() re-derives the directory from the path it is given. */
  for (i = 0; i < 3; i++) {
    static const char *const fil[3] = {"", "/dir/page.html", "/dir/page.html"};
    static const char *const lnk[3] = {"sub/page.html", "sub/page.html",
                                       "?x=1"};
    static const char *const want[3] = {
        "untouched", "http://www.example.com/dir/sub/page.html",
        "http://www.example.com/dir/?x=1"};
    char BIGSTK loc[HTS_URLMAXSIZE * 2];
    char BIGSTK link[HTS_URLMAXSIZE];

    strcpybuff(loc, "untouched");
    strcpybuff(link, lnk[i]);
    str.localLink = loc;
    str.localLinkSize = (int) sizeof(loc);
    if (!hts_record_link(opt, "www.example.com", fil[i], "", "", "", "")) {
      rc = 1;
      goto cleanup;
    }
    ptr = heap_top_index();
    str.url_host = heap(ptr)->adr;
    str.url_file = heap(ptr)->fil;
    assertf(htsAddLink(&str, link) == 0); /* refused by the wizard either way */
    if (strcmp(loc, want[i]) != 0) {
      fprintf(stderr, "addlink[%d]: got '%s' want '%s'\n", i, loc, want[i]);
      rc = 1;
      goto cleanup;
    }
  }

  printf("addlink self-test OK\n");

cleanup:
  st_mirror_wiring_free(opt, &cache, &sback, &hash);
  return rc;
}

static void makeindex_run(httrackp *opt, const char *path, const char *footer,
                          int links, const char *firstlink, char *buf,
                          size_t size) {
  FILE *fp = fopen(path, "wb");
  int done = 0;
  size_t n;

  assertf(fp != NULL);
  hts_finish_makeindex(opt, &done, &fp, links, firstlink, footer, "", "");
  assertf(fp == NULL); /* the function closed and cleared it */
  assertf(done != 0);
  fp = fopen(path, "rb");
  assertf(fp != NULL);
  n = fread(buf, 1, size - 1, fp);
  fclose(fp);
  buf[n] = '\0';
}

// Whole file as a NUL-terminated heap string, or NULL when it cannot be read.
static char *makeindex_slurp(const char *path) {
  FILE *fp = fopen(path, "rb");
  long size;
  char *buf;

  if (fp == NULL)
    return NULL;
  assertf(fseek(fp, 0, SEEK_END) == 0);
  size = ftell(fp);
  assertf(size >= 0);
  rewind(fp);
  buf = malloct((size_t) size + 1);
  /* a read error would otherwise render a silently truncated template */
  assertf(hts_fread_exact(buf, (size_t) size, fp));
  buf[size] = '\0';
  fclose(fp);
  return buf;
}

// Whether `at` sits in live markup rather than inside an HTML comment.
static hts_boolean makeindex_outside_comment(const char *buf, const char *at) {
  const char *open = NULL, *close = NULL, *p;

  for (p = buf; (p = strstr(p, "<!--")) != NULL && p < at; p += 4)
    open = p;
  for (p = buf; (p = strstr(p, "-->")) != NULL && p < at; p += 3)
    close = p;
  return open == NULL || (close != NULL && close > open) ? HTS_TRUE : HTS_FALSE;
}

// %-contract of a built-in template: the slots its caller fills, and no escape
// besides %%.
static void makeindex_template_contract(const char *name, const char *tmpl,
                                        size_t slots) {
  size_t i, n;

  for (i = 0, n = 0; tmpl[i] != '\0'; i++) {
    if (tmpl[i] != '%')
      continue;
    if (tmpl[i + 1] == 's')
      n++;
    else if (tmpl[i + 1] != '%') {
      fprintf(stderr, "%s: stray %%%c escape\n", name, tmpl[i + 1]);
      assertf(!"built-in template carries an escape that is neither %s nor %%");
    }
    i++;
  }
  if (n != slots) {
    fprintf(stderr, "%s: %d %%s slots, expected %d\n", name, (int) n,
            (int) slots);
    assertf(!"built-in template slot count drifted from its caller");
  }
}

// hts_finish_makeindex writes the footer, emits the refresh meta only when
// makeindex_links==1, and clears *fp / sets *done. argv[0] is a writable dir,
// argv[1] is an optional templates/ whose on-disk footer is rendered too.
static int st_makeindex(httrackp *opt, int argc, char **argv) {
  /* the minimal template is the control: it proves an assertion below can only
     fail on the real one because that template lost a slot */
  const char *footers[] = {"%s%s", HTS_INDEX_FOOTER, NULL};
  size_t nfooters = sizeof(footers) / sizeof(*footers) - 1;
  char *ondisk = NULL;
  char path[HTS_URLMAXSIZE];
  char BIGSTK buf[8192];
  size_t t;

  assertf(argc >= 1);
  snprintf(path, sizeof(path), "%s/index.html", argv[0]);

  /* heap-allocated so a sanitizer sees an overrun, and poisoned so the stray
     NUL one writes cannot pass for the terminator */
  {
    char *trailing = strdupt("credit %s, then a stray %");
    const char *const want = "credit X, then a stray %";
    char out[64];
    size_t i;

    memset(out, 'Z', sizeof(out));
    assertf(hts_template_format_str(out, sizeof(out), trailing, "X", NULL) >=
            0);
    assertf(strcmp(out, want) == 0);
    for (i = strlen(want) + 1; i < sizeof(out); i++)
      assertf(out[i] == 'Z');
    freet(trailing);
  }

  makeindex_template_contract("HTS_INDEX_HEADER", HTS_INDEX_HEADER, 1);
  makeindex_template_contract("HTS_INDEX_BODY", HTS_INDEX_BODY, 2);
  makeindex_template_contract("HTS_TOPINDEX_BODYCAT", HTS_TOPINDEX_BODYCAT, 1);
  makeindex_template_contract("HTS_INDEX_FOOTER", HTS_INDEX_FOOTER, 2);
  makeindex_template_contract("HTS_TOPINDEX_HEADER", HTS_TOPINDEX_HEADER, 1);
  makeindex_template_contract("HTS_TOPINDEX_BODY", HTS_TOPINDEX_BODY, 2);
  makeindex_template_contract("HTS_TOPINDEX_FOOTER", HTS_TOPINDEX_FOOTER, 1);
  /* htsparse.c writes this one as an fprintf argument, so a % reaches
     external.html as typed: a %s prints unfilled and a %% prints doubled */
  {
    const char *const verbatim = HTS_DATA_UNKNOWN_HTML;

    assertf(strstr(verbatim, "%s") == NULL);
    assertf(strstr(verbatim, "%%") == NULL);
  }

  /* the shipped copy, so the same scenarios run through the file a real install
     reads and not only through the fallback macro */
  if (argc >= 2) {
    char tmpl[HTS_URLMAXSIZE];

    snprintf(tmpl, sizeof(tmpl), "%s/index-footer.html", argv[1]);
    ondisk = makeindex_slurp(tmpl);
    assertf(ondisk != NULL);
    footers[nfooters++] = ondisk;
  }

  for (t = 0; t < nfooters; t++) {
    const char *const footer = footers[t];

    /* single first link: footer + a refresh meta carrying the escaped URL */
    makeindex_run(opt, path, footer, 1, "http://example.com/a b", buf,
                  sizeof(buf));
    {
      const char *const credit = strstr(buf, "<!-- Mirror and index made by");
      const char *const meta = strstr(buf, "<meta HTTP-EQUIV=\"Refresh\"");

      assertf(credit != NULL);
      assertf(meta != NULL);
      assertf(strstr(meta, "example.com") != NULL);
      /* the slots are positional: credit first, and a redirect the browser
         never sees is no redirect */
      assertf(credit < meta);
      assertf(makeindex_outside_comment(buf, credit));
      assertf(makeindex_outside_comment(buf, meta));
    }

    /* a first link whose escaped form overruns the old flat 1024-byte tempo:
       the redirect must carry the whole URL, not a clipped prefix */
    {
      char BIGSTK link[HTS_URLMAXSIZE * 2];
      char *p = link;

      strcpybuff(link, "http://example.com/");
      p += strlen(link);
      memset(p, 'a', 1200);
      p += 1200;
      strcpy(p, "/end.html");

      makeindex_run(opt, path, footer, 1, link, buf, sizeof(buf));
      /* the closing quote proves the URL was not clipped mid-way */
      assertf(strstr(buf, "/end.html\">") != NULL);
    }

    /* no single link: footer only, no refresh meta */
    makeindex_run(opt, path, footer, 0, NULL, buf, sizeof(buf));
    assertf(strstr(buf, "Mirror and index made by HTTrack") != NULL);
    assertf(strstr(buf, "Refresh") == NULL);
  }

  freet(ondisk);
  UNLINK(path);
  printf("makeindex self-test OK\n");
  return 0;
}

static void datadir_expect(const char *selfpath, const char *builtin,
                           const char *expect) {
  char got[HTS_URLMAXSIZE * 2];

  hts_resolve_datadir(got, sizeof(got), selfpath, builtin);
  if (strcmp(got, expect) != 0) {
    fprintf(stderr,
            "datadir: self=%s builtin=\"%s\" gave \"%s\", expected \"%s\"\n",
            selfpath != NULL ? selfpath : "(null)", builtin, got, expect);
  }
  assertf(strcmp(got, expect) == 0);
}

// -#test=datadir <dir>: a relocated tree must find its own templates instead of
// silently falling back to the built-in ones (#894). argv[0] is writable.
static int st_datadir(httrackp *opt, int argc, char **argv) {
  char path[HTS_URLMAXSIZE];
  char probe[HTS_URLMAXSIZE];
  char untouched[HTS_URLMAXSIZE];
  char self[HTS_URLMAXSIZE];
  char expect[HTS_URLMAXSIZE * 2];
  char installed[HTS_URLMAXSIZE];
  char gone[HTS_URLMAXSIZE];
  size_t selflen;
  /* Each holds a templates/index-header.html, the file path_bin is read for.
     nest/ keeps the flat case away from the installed share/httrack above. */
  static const char *const dirs[] = {"share/httrack", "bin", "nest/flat"};
  size_t i;

  (void) opt;
  assertf(argc >= 1);

  /* argv[0] is a fallback: what the engine actually resolves from is this. */
  assertf(hts_self_path(path, sizeof(path)) != NULL);
  assertf(fexist(path));
  /* A buffer the path does not fit in must refuse rather than clip. A refusal
     empties the buffer and writes nothing past the size it was given. */
  selflen = strlen(path);
  assertf(selflen < sizeof(probe));
  memset(untouched, 'X', sizeof(untouched));
  /* A zero size has nothing to empty, so it refuses without writing at all. */
  memset(probe, 'X', sizeof(probe));
  assertf(hts_self_path(probe, 0) == NULL);
  assertf(memcmp(probe, untouched, sizeof(probe)) == 0);
  for (i = 1; i <= selflen; i++) {
    memset(probe, 'X', sizeof(probe));
    assertf(hts_self_path(probe, i) == NULL);
    assertf(memcmp(probe + i, untouched, sizeof(probe) - i) == 0);
    assertf(probe[0] == '\0');
  }

  for (i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
    FILE *fp;

    snprintf(path, sizeof(path), "%s/%s/templates/", argv[0], dirs[i]);
    assertf(structcheck(path) == 0);
    snprintf(path, sizeof(path), "%s/%s/templates/index-header.html", argv[0],
             dirs[i]);
    fp = fopen(path, "wb");
    assertf(fp != NULL);
    fclose(fp);
  }
  snprintf(installed, sizeof(installed), "%s/share/httrack/", argv[0]);
  snprintf(gone, sizeof(gone), "%s/gone/", argv[0]);

  /* A moved install: bin/ carries templates too, so this pins the order. */
  snprintf(self, sizeof(self), "%s/bin/httrack", argv[0]);
  snprintf(expect, sizeof(expect), "%s/bin/../share/httrack/", argv[0]);
  datadir_expect(self, gone, expect);

  /* The compiled-in path still wins when it exists. */
  datadir_expect(self, installed, installed);

  /* Flat layout: templates/ sits beside the binary. */
  snprintf(self, sizeof(self), "%s/nest/flat/httrack", argv[0]);
  snprintf(expect, sizeof(expect), "%s/nest/flat/", argv[0]);
  datadir_expect(self, gone, expect);

  /* Nothing to derive from, or nothing found: the compiled-in path stands. */
  datadir_expect("httrack", gone, gone);
  datadir_expect(NULL, gone, gone);
  snprintf(self, sizeof(self), "%s/nowhere/deep/httrack", argv[0]);
  datadir_expect(self, gone, gone);

  /* No compiled-in path, as on Windows: the executable's own directory. */
  snprintf(self, sizeof(self), "%s/nowhere/deep/httrack", argv[0]);
  snprintf(expect, sizeof(expect), "%s/nowhere/deep/", argv[0]);
  datadir_expect(self, "", expect);
  datadir_expect("httrack", "", "");

  /* A directory part too long for the layout suffix to be appended must clip,
     not abort: appending to a non-empty buffer is the *_safe_ abort path. */
  {
    /* Long enough that dirname + "../share/httrack/" overflows the candidate
       buffer, short enough that the dirname itself still fits. */
    const size_t dirlen = HTS_URLMAXSIZE * 2 - 8;
    char huge[HTS_URLMAXSIZE * 3];
    char got[HTS_URLMAXSIZE * 2];
    size_t n;

    huge[0] = '/';
    for (n = 1; n < dirlen - 1; n++) {
      huge[n] = 'a';
    }
    huge[dirlen - 1] = '/';
    memcpy(huge + dirlen, "httrack", sizeof("httrack"));
    hts_resolve_datadir(got, sizeof(got), huge, gone);
    assertf(strcmp(got, gone) == 0);
  }

  printf("datadir self-test OK\n");
  return 0;
}

// -#test=pathbin: report what startup resolved, which -#test=datadir cannot see
// because it hands hts_resolve_datadir() the path instead (#904).
static int st_pathbin(httrackp *opt, int argc, char **argv) {
  (void) argc;
  (void) argv;
  printf("path_bin=%s\n", StringBuff(opt->path_bin));
  return 0;
}

// -#test=instpaths: the compiled-in install paths, which must follow configure
// rather than the hardcoded /usr Termux patched by hand.
static int st_instpaths(httrackp *opt, int argc, char **argv) {
  (void) opt;
  (void) argc;
  (void) argv;
#ifdef _WIN32
  return 77; /* Windows defines none of these */
#else
  printf("prefix=%s\n", HTS_PREFIX);
  printf("etcpath=%s\n", HTS_ETCPATH);
  printf("binpath=%s\n", HTS_BINPATH);
  printf("libpath=%s\n", HTS_LIBPATH);
  printf("httrackcnf=%s\n", HTS_HTTRACKCNF);
  printf("httrackdir=%s\n", HTS_HTTRACKDIR);
  return 0;
#endif
}

// hts_buildtopindex() writes a system-charset name into a charset=utf-8 doc: on
// Windows the gifs land in a mangled twin dir (#217) and a listed name renders
// as mojibake (#216). Both must come out utf-8. argv[0] is writable.
static int st_topindex(httrackp *opt, int argc, char **argv) {
  char topdir[HTS_URLMAXSIZE];
  char path[HTS_URLMAXSIZE + 32];
  char buf[16384]; /* the listing sits after the whole header template */
  FILE *fp;
  size_t n;
#ifdef _WIN32
  /* GUI writes ANSI paths and winprofile.ini; mimic it (CP1252) */
  static const char *const projName = "caf\xE9";
  static const char *const catName = "th\xE9";
#else
  /* POSIX system charset is already utf-8 */
  static const char *const projName = "caf\xC3\xA9";
  static const char *const catName = "th\xC3\xA9";
#endif
  /* utf-8 forms the index must carry whatever the input charset was */
  static const char *const projUTF8 = "caf\xC3\xA9";
  static const char *const catUTF8 = "th\xC3\xA9";

  assertf(argc >= 1);
  /* a non-ASCII top dir (#217) holding a non-ASCII sub-project (#216) */
  snprintf(topdir, sizeof(topdir), "%s/%s", argv[0], projName);
  snprintf(path, sizeof(path), "%s/%s/", topdir, projName);
  /* structcheck(), not the utf-8 MKDIR family: same charset as buildtopindex */
  assertf(structcheck(path) == 0);
  /* the sub-project is listed only if it holds an index.html */
  snprintf(path, sizeof(path), "%s/%s/index.html", topdir, projName);
  fp = fopen(path, "wb");
  assertf(fp != NULL);
  fclose(fp);
  /* a non-ASCII category exercises the winprofile.ini charset path (#216) */
  snprintf(path, sizeof(path), "%s/%s/hts-cache/", topdir, projName);
  assertf(structcheck(path) == 0);
  snprintf(path, sizeof(path), "%s/%s/hts-cache/winprofile.ini", topdir,
           projName);
  fp = fopen(path, "wb");
  assertf(fp != NULL);
  fprintf(fp, "category=%s\n", catName);
  fclose(fp);

  assertf(hts_buildtopindex(opt, topdir, "") != 0);

  /* #217: gifs land in the top dir itself, not in a mangled sibling */
  snprintf(path, sizeof(path), "%s/backblue.gif", topdir);
  assertf(fexist(path));

  /* #216: the listed name is utf-8, not raw system-charset mojibake */
  snprintf(path, sizeof(path), "%s/index.html", topdir);
  fp = fopen(path, "rb");
  assertf(fp != NULL);
  n = fread(buf, 1, sizeof(buf) - 1, fp);
  fclose(fp);
  buf[n] = '\0';
  assertf(strstr(buf, projUTF8) != NULL);
  assertf(strstr(buf, catUTF8) != NULL);
  /* the marker proves binpath "" found no templates/ to read, so the rest of
     this checks the compiled-in copies and not a stray on-disk set */
  assertf(strstr(buf, "Template file not found") != NULL);
  /* they must be the top-index copies ("???" is the formatter running out of
     arguments) */
  assertf(strstr(buf, "locally available projects") != NULL);
  assertf(strstr(buf, "/index.html\">") != NULL);
  assertf(strstr(buf, "???") == NULL);

#ifndef _WIN32
  /* #1329: a rebuild replaces the index instead of rewriting it, so a reader
     holding the old one open still reads it whole. Windows refuses to rename
     over an open target, so this probe is POSIX-only. */
  {
    FILE *const held = fopen(path, "rb");

    assertf(held != NULL);
    snprintf(path, sizeof(path), "%s/mocha/", topdir);
    assertf(structcheck(path) == 0);
    snprintf(path, sizeof(path), "%s/mocha/index.html", topdir);
    fp = fopen(path, "wb");
    assertf(fp != NULL);
    fclose(fp);
    assertf(hts_buildtopindex(opt, topdir, "") != 0);

    n = fread(buf, 1, sizeof(buf) - 1, held);
    fclose(held);
    buf[n] = '\0';
    assertf(n < sizeof(buf) - 1); /* a clipped read fakes an absence */
    assertf(strstr(buf, projUTF8) != NULL);
    assertf(strstr(buf, "mocha") == NULL);

    /* and the rebuild landed whole, footer included */
    snprintf(path, sizeof(path), "%s/index.html", topdir);
    fp = fopen(path, "rb");
    assertf(fp != NULL);
    n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[n] = '\0';
    assertf(strstr(buf, "mocha") != NULL);
    assertf(strstr(buf, "Thanks for using HTTrack") != NULL);

    snprintf(path, sizeof(path), "%s/mocha/index.html", topdir);
    unlink(path);
    snprintf(path, sizeof(path), "%s/mocha", topdir);
    rmdir(path);
  }
#endif
  /* the temporary is gone either way */
  snprintf(path, sizeof(path), "%s/index.tmp", topdir);
  assertf(!fexist(path));

  /* raw unlink/rmdir: UNLINK is utf-8 on Windows, these paths aren't */
  snprintf(path, sizeof(path), "%s/index.html", topdir);
  unlink(path);

#ifndef _WIN32
  /* a directory in the way fails the move (EISDIR, not the EEXIST fallback):
     the rebuild reports failure and leaves neither a temporary nor a stub */
  assertf(mkdir(path, 0755) == 0);
  assertf(hts_buildtopindex(opt, topdir, "") == 0);
  snprintf(path, sizeof(path), "%s/index.tmp", topdir);
  assertf(!fexist(path));
  snprintf(path, sizeof(path), "%s/index.html", topdir);
  assertf(rmdir(path) == 0);
#endif

  snprintf(path, sizeof(path), "%s/backblue.gif", topdir);
  unlink(path);
  snprintf(path, sizeof(path), "%s/fade.gif", topdir);
  unlink(path);
  snprintf(path, sizeof(path), "%s/%s/hts-cache/winprofile.ini", topdir,
           projName);
  unlink(path);
  snprintf(path, sizeof(path), "%s/%s/hts-cache", topdir, projName);
  rmdir(path);
  snprintf(path, sizeof(path), "%s/%s/index.html", topdir, projName);
  unlink(path);
  snprintf(path, sizeof(path), "%s/%s", topdir, projName);
  rmdir(path);
  rmdir(topdir);
  printf("topindex self-test OK\n");
  return 0;
}

/* filesave() reports a write failure only through its close, and its one crawl
   caller is unreachable: <doomed> is a save name that cannot take the bytes. */
static int st_filesave(httrackp *opt, int argc, char **argv) {
  static const char body[] = "DISKFULL-FILESAVE";
  const int len = (int) sizeof(body) - 1;
  int rc, err;

  if (argc < 2) {
    fprintf(stderr, "usage: -#test=filesave <doomed-save> <control-save>\n");
    return 1;
  }
  errno = 0;
  rc = filesave(opt, body, len, argv[0], "127.0.0.1", "/x.bin");
  err = errno;
  assertf(rc == -1);
  assertf(err == ENOSPC);
  /* control: the same call succeeds on a name that can take the bytes */
  assertf(filesave(opt, body, len, argv[1], "127.0.0.1", "/x.bin") == 0);
  printf("filesave: refused with ENOSPC, control saved\n");
  return 0;
}

// -#test=longpath <dir>: round-trip a >MAX_PATH (260) file through the file
// wrappers, exercising hts_pathToUCS2's \\?\ prefixing on Windows (#133).
static int st_longpath(httrackp *opt, int argc, char **argv) {
  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "longpath: needs a writable base dir\n");
    return 1;
  }
  char path[HTS_URLMAXSIZE * 2];
  size_t n = st_mkdeep(path, sizeof(path), argv[0], NULL, "longpath", NULL);

  if (n == 0) {
    return 1;
  }
  memcpybuff(path + n, "/leaf.bin", sizeof("/leaf.bin"));
  n += sizeof("/leaf.bin") - 1;
  assertf(st_utf16_units(path, n) > 260); /* the limit \\?\ lifts */

  static const char payload[] = "longpath-ok";
  FILE *fp = FOPEN(path, "wb");

  if (fp == NULL) {
    fprintf(stderr, "longpath: create failed (%u chars): %s\n", (unsigned) n,
            strerror(errno));
    return 1;
  }
  assertf(hts_fwrite_exact(payload, sizeof(payload), fp));
  fclose(fp);

  STRUCT_STAT st;

  assertf(STAT(path, &st) == 0);
  assertf((size_t) st.st_size == sizeof(payload));

  char buf[64];

  fp = FOPEN(path, "rb");
  assertf(fp != NULL);
  assertf(hts_fread_exact(buf, sizeof(payload), fp));
  fclose(fp);
  assertf(memcmp(buf, payload, sizeof(payload)) == 0);
  assertf(UNLINK(path) == 0);

  printf("longpath: round-tripped a %u-char path: OK\n", (unsigned) n);
  return 0;
}

// -#test=mirrorio <dir>: round-trip a file through a long AND non-ASCII path
// via the mirror I/O wrappers — fexist_utf8/fsize_utf8, FOPEN/RENAME/UNLINK,
// and the new hts_rmdir_utf8 (RMDIR) teardown (#133, #630).
static int st_mirrorio(httrackp *opt, int argc, char **argv) {
  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "mirrorio: needs a writable base dir\n");
    return 1;
  }
  char path[HTS_URLMAXSIZE * 2];
  size_t base = 0;
  size_t n = st_mkdeep(path, sizeof(path), argv[0],
                       "/" ST_NONASCII "-non-ascii-seg", "mirrorio", &base);

  if (n == 0) {
    return 1;
  }
  const size_t leafdir = n;

  memcpybuff(path + n, "/leaf.bin", sizeof("/leaf.bin"));
  n += sizeof("/leaf.bin") - 1;
  assertf(st_utf16_units(path, n) > 260); /* the limit \\?\ lifts */

  static const char payload[] = "mirrorio-ok";

  assertf(!fexist_utf8(path)); /* absent before creation, through the guard */
  FILE *fp = FOPEN(path, "wb");

  if (fp == NULL) {
    fprintf(stderr, "mirrorio: create failed (%u chars): %s\n", (unsigned) n,
            strerror(errno));
    return 1;
  }
  assertf(hts_fwrite_exact(payload, sizeof(payload), fp));
  fclose(fp);
  assertf(fexist_utf8(path));
  assertf(fsize_utf8(path) == (LLint) sizeof(payload));

  // Rename to a non-ASCII sibling to exercise RENAME on the long path.
  char path2[HTS_URLMAXSIZE * 2];

  memcpybuff(path2, path, leafdir);
  memcpybuff(path2 + leafdir, "/\xC3\xA9-leaf.bin",
             sizeof("/\xC3\xA9-leaf.bin"));
  assertf(RENAME(path, path2) == 0);
  assertf(!fexist_utf8(path));
  assertf(fexist_utf8(path2));
  assertf(fsize_utf8(path2) == (LLint) sizeof(payload));
  assertf(UNLINK(path2) == 0);
  assertf(!fexist_utf8(path2));

  // Tear the directory chain down through the UTF-8/long-path rmdir wrapper.
  path[leafdir] = '\0';
  while (strlen(path) > base) {
    char *const slash = strrchr(path, '/');

    if (RMDIR(path) != 0) {
      fprintf(stderr, "mirrorio: rmdir failed: %s\n", strerror(errno));
      return 1;
    }
    if (slash == NULL || (size_t) (slash - path) < base) {
      break;
    }
    *slash = '\0';
  }

  printf("mirrorio: round-tripped a %u-char non-ASCII path: OK\n",
         (unsigned) n);
  return 0;
}

static void ro_put(const char *path, const char *data) {
  FILE *const fp = FOPEN(path, "wb");

  assertf(fp != NULL);
  assertf(hts_fwrite_exact(data, strlen(data), fp));
  fclose(fp);
}

/* HTS_TRUE if path holds exactly data. */
static hts_boolean ro_is(const char *path, const char *data) {
  char buf[64];
  FILE *const fp = FOPEN(path, "rb");
  size_t n;

  if (fp == NULL)
    return HTS_FALSE;
  n = fread(buf, 1, sizeof(buf), fp);
  fclose(fp);
  return n == strlen(data) && memcmp(buf, data, n) == 0 ? HTS_TRUE : HTS_FALSE;
}

// -#test=renameover <dir>: hts_rename_over() must replace an existing dst and
// never lose one it did not replace (#779, #790). Which half is live depends on
// what rename() does to an existing target, so probe that and name the regime.
static int st_renameover(httrackp *opt, int argc, char **argv) {
  if (argc < 1) {
    fprintf(stderr, "renameover: needs a writable base dir\n");
    return 1;
  }
  char src[HTS_URLMAXSIZE * 2], dst[HTS_URLMAXSIZE * 2];
  int err = 0;

  fconcat(src, sizeof(src), argv[0], "renameover-src.bin");
  fconcat(dst, sizeof(dst), argv[0], "renameover-dst.bin");

  (void) UNLINK(src);
  (void) UNLINK(dst);
  ro_put(src, "probe");
  ro_put(dst, "probe");

  const int probe = RENAME(src, dst) == 0 ? 0 : errno;
  /* Only a target in the way is something the unlink can clear. */
  const hts_boolean replaceable = probe == 0 || probe == EEXIST;

  printf("renameover: regime %s\n",
         probe == 0 ? "clobber" : (probe == EEXIST ? "fallback" : "refused"));

  (void) UNLINK(src);
  (void) UNLINK(dst);
  ro_put(src, "new");
  ro_put(dst, "old");
  if (replaceable) {
    /* An existing dst must still be replaced: the unlink is for this. */
    if (!hts_rename_over(opt, src, dst)) {
      fprintf(stderr, "renameover: replacing an existing dst failed: %s\n",
              strerror(errno));
      err++;
    } else if (!ro_is(dst, "new") || fexist_utf8(src)) {
      fprintf(stderr, "renameover: dst was not replaced by src\n");
      err++;
    }
  } else {
    /* A failure the unlink cannot fix must leave dst as it was. */
    if (hts_rename_over(opt, src, dst)) {
      fprintf(stderr, "renameover: an unfixable failure reported success\n");
      err++;
    }
    if (!ro_is(dst, "old")) {
      fprintf(stderr, "renameover: an unfixable failure destroyed dst\n");
      err++;
    }
  }

  /* A directory in the way is not something the caller asked to replace: it
     must be refused, never parked aside and orphaned. */
  (void) UNLINK(dst);
  ro_put(src, "new");
  if (MKDIR(dst) == 0) {
    char parked[sizeof(dst) + 16];

    snprintf(parked, sizeof(parked), "%s.hts-old0", dst);
    if (hts_rename_over(opt, src, dst)) {
      fprintf(stderr, "renameover: a directory at dst reported success\n");
      err++;
    }
    if (!ro_is(src, "new")) {
      fprintf(stderr, "renameover: a directory at dst consumed src\n");
      err++;
    }
    /* RMDIR only succeeds on a directory that is there, so it doubles as the
       probe: the parked name must not exist at all. */
    if (RMDIR(parked) == 0 || fexist_utf8(parked)) {
      fprintf(stderr, "renameover: a directory at dst was parked aside\n");
      err++;
    }
    (void) RMDIR(dst);
  }
  (void) UNLINK(src);

  /* A missing src must leave dst alone and report failure. */
  (void) UNLINK(src);
  ro_put(dst, "keep");
  if (hts_rename_over(opt, src, dst)) {
    fprintf(stderr, "renameover: a missing src reported success\n");
    err++;
  }
  if (!ro_is(dst, "keep")) {
    fprintf(stderr, "renameover: a missing src destroyed dst\n");
    err++;
  }

  /* Same, with dst absent too: nothing to lose, still a failure. */
  (void) UNLINK(dst);
  if (hts_rename_over(opt, src, dst)) {
    fprintf(stderr, "renameover: a missing src and dst reported success\n");
    err++;
  }

  /* The aside fallback, driven directly: a clobbering rename() never reaches
     it. Skipped in the refused regime, where no rename at all succeeds. */
  if (replaceable) {
    char aside[sizeof(dst) + 16], keep[sizeof(dst) + 16];

    snprintf(aside, sizeof(aside), "%s.hts-old0", dst);
    snprintf(keep, sizeof(keep), "%s.hts-old1", dst);
    (void) UNLINK(aside);
    (void) UNLINK(keep);
    ro_put(src, "new");
    ro_put(dst, "old");
    if (!hts_rename_over_aside_selftest(opt, src, dst)) {
      fprintf(stderr, "renameover: the aside fallback failed: %s\n",
              strerror(errno));
      err++;
    } else if (!ro_is(dst, "new") || fexist_utf8(src) || fexist_utf8(aside)) {
      fprintf(stderr, "renameover: the aside fallback did not replace dst\n");
      err++;
    }

    /* #790: the retry fails (no src). The old content must survive, back at dst
       or, when the move back fails too, under the parked name it is logged as.
       Name the outcome so a leg cannot pass having tested the other one. */
    (void) UNLINK(src);
    ro_put(dst, "old");
    if (hts_rename_over_aside_selftest(opt, src, dst)) {
      fprintf(stderr, "renameover: a failed aside retry reported success\n");
      err++;
    }
    if (ro_is(dst, "old") && !fexist_utf8(aside)) {
      printf("renameover: restore back\n");
    } else if (ro_is(aside, "old") && !fexist_utf8(dst)) {
      printf("renameover: restore parked\n");
      (void) UNLINK(aside);
      ro_put(dst, "old");
    } else {
      fprintf(stderr, "renameover: a failed aside retry lost the old copy\n");
      err++;
    }

    /* An unrelated file already sitting on the aside name must survive. */
    ro_put(src, "new");
    ro_put(aside, "mine");
    if (!hts_rename_over_aside_selftest(opt, src, dst)) {
      fprintf(stderr, "renameover: a taken aside name failed the move: %s\n",
              strerror(errno));
      err++;
    } else if (!ro_is(dst, "new") || !ro_is(aside, "mine") ||
               fexist_utf8(keep)) {
      fprintf(stderr, "renameover: a taken aside name was not skipped\n");
      err++;
    }
    (void) UNLINK(aside);
    (void) UNLINK(keep);

    /* A directory there reads as free to the probe, so the park must skip it
       on the refusal rather than give up. */
    ro_put(src, "new");
    ro_put(dst, "old");
    if (MKDIR(aside) == 0) {
      if (!hts_rename_over_aside_selftest(opt, src, dst)) {
        fprintf(stderr,
                "renameover: a directory on the aside name blocked the "
                "move: %s\n",
                strerror(errno));
        err++;
      } else if (!ro_is(dst, "new") || fexist_utf8(keep)) {
        fprintf(stderr, "renameover: a directory on the aside name was not "
                        "skipped\n");
        err++;
      }
      (void) RMDIR(aside);
    }
    (void) UNLINK(keep);
  }

  (void) UNLINK(src);
  (void) UNLINK(dst);
  printf("renameover: %s\n", err ? "FAIL" : "OK");
  return err;
}

// -#test=refetchbackup <dir>: the #77 re-fetch backup must build its temporary
// inside the ~hts-tmp directory, which no save name can spell (#774), and must
// never leave the resource without a copy (#775).
static int st_refetchbackup(httrackp *opt, int argc, char **argv) {
  lien_back *back;
  char want[HTS_URLMAXSIZE * 2 + 32];
  int err = 0;

  if (argc < 1) {
    fprintf(stderr, "refetchbackup: needs a writable base dir\n");
    return 1;
  }
  back = calloct(1, sizeof(lien_back));
  if (back == NULL) {
    fprintf(stderr, "refetchbackup: out of memory\n");
    return 1;
  }
  /* explicit separator: fconcat() joins without one, which would put the
     temporary in the parent of the directory under test */
  snprintf(back->url_sav, sizeof(back->url_sav), "%s/refetch.bin", argv[0]);
  snprintf(want, sizeof(want), "%s/~hts-tmp/refetch.bin.bak", argv[0]);

  /* #774: pin the name, so moving the temporary back into the mirror namespace
     cannot pass unnoticed. */
  ro_put(back->url_sav, "old");
  back_refetch_backup(opt, back);
  if (back->tmpfile == NULL || fexist_utf8(back->url_sav)) {
    fprintf(stderr, "refetchbackup: the previous copy was not moved aside\n");
    err++;
  } else if (strcmp(back->tmpfile, want) != 0) {
    fprintf(stderr, "refetchbackup: temporary is %s, want %s\n", back->tmpfile,
            want);
    err++;
  }
  ro_put(back->url_sav, "new"); /* what filecreate() + the transfer produce */
  back_finalize_backup(opt, back, HTS_TRUE);
  if (!ro_is(back->url_sav, "new")) {
    fprintf(stderr, "refetchbackup: the committed copy is not the new one\n");
    err++;
  }

  /* #758: only a killed run can leave something there, and it must be replaced
     rather than disable the backup for good. */
  if (structcheck(want) != 0) {
    fprintf(stderr, "refetchbackup: cannot create %s\n", want);
    freet(back);
    return 1;
  }
  ro_put(want, "leftover");
  back_refetch_backup(opt, back);
  if (back->tmpfile == NULL || !ro_is(want, "new")) {
    fprintf(stderr, "refetchbackup: a leftover temporary blocked the backup\n");
    err++;
  }

  /* #775: filecreate() failed, so there is nothing to commit to. Saying so is
     load-bearing: the caller must not cache this response against the old
     body, or the next --update gets a 304 pinning it. */
  (void) UNLINK(back->url_sav);
  if (back_finalize_backup(opt, back, HTS_TRUE)) {
    fprintf(stderr, "refetchbackup: a commit that restored reported success\n");
    err++;
  }
  if (!ro_is(back->url_sav, "new")) {
    fprintf(stderr, "refetchbackup: a commit with no new copy lost both\n");
    err++;
  }

  /* An aborted transfer restores, as before. The resume reference goes with
     the partial it described, because a kept one would make the next run ask
     past the restored copy's end (#1595). */
  {
    String saved = STRING_EMPTY;

    StringCopy(saved, StringBuff(opt->path_log));
    /* explicit separator, as above: fconcat() joins without one, which would
       put the reference in a sibling of the directory under test */
    StringCopy(opt->path_log, argv[0]);
    StringCat(opt->path_log, "/");
    strcpybuff(back->url_adr, "127.0.0.1");
    strcpybuff(back->url_fil, "/refetch.bin");
    back_refetch_backup(opt, back);
    ro_put(back->url_sav, "partial");
    /* a real mirror already has hts-cache/, and the writer only mkdirs ref/ */
    (void) structcheck(
        url_savename_refname_fullpath(opt, back->url_adr, back->url_fil));
    if (back_serialize_ref(opt, back) != 0 ||
        !fexist_utf8(
            url_savename_refname_fullpath(opt, back->url_adr, back->url_fil))) {
      fprintf(stderr, "refetchbackup: could not seed a resume reference\n");
      err++;
    }
    back_finalize_backup(opt, back, HTS_FALSE);
    if (!ro_is(back->url_sav, "new")) {
      fprintf(stderr, "refetchbackup: an aborted re-fetch kept the partial\n");
      err++;
    }
    if (fexist_utf8(
            url_savename_refname_fullpath(opt, back->url_adr, back->url_fil))) {
      fprintf(stderr, "refetchbackup: the restore kept the partial's ref\n");
      err++;
    }
    (void) RMDIR(fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                         StringBuff(opt->path_log), CACHE_REFNAME));
    (void) RMDIR(fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                         StringBuff(opt->path_log), "hts-cache"));
    StringCopy(opt->path_log, StringBuff(saved));
    StringFree(saved);
  }

  (void) UNLINK(back->url_sav);
  freet(back);
  printf("refetchbackup: %s\n", err ? "FAIL" : "OK");
  return err;
}

// -#test=spoolname <dir>: a frozen backlog slot must spool inside ~hts-tmp, not
// beside the mirrored file where a site serving <path>.tmp collides (#859).
static int st_spoolname(httrackp *opt, int argc, char **argv) {
  char BIGSTK got[HTS_URLMAXSIZE * 2 + 32];
  char BIGSTK want[HTS_URLMAXSIZE * 2 + 32];
  char BIGSTK save[HTS_URLMAXSIZE * 2];
  int err = 0;

  if (argc < 1) {
    fprintf(stderr, "spoolname: needs a writable base dir\n");
    return 1;
  }

  /* named: the spool lands in the save name's own ~hts-tmp, which no URL can
     spell since url_savename() maps '~' to '_' */
  snprintf(save, sizeof(save), "%s/sub/page.html", argv[0]);
  snprintf(want, sizeof(want), "%s/sub/~hts-tmp/page.html.tmp", argv[0]);
  opt->getmode = 1;
  if (!back_spoolname(opt, save, got, sizeof(got))) {
    fprintf(stderr, "spoolname: naming failed for %s\n", save);
    err++;
  } else if (strcmp(got, want) != 0) {
    fprintf(stderr, "spoolname: got %s, want %s\n", got, want);
    err++;
  }

  /* pin the pre-#859 name as forbidden too: a site serving sub/page.html.tmp
     was mirrored straight onto it */
  snprintf(want, sizeof(want), "%s.tmp", save);
  if (strcmp(got, want) == 0) {
    fprintf(stderr, "spoolname: still spooling into the mirror namespace\n");
    err++;
  }

  /* -p0 keeps no save name, so the spool counts inside path_html's ~hts-tmp */
  {
    char BIGSTK base[HTS_URLMAXSIZE * 2];

    snprintf(base, sizeof(base), "%s/", argv[0]);
    StringCopy(opt->path_html_utf8, base);
    opt->getmode = 0;
    opt->state.tmpnameid = 7;
    snprintf(want, sizeof(want), "%s/~hts-tmp/tmpfile7.tmp", argv[0]);
    if (!back_spoolname(opt, "", got, sizeof(got))) {
      fprintf(stderr, "spoolname: naming failed under -p0\n");
      err++;
    } else if (strcmp(got, want) != 0) {
      fprintf(stderr, "spoolname: -p0 got %s, want %s\n", got, want);
      err++;
    }
    if (opt->state.tmpnameid != 8) {
      fprintf(stderr, "spoolname: -p0 did not consume a tmpnameid\n");
      err++;
    }
  }

  /* with no -O, path_html_utf8 is empty and the spool must stay relative to
     the working directory; a separator of our own would put it in / */
  StringCopy(opt->path_html_utf8, "");
  opt->getmode = 0;
  opt->state.tmpnameid = 0;
  if (!back_spoolname(opt, "", got, sizeof(got))) {
    fprintf(stderr, "spoolname: naming failed with no output directory\n");
    err++;
  } else if (strcmp(got, "~hts-tmp/tmpfile0.tmp") != 0) {
    fprintf(stderr, "spoolname: no -O gave %s, want ~hts-tmp/tmpfile0.tmp\n",
            got);
    err++;
  }

  /* too long must empty dest, not hand back a truncated name landing
     somewhere real */
  opt->getmode = 1;
  if (back_spoolname(opt, save, got, 8) || got[0] != '\0') {
    fprintf(stderr, "spoolname: an overlong name was not rejected\n");
    err++;
  }

  printf("spoolname: %s\n", err ? "FAIL" : "OK");
  return err;
}

// -#test=direnum <dir>: enumerate a long+non-ASCII directory via the
// opendir/readdir wrappers; children must round-trip as UTF-8 (#133,#630).
static int st_direnum(httrackp *opt, int argc, char **argv) {
  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "direnum: needs a writable base dir\n");
    return 1;
  }
  char path[HTS_URLMAXSIZE * 2];
  size_t base = 0;
  const size_t dirlen =
      st_mkdeep(path, sizeof(path), argv[0], "/" ST_NONASCII "-non-ascii-seg",
                "direnum", &base);

  if (dirlen == 0) {
    return 1;
  }

  // Two non-ASCII leaf files to read back by name.
  static const char *const leaves[] = {"/\xC3\xA9-un.bin",
                                       "/\xE4\xB8\xAD-deux.bin"};
  for (size_t i = 0; i < 2; i++) {
    memcpybuff(path + dirlen, leaves[i], strlen(leaves[i]) + 1);
    FILE *fp = FOPEN(path, "wb");

    if (fp == NULL) {
      fprintf(stderr, "direnum: create failed: %s\n", strerror(errno));
      return 1;
    }
    fclose(fp);
  }
  path[dirlen] = '\0';

  int found = 0;
  DIR *d = opendir(path);

  if (d == NULL) {
    fprintf(stderr, "direnum: opendir failed: %s\n", strerror(errno));
    return 1;
  }
  struct dirent *e;

  while ((e = readdir(d)) != NULL) {
    for (size_t i = 0; i < 2; i++) {
      if (strcmp(e->d_name, leaves[i] + 1) == 0) { /* +1: drop the '/' */
        found |= 1 << i;
      }
    }
  }
  closedir(d);
  if (found != 0x3) {
    fprintf(stderr, "direnum: missing entries (found mask 0x%x)\n", found);
    return 1;
  }

  // Teardown: leaves then the directory chain, via the long-path wrappers.
  for (size_t i = 0; i < 2; i++) {
    memcpybuff(path + dirlen, leaves[i], strlen(leaves[i]) + 1);
    assertf(UNLINK(path) == 0);
  }
  path[dirlen] = '\0';
  while (strlen(path) > base) {
    char *const slash = strrchr(path, '/');

    if (RMDIR(path) != 0) {
      fprintf(stderr, "direnum: rmdir failed: %s\n", strerror(errno));
      return 1;
    }
    if (slash == NULL || (size_t) (slash - path) < base) {
      break;
    }
    *slash = '\0';
  }

  printf("direnum: enumerated 2 non-ASCII leaves under a %u-char dir: OK\n",
         (unsigned) dirlen);
  return 0;
}

// -#test=ioexact <dir>: the hts_fread_exact/hts_fwrite_exact contract - a
// partial transfer is a failure, a zero-length one is not, and a short read
// leaves the bytes it could not fill alone.
static int st_ioexact(httrackp *opt, int argc, char **argv) {
  const size_t payload_size = 70000; /* past any stdio buffer */
  const unsigned char poison = 0xa5;
  char path[HTS_URLMAXSIZE * 2];
  char *payload, *readback;
  FILE *fp;
  int plen;
  size_t i;

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "ioexact: needs a writable dir\n");
    return 1;
  }
  plen = snprintf(path, sizeof(path), "%s/ioexact.bin", argv[0]);
  assertf(plen > 0 && (size_t) plen < sizeof(path));
  payload = malloct(payload_size);
  readback = malloct(payload_size + 1);
  for (i = 0; i < payload_size; i++)
    payload[i] = (char) (i * 7 + (i >> 8));

  /* Round trip: a helper that swapped fwrite's size and nmemb would report a
     count of 1 here and die. */
  fp = FOPEN(path, "wb");
  assertf(fp != NULL);
  assertf(hts_fwrite_exact(payload, payload_size, fp) == HTS_TRUE);
  assertf(fclose(fp) == 0);
  memset(readback, poison, payload_size + 1);
  fp = FOPEN(path, "rb");
  assertf(fp != NULL);
  assertf(hts_fread_exact(readback, payload_size, fp) == HTS_TRUE);
  assertf(memcmp(readback, payload, payload_size) == 0);
  assertf((unsigned char) readback[payload_size] == poison);
  assertf(hts_fread_exact(readback, 1, fp) == HTS_FALSE); /* at EOF */
  assertf(hts_fread_exact(readback, 0, fp) == HTS_TRUE);
  assertf(fclose(fp) == 0);

  /* Short read: four of the eight bytes asked for, so a failure, and the tail
     of the destination keeps its poison. */
  fp = FOPEN(path, "wb");
  assertf(fp != NULL);
  assertf(hts_fwrite_exact(payload, 4, fp) == HTS_TRUE);
  assertf(fclose(fp) == 0);
  memset(readback, poison, 16);
  fp = FOPEN(path, "rb");
  assertf(fp != NULL);
  assertf(hts_fread_exact(readback, 8, fp) == HTS_FALSE);
  assertf(memcmp(readback, payload, 4) == 0);
  for (i = 4; i < 16; i++)
    assertf((unsigned char) readback[i] == poison);
  assertf(fclose(fp) == 0);

  /* Wrong direction: an I/O error is a failure, not a silent no-op. */
  fp = FOPEN(path, "wb");
  assertf(fp != NULL);
  assertf(hts_fread_exact(readback, 4, fp) == HTS_FALSE);
  assertf(fclose(fp) == 0);
  fp = FOPEN(path, "rb");
  assertf(fp != NULL);
  assertf(hts_fwrite_exact(payload, 4, fp) == HTS_FALSE);
  assertf(hts_fwrite_exact(payload, 0, fp) == HTS_TRUE);
  assertf(fclose(fp) == 0);

  /* Short WRITE: the cases above only ever get 0 back, so a helper accepting
     any non-zero count survives them. fmemopen's buffer is smaller than the
     payload, which yields a partial count instead. */
#if !defined(_WIN32)
  {
    char small[8];
    /* Short by many, then by exactly one: only the second catches a helper
       that accepts an off-by-one count. */
    const size_t over[2] = {sizeof(small) * 2, sizeof(small) + 1};

    for (i = 0; i < 2; i++) {
      /* Unbuffered, or stdio takes the whole payload and reports the overflow
         only at flush, leaving fwrite's own count complete. */
      FILE *mem = fmemopen(small, sizeof(small), "wb");

      if (mem != NULL && setvbuf(mem, NULL, _IONBF, 0) == 0)
        assertf(hts_fwrite_exact(payload, over[i], mem) == HTS_FALSE);
      if (mem != NULL)
        (void) fclose(mem);
    }
  }
#endif

  /* A zero size must leave dest alone rather than write a terminator. */
  memset(readback, poison, 4);
  fp = FOPEN(path, "rb");
  assertf(fp != NULL);
  assertf(hts_fread_exact(readback, 0, fp) == HTS_TRUE);
  for (i = 0; i < 4; i++)
    assertf((unsigned char) readback[i] == poison);
  assertf(hts_fread_exact(NULL, 0, fp) == HTS_TRUE);
  assertf(fclose(fp) == 0);

  assertf(UNLINK(path) == 0);
  freet(payload);
  freet(readback);
  printf("ioexact: a partial transfer is a failure: OK\n");
  return 0;
}

/* ------------------------------------------------------------ */
/* Registry: this module's tests, in the order -#test lists them. */
/* ------------------------------------------------------------ */

const struct selftest_entry selftests_io[] = {
    {"fsize", "<dir>", "file size past the 2GB signed-32-bit wrap", st_fsize},
    {"findsize", "<dir>",
     "directory-enumeration file size past the 2GB signed-32-bit wrap",
     st_findsize},
    {"growsize", "", "buffer capacity for a 64-bit file size (no int wrap)",
     st_growsize},
    {"addlink", "", "htsAddLink codebase walk over an empty current path",
     st_addlink},
    {"makeindex", "[dir]", "hts_finish_makeindex footer/refresh self-test",
     st_makeindex},
    {"filesave", "<doomed-save> <control-save>",
     "filesave() reports a failing close, errno intact", st_filesave},
    {"topindex", "[dir]",
     "hts_buildtopindex charset handling of a non-ASCII project dir",
     st_topindex},
    {"datadir", "<dir>",
     "data directory resolution: compiled-in path, then the executable's tree",
     st_datadir},
    {"pathbin", "", "print the data directory this run resolved at startup",
     st_pathbin},
    {"instpaths", "", "print the compiled-in install paths (POSIX only)",
     st_instpaths},
    {"ioexact", "<dir>",
     "hts_fread_exact/hts_fwrite_exact reject a partial transfer", st_ioexact},
    {"longpath", "<dir>",
     "round-trip a >MAX_PATH file through the _w* wrappers (\\\\?\\ on "
     "Windows)",
     st_longpath},
    {"mirrorio", "<dir>",
     "round-trip a long+non-ASCII path through the mirror I/O wrappers",
     st_mirrorio},
    {"renameover", "<dir>",
     "hts_rename_over(): replace dst, but never delete a dst it did not "
     "replace",
     st_renameover},
    {"refetchbackup", "<dir>",
     "the re-fetch backup always leaves a copy, and stays out of the mirror",
     st_refetchbackup},
    {"spoolname", "<dir>",
     "a frozen backlog slot spools outside the mirror namespace", st_spoolname},
    {"direnum", "<dir>",
     "enumerate a long+non-ASCII directory through opendir/readdir",
     st_direnum},
    {NULL, NULL, NULL, NULL},
};
