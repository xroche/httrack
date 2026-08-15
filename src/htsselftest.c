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
#include "htsmodules.h"
#include "htsback.h"
#include "htsdefines.h"
#include "htslib.h"
#include "htsalias.h"
#include "htsarrays.h"
#include "htsparse.h"
#include "htscache.h"
#include "htscache_selftest.h"
#include "htsdns_selftest.h"
#include "htscharset.h"
#include "htscmdline.h"
#include "htscoremain.h"
#include "htsencoding.h"
#include "htsftp.h"
#include "htsmd5.h"
#include "htssniff.h"
#include "htscodec.h"
#include "htsproxy.h"
#include "htsrandom.h"
#include "htssitemap.h"
#include "htswarc.h"
#include "htschanges.h"
#include "htssinglefile.h"
#include "htszlib.h"
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
#include <wchar.h>
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

/* Wider than the longest test value, so a byte written past any destination
   still lands in the canary rather than beyond it. */
enum { ST_ASSUME_CANARY = 1200 };

/* Build "\ncgi=<value_len bytes of 'B'>\n" into opt->mimedefs. */
static void st_assume_rule(httrackp *opt, size_t value_len) {
  size_t i;

  StringCopy(opt->mimedefs, "\ncgi=");
  for (i = 0; i < value_len; i++)
    StringAddchar(opt->mimedefs, 'B');
  StringCat(opt->mimedefs, "\n");
}

static unsigned st_assume_clips; /* clip warnings the callback has seen */

static void st_assume_log(httrackp *opt, int type, const char *format,
                          va_list args) {
  (void) opt;
  (void) format;
  (void) args;
  if ((type & 0xff) == LOG_WARNING)
    st_assume_clips++;
}

/* An empty selector runs every destination. */
static hts_boolean st_assume_wants(const char *only, const char *capacity) {
  return *only == '\0' || strcmp(only, capacity) == 0 ? HTS_TRUE : HTS_FALSE;
}

/* An --assume value is clipped to the buffer it lands in, and the clip is
   reported rather than fatal (#1276). Each value overshoots a different subset
   of the three destinations, so one shared bound cannot pass them all. */
