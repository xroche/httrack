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

/* specific definitions */
#include "htscore.h"
#include "htsbase.h"
#include "htslib.h"
/* END specific definitions */

#include "htsrobots.h"

// -- robots --

/* Leftmost match of seg in hay by KMP, O(hay + seg); -1 if absent. `fail`
   holds seg.len entries. */
static long robots_find(hts_span seg, hts_span hay, unsigned int *fail) {
  size_t i, k = 0;

  if (seg.len == 0)
    return 0;
  fail[0] = 0;
  for (i = 1; i < seg.len; i++) {
    while (k > 0 && seg.p[i] != seg.p[k])
      k = fail[k - 1];
    if (seg.p[i] == seg.p[k])
      k++;
    fail[i] = (unsigned int) k;
  }
  for (i = 0, k = 0; i < hay.len; i++) {
    while (k > 0 && hay.p[i] != seg.p[k])
      k = fail[k - 1];
    if (hay.p[i] == seg.p[k])
      k++;
    if (k == seg.len)
      return (long) (i + 1 - seg.len);
  }
  return -1;
}

/* RFC 9309 path-prefix match, linear: each '*'-separated piece is searched
 * once. */
static hts_boolean robots_pattern_match(const char *pattern, const char *path) {
  size_t patlen = strlen(pattern);
  const size_t pathlen = strlen(path);
  hts_boolean anchored = HTS_FALSE;
  const char *p, *pend, *star;
  size_t pos;
  unsigned int fail[HTS_ROBOTS_LINE_SIZE + 2];

  if (patlen > 0 && pattern[patlen - 1] == '$') {
    anchored = HTS_TRUE;
    patlen--;
  }
  if (patlen > HTS_ROBOTS_LINE_SIZE + 1)
    return HTS_FALSE; // cannot come from a stored rule
  p = pattern;
  pend = pattern + patlen;
  star = memchr(p, '*', patlen);

  /* First piece is a literal prefix of the path. */
  {
    const size_t seglen = (star != NULL) ? (size_t) (star - p) : patlen;

    if (seglen > pathlen || memcmp(p, path, seglen) != 0)
      return HTS_FALSE;
    if (star == NULL)
      return (!anchored || seglen == pathlen) ? HTS_TRUE : HTS_FALSE;
    pos = seglen;
    p = star + 1;
  }
  /* Then each piece after a '*' (the last one is a suffix when anchored). */
  while (p < pend) {
    const char *next = memchr(p, '*', (size_t) (pend - p));
    const size_t seglen =
        (next != NULL) ? (size_t) (next - p) : (size_t) (pend - p);

    if (next == NULL && anchored) {
      return (seglen <= pathlen - pos &&
              memcmp(p, path + pathlen - seglen, seglen) == 0)
                 ? HTS_TRUE
                 : HTS_FALSE;
    }
    if (seglen > 0) {
      const long at = robots_find(hts_span_at(p, seglen),
                                  hts_span_at(path + pos, pathlen - pos), fail);

      if (at < 0)
        return HTS_FALSE;
      pos += (size_t) at + seglen;
    }
    if (next == NULL)
      break;
    p = next + 1;
  }
  return HTS_TRUE;
}

