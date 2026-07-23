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
#include "htsalias.h"
#include "htsparse.h"
#include "htscache.h"
#include "htscache_selftest.h"
#include "htsdns_selftest.h"
#include "htscharset.h"
#include "htsencoding.h"
#include "htsftp.h"
#include "htsmd5.h"
#include "htssniff.h"
#include "htscodec.h"
#include "htsproxy.h"
#include "htswarc.h"
#if HTS_USEZLIB
#include "htszlib.h"
#endif
#if HTS_USEZSTD
#include <zstd.h>
#endif
#if HTS_USEOPENSSL
#include <openssl/evp.h>
#endif
#include "coucal/coucal.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <unistd.h>
#else
#include <io.h>       /* _get_osfhandle, for the sparse-file hint */
#include <winioctl.h> /* FSCTL_SET_SPARSE */
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

/* Length/work caps stop a hostile pattern stack-overflowing or hanging the
   process (OSS-Fuzz 5060751291908096 / 5745936014573568). */
static int st_filterbounds(httrackp *opt, int argc, char **argv) {
  const size_t big = 100000; /* well past the length cap */
  const size_t stars = 1023; /* pattern len 2047, under the length cap */
  const size_t subjlen = 2048;
  char *subj = malloct(big + 1);
  char *pat = malloct(2 * stars + 2);
  size_t steps = 0, maxsteps = 0, depth = 0, maxdepth = 0, i;

  (void) opt;
  (void) argc;
  (void) argv;
  memset(subj, 'a', big);
  subj[big] = '\0';
  /* '*' matches anything, but an over-length subject trips the length cap */
  assertf(strjoker(subj, "*", NULL, NULL) == NULL);
  assertf(strjokerfind(subj, "*") == NULL);
  /* Star-heavy dead-end at the length cap: unbounded it runs ~1.26e9 memo-steps
     (~6s). */
  for (i = 0; i < stars; i++) {
    pat[2 * i] = '*';
    pat[2 * i + 1] = 'a';
  }
  pat[2 * stars] = 'b'; /* never matches an all-'a' subject */
  pat[2 * stars + 1] = '\0';
  subj[subjlen] = '\0';
  /* Budget must fire and hold: steps > cap (deleting the budget zeroes the
     counter that is the enforcement), steps < 10*cap (unbudgeted ~1.26e9). */
  assertf(strjoker_bounds(subj, pat, &steps, &maxsteps, &depth, &maxdepth) ==
          NULL);
  assertf(steps > maxsteps && steps < 10 * maxsteps);
  /* Depth caps the stack: uncapped this recurses 2046 frames, ~900KB (#574). */
  assertf(depth == maxdepth);
  assertf(strjokerfind(subj, pat) == NULL);
  /* Pin the cap from below: 32 segments must still match, so a cap set so low
     it would break real multi-segment filters (which use far fewer) fails. */
  for (i = 0; i < 32; i++) {
    pat[2 * i] = '*';
    pat[2 * i + 1] = 'a';
  }
  pat[64] = '\0';
  memset(subj, 'a', 32);
  subj[32] = '\0';
  assertf(strjoker(subj, pat, NULL, NULL) != NULL);
  /* Same pin for the class-branch shape users actually write (*[..]), against a
     long subject: it must match with room to spare under the work cap. */
  {
    const char *seg = "*[A-Z,a-z,0-9]";
    const size_t seglen = strlen(seg), nseg = 16;

    for (i = 0; i < (int) nseg; i++)
      memcpy(pat + i * seglen, seg, seglen);
    pat[nseg * seglen] = '\0';
    memset(subj, 'a', 512);
    subj[512] = '\0';
    assertf(strjoker_bounds(subj, pat, &steps, &maxsteps, NULL, NULL) != NULL);
    assertf(steps < maxsteps);
  }
  freet(pat);
  freet(subj);
  printf("filterbounds: OK\n");
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

static int st_expandhome(httrackp *opt, int argc, char **argv) {
  String path = STRING_EMPTY;

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "expandhome: needs a path\n");
    return 1;
  }
  StringCopy(path, argv[0]);
  expand_home(&path);
  printf("expanded=%s\n", StringBuff(path));
  StringFree(path);
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

static size_t st_decode_body(const char *arg, char *buf, size_t size);

static int st_charset(httrackp *opt, int argc, char **argv) {
  char buf[512];
  size_t len;
  char *s;

  (void) opt;
  if (argc < 2) {
    fprintf(stderr, "charset: needs a charset and a string\n");
    return 1;
  }
  len = st_decode_body(argv[1], buf, sizeof(buf));
  s = hts_convertStringToUTF8(buf, len, argv[0]);
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
  char buf[512];
  size_t len;

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "isutf8: needs a string\n");
    return 1;
  }
  len = st_decode_body(argv[0], buf, sizeof(buf));
  printf("%d\n", hts_isStringUTF8(buf, len) ? 1 : 0);
  return 0;
}

static int st_idna_encode(httrackp *opt, int argc, char **argv) {
  char buf[512];
  size_t len;
  char *s;

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "idna-encode: needs a hostname\n");
    return 1;
  }
  len = st_decode_body(argv[0], buf, sizeof(buf));
  s = hts_convertStringUTF8ToIDNA(buf, len);
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

// -#test=footerfmt <template>: expand a -%F footer with fixed fields (drives
// tests/01_engine-footerfmt.test). Also asserts the overflow/zero-size returns
// the CLI cap keeps out of reach.
static int st_footerfmt(httrackp *opt, int argc, char **argv) {
  static const hts_footer_field fields[] = {
      {"addr", "host.example"},
      {"path", "/dir/page.html"},
      {"url", "http://host.example/dir/page.html"},
      {"date", "DATE"},
      {"lastmodified", "LASTMOD"},
      {"version", "VER"},
      {"mime", "text/html"},
      {"charset", "utf-8"},
      {"status", "200"},
      {"size", "1234"},
  };
  const size_t nfields = sizeof(fields) / sizeof(fields[0]);
  char out[1024];
  char tiny[4];

  (void) opt;
  // Overflow (named and legacy) and a zero-size buffer must return <0, never
  // truncate silently or write out of bounds.
  assertf(hts_footer_format(tiny, sizeof(tiny), "{addr}", fields, nfields) < 0);
  assertf(hts_footer_format(tiny, sizeof(tiny), "a %s b", fields, nfields) < 0);
  assertf(hts_footer_format(out, 0, "", fields, nfields) < 0);
  // An empty template yields an empty, terminated string.
  assertf(hts_footer_format(out, sizeof(out), "", fields, nfields) == 1 &&
          out[0] == '\0');
  if (argc < 1) {
    fprintf(stderr, "footerfmt: needs a template\n");
    return 1;
  }
  if (hts_footer_format(out, sizeof(out), argv[0], fields, nfields) < 0) {
    fprintf(stderr, "footerfmt: overflow\n");
    return 1;
  }
  printf("%s\n", out);
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
    const LLint size = fsize(snum);
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
    /* Not sizeof(char*): on ILP32 a char[4] equals the pointer size, and the
       MSVC array-vs-pointer heuristic (sizeof(A) != sizeof(char*)) then reads
       it as a pointer and silently skips the bound. */
    char small[6];
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

  /* warc_file: same String deep-copy path as cookies_file */
  StringCopy(from->warc_file, "run.warc.gz");
  StringCopy(to->warc_file, "");
  copy_htsopt(from, to);
  if (strcmp(StringBuff(to->warc_file), "run.warc.gz") != 0)
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

static int st_proxyurl(httrackp *opt, int argc, char **argv) {
  char BIGSTK name[HTS_URLMAXSIZE * 2];
  int port = -1;

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "proxyurl: needs a proxy argument\n");
    return 1;
  }
  hts_parse_proxy(argv[0], name, sizeof(name), &port);
  // host= is the resolved host (scheme/userinfo stripped); kind= the transport
  printf("name=%s port=%d host=%s kind=%s\n", name, port,
         jump_identification_const(name),
         hts_proxy_is_socks(name)     ? "socks"
         : hts_proxy_is_connect(name) ? "connect"
                                      : "http");
  return 0;
}

/* Scripted SOCKS5 server: no-auth method reply, then a success CONNECT reply
   carrying the given ATYP address, then a sentinel origin byte the handshake
   must leave unread (an off-by-one in the reply framing eats it). */
static size_t socks5_reply(unsigned char *buf, unsigned char atyp,
                           const unsigned char *addr, size_t addrlen) {
  size_t n = 0;

  buf[n++] = 0x05;
  buf[n++] = 0x00; /* method: no authentication */
  buf[n++] = 0x05;
  buf[n++] = 0x00; /* REP: succeeded */
  buf[n++] = 0x00;
  buf[n++] = atyp;
  memcpy(buf + n, addr, addrlen);
  n += addrlen;
  buf[n++] = 0x1f;
  buf[n++] = 0x90; /* BND.PORT */
  buf[n++] = 0xAA; /* sentinel */
  return n;
}

#define SOCKS5_SENTINEL_LEFT(io, len) ((io).consumed == (len) - 1)

