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
   Runs on the finished tree, so resolving a reference is path arithmetic
   clamped to the mirror root; anything else (absolute, scheme-bearing, already
   data:) is left alone, which is what makes a second --update run a no-op. */
/* ------------------------------------------------------------ */

#define HTS_INTERNAL_BYTECODE

#include "htssinglefile.h"

#include "htscore.h"
#include "htslib.h"
#include "htssafe.h"
#include "htstools.h"

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>

/* Inlinable MIME classes. */
#define SF_C_IMAGE 1
#define SF_C_FONT 2
#define SF_C_CSS 4
#define SF_C_JS 8
#define SF_C_ANY (SF_C_IMAGE | SF_C_FONT | SF_C_CSS | SF_C_JS)

/* Shape of an attribute value. */
#define SF_V_URL 0    /* one URL */
#define SF_V_SRCSET 1 /* HTML srcset candidate list */
#define SF_V_CSS 2    /* CSS declarations (style="...") */

/* @import chains deeper than this keep their links. */
#define SF_MAX_CSS_DEPTH 4

/* Bounds on hostile input: a longer reference, or one resolving to more
   components than this, is left alone. */
#define SF_MAX_REF 4096
#define SF_MAX_COMPONENTS 128

/* Attributes collected from one start tag before it is re-emitted. */
#define SF_MAX_ATTRS 64

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
} sf_ctx;

/* ------------------------------------------------------------ */
/* Spans                                                         */
/* ------------------------------------------------------------ */

static int sf_is_space(int c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f';
}

static int sf_is_sep(int c) { return c == '/' || c == '\\'; }

/* Case-insensitive equality between the span [p,p+n) and a lowercase literal.
 */
static hts_boolean sf_span_eq(const char *p, size_t n, const char *lit) {
  size_t i;

  for (i = 0; i < n; i++) {
    if (lit[i] == '\0' || tolower((unsigned char) p[i]) != lit[i])
      return HTS_FALSE;
  }
  return lit[n] == '\0' ? HTS_TRUE : HTS_FALSE;
}

/* Case-insensitive "does the span start with the lowercase literal". */
static hts_boolean sf_span_starts(const char *p, size_t n, const char *lit) {
  size_t i;

  for (i = 0; lit[i] != '\0'; i++) {
    if (i >= n || tolower((unsigned char) p[i]) != lit[i])
      return HTS_FALSE;
  }
  return HTS_TRUE;
}

/* Case-insensitive search for a lowercase literal inside a span. */
static hts_boolean sf_span_has(const char *p, size_t n, const char *lit) {
  const size_t l = strlen(lit);
  size_t i;

  for (i = 0; l <= n && i + l <= n; i++) {
    if (sf_span_starts(p + i, n - i, lit))
      return HTS_TRUE;
  }
  return HTS_FALSE;
}

/* ------------------------------------------------------------ */
/* Paths                                                         */
/* ------------------------------------------------------------ */

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

/* Append the path of to_path as seen from from_dir; both '/'-separated and
   under the same root. */
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

/* Resolve the mirrored reference [ref,ref+reflen) against base_dir into out, a
   '/'-separated path under ctx->root naming an existing regular file. Returns
   HTS_FALSE for anything not resolvable that way: a fragment, an absolute or
   scheme-bearing URL (which covers data:), a reference climbing out of the
   mirror, or a missing target. Containment is lexical, so a symlink planted in
   the output directory beforehand is followed; no fetched content can create
   one. */
