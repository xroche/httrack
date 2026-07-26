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
/* File: sitemap ingestion (sitemaps.org 0.9)                   */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#define HTS_INTERNAL_BYTECODE

#include "htscore.h"
#include "htssitemap.h"

#include "htsbase.h"
#include "htscodec.h"
#include "htsfilters.h"
#include "htslib.h"
#include "htsrobots.h"
#include "htssafe.h"
#include "htstools.h"
#include "htszlib.h"

#include <ctype.h>
#include <string.h>

/* One queued sitemap document. `is_robots` marks the robots.txt probe, whose
   Sitemap: lines feed this same list. */
typedef struct sitemap_doc {
  char adr[HTS_URLMAXSIZE];
  char fil[HTS_URLMAXSIZE];
  int level;
  hts_boolean is_robots;
  hts_boolean done;
  struct sitemap_doc *next;
} sitemap_doc;

struct hts_sitemap_state {
  sitemap_doc *docs;
  int ndocs; /* documents queued, capped by HTS_SITEMAP_MAX_DOCS */
  int nurls; /* URLs seeded, capped by HTS_SITEMAP_MAX_URLS_TOTAL */
  hts_boolean fallback_done; /* the /sitemap.xml fallback was already queued */
};
typedef struct hts_sitemap_state hts_sitemap_state;

/* --------------------------------------------------------------------- */
/* Document parsing (no engine state: fuzzable and self-testable)         */
/* --------------------------------------------------------------------- */

/* Decode the five XML predefined entities and numeric character references
   in-place, never growing the string. Returns HTS_FALSE when a reference
   decodes outside printable ASCII: the value is then not a URL the site can
   have published, and decoding it would smuggle a control byte into the
   crawl. An unrecognized "&..." run is left verbatim ('&' is legal in a URL).
 */
static hts_boolean sitemap_unescape(char *s) {
  char *r = s, *w = s;
  hts_boolean ok = HTS_TRUE;

  while (*r != '\0') {
    if (*r != '&') {
      *w++ = *r++;
      continue;
    }
    {
      char *const semi = strchr(r + 1, ';');
      const size_t len = semi != NULL ? (size_t) (semi - (r + 1)) : 0;
      int c = -1;

      if (len == 0 || len > 8) {
        *w++ = *r++;
        continue;
      }
      if (len == 3 && strncmp(r + 1, "amp", 3) == 0)
        c = '&';
      else if (len == 2 && strncmp(r + 1, "lt", 2) == 0)
        c = '<';
      else if (len == 2 && strncmp(r + 1, "gt", 2) == 0)
        c = '>';
      else if (len == 4 && strncmp(r + 1, "quot", 4) == 0)
        c = '"';
      else if (len == 4 && strncmp(r + 1, "apos", 4) == 0)
        c = '\'';
      else if (r[1] == '#') {
        const int hex = (r[2] == 'x' || r[2] == 'X');
        const char *p = r + (hex ? 3 : 2);
        long v = 0;

        if (p < semi) {
          for (; p < semi; p++) {
            const int d =
                hex ? (isxdigit((unsigned char) *p)
                           ? (isdigit((unsigned char) *p)
                                  ? *p - '0'
                                  : (tolower((unsigned char) *p) - 'a' + 10))
                           : -1)
                    : (isdigit((unsigned char) *p) ? *p - '0' : -1);

            if (d < 0) {
              v = -1;
              break;
            }
            v = v * (hex ? 16 : 10) + d;
            if (v > 0x7e)
              break;
          }
          if (v >= 0x20 && v <= 0x7e)
            c = (int) v;
          else if (v >= 0)
            ok = HTS_FALSE; /* a well-formed reference to a byte no URL holds */
        }
      }
      if (c < 0) {
        *w++ = *r++;
      } else {
        *w++ = (char) c;
        r = semi + 1;
      }
    }
  }
  *w = '\0';
  return ok;
}

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

/* Bounded substring search: the document may hold NUL bytes. */
static const char *sitemap_memstr(const char *p, size_t len,
                                  const char *needle) {
  const size_t nlen = strlen(needle);

  if (nlen == 0 || len < nlen)
    return NULL;
  for (; len >= nlen; p++, len--) {
    if (*p == *needle && memcmp(p, needle, nlen) == 0)
      return p;
  }
  return NULL;
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
      const char *const e = sitemap_memstr(doc + i, size - i, "-->");

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
  n = hts_zhead(body, size, out, cap);
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

  /* A .xml.gz body arrives raw here: Content-Encoding gzip was already undone
     upstream, so only the gzip container is left to peel. */
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

  /* The root element classifies the document; the handler reads the verdict
     before the first URL, so it must be set up front. */
  if (is_index != NULL)
    *is_index = sitemap_root_is(doc, size, "sitemapindex");

  for (p = doc; n < maxurls;) {
    const char *loc = sitemap_memstr(p, (size_t) (end - p), "<loc");
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
    /* No closing tag: the document is truncated (cut short, or clipped by the
       decompression cap), so the last value may be a partial URL. Drop it. */
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
    if (!sitemap_unescape(url) || !sitemap_url_ok(url))
      continue;
    n++;
    if (!handler(arg, url))
      break;
  }

  if (unpacked != NULL)
    freet(unpacked);
  return n;
}

