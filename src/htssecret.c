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
/* File: htssecret.c subroutines:                              */
/*       Secret redaction for logs and headers                  */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htscore.h"
#include "htsbase.h"
#include "htslib.h"
#include "htsthread.h"

/* The backstop: the passwords and tokens the engine has seen, hidden by value
   in every log line, behind the per-site masking. */
typedef struct hts_secret_set {
  htsmutex lock;
  int count;
  hts_boolean full;    /* a secret was refused for want of room */
  hts_boolean toolong; /* a secret was refused for its length */
  char *item[HTS_SECRET_MAX];
  size_t len[HTS_SECRET_MAX];
  hts_secret_kind kind[HTS_SECRET_MAX];
} hts_secret_set;

void hts_secret_init(httrackp *opt) {
  hts_secret_set *const set = calloct(1, sizeof(hts_secret_set));

  if (set != NULL)
    hts_mutexinit(&set->lock);
  opt->secrets = set;
}

void hts_secret_free(httrackp *opt) {
  hts_secret_set *set = (hts_secret_set *) opt->secrets;
  int i;

  if (set == NULL)
    return;
  for (i = 0; i < set->count; i++)
    freet(set->item[i]);
  hts_mutexfree(&set->lock);
  freet(set);
  opt->secrets = NULL;
}

void hts_secret_add(httrackp *opt, const char *s, size_t len,
                    hts_secret_kind kind) {
  hts_secret_set *const set =
      opt != NULL ? (hts_secret_set *) opt->secrets : NULL;
  hts_boolean warn = HTS_FALSE, warnlong = HTS_FALSE;
  int i;

  if (set == NULL || len == 0 ||
      (kind == HTS_SECRET_HEADER && len < HTS_SECRET_TOKEN))
    return;
  hts_mutexlock(&set->lock);
  for (i = 0; i < set->count; i++)
    if (set->kind[i] == kind && set->len[i] == len &&
        memcmp(set->item[i], s, len) == 0)
      break;
  if (len > HTS_SECRET_MAXLEN) {
    warnlong = set->toolong ? HTS_FALSE : HTS_TRUE;
    set->toolong = HTS_TRUE;
  } else if (i == set->count) {
    if (set->count < HTS_SECRET_MAX) {
      char *const copy = hts_span_dup(hts_span_at(s, len));

      if (copy != NULL) {
        set->item[set->count] = copy;
        set->len[set->count] = len;
        set->kind[set->count] = kind;
        set->count++;
      }
    } else if (!set->full) {
      set->full = HTS_TRUE;
      warn = HTS_TRUE;
    }
  }
  hts_mutexrelease(&set->lock);
  // the log line takes the lock again
  if (warn)
    hts_log_print(opt, LOG_WARNING,
                  "more than %d passwords seen: the log hides the later ones"
                  " only where they stand in a URL",
                  HTS_SECRET_MAX);
  if (warnlong)
    hts_log_print(opt, LOG_WARNING,
                  "a password or token is over %d bytes, so the log hides it"
                  " only in a URL login or a known header",
                  HTS_SECRET_MAXLEN);
}

void hts_secret_add_login(httrackp *opt, const char *url) {
  const char *start, *host, *colon;
  char pass[HTS_SECRET_MAXLEN];

  if (url == NULL)
    return;
  start = login_start(url);
  // up to the first '/', so a password with a raw '?', '#' or '\' is learnt
  host = strrchr_limit(start, '@', strchr(start, '/'));
  if (host == NULL)
    return;
  colon = memchr(start, ':', (size_t) (host - 1 - start));
  if (colon == NULL)
    return;
  hts_secret_add(opt, colon + 1, (size_t) (host - 1 - (colon + 1)),
                 HTS_SECRET_LOGIN);
  // the Basic login is sent unescaped
  if (memchr(colon + 1, '%', (size_t) (host - 1 - (colon + 1))) != NULL &&
      hts_buf_put_span(HTS_BUF(pass), hts_span_between(colon + 1, host - 1))) {
    char plain[HTS_SECRET_MAXLEN + 1];
    const char *const p = unescape_http(plain, sizeof(plain), pass);

    hts_secret_add(opt, p, strlen(p), HTS_SECRET_LOGIN);
  }
}

