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
/* File: htsselftest.c subroutines:                             */
/*       named dispatch for the hidden engine self-tests        */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Each test was historically a single-letter `-#` arm in htscoremain.c; they
   now live behind one registry reached as `httrack -#test=NAME [args]` (and
   `-#test` lists them). A handler runs over the positional args
   (argv[0..argc-1]), prints one result line, and returns the process exit code.
 */

#define HTS_INTERNAL_BYTECODE

#include "htsselftest.h"

#include "htsglobal.h"
#include "htscore.h"
#include "htsdefines.h"
#include "htslib.h"
#include "htscache_selftest.h"
#include "htsdns_selftest.h"
#include "htscharset.h"
#include "htsencoding.h"
#include "htsmd5.h"
#include "coucal/coucal.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* very minimalistic internal tests */
static void basic_selftests(void) {
  // BUG 756328
  const char *const source =
      "/intent/"
      "tweet?url=https%3A%2F%2Fwww.httrack.com%2Fvacatures%2F1562519%"
      "2Fmedewerker-data-services&text=Medewerker+Data+Services&via=httrackcom";
  char buffer[1024];
  fil_normalized(source, buffer);
  // MD5 selftests
  md5selftest();
  // cookie_get field extraction (tab-separated, 0-based)
  {
    char cbuf[8192];

    assertf(strcmp(cookie_get(cbuf, "a\tb\tc", 0), "a") == 0);
    assertf(strcmp(cookie_get(cbuf, "a\tb\tc", 1), "b") == 0);
    assertf(strcmp(cookie_get(cbuf, "a\tb\tc", 2), "c") == 0);
    // multi-char fields catch length/boundary bugs that 1-char fields hide
    assertf(strcmp(cookie_get(cbuf, "host\tx\t/path/to", 0), "host") == 0);
    assertf(strcmp(cookie_get(cbuf, "host\tx\t/path/to", 2), "/path/to") == 0);
    assertf(strcmp(cookie_get(cbuf, "a\t\tc", 1), "") == 0);  // empty field
    assertf(strcmp(cookie_get(cbuf, "a\tb\tc", 9), "") == 0); // beyond last
  }
  // back_infostr() status-line formatting (no sockets: pure formatting over
  // in-memory slots). Stresses a few thousand entries across every status-code
  // arm. Regression for a clobber bug where the size/totalsize trailer was
  // written straight into the destination, wiping the URL it had just built.
  {
    static const struct {
      int code;
      const char *tag;
    } cases[] = {
        {200, "READY "},     {-1, "ERROR "},       {-2, "TIMEOUT "},
        {-3, "TOOSLOW "},    {400, "BADREQUEST "}, {403, "FORBIDDEN "},
        {404, "NOT FOUND "}, {500, "SERVERROR "},  {999, "ERROR(999)"},
    };

    const int ncases = (int) (sizeof(cases) / sizeof(cases[0]));
    const int n = 2000;
    lien_back *slots = calloct(n, sizeof(lien_back));
    char line[HTS_URLMAXSIZE * 4 + 1024];
    char expect[HTS_URLMAXSIZE * 4 + 1024];
    struct_back sb;
    int idx;

    sb.lnk = slots;
    sb.count = n;
    sb.ready = NULL;
    sb.ready_size_bytes = 0;
    for (idx = 0; idx < n; idx++) {
      lien_back *const slot = &slots[idx];

      slot->r.location = slot->location_buffer;
      slot->status = STATUS_READY;
      slot->r.statuscode = cases[idx % ncases].code;
      slot->r.size = idx;
      slot->r.totalsize = idx + 1;
      snprintf(slot->url_adr, sizeof(slot->url_adr), "http://h%d.example", idx);
      snprintf(slot->url_fil, sizeof(slot->url_fil), "/p/%d.html", idx);
    }
    for (idx = 0; idx < n; idx++) {
      line[0] = '\0';
      back_infostr(&sb, idx, 3, line, sizeof(line));
      // Exact match (not substring): pins tag/URL/trailer order and rejects a
      // partial clobber, duplication, or truncation that a presence check would
      // let through. The expected format is stated here independently.
      snprintf(expect, sizeof(expect),
               "%s\"http://h%d.example/p/%d.html\" " LLintP " " LLintP " ",
               cases[idx % ncases].tag, idx, idx, (LLint) idx,
               (LLint) (idx + 1));
      assertf(strcmp(line, expect) == 0);
    }
    // Near-maximal URL, driven through back_info() (which owns the status
    // buffer internally and prints to a FILE*). url_adr + url_fil together
    // overrun the old HTS_URLMAXSIZE*2+1024 buffer, so the bounded appends
    // would abort unless that buffer is sized to hold both fields. Regression
    // for that sizing -- exercising back_infostr() directly would miss it,
    // since the caller's buffer is what matters.
    {
      lien_back *const slot = &slots[0];
      const size_t adrlen = sizeof(slot->url_adr) - 8;
      const size_t fillen = sizeof(slot->url_fil) - 8;
      FILE *const fp = tmpfile();
      size_t got;

      assertf(fp != NULL);
      slot->status = STATUS_READY;
      slot->r.statuscode = 200;
      slot->r.size = 1;
      slot->r.totalsize = 2;
      memset(slot->url_adr, 'a', adrlen);
      slot->url_adr[adrlen] = '\0';
      slot->url_fil[0] = '/';
      memset(slot->url_fil + 1, 'b', fillen - 1);
      slot->url_fil[fillen] = '\0';
      back_info(&sb, 0, 3, fp);
      rewind(fp);
      got = fread(line, 1, sizeof(line) - 1, fp);
      line[got] = '\0';
      fclose(fp);
      snprintf(expect, sizeof(expect),
               "READY \"%s%s\" " LLintP " " LLintP " " LF, slot->url_adr,
               slot->url_fil, (LLint) 1, (LLint) 2);
      assertf(strcmp(line, expect) == 0);
    }
    freet(slots);
  }
  // next_token(): in-place token scanner. Strips surrounding quotes, unescapes
  // \" and \\ when flag is set, and returns the token terminator (the space, or
  // NULL at end of string). The unquote/unescape rewrites the string in place
  // by shifting left, so the result is always shorter -- regression for that
  // compaction.
  {
    char tok[64];

    // plain token: unchanged, returns a pointer AT the separating space (exact
    // position, not just any space -- a strchr-style impl would land elsewhere
    // once quotes shift the content)
    strcpybuff(tok, "abc def");
    {
      char *const end = next_token(tok, 0);
      assertf(end == tok + 3 && *end == ' ' && strcmp(tok, "abc def") == 0);
    }
    // surrounding quotes stripped, returns the (post-shift) trailing space
    strcpybuff(tok, "\"ab\" cd");
    {
      char *const end = next_token(tok, 1);
      assertf(end == tok + 2 && *end == ' ' && strcmp(tok, "ab cd") == 0);
    }
    // a space inside quotes does not end the token; end of string returns NULL
    strcpybuff(tok, "\"a b\"c");
    {
      char *const end = next_token(tok, 1);
      assertf(end == NULL && strcmp(tok, "a bc") == 0);
    }
    // \" and \\ are unescaped to literal " and \ in place
    strcpybuff(tok, "\"a\\\"b\\\\c\"");
    {
      char *const end = next_token(tok, 1);
      assertf(end == NULL && strcmp(tok, "a\"b\\c") == 0);
    }
    // unterminated quote: the opening quote is dropped, the rest survives, and
    // the scan runs to the NUL (returns NULL)
    strcpybuff(tok, "\"ab");
    {
      char *const end = next_token(tok, 1);
      assertf(end == NULL && strcmp(tok, "ab") == 0);
    }
    // trailing lone backslash in a quote: *(p+1) is the NUL, not an escape, so
    // the backslash is kept intact (and there is no over-read past the NUL)
    strcpybuff(tok, "\"a\\");
    {
      char *const end = next_token(tok, 1);
      assertf(end == NULL && strcmp(tok, "a\\") == 0);
    }
  }
  // fil_normalized(): canonicalizes a URL path. Query arguments are sorted
  // alphabetically (by the text after each '?'/'&') and the query is rebuilt
  // through a bounded builder; outside the query, "//" collapses to "/".
  // Regression for that builder.
  {
    char norm[256];

    assertf(strcmp(fil_normalized("/p?b=2&a=1&c=3", norm), "/p?a=1&b=2&c=3") ==
            0);
    assertf(strcmp(fil_normalized("/a//b", norm), "/a/b") == 0);
    // "//" is collapsed only before the query; inside the query it is kept
    assertf(strcmp(fil_normalized("/a//b?x=c//d", norm), "/a/b?x=c//d") == 0);
  }
  // give_mimext(): mime type -> file extension, bounded into the caller buffer.
  // Returns 1 when an extension was written, 0 otherwise.
  {
    char ext[16];

    assertf(give_mimext(ext, sizeof(ext), "image/gif") == 1);
    assertf(strcmp(ext, "gif") == 0);
    assertf(give_mimext(ext, sizeof(ext), "text/html") == 1);
    assertf(strcmp(ext, "html") == 0);
    assertf(give_mimext(ext, sizeof(ext), "no/such-mime-type") == 0);
    assertf(ext[0] == '\0');
    // modern web formats -> extension. Avoid MIME types the
    // application/<=4-char-subtype fallback could fabricate without a row.
    assertf(give_mimext(ext, sizeof(ext), "image/webp") == 1);
    assertf(strcmp(ext, "webp") == 0);
    assertf(give_mimext(ext, sizeof(ext), "application/manifest+json") == 1);
    assertf(strcmp(ext, "webmanifest") == 0);
    assertf(give_mimext(ext, sizeof(ext), "font/woff2") == 1);
    assertf(strcmp(ext, "woff2") == 0);
  }
  // convtolower(): lower-cases into the caller buffer (bounded by its size).
  {
    char low[64];

    assertf(strcmp(convtolower(low, sizeof(low), "ABC/Def.HTML"),
                   "abc/def.html") == 0);
  }
  // cut_path(): splits a path into directory (with trailing '/') and basename,
  // each bounded by its buffer size.
  {
    char path[256];
    char pname[256];

    {
      char full[] = "/dir/sub/file.html";

      cut_path(full, path, sizeof(path), pname, sizeof(pname));
      assertf(strcmp(path, "/dir/sub/") == 0);
      assertf(strcmp(pname, "file.html") == 0);
    }
    { // a trailing slash is trimmed before the split
      char full[] = "/dir/sub/";

      cut_path(full, path, sizeof(path), pname, sizeof(pname));
      assertf(strcmp(path, "/dir/") == 0);
      assertf(strcmp(pname, "sub") == 0);
    }
    { // a path of length <= 1 yields empty results
      char full[] = "/";

      cut_path(full, path, sizeof(path), pname, sizeof(pname));
      assertf(path[0] == '\0' && pname[0] == '\0');
    }
  }
  // get_httptype_sized(): a long MIME type (Office OOXML reaches 73 chars) is
  // written whole into a contenttype-sized buffer; returns 1 on a match, 0 when
  // flag==0 and nothing matched. Regression for the old contenttype[64]
  // overflow.
  {
    httrackp *opt = hts_create_opt();
    htsblk r; // write into the real struct field, not a stand-in

    assertf(opt != NULL);
    // a long MIME (Office OOXML reaches 73 chars) must fit htsblk.contenttype
    // whole: a [64] field would make this bounded copy abort.
    assertf(get_httptype_sized(opt, r.contenttype, sizeof(r.contenttype),
                               "deck.pptx", 0) == 1);
    assertf(strcmp(r.contenttype,
                   "application/vnd.openxmlformats-officedocument."
                   "presentationml.presentation") == 0);
    assertf(get_httptype_sized(opt, r.contenttype, sizeof(r.contenttype),
                               "x.gif", 0) == 1);
    assertf(strcmp(r.contenttype, "image/gif") == 0);
    // modern extensions map back to their MIME type
    assertf(get_httptype_sized(opt, r.contenttype, sizeof(r.contenttype),
                               "x.webp", 0) == 1);
    assertf(strcmp(r.contenttype, "image/webp") == 0);
    assertf(get_httptype_sized(opt, r.contenttype, sizeof(r.contenttype),
                               "app.wasm", 0) == 1);
    assertf(strcmp(r.contenttype, "application/wasm") == 0);
    assertf(get_httptype_sized(opt, r.contenttype, sizeof(r.contenttype),
                               "mod.mjs", 0) == 1);
    assertf(strcmp(r.contenttype, "text/javascript") == 0);
    // no extension and flag==0: nothing written, returns 0
    assertf(get_httptype_sized(opt, r.contenttype, sizeof(r.contenttype),
                               "noextfile", 0) == 0);
    assertf(r.contenttype[0] == '\0');
    // no extension and flag==1: octet-stream fallback, returns 1
    assertf(get_httptype_sized(opt, r.contenttype, sizeof(r.contenttype),
                               "noextfile", 1) == 1);
    assertf(strcmp(r.contenttype, "application/octet-stream") == 0);
    // empty fil: no extension to scan; must not over-read before the string.
    // flag==0 -> 0 (nothing written), flag==1 -> octet-stream.
    assertf(get_httptype_sized(opt, r.contenttype, sizeof(r.contenttype), "",
                               0) == 0);
    assertf(r.contenttype[0] == '\0');
    assertf(get_httptype_sized(opt, r.contenttype, sizeof(r.contenttype), "",
                               1) == 1);
    assertf(strcmp(r.contenttype, "application/octet-stream") == 0);
    // a user --assume rule with an empty value matches but writes nothing:
    // get_userhttptype returns 1 with the buffer empty, so get_httptype_sized
    // must still report 0 (callers test the return like the old
    // strnotempty(s)).
    StringCopy(opt->mimedefs, "\ncgi=\n");
    assertf(get_httptype_sized(opt, r.contenttype, sizeof(r.contenttype),
                               "/x.cgi", 0) == 0);
    assertf(r.contenttype[0] == '\0');
    StringCopy(opt->mimedefs, "\ncgi=text/html\n");
    assertf(get_httptype_sized(opt, r.contenttype, sizeof(r.contenttype),
                               "/x.cgi", 0) == 1);
    assertf(strcmp(r.contenttype, "text/html") == 0);
    hts_free_opt(opt);
  }
  // adr_normalized_sized(): bounded host normalization (passthrough when
  // already normal).
  {
    char n[HTS_URLMAXSIZE];

    assertf(strcmp(adr_normalized_sized("example.com", n, sizeof(n)),
                   "example.com") == 0);
  }
  // standard_name(): builds "<name><md5?>.<ext>" into a bounded buffer. The md5
  // is appended (4 chars) only when the URL has a query string (see url_md5),
  // so test both; pin the structure (name + ext, lengths), not the md5 chars.
  {
    char b[HTS_URLMAXSIZE * 2];
    const char *nom = "index.html"; // name part
    const char *dot = nom + 5;      // points at ".html"
    size_t len;

    // no query -> no md5: "index" + ".html"
    standard_name(b, sizeof(b), dot, nom, "http://example.com/index.html", 0);
    assertf(strcmp(b, "index.html") == 0);
    // query -> 4 md5 chars between name and ext: "index" + md5(4) + ".html"
    standard_name(b, sizeof(b), dot, nom, "http://example.com/index.html?v=1",
                  0);
    len = strlen(b);
    assertf(len == 5 + 4 + 5);
    assertf(strncmp(b, "index", 5) == 0);
    assertf(strcmp(b + len - 5, ".html") == 0);
    // short names: name kept (<=8), the extension is clamped to 3 -> ".htm"
    standard_name(b, sizeof(b), dot, nom, "http://example.com/index.html?v=1",
                  1);
    len = strlen(b);
    assertf(len == 5 + 4 + 4);
    assertf(strcmp(b + len - 4, ".htm") == 0);
    // short names with a >8-char name: the name is clamped to 8 ("indexpag")
    {
      const char *lnom = "indexpage.html";
      const char *ldot = lnom + 9; // points at ".html"

      standard_name(b, sizeof(b), ldot, lnom,
                    "http://example.com/indexpage.html?v=1", 1);
      len = strlen(b);
      assertf(len == 8 + 4 + 4);
      assertf(strncmp(b, "indexpag", 8) == 0);
      assertf(strcmp(b + len - 4, ".htm") == 0);
    }
  }
  // longfile_to_83(): single-name 8-3 (mode 1) / ISO9660 (mode 2) conversion;
  // uppercases, clamps the name (8 / 31) and the extension (3). It rewrites
  // 'save' in place, so pass a mutable array.
  {
    char n83[256];

    {
      char save[] = "longfilename.html";

      longfile_to_83(1, n83, sizeof(n83), save); // 8-3: name->8, ext->3
      assertf(strcmp(n83, "LONGFILE.HTM") == 0);
    }
    {
      char save[] = "longfilename.html";

      longfile_to_83(2, n83, sizeof(n83), save); // ISO9660: name->31, ext->3
      assertf(strcmp(n83, "LONGFILENAME.HTM") == 0);
    }
    { // sanitization: leading '.'->'_', interior dots
      char save[] = ".a b.c.d e"; // collapse to '_', spaces/specials -> '_'
                                  // (only the last dot stays as the separator)
      longfile_to_83(1, n83, sizeof(n83), save);
      assertf(strcmp(n83, "_A_B_C.D_E") == 0);
    }
  }
  // long_to_83(): per-segment 8-3 conversion of a whole path.
  {
    char n83[HTS_URLMAXSIZE * 2];
    char save[] = "dir/longfilename.html";

    long_to_83(1, n83, sizeof(n83), save);
    assertf(strcmp(n83, "DIR/LONGFILE.HTM") == 0);
  }
  // lienrelatif(): relative path from the directory of curr_fil to link.
  {
    char s[HTS_URLMAXSIZE * 2];

    // same directory -> just the basename
    assertf(lienrelatif(s, sizeof(s), "dir/page.html", "dir/index.html") == 0);
    assertf(strcmp(s, "page.html") == 0);
    // link one level up -> a "../" prefix
    assertf(lienrelatif(s, sizeof(s), "a.html", "dir/index.html") == 0);
    assertf(strcmp(s, "../a.html") == 0);
  }
}

