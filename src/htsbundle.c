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
/* File: htsbundle.c subroutines:                               */
/*       login bundles named by a start URL                     */
/* ------------------------------------------------------------ */

#define HTS_INTERNAL_BYTECODE

#include "htsbundle.h"

#include "htscore.h"
#include "htslib.h"
#include "htslines.h"
#include "htspsl.h"
#include "htssafe.h"

#include <string.h>

/* A bundle line, the jar aside, holds fewer bytes than this. */
#define BUNDLE_LINE_MAX 8192

/* A bundle holds at most this many url lines, and as many scope entries. */
#define BUNDLE_LIST_MAX 256

/* A bundle's header lines, with their CRLFs, fit in this many bytes, so the
   request still has room for the rest. */
#define BUNDLE_HEADERS_MAX 8192

typedef struct bundle {
  char name[HTS_BUNDLE_NAME_MAX];
  char file[HTS_URLMAXSIZE * 2];
  char **start_urls; /* the start URLs naming it */
  size_t nstart;
  char **urls; /* its url lines */
  size_t nurls;
  /* lowercase domains, each covering its subdomains, or "=host" for a host
     alone */
  char **scope;
  size_t nscope;
  char *ua; /* NULL when it gives none */
  String headers;
  hts_boolean https_only; /* Are its headers for https alone? */
} bundle;

typedef struct hts_bundle_state {
  bundle *items;
  size_t count;
} hts_bundle_state;

static hts_boolean strlist_add(char ***list, size_t *count, const char *s) {
  char **const grown = realloct(*list, (*count + 1) * sizeof(char *));

  if (grown == NULL)
    return HTS_FALSE;
  *list = grown;
  grown[*count] = strdupt(s);
  if (grown[*count] == NULL)
    return HTS_FALSE;
  (*count)++;
  return HTS_TRUE;
}

static void strlist_free(char ***list, size_t *count) {
  size_t i;

  for (i = 0; i < *count; i++)
    freet((*list)[i]);
  freet(*list);
  *count = 0;
}

/* Forget what reading b's file gave. */
static void bundle_clear(bundle *b) {
  strlist_free(&b->urls, &b->nurls);
  strlist_free(&b->scope, &b->nscope);
  freet(b->ua);
  StringClear(b->headers);
  b->https_only = HTS_FALSE;
}

/* Is name a Windows device name, such as CON, aux.txt or com1? */
static hts_boolean bundle_name_is_device(const char *name) {
  static const char *const devices[] = {"con", "prn", "aux", "nul"};
  const size_t base = strcspn(name, ".");
  char dev[5];
  size_t i;

  if (base < 3 || base > 4)
    return HTS_FALSE;
  dev[0] = '\0';
  strlncatbuff(dev, name, sizeof(dev), base);
  hts_lowcase(dev);
  if (base == 4)
    return (strncmp(dev, "com", 3) == 0 || strncmp(dev, "lpt", 3) == 0) &&
           dev[3] >= '1' && dev[3] <= '9';
  for (i = 0; i < sizeof(devices) / sizeof(devices[0]); i++) {
    if (strcmp(dev, devices[i]) == 0)
      return HTS_TRUE;
  }
  return HTS_FALSE;
}

/* Why name is not a bundle name, or NULL when it is one. */
static const char *bundle_name_refusal(const char *name) {
  const size_t len = strlen(name);

  if (len >= HTS_BUNDLE_NAME_MAX)
    return "invalid login bundle name: 128 bytes or more";
  if (len == 0 ||
      strspn(name, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
                   "0123456789._-") != len)
    return "invalid login bundle name: use letters, digits, '.', '_' and '-'";
  if (name[0] == '.' || strstr(name, "..") != NULL)
    return "invalid login bundle name: it starts with '.' or holds \"..\"";
  if (bundle_name_is_device(name))
    return "invalid login bundle name: a Windows device name";
  return NULL;
}