/* Does a word start at s + i, after the line start, a blank, a quote, '(' or
   '<', or right after "://"? */
static hts_boolean word_start(const char *s, size_t i) {
  return i == 0 || strchr(HTS_BLANKS "\"'(<", s[i - 1]) != NULL ||
                 (i >= 3 && strncmp(s + i - 3, "://", 3) == 0)
             ? HTS_TRUE
             : HTS_FALSE;
}

/* Does the n bytes at s + i stand as a login, as in "SECRET@" or
   "user:SECRET@", whose userinfo starts a word? */
static hts_boolean login_at(const char *s, size_t len, size_t i, size_t n) {
  // schemes whose "name:local@host" is an address, not a login
  static const char *const opaque[] = {"mailto", "news", "sip",
                                       "sips",   "tel",  "xmpp"};
  size_t u = i, k;

  if (i + n >= len || s[i + n] != '@')
    return HTS_FALSE;
  if (i != 0 && s[i - 1] == ':') {
    u = i - 1;
    while (u != 0 && strchr("/ \t\r\n", s[u - 1]) == NULL)
      u--;
    for (k = 0; k < sizeof(opaque) / sizeof(opaque[0]); k++)
      if (strlen(opaque[k]) == i - 1 - u && strfield(s + u, opaque[k]))
        return HTS_FALSE;
  }
  return word_start(s, u);
}

/* Returns the length of the longest secret of set standing at s + i, or 0. A
   URL password counts only as a login, a header token anywhere. */
static size_t secret_at(const hts_secret_set *set, const char *s, size_t len,
                        size_t i) {
  size_t best = 0;
  int k;

  for (k = 0; k < set->count; k++) {
    const size_t n = set->len[k];

    if (n <= best || n > len - i || memcmp(s + i, set->item[k], n) != 0)
      continue;
    if (set->kind[k] == HTS_SECRET_HEADER || login_at(s, len, i, n))
      best = n;
  }
  return best;
}

/* Copies the len bytes at s to out, if not NULL, with each secret of set
   replaced. Returns the length written. */
static size_t secret_scan(const hts_secret_set *set, const char *s, size_t len,
                          char *out) {
  size_t i = 0, o = 0;

  while (i < len) {
    const size_t n = secret_at(set, s, len, i);

    if (n != 0) {
      if (out != NULL)
        memcpy(out + o, "***", 3);
      o += 3;
      i += n;
    } else {
      if (out != NULL)
        out[o] = s[i];
      o++;
      i++;
    }
  }
  return o;
}

/* Returns a copy of the len bytes at s with each secret of opt as ***,
   allocated with malloct, its length in *outlen. NULL when no secret is
   known, or *outlen set to 0 too when out of memory. */
static char *secret_mask(httrackp *opt, const char *s, size_t len,
                         size_t *outlen) {
  hts_secret_set *const set =
      opt != NULL ? (hts_secret_set *) opt->secrets : NULL;
  char *out = NULL;

  *outlen = 1;
  if (set == NULL)
    return NULL;
  hts_mutexlock(&set->lock);
  if (set->count != 0) {
    *outlen = secret_scan(set, s, len, NULL);
    out = malloct(*outlen + 1);
    if (out != NULL) {
      secret_scan(set, s, len, out);
      out[*outlen] = '\0';
    } else
      *outlen = 0;
  }
  hts_mutexrelease(&set->lock);
  return out;
}

/* Writes the len bytes at s to fp, with each secret of opt as ***. */
static void hts_secret_fwrite(httrackp *opt, FILE *fp, const char *s,
                              size_t len) {
  size_t n;
  char *masked = secret_mask(opt, s, len, &n);

  if (masked != NULL)
    fwrite(masked, 1, n, fp);
  else if (n == 0)
    fputs("<out of memory>", fp);
  else
    fwrite(s, 1, len, fp);
  freet(masked);
}