/* Self-tests for the htssafe.h bounded string ops.
   Returns 0 if every bounded operation behaved correctly, 1 otherwise.
   The abort-on-overflow guarantee is checked separately by the "overflow"
   sub-mode (it aborts the process by design). */
static int string_safety_selftests(void) {
  char buf[8];

  /* strcpybuff into a sized array: exact copy */
  strcpybuff(buf, "abc");
  if (strcmp(buf, "abc") != 0)
    return 1;

  /* strcatbuff append within capacity */
  strcatbuff(buf, "de");
  if (strcmp(buf, "abcde") != 0)
    return 1;

  /* strncatbuff appends at most N source chars */
  strcpybuff(buf, "ab");
  strncatbuff(buf, "cdef", 2);
  if (strcmp(buf, "abcd") != 0)
    return 1;

  /* strlcpybuff: explicit-capacity copy into a pointer destination, the form
     the migration moves toward */
  {
    char storage[8];
    char *const p = storage;

    strlcpybuff(p, "hello", sizeof(storage));
    if (strcmp(p, "hello") != 0)
      return 1;
  }

  /* strcpybuff into a pointer destination: routes through the unchecked
     strcpybuff_ptr_ fallback (the path the overflow warning flags). The warning
     is intentional here; we only verify the fallback still copies correctly. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattribute-warning"
#endif
  {
    char storage[8];
    char *const p = storage;

    strcpybuff(p, "ptr");
    if (strcmp(p, "ptr") != 0)
      return 1;
  }
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

  /* htsbuff: bounded builder over a fixed array (append, truncating append,
     reset, and length tracking) */
  {
    char dst[8];
    htsbuff b = htsbuff_array(dst);

    htsbuff_cat(&b, "ab");
    htsbuff_cat(&b, "cd");
    if (strcmp(htsbuff_str(&b), "abcd") != 0 || b.len != 4)
      return 1;

    htsbuff_catn(&b, "efghij", 2); /* append at most 2 */
    if (strcmp(htsbuff_str(&b), "abcdef") != 0)
      return 1;

    htsbuff_cpy(&b, "xyz"); /* reset */
    if (strcmp(htsbuff_str(&b), "xyz") != 0 || b.len != 3)
      return 1;

    htsbuff_catc(&b, '!'); /* single character */
    if (strcmp(htsbuff_str(&b), "xyz!") != 0 || b.len != 4)
      return 1;
  }

  /* boundary: filling to exactly cap-1 must succeed (one more aborts, which the
     overflow-buff mode checks) */
  {
    char d2[4];
    htsbuff c = htsbuff_array(d2);

    htsbuff_cat(&c, "abc");
    if (strcmp(htsbuff_str(&c), "abc") != 0 || c.len != 3)
      return 1;
  }

  return 0;
}

