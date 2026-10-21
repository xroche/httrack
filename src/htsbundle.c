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
}

/* Is name a bundle name: [A-Za-z0-9._-], no leading '.', no "..", and no
   Windows device name such as CON or com1.txt? */
static hts_boolean bundle_name_valid(const char *name) {
  static const char *const devices[] = {"con", "prn", "aux", "nul"};
  const size_t len = strlen(name);
  const size_t base = strcspn(name, ".");
  char dev[5];
  size_t i;

  if (len == 0 || len >= HTS_BUNDLE_NAME_MAX || name[0] == '.' ||
      strstr(name, "..") != NULL ||
      strspn(name, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
                   "0123456789._-") != len)
    return HTS_FALSE;
  if (base < 3 || base > 4)
    return HTS_TRUE;
  dev[0] = '\0';
  strlncatbuff(dev, name, sizeof(dev), base);
  hts_lowcase(dev);
  for (i = 0; base == 3 && i < sizeof(devices) / sizeof(devices[0]); i++) {
    if (strcmp(dev, devices[i]) == 0)
      return HTS_FALSE;
  }
  return base == 3 || dev[3] < '1' || dev[3] > '9' ||
         (strncmp(dev, "com", 3) != 0 && strncmp(dev, "lpt", 3) != 0);
}

