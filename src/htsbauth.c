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
/* File: httrack.c subroutines:                                 */
/*       basic authentication: password storage                 */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Internal engine bytecode */
#define HTS_INTERNAL_BYTECODE

#include "htsbauth.h"

#include <limits.h>
#include <stdarg.h>
#include <stdint.h>

/* specific definitions */
#include "htsglobal.h"
#include "htslib.h"
#include "htslines.h"
#include "htscore.h"
#include "htspsl.h"
#include "htsbundle.h"
#include "htsdate.h"
#include "htsio.h"
#include "htscookie_selftest.h"
#include "htscharset.h"
#ifdef _WIN32
#include <io.h> /* _commit */
#endif

/* END specific definitions */

/* A cookies.txt line marks HttpOnly by this prefix. */
#define COOKIE_HTTPONLY_PREFIX "#HttpOnly_"
/* A jar record marks it in field 7. The fields are 0 domain, 1 subdomains,
   2 path, 3 secure, 4 expiry, 5 name, 6 value and 7 HttpOnly. */
#define COOKIE_HTTPONLY_FIELD "HttpOnly"

/* Is uc a capital letter of the Latin, Greek, Cyrillic or Armenian script? */
static hts_boolean cookie_is_capital(hts_UCS4 uc) {
  if (uc >= 0x100 && uc <= 0x17f) // Latin Extended-A pairs capital first
    return uc == 0x178 ||
           ((uc <= 0x137 || (uc >= 0x14a && uc <= 0x177)) && uc % 2 == 0) ||
           (((uc >= 0x139 && uc <= 0x148) || (uc >= 0x179 && uc <= 0x17e)) &&
            uc % 2 == 1);
  if ((uc >= 0x460 && uc <= 0x481) || (uc >= 0x48a && uc <= 0x4bf) ||
      (uc >= 0x4d0 && uc <= 0x52f) || (uc >= 0x1e00 && uc <= 0x1e95) ||
      (uc >= 0x1ea0 && uc <= 0x1eff))
    return uc % 2 == 0;
  if (uc >= 0x4c1 && uc <= 0x4ce)
    return uc % 2 == 1;
  if (uc >= 0x3d8 && uc <= 0x3ef) // Greek archaic letters and Coptic
    return uc % 2 == 0;
  return (uc >= 0xc0 && uc <= 0xde && uc != 0xd7) || uc == 0x370 ||
         uc == 0x372 || uc == 0x376 || uc == 0x37f || uc == 0x386 ||
         (uc >= 0x388 && uc <= 0x38a) || uc == 0x38c || uc == 0x38e ||
         uc == 0x38f || (uc >= 0x391 && uc <= 0x3ab && uc != 0x3a2) ||
         uc == 0x3cf || (uc >= 0x3d2 && uc <= 0x3d4) || uc == 0x3f4 ||
         uc == 0x3f7 || uc == 0x3f9 || uc == 0x3fa ||
         (uc >= 0x3fd && uc <= 0x3ff) || (uc >= 0x400 && uc <= 0x42f) ||
         uc == 0x4c0 || (uc >= 0x531 && uc <= 0x556) || uc == 0x1e9e ||
         (uc >= 0xff21 && uc <= 0xff3a);
}

/* See htsbundle.h. */
const char *cookie_host_refusal(const char *host, size_t len) {
  size_t i, n;
  hts_UCS4 uc;

  if (!hts_isStringUTF8(host, len))
    return "the host is not valid UTF-8";
  for (i = 0; i < len; i += n) {
    n = hts_readUTF8(&host[i], len - i, &uc);
    if (n == 0) // a stalled reader would loop forever
      return "the host is not valid UTF-8";
    if (cookie_is_capital(uc))
      return "write the host in lowercase";
  }
  return NULL;
}

/* See htsbauth.h. */
hts_boolean cookie_host(const char *adr, char *dst, size_t dst_size) {
  const char *host = jump_identification_const(adr);
  hts_boolean literal = HTS_FALSE;
  size_t len;

  if (host[0] == '[') { // bracketed IPv6 literal, [::1]:8080
    const char *const end = strchr(host, ']');

    literal = HTS_TRUE;
    host++;
    len = end != NULL ? (size_t) (end - host) : strlen(host);
  } else {
    const char *port = strchr(host, ':');

    // several colons means a bare IPv6 literal, which has no port to cut
    if (port != NULL && strchr(port + 1, ':') != NULL)
      port = NULL;
    len = port != NULL ? (size_t) (port - host) : strlen(host);
  }
  // an empty domain is a suffix of every host, so it would match all of them
  if (len == 0 || len >= dst_size)
    return HTS_FALSE;
  dst[0] = '\0';
  strlncatbuff(dst, host, dst_size, len);
  // host names are case-insensitive, so one folded spelling reaches one entry
  hts_lowcase(dst);
  // the request names an international host in its ASCII form, so a jar must
  if (!hts_isStringAscii(dst, strlen(dst))) {
    char *idna;
    hts_boolean fits;

    if (cookie_host_refusal(dst, strlen(dst)) != NULL)
      return HTS_FALSE;
    idna = hts_convertStringUTF8ToIDNA(dst, strlen(dst));
    fits = idna != NULL && strlen(idna) < dst_size;
    if (fits)
      strlcpybuff(dst, idna, dst_size);
    freet(idna);
    if (!fits)
      return HTS_FALSE;
  }
  /* "%25" is RFC 6874's URI spelling of a zone id's '%'. Only a bracketed
     literal carries it, because a bare host's '%' is already literal. */
  if (literal) {
    char *const zone = strchr(dst, '%');

    if (zone != NULL && zone[1] == '2' && zone[2] == '5')
      memmove(zone + 1, zone + 3, strlen(zone + 3) + 1);
  }
  return HTS_TRUE;
}

static hts_boolean cookie_domain_is_suffix(const char *domain);

/* See htsbauth.h. Records are kept longest path first. */
int cookie_set(t_cookie *cookie, const char *cook_name, const char *cook_value,
               const char *domain, const char *path, const cookie_attr *attr) {
  static const cookie_attr session = {.host_only = HTS_FALSE,
                                      .secure = HTS_FALSE,
                                      .http_only = HTS_FALSE,
                                      .expires = 0};
  char buffer[8192];
  char host[COOKIE_MAX_DOMAIN];
  char *a = cookie->data;
  char *insert;
  char *old;
  size_t used;
  char cook[16384];
  hts_boolean host_only;

  if (attr == NULL)
    attr = &session;
  /* One representation per host, because an old or hand-edited jar carries a
     port the send side no longer queries with. */
  if (!cookie_host(domain, host, sizeof(host)))
    return -1;
  // a TAB or a line break inside a field would forge another field or record
  if (strpbrk(cook_name, "\t\r\n") != NULL ||
      strpbrk(cook_value, "\t\r\n") != NULL ||
      strpbrk(host, "\t\r\n") != NULL || strpbrk(path, "\t\r\n") != NULL)
    return -1;

  /* RFC 6265 6.1 asks for 4096 bytes of name and value per cookie. The first
     test keeps the subtraction in the second from wrapping. */
  if (strlen(cook_name) > COOKIE_MAX_PAIR ||
      strlen(cook_value) > COOKIE_MAX_PAIR - strlen(cook_name))
    return -1;
  if (strlen(path) > COOKIE_MAX_PATH)
    return -1;

  domain = host;
  // a public suffix such as "com" reaches only that host (RFC 6265 5.3 step 5)
  host_only = attr->host_only || cookie_domain_is_suffix(domain);
  /* The record this one replaces still holds its room, so count it free, but
     remove it only once the new one is known to fit. */
  old = cookie_find_exact(cookie->data, cook_name, domain, path);
  used = strlen(cookie->data) -
         (old != NULL ? (size_t) (cookie_nextfield(old) - old) : 0);
  /* Each part against the room left, never a sum: the sum of five lengths
     read off the wire can wrap and pass a check it should fail. */
  {
    const size_t parts[] = {
        used,           strlen(cook_value), strlen(cook_name),
        strlen(domain), strlen(path),       COOKIE_HEADROOM};
    size_t left = cookie->max_len;
    size_t i;

    for (i = 0; i < sizeof(parts) / sizeof(parts[0]); i++) {
      if (parts[i] > left)
        return -1; // no room to add it
      left -= parts[i];
    }
  }
  /* The Netscape fields, then an eighth only this jar reads, because the file
     form marks HttpOnly with a "#HttpOnly_" prefix instead (see cookie_save).
     The checks above bound the record far below sizeof(cook). */
  (void) snprintf(cook, sizeof(cook), "%s\t%s\t%s\t%s\t" LLintP "\t%s\t%s%s\n",
                  domain, host_only ? "FALSE" : "TRUE", path,
                  attr->secure ? "TRUE" : "FALSE", (LLint) attr->expires,
                  cook_name, cook_value,
                  attr->http_only ? "\t" COOKIE_HTTPONLY_FIELD : "");
  /* Backstop, not a live gate: the check above reserves COOKIE_HEADROOM and
     this record adds ~30 of literals, so it fires only if that assembly ever
     outgrows the headroom. Overflow-safe form regardless. */
  if (used >= cookie->max_len || strlen(cook) >= cookie->max_len - used)
    return -1; // no room to add it
  if (old != NULL)
    cookie_delete(old, cookie->max_len - (size_t) (old - cookie->data),
                  (size_t) (cookie_nextfield(old) - old));

  insert = a; // the longest path goes first, so insert before shorter ones
  while (*a) {
    if (strlen(cookie_get(buffer, a, 2)) < strlen(path))
      a = cookie->data + strlen(cookie->data);
    else {
      a = cookie_nextfield(a);
      insert = a;
    }
  }
  cookie_insert(insert, cookie->max_len - (size_t) (insert - cookie->data),
                cook);
#if DEBUG_COOK
  printf("add_new cookie: name=\"%s\" value=\"%s\" domain=\"%s\" path=\"%s\"\n",
         cook_name, cook_value, domain, path);
#endif
  return 0;
}

