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
/* File: htsparse.c parser                                      */
/*       html/javascript/css parser                             */
/*       and other parser routines                              */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include <fcntl.h>

/* File defs */
#include "htscore.h"

/* specific definitions */
#include "htsbase.h"
#include "htsio.h"
#include "htsnet.h"
#include "htsbauth.h"
#include "htsmd5.h"
#include "htsindex.h"
#include "htslines.h"
#include "htscharset.h"
#include "htsencoding.h"
#include "htssniff.h"
#include "htsname.h"
#include "htsthread.h"

/* external modules */
#include "htsmodules.h"

// htswrap_add
#include "htswrap.h"

// parser
#include "htsparse.h"
#include "htsmirror.h"
#include "htssinglefile.h"
#include "htsback.h"
#include "htsfetch.h"

// arrays
#include "htsarrays.h"

/* Room a later stage still needs inside lien[]: a scheme it may prepend
   ("http://") plus a ".class" it may append, and the NUL. */
#define HTS_LINK_TAIL_ROOM 16

/* htsparse()'s ps->sink, under the short names its call sites use. */
#define HT_ADD_N(PTR, COUNT) sink_add_n(&ps->sink, hts_span_at((PTR), (COUNT)))
#define HT_ADD(A) sink_add(&ps->sink, (A))
#define HT_ADD_BUF(A, N) HT_ADD_N(A, htssafe_strnlen_(A, N))
#define HT_ADD_HTMLESCAPED(A) sink_add_htmlescaped(&ps->sink, (A))
#define HT_ADD_HTMLESCAPED_FULL(A) sink_add_htmlescaped_full(&ps->sink, (A))
#define HT_ADD_URL_HTMLESCAPED(A) sink_add_url_htmlescaped(&ps->sink, (A))
#define HT_SET_SIZE(N) sink_set_size(&ps->sink, (N))

/** Append bytes to the output buffer up to the pointer 'html'. **/
#define HT_add_adr                                                             \
  do {                                                                         \
    if (ps->sink.enabled && ps->html != ps->lastsaved) {                       \
      HT_ADD_N(ps->lastsaved, (size_t) (ps->html - ps->lastsaved));            \
      ps->lastsaved = ps->html;                                                \
    }                                                                          \
  } while (0)

#define _ROBOTS      ((robots_wizard*)opt->robotsptr)

/* JS-detection automaton states; INSCRIPT_DEFAULT is the synthetic "any other
   char" column of the transition table. */
typedef enum {
  INSCRIPT_START = 0,
  INSCRIPT_ANTISLASH,
  INSCRIPT_INQUOTE,
  INSCRIPT_INQUOTE2,
  INSCRIPT_SLASH,
  INSCRIPT_SLASHSLASH,
  INSCRIPT_COMMENT,
  INSCRIPT_COMMENT2,
  INSCRIPT_ANTISLASH_IN_QUOTE,
  INSCRIPT_ANTISLASH_IN_QUOTE2,
  INSCRIPT_DEFAULT = 256
} INSCRIPT;

#define INSCRIPT_NSTATES 10 /* rows in the transition table */

/* The helpers below drive the parser's automaton locals through this view. */
typedef struct {
  const int *inscript;             /* nonzero while inside a script body */
  const hts_boolean *css_file;     /* is the whole document a stylesheet? */
  const signed char (*table)[257]; /* [INSCRIPT_NSTATES][257] transitions */
  INSCRIPT *pos;                   /* current state */
  const char **html;               /* parse cursor */
} script_automate;

/* Feed the current *html byte to the automaton. No-op outside a script body. */
static void hts_automate_lookup(const script_automate *aut) {
  if (*aut->inscript) {
    int next = aut->table[*aut->pos][(unsigned char) **aut->html];
    if (next < 0) {
      next = aut->table[*aut->pos][INSCRIPT_DEFAULT];
    }
    assertf(next >= 0 && next < INSCRIPT_NSTATES);
    /* CSS has no // comment, and the one in http:// would otherwise kill the
       rest of the line, url() calls included (#139). */
    if (next == INSCRIPT_SLASHSLASH && *aut->css_file) {
      next = INSCRIPT_SLASH;
    }
    *aut->pos = (INSCRIPT) next;
  }
}

/* Contract in htsparse.h. */
const char *hts_dirty_attr_name(const char *quote, const char *tag_start,
                                const char **nend) {
  const char *a = quote - 1;
  while (a > tag_start && is_taborspace(*a))
    a--;
  if (a == tag_start || *a != '=')
    return NULL;
  a--;
  while (a > tag_start && is_taborspace(*a))
    a--;
  *nend = a + 1;
  while (a > tag_start && *a != '=' && *a != '\"' && *a != '\'' &&
         !is_realspace(*a))
    a--;
  a++;
  // a name starting right after '<' is the tag name, not an attribute
  return a < *nend && a > tag_start + 1 ? a : NULL;
}

/* Attribute names that never hold a link. */
const char *const hts_nodetect[] = {
    "accept-charset", "accesskey", "action",   "align",    "alt",     "axes",
    "axis",           "char",      "charset",  "cite",     "class",   "classid",
    "code",           "color",     "datetime", "dir",      "enctype", "face",
    "height",         "id",        "lang",     "language", "media",   "method",
    "name",           "prompt",    "scheme",   "size",     "style",   "target",
    "title",          "type",      "valign",   "version",  "width",   ""};

const char *const hts_main_mime[] = {"application", "audio", "image", "message",
                                     "multipart",   "text",  "video", ""};

/* Is this attribute a framework binding (Vue :x, v-x, @x, Angular [x], (x),
   Alpine x-bind:x)? Its value is a JavaScript expression, never a URL. */
static hts_boolean hts_attr_is_js_binding(const char *name, const char *nend) {
  static const char *const prefixes[] = {":",  "@",       "[",     "(",
                                         "v-", "x-bind:", "x-on:", NULL};
  const size_t len = (size_t) (nend - name);
  int i;

  for (i = 0; prefixes[i] != NULL; i++) {
    const size_t l = strlen(prefixes[i]);

    if (len > l && hts_strncasecmp(name, prefixes[i], l) == 0)
      return HTS_TRUE;
  }
  return HTS_FALSE;
}

/* At most one row matches at a byte, because each name must be followed by
   '=', so row order does not matter. */
const hts_attr hts_attrs[] = {
    {"archive", HTS_ATTR_LINK},
    {"background", HTS_ATTR_LINK},
    {"data", HTS_ATTR_LINK}, // OBJECT
    {"data-src", HTS_ATTR_LINK},
    {"data-srcset", HTS_ATTR_LINK},
    {"dynsrc", HTS_ATTR_LINK},
    {"lowsrc", HTS_ATTR_LINK},
    {"profile", HTS_ATTR_LINK}, // META
    {"src", HTS_ATTR_LINK},
    {"srcset", HTS_ATTR_LINK}, // HTML5 responsive images
    {"swurl", HTS_ATTR_LINK},
    {"url", HTS_ATTR_LINK},
    {"usemap", HTS_ATTR_LINK},
    {"longdesc", HTS_ATTR_LINK},
    {"xlink:href", HTS_ATTR_LINK}, // XML, SVG
    {"poster", HTS_ATTR_LINK},     // HTML5
    {"hotspot", HTS_ATTR_HOTSPOT},
    {"content", HTS_ATTR_REFRESH},
    {"action", HTS_ATTR_LEAVE},
    // The parser also takes any other onXxx= with an upper case X.
    {"onAbort", HTS_ATTR_EVENT},
    {"onBlur", HTS_ATTR_EVENT},
    {"onChange", HTS_ATTR_EVENT},
    {"onClick", HTS_ATTR_EVENT},
    {"onDblClick", HTS_ATTR_EVENT},
    {"onDragDrop", HTS_ATTR_EVENT},
    {"onError", HTS_ATTR_EVENT},
    {"onFocus", HTS_ATTR_EVENT},
    {"onKeyDown", HTS_ATTR_EVENT},
    {"onKeyPress", HTS_ATTR_EVENT},
    {"onKeyUp", HTS_ATTR_EVENT},
    {"onLoad", HTS_ATTR_EVENT},
    {"onMouseDown", HTS_ATTR_EVENT},
    {"onMouseMove", HTS_ATTR_EVENT},
    {"onMouseOut", HTS_ATTR_EVENT},
    {"onMouseOver", HTS_ATTR_EVENT},
    {"onMouseUp", HTS_ATTR_EVENT},
    {"onMove", HTS_ATTR_EVENT},
    {"onReset", HTS_ATTR_EVENT},
    {"onResize", HTS_ATTR_EVENT},
    {"onSelect", HTS_ATTR_EVENT},
    {"onSubmit", HTS_ATTR_EVENT},
    {"onUnload", HTS_ATTR_EVENT},
    {"style", HTS_ATTR_STYLE},
    {NULL, HTS_ATTR_LINK},
};

const hts_attr *hts_attr_match(const char *html, hts_boolean scripts,
                               int *value) {
  int i;

  *value = 0;
  for (i = 0; hts_attrs[i].name != NULL; i++) {
    const hts_attr *const at = &hts_attrs[i];

    if (!scripts && (at->kind == HTS_ATTR_EVENT || at->kind == HTS_ATTR_STYLE))
      continue;
    *value = at->kind == HTS_ATTR_HOTSPOT ? rech_tageqbegdigits(html, at->name)
                                          : rech_tageq(html, at->name);
    if (*value != 0)
      return at;
  }
  return NULL;
}

/* Does s open with tag (A-Z folded), followed by a space? */
static hts_boolean tag_opens_with_space(const char *s, const char *tag) {
  return hts_skip_prefix_ci(&s, tag) && hts_isspace((unsigned char) *s);
}

/* Does s open with tag (A-Z folded), followed by '>', '/', a space or the
   end? */
static hts_boolean tag_opens_whole(const char *s, const char *tag) {
  return hts_skip_prefix_ci(&s, tag) && (*s == '>' || *s == '/' || *s == '\0' ||
                                         hts_isspace((unsigned char) *s));
}

/* Is the attribute name [name, nend) one whose value is never a link, a
   hts_nodetect word or an xmlns declaration? Returns the word, or NULL. */
static const char *hts_dirty_attr_refused(const char *name, const char *nend) {
  int i;
  for (i = 0; strnotempty(hts_nodetect[i]); i++) {
    const char *end = name;
    if (hts_skip_prefix_ci(&end, hts_nodetect[i]) && end == nend)
      return hts_nodetect[i];
  }
  if (hts_skip_prefix_ci(&name, "xmlns") && (name == nend || *name == ':'))
    return "xmlns";
  return NULL;
}

/* Contract in htsparse.h. Resolves the owning attribute itself, since
   intag_startattr is unreliable mid-tag. */
hts_boolean hts_dirty_attr_detectable(const char *quote,
                                      const char *tag_start) {
  const char *nend;
  const char *name = hts_dirty_attr_name(quote, tag_start, &nend);
  return name != NULL && hts_dirty_attr_refused(name, nend) == NULL;
}

/* Advance the cursor by 'steps' bytes, feeding each to the automaton. */
static void hts_automate_increment(const script_automate *aut, int steps) {
  while (steps > 0) {
    (*aut->html)++;
    hts_automate_lookup(aut);
    steps--;
  }
}

/* Percent-encode the angle brackets of a string so it is safe to embed inside
   an HTML comment (the default footer) or any other HTML context. A URL holding
   "-->" would otherwise close the footer comment and inject markup (issue #165).
   Raw '<' and '>' are not valid URL characters, so encoding them is harmless. */
static const char *html_inline_safe(const char *src, hts_buf dst) {
  size_t i, j;

  for (i = 0, j = 0; src[i] != '\0' && j + 4 < dst.size; i++) {
    const char c = src[i];

    if (c == '<' || c == '>') {
      dst.p[j++] = '%';
      dst.p[j++] = '3';
      dst.p[j++] = (c == '<') ? 'C' : 'E';
    } else {
      dst.p[j++] = c;
    }
  }
  dst.p[j] = '\0';
  return dst.p;
}

/* Copies the first max bytes of the page text s into buf for a debug line,
   with a URL login masked, or "(hidden)" when a login cannot be told apart. */
static const char *text_excerpt(httrackp *opt, hts_buf buf, hts_span s,
                                size_t max) {
  char *copy = hts_span_dup(s);
  const char *shown;

  buf.p[0] = '\0';
  if (copy == NULL)
    return buf.p;
  shown = url_nologin(opt, copy);
  if (shown == copy && jump_protocol_const(copy) == copy &&
      strchr(copy, '@') != NULL)
    shown = "(hidden)";
  hts_buf_catn(buf, shown, max);
  freet(copy);
  return buf.p;
}

/* What the link a tag attribute holds stands for. */
typedef enum hts_link_role {
  HTS_LINK_PLAIN = 0,        /* a link to fetch */
  HTS_LINK_APPLET_CODE = -1, /* applet code=, a class name */
  HTS_LINK_BASE = 2,         /* base href= or xml:base= */
  HTS_LINK_CODEBASE = -2     /* applet codebase= */
} hts_link_role;

/* Does the link set the base later links resolve against? */
static hts_boolean link_role_is_base(const hts_link_role role) {
  return role == HTS_LINK_BASE || role == HTS_LINK_CODEBASE;
}

/* Return the first '>' in the first max bytes of s, or NULL. The bound stops a
   tag with no '>' from being rescanned to the page end once per attribute. */
static const char *tag_close_within(const char *s, size_t max) {
  size_t i;

  for (i = 0; i < max && s[i] != '\0'; i++) {
    if (s[i] == '>')
      return &s[i];
  }
  return NULL;
}

/* Return the page's decode charset, or NULL when conversion is off. It
   sets *declared to FALSE for the iso-8859-1 guess. */
static const char *page_charset(const htsmoduleStruct *str,
                                hts_boolean *declared) {
  const char *const charset = str->page_charset_;

  if (charset == NULL) {
    *declared = HTS_FALSE;
    return NULL;
  }
  *declared = *charset != '\0' ? HTS_TRUE : HTS_FALSE;
  return *declared ? charset : "iso-8859-1";
}

/* Is s exactly an HTTP method token, in any case? XHR.open's first argument
   is a method, not a URL (#218). */
static hts_boolean is_http_method(hts_span s) {
  static const char *const methods[] = {"GET",    "POST",  "PUT",
                                        "DELETE", "HEAD",  "OPTIONS",
                                        "PATCH",  "TRACE", NULL};

  return hts_span_index_ci(s, methods) >= 0;
}

/* Contract in htsparse.h. */
hts_boolean hts_dirty_link_is_url(httrackp *opt, hts_span str, char lastc,
                                  hts_boolean inscript) {
  char BIGSTK tempo[HTS_URLMAXSIZE * 2];
  char type[256];
  hts_boolean url_ok = HTS_FALSE;
  hts_boolean had_fragment_or_query;
  char *a;

  /* The parser never offers more, and a caller that does gets no verdict
     rather than an aborted process. */
  if (str.len >= HTS_URLMAXSIZE || !hts_buf_put_span(HTS_BUF(tempo), str))
    return HTS_FALSE;
  type[0] = '\0';

  /* A space is suspicious outside script code */
  if (strchr(tempo, ' ') != NULL && !inscript)
    return HTS_FALSE;

  /* Ask before the cut destroys the evidence and unescape_amp forges it. */
  had_fragment_or_query = link_dir_has_fragment_or_query(tempo);

  unescape_amp(tempo);

  /* Cut at the fragment or the query */
  if ((a = strchr(tempo, '#')) != NULL)
    *a = '\0';
  if ((a = strchr(tempo, '?')) != NULL)
    *a = '\0';

  /* No special characters allowed */
  if (!strnotempty(tempo))
    return HTS_FALSE;
  if (strchr(tempo, '*') || strchr(tempo, '<') || strchr(tempo, '>') ||
      strchr(tempo, ',')     /* list of files ? */
      || strchr(tempo, '\"') /* potential parsing bug */
      || strchr(tempo, '\'') /* potential parsing bug */
  )
    return HTS_FALSE;
  if (tempo[0] == '.' && hts_isalnum((unsigned char) tempo[1])) // ".gif"
    return HTS_FALSE;

  // a trailing '+' concatenates, so only an extension speaks for the string
  if (lastc != '+') {
    // "Comparisons of scheme names MUST be case-insensitive" (RFC2616)
    if (hts_starts_with_ci(tempo, "http:") || hts_starts_with_ci(tempo, "ftp:")
#if HTS_USEOPENSSL
        || hts_starts_with_ci(tempo, "https:")
#endif
    )
      url_ok = HTS_TRUE;
    else if (hts_lastchar(tempo) == '/') {
      /* A trailing slash alone is no evidence inside a script, where "/" and
         "image/" are ordinary strings. */
      if (inscript &&
          (had_fragment_or_query || link_dir_is_multisegment(tempo)))
        url_ok = HTS_TRUE;
    }
  }
  // a recognized extension names a file
  if (!url_ok) {
    if (get_httptype_sized(opt, type, sizeof(type), tempo,
                           0)) // recognized type
      url_ok = HTS_TRUE;
    else if (is_dyntype(hts_fil_ext(tempo))) // php, cgi, asp
      url_ok = HTS_TRUE;
    // but not foobar@aol.com
    if (strchr(tempo, '@'))
      url_ok = HTS_FALSE;
  }
  return url_ok;
}

/* Can the byte before "at" continue a name, making the keyword the tail of a
   longer one? A dot glues, because the ".xxx" rows above own property names. */
static hts_boolean js_glues_keyword(const char *at, const char *buffer) {
  const unsigned char *const p = (const unsigned char *) at;
  const unsigned char c = (unsigned char) html_prevc(at, buffer);
  const size_t before = (size_t) (at - buffer);

  if (c < 0x80)
    return hts_isalnum((unsigned char) c) || c == '_' || c == '$' || c == '.';
  /* Above 127 a name may be Unicode, so only a space ends one. This is every
     space the language reads as one: U+00A0 (or a lone 0xA0 where the page is
     not UTF-8, since no continuation byte follows an ASCII one), U+1680,
     U+2000..200A, U+2028, U+2029, U+202F, U+205F, U+3000 and U+FEFF. */
  if (c == 0xA0 && (before < 2 || p[-2] == 0xC2 || p[-2] < 0x80))
    return HTS_FALSE;
  if (before >= 3 && ((p[-3] == 0xE2 && p[-2] == 0x80 &&
                       (c <= 0x8A || c == 0xA8 || c == 0xA9 || c == 0xAF)) ||
                      (p[-3] == 0xE1 && p[-2] == 0x9A && c == 0x80) ||
                      (p[-3] == 0xE2 && p[-2] == 0x81 && c == 0x9F) ||
                      (p[-3] == 0xE3 && p[-2] == 0x80 && c == 0x80) ||
                      (p[-3] == 0xEF && p[-2] == 0xBB && c == 0xBF)))
    return HTS_FALSE;
  return HTS_TRUE;
}

/* CSS continues an ident across "-", and across U+1680 and U+FEFF, which the
   language above reads as spaces. So image-url( is a function of its own and
   its argument is never a url-token (CSS Syntax 3, #1754). */
static hts_boolean css_glues_keyword(const char *at, const char *buffer) {
  const unsigned char *const p = (const unsigned char *) at;
  const size_t before = (size_t) (at - buffer);

  if (html_prevc(at, buffer) == '-')
    return HTS_TRUE;
  if (before >= 3 && p[-3] == 0xE1 && p[-2] == 0x9A && p[-1] == 0x80)
    return HTS_TRUE; /* U+1680 */
  if (before >= 3 && p[-3] == 0xEF && p[-2] == 0xBB && p[-1] == 0xBF)
    return HTS_TRUE; /* U+FEFF */
  return js_glues_keyword(at, buffer);
}

/* Contract in htsparse.h; indexed so no pointer leaves the buffer. */
hts_boolean hts_js_quote_is_import_arg(const char *quote, const char *buffer) {
  static const char kw[] = "import";
  const size_t kwlen = sizeof(kw) - 1;
  size_t i;

  if (quote == NULL || buffer == NULL || quote <= buffer)
    return HTS_FALSE;
  i = (size_t) (quote - buffer);
  while (i != 0 && is_realspace(buffer[i - 1]))
    i--;
  if (i == 0 || buffer[i - 1] != '(')
    return HTS_FALSE;
  i--;
  while (i != 0 && is_realspace(buffer[i - 1]))
    i--;
  if (i < kwlen || memcmp(buffer + i - kwlen, kw, kwlen) != 0)
    return HTS_FALSE;
  i -= kwlen;
  return !js_glues_keyword(buffer + i, buffer);
}

