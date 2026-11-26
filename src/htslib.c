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
/* File: Subroutines                                            */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

// Fichier librairie .c

#include "htscore.h"
#include "htsbundle.h"
#include "htssitemap.h"
#include "htswarc.h"
#include "htschanges.h"
#include "htssinglefile.h"
#include "htsfetch.h"
#include "htspsl.h"

/* specific definitions */
#include "htsdate.h"
#include "htsbase.h"
#include "htsio.h"
#include "htsnet.h"
#include "htsbauth.h"
#include "htsthread.h"
#include "htscrashtest.h"
#include "htsback.h"
#include "htsftp.h"
#include "htswrap.h"
#include "htsmd5.h"
#include "htsmodules.h"
#include "htscharset.h"
#include "htsencoding.h"
#include "htsescape.h"
#include "htscodec.h"
#include "htslines.h"
#include "htsckd.h"
#include "htsbuf.h"

#include <limits.h>

/* macOS reaches Multipath TCP through connectx() on an AF_MULTIPATH socket. */
#if HTS_INET_MPTCP && defined(__APPLE__)
#define HTS_INET_MPTCP_DARWIN 1
#ifndef AF_MULTIPATH
/* The macOS SDK lacks it, and a wrong value only leaves Multipath TCP off. */
#define AF_MULTIPATH 39
#endif
#else
#define HTS_INET_MPTCP_DARWIN 0
#endif
#if HTS_INET_MPTCP && !HTS_INET_MPTCP_DARWIN
#include <sys/utsname.h>
#endif

/* Without this header Multipath TCP still works but goes unreported, which is
   also where macOS stands. */
#if HTS_INET_MPTCP && defined(HAVE_LINUX_MPTCP_H)
#include <linux/mptcp.h>
#define HTS_INET_MPTCP_INFO 1
#else
#define HTS_INET_MPTCP_INFO 0
#endif

#ifdef _WIN32
#include <direct.h>
#else
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#endif /* _WIN32 */
#include <stdarg.h>

#include <stdint.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#ifndef _WIN32
#include <sys/time.h>
#else
#include <sys/timeb.h>
#endif
#include <fcntl.h>

// pour utimbuf
#ifdef _WIN32
#include <sys/utime.h>
#else
#include <utime.h>
#endif /* _WIN32 */

#include <sys/stat.h>

/* END specific definitions */

static void deletesoc_r(htsblk *r);
static void time_rfc822_local(hts_buf s, struct tm *A);
/* Split byte count n into {number, unit}, both stored inside strc. */
static char **int2bytes2(strc_int2bytes2 *strc, LLint n);
/* Percent-escaper behind the escape_* family. mode selects the class: 0
   check_url, 1 in_url, 2 spc_url, 3 uri, 4 control_url, 30 uri_utf. */
static size_t x_escape_http(const char *const s, hts_buf out, const int mode);
/* Copies src to dst, writing each byte of set that lies inside escaped, a
   part of src, as %XX. An escape that does not fit is dropped whole. Returns
   HTS_FALSE when dst was too small. */
static hts_boolean hts_escape_span(hts_span src, hts_span escaped,
                                   const char *set, hts_buf dst);
static int hts_read(htsblk *r, hts_buf buff);

/* variables globales */
int _DEBUG_HEAD;
FILE *ioinfo;

#if HTS_USEOPENSSL
SSL_CTX *openssl_ctx = NULL;
#endif

// Reserved (RFC2396)
#define CIS(c,ch) ( ((unsigned char)(c)) == (ch) )
#define CHAR_RESERVED(c)                                                       \
  (CIS(c, ';') || CIS(c, '/') || CIS(c, '?') || CIS(c, ':') || CIS(c, '@') ||  \
   CIS(c, '&') || CIS(c, '=') || CIS(c, '+') || CIS(c, '$') || CIS(c, ','))
// Delimiters (RFC2396)
#define CHAR_DELIM(c)                                                          \
  (CIS(c, '<') || CIS(c, '>') || CIS(c, '#') || CIS(c, '%') || CIS(c, '\"'))
// Unwise (RFC2396)
#define CHAR_UNWISE(c)                                                         \
  (CIS(c, '{') || CIS(c, '}') || CIS(c, '|') || CIS(c, '\\') || CIS(c, '^') || \
   CIS(c, '[') || CIS(c, ']') || CIS(c, '`'))
// Special (escape chars) (RFC2396 + >127 )
#define CHAR_LOW(c)       ( ((unsigned char)(c) <= 31) )
#define CHAR_HIG(c)       ( ((unsigned char)(c) >= 127) )
#define CHAR_SPECIAL(c)   ( CHAR_LOW(c) || CHAR_HIG(c) )
// We try to avoid them and encode them instead
#define CHAR_XXAVOID(c)                                                        \
  (CIS(c, ' ') || CIS(c, '*') || CIS(c, '\'') || CIS(c, '\"') ||               \
   CIS(c, '&') || CIS(c, '!'))
#define CHAR_MARK(c)                                                           \
  (CIS(c, '-') || CIS(c, '_') || CIS(c, '.') || CIS(c, '!') || CIS(c, '~') ||  \
   CIS(c, '*') || CIS(c, '\'') || CIS(c, '(') || CIS(c, ')'))

// Initialize a htsblk structure
void hts_init_htsblk(htsblk * r) {
  memset(r, 0, sizeof(htsblk)); // effacer
  r->soc = INVALID_SOCKET;
  r->msg[0] = '\0';
  r->statuscode = STATUSCODE_INVALID;
  r->totalsize = -1;
  r->retry_after = -1;
}

T_SOC http_xfopen(httrackp *opt, const char *adr, const char *fil,
                  htsblk *retour) {
  T_SOC soc = INVALID_SOCKET;
  char BIGSTK tempo_fil[HTS_URLMAXSIZE * 2];

  // default result: error
  retour->adr = NULL;
  retour->size = 0;
  retour->msg[0] = '\0';
  retour->statuscode = STATUSCODE_NON_FATAL;

  /* no proxy, or proxy not usable here (local file) */
  if ((!(retour->req.proxy.active)) || (strcmp(adr, "file://") == 0)) {
    soc = newhttp(opt, adr, retour, -1);
  } else {
    // to the proxy; https tunnels to the origin via CONNECT in back_wait
    // (#85)
    soc = newhttp(opt, retour->req.proxy.name, retour, retour->req.proxy.port);
  }

  retour->soc = soc;

  /* Check for errors */
  if (soc == INVALID_SOCKET) {
    if (!strnotempty(retour->msg)) {
      const int last_errno = hts_socket_errno();
      char errbuf[HTS_STRERROR_SIZE];

      htsblk_failf(retour, "Connect error: %s",
                   hts_strerror(last_errno, HTS_BUF(errbuf)));
    }
  }
  // a local file: LOCAL_SOCKET_ID is a pseudo-socket, and no proxy applies
  if (soc == LOCAL_SOCKET_ID) {
    retour->is_file = 1;

    // file:///C|... : retry without the leading slash
    if (!fexist(fconv(
            OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
            unescape_http(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), fil))))
      if (fexist(fconv(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                       unescape_http(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                                     fil + 1)))) {
        strcpybuff(tempo_fil, fil + 1);
        fil = tempo_fil;
      }
    retour->totalsize = fsize(
        fconv(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
              unescape_http(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), fil)));
    retour->msg[0] = '\0';
    soc = INVALID_SOCKET;
    if (retour->totalsize < 0)
      strcpybuff(retour->msg, "Unable to open local file");
    else {
      retour->fp = FOPEN(
          fconv(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                unescape_http(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt), fil)),
          "rb");
      if (retour->fp == NULL)
        soc = INVALID_SOCKET;
      else
        soc = LOCAL_SOCKET_ID;
    }
    retour->soc = soc;
    if (soc != INVALID_SOCKET) {
      retour->statuscode = HTTP_OK;
      strcpybuff(retour->msg, "OK");
      guess_httptype_sized(opt, retour->contenttype,
                           sizeof(retour->contenttype), fil);
    } else if (strnotempty(retour->msg) == 0)
      strcpybuff(retour->msg, "Unable to open local file");
    return soc;
  }

  // the header is read later, by back_wait
  if (soc != INVALID_SOCKET)
    retour->totalsize = -1;

  return soc;
}

/* Buffer printing */
typedef struct buff_struct {
  /** Buffer **/
  char *buffer;
  /** Buffer capacity in bytes **/
  size_t capacity;
  /** Buffer write position ; MUST point to a valid \0. **/
  size_t pos;
  /** Did a print not fit? Every later one is then skipped. **/
  hts_boolean overflow;
} buff_struct;

static void print_buffer(buff_struct *const str,
                         HTS_FORMAT_STRING const char *format, ...)
    HTS_PRINTF_FUN(2, 3);

/* Prints on a static buffer, raising str->overflow rather than writing past
   its end; what to do about a partial buffer is the caller's call. */
static void print_buffer(buff_struct *const str,
                         HTS_FORMAT_STRING const char *format, ...) {
  size_t result;
  va_list args;
  size_t remaining;
  char *position;

  /* Security check. */
  assertf(str != NULL);
  assertf(str->pos < str->capacity);

  if (str->overflow)
    return;

  /* Print */
  position = &str->buffer[str->pos];
  remaining = str->capacity - str->pos;
  va_start(args, format);
  result = (size_t) vsnprintf(position, remaining, format, args);
  va_end(args);
  if (result >= remaining) {
    position[0] = '\0';
    str->overflow = HTS_TRUE;
    return;
  }

  /* Increment. */
  str->pos += strlen(position);
  assertf(str->pos < str->capacity);
}

/* Append a "Cookie:" request line for every stored cookie matching adr/path.
   Only an "https://" adr gets Secure cookies, and its port is ignored. Pairs
   use the RFC 6265 form, because modern servers reject RFC 2965 $Version and
   $Path (#151). A pair that does not fit is dropped whole and counted in
   *skipped (may be NULL), so the line never ends mid-pair. The line takes all
   of bstr's remaining room, so reserve is the room the caller still needs
   after it. Returns the number emitted. */
static int append_cookie_header(buff_struct *bstr, t_cookie *cookie,
                                const char *adr, const char *path,
                                size_t reserve, int *skipped) {
  const size_t crlf_len = sizeof(H_CRLF) - 1;
  const hts_boolean https = strfield(adr, "https://") != 0;
  const int64_t now = (int64_t) time(NULL);
  const char *domain;
  char host[COOKIE_MAX_DOMAIN];
  char *b;
  int count = 0;
  size_t max_pos;

  if (skipped != NULL)
    *skipped = 0;
  if (cookie == NULL || bstr->overflow)
    return 0;
  if (!cookie_host(adr, HTS_SIZED(host)))
    return 0;
  domain = host;

  /* Highest position a pair may reach: cookies get what is left once this
     line's CRLF, the NUL and whatever the caller still owes are set aside.
     Both tests are what keep the subtractions from wrapping. */
  if (bstr->capacity <= crlf_len + 1)
    return 0;
  max_pos = bstr->capacity - crlf_len - 1;
  if (reserve >= max_pos)
    return 0;
  max_pos -= reserve;

  b = cookie->data;
  do {
    b = cookie_find(b, "", domain, path); // next matching cookie
    if (b != NULL && !cookie_sendable(b, https, now)) {
      b = cookie_nextfield(b);
    } else if (b != NULL) {
      const size_t mark = bstr->pos;
      hts_boolean kept = HTS_FALSE;

      if (mark < max_pos) {
        const hts_span name = cookie_field(b, 5), value = cookie_field(b, 6);

        if (count == 0)
          print_buffer(bstr, "Cookie: ");
        else
          print_buffer(bstr, "; ");
        print_buffer(bstr, "%.*s=%.*s", (int) name.len, name.p, (int) value.len,
                     value.p);
        kept = !bstr->overflow && bstr->pos <= max_pos;
      }
      if (kept) {
        count++;
      } else {
        /* Rewind: half a pair would be read as another cookie's value. */
        bstr->pos = mark;
        bstr->buffer[mark] = '\0';
        bstr->overflow = HTS_FALSE;
        if (skipped != NULL)
          (*skipped)++;
      }
      b = cookie_nextfield(b);
    }
  } while (b != NULL);
  if (count != 0)
    print_buffer(bstr, H_CRLF);
  return count;
}

/* Build the request Cookie line for adr/path into dst (always
   NUL-terminated). Returns the number of cookies emitted. */
int http_cookie_header(t_cookie *cookie, const char *adr, const char *path,
                       hts_buf dst) {
  buff_struct bstr = {dst.p, dst.size, 0};

  assertf(dst.p != NULL && dst.size > 0);
  dst.p[0] = '\0';
  return append_cookie_header(&bstr, cookie, adr, path, 0, NULL);
}

hts_boolean hts_body_missing_unexpectedly(const htsblk *r) {
  if (r->adr != NULL || r->is_write != 0)
    return HTS_FALSE;
  if (HTTP_IS_REDIRECT(r->statuscode) ||
      r->statuscode == HTTP_PRECONDITION_FAILED ||
      r->statuscode == HTTP_REQUESTED_RANGE_NOT_SATISFIABLE)
    return HTS_FALSE;
  return HTS_TRUE;
}

hts_span hts_blk_body(const htsblk *r) {
  return hts_span_at(r->adr,
                     r->adr != NULL && r->size > 0 ? (size_t) r->size : 0);
}

/* Read the next line of the custom block *REST, ended by CRLF, CR or LF.
   Unlike hts_header_next, a blank line does not end the block. */
static hts_boolean custom_line_next(hts_span *rest, hts_span *line) {
  const char *const p = rest->p;
  size_t n = 0;

  if (hts_span_empty(*rest))
    return HTS_FALSE;
  while (n < rest->len && p[n] != '\r' && p[n] != '\n')
    n++;
  *line = hts_span_at(p, n);
  if (n + 1 < rest->len && p[n] == '\r' && p[n + 1] == '\n')
    n++;
  *rest = hts_span_skip(*rest, n + 1);
  return HTS_TRUE;
}

/* Does LINE continue the line before it? */
static hts_boolean header_folded(hts_span line) {
  const int c = hts_span_first(line);

  return c == ' ' || c == '\t';
}

/* Servers differ on "Host : x", so the engine sends it as Host without the
   blank. */
static hts_span header_name(hts_span line) {
  hts_span name, value;

  hts_header_split(line, &name, &value);
  return name;
}

/* Cut S at its first CR or LF. */
static hts_span first_line(const char *s) {
  return hts_span_at(s, strcspn(s, "\r\n"));
}