/* See htsbauth.h. */
int cookie_add(t_cookie *cookie, const char *cook_name, const char *cook_value,
               const char *domain, const char *path) {
  return cookie_set(cookie, cook_name, cook_value, domain, path, NULL);
}

/* Is the record at rec host-only? Field 1 is the Netscape include-subdomains
   flag, so only an explicit FALSE says so. */
static hts_boolean cookie_is_host_only(const char *rec) {
  char buffer[8192];

  return strcmpnocase(cookie_get(buffer, rec, 1), "FALSE") == 0;
}

/* Do two jar domains name the same domain? The leading dot is the Netscape
   spelling of the same thing. */
static hts_boolean cookie_same_domain(const char *a, const char *b) {
  if (a[0] == '.')
    a++;
  if (b[0] == '.')
    b++;
  return strcmpnocase(a, b) == 0;
}

/* See htsbundle.h. The host-only flag is not part of a record's identity
   (RFC 6265 5.3 step 11). */
char *cookie_find_exact(char *s, const char *cook_name, const char *domain,
                        const char *path) {
  char buffer[8192];
  char *a;

  for (a = s; *a != '\0'; a = cookie_nextfield(a)) {
    if (strcmp(cookie_get(buffer, a, 5), cook_name) == 0 &&
        cookie_same_domain(cookie_get(buffer, a, 0), domain) &&
        strcmp(cookie_get(buffer, a, 2), path) == 0)
      return a;
  }
  return NULL;
}

/* See htsbauth.h. */
int cookie_remove(t_cookie *cookie, const char *cook_name, const char *domain,
                  const char *path) {
  char *a, *b;
  char host[COOKIE_MAX_DOMAIN];

  /* Same normalisation as cookie_set, or the pair would disagree on where a
     port-qualified domain was stored. */
  if (!cookie_host(domain, host, sizeof(host)))
    return 0; // nothing can be stored under it, so nothing to delete

  b = cookie_find_exact(cookie->data, cook_name, host, path);
  if (b) {
    a = cookie_nextfield(b);
    cookie_delete(b, cookie->max_len - (size_t) (b - cookie->data), a - b);
#if DEBUG_COOK
    printf("deleted old cookie: %s %s %s\n", cook_name, host, path);
#endif
  }
  return 0;
}

/* See htsbauth.h. */
hts_boolean cookie_domain_match(const char *jar_dom, const char *host) {
  size_t dom_len;
  const size_t host_len = strlen(host);

  // a Netscape jar writes ".example.com" where RFC 6265 stores example.com
  if (jar_dom[0] == '.')
    jar_dom++;
  dom_len = strlen(jar_dom);
  // an empty domain is a suffix of every host, so it would match all of them
  if (dom_len == 0 || dom_len > host_len)
    return HTS_FALSE;
  if (strcmpnocase(jar_dom, host + host_len - dom_len) != 0)
    return HTS_FALSE;
  if (dom_len == host_len)
    return HTS_TRUE; // the same host
  /* Shorter than the host, so it is a parent domain only if the byte before it
     ends a label, for example "example.com" is no parent of wwwexample.com. */
  if (host[host_len - dom_len - 1] != '.')
    return HTS_FALSE;
  // an address has no parent domain, or "1.1" would reach 192.168.1.1
  if (hts_host_is_address(host, host_len))
    return HTS_FALSE;
  return HTS_TRUE;
}

/* Is DOMAIN a public suffix such as "com"? A leading or trailing dot is
   ignored. */
static hts_boolean cookie_domain_is_suffix(const char *domain) {
  char dom[COOKIE_MAX_DOMAIN];
  size_t len;

  if (domain[0] == '.')
    domain++;
  len = strlen(domain);
  if (len != 0 && domain[len - 1] == '.')
    len--;
  if (len == 0 || !hts_psl_loaded() || hts_host_is_address(domain, len))
    return HTS_FALSE;
  // no caller passes a longer one, and a clipped copy names another domain
  if (len >= sizeof(dom))
    return HTS_FALSE;
  dom[0] = '\0';
  strlncatbuff(dom, domain, sizeof(dom), len);
  hts_lowcase(dom);
  return hts_psl_registrable_domain(dom) == NULL;
}

/* See htsbauth.h. */
hts_boolean cookie_domain_allowed(const char *domain, const char *host) {
  char dom[COOKIE_MAX_DOMAIN];

  // a Netscape jar writes ".example.com" where RFC 6265 stores example.com
  if (domain[0] == '.')
    domain++;
  if (domain[0] == '\0' || strlen(domain) >= sizeof(dom))
    return HTS_FALSE;
  strcpybuff(dom, domain);
  hts_lowcase(dom);
  if (strcmp(dom, host) == 0)
    return HTS_TRUE;
  if (!cookie_domain_match(dom, host))
    return HTS_FALSE;
  return !cookie_domain_is_suffix(dom);
}

/* RFC 6265bis 5.6.1 caps a cookie's life at 400 days, which also keeps the
   sum below from overflowing. */
#define COOKIE_MAX_LIFE ((int64_t) 400 * 24 * 3600)

/* Bound WHEN to (0, now + COOKIE_MAX_LIFE], because 0 means a session. */
static int64_t cookie_bound_expiry(int64_t when, int64_t now) {
  if (when > now + COOKIE_MAX_LIFE)
    return now + COOKIE_MAX_LIFE;
  return when < 1 ? 1 : when;
}

/* See htsbauth.h. */
hts_boolean cookie_max_age_time(const char *value, int64_t now, int64_t *when) {
  const hts_boolean negative = value[0] == '-';
  const char *const digits = value + (negative ? 1 : 0);
  const size_t ndigits = strspn(digits, "0123456789");
  int64_t delta = 0;
  size_t i;

  if (ndigits == 0 || digits[ndigits] != '\0')
    return HTS_FALSE;
  // saturate at the cap, so no delta is too large to read
  for (i = 0; i < ndigits && delta <= COOKIE_MAX_LIFE; i++)
    delta = delta * 10 + (digits[i] - '0');
  *when = negative || delta == 0 ? 1 : cookie_bound_expiry(now + delta, now);
  return HTS_TRUE;
}

/* See htsbauth.h. */
hts_boolean cookie_expires_time(const char *value, int64_t now, int64_t *when) {
  struct tm tm;

  // RFC 6265 5.1.1 reads a two-digit year below 70 as 20xx
  if (convert_time_rfc822_pivot(&tm, value, 69) == NULL)
    return HTS_FALSE;
  *when = cookie_bound_expiry(hts_utc_seconds(&tm), now);
  return HTS_TRUE;
}

/* Is s a non-empty run of decimal digits? */
static hts_boolean cookie_is_number(const char *s) {
  const size_t n = strspn(s, "0123456789");

  return n > 0 && s[n] == '\0';
}

