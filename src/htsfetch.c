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
/* File: htsfetch.c subroutines:                               */
/*       Fetch Metadata request headers (Sec-Fetch-*)           */
/* ------------------------------------------------------------ */

#define HTS_INTERNAL_BYTECODE

#include "htsfetch.h"

#include "htscore.h"
#include "htspsl.h"
#include "htswizard.h"

static hts_boolean span_eq(const char *a, size_t alen, const char *b,
                           size_t blen) {
  return alen == blen && strncasecmp(a, b, alen) == 0 ? HTS_TRUE : HTS_FALSE;
}

/* Find attribute name in the start tag at tag_name. On a match, *value and
 *len hold its value, empty when it has none. Otherwise *end, if not NULL,
 gets the tag's end, which is its '>', an unquoted '<' or the NUL. */
static hts_boolean tag_attr_find_end(const char *tag_name, const char *name,
                                     const char **value, size_t *len,
                                     const char **end) {
  const size_t want = strlen(name);
  const char *p = tag_name;

  while (*p != '\0' && *p != '>' && *p != '<' && *p != '/' && !is_realspace(*p))
    p++;
  for (;;) {
    const char *n;
    size_t nlen;

    while (is_realspace(*p) || *p == '/')
      p++;
    if (*p == '\0' || *p == '>' || *p == '<') {
      if (end != NULL)
        *end = p;
      return HTS_FALSE;
    }
    for (n = p; *p != '\0' && *p != '>' && *p != '<' && *p != '=' &&
                !is_realspace(*p);)
      p++;
    nlen = (size_t) (p - n);
    while (is_realspace(*p))
      p++;
    *value = p;
    *len = 0;
    if (*p == '=') {
      p++;
      while (is_realspace(*p))
        p++;
      if (*p == '"' || *p == '\'') {
        const char quote = *p++;

        for (*value = p; *p != '\0' && *p != quote; p++)
          ;
        *len = (size_t) (p - *value);
        if (*p != '\0')
          p++;
      } else {
        for (*value = p;
             *p != '\0' && *p != '>' && *p != '<' && !is_realspace(*p); p++)
          ;
        *len = (size_t) (p - *value);
      }
    }
    if (nlen == want && strncasecmp(n, name, want) == 0)
      return HTS_TRUE;
  }
}

static hts_boolean tag_attr_find(const char *tag_name, const char *name,
                                 const char **value, size_t *len) {
  return tag_attr_find_end(tag_name, name, value, len, NULL);
}

/* Does the whitespace-separated list s, length len, hold token? */
static hts_boolean has_token(const char *s, size_t len, const char *token) {
  const size_t want = strlen(token);
  size_t i = 0;

  while (i < len) {
    size_t start;

    while (i < len && is_realspace(s[i]))
      i++;
    for (start = i; i < len && !is_realspace(s[i]); i++)
      ;
    if (i - start == want && strncasecmp(s + start, token, want) == 0)
      return HTS_TRUE;
  }
  return HTS_FALSE;
}

/* Is the value of attribute name in the start tag at tag_name exactly word? */
static hts_boolean tag_attr_is(const char *tag_name, const char *name,
                               const char *word) {
  const char *value;
  size_t len;

  return tag_attr_find(tag_name, name, &value, &len) && len == strlen(word) &&
                 strncasecmp(value, word, len) == 0
             ? HTS_TRUE
             : HTS_FALSE;
}

static hts_boolean tag_has_attr(const char *tag_name, const char *name) {
  const char *value;
  size_t len;

  return tag_attr_find(tag_name, name, &value, &len);
}

/* Is the extension of the path fil one of exts? */
static hts_boolean fil_ext_in(const char *fil, const char *const *exts) {
  const char *const end = fil + strcspn(fil, "?#");
  const char *dot = NULL;
  const char *p;
  size_t i;

  for (p = fil; p < end; p++) {
    if (*p == '.')
      dot = p + 1;
    else if (*p == '/')
      dot = NULL;
  }
  if (dot == NULL)
    return HTS_FALSE;
  for (i = 0; exts[i] != NULL; i++) {
    const size_t n = strlen(exts[i]);

    if ((size_t) (end - dot) == n && strncasecmp(dot, exts[i], n) == 0)
      return HTS_TRUE;
  }
  return HTS_FALSE;
}