/* Is NAME an RFC 7230 token? */
static hts_boolean header_name_valid(hts_span name) {
  size_t i;

  for (i = 0; i < name.len; i++) {
    const unsigned char c = (unsigned char) name.p[i];

    if (!(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') &&
        !(c >= '0' && c <= '9') && strchr("!#$%&'*+-.^_`|~", c) == NULL)
      return HTS_FALSE;
  }
  return !hts_span_empty(name);
}

/* No list may hold "", which would match a line with no name. */
hts_boolean header_name_in(const char *line, const char *const *names) {
  return hts_span_index_ci(header_name(first_line(line)), names) >= 0;
}

/* http_headers_have_field() for a NAME without the colon */
static hts_boolean http_headers_have_name(hts_span headers, hts_span name) {
  hts_span line;

  if (hts_span_empty(name))
    return HTS_FALSE;
  /* anchored, so a folded line or a name inside a value cannot match */
  while (custom_line_next(&headers, &line))
    /* the prefix test first, because most lines fail it */
    if (hts_span_starts_with_ci(line, name) &&
        hts_span_eq_ci(header_name(line), name))
      return HTS_TRUE;
  return HTS_FALSE;
}

hts_boolean http_headers_have_field(const char *headers,
                                    const char *field_name) {
  hts_span name = hts_span_of(field_name);

  if (hts_span_ends_with(name, hts_span_of(":"))) /* the tree spells both */
    name.len--;
  return http_headers_have_name(hts_span_of(headers), name);
}

/* These fields frame the message or act per hop. */
static hts_boolean header_refused(hts_span name) {
  static const char *const refused[] = {
      HTS_SPAN_WORDS("Content-Length", "Transfer-Encoding", "Connection",
                     "Keep-Alive", "TE", "Upgrade", "Expect")};

  return hts_span_index_ci(name, refused) >= 0;
}

/* A request carries these once, so a later custom line overrides one before. */
static hts_boolean header_single(hts_span name) {
  static const char *const single[] = {
      HTS_SPAN_WORDS("Host", "Cookie", "Authorization")};

  return hts_span_index_ci(name, single) >= 0;
}

/* An update or a resume writes these itself, dropping the custom line. */
static hts_boolean header_engine_conditional(hts_span name) {
  static const char *const conditional[] = {
      HTS_SPAN_WORDS("Range", "If-Match", "If-None-Match", "If-Modified-Since",
                     "If-Unmodified-Since")};

  return hts_span_index_ci(name, conditional) >= 0;
}

/* Find the last unfolded line of HEADERS. */
static hts_boolean last_unfolded_line(hts_span headers, hts_span *last) {
  hts_boolean found = HTS_FALSE;
  hts_span line;

  while (custom_line_next(&headers, &line))
    if (!header_folded(line)) {
      *last = line;
      found = HTS_TRUE;
    }
  return found;
}

static hts_header_verdict header_verdict(hts_span headers, hts_span line) {
  const hts_span name = header_name(line);
  hts_span parent;

  if (header_folded(line))
    /* alone, or the parent would find its own field in headers */
    return last_unfolded_line(headers, &parent)
               ? header_verdict(hts_span_of(NULL), parent)
               : HTS_HEADER_DROPPED_ORPHAN_FOLD;
  if (!header_name_valid(name))
    return HTS_HEADER_DROPPED_MALFORMED;
  if (header_refused(name))
    return HTS_HEADER_DROPPED_FRAMING;
  if (header_single(name) && http_headers_have_name(headers, name))
    return HTS_HEADER_SENT_REPLACES_EARLIER;
  if (header_engine_conditional(name))
    return HTS_HEADER_SENT_UNLESS_ENGINE_OVERRIDES;
  return HTS_HEADER_SENT;
}

HTSEXT_API hts_header_verdict hts_custom_header_verdict(const char *headers,
                                                        const char *line) {
  if (line == NULL)
    return HTS_HEADER_DROPPED_MALFORMED;
  return header_verdict(hts_span_of(headers), first_line(line));
}

const char *http_custom_header_warning(const char *headers, const char *line) {
  /* a fold's parent line already got its warning */
  if ((*line == ' ' || *line == '\t') && headers != NULL && *headers != '\0')
    return NULL;
  switch (hts_custom_header_verdict(headers, line)) {
  case HTS_HEADER_DROPPED_ORPHAN_FOLD:
    return "folded line with no header to continue ignored";
  case HTS_HEADER_DROPPED_MALFORMED:
    return "malformed header ignored";
  case HTS_HEADER_DROPPED_FRAMING:
    return "framing header ignored";
  case HTS_HEADER_SENT_REPLACES_EARLIER:
    return "header replaces an earlier one";
  case HTS_HEADER_SENT_UNLESS_ENGINE_OVERRIDES: /* valid, so a bundle may carry
                                                   it */
  case HTS_HEADER_SENT:
    break;
  }
  return NULL;
}

/* The -%X warnings, plus Host, Cookie and Proxy-*. */
const char *http_bundle_header_refusal(const char *headers, const char *line) {
  static const char *const engine[] = {HTS_SPAN_WORDS("Host", "Cookie")};
  const char *const why = http_custom_header_warning(headers, line);

  if (why != NULL)
    return why;
  if (header_name_in(line, engine))
    return "Host and Cookie are the engine's";
  if (strfield(line, "Proxy-"))
    return "a proxy header is not for the site";
  return NULL;
}

/* Dropping a line drops its folded continuations too: kept, that value would
   attach to whichever header precedes it in the request. */
static void append_custom_headers(buff_struct *bstr, const char *headers) {
  /* only what the engine wrote, so the box may repeat its own fields */
  const hts_span engine = hts_span_at(bstr->buffer, bstr->pos);
  hts_span rest = hts_span_of(headers), line;
  /* a leading folded line would attach to the engine's last header */
  hts_boolean keep = HTS_FALSE;

  while (custom_line_next(&rest, &line)) {
    const hts_span name = header_name(line);

    if (!header_folded(line))
      keep = header_name_valid(name) && !header_refused(name) &&
             !http_headers_have_name(engine, name) &&
             /* the last of a single-valued field wins, as later options do */
             !(header_single(name) && http_headers_have_name(rest, name));
    if (keep) {
      /* a folded line has no name, and a kept named line has a colon */
      const char *const from =
          line.p + (header_folded(line) ? 0 : hts_span_find(line, ':'));

      /* One write, so a line that does not fit leaves nothing behind. */
      print_buffer(bstr, "%.*s%.*s", (int) name.len, name.p,
                   (int) (rest.p - from), from);
    }
  }
}

void http_append_custom_headers(hts_buf dst, const char *headers) {
  buff_struct bstr = {dst.p, dst.size, 0};

  assertf(dst.p != NULL && dst.size > 0);
  bstr.pos = strlen(dst.p);
  append_custom_headers(&bstr, headers);
}

/* See htslib.h. Clip an oversized body, because aborting loses the mirror. */
size_t http_postfile_body(hts_buf buffer, size_t pos, FILE *fp) {
  size_t room;
  size_t got;

  assertf(pos < buffer.size);
  room = buffer.size - pos - 1; /* the NUL every consumer strlen()s up to */
  got = fread(&buffer.p[pos], 1, room, fp);
  buffer.p[pos + got] = '\0';
  return pos + strlen(&buffer.p[pos]);
}

hts_boolean hts_location_is_safe(const char *location) {
  return (location == NULL || strstr(location, POSTTOK) == NULL) ? HTS_TRUE
                                                                 : HTS_FALSE;
}

static hts_boolean hts_request_is_secure(const htsblk *retour) {
#if HTS_USEOPENSSL
  return retour->ssl ? HTS_TRUE : HTS_FALSE;
#else
  (void) retour;
  return HTS_FALSE;
#endif
}

/* Is the product token TOK named name ("Chrome" for "Chrome/141")? On a
   match, *major is the version's first number, or -1 when the version is
   malformed. */
static hts_boolean ua_product_is(hts_span tok, const char *name, int *major) {
  hts_span product, version, head, tail;
  LLint v;

  if (!hts_span_split(tok, '/', &product, &version) ||
      !hts_span_eq(product, hts_span_of(name)))
    return HTS_FALSE;
  *major = -1;
  if (!hts_span_split(version, '.', &head, &tail))
    head = version;
  if (hts_span_starts_with(version, hts_span_of("0")) ||
      !hts_span_parse_llint(head, 0, 99999, &v) ||
      !hts_span_empty(hts_span_trim(version, "0123456789.", "")))
    return HTS_TRUE;
  *major = (int) v;
  return HTS_TRUE;
}

/* Does the UA comment contain word? */
static hts_boolean ua_comment_has(hts_span comment, const char *word) {
  const hts_span w = hts_span_of(word);

  for (; comment.len >= w.len; comment = hts_span_skip(comment, 1)) {
    if (hts_span_starts_with(comment, w))
      return HTS_TRUE;
  }
  return HTS_FALSE;
}

hts_boolean hts_client_hints_of(const char *user_agent, hts_client_hints *ch) {
  /* Chromium's GREASE brand and brand order (user_agent_utils.cc), seeded by
     the major. Before M108 the formula differed. */
  enum { GREASE_FIRST_MAJOR = 108 };

  static const char grease_chars[] = " (:-./);=?_";
  static const char *const grease_versions[] = {"8", "99", "24"};
  static const int orders[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2},
                                   {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
  hts_span comment = {NULL, 0};
  int chrome = -1;
  int edge = 0; /* 0: no Edg token */
  hts_boolean mobile = HTS_FALSE, mozilla = HTS_FALSE;
  const char *p;
  char brands[3][48];
  const char *shuffled[3];
  int i;

  memset(ch, 0, sizeof(*ch));
  if (user_agent == NULL)
    return HTS_FALSE;
  /* Every product token must be one Chrome or Edge sends, so any other
     browser built on Chromium (Opera, Samsung, headless) gets no hints. */
  for (p = user_agent; *p != '\0';) {
    size_t n;
    int major;
    hts_span tok;

    if (*p == ' ') {
      p++;
    } else if (*p == '(') {
      const char *const end = strchr(p, ')');

      if (end == NULL)
        return HTS_FALSE;
      if (comment.p == NULL)
        comment = hts_span_between(p + 1, end);
      p = end + 1;
    } else {
      for (n = 0; (unsigned char) p[n] > ' ' && p[n] != '('; n++)
        ;
      tok = hts_span_at(p, n);
      if (ua_product_is(tok, "Chrome", &major))
        chrome = major;
      else if (ua_product_is(tok, "Edg", &major))
        edge = major;
      else if (ua_product_is(tok, "Mozilla", &major))
        mozilla = HTS_TRUE;
      else if (hts_span_eq(tok, hts_span_of("Mobile")))
        mobile = HTS_TRUE;
      else if (!ua_product_is(tok, "AppleWebKit", &major) &&
               !ua_product_is(tok, "Safari", &major))
        return HTS_FALSE;
      p += n;
    }
  }
  /* Edge's major is always the Chromium one. */
  if (!mozilla || chrome < GREASE_FIRST_MAJOR || (edge != 0 && edge != chrome))
    return HTS_FALSE;

  /* Android before Linux: an Android comment names both. */
  if (ua_comment_has(comment, "Android"))
    strcpybuff(ch->platform, "\"Android\"");
  else if (ua_comment_has(comment, "CrOS"))
    strcpybuff(ch->platform, "\"Chrome OS\"");
  else if (ua_comment_has(comment, "Windows"))
    strcpybuff(ch->platform, "\"Windows\"");
  else if (ua_comment_has(comment, "Macintosh"))
    strcpybuff(ch->platform, "\"macOS\"");
  else if (ua_comment_has(comment, "Linux"))
    strcpybuff(ch->platform, "\"Linux\"");
  else
    return HTS_FALSE;

  snprintf(brands[0], sizeof(brands[0]), "\"Not%cA%cBrand\";v=\"%s\"",
           grease_chars[chrome % 11], grease_chars[(chrome + 1) % 11],
           grease_versions[chrome % 3]);
  snprintf(brands[1], sizeof(brands[1]), "\"Chromium\";v=\"%d\"", chrome);
  if (edge > 0)
    snprintf(brands[2], sizeof(brands[2]), "\"Microsoft Edge\";v=\"%d\"", edge);
  else
    snprintf(brands[2], sizeof(brands[2]), "\"Google Chrome\";v=\"%d\"",
             chrome);
  for (i = 0; i < 3; i++)
    shuffled[orders[chrome % 6][i]] = brands[i];
  snprintf(ch->ua, sizeof(ch->ua), "%s, %s, %s", shuffled[0], shuffled[1],
           shuffled[2]);
  ch->mobile = mobile;
  return HTS_TRUE;
}

/* Is user_agent Firefox from 90, the first to send Sec-Fetch-*? Every product
   token must be one Firefox sends, so a fork naming itself gets no table. */
static hts_boolean ua_is_firefox(const char *user_agent) {
  enum { FETCH_META_FIRST_MAJOR = 90 };

  hts_boolean mozilla = HTS_FALSE, gecko = HTS_FALSE;
  int firefox = -1;
  const char *p;

  for (p = user_agent; *p != '\0';) {
    size_t n;
    int major;
    hts_span tok;

    if (*p == ' ') {
      p++;
    } else if (*p == '(') {
      const char *const end = strchr(p, ')');

      if (end == NULL)
        return HTS_FALSE;
      p = end + 1;
    } else {
      for (n = 0; (unsigned char) p[n] > ' ' && p[n] != '('; n++)
        ;
      tok = hts_span_at(p, n);
      if (ua_product_is(tok, "Firefox", &major))
        firefox = major;
      else if (ua_product_is(tok, "Gecko", &major))
        gecko = HTS_TRUE;
      else if (ua_product_is(tok, "Mozilla", &major))
        mozilla = HTS_TRUE;
      else
        return HTS_FALSE;
      p += n;
    }
  }
  return mozilla && gecko && firefox >= FETCH_META_FIRST_MAJOR ? HTS_TRUE
                                                               : HTS_FALSE;
}

hts_browser_family hts_browser_family_of(const char *user_agent) {
  hts_client_hints ch;

  if (user_agent == NULL)
    return HTS_BROWSER_NONE;
  if (hts_client_hints_of(user_agent, &ch))
    return HTS_BROWSER_CHROME;
  return ua_is_firefox(user_agent) ? HTS_BROWSER_FIREFOX : HTS_BROWSER_NONE;
}

/* Return fetch, or NULL when a browser would send no Sec-Fetch-*. That is the
   case for an unknown destination, -%Q0, an untrustworthy URL, and a
   User-Agent that is no current browser or that -%X replaces. An unset site
   means a hop missed hts_fetch_through, which must not read as same-origin. */
static const hts_fetch_meta *
fetch_meta_sent(httrackp *opt, const hts_fetch_meta *fetch, const char *adr,
                const htsblk *retour, const char *custom) {
  if (fetch == NULL || fetch->dest == HTS_FETCH_DEST_UNKNOWN ||
      fetch->site == HTS_FETCH_SITE_UNSET ||
      opt->browser_headers == HTS_FALSE || !hts_fetch_trustworthy(adr) ||
      !retour->req.user_agent_send || retour->req.user_agent == NULL ||
      strncmp(retour->req.user_agent, "Mozilla/5.0 ", 12) != 0 ||
      http_headers_have_field(custom, "User-Agent"))
    return NULL;
  return fetch;
}

/* A browser table may reorder these headers, which go out in this order
   without one. */
typedef enum http_header_slot {
  HDR_REFERER,
  HDR_CONTENT_LENGTH,
  HDR_CONNECTION,
  HDR_HOST,
  HDR_FROM,
  HDR_CH_UA,
  HDR_CH_UA_MOBILE,
  HDR_CH_UA_PLATFORM,
  HDR_UPGRADE_INSECURE,
  HDR_USER_AGENT,
  HDR_ACCEPT,
  HDR_FETCH_SITE,
  HDR_FETCH_MODE,
  HDR_FETCH_USER,
  HDR_FETCH_DEST,
  HDR_ACCEPT_LANGUAGE,
  HDR_ACCEPT_ENCODING,
  HDR_ORIGIN,
  HDR_COUNT
} http_header_slot;

/* A span locates one header in the scratch block, and is empty when unsent. */
typedef struct http_header_span {
  size_t start, end;
} http_header_span;

/* Chrome 155 and Firefox 158 send these orders. A header the browser never
   sends goes first (Content-length, From) or last. */
static const unsigned char header_orders[][HDR_COUNT] = {
    {HDR_REFERER, HDR_CONTENT_LENGTH, HDR_CONNECTION, HDR_HOST, HDR_FROM,
     HDR_CH_UA, HDR_CH_UA_MOBILE, HDR_CH_UA_PLATFORM, HDR_UPGRADE_INSECURE,
     HDR_USER_AGENT, HDR_ACCEPT, HDR_FETCH_SITE, HDR_FETCH_MODE, HDR_FETCH_USER,
     HDR_FETCH_DEST, HDR_ACCEPT_LANGUAGE, HDR_ACCEPT_ENCODING, HDR_ORIGIN},
    /* Chrome navigation */
    {HDR_CONTENT_LENGTH, HDR_FROM, HDR_HOST, HDR_CONNECTION, HDR_ORIGIN,
     HDR_CH_UA, HDR_CH_UA_MOBILE, HDR_CH_UA_PLATFORM, HDR_ACCEPT_LANGUAGE,
     HDR_UPGRADE_INSECURE, HDR_USER_AGENT, HDR_ACCEPT, HDR_FETCH_SITE,
     HDR_FETCH_MODE, HDR_FETCH_USER, HDR_FETCH_DEST, HDR_REFERER,
     HDR_ACCEPT_ENCODING},
    /* Chrome subresource */
    {HDR_CONTENT_LENGTH, HDR_FROM, HDR_HOST, HDR_CONNECTION, HDR_ORIGIN,
     HDR_CH_UA_PLATFORM, HDR_ACCEPT_LANGUAGE, HDR_CH_UA, HDR_UPGRADE_INSECURE,
     HDR_USER_AGENT, HDR_CH_UA_MOBILE, HDR_ACCEPT, HDR_FETCH_SITE,
     HDR_FETCH_MODE, HDR_FETCH_USER, HDR_FETCH_DEST, HDR_REFERER,
     HDR_ACCEPT_ENCODING},
    /* Firefox: Origin was never captured, so it sits where Firefox's CORS
       requests put it. */
    {HDR_CONTENT_LENGTH, HDR_FROM, HDR_HOST, HDR_USER_AGENT, HDR_ACCEPT,
     HDR_ACCEPT_LANGUAGE, HDR_ACCEPT_ENCODING, HDR_ORIGIN, HDR_CONNECTION,
     HDR_REFERER, HDR_UPGRADE_INSECURE, HDR_FETCH_DEST, HDR_FETCH_MODE,
     HDR_FETCH_SITE, HDR_FETCH_USER, HDR_CH_UA, HDR_CH_UA_MOBILE,
     HDR_CH_UA_PLATFORM}};

const unsigned char *http_header_order(hts_browser_family family, int dest) {
  switch (family) {
  case HTS_BROWSER_CHROME:
    return header_orders[hts_fetch_is_navigation(dest) ? 1 : 2];
  case HTS_BROWSER_FIREFOX:
    return header_orders[3];
  default:
    return header_orders[0];
  }
}

const char *http_header_slot_name(int slot) {
  static const char *const names[HDR_COUNT] = {"Referer",
                                               "Content-length",
                                               "Connection",
                                               "Host",
                                               "From",
                                               "sec-ch-ua",
                                               "sec-ch-ua-mobile",
                                               "sec-ch-ua-platform",
                                               "Upgrade-Insecure-Requests",
                                               "User-Agent",
                                               "Accept",
                                               "Sec-Fetch-Site",
                                               "Sec-Fetch-Mode",
                                               "Sec-Fetch-User",
                                               "Sec-Fetch-Dest",
                                               "Accept-Language",
                                               "Accept-Encoding",
                                               "Origin"};

  return slot >= 0 && slot < HDR_COUNT ? names[slot] : NULL;
}

int http_header_slot_count(void) { return HDR_COUNT; }

// envoi d'une requète
int http_sendhead(httrackp *opt, t_cookie *cookie, int mode, const char *xsend,
                  const char *adr, const char *fil, const char *referer_adr,
                  const char *referer_fil, const hts_fetch_meta *fetch,
                  htsblk *retour) {
  char BIGSTK buffer_head_request[16384];
  buff_struct bstr = { buffer_head_request, sizeof(buffer_head_request), 0 };
  /* The headers a browser table orders are rendered here before they go out. */
  char BIGSTK buffer_headers[16384];
  buff_struct hdr = {buffer_headers, sizeof(buffer_headers), 0};
  http_header_span slots[HDR_COUNT];
#define HDR_OPEN(h) (slots[h].start = hdr.pos)
#define HDR_CLOSE(h) (slots[h].end = hdr.pos)

  int direct_url = 0;           // ne pas analyser l'url (exemple: ftp://)
  const char *search_tag = NULL;
  /* What follows the headers, so the cookie line can leave it room. */
  size_t body_len = 0;

  /* the extra-headers box wins: skip the engine's own line (#1337, #1340) */
  const char *const custom = retour->req.headers;
  const hts_fetch_meta *const sent_fetch =
      fetch_meta_sent(opt, fetch, adr, retour, custom);
  /* That browser's tables pick the Accept, header order and Accept-Language. */
  const hts_browser_family family =
      sent_fetch != NULL ? hts_browser_family_of(retour->req.user_agent)
                         : HTS_BROWSER_NONE;

  /* adr and the referer come off the network and can carry raw CR/LF; capped
     at their own source buffers, so the worst case emitted does not grow. */
  char BIGSTK esc[HTS_URLMAXSIZE * 2];

  /* Only a user-supplied URL has no referer, so a crawled page cannot name
     >postfile: below and have the engine send it a local file. */
  const char *const post_fil = strnotempty(referer_adr) ? "" : fil;

  // Initialize buffer
  buffer_head_request[0] = '\0';
  buffer_headers[0] = '\0';
  memset(slots, 0, sizeof(slots));

  // Undocumented: >post: posts what follows it, >postfile: sends the file it
  // names as the raw request.
  // example: http://www.example.com/test.cgi?foo>post:posteddata=10&foo=5
  // example: http://www.example.com/test.cgi?foo>postfile:post0.txt
  search_tag = strstr(post_fil, POSTTOK ":");
  if (!search_tag) {
    search_tag = strstr(post_fil, POSTTOK "file:");
    if (search_tag) {           // postfile
      if (mode == 0) {          // GET!
        FILE *fp =
          FOPEN(unescape_http(OPT_GET_BUFF(opt), 
                OPT_GET_BUFF_SIZE(opt), search_tag + strlen(POSTTOK) + 5), "rb");
        if (fp) {
          char BIGSTK line[1100];
          char BIGSTK protocol[256], url[HTS_URLMAXSIZE * 2], method[256];

          /* a TAB may separate the request line's words */
          hts_readline(fp, HTS_BUF_N(line, 1000), HTS_LINE_DROP_FF);
          /* widths bound method[256], url[HTS_URLMAXSIZE*2], protocol[256] */
          if (sscanf(line, "%255s %2047s %255s", method, url, protocol) == 3) {
            // http proxy: absolute-URI; socks/CONNECT tunnel: origin-form
            if (retour->req.proxy.active &&
                !hts_proxy_is_socks(retour->req.proxy.name) &&
                !hts_proxy_is_connect(retour->req.proxy.name)) {
              /* only adr is raw here: the other three are %s-scanned */
              print_buffer(&bstr, "%s http://%s%s %s\r\n", method,
                           escape_check_url_addr(adr, HTS_BUF(esc)), url,
                           protocol);
            } else {
              print_buffer(&bstr,
                       "%s %s %s\r\n", method, url, protocol);
            }
            // the rest of the file goes out raw
            bstr.pos = http_postfile_body(
                hts_buf_at(bstr.buffer, bstr.capacity), bstr.pos, fp);
          }
          fclose(fp);
        }
      }
    }
  }
  // Fin postfile

  if (bstr.pos == 0) { // PAS POSTFILE
    // Type de requète?
    if ((search_tag) && (mode == 0)) {
      print_buffer(&bstr, "POST ");
    } else if (mode == 0) {     // GET
      print_buffer(&bstr, "GET ");
    } else {                    // if (mode==1) {
      if (!retour->req.http11)  // forcer HTTP/1.0
        print_buffer(&bstr, "GET ");       // certains serveurs (cgi) buggent avec HEAD
      else
        print_buffer(&bstr, "HEAD ");
    }

    // an http proxy needs an absolute URI; a socks or CONNECT tunnel does not
    if (retour->req.proxy.active &&
        !hts_proxy_is_socks(retour->req.proxy.name) &&
        !hts_proxy_is_connect(retour->req.proxy.name) &&
        (strncmp(adr, "https://", 8) != 0)) {
      if (!link_has_authority(adr)) { // default http
        print_buffer(&bstr, "http://%s",
                     escape_check_url_addr(jump_identification_const(adr),
                                           HTS_BUF(esc)));
      } else {                  // ftp:// en proxy http
        direct_url = 1;         // ne pas analyser user/pass
        print_buffer(&bstr, "%s", escape_check_url_addr(adr, HTS_BUF(esc)));
      }
    }
    // NOM DU FICHIER
    // on slash doit être présent en début, sinon attention aux bad request! (400)
    if (*fil != '/')
      print_buffer(&bstr, "/");

    {
      char BIGSTK tempo[HTS_URLMAXSIZE * 2];

      tempo[0] = '\0';
      if (search_tag)
        strncatbuff(tempo, fil, (int) (search_tag - fil));
      else
        strcpybuff(tempo, fil);
      inplace_escape_check_url(tempo, sizeof(tempo));
      print_buffer(&bstr, "%s", tempo);  // avec échappement
    }

    // protocole
    if (!retour->req.http11) { // forcer HTTP/1.0
      print_buffer(&bstr, " HTTP/1.0\x0d\x0a");
    } else { // Requète 1.1
      print_buffer(&bstr, " HTTP/1.1\x0d\x0a");
    }

    /* supplemental data */
    if (xsend)
      print_buffer(&bstr, "%s", xsend);  // éventuelles autres lignes

    // https/connect://: auth rides the CONNECT; socks: the handshake
    if (retour->req.proxy.active &&
        !hts_proxy_is_socks(retour->req.proxy.name) &&
        !hts_proxy_is_connect(retour->req.proxy.name) &&
        strncmp(adr, "https://", 8) != 0) {
      if (link_has_authorization(retour->req.proxy.name) &&
          !http_headers_have_field(custom, "Proxy-Authorization")) {
        char autorisation[1100];

        const int got = hts_basic_auth_value(opt, retour->req.proxy.name,
                                             HTS_BUF(autorisation));

        if (got > 0)
          print_buffer(&bstr, "Proxy-Authorization: Basic %s" H_CRLF,
                       autorisation);
        else if (got < 0)
          hts_log_print(opt, LOG_WARNING,
                        "proxy authorization dropped, credentials too long");
      }
    }
    // Referer?
    if (referer_adr != NULL && referer_fil != NULL && strnotempty(referer_adr)
        && strnotempty(referer_fil)
      ) {                       // non vide
      if ((strcmp(referer_adr, "file://") != 0) &&
          (/* no https referer to http urls */
           (strncmp(referer_adr, "https://", 8) != 0) /* referer is not https */
           || (strncmp(adr, "https://", 8) ==
               0) /* or referer AND addresses are https */
           ) &&
          !http_headers_have_field(custom, "Referer")) { // not file://
        /* An https or ftp address is stored with its scheme, an http one bare.
         */
        const char *const scheme =
            strncmp(referer_adr, "https://", 8) == 0 ? "https://"
            : strncmp(referer_adr, "ftp://", 6) == 0 ? "ftp://"
                                                     : "http://";

        /* one escape per piece, so neither can outgrow its own source buffer */
        HDR_OPEN(HDR_REFERER);
        print_buffer(&hdr, "Referer: %s%s", scheme,
                     escape_check_url_addr(
                         jump_identification_const(referer_adr), HTS_BUF(esc)));
        print_buffer(&hdr, "%s" H_CRLF,
                     escape_check_url_addr(referer_fil, HTS_BUF(esc)));
        HDR_CLOSE(HDR_REFERER);
      }
    }
    // HTTP field: referer
    else if (strnotempty(retour->req.referer) &&
             !http_headers_have_field(custom, "Referer")) {
      HDR_OPEN(HDR_REFERER);
      print_buffer(&hdr, "Referer: %s" H_CRLF, retour->req.referer);
      HDR_CLOSE(HDR_REFERER);
    }
    // POST?
    if (mode == 0) {            // GET!
      if (search_tag) {
        body_len =
            strlen(unescape_http(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                                 search_tag + strlen(POSTTOK) + 1));
        HDR_OPEN(HDR_CONTENT_LENGTH);
        print_buffer(&hdr, "Content-length: %d" H_CRLF, (int) body_len);
        HDR_CLOSE(HDR_CONTENT_LENGTH);
      }
    }
    // keep-alive reuses the socket
    HDR_OPEN(HDR_CONNECTION);
    if (retour->req.http11 && !retour->req.nokeepalive) {
      print_buffer(&hdr, "Connection: keep-alive" H_CRLF);
    } else {
      print_buffer(&hdr, "Connection: close" H_CRLF);
    }
    HDR_CLOSE(HDR_CONNECTION);

    {
      const char *real_adr = jump_identification_const(adr);

      // Mandatory per RFC2616
      if (!direct_url &&
          !http_headers_have_field(custom, "Host")) { // not ftp:// for example
        HDR_OPEN(HDR_HOST);
        print_buffer(&hdr, "Host: %s" H_CRLF,
                     escape_check_url_addr(real_adr, HTS_BUF(esc)));
        HDR_CLOSE(HDR_HOST);
      }

      // HTTP field: from
      if (strnotempty(retour->req.from) &&
          !http_headers_have_field(custom, "From")) { // HTTP from
        HDR_OPEN(HDR_FROM);
        print_buffer(&hdr, "From: %s" H_CRLF, retour->req.from);
        HDR_CLOSE(HDR_FROM);
      }

      /* HTTPS only, and only beside the User-Agent the hints come from. */
      if (opt->browser_headers != HTS_FALSE && hts_request_is_secure(retour) &&
          retour->req.user_agent_send &&
          !http_headers_have_field(custom, "User-Agent")) {
        hts_client_hints ch;

        if (hts_client_hints_of(retour->req.user_agent, &ch)) {
          if (!http_headers_have_field(custom, "Sec-CH-UA")) {
            HDR_OPEN(HDR_CH_UA);
            print_buffer(&hdr, "sec-ch-ua: %s" H_CRLF, ch.ua);
            HDR_CLOSE(HDR_CH_UA);
          }
          if (!http_headers_have_field(custom, "Sec-CH-UA-Mobile")) {
            HDR_OPEN(HDR_CH_UA_MOBILE);
            print_buffer(&hdr, "sec-ch-ua-mobile: ?%d" H_CRLF,
                         ch.mobile ? 1 : 0);
            HDR_CLOSE(HDR_CH_UA_MOBILE);
          }
          if (!http_headers_have_field(custom, "Sec-CH-UA-Platform")) {
            HDR_OPEN(HDR_CH_UA_PLATFORM);
            print_buffer(&hdr, "sec-ch-ua-platform: %s" H_CRLF, ch.platform);
            HDR_CLOSE(HDR_CH_UA_PLATFORM);
          }
        }
      }

      /* On https only, so a server answering it cannot move a plain http
         crawl onto https. */
      if (sent_fetch != NULL && hts_fetch_is_navigation(sent_fetch->dest) &&
          hts_request_is_secure(retour) &&
          !http_headers_have_field(custom, "Upgrade-Insecure-Requests")) {
        HDR_OPEN(HDR_UPGRADE_INSECURE);
        print_buffer(&hdr, "Upgrade-Insecure-Requests: 1" H_CRLF);
        HDR_CLOSE(HDR_UPGRADE_INSECURE);
      }

      if (retour->req.user_agent_send && strnotempty(retour->req.user_agent) &&
          !http_headers_have_field(custom, "User-Agent")) {
        HDR_OPEN(HDR_USER_AGENT);
        print_buffer(&hdr, "User-Agent: %s" H_CRLF, retour->req.user_agent);
        HDR_CLOSE(HDR_USER_AGENT);
      }

      /* dest comes from fetch, so a KIND= entry applies without Sec-Fetch-*. */
      {
        char accept_buf[HTS_ACCEPT_MAXSIZE];
        const char *accept;

        /* held until printed: accept may point into accept_kinds */
        hts_mutexlock(&opt->state.lock);
        accept = hts_accept_lookup(StringBuff(opt->accept_kinds),
                                   retour->req.accept, opt->accept_given,
                                   fetch != NULL ? fetch->dest
                                                 : HTS_FETCH_DEST_UNKNOWN,
                                   family, HTS_BUF(accept_buf));
        if (strnotempty(accept) && !http_headers_have_field(custom, "Accept")) {
          HDR_OPEN(HDR_ACCEPT);
          print_buffer(&hdr, "Accept: %s" H_CRLF, accept);
          HDR_CLOSE(HDR_ACCEPT);
        }
        hts_mutexrelease(&opt->state.lock);
      }

      if (sent_fetch != NULL) {
        if (!http_headers_have_field(custom, "Sec-Fetch-Site")) {
          HDR_OPEN(HDR_FETCH_SITE);
          print_buffer(&hdr, "Sec-Fetch-Site: %s" H_CRLF,
                       hts_fetch_site_name(sent_fetch->site));
          HDR_CLOSE(HDR_FETCH_SITE);
        }
        if (!http_headers_have_field(custom, "Sec-Fetch-Mode")) {
          HDR_OPEN(HDR_FETCH_MODE);
          print_buffer(&hdr, "Sec-Fetch-Mode: %s" H_CRLF,
                       hts_fetch_mode_name(sent_fetch));
          HDR_CLOSE(HDR_FETCH_MODE);
        }
        if (sent_fetch->user &&
            !http_headers_have_field(custom, "Sec-Fetch-User")) {
          HDR_OPEN(HDR_FETCH_USER);
          print_buffer(&hdr, "Sec-Fetch-User: ?1" H_CRLF);
          HDR_CLOSE(HDR_FETCH_USER);
        }
        if (!http_headers_have_field(custom, "Sec-Fetch-Dest")) {
          HDR_OPEN(HDR_FETCH_DEST);
          print_buffer(&hdr, "Sec-Fetch-Dest: %s" H_CRLF,
                       hts_fetch_dest_name(sent_fetch->dest));
          HDR_CLOSE(HDR_FETCH_DEST);
        }
      }

      /* A browser table replaces the built-in -%l, never the user's. */
      if (strnotempty(retour->req.lang_iso) &&
          !http_headers_have_field(custom, "Accept-Language")) {
        const char *lang = retour->req.lang_iso;

        if (family != HTS_BROWSER_NONE && !opt->lang_given)
          lang = HTS_ACCEPT_LANGUAGE_BROWSER;
        HDR_OPEN(HDR_ACCEPT_LANGUAGE);
        print_buffer(&hdr, "Accept-Language: %s" H_CRLF, lang);
        HDR_CLOSE(HDR_ACCEPT_LANGUAGE);
      }

      // Compression accepted ?
      if (retour->req.http11 &&
          !http_headers_have_field(custom, "Accept-Encoding")) {
        hts_boolean compressible =
            (!retour->req.range_used && !retour->req.nocompression);

        HDR_OPEN(HDR_ACCEPT_ENCODING);
        print_buffer(
            &hdr, "Accept-Encoding: %s" H_CRLF,
            hts_acceptencoding(compressible, hts_request_is_secure(retour)));
        HDR_CLOSE(HDR_ACCEPT_ENCODING);
      }

      /* Chrome sends a cors request's Origin even same-origin, Firefox only
         cross-origin. */
      if (sent_fetch != NULL && family != HTS_BROWSER_NONE &&
          strcmp(hts_fetch_mode_name(sent_fetch), "cors") == 0 &&
          !http_headers_have_field(custom, "Origin")) {
        const char *const from =
            sent_fetch->initiator > 0 && sent_fetch->initiator < opt->lien_tot
                ? heap(sent_fetch->initiator)->adr
                : NULL;
        char origin[HTS_URLMAXSIZE * 2];

        if (from != NULL && hts_fetch_origin(from, HTS_BUF(origin)) &&
            (family == HTS_BROWSER_CHROME ||
             hts_fetch_site_of(from, adr) != HTS_FETCH_SITE_SAME_ORIGIN)) {
          HDR_OPEN(HDR_ORIGIN);
          print_buffer(&hdr, "Origin: %s" H_CRLF, origin);
          HDR_CLOSE(HDR_ORIGIN);
        }
      }

      /* Each header in the order the browser sends them. */
      {
        const unsigned char *const order = http_header_order(
            family,
            sent_fetch != NULL ? sent_fetch->dest : HTS_FETCH_DEST_UNKNOWN);
        int i;

        /* Only a library caller can set opt strings past the argv caps. */
        if (hdr.overflow)
          bstr.overflow = HTS_TRUE;
        for (i = 0; i < HDR_COUNT; i++) {
          const http_header_span *const h = &slots[order[i]];

          if (h->end > h->start)
            print_buffer(&bstr, "%.*s", (int) (h->end - h->start),
                         hdr.buffer + h->start);
        }
      }
#undef HDR_OPEN
#undef HDR_CLOSE
      /* Authentification */
      {
        char autorisation[1100];
        const char *a;

        autorisation[0] = '\0';
        if (link_has_authorization(adr)) {      // ohh une authentification!
          const char *a = jump_identification_const(adr);

          if (!direct_url) {    // pas ftp:// par exemple
            const int got =
                hts_basic_auth_value(opt, adr, HTS_BUF(autorisation));

            if (got > 0) {
              if (cookie != NULL && strcmp(fil, "/robots.txt") != 0 &&
                  !bauth_add(cookie, adr, fil, autorisation))
                /* debug: it repeats on every request under this login */
                hts_log_print(opt, LOG_DEBUG,
                              "authorization not kept for later requests to %s",
                              a);
            } else if (got < 0) {
              /* 'a' is past the '@': the log gets the host, not the secret. */
              hts_log_print(
                  opt, LOG_WARNING,
                  "authorization dropped, credentials too long for %s", a);
            }
          }
        } else if ((a = bauth_check(cookie, adr, fil)))
          strcpybuff(autorisation, a);
        if (strnotempty(autorisation) &&
            !http_headers_have_field(custom, "Authorization")) {
          print_buffer(&bstr, "Authorization: Basic %s"H_CRLF, autorisation);
        }
      }
    }

    // Custom header(s), minus the fields the request already carries
    if (strnotempty(custom)) {
      append_custom_headers(&bstr, custom);
    }

    /* Stored cookies matching this host/path, last because the line takes
       whatever room the headers above left it. */
    if (cookie != NULL && !http_headers_have_field(custom, "Cookie")) {
      int skipped;

      append_cookie_header(&bstr, cookie, adr, fil,
                           sizeof(H_CRLF) - 1 + body_len, &skipped);
      if (skipped != 0) {
        hts_log_print(opt, LOG_WARNING,
                      "Request too large for %d cookie(s) of %s%s", skipped,
                      jump_identification_const(adr), fil);
      }
    }

    // CRLF de fin d'en tête
    print_buffer(&bstr, H_CRLF);

    // données complémentaires?
    if (search_tag)
      if (mode == 0)            // GET!
        print_buffer(&bstr, "%s",
                   unescape_http(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                                 search_tag + strlen(POSTTOK) + 1));
  }

  /* Nothing sums the fields above against the request buffer. A clipped
     request would fetch another resource, or drop the credentials. */
  if (bstr.overflow) {
    hts_log_print(opt, LOG_WARNING, "Request header too large for %s%s",
                  jump_identification_const(adr), fil);
    deletesoc_r(retour);
    strcpybuff(retour->msg, "Request header too large");
    retour->soc = INVALID_SOCKET;
    return -1;
  }
  if (_DEBUG_HEAD) {
    if (ioinfo) {
      hts_secret_fprintf(opt, ioinfo, "[%d] request for %s%s:\r\n",
                         retour->debugid, jump_identification_const(adr), fil);
      fprintfio(ioinfo, bstr.buffer, "<<< ", opt);
      fprintf(ioinfo, "\r\n");
      fflush(ioinfo);
    }
  }                             // Fin test pas postfile
  //

  // Stash the raw request for the WARC request record (freed at
  // back_clear_entry)
  if (StringNotEmpty(opt->warc_file))
    warc_stash_request(retour, bstr.buffer);

  // Callback
  {
    int test_head =
      RUN_CALLBACK6(opt, sendhead, bstr.buffer, adr, fil, referer_adr, referer_fil,
                    retour);
    if (test_head != 1) {
      deletesoc_r(retour);
      strcpybuff(retour->msg, "Header refused by external wrapper");
      retour->soc = INVALID_SOCKET;
      return -1;
    }
  }

  // Envoi
  HTS_STAT.last_request = mtime_local();
  if (sendc(retour, bstr.buffer) < 0) {        // ERREUR, socket rompue?...
    deletesoc_r(retour);        // fermer tout de même
    // et tenter de reconnecter

    strcpybuff(retour->msg, "Write error");
    retour->soc = INVALID_SOCKET;
  }

  // RX'98
  return 0;
}

/* The three-digit HTTP status code at s, or -1 when s does not start with
   exactly three digits. */
static int hts_status_code(const char *s) {
  /* RFC 9110 15: exactly three digits. sscanf("%d") range-checks nothing and
     glibc wraps, so "4294967496" would read as a 200. */
  if (s[0] >= '0' && s[0] <= '9' && s[1] >= '0' && s[1] <= '9' && s[2] >= '0' &&
      s[2] <= '9' && (s[3] < '0' || s[3] > '9'))
    return (s[0] - '0') * 100 + (s[1] - '0') * 10 + (s[2] - '0');
  return -1;
}

int hts_status_line_code(const char *line) {
  const char *a = line;

  while (*a == ' ' || *a == '\t' || *a == '\r' || *a == '\n')
    a++;
  if (!strfield(a, "HTTP/"))
    return -1;
  while (*a != '\0' && *a != ' ' && *a != '\t' && *a != '\r' && *a != '\n')
    a++;
  while (*a == ' ' || *a == '\t' || *a == '\r' || *a == '\n')
    a++;
  return hts_status_code(a);
}

// traiter 1ere ligne d'en tête
void treatfirstline(htsblk * retour, const char *rcvd) {
  const char *a = rcvd;

  // exemple:
  // HTTP/1.0 200 OK
  if (*a) {
    // note: certains serveurs buggés renvoient HTTP/1.0\n200 OK ou " HTTP/1.0 200 OK"
    while((*a == ' ') || (*a == 10) || (*a == 13) || (*a == 9))
      a++;                      // épurer espaces au début
    if (strfield(a, "HTTP/")) {
      // sauter HTTP/1.x
      while((*a != ' ') && (*a != '\0') && (*a != 10) && (*a != 13)
            && (*a != 9))
        a++;
      if (*a != '\0') {
        while((*a == ' ') || (*a == 10) || (*a == 13) || (*a == 9))
          a++;                  // épurer espaces
        if ((retour->statuscode = hts_status_code(a)) >= 0) {
          // skip the code
          while((*a != ' ') && (*a != '\0') && (*a != 10) && (*a != 13)
                && (*a != 9))
            a++;
          while((*a == ' ') || (*a == 10) || (*a == 13) || (*a == 9))
            a++;                // épurer espaces
          /* A lone CR does not end the line the reader stopped at, so a phrase
             kept as-is would write the log's own field separators. */
          if ((strlen(a) > 1) && (strlen(a) < 64) && hts_is_control_free(a))
            strcpybuff(retour->msg, a); // message retour
          else
            infostatuscode_sized(HTS_SIZED(retour->msg), retour->statuscode);
          // type MIME par défaut2
          strcpybuff(retour->contenttype, HTS_UNKNOWN_MIME);
        } else { // pas de code!
          retour->statuscode = STATUSCODE_INVALID;
          strcpybuff(retour->msg, "Unknown response structure");
        }
      } else {                  // euhh??
        retour->statuscode = STATUSCODE_INVALID;
        strcpybuff(retour->msg, "Unknown response structure");
      }
    } else {
      if (*a == '<') {
        /* This is dirty .. */
        retour->statuscode = HTTP_OK;
        retour->keep_alive = 0;
        strcpybuff(retour->msg, "Unknown, assuming junky server");
        strcpybuff(retour->contenttype, HTS_UNKNOWN_MIME);
      } else if (strnotempty(a)) {
        retour->statuscode = STATUSCODE_INVALID;
        strcpybuff(retour->msg, "Unknown (not HTTP/xx) response structure");
      } else {
        /* This is dirty .. */
        retour->statuscode = HTTP_OK;
        retour->keep_alive = 0;
        strcpybuff(retour->msg, "Unknown, assuming junky server");
        strcpybuff(retour->contenttype, HTS_UNKNOWN_MIME);
      }
    }
  } else {                      // vide!
    /*
       retour->statuscode=STATUSCODE_INVALID;
       strcpybuff(retour->msg,"Empty reponse or internal error");
     */
    /* This is dirty .. */
    retour->statuscode = HTTP_OK;
    strcpybuff(retour->msg, "Unknown, assuming junky server");
    strcpybuff(retour->contenttype, HTS_UNKNOWN_MIME);
  }
}

/* A signed or oversized value reads as 0, which turns keep-alive off. */
static void keep_alive_param(const char *s, int *value) {
  LLint v;
  const hts_scan_result got = hts_scan_llint(&s, 0, INT_MAX, &v);

  if (got != HTS_SCAN_NONE)
    *value = got == HTS_SCAN_OK ? (int) v : 0;
}

hts_boolean hts_content_length_agrees(const char *s, LLint *seen) {
  for (;;) {
    LLint v;
    const hts_scan_result got = hts_scan_llint(&s, 0, INT64_MAX, &v);

    if (got != HTS_SCAN_NONE) {
      if (got == HTS_SCAN_REFUSED)
        v = -2;
      if (*seen != -1 && (v < 0 || v != *seen))
        return HTS_FALSE;
      *seen = v;
    }
    s += strcspn(s, ",");
    if (*s == '\0')
      return HTS_TRUE;
    s++;
  }
}

/* RFC 6265 5.1.4 default-path of request path fil, as a length into fil below
   size, the destination capacity; 0 means "/". A directory too long backs off
   to an ancestor. */
static size_t cookie_default_path_len(const char *fil, size_t size) {
  size_t len = fil != NULL && fil[0] == '/' ? strcspn(fil, "?") : 0;

  // fil[len] is never "/" at the start, so this stops on the rightmost one
  while (len > 0 && (len >= size || fil[len] != '/'))
    len--;
  return len;
}

// Read one response header line, and store any cookie it sets.
void treathead(t_cookie * cookie, const char *adr, const char *fil, htsblk * retour,
               char *rcvd) {
  treathead_opt(NULL, cookie, adr, fil, retour, rcvd);
}

/* Is NAME the header LIT, in any case? The length test skips most calls. */
#define HEADER_IS(name, lit)                                                   \
  ((name).len == sizeof(lit) - 1 && hts_span_eq_ci((name), hts_span_of(lit)))

/* VALUE past its leading blanks. */
static hts_span header_value(const char *value) {
  return hts_span_trim(hts_span_of(value), HTS_REALSPACES, "");
}

/* A header added here also goes into hts_header_is_read(). */
void treathead_opt(httrackp *opt, t_cookie *cookie, const char *adr,
                   const char *fil, htsblk *retour, char *rcvd) {
  char *const colon = strchr(rcvd, ':');
  hts_span name;
  char *value;

  if (colon == NULL)
    return;
  name = hts_span_between(rcvd, colon);
  value = colon + 1;
  if (HEADER_IS(name, "Content-length")) {
    const char *a = value;
    LLint size;

    if (hts_scan_llint(&a, 0, INT64_MAX, &size) == HTS_SCAN_OK) {
      retour->totalsize = size;
      if (retour->totalsize == 0) {
        retour->empty = 1;
      }
    }
  } else if (HEADER_IS(name, "Content-Disposition")) {
    char tmp[256];

    if (hts_header_param(value, ";", "filename", HTS_SIZED(tmp))) {
      const char *a = tmp;
      const char *c;

      while (is_space(*a))
        a++;
      while ((c = strchr(a, '/')) != NULL) /* skip all / (see RFC2616) */
        a = c + 1;
      hts_rtrim(tmp, HTS_SPACES);
      if (strlen(a) < 200) {
        strcpybuff(retour->cdispo, a);
      }
    }
  } else if (HEADER_IS(name, "Last-Modified") ||
             (HEADER_IS(name, "Date") &&
              retour->lastmodified[0] == '\0')) { // Date stands in for it
    const hts_span v = header_value(value);

    if (v.len < sizeof(retour->lastmodified))
      (void) !hts_buf_put_span_clip(HTS_BUF(retour->lastmodified), v); // fits
  } else if (HEADER_IS(name, "Etag")) {
    if (retour) {
      const hts_span v = header_value(value);

      if (v.len < 64)
        (void) !hts_buf_put_span_clip(HTS_BUF(retour->etag), v); // fits
      else
        retour->etag[0] = '\0';
    }
  } else if (HEADER_IS(name, "Transfer-Encoding")) {
    static const char *const compression[] = {
        HTS_SPAN_WORDS("gzip", "x-gzip", "deflate")};
    hts_span rest = hts_span_of(value);
    hts_span item;

    // RFC 2616 3.6: codings in the order applied, chunked once and final
    while (hts_span_next_in(&rest, ',', &item)) {
      item = hts_span_trim(item, HTS_REALSPACES, HTS_REALSPACES);
      if (hts_span_empty(item))
        continue;
      /* a chunked before this item was not the final coding */
      if (retour->is_chunk)
        retour->transfer_coding = HTS_TE_UNREADABLE;
      retour->is_chunk = hts_span_eq_ci(item, hts_span_of("chunked"));
      if (retour->is_chunk || hts_span_eq_ci(item, hts_span_of("identity")))
        continue;
      /* one compression layer, as for Content-Encoding: a parameter, a second
         layer or an unknown coding leaves a body we can not read */
      retour->transfer_coding =
          retour->transfer_coding == HTS_TE_NONE &&
                  hts_span_index_ci(item, compression) >= 0
              ? HTS_TE_INFLATE
              : HTS_TE_UNREADABLE;
    }
    if (retour->transfer_coding != HTS_TE_NONE)
      retour->compressed = 1;
  } else if (HEADER_IS(name, "Content-type")) {
    if (retour) {
      char tempo[1100];

      char chs[256];
      char *const semi = strchr(value, ';');

      if (hts_header_param(value, ";", "charset", HTS_SIZED(chs))) {
        const char *const c = chs + strspn(chs, HTS_SPACES);
        const size_t n = strcspn(c, HTS_SPACES);

        // A charset is one token.
        if (n != 0 && n < sizeof(retour->charset) - 2) {
          retour->charset[0] = '\0';
          strlncatbuff(retour->charset, c, sizeof(retour->charset), n);
        }
      }
      if (semi != NULL)
        *semi = '\0';
      // An empty/whitespace Content-Type value yields no token: keep the
      // sentinel default rather than reading an uninitialized tempo.
      if (sscanf(value, "%1099s", tempo) == 1) { // tempo[1100], server data
        if (!hts_buf_try_cpy(
                HTS_BUF_N(retour->contenttype, sizeof(retour->contenttype) - 2),
                tempo))
          strcpybuff(retour->contenttype, "application/octet-stream-unknown");
      }
    }
  } else if (HEADER_IS(name, "Content-Range")) {
    // Content-Range: bytes 0-70870/70871
    const char *a;

    for (a = value; is_space(*a); a++)
      ;
    if (hts_strncasecmp(a, "bytes ", 6) == 0) {
      LLint start = 0, end = 0, total = 0;
      hts_scan_result got;
      hts_boolean refused;

      for(a += 6; is_space(*a); a++) ;
      got = hts_scan_llint(&a, 0, INT64_MAX, &start);
      refused = got == HTS_SCAN_REFUSED;

      // A signed or oversized field in "start-end/total" refuses the header.
      if (got != HTS_SCAN_NONE && *a == '-') {
        a++;
        got = hts_scan_llint(&a, 0, INT64_MAX, &end);
        refused = refused || got == HTS_SCAN_REFUSED;
        if (got != HTS_SCAN_NONE && *a == '/') {
          a++;
          got = hts_scan_llint(&a, 0, INT64_MAX, &total);
          refused = refused || got == HTS_SCAN_REFUSED;
        } else {
          got = HTS_SCAN_NONE;
        }
      } else {
        got = HTS_SCAN_NONE;
      }
      // Only the total after the first '/' counts, as in "*/N".
      if (got == HTS_SCAN_NONE) {
        start = end = total = 0;
        a = strchr(value, '/');
        if (a != NULL) {
          a++;
          if (hts_scan_llint(&a, 1, INT64_MAX, &total) == HTS_SCAN_OK) {
            end = total - 1;
          } else {
            total = 0;
          }
        }
      } else if (refused) {
        start = end = total = 0;
      }
      retour->crange_start = start;
      retour->crange_end = end;
      retour->crange = total;
    }
  } else if (HEADER_IS(name, "Connection")) {
    const hts_span v = hts_span_of(value);

    // a list such as "Upgrade, Keep-Alive", where close wins
    if (!hts_span_empty(header_value(value))) {
      if (hts_span_list_has(v, ',', hts_span_of("Keep-Alive")) &&
          !hts_span_list_has(v, ',', hts_span_of("close"))) {
        if (!retour->keep_alive) {
          retour->keep_alive_max = 10;
          retour->keep_alive_t = 15;
        }
        retour->keep_alive = 1;
      } else {
        retour->keep_alive = 0;
      }
    }
  } else if (HEADER_IS(name, "Keep-Alive")) {
    char *a = value;

    while(is_space(*a))
      a++;
    if (*a) {
      char v[64];

      retour->keep_alive = 1;
      retour->keep_alive_max = 10;
      retour->keep_alive_t = 15;
      if (hts_header_param(a, ",;", "timeout", HTS_SIZED(v)))
        keep_alive_param(v, &retour->keep_alive_t);
      if (hts_header_param(a, ",;", "max", HTS_SIZED(v)))
        keep_alive_param(v, &retour->keep_alive_max);
      if (retour->keep_alive_max <= 1 || retour->keep_alive_t < 1) {
        retour->keep_alive = 0;
      }
    }
  } else if (HEADER_IS(name, "TE")) {
    char *a = value;

    while(is_space(*a))
      a++;
    if (*a) {
      if (strfield(a, "trailers")) {
        retour->keep_alive_trailers = 1;
      }
    }
  } else if (HEADER_IS(name, "Content-Encoding")) {
    if (retour) {
      char tempo[1100];
      char *a = value;

      while(is_space(*a))
        a++;
      {
        char *a = strchr(value, ';');

        if (a)
          *a = '\0';
      }
      // Bound to tempo[1100]; no token => leave empty, don't read uninit tempo.
      if (sscanf(a, "%1099s", tempo) == 1 && strlen(tempo) < 64)
        strcpybuff(retour->contentencoding, tempo);
      else
        retour->contentencoding[0] = '\0';
      /* A coding to undo (or one we can not undo, which must fail the fetch
         rather than save the coded bytes as the page) */
      if (hts_codec_parse(retour->contentencoding) != HTS_CODEC_IDENTITY)
        retour->compressed = 1;
    }
  } else if (HEADER_IS(name, "Retry-After")) {
    if (retour) {
      /* uncapped: treathead() has no opt, so the caller clips it */
      retour->retry_after = hts_parse_retry_after(value, time(NULL));
    }
  } else if (HEADER_IS(name, "Location")) {
    if (retour) {
      if (retour->location) {
        const hts_span v = header_value(value);

        if (!hts_buf_put_span(hts_buf_at(retour->location, HTS_LOCATION_SIZE),
                              v)) {
          /* no opt here, so this only reaches a registered log callback */
          hts_log_print(NULL, LOG_WARNING,
                        "Location header too long (%d bytes), redirect ignored",
                        (int) v.len);
        } else if (!hts_location_is_safe(retour->location)) {
          hts_log_print(NULL, LOG_WARNING,
                        "Location naming a post token, redirect ignored");
          retour->location[0] = '\0';
        }
      }
    }
  } else if (HEADER_IS(name, "Set-Cookie") && cookie != NULL) {
    hts_span rest = hts_span_of(value), n, v;
    char domain[COOKIE_MAX_DOMAIN]; // .netscape.com
    char scoped[COOKIE_MAX_DOMAIN]; // adr without userinfo, brackets or port
    char path[COOKIE_MAX_PATH];     // /
    char defpath[COOKIE_MAX_PATH];
    char cook_name[256];        // nom cookie (MYCOOK)
    char BIGSTK cook_value[8192];       // valeur (ID=toto,S=1234)
    const int64_t now = (int64_t) time(NULL);

    /* Refuse a host cookie_host() cannot scope, and one too long for the
       default-domain copy below (that would abort the mirror). */
    if (adr != NULL &&
        (strlen(adr) >= sizeof(domain) || !cookie_host(adr, HTS_SIZED(scoped))))
      return;
    {
      char name[256];
      char BIGSTK value[8192];
      hts_boolean has_domain = HTS_FALSE;
      hts_boolean has_max_age = HTS_FALSE;
      hts_boolean has_expires = HTS_FALSE;
      int64_t max_age_when = 0, expires_when = 0;
      cookie_attr attr = {.host_only = HTS_FALSE,
                          .secure = HTS_FALSE,
                          .http_only = HTS_FALSE,
                          .expires = 0};
      hts_boolean first = HTS_TRUE;

      name[0] = value[0] = '\0';
      //

      /* raw, because cookie_set() normalises and doing that twice is not the
         same as doing it once; no adr means empty, which it refuses */
      strcpybuff(domain, adr != NULL ? adr : "");
      {
        const size_t dlen = cookie_default_path_len(fil, sizeof(defpath));

        defpath[0] = '\0';
        if (dlen == 0)
          strcpybuff(defpath, "/");
        else
          strncatbuff(defpath, fil, dlen);
      }
      strcpybuff(path, defpath);
      strcpybuff(cook_name, "");
      strcpybuff(cook_value, "");
      // read every pair up to the end of the header
      while (hts_param_next(&rest, ';', &n, &v)) {
        // a name stops at a blank or a quote, so "a b=c" is the bare word a
        const hts_span key = hts_span_trim(n, HTS_SPACES, HTS_SPACES);
        size_t word = 0;

        v = hts_span_trim(v, HTS_REALSPACES, HTS_REALSPACES);
        while (word < key.len && !is_space(key.p[word]))
          word++;
        if (hts_span_present(v) && word == key.len) {
          /* RFC 6265 4.1.1 makes the quotes part of a quoted cookie-value. */
          if (key.len > 0 && key.len < 200 && v.len < 8000) {
            /* both fit, by the bounds above */
            (void) !hts_buf_put_span_clip(HTS_BUF(name), key);
            (void) !hts_buf_put_span_clip(HTS_BUF(value), v);
            if (first) {
              // the first pair is the cookie, and "name=" may erase one
              strcpybuff(cook_name, name);
              strcpybuff(cook_value, value);
            } else if (strfield2(name, "domain")) {
              if (v.len == 0 || strcmp(value, ".") == 0) {
                // ignored, so the cookie stays on the host
              } else if (v.len < sizeof(domain) - 1) {
                strcpybuff(domain, value);
                has_domain = HTS_TRUE;
              } else {
                cook_name[0] = 0;
                break;
              }
            } else if (strfield2(name, "path")) {
              if (value[0] != '/') {
                strcpybuff(path, defpath); // RFC 6265 5.2.4
              } else if (v.len < sizeof(path) - 1) {
                strcpybuff(path, value);
              } else {
                cook_name[0] = 0;
                break;
              }
            } else if (strfield2(name, "max-age")) {
              int64_t when;

              // an invalid value is ignored, not a reset (5.3)
              if (cookie_max_age_time(value, now, &when)) {
                has_max_age = HTS_TRUE;
                max_age_when = when;
              }
            } else if (strfield2(name, "expires")) {
              int64_t when;

              if (cookie_expires_time(value, now, &when)) {
                has_expires = HTS_TRUE;
                expires_when = when;
              }
            } else if (strfield2(name, "secure")) {
              attr.secure = HTS_TRUE;
            } else if (strfield2(name, "httponly")) {
              attr.http_only = HTS_TRUE;
            }
            // RFC 6265 5.2 ignores any other attribute, SameSite for example
          }
        } else { // a bare attribute, or the word before a blank
          const hts_span attribute = hts_span_at(key.p, word);

          if (hts_span_eq_ci(attribute, hts_span_of("secure")))
            attr.secure = HTS_TRUE;
          else if (hts_span_eq_ci(attribute, hts_span_of("httponly")))
            attr.http_only = HTS_TRUE;
        }
        // an unusable first pair leaves no cookie, so the header is ignored
        first = HTS_FALSE;
      }
      if (strnotempty(cook_name)) {     // cookie?
        /* Max-Age outranks Expires (RFC 6265 5.3 step 3) */
        attr.expires =
            has_max_age ? max_age_when : (has_expires ? expires_when : 0);
        attr.host_only = !has_domain;

        // a Domain= the host may not claim drops the cookie (5.3 step 6)
        if (!has_domain ||
            (adr != NULL && cookie_domain_allowed(domain, scoped))) {
          /* RFC 6265bis 5.6: plain http may neither set a Secure cookie nor
             shadow, replace or erase one */
          if (adr != NULL && !strfield(adr, "https://") &&
              (attr.secure ||
               cookie_is_secure_record(cookie, cook_name, domain, path))) {
            hts_log_print(
                opt, LOG_WARNING,
                "Cookies: ignored '%.64s' from plain http, because it "
                "sets or hides a Secure cookie (%s)",
                cook_name, adr_nologin(opt, adr));
          } else if (attr.expires != 0 && attr.expires <= now) {
            cookie_remove(cookie, cook_name, domain, path);
          } else if (cookie_set_capped(cookie, cook_name, cook_value, domain,
                                       path, &attr) != 0) {
            hts_log_print(opt, LOG_WARNING,
                          "Cookies: the jar did not store '%.64s' set by %s",
                          cook_name, adr_nologin(opt, adr));
          }
        }
      }
    }
  }
}

// HTTP status code -> reason phrase (per RFC), or NULL if unknown.
const char *infostatuscode_const(int statuscode) {
  return hts_status_reason(statuscode);
}

/* Contract in httrack-library.h. */
HTSEXT_API void infostatuscode_sized(char *msg, size_t size, int statuscode) {
  const char *const text = infostatuscode_const(statuscode);

  assertf(size != 0);
  if (text != NULL) {
    strclipbuff(msg, size, text);
  } else if (strnotempty(msg) == 0) {
    strclipbuff(msg, size, "Unknown error");
  }
}

// the 64 bytes the deprecated contract documents
HTSEXT_API void infostatuscode(char *msg, int statuscode) {
  infostatuscode_sized(msg, 64, statuscode);
}

/* Wait up to timeout seconds for soc to be readable, or writable when for_write
   is set. No exception set, because POSIX reports urgent data there. */
static hts_boolean soc_wait(T_SOC soc, int timeout, hts_boolean for_write) {
  fd_set fds;
  struct timeval tv;
  const int isoc = (int) soc;

  if (soc == INVALID_SOCKET)
    return HTS_FALSE;
  assertf(isoc == soc);
  if (!hts_soc_selectable(isoc))
    return HTS_FALSE;
  FD_ZERO(&fds);
  FD_SET(isoc, &fds);
  tv.tv_sec = timeout;
  tv.tv_usec = 0;
  select(isoc + 1, for_write ? NULL : &fds, for_write ? &fds : NULL, NULL, &tv);
  return FD_ISSET(isoc, &fds) ? HTS_TRUE : HTS_FALSE;
}

int check_readinput(htsblk *r) { return soc_wait(r->soc, 0, HTS_FALSE); }

int check_readinput_t(T_SOC soc, int timeout) {
  return soc_wait(soc, timeout, HTS_FALSE);
}

int check_writeinput_t(T_SOC soc, int timeout) {
  return soc_wait(soc, timeout, HTS_TRUE);
}

/* Class the live errno now: back_wait() sees it after free and fflush and
   would rewrite this as a retryable connection error. */
static void classify_write_error(htsblk *r) {
  r->statuscode =
      check_fatal_io_errno() ? STATUSCODE_IO_FATAL : STATUSCODE_IO_ERROR;
  strcpybuff(r->msg, "Write error on disk");
}

hts_boolean hts_inmem_size_fits(LLint size) {
  return size >= 0 && size < INT32_MAX ? HTS_TRUE : HTS_FALSE;
}

/* A line-mode call reads at most this many bytes, CRs included. */
#define HTS_XFREAD_LINE_CALL_MAX 256
/* Returned by hts_peek() when it must not peek on this transport. */
#define PEEK_UNSUPPORTED (-100)

/* Set once an MPTCP socket exists on a kernel that may ignore MSG_PEEK. */
static volatile int hts_peek_unsafe = 0;
/* Set by the self-test hook. */
static volatile int hts_peek_forced_off = 0;
/* Line-mode calls that fell back to the bytewise loop, for the self-test. */
static int hts_xfread_bytewise_calls = 0;

#if HTS_INET_MPTCP && !HTS_INET_MPTCP_DARWIN
/* Can MSG_PEEK on an MPTCP socket consume the data? Linux 5.6 to 5.12 ignore
   the flag, and an unreadable version counts as one of them. */
static hts_boolean hts_mptcp_peek_is_broken(void) {
  struct utsname u;
  int major = 0, minor = 0;

  if (uname(&u) != 0 || sscanf(u.release, "%d.%d", &major, &minor) != 2)
    return HTS_TRUE;
  return major < 5 || (major == 5 && minor < 13) ? HTS_TRUE : HTS_FALSE;
}
#endif

void hts_peek_force_off(hts_boolean off) {
  hts_store_release_int(&hts_peek_forced_off, off ? 1 : 0);
}

int hts_xfread_bytewise_count(void) { return hts_xfread_bytewise_calls; }

/* Copy up to size buffered bytes without consuming them. Returns the count, 0
   when TLS has nothing decrypted yet, READ_EOF, READ_ERROR or PEEK_UNSUPPORTED.
   Only call it when the transport has data, as for hts_read(). */
static int hts_peek(htsblk *r, hts_buf buff) {
  int ret;

  if (r->is_file || hts_load_acquire_int(&hts_peek_unsafe) ||
      hts_load_acquire_int(&hts_peek_forced_off))
    return PEEK_UNSUPPORTED;
#if HTS_USEOPENSSL
  if (r->ssl) {
    sigpipe_mask m;

    sigpipe_hold(&m);
    ret = SSL_peek(r->ssl_con, buff.p, hts_sat_int(buff.size));
    sigpipe_release(&m);
    if (ret <= 0) {
      const int err_code = SSL_get_error(r->ssl_con, ret);

      if (err_code == SSL_ERROR_WANT_READ || err_code == SSL_ERROR_WANT_WRITE)
        return 0;
      return err_code == SSL_ERROR_ZERO_RETURN ? READ_EOF : READ_ERROR;
    }
    return ret;
  }
#endif
  ret = (int) recv(r->soc, buff.p, hts_sat_int(buff.size), MSG_PEEK);
  if (ret == 0)
    return READ_EOF;
  return ret < 0 ? READ_ERROR : ret;
}

typedef struct xfread_line_state {
  hts_boolean lf_detected;
  hts_boolean at_beginning;
} xfread_line_state;

/* Appends c at adr[*size] unless it is a CR, and advances *size. A NULL adr is
   a dry run. Returns whether the call must stop after c: on a blank line, on an
   LF opening the call, or on any LF when bufl is HTS_XFREAD_LINE. */
static hts_boolean xfread_line_step(xfread_line_state *st, char *adr,
                                    LLint *size, char c, int bufl) {
  hts_boolean stop = HTS_FALSE;

  if (c == 10) {
    if (st->lf_detected || st->at_beginning || bufl < 0)
      stop = HTS_TRUE;
    st->lf_detected = HTS_TRUE;
  }
  if (c != 13) {
    if (c != 10)
      st->lf_detected = HTS_FALSE;
    if (adr != NULL)
      adr[*size] = c;
    (*size)++;
    st->at_beginning = HTS_FALSE;
  }
  if (adr != NULL)
    adr[*size] = '\0';
  return stop;
}

/* The bytewise line modes, the fallback when hts_peek() declines. */
static int xfread_lines_bytewise(htsblk *r, int bufl) {
  xfread_line_state st = {HTS_FALSE, HTS_TRUE};
  int count = HTS_XFREAD_LINE_CALL_MAX;
  int tot_nl = 0;
  int nl;

  do {
    char c;

    if (r->size >= HTS_LINE_BLOCK_SIZE - 2)
      return READ_INTERNAL_ERROR;
    nl = hts_read(r, hts_buf_at(&c, 1));
    if (nl < 0)
      return nl;
    if (nl > 0 && xfread_line_step(&st, r->adr, &r->size, c, bufl))
      count = 0;
    tot_nl += nl;
  } while (--count > 0 && check_readinput(r));
  return tot_nl;
}

/* The line modes. Reads a peeked block in one go, consuming exactly what the
   bytewise loop would, so the bytes left on the socket are the same. */
static int xfread_lines(htsblk *r, int bufl) {
  char buff[HTS_XFREAD_LINE_CALL_MAX];
  xfread_line_state st = {HTS_FALSE, HTS_TRUE};
  hts_boolean overflow = HTS_FALSE;
  int avail, want, got, i;

  if (r->adr == NULL) {
    r->adr = (char *) malloct(HTS_LINE_BLOCK_SIZE);
    r->size = 0;
    if (r->adr == NULL)
      return READ_INTERNAL_ERROR;
  }
  if (r->size >= HTS_LINE_BLOCK_SIZE - 2)
    return READ_INTERNAL_ERROR;
  avail = hts_peek(r, HTS_BUF(buff));
  if (avail == PEEK_UNSUPPORTED) {
    hts_xfread_bytewise_calls++;
    return xfread_lines_bytewise(r, bufl);
  }
  if (avail <= 0)
    return avail;
  /* Dry run: where would the bytewise loop stop? */
  {
    xfread_line_state dry = st;
    LLint size = r->size;

    for (want = 0; want < avail;) {
      if (size >= HTS_LINE_BLOCK_SIZE - 2) {
        overflow = HTS_TRUE;
        break;
      }
      if (xfread_line_step(&dry, NULL, &size, buff[want++], bufl))
        break;
    }
  }
  got = hts_read(r, hts_buf_at(buff, want));
  if (got <= 0) /* peeked, so the read cannot fail */
    return got;
  for (i = 0; i < got; i++) {
    if (xfread_line_step(&st, r->adr, &r->size, buff[i], bufl))
      break;
  }
  /* The bytewise loop fails when it needs a byte past the line buffer. */
  return overflow && got == want ? READ_INTERNAL_ERROR : got;
}

// Read one block: bufl is a byte count, or one of the HTS_XFREAD_* line modes.
// Note: the +1 in the mallocs is the trailing NUL appended to the data.
LLint http_xfread1(htsblk * r, int bufl) {
  int nl = -1;

  // EOF
  if (r->totalsize >= 0 && r->size == r->totalsize) {
    return READ_EOF;
  }

  if (bufl > 0) {
    if (!r->is_write) {         // stocker en mémoire
      // Reject a hostile Content-Length, or an endless stream.
      const LLint inmem_want =
          (r->totalsize >= 0) ? r->totalsize : (r->size + bufl);
      if (!hts_inmem_size_fits(inmem_want)) {
        r->statuscode = STATUSCODE_INVALID;
        strcpybuff(r->msg, "In-memory content too large");
        return READ_ERROR;
      }
      if (r->totalsize >= 0) {  // totalsize déterminé ET ALLOUE
        if (r->adr == NULL) {
          r->adr = (char *) malloct((size_t) r->totalsize + 1);
          r->size = 0;
        }
        if (r->adr != NULL) {
          // lecture
          /* Signed first: a body longer than its announced totalsize makes
             this negative, and as a size_t it would drive a read past adr. */
          const LLint remaining = r->totalsize - r->size;
          const size_t req_size = remaining > 0 ? (size_t) remaining : 0;
          const hts_buf rb = hts_buf_at(r->adr + (int) r->size, req_size);

          nl = req_size > 0 ? hts_read(r, rb) : 0;
          // nouvelle taille
          if (nl >= 0)
            r->size += nl;

          /*
             if (r->size >= r->totalsize)
             nl = -1;  // break
           */

          r->adr[r->size] = '\0';       // caractère NULL en fin au cas où l'on traite des HTML
        }

      } else {                  // inconnu..
        // réserver de la mémoire?
        if (r->adr == NULL) {
          r->adr = (char *) malloct(bufl + 1);
          r->size = 0;
        } else {
          size_t alloc;
          char *const grown =
              hts_ckd_add(&alloc, (size_t) r->size, (size_t) bufl + 1)
                  ? NULL
                  : (char *) realloct(r->adr, alloc);

          if (grown == NULL) {
            // On failure, keep what was read, as the size guard does.
            return READ_ERROR;
          }
          r->adr = grown;
        }

        if (r->adr != NULL) {
          nl = hts_read(r, hts_buf_at(r->adr + (int) r->size, bufl));
          if (nl > 0) {
            // A failed shrink leaves the larger block, which still fits.
            char *const fit =
                (char *) realloct(r->adr, (size_t) r->size + nl + 1);

            if (fit != NULL)
              r->adr = fit;
            r->size += nl;
            r->adr[r->size] = '\0';
          }
        }
      }

      // No buffer is an error.
      if (r->adr == NULL)
        nl = READ_ERROR;

    } else {                    // stocker sur disque
      char *buff;

      buff = (char *) malloct(bufl);
      if (buff != NULL) {
        // lecture
        nl = hts_read(r, hts_buf_at(buff, bufl));
        // nouvelle taille
        if (nl > 0) {
          r->size += nl;
          if (!hts_fwrite_exact(buff, (size_t) nl, r->out)) {
            classify_write_error(r);
            nl = READ_ERROR;
          }
        }

        // libérer bloc tempo
        freet(buff);
      } else
        nl = READ_ERROR;

      /* The tail stdio held is written here, and glibc's later fclose() returns
         0 once this flush has taken the error. */
      if ((nl < 0) && (r->out != NULL) && fflush(r->out) != 0 &&
          !statuscode_is_write_error(r->statuscode)) {
        classify_write_error(r);
        nl = READ_ERROR;
      }

    }                           // stockage disque ou mémoire

  } else {
    nl = xfread_lines(r, bufl);
  }
  /* Ahead of the EOF test below: r->size is advanced above the fwrite that
     failed, so a body completed by that very read would report a clean EOF. */
  if (nl == READ_ERROR && statuscode_is_write_error(r->statuscode)) {
    return READ_ERROR;
  }
  // EOF
  if (r->totalsize >= 0 && r->size == r->totalsize) {
    return READ_EOF;
  } else {
    return nl;
  }
}

hts_boolean socket_set_nonblocking(T_SOC soc, hts_boolean nonblocking) {
#ifdef _WIN32
  unsigned long p = nonblocking ? 1 : 0;

  return ioctlsocket(soc, FIONBIO, &p) == 0 ? HTS_TRUE : HTS_FALSE;
#else
  const int flags = fcntl(soc, F_GETFL, 0);
  const int wanted = nonblocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);

  if (flags == -1 || fcntl(soc, F_SETFL, wanted) == -1)
    return HTS_FALSE;
  return HTS_TRUE;
#endif
}

/* The only mechanism that also covers OpenSSL's own writes, and it exists on
   macOS and the BSDs alone. */
void socket_set_nosigpipe(T_SOC soc) {
#ifdef SO_NOSIGPIPE
  const int on = 1;

  (void) setsockopt(soc, SOL_SOCKET, SO_NOSIGPIPE, (const char *) &on,
                    sizeof(on));
#else
  (void) soc;
#endif
}

/* Resolved by hts_init(), before any crawl thread reads them. */
static hts_boolean hts_mptcp_usable = HTS_FALSE;
static hts_boolean hts_mptcp_on_by_default = HTS_FALSE;
#if HTS_INET_MPTCP
/* macOS is left unverified, so its multipath sockets are never peeked. */
static hts_boolean hts_mptcp_peek_broken = HTS_TRUE;
#endif

#if HTS_INET_MPTCP && !HTS_INET_MPTCP_DARWIN
/* Read one small integer sysctl, or -1 when the file cannot be read. */
static long hts_mptcp_sysctl(const char *path) {
  FILE *fp = fopen(path, "rb");
  long value = -1;

  if (fp == NULL)
    return -1;
  if (fscanf(fp, "%ld", &value) != 1)
    value = -1;
  fclose(fp);
  return value;
}

/* Can the kernel recover from a SYN a middlebox dropped for its MPTCP option?
   Without the retry and blackhole detection, such a host costs a timeout. */
static hts_boolean hts_mptcp_kernel_recovers(void) {
  static const char retrans[] =
      "/proc/sys/net/mptcp/syn_retrans_before_tcp_fallback";

  if (hts_mptcp_sysctl(retrans) < 0)
    return HTS_FALSE;
  if (hts_mptcp_sysctl("/proc/sys/net/mptcp/blackhole_timeout") <= 0)
    return HTS_FALSE;
  return HTS_TRUE;
}
#endif

/* Being able to open an MPTCP socket is not enough to turn it on by default. */
static void hts_mptcp_probe(void) {
#if HTS_INET_MPTCP_DARWIN
  /* Off by default, because macOS has neither the blackhole sysctls nor a way
     to see whether a connection negotiated it. */
  const T_SOC soc = (T_SOC) socket(AF_MULTIPATH, SOCK_STREAM, 0);

  if (soc != INVALID_SOCKET) {
    close(soc);
    hts_mptcp_usable = HTS_TRUE;
  }
#elif HTS_INET_MPTCP
  const T_SOC soc = (T_SOC) socket(AF_INET, SOCK_STREAM, IPPROTO_MPTCP);

  if (soc != INVALID_SOCKET) {
    close(soc);
    hts_mptcp_usable = HTS_TRUE;
    hts_mptcp_on_by_default = hts_mptcp_kernel_recovers();
    hts_mptcp_peek_broken = hts_mptcp_peek_is_broken();
  }
#endif
}

hts_boolean hts_mptcp_available(void) { return hts_mptcp_usable; }

hts_boolean hts_mptcp_reports(void) {
  return HTS_INET_MPTCP_INFO ? HTS_TRUE : HTS_FALSE;
}

/* Should this connection be opened with MPTCP? */
static hts_boolean hts_mptcp_enabled(const httrackp *opt) {
  if (!hts_mptcp_usable)
    return HTS_FALSE;
  if (opt->mptcp == HTS_DEFAULT)
    return hts_mptcp_on_by_default;
  return opt->mptcp == HTS_TRUE ? HTS_TRUE : HTS_FALSE;
}

T_SOC hts_socket_client(int family, const httrackp *opt,
                        hts_boolean will_bind) {
  T_SOC soc = INVALID_SOCKET;

#if HTS_INET_MPTCP_DARWIN
  /* AF_MULTIPATH cannot bind on macOS, so a caller that binds gets TCP. */
  if (!will_bind && hts_mptcp_enabled(opt))
    soc = (T_SOC) socket(AF_MULTIPATH, SOCK_STREAM, 0);
#elif HTS_INET_MPTCP
  (void) will_bind;
  /* A refusal here is the kernel declining the protocol. The peer declining it
     is not visible: the kernel completes the handshake as plain TCP. */
  if (hts_mptcp_enabled(opt))
    soc = (T_SOC) socket(family, SOCK_STREAM, IPPROTO_MPTCP);
#else
  (void) opt;
  (void) will_bind;
#endif
#if HTS_INET_MPTCP
  if (soc != INVALID_SOCKET && hts_mptcp_peek_broken)
    hts_store_release_int(&hts_peek_unsafe, 1);
#endif
  if (soc == INVALID_SOCKET)
    soc = (T_SOC) socket(family, SOCK_STREAM, 0);
  if (soc != INVALID_SOCKET)
    socket_set_nosigpipe(soc);
  return soc;
}

int hts_socket_connect(T_SOC soc, const struct sockaddr *addr, SOClen len,
                       const httrackp *opt) {
#if HTS_INET_MPTCP_DARWIN
  /* connectx() is how a multipath socket is connected, and it accepts an
     ordinary socket too, so the fallback needs no second path here. A NULL opt
     is the caller saying this socket is not a multipath one. */
  if (opt != NULL && hts_mptcp_enabled(opt)) {
    sa_endpoints_t ep;

    memset(&ep, 0, sizeof(ep));
    ep.sae_dstaddr = addr;
    ep.sae_dstaddrlen = (socklen_t) len;
    return connectx(soc, &ep, SAE_ASSOCID_ANY, 0, NULL, 0, NULL, NULL);
  }
#else
  (void) opt;
#endif
  return connect(soc, addr, len);
}

hts_boolean hts_socket_connect_started(T_SOC soc, const SOCaddr *server,
                                       const httrackp *opt,
                                       hts_boolean *pending) {
  const hts_boolean wait = hts_socket_connect(soc, &SOCaddr_sockaddr(*server),
                                              SOCaddr_size(*server), opt) != 0;

  if (pending != NULL)
    *pending = wait;
  return !wait || hts_socket_connect_pending();
}

hts_boolean hts_socket_is_mptcp(T_SOC soc) {
#if HTS_INET_MPTCP_INFO
  struct mptcp_info info;
  socklen_t len = (socklen_t) sizeof(info);

  /* Refused on a socket the kernel fell back to TCP, where SO_PROTOCOL still
     reads IPPROTO_MPTCP and so answers nothing. */
  if (getsockopt(soc, SOL_MPTCP, MPTCP_INFO, (char *) &info, &len) != 0)
    return HTS_FALSE;
  return HTS_TRUE;
#else
  (void) soc;
  return HTS_FALSE;
#endif
}

void hts_mptcp_account(httrackp *opt, T_SOC soc) {
  /* Without fallback reporting, every connection would count as multipath. */
  if (!hts_mptcp_reports() || !hts_mptcp_enabled(opt))
    return;
  if (hts_socket_is_mptcp(soc))
    opt->mptcp_connections++;
  else
    opt->mptcp_fallbacks++;
}

int connect_socket_error(T_SOC soc) {
  int soerr = 0;
  socklen_t len = (socklen_t) sizeof(soerr);

  if (getsockopt(soc, SOL_SOCKET, SO_ERROR, (char *) &soerr, &len) != 0)
    return -1;
  return soerr;
}

T_SOC newhttp(httrackp *opt, const char *_iadr, htsblk *retour, int port) {
  return newhttp_addr(opt, _iadr, retour, port, -1, NULL);
}

/* Resolve the -%b host into res. A bare IPv6 literal gets brackets, or the
   cache key would read its last group as a port. */
static hts_dns_status newhttp_bind_resolve(httrackp *opt, const char *host,
                                           hts_dns_result *res) {
  char buf[HTS_BINDHOST_MAXSIZE + 3];

  if (host[0] != '[' && strchr(host, ':') != strrchr(host, ':') &&
      strlen(host) <= HTS_BINDHOST_MAXSIZE) {
    snprintf(buf, sizeof(buf), "[%s]", host);
    host = buf;
  }
  return hts_dns_resolve(opt, host, opt->timeout, &opt->state.stop, res);
}

/* Open a non-blocking connect to *server from the first bind_dns address of
   its family that binds. On failure, set retour's message, and set *try_next
   when the next address may work. */
static T_SOC newhttp_open(httrackp *opt, htsblk *retour, SOCaddr *server,
                          int port, const hts_dns_result *bind_dns,
                          hts_boolean *try_next) {
  const int family = SOCaddr_sinfamily(*server);
  const SOCaddr *local = NULL;
  T_SOC soc = INVALID_SOCKET;
  int i;

  assertf(retour != NULL);
  *try_next = HTS_FALSE;
  // make a copy for external clients
  SOCaddr_copy_SOCaddr(retour->address, *server);
  retour->address_size = SOCaddr_size(retour->address);

  if (bind_dns != NULL) {
    hts_boolean family_seen = HTS_FALSE;

    for (i = 0; i < bind_dns->count && soc == INVALID_SOCKET; i++) {
      if (SOCaddr_sinfamily(bind_dns->addr[i]) != family)
        continue;
      family_seen = HTS_TRUE;
      local = &bind_dns->addr[i];
      // a socket that binds cannot be a multipath one
      soc = hts_socket_client(family, opt, HTS_TRUE);
      retour->debugid = HTS_STAT.stat_sockid++;
      if (soc == INVALID_SOCKET)
        break;
      if (bind(soc, &SOCaddr_sockaddr(*local), SOCaddr_size(*local)) != 0) {
        const int last_errno = hts_socket_errno();
        char errbuf[HTS_STRERROR_SIZE];

        htsblk_failf(retour, HTS_DNS_BIND_PREFIX "%s",
                     hts_strerror(last_errno, HTS_BUF(errbuf)));
        deletesoc(soc);
        soc = INVALID_SOCKET;
        local = NULL;
      }
    }
    if (!family_seen)
      htsblk_failf(retour, HTS_DNS_BIND_PREFIX "%s",
                   "no address of the server's family");
    if (soc == INVALID_SOCKET && local == NULL) {
      *try_next = HTS_TRUE;
      return INVALID_SOCKET;
    }
  } else {
    soc = hts_socket_client(family, opt, HTS_FALSE);
    retour->debugid = HTS_STAT.stat_sockid++;
  }
  if (soc == INVALID_SOCKET) {
    const int last_errno = hts_socket_errno();
    char errbuf[HTS_STRERROR_SIZE];

    htsblk_failf(retour, "Unable to create a socket: %s",
                 hts_strerror(last_errno, HTS_BUF(errbuf)));
    return INVALID_SOCKET;
  }
  SOCaddr_initport(*server, port);

  if (!socket_set_nonblocking(soc, HTS_TRUE)) {
    const int last_errno = hts_socket_errno();
    char errbuf[HTS_STRERROR_SIZE];

    htsblk_failf(retour, "Non-blocking socket failed: %s",
                 hts_strerror(last_errno, HTS_BUF(errbuf)));
    deletesoc(soc);
    return INVALID_SOCKET;
  }
  HTS_STAT.last_connect = mtime_local();

  // non-blocking: back_wait sees how a pending connect ended
  if (!hts_socket_connect_started(soc, server, local != NULL ? NULL : opt,
                                  NULL)) {
    const int last_errno = hts_socket_errno();
    char errbuf[HTS_STRERROR_SIZE];

    htsblk_failf(retour, "Connect error: %s",
                 hts_strerror(last_errno, HTS_BUF(errbuf)));
    deletesoc(soc);
    *try_next = HTS_TRUE;
    return INVALID_SOCKET;
  }
  return soc;
}

T_SOC newhttp_addr(httrackp *opt, const char *_iadr, htsblk *retour, int port,
                   int addr_index, int *addr_count) {
  return newhttp_addrs(opt, _iadr, retour, port, addr_index, 1, addr_count,
                       NULL);
}

T_SOC newhttp_addrs(httrackp *opt, const char *_iadr, htsblk *retour, int port,
                    int addr_index, int tries, int *addr_count, int *skipped) {
  T_SOC soc = INVALID_SOCKET; // descipteur de la socket

  if (addr_count != NULL) {
    *addr_count = 0;
  }
  if (skipped != NULL) {
    *skipped = 0;
  }

  if (strcmp(_iadr, "file://") != 0) {  /* non fichier */
    SOCaddr server;
    hts_dns_result dns, bind_res;
    const hts_dns_result *bind_dns = NULL;
    hts_boolean try_next = HTS_FALSE;
    char msg[sizeof(retour->msg)];
    int t;

    // tester un éventuel id:pass et virer id:pass@ si détecté
    const char *const iadr = jump_identification_const(_iadr);
    const char *resolve_host = iadr;
    char BIGSTK iadr2[HTS_URLMAXSIZE * 2];

    SOCaddr_clear(server);

    // tester un éventuel port
    if (port == -1) {
      const char *a = jump_toport_const(iadr);

#if HTS_USEOPENSSL
      if (retour->ssl)
        port = 443;
      else
        port = 80;              // port par défaut
#else
      port = 80;                // port par défaut
#endif

      if (a != NULL) {
        // folding a nonsense port into 0..65535 crawls one neither the link nor
        // a port filter named; an empty "host:" just means the default (#614)
        if (a[1] != '\0' && !hts_parse_url_port(a + 1, &port)) {
          if (retour != NULL) {
            htsblk_failf(retour, "Invalid port: %s", a + 1);
          }
          return INVALID_SOCKET;
        }

        iadr2[0] = '\0';
        // the address itself, without the ":port"
        strncatbuff(iadr2, iadr, (int) (a - iadr));
        resolve_host = iadr2;
      }
    }

    // resolve the full address list and pick the requested candidate; the
    // scheduler retries the next index when a connect fails (dead IPv6 etc.)
    (void) hts_dns_resolve(opt, resolve_host, opt->timeout, &opt->state.stop,
                           &dns);
    if (addr_count != NULL) {
      *addr_count = dns.count;
    }
    if (addr_index < 0) {
      tries = dns.count;
      addr_index = hts_dns_preferred(opt, resolve_host);
      if (addr_index >= dns.count)
        addr_index = 0;
    }
    if (addr_index >= 0 && addr_index < dns.count) {
      SOCaddr_copy_SOCaddr(server, dns.addr[addr_index]);
    }

    if (!SOCaddr_is_valid(server)) {
      if (retour != NULL) {
        htsblk_failf(retour, HTS_DNS_FAIL_PREFIX "%s", hts_dns_strerror(&dns));
      }
      return INVALID_SOCKET;
    }

    if (retour != NULL && strnotempty(retour->req.proxy.bindhost)) {
      if (newhttp_bind_resolve(opt, retour->req.proxy.bindhost, &bind_res) !=
          HTS_DNS_OK) {
        htsblk_failf(retour, HTS_DNS_BIND_PREFIX "%s",
                     hts_dns_strerror(&bind_res));
        return INVALID_SOCKET;
      }
      bind_dns = &bind_res;
    }

    /* A first connect skips addresses it cannot bind for or connect to at
       once, and back_connect_next() does the same for a fallback. */
    strcpybuff(msg, retour->msg);
    for (t = 0; t < tries && t < dns.count; t++) {
      SOCaddr_copy_SOCaddr(server, dns.addr[(addr_index + t) % dns.count]);
      soc = newhttp_open(opt, retour, &server, port, bind_dns, &try_next);
      if (soc != INVALID_SOCKET || !try_next)
        break;
      hts_log_print(opt, LOG_DEBUG, "skipping address %d/%d of %s: %s",
                    (addr_index + t) % dns.count + 1, dns.count, resolve_host,
                    retour->msg);
    }
    if (soc != INVALID_SOCKET) {
      // a skipped address must not leave its reason on a working connect
      strcpybuff(retour->msg, msg);
      if (skipped != NULL) {
        *skipped = t;
      }
    }

  } else {                      // on doit ouvrir un fichier local!
    // il sera géré de la même manière qu'une socket (c'est idem!)

    soc = LOCAL_SOCKET_ID;      // pseudo-socket locale..
    // soc sera remplacé lors d'un http_fopen() par un handle véritable!

  }                             // teste fichier local ou http

  return soc;
}

// couper http://www.truc.fr/pub/index.html -> www.truc.fr /pub/index.html
// retour=-1 si erreur.
// si file://... alors adresse=file:// (et coupe le ?query dans ce cas)
int ident_url_absolute(const char *url, lien_adrfil *adrfil) {
  int pos = 0;
  int scheme = 0;

  // effacer adrfil->adr et adrfil->fil
  adrfil->adr[0] = adrfil->fil[0] = '\0';

  // Reject an over-long URL: a root-relative path is copied whole into fil[].
  if (strlen(url) >= HTS_URLMAXSIZE * 2)
    return -1;

  // Scheme?
  {
    const char *a = url;

    while (hts_isalpha((unsigned char) *a))
      a++;
    if (*a == ':')
      scheme = 1;
  }

  // 1. optional scheme ":"
  if ((pos = strfield(url, "file:"))) { // fichier local!! (pour les tests)
    //!!p+=3;
    strcpybuff(adrfil->adr, "file://");
  } else if ((pos = strfield(url, "http:"))) {  // HTTP
    //!!p+=3;
  } else if ((pos = strfield(url, "ftp:"))) {   // FTP
    strcpybuff(adrfil->adr, "ftp://");  // FTP!!
    //!!p+=3;
#if HTS_USEOPENSSL
  } else if ((pos = strfield(url, "https:"))) {     // HTTPS
    strcpybuff(adrfil->adr, "https://");
#endif
  } else if (scheme) {
    return -1;                  // erreur non reconnu
  } else
    pos = 0;

  // 2. optional "//" authority
  if (strncmp(url + pos, "//", 2) == 0)
    pos += 2;

  // (url+pos) now points to the path (not net path)

  //## if (adrfil->adr[0]!=lOCAL_CHAR) {    // adrfil->adresse normale http
  if (!strfield(adrfil->adr, "file:")) {        // PAS adrfil->file://
    const char *p, *q;

    p = url + pos;

    // p pointe sur le début de l'adrfil->adresse, ex: www.truc.fr/sommaire/index.html
    q = hts_authority_end(jump_identification_const(p));

    // chemin www... trop long!!
    if ((((int) (q - p))) > HTS_URLMAXSIZE) {
      return -1;                // erreur
    }
    // recopier adrfil->adresse www..
    strncatbuff(adrfil->adr, p, ((int) (q - p)));
    // *( adrfil->adr+( ((int) q) - ((int) p) ) )=0;  // faut arrêter la fumette!

    if (q[0] != '/') // the default page
      strcatbuff(adrfil->fil, "/");
    if (!hts_buf_try_cat(HTS_BUF(adrfil->fil), q))
      return -1;
    // SECURITE:
    // simplifier url pour les ../
    fil_simplifie(adrfil->fil);
  } else {                      // localhost adrfil->file://
    const char *p;
    size_t i;
    char *a;

    p = url + pos;
    if (*p == '/' || *p == '\\') {      /* adrfil->file:///.. */
      strcatbuff(adrfil->fil, p);       // fichier local ; adrfil->adr="#"
    } else {
      if (*p == '\0' || p[1] != ':') {  /* empty path: not a DOS drive letter */
        strcatbuff(adrfil->fil, "//");  /* adrfil->file://server/foo */
        strcatbuff(adrfil->fil, p);
      } else {
        strcatbuff(adrfil->fil, p);     // adrfil->file://C:\..
      }
    }

    a = strchr(adrfil->fil, '?');
    if (a)
      *a = '\0';                /* couper query (inutile pour adrfil->file:// lors de la requête) */
    // adrfil->filtrer les \\ -> / pour les fichiers DOS
    for(i = 0; adrfil->fil[i] != '\0'; i++)
      if (adrfil->fil[i] == '\\')
        adrfil->fil[i] = '/';
    // collapse ../ like the http branch above (path-traversal safety)
    fil_simplifie(adrfil->fil);
  }

  // no hostname
  if (!strnotempty(adrfil->adr))
    return -1;                  // erreur non reconnu

  // nommer au besoin.. (non utilisé normalement)
  if (!strnotempty(adrfil->fil))
    strcpybuff(adrfil->fil, "default-index.html");

  // host names are case-insensitive
  hts_lowcase(jump_identification(adrfil->adr));

  return 0;
}

/* Contract in htslib.h. */
void fil_simplifie(char *f) {
  char *a, *b;
  char lc = '/';
  int query = 0;
  int wasAbsolute = (*f == '/');

  for(a = b = f; *a != '\0';) {
    if (*a == '?')
      query = 1;
    if (query == 0 && lc == '/' && a[0] == '.' && a[1] == '/') {        /* foo/./bar or ./foo  */
      a += 2;
    } else if (query == 0 && lc == '/' && a[0] == '.' && a[1] == '.' && (a[2] == '/' || a[2] == '\0')) {        /* foo/../bar or ../foo or .. */
      if (a[2] == '\0')
        a += 2;
      else
        a += 3;
      /* drop the last segment written, never past the root */
      if (b > f)
        b--;
      while (b > f && b[-1] != '/')
        b--;
      if (wasAbsolute && b == f)
        b++;
    } else {
      *b++ = lc = *a;
      a++;
    }
  }
  *b = '\0';
  if (*f == '\0') {
    if (wasAbsolute) {
      f[0] = '/';
      f[1] = '\0';
    } else {
      f[0] = '.';
      f[1] = '/';
      f[2] = '\0';
    }
  }
}

#if HTS_USEOPENSSL
/* close_notify goes to a peer that has usually gone already, so this is the
   write most likely to raise SIGPIPE. */
static void ssl_shutdown_free(SSL **ssl_con) {
  sigpipe_mask m;

  sigpipe_hold(&m);
  SSL_shutdown(*ssl_con);
  sigpipe_release(&m);
  SSL_free(*ssl_con);
  *ssl_con = NULL;
}
#endif

// fermer liaison fichier ou socket
void deletehttp(htsblk *r) {
#if HTS_USEOPENSSL
  /* Free OpenSSL structures */
  if (r->ssl_con)
    ssl_shutdown_free(&r->ssl_con);
#endif
  if (r->soc != INVALID_SOCKET) {
    if (r->is_file) {
      if (r->fp)
        fclose(r->fp);
      r->fp = NULL;
    } else {
      if (r->soc != LOCAL_SOCKET_ID)
        deletesoc_r(r);
    }
    r->soc = INVALID_SOCKET;
  }
}

// free the addr buffer
// always returns 1
int deleteaddr(htsblk * r) {
  if (r->adr != NULL) {
    freet(r->adr);
    r->adr = NULL;
  }
  if (r->headers != NULL) {
    freet(r->headers);
    r->headers = NULL;
  }
  return 1;
}

// Close a socket, logging a failure.
void deletesoc(T_SOC soc) {
  if (soc != INVALID_SOCKET && soc != LOCAL_SOCKET_ID) {
    if (!hts_closesocket(soc)) {
      const int err = hts_socket_errno();
      char errbuf[HTS_STRERROR_SIZE];

      fprintf(stderr, "* error closing socket " T_SOCP ": %s\n", soc,
              hts_strerror(err, HTS_BUF(errbuf)));
    }
  }
}

/* Will also clean other things */
static void deletesoc_r(htsblk *r) {
#if HTS_USEOPENSSL
  if (r->ssl_con)
    ssl_shutdown_free(&r->ssl_con);
#endif
  if (r->soc != INVALID_SOCKET) {
    deletesoc(r->soc);
    r->soc = INVALID_SOCKET;
  }
}

// renvoi le nombre de secondes depuis 1970
TStamp time_local(void) {
  return ((TStamp) time(NULL));
}

// number of millisec since 1970
HTSEXT_API TStamp mtime_local(void) {
#ifndef _WIN32
  struct timeval tv;
  if (gettimeofday(&tv, NULL) != 0) {
    assert(! "gettimeofday");
  }

  return (TStamp) (((TStamp) tv.tv_sec * (TStamp) 1000) +
                   ((TStamp) tv.tv_usec / (TStamp) 1000));
#else
  struct timeb B;
  ftime(&B);
  return (TStamp) (((TStamp) B.time * (TStamp) 1000)
                   + ((TStamp) B.millitm));
#endif
}

/* Contract in htslib.h. Falls back to the wall clock where the platform has
   no monotonic one, which is no worse than before. */
TStamp mtime_monotonic(void) {
#if defined(_WIN32)
  return (TStamp) GetTickCount64();
#elif defined(HAVE_CLOCK_GETTIME) && defined(CLOCK_MONOTONIC)
  struct timespec ts;

  if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
    return (TStamp) ts.tv_sec * 1000 + (TStamp) (ts.tv_nsec / 1000000);
  return mtime_local();
#else
  return mtime_local();
#endif
}

hts_boolean hts_timeout_elapsed(TStamp since_ms, TStamp now_ms, int timeout_s) {
  return now_ms - since_ms >= (TStamp) timeout_s * 1000;
}

/* Writes t seconds as days, hours, minutes and seconds, dropping the empty
   leading units, clipped to ST. */
static void duration_str(hts_buf st, TStamp t, hts_boolean quick) {
  const int j = (int) (t / (3600 * 24));
  int h, m, s;

  t -= ((TStamp) j) * (3600 * 24);
  h = (int) (t / 3600);
  t -= ((TStamp) h) * 3600;
  m = (int) (t / 60);
  t -= ((TStamp) m) * 60;
  s = (int) t;

  if (j > 0)
    snprintf(st.p, st.size,
             quick == HTS_TRUE ? "%dd,%02dh,%02dmin%02ds"
                               : "%d days, %d hours %d minutes %d seconds",
             j, h, m, s);
  else if (h > 0)
    snprintf(st.p, st.size,
             quick == HTS_TRUE ? "%dh,%02dmin%02ds"
                               : "%d hours %d minutes %d seconds",
             h, m, s);
  else if (m > 0)
    snprintf(st.p, st.size,
             quick == HTS_TRUE ? "%dmin%02ds" : "%d minutes %d seconds", m, s);
  else
    snprintf(st.p, st.size, quick == HTS_TRUE ? "%ds" : "%d seconds", s);
}

void sec2str(hts_buf st, TStamp t) { duration_str(st, t, HTS_FALSE); }

/* Contract in httrack-library.h. */
HTSEXT_API void qsec2str_sized(char *st, size_t size, TStamp t) {
  duration_str(hts_buf_at(st, size), t, HTS_TRUE);
}

// the 32 bytes the deprecated contract documents
HTSEXT_API void qsec2str(char *st, TStamp t) { qsec2str_sized(st, 32, t); }

void time_gmt_rfc822(hts_buf s) {
  time_t tt;
  struct tm tmv;

  assertf(s.size != 0);
  tt = time(NULL);
  /* no local-time fallback: it would format local time and still label it GMT
   * (#806) */
  if (hts_gmtime(tt, &tmv))
    time_rfc822(s, &tmv);
  else
    s.p[0] = '\0';
}

void hts_now_iso8601(char out[32]) {
  time_t t = time(NULL);
  struct tm tmv;

  if (!hts_gmtime(t, &tmv))
    memset(&tmv, 0, sizeof(tmv));
  strftime(out, 32, "%Y-%m-%dT%H:%M:%SZ", &tmv);
}

void time_local_rfc822(hts_buf s) {
  time_t tt;
  struct tm tmv;

  assertf(s.size != 0);
  tt = time(NULL);
  if (hts_localtime(tt, &tmv))
    time_rfc822_local(s, &tmv);
  else
    s.p[0] = '\0';
}

/* sets file time. -1 if error */
/* Note: utf-8 */
static int set_filetime(const char *file, struct tm *tm_time) {
  time_t t;

  if (hts_tm_filetime(tm_time, &t)) {
    STRUCT_UTIMBUF tim;

    memset(&tim, 0, sizeof(tim));
    tim.actime = tim.modtime = t;
    return UTIME(file, &tim);
  }
  return -1;
}

/* Both Retry-After forms (RFC 9110 10.2.3). An unusable value gives -1 rather
   than 0, so a caller can tell it from a server that named zero. */
int hts_parse_retry_after(const char *value, time_t now) {
  const char *p = value;
  const char *end;

  if (value == NULL)
    return -1;
  while (is_space(*p))
    p++;
  end = p + strspn(p, "0123456789");
  while (is_space(*end))
    end++;

  // delta-seconds is the whole value, so "06 Nov 2026 ..." is a date
  if (end > p && *end == '\0') {
    LLint secs;

    // Every byte is a digit, so a refusal means too big, which clips.
    return hts_parse_llint(p, NULL, 0, INT_MAX, &secs) ? (int) secs : INT_MAX;
  }

  {
    const int64_t when = hts_parse_date(p, 50);

    if (when == -1)
      return -1;
    if (when <= (int64_t) now)
      return 0;
    if (when - (int64_t) now > INT_MAX)
      return INT_MAX;
    return (int) (when - (int64_t) now);
  }
}

/* See htslib.h. */
int64_t hts_parse_date(const char *s, int pivot) {
  struct tm buffer;
  struct tm *const tm_s = convert_time_rfc822_pivot(&buffer, s, pivot);

  return tm_s != NULL ? hts_utc_seconds(tm_s) : -1;
}

/* sets file time from RFC822 date+time, -1 if error*/
/* Note: utf-8 */
int set_filetime_rfc822(const char *file, const char *date) {
  struct tm buffer;
  struct tm *tm_s = convert_time_rfc822(&buffer, date);

  if (tm_s) {
    return set_filetime(file, tm_s);
  } else
    return -1;
}

void hts_stamp_filetime(httrackp *opt, const char *file, const char *date) {
  /* an unparsable date is common, so debug: the file keeps its fetch time */
  if (set_filetime_rfc822(file, date) != 0)
    hts_log_print(opt, LOG_DEBUG, "Could not set the date of %s to %s", file,
                  date);
}

/* Note: utf-8 */
time_t get_filetime(const char *file) {
  STRUCT_STAT buf;

  if (STAT(file, &buf) != 0)
    return (time_t) -1;
  return (time_t) buf.st_mtime;
}

/* Note: utf-8 */
hts_boolean hts_file_mtime(const char *file, hts_filetime_t *when) {
#ifdef _WIN32
  /* _wstat64() gives whole seconds, too coarse to order a request against a
     mirror started in the same second. GetFileAttributesExW gives 100ns. */
  WIN32_FILE_ATTRIBUTE_DATA attr;
  LPWSTR wfile = hts_pathToUCS2(file);
  ULARGE_INTEGER ticks;

  if (wfile == NULL)
    return HTS_FALSE;
  if (!GetFileAttributesExW(wfile, GetFileExInfoStandard, &attr)) {
    freet(wfile);
    return HTS_FALSE;
  }
  freet(wfile);
  ticks.LowPart = attr.ftLastWriteTime.dwLowDateTime;
  ticks.HighPart = attr.ftLastWriteTime.dwHighDateTime;
  when->sec = (int64_t) (ticks.QuadPart / 10000000ULL);
  when->nsec = (int32_t) ((ticks.QuadPart % 10000000ULL) * 100);
  when->is_plain_file = (attr.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0
                            ? HTS_TRUE
                            : HTS_FALSE;
  return HTS_TRUE;
#else
  STRUCT_STAT buf;

  if (STAT(file, &buf) != 0)
    return HTS_FALSE;
  when->sec = (int64_t) buf.st_mtime;
#if defined(HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC)
  when->nsec = (int32_t) buf.st_mtim.tv_nsec; /* POSIX 2008 */
#elif defined(HAVE_STRUCT_STAT_ST_MTIMESPEC_TV_NSEC)
  when->nsec = (int32_t) buf.st_mtimespec.tv_nsec; /* macOS, older BSD */
#else
  /* Whole seconds, which is all a FAT or exFAT filesystem keeps anyway. */
  when->nsec = 0;
#endif
  when->is_plain_file = S_ISREG(buf.st_mode) ? HTS_TRUE : HTS_FALSE;
  return HTS_TRUE;
#endif
}

/* Note: utf-8 */
hts_boolean hts_file_is_newer(const char *a, const char *b) {
  hts_filetime_t ta, tb;

  if (!hts_file_mtime(a, &ta) || !ta.is_plain_file)
    return HTS_FALSE;
  if (!hts_file_mtime(b, &tb))
    return HTS_FALSE;
  if (ta.sec != tb.sec)
    return ta.sec > tb.sec ? HTS_TRUE : HTS_FALSE;
  return ta.nsec > tb.nsec ? HTS_TRUE : HTS_FALSE;
}

/* Note: utf-8 */
hts_boolean hts_file_backdate(const char *file, int seconds) {
  hts_filetime_t when;

  /* The file's own stamp rather than a fresh clock reading, so the two can not
     disagree about when this file was written. */
  if (!hts_file_mtime(file, &when))
    return HTS_FALSE;
  when.sec -= seconds;
#ifdef _WIN32
  {
    LPWSTR wfile = hts_pathToUCS2(file);
    ULARGE_INTEGER ticks;
    FILETIME ft;
    HANDLE h;
    hts_boolean ok;

    if (wfile == NULL)
      return HTS_FALSE;
    h = CreateFileW(wfile, FILE_WRITE_ATTRIBUTES,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    freet(wfile);
    if (h == INVALID_HANDLE_VALUE)
      return HTS_FALSE;
    if (when.sec < 0) { /* no write time on this volume */
      CloseHandle(h);
      return HTS_FALSE;
    }
    ticks.QuadPart =
        (ULONGLONG) when.sec * 10000000ULL + (ULONGLONG) (when.nsec / 100);
    ft.dwLowDateTime = ticks.LowPart;
    ft.dwHighDateTime = ticks.HighPart;
    ok = SetFileTime(h, NULL, NULL, &ft) ? HTS_TRUE : HTS_FALSE;
    CloseHandle(h);
    return ok;
  }
#elif defined(HAVE_UTIMENSAT)
  {
    struct timespec times[2];

    times[0].tv_sec = times[1].tv_sec = (time_t) when.sec;
    times[0].tv_nsec = times[1].tv_nsec = (long) when.nsec;
    return utimensat(AT_FDCWD, file, times, 0) == 0 ? HTS_TRUE : HTS_FALSE;
  }
#else
  {
    struct timeval times[2];

    times[0].tv_sec = times[1].tv_sec = (time_t) when.sec;
    times[0].tv_usec = times[1].tv_usec = (long) (when.nsec / 1000);
    return utimes(file, times) == 0 ? HTS_TRUE : HTS_FALSE;
  }
#endif
}

/* Note: utf-8 */
int get_filetime_rfc822(const char *file, hts_buf date) {
  STRUCT_STAT buf;

  assertf(date.size != 0);
  date.p[0] = '\0';
  if (STAT(file, &buf) == 0) {
    struct tm tmv;
    time_t tt = buf.st_mtime;

    /* no local-time fallback: it would format local time and still label it GMT
     * (#806) */
    if (hts_gmtime(tt, &tmv)) {
      time_rfc822(date, &tmv);
      return 1;
    }
  }
  return 0;
}

void time_rfc822(hts_buf s, struct tm *A) {
  if (A == NULL) {
    int localtime_returned_null = 0;

    assertf(localtime_returned_null);
  }
  hts_format_rfc822(s, A, " GMT");
}

// Writes A as an RFC 822 date in local time.
static void time_rfc822_local(hts_buf s, struct tm *A) {
  if (A == NULL) {
    int localtime_returned_null = 0;

    assertf(localtime_returned_null);
  }
  hts_format_rfc822(s, A, "");
}

// conversion en b,Kb,Mb
HTSEXT_API char *int2bytes(strc_int2bytes2 * strc, LLint n) {
  char **a = int2bytes2(strc, n);

  strcpybuff(strc->catbuff, a[0]);
  strcatbuff(strc->catbuff, a[1]);
  return strc->catbuff;
}

// conversion en b/s,Kb/s,Mb/s
HTSEXT_API char *int2bytessec(strc_int2bytes2 * strc, long int n) {
  char buff[256];
  char **a = int2bytes2(strc, n);

  strcpybuff(buff, a[0]);
  strcatbuff(buff, a[1]);
  return concat(strc->catbuff, sizeof(strc->catbuff), buff, "/s");
}

// conversion en b,Kb,Mb, nombre et type séparés
// limite: 2.10^9.10^6B

/* See http://physics.nist.gov/cuu/Units/binary.html */
#define ToLLint(a) ((LLint)(a))
#define ToLLintKiB (ToLLint(1024))
#define ToLLintMiB (ToLLintKiB * ToLLintKiB)
#define ToLLintGiB (ToLLintKiB * ToLLintKiB * ToLLintKiB)
#define ToLLintTiB (ToLLintKiB * ToLLintKiB * ToLLintKiB * ToLLintKiB)
#define ToLLintPiB                                                             \
  (ToLLintKiB * ToLLintKiB * ToLLintKiB * ToLLintKiB * ToLLintKiB)

static char **int2bytes2(strc_int2bytes2 *strc, LLint n) {
  if (n < ToLLintKiB) {
    sprintf(strc->buff1, "%d", (int) (LLint) n);
    strcpybuff(strc->buff2, "B");
  } else if (n < ToLLintMiB) {
    sprintf(strc->buff1, "%d,%02d", (int) ((LLint) (n / ToLLintKiB)),
            (int) ((LLint) ((n % ToLLintKiB) * 100) / ToLLintKiB));
    strcpybuff(strc->buff2, "KiB");
  } else if (n < ToLLintGiB) {
    sprintf(strc->buff1, "%d,%02d", (int) ((LLint) (n / (ToLLintMiB))),
            (int) ((LLint) (((n % (ToLLintMiB)) * 100) / (ToLLintMiB))));
    strcpybuff(strc->buff2, "MiB");
  } else if (n < ToLLintTiB) {
    sprintf(strc->buff1, "%d,%02d", (int) ((LLint) (n / (ToLLintGiB))),
            (int) ((LLint) (((n % (ToLLintGiB)) * 100) / (ToLLintGiB))));
    strcpybuff(strc->buff2, "GiB");
  } else if (n < ToLLintPiB) {
    sprintf(strc->buff1, "%d,%02d", (int) ((LLint) (n / (ToLLintTiB))),
            (int) ((LLint) (((n % (ToLLintTiB)) * 100) / (ToLLintTiB))));
    strcpybuff(strc->buff2, "TiB");
  } else {
    sprintf(strc->buff1, "%d,%02d", (int) ((LLint) (n / (ToLLintPiB))),
            (int) ((LLint) (((n % (ToLLintPiB)) * 100) / (ToLLintPiB))));
    strcpybuff(strc->buff2, "PiB");
  }
  strc->buffadr[0] = strc->buff1;
  strc->buffadr[1] = strc->buff2;
  return strc->buffadr;
}

// envoi de texte (en têtes généralement) sur la socket soc
int sendc(htsblk * r, const char *s) {
  const size_t len = strlen(s);
  int n, ssz;

  if (len > INT_MAX)
    return -1;
  ssz = (int) len;

#if HTS_USEOPENSSL
  if (r->ssl) {
    sigpipe_mask m;

    sigpipe_hold(&m);
    n = SSL_write(r->ssl_con, s, ssz);
    sigpipe_release(&m);
  } else
#endif
    n = hts_send_nosignal(r->soc, s, ssz);

  return (n == ssz) ? n : -1;
}

// Like linput, but in memory (optimized). A line too long for `s` is clipped
// but consumed whole: resuming inside it reads its tail back as another line.
int binput(const char *buff, hts_buf s) {
  int count = 0;
  size_t destCount = 0;

  assertf(s.size != 0);
  // Note: \0 will return 1
  while (buff != NULL && buff[count] != '\0' && buff[count] != '\n') {
    if (buff[count] != '\r' && destCount < s.size - 1) {
      s.p[destCount++] = buff[count];
    }
    count++;
  }
  s.p[destCount] = '\0';

  // then return the supplemental jump offset
  return count + 1;
}

hts_boolean hts_header_is_read(const char *line) {
  static const char *const names[] = {"Connection:",
                                      "Content-Disposition:",
                                      "Content-Encoding:",
                                      "Content-length:",
                                      "Content-Range:",
                                      "Content-type:",
                                      "Date:",
                                      "Etag:",
                                      "Keep-Alive:",
                                      "Last-Modified:",
                                      "Location:",
                                      "Retry-After:",
                                      "Set-Cookie:",
                                      "TE:",
                                      "Transfer-Encoding:"};
  size_t i;

  for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    if (strfield(line, names[i]))
      return HTS_TRUE;
  return HTS_FALSE;
}

hts_boolean binput_line(const char *buff, const char *end, hts_buf s,
                        int *offset) {
  hts_boolean cut = HTS_FALSE;
  const char *p;
  size_t j = 0;

  assertf(s.size != 0);
  *offset = 0;
  s.p[0] = '\0';
  if (buff == NULL || buff >= end)
    return HTS_FALSE;
  for (p = buff; p < end && *p != '\0' && *p != '\n'; p++) {
    if (*p == '\r')
      continue;
    if (j < s.size - 1)
      s.p[j++] = *p;
    else
      cut = HTS_TRUE; /* keep walking: the tail must not become more headers */
  }
  s.p[j] = '\0';
  *offset = (int) (p - buff) + 1; /* binput's convention: step over the \n */
  return cut;
}

// case-insensitive strstr()
const char *strstrcase(const char *s, const char *o) {
  while(*s && strfield(s, o) == 0)
    s++;
  if (*s == '\0')
    return NULL;
  return s;
}

void map_characters(hts_span s, unsigned int *map) {
  size_t i;

  memset(map, 0, sizeof(unsigned int) * 256);
  for (i = 0; i < s.len; i++)
    map[(unsigned char) s.p[i]]++;
}

// le fichier est-il un fichier html?
//  0 : non
//  1 : oui
// -1 : on sait pas
// -2 : on sait pas, pas d'extension
int ishtml(httrackp * opt, const char *fil) {
  /* User-defined MIME types (overrides ishtml()) */
  char BIGSTK fil_noquery[HTS_URLMAXSIZE * 2];
  char mime[256];
  char *a;
  const char *dotted;

  strcpybuff(fil_noquery, fil);
  if ((a = strchr(fil_noquery, '?')) != NULL) {
    *a = '\0';
  }
  if (get_userhttptype(opt, HTS_BUF(mime), fil_noquery)) {
    if (is_html_mime_type(mime)) {
      return 1;
    } else {
      return 0;
    }
  }

  if (!strnotempty(fil_noquery)) {
    return -2;
  }

  /* Search for known ext */
  dotted = hts_path_ext_dot(fil_noquery, "/");
  if (dotted != NULL) {
    int ret = ishtml_ext(hts_span_of(dotted + 1));

    if (ret == -1) {
      switch (is_knowntype(opt, dotted)) {
      case 1:
        ret = 0;                // connu, non html
        break;
      case 2:
        ret = 1;                // connu, html
        break;
      default:
        ret = -1;               // inconnu..
        break;
      }
    }
    return ret;
  } else
    return -2;                  // indéterminé, par exemple /truc
}

int ishtml_ext(hts_span ext) {
  static const char *const html_exts[] = {HTS_SPAN_WORDS(
      "html", "htm", "shtml", "phtml", "htmlx", "shtm", "phtm", "htmx")};

  return hts_span_index_ci(ext, html_exts) >= 0 ? 1 : -1;
}

/* Declare a non-const version of FUN */
#define DECLARE_NON_CONST_VERSION(FUN) \
char *FUN(char *source) { \
  const char *const ret = FUN ##_const(source); \
  return ret != NULL ? source + ( ret - source ) : NULL; \
}

/* Appends s at buf.p[*len], clipped to buf.size - 1, NUL-terminated. */
static void append_clipped(hts_buf buf, size_t *len, hts_span s) {
  size_t n = s.len;

  if (n > buf.size - 1 - *len)
    n = buf.size - 1 - *len;
  memcpy(buf.p + *len, s.p, n);
  *len += n;
  buf.p[*len] = '\0';
}

/* Returns adr past its blanks, scheme and "//", the engine's or any
   "scheme://", or adr when it has no scheme. */
const char *login_start(const char *adr) {
  const char *s = adr;
  const char *known;

  while (hts_isspace((unsigned char) *s))
    s++;
  known = jump_protocol_const(s);
  if (known != s)
    return known;
  if (!hts_isalpha((unsigned char) *s))
    return adr;
  while (hts_isalnum((unsigned char) *s) || *s == '+' || *s == '-' || *s == '.')
    s++;
  return strncmp(s, "://", 3) == 0 ? s + 3 : adr;
}

/* Returns adr past its login, or login_start(adr) when it has none. */
const char *login_end(const char *adr) {
  const char *s = adr;
  hts_url_view v;

  while (hts_isspace((unsigned char) *s))
    s++;
  (void) hts_url_view_parse(hts_span_of(s), &v);
  return hts_span_present(v.userinfo) ? v.host.p : login_start(adr);
}

hts_boolean hts_escape_typed_login(const char *url, hts_buf dst) {
  const char *const start = login_start(url);
  const char *const at = strrchr_limit(start, '@', strchr(start, '/'));

  return hts_escape_span(hts_span_of(url),
                         hts_span_between(start, at != NULL ? at - 1 : start),
                         "?#\\", dst);
}

/* Returns HTS_TRUE when adr has a login, a user alone included. */
static hts_boolean has_login(const char *adr) {
  return login_end(adr) > login_start(adr) ? HTS_TRUE : HTS_FALSE;
}

const char *adr_nologin_r(hts_buf out, const char *adr) {
  const char *const scheme = login_start(adr);
  const char *const host = login_end(adr);
  const char *colon;
  size_t len = 0;

  if (!has_login(adr) || out.size == 0)
    return adr;
  colon = memchr(scheme, ':', (size_t) (host - scheme));
  if (colon != NULL) {
    append_clipped(out, &len, hts_span_between(adr, colon));
    append_clipped(out, &len, hts_span_of(":***@"));
  } else {
    append_clipped(out, &len, hts_span_between(adr, scheme));
    append_clipped(out, &len, hts_span_of("***@"));
  }
  append_clipped(out, &len, hts_span_of(host));
  return out.p;
}

const char *url_nologin_r(hts_buf buf, const char *url) {
  return login_start(url) != url ? adr_nologin_r(buf, url) : url;
}

const char *adr_nologin(httrackp *opt, const char *adr) {
  if (adr == NULL) // treathead() logs a cookie its caller gave no URL for
    return "";
  if (!has_login(adr))
    return adr;
  // no buffer to mask into: drop the whole login
  if (opt == NULL)
    return login_end(adr);
  return adr_nologin_r(hts_buf_at(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt)),
                       adr);
}

const char *url_nologin(httrackp *opt, const char *url) {
  return login_start(url) != url ? adr_nologin(opt, url) : url;
}

// Returns source past its login (user:pass@), if any.
HTSEXT_API const char *jump_identification_const(const char *source) {
  hts_url_view v;

  if (strcmp(source, "file://") == 0)
    return source;
  (void) hts_url_view_parse(hts_span_of(source), &v);
  // a scheme the engine does not know is kept whole
  if (hts_span_present(v.scheme) && jump_protocol_const(source) == source)
    return source;
  return v.host.p;
}

/* clang-format reads the macro line as an unfinished statement. */
/* clang-format off */
HTSEXT_API DECLARE_NON_CONST_VERSION(jump_identification)

const char *jump_normalized_const(const char *source) {
  if (strcmp(source, "file://") == 0)
    return source;
  source = jump_identification_const(source);
  if (strfield(source, "www") && source[3] != '\0') {
    if (source[3] == '.') {     // www.foo.com -> foo.com
      source += 4;
    } else {                    // www-4.foo.com -> foo.com
      const char *a = source + 3;

      while (*a && (hts_isdigit((unsigned char) *a) || *a == '-'))
        a++;
      if (*a == '.') {
        source = a + 1;
      }
    }
  }
  return source;
}

/* clang-format on */

static int sortNormFnc(const void *a_, const void *b_) {
  const hts_query_field *const a = (const hts_query_field *) a_;
  const hts_query_field *const b = (const hts_query_field *) b_;
  const size_t n = a->whole.len < b->whole.len ? a->whole.len : b->whole.len;
  const int c = memcmp(a->whole.p, b->whole.p, n);

  if (c != 0)
    return c;
  return a->whole.len < b->whole.len ? -1 : a->whole.len > b->whole.len ? 1 : 0;
}

/* Path normalizer core: optionally collapse redundant '//' (DO_SLASH) and/or
   sort query arguments (DO_QUERY) so equivalent URLs dedupe. */
static char *fil_normalized_ex(const char *source, char *dest, int do_slash,
                               int do_query) {
  char lastc = 0;
  int gotquery = 0;
  size_t i, j;
  char *query;

  for(i = j = 0; source[i] != '\0'; i++) {
    if (!gotquery && source[i] == '?') {
      gotquery = 1;
    }
    if (do_slash && !gotquery && lastc == '/' && source[i] == '/') {
      // foo//bar -> foo/bar
    } else {
      dest[j++] = source[i];
    }
    lastc = source[i];
  }
  dest[j++] = '\0';

  /* Sort arguments (&foo=1&bar=2 == &bar=2&foo=1) */
  if (do_query && (query = strchr(dest, '?')) != NULL) {
    const char *cur = query + 1;
    hts_query_field f;
    size_t nfields = 0;

    while (hts_query_next(&cur, &f))
      nfields++;
    if (nfields > 1) {
      hts_query_field *fields = malloct(nfields * sizeof(*fields));
      const size_t qLen = strlen(query);
      char *copyBuff = malloct(qLen + 1);
      htsbuff cb;

      assertf(fields != NULL);
      assertf(copyBuff != NULL);
      cur = query + 1;
      for (i = 0; hts_query_next(&cur, &f); i++) {
        assertf(i < nfields);
        fields[i] = f;
      }
      assertf(i == nfields);

      qsort(fields, nfields, sizeof(*fields), sortNormFnc);

      /* Replace query by sorted query, of the same length */
      cb = htsbuff_ptr(copyBuff, qLen + 1);
      for (i = 0; i < nfields; i++) {
        htsbuff_catc(&cb, i == 0 ? '?' : '&');
        htsbuff_catn(&cb, fields[i].whole.p, fields[i].whole.len);
      }
      assertf(cb.len == qLen);
      strlcpybuff(query, copyBuff, qLen + 1);

      freet(fields);
      freet(copyBuff);
    }
  }

  return dest;
}

char *fil_normalized(const char *source, char *dest) {
  return fil_normalized_ex(source, dest, 1, 1);
}

/* see htscore.h */
hts_boolean hts_query_key_stripped(hts_span key, const char *strip) {
  hts_span rest = hts_span_of(strip);
  hts_span tok;

  while (hts_span_next_in(&rest, ',', &tok)) {
    tok = hts_span_trim(tok, " ", " ");
    if (hts_span_eq(tok, hts_span_of("*")) || hts_span_eq(tok, key))
      return HTS_TRUE;
  }
  return HTS_FALSE;
}

/* see htscore.h */
char *fil_normalized_filtered_ex(const char *source, char *dest,
                                 const char *strip, int do_slash,
                                 int do_query) {
  const char *query;

  /* No strip list, or no query: plain normalization. */
  if (strip == NULL || *strip == '\0' ||
      (query = strchr(source, '?')) == NULL) {
    return fil_normalized_ex(source, dest, do_slash, do_query);
  }

  /* Copy the path, re-emit kept query args, let fil_normalized() sort. Walk
     every field incl. empty/trailing ("a&","?&&") so the result is a fixpoint
     (the read re-normalizes it; a dropped empty arg would miss dedup). */
  {
    char BIGSTK tmp[HTS_URLMAXSIZE * 2];
    htsbuff cb = htsbuff_ptr(tmp, sizeof(tmp));
    hts_query_field f;
    const char *cur;
    int wrote = 0;

    htsbuff_catn(&cb, source, (size_t) (query - source));
    for (cur = query + 1; hts_query_next(&cur, &f);) {
      if (!hts_query_key_stripped(f.key, strip)) {
        htsbuff_catc(&cb, wrote ? '&' : '?');
        htsbuff_catn(&cb, f.whole.p, f.whole.len);
        wrote = 1;
      }
    }
    return fil_normalized_ex(tmp, dest, do_slash, do_query);
  }
}

/* see htscore.h */
char *fil_normalized_filtered(const char *source, char *dest,
                              const char *strip) {
  return fil_normalized_filtered_ex(source, dest, strip, 1, 1);
}

/* hts_query_strip_keys() once some strip rules are known to exist */
static const char *hts_query_strip_keys_match(const char *rules,
                                              const char *adr, const char *fil,
                                              hts_buf dest) {
  const char *q;
  const char *result = NULL;
  hts_span rest, line;
  /* holds a host and a path, each up to HTS_URLMAXSIZE * 2 */
  char BIGSTK url[HTS_URLMAXSIZE * 4];

  /* Match the normalized host/path without the query, so read and write agree.
     A host/path longer than url[] is hostile input and is clipped, because it
     only changes which rule matches. */
  (void) strclipbuff(url, sizeof(url), jump_normalized_const(adr));
  if (fil[0] != '/')
    (void) strclipbuff(&url[strlen(url)], sizeof(url) - strlen(url), "/");
  q = strchr(fil, '?');
  {
    const size_t room = sizeof(url) - strlen(url) - 1;
    const size_t fillen = q != NULL ? (size_t) (q - fil) : strlen(fil);

    strlncatbuff(url, fil, sizeof(url), fillen < room ? fillen : room);
  }

  rest = hts_span_of(rules);
  /* Walk the '\n' entries; last match wins (like the +/- filter eval). Each is
     "pattern=keys"; no '=' is the bare form, pattern "*". */
  while (hts_span_next_in(&rest, '\n', &line)) {
    hts_span pattern, keys;
    char BIGSTK pat[HTS_URLMAXSIZE * 2];

    if (hts_span_empty(line))
      continue;
    if (hts_span_split(line, '=', &pattern, &keys)) {
      /* Clip a too-long pattern, never skip it. */
      (void) !hts_buf_put_span_clip(HTS_BUF(pat), pattern);
    } else {
      strcpybuff(pat, "*");
      keys = line;
    }
    if (strjoker(url, pat, NULL, NULL) != NULL) {
      /* a user rule; every key lookup clips it alike */
      (void) !hts_buf_put_span_clip(dest, keys);
      result = dest.p;
    }
  }
  return result;
}

/* see htscore.h */
const char *hts_query_strip_keys(const char *rules, const char *adr,
                                 const char *fil, hts_buf dest) {
  if (rules == NULL || *rules == '\0' || dest.size == 0)
    return NULL;
  return hts_query_strip_keys_match(rules, adr, fil, dest);
}

/* see htscore.h */
const char *hts_host_alias_rules(httrackp *opt) {
  if (opt->hash != NULL)
    return opt->hash->host_alias;
  return StringNotEmpty(opt->host_alias) ? StringBuff(opt->host_alias) : NULL;
}

/* see htslib.h */
const char *hts_strip_query_rules(httrackp *opt) {
  if (opt->hash != NULL)
    return opt->hash->strip_query;
  return StringNotEmpty(opt->strip_query) ? StringBuff(opt->strip_query) : NULL;
}

/* Split RULE "alias[,alias...]=canonical" at its first '=', both sides
   untrimmed. The matcher and the validator share it so they agree on what a
   rule is. HTS_FALSE when either side is empty. */
static hts_boolean hts_host_alias_split(hts_span rule, hts_span *aliases,
                                        hts_span *canon) {
  return hts_span_split(rule, '=', aliases, canon) &&
         !hts_span_empty(*aliases) && !hts_span_empty(*canon);
}

/* see htscore.h */
const char *hts_host_alias_match(const char *rules, const char *host,
                                 const char *full, hts_boolean collapse_www,
                                 size_t *canonlen) {
  hts_span rest = hts_span_of(rules);
  const char *found = NULL;
  hts_span line;

  while (hts_span_next_in(&rest, '\n', &line)) {
    hts_span aliases, canon, alias;

    if (!hts_host_alias_split(line, &aliases, &canon))
      continue;
    while (hts_span_next_in(&aliases, ',', &alias)) {
      char BIGSTK glob[HTS_URLMAXSIZE * 2];

      /* A trailing slash is not part of a path. */
      alias = hts_span_trim(alias, " \t", " \t/");
      if (!hts_span_empty(alias) && hts_span_copy(alias, HTS_SIZED(glob))) {
        char *ghost;

        /* the collapse applies to the pattern too, so a rule may spell either
           www.example.com or example.com under -%u */
        ghost = glob + (jump_protocol_const(glob) - glob);
        if (collapse_www) {
          const char *const bare = jump_normalized_const(ghost);

          if (bare != ghost)
            memmove(ghost, bare, strlen(bare) + 1);
        }
        /* a rule that names no scheme keeps matching every scheme */
        if (strjoker(strstr(glob, "://") != NULL ? full : host,
                     strstr(glob, "://") != NULL ? glob : ghost, NULL,
                     NULL) != NULL) {
          /* Trim here, because the resolver compares each match with the
             next to see a chain settle. */
          canon = hts_span_trim(canon, " \t", " \t/");
          found = canon.p;
          *canonlen = canon.len;
          break;
        }
      }
    }
  }
  return found;
}

/* Build "scheme://host" from URL's scheme and HOST, for the rules that name a
   scheme. HTS_FALSE if it does not fit DEST. */
static hts_boolean hts_host_alias_subject(const char *url, const char *host,
                                          hts_buf dest) {
  /* An address with no scheme is the engine's plain-http form. Spell that one
     out, or a rule naming http:// could never match anything. */
  const hts_boolean bare =
      jump_protocol_const(url) == url ? HTS_TRUE : HTS_FALSE;
  const char *const scheme = bare ? "http://" : url;
  const size_t schemelen =
      bare ? strlen("http://") : (size_t) (jump_protocol_const(url) - url);
  const size_t hostlen = strlen(host);

  if (schemelen >= dest.size || hostlen >= dest.size - schemelen)
    return HTS_FALSE;
  memcpy(dest.p, scheme, schemelen);
  memcpy(dest.p + schemelen, host, hostlen);
  dest.p[schemelen + hostlen] = '\0';
  return HTS_TRUE;
}

/* Chain hops resolved before giving up. A longer chain, or a loop, is a
   misconfiguration; the fold leaves those hosts alone rather than settle on an
   arbitrary hop, which would depend on how often a link is named. */
enum { HTS_HOST_ALIAS_MAXHOPS = 8 };

/* hts_host_alias_resolve() once some alias rules are known to exist */
static const char *hts_host_alias_resolve_rules(const char *rules,
                                                const char *adr,
                                                hts_boolean collapse_www,
                                                hts_buf dest,
                                                hts_boolean *settled) {
  const char *proto, *host, *hostsubj, *canon;
  char BIGSTK fullsubj[HTS_URLMAXSIZE * 2];
  char schemebuf[32]; /* longest jump_protocol_const knows is "socks5h://" */
  size_t canonlen = 0, schemelen;
  int hop;

  proto = jump_protocol_const(adr);      /* after the scheme */
  host = jump_identification_const(adr); /* after the credentials */
  schemelen = (size_t) (proto - adr);
  if (!hts_buf_put_span(HTS_BUF(schemebuf), hts_span_between(adr, proto)))
    return NULL;
  /* Match the host as the dedup layer does, so with the www. collapse on, a
     rule naming example.com also covers www.example.com. */
  hostsubj = collapse_www ? jump_normalized_const(host) : host;
  if (!hts_host_alias_subject(adr, hostsubj, HTS_BUF(fullsubj)))
    return NULL;
  canon =
      hts_host_alias_match(rules, hostsubj, fullsubj, collapse_www, &canonlen);
  if (canon == NULL)
    return NULL;

  /* Follow the chain to its end, so the fold is a fixpoint: url_savename names
     the same link more than once, each time from the host it wrote last. */
  for (hop = 0;; hop++) {
    char BIGSTK canonbuf[HTS_URLMAXSIZE * 2];
    const char *next, *dhost, *chost, *cproto;
    size_t nextlen = 0, credlen, hostlen;

    /* A rule may name the canonical host's scheme. If not, the scheme in
       effect stays, which may come from an earlier rule in the chain. */
    if (!hts_buf_put_span(HTS_BUF(canonbuf), hts_span_at(canon, canonlen)))
      return NULL;
    /* the link's credentials are the ones kept, so drop any the rule carries:
       they would otherwise be prepended to them again on every re-fold */
    cproto = jump_protocol_const(canonbuf);
    chost = jump_identification_const(cproto);
    hostlen = canonlen - (size_t) (chost - canonbuf);
    if (cproto != canonbuf) { /* the rule names a scheme */
      if (!hts_buf_put_span(HTS_BUF(schemebuf),
                            hts_span_between(canonbuf, cproto)))
        return NULL;
      schemelen = (size_t) (cproto - canonbuf);
    }
    credlen = (size_t) (host - proto);

    if (hostlen == 0)
      return NULL; /* a rule naming no host: leave the URL alone */
    if (schemelen >= dest.size || credlen >= dest.size - schemelen ||
        hostlen >= dest.size - schemelen - credlen)
      return NULL; /* would not fit: leave the URL alone */
    memcpy(dest.p, schemebuf, schemelen);
    memcpy(dest.p + schemelen, proto, credlen);
    memcpy(dest.p + schemelen + credlen, chost, hostlen);
    dest.p[schemelen + credlen + hostlen] = '\0';

    dhost = jump_identification_const(dest.p);
    if (collapse_www)
      dhost = jump_normalized_const(dhost);
    if (!hts_host_alias_subject(dest.p, dhost, HTS_BUF(fullsubj)))
      return NULL;
    next = hts_host_alias_match(rules, dhost, fullsubj, collapse_www, &nextlen);
    if (next == NULL)
      break; /* settled: no rule maps this host further */
    if (nextlen == canonlen && hts_strncasecmp(next, canon, canonlen) == 0)
      break; /* a rule matching its own target */
    if (hop + 1 >= HTS_HOST_ALIAS_MAXHOPS) {
      if (settled != NULL)
        *settled = HTS_FALSE;
      return NULL;
    }
    canon = next;
    canonlen = nextlen;
  }
  /* a host that maps to itself: keep the callers' fast path */
  return hts_strcasecmp(dest.p, adr) != 0 ? dest.p : NULL;
}

/* see htscore.h */
const char *hts_host_alias_resolve(const char *rules, const char *adr,
                                   hts_boolean collapse_www, hts_buf dest,
                                   hts_boolean *settled) {
  if (settled != NULL)
    *settled = HTS_TRUE;
  if (rules == NULL || *rules == '\0' || adr == NULL || dest.size == 0)
    return NULL;
  /* engine pseudo-hosts: a "*=host" rule must not swallow them */
  if (adr[0] == '\0' || strcmp(adr, "primary") == 0 ||
      strcmp(adr, "file://") == 0)
    return NULL;
  return hts_host_alias_resolve_rules(rules, adr, collapse_www, dest, settled);
}

/* see htscore.h */
const char *hts_host_alias(const char *rules, const char *adr,
                           hts_boolean collapse_www, hts_buf dest) {
  return hts_host_alias_resolve(rules, adr, collapse_www, dest, NULL);
}

/* see htscore.h */
hts_boolean hts_host_alias_collapse_www(httrackp *opt) {
  return opt->urlhack && !opt->no_www_dedup ? HTS_TRUE : HTS_FALSE;
}

/* see htscore.h */
const char *hts_host_alias_fold(httrackp *opt, lien_adrfil *af) {
  const char *const rules = hts_host_alias_rules(opt);

  if (rules != NULL) {
    char BIGSTK canon[HTS_URLMAXSIZE * 2];
    const char *const alias = hts_host_alias(
        rules, af->adr, hts_host_alias_collapse_www(opt), HTS_BUF(canon));

    if (alias != NULL)
      strcpybuff(af->adr, alias);
  }
  return af->adr;
}

/* see htscore.h */
hts_boolean hts_host_same_alias(const char *rules, const char *adra,
                                const char *adrb, hts_boolean collapse_www) {
  char BIGSTK ca[HTS_URLMAXSIZE * 2], cb[HTS_URLMAXSIZE * 2];
  const char *a, *b;

  if (rules == NULL || *rules == '\0' || adra == NULL || adrb == NULL)
    return HTS_FALSE;
  a = hts_host_alias(rules, adra, collapse_www, HTS_BUF(ca));
  b = hts_host_alias(rules, adrb, collapse_www, HTS_BUF(cb));
  if (a == NULL && b == NULL)
    return HTS_FALSE; /* neither is aliased: the caller's own test */
  return hts_strcasecmp(jump_identification_const(a != NULL ? a : adra),
                        jump_identification_const(b != NULL ? b : adrb)) == 0
             ? HTS_TRUE
             : HTS_FALSE;
}

/* HTS_TRUE if TOKEN is a host, optionally behind the scheme it speaks
   and with a trailing slash. GLOB keeps the filter metacharacters legal, which
   only the alias side of a rule may use. */
static hts_boolean hts_host_alias_token_ok(hts_span token, hts_boolean glob) {
  char BIGSTK buff[HTS_URLMAXSIZE * 2];
  const char *host;
  size_t len;

  token = hts_span_trim(token, " \t", " \t");
  if (hts_span_empty(token) || !hts_span_copy(token, HTS_SIZED(buff)))
    return HTS_FALSE;
  len = token.len;
  while (len > 0 && buff[len - 1] == '/')
    buff[--len] = '\0'; /* the matcher strips the whole run */
  /* an alias may glob its scheme; an unknown canonical one lands in the host */
  host = glob && strstr(buff, "://") != NULL ? strstr(buff, "://") + 3
                                             : jump_protocol_const(buff);
  /* a scheme with no host behind it, and the engine's own pseudo-host, name
     nothing the fold could write into an address */
  if (*host == '\0' || strcmp(host, "primary") == 0)
    return HTS_FALSE;
  /* --host-alias maps hosts: a path belongs to no part of an address */
  if (strchr(host, '/') != NULL)
    return HTS_FALSE;
  /* a control byte names no host, and '\n' would split the store's rule list */
  for (; *host != '\0'; host++) {
    if ((unsigned char) *host < ' ' || *host == '\x7f')
      return HTS_FALSE;
    if (strchr(glob ? "=# " : "=,*?();\\#@ ", *host) != NULL)
      return HTS_FALSE;
  }
  return HTS_TRUE;
}

/* see httrack-library.h */
HTSEXT_API hts_boolean hts_host_alias_rule_ok(const char *rule) {
  hts_span aliases, canon, alias;

  if (!hts_host_alias_split(hts_span_of(rule), &aliases, &canon) ||
      !hts_host_alias_token_ok(canon, HTS_FALSE))
    return HTS_FALSE;
  while (hts_span_next_in(&aliases, ',', &alias)) {
    if (!hts_host_alias_token_ok(alias, HTS_TRUE))
      return HTS_FALSE;
  }
  return HTS_TRUE;
}

/* see htscore.h */
const char *hts_host_alias_looping(const char *rules, hts_boolean collapse_www,
                                   hts_buf dest) {
  hts_span rest, line, aliases, target;

  if (rules == NULL || dest.size == 0)
    return NULL;
  rest = hts_span_of(rules);
  while (hts_span_next_in(&rest, '\n', &line)) {
    char BIGSTK canon[HTS_URLMAXSIZE * 2], tmp[HTS_URLMAXSIZE * 2];
    hts_boolean settled;

    if (!hts_span_split(line, '=', &aliases, &target) ||
        hts_span_empty(target) || !hts_span_copy(target, HTS_SIZED(canon)))
      continue;
    (void) hts_host_alias_resolve(rules, canon, collapse_www, HTS_BUF(tmp),
                                  &settled);
    if (!settled) {
      if (strlen(canon) >= dest.size)
        continue; /* Skip a name that does not fit. */
      hts_buf_cpy(dest, canon);
      return dest.p;
    }
  }
  return NULL;
}

#define endwith(a) ( (len >= (sizeof(a)-1)) ? ( strncmp(dest, a+len-(sizeof(a)-1), sizeof(a)-1) == 0 ) : 0 );
HTSEXT_API char *adr_normalized_sized(const char *source, char *dest,
                                      size_t destsize) {
  /* not yet too aggressive (no com<->net<->org checkings) */
  strlcpybuff(dest, jump_normalized_const(source), destsize);
  return dest;
}

// deprecated variant; kept for ABI compatibility. Bounds to the implicit
// contract the old callers relied on (an HTS_URLMAXSIZE*2 URL buffer).
HTSEXT_API char *adr_normalized(const char *source, char *dest) {
  return adr_normalized_sized(source, dest, HTS_URLMAXSIZE * 2);
}

#undef endwith

int hts_basic_auth_value(httrackp *opt, const char *url, hts_buf out) {
  hts_url_view v;
  size_t idlen;
  char user_pass[768];

  assertf(out.size >= 1100); /* base-64 of 765 bytes is 1020 */
  out.p[0] = '\0';
  (void) hts_url_view_parse(hts_span_of(url), &v);
  // no login where jump_identification_const() keeps the URL whole
  idlen = jump_identification_const(url) == v.host.p ? v.userinfo.len : 0;
  if (idlen == 0)
    return 0;
  /* a clipped credential authenticates as somebody else, and an encoded one
     wider than bauth_chain.auth could not be stored for the child pages */
  if (idlen > (sizeof(((bauth_chain *) 0)->auth) - 1) / 4 * 3 ||
      !hts_buf_put_span(HTS_BUF(user_pass), v.userinfo))
    return -1;
  strcpybuff(user_pass, unescape_http(OPT_GET_BUFF(opt), OPT_GET_BUFF_SIZE(opt),
                                      user_pass));
  code64(hts_span_of(user_pass), out, HTS_FALSE);
  hts_secret_add(opt, hts_span_of(out.p), HTS_SECRET_HEADER);
  return 1;
}

hts_boolean hts_sni_hostname(const char *url_adr, hts_buf out) {
  hts_url_view v;

  (void) hts_url_view_parse(hts_span_of(url_adr), &v);
  out.p[0] = '\0';
  if (hts_span_empty(v.host) ||
      hts_span_starts_with(v.host, hts_span_of("[")) ||
      hts_host_is_ipv4(v.host))
    return HTS_FALSE;
  return hts_buf_put_span(out, v.host);
}

#if HTS_USEOPENSSL
static htsmutex ssl_ca_lock = HTSMUTEX_INIT;
static int ssl_ca_state = 0; /* 0 not tried, 1 loaded, -1 failed */

hts_boolean hts_ssl_check_cert(SSL *ssl, const char *url_adr) {
  X509_VERIFY_PARAM *const param = SSL_get0_param(ssl);
  char name[256];
  int state;

  hts_mutexlock(&ssl_ca_lock);
  if (ssl_ca_state == 0)
    ssl_ca_state = SSL_CTX_set_default_verify_paths(openssl_ctx) == 1 ? 1 : -1;
  state = ssl_ca_state;
  hts_mutexrelease(&ssl_ca_lock);
  if (state != 1 || param == NULL)
    return HTS_FALSE;
  if (hts_sni_hostname(url_adr, HTS_BUF(name))) {
    const size_t len = strlen(name);

    /* certificates name "host", never the absolute "host." */
    if (len > 1 && name[len - 1] == '.')
      name[len - 1] = '\0';
    if (X509_VERIFY_PARAM_set1_host(param, name, 0) != 1)
      return HTS_FALSE;
  } else {
    /* an address literal must match an IP name of the certificate */
    const char *const host = jump_identification_const(url_adr);
    const char *const start = host[0] == '[' ? host + 1 : host;
    const size_t len = strcspn(start, host[0] == '[' ? "]" : ":/?#\\");

    if (len == 0 || !hts_buf_put_span(HTS_BUF(name), hts_span_at(start, len)))
      return HTS_FALSE;
    if (X509_VERIFY_PARAM_set1_ip_asc(param, name) != 1)
      return HTS_FALSE;
  }
  SSL_set_verify(ssl, SSL_VERIFY_PEER, NULL);
  return HTS_TRUE;
}
#endif

const char *hts_authority_end(const char *s) { return s + strcspn(s, "/?#\\"); }

/* jump_protocol_const() and the view both read these schemes. A scheme longer
   than a row does not compile. */
static const char hts_engine_schemes[][8] = {
    "http", "ftp", "https", "file", "socks5h", "socks5", "connect"};

/* The most bytes an engine scheme and its ':' span. */
#define ENGINE_SCHEME_SPAN (sizeof(hts_engine_schemes[0]) + 1)

/* Return the length of the engine scheme and ':' that start S, or 0. */
static size_t engine_scheme_len(hts_span s) {
  size_t i;

  for (i = 0; i < sizeof(hts_engine_schemes) / sizeof(hts_engine_schemes[0]);
       i++) {
    const char *const name = hts_engine_schemes[i];
    const size_t n = sizeof(hts_engine_schemes[i]);
    size_t j;

    for (j = 0;
         j < n && name[j] != '\0' && j < s.len && streql(s.p[j], name[j]); j++)
      ;
    if (j < s.len && s.p[j] == ':' && (j == n || name[j] == '\0'))
      return j + 1;
  }
  return 0;
}

/* Is the span a decimal port in 0..65535? Empty and 0 pass, because
   hts_parse_url_port() checks the 1..65535 range. */
static hts_boolean port_is_valid(hts_span port) {
  LLint value;

  return hts_span_empty(port) || hts_span_parse_llint(port, 0, 65535, &value);
}

hts_boolean hts_url_view_parse(hts_span url, hts_url_view *v) {
  const char *const s = url.p;
  const char *const end = s + url.len;
  const char *p = s;
  const char *auth_end, *at, *q;
  hts_span auth;
  hts_boolean ok = HTS_TRUE;
  size_t n;

  memset(v, 0, sizeof(*v));
  if ((n = engine_scheme_len(url)) != 0) {
    v->scheme = hts_span_at(s, n - 1);
    p = s + n;
  } else if (url.len != 0 && hts_isalpha((unsigned char) s[0])) {
    for (q = s + 1; q < end && (hts_isalnum((unsigned char) *q) || *q == '+' ||
                                *q == '-' || *q == '.');
         q++)
      ;
    if (hts_span_starts_with(hts_span_between(q, end), hts_span_of("://"))) {
      v->scheme = hts_span_between(s, q);
      p = q + 1;
    }
  }
  if (hts_span_starts_with(hts_span_between(p, end), hts_span_of("//")))
    p += 2;

  // the same set as hts_authority_end(), but a NUL is data here
  for (auth_end = p; auth_end < end && *auth_end != '/' && *auth_end != '?' &&
                     *auth_end != '#' && *auth_end != '\\';
       auth_end++)
    ;
  for (at = NULL, q = p; q < auth_end; q++)
    if (*q == '@')
      at = q;
  if (at != NULL) {
    v->userinfo = hts_span_between(p, at);
    p = at + 1;
  }

  auth = hts_span_between(p, auth_end);
  if (hts_span_starts_with(auth, hts_span_of("["))) {
    hts_span inside, rest;

    if (hts_span_split(auth, ']', &inside, &rest) &&
        (hts_span_empty(rest) ||
         hts_span_starts_with(rest, hts_span_of(":")))) {
      v->host = hts_span_at(auth.p, inside.len + 1);
      if (!hts_span_empty(rest))
        v->port = hts_span_skip(rest, 1);
    } else {
      v->host = auth;
      ok = HTS_FALSE;
    }
  } else if (!hts_span_split(auth, ':', &v->host, &v->port)) {
    v->host = auth;
  }
  if (hts_span_present(v->port) && !port_is_valid(v->port))
    ok = HTS_FALSE;

  for (q = auth_end; q < end && *q != '?' && *q != '#'; q++)
    ;
  v->path = hts_span_between(auth_end, q);
  if (q < end && *q == '?') {
    const char *const query = q + 1;

    for (q = query; q < end && *q != '#'; q++)
      ;
    v->query = hts_span_between(query, q);
  }
  if (q < end)
    v->fragment = hts_span_between(q + 1, end);
  return ok;
}

// find the authority's port (:80) or NULL; handles IPv6 literals
const char *jump_toport_const(const char *source) {
  hts_url_view v;

  (void) hts_url_view_parse(hts_span_of(source), &v);
  // jump_identification_const() keeps these whole, so their ':' is the port
  if (hts_span_present(v.scheme) &&
      (strcmp(source, "file://") == 0 || jump_protocol_const(source) == source))
    return source + v.scheme.len;
  return hts_span_present(v.port) ? v.port.p - 1 : NULL;
}

DECLARE_NON_CONST_VERSION(jump_toport)

// strrchr, but not too far
const char *strrchr_limit(const char *s, char c, const char *limit) {
  if (limit == NULL) {
    const char *p = strrchr(s, c);

    return p ? (p + 1) : NULL;
  } else {
    const char *a = NULL, *p;

    for(;;) {
      p = strchr((a) ? a : s, c);
      if ((p >= limit) || (p == NULL))
        return a;
      a = p + 1;
    }
  }
}

// Returns source past its engine scheme and "//", if any.
const char *jump_protocol_const(const char *source) {
  // "Comparisons of scheme names MUST be case-insensitive" (RFC2616)
  source += engine_scheme_len(
      hts_span_at(source, strnlen(source, ENGINE_SCHEME_SPAN)));
  // net_path
  if (strncmp(source, "//", 2) == 0)
    source += 2;
  return source;
}

DECLARE_NON_CONST_VERSION(jump_protocol)

hts_boolean hts_span_is_control_free(hts_span s) {
  size_t i;

  for (i = 0; i < s.len; i++) {
    if ((unsigned char) s.p[i] < ' ')
      return HTS_FALSE;
  }
  return HTS_TRUE;
}

hts_boolean hts_is_control_free(const char *str) {
  return hts_span_is_control_free(hts_span_of(str));
}

hts_boolean hts_host_is_ipv4(hts_span host) {
  size_t i;
  int dots = 0;

  for (i = 0; i < host.len; i++) {
    if (host.p[i] == '.')
      dots++;
    else if (host.p[i] < '0' || host.p[i] > '9')
      return HTS_FALSE;
  }
  return dots > 0 ? HTS_TRUE : HTS_FALSE;
}

hts_boolean hts_host_is_address(hts_span host) {
  return hts_span_find(host, ':') < host.len || hts_host_is_ipv4(host);
}

hts_boolean hts_proxy_is_socks(const char *name) {
  if (name == NULL)
    return HTS_FALSE;
  return (strfield(name, "socks5h:") || strfield(name, "socks5:")) ? HTS_TRUE
                                                                   : HTS_FALSE;
}

hts_boolean hts_proxy_is_connect(const char *name) {
  if (name == NULL)
    return HTS_FALSE;
  return strfield(name, "connect:") ? HTS_TRUE : HTS_FALSE;
}

// default proxy port for a -P argument, keyed on the scheme
static int proxy_default_port(const char *arg) {
  return hts_proxy_is_socks(arg) ? 1080 : 8080;
}

// port "a" of -P argument "arg": digits fitting TCP's 1..65535, else the scheme
// default. Not sscanf("%d"): past INT_MAX it wraps to a garbage port (#602)
static int parse_proxy_port(const char *a, const char *arg) {
  int port;

  if (!hts_parse_url_port(a, &port))
    return proxy_default_port(arg);
  return port;
}

void hts_parse_proxy(const char *arg, hts_buf name, int *port) {
  const char *authority = strstr(arg, "://");
  const char *a, *at;
  size_t namelen, i;

  if (name.size == 0)
    return;
  // scan back to the port ':' (or userinfo '@'), never past the authority (a
  // scheme's own colon is not a port separator) nor into an IPv6 literal (']'
  // stops it); inspect a[-1] from one-past-end so no pointer underflows
  authority = (authority != NULL) ? authority + 3 : arg;
  a = arg + strlen(arg);
  while (a > authority && a[-1] != ':' && a[-1] != '@' && a[-1] != ']')
    a--;
  if (a > authority && a[-1] == ':') {
    *port = parse_proxy_port(a, arg);
    namelen = (size_t) (a - 1 - arg);
  } else {
    *port = proxy_default_port(arg);
    namelen = strlen(arg);
  }
  at = authority;
  for (i = (size_t) (authority - arg); i < namelen; i++)
    if (arg[i] == '@')
      at = arg + i;
  // escape what would end the authority early: the login is unescaped later
  (void) hts_escape_span(hts_span_at(arg, namelen),
                         hts_span_between(authority, at), "/?#\\", name);
}

static hts_boolean hts_escape_span(hts_span src, hts_span escaped,
                                   const char *set, hts_buf dst) {
  size_t i, len = 0;

  if (dst.size == 0)
    return HTS_FALSE;
  for (i = 0; i < src.len; i++) {
    const char *const c = src.p + i;

    if (c >= escaped.p && c < escaped.p + escaped.len &&
        strchr(set, *c) != NULL) {
      if (dst.size - len <= 3)
        break;
      snprintf(dst.p + len, 4, "%%%02X", (unsigned char) *c);
      len += 3;
    } else {
      if (dst.size - len <= 1)
        break;
      dst.p[len++] = *c;
    }
  }
  dst.p[len] = '\0';
  return i == src.len ? HTS_TRUE : HTS_FALSE;
}

void code64(hts_span in, hts_buf out, hts_boolean crlf) {
  static const char alphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  const unsigned char *const a = (const unsigned char *) in.p;
  const size_t groups = (in.len + 2) / 3;
  size_t i, j = 0, line = 0;

  assertf(groups * 4 + (crlf ? groups / (HTS_BASE64_LINE_BYTES / 3) * 2 : 0) <
          out.size);
  for (i = 0; i < in.len; i += 3) {
    const size_t n = in.len - i < 3 ? in.len - i : 3;
    const unsigned long store = (unsigned long) a[i] << 16 |
                                (n > 1 ? (unsigned long) a[i + 1] << 8 : 0) |
                                (n > 2 ? a[i + 2] : 0);

    out.p[j++] = alphabet[(store >> 18) & 63];
    out.p[j++] = alphabet[(store >> 12) & 63];
    out.p[j++] = n > 1 ? alphabet[(store >> 6) & 63] : '=';
    out.p[j++] = n > 2 ? alphabet[store & 63] : '=';
    if (crlf && ++line == HTS_BASE64_LINE_BYTES / 3) {
      line = 0;
      out.p[j++] = '\r';
      out.p[j++] = '\n';
    }
  }
  out.p[j] = '\0';
}

void unescape_amp(char *s) {
  if (hts_unescapeEntities(s, hts_buf_at(s, strlen(s) + 1)) != 0) {
    assertf(! "error escaping html entities");
  }
}

// remplacer %20 par ' ', etc..
// buffer MAX 1Ko
HTSEXT_API char *unescape_http(char *const catbuff, const size_t size, const char *const s) {
  size_t i, j;

  RUNTIME_TIME_CHECK_SIZE(size);

  for(i = 0, j = 0; s[i] != '\0' && j + 1 < size ; i++) {
    int h;
    if (s[i] == '%' && (h = hts_ehex(&s[i + 1])) >= 0) {
      catbuff[j++] = (char) h;
      i += 2;
    } else
      catbuff[j++] = s[i];
  }
  catbuff[j++] = '\0';
  return catbuff;
}

// unescape in URL/URI ONLY what has to be escaped, to form a standard URL/URI
// DOES NOT DECODE %25 (part of CHAR_DELIM)
// no_high & 1: keep high chars (>= 127) escaped
// no_high & 2: decode space
HTSEXT_API char *unescape_http_unharm(char *const catbuff, const size_t size,
                                      const char *s,
                                      const hts_boolean no_high) {
  size_t i, j;

  RUNTIME_TIME_CHECK_SIZE(size);

  for(i = 0, j = 0; s[i] != '\0' && j + 1 < size ; i++) {
    if (s[i] == '%') {
      const int nchar = hts_ehex(&s[i + 1]);

      const int test = 
        ( CHAR_RESERVED(nchar) && nchar != '+' )        /* %2B => + (not in query!) */
        || CHAR_DELIM(nchar)
        || CHAR_UNWISE(nchar)
        || CHAR_LOW(nchar)    /* CHAR_SPECIAL */
        || ( CHAR_XXAVOID(nchar) && ( nchar != ' ' || ( no_high & 2) == 0 ) )
        || ( ( no_high & 1 ) && CHAR_HIG(nchar) )
        ;

      if (!test && nchar >= 0) {  /* can safely unescape */
        catbuff[j++] = (char) nchar;
        i += 2;
      } else {
        catbuff[j++] = '%';
      }
    } else {
      catbuff[j++] = s[i];
    }
  }
  catbuff[j++] = '\0';
  return catbuff;
}

// remplacer " par %xx etc..
// buffer MAX 1Ko
size_t escape_spc_url(const char *const src, char *const dest,
                      const size_t size) {
  return x_escape_http(src, hts_buf_at(dest, size), 2);
}

// smith / john -> smith%20%2f%20john
HTSEXT_API size_t escape_in_url(const char *const src, 
                                char *const dest, const size_t size) {
  return x_escape_http(src, hts_buf_at(dest, size), 1);
}

// smith / john -> smith%20/%20john
size_t escape_uri(const char *const src, char *const dest, const size_t size) {
  return x_escape_http(src, hts_buf_at(dest, size), 3);
}

size_t escape_uri_utf(const char *const src, char *const dest,
                      const size_t size) {
  return x_escape_http(src, hts_buf_at(dest, size), 30);
}

HTSEXT_API size_t escape_check_url(const char *const src, char *const dest,
                                   const size_t size) {
  return x_escape_http(src, hts_buf_at(dest, size), 0);
}

size_t escape_control_url(const char *const src, hts_buf dest) {
  return x_escape_http(src, dest, 4);
}

// same as escape_check_url, but returns char*
char *escape_check_url_addr(const char *const src, hts_buf dest) {
  escape_check_url(src, dest.p, dest.size);
  return dest.p;
}

// Same as above, but appending to "dest"
#undef DECLARE_APPEND_ESCAPE_VERSION
/* clang-format off: an edit realigns all backslashes, churning the macro. */
/* clang-format off */
#define DECLARE_APPEND_ESCAPE_VERSION(NAME) \
size_t append_ ##NAME(const char *const src, hts_buf dest) { \
  const size_t len = strnlen(dest.p, dest.size); \
  assertf(len < dest.size); \
  RUNTIME_TIME_CHECK_SIZE(dest.size); \
  /* A near-full buffer can leave the remainder the size heuristic rejects. \
     Report it as the overflow the caller already tests for. */ \
  if (dest.size - len == sizeof(void *)) { \
    return dest.size - len; \
  } \
  return NAME(src, dest.p + len, dest.size - len); \
}
/* clang-format on */

DECLARE_APPEND_ESCAPE_VERSION(escape_in_url)
DECLARE_APPEND_ESCAPE_VERSION(escape_spc_url)
DECLARE_APPEND_ESCAPE_VERSION(escape_check_url)

#undef DECLARE_APPEND_ESCAPE_VERSION

// In-place escaping: copy dest aside, then escape that copy back into dest.
typedef size_t (*escape_fn_t)(const char *src, char *dest, size_t size);

static size_t inplace_escape(hts_buf dest, escape_fn_t escape) {
  char buffer[256];
  const size_t len = strnlen(dest.p, dest.size);
  const int in_buffer = len + 1 < sizeof(buffer);
  char *src = in_buffer ? buffer : malloct(len + 1);
  size_t ret;
  assertf(src != NULL);
  assertf(len < dest.size);
  memcpy(src, dest.p, len + 1);
  ret = escape(src, dest.p, dest.size);
  if (!in_buffer) {
    freet(src);
  }
  return ret;
}

// Thin wrappers binding inplace_escape() to each escaper.
#undef DECLARE_INPLACE_ESCAPE_VERSION
#define DECLARE_INPLACE_ESCAPE_VERSION(NAME)                                   \
  size_t inplace_##NAME(char *const dest, const size_t size) {                 \
    return inplace_escape(hts_buf_at(dest, size), NAME);                       \
  }