/* Copy one CR-stripped line into `line`, truncating an over-long one, and
   return how far to advance. Unlike binput() this stops at `size`, so a body
   that is not NUL-terminated cannot be read past its end. */
static size_t sitemap_line(const char *body, size_t size, char *line,
                           size_t linesize) {
  size_t i = 0, w = 0;

  while (i < size && body[i] != '\n') {
    if (body[i] != '\r' && w < linesize - 1)
      line[w++] = body[i];
    i++;
  }
  line[w] = '\0';
  return i < size ? i + 1 : i;
}

int hts_sitemap_scan_robots(const char *body, size_t size, int maxurls,
                            hts_sitemap_handler handler, void *arg) {
  size_t bptr = 0;
  int n = 0;

  if (body == NULL || handler == NULL)
    return 0;
  while (bptr < size && n < maxurls) {
    char BIGSTK line[HTS_URLMAXSIZE];
    char *comm;
    char *a;

    bptr += sitemap_line(body + bptr, size - bptr, line, sizeof(line));
    comm = strchr(line, '#');
    if (comm != NULL)
      *comm = '\0';
    if (!strfield(line, "sitemap:"))
      continue;
    a = line + 8;
    while (is_realspace(*a))
      a++;
    {
      size_t l = strlen(a);

      while (l > 0 && is_realspace(a[l - 1]))
        a[--l] = '\0';
    }
    if (!sitemap_url_ok(a))
      continue;
    n++;
    if (!handler(arg, a))
      break;
  }
  return n;
}

/* --------------------------------------------------------------------- */
/* Engine glue                                                           */
/* --------------------------------------------------------------------- */

static hts_sitemap_state *sitemap_state(httrackp *opt) {
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

/* A sitemap document is fetched like any other resource, so the user's filters
   and robots.txt decide whether it may be. The wizard proper is not usable
   here: it wants a referring link, and its up/down travel rules would judge a
   child sitemap against the parent sitemap's directory. */
static hts_boolean sitemap_fetch_allowed(httrackp *opt, const char *adr,
                                         const char *fil) {
  char BIGSTK l[HTS_URLMAXSIZE * 2], lfull[HTS_URLMAXSIZE * 2];
  int jokdepth = 0;

  if (opt->robots && opt->robotsptr != NULL &&
      checkrobots((robots_wizard *) opt->robotsptr, adr, fil) == -1) {
    hts_log_print(opt, LOG_NOTICE, "Sitemap: robots.txt forbids %s%s", adr,
                  fil);
    return HTS_FALSE;
  }
  strcpybuff(l, jump_identification_const(adr));
  if (*fil != '/')
    strcatbuff(l, "/");
  strcatbuff(l, fil);
  strcpybuff(lfull, link_has_authority(adr) ? "" : "http://");
  strcatbuff(lfull, adr);
  if (*fil != '/')
    strcatbuff(lfull, "/");
  strcatbuff(lfull, fil);
  if (fa_strjoker_dual(0, *opt->filters.filters, *opt->filters.filptr, lfull, l,
                       NULL, NULL, &jokdepth) == -1) {
    hts_log_print(opt, LOG_NOTICE, "Sitemap: filter rule #%d refuses %s%s",
                  jokdepth + 1, adr, fil);
    return HTS_FALSE;
  }
  return HTS_TRUE;
}

/* Queue a document and record its link with save="" so the body stays in
   memory: a sitemap is ingested, never mirrored. Top priority so its URLs get
   the full depth budget through htsAddLink. */
static hts_boolean sitemap_queue_(httrackp *opt, const char *adr,
                                  const char *fil, int level,
                                  hts_boolean is_robots, hts_boolean link_it) {
  hts_sitemap_state *const st = sitemap_state(opt);
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
  /* The robots.txt probe is the request that fetches the rules, so it cannot be
     judged by them; everything else can. */
  if (!is_robots && !sitemap_fetch_allowed(opt, adr, fil))
    return HTS_FALSE;
  d = calloct(1, sizeof(sitemap_doc));
  if (d == NULL)
    return HTS_FALSE;
  strcpybuff(d->adr, adr);
  strcpybuff(d->fil, fil);
  d->level = level;
  d->is_robots = is_robots;
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
                                 hts_boolean is_robots) {
  return sitemap_queue_(opt, adr, fil, level, is_robots, HTS_TRUE);
}

void hts_sitemap_redirect(httrackp *opt, const char *adr, const char *fil,
                          const char *newadr, const char *newfil) {
  sitemap_doc *const d = sitemap_find(opt, adr, fil);

  if (d == NULL || d->done)
    return;
  d->done = HTS_TRUE; /* the body lives at the target now */
  /* The engine already queued the target link, so only the marking moves. */
  (void) sitemap_queue_(opt, newadr, newfil, d->level, d->is_robots, HTS_FALSE);
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
        (void) sitemap_queue(opt, af.adr, af.fil, 0, HTS_FALSE);
    }
  }
  /* --sitemap: probe the start host's robots.txt, whose Sitemap: lines decide
     whether the /sitemap.xml fallback is needed. */
  if (!opt->sitemap || starturl == NULL || starturl[0] == '\0' ||
      strlen(starturl) >= sizeof(url))
    return;
  strcpybuff(url, starturl);
  if (ident_url_absolute(url, &af) < 0)
    return;
  if (sitemap_queue(opt, af.adr, "/robots.txt", 0, HTS_TRUE)) {
    /* Claim the host so the parser does not queue robots.txt a second time. */
    if (opt->robotsptr != NULL)
      (void) checkrobots_set((robots_wizard *) opt->robotsptr, af.adr, "");
  }
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