/* Does opt know a secret? */
static hts_boolean secrets_known(httrackp *opt) {
  hts_secret_set *const set =
      opt != NULL ? (hts_secret_set *) opt->secrets : NULL;
  hts_boolean known;

  if (set == NULL)
    return HTS_FALSE;
  hts_mutexlock(&set->lock);
  known = set->count != 0 ? HTS_TRUE : HTS_FALSE;
  hts_mutexrelease(&set->lock);
  return known;
}

/* vfprintf() to fp, with each secret of opt as ***. */
void secret_vfprintf(httrackp *opt, FILE *fp,
                     HTS_FORMAT_STRING const char *format, va_list args) {
  va_list args_copy;
  int n;
  char *line;

  if (!secrets_known(opt)) {
    (void) vfprintf(fp, format, args);
    return;
  }
  va_copy(args_copy, args);
  n = vsnprintf(NULL, 0, format, args_copy);
  va_end(args_copy);
  if (n < 0)
    return;
  line = malloct((size_t) n + 1);
  if (line == NULL) {
    fputs("<out of memory>", fp);
    return;
  }
  (void) vsnprintf(line, (size_t) n + 1, format, args);
  hts_secret_fwrite(opt, fp, line, (size_t) n);
  freet(line);
}

void hts_secret_fprintf(httrackp *opt, FILE *fp,
                        HTS_FORMAT_STRING const char *format, ...) {
  va_list args;

  va_start(args, format);
  secret_vfprintf(opt, fp, format, args);
  va_end(args);
}

HTSEXT_API void hts_hide_secrets(httrackp *opt, char *s, size_t size) {
  size_t n;
  char *masked;

  if (size == 0)
    return;
  masked = secret_mask(opt, s, strlen(s), &n);
  if (masked != NULL) {
    strclipbuff(s, size, masked);
  } else if (n == 0)
    s[0] = '\0';
  freet(masked);
}

void fprint_redacted(FILE *fp, size_t len) {
  fprintf(fp, "<redacted, %lu bytes>", (unsigned long) len);
}

/* Only the -%H log hides these fields, because they carry logins. */
static const char *const secret_fields[] = {HTS_SPAN_WORDS(
    "Authorization", "Proxy-Authorization", "Cookie", "Set-Cookie")};

/* The -%H log hides the login of the URL these fields carry. */
static const char *const url_fields[] = {
    HTS_SPAN_WORDS("Location", "Content-Location", "Referer")};

/* Writes the line buff, len bytes long, with the login of its URL, from keep
   up to end, masked. Returns HTS_FALSE, writing nothing, when out of memory. */
static hts_boolean fprint_url_span(httrackp *opt, FILE *fp, const char *buff,
                                   size_t len, size_t keep, size_t end) {
  char *line = malloct(2 * end + 8);

  if (line == NULL)
    return HTS_FALSE;
  memcpy(line, buff, end);
  line[end] = '\0';
  {
    const char *const url = url_nologin_r(line + end + 1, end + 7, line + keep);

    hts_secret_fwrite(opt, fp, line, keep);
    hts_secret_fwrite(opt, fp, url, strlen(url));
    hts_secret_fwrite(opt, fp, buff + end, len - end);
  }
  freet(line);
  return HTS_TRUE;
}

/* Writes the header line buff, len bytes long, with the login of its URL
   value masked. Returns HTS_FALSE, writing nothing, when out of memory. */
static hts_boolean fprint_url_field(httrackp *opt, FILE *fp, const char *buff,
                                    size_t len) {
  const char *const colon = memchr(buff, ':', len);
  size_t keep = colon != NULL ? (size_t) (colon - buff) + 1 : len;

  while (keep < len && (buff[keep] == ' ' || buff[keep] == '\t'))
    keep++;
  return fprint_url_span(opt, fp, buff, len, keep, len);
}

/* Writes the request line buff, len bytes long, with the login of its target
   masked, as a proxied ftp:// target carries one. Returns HTS_FALSE, writing
   nothing, when buff is no "METHOD target HTTP/" line or out of memory. */