DECLARE_INPLACE_ESCAPE_VERSION(escape_in_url)
DECLARE_INPLACE_ESCAPE_VERSION(escape_spc_url)
DECLARE_INPLACE_ESCAPE_VERSION(escape_uri_utf)
DECLARE_INPLACE_ESCAPE_VERSION(escape_check_url)
DECLARE_INPLACE_ESCAPE_VERSION(escape_uri)

#undef DECLARE_INPLACE_ESCAPE_VERSION

size_t make_content_id(const char *const adr, const char *const fil,
                       hts_buf dest) {
  char *a;
  size_t esc_size = escape_in_url(adr, dest.p, dest.size);
  /* the append form takes a remainder the size check would misread */
  if (esc_size < dest.size)
    esc_size += append_escape_in_url(fil, dest);
  RUNTIME_TIME_CHECK_SIZE(dest.size);
  /* A '%' would be decoded again by whoever reads the cid: URL. The marker has
     to be a character escape_in_url() itself escapes, or two URLs can share one
     Content-ID and one part then serves both (#1925). */
  for (a = dest.p; (a = strchr(a, '%')) != NULL; a++) {
    *a = '-';
  }
  return esc_size;
}

// strip all control characters
void escape_remove_control(char *const s) {
  size_t i, j;
  for(i = 0, j = 0 ; s[i] != '\0' ; i++) {
    const unsigned char c = (unsigned  char) s[i];
    if (c >= 32) {
      if (i != j) {
        assertf(j < i);
        s[j] = s[i];
      }
      j++;
    }
  }
  // compaction left the original tail sitting past j
  s[j] = '\0';
}