/* Is this module specifier a URL, or a module id the loader resolves for
   itself? The ESM grammar decides it on the leading bytes: a path begins "/",
   "./" or "../", so ".config/a.js" and "..x.js" are ids, not paths. */
static hts_boolean js_specifier_is_url(const char *s) {
  if (s[0] == '/')
    return HTS_TRUE;
  if (s[0] == '.' && (s[1] == '/' || (s[1] == '.' && s[2] == '/')))
    return HTS_TRUE;
  /* an absolute URL is one too, the way a bare import already takes it */
  return hts_starts_with_ci(s, "http:") || hts_starts_with_ci(s, "https:") ||
         hts_starts_with_ci(s, "ftp:");
}

/* The quote an HTML entity at "s" stands for, or '\0'. Sets "*len", unless
   NULL, to the entity's length when it is one. */
static char html_entity_quote(const char *s, int *len) {
  static const struct {
    const char *name;
    char quote;
  } entities[] = {{"&quot;", '"'}, {"&QUOT;", '"'},  {"&#34;", '"'},
                  {"&#x22;", '"'}, {"&#X22;", '"'},  {"&apos;", '\''},
                  {"&#39;", '\''}, {"&#x27;", '\''}, {"&#X27;", '\''}};

  size_t i;

  // case matters: HTML has &QUOT; but no &APOS;
  for (i = 0; i < sizeof(entities) / sizeof(entities[0]); i++) {
    const size_t n = strlen(entities[i].name);

    if (strncmp(s, entities[i].name, n) == 0) {
      if (len != NULL)
        *len = (int) n;
      return entities[i].quote;
    }
  }
  return '\0';
}

/* Contract in htsparse.h. */
hts_boolean hts_js_scan_link(httrackp *opt, const char *cursor,
                             const char *buffer, hts_boolean in_tag,
                             char tag_lastc, hts_boolean in_css,
                             hts_js_link *link) {
  char expected = '='; // byte that must follow the keyword
  const char *expected_end = ";";
  int can_avoid_quotes = 0;
  char quotes_replacement = '\0';
  int ensure_not_mime = 0;
  // .open(method,url): reject an HTTP-method first arg (#218)
  int ensure_not_method = 0;
  // @import: the quoted token is the URL; a trailing media/supports/layer
  // condition is not part of it
  int is_import = 0;
  // from "./mod.js": an ES module specifier, which must be a path
  int is_specifier = 0;
  const char *a = cursor;
  hts_boolean nc;

  link->offset = 0;
  link->length = 0;
  link->unquoted_end = '\0';
  link->is_module = HTS_FALSE;
  link->escape_parens = HTS_FALSE;
  link->entity_quote = '\0';

  /* Can we parse javascript ? */
  if ((opt->parsejava & HTSPARSE_NO_JAVASCRIPT) != 0)
    return HTS_FALSE;

  if (in_tag)
    expected_end = ";\"\'"; // a href="javascript:doc.location='foo'" ends on
                            // the attribute's quote

  nc = hts_skip_prefix_ci(&a, ".src"); // nom.src="image";
  if (!nc && in_tag && tag_lastc == html_prevc(cursor, buffer))
    nc = hts_skip_prefix_ci(&a, "src"); // onXXX='src="image";'
  if (!nc)
    nc = hts_skip_prefix_ci(&a, ".location"); // document.location="doc"
  if (!nc)
    nc = hts_skip_prefix_ci(&a, ":location"); // javascript:location="doc"
  if (!nc && (nc = hts_skip_prefix_ci(&a, "location")) && // location="doc"
      !hts_isspace((unsigned char) html_prevc(cursor, buffer))) {
    nc = HTS_FALSE;
    a = cursor;
  }
  if (!nc)
    nc = hts_skip_prefix_ci(&a, ".href"); // document.location="doc"
  if (!nc)
    if ((nc = hts_skip_prefix_ci(&a, ".open"))) { // window.open("doc",..
      expected = '(';
      expected_end = "),";   // ends on a comma or the paren
      ensure_not_mime = 1;   // refuse a mime type
      ensure_not_method = 1; // xhr.open: don't grab method
    }
  if (!nc)
    if ((nc = hts_skip_prefix_ci(&a, ".replace"))) { // window.replace("url")
      expected = '(';
      expected_end = ")";
    }
  if (!nc)
    if ((nc = hts_skip_prefix_ci(&a, ".link"))) { // window.link("url")
      expected = '(';
      expected_end = ")";
    }
  if (!nc)
    if ((nc = hts_skip_prefix_ci(&a, ".url"))) { // obj.url("doc")
      expected = '(';
      expected_end = ")";
    }
  /* The guard has to gate the match: a name ending in "url" must leave nc at
     zero, or the default rule takes its assignment as a link. */
  if (!nc &&
      !(in_css ? css_glues_keyword(cursor, buffer)
               : js_glues_keyword(cursor, buffer)) &&
      (nc = hts_skip_prefix_ci(&a, "url"))) { // url(url)
    expected = '(';
    expected_end = ")";
    /* CSS writes url(foo.png) unquoted, but JavaScript's new URL(x) matches
       the same token, so an unquoted operand there rewrites the expression
       itself. */
    can_avoid_quotes = in_css;
    quotes_replacement = ')';
  }
  if (!nc)
    if ((nc = hts_skip_prefix_ci(&a, "import"))) { // import "url"
      if (is_space(*a)) {
        expected = 0; // no char expected
        is_import = 1;
      } else {
        nc = HTS_FALSE;
        a = cursor;
      }
    }
  if (!nc) { // import x from "./mod.js"
    /* The guard has to gate the match: a name merely ending in "from" must
       leave nc at zero, or the default rule takes its assignment as a link.
       A quote before it ends a string, and no import clause starts there. */
    const char before = html_prevc(cursor, buffer);

    if (!js_glues_keyword(cursor, buffer) && before != '"' && before != '\'' &&
        before != '`' && (nc = hts_skip_prefix_ci(&a, "from"))) {
      expected = 0;
      is_specifier = 1;
    }
  }
  if (!nc)
    return HTS_FALSE;

  while (is_realspace(*a))
    a++;
  if ((*a != expected) && expected)
    return HTS_FALSE;
  if (expected)
    a++;
  while (is_realspace(*a))
    a++;
  if ((*a != 34) && (*a != '\'') && (!can_avoid_quotes))
    return HTS_FALSE;

  {
    const char *b, *c;
    int ndelim = 1;    // length of the opening delimiter
    int close_len = 1; // length of the closing delimiter
    int valid_url = 0;
    // a style attribute may quote url() with an entity: url(&quot;a.png&quot;)
    const char entity_quote =
        in_tag && can_avoid_quotes ? html_entity_quote(a, &ndelim) : '\0';

    if (entity_quote != '\0')
      a += ndelim;
    else if ((*a == 34) || (*a == '\''))
      a++;
    else
      ndelim = 0;
    b = a;
    if (entity_quote != '\0') {
      while (*b != 34 && *b != '\'' && *b != '>' && *b != '\0' &&
             html_entity_quote(b, &close_len) != entity_quote)
        b++;
      // a raw quote or '>' ends the attribute before the closing entity
      if (*b == 34 || *b == '\'' || *b == '>')
        return HTS_FALSE;
    } else if (ndelim) {
      while ((*b != 34) && (*b != '\'') && (*b != '\0'))
        b++;
    } else {
      close_len = 0;
      // Bounded scan: a url( not closed within 2 KB is not taken as a link.
      while ((*b != quotes_replacement) && (*b != '\0')) {
        if (b - a >= HTS_URLMAXSIZE * 2)
          return HTS_FALSE;
        b++;
      }
    }
    c = b--;
    // no closing delimiter here (truncated input):
    // Don't scan past the buffer NUL or capture it.
    if (*c != '\0') {
      c += close_len;
      while (*c == ' ')
        c++;
      valid_url = (strchr(expected_end, *c)) || (*c == '\n') || (*c == '\r') ||
                  (is_import && *(b + 1 + close_len) == ' ');
    }
    if (!valid_url)
      return HTS_FALSE;
    // URL end = last char (b), not the delimiter
    c = b;
    if ((int) (c - a + 1) == 0) // nothing between the delimiters
      return HTS_FALSE;
    if (ensure_not_mime) {
      int i = 0;

      while (a != NULL && hts_main_mime[i] != NULL &&
             hts_main_mime[i][0] != '\0') {
        const char *slash = a;

        if (hts_skip_prefix_ci(&slash, hts_main_mime[i]) && *slash == '/') {
          a = NULL;
        }
        i++;
      }
    }
    // XHR.open's "GET" etc. is a method, not a URL
    if (a != NULL && ensure_not_method &&
        is_http_method(hts_span_at(a, (size_t) (c - a + 1))))
      a = NULL;
    // "jquery" names a module the loader resolves, never a file
    if (a != NULL && is_specifier && !js_specifier_is_url(a))
      a = NULL;
    // Check for bogus links (Vasiliy)
    if (a != NULL) {
      const size_t size = c - a + 1;
      size_t i;
      int first = 1;

      for (i = 0; i < size; i++) {
        // A template literal interpolates code here, so this is not a path.
        if (a[i] == '$' && i + 1 < size && a[i + 1] == '{') {
          a = NULL;
          break;
        }
        // Suspicious (in code ?), abort.
        if (a[i] == ',' || a[i] == ';') {
          if (first) {
            a = NULL;
            break;
          }
        }
        // Suspicious, abort.
        else if (a[i] == '"' || a[i] == '\'' || a[i] == '\t' || a[i] == '\r' ||
                 a[i] == '\n') {
          a = NULL;
          break;
        } else if (a[i] != ' ') {
          first = 0;
        }
      }
    }
    if (a == NULL)
      return HTS_FALSE;
    link->offset = (int) (a - cursor);
    link->length = (int) (c - a + 1);
    link->unquoted_end =
        can_avoid_quotes && !ndelim ? quotes_replacement : '\0';
    link->escape_parens = can_avoid_quotes ? HTS_TRUE : HTS_FALSE;
    link->entity_quote = entity_quote;
    /* CSS @import hits the same keyword, and module_base is inert in CSS. */
    link->is_module = (is_import || is_specifier) && !in_css;
    return HTS_TRUE;
  }
}

/* Where parse_fetch_meta's link was found. */
typedef struct parse_fetch_flags {
  hts_boolean redirect_stub; /* the page written in place of a redirect */
  hts_boolean meta_refresh;
  hts_boolean opaque; /* inside media, or of a type the parser forced */
  hts_boolean inscript;
  hts_boolean incss;
  hts_boolean tagless_body;
} parse_fetch_flags;

/* How a browser fetches the link found while parsing link ptr. Links in a
   stylesheet or script file compare with the page that loaded that file. */
static void parse_fetch_meta(httrackp *opt, int ptr, hts_fetch_meta *fm,
                             const parse_fetch_flags *flags, int link_frame,
                             hts_fetch_tag *tag_cache, const char *tag_name,
                             const char *attr, const char *fil) {
  const hts_fetch_meta *const page = &heap(ptr)->fetch;

  memset(fm, 0, sizeof(*fm));
  if (ptr == 0) { /* a start URL, as if typed in */
    fm->dest = HTS_FETCH_DEST_DOCUMENT;
    fm->site = HTS_FETCH_SITE_NONE;
    fm->user = HTS_TRUE;
    return;
  }
  /* The page written in place of a redirect continues its chain. */
  if (flags->redirect_stub) {
    *fm = *page;
    hts_fetch_through(opt, fm, heap(ptr)->adr);
    return;
  }
  /* A refresh navigates whatever frame holds the page. */
  if (flags->meta_refresh)
    fm->dest = (unsigned char) hts_fetch_same_frame(page->dest);
  else if (!flags->opaque && flags->inscript && flags->incss)
    hts_fetch_classify(fm, tag_cache, NULL, attr, fil, HTS_FETCH_DEST_UNKNOWN);
  /* A script body names URLs for code a crawler never runs. */
  else if (!flags->opaque && !flags->inscript && tag_name != NULL)
    hts_fetch_classify(fm, tag_cache, tag_name, attr, fil, link_frame);
  /* A file no known page loaded leaves initiator 0, which sends none. */
  fm->initiator = flags->tagless_body ? page->initiator : ptr;
}

/* Is this raw data-src / data-srcset value a lazy-load placeholder rather
   than a URL? Templates ({{x}}, JSON) and a few placeholder words are. */
static hts_boolean data_attr_is_not_url(hts_span s) {
  static const char *const words[] = {
      "lazy",  "placeholder", "none",  "null",  "undefined",
      "false", "true",        "blank", "empty", NULL};
  hts_span head;

  while (!hts_span_empty(s) && hts_span_first(s) <= ' ')
    s = hts_span_skip(s, 1);
  while (!hts_span_empty(s) && hts_span_last(s) <= ' ')
    s.len--;
  head = hts_span_at(s.p, hts_span_find(s, '?')); /* a query may carry JSON */
  if (hts_span_find_any(head, "{}<>\"") < head.len)
    return HTS_TRUE;
  return hts_span_index_ci(s, words) >= 0;
}

/* Percent-encode '(' and ')' in a link emitted into an unquoted url(...) (CSS
   or JS): a literal ')' closes the token early and the UA mis-parses the value
   (#163). The UA decodes %28/%29 back to the saved-on-disk name. */
static void escape_url_parens(hts_buf s) {
  char BIGSTK buff[HTS_URLMAXSIZE * 2];
  size_t i, j;

  for (i = 0, j = 0; s.p[i] != '\0' && j + 3 < s.size && j + 3 < sizeof(buff);
       i++) {
    if (s.p[i] == '(' || s.p[i] == ')') {
      buff[j++] = '%';
      buff[j++] = '2';
      buff[j++] = s.p[i] == '(' ? '8' : '9';
    } else {
      buff[j++] = s.p[i];
    }
  }
  buff[j] = '\0';
  hts_buf_cpy(s, buff);
}

/* Copy a URL piece with " ' < > percent-encoded, so it cannot end the
   attribute or script string it is written into; with html, & becomes &amp;.
   Returns the length written, or dest.size on overflow. */
static size_t escape_url_quotes_any(const char *const s, hts_buf dest,
                                    const hts_boolean html) {
  static const char hex[] = "0123456789abcdef";
  size_t i, j = 0;

  if (dest.size == 0)
    return 0;
  for (i = 0; s[i] != '\0'; i++) {
    const unsigned char c = (unsigned char) s[i];
    char out[5];
    size_t n = 1;

    out[0] = (char) c;
    if (c == '"' || c == '\'' || c == '<' || c == '>') {
      out[0] = '%';
      out[1] = hex[c / 16];
      out[2] = hex[c % 16];
      n = 3;
    } else if (html && c == '&') {
      memcpy(out, "&amp;", 5);
      n = 5;
    }
    if (n >= dest.size - j) {
      dest.p[j] = '\0';
      return dest.size;
    }
    memcpy(dest.p + j, out, n);
    j += n;
  }
  dest.p[j] = '\0';
  return j;
}

static size_t escape_url_quotes(const char *const s, char *const dest,
                                const size_t size) {
  return escape_url_quotes_any(s, hts_buf_at(dest, size), HTS_FALSE);
}

static size_t escape_url_quotes_html(const char *const s, char *const dest,
                                     const size_t size) {
  return escape_url_quotes_any(s, hts_buf_at(dest, size), HTS_TRUE);
}

/* The rewritten page htsparse() builds. */
typedef struct parse_sink {
  TypedArray(char) buf;
  /* Latched when buf cannot grow. */
  hts_boolean oom;
  /* Every write is dropped when the page is not saved as HTML. */
  hts_boolean enabled;
  /* The parser's script state: script text is raw, but a handler and the
     HTML a write string holds are markup. */
  const int *inscript;
  const int *inscript_tag;
  const int *inscriptgen;
} parse_sink;

typedef size_t (*sink_escaper)(const char *const s, char *const dest,
                               const size_t size);

static void sink_add_n(parse_sink *const sink, const hts_span s) {
  if (!sink->enabled)
    return;
  if (TypedArrayReserve(sink->buf, s.len)) {
    TypedArrayAppend(sink->buf, s.p, s.len);
  } else {
    sink->oom = HTS_TRUE;
  }
}

static void sink_add(parse_sink *const sink, const char *const s) {
  sink_add_n(sink, hts_span_of(s));
}

/* Set the buffer's size, latching oom rather than claiming bytes it lacks. */
static void sink_set_size(parse_sink *const sink, const size_t size) {
  if (size <= TypedArrayCapa(sink->buf)) {
    TypedArraySize(sink->buf) = size;
  } else {
    sink->oom = HTS_TRUE;
  }
}

/* Append 's' through 'escape', which grows a byte to at most 'factor'. */
static void sink_add_escaped(parse_sink *const sink, const char *const s,
                             const sink_escaper escape, const size_t factor) {
  size_t want;

  if (!sink->enabled)
    return;
  want = strlen(s) * factor + 1024;
  if (TypedArrayReserve(sink->buf, want)) {
    TypedArraySize(sink->buf) +=
        escape(s, &TypedArrayTail(sink->buf), TypedArrayRoom(sink->buf));
  } else {
    sink->oom = HTS_TRUE;
  }
}

/* Is the parser inside script or style text, where no entity is decoded? */
static hts_boolean sink_raw_text(const parse_sink *const sink) {
  return *sink->inscript && !*sink->inscript_tag && !*sink->inscriptgen;
}

/* Append 's', with & html-escaped outside raw script text. */
static void sink_add_htmlescaped(parse_sink *const sink, const char *const s) {
  if (sink_raw_text(sink)) {
    sink_add(sink, s);
  } else {
    sink_add_escaped(sink, s, escape_for_html_print, HTS_HTMLESCAPE_MAXEXP);
  }
}

/* Append 's' with & and high bytes html-escaped. Its one caller passes a
   percent-encoded path, so no bare & reaches it. */
static void sink_add_htmlescaped_full(parse_sink *const sink,
                                      const char *const s) {
  sink_add_escaped(sink, s, escape_for_html_print_full,
                   HTS_HTMLESCAPE_FULL_MAXEXP);
}

/* Append the URL host or path 's' with " ' < > percent-encoded, then
   html-escaped like sink_add_htmlescaped(). */
static void sink_add_url_htmlescaped(parse_sink *const sink,
                                     const char *const s) {
  if (sink_raw_text(sink)) {
    sink_add_escaped(sink, s, escape_url_quotes, 3);
  } else {
    sink_add_escaped(sink, s, escape_url_quotes_html, HTS_HTMLESCAPE_MAXEXP);
  }
}

/* Append the absolute URL of adr and fil, with the login unless privacy is on.
   An adr with no "//" gets "http://", and is written as it is if noauth_raw. */
static void sink_add_absolute_url(parse_sink *const sink, const char *const adr,
                                  const char *const fil,
                                  const hts_boolean passprivacy,
                                  const hts_boolean noauth_raw) {
  const hts_boolean has_authority = link_has_authority(adr);

  if (!has_authority) {
    sink_add(sink, "http://");
  } else {
    const char *const aut = strstr(adr, "//");

    sink_add_n(sink, hts_span_at(adr, (size_t) (aut - adr) + 2));
  }
  if (passprivacy)
    sink_add_url_htmlescaped(sink, jump_identification_const(adr));
  else if (!has_authority && noauth_raw)
    sink_add_url_htmlescaped(sink, adr);
  else
    sink_add_url_htmlescaped(sink, jump_protocol_const(adr));
  if (fil[0] != '/')
    sink_add(sink, "/");
  sink_add_url_htmlescaped(sink, fil);
}

const char *hts_parse_escape_selftest(const hts_boolean parens,
                                      const char *const s, hts_buf out) {
  if (!parens)
    return html_inline_safe(s, out);
  hts_buf_cpy(out, s);
  escape_url_parens(out);
  return out.p;
}

char *hts_parse_sink_selftest(const hts_boolean enabled, const int inscript,
                              const int inscript_tag, const char *const s) {
  const int inscriptgen = 0;
  parse_sink sink = {.buf = EMPTY_TYPED_ARRAY,
                     .oom = HTS_FALSE,
                     .enabled = enabled,
                     .inscript = &inscript,
                     .inscript_tag = &inscript_tag,
                     .inscriptgen = &inscriptgen};
  char *copy;

  sink_add(&sink, s);
  sink_add(&sink, "|");
  sink_add_htmlescaped(&sink, s);
  sink_add(&sink, "|");
  sink_add_htmlescaped_full(&sink, s);
  sink_add(&sink, "|");
  sink_add_url_htmlescaped(&sink, s);
  assertf(!sink.oom);
  copy = (char *) malloct(TypedArraySize(sink.buf) + 1);
  assertf(copy != NULL);
  if (TypedArraySize(sink.buf) != 0)
    memcpy(copy, TypedArrayElts(sink.buf), TypedArraySize(sink.buf));
  copy[TypedArraySize(sink.buf)] = '\0';
  TypedArrayFree(sink.buf);
  return copy;
}