/* The destination a <link> gets from its rel and as attributes. */
static hts_fetch_dest link_dest(const char *tag_name, hts_boolean *cors) {
  static const struct {
    const char *as;
    hts_fetch_dest dest;
  } preload[] = {{"style", HTS_FETCH_DEST_STYLE},
                 {"script", HTS_FETCH_DEST_SCRIPT},
                 {"image", HTS_FETCH_DEST_IMAGE},
                 {"font", HTS_FETCH_DEST_FONT}};

  const char *rel;
  size_t len, i;

  if (!tag_attr_find(tag_name, "rel", &rel, &len))
    return HTS_FETCH_DEST_UNKNOWN;
  if (has_token(rel, len, "stylesheet"))
    return HTS_FETCH_DEST_STYLE;
  if (has_token(rel, len, "icon"))
    return HTS_FETCH_DEST_IMAGE;
  if (has_token(rel, len, "modulepreload")) {
    *cors = HTS_TRUE;
    return HTS_FETCH_DEST_SCRIPT;
  }
  if (has_token(rel, len, "preload")) {
    for (i = 0; i < sizeof(preload) / sizeof(preload[0]); i++) {
      if (tag_attr_is(tag_name, "as", preload[i].as))
        return preload[i].dest;
    }
  }
  return HTS_FETCH_DEST_UNKNOWN;
}

int hts_fetch_same_frame(int page_dest) {
  switch (page_dest) {
  case HTS_FETCH_DEST_DOCUMENT:
  case HTS_FETCH_DEST_IFRAME:
  case HTS_FETCH_DEST_FRAME:
    return page_dest;
  default:
    return HTS_FETCH_DEST_UNKNOWN;
  }
}

hts_boolean hts_fetch_has_base_target(const char *html) {
  const char *p;

  /* Resume where each <base> ended, so an unclosed one is not rescanned. */
  for (p = strchr(html, '<'); p != NULL; p = strchr(p, '<')) {
    const char *value;
    size_t len;

    p++;
    if (hts_cmp_tag_token(p, "base") &&
        tag_attr_find_end(p, "target", &value, &len, &p))
      return HTS_TRUE;
  }
  return HTS_FALSE;
}

/* Return the destination a click on a link with this target loads. A named
   target is a frame we cannot tell. */
static int link_target_dest(const hts_fetch_tag *tag, int page_frame) {
  const char *const target = tag->target;
  const size_t len = tag->target_len;

  if (target == NULL || span_eq(target, len, "_self", 5))
    return page_frame;
  if (span_eq(target, len, "_top", 4) || span_eq(target, len, "_blank", 6))
    return HTS_FETCH_DEST_DOCUMENT;
  if (span_eq(target, len, "_parent", 7) &&
      page_frame == HTS_FETCH_DEST_DOCUMENT)
    return HTS_FETCH_DEST_DOCUMENT;
  return HTS_FETCH_DEST_UNKNOWN;
}

/* Read the start tag at name into *tag, unless *tag already holds it. */
static void fetch_tag_read(hts_fetch_tag *tag, const char *name) {
  const char *value;
  size_t len;

  if (tag->name == name)
    return;
  memset(tag, 0, sizeof(*tag));
  tag->name = name;
  if (hts_cmp_tag_token(name, "link"))
    tag->link_dest = (unsigned char) link_dest(name, &tag->link_cors);
  tag->download = tag_has_attr(name, "download");
  tag->crossorigin = tag_has_attr(name, "crossorigin");
  tag->module = tag_attr_is(name, "type", "module");
  if (tag_attr_find(name, "target", &value, &len)) {
    tag->target = value;
    tag->target_len = len;
  }
}