static int st_socks5(httrackp *opt, int argc, char **argv) {
  static const unsigned char v4[4] = {127, 0, 0, 1};
  static const unsigned char v6[16] = {0};
  static const unsigned char domain[6] = {5, 'p', 'r', 'o', 'x', 'y'};
  const char *const proxy = "socks5://127.0.0.1";
  unsigned char script[64];
  socks5_test_io io;
  size_t len;

  (void) argc;
  (void) argv;

  /* each reply address type is drained exactly, sentinel untouched */
  len = socks5_reply(script, 0x01, v4, sizeof(v4));
  io.reply = script;
  io.reply_len = len;
  assertf(socks5_handshake_scripted(opt, "origin.test", proxy, &io) == 1);
  assertf(SOCKS5_SENTINEL_LEFT(io, len));
  /* the greeting offers no-auth only, and the origin goes out by name, port 80
   */
  assertf(io.sent_len == 3 + 7 + 11);
  assertf(memcmp(io.sent, "\x05\x01\x00", 3) == 0);
  assertf(memcmp(io.sent + 3, "\x05\x01\x00\x03\x0borigin.test\x00\x50", 18) ==
          0);

  len = socks5_reply(script, 0x04, v6, sizeof(v6));
  io.reply = script;
  io.reply_len = len;
  assertf(socks5_handshake_scripted(opt, "origin.test", proxy, &io) == 1);
  assertf(SOCKS5_SENTINEL_LEFT(io, len));

  len = socks5_reply(script, 0x03, domain, sizeof(domain));
  io.reply = script;
  io.reply_len = len;
  assertf(socks5_handshake_scripted(opt, "origin.test", proxy, &io) == 1);
  assertf(SOCKS5_SENTINEL_LEFT(io, len));

  /* an unknown address type has no known length: fail, never guess */
  len = socks5_reply(script, 0x02, v4, sizeof(v4));
  io.reply = script;
  io.reply_len = len;
  assertf(socks5_handshake_scripted(opt, "origin.test", proxy, &io) == 0);

  /* truncated frames (here: no BND.PORT) fail instead of over-reading */
  len = socks5_reply(script, 0x01, v4, sizeof(v4)) - 3;
  io.reply = script;
  io.reply_len = len;
  assertf(socks5_handshake_scripted(opt, "origin.test", proxy, &io) == 0);

  /* explicit origin port is encoded big-endian (8443 = 0x20fb) */
  len = socks5_reply(script, 0x01, v4, sizeof(v4));
  io.reply = script;
  io.reply_len = len;
  assertf(socks5_handshake_scripted(opt, "origin.test:8443", proxy, &io) == 1);
  assertf(memcmp(io.sent + io.sent_len - 2, "\x20\xfb", 2) == 0);

  /* a bad origin port is refused before any byte goes out (#614). 4294967376 is
     the case the old range check could not see: it overflowed the sscanf("%d")
     into a plausible 80 and passed. 65616 would not prove anything here, since
     it fits an int and the old check already caught it. */
  {
    static const char *const bad[] = {"origin.test:4294967376",
                                      "origin.test:80x", "origin.test:+80",
                                      "origin.test: 80", "origin.test:8.0"};
    size_t k;

    for (k = 0; k < sizeof(bad) / sizeof(bad[0]); k++) {
      len = socks5_reply(script, 0x01, v4, sizeof(v4));
      io.reply = script;
      io.reply_len = len;
      assertf(socks5_handshake_scripted(opt, bad[k], proxy, &io) == 0);
      assertf(io.sent_len == 0);
    }
  }

  /* credentials: split on the first colon of the escaped userinfo, so %3a stays
     inside the username and a colon in the password is not a delimiter */
  {
    static const unsigned char auth_script[] = {
        0x05, 0x02,             /* method: user/pass */
        0x01, 0x00,             /* auth: success */
        0x05, 0x00, 0x00, 0x01, /* REP: succeeded, ATYP ipv4 */
        127,  0,    0,    1,    0x1f, 0x90, 0xAA};

    io.reply = auth_script;
    io.reply_len = sizeof(auth_script);
    assertf(socks5_handshake_scripted(opt, "origin.test",
                                      "socks5://us%3aer:p:ass@127.0.0.1",
                                      &io) == 1);
    assertf(SOCKS5_SENTINEL_LEFT(io, sizeof(auth_script)));
    assertf(memcmp(io.sent, "\x05\x02\x00\x02", 4) == 0);
    assertf(memcmp(io.sent + 4, "\x01\x05us:er\x05p:ass", 13) == 0);
  }

  /* a proxy demanding auth we cannot provide, and one refusing every method */
  io.reply = (const unsigned char *) "\x05\x02";
  io.reply_len = 2;
  assertf(socks5_handshake_scripted(opt, "origin.test", proxy, &io) == 0);
  io.reply = (const unsigned char *) "\x05\xff";
  io.reply_len = 2;
  assertf(socks5_handshake_scripted(opt, "origin.test", proxy, &io) == 0);

  /* a refused CONNECT is an error, not a tunnel */
  io.reply = (const unsigned char
                  *) "\x05\x00\x05\x05\x00\x01\x7f\x00\x00\x01\x1f\x90";
  io.reply_len = 12;
  assertf(socks5_handshake_scripted(opt, "origin.test", proxy, &io) == 0);

  /* over-long host or credentials are rejected before anything is sent */
  {
    char host[512];
    char name[1024];
    size_t i;

    host[0] = '\0';
    for (i = 0; i < 256; i++)
      strcatbuff(host, "a");
    io.reply = script;
    io.reply_len = len;
    assertf(socks5_handshake_scripted(opt, host, proxy, &io) == 0);
    assertf(io.sent_len == 0);

    strcpybuff(name, "socks5://user:");
    for (i = 0; i < 256; i++)
      strcatbuff(name, "p");
    strcatbuff(name, "@127.0.0.1");
    io.reply = script;
    io.reply_len = len;
    assertf(socks5_handshake_scripted(opt, "origin.test", name, &io) == 0);
    assertf(io.sent_len == 0);
  }

  /* the request is always ATYP=domain, which cannot carry an IPv6 literal: a
     bracketed origin is rejected rather than sent as a bogus domain name. The
     msg check pins the reason: a stricter host validator would also reject
     these, but for the wrong cause. */
  io.reply = script;
  io.reply_len = len;
  assertf(socks5_handshake_scripted(opt, "[::1]", proxy, &io) == 0);
  assertf(io.sent_len == 0);
  assertf(strstr(io.msg, "IPv6") != NULL);
  io.reply = script;
  io.reply_len = len;
  assertf(socks5_handshake_scripted(opt, "[2001:db8::1]:8443", proxy, &io) ==
          0);
  assertf(io.sent_len == 0);
  assertf(strstr(io.msg, "IPv6") != NULL);

  printf("socks5 self-test OK\n");
  return 0;
}

/* Regression for the one-byte fil[] overflow: a 2047-byte hostless "?"-URL used
   to abort in strncat_safe_ when the missing leading '/' pushed fil to 2048. */
static int st_identabs(httrackp *opt, int argc, char **argv) {
  lien_adrfil af;
  const size_t len =
      sizeof(af.fil) - 1; /* 2047: max URL the top guard admits */
  char *url = malloct(len + 1);

  (void) opt;
  (void) argc;
  (void) argv;
  url[0] = '?';
  memset(url + 1, 'a', len - 1);
  url[len] = '\0';
  assertf(ident_url_absolute(url, &af) == -1);
  freet(url);
  /* valid URLs still parse, so the guard is not over-rejecting */
  assertf(ident_url_absolute("http://www.example.com/a/b/c.html?x=1", &af) ==
          0);
  assertf(ident_url_absolute("www.foo.com?bar=1", &af) == 0);
  printf("identabs self-test OK\n");
  return 0;
}

/* Default-port strip is scheme-aware (#638), overflow-safe (#614): a scheme's
   own default (any spelling) is dropped, a real port stays; guards #627. */
static int st_stripport(httrackp *opt, int argc, char **argv) {
  static const struct {
    const char *in, *out;
  } cases[] = {
      {"http://127.0.0.1:80/x", "http://127.0.0.1/x"},
      {"http://127.0.0.1:080/x", "http://127.0.0.1/x"},
      {"http://127.0.0.1:0080/x", "http://127.0.0.1/x"},
      {"http://127.0.0.1:80", "http://127.0.0.1"},
      {"http://127.0.0.1:0081/x", "http://127.0.0.1:0081/x"},
      {"http://127.0.0.1:81/x", "http://127.0.0.1:81/x"},
      {"http://127.0.0.1:8080/x", "http://127.0.0.1:8080/x"},
      {"http://127.0.0.1:4294967376/x", "http://127.0.0.1:4294967376/x"},
      {"http://127.0.0.1/x", "http://127.0.0.1/x"},
      {"https://127.0.0.1:443/x", "https://127.0.0.1/x"},
      {"https://127.0.0.1:80/x", "https://127.0.0.1:80/x"},
      // Scheme match is case-insensitive: HTTPS' default is 443, so :80 stays.
      {"HTTPS://127.0.0.1:80/x", "HTTPS://127.0.0.1:80/x"},
      {"ftp://127.0.0.1:21/x", "ftp://127.0.0.1/x"},
      {"ftp://127.0.0.1:80/x", "ftp://127.0.0.1:80/x"},
      {"http://127.0.0.1:443/x", "http://127.0.0.1:443/x"},
  };

  size_t k;

  (void) opt;
  (void) argc;
  (void) argv;
  for (k = 0; k < sizeof(cases) / sizeof(cases[0]); k++) {
    char BIGSTK buff[HTS_URLMAXSIZE * 2];

    strcpybuff(buff, cases[k].in);
    hts_strip_default_port(buff, sizeof(buff));
    assertf(strcmp(buff, cases[k].out) == 0);
  }
  printf("stripport self-test OK\n");
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

/* http_xfread1 must refuse an in-memory buffer whose size would exceed a 32-bit
   index (hostile Content-Length or endless stream) rather than allocate it.
   The guard returns before any socket read, so no real connection is needed. */
static int st_xfread_limit(httrackp *opt, int argc, char **argv) {
  htsblk r;

  (void) opt;
  (void) argc;
  (void) argv;

  // Content-Length just over 2 GiB.
  memset(&r, 0, sizeof(r));
  r.soc = INVALID_SOCKET;
  r.totalsize = (LLint) INT32_MAX + 1;
  printf("bylen: refused=%d adr=%s msg=%s\n",
         http_xfread1(&r, 8192) == READ_ERROR, r.adr != NULL ? "alloc" : "null",
         r.msg);
  if (r.adr != NULL)
    freet(r.adr);

  // Unknown length, buffer already at the limit: the next read would exceed it.
  memset(&r, 0, sizeof(r));
  r.soc = INVALID_SOCKET;
  r.totalsize = -1;
  r.size = (LLint) INT32_MAX;
  printf("bygrow: refused=%d adr=%s msg=%s\n",
         http_xfread1(&r, 8192) == READ_ERROR, r.adr != NULL ? "alloc" : "null",
         r.msg);
  if (r.adr != NULL)
    freet(r.adr);

  // Exactly at the 2 GiB index (size + bufl == INT32_MAX): must also be
  // refused, since the reallocs below add 1 (a `> INT32_MAX` check would let
  // this through and overflow the int realloc size).
  memset(&r, 0, sizeof(r));
  r.soc = INVALID_SOCKET;
  r.totalsize = -1;
  r.size = (LLint) INT32_MAX - 8192;
  http_xfread1(&r, 8192);
  printf("boundary: msg=%s\n", r.msg);

  // A legitimate small size must NOT be refused by the guard (the read then
  // fails on the invalid socket, but the size-too-large msg must not be set).
  memset(&r, 0, sizeof(r));
  r.soc = INVALID_SOCKET;
  r.totalsize = 1000;
  http_xfread1(&r, 8192);
  printf("accept: msg=%s\n", r.msg);
  if (r.adr != NULL)
    freet(r.adr);
  return 0;
}

/* Parse a Content-Range header and print the sanitized triple. A hostile value
   (negative or INT64 extreme) must clamp to 0 without signed-overflow UB. */
static int st_crange(httrackp *opt, int argc, char **argv) {
  int i;

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "crange: needs at least one raw Content-Range line\n");
    return 1;
  }
  for (i = 0; i < argc; i++) {
    htsblk r;
    char BIGSTK line[HTS_URLMAXSIZE * 2];

    memset(&r, 0, sizeof(r));
    strcpybuff(line, argv[i]);
    treathead(NULL, "www.example.com", "/", &r, line);
    printf("crange_start=" LLintP " crange_end=" LLintP " crange=" LLintP "\n",
           (LLint) r.crange_start, (LLint) r.crange_end, (LLint) r.crange);
  }
  return 0;
}

/* Decode an argument ("hex:FFD8.." or literal text) into buf. Raw non-UTF-8
   bytes cannot survive a Windows command line, so hostile inputs go as hex. */
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

/* fsize()/fsize_utf8()/fpsize() must report a size past 4GB: 32-bit wraps both
   ways there (MSVC's off_t and struct _stat st_size are long, 32-bit even on
   x64), and a size under 4GB would survive an *unsigned* 32-bit truncation. */
static int st_fsize(httrackp *opt, int argc, char **argv) {
  const LLint expected = 5 * 1024 * 1024 * 1024LL;
  char BIGSTK path[HTS_URLMAXSIZE * 2];
  char BIGSTK absent[HTS_URLMAXSIZE * 2];
  /* both variants: only fsize() feeds the >2GB readers (file://, --list) */
  const int width = (int) sizeof(fsize(""));
  const int width_utf8 = (int) sizeof(fsize_utf8(""));
  FILE *fp;
  LLint got, got_utf8, gotp, gone;
  int rc = 0;

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "fsize: needs a directory\n");
    return 1;
  }
  concat(path, sizeof(path), argv[0], "/sparse-5g.bin");
  concat(absent, sizeof(absent), argv[0], "/no-such-file.bin");

  /* sparse: seek past 4GB and write the last byte */
  fp = FOPEN(path, "wb");
  if (fp == NULL) {
    fprintf(stderr, "fsize: cannot create '%s': %s\n", path, strerror(errno));
    return 1;
  }
#ifdef _WIN32
  {
    /* NTFS allocates the hole unless asked not to; POSIX gives it for free.
       Best-effort: a non-sparse file still measures the same, just costs 5GB.
     */
    HANDLE h = (HANDLE) _get_osfhandle(_fileno(fp));
    DWORD ret;

    if (h != INVALID_HANDLE_VALUE)
      (void) DeviceIoControl(h, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &ret, NULL);
  }