#undef ADD_CHAR
#define ADD_CHAR(C) do {   \
      assertf(j < size);    \
      if (j + 1 == size) { \
        dest[j] = '\0';    \
        return size;       \
      }                    \
      dest[j++] = (C);     \
  } while(0)

/* Returns the number of characters written (not taking in account the terminating \0), or 'size' upon overflow. */
static size_t x_escape_http(const char *const s, hts_buf out, const int mode) {
  static const char hex[] = "0123456789abcdef";
  char *const dest = out.p; // ADD_CHAR writes through dest and size
  const size_t size = out.size;
  size_t i, j;

  RUNTIME_TIME_CHECK_SIZE(size);

  // Out-of-bound.
  // Previous character is supposed to be the terminating \0.
  if (size == 0) {
    return 0;
  }

  for(i = 0, j = 0 ; s[i] != '\0' ; i++) {
    const unsigned char c = (unsigned char) s[i];
    int test = 0;

    if (mode == 0)
      test = c == '"' || c == ' ' || CHAR_SPECIAL(c);
    else if (mode == 1)
      test = CHAR_RESERVED(c)
             || CHAR_DELIM(c)
             || CHAR_UNWISE(c)
             || CHAR_SPECIAL(c)
             || CHAR_XXAVOID(c)
             || CHAR_MARK(c);
    else if (mode == 2)
      test = c == ' ';       // n'escaper que espace
    else if (mode == 3)         // échapper que ce qui est nécessaire
      test = CHAR_SPECIAL(c)
             || CHAR_XXAVOID(c);
    else if (mode == 4) // C0 controls only, leaving high bytes (UTF-8)
      test = CHAR_LOW(c);
    else if (mode == 30)      // échapper que ce qui est nécessaire
      test = (c != '/' && CHAR_RESERVED(c))
        || CHAR_DELIM(c)
        || CHAR_UNWISE(c)
        || CHAR_SPECIAL(c)
        || CHAR_XXAVOID(c);

    if (!test) {
      ADD_CHAR(c);
    } else {
      ADD_CHAR('%');
      ADD_CHAR(hex[c / 16]);
      ADD_CHAR(hex[c % 16]);
    }
  }

  assertf(j < size);
  dest[j] = '\0';
  return j;
}

