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
#include "htsparse.h"
#include "htscache.h"
#include "htscache_selftest.h"
#include "htsdns_selftest.h"
#include "htscharset.h"
#include "htsencoding.h"
#include "htsftp.h"
#include "htsmd5.h"
#include "htssniff.h"
#if HTS_USEZLIB
#include "htszlib.h"
#endif
#include "coucal/coucal.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <unistd.h>
#endif

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

  /* StringCatN/StringSetLength must eval SIZE once: (n_eval++, V) leaves
     n_eval == 2 on a double-eval macro. */
  {
    String s = STRING_EMPTY;
    int n_eval = 0;

    StringCat(s, "hello");
    StringCatN(s, "world", (n_eval++, 3)); /* strlen>SIZE so the clamp runs */
    if (n_eval != 1 || strcmp(StringBuff(s), "hellowor") != 0) {
      StringFree(s);
      return 1;
    }

    n_eval = 0;
    StringSetLength(s, (n_eval++, 5));
    if (n_eval != 1 || StringLength(s) != 5) {
      StringFree(s);
      return 1;
    }
    StringFree(s);
  }

  /* StringSubRW still reads/writes after dropping its duplicate definition. */
  {
    String s = STRING_EMPTY;

    StringCat(s, "abc");
    StringSubRW(s, 1) = 'X';
    if (StringSub(s, 1) != 'X' || strcmp(StringBuff(s), "aXc") != 0) {
      StringFree(s);
      return 1;
    }
    StringFree(s);
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
  if (argc < 1) {
    fprintf(stderr, "filter: needs a filter pattern and a string\n");
    return 1;
  }
  /* exact-size heap copies so a sanitizer traps an over-read; a missing
     subject means "" (not reachable as a CLI arg) */
  str = strdupt(argc >= 2 ? argv[1] : "");
  pat = strdupt(argv[0]);
  matched = strjoker(str, pat, NULL, NULL) != NULL;
  printf("%s does %s %s\n", str, matched ? "match" : "NOT match", argv[0]);
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

/* Mime-type filter verdict via fa_strjoker(type=1): only mime: rules apply. */
static int st_filtermime(httrackp *opt, int argc, char **argv) {
  int verdict;

  (void) opt;
  if (argc < 2) {
    fprintf(stderr, "filtermime: needs <mime> <filter> [filter...]\n");
    return 1;
  }
  verdict = fa_strjoker(1, &argv[1], argc - 1, argv[0], NULL, NULL, NULL);
  printf("verdict=%s\n", verdict > 0   ? "allowed"
                         : verdict < 0 ? "forbidden"
                                       : "unknown");
  return 0;
}

/* SplitMix64: deterministic, platform-independent case generator. */
static uint64_t st_mix64(uint64_t *state) {
  uint64_t z = (*state += UINT64_C(0x9E3779B97F4A7C15));

  z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4B5B9);
  z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
  return z ^ (z >> 31);
}

/* Differential test: memoized strjoker vs the no-memo oracle on seeded random
   pattern/subject/size cases must agree on result, *size and *size_flag. */
static int st_filtermemo(httrackp *opt, int argc, char **argv) {
  static const char *const pieces[] = {
      "a",       "b",        "A",    "c",      ".",       "/",
      "?",       "*",        "*[a]", "*[ab]",  "*[a-c]",  "*[A-Z]",
      "*[<5]",   "*[>5]",    "*(a)", "*(a,b)", "*[file]", "*[path]",
      "*[name]", "*[param]", "*[]",  "*[\\a]", "*[a-"};
  static const char subject_chars[] = "abAc./?";
  uint64_t rng = UINT64_C(0x501);
  int iters = 20000, matched = 0, unmatched = 0;
  int it;

  (void) opt;
  if (argc >= 1)
    sscanf(argv[0], "%d", &iters);
  for (it = 0; it < iters; it++) {
    char pat[64], str[16];
    size_t pl = 0;
    const int npieces = (int) (st_mix64(&rng) % 5);
    const int slen = (int) (st_mix64(&rng) % 13);
    const int szsel = (int) (st_mix64(&rng) % 4); /* none/unknown/3/20 */
    LLint sz1, sz2, off1, off2;
    int i, flag1 = 0, flag2 = 0;
    char *hpat, *hstr;
    const char *adr;

    for (i = 0; i < npieces; i++) {
      const char *p =
          pieces[st_mix64(&rng) % (sizeof(pieces) / sizeof(pieces[0]))];
      const size_t len = strlen(p);

      if (len >= sizeof(pat) - pl)
        break;
      memcpy(pat + pl, p, len);
      pl += len;
    }
    pat[pl] = '\0';
    for (i = 0; i < slen; i++)
      str[i] = subject_chars[st_mix64(&rng) % (sizeof(subject_chars) - 1)];
    str[slen] = '\0';
    sz1 = sz2 = (szsel <= 1) ? -1 : (szsel == 2) ? 3 : 20;
    /* exact-size heap copies per run so a sanitizer traps any over-read */
    hpat = strdupt(pat);
    hstr = strdupt(str);
    adr = strjoker(hstr, hpat, szsel ? &sz1 : NULL, szsel ? &flag1 : NULL);
    off1 = adr != NULL ? (LLint) (adr - hstr) : -1;
    freet(hpat);
    freet(hstr);
    hpat = strdupt(pat);
    hstr = strdupt(str);
    adr =
        strjoker_nomemo(hstr, hpat, szsel ? &sz2 : NULL, szsel ? &flag2 : NULL);
    off2 = adr != NULL ? (LLint) (adr - hstr) : -1;
    freet(hpat);
    freet(hstr);
    if (off1 != off2 || sz1 != sz2 || flag1 != flag2) {
      printf("filtermemo MISMATCH pat=[%s] str=[%s] szsel=%d: "
             "off %d/%d size %d/%d flag %d/%d\n",
             pat, str, szsel, (int) off1, (int) off2, (int) sz1, (int) sz2,
             flag1, flag2);
      return 1;
    }
    if (off1 >= 0)
      matched++;
    else
      unmatched++;
  }
  /* both polarities must actually occur or the test proves nothing */
  assertf(matched > 0 && unmatched > 0);
  printf("filtermemo: %d cases OK\n", iters);
  return 0;
}