void hts_fetch_classify(hts_fetch_meta *fm, hts_fetch_tag *tag,
                        const char *tag_name, const char *attr, const char *fil,
                        int page_frame) {
  static const char *const font_exts[] = {"woff2", "woff", "ttf",
                                          "otf",   "eot",  NULL};
  /* No svg, which @font-face can name too. */
  static const char *const image_exts[] = {"png",  "jpg", "jpeg", "gif", "webp",
                                           "avif", "ico", "bmp",  NULL};
  static const char *const css_exts[] = {"css", NULL};

  /* (tag, attribute) pairs with a fixed destination; NULL matches any tag. */
  static const struct {
    const char *tag, *attr;
    hts_fetch_dest dest;
  } rules[] = {
      {"a", "href", HTS_FETCH_DEST_DOCUMENT},
      {"area", "href", HTS_FETCH_DEST_DOCUMENT},
      {"iframe", "src", HTS_FETCH_DEST_IFRAME},
      {"frame", "src", HTS_FETCH_DEST_FRAME},
      {"object", "data", HTS_FETCH_DEST_OBJECT},
      {"embed", "src", HTS_FETCH_DEST_EMBED},
      {"img", "src", HTS_FETCH_DEST_IMAGE},
      {"img", "srcset", HTS_FETCH_DEST_IMAGE},
      {"img", "data-src", HTS_FETCH_DEST_IMAGE},
      {"img", "data-srcset", HTS_FETCH_DEST_IMAGE},
      {"source", "srcset", HTS_FETCH_DEST_IMAGE},
      {"input", "src", HTS_FETCH_DEST_IMAGE},
      {"video", "poster", HTS_FETCH_DEST_IMAGE},
      {NULL, "background", HTS_FETCH_DEST_IMAGE},
      {"video", "src", HTS_FETCH_DEST_VIDEO},
      {"audio", "src", HTS_FETCH_DEST_AUDIO},
      {"script", "src", HTS_FETCH_DEST_SCRIPT},
  };

  size_t i;

  fm->dest = HTS_FETCH_DEST_UNKNOWN;
  fm->cors = HTS_FALSE;
  fm->user = HTS_FALSE;
  if (attr == NULL)
    return;
  if (tag_name == NULL) {
    /* url() and @import look alike to the parser, so the path decides. */
    if (fil_ext_in(fil, css_exts))
      fm->dest = HTS_FETCH_DEST_STYLE;
    else if (fil_ext_in(fil, font_exts))
      fm->dest = HTS_FETCH_DEST_FONT;
    else if (fil_ext_in(fil, image_exts))
      fm->dest = HTS_FETCH_DEST_IMAGE;
    return;
  }
  fetch_tag_read(tag, tag_name);
  if (hts_cmp_tag_token(tag->name, "link") && hts_cmp_tag_token(attr, "href")) {
    fm->dest = tag->link_dest;
    fm->cors = tag->link_cors;
  } else {
    for (i = 0; i < sizeof(rules) / sizeof(rules[0]); i++) {
      if ((rules[i].tag == NULL ||
           hts_cmp_tag_token(tag->name, rules[i].tag)) &&
          strncasecmp(attr, rules[i].attr, strlen(rules[i].attr)) == 0 &&
          !isalnum((unsigned char) attr[strlen(rules[i].attr)]) &&
          attr[strlen(rules[i].attr)] != '-') {
        fm->dest = (unsigned char) rules[i].dest;
        break;
      }
    }
  }
  /* A download is no navigation, and its destination is not one we send. */
  if (fm->dest == HTS_FETCH_DEST_DOCUMENT && tag->download)
    fm->dest = HTS_FETCH_DEST_UNKNOWN;
  if (fm->dest == HTS_FETCH_DEST_DOCUMENT) {
    fm->dest = (unsigned char) link_target_dest(tag, page_frame);
    fm->user = HTS_TRUE;
  }
  if (tag->crossorigin || (fm->dest == HTS_FETCH_DEST_SCRIPT && tag->module))
    fm->cors = HTS_TRUE;
}

/* An origin splits into scheme, host and port. */
typedef struct fetch_origin {
  hts_boolean https;
  const char *host;
  size_t host_len;
  int port;
} fetch_origin;