/* ------------------------------------------------------------ */
/* The individual self-tests. Each runs over argv[0..argc-1] and returns the */
/* process exit code (0 == success); a result line goes to stdout. */
/* ------------------------------------------------------------ */

static int st_filter(httrackp *opt, int argc, char **argv) {
  char *str, *pat;
  int matched;

  (void) opt;
  if (argc < 2) {
    fprintf(stderr, "filter: needs a filter pattern and a string\n");
    return 1;
  }
  /* exact-size heap copies so a sanitizer traps any over-read of the pattern */
  str = strdupt(argv[1]);
  pat = strdupt(argv[0]);
  matched = strjoker(str, pat, NULL, NULL) != NULL;
  printf("%s does %s %s\n", argv[1], matched ? "match" : "NOT match", argv[0]);
  freet(str);
  freet(pat);
  return 0;
}

/* Size-aware filter verdict via fa_strjoker: a negative <size> means the size
   is still unknown (scan time), so a size rule like -*.jpg*[<10] must stay
   neutral. */
static int st_filtersize(httrackp *opt, int argc, char **argv) {
  LLint sz;
  int size_flag = 0, verdict, known;

  (void) opt;
  if (argc < 3) {
    fprintf(stderr, "filtersize: needs <size> <string> <filter> [filter...]\n");
    return 1;
  }
  known = (argv[0][0] != '-'); /* "-1"/"-" => size unknown */
  sz = -1;
  if (known)
    sscanf(argv[0], LLintP, &sz);
  verdict = fa_strjoker(0, &argv[2], argc - 2, argv[1], known ? &sz : NULL,
                        known ? &size_flag : NULL, NULL);
  printf("verdict=%s size_flag=%d\n",
         verdict > 0   ? "allowed"
         : verdict < 0 ? "forbidden"
                       : "unknown",
         size_flag);
  return 0;
}