static hts_boolean sf_resolve(const sf_ctx *ctx, const char *base_dir,
                              const char *ref, size_t reflen, String *out) {
  char raw[SF_MAX_REF];
  char decoded[SF_MAX_REF];
  size_t stack[SF_MAX_COMPONENTS];
  String acc = STRING_EMPTY;
  hts_boolean ok = HTS_TRUE;
  size_t i, n;
  int sp = 0, part;

  while (reflen > 0 && sf_is_space((unsigned char) *ref)) {
    ref++;
    reflen--;
  }
  while (reflen > 0 && sf_is_space((unsigned char) ref[reflen - 1]))
    reflen--;
  /* A mirrored name carries no query or fragment. */
  for (n = 0; n < reflen; n++) {
    if (ref[n] == '#' || ref[n] == '?')
      break;
  }
  if (n == 0 || n >= sizeof(raw))
    return HTS_FALSE;
  /* Site-root-relative: no base to resolve it against. */
  if (sf_is_sep((unsigned char) ref[0]))
    return HTS_FALSE;
  for (i = 0; i < n; i++) { /* a colon before any separator marks a scheme */
    const unsigned char c = (unsigned char) ref[i];

    if (c == ':')
      return HTS_FALSE;
    if (sf_is_sep(c) || (!isalnum(c) && c != '+' && c != '.' && c != '-'))
      break;
  }
  memcpy(raw, ref, n);
  raw[n] = '\0';
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

/* Inlinable class of the file at path, or 0; mime receives the type to
   advertise in the data: URI. */
static int sf_mime_class(httrackp *opt, const char *path, char *mime,
                         size_t mimesize) {
  mime[0] = '\0';
  if (!guess_httptype_sized(opt, mime, mimesize, path) || mime[0] == '\0')
    return 0;
  if (strfield(mime, "image/") != 0)
    return SF_C_IMAGE;
  if (strfield(mime, "font/") != 0 || strfield(mime, "application/font") != 0 ||
      strfield(mime, "application/x-font") != 0 ||
      strfield2(mime, "application/vnd.ms-fontobject") != 0)
    return SF_C_FONT;
  if (strfield2(mime, "text/css") != 0)
    return SF_C_CSS;
  if (strfield2(mime, "text/javascript") != 0 ||
      strfield2(mime, "application/javascript") != 0 ||
      strfield2(mime, "application/x-javascript") != 0 ||
      strfield2(mime, "application/ecmascript") != 0)
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

static hts_boolean sf_append_base64(String *out, char *data, size_t len) {
  /* code64() emits exactly one padded 4-byte group per 3 input bytes. */
  const size_t b64len = ((len + 2) / 3) * 4;
  char *buf;

  if (len > SINGLEFILE_HARD_MAX_SIZE)
    return HTS_FALSE;
  buf = (char *) malloct(b64len + 1);
  if (buf == NULL)
    return HTS_FALSE;
  code64((unsigned char *) data, (int) len, (unsigned char *) buf, 0);
  StringMemcat(*out, buf, b64len);
  freet(buf);
  return HTS_TRUE;
}

/* Append a mirror-relative path, percent-escaping everything an unquoted CSS
   url() or an HTML attribute could choke on. */
static void sf_append_escaped_path(String *out, const char *p) {
  static const char hex[] = "0123456789ABCDEF";
  size_t i;

  for (i = 0; p[i] != '\0'; i++) {
    const unsigned char c = (unsigned char) p[i];

    if (c <= 32 || c >= 127 || c == '"' || c == '\'' || c == '(' || c == ')' ||
        c == '\\' || c == '<' || c == '>' || c == '&' || c == '%') {
      StringAddchar(*out, '%');
      StringAddchar(*out, hex[c >> 4]);
      StringAddchar(*out, hex[c & 15]);
    } else {
      StringAddchar(*out, (char) c);
    }
  }
}

static void sf_warn_oversize(sf_ctx *ctx, const char *path, LLint size,
                             LLint cap) {
  if (*ctx->warn_budget <= 0)
    return;
  if (--(*ctx->warn_budget) == 0) {
    hts_log_print(ctx->opt, LOG_NOTICE,
                  "single-file: further over-cap assets not reported");
    return;
  }
  hts_log_print(ctx->opt, LOG_NOTICE,
                "single-file: %s left as a link (" LLintP
                " bytes, over the " LLintP "-byte cap)",
                path, size, cap);
}

static void sf_rewrite_css(sf_ctx *ctx, const char *base_dir, int depth,
                           const char *css, size_t len, String *out);

/* Replace the reference [ref,ref+reflen), resolved against base_dir, with its
   data: URI appended to out. classes gates the acceptable MIME classes;
   fallback_mime types an asset whose class cannot be guessed (NULL: give up).
   rebase_dir re-expresses an un-inlinable asset relative to that directory.
   A data: URL's path is opaque, so nothing relative inside an inlined
   stylesheet resolves anyway; this only aims it at a lenient resolver's base.
   Returns HTS_TRUE if out received a replacement. */
static hts_boolean sf_inline(sf_ctx *ctx, const char *base_dir, const char *ref,
                             size_t reflen, int classes,
                             const char *fallback_mime, const char *rebase_dir,
                             int depth, String *out) {
  String path = STRING_EMPTY;
  char mime[HTS_MIMETYPE_SIZE];
  char *body;
  size_t body_len = 0;
  LLint size, cap;
  int cls;
  hts_boolean done = HTS_FALSE;

  if (!sf_resolve(ctx, base_dir, ref, reflen, &path)) {
    StringFree(path);
    return HTS_FALSE;
  }
  cls = sf_mime_class(ctx->opt, StringBuff(path), mime, sizeof(mime));
  if (cls == 0 && fallback_mime != NULL) {
    cls = classes;
    strlcpybuff(mime, fallback_mime, sizeof(mime));
  }
  cap = ctx->opt->single_file_max_size;
  size = fsize_utf8(StringBuff(path));
  if ((cls & classes) == 0 || size < 0)
    goto fallback;
  if (size > cap || size > SINGLEFILE_HARD_MAX_SIZE) {
    sf_warn_oversize(ctx, StringBuff(path), size, cap);
    goto fallback;
  }
  if (size > ctx->budget) { /* the page has inlined all it may */
    sf_warn_oversize(ctx, StringBuff(path), size, ctx->budget);
    goto fallback;
  }
  body = sf_readfile(StringBuff(path), &body_len);
  if (body == NULL)
    goto fallback;
  /* Charged before the nested rewrite: an @import chain otherwise spends what
     its ancestors already claimed, and the budget ends up negative. */
  ctx->budget -= size;
  /* Encode into a scratch String: a failed encode must leave out untouched,
     not a truncated "data:...;base64," with no payload. */
  {
    String payload = STRING_EMPTY;

    StringClear(payload);
    if ((cls & SF_C_CSS) != 0 && depth < SF_MAX_CSS_DEPTH) {
      String nested = STRING_EMPTY;
      String nested_dir = STRING_EMPTY;

      sf_dirname(StringBuff(path), &nested_dir);
      StringClear(nested);
      sf_rewrite_css(ctx, StringBuff(nested_dir), depth + 1, body, body_len,
                     &nested);
      done = sf_append_base64(&payload, StringBuffRW(nested),
                              StringLength(nested));
      StringFree(nested);
      StringFree(nested_dir);
    } else {
      done = sf_append_base64(&payload, body, body_len);
    }
    if (done) {
      StringCat(*out, "data:");
      StringCat(*out, mime);
      StringCat(*out, ";base64,");
      StringMemcat(*out, StringBuff(payload), StringLength(payload));
    }
    StringFree(payload);
  }
  freet(body);
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
      sf_append_escaped_path(out, StringBuff(rel));
      done = HTS_TRUE;
    }
    StringFree(rel);
  }
  StringFree(path);
  return done;
}