static void origin_of(const char *adr, fetch_origin *o) {
  const char *p = jump_protocol_const(adr);
  const char *at, *end;

  o->https = strfield(adr, "https:") ? HTS_TRUE : HTS_FALSE;
  end = p + strcspn(p, "/?#");
  /* The last '@' ends the userinfo. */
  at = strrchr_limit(p, '@', end);
  if (at != NULL)
    p = at;
  o->host = p;
  if (*p == '[') {
    const char *const close = memchr(p, ']', (size_t) (end - p));

    p = close != NULL ? close + 1 : end;
  } else {
    while (p < end && *p != ':')
      p++;
  }
  o->host_len = (size_t) (p - o->host);
  o->port = o->https ? 443 : 80;
  if (p < end && *p == ':' && p + 1 < end)
    o->port = atoi(p + 1);
}

/* The registrable domain of the first n bytes of the host, extended to its end,
   or the whole host when it is a public suffix or too long. NULL when the list
   did not load. */
static const char *psl_site_key(const fetch_origin *o, size_t n, size_t *len) {
  char host[256];
  const char *reg;

  if (!hts_psl_loaded())
    return NULL;
  if (n >= sizeof(host))
    return o->host;
  memcpy(host, o->host, n);
  host[n] = '\0';
  hts_lowcase(host);
  reg = hts_psl_registrable_domain(host);
  if (reg == NULL)
    reg = host;
  *len = o->host_len - (size_t) (reg - host);
  return o->host + (reg - host);
}

/* The tail of the host same-site compares: the registrable domain, else the
   last two labels, or the whole host for an IP literal. */
static const char *site_key(const fetch_origin *o, size_t *len) {
  size_t n = o->host_len, i, dots = 0;

  *len = o->host_len;
  if (hts_host_is_address(o->host, o->host_len))
    return o->host;
  /* "a.example.com." keys on "example.com.", which the list cannot read. */
  if (n > 1 && o->host[n - 1] == '.')
    n--;
  /* An empty label leaves no domain, so only the same host matches. */
  for (i = 0; i < n; i++) {
    if (o->host[i] == '.' && (i == 0 || i + 1 == n || o->host[i + 1] == '.'))
      return o->host;
  }
  /* A browser reads "0x7f.0.0.1" as IPv4, and no TLD starts with a digit. */
  for (i = n; i > 0 && o->host[i - 1] != '.'; i--)
    ;
  if (i < n && isdigit((unsigned char) o->host[i]))
    return o->host;
  {
    const char *const key = psl_site_key(o, n, len);

    if (key != NULL)
      return key;
  }
  i = n;
  while (i > 0) {
    if (o->host[i - 1] == '.' && ++dots == 2)
      break;
    i--;
  }
  *len = o->host_len - i;
  return o->host + i;
}

hts_fetch_site hts_fetch_site_of(const char *from_adr, const char *to_adr) {
  fetch_origin from, to;
  const char *fk, *tk;
  size_t fl, tl;

  origin_of(from_adr, &from);
  origin_of(to_adr, &to);
  if (from.https != to.https)
    return HTS_FETCH_SITE_CROSS_SITE;
  if (span_eq(from.host, from.host_len, to.host, to.host_len))
    return from.port == to.port ? HTS_FETCH_SITE_SAME_ORIGIN
                                : HTS_FETCH_SITE_SAME_SITE;
  fk = site_key(&from, &fl);
  tk = site_key(&to, &tl);
  return span_eq(fk, fl, tk, tl) ? HTS_FETCH_SITE_SAME_SITE
                                 : HTS_FETCH_SITE_CROSS_SITE;
}

void hts_fetch_through(httrackp *opt, hts_fetch_meta *fm, const char *adr) {
  hts_fetch_site site;

  if (fm->dest == HTS_FETCH_DEST_UNKNOWN || fm->site == HTS_FETCH_SITE_NONE)
    return;
  /* No page to compare with: send nothing rather than a guess. */
  if (fm->initiator <= 0 || fm->initiator >= opt->lien_tot) {
    fm->dest = HTS_FETCH_DEST_UNKNOWN;
    return;
  }
  site = hts_fetch_site_of(heap(fm->initiator)->adr, adr);
  if (site > fm->site)
    fm->site = (unsigned char) site;
}