/* Default port for lien's scheme (case-insensitive), 80 if absent/unknown; so
   schemeless and protocol-relative //host links default to 80 (known gap). */
static int scheme_default_port(const char *lien) {
  if (hts_starts_with_ci(lien, "https:"))
    return 443;
  if (hts_starts_with_ci(lien, "ftp:"))
    return 21;
  return 80;
}

/* Strip the scheme's own default port (80 http, 443 https, 21 ftp) from lien's
   authority in place; :80 on https/ftp stays as a real port (#638, #614). */
void hts_strip_default_port(hts_buf link) {
  char *const lien = link.p;
  char *a;

  if (!link_has_authority(lien))
    return;
  a = strstr(lien, "//"); // "//" authority
  if (a)
    a += 2;
  else
    a = lien;
  a = jump_toport(a);
  if (a) { // port present
    char *b = a + 1;
    char saved;
    int port;
    hts_boolean is_default;

    while (hts_isdigit((unsigned char) *b))
      b++;
    saved = *b;
    *b = '\0';
    is_default =
        hts_parse_url_port(a + 1, &port) && port == scheme_default_port(lien);
    *b = saved;
    if (is_default) { // default port, strip it
      char BIGSTK tempo[HTS_URLMAXSIZE * 2];

      tempo[0] = '\0';
      strncatbuff(tempo, lien, a - lien);
      strcatbuff(tempo, b); // skip the whole matched :port
      hts_buf_cpy(link, tempo);
    }
  }
}

/* Main parser */
/* Say once that the -#L cap is full, and record its abort (#1419). The parse
   then finishes and keeps the page it has rewritten. */
void maxlinks_give_up(httrackp *opt, volatile int *exit_xh) {
  if (hts_load_acquire_int(exit_xh) == -1)
    return;
  hts_log_print(opt, LOG_PANIC, "Too many URLs, giving up..(>%d)",
                opt->maxlink);
  hts_log_print(opt, LOG_INFO,
                "To avoid that: use #L option for more links "
                "(example: -#L1000000)");
  hts_store_release_int(exit_xh, -1);
}

/* Decode the escapes and entities of a link, convert its path to UTF-8 and
   re-escape its query. Sets *stre->error_ when the link no longer fits. */
static void decode_link_escapes(htsmoduleStruct *str,
                                htsmoduleStructExtended *stre, hts_buf link,
                                hts_buf catbuff) {
  ENGINE_LOAD_CONTEXT();
  char *const lien = link.p;

  // NULL when UTF-8 conversion is off (-%T0)
  hts_boolean declared;
  const char *const charset = page_charset(str, &declared);
  const int hasCharset = charset != NULL;
  char BIGSTK query[HTS_URLMAXSIZE * 2];

  // cut query string
  {
    char *const a = strchr(lien, '?');
    if (a != NULL) {
      strcpybuff(query, a);
      *a = '\0';
    } else {
      query[0] = '\0';
    }
  }

  // Unescape %XX, but not yet high-chars (supposedly encoded with UTF-8)
  hts_buf_cpy(link,
              unescape_http_unharm(catbuff.p, catbuff.size, lien,
                                   1 | 2)); /* note: '%' is still escaped */

  // Percent-encode the control bytes as a browser does (#982);
  // a byte grows to three, so a link that outgrows the buffer
  // is dropped rather than clipped to a URL nobody wrote.
  {
    char BIGSTK tempo[HTS_URLMAXSIZE * 2];

    if (escape_control_url(lien, HTS_BUF(tempo)) <
        sizeof(tempo) - HTS_LINK_TAIL_ROOM) {
      hts_buf_cpy(link, tempo);
    } else {
      *stre->error_ = 1;
      hts_log_print(opt, LOG_DEBUG,
                    "link rejected (control bytes do not fit) %s",
                    url_nologin(opt, lien));
    }
  }

  // charset conversion for the URI filename (not the query
  // string), unless the bytes already are valid UTF-8:
  // converting those would double-encode them (#180)
  if (hasCharset && !hts_isCharsetUTF8(charset) &&
      !hts_isStringUTF8(hts_span_of(lien))) {
    char *s = hts_convertStringToUTF8(lien, strlen(lien), charset);
    if (s != NULL) {
      hts_log_print(opt, LOG_DEBUG,
                    "engine: save-name: '%s' charset "
                    "conversion from '%s' to '%s'",
                    charset, url_nologin(opt, lien), url_nologin(opt, s));
      /* One byte can become three, and a clipped URL names
         another resource, so drop the link instead. */
      if (strlen(s) < link.size - HTS_LINK_TAIL_ROOM) {
        hts_buf_cpy(link, s);
      } else {
        *stre->error_ = 1;
        hts_log_print(opt, LOG_WARNING,
                      "Link is too long after charset conversion: %s",
                      url_nologin(opt, lien));
      }
      freet(s);
    }
  }

  // decode URI entities with UTF-8 charset
  if (hts_unescapeEntities(lien, link) != 0) {
    hts_log_print(opt, LOG_WARNING, "could not decode URI '%s' as UTF-8",
                  url_nologin(opt, lien));
  }

  // decode query entities with the page charset, out-of-place
  // because the escape grows it (#854)
  if (hasCharset) {
    char BIGSTK decoded[sizeof(query)];

    if (hts_unescapeEntitiesWithCharsetSpecial(query, HTS_BUF(decoded), charset,
                                               UNESCAPE_ENTITIES_URL_QUERY) ==
        0) {
      strcpybuff(query, decoded);
    } else {
      hts_log_print(opt, LOG_WARNING,
                    "could not decode query string '%s' with "
                    "charset '%s'",
                    query, charset);
    }
  }

  // Decode remaining %XX high characters with UTF-8
  // but only when this leads to valid UTF-8.
  // Otherwise, leave them unescaped.
  if (hts_unescapeUrlSpecial(lien, catbuff, UNESCAPE_URL_NO_ASCII) == 0) {
    hts_buf_cpy(link, catbuff.p);
  } else {
    hts_log_print(opt, LOG_WARNING, "could not URL-decode string '%s'",
                  url_nologin(opt, lien));
  }

  // we need to encode query string non-ascii chars,
  // leaving the encoding as-is (unlike the file part)
  // and copy back query
  {
    const size_t cap = link.size - HTS_LINK_TAIL_ROOM;
    const size_t used = strlen(lien);

    // the append grows the query too, and clips silently on
    // overflow: drop, or we fetch a query nobody wrote (#982)
    if (used >= cap ||
        append_escape_check_url(query, hts_buf_at(lien, cap)) >= cap - used) {
      *stre->error_ = 1;
      hts_log_print(opt, LOG_DEBUG, "link rejected (query does not fit) %s",
                    url_nologin(opt, lien));
    }
  }
}

/* Store a base or codebase link, or prefix another link with the base that
   applies. */
static void resolve_link_base(htsmoduleStruct *str,
                              htsmoduleStructExtended *stre, hts_buf link,
                              hts_link_role p_type, hts_boolean p_xmlbase) {
  ENGINE_LOAD_CONTEXT();
  char *const lien = link.p;

  hts_log_print(opt, LOG_DEBUG, "position link check %s",
                url_nologin(opt, lien));

  if (link_role_is_base(p_type)) {
    // a codebase names a directory
    if (p_type == HTS_LINK_CODEBASE) {
      if (strnotempty(lien)) {
        if (hts_lastchar(lien) != '/') {
          hts_buf_cat(link, "/");
        }
      }
    }

    /* A base with no authority is relative per RFC 3986 5.2,
       except the hostname shape legacy markup writes. */
    if (p_type == HTS_LINK_BASE && !link_has_authority(lien) &&
        link_base_is_hostname(lien)) {
      char BIGSTK tmp[HTS_URLMAXSIZE * 2];

      strcpybuff(tmp, "http://");
      strcatbuff(tmp, lien);
      hts_buf_cpy(link, tmp);
    }

    /* only one ending / (bug on some pages) */
    if (strlen(lien) > 2) {
      size_t len = strlen(lien);

      while (len > 1 && lien[len - 1] == '/' &&
             lien[len - 2] == '/') /* double // (bug) */
        lien[--len] = '\0';
    }
    // resolve a relative base against the page
    if (!link_has_authority(lien)) {
      lien_adrfil af2;

      if (ident_url_relatif(lien, urladr(), urlfil(), &af2) < 0) {
        *stre->error_ = 1;
      } else if (strlen(af2.adr) + strlen(af2.fil) >=
                 link.size - HTS_LINK_TAIL_ROOM - sizeof("http:///")) {
        /* the rebuild below is unbounded in the two of them */
        *stre->error_ = 1;
        hts_log_print(opt, LOG_WARNING, "Link is too long once resolved: %s",
                      url_nologin(opt, lien));
      } else {
        hts_buf_cpy(link, "http://");
        hts_buf_cat(link, af2.adr);
        if (*af2.fil != '/')
          hts_buf_cat(link, "/");
        hts_buf_cat(link, af2.fil);
        {
          char *a;

          a = hts_lastcharptr(lien);
          while ((*a) && (*a != '/') && (a > lien))
            a--;
          if (*a == '/') {
            *(a + 1) = '\0';
          }
        }
      }
    }

    if (!*stre->error_) {
      if (p_type == HTS_LINK_BASE) {
        char *a = hts_lastcharptr(lien);
        char *start_of_filename = jump_identification(lien);

        if (start_of_filename != NULL &&
            (start_of_filename = strchr(start_of_filename, '/')) != NULL)
          start_of_filename++;
        if (start_of_filename == NULL)
          hts_buf_cat(link, "/");
        while ((a > lien) && (*a) && (*a != '/'))
          a--;
        if (*a == '/') {
          if (start_of_filename != NULL && a + 1 >= start_of_filename) {
            *(a + 1) = '\0'; // keep the directory
          }
        } else {
          *lien = '\0';
          *stre->error_ = 1;
        }
      }
      // remember the base or the codebase
      if (p_type == HTS_LINK_BASE) {
        // browsers ignore xml:base in HTML
        if (!p_xmlbase || !is_html_mime_type(r->contenttype))
          strlcpybuff(base, lien, HTS_URLMAXSIZE * 2);
      } else {
        strlcpybuff(codebase, lien, HTS_URLMAXSIZE * 2);
      }

      hts_log_print(opt, LOG_DEBUG, "code/codebase link %s base %s",
                    url_nologin(opt, lien), url_nologin(opt, base));
    }

  } else {
    char *_base;

    if (p_type == HTS_LINK_APPLET_CODE)
      _base = codebase;
    else
      _base = base;

    // prefix the base href
    if (strnotempty(_base)) {
      if (!link_has_authority(lien)) {
        if (*lien != '/') { // relative to the directory
          if ((strlen(_base) + strlen(lien)) < HTS_URLMAXSIZE) {
            lien_adrfil af;

            // mailto: and co: do NOT add base
            if (ident_url_relatif(lien, urladr(), urlfil(), &af) >= 0) {
              char BIGSTK tempo[HTS_URLMAXSIZE * 2];

              strcpybuff(tempo, _base);
              strcatbuff(tempo, lien + ((*lien == '/') ? 1 : 0));
              hts_buf_cpy(link, tempo);

              hts_log_print(opt, LOG_DEBUG,
                            "link modified with code/codebase %s",
                            url_nologin(opt, lien));
            }
          } else {
            *stre->error_ = 1;
            hts_log_print(opt, LOG_ERROR, "Link %s too long with base href",
                          url_nologin(opt, lien));
          }
        } else {
          lien_adrfil baseaf;
          if (ident_url_absolute(_base, &baseaf) >= 0) {
            if ((strlen(baseaf.adr) + strlen(lien)) < HTS_URLMAXSIZE) {
              char BIGSTK tempo[HTS_URLMAXSIZE * 2];

              tempo[0] = '\0';
              if (!link_has_authority(baseaf.adr)) {
                strcatbuff(tempo, "http://");
              }
              strcatbuff(tempo, baseaf.adr);
              strcatbuff(tempo, lien);
              hts_buf_cpy(link, tempo);

              hts_log_print(opt, LOG_DEBUG,
                            "link modified with code/codebase %s",
                            url_nologin(opt, lien));
            } else {
              *stre->error_ = 1;
              hts_log_print(opt, LOG_ERROR, "Link %s too long with base href",
                            url_nologin(opt, lien));
            }
          }
        }
      }
    }
  }
}

/* Write the index.html entry of the page, under its title if it has one. */
static void makeindex_add_entry(htsmoduleStruct *str,
                                htsmoduleStructExtended *stre, const char *html,
                                const hts_boolean has_title,
                                int *detect_title) {
  ENGINE_LOAD_CONTEXT();

  if (*stre->makeindex_fp_ == NULL) {
    file_notify(opt, "", "",
                hts_optpath(opt, StringBuff(opt->path_html_utf8), "index.html"),
                1, 1, 0);
    verif_backblue(opt, StringBuff(opt->path_html_utf8));
    *stre->makeindex_fp_ = filecreate(
        &opt->state.strc,
        hts_optpath(opt, StringBuff(opt->path_html_utf8), "index.html"));
    if (*stre->makeindex_fp_ != NULL) {

      // Header
      hts_template_format(*stre->makeindex_fp_, template_header,
                          "<!-- Mirror and index made by HTTrack Website "
                          "Copier/" HTTRACK_VERSION " " HTTRACK_AFF_AUTHORS
                          " -->",
                          /* EOF */ NULL);

    } else
      *stre->makeindex_done_ = -1; // failed, give up
  }

  if (*stre->makeindex_fp_ != NULL) {
    char BIGSTK tempo[HTS_URLMAXSIZE * 2];
    char BIGSTK s[HTS_URLMAXSIZE * 2];
    char *a = NULL;
    char *b = NULL;

    s[0] = '\0';
    if (has_title) {
      a = strchr(html, '>');
      if (a != NULL) {
        a++;
        while (is_space(*a))
          a++;
        b = strchr(a, '<'); // next tag
      }
    }
    const hts_buf index_path = OPT_GET_HTS_BUF(opt);

    /* the output path is capped well under the buffer */
    assertf(hts_path_join(index_path, StringBuff(opt->path_html_utf8),
                          "index.html"));
    if (lienrelatif(HTS_BUF(tempo), heap(ptr)->sav, index_path.p) == 0) {
      *detect_title = 1; // indexed for this page
      (*stre->makeindex_links_)++;
      strlcpybuff(makeindex_firstlink, tempo, HTS_URLMAXSIZE * 2);
      /* MHTML: the first link is the primary part */
      if (opt->mimehtml) {
        strlcpybuff(makeindex_firstlink, "cid:primary/primary",
                    HTS_URLMAXSIZE * 2);
      }

      if ((b == a) || (a == NULL) || (b == NULL)) { // no title
        strcpybuff(s, tempo);
      } else if ((b - a) < 256) {
        b--;
        while (is_space(*b))
          b--;
        strncpy(s, a, b - a + 1);
        *(s + (b - a) + 1) = '\0';
      }

      // Decode title with encoding
      {
        hts_boolean declared;
        const char *const charset = page_charset(str, &declared);

        // Guessed charset: never re-encode valid UTF-8 (#833)
        if (charset != NULL && !hts_isCharsetUTF8(charset) &&
            (declared || !hts_isStringUTF8(hts_span_of(s)))) {
          char *sUtf = hts_convertStringToUTF8(s, strlen(s), charset);

          if (sUtf != NULL) {
            /* UTF-8 can expand past s[]; truncate to fit */
            snprintf(s, sizeof(s), "%s", sUtf);
            freet(sUtf);
          }
        }
      }

      // Body
      inplace_escape_uri_utf(HTS_SIZED(tempo));
      hts_template_format(*stre->makeindex_fp_, template_body, tempo, s,
                          /* EOF */ NULL);
    }
  }
}

/* Add a first-level page to the top index.html, and close the index once a
   deeper page comes. */
static void makeindex_add_page(htsmoduleStruct *str,
                               htsmoduleStructExtended *stre, const char *html,
                               int *detect_title, const char *in_media) {
  ENGINE_LOAD_CONTEXT();
  int p = 0; // > 0: a title tag at html, -1: index the page with no title

  if (opt->depth == heap(ptr)->depth) { // always index the first-level pages
    if (!in_media) {
      if (opt->makeindex && (ptr > 0)) {
        if (opt->getmode & HTS_GETMODE_HTML) {
          p = hts_starts_with_ci(html, "title");
          if (p) {
            if (html_prevc(html, r->adr) == '/')
              p = 0; // </title>
          } else {
            if (hts_starts_with_ci(html, "/html"))
              p = -1;
            else if (hts_starts_with_ci(html, "body"))
              p = -1;
            else if (html - r->adr >= r->size - 1)
              p = -1;
            else if (html - r->adr >= r->size - 2) // we got to hurry
              p = -1;
          }
        } else
          p = 0;

        if (p)
          makeindex_add_entry(str, stre, html, p > 0, detect_title);
      }
    }

  } else if (heap(ptr)->depth < opt->depth) { // deeper page: close the index
    hts_finish_makeindex(opt, stre->makeindex_done_, stre->makeindex_fp_,
                         *stre->makeindex_links_, makeindex_firstlink,
                         template_footer, "primary", "primary");
  }
}

/* Does the meta tag at TAG_START, whose name attribute is at NAME_ATTR, ask
   robots not to follow its links? Logs it when it does. */
static hts_boolean meta_robots_nofollow(htsmoduleStruct *str,
                                        const char *tag_start,
                                        const char *name_attr) {
  ENGINE_LOAD_CONTEXT_BASE();
  char tempo[1100];
  const char *a;
  const char *name;
  const int name_len =
      rech_endtoken(name_attr + rech_tageq(name_attr, "name"), &name);
  hts_span value = hts_span_at(name, name_len);

  value.len = hts_span_find(value, '>'); // an unquoted value runs past the tag
  if (!hts_span_eq_ci(hts_span_trim(value, "", HTS_REALSPACES "/"),
                      hts_span_of("robots")))
    return HTS_FALSE;
  a = tag_close_within(name_attr, 999);
  if (a == NULL)
    return HTS_FALSE;
  if (a - tag_start >= (ptrdiff_t) sizeof(tempo)) // too long: from name only
    tag_start = name_attr;
  tempo[0] = '\0';
  strncatbuff(tempo, tag_start, a - tag_start); // content may precede name
  if (strstrcase(tempo, "content") && strstrcase(tempo, "nofollow")) {
    hts_log_print(opt, LOG_WARNING,
                  "Link %s%s not scanned "
                  "(follow robots meta tag)",
                  adr_nologin(opt, urladr()), urlfil());
    return HTS_TRUE;
  }
  return HTS_FALSE;
}

/* gcc keeps a per-byte helper out of line otherwise (+0.4% instructions) */
#if defined(__GNUC__)
#define PARSE_INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define PARSE_INLINE __forceinline
#else
#define PARSE_INLINE inline
#endif

