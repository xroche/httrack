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
   htsparse marked every inlinable reference while saving, so this is a
   substitution over those marks: no HTML and no CSS is parsed here. Resolving
   a mark is path arithmetic clamped to the mirror root; anything the mark
   cannot be resolved to keeps its link, which is what makes a second --update
   run a no-op. */
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
} sf_ctx;

/* ------------------------------------------------------------ */
/* Marks                                                         */
/* ------------------------------------------------------------ */

/* (tag, attribute) pairs htsparse detects that name a page or a media stream
   rather than an asset; a NULL tag matches any. Everything else it detects is
   offered to the pass, which decides from the referenced file's own MIME
   type, so a new row in hts_detect[] is covered without a change here. */
static const struct sf_deny_rule {
  const char *tag;
  const char *attr;
} sf_deny_rules[] = {
    {"a", "href"},      {"area", "href"},   {"iframe", "src"},
    {"frame", "src"},   {"applet", "code"}, {NULL, "longdesc"},
    {NULL, "usemap"},   {NULL, "archive"},  {NULL, "profile"},
    {NULL, "codebase"},
};

hts_boolean singlefile_may_inline(const char *tag_start, const char *attr) {
  size_t i;

  if (attr == NULL)
    return HTS_FALSE;
  for (i = 0; i < sizeof(sf_deny_rules) / sizeof(sf_deny_rules[0]); i++) {
    const struct sf_deny_rule *const r = &sf_deny_rules[i];

    if (r->tag != NULL && (tag_start == NULL || !check_tag(tag_start, r->tag)))
      continue;
    if (rech_tageq(attr, r->attr))
      return HTS_FALSE;
  }
  return HTS_TRUE;
}

/* ------------------------------------------------------------ */
/* Paths                                                         */
/* ------------------------------------------------------------ */

static int sf_is_space(int c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f';
}

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

/* ------------------------------------------------------------ */
/* Substitution                                                  */
/* ------------------------------------------------------------ */

static void sf_expand(sf_ctx *ctx, const char *base_dir, int depth,
                      const char *body, size_t len, String *out);

/* Replace the reference [ref,ref+reflen), resolved against base_dir, with its
   data: URI appended to out. rebase_dir re-expresses an un-inlinable asset
   relative to that directory: a data: URL's path is opaque, so nothing
   relative inside an inlined stylesheet resolves anyway; this only aims it at
   a lenient resolver's base. Returns HTS_TRUE if out received a data: URI. */
static hts_boolean sf_inline(sf_ctx *ctx, const char *base_dir, const char *ref,
                             size_t reflen, const char *rebase_dir, int depth,
                             String *out) {
  String path = STRING_EMPTY;
  char mime[HTS_MIMETYPE_SIZE];
  char *file;
  size_t file_len = 0;
  LLint size, cap;
  int cls;
  hts_boolean done = HTS_FALSE;

  if (!sf_resolve(ctx, base_dir, ref, reflen, &path)) {
    StringFree(path);
    return HTS_FALSE;
  }
  cls = sf_mime_class(ctx->opt, StringBuff(path), mime, sizeof(mime));
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
  /* Encode into a scratch String: a failed encode must leave out untouched,
     not a truncated "data:...;base64," with no payload. */
  {
    String payload = STRING_EMPTY;

    StringClear(payload);
    if ((cls & (SF_C_CSS | SF_C_JS)) != 0 && depth < SF_MAX_CSS_DEPTH) {
      String nested = STRING_EMPTY;
      String nested_dir = STRING_EMPTY;

      sf_dirname(StringBuff(path), &nested_dir);
      StringClear(nested);
      sf_expand(ctx, StringBuff(nested_dir), depth + 1, file, file_len,
                &nested);
      done = sf_append_base64(&payload, StringBuffRW(nested),
                              StringLength(nested));
      StringFree(nested);
      StringFree(nested_dir);
    } else {
      done = sf_append_base64(&payload, file, file_len);
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
      sf_append_escaped_path(out, StringBuff(rel));
      done = HTS_TRUE;
    }
    StringFree(rel);
  }
  StringFree(path);
  return done;
}

/* The mark ends a reference; these end the token the mark was appended to. */
static int sf_is_ref_delim(int c) {
  return sf_is_space(c) || c == '"' || c == '\'' || c == '(' || c == ')' ||
         c == '=' || c == ',' || c == '<' || c == '>' || c == ';';
}

/* Copy [body,body+len) to out, replacing each marked reference by its data:
   URI, or by the bare reference when it cannot be inlined. */
static void sf_expand(sf_ctx *ctx, const char *base_dir, int depth,
                      const char *body, size_t len, String *out) {
  const size_t marklen = strlen(SINGLEFILE_MARK);
  size_t i = 0, flushed = 0;

  while (i + marklen <= len) {
    size_t start, tail;

    if (memcmp(body + i, SINGLEFILE_MARK, marklen) != 0) {
      i++;
      continue;
    }
    for (start = i; start > flushed && !sf_is_ref_delim(body[start - 1]);
         start--)
      ;
    /* htsparse writes the fragment and the kept query string after the mark;
       a data: URI has no use for either. */
    for (tail = i + marklen; tail < len && !sf_is_ref_delim(body[tail]); tail++)
      ;
    StringMemcat(*out, body + flushed, start - flushed);
    if (!sf_inline(ctx, base_dir, body + start, i - start,
                   depth > 0 ? ctx->page_dir : NULL, depth, out)) {
      StringMemcat(*out, body + start, i - start);
      StringMemcat(*out, body + i + marklen, tail - i - marklen);
    }
    i = tail;
    flushed = i;
  }
  StringMemcat(*out, body + flushed, len - flushed);
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
  sf_expand(&ctx, StringBuff(dir), 0, html, html_len, out);
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
