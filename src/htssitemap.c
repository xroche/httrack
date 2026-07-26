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
/* File: sitemap ingestion (sitemaps.org 0.9)                   */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#define HTS_INTERNAL_BYTECODE

#include "htscore.h"
#include "htssitemap.h"

#include "htsbase.h"
#include "htscodec.h"
#include "htsencoding.h"
#include "htsfilters.h"
#include "htshash.h"
#include "htsmodules.h"
#include "htslib.h"
#include "htsrobots.h"
#include "htssafe.h"
#include "htstools.h"

#include <ctype.h>
#include <string.h>

/* One queued sitemap document awaiting ingestion. */
typedef struct sitemap_doc {
  char adr[HTS_URLMAXSIZE];
  char fil[HTS_URLMAXSIZE];
  int level;
  hts_sitemap_source src;
  hts_boolean done;
  struct sitemap_doc *next;
} sitemap_doc;

struct hts_sitemap_state {
  sitemap_doc *docs;
  int ndocs; /* documents queued, capped by HTS_SITEMAP_MAX_DOCS */
  int nurls; /* URLs seeded, capped by HTS_SITEMAP_MAX_URLS_TOTAL */
  hts_boolean probe_done;    /* the robots.txt probe has been answered */
  hts_boolean fallback_done; /* the /sitemap.xml fallback was already queued */
  /* The crawl's own start URL. Seeded URLs are judged against it, so a site
     cannot widen a subtree crawl by putting its sitemap at the root. */
  char anchor_adr[HTS_URLMAXSIZE];
  char anchor_fil[HTS_URLMAXSIZE];
};
typedef struct hts_sitemap_state hts_sitemap_state;

/* --------------------------------------------------------------------- */
/* Document parsing (no engine state: fuzzable and self-testable)         */
/* --------------------------------------------------------------------- */

/* Accept only an absolute http(s) URL with no space or control byte. */
static hts_boolean sitemap_url_ok(const char *url) {
  const char *p;

  if (!strfield(url, "http://") && !strfield(url, "https://"))
    return HTS_FALSE;
  for (p = url; *p != '\0'; p++) {
    if ((unsigned char) *p <= ' ' || (unsigned char) *p == 0x7f)
      return HTS_FALSE;
  }
  return HTS_TRUE;
}

/* Skip to the character after the next '>' at or after p, or NULL. */
static const char *sitemap_tag_end(const char *p, const char *end) {
  while (p < end && *p != '>')
    p++;
  return p < end ? p + 1 : NULL;
}

/* HTS_TRUE when the document's root element is `name`. Skips the XML
   declaration, comments and processing instructions first, so a comment
   mentioning the other root element cannot decide the document type. */
static hts_boolean sitemap_root_is(const char *doc, size_t size,
                                   const char *name) {
  const size_t nlen = strlen(name);
  size_t i = 0;

  if (size >= 3 && memcmp(doc, "\xef\xbb\xbf", 3) == 0)
    i = 3; /* UTF-8 BOM */
  while (i < size) {
    if (isspace((unsigned char) doc[i])) {
      i++;
    } else if (doc[i] != '<') {
      return HTS_FALSE; /* character data before any element: not XML */
    } else if (i + 4 <= size && memcmp(doc + i, "<!--", 4) == 0) {
      const char *const e = hts_memstr(doc + i, size - i, "-->", 3);

      if (e == NULL)
        return HTS_FALSE;
      i = (size_t) (e - doc) + 3;
    } else if (i + 2 <= size && (doc[i + 1] == '?' || doc[i + 1] == '!')) {
      while (i < size && doc[i] != '>')
        i++;
      i++;
    } else {
      size_t j = i + 1;

      /* an optional namespace prefix: <sm:sitemapindex> is the same element */
      while (j < size && doc[j] != ':' && doc[j] != '>' &&
             !isspace((unsigned char) doc[j]))
        j++;
      if (j >= size || doc[j] != ':')
        j = i + 1;
      else
        j++;
      return j + nlen <= size && memcmp(doc + j, name, nlen) == 0 &&
                     (j + nlen == size ||
                      isspace((unsigned char) doc[j + nlen]) ||
                      doc[j + nlen] == '>' || doc[j + nlen] == '/')
                 ? HTS_TRUE
                 : HTS_FALSE;
    }
  }
  return HTS_FALSE;
}

