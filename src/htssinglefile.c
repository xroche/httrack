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
/* --single-file asset inliner. See htssinglefile.h.
   This pass substitutes the marks htsparse left while saving, and parses no
   HTML or CSS. An unresolvable mark keeps its link, so a rerun is a no-op. */
/* ------------------------------------------------------------ */

#include "htssinglefile.h"

#include "htscore.h"
#include "htslib.h"
#include "htsio.h"
#include "htssafe.h"
#include "htsrandom.h"
#include "htstools.h"
#include "htswizard.h"

#include <stdint.h>
#include <stdlib.h>

/* Inlinable MIME classes. */
#define SF_C_IMAGE 1
#define SF_C_FONT 2
#define SF_C_CSS 4
#define SF_C_JS 8

/* An asset that carries marks of its own (a stylesheet importing another) is
   expanded before encoding; deeper than this it keeps its links. */
#define SF_MAX_CSS_DEPTH 4

/* Bounds on hostile input: a longer reference, or one resolving to more
   components than this, is left alone. */
#define SF_MAX_REF 4096
#define SF_MAX_COMPONENTS 128

/* Over-cap assets reported per pass; beyond that the log would carry one line
   per referencing page. */
#define SF_MAX_WARN 32

typedef struct sf_ctx {
  httrackp *opt;
  const char *root; /* mirror root, '/'-separated, no trailing separator */
  size_t root_len;
  const char *page_dir; /* directory holding the page being rewritten */
  int *warn_budget;
  LLint budget; /* bytes this page may still inline */
  int inlined;
  int marks; /* marks consumed: the file changed even when none inlined */
} sf_ctx;

/* ------------------------------------------------------------ */
/* Marks                                                         */
/* ------------------------------------------------------------ */

/* This run's mark secret. In memory only: persisting it would put a forgery
   key inside a tree people publish. */
struct hts_singlefile_state {
  char intro[sizeof(SINGLEFILE_MARK_INTRO) + SINGLEFILE_SECRET_HEX];
};

void singlefile_free(httrackp *opt) {
  if (opt != NULL && opt->singlefile_state != NULL) {
    freet(opt->singlefile_state);
    opt->singlefile_state = NULL;
  }
}

const char *singlefile_intro(httrackp *opt) {
  static const char hex[] = "0123456789abcdef";
  struct hts_singlefile_state *st;
  unsigned char raw[SINGLEFILE_SECRET_HEX / 2];
  size_t i;

  if (opt->singlefile_state != NULL)
    return ((struct hts_singlefile_state *) opt->singlefile_state)->intro;
  if (!hts_random_bytes(raw, sizeof(raw))) {
    /* A guessable mark is a forgeable one, so there is no safe fallback. */
    hts_log_print(opt, LOG_ERROR,
                  "single-file: no secure random source, nothing will be "
                  "inlined");
    return NULL;
  }
  st = (struct hts_singlefile_state *) calloct(1, sizeof(*st));
  if (st == NULL)
    return NULL;
  strcpybuff(st->intro, SINGLEFILE_MARK_INTRO);
  for (i = 0; i < sizeof(raw); i++) {
    st->intro[sizeof(SINGLEFILE_MARK_INTRO) - 1 + i * 2] = hex[raw[i] >> 4];
    st->intro[sizeof(SINGLEFILE_MARK_INTRO) + i * 2] = hex[raw[i] & 15];
  }
  opt->singlefile_state = st;
  return st->intro;
}

hts_boolean singlefile_may_mark(httrackp *opt, hts_span body) {
  const char *const intro = singlefile_intro(opt);

  if (intro == NULL)
    return HTS_FALSE;
  if (hts_memstr(body, hts_span_of(intro)) != NULL) {
    hts_log_print(opt, LOG_ERROR,
                  "single-file: document already carries this run's mark, "
                  "leaving it alone");
    return HTS_FALSE;
  }
  return HTS_TRUE;
}

const char *singlefile_mark(httrackp *opt, hts_buf buf, char cls,
                            size_t reflen) {
  const char *const intro = singlefile_intro(opt);

  buf.p[0] = '\0';
  /* Nothing sf_parse_mark would refuse: an unread mark stays in the page as
     text and takes its reference with it. */
  if (intro != NULL && reflen <= SINGLEFILE_MAX_SPAN)
    snprintf(buf.p, buf.size, "%s.%c.%d", intro, cls, (int) reflen);
  return buf.p;
}