/* Merged two-form filter verdict via fa_strjoker_dual (see htsfilters.h). */
static int st_filterdual(httrackp *opt, int argc, char **argv) {
  int depth = -1, verdict;

  (void) opt;
  if (argc < 3) {
    fprintf(stderr,
            "filterdual: needs <string1> <string2> <filter> [filter...]\n");
    return 1;
  }
  verdict = fa_strjoker_dual(0, &argv[2], argc - 2, argv[0], argv[1], NULL,
                             NULL, &depth);
  printf("verdict=%s rule=%d\n",
         verdict > 0   ? "allowed"
         : verdict < 0 ? "forbidden"
                       : "unknown",
         depth);
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

static int st_metacharset(httrackp *opt, int argc, char **argv) {
  char *s;

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "metacharset: needs an html string\n");
    return 1;
  }
  s = hts_getCharsetFromMeta(argv[0], strlen(argv[0]));
  printf("%s\n", s != NULL ? s : "(none)");
  freet(s);
  return 0;
}

static int st_isutf8(httrackp *opt, int argc, char **argv) {
  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "isutf8: needs a string\n");
    return 1;
  }
  printf("%d\n", hts_isStringUTF8(argv[0], strlen(argv[0])) ? 1 : 0);
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
  if (s != NULL &&
      hts_unescapeEntitiesWithCharset(s, s, strlen(s) + 1, enc) == 0) {
    printf("%s\n", s);
    freet(s);
  } else {
    fprintf(stderr, "invalid string '%s'\n", argv[0]);
  }
  return 0;
}

/* The unescapers must reserve one byte for the trailing NUL: a 'max'-byte
   dest holding 'max' output chars pre-fix wrote dest[max] (1-byte OOB, caught
   by ASan). Both unescapeEntities and unescapeUrl share the guard. */
static int st_unescape_bounds(httrackp *opt, int argc, char **argv) {
  char dest[4];

  (void) opt;
  (void) argc;
  (void) argv;
  assertf(hts_unescapeEntities("abcd", dest, sizeof(dest)) == -1);
  assertf(hts_unescapeUrl("abcd", dest, sizeof(dest)) == -1);
  assertf(hts_unescapeEntities("abc", dest, sizeof(dest)) == 0);
  assertf(strcmp(dest, "abc") == 0);
  /* raw multi-byte UTF-8 flush path (bypasses the per-byte guard) */
  assertf(hts_unescapeUrl("ab\xC3\xA9", dest, sizeof(dest)) == -1);
  assertf(hts_unescapeUrl("a\xC3\xA9", dest, sizeof(dest)) == 0);
  assertf(strcmp(dest, "a\xC3\xA9") == 0);
  {
    /* %xx-encoded flush path (utfBufferJ = lastJ rollback) */
    char wide[8];

    assertf(hts_unescapeUrl("%C3%A9", wide, sizeof(wide)) == 0);
    assertf(strcmp(wide, "\xC3\xA9") == 0);
  }
  printf("unescape-bounds self-test OK\n");
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

/* Split a URL into (adr, fil), or print "error" if rejected. A second arg pads
   the URL with that many 'a's to reach lengths a CLI arg can't. */
static int st_identurl(httrackp *opt, int argc, char **argv) {
  lien_adrfil af;
  char *url;
  size_t len, pad = 0;

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "identurl: needs a URL\n");
    return 1;
  }
  if (argc >= 2)
    pad = (size_t) atoi(argv[1]);
  len = strlen(argv[0]);
  url = malloct(len + pad + 1);
  memcpy(url, argv[0], len);
  memset(url + len, 'a', pad);
  url[len + pad] = '\0';
  if (ident_url_absolute(url, &af) >= 0)
    printf("adr=%s fil=%s\n", af.adr, af.fil);
  else
    printf("error\n");
  freet(url);
  return 0;
}

/* Extra args are key=value: adr= cdispo= statuscode= status= strip= urlhack=
   no-www= no-slash= no-query= n83= type=, plus repeatable prior=adr|fil|sav
   registering an already-crawled link (dedup/collision paths). */
/* Parse raw response-header lines and print the naming-relevant fields. */
static int st_header(httrackp *opt, int argc, char **argv) {
  htsblk r;
  int i;

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "header: needs at least one raw header line\n");
    return 1;
  }
  memset(&r, 0, sizeof(r));
  for (i = 0; i < argc; i++) {
    char BIGSTK line[HTS_URLMAXSIZE * 2];

    strcpybuff(line, argv[i]);
    treathead(NULL, "www.example.com", "/", &r, line);
  }
  printf("contenttype=%s cdispo=%s\n", r.contenttype, r.cdispo);
  printf("contentencoding=%s\n", r.contentencoding);
  return 0;
}

/* An over-long header value must not overflow treathead's tempo[1100]. */
static int st_headerlong(httrackp *opt, int argc, char **argv) {
  htsblk r;
  char BIGSTK line[HTS_URLMAXSIZE * 2];
  const char *const name = argc >= 1 ? argv[0] : "Content-Type:";
  const int pad = 1500; /* > tempo[1100] */
  size_t n;

  (void) opt;
  memset(&r, 0, sizeof(r));
  n = (size_t) snprintf(line, sizeof(line), "%s ", name);
  memset(line + n, 'a', pad);
  line[n + pad] = '\0';
  treathead(NULL, "www.example.com", "/", &r, line);
  printf("contenttype_len=%d contentencoding_len=%d\n",
         (int) strlen(r.contenttype), (int) strlen(r.contentencoding));
  return 0;
}

