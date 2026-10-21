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
#include "htspsl.h"
#include "htssafe.h"
#include "htsthread.h"

#include <string.h>

/* A bundle line, the jar aside, holds fewer bytes than this. */
#define BUNDLE_LINE_MAX 8192

/* The jar room the crawl's own jar gets (see httpmirror). */
#define BUNDLE_JAR_MAX 30000

typedef struct bundle_entry {
  char name[HTS_BUNDLE_NAME_MAX];
  char file[HTS_URLMAXSIZE * 2];
  char **named; /* the start URLs naming it */
  size_t nnamed;
  char **urls; /* its url lines */
  size_t nurls;
  char **scope; /* lowercase domains, each covering its subdomains */
  size_t nscope;
  char *ua; /* NULL when it gives none */
  String headers;
  t_cookie *jar; /* NULL until loaded */
} bundle_entry;

/* The bundles of one opt, kept out of httrackp so its layout is unchanged. */
typedef struct bundle_set {
  const httrackp *opt;
  char *dir;
  bundle_entry *items;
  size_t count;
  struct bundle_set *next;
} bundle_set;

static htsmutex bundle_lock = HTSMUTEX_INIT;
static bundle_set *bundle_sets = NULL;

/* Call with bundle_lock held. */
static bundle_set *bundle_set_of(const httrackp *opt, hts_boolean create) {
  bundle_set *set;

  for (set = bundle_sets; set != NULL; set = set->next) {
    if (set->opt == opt)
      return set;
  }
  if (!create)
    return NULL;
  set = calloct(1, sizeof(*set));
  if (set != NULL) {
    set->opt = opt;
    set->next = bundle_sets;
    bundle_sets = set;
  }
  return set;
}

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

static void bundle_entry_free(bundle_entry *b) {
  strlist_free(&b->named, &b->nnamed);
  strlist_free(&b->urls, &b->nurls);
  strlist_free(&b->scope, &b->nscope);
  freet(b->ua);
  StringFree(b->headers);
  if (b->jar != NULL)
    bauth_free(b->jar);
  freet(b->jar);
}

hts_boolean hts_bundle_name_valid(const char *name) {
  const size_t len = strlen(name);

  if (len == 0 || len >= HTS_BUNDLE_NAME_MAX || name[0] == '.' ||
      strstr(name, "..") != NULL)
    return HTS_FALSE;
  return strspn(name, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
                      "0123456789._-") == len;
}

hts_bundle_suffix hts_bundle_cut(char *url, char *name, size_t name_size) {
  char *const at = strstr(url, HTS_BUNDLE_SUFFIX);
  const char *given;

  if (at == NULL)
    return HTS_BUNDLE_NONE;
  given = at + strlen(HTS_BUNDLE_SUFFIX);
  if (!hts_bundle_name_valid(given) || strlen(given) >= name_size)
    return HTS_BUNDLE_BAD;
  strlcpybuff(name, given, name_size);
  *at = '\0';
  return HTS_BUNDLE_NAMED;
}

void hts_bundle_reset(httrackp *opt) {
  bundle_set **link;

  hts_mutexlock(&bundle_lock);
  for (link = &bundle_sets; *link != NULL; link = &(*link)->next) {
    if ((*link)->opt == opt) {
      bundle_set *set = *link;
      size_t i;

      *link = set->next;
      for (i = 0; i < set->count; i++)
        bundle_entry_free(&set->items[i]);
      freet(set->items);
      freet(set->dir);
      freet(set);
      break;
    }
  }
  hts_mutexrelease(&bundle_lock);
}

hts_boolean hts_bundle_set_dir(httrackp *opt, const char *dir) {
  hts_boolean ok = HTS_FALSE;
  bundle_set *set;

  hts_mutexlock(&bundle_lock);
  set = bundle_set_of(opt, HTS_TRUE);
  if (set != NULL) {
    freet(set->dir);
    set->dir = strdupt(dir);
    ok = set->dir != NULL;
  }
  hts_mutexrelease(&bundle_lock);
  return ok;
}