/* Is s an expiry: digits, or empty for a session? */
static hts_boolean cookie_is_expiry(const char *s) {
  return cookie_is_number(s) || s[0] == '\0';
}

/* Return the expiry in field 4 of rec, or 0, a session, when it is not all
   digits. */
static int64_t cookie_record_expiry(const char *rec) {
  char buffer[8192];
  const char *const field = cookie_get(buffer, rec, 4);
  const size_t ndigits = strspn(field, "0123456789");
  int64_t when = 0;
  size_t i;

  if (!cookie_is_number(field))
    return 0;
  // saturate rather than overflow on a hand-edited value
  for (i = 0; i < ndigits && when < INT64_MAX / 10 - 9; i++)
    when = when * 10 + (field[i] - '0');
  return when;
}

/* See htsbauth.h. */
hts_boolean cookie_sendable(const char *rec, hts_boolean https, int64_t now) {
  char buffer[8192];
  const int64_t expires = cookie_record_expiry(rec);

  if (expires != 0 && expires <= now)
    return HTS_FALSE;
  return https || strcmpnocase(cookie_get(buffer, rec, 3), "TRUE") != 0;
}

// rechercher cookie à partir de la position s (par exemple s=cookie.data)
// renvoie pointeur sur ligne, ou NULL si introuvable
// path est aligné à droite et cook_name peut être vide (chercher alors tout cookie)
// .doubleclick.net     TRUE    /       FALSE   1999999999      id      A
char *cookie_find(char *s, const char *cook_name, const char *domain, const char *path) {
  char buffer[8192];
  char *a = s;

  while(*a) {
    int t;

    if (strnotempty(cook_name) == 0)
      t = 1;                    // accepter par défaut
    else
      t = (strcmp(cookie_get(buffer, a, 5), cook_name) == 0);   // tester si même nom
    if (t) {                    // même nom ou nom qualconque
      //
      const char *chk_dom = cookie_get(buffer, a, 0); // cookie's own domain

      /* The path below is byte-exact where the domain is not, because RFC 6265
         folds a host name and matches a path literally. */
      if (cookie_domain_match(chk_dom, domain) &&
          (!cookie_is_host_only(a) || cookie_same_domain(chk_dom, domain))) {
        //
        const char *chk_path = cookie_get(buffer, a, 2);    // chemin concerné par le cookie

        if (strlen(chk_path) <= strlen(path)) {
          if (strncmp(path, chk_path, strlen(chk_path)) == 0) {       // même chemin
            return a;
          }
        }
      }
    }
    a = cookie_nextfield(a);
  }
  return NULL;
}

// renvoie prochain champ
char *cookie_nextfield(char *a) {
  char *b = a;

  a = strchr(a, '\n');          // prochain champ
  if (a == NULL)
    a = b + strlen(b);          // fin
  else
    a++;
  a += strspn(a, "\n");
  return a;
}

/* Warn about the line just read, with fmt for -%K and cookies.txt. A bundle's
   warning gives the line number and reason, never a byte that can be secret. */
static void cookie_warn(httrackp *opt, const cookie_filter *filter,
                        const char *given, const char *reason, const char *fmt,
                        ...) HTS_PRINTF_FUN(5, 6);

static void cookie_warn(httrackp *opt, const cookie_filter *filter,
                        const char *given, const char *reason, const char *fmt,
                        ...) {
  va_list args;

  if (filter != NULL) {
    hts_log_print(opt, LOG_WARNING, "Bundle %s, line %d: %s", given,
                  filter->line, reason);
    return;
  }
  va_start(args, fmt);
  hts_log_vprint(opt, LOG_WARNING, fmt, args);
  va_end(args);
}

/* Copy Netscape-jar field <param> into dst, or refuse the line and say so: a
   shortened domain or path would silently change where the cookie is sent. */
static hts_boolean cookie_load_field(httrackp *opt, char *dst, size_t dst_size,
                                     char *buffer, const char *line, int param,
                                     const char *field_name, const char *given,
                                     const cookie_filter *filter) {
  const char *const value = cookie_get(buffer, line, param);
  const size_t len = strlen(value);

  if (len >= dst_size) {
    cookie_warn(opt, filter, given, "a cookie field is too long",
                "Cookies: ignoring a line whose %s field is %d bytes "
                "(maximum %d): %s",
                field_name, (int) len, (int) dst_size - 1, given);
    return HTS_FALSE;
  }
  strlcpybuff(dst, value, dst_size);
  return HTS_TRUE;
}

/* Read a line of fp into s without its LF or the CR before it. *len receives
   the whole length, even when the line did not fit. A NUL is stored as is. */
/* See htsbundle.h. */
text_src text_src_file(FILE *fp) {
  text_src src;

  memset(&src, 0, sizeof(src));
  src.fp = fp;
  return src;
}

/* See htsbundle.h. */
text_src text_src_mem(const char *buf, size_t len) {
  text_src src;

  memset(&src, 0, sizeof(src));
  src.buf = buf;
  src.len = len;
  return src;
}

/* The next byte of src, or EOF. */
static int text_src_getc(text_src *src) {
  int c;

  if (src->fp == NULL) {
    if (src->pos < src->len)
      return (unsigned char) src->buf[src->pos++];
    src->eof = HTS_TRUE;
    return EOF;
  }
  c = fgetc(src->fp);
  if (c == EOF) {
    src->eof = feof(src->fp) != 0;
    src->error = ferror(src->fp) != 0;
  }
  return c;
}

/* See htsbundle.h. */
void text_src_readline(text_src *src, char *s, size_t size, size_t *len) {
  size_t n = 0, total = 0;
  int c;

  while ((c = text_src_getc(src)) != EOF && c != '\n') {
    if (n + 1 < size)
      s[n++] = (char) c;
    total++;
  }
  if (n > 0 && s[n - 1] == '\r' && n == total) {
    n--;
    total--;
  }
  s[n] = '\0';
  *len = total;
}

/* Is this comment line one curl, wget, yt-dlp, a browser export or HTTrack
   writes, so that a rewrite loses nothing by dropping it? Runs of spaces and
   TABs compare as one space, and trailing ones are ignored. */
static hts_boolean cookie_known_comment(const char *line) {
  static const char *const known[] = {
      "# Netscape HTTP Cookie File",
      "# HTTP Cookie File",
      "# HTTP cookie file.",
      "# Written by HTTrack Website Copier",
      "# https://curl.se/docs/http-cookies.html",
      "# https://curl.haxx.se/docs/http-cookies.html",
      "# http://curl.haxx.se/rfc/cookie_spec.html",
      "# https://curl.haxx.se/rfc/cookie_spec.html",
      "# This file was generated by libcurl! Edit at your own risk.",
      "# http://www.netscape.com/newsref/std/cookie_spec.html",
      "# This is a generated file! Do not edit.",
      "# This file is generated by yt-dlp. Do not edit.",
      "# Edit at your own risk.",
  };
  // wget dates its header line
  static const char wget[] = "# Generated by Wget on ";
  char norm[128];
  size_t n = 0, i;
  hts_boolean cut = HTS_FALSE;

  for (; *line != '\0' && !cut; line++) {
    const hts_boolean blank = *line == ' ' || *line == '\t';

    if (blank && n > 0 && norm[n - 1] == ' ')
      continue;
    if (n + 1 >= sizeof(norm))
      cut = HTS_TRUE; // longer than any known line, so only wget's can match
    else
      norm[n++] = blank ? ' ' : *line;
  }
  if (n > 0 && norm[n - 1] == ' ')
    n--;
  norm[n] = '\0';
  if (strncmp(norm, wget, sizeof(wget) - 1) == 0)
    return HTS_TRUE;
  if (cut)
    return HTS_FALSE;
  for (i = 0; i < sizeof(known) / sizeof(known[0]); i++) {
    if (strcmp(norm, known[i]) == 0)
      return HTS_TRUE;
  }
  return HTS_FALSE;
}

/* Copy into file the path that name names once symbolic links are followed,
   so the load and the save find the same .bak. Returns HTS_FALSE when name
   is a dangling link or the path does not fit. */
static hts_boolean cookie_resolve(char *file, size_t size, const char *name) {
#ifndef _WIN32
  char *real = realpath(name, NULL);
  struct stat st;

  if (real != NULL) {
    const hts_boolean fits = strlen(real) < size;

    if (fits)
      strlcpybuff(file, real, size);
    freet(real);
    return fits;
  }
  // a save would replace the link by a plain file
  if (lstat(name, &st) == 0 && S_ISLNK(st.st_mode))
    return HTS_FALSE;
#endif
  return slprintfbuff(file, size, "%s", name);
}

