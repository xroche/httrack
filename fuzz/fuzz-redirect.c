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

Please visit our Website: http://www.httrack.com
*/

/* Fuzz hts_mirror_check_moved(): a redirect's Location, or a 412/416 over a
   partial file and its resume reference, against the crawl state fuzz-htsparse
   builds. The str/stre wiring mirrors htscore.c's call site. */
#include "fuzz.h"

#include <unistd.h>

#include "httrack-library.h"
#include "htscore.h"
#include "htsback.h"
#include "htshash.h"
#include "htsname.h"
#include "htsrobots.h"
#include "htsparse.h"
#include "htsmirror.h"
#include "htsmodules.h"
#include "coucal.h"

/* first byte, low 3 bits: the status */
static const int statuses[] = {301, 302, 303, 307, 308, 412, 416, 404};

/* first byte, bits 3-4: the page that answered */
static const char *const pages[] = {"/", "/dir/index.html", "/a/b.php?x=1&y=2",
                                    "/Dir/Page.HTML"};

#define OPT_PAGE_SHIFT 3
#define OPT_PAGE_MASK 0x03
#define OPT_MIMEHTML 0x20
#define OPT_PARTIAL 0x40 /* a partial file sits at the page's save name */
#define OPT_REF 0x80     /* the bytes after the Location go to its .ref file */

/* second byte */
#define OPT_URLHACK 0x01
#define OPT_ERRPAGE 0x02
#define OPT_OTHER_HOST 0x04 /* the page is on www.example.com:8080 */

static char redir_dir[256];
static char redir_sav[sizeof(redir_dir) + 32];

static void redir_cleanup(void) {
  char path[sizeof(redir_dir) + 32];

  (void) unlink(redir_sav);
  snprintf(path, sizeof(path), "%s/" CACHE_REFNAME, redir_dir);
  (void) rmdir(path);
  snprintf(path, sizeof(path), "%s/hts-cache", redir_dir);
  (void) rmdir(path);
  (void) rmdir(redir_dir);
}

static void put_file(const char *path, hts_span bytes) {
  FILE *const fp = FOPEN(path, "wb");

  if (fp == NULL)
    abort();
  if (bytes.len != 0 && fwrite(bytes.p, 1, bytes.len, fp) != bytes.len)
    abort();
  fclose(fp);
}

/* the stub page quotes each attribute; an escape that let a '"' through would
   change this count */