/* htsparse()'s state that lives across iterations of its byte loop. */
typedef struct parse_state {
  const char *html;      /* parse cursor */
  const char *lastsaved; /* one past the last byte copied to the sink */

  struct {                       /* the tag being scanned */
    int intag;                   /* inside a tag */
    const char *intag_start;     /* the '<' of the current tag */
    const char *intag_name;      /* the current tag's name */
    const char *intag_startattr; /* start of the current attribute */
    int intag_start_valid;       /* intag_start points at a real tag */
    int intag_ctype; /* meta http-equiv: 1 content-type, 2 refresh */
    const char *quoted_value_end; /* Where does the quoted value close? */
  } tag;

  struct {                        /* script, style and comment state */
    int incomment;                /* inside <!-- */
    const char *comment_close;    /* cached "-->" at or after html */
    hts_boolean comment_unclosed; /* no "-->" left in the page */
    int inscript;                 /* inside a script or style body */
    int incss;                    /* CSS lets url(x) go unquoted */
    int inscript_locked; /* a .js or .css file: </script> does not exit */
    INSCRIPT inscript_state_pos; /* automaton state */
    const char *inscript_name;   /* the tag that opened the script */
    int inscript_tag;        /* inside an event handler such as onLoad="..." */
    char inscript_tag_lastc; /* the quote that ends that handler */
    const char *handler_value_end; /* Where does that value end, if known? */
    int inscriptgen;         /* inside generated HTML, as in obj.write("...") */
    const char *scriptgen_end; /* the closing quote of that string */
    hts_boolean css_file;    /* the whole document is a stylesheet */
  } script;

  struct {                /* facts about the whole page */
    const char *in_media; /* media link list kind ("LNK", "AAM"), or NULL */
    int nofollow;         /* stop scanning links */
    int parent_relative;  /* links resolve against the parent (.js) */
    int link_frame;       /* what a link with no target navigates, once known */
    hts_fetch_tag fetch_tag;        /* the tag a fetched link came from */
    int detect_title;               /* this page entered the index */
    int emited_footer;              /* footer comment tags emitted */
    hts_boolean bare_html_end;      /* </html> seen before any footer */
    int parseall_lastc;             /* last byte seen by the parseall scan */
    hts_boolean sf_may_mark;        /* --single-file: mark this one? */
    time_t user_interact_timestamp; /* last user-interaction poll */
    int back_add_stats;       /* opt->state.back_add_stats at the last check */
    unsigned int stats_calls; /* engine_stats_tick() counter */
  } page;

  parse_sink sink; /* output HTML */
} parse_state;

/* What htsparse() knows about the link at the cursor, reset for each byte. */
typedef struct parse_link {
  int p;       /* offset from html to a link value, 0 none, -1 stop looking */
  int valid_p; /* take the link even when p is 0 */
  int module_base;           /* module specifier: resolve against the script */
  int ending_p;              /* the quote that ends the value, or 0 */
  hts_boolean escape_parens; /* is this a CSS url()? */
  char entity_end;           /* quote of an entity-quoted url() */
  int archivetag_p;          /* archive=: comma-separated archives */
  int srcset_p;              /* srcset="url1 480w, url2 2x": list of URLs */
  int dataattr_p;            /* data-src / data-srcset: may hold a template */
  int unquoted_script;       /* an unquoted script link: quotes do not end it */
  hts_link_role p_type;      /* what the link is for */
  hts_boolean p_xmlbase;     /* the base came from xml:base= */
  int p_nocatch;             /* write the link but do not fetch it */
  int p_searchMETAURL;       /* look for ..URL=<url> */
  int add_class;             /* add .class if missing */
} parse_link;

/* Emit the --footer, plus the charset meta when emited_footer is 1, so the
   caller counts the emission first. PTR is the page's heap index. */
static void parse_emit_footer(parse_state *const ps, const httrackp *const opt,
                              const htsblk *const r, const int ptr) {
  const char *eol = "\n";

  if (strchr(r->adr, '\r'))
    eol = "\r\n";
  if (StringNotEmpty(opt->footer) ||
      opt->urlmode != HTS_URLMODE_KEEP_ORIGINAL) {
    if (StringNotEmpty(opt->footer)) {
      char BIGSTK tempo[1024 + HTS_URLMAXSIZE * 2];
      char gmttime[256];
      char BIGSTK safe_adr[HTS_URLMAXSIZE * 3 + 4];
      char BIGSTK safe_fil[HTS_URLMAXSIZE * 3 + 4];
      char BIGSTK safe_url[HTS_URLMAXSIZE * 6 + 16];
      char safe_lastmod[sizeof(r->lastmodified) * 3 + 4];
      char safe_ctype[sizeof(r->contenttype) * 3 + 4];
      char safe_charset[sizeof(r->charset) * 3 + 4];
      char status_str[16];
      char size_str[32];
      // {url} scheme: jump_identification_const strips it for
      // http/https/ftp, so re-add it (http for a bare host).
      const char *const url_host = jump_identification_const(urladr());
      const char *const url_scheme =
          strstr(url_host, "://") != NULL            ? ""
          : hts_starts_with_ci(urladr(), "https://") ? "https://"
          : hts_starts_with_ci(urladr(), "ftp://")   ? "ftp://"
                                                     : "http://";

      // credentials are already stripped and the scheme is a literal, so no
      // re-escaping
      html_inline_safe(url_host, HTS_BUF(safe_adr));
      html_inline_safe(urlfil(), HTS_BUF(safe_fil));
      snprintf(safe_url, sizeof(safe_url), "%s%s%s", url_scheme, safe_adr,
               safe_fil);
      snprintf(status_str, sizeof(status_str), "%d", r->statuscode);
      snprintf(size_str, sizeof(size_str), LLintP, (LLint) r->size);
      time_gmt_rfc822(HTS_BUF(gmttime));

      {
        // Network strings go through html_inline_safe(), because a
        // "-->" would close the footer's HTML comment (#165).
        const char *values[HTS_FOOTER_FIELD_COUNT] = {NULL};

        values[HTS_FOOTER_ADDR] = safe_adr;
        values[HTS_FOOTER_PATH] = safe_fil;
        values[HTS_FOOTER_URL] = safe_url;
        values[HTS_FOOTER_DATE] = gmttime;
        values[HTS_FOOTER_LASTMODIFIED] =
            html_inline_safe(r->lastmodified, HTS_BUF(safe_lastmod));
        values[HTS_FOOTER_VERSION] = HTTRACK_VERSIONID;
        values[HTS_FOOTER_MIME] =
            html_inline_safe(r->contenttype, HTS_BUF(safe_ctype));
        values[HTS_FOOTER_CHARSET] =
            html_inline_safe(r->charset, HTS_BUF(safe_charset));
        values[HTS_FOOTER_STATUS] = status_str;
        values[HTS_FOOTER_SIZE] = size_str;

        tempo[0] = '\0';
        strcatbuff(tempo, eol);
        // Overflow (<0) leaves tempo unterminated, and the closing
        // eol is reserved below because strcatbuff aborts rather
        // than clips: either one would kill the crawl.
        if (hts_footer_format(
                hts_buf_skip(hts_buf_at(tempo, sizeof(tempo) - strlen(eol)),
                             strlen(tempo)),
                StringBuff(opt->footer), values) >= 0) {
          strcatbuff(tempo, eol);
          HT_ADD(tempo);
        }
      }
    }
    // the charset meta goes with the first emission only
    if (ps->page.emited_footer == 1 && strnotempty(r->charset)) {
      HT_ADD("<!-- Added by HTTrack --><meta http-equiv=\"content-type\" "
             "content=\"text/html;charset=");
      HT_ADD(r->charset);
      HT_ADD("\" /><!-- /Added by HTTrack -->");
      HT_ADD(eol);
    }
  }
}

/* Does the QUOTE at E close the value opened just before S? It must be
   followed by a space, '>' or the end, and not be the opening quote of
   a later attribute, which an unclosed quote on a sloppy page would reach. */
static hts_boolean quote_closes_value(const char *const s, const char *const e,
                                      const char *const end) {
  const char *p = e;

  if (e + 1 < end && !is_realspace(e[1]) && e[1] != '>')
    return HTS_FALSE;
  while (p > s && is_realspace(p[-1]))
    p--;
  return p == s || p[-1] != '=';
}

/* Return the closing QUOTE of a value starting at S, or NULL when none looks
   like one, so '>' ends the tag as it always did. An unquoted value ('\0')
   ends at the first space or '>', or at the end of the page. */
static const char *attr_value_end(const char *const s, const char quote,
                                  const htsblk *const r) {
  const char *const end = r->adr + r->size;
  const char *e;

  if (quote == '\0') {
    for (e = s; e < end && !is_realspace(*e) && *e != '>'; e++)
      ;
    return e;
  }
  e = memchr(s, quote, (size_t) (end - s));
  return e != NULL && quote_closes_value(s, e, end) ? e : NULL;
}

/* Return where a handler value starting at S ends: QUOTE is the one that
   opened it, '\0' for none, or another byte (a newline) when nothing ends it
   but the legacy rule. */
static const char *handler_end_at(const char *const s, const char quote,
                                  const htsblk *const r) {
  return quote == '"' || quote == '\'' || quote == '\0'
             ? attr_value_end(s, quote, r)
             : NULL;
}

/* Is the cursor inside a quoted attribute value, where '<' and '>' are text? */
static hts_boolean in_quoted_value(const parse_state *const ps) {
  return ps->tag.intag && ps->tag.quoted_value_end != NULL &&
         ps->html < ps->tag.quoted_value_end;
}

/* Does the quote at the cursor open a value, being the first byte after '='? */
static hts_boolean quote_opens_value(const parse_state *const ps) {
  const char *p = ps->html - 1;

  while (p > ps->tag.intag_start && is_realspace(*p))
    p--;
  return p > ps->tag.intag_start && *p == '=';
}

/* Is the cursor inside an event handler whose known end is still ahead? */
static hts_boolean handler_holds_gt(const parse_state *const ps) {
  return ps->script.inscript_tag && ps->script.handler_value_end != NULL &&
         ps->html < ps->script.handler_value_end;
}

/* Is the cursor at or past the end of the event handler it is in? */
static hts_boolean handler_ends_here(const parse_state *const ps) {
  if (ps->script.handler_value_end != NULL)
    return !handler_holds_gt(ps);
  return ps->script.inscript_tag_lastc != '\0' &&
         *ps->html == ps->script.inscript_tag_lastc;
}

/* Start the tag this '<' opens, or the comment a '<!--' opens, and emit the
   footer at the first <head>, <body> or <meta>, and at </html>. */
static void parse_tag_open(parse_state *const ps, const httrackp *const opt,
                           const htsblk *const r, const int ptr,
                           char *const codebase) {
  ps->tag.intag = 1;
  ps->tag.intag_ctype = 0;
  ps->tag.quoted_value_end = NULL;
  ps->tag.intag_start = ps->html;
  for (ps->tag.intag_name = ps->html + 1; is_realspace(*ps->tag.intag_name);
       ps->tag.intag_name++)
    ;
  ps->tag.intag_start_valid = 1;
  codebase[0] = '\0'; // a codebase belongs to one tag

  // meta http-equiv: remember which header it overrides
  if (check_tag(ps->tag.intag_start, "meta")) {
    int pos;

    // <meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
    if ((pos = rech_tageq_all(ps->html, "http-equiv"))) {
      const char *token = NULL;
      int len = rech_endtoken(ps->html + pos, &token);

      if (len > 0) {
        if (hts_starts_with_ci(token, "content-type")) {
          ps->tag.intag_ctype = 1;
        } else if (hts_starts_with_ci(token, "refresh")) {
          ps->tag.intag_ctype = 2;
        }
      }
    }
  }

  if (opt->getmode & HTS_GETMODE_HTML) {
    int p = 0;
    switch (ps->page.emited_footer) {
    case 0:
      // emit footer and charset early, at the first <head>, <body> or <meta>
      // FIXME: this emits before <head> rather than inside it
      if (hts_starts_with_ci(ps->html, "<head>") ||
          tag_opens_with_space(ps->html, "<head") ||
          hts_starts_with_ci(ps->html, "<body>") ||
          tag_opens_with_space(ps->html, "<body") ||
          tag_opens_with_space(ps->html, "<meta")) {
        p = 1;
        ps->page.emited_footer++;
      } else if (tag_opens_whole(ps->html, "</html")) {
        ps->page.bare_html_end = HTS_TRUE;
      }
      break;
    case 1:
      // the footer again before </html>
      p = tag_opens_whole(ps->html, "</html");
      if (p)
        ps->page.emited_footer++;
      break;
    }

    if (p != 0)
      parse_emit_footer(ps, opt, r, ptr);
  }
  // <!-- opens a comment, not a tag
  if (*(ps->html + 1) == '!')
    if (*(ps->html + 2) == '-')
      if (*(ps->html + 3) == '-') {
        ps->tag.intag = 0;
        ps->script.incomment = 1;
        ps->tag.intag_start_valid = 0;
      }
}

/* End the tag, event handler or comment this '>' closes, entering a script
   or style body. */
static void parse_tag_close(parse_state *const ps, const httrackp *const opt) {
  if (ps->script.inscript_tag) {
    ps->script.inscript_tag = ps->script.inscript = 0;
    // reset the automaton on exit or its state leaks into plain HTML
    ps->script.inscript_state_pos = INSCRIPT_START;
    ps->script.inscriptgen = 0;
    ps->tag.intag = 0;
    ps->script.incomment = 0;
    ps->tag.intag_start_valid = 0;
    ps->tag.intag_name = NULL;
    if (opt->parsedebug) {
      HT_ADD("<@@ /inscript @@>");
    }
  } else if (!ps->script.incomment) {
    ps->tag.intag = 0;
    ps->tag.quoted_value_end = NULL;

    // enter the script or style body only now, once its src= is parsed
    if ((ps->tag.intag_start_valid) &&
        (check_tag(ps->tag.intag_start, "script") ||
         check_tag(ps->tag.intag_start, "style"))) {
      if (check_tag(ps->tag.intag_start, "script")) {
        ps->script.inscript_name = "script";
        ps->script.incss = 0;
      } else {
        ps->script.inscript_name = "style";
        ps->script.incss = 1;
      }
      ps->script.inscript = 1;
      ps->script.inscript_state_pos = INSCRIPT_START;
      ps->tag.intag = 1; // as after a <script> tag
      // the body is code, so the tag start is spent
      ps->tag.intag_start_valid = 0;
      if (opt->parsedebug) {
        HT_ADD("<@@ inscript @@>");
      }
    }
  } else {
    // a proper "-->" ending
    if ((*(ps->html - 1) == '-') && (*(ps->html - 2) == '-')) {
      ps->tag.intag = 0;
      ps->script.incomment = 0;
      ps->tag.intag_start_valid = 0;
      ps->tag.intag_name = NULL;
    }
#if GT_ENDS_COMMENT
    /* wrong comment ending */
    else {
      // browsers accept <!-- foo > when no "-->" follows in the page
      if (!ps->script.comment_unclosed &&
          (ps->script.comment_close == NULL ||
           ps->script.comment_close < ps->html)) {
        ps->script.comment_close = strstr(ps->html, "-->");
        if (ps->script.comment_close == NULL)
          ps->script.comment_unclosed = HTS_TRUE;
      }
      if (ps->script.comment_unclosed) {
        ps->tag.intag = 0;
        ps->script.incomment = 0;
        ps->tag.intag_start_valid = 0;
        ps->tag.intag_name = NULL;
      }
    }
#endif
  }
}

/* Step past the scanned byte, then skip plain text to the next '<'. */
static void parse_advance(parse_state *const ps, const htsblk *const r,
                          const int ptr) {
  const char *html = ps->html;

  html++; // automate will be checked next loop
  // lastsaved <= html <= r->adr + r->size at the flush: never the NUL
  if (html > r->adr + r->size) {
    html = r->adr + r->size;
    if (ps->lastsaved > html) // a skipped base href ends past the NUL
      ps->lastsaved = html;
  }

  if (ptr > 0) {
    if ((!ps->tag.intag)              /* Not in tag */
        && (!ps->script.inscript)     /* Not in (java)script */
        && (!ps->page.in_media)       /* Not in media */
        && (!ps->script.incomment)    /* Not in comment (<!--) */
        && (!ps->script.inscript_tag) /* Not in tag with script inside */
    ) {
      /* Not at the end */
      if (html - r->adr < r->size) {
        /* Not on a starting tag yet */
        if (*html != '<') {
          /* not strchr: the page may hold NUL bytes */
          const char *adr_next = html;

          while (*adr_next != '<' && (adr_next - r->adr) < r->size) {
            adr_next++;
          }
          /* no tag left: jump close to the end */
          if (*adr_next != '<') {
            if (html - r->adr < r->size - 4 && r->size > 4) {
              html = r->adr + r->size - 2;
            }
          } else {
            html = adr_next;
          }
        }
      }
    }
  }
  ps->html = html;
}

/* Copy what was scanned to the sink, and end the parse once the sink is out
   of memory, failing the page rather than the crawl. */
static void parse_flush(parse_state *const ps, const htsblk *const r) {
  HT_add_adr;
  ps->lastsaved = ps->html; // one past the last byte written
  if (ps->sink.oom) {
    ps->html = r->adr + r->size;
  }
}

/* Time limit, progress and user polling, once per loop pass. Returns
   HTS_FALSE when the user asked to exit. */
static hts_boolean parse_poll(parse_state *const ps, httrackp *const opt,
                              struct_back *const sback, cache_back *const cache,
                              htsmoduleStructExtended *const stre,
                              const htsblk *const r, const int ptr,
                              const int numero_passe) {
  if (ps->page.back_add_stats != opt->state.back_add_stats) {
    ps->page.back_add_stats = opt->state.back_add_stats;

    // Check max time
    if (!back_checkmirror(opt)) {
      ps->html = r->adr + r->size;
    }
  }
  // progress, for the shell
  if (r->size)
    opt->state._hts_in_html_done =
        (int) ((100 * (LLint) (ps->html - r->adr)) / (LLint) r->size);
  if (opt->state._hts_in_html_poll) {
    opt->state._hts_in_html_poll = 0;
    // wait, then fill the backing cache
    back_wait(sback, opt, cache, HTS_STAT.stat_timestart);
    back_fillmax(sback, opt, cache, ptr, numero_passe);

    if (!hts_loop_tick(sback, opt, 0, ptr)) {
      hts_log_print(opt, LOG_ERROR, "Exit requested by shell or user");
      hts_store_release_int(stre->exit_xh_, 1); // exit requested
      return HTS_FALSE;
    } else if (opt->state._hts_cancel == 1) {
      ps->page.nofollow = 1; // gentler than exiting
      opt->state._hts_cancel = 0;
    }
  }
  // refresh the backing system each 2 seconds
  if (engine_stats_tick(&ps->page.stats_calls)) {
    back_wait(sback, opt, cache, HTS_STAT.stat_timestart);
    back_fillmax(sback, opt, cache, ptr, numero_passe);
  }
  return HTS_TRUE;
}

/* Dirty parsing (-%P), the last resort: take any quoted string that looks
   like a link, even outside an attribute. On success sets lk->valid_p and
   lk->module_base, and always updates parseall_lastc. POS_PREV is the script
   automaton state before this byte. */
static PARSE_INLINE void parse_dirty_scan(parse_state *const ps,
                                          parse_link *const lk,
                                          httrackp *const opt,
                                          const htsblk *const r,
                                          const INSCRIPT pos_prev) {
  hts_boolean noparse = HTS_FALSE;

  /* a script byte counts only inside a string */
  if (ps->script.inscript &&
      (ps->script.inscript_state_pos != INSCRIPT_INQUOTE &&
       ps->script.inscript_state_pos != INSCRIPT_INQUOTE2)) {
    noparse = HTS_TRUE;
  }

  /* never in a plain <!-- --> */
  if ((!ps->tag.intag) && (ps->script.incomment) && (!ps->script.inscript))
    noparse = HTS_TRUE;

  if (!noparse) {
    if (!lk->p) {                                         // no link found yet
      if (ps->html != r->adr) {                           // past the first byte
        if ((*ps->html == '\"') || (*ps->html == '\'')) { // "xx.gif" 'xx.gif'
          // for example a="img.gif.. (handles comments)
          if (strchr("=(,", ps->page.parseall_lastc)) {
            const char *a = ps->html;
            char stop = *ps->html; // " or '
            int count = 0;

            a++;
            while ((*a) && (*a != '\'') && (*a != '\"') &&
                   (count < HTS_URLMAXSIZE)) {
              count++;
              a++;
            }

            // a non-empty string closed by its own quote
            if ((*a == stop) && (count < HTS_URLMAXSIZE) && (count > 0)) {
              char c;

              a++;
              while (is_taborspace(*a))
                a++;
              c = *a;
              // in-tag, an attribute value ends at its quote: no
              // delimiter required after it (mid-tag attrs, #201)
              if (strchr("),;>/+\r\n", c) ||
                  (ps->tag.intag && !ps->script.inscript &&
                   ps->tag.intag_start_valid &&
                   hts_dirty_attr_detectable(ps->html, ps->tag.intag_start))) {
                // '/' covers a value followed by a JS comment
                hts_boolean url_ok = hts_dirty_link_is_url(
                    opt, hts_span_at(ps->html + 1, (size_t) count), c,
                    ps->script.inscript);

                if (url_ok) {

                  if (ps->tag.intag && ps->tag.intag_start_valid &&
                      ps->tag.intag_start) {
                    const char *nend;
                    const char *const name = hts_dirty_attr_name(
                        ps->html, ps->tag.intag_start, &nend);
                    const char *refused = NULL;

                    // a quote nested in a value has no name, so
                    // also ask the attribute holding it
                    if (!ps->script.inscript && !ps->script.incomment) {
                      if (name != NULL)
                        refused = hts_dirty_attr_refused(name, nend);
                      if (refused == NULL && ps->tag.intag_startattr != NULL)
                        refused = hts_dirty_attr_refused(
                            ps->tag.intag_startattr,
                            ps->tag.intag_startattr +
                                strcspn(ps->tag.intag_startattr,
                                        "=" HTS_SPACES));
                    }
                    if (refused != NULL) {
                      url_ok = HTS_FALSE;
                      hts_log_print(opt, LOG_DEBUG,
                                    "dirty parsing: %s attribute "
                                    "avoided",
                                    refused);
                    } else if (name != NULL &&
                               hts_attr_is_js_binding(name, nend)) {
                      url_ok = HTS_FALSE;
                      hts_log_print(opt, LOG_DEBUG,
                                    "dirty parsing: binding "
                                    "attribute avoided");
                    }
                  }

                  // accept it, as an ordinary link
                  if (url_ok) {
                    lk->valid_p = 1;
                    lk->module_base =
                        ps->script.inscript &&
                        hts_js_quote_is_import_arg(ps->html, r->adr);
                  }
                }
              }
            }
          }
        }
      }
    } // p == 0

  } // not in comment

  if (ps->script.inscript_state_pos == INSCRIPT_START &&
      pos_prev == INSCRIPT_START) {
    // last byte seen by the parseall scan
    ps->page.parseall_lastc = *ps->html;
  }
}