/* ------------------------------------------------------------ */
/* CSS                                                           */
/* ------------------------------------------------------------ */

/* Rewrite the CSS in [css,css+len) into out, resolving its references against
   base_dir. depth counts the @import nesting. */
static void sf_rewrite_css(sf_ctx *ctx, const char *base_dir, int depth,
                           const char *css, size_t len, String *out) {
  size_t i = 0;
  int importing = 0; /* set for exactly one iteration: see the @import branch */

  while (i < len) {
    const int is_import = importing;

    importing = 0;
    /* A comment or a string is copied verbatim: a url( inside either is not a
       reference. */
    if (css[i] == '/' && i + 1 < len && css[i + 1] == '*') {
      const size_t start = i;

      i += 2;
      while (i + 1 < len && !(css[i] == '*' && css[i + 1] == '/'))
        i++;
      i = i + 1 < len ? i + 2 : len;
      StringMemcat(*out, css + start, i - start);
      continue;
    }
    if (css[i] == '@' && sf_span_starts(css + i, len - i, "@import")) {
      size_t j = i + 7;

      while (j < len && sf_is_space((unsigned char) css[j]))
        j++;
      /* url(...) here names a stylesheet, so hand it to the url( branch with
         the import's class rather than the image/font one. */
      if (j < len && sf_span_starts(css + j, len - j, "url(")) {
        StringMemcat(*out, css + i, j - i);
        i = j;
        importing = 1;
        continue;
      }
      if (j < len && (css[j] == '"' || css[j] == '\'')) {
        const char quote = css[j];
        const size_t vstart = j + 1;
        size_t vend = vstart;

        while (vend < len && css[vend] != quote && css[vend] != '\n') {
          if (css[vend] == '\\' && vend + 1 < len)
            vend++; /* an escaped quote does not end the string */
          vend++;
        }
        if (vend < len && css[vend] == quote) {
          String repl = STRING_EMPTY;

          StringClear(repl);
          StringMemcat(*out, css + i, j - i);
          if (sf_inline(ctx, base_dir, css + vstart, vend - vstart, SF_C_CSS,
                        "text/css", NULL, depth, &repl)) {
            StringCat(*out, "\"");
            StringMemcat(*out, StringBuff(repl), StringLength(repl));
            StringCat(*out, "\"");
          } else {
            StringMemcat(*out, css + j, vend + 1 - j);
          }
          StringFree(repl);
          i = vend + 1;
          continue;
        }
      }
      StringMemcat(*out, css + i, 7);
      i += 7;
      continue;
    }
    if ((css[i] == 'u' || css[i] == 'U') &&
        sf_span_starts(css + i, len - i, "url(") &&
        (i == 0 || (!isalnum((unsigned char) css[i - 1]) && css[i - 1] != '-' &&
                    css[i - 1] != '_'))) {
      const size_t start = i;
      size_t j = i + 4, vstart, vend;
      char quote = '\0';

      while (j < len && sf_is_space((unsigned char) css[j]))
        j++;
      if (j < len && (css[j] == '"' || css[j] == '\'')) {
        quote = css[j];
        j++;
      }
      vstart = j;
      while (j < len && (quote != '\0' ? css[j] != quote : css[j] != ')') &&
             css[j] != '\n') {
        if (quote != '\0' && css[j] == '\\' && j + 1 < len)
          j++; /* an escaped quote does not end the string */
        j++;
      }
      vend = j;
      if (quote != '\0' && j < len && css[j] == quote) {
        j++;
        while (j < len && sf_is_space((unsigned char) css[j]))
          j++;
      }
      if (j < len && css[j] == ')') {
        String repl = STRING_EMPTY;

        j++;
        StringClear(repl);
        /* Emit unquoted: the CSS could be a style="..." attribute value, where
           a quote would end the attribute. A data: payload and an escaped path
           both stay inside the unquoted url-token alphabet. */
        if (sf_inline(ctx, base_dir, css + vstart, vend - vstart,
                      is_import ? SF_C_CSS : SF_C_IMAGE | SF_C_FONT,
                      is_import ? "text/css" : NULL,
                      !is_import && depth > 0 ? ctx->page_dir : NULL, depth,
                      &repl)) {
          StringCat(*out, "url(");
          StringMemcat(*out, StringBuff(repl), StringLength(repl));
          StringCat(*out, ")");
        } else {
          StringMemcat(*out, css + start, j - start);
        }
        StringFree(repl);
        i = j;
        continue;
      }
      StringMemcat(*out, css + start, 4); /* unterminated url(: leave it */
      i = start + 4;
      continue;
    }
    if (css[i] == '"' || css[i] == '\'') {
      const char quote = css[i];
      const size_t start = i;

      i++;
      while (i < len && css[i] != quote && css[i] != '\n') {
        if (css[i] == '\\' && i + 1 < len)
          i++;
        i++;
      }
      if (i < len && css[i] == quote)
        i++;
      StringMemcat(*out, css + start, i - start);
      continue;
    }
    StringAddchar(*out, css[i]);
    i++;
  }
}