static int st_simplify(httrackp *opt, int argc, char **argv) {
  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "simplify: needs a path\n");
    return 1;
  }
  fil_simplifie(argv[0]);
  printf("simplified=%s\n", argv[0]);
  return 0;
}

static int st_mime(httrackp *opt, int argc, char **argv) {
  char mime[256];

  if (argc < 1) {
    fprintf(stderr, "mime: needs a filename\n");
    return 1;
  }
  if (get_httptype_sized(opt, mime, sizeof(mime), argv[0], 0)) {
    char ext[256];

    printf("%s is '%s'\n", argv[0], mime);
    if (give_mimext(ext, sizeof(ext), mime))
      printf("and its local type is '.%s'\n", ext);
  } else {
    printf("%s is of an unknown MIME type\n", argv[0]);
  }
  return 0;
}

static int st_charset(httrackp *opt, int argc, char **argv) {
  char *s;

  (void) opt;
  if (argc < 2) {
    fprintf(stderr, "charset: needs a charset and a string\n");
    return 1;
  }
  s = hts_convertStringToUTF8(argv[1], strlen(argv[1]), argv[0]);
  if (s != NULL) {
    printf("%s\n", s);
    freet(s);
  } else {
    fprintf(stderr, "invalid string for charset %s\n", argv[0]);
  }
  return 0;
}

static int st_idna_encode(httrackp *opt, int argc, char **argv) {
  char *s;

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "idna-encode: needs a hostname\n");
    return 1;
  }
  s = hts_convertStringUTF8ToIDNA(argv[0], strlen(argv[0]));
  if (s != NULL) {
    printf("%s\n", s);
    freet(s);
  } else {
    fprintf(stderr, "invalid string '%s'\n", argv[0]);
  }
  return 0;
}

static int st_idna_decode(httrackp *opt, int argc, char **argv) {
  char *s;

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "idna-decode: needs a hostname\n");
    return 1;
  }
  s = hts_convertStringIDNAToUTF8(argv[0], strlen(argv[0]));
  if (s != NULL) {
    printf("%s\n", s);
    freet(s);
  } else {
    fprintf(stderr, "invalid string '%s'\n", argv[0]);
  }
  return 0;
}

static int st_entities(httrackp *opt, int argc, char **argv) {
  char *s;
  const char *enc;

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "entities: needs a string\n");
    return 1;
  }
  s = strdupt(argv[0]);
  enc = argc >= 2 ? argv[1] : "UTF-8";
  if (s != NULL && hts_unescapeEntitiesWithCharset(s, s, strlen(s), enc) == 0) {
    printf("%s\n", s);
    freet(s);
  } else {
    fprintf(stderr, "invalid string '%s'\n", argv[0]);
  }
  return 0;
}