/* A media link list (in_media): one link per line for LNK, or each
   blank-separated word with a known extension for AAM. Sets lk->valid_p, and
   steps ps->html past the blank for AAM. */
static void detect_media(parse_state *const ps, parse_link *const lk,
                         httrackp *const opt, const htsblk *const r) {
  if (strcmp(ps->page.in_media, "LNK") == 0) { // real media
    // a UTF-8 BOM is kept as is, not read as a link
    const char *const start =
        r->adr +
        (r->size >= 3 && memcmp(r->adr, "\xEF\xBB\xBF", 3) == 0 ? 3 : 0);
    // one link per line, starting at its first non-blank byte
    // *html, not *bol: an indented # line is a comment too
    if (ps->html >= start && !is_realspace(*ps->html) && *ps->html != '#') {
      // starts only on a non-blank byte, so each line is scanned once
      const char *bol = ps->html;

      while (bol > start && (bol[-1] == ' ' || bol[-1] == '\t'))
        bol--;
      if (bol == start || bol[-1] == '\n' || bol[-1] == '\r') {
        lk->valid_p = 1;
      }
    }
  } else if (strcmp(ps->page.in_media, "AAM") == 0) { // AAM
    if (is_space((unsigned char) ps->html[0]) &&
        !is_space((unsigned char) ps->html[1])) {
      const char *a = ps->html + 1;
      int n = 0;
      int ok = 0;
      int dot = 0;

      while (n < HTS_URLMAXSIZE / 2 && a[n] != '\0' &&
             (!is_space((unsigned char) a[n]) || !(ok = 1))) {
        if (a[n] == '.') {
          dot = n;
        }
        n++;
      }
      if (ok && dot > 0) {
        char BIGSTK tmp[HTS_URLMAXSIZE / 2 + 2];

        tmp[0] = '\0';
        strncat(tmp, a + dot + 1, n - dot - 1);
        if (is_knowntype(opt, tmp) || ishtml_ext(hts_span_of(tmp)) != -1) {
          ps->html++;
          lk->valid_p = 1;
          lk->unquoted_script = 1;
        }
      }
    }
  }
}

/* In a script: track an obj.write("...") string, whose content is parsed
   as HTML (inscriptgen). On entry, jumps ps->html to the opening quote,
   sets the automaton inside the string and records its closing quote; the
   mode ends when the cursor reaches that quote. Never touches lk. */
static PARSE_INLINE void detect_js_write(parse_state *const ps,
                                         httrackp *const opt,
                                         const htsblk *const r) {
  const char *const end = r->adr + r->size;

  if (ps->script.inscriptgen) {
    // the cursor has reached the string's closing quote
    if (ps->html >= ps->script.scriptgen_end) {
      ps->script.inscriptgen = 0;
    }
  } else if (ps->script.inscript_state_pos == INSCRIPT_START) {
    const char *a = ps->html;

    // ...object.write[ln]("generated html"), outside strings and comments
    if (!hts_skip_prefix_ci(&a, ".writeln") &&
        !hts_skip_prefix_ci(&a, ".write"))
      return;
    while (is_realspace(*a))
      a++;
    if (*a != '(')
      return;
    a++;
    while (is_realspace(*a))
      a++;
    if (*a != '\'' && *a != '"')
      return;

    const char q = *a;
    const char *const b = a + 1; // the string's first byte

    /* Find the closing quote. A raw line break leaves the string
       unterminated, and a </script> ends an inline script before it. */
    for (a = b; a < end && *a != q && *a != '\n' && *a != '\r'; a++) {
      if (*a == '<' && !ps->script.inscript_locked &&
          hts_starts_with_ci(a, "</script"))
        return;
      if (*a == '\\') {
        if (end - a < 2)
          return;
        a++;                                            // the escaped byte
        if (*a == '\r' && end - a >= 2 && a[1] == '\n') // a CRLF continuation
          a++;
      }
    }
    if (a >= end || *a != q)
      return;
    const char *const close = a;
    a++;
    while (is_realspace(*a))
      a++;
    // accept ("code") only, never ("code"+var)
    if (*a != ')')
      return;

    // HTML generated inside javascript inside HTML: parse it with the
    // standard rules, the automaton inside the string it skipped into.
    ps->script.inscriptgen = 1;
    ps->script.scriptgen_end = close;
    ps->script.inscript_state_pos =
        q == '"' ? INSCRIPT_INQUOTE2 : INSCRIPT_INQUOTE;
    ps->html = b - 1; // the automaton reads b next

    if ((opt->debug > 1) && (opt->log != NULL)) {
      char str[64];

      hts_log_print(opt, LOG_DEBUG,
                    "active code (%s) detected in javascript: %s", "parse",
                    text_excerpt(opt, HTS_BUF(str),
                                 hts_span_at(b, (size_t) (a - b)), 32));
    }
  }
}

/* In a tag, or in HTML generated by a script: find an attribute holding
   a link, an event handler or a robots meta. May move ps->html into a
   handler, swap an applet's codebase= in the page bytes, and set nofollow. */
static PARSE_INLINE void detect_tag_attr(parse_state *const ps,
                                         parse_link *const lk,
                                         httrackp *const opt,
                                         htsmoduleStruct *const str,
                                         const htsblk *const r) {
  if ((*(ps->html - 1) == '<') ||
      (is_space(*(ps->html - 1)))) { // <tag < tag etc
    // <a href=..: an HTML link
    lk->p = rech_tageq(ps->html, "href");
    if (lk->p) { // is it a <base href>?
      if ((ps->tag.intag_start_valid) &&
          check_tag(ps->tag.intag_start, "base")) {
        // base href and codebase do not mix well
        lk->p_type = HTS_LINK_BASE;
      }
    }
    // XML Base, document-wide: later elements inherit it
    if (lk->p == 0 && (lk->p = rech_tageq(ps->html, "xml:base")) != 0) {
      lk->p_type = HTS_LINK_BASE;
      lk->p_xmlbase = HTS_TRUE;
    }

    /* The other attributes that may hold a link */
    const hts_attr *const at =
        lk->p == 0 ? hts_attr_match(ps->html, !ps->script.inscript, &lk->p)
                   : NULL;
    const hts_boolean is_script_attr =
        at != NULL &&
        (at->kind == HTS_ATTR_EVENT || at->kind == HTS_ATTR_STYLE);

    if (at != NULL) {
      switch (at->kind) {
      case HTS_ATTR_LINK:
        lk->dataattr_p = strncmp(at->name, "data-", 5) == 0;
        /* A temporary hack to avoid archive=foo.jar,bar.jar */
        if (strcmp(at->name, "archive") == 0) {
          lk->archivetag_p = 1;
        }
        /* srcset: a comma-list of candidate URLs, each split
           out and rewritten below (#235, #236) */
        else if (strcmp(at->name, "srcset") == 0 ||
                 strcmp(at->name, "data-srcset") == 0) {
          lk->srcset_p = 1;
        }
        break;
      case HTS_ATTR_REFRESH:
        // <meta http-equiv="Refresh" content="3;URL=a.html">
        if (ps->tag.intag_ctype == 2) {
          lk->p_searchMETAURL = 1;
        } else {
          lk->p = 0; /* cancel */
        }
        break;
      case HTS_ATTR_LEAVE:
        /* rewrite the link, never capture it */
        lk->p_nocatch = 1;
        break;
      default:
        break;
      }
    }

    /* Events, but not inside document.write */
    if ((lk->p == 0 || is_script_attr) && !ps->script.inscript) {
      /* not in the table: any other onXxx= */
      if (lk->p == 0) {
        if ((*ps->html == 'o') && (*(ps->html + 1) == 'n') &&
            isUpperLetter(*(ps->html + 2))) {
          while ((lk->p < 64) && hts_isalpha((unsigned char) ps->html[lk->p]))
            lk->p++;
          if (lk->p < 64) {
            while (is_space(ps->html[lk->p]))
              lk->p++;
            if (ps->html[lk->p] == '=')
              lk->p++;
            else
              lk->p = 0;
          } else
            lk->p = 0;
        }
      }
      /* an event handler was found */
      if (lk->p) {
        const char *v = ps->html + lk->p;

        /* the quote that ends it */
        ps->script.inscript_tag_lastc = *v;
        while (v < r->adr + r->size && is_realspace(*v))
          v++;
        if (v < r->adr + r->size && (*v == '"' || *v == '\''))
          ps->script.handler_value_end = handler_end_at(v + 1, *v, r);
        else
          ps->script.handler_value_end = handler_end_at(v, '\0', r);
        ps->html += lk->p /*+ 1*/; /* skip */
        /* now inside JavaScript code */
        ps->script.inscript_name = "";
        ps->script.incss = at != NULL && at->kind == HTS_ATTR_STYLE;
        ps->script.inscript = ps->script.inscript_tag = 1;
        ps->script.inscript_state_pos = INSCRIPT_START;
        if (opt->parsedebug) {
          HT_ADD("<@@ inscript @@>");
        }
      }
      lk->p = 0; /* never start a link here, whatever happened */
    }
    // <applet code=..: a Java applet
    if (lk->p == 0) {
      lk->p = rech_tageq(ps->html, "code");
      if (lk->p) {
        // inside an <applet>
        if ((ps->tag.intag_start_valid) &&
            check_tag(ps->tag.intag_start, "applet")) {
          // the codebase is written before it
          lk->p_type = HTS_LINK_APPLET_CODE;
          lk->add_class = 1; // add .class if missing

          // A codebase= after code= is swapped before it.
          {
            /* '>' must be within this many bytes */
            const size_t window = 1000;
            const char *a;

            a = ps->html;
            while ((size_t) (a - ps->html) < window && *a && *a != '>' &&
                   !rech_tageq(a, "codebase"))
              a++;
            if (rech_tageq(a, "codebase")) {
              const char *const b =
                  tag_close_within(a, window - (a - ps->html));

              if (b != NULL) {
                char BIGSTK tempo[HTS_URLMAXSIZE * 2];
                const size_t offset = ps->html - r->adr;
                char *const modify = &r->adr[offset];
                assertf(modify == ps->html);

                tempo[0] = '\0';
                strncatbuff(tempo, a, b - a);
                strcatbuff(tempo, " ");
                strncatbuff(tempo, ps->html, a - ps->html - 1);
                // pad with spaces to the tag's exact length
                while (strlen(tempo) < (size_t) (b - ps->html))
                  strcatbuff(tempo, " ");
                // no error?
                if (strlen(tempo) == (size_t) (b - ps->html)) {
                  // overwrites the tag in place, so no NUL
                  memcpy(modify, tempo, strlen(tempo));
                  /* the tag changed */
                  ps->page.fetch_tag.name = NULL;
                  lk->p = 0; // the link moved, so drop it
                  lk->p_type = HTS_LINK_PLAIN;
                  lk->add_class = 0;
                }
              }
            }
          }
        }
      }
    }
    // rewritten but not fetched, even when the .class is skipped
    if (lk->p == 0) {
      lk->p = rech_tageq(ps->html, "codebase");
      if (lk->p) {
        // inside an <applet>
        if ((ps->tag.intag_start_valid) &&
            check_tag(ps->tag.intag_start, "applet")) {
          lk->p_type = HTS_LINK_CODEBASE;
        } else
          lk->p = -1; // stop looking
      }
    }

    // robots meta tag
    if (lk->p == 0) {
      if (opt->robots) {
        if ((ps->tag.intag_start_valid) &&
            check_tag(ps->tag.intag_start, "meta")) {
          if (rech_tageq(ps->html, "name") &&
              meta_robots_nofollow(str, ps->tag.intag_start, ps->html))
            // follow no link of this page
            ps->page.nofollow = 1;
        }
      }
    }
  }
}

/* In a script body: leave it on its end tag, or find a link at the
   start of a JS token. */
static PARSE_INLINE void detect_script(parse_state *const ps,
                                       parse_link *const lk,
                                       httrackp *const opt,
                                       const htsblk *const r) {
  assertf(ps->script.inscript_name != NULL);
  if (*ps->html == '/' &&
      ((hts_starts_with_ci(ps->html, "/script") &&
        hts_starts_with_ci(ps->script.inscript_name, "script")) ||
       (hts_starts_with_ci(ps->html, "/style") &&
        hts_starts_with_ci(ps->script.inscript_name, "style"))) &&
      ps->script.inscript_locked == 0) {
    if (ps->html[-1] == '<') { // really a tag?
      ps->script.inscript = 0;
      ps->script.inscriptgen = 0;
      ps->script.inscript_state_pos = INSCRIPT_START;
      if (opt->parsedebug) {
        HT_ADD("<@@ /inscript @@>");
      }
    }
  } else if (ps->script.inscript_state_pos == INSCRIPT_START) {
    /* Script analyzing: foo="url", foo("url"), foo(url),
       foo "url" */
    hts_js_link jslink;

    if (hts_js_scan_link(opt, ps->html, r->adr, ps->script.inscript_tag,
                         ps->script.inscript_tag_lastc, ps->script.incss,
                         &jslink)) {
      if ((opt->debug > 1) && (opt->log != NULL)) {
        char str[64];

        hts_log_print(opt, LOG_DEBUG, "link detected in javascript: %s",
                      text_excerpt(opt, HTS_BUF(str),
                                   hts_span_at(ps->html + jslink.offset,
                                               (size_t) jslink.length),
                                   32));
      }
      // a non-zero p: the string is a link
      lk->p = jslink.offset;
      lk->module_base = jslink.is_module;
      lk->escape_parens = jslink.escape_parens;
      lk->entity_end = jslink.entity_quote;
      if (jslink.unquoted_end != '\0')
        lk->ending_p = jslink.unquoted_end;
    }
  }
}

/* Write the captured link into the output as opt->urlmode asks. Writes
   ps->sink and ps->lastsaved, and updates *ADD_CLASS_DOTS_TO_PATCH_IO.
   Returns EADR, moved past the attribute for a <base>. PTR, PARENT_RELATIVE
   and MODULE_BASE are read by the relativesavename() macro. */