/* Decompress a gzip-framed body into a fresh buffer. The 64 MiB cap is what
   binds in practice; deflate tops out near 1032:1, so the tree's codec budget
   only matters as the shared policy for a coding that could go further. */
static char *sitemap_gunzip(const char *body, size_t size, size_t *outsize) {
  const LLint budget = hts_codec_maxout((LLint) size);
  size_t cap = budget < (LLint) HTS_SITEMAP_MAX_BYTES
                   ? (size_t) budget
                   : (size_t) HTS_SITEMAP_MAX_BYTES;
  char *out;
  size_t n;

  if (cap == 0)
    return NULL;
  out = malloct(cap + 1);
  if (out == NULL)
    return NULL;
  n = hts_codec_head(HTS_CODEC_DEFLATE, body, size, out, cap);
  if (n == 0) {
    freet(out);
    return NULL;
  }
  out[n] = '\0';
  *outsize = n;
  return out;
}

int hts_sitemap_scan(const char *body, size_t size, int maxurls,
                     hts_boolean *is_index, hts_sitemap_handler handler,
                     void *arg) {
  char *unpacked = NULL;
  const char *doc;
  const char *end;
  const char *p;
  int n = 0;

  if (is_index != NULL)
    *is_index = HTS_FALSE;
  if (body == NULL || size < 2 || handler == NULL)
    return 0;

  /* Content-Encoding gzip is undone upstream; only the container is left. */
  if ((unsigned char) body[0] == 0x1f && (unsigned char) body[1] == 0x8b) {
    unpacked = sitemap_gunzip(body, size, &size);
    if (unpacked == NULL)
      return -1;
    doc = unpacked;
  } else {
    if (size > (size_t) HTS_SITEMAP_MAX_BYTES)
      size = (size_t) HTS_SITEMAP_MAX_BYTES;
    doc = body;
  }
  end = doc + size;

  /* Set before the first callback: the handler reads the verdict. */
  if (is_index != NULL)
    *is_index = sitemap_root_is(doc, size, "sitemapindex");

  for (p = doc; n < maxurls;) {
    const char *loc = hts_memstr(p, (size_t) (end - p), "<loc", 4);
    const char *val;
    const char *stop;
    size_t len;
    char BIGSTK url[HTS_URLMAXSIZE];

    if (loc == NULL)
      break;
    /* "<loc>" or "<loc xmlns:..>", never "<location>" */
    if (loc + 4 >= end || (loc[4] != '>' && !isspace((unsigned char) loc[4]))) {
      p = loc + 4;
      continue;
    }
    val = sitemap_tag_end(loc + 4, end);
    if (val == NULL)
      break;
    for (stop = val; stop < end && *stop != '<'; stop++)
      ;
    /* No closing tag: truncated document, so the value may be a partial URL. */
    if (stop == end)
      break;
    p = stop;
    while (val < stop && isspace((unsigned char) *val))
      val++;
    while (stop > val && isspace((unsigned char) *(stop - 1)))
      stop--;
    len = (size_t) (stop - val);
    /* Overflow-safe: the untrusted length alone against the room left. */
    if (len == 0 || len >= sizeof(url))
      continue;
    memcpy(url, val, len);
    url[len] = '\0';
    /* hts_unescapeEntities decodes in place and tolerates src == dest; a
       reference to a control byte survives as one and sitemap_url_ok drops it.
     */
    if (hts_unescapeEntities(url, url, sizeof(url)) != 0 ||
        !sitemap_url_ok(url))
      continue;
    n++;
    if (!handler(arg, url))
      break;
  }

  if (unpacked != NULL)
    freet(unpacked);
  return n;
}