/* ------------------------------------------------------------ */
/* HTML                                                          */
/* ------------------------------------------------------------ */

/* srcset is "url [descriptor]" candidates separated by commas. Per the HTML
   candidate-parsing rules the URL token runs to the next whitespace and may
   carry trailing commas, so an emitted "data:...;base64,AAAA 2x" round-trips.
 */
static void sf_rewrite_srcset(sf_ctx *ctx, const char *v, size_t len,
                              String *out) {
  size_t i = 0;
  int first = 1;

  while (i < len) {
    size_t ustart, uend, dstart, dend;

    while (i < len && (sf_is_space((unsigned char) v[i]) || v[i] == ','))
      i++;
    if (i >= len)
      break;
    ustart = i;
    while (i < len && !sf_is_space((unsigned char) v[i]))
      i++;
    uend = i;
    while (uend > ustart && v[uend - 1] == ',')
      uend--;
    dstart = dend = i;
    if (uend == i) { /* no trailing comma, so a descriptor may follow */
      while (i < len && v[i] != ',')
        i++;
      dstart = uend;
      dend = i;
      while (dend > dstart && sf_is_space((unsigned char) v[dend - 1]))
        dend--;
      while (dstart < dend && sf_is_space((unsigned char) v[dstart]))
        dstart++;
    }
    if (!first)
      StringCat(*out, ", ");
    first = 0;
    if (!sf_inline(ctx, ctx->page_dir, v + ustart, uend - ustart, SF_C_IMAGE,
                   NULL, NULL, 0, out))
      StringMemcat(*out, v + ustart, uend - ustart);
    if (dend > dstart) {
      StringAddchar(*out, ' ');
      StringMemcat(*out, v + dstart, dend - dstart);
    }
  }
}

