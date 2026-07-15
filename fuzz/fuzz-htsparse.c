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

/* Fuzz the real HTML parser htsparse() (htsparse.c) over a mocked engine: the
   tag/attribute/JS-inscript scanner, the link rewriter, and the
   accept -> url_savename -> hts_record_link path, on every crawled page. The
   coupling is state, not network, so we build the minimal crawl state
   httpmirror() sets up and let the page walk it, then discard.
   The str/stre wiring below mirrors the htsparse() call site in htscore.c; keep
   it in sync if those structs gain a field the parser reads. */
#include "fuzz.h"

#include "httrack-library.h" /* hts_init, hts_create_opt, hts_free_opt */
#include "htscore.h"   /* heap(), hts_record_*, filters_init, cache_back */
#include "htsback.h"   /* back_new, back_free, back_delete_all */
#include "htshash.h"   /* hash_init, hash_free */
#include "htsrobots.h" /* checkrobots_free, robots_wizard */
#include "htsparse.h"  /* htsparse, htsmoduleStructExtended */
#include "htsmodules.h"
#include "coucal.h"

/* htsparse never calls the module addLink on the internal parse; stub anyway.
 */
static int fuzz_addlink(htsmoduleStruct *str, char *link) {
  (void) str;
  (void) link;
  return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static int inited = 0;

  httrackp *opt;
  cache_back cache;
  hash_struct hash;
  robots_wizard robots;
  struct_back *sback;
  char **filters = NULL;
  int filptr = 0;

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

  if (!inited) {
    hts_init();
    inited = 1;
  }

  opt = hts_create_opt();
  opt->log = opt->errlog = NULL;
  opt->robots = 0; /* skip the robots.txt fetch path */

  memset(&cache, 0, sizeof(cache));
  cache.type = 0; /* no on-disk cache */
  cache.hashtable = coucal_new(0);
  cache.cached_tests = coucal_new(0);
  coucal_value_is_malloc(cache.cached_tests, 1);

  memset(&robots, 0, sizeof(robots));
  strcpybuff(robots.adr, "!");
  opt->robotsptr = &robots;

  opt->maxfilter = maximum(opt->maxfilter, 128);
  filters_init(&filters, opt->maxfilter, 0);
  opt->filters.filters = &filters;
  opt->filters.filptr = &filptr;

  opt->hash = &hash;
  hts_record_init(opt);
  hash_init(opt, &hash, opt->urlhack);
  hash.liens = (const lien_url *const *const *) &opt->liens;

  sback = back_new(opt, opt->maxsoc * 32 + 1024);

  /* Seed the link heap: idx 0 primary, idx 1 the parsed page; ptr>0 selects
     full HTML parsing + the rewriter. urladr()/urlfil()/savename() alias
     heap(ptr); save=/dev/null makes the end-of-parse flush hermetic. */
  hts_record_link(opt, "example.com", "/", "/dev/null", "", "", NULL);
  hts_record_link(opt, "example.com", "/index.html", "/dev/null", "", "", NULL);
  ptr = 1;

  /* The downloaded object: NUL-terminated, size bytes in a size+1 allocation
     (htsparse one-past-reads onto the terminator). */
  hts_init_htsblk(&r);
  r.statuscode = 200;
  r.size = (LLint) size;
  r.adr = malloct(size + 1);
  memcpy(r.adr, data, size);
  r.adr[size] = '\0';
  strcpybuff(r.contenttype, "text/html");

  memset(&str, 0, sizeof(str));
  memset(&stre, 0, sizeof(stre));
  str.err_msg = codebase; /* scratch; htsparse writes it only on error */
  str.filename = heap(ptr)->sav;
  str.mime = r.contenttype;
  str.url_host = heap(ptr)->adr;
  str.url_file = heap(ptr)->fil;
  str.size = (int) r.size;
  str.addLink = fuzz_addlink;
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
  stre.template_header_ = "";
  stre.template_body_ = "";
  stre.template_footer_ = "";
  stre.stat_fragment_ = &stat_fragment;
  stre.makestat_time = 0;
  stre.makestat_fp = NULL;
  stre.makestat_total_ = &makestat_total;
  stre.makestat_lnk_ = &makestat_lnk;
  stre.maketrack_fp = NULL;

  (void) htsparse(&str, &stre);

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