hts_boolean hts_fetch_trustworthy(const char *adr) {
  fetch_origin o;
  const char *dot;

  if (strfield(adr, "ftp:"))
    return HTS_FALSE;
  origin_of(adr, &o);
  if (o.https)
    return HTS_TRUE;
  if (span_eq(o.host, o.host_len, "localhost", 9) ||
      span_eq(o.host, o.host_len, "[::1]", 5))
    return HTS_TRUE;
  if (o.host_len > 10 &&
      span_eq(o.host + o.host_len - 10, 10, ".localhost", 10))
    return HTS_TRUE;
  dot = memchr(o.host, '.', o.host_len);
  return dot != NULL && span_eq(o.host, (size_t) (dot - o.host), "127", 3) &&
                 hts_host_is_ipv4(o.host, o.host_len)
             ? HTS_TRUE
             : HTS_FALSE;
}

hts_boolean hts_fetch_is_navigation(int dest) {
  return dest >= HTS_FETCH_DEST_DOCUMENT && dest <= HTS_FETCH_DEST_EMBED
             ? HTS_TRUE
             : HTS_FALSE;
}

const char *hts_fetch_dest_name(int dest) {
  static const char *const names[] = {"empty",  "document", "iframe", "frame",
                                      "object", "embed",    "image",  "script",
                                      "style",  "font",     "audio",  "video"};

  return dest >= 0 && (size_t) dest < sizeof(names) / sizeof(names[0])
             ? names[dest]
             : "empty";
}

const char *hts_fetch_mode_name(const hts_fetch_meta *fm) {
  if (hts_fetch_is_navigation(fm->dest))
    return "navigate";
  return fm->cors || fm->dest == HTS_FETCH_DEST_FONT ? "cors" : "no-cors";
}

const char *hts_fetch_site_name(int site) {
  switch (site) {
  case HTS_FETCH_SITE_NONE:
    return "none";
  case HTS_FETCH_SITE_SAME_SITE:
    return "same-site";
  case HTS_FETCH_SITE_CROSS_SITE:
    return "cross-site";
  default:
    return "same-origin";
  }
}

hts_boolean hts_fetch_origin(const char *adr, char *dst, size_t size) {
  fetch_origin o;
  size_t i;
  int n;

  origin_of(adr, &o);
  if (o.host_len == 0)
    return HTS_FALSE;
  for (i = 0; i < o.host_len; i++) {
    if ((unsigned char) o.host[i] <= ' ' || (unsigned char) o.host[i] >= 0x7f)
      return HTS_FALSE;
  }
  if (o.port == (o.https ? 443 : 80))
    n = snprintf(dst, size, "%s://%.*s", o.https ? "https" : "http",
                 (int) o.host_len, o.host);
  else
    n = snprintf(dst, size, "%s://%.*s:%d", o.https ? "https" : "http",
                 (int) o.host_len, o.host, o.port);
  return n > 0 && (size_t) n < size ? HTS_TRUE : HTS_FALSE;
}

/* hts_accept_kinds() publishes the --accept kinds in this display order. */
static const char *const accept_kinds[] = {
    "document", "iframe", "frame", "image",  "style", "script",
    "font",     "audio",  "video", "object", "embed", NULL};

enum { ACCEPT_KINDS = sizeof(accept_kinds) / sizeof(accept_kinds[0]) - 1 };

#define CHROME_NAV                                                             \
  "text/html,application/xhtml+xml,application/xml;q=0.9,image/jxl,image/"     \
  "avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q="   \
  "0.7"
#define FIREFOX_NAV                                                            \
  "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"
#define BROWSER_STYLE "text/css,*/*;q=0.1"

/* Chrome 155 and Firefox 158 send these, indexed by family - 1 and
   accept_kinds entry. embed was never captured, so it follows object. */
