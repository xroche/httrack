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

/* Fuzz the save-name builder (htsname.c): url_savename() turns a hostile
   URL into the local on-disk path through the query stripper, the URL
   normaliser, the extension logic, the 8.3/ISO9660 shorteners and the
   hash-backed collision renamer. The fuzz input is a flag byte plus
   "host<sep>path" lines; every line is one url_savename() call against the
   shared link table, so later lines hit the dedup and collision branches of
   earlier ones. Hosts keep their attacker-chosen shape (scheme, credentials,
   port, IPv6, trailing dot). The module's small string helpers run alongside
   the same bytes. A path that escapes the mirror folder is a real finding,
   as is any overflow along the way. */
#include "fuzz.h"

#include "htscore.h"
#include "htshash.h"
#include "htsmodules.h"
#include "htsname.h"
#include "coucal.h"

#include <string.h>

/* One save-name computation per input line; results accumulate into a cheap
   checksum so no call is optimised away and every output byte is read. */
static unsigned long savename_sink;

static void fuzz_line(const char *line, httrackp *opt, cache_back *cache,
                      hash_struct *hash, int flags) {
  lien_adrfilsave afs;
  lien_adrfil former;
  const char *sep;
  size_t n;

  memset(&afs, 0, sizeof(afs));
  memset(&former, 0, sizeof(former));
  n = strcspn(line, " \t");
  if (strlen(line) >= HTS_URLMAXSIZE * 2)
    return;
  if (n > 0 && line[n] != '\0' && n < HTS_URLMAXSIZE * 2) {
    memcpy(afs.af.adr, line, n);
    afs.af.adr[n] = '\0';
    strlcpybuff(afs.af.fil, line + n + 1, HTS_URLMAXSIZE * 2);
  } else {
    strlcpybuff(afs.af.adr, "example.com", HTS_URLMAXSIZE * 2);
    strlcpybuff(afs.af.fil, line, HTS_URLMAXSIZE * 2);
  }

  (void) url_savename(&afs, &former, "example.com", "/", opt, NULL, cache,
                      hash, 1, 0, NULL);
  savename_sink += (unsigned long) strlen(afs.save);
  savename_sink += (unsigned char) afs.save[0];

  /* The ref name (host+path fingerprint) of the same URL. */
  {
    char refname[HTS_URLMAXSIZE * 2];

    url_savename_refname(afs.af.adr, afs.af.fil, refname);
    savename_sink += (unsigned long) strlen(refname);
  }

  /* The append helpers that build names piece by piece. */
  {
    char d[128];

    d[0] = '\0';
    url_savename_addstr(d, sizeof(d), line);
    url_savename_addtail(d, sizeof(d), "-", line);
    savename_sink += (unsigned long) strlen(d);
  }

  /* The query stripper's lookup. */
  {
    char dst[64];

    (void) url_query_value(line, "id", dst, sizeof(dst));
    savename_sink += (unsigned long) strlen(dst);
  }

  /* The short-name layouts (standard_name works on positions inside the
     path buffer). */
  {
    char fil[HTS_URLMAXSIZE * 2];
    char b[HTS_URLMAXSIZE * 2];
    const char *dot;
    const char *nom;

    strlcpybuff(fil, afs.af.fil, sizeof(fil));
    dot = strrchr(fil, '.');
    nom = strrchr(fil, '/');
    standard_name(b, sizeof(b), dot != NULL ? dot : fil + strlen(fil),
                  nom != NULL ? nom : fil, fil, flags & 1);
    savename_sink += (unsigned long) strlen(b);
  }

  /* The md5 fallback name. */
  {
    char digest[32 + 1];

    savename_sink += (unsigned long) strlen(url_md5(digest, afs.af.fil));
  }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static int inited = 0;
  httrackp *opt;
  cache_back cache;
  hash_struct hash;
  char *buf;
  char *p;
  int flags;

  if (size < 2)
    return 0;
  if (!inited) {
    hts_init();
    inited = 1;
  }

  flags = data[0];
  data++;
  size--;

  opt = hts_create_opt();
  opt->log = opt->errlog = NULL;

  /* The saved-name option matrix the command line can reach. */
  opt->savename_83 = (hts_savename_83) (flags & 3);
  opt->urlhack = (flags >> 2) & 1;
  opt->check_type = (flags >> 3) & 1;
  opt->savename_delayed = (hts_savename_delayed) ((flags >> 4) & 3);

  memset(&cache, 0, sizeof(cache));
  cache.type = 0; /* no on-disk cache */
  cache.hashtable = coucal_new(0);
  cache.cached_tests = coucal_new(0);
  coucal_value_is_malloc(cache.cached_tests, 1);

  opt->hash = &hash;
  hts_record_init(opt);
  hash_init(opt, &hash, opt->urlhack);
  hash.liens = (const lien_url *const *const *) &opt->liens;
  hts_record_link(opt, "example.com", "/", "/dev/null", "", "", NULL);
  hts_record_link(opt, "example.com", "/index.html", "/dev/null", "", "", NULL);

  buf = fuzz_strdup(data, size);
  p = buf;
  while (p != NULL && *p != '\0') {
    char *nl = strchr(p, '\n');
    char *cr;
    size_t len;

    if (nl != NULL)
      *nl = '\0';
    cr = strchr(p, '\r');
    if (cr != NULL)
      *cr = '\0';
    len = strlen(p);
    if (len > 0)
      fuzz_line(p, opt, &cache, &hash, flags);
    p = (nl != NULL) ? nl + 1 : NULL;
  }
  freet(buf);

  hash_free(&hash);
  coucal_delete(&cache.hashtable);
  coucal_delete(&cache.cached_tests);
  hts_record_free(opt);
  hts_free_opt(opt);
  return 0;
}