hts_boolean hts_bundle_name(httrackp *opt, const char *name, const char *url) {
  hts_boolean ok = HTS_FALSE;
  bundle_set *set;

  hts_mutexlock(&bundle_lock);
  set = bundle_set_of(opt, HTS_TRUE);
  if (set != NULL) {
    bundle_entry *b = NULL;
    size_t i;

    for (i = 0; i < set->count && b == NULL; i++) {
      if (strcmp(set->items[i].name, name) == 0)
        b = &set->items[i];
    }
    if (b == NULL) {
      bundle_entry *const grown =
          realloct(set->items, (set->count + 1) * sizeof(bundle_entry));

      if (grown != NULL) {
        set->items = grown;
        b = &grown[set->count++];
        memset(b, 0, sizeof(*b));
        StringInit(b->headers);
        strcpybuff(b->name, name);
      }
    }
    ok = b != NULL && strlist_add(&b->named, &b->nnamed, url);
  }
  hts_mutexrelease(&bundle_lock);
  return ok;
}

/* Read one line of fp into line, without its LF or a CR before it. Returns 1
   for a line, 0 at the end of the file, -1 for a line too long or holding a
   NUL. */
static int bundle_readline(FILE *fp, char *line, size_t size) {
  size_t n = 0;
  int c;
  hts_boolean bad = HTS_FALSE;

  while ((c = fgetc(fp)) != EOF && c != '\n') {
    if (c == '\0' || n + 1 >= size)
      bad = HTS_TRUE;
    else
      line[n++] = (char) c;
  }
  if (c == EOF && n == 0 && !bad)
    return 0;
  if (n > 0 && line[n - 1] == '\r')
    n--;
  line[n] = '\0';
  return bad ? -1 : 1;
}

/* Is line the version line of a bundle this engine reads? */
static hts_boolean bundle_version(const char *line, hts_boolean *known) {
  static const char magic[] = "# HTTrack bundle ";
  const char *p = line;
  size_t digits;

  if (strncmp(p, magic, sizeof(magic) - 1) != 0)
    return HTS_FALSE;
  p += sizeof(magic) - 1;
  digits = strspn(p, "0123456789");
  if (digits == 0)
    return HTS_FALSE;
  *known = digits == 1 && p[0] == '1';
  p += digits;
  if (*p == '.') {
    p++;
    digits = strspn(p, "0123456789");
    if (digits == 0)
      return HTS_FALSE;
    p += digits;
  }
  return *p == '\0';
}

/* Copy into site the domain a scope gets for host: its registrable domain, or
   host itself when it has none and alone is HTS_TRUE. */
static hts_boolean bundle_site(const char *host, hts_boolean alone, char *site,
                               size_t size) {
  const char *reg = NULL;

  if (!hts_host_is_address(host, strlen(host)))
    reg = hts_psl_registrable_domain(host);
  if (reg == NULL && !alone)
    return HTS_FALSE;
  if (reg == NULL)
    reg = host;
  if (strlen(reg) >= size)
    return HTS_FALSE;
  strlcpybuff(site, reg, size);
  return HTS_TRUE;
}

static hts_boolean bundle_in_scope(const bundle_entry *b, const char *host) {
  size_t i;

  for (i = 0; i < b->nscope; i++) {
    if (cookie_domain_match(b->scope[i], host))
      return HTS_TRUE;
  }
  return HTS_FALSE;
}

static hts_boolean bundle_scope_add(bundle_entry *b, const char *domain) {
  size_t i;

  for (i = 0; i < b->nscope; i++) {
    if (strcmp(b->scope[i], domain) == 0)
      return HTS_TRUE;
  }
  return strlist_add(&b->scope, &b->nscope, domain);
}

/* Add the scope of a URL naming b or listed in it. */
static hts_boolean bundle_scope_url(bundle_entry *b, const char *url) {
  char host[256];
  char site[256];
  hts_boolean https;

  return cookie_url_host(url, host, sizeof(host), &https) &&
         bundle_site(host, HTS_TRUE, site, sizeof(site)) &&
         bundle_scope_add(b, site);
}

#define BUNDLE_FAIL(...)                                                       \
  do {                                                                         \
    snprintf(err, err_size, __VA_ARGS__);                                      \
    return HTS_FALSE;                                                          \
  } while (0)