static const char *capture_rewrite(
    parse_state *const ps, const parse_link *const lk, httrackp *const opt,
    const htsblk *const r, const int ptr, const int parent_relative,
    const int module_base, const lien_adrfilsave *const afs,
    const int forbidden_url, const char *const lien, const char *eadr,
    int *const add_class_dots_to_patch_io, const char quote, const int noquote,
    const char *const p_flush, const char *const sf_tag,
    const int sf_tagless_body, const char *const tag_attr_start) {
  int add_class_dots_to_patch = *add_class_dots_to_patch_io;

  if (link_role_is_base(lk->p_type)) { // skip it
    /* An emptied base href leaves <base >, which means "no
       base" and stops a framework that requires one (Angular
       throws). An empty href keeps the document itself as the
       base, so every reference resolves as it did. */
    if (lk->p_type == HTS_LINK_BASE)
      HT_ADD(lk->p_xmlbase ? "xml:base=\"\"" : "href=\"\"");
    // resume at the attribute's end, not at a '#' inside it
    {
      const char *const end = r->adr + r->size;
      const char *a = eadr - 1;

      if (quote != '\0') {
        while (a < end && *a != quote)
          a++;
        if (a < end)
          eadr = a + 1;
        else if (eadr < end)
          eadr = end;
        ps->lastsaved = eadr;
      } else {
        while (a < end && !is_realspace(*a) && *a != '>' &&
               !(noquote && (*a == '"' || *a == '\'')))
          a++;
        // leave the terminator to the main loop
        if (a != eadr - 1)
          eadr = a;
        ps->lastsaved = a;
      }
    }
  }
  /* */
  else if (opt->urlmode == HTS_URLMODE_ABSOLUTE) {
    // -K strips a scheme word from an adr with no "//"
    sink_add_absolute_url(&ps->sink, afs->af.adr, afs->af.fil, opt->passprivacy,
                          HTS_FALSE);
    // the caller's ++ skips the quote
    ps->lastsaved = eadr - 1;
    /* */
  } else if (opt->urlmode == HTS_URLMODE_KEEP_ORIGINAL) {
    /* */
    /* leave the link 'as is' */
    /* otherwise it depends on internal or external */
  } else if (forbidden_url == 1) { // not fetched: an external link
    /* Not only output: also writes external.html or .gif. */
    if (ps->sink.enabled) {
      if (lk->p_type != HTS_LINK_APPLET_CODE) { // not a class
        if (!opt->external) {
          sink_add_absolute_url(&ps->sink, afs->af.adr, afs->af.fil,
                                opt->passprivacy, HTS_TRUE);
        } else { // external page: link to an error page
          const char *cat_name = NULL;
          const char *cat_data = NULL;
          int cat_nb = 0;
          int cat_data_len = 0;
          switch ((link_has_authority(afs->af.adr))
                      ? 1
                      : ((hts_lastchar(afs->af.fil) == '/')
                             ? 1
                             : (ishtml(opt, afs->af.fil)))) {
          case 1:
          case -2: // html or directory
            if (opt->getmode & HTS_GETMODE_HTML) {
              cat_name = "external.html";
              cat_nb = 0;
              cat_data = HTS_DATA_UNKNOWN_HTML;
              cat_data_len = HTS_DATA_UNKNOWN_HTML_LEN;
            }
            break;
          default: { // anything else: asp, cgi, images..
            char BIGSTK mime[HTS_MIMETYPE_SIZE];

            // A name the mime table calls an image gets a
            // GIF, not a broken <img>.
            if (get_httptype_sized(opt, mime, sizeof(mime), afs->af.fil, 0) &&
                hts_starts_with_ci(mime, "image/")) {
              cat_name = "external.gif";
              cat_nb = 1;
              cat_data = HTS_DATA_UNKNOWN_GIF;
              cat_data_len = HTS_DATA_UNKNOWN_GIF_LEN;
            } else {
              cat_name = "external.html";
              cat_nb = 0;
              cat_data = HTS_DATA_UNKNOWN_HTML;
              cat_data_len = HTS_DATA_UNKNOWN_HTML_LEN;
            }
            break;
          }
          } // html,gif

          char BIGSTK save[HTS_URLMAXSIZE * 2];
          char BIGSTK tempo[HTS_URLMAXSIZE * 2];

          strcpybuff(save, StringBuff(opt->path_html_utf8));
          strcatbuff(save, cat_name);
          if (lienrelatif(HTS_BUF(tempo), save, relativesavename()) == 0) {
            /* Never escape high-chars (we don't know the
             * encoding!!) */
            inplace_escape_uri_utf(HTS_SIZED(tempo)); // escape with %xx
            HT_ADD_HTMLESCAPED(tempo);
            HT_ADD("?link=");

            sink_add_absolute_url(&ps->sink, afs->af.adr, afs->af.fil,
                                  opt->passprivacy, HTS_TRUE);
          }
          // write the placeholder file?
          if (verif_external(opt, cat_nb)) {
            FILE *fp = filecreate(
                &opt->state.strc,
                hts_optpath(opt, StringBuff(opt->path_html_utf8), cat_name));
            if (fp) {
              if (cat_data_len == 0) { // text
                verif_backblue(opt, StringBuff(opt->path_html_utf8));
                fprintf(fp, "%s%s",
                        "<!-- Created by HTTrack Website "
                        "Copier/" HTTRACK_VERSION " " HTTRACK_AFF_AUTHORS
                        " -->" LF,
                        cat_data);
              } else { // data
                (void) hts_fwrite_exact(cat_data, (size_t) cat_data_len, fp);
              }
              fclose(fp);
              usercommand(
                  opt, 0, NULL,
                  hts_optpath(opt, StringBuff(opt->path_html_utf8), cat_name),
                  "", "");
            }
          }
        } // external
      } else { // a bare java class name
        // as below: copy codebase and base
        if (p_flush) {
          char BIGSTK tempo[HTS_URLMAXSIZE * 2];
          char BIGSTK tempo_pat[HTS_URLMAXSIZE * 2];

          // compute the path
          tempo_pat[0] = '\0';
          strcpybuff(tempo, afs->af.fil);
          {
            char *a = strrchr(tempo, '/');

            // Example: we converted code="x.y.z.foo.class" into
            // "x/y/z/foo.class" we have to do the contrary now
            if (add_class_dots_to_patch > 0) {
              while ((add_class_dots_to_patch > 0) && (a)) {
                *a = '.'; // convert "false" java / into .
                add_class_dots_to_patch--;
                a = strrchr(tempo, '/');
              }
              // if add_class_dots_to_patch, this is because there is a
              // problem!!
              if (add_class_dots_to_patch) {
                hts_log_print(
                    opt, LOG_WARNING,
                    "Error: can not rewind java path %s, check html code",
                    tempo);
              }
            }
            // Cut path/filename
            if (a) {
              char BIGSTK tempo2[HTS_URLMAXSIZE * 2];

              strcpybuff(tempo2, a + 1);                      // file
              strncatbuff(tempo_pat, tempo, (a - tempo) + 1); // path
              strcpybuff(tempo, tempo2);                      // file
            }
          }

          // write codebase="path"
          if (strnotempty(tempo_pat)) {
            HT_ADD("codebase=\"");
            sink_add_absolute_url(&ps->sink, afs->af.adr, tempo_pat,
                                  opt->passprivacy, HTS_TRUE);
            HT_ADD("\" ");
          }

          /* The page picks this span's width, so it
             goes to the growable output. */
          if (p_flush > ps->lastsaved) {
            HT_ADD_BUF(ps->lastsaved, (size_t) (p_flush - ps->lastsaved));
          }
          HT_ADD_URL_HTMLESCAPED(tempo);
        }
      }
    }
    ps->lastsaved = eadr - 1;
  } else if (opt->mimehtml) {
    char BIGSTK cid[HTS_URLMAXSIZE * 3];

    HT_ADD("cid:");
    make_content_id(afs->af.adr, afs->af.fil, HTS_BUF(cid));
    HT_ADD_HTMLESCAPED(cid);
    // last written + 1, before the ++ that follows
    ps->lastsaved = eadr - 1;
  } else if (opt->urlmode == HTS_URLMODE_ABSOLUTE_URI) {
    HT_ADD_URL_HTMLESCAPED(afs->af.fil);
    ps->lastsaved = eadr - 1; // resume copying at the closing quote
  } else if (opt->urlmode == HTS_URLMODE_TRANSPARENT_PROXY) {
    char BIGSTK tempo[HTS_URLMAXSIZE * 2];
    const char *uri;
    int i;
    char *pos;

    /* Not only output: also fills fixed buffers below. */
    if (ps->sink.enabled) {
      if (!link_has_authority(afs->af.adr)) {
        HT_ADD("http://");
      } else {
        char *aut = strstr(afs->af.adr, "//");

        if (aut) {
          char tmp[256];

          tmp[0] = '\0';
          strncatbuff(tmp, afs->af.adr, (aut - afs->af.adr)); // scheme
          HT_ADD(tmp);                                        // Protocol
          HT_ADD("//");
        }
      }

      // filename is taken as URI (ex: "C:\My
      // Website\www.example.com\foo4242.html)
      uri = afs->save;

      // .. after stripping the path prefix (ex: "www.example.com\foo4242.html)
      if (strnotempty(StringBuff(opt->path_html_utf8))) {
        uri += StringLength(opt->path_html_utf8);
        for (; uri[0] == '/' || uri[0] == '\\'; uri++)
          ;
      }
      // and replacing all \ by / (ex: "www.example.com/foo4242.html)
      strcpybuff(tempo, uri);
      for (i = 0; tempo[i] != '\0'; i++) {
        if (tempo[i] == '\\') {
          tempo[i] = '/';
        }
      }

      // put original query string if any (ex:
      // "www.example.com/foo4242.html?q=45)
      pos = strchr(afs->af.fil, '?');
      if (pos != NULL) {
        strcatbuff(tempo, pos);
      }
      // write it
      HT_ADD_URL_HTMLESCAPED(tempo);
    }
    // last written + 1, before the ++ that follows
    ps->lastsaved = eadr - 1;
  } else if (opt->urlmode == HTS_URLMODE_RELATIVE) {
    char BIGSTK tempo[HTS_URLMAXSIZE * 2];

    tempo[0] = '\0';
    // compute the relative link

    if (lienrelatif(HTS_BUF(tempo), afs->save, relativesavename()) == 0) {
      if (!ps->page.in_media) { // In media (such as real
                                // audio): don't patch
        /* Never escape high-chars (we don't know the
         * encoding!!) */
        inplace_escape_uri_utf(HTS_SIZED(tempo));

        // CSS url(): keep parens escaped
        if (lk->escape_parens)
          escape_url_parens(HTS_BUF(tempo));
      }
      hts_log_print(
          opt, LOG_DEBUG, "relative link at %s build with %s and %s: %s",
          adr_nologin(opt, afs->af.adr), afs->save, relativesavename(), tempo);

      // an applet code link needs a codebase first
      if (lk->p_type == HTS_LINK_APPLET_CODE) {

        if (p_flush) {
          char BIGSTK tempo_pat[HTS_URLMAXSIZE * 2];

          tempo_pat[0] = '\0';
          {
            char *a = strrchr(tempo, '/');

            // Example: we converted code="x.y.z.foo.class" into
            // "x/y/z/foo.class" we have to do the contrary now
            if (add_class_dots_to_patch > 0) {
              while ((add_class_dots_to_patch > 0) && (a)) {
                *a = '.'; // convert "false" java / into .
                add_class_dots_to_patch--;
                a = strrchr(tempo, '/');
              }
              // if add_class_dots_to_patch, this is because there is a
              // problem!!
              if (add_class_dots_to_patch) {
                hts_log_print(
                    opt, LOG_WARNING,
                    "Error: can not rewind java path %s, check html code",
                    tempo);
              }
            }

            if (a) {
              char BIGSTK tempo2[HTS_URLMAXSIZE * 2];

              strcpybuff(tempo2, a + 1);
              strncatbuff(tempo_pat, tempo, a - tempo + 1); // path
              strcpybuff(tempo, tempo2);                    // file
            }
          }

          // write codebase="path"
          if (strnotempty(tempo_pat)) {
            HT_ADD("codebase=\"");
            HT_ADD_HTMLESCAPED(tempo_pat);
            HT_ADD("\" ");
          }

          /* The page picks this span's width, so it
             goes to the growable output. */
          if (p_flush > ps->lastsaved) {
            HT_ADD_BUF(ps->lastsaved, (size_t) (p_flush - ps->lastsaved));
          }
        }
      }

      /* Not only output: also tags --single-file links. */
      if (ps->sink.enabled) {

        /* --single-file: tag the reference for the
           end-of-mirror pass. A fragment, so a mirror left
           marked by an interrupted run still browses. */
        /* A NULL tag reads as "a CSS or JS body", so a tag
           we cannot name must not reach the classifier. */
        const char sf_class =
            (opt->single_file && ps->page.sf_may_mark && !ps->page.in_media &&
             lk->p_type == HTS_LINK_PLAIN && !lk->p_searchMETAURL &&
             (sf_tag != NULL || sf_tagless_body))
                ? singlefile_ref_class(sf_tag, tag_attr_start, r->adr)
                : 0;
        const size_t sf_start = TypedArraySize(ps->sink.buf);

        // write the rewritten relative link
        // Note: escape all chars, even >127 (no UTF)
        HT_ADD_HTMLESCAPED_FULL(tempo);

        // Add query-string, for informational purpose only
        // Useless, because all parameters-pages are saved into different
        // targets
        if (opt->includequery) {
          char *a = strchr(lien, '?');

          if (a) {
            HT_ADD_HTMLESCAPED(a);
          }
        }

        /* Measure the appended bytes, because escaping
           can change the length. The query sits inside the
           span so inlining drops it. */
        if (sf_class != 0) {
          char sf_mark[SINGLEFILE_MARK_MAX];

          /* SINGLEFILE_MAX_SPAN is derived from these two
             buffers, so a resize must move it. */
          HTS_STATIC_ASSERT(sizeof(tempo) * HTS_HTMLESCAPE_FULL_MAXEXP +
                                    (HTS_URLMAXSIZE * 2) *
                                        HTS_HTMLESCAPE_MAXEXP <=
                                SINGLEFILE_MAX_SPAN,
                            sf_span_fits);

          HT_ADD(singlefile_mark(opt, HTS_BUF(sf_mark), sf_class,
                                 TypedArraySize(ps->sink.buf) - sf_start));
        }
      }
      // last written + 1, before the ++ that follows
      ps->lastsaved = eadr - 1;
    } else {
      hts_log_print(opt, LOG_WARNING, "Error building relative link %s and %s",
                    afs->save, relativesavename());
    }
  }
  *add_class_dots_to_patch_io = add_class_dots_to_patch;
  return eadr;
}

/* Add the captured link to the crawl's link table, robots.txt first when
   the host needs one. Blanks afs->af.adr when the link is too long, and
   stops the crawl when the table is full. Returns HTS_FALSE when recording
   fails, and the parse must then end. */
static hts_boolean
capture_record(httrackp *const opt, htsmoduleStructExtended *const stre,
               hash_struct *const hash, const int numero_passe, const int ptr,
               const char *const codebase, const parse_link *const lk,
               lien_adrfilsave *const afs, const lien_adrfil *const former,
               const hts_fetch_meta *const link_fetch, const int forbidden_url,
               const int just_test_it, const int set_prio_to,
               const int import_done, const int meme_adresse) {
  /* Security check */
  if (strlen(afs->save) >= HTS_URLMAXSIZE) {
    afs->af.adr[0] = '\0';
    hts_log_print(opt, LOG_WARNING, "Link is too long: %s", afs->save);
  }

  if ((afs->af.adr[0] != '\0') && !link_role_is_base(lk->p_type) &&
      (forbidden_url != 1)) { // if the file does not exist, add it to the list
    /* A front end's hts_addurl() can fill the cap after the
       gate above. Skip to keep room for the records below. */
    if (hts_maxlinks_no_room(opt)) {
      maxlinks_give_up(opt, stre->exit_xh_);
    } else { // room for it: record the link
      int pass_fix, dejafait = 0;

      // this link's priority
      if ((opt->getmode & HTS_GETMODE_HTML_FIRST) == 0) {
        pass_fix = 0;
      } else { // html first: non-html waits
        if (!ishtml(opt, afs->af.fil))
          pass_fix = 1; // lower priority, handled later
        else
          pass_fix = hts_max_int(0, numero_passe); // normal priority
      }

      /* If the file seems to be an html file, get depth-1 */

      // already recorded? keep the larger of the two depths
      {
        int i = hash_read(hash, afs->save, NULL, 0); // look up by save name

        if (i >= 0) {
          if ((opt->debug > 1) && (opt->log != NULL)) {
            if (strcmp(afs->af.adr, heap(i)->adr) != 0 ||
                strcmp(afs->af.fil, heap(i)->fil) != 0) {
              hts_log_print(opt, LOG_DEBUG,
                            "merging similar links %s%s and %s%s",
                            adr_nologin(opt, afs->af.adr), afs->af.fil,
                            adr_nologin(opt, heap(i)->adr), heap(i)->fil);
            }
          }
          heap(i)->depth = hts_max_int(heap(i)->depth, heap(ptr)->depth - 1);
          dejafait = 1;
        }
      }

      // never recorded: create it
      if (!dejafait) {

        // record the host's robots.txt first
        if (!just_test_it) {
          if (!hts_starts_with_ci(afs->af.adr, "ftp://") &&
              !hts_starts_with_ci(afs->af.adr, "file://")) {
            if (opt->robots) {
              if (ishtml(opt, afs->af.fil) != 0) { // not for a lone file
                if (checkrobots(_ROBOTS, afs->af.adr, "") !=
                    -1) { // robots.txt ?
                  robots_claim(opt, _ROBOTS, afs->af.adr);
                  if (checkrobots(_ROBOTS, afs->af.adr, "") ==
                      -1) { // robots.txt ?
                    if (!hts_record_link(opt, afs->af.adr, "/robots.txt", "",
                                         "", "", NULL)) {
                      return HTS_FALSE;
                    }
                    heap_top()->testmode = 0;    // not test mode
                    heap_top()->link_import = 0; // not import mode
                    heap_top()->premier = heap_top_index();
                    heap_top()->precedent = ptr;
                    heap_top()->depth = 0;
                    heap_top()->pass2 = hts_max_int(0, numero_passe);
                    heap_top()->retry = 0;
                    hts_log_print(opt, LOG_DEBUG, "robots.txt added at %s",
                                  adr_nologin(opt, afs->af.adr));
                  } else {
                    hts_log_print(opt, LOG_ERROR,
                                  "Unexpected robots.txt error at %d",
                                  __LINE__);
                  }
                }
              }
            }
          }
        }

        // record the link
        if (!hts_record_link(opt, afs->af.adr, afs->af.fil, afs->save,
                             former->adr, former->fil, codebase)) {
          return HTS_FALSE;
        }
        heap_top()->fetch = *link_fetch;
        if (!just_test_it)
          heap_top()->testmode = 0; // not test mode
        else
          heap_top()->testmode = 1; // test mode
        if (!import_done)
          heap_top()->link_import = 0; // not import mode
        else
          heap_top()->link_import = 1; // import mode
        if ((meme_adresse) && (!import_done) && (heap(ptr)->premier != 0))
          heap_top()->premier = heap(ptr)->premier;
        else // else the new link starts its own chain
          heap_top()->premier = heap_top_index();

        heap_top()->precedent = ptr;
        if (!set_prio_to)
          heap_top()->depth = heap(ptr)->depth - 1;
        else
          /* lowest priority (catch page) */
          heap_top()->depth = hts_max_int(
              0, hts_min_int(heap(ptr)->depth - 1, set_prio_to - 1));
        heap_top()->pass2 = pass_fix;
        heap_top()->retry = opt->retry;

        if (!just_test_it) {
          hts_log_print(opt, LOG_DEBUG, "OK, NOTE: %s%s -> %s",
                        adr_nologin(opt, heap_top()->adr), heap_top()->fil,
                        heap_top()->sav);
        } else {
          hts_log_print(opt, LOG_DEBUG, "OK, TEST: %s%s",
                        adr_nologin(opt, heap_top()->adr), heap_top()->fil);
        }

      } else { // if !dejafait
        hts_log_print(opt, LOG_DEBUG,
                      "link has already been recorded, cancelled: %s",
                      afs->save);
      }

    } // room for it
  } // if adr[0]!='\0'
  return HTS_TRUE;
}

/* What capture_link() asks its caller to do next. */
typedef enum {
  CAPTURE_LINK_DONE, // link handled or skipped
  CAPTURE_LINK_NEXT, // another srcset candidate follows
  CAPTURE_LINK_FAIL  // capture_record() failed, so abort the parse
} capture_link_status;

/* Cut, resolve, rewrite and record the link at ps->html, given the scan
   state OK. Moves ps->html through SAUT, writes ps->sink, ps->lastsaved
   and ps->page, and updates *LINK_FETCH, *ADD_CLASS_DOTS_TO_PATCH and
   *P_FLUSH. Leaves the tag when the cut reaches its '>', and sets
   *stre->error_ when a callback refuses the link. OPT, PTR, PARENT_RELATIVE and
   MODULE_BASE keep their names because the heap() and relativeurladr() macros
   read them. */
