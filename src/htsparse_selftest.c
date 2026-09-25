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
/* File: htsparse_selftest.c subroutines:                       */
/*       in-process self-tests for the html/javascript parser   */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* The parser decides three things about a quoted string, and each decision is
   swept here over a generated corpus rather than a list of past incidents. */

#define HTS_INTERNAL_BYTECODE

#include "htsparse_selftest.h"

#include "htsselftest_int.h"

/* This models the rules hts_dirty_link_is_url answers, restated from the
   parser's intent rather than its code, so a disagreement is a finding about
   either. It reuses link_dir_has_fragment_or_query, unescape_amp and the mime
   tables, which are data or separately pinned by -#test=linkdir. */
static hts_boolean dirtylink_model(httrackp *opt, const char *str, char lastc,
                                   hts_boolean inscript) {
  char BIGSTK cut[HTS_URLMAXSIZE * 2];
  char type[256];
  hts_boolean opens_path, by_ext;
  size_t i, slashes = 0, named = 0, len;

  if (strlen(str) >= HTS_URLMAXSIZE)
    return HTS_FALSE;
  /* a space reads as prose, except in script code */
  if (strchr(str, ' ') != NULL && !inscript)
    return HTS_FALSE;
  /* asked on the source bytes, before an entity can forge a marker */
  opens_path = link_dir_has_fragment_or_query(str);

  strlcpybuff(cut, str, sizeof(cut));
  unescape_amp(cut);
  cut[strcspn(cut, "#?")] = '\0';
  len = strlen(cut);

  if (len == 0)
    return HTS_FALSE;
  if (strpbrk(cut, "*<>,\"'") != NULL)
    return HTS_FALSE;
  if (cut[0] == '.' && isalnum((unsigned char) cut[1])) /* ".gif" */
    return HTS_FALSE;

  by_ext = (get_httptype_sized(opt, type, sizeof(type), cut, 0) ||
            is_dyntype(get_ext(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), cut)))
               ? HTS_TRUE
               : HTS_FALSE;

  /* a string the source continues with '+' is an expression, so only an
     extension speaks for it */
  if (lastc != '+') {
    if (strfield(cut, "http:") || strfield(cut, "ftp:")
#if HTS_USEOPENSSL
        || strfield(cut, "https:")
#endif
    )
      return HTS_TRUE;
    for (i = 0; i < len; i++) {
      if (cut[i] == '/')
        slashes++;
      else
        named++;
    }
    if (cut[len - 1] == '/' && inscript &&
        (opens_path || (slashes >= 2 && named != 0)))
      return HTS_TRUE;
  }
  /* An address is not a link, but only where the extension is the whole case
     for it: "http://a@b/" and "//a@b/" were already taken above. */
  return (by_ext && strchr(cut, '@') == NULL) ? HTS_TRUE : HTS_FALSE;
}

typedef struct selftest_sweep {
  size_t cases;
  size_t bad;
  size_t accepted;
  hts_boolean dump;
} selftest_sweep;

/* One string against both, in each context the parser can offer. */
static void dirtylink_case(httrackp *opt, selftest_sweep *sw, const char *str) {
  static const char lastcs[] = {')', '+'};
  size_t i, j;

  for (i = 0; i < sizeof(lastcs); i++) {
    for (j = 0; j < 2; j++) {
      const hts_boolean inscript = j != 0 ? HTS_TRUE : HTS_FALSE;
      const hts_boolean got =
          hts_dirty_link_is_url(opt, str, strlen(str), lastcs[i], inscript);

      sw->cases++;
      if (got)
        sw->accepted++;
      if (sw->dump) {
        printf("%d\t%c\t%d\t%s\n", (int) got, lastcs[i], (int) inscript, str);
        continue;
      }
      if (got != dirtylink_model(opt, str, lastcs[i], inscript)) {
        if (sw->bad < 20) {
          fprintf(stderr,
                  "dirtylink \"%s\" (next byte '%c', %s): engine says %d, "
                  "model says %d\n",
                  str, lastcs[i], inscript ? "in script" : "in tag", (int) got,
                  (int) !got);
        }
        sw->bad++;
      }
    }
  }
}

/* The sweep tries every string of up to four bytes over the alphabet the
   predicate branches on, so no family goes untested for want of somebody
   imagining it. */
static void dirtylink_enumerate_bytes(httrackp *opt, selftest_sweep *sw) {
  static const char alphabet[] = "/?#&.:+*<>,\"'a@ ";
  const size_t n = sizeof(alphabet) - 1;
  char buf[8];
  size_t len;

  for (len = 0; len <= 4; len++) {
    size_t i, k, total = 1;

    for (k = 0; k < len; k++)
      total *= n;
    for (i = 0; i < total; i++) {
      size_t v = i;

      for (k = 0; k < len; k++) {
        buf[k] = alphabet[v % n];
        v /= n;
      }
      buf[len] = '\0';
      dirtylink_case(opt, sw, buf);
    }
  }
}

/* The branches four bytes cannot reach: a scheme, a name, an extension. A
   token earns its place by killing a mutant the others leave alive. */
static void dirtylink_enumerate_tokens(httrackp *opt, selftest_sweep *sw) {
  static const char *const tok[] = {
      "",      "http:", "HTTPS:", "ftp:", "mailto:", "a",  ".gif",
      ".php",  "a.gif", "/",      "//",   "#x",      "?q", "&amp;",
      "&#35;", "@",     ".",      ":",    "+",       " "};
  const size_t n = sizeof(tok) / sizeof(tok[0]);
  char buf[64];
  size_t i, j, k;

  for (i = 0; i < n; i++) {
    for (j = 0; j < n; j++) {
      for (k = 0; k < n; k++) {
        buf[0] = '\0';
        strcatbuff(buf, tok[i]);
        strcatbuff(buf, tok[j]);
        strcatbuff(buf, tok[k]);
        dirtylink_case(opt, sw, buf);
      }
    }
  }
}

int parse_selftest_dirtylink(httrackp *opt, hts_boolean dump) {
  /* Verdicts to hold whatever the sweep says, each naming why. */
  static const struct {
    const char *str;
    char lastc;
    hts_boolean inscript;
    hts_boolean want;
  } cases[] = {
      /* a directory string alone is no link, in script or in a tag (#1612) */
      {"/", ')', HTS_TRUE, HTS_FALSE},
      {"image/", ')', HTS_TRUE, HTS_FALSE},
      {"Alt+/", ')', HTS_TRUE, HTS_FALSE},
      {"$&/", ')', HTS_TRUE, HTS_FALSE},
      /* a fragment or query opening a path is (#1618) */
      {"/#top", ')', HTS_TRUE, HTS_TRUE},
      {"/?q=1", ')', HTS_TRUE, HTS_TRUE},
      {"img/#x", ')', HTS_TRUE, HTS_TRUE},
      /* so is a second segment */
      {"a/b/", ')', HTS_TRUE, HTS_TRUE},
      {"/a/b/", ')', HTS_TRUE, HTS_TRUE},
      /* outside script code a trailing slash never is, since a base href and a
         bare directory string look alike */
      {"a/b/", ')', HTS_FALSE, HTS_FALSE},
      /* a scheme speaks for itself, whatever follows */
      {"http://ex.co/", ')', HTS_FALSE, HTS_TRUE},
      {"HTTP://ex.co/", ')', HTS_FALSE, HTS_TRUE},
      {"ftp://ex.co/", ')', HTS_FALSE, HTS_TRUE},
      {"mailto:a@b.c", ')', HTS_TRUE, HTS_FALSE},
      {"javascript:void(0)", ')', HTS_TRUE, HTS_FALSE},
      /* an extension names a file, even where the string is concatenated */
      {"a.gif", '+', HTS_TRUE, HTS_TRUE},
      {"a.php", '+', HTS_TRUE, HTS_TRUE},
      {"page.html", ')', HTS_FALSE, HTS_TRUE},
      {"page.html#f", ')', HTS_FALSE, HTS_TRUE},
      /* but a '+' rules out everything an extension does not carry */
      {"http://ex.co/", '+', HTS_TRUE, HTS_FALSE},
      {"a/b/", '+', HTS_TRUE, HTS_FALSE},
      /* an address is not a link */
      {"foobar@aol.com", ')', HTS_TRUE, HTS_FALSE},
      /* a bare extension is a type name, not a file */
      {".gif", ')', HTS_TRUE, HTS_FALSE},
      /* code punctuation rules a string out */
      {"a*.gif", ')', HTS_TRUE, HTS_FALSE},
      {"a,b.gif", ')', HTS_TRUE, HTS_FALSE},
      {"<b>.gif", ')', HTS_TRUE, HTS_FALSE},
      /* the cut leaves nothing to name */
      {"#f", ')', HTS_TRUE, HTS_FALSE},
      {"?q=1", ')', HTS_TRUE, HTS_FALSE},
      {"", ')', HTS_TRUE, HTS_FALSE},
      /* a space is prose outside script code, and ordinary inside it */
      {"a b.gif", ')', HTS_FALSE, HTS_FALSE},
      {"a b.gif", ')', HTS_TRUE, HTS_TRUE},
  };

  selftest_sweep sw;
  size_t i;
  int err = 0;

  memset(&sw, 0, sizeof(sw));
  sw.dump = dump;

  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    const hts_boolean got =
        hts_dirty_link_is_url(opt, cases[i].str, strlen(cases[i].str),
                              cases[i].lastc, cases[i].inscript);

    sw.cases++;
    if (got != cases[i].want) {
      fprintf(stderr,
              "dirtylink \"%s\" (next byte '%c', %s): got %d, "
              "wanted %d\n",
              cases[i].str, cases[i].lastc,
              cases[i].inscript ? "in script" : "in tag", (int) got,
              (int) cases[i].want);
      err = 1;
    }
  }

  /* The parser hands a slice of the page, never a string of its own, so a
     reader that runs past "len" must fail here: the slice is a link, the
     buffer holding it is not. */
  if (!hts_dirty_link_is_url(opt, "a.gif*rest", 5, ')', HTS_TRUE) ||
      hts_dirty_link_is_url(opt, "a.gif*rest", 10, ')', HTS_TRUE)) {
    fprintf(stderr, "dirtylink: the verdict must read the first len bytes\n");
    err = 1;
  }
  /* A string at the parser's cap gets no verdict, one byte under it does. */
  {
    char BIGSTK big[HTS_URLMAXSIZE + 1];

    memset(big, 'a', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    memcpy(big + sizeof(big) - 5, ".gif", 4);
    if (hts_dirty_link_is_url(opt, big, HTS_URLMAXSIZE, ')', HTS_TRUE) ||
        !hts_dirty_link_is_url(opt, big + 1, HTS_URLMAXSIZE - 1, ')',
                               HTS_TRUE)) {
      fprintf(stderr, "dirtylink: the length cap is off by one or absent\n");
      err = 1;
    }
  }

  dirtylink_enumerate_bytes(opt, &sw);
  dirtylink_enumerate_tokens(opt, &sw);
  if (sw.dump)
    return 0;
  if (sw.bad != 0) {
    fprintf(stderr, "dirtylink: %d of %d swept cases disagree with the model\n",
            (int) sw.bad, (int) sw.cases);
    err = 1;
  }
  printf("dirtylink self-test %s (%d cases swept, %d links)\n",
         err ? "FAILED" : "OK", (int) sw.cases, (int) sw.accepted);
  return err;
}