/* Decode a body argument ("hex:FFD8.." or literal text) into buf. */
static size_t st_decode_body(const char *arg, char *buf, size_t size) {
  size_t n = 0;

  if (strncmp(arg, "hex:", 4) == 0) {
    const char *s = arg + 4;

    for (; s[0] != '\0' && s[1] != '\0' && n + 1 < size; s += 2) {
      unsigned int byte;

      if (sscanf(s, "%2x", &byte) != 1)
        break;
      buf[n++] = (char) byte;
    }
  } else {
    n = strlen(arg);
    if (n >= size)
      n = size - 1;
    memcpy(buf, arg, n);
  }
  buf[n] = '\0';
  return n;
}

static int st_sniff(httrackp *opt, int argc, char **argv) {
  char BIGSTK body[1024];
  size_t n;

  (void) opt;
  if (argc < 2) {
    fprintf(stderr, "sniff: needs a content-type and a body\n");
    return 1;
  }
  n = st_decode_body(argv[1], body, sizeof(body));
  printf("sniff: known=%d consistent=%d\n",
         hts_sniff_mime_known(argv[0]) == HTS_TRUE,
         hts_sniff_mime_consistent(body, n, argv[0]) == HTS_TRUE);
  return 0;
}

static int st_savename(httrackp *opt, int argc, char **argv) {
  lien_adrfilsave afs;
  cache_back cache;
  struct_back *sback;
  hash_struct hash;
  lien_back headers;
  const char *adr = "www.example.com";
  const char *cdispo = NULL;
  const char *body = NULL;
  const char *cached = NULL;
  const char *bodyfile = "st-savename-body.tmp";
  int statuscode = HTTP_OK, status = 0;
  int i;

  if (argc < 2) {
    fprintf(stderr, "savename: needs a fil and a content-type\n");
    return 1;
  }
  /* knobs first: hash_init and the prior links depend on them */
  for (i = 2; i < argc; i++) {
    const char *const a = argv[i];

    if (strncmp(a, "adr=", 4) == 0)
      adr = a + 4;
    else if (strncmp(a, "cdispo=", 7) == 0)
      cdispo = a + 7;
    else if (strncmp(a, "statuscode=", 11) == 0)
      statuscode = atoi(a + 11);
    else if (strncmp(a, "status=", 7) == 0)
      status = atoi(a + 7);
    else if (strncmp(a, "strip=", 6) == 0)
      StringCopy(opt->strip_query, a + 6);
    else if (strncmp(a, "urlhack=", 8) == 0)
      opt->urlhack = atoi(a + 8) ? HTS_TRUE : HTS_FALSE;
    else if (strncmp(a, "no-www=", 7) == 0)
      opt->no_www_dedup = atoi(a + 7) ? HTS_TRUE : HTS_FALSE;
    else if (strncmp(a, "no-slash=", 9) == 0)
      opt->no_slash_dedup = atoi(a + 9) ? HTS_TRUE : HTS_FALSE;
    else if (strncmp(a, "no-query=", 9) == 0)
      opt->no_query_dedup = atoi(a + 9) ? HTS_TRUE : HTS_FALSE;
    else if (strncmp(a, "n83=", 4) == 0)
      opt->savename_83 = atoi(a + 4);
    else if (strncmp(a, "type=", 5) == 0)
      opt->savename_type = atoi(a + 5);
    else if (strncmp(a, "body=", 5) == 0)
      body = a + 5;
    else if (strncmp(a, "cached=", 7) == 0)
      cached = a + 7;
    else if (strncmp(a, "prior=", 6) != 0) {
      fprintf(stderr, "savename: unknown arg '%s'\n", a);
      return 1;
    }
  }
  memset(&afs, 0, sizeof(afs));
  strcpybuff(afs.af.adr, adr);
  strcpybuff(afs.af.fil, argv[0]);

  memset(&cache, 0, sizeof(cache));
  if (cached != NULL) { /* cached=<content-type>|<save name> */
    char *dup = strdupt(cached);
    char *const sep = strchr(dup, '|');
    char locbuf[64] = "";
    htsblk cr;

    if (sep == NULL) {
      fprintf(stderr, "savename: cached needs ctype|save\n");
      return 1;
    }
    *sep = '\0';
    /* one-entry cache in cwd, reopened read-only; body is PNG magic on
       purpose: only the recorded name (X-Save) may drive the naming */
    StringCopy(opt->path_log, "");
    cache.type = 1;
    cache.log = cache.errlog = stderr;
    cache.hashtable = coucal_new(0);
    cache_init(&cache, opt);
    hts_init_htsblk(&cr);
    cr.statuscode = HTTP_OK;
    strcpybuff(cr.msg, "OK");
    strcpybuff(cr.contenttype, dup);
    cr.location = locbuf;
    cr.adr = strdupt("\x89PNG\r\n\x1a\n");
    cr.size = 8;
    cache_add(opt, &cache, &cr, adr, argv[0], sep + 1, 1, NULL);
    freet(cr.adr);
    if (cache.zipOutput != NULL) {
      zipClose(cache.zipOutput, NULL);
      cache.zipOutput = NULL;
    }
    memset(&cache, 0, sizeof(cache));
    cache.type = 1;
    cache.log = cache.errlog = stderr;
    cache.hashtable = coucal_new(0);
    cache.ro = 1;
    cache_init(&cache, opt);
    freet(dup);
  } else {
    cache.hashtable = (void *) coucal_new(0);
  }

  sback = back_new(opt, opt->maxsoc * 32 + 1024);
  /* same wiring as hts_mirror (htscore.c) */
  hash_init(opt, &hash, opt->urlhack);
  hash.liens = (const lien_url *const *const *) &opt->liens;
  opt->hash = &hash;
  hts_record_init(opt);

  for (i = 2; i < argc; i++) {
    if (strncmp(argv[i], "prior=", 6) == 0) {
      char *dup = strdupt(argv[i] + 6);
      char *const p1 = strchr(dup, '|');
      char *const p2 = p1 != NULL ? strchr(p1 + 1, '|') : NULL;

      if (p2 == NULL) {
        fprintf(stderr, "savename: prior needs adr|fil|sav\n");
        return 1;
      }
      *p1 = *p2 = '\0';
      if (!hts_record_link(opt, dup, p1 + 1, p2 + 1, "", "", NULL))
        return 1;
      freet(dup);
    }
  }

  memset(&headers, 0, sizeof(headers));
  headers.status = status;
  headers.r.statuscode = statuscode;
  strcpybuff(headers.r.contenttype, argv[1]);
  if (cdispo != NULL)
    strcpybuff(headers.r.cdispo, cdispo);
  strcpybuff(headers.url_fil, argv[0]);
  if (body != NULL) { /* leading body bytes, read via url_sav */
    char BIGSTK data[1024];
    const size_t n = st_decode_body(body, data, sizeof(data));
    FILE *const fp = fopen(bodyfile, "wb");

    if (fp == NULL || fwrite(data, 1, n, fp) != n) {
      fprintf(stderr, "savename: can not write %s\n", bodyfile);
      return 1;
    }
    fclose(fp);
    strcpybuff(headers.url_sav, bodyfile);
  }

  url_savename(&afs, NULL, NULL, NULL, opt, sback, &cache, &hash, 0, 0,
               &headers);
  if (body != NULL)
    (void) UNLINK(bodyfile);
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

/* A corrupt cache index (.ndx) must not walk the length-prefixed scan past
   the buffer. Checks the two primitives the loader is built from. */
static int st_cacheindex(httrackp *opt, int argc, char **argv) {
  int fail = 0;

  (void) opt;
  (void) argc;
  (void) argv;

  /* A length prefix that overstates the bytes present must bound the advance
     to the buffer, not trust the declared length. */
  {
    static const char src[] = "32768\nCACHE-1.1";
    const size_t len = sizeof(src) - 1;
    char *buf = malloct(len + 1);
    char s[256];
    int off;

    memcpy(buf, src, len + 1);
    off = cache_brstr(buf, s, sizeof(s));
    if (off > (int) len) {
      printf("cacheindex: over-advance off=%d len=%d\n", off, (int) len);
      fail = 1;
    }
    if (strcmp(s, "CACHE-1.1") != 0) {
      printf("cacheindex: value=%s\n", s);
      fail = 1;
    }
    freet(buf);
  }

  /* cache_binput reads a field while in bounds, but refuses one starting at
     or past end-of-buffer. */
  {
    char buf[8] = "ab\ncd";
    const char *const end = buf + 5;
    char s[16];

    if (cache_binput(buf, end, s, sizeof(s)) != 3 || strcmp(s, "ab") != 0)
      fail = 1; /* normal read: "ab" then the '\n', 3 bytes consumed */
    if (cache_binput(end, end, s, sizeof(s)) != 0 || s[0] != '\0')
      fail = 1;
  }

  /* Drive the full loader scan over a truncated index: a declared length that
     overshoots plus a half-written entry. ASan aborts here on the pre-fix
     scan; the cursor must never leave the buffer. */
  {
    static const char src[] = "9\nCACHE-1.1\n99\nwww.example.com\n/a";
    const size_t len = sizeof(src) - 1;
    char *buf = malloct(len + 1);
    const char *const end = buf + len;
    char line[256];
    char *a = buf;

    memcpy(buf, src, len + 1);
    a += cache_brstr(a, line, sizeof(line));
    a += cache_brstr(a, line, sizeof(line));
    while (a != NULL && a < end) {
      a = strchr(a + 1, '\n');
      if (a == NULL)
        break;
      a++;
      a += cache_binput(a, end, line, sizeof(line));
    }
    freet(buf);
  }

  printf("cacheindex: %s\n", fail ? "FAIL" : "OK");
  return fail;
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

static int st_cache_corrupt(httrackp *opt, int argc, char **argv) {
  int err;

  if (argc < 1) {
    fprintf(stderr, "cache-corrupt: needs a directory\n");
    return 1;
  }
  err = cache_corruption_selftest(opt, argv[0]);
  printf("cache-corrupt: %s\n", err ? "FAIL" : "OK");
  return err;
}

static int st_cache_legacy(httrackp *opt, int argc, char **argv) {
  int err;

  if (argc < 1) {
    fprintf(stderr, "cache-legacy: needs a directory\n");
    return 1;
  }
  err = cache_legacy_refused_selftest(opt, argv[0]);
  printf("cache-legacy: %s\n", err ? "FAIL" : "OK");
  return err;
}

static int st_reconcile(httrackp *opt, int argc, char **argv) {
  int err;

  if (argc < 1) {
    fprintf(stderr, "reconcile: needs a directory\n");
    return 1;
  }
  err = cache_reconcile_selftest(opt, argv[0]);
  printf("cache-reconcile: %s\n", err ? "FAIL" : "OK");
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
#ifndef _WIN32
  /* the jar holds live session cookies: cookie_save must keep it 0600 */
  {
    const char *jar = "st-cookies-jar.txt";
    struct stat st;

    (void) UNLINK(jar);
    assertf(cookie_save(&cookie, jar) == 0);
    assertf(stat(jar, &st) == 0);
    assertf((st.st_mode & 07777) == HTS_PROTECT_FILE);
    assertf(st.st_size > 0); /* mode-only checks would pass an empty jar */
    /* a pre-existing world-readable jar must be tightened, not kept */
    assertf(chmod(jar, 0644) == 0);
    assertf(cookie_save(&cookie, jar) == 0);
    assertf(stat(jar, &st) == 0);
    assertf((st.st_mode & 07777) == HTS_PROTECT_FILE);
    assertf(st.st_size > 0);
    (void) UNLINK(jar);
  }
#endif
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

/* #159: hts_redirect_same_savefile decides whether a redirect is a same-file
 * alias. */
static int st_redirect_samefile(httrackp *opt, int argc, char **argv) {
  (void) argc;
  (void) argv;
#define SAME(aa, fa, ab, fb) hts_redirect_same_savefile(opt, aa, fa, ab, fb)
  /* scheme and userinfo collapse (the #159 case); a different path does not */
  assertf(SAME("http://foo.com", "/a/b", "https://foo.com", "/a/b"));
  assertf(SAME("http://user@foo.com", "/a", "http://foo.com", "/a"));
  assertf(!SAME("http://foo.com", "/a", "http://foo.com", "/b"));
  /* www stays distinct here; the crawl's dedup layer folds www, not this helper
   */
  opt->urlhack = HTS_TRUE;
  opt->no_www_dedup = opt->no_slash_dedup = opt->no_query_dedup = HTS_FALSE;
  assertf(!SAME("http://www.foo.com", "/a", "http://foo.com", "/a"));
  /* slash/query fold only when the dedup flag is on */
  assertf(SAME("https://foo.com", "/a//b", "http://foo.com", "/a/b"));
  assertf(
      SAME("https://foo.com", "/p?b=2&a=1", "http://foo.com", "/p?a=1&b=2"));
  opt->no_slash_dedup = opt->no_query_dedup = HTS_TRUE;
  assertf(!SAME("https://foo.com", "/a//b", "http://foo.com", "/a/b"));
  assertf(
      !SAME("https://foo.com", "/p?b=2&a=1", "http://foo.com", "/p?a=1&b=2"));
  /* but a pure scheme alias still collapses regardless of dedup opt-outs */
  assertf(SAME("http://foo.com", "/a/b", "https://foo.com", "/a/b"));
  opt->no_slash_dedup = opt->no_query_dedup = HTS_FALSE;
#undef SAME
  printf("redirect-samefile self-test OK\n");
  return 0;
}

// hts_finish_makeindex writes the footer, emits the refresh meta only when
// makeindex_links==1, and clears *fp / sets *done. argv[0] is a writable dir.
static int st_makeindex(httrackp *opt, int argc, char **argv) {
  char path[HTS_URLMAXSIZE];
  char buf[4096];
  FILE *fp;
  size_t n;
  int done;

  assertf(argc >= 1);
  snprintf(path, sizeof(path), "%s/index.html", argv[0]);

  /* single first link: footer + a refresh meta carrying the escaped URL */
  done = 0;
  fp = fopen(path, "wb");
  assertf(fp != NULL);
  hts_finish_makeindex(opt, &done, &fp, 1, "http://example.com/a b", "%s%s", "",
                       "");
  assertf(fp == NULL); /* the function closed and cleared it */
  assertf(done != 0);
  fp = fopen(path, "rb");
  assertf(fp != NULL);
  n = fread(buf, 1, sizeof(buf) - 1, fp);
  fclose(fp);
  buf[n] = '\0';
  assertf(strstr(buf, "Mirror and index made by HTTrack") != NULL);
  assertf(strstr(buf, "Refresh") != NULL);
  assertf(strstr(buf, "example.com") != NULL);

  /* no single link: footer only, no refresh meta */
  done = 0;
  fp = fopen(path, "wb");
  assertf(fp != NULL);
  hts_finish_makeindex(opt, &done, &fp, 0, NULL, "%s%s", "", "");
  assertf(fp == NULL);
  assertf(done != 0);
  fp = fopen(path, "rb");
  assertf(fp != NULL);
  n = fread(buf, 1, sizeof(buf) - 1, fp);
  fclose(fp);
  buf[n] = '\0';
  assertf(strstr(buf, "Mirror and index made by HTTrack") != NULL);
  assertf(strstr(buf, "Refresh") == NULL);

  UNLINK(path);
  printf("makeindex self-test OK\n");
  return 0;
}

/* Each inplace_escape_*() must equal escape_*() on a copy. */
static int st_inplace_escape(httrackp *opt, int argc, char **argv) {
  /* >255 bytes forces the helper's malloct path, not the stack buffer */
  static char longstr[600];
  static const char *const samples[] = {
      "",          "abc",           "a b/c?d=e&f", "h\x8ello w\x94rld",
      "a%b\"c<d>", "/path to/file", longstr};
  static size_t (*const inplace[])(char *, size_t) = {
      inplace_escape_in_url, inplace_escape_spc_url, inplace_escape_uri_utf,
      inplace_escape_check_url, inplace_escape_uri};
  static size_t (*const plain[])(const char *, char *, size_t) = {
      escape_in_url, escape_spc_url, escape_uri_utf, escape_check_url,
      escape_uri};
  size_t i, f;

  (void) opt;
  (void) argc;
  (void) argv;

  memset(longstr, 'a', sizeof(longstr) - 1);
  for (f = 0; f < sizeof(inplace) / sizeof(inplace[0]); f++) {
    for (i = 0; i < sizeof(samples) / sizeof(samples[0]); i++) {
      char ref[4096], work[4096];
      size_t rret, iret;
      rret = plain[f](samples[i], ref, sizeof(ref));
      strcpybuff(work, samples[i]);
      iret = inplace[f](work, sizeof(work));
      assertf(iret == rret);
      assertf(strcmp(work, ref) == 0);
    }
  }
  printf("inplace-escape self-test OK\n");
  return 0;
}

/* Pin HTS_HTMLESCAPE*_MAXEXP to each escaper's true max byte expansion. */
static int st_escape_room(httrackp *opt, int argc, char **argv) {
  /* N > 1023: where 6n outgrows the old 5n+1024 reservation */
  enum { N = 2000 };

  char *src = malloct(N + 1);
  char *dst;
  size_t room, got;
  (void) opt;
  (void) argc;
  (void) argv;

  /* _full worst case: a high byte expands to "&#xHH;" (6 bytes) */
  memset(src, 0xE9, N);
  src[N] = '\0';
  room = (size_t) N * HTS_HTMLESCAPE_FULL_MAXEXP + 1024;
  dst = malloct(room);
  got = escape_for_html_print_full(src, dst, room);
  assertf(got == (size_t) N * HTS_HTMLESCAPE_FULL_MAXEXP);
  assertf(strlen(dst) == got);
  freet(dst);

  /* one factor short overflows (returns size), truncating the page: the bug */
  room = (size_t) N * (HTS_HTMLESCAPE_FULL_MAXEXP - 1) + 1024;
  dst = malloct(room);
  got = escape_for_html_print_full(src, dst, room);
  assertf(got == room);
  freet(dst);

  /* plain escaper worst case: '&' -> "&amp;" (5); high bytes stay verbatim */
  memset(src, '&', N);
  src[N] = '\0';
  room = (size_t) N * HTS_HTMLESCAPE_MAXEXP + 1024;
  dst = malloct(room);
  got = escape_for_html_print(src, dst, room);
  assertf(got == (size_t) N * HTS_HTMLESCAPE_MAXEXP);
  assertf(strlen(dst) == got);
  freet(dst);

  freet(src);
  printf("escape-room self-test OK\n");
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

#if HTS_USEZLIB
/* Deflate src->path at windowBits (16+ gzip, + zlib, - raw); 0 on success. */
static int ae_write_packed(const char *path, int windowBits,
                           const unsigned char *src, size_t len) {
  unsigned char out[8192];
  z_stream strm;
  FILE *f;
  int zerr;

  memset(&strm, 0, sizeof(strm));
  if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, windowBits, 8,
                   Z_DEFAULT_STRATEGY) != Z_OK)
    return 1;
  if ((f = FOPEN(path, "wb")) == NULL) {
    deflateEnd(&strm);
    return 1;
  }
  strm.next_in = (Bytef *) src;
  strm.avail_in = (uInt) len;
  do {
    size_t n;

    strm.next_out = out;
    strm.avail_out = sizeof(out);
    zerr = deflate(&strm, Z_FINISH);
    n = sizeof(out) - strm.avail_out;
    if (n > 0 && fwrite(out, 1, n, f) != n) {
      deflateEnd(&strm);
      fclose(f);
      return 1;
    }
  } while (zerr == Z_OK);
  deflateEnd(&strm);
  fclose(f);
  return (zerr == Z_STREAM_END) ? 0 : 1;
}

/* Forged raw deflate (08 1D) that misdetects as zlib; only fallback decodes */
static int ae_write_collision(const char *path, const unsigned char *src,
                              size_t len) {
  /* block-1 LEN low byte 0x1D: with 0x08, (0x081D)%31==0 */
  const size_t n1 = 29;
  size_t n2, p = 0;
  unsigned char *buf;
  FILE *f;
  int ok;

  if (len < n1 || len - n1 > 0xFFFF)
    return 1;
  n2 = len - n1;
  buf = malloct(10 + len);
  if (buf == NULL)
    return 1;
  buf[p++] = 0x08; /* BFINAL=0, BTYPE=00, forged padding -> zlib CMF nibble */
  buf[p++] = (unsigned char) (n1 & 0xff);
  buf[p++] = (unsigned char) (n1 >> 8);
  buf[p++] = (unsigned char) (~n1 & 0xff);
  buf[p++] = (unsigned char) ((~n1 >> 8) & 0xff);
  memcpy(buf + p, src, n1);
  p += n1;
  buf[p++] = 0x01; /* BFINAL=1, BTYPE=00 */
  buf[p++] = (unsigned char) (n2 & 0xff);
  buf[p++] = (unsigned char) (n2 >> 8);
  buf[p++] = (unsigned char) (~n2 & 0xff);
  buf[p++] = (unsigned char) ((~n2 >> 8) & 0xff);
  memcpy(buf + p, src + n1, n2);
  p += n2;
  f = FOPEN(path, "wb");
  ok = (f != NULL && fwrite(buf, 1, p, f) == p);
  if (f != NULL)
    fclose(f);
  freet(buf);
  return ok ? 0 : 1;
}

/* Write src[0..len) to path as-is; 0 on success. */
static int ae_write_raw(const char *path, const unsigned char *src,
                        size_t len) {
  FILE *const f = FOPEN(path, "wb");
  int ok;

  if (f == NULL)
    return 1;
  ok = fwrite(src, 1, len, f) == len;
  fclose(f);
  return ok ? 0 : 1;
}

/* Compare path's bytes to expect[0..len); 0 if equal. Streams (large files). */
static int ae_check_decoded(const char *path, const unsigned char *expect,
                            size_t len) {
  unsigned char buf[8192];
  FILE *f = FOPEN(path, "rb");
  size_t off = 0, n;

  if (f == NULL)
    return 1;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
    if (n > len - off || memcmp(buf, expect + off, n) != 0) {
      fclose(f);
      return 1;
    }
    off += n;
  }
  fclose(f);
  return (off == len) ? 0 : 1;
}
#endif

/* Accept-Encoding (#450): advertise gzip+deflate; both decode (hts_zunpack) */
static int st_acceptencoding(httrackp *opt, int argc, char **argv) {
  const char *off = hts_acceptencoding(HTS_FALSE);
  const char *on = hts_acceptencoding(HTS_TRUE);

  (void) opt;
  assertf(strcmp(off, "identity") == 0);
  assertf(strstr(on, "gzip") != NULL);
  assertf(strstr(on, "deflate") != NULL); /* fails on the old gzip-only list */
#if HTS_USEZLIB
  if (argc >= 1) {
    static const int windowBits[] = {16 + MAX_WBITS, MAX_WBITS, -MAX_WBITS};
    const unsigned char small[] =
        "deflate round-trip: HTTrack decodes gzip and deflate alike. "
        "deflate round-trip: HTTrack decodes gzip and deflate alike.";
    const size_t slen = sizeof(small) - 1;
    /* 64 KiB of varied (LCG) bytes: forces the multi-fread loop */
    const size_t blen = 64 * 1024;
    unsigned char *body = malloct(blen);
    uint32_t x = 0x1234567u;
    char inpath[HTS_URLMAXSIZE], outpath[HTS_URLMAXSIZE];
    size_t i;

    assertf(body != NULL);
    for (i = 0; i < blen; i++) {
      x = x * 1103515245u + 12345u;
      body[i] = (unsigned char) (x >> 16);
    }
    /* gzip, zlib (RFC1950) and raw deflate (RFC1951), both small and large. */
    for (i = 0; i < sizeof(windowBits) / sizeof(windowBits[0]); i++) {
      snprintf(inpath, sizeof(inpath), "%s/ae-in-%d.z", argv[0], windowBits[i]);
      snprintf(outpath, sizeof(outpath), "%s/ae-out-%d", argv[0],
               windowBits[i]);
      assertf(ae_write_packed(inpath, windowBits[i], small, slen) == 0);
      assertf(hts_zunpack(inpath, outpath) == (int) slen);
      assertf(ae_check_decoded(outpath, small, slen) == 0);
      assertf(ae_write_packed(inpath, windowBits[i], body, blen) == 0);
      assertf(hts_zunpack(inpath, outpath) == (int) blen);
      assertf(ae_check_decoded(outpath, body, blen) == 0);
    }
    /* Fallback teeth: raw deflate misdetected as zlib; -1 without the retry. */
    snprintf(inpath, sizeof(inpath), "%s/ae-collide.z", argv[0]);
    snprintf(outpath, sizeof(outpath), "%s/ae-collide.out", argv[0]);
    assertf(ae_write_collision(inpath, body, 64) == 0);
    assertf(hts_zunpack(inpath, outpath) == 64);
    assertf(ae_check_decoded(outpath, body, 64) == 0);
    /* Identity fallback (#47): a plain body mislabeled as compressed is kept
       verbatim, small and multi-chunk (> one 8 KiB fread). */
    assertf(ae_write_raw(inpath, small, slen) == 0);
    assertf(hts_zunpack(inpath, outpath) == (int) slen);
    assertf(ae_check_decoded(outpath, small, slen) == 0);
    {
      const size_t ilen = 16 * 1024;
      unsigned char *idbody = malloct(ilen);

      assertf(idbody != NULL);
      for (i = 0; i < ilen; i++)
        idbody[i] = small[i % slen];
      assertf(ae_write_raw(inpath, idbody, ilen) == 0);
      assertf(hts_zunpack(inpath, outpath) == (int) ilen);
      assertf(ae_check_decoded(outpath, idbody, ilen) == 0);
      freet(idbody);
    }
    /* Truncated gzip (CRC+ISIZE cut), zlib (ADLER32 cut) and raw deflate
       must all still fail, not fall back to a verbatim copy. */
    {
      static const struct {
        int wb;
        size_t cut;
      } tr[] = {{16 + MAX_WBITS, 8}, {MAX_WBITS, 4}, {-MAX_WBITS, 5}};

      for (i = 0; i < sizeof(tr) / sizeof(tr[0]); i++) {
        unsigned char z[512];
        size_t zlen;
        FILE *f;

        assertf(ae_write_packed(inpath, tr[i].wb, small, slen) == 0);
        f = FOPEN(inpath, "rb");
        assertf(f != NULL);
        zlen = fread(z, 1, sizeof(z), f);
        fclose(f);
        assertf(zlen > tr[i].cut && zlen < sizeof(z));
        assertf(ae_write_raw(inpath, z, zlen - tr[i].cut) == 0);
        assertf(hts_zunpack(inpath, outpath) < 0);
      }
    }
    freet(body);
  }
#else
  (void) argc;
  (void) argv;
#endif
  printf("acceptencoding self-test OK: %s\n", on);
  return 0;
}

/* Each call parses `txt` under a fresh host, then checkrobots() for `path`. */
static int rb_decide(robots_wizard *r, const char *txt, const char *path) {
  static int n = 0;
  char host[64];

  snprintf(host, sizeof(host), "h%d.example", n++);
  robots_parse(r, host, txt, strlen(txt), NULL, 0, HTS_TRUE);
  return checkrobots(r, host, path);
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

  /* No rules: everything is allowed. */
  assertf(rb_decide(&robots, "User-agent: *\nDisallow:\n", "/x") == 0);

  checkrobots_free(&robots);
  printf("robots self-test OK\n");
  return 0;
}

/* get_ftp_line must bound a hostile, CRLF-less reply into its internal
   1024-byte buffer; ASan turns the pre-fix overflow into an abort here. */
#ifndef _WIN32
static int st_ftpline(httrackp *opt, int argc, char **argv) {
  int sv[2];
  char line[2048];
  char flood[4096];

  (void) opt;
  (void) argc;
  (void) argv;
  memset(flood, 'x', sizeof(flood));
  assertf(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
  assertf(write(sv[1], "220 ", 4) == 4); // valid 3-digit code
  assertf(write(sv[1], flood, sizeof(flood)) == (ssize_t) sizeof(flood));
  assertf(write(sv[1], "\r\n", 2) == 2); // end the line so we return
  close(sv[1]);
  line[0] = '\0';
  get_ftp_line(sv[0], line, sizeof(line), 5);
  close(sv[0]);
  printf("ftp-line self-test OK (bounded %d-byte reply)\n",
         (int) sizeof(flood));
  return 0;
}
#endif

/* ftp_split_userpass: well-formed split, plus a hostile over-long userinfo
   that pre-fix overran user[256]/pass[256]. */
static int st_ftpuser(httrackp *opt, int argc, char **argv) {
  char user[256], pass[256];
  char in[1200];

  (void) opt;
  (void) argc;
  (void) argv;
  {
    const char ok[] = "bob:secret@host/f"; // '@' at index 10

    ftp_split_userpass(ok, ok + 11, user, sizeof(user), pass, sizeof(pass));
    assertf(strcmp(user, "bob") == 0);
    assertf(strcmp(pass, "secret") == 0);
  }
  memset(in, 'u', 400);
  in[400] = ':';
  memset(in + 401, 'p', 400);
  in[801] = '@';
  in[802] = '\0';
  ftp_split_userpass(in, in + 802, user, sizeof(user), pass, sizeof(pass));
  assertf(strlen(user) == sizeof(user) - 1);
  assertf(strlen(pass) == sizeof(pass) - 1);
  {
    /* tight sizes + guard byte catch an off-by-one the 256 case can't */
    char ubuf[16], pbuf[16];

    memset(ubuf, 'Z', sizeof(ubuf));
    memset(pbuf, 'Z', sizeof(pbuf));
    ftp_split_userpass(in, in + 802, ubuf, 8, pbuf, 8);
    assertf(strcmp(ubuf, "uuuuuuu") == 0);
    assertf(strcmp(pbuf, "ppppppp") == 0);
    assertf(ubuf[8] == 'Z' && pbuf[8] == 'Z');
  }
  printf("ftp-userpass self-test OK\n");
  return 0;
}

/* hts_count_fits caps the .class constant-pool entry count to the file size,
   rejecting the ~68 MB-per-file calloc DoS. */
static int st_java(httrackp *opt, int argc, char **argv) {
  (void) opt;
  (void) argc;
  (void) argv;
  assertf(hts_count_fits(10, 1000) == HTS_TRUE);
  assertf(hts_count_fits(0, 10) == HTS_TRUE);
  assertf(hts_count_fits(65535, 10) == HTS_FALSE);
  assertf(hts_count_fits(1, 0) == HTS_FALSE);
  assertf(hts_count_fits(1, -1) == HTS_FALSE);
  printf("java constant-pool cap self-test OK\n");
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
    {"filtermime", "<mime> <filter>...",
     "mime-type filter verdict (fa_strjoker type=1)", st_filtermime},
    {"filtermemo", "[iterations]",
     "memoized vs unmemoized matcher differential", st_filtermemo},
    {"filterdual", "<string1> <string2> <filter>...",
     "merged two-form filter verdict (fa_strjoker_dual)", st_filterdual},
    {"simplify", "<path>", "collapse ./ and ../ in a path", st_simplify},
    {"stripquery", "", "--strip-query pattern/key stripping self-test",
     st_stripquery},
    {"urlhack", "", "-%u url-hack sub-flag (www/slash/query) self-test",
     st_urlhack},
    {"redirect-samefile", "", "same-file redirect detection self-test (#159)",
     st_redirect_samefile},
    {"mime", "<filename>", "MIME type for a filename", st_mime},
    {"charset", "<charset> <string>",
     "convert a string to UTF-8 from a charset", st_charset},
    {"metacharset", "<html>", "extract the <meta> charset from an HTML page",
     st_metacharset},
    {"isutf8", "<string>", "is the string valid UTF-8 (1/0)", st_isutf8},
    {"idna-encode", "<host>", "encode a hostname to IDNA/punycode",
     st_idna_encode},
    {"idna-decode", "<host>", "decode an IDNA/punycode hostname",
     st_idna_decode},
    {"entities", "<string> [encoding]", "unescape HTML entities", st_entities},
    {"unescape-bounds", "", "unescapers reserve the NUL byte (no 1-byte OOB)",
     st_unescape_bounds},
    {"hashtable", "<count|file>", "coucal hashtable stress test", st_hashtable},
    {"strsafe", "[overflow|overflow-buff [str]]", "bounded string-op self-test",
     st_strsafe},
    {"copyopt", "", "copy_htsopt option-copy self-test", st_copyopt},
    {"pause", "", "randomized inter-file pause target self-test", st_pause},
    {"relative", "<link> <curr-file>", "relative link between two paths",
     st_relative},
    {"resolve", "<link> <adr> <fil>", "resolve a link against an origin",
     st_resolve},
    {"identurl", "<url>", "split an absolute URL into (adr, fil)", st_identurl},
    {"header", "<raw-header-line> ...", "response header-line parsing",
     st_header},
    {"headerlong", "[header-name:]",
     "over-long header value must not overflow the parse scratch",
     st_headerlong},
    {"savename", "<fil> <content-type> [key=value ...]",
     "local save-name for a URL", st_savename},
    {"sniff", "<content-type> <hex:..|text>", "MIME magic consistency",
     st_sniff},
    {"cache", "<dir>", "cache read/write round-trip self-test", st_cache},
    {"cacheindex", "", "cache-index (.ndx) parse must stay in bounds",
     st_cacheindex},
    {"cache-golden", "<dir> [regen]", "frozen cache-format read self-test",
     st_cache_golden},
    {"cache-writefail", "<dir>", "cache write-failure handling self-test",
     st_cache_writefail},
    {"reconcile", "<dir>", "cache generation reconcile policy self-test",
     st_reconcile},
    {"cache-legacy", "<dir>", "pre-3.31 legacy cache refusal self-test",
     st_cache_legacy},
    {"cache-corrupt", "<dir>", "cache read-side corruption self-test",
     st_cache_corrupt},
    {"dns", "", "DNS resolver/cache self-test", st_dns},
    {"cookies", "", "cookie request-header self-test", st_cookies},
    {"useragent", "", "default User-Agent self-test", st_useragent},
    {"makeindex", "[dir]", "hts_finish_makeindex footer/refresh self-test",
     st_makeindex},
    {"inplace-escape", "", "inplace_escape_* vs escape_* equivalence self-test",
     st_inplace_escape},
    {"escape-room", "", "HT_ADD_HTMLESCAPED* reservation-factor self-test",
     st_escape_room},
    {"status", "", "HTTP status code -> reason phrase self-test", st_status},
    {"acceptencoding", "[dir]",
     "Accept-Encoding advertises gzip+deflate, both decode", st_acceptencoding},
    {"robots", "", "robots.txt RFC 9309 Allow/Disallow precedence self-test",
     st_robots},
#ifndef _WIN32
    {"ftp-line", "", "get_ftp_line bounds a hostile FTP reply line",
     st_ftpline},
#endif
    {"ftp-userpass", "", "ftp_split_userpass bounds URL userinfo", st_ftpuser},
    {"java", "", "java .class constant-pool count cap self-test", st_java},
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