/* A <loc> of a <urlset>: hand it to the wizard as a top-level seed. */
static hts_boolean sitemap_seed_url(void *arg, const char *url) {
  sitemap_ingest_ctx *const c = (sitemap_ingest_ctx *) arg;
  hts_sitemap_state *const st = sitemap_state(c->opt);
  char BIGSTK buff[HTS_URLMAXSIZE];

  if (st == NULL || st->nurls >= HTS_SITEMAP_MAX_URLS_TOTAL) {
    hts_log_print(c->opt, LOG_WARNING,
                  "Sitemap: URL cap reached, ignoring the rest");
    return HTS_FALSE;
  }
  /* Both scanners bound the URL below this, but strcpybuff aborts rather than
     truncating, so never let hostile input reach it unchecked. */
  if (strlen(url) >= sizeof(buff))
    return HTS_TRUE;
  st->nurls++;
  strcpybuff(buff, url);
  /* htsAddLink reports the wizard's verdict; only count what it took. */
  if (htsAddLink(c->str, buff))
    c->accepted++;
  return HTS_TRUE;
}

/* A <loc> of a <sitemapindex>, or a robots.txt Sitemap: line. Cross-host
   children are dropped: a hostile sitemap must not aim the fetcher elsewhere.
 */
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
  if (sitemap_queue(c->opt, af.adr, af.fil, c->level + 1, HTS_FALSE))
    c->accepted++;
  return HTS_TRUE;
}

/* hts_sitemap_scan classifies the document before the first callback, so the
   urlset/sitemapindex split can be decided here. */
static hts_boolean sitemap_seed_any(void *arg, const char *url) {
  sitemap_ingest_ctx *const c = (sitemap_ingest_ctx *) arg;

  return c->is_index ? sitemap_seed_child(arg, url)
                     : sitemap_seed_url(arg, url);
}

void hts_sitemap_ingest(httrackp *opt, htsmoduleStruct *str, const char *adr,
                        const char *fil, const char *body, size_t size) {
  sitemap_doc *const d = sitemap_find(opt, adr, fil);
  sitemap_ingest_ctx ctx;
  int n;

  if (d == NULL || d->done)
    return;
  d->done = HTS_TRUE;
  ctx.opt = opt;
  ctx.str = str;
  ctx.adr = adr;
  ctx.level = d->level;
  ctx.is_index = HTS_FALSE;
  ctx.accepted = 0;

  if (d->is_robots) {
    hts_sitemap_state *const st = sitemap_state(opt);

    n = body != NULL ? hts_sitemap_scan_robots(body, size, HTS_SITEMAP_MAX_DOCS,
                                               sitemap_seed_child, &ctx)
                     : 0;
    /* Fall back to the well-known location when robots.txt queued nothing: a
     lone off-host or filtered Sitemap: line leaves us with no sitemap at all.
   */
    if (ctx.accepted == 0 && st != NULL && !st->fallback_done) {
      st->fallback_done = HTS_TRUE;
      (void) sitemap_queue(opt, adr, "/sitemap.xml", 0, HTS_FALSE);
    }
    hts_log_print(opt, LOG_INFO,
                  "Sitemap: %d of %d sitemap(s) declared in %s%s", ctx.accepted,
                  n, adr, fil);
    return;
  }

  n = hts_sitemap_scan(body, size, HTS_SITEMAP_MAX_URLS_DOC, &ctx.is_index,
                       sitemap_seed_any, &ctx);
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