/* Keyword a script assignment or call may carry a URL through, with what must
   follow it and what may close the statement. Order matters: the engine takes
   the first that matches. */
/* What must sit around a keyword for it to introduce a URL. */
typedef enum {
  JSGUARD_NONE = 0,
  JSGUARD_TAG_QUOTE, /* the quote the enclosing attribute is written with */
  JSGUARD_SPACE_BEFORE,
  JSGUARD_NO_IDENT_BYTE,  /* nothing JavaScript allows inside a name */
  JSGUARD_NO_IDENT_QUOTE, /* nor a quote, which ends a string */
  JSGUARD_SPACE_AFTER
} jsscan_guard;

static const struct jsscan_word {
  const char *word;
  char sep;              /* byte that must follow the keyword, 0 for none */
  const char *ends;      /* what may close it, NULL for the default */
  hts_boolean no_mime;   /* window.open("text/html") is a type, not a URL */
  hts_boolean no_method; /* xhr.open("GET", url): the method is not a URL */
  hts_boolean url_only;  /* from "jquery" names a module, not a file */
  jsscan_guard guard;
} jsscan_words[] = {
    {".src", '=', NULL, HTS_FALSE, HTS_FALSE, HTS_FALSE, JSGUARD_NONE},
    {"src", '=', NULL, HTS_FALSE, HTS_FALSE, HTS_FALSE, JSGUARD_TAG_QUOTE},
    {".location", '=', NULL, HTS_FALSE, HTS_FALSE, HTS_FALSE, JSGUARD_NONE},
    {":location", '=', NULL, HTS_FALSE, HTS_FALSE, HTS_FALSE, JSGUARD_NONE},
    {"location", '=', NULL, HTS_FALSE, HTS_FALSE, HTS_FALSE,
     JSGUARD_SPACE_BEFORE},
    {".href", '=', NULL, HTS_FALSE, HTS_FALSE, HTS_FALSE, JSGUARD_NONE},
    {".open", '(', "),", HTS_TRUE, HTS_TRUE, HTS_FALSE, JSGUARD_NONE},
    {".replace", '(', ")", HTS_FALSE, HTS_FALSE, HTS_FALSE, JSGUARD_NONE},
    {".link", '(', ")", HTS_FALSE, HTS_FALSE, HTS_FALSE, JSGUARD_NONE},
    {".url", '(', ")", HTS_FALSE, HTS_FALSE, HTS_FALSE, JSGUARD_NONE},
    {"url", '(', ")", HTS_FALSE, HTS_FALSE, HTS_FALSE, JSGUARD_NO_IDENT_BYTE},
    {"import", 0, NULL, HTS_FALSE, HTS_FALSE, HTS_FALSE, JSGUARD_SPACE_AFTER},
    {"from", 0, NULL, HTS_FALSE, HTS_FALSE, HTS_TRUE, JSGUARD_NO_IDENT_QUOTE},
};

/* The code point ending at cursor[-1], or the byte itself where no UTF-8 lead
   precedes it, which is how a Latin-1 page spells a non-breaking space. */
static unsigned int model_prev_codepoint(const char *cursor,
                                         const char *buffer) {
  const unsigned char *const p = (const unsigned char *) cursor;
  const size_t before = (size_t) (cursor - buffer);

  if (before >= 3 && (p[-3] & 0xF0) == 0xE0)
    return ((unsigned int) (p[-3] & 0x0F) << 12) |
           ((unsigned int) (p[-2] & 0x3F) << 6) | (p[-1] & 0x3Fu);
  if (before >= 2 && (p[-2] & 0xE0) == 0xC0)
    return ((unsigned int) (p[-2] & 0x1F) << 6) | (p[-1] & 0x3Fu);
  return p[-1];
}

/* Every ECMAScript WhiteSpace and LineTerminator above 127. */
static hts_boolean js_space_codepoint(unsigned int cp) {
  return cp == 0xA0 || cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200A) ||
                 cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F ||
                 cp == 0x3000 || cp == 0xFEFF
             ? HTS_TRUE
             : HTS_FALSE;
}

/* The CSS ident code points that ECMAScript reads as spaces, so that a name
   ends in one language and continues in the other. */
static hts_boolean css_ident_codepoint(unsigned int cp) {
  return cp == 0x1680 || cp == 0xFEFF ? HTS_TRUE : HTS_FALSE;
}

/* This models what hts_js_scan_link is meant to find: one of the keywords
   above, its separator, a quoted operand, and a byte closing the statement. */
static hts_boolean jsscan_model(httrackp *opt, const char *cursor,
                                const char *buffer, hts_boolean in_tag,
                                char tag_lastc, hts_boolean in_css, int *offset,
                                int *length) {
  const char *const dflt_ends = in_tag ? ";\"'" : ";";
  const struct jsscan_word *w = NULL;
  const char *ends;
  const char *a, *b;
  char prev;
  hts_boolean unquoted = HTS_FALSE, quoted;
  size_t i, n;
  int len;

  if ((opt->parsejava & HTSPARSE_NO_JAVASCRIPT) != 0)
    return HTS_FALSE;
  prev = html_prevc(cursor, buffer);

  for (i = 0; i < sizeof(jsscan_words) / sizeof(jsscan_words[0]); i++) {
    const int l = strfield(cursor, jsscan_words[i].word);

    if (l == 0)
      continue;
    switch (jsscan_words[i].guard) {
    case JSGUARD_TAG_QUOTE:
      if (!in_tag || tag_lastc != prev)
        continue;
      break;
    case JSGUARD_SPACE_BEFORE:
      if (!isspace(prev))
        continue;
      break;
    case JSGUARD_NO_IDENT_BYTE:
      /* The engine matches UTF-8 byte triples and this decodes the code point,
         so the two agree on which characters glue only by accident. Membership
         itself is pinned by cases[] below, never by the sweep. */
      if (isalnum((unsigned char) prev) || prev == '_' || prev == '$' ||
          prev == '.' ||
          ((unsigned char) prev >= 0x80 &&
           !js_space_codepoint(model_prev_codepoint(cursor, buffer))))
        continue;
      /* CSS reads a wider name, so url() glues to more there (#1754) */
      if (in_css &&
          (prev == '-' ||
           ((unsigned char) prev >= 0x80 &&
            css_ident_codepoint(model_prev_codepoint(cursor, buffer)))))
        continue;
      break;
    case JSGUARD_NO_IDENT_QUOTE:
      if (prev == '"' || prev == '\'' || prev == '`')
        continue;
      /* FALLTHROUGH to the identifier test */
      if (isalnum((unsigned char) prev) || prev == '_' || prev == '$' ||
          prev == '.' ||
          ((unsigned char) prev >= 0x80 &&
           !js_space_codepoint(model_prev_codepoint(cursor, buffer))))
        continue;
      break;
    case JSGUARD_SPACE_AFTER:
      if (!is_space(cursor[l]))
        continue;
      break;
    default:
      break;
    }
    w = &jsscan_words[i];
    a = cursor + l;
    goto matched;
  }
  return HTS_FALSE;

matched:
  ends = w->ends != NULL ? w->ends : dflt_ends;
  while (is_realspace(*a))
    a++;
  if (w->sep != 0) {
    if (*a != w->sep)
      return HTS_FALSE;
    a++;
    while (is_realspace(*a))
      a++;
  }
  quoted = (*a == '"' || *a == '\'') ? HTS_TRUE : HTS_FALSE;
  /* CSS lets url() take a bare operand, which then ends at the ')' */
  unquoted = (!quoted && in_css && w->sep == '(' && strcmp(w->word, "url") == 0)
                 ? HTS_TRUE
                 : HTS_FALSE;
  if (!quoted && !unquoted)
    return HTS_FALSE;
  if (quoted)
    a++;
  for (b = a; *b != '\0'; b++) {
    if (quoted ? (*b == '"' || *b == '\'') : (*b == ')'))
      break;
  }
  if (*b == '\0') /* truncated input: no closing delimiter */
    return HTS_FALSE;

  { /* the statement must close after the operand */
    const char *c = b + (quoted ? 1 : 0);

    while (*c == ' ')
      c++;
    if (strchr(ends, *c) == NULL && *c != '\n' && *c != '\r' &&
        !(w->guard == JSGUARD_SPACE_AFTER && b[quoted ? 1 : 0] == ' '))
      return HTS_FALSE;
  }
  len = (int) (b - a);
  if (len == 0) /* nothing between the delimiters */
    return HTS_FALSE;

  if (w->no_mime) {
    for (i = 0; hts_main_mime[i] != NULL && hts_main_mime[i][0] != '\0'; i++) {
      const int l = strfield(a, hts_main_mime[i]);

      if (l && a[l] == '/')
        return HTS_FALSE;
    }
  }
  if (w->no_method) {
    /* The engine's own list, spelled again rather than shared: a model that
       calls the code it judges cannot see a bug inside it. */
    static const char *const methods[] = {"GET",    "POST",  "PUT",
                                          "DELETE", "HEAD",  "OPTIONS",
                                          "PATCH",  "TRACE", NULL};

    for (i = 0; methods[i] != NULL; i++) {
      if (strlen(methods[i]) == (size_t) len && strfield(a, methods[i]) == len)
        return HTS_FALSE;
    }
  }

  /* Spelled again as prefixes, not as indexed bytes: a specifier is a URL
     when it is a path or carries a scheme. */
  if (w->url_only && strncmp(a, "/", 1) != 0 && strncmp(a, "./", 2) != 0 &&
      strncmp(a, "../", 3) != 0 && strfield(a, "http:") == 0 &&
      strfield(a, "https:") == 0 && strfield(a, "ftp:") == 0)
    return HTS_FALSE;

  /* a leading ',' or ';' says this is code, a quote or a control byte says the
     operand never was one string, and "${" says it is interpolated.
     Near-verbatim with the engine's loop on purpose. The sweep compares the
     two, so an edit to both would agree; what catches that is cases[] and the
     crawl in test 490, which never consult this model. */
  for (n = 0, i = 0; i < (size_t) len; i++) {
    if (a[i] == '$' && i + 1 < (size_t) len && a[i + 1] == '{') {
      return HTS_FALSE;
    } else if (a[i] == ',' || a[i] == ';') {
      if (n == 0)
        return HTS_FALSE;
    } else if (a[i] == '"' || a[i] == '\'' || a[i] == '\t' || a[i] == '\r' ||
               a[i] == '\n') {
      return HTS_FALSE;
    } else if (a[i] != ' ') {
      n++;
    }
  }
  *offset = (int) (a - cursor);
  *length = len;
  return HTS_TRUE;
}