/* --------------------------------------------------------------------- */
/* Engine glue                                                           */
/* --------------------------------------------------------------------- */

static hts_sitemap_state *sitemap_get_state(httrackp *opt) {
  if (opt->sitemap_state == NULL)
    opt->sitemap_state = calloct(1, sizeof(hts_sitemap_state));
  return (hts_sitemap_state *) opt->sitemap_state;
}

static sitemap_doc *sitemap_find(httrackp *opt, const char *adr,
                                 const char *fil) {
  hts_sitemap_state *const st = (hts_sitemap_state *) opt->sitemap_state;
  sitemap_doc *d;

  if (st == NULL)
    return NULL;
  for (d = st->docs; d != NULL; d = d->next) {
    if (strfield2(d->adr, adr) && strcmp(d->fil, fil) == 0)
      return d;
  }
  return NULL;
}

/* Who asked for this document decides how far it is gated. The wizard proper
   is not usable here: it wants a referring link, and its up/down travel rules
   would judge a child sitemap against the parent sitemap's directory. */
static hts_boolean sitemap_fetch_allowed(httrackp *opt, const char *adr,
                                         const char *fil,
                                         hts_sitemap_source src) {
  /* adr and fil are each capped just under HTS_URLMAXSIZE, and lfull prefixes
     a scheme and a slash on top of both: 2 * HTS_URLMAXSIZE does not fit. */
  char BIGSTK l[HTS_URLMAXSIZE * 2 + 16], lfull[HTS_URLMAXSIZE * 2 + 16];
  int jokdepth = 0, jok;

  hts_boolean refused;

  /* The user naming a sitemap is the same intent as naming a start URL, which
     the wizard admits unconditionally. */
  if (src == HTS_SITEMAP_SRC_USER)
    return HTS_TRUE;
  strcpybuff(l, jump_identification_const(adr));
  if (*fil != '/')
    strcatbuff(l, "/");
  strcatbuff(l, fil);
  strcpybuff(lfull, link_has_authority(adr) ? "" : "http://");
  strcatbuff(lfull, adr);
  if (*fil != '/')
    strcatbuff(lfull, "/");
  strcatbuff(lfull, fil);
  jok = fa_strjoker_dual(0, *opt->filters.filters, *opt->filters.filptr, lfull,
                         l, NULL, NULL, &jokdepth);
  refused = (jok == -1) ? HTS_TRUE : HTS_FALSE;
  if (refused) {
    hts_log_print(opt, LOG_NOTICE, "Sitemap: filter rule #%d refuses %s%s",
                  jokdepth + 1, adr, fil);
    return HTS_FALSE;
  }
  /* A Sitemap: line, or a sitemapindex entry, is the site inviting the fetch;
     a Disallow elsewhere in the same file does not retract it. The well-known
     location is only ever a guess, so there a Disallow wins. */
  if (src == HTS_SITEMAP_SRC_GUESSED &&
      hts_robots_forbids(opt, adr, fil, (jok != 0) ? HTS_TRUE : HTS_FALSE,
                         refused)) {
    hts_log_print(opt, LOG_NOTICE, "Sitemap: robots.txt forbids %s%s", adr,
                  fil);
    return HTS_FALSE;
  }
  return HTS_TRUE;
}

/* Record the link with save="" so the body stays in memory: a sitemap is
   ingested, never mirrored. */