#endif
  if (fseeko(fp, expected - 1, SEEK_SET) != 0 || fputc(0, fp) == EOF ||
      fclose(fp) != 0) {
    fprintf(stderr, "fsize: cannot extend '%s' to " LLintP ": %s\n", path,
            expected, strerror(errno));
    UNLINK(path);
    return 1;
  }

  got = fsize(path);
  got_utf8 = fsize_utf8(path);
  fp = FOPEN(path, "rb");
  if (fp == NULL) {
    fprintf(stderr, "fsize: cannot reopen '%s': %s\n", path, strerror(errno));
    gotp = -1;
  } else {
    gotp = fpsize(fp);
    fclose(fp);
  }
  UNLINK(path);
  gone = fsize(absent); /* contract: -1, not 0, when absent */

  printf("fsize: width=%d,%d size=" LLintP "," LLintP " psize=" LLintP
         " absent=" LLintP "\n",
         width, width_utf8, got, got_utf8, gotp, gone);
  if (width != 8 || width_utf8 != 8) {
    fprintf(stderr, "fsize: return types are %d/%d bytes, expected 8\n", width,
            width_utf8);
    rc = 1;
  }
  if (got != expected || got_utf8 != expected) {
    fprintf(stderr,
            "fsize: fsize/fsize_utf8 are " LLintP "/" LLintP
            ", expected " LLintP "\n",
            got, got_utf8, expected);
    rc = 1;
  }
  if (gotp != expected) {
    fprintf(stderr, "fsize: fpsize is " LLintP ", expected " LLintP "\n", gotp,
            expected);
    rc = 1;
  }
  if (gone != -1) {
    fprintf(stderr, "fsize: absent file is " LLintP ", expected -1\n", gone);
    rc = 1;
  }
  return rc;
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

/* Drives unzRepair over a damaged local file header whose CRC field's high
   16-bit word has bit 15 set. Before the READ_32 fix that shifted an int and
   overflowed, so UBSan aborts here; after it, repair recovers the one entry. */
static int st_zip_repair_shift(httrackp *opt, int argc, char **argv) {
  static const unsigned char zip[] = {
      0x50, 0x4b, 0x03, 0x04, /* local file header signature */
      0x14, 0x00,             /* version needed */
      0x00, 0x00,             /* general purpose flag */
      0x00, 0x00,             /* method */
      0x00, 0x00,             /* time */
      0x00, 0x00,             /* date */
      0x00, 0x00, 0xe8, 0x8a, /* crc: high word 0x8ae8, bit 15 set */
      0x00, 0x00, 0x00, 0x00, /* compressed size */
      0x00, 0x00, 0x00, 0x00, /* uncompressed size */
      0x01, 0x00,             /* filename length */
      0x00, 0x00,             /* extra field length */
      0x61                    /* filename "a" */
  };
  char in[HTS_URLMAXSIZE], out[HTS_URLMAXSIZE], tmp[HTS_URLMAXSIZE];
  uLong nrec = 0, bytes = 0;
  FILE *fp;
  int err;

  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "zip-repair-shift: needs a directory\n");
    return 1;
  }
  snprintf(in, sizeof(in), "%s/damaged.zip", argv[0]);
  snprintf(out, sizeof(out), "%s/repair.zip", argv[0]);
  snprintf(tmp, sizeof(tmp), "%s/repair.tmp", argv[0]);
  fp = fopen(in, "wb");
  if (fp == NULL || fwrite(zip, 1, sizeof(zip), fp) != sizeof(zip)) {
    if (fp != NULL)
      fclose(fp);
    fprintf(stderr, "zip-repair-shift: cannot write %s\n", in);
    return 1;
  }
  fclose(fp);
  err = unzRepair(in, out, tmp, &nrec, &bytes);
  printf("zip-repair-shift: %s (recovered %lu entr%s)\n",
         (err == Z_OK && nrec == 1) ? "OK" : "FAIL", (unsigned long) nrec,
         nrec == 1 ? "y" : "ies");
  return (err == Z_OK && nrec == 1) ? 0 : 1;
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