/* Read b's header lines up to "cookies", then its jar. */
static hts_boolean bundle_parse(httrackp *opt, bundle_entry *b, FILE *fp,
                                hts_boolean *scope_given, char *err,
                                size_t err_size) {
  char BIGSTK line[BUNDLE_LINE_MAX];
  int lineno = 0;
  int got;
  hts_boolean known;

  got = bundle_readline(fp, line, sizeof(line));
  lineno++;
  {
    static const char bom[] = "\xEF\xBB\xBF";
    const char *const first =
        got > 0 && strncmp(line, bom, sizeof(bom) - 1) == 0
            ? line + sizeof(bom) - 1
            : line;

    if (got <= 0 || !bundle_version(first, &known))
      BUNDLE_FAIL("%s: not an HTTrack bundle, the first line must be "
                  "\"# HTTrack bundle 1\"",
                  b->file);
  }
  if (!known)
    BUNDLE_FAIL("%s: this bundle version is not supported: %s", b->file, line);
  *scope_given = HTS_FALSE;
  while ((got = bundle_readline(fp, line, sizeof(line))) != 0) {
    const char *value;
    size_t klen;

    lineno++;
    if (got < 0)
      BUNDLE_FAIL("%s, line %d: line too long or holding a NUL", b->file,
                  lineno);
    if (line[0] == '\0' || line[0] == '#')
      continue;
    klen = strcspn(line, " \t");
    value = line + klen;
    value += strspn(value, " \t");
    if (klen == 7 && strncmp(line, "cookies", klen) == 0) {
      int refused;

      if (*value != '\0')
        BUNDLE_FAIL("%s, line %d: \"cookies\" takes no value", b->file, lineno);
      b->jar = calloct(1, sizeof(t_cookie));
      if (b->jar == NULL)
        BUNDLE_FAIL("%s: out of memory", b->file);
      b->jar->max_len = BUNDLE_JAR_MAX;
      refused = cookie_load_stream(opt, b->jar, fp, b->file);
      if (refused == COOKIE_LOAD_KEEP)
        BUNDLE_FAIL("%s: out of memory", b->file);
      if (refused != 0)
        BUNDLE_FAIL("%s: %d cookie line(s) could not be read, see the log",
                    b->file, refused);
      return HTS_TRUE;
    }
    if (*value == '\0')
      BUNDLE_FAIL("%s, line %d: no value", b->file, lineno);
    if (klen == 3 && strncmp(line, "url", klen) == 0) {
      char host[256];
      hts_boolean https;

      if (strlen(value) >= HTS_URLMAXSIZE ||
          strstr(value, HTS_BUNDLE_SUFFIX) != NULL ||
          !cookie_url_host(value, host, sizeof(host), &https) ||
          host[0] == '\0')
        BUNDLE_FAIL("%s, line %d: invalid url: %.64s", b->file, lineno, value);
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

      if (why != NULL)
        BUNDLE_FAIL("%s, line %d: refused header (%s): %.64s", b->file, lineno,
                    why, value);
      StringCat(b->headers, value);
      StringCat(b->headers, "\r\n");
    } else if (klen == 5 && strncmp(line, "scope", klen) == 0) {
      char dom[256];

      if (value[0] == '.')
        value++;
      if (strlen(value) >= sizeof(dom) ||
          strspn(value, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
                        "0123456789.-") != strlen(value) ||
          value[0] == '\0')
        BUNDLE_FAIL("%s, line %d: invalid scope: %.64s", b->file, lineno,
                    value);
      strcpybuff(dom, value);
      hts_lowcase(dom);
      if (!hts_host_is_address(dom, strlen(dom)) &&
          hts_psl_registrable_domain(dom) == NULL)
        BUNDLE_FAIL("%s, line %d: scope %s is a public suffix", b->file, lineno,
                    dom);
      if (!*scope_given) {
        strlist_free(&b->scope, &b->nscope);
        *scope_given = HTS_TRUE;
      }
      if (!bundle_scope_add(b, dom))
        BUNDLE_FAIL("%s: out of memory", b->file);
    } else {
      hts_log_print(opt, LOG_WARNING, "Bundle %s, line %d: unknown key %.*s",
                    b->file, lineno, (int) klen, line);
    }
  }
  return HTS_TRUE;
}