/* These (tag, attribute) pairs name a page or media stream, never an asset.
   A NULL tag matches any. Other references go by MIME type alone. */
static const htspair_t sf_deny_rules[] = {
    {"a", "href"},      {"area", "href"},   {"iframe", "src"},
    {"frame", "src"},   {"applet", "code"}, {NULL, "longdesc"},
    {NULL, "usemap"},   {NULL, "archive"},  {NULL, "profile"},
    {NULL, "codebase"}, {NULL, NULL},
};

/* Does the start tag at tag_name carry a rel naming a stylesheet? 1 yes, 0 a
   rel that names something else, -1 no rel at all. */
static int sf_rel_is_stylesheet(const char *tag_name) {
  size_t i;

  for (i = 0; i < HTS_URLMAXSIZE && tag_name[i] != '\0' && tag_name[i] != '>';
       i++) {
    const int want = (int) sizeof("stylesheet") - 1;
    const char *value;
    int p, j, len;

    if (i != 0 && !is_space(tag_name[i - 1]))
      continue;
    p = rech_tageq(tag_name + i, "rel");
    if (p == 0)
      continue;
    /* rel's own value only, as a token: an unbounded search reads the next
       tag's rel, and a stray "stylesheet" there inlines a page. */
    len = rech_endtoken(tag_name + i + p, &value);
    /* Cut at this tag's '>'. A tag left unclosed has none, so the value runs
       to the document NUL. */
    for (j = 0; j < len; j++) {
      if (value[j] == '>') {
        len = j;
        break;
      }
    }
    for (j = 0; j < len;) {
      int start;

      while (j < len && is_space(value[j]))
        j++;
      start = j;
      while (j < len && !is_space(value[j]))
        j++;
      if (j - start == want &&
          hts_strncasecmp(value + start, "stylesheet", want) == 0)
        return 1;
    }
    return 0;
  }
  return -1;
}

/* A NULL tag means "any tag" here, but ends htswizard's table. */
static hts_boolean sf_denied(const char *tag_name, const char *attr) {
  size_t i;

  for (i = 0; i < sizeof(sf_deny_rules) / sizeof(sf_deny_rules[0]); i++) {
    const htspair_t *const r = &sf_deny_rules[i];

    if (r->attr == NULL)
      break;
    if (r->tag != NULL && !hts_cmp_tag_token(tag_name, r->tag))
      continue;
    if (hts_cmp_tag_token(attr, r->attr))
      return HTS_TRUE;
  }
  return HTS_FALSE;
}

char singlefile_ref_class(const char *tag_name, const char *attr,
                          const char *body) {
  char prevc;

  if (attr == NULL)
    return 0;
  if (sf_denied(tag_name, attr))
    return 0;
  prevc = html_prevc(attr, body);
  if (tag_name == NULL) {
    /* A stylesheet or a script body: only @import names another stylesheet. */
    return rech_tageq_at(attr, prevc, "import") != 0 ? SINGLEFILE_CLASS_CSS
                                                     : SINGLEFILE_CLASS_ANY;
  }
  if (hts_cmp_tag_token(tag_name, "script") &&
      rech_tageq_at(attr, prevc, "src") != 0)
    return SINGLEFILE_CLASS_JS;
  if (hts_cmp_tag_token(tag_name, "link") &&
      rech_tageq_at(attr, prevc, "href") != 0) {
    const int rel = sf_rel_is_stylesheet(tag_name);

    /* No rel names nothing the browser would load, so it stays a link. */
    if (rel < 0)
      return 0;
    return rel > 0 ? SINGLEFILE_CLASS_CSS : SINGLEFILE_CLASS_ANY;
  }
  return SINGLEFILE_CLASS_ANY;
}

/* ------------------------------------------------------------ */
/* Paths                                                         */
/* ------------------------------------------------------------ */

static int sf_is_sep(int c) { return c == '/' || c == '\\'; }