static int st_dnstimeout(httrackp *opt, int argc, char **argv) {
  const int err = dns_timeout_selftests(opt);

  (void) argc;
  (void) argv;
  printf("dns-timeout-selftest: %s\n", err ? "FAIL" : "OK");
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

  http_cookie_header(&cookie, dom, "/", hdr, sizeof(hdr));
  if (strcmp(hdr, expected) != 0)
    err = 1;

  /* A hostile over-long request host must not overflow domain[256] in
     treathead's default-domain copy (that would abort the mirror). */
  {
    static t_cookie ck2;
    htsblk r;
    char host[600];

    memset(&r, 0, sizeof(r));
    memset(host, 'a', sizeof(host) - 1);
    host[sizeof(host) - 1] = '\0';
    ck2.max_len = (int) sizeof(ck2.data);
    ck2.data[0] = '\0';
    treathead(&ck2, host, "/", &r, "Set-Cookie: SID=1; path=/");
    if (strnotempty(ck2.data)) // oversize-host cookie was not dropped
      err = 1;
    /* control: a normal host still yields a cookie through treathead */
    treathead(&ck2, dom, "/", &r, "Set-Cookie: SID=1; path=/");
    if (strstr(ck2.data, "SID") == NULL) // guard wrongly dropped a valid cookie
      err = 1;
  }
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

// hts_buildtopindex() writes a system-charset name into a charset=utf-8 doc: on
// Windows the gifs land in a mangled twin dir (#217) and a listed name renders
// as mojibake (#216). Both must come out utf-8. argv[0] is writable.
static int st_topindex(httrackp *opt, int argc, char **argv) {
  char topdir[HTS_URLMAXSIZE];
  char path[HTS_URLMAXSIZE + 32];
  char buf[16384]; /* the listing sits after the whole header template */
  FILE *fp;
  size_t n;
#ifdef _WIN32
  /* the GUI hands hts_buildtopindex ANSI paths; mimic it. CP1252 'cafe' */
  static const char *const projName = "caf\xE9";
#else
  /* POSIX system charset is already utf-8 */
  static const char *const projName = "caf\xC3\xA9";
#endif
  /* utf-8 form the index must carry whatever the input charset was */
  static const char *const projUTF8 = "caf\xC3\xA9";

  assertf(argc >= 1);
  /* a non-ASCII top dir (#217) holding a non-ASCII sub-project (#216) */
  snprintf(topdir, sizeof(topdir), "%s/%s", argv[0], projName);
  snprintf(path, sizeof(path), "%s/%s/", topdir, projName);
  /* structcheck(), not the utf-8 MKDIR family: same charset as buildtopindex */
  assertf(structcheck(path) == 0);
  /* the sub-project is listed only if it holds an index.html */
  snprintf(path, sizeof(path), "%s/%s/index.html", topdir, projName);
  fp = fopen(path, "wb");
  assertf(fp != NULL);
  fclose(fp);

  assertf(hts_buildtopindex(opt, topdir, "") != 0);

  /* #217: gifs land in the top dir itself, not in a mangled sibling */
  snprintf(path, sizeof(path), "%s/backblue.gif", topdir);
  assertf(fexist(path));

  /* #216: the listed name is utf-8, not raw system-charset mojibake */
  snprintf(path, sizeof(path), "%s/index.html", topdir);
  fp = fopen(path, "rb");
  assertf(fp != NULL);
  n = fread(buf, 1, sizeof(buf) - 1, fp);
  fclose(fp);
  buf[n] = '\0';
  assertf(strstr(buf, projUTF8) != NULL);

  /* raw unlink/rmdir: UNLINK is utf-8 on Windows, these paths aren't */
  unlink(path);
  snprintf(path, sizeof(path), "%s/backblue.gif", topdir);
  unlink(path);
  snprintf(path, sizeof(path), "%s/fade.gif", topdir);
  unlink(path);
  snprintf(path, sizeof(path), "%s/%s/index.html", topdir, projName);
  unlink(path);
  snprintf(path, sizeof(path), "%s/%s", topdir, projName);
  rmdir(path);
  rmdir(topdir);
  printf("topindex self-test OK\n");
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
#endif

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

/* Accept-Encoding (#450): advertise gzip+deflate; both decode (hts_zunpack) */
static int st_acceptencoding(httrackp *opt, int argc, char **argv) {
  const char *off = hts_acceptencoding(HTS_FALSE, HTS_TRUE);
  const char *on = hts_acceptencoding(HTS_TRUE, HTS_FALSE);
  const char *tls = hts_acceptencoding(HTS_TRUE, HTS_TRUE);

  (void) opt;
  assertf(strcmp(off, "identity") == 0);
  assertf(strstr(on, "gzip") != NULL);
  assertf(strstr(on, "deflate") != NULL); /* fails on the old gzip-only list */
  /* br and zstd ride on TLS only, so a cleartext proxy can not be handed a
     coding it may try to rewrite */
  assertf(strstr(on, "br") == NULL && strstr(on, "zstd") == NULL);
  assertf((strstr(tls, ", br") != NULL) == (HTS_USEBROTLI != 0));
  assertf((strstr(tls, "zstd") != NULL) == (HTS_USEZSTD != 0));
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

#if HTS_USEBROTLI
/* No brotli encoder is linked, so the coded bytes are canned: brotli quality 9
   over cc_text, and over 4 MiB of 'A' (14 bytes, ~300000x). */
static const unsigned char cc_br_text[] = {
    0x1b, 0x43, 0x00, 0x00, 0x44, 0xdd, 0x96, 0xea, 0xe8, 0x22, 0xdd, 0x90,
    0xa4, 0x1b, 0x8a, 0xf7, 0x47, 0x0e, 0xc2, 0xc5, 0x3d, 0x09, 0x1b, 0x70,
    0xe0, 0x1e, 0x60, 0xa0, 0x8b, 0xcc, 0xbe, 0xcb, 0xb0, 0x31, 0x76, 0x9e,
    0xcf, 0x6e, 0x41, 0xb5, 0xe8, 0x2e, 0x56, 0x78, 0x08, 0x1b, 0xfa, 0x08,
    0x8a, 0x50, 0x83, 0x4e, 0x62, 0x7f, 0xbf, 0x05, 0xf2, 0x22, 0x8f, 0xdf,
    0x28, 0xdc, 0x9f, 0xa9, 0x90, 0x50, 0x37, 0x62, 0x56, 0x4f, 0xa8};
static const unsigned char cc_br_bomb[] = {0x9b, 0xff, 0xff, 0x3f, 0x00,
                                           0x24, 0x82, 0xe2, 0xb1, 0x40,
                                           0x72, 0xef, 0x7f, 0x00};
#endif

static const unsigned char cc_text[] =
    "content codings: HTTrack decodes gzip, brotli and zstd bodies alike.";

/* Content codings: br and zstd decode, junk tokens stay identity, a coding we
   can not undo fails the fetch, and a bomb never lands on disk. */
static int st_contentcodings(httrackp *opt, int argc, char **argv) {
  const size_t tlen = sizeof(cc_text) - 1;
  char inpath[HTS_URLMAXSIZE], outpath[HTS_URLMAXSIZE];

  (void) opt;
  assertf(hts_codec_parse("gzip") == HTS_CODEC_DEFLATE);
  assertf(hts_codec_parse("x-deflate") == HTS_CODEC_DEFLATE);
  assertf(hts_codec_parse("") == HTS_CODEC_IDENTITY);
  assertf(hts_codec_parse("identity") == HTS_CODEC_IDENTITY);
  /* servers do label plain bodies with junk; the page must survive that */
  assertf(hts_codec_parse("utf-8") == HTS_CODEC_IDENTITY);
  /* a real coding with no decoder here: fail, never save the coded bytes */
  assertf(hts_codec_parse("compress") == HTS_CODEC_UNSUPPORTED);
  assertf(hts_codec_parse("br") ==
          (HTS_USEBROTLI ? HTS_CODEC_BROTLI : HTS_CODEC_UNSUPPORTED));
  assertf(hts_codec_parse("zstd") ==
          (HTS_USEZSTD ? HTS_CODEC_ZSTD : HTS_CODEC_UNSUPPORTED));
  assertf(hts_codec_is_archive_ext(HTS_CODEC_DEFLATE, "tgz"));
  assertf(!hts_codec_is_archive_ext(HTS_CODEC_DEFLATE, "html"));
  assertf(hts_codec_is_archive_ext(HTS_CODEC_BROTLI, "br"));
  assertf(hts_codec_is_archive_ext(HTS_CODEC_ZSTD, "zst"));
  /* decoded-size budget: 4096x, floor 1 MiB, ceiling INT_MAX */
  assertf(hts_codec_maxout(1) == 1024 * 1024);
  assertf(hts_codec_maxout(1024) == 4096 * 1024);
  assertf(hts_codec_maxout(1024 * 1024) == INT_MAX);

  if (argc >= 1) {
    snprintf(inpath, sizeof(inpath), "%s/cc-in", argv[0]);
    snprintf(outpath, sizeof(outpath), "%s/cc-out", argv[0]);
#if HTS_USEBROTLI
    {
      unsigned char head[16];

      assertf(ae_write_raw(inpath, cc_br_text, sizeof(cc_br_text)) == 0);
      assertf(hts_codec_unpack(HTS_CODEC_BROTLI, inpath, outpath) ==
              (int) tlen);
      assertf(ae_check_decoded(outpath, cc_text, tlen) == 0);
      assertf(hts_codec_head(HTS_CODEC_BROTLI, cc_br_text, sizeof(cc_br_text),
                             head, sizeof(head)) == sizeof(head));
      assertf(memcmp(head, cc_text, sizeof(head)) == 0);
      /* truncated: must fail, not fall back to a verbatim copy */
      assertf(ae_write_raw(inpath, cc_br_text, sizeof(cc_br_text) - 4) == 0);
      assertf(hts_codec_unpack(HTS_CODEC_BROTLI, inpath, outpath) < 0);
      /* cc_br_bomb is a valid stream that expands to 4 MiB; the budget, not a
         corrupt frame, is what must reject it. */
      assertf((LLint) (4 * 1024 * 1024) >
              hts_codec_maxout((LLint) sizeof(cc_br_bomb)));
      assertf(ae_write_raw(inpath, cc_br_bomb, sizeof(cc_br_bomb)) == 0);
      assertf(hts_codec_unpack(HTS_CODEC_BROTLI, inpath, outpath) < 0);
    }
#endif
#if HTS_USEZSTD
    {
      const size_t bomblen = 4 * 1024 * 1024;
      const size_t bound = ZSTD_compressBound(bomblen);
      unsigned char *bomb = malloct(bomblen);
      unsigned char *zbuf = malloct(bound);
      unsigned char head[16];
      size_t zlen;

      assertf(bomb != NULL && zbuf != NULL);
      zlen = ZSTD_compress(zbuf, bound, cc_text, tlen, 6);
      assertf(!ZSTD_isError(zlen));
      assertf(ae_write_raw(inpath, zbuf, zlen) == 0);
      assertf(hts_codec_unpack(HTS_CODEC_ZSTD, inpath, outpath) == (int) tlen);
      assertf(ae_check_decoded(outpath, cc_text, tlen) == 0);
      assertf(hts_codec_head(HTS_CODEC_ZSTD, zbuf, zlen, head, sizeof(head)) ==
              sizeof(head));
      assertf(memcmp(head, cc_text, sizeof(head)) == 0);
      assertf(ae_write_raw(inpath, zbuf, zlen - 4) == 0);
      assertf(hts_codec_unpack(HTS_CODEC_ZSTD, inpath, outpath) < 0);
      memset(bomb, 'A', bomblen);
      zlen = ZSTD_compress(zbuf, bound, bomb, bomblen, 6);
      assertf(!ZSTD_isError(zlen));
      /* the fixture must really be past the budget, whatever the ratio is */
      assertf((LLint) bomblen > hts_codec_maxout((LLint) zlen));
      assertf(ae_write_raw(inpath, zbuf, zlen) == 0);
      assertf(hts_codec_unpack(HTS_CODEC_ZSTD, inpath, outpath) < 0);
      freet(bomb);
      freet(zbuf);
    }
#endif
    /* a coding we can not undo yields no file at all */
    assertf(hts_codec_unpack(HTS_CODEC_UNSUPPORTED, inpath, outpath) < 0);
  }
  printf("contentcodings self-test OK\n");
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

/* Connected stream pair over loopback; Windows has no socketpair(). */
static int st_socketpair(T_SOC sv[2]) {
  struct sockaddr_in sa;
  socklen_t len = sizeof(sa);
  T_SOC srv, cli, acc;

  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if ((srv = (T_SOC) socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
    return -1;
  if (bind(srv, (struct sockaddr *) &sa, sizeof(sa)) != 0 ||
      listen(srv, 1) != 0 ||
      getsockname(srv, (struct sockaddr *) &sa, &len) != 0) {
    deletesoc(srv);
    return -1;
  }
  if ((cli = (T_SOC) socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
    deletesoc(srv);
    return -1;
  }
  if (connect(cli, (struct sockaddr *) &sa, sizeof(sa)) != 0 ||
      (acc = (T_SOC) accept(srv, NULL, NULL)) == INVALID_SOCKET) {
    deletesoc(cli);
    deletesoc(srv);
    return -1;
  }
  deletesoc(srv);
  sv[0] = acc;
  sv[1] = cli;
  return 0;
}

/* get_ftp_line must bound a hostile, CRLF-less reply into its internal
   1024-byte buffer; ASan turns the pre-fix overflow into an abort here. */
static int st_ftpline(httrackp *opt, int argc, char **argv) {
  T_SOC sv[2];
  char line[2048];
  char flood[4096];

  (void) opt;
  (void) argc;
  (void) argv;
  memset(flood, 'x', sizeof(flood));
  assertf(st_socketpair(sv) == 0);
  // the 4102-byte reply fits the loopback send buffer, so no reader is needed
  assertf(send(sv[1], "220 ", 4, 0) == 4); // valid 3-digit code
  assertf(send(sv[1], flood, (int) sizeof(flood), 0) == (int) sizeof(flood));
  assertf(send(sv[1], "\r\n", 2, 0) == 2); // end the line so we return
  deletesoc(sv[1]);
  line[0] = '\0';
  get_ftp_line(sv[0], line, sizeof(line), 5);
  deletesoc(sv[0]);
  printf("ftp-line self-test OK (bounded %d-byte reply)\n",
         (int) sizeof(flood));
  return 0;
}

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

/* Bounded substring search (records carry NUL bytes; strstr won't do). */
static const char *warc_memstr(const char *hay, const char *needle,
                               size_t haylen, size_t nlen) {
  if (nlen == 0 || haylen < nlen)
    return NULL;
  {
    size_t i;
    for (i = 0; i + nlen <= haylen; i++) {
      if (memcmp(hay + i, needle, nlen) == 0)
        return hay + i;
    }
  }
  return NULL;
}

/* Slurp a whole file into a malloc'd buffer; sets *len. NULL on error. */
static unsigned char *warc_slurp(const char *path, size_t *len) {
  FILE *f = FOPEN(path, "rb");
  unsigned char *buf;
  long sz;
  if (f == NULL)
    return NULL;
  if (fseek(f, 0, SEEK_END) != 0 || (sz = ftell(f)) < 0) {
    fclose(f);
    return NULL;
  }
  rewind(f);
  buf = malloct((size_t) sz + 1);
  if (buf == NULL) {
    fclose(f);
    return NULL;
  }
  *len = fread(buf, 1, (size_t) sz, f);
  fclose(f);
  return buf;
}

/* Inflate one gzip member at *in (limit end); returns the decompressed record
   in a malloc'd buffer (*out_len), advancing *in past the member. NULL at end
   or on error (*out_len distinguishes: 0 and NULL = clean end). */
static unsigned char *warc_next_member(const unsigned char **in,
                                       const unsigned char *end,
                                       size_t *out_len) {
  z_stream zs;
  unsigned char *out = NULL;
  size_t len = 0;
  int zerr;
  *out_len = 0;
  if (*in >= end)
    return NULL;
  memset(&zs, 0, sizeof(zs));
  if (inflateInit2(&zs, 15 + 32) != Z_OK)
    return NULL;
  zs.next_in = (const Bytef *) *in;
  zs.avail_in = (uInt) (end - *in);
  do {
    unsigned char tmp[8192];
    size_t got;
    zs.next_out = tmp;
    zs.avail_out = sizeof(tmp);
    zerr = inflate(&zs, Z_NO_FLUSH);
    if (zerr != Z_OK && zerr != Z_STREAM_END) {
      freet(out);
      inflateEnd(&zs);
      return NULL;
    }
    got = sizeof(tmp) - zs.avail_out;
    if (got > 0) {
      unsigned char *n = realloct(out, len + got + 1);
      if (n == NULL) {
        freet(out);
        inflateEnd(&zs);
        return NULL;
      }
      out = n;
      memcpy(out + len, tmp, got);
      len += got;
    }
  } while (zerr != Z_STREAM_END);
  *in = (const unsigned char *) zs.next_in; /* start of the next member */
  inflateEnd(&zs);
  if (out != NULL)
    out[len] = '\0';
  *out_len = len;
  return out;
}

/* Feed a synthetic transaction and validate the resulting .warc.gz against the
   WARC/1.1 spec: each record a self-standing gzip member starting WARC/1.,
   Content-Length == block length, the \r\n\r\n trailer intact, the response
   body round-trips, and the hop-by-hop Transfer-Encoding is dropped (a real
   Content-Encoding is kept verbatim; see warc-verbatim). */
static int st_warc(httrackp *opt, int argc, char **argv) {
  char path[HTS_URLMAXSIZE];
  warc_writer *w;
  unsigned char *data;
  size_t data_len = 0;
  const unsigned char *p, *end;
  int err = 0, nrec = 0, nresp = 0, nreq = 0, nrevisit = 0, ninfo = 0;
  int seen_a_body = 0, body_occurrences = 0, a2_bodyless = 0, nm_cl_ok = 0;
  static const char a_body[] = "Hello, WARC!\n";

  if (argc < 1) {
    fprintf(stderr, "warc: needs a writable directory\n");
    return 1;
  }
  fconcat(path, sizeof(path), argv[0], "warc-selftest.warc.gz");

  w = warc_open(opt, path);
  assertf(w != NULL);

  /* 200 HTML, plaintext body: bogus Content-Length rewritten, hop-by-hop
     Transfer-Encoding dropped. The whitespace before its ':' exercises
     header_is tolerating "Name : value". */
  warc_write_transaction(
      w, "http://test.local/a.html", "127.0.0.1",
      "GET /a.html HTTP/1.1\r\nHost: test.local\r\n\r\n",
      "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n"
      "Transfer-Encoding : chunked\r\nContent-Length: 999\r\n\r\n",
      a_body, sizeof(a_body) - 1, NULL, 200, 0, 0);

  /* 302 redirect: header-only, no body. */
  warc_write_transaction(
      w, "http://test.local/r", "127.0.0.1",
      "GET /r HTTP/1.1\r\nHost: test.local\r\n\r\n",
      "HTTP/1.1 302 Found\r\nLocation: http://test.local/a.html\r\n\r\n", NULL,
      0, NULL, 302, 0, 0);

  /* 200 binary, chunked coding on the wire (already de-chunked here). */
  warc_write_transaction(
      w, "http://test.local/b.bin", "127.0.0.1",
      "GET /b.bin HTTP/1.1\r\nHost: test.local\r\n\r\n",
      "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\n"
      "Transfer-Encoding: chunked\r\n\r\n",
      "\x00\x01\x02\x03\x04", 5, NULL, 200, 0, 0);

  /* 200 with a body shorter than the declared Content-Length (rewritten). */
  warc_write_transaction(
      w, "http://test.local/trunc", "127.0.0.1",
      "GET /trunc HTTP/1.1\r\nHost: test.local\r\n\r\n",
      "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: "
      "100\r\n\r\n",
      "short", 5, NULL, 200, 0, 0);

  /* Same payload as a.html at a new URL: identical-payload-digest revisit
     (OpenSSL builds only; a plain build writes a second full response). */
  warc_write_transaction(w, "http://test.local/a2.html", "127.0.0.1",
                         "GET /a2.html HTTP/1.1\r\nHost: test.local\r\n\r\n",
                         "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n",
                         a_body, sizeof(a_body) - 1, NULL, 200, 0, 0);

  /* 304 revisit with an EMPTY response-header block: the block is just the
     2-byte separator, so declared Content-Length must be exactly 2 (F3). */
  warc_write_transaction(w, "http://test.local/nm", "127.0.0.1",
                         "GET /nm HTTP/1.1\r\nHost: test.local\r\n\r\n", "",
                         NULL, 0, NULL, 304, 1, 0);

  warc_close(w);

  data = warc_slurp(path, &data_len);
  assertf(data != NULL);
  p = data;
  end = data + data_len;

  while (p < end) {
    size_t rlen = 0;
    unsigned char *rec = warc_next_member(&p, end, &rlen);
    const char *sep, *cl;
    long long block_len = 0; /* 0 on a parse failure; err is already set */
    size_t hdr_len;
    if (rec == NULL) {
      if (rlen == 0)
        break; /* clean end */
      err = 1;
      break;
    }
    nrec++;
    /* magic */
    if (rlen < 8 || memcmp(rec, "WARC/1.", 7) != 0)
      err = 1;
    /* record header ends at the first blank line */
    sep = warc_memstr((char *) rec, "\r\n\r\n", rlen, 4);
    if (sep == NULL) {
      err = 1;
      freet(rec);
      continue;
    }
    hdr_len = (size_t) ((const unsigned char *) sep - rec) + 4;
    /* Content-Length must equal the actual block length */
    cl = warc_memstr((char *) rec, "Content-Length:", hdr_len, 15);
    if (cl == NULL || sscanf(cl + 15, "%lld", &block_len) != 1)
      err = 1;
    else {
      if (hdr_len + (size_t) block_len + 4 != rlen)
        err = 1; /* header + block + trailing CRLFCRLF */
      else if (memcmp(rec + hdr_len + block_len, "\r\n\r\n", 4) != 0)
        err = 1; /* trailer intact */
    }
    if (warc_memstr((char *) rec, "WARC-Type: warcinfo", hdr_len, 19) != NULL)
      ninfo++;
    if (warc_memstr((char *) rec, "WARC-Type: request", hdr_len, 18) != NULL)
      nreq++;
    if (warc_memstr((char *) rec, "WARC-Type: response", hdr_len, 19) != NULL)
      nresp++;
    if (warc_memstr((char *) rec, "WARC-Type: revisit", hdr_len, 18) != NULL)
      nrevisit++;
    /* F1: the full body must appear exactly once across the whole file (a
       revisit must not re-embed it). */
    if (warc_memstr((char *) rec, a_body, rlen, sizeof(a_body) - 1) != NULL)
      body_occurrences++;
    /* F1: the a2.html identical-payload-digest revisit carries no body. */
    if (warc_memstr((char *) rec, "WARC-Target-URI: http://test.local/a2.html",
                    hdr_len, 42) != NULL &&
        warc_memstr((char *) rec, "WARC-Type: revisit", hdr_len, 18) != NULL)
      a2_bodyless =
          (warc_memstr((char *) rec, a_body, rlen, sizeof(a_body) - 1) == NULL);
    /* F3: the empty-header 304 revisit block is exactly the 2-byte separator
       (the request record shares this target URI, so match the revisit only).
     */
    if (warc_memstr((char *) rec, "WARC-Target-URI: http://test.local/nm",
                    hdr_len, 37) != NULL &&
        warc_memstr((char *) rec, "WARC-Type: revisit", hdr_len, 18) != NULL)
      nm_cl_ok = (block_len == 2);
    /* a.html response body round-trips; no Content-Encoding (plaintext) and the
       whitespaced Transfer-Encoding was dropped (header_is robustness). */
    if (warc_memstr((char *) rec, "WARC-Target-URI: http://test.local/a.html",
                    hdr_len, 41) != NULL &&
        warc_memstr((char *) rec, "msgtype=response", hdr_len, 16) != NULL) {
      const char *bsep = warc_memstr((char *) rec + hdr_len, "\r\n\r\n",
                                     (size_t) block_len, 4);
      if (bsep == NULL)
        err = 1;
      else {
        size_t bodyoff = (size_t) (bsep - (char *) rec) + 4;
        size_t got = rlen - 4 - bodyoff; /* minus record trailer */
        if (got != sizeof(a_body) - 1 ||
            memcmp(rec + bodyoff, a_body, got) != 0)
          err = 1;
        seen_a_body = 1;
      }
      if (warc_memstr((char *) rec, "Content-Encoding", hdr_len + block_len,
                      16) != NULL ||
          warc_memstr((char *) rec, "Transfer-Encoding", hdr_len + block_len,
                      17) != NULL)
        err = 1;
    }
    freet(rec);
  }
  freet(data);

  /* warcinfo + 6 transactions (response/revisit + request each) = 13 records.
   */
  if (ninfo != 1 || nreq != 6 || nrec != 13 || !seen_a_body || !nm_cl_ok)
    err = 1;
#if HTS_USEOPENSSL
  /* a.html + b.bin + trunc + 302 are full responses; a2.html deduped to a
     revisit (bodyless), nm is the 304 revisit; the body appears exactly once.
   */
  if (nrevisit != 2 || nresp != 4 || !a2_bodyless || body_occurrences != 1)
    err = 1;
#else
  /* No digests: a2.html is a second full response, so the body appears twice
     and only the 304 nm is a revisit. */
  if (nrevisit != 1 || nresp != 5 || body_occurrences != 2)
    err = 1;
  (void) a2_bodyless; /* only meaningful with digests */
#endif

  printf("warc: %d records (%d response, %d request, %d revisit): %s\n", nrec,
         nresp, nreq, nrevisit, err ? "FAIL" : "OK");
  return err;
}

/* Parse a record's header/block split; sets *hdr_len and *block_len, returns 0
   when Content-Length matches the actual block bytes, -1 otherwise. */
static int warc_rec_split(const unsigned char *rec, size_t rlen,
                          size_t *hdr_len, long long *block_len) {
  const char *sep = warc_memstr((const char *) rec, "\r\n\r\n", rlen, 4);
  const char *cl;
  *block_len = 0;
  if (sep == NULL)
    return -1;
  *hdr_len = (size_t) ((const unsigned char *) sep - rec) + 4;
  cl = warc_memstr((const char *) rec, "Content-Length:", *hdr_len, 15);
  if (cl == NULL || sscanf(cl + 15, "%lld", block_len) != 1 ||
      *hdr_len + (size_t) *block_len + 4 != rlen)
    return -1;
  return 0;
}

/* A cap-truncated body is still archived, tagged WARC-Truncated (v1.1). A
   compressed body cut short by a cap keeps its Content-Encoding (the stored
   bytes are the coded partial), so the record's label matches its body: assert
   the plaintext response carries "WARC-Truncated: length", and the gzip-coded
   one carries "WARC-Truncated: time", keeps Content-Encoding, and stores the
   coded bytes verbatim. */
static int st_warc_trunc(httrackp *opt, int argc, char **argv) {
  char path[HTS_URLMAXSIZE];
  warc_writer *w;
  unsigned char *data;
  size_t data_len = 0;
  const unsigned char *p, *end;
  int err = 0, trunc_len = 0, trunc_gz = 0, nresp = 0;
  static const char body[] = "partial body bytes\n";
  /* a valid gzip member (inflates to a known plaintext), as the coded partial
   */
  static const unsigned char gz[] = {
      0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xff, 0x0b, 0x2e,
      0x29, 0x4a, 0x2c, 0x49, 0x4d, 0xaf, 0xd4, 0x75, 0x54, 0x28, 0x4b, 0x2d,
      0x4a, 0x4a, 0x2c, 0xc9, 0xcc, 0x55, 0x08, 0x77, 0x0c, 0x72, 0x56, 0x48,
      0xca, 0x4f, 0xa9, 0xb4, 0x52, 0x28, 0xc9, 0x48, 0x55, 0x28, 0x2c, 0xcd,
      0x4c, 0xce, 0x56, 0x48, 0x2a, 0xca, 0x2f, 0xcf, 0x53, 0x48, 0xcb, 0xaf,
      0x50, 0xc8, 0x2a, 0xcd, 0x2d, 0x28, 0xd6, 0xe3, 0x02, 0x00, 0x5e, 0xb8,
      0xe7, 0x66, 0x3a, 0x00, 0x00, 0x00};

  if (argc < 1) {
    fprintf(stderr, "warc-trunc: needs a writable directory\n");
    return 1;
  }
  fconcat(path, sizeof(path), argv[0], "warc-trunc.warc.gz");
  w = warc_open(opt, path);
  assertf(w != NULL);
  warc_write_transaction(
      w, "http://test.local/big.bin", "127.0.0.1",
      "GET /big.bin HTTP/1.1\r\nHost: test.local\r\n\r\n",
      "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\n\r\n", body,
      sizeof(body) - 1, NULL, 200, 0, WARC_TRUNC_LENGTH);
  warc_write_transaction(
      w, "http://test.local/big.gz", "127.0.0.1",
      "GET /big.gz HTTP/1.1\r\nHost: test.local\r\n\r\n",
      "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Encoding: "
      "gzip\r\n\r\n",
      (const char *) gz, sizeof(gz), NULL, 200, 0, WARC_TRUNC_TIME);
  warc_close(w);

  data = warc_slurp(path, &data_len);
  assertf(data != NULL);
  p = data;
  end = data + data_len;
  while (p < end) {
    size_t rlen = 0, hdr_len = 0;
    long long block_len = 0;
    unsigned char *rec = warc_next_member(&p, end, &rlen);
    if (rec == NULL) {
      if (rlen != 0)
        err = 1;
      break;
    }
    if (warc_rec_split(rec, rlen, &hdr_len, &block_len) != 0) {
      err = 1;
      freet(rec);
      continue;
    }
    if (warc_memstr((char *) rec, "WARC-Type: response", hdr_len, 19) != NULL) {
      nresp++;
      if (warc_memstr((char *) rec,
                      "WARC-Target-URI: http://test.local/big.bin", hdr_len,
                      42) != NULL &&
          warc_memstr((char *) rec, "WARC-Truncated: length", hdr_len, 22) !=
              NULL)
        trunc_len = 1;
      if (warc_memstr((char *) rec, "WARC-Target-URI: http://test.local/big.gz",
                      hdr_len, 41) != NULL) {
        const char *bsep = warc_memstr((char *) rec + hdr_len, "\r\n\r\n",
                                       (size_t) block_len, 4);
        size_t bodyoff = bsep ? (size_t) (bsep - (char *) rec) + 4 : 0;
        size_t got = bsep ? rlen - 4 - bodyoff : 0;
        /* WARC-Truncated: time, Content-Encoding kept, stored body == coded. */
        if (bsep != NULL &&
            warc_memstr((char *) rec, "WARC-Truncated: time", hdr_len, 20) !=
                NULL &&
            warc_memstr((char *) rec + hdr_len,
                        "Content-Encoding:", (size_t) block_len, 17) != NULL &&
            got == sizeof(gz) && memcmp(rec + bodyoff, gz, sizeof(gz)) == 0)
          trunc_gz = 1;
      }
    }
    freet(rec);
  }
  freet(data);
  if (!trunc_len || !trunc_gz || nresp != 2)
    err = 1;
  printf("warc-trunc: %s\n", err ? "FAIL" : "OK");
  return err;
}

/* An ftp:// capture is ONE resource record: WARC-Type: resource, the payload's
   own Content-Type, block == payload, and no request/response pair. */
static int st_warc_ftp(httrackp *opt, int argc, char **argv) {
  char path[HTS_URLMAXSIZE];
  warc_writer *w;
  unsigned char *data;
  size_t data_len = 0;
  const unsigned char *p, *end;
  int err = 0, nresource = 0, nresp = 0, nreq = 0;
  static const char body[] = "\x00\x01"
                             "FTP payload"
                             "\x02\x03";

  if (argc < 1) {
    fprintf(stderr, "warc-ftp: needs a writable directory\n");
    return 1;
  }
  fconcat(path, sizeof(path), argv[0], "warc-ftp.warc.gz");
  w = warc_open(opt, path);
  assertf(w != NULL);
  warc_write_resource(w, "ftp://ftp.local/file.bin", "127.0.0.1",
                      "application/octet-stream", body, sizeof(body) - 1, NULL,
                      0);
  warc_close(w);

  data = warc_slurp(path, &data_len);
  assertf(data != NULL);
  p = data;
  end = data + data_len;
  while (p < end) {
    size_t rlen = 0, hdr_len = 0;
    long long block_len = 0;
    unsigned char *rec = warc_next_member(&p, end, &rlen);
    if (rec == NULL) {
      if (rlen != 0)
        err = 1;
      break;
    }
    if (warc_rec_split(rec, rlen, &hdr_len, &block_len) != 0)
      err = 1;
    if (warc_memstr((char *) rec, "WARC-Type: resource", hdr_len, 19) != NULL) {
      nresource++;
      if ((size_t) block_len != sizeof(body) - 1 ||
          memcmp(rec + hdr_len, body, sizeof(body) - 1) != 0)
        err = 1; /* block is the raw payload, no HTTP envelope */
      if (warc_memstr((char *) rec, "WARC-Target-URI: ftp://ftp.local/file.bin",
                      hdr_len, 41) == NULL ||
          warc_memstr((char *) rec, "Content-Type: application/octet-stream",
                      hdr_len, 38) == NULL)
        err = 1;
    }
    if (warc_memstr((char *) rec, "WARC-Type: response", hdr_len, 19) != NULL)
      nresp++;
    if (warc_memstr((char *) rec, "WARC-Type: request", hdr_len, 18) != NULL)
      nreq++;
    freet(rec);
  }
  freet(data);
  if (nresource != 1 || nresp != 0 || nreq != 0)
    err = 1;
  printf("warc-ftp: resource=%d response=%d request=%d: %s\n", nresource, nresp,
         nreq, err ? "FAIL" : "OK");
  return err;
}

/* --warc-max-size rotates into <base>-00000.warc.gz, -00001, ...; each segment
   is independently valid and begins with its own warcinfo. */
static int st_warc_rotate(httrackp *opt, int argc, char **argv) {
  char path[HTS_URLMAXSIZE];
  char seg[HTS_URLMAXSIZE];
  warc_writer *w;
  LLint saved_max;
  unsigned char body[600];
  unsigned int rng = 0x12345678u;
  int err = 0, nseg = 0, i;
  size_t j;

  if (argc < 1) {
    fprintf(stderr, "warc-rotate: needs a writable directory\n");
    return 1;
  }
  for (j = 0; j < sizeof(body);
       j++) { /* incompressible: gzip can't shrink it */
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    body[j] = (unsigned char) (rng >> 24);
  }
  fconcat(path, sizeof(path), argv[0], "warc-rot.warc.gz");
  saved_max = opt->warc_max_size;
  opt->warc_max_size =
      1000; /* a couple records per segment => several segments */
  w = warc_open(opt, path);
  assertf(w != NULL);
  for (i = 0; i < 8; i++) {
    char uri[64];
    snprintf(uri, sizeof(uri), "http://test.local/f%d.bin", i);
    warc_write_transaction(
        w, uri, "127.0.0.1", "GET / HTTP/1.1\r\nHost: test.local\r\n\r\n",
        "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\n\r\n",
        (const char *) body, sizeof(body), NULL, 200, 0, 0);
  }
  warc_close(w);
  opt->warc_max_size = saved_max;

  for (i = 0;; i++) {
    char fname[64];
    unsigned char *data;
    size_t data_len = 0;
    const unsigned char *p, *pend;
    int first = 1;
    snprintf(fname, sizeof(fname), "warc-rot-%05d.warc.gz", i);
    fconcat(seg, sizeof(seg), argv[0], fname);
    data = warc_slurp(seg, &data_len);
    if (data == NULL)
      break; /* past the last segment */
    nseg++;
    p = data;
    pend = data + data_len;
    while (p < pend) {
      size_t rlen = 0, hdr_len = 0;
      long long block_len = 0;
      unsigned char *rec = warc_next_member(&p, pend, &rlen);
      if (rec == NULL) {
        if (rlen != 0)
          err = 1;
        break;
      }
      if (warc_rec_split(rec, rlen, &hdr_len, &block_len) != 0)
        err = 1;
      if (first) { /* each segment leads with its own warcinfo */
        if (warc_memstr((char *) rec, "WARC-Type: warcinfo", hdr_len, 19) ==
            NULL)
          err = 1;
        first = 0;
      }
      freet(rec);
    }
    freet(data);
    if (first) /* empty segment */
      err = 1;
  }
  if (nseg < 2)
    err = 1;
  printf("warc-rotate: %d segments: %s\n", nseg, err ? "FAIL" : "OK");
  return err;
}

/* The default body storage: assert the stored WARC record is byte-verbatim gzip
   with Content-Encoding preserved and Content-Length = the coded length. */
static int st_warc_verbatim(httrackp *opt, int argc, char **argv) {
  char path[HTS_URLMAXSIZE];
  warc_writer *w;
  unsigned char *data;
  size_t data_len = 0;
  const unsigned char *p, *end;
  int err = 0, checked = 0;
  static const char a_plain[] =
      "Strategy-A verbatim WARC body: the quick brown fox jumps.\n";
  static const unsigned char a_gz[] = {
      0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xff, 0x0b, 0x2e,
      0x29, 0x4a, 0x2c, 0x49, 0x4d, 0xaf, 0xd4, 0x75, 0x54, 0x28, 0x4b, 0x2d,
      0x4a, 0x4a, 0x2c, 0xc9, 0xcc, 0x55, 0x08, 0x77, 0x0c, 0x72, 0x56, 0x48,
      0xca, 0x4f, 0xa9, 0xb4, 0x52, 0x28, 0xc9, 0x48, 0x55, 0x28, 0x2c, 0xcd,
      0x4c, 0xce, 0x56, 0x48, 0x2a, 0xca, 0x2f, 0xcf, 0x53, 0x48, 0xcb, 0xaf,
      0x50, 0xc8, 0x2a, 0xcd, 0x2d, 0x28, 0xd6, 0xe3, 0x02, 0x00, 0x5e, 0xb8,
      0xe7, 0x66, 0x3a, 0x00, 0x00, 0x00};

  if (argc < 1) {
    fprintf(stderr, "warc-verbatim: needs a writable directory\n");
    return 1;
  }
  fconcat(path, sizeof(path), argv[0], "warc-verbatim.warc.gz");

  w = warc_open(opt, path);
  assertf(w != NULL);
  /* the body is the coded (gzip) octets, stored verbatim. */
  warc_write_transaction(
      w, "http://test.local/z.html", "127.0.0.1",
      "GET /z.html HTTP/1.1\r\nHost: test.local\r\n\r\n",
      "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Encoding: "
      "gzip\r\nTransfer-Encoding: chunked\r\nContent-Length: 999\r\n\r\n",
      (const char *) a_gz, sizeof(a_gz), NULL, 200, 0, 0);
  warc_close(w);

  data = warc_slurp(path, &data_len);
  assertf(data != NULL);
  p = data;
  end = data + data_len;

  while (p < end) {
    size_t rlen = 0, hdr_len = 0;
    long long block_len = 0;
    unsigned char *rec = warc_next_member(&p, end, &rlen);
    if (rec == NULL) {
      if (rlen != 0)
        err = 1;
      break;
    }
    if (warc_memstr((char *) rec, "msgtype=response", rlen, 16) == NULL) {
      freet(rec);
      continue;
    }
    if (warc_rec_split(rec, rlen, &hdr_len, &block_len) != 0) {
      err = 1;
      freet(rec);
      continue;
    }
    /* Assert: one Content-Encoding, no Transfer-Encoding, Content-Length =
       compressed size. */
    {
      const char *block = (char *) rec + hdr_len;
      const char *ce =
          warc_memstr(block, "Content-Encoding:", (size_t) block_len, 17);
      const char *hcl =
          warc_memstr(block, "Content-Length:", (size_t) block_len, 15);
      long long http_cl = -1;
      int nce = 0;
      const char *scan = ce;
      while (scan != NULL) {
        size_t rem = (size_t) block_len - (size_t) (scan - block);
        nce++;
        scan = warc_memstr(scan + 17, "Content-Encoding:", rem - 17, 17);
      }
      if (ce == NULL || strncasecmp(ce + 17, " gzip", 5) != 0 || nce != 1)
        err = 1;
      if (warc_memstr(block, "Transfer-Encoding:", (size_t) block_len, 18) !=
          NULL)
        err = 1;
      if (hcl == NULL || sscanf(hcl + 15, "%lld", &http_cl) != 1 ||
          http_cl != (long long) sizeof(a_gz))
        err = 1;
    }
    /* Stored block bytes equal the gzip input, and inflate to the plaintext. */
    {
      const char *bsep = warc_memstr((char *) rec + hdr_len, "\r\n\r\n",
                                     (size_t) block_len, 4);
      if (bsep == NULL)
        err = 1;
      else {
        size_t bodyoff = (size_t) (bsep - (char *) rec) + 4;
        size_t got = rlen - 4 - bodyoff; /* minus the record trailer */
        if (got != sizeof(a_gz) ||
            memcmp(rec + bodyoff, a_gz, sizeof(a_gz)) != 0)
          err = 1;
        else {
          const unsigned char *bp = rec + bodyoff;
          size_t plen = 0;
          unsigned char *plain = warc_next_member(&bp, bp + got, &plen);
          if (plain == NULL || plen != sizeof(a_plain) - 1 ||
              memcmp(plain, a_plain, plen) != 0)
            err = 1;
          freet(plain);
        }
      }
    }
    checked = 1;
    freet(rec);
  }
  freet(data);
  if (!checked)
    err = 1;
  printf("warc-verbatim: %s\n", err ? "FAIL" : "OK");
  return err;
}

/* SURT canonicalization vectors (the CDXJ sort key: www-strip, default-port
   strip, host reversal, non-default port kept, IP/IPv6 verbatim). */
static int st_warc_surt(httrackp *opt, int argc, char **argv) {
  static const struct {
    const char *url, *want;
  } cases[] = {
      {"http://www.example.com/", "com,example)/"},
      {"http://example.com:80/a/b?q=1", "com,example)/a/b?q=1"},
      {"https://www.EXAMPLE.com/Path", "com,example)/Path"},
      {"https://example.com:443/", "com,example)/"},
      {"http://www2.example.com/x", "com,example)/x"},
      {"http://example.com:8080/p", "com,example:8080)/p"},
      {"http://user:pass@www.example.com/y", "com,example)/y"},
      {"http://192.168.0.1/z", "192.168.0.1)/z"},
      {"http://[2001:db8::1]/w", "[2001:db8::1])/w"},
      {"http://sub.a.example.co.uk/deep?x=1#frag",
       "uk,co,example,a,sub)/deep?x=1"},
  };

  int err = 0;
  size_t i;
  (void) opt;
  (void) argc;
  (void) argv;
  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    char out[512];
    if (warc_surt(cases[i].url, out, sizeof(out)) != 0 ||
        strcmp(out, cases[i].want) != 0) {
      fprintf(stderr, "warc-surt: %s -> %s (want %s)\n", cases[i].url, out,
              cases[i].want);
      err = 1;
    }
  }
  printf("warc-surt: %s\n", err ? "FAIL" : "OK");
  return err;
}

/* End-to-end CDXJ: crawl a handful of records with --warc-cdx, then verify the
   .cdx is sorted, has exactly one line per response/revisit/resource (none for
   warcinfo/request), and each offset/length points at a gzip member that
   independently inflates to a record whose WARC-Target-URI matches the line. */
static int st_warc_cdx(httrackp *opt, int argc, char **argv) {
  char wpath[HTS_URLMAXSIZE], cpath[HTS_URLMAXSIZE];
  warc_writer *w;
  unsigned char *warc = NULL, *cdx = NULL;
  size_t warc_len = 0, cdx_len = 0;
  hts_boolean saved_cdx;
  int err = 0, nlines = 0;
  const char *lp, *cend;
  char prev[2048];

  if (argc < 1) {
    fprintf(stderr, "warc-cdx: needs a writable directory\n");
    return 1;
  }
  fconcat(wpath, sizeof(wpath), argv[0], "warc-cdx.warc.gz");
  fconcat(cpath, sizeof(cpath), argv[0], "warc-cdx.cdx");
  saved_cdx = opt->warc_cdx;
  opt->warc_cdx = 1;
  w = warc_open(opt, wpath);
  assertf(w != NULL);
  warc_write_transaction(w, "http://www.example.com/one", "127.0.0.1",
                         "GET /one HTTP/1.1\r\nHost: www.example.com\r\n\r\n",
                         "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n",
                         "one body\n", 9, NULL, 200, 0, 0);
  warc_write_resource(w, "ftp://files.example.com/data.bin", "127.0.0.1",
                      "application/octet-stream", "\x00\x01\x02\x03", 4, NULL,
                      0);
  warc_write_transaction(w, "http://alpha.example.com/two", "127.0.0.1",
                         "GET /two HTTP/1.1\r\nHost: alpha.example.com\r\n\r\n",
                         "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n",
                         "two body\n", 9, NULL, 200, 0, 0);
  /* Same payload as /one at a new URL: identical-payload-digest revisit under
     OpenSSL, a full response otherwise; either way one index line. */
  warc_write_transaction(w, "http://zeta.example.com/dup", "127.0.0.1",
                         "GET /dup HTTP/1.1\r\nHost: zeta.example.com\r\n\r\n",
                         "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n",
                         "one body\n", 9, NULL, 200, 0, 0);
  warc_close(w);
  opt->warc_cdx = saved_cdx;

  warc = warc_slurp(wpath, &warc_len);
  cdx = warc_slurp(cpath, &cdx_len);
  assertf(warc != NULL);
  assertf(cdx != NULL);

  prev[0] = '\0';
  lp = (const char *) cdx;
  cend = (const char *) cdx + cdx_len;
  while (lp < cend) {
    const char *eol = memchr(lp, '\n', (size_t) (cend - lp));
    size_t llen = eol ? (size_t) (eol - lp) : (size_t) (cend - lp);
    char line[2048];
    const char *j, *us, *ue, *o, *l;
    char url[1024];
    unsigned long long off = 0, len = 0;
    const unsigned char *mp, *mend;
    unsigned char *rec;
    size_t rlen = 0, urllen;
    if (llen == 0) {
      lp = eol ? eol + 1 : cend;
      continue;
    }
    if (llen >= sizeof(line)) {
      err = 1;
      break;
    }
    memcpy(line, lp, llen);
    line[llen] = '\0';
    nlines++;
    if (prev[0] != '\0' && strcmp(prev, line) > 0)
      err = 1; /* must be sorted */
    strlcpybuff(prev, line, sizeof(prev));
    j = strstr(line, "\"url\": \"");
    o = strstr(line, "\"offset\": \"");
    l = strstr(line, "\"length\": \"");
    if (j == NULL || o == NULL || l == NULL ||
        sscanf(o + 11, "%llu", &off) != 1 ||
        sscanf(l + 11, "%llu", &len) != 1) {
      err = 1;
      goto nextline;
    }
    us = j + 8;
    ue = strchr(us, '"');
    if (ue == NULL || (urllen = (size_t) (ue - us)) >= sizeof(url)) {
      err = 1;
      goto nextline;
    }
    memcpy(url, us, urllen);
    url[urllen] = '\0';
    if (len == 0 || off > warc_len || len > warc_len - off) {
      err = 1;
      goto nextline;
    }
    mp = warc + off;
    mend = warc + off + len;
    rec = warc_next_member(&mp, mend, &rlen);
    if (rec == NULL) {
      err = 1;
      goto nextline;
    }
    {
      char needle[1100];
      snprintf(needle, sizeof(needle), "WARC-Target-URI: %s\r\n", url);
      if (warc_memstr((char *) rec, needle, rlen, strlen(needle)) == NULL)
        err = 1;
    }
    freet(rec);
  nextline:
    lp = eol ? eol + 1 : cend;
  }
  freet(warc);
  freet(cdx);
  if (nlines != 4)
    err = 1; /* 3 responses/revisits + 1 resource; no warcinfo/request */
  printf("warc-cdx: %d index lines: %s\n", nlines, err ? "FAIL" : "OK");
  return err;
}

#if HTS_USEOPENSSL
/* Lowercase-hex SHA-256 of n bytes into out[65]; 1 on success. */
static int wacz_test_sha256(const void *p, size_t n, char out[65]) {
  EVP_MD_CTX *c = EVP_MD_CTX_new();
  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned int mdlen = 0, i;
  static const char hx[] = "0123456789abcdef";
  int ok;
  if (c == NULL)
    return 0;
  ok = EVP_DigestInit_ex(c, EVP_sha256(), NULL) == 1 &&
       (n == 0 || EVP_DigestUpdate(c, p, n) == 1) &&
       EVP_DigestFinal_ex(c, md, &mdlen) == 1 && mdlen == 32;
  EVP_MD_CTX_free(c);
  if (!ok)
    return 0;
  for (i = 0; i < 32; i++) {
    out[i * 2] = hx[md[i] >> 4];
    out[i * 2 + 1] = hx[md[i] & 0x0F];
  }
  out[64] = '\0';
  return 1;
}

/* One unzipped WACZ member: name, raw bytes, and the ZIP compression method. */
typedef struct {
  char name[256];
  unsigned char *data;
  size_t len;
  int method;
} wacz_entry;

/* Package a 2-record WARC as a WACZ, then unzip it in-process and assert the
   fixed layout, STORE-mode entries, recomputing sha256 digests, the digest
   chain, and the pages.jsonl header. */
static int st_warc_wacz(httrackp *opt, int argc, char **argv) {
  char wpath[HTS_URLMAXSIZE], waczpath[HTS_URLMAXSIZE], cdxpath[HTS_URLMAXSIZE];
  warc_writer *w;
  hts_boolean saved_cdx, saved_wacz;
  wacz_entry ent[16];
  int nent = 0, err = 0, i;
  unzFile uf;
  const wacz_entry *dp = NULL, *dig = NULL, *pages = NULL;
  int have_archive = 0, have_index = 0, all_store = 1;
  LLint good_size;

  if (argc < 1) {
    fprintf(stderr, "warc-wacz: needs a writable directory\n");
    return 1;
  }
  fconcat(wpath, sizeof(wpath), argv[0], "warc-wacz.warc.gz");
  fconcat(waczpath, sizeof(waczpath), argv[0], "warc-wacz.wacz");
  fconcat(cdxpath, sizeof(cdxpath), argv[0], "warc-wacz.cdx");
  saved_cdx = opt->warc_cdx;
  saved_wacz = opt->warc_wacz;
  opt->warc_cdx = 1;
  opt->warc_wacz = 1;
  w = warc_open(opt, wpath);
  assertf(w != NULL);
  warc_write_transaction(w, "http://www.example.com/", "127.0.0.1",
                         "GET / HTTP/1.1\r\nHost: www.example.com\r\n\r\n",
                         "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n",
                         "<html>home</html>\n", 18, NULL, 200, 0, 0);
  warc_write_transaction(
      w, "http://www.example.com/data.bin", "127.0.0.1",
      "GET /data.bin HTTP/1.1\r\nHost: www.example.com\r\n\r\n",
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/octet-stream\r\n\r\n",
      "\x00\x01\x02\x03\x04", 5, NULL, 200, 0, 0);
  warc_close(w);

  /* Unzip every member in-process. */
  uf = unzOpen(waczpath);
  assertf(uf != NULL);
  if (unzGoToFirstFile(uf) == UNZ_OK) {
    do {
      unz_file_info info;
      wacz_entry *e;
      if (nent >= (int) (sizeof(ent) / sizeof(ent[0]))) {
        err = 1;
        break;
      }
      e = &ent[nent];
      if (unzGetCurrentFileInfo(uf, &info, e->name, sizeof(e->name), NULL, 0,
                                NULL, 0) != UNZ_OK) {
        err = 1;
        break;
      }
      e->method = (int) info.compression_method;
      e->len = (size_t) info.uncompressed_size;
      e->data = malloct(e->len ? e->len : 1);
      if (e->data == NULL || unzOpenCurrentFile(uf) != UNZ_OK) {
        err = 1;
        break;
      }
      if (e->len > 0 &&
          unzReadCurrentFile(uf, e->data, (unsigned) e->len) != (int) e->len)
        err = 1;
      unzCloseCurrentFile(uf);
      nent++;
    } while (unzGoToNextFile(uf) == UNZ_OK);
  }
  unzClose(uf);

  /* Classify members and assert STORE mode (WACZ spec requirement). */
  for (i = 0; i < nent; i++) {
    const wacz_entry *e = &ent[i];
    if (e->method != 0)
      all_store = 0;
    if (strncmp(e->name, "archive/", 8) == 0)
      have_archive = 1;
    else if (strcmp(e->name, "indexes/index.cdx") == 0)
      have_index = 1;
    else if (strcmp(e->name, "pages/pages.jsonl") == 0)
      pages = e;
    else if (strcmp(e->name, "datapackage.json") == 0)
      dp = e;
    else if (strcmp(e->name, "datapackage-digest.json") == 0)
      dig = e;
  }
  if (!have_archive || !have_index || pages == NULL || dp == NULL ||
      dig == NULL || !all_store)
    err = 1;

  /* pages.jsonl: header line, then >= 1 body row carrying url + ts. */
  if (pages != NULL) {
    if (pages->len < 27 ||
        memcmp(pages->data, "{\"format\": \"json-pages-1.0\"", 27) != 0)
      err = 1;
    else {
      const char *nl = memchr(pages->data, '\n', pages->len);
      const char *body = nl ? nl + 1 : NULL;
      size_t blen =
          body ? pages->len - (size_t) (body - (char *) pages->data) : 0;
      if (body == NULL || blen == 0 ||
          warc_memstr(body, "\"url\": ", blen, 7) == NULL ||
          warc_memstr(body, "\"ts\": ", blen, 6) == NULL)
        err = 1;
    }
  }

  /* Every datapackage resource hash recomputes from the stored member bytes. */
  if (dp != NULL) {
    char *json = malloct(dp->len + 1);
    if (json == NULL) {
      err = 1;
    } else {
      const char *p;
      memcpy(json, dp->data, dp->len);
      json[dp->len] = '\0';
      if (strstr(json, "\"profile\": \"data-package\"") == NULL ||
          strstr(json, "\"wacz_version\": \"") == NULL)
        err = 1;
      p = json;
      while ((p = strstr(p, "\"path\": \"")) != NULL) {
        char path[256], want[80], got[65];
        const char *pe, *h;
        size_t plen;
        p += 9;
        pe = strchr(p, '"');
        if (pe == NULL || (plen = (size_t) (pe - p)) >= sizeof(path)) {
          err = 1;
          break;
        }
        memcpy(path, p, plen);
        path[plen] = '\0';
        h = strstr(pe, "\"hash\": \"sha256:");
        if (h == NULL || sscanf(h + 16, "%79[0-9a-f]", want) != 1) {
          err = 1;
          break;
        }
        for (i = 0; i < nent; i++)
          if (strcmp(ent[i].name, path) == 0)
            break;
        if (i == nent || !wacz_test_sha256(ent[i].data, ent[i].len, got) ||
            strcmp(got, want) != 0)
          err = 1;
        p = pe;
      }
      freet(json);
    }
  }

  /* datapackage-digest.json chains sha256(datapackage.json). */
  if (dp != NULL && dig != NULL) {
    char dphex[65], *djson = malloct(dig->len + 1);
    const char *h;
    char want[80];
    if (djson == NULL || !wacz_test_sha256(dp->data, dp->len, dphex)) {
      err = 1;
    } else {
      memcpy(djson, dig->data, dig->len);
      djson[dig->len] = '\0';
      if (strstr(djson, "\"path\": \"datapackage.json\"") == NULL)
        err = 1;
      h = strstr(djson, "\"hash\": \"sha256:");
      if (h == NULL || sscanf(h + 16, "%79[0-9a-f]", want) != 1 ||
          strcmp(want, dphex) != 0)
        err = 1;
    }
    freet(djson);
  }

  for (i = 0; i < nent; i++)
    freet(ent[i].data);

  /* #522-class: a failed re-package must leave the existing .wacz untouched.
     Drop the .cdx and re-run empty so packaging fails on the missing index. */
  good_size = fsize(waczpath);
  if (good_size <= 0)
    err = 1;
  (void) UNLINK(cdxpath);
  w = warc_open(opt, wpath);
  assertf(w != NULL);
  warc_close(w);
  if (fsize(waczpath) != good_size) /* destroyed or rewritten = data loss */
    err = 1;

  opt->warc_cdx = saved_cdx;
  opt->warc_wacz = saved_wacz;
  printf("warc-wacz: %d members (store=%d): %s\n", nent, all_store,
         err ? "FAIL" : "OK");
  return err;
}
#endif

// -#test=longpath <dir>: round-trip a >MAX_PATH (260) file through the file
// wrappers, exercising hts_pathToUCS2's \\?\ prefixing on Windows (#133).
static int st_longpath(httrackp *opt, int argc, char **argv) {
  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "longpath: needs a writable base dir\n");
    return 1;
  }
  char path[HTS_URLMAXSIZE * 2];
  size_t n = (size_t) snprintf(path, sizeof(path), "%s", argv[0]);

  while (n > 0 && (path[n - 1] == '/' || path[n - 1] == '\\')) {
    path[--n] = '\0';
  }
  // 40-char segments: each under the 255 per-component limit \\?\ can't lift.
  static const char seg[] = "/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  while (n + sizeof(seg) - 1 < 300) {
    memcpybuff(path + n, seg, sizeof(seg));
    n += sizeof(seg) - 1;
    if (MKDIR(path) != 0 && errno != EEXIST) {
      fprintf(stderr, "longpath: mkdir failed at %u chars: %s\n", (unsigned) n,
              strerror(errno));
      return 1;
    }
  }
  memcpybuff(path + n, "/leaf.bin", sizeof("/leaf.bin"));
  n += sizeof("/leaf.bin") - 1;
  assertf(n > 260); /* must exceed the limit \\?\ lifts */

  static const char payload[] = "longpath-ok";
  FILE *fp = FOPEN(path, "wb");

  if (fp == NULL) {
    fprintf(stderr, "longpath: create failed (%u chars): %s\n", (unsigned) n,
            strerror(errno));
    return 1;
  }
  assertf(fwrite(payload, 1, sizeof(payload), fp) == sizeof(payload));
  fclose(fp);

  STRUCT_STAT st;

  assertf(STAT(path, &st) == 0);
  assertf((size_t) st.st_size == sizeof(payload));

  char buf[64];

  fp = FOPEN(path, "rb");
  assertf(fp != NULL);
  assertf(fread(buf, 1, sizeof(payload), fp) == sizeof(payload));
  fclose(fp);
  assertf(memcmp(buf, payload, sizeof(payload)) == 0);
  assertf(UNLINK(path) == 0);

  printf("longpath: round-tripped a %u-char path: OK\n", (unsigned) n);
  return 0;
}

// -#test=mirrorio <dir>: round-trip a file through a long AND non-ASCII path
// via the mirror I/O wrappers — fexist_utf8/fsize_utf8, FOPEN/UNLINK, and the
// new hts_rmdir_utf8 (RMDIR) teardown (#133, #630).
static int st_mirrorio(httrackp *opt, int argc, char **argv) {
  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "mirrorio: needs a writable base dir\n");
    return 1;
  }
  char path[HTS_URLMAXSIZE * 2];
  size_t n = (size_t) snprintf(path, sizeof(path), "%s", argv[0]);

  while (n > 0 && (path[n - 1] == '/' || path[n - 1] == '\\')) {
    path[--n] = '\0';
  }
  const size_t base = n; /* the caller's base dir; teardown stops here */
  // First segment carries non-ASCII UTF-8 (é 中) to drive the charset axis
  // (#630); ASCII 40-char segments then push the total past MAX_PATH (#133).
  static const char nseg[] = "/\xC3\xA9\xE4\xB8\xAD-non-ascii-seg";
  static const char seg[] = "/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

  memcpybuff(path + n, nseg, sizeof(nseg));
  n += sizeof(nseg) - 1;
  if (MKDIR(path) != 0 && errno != EEXIST) {
    fprintf(stderr, "mirrorio: mkdir failed (non-ascii): %s\n",
            strerror(errno));
    return 1;
  }
  while (n + sizeof(seg) - 1 < 300) {
    memcpybuff(path + n, seg, sizeof(seg));
    n += sizeof(seg) - 1;
    if (MKDIR(path) != 0 && errno != EEXIST) {
      fprintf(stderr, "mirrorio: mkdir failed at %u chars: %s\n", (unsigned) n,
              strerror(errno));
      return 1;
    }
  }
  const size_t leafdir = n;

  memcpybuff(path + n, "/leaf.bin", sizeof("/leaf.bin"));
  n += sizeof("/leaf.bin") - 1;
  assertf(n > 260); /* must exceed the limit \\?\ lifts */

  static const char payload[] = "mirrorio-ok";

  assertf(!fexist_utf8(path)); /* absent before creation, through the guard */
  FILE *fp = FOPEN(path, "wb");

  if (fp == NULL) {
    fprintf(stderr, "mirrorio: create failed (%u chars): %s\n", (unsigned) n,
            strerror(errno));
    return 1;
  }
  assertf(fwrite(payload, 1, sizeof(payload), fp) == sizeof(payload));
  fclose(fp);
  assertf(fexist_utf8(path));
  assertf(fsize_utf8(path) == (LLint) sizeof(payload));
  assertf(UNLINK(path) == 0);
  assertf(!fexist_utf8(path));

  // Tear the directory chain down through the UTF-8/long-path rmdir wrapper.
  path[leafdir] = '\0';
  while (strlen(path) > base) {
    char *const slash = strrchr(path, '/');

    if (RMDIR(path) != 0) {
      fprintf(stderr, "mirrorio: rmdir failed: %s\n", strerror(errno));
      return 1;
    }
    if (slash == NULL || (size_t) (slash - path) < base) {
      break;
    }
    *slash = '\0';
  }

  printf("mirrorio: round-tripped a %u-char non-ASCII path: OK\n",
         (unsigned) n);
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
    {"filterbounds", "", "matcher length/work caps reject hostile patterns",
     st_filterbounds},
    {"simplify", "<path>", "collapse ./ and ../ in a path", st_simplify},
    {"expandhome", "<path>", "expand a leading ~/ into $HOME", st_expandhome},
    {"stripquery", "", "--strip-query pattern/key stripping self-test",
     st_stripquery},
    {"urlhack", "", "-%u url-hack sub-flag (www/slash/query) self-test",
     st_urlhack},
    {"redirect-samefile", "", "same-file redirect detection self-test (#159)",
     st_redirect_samefile},
    {"mime", "<filename>", "MIME type for a filename", st_mime},
    {"charset", "<charset> <hex:..|string>",
     "convert a string to UTF-8 from a charset", st_charset},
    {"metacharset", "<html>", "extract the <meta> charset from an HTML page",
     st_metacharset},
    {"isutf8", "<hex:..|string>", "is the string valid UTF-8 (1/0)", st_isutf8},
    {"idna-encode", "<host>", "encode a hostname to IDNA/punycode",
     st_idna_encode},
    {"idna-decode", "<host>", "decode an IDNA/punycode hostname",
     st_idna_decode},
    {"entities", "<string> [encoding]", "unescape HTML entities", st_entities},
    {"footerfmt", "<template>", "-%F footer positional/named expansion",
     st_footerfmt},
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
    {"proxyurl", "<proxy-arg>", "parse a -P proxy URL into host/port",
     st_proxyurl},
    {"socks5", "", "SOCKS5 handshake framing and credential self-test",
     st_socks5},
    {"identabs", "", "ident_url_absolute one-byte fil[] overflow self-test",
     st_identabs},
    {"stripport", "", "default :80 port strip preserves host (#627)",
     st_stripport},
    {"header", "<raw-header-line> ...", "response header-line parsing",
     st_header},
    {"headerlong", "[header-name:]",
     "over-long header value must not overflow the parse scratch",
     st_headerlong},
    {"crange", "<raw-content-range-line> ...",
     "Content-Range parse integer safety", st_crange},
    {"xfread-limit", "", "in-memory receive buffer size bound",
     st_xfread_limit},
    {"savename", "<fil> <content-type> [key=value ...]",
     "local save-name for a URL", st_savename},
    {"sniff", "<content-type> <hex:..|text>", "MIME magic consistency",
     st_sniff},
    {"fsize", "<dir>", "file size past the 2GB signed-32-bit wrap", st_fsize},
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
    {"zip-repair-shift", "<dir>",
     "cache zip-repair header read must not overflow a signed shift",
     st_zip_repair_shift},
    {"dns", "", "DNS resolver/cache self-test", st_dns},
    {"dnstimeout", "", "a slow DNS resolve is bounded and holds no lock",
     st_dnstimeout},
    {"cookies", "", "cookie request-header self-test", st_cookies},
    {"useragent", "", "default User-Agent self-test", st_useragent},
    {"makeindex", "[dir]", "hts_finish_makeindex footer/refresh self-test",
     st_makeindex},
    {"topindex", "[dir]",
     "hts_buildtopindex charset handling of a non-ASCII project dir",
     st_topindex},
    {"inplace-escape", "", "inplace_escape_* vs escape_* equivalence self-test",
     st_inplace_escape},
    {"escape-room", "", "HT_ADD_HTMLESCAPED* reservation-factor self-test",
     st_escape_room},
    {"status", "", "HTTP status code -> reason phrase self-test", st_status},
    {"acceptencoding", "[dir]",
     "Accept-Encoding advertises gzip+deflate, both decode", st_acceptencoding},
    {"contentcodings", "[dir]",
     "brotli and zstd bodies decode; bombs and unknown codings are refused",
     st_contentcodings},
    {"robots", "", "robots.txt RFC 9309 Allow/Disallow precedence self-test",
     st_robots},
    {"ftp-line", "", "get_ftp_line bounds a hostile FTP reply line",
     st_ftpline},
    {"ftp-userpass", "", "ftp_split_userpass bounds URL userinfo", st_ftpuser},
    {"warc", "<dir>", "WARC/1.1 writer: framing, digests, revisit dedup",
     st_warc},
    {"warc-trunc", "<dir>", "WARC-Truncated on a cap-truncated body",
     st_warc_trunc},
    {"warc-ftp", "<dir>", "ftp resource record (no HTTP envelope)",
     st_warc_ftp},
    {"warc-rotate", "<dir>", "--warc-max-size segment rotation",
     st_warc_rotate},
    {"warc-verbatim", "<dir>", "verbatim compressed response body (default)",
     st_warc_verbatim},
    {"warc-surt", "", "SURT canonicalization of the CDXJ sort key",
     st_warc_surt},
    {"longpath", "<dir>",
     "round-trip a >MAX_PATH file through the _w* wrappers (\\\\?\\ on "
     "Windows)",
     st_longpath},
    {"mirrorio", "<dir>",
     "round-trip a long+non-ASCII path through the mirror I/O wrappers",
     st_mirrorio},
    {"warc-cdx", "<dir>", "--warc-cdx CDXJ index: sorted, offsets inflate",
     st_warc_cdx},
#if HTS_USEOPENSSL
    {"warc-wacz", "<dir>", "--wacz package: layout, STORE mode, sha256 digests",
     st_warc_wacz},
#endif
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