static hts_boolean fprint_request_line(httrackp *opt, FILE *fp,
                                       const char *buff, size_t len) {
  const char *const sp = memchr(buff, ' ', len);
  const char *version;

  if (sp == NULL)
    return HTS_FALSE;
  version = memchr(sp + 1, ' ', len - (size_t) (sp + 1 - buff));
  if (version == NULL || (size_t) (buff + len - version) < 6 ||
      strncmp(version, " HTTP/", 6) != 0)
    return HTS_FALSE;
  return fprint_url_span(opt, fp, buff, len, (size_t) (sp + 1 - buff),
                         (size_t) (version - buff));
}

/* Finds the secret of the userinfo at the authority a: the password after the
   first ':', or a user alone, up to the last '@' before the first '/'. That
   reaches past the '?', '#' or '\' where jump_identification_const() stops,
   so a password a user typed with one is masked too. A token also ends at a
   blank. Returns HTS_FALSE when there is no userinfo. */
static hts_boolean userinfo_password(const char *a, hts_boolean token,
                                     const char **pass, const char **at) {
  const char *end = a + strcspn(a, token ? "/ \t\r\n" : "/");
  const char *colon;

  while (end > a && end[-1] != '@')
    end--;
  if (end == a)
    return HTS_FALSE;
  colon = memchr(a, ':', (size_t) (end - 1 - a));
  *pass = colon != NULL ? colon + 1 : a;
  *at = end - 1;
  return HTS_TRUE;
}

/* The masked copy: dst (NULL to only count) holds o bytes, and the source is
   copied up to p. */
typedef struct {
  char *dst;
  size_t o;
  const char *p;
} mask_out;

static void mask_copy(mask_out *m, const char *src, size_t n) {
  if (m->dst != NULL)
    memcpy(m->dst + m->o, src, n);
  m->o += n;
}

/* Masks the login secret of the address at a, if any, and moves *i past it. */
static void mask_at(mask_out *m, const char *a, hts_boolean token,
                    const char **i) {
  const char *pass, *at;

  if (!userinfo_password(jump_protocol_const(a), token, &pass, &at))
    return;
  mask_copy(m, m->p, (size_t) (pass - m->p));
  mask_copy(m, "***", 3);
  m->p = *i = at;
}

/* Copies s to dst, or only counts when dst is NULL, with the login secret of
   each address written as ***. Returns the length. */
static size_t mask_passwords(const char *s, char *dst, hts_mask_mode mode) {
  mask_out m = {dst, 0, s};
  const char *i = s;

  if (mode == HTS_MASK_ADDR)
    mask_at(&m, s, HTS_FALSE, &i);
  else
    for (; *i != '\0'; i++)
      // a token that is no filter is a start URL
      if ((i == s || strchr(HTS_BLANKS, i[-1]) != NULL) &&
          strchr(HTS_BLANKS "+-", *i) == NULL)
        mask_at(&m, i, HTS_TRUE, &i);
  mask_copy(&m, m.p, strlen(m.p) + 1);
  return m.o - 1;
}

char *hts_mask_passwords(const char *s, hts_mask_mode mode) {
  const size_t len = mask_passwords(s, NULL, mode);
  char *const dst = (char *) malloct(len + 1);

  if (dst != NULL)
    mask_passwords(s, dst, mode);
  return dst;
}

/* Where is the first blank of S, or its end? */
static size_t blank_at(hts_span s) {
  size_t n = 0;

  while (n < s.len && s.p[n] != ' ' && s.p[n] != '\t')
    n++;
  return n;
}

/* Does LINE, without its CR or LF, carry a secret field? A name with no colon
   behind it counts, its value then starting after the next blank. A fold or a
   leading ':' takes that path too and never matches. */
static hts_boolean secret_field(hts_span line, hts_span *name,
                                hts_span *value) {
  hts_header_split(line, name, value);
  if (hts_span_empty(*name)) {
    *name = hts_span_at(line.p, blank_at(line));
    *value = hts_span_trim(hts_span_skip(line, name->len), HTS_TABORSPACES,
                           HTS_TABORSPACES);
  }
  return hts_span_index_ci(*name, secret_fields) >= 0;
}