/* (tag, attribute) pairs naming an inlinable asset; a NULL tag matches any.
   Deliberately absent though hts_detect[] downloads them: a@href, iframe@src
   and longdesc name pages, dynsrc and non-image source@src name media,
   track@src has no inlinable MIME class, usemap is a same-document fragment,
   archive lists applet jars. link@href is decided from its rel
   (sf_link_classes); base@href needs no rule, htsparse drops it when saving. */
static const struct sf_attr_rule {
  const char *tag;
  const char *attr;
  int classes;
  int shape;
} sf_attr_rules[] = {
    {"img", "src", SF_C_IMAGE, SF_V_URL},
    {"img", "srcset", SF_C_IMAGE, SF_V_SRCSET},
    {"source", "src", SF_C_IMAGE, SF_V_URL},
    {"source", "srcset", SF_C_IMAGE, SF_V_SRCSET},
    {"input", "src", SF_C_IMAGE, SF_V_URL},
    {"video", "poster", SF_C_IMAGE, SF_V_URL},
    {"image", "href", SF_C_IMAGE, SF_V_URL},
    {"image", "xlink:href", SF_C_IMAGE, SF_V_URL},
    {"object", "data", SF_C_IMAGE, SF_V_URL},
    {"embed", "src", SF_C_IMAGE, SF_V_URL},
    {"script", "src", SF_C_JS, SF_V_URL},
    /* Lazy loading: src is a placeholder and the real image rides one of
       these. The image class keeps a lazy <iframe> or <script> a link. */
    {NULL, "data-src", SF_C_IMAGE, SF_V_URL},
    {NULL, "data-srcset", SF_C_IMAGE, SF_V_SRCSET},
    {NULL, "lowsrc", SF_C_IMAGE, SF_V_URL},
    {NULL, "background", SF_C_IMAGE, SF_V_URL},
    {NULL, "style", 0, SF_V_CSS},
};

typedef struct sf_attr {
  const char *pre; /* whitespace before the attribute */
  size_t pre_len;
  const char *raw; /* name plus value, with the original quoting */
  size_t raw_len;
  const char *name;
  size_t name_len;
  const char *value;
  size_t value_len;
  hts_boolean has_value;
} sf_attr;

/* Accepted classes and fallback type for <link href>, read from its rel: a
   navigational rel (canonical, alternate, next) yields 0 and keeps the link. */
static int sf_link_classes(const sf_attr *attrs, int nattrs,
                           const char **fallback) {
  int i;

  *fallback = NULL;
  for (i = 0; i < nattrs; i++) {
    if (!sf_span_eq(attrs[i].name, attrs[i].name_len, "rel"))
      continue;
    if (sf_span_has(attrs[i].value, attrs[i].value_len, "stylesheet")) {
      *fallback = "text/css";
      return SF_C_CSS;
    }
    if (sf_span_has(attrs[i].value, attrs[i].value_len, "icon"))
      return SF_C_IMAGE;
    if (sf_span_has(attrs[i].value, attrs[i].value_len, "preload") ||
        sf_span_has(attrs[i].value, attrs[i].value_len, "prefetch"))
      return SF_C_ANY;
    return 0;
  }
  return 0;
}