/* Copy path into out with '/' separators and no trailing one. */
static void sf_normalize_path(const char *path, String *out) {
  size_t i;

  StringClear(*out);
  for (i = 0; path[i] != '\0'; i++)
    StringAddchar(*out, sf_is_sep((unsigned char) path[i]) ? '/' : path[i]);
  while (StringLength(*out) > 1 && StringRight(*out, 1) == '/')
    StringPopRight(*out);
}

/* Directory part of a '/'-separated path (empty when it has none). */
static void sf_dirname(const char *path, String *out) {
  const char *last = NULL;
  const char *p;

  for (p = path; *p != '\0'; p++) {
    if (*p == '/')
      last = p;
  }
  StringClear(*out);
  if (last != NULL)
    StringMemcat(*out, path, (size_t) (last - path));
}

/* Append to_path as seen from from_dir, both '/'-separated under one root. */
static void sf_relative_from(const char *from_dir, const char *to_path,
                             String *out) {
  size_t common = 0, i = 0, j, nup = 0;

  while (from_dir[i] != '\0' && to_path[i] != '\0' &&
         from_dir[i] == to_path[i]) {
    if (from_dir[i] == '/')
      common = i + 1;
    i++;
  }
  /* from_dir is an ancestor of to_path: nothing to climb, and common must not
     advance past from_dir's terminator. */
  if (from_dir[i] == '\0' && to_path[i] == '/') {
    StringCat(*out, to_path + i + 1);
    return;
  }
  for (j = common; from_dir[j] != '\0'; j++) {
    if (from_dir[j] == '/')
      nup++;
  }
  if (from_dir[common] != '\0')
    nup++;
  for (j = 0; j < nup; j++)
    StringMemcat(*out, "../", 3);
  StringCat(*out, to_path + common);
}

/* Resolve the mirrored reference ref against base_dir into out, a
   '/'-separated path under ctx->root naming an existing regular file. Returns
   HTS_FALSE for a fragment, an absolute or scheme-bearing URL (data:
   included), a reference climbing out of the mirror, or a missing target.
   Containment is lexical, so a symlink planted in the output directory is
   followed. Fetched content cannot create one. */
static hts_boolean sf_resolve(const sf_ctx *ctx, const char *base_dir,
                              hts_span ref, String *out) {
  char raw[SF_MAX_REF];
  char decoded[SF_MAX_REF];
  size_t stack[SF_MAX_COMPONENTS];
  String acc = STRING_EMPTY;
  hts_boolean ok = HTS_TRUE;
  size_t i, n;
  int sp = 0, part;
  const hts_span r = hts_span_trim(ref, " \t\r\n\f", " \t\r\n\f");

  /* A mirrored name carries no query or fragment. */
  n = hts_span_find_any(r, "#?");
  if (n == 0)
    return HTS_FALSE;
  /* Site-root-relative: no base to resolve it against. */
  if (sf_is_sep((unsigned char) r.p[0]))
    return HTS_FALSE;
  for (i = 0; i < n; i++) { /* a colon before any separator marks a scheme */
    const unsigned char c = (unsigned char) r.p[i];

    if (c == ':')
      return HTS_FALSE;
    if (sf_is_sep(c) ||
        (!hts_isalnum((unsigned char) c) && c != '+' && c != '.' && c != '-'))
      break;
  }
  if (!hts_buf_put_span(HTS_BUF(raw), hts_span_at(r.p, n)))
    return HTS_FALSE;
  unescape_amp(raw); /* HTML entities first, then the percent escapes */
  unescape_http(decoded, sizeof(decoded), raw);
  if (decoded[0] == '\0')
    return HTS_FALSE;
  /* base_dir must be the root or a directory under it, matched on a component
     boundary: a bare prefix test would also accept a sibling <root>foo. */
  if (strncmp(base_dir, ctx->root, ctx->root_len) != 0 ||
      (base_dir[ctx->root_len] != '\0' &&
       !sf_is_sep((unsigned char) base_dir[ctx->root_len])))
    return HTS_FALSE;

  /* Walk the page's directory relative to the root, then the reference. */
  StringClear(acc);
  for (part = 0; ok && part < 2; part++) {
    const char *p = part == 0 ? base_dir + ctx->root_len : decoded;

    while (ok && *p != '\0') {
      const char *const start = p;
      size_t clen;

      while (*p != '\0' && !sf_is_sep((unsigned char) *p))
        p++;
      clen = (size_t) (p - start);
      if (*p != '\0')
        p++;
      if (clen == 0 || (clen == 1 && start[0] == '.'))
        continue;
      if (clen == 2 && start[0] == '.' && start[1] == '.') {
        if (sp == 0) { /* would climb out of the mirror */
          ok = HTS_FALSE;
          break;
        }
        sp--;
        StringSetLength(acc, sp > 0 ? (int) stack[sp - 1] : 0);
        StringBuffRW(acc)[StringLength(acc)] = '\0';
        continue;
      }
      if (sp == SF_MAX_COMPONENTS) {
        ok = HTS_FALSE;
        break;
      }
      if (StringLength(acc) > 0)
        StringAddchar(acc, '/');
      StringMemcat(acc, start, clen);
      stack[sp++] = StringLength(acc);
    }
  }
  if (ok && StringLength(acc) > 0) {
    StringCopy(*out, ctx->root);
    StringCat(*out, "/");
    StringCat(*out, StringBuff(acc));
    ok = fexist_utf8(StringBuff(*out)) ? HTS_TRUE : HTS_FALSE;
  } else {
    ok = HTS_FALSE;
  }
  StringFree(acc);
  return ok;
}

