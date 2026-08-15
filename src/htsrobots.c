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
/* File: httrack.c subroutines:                                 */
/*       robots.txt (website robot file)                        */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Internal engine bytecode */
#define HTS_INTERNAL_BYTECODE

/* specific definitions */
#include "htscore.h"
#include "htsbase.h"
#include "htslib.h"
/* END specific definitions */

#include "htsrobots.h"

// -- robots --

/* RFC 9309 path-prefix match; '*' any run, '$' anchors end; linear. */
static hts_boolean robots_pattern_match(const char *pattern, const char *path) {
  size_t patlen = strlen(pattern);
  hts_boolean anchored = HTS_FALSE;
  const char *p, *pend, *s;
  const char *star = NULL, *star_s = NULL;

  if (patlen > 0 && pattern[patlen - 1] == '$') {
    anchored = HTS_TRUE;
    patlen--;
  }
  p = pattern;
  pend = pattern + patlen;
  s = path;
  while (*s != '\0') {
    if (p == pend) {
      if (!anchored)
        return HTS_TRUE;  // prefix matched
      if (star != NULL) { // anchored: '*' must eat the rest
        p = star + 1;
        s = ++star_s;
        continue;
      }
      return HTS_FALSE;
    }
    if (*p == '*') {
      star = p++;
      star_s = s;
    } else if (*p == *s) {
      p++;
      s++;
    } else if (star != NULL) {
      p = star + 1;
      s = ++star_s;
    } else {
      return HTS_FALSE;
    }
  }
  while (p < pend && *p == '*')
    p++;
  return (p == pend) ? HTS_TRUE : HTS_FALSE;
}

/* fil="": is a rule set already recorded for this host? */
int checkrobots(robots_wizard * robots, const char *adr, const char *fil) {
  while(robots) {
    if (robots->adr != NULL && strfield2(robots->adr, adr)) {
      if (fil[0]) {
        /* RFC 9309: longest pattern wins, Allow beats Disallow on ties. */
        int ptr = 0;
        char line[HTS_ROBOTS_LINE_SIZE + 2];
        size_t toklen = robots->token != NULL ? strlen(robots->token) : 0;
        size_t best_len = 0;
        hts_boolean matched = HTS_FALSE;
        hts_boolean best_allow = HTS_FALSE;

        while (ptr < (int) toklen) {
          ptr += binput(robots->token + ptr, line, sizeof(line) - 1);
          if (line[0] != 'A' && line[0] != 'D')
            continue;
          {
            const hts_boolean is_allow =
                (line[0] == 'A') ? HTS_TRUE : HTS_FALSE;
            const char *pat = line + 1;

            if (robots_pattern_match(pat, fil)) {
              const size_t len = strlen(pat);

              if (!matched || len > best_len || (len == best_len && is_allow)) {
                matched = HTS_TRUE;
                best_len = len;
                best_allow = is_allow;
              }
            }
          }
        }
        if (matched && !best_allow)
          return -1; // forbidden
      } else {
        return -1;
      }
    }
    robots = robots->next;
  }
  return 0;
}

/* Rule blob under construction: grown to what the site wrote, up to the cap. */
typedef struct {
  char *data;
  size_t len;
  size_t capa;
} robots_blob;

/* Append "<marker><pattern>\n"; HTS_FALSE when the cap or the heap says no. */
static hts_boolean robots_blob_add(robots_blob *blob, char marker,
                                   const char *pat) {
  const size_t patlen = strlen(pat);
  const size_t need = patlen + 2; // marker + '\n'

  // overflow-safe: len <= HTS_ROBOTS_MAX_TOKEN_SIZE
  if (need > HTS_ROBOTS_MAX_TOKEN_SIZE - blob->len)
    return HTS_FALSE;
  if (need + 1 > blob->capa - blob->len) {
    size_t capa = blob->capa != 0 ? blob->capa : 512;
    char *data;

    while (need + 1 > capa - blob->len)
      capa *= 2;
    data = (char *) realloct(blob->data, capa);
    if (data == NULL)
      return HTS_FALSE;
    blob->data = data;
    blob->capa = capa;
  }
  blob->data[blob->len++] = marker;
  memcpy(blob->data + blob->len, pat, patlen);
  blob->len += patlen;
  blob->data[blob->len++] = '\n';
  blob->data[blob->len] = '\0';
  return HTS_TRUE;
}

