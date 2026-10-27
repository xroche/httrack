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
#include "htscmdline.h"
#include "htslib.h"
#include "htslines.h"
#include "htspsl.h"
#include "htsio.h"
#include "htscharset.h"
#include "htssafe.h"

#include <limits.h>
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
  hts_boolean https_only;  /* Are its headers for https alone? */
  hts_boolean scope_given; /* Do scope lines replace the default scope? */
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
  b->scope_given = HTS_FALSE;
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

/* Write dir/name.hts into dst. HTS_FALSE when it does not fit. */
static hts_boolean bundle_path(char *dst, size_t dst_size, const char *dir,
                               const char *name) {
  const size_t dlen = strlen(dir);

  return slprintfbuff(
      dst, dst_size, "%s%s%s.hts", dir,
      dlen != 0 && dir[dlen - 1] != '/' && dir[dlen - 1] != '\\' ? "/" : "",
      name);
}

/* Cut the suffix off url into name ("" if none). Returns NULL, or why the name
 * is refused. */
static const char *bundle_cut(char *url, char *name, size_t name_size) {
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
  const char *const why = bundle_cut(url, name, sizeof(name));

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

/* Read a line of fewer than BUNDLE_LINE_MAX bytes, its line end aside, into
   line. NULL, or why the line is refused: too long, or with a control byte
   other than TAB, because a CR would end a header early. */
static const char *bundle_readline(text_src *src,
                                   char line[BUNDLE_LINE_MAX + 1]) {
  size_t len;

  text_src_readline(src, line, BUNDLE_LINE_MAX + 1, &len);
  if (len >= BUNDLE_LINE_MAX)
    return "too long";
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

/* Does b's scope cover a cookie set for domain? host gets the folded domain. */
static hts_boolean bundle_keeps_domain(const bundle *b, const char *domain,
                                       char host[COOKIE_MAX_DOMAIN]) {
  return cookie_host(domain, host, COOKIE_MAX_DOMAIN) &&
         bundle_in_scope(b, host[0] == '.' ? host + 1 : host);
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

/* Add url's https flag and default scope to b. NULL, or why it could not. */
static const char *bundle_note_url(bundle *b, const char *url) {
  char host[COOKIE_MAX_DOMAIN];
  char entry[sizeof(host) + 1];
  const char *reg;

  if (strfield(url, "https:"))
    b->https_only = HTS_TRUE;
  if (b->scope_given)
    return NULL;
  if (!cookie_url_host(url, host, sizeof(host)))
    return "cannot work out the bundle's scope";
  // a host with no registrable domain covers itself alone
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
  char dom[COOKIE_MAX_DOMAIN];
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
static hts_boolean bundle_parse(httrackp *opt, bundle *b, text_src *src,
                                int *cookies_line, char *err, size_t err_size) {
  static const char bom[] = "\xEF\xBB\xBF";
  char BIGSTK line[BUNDLE_LINE_MAX + 1];
  int lineno = 1;
  int version;
  const char *why = bundle_readline(src, line);

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
  *cookies_line = 0;
  while (!src->eof) {
    const char *value;
    size_t klen;

    lineno++;
    why = bundle_readline(src, line);
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
      char host[COOKIE_MAX_DOMAIN];

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
      b->scope_given = HTS_TRUE;
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
  t_cookie *alone;       /* the same in a jar of opt->cookie's size, or NULL */
  hts_boolean jar_wins;  /* A record the jar holds already wins. */
  int dropped;           /* records outside the scope */
} bundle_jar;

/* The cookie_filter admit callback: keep a record in the bundle's scope, and
   stop on one another bundle gives another value. */
static hts_boolean bundle_admit(cookie_filter *filter, t_cookie *cookie,
                                hts_boolean repeat, const char *name,
                                const char *value, const char *domain,
                                const char *path, const cookie_attr *attr) {
  bundle_jar *const ctx = (bundle_jar *) filter->ctx;
  char buffer[8192];
  char host[COOKIE_MAX_DOMAIN];
  const char *other;

  if (!bundle_keeps_domain(ctx->b, domain, host)) {
    ctx->dropped++;
    return HTS_FALSE;
  }
  if (strlen(name) > COOKIE_MAX_PAIR ||
      strlen(value) > COOKIE_MAX_PAIR - strlen(name)) {
    filter->why = "a cookie name and value pass 4096 bytes";
    return HTS_FALSE;
  }
  other = cookie_find_exact(ctx->all_bundles->data, name, host, path);
  if (other != NULL && !repeat &&
      strcmp(cookie_get(buffer, other, 6), value) != 0) {
    filter->why = "a cookie has another value in another bundle";
    return HTS_FALSE;
  }
  // a silent skip would stop the clash check
  if (cookie_set(ctx->all_bundles, name, value, domain, path, NULL) != 0) {
    filter->why = "too many cookies to compare with the other bundles";
    return HTS_FALSE;
  }
  if (ctx->jar_wins &&
      cookie_find_exact(cookie->data, name, host, path) != NULL)
    return HTS_FALSE;
  // the bundles overflow a jar without the -%K records, so the bundle is wrong
  if (ctx->alone != NULL &&
      cookie_set(ctx->alone, name, value, domain, path, attr) != 0) {
    filter->why = "the cookie jar did not store a cookie";
    return HTS_FALSE;
  }
  return HTS_TRUE;
}

/* Read b's bundle from src, set its scope, and load its cookies, comparing
   them with all_bundles. b->file names it in messages. */
static hts_boolean bundle_load_src(httrackp *opt, bundle *b, text_src *src,
                                   const char *jar_file, t_cookie *all_bundles,
                                   t_cookie *alone, char *err,
                                   size_t err_size) {
  int cookies_line;
  t_cookie *scratch = NULL;
  bundle_jar ctx;
  cookie_filter filter;
  const char *why = NULL;
  size_t i;

  bundle_clear(b);
  if (!bundle_parse(opt, b, src, &cookies_line, err, err_size))
    return HTS_FALSE;
  for (i = 0; i < b->nstart && why == NULL; i++)
    why = bundle_note_url(b, b->start_urls[i]);
  for (i = 0; i < b->nurls && why == NULL; i++)
    why = bundle_note_url(b, b->urls[i]);
  if (why != NULL)
    BUNDLE_FAIL("%s: %s", b->file, why);
  memset(&ctx, 0, sizeof(ctx));
  ctx.b = b;
  ctx.all_bundles = all_bundles;
  ctx.alone = alone;
  ctx.jar_wins = bundle_jar_wins(b, jar_file);
  memset(&filter, 0, sizeof(filter));
  filter.admit = bundle_admit;
  filter.ctx = &ctx;
  filter.line = cookies_line;
  // alone took the record, so only the -%K records leave no room for it
  filter.full_skips = alone != NULL;
  if (cookies_line != 0 && opt->cookie == NULL) {
    hts_log_print(opt, LOG_WARNING,
                  "Cookies are disabled, so the cookies of %s are not sent",
                  b->file);
    // read all the same, so -b0 does not decide whether a bundle is valid
    scratch = calloct(1, sizeof(t_cookie));
    if (scratch == NULL)
      BUNDLE_FAIL("%s: out of memory", b->file);
    scratch->max_len = sizeof(scratch->data);
  }
  if (cookies_line != 0)
    cookie_load_stream(opt, scratch != NULL ? scratch : opt->cookie, src,
                       b->file, &filter);
  freet(scratch);
  if (ctx.dropped != 0)
    hts_log_print(opt, LOG_WARNING,
                  "Bundle %s: %d cookie(s) outside its scope left out; a "
                  "scope line keeps them",
                  b->file, ctx.dropped);
  if (filter.why != NULL)
    BUNDLE_FAIL("%s, line %d: %s", b->file, filter.line, filter.why);
  return HTS_TRUE;
}

/* Read b's file, set its scope, and load its cookies, comparing them with
   all_bundles. */
static hts_boolean bundle_load(httrackp *opt, bundle *b, const char *jar_file,
                               t_cookie *all_bundles, t_cookie *alone,
                               char *err, size_t err_size) {
  const char *const dir = StringNotEmpty(opt->bundle_dir)
                              ? StringBuff(opt->bundle_dir)
                              : StringBuff(opt->path_log);
  hts_boolean ok;
  text_src src;
  FILE *fp;

  if (!bundle_path(b->file, sizeof(b->file), dir, b->name))
    BUNDLE_FAIL("bundle %s: the folder path is too long", b->name);
  fp = FOPEN(b->file, "rb");
  if (fp == NULL)
    BUNDLE_FAIL("%s: cannot read the login bundle: %s", b->file,
                strerror(errno));
  src = text_src_file(fp);
  ok = bundle_load_src(opt, b, &src, jar_file, all_bundles, alone, err,
                       err_size);
  fclose(fp);
  return ok;
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

/* The bundle that stands for the merged group holding i. */
static size_t bundle_group(size_t *group, size_t i) {
  while (group[i] != i)
    i = group[i] = group[group[i]];
  return i;
}

hts_boolean hts_bundle_load_all(httrackp *opt, const char *jar_file, char *err,
                                size_t err_size) {
  hts_bundle_state *const st = (hts_bundle_state *) opt->bundle_state;
  t_cookie *all_bundles;
  t_cookie *alone = NULL;
  size_t *group;
  hts_boolean ok = HTS_TRUE;
  size_t i, j;

  if (st == NULL)
    return HTS_TRUE;
  all_bundles = calloct(1, sizeof(t_cookie));
  group = calloct(st->count, sizeof(size_t));
  if (opt->cookie != NULL)
    alone = calloct(1, sizeof(t_cookie));
  if (all_bundles == NULL || group == NULL ||
      (opt->cookie != NULL && alone == NULL)) {
    freet(all_bundles);
    freet(group);
    freet(alone);
    BUNDLE_FAIL("login bundles: out of memory");
  }
  all_bundles->max_len = sizeof(all_bundles->data);
  if (alone != NULL)
    alone->max_len = opt->cookie->max_len;
  for (i = 0; i < st->count; i++)
    group[i] = i;
  for (i = 0; i < st->count && ok; i++)
    ok = bundle_load(opt, &st->items[i], jar_file, all_bundles, alone, err,
                     err_size);
  // the log backstop hides a bundle's tokens and url-line passwords too
  for (i = 0; i < st->count && ok; i++) {
    for (j = 0; j < st->items[i].nurls; j++)
      hts_secret_add_login(opt, st->items[i].urls[j]);
    hts_secret_add_headers(opt, StringBuff(st->items[i].headers));
  }
  for (i = 0; i < st->count && ok; i++) {
    for (j = i + 1; j < st->count && ok; j++) {
      const bundle *const a = &st->items[i];
      const bundle *const b = &st->items[j];

      if (!bundle_scopes_overlap(a, b))
        continue;
      if (!bundle_same_identity(a, b)) {
        snprintf(err, err_size,
                 "%s and %s cover the same site with a different user agent "
                 "or headers",
                 a->file, b->file);
        ok = HTS_FALSE;
      } else {
        group[bundle_group(group, j)] = bundle_group(group, i);
      }
    }
  }
  // an https URL in any merged bundle makes the headers of all https-only
  for (i = 0; i < st->count; i++) {
    if (st->items[i].https_only)
      st->items[bundle_group(group, i)].https_only = HTS_TRUE;
  }
  for (i = 0; i < st->count; i++)
    st->items[i].https_only = st->items[bundle_group(group, i)].https_only;
  freet(group);
  freet(all_bundles);
  freet(alone);
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
  char host[COOKIE_MAX_DOMAIN];
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

/* See httrack-library.h. */
HTSEXT_API hts_boolean hts_registrable_domain(const char *host, char *out,
                                              size_t size) {
  char dom[COOKIE_MAX_DOMAIN];
  const char *reg;
  size_t len;

  if (out == NULL || size == 0)
    return HTS_FALSE;
  out[0] = '\0';
  if (host == NULL)
    return HTS_FALSE;
  len = strlen(host);
  if (len != 0 && host[len - 1] == '.')
    len--;
  if (len == 0 || len >= sizeof(dom))
    return HTS_FALSE;
  dom[0] = '\0';
  strlncatbuff(dom, host, sizeof(dom), len);
  hts_lowcase(dom);
  if (hts_host_is_address(dom, len))
    return HTS_FALSE;
  reg = hts_psl_registrable_domain(dom);
  if (reg == NULL || strlen(reg) >= size)
    return HTS_FALSE;
  strlcpybuff(out, reg, size);
  return HTS_TRUE;
}

/* See httrack-library.h. */
HTSEXT_API const char *hts_bundle_name_refusal(const char *name) {
  return name != NULL ? bundle_name_refusal(name)
                      : "invalid login bundle name: none given";
}

HTSEXT_API hts_boolean hts_bundle_cookie_in_scope(const char *const *urls,
                                                  size_t nurls,
                                                  const char *cookie_domain) {
  char host[COOKIE_MAX_DOMAIN];
  const char *why = NULL;
  hts_boolean keeps;
  bundle b;
  size_t i;

  if (urls == NULL || cookie_domain == NULL)
    return HTS_FALSE;
  memset(&b, 0, sizeof(b));
  StringInit(b.headers);
  for (i = 0; i < nurls && why == NULL; i++) {
    char url[HTS_URLMAXSIZE];
    char name[HTS_BUNDLE_NAME_MAX];

    if (urls[i] == NULL || strlen(urls[i]) >= sizeof(url))
      why = "invalid url";
    else {
      strcpybuff(url, urls[i]);
      why = bundle_cut(url, name, sizeof(name));
    }
    if (why == NULL)
      why = bundle_note_url(&b, url);
  }
  keeps = why == NULL && bundle_keeps_domain(&b, cookie_domain, host);
  bundle_clear(&b);
  StringFree(b.headers);
  return keeps;
}

/* Add name to the n names of *names unless it is there. HTS_FALSE when out of
   memory. */
static hts_boolean names_add_once(char ***names, size_t *n, const char *name) {
  size_t i;

  for (i = 0; i < *n; i++) {
    if (strcmp((*names)[i], name) == 0)
      return HTS_TRUE;
  }
  return strlist_add(names, n, name);
}

/* Add to *names the bundle that the start URL url names, if any. Returns NULL,
   or why the crawl refuses url. */
static const char *names_note(char ***names, size_t *n, const char *url) {
  char copy[HTS_URLLIST_LINE_SIZE];
  char name[HTS_BUNDLE_NAME_MAX];
  const char *why;

  strcpybuff(copy, url);
  why = bundle_cut(copy, name, sizeof(name));
  if (why == NULL && name[0] != '\0' && !names_add_once(names, n, name))
    why = "out of memory";
  return why;
}

/* Add to *names the bundles the words of urls name, as the command line reads
   them. Returns NULL, or why the crawl refuses a word. */
static const char *names_note_words(char ***names, size_t *n,
                                    const char *urls) {
  char *copy = strdupt(urls);
  char *a = copy;
  const char *why = NULL;

  if (copy == NULL)
    return "out of memory";
  a += strspn(a, HTS_REALSPACES);
  while (why == NULL && *a != '\0') {
    /* the crawl refuses a longer word */
    char word[HTS_URLMAXSIZE];

    if (!hts_scan_token(&a, word, sizeof(word)))
      why = "address too long";
    else if (hts_is_quoted(word, strlen(word)))
      why = "address is quoted";
    else if (strcmp(word, HTS_NOPARAM) == 0)
      why = "address is (none)";
    /* the crawl reads -& as -%, and -- and + as aliases, and an option may
       take the next word */
    else if (strncmp(word, "-&", 2) == 0 || strncmp(word, "--", 2) == 0 ||
             strcmp(word, "+") == 0 || hts_rule_would_be_read_as_opt(word))
      why = "address list holds an option";
    else if (word[0] != '+' && word[0] != '-')
      why = names_note(names, n, word);
  }
  freet(copy);
  return why;
}

/* Add to *names the bundles the lines of the -%L file path name. Returns NULL,
   or why the file cannot be read or the crawl refuses a line. */
static const char *names_note_file(char ***names, size_t *n, const char *path,
                                   char errbuf[HTS_STRERROR_SIZE]) {
  char *buff;
  size_t size;
  size_t pos = 0;
  const char *why = hts_urllist_load(path, &buff, &size, errbuf);

  while (why == NULL) {
    char line[HTS_URLLIST_LINE_SIZE];
    const hts_urllist_status status = hts_urllist_next(buff, size, &pos, line);

    if (status == HTS_URLLIST_END)
      break;
    /* the crawl drops these lines rather than refuse them */
    if (status == HTS_URLLIST_LINE && !hts_is_quoted(line, strlen(line)))
      why = names_note(names, n, line);
  }
  freet(buff);
  return why;
}

HTSEXT_API int hts_bundle_names_in_use(const char *urls, const char *list_file,
                                       char ***names, char *error,
                                       size_t error_size) {
  char **list = NULL;
  size_t n = 0;
  char errbuf[HTS_STRERROR_SIZE];
  const char *why = NULL;

  if (error != NULL && error_size != 0)
    error[0] = '\0';
  if (names == NULL)
    why = "no names pointer";
  else
    *names = NULL;
  if (why == NULL && urls != NULL)
    why = names_note_words(&list, &n, urls);
  if (why == NULL && list_file != NULL)
    why = names_note_file(&list, &n, list_file, errbuf);
  if (why == NULL) {
    char **const grown = realloct(list, (n + 1) * sizeof(char *));

    if (grown == NULL) {
      why = "out of memory";
    } else {
      list = grown;
      list[n] = NULL;
    }
  }
  if (why != NULL) {
    strlist_free(&list, &n);
    if (error != NULL && error_size != 0)
      strlncatbuff(error, why, error_size, error_size - 1);
    return -1;
  }
  *names = list;
  return (int) n;
}

HTSEXT_API void hts_bundle_names_free(char **names) {
  size_t i;

  if (names == NULL)
    return;
  for (i = 0; names[i] != NULL; i++)
    freet(names[i]);
  freet(names);
}

/* A paste holds fewer bytes than this. */
#define PASTE_MAX (256 * 1024)

/* A cURL command holds at most this many words. */
#define PASTE_WORDS_MAX 1024

HTS_STATIC_ASSERT(HTS_PASTE_NAME_SIZE == HTS_BUNDLE_NAME_MAX, paste_name_size);
HTS_STATIC_ASSERT(HTS_PASTE_URL_SIZE == HTS_CDLMAXSIZE, paste_url_size);

/* What a pasted cURL command gives. */
typedef struct paste_curl {
  char **words;
  size_t nwords;
  const char *url;  /* the URL word, or NULL */
  const char *ua;   /* the User-Agent, or NULL */
  const char *auth; /* the whole Authorization header line, or NULL */
  String cookies;   /* the Cookie header and -b values, joined by ';' */
  char why[96];     /* a refusal naming an option */
} paste_curl;

/* Is c a byte bash keeps as it is outside quotes? */
static hts_boolean curl_plain_byte(unsigned char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || strchr("-_./:=@,+%", c) != NULL || c >= 0x80;
}

/* The length of a backslash-newline join at p, or 0 when there is none. */
static size_t curl_join(const char *p) {
  if (p[0] != '\\')
    return 0;
  if (p[1] == '\n')
    return 2;
  return p[1] == '\r' && p[2] == '\n' ? 3 : 0;
}

/* The value of the hex digit c, or -1. */
static int curl_hex(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

/* The value of the n hex digits at p, or -1. */
static long curl_hex_run(const char *p, int n) {
  long v = 0;
  int i;

  for (i = 0; i < n; i++) {
    const int d = curl_hex(p[i]);

    if (d < 0)
      return -1;
    v = v * 16 + d;
  }
  return v;
}

/* Decode the $'...' body at *pp, past the opening quote, into dst at *len,
   and leave *pp past the closing quote. Only the escapes Chrome and Firefox
   write are read, and none decodes to more bytes than it takes. NULL, or why
   it is refused. */
static const char *curl_ansi_quote(const char **pp, char *dst, size_t *len) {
  static const char digits[] = "an escape in $'...' lacks its hex digits";
  const char *p = *pp;

  while (*p != '\'') {
    long cp;

    if (*p == '\0')
      return "a quote is not closed";
    if (*p != '\\') {
      dst[(*len)++] = *p++;
      continue;
    }
    p++;
    switch (*p) {
    case '\\':
    case '\'':
      cp = (unsigned char) *p++;
      break;
    case 'n':
    case 'r':
    case 't':
      cp = *p == 'n' ? '\n' : *p == 'r' ? '\r' : '\t';
      p++;
      break;
    case 'x':
      if ((cp = curl_hex_run(p + 1, 2)) < 0)
        return digits;
      p += 3;
      if (cp == 0)
        return "the text holds a NUL byte";
      dst[(*len)++] = (char) cp; // a raw byte, as bash gives it
      continue;
    case 'u':
      if ((cp = curl_hex_run(p + 1, 4)) < 0)
        return digits;
      p += 5;
      // Firefox writes a character past U+FFFF as two UTF-16 halves
      if (cp >= 0xd800 && cp <= 0xdbff && p[0] == '\\' && p[1] == 'u') {
        const long low = curl_hex_run(p + 2, 4);

        if (low >= 0xdc00 && low <= 0xdfff) {
          cp = 0x10000 + ((cp - 0xd800) << 10) + (low - 0xdc00);
          p += 6;
        }
      }
      break;
    default:
      return "$'...' holds an escape no browser writes";
    }
    if (cp == 0)
      return "the text holds a NUL byte";
    if (cp >= 0xd800 && cp <= 0xdfff)
      return "an escape in $'...' is out of range";
    if (cp < 0x80) {
      dst[(*len)++] = (char) cp;
    } else if (cp < 0x800) {
      dst[(*len)++] = (char) (0xc0 | (cp >> 6));
      dst[(*len)++] = (char) (0x80 | (cp & 0x3f));
    } else if (cp < 0x10000) {
      dst[(*len)++] = (char) (0xe0 | (cp >> 12));
      dst[(*len)++] = (char) (0x80 | ((cp >> 6) & 0x3f));
      dst[(*len)++] = (char) (0x80 | (cp & 0x3f));
    } else {
      dst[(*len)++] = (char) (0xf0 | (cp >> 18));
      dst[(*len)++] = (char) (0x80 | ((cp >> 12) & 0x3f));
      dst[(*len)++] = (char) (0x80 | ((cp >> 6) & 0x3f));
      dst[(*len)++] = (char) (0x80 | (cp & 0x3f));
    }
  }
  *pp = p + 1;
  return NULL;
}

/* Split the bash command s into c->words, accepting '...' and $'...' quotes
   and backslash-newline joins alone. NULL, or why it is refused. */
static const char *curl_split(const char *s, paste_curl *c) {
  // no word decodes to more bytes than s holds
  char *word = malloct(strlen(s) + 1);
  const char *p = s;
  const char *why = NULL;

  if (word == NULL)
    return "out of memory";
  while (why == NULL) {
    size_t len = 0, n;

    for (;;) {
      if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
      else if ((n = curl_join(p)) != 0)
        p += n;
      else
        break;
    }
    if (*p == '\0')
      break;
    while (why == NULL && *p != '\0' && strchr(" \t\r\n", *p) == NULL) {
      if (*p == '\'') {
        const char *const end = strchr(p + 1, '\'');

        if (end == NULL) {
          why = "a quote is not closed";
        } else {
          memcpy(word + len, p + 1, (size_t) (end - p - 1));
          len += (size_t) (end - p - 1);
          p = end + 1;
        }
      } else if (p[0] == '$' && p[1] == '\'') {
        p += 2;
        why = curl_ansi_quote(&p, word, &len);
      } else if ((n = curl_join(p)) != 0) {
        p += n;
      } else if (curl_plain_byte((unsigned char) *p)) {
        word[len++] = *p++;
      } else {
        why = "the text holds shell syntax other than '...' and $'...' quotes";
      }
    }
    if (why != NULL)
      break;
    word[len] = '\0';
    if (hts_bundle_has_control(word, len))
      why = "the text holds a control byte";
    else if (c->nwords >= PASTE_WORDS_MAX)
      why = "the command has too many words";
    else if (!strlist_add(&c->words, &c->nwords, word))
      why = "out of memory";
  }
  freet(word);
  return why;
}

/* Read the header line h into c. */
static void curl_header(paste_curl *c, const char *h) {
  const size_t nlen = strcspn(h, ":");
  const char *value = h + nlen;

  if (*value == '\0')
    return; // "Name;" sends an empty header, which carries nothing here
  value++;
  value += strspn(value, " \t");
  if (nlen == strlen("Cookie") && strfield(h, "Cookie")) {
    StringCat(c->cookies, value);
    StringCat(c->cookies, ";");
  } else if (nlen == strlen("User-Agent") && strfield(h, "User-Agent")) {
    c->ua = value;
  } else if (nlen == strlen("Authorization") && strfield(h, "Authorization")) {
    c->auth = *value != '\0' ? h : NULL;
  }
}

/* Read the curl options of c->words, whose first is "curl". NULL, or why they
   are refused. */
static const char *curl_options(paste_curl *c) {
  static const char *const no_arg[] = {
      "--compressed", "--insecure", "-k",         "-s",
      "--silent",     "-L",         "--location", "-i",
      "--include",    "-v",         "--verbose",  "-g",
      "--globoff",    "-I",         "--head",     "--http1.0",
      "--http1.1",    "--http2",    "--http3",    "--http2-prior-knowledge"};
  static const char *const skip_arg[] = {
      "-X",         "--request",     "-d",           "--data",
      "--data-raw", "--data-binary", "--data-ascii", "--data-urlencode",
      "-e",         "--referer",     "-F",           "--form"};
  size_t i, k;

  for (i = 1; i < c->nwords; i++) {
    const char *const w = c->words[i];
    const char *const arg = i + 1 < c->nwords ? c->words[i + 1] : NULL;
    hts_boolean known = HTS_FALSE;

    if (w[0] != '-' || strcmp(w, "--url") == 0) {
      if (w[0] == '-') {
        if (arg == NULL)
          return "--url has no value";
        i++;
      }
      if (c->url != NULL)
        return "the command holds more than one URL";
      c->url = c->words[i];
      continue;
    }
    for (k = 0; k < sizeof(no_arg) / sizeof(no_arg[0]) && !known; k++)
      known = strcmp(w, no_arg[k]) == 0;
    if (known)
      continue;
    for (k = 0; k < sizeof(skip_arg) / sizeof(skip_arg[0]) && !known; k++)
      known = strcmp(w, skip_arg[k]) == 0;
    known = known || strcmp(w, "-H") == 0 || strcmp(w, "--header") == 0 ||
            strcmp(w, "-b") == 0 || strcmp(w, "--cookie") == 0 ||
            strcmp(w, "-A") == 0 || strcmp(w, "--user-agent") == 0;
    if (!known) {
      // the name alone, because "-bSID=x" or "--opt=x" glue a value to it
      int shown = w[1] == '-' ? (int) strcspn(w, "=") : 2;

      if (shown > 32)
        shown = 32;
      snprintf(c->why, sizeof(c->why),
               "the command holds the unknown curl option %.*s%s", shown, w,
               w[shown] != '\0' && w[shown] != '=' ? "..." : "");
      return c->why;
    }
    if (arg == NULL)
      return "an option has no value";
    if (strcmp(w, "-H") == 0 || strcmp(w, "--header") == 0) {
      curl_header(c, arg);
    } else if (strcmp(w, "-b") == 0 || strcmp(w, "--cookie") == 0) {
      if (strchr(arg, '=') == NULL)
        return "-b names a file, not cookies";
      StringCat(c->cookies, arg);
      StringCat(c->cookies, ";");
    } else if (strcmp(w, "-A") == 0 || strcmp(w, "--user-agent") == 0) {
      c->ua = arg;
    }
    i++;
  }
  if (c->url == NULL)
    return "the command holds no URL";
  return NULL;
}

/* Is the paste at s a cURL command? */
static hts_boolean paste_is_curl(const char *s) {
  s += strspn(s, " \t\r\n");
  return strncmp(s, "curl", 4) == 0 &&
         (s[4] == ' ' || s[4] == '\t' || curl_join(s + 4) != 0);
}

/* Copy the cookies.txt paste s into out line for line, comments blanked. */
static void paste_records(const char *s, String *out) {
  static const char httponly[] = "#HttpOnly_";

  while (*s != '\0') {
    size_t len = strcspn(s, "\n");
    const size_t next = len + (s[len] == '\n');

    if (len > 0 && s[len - 1] == '\r')
      len--;
    if (strspn(s, " \t") < len &&
        (s[0] != '#' || strncmp(s, httponly, sizeof(httponly) - 1) == 0))
      StringMemcat(*out, s, len);
    if (s[next - 1] == '\n')
      StringCat(*out, "\n");
    s += next;
  }
}

/* Do the len bytes at s hold a bidi control, U+202A to U+202E or U+2066 to
   U+2069, which could make the preview show another text? */
static hts_boolean paste_has_bidi(const char *s, size_t len) {
  size_t i;

  for (i = 0; i + 2 < len; i++) {
    const unsigned char a = (unsigned char) s[i], b = (unsigned char) s[i + 1],
                        c = (unsigned char) s[i + 2];

    if (a == 0xe2 && ((b == 0x80 && c >= 0xaa && c <= 0xae) ||
                      (b == 0x81 && c >= 0xa6 && c <= 0xa9)))
      return HTS_TRUE;
  }
  return HTS_FALSE;
}

/* Set up b as a bundle whose only start URL is url. HTS_FALSE when out of
   memory. */
static hts_boolean paste_bundle(bundle *b, const char *name, const char *url) {
  memset(b, 0, sizeof(*b));
  StringInit(b->headers);
  strcpybuff(b->name, name);
  snprintf(b->file, sizeof(b->file), "%s.hts", name);
  return strlist_add(&b->start_urls, &b->nstart, url);
}

static void paste_bundle_free(bundle *b) {
  bundle_clear(b);
  strlist_free(&b->start_urls, &b->nstart);
  StringFree(b->headers);
}

/* Store the cookie pairs of c in jar as document.cookie pairs of url, bound
   to its registrable domain. NULL, or why they are refused. */
static const char *paste_curl_cookies(httrackp *opt, t_cookie *jar,
                                      const paste_curl *c, const char *url) {
  char host[COOKIE_MAX_DOMAIN];
  cookie_attr attr;
  const char *const domain = cookie_flat_bind(url, host, sizeof(host), &attr);
  char *pairs =
      strdupt(StringNotEmpty(c->cookies) ? StringBuff(c->cookies) : "");
  const char *why = NULL;
  char *p = pairs;
  int ignored = 0;

  if (pairs == NULL)
    return "out of memory";
  if (domain == NULL)
    why = "the address has no host";
  while (why == NULL && *p != '\0') {
    char *const end = p + strcspn(p, ";");
    char *pair = p, *eq, *name_end;
    const char *value;
    size_t len;

    p = *end != '\0' ? end + 1 : end;
    *end = '\0';
    pair += strspn(pair, " ");
    for (len = strlen(pair); len > 0 && pair[len - 1] == ' ';)
      pair[--len] = '\0';
    if (*pair == '\0')
      continue;
    if (strchr(pair, '\t') != NULL) {
      why = "a cookie holds a TAB";
      break;
    }
    eq = strchr(pair, '=');
    for (name_end = eq;
         name_end != NULL && name_end > pair && name_end[-1] == ' ';)
      name_end--;
    if (eq == NULL || name_end == pair) {
      why = "a cookie in the paste has no name";
      break;
    }
    // "g = h" is the cookie g, as a browser reads it
    value = eq + 1 + strspn(eq + 1, " ");
    *name_end = '\0';
    if (cookie_find_exact(jar->data, pair, domain, "/") != NULL)
      continue; // a browser lists the most specific path first, so it wins
    *name_end = '=';
    memmove(name_end + 1, value, strlen(value) + 1);
    if (!cookie_flat_pair(opt, jar, pair, domain, &attr, "the paste", &ignored))
      why = "the cookies of the paste do not fit in a bundle";
  }
  freet(pairs);
  return why;
}

/* Load the cookies.txt records of text into jar, keeping those inside the
   scope of b. NULL, or why the paste is refused, with its line in *line. */
static const char *paste_load_records(httrackp *opt, bundle *b,
                                      const String *text, t_cookie *jar,
                                      int *line) {
  t_cookie *all = calloct(1, sizeof(t_cookie));
  text_src src = text_src_mem(StringBuff(*text), StringLength(*text));
  bundle_jar ctx;
  cookie_filter filter;
  const char *why = NULL;
  size_t i;

  *line = 0;
  if (all == NULL)
    return "out of memory";
  all->max_len = sizeof(all->data);
  for (i = 0; i < b->nstart && why == NULL; i++)
    why = bundle_note_url(b, b->start_urls[i]);
  if (why == NULL) {
    memset(&ctx, 0, sizeof(ctx));
    ctx.b = b;
    ctx.all_bundles = all;
    memset(&filter, 0, sizeof(filter));
    filter.admit = bundle_admit;
    filter.ctx = &ctx;
    cookie_load_stream(opt, jar, &src, "the paste", &filter);
    why = filter.why;
    *line = filter.line;
  }
  freet(all);
  return why;
}

/* Count the records of jar into *count, and list their domains in hosts,
   ending with ",..." when they do not all fit. */
static void paste_jar_summary(const t_cookie *jar, int *count,
                              char hosts[HTS_PASTE_HOSTS_SIZE]) {
  static const char more[] = ",...";
  char buffer[8192];
  char line[8192];
  const char *a = jar->data;
  hts_boolean cut = HTS_FALSE;

  *count = 0;
  hosts[0] = '\0';
  while (*a != '\0') {
    char needle[COOKIE_MAX_DOMAIN + 2];
    char listed[HTS_PASTE_HOSTS_SIZE + 2];
    const char *dom;

    a += binput(a, line, (int) sizeof(line) - 1);
    dom = cookie_get(buffer, line, 0);
    if (dom[0] == '.')
      dom++;
    if (*count < INT_MAX)
      (*count)++;
    // the list is searched with commas around it, so one domain is one entry
    snprintf(needle, sizeof(needle), ",%s,", dom);
    snprintf(listed, sizeof(listed), ",%s,", hosts);
    if (strstr(listed, needle) != NULL)
      continue;
    if (strlen(dom) + sizeof(more) < HTS_PASTE_HOSTS_SIZE - strlen(hosts)) {
      if (hosts[0] != '\0')
        strlcatbuff(hosts, ",", HTS_PASTE_HOSTS_SIZE);
      strlcatbuff(hosts, dom, HTS_PASTE_HOSTS_SIZE);
    } else {
      cut = HTS_TRUE;
    }
  }
  if (cut)
    strlcatbuff(hosts, hosts[0] != '\0' ? more : more + 1,
                HTS_PASTE_HOSTS_SIZE);
}

/* Write the reason into result->error and return HTS_FALSE. */
#define PASTE_FAIL(...)                                                        \
  do {                                                                         \
    slprintfbuff_clip(result->error, sizeof(result->error), __VA_ARGS__);      \
    ok = HTS_FALSE;                                                            \
    goto done;                                                                 \
  } while (0)

/* Drop the trailing dots of the host of url, in place, so the cookies and the
   start URL name the host the name does. */
static void paste_trim_host_dots(char *url) {
  char *const host = strstr(url, "://") + 3;
  char *const end = host + strcspn(host, ":/?\\");
  char *p = end;

  while (p > host && p[-1] == '.')
    p--;
  memmove(p, end, strlen(end) + 1);
}

/* Write the host of url, of size bytes, in its ASCII form, as the engine
   requests it. NULL, or why it has none. */
static const char *paste_host_idna(char *url, size_t size) {
  char *const host = strstr(url, "://") + 3;
  const size_t len = strcspn(host, ":/?\\");
  const char *const rest = host + len;
  char *idna;
  const char *why = NULL;

  if (hts_isStringAscii(host, len))
    return NULL;
  idna = hts_convertStringUTF8ToIDNA(host, len);
  if (idna == NULL ||
      strlen(idna) >= size - (size_t) (host - url) - strlen(rest))
    why = "the address is too long once its host is in ASCII";
  else {
    hts_lowcase(idna);
    memmove(host + strlen(idna), rest, strlen(rest) + 1);
    memcpy(host, idna, strlen(idna));
  }
  freet(idna);
  return why;
}

/* Copy into name the bundle name of url: the registrable domain its scope
   covers, else the host without the port, a leading "www." or trailing dots,
   with "_" before a device name. NULL, or why there is none. */
static const char *paste_name(const char *url, char name[HTS_BUNDLE_NAME_MAX]) {
  char host[COOKIE_MAX_DOMAIN];
  const char *h = host;
  // "_con.com" is what the escape below makes of con.com
  const char *const taken = "the name is kept for an escaped device name";
  const char *escape;
  hts_boolean www;
  size_t len;

  if (!cookie_url_host(url, host, sizeof(host)))
    return "the address has no host";
  if (host[0] == '[' || strchr(host, ':') != NULL)
    return "an IP address cannot name a login bundle";
  www = strncmp(host, "www.", 4) == 0;
  for (len = strlen(host); len > 0 && host[len - 1] == '.';)
    host[--len] = '\0';
  // www.github.io is a site of its own, so "www." stays here
  if (hts_registrable_domain(host, name, HTS_BUNDLE_NAME_MAX) &&
      !hts_host_is_address(name, strlen(name)) &&
      hts_bundle_name_refusal(name) == NULL)
    return name[0] == '_' && bundle_name_is_device(name + 1) ? taken : NULL;
  if (www)
    h += len < 4 ? len : 4;
  if (*h == '\0')
    return "the address names no site";
  if (*h == '_' && bundle_name_is_device(h + 1))
    return taken;
  escape = bundle_name_is_device(h) ? "_" : "";
  if (strlen(h) >= HTS_BUNDLE_NAME_MAX - strlen(escape))
    return "invalid login bundle name: 128 bytes or more";
  strlcpybuff(name, escape, HTS_BUNDLE_NAME_MAX);
  strlcatbuff(name, h, HTS_BUNDLE_NAME_MAX);
  return hts_bundle_name_refusal(name);
}

/* Trim the blanks around the user agent at *ua, into buf. NULL, or why it
   cannot be sent. */
static const char *paste_agent(const char **ua,
                               char buf[HTS_PASTE_AGENT_SIZE]) {
  const char *s = *ua;
  size_t len;

  if (s == NULL || s[0] == '\0') {
    *ua = NULL;
    return NULL;
  }
  s += strspn(s, " \t");
  for (len = strlen(s); len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t');)
    len--;
  if (len == 0)
    return "the user agent is blank";
  if (len >= HTS_PASTE_AGENT_SIZE || hts_bundle_has_control(s, len))
    return "the user agent cannot be used";
  if (paste_has_bidi(s, len))
    return "the user agent holds a text direction control";
  buf[0] = '\0';
  strlncatbuff(buf, s, HTS_PASTE_AGENT_SIZE, len);
  *ua = buf;
  return NULL;
}

/* See httrack-library.h. */
HTSEXT_API hts_boolean hts_bundle_from_paste(const char *paste,
                                             const char *folder,
                                             const char *agent, const char *url,
                                             hts_boolean dry_run,
                                             hts_paste_result *result) {
  return hts_bundle_from_paste_flags(paste, folder, agent, url,
                                     dry_run ? HTS_PASTE_DRY_RUN : 0, result);
}

/* See httrack-library.h. */
HTSEXT_API hts_boolean hts_bundle_from_paste_flags(
    const char *paste, const char *folder, const char *agent, const char *url,
    unsigned int flags, hts_paste_result *result) {
  const hts_boolean dry_run = (flags & HTS_PASTE_DRY_RUN) != 0;
  const hts_boolean keep_empty = (flags & HTS_PASTE_KEEP_EMPTY) != 0;
  paste_curl c;
  String records = STRING_EMPTY;
  String text = STRING_EMPTY;
  bundle b, check;
  t_cookie *jar = NULL, *all = NULL;
  httrackp *opt = NULL;
  char name[HTS_BUNDLE_NAME_MAX];
  char base[HTS_URLMAXSIZE];
  char path[HTS_URLMAXSIZE * 2];
  char ua_buf[HTS_PASTE_AGENT_SIZE];
  char err[512];
  const char *ua;
  const char *why;
  text_src src;
  hts_boolean curl, ok = HTS_TRUE;
  int count = 0, line;
  size_t len, i, size = 0;

  memset(&c, 0, sizeof(c));
  memset(&b, 0, sizeof(b));
  memset(&check, 0, sizeof(check));
  StringInit(c.cookies);
  if (result == NULL || result->size < sizeof(*result))
    return HTS_FALSE;
  size = result->size;
  memset(result, 0, sizeof(*result));
  result->size = size;

  if ((flags & ~(HTS_PASTE_DRY_RUN | HTS_PASTE_KEEP_EMPTY)) != 0)
    PASTE_FAIL("unknown flags 0x%x", flags);
  if (paste == NULL && keep_empty)
    paste = "";
  // validate the paste before reading it, so a stray byte never reaches a file
  if (paste == NULL || (paste[strspn(paste, " \t\r\n")] == '\0' && !keep_empty))
    PASTE_FAIL("nothing was pasted");
  len = strlen(paste);
  if (len >= PASTE_MAX)
    PASTE_FAIL("the paste is longer than %d bytes", PASTE_MAX - 1);
  for (i = 0; i < len; i++) {
    const unsigned char ch = (unsigned char) paste[i];

    if ((ch < 0x20 && ch != '\t' && ch != '\n' && ch != '\r') || ch == 0x7f)
      PASTE_FAIL("the paste holds a control byte");
  }
  if (!dry_run && (folder == NULL || folder[0] == '\0'))
    PASTE_FAIL("no project folder to write the bundle to");
  if (strncmp(paste, "\xEF\xBB\xBF", 3) == 0)
    paste += 3;

  curl = paste_is_curl(paste);
  if (curl) {
    why = curl_split(paste, &c);
    if (why == NULL)
      why = curl_options(&c);
    if (why != NULL)
      PASTE_FAIL("%s", why);
    url = c.url;
  } else if (url == NULL || url[0] == '\0') {
    PASTE_FAIL("a cookies.txt needs the address of the site");
  }
  ua = curl && c.ua != NULL && c.ua[0] != '\0' ? c.ua : agent;
  why = paste_agent(&ua, ua_buf);
  if (why != NULL)
    PASTE_FAIL("%s", why);

  // the fragment is cut, because a sign-in can return a token there
  len = strcspn(url, "#");
  if (!strfield(url, "http://") && !strfield(url, "https://"))
    PASTE_FAIL("the address is not an http or https URL");
  if (len >= sizeof(base) || hts_bundle_has_control(url, len))
    PASTE_FAIL("the address cannot be used");
  if (paste_has_bidi(url, len))
    PASTE_FAIL("the address holds a text direction control");
  base[0] = '\0';
  strlncatbuff(base, url, sizeof(base), len);
  {
    const char *const auth = strstr(base, "://") + 3;

    // the password would land in the URL list and the project file
    if (memchr(auth, '@', strcspn(auth, "/?\\")) != NULL)
      PASTE_FAIL("the address holds a user name or password");
    why = cookie_host_refusal(auth, strcspn(auth, ":/?\\"));
    if (why != NULL)
      PASTE_FAIL("%s", why);
  }
  why = paste_name(base, name);
  if (why != NULL)
    PASTE_FAIL("%s", why);
  paste_trim_host_dots(base);
  why = paste_host_idna(base, sizeof(base));
  if (why != NULL)
    PASTE_FAIL("%s", why);
  if (strlen(name) + strlen(HTS_BUNDLE_SUFFIX) >= HTS_CDLMAXSIZE - strlen(base))
    PASTE_FAIL("the address is longer than %d bytes once the bundle name is "
               "added",
               HTS_CDLMAXSIZE - 1);

  opt = hts_create_opt();
  jar = calloct(1, sizeof(t_cookie));
  all = calloct(1, sizeof(t_cookie));
  if (opt == NULL || jar == NULL || all == NULL ||
      !paste_bundle(&b, name, base) || !paste_bundle(&check, name, base))
    PASTE_FAIL("out of memory");
  opt->log = NULL; // the reason goes to the caller, not to stdout
  jar->max_len = COOKIE_MAX_JAR;
  all->max_len = sizeof(all->data);

  // the cookies, into jar
  if (curl) {
    why = paste_curl_cookies(opt, jar, &c, base);
    if (why != NULL)
      PASTE_FAIL("%s", why);
  } else {
    paste_records(paste, &records);
    why = paste_load_records(opt, &b, &records, jar, &line);
    if (why != NULL)
      PASTE_FAIL("line %d of the paste: %s", line, why);
  }

  StringCat(text, "# HTTrack bundle 1" LF);
  if (ua != NULL) {
    StringCat(text, "ua ");
    StringCat(text, ua);
    StringCat(text, LF);
  }
  if (c.auth != NULL) {
    StringCat(text, "header ");
    StringCat(text, c.auth);
    StringCat(text, LF);
  }
  if (!keep_empty || jar->data[0] != '\0') {
    StringCat(text, "cookies" LF);
    cookie_records_text(jar, &text);
  }

  // read it back through the engine's own loader, which also gives the count
  memset(jar->data, 0, sizeof(jar->data));
  opt->cookie = jar;
  src = text_src_mem(StringBuff(text), StringLength(text));
  if (!bundle_load_src(opt, &check, &src, NULL, all, NULL, err, sizeof(err)))
    PASTE_FAIL("%s", err);
  paste_jar_summary(jar, &count, result->hosts);
  if (count == 0 && c.auth == NULL && !keep_empty)
    PASTE_FAIL("the paste holds no cookie for %s", name);

  if (!dry_run) {
    if (!bundle_path(path, sizeof(path), folder, name))
      PASTE_FAIL("the folder path is too long");
    if (!cookie_write_replace(path, StringBuff(text), StringLength(text)))
      PASTE_FAIL("cannot write %s: %s", path, strerror(errno));
  }
  strcpybuff(result->name, name);
  slprintfbuff_clip(result->url, sizeof(result->url), "%s%s%s", base,
                    HTS_BUNDLE_SUFFIX, name);
  if (ua != NULL)
    strcpybuff(result->agent, ua);
  result->cookies = count;

done:
  if (!ok) {
    char reason[sizeof(result->error)];

    strcpybuff(reason, result->error);
    memset(result, 0, sizeof(*result));
    result->size = size;
    strcpybuff(result->error, reason);
  }
  if (opt != NULL) {
    opt->cookie = NULL;
    hts_free_opt(opt);
  }
  freet(jar);
  freet(all);
  paste_bundle_free(&b);
  paste_bundle_free(&check);
  strlist_free(&c.words, &c.nwords);
  StringFree(c.cookies);
  StringFree(records);
  StringFree(text);
  return ok;
}