static int st_assumemime(httrackp *opt, int argc, char **argv) {
  static const size_t values[] = {200, 300, 1100};
  /* pre-fix every destination smashes and only the first assertion to fire is
     visible, so each is selectable by its capacity */
  const char *const only = argc >= 1 ? argv[0] : "";
  const int save_debug = opt->debug;
  char canary[ST_ASSUME_CANARY];
  size_t v;

  if (!st_assume_wants(only, "128") && !st_assume_wants(only, "256") &&
      !st_assume_wants(only, "1024")) {
    fprintf(stderr, "assumemime: want one of 128, 256, 1024\n");
    return 1;
  }
  memset(canary, '#', sizeof(canary));
  /* the callback runs above the level filter, so the warnings can be counted
     while the log itself stays quiet and stdout carries one line */
  opt->debug = LOG_ERROR;
  hts_set_log_vprint_callback(st_assume_log);

  for (v = 0; v < sizeof(values) / sizeof(values[0]); v++) {
    const size_t len = values[v];

    st_assume_rule(opt, len);

    /* ishtml()'s mime[256] */
    if (st_assume_wants(only, "256")) {
      struct {
        char dst[256];
        char canary[ST_ASSUME_CANARY];
      } s;

      memset(&s, '#', sizeof(s));
      st_assume_clips = 0;
      assertf(get_userhttptype(opt, s.dst, sizeof(s.dst), "/x.cgi"));
      assertf(strlen(s.dst) == (len < sizeof(s.dst) ? len : sizeof(s.dst) - 1));
      assertf(memcmp(s.canary, canary, sizeof(s.canary)) == 0);
      assertf(st_assume_clips == (len < sizeof(s.dst) ? 0u : 1u));
      /* the frame ishtml() owns, which is where #1276 was reported */
      assertf(ishtml(opt, "/x.cgi") == 0);
    }

    /* url_savename()'s mime[1024] */
    if (st_assume_wants(only, "1024")) {
      struct {
        char dst[1024];
        char canary[ST_ASSUME_CANARY];
      } l;

      memset(&l, '#', sizeof(l));
      st_assume_clips = 0;
      assertf(get_userhttptype(opt, l.dst, sizeof(l.dst), "/x.cgi"));
      assertf(strlen(l.dst) == (len < sizeof(l.dst) ? len : sizeof(l.dst) - 1));
      assertf(memcmp(l.canary, canary, sizeof(l.canary)) == 0);
      assertf(st_assume_clips == (len < sizeof(l.dst) ? 0u : 1u));
    }

    /* htsblk.contenttype[128]: charset and contentencoding follow it in the
       same struct, so an overrun stays intra-object and neither ASan nor
       _FORTIFY_SOURCE reports it */
    if (st_assume_wants(only, "128")) {
      htsblk r;

      memset(&r, '#', sizeof(r));
      r.contenttype[0] = '\0';
      st_assume_clips = 0;
      assertf(get_httptype_sized(opt, r.contenttype, sizeof(r.contenttype),
                                 "/x.cgi", 0));
      assertf(strlen(r.contenttype) == sizeof(r.contenttype) - 1);
      assertf(memcmp(r.charset, canary, sizeof(r.charset)) == 0);
      assertf(memcmp(r.contentencoding, canary, sizeof(r.contentencoding)) ==
              0);
      assertf(st_assume_clips == 1);
    }
  }

  /* control: a value every destination holds is written whole, unclipped and
     unreported, and still reaches ishtml() as the type it names */
  StringCopy(opt->mimedefs, "\ncgi=text/html\n");
  {
    char mime[256];

    st_assume_clips = 0;
    assertf(get_userhttptype(opt, mime, sizeof(mime), "/x.cgi"));
    assertf(strcmp(mime, "text/html") == 0);
    assertf(st_assume_clips == 0);
    assertf(ishtml(opt, "/x.cgi") == 1);
  }

  hts_set_log_vprint_callback(NULL);
  opt->debug = save_debug;
  printf("assumemime ok\n");
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

/* Oracle is the raw Win32 two-step this replaces, so a mis-wired direction or
   a plain copy fails whatever the machine's ACP. */
static int st_syscharset(httrackp *opt, int argc, char **argv) {
#ifdef _WIN32
  static const char *const utf8 = "caf\xC3\xA9 \xE2\x82\xAC"; /* "café €" */
  const UINT cp = GetACP();
  const int len = (int) strlen(utf8);
  WCHAR wide[64], round[64];
  char want[64];
  char *sys, *back, *part;
  int wn, n, lossless;

  (void) opt;
  (void) argc;
  (void) argv;
  wn = MultiByteToWideChar(CP_UTF8, 0, utf8, len, wide,
                           (int) (sizeof(wide) / sizeof(wide[0])));
  assertf(wn > 0);
  n = WideCharToMultiByte(cp, 0, wide, wn, want, (int) sizeof(want) - 1, NULL,
                          NULL);
  assertf(n > 0);
  want[n] = '\0';
  /* the ACP holds the string only if its bytes decode back to the same UTF-16;
     lpUsedDefaultChar would miss a best-fit mapping (é to a bare e) */
  lossless =
      MultiByteToWideChar(cp, 0, want, n, round,
                          (int) (sizeof(round) / sizeof(round[0]))) == wn &&
      memcmp(round, wide, (size_t) wn * sizeof(WCHAR)) == 0;

  sys = hts_convertStringUTF8ToSystem(utf8, (size_t) len);
  assertf(sys != NULL);
  assertf(strcmp(sys, want) == 0);
  assertf(strlen(sys) == (size_t) n); /* NUL-terminated, nothing past it */
  /* the copy the ASCII fast path returns is a pass on a UTF-8 ACP only */
  assertf(cp == CP_UTF8 || strcmp(sys, utf8) != 0);
  /* size bounds the read: a caller-given length, not strlen() */
  part = hts_convertStringUTF8ToSystem(utf8, 3);
  assertf(part != NULL && strcmp(part, "caf") == 0);
  freet(part);
  part = hts_convertStringUTF8ToSystem(utf8, 0);
  assertf(part != NULL && *part == '\0');
  freet(part);
  if (lossless) {
    back = hts_convertStringSystemToUTF8(sys, strlen(sys));
    assertf(back != NULL);
    assertf(strcmp(back, utf8) == 0);
    freet(back);
  }
  freet(sys);
  printf("syscharset: acp=%u %s: OK\n", (unsigned) cp,
         lossless ? "round-trip" : "one-way");
  return 0;
#else
  (void) opt;
  (void) argc;
  (void) argv;
  return 77; /* WIN32-only entry point */
#endif
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

// -#test=footerfmt <template>: expand a -%F footer with fixed values (drives
// tests/01_engine-footerfmt.test). Also asserts the published field names and
// the overflow/zero-size returns the CLI cap keeps out of reach.
static int st_footerfmt(httrackp *opt, int argc, char **argv) {
  // Spelled out, not read back from the engine: these ten are the published
  // contract front ends validate their templates against.
  static const char *const names[] = {
      "addr",    "path", "url",     "date",   "lastmodified",
      "version", "mime", "charset", "status", "size"};
  // Filled by id, as htsparse.c does, so the .test's expected strings pin each
  // name to its enum slot; a positional list would not see them drift apart.
  const char *values[HTS_FOOTER_FIELD_COUNT] = {NULL};
  size_t i;
  char out[1024];
  char tiny[4];

  (void) opt;
  values[HTS_FOOTER_ADDR] = "host.example";
  values[HTS_FOOTER_PATH] = "/dir/page.html";
  values[HTS_FOOTER_URL] = "http://host.example/dir/page.html";
  values[HTS_FOOTER_DATE] = "DATE";
  values[HTS_FOOTER_LASTMODIFIED] = "LASTMOD";
  values[HTS_FOOTER_VERSION] = "VER";
  values[HTS_FOOTER_MIME] = "text/html";
  values[HTS_FOOTER_CHARSET] = "utf-8";
  values[HTS_FOOTER_STATUS] = "200";
  values[HTS_FOOTER_SIZE] = "1234";

  for (i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
    assertf(hts_footer_field_ok(names[i]) == HTS_TRUE);
  }
  assertf(!hts_footer_field_ok(NULL));
  assertf(!hts_footer_field_ok(""));
  assertf(!hts_footer_field_ok("nosuchfield"));
  // A prefix or a longer name must not match, or "{addr}" and "{addrx}" would
  // validate alike.
  assertf(!hts_footer_field_ok("add"));
  assertf(!hts_footer_field_ok("addrx"));
  // Matching is exact: the expander is case-sensitive, so a validator that
  // accepted "{ADDR}" would green-light a template the crawl emits verbatim.
  assertf(!hts_footer_field_ok("Addr"));
  // Overflow (named and legacy) and a zero-size buffer must return <0, never
  // truncate silently or write out of bounds.
  assertf(hts_footer_format(tiny, sizeof(tiny), "{addr}", values) < 0);
  assertf(hts_footer_format(tiny, sizeof(tiny), "a %s b", values) < 0);
  assertf(hts_footer_format(out, 0, "", values) < 0);
  // An empty template yields an empty, terminated string.
  assertf(hts_footer_format(out, sizeof(out), "", values) == 1 &&
          out[0] == '\0');
  if (argc < 1) {
    fprintf(stderr, "footerfmt: needs a template\n");
    return 1;
  }
  if (hts_footer_format(out, sizeof(out), argv[0], values) < 0) {
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

// hts_split_cmdline(): the vector must grow with the argument count, and a
// quote inside a value must not end the argument and hand -V to the parser.
static int st_cmdlinesplit(httrackp *opt, int argc, char **argv) {
  char line[512];
  char **args;
  int nargs = 0;

  (void) opt;
  (void) argc;
  (void) argv;

  // control: every separator splits, and argv[0] is the program name
  strcpybuff(line, "httrack http://x/ --quiet\t-c8\n-O out");
  args = hts_split_cmdline(line, &nargs);
  assertf(args != NULL && nargs == 6);
  assertf(args[nargs] == NULL); // callers may walk to the terminator
  assertf(strcmp(args[0], "httrack") == 0);
  assertf(strcmp(args[1], "http://x/") == 0);
  assertf(strcmp(args[2], "--quiet") == 0);
  assertf(strcmp(args[3], "-c8") == 0);
  assertf(strcmp(args[4], "-O") == 0);
  assertf(strcmp(args[5], "out") == 0);
  freet(args);

  // the template pads with whitespace: empty arguments are kept (the engine
  // skips them), so the count is one per separator
  strcpybuff(line, "httrack  --quiet");
  args = hts_split_cmdline(line, &nargs);
  assertf(nargs == 3 && args[1][0] == '\0');
  assertf(strcmp(args[2], "--quiet") == 0);
  freet(args);

  // a quoted run keeps both its spaces and its quotes: the engine unquotes
  strcpybuff(line, "httrack --user-agent \"Mozilla 5.0\" -c8");
  args = hts_split_cmdline(line, &nargs);
  assertf(nargs == 4);
  assertf(strcmp(args[2], "\"Mozilla 5.0\"") == 0);
  assertf(strcmp(args[3], "-c8") == 0);
  freet(args);

  // an escaped quote is a literal quote, not the end of the argument: the
  // engine strips only the outer pair
  strcpybuff(line, "httrack --user-agent \"x\\\" -V \\\"touch /tmp/pwn\" -c8");
  args = hts_split_cmdline(line, &nargs);
  assertf(nargs == 4);
  assertf(strcmp(args[2], "\"x\" -V \"touch /tmp/pwn\"") == 0);
  assertf(strcmp(args[3], "-c8") == 0);
  freet(args);

  // \\ is a literal backslash, so a Windows path survives
  strcpybuff(line, "httrack --path \"C:\\\\dir\\\\sub\"");
  args = hts_split_cmdline(line, &nargs);
  assertf(nargs == 3);
  assertf(strcmp(args[2], "\"C:\\dir\\sub\"") == 0);
  freet(args);

  // outside a quoted run a backslash is literal: the url and wildcard-filter
  // fields, which the wizard cannot quote, read as before
  strcpybuff(line, "httrack -*\\** +*.png");
  args = hts_split_cmdline(line, &nargs);
  assertf(nargs == 3);
  assertf(strcmp(args[1], "-*\\**") == 0);
  assertf(strcmp(args[2], "+*.png") == 0);
  freet(args);

  // a quoted run leaves slots unused, so the terminator has to be written and
  // not inherited: size the vector from a full line first, so freeing it hands
  // the same chunk back with stale pointers in those slots
  strcpybuff(line, "httrack a b c d e");
  args = hts_split_cmdline(line, &nargs);
  assertf(nargs == 6);
  freet(args);
  strcpybuff(line, "httrack \"a b c d e\"");
  args = hts_split_cmdline(line, &nargs);
  assertf(nargs == 2);
  assertf(args[nargs] == NULL);
  freet(args);

  // an unterminated quote protects the rest of the line, as one argument
  strcpybuff(line, "httrack --footer \"unbalanced -V x");
  args = hts_split_cmdline(line, &nargs);
  assertf(nargs == 3);
  assertf(strcmp(args[2], "\"unbalanced -V x") == 0);
  freet(args);

  // past the 1024 entries the vector used to hold: distinct arguments, so a
  // write beyond the allocation cannot read back as the expected parse
  {
    const int n = 2000;
    const size_t size = 16 * (size_t) n + 16;
    char *big = malloct(size);
    size_t pos = 0;
    int i;

    assertf(big != NULL);
    pos = (size_t) snprintf(big, size, "httrack");
    assertf(pos < size);
    for (i = 0; i < n; i++) {
      // snprintf returns what it wanted to write, so accumulating it blind
      // would let the next size argument wrap
      const int len = snprintf(big + pos, size - pos, " a%d", i);

      assertf(len > 0 && (size_t) len < size - pos);
      pos += (size_t) len;
    }
    args = hts_split_cmdline(big, &nargs);
    assertf(args != NULL && nargs == n + 1);
    assertf(args[nargs] == NULL);
    for (i = 0; i < n; i++) {
      char expect[16];

      snprintf(expect, sizeof(expect), "a%d", i);
      assertf(strcmp(args[i + 1], expect) == 0);
    }
    freet(args);
    freet(big);
  }

  printf("cmdline-split self-test OK\n");
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
    /* Doubling past SIZE_MAX used to wrap capa to 0 and spin forever. */
    TypedArray(char) a = EMPTY_TYPED_ARRAY;

    room = (size_t) -1;
    TypedArrayEnsureRoom(a, room);
    printf("arrays: NOT aborted (%p)\n", TypedArrayPtr(a)); /* unreachable */
    return 1;
  } else {
    const int err = array_growth_selftests();

    printf("arrays: %s\n", err ? "FAIL" : "OK");
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

  from->changes = HTS_TRUE;
  to->changes = HTS_FALSE;
  copy_htsopt(from, to);
  if (to->changes != HTS_TRUE)
    err = 1;

  /* single_file pair: the cap is guarded by >0, so an unset source must not
     overwrite the default the target already carries */
  from->single_file = HTS_TRUE;
  from->single_file_max_size = 4096;
  to->single_file = HTS_FALSE;
  to->single_file_max_size = SINGLEFILE_DEFAULT_MAX_SIZE;
  copy_htsopt(from, to);
  if (!to->single_file || to->single_file_max_size != 4096)
    err = 1;
  from->single_file_max_size = 0;
  copy_htsopt(from, to);
  if (to->single_file_max_size != 4096)
    err = 1;

  /* sitemap pair: the flag latches on, the URL takes the String deep copy */
  from->sitemap = HTS_TRUE;
  StringCopy(from->sitemap_url, "http://h.test/sitemap.xml");
  to->sitemap = HTS_FALSE;
  StringCopy(to->sitemap_url, "");
  copy_htsopt(from, to);
  if (!to->sitemap ||
      strcmp(StringBuff(to->sitemap_url), "http://h.test/sitemap.xml") != 0)
    err = 1;
  from->sitemap = HTS_FALSE;
  StringCopy(from->sitemap_url, "");
  copy_htsopt(from, to);
  if (!to->sitemap ||
      strcmp(StringBuff(to->sitemap_url), "http://h.test/sitemap.xml") != 0)
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

  /* a control byte in the host would be a field of its own in the ATYP=domain
     request; the port that follows it must not hide it (#1010) */
  {
    static const char *const hostile[] = {"ori\rgin.test", "ori\rgin.test:80"};
    size_t k;

    for (k = 0; k < sizeof(hostile) / sizeof(hostile[0]); k++) {
      len = socks5_reply(script, 0x01, v4, sizeof(v4));
      io.reply = script;
      io.reply_len = len;
      assertf(socks5_handshake_scripted(opt, hostile[k], proxy, &io) == 0);
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

/* escape_remove_control() compacts in place, so it has to terminate at the new
   end or the caller reads the compacted head plus the original tail (#974). */
static int st_escape_control(httrackp *opt, int argc, char **argv) {
  static const struct {
    const char *in;
    const char *out;
  } cases[] = {
      /* VT and FF are the ones that reach here: is_space() passes them */
      {"/a\013bc", "/abc"},
      {"\014abc", "abc"},
      /* nothing moves, but the end does */
      {"abc\013", "abc"},
      {"abc\001def", "abcdef"},
      {"\001\002\003", ""},
      /* untouched inputs: the terminator must stay where it was */
      {"abc", "abc"},
      {"", ""},
      /* only bytes below 32 go: DEL and high bytes are not control here */
      {"a\177\303\251", "a\177\303\251"},
      /* both sides of the >= 32 cut, and a shrink of more than one byte */
      {"a b", "a b"},
      {"a\037b\036c", "abc"},
  };

  const size_t ncases = sizeof(cases) / sizeof(cases[0]);
  char buf[1024];
  size_t k, m;

  (void) opt;
  if (argc > 0) {
    const size_t n = st_decode_body(argv[0], buf, sizeof(buf));

    assertf(n < sizeof(buf));
    escape_remove_control(buf);
    printf("escape-control: len=%d out=hex:", (int) strlen(buf));
    for (k = 0; buf[k] != '\0'; k++) {
      printf("%02x", (unsigned char) buf[k]);
    }
    printf("\n");
    return 0;
  }

  for (k = 0; k < ncases; k++) {
    const size_t inlen = strlen(cases[k].in);
    const size_t outlen = strlen(cases[k].out);

    /* poison, so a stray write shows up as a byte a zeroed buffer would hide */
    memset(buf, '#', sizeof(buf));
    memcpy(buf, cases[k].in, inlen + 1);
    escape_remove_control(buf);
    assertf(strlen(buf) == outlen);
    assertf(memcmp(buf, cases[k].out, outlen + 1) == 0);
    /* the terminator belongs inside the original string, never past its NUL */
    for (m = inlen + 1; m < sizeof(buf); m++) {
      assertf(buf[m] == '#');
    }
  }
  printf("escape-control self-test OK\n");
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
    const int err = errno;

    fprintf(stderr, "fsize: cannot extend '%s' to " LLintP ": %s\n", path,
            expected, strerror(err));
    UNLINK(path);
    /* EFBIG: file-size cap below 5GB (GNU/Hurd ext2fs); skip, don't fail. */
    return err == EFBIG ? 77 : 1;
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

/* 4GB+100KB wraps to ~108KB through an int, and needs 33 unsigned bits. A
   macro, not a static const: MSVC's C mode (/TC) rejects a const object
   used inside another object's static initializer below (C2099). */
#define HTS_ST_GROWSIZE_OVER32 (4LL * 1024 * 1024 * 1024 + 100 * 1024)

/* llint_grow_size_t() sizes the buffer holding a whole -%S list file: the
   result must be the exact 64-bit sum or a clean refusal, never a short one. */
static int st_growsize(httrackp *opt, int argc, char **argv) {
  enum { REFUSE, ACCEPT, WIDTH };

  static const struct {
    size_t used;
    LLint extra;
    size_t slack;
    int want;
  } cases[] = {
      {0, 0, 0, ACCEPT},
      {10, 100, 8192, ACCEPT},
      {(size_t) -2 - 8, 4, 4, ACCEPT}, /* exact fit, no room to spare */
      {(size_t) -2, 0, 0, ACCEPT},     /* largest representable capacity */
      {0, -1, 0, REFUSE},              /* fsize() failure */
      /* -1 already maps to SIZE_MAX; only this exercises the negative guard */
      {0, -4096, 0, REFUSE},
      {(size_t) -1, 1, 0, REFUSE},
      {(size_t) -2, 0, 1, REFUSE},     /* slack alone overruns */
      {(size_t) -1 - 8, 4, 4, REFUSE}, /* total would be the error value */
      {(size_t) -1 - 8, 4, 8, REFUSE},
      {0, HTS_ST_GROWSIZE_OVER32, 8192,
       WIDTH}, /* 32-bit size_t can't hold these */
      {10, HTS_ST_GROWSIZE_OVER32, 8192, WIDTH},
  };

  size_t k;
  int rc = 0;

  (void) opt;
  (void) argc;
  (void) argv;
  for (k = 0; k < sizeof(cases) / sizeof(cases[0]); k++) {
    const size_t used = cases[k].used, slack = cases[k].slack;
    const LLint extra = cases[k].extra;
    const size_t got = llint_grow_size_t(used, extra, slack);
    const hts_boolean refused = got == (size_t) -1 ? HTS_TRUE : HTS_FALSE;
    const hts_boolean exact =
        !refused && extra >= 0 && got - used - slack == (size_t) extra;
    hts_boolean ok;

    switch (cases[k].want) {
    case ACCEPT:
      ok = exact;
      break;
    case REFUSE:
      ok = refused;
      break;
    default:
      ok = sizeof(size_t) >= sizeof(LLint) ? exact : refused;
      break;
    }
    if (!ok) {
      fprintf(stderr,
              "growsize: grow(" LLintP ", " LLintP ", " LLintP ") = " LLintP
              " (want %s)\n",
              (LLint) used, extra, (LLint) slack, (LLint) got,
              cases[k].want == REFUSE ? "refusal" : "exact sum");
      rc = 1;
    }
  }
  printf("growsize self-test %s\n", rc == 0 ? "OK" : "FAILED");
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

static char st_log_callback_seen[256];

static void st_log_callback(httrackp *opt, int type, const char *format,
                            va_list args) {
  (void) opt;
  (void) type;
  (void) vsnprintf(st_log_callback_seen, sizeof(st_log_callback_seen), format,
                   args);
}

/* The callback must not consume the va_list the log file's vfprintf() needs. */
static int st_logcallback(httrackp *opt, int argc, char **argv) {
  static const char want[] = "42 sentinel";
  static const char want_filtered[] = "7 filtered";
  char BIGSTK seen[sizeof(st_log_callback_seen)];
  char BIGSTK line[256];
  FILE *fp;
  int rc = 1;

  (void) argc;
  (void) argv;

  fp = tmpfile();
  if (fp == NULL) {
    fprintf(stderr, "logcallback: tmpfile() failed\n");
    return 1;
  }
  opt->log = fp;
  opt->debug = LOG_NOTICE;
  st_log_callback_seen[0] = '\0';
  hts_set_log_vprint_callback(st_log_callback);
  hts_log_print(opt, LOG_NOTICE, "%d %s", 42, "sentinel");
  hts_set_log_vprint_callback(NULL);
  opt->log = NULL;
  strcpybuff(seen, st_log_callback_seen);

  rewind(fp);
  if (fgets(line, (int) sizeof(line), fp) == NULL) {
    fprintf(stderr, "logcallback: log file is empty, nothing was written\n");
    fclose(fp);
    return 1;
  }
  fclose(fp);

  /* The callback runs above the level filter and without a log file at all;
     the front-ends that install one usually have no opt->log open. */
  st_log_callback_seen[0] = '\0';
  hts_set_log_vprint_callback(st_log_callback);
  hts_log_print(opt, LOG_DEBUG, "%d %s", 7, "filtered");
  hts_set_log_vprint_callback(NULL);

  /* Same arguments both ways; the file line carries a level prefix. */
  if (strcmp(seen, want) != 0)
    fprintf(stderr, "logcallback: callback got '%s' want '%s'\n", seen, want);
  else if (strstr(line, want) == NULL)
    fprintf(stderr, "logcallback: log file got '%s' want it to carry '%s'\n",
            line, want);
  else if (strcmp(st_log_callback_seen, want_filtered) != 0)
    fprintf(stderr, "logcallback: unfiltered callback got '%s' want '%s'\n",
            st_log_callback_seen, want_filtered);
  else
    rc = 0;

  if (rc == 0)
    printf("logcallback self-test OK\n");
  return rc;
}

/* an empty fil started htsAddLink's codebase walk before the buffer (#730) */
static int st_addlink(httrackp *opt, int argc, char **argv) {
  htsmoduleStruct BIGSTK str;
  cache_back cache;
  struct_back *sback;
  hash_struct hash;
  int ptr = 0;
  int i;

  (void) argc;
  (void) argv;

  memset(&cache, 0, sizeof(cache));
  cache.hashtable = (void *) coucal_new(0);
  sback = back_new(opt, opt->maxsoc * 32 + 1024);
  /* same wiring as hts_mirror (htscore.c) */
  hash_init(opt, &hash, opt->urlhack);
  hash.liens = (const lien_url *const *const *) &opt->liens;
  opt->hash = &hash;
  hts_record_init(opt);

  memset(&str, 0, sizeof(str));
  str.opt = opt;
  str.sback = sback;
  str.cache = &cache;
  str.hashptr = &hash;
  str.ptr_ = &ptr;
  str.addLink = htsAddLink;

  /* [0] is the underflow; [1] and [2] are controls that the trim is unchanged.
     A query-only link is the one that notices the trim at all: for the others
     ident_url_relatif() re-derives the directory from the path it is given. */
  for (i = 0; i < 3; i++) {
    static const char *const fil[3] = {"", "/dir/page.html", "/dir/page.html"};
    static const char *const lnk[3] = {"sub/page.html", "sub/page.html",
                                       "?x=1"};
    static const char *const want[3] = {
        "untouched", "http://www.example.com/dir/sub/page.html",
        "http://www.example.com/dir/?x=1"};
    char BIGSTK loc[HTS_URLMAXSIZE * 2];
    char BIGSTK link[HTS_URLMAXSIZE];

    strcpybuff(loc, "untouched");
    strcpybuff(link, lnk[i]);
    str.localLink = loc;
    str.localLinkSize = (int) sizeof(loc);
    if (!hts_record_link(opt, "www.example.com", fil[i], "", "", "", ""))
      return 1;
    ptr = heap_top_index();
    str.url_host = heap(ptr)->adr;
    str.url_file = heap(ptr)->fil;
    assertf(htsAddLink(&str, link) == 0); /* refused by the wizard either way */
    if (strcmp(loc, want[i]) != 0) {
      fprintf(stderr, "addlink[%d]: got '%s' want '%s'\n", i, loc, want[i]);
      return 1;
    }
  }

  printf("addlink self-test OK\n");
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

static int st_cache_hdrbounds(httrackp *opt, int argc, char **argv) {
  int err;

  if (argc < 1) {
    fprintf(stderr, "cache-hdrbounds: needs a directory\n");
    return 1;
  }
  err = cache_header_bounds_selftest(opt, argv[0]);
  printf("cache-hdrbounds: %s\n", err ? "FAIL" : "OK");
  return err;
}

static int st_cache_urlbounds(httrackp *opt, int argc, char **argv) {
  int err;

  if (argc < 1) {
    fprintf(stderr, "cache-urlbounds: needs a directory\n");
    return 1;
  }
  err = cache_url_bounds_selftest(opt, argv[0]);
  printf("cache-urlbounds: %s\n", err ? "FAIL" : "OK");
  return err;
}

static int st_cache_savebounds(httrackp *opt, int argc, char **argv) {
  int err;

  if (argc < 1) {
    fprintf(stderr, "cache-savebounds: needs a directory\n");
    return 1;
  }
  err = cache_savename_bounds_selftest(opt, argv[0]);
  printf("cache-savebounds: %s\n", err ? "FAIL" : "OK");
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
    char line[64]; /* treathead NUL-cuts the header in place: never a literal */

    memset(&r, 0, sizeof(r));
    memset(host, 'a', sizeof(host) - 1);
    host[sizeof(host) - 1] = '\0';
    ck2.max_len = (int) sizeof(ck2.data);
    ck2.data[0] = '\0';
    strcpybuff(line, "Set-Cookie: SID=1; path=/");
    treathead(&ck2, host, "/", &r, line);
    if (strnotempty(ck2.data)) // oversize-host cookie was not dropped
      err = 1;
    /* control: a normal host still yields a cookie through treathead */
    strcpybuff(line, "Set-Cookie: SID=1; path=/");
    treathead(&ck2, dom, "/", &r, line);
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

/* The short form optalias_check() emits for the option words, both joined by a
   space when it returns two, or NULL when it refuses them; *used counts the
   words consumed. */
static const char *st_optalias_expand(char *dest, size_t dest_size,
                                      const char *word, const char *next,
                                      int *used) {
  char BIGSTK out[2][HTS_CDLMAXSIZE];
  char *outv[2] = {out[0], out[1]};
  const char *argv[2];
  char error[256];
  const int argc = next != NULL ? 2 : 1;
  int outc = 0;

  argv[0] = word;
  argv[1] = next;
  out[0][0] = out[1][0] = dest[0] = '\0';
  *used = optalias_check(argc, argv, 0, &outc, outv, sizeof(out[0]), error,
                         sizeof(error));
  if (*used == 0) {
    assertf(error[0] != '\0'); /* a refusal has to say why */
    return NULL;
  }
  assertf(outc >= 1 && outc <= 2);
  strlcpybuff(dest, out[0], dest_size);
  if (outc == 2) {
    strlcatbuff(dest, " ", dest_size);
    strlcatbuff(dest, out[1], dest_size);
  }
  return dest;
}

/* Long-option value handling (#1195): a value the option's class did not take
   was dropped, so --index=0 read back as the enabling bare --index. */
static int st_optalias(httrackp *opt, int argc, char **argv) {
  char got[HTS_CDLMAXSIZE * 2];
  int i, used;

  (void) opt;
  /* -list gives 282 the whole table: the rows to try against the engine's
     own parser, and the class of every name the wizard writes a value to */
  if (argc == 1 && strcmp(argv[0], "-list") == 0) {
    for (i = 0; optalias_value(i)[0] != '\0'; i++)
      printf("%s %s\n", opttype_value(i), optalias_value(i));
    return 0;
  }
  if (argc >= 1) {
    const char *const out = st_optalias_expand(
        got, sizeof(got), argv[0], argc >= 2 ? argv[1] : NULL, &used);

    printf("%s\n", out != NULL ? out : "(refused)");
    return out != NULL ? 0 : 1;
  }
#define EXPANDS(want, word, next)                                              \
  do {                                                                         \
    const char *const out__ =                                                  \
        st_optalias_expand(got, sizeof(got), (word), (next), &used);           \
    assertf(out__ != NULL && strcmp(out__, (want)) == 0);                      \
  } while (0)
#define REFUSES(word, next)                                                    \
  assertf(st_optalias_expand(got, sizeof(got), (word), (next), &used) == NULL)

  /* -I0 has always disabled the index; now the long form can say it too */
  EXPANDS("-I0", "--index=0", NULL);
  EXPANDS("-I0", "--index=off", NULL);
  EXPANDS("-I0", "--noindex", NULL);
  EXPANDS("-I", "--index=1", NULL);
  EXPANDS("-I", "--index=on", NULL);
  /* detached, as a config file writes it ("index off"), and never a URL */
  EXPANDS("-I0", "--index", "off");
  assertf(used == 2);
  EXPANDS("-I", "--index", "http://foo/");
  assertf(used == 1);
  EXPANDS("-I", "--index", NULL);
  assertf(used == 1);
  /* -I takes 0 alone, so an out-of-range value is an error, not a bare -I */
  REFUSES("--index=2", NULL);
  REFUSES("--index=yes", NULL);

  /* a numeric level reaches the short form whole */
  EXPANDS("-%I0", "--search-index=0", NULL);
  EXPANDS("-%I2", "--search-index=2", NULL);
  EXPANDS("-%v2", "--display=2", NULL);
  EXPANDS("-%N0", "--delayed-type-check=0", NULL);
  EXPANDS("-o0", "--generate-errors=0", NULL);
  REFUSES("--display=full", NULL);
  REFUSES("--display=99999999999999999999", NULL);

  /* three rows were "param", a class that demands a following token: the URL
     after --purge-old became its value, and =1 died on "invalid option 1" */
  EXPANDS("-X", "--purge-old", NULL);
  EXPANDS("-X", "--purge-old", "http://foo/");
  assertf(used == 1);
  EXPANDS("-X0", "--purge-old=0", NULL);
  EXPANDS("-X", "--purge-old=1", NULL);
  EXPANDS("-X0", "--purge-old", "0");
  assertf(used == 2);
  EXPANDS("-X", "--purge-old", "1");
  EXPANDS("-X0", "--nopurge-old", NULL);
  EXPANDS("-%f0", "--httpproxy-ftp=0", NULL);
  EXPANDS("-%f", "--httpproxy-ftp=1", NULL);
  EXPANDS("-%P0", "--extended-parsing=0", NULL);
  EXPANDS("-%P", "--extended-parsing=1", NULL);

  /* a flag that takes no value refuses one rather than dropping it */
  REFUSES("--mirror=0", NULL);
  REFUSES("--nomirror", NULL);
  REFUSES("--warc=0", NULL);
  REFUSES("--single-file=s", NULL);
  /* ...and so does a compound alias, whose value would land in the cluster */
  REFUSES("--spider=0", NULL);

  /* an alias whose own name begins with "no" is not a negation */
  EXPANDS("-%T0", "--no-utf8-conversion", NULL);
  EXPANDS("-y0", "--no-background-on-suspend", NULL);
  EXPANDS("-%T", "--utf8-conversion", NULL);

  /* the value-taking classes are untouched */
  EXPANDS("-C0", "--cache=0", NULL);
  EXPANDS("-C0", "--nocache", NULL);
  EXPANDS("-C2", "--cache", "2");
  EXPANDS("-P proxy:8080", "--proxy", "proxy:8080");
  EXPANDS("+*.gif", "--allow", "*.gif");

  /* invariant: every name's bare long form still emits its short form, and a
     param name still demands its own value. A duplicate name (test, continue)
     resolves to its first row */
  for (i = 0; optalias_value(i)[0] != '\0'; i++) {
    const int p = optalias_find(optalias_value(i));
    char word[HTS_CDLMAXSIZE];

    assertf(p >= 0);
    snprintf(word, sizeof(word), "--%s", optalias_value(i));
    if (strncmp(opttype_value(p), "param", 5) == 0) {
      REFUSES(word, NULL);
    } else {
      EXPANDS(optreal_value(p), word, NULL);
      assertf(used == 1);
    }
  }
  assertf(i > 100); /* the table was walked, not skipped */
#undef EXPANDS
#undef REFUSES

  printf("optalias self-test OK\n");
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

/* --host-alias (#16): rule resolution, and the dedup key it feeds. */
static int st_hostalias(httrackp *opt, int argc, char **argv) {
  char dest[HTS_URLMAXSIZE * 2];
  /* two rules, three aliases of a.com */
  static const char *const rules = "b.com=a.com\nc.com,d.com=a.com";

  (void) argc;
  (void) argv;

  /* nothing declared, or nothing matching: the host is left alone */
  assertf(hts_host_alias(NULL, "b.com", HTS_TRUE, dest, sizeof(dest)) == NULL);
  assertf(hts_host_alias("", "b.com", HTS_TRUE, dest, sizeof(dest)) == NULL);
  assertf(hts_host_alias(rules, "other.com", HTS_TRUE, dest, sizeof(dest)) ==
          NULL);
  /* a rule mapping a host to itself is a no-op, not a rewrite */
  assertf(hts_host_alias("a.com=a.com", "a.com", HTS_TRUE, dest,
                         sizeof(dest)) == NULL);

  /* exact, comma-list and glob aliases */
  assertf(strcmp(hts_host_alias(rules, "b.com", HTS_TRUE, dest, sizeof(dest)),
                 "a.com") == 0);
  assertf(strcmp(hts_host_alias(rules, "d.com", HTS_TRUE, dest, sizeof(dest)),
                 "a.com") == 0);
  assertf(strcmp(hts_host_alias("*.cdn.b.com=a.com", "img.cdn.b.com", HTS_TRUE,
                                dest, sizeof(dest)),
                 "a.com") == 0);
  /* hosts compare case-insensitively, like the filters */
  assertf(strcmp(hts_host_alias(rules, "B.CoM", HTS_TRUE, dest, sizeof(dest)),
                 "a.com") == 0);
  /* the port is part of the matched host */
  assertf(strcmp(hts_host_alias("b.com:8080=a.com:80", "b.com:8080", HTS_TRUE,
                                dest, sizeof(dest)),
                 "a.com:80") == 0);
  assertf(hts_host_alias("b.com:8080=a.com", "b.com", HTS_TRUE, dest,
                         sizeof(dest)) == NULL);
  /* last match wins */
  assertf(strcmp(hts_host_alias("b.com=a.com\nb.com=z.com", "b.com", HTS_TRUE,
                                dest, sizeof(dest)),
                 "z.com") == 0);
  /* Under the www. collapse a rule on the bare host covers its www. forms,
     which -%u would otherwise key onto the bare name and away from the
     canonical one. With the collapse off, the host matches as the link
     spells it: --no-www-dedup keeps www.b.com and b.com apart. */
  assertf(
      strcmp(hts_host_alias(rules, "www.b.com", HTS_TRUE, dest, sizeof(dest)),
             "a.com") == 0);
  assertf(strcmp(hts_host_alias(rules, "www-42.b.com", HTS_TRUE, dest,
                                sizeof(dest)),
                 "a.com") == 0);
  assertf(hts_host_alias(rules, "www.b.com", HTS_FALSE, dest, sizeof(dest)) ==
          NULL);
  assertf(strcmp(hts_host_alias(rules, "b.com", HTS_FALSE, dest, sizeof(dest)),
                 "a.com") == 0);
  /* Under the collapse a rule may spell either form of its alias and both names
     fold, so naming www.b.com cannot split the pair -%u had merged. */
  assertf(strcmp(hts_host_alias("www.b.com=a.com", "www.b.com", HTS_TRUE, dest,
                                sizeof(dest)),
                 "a.com") == 0);
  assertf(strcmp(hts_host_alias("www.b.com=a.com", "b.com", HTS_TRUE, dest,
                                sizeof(dest)),
                 "a.com") == 0);
  /* with the collapse off the two names are distinct, and so are the rules */
  assertf(strcmp(hts_host_alias("www.b.com=a.com", "www.b.com", HTS_FALSE, dest,
                                sizeof(dest)),
                 "a.com") == 0);
  assertf(hts_host_alias("www.b.com=a.com", "b.com", HTS_FALSE, dest,
                         sizeof(dest)) == NULL);

  /* scheme and credentials survive, only the host is replaced */
  assertf(strcmp(hts_host_alias(rules, "https://b.com", HTS_TRUE, dest,
                                sizeof(dest)),
                 "https://a.com") == 0);
  assertf(strcmp(hts_host_alias(rules, "http://user:pw@b.com", HTS_TRUE, dest,
                                sizeof(dest)),
                 "http://user:pw@a.com") == 0);

  assertf(
      strcmp(hts_host_alias(rules, "ftp://b.com", HTS_TRUE, dest, sizeof(dest)),
             "ftp://a.com") == 0);
  assertf(
      strcmp(hts_host_alias("b.com:8080=a.com:80", "https://user:pw@b.com:8080",
                            HTS_TRUE, dest, sizeof(dest)),
             "https://user:pw@a.com:80") == 0);
  /* An alias-side scheme narrows the match to it; a canonical-side scheme
     replaces the link's, for a host that speaks one scheme only. */
  assertf(strcmp(hts_host_alias("b.com=https://a.com", "http://b.com", HTS_TRUE,
                                dest, sizeof(dest)),
                 "https://a.com") == 0);
  assertf(strcmp(hts_host_alias("https://www.foo.com=ftp://ftp.foo.com",
                                "https://www.foo.com", HTS_TRUE, dest,
                                sizeof(dest)),
                 "ftp://ftp.foo.com") == 0);
  assertf(hts_host_alias("https://www.foo.com=ftp://ftp.foo.com",
                         "http://www.foo.com", HTS_TRUE, dest,
                         sizeof(dest)) == NULL);
  /* a rule without a scheme still matches whichever scheme the link uses */
  assertf(
      strcmp(hts_host_alias(rules, "ftp://b.com", HTS_TRUE, dest, sizeof(dest)),
             "ftp://a.com") == 0);
  /* credentials are the link's, whichever side names the scheme */
  assertf(strcmp(hts_host_alias("b.com=ftp://a.com", "http://user:pw@b.com",
                                HTS_TRUE, dest, sizeof(dest)),
                 "ftp://user:pw@a.com") == 0);
  /* A plain-http address carries no scheme, so http:// has to be spelled out
     for the rule to reach it: naming the default scheme must not be dead. */
  assertf(strcmp(hts_host_alias("http://b.com=a.com", "b.com", HTS_TRUE, dest,
                                sizeof(dest)),
                 "a.com") == 0);
  assertf(hts_host_alias("http://b.com=a.com", "https://b.com", HTS_TRUE, dest,
                         sizeof(dest)) == NULL);
  /* a scheme named mid-chain stays in effect: the hop after it names none, and
     dropping back to the link's would undo the reachability the rule bought */
  assertf(strcmp(hts_host_alias("a.com=https://b.com\nb.com=c.com",
                                "http://a.com", HTS_TRUE, dest, sizeof(dest)),
                 "https://c.com") == 0);
  /* the spaces a user leaves around a token are not part of the host */
  assertf(hts_host_alias_rule_ok("b.com = a.com"));
  assertf(strcmp(hts_host_alias("b.com = a.com", "http://b.com", HTS_TRUE, dest,
                                sizeof(dest)),
                 "http://a.com") == 0);
  assertf(strcmp(hts_host_alias("b.com\t=\ta.com", "http://b.com", HTS_TRUE,
                                dest, sizeof(dest)),
                 "http://a.com") == 0);
  /* a pattern naming no scheme is matched against the bare host, not the URL */
  assertf(hts_host_alias("http*=x.com", "http://b.com", HTS_TRUE, dest,
                         sizeof(dest)) == NULL);
  /* the "//host" form names a host, and a glob may stand where a scheme goes */
  assertf(strcmp(hts_host_alias("//b.com=a.com", "http://b.com", HTS_TRUE, dest,
                                sizeof(dest)),
                 "http://a.com") == 0);
  assertf(hts_host_alias_rule_ok("*://b.com=a.com"));
  assertf(strcmp(hts_host_alias("*://b.com=a.com", "ftp://b.com", HTS_TRUE,
                                dest, sizeof(dest)),
                 "ftp://a.com") == 0);
  /* an IPv6 literal is a host; a scheme with none behind it is not, nor is the
     engine's own pseudo-host */
  assertf(hts_host_alias_rule_ok("b.com=[::1]"));
  assertf(hts_host_alias_rule_ok("b.com=[::1]:8080"));
  assertf(!hts_host_alias_rule_ok("b.com=https://"));
  assertf(!hts_host_alias_rule_ok("b.com=///"));
  assertf(!hts_host_alias_rule_ok("b.com=primary"));
  assertf(hts_host_alias("b.com=///", "http://b.com", HTS_TRUE, dest,
                         sizeof(dest)) == NULL);
  /* a run of trailing slashes is stripped by both the check and the fold */
  assertf(hts_host_alias_rule_ok("b.com//=a.com//"));
  /* a canonical whose trailing slash is trimmed must still settle: the chain
     compares one match against the next, and an untrimmed one never equals it
   */
  assertf(strcmp(hts_host_alias("*=a.com/", "http://b.com", HTS_TRUE, dest,
                                sizeof(dest)),
                 "http://a.com") == 0);
  /* Credentials belong to the link. The command line refuses a rule carrying
     its own, and the fold drops them, since a caller can set the rules without
     going through that check: kept, they would be prepended to the link's again
     on every re-fold, growing the address until it no longer fits. */
  assertf(!hts_host_alias_rule_ok("a.com=user:pw@a.com"));
  assertf(strcmp(hts_host_alias("*=u:p@b.com", "z.com", HTS_TRUE, dest,
                                sizeof(dest)),
                 "b.com") == 0);
  assertf(hts_host_alias("*=u:p@b.com", "b.com", HTS_TRUE, dest,
                         sizeof(dest)) == NULL);
  assertf(strcmp(hts_host_alias("*=u:p@b.com", "http://user:pw@z.com", HTS_TRUE,
                                dest, sizeof(dest)),
                 "http://user:pw@b.com") == 0);
  /* a trailing slash is not a path */
  assertf(strcmp(hts_host_alias("b.com/=https://a.com/", "http://b.com",
                                HTS_TRUE, dest, sizeof(dest)),
                 "https://a.com") == 0);
  /* every scheme of one host is one site to the scope test */
  assertf(hts_host_same_alias(rules, "ftp://b.com", "https://a.com", HTS_TRUE));

  /* engine pseudo-hosts survive a catch-all rule */
  assertf(hts_host_alias("*=a.com", "primary", HTS_TRUE, dest, sizeof(dest)) ==
          NULL);
  assertf(hts_host_alias("*=a.com", "file://", HTS_TRUE, dest, sizeof(dest)) ==
          NULL);
  assertf(hts_host_alias("*=a.com", "", HTS_TRUE, dest, sizeof(dest)) == NULL);
  /* control: the catch-all does rewrite a real host */
  assertf(
      strcmp(hts_host_alias("*=a.com", "b.com", HTS_TRUE, dest, sizeof(dest)),
             "a.com") == 0);

  /* Chains resolve to the end of the chain, and the result is a fixpoint:
     url_savename names the same link again from the host it folded last, so a
     one-hop mapping would key that link under an intermediate host. */
  {
    static const char *const chain = "a.com=b.com\nb.com=c.com";
    /* the www form of a canonical is a chain that does not look like one */
    static const char *const wwwchain = "b.com=www.c.com\nc.com=d.com";
    char again[HTS_URLMAXSIZE * 2], looping[HTS_URLMAXSIZE * 2];

    assertf(strcmp(hts_host_alias(chain, "a.com", HTS_TRUE, dest, sizeof(dest)),
                   "c.com") == 0);
    assertf(hts_host_alias(chain, dest, HTS_TRUE, again, sizeof(again)) ==
            NULL);
    assertf(
        strcmp(hts_host_alias(wwwchain, "b.com", HTS_TRUE, dest, sizeof(dest)),
               "d.com") == 0);
    assertf(hts_host_alias(wwwchain, dest, HTS_TRUE, again, sizeof(again)) ==
            NULL);
    assertf(hts_host_alias_looping(chain, HTS_TRUE, looping, sizeof(looping)) ==
            NULL);
    assertf(hts_host_alias_looping(rules, HTS_TRUE, looping, sizeof(looping)) ==
            NULL);

    /* rules pointing in a circle name no canonical host: leave them alone */
    {
      static const char *const loop = "a.com=b.com\nb.com=a.com";

      assertf(hts_host_alias(loop, "a.com", HTS_TRUE, dest, sizeof(dest)) ==
              NULL);
      assertf(hts_host_alias(loop, "b.com", HTS_TRUE, dest, sizeof(dest)) ==
              NULL);
      assertf(strcmp(hts_host_alias_looping(loop, HTS_TRUE, looping,
                                            sizeof(looping)),
                     "b.com") == 0);
    }
  }

  /* alias lists tolerate the spaces a user writes after the commas */
  assertf(strcmp(hts_host_alias("b.com, c.com =a.com", "c.com", HTS_TRUE, dest,
                                sizeof(dest)),
                 "a.com") == 0);

  /* a canonical host that would not fit leaves the URL alone: never a
     truncated (and therefore wrong) hostname */
  {
    char small[5]; /* "a.com" needs 6 with its terminator */

    assertf(hts_host_alias(rules, "b.com", HTS_TRUE, small, sizeof(small)) ==
            NULL);
    assertf(hts_host_alias(rules, "https://b.com", HTS_TRUE, small,
                           sizeof(small)) == NULL);
    /* control: one more byte and it fits */
    assertf(strcmp(hts_host_alias(rules, "b.com", HTS_TRUE, dest, 6),
                   "a.com") == 0);
  }

  /* malformed rules are ignored... */
  assertf(hts_host_alias("b.com", "b.com", HTS_TRUE, dest, sizeof(dest)) ==
          NULL);
  assertf(hts_host_alias("=a.com", "b.com", HTS_TRUE, dest, sizeof(dest)) ==
          NULL);
  assertf(hts_host_alias("b.com=", "b.com", HTS_TRUE, dest, sizeof(dest)) ==
          NULL);
  /* ...and the command line refuses them up front */
  assertf(hts_host_alias_rule_ok("b.com,c.com=a.com"));
  assertf(hts_host_alias_rule_ok("*.b.com=a.com:8080"));
  assertf(!hts_host_alias_rule_ok(NULL));
  assertf(!hts_host_alias_rule_ok("b.com"));
  assertf(!hts_host_alias_rule_ok("=a.com"));
  assertf(!hts_host_alias_rule_ok("b.com="));
  assertf(!hts_host_alias_rule_ok("b.com=a.com,z.com"));
  assertf(!hts_host_alias_rule_ok("b.com=*.a.com"));
  assertf(!hts_host_alias_rule_ok("b.com=a com"));
  assertf(!hts_host_alias_rule_ok("b.com=a.com#x"));
  /* a scheme is part of an address, a path is not */
  assertf(hts_host_alias_rule_ok("https://www.foo.com=ftp://ftp.foo.com"));
  assertf(hts_host_alias_rule_ok("https://www.foo.com/=ftp://ftp.foo.com/"));
  assertf(
      !hts_host_alias_rule_ok("https://www.foo.com/=ftp://ftp.foo.com/pub/"));
  assertf(!hts_host_alias_rule_ok("https://www.foo.com/a=ftp://ftp.foo.com"));
  assertf(!hts_host_alias_rule_ok("a.com,b.com/deep=c.com"));
  assertf(!hts_host_alias_rule_ok("b.com=http://a.com/x"));
  /* a control byte, or a scheme the fold cannot parse, lands in the host */
  assertf(!hts_host_alias_rule_ok("b.com=a.com\nc.com"));
  assertf(!hts_host_alias_rule_ok("b.com\nc.com=a.com"));
  assertf(!hts_host_alias_rule_ok("b.com=a.com\n"));
  assertf(!hts_host_alias_rule_ok("\nb.com=a.com"));
  assertf(!hts_host_alias_rule_ok("b.com=a.com\r"));
  assertf(!hts_host_alias_rule_ok("b.com=a.com\v"));
  assertf(!hts_host_alias_rule_ok("b.com=a.com\f"));
  assertf(!hts_host_alias_rule_ok("b.com=a\x7f.com"));
  assertf(!hts_host_alias_rule_ok("b\v.com=a.com"));
  /* a high byte is a host in some local charset, not a control byte */
  assertf(hts_host_alias_rule_ok("caf\xe9.example.com=h\xf4tel.example.com"));
  /* an alias may lead with '-': why case 'C' carries no dash guard (#1179) */
  assertf(hts_host_alias_rule_ok("-legacy.example.com=example.com"));
  assertf(!hts_host_alias_rule_ok("b.com=x://a.com"));
  assertf(!hts_host_alias_rule_ok("b.com=a.com://c.com"));
  assertf(hts_host_alias_rule_ok("b.com=ftp://a.com"));
  assertf(hts_host_alias_rule_ok("*://b.com=a.com"));

  /* the same-address test the wizard uses to decide scope */
  assertf(hts_host_same_alias(rules, "b.com", "a.com", HTS_TRUE));
  assertf(
      hts_host_same_alias(rules, "https://b.com", "http://a.com", HTS_TRUE));
  assertf(hts_host_same_alias(rules, "b.com", "d.com",
                              HTS_TRUE)); /* two aliases of one */
  assertf(!hts_host_same_alias(rules, "b.com", "other.com", HTS_TRUE));
  assertf(!hts_host_same_alias(rules, "other.com", "elsewhere.com", HTS_TRUE));
  /* no rules: the caller's own exact compare decides, not this one */
  assertf(!hts_host_same_alias(NULL, "b.com", "b.com", HTS_TRUE));

  StringCopy(opt->host_alias, rules);

  /* the in-place fold every link goes through before it is probed and fetched
   */
  {
    lien_adrfil af;

    memset(&af, 0, sizeof(af));
    strcpybuff(af.adr, "https://b.com");
    strcpybuff(af.fil, "/x");
    assertf(strcmp(hts_host_alias_fold(opt, &af), "https://a.com") == 0);
    assertf(strcmp(af.adr, "https://a.com") == 0); /* rewritten in place */
    assertf(strcmp(af.fil, "/x") == 0);            /* the path is not touched */
    /* idempotent: the delayed-type path names the same link again */
    assertf(strcmp(hts_host_alias_fold(opt, &af), "https://a.com") == 0);
    strcpybuff(af.adr, "other.com");
    assertf(strcmp(hts_host_alias_fold(opt, &af), "other.com") == 0);
  }

  /* The dedup key itself: aliases collapse whatever the url hacks say, so
     --host-alias never silently no-ops under -%u0. */
#define EQ(aa, fa, ab, fb) hash_url_equals(opt, aa, fa, ab, fb)
  opt->urlhack = HTS_TRUE;
  opt->no_www_dedup = opt->no_slash_dedup = opt->no_query_dedup = HTS_FALSE;
  assertf(EQ("b.com", "/x", "a.com", "/x"));
  assertf(EQ("b.com", "/x", "d.com", "/x"));
  assertf(EQ("www.b.com", "/x", "a.com", "/x"));
  assertf(!EQ("b.com", "/x", "a.com", "/y"));
  assertf(!EQ("other.com", "/x", "a.com", "/x"));
  /* with the url hacks off the alias still fires, but it stops covering the
     www. forms: -%u0 asked for those to stay distinct */
  opt->urlhack = HTS_FALSE;
  assertf(EQ("b.com", "/x", "a.com", "/x"));
  assertf(!EQ("www.b.com", "/x", "a.com", "/x"));
  assertf(!EQ("other.com", "/x", "a.com", "/x"));
  opt->urlhack = HTS_TRUE;
  opt->no_www_dedup = HTS_TRUE;
  assertf(EQ("b.com", "/x", "a.com", "/x"));
  assertf(!EQ("www.b.com", "/x", "a.com", "/x"));
  opt->no_www_dedup = HTS_FALSE;
#undef EQ
  StringCopy(opt->host_alias, "");

  printf("host-alias self-test OK\n");
  return 0;
}

/* Build the key for ADR/FIL into ARENA, whose KEYSIZE bytes are followed by
   GUARD poisoned ones, and report whether those are still untouched. */
static hts_boolean st_hashkey(httrackp *opt, const char *adr, const char *fil,
                              char *arena, size_t keysize, size_t guard) {
  hash_struct hash;
  size_t i;

  /* poisoned with '#', not 0, or the stray NUL of an off-by-one would read as
     untouched */
  memset(arena, '#', keysize + guard);
  hash_init(opt, &hash, opt->urlhack);
  hash_url_key(&hash, adr, fil, arena, keysize);
  hash_free(&hash);
  for (i = keysize; i < keysize + guard; i++) {
    if (arena[i] != '#')
      return HTS_FALSE;
  }
  return HTS_TRUE;
}

/* The dedup key is a host and a path built in one buffer: two halves at the
   engine's per-field maximum must fit whole, and either half alone must clip
   rather than run past the end (#1160). */
static int st_hashkey_bounds(httrackp *opt, int argc, char **argv) {
  /* what lien_adrfil.adr / .fil hold, and what a whole key needs */
  enum { field = HTS_URLMAXSIZE * 2, keysize = 2 * field, guard = 64 };

  char *adr = malloct(2 * keysize);
  char *fil = malloct(2 * keysize);
  char *cat = malloct(4 * keysize);
  char *arena = malloct(keysize + guard);
  int hack;

  (void) argc;
  (void) argv;
  assertf(adr != NULL && fil != NULL && cat != NULL && arena != NULL);
  opt->no_www_dedup = opt->no_slash_dedup = opt->no_query_dedup = HTS_FALSE;
  /* the crawler's own key buffer, or every maximal URL would collide */
  assertf(sizeof(((hash_struct *) NULL)->normfil) >= (size_t) keysize);

  /* a host and a path one byte short of their own buffers: nothing here
     normalizes away, so the key is the plain concatenation */
  memset(adr, 'a', field - 1);
  adr[field - 1] = '\0';
  fil[0] = '/';
  memset(fil + 1, 'b', field - 2);
  fil[field - 1] = '\0';
  snprintf(cat, 4 * keysize, "%s%s", adr, fil);
  for (hack = 0; hack < 2; hack++) {
    opt->urlhack = hack != 0 ? HTS_TRUE : HTS_FALSE;
    assertf(st_hashkey(opt, adr, fil, arena, keysize, guard));
    assertf(strcmp(arena, cat) == 0);
  }

  /* either half alone past the whole key: each copy is bounded on its own */
  for (hack = 0; hack < 4; hack++) {
    const hts_boolean longadr = (hack & 1) != 0 ? HTS_TRUE : HTS_FALSE;

    memset(adr, 'a', longadr ? keysize + 32 : 8);
    adr[longadr ? keysize + 32 : 8] = '\0';
    fil[0] = '/';
    memset(fil + 1, 'b', longadr ? 8 : keysize + 32);
    fil[(longadr ? 8 : keysize + 32) + 1] = '\0';
    snprintf(cat, 4 * keysize, "%s%s", adr, fil);
    opt->urlhack = (hack & 2) != 0 ? HTS_TRUE : HTS_FALSE;
    assertf(st_hashkey(opt, adr, fil, arena, keysize, guard));
    /* clipped and terminated, and a prefix of the pair rather than garbage */
    assertf(strlen(arena) == keysize - 1);
    assertf(strncmp(arena, cat, strlen(arena)) == 0);
  }

  /* --strip-query builds its match string from the same two halves, and used
     to abort on a maximal pair. Past STRJOKER_MAXLEN no rule can match, so a
     clipped string would answer where the whole one must stay silent. */
  {
    const char *const rules = "*=other\n*bbz=sid";
    char keys[64];
    const char *k;

    /* control: the rules do match a short URL */
    k = hts_query_strip_keys(rules, "h.com", "/bbz", keys, sizeof(keys));
    assertf(k != NULL && strcmp(k, "sid") == 0);
    memset(adr, 'a', field - 1);
    adr[field - 1] = '\0';
    fil[0] = '/';
    memset(fil + 1, 'b', field - 3);
    fil[field - 2] = 'z';
    fil[field - 1] = '\0';
    assertf(hts_query_strip_keys(rules, adr, fil, keys, sizeof(keys)) == NULL);
    opt->urlhack = HTS_TRUE; /* and the key builder survives the same pair */
    StringCopy(opt->strip_query, rules);
    assertf(st_hashkey(opt, adr, fil, arena, keysize, guard));

    /* a matching rule plus a query longer than the normalizer's own scratch:
       the key builder must clip instead of handing it over */
    snprintf(fil, 2 * keysize, "/bbz?");
    memset(&fil[5], 'q', field + 64); /* past the normalizer, inside the key */
    fil[5 + field + 64] = '\0';
    assertf(st_hashkey(opt, "h.com", fil, arena, keysize, guard));
    StringCopy(opt->strip_query, "");
  }

  freet(adr);
  freet(fil);
  freet(cat);
  freet(arena);
  printf("hashkey-bounds self-test OK\n");
  return 0;
}

/* Prints the filter answer <n> emits for (adr, fil) [up] in [slot]; with no
   arguments, asserts every answer against its expected pattern (#1119). */
static int st_wizardfilter(httrackp *opt, int argc, char **argv) {
  char pattern[HTS_URLMAXSIZE * 2];
  htsbuff f = htsbuff_array(pattern);

  (void) opt;
  if (argc >= 3) {
    hts_wizard_answer_filter(
        &f, argc >= 5 ? atoi(argv[4]) : 0, atoi(argv[0]), argv[1], argv[2],
        argc >= 4 && atoi(argv[3]) != 0 ? HTS_TRUE : HTS_FALSE);
    printf("%s\n", pattern);
    return 0;
  }
#define EMITS_SLOT(slot, n, adr, fil, up, expect)                              \
  do {                                                                         \
    hts_wizard_answer_filter(&f, (slot), (n), (adr), (fil), (up));             \
    assertf(strcmp(pattern, (expect)) == 0);                                   \
  } while (0)
#define EMITS(n, adr, fil, up, expect)                                         \
  EMITS_SLOT(0, (n), (adr), (fil), (up), (expect))

  /* the host-wide answers: 2 forbids, 5 (allowed to go up) and 6 authorize */
  EMITS(2, "foo.com", "/index.html", HTS_FALSE, "-foo.com/*");
  EMITS(5, "foo.com", "/dir/page.html", HTS_TRUE, "+foo.com/*");
  EMITS(6, "foo.com", "/dir/page.html", HTS_FALSE, "+foo.com/*");
  /* the port is part of the host, the credentials are not */
  EMITS(2, "foo.com:8080", "/x", HTS_FALSE, "-foo.com:8080/*");
  EMITS(2, "user:pass@foo.com", "/x", HTS_FALSE, "-foo.com/*");
  EMITS(5, "foo.com:8080", "/x", HTS_TRUE, "+foo.com:8080/*");
  EMITS(5, "user:pass@foo.com", "/x", HTS_TRUE, "+foo.com/*");
  EMITS(6, "foo.com:8080", "/x", HTS_FALSE, "+foo.com:8080/*");
  EMITS(6, "user:pass@foo.com", "/x", HTS_FALSE, "+foo.com/*");

  /* the trailing slash is what keeps a longer host out */
  assertf(strjoker("foo.com/index.html", pattern + 1, NULL, NULL) != NULL);
  assertf(strjoker("foo.com/", pattern + 1, NULL, NULL) != NULL);
  assertf(strjoker("foo.com.evil.org/x", pattern + 1, NULL, NULL) == NULL);
  assertf(strjoker("foo.com:8080/x", pattern + 1, NULL, NULL) == NULL);

  /* the link and directory answers */
  EMITS(0, "foo.com", "/dir/page.html", HTS_FALSE, "-foo.com/dir/page.html");
  EMITS(0, "foo.com", "index.html", HTS_FALSE, "-foo.com/index.html");
  /* #1251: an answer we could not read records that same default, separator
     included */
  EMITS(-999, "foo.com", "/dir/page.html", HTS_FALSE, "-foo.com/dir/page.html");
  EMITS(-999, "foo.com", "index.html", HTS_FALSE, "-foo.com/index.html");
  EMITS(1, "foo.com", "/dir/page.html", HTS_FALSE, "-foo.com/dir/*");
  EMITS(1, "foo.com", "/page.html", HTS_FALSE, "-foo.com/*");
  EMITS(5, "foo.com", "/dir/page.html", HTS_FALSE, "+foo.com/dir/*");
  EMITS(7, "foo.com", "/dir/page.html", HTS_FALSE, "+foo.com/dir/*[file]");
  /* answer 1 collapses a doubled trailing slash, answers 5 and 7 keep it */
  EMITS(1, "foo.com", "/dir//page.html", HTS_FALSE, "-foo.com/dir/*");
  EMITS(5, "foo.com", "/dir//page.html", HTS_FALSE, "+foo.com/dir//*");
  EMITS(7, "foo.com", "/dir//page.html", HTS_FALSE, "+foo.com/dir//*[file]");
  /* no directory to anchor on, so answers 1, 5 and 7 emit nothing */
  EMITS(1, "foo.com", "page.html", HTS_FALSE, "");
  EMITS(5, "foo.com", "page.html", HTS_FALSE, "");
  EMITS(7, "foo.com", "page.html", HTS_FALSE, "");

  /* the answers that add no filter at all */
  EMITS(-1, "foo.com", "/x", HTS_FALSE, "");
  EMITS(3, "foo.com", "/x", HTS_FALSE, "");
  EMITS(4, "foo.com", "/x", HTS_FALSE, "");
  EMITS(50, "foo.com", "/x", HTS_FALSE, "");
  /* only slot 0 is ever filled outside the host-scope answers */
  EMITS_SLOT(1, 2, "foo.com", "/x", HTS_FALSE, "");
  EMITS_SLOT(1, 6, "foo.com", "/x", HTS_FALSE, "");

  /* the host-scope answers (#1117): both slots, the starred one missing the
     apex is why the second exists */
#define SCOPE_IN HTS_WIZARD_SCOPE_INCLUDE
#define SCOPE_EX HTS_WIZARD_SCOPE_EXCLUDE
  EMITS_SLOT(0, SCOPE_IN, "www.example.co.uk", "/x", HTS_FALSE,
             "+*.www.example.co.uk/*");
  EMITS_SLOT(1, SCOPE_IN, "www.example.co.uk", "/x", HTS_FALSE,
             "+www.example.co.uk/*");
  EMITS_SLOT(0, SCOPE_IN + 1, "www.example.co.uk", "/x", HTS_FALSE,
             "+*.example.co.uk/*");
  EMITS_SLOT(1, SCOPE_IN + 1, "www.example.co.uk", "/x", HTS_FALSE,
             "+example.co.uk/*");
  EMITS_SLOT(0, SCOPE_EX + 1, "www.example.co.uk", "/x", HTS_FALSE,
             "-*.example.co.uk/*");
  EMITS_SLOT(1, SCOPE_EX + 1, "www.example.co.uk", "/x", HTS_FALSE,
             "-example.co.uk/*");
  /* the port rides along, the credentials do not */
  EMITS_SLOT(0, SCOPE_IN + 1, "www.foo.com:8080", "/x", HTS_FALSE,
             "+*.foo.com:8080/*");
  EMITS_SLOT(1, SCOPE_IN, "user:pass@www.foo.com", "/x", HTS_FALSE,
             "+www.foo.com/*");
  /* #1251: a host with no domain below takes the answer on the host itself */
  EMITS_SLOT(0, SCOPE_IN + 2, "www.foo.com", "/x", HTS_FALSE, "+www.foo.com/*");
  EMITS_SLOT(1, SCOPE_IN + 2, "www.foo.com", "/x", HTS_FALSE, "");
  EMITS_SLOT(0, SCOPE_IN, "127.0.0.1:8080", "/x", HTS_FALSE,
             "+127.0.0.1:8080/*");
  EMITS_SLOT(0, SCOPE_IN, "user:pass@127.0.0.1", "/x", HTS_FALSE,
             "+127.0.0.1/*");
  EMITS_SLOT(1, SCOPE_IN, "127.0.0.1:8080", "/x", HTS_FALSE, "");
  EMITS_SLOT(0, SCOPE_EX, "localhost", "/x", HTS_FALSE, "-localhost/*");
  EMITS_SLOT(0, SCOPE_EX, "[::1]", "/x", HTS_FALSE, "-[::1]/*");
  /* slot 2 is past the pair either way */
  EMITS_SLOT(2, SCOPE_IN, "www.foo.com", "/x", HTS_FALSE, "");
  EMITS_SLOT(2, SCOPE_IN, "127.0.0.1", "/x", HTS_FALSE, "");

  /* what the pair must and must not catch */
  EMITS_SLOT(0, SCOPE_IN + 1, "www.example.co.uk", "/x", HTS_FALSE,
             "+*.example.co.uk/*");
  assertf(strjoker("a.b.example.co.uk/x", pattern + 1, NULL, NULL) != NULL);
  assertf(strjoker("example.co.uk/x", pattern + 1, NULL, NULL) == NULL);
  assertf(strjoker("notexample.co.uk/x", pattern + 1, NULL, NULL) == NULL);
  assertf(strjoker("example.co.uk.evil.com/x", pattern + 1, NULL, NULL) ==
          NULL);
  EMITS_SLOT(1, SCOPE_IN + 1, "www.example.co.uk", "/x", HTS_FALSE,
             "+example.co.uk/*");
  assertf(strjoker("example.co.uk/x", pattern + 1, NULL, NULL) != NULL);
  assertf(strjoker("notexample.co.uk/x", pattern + 1, NULL, NULL) == NULL);
#undef SCOPE_IN
#undef SCOPE_EX
#undef EMITS_SLOT
#undef EMITS
  printf("wizardfilter self-test OK\n");
  return 0;
}

/* Prints the domain scopes offered for <question>; with no argument, asserts
   the enumeration (#1117). */
static int st_wizardscope(httrackp *opt, int argc, char **argv) {
  char scope[HTS_URLMAXSIZE];
  int k;

  (void) opt;
  if (argc >= 1) {
    for (k = 0; hts_wizard_host_scope(argv[0], k, scope, sizeof(scope)); k++)
      printf("%d %s\n", k, scope);
    return 0;
  }
#define SCOPE(question, k, expect)                                             \
  do {                                                                         \
    assertf(hts_wizard_host_scope((question), (k), scope, sizeof(scope)));     \
    assertf(strcmp(scope, (expect)) == 0);                                     \
  } while (0)
/* poisoned first: comparing against '\0' cannot see a clear that never ran */
#define NOSCOPE_SIZED(question, k, size)                                       \
  do {                                                                         \
    memset(scope, 'X', sizeof(scope));                                         \
    assertf(!hts_wizard_host_scope((question), (k), scope, (size)));           \
    assertf(scope[0] == '\0');                                                 \
  } while (0)
#define NOSCOPE(question, k) NOSCOPE_SIZED((question), (k), sizeof(scope))

  /* k widens by one label at a time, starting at the host itself */
  SCOPE("download.example.co.uk/x", 0, "download.example.co.uk");
  SCOPE("download.example.co.uk/x", 1, "example.co.uk");
  SCOPE("download.example.co.uk/x", 2, "co.uk");
  NOSCOPE("download.example.co.uk/x", 3); /* "uk" is a bare TLD */
  SCOPE("example.com", 0, "example.com"); /* an adr with no fil works */
  NOSCOPE("example.com", 1);
  NOSCOPE("localhost/x", 0); /* nothing to widen into */
  NOSCOPE("download.example.co.uk/x", -1);

  /* protocol and credentials are stripped, the port kept */
  SCOPE("ftp://user:pass@www.foo.com/x", 1, "foo.com");
  SCOPE("www.foo.com:8080/x", 0, "www.foo.com:8080");
  SCOPE("www.foo.com:8080/x", 1, "foo.com:8080");
  NOSCOPE("www.foo.com:8080/x", 2);
  /* a path that carries dots or a colon must not be read as host labels */
  SCOPE("foo.com/a.b.c/d:e", 0, "foo.com");
  NOSCOPE("foo.com/a.b.c/d:e", 1);

  /* the shared predicate: a dotless run of digits is a hostname, not an IP */
  assertf(hts_host_is_ipv4("1.2.3.4", 7));
  assertf(!hts_host_is_ipv4("12345", 5));
  assertf(!hts_host_is_ipv4("foo.com", 7));

  /* an IP literal splits on dots without being a domain */
  NOSCOPE("192.168.1.1/x", 0);
  NOSCOPE("192.168.1.1:8080/x", 0);
  NOSCOPE("[3ffe:b80:1234::1]/x", 0);
  /* the dots inside this one reach the label walk unless brackets are refused
   */
  NOSCOPE("[::ffff:1.2.3.4]/x", 0);

  /* the root label of a fully-qualified host is not a label */
  SCOPE("www.foo.com./x", 0, "www.foo.com.");
  SCOPE("www.foo.com./x", 1, "foo.com.");
  NOSCOPE("www.foo.com./x", 2); /* "com." is still a bare TLD */
  NOSCOPE(".", 0);

  /* the destination must fit the scope and its terminator, and never truncate
   */
  {
    const char *q = "www.example.com/x";
    const size_t need = strlen("www.example.com");

    memset(scope, 'X', sizeof(scope));
    assertf(hts_wizard_host_scope(q, 0, scope, need + 1));
    assertf(strcmp(scope, "www.example.com") == 0);
    NOSCOPE_SIZED(q, 0, need);
    NOSCOPE_SIZED(q, 0, 4);
    NOSCOPE_SIZED(q, 0, 1);
  }
#undef SCOPE
#undef NOSCOPE
#undef NOSCOPE_SIZED
  printf("wizardscope self-test OK\n");
  return 0;
}

/* #1117: which host-scope range an answer falls in. */
static int st_wizardscopeanswer(httrackp *opt, int argc, char **argv) {
  (void) opt;
  (void) argc;
  (void) argv;
#define ANSWER(n, expect) assertf(hts_wizard_scope_answer(n) == (expect))
  /* the plain answers, and the boundary just below the first range */
  ANSWER(-999, HTS_DEFAULT);
  ANSWER(-1, HTS_DEFAULT);
  ANSWER(0, HTS_DEFAULT);
  ANSWER(7, HTS_DEFAULT);
  ANSWER(50, HTS_DEFAULT);
  ANSWER(HTS_WIZARD_SCOPE_INCLUDE - 1, HTS_DEFAULT);
  /* include runs up to the exclude base, and exclude has no upper end */
  ANSWER(HTS_WIZARD_SCOPE_INCLUDE, HTS_FALSE);
  ANSWER(HTS_WIZARD_SCOPE_EXCLUDE - 1, HTS_FALSE);
  ANSWER(HTS_WIZARD_SCOPE_EXCLUDE, HTS_TRUE);
  ANSWER(INT_MAX, HTS_TRUE);
#undef ANSWER
  printf("wizardscopeanswer self-test OK\n");
  return 0;
}

/* Poison: comparing the recursion cap against 0 would not see a stray write of
   the level the crawl uses. */
#define PRIO_UNSET 42

/* Prints what answer `n` does to the crawl; with no arguments, asserts every
   answer, on an undecided, an allowed and an already refused link. */
static int st_wizardverdict(httrackp *opt, int argc, char **argv) {
  const hts_wizard asked = opt->wizard;
  FILE *const projectlog = opt->log;
  char line[HTS_URLMAXSIZE];
  FILE *log;
  int url, depth;

  url = -1; /* the undecided verdict the wizard is asked about */
  depth = PRIO_UNSET;
  opt->wizard = HTS_WIZARD_ASK;
  if (argc >= 1) {
    hts_wizard_apply_verdict(opt, atoi(argv[0]), "foo.com", "/a/b.html", &url,
                             &depth);
    printf("forbidden=%d stop=%d prio=%d\n", url,
           opt->wizard == HTS_WIZARD_AUTO, depth);
    opt->wizard = asked;
    return 0;
  }
  opt->log = NULL; /* the battery walks the answers that warn */
/* answer `n` over a link the crawl had left at `in`: the verdict it must leave,
   whether it stops the questions, and the recursion cap it must set. */
#define APPLIES(n, in, forbidden, stop, prio)                                  \
  do {                                                                         \
    url = (in);                                                                \
    depth = PRIO_UNSET;                                                        \
    opt->wizard = HTS_WIZARD_ASK;                                              \
    hts_wizard_apply_verdict(opt, (n), "foo.com", "/a/b.html", &url, &depth);  \
    assertf(url == (forbidden));                                               \
    assertf(opt->wizard == ((stop) ? HTS_WIZARD_AUTO : HTS_WIZARD_ASK));       \
    assertf(depth == (prio));                                                  \
  } while (0)
  /* '*' refuses and stops the questions */
  APPLIES(-1, 0, 1, 1, PRIO_UNSET);
  APPLIES(-1, 1, 1, 1, PRIO_UNSET);
  /* the refusing answers, 3 included although it emits no filter yet */
  APPLIES(0, 0, 1, 0, PRIO_UNSET);
  APPLIES(1, 0, 1, 0, PRIO_UNSET);
  APPLIES(2, 0, 1, 0, PRIO_UNSET);
  APPLIES(3, 0, 1, 0, PRIO_UNSET);
  /* 4 caps the recursion, and takes the link like any accepting answer */
  APPLIES(4, 0, 0, 0, 1);
  APPLIES(4, 1, 1, 0, 1);
  /* an accepting answer never clears a refusal the crawl already computed */
  APPLIES(5, 1, 1, 0, PRIO_UNSET);
  APPLIES(6, 1, 1, 0, PRIO_UNSET);
  APPLIES(7, 1, 1, 0, PRIO_UNSET);
  APPLIES(50, 1, 1, 0, PRIO_UNSET);
  APPLIES(6, 0, 0, 0, PRIO_UNSET);
  /* #1251: an answer we could not read refuses, like the empty one */
  APPLIES(-999, 0, 1, 0, PRIO_UNSET);
  APPLIES(-999, 1, 1, 0, PRIO_UNSET);
  /* both ends of each scope range: include allows, exclude forbids */
  APPLIES(HTS_WIZARD_SCOPE_INCLUDE, 0, 0, 0, PRIO_UNSET);
  APPLIES(HTS_WIZARD_SCOPE_EXCLUDE - 1, 0, 0, 0, PRIO_UNSET);
  APPLIES(HTS_WIZARD_SCOPE_EXCLUDE, 0, 1, 0, PRIO_UNSET);
  APPLIES(INT_MAX, 0, 1, 0, PRIO_UNSET);
  /* an answer in no range never overturns an accept or a refusal */
  APPLIES(8, 0, 0, 0, PRIO_UNSET);
  APPLIES(8, 1, 1, 0, PRIO_UNSET);
  APPLIES(999, 0, 0, 0, PRIO_UNSET);
  APPLIES(-2, 0, 0, 0, PRIO_UNSET);
  APPLIES(-1000, 0, 0, 0, PRIO_UNSET);
  APPLIES(INT_MIN, 0, 0, 0, PRIO_UNSET);
  APPLIES(HTS_WIZARD_SCOPE_INCLUDE - 1, 0, 0, 0, PRIO_UNSET);
  /* an answer that does not refuse leaves the link allowed, never undecided */
  APPLIES(4, -1, 0, 0, 1);
  APPLIES(5, -1, 0, 0, PRIO_UNSET);
  APPLIES(6, -1, 0, 0, PRIO_UNSET);
  APPLIES(7, -1, 0, 0, PRIO_UNSET);
  APPLIES(50, -1, 0, 0, PRIO_UNSET);
  APPLIES(8, -1, 0, 0, PRIO_UNSET); /* -999 is #1259's, not asserted here */
  APPLIES(HTS_WIZARD_SCOPE_INCLUDE, -1, 0, 0, PRIO_UNSET);
  /* and one that does refuse must still refuse it */
  APPLIES(-1, -1, 1, 1, PRIO_UNSET);
  APPLIES(0, -1, 1, 0, PRIO_UNSET);
  APPLIES(1, -1, 1, 0, PRIO_UNSET);
  APPLIES(2, -1, 1, 0, PRIO_UNSET);
  APPLIES(3, -1, 1, 0, PRIO_UNSET);
  APPLIES(HTS_WIZARD_SCOPE_EXCLUDE, -1, 1, 0, PRIO_UNSET);
#undef APPLIES

/* the one log line answer `n` leaves for `adr`, "" for none */
#define WARNS(n, adr, expect)                                                  \
  do {                                                                         \
    log = tmpfile();                                                           \
    assertf(log != NULL);                                                      \
    opt->log = log;                                                            \
    url = 0;                                                                   \
    depth = PRIO_UNSET;                                                        \
    hts_wizard_apply_verdict(opt, (n), (adr), "/a/b.html", &url, &depth);      \
    rewind(log);                                                               \
    if (fgets(line, (int) sizeof(line), log) == NULL)                          \
      line[0] = '\0';                                                          \
    /* the wanted text, an empty log where there is none, nothing after it */  \
    assertf(strstr(line, (expect)) != NULL &&                                  \
            ((expect)[0] != '\0') == (line[0] != '\0') &&                      \
            fgets(line, (int) sizeof(line), log) == NULL);                     \
    fclose(log);                                                               \
  } while (0)
  /* an answer the engine can honour in full says nothing */
  WARNS(6, "foo.com", "");
  WARNS(8, "foo.com", "unknown answer 8");
  WARNS(HTS_WIZARD_SCOPE_INCLUDE, "www.foo.com", "");
  /* #1251: the answers the engine cannot honour as asked */
  WARNS(-999, "foo.com", "could not read your answer");
  WARNS(HTS_WIZARD_SCOPE_INCLUDE, "127.0.0.1", "has no domain above it");
  WARNS(HTS_WIZARD_SCOPE_EXCLUDE + 5, "www.foo.com", "has no domain above it");
#undef WARNS

  opt->log = projectlog;
  opt->wizard = asked;
  printf("wizardverdict self-test OK\n");
  return 0;
}

#undef PRIO_UNSET

/* Prints the prompt the wizard asks about <adr> <fil>; with no argument,
   asserts it. */
static int st_wizardprompt(httrackp *opt, int argc, char **argv) {
  char prompt[HTS_URLMAXSIZE * 2];
  char adr[HTS_URLMAXSIZE], fil[HTS_URLMAXSIZE * 2];

  (void) opt;
  if (argc >= 2) {
    hts_wizard_prompt_url(prompt, sizeof(prompt), argv[0], argv[1]);
    printf("%s\n", prompt);
    return 0;
  }
#define ASKS(adr_, fil_, expect)                                               \
  do {                                                                         \
    hts_wizard_prompt_url(prompt, sizeof(prompt), (adr_), (fil_));             \
    assertf(strcmp(prompt, (expect)) == 0);                                    \
  } while (0)
  ASKS("foo.com", "/dir/page.html", "foo.com/dir/page.html");
  ASKS("foo.com:8080", "/", "foo.com:8080/");
  ASKS("user:pass@foo.com", "/x", "user:pass@foo.com/x");
  /* the separator, for the slash-less forms the parser does not emit today */
  ASKS("foo.com", "index.html", "foo.com/index.html");
  ASKS("foo.com", "", "foo.com/");
#undef ASKS

  /* #1251: a link too long for the prompt is clipped, not fatal, and what is
     left still names the host it came from */
  memset(adr, 'a', sizeof(adr) - 1);
  adr[sizeof(adr) - 1] = '\0';
  memset(fil, 'b', sizeof(fil) - 1);
  fil[sizeof(fil) - 1] = '\0';
  fil[0] = '/';
  hts_wizard_prompt_url(prompt, sizeof(prompt), adr, fil);
  assertf(strlen(prompt) == sizeof(prompt) - 1);
  assertf(strncmp(prompt, adr, sizeof(adr) - 1) == 0);
  assertf(prompt[sizeof(adr) - 1] == '/');
  /* and again with the separator to insert, which eats one more byte */
  fil[0] = 'b';
  hts_wizard_prompt_url(prompt, sizeof(prompt), adr, fil);
  assertf(strlen(prompt) == sizeof(prompt) - 1);
  assertf(strncmp(prompt, adr, sizeof(adr) - 1) == 0);
  assertf(prompt[sizeof(adr) - 1] == '/');
  assertf(prompt[sizeof(adr)] == 'b');
  /* a host alone long enough to fill it leaves no room for either */
  memset(fil, 'b', sizeof(fil) - 1);
  fil[0] = '/';
  hts_wizard_prompt_url(prompt, sizeof(prompt), fil, fil);
  assertf(strlen(prompt) == sizeof(prompt) - 1);
  assertf(strncmp(prompt, fil, sizeof(prompt) - 1) == 0);

  printf("wizardprompt self-test OK\n");
  return 0;
}

/* Resets the wizard's filter array to the command-line filters `cmd`
   (NULL-terminated). */
static void wz_seed(httrackp *opt, char **filters, int *filptr,
                    const char *const *cmd) {
  int i;

  *filptr = 0;
  opt->wizard_filters = 0;
  for (i = 0; cmd != NULL && cmd[i] != NULL; i++)
    strlcpybuff(filters[(*filptr)++], cmd[i], HTS_FILTER_SLOT_SIZE);
}

/* Asserts the array holds exactly `want`, in order, naming the slot that
   differs. */
static void wz_holds(char **filters, int filptr, const char *const *want) {
  int i;

  for (i = 0; want[i] != NULL; i++) {
    if (i >= filptr || strcmp(filters[i], want[i]) != 0)
      fprintf(stderr, "filter %d: got [%s], want [%s]\n", i,
              i < filptr ? filters[i] : "<past the end>", want[i]);
    assertf(i < filptr);
    assertf(strcmp(filters[i], want[i]) == 0);
  }
  if (filptr != i)
    fprintf(stderr, "filter %d: got [%s], want nothing\n", i,
            i < filptr ? filters[i] : "<past the end>");
  assertf(filptr == i);
}

/* Drives hts_wizard_insert_filters(): prints the array the given answers build
   over the command-line filters, or asserts the precedence rules. */
static int st_wizardinsert(httrackp *opt, int argc, char **argv) {
  const htsfilters saved = opt->filters;
  const int savedwizard = opt->wizard_filters;
  const int savedmax = opt->maxfilter;
  char **filters = NULL;
  int filptr = 0;
  int i;

  assertf(filters_init(&filters, opt->maxfilter, 0) != 0);
  opt->filters.filters = &filters;
  opt->filters.filptr = &filptr;
  opt->wizard_filters = 0;
  /* Answers are httrack.c's query3 codes; seeker_up is pinned off, so answer 5
     takes its directory branch (tests/264 covers the host one). */
#define INSERT(n, adr, fil)                                                    \
  hts_wizard_insert_filters(opt, (n), (adr), (fil), HTS_FALSE)
#define HOLDS(...)                                                             \
  do {                                                                         \
    const char *const want[] = {__VA_ARGS__, NULL};                            \
    wz_holds(filters, filptr, want);                                           \
  } while (0)
#define SEED(...)                                                              \
  do {                                                                         \
    const char *const cmd[] = {__VA_ARGS__, NULL};                             \
    wz_seed(opt, filters, &filptr, cmd);                                       \
  } while (0)
#define RESET() wz_seed(opt, filters, &filptr, NULL)
/* what the array as it stands decides for `url` */
#define VERDICT(url) fa_strjoker(0, filters, filptr, (url), NULL, NULL, NULL)

  if (argc >= 2) {
    int sep = argc;

    for (i = 2; i < argc; i++) {
      if (strcmp(argv[i], "@") == 0) {
        sep = i;
        break;
      }
    }
    for (i = sep + 1;
         i < argc && filptr + HTS_WIZARD_MAX_FILTERS < opt->maxfilter; i++)
      strlcpybuff(filters[filptr++], argv[i], HTS_FILTER_SLOT_SIZE);
    for (i = 2; i < sep; i++)
      INSERT(atoi(argv[i]), argv[0], argv[1]);
    for (i = 0; i < filptr; i++)
      printf("%s%s", i != 0 ? " " : "", filters[i]);
    printf("\n");
    goto done;
  }

  /* wizard_filters indexes one crawl's array, so binding another resets it */
  {
    char **other = NULL;
    int otherptr = 7;

    opt->wizard_filters = 3;
    filters_bind(opt, &other, &otherptr);
    assertf(opt->wizard_filters == 0);
    assertf(opt->filters.filters == &other && opt->filters.filptr == &otherptr);
    filters_bind(opt, &filters, &filptr);
  }

  /* a counter that outlived its array still cannot index past the end */
  SEED("-*.zip");
  opt->wizard_filters = 99;
  INSERT(6, "h", "/dir/two.html");
  assertf(opt->wizard_filters <= filptr);

  /* the correction case: "ignore this link", then "mirror the whole host" */
  RESET();
  assertf(INSERT(0, "h", "/dir/one.html") == 1);
  HOLDS("-h/dir/one.html");
  assertf(opt->wizard_filters == 1);
  assertf(VERDICT("h/dir/one.html") == -1);
  INSERT(6, "h", "/dir/two.html");
  HOLDS("-h/dir/one.html", "+h/*");
  assertf(opt->wizard_filters == 2);
  assertf(VERDICT("h/dir/one.html") == 1);

  /* reversing the answers reverses the outcome, or the block is not ordered */
  RESET();
  INSERT(6, "h", "/dir/two.html");
  INSERT(0, "h", "/dir/one.html");
  HOLDS("+h/*", "-h/dir/one.html");
  assertf(VERDICT("h/dir/one.html") == -1);
  assertf(VERDICT("h/dir/two.html") == 1);

  /* no answer reaches above the command line */
  SEED("-h/*.zip", "+h/keep/*");
  INSERT(6, "h", "/dir/two.html");
  HOLDS("+h/*", "-h/*.zip", "+h/keep/*");
  assertf(opt->wizard_filters == 1);
  assertf(VERDICT("h/a.zip") == -1);
  assertf(VERDICT("h/dir/two.html") == 1);

  /* the primary link records its scope first, so a later answer overrides it */
  RESET();
  INSERT(7, "h", "/dir/index.html");
  INSERT(0, "h", "/dir/one.html");
  HOLDS("+h/dir/*[file]", "-h/dir/one.html");
  assertf(VERDICT("h/dir/one.html") == -1);

  /* both halves of a host-scope answer land in the block, in emission order.
     The count returned is what the caller slices the block by, so assert it. */
  SEED("-*.zip");
  assertf(INSERT(HTS_WIZARD_SCOPE_INCLUDE + 1, "www.example.co.uk",
                 "/x.html") == 2);
  HOLDS("+*.example.co.uk/*", "+example.co.uk/*", "-*.zip");
  assertf(opt->wizard_filters == 2);

  /* an answer that emits nothing leaves the block and the array unchanged */
  SEED("-*.zip");
  assertf(INSERT(4, "h", "/dir/one.html") == 0);
  assertf(INSERT(50, "h", "/dir/one.html") == 0);
  assertf(INSERT(3, "h", "/dir/one.html") == 0);
  HOLDS("-*.zip");
  assertf(opt->wizard_filters == 0);

  /* answers 1, 5 and 7 emit nothing when the file part has no directory */
  RESET();
  INSERT(1, "h", "one.html");
  INSERT(5, "h", "one.html");
  INSERT(7, "h", "one.html");
  assertf(filptr == 0 && opt->wizard_filters == 0);

  printf("wizardinsert self-test OK\n");
done:
#undef INSERT
#undef HOLDS
#undef SEED
#undef RESET
#undef VERDICT
  freet(filters[0]);
  freet(filters);
  opt->filters = saved;
  opt->wizard_filters = savedwizard;
  opt->maxfilter = savedmax;
  return 0;
}

/* #1270: filters_insert() refuses a rule the matcher would never read, and
   says so, instead of storing one that can never fire. */
static int st_filtercap(httrackp *opt, int argc, char **argv) {
  const htsfilters saved = opt->filters;
  const int savedwizard = opt->wizard_filters;
  const int saveddebug = opt->debug;
  FILE *const projectlog = opt->log;
  char BIGSTK atcap[HTS_FILTER_MAXLEN + 1];   /* "+a..a*", the longest rule */
  char BIGSTK overcap[HTS_FILTER_MAXLEN + 2]; /* the same, one byte too long */
  char BIGSTK toolong[STRJOKER_MAXLEN + 3];   /* and one the matcher skips */
  char BIGSTK subject[HTS_FILTER_MAXLEN];     /* a URL all three would match */
  char BIGSTK line[STRJOKER_MAXLEN + 256]; /* room for a warning quoting one */
  char **filters = NULL;
  int filptr = 0;
  int taken, verdict, warned;

  (void) argc;
  (void) argv;
  memset(atcap, 'a', sizeof(atcap) - 1);
  atcap[0] = '+';
  atcap[sizeof(atcap) - 2] = '*';
  atcap[sizeof(atcap) - 1] = '\0';
  memset(overcap, 'a', sizeof(overcap) - 1);
  overcap[0] = '-';
  overcap[sizeof(overcap) - 2] = '*';
  overcap[sizeof(overcap) - 1] = '\0';
  memset(toolong, 'a', sizeof(toolong) - 1);
  toolong[0] = '-';
  toolong[sizeof(toolong) - 2] = '*';
  toolong[sizeof(toolong) - 1] = '\0';
  memset(subject, 'a', sizeof(subject) - 1);
  subject[sizeof(subject) - 1] = '\0';
  assertf(strlen(atcap) == HTS_FILTER_MAXLEN);
  assertf(strlen(overcap) == HTS_FILTER_MAXLEN + 1);
  assertf(strlen(toolong) > STRJOKER_MAXLEN);
  assertf(strlen(subject) <= STRJOKER_MAXLEN); /* else none could match */
  assertf(strjoker(subject, toolong + 1, NULL, NULL) == NULL);

  assertf(filters_init(&filters, opt->maxfilter, 0) != 0);
  opt->filters.filters = &filters;
  opt->filters.filptr = &filptr;
  opt->wizard_filters = 0;
  opt->debug = LOG_NOTICE;
/* non-zero, so a stray write into the slot above the last rule shows up */
#define POISON "-poison/*"
/* offer `pattern` to the array, and report what it did with it */
#define TRY(label, pattern)                                                    \
  do {                                                                         \
    FILE *const log = tmpfile();                                               \
    char want[64];                                                             \
    assertf(log != NULL);                                                      \
    opt->log = log;                                                            \
    taken = filters_insert(opt, filptr, (pattern));                            \
    verdict = fa_strjoker(0, filters, filptr, subject, NULL, NULL, NULL);      \
    rewind(log);                                                               \
    if (fgets(line, (int) sizeof(line), log) == NULL)                          \
      line[0] = '\0';                                                          \
    /* the reason, this rule's own length, and the rule itself */              \
    snprintf(want, sizeof(want), "%d bytes", (int) strlen(pattern));           \
    warned = strstr(line, "could never match") != NULL &&                      \
             strstr(line, want) != NULL && strstr(line, (pattern)) != NULL;    \
    assertf(line[0] == '\0' || warned); /* nothing else may be logged */       \
    fclose(log);                                                               \
    printf("%s: stored=%d rules=%d verdict=%d warned=%d\n", (label),           \
           taken != 0, filptr, verdict, warned);                               \
  } while (0)
/* a refusal leaves the count, the verdict and the slot above it untouched */
#define REFUSED()                                                              \
  do {                                                                         \
    assertf(!taken && filptr == 1 && verdict == 1 && warned);                  \
    assertf(strcmp(filters[0], atcap) == 0);                                   \
    assertf(strcmp(filters[1], POISON) == 0);                                  \
  } while (0)

  /* a rule at the cap is stored, silently, and still matches; refusing one byte
     early would be silent too */
  TRY("at the cap", atcap);
  assertf(taken && filptr == 1 && verdict == 1 && !warned);
  assertf(strcmp(filters[0], atcap) == 0);

  /* last match wins, so a stored overcap rule would forbid what the first
     allows: the verdict below is what proves it is really absent */
  strlcpybuff(filters[1], POISON, HTS_FILTER_SLOT_SIZE);
  TRY("one past the cap", overcap);
  REFUSED();
  TRY("past the matcher", toolong);
  REFUSED();

#undef REFUSED
#undef TRY
#undef POISON
  opt->log = projectlog;
  opt->debug = saveddebug;
  freet(filters[0]);
  freet(filters);
  opt->filters = saved;
  opt->wizard_filters = savedwizard;
  printf("filtercap self-test OK\n");
  return 0;
}

#undef POISON

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

  /* a first link whose escaped form overruns the old flat 1024-byte tempo: the
     redirect must carry the whole URL, not a clipped prefix */
  {
    char BIGSTK link[HTS_URLMAXSIZE * 2];
    char *p = link;

    strcpybuff(link, "http://example.com/");
    p += strlen(link);
    memset(p, 'a', 1200);
    p += 1200;
    strcpy(p, "/end.html");

    done = 0;
    fp = fopen(path, "wb");
    assertf(fp != NULL);
    hts_finish_makeindex(opt, &done, &fp, 1, link, "%s%s", "", "");
    assertf(fp == NULL);
    fp = fopen(path, "rb");
    assertf(fp != NULL);
    n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[n] = '\0';
    /* the closing quote proves the URL was not clipped mid-way */
    assertf(strstr(buf, "/end.html\">") != NULL);
  }

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

static void datadir_expect(const char *selfpath, const char *builtin,
                           const char *expect) {
  char got[HTS_URLMAXSIZE * 2];

  hts_resolve_datadir(got, sizeof(got), selfpath, builtin);
  if (strcmp(got, expect) != 0) {
    fprintf(stderr,
            "datadir: self=%s builtin=\"%s\" gave \"%s\", expected \"%s\"\n",
            selfpath != NULL ? selfpath : "(null)", builtin, got, expect);
  }
  assertf(strcmp(got, expect) == 0);
}

// -#test=datadir <dir>: a relocated tree must find its own templates instead of
// silently falling back to the built-in ones (#894). argv[0] is writable.
static int st_datadir(httrackp *opt, int argc, char **argv) {
  char path[HTS_URLMAXSIZE];
  char self[HTS_URLMAXSIZE];
  char expect[HTS_URLMAXSIZE * 2];
  char installed[HTS_URLMAXSIZE];
  char gone[HTS_URLMAXSIZE];
  /* Each holds a templates/index-header.html, the file path_bin is read for.
     nest/ keeps the flat case away from the installed share/httrack above. */
  static const char *const dirs[] = {"share/httrack", "bin", "nest/flat"};
  size_t i;

  (void) opt;
  assertf(argc >= 1);

  /* argv[0] is a fallback: what the engine actually resolves from is this. */
  assertf(hts_self_path(path, sizeof(path)) != NULL);
  assertf(fexist(path));
  /* Too small for any real path, so the truncation guard must refuse. */
  assertf(hts_self_path(path, 2) == NULL);

  for (i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
    FILE *fp;

    snprintf(path, sizeof(path), "%s/%s/templates/", argv[0], dirs[i]);
    assertf(structcheck(path) == 0);
    snprintf(path, sizeof(path), "%s/%s/templates/index-header.html", argv[0],
             dirs[i]);
    fp = fopen(path, "wb");
    assertf(fp != NULL);
    fclose(fp);
  }
  snprintf(installed, sizeof(installed), "%s/share/httrack/", argv[0]);
  snprintf(gone, sizeof(gone), "%s/gone/", argv[0]);

  /* A moved install: bin/ carries templates too, so this pins the order. */
  snprintf(self, sizeof(self), "%s/bin/httrack", argv[0]);
  snprintf(expect, sizeof(expect), "%s/bin/../share/httrack/", argv[0]);
  datadir_expect(self, gone, expect);

  /* The compiled-in path still wins when it exists. */
  datadir_expect(self, installed, installed);

  /* Flat layout: templates/ sits beside the binary. */
  snprintf(self, sizeof(self), "%s/nest/flat/httrack", argv[0]);
  snprintf(expect, sizeof(expect), "%s/nest/flat/", argv[0]);
  datadir_expect(self, gone, expect);

  /* Nothing to derive from, or nothing found: the compiled-in path stands. */
  datadir_expect("httrack", gone, gone);
  datadir_expect(NULL, gone, gone);
  snprintf(self, sizeof(self), "%s/nowhere/deep/httrack", argv[0]);
  datadir_expect(self, gone, gone);

  /* No compiled-in path, as on Windows: the executable's own directory. */
  snprintf(self, sizeof(self), "%s/nowhere/deep/httrack", argv[0]);
  snprintf(expect, sizeof(expect), "%s/nowhere/deep/", argv[0]);
  datadir_expect(self, "", expect);
  datadir_expect("httrack", "", "");

  /* A directory part too long for the layout suffix to be appended must clip,
     not abort: appending to a non-empty buffer is the *_safe_ abort path. */
  {
    /* Long enough that dirname + "../share/httrack/" overflows the candidate
       buffer, short enough that the dirname itself still fits. */
    const size_t dirlen = HTS_URLMAXSIZE * 2 - 8;
    char huge[HTS_URLMAXSIZE * 3];
    char got[HTS_URLMAXSIZE * 2];
    size_t n;

    huge[0] = '/';
    for (n = 1; n < dirlen - 1; n++) {
      huge[n] = 'a';
    }
    huge[dirlen - 1] = '/';
    memcpy(huge + dirlen, "httrack", sizeof("httrack"));
    hts_resolve_datadir(got, sizeof(got), huge, gone);
    assertf(strcmp(got, gone) == 0);
  }

  printf("datadir self-test OK\n");
  return 0;
}

// -#test=pathbin: report what startup resolved, which -#test=datadir cannot see
// because it hands hts_resolve_datadir() the path instead (#904).
static int st_pathbin(httrackp *opt, int argc, char **argv) {
  (void) argc;
  (void) argv;
  printf("path_bin=%s\n", StringBuff(opt->path_bin));
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
  /* GUI writes ANSI paths and winprofile.ini; mimic it (CP1252) */
  static const char *const projName = "caf\xE9";
  static const char *const catName = "th\xE9";
#else
  /* POSIX system charset is already utf-8 */
  static const char *const projName = "caf\xC3\xA9";
  static const char *const catName = "th\xC3\xA9";
#endif
  /* utf-8 forms the index must carry whatever the input charset was */
  static const char *const projUTF8 = "caf\xC3\xA9";
  static const char *const catUTF8 = "th\xC3\xA9";

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
  /* a non-ASCII category exercises the winprofile.ini charset path (#216) */
  snprintf(path, sizeof(path), "%s/%s/hts-cache/", topdir, projName);
  assertf(structcheck(path) == 0);
  snprintf(path, sizeof(path), "%s/%s/hts-cache/winprofile.ini", topdir,
           projName);
  fp = fopen(path, "wb");
  assertf(fp != NULL);
  fprintf(fp, "category=%s\n", catName);
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
  assertf(strstr(buf, catUTF8) != NULL);

  /* raw unlink/rmdir: UNLINK is utf-8 on Windows, these paths aren't */
  unlink(path);
  snprintf(path, sizeof(path), "%s/backblue.gif", topdir);
  unlink(path);
  snprintf(path, sizeof(path), "%s/fade.gif", topdir);
  unlink(path);
  snprintf(path, sizeof(path), "%s/%s/hts-cache/winprofile.ini", topdir,
           projName);
  unlink(path);
  snprintf(path, sizeof(path), "%s/%s/hts-cache", topdir, projName);
  rmdir(path);
  snprintf(path, sizeof(path), "%s/%s/index.html", topdir, projName);
  unlink(path);
  snprintf(path, sizeof(path), "%s/%s", topdir, projName);
  rmdir(path);
  rmdir(topdir);
  printf("topindex self-test OK\n");
  return 0;
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
  strm.next_in = (const Bytef *) src;
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
  robots_parse(r, host, txt, strlen(txt), NULL, 0, HTS_TRUE, NULL, 0);
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
    robots_parse(&rb, "h.test", txt, strlen(txt), NULL, 0, HTS_TRUE, maps,
                 sizeof(maps));
    assertf(strcmp(maps, "http://h.test/s1.xml\nhttps://h.test/s2.xml\n") == 0);
    assertf(checkrobots(&rb, "h.test", "/x") == -1);
    checkrobots_free(&rb);
  }

  printf("sitemap self-test OK\n");
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
  get_ftp_line(NULL, sv[0], line, sizeof(line), 5, NULL);
  deletesoc(sv[0]);
  printf("ftp-line self-test OK (bounded %d-byte reply)\n",
         (int) sizeof(flood));
  return 0;
}

/* ftp_split_userpass: the split itself, and userinfo refused rather than
   clipped into another account's name (#1032). */
static int st_ftpuser(httrackp *opt, int argc, char **argv) {
  static const size_t caps[] = {16, 256}; /* asymmetric: a shared bound shows */
  char ubuf[256 + 32], pbuf[sizeof(ubuf)], poison[sizeof(ubuf)];
  char in[2 * 256 + 8];
  size_t c, over;

  (void) opt;
  (void) argc;
  (void) argv;
  memset(poison, '#', sizeof(poison));
  {
    const char ok[] = "bob:secret@host/f"; // '@' at index 10

    assertf(ftp_split_userpass(ok, ok + 11, ubuf, sizeof(ubuf), pbuf,
                               sizeof(pbuf)) == HTS_TRUE);
    assertf(strcmp(ubuf, "bob") == 0);
    assertf(strcmp(pbuf, "secret") == 0);
  }
  {
    const char ok[] = "bob@host/f"; // no password: the '@' still ends the user

    assertf(ftp_split_userpass(ok, ok + 4, ubuf, sizeof(ubuf), pbuf,
                               sizeof(pbuf)) == HTS_TRUE);
    assertf(strcmp(ubuf, "bob") == 0);
    assertf(pbuf[0] == '\0');
  }
  {
    const char ok[] = "u@relay:pw@gw/f"; // only the last '@' ends the userinfo

    assertf(ftp_split_userpass(ok, ok + 11, ubuf, sizeof(ubuf), pbuf,
                               sizeof(pbuf)) == HTS_TRUE);
    assertf(strcmp(ubuf, "u@relay") == 0);
    assertf(strcmp(pbuf, "pw") == 0);
  }
  for (c = 0; c < sizeof(caps) / sizeof(caps[0]); c++) {
    const size_t ucap = caps[c], pcap = caps[1 - c];

    /* overshoot the user, the pass, then a bare name bounded only by '@' */
    for (over = 0; over <= 2; over++) {
      const size_t cap = over == 1 ? pcap : ucap;
      size_t len;

      for (len = cap - 2; len <= cap + 1; len++) {
        const size_t user_len = over == 1 ? 1 : len;
        const size_t pass_len = over == 0 ? 1 : (over == 1 ? len : 0);
        const size_t total = user_len + pass_len + (over == 2 ? 1 : 2);
        const hts_boolean fits = len < cap ? HTS_TRUE : HTS_FALSE;

        memset(in, 'u', user_len);
        if (over != 2) {
          in[user_len] = ':';
          memset(in + user_len + 1, 'p', pass_len);
        }
        in[total - 1] = '@';
        in[total] = '\0';
        memcpy(ubuf, poison, sizeof(ubuf)); /* a zero canary would hide a NUL */
        memcpy(pbuf, poison, sizeof(pbuf));
        assertf(ftp_split_userpass(in, in + total, ubuf, ucap, pbuf, pcap) ==
                fits);
        if (fits) {
          assertf(strlen(ubuf) == user_len && strlen(pbuf) == pass_len);
          assertf(ubuf[user_len - 1] == 'u');
          assertf(pass_len == 0 || pbuf[pass_len - 1] == 'p');
        } else {
          assertf(ubuf[0] == '\0' && pbuf[0] == '\0'); /* fail safe */
        }
        /* the whole tail: one canary byte misses a write just past it */
        assertf(memcmp(ubuf + ucap, poison, sizeof(ubuf) - ucap) == 0);
        assertf(memcmp(pbuf + pcap, poison, sizeof(pbuf) - pcap) == 0);
      }
    }
  }
  printf("ftp-userpass self-test OK\n");
  return 0;
}

/* Both quoting forms at two capacities: the quoted form is two bytes wider
   (#1019). */
static int st_ftpcmdlen(httrackp *opt, int argc, char **argv) {
  static const size_t caps[] = {32, FTP_LINE_SIZE};
  char BIGSTK buf[FTP_LINE_SIZE + 32];
  char BIGSTK poison[FTP_LINE_SIZE + 32];
  char BIGSTK path[FTP_LINE_SIZE + 2];
  char BIGSTK wire[FTP_LINE_SIZE * 2];
  size_t c, got = 0;
  T_SOC sv[2];

  (void) opt;
  (void) argc;
  (void) argv;
  memset(poison, '#', sizeof(poison));
  for (c = 0; c < sizeof(caps) / sizeof(caps[0]); c++) {
    const size_t cap = caps[c];
    int quoted;

    for (quoted = 0; quoted <= 1; quoted++) {
      const size_t verb = 5 + 2 * (size_t) quoted; /* "RETR " plus quotes */
      const size_t fit = cap - 1 - verb; /* longest path still fitting */
      size_t len;

      for (len = fit - 1; len <= fit + 1; len++) {
        memset(path, 'p', len);
        path[len] = '\0';
        if (quoted)
          path[0] = ' '; /* any of these forces the quoted form */
        memcpy(buf, poison, sizeof(buf)); /* a zero canary would hide a NUL */
        if (len > fit) {
          assertf(ftp_command(buf, cap, "RETR", path) == HTS_FALSE);
          assertf(buf[0] == '\0'); /* fail-safe for an ignored result */
        } else {
          assertf(ftp_command(buf, cap, "RETR", path) == HTS_TRUE);
          assertf(strlen(buf) == verb + len);
          assertf(strncmp(buf, "RETR ", 5) == 0);
          assertf(buf[verb + len - 1] == (quoted ? '\"' : 'p'));
        }
        /* the whole tail: one canary byte misses a write just past it */
        assertf(memcmp(buf + cap, poison, sizeof(buf) - cap) == 0);
      }
    }
  }

  /* send_line() adds the CRLF and drops, rather than truncates, an over-length
     line. */
  memset(path, 'q', FTP_LINE_SIZE);
  path[FTP_LINE_SIZE] = '\0';
  assertf(st_socketpair(sv) == 0);
  assertf(send_line(sv[0], path) == 0); /* one byte too long: never sent */
  path[FTP_LINE_SIZE - 1] = '\0';
  assertf(send_line(sv[0], path) != 0);
  deletesoc(sv[0]);
  for (;;) {
    const int n = (int) recv(sv[1], wire + got, (int) (sizeof(wire) - got), 0);

    if (n <= 0)
      break;
    got += (size_t) n;
  }
  deletesoc(sv[1]);
  assertf(got == FTP_LINE_SIZE + 1); /* the maximal command alone */
  assertf(memcmp(wire, path, FTP_LINE_SIZE - 1) == 0);
  assertf(memcmp(wire + FTP_LINE_SIZE - 1, "\r\n", 2) == 0);
  printf("ftp-cmdlen self-test OK (%d bytes sent)\n", (int) got);
  return 0;
}

/* send_line() must drop a command line carrying a control byte (#1010). */
static int st_ftpctrl(httrackp *opt, int argc, char **argv) {
  /* Verb and URL path as run_launch_ftp() hands them over, then the line the
     wire must carry; NULL for a command that must never leave. */
  static const struct {
    const char *verb;
    const char *path;
    const char *sent;
  } cases[] = {
      {"RETR", "/f.txt%0d%0aDELE%20secret.txt", NULL},
      {"RETR", "/f.txt%0dDELE%20secret.txt", NULL},
      {"RETR", "/f.txt%0aDELE%20secret.txt", NULL},
      {"LIST -A", "/d%0d%0aDELE%20secret.txt/", NULL},
      {"RETR", "/plain.txt", "RETR /plain.txt"},
      {"RETR", "/a%20b.txt", "RETR \"/a b.txt\""},
      {"RETR", "%2Fa%25b.txt", "RETR /a%b.txt"},
      /* High bytes must still go out: a plain-char check reads them negative
         and rejects them. */
      {"RETR", "/caf%e9.txt", "RETR /caf\xe9.txt"},
      /* Bare, these two would hand a server that shells out to ls a flag. */
      {"LIST -A", "/x%20-la/", "LIST -A \"/x -la/\""},
      {"LIST -A", "-la", "LIST -A \"-la\""},
  };

  char BIGSTK catbuff[CATBUFF_SIZE];
  char cmd[512];
  char expect[512];
  char wire[512];
  T_SOC sv[2];
  size_t got = 0, dropped = 1, i;

  (void) opt;
  (void) argc;
  (void) argv;
  expect[0] = '\0';
  assertf(st_socketpair(sv) == 0);
  assertf(send_line(sv[0], "USER bob\001") == 0); // any field, not just a path
  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    ftp_command(cmd, sizeof(cmd), cases[i].verb,
                unescape_http(catbuff, sizeof(catbuff), cases[i].path));
    if (cases[i].sent == NULL) {
      assertf(strstr(cmd, "DELE") != NULL); // the payload did reach the builder
      assertf(send_line(sv[0], cmd) == 0);
      dropped++;
    } else {
      assertf(send_line(sv[0], cmd) != 0);
      strcatbuff(expect, cases[i].sent);
      strcatbuff(expect, "\r\n");
    }
  }
  deletesoc(sv[0]); // EOF, so the read below sees the whole wire
  for (;;) {
    const int n = (int) recv(sv[1], wire + got, (int) (sizeof(wire) - got), 0);

    if (n <= 0)
      break;
    got += (size_t) n;
  }
  deletesoc(sv[1]);
  assertf(got == strlen(expect));
  assertf(memcmp(wire, expect, got) == 0);
  printf("ftp-ctrlchars self-test OK (%d bytes sent, %d rejected)\n", (int) got,
         (int) dropped);
  return 0;
}

/* Slurp a whole file into a malloc'd buffer; sets *len. NULL on error. */
static unsigned char *warc_slurp(const char *path, size_t *len) {
  char catbuff[CATBUFF_SIZE];
  FILE *f = FOPEN(fconv(catbuff, sizeof(catbuff), path), "rb");
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
/* Argument order kept for the existing call sites; the search itself is the
   shared hts_memstr. */
static const char *warc_memstr(const char *hay, const char *needle,
                               size_t haylen, size_t nlen) {
  return hts_memstr(hay, haylen, needle, nlen);
}

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
      a_body, sizeof(a_body) - 1, NULL, NULL, 200, 0, 0);

  /* 302 redirect: header-only, no body. */
  warc_write_transaction(
      w, "http://test.local/r", "127.0.0.1",
      "GET /r HTTP/1.1\r\nHost: test.local\r\n\r\n",
      "HTTP/1.1 302 Found\r\nLocation: http://test.local/a.html\r\n\r\n", NULL,
      0, NULL, NULL, 302, 0, 0);

  /* 200 binary, chunked coding on the wire (already de-chunked here). */
  warc_write_transaction(
      w, "http://test.local/b.bin", "127.0.0.1",
      "GET /b.bin HTTP/1.1\r\nHost: test.local\r\n\r\n",
      "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\n"
      "Transfer-Encoding: chunked\r\n\r\n",
      "\x00\x01\x02\x03\x04", 5, NULL, NULL, 200, 0, 0);

  /* 200 with a body shorter than the declared Content-Length (rewritten). */
  warc_write_transaction(
      w, "http://test.local/trunc", "127.0.0.1",
      "GET /trunc HTTP/1.1\r\nHost: test.local\r\n\r\n",
      "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: "
      "100\r\n\r\n",
      "short", 5, NULL, NULL, 200, 0, 0);

  /* Same payload as a.html at a new URL: identical-payload-digest revisit
     (OpenSSL builds only; a plain build writes a second full response). */
  warc_write_transaction(w, "http://test.local/a2.html", "127.0.0.1",
                         "GET /a2.html HTTP/1.1\r\nHost: test.local\r\n\r\n",
                         "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n",
                         a_body, sizeof(a_body) - 1, NULL, NULL, 200, 0, 0);

  /* 304 revisit with an EMPTY response-header block: the block is just the
     2-byte separator, so declared Content-Length must be exactly 2 (F3). */
  warc_write_transaction(w, "http://test.local/nm", "127.0.0.1",
                         "GET /nm HTTP/1.1\r\nHost: test.local\r\n\r\n", "",
                         NULL, 0, NULL, NULL, 304, 1, 0);

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
      sizeof(body) - 1, NULL, NULL, 200, 0, WARC_TRUNC_LENGTH);
  warc_write_transaction(
      w, "http://test.local/big.gz", "127.0.0.1",
      "GET /big.gz HTTP/1.1\r\nHost: test.local\r\n\r\n",
      "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Encoding: "
      "gzip\r\n\r\n",
      (const char *) gz, sizeof(gz), NULL, NULL, 200, 0, WARC_TRUNC_TIME);
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
        (const char *) body, sizeof(body), NULL, NULL, 200, 0, 0);
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
      (const char *) a_gz, sizeof(a_gz), NULL, NULL, 200, 0, 0);
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

/* A URL longer than the old 1024-byte header-format buffer must still reach the
   archive: the record used to be abandoned whole, silently (#785). The sweep
   straddles the boundary so both the stack-buffer and the grow path run. */
static int st_warc_longurl(httrackp *opt, int argc, char **argv) {
  /* "WARC-Target-URI: " + CRLF costs 19 bytes, so the old buffer failed at
     1005; 9000 forces several reallocs within one record. */
  static const size_t lengths[] = {100, 1003, 1004, 1005, 1006, 2000, 9000};
  static const char resp_hdr[] =
      "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
  char path[HTS_URLMAXSIZE * 2];
  char body[64];
  warc_writer *w;
  FILE *fp;
  char *blob;
  LLint fsz;
  const char *at2;
  size_t i, n, nrec = 0;
  int err = 0;

  if (argc < 1) {
    fprintf(stderr, "warc-longurl: need a writable directory\n");
    return 1;
  }
  snprintf(path, sizeof(path), "%s/longurl.warc", argv[0]);

  w = warc_open(opt, path);
  if (w == NULL) {
    fprintf(stderr, "warc-longurl: could not create %s\n", path);
    return 1;
  }
  for (i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
    const size_t len = lengths[i];
    char *uri = malloct(len + 1);

    if (uri == NULL) {
      warc_close(w);
      return 1;
    }
    /* A distinct tail per URI so a truncated one cannot match another. */
    snprintf(uri, len + 1, "http://example.com/%04d/", (int) len);
    memset(uri + strlen(uri), 'a', len - strlen(uri));
    uri[len] = '\0';
    /* Distinct payloads: identical ones dedupe into revisit records. */
    snprintf(body, sizeof(body), "<html><body>%04d</body></html>\n", (int) len);
    if (warc_write_transaction(w, uri, NULL, NULL, resp_hdr, body, strlen(body),
                               NULL, NULL, 200, 0, 0) != 0) {
      fprintf(stderr, "warc-longurl: write failed at length %d\n", (int) len);
      err = 1;
    }
    freet(uri);
  }
  warc_close(w);

  fsz = fsize_utf8(path);
  blob = (fsz > 0) ? malloct((size_t) fsz + 1) : NULL;
  if (blob == NULL) {
    fprintf(stderr, "warc-longurl: no archive written\n");
    return 1;
  }
  fp = FOPEN(path, "rb");
  n = (fp != NULL) ? fread(blob, 1, (size_t) fsz, fp) : 0;
  if (fp != NULL)
    fclose(fp);
  blob[n] = '\0';

  for (i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
    const size_t len = lengths[i];
    char want[64];
    const char *at;

    snprintf(want, sizeof(want), "WARC-Target-URI: http://example.com/%04d/",
             (int) len);
    at = strstr(blob, want);
    if (at == NULL) {
      fprintf(stderr, "warc-longurl: length %d lost its record\n", (int) len);
      err = 1;
    } else if (strlen(at) < strlen("WARC-Target-URI: ") + len ||
               at[strlen("WARC-Target-URI: ") + len] != '\r') {
      fprintf(stderr, "warc-longurl: length %d truncated\n", (int) len);
      err = 1;
    }
  }
  for (at2 = blob; (at2 = strstr(at2, "WARC-Type: response")) != NULL; at2++)
    nrec++;
  if (nrec != sizeof(lengths) / sizeof(lengths[0])) {
    fprintf(stderr, "warc-longurl: %d response records, want %d\n", (int) nrec,
            (int) (sizeof(lengths) / sizeof(lengths[0])));
    err = 1;
  }

  freet(blob);
  printf("warc-longurl: %s\n", err ? "FAIL" : "OK");
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
  int err = 0, nlines = 0, nm_lines = 0;
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
                         "one body\n", 9, NULL, NULL, 200, 0, 0);
  warc_write_resource(w, "ftp://files.example.com/data.bin", "127.0.0.1",
                      "application/octet-stream", "\x00\x01\x02\x03", 4, NULL,
                      0);
  warc_write_transaction(w, "http://alpha.example.com/two", "127.0.0.1",
                         "GET /two HTTP/1.1\r\nHost: alpha.example.com\r\n\r\n",
                         "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n",
                         "two body\n", 9, NULL, NULL, 200, 0, 0);
  /* Same payload as /one at a new URL: identical-payload-digest revisit under
     OpenSSL, a full response otherwise; either way one index line. */
  warc_write_transaction(w, "http://zeta.example.com/dup", "127.0.0.1",
                         "GET /dup HTTP/1.1\r\nHost: zeta.example.com\r\n\r\n",
                         "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n",
                         "one body\n", 9, NULL, NULL, 200, 0, 0);
  /* A 304 declares no type, so the index line takes the caller's (#826). */
  warc_write_transaction(w, "http://nm.example.com/kept", "127.0.0.1",
                         "GET /kept HTTP/1.1\r\nHost: nm.example.com\r\n"
                         "If-Modified-Since: Mon, 01 Jan 2024 00:00:00 GMT\r\n"
                         "\r\n",
                         "HTTP/1.1 304 Not Modified\r\n\r\n", NULL, 0, NULL,
                         "text/html", 200, 1, 0);
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
    if (strstr(url, "nm.example.com") != NULL) {
      nm_lines++;
      if (strstr(line, "\"mime\": \"text/html\"") == NULL)
        err = 1;
    }
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
  if (nlines != 5 || nm_lines != 1)
    err = 1; /* 4 responses/revisits + 1 resource; no warcinfo/request */
  printf("warc-cdx: %d index lines: %s\n", nlines, err ? "FAIL" : "OK");
  return err;
}

static char st_cdx_log[2048];

/* Collect the "WARC:" diagnostics only, so an unrelated message cannot satisfy
   (or break) a silence assertion. */
static void st_cdx_log_cb(httrackp *opt, int type, const char *format,
                          va_list args) {
  char line[512];
  (void) opt;
  (void) type;
  (void) vsnprintf(line, sizeof(line), format, args);
  if (strncmp(line, "WARC:", 5) == 0)
    strlncatbuff(st_cdx_log, line, sizeof(st_cdx_log),
                 sizeof(st_cdx_log) - strlen(st_cdx_log) - 1);
}

/* 1 on mismatch; want == NULL asks for silence. Resets the capture. */
static int st_cdx_logged(const char *name, const char *want) {
  const int bad =
      want != NULL ? strstr(st_cdx_log, want) == NULL : st_cdx_log[0] != '\0';
  if (bad)
    fprintf(stderr, "warc-cdx-errors: %s logged \"%s\", want \"%s\"\n", name,
            st_cdx_log, want != NULL ? want : "(nothing)");
  st_cdx_log[0] = '\0';
  return bad;
}

/* Leave a previous run's file behind for this one to find. */
static void st_cdx_leave(const char *path, const char *content) {
  FILE *const fp = FOPEN(path, "wb");

  assertf(fp != NULL);
  fputs(content, fp);
  fclose(fp);
}

/* Feed n index lines whose URLs are `pad` characters long. */
static void st_cdx_fill(warc_writer *w, int n, size_t pad) {
  char url[1024], req[1100];
  int i;

  for (i = 0; i < n; i++) {
    size_t l =
        (size_t) snprintf(url, sizeof(url), "http://e%d.example.com/", i);

    while (l + 1 < sizeof(url) && l < pad)
      url[l++] = 'p';
    url[l] = '\0';
    snprintf(req, sizeof(req),
             "GET / HTTP/1.1\r\nHost: e%d.example.com\r\n\r\n", i);
    warc_write_transaction(w, url, "127.0.0.1", req,
                           "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n",
                           "body\n", 5, NULL, NULL, 200, 0, 0);
  }
}

/* Every way warc_cdx_flush can fail to leave a usable index beside the archive
   it just replaced. The empty-index warning must stay silent for a first run,
   which has no previous index to invalidate (#1041). */
static int st_warc_cdx_errors(httrackp *opt, int argc, char **argv) {
  char path[HTS_URLMAXSIZE], cdx[HTS_URLMAXSIZE];
  static const char stale[] = "no record was indexed";
  static const char unwritable[] = "could not write the index";
  hts_boolean saved_cdx;
  void *saved_state;
  warc_writer *w;
  int err = 0;

  if (argc < 1) {
    fprintf(stderr, "warc-cdx-errors: needs a writable directory\n");
    return 1;
  }
  saved_state = opt->state.warc;
  saved_cdx = opt->warc_cdx;
  opt->warc_cdx = 1;
  st_cdx_log[0] = '\0';
  hts_set_log_vprint_callback(st_cdx_log_cb);

  /* No previous archive and nothing indexed: nothing on disk went stale. */
  fconcat(path, sizeof(path), argv[0], "cdxerr-first.warc.gz");
  w = warc_open(opt, path);
  assertf(w != NULL);
  warc_close(w);
  err |= st_cdx_logged("first run", NULL);

  /* A previous archive and its index: this run swapped over the first, so the
     second now describes an archive that is gone. */
  fconcat(path, sizeof(path), argv[0], "cdxerr-prev.warc.gz");
  fconcat(cdx, sizeof(cdx), argv[0], "cdxerr-prev.cdx");
  st_cdx_leave(path, "previous archive");
  st_cdx_leave(cdx, "com,example)/ 20240101000000 {}\n");
  w = warc_open(opt, path);
  assertf(w != NULL);
  warc_close(w);
  err |= st_cdx_logged("swap over a previous archive", stale);

  /* Mirror case: written in place, then abandoned, so the archive is clobbered
     and the index beside it describes what used to be there. */
  fconcat(path, sizeof(path), argv[0], "cdxerr-abort.warc.gz");
  fconcat(cdx, sizeof(cdx), argv[0], "cdxerr-abort.cdx");
  st_cdx_leave(cdx, "com,example)/ 20240101000000 {}\n");
  w = warc_open(opt, path);
  assertf(w != NULL);
  opt->state.warc = w;
  warc_abort_opt(opt);
  err |= st_cdx_logged("abandoned in-place run", stale);

  /* Same, with no index left behind: the message would name a file that never
     existed, which is the false alarm the gate is there to avoid. */
  fconcat(path, sizeof(path), argv[0], "cdxerr-abort-noidx.warc.gz");
  w = warc_open(opt, path);
  assertf(w != NULL);
  opt->state.warc = w;
  warc_abort_opt(opt);
  err |= st_cdx_logged("abandoned run with no index", NULL);

  /* An archive that never opened replaced nothing, so the index beside it
     still describes what is there: the failed open is the only error. */
  fconcat(path, sizeof(path), argv[0], "cdxerr-noopen.warc.gz");
  fconcat(cdx, sizeof(cdx), argv[0], "cdxerr-noopen.cdx");
  st_cdx_leave(cdx, "com,example)/ 20240101000000 {}\n");
  if (MKDIR(path) != 0 && errno != EEXIST) {
    fprintf(stderr, "warc-cdx-errors: mkdir %s failed: %s\n", path,
            strerror(errno));
    err = 1;
  } else {
    assertf(warc_open(opt, path) == NULL);
    err |= st_cdx_logged("archive that never opened", NULL);
  }

  /* The index path cannot be opened at all. */
  fconcat(path, sizeof(path), argv[0], "cdxerr-fopen.warc.gz");
  fconcat(cdx, sizeof(cdx), argv[0], "cdxerr-fopen.cdx");
  if (MKDIR(cdx) != 0 && errno != EEXIST) {
    fprintf(stderr, "warc-cdx-errors: mkdir %s failed: %s\n", cdx,
            strerror(errno));
    err = 1;
  } else {
    w = warc_open(opt, path);
    assertf(w != NULL);
    st_cdx_fill(w, 1, 0);
    warc_close(w);
    err |= st_cdx_logged("unopenable index path", unwritable);
  }

#ifndef _WIN32
  /* /dev/full fails every write with ENOSPC. An index below one stdio buffer
     never reaches the device until fclose, and a larger one errors before it,
     which is the difference the two messages carry. */
  if (access("/dev/full", W_OK) == 0) {
    fconcat(path, sizeof(path), argv[0], "cdxerr-short.warc.gz");
    fconcat(cdx, sizeof(cdx), argv[0], "cdxerr-short.cdx");
    (void) UNLINK(cdx);
    if (symlink("/dev/full", cdx) != 0) {
      fprintf(stderr, "warc-cdx-errors: symlink %s failed: %s\n", cdx,
              strerror(errno));
      err = 1;
    } else {
      w = warc_open(opt, path);
      assertf(w != NULL);
      st_cdx_fill(w, 1, 0);
      warc_close(w);
      err |= st_cdx_logged("index lost at fclose", unwritable);

      fconcat(path, sizeof(path), argv[0], "cdxerr-partial.warc.gz");
      fconcat(cdx, sizeof(cdx), argv[0], "cdxerr-partial.cdx");
      (void) UNLINK(cdx);
      if (symlink("/dev/full", cdx) != 0) {
        fprintf(stderr, "warc-cdx-errors: symlink %s failed: %s\n", cdx,
                strerror(errno));
        err = 1;
      } else {
        w = warc_open(opt, path);
        assertf(w != NULL);
        st_cdx_fill(w, 64, 512); /* well over any stdio buffer */
        warc_close(w);
        err |= st_cdx_logged("index truncated mid-write", "is incomplete");
      }
    }
  } else {
    printf("warc-cdx-errors: no /dev/full, skipping the write-failure cases\n");
  }
#endif

  hts_set_log_vprint_callback(NULL);
  opt->warc_cdx = saved_cdx;
  opt->state.warc = saved_state;
  printf("warc-cdx-errors: %s\n", err ? "FAIL" : "OK");
  return err;
}

/* One finished transaction through the engine hook. No stashed headers, so the
   hook synthesizes the status line the real caller supplies. */
static void st_warc_emit(httrackp *opt, const char *body) {
  lien_back *back = calloct(1, sizeof(lien_back));

  assertf(back != NULL);
  strcpybuff(back->url_adr, "example.com");
  strcpybuff(back->url_fil, "/teardown.html");
  back->r.statuscode = 200;
  strcpybuff(back->r.msg, "OK");
  strcpybuff(back->r.contenttype, "text/html");
  back->r.adr = strdupt(body);
  back->r.size = (LLint) strlen(body);
  warc_write_backtransaction(opt, back);
  freet(back->r.adr);
  freet(back);
}

/* memmem(): a NUL byte anywhere in the archive would cut strstr() short. */
static hts_boolean st_blob_has(const unsigned char *blob, size_t len,
                               const char *s) {
  const size_t n = strlen(s);
  size_t i;

  for (i = 0; n <= len && i <= len - n; i++) {
    if (memcmp(blob + i, s, n) == 0)
      return HTS_TRUE;
  }
  return HTS_FALSE;
}

/* The archive on disk must hold `want`'s run, and no trace of the others. */
static int st_warc_holds(const char *name, const char *path, const char *want,
                         const char *absent1, const char *absent2) {
  const char *const absent[2] = {absent1, absent2};
  size_t len = 0, i;
  unsigned char *blob = warc_slurp(path, &len);
  int err = 0;

  if (blob == NULL) {
    fprintf(stderr, "warc-teardown: %s: cannot read %s\n", name, path);
    return 1;
  }
  /* shape, not validity: the first record's version line, nothing more */
  if (len < 8 || memcmp(blob, "WARC/1.1", 8) != 0) {
    fprintf(stderr, "warc-teardown: %s: %s does not open on a WARC record\n",
            name, path);
    err++;
  }
  if (!st_blob_has(blob, len, want)) {
    fprintf(stderr, "warc-teardown: %s: %s lost the run holding \"%s\"\n", name,
            path, want);
    err++;
  }
  for (i = 0; i < 2; i++) {
    if (st_blob_has(blob, len, absent[i])) {
      fprintf(stderr, "warc-teardown: %s: %s took \"%s\"\n", name, path,
              absent[i]);
      err++;
    }
  }
  freet(blob);
  return err;
}

/* One archive, three writes: two runs, then the emit teardown makes. */
static int st_warc_teardown_case(httrackp *opt, const char *name,
                                 const char *path, hts_boolean abort_run) {
  static const char run1[] = "teardown-run-1-body";
  static const char run2[] = "teardown-run-2-body-longer";
  static const char late[] = "teardown-late-body";
  char tmp[HTS_URLMAXSIZE * 2 + 8];
  char catbuff[CATBUFF_SIZE];
  int err = 0;

  snprintf(tmp, sizeof(tmp), "%s.tmp", path);
  (void) UNLINK(fconv(catbuff, sizeof(catbuff), path));
  (void) UNLINK(fconv(catbuff, sizeof(catbuff), tmp));
  StringCopy(opt->warc_file, path);

  /* stands in for the per-mirror memset in hts_create_opt(): this test reuses
     the dispatcher's opt where the engine always has a fresh one */
  opt->state.warc = NULL;
  st_warc_emit(opt, run1);
  warc_close_opt(opt);
  if (fsize_utf8(path) <= 0) {
    fprintf(stderr, "warc-teardown: %s: the hook opened no archive\n", name);
    return 1;
  }

  /* a previous archive is now there, so this run goes through the temporary */
  opt->state.warc = NULL;
  st_warc_emit(opt, run2);
  if (!fexist_utf8(tmp)) {
    fprintf(stderr, "warc-teardown: %s: no temporary beside %s\n", name, path);
    return 1;
  }
  if (abort_run)
    warc_abort_opt(opt);
  else
    warc_close_opt(opt);

  st_warc_emit(opt, late); /* what back_finalize emits during teardown */
  if (fexist_utf8(tmp)) {
    fprintf(stderr, "warc-teardown: %s: orphan temporary %s\n", name, tmp);
    err++;
  }
  /* abort keeps run 1, close commits run 2; the late body guards against an
     append, no reopen can reach the archive without losing that run first */
  err += abort_run ? st_warc_holds(name, path, run1, run2, late)
                   : st_warc_holds(name, path, run2, run1, late);
  return err;
}

// -#test=warc-teardown <dir>: teardown finalizes the slots left in flight, so
// back_finalize emits after warc_close_opt/warc_abort_opt has run (#1060).
static int st_warc_teardown(httrackp *opt, int argc, char **argv) {
  char path[HTS_URLMAXSIZE], saved_file[HTS_URLMAXSIZE];
  void *saved_state;
  int err = 0;

  if (argc < 1) {
    fprintf(stderr, "warc-teardown: needs a writable directory\n");
    return 1;
  }
  saved_state = opt->state.warc;
  strlcpybuff(saved_file, StringBuff(opt->warc_file), sizeof(saved_file));

  fconcat(path, sizeof(path), argv[0], "teardown-close.warc");
  err += st_warc_teardown_case(opt, "close", path, HTS_FALSE);
  fconcat(path, sizeof(path), argv[0], "teardown-abort.warc");
  err += st_warc_teardown_case(opt, "abort", path, HTS_TRUE);

  StringCopy(opt->warc_file, saved_file);
  opt->state.warc = saved_state;
  printf("warc-teardown: %s\n", err ? "FAIL" : "OK");
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
                         "<html>home</html>\n", 18, NULL, NULL, 200, 0, 0);
  warc_write_transaction(
      w, "http://www.example.com/data.bin", "127.0.0.1",
      "GET /data.bin HTTP/1.1\r\nHost: www.example.com\r\n\r\n",
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/octet-stream\r\n\r\n",
      "\x00\x01\x02\x03\x04", 5, NULL, NULL, 200, 0, 0);
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

/* ------------------------------------------------------------ */
/* --single-file                                                 */
/* ------------------------------------------------------------ */

static int sf_err = 0;

/* Set when the filesystem accepted a ':' in a name; Windows never does. */
static hts_boolean sf_colon_ok = HTS_FALSE;

static void sf_check(int ok, const char *what) {
  if (!ok) {
    fprintf(stderr, "singlefile: %s\n", what);
    sf_err++;
  }
}

/* Write rel (a '/'-separated path under dir), creating the directories.
   Returns HTS_FALSE if the name is one the filesystem will not take. */
static hts_boolean sf_try_put(const char *dir, const char *rel,
                              const void *data, size_t len) {
  char BIGSTK path[HTS_URLMAXSIZE * 2];
  char catbuff[CATBUFF_SIZE];
  FILE *fp;

  fconcat(path, sizeof(path), dir, rel);
  structcheck_utf8(path);
  fp = FOPEN(fconv(catbuff, sizeof(catbuff), path), "wb");
  if (fp == NULL)
    return HTS_FALSE;
  assertf(len == 0 || fwrite(data, 1, len, fp) == len);
  fclose(fp);
  return HTS_TRUE;
}

/* Expand a fixture: \001<ref>\002 becomes <ref> plus this run's mark for it.
   The secret is random per run, so a fixture cannot spell a mark itself. */
static void sf_expand_fixture(httrackp *opt, const char *in, size_t len,
                              String *out) {
  size_t i, start = 0;

  StringClear(*out);
  for (i = 0; i < len; i++) {
    if (in[i] == '\001') {
      start = StringLength(*out);
    } else if (in[i] == '\002') {
      char mark[SINGLEFILE_MARK_MAX];

      StringCat(*out,
                singlefile_mark(opt, mark, sizeof(mark), SINGLEFILE_CLASS_ANY,
                                StringLength(*out) - start));
    } else {
      StringAddchar(*out, in[i]);
    }
  }
}

static void sf_put(const char *dir, const char *rel, const void *data,
                   size_t len) {
  assertf(sf_try_put(dir, rel, data, len));
}

/* sf_put() for a text fixture, expanding its \001<ref>\002 delimiters. Binary
   fixtures must not go through here: sf_png carries those very bytes. */
static size_t sf_put_marked(httrackp *opt, const char *dir, const char *rel,
                            const void *data, size_t len) {
  String body = STRING_EMPTY;
  size_t written;

  sf_expand_fixture(opt, (const char *) data, len, &body);
  assertf(sf_try_put(dir, rel, StringBuff(body), StringLength(body)));
  written = StringLength(body);
  StringFree(body);
  return written;
}

/* Number of times needle occurs in hay. */
static int sf_count(const char *hay, const char *needle) {
  const size_t l = strlen(needle);
  int n = 0;
  const char *p = hay;

  while ((p = strstr(p, needle)) != NULL) {
    n++;
    p += l;
  }
  return n;
}

/* The base64 payload following the first occurrence of prefix, up to the first
   byte outside the base64 alphabet. NULL if prefix is absent. */
static const char *sf_payload(const char *hay, const char *prefix,
                              size_t *len) {
  const char *p = strstr(hay, prefix);
  size_t n = 0;

  if (p == NULL)
    return NULL;
  p += strlen(prefix);
  while (p[n] != '\0' && (isalnum((unsigned char) p[n]) || p[n] == '+' ||
                          p[n] == '/' || p[n] == '='))
    n++;
  *len = n;
  return p;
}

/* Independent base64 decoder: the round-trip check must not lean on code64().
 */
static unsigned char *sf_unb64(const char *s, size_t len, size_t *outlen) {
  static const char alpha[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  unsigned char *out = (unsigned char *) malloct(len / 4 * 3 + 4);
  unsigned int acc = 0;
  size_t i, n = 0;
  int bits = 0;

  if (out == NULL)
    return NULL;
  for (i = 0; i < len; i++) {
    const char *const p = s[i] != '\0' ? strchr(alpha, s[i]) : NULL;

    if (s[i] == '=')
      break;
    if (p == NULL) {
      freet(out);
      return NULL;
    }
    acc = (acc << 6) | (unsigned int) (p - alpha);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out[n++] = (unsigned char) ((acc >> bits) & 0xff);
    }
  }
  *outlen = n;
  return out;
}

/* Decoded payload of the first data: URI with that MIME, as a NUL-terminated
   buffer the caller freet()s. NULL when absent or undecodable. */
static char *sf_decode(const char *hay, const char *mime, size_t *outlen) {
  char prefix[128];
  size_t len = 0, dlen = 0;
  const char *b64;
  unsigned char *raw;

  snprintf(prefix, sizeof(prefix), "data:%s;base64,", mime);
  b64 = sf_payload(hay, prefix, &len);
  if (b64 == NULL)
    return NULL;
  raw = sf_unb64(b64, len, &dlen);
  if (raw == NULL)
    return NULL;
  raw[dlen] = '\0';
  if (outlen != NULL)
    *outlen = dlen;
  return (char *) raw;
}

/* How many data: URIs of that MIME nest inside each other, starting at hay. */
static int sf_nesting(const char *hay, const char *mime) {
  char *cur = strdupt(hay);
  int n = 0;

  while (cur != NULL) {
    char *const inner = sf_decode(cur, mime, NULL);

    freet(cur);
    cur = inner;
    if (inner != NULL)
      n++;
  }
  return n;
}

/* Above the tag parser's attribute limit, so the fixture crosses it. */
#define SF_ST_MAX_ATTRS 64

/* The 12-byte asset: high bytes and an embedded NUL, so a text-shaped copy
   would be caught. */
static const char sf_png[] = "\x89PNG\r\n\x1a\n\x00\x01\x02\xff";
#define SF_PNG_LEN 12

static const char sf_svg[] = "<svg><g id=\"icon-a\"/></svg>";

static const char sf_page[] =
    "<html><head>\n"
    "<link rel=\"stylesheet\" href=\"\001css/main.css\002\">\n"
    "<link rel=\"canonical\" href=\"\001other.html\002\">\n"
    "<title>t</title>\n"
    "<style>body { background: url(\"\001img/a%20b.png\002\"); }\n"
    /* A fragment is document text the mark never covers, so it reaches the
       data: URI exactly as written, in the quoting the author gave it. */
    "b { background: url('\001img/sprite.svg\002#w x'); }\n"
    "i { background: url('\001img/sprite.svg\002#lt<s>gt'); }\n"
    "u { background: url(\"\001img/sprite.svg\002#z'(\\\xC3\xA9\"); }</style>\n"
    "</head><body>\n"
    "<img src=\"\001img/a%20b.png\002\" srcset=\"\001img/a%20b.png\002 "
    "1x, \001img/big.png\002 2x\">\n"
    "<link rel=\"icon\" href=\"\001icon.png\002\">\n"
    "<link rel=\"preload\" as=\"font\" href=\"\001font/f.woff2\002\">\n"
    "<img src=\"data:image/gif;base64,QUJD\">\n"
    /* Each has a real file where its guard's removal would land it; without
       that they stay links either way, the target merely being absent. */
    "<img src=\"http://example.com/x.png\">\n"
    "<img src=\"//example.com/x.png\">\n"
    "<input type=\"image\" src=\"\001img/in.png\002\">\n"
    /* Lazy loading: src is the placeholder, the real image rides data-src. */
    "<img src=\"\001img/ph.png\002\" data-src=\"\001img/lz.png\002\" "
    "data-srcset=\"\001img/lz2.png\002 2x\" "
    "lowsrc=\"\001img/low.png\002\">\n"
    "<object data=\"\001img/ob.png\002\"></object>\n"
    "<embed src=\"\001img/em.png\002\">\n"
    "<img data-src=\"\001other.html\002\">\n"
    /* What a first pass emits: re-resolving it is what a second pass must not
       do, and the fallback type would inline whatever the walk found. */
    "<link rel=\"stylesheet\" href=\"data:text/css;base64,QUJD\">\n"
    "<video poster=\"\001img/po.png\002\" controls>"
    "<source src=\"\001v.mp4\002\" type=\"video/mp4\"></video>\n"
    "<svg><image href=\"\001img/sv.png\002\"/></svg>\n"
    "<table background=\"\001img/bg.png\002\"><tr><td>x</td></tr></table>\n"
    /* The second is what bites: drop the clamp and its leading ".." lands it
       back on <root>/img/a b.png. The first can only 404 either way. */
    "<img src=\"\001../escape.png\002\">\n"
    "<img src=\"\001../img/a%20b.png\002\">\n"
    "<a href=\"img/a%20b.png\">link</a>\n"
    "<script src=\"\001js/app.js\002\"></script>\n"
    "<script>var s = \"</scripting>\"; var t = \"<img src='img/a%20b.png'>\";"
    "</script>\n"
    /* A fragment selects inside the asset, so it has to survive onto the
       data: URI; the query named the remote resource and must not. */
    "<img src=\"\001img/sprite.svg\002#icon-a\">\n"
    "<img srcset=\"\001img/sprite.svg\002#icon-b 2x\">\n"
    "<svg><image xlink:href=\"\001img/sprite.svg\002#icon-c\"/></svg>\n"
    "<div style=\"background:url(\001img/sprite.svg\002#icon-d)\"></div>\n"
    "<div style=\"background:url('\001img/sprite.svg\002#i)e')\"></div>\n"
    "<img src=\"\001img/sprite.svg?v=1\002#icon-f\">\n"
    "<img src=\"\001img/sprite.svg\002#g&amp;h%2Di\">\n"
    "<img src='\001img/sprite.svg\002#q\"z'>\n"
    "<img src=\"\001missing.png\002\" >\n"
    "<!--><img src=\"\001img/a%20b.png\002\">\n"
    "<div style=\"background:url(\001img/a%20b.png\002)\"></div>\n"
    "<div style='content:\"x\"; "
    "background:url(\001img/a%20b.png\002)'></div>\n"
    "</body></html>\n";

/* Lay a small mirror down under root. */
static void sf_fixture(httrackp *opt, const char *root) {
  /* The over-cap url() is what drives the rebase fallback: a reference an
     inlined stylesheet could not embed has to come out relative to the page,
     not to the stylesheet, or it dangles. */
  static const char css[] =
      "@import \"\001sub/nested.css\002\";\n"
      "@import url(\"\001sub/two.css\002\");\n"
      "@import \"a\\\"url(../img/a b.png)b.css\";\n"
      "@font-face { font-family: f; src: url(\001../font/f.woff2\002); }\n"
      "body { background: url(\001../img/a%20b.png\002); }\n"
      "div { background: url(\001../img/big.png\002); }\n"
      "div.s { background: url(\001../img/big-sprite.svg\002#icon-g); }\n"
      /* A name whose escapes the rebase has to put back, unlike a fragment's;
         the '#' has to come back encoded or it reads as one. */
      "div.h { background: url(\001../img/b&amp;c%25d%23e.png\002); }\n"
      "/* url(../img/never.png) */\n";
  static const char nested[] =
      "div { background: url(\001../../img/a%20b.png\002); }\n";
  static const char two[] =
      "p { background: url(\001../../img/a%20b.png\002); }\n";
  static const char deep[] = "<html><head><link rel=\"stylesheet\" "
                             "href=\"\001../../css/main.css\002\">\n"
                             "</head><body>d</body></html>\n";
  static const char js[] = "var app = 1;\n";
  char big[4096];

  memset(big, 'B', sizeof(big));
  sf_put_marked(opt, root, "page.html", sf_page, sizeof(sf_page) - 1);
  sf_put_marked(opt, root, "deep/sub/page.html", deep, sizeof(deep) - 1);
  sf_put(root, "other.html", "<html>o</html>", 14);
  sf_put_marked(opt, root, "css/main.css", css, sizeof(css) - 1);
  sf_put_marked(opt, root, "css/sub/nested.css", nested, sizeof(nested) - 1);
  sf_put_marked(opt, root, "css/sub/two.css", two, sizeof(two) - 1);
  sf_put(root, "js/app.js", js, sizeof(js) - 1);
  sf_put(root, "img/a b.png", sf_png, SF_PNG_LEN);
  sf_put(root, "img/big.png", big, sizeof(big));
  sf_put(root, "img/sprite.svg", sf_svg, sizeof(sf_svg) - 1);
  sf_put(root, "img/big-sprite.svg", big, sizeof(big));
  sf_put(root, "img/b&c%d#e.png", big, sizeof(big));
  sf_put(root, "img/in.png", sf_png, SF_PNG_LEN);
  sf_put(root, "img/po.png", sf_png, SF_PNG_LEN);
  sf_put(root, "img/sv.png", sf_png, SF_PNG_LEN);
  sf_put(root, "img/bg.png", sf_png, SF_PNG_LEN);
  sf_put(root, "img/ph.png", sf_png, SF_PNG_LEN);
  sf_put(root, "img/lz.png", sf_png, SF_PNG_LEN);
  sf_put(root, "img/lz2.png", sf_png, SF_PNG_LEN);
  sf_put(root, "img/low.png", sf_png, SF_PNG_LEN);
  sf_put(root, "img/ob.png", sf_png, SF_PNG_LEN);
  sf_put(root, "img/em.png", sf_png, SF_PNG_LEN);
  sf_put(root, "icon.png", sf_png, SF_PNG_LEN);
  sf_put(root, "font/f.woff2", "wOF2\x00\x01", 6);
  /* Where a guard's removal would land each reference that has to stay a
     link. The colon-bearing two are impossible on Windows, where the scheme
     guard then only gets the weaker "the link survived" check. */
  sf_put(root, "img/never.png", sf_png, SF_PNG_LEN);
  sf_put(root, "example.com/x.png", sf_png, SF_PNG_LEN);
  sf_colon_ok =
      sf_try_put(root, "http:/example.com/x.png", sf_png, SF_PNG_LEN) &&
      sf_try_put(root, "data:text/css;base64,QUJD", "p{}", 3);
  { /* More attributes than the tag parser records, with a '>' inside a quoted
       value: the give-up path must not rescan quote-blind. */
    String wide = STRING_EMPTY;
    int n;

    StringCopy(wide, "<html><body><p");
    for (n = 0; n <= SF_ST_MAX_ATTRS; n++) {
      char one[32];

      assertf(sprintfbuff(one, " a%d=1", n));
      StringCat(wide, one);
    }
    StringCat(wide,
              " title=\"> <img src=img/a%20b.png> \">end</p></body></html>");
    sf_put_marked(opt, root, "wide.html", StringBuff(wide), StringLength(wide));
    StringFree(wide);
  }
  sf_put(root, "v.mp4",
         "\x00\x00\x00\x18"
         "ftypisom",
         12);
}

/* -#test=singlefile <dir>: rewrite a hand-built mirror and check what gets
   inlined, what must keep its link, the per-asset cap, and idempotence. */
static int st_singlefile(httrackp *opt, int argc, char **argv) {
  char BIGSTK root[HTS_URLMAXSIZE];
  char BIGSTK page[HTS_URLMAXSIZE * 2];
  const LLint saved_cap = opt->single_file_max_size;
  char *out, *css, *nested;
  size_t outlen = 0, len = 0;

  if (argc < 1) {
    fprintf(stderr, "singlefile: needs a writable directory\n");
    return 1;
  }
  sf_err = 0;
  sf_put(argv[0], "escape.png", sf_png,
         SF_PNG_LEN); /* just outside the mirror */
  fconcat(root, sizeof(root), argv[0], "mirror/");
  sf_fixture(opt, root);
  fconcat(page, sizeof(page), root, "page.html");

  /* Cap between the small assets and big.png. */
  opt->single_file_max_size = 1024;
  sf_check(singlefile_rewrite_file(opt, root, page),
           "first pass changed nothing");
  out = readfile_utf8(page);
  assertf(out != NULL);

  /* Inlined, and the payload is the file's exact bytes. */
  {
    char *img = sf_decode(out, "image/png", &len);

    sf_check(img != NULL && len == SF_PNG_LEN &&
                 memcmp(img, sf_png, SF_PNG_LEN) == 0,
             "image payload does not round-trip");
    freet(img);
  }
  {
    char *js = sf_decode(out, "application/x-javascript", &len);

    sf_check(js != NULL && len == 13 && memcmp(js, "var app = 1;\n", 13) == 0,
             "script payload does not round-trip");
    freet(js);
  }
  sf_check(strstr(out, "href=\"data:text/css;base64,") != NULL,
           "stylesheet not inlined into the link");
  {
    char *font = sf_decode(out, "font/woff2", &len);

    sf_check(font != NULL && len == 6 && memcmp(font, "wOF2\x00\x01", 6) == 0,
             "rel=preload font payload does not round-trip");
    freet(font);
  }

  /* Every other (tag, attribute) rule in the table. */
  sf_check(strstr(out, "icon.png") == NULL, "rel=icon not inlined");
  sf_check(strstr(out, "img/in.png") == NULL, "input src not inlined");
  sf_check(strstr(out, "img/po.png") == NULL, "video poster not inlined");
  sf_check(strstr(out, "img/sv.png") == NULL, "svg image href not inlined");
  sf_check(strstr(out, "img/bg.png") == NULL,
           "legacy background attribute not inlined");
  sf_check(strstr(out, "img/ob.png") == NULL, "object data not inlined");
  sf_check(strstr(out, "img/em.png") == NULL, "embed src not inlined");
  sf_check(strstr(out, "img/ph.png") == NULL, "lazy placeholder not inlined");
  sf_check(strstr(out, "img/lz.png") == NULL, "data-src not inlined");
  sf_check(strstr(out, "img/lz2.png") == NULL, "data-srcset not inlined");
  sf_check(strstr(out, "img/low.png") == NULL, "lowsrc not inlined");
  /* The class gate, not the attribute name, is what keeps a page out. */
  sf_check(strstr(out, "data-src=\"other.html\"") != NULL,
           "data-src pointing at a page was inlined");

  sf_check(strstr(out, "<a href=\"img/a%20b.png\">") != NULL, "anchor inlined");
  sf_check(strstr(out, "href=\"other.html\"") != NULL, "rel=canonical inlined");
  sf_check(strstr(out, "src=\"v.mp4\"") != NULL, "video source inlined");
  sf_check(strstr(out, "src=\"http://example.com/x.png\"") != NULL,
           "absolute URL rewritten");
  sf_check(strstr(out, "src=\"//example.com/x.png\"") != NULL,
           "site-root-relative URL rewritten");
  sf_check(sf_count(out, "QUJD") == 2, "existing data: URI not preserved");
  sf_check(strstr(out, "src=\"../escape.png\"") != NULL,
           "a reference outside the mirror was resolved");
  sf_check(strstr(out, "src=\"../img/a%20b.png\"") != NULL,
           "a leading .. was dropped instead of rejected");
  sf_check(strstr(out, "var t = \"<img src='img/a%20b.png'>\";") != NULL,
           "script body rewritten past a </scripting> lookalike");

  /* Only the marked reference is touched: the value keeps its own quoting, so
     nothing can be emitted that the attribute could not already hold. */
  sf_check(strstr(out, "style='content:\"x\"; background:url(data:") != NULL,
           "style attribute re-quoted instead of substituted in place");

  /* Fragments: the mark covers the reference only, so what followed it comes
     back byte-identical -- including the escapes the document already carried
     -- while the query rode inside the mark and went with it. */
  sf_check(strstr(out, "img/sprite.svg") == NULL,
           "a fragment-bearing reference was left a link");
  sf_check(sf_count(out, "#icon-a\"") == 1, "img src fragment dropped");
  sf_check(sf_count(out, "#icon-b 2x\"") == 1, "srcset fragment dropped");
  sf_check(sf_count(out, "#icon-c\"") == 1, "xlink:href fragment dropped");
  sf_check(sf_count(out, "#icon-d)") == 1, "style url() fragment dropped");
  sf_check(sf_count(out, "#i)e')") == 1,
           "a fragment closing the url() token was rewritten");
  sf_check(sf_count(out, "#icon-f\"") == 1, "fragment after a query dropped");
  sf_check(strstr(out, "?v=1") == NULL, "query carried onto the data: URI");
  sf_check(sf_count(out, "#g&amp;h%2Di\"") == 1,
           "an escape the document already carried was encoded again");
  sf_check(sf_count(out, "#q\"z'") == 1, "a quote in a fragment was rewritten");
  sf_check(sf_count(out, "#w x')") == 1,
           "whitespace in a fragment was rewritten");
  sf_check(sf_count(out, "#lt<s>gt')") == 1,
           "'<'/'>' in a fragment were rewritten");
  sf_check(sf_count(out, "#z'(\\\xC3\xA9\")") == 1,
           "a quote, paren, backslash or high byte was rewritten");

  sf_check(strstr(out, "img/big.png 2x") != NULL, "over-cap asset inlined");
  sf_check(strstr(out, " 1x") != NULL, "srcset descriptor lost");
  sf_check(sf_count(out, "img/a%20b.png") ==
               3, /* the anchor, the script body, and the ".." one */
           "an inlinable reference was left as a link");

  /* The inlined stylesheet carries its own @import and url() inlined. */
  css = sf_decode(out, "text/css", NULL);
  sf_check(css != NULL, "stylesheet payload undecodable");
  if (css != NULL) {
    sf_check(strstr(css, "@import \"data:text/css;base64,") != NULL,
             "@import not inlined");
    sf_check(strstr(css, "url(data:image/png;base64,") != NULL,
             "url() in stylesheet not inlined");
    sf_check(strstr(css, "url(../img/never.png)") != NULL,
             "url() inside a CSS comment was rewritten");
    sf_check(strstr(css, "url(data:font/woff2;base64,") != NULL,
             "@font-face src not inlined");
    sf_check(strstr(css, "@import url(\"data:text/css;base64,") != NULL,
             "@import url() form not inlined");
    sf_check(strstr(css, "url(../img/a b.png)b.css") != NULL,
             "url() inside a string with an escaped quote was rewritten");
    /* The over-cap url() read ../img/big.png from css/; this page sits at the
       root, so it has to come back out as img/big.png or it dangles. */
    sf_check(strstr(css, "url(img/big.png)") != NULL,
             "over-cap url() not rebased onto the page's directory");
    sf_check(strstr(css, "url(img/big-sprite.svg#icon-g)") != NULL,
             "a rebased url() lost its fragment");
    sf_check(strstr(css, "url(img/b%26c%25d%23e.png)") != NULL,
             "a rebased name came back unescaped");
    nested = sf_decode(css, "text/css", NULL);
    sf_check(nested != NULL &&
                 strstr(nested, "url(data:image/png;base64,") != NULL,
             "url() in the @import'ed stylesheet not inlined");
    freet(nested);
    freet(css);
  }

  /* A tag with more attributes than the parser records comes back byte for
     byte, quoted '>' and all, instead of being re-scanned as markup. */
  {
    char BIGSTK wide[HTS_URLMAXSIZE * 2];
    char *before, *after;

    fconcat(wide, sizeof(wide), root, "wide.html");
    before = readfile_utf8(wide);
    assertf(before != NULL);
    (void) singlefile_rewrite_file(opt, root, wide);
    after = readfile_utf8(wide);
    sf_check(after != NULL && strcmp(before, after) == 0,
             "an over-wide tag was rewritten");
    freet(after);
    freet(before);
  }

  /* The same stylesheet from two directories down, where the rebase has to
     climb: the emitted path must stay inside the mirror and name the file. */
  {
    char BIGSTK deep[HTS_URLMAXSIZE * 2];
    char *dout, *dcss;

    fconcat(deep, sizeof(deep), root, "deep/sub/page.html");
    sf_check(singlefile_rewrite_file(opt, root, deep),
             "deep page not rewritten");
    dout = readfile_utf8(deep);
    assertf(dout != NULL);
    dcss = sf_decode(dout, "text/css", NULL);
    sf_check(dcss != NULL && strstr(dcss, "url(../../img/big.png)") != NULL,
             "over-cap url() not rebased from a nested page");
    freet(dcss);
    freet(dout);
  }

  /* Idempotence: a second pass must find nothing and leave the bytes alone. */
  sf_check(!singlefile_rewrite_file(opt, root, page), "second pass rewrote");
  {
    char *again = readfile_utf8(page);

    sf_check(again != NULL && strcmp(again, out) == 0,
             "second pass changed the page");
    freet(again);
  }
  freet(out);

  /* Same page, a cap above big.png: it now inlines. */
  fconcat(root, sizeof(root), argv[0], "mirror2/");
  sf_fixture(opt, root);
  fconcat(page, sizeof(page), root, "page.html");
  opt->single_file_max_size = 1024 * 1024;
  sf_check(singlefile_rewrite_file(opt, root, page),
           "raised-cap pass changed nothing");
  out = readfile_utf8(page);
  assertf(out != NULL);
  sf_check(strstr(out, "img/big.png 2x") == NULL,
           "asset under the raised cap still a link");
  freet(out);

  /* Nothing inlines, so the rewriter must be byte transparent. Assert on its
     output: the file it declined to write could not have changed regardless. */
  fconcat(root, sizeof(root), argv[0], "mirror3/");
  sf_fixture(opt, root);
  fconcat(page, sizeof(page), root, "page.html");
  opt->single_file_max_size = 1;
  /* The page is still rewritten: nothing inlines, but the marks must go. */
  sf_check(singlefile_rewrite_file(opt, root, page),
           "one-byte cap left the marks in place");
  {
    char *capped = readfile_utf8(page);

    /* Only the two data: URIs the fixture itself ships. */
    sf_check(capped != NULL && sf_count(capped, ";base64,") == 2,
             "one-byte cap inlined");
    freet(capped);
  }
  {
    String verbatim = STRING_EMPTY;

    StringClear(verbatim);
    String marked = STRING_EMPTY;

    sf_expand_fixture(opt, sf_page, sizeof(sf_page) - 1, &marked);
    (void) singlefile_rewrite_html(opt, root, page, StringBuff(marked),
                                   StringLength(marked),
                                   SINGLEFILE_MAX_PAGE_SIZE, &verbatim);
    /* Mark-transparent, not byte-transparent: a reference that cannot be
       inlined loses its mark and keeps everything else. */
    sf_check(StringLength(verbatim) < StringLength(marked),
             "a page with nothing to inline kept its marks");
    sf_check(strstr(StringBuff(verbatim), singlefile_intro(opt)) == NULL,
             "an un-inlinable reference kept its mark");
    StringFree(marked);
    StringFree(verbatim);
  }
  (void) outlen;

  /* A mark the pass cannot parse stays in the page as text, so whatever
     singlefile_mark writes it has to read back. emit_max restates htsparse's
     worst case for one reference independently of SINGLEFILE_MAX_SPAN, and the
     spans straddle it. */
  {
    const size_t emit_max =
        HTS_URLMAXSIZE * 2 *
        (HTS_HTMLESCAPE_FULL_MAXEXP + HTS_HTMLESCAPE_MAXEXP);
    const size_t spans[] = {4097, emit_max, emit_max + 1, emit_max * 4};
    char mark[SINGLEFILE_MARK_MAX];
    size_t k;

    sf_check(singlefile_mark(opt, mark, sizeof(mark), SINGLEFILE_CLASS_ANY,
                             emit_max)[0] != '\0',
             "a span htsparse can emit was refused a mark");
    for (k = 0; k < sizeof(spans) / sizeof(spans[0]); k++) {
      String marked = STRING_EMPTY, got = STRING_EMPTY;
      char *pad = (char *) malloct(spans[k]);
      size_t marklen;

      assertf(pad != NULL);
      memset(pad, 'a', spans[k]);
      StringClear(marked);
      StringCat(marked, "<img src=\"");
      StringMemcat(marked, pad, spans[k]);
      marklen = strlen(singlefile_mark(opt, mark, sizeof(mark),
                                       SINGLEFILE_CLASS_ANY, spans[k]));
      StringCat(marked, mark);
      StringCat(marked, "\">\n");
      StringClear(got);
      (void) singlefile_rewrite_html(opt, root, page, StringBuff(marked),
                                     StringLength(marked),
                                     SINGLEFILE_MAX_PAGE_SIZE, &got);
      sf_check(strstr(StringBuff(got), singlefile_intro(opt)) == NULL,
               "a mark the emitter wrote stayed in the page as text");
      sf_check(hts_memstr(StringBuff(got), StringLength(got), pad, spans[k]) !=
                   NULL,
               "the reference the mark measured was lost");
      /* Nothing resolves here, so the mark is the only thing that may go: a
         length check catches what a presence check cannot. */
      sf_check(StringLength(got) == StringLength(marked) - marklen,
               "the pass removed something other than the mark");
      if (spans[k] == emit_max + 1) {
        char tail[32];

        /* Hand-built at cap+1, which singlefile_mark now refuses: without the
           cap sf_parse_mark reads it and eats the padding behind it. */
        snprintf(tail, sizeof(tail), ".%c.%d", SINGLEFILE_CLASS_ANY,
                 (int) spans[k]);
        StringClear(marked);
        StringCat(marked, "<img src=\"");
        StringMemcat(marked, pad, spans[k]);
        StringCat(marked, singlefile_intro(opt));
        StringCat(marked, tail);
        StringCat(marked, "\">\n");
        StringClear(got);
        (void) singlefile_rewrite_html(opt, root, page, StringBuff(marked),
                                       StringLength(marked),
                                       SINGLEFILE_MAX_PAGE_SIZE, &got);
        sf_check(strstr(StringBuff(got), singlefile_intro(opt)) != NULL,
                 "an over-cap length was read as a mark");
        /* 2^64 + 10: unsaturated, the digits wrap to 10 and the mark eats ten
           bytes of padding instead of being refused. */
        StringClear(marked);
        StringCat(marked, "<img src=\"");
        StringMemcat(marked, pad, spans[k]);
        StringCat(marked, singlefile_intro(opt));
        StringCat(marked, ".-.18446744073709551626");
        StringCat(marked, "\">\n");
        StringClear(got);
        (void) singlefile_rewrite_html(opt, root, page, StringBuff(marked),
                                       StringLength(marked),
                                       SINGLEFILE_MAX_PAGE_SIZE, &got);
        sf_check(StringLength(got) == StringLength(marked),
                 "a wrapping digit run was read as a mark");
      }
      freet(pad);
      StringFree(marked);
      StringFree(got);
    }
  }

  /* singlefile_may_mark searches with hts_memstr, which finds nothing for an
     empty needle; the intro is fixed-width, so that case cannot arise. */
  {
    String probe = STRING_EMPTY;

    sf_check(strlen(singlefile_intro(opt)) == SINGLEFILE_INTRO_LEN,
             "the intro is not the fixed-width string may_mark assumes");
    sf_check(singlefile_may_mark(opt, "", 0), "an empty body was refused");
    StringClear(probe);
    StringMemcat(probe, "x\0", 2); /* bodies carry NULs; strstr would stop */
    StringCat(probe, singlefile_intro(opt));
    sf_check(!singlefile_may_mark(opt, StringBuff(probe), StringLength(probe)),
             "an intro ending the body, past a NUL, was missed");
    sf_check(
        singlefile_may_mark(opt, StringBuff(probe), StringLength(probe) - 1),
        "a truncated intro was read as one");
    StringFree(probe);
  }

  /* The per-page budget against a self-importing stylesheet. The large-budget
     run is the control: it proves the fan-out is real, so the small one was
     cut short by the budget and not by the fixture. */
  {
    static const char bomb_css[] =
        "@import \"\001b.css\002\";@import \"\001b.css\002\";"
        "@import \"\001b.css\002\";@import \"\001b.css\002\";\n";
    static const char bomb_html[] =
        "<html><head>"
        "<link rel=\"stylesheet\" href=\"\001b.css\002\">"
        "</head></html>\n";
    size_t css_len;
    String small = STRING_EMPTY, large = STRING_EMPTY, bomb = STRING_EMPTY;

    fconcat(root, sizeof(root), argv[0], "bomb/");
    css_len = sf_put_marked(opt, root, "b.css", bomb_css, sizeof(bomb_css) - 1);
    fconcat(page, sizeof(page), root, "page.html");
    opt->single_file_max_size = 1024 * 1024;
    StringClear(small);
    StringClear(large);
    sf_expand_fixture(opt, bomb_html, sizeof(bomb_html) - 1, &bomb);
    (void) singlefile_rewrite_html(opt, root, page, StringBuff(bomb),
                                   StringLength(bomb), (LLint) css_len * 3,
                                   &small);
    (void) singlefile_rewrite_html(opt, root, page, StringBuff(bomb),
                                   StringLength(bomb), SINGLEFILE_MAX_PAGE_SIZE,
                                   &large);
    sf_check(StringLength(large) > 4096, "the @import bomb did not fan out");
    sf_check(StringLength(small) < StringLength(large) / 8,
             "the per-page budget did not cut the fan-out short");
    /* Three files fit in three file-lengths only if the budget is charged
       before each nested rewrite; charging after measured five levels. */
    sf_check(sf_nesting(StringBuff(small), "text/css") == 3,
             "the per-page budget was not charged as each asset was taken");
    StringFree(small);
    StringFree(large);
    StringFree(bomb);
  }

  if (!sf_colon_ok)
    printf("singlefile: ':' is not a legal filename here, so the scheme guard "
           "only gets the weaker check\n");
  opt->single_file_max_size = saved_cap;
  printf("singlefile: %s\n", sf_err ? "FAIL" : "OK");
  return sf_err;
}

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
// via the mirror I/O wrappers — fexist_utf8/fsize_utf8, FOPEN/RENAME/UNLINK,
// and the new hts_rmdir_utf8 (RMDIR) teardown (#133, #630).
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

  // Rename to a non-ASCII sibling to exercise RENAME on the long path.
  char path2[HTS_URLMAXSIZE * 2];

  memcpybuff(path2, path, leafdir);
  memcpybuff(path2 + leafdir, "/\xC3\xA9-leaf.bin",
             sizeof("/\xC3\xA9-leaf.bin"));
  assertf(RENAME(path, path2) == 0);
  assertf(!fexist_utf8(path));
  assertf(fexist_utf8(path2));
  assertf(fsize_utf8(path2) == (LLint) sizeof(payload));
  assertf(UNLINK(path2) == 0);
  assertf(!fexist_utf8(path2));

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

static void ro_put(const char *path, const char *data) {
  FILE *const fp = FOPEN(path, "wb");

  assertf(fp != NULL);
  assertf(fwrite(data, 1, strlen(data), fp) == strlen(data));
  fclose(fp);
}

/* HTS_TRUE if path holds exactly data. */
static hts_boolean ro_is(const char *path, const char *data) {
  char buf[64];
  FILE *const fp = FOPEN(path, "rb");
  size_t n;

  if (fp == NULL)
    return HTS_FALSE;
  n = fread(buf, 1, sizeof(buf), fp);
  fclose(fp);
  return n == strlen(data) && memcmp(buf, data, n) == 0 ? HTS_TRUE : HTS_FALSE;
}

// -#test=renameover <dir>: hts_rename_over() must replace an existing dst and
// never lose one it did not replace (#779, #790). Which half is live depends on
// what rename() does to an existing target, so probe that and name the regime.
static int st_renameover(httrackp *opt, int argc, char **argv) {
  if (argc < 1) {
    fprintf(stderr, "renameover: needs a writable base dir\n");
    return 1;
  }
  char src[HTS_URLMAXSIZE * 2], dst[HTS_URLMAXSIZE * 2];
  int err = 0;

  fconcat(src, sizeof(src), argv[0], "renameover-src.bin");
  fconcat(dst, sizeof(dst), argv[0], "renameover-dst.bin");

  (void) UNLINK(src);
  (void) UNLINK(dst);
  ro_put(src, "probe");
  ro_put(dst, "probe");

  const int probe = RENAME(src, dst) == 0 ? 0 : errno;
  /* Only a target in the way is something the unlink can clear. */
  const hts_boolean replaceable = probe == 0 || probe == EEXIST;

  printf("renameover: regime %s\n",
         probe == 0 ? "clobber" : (probe == EEXIST ? "fallback" : "refused"));

  (void) UNLINK(src);
  (void) UNLINK(dst);
  ro_put(src, "new");
  ro_put(dst, "old");
  if (replaceable) {
    /* An existing dst must still be replaced: the unlink is for this. */
    if (!hts_rename_over(opt, src, dst)) {
      fprintf(stderr, "renameover: replacing an existing dst failed: %s\n",
              strerror(errno));
      err++;
    } else if (!ro_is(dst, "new") || fexist_utf8(src)) {
      fprintf(stderr, "renameover: dst was not replaced by src\n");
      err++;
    }
  } else {
    /* A failure the unlink cannot fix must leave dst as it was. */
    if (hts_rename_over(opt, src, dst)) {
      fprintf(stderr, "renameover: an unfixable failure reported success\n");
      err++;
    }
    if (!ro_is(dst, "old")) {
      fprintf(stderr, "renameover: an unfixable failure destroyed dst\n");
      err++;
    }
  }

  /* A directory in the way is not something the caller asked to replace: it
     must be refused, never parked aside and orphaned. */
  (void) UNLINK(dst);
  ro_put(src, "new");
  if (MKDIR(dst) == 0) {
    char parked[sizeof(dst) + 16];

    snprintf(parked, sizeof(parked), "%s.hts-old0", dst);
    if (hts_rename_over(opt, src, dst)) {
      fprintf(stderr, "renameover: a directory at dst reported success\n");
      err++;
    }
    if (!ro_is(src, "new")) {
      fprintf(stderr, "renameover: a directory at dst consumed src\n");
      err++;
    }
    /* RMDIR only succeeds on a directory that is there, so it doubles as the
       probe: the parked name must not exist at all. */
    if (RMDIR(parked) == 0 || fexist_utf8(parked)) {
      fprintf(stderr, "renameover: a directory at dst was parked aside\n");
      err++;
    }
    (void) RMDIR(dst);
  }
  (void) UNLINK(src);

  /* A missing src must leave dst alone and report failure. */
  (void) UNLINK(src);
  ro_put(dst, "keep");
  if (hts_rename_over(opt, src, dst)) {
    fprintf(stderr, "renameover: a missing src reported success\n");
    err++;
  }
  if (!ro_is(dst, "keep")) {
    fprintf(stderr, "renameover: a missing src destroyed dst\n");
    err++;
  }

  /* Same, with dst absent too: nothing to lose, still a failure. */
  (void) UNLINK(dst);
  if (hts_rename_over(opt, src, dst)) {
    fprintf(stderr, "renameover: a missing src and dst reported success\n");
    err++;
  }

  /* The aside fallback, driven directly: a clobbering rename() never reaches
     it. Skipped in the refused regime, where no rename at all succeeds. */
  if (replaceable) {
    char aside[sizeof(dst) + 16], keep[sizeof(dst) + 16];

    snprintf(aside, sizeof(aside), "%s.hts-old0", dst);
    snprintf(keep, sizeof(keep), "%s.hts-old1", dst);
    (void) UNLINK(aside);
    (void) UNLINK(keep);
    ro_put(src, "new");
    ro_put(dst, "old");
    if (!hts_rename_over_aside_selftest(opt, src, dst)) {
      fprintf(stderr, "renameover: the aside fallback failed: %s\n",
              strerror(errno));
      err++;
    } else if (!ro_is(dst, "new") || fexist_utf8(src) || fexist_utf8(aside)) {
      fprintf(stderr, "renameover: the aside fallback did not replace dst\n");
      err++;
    }

    /* #790: the retry fails (no src). The old content must survive, back at dst
       or, when the move back fails too, under the parked name it is logged as.
       Name the outcome so a leg cannot pass having tested the other one. */
    (void) UNLINK(src);
    ro_put(dst, "old");
    if (hts_rename_over_aside_selftest(opt, src, dst)) {
      fprintf(stderr, "renameover: a failed aside retry reported success\n");
      err++;
    }
    if (ro_is(dst, "old") && !fexist_utf8(aside)) {
      printf("renameover: restore back\n");
    } else if (ro_is(aside, "old") && !fexist_utf8(dst)) {
      printf("renameover: restore parked\n");
      (void) UNLINK(aside);
      ro_put(dst, "old");
    } else {
      fprintf(stderr, "renameover: a failed aside retry lost the old copy\n");
      err++;
    }

    /* An unrelated file already sitting on the aside name must survive. */
    ro_put(src, "new");
    ro_put(aside, "mine");
    if (!hts_rename_over_aside_selftest(opt, src, dst)) {
      fprintf(stderr, "renameover: a taken aside name failed the move: %s\n",
              strerror(errno));
      err++;
    } else if (!ro_is(dst, "new") || !ro_is(aside, "mine") ||
               fexist_utf8(keep)) {
      fprintf(stderr, "renameover: a taken aside name was not skipped\n");
      err++;
    }
    (void) UNLINK(aside);
    (void) UNLINK(keep);

    /* A directory there reads as free to the probe, so the park must skip it
       on the refusal rather than give up. */
    ro_put(src, "new");
    ro_put(dst, "old");
    if (MKDIR(aside) == 0) {
      if (!hts_rename_over_aside_selftest(opt, src, dst)) {
        fprintf(stderr,
                "renameover: a directory on the aside name blocked the "
                "move: %s\n",
                strerror(errno));
        err++;
      } else if (!ro_is(dst, "new") || fexist_utf8(keep)) {
        fprintf(stderr, "renameover: a directory on the aside name was not "
                        "skipped\n");
        err++;
      }
      (void) RMDIR(aside);
    }
    (void) UNLINK(keep);
  }

  (void) UNLINK(src);
  (void) UNLINK(dst);
  printf("renameover: %s\n", err ? "FAIL" : "OK");
  return err;
}

// -#test=refetchbackup <dir>: the #77 re-fetch backup must build its temporary
// inside the ~hts-tmp directory, which no save name can spell (#774), and must
// never leave the resource without a copy (#775).
static int st_refetchbackup(httrackp *opt, int argc, char **argv) {
  lien_back *back;
  char want[HTS_URLMAXSIZE * 2 + 32];
  int err = 0;

  if (argc < 1) {
    fprintf(stderr, "refetchbackup: needs a writable base dir\n");
    return 1;
  }
  back = calloct(1, sizeof(lien_back));
  if (back == NULL) {
    fprintf(stderr, "refetchbackup: out of memory\n");
    return 1;
  }
  /* explicit separator: fconcat() joins without one, which would put the
     temporary in the parent of the directory under test */
  snprintf(back->url_sav, sizeof(back->url_sav), "%s/refetch.bin", argv[0]);
  snprintf(want, sizeof(want), "%s/~hts-tmp/refetch.bin.bak", argv[0]);

  /* #774: pin the name, so moving the temporary back into the mirror namespace
     cannot pass unnoticed. */
  ro_put(back->url_sav, "old");
  back_refetch_backup(opt, back);
  if (back->tmpfile == NULL || fexist_utf8(back->url_sav)) {
    fprintf(stderr, "refetchbackup: the previous copy was not moved aside\n");
    err++;
  } else if (strcmp(back->tmpfile, want) != 0) {
    fprintf(stderr, "refetchbackup: temporary is %s, want %s\n", back->tmpfile,
            want);
    err++;
  }
  ro_put(back->url_sav, "new"); /* what filecreate() + the transfer produce */
  back_finalize_backup(opt, back, HTS_TRUE);
  if (!ro_is(back->url_sav, "new")) {
    fprintf(stderr, "refetchbackup: the committed copy is not the new one\n");
    err++;
  }

  /* #758: only a killed run can leave something there, and it must be replaced
     rather than disable the backup for good. */
  if (structcheck(want) != 0) {
    fprintf(stderr, "refetchbackup: cannot create %s\n", want);
    freet(back);
    return 1;
  }
  ro_put(want, "leftover");
  back_refetch_backup(opt, back);
  if (back->tmpfile == NULL || !ro_is(want, "new")) {
    fprintf(stderr, "refetchbackup: a leftover temporary blocked the backup\n");
    err++;
  }

  /* #775: filecreate() failed, so there is nothing to commit to. Saying so is
     load-bearing: the caller must not cache this response against the old
     body, or the next --update gets a 304 pinning it. */
  (void) UNLINK(back->url_sav);
  if (back_finalize_backup(opt, back, HTS_TRUE)) {
    fprintf(stderr, "refetchbackup: a commit that restored reported success\n");
    err++;
  }
  if (!ro_is(back->url_sav, "new")) {
    fprintf(stderr, "refetchbackup: a commit with no new copy lost both\n");
    err++;
  }

  /* An aborted transfer restores, as before. */
  back_refetch_backup(opt, back);
  ro_put(back->url_sav, "partial");
  back_finalize_backup(opt, back, HTS_FALSE);
  if (!ro_is(back->url_sav, "new")) {
    fprintf(stderr, "refetchbackup: an aborted re-fetch kept the partial\n");
    err++;
  }

  (void) UNLINK(back->url_sav);
  freet(back);
  printf("refetchbackup: %s\n", err ? "FAIL" : "OK");
  return err;
}

// -#test=spoolname <dir>: a frozen backlog slot must spool inside ~hts-tmp, not
// beside the mirrored file where a site serving <path>.tmp collides (#859).
static int st_spoolname(httrackp *opt, int argc, char **argv) {
  char BIGSTK got[HTS_URLMAXSIZE * 2 + 32];
  char BIGSTK want[HTS_URLMAXSIZE * 2 + 32];
  char BIGSTK save[HTS_URLMAXSIZE * 2];
  int err = 0;

  if (argc < 1) {
    fprintf(stderr, "spoolname: needs a writable base dir\n");
    return 1;
  }

  /* named: the spool lands in the save name's own ~hts-tmp, which no URL can
     spell since url_savename() maps '~' to '_' */
  snprintf(save, sizeof(save), "%s/sub/page.html", argv[0]);
  snprintf(want, sizeof(want), "%s/sub/~hts-tmp/page.html.tmp", argv[0]);
  opt->getmode = 1;
  if (!back_spoolname(opt, save, got, sizeof(got))) {
    fprintf(stderr, "spoolname: naming failed for %s\n", save);
    err++;
  } else if (strcmp(got, want) != 0) {
    fprintf(stderr, "spoolname: got %s, want %s\n", got, want);
    err++;
  }

  /* pin the pre-#859 name as forbidden too: a site serving sub/page.html.tmp
     was mirrored straight onto it */
  snprintf(want, sizeof(want), "%s.tmp", save);
  if (strcmp(got, want) == 0) {
    fprintf(stderr, "spoolname: still spooling into the mirror namespace\n");
    err++;
  }

  /* -p0 keeps no save name, so the spool counts inside path_html's ~hts-tmp */
  {
    char BIGSTK base[HTS_URLMAXSIZE * 2];

    snprintf(base, sizeof(base), "%s/", argv[0]);
    StringCopy(opt->path_html_utf8, base);
    opt->getmode = 0;
    opt->state.tmpnameid = 7;
    snprintf(want, sizeof(want), "%s/~hts-tmp/tmpfile7.tmp", argv[0]);
    if (!back_spoolname(opt, "", got, sizeof(got))) {
      fprintf(stderr, "spoolname: naming failed under -p0\n");
      err++;
    } else if (strcmp(got, want) != 0) {
      fprintf(stderr, "spoolname: -p0 got %s, want %s\n", got, want);
      err++;
    }
    if (opt->state.tmpnameid != 8) {
      fprintf(stderr, "spoolname: -p0 did not consume a tmpnameid\n");
      err++;
    }
  }

  /* with no -O, path_html_utf8 is empty and the spool must stay relative to
     the working directory; a separator of our own would put it in / */
  StringCopy(opt->path_html_utf8, "");
  opt->getmode = 0;
  opt->state.tmpnameid = 0;
  if (!back_spoolname(opt, "", got, sizeof(got))) {
    fprintf(stderr, "spoolname: naming failed with no output directory\n");
    err++;
  } else if (strcmp(got, "~hts-tmp/tmpfile0.tmp") != 0) {
    fprintf(stderr, "spoolname: no -O gave %s, want ~hts-tmp/tmpfile0.tmp\n",
            got);
    err++;
  }

  /* too long must empty dest, not hand back a truncated name landing
     somewhere real */
  opt->getmode = 1;
  if (back_spoolname(opt, save, got, 8) || got[0] != '\0') {
    fprintf(stderr, "spoolname: an overlong name was not rejected\n");
    err++;
  }

  printf("spoolname: %s\n", err ? "FAIL" : "OK");
  return err;
}

// -#test=direnum <dir>: enumerate a long+non-ASCII directory via the
// opendir/readdir wrappers; children must round-trip as UTF-8 (#133,#630).
static int st_direnum(httrackp *opt, int argc, char **argv) {
  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "direnum: needs a writable base dir\n");
    return 1;
  }
  char path[HTS_URLMAXSIZE * 2];
  size_t n = (size_t) snprintf(path, sizeof(path), "%s", argv[0]);

  while (n > 0 && (path[n - 1] == '/' || path[n - 1] == '\\')) {
    path[--n] = '\0';
  }
  const size_t base = n;
  // Non-ASCII first segment + 40-char ASCII segments push the dir past
  // MAX_PATH.
  static const char nseg[] = "/\xC3\xA9\xE4\xB8\xAD-non-ascii-seg";
  static const char seg[] = "/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

  memcpybuff(path + n, nseg, sizeof(nseg));
  n += sizeof(nseg) - 1;
  if (MKDIR(path) != 0 && errno != EEXIST) {
    fprintf(stderr, "direnum: mkdir failed (non-ascii): %s\n", strerror(errno));
    return 1;
  }
  while (n + sizeof(seg) - 1 < 300) {
    memcpybuff(path + n, seg, sizeof(seg));
    n += sizeof(seg) - 1;
    if (MKDIR(path) != 0 && errno != EEXIST) {
      fprintf(stderr, "direnum: mkdir failed at %u chars: %s\n", (unsigned) n,
              strerror(errno));
      return 1;
    }
  }
  const size_t dirlen = n;

  assertf(dirlen > 260); /* the enumerated directory itself exceeds MAX_PATH */

  // Two non-ASCII leaf files to read back by name.
  static const char *const leaves[] = {"/\xC3\xA9-un.bin",
                                       "/\xE4\xB8\xAD-deux.bin"};
  for (size_t i = 0; i < 2; i++) {
    memcpybuff(path + dirlen, leaves[i], strlen(leaves[i]) + 1);
    FILE *fp = FOPEN(path, "wb");

    if (fp == NULL) {
      fprintf(stderr, "direnum: create failed: %s\n", strerror(errno));
      return 1;
    }
    fclose(fp);
  }
  path[dirlen] = '\0';

  int found = 0;
  DIR *d = opendir(path);

  if (d == NULL) {
    fprintf(stderr, "direnum: opendir failed: %s\n", strerror(errno));
    return 1;
  }
  struct dirent *e;

  while ((e = readdir(d)) != NULL) {
    for (size_t i = 0; i < 2; i++) {
      if (strcmp(e->d_name, leaves[i] + 1) == 0) { /* +1: drop the '/' */
        found |= 1 << i;
      }
    }
  }
  closedir(d);
  if (found != 0x3) {
    fprintf(stderr, "direnum: missing entries (found mask 0x%x)\n", found);
    return 1;
  }

  // Teardown: leaves then the directory chain, via the long-path wrappers.
  for (size_t i = 0; i < 2; i++) {
    memcpybuff(path + dirlen, leaves[i], strlen(leaves[i]) + 1);
    assertf(UNLINK(path) == 0);
  }
  path[dirlen] = '\0';
  while (strlen(path) > base) {
    char *const slash = strrchr(path, '/');

    if (RMDIR(path) != 0) {
      fprintf(stderr, "direnum: rmdir failed: %s\n", strerror(errno));
      return 1;
    }
    if (slash == NULL || (size_t) (slash - path) < base) {
      break;
    }
    *slash = '\0';
  }

  printf("direnum: enumerated 2 non-ASCII leaves under a %u-char dir: OK\n",
         (unsigned) dirlen);
  return 0;
}

/* -#test=cookieimport <dir>: load a jar (and, on Windows, copied IE cookies
   *@*.txt) from a long, non-ASCII folder via the UTF-8/long-path wrappers
   (#133). POSIX compiles the IE block out; there it is a positive control. */
static int st_cookieimport(httrackp *opt, int argc, char **argv) {
  (void) opt;
  if (argc < 1) {
    fprintf(stderr, "cookieimport: needs a writable base dir\n");
    return 1;
  }
  char dir[HTS_URLMAXSIZE * 2];
  size_t n = (size_t) snprintf(dir, sizeof(dir), "%s", argv[0]);

  while (n > 0 && (dir[n - 1] == '/' || dir[n - 1] == '\\')) {
    dir[--n] = '\0';
  }
  const size_t base = n;
  static const char nseg[] = "/\xC3\xA9\xE4\xB8\xAD-cookie-seg";
  static const char seg[] = "/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

  memcpybuff(dir + n, nseg, sizeof(nseg));
  n += sizeof(nseg) - 1;
  if (MKDIR(dir) != 0 && errno != EEXIST) {
    fprintf(stderr, "cookieimport: mkdir failed: %s\n", strerror(errno));
    return 1;
  }
  while (n + sizeof(seg) - 1 < 300) {
    memcpybuff(dir + n, seg, sizeof(seg));
    n += sizeof(seg) - 1;
    if (MKDIR(dir) != 0 && errno != EEXIST) {
      fprintf(stderr, "cookieimport: mkdir failed at %u: %s\n", (unsigned) n,
              strerror(errno));
      return 1;
    }
  }
  const size_t dirlen = n;

  assertf(dirlen > 260); /* the cookie folder itself exceeds MAX_PATH */

  char fpath[HTS_URLMAXSIZE * 2];
  char file[HTS_URLMAXSIZE * 2];

  assertf(sprintfbuff(fpath, "%s/", dir)); /* IE glob wants a trailing sep */

  /* cookies.txt: one Netscape record (host, _, path, _, _, name, value). */
  assertf(sprintfbuff(file, "%scookies.txt", fpath));
  {
    FILE *fp = FOPEN(file, "wb");

    assertf(fp != NULL);
    fprintf(fp, "www.example.com\tFALSE\t/\tFALSE\t0\tJARCOOK\tjarval\n");
    fclose(fp);
  }

  /* A copied IE cookie u@v.txt: name, value, url, then 6 unused fields. */
  assertf(sprintfbuff(file, "%su@v.txt", fpath));
  {
    FILE *fp = FOPEN(file, "wb");

    assertf(fp != NULL);
    fprintf(fp, "IECOOK\nieval\nwww.example.com/\n0\n0\n0\n0\n0\n*\n");
    fclose(fp);
  }

  static t_cookie ck;

  ck.max_len = (int) sizeof(ck.data);
  ck.data[0] = '\0';
  assertf(cookie_load(NULL, &ck, fpath, "cookies.txt") == 0);
  assertf(strstr(ck.data, "JARCOOK") != NULL); /* jar read on a long path */
#ifdef _WIN32
  /* the IE scan merged the cookie and unlinked the consumed file */
  assertf(strstr(ck.data, "IECOOK") != NULL);
  assertf(FOPEN(file, "rb") == NULL);
#endif

  (void) UNLINK(file); /* u@v.txt (already gone on Windows) */
  assertf(sprintfbuff(file, "%scookies.txt", fpath));
  (void) UNLINK(file);
  dir[dirlen] = '\0';
  while (strlen(dir) > base) {
    char *const slash = strrchr(dir, '/');

    if (RMDIR(dir) != 0) {
      fprintf(stderr, "cookieimport: rmdir failed: %s\n", strerror(errno));
      return 1;
    }
    if (slash == NULL || (size_t) (slash - dir) < base) {
      break;
    }
    *slash = '\0';
  }

  printf("cookieimport: merged jar%s under a %u-char non-ASCII dir: OK\n",
#ifdef _WIN32
         " + IE cookie",
#else
         "",
#endif
         (unsigned) dirlen);
  return 0;
}

/* --changes bucket accounting and JSON escaping (#714). */
static int st_changes(httrackp *opt, int argc, char **argv) {
  String out = STRING_EMPTY;
  int err = 0;

  (void) opt;
  (void) argc;
  (void) argv;

  /* A file the crawl did not rewrite is unchanged whatever the wire said. */
  assertf(hts_changes_classify(HTS_FALSE, HTS_TRUE, HTS_FALSE, HTS_FALSE,
                               HTS_FALSE) == HTS_CHANGE_UNCHANGED);
  /* Rewritten with no previous copy: new, digests or not. */
  assertf(hts_changes_classify(HTS_TRUE, HTS_FALSE, HTS_FALSE, HTS_TRUE,
                               HTS_FALSE) == HTS_CHANGE_NEW);
  /* Digests decide, and outrank the transfer signal both ways: a server with
     no validators answers 200 with the same bytes, and a 304 can still sit in
     front of a locally damaged copy. */
  assertf(hts_changes_classify(HTS_TRUE, HTS_TRUE, HTS_FALSE, HTS_TRUE,
                               HTS_TRUE) == HTS_CHANGE_UNCHANGED);
  assertf(hts_changes_classify(HTS_TRUE, HTS_TRUE, HTS_TRUE, HTS_TRUE,
                               HTS_FALSE) == HTS_CHANGE_CHANGED);
  /* Only with no digest at all does the transfer signal get a say. */
  assertf(hts_changes_classify(HTS_TRUE, HTS_TRUE, HTS_TRUE, HTS_FALSE,
                               HTS_FALSE) == HTS_CHANGE_UNCHANGED);
  assertf(hts_changes_classify(HTS_TRUE, HTS_TRUE, HTS_FALSE, HTS_FALSE,
                               HTS_FALSE) == HTS_CHANGE_CHANGED);

#define JSON_IS(SRC, WANT)                                                     \
  do {                                                                         \
    StringClear(out);                                                          \
    hts_changes_json_string(&out, SRC);                                        \
    if (strcmp(StringBuff(out), WANT) != 0) {                                  \
      fprintf(stderr, "changes: %s -> %s, expected %s\n", #SRC,                \
              StringBuff(out), WANT);                                          \
      err = 1;                                                                 \
    }                                                                          \
  } while (0)

  JSON_IS("/a/b.html", "\"/a/b.html\"");
  JSON_IS("a\"b\\c", "\"a\\\"b\\\\c\"");
  JSON_IS("tab\there", "\"tab\\u0009here\"");
  /* Valid UTF-8 rides through; a lone Latin-1 byte, a truncated sequence and
     an overlong encoding of '/' each become U+FFFD rather than invalid JSON. */
  JSON_IS("caf\xc3\xa9", "\"caf\xc3\xa9\"");
  JSON_IS("caf\xe9", "\"caf\\ufffd\"");
  JSON_IS("\xc3", "\"\\ufffd\"");
  JSON_IS("\xc0\xaf", "\"\\ufffd\\ufffd\"");
  /* A UTF-16 surrogate half is well-formed UTF-8 by shape only. */
  JSON_IS("\xed\xa0\x80", "\"\\ufffd\\ufffd\\ufffd\"");

#undef JSON_IS
  StringFree(out);
  printf("changes self-test: %s\n", err ? "FAIL" : "OK");
  return err;
}

/* #747: a thread is outstanding from the moment hts_newthread() returns, not
   from the moment it starts running, or a wait right after the spawn joins
   nothing. One thread per round is what makes the old bug visible: the wait
   had to find the counter at zero, and a batch of spawns gives the earlier
   threads time to raise it. Unfixed, one round in two caught it, so the round
   count is what turns that into a reliable failure. */
#define THREADWAIT_N 8
#define THREADWAIT_ROUNDS 16
#define THREADWAIT_SLEEP_MS 50
#define THREADWAIT_GATE_MS 10000

static htsmutex threadwait_lock = HTSMUTEX_INIT;
static int threadwait_done = 0;
static hts_boolean threadwait_gated = HTS_FALSE;

static int threadwait_count(void) {
  int n;

  hts_mutexlock(&threadwait_lock);
  n = threadwait_done;
  hts_mutexrelease(&threadwait_lock);
  return n;
}

static void threadwait_thread(void *arg) {
  (void) arg;
  Sleep(THREADWAIT_SLEEP_MS);
  hts_mutexlock(&threadwait_lock);
  threadwait_done++;
  hts_mutexrelease(&threadwait_lock);
}

/* Stays outstanding until the gate clears, so wait_n() can be asked to leave a
   known number of live threads behind. Bounded: a wait_n() that wrongly drains
   them would otherwise never return, and hang the suite instead of failing. */
static void threadwait_gated_thread(void *arg) {
  int waited;

  (void) arg;
  for (waited = 0; waited < THREADWAIT_GATE_MS; waited += 10) {
    hts_boolean gated;

    hts_mutexlock(&threadwait_lock);
    gated = threadwait_gated;
    hts_mutexrelease(&threadwait_lock);
    if (!gated)
      break;
    Sleep(10);
  }
  hts_mutexlock(&threadwait_lock);
  threadwait_done++;
  hts_mutexrelease(&threadwait_lock);
}

static int st_backswap(httrackp *opt, int argc, char **argv) {
  (void) opt;
  (void) argc;
  (void) argv;
  return back_selftest_slot_swap();
}

/* TEST-NET-1: routed nowhere, so a connect to it stays pending. */
#define ST_BACKSTOP_DEADHOST "192.0.2.1"

/* Open a connect that is still in flight, or INVALID_SOCKET where the network
   answers for the blackhole instead of dropping. */
static T_SOC st_backstop_pending(httrackp *opt, htsblk *r) {
  T_SOC soc;

  hts_init_htsblk(r);
  soc = newhttp_addr(opt, ST_BACKSTOP_DEADHOST, r, 80, 0, 0, NULL);
  if (soc == INVALID_SOCKET)
    return INVALID_SOCKET;
  r->soc = soc;
  if (check_socket_connect(soc) != 0) {
    strcpybuff(r->msg, "the network answers for this address");
    deletehttp(r);
    return INVALID_SOCKET;
  }
  return soc;
}

/* Is the descriptor still an open socket? Clearing r.soc proves nothing about
   the connection: an abort that forgets deletehttp() leaks one fd per slot. */
static hts_boolean st_backstop_soc_open(T_SOC soc) {
  int type = 0;
  socklen_t len = (socklen_t) sizeof(type);

  return getsockopt(soc, SOL_SOCKET, SO_TYPE, (char *) &type, &len) == 0
             ? HTS_TRUE
             : HTS_FALSE;
}

static void st_backstop_slot(struct_back *sback, int p, int status,
                             const htsblk *r) {
  lien_back *const back = &sback->lnk[p];

  back->r = *r;
  back->r.location = back->location_buffer;
  back->status = status;
  back->timeout = -1; /* only the stop may end these slots */
  back->rateout = -1;
  /* poisoned, so the expected outcome cannot be the value the slot came with */
  back->r.statuscode = STATUSCODE_TIMEOUT;
  strcpybuff(back->r.msg, "untouched");
  strcpybuff(back->url_adr, ST_BACKSTOP_DEADHOST);
  strcpybuff(back->url_fil, "/stalled.html");
  /* address list already probed, as it is for a live connect: re-probing it
     would resolve, and the fd churn would spoil the closed-socket check */
  sback->connect_fallback[p].addr_count = 1;
}

/* A network that rejects TEST-NET-1 instead of dropping it answers in ms. */
#define ST_BACKSTOP_SETTLE_MS 500

/* Refill every slot: a fresh pending connect for each state that owns a socket,
   plus the poison the assertions read back. False if the blackhole answered. */
static hts_boolean st_backstop_arm(httrackp *opt, struct_back *sback,
                                   const int *status, htsblk *r, int slots,
                                   int dnsslot) {
  int i;

  for (i = 0; i < slots; i++) {
    deletehttp(&sback->lnk[i].r); /* whatever the previous round left */
    if (i == dnsslot) {
      hts_init_htsblk(&r[i]);
    } else if (st_backstop_pending(opt, &r[i]) == INVALID_SOCKET) {
      printf("backstop: SKIP (no stalled connect to " ST_BACKSTOP_DEADHOST
             ": %s)\n",
             r[i].msg);
      return HTS_FALSE;
    }
    st_backstop_slot(sback, i, status[i], &r[i]);
  }
  /* The readback above is too early to see a refusal, and back_wait() then
     ends slots the caller asserts are intact (the powerpc buildds). */
  Sleep(ST_BACKSTOP_SETTLE_MS);
  for (i = 0; i < slots; i++) {
    if (i != dnsslot && check_socket_connect(sback->lnk[i].r.soc) != 0) {
      printf("backstop: SKIP (the network answered for " ST_BACKSTOP_DEADHOST
             " within %d ms)\n",
             ST_BACKSTOP_SETTLE_MS);
      return HTS_FALSE;
    }
  }
  return HTS_TRUE;
}

/* A user stop must drop every slot but the FTP one (#1073, #1110). */
static int st_backstop(httrackp *opt, int argc, char **argv) {
  /* the states a stop sweeps, pre-connect then receive, and last the FTP one
     whose socket another thread owns */
  enum {
    SLOT_DNS = 0,
    SLOT_CONNECT,
    SLOT_SSL,
    SLOT_HEADERS,
    SLOT_XFER,
    SLOT_CHUNK,
    SLOT_FTP,
    SLOTS
  };

  static const int status[SLOTS] = {
      STATUS_WAIT_DNS,     STATUS_CONNECTING, STATUS_SSL_WAIT_HANDSHAKE,
      STATUS_WAIT_HEADERS, STATUS_TRANSFER,   STATUS_CHUNK_WAIT,
      STATUS_FTP_TRANSFER};
  T_SOC swept[SLOTS];
  htsblk r[SLOTS];
  struct_back *sback = NULL;
  cache_back cache;
  lien_back *back;
  int err = 0;
  int skipped = 0;
  int round;
  int i;

  (void) argc;
  (void) argv;

  /* no quota may fire instead of the stop, and no slot may time out */
  opt->maxtime = opt->maxsite = 0;
  opt->timeout = 0;
  opt->state.stop = 0;

  memset(&cache, 0, sizeof(cache));

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("  FAIL line %d: %s\n", __LINE__, #cond);                         \
      err = 1;                                                                 \
    }                                                                          \
  } while (0)

  cache.hashtable = coucal_new(0);
  sback = back_new(opt, SLOTS);
  back = sback->lnk;
  if (!st_backstop_arm(opt, sback, status, r, SLOTS, SLOT_DNS)) {
    skipped = 1;
    goto cleanup;
  }

  /* Control: with the mirror running, back_wait ends nothing. A sweep that
     fired unconditionally would strand every connect the crawl opens. Only the
     states back_wait leaves alone are checked; it advances the resolution and
     handshake waits by itself, the first to connect and the second to an error
     for want of a TLS session this fixture cannot build. */
  back_wait(sback, opt, &cache, 0);
  CHECK(back[SLOT_CONNECT].status == STATUS_CONNECTING);
  for (i = SLOT_HEADERS; i < SLOTS; i++)
    CHECK(back[i].status == status[i]);
  for (i = SLOT_CONNECT; i < SLOTS; i++) {
    if (i == SLOT_SSL)
      continue;
    CHECK(back[i].r.statuscode == STATUSCODE_TIMEOUT);
    CHECK(strcmp(back[i].r.msg, "untouched") == 0);
  }

  /* Twice, re-armed in between: the sweep runs on every wait, not once. A slot
     that was connecting when the user hit ^C is the case under test, so the
     flag goes up after the slots are in place. */
  for (round = 0; round < 2 && !err; round++) {
    opt->state.stop = 0;
    if (!st_backstop_arm(opt, sback, status, r, SLOTS, SLOT_DNS)) {
      skipped = 1;
      goto cleanup;
    }
    for (i = 0; i < SLOTS; i++)
      swept[i] = r[i].soc;
    hts_request_stop(opt, 0);

    back_wait(sback, opt, &cache, 0);

    /* every slot but the FTP one is gone, its socket closed rather than merely
       forgotten, and reported as fatal so the link is not queued again */
    for (i = SLOT_DNS; i < SLOT_FTP; i++) {
      CHECK(back[i].status == STATUS_READY);
      CHECK(back[i].r.soc == INVALID_SOCKET);
      CHECK(back[i].r.statuscode == STATUSCODE_INVALID);
      CHECK(strcmp(back[i].r.msg, "mirror stopped by user") == 0);
      if (swept[i] != INVALID_SOCKET)
        CHECK(!st_backstop_soc_open(swept[i]));
    }
    /* control: an FTP worker is still writing through this slot, so a sweep
       reaching it frees a socket and a buffer under a live thread */
    CHECK(back[SLOT_FTP].status == status[SLOT_FTP]);
    CHECK(back[SLOT_FTP].r.soc == swept[SLOT_FTP]);
    CHECK(st_backstop_soc_open(back[SLOT_FTP].r.soc));
    CHECK(back[SLOT_FTP].r.statuscode == STATUSCODE_TIMEOUT);
    CHECK(strcmp(back[SLOT_FTP].r.msg, "untouched") == 0);
  }

  /* A cap raises the stop flag itself and leaves the transfers already running
     a grace (#77, #481): the sweep takes the pre-connect slots only until the
     grace overruns, and back_wait's own limit block ends the rest. */
  if (!err) {
    const LLint recv_was = HTS_STAT.HTS_TOTAL_RECV;

    opt->state.stop = 0;
    if (!st_backstop_arm(opt, sback, status, r, SLOTS, SLOT_DNS)) {
      skipped = 1;
      goto cleanup;
    }
    for (i = 0; i < SLOTS; i++)
      swept[i] = r[i].soc;
    /* cap reached, a tenth of it still to overrun before the hard stop */
    HTS_STAT.HTS_TOTAL_RECV = 1000;
    opt->maxsite = 1000;
    hts_request_stop(opt, 0);

    back_wait(sback, opt, &cache, 0);

    for (i = SLOT_DNS; i < SLOT_HEADERS; i++) {
      CHECK(back[i].status == STATUS_READY);
      CHECK(back[i].r.statuscode == STATUSCODE_INVALID);
    }
    for (i = SLOT_HEADERS; i < SLOTS; i++) {
      CHECK(back[i].status == status[i]);
      CHECK(back[i].r.soc == swept[i]);
      CHECK(st_backstop_soc_open(back[i].r.soc));
      CHECK(back[i].r.statuscode == STATUSCODE_TIMEOUT);
      CHECK(strcmp(back[i].r.msg, "untouched") == 0);
    }
    opt->maxsite = 0;
    HTS_STAT.HTS_TOTAL_RECV = recv_was;
  }
#undef CHECK

cleanup:
  opt->maxsite = 0;
  opt->state.stop = 0;
  for (i = 0; i < SLOTS; i++)
    deletehttp(&back[i].r);
  back_free(&sback);
  coucal_delete(&cache.hashtable);
  if (skipped)
    return 77;

  printf("backstop self-test: %s\n", err ? "FAIL" : "OK");
  return err;
}

static int st_threadwait(httrackp *opt, int argc, char **argv) {
  int err = 0;
  int i, round;

  (void) opt;
  (void) argc;
  (void) argv;

  /* htsthread_wait() joins a thread spawned just before it */
  for (round = 0; round < THREADWAIT_ROUNDS && !err; round++) {
    hts_mutexlock(&threadwait_lock);
    threadwait_done = 0;
    hts_mutexrelease(&threadwait_lock);
    if (hts_newthread(threadwait_thread, NULL) != 0) {
      fprintf(stderr, "threadwait: cannot spawn\n");
      return 1;
    }
    htsthread_wait();
    if (threadwait_count() != 1) {
      fprintf(stderr, "threadwait: round %d returned before the thread ran\n",
              round);
      err = 1;
    }
  }

  /* htsthread_wait_n(n) leaves n behind rather than draining everything */
  hts_mutexlock(&threadwait_lock);
  threadwait_done = 0;
  threadwait_gated = HTS_TRUE;
  hts_mutexrelease(&threadwait_lock);
  for (i = 0; i < THREADWAIT_N; i++) {
    if (hts_newthread(threadwait_gated_thread, NULL) != 0) {
      fprintf(stderr, "threadwait: cannot spawn a gated thread\n");
      return 1;
    }
  }
  htsthread_wait_n(THREADWAIT_N);
  if (threadwait_count() != 0) {
    fprintf(stderr, "threadwait: wait_n(%d) joined %d gated threads\n",
            THREADWAIT_N, threadwait_count());
    err = 1;
  }
  hts_mutexlock(&threadwait_lock);
  threadwait_gated = HTS_FALSE;
  hts_mutexrelease(&threadwait_lock);
  htsthread_wait();
  if (threadwait_count() != THREADWAIT_N) {
    fprintf(stderr, "threadwait: wait left %d/%d gated threads running\n",
            THREADWAIT_N - threadwait_count(), THREADWAIT_N);
    err = 1;
  }

  printf("threadwait self-test: %s\n", err ? "FAIL" : "OK");
  return err;
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

#define CHANGES_RACE_FILES 8
#define CHANGES_RACE_ROUNDS 400

static void changes_race_notify(httrackp *opt, int n) {
  char fil[64];
  char BIGSTK save[HTS_URLMAXSIZE * 2];

  snprintf(fil, sizeof(fil), "/f%d.bin", n);
  strlcpybuff(save, StringBuff(opt->path_html), sizeof(save));
  strlcatbuff(save, "race.example", sizeof(save));
  strlcatbuff(save, fil, sizeof(save));
  hts_changes_notify(opt, "race.example", fil, save, HTS_TRUE, HTS_FALSE);
}

static htsmutex changes_race_lock = HTSMUTEX_INIT;
static int changes_race_started = 0;

static int changes_race_count(int *which) {
  int n;

  hts_mutexlock(&changes_race_lock);
  n = *which;
  hts_mutexrelease(&changes_race_lock);
  return n;
}

static void changes_race_thread(void *arg) {
  httrackp *const opt = (httrackp *) arg;
  int i;

  hts_mutexlock(&changes_race_lock);
  changes_race_started++;
  hts_mutexrelease(&changes_race_lock);
  for (i = 0; i < CHANGES_RACE_ROUNDS; i++)
    changes_race_notify(opt, i % CHANGES_RACE_FILES);
}

/* A transfer thread the crawl never joins (FTP) reaches hts_changes_notify()
   while the report is being resolved and written. Run it under TSan. */
static int st_changes_race(httrackp *opt, int argc, char **argv) {
  String out = STRING_EMPTY;
  char base[HTS_URLMAXSIZE];
  int err = 0;
  int i;

  if (argc < 1) {
    fprintf(stderr, "usage: -#test=changes-race <writable directory>\n");
    return 1;
  }
  strcpybuff(base, argv[0]);
  if (base[0] != '\0' && hts_lastchar(base) != '/')
    strcatbuff(base, "/");
  StringCopy(opt->path_html, base);
  StringCopy(opt->path_html_utf8, base);
  StringCopy(opt->path_log, base);
  opt->changes = HTS_TRUE;
  hts_changes_free_opt(opt);

  /* Real files, so the reader hashes and stats them for as long as it takes. */
  {
    char BIGSTK dir[HTS_URLMAXSIZE * 2];

    strlcpybuff(dir, base, sizeof(dir));
    strlcatbuff(dir, "race.example", sizeof(dir));
    for (i = 0; i < CHANGES_RACE_FILES; i++) {
      char BIGSTK path[HTS_URLMAXSIZE * 2];
      char name[64];
      FILE *fp;
      int n;

      snprintf(name, sizeof(name), "/f%d.bin", i);
      strlcpybuff(path, dir, sizeof(path));
      strlcatbuff(path, name, sizeof(path));
      structcheck(path);
      fp = FOPEN(path, "wb");
      if (fp == NULL) {
        fprintf(stderr, "changes-race: cannot write %s\n", path);
        return 1;
      }
      for (n = 0; n < 16384; n++)
        fwrite("0123456789abcdef", 1, 16, fp);
      fclose(fp);
    }
  }

  /* Take both locks once here: hts_mutexlock() initializes lazily, and two
     threads reaching a fresh one together race on the init itself. */
  hts_mutexlock(&changes_race_lock);
  changes_race_started = 0;
  hts_mutexrelease(&changes_race_lock);
  for (i = 0; i < 4; i++) {
    if (hts_newthread(changes_race_thread, opt) != 0) {
      fprintf(stderr, "changes-race: cannot spawn a notifier thread\n");
      return 1;
    }
  }
  /* Report only once they are all notifying, or there is nothing to race. */
  while (changes_race_count(&changes_race_started) < 4)
    Sleep(10);
  for (i = 0; i < 64; i++)
    hts_changes_report(opt, &out);
  hts_changes_close_opt(opt);
  htsthread_wait();

  /* Sealed: a straggler must be dropped, not start a report nobody writes. */
  changes_race_notify(opt, CHANGES_RACE_FILES + 1);
  hts_changes_report(opt, &out);
  if (StringLength(out) == 0) {
    fprintf(stderr, "changes-race: the report was lost after close\n");
    err = 1;
  } else if (strstr(StringBuff(out), "f9.bin") != NULL) {
    fprintf(stderr, "changes-race: a post-close notify reached the report\n");
    err = 1;
  }

  StringFree(out);
  hts_changes_free_opt(opt);
  printf("changes-race self-test: %s\n", err ? "FAIL" : "OK");
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
    {"filtercap", "", "an over-long filter rule is refused, not stored dead",
     st_filtercap},
    {"simplify", "<path>", "collapse ./ and ../ in a path", st_simplify},
    {"expandhome", "<path>", "expand a leading ~/ into $HOME", st_expandhome},
    {"stripquery", "", "--strip-query pattern/key stripping self-test",
     st_stripquery},
    {"urlhack", "", "-%u url-hack sub-flag (www/slash/query) self-test",
     st_urlhack},
    {"optalias", "[-list | <option> [<value>]]",
     "long-option alias expansion (--index=0 and friends)", st_optalias},
    {"hostalias", "", "--host-alias hostname folding self-test", st_hostalias},
    {"hashkey-bounds", "", "dedup key holds a maximal host+path (#1160)",
     st_hashkey_bounds},
    {"redirect-samefile", "", "same-file redirect detection self-test (#159)",
     st_redirect_samefile},
    {"wizardfilter", "[<answer> <adr> <fil> [up [slot]]]",
     "filter emitted by a wizard answer", st_wizardfilter},
    {"wizardscope", "[<question>]",
     "domain scopes the wizard can offer for a host", st_wizardscope},
    {"wizardscopeanswer", "", "host-scope range of a wizard answer",
     st_wizardscopeanswer},
    {"wizardverdict", "[<answer>]", "what a wizard answer applies",
     st_wizardverdict},
    {"wizardprompt", "[<adr> <fil>]", "URL the wizard prompt asks about",
     st_wizardprompt},
    {"wizardinsert", "[<adr> <fil> [answer...] [@ filter...]]",
     "where a wizard answer lands in the filter array", st_wizardinsert},
    {"mime", "<filename>", "MIME type for a filename", st_mime},
    {"assumemime", "[128|256|1024]",
     "--assume value clipped to each MIME destination", st_assumemime},
    {"charset", "<charset> <hex:..|string>",
     "convert a string to UTF-8 from a charset", st_charset},
    {"syscharset", "", "UTF-8 <-> system codepage conversion (WIN32 only)",
     st_syscharset},
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
    {"cmdline-split", "",
     "webhttrack command-line to argv split (bounds, quoting)",
     st_cmdlinesplit},
    {"hashtable", "<count|file>", "coucal hashtable stress test", st_hashtable},
    {"strsafe", "[overflow|overflow-buff|overflow-src [str]]",
     "bounded string-op self-test", st_strsafe},
    {"strsprintf", "", "StringSprintf grows to fit at every capacity boundary",
     st_strsprintf},
    {"arena", "", "htsarena.h hands out addresses that never move", st_arena},
    {"arrays", "[overflow-capa|overflow-loop]",
     "htsarrays.h growth reaches the requested room, overflow aborts",
     st_arrays},
    {"copyopt", "", "copy_htsopt option-copy self-test", st_copyopt},
    {"lastchar", "",
     "last-char helpers never index before the buffer (#770, #781, #821)",
     st_lastchar},
    {"rtrim", "", "hts_rtrim never walks below the buffer", st_rtrim},
    {"changes", "", "--changes bucket accounting and JSON escaping (#714)",
     st_changes},
    {"changes-race", "<dir>", "--changes under a late transfer thread (#714)",
     st_changes_race},
    {"threadwait", "", "htsthread_wait() joins threads spawned just before it",
     st_threadwait},
    {"gmtime", "",
     "hts_gmtime() fills the caller's buffer, not a static (#794)", st_gmtime},
    {"localtime", "<dir>",
     "hts_localtime() and get_filetime_rfc822()'s GMT labelling (#806)",
     st_localtime},
    {"backswap", "", "which backlog slots may be swapped to the ready table",
     st_backswap},
    {"backstop", "",
     "a user stop drops the slots still waiting to connect (#1073)",
     st_backstop},
    {"pause", "", "randomized inter-file pause target self-test", st_pause},
    {"random", "", "hts_random_bytes() fills exactly the requested length",
     st_random},
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
    {"escape-control", "[hex:..|string]",
     "escape_remove_control() terminates at the compacted end",
     st_escape_control},
    {"fsize", "<dir>", "file size past the 2GB signed-32-bit wrap", st_fsize},
    {"growsize", "", "buffer capacity for a 64-bit file size (no int wrap)",
     st_growsize},
    {"addlink", "", "htsAddLink codebase walk over an empty current path",
     st_addlink},
    {"logcallback", "", "log callback must not consume the log file's va_list",
     st_logcallback},
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
    {"cache-hdrbounds", "<dir>",
     "cache header block must stay bounded at max-length fields",
     st_cache_hdrbounds},
    {"cache-urlbounds", "<dir>",
     "cache store and lookup at max-length URLs must not abort or alias",
     st_cache_urlbounds},
    {"cache-savebounds", "<dir>",
     "cached save name rebuilt under a deeper html path must fit or be refused",
     st_cache_savebounds},
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
    {"structcheck", "<dir>",
     "structcheck path guard and the <name>.txt rename it performs",
     st_structcheck},
    {"topindex", "[dir]",
     "hts_buildtopindex charset handling of a non-ASCII project dir",
     st_topindex},
    {"datadir", "<dir>",
     "data directory resolution: compiled-in path, then the executable's tree",
     st_datadir},
    {"pathbin", "", "print the data directory this run resolved at startup",
     st_pathbin},
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
    {"sitemap", "",
     "sitemap <loc> extraction, caps and robots.txt Sitemap:", st_sitemap},
    {"ftp-line", "", "get_ftp_line bounds a hostile FTP reply line",
     st_ftpline},
    {"ftp-userpass", "", "ftp_split_userpass bounds URL userinfo", st_ftpuser},
    {"ftp-ctrlchars", "", "send_line rejects a control byte in an FTP command",
     st_ftpctrl},
    {"ftp-cmdlen", "",
     "an FTP command too long for its control line is refused", st_ftpcmdlen},
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
    {"warc-longurl", "<dir>",
     "a URL past the header-format buffer still reaches the archive",
     st_warc_longurl},
    {"longpath", "<dir>",
     "round-trip a >MAX_PATH file through the _w* wrappers (\\\\?\\ on "
     "Windows)",
     st_longpath},
    {"mirrorio", "<dir>",
     "round-trip a long+non-ASCII path through the mirror I/O wrappers",
     st_mirrorio},
    {"renameover", "<dir>",
     "hts_rename_over(): replace dst, but never delete a dst it did not "
     "replace",
     st_renameover},
    {"refetchbackup", "<dir>",
     "the re-fetch backup always leaves a copy, and stays out of the mirror",
     st_refetchbackup},
    {"spoolname", "<dir>",
     "a frozen backlog slot spools outside the mirror namespace", st_spoolname},
    {"direnum", "<dir>",
     "enumerate a long+non-ASCII directory through opendir/readdir",
     st_direnum},
    {"cookieimport", "<dir>",
     "load a jar (and Windows IE cookies) from a long+non-ASCII folder",
     st_cookieimport},
    {"warc-cdx", "<dir>", "--warc-cdx CDXJ index: sorted, offsets inflate",
     st_warc_cdx},
    {"warc-cdx-errors", "<dir>",
     "--warc-cdx diagnostics when the index cannot be written or is empty",
     st_warc_cdx_errors},
    {"warc-teardown", "<dir>",
     "a closed or abandoned archive is not reopened by a late transaction",
     st_warc_teardown},
#if HTS_USEOPENSSL
    {"warc-wacz", "<dir>", "--wacz package: layout, STORE mode, sha256 digests",
     st_warc_wacz},
#endif
    {"singlefile", "<dir>",
     "--single-file: what is inlined, the per-asset cap, idempotence",
     st_singlefile},
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