void robots_parse(httrackp *opt, robots_wizard *robots, const char *adr,
                  const char *body, size_t bodysize, char *info,
                  size_t infosize, hts_boolean keep_root_disallow,
                  char *sitemaps, size_t sitemapsize) {
  size_t bptr = 0;
  int record = 0;
  int ndropped = 0;
  char BIGSTK line[HTS_ROBOTS_LINE_SIZE];
  char dropped[128]; // first rule we could not honour, for the log
  robots_blob blob;

  blob.data = NULL;
  blob.len = 0;
  blob.capa = 0;
  dropped[0] = '\0';
  if (info != NULL && infosize > 0)
    info[0] = '\0';
  if (sitemaps != NULL && sitemapsize > 0)
    sitemaps[0] = '\0';
#if DEBUG_ROBOTS
  printf("robots.txt dump:\n%s\n", body);
#endif
  while (bptr < bodysize) {
    char *comm;
    int llen;
    hts_boolean cut;

    bptr += binput(body + bptr, line, sizeof(line) - 2);
    /* binput stops at the limit and resumes mid-line, so what follows is the
       tail of this record and not a record of its own. */
    cut = (strlen(line) >= sizeof(line) - 2) ? HTS_TRUE : HTS_FALSE;
    comm = strchr(line, '#'); // strip comment
    if (comm != NULL)
      *comm = '\0';
    llen = (int) strlen(line); // strip trailing spaces
    while (llen > 0 && is_realspace(line[llen - 1])) {
      line[llen - 1] = '\0';
      llen--;
    }
    if (llen < (int) sizeof(line) - 2)
      cut = HTS_FALSE; // a comment or a space ended the value before the limit
    if (sitemaps != NULL && strfield(line, "sitemap:")) {
      // group-independent record (RFC 9309): collected whatever the group
      char *a = line + 8;

      while (is_realspace(*a))
        a++;
      /* A line at the buffer limit was truncated: a half URL is not one. */
      if (strnotempty(a) && !cut &&
          strlen(a) + 2 < sitemapsize - strlen(sitemaps)) {
        strlcatbuff(sitemaps, a, sitemapsize);
        strlcatbuff(sitemaps, "\n", sitemapsize);
      }
    } else if (strfield(line, "user-agent:")) {
      char *a = line + 11;

      while (is_realspace(*a))
        a++;
      if (*a == '*') {
        if (record != 2)
          record = 1; // generic group applies to us
      } else if (strfield(a, "httrack") || strfield(a, "winhttrack") ||
                 strfield(a, "webhttrack")) {
        blob.len = 0; // explicit group: restart capture
        if (blob.data != NULL)
          blob.data[0] = '\0';
        ndropped = 0;
        dropped[0] = '\0';
        if (info != NULL && infosize > 0)
          info[0] = '\0';
        record = 2; // locked to the httrack group
      } else
        record = 0;
    } else if (record) {
      hts_boolean is_allow = strfield(line, "allow:");
      hts_boolean is_disallow = !is_allow && strfield(line, "disallow:");

      if (is_allow || is_disallow) {
        char *a = line + (is_allow ? 6 : 9);

        while (is_realspace(*a))
          a++;
        if (strnotempty(a)) {
          if (is_disallow && !keep_root_disallow && strcmp(a, "/") == 0) {
            // dropped: site-wide disallow ignored by option
          } else {
            /* An Allow cut short by the line buffer is a shorter prefix, so it
               would permit more than the site wrote; a Disallow prefix can only
               forbid more, and is kept. Neither is the rule as written. */
            const hts_boolean kept =
                (cut && is_allow)
                    ? HTS_FALSE
                    : robots_blob_add(&blob, is_allow ? 'A' : 'D', a);

            if (!kept || cut) {
              if (ndropped++ == 0) {
                dropped[0] = '\0'; // clip, never abort: this is remote data
                strlncatbuff(dropped, a, sizeof(dropped), sizeof(dropped) - 1);
              }
            }
            /* info reports what we honour, not what we read. */
            if (kept && is_disallow && info != NULL &&
                strlen(a) + 2 < infosize - strlen(info)) {
              if (strnotempty(info))
                strlcatbuff(info, ", ", infosize);
              strlcatbuff(info, a, infosize);
            }
          }
        }
      }
    }
  }
  if (ndropped != 0)
    hts_log_print(opt, LOG_WARNING,
                  "robots.txt for %s: %d rule(s) not honoured as written, "
                  "starting with '%s' (rules kept up to %d bytes, lines to %d)",
                  adr, ndropped, dropped, (int) HTS_ROBOTS_MAX_TOKEN_SIZE,
                  (int) HTS_ROBOTS_LINE_SIZE);
  if (blob.len != 0)
    checkrobots_set(robots, adr, blob.data);
  freet(blob.data);
}

int checkrobots_set(robots_wizard *robots, const char *adr, const char *data) {
  while(robots) {
    if (robots->adr != NULL && strfield2(robots->adr, adr)) { // entry exists
      char *const token = strdupt(data);

      if (token == NULL)
        return 0;
      freet(robots->token);
      robots->token = token;
#if DEBUG_ROBOTS
      printf("robots.txt: set %s to %s\n", adr, data);
#endif
      return -1;
    } else if (!robots->next) {
      robots_wizard *node = (robots_wizard *) calloct(1, sizeof(robots_wizard));

      if (node == NULL)
        return 0;
      node->adr = strdupt(adr);
      node->token = strdupt(data);
      if (node->adr == NULL || node->token == NULL) {
        freet(node->adr);
        freet(node->token);
        freet(node);
        return 0;
      }
      robots->next = node;
#if DEBUG_ROBOTS
      printf("robots.txt: new set %s to %s\n", adr, data);
#endif
      return -1;
    }
    robots = robots->next;
  }
  return 0;
}
void checkrobots_free(robots_wizard * robots) {
  if (robots->next) {
    checkrobots_free(robots->next);
    freet(robots->next);
    robots->next = NULL;
  }
  freet(robots->adr);
  freet(robots->token);
}

// -- robots --