/* ------------------------------------------------------------ */
/* MIME, reading, encoding                                       */
/* ------------------------------------------------------------ */

/* Inlinable class of the file at path, or 0; out receives the type to
   advertise in the data: URI. */
static int sf_mime_class(httrackp *opt, const char *path, hts_buf out) {
  const char *const mime = out.p;

  out.p[0] = '\0';
  if (!guess_httptype_sized(opt, out.p, out.size, path) || mime[0] == '\0')
    return 0;
  if (hts_starts_with_ci(mime, "image/"))
    return SF_C_IMAGE;
  if (hts_starts_with_ci(mime, "font/") ||
      hts_starts_with_ci(mime, "application/font") ||
      hts_starts_with_ci(mime, "application/x-font") ||
      hts_streq_ci(mime, "application/vnd.ms-fontobject"))
    return SF_C_FONT;
  if (hts_streq_ci(mime, "text/css"))
    return SF_C_CSS;
  if (is_javascript_mime_type(mime))
    return SF_C_JS;
  return 0;
}

/* readfile2_utf8() with the base64 size limit applied. */
static char *sf_readfile(const char *path, size_t *size) {
  LLint len = 0;
  char *adr;

  *size = 0;
  adr = readfile2_utf8(path, &len);
  if (adr == NULL)
    return NULL;
  if (len < 0 || len > SINGLEFILE_HARD_MAX_SIZE) {
    freet(adr);
    return NULL;
  }
  *size = llint_to_size_t(len);
  return adr;
}

static hts_boolean sf_append_base64(String *out, hts_span data) {
  /* code64() emits exactly one padded 4-byte group per 3 input bytes. */
  const size_t b64len = ((data.len + 2) / 3) * 4;
  char *buf;

  if (data.len > SINGLEFILE_HARD_MAX_SIZE)
    return HTS_FALSE;
  buf = (char *) malloct(b64len + 1);
  if (buf == NULL)
    return HTS_FALSE;
  code64(data, hts_buf_at(buf, b64len + 1), HTS_FALSE);
  StringMemcat(*out, buf, b64len);
  freet(buf);
  return HTS_TRUE;
}

/* Append s, escaping what an unquoted CSS url(), an HTML attribute or
   an appended fragment could choke on. preencoded text already carries the
   document's own '%' and '&' escapes, and encoding those again changes it. */
static void sf_append_escaped(String *out, hts_span s, hts_boolean preencoded) {
  static const char hex[] = "0123456789ABCDEF";
  size_t i;

  for (i = 0; i < s.len; i++) {
    const unsigned char c = (unsigned char) s.p[i];

    if (c <= 32 || c >= 127 || c == '"' || c == '\'' || c == '(' || c == ')' ||
        c == '\\' || c == '<' || c == '>' || c == '#' ||
        (!preencoded && (c == '%' || c == '&'))) {
      StringAddchar(*out, '%');
      StringAddchar(*out, hex[c >> 4]);
      StringAddchar(*out, hex[c & 15]);
    } else {
      StringAddchar(*out, (char) c);
    }
  }
}