const char *hts_bundle_cut(char *url, char *name, size_t name_size) {
  char *const at = strstr(url, HTS_BUNDLE_SUFFIX);
  const char *why;

  name[0] = '\0';
  if (at == NULL)
    return NULL;
  why = bundle_name_refusal(at + strlen(HTS_BUNDLE_SUFFIX));
  if (why != NULL)
    return why;
  strlcpybuff(name, at + strlen(HTS_BUNDLE_SUFFIX), name_size);
  *at = '\0';
  return NULL;
}

void hts_bundle_free(httrackp *opt) {
  hts_bundle_state *const st = (hts_bundle_state *) opt->bundle_state;
  size_t i;

  if (st == NULL)
    return;
  for (i = 0; i < st->count; i++) {
    bundle_clear(&st->items[i]);
    strlist_free(&st->items[i].start_urls, &st->items[i].nstart);
    StringFree(st->items[i].headers);
  }
  freet(st->items);
  freet(opt->bundle_state);
}

/* Record that the start URL url names bundle name. HTS_FALSE when out of
   memory. */
static hts_boolean bundle_add(httrackp *opt, const char *name,
                              const char *url) {
  hts_bundle_state *st = (hts_bundle_state *) opt->bundle_state;
  bundle *b = NULL;
  size_t i;

  if (st == NULL) {
    st = calloct(1, sizeof(*st));
    if (st == NULL)
      return HTS_FALSE;
    opt->bundle_state = st;
  }
  for (i = 0; i < st->count && b == NULL; i++) {
    if (strcmp(st->items[i].name, name) == 0)
      b = &st->items[i];
  }
  if (b == NULL) {
    bundle *const grown = realloct(st->items, (st->count + 1) * sizeof(bundle));

    if (grown == NULL)
      return HTS_FALSE;
    st->items = grown;
    b = &grown[st->count++];
    memset(b, 0, sizeof(*b));
    StringInit(b->headers);
    strcpybuff(b->name, name);
  }
  return strlist_add(&b->start_urls, &b->nstart, url);
}

const char *hts_bundle_take(httrackp *opt, char *url) {
  char name[HTS_BUNDLE_NAME_MAX];
  const char *const why = hts_bundle_cut(url, name, sizeof(name));

  if (why != NULL || name[0] == '\0')
    return why;
  return bundle_add(opt, name, url) ? NULL : "out of memory";
}

hts_boolean hts_bundle_has_control(const char *s, size_t len) {
  size_t i;

  for (i = 0; i < len; i++) {
    const unsigned char c = (unsigned char) s[i];

    if ((c < 0x20 && c != '\t') || c == 0x7f)
      return HTS_TRUE;
  }
  return HTS_FALSE;
}

/* Read a line into line without its line end. NULL, or why the line is
   refused: too long, or with a control byte other than TAB, because a CR
   would end a header early. */
static const char *bundle_readline(FILE *fp, char *line, size_t size) {
  size_t len;

  if (hts_readline_raw(fp, line, size, &len))
    return "too long";
  if (len > 0 && line[len - 1] == '\r')
    line[--len] = '\0';
  return hts_bundle_has_control(line, len) ? "holds a control byte" : NULL;
}

/* The major version a "# HTTrack bundle N[.M]" line gives, or -1 when line is
   not one. */
static int bundle_version_of(const char *line) {
  static const char magic[] = "# HTTrack bundle ";
  const char *p = line;
  size_t digits;
  int major;

  if (strncmp(p, magic, sizeof(magic) - 1) != 0)
    return -1;
  p += sizeof(magic) - 1;
  digits = strspn(p, "0123456789");
  if (digits == 0 || digits > 3 || (p[0] == '0' && digits > 1))
    return -1;
  major = atoi(p);
  p += digits;
  if (*p == '.') {
    digits = strspn(++p, "0123456789");
    if (digits == 0)
      return -1;
    p += digits;
  }
  return *p == '\0' ? major : -1;
}

/* Does the scope entry cover host? */
static hts_boolean scope_covers(const char *entry, const char *host) {
  return entry[0] == '=' ? strcmp(entry + 1, host) == 0
                         : cookie_domain_match(entry, host);
}