size_t escape_for_html_print(const char *const s, char *const dest,
                             const size_t size) {
  size_t i, j;

  RUNTIME_TIME_CHECK_SIZE(size);

  for(i = 0, j = 0 ; s[i] != '\0' ; i++) {
    const unsigned char c = (unsigned char) s[i];
    if (c == '&') {
      ADD_CHAR('&');
      ADD_CHAR('a');
      ADD_CHAR('m');
      ADD_CHAR('p');
      ADD_CHAR(';');
    } else {
      ADD_CHAR(c);
    }
  }
  assertf(j < size);
  dest[j] = '\0';
  return j;
}

size_t escape_for_html_print_full(const char *const s, char *const dest,
                                  const size_t size) {
  static const char hex[] = "0123456789abcdef";
  size_t i, j;

  RUNTIME_TIME_CHECK_SIZE(size);

  for(i = 0, j = 0 ; s[i] != '\0' ; i++) {
    const unsigned char c = (unsigned char) s[i];
    if (c == '&') {
      ADD_CHAR('&');
      ADD_CHAR('a');
      ADD_CHAR('m');
      ADD_CHAR('p');
      ADD_CHAR(';');
    } else if (CHAR_HIG(c)) {
      ADD_CHAR('&');
      ADD_CHAR('#');
      ADD_CHAR('x');
      ADD_CHAR(hex[c / 16]);
      ADD_CHAR(hex[c % 16]);
      ADD_CHAR(';');
    } else {
      ADD_CHAR(c);
    }
  }
  assertf(j < size);
  dest[j] = '\0';
  return j;
}