static const struct sf_attr_rule *sf_find_rule(const char *tag, size_t tag_len,
                                               const char *attr,
                                               size_t attr_len) {
  size_t i;

  for (i = 0; i < sizeof(sf_attr_rules) / sizeof(sf_attr_rules[0]); i++) {
    const struct sf_attr_rule *const r = &sf_attr_rules[i];

    if (r->tag != NULL && !sf_span_eq(tag, tag_len, r->tag))
      continue;
    if (sf_span_eq(attr, attr_len, r->attr))
      return r;
  }
  return NULL;
}

/* Emit one attribute, substituting its value when a rule applies. */
static void sf_emit_attr(sf_ctx *ctx, const char *tag, size_t tag_len,
                         const sf_attr *a, int link_classes,
                         const char *link_fallback, String *out) {
  const struct sf_attr_rule *rule = NULL;
  const char *fallback = NULL;
  String repl = STRING_EMPTY;
  hts_boolean done = HTS_FALSE;
  int classes;

  StringMemcat(*out, a->pre, a->pre_len);
  if (!a->has_value) {
    StringMemcat(*out, a->raw, a->raw_len);
    return;
  }
  if (sf_span_eq(tag, tag_len, "link") &&
      sf_span_eq(a->name, a->name_len, "href")) {
    classes = link_classes;
    fallback = link_fallback;
  } else {
    rule = sf_find_rule(tag, tag_len, a->name, a->name_len);
    classes = rule != NULL ? rule->classes : 0;
    if (rule != NULL && rule->classes == SF_C_JS)
      fallback = "text/javascript";
  }
  if (rule == NULL && classes == 0) {
    StringMemcat(*out, a->raw, a->raw_len);
    return;
  }
  StringClear(repl);
  if (rule != NULL && (rule->shape == SF_V_SRCSET || rule->shape == SF_V_CSS)) {
    /* These always produce a value; substitute only when it really differs, so
       an untouched attribute keeps its original spelling and quoting. */
    if (rule->shape == SF_V_SRCSET)
      sf_rewrite_srcset(ctx, a->value, a->value_len, &repl);
    else
      sf_rewrite_css(ctx, ctx->page_dir, 0, a->value, a->value_len, &repl);
    done = StringLength(repl) != a->value_len ||
                   memcmp(StringBuff(repl), a->value, a->value_len) != 0
               ? HTS_TRUE
               : HTS_FALSE;
  } else {
    done = sf_inline(ctx, ctx->page_dir, a->value, a->value_len, classes,
                     fallback, NULL, 0, &repl);
  }
  if (done) {
    size_t n;

    /* Always re-quote with '"', so a value carrying a literal quote (a CSS
       declaration that was single-quoted in the source) must be escaped. */
    StringMemcat(*out, a->name, a->name_len);
    StringCat(*out, "=\"");
    for (n = 0; n < StringLength(repl); n++) {
      const char c = StringSub(repl, n);

      if (c == '"')
        StringCat(*out, "&quot;");
      else
        StringAddchar(*out, c);
    }
    StringCat(*out, "\"");
  } else {
    StringMemcat(*out, a->raw, a->raw_len);
  }
  StringFree(repl);
}

/* Elements whose content is raw text, never markup. */
static const char *const sf_rawtext_tags[] = {"script", "style", "textarea",
                                              "title"};

/* The raw-text tag matching [tag,tag+len), or NULL. */
static const char *sf_rawtext_tag(const char *tag, size_t len) {
  size_t i;

  for (i = 0; i < sizeof(sf_rawtext_tags) / sizeof(sf_rawtext_tags[0]); i++) {
    if (sf_span_eq(tag, len, sf_rawtext_tags[i]))
      return sf_rawtext_tags[i];
  }
  return NULL;
}