static hts_boolean bundle_in_scope(const bundle *b, const char *host) {
  size_t i;

  for (i = 0; i < b->nscope; i++) {
    if (scope_covers(b->scope[i], host))
      return HTS_TRUE;
  }
  return HTS_FALSE;
}

/* Add entry to b's scope. NULL, or why it could not. */
static const char *bundle_scope_add(bundle *b, const char *entry) {
  size_t i;

  for (i = 0; i < b->nscope; i++) {
    if (strcmp(b->scope[i], entry) == 0)
      return NULL;
  }
  if (b->nscope >= BUNDLE_LIST_MAX)
    return "more than 256 scope domains";
  return strlist_add(&b->scope, &b->nscope, entry) ? NULL : "out of memory";
}

/* Note that url names b. An https one makes b's headers https-only, and
   unless scope lines replace it, the URL's registrable domain joins the scope,
   or its host alone when it has none: a public suffix, a single label or an
   address. NULL, or why it could not. */
static const char *bundle_note_url(bundle *b, const char *url,
                                   hts_boolean scope_given) {
  char host[256];
  char entry[sizeof(host) + 1];
  const char *reg;

  if (strfield(url, "https:"))
    b->https_only = HTS_TRUE;
  if (scope_given)
    return NULL;
  if (!cookie_url_host(url, host, sizeof(host)))
    return "cannot work out the bundle's scope";
  reg = hts_host_is_address(host, strlen(host))
            ? NULL
            : hts_psl_registrable_domain(host);
  snprintf(entry, sizeof(entry), "%s%s", reg != NULL ? "" : "=",
           reg != NULL ? reg : host);
  return bundle_scope_add(b, entry);
}

/* Write the reason into err and return HTS_FALSE. The caller needs err and
   err_size. */
#define BUNDLE_FAIL(...)                                                       \
  do {                                                                         \
    snprintf(err, err_size, __VA_ARGS__);                                      \
    return HTS_FALSE;                                                          \
  } while (0)

/* Add a scope line's domain to b's scope. */
static hts_boolean bundle_scope_line(bundle *b, const char *value, int lineno,
                                     char *err, size_t err_size) {
  char dom[256];
  char entry[sizeof(dom) + 1];
  const char *why;

  if (value[0] == '.')
    value++;
  if (value[0] == '\0' || strlen(value) >= sizeof(dom) ||
      strspn(value, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
                    "0123456789.-") != strlen(value))
    BUNDLE_FAIL("%s, line %d: invalid scope", b->file, lineno);
  strcpybuff(dom, value);
  hts_lowcase(dom);
  if (hts_host_is_address(dom, strlen(dom)))
    snprintf(entry, sizeof(entry), "=%s", dom);
  else if (hts_psl_registrable_domain(dom) != NULL)
    strcpybuff(entry, dom);
  else
    BUNDLE_FAIL("%s, line %d: the scope is a public suffix", b->file, lineno);
  why = bundle_scope_add(b, entry);
  if (why != NULL)
    BUNDLE_FAIL("%s, line %d: %s", b->file, lineno, why);
  return HTS_TRUE;
}

/* Is the klen bytes at line the key key? */
static hts_boolean key_is(const char *line, size_t klen, const char *key) {
  return klen == strlen(key) && strncmp(line, key, klen) == 0;
}

/* Read b's lines up to "cookies", leaving fp at its jar. *cookies_line gets
   the number of that line, or 0 when b has no cookies. */