/* Warn, naming shown, and return HTS_TRUE when file is missing beside bak or
   empty beside a non-empty bak. */
static hts_boolean cookie_bak_stranded(httrackp *opt, const char *file,
                                       const char *bak, const char *shown) {
  if (!fexist_utf8(bak))
    return HTS_FALSE;
  if (!fexist_utf8(file)) {
    hts_log_print(opt, LOG_WARNING,
                  "Cookies: %s is missing but its .bak exists, so it is "
                  "not created until you restore or delete the .bak",
                  shown);
    return HTS_TRUE;
  }
  if (fsize_utf8(file) == 0 && fsize_utf8(bak) > 0) {
    hts_log_print(opt, LOG_WARNING,
                  "Cookies: %s is empty but its .bak is not, so it is "
                  "left unchanged until you restore or delete the .bak",
                  shown);
    return HTS_TRUE;
  }
  return HTS_FALSE;
}

/* A Windows editor may put this UTF-8 byte order mark first. */
static const char cookie_bom[] = "\xEF\xBB\xBF";

/* Return where the pairs of a document.cookie line start, past blanks and a
   pasted "Cookie:" header name. */
static char *cookie_flat_start(char *line) {
  int n;

  line += strspn(line, " \t");
  if ((n = strfield(line, "Cookie:")) != 0)
    line += n + strspn(line + n, " \t");
  return line;
}

/* Does the first non-blank line of fp look like a document.cookie string?
 *quote receives the quote around that whole line, or 0. Rewinds fp. */
static hts_boolean cookie_is_flat(FILE *fp, int *quote) {
  char line[8192];
  hts_boolean first = HTS_TRUE;
  hts_boolean flat = HTS_FALSE;

  *quote = 0;
  while (fgets(line, sizeof(line), fp) != NULL) {
    char *p = line;
    const char *eq;
    size_t end;

    if (first && strncmp(p, cookie_bom, sizeof(cookie_bom) - 1) == 0)
      p += sizeof(cookie_bom) - 1;
    first = HTS_FALSE;
    if (p[strspn(p, " \t\r\n")] == '\0')
      continue;
    p += strspn(p, " \t");
    for (end = strlen(p); end > 0 && strchr(" \t\r\n", p[end - 1]); end--)
      ;
    // a browser console shows the string quoted
    if ((p[0] == '\'' || p[0] == '"') && end > 1 && p[end - 1] == p[0])
      *quote = *p++;
    p = cookie_flat_start(p);
    eq = strchr(p, '=');
    // a name holds no blank, so a Netscape line spaced instead of TABbed fails
    flat = p[0] != '#' && eq != NULL && strchr(p, '\t') == NULL &&
           p + strcspn(p, " ") >= eq;
    break;
  }
  rewind(fp);
  if (!flat)
    *quote = 0;
  return flat;
}

/* See htsbundle.h. */
hts_boolean cookie_url_host(const char *url, char *dst, size_t dst_size) {
  const char *const rest = jump_protocol_const(url);
  const size_t len = strcspn(rest, "/?#");
  char adr[HTS_URLMAXSIZE];

  if (len >= sizeof(adr))
    return HTS_FALSE;
  // file://C:/x names a Windows drive, not a host
  if (strfield(url, "file:") && len == 2 && isalpha((unsigned char) rest[0]) &&
      (rest[1] == ':' || rest[1] == '|'))
    return HTS_FALSE;
  adr[0] = '\0';
  strlncatbuff(adr, rest, sizeof(adr), len);
  return cookie_host(adr, dst, dst_size);
}

/* A document.cookie string logs this many ignored cookies, then a count. */
#define COOKIE_FLAT_WARNINGS 10

/* Count one more ignored cookie in *ignored. Should it still be logged? */
static hts_boolean cookie_flat_ignore(int *ignored) {
  if (*ignored < INT_MAX)
    (*ignored)++;
  return *ignored <= COOKIE_FLAT_WARNINGS;
}

/* See htsbundle.h. */
hts_boolean cookie_flat_pair(httrackp *opt, t_cookie *cookie, char *pair,
                             const char *domain, const cookie_attr *attr,
                             const char *given, int *ignored) {
  char *const eq = strchr(pair, '=');

  if (eq == NULL || eq == pair) {
    if (cookie_flat_ignore(ignored))
      hts_log_print(opt, LOG_WARNING,
                    "Cookies: ignoring a cookie with no name in %s", given);
    return HTS_FALSE;
  }
  *eq = '\0';
  // a browser lists the most specific path first, so the first one wins
  if (cookie_find_exact(cookie->data, pair, domain, "/") != NULL) {
    if (cookie_flat_ignore(ignored))
      hts_log_print(
          opt, LOG_WARNING,
          "Cookies: ignoring a repeated '%.64s' in %s, the first wins", pair,
          given);
    return HTS_FALSE;
  }
  if (cookie_set(cookie, pair, eq + 1, domain, "/", attr) != 0) {
    if (cookie_flat_ignore(ignored))
      hts_log_print(opt, LOG_WARNING,
                    "Cookies: ignoring '%.64s' in %s (not stored)", pair,
                    given);
    return HTS_FALSE;
  }
  return HTS_TRUE;
}

/* See htsbundle.h. */
const char *cookie_flat_bind(const char *url, char *host, size_t host_size,
                             cookie_attr *attr) {
  const char *domain;

  if (!cookie_url_host(url, host, host_size))
    return NULL;
  // an address or a public suffix has no parent domain to share cookies with
  domain = hts_host_is_address(host, strlen(host))
               ? NULL
               : hts_psl_registrable_domain(host);
  attr->host_only = domain == NULL;
  attr->secure = strfield(url, "https:") != 0;
  attr->http_only = HTS_FALSE;
  attr->expires = 0;
  return domain != NULL ? domain : host;
}

/* Load fp, a document.cookie string, as session cookies of the registrable
   domain of start_url. The file is never rewritten, because the save would
   turn it into a Netscape jar, so this returns COOKIE_LOAD_KEEP. */
static int cookie_load_flat(httrackp *opt, t_cookie *cookie, FILE *fp,
                            const char *given, const char *start_url, int quote,
                            hts_cookies_verdict *why) {
  char host[COOKIE_MAX_DOMAIN];
  char BIGSTK pair[COOKIE_MAX_PAIR + 2]; // name, '=', value and the NUL
  size_t len = 0;
  hts_boolean nul = HTS_FALSE, too_long = HTS_FALSE;
  hts_boolean line_start = HTS_TRUE;
  hts_boolean quoted = HTS_FALSE, quote_done = quote == 0;
  cookie_attr attr;
  const char *domain;
  int stored = 0, ignored = 0;
  size_t i;
  int c;

  domain = cookie_flat_bind(start_url, host, sizeof(host), &attr);
  *why = domain != NULL ? HTS_COOKIES_KEPT_DOCUMENT_COOKIE
                        : HTS_COOKIES_KEPT_NO_START_URL;
  if (domain == NULL) {
    hts_log_print(opt, LOG_WARNING,
                  "Cookies: %s is a document.cookie string, but there is no "
                  "start URL to bind it to, so it is ignored",
                  given);
    return COOKIE_LOAD_KEEP;
  }

  for (i = 0; i < sizeof(cookie_bom) - 1; i++) {
    if (fgetc(fp) != (unsigned char) cookie_bom[i]) {
      rewind(fp);
      break;
    }
  }
  do {
    c = fgetc(fp);
    if (c != EOF && c != ';' && c != '\n') {
      // leading blanks are not counted against the room
      if (len == 0 && (c == ' ' || c == '\t' || c == '\r'))
        continue;
      if (len < sizeof(pair) - 1)
        pair[len++] = (char) c;
      else
        too_long = HTS_TRUE;
      if (c == '\0')
        nul = HTS_TRUE;
      continue;
    }
    if (too_long) {
      if (cookie_flat_ignore(&ignored))
        hts_log_print(opt, LOG_WARNING,
                      "Cookies: ignoring a cookie over %d bytes in %s",
                      (int) sizeof(pair) - 2, given);
    } else if (nul) {
      if (cookie_flat_ignore(&ignored))
        hts_log_print(opt, LOG_WARNING,
                      "Cookies: ignoring a cookie holding a NUL in %s", given);
    } else if (len > 0) {
      char *p = pair;

      while (len > 0 && strchr(" \t\r", pair[len - 1]) != NULL)
        len--;
      pair[len] = '\0';
      // the quotes around the first line, which cookie_is_flat found
      if (line_start && !quote_done && pair[0] == quote) {
        quote_done = quoted = HTS_TRUE;
        p++;
      }
      if (quoted && c != ';' && pair + len - 1 >= p && pair[len - 1] == quote)
        pair[--len] = '\0';
      if (line_start)
        p = cookie_flat_start(p);
      if (*p != '\0' &&
          cookie_flat_pair(opt, cookie, p, domain, &attr, given, &ignored))
        stored++;
    }
    len = 0;
    nul = too_long = HTS_FALSE;
    if (c != ';')
      quoted = HTS_FALSE;
    line_start = c == '\n';
  } while (c != EOF);
  if (ignored > COOKIE_FLAT_WARNINGS)
    hts_log_print(opt, LOG_WARNING, "Cookies: ignored %d more cookies in %s",
                  ignored - COOKIE_FLAT_WARNINGS, given);
  if (ferror(fp))
    hts_log_print(opt, LOG_WARNING,
                  "Cookies: stopped reading before the end of %s", given);
  hts_log_print(opt, LOG_WARNING,
                "Cookies: loaded %d from the document.cookie string in %s for "
                "%s%s. HttpOnly cookies are missing: use a cookies.txt export, "
                "or paste the Cookie header from DevTools \"Copy as cURL\"",
                stored, given, domain,
                attr.host_only ? " alone" : " and its subdomains");
  return COOKIE_LOAD_KEEP;
}