static int st_hashtable(httrackp *opt, int argc, char **argv) {
  char *snum;
  unsigned long count = 0;
  const char *const names[] = {
      "",        "add",         "delete",        "dry-add",
      "dry-del", "test-exists", "test-not-exist"};

  const struct {
    enum {
      DO_END,
      DO_ADD,
      DO_DEL,
      DO_DRY_ADD,
      DO_DRY_DEL,
      TEST_ADD,
      TEST_DEL
    } type;

    size_t modulus;
    size_t offset;
  } bench[] = {{DO_ADD, 4, 0},     /* add 4/0 */
               {TEST_ADD, 4, 0},   /* check 4/0 */
               {TEST_DEL, 4, 1},   /* check 4/1 */
               {TEST_DEL, 4, 2},   /* check 4/2 */
               {TEST_DEL, 4, 3},   /* check 4/3 */
               {DO_DRY_DEL, 4, 1}, /* del 4/1 */
               {DO_DRY_DEL, 4, 2}, /* del 4/2 */
               {DO_DRY_DEL, 4, 3}, /* del 4/3 */
               {DO_ADD, 4, 1},     /* add 4/1 */
               {DO_DRY_ADD, 4, 1}, /* add 4/1 */
               {TEST_ADD, 4, 0},   /* check 4/0 */
               {TEST_ADD, 4, 1},   /* check 4/1 */
               {TEST_DEL, 4, 2},   /* check 4/2 */
               {TEST_DEL, 4, 3},   /* check 4/3 */
               {DO_ADD, 4, 2},     /* add 4/2 */
               {DO_DRY_DEL, 4, 3}, /* del 4/3 */
               {DO_ADD, 4, 3},     /* add 4/3 */
               {DO_DEL, 4, 3},     /* del 4/3 */
               {TEST_ADD, 4, 0},   /* check 4/0 */
               {TEST_ADD, 4, 1},   /* check 4/1 */
               {TEST_ADD, 4, 2},   /* check 4/2 */
               {TEST_DEL, 4, 3},   /* check 4/3 */
               {DO_DEL, 4, 0},     /* del 4/0 */
               {DO_DEL, 4, 1},     /* del 4/1 */
               {DO_DEL, 4, 2},     /* del 4/2 */
               /* empty here */
               {TEST_DEL, 1, 0},  /* check */
               {DO_ADD, 4, 0},    /* add 4/0 */
               {DO_ADD, 4, 1},    /* add 4/1 */
               {DO_ADD, 4, 2},    /* add 4/2 */
               {DO_DEL, 42, 0},   /* add 42/0 */
               {TEST_DEL, 42, 0}, /* check 42/0 */
               {TEST_ADD, 42, 2}, /* check 42/2 */
               {DO_END}};

  char *buff = NULL;
  const char **strings = NULL;

  (void) opt;
  basic_selftests();
  if (argc < 1) {
    fprintf(stderr, "hashtable: needs a count or a file\n");
    exit(EXIT_FAILURE);
  }
  snum = strdupt(argv[0]);

  /* produce key #i */
#define FMT()                                                                  \
  char buffer[256];                                                            \
  const char *name;                                                            \
  const long expected = (long) i * 1664525 + 1013904223;                       \
  do {                                                                         \
    if (strings == NULL) {                                                     \
      snprintf(buffer, sizeof(buffer),                                         \
               "http://www.example.com/website/sample/for/hashtable/"          \
               "%ld/index.html?foo=%ld&bar",                                   \
               (long) i, (long) (expected));                                   \
      name = buffer;                                                           \
    } else {                                                                   \
      name = strings[i];                                                       \
    }                                                                          \
  } while (0)

  /* produce random patterns, or read from a file */
  if (sscanf(snum, "%lu", &count) != 1) {
    const off_t size = fsize(snum);
    FILE *fp = fopen(snum, "rb");
    if (fp != NULL) {
      buff = malloct(size);
      if (buff != NULL && fread(buff, 1, size, fp) == size) {
        size_t capa = 0;
        size_t i, last;
        for (i = 0, last = 0, count = 0; i < size; i++) {
          if (buff[i] == 10 || buff[i] == 0) {
            buff[i] = '\0';
            if (capa == count) {
              if (capa == 0) {
                capa = 16;
              } else {
                capa <<= 1;
              }
              strings = (const char **) realloct((void *) strings,
                                                 capa * sizeof(char *));
            }
            strings[count++] = &buff[last];
            last = i + 1;
          }
        }
      }
      fclose(fp);
    }
  }

  /* successfully read */
  if (count > 0) {
    coucal hashtable = coucal_new(0);
    size_t loop;
    for (loop = 0; bench[loop].type != DO_END; loop++) {
      size_t i;
      for (i = bench[loop].offset; i < (size_t) count;
           i += bench[loop].modulus) {
        int result;
        FMT();
        if (bench[loop].type == DO_ADD || bench[loop].type == DO_DRY_ADD) {
          size_t k;
          result = coucal_write(hashtable, name, (uintptr_t) expected);
          for (k = 0; k < /* stash_size*2 */ 32; k++) {
            (void) coucal_write(hashtable, name, (uintptr_t) expected);
          }
          /* revert logic */
          if (bench[loop].type == DO_DRY_ADD) {
            result = result ? 0 : 1;
          }
        } else if (bench[loop].type == DO_DEL ||
                   bench[loop].type == DO_DRY_DEL) {
          size_t k;
          result = coucal_remove(hashtable, name);
          for (k = 0; k < /* stash_size*2 */ 32; k++) {
            (void) coucal_remove(hashtable, name);
          }
          /* revert logic */
          if (bench[loop].type == DO_DRY_DEL) {
            result = result ? 0 : 1;
          }
        } else if (bench[loop].type == TEST_ADD ||
                   bench[loop].type == TEST_DEL) {
          intptr_t value = -1;
          result = coucal_readptr(hashtable, name, &value);
          if (bench[loop].type == TEST_ADD && result && value != expected) {
            fprintf(stderr, "value failed for %s (expected %ld, got %ld)\n",
                    name, (long) expected, (long) value);
            exit(EXIT_FAILURE);
          }
          /* revert logic */
          if (bench[loop].type == TEST_DEL) {
            result = result ? 0 : 1;
          }
        }
        if (!result) {
          fprintf(stderr,
                  "failed %s{%d/+%d} test on loop %ld"
                  " at offset %ld for %s\n",
                  names[bench[loop].type], (int) bench[loop].modulus,
                  (int) bench[loop].offset, (long) loop, (long) i, name);
          exit(EXIT_FAILURE);
        }
      }
    }
    coucal_delete(&hashtable);
    fprintf(stderr, "all hashtable tests were successful!\n");
  } else {
    fprintf(stderr, "Malformed number\n");
    exit(EXIT_FAILURE);
  }
#undef FMT
  return 0;
}

static int st_strsafe(httrackp *opt, int argc, char **argv) {
  (void) opt;
  if (argc >= 1 && strncmp(argv[0], "overflow", 8) == 0) {
    /* Deliberately exceed a sized buffer: the bounded op must abort. The source
       comes from argv so its length is opaque to the compiler (no static
       -Wstringop-overflow, genuine runtime check). "overflow-buff" exercises
       htsbuff. */
    char small[4];
    const char *const src = (argc >= 2) ? argv[1] : "overflowing";

    if (strcmp(argv[0], "overflow-buff") == 0) {
      htsbuff b = htsbuff_array(small);

      htsbuff_cat(&b, src);
    } else {
      strcpybuff(small, src);
    }
    printf("strsafe: NOT aborted\n"); /* must be unreachable */
    return 1;
  } else {
    const int err = string_safety_selftests();

    printf("strsafe: %s\n", err ? "FAIL" : "OK");
    return err;
  }
}

static int st_copyopt(httrackp *opt, int argc, char **argv) {
  httrackp *from = hts_create_opt();
  httrackp *to = hts_create_opt();
  int err = 0;

  (void) opt;
  (void) argc;
  (void) argv;
  /* from-values differ from both the to-values and the hts_create_opt()
     defaults (nearlink FALSE, errpage/parseall TRUE), so a copy that no-ops or
     just resets to defaults is caught too, not only the unsigned-guard bug. */
  from->retry = 7; /* int field: positive control */
  to->retry = 0;
  from->nearlink = HTS_TRUE;
  to->nearlink = HTS_FALSE;
  from->errpage = HTS_FALSE;
  to->errpage = HTS_TRUE;
  from->parseall = HTS_FALSE;
  to->parseall = HTS_TRUE;

  copy_htsopt(from, to);

  if (to->retry != 7)
    err = 1;
  if (to->nearlink != HTS_TRUE)
    err = 1;
  if (to->errpage != HTS_FALSE)
    err = 1;
  if (to->parseall != HTS_FALSE)
    err = 1;

  /* HTS_DEFAULT (-1) is "unspecified": copy_htsopt must skip it, leaving the
     target intact. Only a signed (int-backed) field can hold -1, so this also
     guards the type against regressing to an unsigned hts_boolean. */
  from->parseall = HTS_DEFAULT;
  to->parseall = HTS_TRUE;
  copy_htsopt(from, to);
  if (to->parseall != HTS_TRUE)
    err = 1;

  /* String field: a non-empty source deep-copies across, an empty source
     leaves the target intact (StringNotEmpty guard). Covers the exported
     copy_htsopt String path that no crawl test reaches. */
  StringCopy(from->cookies_file, "/tmp/jar.txt");
  StringCopy(to->cookies_file, "");
  copy_htsopt(from, to);
  if (strcmp(StringBuff(to->cookies_file), "/tmp/jar.txt") != 0)
    err = 1;
  StringCopy(from->cookies_file, "");
  copy_htsopt(from, to);
  if (strcmp(StringBuff(to->cookies_file), "/tmp/jar.txt") != 0)
    err = 1;

  /* #185 pause pair: copied when enabled (max>0), the 0 sentinel skips */
  from->pause_min_ms = 5000;
  from->pause_max_ms = 10000;
  to->pause_min_ms = to->pause_max_ms = 0;
  copy_htsopt(from, to);
  if (to->pause_min_ms != 5000 || to->pause_max_ms != 10000)
    err = 1;
  from->pause_min_ms = from->pause_max_ms = 0;
  copy_htsopt(from, to);
  if (to->pause_min_ms != 5000 || to->pause_max_ms != 10000)
    err = 1;

  hts_free_opt(from);
  hts_free_opt(to);
  printf("copy-htsopt: %s\n", err ? "FAIL" : "OK");
  return err;
}