/* Set b's default scope, then drop its cookies outside the scope. */
static hts_boolean bundle_scope(httrackp *opt, bundle_entry *b,
                                hts_boolean scope_given) {
  size_t i;
  char *rec;

  if (!scope_given) {
    for (i = 0; i < b->nnamed; i++) {
      if (!bundle_scope_url(b, b->named[i]))
        return HTS_FALSE;
    }
    for (i = 0; i < b->nurls; i++) {
      if (!bundle_scope_url(b, b->urls[i]))
        return HTS_FALSE;
    }
    for (rec = b->jar != NULL ? b->jar->data : NULL;
         rec != NULL && *rec != '\0'; rec = cookie_nextfield(rec)) {
      char buffer[8192];
      const char *dom = cookie_get(buffer, rec, 0);
      char site[256];

      if (dom[0] == '.')
        dom++;
      if (bundle_site(dom, HTS_FALSE, site, sizeof(site)) &&
          !bundle_scope_add(b, site))
        return HTS_FALSE;
    }
  }
  rec = b->jar != NULL ? b->jar->data : NULL;
  while (rec != NULL && *rec != '\0') {
    char buffer[8192];
    const char *dom = cookie_get(buffer, rec, 0);

    if (dom[0] == '.')
      dom++;
    if (bundle_in_scope(b, dom)) {
      rec = cookie_nextfield(rec);
    } else {
      char name[8192];

      hts_log_print(opt, LOG_WARNING,
                    "Bundle %s: cookie '%.64s' for %s is outside the bundle's "
                    "scope, refused",
                    b->file, cookie_get(name, rec, 5), dom);
      cookie_delete(rec, b->jar->max_len - (size_t) (rec - b->jar->data),
                    (size_t) (cookie_nextfield(rec) - rec));
    }
  }
  return HTS_TRUE;
}

static hts_boolean bundle_scopes_overlap(const bundle_entry *a,
                                         const bundle_entry *b) {
  size_t i, j;

  for (i = 0; i < a->nscope; i++) {
    for (j = 0; j < b->nscope; j++) {
      if (cookie_domain_match(a->scope[i], b->scope[j]) ||
          cookie_domain_match(b->scope[j], a->scope[i]))
        return HTS_TRUE;
    }
  }
  return HTS_FALSE;
}

static hts_boolean bundle_same_identity(const bundle_entry *a,
                                        const bundle_entry *b) {
  if ((a->ua == NULL) != (b->ua == NULL))
    return HTS_FALSE;
  if (a->ua != NULL && strcmp(a->ua, b->ua) != 0)
    return HTS_FALSE;
  return strcmp(StringBuff(a->headers), StringBuff(b->headers)) == 0;
}

static hts_boolean bundle_load(httrackp *opt, bundle_set *set, bundle_entry *b,
                               char *err, size_t err_size) {
  const char *const dir =
      set->dir != NULL ? set->dir : StringBuff(opt->path_log);
  const size_t dlen = strlen(dir);
  hts_boolean scope_given = HTS_FALSE;
  hts_boolean ok;
  FILE *fp;

  if (!slprintfbuff(
          b->file, sizeof(b->file), "%s%s%s.hts", dir,
          dlen != 0 && dir[dlen - 1] != '/' && dir[dlen - 1] != '\\' ? "/" : "",
          b->name))
    BUNDLE_FAIL("bundle %s: the folder path is too long", b->name);
  fp = FOPEN(b->file, "rb");
  if (fp == NULL)
    BUNDLE_FAIL("%s: cannot read the login bundle: %s", b->file,
                strerror(errno));
  ok = bundle_parse(opt, b, fp, &scope_given, err, err_size);
  fclose(fp);
  if (!ok)
    return HTS_FALSE;
  if (!bundle_scope(opt, b, scope_given))
    BUNDLE_FAIL("%s: cannot work out the bundle's scope", b->file);
  return HTS_TRUE;
}

hts_boolean hts_bundle_load_all(httrackp *opt, char *err, size_t err_size) {
  hts_boolean ok = HTS_TRUE;
  bundle_set *set;
  size_t i, j;

  err[0] = '\0';
  hts_mutexlock(&bundle_lock);
  set = bundle_set_of(opt, HTS_FALSE);
  for (i = 0; set != NULL && i < set->count && ok; i++)
    ok = bundle_load(opt, set, &set->items[i], err, err_size);
  for (i = 0; set != NULL && i < set->count && ok; i++) {
    for (j = i + 1; j < set->count && ok; j++) {
      bundle_entry *const a = &set->items[i];
      bundle_entry *const b = &set->items[j];
      char name[256];

      if (bundle_scopes_overlap(a, b) && !bundle_same_identity(a, b)) {
        snprintf(err, err_size,
                 "%s and %s cover the same site with a different user agent "
                 "or headers",
                 a->file, b->file);
        ok = HTS_FALSE;
      } else if (a->jar != NULL && b->jar != NULL &&
                 cookie_jars_conflict(a->jar, b->jar, name, sizeof(name))) {
        snprintf(err, err_size,
                 "%s and %s hold two values of the cookie '%.64s'", a->file,
                 b->file, name);
        ok = HTS_FALSE;
      }
    }
  }
  hts_mutexrelease(&bundle_lock);
  return ok;
}