/* fil="": is a rule set already recorded for this host? */
int checkrobots(const robots_wizard *robots, const char *adr, const char *fil) {
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
          int adv;
          const hts_boolean cut = binput_line(
              robots->token + ptr, robots->token + toklen, HTS_BUF(line), &adv);

          ptr += adv;
          /* a cut rule is a prefix, which matches more than the site wrote */
          if (cut || (line[0] != 'A' && line[0] != 'D'))
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

/* Append "<marker><pattern>\n" to the rule blob; HTS_FALSE past the cap. */
static hts_boolean robots_rule_add(String *blob, char marker, const char *pat) {
  const size_t patlen = strlen(pat);

  // overflow-safe: the blob never passes the cap
  if (patlen + 2 > HTS_ROBOTS_MAX_TOKEN_SIZE - StringLength(*blob))
    return HTS_FALSE;
  StringMemcat(*blob, &marker, 1);
  StringMemcat(*blob, pat, patlen);
  StringMemcat(*blob, "\n", 1);
  return HTS_TRUE;
}

void robots_parse(httrackp *opt, robots_wizard *robots, const char *adr,
                  hts_span body, hts_buf info, hts_boolean keep_root_disallow,
                  hts_buf sitemaps) {
  size_t bptr = 0;
  int record = 0;
  hts_boolean in_agents = HTS_FALSE;     // inside the group's user-agent run
  hts_boolean have_specific = HTS_FALSE; // a group named us, so no '*' applies
  int ndropped = 0;
  char BIGSTK line[HTS_ROBOTS_LINE_SIZE];
  char dropped[128]; // first rule we could not honour, for the log
  String blob = STRING_EMPTY;

  dropped[0] = '\0';
  if (info.size > 0)
    info.p[0] = '\0';
  if (sitemaps.size > 0)
    sitemaps.p[0] = '\0';
  while (bptr < body.len) {
    char *comm;
    int adv;
    hts_boolean cut;

    cut = binput_line(body.p + bptr, body.p + body.len,
                      HTS_BUF_N(line, sizeof(line) - 1), &adv);
    bptr += (size_t) adv;
    comm = strchr(line, '#'); // strip comment
    if (comm != NULL) {
      *comm = '\0';
      cut = HTS_FALSE; // the comment ended the value inside the buffer
    }
    hts_rtrim(line, HTS_REALSPACES);
    if (sitemaps.size > 0 && strfield(line, "sitemap:")) {
      // Sitemap belongs to no group (RFC 9309), so always collect it
      char *a = line + strfield(line, "sitemap:");

      a += strspn(a, HTS_REALSPACES);
      /* A line at the buffer limit was truncated: a half URL is not one. */
      if (strnotempty(a) && !cut &&
          strlen(a) + 2 < sitemaps.size - strlen(sitemaps.p)) {
        hts_buf_cat(sitemaps, a);
        hts_buf_cat(sitemaps, "\n");
      }
    } else if (strfield(line, "user-agent:")) {
      char *a = line + strfield(line, "user-agent:");

      a += strspn(a, HTS_REALSPACES);
      /* RFC 9309 2.2.1: consecutive user-agent lines name one group, and the
         first one after a rule opens a new group. */
      if (!in_agents) {
        record = 0;
        in_agents = HTS_TRUE;
      }
      if (*a == '*') {
        if (!have_specific && record == 0)
          record = 1; // generic group, taken only while none named us
      } else if (strfield(a, "httrack") || strfield(a, "winhttrack") ||
                 strfield(a, "webhttrack")) {
        if (!have_specific) {
          StringClear(blob); // the generic rules read so far are not ours
          ndropped = 0;
          dropped[0] = '\0';
          if (info.size > 0)
            info.p[0] = '\0';
          have_specific = HTS_TRUE;
        }
        record = 2; // locked to the httrack groups
      }
    } else if (strfield(line, "allow:") || strfield(line, "disallow:")) {
      const hts_boolean is_allow = strfield(line, "allow:") != 0;
      const hts_boolean is_disallow = !is_allow;
      char *a = line + strfield(line, is_allow ? "allow:" : "disallow:");

      in_agents = HTS_FALSE; // a rule closes the group's user-agent lines
      a += strspn(a, HTS_REALSPACES);
      if (record && strnotempty(a)) {
        if (is_disallow && !keep_root_disallow && strcmp(a, "/") == 0) {
          // dropped: site-wide disallow ignored by option
        } else {
          /* Drop a cut Allow, which would permit more than the site wrote.
             Keep a cut Disallow, which can only forbid more. */
          const hts_boolean kept =
              (cut && is_allow)
                  ? HTS_FALSE
                  : robots_rule_add(&blob, is_allow ? 'A' : 'D', a);

          if (!kept || cut) {
            if (ndropped++ == 0) {
              // clip, never abort: this is remote data
              strclipbuff(dropped, sizeof(dropped), a);
            }
          }
          /* info lists only the rules we honour. */
          if (kept && is_disallow && info.size > 0 &&
              strlen(a) + 2 < info.size - strlen(info.p)) {
            if (strnotempty(info.p))
              hts_buf_cat(info, ", ");
            hts_buf_cat(info, a);
          }
        }
      }
    }
  }
  if (ndropped != 0)
    hts_log_print(opt, LOG_WARNING,
                  "robots.txt for %s: %d rule(s) not honoured as written, "
                  "starting with '%s' (rules kept up to %d bytes, lines to %d)",
                  adr_nologin(opt, adr), ndropped, dropped,
                  (int) HTS_ROBOTS_MAX_TOKEN_SIZE, (int) HTS_ROBOTS_LINE_SIZE);
  if (StringNotEmpty(blob) && !checkrobots_set(robots, adr, StringBuff(blob)))
    hts_log_print(opt, LOG_WARNING,
                  "No memory to store robots.txt rules for %s",
                  adr_nologin(opt, adr));
  StringFree(blob);
}

hts_boolean checkrobots_set(robots_wizard *robots, const char *adr,
                            const char *data) {
  while(robots) {
    if (robots->adr != NULL && strfield2(robots->adr, adr)) {
      char *const token = strdupt(data);

      if (token == NULL)
        return HTS_FALSE;
      freet(robots->token);
      robots->token = token;
      return HTS_TRUE;
    } else if (!robots->next) {
      robots_wizard *node = (robots_wizard *) calloct(1, sizeof(robots_wizard));

      if (node == NULL)
        return HTS_FALSE;
      node->adr = strdupt(adr);
      node->token = strdupt(data);
      if (node->adr == NULL || node->token == NULL) {
        freet(node->adr);
        freet(node->token);
        freet(node);
        return HTS_FALSE;
      }
      robots->next = node;
      return HTS_TRUE;
    }
    robots = robots->next;
  }
  return HTS_FALSE;
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