static int st_pause(httrackp *opt, int argc, char **argv) {
  int err = 0, i, seen_low = 0, seen_high = 0;

  (void) opt;
  (void) argc;
  (void) argv;
  /* Consecutive-ms seeds (production shape: launch timestamps a few ms apart)
     must stay in range and spread, not collapse to a bound -- worst case for a
     weak low-bit mixer. */
  for (i = 0; i < 10000; i++) {
    int t = hts_pause_target_ms((TStamp) (1719500000000LL + i), 5000, 10000);

    if (t < 5000 || t > 10000)
      err = 1;
    seen_low |= (t < 6000);
    seen_high |= (t > 9000);
  }
  if (!seen_low || !seen_high)
    err = 1;
  if (hts_pause_target_ms(12345, 8000, 8000) != 8000) /* equal bounds = fixed */
    err = 1;
  /* deterministic: a seed yields the same target even after an intervening call
     with another seed (no global PRNG state to perturb it) */
  {
    int a = hts_pause_target_ms(99, 5000, 10000);

    (void) hts_pause_target_ms(54321, 5000, 10000);
    if (hts_pause_target_ms(99, 5000, 10000) != a)
      err = 1;
  }
  printf("pause: %s\n", err ? "FAIL" : "OK");
  return err;
}

static int st_relative(httrackp *opt, int argc, char **argv) {
  char s[HTS_URLMAXSIZE * 2];

  (void) opt;
  if (argc < 2) {
    fprintf(stderr, "relative: needs a link and a current-file path\n");
    return 1;
  }
  if (lienrelatif(s, sizeof(s), argv[0], argv[1]) == 0)
    printf("relative=%s\n", s);
  else
    printf("relative=<ERROR>\n");
  return 0;
}

static int st_resolve(httrackp *opt, int argc, char **argv) {
  lien_adrfil af;
  int r;

  (void) opt;
  if (argc < 3) {
    fprintf(stderr, "resolve: needs a link, an origin address and file\n");
    return 1;
  }
  r = ident_url_relatif(argv[0], argv[1], argv[2], &af);
  if (r == 0)
    printf("adr=%s fil=%s\n", af.adr, af.fil);
  else
    printf("error=%d\n", r);
  return 0;
}

static int st_savename(httrackp *opt, int argc, char **argv) {
  lien_adrfilsave afs;
  cache_back cache;
  struct_back *sback;
  hash_struct hash;
  lien_back headers;

  if (argc < 2) {
    fprintf(stderr, "savename: needs a fil and a content-type\n");
    return 1;
  }
  memset(&afs, 0, sizeof(afs));
  strcpybuff(afs.af.adr, "www.example.com");
  strcpybuff(afs.af.fil, argv[0]);

  memset(&cache, 0, sizeof(cache));
  cache.hashtable = (void *) coucal_new(0);

  sback = back_new(opt, opt->maxsoc * 32 + 1024);
  hash_init(opt, &hash, opt->urlhack);

  memset(&headers, 0, sizeof(headers));
  headers.status = 0;
  headers.r.statuscode = HTTP_OK;
  strcpybuff(headers.r.contenttype, argv[1]);
  strcpybuff(headers.url_fil, argv[0]);

  url_savename(&afs, NULL, NULL, NULL, opt, sback, &cache, &hash, 0, 0,
               &headers);
  printf("savename: %s\n", afs.save);
  return 0;
}

static int st_cache(httrackp *opt, int argc, char **argv) {
  int err;

  if (argc < 1) {
    fprintf(stderr, "cache: needs a directory\n");
    return 1;
  }
  err = cache_selftests(opt, argv[0]);
  printf("cache-selftest: %s\n", err ? "FAIL" : "OK");
  return err;
}

static int st_cache_golden(httrackp *opt, int argc, char **argv) {
  int regen, err;

  if (argc < 1) {
    fprintf(stderr, "cache-golden: needs a directory\n");
    return 1;
  }
  regen = (argc >= 2 && strcmp(argv[1], "regen") == 0);
  err = cache_golden_selftest(opt, argv[0], regen);
  printf("cache-golden: %s\n", err ? "FAIL" : "OK");
  return err;
}

static int st_cache_writefail(httrackp *opt, int argc, char **argv) {
  int err;

  if (argc < 1) {
    fprintf(stderr, "cache-writefail: needs a directory\n");
    return 1;
  }
  err = cache_write_failure_selftest(opt, argv[0]);
  printf("cache-writefail: %s\n", err ? "FAIL" : "OK");
  return err;
}

static int st_dns(httrackp *opt, int argc, char **argv) {
  const int err = dns_selftests(opt);

  (void) argc;
  (void) argv;
  printf("dns-selftest: %s\n", err ? "FAIL" : "OK");
  return err;
}

static int st_cookies(httrackp *opt, int argc, char **argv) {
  static t_cookie cookie;
  char hdr[1024];
  /* RFC 6265: bare name=value pairs, no $Version/$Path (#151). */
  const char *expected = "Cookie: name=value; has_js=1" H_CRLF;
  const char *dom = "www.example.com";
  int err = 0;
  int added;

  (void) opt;
  (void) argc;
  (void) argv;
  cookie.max_len = (int) sizeof(cookie.data);
  cookie.data[0] = '\0';
  added = cookie_add(&cookie, "name", "value", dom, "/");
  added |= cookie_add(&cookie, "has_js", "1", dom, "/");
  /* different domain: must be filtered out */
  added |= cookie_add(&cookie, "junk", "x", "other.org", "/");
  if (added) {
    printf("cookie-header: FAIL (cookie_add setup)\n");
    return 1;
  }

  http_cookie_header_selftest(&cookie, dom, "/", hdr, sizeof(hdr));
  if (strcmp(hdr, expected) != 0)
    err = 1;
  if (strstr(hdr, "$Version") != NULL || strstr(hdr, "$Path") != NULL)
    err = 1;
  if (strstr(hdr, "junk") != NULL) // wrong-domain cookie leaked
    err = 1;
  printf("cookie-header: %s\n", err ? "FAIL" : "OK");
  if (err)
    printf("  got: %s\n", hdr);
  return err;
}