char *hts_bundle_urls(httrackp *opt) {
  String all = STRING_EMPTY;
  bundle_set *set;
  size_t i, j;

  hts_mutexlock(&bundle_lock);
  set = bundle_set_of(opt, HTS_FALSE);
  for (i = 0; set != NULL && i < set->count; i++) {
    for (j = 0; j < set->items[i].nurls; j++) {
      StringCat(all, set->items[i].urls[j]);
      StringCat(all, "\n");
    }
  }
  hts_mutexrelease(&bundle_lock);
  return StringNotEmpty(all) ? StringAcquire(&all) : NULL;
}

int hts_bundle_cookie_count(httrackp *opt) {
  int count = 0;
  bundle_set *set;
  size_t i;

  hts_mutexlock(&bundle_lock);
  set = bundle_set_of(opt, HTS_FALSE);
  for (i = 0; set != NULL && i < set->count; i++) {
    char *rec;

    for (rec = set->items[i].jar != NULL ? set->items[i].jar->data : NULL;
         rec != NULL && *rec != '\0'; rec = cookie_nextfield(rec))
      count++;
  }
  hts_mutexrelease(&bundle_lock);
  return count;
}

int hts_bundle_cookies(httrackp *opt, t_cookie *jar) {
  int refused = 0;
  bundle_set *set;
  size_t i;

  hts_mutexlock(&bundle_lock);
  set = bundle_set_of(opt, HTS_FALSE);
  for (i = 0; set != NULL && i < set->count; i++) {
    if (set->items[i].jar != NULL)
      refused += cookie_merge(jar, set->items[i].jar);
  }
  hts_mutexrelease(&bundle_lock);
  return refused;
}

hts_boolean hts_bundle_identity(httrackp *opt, const char *adr, const char **ua,
                                const char **headers) {
  hts_boolean changed = HTS_FALSE;
  char host[256];
  hts_boolean https;
  bundle_set *set;
  size_t i;

  hts_mutexlock(&bundle_lock);
  set = bundle_set_of(opt, HTS_FALSE);
  if (set != NULL && set->count != 0 &&
      cookie_url_host(adr, host, sizeof(host), &https)) {
    for (i = 0; i < set->count; i++) {
      const bundle_entry *const b = &set->items[i];

      if (bundle_in_scope(b, host)) {
        if (b->ua != NULL) {
          *ua = b->ua;
          changed = HTS_TRUE;
        }
        if (StringNotEmpty(b->headers))
          *headers = StringBuff(b->headers);
        break;
      }
    }
  }
  hts_mutexrelease(&bundle_lock);
  return changed;
}

void hts_bundle_dump(httrackp *opt) {
  bundle_set *set;
  size_t i, j;

  hts_mutexlock(&bundle_lock);
  set = bundle_set_of(opt, HTS_FALSE);
  for (i = 0; set != NULL && i < set->count; i++) {
    const bundle_entry *const b = &set->items[i];
    char *rec;

    printf("bundle %s\n", b->name);
    if (b->ua != NULL)
      printf("ua %s\n", b->ua);
    for (j = 0; j < b->nurls; j++)
      printf("url %s\n", b->urls[j]);
    for (j = 0; j < b->nscope; j++)
      printf("scope %s\n", b->scope[j]);
    if (StringNotEmpty(b->headers)) {
      const char *p;

      for (p = StringBuff(b->headers); *p != '\0';) {
        const size_t len = strcspn(p, "\r\n");

        printf("header %.*s\n", (int) len, p);
        p += len;
        p += strspn(p, "\r\n");
      }
    }
    for (rec = b->jar != NULL ? b->jar->data : NULL;
         rec != NULL && *rec != '\0'; rec = cookie_nextfield(rec)) {
      char dom[8192], name[8192], value[8192];

      printf("cookie %s %s=%s\n", cookie_get(dom, rec, 0),
             cookie_get(name, rec, 5), cookie_get(value, rec, 6));
    }
  }
  hts_mutexrelease(&bundle_lock);
}