static hts_boolean bundle_parse(httrackp *opt, bundle *b, FILE *fp,
                                hts_boolean *scope_given, int *cookies_line,
                                char *err, size_t err_size) {
  static const char bom[] = "\xEF\xBB\xBF";
  char BIGSTK line[BUNDLE_LINE_MAX];
  int lineno = 1;
  int version;
  const char *why = bundle_readline(fp, line, sizeof(line));

  if (why != NULL)
    BUNDLE_FAIL("%s, line 1: %s", b->file, why);
  version = bundle_version_of(
      strncmp(line, bom, sizeof(bom) - 1) == 0 ? line + sizeof(bom) - 1 : line);
  if (version < 0)
    BUNDLE_FAIL("%s: not an HTTrack bundle, the first line must be "
                "\"# HTTrack bundle 1\"",
                b->file);
  if (version != 1)
    BUNDLE_FAIL("%s: bundle version %d is not supported", b->file, version);
  *scope_given = HTS_FALSE;
  *cookies_line = 0;
  while (!feof(fp)) {
    const char *value;
    size_t klen;

    lineno++;
    why = bundle_readline(fp, line, sizeof(line));
    if (why != NULL)
      BUNDLE_FAIL("%s, line %d: %s", b->file, lineno, why);
    if (line[0] == '\0' || line[0] == '#')
      continue;
    klen = strcspn(line, " \t");
    value = line + klen;
    value += strspn(value, " \t");
    if (key_is(line, klen, "cookies")) {
      if (*value != '\0')
        BUNDLE_FAIL("%s, line %d: \"cookies\" takes no value", b->file, lineno);
      *cookies_line = lineno;
      return HTS_TRUE;
    }
    if (!key_is(line, klen, "url") && !key_is(line, klen, "ua") &&
        !key_is(line, klen, "header") && !key_is(line, klen, "scope")) {
      /* a key in lowercase letters is no secret, so it alone is named */
      const hts_boolean shown =
          klen <= 32 && strspn(line, "abcdefghijklmnopqrstuvwxyz") == klen;

      hts_log_print(opt, LOG_WARNING, "Bundle %s, line %d: unknown key%s%.*s",
                    b->file, lineno, shown ? " " : "", shown ? (int) klen : 0,
                    line);
      continue;
    }
    if (*value == '\0')
      BUNDLE_FAIL("%s, line %d: no value", b->file, lineno);
    if (key_is(line, klen, "url")) {
      char host[256];

      if (strlen(value) >= HTS_URLMAXSIZE ||
          strstr(value, HTS_BUNDLE_SUFFIX) != NULL ||
          !cookie_url_host(value, host, sizeof(host)))
        BUNDLE_FAIL("%s, line %d: invalid url", b->file, lineno);
      if (b->nurls >= BUNDLE_LIST_MAX)
        BUNDLE_FAIL("%s, line %d: more than %d url lines", b->file, lineno,
                    BUNDLE_LIST_MAX);
      if (!strlist_add(&b->urls, &b->nurls, value))
        BUNDLE_FAIL("%s: out of memory", b->file);
    } else if (key_is(line, klen, "ua")) {
      if (b->ua != NULL)
        BUNDLE_FAIL("%s, line %d: ua given twice", b->file, lineno);
      b->ua = strdupt(value);
      if (b->ua == NULL)
        BUNDLE_FAIL("%s: out of memory", b->file);
    } else if (key_is(line, klen, "header")) {
      why = http_bundle_header_refusal(StringBuff(b->headers), value);
      if (why != NULL)
        BUNDLE_FAIL("%s, line %d: refused header: %s", b->file, lineno, why);
      if (strlen(value) + 2 > BUNDLE_HEADERS_MAX - StringLength(b->headers))
        BUNDLE_FAIL("%s, line %d: the header lines pass %d bytes", b->file,
                    lineno, BUNDLE_HEADERS_MAX);
      StringCat(b->headers, value);
      StringCat(b->headers, "\r\n");
    } else {
      *scope_given = HTS_TRUE;
      if (!bundle_scope_line(b, value, lineno, err, err_size))
        return HTS_FALSE;
    }
  }
  return HTS_TRUE;
}

/* Is jar_file newer than b's file, so its records win? */
static hts_boolean bundle_jar_wins(const bundle *b, const char *jar_file) {
  hts_filetime_t jt, bt;

  return jar_file != NULL && hts_file_mtime(jar_file, &jt) &&
         hts_file_mtime(b->file, &bt) &&
         (jt.sec > bt.sec || (jt.sec == bt.sec && jt.nsec > bt.nsec));
}

/* What bundle_admit needs about the bundle being read. */
typedef struct bundle_jar {
  const bundle *b;
  t_cookie *all_bundles; /* every bundle's records so far */
  hts_boolean jar_wins;  /* A record the jar holds already wins. */
  int dropped;           /* records outside the scope */
} bundle_jar;

