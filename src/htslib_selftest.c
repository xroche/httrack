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
/* File: htslib_selftest.c subroutines:                         */
/*       self-tests for the engine utility primitives           */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htsselftest_int.h"

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
    // an empty current path: the trim used to walk off the front of it, which
    // "?x" reaches too because the query pre-pass hands on the part before it
    assertf(lienrelatif(s, sizeof(s), "dir/page.html", "") == 0);
    assertf(strcmp(s, "dir/page.html") == 0);
    assertf(lienrelatif(s, sizeof(s), "dir/page.html", "?x") == 0);
    assertf(strcmp(s, "dir/page.html") == 0);
  }
}

/* Self-tests for the htssafe.h bounded string ops.
   Returns 0 if every bounded operation behaved correctly, 1 otherwise.
   The abort-on-overflow guarantee is checked separately by the "overflow"
   sub-mode (it aborts the process by design). */
static int string_safety_selftests(void) {
  char buf[16];

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

  /* A decayed source has no known capacity, so the whole tail must land; a
     sizeof(char*) capacity would abort here instead. */
  {
    char src[32] = "0123456789abcdefghij";
    char dst[32];

    strcpybuff(dst, src + 1);
    if (strcmp(dst, "123456789abcdefghij") != 0)
      return 1;
  }

  /* Truncating append: stops at N without aborting, what the status-message
     call sites rely on. */
  {
    char dst[10]; /* never sizeof(char*), or MSVC reads it as a pointer */

    dst[0] = '\0';
    strncatbuff(dst, "abcdefghijkl", sizeof(dst) - 1);
    if (strcmp(dst, "abcdefghi") != 0)
      return 1;
  }

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

  /* sprintfbuff: truncate-and-report. Must never abort (its callers format
     remote banners) nor write past the array, which the canary catches. */
  {
    struct {
      char dst[8];
      char canary[8];
    } s;

    const char *const big = "0123456789abcdefghijklmnopqrstuvwxyz";

    /* repoison before every call, or an implementation that measures first and
       writes nothing "passes" the truncating cases on the previous content */
#define POISON_DST() memset(s.dst, '#', sizeof(s.dst))

    memset(&s, '#', sizeof(s));
    if (!sprintfbuff(s.dst, "%s-%d", "ab", 42) || strcmp(s.dst, "ab-42") != 0)
      return 1;

    /* exact fit: 7 characters plus the NUL */
    POISON_DST();
    if (!sprintfbuff(s.dst, "%s", "1234567") || strcmp(s.dst, "1234567") != 0)
      return 1;

    /* one over, then far over: truncated to the prefix, terminated, reported */
    POISON_DST();
    if (sprintfbuff(s.dst, "%s", "12345678") || strcmp(s.dst, "1234567") != 0)
      return 1;
    POISON_DST();
    if (sprintfbuff(s.dst, "%s", big) || strcmp(s.dst, "0123456") != 0)
      return 1;

    /* explicit-capacity form, down to the degenerate size 1 */
    {
      char *const p = s.dst;

      POISON_DST();
      if (slprintfbuff(p, 1, "%s", "x") || p[0] != '\0')
        return 1;
      POISON_DST();
      if (!slprintfbuff(p, sizeof(s.dst), "%s", "ok") || strcmp(p, "ok") != 0)
        return 1;
    }
#undef POISON_DST

    if (memcmp(s.canary, "########", sizeof(s.canary)) != 0)
      return 1;
  }

  /* slcatprintfbuff_clip: append, clip, advance the cursor. Same canary shape,
     and the destination is reset before every case so an implementation that
     writes nothing cannot pass on the previous one's bytes (#1685). */
  {
    struct {
      char dst[8];
      char canary[8];
    } s;

    const char *const big = "0123456789abcdefghijklmnopqrstuvwxyz";
    size_t used;

    memset(&s, '#', sizeof(s));
#define RESET_DST()                                                            \
  do {                                                                         \
    memset(s.dst, '#', sizeof(s.dst));                                         \
    s.dst[0] = '\0';                                                           \
    used = 0;                                                                  \
  } while (0)

    /* well under capacity: the cursor lands on the new end */
    RESET_DST();
    slcatprintfbuff_clip(s.dst, sizeof(s.dst), &used, "%s-%d", "ab", 42);
    if (strcmp(s.dst, "ab-42") != 0 || used != 5)
      return 1;

    /* a second append continues where the cursor left it */
    slcatprintfbuff_clip(s.dst, sizeof(s.dst), &used, "%s", "xy");
    if (strcmp(s.dst, "ab-42xy") != 0 || used != 7)
      return 1;

    /* exact fit: capacity - 1 characters plus the NUL */
    RESET_DST();
    slcatprintfbuff_clip(s.dst, sizeof(s.dst), &used, "%s", "1234567");
    if (strcmp(s.dst, "1234567") != 0 || used != 7)
      return 1;

    /* one over, then far over: clipped to the prefix and terminated. The
       expected bytes differ between the two, so a write-nothing implementation
       cannot pass on the leftovers. */
    RESET_DST();
    slcatprintfbuff_clip(s.dst, sizeof(s.dst), &used, "%s", "12345678");
    if (strcmp(s.dst, "1234567") != 0 || used != 7)
      return 1;
    RESET_DST();
    slcatprintfbuff_clip(s.dst, sizeof(s.dst), &used, "%s", big);
    if (strcmp(s.dst, "0123456") != 0 || used != 7)
      return 1;

    /* a full destination takes nothing more and the cursor stays put */
    slcatprintfbuff_clip(s.dst, sizeof(s.dst), &used, "%s", "zz");
    if (strcmp(s.dst, "0123456") != 0 || used != 7)
      return 1;
#undef RESET_DST

    if (memcmp(s.canary, "########", sizeof(s.canary)) != 0)
      return 1;
  }

  /* strclipbuff: truncate-and-report, never abort. Same canary shape; the
     destination is poisoned before every call so a case cannot pass on the
     previous one's bytes. */
  {
    struct {
      char dst[8];
      char canary[8];
    } s;

    memset(&s, '#', sizeof(s));
#define POISON_DST() memset(s.dst, '#', sizeof(s.dst))

    /* well under capacity: no padding, nothing eaten off the end */
    POISON_DST();
    if (!strclipbuff(s.dst, sizeof(s.dst), "abc") || strcmp(s.dst, "abc") != 0)
      return 1;

    /* exact fit: capacity - 1 characters plus the NUL */
    POISON_DST();
    if (!strclipbuff(s.dst, sizeof(s.dst), "1234567") ||
        strcmp(s.dst, "1234567") != 0)
      return 1;

    /* one over, then far over: clipped, terminated, reported. The expected
       bytes differ from the case above, so a write-nothing implementation
       cannot pass on the leftovers. */
    POISON_DST();
    if (strclipbuff(s.dst, sizeof(s.dst), "abcdefgh") ||
        strcmp(s.dst, "abcdefg") != 0)
      return 1;
    POISON_DST();
    if (strclipbuff(s.dst, sizeof(s.dst), "0123456789abcdef") ||
        strcmp(s.dst, "0123456") != 0)
      return 1;

    /* degenerate capacity 1: only the NUL fits. Capacity 2 pins the boundary
       between that and the sizes above: one character plus the NUL. */
    POISON_DST();
    if (strclipbuff(s.dst, 1, "x") || s.dst[0] != '\0')
      return 1;
    POISON_DST();
    if (strclipbuff(s.dst, 2, "yz") || strcmp(s.dst, "y") != 0)
      return 1;

    /* the empty string fits any non-zero capacity */
    POISON_DST();
    if (!strclipbuff(s.dst, sizeof(s.dst), "") || s.dst[0] != '\0')
      return 1;

    /* a byte over 0x7f must not end the copy early */
    POISON_DST();
    if (!strclipbuff(s.dst, sizeof(s.dst), "\xff\xfe") ||
        strcmp(s.dst, "\xff\xfe") != 0)
      return 1;
#undef POISON_DST

    if (memcmp(s.canary, "########", sizeof(s.canary)) != 0)
      return 1;
  }

  /* htsblk_failf: clips a reason quoted from a remote reply into msg[] and
     touches nothing else in the block */
  {
    htsblk r;
    char expect[sizeof(r.msg)];
    char big[4 * sizeof(r.msg)];

    /* contenttype abuts msg, so a one-past-the-end store lands in it rather
       than in padding. Poison it: a stray NUL is invisible against zeroes,
       and a stray NUL is exactly what an off-by-one terminator writes. */
#define NEIGHBOURS_INTACT() (r.contenttype[0] == '#' && r.statuscode == 1234)

    memset(&r, 0, sizeof(r));
    memset(r.contenttype, '#', sizeof(r.contenttype));
    r.statuscode = 1234;

    memset(r.msg, '#', sizeof(r.msg));
    htsblk_failf(&r, "PASV incorrect: %s", "220 ok");
    if (strcmp(r.msg, "PASV incorrect: 220 ok") != 0 || !NEIGHBOURS_INTACT())
      return 1;

    /* exact fit: capacity - 1 characters plus the NUL */
    memset(expect, 'y', sizeof(expect) - 1);
    expect[sizeof(expect) - 1] = '\0';
    memcpy(expect, "Bad password: ", sizeof("Bad password: ") - 1);
    memset(r.msg, '#', sizeof(r.msg));
    htsblk_failf(&r, "%s", expect);
    if (strcmp(r.msg, expect) != 0 || !NEIGHBOURS_INTACT())
      return 1;

    /* far over: the expected bytes differ from the cases above, so writing
       nothing cannot pass on the leftovers */
    memset(big, 'z', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    memset(expect, 'z', sizeof(expect) - 1);
    expect[sizeof(expect) - 1] = '\0';
    memcpy(expect, "Bad user name: ", sizeof("Bad user name: ") - 1);

    memset(r.msg, '#', sizeof(r.msg));
    htsblk_failf(&r, "Bad user name: %s", big);
    if (strcmp(r.msg, expect) != 0 || !NEIGHBOURS_INTACT())
      return 1;
#undef NEIGHBOURS_INTACT
  }

  /* back_read_ftp_result: the helper's result file is external input, so an
     over-long message must stop at msg[]'s capacity */
  {
    htsblk r;
    size_t k;

    /* poisoned so a short message cannot pass on leftovers, and so a stray
       NUL past msg[] is visible in the neighbour */
#define FTP_RESULT_CASE(BODY)                                                  \
  do {                                                                         \
    FILE *fp_ = tmpfile();                                                     \
                                                                               \
    if (fp_ == NULL)                                                           \
      return 1;                                                                \
    BODY;                                                                      \
    rewind(fp_);                                                               \
    memset(&r, 0, sizeof(r));                                                  \
    memset(r.msg, '#', sizeof(r.msg));                                         \
    memset(r.contenttype, '#', sizeof(r.contenttype));                         \
    back_read_ftp_result(fp_, &r);                                             \
    fclose(fp_);                                                               \
    if (r.contenttype[0] != '#')                                               \
      return 1;                                                                \
  } while (0)

    /* over capacity: clipped to 79 payload bytes plus the NUL */
    FTP_RESULT_CASE({
      fprintf(fp_, "226 ");
      for (k = 0; k < 4 * sizeof(r.msg); k++)
        fputc('q', fp_);
    });
    if (r.statuscode != 226 || strlen(r.msg) != sizeof(r.msg) - 1)
      return 1;
    for (k = 0; k < sizeof(r.msg) - 1; k++) {
      if (r.msg[k] != 'q')
        return 1;
    }

    /* well under capacity: nothing padded, nothing eaten off the end */
    FTP_RESULT_CASE(fprintf(fp_, "550 no such file"));
    if (r.statuscode != 550 || strcmp(r.msg, "no such file") != 0)
      return 1;

    /* a byte over 0x7f must not read as EOF and cut the message short */
    FTP_RESULT_CASE(fprintf(fp_, "226 \xff ok"));
    if (r.statuscode != 226 || strcmp(r.msg, "\xff ok") != 0)
      return 1;

    /* unparseable status: the message still loads, the code reports failure */
    FTP_RESULT_CASE(fprintf(fp_, "not-a-number here"));
    if (r.statuscode != STATUSCODE_INVALID)
      return 1;
#undef FTP_RESULT_CASE
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
    const LLint fs = fsize(snum);
    /* one width for the buffer and the walk below: a 64-bit size wraps
       malloct() short while the loop still counts to the real end */
    const size_t size = fs >= 0 ? llint_to_size_t(fs) : (size_t) -1;
    FILE *fp = size != (size_t) -1 ? fopen(snum, "rb") : NULL;
    if (fp != NULL) {
      buff = malloct(size);
      if (buff != NULL && hts_fread_exact(buff, size, fp)) {
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
              assertf(strings != NULL);
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
        int result = 0; /* no final else: an unknown type reports failure */
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
  freet(buff);
  freet(strings);
  freet(snum);
  return 0;
}

static int st_strsafe(httrackp *opt, int argc, char **argv) {
  (void) opt;
  if (argc >= 1 && strncmp(argv[0], "overflow", 8) == 0) {
    /* Deliberately exceed a sized buffer: the bounded op must abort. The source
       comes from argv so its length is opaque to the compiler (no static
       -Wstringop-overflow, genuine runtime check). "overflow-buff" exercises
       htsbuff. */
    /* Not sizeof(char*): on ILP32 a char[4] equals the pointer size, and the
       MSVC array-vs-pointer heuristic (sizeof(A) != sizeof(char*)) then reads
       it as a pointer and silently skips the bound. */
    char small[6];
    const char *const src = (argc >= 2) ? argv[1] : "overflowing";

    if (strcmp(argv[0], "overflow-buff") == 0) {
      htsbuff b = htsbuff_array(small);

      htsbuff_cat(&b, src);
    } else if (strcmp(argv[0], "overflow-src") == 0) {
      /* Array source with no NUL: its capacity still comes from sizeof(), so
         the bounded strlen aborts rather than running off the array. */
      char nonul[6]; /* never sizeof(char*), per the note above */
      char big[64];

      memset(nonul, src[0], sizeof(nonul));
      strcpybuff(big, nonul);
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

/* Wide enough that a capacity still fitting a size_t makes capa*width wrap. */
typedef struct {
  char pad[4096];
} arrays_wide_t;

/* Self-tests for the htsarrays.h growth macros.
   Returns 0 if growth always reached the requested room, 1 otherwise.
   The abort on an unsatisfiable request is checked by the "overflow-capa" and
   "overflow-loop" sub-modes (they abort the process by design). */
static int array_growth_selftests(void) {
  TypedArray(char) a = EMPTY_TYPED_ARRAY;
  TypedArray(arrays_wide_t) w = EMPTY_TYPED_ARRAY;
  size_t i;
  int err = 0;

  TypedArrayEnsureRoom(a, 1);
  if (TypedArrayRoom(a) < 1 || TypedArrayCapa(a) < 16)
    err = 1;

  /* A request past the current capacity must be met, not landed short of. */
  TypedArrayAppend(a, "0123456789", 10);
  TypedArrayEnsureRoom(a, 1000);
  if (TypedArrayRoom(a) < 1000 || TypedArraySize(a) != 10)
    err = 1;
  if (memcmp(TypedArrayElts(a), "0123456789", 10) != 0)
    err = 1;

  /* Capacity must always keep capa*width representable. */
  TypedArrayEnsureRoom(w, 3);
  if (TypedArrayRoom(w) < 3 ||
      TypedArrayCapa(w) > ((size_t) -1) / sizeof(arrays_wide_t))
    err = 1;

  /* Many small growths, checking the payload survives every reallocation. */
  for (i = 0; i < 5000; i++) {
    TypedArrayAdd(a, (char) ('a' + (i % 26)));
  }
  if (TypedArraySize(a) != 5010)
    err = 1;
  else {
    for (i = 0; i < 5000; i++) {
      if (TypedArrayNth(a, 10 + i) != (char) ('a' + (i % 26)))
        err = 1;
    }
  }

  TypedArrayFree(a);
  TypedArrayFree(w);
  return err;
}

/* The arena's promise is that what it hands out never moves, so this keeps
   every pointer it returns and re-reads them all at the end. */
static int st_arena(httrackp *opt, int argc, char **argv) {
  enum { count = 4096 };

  hts_arena arena = {NULL, 0, 0};
  char **kept = (char **) calloct(count, sizeof(*kept));
  char BIGSTK big[HTS_ARENA_MIN * 2];
  char token[256];
  int i;

  (void) opt;
  (void) argc;
  (void) argv;
  assertf(kept != NULL);

  /* Enough tokens to span many chunks, each holding its own index. */
  for (i = 0; i < count; i++) {
    snprintf(token, sizeof(token), "%d-%*s", i, 200, "x");
    kept[i] = hts_arena_strdup(&arena, token);
    assertf(kept[i] != NULL);
  }
  /* One allocation larger than a whole chunk takes one of its own. */
  memset(big, 'b', sizeof(big) - 1);
  big[sizeof(big) - 1] = '\0';
  assertf(hts_arena_strdup(&arena, big) != NULL);
  /* An aligned allocation is aligned whatever the byte-sized ones did to it. */
  for (i = 0; i < 8; i++) {
    void *const p = hts_arena_alloc(&arena, sizeof(hts_arena_align));

    assertf(p != NULL);
    assertf(((size_t) (char *) p) % HTS_ARENA_ALIGN == 0);
    assertf(hts_arena_strdup(&arena, "x") != NULL);
  }
  /* Nothing moved: every token still reads back as itself. */
  for (i = 0; i < count; i++) {
    snprintf(token, sizeof(token), "%d-%*s", i, 200, "x");
    assertf(strcmp(kept[i], token) == 0);
  }
  /* A size no allocation could hold is refused, not truncated. */
  assertf(hts_arena_alloc(&arena, (size_t) -1) == NULL);
  /* Strings are packed: alignment is charged where it is needed, not to every
     allocation, which would cost several bytes on each link a mirror records.
   */
  {
    hts_arena packed = {NULL, 0, 0};
    const char *const a = hts_arena_strdup(&packed, "abc");
    const char *const b = hts_arena_strdup(&packed, "de");

    assertf(a != NULL && b == a + sizeof("abc"));
    hts_arena_free(&packed);
  }

  freet(kept);
  hts_arena_free(&arena);
  assertf(arena.chunks == NULL && arena.size == 0 && arena.used == 0);
  hts_arena_free(&arena); /* releasing an empty arena is a no-op */
  printf("arena self-test OK\n");
  return 0;
}

/* Serialize 'count' UCS2 code units, re-encode them and compare the result
   against 'expect' (its length taken as the expected byte count, so a NUL is
   not usable inside a case). */
static int ucs2_check(const unsigned short *units, size_t count,
                      const char *expect, hts_boolean swap) {
  const size_t want = strlen(expect);
  unsigned char *src = (unsigned char *) malloct(count * 2 + 1);
  size_t got_size = (size_t) -1;
  char *got;
  size_t i;
  int err = 0;

  if (src == NULL)
    return 1;
  for (i = 0; i < count; i++) {
    src[i * 2 + (swap ? 0 : 1)] = (unsigned char) (units[i] & 0xff);
    src[i * 2 + (swap ? 1 : 0)] = (unsigned char) (units[i] >> 8);
  }
  got = hts_ucs2_to_utf8(src, count * 2, swap, &got_size);
  if (got == NULL || got_size != want || memcmp(got, expect, want) != 0 ||
      got[got_size] != '\0')
    err = 1;
  freet(got);
  freet(src);
  return err;
}

/* The growth is the UCS4 re-encoder's only allocation, so any return at all
   from a starved run says the refusal was not caught. */
static int st_ucs4_oom(httrackp *opt, int argc, char **argv) {
  (void) opt;
  (void) argc;
  (void) argv;
#if defined(__GNU__)
  /* GNU/Hurd does not come back from a starved process. The run dies on
     SIGSEGV before the abort it grades, and the assert's own message never
     reaches stderr. */
  printf("ucs4: a capped process does not survive here, skipped\n");
  return 0;
#elif !defined(_WIN32)
  {
    enum { units = 512 * 1024 };

    char *src = (char *) malloct(units);
    struct rlimit saved, tight;
    hts_UCS4 *out;

    if (src == NULL || getrlimit(RLIMIT_AS, &saved) != 0) {
      printf("ucs4: cannot cap memory, skipped\n");
      freet(src);
      return 0;
    }
    memset(src, 'a', units); /* one UCS4 unit per byte, so the output is 4x */
    tight = saved;
    tight.rlim_cur = 1024 * 1024;
    if (setrlimit(RLIMIT_AS, &tight) != 0) {
      printf("ucs4: cannot cap memory, skipped\n");
      freet(src);
      return 0;
    }
    out = hts_convertUTF8StringToUCS4(src, units, NULL);
    (void) setrlimit(RLIMIT_AS, &saved);
    freet(src);
    if (out == NULL) {
      printf("ucs4: the refused growth returned instead of aborting\n");
      return 1;
    }
    freet(out);
    printf("ucs4: cap did not bite, skipped\n");
    return 0;
  }
#else
  printf("ucs4: cannot cap memory, skipped\n");
  return 0;
#endif
}

static int st_ucs2(httrackp *opt, int argc, char **argv) {
  /* A BOM, ASCII, a two-byte and a three-byte code point, and an unpaired
     surrogate: only the ASCII one is a single output byte. */
  static const unsigned short mixed[] = {0xFEFF, 'a', 0x00E9, 0x20AC, 0xD800};
  static const char mixed_utf8[] = "\xEF\xBB\xBF"
                                   "a"
                                   "\xC3\xA9"
                                   "\xE2\x82\xAC"
                                   "?";
  static const unsigned short empty[] = {0};

  (void) opt;
  if (argc >= 1 && strcmp(argv[0], "oom") == 0) {
#ifndef _WIN32
    /* Starve the output allocation. Big enough that the allocator must ask
       the kernel rather than serve it from what it already holds. */
    enum { units = 512 * 1024 };

    unsigned char *src = (unsigned char *) malloct(units * 2);
    struct rlimit saved, tight;
    size_t out_size = 0;
    char *out;
    size_t i;

    if (src == NULL || getrlimit(RLIMIT_AS, &saved) != 0) {
      printf("ucs2: cannot cap memory, skipped\n");
      freet(src);
      return 0;
    }
    for (i = 0; i < units; i++) { /* U+20AC, three UTF-8 bytes each */
      src[i * 2] = 0x20;
      src[i * 2 + 1] = 0xAC;
    }
    tight = saved;
    tight.rlim_cur = 1024 * 1024;
    if (setrlimit(RLIMIT_AS, &tight) != 0) {
      printf("ucs2: cannot cap memory, skipped\n");
      freet(src);
      return 0;
    }
    out = hts_ucs2_to_utf8(src, units * 2, HTS_FALSE, &out_size);
    (void) setrlimit(RLIMIT_AS, &saved);
    freet(src);
    if (out != NULL) {
      /* The cap did not bite (a host where RLIMIT_AS is advisory), so this run
         proves nothing either way. */
      freet(out);
      printf("ucs2: cap did not bite, skipped\n");
      return 0;
    }
    printf("ucs2: oom OK\n");
    return 0;
#else
    printf("ucs2: cannot cap memory, skipped\n");
    return 0;
#endif
  } else {
    int err = 0;

    if (ucs2_check(mixed, sizeof(mixed) / sizeof(mixed[0]), mixed_utf8,
                   HTS_TRUE))
      err = 1;
    if (ucs2_check(mixed, sizeof(mixed) / sizeof(mixed[0]), mixed_utf8,
                   HTS_FALSE))
      err = 1;
    /* Every BMP code unit, to pin the encoder against its ranges. */
    {
      unsigned int u;

      for (u = 0; u <= 0xFFFF && !err; u++) {
        const unsigned short unit = (unsigned short) u;
        char expect[4];
        size_t len = 0;

        if (u <= 0x7F)
          expect[len++] = (char) u;
        else if (u <= 0x7FF) {
          expect[len++] = (char) (0xC0 | (u >> 6));
          expect[len++] = (char) (0x80 | (u & 0x3F));
        } else if (u >= 0xD800 && u <= 0xDFFF)
          expect[len++] = '?';
        else {
          expect[len++] = (char) (0xE0 | (u >> 12));
          expect[len++] = (char) (0x80 | ((u >> 6) & 0x3F));
          expect[len++] = (char) (0x80 | (u & 0x3F));
        }
        expect[len] = '\0';
        if (u != 0 && ucs2_check(&unit, 1, expect, HTS_TRUE))
          err = 1;
      }
    }
    if (ucs2_check(empty, 0, "", HTS_TRUE))
      err = 1;
    printf("ucs2: %s\n", err ? "FAIL" : "OK");
    return err;
  }
}

static int st_arrays(httrackp *opt, int argc, char **argv) {
  /* volatile keeps the sizes below opaque, so these stay runtime checks rather
     than compile-time allocation warnings. */
  volatile size_t room;

  (void) opt;
  if (argc >= 1 && strcmp(argv[0], "overflow-capa") == 0) {
    /* Room whose byte size cannot fit a size_t: capa*width used to wrap and
       hand back a short allocation. */
    TypedArray(arrays_wide_t) w = EMPTY_TYPED_ARRAY;

    room = ((size_t) -1) / sizeof(arrays_wide_t) + 1;
    TypedArrayEnsureRoom(w, room);
    /* Unreachable, and printing the pointer is what keeps it so: an allocation
       reaching nobody is removable, and its "== NULL" folds away with it. */
    printf("arrays: NOT aborted (%p)\n", TypedArrayPtr(w));
    return 1;
  } else if (argc >= 1 && strcmp(argv[0], "overflow-loop") == 0) {
    /* Doubling past SIZE_MAX used to wrap capa to 0 and spin forever, and the
       allocation it ends on used to abort the process. */
    TypedArray(char) a = EMPTY_TYPED_ARRAY;
    int err = 0;

    TypedArrayAppend(a, "kept", 4);
    room = (size_t) -1 - TypedArraySize(a);
    TypedArrayEnsureRoom(a, room);
    if (TypedArrayHasRoom(a, room))
      err = 1;
    /* The bytes already there survive, and the append that cannot fit is a
       no-op rather than a smash. */
    TypedArrayAppend(a, "lost", room);
    if (TypedArraySize(a) != 4 || memcmp(TypedArrayElts(a), "kept", 4) != 0)
      err = 1;
    printf("arrays: growth failure %s\n", err ? "FAIL" : "OK");
    TypedArrayFree(a);
    return err;
  } else {
    const int err = array_growth_selftests();

    printf("arrays: %s\n", err ? "FAIL" : "OK");
    return err;
  }
}

static int st_random(httrackp *opt, int argc, char **argv) {
  enum { want = 64, pad = 16, rounds = 8 };

  unsigned char buf[want + pad], first[want], touched[want], varies[want];
  int err = 0, live = 0, r, i;

  (void) opt;
  (void) argc;
  (void) argv;
  memset(touched, 0, sizeof(touched));
  memset(varies, 0, sizeof(varies));
  for (r = 0; r < rounds; r++) {
    /* A fresh non-zero filler each round: a byte the fill skips keeps every one
       of them, which a single canary value cannot tell from a written byte. */
    const unsigned char filler = (unsigned char) (0xa5 + r);

    memset(buf, filler, sizeof(buf));
    if (!hts_random_bytes(buf, want))
      err = 1;
    for (i = 0; i < want; i++) {
      if (buf[i] != filler)
        touched[i] = 1;
    }
    for (i = 0; i < pad; i++) {
      if (buf[want + i] != filler)
        err = 1; /* wrote past the requested length */
    }
    if (r == 0) {
      memcpy(first, buf, want);
    } else {
      for (i = 0; i < want; i++) {
        if (buf[i] != first[i])
          varies[i] = 1;
      }
    }
  }
  for (i = 0; i < want; i++) {
    if (!touched[i])
      err = 1; /* a short fill left this byte alone in every round */
    live += varies[i];
  }
  /* Over 8 draws a live source moves all 64 bytes; half is a floor no real one
     misses, and a source stuck on one byte or a constant cannot reach it. */
  if (live < want / 2)
    err = 1;
  /* a zero-length ask succeeds and writes nothing */
  memset(buf, 0x5a, sizeof(buf));
  if (!hts_random_bytes(buf, 0))
    err = 1;
  for (i = 0; i < (int) sizeof(buf); i++) {
    if (buf[i] != 0x5a)
      err = 1;
  }
  printf("random: %s\n", err ? "FAIL" : "OK");
  return err;
}

/* Build a path of exactly len chars under base; returns that length. */
static size_t st_structcheck_longpath(char *dst, size_t dstsize,
                                      const char *base, size_t len) {
  size_t n = strlen(base);

  assertf(len < dstsize && n + 2 <= len);
  memmove(dst, base, n);
  while (n < len) {
    size_t seg = len - n - 1;

    if (seg > 200) /* stay under the usual 255-byte component limit */
      seg = len - n == 202 ? 199 : 200; /* never leave a bare separator */
    dst[n++] = '/';
    memset(dst + n, 'x', seg);
    n += seg;
  }
  dst[n] = '\0';
  return n;
}

/* The path guard, and the <name>.txt rename structcheck() performs when a
   regular file sits where a directory has to go (#745). */
static int st_structcheck(httrackp *opt, int argc, char **argv) {
  char BIGSTK path[HTS_URLMAXSIZE * 2];
  char BIGSTK target[HTS_URLMAXSIZE * 2];
  FILE *fp;

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "usage: -#test=structcheck <writable directory>\n");
    return 1;
  }

  /* over the guard: refused before a single directory is created */
  st_structcheck_longpath(path, sizeof(path), argv[0], HTS_URLMAXSIZE + 1);
  errno = 0;
  assertf(structcheck(path) == -1);
  assertf(errno == EINVAL);
  errno = 0;
  assertf(structcheck_utf8(path) == -1);
  assertf(errno == EINVAL);
  {
    char *const sep = strchr(path + strlen(argv[0]) + 1, '/');

    assertf(sep != NULL);
    sep[1] = '\0'; /* the outermost component it would have created */
    assertf(!dir_exists(path));
  }

  /* a regular file where a directory belongs is renamed to <name>.txt */
  snprintf(path, sizeof(path), "%s/sc", argv[0]);
  fp = fopen(path, "wb");
  assertf(fp != NULL);
  fclose(fp);
  snprintf(path, sizeof(path), "%s/sc/sub/", argv[0]);
  assertf(structcheck(path) == 0);
  assertf(dir_exists(path));
  snprintf(target, sizeof(target), "%s/sc.txt", argv[0]);
  assertf(fexist(target));

  /* the utf-8 entry point carries the same rename */
  snprintf(path, sizeof(path), "%s/u8", argv[0]);
  fp = FOPEN(path, "wb");
  assertf(fp != NULL);
  fclose(fp);
  snprintf(path, sizeof(path), "%s/u8/sub/", argv[0]);
  assertf(structcheck_utf8(path) == 0);
  assertf(dir_exists(path));
  snprintf(target, sizeof(target), "%s/u8.txt", argv[0]);
  assertf(fexist_utf8(target));

  printf("structcheck self-test OK\n");
  return 0;
}

/* #794: hts_gmtime() must own its output. The table is an independent oracle;
   the threaded phase is what corrupts if it ever goes back to gmtime()'s
   shared static. */
#define GMTIME_THREADS 8
#define GMTIME_ROUNDS 50000

static const struct {
  time_t t;
  int year, mon, mday, hour, min, sec, wday, yday;
} gmtime_refs[] = {
    {(time_t) 0, 70, 0, 1, 0, 0, 0, 4, 0},
    {(time_t) 951782400, 100, 1, 29, 0, 0, 0, 2, 59}, /* a leap day */
    {(time_t) 1000000000, 101, 8, 9, 1, 46, 40, 0, 251},
    {(time_t) 2147483647, 138, 0, 19, 3, 14, 7, 2, 18}, /* 32-bit ceiling */
};

#define GMTIME_REFS ((int) (sizeof(gmtime_refs) / sizeof(gmtime_refs[0])))

static hts_boolean gmtime_ref_matches(int i, const struct tm *tm) {
  if (tm->tm_year != gmtime_refs[i].year || tm->tm_mon != gmtime_refs[i].mon ||
      tm->tm_mday != gmtime_refs[i].mday ||
      tm->tm_hour != gmtime_refs[i].hour || tm->tm_min != gmtime_refs[i].min ||
      tm->tm_sec != gmtime_refs[i].sec || tm->tm_wday != gmtime_refs[i].wday ||
      tm->tm_yday != gmtime_refs[i].yday)
    return HTS_FALSE;
  return HTS_TRUE;
}

static htsmutex gmtime_lock = HTSMUTEX_INIT;
static int gmtime_bad = 0;

static void gmtime_thread(void *arg) {
  const int i = *(const int *) arg;
  int bad = 0, round;

  for (round = 0; round < GMTIME_ROUNDS; round++) {
    struct tm tmv;

    if (!hts_gmtime(gmtime_refs[i].t, &tmv) || !gmtime_ref_matches(i, &tmv))
      bad++;
  }
  hts_mutexlock(&gmtime_lock);
  gmtime_bad += bad;
  hts_mutexrelease(&gmtime_lock);
}

static int st_gmtime(httrackp *opt, int argc, char **argv) {
  static int idx[GMTIME_THREADS];
  int err = 0, i;

  (void) opt;
  (void) argc;
  (void) argv;

  for (i = 0; i < GMTIME_REFS; i++) {
    struct tm tmv;

    if (!hts_gmtime(gmtime_refs[i].t, &tmv)) {
      fprintf(stderr, "gmtime: conversion #%d failed\n", i);
      err = 1;
    } else if (!gmtime_ref_matches(i, &tmv)) {
      fprintf(stderr,
              "gmtime: #%d gave %04d-%02d-%02d %02d:%02d:%02d (wday %d, "
              "yday %d)\n",
              i, tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday, tmv.tm_hour,
              tmv.tm_min, tmv.tm_sec, tmv.tm_wday, tmv.tm_yday);
      err = 1;
    }
  }

  /* the return is the only failure signal the callers have, so a helper that
     always claims success leaves them formatting an uninitialised struct tm.
     Out of range for a 64-bit time_t: NULL from gmtime_r, EINVAL from
     _gmtime64_s. */
  if (sizeof(time_t) >= 8) {
    const time_t beyond = (time_t) INT64_MAX;
    struct tm tmv;

    if (hts_gmtime(beyond, &tmv)) {
      fprintf(stderr,
              "gmtime: an out-of-range time_t was reported converted\n");
      err = 1;
    }
  }

  for (i = 0; i < GMTIME_THREADS; i++) {
    idx[i] = i % GMTIME_REFS;
    if (hts_newthread(gmtime_thread, &idx[i]) != 0) {
      fprintf(stderr, "gmtime: cannot spawn\n");
      return 1;
    }
  }
  htsthread_wait();
  if (gmtime_bad != 0) {
    fprintf(stderr, "gmtime: %d/%d concurrent conversions were corrupt\n",
            gmtime_bad, GMTIME_THREADS * GMTIME_ROUNDS);
    err = 1;
  }

  printf("gmtime self-test: %s\n", err ? "FAIL" : "OK");
  return err;
}

/* #806: hts_localtime() must own its output too, same rationale as
   hts_gmtime() (#794). Reference table computed under TZ=XXX5 (fixed
   UTC-5, no DST), which the driving .test script sets. */
#define LOCALTIME_THREADS 8
#define LOCALTIME_ROUNDS 50000

static const struct {
  time_t t;
  int year, mon, mday, hour, min, sec, wday, yday;
} localtime_refs[] = {
    {(time_t) 0, 69, 11, 31, 19, 0, 0, 3, 364},
    {(time_t) 951782400, 100, 1, 28, 19, 0, 0, 1,
     58}, /* a leap day, GMT side */
    {(time_t) 1000000000, 101, 8, 8, 20, 46, 40, 6, 250},
    {(time_t) 2147483647, 138, 0, 18, 22, 14, 7, 1, 17},
};

#define LOCALTIME_REFS                                                         \
  ((int) (sizeof(localtime_refs) / sizeof(localtime_refs[0])))

static hts_boolean localtime_ref_matches(int i, const struct tm *tm) {
  if (tm->tm_year != localtime_refs[i].year ||
      tm->tm_mon != localtime_refs[i].mon ||
      tm->tm_mday != localtime_refs[i].mday ||
      tm->tm_hour != localtime_refs[i].hour ||
      tm->tm_min != localtime_refs[i].min ||
      tm->tm_sec != localtime_refs[i].sec ||
      tm->tm_wday != localtime_refs[i].wday ||
      tm->tm_yday != localtime_refs[i].yday)
    return HTS_FALSE;
  return HTS_TRUE;
}

static htsmutex localtime_lock = HTSMUTEX_INIT;
static int localtime_bad = 0;

static void localtime_thread(void *arg) {
  const int i = *(const int *) arg;
  int bad = 0, round;

  for (round = 0; round < LOCALTIME_ROUNDS; round++) {
    struct tm tmv;

    if (!hts_localtime(localtime_refs[i].t, &tmv) ||
        !localtime_ref_matches(i, &tmv))
      bad++;
  }
  hts_mutexlock(&localtime_lock);
  localtime_bad += bad;
  hts_mutexrelease(&localtime_lock);
}

static int st_localtime(httrackp *opt, int argc, char **argv) {
  static int idx[LOCALTIME_THREADS];
  int err = 0, i;

  (void) opt;

  if (argc < 1) {
    fprintf(stderr, "usage: -#test=localtime <writable directory>\n");
    return 1;
  }

  for (i = 0; i < LOCALTIME_REFS; i++) {
    struct tm tmv;

    if (!hts_localtime(localtime_refs[i].t, &tmv)) {
      fprintf(stderr, "localtime: conversion #%d failed\n", i);
      err = 1;
    } else if (!localtime_ref_matches(i, &tmv)) {
      fprintf(stderr,
              "localtime: #%d gave %04d-%02d-%02d %02d:%02d:%02d (wday %d, "
              "yday %d)\n",
              i, tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday, tmv.tm_hour,
              tmv.tm_min, tmv.tm_sec, tmv.tm_wday, tmv.tm_yday);
      err = 1;
    }
  }

  if (sizeof(time_t) >= 8) {
    const time_t beyond = (time_t) INT64_MAX;
    struct tm tmv;

    if (hts_localtime(beyond, &tmv)) {
      fprintf(stderr,
              "localtime: an out-of-range time_t was reported converted\n");
      err = 1;
    }
  }

  for (i = 0; i < LOCALTIME_THREADS; i++) {
    idx[i] = i % LOCALTIME_REFS;
    if (hts_newthread(localtime_thread, &idx[i]) != 0) {
      fprintf(stderr, "localtime: cannot spawn\n");
      return 1;
    }
  }
  htsthread_wait();
  if (localtime_bad != 0) {
    fprintf(stderr, "localtime: %d/%d concurrent conversions were corrupt\n",
            localtime_bad, LOCALTIME_THREADS * LOCALTIME_ROUNDS);
    err = 1;
  }

  /* get_filetime_rfc822() must report GMT, never the process's local zone,
     and never a silent fallback to it on gmtime() failure (#806). */
  {
    char path[HTS_URLMAXSIZE];
    char date[256];
    struct tm parsed;

    snprintf(path, sizeof(path), "%s/filetime.bin", argv[0]);
    structcheck(path);
    {
      FILE *fp = FOPEN(path, "wb");

      if (fp == NULL) {
        fprintf(stderr, "localtime: cannot write %s\n", path);
        return 1;
      }
      fputc('x', fp);
      fclose(fp);
    }
    if (set_filetime_rfc822(path, "Tue, 29 Feb 2000 00:00:00 GMT") != 0) {
      fprintf(stderr, "localtime: cannot set %s's mtime\n", path);
      err = 1;
    } else if (!get_filetime_rfc822(path, date)) {
      fprintf(stderr, "localtime: get_filetime_rfc822 failed on %s\n", path);
      err = 1;
    } else if (convert_time_rfc822(&parsed, date) == NULL ||
               parsed.tm_year != 100 || parsed.tm_mon != 1 ||
               parsed.tm_mday != 29 || parsed.tm_hour != 0) {
      fprintf(stderr,
              "localtime: get_filetime_rfc822 reported \"%s\" (TZ=%s)\n", date,
              getenv("TZ") ? getenv("TZ") : "");
      err = 1;
    }
  }

  printf("localtime self-test: %s\n", err ? "FAIL" : "OK");
  return err;
}

/* The x[strlen(x) - 1] class (#770), its pointer spelling x + strlen(x) - 1
   (#781) and its size_t index spelling (#821). The string starts mid-arena so
   the byte it must not touch is a real neighbour; poisoned with '#', not 0, or
   a stray NUL terminator would read as untouched. */
static int st_lastchar(httrackp *opt, int argc, char **argv) {
  enum { off = 8 };

  char arena[16];
  char *const s = &arena[off];
  const int guard = off - 1; /* what the old idiom clobbers */
  int err = 0;

  (void) opt;
  (void) argc;
  (void) argv;

#define REPOISON(str)                                                          \
  do {                                                                         \
    memset(arena, '#', sizeof(arena));                                         \
    strlcpybuff(s, (str), sizeof(arena) - off);                                \
  } while (0)
#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("  FAIL line %d: %s\n", __LINE__, #cond);                         \
      err = 1;                                                                 \
    }                                                                          \
  } while (0)

  /* the empty string: every helper must report "nothing" and touch nothing */
  REPOISON("");
  CHECK(hts_lastchar(s) == '\0');
  CHECK(arena[guard] == '#');
  REPOISON("");
  CHECK(hts_striplastchar(s, '/') == HTS_FALSE);
  CHECK(arena[guard] == '#');
  CHECK(s[0] == '\0');
  REPOISON("");
  CHECK(hts_choplastchar(s) == HTS_FALSE);
  CHECK(arena[guard] == '#');
  CHECK(s[0] == '\0');

  /* a '/' sitting where the underflow would land must not be mistaken for the
     string's own last byte -- this is the #768 shape */
  REPOISON("");
  arena[guard] = '/';
  CHECK(hts_lastchar(s) == '\0');
  CHECK(hts_striplastchar(s, '/') == HTS_FALSE);
  CHECK(arena[guard] == '/');

  /* non-empty: ordinary behaviour */
  REPOISON("ab/");
  CHECK(hts_lastchar(s) == '/');
  CHECK(hts_striplastchar(s, '/') == HTS_TRUE);
  CHECK(strcmp(s, "ab") == 0);
  CHECK(hts_striplastchar(s, '/') == HTS_FALSE);
  CHECK(strcmp(s, "ab") == 0);
  CHECK(hts_choplastchar(s) == HTS_TRUE);
  CHECK(strcmp(s, "a") == 0);
  CHECK(hts_choplastchar(s) == HTS_TRUE);
  CHECK(s[0] == '\0');
  CHECK(arena[guard] == '#');

  /* one-character string: the boundary the guards get wrong */
  REPOISON("/");
  CHECK(hts_lastchar(s) == '/');
  CHECK(hts_striplastchar(s, '/') == HTS_TRUE);
  CHECK(s[0] == '\0');
  CHECK(arena[guard] == '#');

  /* the pointer spelling (#781): on an empty string the address must be the
     terminating NUL, never the byte before it */
  REPOISON("");
  CHECK(hts_lastcharoffset(s) == 0);
  CHECK(hts_lastcharptr(s) == s);
  CHECK(*hts_lastcharptr(s) == '\0');
  *hts_lastcharptr(s) = 'Z'; /* a write through it must stay inside s */
  CHECK(arena[guard] == '#');
  CHECK(s[0] == 'Z');

  /* the neighbour must not be mistaken for the string's own last byte */
  REPOISON("");
  arena[guard] = '/';
  CHECK(hts_lastcharptr(s) == s);
  CHECK(*hts_lastcharptr(s) != '/');
  CHECK(arena[guard] == '/');

  /* the walk-back loops the sites use must stop at once on an empty string */
  REPOISON("");
  {
    const char *p = hts_lastcharptr(s);
    int steps = 0;

    while (p > s && *p != '/')
      p--, steps++;
    CHECK(steps == 0);
    CHECK(p == s);
  }

  REPOISON("ab/");
  CHECK(hts_lastcharoffset(s) == 2);
  CHECK(hts_lastcharptr(s) == s + 2);
  CHECK(*hts_lastcharptr(s) == '/');
  REPOISON("/");
  CHECK(hts_lastcharptr(s) == s);
  CHECK(*hts_lastcharptr(s) == '/');
  CHECK(arena[guard] == '#');

  /* the size_t index spelling (#821): (i > 0) cannot reject SIZE_MAX, so only
     a safe seed stops the sites' walk-back */
  REPOISON("");
  {
    const size_t vacuous = strlen(s) - 1;
    size_t i = hts_lastcharoffset(s);
    int steps = 0;

    CHECK(vacuous > 0);
    CHECK(i == 0);
    /* step-capped so a bad seed fails the count instead of running off */
    while ((i > 0) && (steps < 8) && (s[i] != '/'))
      i--, steps++;
    CHECK(steps == 0);
    CHECK(s[i] != '/');
    CHECK(arena[guard] == '#');
  }

  /* and the same loop must still find the real byte on a non-empty string */
  REPOISON("a/b");
  {
    size_t i = hts_lastcharoffset(s);
    int steps = 0;

    while ((i > 0) && (steps < 8) && (s[i] != '/'))
      i--, steps++;
    CHECK(i == 1);
    CHECK(arena[guard] == '#');
  }

  /* control: the canary must be able to fail, or the checks above prove
     nothing. Clobber it exactly as the unguarded idiom would. */
  REPOISON("");
  s[-1] = '\0';
  CHECK(arena[guard] != '#');
  REPOISON("");
  *(s + strlen(s) - 1) = 'X';
  CHECK(arena[guard] != '#');

#undef REPOISON
#undef CHECK

  printf("lastchar self-test: %s\n", err ? "FAIL" : "OK");
  return err;
}

/* hts_rtrim() and the sets it is called with. The string starts mid-arena, and
   the byte below it is poisoned with '#' rather than 0, or the stray NUL the
   old loop wrote there would read as untouched. */
static int st_rtrim(httrackp *opt, int argc, char **argv) {
  enum { off = 8 };

  char arena[24];
  char *const s = &arena[off];
  const int guard = off - 1;
  int err = 0;
  int c;

  (void) opt;
  (void) argc;
  (void) argv;

#define REPOISON(str)                                                          \
  do {                                                                         \
    memset(arena, '#', sizeof(arena));                                         \
    strlcpybuff(s, (str), sizeof(arena) - off);                                \
  } while (0)
#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("  FAIL line %d: %s\n", __LINE__, #cond);                         \
      err = 1;                                                                 \
    }                                                                          \
  } while (0)

  /* nothing but spaces: the case that ran the old loop off the front */
  REPOISON("   ");
  hts_rtrim(s, HTS_REALSPACES);
  CHECK(s[0] == '\0');
  CHECK(arena[guard] == '#');

  /* a space sitting below the string must not be eaten as if it were part of
     it, which is exactly what the old loop did */
  REPOISON("   ");
  arena[guard] = ' ';
  hts_rtrim(s, HTS_REALSPACES);
  CHECK(s[0] == '\0');
  CHECK(arena[guard] == ' ');

  REPOISON("");
  hts_rtrim(s, HTS_REALSPACES);
  CHECK(s[0] == '\0');
  CHECK(arena[guard] == '#');

  REPOISON("a b \t\r\n");
  hts_rtrim(s, HTS_REALSPACES);
  CHECK(strcmp(s, "a b") == 0);
  CHECK(arena[guard] == '#');

  REPOISON("ab");
  hts_rtrim(s, HTS_REALSPACES);
  CHECK(strcmp(s, "ab") == 0);

  /* quotes count as space for is_space() but not for is_realspace() */
  REPOISON("v\" ");
  hts_rtrim(s, HTS_REALSPACES);
  CHECK(strcmp(s, "v\"") == 0);
  REPOISON("v\" ");
  hts_rtrim(s, HTS_SPACES);
  CHECK(strcmp(s, "v") == 0);

  /* the sets must stay the macros they stand for */
  for (c = 1; c < 256; c++) {
    const char b = (char) c;

    CHECK((strchr(HTS_SPACES, b) != NULL) == (is_space(b) != 0));
    CHECK((strchr(HTS_REALSPACES, b) != NULL) == (is_realspace(b) != 0));
  }

  /* control: the canary must be able to fail */
  REPOISON("   ");
  s[-1] = '\0';
  CHECK(arena[guard] != '#');

#undef REPOISON
#undef CHECK

  printf("rtrim self-test: %s\n", err ? "FAIL" : "OK");
  return err;
}

/* Format LEN bytes of EXPECTED into S as two arguments, and check what came
   back. HEAD and TAIL are scratch buffers of at least LEN+1 bytes. */
static int strsprintf_case(String *s, const char *expected, size_t len,
                           char *head, char *tail) {
  const size_t half = len / 2;

  memcpy(head, expected, half);
  head[half] = '\0';
  memcpy(tail, expected + half, len - half);
  tail[len - half] = '\0';
  StringSprintf(*s, "%s%s", head, tail);
  return StringLength(*s) == len &&
         memcmp(StringBuff(*s), expected, len) == 0 &&
         StringBuff(*s)[len] == '\0';
}

/* StringSprintf_ stores the terminator at buffer[ret], so its `ret < capacity`
   guard is off by one byte at the exact fill: an output whose length equals the
   capacity writes past the allocation (#836). The lengths that reach it are the
   capacities themselves, floored at 256 and doubling from there. */
static int st_strsprintf(httrackp *opt, int argc, char **argv) {
  static const size_t caps[] = {256, 512, 1024, 2048};

  enum { maxLen = 2100 };

  char *expected = malloct(maxLen + 1);
  char *head = malloct(maxLen + 1);
  char *tail = malloct(maxLen + 1);
  String reused = STRING_EMPTY;
  size_t i, len;
  int err = 0;

  (void) opt;
  (void) argc;
  (void) argv;

  if (expected == NULL || head == NULL || tail == NULL) {
    printf("strsprintf self-test: FAIL (out of memory)\n");
    return 1;
  }
  for (i = 0; i < maxLen; i++)
    expected[i] = (char) ('a' + (i % 26));
  expected[maxLen] = '\0';

  /* one call on a String whose capacity is pinned to the boundary, so len ==
     capacity is reached exactly once per boundary */
  for (i = 0; !err && i < sizeof(caps) / sizeof(caps[0]); i++) {
    for (len = caps[i] - 3; !err && len <= caps[i] + 3; len++) {
      String s = STRING_EMPTY;

      StringRoomTotal(s, caps[i]);
      if (StringCapacity(s) != caps[i]) {
        printf("  FAIL: capacity %u pinned to %u\n", (unsigned) caps[i],
               (unsigned) StringCapacity(s));
        err = 1;
      } else if (!strsprintf_case(&s, expected, len, head, tail)) {
        printf("  FAIL: length %u at capacity %u\n", (unsigned) len,
               (unsigned) caps[i]);
        err = 1;
      }
      StringFree(s);
    }
  }

  /* the same String reused: its capacity grows under it between calls, and a
     shorter output must not leave the previous one behind */
  for (len = 0; !err && len <= maxLen; len++) {
    if (!strsprintf_case(&reused, expected, len, head, tail)) {
      printf("  FAIL: growing length %u\n", (unsigned) len);
      err = 1;
    }
  }
  for (len = maxLen + 1; !err && len-- > 0;) {
    if (!strsprintf_case(&reused, expected, len, head, tail)) {
      printf("  FAIL: shrinking length %u\n", (unsigned) len);
      err = 1;
    }
  }
  StringFree(reused);

  /* The give-up path: an argument libc cannot convert fails at every capacity,
     so the retry loop climbs to STRING_SPRINTF_MAX and then empties the
     String. Probe libc first -- a platform that formats an unpaired surrogate
     without faulting never reaches the path. */
  {
    static const wchar_t bad[] = {(wchar_t) 0xd800, 0};
    char probe[32];

    if (snprintf(probe, sizeof(probe), "%ls", bad) < 0) {
      String s = STRING_EMPTY;

      StringCopy(s, "leftover");
      StringSprintf(s, "%ls", bad);
      if (StringNotEmpty(s) || StringBuff(s) == NULL ||
          StringBuff(s)[0] != '\0') {
        printf("  FAIL: a failed conversion left %u bytes behind\n",
               (unsigned) StringLength(s));
        err = 1;
      }
      StringFree(s);
    } else { /* stderr: test 150 pins stdout to the one-line verdict */
      fprintf(stderr, "  (skipped: this libc formats an unconvertible wide "
                      "string)\n");
    }
  }

  /* StringSprintf empties the String when it gives up, and the WebDAV
     enumeration pops the trailing '/' right after: on an empty String an
     unguarded pop would wrap the unsigned length and write off the end. */
  {
    String never = STRING_EMPTY;
    String cleared = STRING_EMPTY;

    StringPopRight(never); /* never written to: buffer_ is still NULL */
    if (StringLength(never) != 0 || StringBuff(never) != NULL) {
      printf("  FAIL: pop on an unallocated String\n");
      err = 1;
    }
    StringClear(cleared);
    StringPopRight(cleared);
    if (StringLength(cleared) != 0 || StringBuff(cleared)[0] != '\0') {
      printf("  FAIL: pop on an emptied String\n");
      err = 1;
    }
    /* control: the guard must not swallow a pop that has a byte to drop */
    StringSprintf(cleared, "ab");
    StringPopRight(cleared);
    if (StringLength(cleared) != 1 || strcmp(StringBuff(cleared), "a") != 0) {
      printf("  FAIL: pop on a non-empty String\n");
      err = 1;
    }
    StringFree(never);
    StringFree(cleared);
  }

  freet(expected);
  freet(head);
  freet(tail);

  printf("strsprintf self-test: %s\n", err ? "FAIL" : "OK");
  return err;
}

/* ------------------------------------------------------------ */
/* Registry: this module's tests, in the order -#test lists them. */
/* ------------------------------------------------------------ */

const struct selftest_entry selftests_lib[] = {
    {"hashtable", "<count|file>", "coucal hashtable stress test", st_hashtable},
    {"strsafe", "[overflow|overflow-buff|overflow-src [str]]",
     "bounded string-op self-test", st_strsafe},
    {"strsprintf", "", "StringSprintf grows to fit at every capacity boundary",
     st_strsprintf},
    {"arena", "", "htsarena.h hands out addresses that never move", st_arena},
    {"arrays", "[overflow-capa|overflow-loop]",
     "htsarrays.h growth reaches the requested room, or reports it failed",
     st_arrays},
    {"ucs4-oom", "", "a UCS4 re-encode the allocator refuses aborts",
     st_ucs4_oom},
    {"ucs2", "[oom]", "UCS2 to UTF-8 re-encoding, and its failure return",
     st_ucs2},
    {"lastchar", "",
     "last-char helpers never index before the buffer (#770, #781, #821)",
     st_lastchar},
    {"rtrim", "", "hts_rtrim never walks below the buffer", st_rtrim},
    {"gmtime", "",
     "hts_gmtime() fills the caller's buffer, not a static (#794)", st_gmtime},
    {"localtime", "<dir>",
     "hts_localtime() and get_filetime_rfc822()'s GMT labelling (#806)",
     st_localtime},
    {"random", "", "hts_random_bytes() fills exactly the requested length",
     st_random},
    {"structcheck", "<dir>",
     "structcheck path guard and the <name>.txt rename it performs",
     st_structcheck},
    {NULL, NULL, NULL, NULL},
};