/* Count a refused line, saturating like lineno. */
static void cookie_refuse(int *refused) {
  if (*refused < INT_MAX)
    (*refused)++;
}

/* Read the Netscape records from fp's position to its end into cookie, and
   name the file as given in warnings. A UTF-8 BOM is skipped on the first line
   when first_line is HTS_TRUE. filter, when not NULL, picks the records.
   Returns what cookie_load_from does. */
static int cookie_load_records(httrackp *opt, t_cookie *cookie, text_src *src,
                               const char *given, hts_boolean first_line,
                               cookie_filter *filter) {
  char buffer[8192];
  int refused = 0;
  hts_boolean keep = HTS_FALSE;
  char BIGSTK line[8192];
  const size_t line_max = COOKIE_MAX_LINE;
  int lineno = 0;
  // this load's records alone, so a line repeating one is caught
  t_cookie *seen = (t_cookie *) calloct(1, sizeof(t_cookie));

  if (seen != NULL) {
    seen->max_len = sizeof(seen->data);
  } else {
    hts_log_print(opt, LOG_WARNING,
                  "Cookies: out of memory, so %s is left unchanged", given);
    keep = HTS_TRUE;
  }
  // ferror ends the loop too, because a directory never reaches EOF
  while (!src->eof && !src->error && strlen(cookie->data) < cookie->max_len &&
         (filter == NULL || filter->why == NULL)) {
    size_t len;

    // len counts what the buffer cut too, so this catches that line
    text_src_readline(src, line, sizeof(line), &len);
    if (lineno < INT_MAX)
      lineno++;
    if (filter != NULL)
      filter->line++;
    if (first_line && len >= sizeof(cookie_bom) - 1 &&
        strncmp(line, cookie_bom, sizeof(cookie_bom) - 1) == 0) {
      len -= sizeof(cookie_bom) - 1;
      memmove(line, line + sizeof(cookie_bom) - 1,
              strlen(line) + 1 - (sizeof(cookie_bom) - 1));
    }
    first_line = HTS_FALSE;
    // a bundle is written for this run, so a damaged one stops it
    if (filter != NULL &&
        hts_bundle_has_control(line,
                               len < sizeof(line) ? len : sizeof(line) - 1)) {
      filter->why = "a cookie line holds a control byte";
    } else if (filter != NULL && len >= line_max) {
      filter->why = "a cookie line is too long";
    } else if (len >= line_max) {
      hts_log_print(opt, LOG_WARNING,
                    "Cookies: ignoring an over-long line "
                    "(%d bytes, maximum %d): %s",
                    (int) len, (int) line_max - 1, given);
      cookie_refuse(&refused);
    } else if (memchr(line, '\0', len) != NULL ||
               memchr(line, '\r', len) != NULL) {
      hts_log_print(opt, LOG_WARNING,
                    "Cookies: ignoring a line holding a NUL or a CR: %s",
                    given);
      cookie_refuse(&refused);
    } else if (line[strspn(line, " \t")] != '\0') {
      /* curl and browser exports mark an HttpOnly cookie this way */
      static const char httponly[] = COOKIE_HTTPONLY_PREFIX;
      const hts_boolean is_httponly =
          strncmp(line, httponly, sizeof(httponly) - 1) == 0;
      const char *const rec = is_httponly ? line + sizeof(httponly) - 1 : line;

      if (rec[0] == '#') {
        if (!cookie_known_comment(rec)) {
          // no line text, because any part of it can hold a cookie value
          cookie_warn(opt, filter, given, "an unknown comment",
                      "Cookies: ignoring an unknown comment on line %d of %s",
                      lineno, given);
          cookie_refuse(&refused);
        }
      } else {
        char domain[COOKIE_MAX_DOMAIN];             // .netscape.com
        char path[COOKIE_MAX_PATH];                 // /
        char BIGSTK cook_name[COOKIE_MAX_PAIR + 1]; // MYCOOK
        char BIGSTK cook_value[8192];               // ID=toto,S=1234
        const char *p;
        int tabs = 0;

#if DEBUG_COOK
        printf("%s\n", rec);
#endif
        for (p = strchr(rec, '\t'); p != NULL; p = strchr(p + 1, '\t'))
          tabs++;
        if (tabs < 6 || !strnotempty(cookie_get(buffer, rec, 5))) {
          cookie_warn(opt, filter, given,
                      "a cookie line has fewer than 7 fields or no name",
                      "Cookies: ignoring a line with fewer than 7 "
                      "fields or no name: %s",
                      given);
          cookie_refuse(&refused);
        } else if (tabs > 7 ||
                   (tabs == 7 && strcmp(cookie_get(buffer, rec, 7),
                                        COOKIE_HTTPONLY_FIELD) != 0)) {
          // a TAB in the value, because only HTTrack writes an eighth field
          cookie_warn(opt, filter, given,
                      "a cookie line has more than 7 fields",
                      "Cookies: ignoring a line with more than 7 "
                      "fields: %s",
                      given);
          cookie_refuse(&refused);
        } else if (!cookie_is_expiry(cookie_get(buffer, rec, 4))) {
          cookie_warn(opt, filter, given,
                      "a cookie expiry is not a whole number",
                      "Cookies: ignoring a line whose expiry is not a "
                      "whole number: %s",
                      given);
          cookie_refuse(&refused);
        } else if (cookie_load_field(opt, domain, sizeof(domain), buffer, rec,
                                     0, "domain", given, filter) &&
                   cookie_load_field(opt, path, sizeof(path), buffer, rec, 2,
                                     "path", given, filter) &&
                   cookie_load_field(opt, cook_name, sizeof(cook_name), buffer,
                                     rec, 5, "name", given, filter) &&
                   cookie_load_field(opt, cook_value, sizeof(cook_value),
                                     buffer, rec, 6, "value", given, filter)) {
          cookie_attr attr;
          char host[COOKIE_MAX_DOMAIN];
          const hts_boolean repeated =
              seen != NULL && cookie_host(domain, host, sizeof(host)) &&
              cookie_find_exact(seen->data, cook_name, host, path) != NULL;

          attr.host_only = cookie_is_host_only(rec);
          attr.secure = strcmpnocase(cookie_get(buffer, rec, 3), "TRUE") == 0;
          attr.http_only = is_httponly || tabs == 7;
          attr.expires = cookie_record_expiry(rec);
          // an expired one is skipped, because a browser drops it too
          if ((attr.expires == 0 || attr.expires > (int64_t) time(NULL)) &&
              (filter == NULL ||
               filter->admit(filter, cookie, repeated, cook_name, cook_value,
                             domain, path, &attr))) {
            // cookie_set stores it host-only
            if (!attr.host_only && cookie_domain_is_suffix(domain))
              cookie_warn(opt, filter, given,
                          "a cookie for a public suffix is now host-only",
                          "Cookies: '%.64s' for the public suffix %s is "
                          "now host-only: %s",
                          cook_name, domain, given);
            // the last one wins, as curl reads it, but a rewrite loses one
            if (repeated) {
              cookie_warn(opt, filter, given, "a cookie appears twice",
                          "Cookies: '%.64s' appears twice in %s", cook_name,
                          given);
              cookie_refuse(&refused);
            }
            if (cookie_set(cookie, cook_name, cook_value, domain, path,
                           &attr) != 0) {
              // with full_skips, the bundles alone fit, so -%K filled the jar
              if (filter != NULL && filter->full_skips)
                cookie_warn(opt, filter, given,
                            "the cookie jar is full, so a cookie is left out",
                            "Cookies: the jar is full: %s", given);
              else if (filter != NULL)
                filter->why = "the cookie jar did not store a cookie";
              else
                hts_log_print(opt, LOG_WARNING,
                              "Cookies: the jar did not store '%.64s' from "
                              "%s",
                              cook_name, given);
              cookie_refuse(&refused);
            } else if (seen != NULL && !repeated) {
              (void) cookie_set(seen, cook_name, "", domain, path, NULL);
            }
          }
        } else {
          cookie_refuse(&refused);
        }
      }
    }
  }
  if (!src->eof && (filter == NULL || filter->why == NULL)) {
    cookie_warn(opt, filter, given, "stopped reading before the end",
                "Cookies: stopped reading before the end of %s", given);
    cookie_refuse(&refused);
  }
  freet(seen);
  return keep ? COOKIE_LOAD_KEEP : refused;
}