static capture_link_status
capture_link(parse_state *const ps, const parse_link *const lk,
             httrackp *const opt, htsmoduleStruct *const str,
             htsmoduleStructExtended *const stre, htsblk *const r,
             struct_back *const sback, cache_back *const cache,
             hash_struct *const hash, const int numero_passe, const int ptr,
             const int parent_relative, const int module_base,
             const char *const codebase, const hts_buf catbuff,
             const script_automate *const saut, int ok, const char quote,
             const int quoteinscript, const int noquote,
             const char *const tag_attr_start, const char *const sf_tag,
             const int sf_tagless_body, hts_fetch_meta *const link_fetch,
             int *const add_class_dots_to_patch, const char **const p_flush) {
  const char *eadr = NULL; /* end of the URL */
  hts_boolean not_url = HTS_FALSE;
  hts_boolean gen_cut = HTS_FALSE; /* stopped at a write string's end */

  /* srcset: skip leading whitespace/commas before each candidate;
     the skipped bytes flush verbatim below */
  if (lk->srcset_p) {
    while (ps->html < r->adr + r->size &&
           (is_realspace(*ps->html) || *ps->html == ','))
      hts_automate_increment(saut, 1);
  }
  eadr = ps->html;

  // ne pas flusher après code si on doit écrire le codebase avant!
  if (lk->p_type != HTS_LINK_APPLET_CODE && !link_role_is_base(lk->p_type)) {
    HT_add_adr;               // refresh
    ps->lastsaved = ps->html; // one past the last byte written
    // après on écrira soit les données initiales,
    // soir une URL/lien modifié!
  } else if (lk->p_type == HTS_LINK_APPLET_CODE)
    *p_flush = ps->html; // flush up to here afterwards

  if (ok != -1) { // continuer
    // découper le lien
    do {
      if (*eadr == '\0') // end of the parsed buffer
        ok = 0;
      // too long: HTS_URLMAXSIZE more is kept for the path
      if (eadr - ps->html > HTS_URLMAXSIZE)
        ok = -1; // ne pas traiter ce lien

      /* A written value ends with its string. A quote the string never
         closes is a value built by later code: leave it as is. */
      if (ok > 0 && ps->script.inscriptgen &&
          eadr >= ps->script.scriptgen_end) {
        ok = quote != '\0' ? -1 : 0;
        gen_cut = HTS_TRUE;
      } else if (ok > 0) {
        if (quote == '\n' && *eadr == '\r' && ps->page.in_media != NULL &&
            strcmp(ps->page.in_media, "LNK") == 0)
          ok = 0;                   // a playlist line may end in CR
        else if (is_space(*eadr)) { // guillemets,CR, etc
          if ((*eadr == quote &&
               (!quoteinscript || *(eadr - 1) == '\\')) // end quote
              ||
              (noquote && (*eadr == '\"' || *eadr == '\'')) // end at any quote
              || ((!noquote || ps->script.inscriptgen) && quote == '\0' &&
                  is_realspace(*eadr)) // unquoted href
              || lk->srcset_p          // whitespace ends a srcset candidate URL
              ) // si pas d'attente de quote spéciale ou si quote
                // atteinte
            ok = 0;
        } else if (lk->ending_p && (*eadr == lk->ending_p))
          ok = 0;
        else if (lk->entity_end != '\0' && *eadr == '&' &&
                 html_entity_quote(eadr, NULL) == lk->entity_end)
          ok = 0;
        else {
          switch (*eadr) {
          case '>':
            if (!quote) {
              // a '>' inside another quoted value leaves the tag open
              if (!ps->script.inscript && !ps->page.in_media &&
                  (ps->tag.quoted_value_end == NULL ||
                   eadr >= ps->tag.quoted_value_end)) {
                ps->tag.intag = 0; // no longer in a tag
                ps->tag.intag_start_valid = 0;
                ps->tag.intag_name = NULL;
              }
              ok = 0;
            }
            break;
            /*case '<': */
          case '#':
            /* a playlist link can start the buffer (#EXTM3U) */
            if (eadr == r->adr || *(eadr - 1) != '&') // &#40;
              ok = 0;
            break;
            // case '?': non!
          case '\\':
            if (ps->script.inscript)
              ok = 0;
            break;  // \" ou \' point d'arrêt
          case '?': /*quote_adr=adr; */
            break;  // noter position query
          case ',':
            if (lk->srcset_p) {
              /* split only on a trailing comma; one inside the URL
                 (data: URI, CDN path) is kept, per the WHATWG algo */
              const char *const n = eadr + 1;

              if (n >= r->adr + r->size || is_space(*n) || *n == ',')
                ok = 0;
            }
            break;
          }
        }
      }
      eadr++;
    } while (ok == 1);

    // Empty link detected
    if (eadr - ps->html <= 1) { // link empty
      ok = -1;                  // No
      // "#" would land before an unflushed base's name
      if (*ps->html != '#' && quote && !gen_cut &&
          !link_role_is_base(lk->p_type)) {
        HT_ADD("#"); // We add this for a <href="">
      }
    }
    // This is a dirty and horrible hack to avoid parsing an Adobe GoLive bogus
    // tag
    if (hts_starts_with_ci(ps->html, "(Empty Reference!)")) {
      ok = -1; // No
    }
    // a lazy-load placeholder or template is no URL: leave it as is
    const hts_span value =
        hts_span_at(ps->html, (size_t) (eadr - ps->html - 1));

    if (ok == 0 && lk->dataattr_p && data_attr_is_not_url(value)) {
      char *shown = malloct(value.len + 8);

      if (shown != NULL)
        hts_log_print(opt, LOG_DEBUG, "data attribute is no URL: %s",
                      text_excerpt(opt, hts_buf_at(shown, value.len + 8), value,
                                   value.len + 7));
      freet(shown);
      not_url = HTS_TRUE;
    }
  }

  if (ok == 0 && !not_url) { // test a link
    char BIGSTK lien[HTS_URLMAXSIZE * 2];
    int meme_adresse = 0; // 0 par défaut pour primary

    // construire lien (découpage)
    if (eadr - ps->html - 1 < HTS_URLMAXSIZE) { // not too long?
      strncpy(lien, ps->html, eadr - ps->html - 1);
      lien[eadr - ps->html - 1] = '\0';
      while (hts_striplastchar(lien, ' ')) {
      }

    } else
      lien[0] = '\0'; // erreur

    // ------------------------------------------------------
    // Lien repéré et extrait
    if (strnotempty(lien) > 0) { // construction du lien
      lien_adrfilsave afs;
      int forbidden_url = -1; // lien non interdit (mais non autorisé..)
      int just_test_it = 0;   // mode de test des liens
      int set_prio_to = 0;    // pour capture de page isolée
      int import_done = 0; // lien importé (ne pas scanner ensuite *à priori*)

      //
      afs.af.adr[0] = '\0';
      afs.af.fil[0] = '\0';
      afs.save[0] = '\0';
      //
      // 0: autorisé
      // 1: interdit (patcher tout de même adresse)

      hts_log_print(opt, LOG_DEBUG, "link detected in html (tag): %s",
                    url_nologin(opt, lien));

      // external check
      if (!RUN_CALLBACK1(opt, linkdetected, lien) ||
          !RUN_CALLBACK2(opt, linkdetected2, lien, ps->tag.intag_start)) {
        *stre->error_ = 1;
        hts_log_print(opt, LOG_ERROR, "Link %s refused by external wrapper",
                      url_nologin(opt, lien));
      }
      // purger espaces de début et fin, CR,LF résiduels
      // (IMG SRC="foo.<\n><\t>gif<\t>")
      {
        char *a = lien;
        size_t llen;

        // strip both ends of every C0 control or space, as a browser
        // does; encoding one there would 404 a link that fetched fine
        llen = (*a != '\0') ? strlen(a) : 0;
        while (llen > 0 && (unsigned char) lien[llen - 1] <= ' ') {
          a[--llen] = '\0';
        }
        // '\0' is <= ' ' too, and an all-control link ends up empty
        while (*a != '\0' && (unsigned char) *a <= ' ')
          a++;
        // strip cr, lf, tab inside URL, as a browser does; every
        // other control byte percent-encodes below
        llen = 0;
        while (*a) {
          if (!is_retorsep(*a)) {
            lien[llen++] = *a;
          }
          a++;
        }
        lien[llen] = '\0';
      }

      // commas are forbidden
      if (lk->archivetag_p) {
        if (strchr(lien, ',')) {
          *stre->error_ = 1;
          hts_log_print(opt, LOG_DEBUG, "link rejected (multiple-archive) %s",
                        url_nologin(opt, lien));
        }
      }

      decode_link_escapes(str, stre, HTS_BUF(lien), catbuff);

      // convertir les éventuels \ en des / pour éviter des problèmes de
      // reconnaissance!
      {
        char *a;

        for (a = jump_identification(lien); *a != '\0' && *a != '?'; a++) {
          if (*a == '\\') {
            *a = '/';
          }
        }
      }

      // supprimer le(s) ./
      while ((lien[0] == '.') && (lien[1] == '/')) {
        char BIGSTK tempo[HTS_URLMAXSIZE * 2];

        strcpybuff(tempo, lien + /* ./ */ 2);
        strcpybuff(lien, tempo);
      }
      if (strnotempty(lien) == 0) // sauf si plus de nom de fichier
        strcpybuff(lien, "./");

      // APPLET CODE="mixer.MixerApplet.class" --> APPLET
      // CODE="mixer/MixerApplet.class" yes, this is dirty but I'm so lazzy..
      // and besides the java "code" convention is really a pain in html code
      if (lk->p_type == HTS_LINK_APPLET_CODE) {
        char *a = strrchr(lien, '.');

        *add_class_dots_to_patch = 0;
        if (a) {
          char *b;

          do {
            b = strchr(lien, '.');
            if ((b != a) && (b)) {
              (*add_class_dots_to_patch)++;
              *b = '/';
            }
          } while ((b != a) && (b));
        }
      }
      // drop a default :80 port from the authority
      hts_strip_default_port(HTS_BUF(lien));
      // vérifier que l'on ne doit pas ajouter de .class
      if (!*stre->error_) {
        if (lk->add_class) {
          const char *const a = hts_path_ext_dot(lien, "/");

          if (a == NULL || !hts_streq_ci(a, ".class"))
            strcatbuff(lien, ".class");
        }
      }
      if (!*stre->error_) {
        resolve_link_base(str, stre, HTS_BUF(lien), lk->p_type, lk->p_xmlbase);
      }
      // transformer lien quelconque (http, relatif, etc) en une adresse
      // et un chemin+fichier (adr,fil)
      if (!*stre->error_) {
        int reponse;

        hts_log_print(opt, LOG_DEBUG, "build relative link %s with %s%s",
                      url_nologin(opt, lien),
                      adr_nologin(opt, relativeurladr()), relativeurlfil());
        if ((reponse = ident_url_relatif(lien, relativeurladr(),
                                         relativeurlfil(), &afs.af)) < 0) {
          afs.af.adr[0] = '\0'; // erreur
          if (reponse == -2) {
            hts_log_print(opt, LOG_WARNING,
                          "Link %s not caught (protocol not "
                          "supported by this build)",
                          url_nologin(opt, lien));
          } else {
            hts_log_print(opt, LOG_DEBUG,
                          "ident_url_relatif failed for %s with %s%s",
                          url_nologin(opt, lien),
                          adr_nologin(opt, relativeurladr()), relativeurlfil());
          }
        } else {
          hts_log_print(
              opt, LOG_DEBUG, "built relative link %s with %s%s -> %s%s",
              url_nologin(opt, lien), adr_nologin(opt, relativeurladr()),
              relativeurlfil(), adr_nologin(opt, afs.af.adr), afs.af.fil);
        }
      } else {
        hts_log_print(opt, LOG_DEBUG,
                      "link %s not build, error detected before",
                      url_nologin(opt, lien));
        afs.af.adr[0] = '\0';
      }

      // Le lien doit juste être réécrit, mais ne doit pas générer un lien
      // exemple: <FORM ACTION="url_cgi">
      if (lk->p_nocatch) {
        forbidden_url = 1; // interdire récupération du lien
        hts_log_print(opt, LOG_DEBUG, "link forced external at %s%s",
                      adr_nologin(opt, afs.af.adr), afs.af.fil);
      }
      // Tester si un lien doit être accepté ou refusé (wizard)
      // forbidden_url=1 : lien refusé
      // forbidden_url=0 : lien accepté
      if (!link_role_is_base(lk->p_type)) { // check the filters
        if (!lk->p_nocatch) {
          if (afs.af.adr[0] != '\0') {
            hts_log_print(opt, LOG_DEBUG, "wizard link test at %s%s..",
                          adr_nologin(opt, afs.af.adr), afs.af.fil);
            forbidden_url = hts_acceptlink(
                opt, ptr, afs.af.adr, afs.af.fil, ps->tag.intag_name,
                ps->tag.intag_name ? tag_attr_start : NULL, ps->script.css_file,
                &set_prio_to, &just_test_it);
            hts_log_print(opt, LOG_DEBUG, "result for wizard link test: %d",
                          forbidden_url);
          }
        }
      }
      // calculer meme_adresse
      {
        const char *const host = jump_identification_const(afs.af.adr);

        // two empty hosts are not the same address
        meme_adresse = *host != '\0' &&
                       hts_streq_ci(host, jump_identification_const(urladr()));
      }

      // Début partie sauvegarde

      // ici on forme le nom du fichier à sauver, et on patche l'URL
      if (afs.af.adr[0] != '\0') {
        // savename(): simplifier les ../ et autres joyeusetés
        int r_sv = 0;

        // En cas de moved, adresse première
        lien_adrfil former;

        //
        afs.save[0] = '\0';
        former.adr[0] = '\0';
        former.fil[0] = '\0';
        //

        // nom du chemin à sauver si on doit le calculer
        // note: url_savename peut décider de tester le lien si il le trouve
        // suspect, et modifier alors adr et fil
        // dans ce cas on aura une référence directe au lieu des traditionnels
        // moved en cascade (impossible à reproduire à priori en local, lorsque
        // des fichiers gif sont impliqués par exemple)
        if (!link_role_is_base(lk->p_type)) {
          if (forbidden_url != 1) {
            char BIGSTK last_adr[HTS_URLMAXSIZE * 2];

            /* Calc */
            last_adr[0] = '\0';
            strcpybuff(last_adr, afs.af.adr); // ancienne adresse
            r_sv =
                url_savename(&afs, &former, heap(ptr)->adr, heap(ptr)->fil, opt,
                             sback, cache, hash, ptr, numero_passe, NULL);
            if (strcmp(jump_identification_const(last_adr),
                       jump_identification_const(afs.af.adr)) !=
                0) { // the address moved, so retest it against the filters
              if (ptr > 0 && afs.af.adr[0] != '\0') { // check the filters
                hts_log_print(opt, LOG_DEBUG,
                              "wizard moved link retest at %s%s..",
                              adr_nologin(opt, afs.af.adr), afs.af.fil);
                forbidden_url = hts_acceptlink(
                    opt, ptr, afs.af.adr, afs.af.fil, ps->tag.intag_name,
                    ps->tag.intag_name ? tag_attr_start : NULL,
                    ps->script.css_file, &set_prio_to, &just_test_it);
                hts_log_print(opt, LOG_DEBUG,
                              "result for wizard moved link retest: %d",
                              forbidden_url);
              }
              meme_adresse = 0; // on a changé
            }
          } else {
            strcpybuff(afs.save, ""); // dummy
          }
        }
        /* A <base target> sends every link with no target to a
           frame we cannot tell. */
        if (ps->page.link_frame < 0)
          ps->page.link_frame =
              hts_fetch_has_base_target(r->adr)
                  ? HTS_FETCH_DEST_UNKNOWN
                  : hts_fetch_same_frame(heap(ptr)->fetch.dest);
        {
          const parse_fetch_flags flags = {
              .redirect_stub = HTTP_IS_REDIRECT(r->statuscode),
              .meta_refresh = lk->p_searchMETAURL,
              .opaque =
                  ps->page.in_media != NULL || lk->p_type != HTS_LINK_PLAIN,
              .inscript = ps->script.inscript,
              .incss = ps->script.incss,
              .tagless_body = sf_tagless_body};

          parse_fetch_meta(opt, ptr, link_fetch, &flags, ps->page.link_frame,
                           &ps->page.fetch_tag, sf_tag, tag_attr_start,
                           afs.af.fil);
        }
        /* Naming followed redirects whose hops it does not keep. */
        if (former.adr[0] != '\0')
          link_fetch->dest = HTS_FETCH_DEST_UNKNOWN;
        // resolve unresolved type
        if (r_sv != -1 && !link_role_is_base(lk->p_type) &&
            forbidden_url == 0 && IS_DELAYED_EXT(afs.save)) {
          time_t t;

          // pas d'erreur, on continue
          r_sv = hts_wait_delayed(str, &afs, heap(ptr)->adr, heap(ptr)->fil,
                                  &former, &forbidden_url, link_fetch);

          /* User interaction, because hts_wait_delayed can be slow.. (3.43) */
          t = time(NULL);
          if (ps->page.user_interact_timestamp == 0 ||
              t - ps->page.user_interact_timestamp > 0) {
            ps->page.user_interact_timestamp = t;
            hts_mirror_process_user_interaction(str, stre);
          }
        }
        // record!
        if (r_sv != -1) { // pas d'erreur, on continue
          /* log */
          if ((opt->debug > 1) && (opt->log != NULL)) {
            if (forbidden_url != 1) {              // le lien va être chargé
              if (link_role_is_base(lk->p_type)) { // not a link to fetch
                hts_log_print(opt, LOG_DEBUG, "Code/Codebase: %s%s",
                              adr_nologin(opt, afs.af.adr), afs.af.fil);
              } else if ((opt->getmode & HTS_GETMODE_HTML_FIRST) == 0) {
                hts_log_print(opt, LOG_DEBUG, "Record: %s%s -> %s",
                              adr_nologin(opt, afs.af.adr), afs.af.fil,
                              afs.save);
              } else {
                if (!ishtml(opt, afs.af.fil))
                  hts_log_print(opt, LOG_DEBUG, "Record after: %s%s -> %s",
                                adr_nologin(opt, afs.af.adr), afs.af.fil,
                                afs.save);
                else
                  hts_log_print(opt, LOG_DEBUG, "Record: %s%s -> %s",
                                adr_nologin(opt, afs.af.adr), afs.af.fil,
                                afs.save);
              }
            } else
              hts_log_print(opt, LOG_DEBUG, "External: %s%s",
                            adr_nologin(opt, afs.af.adr), afs.af.fil);
          }
          /* FIN log */

          // sf_span_fits in capture_rewrite() needs this size
          HTS_STATIC_ASSERT(sizeof(lien) == HTS_URLMAXSIZE * 2, lien_size);
          eadr = capture_rewrite(
              ps, lk, opt, r, ptr, parent_relative, module_base, &afs,
              forbidden_url, lien, eadr, add_class_dots_to_patch, quote,
              noquote, *p_flush, sf_tag, sf_tagless_body, tag_attr_start);

          if (!capture_record(opt, stre, hash, numero_passe, ptr, codebase, lk,
                              &afs, &former, link_fetch, forbidden_url,
                              just_test_it, set_prio_to, import_done,
                              meme_adresse)) {
            return CAPTURE_LINK_FAIL;
          }

        } // if adr[0]!='\0'

      } // if adr[0]!='\0'

    } // if strlen(lien)>0

  } // if ok==0

  assertf(eadr - ps->html >= 0); // Should not go back
  if (eadr > ps->html) {
    hts_automate_increment(saut, (int) (eadr - 1 - ps->html));
  }

  /* srcset candidate loop: skip the descriptor and comma, then
     have the caller re-enter the capture for the next URL. */
  if (lk->srcset_p && ok == 0) {
    const char *const endp = r->adr + r->size;
    const char *q = ps->html;
    /* whitespace ends an unquoted value, so its descriptor too */
    const hts_boolean unquoted = !noquote && quote == '\0';

    while (q < endp && *q != '\0' && *q != ',' && *q != quote && *q != '<' &&
           *q != '>' && (unsigned char) *q >= 32 &&
           !(unquoted && is_realspace(*q)))
      q++; // skip the descriptor
    if (q < endp && *q == ',') {
      q++;
      while (q < endp && (is_realspace(*q) || *q == ','))
        q++; // skip whitespace and empty candidates
      if (q < endp && *q != '\0' && *q != ',' && *q != quote && *q != '<' &&
          *q != '>' && (unsigned char) *q >= 32) {
        hts_automate_increment(
            saut, (int) (q - ps->html)); // keep the automate in sync
        return CAPTURE_LINK_NEXT;
      }
    }
  }
  return CAPTURE_LINK_DONE;
}