void hts_secret_add_headers(httrackp *opt, const char *headers) {
  const char *p = headers;

  while (p != NULL && *p != '\0') {
    const hts_span line = hts_span_at(p, strcspn(p, "\r\n"));
    hts_span name, value;

    // a cookie value is often a plain word, and -%H hides the whole header
    if (secret_field(line, &name, &value) &&
        !hts_span_eq_ci(name, hts_span_of("Cookie")) &&
        !hts_span_eq_ci(name, hts_span_of("Set-Cookie"))) {
      // the whole value, then the credentials after the scheme ("Basic TOKEN")
      const hts_span cred = hts_span_trim(hts_span_skip(value, blank_at(value)),
                                          HTS_TABORSPACES, "");

      hts_secret_add(opt, value.p, value.len, HTS_SECRET_HEADER);
      hts_secret_add(opt, cred.p, cred.len, HTS_SECRET_HEADER);
    }
    p = line.p + line.len;
    p += strspn(p, "\r\n");
  }
}

void fprintfio(FILE *fp, const char *buff, const char *prefix, httrackp *opt) {
  const char *const start = buff;
  hts_boolean nl = HTS_TRUE;
  hts_boolean secret = HTS_FALSE;

  while (*buff != '\0') {
    const size_t len = strcspn(buff, "\r\n");

    if (len != 0) {
      hts_span name, value;
      const hts_boolean field =
          secret_field(hts_span_at(buff, len), &name, &value);
      size_t keep;

      /* a folded line continues the value above it */
      if (*buff != ' ' && *buff != '\t')
        secret = field;
      keep = secret ? (size_t) (value.p - buff) : len;
      if (nl)
        fprintf(fp, "%s", prefix);
      nl = HTS_FALSE;
      if (!(buff == start && fprint_request_line(opt, fp, buff, len)) &&
          (secret || *buff == ' ' || *buff == '\t' ||
           !header_name_in(buff, url_fields) ||
           !fprint_url_field(opt, fp, buff, len))) {
        hts_secret_fwrite(opt, fp, buff, keep);
        if (keep < len)
          fprint_redacted(fp, len - keep);
      }
    }
    buff += len;
    if (*buff == '\n') {
      fprintf(fp, "\r\n");
      nl = HTS_TRUE;
    }
    if (*buff != '\0')
      buff++;
  }
}

/* See htslib.h. */
const char *hts_header_shown(httrackp *opt, const char *line, char *dst,
                             size_t size) {
  const size_t len = strcspn(line, "\r\n");
  const char *const colon = memchr(line, ':', len);
  hts_span name, field;
  const hts_boolean secret =
      secret_field(hts_span_at(line, len), &name, &field);
  size_t keep = secret          ? (size_t) (field.p - line)
                : colon != NULL ? (size_t) (colon - line) + 1
                                : len;
  char value[HTS_URLMAXSIZE * 2];
  size_t n;

  while (keep < len && (line[keep] == ' ' || line[keep] == '\t'))
    keep++;
  n = keep < size ? keep : size - 1;
  memcpy(dst, line, n);
  dst[n] = '\0';
  if (keep < len && secret) {
    // a clipped marker still hides the value
    if (!slprintfbuff(dst + n, size - n, "<redacted, %lu bytes>",
                      (unsigned long) (len - keep)))
      (void) strclipbuff(dst + n, size - n, "<redacted>");
  } else if (keep < len) {
    hts_buf_put_span_clip(HTS_BUF(value), hts_span_at(line + keep, len - keep));
    if (header_name_in(line, url_fields)) {
      char masked[HTS_URLMAXSIZE * 2];

      (void) strclipbuff(dst + n, size - n,
                         url_nologin_r(HTS_SIZED(masked), value));
    } else {
      (void) strclipbuff(dst + n, size - n, value);
    }
  }
  hts_hide_secrets(opt, dst, size);
  return dst;
}