/* cookie_load_from without the IE jars. *why receives the verdict. */
static int cookie_load_file(httrackp *opt, t_cookie *cookie, const char *given,
                            const char *start_url, hts_cookies_verdict *why) {
  char file[CATBUFF_SIZE];
  char bak[CATBUFF_SIZE];
  int quote = 0;
  FILE *fp;

  *why = HTS_COOKIES_REWRITTEN;
  if (!cookie_resolve(file, sizeof(file), given)) {
    hts_log_print(opt, LOG_WARNING,
                  "Cookies: %s is a symbolic link to a missing file, so it "
                  "is left unchanged",
                  given);
    *why = HTS_COOKIES_KEPT_DANGLING_LINK;
    return COOKIE_LOAD_KEEP;
  }
  if (slprintfbuff(bak, sizeof(bak), "%s.bak", file) &&
      cookie_bak_stranded(opt, file, bak, given)) {
    *why = fexist_utf8(file) ? HTS_COOKIES_KEPT_EMPTY_BESIDE_BAK
                             : HTS_COOKIES_KEPT_MISSING_BESIDE_BAK;
    return COOKIE_LOAD_KEEP;
  }
  fp = FOPEN(file, "rb");
  if (fp != NULL && start_url != NULL && cookie_is_flat(fp, &quote)) {
    const int ret =
        cookie_load_flat(opt, cookie, fp, given, start_url, quote, why);

    fclose(fp);
    return ret;
  }
  if (fp) {
    text_src src = text_src_file(fp);
    const int ret = cookie_load_records(opt, cookie, &src, given,
                                        /* first_line */ HTS_TRUE, NULL);

    fclose(fp);
    if (ret == COOKIE_LOAD_KEEP)
      *why = HTS_COOKIES_KEPT_UNREADABLE;
    return ret;
  } else if (errno != ENOENT) {
    // the file exists, so a rewrite would replace what was never read
    hts_log_print(opt, LOG_WARNING | LOG_ERRNO,
                  "Cookies: could not read %s, so it is left unchanged", given);
    *why = HTS_COOKIES_KEPT_UNREADABLE;
    return COOKIE_LOAD_KEEP;
  }
  *why = HTS_COOKIES_CREATED;
  return -1;
}

/* See htsbauth.h. */
int cookie_load(httrackp *opt, t_cookie *cookie, const char *fpath,
                const char *name) {
  return cookie_load_from(opt, cookie, fpath, name, NULL);
}