static void check_stub(const htsblk *r) {
  size_t quotes = 0;
  const char *s;

  if (r->adr == NULL || strcmp(r->contenttype, "text/html") != 0)
    return;
  if ((size_t) r->size != strlen(r->adr))
    abort();
  for (s = r->adr; (s = strchr(s, '"')) != NULL; s++)
    quotes++;
  if (quotes != 10)
    abort();
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static int inited = 0;

  unsigned bits, bits2;
  const char *host, *page;
  const uint8_t *eol, *rest;
  size_t loclen, restlen;
  httrackp *opt;
  cache_back cache;
  hash_struct hash;
  robots_wizard robots;
  struct_back *sback;
  char **filters = NULL;
  int filptr = 0;
  char *ref = NULL;

  htsblk r;
  htsmoduleStruct str;
  htsmoduleStructExtended stre;
  int ptr, error = 0, store_errpage = 0;
  int makeindex_done = 0, makeindex_links = 0;
  FILE *makeindex_fp = NULL;
  char makeindex_firstlink[HTS_URLMAXSIZE * 2] = "";
  LLint stat_fragment = 0, makestat_total = 0;
  int makestat_lnk = 0;
  char base[HTS_URLMAXSIZE * 2] = "";
  char codebase[HTS_URLMAXSIZE * 2] = "";
  char err_msg[1024] = "";
  char location[HTS_LOCATION_SIZE];

  if (size < 2)
    return 0;
  bits = data[0];
  bits2 = data[1];
  data += 2, size -= 2;

  /* treathead() hands over one header line, clipped at the first NUL */
  eol = memchr(data, '\n', size);
  loclen = eol != NULL ? (size_t) (eol - data) : size;
  rest = eol != NULL ? eol + 1 : data + size;
  restlen = (size_t) (data + size - rest);
  if (loclen != 0 && data[loclen - 1] == '\r')
    loclen--;
  if (memchr(data, '\0', loclen) != NULL)
    loclen = (size_t) ((const uint8_t *) memchr(data, '\0', loclen) - data);
  /* the gates treathead() applies before anything reads the value */
  if (loclen >= sizeof(location))
    loclen = 0;
  memcpy(location, data, loclen);
  location[loclen] = '\0';
  if (!hts_location_is_safe(location))
    location[0] = '\0';

  if (!inited) {
    hts_init();
    if (!fuzz_tmpdir(HTS_BUF(redir_dir), "fuzz-redirect", redir_cleanup))
      abort();
    snprintf(redir_sav, sizeof(redir_sav), "%s/page.html", redir_dir);
    inited = 1;
  }

  opt = hts_create_opt();
  opt->log = opt->errlog = NULL;
  opt->mimehtml = (bits & OPT_MIMEHTML) != 0;
  opt->urlhack = (bits2 & OPT_URLHACK) ? HTS_TRUE : HTS_FALSE;
  opt->errpage = (bits2 & OPT_ERRPAGE) ? HTS_TRUE : HTS_FALSE;
  StringCopy(opt->path_html, redir_dir);
  StringCat(opt->path_html, "/");
  StringCopy(opt->path_html_utf8, StringBuff(opt->path_html));
  StringCopy(opt->path_log, StringBuff(opt->path_html));

  memset(&cache, 0, sizeof(cache));
  cache.type = 0; /* no on-disk cache */
  cache.hashtable = coucal_new(0);
  cache.cached_tests = coucal_new(0);
  coucal_value_is_malloc(cache.cached_tests, 1);

  memset(&robots, 0, sizeof(robots));
  opt->robotsptr = &robots;

  opt->maxfilter = maximum(opt->maxfilter, 128);
  filters_init(&filters, opt->maxfilter, 0);
  opt->filters.filters = &filters;
  opt->filters.filptr = &filptr;

  opt->hash = &hash;
  hts_record_init(opt);
  hash_init(opt, &hash, opt->urlhack);
  hash.liens = (const lien_url *const *const *) &opt->liens;

  sback = back_new(opt, 8); /* a small table keeps runs fast */

  host = (bits2 & OPT_OTHER_HOST) ? "www.example.com:8080" : "example.com";
  page = pages[(bits >> OPT_PAGE_SHIFT) & OPT_PAGE_MASK];
  hts_record_link(opt, host, "/", "/dev/null", "", "", NULL);
  hts_record_link(opt, host, page, redir_sav, "", "", NULL);
  ptr = 1;

  if (bits & OPT_PARTIAL)
    put_file(redir_sav, fuzz_span(rest, restlen));
  if (bits & OPT_REF) {
    ref = strdupt(url_savename_refname_fullpath(opt, host, page));
    if (structcheck(ref) != 0)
      abort();
    put_file(ref, fuzz_span(rest, restlen));
  }

  hts_init_htsblk(&r);
  r.statuscode = statuses[bits & 0x07];
  r.location = location;
  strcpybuff(r.msg, "Moved");
  strcpybuff(r.contenttype, "application/octet-stream");
  r.adr = strdupt("body");
  r.size = (LLint) strlen(r.adr);

  memset(&str, 0, sizeof(str));
  memset(&stre, 0, sizeof(stre));
  str.err_msg = err_msg;
  str.filename = heap(ptr)->sav;
  str.mime = r.contenttype;
  str.url_host = heap(ptr)->adr;
  str.url_file = heap(ptr)->fil;
  str.size = (int) r.size;
  str.opt = opt;
  str.sback = sback;
  str.cache = &cache;
  str.hashptr = &hash;
  str.numero_passe = 0;
  str.ptr_ = &ptr;
  str.page_charset_ = NULL;

  stre.r_ = &r;
  stre.error_ = &error;
  stre.exit_xh_ = &opt->state.exit_xh;
  stre.store_errpage_ = &store_errpage;
  stre.base = base;
  stre.codebase = codebase;
  stre.filters_ = &filters;
  stre.filptr_ = &filptr;
  stre.robots_ = &robots;
  stre.hash_ = &hash;
  stre.makeindex_done_ = &makeindex_done;
  stre.makeindex_fp_ = &makeindex_fp;
  stre.makeindex_links_ = &makeindex_links;
  stre.makeindex_firstlink_ = makeindex_firstlink;
  stre.stat_fragment_ = &stat_fragment;
  stre.makestat_time = 0;
  stre.makestat_fp = NULL;
  stre.makestat_total_ = &makestat_total;
  stre.makestat_lnk_ = &makestat_lnk;
  stre.maketrack_fp = NULL;

  (void) hts_mirror_check_moved(&str, &stre);
  if (HTTP_IS_REDIRECT(r.statuscode))
    check_stub(&r);

  (void) unlink(redir_sav);
  if (ref != NULL) {
    (void) unlink(ref);
    freet(ref);
  }
  if (r.adr != NULL)
    freet(r.adr);
  back_delete_all(opt, &cache, sback);
  back_free(&sback);
  hash_free(&hash);
  coucal_delete(&cache.hashtable);
  coucal_delete(&cache.cached_tests);
  checkrobots_free(&robots);
  if (filters != NULL) {
    if (filters[0] != NULL)
      freet(filters[0]);
    freet(filters);
  }
  hts_record_free(opt);
  hts_free_opt(opt);
  return 0;
}
