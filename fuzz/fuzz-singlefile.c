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

static void sf_init(void) {
  static const char png[] = "\x89PNG\r\n\x1a\n";
  static const char big[4096] = "\x89PNG";
  const char *tmp = getenv("TMPDIR");
  char path[700];

  hts_init();
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
  sf_text("s.css", "@import url(sub/b.css" SINGLEFILE_MARK ");\n"
                   "div{background:url(a.png" SINGLEFILE_MARK ")}\n"
                   "p{background:url(big.png" SINGLEFILE_MARK ")}\n");
  sf_text("sub/b.css", "p{background:url(../a.png" SINGLEFILE_MARK ")}\n");
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static int inited = 0;

  String out = STRING_EMPTY;
  httrackp *opt;
  /* Exact-length, unterminated: the rewriter is span-based, so ASan bounds a
     read past html_len instead of it landing on a terminator. */
  char *html = malloct(size != 0 ? size : 1);

  if (!inited) {
    sf_init();
    inited = 1;
  }
  memcpy(html, data, size);

  opt = hts_create_opt();
  opt->log = opt->errlog = NULL;
  opt->single_file_max_size = FUZZ_SF_CAP;

  StringClear(out);
  (void) singlefile_rewrite_html(opt, sf_root, sf_page, html, size,
                                 SINGLEFILE_MAX_PAGE_SIZE, &out);

  StringFree(out);
  freet(html);
  hts_free_opt(opt);
  return 0;
}