#undef ADD_CHAR

// -- Utils fichiers

/* Le fichier existe-t-il? (ou est-il accessible?) */
/* Note: NOT utf-8 */
/* Note: preserve errno */
int fexist(const char *s) {
  char catbuff[CATBUFF_SIZE];
  const int err = errno;
  struct stat st;

  memset(&st, 0, sizeof(st));
  if (stat(fconv(catbuff, sizeof(catbuff), s), &st) == 0) {
    if (S_ISREG(st.st_mode)) {
      return 1;
    } else {
      return 0;
    }
  }
  errno = err;
  return 0;
}

/* Le fichier existe-t-il? (ou est-il accessible?) */
/* Note: utf-8 */
/* Note: preserve errno */
int fexist_utf8(const char *s) {
  char catbuff[CATBUFF_SIZE];
  const int err = errno;
  STRUCT_STAT st;

  memset(&st, 0, sizeof(st));
  if (STAT(fconv(catbuff, sizeof(catbuff), s), &st) == 0) {
    if (S_ISREG(st.st_mode)) {
      return 1;
    } else {
      return 0;
    }
  }
  errno = err;
  return 0;
}

/* See htslib.h. */
hts_boolean hts_path_absent(const char *path) {
  char BIGSTK buf[HTS_URLMAXSIZE * 2];
  size_t len = strlen(path);
  STRUCT_STAT st;

  if (len == 0 || len >= sizeof(buf))
    return HTS_FALSE;
  /* MSVC's stat fails on a directory named with a trailing separator */
  while (len > 1 && (path[len - 1] == '/' || path[len - 1] == PATH_SEPARATOR))
    len--;
  memcpy(buf, path, len);
  buf[len] = '\0';
  return STAT(buf, &st) != 0 && errno == ENOENT;
}

