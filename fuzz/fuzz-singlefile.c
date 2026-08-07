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

/* Fuzz the --single-file rewriter (htssinglefile.c): hostile HTML walked
   through the tag, CSS url()/@import and srcset parsers, then re-serialized.
   The resolver is aimed at a private temp tree, so the inlining half (MIME
   guess, base64, nested stylesheet) is reached and nothing else on disk is. */
#include "fuzz.h"

#include "httrack-library.h"
#include "htssinglefile.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

/* Between a.png and big.png, so one input reaches both the inline path and the
   over-cap fallback. */
#define FUZZ_SF_CAP 64

static char sf_root[512];
static char sf_page[600];

/* The asset tree, in removal order: the subdirectory comes after its file. */
static const char *const sf_files[] = {"a.png",     "big.png", "j.js", "s.css",
                                       "sub/b.css", "sub",     NULL};

static void sf_cleanup(void) {
  char path[700];
  int i;

  for (i = 0; sf_files[i] != NULL; i++) {
    snprintf(path, sizeof(path), "%s/%s", sf_root, sf_files[i]);
    (void) remove(path);
  }
  (void) remove(sf_root);
}

/* One httrackp for the whole run: the mark secret lives on it, so a fresh one
   per input would make every mark in the corpus unrecognisable. */
static httrackp *sf_opt = NULL;

/* A missing asset would silently reduce the target to its parser half. */
static void sf_write(const char *name, const char *data, size_t len) {
  char path[700];
  FILE *fp;

  snprintf(path, sizeof(path), "%s/%s", sf_root, name);
  fp = fopen(path, "wb");
  if (fp == NULL || fwrite(data, 1, len, fp) != len)
    abort();
  fclose(fp);
}

static void sf_text(const char *name, const char *data) {
  sf_write(name, data, strlen(data));
}

/* Append ref plus a real mark for it. The secret is drawn per httrackp, so a
   fixture cannot spell one; every mark the target sees is built here. */
static void sf_marked(String *out, const char *ref, char cls) {
  char mark[SINGLEFILE_MARK_MAX];

  StringCat(*out, ref);
  StringCat(*out,
            singlefile_mark(sf_opt, mark, sizeof(mark), cls, strlen(ref)));
}

/* \001<ref>\002 in an input becomes <ref> plus its mark, which is the only
   way a corpus file can reach the mark parser at all. */
static void sf_expand_input(const char *in, size_t len, String *out) {
  size_t i, start = 0;

  StringClear(*out);
  for (i = 0; i < len; i++) {
    if (in[i] == '\001') {
      start = StringLength(*out);
    } else if (in[i] == '\002') {
      char mark[SINGLEFILE_MARK_MAX];

      StringCat(*out, singlefile_mark(sf_opt, mark, sizeof(mark),
                                      SINGLEFILE_CLASS_ANY,
                                      StringLength(*out) - start));
    } else {
      StringAddchar(*out, in[i]);
    }
  }
}

static void sf_css(const char *name, const char *pre, const char *ref,
                   const char *post) {
  String body = STRING_EMPTY;

  StringCopy(body, pre);
  sf_marked(&body, ref, SINGLEFILE_CLASS_ANY);
  StringCat(body, post);
  sf_write(name, StringBuff(body), StringLength(body));
  StringFree(body);
}

static void sf_init(void) {
  static const char png[] = "\x89PNG\r\n\x1a\n";
  static const char big[4096] = "\x89PNG";
  const char *tmp = getenv("TMPDIR");
  char path[700];

  hts_init();
  sf_opt = hts_create_opt();
  sf_opt->log = sf_opt->errlog = NULL;
  sf_opt->single_file_max_size = FUZZ_SF_CAP;
  snprintf(sf_root, sizeof(sf_root), "%s/httrack-fuzz-sf-XXXXXX",
           tmp != NULL && tmp[0] != '\0' ? tmp : "/tmp");
  if (mkdtemp(sf_root) == NULL)
    abort();
  atexit(sf_cleanup);
  snprintf(sf_page, sizeof(sf_page), "%s/page.html", sf_root);
  snprintf(path, sizeof(path), "%s/sub", sf_root);
  if (mkdir(path, 0700) != 0)
    abort();
  sf_write("a.png", png, sizeof(png) - 1);
  sf_write("big.png", big, sizeof(big));
  sf_text("j.js", "var x=1;\n");
  /* Marked, so an inlined stylesheet recurses into its own marks and its
     un-inlinable reference is rebased; unmarked assets leave the target as a
     bare scan that reaches nothing. */
  {
    String css = STRING_EMPTY;

    StringCopy(css, "@import url(");
    sf_marked(&css, "sub/b.css", SINGLEFILE_CLASS_CSS);
    StringCat(css, ");\ndiv{background:url(");
    sf_marked(&css, "a.png", SINGLEFILE_CLASS_ANY);
    StringCat(css, ")}\np{background:url(");
    sf_marked(&css, "big.png", SINGLEFILE_CLASS_ANY);
    StringCat(css, ")}\n");
    sf_write("s.css", StringBuff(css), StringLength(css));
    StringFree(css);
  }
  sf_css("sub/b.css", "p{background:url(", "../a.png", ")}\n");
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static int inited = 0;

  String out = STRING_EMPTY;
  String in = STRING_EMPTY;
  char *html;
  size_t html_len;

  if (!inited) {
    sf_init();
    inited = 1;
  }
  sf_expand_input((const char *) data, size, &in);
  html_len = StringLength(in);
  /* Exact-length, unterminated: the rewriter is span-based, so ASan bounds a
     read past html_len instead of it landing on a terminator. */
  html = malloct(html_len != 0 ? html_len : 1);
  memcpy(html, StringBuff(in), html_len);
  StringFree(in);

  StringClear(out);
  (void) singlefile_rewrite_html(sf_opt, sf_root, sf_page, html, html_len,
                                 SINGLEFILE_MAX_PAGE_SIZE, &out);

  StringFree(out);
  freet(html);
  return 0;
}