int htsparse(htsmoduleStruct * str, htsmoduleStructExtended * stre) {
  char catbuff[CATBUFF_SIZE];

  /* Load engine variables */
  ENGINE_LOAD_CONTEXT();

  {
    char *cAddr = r->adr;
    int cSize = (int) r->size;
    int took;

    hts_log_print(opt, LOG_DEBUG, "engine: preprocess-html: %s%s",
                  adr_nologin(opt, urladr()), urlfil());
    took = RUN_CALLBACK4(opt, preprocess, &cAddr, &cSize, urladr(), urlfil());
    if (took == 1 && cAddr != NULL) { // a plug-in answering no page keeps ours
      r->adr = cAddr;
      r->size = cSize;
    }
  }
  assertf(r->adr != NULL); // for the analyzer: the caller always holds a page
  if (RUN_CALLBACK4(opt, check_html, r->adr, (int) r->size, urladr(), urlfil())) {
    FILE *fp = NULL; // file written locally
    hts_boolean sf_may_mark = HTS_FALSE;

    hts_log_print(opt, LOG_DEBUG, "scanning file %s%s (%s)..",
                  adr_nologin(opt, urladr()), urlfil(), savename());

    /* Hack to avoid NULL char problems with C syntax */
    /* Yes, some bogus HTML pages can embed null chars
    and therefore can not be properly handled if this hack is not done
    */
    if (r->adr != NULL) {
      size_t i;
      for(i = 0 ; i < (size_t) r->size ; i++) {
        if (r->adr[i] == '\0') {
          r->adr[i] = ' ';
        }
      }
      /* Only chance can put this run's secret in a fetched document. */
      if (opt->single_file) {
        sf_may_mark = singlefile_may_mark(opt, hts_blk_body(r));
      }
    }

    // Indexing!
    if (opt->kindex != HTS_KINDEX_OFF) {
      if (index_keyword(hts_span_at(r->adr, (size_t) r->size), r->contenttype,
                        savename(), StringBuff(opt->path_html_utf8))) {
        hts_log_print(opt, LOG_DEBUG, "indexing file..done");
      } else {
        hts_log_print(opt, LOG_DEBUG, "indexing file..error!");
      }
    }

    // Now, parsing
    if (!*stre->error_) {
      signed char inscript_state[INSCRIPT_NSTATES][257];
      parse_state ps_store = {
          .html = r->adr,
          .tag = {.intag_start = r->adr},
          .script = {.inscript_state_pos = INSCRIPT_START},
          .page = {.link_frame = -1,
                   .sf_may_mark = sf_may_mark,
                   .back_add_stats = opt->state.back_add_stats}};
      parse_state *const ps = &ps_store;

      ps->sink.enabled = (opt->getmode & HTS_GETMODE_HTML) && ptr > 0;
      ps->sink.inscript = &ps->script.inscript;
      ps->sink.inscript_tag = &ps->script.inscript_tag;
      ps->sink.inscriptgen = &ps->script.inscriptgen;
      ps->lastsaved = ps->html;

      /* Initialize script automate for comments, quotes.. */
      memset(inscript_state, 0xff, sizeof(inscript_state));
      inscript_state[INSCRIPT_START][INSCRIPT_DEFAULT] = INSCRIPT_START;        /* by default, stay in START */
      inscript_state[INSCRIPT_START]['\\'] = INSCRIPT_ANTISLASH;        /* #1: \ escapes the next character whatever it is */
      inscript_state[INSCRIPT_ANTISLASH][INSCRIPT_DEFAULT] = INSCRIPT_START;
      inscript_state[INSCRIPT_START]['\''] = INSCRIPT_INQUOTE;  /* #2: ' opens quote and only ' returns to 0 */
      inscript_state[INSCRIPT_INQUOTE][INSCRIPT_DEFAULT] = INSCRIPT_INQUOTE;
      inscript_state[INSCRIPT_INQUOTE]['\''] = INSCRIPT_START;
      inscript_state[INSCRIPT_INQUOTE]['\\'] = INSCRIPT_ANTISLASH_IN_QUOTE;
      inscript_state[INSCRIPT_START]['\"'] = INSCRIPT_INQUOTE2; /* #3: " opens double-quote and only " returns to 0 */
      inscript_state[INSCRIPT_INQUOTE2][INSCRIPT_DEFAULT] = INSCRIPT_INQUOTE2;
      inscript_state[INSCRIPT_INQUOTE2]['\"'] = INSCRIPT_START;
      inscript_state[INSCRIPT_INQUOTE2]['\\'] = INSCRIPT_ANTISLASH_IN_QUOTE2;
      inscript_state[INSCRIPT_START]['/'] = INSCRIPT_SLASH;     /* #4: / state, default to #0 */
      inscript_state[INSCRIPT_SLASH][INSCRIPT_DEFAULT] = INSCRIPT_START;
      inscript_state[INSCRIPT_SLASH]['/'] = INSCRIPT_SLASHSLASH;        /* #5: // with only LF to escape */
      inscript_state[INSCRIPT_SLASHSLASH][INSCRIPT_DEFAULT] =
        INSCRIPT_SLASHSLASH;
      inscript_state[INSCRIPT_SLASHSLASH]['\n'] = INSCRIPT_START;
      inscript_state[INSCRIPT_SLASH]['*'] = INSCRIPT_COMMENT;   /* #6: / * with only * / to escape */
      inscript_state[INSCRIPT_COMMENT][INSCRIPT_DEFAULT] = INSCRIPT_COMMENT;
      inscript_state[INSCRIPT_COMMENT]['*'] = INSCRIPT_COMMENT2;        /* #7: closing comments */
      inscript_state[INSCRIPT_COMMENT2][INSCRIPT_DEFAULT] = INSCRIPT_COMMENT;
      inscript_state[INSCRIPT_COMMENT2]['/'] = INSCRIPT_START;
      inscript_state[INSCRIPT_COMMENT2]['*'] = INSCRIPT_COMMENT2;
      inscript_state[INSCRIPT_ANTISLASH_IN_QUOTE][INSCRIPT_DEFAULT] = INSCRIPT_INQUOTE; /* #8: escape in '' */
      inscript_state[INSCRIPT_ANTISLASH_IN_QUOTE2][INSCRIPT_DEFAULT] = INSCRIPT_INQUOTE2;       /* #9: escape in "" */
      const script_automate saut = {&ps->script.inscript, &ps->script.css_file,
                                    inscript_state,
                                    &ps->script.inscript_state_pos, &ps->html};

      /* Primary list or URLs */
      if (ptr == 0) {
        ps->tag.intag = 1;
        ps->tag.intag_start_valid = 0;
        ps->tag.intag_name = NULL;
      }
      /* Check is the file is a .js file */
      else if ((is_javascript_mime(opt, r->contenttype, str->url_file) != 0) ||
               (compare_mime(opt, r->contenttype, str->url_file, "text/css") !=
                0)) { /* JavaScript js file */
        ps->script.inscript = 1;
        ps->script.incss =
            (compare_mime(opt, r->contenttype, str->url_file, "text/css") != 0);
        ps->script.css_file = ps->script.incss ? HTS_TRUE : HTS_FALSE;
        ps->script.inscript_locked = 1; /* Don't exit js space upon </script> */
        if (opt->parsedebug) {
          HT_ADD("<@@ inscript @@>");
        }
        ps->script.inscript_name = "script";
        ps->tag.intag = 1;             // as after a <script> tag
        ps->tag.intag_start_valid = 0; // yes: this is code, not a real tag
        hts_log_print(opt, LOG_DEBUG, "note: this file is a javascript file");
        // for javascript only
        if (is_javascript_mime(opt, r->contenttype, str->url_file) != 0) {
          // a script's links take the page's base, bar a module specifier
          if (heap(ptr)->precedent != 0) {
            ps->page.parent_relative = 1;
          }
        }
      }
      /* Or a real audio */
      else if (compare_mime(opt, r->contenttype, str->url_file,
                            "audio/x-pn-realaudio") !=
               0) { /* realaudio link file */
        ps->script.inscript = ps->tag.intag = 0;
        ps->script.inscript_name = "media";
        ps->tag.intag_start_valid = 0;
        ps->page.in_media = "LNK"; // real media! -> links
      }
      /* Or a m3u playlist */
      else if (is_playlist_mime(opt, r->contenttype, str->url_file) != 0) {
        ps->script.inscript = ps->tag.intag = 0;
        ps->script.inscript_name = "media";
        ps->tag.intag_start_valid = 0;
        ps->page.in_media = "LNK"; // m3u! -> links
      } else if (compare_mime(opt, r->contenttype, str->url_file,
                              "application/x-authorware-map") !=
                 0) { /* macromedia aam file */
        ps->script.inscript = ps->tag.intag = 0;
        ps->script.inscript_name = "media";
        ps->tag.intag_start_valid = 0;
        ps->page.in_media = "AAM"; // aam
      }

      // Hack to prevent any problems with ram files of other files
      *(r->adr + r->size) = '\0';

      // ------------------------------------------------------------
      // analyser ce qu'il y a en mémoire (fichier html)
      // on scanne les balises
      // ------------------------------------------------------------
      opt->state._hts_in_html_done = 0; // 0% scannés
      opt->state._hts_in_html_parsing = 1;      // flag pour indiquer un parsing

      // the relativeurladr() family reads this name
      const int parent_relative = ps->page.parent_relative;

      base[0] = '\0'; // clear base href
      while (ps->html - r->adr < r->size) {
        parse_link lk; // p_type..add_class are set when a tag body is entered
        lk.p = 0;
        lk.valid_p = 0;
        lk.module_base = 0;
        lk.ending_p = '\0';
        lk.escape_parens = HTS_FALSE;
        lk.entity_end = '\0';
        lk.archivetag_p = 0;
        lk.srcset_p = 0;
        lk.dataattr_p = 0;
        lk.unquoted_script = 0;
        INSCRIPT inscript_state_pos_prev = ps->script.inscript_state_pos;

        *stre->error_ = 0;
        // I1: a comment byte is in no tag, script or media
        assertf(!ps->script.incomment ||
                (!ps->tag.intag && !ps->script.inscript &&
                 !ps->script.inscript_tag && !ps->page.in_media));

        // inline pre-test: the out-of-line helper would run on every byte
        if (!*stre->makeindex_done_ && !ps->page.detect_title &&
            (opt->makeindex || heap(ptr)->depth < opt->depth))
          makeindex_add_page(str, stre, ps->html, &ps->page.detect_title,
                             ps->page.in_media);

        /* Parse */
        if ((*ps->html == '<')         /* No starting tag */
            && !in_quoted_value(ps)    /* Not in title="<b>" */
            && (!ps->script.inscript)  /* Not in (java)script */
            && (!ps->script.incomment) /* Not in comment (<!--) */
            && (!ps->page.in_media)    /* Not in media */
        ) {
          parse_tag_open(ps, opt, r, ptr, codebase);
        } else if ((*ps->html == '>') /* ending tag */
                   &&
                   ((!ps->script.inscript && !ps->page.in_media) ||
                    (ps->script.inscript_tag)) /* and in tag (or in script) */
                   /* a quoted value or handler holds '>' */
                   && !in_quoted_value(ps) && !handler_holds_gt(ps)) {
          parse_tag_close(ps, opt);
        } else if (ps->tag.intag || ps->script.inscript ||
                   ps->page.in_media) { // in a tag, script or media body
          lk.p_type = HTS_LINK_PLAIN;
          lk.p_xmlbase = HTS_FALSE;
          lk.p_nocatch = 0;
          lk.p_searchMETAURL = 0;
          lk.add_class = 0;
          int add_class_dots_to_patch = 0;      // number of '.' in code="x.y.z<realname>"
          const char *p_flush = NULL;

          // a quoted value ends only at its own quote, as in a browser
          if ((*ps->html == '"' || *ps->html == '\'') && ps->tag.intag &&
              !ps->script.inscript && !in_quoted_value(ps) &&
              quote_opens_value(ps))
            ps->tag.quoted_value_end =
                attr_value_end(ps->html + 1, *ps->html, r);

          // ------------------------------------------------------------
          // link and script detection
          // ------------------------------------------------------------
          if (((hts_isalpha((unsigned char) *ps->html)) || (*ps->html == '/') ||
               (ps->script.inscript) || (ps->page.in_media) ||
               (ps->script.inscriptgen))) { // else nothing to test

            /* leave a handler or javascript: URL at its end, as in
               <a href="javascript:()" action="foo"> */
            if (ps->script.inscript_tag && handler_ends_here(ps)) {
              ps->script.handler_value_end = NULL;
              ps->script.inscript_tag = ps->script.inscript = 0;
              ps->script.inscriptgen = 0;
              ps->script.inscript_state_pos = INSCRIPT_START;
              ps->script.incomment = 0;
              if (opt->parsedebug) {
                HT_ADD("<@@ /inscript @@>");
              }
            }

            /* automate */
            hts_automate_lookup(&saut);

            // Note:
            // Certaines pages ne respectent pas le html
            // notamment les guillements ne sont pas fixés
            // Nous sommes dans un tag, donc on peut faire un test plus
            // large pour pouvoi prendre en compte ces particularités

            // à vérifier: ACTION, CODEBASE, VRML

            if (ps->page.in_media) {
              detect_media(ps, &lk, opt, r);
            } else if (ptr > 0) { /* not the primary page 0 */
              if (ps->script.inscript) {
                detect_js_write(ps, opt, r);
              }
              // in a tag outside a script, or in HTML a script writes
              if ((ps->tag.intag && (!ps->script.inscript)) ||
                  ps->script.inscriptgen) {
                detect_tag_attr(ps, &lk, opt, str, r);
              } else if (ps->script.inscript) {
                detect_script(ps, &lk, opt, r);
              }

            } else {            // ptr == 0
              lk.valid_p = 1;   // Valid even if p==0
              while ((ps->html[lk.p] == '\r') || (ps->html[lk.p] == '\n'))
                lk.p++;
              lk.ending_p = '\r';
            }

          } else if (hts_isspace((unsigned char) *ps->html)) {
            // attribute in tag (for dirty parsing)
            ps->tag.intag_startattr = ps->html + 1;
          }

          if (opt->parseall && (opt->parsejava & HTSPARSE_NO_AGGRESSIVE) == 0 &&
              (ptr > 0) && (!ps->page.in_media)) {
            if (!is_realspace(*ps->html))
              parse_dirty_scan(ps, &lk, opt, r, inscript_state_pos_prev);
          }

          // ------------------------------------------------------------
          // p!=0 : on a repéré un éventuel lien
          // ------------------------------------------------------------
          //
          if ((lk.p > 0) || (lk.valid_p)) { // on a repéré un lien
            // the relativeurladr() family reads this name
            const int module_base = lk.module_base;

            int ok = 1;
            char quote = '\0';
            int quoteinscript = 0;
            int noquote = 0;
            const char *tag_attr_start = ps->html;
            /* Save the tag for --single-file now, because the value scan
               below clears intag on an unquoted value ending at '>'. */
            const char *const sf_tag =
                ps->tag.intag_start_valid ? ps->tag.intag_name : NULL;
            const int sf_tagless_body = ps->script.inscript_locked;
            hts_fetch_meta link_fetch = {0};

            /* Past the cap nothing fetches links, so write them external. */
            if (hts_maxlinks_no_room(opt)) {
              lk.p_nocatch = 1;
              maxlinks_give_up(opt, stre->exit_xh_);
            }
            // si nofollow ou un stop a été déclenché, réécrire tous les liens en externe
            /* an update keeps following, so the cache keeps its data */
            if ((ps->page.nofollow) ||
                (hts_load_acquire_int(&opt->state.stop) && !opt->is_update))
              lk.p_nocatch = 1;

            // écrire codebase avant, flusher avant code
            if (lk.p_type == HTS_LINK_APPLET_CODE ||
                lk.p_type == HTS_LINK_CODEBASE) {
              HT_add_adr;       // refresh
              ps->lastsaved = ps->html; // one past the last byte written
            }
            // sauter espaces
            hts_automate_increment(&saut, lk.p);
            while ((is_space(*ps->html) ||
                    (ps->script.inscriptgen && ps->html[0] == '\\' &&
                     is_space(ps->html[1]))) &&
                   quote == '\0') {
              if (!quote)
                if ((*ps->html == '\"') || (*ps->html == '\'')) {
                  quote = *ps->html; // the closing quote to wait for
                  if (ps->script.inscriptgen && *(ps->html - 1) == '\\') {
                    quoteinscript = 1;  /* will wait for \" */
                  }
                }
              // puis quitter
              // html++;    // sauter les espaces, "" et cie
              hts_automate_increment(&saut, 1);
            }

            /* Stop at \n (LF) if primary links or link lists */
            if (ptr == 0 ||
                (ps->page.in_media && strcmp(ps->page.in_media, "LNK") == 0))
              quote = '\n';
            /* s'arrêter que ce soit un ' ou un " : pour document.write('<img src="foo'+a); par exemple! */
            else if (ps->script.inscript && !lk.unquoted_script)
              noquote = 1;

            // sauter éventuel \" ou \' javascript
            if (ps->script.inscript) { // inside obj.write("..
              if (*ps->html == '\\') {
                if ((*(ps->html + 1) == '\'') ||
                    (*(ps->html + 1) == '"')) { // \" or \'
                  hts_automate_increment(&saut, 2);
                }
              }
            }
            // sauter content="1;URL=http://..
            if (lk.p_searchMETAURL) {
              int l = 0;

              while ((ps->html + l + 4 < r->adr + r->size) &&
                     !hts_starts_with_ci(ps->html + l, "URL=") && (l < 128))
                l++;
              if (!hts_starts_with_ci(ps->html + l, "URL="))
                ok = -1;
              else
                ps->html += (l + 4);
            }

            /* parse javascript:document.location=.. as code, not as a link */
            if (ok != -1) {
              // not inside a script, where document.write may hold it
              if (hts_starts_with_ci(ps->html, "javascript:") &&
                  !ps->script.inscript) {
                ok = -1;
                /*
                   On est désormais dans du code javascript
                 */
                ps->script.inscript_name = "";
                /* A javascript: URL is JavaScript, whatever an earlier
                   <style> left in incss. */
                ps->script.incss = 0;
                ps->script.inscript_tag = ps->script.inscript = 1;
                ps->script.inscript_state_pos = INSCRIPT_START;
                /* the quote that ends it */
                ps->script.inscript_tag_lastc = quote;
                ps->script.handler_value_end =
                    handler_end_at(ps->html, quote, r);
                if (opt->parsedebug) {
                  HT_ADD("<@@ inscript @@>");
                }
              }
            }

          srcset_next:
            switch (capture_link(
                ps, &lk, opt, str, stre, r, sback, cache, hash, numero_passe,
                ptr, parent_relative, module_base, codebase, HTS_BUF(catbuff),
                &saut, ok, quote, quoteinscript, noquote, tag_attr_start,
                sf_tag, sf_tagless_body, &link_fetch, &add_class_dots_to_patch,
                &p_flush)) {
            case CAPTURE_LINK_NEXT:
              ok = 1;
              goto srcset_next;
            case CAPTURE_LINK_FAIL:
              TypedArrayFree(ps->sink.buf);
              return -1;
            case CAPTURE_LINK_DONE:
              break;
            }

            /* We skipped bytes and skip the " : reset state */

 } // if (p)

        } // si '<' ou '>'

        parse_advance(ps, r, ptr);
        parse_flush(ps, r);
        if (!parse_poll(ps, opt, sback, cache, stre, r, ptr, numero_passe)) {
          TypedArrayFree(ps->sink.buf);
          return -1; // user exit
        }
      }

      // </html> but no <head>, <body> or <meta>: one footer at the very end
      if (ps->page.emited_footer == 0 && ps->page.bare_html_end &&
          !ps->tag.intag && (opt->getmode & HTS_GETMODE_HTML) &&
          is_html_mime_type(r->contenttype)) {
        ps->page.emited_footer = 2; // no charset meta this late
        parse_emit_footer(ps, opt, r, ptr);
      }

      opt->state._hts_in_html_parsing = 0;      // flag
      opt->state._hts_cancel = 0;       // pas de cancel

      if (ps->sink.enabled) {
        if (!ps->sink.oom) {
          char *cAddr = TypedArrayElts(ps->sink.buf);
          int cSize = (int) TypedArraySize(ps->sink.buf);

          hts_log_print(opt, LOG_DEBUG, "engine: postprocess-html: %s%s",
                        adr_nologin(opt, urladr()), urlfil());
          if (RUN_CALLBACK4(opt, postprocess, &cAddr, &cSize, urladr(), urlfil()) == 1) {
            hts_log_print(opt, LOG_DEBUG,
              "engine: postprocess-html: callback modified data, applying %d bytes", cSize);
            /* A reply inside sink.buf was edited in place. Move it down, since
               an append onto itself can move the buffer under cAddr. */
            if (!hts_postprocess_reply_ok(cAddr, cSize,
                                          TypedArrayElts(ps->sink.buf),
                                          TypedArraySize(ps->sink.buf),
                                          TypedArrayCapa(ps->sink.buf))) {
              hts_log_print(opt, LOG_ERROR,
                            "engine: postprocess-html: callback returned %d "
                            "bytes for %s%s, page skipped",
                            cSize, adr_nologin(opt, urladr()), urlfil());
              HT_SET_SIZE(0);
              *stre->error_ = 1;
            } else if (hts_postprocess_reply_inplace(
                           cAddr, TypedArrayElts(ps->sink.buf),
                           TypedArrayCapa(ps->sink.buf))) {
              memmove(TypedArrayElts(ps->sink.buf), cAddr, (size_t) cSize);
              HT_SET_SIZE((size_t) cSize);
            } else {
              HT_SET_SIZE(0);
              HT_ADD_N(cAddr, (size_t) cSize);
            }
          }
        }

        /* Flush and save to disk */
        if (ps->sink.oom) {
          hts_log_print(opt, LOG_ERROR,
                        "Not enough memory to rewrite %s%s, page skipped",
                        adr_nologin(opt, urladr()), urlfil());
          *stre->error_ = 1;
        } else if (TypedArraySize(ps->sink.buf) != 0) {
          hts_finish_html_file(opt, cache, r, &fp,
                               hts_span_at(TypedArrayElts(ps->sink.buf),
                                           TypedArraySize(ps->sink.buf)),
                               urladr(), urlfil(), savename());
        }
      }
      /* outside the write: mimehtml appends whatever the output mode is */
      TypedArrayFree(ps->sink.buf);
      //
      //
      //
    } // if !error

  }                             // analyse OK

  return 0;
}

/* Contract in htsparse.h. */
hts_boolean hts_postprocess_reply_inplace(const char *html, const char *buffer,
                                          size_t capa) {
  /* through uintptr_t: comparing pointers into unrelated objects is not
     defined, and the callback's buffer is one */
  const uintptr_t at = (uintptr_t) html, base = (uintptr_t) buffer;

  return buffer != NULL && at >= base && at - base < capa;
}

/* Contract in htsparse.h. */
hts_boolean hts_postprocess_reply_ok(const char *html, int len,
                                     const char *buffer, size_t size,
                                     size_t capa) {
  if (len < 0 || html == NULL)
    return HTS_FALSE;
  if (hts_postprocess_reply_inplace(html, buffer, capa)) {
    const size_t at = (size_t) (html - buffer);

    return at <= size && (size_t) len <= size - at;
  }
  return HTS_TRUE;
}

/* Mirror the savename to tell whether a redirect saves to the same file (#159);
 * contract in htsparse.h. */
hts_boolean hts_redirect_same_savefile(httrackp *opt, const char *cur_adr,
                                       const char *cur_fil,
                                       const char *moved_adr,
                                       const char *moved_fil) {
  const int norm_slash = opt->urlhack && !opt->no_slash_dedup;
  const int norm_query = opt->urlhack && !opt->no_query_dedup;
  char BIGSTK n_fil[HTS_URLMAXSIZE * 2], pn_fil[HTS_URLMAXSIZE * 2];

  if (hts_strcasecmp(jump_identification_const(moved_adr),
                     jump_identification_const(cur_adr)) != 0)
    return HTS_FALSE;
  fil_normalized_filtered_ex(moved_fil, n_fil, NULL, norm_slash, norm_query);
  fil_normalized_filtered_ex(cur_fil, pn_fil, NULL, norm_slash, norm_query);
  return hts_strcasecmp(n_fil, pn_fil) == 0;
}