static hts_boolean sitemap_queue_(httrackp *opt, const char *adr,
                                  const char *fil, int level,
                                  hts_sitemap_source src, hts_boolean link_it) {
  hts_sitemap_state *const st = sitemap_get_state(opt);
  sitemap_doc *d;

  if (st == NULL)
    return HTS_FALSE;
  if (st->ndocs >= HTS_SITEMAP_MAX_DOCS || level > HTS_SITEMAP_MAX_LEVEL) {
    hts_log_print(opt, LOG_WARNING, "Sitemap: cap reached, skipping %s%s", adr,
                  fil);
    return HTS_FALSE;
  }
  if (strlen(adr) >= sizeof(d->adr) || strlen(fil) >= sizeof(d->fil))
    return HTS_FALSE;
  if (sitemap_find(opt, adr, fil) != NULL)
    return HTS_FALSE;
  if (!sitemap_fetch_allowed(opt, adr, fil, src))
    return HTS_FALSE;
  d = calloct(1, sizeof(sitemap_doc));
  if (d == NULL)
    return HTS_FALSE;
  strcpybuff(d->adr, adr);
  strcpybuff(d->fil, fil);
  d->level = level;
  d->src = src;
  d->next = st->docs;
  st->docs = d;
  st->ndocs++;

  if (!link_it)
    return HTS_TRUE;
  if (!hts_record_link(opt, adr, fil, "", "", "", NULL))
    return HTS_FALSE;
  heap_top()->testmode = 0;
  heap_top()->link_import = 0;
  heap_top()->depth = opt->depth + 1;
  heap_top()->pass2 = 0;
  heap_top()->retry = opt->retry;
  heap_top()->premier = heap_top_index();
  heap_top()->precedent = heap_top_index();
  hts_log_print(opt, LOG_INFO, "Sitemap: queued %s%s", adr, fil);
  return HTS_TRUE;
}

static hts_boolean sitemap_queue(httrackp *opt, const char *adr,
                                 const char *fil, int level,
                                 hts_sitemap_source src) {
  return sitemap_queue_(opt, adr, fil, level, src, HTS_TRUE);
}

void hts_sitemap_redirect(httrackp *opt, const char *adr, const char *fil,
                          const char *newadr, const char *newfil) {
  sitemap_doc *const d = sitemap_find(opt, adr, fil);

  if (d == NULL || d->done)
    return;
  d->done = HTS_TRUE; /* the body lives at the target now */
  /* The engine already queued the target link, so only the marking moves. */
  (void) sitemap_queue_(opt, newadr, newfil, d->level, d->src, HTS_FALSE);
  hts_log_print(opt, LOG_NOTICE, "Sitemap: %s%s redirects to %s%s", adr, fil,
                newadr, newfil);
}

void hts_sitemap_seed(httrackp *opt, const char *starturl) {
  char BIGSTK url[HTS_URLMAXSIZE * 2];
  lien_adrfil af;

  if (StringNotEmpty(opt->sitemap_url)) {
    if (strlen(StringBuff(opt->sitemap_url)) >= sizeof(url)) {
      hts_log_print(opt, LOG_ERROR, "Sitemap URL too long");
    } else {
      strcpybuff(url, StringBuff(opt->sitemap_url));
      if (strstr(url, ":/") == NULL)
        hts_log_print(opt, LOG_ERROR, "Sitemap URL must be absolute: %s", url);
      else if (ident_url_absolute(url, &af) >= 0)
        (void) sitemap_queue(opt, af.adr, af.fil, 0, HTS_SITEMAP_SRC_USER);
    }
  }
  if (starturl == NULL || starturl[0] == '\0' ||
      strlen(starturl) >= sizeof(url))
    return;
  strcpybuff(url, starturl);
  if (ident_url_absolute(url, &af) < 0)
    return;
  {
    hts_sitemap_state *const st = sitemap_get_state(opt);

    if (st != NULL && strlen(af.adr) < sizeof(st->anchor_adr) &&
        strlen(af.fil) < sizeof(st->anchor_fil)) {
      strcpybuff(st->anchor_adr, af.adr);
      strcpybuff(st->anchor_fil, af.fil);
    }
  }
  if (!opt->sitemap)
    return;
  /* Answered in hts_sitemap_robots, once the parsed rules are installed. */
  if (hts_record_link(opt, af.adr, "/robots.txt", "", "", "", NULL)) {
    heap_top()->testmode = 0;
    heap_top()->link_import = 0;
    heap_top()->depth = 0;
    heap_top()->pass2 = 0;
    heap_top()->retry = opt->retry;
    heap_top()->premier = heap_top_index();
    heap_top()->precedent = heap_top_index();
    /* Claim the host so the parser does not queue robots.txt a second time. */
    if (opt->robotsptr != NULL)
      (void) checkrobots_set((robots_wizard *) opt->robotsptr, af.adr, "");
  }
}