/* --strip-query: resolver + fil_normalized_filtered, end to end. */
static int st_stripquery(httrackp *opt, int argc, char **argv) {
  char dest[1024], keys[256], ref[1024];
  const char *k;

  (void) opt;
  (void) argc;
  (void) argv;

  /* empty rules == plain fil_normalized */
  assertf(hts_query_strip_keys(NULL, "h.com", "/p?a=1", keys, sizeof(keys)) ==
          NULL);
  assertf(hts_query_strip_keys("", "h.com", "/p?a=1", keys, sizeof(keys)) ==
          NULL);
  assertf(strcmp(fil_normalized_filtered("/p?b=2&a=1", dest, NULL),
                 fil_normalized("/p?b=2&a=1", ref)) == 0);

  /* bare form (*=keys): strip the key everywhere, keep+sort the rest */
  k = hts_query_strip_keys("sid", "any.com", "/p?b=2&sid=x&a=1", keys,
                           sizeof(keys));
  assertf(k != NULL && strcmp(k, "sid") == 0);
  assertf(strcmp(fil_normalized_filtered("/p?b=2&sid=x&a=1", dest, k),
                 "/p?a=1&b=2") == 0);

  /* reordered variant + an extra stripped key == the clean URL */
  assertf(strcmp(fil_normalized_filtered("/p?sid=y&a=1&b=2", dest, "sid"),
                 fil_normalized("/p?a=1&b=2", ref)) == 0);

  /* host pattern matches only that host, incl. its www-normalized forms */
  assertf(hts_query_strip_keys("ex.com/*=utm", "other.com", "/p?utm=1", keys,
                               sizeof(keys)) == NULL);
  assertf(hts_query_strip_keys("ex.com/*=utm", "ex.com", "/p?utm=1", keys,
                               sizeof(keys)) != NULL);
  assertf(hts_query_strip_keys("ex.com/*=utm", "www.ex.com", "/p?utm=1", keys,
                               sizeof(keys)) != NULL);
  assertf(hts_query_strip_keys("ex.com/*=utm", "http://www-3.ex.com",
                               "/p?utm=1", keys, sizeof(keys)) != NULL);

  /* last match wins, wholesale: host rule overrides global, no union */
  k = hts_query_strip_keys("*=sid\nex.com/*=utm", "ex.com",
                           "/p?sid=1&utm=2&a=3", keys, sizeof(keys));
  assertf(k != NULL && strcmp(k, "utm") == 0);
  assertf(strcmp(fil_normalized_filtered("/p?sid=1&utm=2&a=3", dest, k),
                 "/p?a=3&sid=1") == 0);
  k = hts_query_strip_keys("*=sid\nex.com/*=utm", "z.com", "/p?sid=1&a=3", keys,
                           sizeof(keys));
  assertf(k != NULL && strcmp(k, "sid") == 0);

  /* whole-key match, not prefix: "utm" must not strip utm_source */
  assertf(strcmp(fil_normalized_filtered("/p?utm_source=x&a=1", dest, "utm"),
                 "/p?a=1&utm_source=x") == 0);

  /* "*" drops every param; a fully-stripped single-arg query loses its '?' */
  assertf(strcmp(fil_normalized_filtered("/p?a=1&b=2", dest, "*"), "/p") == 0);
  assertf(strcmp(fil_normalized_filtered("/p?utm=1", dest, "utm"), "/p") == 0);

  /* degenerate forms a=, b, c== (key 'c'); strip c keeps a= and b */
  assertf(strcmp(fil_normalized_filtered("/p?a=&b&c==", dest, "c"),
                 "/p?a=&b") == 0);
  /* short key must not strip a longer one: 'c' must not touch 'cc' */
  assertf(strcmp(fil_normalized_filtered("/p?cc=1&c=2", dest, "c"),
                 "/p?cc=1") == 0);

  /* repeated key: every occurrence is stripped, not just the first */
  assertf(
      strcmp(fil_normalized_filtered("/p?foo=42&bar=13&foo=43", dest, "foo"),
             "/p?bar=13") == 0);
  /* repeated key mixing missing/empty values */
  assertf(
      strcmp(fil_normalized_filtered("/p?foo&bar=13&foo=42&foo=", dest, "foo"),
             "/p?bar=13") == 0);
  /* repeated key kept (no match): all occurrences retained, then sorted */
  assertf(strcmp(fil_normalized_filtered("/p?foo=42&bar=13&foo=43", dest, "z"),
                 "/p?bar=13&foo=42&foo=43") == 0);

  /* value containing '=': the key is only the part before the first '='. Strip
     'foo' drops "foo=42=17" whole; the '=' in the value is not a delimiter. */
  assertf(strcmp(fil_normalized_filtered("/p?foo=42=17&bar=", dest, "foo"),
                 "/p?bar=") == 0);
  /* keeping it preserves the embedded '=' verbatim */
  assertf(strcmp(fil_normalized_filtered("/p?foo=42=17&bar=", dest, "bar"),
                 "/p?foo=42=17") == 0);
  /* a value segment is not a key: stripping "42" must not touch foo=42=17 */
  assertf(strcmp(fil_normalized_filtered("/p?foo=42=17", dest, "42"),
                 "/p?foo=42=17") == 0);

  /* Idempotency: the read path re-normalizes an already-normalized fil, so the
     result must be a fixpoint or dedup misses (catches a dropped empty/trailing
     arg like "?&&", "a&"). */
  {
    static const char *const qs[] = {"/p?a=&b&c==",
                                     "/p?a&&b",
                                     "/p?&a",
                                     "/p?a&",
                                     "/p?",
                                     "/p?=v",
                                     "/p?&&",
                                     "/p?b=2&a=1",
                                     "/p?utm=x&",
                                     "/p?&utm=x",
                                     "/p?foo=42&bar=13&foo=43",
                                     "/p?foo&bar=13&foo=42&foo=",
                                     "/p?foo=42=17&bar="};
    static const char *const strips[] = {NULL, "z", "utm", "*", "a", "foo"};
    char once[1024], twice[1024];
    size_t i, j;

    for (i = 0; i < sizeof(qs) / sizeof(qs[0]); i++) {
      for (j = 0; j < sizeof(strips) / sizeof(strips[0]); j++) {
        fil_normalized_filtered(qs[i], once, strips[j]);
        fil_normalized_filtered(once, twice, strips[j]);
        assertf(strcmp(once, twice) == 0);
      }
    }
  }

  printf("strip-query self-test OK\n");
  return 0;
}