static void sf_rewrite_html_(sf_ctx *ctx, const char *p, size_t len,
                             String *out) {
  size_t i = 0;

  while (i < len) {
    sf_attr attrs[SF_MAX_ATTRS];
    const char *tag, *rawtext, *link_fallback = NULL;
    size_t tag_len, tag_start;
    int nattrs = 0, k, link_classes = 0;
    hts_boolean selfclose = HTS_FALSE, overflow = HTS_FALSE;

    if (p[i] != '<') {
      StringAddchar(*out, p[i]);
      i++;
      continue;
    }
    if (i + 4 <= len && memcmp(p + i, "<!--", 4) == 0) {
      const size_t start = i;

      i += 4;
      /* "<!-->" and "<!--->" are empty comments, not unterminated ones. */
      if (i < len && p[i] == '>')
        i++;
      else if (i + 2 <= len && memcmp(p + i, "->", 2) == 0)
        i += 2;
      else {
        while (i + 3 <= len && memcmp(p + i, "-->", 3) != 0)
          i++;
        i = i + 3 <= len ? i + 3 : len;
      }
      StringMemcat(*out, p + start, i - start);
      continue;
    }
    if (i + 1 >= len || !isalpha((unsigned char) p[i + 1])) {
      const size_t start = i; /* doctype, processing instruction, end tag */

      i++;
      while (i < len && p[i] != '>') {
        /* A '>' inside a quoted value does not close the tag. */
        if (p[i] == '"' || p[i] == '\'') {
          const char q = p[i++];

          while (i < len && p[i] != q)
            i++;
        }
        if (i < len)
          i++;
      }
      if (i < len)
        i++;
      StringMemcat(*out, p + start, i - start);
      continue;
    }

    /* Start tag. Collect every attribute before deciding: <link href> depends
       on the rel, which may come after it. */
    tag_start = i;
    i++;
    tag = p + i;
    while (i < len && !sf_is_space((unsigned char) p[i]) && p[i] != '>' &&
           p[i] != '/')
      i++;
    tag_len = (size_t) (p + i - tag);
    while (i < len) {
      const char *const pre = p + i;
      size_t pre_len;
      sf_attr *a;

      while (i < len && sf_is_space((unsigned char) p[i]))
        i++;
      pre_len = (size_t) (p + i - pre);
      if (i >= len || p[i] == '>' ||
          (p[i] == '/' && i + 1 < len && p[i + 1] == '>')) {
        i = (size_t) (pre - p); /* the caller copies this whitespace */
        break;
      }
      if (nattrs == SF_MAX_ATTRS) {
        overflow = HTS_TRUE; /* keep parsing, but stop recording */
        nattrs--;
      }
      a = &attrs[nattrs++];
      a->pre = pre;
      a->pre_len = pre_len;
      a->name = p + i;
      a->raw = p + i;
      a->has_value = HTS_FALSE;
      a->value = NULL;
      a->value_len = 0;
      while (i < len && !sf_is_space((unsigned char) p[i]) && p[i] != '=' &&
             p[i] != '>' && !(p[i] == '/' && i + 1 < len && p[i + 1] == '>'))
        i++;
      a->name_len = (size_t) (p + i - a->name);
      if (a->name_len == 0) { /* a stray '=' or '/': copy it and move on */
        i++;
        a->name_len = 1;
        a->raw_len = 1;
        continue;
      }
      {
        const size_t save = i;

        while (i < len && sf_is_space((unsigned char) p[i]))
          i++;
        if (i >= len || p[i] != '=') {
          i = save;
          a->raw_len = a->name_len;
          continue;
        }
      }
      i++;
      while (i < len && sf_is_space((unsigned char) p[i]))
        i++;
      if (i < len && (p[i] == '"' || p[i] == '\'')) {
        const char quote = p[i];

        i++;
        a->value = p + i;
        while (i < len && p[i] != quote)
          i++;
        a->value_len = (size_t) (p + i - a->value);
        if (i < len)
          i++;
      } else {
        a->value = p + i;
        while (i < len && !sf_is_space((unsigned char) p[i]) && p[i] != '>')
          i++;
        a->value_len = (size_t) (p + i - a->value);
      }
      a->has_value = HTS_TRUE;
      a->raw_len = (size_t) (p + i - a->raw);
    }
    if (overflow) { /* pathological tag: copy the whole of it untouched */
      StringMemcat(*out, p + tag_start, i - tag_start);
    } else {
      if (sf_span_eq(tag, tag_len, "link"))
        link_classes = sf_link_classes(attrs, nattrs, &link_fallback);
      StringAddchar(*out, '<');
      StringMemcat(*out, tag, tag_len);
      for (k = 0; k < nattrs; k++)
        sf_emit_attr(ctx, tag, tag_len, &attrs[k], link_classes, link_fallback,
                     out);
    }
    while (i < len && sf_is_space((unsigned char) p[i])) {
      StringAddchar(*out, p[i]);
      i++;
    }
    if (i < len && p[i] == '/') {
      selfclose = HTS_TRUE;
      StringAddchar(*out, '/');
      i++;
    }
    if (i < len && p[i] == '>') {
      StringAddchar(*out, '>');
      i++;
    }

    rawtext = selfclose ? NULL : sf_rawtext_tag(tag, tag_len);
    if (rawtext != NULL) {
      const size_t body = i;
      size_t end = i;

      while (end < len) {
        if (p[end] == '<' && end + 2 < len && p[end + 1] == '/' &&
            sf_span_starts(p + end + 2, len - end - 2, rawtext)) {
          const size_t after = end + 2 + strlen(rawtext);

          if (after >= len || sf_is_space((unsigned char) p[after]) ||
              p[after] == '>' || p[after] == '/')
            break;
        }
        end++;
      }
      if (sf_span_eq(tag, tag_len, "style"))
        sf_rewrite_css(ctx, ctx->page_dir, 0, p + body, end - body, out);
      else
        StringMemcat(*out, p + body, end - body);
      i = end;
    }
  }
}

