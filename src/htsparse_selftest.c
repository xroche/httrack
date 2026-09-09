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

#include "htscore.h"
#include "htslib.h"
#include "htsparse.h"
#include "htstools.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

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
  JSGUARD_NO_NAME_BYTE, /* neither an alphanumeric nor an underscore */
  JSGUARD_SPACE_AFTER
} jsscan_guard;

static const struct jsscan_word {
  const char *word;
  char sep;              /* byte that must follow the keyword, 0 for none */
  const char *ends;      /* what may close it, NULL for the default */
  hts_boolean no_mime;   /* window.open("text/html") is a type, not a URL */
  hts_boolean no_method; /* xhr.open("GET", url): the method is not a URL */
  jsscan_guard guard;
} jsscan_words[] = {
    {".src", '=', NULL, HTS_FALSE, HTS_FALSE, JSGUARD_NONE},
    {"src", '=', NULL, HTS_FALSE, HTS_FALSE, JSGUARD_TAG_QUOTE},
    {".location", '=', NULL, HTS_FALSE, HTS_FALSE, JSGUARD_NONE},
    {":location", '=', NULL, HTS_FALSE, HTS_FALSE, JSGUARD_NONE},
    {"location", '=', NULL, HTS_FALSE, HTS_FALSE, JSGUARD_SPACE_BEFORE},
    {".href", '=', NULL, HTS_FALSE, HTS_FALSE, JSGUARD_NONE},
    {".open", '(', "),", HTS_TRUE, HTS_TRUE, JSGUARD_NONE},
    {".replace", '(', ")", HTS_FALSE, HTS_FALSE, JSGUARD_NONE},
    {".link", '(', ")", HTS_FALSE, HTS_FALSE, JSGUARD_NONE},
    {"url", '(', ")", HTS_FALSE, HTS_FALSE, JSGUARD_NO_NAME_BYTE},
    {"import", 0, NULL, HTS_FALSE, HTS_FALSE, JSGUARD_SPACE_AFTER},
};

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
    case JSGUARD_NO_NAME_BYTE:
      /* The engine leaves its "url" match in place when this guard fails, so
         "aurl=" and "_url=" go on to match the plain assignment form. Modelled
         as the engine behaves, not as the guard reads. */
      if (isalnum(prev) || prev == '_') {
        w = &jsscan_words[0]; /* borrow the plain "name=" shape */
        a = cursor + l;
        goto matched;
      }
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

  /* a leading ',' or ';' says this is code, and a quote or a control byte says
     the operand never was one string */
  for (n = 0, i = 0; i < (size_t) len; i++) {
    if (a[i] == ',' || a[i] == ';') {
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
  static const char *const prefix[] = {"", " ", "a", "_", "\""};
  static const char *const word[] = {
      ".src",     "src",    ".SRC",  ".location", ":location",
      "location", ".href",  ".open", ".replace",  ".link",
      "url",      "import", "foo"};
  static const char *const sep[] = {"=", "(", ",", "", " ="};
  static const char *const quote[] = {"\"", "'", ""};
  static const char *const operand[] = {"a.gif", "text/html", "GET", ",x",
                                        "x;y",   "",          "a b"};
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
      /* the "url" guard rejects the match and keeps it, so the assignment
         form still fires: see JSGUARD_NO_NAME_BYTE */
      {"aurl=\"pic.gif\";", 1, HTS_FALSE, HTS_FALSE, HTS_TRUE, 5, 7},
      {"myurl(\"pic.gif\")", 2, HTS_FALSE, HTS_FALSE, HTS_FALSE, 0, 0},
      /* an operand holding code is not a URL */
      {"x.src=\",a.gif\";", 1, HTS_FALSE, HTS_FALSE, HTS_FALSE, 0, 0},
      {"x.src=\"\";", 1, HTS_FALSE, HTS_FALSE, HTS_FALSE, 0, 0},
      /* a script inside a tag ends on the attribute's own quote */
      {"x.src='a.gif'\"", 1, HTS_TRUE, HTS_FALSE, HTS_TRUE, 6, 5},
  };

  selftest_sweep sw;
  size_t i;
  int err = 0;

  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    hts_js_link got;
    const hts_boolean found =
        hts_js_scan_link(opt, cases[i].text + cases[i].at, cases[i].text,
                         cases[i].in_tag, '"', cases[i].in_css, &got);

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

  memset(&sw, 0, sizeof(sw));
  sw.dump = dump;
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
static int tagattr_case(const char *before, const char *name, const char *gap,
                        char quote, hts_boolean want) {
  char text[256];
  const char *nend = NULL;
  const char *quotep, *got;
  size_t at;
  int err = 0;

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
          err |= tagattr_case(before[b], linky[i], gap[g], quote, HTS_TRUE);
        for (i = 0; i < sizeof(nolink) / sizeof(nolink[0]); i++)
          err |= tagattr_case(before[b], nolink[i], gap[g], quote, HTS_FALSE);
      }
    }
  }
  err |= tagattr_junk_bytes(&sw);
  err |= tagattr_junk_names(&sw);
  if (err) {
    printf("tagattr self-test FAILED (%d junk tags swept)\n", (int) sw.cases);
    return 1;
  }
  printf("tagattr self-test OK (%d junk tags swept, %d readable)\n",
         (int) sw.cases, (int) sw.accepted);
  return 0;
}