/* File size, -1 if absent */
/* Note: utf-8 */
LLint fsize_utf8(const char *s) {
  STRUCT_STAT st;

  if (!strnotempty(s)) // empty name: error
    return -1;
  if (STAT(s, &st) == 0 && S_ISREG(st.st_mode)) {
    return st.st_size;
  } else {
    return -1;
  }
}

/* root dir, with ending / */
typedef struct {
  char path[1024 + 4];
  int init;
} hts_rootdir_strc;
HTSEXT_API const char *hts_rootdir(char *file) {
  static hts_rootdir_strc strc = { "", 0 };
  if (file) {
    if (!strc.init) {
      strc.path[0] = '\0';
      strc.init = 1;
      if (strnotempty(file)) {
        const size_t file_len = strlen(file);
        char *a;

        assertf(file_len < sizeof(strc.path));
        strcpybuff(strc.path, file);
        while((a = strrchr(strc.path, '\\')))
          *a = '/';
        if ((a = strrchr(strc.path, '/'))) {
          *(a + 1) = '\0';
        } else
          strc.path[0] = '\0';
      }
      if (!strnotempty(strc.path)) {
        if (getcwd(strc.path, sizeof(strc.path)) == NULL)
          strc.path[0] = '\0';
        else
          strcatbuff(strc.path, "/");
      }
    }
    return NULL;
  } else if (strc.init)
    return strc.path;
  else
    return "";
}

HTSEXT_API hts_stat_struct HTS_STAT;

/* Where the received-byte total lives (see htslib.h). Its lock guards this and
   nothing else, so a worker counting bytes waits on nothing else either. */
static LLint stat_recv_total = 0;
static htsmutex stat_recv_lock = HTSMUTEX_INIT;

void hts_stat_recv_add(LLint bytes) {
  hts_mutexlock(&stat_recv_lock);
  stat_recv_total += bytes;
  hts_mutexrelease(&stat_recv_lock);
}

LLint hts_stat_recv_get(void) {
  LLint bytes;

  hts_mutexlock(&stat_recv_lock);
  bytes = stat_recv_total;
  hts_mutexrelease(&stat_recv_lock);
  return bytes;
}

void hts_stat_recv_set(LLint bytes) {
  hts_mutexlock(&stat_recv_lock);
  stat_recv_total = bytes;
  hts_mutexrelease(&stat_recv_lock);
}

LLint hts_stat_recv_publish(void) {
  return HTS_STAT.HTS_TOTAL_RECV = hts_stat_recv_get();
}

//
// return  number of downloadable bytes, depending on rate limiter
// see engine_stats() routine, too
// this routine works quite well for big files and regular ones, but apparently the rate limiter has
// some problems with very small files (rate too high)
LLint check_downloadable_bytes(int rate) {
  if (rate > 0) {
    TStamp time_now;
    TStamp elapsed_useconds;
    LLint bytes_transferred_during_period;
    LLint left;

    // get the older timer
    int id_timer = (HTS_STAT.istat_idlasttimer + 1) % 2;

    time_now = mtime_monotonic();
    elapsed_useconds = time_now - HTS_STAT.istat_timestart[id_timer];
    bytes_transferred_during_period =
        (hts_stat_recv_get() - HTS_STAT.istat_bytes[id_timer]);

    left = ((rate * elapsed_useconds) / 1000) - bytes_transferred_during_period;
    if (left <= 0)
      left = 0;

    return left;
  } else
    return TAILLE_BUFFER;
}

// Lecture dans buff de size octets au maximum en utilisant la socket r (structure htsblk)
// returns: 
// >0 : data received
// == 0 : not yet data
// <0: error or no data: READ_ERROR, READ_EOF or READ_TIMEOUT
static int hts_read(htsblk *r, hts_buf buff) {
  const int size = hts_sat_int(buff.size);
  int retour;

  if (r->is_file) {
    if (r->fp) {
      retour = (int) fread(buff.p, 1, size, r->fp);
      if (retour == 0)  // can happen with directories (!)
        retour = READ_ERROR;
    } else
      retour = READ_ERROR;
  } else {
#if HTS_USEOPENSSL
    if (r->ssl) {
      /* It writes: the SSL_ERROR_WANT_WRITE arm below is that case. */
      sigpipe_mask m;

      sigpipe_hold(&m);
      retour = SSL_read(r->ssl_con, buff.p, size);
      sigpipe_release(&m);
      if (retour <= 0) {
        int err_code = SSL_get_error(r->ssl_con, retour);

        if ((err_code == SSL_ERROR_WANT_READ)
            || (err_code == SSL_ERROR_WANT_WRITE)
          ) {
          retour = 0;           /* no data yet (ssl cache) */
        } else if (err_code == SSL_ERROR_ZERO_RETURN) {
          retour = READ_EOF;    /* completed */
        } else {
          retour = READ_ERROR;  /* eof or error */
        }
      }
    } else {
#endif
      retour = (int) recv(r->soc, buff.p, size, 0);
      if (retour == 0) {
        retour = READ_EOF;
      } else if (retour < 0) {
        retour = READ_ERROR;
      }
    }
    if (retour > 0)             // compter flux entrant
      hts_stat_recv_add(retour);
#if HTS_USEOPENSSL
  }
#endif
  return retour;
}

// -- divers //

int hts_realloct_fault_selftest = 0;

static void hts_debug_log_print(HTS_FORMAT_STRING const char *format, ...)
    HTS_PRINTF_FUN(1, 2);

static int hts_dgb_init = 0;
static FILE *hts_dgb_init_fp = NULL;

/* Set the debug verbosity (0 is off); higher levels log more to stderr. Bit
   0x80 redirects debug output to "hts-debug.txt". */
static void hts_debug(int level) {
  hts_dgb_init = level;
  if (hts_dgb_init > 0) {
    hts_debug_log_print("hts_debug() called");
  }
}

static FILE *hts_dgb_(void) {
  if (hts_dgb_init_fp == NULL) {
    if ((hts_dgb_init & 0x80) == 0) {
      hts_dgb_init_fp = stderr;
    } else {
      hts_dgb_init_fp = FOPEN("hts-debug.txt", "wb");
      if (hts_dgb_init_fp != NULL) {
        fprintf(hts_dgb_init_fp, "* Creating file\r\n");
      }
    }
  }
  return hts_dgb_init_fp;
}

static void hts_debug_log_print(HTS_FORMAT_STRING const char *format, ...) {
  if (hts_dgb_init > 0) {
    const int error = errno;
    FILE *const fp = hts_dgb_();
    va_list args;

    assertf(format != NULL);
    va_start(args, format);
    (void) vfprintf(fp, format, args);
    va_end(args);
    fputs("\n", fp);
    fflush(fp);
    errno = error;
  }
}

HTSEXT_API const char* hts_version(void) {
  return HTTRACK_VERSIONID;
}

#if HTS_USEOPENSSL
static int ssl_vulnerable(const char *version) {
#ifdef _WIN32
  static const char *const match = "OpenSSL 1.0.1";
  const size_t match_len = strlen(match);
  if (version != NULL && strncmp(version, match, match_len) == 0) {
    // CVE-2014-0160
    // "OpenSSL 1.0.1g 7 Apr 2014"
    const char minor = version[match_len];
    return minor == ' ' || ( minor >= 'a' && minor <= 'f' );
  }
#endif
  return 0;
}
#endif

static void default_coucal_asserthandler(void *arg, const char* exp, const char* file, int line) {
  abortf_(exp, file, line);
}

static int get_loglevel_from_coucal(coucal_loglevel level) {
  switch(level) {
  case coucal_log_critical:
    return LOG_PANIC;
    break;
  case coucal_log_warning:
    return LOG_WARNING;
    break;
  case coucal_log_info:
    return LOG_INFO;
    break;
  case coucal_log_debug:
    return LOG_DEBUG;
    break;
  case coucal_log_trace:
    return LOG_TRACE;
    break;
  default:
    return LOG_ERROR;
    break;
  }
}

/* log to default console */
static HTS_PRINTF_FUN(3, 0) void default_coucal_loghandler(
    void *arg, coucal_loglevel level, const char *format, va_list args) {

  /* informational chatter (hashtable stats on delete, etc.) only when
     debugging; keep warnings and critical errors always visible. */
  if (level > coucal_log_warning && hts_dgb_init <= 0) {
    return;
  }
  if (level <= coucal_log_warning) {
    fprintf(stderr, "** warning: ");
  }
  vfprintf(stderr, format, args);
  fprintf(stderr, "\n");
}

/* log to project log */
static HTS_PRINTF_FUN(3, 0) void htsopt_coucal_loghandler(void *arg,
                                                          coucal_loglevel level,
                                                          const char *format,
                                                          va_list args) {
  httrackp *const opt = (httrackp*) arg;
  if (opt != NULL && opt->log != NULL) {
    hts_log_vprint(opt, get_loglevel_from_coucal(level), 
      format, args);
  } else {
    default_coucal_loghandler(NULL, level, format, args);
  }
}

/* attach hashtable logger to project log */
void hts_set_hash_handler(coucal hashtable, httrackp *opt) {
  /* Init hashtable default assertion handler. */
  coucal_set_assert_handler(hashtable,
    htsopt_coucal_loghandler, 
    default_coucal_asserthandler,
    opt);
}

static int hts_init_ok = 0;
HTSEXT_API int hts_init(void) {
  const char *dbg_env;

  /* */
  if (hts_init_ok)
    return 1;
  hts_init_ok = 1;

  /* enable debugging ? */
  dbg_env = getenv("HTS_LOG");
  if (dbg_env != NULL && *dbg_env != 0) {
    int level = 0;

    if (sscanf(dbg_env, "%d", &level) == 1) {
      hts_debug(level);
    }
  }

  hts_debug_log_print("entering hts_init()");   /* debug */

  /* Init hashtable default assertion handler. */
  coucal_set_global_assert_handler(default_coucal_loghandler, 
    default_coucal_asserthandler);

  /* Init threads (lazy init) */
  htsthread_init();

  /* Before any thread reads the verdict */
  hts_mptcp_probe();
  hts_psl_init();

  /* Ensure external modules are loaded */
  hts_debug_log_print("calling htspe_init()");  /* debug */
  htspe_init();                 /* module load (lazy) */

  /* MD5 Auto-test */
  {
    char digest[32 + 2];
    const char *atest = "MD5 Checksum Autotest";

    digest[0] = '\0';
    domd5mem(hts_span_of(atest), HTS_BUF(digest),
             HTS_TRUE); /* a42ec44369da07ace5ec1d660ba4a69a */
    if (strcmp(digest, "a42ec44369da07ace5ec1d660ba4a69a") != 0) {
      int fatal_broken_md5 = 0;

      assertf(fatal_broken_md5);
    }
  }

  hts_debug_log_print("initializing SSL");      /* debug */
#if HTS_USEOPENSSL
  /*
     Initialize the OpensSSL library
   */
  if (!openssl_ctx) {
    const char *version;
    const SSL_METHOD *method;

    /* OpenSSL >= 1.1.0 / LibreSSL >= 2.7.0 auto-init and provide the generic
       methods. The legacy init and SSLv23/SSLeay calls (deprecated since 1.1.0,
       likely gone in 4.0) are kept only for older OpenSSL. */
#if OPENSSL_VERSION_NUMBER < 0x10100000L \
  || (defined(LIBRESSL_VERSION_NUMBER) && LIBRESSL_VERSION_NUMBER < 0x2070000fL)
    SSL_load_error_strings();
    SSL_library_init();
    version = SSLeay_version(SSLEAY_VERSION);
    method = SSLv23_client_method();
#else
    version = OpenSSL_version(OPENSSL_VERSION);
    method = TLS_client_method();
#endif

    // Check CVE-2014-0160.
    if (ssl_vulnerable(version)) {
      fprintf(stderr, "OpenSSL version == '%s'\n", version);
      abortLog("unable to initialize TLS: OpenSSL version seems vulnerable to heartbleed bug (CVE-2014-0160)");
      assertf("OpenSSL version seems vulnerable to heartbleed bug (CVE-2014-0160)" == NULL);
    }

    openssl_ctx = SSL_CTX_new(method);
    if (!openssl_ctx) {
      fprintf(stderr, "fatal: unable to initialize TLS: SSL_CTX_new()\n");
      abortLog("unable to initialize TLS: SSL_CTX_new()");
      assertf("unable to initialize TLS" == NULL);
    }
    /* TLS floor (no SSLv3/TLS1.0/1.1); --check-cert verifies certs */
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
    SSL_CTX_set_min_proto_version(openssl_ctx, TLS1_2_VERSION);
#else
    SSL_CTX_set_options(openssl_ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 |
                                         SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1);
#endif
  }
#endif

  hts_debug_log_print("ending hts_init()");     /* debug */
  return 1;
}

HTSEXT_API int hts_uninit(void) {
  if (!hts_init_ok)
    return 1;
  htsthread_uninit();
  hts_psl_uninit();
  hts_init_ok = 0;
  return 1;
}

// legacy. do not use
HTSEXT_API hts_boolean hts_log(httrackp *opt, const char *prefix,
                               const char *msg) {
  if (opt->log != NULL) {
    fspc_count(opt, prefix);
    fspc(opt, opt->log, prefix);
    fprintf(opt->log, "%s" LF, msg);
    return 0;
  }
  return 1;                     /* Error */
}

static void (*hts_log_print_callback)(httrackp * opt, int type, const char *format, va_list args) = NULL;

HTSEXT_API void hts_set_log_vprint_callback(void (*callback)(httrackp * opt,
                                            int type, const char *format, va_list args)) {
  hts_log_print_callback = callback;
}

char hts_log_quiet_tag;

void hts_log_vprint(httrackp *opt, int type,
                    HTS_FORMAT_STRING const char *format, va_list args) {
  assertf(format != NULL);
  if (opt == HTS_LOG_QUIET)
    return;
  if (hts_log_print_callback != NULL) {
    va_list args_copy;
    va_copy(args_copy, args);
    /* the copy, so the vfprintf() below still has an unread list */
    hts_log_print_callback(opt, type, format, args_copy);
    va_end(args_copy);
  }
  if (opt != NULL) {
    const int save_errno = errno;
    const char *s_type = "unknown";
    const int level = type & 0xff;

    // Check log level
    if ((int) opt->debug < level) {
      return;
    }

    switch (level) {
    case LOG_TRACE:
      s_type = "trace";
      break;
    case LOG_DEBUG:
      s_type = "debug";
      break;
    case LOG_INFO:
    case LOG_NOTICE: /* not a warning: counted in the footer's messages */
      s_type = "info";
      break;
    case LOG_WARNING:
      s_type = "warning";
      break;
    case LOG_ERROR:
      s_type = "error";
      break;
    case LOG_PANIC:
      s_type = "panic";
      break;
    }
    /* Count the event, not the line, so errors are reported with no log
     * file (#1681). */
    fspc_count(opt, s_type);

    if (opt->log != NULL) {
      fspc(opt, opt->log, s_type);
      secret_vfprintf(opt, opt->log, format, args);
      if ((type & LOG_ERRNO) != 0) {
        char errbuf[HTS_STRERROR_SIZE];

        fprintf(opt->log, ": %s", hts_strerror(save_errno, HTS_BUF(errbuf)));
      }
      fputs(LF, opt->log);
      if (opt->flush) {
        fflush(opt->log);
      }
    }
    errno = save_errno;
  }
}

HTSEXT_API void hts_log_print(httrackp *opt, int type,
                              HTS_FORMAT_STRING const char *format, ...) {
  va_list args;
  assertf(format != NULL);
  va_start(args, format);
  hts_log_vprint(opt, type, format, args);
  va_end(args);
}

hts_boolean hts_libhandles_add(httrackp *opt, void *handle,
                               const char *moduleName) {
  char *name = strdupt(moduleName);
  htslibhandle *grown;

  if (name == NULL)
    return HTS_FALSE;
  grown = (htslibhandle *) realloct(opt->libHandles.handles,
                                    ((size_t) opt->libHandles.count + 1) *
                                        sizeof(htslibhandle));
  if (grown == NULL) {
    freet(name);
    return HTS_FALSE;
  }
  opt->libHandles.handles = grown;
  grown[opt->libHandles.count].handle = handle;
  grown[opt->libHandles.count].moduleName = name;
  opt->libHandles.count++;
  return HTS_TRUE;
}

int plug_wrapper(httrackp *opt, const char *moduleName, const char *argv) {
  void *handle = openFunctionLib(moduleName);

  if (handle != NULL) {
    t_hts_plug plug = (t_hts_plug) getFunctionPtr(handle, "hts_plug");
    t_hts_unplug unplug = (t_hts_unplug) getFunctionPtr(handle, "hts_unplug");

    if (plug != NULL) {
      int ret = plug(opt, argv);

      if (hts_dgb_init > 0 && opt->log != NULL) {
        hts_debug_log_print("plugged module '%s' (return code=%d)", moduleName,
                            ret);
      }
      if (ret == 1) {           /* Success! */
        if (hts_libhandles_add(opt, handle, moduleName))
          return 1;
        hts_debug_log_print("* note: out of memory registering %s", moduleName);
        if (unplug)
          unplug(opt);
      } else {
        hts_debug_log_print
          ("* note: error while running entry point 'hts_plug' in %s",
           moduleName);
        if (unplug)
          unplug(opt);
      }
    } else {
      const int last_errno = errno;
      char errbuf[HTS_STRERROR_SIZE];

      hts_debug_log_print("* note: can't find entry point 'hts_plug' in %s: %s",
                          moduleName,
                          hts_strerror(last_errno, HTS_BUF(errbuf)));
    }
    closeFunctionLib(handle);
    return 0;
  } else {
    const int last_errno = errno;
    char errbuf[HTS_STRERROR_SIZE];

    hts_debug_log_print("* note: can't load %s: %s", moduleName,
                        hts_strerror(last_errno, HTS_BUF(errbuf)));
  }
  return -1;
}

static void unplug_wrappers(httrackp * opt) {
  if (opt->libHandles.handles != NULL) {
    int i;

    for(i = 0; i < opt->libHandles.count; i++) {
      if (opt->libHandles.handles[i].handle != NULL) {
        /* hts_unplug(), the dll exit point (finalizer) */
        t_hts_unplug unplug =
          (t_hts_unplug) getFunctionPtr(opt->libHandles.handles[i].handle,
                                        "hts_unplug");
        if (unplug != NULL)
          unplug(opt);
        closeFunctionLib(opt->libHandles.handles[i].handle);
        opt->libHandles.handles[i].handle = NULL;
      }
      if (opt->libHandles.handles[i].moduleName != NULL) {
        freet(opt->libHandles.handles[i].moduleName);
        opt->libHandles.handles[i].moduleName = NULL;
      }
    }
    freet(opt->libHandles.handles);
    opt->libHandles.handles = NULL;
    opt->libHandles.count = 0;
  }
}