/* A marked reference resolved to a file of another type, which is a bug. */
static void sf_warn_mismatch(sf_ctx *ctx, const char *path, char want,
                             const char *mime) {
  if (*ctx->warn_budget <= 0)
    return;
  (*ctx->warn_budget)--;
  hts_log_print(
      ctx->opt, LOG_WARNING,
      "single-file: %s is %s but was referenced as %s, left as a link", path,
      mime, want == SINGLEFILE_CLASS_CSS ? "a stylesheet" : "a script");
}

static void sf_warn_oversize(sf_ctx *ctx, const char *path, LLint size,
                             LLint cap) {
  if (*ctx->warn_budget <= 0)
    return;
  if (--(*ctx->warn_budget) == 0) {
    hts_log_print(ctx->opt, LOG_WARNING,
                  "single-file: further over-cap assets not reported");
    return;
  }
  hts_log_print(ctx->opt, LOG_WARNING,
                "single-file: %s left as a link (" LLintP
                " bytes, over the " LLintP "-byte cap)",
                path, size, cap);
}

/* ------------------------------------------------------------ */
/* Substitution                                                  */
/* ------------------------------------------------------------ */

static void sf_expand(sf_ctx *ctx, const char *base_dir, int depth,
                      hts_span body, String *out);
static size_t sf_strip_marks(const char *intro, hts_buf body);

/* Replace the reference ref, resolved against base_dir, with its
   data: URI appended to out. rebase_dir re-expresses an un-inlinable asset
   relative to that directory, for a lenient resolver's base. Returns HTS_TRUE
   if out received a data: URI. */
static hts_boolean sf_inline(sf_ctx *ctx, const char *base_dir, hts_span ref,
                             const char *rebase_dir, int depth, char want,
                             hts_boolean *resolved, String *out) {
  String path = STRING_EMPTY;
  char mime[HTS_MIMETYPE_SIZE];
  char *file;
  size_t file_len = 0;
  LLint size, cap;
  int cls;
  hts_boolean done = HTS_FALSE;

  *resolved = sf_resolve(ctx, base_dir, ref, &path);
  if (!*resolved) {
    StringFree(path);
    return HTS_FALSE;
  }
  cls = sf_mime_class(ctx->opt, StringBuff(path), HTS_BUF(mime));
  /* The referencing context decides when the name carries no usable type.
     A contradiction between the two is a bug and is not inlined. */
  if (want != SINGLEFILE_CLASS_ANY) {
    const int wanted = want == SINGLEFILE_CLASS_CSS ? SF_C_CSS : SF_C_JS;

    if (cls == 0) {
      cls = wanted;
      strlcpybuff(mime, wanted == SF_C_CSS ? "text/css" : "text/javascript",
                  sizeof(mime));
    } else if (cls != wanted) {
      sf_warn_mismatch(ctx, StringBuff(path), want, mime);
      cls = 0;
    }
  }
  cap = ctx->opt->single_file_max_size;
  size = fsize_utf8(StringBuff(path));
  if (cls == 0 || size < 0)
    goto fallback;
  if (size > cap || size > SINGLEFILE_HARD_MAX_SIZE) {
    sf_warn_oversize(ctx, StringBuff(path), size, cap);
    goto fallback;
  }
  if (size > ctx->budget) { /* the page has inlined all it may */
    sf_warn_oversize(ctx, StringBuff(path), size, ctx->budget);
    goto fallback;
  }
  file = sf_readfile(StringBuff(path), &file_len);
  if (file == NULL)
    goto fallback;
  /* Charged before the nested expansion: an @import chain otherwise spends
     what its ancestors already claimed, and the budget ends up negative. */
  ctx->budget -= size;
  /* Encode into a scratch String, so a failed encode leaves out unchanged. */
  {
    String payload = STRING_EMPTY;

    StringClear(payload);
    if ((cls & (SF_C_CSS | SF_C_JS)) != 0 && depth < SF_MAX_CSS_DEPTH) {
      String nested = STRING_EMPTY;
      String nested_dir = STRING_EMPTY;

      sf_dirname(StringBuff(path), &nested_dir);
      StringClear(nested);
      sf_expand(ctx, StringBuff(nested_dir), depth + 1,
                hts_span_at(file, file_len), &nested);
      done = sf_append_base64(
          &payload, hts_span_at(StringBuff(nested), StringLength(nested)));
      StringFree(nested);
      StringFree(nested_dir);
    } else {
      /* Past the nesting cap the body is embedded as it stands, so a mark it
         still carries would ride into the payload and publish the secret. */
      if ((cls & (SF_C_CSS | SF_C_JS)) != 0) {
        const char *const intro = singlefile_intro(ctx->opt);

        if (intro != NULL)
          file_len = sf_strip_marks(intro, hts_buf_at(file, file_len));
      }
      done = sf_append_base64(&payload, hts_span_at(file, file_len));
    }
    if (done) {
      StringCat(*out, "data:");
      StringCat(*out, mime);
      StringCat(*out, ";base64,");
      StringMemcat(*out, StringBuff(payload), StringLength(payload));
    }
    StringFree(payload);
  }
  freet(file);
  if (done) {
    ctx->inlined++;
    StringFree(path);
    return HTS_TRUE;
  }
  ctx->budget += size;

fallback:
  if (rebase_dir != NULL) {
    String rel = STRING_EMPTY;

    StringClear(rel);
    sf_relative_from(rebase_dir, StringBuff(path), &rel);
    if (StringLength(rel) > 0) {
      sf_append_escaped(out, hts_span_at(StringBuff(rel), StringLength(rel)),
                        HTS_FALSE);
      done = HTS_TRUE;
    }
    StringFree(rel);
  }
  StringFree(path);
  return done;
}