/* One snippet in one context, engine against model. */
static void jsscan_case(httrackp *opt, selftest_sweep *sw, const char *text,
                        size_t at, hts_boolean in_tag, char tag_lastc,
                        hts_boolean in_css) {
  hts_js_link got;
  int woff = 0, wlen = 0;
  const hts_boolean wanted = jsscan_model(opt, text + at, text, in_tag,
                                          tag_lastc, in_css, &woff, &wlen);
  const hts_boolean found =
      hts_js_scan_link(opt, text + at, text, in_tag, tag_lastc, in_css, &got);

  sw->cases++;
  if (found)
    sw->accepted++;
  if (sw->dump) {
    printf("%d\t%d\t%d\t%d\t%d\t%s\n", (int) found, got.offset, got.length,
           (int) in_tag, (int) in_css, text);
    return;
  }
  if (found != wanted ||
      (found && (got.offset != woff || got.length != wlen))) {
    if (sw->bad < 20) {
      fprintf(stderr,
              "jsscan \"%s\" (at %d, %s, %s): engine %d [%d,%d], "
              "model %d [%d,%d]\n",
              text, (int) at, in_tag ? "in tag" : "free", in_css ? "css" : "js",
              (int) found, got.offset, got.length, (int) wanted, woff, wlen);
    }
    sw->bad++;
  }
}

/* The sweep builds each snippet as prefix + keyword + separator + quote +
   operand + tail, taking one representative per class the scanner branches on
   rather than every byte. */
