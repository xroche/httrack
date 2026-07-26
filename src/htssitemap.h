/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998 Xavier Roche and other contributors

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
/* HTTrack sitemap ingestion (sitemaps.org 0.9). Internal, not installed.
   Reads <urlset>/<sitemapindex> documents, plain or gzip-framed, and feeds
   their <loc> URLs to the crawl as top-level seeds. The whole input is
   attacker-controlled, so every entry point below is capped. */
/* ------------------------------------------------------------ */

#ifndef HTS_SITEMAP_DEFH
#define HTS_SITEMAP_DEFH

#include "htsdefines.h"
#include "htsopt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Caps. sitemaps.org allows 50000 URLs and 50 MB uncompressed per document;
   the byte cap sits above that so a conformant sitemap always fits. */
#define HTS_SITEMAP_MAX_URLS_DOC 50000           /* <loc> per document */
#define HTS_SITEMAP_MAX_URLS_TOTAL 200000        /* <loc> per mirror */
#define HTS_SITEMAP_MAX_DOCS 256                 /* documents per mirror */
#define HTS_SITEMAP_MAX_LEVEL 4                  /* sitemapindex nesting */
#define HTS_SITEMAP_MAX_BYTES (64 * 1024 * 1024) /* decompressed document */

/* Per-URL handler; returning HTS_FALSE stops the scan. */
typedef hts_boolean (*hts_sitemap_handler)(void *arg, const char *url);

/* Scan one sitemap document, plain or gzip-framed, handing every acceptable
   absolute http(s) <loc> URL to `handler`. Stops after `maxurls` URLs, or when
   the handler refuses. `is_index` (optional) reports a <sitemapindex>, whose
   URLs are child sitemaps rather than pages. Returns the number of URLs handed
   out, or -1 when the document could not be decompressed within the caps. */
int hts_sitemap_scan(const char *body, size_t size, int maxurls,
                     hts_boolean *is_index, hts_sitemap_handler handler,
                     void *arg);

/* Same, over a NUL-terminated robots.txt body: hands out the "Sitemap:" URLs
   (a group-independent record in RFC 9309, so no user-agent grouping). */
int hts_sitemap_scan_robots(const char *body, size_t size, int maxurls,
                            hts_sitemap_handler handler, void *arg);

/* --- Engine glue (needs a live httrackp). --- */

/* Queue the first sitemap document of the mirror: the explicit --sitemap-url,
   or the start host's /robots.txt probe for --sitemap. `starturl` is the first
   command-line seed. No-op when neither option is set. */
void hts_sitemap_seed(httrackp *opt, const char *starturl);

/* HTS_TRUE when (adr,fil) is a queued sitemap document awaiting ingestion. */
hts_boolean hts_sitemap_pending(httrackp *opt, const char *adr,
                                const char *fil);

/* Ingest a fetched sitemap document (or the robots.txt probe): seed its URLs
   through the wizard via htsAddLink, and queue nested sitemaps. `str` supplies
   the parser context of the document being processed. */
void hts_sitemap_ingest(httrackp *opt, htsmoduleStruct *str, const char *adr,
                        const char *fil, const char *body, size_t size);

/* Release the ingestion state held in opt (NULL-safe, idempotent). */
void hts_sitemap_free(httrackp *opt);

#ifdef __cplusplus
}
#endif

#endif