hts_bundle_suffix hts_bundle_cut(char *url, char *name, size_t name_size) {
  char *const at = strstr(url, HTS_BUNDLE_SUFFIX);
  const char *text;

  if (at == NULL)
    return HTS_BUNDLE_NONE;
  text = at + strlen(HTS_BUNDLE_SUFFIX);
  if (!bundle_name_valid(text) || strlen(text) >= name_size)
    return HTS_BUNDLE_BAD;
  strlcpybuff(name, text, name_size);
  *at = '\0';
  return HTS_BUNDLE_NAMED;
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

hts_boolean hts_bundle_add(httrackp *opt, const char *name, const char *url) {
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

/* Read a line into line without its line end. HTS_FALSE for a line too long,
   or holding a control byte other than TAB: a CR would end a header early. */
static hts_boolean bundle_readline(FILE *fp, char *line, size_t size) {
  size_t len;
  size_t i;

  if (hts_readline_raw(fp, line, size, &len))
    return HTS_FALSE;
  if (len > 0 && line[len - 1] == '\r')
    line[--len] = '\0';
  for (i = 0; i < len; i++) {
    const unsigned char c = (unsigned char) line[i];

    if ((c < 0x20 && c != '\t') || c == 0x7f)
      return HTS_FALSE;
  }
  return HTS_TRUE;
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

static hts_boolean bundle_in_scope(const void *b_, const char *host) {
  const bundle *const b = (const bundle *) b_;
  size_t i;

  for (i = 0; i < b->nscope; i++) {
    if (scope_covers(b->scope[i], host))
      return HTS_TRUE;
  }
  return HTS_FALSE;
}

/* Add entry to b's scope. HTS_FALSE when out of memory or full. */
static hts_boolean bundle_scope_add(bundle *b, const char *entry) {
  size_t i;

  for (i = 0; i < b->nscope; i++) {
    if (strcmp(b->scope[i], entry) == 0)
      return HTS_TRUE;
  }
  return b->nscope < BUNDLE_LIST_MAX &&
         strlist_add(&b->scope, &b->nscope, entry);
}

/* Add url's registrable domain to b's scope, or its host alone when it has
   none: a public suffix, a single label or an address. */
static hts_boolean bundle_scope_url(bundle *b, const char *url) {
  char host[256];
  char entry[sizeof(host) + 1];
  hts_boolean https;
  const char *reg;

  if (!cookie_url_host(url, host, sizeof(host), &https))
    return HTS_FALSE;
  reg = hts_host_is_address(host, strlen(host))
            ? NULL
            : hts_psl_registrable_domain(host);
  snprintf(entry, sizeof(entry), "%s%s", reg != NULL ? "" : "=",
           reg != NULL ? reg : host);
  return bundle_scope_add(b, entry);
}

/* Write why into err and return HTS_FALSE, so it needs err and err_size in
   scope. */
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

  if (value[0] == '.')
    value++;
  if (value[0] == '\0' || strlen(value) >= sizeof(dom) ||
      strspn(value, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
                    "0123456789.-") != strlen(value))
    BUNDLE_FAIL("%s, line %d: invalid scope: %.64s", b->file, lineno, value);
  strcpybuff(dom, value);
  hts_lowcase(dom);
  if (hts_host_is_address(dom, strlen(dom)))
    snprintf(entry, sizeof(entry), "=%s", dom);
  else if (hts_psl_registrable_domain(dom) != NULL)
    strcpybuff(entry, dom);
  else
    BUNDLE_FAIL("%s, line %d: scope %s is a public suffix", b->file, lineno,
                dom);
  if (!bundle_scope_add(b, entry))
    BUNDLE_FAIL("%s, line %d: more than %d scope lines, or out of memory",
                b->file, lineno, BUNDLE_LIST_MAX);
  return HTS_TRUE;
}

/* Read b's lines up to "cookies", leaving fp at its jar, and set *jar. */
static hts_boolean bundle_parse(httrackp *opt, bundle *b, FILE *fp,
                                hts_boolean *scope_given, hts_boolean *jar,
                                char *err, size_t err_size) {
  static const char bom[] = "\xEF\xBB\xBF";
  char BIGSTK line[BUNDLE_LINE_MAX];
  int lineno = 1;
  int version;

  if (!bundle_readline(fp, line, sizeof(line)))
    BUNDLE_FAIL("%s, line 1: too long or holding a control byte", b->file);
  version = bundle_version_of(
      strncmp(line, bom, sizeof(bom) - 1) == 0 ? line + sizeof(bom) - 1 : line);
  if (version < 0)
    BUNDLE_FAIL("%s: not an HTTrack bundle, the first line must be "
                "\"# HTTrack bundle 1\"",
                b->file);
  if (version != 1)
    BUNDLE_FAIL("%s: bundle version %d is not supported", b->file, version);
  *scope_given = *jar = HTS_FALSE;
  while (!feof(fp)) {
    const char *value;
    size_t klen;

    lineno++;
    if (!bundle_readline(fp, line, sizeof(line)))
      BUNDLE_FAIL("%s, line %d: too long or holding a control byte", b->file,
                  lineno);
    if (line[0] == '\0' || line[0] == '#')
      continue;
    klen = strcspn(line, " \t");
    value = line + klen;
    value += strspn(value, " \t");
    if (klen == 7 && strncmp(line, "cookies", klen) == 0) {
      if (*value != '\0')
        BUNDLE_FAIL("%s, line %d: \"cookies\" takes no value", b->file, lineno);
      *jar = HTS_TRUE;
      return HTS_TRUE;
    }
    if (*value == '\0')
      BUNDLE_FAIL("%s, line %d: no value", b->file, lineno);
    if (klen == 3 && strncmp(line, "url", klen) == 0) {
      char host[256];
      hts_boolean https;

      /* a url can hold a token, so the message leaves it out */
      if (strlen(value) >= HTS_URLMAXSIZE ||
          strstr(value, HTS_BUNDLE_SUFFIX) != NULL ||
          !cookie_url_host(value, host, sizeof(host), &https))
        BUNDLE_FAIL("%s, line %d: invalid url", b->file, lineno);
      if (b->nurls >= BUNDLE_LIST_MAX)
        BUNDLE_FAIL("%s, line %d: more than %d url lines", b->file, lineno,
                    BUNDLE_LIST_MAX);
      if (!strlist_add(&b->urls, &b->nurls, value))
        BUNDLE_FAIL("%s: out of memory", b->file);
    } else if (klen == 2 && strncmp(line, "ua", klen) == 0) {
      if (b->ua != NULL)
        BUNDLE_FAIL("%s, line %d: ua given twice", b->file, lineno);
      b->ua = strdupt(value);
      if (b->ua == NULL)
        BUNDLE_FAIL("%s: out of memory", b->file);
    } else if (klen == 6 && strncmp(line, "header", klen) == 0) {
      const char *const why =
          http_bundle_header_refusal(StringBuff(b->headers), value);
      /* the value can be a secret, so only the name is shown */
      const size_t name = strcspn(value, ":");
      const int shown = value[name] == ':' && name <= 64 ? (int) name : 0;

      if (why != NULL)
        BUNDLE_FAIL("%s, line %d: refused header%s%.*s: %s", b->file, lineno,
                    shown != 0 ? " " : "", shown, value, why);
      StringCat(b->headers, value);
      StringCat(b->headers, "\r\n");
    } else if (klen == 5 && strncmp(line, "scope", klen) == 0) {
      *scope_given = HTS_TRUE;
      if (!bundle_scope_line(b, value, lineno, err, err_size))
        return HTS_FALSE;
    } else {
      hts_log_print(opt, LOG_WARNING, "Bundle %s, line %d: unknown key %.*s",
                    b->file, lineno, (int) klen, line);
    }
  }
  return HTS_TRUE;
}

/* Is the jar file newer than b's file, so its records win? */
static hts_boolean bundle_under(const bundle *b, const char *jar) {
  hts_filetime_t jt, bt;

  return jar != NULL && hts_file_mtime(jar, &jt) &&
         hts_file_mtime(b->file, &bt) &&
         (jt.sec > bt.sec || (jt.sec == bt.sec && jt.nsec > bt.nsec));
}

/* Read b's file, set its scope, and load its cookies through others. */
static hts_boolean bundle_load(httrackp *opt, bundle *b, const char *jar,
                               t_cookie *others, char *err, size_t err_size) {
  const char *const dir = StringNotEmpty(opt->bundle_dir)
                              ? StringBuff(opt->bundle_dir)
                              : StringBuff(opt->path_log);
  const size_t dlen = strlen(dir);
  hts_boolean scope_given, has_jar;
  cookie_filter filter;
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
  if (!bundle_parse(opt, b, fp, &scope_given, &has_jar, err, err_size)) {
    fclose(fp);
    return HTS_FALSE;
  }
  b->https_only = HTS_TRUE;
  for (i = 0; i < b->nstart; i++)
    b->https_only = b->https_only && strfield(b->start_urls[i], "https:");
  for (i = 0; !scope_given && i < b->nstart + b->nurls; i++) {
    if (!bundle_scope_url(b, i < b->nstart ? b->start_urls[i]
                                           : b->urls[i - b->nstart])) {
      fclose(fp);
      BUNDLE_FAIL("%s: cannot work out the bundle's scope", b->file);
    }
  }
  memset(&filter, 0, sizeof(filter));
  filter.covers = bundle_in_scope;
  filter.scope = b;
  filter.others = others;
  filter.under = bundle_under(b, jar);
  if (has_jar && opt->cookie != NULL)
    cookie_load_stream(opt, opt->cookie, fp, b->file, &filter);
  else if (has_jar)
    hts_log_print(opt, LOG_WARNING,
                  "Cookies are disabled, so the cookies of %s are not sent",
                  b->file);
  fclose(fp);
  if (filter.dropped != 0)
    hts_log_print(opt, LOG_WARNING,
                  "Bundle %s: %d cookie(s) outside its scope left out; a "
                  "scope line keeps them",
                  b->file, filter.dropped);
  if (filter.clash[0] != '\0')
    BUNDLE_FAIL("%s: cookie '%s' has another value in another bundle", b->file,
                filter.clash);
  if (filter.lost != 0)
    BUNDLE_FAIL("%s: the cookie jar is full, or the file could not be read to "
                "its end",
                b->file);
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

hts_boolean hts_bundle_load_all(httrackp *opt, const char *jar, char *err,
                                size_t err_size) {
  hts_bundle_state *const st = (hts_bundle_state *) opt->bundle_state;
  t_cookie *others;
  hts_boolean ok = HTS_TRUE;
  size_t i, j;

  if (st == NULL)
    return HTS_TRUE;
  others = calloct(1, sizeof(t_cookie));
  if (others == NULL)
    BUNDLE_FAIL("login bundles: out of memory");
  others->max_len = sizeof(others->data);
  for (i = 0; i < st->count && ok; i++)
    ok = bundle_load(opt, &st->items[i], jar, others, err, err_size);
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
  freet(others);
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
        req->user_agent_send = 1;
      }
      if (StringNotEmpty(b->headers) &&
          (!b->https_only || strfield(adr, "https://")))
        req->headers = StringBuff(b->headers);
      return;
    }
  }
}

void hts_bundle_dump(httrackp *opt) {
  const hts_bundle_state *const st = (hts_bundle_state *) opt->bundle_state;
  size_t i, j;

  for (i = 0; st != NULL && i < st->count; i++) {
    const bundle *const b = &st->items[i];
    const char *p;

    printf("bundle %s\n", b->name);
    if (b->ua != NULL)
      printf("ua %s\n", b->ua);
    for (j = 0; j < b->nurls; j++)
      printf("url %s\n", b->urls[j]);
    for (j = 0; j < b->nscope; j++)
      printf("scope %s\n", b->scope[j]);
    for (p = StringBuff(b->headers); *p != '\0';) {
      const size_t len = strcspn(p, "\r\n");

      printf("header %.*s\n", (int) len, p);
      p += len;
      p += strspn(p, "\r\n");
    }
  }
}