/* The cookie_filter admit callback: keep a record in the bundle's scope, and
   stop on one another bundle gives another value. */
static hts_boolean bundle_admit(cookie_filter *filter, t_cookie *cookie,
                                hts_boolean repeat, const char *name,
                                const char *value, const char *domain,
                                const char *path) {
  bundle_jar *const ctx = (bundle_jar *) filter->ctx;
  char buffer[8192];
  char host[256];
  const char *other;

  if (!cookie_host(domain, host, sizeof(host)) ||
      !bundle_in_scope(ctx->b, host[0] == '.' ? host + 1 : host)) {
    ctx->dropped++;
    return HTS_FALSE;
  }
  other = cookie_find_exact(ctx->all_bundles->data, name, host, path);
  if (other != NULL && !repeat &&
      strcmp(cookie_get(buffer, other, 6), value) != 0) {
    filter->why = "a cookie has another value in another bundle";
    return HTS_FALSE;
  }
  if (cookie_set(ctx->all_bundles, name, value, domain, path, NULL) != 0) {
    filter->why = "too many cookies to compare with the other bundles";
    return HTS_FALSE;
  }
  return !ctx->jar_wins ||
         cookie_find_exact(cookie->data, name, host, path) == NULL;
}

/* Read b's file, set its scope, and load its cookies, comparing them with
   all_bundles. */
static hts_boolean bundle_load(httrackp *opt, bundle *b, const char *jar_file,
                               t_cookie *all_bundles, char *err,
                               size_t err_size) {
  const char *const dir = StringNotEmpty(opt->bundle_dir)
                              ? StringBuff(opt->bundle_dir)
                              : StringBuff(opt->path_log);
  const size_t dlen = strlen(dir);
  hts_boolean scope_given;
  int cookies_line;
  bundle_jar ctx;
  cookie_filter filter;
  const char *why = NULL;
  size_t i;
  FILE *fp;

  bundle_clear(b);
  if (!slprintfbuff(
          b->file, sizeof(b->file), "%s%s%s.hts", dir,
          dlen != 0 && dir[dlen - 1] != '/' && dir[dlen - 1] != '\\' ? "/" : "",
          b->name))
    BUNDLE_FAIL("bundle %s: the folder path is too long", b->name);
  fp = FOPEN(b->file, "rb");
  if (fp == NULL)
    BUNDLE_FAIL("%s: cannot read the login bundle: %s", b->file,
                strerror(errno));
  if (!bundle_parse(opt, b, fp, &scope_given, &cookies_line, err, err_size)) {
    fclose(fp);
    return HTS_FALSE;
  }
  for (i = 0; i < b->nstart && why == NULL; i++)
    why = bundle_note_url(b, b->start_urls[i], scope_given);
  for (i = 0; i < b->nurls && why == NULL; i++)
    why = bundle_note_url(b, b->urls[i], scope_given);
  if (why != NULL) {
    fclose(fp);
    BUNDLE_FAIL("%s: %s", b->file, why);
  }
  memset(&ctx, 0, sizeof(ctx));
  ctx.b = b;
  ctx.all_bundles = all_bundles;
  ctx.jar_wins = bundle_jar_wins(b, jar_file);
  memset(&filter, 0, sizeof(filter));
  filter.admit = bundle_admit;
  filter.ctx = &ctx;
  filter.line = cookies_line;
  if (cookies_line != 0 && opt->cookie != NULL)
    cookie_load_stream(opt, opt->cookie, fp, b->file, &filter);
  else if (cookies_line != 0)
    hts_log_print(opt, LOG_WARNING,
                  "Cookies are disabled, so the cookies of %s are not sent",
                  b->file);
  fclose(fp);
  if (ctx.dropped != 0)
    hts_log_print(opt, LOG_WARNING,
                  "Bundle %s: %d cookie(s) outside its scope left out; a "
                  "scope line keeps them",
                  b->file, ctx.dropped);
  if (filter.why != NULL)
    BUNDLE_FAIL("%s, line %d: %s", b->file, filter.line, filter.why);
  return HTS_TRUE;
}

