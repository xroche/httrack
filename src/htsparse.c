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
#include <ctype.h>

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

/* The parse_sink of htsparse(), under the short names its call sites use. */
#define HT_ADD_N(PTR, COUNT) sink_add_n(&sink, (PTR), (COUNT))
#define HT_ADD(A) sink_add(&sink, (A))
#define HT_ADD_BUF(A, N) HT_ADD_N(A, htssafe_strnlen_(A, N))
#define HT_ADD_HTMLESCAPED(A) sink_add_htmlescaped(&sink, (A))
#define HT_ADD_HTMLESCAPED_FULL(A) sink_add_htmlescaped_full(&sink, (A))
#define HT_ADD_URL_HTMLESCAPED(A) sink_add_url_htmlescaped(&sink, (A))
#define HT_SET_SIZE(N) sink_set_size(&sink, (N))

/** Append bytes to the output buffer up to the pointer 'html'. **/
#define HT_add_adr                                                             \
  do {                                                                         \
    if (sink.enabled && html != lastsaved) {                                   \
      HT_ADD_N(lastsaved, (size_t) (html - lastsaved));                        \
      lastsaved = html;                                                        \
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

/* Live view of the parser's automaton locals, set up once so the helpers below
   can drive it without capturing them by lexical scope. */
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
const char *hts_nodetect[] = {
    "accept-charset", "accesskey", "action",   "align",    "alt",     "axes",
    "axis",           "char",      "charset",  "cite",     "class",   "classid",
    "code",           "color",     "datetime", "dir",      "enctype", "face",
    "height",         "id",        "lang",     "language", "media",   "method",
    "name",           "prompt",    "scheme",   "size",     "style",   "target",
    "title",          "type",      "valign",   "version",  "width",   ""};

const char *hts_main_mime[] = {"application", "audio", "image", "message",
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

    if (len > l && strncasecmp(name, prefixes[i], l) == 0)
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

/* Is the attribute name [name, nend) one whose value is never a link, a
   hts_nodetect word or an xmlns declaration? Returns the word, or NULL. */
static const char *hts_dirty_attr_refused(const char *name, const char *nend) {
  int i;
  for (i = 0; strnotempty(hts_nodetect[i]); i++) {
    const int l = strfield(name, hts_nodetect[i]);
    if (l && name + l == nend)
      return hts_nodetect[i];
  }
  i = strfield(name, "xmlns");
  if (i && (name + i == nend || name[i] == ':'))
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
static const char *html_inline_safe(const char *src, char *dst, size_t size) {
  size_t i, j;

  for(i = 0, j = 0; src[i] != '\0' && j + 4 < size; i++) {
    const char c = src[i];

    if (c == '<' || c == '>') {
      dst[j++] = '%';
      dst[j++] = '3';
      dst[j++] = (c == '<') ? 'C' : 'E';
    } else {
      dst[j++] = c;
    }
  }
  dst[j] = '\0';
  return dst;
}

/* Copies the first max bytes of the page text s, len bytes long, into buf
   for a debug line, with a URL login masked, or "(hidden)" when a login
   cannot be told apart. */
static const char *text_excerpt(httrackp *opt, char *buf, size_t size,
                                const char *s, size_t len, size_t max) {
  char *copy = malloct(len + 1);
  const char *shown;

  buf[0] = '\0';
  if (copy == NULL)
    return buf;
  memcpy(copy, s, len);
  copy[len] = '\0';
  shown = url_nologin(opt, copy);
  if (shown == copy && jump_protocol_const(copy) == copy &&
      strchr(copy, '@') != NULL)
    shown = "(hidden)";
  strlncatbuff(buf, shown, size, minimum(strlen(shown), max));
  freet(copy);
  return buf;
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

/* Charset to decode the page with, or NULL when conversion is off. *declared
   tells a charset the document named from the iso-8859-1 guess standing in. */
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

/* True if [s, s+len) is exactly an HTTP method token (XHR.open's first
   argument is a method, not a URL: #218). Case-insensitive. */
static int is_http_method(const char *s, size_t len) {
  static const char *const methods[] = {"GET",    "POST",  "PUT",
                                        "DELETE", "HEAD",  "OPTIONS",
                                        "PATCH",  "TRACE", NULL};
  int i;

  for (i = 0; methods[i] != NULL; i++) {
    if (strlen(methods[i]) == len && strfield(s, methods[i]) == (int) len)
      return 1;
  }
  return 0;
}

/* Contract in htsparse.h. */
hts_boolean hts_dirty_link_is_url(httrackp *opt, const char *str, size_t len,
                                  char lastc, hts_boolean inscript) {
  char BIGSTK tempo[HTS_URLMAXSIZE * 2];
  char type[256];
  hts_boolean url_ok = HTS_FALSE;
  hts_boolean had_fragment_or_query;
  char *a;

  /* The parser never offers more, and a caller that does gets no verdict
     rather than an aborted process. */
  if (len >= HTS_URLMAXSIZE)
    return HTS_FALSE;
  tempo[0] = '\0';
  type[0] = '\0';
  strncatbuff(tempo, str, len);

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
  if (tempo[0] == '.' && isalnum((unsigned char) tempo[1])) // ".gif"
    return HTS_FALSE;

  // a trailing '+' concatenates, so only an extension speaks for the string
  if (lastc != '+') {
    // "Comparisons of scheme names MUST be case-insensitive" (RFC2616)
    if ((strfield(tempo, "http:")) || (strfield(tempo, "ftp:"))
#if HTS_USEOPENSSL
        || (strfield(tempo, "https:"))
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
    else if (is_dyntype(get_ext(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                                tempo))) // php, cgi, asp
      url_ok = HTS_TRUE;
    // but not foobar@aol.com
    if (strchr(tempo, '@'))
      url_ok = HTS_FALSE;
  }
  return url_ok;
}

/* Can the byte before "at" continue a name, so that the keyword is the tail
   of a longer one rather than one of its own? A dot glues because the keyword
   is then a property name, which the ".xxx" rows above take when they want
   it. A quote does not: it ends a string, and both CSS and a template literal
   put a url() right after one. */
static hts_boolean js_glues_keyword(const char *at, const char *buffer) {
  const unsigned char *const p = (const unsigned char *) at;
  const unsigned char c = (unsigned char) html_prevc(at, buffer);
  const size_t before = (size_t) (at - buffer);

  if (c < 0x80)
    return isalnum(c) || c == '_' || c == '$' || c == '.';
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
  return strfield(s, "http:") || strfield(s, "https:") || strfield(s, "ftp:")
             ? HTS_TRUE
             : HTS_FALSE;
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
  const char *a;
  int nc;

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

  nc = strfield(cursor, ".src"); // nom.src="image";
  if (!nc && in_tag && tag_lastc == html_prevc(cursor, buffer))
    nc = strfield(cursor, "src"); // onXXX='src="image";'
  if (!nc)
    nc = strfield(cursor, ".location"); // document.location="doc"
  if (!nc)
    nc = strfield(cursor, ":location"); // javascript:location="doc"
  if (!nc) {                            // location="doc"
    if ((nc = strfield(cursor, "location")) &&
        !isspace(html_prevc(cursor, buffer)))
      nc = 0;
  }
  if (!nc)
    nc = strfield(cursor, ".href"); // document.location="doc"
  if (!nc)
    if ((nc = strfield(cursor, ".open"))) { // window.open("doc",..
      expected = '(';
      expected_end = "),";   // ends on a comma or the paren
      ensure_not_mime = 1;   // refuse a mime type
      ensure_not_method = 1; // xhr.open: don't grab method
    }
  if (!nc)
    if ((nc = strfield(cursor, ".replace"))) { // window.replace("url")
      expected = '(';
      expected_end = ")";
    }
  if (!nc)
    if ((nc = strfield(cursor, ".link"))) { // window.link("url")
      expected = '(';
      expected_end = ")";
    }
  if (!nc)
    if ((nc = strfield(cursor, ".url"))) { // obj.url("doc")
      expected = '(';
      expected_end = ")";
    }
  /* The guard has to gate the match: a name ending in "url" must leave nc at
     zero, or the default rule takes its assignment as a link. */
  if (!nc &&
      !(in_css ? css_glues_keyword(cursor, buffer)
               : js_glues_keyword(cursor, buffer)) &&
      (nc = strfield(cursor, "url")) != 0) { // url(url)
    expected = '(';
    expected_end = ")";
    /* CSS writes url(foo.png) unquoted, but JavaScript's new URL(x) matches
       the same token, so an unquoted operand there rewrites the expression
       itself. */
    can_avoid_quotes = in_css;
    quotes_replacement = ')';
  }
  if (!nc)
    if ((nc = strfield(cursor, "import"))) { // import "url"
      if (is_space(*(cursor + nc))) {
        expected = 0; // no char expected
        is_import = 1;
      } else
        nc = 0;
    }
  if (!nc) { // import x from "./mod.js"
    /* The guard has to gate the match: a name merely ending in "from" must
       leave nc at zero, or the default rule takes its assignment as a link.
       A quote before it ends a string, and no import clause starts there. */
    const char before = html_prevc(cursor, buffer);

    if (!js_glues_keyword(cursor, buffer) && before != '"' && before != '\'' &&
        before != '`' && (nc = strfield(cursor, "from")) != 0) {
      expected = 0;
      is_specifier = 1;
    }
  }
  if (!nc)
    return HTS_FALSE;

  a = cursor + nc;
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
        int p;

        if ((p = strfield(a, hts_main_mime[i])) && a[p] == '/') {
          a = NULL;
        }
        i++;
      }
    }
    // XHR.open's "GET" etc. is a method, not a URL
    if (a != NULL && ensure_not_method &&
        is_http_method(a, (size_t) (c - a + 1)))
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

/* How a browser fetches the link found while parsing link ptr. Links in a
   stylesheet or script file compare with the page that loaded that file. */
static void parse_fetch_meta(httrackp *opt, int ptr, hts_fetch_meta *fm,
                             hts_boolean redirect_stub,
                             hts_boolean meta_refresh, hts_boolean opaque,
                             hts_boolean inscript, hts_boolean incss,
                             hts_boolean tagless_body, int link_frame,
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
  if (redirect_stub) {
    *fm = *page;
    hts_fetch_through(opt, fm, heap(ptr)->adr);
    return;
  }
  /* A refresh navigates whatever frame holds the page. */
  if (meta_refresh)
    fm->dest = (unsigned char) hts_fetch_same_frame(page->dest);
  else if (!opaque && inscript && incss)
    hts_fetch_classify(fm, tag_cache, NULL, attr, fil, HTS_FETCH_DEST_UNKNOWN);
  /* A script body names URLs for code a crawler never runs. */
  else if (!opaque && !inscript && tag_name != NULL)
    hts_fetch_classify(fm, tag_cache, tag_name, attr, fil, link_frame);
  /* A file no known page loaded leaves initiator 0, which sends none. */
  fm->initiator = tagless_body ? page->initiator : ptr;
}

/* Is this raw data-src / data-srcset value a lazy-load placeholder rather
   than a URL? Templates ({{x}}, JSON) and a few placeholder words are. */
static hts_boolean data_attr_is_not_url(const char *s, size_t len) {
  static const char *const words[] = {
      "lazy",  "placeholder", "none",  "null",  "undefined",
      "false", "true",        "blank", "empty", NULL};
  size_t i;

  while (len > 0 && (unsigned char) *s <= ' ') {
    s++;
    len--;
  }
  while (len > 0 && (unsigned char) s[len - 1] <= ' ')
    len--;
  for (i = 0; i < len && s[i] != '?'; i++) { /* a query may carry JSON */
    if (s[i] == '{' || s[i] == '}' || s[i] == '<' || s[i] == '>' || s[i] == '"')
      return HTS_TRUE;
  }
  for (i = 0; words[i] != NULL; i++) {
    if (strlen(words[i]) == len && strncasecmp(s, words[i], len) == 0)
      return HTS_TRUE;
  }
  return HTS_FALSE;
}

/* Percent-encode '(' and ')' in a link emitted into an unquoted url(...) (CSS
   or JS): a literal ')' closes the token early and the UA mis-parses the value
   (#163). The UA decodes %28/%29 back to the saved-on-disk name. */
static void escape_url_parens(char *const s, const size_t size) {
  char BIGSTK buff[HTS_URLMAXSIZE * 2];
  size_t i, j;

  for (i = 0, j = 0; s[i] != '\0' && j + 3 < size && j + 3 < sizeof(buff);
       i++) {
    if (s[i] == '(' || s[i] == ')') {
      buff[j++] = '%';
      buff[j++] = '2';
      buff[j++] = s[i] == '(' ? '8' : '9';
    } else {
      buff[j++] = s[i];
    }
  }
  buff[j] = '\0';
  strlcpybuff(s, buff, size);
}

/* Copy a URL piece with " ' < > percent-encoded, so it cannot end the
   attribute or script string it is written into; with html, & becomes &amp;.
   Returns the length written, or size on overflow. */
static size_t escape_url_quotes_any(const char *const s, char *const dest,
                                    const size_t size, const hts_boolean html) {
  static const char hex[] = "0123456789abcdef";
  size_t i, j = 0;

  if (size == 0)
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
    if (n >= size - j) {
      dest[j] = '\0';
      return size;
    }
    memcpy(dest + j, out, n);
    j += n;
  }
  dest[j] = '\0';
  return j;
}

static size_t escape_url_quotes(const char *const s, char *const dest,
                                const size_t size) {
  return escape_url_quotes_any(s, dest, size, HTS_FALSE);
}

static size_t escape_url_quotes_html(const char *const s, char *const dest,
                                     const size_t size) {
  return escape_url_quotes_any(s, dest, size, HTS_TRUE);
}

/* The rewritten page htsparse() builds. */
typedef struct parse_sink {
  TypedArray(char) buf;
  /* Latched when buf cannot grow. */
  hts_boolean oom;
  /* FALSE when the page is not saved as HTML: every write is then dropped. */
  hts_boolean enabled;
  /* The parser's script state: script text is raw, but a handler is markup. */
  const int *inscript;
  const int *inscript_tag;
} parse_sink;

typedef size_t (*sink_escaper)(const char *const s, char *const dest,
                               const size_t size);

/* Append 'count' bytes from 'p'. */
static void sink_add_n(parse_sink *const sink, const char *const p,
                       const size_t count) {
  if (!sink->enabled)
    return;
  TypedArrayEnsureRoom(sink->buf, count);
  if (TypedArrayHasRoom(sink->buf, count)) {
    TypedArrayAppend(sink->buf, p, count);
  } else {
    sink->oom = HTS_TRUE;
  }
}

static void sink_add(parse_sink *const sink, const char *const s) {
  sink_add_n(sink, s, strlen(s));
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
  TypedArrayEnsureRoom(sink->buf, want);
  if (TypedArrayHasRoom(sink->buf, want)) {
    TypedArraySize(sink->buf) +=
        escape(s, &TypedArrayTail(sink->buf), TypedArrayRoom(sink->buf));
  } else {
    sink->oom = HTS_TRUE;
  }
}

/* Is the parser inside script or style text, where no entity is decoded? */
static hts_boolean sink_raw_text(const parse_sink *const sink) {
  return *sink->inscript && !*sink->inscript_tag;
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

    sink_add_n(sink, adr, (size_t) (aut - adr) + 2);
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

char *hts_parse_sink_selftest(const hts_boolean enabled, const int inscript,
                              const int inscript_tag, const char *const s) {
  parse_sink sink = {EMPTY_TYPED_ARRAY, HTS_FALSE, enabled, &inscript,
                     &inscript_tag};
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
  if (strfield(lien, "https:"))
    return 443;
  if (strfield(lien, "ftp:"))
    return 21;
  return 80;
}

/* Strip the scheme's own default port (80 http, 443 https, 21 ftp) from lien's
   authority in place; :80 on https/ftp stays as a real port (#638, #614). */
void hts_strip_default_port(char *lien, size_t size) {
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

    while (isdigit((unsigned char) *b))
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
      strlcpybuff(lien, tempo, size);
    }
  }
}

/* Main parser */
/* Say once that the -#L cap is full, and record the abort #1419 gave it. The
   parse then finishes, so the page it has rewritten is kept. */
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

int htsparse(htsmoduleStruct * str, htsmoduleStructExtended * stre) {
  char catbuff[CATBUFF_SIZE];

  /* Load engine variables */
  ENGINE_LOAD_CONTEXT();

  {
    char *cAddr = r->adr;
    int cSize = (int) r->size;

    hts_log_print(opt, LOG_DEBUG, "engine: preprocess-html: %s%s",
                  adr_nologin(opt, urladr()), urlfil());
    if (RUN_CALLBACK4(opt, preprocess, &cAddr, &cSize, urladr(), urlfil()) == 1) {
      r->adr = cAddr;
      r->size = cSize;
    }
  }
  if (RUN_CALLBACK4(opt, check_html, r->adr, (int) r->size, urladr(), urlfil())) {
    FILE *fp = NULL;                  // fichier écrit localement 
    const char *html = r->adr;        // pointeur (on parcours)
    const char *lastsaved;            // adresse du dernier octet sauvé + 1
    hts_boolean sf_may_mark = HTS_FALSE; // --single-file: mark this one?

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
        sf_may_mark = singlefile_may_mark(opt, r->adr, (size_t) r->size);
      }
    }

    // Indexing!
    if (opt->kindex) {
      if (index_keyword
          (r->adr, r->size, r->contenttype, savename(),
           StringBuff(opt->path_html_utf8))) {
        hts_log_print(opt, LOG_DEBUG, "indexing file..done");
      } else {
        hts_log_print(opt, LOG_DEBUG, "indexing file..error!");
      }
    }

    // Now, parsing
    if (!error) {
      // output HTML
      parse_sink sink = {EMPTY_TYPED_ARRAY, HTS_FALSE, HTS_FALSE, NULL, NULL};

      time_t user_interact_timestamp = 0;
      int detect_title = 0;     // détection  du title
      int back_add_stats = opt->state.back_add_stats;

      const char *in_media = NULL;    // in other media type (real media and so..)
      int intag = 0;            // on est dans un tag
      int incomment = 0;        // dans un <!--
      const char *comment_close = NULL;         // cached "-->" at or after html
      hts_boolean comment_unclosed = HTS_FALSE; // no "-->" left in the page
      int inscript = 0;         // dans un scipt pour applets javascript)
      int incss = 0;            /* CSS lets url(x) go unquoted */
      int link_frame =
          -1; /* what a link with no target navigates, once known */
      hts_fetch_tag fetch_tag = {NULL};
      hts_boolean css_file = HTS_FALSE;
      int inscript_locked = 0;  // in locked script (ie. js file)
      signed char inscript_state[INSCRIPT_NSTATES][257];
      INSCRIPT inscript_state_pos = INSCRIPT_START;
      const char *inscript_name = NULL;       // script tag name
      int inscript_tag = 0;     // on est dans un <body onLoad="... terminé par >
      char inscript_tag_lastc = '\0';

      // terminaison (" ou ') du "<body onLoad=.."
      int inscriptgen = 0;      // on est dans un code générant, ex après obj.write("..

      char scriptgen_q = '\0';  // caractère faisant office de guillemet (' ou ")

      int nofollow = 0;         // ne pas scanner

      //
      int parseall_lastc = '\0';        // dernier caractère parsé pour parseall

      //
      const char *intag_start = html;
      const char *intag_name = NULL;
      const char *intag_startattr = NULL;
      int intag_start_valid = 0;
      int intag_ctype = 0;

      //
      int emited_footer = 0;    // emitted footer comment tag(s) count

      //
      int parent_relative = 0;  // the parent is the base path (.js, .css..)

      sink.enabled = (opt->getmode & HTS_GETMODE_HTML) && ptr > 0;
      sink.inscript = &inscript;
      sink.inscript_tag = &inscript_tag;
      lastsaved = html;

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
      const script_automate saut = {&inscript, &css_file, inscript_state,
                                    &inscript_state_pos, &html};

      /* Primary list or URLs */
      if (ptr == 0) {
        intag = 1;
        intag_start_valid = 0;
        intag_name = NULL;
      }
      /* Check is the file is a .js file */
      else if ((is_javascript_mime(opt, r->contenttype, str->url_file) != 0) ||
               (compare_mime(opt, r->contenttype, str->url_file, "text/css") !=
                0)) { /* JavaScript js file */
        inscript = 1;
        incss =
            (compare_mime(opt, r->contenttype, str->url_file, "text/css") != 0);
        css_file = incss ? HTS_TRUE : HTS_FALSE;
        inscript_locked = 1;    /* Don't exit js space upon </script> */
        if (opt->parsedebug) {
          HT_ADD("<@@ inscript @@>");
        }
        inscript_name = "script";
        intag = 1;              // because après <script> on y est .. - pas utile
        intag_start_valid = 0;  // OUI car nous sommes dans du code, plus dans du "vrai" tag
        hts_log_print(opt, LOG_DEBUG, "note: this file is a javascript file");
        // for javascript only
        if (is_javascript_mime(opt, r->contenttype, str->url_file) != 0) {
          // a script's links take the page's base, bar a module specifier
          if (heap(ptr)->precedent != 0) {
            parent_relative = 1;
          }
        }
      }
      /* Or a real audio */
      else if (compare_mime(opt, r->contenttype, str->url_file,
                            "audio/x-pn-realaudio") !=
               0) { /* realaudio link file */
        inscript = intag = 0;
        inscript_name = "media";
        intag_start_valid = 0;
        in_media = "LNK";       // real media! -> links
      }
      /* Or a m3u playlist */
      else if (compare_mime(opt, r->contenttype, str->url_file,
                            "audio/x-mpegurl") != 0) { /* mp3 link file */
        inscript = intag = 0;
        inscript_name = "media";
        intag_start_valid = 0;
        in_media = "LNK";       // m3u! -> links
      } else if (compare_mime(opt, r->contenttype, str->url_file,
                              "application/x-authorware-map") !=
                 0) { /* macromedia aam file */
        inscript = intag = 0;
        inscript_name = "media";
        intag_start_valid = 0;
        in_media = "AAM";       // aam
      }
      /* Or a RSS file */
      else if (compare_mime(opt, r->contenttype, str->url_file, "text/xml") !=
                   0 ||
               compare_mime(opt, r->contenttype, str->url_file,
                            "application/xml") != 0) {
        if (strstr(html, "http://purl.org/rss/") != NULL)        // Hmm, this is a bit lame ; will have to cleanup
        {                       /* RSS file */
          inscript = intag = 0;
          intag_start_valid = 0;
          in_media = NULL;      // regular XML
        } else {                // cancel: write all
          html = r->adr + r->size;
          HT_add_adr;
          lastsaved = html;
        }
      }

      // Hack to prevent any problems with ram files of other files
      *(r->adr + r->size) = '\0';

      // ------------------------------------------------------------
      // analyser ce qu'il y a en mémoire (fichier html)
      // on scanne les balises
      // ------------------------------------------------------------
      opt->state._hts_in_html_done = 0; // 0% scannés
      opt->state._hts_in_html_parsing = 1;      // flag pour indiquer un parsing

      base[0] = '\0';           // effacer base-href
      do {
        int p = 0;
        int valid_p = 0;        // force to take p even if == 0
        int module_base = 0;    // module specifier: resolve against the script
        int ending_p = '\0';    // ending quote?
        hts_boolean escape_parens = HTS_FALSE; // is this a CSS url()?
        char entity_end = '\0'; // quote of an entity-quoted url()
        int archivetag_p = 0;   // avoid multiple-archives with commas
        int srcset_p = 0;       // srcset="url1 480w, url2 2x": list of URLs
        int dataattr_p = 0;     // data-src / data-srcset: may hold a template
        int unquoted_script = 0;
        INSCRIPT inscript_state_pos_prev = inscript_state_pos;

        error = 0;

        /* Break if we are done yet */
        if (html - r->adr >= r->size)
          break;

        /*
           index.html built here
         */
        // Construction index.html (sommaire)
        // Avant de tester les a href,
        // Ici on teste si l'on doit construire l'index vers le(s) site(s) miroir(s)
        if (!makeindex_done) {  // autoriation d'écrire un index
          if (!detect_title) {
            if (opt->depth == heap(ptr)->depth) {      // on note toujours les premiers liens
              if (!in_media) {
                if (opt->makeindex && (ptr > 0)) {
                  if (opt->getmode & HTS_GETMODE_HTML) {
                    p = strfield(html, "title");
                    if (p) {
                      if (html_prevc(html, r->adr) == '/')
                        p = 0;  // /title
                    } else {
                      if (strfield(html, "/html"))
                        p = -1; // noter, mais sans titre
                      else if (strfield(html, "body"))
                        p = -1; // noter, mais sans titre
                      else if (html - r->adr >= r->size - 1)
                        p = -1; // noter, mais sans titre
                      else if (html - r->adr >= r->size - 2)     // we got to hurry
                        p = -1; // xxc xxc xxc
                    }
                  } else
                    p = 0;

                  if (p) {      // ok center                            
                    if (makeindex_fp == NULL) {
                      file_notify(opt, "", "",
                                  fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), 
                                          StringBuff(opt->path_html_utf8),
                                          "index.html"), 1, 1, 0);
                      verif_backblue(opt, StringBuff(opt->path_html_utf8));     // générer gif
                      makeindex_fp =
                        filecreate(&opt->state.strc,
                                   fconcat(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), 
                                           StringBuff(opt->path_html_utf8),
                                           "index.html"));
                      if (makeindex_fp != NULL) {

                        // Header
                        hts_template_format(makeindex_fp, template_header,
                                "<!-- Mirror and index made by HTTrack Website Copier/"
                                HTTRACK_VERSION " " HTTRACK_AFF_AUTHORS " -->", /* EOF */ NULL);

                      } else
                        makeindex_done = -1;    // fait, erreur
                    }

                    if (makeindex_fp != NULL) {
                      char BIGSTK tempo[HTS_URLMAXSIZE * 2];
                      char BIGSTK s[HTS_URLMAXSIZE * 2];
                      char *a = NULL;
                      char *b = NULL;

                      s[0] = '\0';
                      if (p > 0) {
                        a = strchr(html, '>');
                        if (a != NULL) {
                          a++;
                          while(is_space(*a))
                            a++;        // sauter espaces & co
                          b = strchr(a, '<');   // prochain tag
                        }
                      }
                      if (lienrelatif(tempo, sizeof(tempo), heap(ptr)->sav,
                                      concat(OPT_GET_BUFF(opt),
                                             OPT_GET_BUFF_SIZE(opt),
                                             StringBuff(opt->path_html_utf8),
                                             "index.html")) == 0) {
                        detect_title = 1;       // ok détecté pour cette page!
                        makeindex_links++;      // un de plus
                        strlcpybuff(makeindex_firstlink, tempo,
                                    HTS_URLMAXSIZE * 2);
                        //

                        /* Hack */
                        if (opt->mimehtml) {
                          strlcpybuff(makeindex_firstlink,
                                      "cid:primary/primary",
                                      HTS_URLMAXSIZE * 2);
                        }

                        if ((b == a) || (a == NULL) || (b == NULL)) {   // pas de titre
                          strcpybuff(s, tempo);
                        } else if ((b - a) < 256) {
                          b--;
                          while(is_space(*b))
                            b--;
                          strncpy(s, a, b - a + 1);
                          *(s + (b - a) + 1) = '\0';
                        }

                        // Decode title with encoding
                        {
                          hts_boolean declared;
                          const char *const charset =
                              page_charset(str, &declared);

                          // Guessed charset: never re-encode valid UTF-8 (#833)
                          if (charset != NULL && !hts_isCharsetUTF8(charset) &&
                              (declared || !hts_isStringUTF8(s, strlen(s)))) {
                            char *sUtf =
                                hts_convertStringToUTF8(s, strlen(s), charset);

                            if (sUtf != NULL) {
                              /* UTF-8 can expand past s[]; truncate to fit */
                              snprintf(s, sizeof(s), "%s", sUtf);
                              freet(sUtf);
                            }
                          }
                        }

                        // Body
                        inplace_escape_uri_utf(tempo, sizeof(tempo));
                        hts_template_format(makeindex_fp, template_body, tempo, s, /* EOF */ NULL);
                      }
                    }
                  }
                }
              }

            } else if (heap(ptr)->depth < opt->depth) {        // on a sauté level1+1 et level1
              hts_finish_makeindex(opt, &makeindex_done, &makeindex_fp,
                                   makeindex_links, makeindex_firstlink,
                                   template_footer, "primary", "primary");
            }
          }                     // if (opt->makeindex)
        }
        // FIN Construction index.html (sommaire)
        /*
           end -- index.html built here
         */

        /* Parse */
        if ((*html == '<')       /* No starting tag */
            &&(!inscript)       /* Not in (java)script */
            &&(!incomment)      /* Not in comment (<!--) */
            &&(!in_media)       /* Not in media */
          ) {
          intag = 1;
          intag_ctype = 0;
          intag_start = html;
          for(intag_name = html + 1; is_realspace(*intag_name); intag_name++) ;
          intag_start_valid = 1;
          codebase[0] = '\0';   // effacer éventuel codebase

          /* Meta ? */
          if (check_tag(intag_start, "meta")) {
            int pos;

            // <meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
            if ((pos = rech_tageq_all(html, "http-equiv"))) {
              const char *token = NULL;
              int len = rech_endtoken(html + pos, &token);

              if (len > 0) {
                if (strfield(token, "content-type")) {
                  intag_ctype = 1;
                } else if (strfield(token, "refresh")) {
                  intag_ctype = 2;
                }
              }
            }
          }

          if (opt->getmode & HTS_GETMODE_HTML) { // sauver html
            p = 0;
            switch (emited_footer) {
            case 0:
              // We are looking for the first head so that we can declare the HTTP-headers charset early
              // Emit as soon as we see the first <head>, <meta>, or <body> tag.
              // FIXME: we currently emit the tag BEFORE the <head> tag, actually, which is not clean
              if ((p = strfield(html, "<head>")) != 0
                  || ((p = strfield(html, "<head")) != 0 && isspace(html[p]))
                  || (p = strfield(html, "<body>")) != 0
                  || ((p = strfield(html, "<body")) != 0 && isspace(html[p]))
                  || ((p = strfield(html, "<meta")) != 0 && isspace(html[p]))
                ) {
                emited_footer++;
              } else {
                p = 0;
              }
              break;
            case 1:
              // And the closing comment info tag
              if ((p = strfield(html, "</html")) != 0) {
                emited_footer++;
              } else {
                p = 0;
              }
              break;
            default:
              p = 0;
              break;
            }

            if (p != 0) {
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
                  // http/https/ftp, so re-add it (bare host is http); for any
                  // other scheme the host keeps its "scheme://" and we add
                  // none.
                  const char *const url_host =
                      jump_identification_const(urladr());
                  const char *const url_scheme =
                      strstr(url_host, "://") != NULL  ? ""
                      : strfield(urladr(), "https://") ? "https://"
                      : strfield(urladr(), "ftp://")   ? "ftp://"
                                                       : "http://";

                  // {addr}/{path} are the escaped host and remote path; {url}
                  // prepends the scheme to them (credentials already stripped
                  // by jump_identification_const, and the scheme is a safe
                  // literal, so no re-escaping is needed).
                  html_inline_safe(url_host, safe_adr, sizeof(safe_adr));
                  html_inline_safe(urlfil(), safe_fil, sizeof(safe_fil));
                  snprintf(safe_url, sizeof(safe_url), "%s%s%s", url_scheme,
                           safe_adr, safe_fil);
                  snprintf(status_str, sizeof(status_str), "%d", r->statuscode);
                  snprintf(size_str, sizeof(size_str), LLintP, (LLint) r->size);
                  time_gmt_rfc822(gmttime);

                  {
                    // Every network-derived string is html_inline_safe()'d: the
                    // footer sits inside an HTML comment, so a value holding
                    // "-->" would otherwise close it and inject markup (#165).
                    // status/size are formatted integers and need no escaping.
                    // Zeroed first: a field added to the enum expands empty
                    // here until this block fills it, never a stale pointer.
                    const char *values[HTS_FOOTER_FIELD_COUNT] = {NULL};

                    values[HTS_FOOTER_ADDR] = safe_adr;
                    values[HTS_FOOTER_PATH] = safe_fil;
                    values[HTS_FOOTER_URL] = safe_url;
                    values[HTS_FOOTER_DATE] = gmttime;
                    values[HTS_FOOTER_LASTMODIFIED] = html_inline_safe(
                        r->lastmodified, safe_lastmod, sizeof(safe_lastmod));
                    values[HTS_FOOTER_VERSION] = HTTRACK_VERSIONID;
                    values[HTS_FOOTER_MIME] = html_inline_safe(
                        r->contenttype, safe_ctype, sizeof(safe_ctype));
                    values[HTS_FOOTER_CHARSET] = html_inline_safe(
                        r->charset, safe_charset, sizeof(safe_charset));
                    values[HTS_FOOTER_STATUS] = status_str;
                    values[HTS_FOOTER_SIZE] = size_str;

                    tempo[0] = '\0';
                    strcatbuff(tempo, eol);
                    // Overflow (<0) leaves tempo unterminated, and the closing
                    // eol is reserved below because strcatbuff aborts rather
                    // than clips: either one would kill the crawl.
                    if (hts_footer_format(
                            tempo + strlen(tempo),
                            sizeof(tempo) - strlen(tempo) - strlen(eol),
                            StringBuff(opt->footer), values) >= 0) {
                      strcatbuff(tempo, eol);
                      HT_ADD(tempo);
                    }
                  }
                }
                // Emit charset ?
                if (emited_footer == 1 && strnotempty(r->charset)) {
                  HT_ADD
                    ("<!-- Added by HTTrack --><meta http-equiv=\"content-type\" content=\"text/html;charset=");
                  HT_ADD(r->charset);
                  HT_ADD("\" /><!-- /Added by HTTrack -->");
                  HT_ADD(eol);
                }
              }
            }
          }
          // éliminer les <!-- (commentaires) : intag dévalidé
          if (*(html + 1) == '!')
            if (*(html + 2) == '-')
              if (*(html + 3) == '-') {
                intag = 0;
                incomment = 1;
                intag_start_valid = 0;
              }

        } else if ((*html == '>')        /* ending tag */
                   &&((!inscript && !in_media) || (inscript_tag))       /* and in tag (or in script) */
          ) {
          if (inscript_tag) {
            inscript_tag = inscript = 0;
            // reset the automaton on exit or its state leaks into plain HTML
            inscript_state_pos = INSCRIPT_START;
            intag = 0;
            incomment = 0;
            intag_start_valid = 0;
            intag_name = NULL;
            if (opt->parsedebug) {
              HT_ADD("<@@ /inscript @@>");
            }
          } else if (!incomment) {
            intag = 0;          //inquote=0;

            // entrée dans du javascript?
            // on parse ICI car il se peut qu'on ait eu a parser les src=..
            // dedans
            if ((intag_start_valid) && (check_tag(intag_start, "script")
                                        || check_tag(intag_start, "style")
                )
              ) {
              const char *a = intag_start;    // <

              // ** while(is_realspace(*(--a)));
              if (*a == '<') {  // sûr que c'est un tag?
                if (check_tag(intag_start, "script")) {
                  inscript_name = "script";
                  incss = 0;
                } else {
                  inscript_name = "style";
                  incss = 1;
                }
                inscript = 1;
                inscript_state_pos = INSCRIPT_START;
                intag = 1;      // because après <script> on y est .. - pas utile
                intag_start_valid = 0;  // OUI car nous sommes dans du code, plus dans du "vrai" tag
                if (opt->parsedebug) {
                  HT_ADD("<@@ inscript @@>");
                }
              }
            }
          } else {              /* end of comment? */
            // vérifier fermeture correcte
            if ((*(html - 1) == '-') && (*(html - 2) == '-')) {
              intag = 0;
              incomment = 0;
              intag_start_valid = 0;
              intag_name = NULL;
            }
#if GT_ENDS_COMMENT
            /* wrong comment ending */
            else {
              /* check if correct ending does not exists
                 <!-- foo > example <!-- bar > is sometimes accepted by browsers
                 when no --> is used somewhere else.. darn those browsers are dirty
               */
              if (!comment_unclosed &&
                  (comment_close == NULL || comment_close < html)) {
                comment_close = strstr(html, "-->");
                if (comment_close == NULL)
                  comment_unclosed = HTS_TRUE;
              }
              if (comment_unclosed) {
                intag = 0;
                incomment = 0;
                intag_start_valid = 0;
                intag_name = NULL;
              }
            }
#endif
          }
        } else if (intag || inscript ||
                   in_media) { // nous sommes dans un tag/commentaire, tester si
                               // on recoit un tag
          int p_type = 0;
          int p_nocatch = 0;
          int p_searchMETAURL = 0;      // chercher ..URL=<url>
          int add_class = 0;    // ajouter .class
          int add_class_dots_to_patch = 0;      // number of '.' in code="x.y.z<realname>"
          const char *p_flush = NULL;

          // ------------------------------------------------------------
          // parsing évolé
          // ------------------------------------------------------------
          if (((isalpha((unsigned char) *html)) || (*html == '/') || (inscript) || (in_media) || (inscriptgen))) {        // sinon pas la peine de tester..

            /* caractère de terminaison pour "miniparsing" javascript=.. ? 
               (ex: <a href="javascript:()" action="foo"> ) */
            if (inscript_tag) {
              if (inscript_tag_lastc) {
                if (*html == inscript_tag_lastc) {
                  /* sortir */
                  inscript_tag = inscript = 0;
                  inscript_state_pos = INSCRIPT_START;
                  incomment = 0;
                  if (opt->parsedebug) {
                    HT_ADD("<@@ /inscript @@>");
                  }
                }
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

            if (in_media) {
              if (strcmp(in_media, "LNK") == 0) {       // real media
                p = 0;
                valid_p = 1;
              } else if (strcmp(in_media, "AAM") == 0) {        // AAM
                if (is_space((unsigned char) html[0])
                    && !is_space((unsigned char) html[1])) {
                  const char *a = html + 1;
                  int n = 0;
                  int ok = 0;
                  int dot = 0;

                  while(n < HTS_URLMAXSIZE / 2 && a[n] != '\0'
                        && (!is_space((unsigned char) a[n]) || !(ok = 1))
                    ) {
                    if (a[n] == '.') {
                      dot = n;
                    }
                    n++;
                  }
                  if (ok && dot > 0) {
                    char BIGSTK tmp[HTS_URLMAXSIZE / 2 + 2];

                    tmp[0] = '\0';
                    strncat(tmp, a + dot + 1, n - dot - 1);
                    if (is_knowntype(opt, tmp) || ishtml_ext(tmp) != -1) {
                      html++;
                      p = 0;
                      valid_p = 1;
                      unquoted_script = 1;
                    }
                  }
                }
              }
            } else if (ptr > 0) {       /* pas première page 0 (primary) */
              p = 0;            // saut pour le nom de fichier: adresse nom fichier=adr+p

              // ------------------------------
              // détection d'écriture JavaScript.
              // osons les obj.write et les obj.href=.. ! osons!
              // note: inscript==1 donc on sautera après les \"
              if (inscript) {
                if (inscriptgen) {      // on est déja dans un objet générant..
                  if (*html == scriptgen_q) {    // fermeture des " ou '
                    if (*(html - 1) != '\\') {   // non
                      inscriptgen = 0;  // ok parsing terminé
                    }
                  }
                } else {
                  const char *a = NULL;
                  char check_this_fking_line = 0;       // parsing code javascript..
                  char must_be_terminated = 0;  // caractère obligatoire de terminaison!
                  int token_size;

                  if (!(token_size = strfield(html, ".writeln")))        // détection ...objet.write[ln]("code html")...
                    token_size = strfield(html, ".write");
                  if (token_size) {
                    a = html + token_size;
                    while(is_realspace(*a))
                      a++;      // sauter espaces
                    if (*a == '(') {    // début parenthèse
                      check_this_fking_line = 2;        // à parser!
                      must_be_terminated = ')';
                      a++;      // sauter (
                    }
                  }
                  // on a un truc du genre instruction"code généré" dont on parse le code
                  if (check_this_fking_line) {
                    while(is_realspace(*a))
                      a++;
                    if ((*a == '\'') || (*a == '"')) {  // départ de '' ou ""
                      const char *b;

                      scriptgen_q = *a; // quote
                      b = a + 1;        // départ de la chaîne
                      // vérifier forme ("code") et pas ("code"+var), ingérable
                      do {
                        if (*a == scriptgen_q && *(a - 1) != '\\')      // quote non slash
                          break;        // sortie
                        else if (*a == 10 && *(a - 1) != '\\'   /* LF and no continue (\) character */
                                 && (*(a - 1) != '\r' || *(a - 2) != '\\'))     /* and not CRLF and no .. */
                          break;
                        else
                          a++;  // caractère suivant
                      } while((a - b) < HTS_URLMAXSIZE / 2);
                      if (*a == scriptgen_q) {  // fin du quote
                        a++;
                        while(is_realspace(*a))
                          a++;
                        if (*a == must_be_terminated) { // parenthèse fermante: ("..")

                          // HTML generated inside javascript inside HTML: parse
                          // it with the standard rules, from the quote on.
                          inscriptgen = 1;
                          html = b;

                          if ((opt->debug > 1) && (opt->log != NULL)) {
                            char str[64];

                            hts_log_print(
                                opt, LOG_DEBUG,
                                "active code (%s) detected in javascript: %s",
                                "parse",
                                text_excerpt(opt, str, sizeof(str), b,
                                             (size_t) (a - b + 1), 32));
                          }
                        }

                      }

                    }

                  }
                }
              }
              // fin detection code générant javascript vers html
              // ------------------------------

              // analyse proprement dite, A HREF=.. etc..
              if (!p) {
                // si dans un tag, et pas dans un script - sauf si on analyse un obj.write("..
                if ((intag && (!inscript)) || inscriptgen) {
                  if ((*(html - 1) == '<') || (is_space(*(html - 1)))) {  // <tag < tag etc
                    // <A HREF=.. pour les liens HTML
                    p = rech_tageq(html, "href");
                    if (p) {    // href.. tester si c'est une bas href!
                      if ((intag_start_valid) && check_tag(intag_start, "base")) {      // oui!
                        // ** note: base href et codebase ne font pas bon ménage..
                        p_type = 2;     // c'est un chemin
                      }
                    }

                    /* The other attributes that may hold a link */
                    const hts_attr *const at =
                        p == 0 ? hts_attr_match(html, !inscript, &p) : NULL;
                    const hts_boolean is_script_attr =
                        at != NULL && (at->kind == HTS_ATTR_EVENT ||
                                       at->kind == HTS_ATTR_STYLE);

                    if (at != NULL) {
                      switch (at->kind) {
                      case HTS_ATTR_LINK:
                        dataattr_p = strncmp(at->name, "data-", 5) == 0;
                        /* A temporary hack to avoid archive=foo.jar,bar.jar */
                        if (strcmp(at->name, "archive") == 0) {
                          archivetag_p = 1;
                        }
                        /* srcset: a comma-list of candidate URLs, each split
                           out and rewritten below (#235, #236) */
                        else if (strcmp(at->name, "srcset") == 0 ||
                                 strcmp(at->name, "data-srcset") == 0) {
                          srcset_p = 1;
                        }
                        break;
                      case HTS_ATTR_REFRESH:
                        // <meta http-equiv="Refresh" content="3;URL=a.html">
                        if (intag_ctype == 2) {
                          p_searchMETAURL = 1;
                        } else {
                          p = 0; /* cancel */
                        }
                        break;
                      case HTS_ATTR_LEAVE:
                        p_nocatch = 1; /* rewrite the link, never capture it */
                        break;
                      default:
                        break;
                      }
                    }

                    /* Events, but not inside document.write */
                    if ((p == 0 || is_script_attr) && !inscript) {
                      /* not in the table: any other onXxx= */
                      if (p == 0) {
                        if ((*html == 'o') && (*(html + 1) == 'n')
                            && isUpperLetter(*(html + 2))) {
                          p = 0;
                          while ((p < 64) && isalpha((unsigned char) html[p]))
                            p++;
                          if (p < 64) {
                            while(is_space(html[p]))
                              p++;
                            if (html[p] == '=')
                              p++;
                            else
                              p = 0;
                          } else
                            p = 0;
                        }
                      }
                      /* OK, événement repéré */
                      if (p) {
                        inscript_tag_lastc = *(html + p);        /* à attendre à la fin */
                        html += p /*+ 1*/;   /* saut */
                        /*
                           On est désormais dans du code javascript
                         */
                        inscript_name = "";
                        incss = at != NULL && at->kind == HTS_ATTR_STYLE;
                        inscript = inscript_tag = 1;
                        inscript_state_pos = INSCRIPT_START;
                        if (opt->parsedebug) {
                          HT_ADD("<@@ inscript @@>");
                        }
                      }
                      p = 0;    /* quoi qu'il arrive, ne rien démarrer ici */
                    }
                    // <APPLET CODE=.. pour les applet java.. [CODEBASE (chemin..) à faire]
                    if (p == 0) {
                      p = rech_tageq(html, "code");
                      if (p) {
                        if ((intag_start_valid) && check_tag(intag_start, "applet")) {  // dans un <applet !
                          p_type =
                              -1; // the bare file name, written before codebase
                          add_class = 1; // add .class if missing

                          // A codebase= after code= is swapped before it.
                          {
                            /* the swap needs '>' within this many bytes of
                             * code= */
                            const size_t window = 1000;
                            const char *a;

                            a = html;
                            while ((size_t) (a - html) < window && *a &&
                                   *a != '>' && !rech_tageq(a, "codebase"))
                              a++;
                            if (rech_tageq(a, "codebase")) {
                              const char *const b =
                                  tag_close_within(a, window - (a - html));

                              if (b != NULL) {
                                char BIGSTK tempo[HTS_URLMAXSIZE * 2];
                                const size_t offset = html - r->adr;
                                char *const modify = &r->adr[offset];
                                assertf(modify == html);

                                tempo[0] = '\0';
                                strncatbuff(tempo, a, b - a);
                                strcatbuff(tempo, " ");
                                strncatbuff(tempo, html, a - html - 1);
                                // pad with spaces to the tag's exact length
                                while (strlen(tempo) < (size_t) (b - html))
                                  strcatbuff(tempo, " ");
                                // no error?
                                if (strlen(tempo) == (size_t) (b - html)) {
                                  // overwrites the tag in place, so no NUL
                                  memcpy(modify, tempo, strlen(tempo));
                                  fetch_tag.name = NULL; /* the tag changed */
                                  p = 0; // the link moved, so drop it
                                  p_type = 0;
                                  add_class = 0;
                                }
                              }
                            }
                          }

                        }
                      }
                    }
                    // liens à patcher mais pas à charger (ex: codebase)
                    if (p == 0) {       // note: si non chargé (ex: ignorer .class) patché tout de même
                      p = rech_tageq(html, "codebase");
                      if (p) {
                        if ((intag_start_valid) && check_tag(intag_start, "applet")) {  // dans un <applet !
                          p_type = -2;
                        } else
                          p = -1;       // ne plus chercher
                      }
                    }

                    // Meta tags pour robots
                    if (p == 0) {
                      if (opt->robots) {
                        if ((intag_start_valid)
                            && check_tag(intag_start, "meta")) {
                          if (rech_tageq(html, "name")) {        // name=robots.txt
                            char tempo[1100];
                            const char *a;

                            tempo[0] = '\0';
                            a = tag_close_within(html, 999);
                            if (a) {
                              if (a - html < 999) {
                                strncatbuff(tempo, html, a - html);
                                if (strstrcase(tempo, "content")) {
                                  if (strstrcase(tempo, "robots")) {
                                    if (strstrcase(tempo, "nofollow")) {
                                      nofollow = 1;     // NE PLUS suivre liens dans cette page
                                      hts_log_print(opt, LOG_WARNING,
                                                    "Link %s%s not scanned "
                                                    "(follow robots meta tag)",
                                                    adr_nologin(opt, urladr()),
                                                    urlfil());
                                    }
                                  }
                                }
                              }
                            }
                          }
                        }
                      }
                    }
                    // entrée dans une applet javascript
                    /*if (!inscript) {  // sinon on est dans un obj.write("..
                       if (p==0)
                       if (rech_sampletag(html,"script"))
                       if (check_tag(intag_start,"script")) {
                       inscript=1;
                       }
                       } */

                    // Ici on procède à une analyse du code javascript pour tenter de récupérer
                    // certains fichiers évidents.
                    // C'est devenu obligatoire vu le nombre de pages qui intègrent
                    // des images réactives par exemple
                  }
                } else if (inscript) {

                  /* Parse */
                  assertf(inscript_name != NULL);
                  if (*html == '/'
                      &&
                      ((strfield(html, "/script")
                        && strfield(inscript_name, "script"))
                       || (strfield(html, "/style")
                           && strfield(inscript_name, "style"))
                      )
                      && inscript_locked == 0) {
                    const char *a = html;

                    while(is_realspace(*a))
                      a--;
                    a--;
                    if (*a == '<') {    // sûr que c'est un tag?
                      inscript = 0;
                      inscript_state_pos = INSCRIPT_START;
                      if (opt->parsedebug) {
                        HT_ADD("<@@ /inscript @@>");
                      }
                    }
                  } else if (inscript_state_pos == INSCRIPT_START) {
                    /* Script analyzing: foo="url", foo("url"), foo(url),
                       foo "url" */
                    hts_js_link jslink;

                    if (hts_js_scan_link(opt, html, r->adr, inscript_tag,
                                         inscript_tag_lastc, incss, &jslink)) {
                      if ((opt->debug > 1) && (opt->log != NULL)) {
                        char str[64];

                        hts_log_print(opt, LOG_DEBUG,
                                      "link detected in javascript: %s",
                                      text_excerpt(opt, str, sizeof(str),
                                                   html + jslink.offset,
                                                   (size_t) jslink.length, 32));
                      }
                      p = jslink.offset; // p non nul: TRAITER CHAINE COMME
                                         // FICHIER
                      module_base = jslink.is_module;
                      escape_parens = jslink.escape_parens;
                      entity_end = jslink.entity_quote;
                      if (jslink.unquoted_end != '\0')
                        ending_p = jslink.unquoted_end;
                    }
                  }
                }
              }

            } else {            // ptr == 0
              p = 0;            // No stupid tag anymore, raw link
              valid_p = 1;      // Valid even if p==0
              while((html[p] == '\r') || (html[p] == '\n'))
                p++;
              ending_p = '\r';
            }

          } else if (isspace((unsigned char) *html)) {
            intag_startattr = html + 1;  // attribute in tag (for dirty parsing)
          }

          // ------------------------------------------------------------
          // dernier recours - parsing "sale" : détection systématique des .gif, etc.
          // risque: générer de faux fichiers parazites
          // fix: ne parse plus dans les commentaires
          // ------------------------------------------------------------
          if (opt->parseall && (opt->parsejava & HTSPARSE_NO_AGGRESSIVE) == 0 &&
              (ptr > 0) && (!in_media)) { // option parsing "brut"
            if (!is_realspace(*html)) {
              int noparse = 0;

              /* ensure automate state  0 (not in comments, quotes..) */
              if (inscript
                  && (inscript_state_pos != INSCRIPT_INQUOTE
                      && inscript_state_pos != INSCRIPT_INQUOTE2)) {
                noparse = 1;
              }

              /* vérifier que l'on est pas dans un <!-- --> pur */
              if ((!intag) && (incomment) && (!inscript))
                noparse = 1;    /* commentaire */

              // recherche d'URLs
              if (!noparse) {
                if (!p) {       // non déja trouvé
                  if (html != r->adr) {  // >1 caractère
                    // scanner les chaines
                    if ((*html == '\"') || (*html == '\'')) {     // "xx.gif" 'xx.gif'
                      if (strchr("=(,", parseall_lastc)) {      // exemple: a="img.gif.. (handles comments)
                        const char *a = html;
                        char stop = *html;       // " ou '
                        int count = 0;

                        // sauter caractères
                        a++;
                        // copier
                        while((*a) && (*a != '\'') && (*a != '\"')
                              && (count < HTS_URLMAXSIZE)) {
                          count++;
                          a++;
                        }

                        // ok chaine terminée par " ou '
                        if ((*a == stop) && (count < HTS_URLMAXSIZE)
                            && (count > 0)) {
                          char c;

                          a++;
                          while(is_taborspace(*a))
                            a++;
                          c = *a;
                          // in-tag, an attribute value ends at its quote: no
                          // delimiter required after it (mid-tag attrs, #201)
                          if (strchr("),;>/+\r\n", c) ||
                              (intag && !inscript && intag_start_valid &&
                               hts_dirty_attr_detectable(html, intag_start))) {
                            // '/' covers a value followed by a JS comment
                            int url_ok = hts_dirty_link_is_url(
                                opt, html + 1, (size_t) count, c, inscript);

                            if (url_ok) {

                              if (intag && intag_start_valid && intag_start) {
                                const char *nend;
                                const char *const name = hts_dirty_attr_name(
                                    html, intag_start, &nend);
                                const char *refused = NULL;

                                // a quote nested in a value has no name, so
                                // also ask the attribute holding it
                                if (!inscript && !incomment) {
                                  if (name != NULL)
                                    refused =
                                        hts_dirty_attr_refused(name, nend);
                                  if (refused == NULL &&
                                      intag_startattr != NULL)
                                    refused = hts_dirty_attr_refused(
                                        intag_startattr,
                                        intag_startattr +
                                            strcspn(intag_startattr,
                                                    "=" HTS_SPACES));
                                }
                                if (refused != NULL) {
                                  url_ok = 0;
                                  hts_log_print(opt, LOG_DEBUG,
                                                "dirty parsing: %s attribute "
                                                "avoided",
                                                refused);
                                } else if (name != NULL &&
                                           hts_attr_is_js_binding(name, nend)) {
                                  url_ok = 0;
                                  hts_log_print(opt, LOG_DEBUG,
                                                "dirty parsing: binding "
                                                "attribute avoided");
                                }
                              }

                              // accept it, as an ordinary link
                              if (url_ok) {
                                valid_p = 1;
                                p = 0;
                                module_base =
                                    inscript &&
                                    hts_js_quote_is_import_arg(html, r->adr);
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                } // p == 0

              } // not in comment

              // plus dans un commentaire
              if (inscript_state_pos == INSCRIPT_START
                  && inscript_state_pos_prev == INSCRIPT_START) {
                parseall_lastc = *html;  // caractère avant le prochain
              }

            }                   // if realspace
          } // if parseall

          // ------------------------------------------------------------
          // p!=0 : on a repéré un éventuel lien
          // ------------------------------------------------------------
          //
          if ((p > 0) || (valid_p)) { // on a repéré un lien
            const char *eadr = NULL;  /* fin de l'URL */

            int ok = 1;
            hts_boolean not_url = HTS_FALSE;
            char quote = '\0';
            int quoteinscript = 0;
            int noquote = 0;
            const char *tag_attr_start = html;
            /* Kept because the value scan below clears intag on an unquoted
               value ending at '>', and --single-file must still know the tag.
               inscript_locked marks a whole CSS or JS file, the one context
               that legitimately has no tag. */
            const char *const sf_tag = intag_start_valid ? intag_name : NULL;
            const int sf_tagless_body = inscript_locked;
            hts_fetch_meta link_fetch = {0};

            /* The cap stops discovery here, so the rest of the page is
               written external rather than pointing at files nothing fetches.
             */
            if (hts_maxlinks_no_room(opt)) {
              p_nocatch = 1;
              maxlinks_give_up(opt, stre->exit_xh_);
            }
            // si nofollow ou un stop a été déclenché, réécrire tous les liens en externe
            /* an update keeps following, so the cache keeps its data */
            if ((nofollow) ||
                (hts_load_acquire_int(&opt->state.stop) && !opt->is_update))
              p_nocatch = 1;

            // écrire codebase avant, flusher avant code
            if ((p_type == -1) || (p_type == -2)) {
              HT_add_adr;       // refresh
              lastsaved = html; // one past the last byte written
            }
            // sauter espaces
            hts_automate_increment(&saut, p);
            while((is_space(*html)
                   || (inscriptgen && html[0] == '\\' && is_space(html[1])
                   )
                  )
                  && quote == '\0') {
              if (!quote)
                if ((*html == '\"') || (*html == '\'')) {
                  quote = *html; // on doit attendre cela à la fin
                  if (inscriptgen && *(html - 1) == '\\') {
                    quoteinscript = 1;  /* will wait for \" */
                  }
                }
              // puis quitter
              // html++;    // sauter les espaces, "" et cie
              hts_automate_increment(&saut, 1);
            }

            /* Stop at \n (LF) if primary links or link lists */
            if (ptr == 0 || (in_media && strcmp(in_media, "LNK") == 0))
              quote = '\n';
            /* s'arrêter que ce soit un ' ou un " : pour document.write('<img src="foo'+a); par exemple! */
            else if (inscript && !unquoted_script)
              noquote = 1;

            // sauter éventuel \" ou \' javascript
            if (inscript) {     // on est dans un obj.write("..
              if (*html == '\\') {
                if ((*(html + 1) == '\'') || (*(html + 1) == '"')) { // \" ou \'
                  hts_automate_increment(&saut, 2);
                }
              }
            }
            // sauter content="1;URL=http://..
            if (p_searchMETAURL) {
              int l = 0;

              while((html + l + 4 < r->adr + r->size)
                    && (!strfield(html + l, "URL="))
                    && (l < 128))
                l++;
              if (!strfield(html + l, "URL="))
                ok = -1;
              else
                html += (l + 4);
            }

            /* éviter les javascript:document.location=.. : les parser, plutôt */
            if (ok != -1) {
              if (strfield(html, "javascript:")
                  && !inscript  /* we don't want to parse 'javascript:' inside document.write inside scripts */
                ) {
                ok = -1;
                /*
                   On est désormais dans du code javascript
                 */
                inscript_name = "";
                /* A javascript: URL is JavaScript, whatever an earlier
                   <style> left in incss. */
                incss = 0;
                inscript_tag = inscript = 1;
                inscript_state_pos = INSCRIPT_START;
                inscript_tag_lastc = quote;     /* à attendre à la fin */
                if (opt->parsedebug) {
                  HT_ADD("<@@ inscript @@>");
                }
              }
            }

            if (p_type == 1) {
              if (*html == '#') {
                html++;          // sauter # pour usemap etc
              }
            }
 srcset_next:
            /* srcset: skip leading whitespace/commas before each candidate;
               the skipped bytes flush verbatim below */
            if (srcset_p) {
              while(html < r->adr + r->size
                    && (is_realspace(*html) || *html == ','))
                hts_automate_increment(&saut, 1);
            }
            eadr = html;

            // ne pas flusher après code si on doit écrire le codebase avant!
            if ((p_type != -1) && (p_type != 2) && (p_type != -2)) {
              HT_add_adr;       // refresh
              lastsaved = html; // one past the last byte written
              // après on écrira soit les données initiales,
              // soir une URL/lien modifié!
            } else if (p_type == -1)
              p_flush = html;    // flusher jusqu'à adr ensuite

            if (ok != -1) {     // continuer
              // découper le lien
              do {
                if (*eadr == '\0') // end of the parsed buffer
                  ok = 0;
                if (eadr - html > HTS_URLMAXSIZE)    // ** trop long, >HTS_URLMAXSIZE caractères (on prévoit HTS_URLMAXSIZE autres pour path)
                  ok = -1;      // ne pas traiter ce lien

                if (ok > 0) {
                  if (is_space(*eadr)) {        // guillemets,CR, etc
                    if ((*eadr == quote && (!quoteinscript || *(eadr - 1) == '\\'))     // end quote
                        || (noquote && (*eadr == '\"' || *eadr == '\''))        // end at any quote
                        || (!noquote && quote == '\0' && is_realspace(*eadr))   // unquoted href
                        || srcset_p     // whitespace ends a srcset candidate URL
                      )         // si pas d'attente de quote spéciale ou si quote atteinte
                      ok = 0;
                  } else if (ending_p && (*eadr == ending_p))
                    ok = 0;
                  else if (entity_end != '\0' && *eadr == '&' &&
                           html_entity_quote(eadr, NULL) == entity_end)
                    ok = 0;
                  else {
                    switch (*eadr) {
                    case '>':
                      if (!quote) {
                        if (!inscript && !in_media) {
                          intag = 0;    // PLUS dans un tag!
                          intag_start_valid = 0;
                          intag_name = NULL;
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
                      if (inscript)
                        ok = 0;
                      break;    // \" ou \' point d'arrêt
                    case '?':  /*quote_adr=adr; */
                      break;    // noter position query
                    case ',':
                      if (srcset_p) {
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
              } while(ok == 1);

              // Empty link detected
              if (eadr - html <= 1) {        // link empty
                ok = -1;        // No
                if (*html != '#') {      // Not empty+unique #
                  if (eadr - html == 1) {    // 1=link empty with delim (end_adr-start_adr)
                    if (quote) {
                      HT_ADD("#"); // We add this for a <href="">
                    }
                  }
                }
              }
              // This is a dirty and horrible hack to avoid parsing an Adobe GoLive bogus tag
              if (strfield(html, "(Empty Reference!)")) {
                ok = -1;        // No
              }
              // a lazy-load placeholder or template is no URL: leave it as is
              if (ok == 0 && dataattr_p &&
                  data_attr_is_not_url(html, (size_t) (eadr - html - 1))) {
                const size_t len = (size_t) (eadr - html - 1);
                char *shown = malloct(len + 8);

                if (shown != NULL)
                  hts_log_print(
                      opt, LOG_DEBUG, "data attribute is no URL: %s",
                      text_excerpt(opt, shown, len + 8, html, len, len + 7));
                freet(shown);
                not_url = HTS_TRUE;
              }
            }

            if (ok == 0 && !not_url) { // test a link
              char BIGSTK lien[HTS_URLMAXSIZE * 2];
              int meme_adresse = 0;     // 0 par défaut pour primary

              // construire lien (découpage)
              if (eadr - html - 1 < HTS_URLMAXSIZE) {        // pas trop long?
                strncpy(lien, html, eadr - html - 1);
                lien[eadr - html - 1] = '\0';
                while (hts_striplastchar(lien, ' ')) {
                }

              } else
                lien[0] = '\0'; // erreur

              // ------------------------------------------------------
              // Lien repéré et extrait
              if (strnotempty(lien) > 0) {      // construction du lien
                lien_adrfilsave afs;
                int forbidden_url = -1; // lien non interdit (mais non autorisé..)
                int just_test_it = 0;   // mode de test des liens
                int set_prio_to = 0;    // pour capture de page isolée
                int import_done = 0;    // lien importé (ne pas scanner ensuite *à priori*)

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
                if (!RUN_CALLBACK1(opt, linkdetected, lien)
                    || !RUN_CALLBACK2(opt, linkdetected2, lien, intag_start)) {
                  error = 1;    // erreur
                  hts_log_print(opt, LOG_ERROR,
                                "Link %s refused by external wrapper",
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
                  while(*a) {
                    if (!is_retorsep(*a)) {
                      lien[llen++] = *a;
                    }
                    a++;
                  }
                  lien[llen] = '\0';
                }

                // commas are forbidden
                if (archivetag_p) {
                  if (strchr(lien, ',')) {
                    error = 1;  // erreur
                    hts_log_print(opt, LOG_DEBUG,
                                  "link rejected (multiple-archive) %s",
                                  url_nologin(opt, lien));
                  }
                }

                /* Unescape/escape %20 and other &nbsp; */
                {
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
                  strcpybuff(lien, 
                    unescape_http_unharm(catbuff, sizeof(catbuff), lien, 1 | 2));     /* note: '%' is still escaped */

                  // Percent-encode the control bytes as a browser does (#982);
                  // a byte grows to three, so a link that outgrows the buffer
                  // is dropped rather than clipped to a URL nobody wrote.
                  {
                    char BIGSTK tempo[sizeof(lien)];

                    if (escape_control_url(lien, tempo, sizeof(tempo)) <
                        sizeof(tempo) - HTS_LINK_TAIL_ROOM) {
                      strcpybuff(lien, tempo);
                    } else {
                      error = 1;
                      hts_log_print(
                          opt, LOG_DEBUG,
                          "link rejected (control bytes do not fit) %s",
                          url_nologin(opt, lien));
                    }
                  }

                  // charset conversion for the URI filename (not the query
                  // string), unless the bytes already are valid UTF-8:
                  // converting those would double-encode them (#180)
                  if (hasCharset && !hts_isCharsetUTF8(charset) &&
                      !hts_isStringUTF8(lien, strlen(lien))) {
                    char *s =
                        hts_convertStringToUTF8(lien, strlen(lien), charset);
                    if (s != NULL) {
                      hts_log_print(opt, LOG_DEBUG,
                                    "engine: save-name: '%s' charset "
                                    "conversion from '%s' to '%s'",
                                    charset, url_nologin(opt, lien),
                                    url_nologin(opt, s));
                      /* One byte can become three, and a clipped URL names
                         another resource, so drop the link instead. */
                      if (strlen(s) < sizeof(lien) - HTS_LINK_TAIL_ROOM) {
                        strcpybuff(lien, s);
                      } else {
                        error = 1;
                        hts_log_print(
                            opt, LOG_WARNING,
                            "Link is too long after charset conversion: %s",
                            url_nologin(opt, lien));
                      }
                      freet(s);
                    }
                  }

                  // decode URI entities with UTF-8 charset
                  if (hts_unescapeEntities(lien, lien, strlen(lien) + 1) != 0) {
                    hts_log_print(opt, LOG_WARNING,
                                  "could not decode URI '%s' with charset '%s'",
                                  url_nologin(opt, lien), charset);
                  }

                  // decode query entities with the page charset, out-of-place
                  // because the escape grows it (#854)
                  if (hasCharset) {
                    char BIGSTK decoded[sizeof(query)];

                    if (hts_unescapeEntitiesWithCharsetSpecial(
                            query, decoded, sizeof(decoded), charset,
                            UNESCAPE_ENTITIES_URL_QUERY) == 0) {
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
                  if (hts_unescapeUrlSpecial(lien, catbuff, sizeof(catbuff),
                                             UNESCAPE_URL_NO_ASCII) == 0) {
                    strcpybuff(lien, catbuff);
                  } else {
                    hts_log_print(opt, LOG_WARNING,
                                  "could not URL-decode string '%s'",
                                  url_nologin(opt, lien));
                  }

                  // we need to encode query string non-ascii chars,
                  // leaving the encoding as-is (unlike the file part)
                  // and copy back query
                  {
                    const size_t cap = sizeof(lien) - HTS_LINK_TAIL_ROOM;
                    const size_t used = strlen(lien);

                    // the append grows the query too, and clips silently on
                    // overflow: drop, or we fetch a query nobody wrote (#982)
                    if (used >= cap || append_escape_check_url(
                                           query, lien, cap) >= cap - used) {
                      error = 1;
                      hts_log_print(opt, LOG_DEBUG,
                                    "link rejected (query does not fit) %s",
                                    url_nologin(opt, lien));
                    }
                  }
                }

                // convertir les éventuels \ en des / pour éviter des problèmes de reconnaissance!
                {
                  char *a;

                  for(a = jump_identification(lien); *a != '\0' && *a != '?';
                      a++) {
                    if (*a == '\\') {
                      *a = '/';
                    }
                  }
                }

                // supprimer le(s) ./
                while((lien[0] == '.') && (lien[1] == '/')) {
                  char BIGSTK tempo[HTS_URLMAXSIZE * 2];

                  strcpybuff(tempo, lien + /* ./ */ 2);
                  strcpybuff(lien, tempo);
                }
                if (strnotempty(lien) == 0)     // sauf si plus de nom de fichier
                  strcpybuff(lien, "./");

                // APPLET CODE="mixer.MixerApplet.class" --> APPLET CODE="mixer/MixerApplet.class"
                // yes, this is dirty
                // but I'm so lazzy..
                // and besides the java "code" convention is really a pain in html code
                if (p_type == -1) {
                  char *a = strrchr(lien, '.');

                  add_class_dots_to_patch = 0;
                  if (a) {
                    char *b;

                    do {
                      b = strchr(lien, '.');
                      if ((b != a) && (b)) {
                        add_class_dots_to_patch++;
                        *b = '/';
                      }
                    } while((b != a) && (b));
                  }
                }
                // drop a default :80 port from the authority
                hts_strip_default_port(lien, sizeof(lien));
                // vérifier que l'on ne doit pas ajouter de .class
                if (!error) {
                  if (add_class) {
                    char *a = hts_lastcharptr(lien);

                    while((a > lien) && (*a != '/') && (*a != '.'))
                      a--;
                    if (*a != '.')
                      strcatbuff(lien, ".class");       // ajouter .class
                    else if (!strfield2(a, ".class"))
                      strcatbuff(lien, ".class");       // idem
                  }
                }
                // si c'est un chemin, alors vérifier (toto/toto.html -> http://www/toto/)
                if (!error) {
                  hts_log_print(opt, LOG_DEBUG, "position link check %s",
                                url_nologin(opt, lien));

                  if ((p_type == 2) || (p_type == -2)) {        // code ou codebase                        
                    // Vérifier les codebase=applet (au lieu de applet/)
                    if (p_type == -2) { // codebase
                      if (strnotempty(lien)) {
                        if (hts_lastchar(lien) != '/') { // pas répertoire
                          strcatbuff(lien, "/");
                        }
                      }
                    }

                    /* A base with no authority is relative per RFC 3986 5.2,
                       except the hostname shape legacy markup writes. */
                    if (p_type == 2 && !link_has_authority(lien) &&
                        link_base_is_hostname(lien)) {
                      char BIGSTK tmp[HTS_URLMAXSIZE * 2];

                      strcpybuff(tmp, "http://");
                      strcatbuff(tmp, lien);
                      strcpybuff(lien, tmp);
                    }

                    /* only one ending / (bug on some pages) */
                    if (strlen(lien) > 2) {
                      size_t len = strlen(lien);

                      while(len > 1 && lien[len - 1] == '/' && lien[len - 2] == '/')    /* double // (bug) */
                        lien[--len] = '\0';
                    }
                    // copier nom host si besoin est
                    if (!link_has_authority(lien)) {    // pas de http://
                      lien_adrfil af2;   // ** euh ident_url_relatif??

                      if (ident_url_relatif(lien, urladr(), urlfil(), &af2) < 0) {
                        error = 1;
                      } else if (strlen(af2.adr) + strlen(af2.fil) >=
                                 sizeof(lien) - HTS_LINK_TAIL_ROOM -
                                     sizeof("http:///")) {
                        /* the rebuild below is unbounded in the two of them */
                        error = 1;
                        hts_log_print(opt, LOG_WARNING,
                                      "Link is too long once resolved: %s",
                                      url_nologin(opt, lien));
                      } else {
                        strcpybuff(lien, "http://");
                        strcatbuff(lien, af2.adr);
                        if (*af2.fil != '/')
                          strcatbuff(lien, "/");
                        strcatbuff(lien, af2.fil);
                        {
                          char *a;

                          a = hts_lastcharptr(lien);
                          while((*a) && (*a != '/') && (a > lien))
                            a--;
                          if (*a == '/') {
                            *(a + 1) = '\0';
                          }
                        }
                      }
                    }

                    if (!error) {       // pas d'erreur?
                      if (p_type == 2) { // code ET PAS codebase
                        char *a = hts_lastcharptr(lien);
                        char *start_of_filename = jump_identification(lien);

                        if (start_of_filename != NULL
                            && (start_of_filename =
                                strchr(start_of_filename, '/')) != NULL)
                          start_of_filename++;
                        if (start_of_filename == NULL)
                          strcatbuff(lien, "/");
                        while((a > lien) && (*a) && (*a != '/'))
                          a--;
                        if (*a == '/') {        // ok on a repéré le dernier /
                          if (start_of_filename != NULL
                              && a + 1 >= start_of_filename) {
                            *(a + 1) = '\0';    // couper
                          }
                        } else {
                          *lien = '\0'; // éliminer
                          error = 1;    // erreur, ne pas poursuivre
                        }
                      }
                      // stocker base ou codebase?
                      switch (p_type) {
                      case 2: {
                        strlcpybuff(base, lien, HTS_URLMAXSIZE * 2);
                      } break; // base
                      case -2: {
                        strlcpybuff(codebase, lien, HTS_URLMAXSIZE * 2);
                      } break; // base
                      }

                      hts_log_print(
                          opt, LOG_DEBUG, "code/codebase link %s base %s",
                          url_nologin(opt, lien), url_nologin(opt, base));
                    }

                  } else {
                    char *_base;

                    if (p_type == -1)   // code (applet)
                      _base = codebase;
                    else
                      _base = base;

                    // ajouter chemin de base href..
                    if (strnotempty(_base)) {   // considérer base
                      if (!link_has_authority(lien)) {  // non absolue
                        if (*lien != '/') {     // non absolu sur le site (/)
                          if ((strlen(_base) + strlen(lien)) < HTS_URLMAXSIZE) {
                            // mailto: and co: do NOT add base
                            if (ident_url_relatif
                                (lien, urladr(), urlfil(), &afs.af) >= 0) {
                              char BIGSTK tempo[HTS_URLMAXSIZE * 2];

                              // base est absolue
                              strcpybuff(tempo, _base);
                              strcatbuff(tempo,
                                         lien + ((*lien == '/') ? 1 : 0));
                              strcpybuff(lien, tempo);  // patcher en considérant base
                              // ** vérifier que ../ fonctionne (ne doit pas arriver mais bon..)

                              hts_log_print(
                                  opt, LOG_DEBUG,
                                  "link modified with code/codebase %s",
                                  url_nologin(opt, lien));
                            }
                          } else {
                            error = 1;  // erreur
                            hts_log_print(opt, LOG_ERROR,
                                          "Link %s too long with base href",
                                          url_nologin(opt, lien));
                          }
                        } else {
                          lien_adrfil baseaf;
                          if (ident_url_absolute(_base, &baseaf) >= 0) {
                            if ((strlen(baseaf.adr) + strlen(lien)) < HTS_URLMAXSIZE) {
                              char BIGSTK tempo[HTS_URLMAXSIZE * 2];

                              // base est absolue
                              tempo[0] = '\0';
                              if (!link_has_authority(baseaf.adr)) {
                                strcatbuff(tempo, "http://");
                              }
                              strcatbuff(tempo, baseaf.adr);
                              strcatbuff(tempo, lien);
                              strcpybuff(lien, tempo);  // patcher en considérant base

                              hts_log_print(
                                  opt, LOG_DEBUG,
                                  "link modified with code/codebase %s",
                                  url_nologin(opt, lien));
                            } else {
                              error = 1;        // erreur
                              hts_log_print(opt, LOG_ERROR,
                                            "Link %s too long with base href",
                                            url_nologin(opt, lien));
                            }
                          }
                        }
                      }
                    }

                  }
                }
                // transformer lien quelconque (http, relatif, etc) en une adresse
                // et un chemin+fichier (adr,fil)
                if (!error) {
                  int reponse;

                  hts_log_print(
                      opt, LOG_DEBUG, "build relative link %s with %s%s",
                      url_nologin(opt, lien),
                      adr_nologin(opt, relativeurladr()), relativeurlfil());
                  if ((reponse =
                       ident_url_relatif(lien, relativeurladr(), relativeurlfil(),
                                         &afs.af)) < 0) {
                    afs.af.adr[0] = '\0';      // erreur
                    if (reponse == -2) {
                      hts_log_print(opt, LOG_WARNING,
                                    "Link %s not caught (protocol not "
                                    "supported by this build)",
                                    url_nologin(opt, lien));
                    } else {
                      hts_log_print(opt, LOG_DEBUG,
                                    "ident_url_relatif failed for %s with %s%s",
                                    url_nologin(opt, lien),
                                    adr_nologin(opt, relativeurladr()),
                                    relativeurlfil());
                    }
                  } else {
                    hts_log_print(opt, LOG_DEBUG,
                                  "built relative link %s with %s%s -> %s%s",
                                  url_nologin(opt, lien),
                                  adr_nologin(opt, relativeurladr()),
                                  relativeurlfil(),
                                  adr_nologin(opt, afs.af.adr), afs.af.fil);
                  }
                } else {
                  hts_log_print(opt, LOG_DEBUG,
                                "link %s not build, error detected before",
                                url_nologin(opt, lien));
                  afs.af.adr[0] = '\0';
                }

                // Le lien doit juste être réécrit, mais ne doit pas générer un lien
                // exemple: <FORM ACTION="url_cgi">
                if (p_nocatch) {
                  forbidden_url = 1;    // interdire récupération du lien
                  hts_log_print(opt, LOG_DEBUG, "link forced external at %s%s",
                                adr_nologin(opt, afs.af.adr), afs.af.fil);
                }
                // Tester si un lien doit être accepté ou refusé (wizard)
                // forbidden_url=1 : lien refusé
                // forbidden_url=0 : lien accepté
                if ((p_type != 2) && (p_type != -2)) {  // tester autorisations?
                  if (!p_nocatch) {
                    if (afs.af.adr[0] != '\0') {
                      hts_log_print(opt, LOG_DEBUG,
                                    "wizard link test at %s%s..",
                                    adr_nologin(opt, afs.af.adr), afs.af.fil);
                      forbidden_url =
                          hts_acceptlink(opt, ptr, afs.af.adr, afs.af.fil,
                                         intag_name ? intag_name : NULL,
                                         intag_name ? tag_attr_start : NULL,
                                         css_file, &set_prio_to, &just_test_it);
                      hts_log_print(opt, LOG_DEBUG,
                                    "result for wizard link test: %d",
                                    forbidden_url);
                    }
                  }
                }
                // calculer meme_adresse
                meme_adresse =
                  strfield2(jump_identification_const(afs.af.adr),
                            jump_identification_const(urladr()));

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
                  // moved en cascade (impossible à reproduire à priori en local, lorsque des fichiers
                  // gif sont impliqués par exemple)
                  if ((p_type != 2) && (p_type != -2)) {        // pas base href ou codebase
                    if (forbidden_url != 1) {
                      char BIGSTK last_adr[HTS_URLMAXSIZE * 2];

                      /* Calc */
                      last_adr[0] = '\0';
                      strcpybuff(last_adr, afs.af.adr); // ancienne adresse
                      r_sv =
                        url_savename(&afs, &former, heap(ptr)->adr, heap(ptr)->fil, opt,
                                     sback, cache, hash, ptr,
                                     numero_passe, NULL);
                      if (strcmp(jump_identification_const(last_adr),
                        jump_identification_const(afs.af.adr)) != 0) {       // a changé

                        // 2e test si moved

                        // Tester si un lien doit être accepté ou refusé (wizard)
                        // forbidden_url=1 : lien refusé
                        // forbidden_url=0 : lien accepté
                        if ((ptr > 0) && (p_type != 2) && (p_type != -2)) {     // tester autorisations?
                          if (!p_nocatch) {
                            if (afs.af.adr[0] != '\0') {
                              hts_log_print(
                                  opt, LOG_DEBUG,
                                  "wizard moved link retest at %s%s..",
                                  adr_nologin(opt, afs.af.adr), afs.af.fil);
                              forbidden_url = hts_acceptlink(
                                  opt, ptr, afs.af.adr, afs.af.fil,
                                  intag_name ? intag_name : NULL,
                                  intag_name ? tag_attr_start : NULL, css_file,
                                  &set_prio_to, &just_test_it);
                              hts_log_print(opt, LOG_DEBUG,
                                            "result for wizard moved link retest: %d",
                                            forbidden_url);
                            }
                          }
                        }
                        meme_adresse = 0;       // on a changé
                      }
                    } else {
                      strcpybuff(afs.save, "");     // dummy
                    }
                  }
                  /* A <base target> sends every link with no target to a
                     frame we cannot tell. */
                  if (link_frame < 0)
                    link_frame =
                        hts_fetch_has_base_target(r->adr)
                            ? HTS_FETCH_DEST_UNKNOWN
                            : hts_fetch_same_frame(heap(ptr)->fetch.dest);
                  parse_fetch_meta(
                      opt, ptr, &link_fetch, HTTP_IS_REDIRECT(r->statuscode),
                      p_searchMETAURL, in_media != NULL || p_type != 0,
                      inscript, incss, sf_tagless_body, link_frame, &fetch_tag,
                      sf_tag, tag_attr_start, afs.af.fil);
                  /* Naming followed redirects whose hops it does not keep. */
                  if (former.adr[0] != '\0')
                    link_fetch.dest = HTS_FETCH_DEST_UNKNOWN;
                  // resolve unresolved type
                  if (r_sv != -1 && p_type != 2 && p_type != -2
                      && forbidden_url == 0 && IS_DELAYED_EXT(afs.save)
                    ) {
                    time_t t;

                    // pas d'erreur, on continue
                    r_sv = hts_wait_delayed(str, &afs, heap(ptr)->adr,
                                            heap(ptr)->fil, &former,
                                            &forbidden_url, &link_fetch);

                    /* User interaction, because hts_wait_delayed can be slow.. (3.43) */
                    t = time(NULL);
                    if (user_interact_timestamp == 0
                        || t - user_interact_timestamp > 0) {
                      user_interact_timestamp = t;
                      ENGINE_SAVE_CONTEXT();
                      {
                        hts_mirror_process_user_interaction(str, stre);
                      }
                      ENGINE_SET_CONTEXT();
                    }
                  }
                  // record!
                  if (r_sv != -1) {     // pas d'erreur, on continue
                    /* log */
                    if ((opt->debug > 1) && (opt->log != NULL)) {
                      if (forbidden_url != 1) { // le lien va être chargé
                        if ((p_type == 2) || (p_type == -2)) {  // base href ou codebase, pas un lien
                          hts_log_print(opt, LOG_DEBUG, "Code/Codebase: %s%s",
                                        adr_nologin(opt, afs.af.adr),
                                        afs.af.fil);
                        } else if ((opt->getmode & HTS_GETMODE_HTML_FIRST) ==
                                   0) {
                          hts_log_print(opt, LOG_DEBUG, "Record: %s%s -> %s",
                                        adr_nologin(opt, afs.af.adr),
                                        afs.af.fil, afs.save);
                        } else {
                          if (!ishtml(opt, afs.af.fil))
                            hts_log_print(opt, LOG_DEBUG,
                                          "Record after: %s%s -> %s",
                                          adr_nologin(opt, afs.af.adr),
                                          afs.af.fil, afs.save);
                          else
                            hts_log_print(opt, LOG_DEBUG, "Record: %s%s -> %s",
                                          adr_nologin(opt, afs.af.adr),
                                          afs.af.fil, afs.save);
                        }
                      } else
                        hts_log_print(opt, LOG_DEBUG, "External: %s%s",
                                      adr_nologin(opt, afs.af.adr), afs.af.fil);
                    }
                    /* FIN log */

                    // écrire lien
                    if ((p_type == 2) || (p_type == -2)) {      // base href ou codebase, sauter
                      /* An emptied base href leaves <base >, which means "no
                         base" and stops a framework that requires one (Angular
                         throws). An empty href keeps the document itself as the
                         base, so every reference resolves as it did. */
                      if (p_type == 2)
                        HT_ADD("href=\"\"");
                      lastsaved = eadr - 1 + 1; // sauter "
                    }
                    /* */
                    else if (opt->urlmode == HTS_URLMODE_ABSOLUTE) {
                      // -K strips a scheme word from an adr with no "//"
                      sink_add_absolute_url(&sink, afs.af.adr, afs.af.fil,
                                            opt->passprivacy, HTS_FALSE);
                      lastsaved = eadr - 1; // the caller's ++ skips the quote
                      /* */
                    } else if (opt->urlmode == HTS_URLMODE_KEEP_ORIGINAL) {
                      /* */
                      /* leave the link 'as is' */
                      /* Sinon, dépend de interne/externe */
                    } else if (forbidden_url ==
                               1) { // le lien ne sera pas chargé, référence
                                    // externe!
                      /* Not only output: also writes external.html or .gif. */
                      if (sink.enabled) {
                        if (p_type != -1) { // a link, not a java class name
                          if (!opt->external) {
                            sink_add_absolute_url(&sink, afs.af.adr, afs.af.fil,
                                                  opt->passprivacy, HTS_TRUE);
                          } else { // external page: link to an error page
                            const char *cat_name = NULL;
                            const char *cat_data = NULL;
                            int cat_nb = 0;
                            int cat_data_len = 0;

                            // ajouter lien external
                            switch ((link_has_authority(afs.af.adr))
                                        ? 1
                                        : ((hts_lastchar(afs.af.fil) == '/')
                                               ? 1
                                               : (ishtml(opt, afs.af.fil)))) {
                            case 1:
                            case -2:   // html ou répertoire
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
                              if (get_httptype_sized(opt, mime, sizeof(mime),
                                                     afs.af.fil, 0) &&
                                  strfield(mime, "image/")) {
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
                            if (lienrelatif(tempo, sizeof(tempo), save,
                                            relativesavename()) == 0) {
                              /* Never escape high-chars (we don't know the
                               * encoding!!) */
                              inplace_escape_uri_utf(
                                  tempo, sizeof(tempo)); // escape with %xx
                              HT_ADD_HTMLESCAPED(tempo); // page externe
                              HT_ADD("?link=");          // page externe

                              sink_add_absolute_url(&sink, afs.af.adr,
                                                    afs.af.fil,
                                                    opt->passprivacy, HTS_TRUE);
                            }
                            // écrire fichier?
                            if (verif_external(opt, cat_nb, 1)) {
                              FILE *fp = filecreate(
                                  &opt->state.strc,
                                  fconcat(OPT_GET_BUFF(opt),
                                          OPT_GET_BUFF_SIZE(opt),
                                          StringBuff(opt->path_html_utf8),
                                          cat_name));
                              if (fp) {
                                if (cat_data_len == 0) { // texte
                                  verif_backblue(
                                      opt, StringBuff(opt->path_html_utf8));
                                  fprintf(fp, "%s%s",
                                          "<!-- Created by HTTrack Website "
                                          "Copier/" HTTRACK_VERSION
                                          " " HTTRACK_AFF_AUTHORS " -->" LF,
                                          cat_data);
                                } else { // data
                                  (void) hts_fwrite_exact(
                                      cat_data, (size_t) cat_data_len, fp);
                                }
                                fclose(fp);
                                usercommand(
                                    opt, 0, NULL,
                                    fconcat(OPT_GET_BUFF(opt),
                                            OPT_GET_BUFF_SIZE(opt),
                                            StringBuff(opt->path_html_utf8),
                                            cat_name),
                                    "", "");
                              }
                            }
                          } // external
                        } else { // a bare java class name
                          // as below: copy codebase and base
                          if (p_flush) {
                            char BIGSTK tempo[HTS_URLMAXSIZE * 2];      // <-- ajouté
                            char BIGSTK tempo_pat[HTS_URLMAXSIZE * 2];

                            // Calculer chemin
                            tempo_pat[0] = '\0';
                            strcpybuff(tempo, afs.af.fil);     // <-- ajouté
                            {
                              char *a = strrchr(tempo, '/');

                              // Example: we converted code="x.y.z.foo.class" into "x/y/z/foo.class"
                              // we have to do the contrary now
                              if (add_class_dots_to_patch > 0) {
                                while((add_class_dots_to_patch > 0) && (a)) {
                                  *a = '.';     // convert "false" java / into .
                                  add_class_dots_to_patch--;
                                  a = strrchr(tempo, '/');
                                }
                                // if add_class_dots_to_patch, this is because there is a problem!!
                                if (add_class_dots_to_patch) {
                                  hts_log_print(opt, LOG_WARNING,
                                                "Error: can not rewind java path %s, check html code",
                                                tempo);
                                }
                              }
                              // Cut path/filename
                              if (a) {
                                char BIGSTK tempo2[HTS_URLMAXSIZE * 2];

                                strcpybuff(tempo2, a + 1);      // FICHIER
                                strncatbuff(tempo_pat, tempo, (a - tempo) + 1);   // chemin
                                strcpybuff(tempo, tempo2);      // fichier
                              }
                            }

                            // write codebase="path"
                            if (strnotempty(tempo_pat)) {
                              HT_ADD("codebase=\"");
                              sink_add_absolute_url(&sink, afs.af.adr,
                                                    tempo_pat, opt->passprivacy,
                                                    HTS_TRUE);
                              HT_ADD("\" ");
                            }

                            /* The page picks this span's width, so it goes
                               to the growable output, not a fixed buffer. */
                            if (p_flush > lastsaved) {
                              HT_ADD_BUF(lastsaved,
                                         (size_t) (p_flush - lastsaved));
                            }
                            HT_ADD_URL_HTMLESCAPED(tempo);
                          }
                        }
                      }
                      lastsaved = eadr - 1;
                    } else if (opt->mimehtml) {
                      char BIGSTK cid[HTS_URLMAXSIZE * 3];

                      HT_ADD("cid:");
                      make_content_id(afs.af.adr, afs.af.fil, cid, sizeof(cid));
                      HT_ADD_HTMLESCAPED(cid);
                      lastsaved = eadr - 1;     // dernier écrit+1 (enfin euh apres on fait un ++ alors hein)
                    } else if (opt->urlmode == HTS_URLMODE_ABSOLUTE_URI) {
                      HT_ADD_URL_HTMLESCAPED(afs.af.fil);
                      lastsaved =
                          eadr - 1; // resume copying at the closing quote
                    } else if (opt->urlmode == HTS_URLMODE_TRANSPARENT_PROXY) {
                      char BIGSTK tempo[HTS_URLMAXSIZE * 2];
                      const char *uri;
                      int i;
                      char *pos;

                      /* Not only output: also fills fixed buffers below. */
                      if (sink.enabled) {
                        if (!link_has_authority(afs.af.adr)) {
                          HT_ADD("http://");
                        } else {
                          char *aut = strstr(afs.af.adr, "//");

                          if (aut) {
                            char tmp[256];

                            tmp[0] = '\0';
                            strncatbuff(tmp, afs.af.adr, (aut - afs.af.adr));   // scheme
                            HT_ADD(tmp);        // Protocol
                            HT_ADD("//");
                          }
                        }

                        // filename is taken as URI (ex: "C:\My Website\www.example.com\foo4242.html)
                        uri = afs.save;

                        // .. after stripping the path prefix (ex: "www.example.com\foo4242.html)
                        if (strnotempty(StringBuff(opt->path_html_utf8))) {
                          uri += StringLength(opt->path_html_utf8);
                          for(; uri[0] == '/' || uri[0] == '\\'; uri++) ;
                        }
                        // and replacing all \ by / (ex: "www.example.com/foo4242.html)
                        strcpybuff(tempo, uri);
                        for(i = 0; tempo[i] != '\0'; i++) {
                          if (tempo[i] == '\\') {
                            tempo[i] = '/';
                          }
                        }

                        // put original query string if any (ex: "www.example.com/foo4242.html?q=45)
                        pos = strchr(afs.af.fil, '?');
                        if (pos != NULL) {
                          strcatbuff(tempo, pos);
                        }
                        // write it
                        HT_ADD_URL_HTMLESCAPED(tempo);
                      }
                      lastsaved = eadr - 1;     // dernier écrit+1 (enfin euh apres on fait un ++ alors hein)
                    } else if (opt->urlmode == HTS_URLMODE_RELATIVE) {
                      char BIGSTK tempo[HTS_URLMAXSIZE * 2];

                      tempo[0] = '\0';
                      // calculer le lien relatif

                      if (lienrelatif(tempo, sizeof(tempo), afs.save,
                                      relativesavename()) == 0) {
                        if (!in_media) {        // In media (such as real audio): don't patch
                          /* Never escape high-chars (we don't know the encoding!!) */
                          inplace_escape_uri_utf(tempo, sizeof(tempo));

                          // CSS url(): keep parens escaped
                          if (escape_parens)
                            escape_url_parens(tempo, sizeof(tempo));
                        }
                        hts_log_print(
                            opt, LOG_DEBUG,
                            "relative link at %s build with %s and %s: %s",
                            adr_nologin(opt, afs.af.adr), afs.save,
                            relativesavename(), tempo);

                        // lien applet (code) - il faut placer un codebase avant
                        if (p_type == -1) {     // que le nom de fichier

                          if (p_flush) {
                            char BIGSTK tempo_pat[HTS_URLMAXSIZE * 2];

                            tempo_pat[0] = '\0';
                            {
                              char *a = strrchr(tempo, '/');

                              // Example: we converted code="x.y.z.foo.class" into "x/y/z/foo.class"
                              // we have to do the contrary now
                              if (add_class_dots_to_patch > 0) {
                                while((add_class_dots_to_patch > 0) && (a)) {
                                  *a = '.';     // convert "false" java / into .
                                  add_class_dots_to_patch--;
                                  a = strrchr(tempo, '/');
                                }
                                // if add_class_dots_to_patch, this is because there is a problem!!
                                if (add_class_dots_to_patch) {
                                  hts_log_print(opt, LOG_WARNING,
                                                "Error: can not rewind java path %s, check html code",
                                                tempo);
                                }
                              }

                              if (a) {
                                char BIGSTK tempo2[HTS_URLMAXSIZE * 2];

                                strcpybuff(tempo2, a + 1);
                                strncatbuff(tempo_pat, tempo, a - tempo + 1);   // chemin
                                strcpybuff(tempo, tempo2);      // fichier
                              }
                            }

                            // write codebase="path"
                            if (strnotempty(tempo_pat)) {
                              HT_ADD("codebase=\"");
                              HT_ADD_HTMLESCAPED(tempo_pat);
                              HT_ADD("\" ");
                            }

                            /* The page picks this span's width, so it goes
                               to the growable output, not a fixed buffer. */
                            if (p_flush > lastsaved) {
                              HT_ADD_BUF(lastsaved,
                                         (size_t) (p_flush - lastsaved));
                            }
                          }
                        }

                        /* Not only output: also tags --single-file links. */
                        if (sink.enabled) {

                          /* --single-file: tag the reference for the
                             end-of-mirror pass. A fragment, so a mirror left
                             marked by an interrupted run still browses. */
                          /* A NULL tag reads as "a CSS or JS body", so a tag
                             we cannot name must not reach the classifier. */
                          const char sf_class =
                              (opt->single_file && sf_may_mark && !in_media &&
                               p_type == 0 && !p_searchMETAURL &&
                               (sf_tag != NULL || sf_tagless_body))
                                  ? singlefile_ref_class(sf_tag, tag_attr_start,
                                                         r->adr)
                                  : 0;
                          const size_t sf_start = TypedArraySize(sink.buf);

                          // écrire le lien modifié, relatif
                          // Note: escape all chars, even >127 (no UTF)
                          HT_ADD_HTMLESCAPED_FULL(tempo);

                          // Add query-string, for informational purpose only
                          // Useless, because all parameters-pages are saved into different targets
                          if (opt->includequery) {
                            char *a = strchr(lien, '?');

                            if (a) {
                              HT_ADD_HTMLESCAPED(a);
                            }
                          }

                          /* Measured on the appended bytes, not tempo: the
                             escape can change the length. The query sits inside
                             the span so inlining drops it. */
                          if (sf_class != 0) {
                            char sf_mark[SINGLEFILE_MARK_MAX];

                            /* SINGLEFILE_MAX_SPAN is derived from these two
                               buffers, so a resize must move it. */
                            HTS_STATIC_ASSERT(
                                sizeof(tempo) * HTS_HTMLESCAPE_FULL_MAXEXP +
                                        sizeof(lien) * HTS_HTMLESCAPE_MAXEXP <=
                                    SINGLEFILE_MAX_SPAN,
                                sf_span_fits);

                            HT_ADD(singlefile_mark(
                                opt, sf_mark, sizeof(sf_mark), sf_class,
                                TypedArraySize(sink.buf) - sf_start));
                          }
                        }
                        lastsaved = eadr - 1;   // dernier écrit+1 (enfin euh apres on fait un ++ alors hein)
                      } else {
                        hts_log_print(opt, LOG_WARNING,
                                      "Error building relative link %s and %s",
                                      afs.save, relativesavename());
                      }
                    } // sinon le lien sera écrit normalement

                    /* Security check */
                    if (strlen(afs.save) >= HTS_URLMAXSIZE) {
                      afs.af.adr[0] = '\0';
                      hts_log_print(opt, LOG_WARNING, "Link is too long: %s",
                                    afs.save);
                    }

                    if ((afs.af.adr[0] != '\0') && (p_type != 2) &&
                        (p_type != -2) &&
                        (forbidden_url !=
                         1)) { // if the file does not exist, add it to the list
                      /* The gate above misses a cap that fills mid-page,
                         which the queued hts_addurl() records in
                         hts_mirror_process_user_interaction() can do after it.
                         Skipping keeps room for the two records below. Only a
                         front end queues those, so no .test reaches this. */
                      if (hts_maxlinks_no_room(opt)) {
                        maxlinks_give_up(opt, stre->exit_xh_);
                      } else { // room for it: record the link
                        int pass_fix, dejafait = 0;

                        // Calculer la priorité de ce lien
                        if ((opt->getmode & HTS_GETMODE_HTML_FIRST) == 0) {
                          pass_fix = 0;
                        } else { // vérifier que ce n'est pas un !html
                          if (!ishtml(opt, afs.af.fil))
                            pass_fix = 1;       // priorité inférieure (traiter après)
                          else
                            pass_fix = max(0, numero_passe);    // priorité normale
                        }

                        /* If the file seems to be an html file, get depth-1 */

                        // vérifier que le lien n'a pas déja été noté
                        // si c'est le cas, alors il faut s'assurer que la priorité associée
                        // au fichier est la plus grande des deux priorités
                        //
                        // On part de la fin et on essaye de se presser (économise temps machine)
                        {
                          int i = hash_read(hash, afs.save, NULL, 0);   // lecture type 0 (sav)

                          if (i >= 0) {
                            if ((opt->debug > 1) && (opt->log != NULL)) {
                              if (strcmp(afs.af.adr, heap(i)->adr) != 0
                                  || strcmp(afs.af.fil, heap(i)->fil) != 0) {
                                hts_log_print(
                                    opt, LOG_DEBUG,
                                    "merging similar links %s%s and %s%s",
                                    adr_nologin(opt, afs.af.adr), afs.af.fil,
                                    adr_nologin(opt, heap(i)->adr),
                                    heap(i)->fil);
                              }
                            }
                            heap(i)->depth =
                              maximum(heap(i)->depth, heap(ptr)->depth - 1);
                            dejafait = 1;
                          }
                        }

                        // le lien n'a jamais été créé.
                        // cette fois ci, on le crée!
                        if (!dejafait) {
                          //
                          // >>>> CREER LE LIEN <<<<
                          //
                          // enregistrer lien à charger

                          // DEBUT ROBOTS.TXT AJOUT
                          if (!just_test_it) {
                            if ((!strfield(afs.af.adr, "ftp://"))      // non ftp
                                && (!strfield(afs.af.adr, "file://"))
                              ) {       // non file
                              if (opt->robots) {        // récupérer robots
                                if (ishtml(opt, afs.af.fil) != 0) {    // pas la peine pour des fichiers isolés
                                  if (checkrobots(_ROBOTS, afs.af.adr, "") != -1) {    // robots.txt ?
                                    checkrobots_set(_ROBOTS, afs.af.adr, "");  // ajouter entrée vide
                                    if (checkrobots(_ROBOTS, afs.af.adr, "") == -1) {  // robots.txt ?
                                      // enregistrer robots.txt (MACRO)
                                      if (!hts_record_link(opt, afs.af.adr,
                                                           "/robots.txt", "",
                                                           "", "", NULL)) {
                                        TypedArrayFree(sink.buf);
                                        return -1;
                                      }
                                      heap_top()->testmode = 0;    // pas mode test
                                      heap_top()->link_import = 0; // pas mode import     
                                      heap_top()->premier = heap_top_index();
                                      heap_top()->precedent = ptr;
                                      heap_top()->depth = 0;
                                      heap_top()->pass2 = max(0, numero_passe);
                                      heap_top()->retry = 0;
                                      hts_log_print(
                                          opt, LOG_DEBUG,
                                          "robots.txt added at %s",
                                          adr_nologin(opt, afs.af.adr));
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
                          // FIN ROBOTS.TXT AJOUT

                          // enregistrer
                          if (!hts_record_link(opt, afs.af.adr, afs.af.fil,
                                               afs.save, former.adr, former.fil,
                                               codebase)) {
                            TypedArrayFree(sink.buf);
                            return -1;
                          }
                          heap_top()->fetch = link_fetch;
                          // mode test?
                          if (!just_test_it)
                            heap_top()->testmode = 0;      // pas mode test
                          else
                            heap_top()->testmode = 1;      // mode test
                          if (!import_done)
                            heap_top()->link_import = 0;   // pas mode import
                          else
                            heap_top()->link_import = 1;   // mode import
                          // écrire autres paramètres de la structure-lien
                          if ((meme_adresse) && (!import_done)
                              && (heap(ptr)->premier != 0))
                            heap_top()->premier = heap(ptr)->premier;
                          else  // sinon l'objet père est le précédent lui même
                            heap_top()->premier = heap_top_index();

                          heap_top()->precedent = ptr;
                          // noter la priorité
                          if (!set_prio_to)
                            heap_top()->depth = heap(ptr)->depth - 1;
                          else
                            heap_top()->depth = max(0, min(heap(ptr)->depth - 1, set_prio_to - 1));       // PRIORITE NULLE (catch page)
                          // noter pass
                          heap_top()->pass2 = pass_fix;
                          heap_top()->retry = opt->retry;

                          if (!just_test_it) {
                            hts_log_print(opt, LOG_DEBUG,
                                          "OK, NOTE: %s%s -> %s",
                                          adr_nologin(opt, heap_top()->adr),
                                          heap_top()->fil, heap_top()->sav);
                          } else {
                            hts_log_print(opt, LOG_DEBUG, "OK, TEST: %s%s",
                                          adr_nologin(opt, heap_top()->adr),
                                          heap_top()->fil);
                          }

                        } else {        // if !dejafait
                          hts_log_print(opt, LOG_DEBUG,
                                        "link has already been recorded, cancelled: %s",
                                        afs.save);

                        }

                      } // room for it
                    } // si adr[0]!='\0'

                  }             // if adr[0]!='\0' 

                }               // if adr[0]!='\0'

              }                 // if strlen(lien)>0

            } // if ok==0

            assertf(eadr - html >= 0);   // Should not go back
            if (eadr > html) {
              hts_automate_increment(&saut, (int) (eadr - 1 - html));
            }

            /* srcset candidate loop: skip the descriptor and comma, then
               re-enter the capture for the next URL. Backward goto, not a loop:
               the per-candidate body is this whole block. */
            if (srcset_p && ok == 0) {
              const char *const endp = r->adr + r->size;
              const char *q = html;
              /* whitespace ends an unquoted value, so its descriptor too */
              const hts_boolean unquoted = !noquote && quote == '\0';

              while (q < endp && *q != '\0' && *q != ',' && *q != quote &&
                     *q != '<' && *q != '>' && (unsigned char) *q >= 32 &&
                     !(unquoted && is_realspace(*q)))
                q++;            // skip the descriptor
              if (q < endp && *q == ',') {
                q++;
                while(q < endp && (is_realspace(*q) || *q == ','))
                  q++;          // skip whitespace and empty candidates
                if (q < endp && *q != '\0' && *q != ',' && *q != quote
                    && *q != '<' && *q != '>' && (unsigned char) *q >= 32) {
                  hts_automate_increment(
                      &saut, (int) (q - html)); // keep the automate in sync
                  ok = 1;
                  not_url = HTS_FALSE;
                  goto srcset_next;
                }
              }
            }

            /* We skipped bytes and skip the " : reset state */

 } // if (p)

        } // si '<' ou '>'

        // plus loin
        html++;                  // automate will be checked next loop

        /* Otimization: if we are scanning in HTML data (not in tag or script), 
           then jump to the next starting tag */
        if (ptr > 0) {
          if ((!intag)          /* Not in tag */
              &&(!inscript)     /* Not in (java)script */
              &&(!in_media)     /* Not in media */
              &&(!incomment)    /* Not in comment (<!--) */
              &&(!inscript_tag) /* Not in tag with script inside */
            ) {
            /* Not at the end */
            if (html - r->adr < r->size) {
              /* Not on a starting tag yet */
              if (*html != '<') {
                /* strchr does not well behave with null chrs.. */
                /* char* adr_next = strchr(adr,'<'); */
                const char *adr_next = html;

                while(*adr_next != '<' && (adr_next - r->adr) < r->size) {
                  adr_next++;
                }
                /* Jump to near end (index hack) */
                if (*adr_next != '<') {
                  if (html - r->adr < r->size - 4
                      && r->size > 4
                    ) {
                    html = r->adr + r->size - 2;
                  }
                } else {
                  html = adr_next;
                }
              }
            }
          }
        }
        // ----------
        // flush what was scanned
        HT_add_adr;
        lastsaved = html; // one past the last byte written
        // ----------

        /* The rewritten page outgrew what we can allocate, so end the parse
           here and fail the page rather than the crawl. */
        if (sink.oom) {
          html = r->adr + r->size;
        }

        // Checks
        if (back_add_stats != opt->state.back_add_stats) {
          back_add_stats = opt->state.back_add_stats;

          // Check max time
          if (!back_checkmirror(opt)) {
            html = r->adr + r->size;
          }
        }
        // pour les stats du shell si parsing trop long
        if (r->size)
          opt->state._hts_in_html_done =
              (int) ((100 * (LLint) (html - r->adr)) / (LLint) r->size);
        if (opt->state._hts_in_html_poll) {
          opt->state._hts_in_html_poll = 0;
          // temps à attendre, et remplir autant que l'on peut le cache (backing)
          back_wait(sback, opt, cache, HTS_STAT.stat_timestart);
          back_fillmax(sback, opt, cache, ptr, numero_passe);

          if (!hts_loop_tick(sback, opt, 0, ptr)) {
            hts_log_print(opt, LOG_ERROR, "Exit requested by shell or user");
            hts_store_release_int(stre->exit_xh_, 1); // exit requested
            TypedArrayFree(sink.buf);
            return -1;
          } else if (opt->state._hts_cancel == 1) {
            nofollow = 1;       // moins violent
            opt->state._hts_cancel = 0;
          }
        }
        // refresh the backing system each 2 seconds
        if (engine_stats()) {
          back_wait(sback, opt, cache, HTS_STAT.stat_timestart);
          back_fillmax(sback, opt, cache, ptr, numero_passe);
        }
      } while(html - r->adr < r->size);

      opt->state._hts_in_html_parsing = 0;      // flag
      opt->state._hts_cancel = 0;       // pas de cancel

      if (sink.enabled) {
        if (!sink.oom) {
          char *cAddr = TypedArrayElts(sink.buf);
          int cSize = (int) TypedArraySize(sink.buf);

          hts_log_print(opt, LOG_DEBUG, "engine: postprocess-html: %s%s",
                        adr_nologin(opt, urladr()), urlfil());
          if (RUN_CALLBACK4(opt, postprocess, &cAddr, &cSize, urladr(), urlfil()) == 1) {
            hts_log_print(opt, LOG_DEBUG,
              "engine: postprocess-html: callback modified data, applying %d bytes", cSize);
            /* A reply pointing anywhere into sink.buf edited it in
               place and is moved down. Appending it onto itself instead would
               memcpy overlapping bytes, after an EnsureRoom whose realloc can
               relocate the block out from under cAddr. */
            if (!hts_postprocess_reply_ok(
                    cAddr, cSize, TypedArrayElts(sink.buf),
                    TypedArraySize(sink.buf), TypedArrayCapa(sink.buf))) {
              hts_log_print(opt, LOG_ERROR,
                            "engine: postprocess-html: callback returned %d "
                            "bytes for %s%s, page skipped",
                            cSize, adr_nologin(opt, urladr()), urlfil());
              HT_SET_SIZE(0);
              error = 1;
            } else if (hts_postprocess_reply_inplace(
                           cAddr, TypedArrayElts(sink.buf),
                           TypedArrayCapa(sink.buf))) {
              memmove(TypedArrayElts(sink.buf), cAddr, (size_t) cSize);
              HT_SET_SIZE((size_t) cSize);
            } else {
              HT_SET_SIZE(0);
              HT_ADD_N(cAddr, (size_t) cSize);
            }
          }
        }

        /* Flush and save to disk */
        if (sink.oom) {
          hts_log_print(opt, LOG_ERROR,
                        "Not enough memory to rewrite %s%s, page skipped",
                        adr_nologin(opt, urladr()), urlfil());
          error = 1;
        } else if (TypedArraySize(sink.buf) != 0) {
          hts_finish_html_file(opt, cache, r, &fp, TypedArrayElts(sink.buf),
                               TypedArraySize(sink.buf), urladr(), urlfil(),
                               savename());
        }
      }
      /* outside the write: mimehtml appends whatever the output mode is */
      TypedArrayFree(sink.buf);
      //
      //
      //
    }                           // if !error

  }                             // analyse OK

  /* Apply changes */
  ENGINE_SAVE_CONTEXT();

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

  if (strcasecmp(jump_identification_const(moved_adr),
                 jump_identification_const(cur_adr)) != 0)
    return HTS_FALSE;
  fil_normalized_filtered_ex(moved_fil, n_fil, NULL, norm_slash, norm_query);
  fil_normalized_filtered_ex(cur_fil, pn_fil, NULL, norm_slash, norm_query);
  return strcasecmp(n_fil, pn_fil) == 0;
}