static const char *const accept_presets[][ACCEPT_KINDS] = {
    /* HTS_BROWSER_CHROME */
    {CHROME_NAV, CHROME_NAV, CHROME_NAV,
     "image/jxl,image/avif,image/webp,image/apng,image/svg+xml,image/*,*/"
     "*;q=0.8",
     BROWSER_STYLE, "*/*", "*/*", "*/*", "*/*", CHROME_NAV, CHROME_NAV},
    /* HTS_BROWSER_FIREFOX */
    {FIREFOX_NAV, FIREFOX_NAV, FIREFOX_NAV,
     "image/avif,image/jxl,image/webp,image/png,image/svg+xml,image/*;q=0.8,*/"
     "*;q=0.5",
     BROWSER_STYLE, "*/*",
     "application/font-woff2;q=1.0,application/font-woff;q=0.9,*/*;q=0.8",
     "audio/webm,audio/ogg,audio/wav,audio/*;q=0.9,application/ogg;q=0.7,"
     "video/*;q=0.6,*/*;q=0.5",
     "video/webm,video/ogg,video/*;q=0.9,application/ogg;q=0.7,audio/*;q=0.6,"
     "*/*;q=0.5",
     "*/*", "*/*"}};

/* Return the accept_kinds index of the n bytes at name, or -1. */
static int accept_kind_index(const char *name, size_t n) {
  int i;

  for (i = 0; i < ACCEPT_KINDS; i++) {
    if (strlen(accept_kinds[i]) == n && strncmp(accept_kinds[i], name, n) == 0)
      return i;
  }
  return -1;
}

/* Return the accept_kinds index of dest, or -1 for an unknown one. */
static int accept_kind_of_dest(int dest) {
  const char *const name = hts_fetch_dest_name(dest);

  return dest == HTS_FETCH_DEST_UNKNOWN ? -1
                                        : accept_kind_index(name, strlen(name));
}

int hts_accept_kind_of(const char *arg) {
  const char *const eq = strchr(arg, '=');

  return eq != NULL ? accept_kind_index(arg, (size_t) (eq - arg)) : -1;
}

/* Copy the value of the last list line for kind into buf, or return NULL. */
static const char *accept_list_find(const char *list, int kind, char *buf,
                                    size_t size) {
  const char *found = NULL;
  size_t found_len = 0;
  const char *line;

  for (line = list; line != NULL && *line != '\0';) {
    const char *const end = line + strcspn(line, "\n");
    const char *const eq = memchr(line, '=', (size_t) (end - line));

    if (eq != NULL && accept_kind_index(line, (size_t) (eq - line)) == kind) {
      found = eq + 1;
      found_len = (size_t) (end - found);
    }
    line = *end != '\0' ? end + 1 : end;
  }
  if (found == NULL || found_len >= size)
    return NULL;
  memcpy(buf, found, found_len);
  buf[found_len] = '\0';
  return buf;
}

const char *hts_accept_lookup(const char *list, const char *user_default,
                              hts_boolean user_given, int dest,
                              hts_browser_family family, char *buf,
                              size_t size) {
  const int kind = accept_kind_of_dest(dest);
  const char *v;

  if (kind >= 0) {
    if ((v = accept_list_find(list, kind, buf, size)) != NULL)
      return v;
    if ((dest == HTS_FETCH_DEST_IFRAME || dest == HTS_FETCH_DEST_FRAME) &&
        (v = accept_list_find(list,
                              accept_kind_of_dest(HTS_FETCH_DEST_DOCUMENT), buf,
                              size)) != NULL)
      return v;
  }
  if (!user_given && kind >= 0 && family != HTS_BROWSER_NONE)
    return accept_presets[family - 1][kind];
  return user_default;
}

HTSEXT_API const char *const *hts_accept_kinds(void) { return accept_kinds; }

HTSEXT_API const char *hts_accept_preset(const char *browser,
                                         const char *kind) {
  const int k = kind != NULL && *kind != '\0'
                    ? accept_kind_index(kind, strlen(kind))
                    : -1;

  if (browser == NULL || (kind != NULL && *kind != '\0' && k < 0))
    return NULL;
  if (strcmp(browser, "legacy") == 0)
    return HTS_ACCEPT_LEGACY;
  if (k < 0)
    return NULL;
  if (strcmp(browser, "chrome") == 0)
    return accept_presets[HTS_BROWSER_CHROME - 1][k];
  if (strcmp(browser, "firefox") == 0)
    return accept_presets[HTS_BROWSER_FIREFOX - 1][k];
  return NULL;
}