static hts_boolean bundle_scopes_overlap(const bundle *a, const bundle *b) {
  size_t i, j;

  for (i = 0; i < a->nscope; i++) {
    for (j = 0; j < b->nscope; j++) {
      const char *const ea = a->scope[i];
      const char *const eb = b->scope[j];

      if (scope_covers(ea, eb + (eb[0] == '=')) ||
          scope_covers(eb, ea + (ea[0] == '=')))
        return HTS_TRUE;
    }
  }
  return HTS_FALSE;
}

static hts_boolean bundle_same_identity(const bundle *a, const bundle *b) {
  return (a->ua == NULL ? b->ua == NULL
                        : b->ua != NULL && strcmp(a->ua, b->ua) == 0) &&
         strcmp(StringBuff(a->headers), StringBuff(b->headers)) == 0;
}

/* Bundles that overlap have merged, so an https URL in any of them makes the
   headers of all of them https-only. */
static void bundle_merge_https(hts_bundle_state *st) {
  hts_boolean changed = HTS_TRUE;
  size_t i, j;

  while (changed) {
    changed = HTS_FALSE;
    for (i = 0; i < st->count; i++) {
      for (j = 0; j < st->count; j++) {
        bundle *const a = &st->items[i];
        const bundle *const b = &st->items[j];

        if (!a->https_only && b->https_only && bundle_scopes_overlap(a, b)) {
          a->https_only = HTS_TRUE;
          changed = HTS_TRUE;
        }
      }
    }
  }
}

hts_boolean hts_bundle_load_all(httrackp *opt, const char *jar_file, char *err,
                                size_t err_size) {
  hts_bundle_state *const st = (hts_bundle_state *) opt->bundle_state;
  t_cookie *all_bundles;
  hts_boolean ok = HTS_TRUE;
  size_t i, j;

  if (st == NULL)
    return HTS_TRUE;
  all_bundles = calloct(1, sizeof(t_cookie));
  if (all_bundles == NULL)
    BUNDLE_FAIL("login bundles: out of memory");
  all_bundles->max_len = sizeof(all_bundles->data);
  for (i = 0; i < st->count && ok; i++)
    ok = bundle_load(opt, &st->items[i], jar_file, all_bundles, err, err_size);
  for (i = 0; i < st->count && ok; i++) {
    for (j = i + 1; j < st->count && ok; j++) {
      const bundle *const a = &st->items[i];
      const bundle *const b = &st->items[j];

      if (bundle_scopes_overlap(a, b) && !bundle_same_identity(a, b)) {
        snprintf(err, err_size,
                 "%s and %s cover the same site with a different user agent "
                 "or headers",
                 a->file, b->file);
        ok = HTS_FALSE;
      }
    }
  }
  if (ok)
    bundle_merge_https(st);
  freet(all_bundles);
  return ok;
}

const char *hts_bundle_url(httrackp *opt, size_t i) {
  const hts_bundle_state *const st = (hts_bundle_state *) opt->bundle_state;
  size_t k;

  for (k = 0; st != NULL && k < st->count; k++) {
    if (i < st->items[k].nurls)
      return st->items[k].urls[i];
    i -= st->items[k].nurls;
  }
  return NULL;
}

void hts_bundle_identity(httrackp *opt, const char *adr, htsrequest *req) {
  const hts_bundle_state *const st = (hts_bundle_state *) opt->bundle_state;
  char host[256];
  size_t i;

  if (st == NULL || !cookie_host(adr, host, sizeof(host)))
    return;
  for (i = 0; i < st->count; i++) {
    const bundle *const b = &st->items[i];

    if (bundle_in_scope(b, host)) {
      if (b->ua != NULL) {
        req->user_agent = b->ua;
        req->user_agent_send = HTS_TRUE;
      }
      if (StringNotEmpty(b->headers) &&
          (!b->https_only || strfield(adr, "https://")))
        req->headers = StringBuff(b->headers);
      return;
    }
  }
}