/* -%u url-hack split (#271): each sub-flag must toggle independently. */
static int st_urlhack(httrackp *opt, int argc, char **argv) {
  (void) argc;
  (void) argv;
#define EQ(aa, fa, ab, fb) hash_url_equals(opt, aa, fa, ab, fb)
  /* urlhack on, no opt-outs: www, // and query order all collapse */
  opt->urlhack = HTS_TRUE;
  opt->no_www_dedup = opt->no_slash_dedup = opt->no_query_dedup = HTS_FALSE;
  assertf(EQ("www.foo.com", "/a", "foo.com", "/a"));
  assertf(EQ("foo.com", "/a//b", "foo.com", "/a/b"));
  assertf(EQ("foo.com", "/p?b=2&a=1", "foo.com", "/p?a=1&b=2"));

  /* keep-www-prefix: host off; // and query still collapse */
  opt->no_www_dedup = HTS_TRUE;
  assertf(!EQ("www.foo.com", "/a", "foo.com", "/a"));
  assertf(EQ("foo.com", "/a//b", "foo.com", "/a/b"));
  assertf(EQ("foo.com", "/p?b=2&a=1", "foo.com", "/p?a=1&b=2"));
  opt->no_www_dedup = HTS_FALSE;

  /* keep-double-slashes: // significant; www, query order still collapse */
  opt->no_slash_dedup = HTS_TRUE;
  assertf(!EQ("foo.com", "/a//b", "foo.com", "/a/b"));
  assertf(EQ("www.foo.com", "/a", "foo.com", "/a"));
  assertf(EQ("foo.com", "/p?b=2&a=1", "foo.com", "/p?a=1&b=2"));
  opt->no_slash_dedup = HTS_FALSE;

  /* keep-query-order: query order significant; www and // still collapse */
  opt->no_query_dedup = HTS_TRUE;
  assertf(!EQ("foo.com", "/p?b=2&a=1", "foo.com", "/p?a=1&b=2"));
  assertf(EQ("www.foo.com", "/a", "foo.com", "/a"));
  assertf(EQ("foo.com", "/a//b", "foo.com", "/a/b"));
  opt->no_query_dedup = HTS_FALSE;

  /* all opt-outs == urlhack off entirely */
  opt->no_www_dedup = opt->no_slash_dedup = opt->no_query_dedup = HTS_TRUE;
  assertf(!EQ("www.foo.com", "/a", "foo.com", "/a"));
  assertf(!EQ("foo.com", "/a//b", "foo.com", "/a/b"));
  assertf(!EQ("foo.com", "/p?b=2&a=1", "foo.com", "/p?a=1&b=2"));
  opt->urlhack = HTS_FALSE;
  opt->no_www_dedup = opt->no_slash_dedup = opt->no_query_dedup = HTS_FALSE;
  assertf(!EQ("www.foo.com", "/a", "foo.com", "/a"));
  assertf(!EQ("foo.com", "/a//b", "foo.com", "/a/b"));
#undef EQ
  printf("urlhack self-test OK\n");
  return 0;
}

/* Default User-Agent: honest HTTrack token, no resurrected Windows 98. */
static int st_useragent(httrackp *opt, int argc, char **argv) {
  const char *ua = StringBuff(opt->user_agent);
  (void) argc;
  (void) argv;
  assertf(ua != NULL);
  assertf(strcmp(ua, HTS_DEFAULT_USER_AGENT) == 0);
  /* Teeth independent of the macro: honest token + self-identifier, and no
     legacy Mozilla/4.x fake-browser string (rejects the whole relic family). */
  assertf(strstr(ua, "HTTrack/") != NULL);
  assertf(strstr(ua, "+https://www.httrack.com/") != NULL);
  assertf(strstr(ua, "Mozilla/4.") == NULL);
  printf("useragent self-test OK: %s\n", ua);
  return 0;
}

/* HTTP status code -> reason phrase, including the modern 429/451. */
static int st_status(httrackp *opt, int argc, char **argv) {
  const char *s;
  (void) opt;
  (void) argc;
  (void) argv;
  s = infostatuscode_const(429);
  assertf(s != NULL && strcmp(s, "Too Many Requests") == 0);
  s = infostatuscode_const(451);
  assertf(s != NULL && strcmp(s, "Unavailable For Legal Reasons") == 0);
  /* A spot-check of a long-standing code, and an unknown one. */
  s = infostatuscode_const(404);
  assertf(s != NULL && strcmp(s, "Not Found") == 0);
  assertf(infostatuscode_const(799) == NULL);
  printf("status self-test OK\n");
  return 0;
}

/* ------------------------------------------------------------ */
/* Registry: name -> handler, with a usage hint and a one-line description. */
/* ------------------------------------------------------------ */

static const struct selftest_entry {
  const char *name;
  const char *args;
  const char *desc;
  int (*fn)(httrackp *opt, int argc, char **argv);
} selftests[] = {
    {"filter", "<pattern> <string>", "match a string against a wildcard filter",
     st_filter},
    {"filtersize", "<size> <string> <filter>...",
     "size-aware filter verdict (negative size = unknown/scan time)",
     st_filtersize},
    {"simplify", "<path>", "collapse ./ and ../ in a path", st_simplify},
    {"stripquery", "", "--strip-query pattern/key stripping self-test",
     st_stripquery},
    {"urlhack", "", "-%u url-hack sub-flag (www/slash/query) self-test",
     st_urlhack},
    {"mime", "<filename>", "MIME type for a filename", st_mime},
    {"charset", "<charset> <string>",
     "convert a string to UTF-8 from a charset", st_charset},
    {"idna-encode", "<host>", "encode a hostname to IDNA/punycode",
     st_idna_encode},
    {"idna-decode", "<host>", "decode an IDNA/punycode hostname",
     st_idna_decode},
    {"entities", "<string> [encoding]", "unescape HTML entities", st_entities},
    {"hashtable", "<count|file>", "coucal hashtable stress test", st_hashtable},
    {"strsafe", "[overflow|overflow-buff [str]]", "bounded string-op self-test",
     st_strsafe},
    {"copyopt", "", "copy_htsopt option-copy self-test", st_copyopt},
    {"pause", "", "randomized inter-file pause target self-test", st_pause},
    {"relative", "<link> <curr-file>", "relative link between two paths",
     st_relative},
    {"resolve", "<link> <adr> <fil>", "resolve a link against an origin",
     st_resolve},
    {"savename", "<fil> <content-type>", "local save-name for a URL",
     st_savename},
    {"cache", "<dir>", "cache read/write round-trip self-test", st_cache},
    {"cache-golden", "<dir> [regen]", "frozen cache-format read self-test",
     st_cache_golden},
    {"cache-writefail", "<dir>", "cache write-failure handling self-test",
     st_cache_writefail},
    {"dns", "", "DNS resolver/cache self-test", st_dns},
    {"cookies", "", "cookie request-header self-test", st_cookies},
    {"useragent", "", "default User-Agent self-test", st_useragent},
    {"status", "", "HTTP status code -> reason phrase self-test", st_status},
};

static void list_selftests(void) {
  size_t i;

  fprintf(stderr, "Engine self-tests (httrack -#test=NAME [args]):\n");
  for (i = 0; i < sizeof(selftests) / sizeof(selftests[0]); i++) {
    fprintf(stderr, "  %-16s %-32s %s\n", selftests[i].name, selftests[i].args,
            selftests[i].desc);
  }
}

int hts_selftest(httrackp *opt, const char *name, int argc, char **argv) {
  size_t i;

  if (name == NULL || name[0] == '\0' || strcmp(name, "list") == 0) {
    list_selftests();
    return 0;
  }
  for (i = 0; i < sizeof(selftests) / sizeof(selftests[0]); i++) {
    if (strcmp(name, selftests[i].name) == 0)
      return selftests[i].fn(opt, argc, argv);
  }
  fprintf(stderr, "Unknown self-test '%s'\n", name);
  list_selftests();
  return 1;
}