void hts_sitemap_robots(httrackp *opt, const char *adr, const char *sitemaps) {
  hts_sitemap_state *const st = (hts_sitemap_state *) opt->sitemap_state;
  int queued = 0;

  if (st == NULL || !opt->sitemap || st->probe_done ||
      !strfield2(st->anchor_adr, adr))
    return;
  st->probe_done = HTS_TRUE;
  if (sitemaps != NULL) {
    const char *p = sitemaps;

    while (*p != '\0') {
      const char *const eol = strchr(p, '\n');
      const size_t len = eol != NULL ? (size_t) (eol - p) : strlen(p);
      char BIGSTK line[HTS_URLMAXSIZE];
      lien_adrfil af;

      if (len > 0 && len < sizeof(line)) {
        memcpy(line, p, len);
        line[len] = '\0';
        /* Same host: a Sitemap: line must not aim the fetcher elsewhere. */
        if (sitemap_url_ok(line) && ident_url_absolute(line, &af) >= 0 &&
            strfield2(af.adr, adr) &&
            sitemap_queue(opt, af.adr, af.fil, 0, HTS_SITEMAP_SRC_DECLARED))
          queued++;
      }
      if (eol == NULL)
        break;
      p = eol + 1;
    }
  }
  if (queued == 0 && !st->fallback_done) {
    st->fallback_done = HTS_TRUE;
    if (sitemap_queue(opt, adr, "/sitemap.xml", 0, HTS_SITEMAP_SRC_GUESSED))
      queued++;
  }
  hts_log_print(opt, LOG_NOTICE, "Sitemap: %d sitemap(s) queued for %s", queued,
                adr);
}

hts_boolean hts_sitemap_pending(httrackp *opt, const char *adr,
                                const char *fil) {
  const sitemap_doc *const d = sitemap_find(opt, adr, fil);

  return d != NULL && !d->done ? HTS_TRUE : HTS_FALSE;
}

/* Handler context: seeding URLs from one document. */
typedef struct sitemap_ingest_ctx {
  httrackp *opt;
  htsmoduleStruct *str;
  const char *adr; /* host of the document being ingested */
  int level;
  hts_boolean is_index;
  int accepted; /* URLs seeded or documents queued, not merely parsed */
} sitemap_ingest_ctx;

/* A <loc> of a <urlset>: hand it to the wizard as a top-level seed.
   The wizard is pointed at the crawl's own start URL, not at the sitemap: the
   site picks where its sitemap lives, so anchoring travel there would let a
   root sitemap widen a subtree crawl to the whole host. The URL then becomes
   its own anchor, exactly as a command-line seed does. */
static hts_boolean sitemap_seed_url(void *arg, const char *url) {
  sitemap_ingest_ctx *const c = (sitemap_ingest_ctx *) arg;
  httrackp *const opt = c->opt;
  hts_sitemap_state *const st = sitemap_get_state(opt);
  char BIGSTK buff[HTS_URLMAXSIZE];
  int before;

  if (st == NULL || st->nurls >= HTS_SITEMAP_MAX_URLS_TOTAL) {
    hts_log_print(opt, LOG_WARNING,
                  "Sitemap: URL cap reached, ignoring the rest");
    return HTS_FALSE;
  }
  /* strcpybuff aborts rather than truncating: never feed it unchecked input. */
  if (strlen(url) >= sizeof(buff))
    return HTS_TRUE;
  st->nurls++;
  strcpybuff(buff, url);
  before = opt->lien_tot;
  if (htsAddLink(c->str, buff))
    c->accepted++;
  if (opt->lien_tot > before)
    heap_top()->premier = heap_top_index(); /* a seed anchors on itself */
  return HTS_TRUE;
}