/* A mark htsparse wrote always names a file it had just saved, so failing to
   resolve one means the mirror moved under us since the page was written. */
static void sf_warn_unresolved(sf_ctx *ctx, hts_span ref) {
  if (*ctx->warn_budget <= 0)
    return;
  (*ctx->warn_budget)--;
  /* Clipped: a span runs far past anything that could name a file. */
  hts_log_print(ctx->opt, LOG_WARNING,
                "single-file: marked reference \"%.*s\" resolves to no "
                "mirrored file, left as a link",
                (int) (ref.len > 256 ? 256 : ref.len), ref.p);
}

/* Parse the mark at the start of s: "<intro>.<class>.<len>". Returns its
   length, or 0 if it is not one. intro is SINGLEFILE_INTRO_LEN bytes. A NULL
   intro matches any well-formed secret, so a later run can delete marks an
   interrupted one left behind without its secret. */
static size_t sf_parse_mark(const char *intro, hts_span s, char *cls,
                            size_t *reflen) {
  size_t i = SINGLEFILE_INTRO_LEN;
  size_t n = 0;
  int digits = 0;

  if (s.len < SINGLEFILE_INTRO_LEN)
    return 0;
  if (intro != NULL) {
    if (memcmp(s.p, intro, SINGLEFILE_INTRO_LEN) != 0)
      return 0;
  } else {
    size_t k;

    if (memcmp(s.p, SINGLEFILE_MARK_INTRO, sizeof(SINGLEFILE_MARK_INTRO) - 1) !=
        0)
      return 0;
    for (k = sizeof(SINGLEFILE_MARK_INTRO) - 1; k < SINGLEFILE_INTRO_LEN; k++) {
      if (!hts_isxdigit((unsigned char) s.p[k]))
        return 0;
    }
  }
  if (i + 3 >= s.len || s.p[i] != '.')
    return 0;
  *cls = s.p[++i];
  if (*cls != SINGLEFILE_CLASS_ANY && *cls != SINGLEFILE_CLASS_CSS &&
      *cls != SINGLEFILE_CLASS_JS)
    return 0;
  if (++i >= s.len || s.p[i++] != '.')
    return 0;
  while (i < s.len && s.p[i] >= '0' && s.p[i] <= '9') {
    if (n > SINGLEFILE_MAX_SPAN) /* saturated: no digit run can overflow n */
      return 0;
    n = n * 10 + (size_t) (s.p[i++] - '0');
    digits++;
  }
  if (digits == 0 || n > SINGLEFILE_MAX_SPAN)
    return 0;
  *reflen = n;
  return i;
}