static void jsscan_sweep(httrackp *opt, selftest_sweep *sw) {
  /* The non-ASCII prefixes are the control on the two decoders agreeing, not
     on the byte list, which cases[] pins. U+20A0 ends in 0xA0, so an engine
     reading that byte as a space disagrees with the model here and nowhere
     else. */
  static const char *const prefix[] = {"",
                                       " ",
                                       "a",
                                       "_",
                                       "\"",
                                       "$",
                                       ".",
                                       "`",
                                       ":",
                                       "}",
                                       "\n",
                                       "\302\240",
                                       "\303\251",
                                       "\342\202\240",
                                       "-",
                                       "\341\232\200",
                                       "\357\273\277"};
  static const char *const word[] = {
      ".src",     "src",   ".SRC",   ".location", ":location",
      "location", ".href", ".open",  ".replace",  ".link",
      ".url",     "url",   "import", "from",      "foo"};
  static const char *const sep[] = {"=", "(", ",", "", " ="};
  static const char *const quote[] = {"\"", "'", ""};
  static const char *const operand[] = {
      "a.gif", "text/html", "GET",     ",x",    "x;y",    "",
      "a b",   "./a.js",    "../a.js", "/a.js", "..x.js", "http://h/a.js"};
  static const char *const tail[] = {";", ")", ",", "\n", " ;", "", "x"};
  char text[128];
  size_t p, w, s, q, o, t;

  for (p = 0; p < sizeof(prefix) / sizeof(prefix[0]); p++) {
    for (w = 0; w < sizeof(word) / sizeof(word[0]); w++) {
      for (s = 0; s < sizeof(sep) / sizeof(sep[0]); s++) {
        for (q = 0; q < sizeof(quote) / sizeof(quote[0]); q++) {
          for (o = 0; o < sizeof(operand) / sizeof(operand[0]); o++) {
            for (t = 0; t < sizeof(tail) / sizeof(tail[0]); t++) {
              const size_t at = strlen(prefix[p]);
              size_t q2, tag, css;

              text[0] = '\0';
              strcatbuff(text, prefix[p]);
              strcatbuff(text, word[w]);
              strcatbuff(text, sep[s]);
              strcatbuff(text, quote[q]);
              strcatbuff(text, operand[o]);
              strcatbuff(text, quote[q]);
              strcatbuff(text, tail[t]);
              for (q2 = 0; q2 < 2; q2++) {      /* the tag's own quote */
                for (tag = 0; tag < 2; tag++) { /* a script inside a tag */
                  for (css = 0; css < 2; css++) {
                    jsscan_case(opt, sw, text, at, tag ? HTS_TRUE : HTS_FALSE,
                                q2 ? '\'' : '"', css ? HTS_TRUE : HTS_FALSE);
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

int parse_selftest_jsscan(httrackp *opt, hts_boolean dump) {
  /* What the sweep only pins by agreeing with the model. The cursor sits at
     "at", the offset and length are counted from it. */
  static const struct {
    const char *text;
    size_t at;
    hts_boolean in_tag;
    hts_boolean in_css;
    hts_boolean want;
    int offset;
    int length;
  } cases[] = {
      /* the assignment form, and the call form */
      {"x.src=\"pic.gif\";", 1, HTS_FALSE, HTS_FALSE, HTS_TRUE, 6, 7},
      {"w.open(\"pic.gif\",z)", 1, HTS_FALSE, HTS_FALSE, HTS_TRUE, 7, 7},
      {"x.SRC=\"pic.gif\";", 1, HTS_FALSE, HTS_FALSE, HTS_TRUE, 6, 7},
      /* .open's first argument can be a method or a type, and is neither */
      {"x.open(\"GET\",u)", 1, HTS_FALSE, HTS_FALSE, HTS_FALSE, 0, 0},
      {"x.open(\"text/html\")", 1, HTS_FALSE, HTS_FALSE, HTS_FALSE, 0, 0},
      /* CSS lets url() drop the quotes, JavaScript's URL(x) does not */
      {"url(pic.png)", 0, HTS_FALSE, HTS_TRUE, HTS_TRUE, 4, 7},
      {"url(pic.png)", 0, HTS_FALSE, HTS_FALSE, HTS_FALSE, 0, 0},
      /* @import ends on the space its media condition needs */
      {"@import \"a.css\" screen;", 1, HTS_FALSE, HTS_FALSE, HTS_TRUE, 8, 5},
      /* a bare location needs a space before it */
      {" location=\"a\";", 1, HTS_FALSE, HTS_FALSE, HTS_TRUE, 10, 1},
      {"alocation=\"a\";", 1, HTS_FALSE, HTS_FALSE, HTS_FALSE, 0, 0},
      /* a name ending in "url" is one identifier token, so neither its
         assignment nor its call carries a link (#1739) */
      {"aurl=\"pic.gif\";", 1, HTS_FALSE, HTS_FALSE, HTS_FALSE, 0, 0},
      {"myurl(\"pic.gif\")", 2, HTS_FALSE, HTS_FALSE, HTS_FALSE, 0, 0},
      {"var myurl = \"hello world\";", 6, HTS_FALSE, HTS_FALSE, HTS_FALSE, 0,
       0},
      {"var baseurl = \"jquery7\";", 8, HTS_FALSE, HTS_FALSE, HTS_FALSE, 0, 0},
      {"var a_url = \"a.gif\";", 6, HTS_FALSE, HTS_FALSE, HTS_FALSE, 0, 0},
      {"$url(\"a.gif\")", 1, HTS_FALSE, HTS_FALSE, HTS_FALSE, 0, 0},
      {"\303\251url(\"a.gif\")", 2, HTS_FALSE, HTS_FALSE, HTS_FALSE, 0, 0},
      /* the spellings CSS writes url() in, which must all still be taken */
      {":url(a.png)", 1, HTS_FALSE, HTS_TRUE, HTS_TRUE, 4, 5},
      {",url(a.png)", 1, HTS_FALSE, HTS_TRUE, HTS_TRUE, 4, 5},
      {"(url(a.png)", 1, HTS_FALSE, HTS_TRUE, HTS_TRUE, 4, 5},
      {" url(a.png)", 1, HTS_FALSE, HTS_TRUE, HTS_TRUE, 4, 5},
      {"\302\240url(a.png)", 2, HTS_FALSE, HTS_TRUE, HTS_TRUE, 4, 5},
      /* CSS has no template literal and juxtaposes its tokens, so a quote
         before url() is a boundary there and a name byte in code */
      {"content:\"x\"url(a.png)", 11, HTS_FALSE, HTS_TRUE, HTS_TRUE, 4, 5},
      {"`url('${t}')`", 1, HTS_FALSE, HTS_FALSE, HTS_FALSE, 0, 0},
      {"`url('/r/r1')`", 1, HTS_FALSE, HTS_FALSE, HTS_TRUE, 5, 5},
      {"`url('/a/${t}.png')`", 1, HTS_FALSE, HTS_FALSE, HTS_FALSE, 0, 0},
      /* .url() is a method call, and has its own row the way .href has */
      {"o.url(\"/t/x.gif\")", 1, HTS_FALSE, HTS_FALSE, HTS_TRUE, 6, 8},
      {"o.myurl(\"/t/x\")", 1, HTS_FALSE, HTS_FALSE, HTS_FALSE, 0, 0},
      {"content:'x'url(a.png)", 11, HTS_FALSE, HTS_TRUE, HTS_TRUE, 4, 5},
      /* a quote ends a string, so the url() right after one is real */
      {"'url('a.gif')", 1, HTS_FALSE, HTS_FALSE, HTS_TRUE, 5, 5},
      {"\"url(\"a.gif\")", 1, HTS_FALSE, HTS_FALSE, HTS_TRUE, 5, 5},
      {"a_url(\"a.gif\")", 2, HTS_FALSE, HTS_FALSE, HTS_FALSE, 0, 0},
      /* a name ends on any Unicode space, not only the three once spelled */
      {"\343\200\200url(a.png)", 3, HTS_FALSE, HTS_TRUE, HTS_TRUE, 4, 5},
      {"\342\200\257url(a.png)", 3, HTS_FALSE, HTS_TRUE, HTS_TRUE, 4, 5},
      {"\341\273\277url(a.png)", 3, HTS_FALSE, HTS_TRUE, HTS_FALSE, 0, 0},
      {"\240url(a.png)", 1, HTS_FALSE, HTS_TRUE, HTS_TRUE, 4, 5},
      /* U+1680 and U+FEFF: a JavaScript space, a CSS ident code point */
      {"\341\232\200url(\"a.png\")", 3, HTS_FALSE, HTS_FALSE, HTS_TRUE, 5, 5},
      {"\341\232\200url(a.png)", 3, HTS_FALSE, HTS_TRUE, HTS_FALSE, 0, 0},
      {"x\341\232\200url(a.png)", 4, HTS_FALSE, HTS_TRUE, HTS_FALSE, 0, 0},
      {"\357\273\277url(\"a.png\")", 3, HTS_FALSE, HTS_FALSE, HTS_TRUE, 5, 5},
      {"\357\273\277url(a.png)", 3, HTS_FALSE, HTS_TRUE, HTS_FALSE, 0, 0},
      {"x\357\273\277url(a.png)", 4, HTS_FALSE, HTS_TRUE, HTS_FALSE, 0, 0},
      /* "-" continues a CSS name, so image-url() is a function of its own.
         JavaScript subtracts there instead, so url() really is called. */
      {"image-url(a.png)", 6, HTS_FALSE, HTS_TRUE, HTS_FALSE, 0, 0},
      {"image-url(\"a.png\")", 6, HTS_FALSE, HTS_FALSE, HTS_TRUE, 5, 5},
      {"-url(a.png)", 1, HTS_FALSE, HTS_TRUE, HTS_FALSE, 0, 0},
      {"\342\201\237url(a.png)", 3, HTS_FALSE, HTS_TRUE, HTS_TRUE, 4, 5},
      {"\342\200\250url(a.png)", 3, HTS_FALSE, HTS_TRUE, HTS_TRUE, 4, 5},
      {"\342\200\251url(a.png)", 3, HTS_FALSE, HTS_TRUE, HTS_TRUE, 4, 5},
      /* U+2000..200A are spaces and U+200B is not, so both ends are pinned */
      {"\342\200\200url(a.png)", 3, HTS_FALSE, HTS_TRUE, HTS_TRUE, 4, 5},
      {"\342\200\212url(a.png)", 3, HTS_FALSE, HTS_TRUE, HTS_TRUE, 4, 5},
      {"\342\200\213url(a.png)", 3, HTS_FALSE, HTS_TRUE, HTS_FALSE, 0, 0},
      /* a zero-width joiner is part of a name, so it glues */
      {"\342\200\214url(a.png)", 3, HTS_FALSE, HTS_TRUE, HTS_FALSE, 0, 0},
      /* JavaScript's new URL(x) takes the same token, quoted */
      {"new URL(\"a.gif\")", 4, HTS_FALSE, HTS_FALSE, HTS_TRUE, 5, 5},
      /* an operand holding code is not a URL */
      {"x.src=\",a.gif\";", 1, HTS_FALSE, HTS_FALSE, HTS_FALSE, 0, 0},
      {"x.src=\"\";", 1, HTS_FALSE, HTS_FALSE, HTS_FALSE, 0, 0},
      /* a script inside a tag ends on the attribute's own quote */
      {"x.src='a.gif'\"", 1, HTS_TRUE, HTS_FALSE, HTS_TRUE, 6, 5},
      /* a module specifier is followed when it is a path, and export takes
         the same keyword as import */
      {"import x from\"./x.js\";", 9, HTS_FALSE, HTS_FALSE, HTS_TRUE, 5, 6},
      {"import x from \"../x.js\";", 9, HTS_FALSE, HTS_FALSE, HTS_TRUE, 6, 7},
      {"from\"/x.js\";", 0, HTS_FALSE, HTS_FALSE, HTS_TRUE, 5, 5},
      {"export{a}from\"./x.js\";", 9, HTS_FALSE, HTS_FALSE, HTS_TRUE, 5, 6},
      /* an absolute specifier is a URL too, and both spellings must agree */
      {"from\"https://h/x.js\";", 0, HTS_FALSE, HTS_FALSE, HTS_TRUE, 5, 14},
      {"import \"https://h/x.js\";", 0, HTS_FALSE, HTS_FALSE, HTS_TRUE, 8, 14},
      /* a query rides along: the rule reads the leading bytes only */
      {"from\"./x.js?v=1\";", 0, HTS_FALSE, HTS_FALSE, HTS_TRUE, 5, 10},
      /* a module id the loader resolves. The ESM grammar wants a slash after
         the dots, so "..x.js" and ".config/a.js" are ids and not paths. */
      {"from\"@scope/pkg\";", 0, HTS_FALSE, HTS_FALSE, HTS_FALSE, 0, 0},
      {"from\"..x.js\";", 0, HTS_FALSE, HTS_FALSE, HTS_FALSE, 0, 0},
      {"from\".config/a.js\";", 0, HTS_FALSE, HTS_FALSE, HTS_FALSE, 0, 0},
      /* the keyword stands alone, and is not a call */
      {"xfrom\"./x.js\";", 1, HTS_FALSE, HTS_FALSE, HTS_FALSE, 0, 0},
      {"x.from(\"./x.js\")", 2, HTS_FALSE, HTS_FALSE, HTS_FALSE, 0, 0},
      /* a name ending in "from" is not the keyword, and the assignment it
         carries is not a link. The sweep's "_" prefix covers that arm of the
         name set, so one spelling is enough here. */
      {"var copyFrom = \"hello world\";", 8, HTS_FALSE, HTS_FALSE, HTS_FALSE, 0,
       0},
      /* '$' opens a name, and a byte above 127 opens a Unicode one. UTF-8
         whitespace does not, so the keyword still stands alone after it. */
      {"$from\"./x.js\";", 1, HTS_FALSE, HTS_FALSE, HTS_FALSE, 0, 0},
      {"\303\251from\"./x.js\";", 2, HTS_FALSE, HTS_FALSE, HTS_FALSE, 0, 0},
      {"\302\240from\"./x.js\";", 2, HTS_FALSE, HTS_FALSE, HTS_TRUE, 5, 6},
  };

  selftest_sweep sw;
  size_t i;
  int err = 0;

  memset(&sw, 0, sizeof(sw));
  sw.dump = dump;
  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    hts_js_link got;
    const hts_boolean found =
        hts_js_scan_link(opt, cases[i].text + cases[i].at, cases[i].text,
                         cases[i].in_tag, '"', cases[i].in_css, &got);

    sw.cases++;
    if (found != cases[i].want || (found && (got.offset != cases[i].offset ||
                                             got.length != cases[i].length))) {
      fprintf(stderr,
              "jsscan \"%s\" (at %d): found %d [%d,%d], wanted %d [%d,%d]\n",
              cases[i].text, (int) cases[i].at, (int) found, got.offset,
              got.length, (int) cases[i].want, cases[i].offset,
              cases[i].length);
      err = 1;
    }
  }

  jsscan_sweep(opt, &sw);
  if (sw.dump)
    return 0;
  if (sw.bad != 0) {
    fprintf(stderr, "jsscan: %d of %d swept cases disagree with the model\n",
            (int) sw.bad, (int) sw.cases);
    err = 1;
  }
  printf("jsscan self-test %s (%d cases swept, %d links)\n",
         err ? "FAILED" : "OK", (int) sw.cases, (int) sw.accepted);
  return err;
}

/* "before" is the source up to the operand's opening quote, which the case
   appends, so the quote the detector is given needs no index. */
static int jsimport_case(const char *before, hts_boolean want) {
  char text[128];
  hts_boolean got;

  text[0] = '\0';
  strcatbuff(text, before);
  strcatbuff(text, "\"x\")");
  got = hts_js_quote_is_import_arg(text + strlen(before), text);
  if (got != want) {
    fprintf(stderr, "jsimport \"%s\": got %d, wanted %d\n", text, (int) got,
            (int) want);
    return 1;
  }
  return 0;
}

int parse_selftest_jsimport(httrackp *opt) {
  static const struct {
    const char *before;
    hts_boolean want;
  } cases[] = {
      {"import(", HTS_TRUE},
      {"import (", HTS_TRUE},
      {"import\t(\n", HTS_TRUE},
      {"await import(", HTS_TRUE},
      {"e=>import(", HTS_TRUE},
      {";import(", HTS_TRUE},
      /* the keyword must stand alone, and a quote before it opens a string */
      {"preimport(", HTS_FALSE},
      {"foo.import(", HTS_FALSE},
      {"_import(", HTS_FALSE},
      {"$import(", HTS_FALSE},
      {"2import(", HTS_FALSE},
      {"a=\"import(\"", HTS_FALSE},
      /* neither a keyword nor a call */
      {"(", HTS_FALSE},
      {"import", HTS_FALSE},
      {"importx(", HTS_FALSE},
      {"import()(", HTS_FALSE},
      {"", HTS_FALSE},
      /* a keyword cut short by the document's first byte */
      {"mport(", HTS_FALSE},
      /* UTF-8 whitespace is a boundary, a Unicode name is not */
      {"\302\240import(", HTS_TRUE},
      {"\342\200\250import(", HTS_TRUE},
      {"caf\303\251import(", HTS_FALSE},
      /* The walk sees bytes, not syntax, so a commented-out call reads as one.
         The parser's comment automaton is what never asks here. */
      {"// import(", HTS_TRUE},
      {"/* import(", HTS_TRUE},
  };

  size_t i;
  int err = 0;

  (void) opt;
  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    err |= jsimport_case(cases[i].before, cases[i].want);
  printf("jsimport self-test %s (%d cases)\n", err ? "FAILED" : "OK",
         (int) (sizeof(cases) / sizeof(cases[0])));
  return err;
}

/* Is this attribute name one that never carries a link? */
static hts_boolean tagattr_is_nodetect(const char *name, size_t len) {
  size_t i;

  for (i = 0; strnotempty(hts_nodetect[i]); i++) {
    if (strlen(hts_nodetect[i]) == len && strfield(name, hts_nodetect[i]))
      return HTS_TRUE;
  }
  if (len >= 5 && strfield(name, "xmlns") && (len == 5 || name[5] == ':'))
    return HTS_TRUE;
  return HTS_FALSE;
}

/* A tag whose attribute we built ourselves, so the name the walk must resolve
   is known rather than recomputed. */
static int tagattr_case(selftest_sweep *sw, const char *before,
                        const char *name, const char *gap, char quote,
                        hts_boolean want) {
  char text[256];
  const char *nend = NULL;
  const char *quotep, *got;
  size_t at;
  int err = 0;

  sw->cases++;
  text[0] = '\0';
  strcatbuff(text, "<a ");
  strcatbuff(text, before);
  at = strlen(text);
  strcatbuff(text, name);
  strcatbuff(text, gap);
  strcatbuff(text, "=");
  strcatbuff(text, gap);
  quotep = text + strlen(text);
  {
    const char q[2] = {quote, '\0'};

    strcatbuff(text, q);
  }
  strcatbuff(text, "x.gif\">");

  got = hts_dirty_attr_name(quotep, text, &nend);
  if (got == NULL || got != text + at || nend != text + at + strlen(name)) {
    fprintf(stderr, "tagattr \"%s\": name resolved as %s, wanted \"%s\"\n",
            text, got == NULL ? "(none)" : got, name);
    err = 1;
  }
  if (hts_dirty_attr_detectable(quotep, text) != want) {
    fprintf(stderr, "tagattr \"%s\": detectable %d, wanted %d\n", text,
            (int) !want, (int) want);
    err = 1;
  }
  return err;
}

/* Every string of up to four bytes over the alphabet the walk branches on,
   sitting between the tag and the quote. Nothing here is a valid tag, so the
   check is on what the walk may claim, not on a second walk. */
/* What the walk may claim about a tag nobody would write. The three span
   rules restate what the walk promises rather than deriving it a second way,
   so they catch a rewrite that abandons them, not an off-by-one inside it.
   The verdict rule is the independent one: the name decides it. */
static int tagattr_probe(selftest_sweep *sw, char *text, size_t at) {
  const char *nend = NULL;
  const char *name;
  const char *quotep;
  const char *a;
  int err = 0;
  size_t q;

  for (q = 0; q < 2; q++) {
    {
      text[at] = q ? '\'' : '"';
      text[at + 1] = '\0';
      quotep = text + at;
      name = hts_dirty_attr_name(quotep, text, &nend);
      sw->cases++;

      /* the value must close an "name=", whatever else the tag holds */
      a = quotep - 1;
      while (a > text && is_taborspace(*a))
        a--;
      if (*a != '=' || a == text) {
        if (name != NULL) {
          if (sw->bad++ < 20)
            fprintf(stderr, "tagattr \"%s\": named an attribute with no '='\n",
                    text);
          err = 1;
        }
        continue;
      }
      if (name == NULL)
        continue;
      /* a name is one token inside the tag, past the tag's own name */
      if (name <= text + 1 || nend <= name || nend > quotep) {
        if (sw->bad++ < 20)
          fprintf(stderr, "tagattr \"%s\": name span out of the tag\n", text);
        err = 1;
      } else {
        const char *p;

        for (p = name; p < nend; p++) {
          if (*p == '=' || *p == '"' || *p == '\'' || is_realspace(*p)) {
            if (sw->bad++ < 20)
              fprintf(stderr, "tagattr \"%s\": name holds a separator\n", text);
            err = 1;
            break;
          }
        }
      }
      if (hts_dirty_attr_detectable(quotep, text) !=
          (tagattr_is_nodetect(name, (size_t) (nend - name)) ? HTS_FALSE
                                                             : HTS_TRUE)) {
        if (sw->bad++ < 20)
          fprintf(stderr, "tagattr \"%s\": verdict does not follow the name\n",
                  text);
        err = 1;
      }
      if (hts_dirty_attr_detectable(quotep, text))
        sw->accepted++;
    }
  }
  return err;
}

/* The sweep tries every tag of up to four bytes over the walk's alphabet. */
static int tagattr_junk_bytes(selftest_sweep *sw) {
  static const char alphabet[] = "= \"'a<\t";
  const size_t n = sizeof(alphabet) - 1;
  char text[16];
  size_t len;
  int err = 0;

  for (len = 0; len <= 4; len++) {
    size_t i, k, total = 1;

    for (k = 0; k < len; k++)
      total *= n;
    for (i = 0; i < total; i++) {
      size_t v = i;

      text[0] = '<';
      for (k = 0; k < len; k++) {
        text[1 + k] = alphabet[v % n];
        v /= n;
      }
      err |= tagattr_probe(sw, text, 1 + len);
    }
  }
  return err;
}

/* The same, over whole attribute names, so the verdict rule meets a name that
   carries no link as well as one that does. */
static int tagattr_junk_names(selftest_sweep *sw) {
  static const char *const tok[] = {"",   "=",     " ",       "\"",
                                    "'",  "a",     "<",       "\t",
                                    "id", "xmlns", "xmlns:x", "href"};
  const size_t n = sizeof(tok) / sizeof(tok[0]);
  char text[64];
  size_t i, j, k;
  int err = 0;

  for (i = 0; i < n; i++) {
    for (j = 0; j < n; j++) {
      for (k = 0; k < n; k++) {
        text[0] = '<';
        text[1] = '\0';
        strcatbuff(text, tok[i]);
        strcatbuff(text, tok[j]);
        strcatbuff(text, tok[k]);
        err |= tagattr_probe(sw, text, strlen(text));
      }
    }
  }
  return err;
}

int parse_selftest_tagattr(httrackp *opt) {
  /* names that may carry a link, and names that never do */
  static const char *const linky[] = {"href",   "src",        "data-src",
                                      "srcset", "background", "longdesc",
                                      "xmlnsx", "onclick"};
  static const char *const nolink[] = {"id",    "name",  "alt",
                                       "class", "title", "type",
                                       "style", "xmlns", "xmlns:xlink"};
  static const char *const before[] = {"", "id=\"x\" ", "alt = 'y'\t"};
  static const char *const gap[] = {"", " ", "\t", "  "};
  selftest_sweep sw;
  size_t b, g, i;
  int err = 0;

  (void) opt;
  memset(&sw, 0, sizeof(sw));
  for (b = 0; b < sizeof(before) / sizeof(before[0]); b++) {
    for (g = 0; g < sizeof(gap) / sizeof(gap[0]); g++) {
      size_t q;

      /* both quote characters against both classes of name, or a walk that
         reads only one of them passes */
      for (q = 0; q < 2; q++) {
        const char quote = q ? '\'' : '"';

        for (i = 0; i < sizeof(linky) / sizeof(linky[0]); i++)
          err |=
              tagattr_case(&sw, before[b], linky[i], gap[g], quote, HTS_TRUE);
        for (i = 0; i < sizeof(nolink) / sizeof(nolink[0]); i++)
          err |=
              tagattr_case(&sw, before[b], nolink[i], gap[g], quote, HTS_FALSE);
      }
    }
  }
  err |= tagattr_junk_bytes(&sw);
  err |= tagattr_junk_names(&sw);
  if (err) {
    printf("tagattr self-test FAILED (%d cases swept)\n", (int) sw.cases);
    return 1;
  }
  printf("tagattr self-test OK (%d cases swept, %d readable)\n", (int) sw.cases,
         (int) sw.accepted);
  return 0;
}

/* Names the string on failure, since a bare line number cannot say which row
   went red. */
static void st_linkdir_case(const char *lien, hts_boolean frag_or_query,
                            hts_boolean multisegment) {
  const hts_boolean got_frag = link_dir_has_fragment_or_query(lien);
  const hts_boolean got_multi = link_dir_is_multisegment(lien);

  if (got_frag != frag_or_query || got_multi != multisegment) {
    fprintf(
        stderr,
        "linkdir \"%s\": fragment %d wanted %d, multisegment %d wanted %d\n",
        lien, got_frag, frag_or_query, got_multi, multisegment);
    assertf(!"linkdir case failed");
  }
}

static int st_linkdir(httrackp *opt, int argc, char **argv) {
  size_t i;

  /* Each row is one string with the answer from
     link_dir_has_fragment_or_query, then from link_dir_is_multisegment. Both
     are asked on the raw string here. The engine asks the first on the raw
     string too, but the second on what the marker cut left. */
  static const struct {
    const char *lien;
    hts_boolean frag_or_query;
    hts_boolean multisegment;
  } cases[] = {
      /* #1612 refused these, and still must */
      {"/", HTS_FALSE, HTS_FALSE},
      {"image/", HTS_FALSE, HTS_FALSE},
      {"Alt+/", HTS_FALSE, HTS_FALSE},
      {"$&/", HTS_FALSE, HTS_FALSE},
      {"////", HTS_FALSE, HTS_FALSE},
      /* a second segment is evidence on its own */
      {"a/b/", HTS_FALSE, HTS_TRUE},
      {"/api/v1/", HTS_FALSE, HTS_TRUE},
      /* a marker opens a path, from the site root to a deeper one */
      {"/#top", HTS_TRUE, HTS_FALSE},
      {"/?q=1", HTS_TRUE, HTS_FALSE},
      {"img/#x", HTS_TRUE, HTS_FALSE},
      {"img/?q=1", HTS_TRUE, HTS_FALSE},
      {"a/b/#x", HTS_TRUE, HTS_TRUE},
      /* the marker must open a path, not follow a name or nothing */
      {"page.html#x", HTS_FALSE, HTS_FALSE},
      {"page.html?q=1", HTS_FALSE, HTS_FALSE},
      {"#top", HTS_FALSE, HTS_FALSE},
      {"?q=1", HTS_FALSE, HTS_FALSE},
      {"", HTS_FALSE, HTS_FALSE},
      /* a run of slashes carries no name, so the marker buys nothing */
      {"////#x", HTS_FALSE, HTS_TRUE},
      {"//#x", HTS_FALSE, HTS_TRUE},
      /* an entity before the marker decodes into an earlier one */
      {"//&num;/#x", HTS_FALSE, HTS_TRUE},
      {"/&#35;", HTS_FALSE, HTS_FALSE},
      {"a&b/#x", HTS_FALSE, HTS_FALSE},
      /* an '&' after the marker is part of the query, so it is left alone */
      {"/?a=1&b=2", HTS_TRUE, HTS_FALSE},
  };

  (void) opt;
  (void) argc;
  (void) argv;
  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    st_linkdir_case(cases[i].lien, cases[i].frag_or_query,
                    cases[i].multisegment);
  }
  printf("linkdir self-test OK\n");
  return 0;
}

static int st_dirtylink(httrackp *opt, int argc, char **argv) {
  return parse_selftest_dirtylink(
      opt, (argc > 0 && strcmp(argv[0], "dump") == 0) ? HTS_TRUE : HTS_FALSE);
}

static int st_jsscan(httrackp *opt, int argc, char **argv) {
  return parse_selftest_jsscan(
      opt, (argc > 0 && strcmp(argv[0], "dump") == 0) ? HTS_TRUE : HTS_FALSE);
}

static int st_jsimport(httrackp *opt, int argc, char **argv) {
  (void) argc;
  (void) argv;
  return parse_selftest_jsimport(opt);
}

static int st_tagattr(httrackp *opt, int argc, char **argv) {
  (void) argc;
  (void) argv;
  return parse_selftest_tagattr(opt);
}

/* Each call parses `txt` under a fresh host, then checkrobots() for `path`. */
static int rb_decide(robots_wizard *r, const char *txt, const char *path) {
  static int n = 0;
  char host[64];

  snprintf(host, sizeof(host), "h%d.example", n++);
  robots_parse(NULL, r, host, txt, strlen(txt), NULL, 0, HTS_TRUE, NULL, 0);
  return checkrobots(r, host, path);
}

/* The rule store must reach the RFC 9309 §2.5 floor, whatever the macro says.
 */
enum { rb_rfc9309_floor = 1 / (HTS_ROBOTS_MAX_TOKEN_SIZE >= 500 * 1024) };

/* robots.txt filling about `blobsize` stored bytes with "/padNNNNN/" rules
   (12 each: pattern plus marker and LF), then the two rules a caller asserts
   on: an Allow re-opening /pad00000/open/, and a final Disallow. */
static char *rb_bulk(size_t blobsize) {
  const size_t capa = blobsize * 2 + 4096;
  char *const txt = (char *) malloct(capa);
  size_t n, blob;
  int i;

  assertf(txt != NULL);
  n = (size_t) snprintf(txt, capa, "User-agent: *\n");
  for (i = 0, blob = 0; blob + 12 <= blobsize; i++, blob += 12) {
    assertf(i < 100000); // past that "/padNNNNNN/" costs 13, not 12
    n += (size_t) snprintf(txt + n, capa - n, "Disallow: /pad%05d/\n", i);
    assertf(n < capa);
  }
  n += (size_t) snprintf(txt + n, capa - n, "Allow: /pad00000/open/\n");
  assertf(n < capa);
  (void) snprintf(txt + n, capa - n, "Disallow: /secret/\n");
  return txt;
}

static int st_robots(httrackp *opt, int argc, char **argv) {
  robots_wizard robots;
  (void) opt;
  (void) argc;
  (void) argv;
  memset(&robots, 0, sizeof(robots));

  /* Longer Allow re-opens subtree under Disallow: / (old matcher couldn't). */
  {
    const char *txt = "User-agent: *\nDisallow: /\nAllow: /public/\n";

    assertf(rb_decide(&robots, txt, "/public/x") == 0); /* allowed */
    assertf(rb_decide(&robots, txt, "/private") == -1); /* denied */
    assertf(rb_decide(&robots, txt, "/") == -1);        /* denied */
  }

  /* Equal-length match: Allow wins the tie over Disallow. */
  {
    const char *txt = "User-agent: *\nDisallow: /foo\nAllow: /foo\n";

    assertf(rb_decide(&robots, txt, "/foo/bar") == 0);
  }

  /* Longest match wins even when it is not the last rule. */
  {
    assertf(rb_decide(&robots, "User-agent: *\nDisallow: /a/b\nAllow: /a\n",
                      "/a/b/c") == -1);
    assertf(rb_decide(&robots, "User-agent: *\nAllow: /a/b\nDisallow: /a\n",
                      "/a/b/c") == 0);
  }

  /* '*' matches any run of characters. */
  {
    const char *txt = "User-agent: *\nDisallow: /*.php\n";

    assertf(rb_decide(&robots, txt, "/a/b/index.php") == -1);
    assertf(rb_decide(&robots, txt, "/a/b/index.html") == 0);
  }

  /* Trailing '$' anchors the end of the path. */
  {
    const char *txt = "User-agent: *\nDisallow: /a$\n";

    assertf(rb_decide(&robots, txt, "/a") == -1);
    assertf(rb_decide(&robots, txt, "/ab") == 0);
    assertf(rb_decide(&robots, txt, "/a/b") == 0);
  }

  /* The httrack-specific group replaces the generic '*' group entirely. */
  {
    const char *txt = "User-agent: *\nDisallow: /everyone\n"
                      "User-agent: httrack\nDisallow: /\n";

    assertf(rb_decide(&robots, txt, "/anything") == -1);
  }

  /* Replace, not merge: the generic group does not bind the httrack group. */
  {
    const char *txt = "User-agent: *\nDisallow: /x\n"
                      "User-agent: httrack\nDisallow: /y\n";

    assertf(rb_decide(&robots, txt, "/x") == 0);
    assertf(rb_decide(&robots, txt, "/y") == -1);
  }

  /* RFC 9309 2.2.1: a user-agent line following a rule opens a new group, so
     the generic group written after ours is a fallback we no longer take. */
  {
    const char *txt = "User-agent: httrack\nDisallow: /secret\n\n"
                      "User-agent: *\nDisallow: /public\n";

    assertf(rb_decide(&robots, txt, "/secret") == -1);
    assertf(rb_decide(&robots, txt, "/public") == 0);

    /* the blank line is not what ends the group */
    txt = "User-agent: httrack\nDisallow: /secret\n"
          "User-agent: *\nDisallow: /public\n";
    assertf(rb_decide(&robots, txt, "/secret") == -1);
    assertf(rb_decide(&robots, txt, "/public") == 0);
  }

  /* Consecutive user-agent lines name one group, whichever comes first. */
  {
    assertf(rb_decide(&robots,
                      "User-agent: httrack\nUser-agent: *\n"
                      "Disallow: /x\n",
                      "/x") == -1);
    assertf(rb_decide(&robots,
                      "User-agent: httrack\nUser-agent: Googlebot\n"
                      "Disallow: /x\n",
                      "/x") == -1);
    assertf(rb_decide(&robots,
                      "User-agent: Googlebot\nUser-agent: httrack\n"
                      "Disallow: /x\n",
                      "/x") == -1);
    /* a blank line does not end the list either */
    assertf(rb_decide(&robots,
                      "User-agent: httrack\n\nUser-agent: *\n"
                      "Disallow: /x\n",
                      "/x") == -1);
  }

  /* A group naming somebody else stays somebody else's. */
  {
    const char *txt = "User-agent: Googlebot\nDisallow: /y\n\n"
                      "User-agent: httrack\nDisallow: /x\n";

    assertf(rb_decide(&robots, txt, "/y") == 0);
    assertf(rb_decide(&robots, txt, "/x") == -1);
  }

  /* Two groups naming us are combined, and the generic one between them is
     still skipped. */
  {
    const char *txt = "User-agent: httrack\nDisallow: /a\n\n"
                      "User-agent: winhttrack\nDisallow: /b\n";

    assertf(rb_decide(&robots, txt, "/a") == -1);
    assertf(rb_decide(&robots, txt, "/b") == -1);

    txt = "User-agent: httrack\nDisallow: /a\n"
          "User-agent: *\nDisallow: /generic\n"
          "User-agent: httrack\nDisallow: /b\n";
    assertf(rb_decide(&robots, txt, "/a") == -1);
    assertf(rb_decide(&robots, txt, "/b") == -1);
    assertf(rb_decide(&robots, txt, "/generic") == 0);
  }

  /* With no group naming us, every generic group still counts. */
  {
    const char *txt = "User-agent: *\nDisallow: /a\n\n"
                      "User-agent: *\nDisallow: /b\n";

    assertf(rb_decide(&robots, txt, "/a") == -1);
    assertf(rb_decide(&robots, txt, "/b") == -1);

    txt = "User-agent: *\nDisallow: /a\n"
          "User-agent: Googlebot\nDisallow: /b\n"
          "User-agent: *\nDisallow: /c\n";
    assertf(rb_decide(&robots, txt, "/a") == -1);
    assertf(rb_decide(&robots, txt, "/b") == 0);
    assertf(rb_decide(&robots, txt, "/c") == -1);
  }

  /* No rules: everything is allowed. */
  assertf(rb_decide(&robots, "User-agent: *\nDisallow:\n", "/x") == 0);

  /* #1286: rules survive to the RFC 9309 floor. Past the old 4 KB store, just
     under the floor, and past it, where only the tail is left out. */
  {
    char *txt = rb_bulk(8192); /* past the old cap, well under the new one */

    assertf(rb_decide(&robots, txt, "/pad00000/x") == -1);
    assertf(rb_decide(&robots, txt, "/pad00000/open/x") == 0); /* Allow wins */
    assertf(rb_decide(&robots, txt, "/secret/x") == -1);
    freet(txt);

    txt = rb_bulk(HTS_ROBOTS_MAX_TOKEN_SIZE - 1024); /* just under the cap */
    assertf(rb_decide(&robots, txt, "/pad00000/x") == -1);
    assertf(rb_decide(&robots, txt, "/pad00000/open/x") == 0);
    assertf(rb_decide(&robots, txt, "/secret/x") == -1);
    freet(txt);

    /* Past it the tail is lost, which robots_parse reports through the log. */
    txt = rb_bulk(HTS_ROBOTS_MAX_TOKEN_SIZE + 4096);
    assertf(rb_decide(&robots, txt, "/pad00000/x") == -1);
    assertf(rb_decide(&robots, txt, "/pad00000/open/x") == -1);
    assertf(rb_decide(&robots, txt, "/secret/x") == 0);
    freet(txt);
  }

  /* A rule costs marker + pattern + LF, the accounting tests/309 computes. */
  {
    const char *const txt = "User-agent: *\nDisallow: /pad00000/\n";
    robots_wizard rb;

    memset(&rb, 0, sizeof(rb));
    robots_parse(NULL, &rb, "h.test", txt, strlen(txt), NULL, 0, HTS_TRUE, NULL,
                 0);
    assertf(rb.next != NULL && rb.next->token != NULL);
    assertf(strlen(rb.next->token) == strlen("/pad00000/") + 2);
    checkrobots_free(&rb);
  }

  /* A rule longer than the line buffer is read as a prefix of itself. Kept for
     a Disallow, which can then only forbid more; dropped for an Allow, which
     would otherwise permit more than the site wrote and beat the Disallow. */
  {
    char BIGSTK txt[HTS_ROBOTS_LINE_SIZE * 3];
    char BIGSTK path[HTS_ROBOTS_LINE_SIZE * 2];

    memset(path, 'a', sizeof(path));
    path[0] = '/';
    path[HTS_ROBOTS_LINE_SIZE + 200] = '\0';

    snprintf(txt, sizeof(txt), "User-agent: *\nDisallow: %s\n", path);
    assertf(rb_decide(&robots, txt, path) == -1);

    snprintf(txt, sizeof(txt), "User-agent: *\nDisallow: /a\nAllow: %s\n",
             path);
    assertf(rb_decide(&robots, txt, path) == -1);

    /* The longest line the buffer holds whole is not cut, so this Allow wins;
       one byte more is cut, and the Disallow it would have beaten stands. */
    path[HTS_ROBOTS_LINE_SIZE - 2 - strlen("Allow: ")] = '\0';
    snprintf(txt, sizeof(txt), "User-agent: *\nDisallow: /a\nAllow: %s\n",
             path);
    assertf(rb_decide(&robots, txt, path) == 0);

    path[HTS_ROBOTS_LINE_SIZE - 2 - strlen("Allow: ")] = 'a';
    path[HTS_ROBOTS_LINE_SIZE - 1 - strlen("Allow: ")] = '\0';
    snprintf(txt, sizeof(txt), "User-agent: *\nDisallow: /a\nAllow: %s\n",
             path);
    assertf(rb_decide(&robots, txt, path) == -1);
  }

  /* #1294: an over-long Disallow must not hand its own tail back as a rule. */
  {
    /* one past the HTS_ROBOTS_LINE_SIZE - 2 bytes the old read kept */
    const size_t resume = HTS_ROBOTS_LINE_SIZE - 1;
    char BIGSTK txt[HTS_ROBOTS_LINE_SIZE * 3];
    size_t head = (size_t) snprintf(
        txt, sizeof(txt), "User-agent: *\nDisallow: /open/\nDisallow: ");
    const size_t line = head - strlen("Disallow: ");

    memset(txt + head, 'a', line + resume - head);
    head = line + resume;
    head += (size_t) snprintf(txt + head, sizeof(txt) - head,
                              "Allow: /open/\nDisallow: /next/\n");
    assertf(head < sizeof(txt));
    assertf(rb_decide(&robots, txt, "/open/x") == -1);
    /* the line after the cut one is still a rule: consuming it whole must not
       become discarding the rest of the file */
    assertf(rb_decide(&robots, txt, "/next/x") == -1);

    /* control: that same text on a line of its own is a rule we do honour, so
       the refusal above is the tail never being read and not a dead pattern */
    assertf(rb_decide(&robots,
                      "User-agent: *\nDisallow: /open/\nAllow: /open/\n",
                      "/open/x") == 0);
  }

  checkrobots_free(&robots);
  printf("robots self-test OK\n");
  return 0;
}

/* Collect the URLs a sitemap scan hands out. */
typedef struct sm_collect {
  int n;
  char url[8][HTS_URLMAXSIZE];
} sm_collect;

static hts_boolean sm_take(void *arg, const char *url) {
  sm_collect *const c = (sm_collect *) arg;

  if (c->n < (int) (sizeof(c->url) / sizeof(c->url[0])))
    strcpybuff(c->url[c->n], url);
  c->n++;
  return HTS_TRUE;
}

/* Scan `doc` off a heap buffer with no NUL terminator, so a read past the
   declared size is an ASan error rather than a silent pass. */
static int sm_scan(const char *doc, int maxurls, hts_boolean *is_index,
                   sm_collect *out) {
  const size_t len = strlen(doc);
  char *raw = malloct(len);
  int n;

  memset(out, 0, sizeof(*out));
  assertf(raw != NULL);
  memcpy(raw, doc, len);
  n = hts_sitemap_scan(raw, len, maxurls, is_index, sm_take, out);
  freet(raw);
  return n;
}

static int st_sitemap(httrackp *opt, int argc, char **argv) {
  sm_collect c;
  hts_boolean idx;
  (void) opt;
  (void) argc;
  (void) argv;

  /* A urlset yields its <loc> URLs, in order, unescaped. */
  assertf(sm_scan("<?xml version=\"1.0\"?><urlset>"
                  "<url><loc>http://h.test/a.html</loc></url>"
                  "<url><loc>  https://h.test/b?x=1&amp;y=2\n  </loc></url>"
                  "</urlset>",
                  100, &idx, &c) == 2);
  assertf(!idx);
  assertf(strcmp(c.url[0], "http://h.test/a.html") == 0);
  assertf(strcmp(c.url[1], "https://h.test/b?x=1&y=2") == 0);

  /* A sitemapindex is flagged: its URLs are child sitemaps, not pages. */
  assertf(sm_scan("<sitemapindex><sitemap><loc>http://h.test/s2.xml.gz</loc>"
                  "</sitemap></sitemapindex>",
                  100, &idx, &c) == 1);
  assertf(idx);

  /* Root element decides even when the other name appears later as text. */
  assertf(sm_scan("<urlset><url><loc>http://h.test/a</loc></url>"
                  "<!-- sitemapindex --></urlset>",
                  100, &idx, &c) == 1);
  assertf(!idx);

  /* Numeric character references, decimal and hex, decode to ASCII. */
  assertf(sm_scan("<urlset><loc>http://h.test/a&#63;b&#x3D;c</loc></urlset>",
                  100, &idx, &c) == 1);
  assertf(strcmp(c.url[0], "http://h.test/a?b=c") == 0);

  /* A reference decoding to a control byte is dropped: the shared decoder
     writes the real character and the URL check refuses it. A reference the
     decoder cannot represent (&#0;) stays verbatim, like an unknown entity. */
  assertf(sm_scan("<urlset><loc>http://h.test/a&#10;b</loc></urlset>", 100,
                  &idx, &c) == 0);
  assertf(sm_scan("<urlset><loc>http://h.test/a&#9;b</loc></urlset>", 100, &idx,
                  &c) == 0);
  assertf(sm_scan("<urlset><loc>http://h.test/a&#0;b</loc></urlset>", 100, &idx,
                  &c) == 1);
  assertf(strcmp(c.url[0], "http://h.test/a&#0;b") == 0);

  /* A comment naming the other root element must not flip the verdict. */
  assertf(sm_scan("<!-- <sitemapindex> --><urlset><url>"
                  "<loc>http://h.test/p</loc></url></urlset>",
                  100, &idx, &c) == 1);
  assertf(!idx);
  assertf(sm_scan("<?xml version=\"1.0\"?><!-- <urlset> -->"
                  "<sitemapindex><loc>http://h.test/s</loc></sitemapindex>",
                  100, &idx, &c) == 1);
  assertf(idx);

  /* <location> is not <loc>. */
  assertf(sm_scan("<urlset><location>http://h.test/a</location></urlset>", 100,
                  &idx, &c) == 0);

  /* Rejected: relative, non-http scheme, embedded space, empty. */
  assertf(sm_scan("<urlset><loc>/a.html</loc><loc>ftp://h.test/a</loc>"
                  "<loc>javascript:alert(1)</loc>"
                  "<loc>http://h.test/a b</loc><loc></loc></urlset>",
                  100, &idx, &c) == 0);

  /* The URL length bound: one under fits, exactly at it is dropped rather than
     truncated into a different URL. */
  {
    char BIGSTK doc[HTS_URLMAXSIZE * 2];
    char BIGSTK url[HTS_URLMAXSIZE + 1];
    size_t i;

    strcpybuff(url, "http://h.test/");
    for (i = strlen(url); i < HTS_URLMAXSIZE - 1; i++)
      url[i] = 'a';
    url[i] = '\0';
    snprintf(doc, sizeof(doc), "<urlset><loc>%s</loc></urlset>", url);
    assertf(sm_scan(doc, 100, &idx, &c) == 1);

    url[i] = 'a';
    url[i + 1] = '\0';
    snprintf(doc, sizeof(doc), "<urlset><loc>%s</loc></urlset>", url);
    assertf(sm_scan(doc, 100, &idx, &c) == 0);
  }

  /* The URL cap stops the scan. */
  assertf(sm_scan("<urlset><loc>http://h.test/1</loc><loc>http://h.test/2</loc>"
                  "<loc>http://h.test/3</loc></urlset>",
                  2, &idx, &c) == 2);

  /* The per-document cap at the value the engine actually uses. */
  {
    const int many = HTS_SITEMAP_MAX_URLS_DOC + 10;
    const size_t cap = (size_t) many * 40 + 32;
    char *big = malloct(cap);
    size_t off;
    int i;

    assertf(big != NULL);
    off = (size_t) snprintf(big, cap, "<urlset>");
    assertf(off < cap);
    for (i = 0; i < many; i++) {
      const int len =
          snprintf(big + off, cap - off, "<loc>http://h.test/%d</loc>", i);

      assertf(len > 0 && (size_t) len < cap - off);
      off += (size_t) len;
    }
    memset(&c, 0, sizeof(c));
    assertf(hts_sitemap_scan(big, off, HTS_SITEMAP_MAX_URLS_DOC, &idx, sm_take,
                             &c) == HTS_SITEMAP_MAX_URLS_DOC);
    /* The handler count, not just the return: a call site hardcoding a smaller
       cap would still return its own argument. */
    assertf(c.n == HTS_SITEMAP_MAX_URLS_DOC);
    freet(big);
  }

  /* A highly compressible document decodes without running away: the ratio
     budget cannot bind (deflate tops out near 1032:1), so this pins the
     decompression path itself rather than the 64 MiB ceiling. */
  {
    const char *const one = "<url><loc>http://h.test/bomb</loc></url>";
    const size_t reps = 40000;
    size_t xlen = 8 + reps * strlen(one) + 10, i;
    char *x = malloct(xlen + 1);
    uLongf zlen;
    char *z;
    z_stream zs;

    assertf(x != NULL);
    {
      size_t w = (size_t) snprintf(x, xlen, "<urlset>");
      int len;

      assertf(w < xlen);
      for (i = 0; i < reps; i++) {
        len = snprintf(x + w, xlen - w, "%s", one);
        assertf(len > 0 && (size_t) len < xlen - w);
        w += (size_t) len;
      }
      len = snprintf(x + w, xlen - w, "</urlset>");
      assertf(len > 0 && (size_t) len < xlen - w);
      w += (size_t) len;
      xlen = w;
    }
    zlen = compressBound((uLong) xlen) + 32;
    z = malloct((size_t) zlen);
    assertf(z != NULL);
    memset(&zs, 0, sizeof(zs));
    assertf(deflateInit2(&zs, 9, Z_DEFLATED, 16 + MAX_WBITS, 8,
                         Z_DEFAULT_STRATEGY) == Z_OK);
    zs.next_in = (const Bytef *) x;
    zs.avail_in = (uInt) xlen;
    zs.next_out = (Bytef *) z;
    zs.avail_out = (uInt) zlen;
    assertf(deflate(&zs, Z_FINISH) == Z_STREAM_END);
    zlen = (uLongf) zs.total_out;
    deflateEnd(&zs);
    /* well over the 4096:1 budget's 1 MiB floor, and far under the 64 MiB cap
     */
    assertf(xlen > 1024 * 1024 && (size_t) zlen < xlen / 100);
    memset(&c, 0, sizeof(c));
    assertf(hts_sitemap_scan(z, (size_t) zlen, 10, &idx, sm_take, &c) == 10);
    assertf(strcmp(c.url[0], "http://h.test/bomb") == 0);
    freet(z);
    freet(x);
  }

  /* An unterminated <loc> at end of buffer must not read past it. */
  assertf(sm_scan("<urlset><loc>http://h.test/a", 100, &idx, &c) == 0);
  assertf(sm_scan("<urlset><lo", 100, &idx, &c) == 0);

  /* A gzip-framed document is decompressed before scanning. */
  {
    const char *const xml =
        "<urlset><url><loc>http://h.test/gz.html</loc></url></urlset>";
    uLongf zlen = compressBound((uLong) strlen(xml)) + 32;
    char *z = malloct((size_t) zlen);
    z_stream zs;

    assertf(z != NULL);
    memset(&zs, 0, sizeof(zs));
    assertf(deflateInit2(&zs, 9, Z_DEFLATED, 16 + MAX_WBITS, 8,
                         Z_DEFAULT_STRATEGY) == Z_OK);
    zs.next_in = (const Bytef *) xml;
    zs.avail_in = (uInt) strlen(xml);
    zs.next_out = (Bytef *) z;
    zs.avail_out = (uInt) zlen;
    assertf(deflate(&zs, Z_FINISH) == Z_STREAM_END);
    zlen = (uLongf) zs.total_out;
    deflateEnd(&zs);

    memset(&c, 0, sizeof(c));
    assertf(hts_sitemap_scan(z, (size_t) zlen, 100, &idx, sm_take, &c) == 1);
    assertf(strcmp(c.url[0], "http://h.test/gz.html") == 0);

    /* Truncated gzip: refused, not scanned as plain text. */
    memset(&c, 0, sizeof(c));
    assertf(hts_sitemap_scan(z, 4, 100, &idx, sm_take, &c) == -1);
    freet(z);
  }

  /* robots.txt: only Sitemap: records, comments stripped, case-insensitive,
     and group-independent (no User-agent line needed). */
  /* robots_parse collects Sitemap: whatever the user-agent group, strips the
     comment and keeps the rules working alongside it. */
  {
    const char *const txt = "User-agent: *\nDisallow: /x\n"
                            "SITEMAP:  http://h.test/s1.xml  # first\n"
                            "Sitemapper: http://h.test/no.xml\n"
                            "Sitemap:\thttps://h.test/s2.xml\n";
    char BIGSTK maps[1024];
    robots_wizard rb;

    memset(&rb, 0, sizeof(rb));
    robots_parse(NULL, &rb, "h.test", txt, strlen(txt), NULL, 0, HTS_TRUE, maps,
                 sizeof(maps));
    assertf(strcmp(maps, "http://h.test/s1.xml\nhttps://h.test/s2.xml\n") == 0);
    assertf(checkrobots(&rb, "h.test", "/x") == -1);
    checkrobots_free(&rb);
  }

  printf("sitemap self-test OK\n");
  return 0;
}

/* ------------------------------------------------------------ */
/* Registry: this module's tests, in the order -#test lists them. */
/* ------------------------------------------------------------ */

const struct selftest_entry selftests_parse[] = {
    {"linkdir", "",
     "a quoted directory string is a link only with a second segment, or a "
     "fragment or query opening right after the path",
     st_linkdir},
    {"dirtylink", "[dump]",
     "is a quoted string a link? sweeps the parser's alphabet against a model, "
     "or dumps its verdicts",
     st_dirtylink},
    {"jsscan", "[dump]",
     "does a script statement hand a URL to .src, .location, .open, url() and "
     "friends? sweeps the shapes against a model",
     st_jsscan},
    {"jsimport", "",
     "is a quoted string the operand of a dynamic import(), whose base is the "
     "script and not the page?",
     st_jsimport},
    {"tagattr", "",
     "may the dirty parser read this in-tag quoted value? resolves the owning "
     "attribute and refuses the names that carry no link",
     st_tagattr},
    {"robots", "", "robots.txt RFC 9309 Allow/Disallow precedence self-test",
     st_robots},
    {"sitemap", "",
     "sitemap <loc> extraction, caps and robots.txt Sitemap:", st_sitemap},
    {NULL, NULL, NULL, NULL},
};