/* A <loc> of a <sitemapindex>: cross-host children are dropped, so a hostile
   sitemap cannot aim the fetcher elsewhere. */
static hts_boolean sitemap_seed_child(void *arg, const char *url) {
  sitemap_ingest_ctx *const c = (sitemap_ingest_ctx *) arg;
  char BIGSTK buff[HTS_URLMAXSIZE];
  lien_adrfil af;

  if (strlen(url) >= sizeof(buff))
    return HTS_TRUE;
  strcpybuff(buff, url);
  if (ident_url_absolute(buff, &af) < 0)
    return HTS_TRUE;
  if (!strfield2(af.adr, c->adr)) {
    hts_log_print(c->opt, LOG_WARNING,
                  "Sitemap: ignoring off-host child sitemap %s%s", af.adr,
                  af.fil);
    return HTS_TRUE;
  }
  if (sitemap_queue(c->opt, af.adr, af.fil, c->level + 1,
                    HTS_SITEMAP_SRC_DECLARED))
    c->accepted++;
  return HTS_TRUE;
}

/* The scan classifies the document before the first callback. */
static hts_boolean sitemap_seed_any(void *arg, const char *url) {
  sitemap_ingest_ctx *const c = (sitemap_ingest_ctx *) arg;

  return c->is_index ? sitemap_seed_child(arg, url)
                     : sitemap_seed_url(arg, url);
}

void hts_sitemap_ingest(httrackp *opt, htsmoduleStruct *str, const char *adr,
                        const char *fil, const char *body, size_t size) {
  sitemap_doc *const d = sitemap_find(opt, adr, fil);
  hts_sitemap_state *const st = (hts_sitemap_state *) opt->sitemap_state;
  sitemap_ingest_ctx ctx;
  int n, anchor, saved_depth;

  if (d == NULL || d->done)
    return;
  d->done = HTS_TRUE;
  /* str->ptr_ is a scratch int owned by the caller, so nothing else moves. */
  anchor = *str->ptr_;
  if (st != NULL && st->anchor_adr[0] != '\0' && opt->hash != NULL) {
    const int i = hash_read((const hash_struct *) opt->hash, st->anchor_adr,
                            st->anchor_fil, 1);

    if (i >= 0)
      anchor = i;
  }
  *str->ptr_ = anchor;
  /* Borrow the anchor's position but keep a seed's full depth budget. */
  saved_depth = heap(anchor)->depth;
  heap(anchor)->depth = opt->depth + 1;
  ctx.opt = opt;
  ctx.str = str;
  ctx.adr = adr;
  ctx.level = d->level;
  ctx.is_index = HTS_FALSE;
  ctx.accepted = 0;

  n = hts_sitemap_scan(body, size, HTS_SITEMAP_MAX_URLS_DOC, &ctx.is_index,
                       sitemap_seed_any, &ctx);
  heap(anchor)->depth = saved_depth;
  if (n < 0) {
    hts_log_print(opt, LOG_ERROR, "Sitemap: could not decompress %s%s", adr,
                  fil);
    return;
  }
  if (ctx.is_index)
    hts_log_print(opt, LOG_NOTICE,
                  "Sitemap: %d of %d child sitemap(s) listed by %s%s",
                  ctx.accepted, n, adr, fil);
  else
    hts_log_print(opt, LOG_NOTICE, "Sitemap: %d of %d URL(s) added from %s%s",
                  ctx.accepted, n, adr, fil);
}

void hts_sitemap_free(httrackp *opt) {
  hts_sitemap_state *const st = (hts_sitemap_state *) opt->sitemap_state;

  if (st == NULL)
    return;
  while (st->docs != NULL) {
    sitemap_doc *const next = st->docs->next;

    freet(st->docs);
    st->docs = next;
  }
  freet(opt->sitemap_state);
  opt->sitemap_state = NULL;
}