/* Drop every mark from body in place; returns the new length. */
static size_t sf_strip_marks(const char *intro, hts_buf body) {
  const hts_span in = hts_span_at(body.p, body.size);
  size_t r, w = 0;

  for (r = 0; r < in.len;) {
    char cls;
    size_t reflen;
    const size_t marklen =
        sf_parse_mark(intro, hts_span_skip(in, r), &cls, &reflen);

    if (marklen != 0)
      r += marklen;
    else
      body.p[w++] = body.p[r++];
  }
  return w;
}

/* Copy body to out, replacing each marked reference by its data:
   URI, or by the bare reference when it cannot be inlined. Whatever follows
   the reference (a fragment, a kept query) stays after the data: URI. The
   substitution is in place, so an unquoted attribute gets base64 '=' padding,
   which browsers accept but the HTML grammar forbids. */
static void sf_expand(sf_ctx *ctx, const char *base_dir, int depth,
                      hts_span body, String *out) {
  const char *const intro = singlefile_intro(ctx->opt);
  size_t i = 0, flushed = 0;

  if (intro == NULL) {
    StringCatSpan(out, body);
    return;
  }
  while (i + SINGLEFILE_INTRO_LEN <= body.len) {
    hts_boolean resolved = HTS_FALSE;
    size_t marklen, reflen = 0;
    char cls = SINGLEFILE_CLASS_ANY;
    hts_span ref;

    /* Every mark opens with the intro, so skip the call on any other byte. */
    marklen = body.p[i] == intro[0]
                  ? sf_parse_mark(intro, hts_span_skip(body, i), &cls, &reflen)
                  : 0;
    /* We wrote the length, but the file may have been truncated since; a
       reference reaching back before what we have is not one of ours. */
    if (marklen == 0 || reflen > i - flushed) {
      i++;
      continue;
    }
    ref = hts_span_at(body.p + i - reflen, reflen);
    StringMemcat(*out, body.p + flushed, i - reflen - flushed);
    if (!sf_inline(ctx, base_dir, ref, depth > 0 ? ctx->page_dir : NULL, depth,
                   cls, &resolved, out)) {
      if (!resolved)
        sf_warn_unresolved(ctx, ref);
      StringCatSpan(out, ref);
    }
    ctx->marks++;
    i += marklen;
    flushed = i;
  }
  StringCatSpan(out, hts_span_skip(body, flushed));
}

/* ------------------------------------------------------------ */
/* Entry points                                                  */
/* ------------------------------------------------------------ */

hts_boolean singlefile_rewrite_html(httrackp *opt, const char *root,
                                    const char *page_path, hts_span html,
                                    LLint page_budget, String *out) {
  String nroot = STRING_EMPTY;
  String npage = STRING_EMPTY;
  String dir = STRING_EMPTY;
  int budget = SF_MAX_WARN;
  sf_ctx ctx;

  sf_normalize_path(root, &nroot);
  sf_normalize_path(page_path, &npage);
  sf_dirname(StringBuff(npage), &dir);
  ctx.opt = opt;
  ctx.root = StringBuff(nroot);
  ctx.root_len = StringLength(nroot);
  ctx.page_dir = StringBuff(dir);
  ctx.warn_budget = &budget;
  ctx.budget = page_budget;
  ctx.inlined = 0;
  ctx.marks = 0;
  sf_expand(&ctx, StringBuff(dir), 0, html, out);
  StringFree(nroot);
  StringFree(npage);
  StringFree(dir);
  /* Any mark consumed means the bytes changed, even if nothing inlined: the
     marks themselves must never survive into the delivered mirror. */
  return ctx.marks > 0 ? HTS_TRUE : HTS_FALSE;
}

/* Overwrite path with body. Spools then renames: a half-written file would
   destroy a complete mirror file that nothing is going to re-fetch. */