HTSEXT_API httrackp *hts_create_opt(void) {
  static const char *const defaultModules[] = {"httrack-plugin", NULL};
  httrackp *opt = malloct(sizeof(httrackp));

  /* default options */
  memset(opt, 0, sizeof(httrackp));
  opt->size_httrackp = sizeof(httrackp);

  /* mutexes */
  hts_mutexinit(&opt->state.lock);
  hts_secret_init(opt);

  /* custom wrappers */
  opt->libHandles.count = 0;

  /* default settings */

  opt->wizard = HTS_WIZARD_AUTO; // wizard automatique
  opt->quiet = HTS_FALSE;
  //
  opt->travel = HTS_TRAVEL_SAME_ADDRESS; // même adresse
  opt->depth = 9999;            // mirror total par défaut
  opt->extdepth = 0;            // mais pas à l'extérieur
  opt->seeker = HTS_SEEKER_DOWN;       // down
  opt->urlmode = HTS_URLMODE_RELATIVE; // relatif par défaut
  opt->no_type_change = HTS_FALSE;
  opt->debug = LOG_NOTICE;      // small log
  opt->getmode = HTS_GETMODE_HTML | HTS_GETMODE_NONHTML;
  opt->maxsite = -1;            // taille max site (aucune)
  opt->maxfile_nonhtml = -1;    // taille max fichier non html
  opt->maxfile_html = -1;       // idem pour html
  opt->maxsoc = 4;              // nbre socket max
  opt->fragment = -1;           // pas de fragmentation
  opt->nearlink = HTS_FALSE;
  opt->makeindex = HTS_TRUE;
  opt->kindex = HTS_FALSE;
  opt->delete_old = HTS_TRUE;
  opt->background_on_suspend = HTS_TRUE;
  opt->makestat = HTS_FALSE;
  opt->maketrack = HTS_FALSE;
  opt->timeout = 120;           // timeout par défaut (2 minutes)
  opt->cache = HTS_CACHE_PRIORITY; // cache prioritaire
  opt->shell = HTS_FALSE;
  opt->proxy.active = 0;        // pas de proxy
  opt->user_agent_send = HTS_TRUE;
  StringCopy(opt->user_agent, HTS_DEFAULT_USER_AGENT);
  StringCopy(opt->referer, "");
  StringCopy(opt->from, "");
  opt->savename_83 = HTS_SAVENAME_83_LONG; // long names by default
  opt->savename_type = 0;       // avec structure originale
  opt->savename_delayed =
      HTS_SAVENAME_DELAYED_HARD; // always delay the type check (default)
  opt->delayed_cached = HTS_TRUE;
  opt->mimehtml = HTS_FALSE;
  opt->parsejava = HTSPARSE_DEFAULT;    // parser classes
  opt->hostcontrol = 0;         // PAS de control host pour timeout et traffic jammer
  opt->retry = 2;               // 2 retry par défaut
  opt->errpage = HTS_TRUE;
  // d'erreur (404 etc.)
  opt->check_type = HTS_TRUE;
  // considéré comme html
  opt->all_in_cache = HTS_FALSE;
  opt->robots = HTS_ROBOTS_ALWAYS; // traiter les robots.txt
  opt->external = HTS_FALSE;
  opt->passprivacy = HTS_FALSE;
  opt->includequery = HTS_TRUE;
  opt->mirror_first_page = HTS_FALSE;
  opt->accept_cookie = HTS_TRUE;
  opt->cookie = NULL;
  opt->http10 = HTS_FALSE;
  opt->nokeepalive = HTS_FALSE;
  opt->nocompression = HTS_FALSE;
  opt->tolerant = HTS_FALSE;
  opt->parseall = HTS_TRUE;
  opt->parsedebug = HTS_FALSE;
  opt->norecatch = HTS_FALSE;
  opt->verbosedisplay = HTS_VERBOSE_NONE; // no text animation
  opt->sizehack = HTS_FALSE;
  opt->urlhack = HTS_TRUE;
  opt->no_www_dedup = HTS_FALSE;
  opt->no_slash_dedup = HTS_FALSE;
  opt->no_query_dedup = HTS_FALSE;
  StringCopy(opt->footer, HTS_DEFAULT_FOOTER);
  StringCopy(opt->strip_query, "");
  StringCopy(opt->host_alias, "");
  StringCopy(opt->cookies_file, "");
  StringCopy(opt->warc_file, "");
  StringCopy(opt->sitemap_url, "");
  opt->warc_max_size = 0; /* no rotation unless --warc-max-size sets it */
  opt->changes = HTS_FALSE;
  opt->changes_state = NULL;
  opt->single_file = HTS_FALSE;
  opt->single_file_max_size = SINGLEFILE_DEFAULT_MAX_SIZE;
  opt->singlefile_state = NULL;
  opt->links_unqueued = HTS_FALSE;
  opt->mirror_completed = HTS_DEFAULT;
  opt->transport_failures = 0;
  opt->mptcp = HTS_DEFAULT;
  opt->browser_headers = HTS_DEFAULT;
  opt->check_cert = HTS_DEFAULT;
  opt->mptcp_connections = 0;
  opt->mptcp_fallbacks = 0;
  opt->upper_links_refused = HTS_FALSE;
  opt->abort_left_partial = HTS_FALSE;
  StringCopy(opt->why_url, "");
  opt->pause_min_ms = 0;
  opt->pause_max_ms = 0;
  opt->max_retry_after = HTS_DEFAULT_MAX_RETRY_AFTER;
  opt->ftp_proxy = HTS_TRUE;
  opt->convert_utf8 = HTS_TRUE;
  StringCopy(opt->filelist, "");
  StringCopy(opt->lang_iso, HTS_ACCEPT_LANGUAGE_LEGACY);
  StringCopy(opt->accept, HTS_ACCEPT_LEGACY);
  StringCopy(opt->accept_kinds, "");
  opt->accept_given = HTS_FALSE;
  opt->lang_given = HTS_FALSE;
  StringCopy(opt->bundle_dir, "");
  opt->bundle_state = NULL;
  StringCopy(opt->headers, "");
  StringCopy(opt->mimedefs, "\n");      // aucun filtre mime (\n IMPORTANT)
  StringClear(opt->mod_blacklist);
  //
  opt->log = stdout;
  opt->errlog = stderr;
  opt->flush = HTS_TRUE;
  opt->keyboard = HTS_FALSE;
  //
  StringCopy(opt->path_html, "");
  StringCopy(opt->path_html_utf8, "");
  StringCopy(opt->path_log, "");
  StringCopy(opt->path_bin, "");
  //
  opt->maxlink = 100000;        // 100,000 liens max par défaut
  opt->maxfilter = 200;         // 200 filtres max par défaut
  opt->maxcache = 1048576 * 32; // a peu près 32Mo en cache max -- OPTION NON
                                // PARAMETRABLE POUR L'INSTANT --
  opt->maxtime = -1;            // temps max en secondes
  opt->maxrate = 100000;        // taux maxi
  opt->maxconn = 5.0;           // nombre connexions/s
  opt->waittime = -1;           // wait until.. hh*3600+mm*60+ss
  //
  opt->exec = "";
  opt->is_update = HTS_FALSE;
  opt->dir_topindex = HTS_FALSE;
  //
  opt->bypass_limits = HTS_FALSE;
  hts_store_release_int(&opt->state.stop, 0);
  hts_store_release_int(&opt->state.exit_xh, 0);
  //
  opt->state.is_ended = 0;

  /* Alocated buffers */

  opt->callbacks_fun =
    (t_hts_htmlcheck_callbacks *) malloct(sizeof(t_hts_htmlcheck_callbacks));
  /* A plug-in that chains to an empty slot gets the engine default as prev. */
  *opt->callbacks_fun = default_callbacks;

  /* Preload callbacks : java and flash parser, and the automatic user-defined callback */

  {
    int i;

    for(i = 0; defaultModules[i] != NULL; i++) {
      int ret = plug_wrapper(opt, defaultModules[i], defaultModules[i]);

      if (ret == 0) {           /* Module aborted initialization */
        /* Ignored. */
      }
    }
  }

  return opt;
}

HTSEXT_API size_t hts_sizeof_opt(void) {
  return sizeof(httrackp);
}

/* The runtime assert below cannot see a same-sized non-callback member. */
HTS_STATIC_ASSERT(offsetof(t_hts_htmlcheck_callbacks, extsavename) +
                          sizeof(t_hts_htmlcheck_callbacks_item) ==
                      sizeof(t_hts_htmlcheck_callbacks),
                  callbacks_are_a_flat_slot_array);

HTSEXT_API void hts_free_opt(httrackp * opt) {
  if (opt != NULL) {
    /* An FTP worker reads opt for its whole run, and not every caller drains
       its threads the way httrack.c does before getting here. */
    ftp_stop_workers();
    hts_bundle_free(opt);

    /* Alocated callbacks */

    if (opt->callbacks_fun != NULL) {
      int i;
      t_hts_htmlcheck_callbacks_item *items =
        (t_hts_htmlcheck_callbacks_item *) opt->callbacks_fun;
      const int size =
        (int) sizeof(t_hts_htmlcheck_callbacks) /
        sizeof(t_hts_htmlcheck_callbacks_item);
      assertf(sizeof(t_hts_htmlcheck_callbacks_item) * size ==
              sizeof(t_hts_htmlcheck_callbacks));

      /* Free all linked lists */
      for(i = 0; i < size; i++) {
        t_hts_callbackarg *carg, *next_carg;

        for(carg = items[i].carg;
            carg != NULL && (next_carg = carg->prev.carg, carg != NULL);
            carg = next_carg) {
          hts_free(carg);
        }
      }

      freet(opt->callbacks_fun);
      opt->callbacks_fun = NULL;
    }

    /* Close library handles */
    unplug_wrappers(opt);

    /* Cache */
    if (opt->state.dns_cache != NULL) {
      coucal root;

      hts_mutexlock(&opt->state.lock);
      root = opt->state.dns_cache;
      opt->state.dns_cache = NULL;
      hts_mutexrelease(&opt->state.lock);

      coucal_delete(&root); // frees records via hts_cache_value_free
    }

    /* URLs a front end queued but the engine never got to inject */
    hts_addurl_free(hts_addurl_take(opt));
    hts_addurl_free(hts_addfilter_take(opt));
    hts_addurl_free(hts_setfilters_take(opt));

    /* Cancel chain */
    if (opt->state.cancel != NULL) {
      htsoptstatecancel *cancel;

      for(cancel = opt->state.cancel; cancel != NULL;) {
        htsoptstatecancel *next = cancel->next;

        if (cancel->url != NULL) {
          freet(cancel->url);
        }
        freet(cancel);
        cancel = next;
      }
      opt->state.cancel = NULL;
    }

    /* Free strings */

    StringFree(opt->proxy.name);
    StringFree(opt->proxy.bindhost);

    StringFree(opt->savename_userdef);
    StringFree(opt->user_agent);
    StringFree(opt->referer);
    StringFree(opt->from);
    StringFree(opt->lang_iso);
    StringFree(opt->accept);
    StringFree(opt->accept_kinds);
    StringFree(opt->bundle_dir);
    StringFree(opt->headers);
    StringFree(opt->sys_com);
    StringFree(opt->mimedefs);
    StringFree(opt->filelist);
    StringFree(opt->urllist);
    StringFree(opt->footer);
    StringFree(opt->mod_blacklist);
    StringFree(opt->strip_query);
    StringFree(opt->host_alias);
    StringFree(opt->cookies_file);
    StringFree(opt->why_url);
    StringFree(opt->warc_file);
    StringFree(opt->sitemap_url);
    StringFree(opt->state.mimemid);
    hts_sitemap_free(opt); /* backstop: httpmirror's early-return paths */
    url_savename_refnames_free(opt);
    singlefile_free(opt);

    hts_changes_free_opt(opt);

    StringFree(opt->path_html);
    StringFree(opt->path_html_utf8);
    StringFree(opt->path_log);
    StringFree(opt->path_bin);

    /* mutexes */
    hts_secret_free(opt);
    hts_mutexfree(&opt->state.lock);

    /* Free structure */
    freet(opt);
  }
}

// TEMPORARY - PUT THIS STRUCTURE INSIDE httrackp !
const hts_stat_struct* hts_get_stats(httrackp * opt) {
  if (opt == NULL) {
    return NULL;
  }

  hts_stat_recv_publish();
  HTS_STAT.stat_nsocket = 0;
  HTS_STAT.stat_errors = fspc(opt, NULL, "error");
  HTS_STAT.stat_warnings = fspc(opt, NULL, "warning");
  HTS_STAT.stat_infos = fspc(opt, NULL, "info");
  HTS_STAT.stat_transport_failures = opt->transport_failures;
  HTS_STAT.stat_mptcp_connections = opt->mptcp_connections;
  HTS_STAT.stat_mptcp_fallbacks = opt->mptcp_fallbacks;
  HTS_STAT.nbk = 0;
  HTS_STAT.nb = 0;

  return &HTS_STAT;
}

// defaut wrappers
static void __cdecl htsdefault_init(t_hts_callbackarg *carg) {}

static void __cdecl htsdefault_uninit(t_hts_callbackarg *carg) {}
static int __cdecl htsdefault_start(t_hts_callbackarg * carg, httrackp * opt) {
  return 1;
}
static int __cdecl htsdefault_chopt(t_hts_callbackarg * carg, httrackp * opt) {
  return 1;
}
static int __cdecl htsdefault_end(t_hts_callbackarg * carg, httrackp * opt) {
  return 1;
}
static int __cdecl htsdefault_preprocesshtml(t_hts_callbackarg * carg,
                                             httrackp * opt, char **html,
                                             int *len, const char *url_adresse,
                                             const char *url_fichier) {
  return 1;
}
static int __cdecl htsdefault_postprocesshtml(t_hts_callbackarg * carg,
                                              httrackp * opt, char **html,
                                              int *len, const char *url_adresse,
                                              const char *url_fichier) {
  return 1;
}
static int __cdecl htsdefault_checkhtml(t_hts_callbackarg * carg,
                                        httrackp * opt, char *html, int len,
                                        const char *url_adresse,
                                        const char *url_fichier) {
  return 1;
}
static int __cdecl htsdefault_loop(t_hts_callbackarg * carg, httrackp * opt, lien_back * back, int back_max, int back_index, int lien_n, int lien_tot, int stat_time, hts_stat_struct * stats) {        // appelé à chaque boucle de HTTrack
  return 1;
}
static const char *__cdecl htsdefault_query(t_hts_callbackarg * carg,
                                            httrackp * opt,
                                            const char *question) {
  return "";
}
static const char *__cdecl htsdefault_query2(t_hts_callbackarg * carg,
                                             httrackp * opt,
                                             const char *question) {
  return "";
}
static const char *__cdecl htsdefault_query3(t_hts_callbackarg * carg,
                                             httrackp * opt,
                                             const char *question) {
  return "";
}
static int __cdecl htsdefault_check(t_hts_callbackarg * carg, httrackp * opt,
                                    const char *adr, const char *fil,
                                    int status) {
  return -1;
}
static int __cdecl htsdefault_check_mime(t_hts_callbackarg * carg,
                                         httrackp * opt, const char *adr,
                                         const char *fil, const char *mime,
                                         int status) {
  return -1;
}
static void __cdecl htsdefault_pause(t_hts_callbackarg * carg, httrackp * opt,
                                     const char *lockfile) {
  String abortlock = STRING_EMPTY, progress = STRING_EMPTY;

  /* Strings, because lockfile already holds a rotating OPT_GET_BUFF. */
  StringCopy(abortlock, StringBuff(opt->path_log));
  StringCat(abortlock, HTS_ABORT_LOCKNAME);
  StringCopy(progress, StringBuff(opt->path_log));
  StringCat(progress, "hts-in_progress.lock");
  /* A stop, an exit or an abort request aimed at this run ends the wait. */
  while (fexist_utf8(lockfile) && !hts_run_is_ending(opt) &&
         !hts_file_is_newer(StringBuff(abortlock), StringBuff(progress))) {
    Sleep(1000);
  }
  StringFree(progress);
  StringFree(abortlock);
}
static void __cdecl htsdefault_filesave(t_hts_callbackarg * carg,
                                        httrackp * opt, const char *file) {
}
static void __cdecl htsdefault_filesave2(t_hts_callbackarg * carg,
                                         httrackp * opt, const char *adr,
                                         const char *file, const char *sav,
                                         int is_new, int is_modified,
                                         int not_updated) {
}
static int __cdecl htsdefault_linkdetected(t_hts_callbackarg * carg,
                                           httrackp * opt, char *link) {
  return 1;
}
static int __cdecl htsdefault_linkdetected2(t_hts_callbackarg * carg,
                                            httrackp * opt, char *link,
                                            const char *start_tag) {
  return 1;
}
static int __cdecl htsdefault_xfrstatus(t_hts_callbackarg * carg,
                                        httrackp * opt, lien_back * back) {
  return 1;
}
static int __cdecl htsdefault_savename(t_hts_callbackarg * carg, httrackp * opt,
                                       const char *adr_complete,
                                       const char *fil_complete,
                                       const char *referer_adr,
                                       const char *referer_fil, char *save) {
  return 1;
}
static int __cdecl htsdefault_sendhead(t_hts_callbackarg * carg, httrackp * opt,
                                       char *buff, const char *adr,
                                       const char *fil, const char *referer_adr,
                                       const char *referer_fil,
                                       htsblk * outgoing) {
  return 1;
}
static int __cdecl htsdefault_receivehead(t_hts_callbackarg * carg,
                                          httrackp * opt, char *buff,
                                          const char *adr, const char *fil,
                                          const char *referer_adr,
                                          const char *referer_fil,
                                          htsblk * incoming) {
  return 1;
}
static int __cdecl htsdefault_detect(t_hts_callbackarg * carg, httrackp * opt,
                                     htsmoduleStruct * str) {
  return 0;
}
static int __cdecl htsdefault_parse(t_hts_callbackarg * carg, httrackp * opt,
                                    htsmoduleStruct * str) {
  return 0;
}

/* Default internal dummy callbacks */
const t_hts_htmlcheck_callbacks default_callbacks = {
  {htsdefault_init, NULL},
  {htsdefault_uninit, NULL},
  {htsdefault_start, NULL},
  {htsdefault_end, NULL},
  {htsdefault_chopt, NULL},
  {htsdefault_preprocesshtml, NULL},
  {htsdefault_postprocesshtml, NULL},
  {htsdefault_checkhtml, NULL},
  {htsdefault_query, NULL},
  {htsdefault_query2, NULL},
  {htsdefault_query3, NULL},
  {htsdefault_loop, NULL},
  {htsdefault_check, NULL},
  {htsdefault_check_mime, NULL},
  {htsdefault_pause, NULL},
  {htsdefault_filesave, NULL},
  {htsdefault_filesave2, NULL},
  {htsdefault_linkdetected, NULL},
  {htsdefault_linkdetected2, NULL},
  {htsdefault_xfrstatus, NULL},
  {htsdefault_savename, NULL},
  {htsdefault_sendhead, NULL},
  {htsdefault_receivehead, NULL},
  {htsdefault_detect, NULL},
  {htsdefault_parse, NULL}
};

#define CALLBACK_OP(CB, NAME, OPERATION, S, FUN) do {   \
  if (strcmp(NAME, S) == 0) {                           \
    OPERATION(t_hts_htmlcheck_ ##FUN, (CB)->FUN.fun);   \
  }                                                     \
} while(0)

#define DISPATCH_CALLBACK(CB, NAME, OPERATION) do { \
  CALLBACK_OP(CB, NAME, OPERATION, "init", init); \
  CALLBACK_OP(CB, NAME, OPERATION, "free", uninit); \
  CALLBACK_OP(CB, NAME, OPERATION, "start", start); \
  CALLBACK_OP(CB, NAME, OPERATION, "end", end); \
  CALLBACK_OP(CB, NAME, OPERATION, "change-options", chopt); \
  CALLBACK_OP(CB, NAME, OPERATION, "preprocess-html", preprocess); \
  CALLBACK_OP(CB, NAME, OPERATION, "postprocess-html", postprocess); \
  CALLBACK_OP(CB, NAME, OPERATION, "check-html", check_html); \
  CALLBACK_OP(CB, NAME, OPERATION, "query", query); \
  CALLBACK_OP(CB, NAME, OPERATION, "query2", query2); \
  CALLBACK_OP(CB, NAME, OPERATION, "query3", query3); \
  CALLBACK_OP(CB, NAME, OPERATION, "loop", loop); \
  CALLBACK_OP(CB, NAME, OPERATION, "check-link", check_link); \
  CALLBACK_OP(CB, NAME, OPERATION, "check-mime", check_mime); \
  CALLBACK_OP(CB, NAME, OPERATION, "pause", pause); \
  CALLBACK_OP(CB, NAME, OPERATION, "save-file", filesave); \
  CALLBACK_OP(CB, NAME, OPERATION, "save-file2", filesave2); \
  CALLBACK_OP(CB, NAME, OPERATION, "link-detected", linkdetected); \
  CALLBACK_OP(CB, NAME, OPERATION, "link-detected2", linkdetected2); \
  CALLBACK_OP(CB, NAME, OPERATION, "transfer-status", xfrstatus); \
  CALLBACK_OP(CB, NAME, OPERATION, "save-name", savename); \
  CALLBACK_OP(CB, NAME, OPERATION, "send-header", sendhead); \
  CALLBACK_OP(CB, NAME, OPERATION, "receive-header", receivehead); \
} while(0)

static void *callback_slot(const t_hts_htmlcheck_callbacks *callbacks,
                           const char *name);

int hts_set_callback(t_hts_htmlcheck_callbacks * callbacks, const char *name,
                     void *function) {
  int error = 1;

  /* NULL restores the engine default, so a later chain never gets a NULL prev.
   */
  if (function == NULL)
    function = callback_slot(&default_callbacks, name);
#define CALLBACK_OPERATION(TYPE, FUNCTION) do { \
    FUNCTION = (TYPE) function;                 \
    error = 0;                                  \
  } while(0)
  DISPATCH_CALLBACK(callbacks, name, CALLBACK_OPERATION);
#undef CALLBACK_OPERATION
  return error;
}

static void *callback_slot(const t_hts_htmlcheck_callbacks *callbacks,
                           const char *name) {
#define CALLBACK_OPERATION(TYPE, FUNCTION) do { \
    return (void*) FUNCTION;                    \
  } while(0)
  DISPATCH_CALLBACK(callbacks, name, CALLBACK_OPERATION);
#undef CALLBACK_OPERATION
  return NULL;
}

void *hts_get_callback(t_hts_htmlcheck_callbacks *callbacks, const char *name) {
  void *const fun = callback_slot(callbacks, name);

  /* An engine default reads as no callback. */
  return fun != callback_slot(&default_callbacks, name) ? fun : NULL;
}

// end defaut wrappers

/* libc stubs */

HTSEXT_API char *hts_strdup(const char *str) {
  return strdup(str); /* libc on purpose */
}

HTSEXT_API void *hts_malloc(size_t size) {
  return malloc(size); /* libc on purpose */
}

HTSEXT_API void *hts_realloc(void *const data, const size_t size) {
  return realloc(data, size); /* libc on purpose */
}

HTSEXT_API void hts_free(void *data) { free(data); /* libc on purpose */ }

/* Dummy functions */
HTSEXT_API int hts_resetvar(void) {
  return 0;
}

#ifdef _WIN32

typedef struct dirent dirent;

DIR *opendir(const char *name) {
  WIN32_FILE_ATTRIBUTE_DATA st;
  DIR *dir;
  size_t len;
  int i;
  LPWSTR wname;

  if (name == NULL || *name == '\0') {
    errno = ENOENT;
    return NULL;
  }
  // Wide \\?\ path: no MAX_PATH cap, no CP_ACP mis-decode (#133,#630).
  wname = hts_pathToUCS2(name);
  if (wname == NULL ||
      !GetFileAttributesExW(wname, GetFileExInfoStandard, &st) ||
      (st.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
    freet(wname);
    errno = ENOENT;
    return NULL;
  }
  freet(wname);
  dir = calloct(1, sizeof(DIR));
  if (dir == NULL) {
    errno = ENOMEM;
    return NULL;
  }
  len = strlen(name);
  dir->h = INVALID_HANDLE_VALUE;
  dir->name = malloct(len + 2 + 1);
  strcpy(dir->name, name);
  for(i = 0; dir->name[i] != '\0'; i++) {
    if (dir->name[i] == '/') {
      dir->name[i] = '\\';
    }
  }
  strcat(dir->name, "\\*");
  return dir;
}

struct dirent *readdir(DIR * dir) {
  WIN32_FIND_DATAW find;

  if (dir->h == INVALID_HANDLE_VALUE) {
    // \\?\-prefix so a long/non-ASCII directory enumerates instead of ENOENT.
    LPWSTR wname = hts_pathToUCS2(dir->name);

    dir->h =
        wname != NULL ? FindFirstFileW(wname, &find) : INVALID_HANDLE_VALUE;
    freet(wname);
  } else {
    if (!FindNextFileW(dir->h, &find)) {
      FindClose(dir->h);
      dir->h = INVALID_HANDLE_VALUE;
    }
  }
  if (dir->h != INVALID_HANDLE_VALUE) {
    char *u = hts_convertUCS2StringToUTF8(find.cFileName, -1);

    dir->entry.d_name[0] = 0;
    if (u != NULL) {
      strncat(dir->entry.d_name, u, HTS_DIRENT_SIZE - 1);
      freet(u);
    }
    return &dir->entry;
  }
  errno = ENOENT;
  return NULL;
}

int closedir(DIR * dir) {
  if (dir != NULL) {
    if (dir->h != INVALID_HANDLE_VALUE) {
      CloseHandle(dir->h);
    }
    freet(dir->name);
    freet(dir);
    return 0;
  }
  errno = EBADF;
  return -1;
}

// UTF-8 aware FILE API

static void copyWchar(LPWSTR dest, const char *src) {
  int i;

  for(i = 0; src[i]; i++) {
    dest[i] = src[i];
  }
  dest[i] = '\0';
}

/* UTF-8 path -> UCS-2 for the _w* file APIs. At/above HTS_WIN_LONGPATH_MIN,
   \\?\-prefix it via GetFullPathNameW to clear MAX_PATH (#133); else unchanged.
   Any prefixing failure falls back to the plain converted path. */
#define HTS_WIN_LONGPATH_MIN 240 /* stay clear of MAX_PATH (260) */

LPWSTR hts_pathToUCS2(const char *path) {
  LPWSTR wpath = hts_convertUTF8StringToUCS2(hts_span_of(path), NULL);

  if (wpath == NULL) {
    return NULL;
  }
  const size_t len = wcslen(wpath);
  // Already "\\?\" or "\\.\": don't re-prefix.
  const int verbatim = len >= 4 && wpath[0] == L'\\' && wpath[1] == L'\\' &&
                       (wpath[2] == L'?' || wpath[2] == L'.') &&
                       wpath[3] == L'\\';

  if (len < HTS_WIN_LONGPATH_MIN || verbatim) {
    return wpath;
  }

  const DWORD need = GetFullPathNameW(wpath, 0, NULL, NULL); /* incl NUL */
  LPWSTR full = need != 0 ? malloct((size_t) need * sizeof(WCHAR)) : NULL;

  if (full == NULL) {
    return wpath; /* fall back to the plain path */
  }
  const DWORD written = GetFullPathNameW(wpath, need, full, NULL);

  if (written == 0 || written >= need || full[0] == L'\0') {
    freet(full);
    return wpath;
  }

  const int isUNC = full[0] == L'\\' && full[1] == L'\\';
  // UNC "\\srv\share" -> "\\?\UNC\srv\share": the prefix subsumes the "\\".
  const WCHAR *const pfx = isUNC ? L"\\\\?\\UNC\\" : L"\\\\?\\";
  const WCHAR *const body = isUNC ? full + 2 : full;
  const size_t pfxLen = wcslen(pfx), bodyLen = wcslen(body);
  LPWSTR out = malloct((pfxLen + bodyLen + 1) * sizeof(WCHAR));

  if (out == NULL) {
    freet(full);
    return wpath;
  }
  memcpybuff(out, pfx, pfxLen * sizeof(WCHAR));
  memcpybuff(out + pfxLen, body, (bodyLen + 1) * sizeof(WCHAR));
  freet(full);
  freet(wpath);
  return out;
}

FILE *hts_fopen_utf8(const char *path, const char *mode) {
  WCHAR wmode[32];
  LPWSTR wpath = hts_pathToUCS2(path);

  assertf(strlen(mode) < sizeof(wmode) / sizeof(WCHAR));
  copyWchar(wmode, mode);
  if (wpath != NULL) {
    FILE *const fp = _wfopen(wpath, wmode);

    freet(wpath);
    return fp;
  } else {
    // Fallback on conversion error.
    return fopen(path, mode);
  }
}

int hts_stat_utf8(const char *path, STRUCT_STAT * buf) {
  LPWSTR wpath = hts_pathToUCS2(path);

  if (wpath != NULL) {
    const int result = _wstat64(wpath, buf);

    freet(wpath);
    return result;
  } else {
    // Fallback on conversion error.
    return _stat64(path, buf);
  }
}

int hts_unlink_utf8(const char *path) {
  LPWSTR wpath = hts_pathToUCS2(path);

  if (wpath != NULL) {
    const int result = _wunlink(wpath);

    freet(wpath);
    return result;
  } else {
    // Fallback on conversion error.
    return _unlink(path);
  }
}

int hts_rename_utf8(const char *oldpath, const char *newpath) {
  LPWSTR woldpath = hts_pathToUCS2(oldpath);
  LPWSTR wnewpath = hts_pathToUCS2(newpath);
  if (woldpath != NULL && wnewpath != NULL) {
    const int result = _wrename(woldpath, wnewpath);
    /* Save errno: callers key off it (#779) and freeing may clobber it. */
    const int err = errno;

    freet(woldpath);
    freet(wnewpath);
    errno = err;
    return result;
  } else {
    freet(woldpath);
    freet(wnewpath);
    // Fallback on conversion error.
    return rename(oldpath, newpath);
  }
}

int hts_rmdir_utf8(const char *path) {
  LPWSTR wpath = hts_pathToUCS2(path);

  if (wpath != NULL) {
    const int result = _wrmdir(wpath);

    freet(wpath);
    return result;
  } else {
    // Fallback on conversion error.
    return _rmdir(path);
  }
}

int hts_mkdir_utf8(const char *path) {
  LPWSTR wpath = hts_pathToUCS2(path);

  if (wpath != NULL) {
    const int result = _wmkdir(wpath);

    freet(wpath);
    return result;
  } else {
    // Fallback on conversion error.
    return _mkdir(path);
  }
}

HTSEXT_API int hts_utime_utf8(const char *path, const STRUCT_UTIMBUF * times) {
  STRUCT_UTIMBUF mtimes = *times;
  LPWSTR wpath = hts_pathToUCS2(path);

  if (wpath != NULL) {
    const int result = _wutime(wpath, &mtimes);

    freet(wpath);
    return result;
  } else {
    // Fallback on conversion error.
    return _utime(path, &mtimes);
  }
}

#endif

// Fin