/* See htsbauth.h. */
int cookie_load_from(httrackp *opt, t_cookie *cookie, const char *fpath,
                     const char *name, const char *start_url) {
  char catbuff[CATBUFF_SIZE];

  // Merge any IE cookies first
#ifdef _WIN32
  {
    WIN32_FIND_DATAW find;
    HANDLE h;
    char pth[HTS_URLMAXSIZE];
    LPWSTR wpth;

    strcpybuff(pth, fpath);
    strcatbuff(pth, "*@*.txt");
    // Wide glob so a long or non-ASCII IE-cookie folder is scanned (#133).
    wpth = hts_pathToUCS2(pth);
    h = wpth != NULL ? FindFirstFileW(wpth, &find) : INVALID_HANDLE_VALUE;
    freet(wpth);
    if (h != INVALID_HANDLE_VALUE) {
      do {
        if (!(find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
          if (!(find.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM)) {
            // cFileName is UTF-16: convert to UTF-8 so the mirror path and
            // file wrappers stay UTF-8 (no CP_ACP mojibake).
            char *u = hts_convertUCS2StringToUTF8(find.cFileName, -1);
            FILE *fp =
                u != NULL
                    ? FOPEN(fconcat(catbuff, sizeof(catbuff), fpath, u), "rb")
                    : NULL;

            if (fp) {
              char cook_name[256];
              char cook_value[1000];
              char domainpathpath[512];
              char dummy[512];

              lien_adrfil af; // host + path (/)
              int cookie_merged = 0;

              // Read all cookies
              while(!feof(fp)) {
                cook_name[0] = cook_value[0] = domainpathpath[0]
                = dummy[0] = af.adr[0] = af.fil[0] = '\0';
                linput(fp, cook_name, 250);
                if (!feof(fp)) {
                  linput(fp, cook_value, 250);
                  if (!feof(fp)) {
                    int i;

                    linput(fp, domainpathpath, 500);
                    /* Read 6 other useless values */
                    for(i = 0; !feof(fp) && i < 6; i++) {
                      linput(fp, dummy, 500);
                    }
                    if (strnotempty(cook_name)
                        && strnotempty(cook_value)
                        && strnotempty(domainpathpath)) {
                      if (ident_url_absolute(domainpathpath, &af) >= 0) {
                        if (cookie_add(cookie, cook_name, cook_value, af.adr,
                                       af.fil) != 0) {
                          hts_log_print(opt, LOG_WARNING,
                                        "Cookies: the jar did not store "
                                        "'%.64s' from %s",
                                        cook_name, u);
                        }
                        cookie_merged = 1;
                      }
                    }
                  }
                }
              }
              fclose(fp);
              if (cookie_merged)
                UNLINK(fconcat(catbuff, sizeof(catbuff), fpath, u));
            }                   // if fp
            freet(u);
          }
      } while (FindNextFileW(h, &find));
      FindClose(h);
    }
  }
#endif

  // then cookies.txt
  {
    hts_cookies_verdict why;

    return cookie_load_file(opt, cookie,
                            fconcat(catbuff, sizeof(catbuff), fpath, name),
                            start_url, &why);
  }
}

HTSEXT_API hts_cookies_verdict hts_cookies_file_verdict(const char *path,
                                                        const char *start_url,
                                                        int *lost) {
  char catbuff[CATBUFF_SIZE];
  hts_cookies_verdict why = HTS_COOKIES_KEPT_UNREADABLE;
  hts_filetime_t when;
  const char *given;
  t_cookie *jar;
  int ret;

  if (lost != NULL)
    *lost = 0;
  if (path == NULL || *path == '\0')
    return HTS_COOKIES_KEPT_UNREADABLE;
  given = fconcat(catbuff, sizeof(catbuff), "", path);
  if (*given == '\0')
    return HTS_COOKIES_KEPT_UNREADABLE;
  // a FIFO or a device would block the read or never end it
  if (hts_file_mtime(given, &when) && !when.is_plain_file)
    return HTS_COOKIES_KEPT_NOT_A_FILE;
  jar = (t_cookie *) calloct(1, sizeof(t_cookie));
  if (jar == NULL)
    return HTS_COOKIES_KEPT_UNREADABLE;
  jar->max_len = COOKIE_MAX_JAR;
  // httpmirror always passes a start URL
  ret = cookie_load_file(HTS_LOG_QUIET, jar, given,
                         start_url != NULL ? start_url : "", &why);
  freet(jar);
  if (ret > 0) {
    if (lost != NULL)
      *lost = ret;
    return HTS_COOKIES_KEPT_LINES_LOST;
  }
  return why;
}

/* See htsbundle.h. */
void cookie_load_stream(httrackp *opt, t_cookie *cookie, text_src *src,
                        const char *given, cookie_filter *filter) {
  (void) cookie_load_records(opt, cookie, src, given,
                             /* first_line */ HTS_FALSE, filter);
}

/* Flush fp to the disk and close it. Did every step succeed? */
static hts_boolean cookie_close_synced(FILE *fp) {
  // a crash after the rename must not leave an empty file behind it
  hts_boolean ok = fflush(fp) == 0;

#ifdef _WIN32
  ok = ok && _commit(_fileno(fp)) == 0;
#else
  // EINVAL: a device such as /dev/stdout cannot be synced
  ok = ok && (fsync(fileno(fp)) == 0 || errno == EINVAL);
#endif
  // Close even after a failure, so the stream never leaks
  return fclose(fp) == 0 && ok;
}

int cookie_save_fault_selftest = 0;

/* See htsbundle.h. */
void cookie_records_text(const t_cookie *cookie, String *out) {
  char BIGSTK line[8192];
  const char *a = cookie->data;

  while (*a != '\0') {
    char *extra = line;
    const char *rec = line;
    int i;

    a += binput(a, line, COOKIE_MAX_LINE);
    // stop on the seventh TAB, so the cut takes it with the eighth field
    for (i = 0; i < 7 && extra != NULL; i++) {
      extra = strchr(extra, '\t');
      if (extra != NULL && i < 6)
        extra++;
    }
    // the eighth field is ours: the file form marks HttpOnly by prefix
    if (extra != NULL) {
      *extra = '\0';
      StringCat(*out, COOKIE_HTTPONLY_PREFIX);
    }
    // a Netscape jar dots the domain exactly when field 1 is TRUE
    if (rec[0] == '.')
      rec++;
    if (!cookie_is_host_only(line))
      StringCat(*out, ".");
    StringCat(*out, rec);
    StringCat(*out, LF);
  }
}

/* Write the jar to name, or only a header if it is empty. Return 0 on
   success. */
static int cookie_write(t_cookie *cookie, const char *name) {
  FILE *fp = hts_fopen_private(name, "wb");

  if (fp) {
    String text = STRING_EMPTY;
    hts_boolean ok = HTS_TRUE;

    // Python's MozillaCookieJar refuses a file without this first line
    if (fprintf(fp, "# Netscape HTTP Cookie File" LF
                    "# Written by HTTrack Website Copier" LF) < 0 ||
        (cookie_save_fault_selftest & COOKIE_FAULT_WRITE))
      ok = HTS_FALSE;
    cookie_records_text(cookie, &text);
    if (StringNotEmpty(text) && fwrite(StringBuff(text), 1, StringLength(text),
                                       fp) != StringLength(text))
      ok = HTS_FALSE;
    StringFree(text);
    // the close reports a write the buffer held back
    if (!cookie_close_synced(fp) ||
        (cookie_save_fault_selftest & COOKIE_FAULT_CLOSE))
      ok = HTS_FALSE;
    return ok ? 0 : -1;
  }
  return -1;
}

/* Copy src to dst, which the caller removes on failure. The copy is
   owner-only on Unix. */
static hts_boolean cookie_copy(const char *src, const char *dst) {
  const int fault = cookie_save_fault_selftest;
  char buf[8192];
  FILE *const in = FOPEN(src, "rb");
  FILE *const out = in != NULL ? hts_fopen_private(dst, "wb") : NULL;
  hts_boolean ok = out != NULL;
  size_t n;

  while (ok && (n = fread(buf, 1, sizeof(buf), in)) > 0)
    ok =
        fwrite(buf, 1, (fault & COOKIE_FAULT_COPY_WRITE) ? n / 2 : n, out) == n;
  if (in != NULL && (ferror(in) || (fault & COOKIE_FAULT_COPY_READ)))
    ok = HTS_FALSE;
  if (in != NULL)
    fclose(in);
  if (out != NULL &&
      (!cookie_close_synced(out) || (fault & COOKIE_FAULT_COPY_CLOSE)))
    ok = HTS_FALSE;
  return ok;
}

/* Move src over dst in one step. Did it succeed? */
static hts_boolean cookie_move(const char *src, const char *dst) {
#ifdef _WIN32
  LPWSTR wsrc = hts_pathToUCS2(src);
  LPWSTR wdst = hts_pathToUCS2(dst);
  hts_boolean ok = HTS_FALSE;
  int tries;

  // dst is refused while another writer is replacing it, so wait that out
  for (tries = 0; wsrc != NULL && wdst != NULL && tries < 20; tries++) {
    DWORD err;

    if (MoveFileExW(wsrc, wdst,
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0) {
      ok = HTS_TRUE;
      break;
    }
    err = GetLastError();
    if (err != ERROR_ACCESS_DENIED && err != ERROR_SHARING_VIOLATION)
      break;
    Sleep(10);
  }
  freet(wsrc);
  freet(wdst);
  if (!ok)
    errno = EACCES; // the log reads errno, not GetLastError()
  return ok;
#else
  return RENAME(src, dst) == 0;
#endif
}

#ifndef _WIN32
/* Flush the directory holding file, so that a rename in it survives a crash.
   A failure is ignored, because some filesystems cannot sync a directory. */
static void cookie_sync_dir(const char *file) {
  char dir[CATBUFF_SIZE];
  char *slash;
  int fd;

  if (!slprintfbuff(dir, sizeof(dir), "%s", file))
    return;
  slash = strrchr(dir, '/');
  if (slash == NULL)
    strcpybuff(dir, ".");
  else
    slash[slash == dir ? 1 : 0] = '\0';
  fd = open(dir, O_RDONLY);
  if (fd != -1) {
    (void) fsync(fd);
    close(fd);
  }
}
#endif

/* Name in dst a temporary beside path that no other run or thread writes. */
static hts_boolean cookie_tmpname(char *dst, size_t size, const char *path) {
  static volatile int calls = 0;

  return slprintfbuff(dst, size, "%s.%lu.%u.tmp", path, back_self_pid(),
                      (unsigned int) hts_increment_int(&calls));
}

/* Make bak hold what file holds, leaving file in place. The old bak stays
   until the new one is complete. */
static hts_boolean cookie_backup(const char *file, const char *bak) {
  char next[CATBUFF_SIZE];
  hts_boolean ok;
  int err;

  if (!cookie_tmpname(next, sizeof(next), bak))
    return HTS_FALSE;
  (void) UNLINK(next);
#ifndef _WIN32
  if (!(cookie_save_fault_selftest & COOKIE_FAULT_NOLINK) &&
      link(file, next) == 0)
    ok = HTS_TRUE;
  else
#endif
    // Windows, or a filesystem without hard links
    ok = cookie_copy(file, next);
  ok = ok && cookie_move(next, bak);
  err = errno;
  // a failed copy, or a move onto another link to the same file, leaves next
  (void) UNLINK(next);
  errno = err;
  return ok;
}

/* Move tmp over file in one step, so that file holds the old or the new jar
   whatever fails. */
static hts_boolean cookie_replace(const char *tmp, const char *file) {
  if (cookie_save_fault_selftest & COOKIE_FAULT_REPLACE)
    return HTS_FALSE;
  if (!cookie_move(tmp, file))
    return HTS_FALSE;
#ifndef _WIN32
  cookie_sync_dir(file);
#endif
  return HTS_TRUE;
}

/* See htsbundle.h. */
hts_boolean cookie_write_replace(const char *file, const char *data,
                                 size_t len) {
  char tmp[CATBUFF_SIZE];
  hts_boolean ok;
  FILE *fp;
  int err;

  if (!cookie_tmpname(tmp, sizeof(tmp), file)) {
    errno = ENAMETOOLONG;
    return HTS_FALSE;
  }
  (void) UNLINK(tmp);
  fp = hts_fopen_private(tmp, "wb");
  if (fp == NULL)
    return HTS_FALSE;
  ok = hts_fwrite_exact(data, len, fp);
  ok = cookie_close_synced(fp) && ok;
  ok = ok && cookie_replace(tmp, file);
  err = errno;
  if (!ok)
    (void) UNLINK(tmp);
  errno = err;
  return ok;
}

/* Write the jar beside file, back file up to bak unless bak is NULL, then
   move the new jar over file. Return 0, or -1 after a warning naming name. */
static int cookie_commit(httrackp *opt, t_cookie *cookie, const char *file,
                         const char *bak, const char *name) {
  char tmp[CATBUFF_SIZE];

  if (!cookie_tmpname(tmp, sizeof(tmp), file)) {
    hts_log_print(opt, LOG_WARNING,
                  "Cookies: cannot rewrite %s because its name is too long",
                  name);
    return -1;
  }
  (void) UNLINK(tmp);
  if (cookie_write(cookie, tmp) != 0) {
    hts_log_print(opt, LOG_WARNING | LOG_ERRNO,
                  "Cookies: could not write %s, so %s is unchanged", tmp, name);
    (void) UNLINK(tmp);
    return -1;
  }
  // the file may have changed since the load checked it
  if (bak != NULL && cookie_bak_stranded(opt, file, bak, name)) {
    (void) UNLINK(tmp);
    return -1;
  }
  if (bak != NULL && fexist_utf8(file) && !cookie_backup(file, bak)) {
    hts_log_print(opt, LOG_WARNING | LOG_ERRNO,
                  "Cookies: could not back up %s, so it is unchanged", name);
    (void) UNLINK(tmp);
    return -1;
  }
  if (!cookie_replace(tmp, file)) {
    hts_log_print(opt, LOG_WARNING | LOG_ERRNO,
                  "Cookies: could not replace %s, so it is unchanged", name);
    (void) UNLINK(tmp);
    return -1;
  }
  return 0;
}

/* Save the jar over name as cookie_save does, keeping the old content in
   name's .bak when keep_bak is HTS_TRUE. */
static int cookie_save_to(httrackp *opt, t_cookie *cookie, const char *name,
                          hts_boolean keep_bak) {
  char file[CATBUFF_SIZE];
  char bak[CATBUFF_SIZE];

  if (!cookie_resolve(file, sizeof(file), fconv(bak, sizeof(bak), name))) {
    hts_log_print(opt, LOG_WARNING,
                  "Cookies: cannot rewrite %s, because it is a symbolic link "
                  "to a missing file",
                  name);
    return -1;
  }
  // an empty jar still empties an existing file, or a deleted cookie survives
  if (!strnotempty(cookie->data) && !fexist_utf8(file))
    return 0;
  if (keep_bak && !slprintfbuff(bak, sizeof(bak), "%s.bak", file)) {
    hts_log_print(opt, LOG_WARNING,
                  "Cookies: cannot rewrite %s because its name is too long",
                  name);
    return -1;
  }
  return cookie_commit(opt, cookie, file, keep_bak ? bak : NULL, name);
}

/* See htsbauth.h. */
int cookie_save(httrackp *opt, t_cookie *cookie, const char *name) {
  return cookie_save_to(opt, cookie, name, HTS_FALSE);
}

/* See htsbauth.h. */
void cookie_save_replacing(httrackp *opt, t_cookie *cookie, const char *name) {
  (void) cookie_save_to(opt, cookie, name, HTS_TRUE);
}

// Insert string ins before s. s_size is the capacity of the buffer at s.
void cookie_insert(char *s, size_t s_size, const char *ins) {
  char *buff;

  if (strnotempty(s) == 0) { // nothing there yet: just concatenate
    strlcatbuff(s, ins, s_size);
  } else {
    buff = (char *) malloct(strlen(s) + 1);
    if (buff) {
      strlcpybuff(buff, s, strlen(s) + 1); // temporary copy of s
      strlcpybuff(s, ins, s_size);         // write ins
      strlcatbuff(s, buff, s_size);        // then the saved content
      freet(buff);
    }
  }
}

// Delete the substring of s at position pos. s_size is the capacity at s.
void cookie_delete(char *s, size_t s_size, size_t pos) {
  char *buff;

  if (strnotempty(s + pos) == 0) { // nothing after pos: truncate
    s[0] = '\0';
  } else {
    buff = (char *) malloct(strlen(s + pos) + 1);
    if (buff) {
      strlcpybuff(buff, s + pos, strlen(s + pos) + 1); // temporary copy
      strlcpybuff(s, buff, s_size);                    // overwrite from start
      freet(buff);
    }
  }
}

// Return field <param> (0-based, tab-separated) of the cookie line cookie_base,
// into buffer. ex: cookie_get("ceci est<tab>un<tab>exemple", 1) returns "un".
// buffer must hold at least COOKIE_FIELD_BUFFER_SIZE bytes (all callers use
// char[8192]).
#define COOKIE_FIELD_BUFFER_SIZE 8192
const char *cookie_get(char *buffer, const char *cookie_base, int param) {
  const char *limit, *cur;
  hts_span field;
  int i;

  cookie_base += strspn(cookie_base, "\n");
  limit = strchr(cookie_base, '\n');
  if (limit == NULL)
    limit = cookie_base + strlen(cookie_base);
  /* A negative PARAM reads field 0, and a missing field leaves BUFFER alone. */
  for (cur = cookie_base, i = 0; hts_span_next(&cur, limit, '\t', &field);
       i++) {
    if (i >= param) {
      htsbuff b = htsbuff_ptr(buffer, COOKIE_FIELD_BUFFER_SIZE);

      htsbuff_catn(&b, field.p, field.len);
      return buffer;
    }
  }
  return "";
}

// fin cookies

// -- basic auth --

/* déclarer un répertoire comme possédant une authentification propre */
int bauth_add(t_cookie * cookie, const char *adr, const char *fil, const char *auth) {
  char buffer[HTS_URLMAXSIZE * 2];

  if (cookie) {
    if (!bauth_check(cookie, adr, fil)) {       // n'existe pas déja
      bauth_chain *chain = &cookie->auth;
      char *prefix = bauth_prefix(buffer, adr, fil);

      if (prefix == NULL)
        return 0;
      /* A clipped prefix matches more URLs than were authenticated. */
      if (strlen(prefix) >= sizeof(chain->prefix) ||
          strlen(auth) >= sizeof(chain->auth)) {
        return 0;
      }

      /* fin de la chaine */
      while(chain->next)
        chain = chain->next;
      chain->next = (bauth_chain *) calloct(sizeof(bauth_chain), 1);
      if (chain->next) {
        chain = chain->next;
        chain->next = NULL;
        strcpybuff(chain->auth, auth);
        strcpybuff(chain->prefix, prefix);
        return 1;
      }
    }
  }
  return 0;
}

/* Release the heap tail of the list; the head node is embedded in the jar. */
void bauth_free(t_cookie *cookie) {
  if (cookie != NULL) {
    bauth_chain *chain = cookie->auth.next;

    cookie->auth.next = NULL;
    while (chain != NULL) {
      bauth_chain *const next = chain->next;

      freet(chain);
      chain = next;
    }
  }
}

/* tester adr et fil, et retourner authentification si nécessaire */
/* sinon, retourne NULL */
char *bauth_check(t_cookie * cookie, const char *adr, const char *fil) {
  char buffer[HTS_URLMAXSIZE * 2];

  if (cookie) {
    bauth_chain *chain = &cookie->auth;
    char *prefix = bauth_prefix(buffer, adr, fil);

    if (prefix == NULL)
      return NULL;
    while(chain) {
      if (strnotempty(chain->prefix)) {
        if (strncmp(prefix, chain->prefix, strlen(chain->prefix)) == 0) {
          return chain->auth;
        }
      }
      chain = chain->next;
    }
  }
  return NULL;
}

/* Build the auth prefix (host + path, query stripped) into prefix, or return
   NULL if adr+fil does not fit. Callers pass HTS_URLMAXSIZE * 2 bytes. */
char *bauth_prefix(char *prefix, const char *adr, const char *fil) {
  const size_t size = HTS_URLMAXSIZE * 2;
  const char *const host = jump_identification_const(adr);
  const size_t used = strlen(host);
  char *a;

  /* a clipped prefix matches URLs nobody authenticated, so refuse instead */
  if (used >= size || strlen(fil) >= size - used)
    return NULL;
  strlcpybuff(prefix, host, size);
  strlcatbuff(prefix, fil, size);
  a = strchr(prefix, '?');
  if (a)
    *a = '\0';
  if (strchr(prefix, '/')) {
    a = hts_lastcharptr(prefix);
    while(*a != '/')
      a--;
    *(a + 1) = '\0';
  }
  return prefix;
}