static hts_boolean sf_replace_file(httrackp *opt, const char *path,
                                   hts_span body) {
  char catbuff[CATBUFF_SIZE];
  String tmp = STRING_EMPTY;
  hts_boolean ok = HTS_TRUE;
  FILE *fp;

  StringCopy(tmp, path);
  StringCat(tmp, ".sfnew");
  fp = FOPEN(fconv(catbuff, sizeof(catbuff), StringBuff(tmp)), "wb");
  if (fp == NULL) {
    hts_log_print(opt, LOG_WARNING | LOG_ERRNO,
                  "single-file: could not rewrite %s", path);
    StringFree(tmp);
    return HTS_FALSE;
  }
  if (!hts_fwrite_span(body, fp))
    ok = HTS_FALSE;
#ifndef _WIN32
  /* The spool bypassed filecreate(), so set the mode it would have. */
  if (ok)
    (void) fchmod(fileno(fp), HTS_ACCESS_FILE);
#endif
  if (fclose(fp) != 0)
    ok = HTS_FALSE;
  if (ok)
    ok = hts_rename_over(opt, StringBuff(tmp), path);
  if (!ok) {
    hts_log_print(opt, LOG_ERROR, "single-file: could not rewrite %s", path);
    (void) UNLINK(StringBuff(tmp));
  }
  StringFree(tmp);
  return ok;
}

hts_boolean singlefile_rewrite_file(httrackp *opt, const char *root,
                                    const char *page_path) {
  String out = STRING_EMPTY;
  size_t len = 0;
  char *html = sf_readfile(page_path, &len);
  hts_boolean ok;

  if (html == NULL)
    return HTS_FALSE;
  StringClear(out);
  ok = singlefile_rewrite_html(opt, root, page_path, hts_span_at(html, len),
                               SINGLEFILE_MAX_PAGE_SIZE, &out);
  freet(html);
  if (ok)
    ok = sf_replace_file(opt, page_path,
                         hts_span_at(StringBuff(out), StringLength(out)));
  StringFree(out);
  return ok;
}

/* Delete every mark from the file, keeping the reference and anything that
   followed it. Returns HTS_TRUE if the file changed. */
static hts_boolean sf_strip_marks_file(httrackp *opt, const char *path) {
  size_t len = 0, w;
  char *body;
  hts_boolean ok;

  body = sf_readfile(path, &len);
  if (body == NULL)
    return HTS_FALSE;
  w = sf_strip_marks(NULL, hts_buf_at(body, len));
  if (w == len) {
    freet(body);
    return HTS_FALSE;
  }
  ok = sf_replace_file(opt, path, hts_span_at(body, w));
  freet(body);
  return ok;
}

void singlefile_process_mirror(httrackp *opt) {
  const char *root;
  int i, pages = 0;

  if (opt == NULL || !opt->single_file)
    return;
  root = StringBuff(opt->path_html_utf8);
  if (root == NULL || root[0] == '\0')
    return;
  for (i = 0; i < opt->lien_tot; i++) {
    const lien_url *const link = opt->liens[i];

    if (link == NULL || link->sav == NULL || link->sav[0] == '\0')
      continue;
    if (ishtml(opt, link->sav) != 1 || !fexist_utf8(link->sav))
      continue;
    if (singlefile_rewrite_file(opt, root, link->sav))
      pages++;
  }
  /* Strip marks only after every page is done, because a stylesheet expands
     from the marks in its copy on disk. */
  for (i = 0; i < opt->lien_tot; i++) {
    const lien_url *const link = opt->liens[i];

    if (link == NULL || link->sav == NULL || link->sav[0] == '\0')
      continue;
    if (!fexist_utf8(link->sav))
      continue;
    (void) sf_strip_marks_file(opt, link->sav);
  }
  if (pages == 0) {
    /* Nothing to inline means the saved pages carry no relative asset links,
       which is what --keep-links and --preserve produce. Say so: a silent
       no-op looks like the option was ignored. */
    hts_log_print(opt, LOG_NOTICE,
                  "single-file: no page had an inlinable asset link "
                  "(--keep-links or --preserve in use?)");
  } else {
    hts_log_print(opt, LOG_INFO,
                  "single-file: %d page(s) rewritten with inlined assets",
                  pages);
  }
}