/* ------------------------------------------------------------ */
/* Entry points                                                  */
/* ------------------------------------------------------------ */

hts_boolean singlefile_rewrite_html(httrackp *opt, const char *root,
                                    const char *page_path, const char *html,
                                    size_t html_len, LLint page_budget,
                                    String *out) {
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
  /* A UTF-16/32 page is not ASCII-delimited, so byte scanning would corrupt
     it; every ASCII-compatible charset is safe. */
  if (html_len >= 2 &&
      (((unsigned char) html[0] == 0xff && (unsigned char) html[1] == 0xfe) ||
       ((unsigned char) html[0] == 0xfe && (unsigned char) html[1] == 0xff))) {
    StringMemcat(*out, html, html_len);
  } else {
    sf_rewrite_html_(&ctx, html, html_len, out);
  }
  StringFree(nroot);
  StringFree(npage);
  StringFree(dir);
  return ctx.inlined > 0 ? HTS_TRUE : HTS_FALSE;
}

hts_boolean singlefile_rewrite_file(httrackp *opt, const char *root,
                                    const char *page_path) {
  char catbuff[CATBUFF_SIZE];
  String out = STRING_EMPTY;
  String tmp = STRING_EMPTY;
  size_t len = 0;
  char *html = sf_readfile(page_path, &len);
  hts_boolean ok;
  FILE *fp;

  if (html == NULL)
    return HTS_FALSE;
  StringClear(out);
  ok = singlefile_rewrite_html(opt, root, page_path, html, len,
                               SINGLEFILE_MAX_PAGE_SIZE, &out);
  freet(html);
  if (!ok) {
    StringFree(out);
    return HTS_FALSE;
  }
  /* Spool then rename: a half-written page would destroy a complete mirror
     file that nothing is going to re-fetch. */
  StringCopy(tmp, page_path);
  StringCat(tmp, ".sfnew");
  fp = FOPEN(fconv(catbuff, sizeof(catbuff), StringBuff(tmp)), "wb");
  if (fp == NULL) {
    hts_log_print(opt, LOG_WARNING | LOG_ERRNO,
                  "single-file: could not rewrite %s", page_path);
    ok = HTS_FALSE;
  } else {
    if (StringLength(out) > 0 &&
        fwrite(StringBuff(out), 1, StringLength(out), fp) != StringLength(out))
      ok = HTS_FALSE;
    if (fclose(fp) != 0)
      ok = HTS_FALSE;
#ifndef _WIN32
    /* The spool bypassed filecreate(), which is what chmods every other
       mirrored file; without this the page keeps the umask's mode. */
    if (ok)
      (void) chmod(fconv(catbuff, sizeof(catbuff), StringBuff(tmp)),
                   HTS_ACCESS_FILE);
#endif
    if (ok) {
      /* RENAME does not clobber an existing target on Windows. */
      if (RENAME(StringBuff(tmp), page_path) != 0) {
        (void) UNLINK(page_path);
        ok = RENAME(StringBuff(tmp), page_path) == 0 ? HTS_TRUE : HTS_FALSE;
      }
    }
    if (!ok) {
      hts_log_print(opt, LOG_ERROR, "single-file: could not rewrite %s",
                    page_path);
      (void) UNLINK(StringBuff(tmp));
    }
  }
  StringFree(tmp);
  StringFree(out);
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
