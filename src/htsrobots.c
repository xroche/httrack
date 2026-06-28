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

// fil="" : vérifier si règle déja enregistrée
int checkrobots(robots_wizard * robots, const char *adr, const char *fil) {
  while(robots) {
    if (strfield2(robots->adr, adr)) {
      if (fil[0]) {
        /* RFC 9309: longest pattern wins, Allow beats Disallow on ties. */
        int ptr = 0;
        char line[HTS_ROBOTS_TOKEN_SIZE];
        size_t toklen = strlen(robots->token);
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

/* Append "<marker><pattern>\n" to the bounded rule blob if it fits. */
static void robots_blob_add(char *blob, size_t blobsize, char marker,
                            const char *pat) {
  const size_t used = strlen(blob);
  const size_t need = strlen(pat) + 2; // marker + '\n'

  if (need < blobsize - used) { // overflow-safe: used <= blobsize-1
    blob[used] = marker;
    blob[used + 1] = '\0';
    strlcatbuff(blob, pat, blobsize);
    strlcatbuff(blob, "\n", blobsize);
  }
}

void robots_parse(robots_wizard *robots, const char *adr, const char *body,
                  size_t bodysize, char *info, size_t infosize,
                  hts_boolean keep_root_disallow) {
  size_t bptr = 0;
  int record = 0;
  char BIGSTK line[1024];
  char BIGSTK blob[HTS_ROBOTS_TOKEN_SIZE];

  blob[0] = '\0';
  if (info != NULL && infosize > 0)
    info[0] = '\0';
#if DEBUG_ROBOTS
  printf("robots.txt dump:\n%s\n", body);
#endif
  while (bptr < bodysize) {
    char *comm;
    int llen;

    bptr += binput(body + bptr, line, sizeof(line) - 2);
    comm = strchr(line, '#'); // strip comment
    if (comm != NULL)
      *comm = '\0';
    llen = (int) strlen(line); // strip trailing spaces
    while (llen > 0 && is_realspace(line[llen - 1])) {
      line[llen - 1] = '\0';
      llen--;
    }
    if (strfield(line, "user-agent:")) {
      char *a = line + 11;

      while (is_realspace(*a))
        a++;
      if (*a == '*') {
        if (record != 2)
          record = 1; // generic group applies to us
      } else if (strfield(a, "httrack") || strfield(a, "winhttrack") ||
                 strfield(a, "webhttrack")) {
        blob[0] = '\0'; // explicit group: restart capture
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
            robots_blob_add(blob, sizeof(blob), is_allow ? 'A' : 'D', a);
            if (is_disallow && info != NULL &&
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
  if (strnotempty(blob))
    checkrobots_set(robots, adr, blob);
}

int checkrobots_set(robots_wizard * robots, const char *adr, const char *data) {
  if (((int) strlen(adr)) >= sizeof(robots->adr) - 2)
    return 0;
  if (((int) strlen(data)) >= sizeof(robots->token) - 2)
    return 0;
  while(robots) {
    if (strfield2(robots->adr, adr)) {  // entrée existe
      strcpybuff(robots->token, data);
#if DEBUG_ROBOTS
      printf("robots.txt: set %s to %s\n", adr, data);
#endif
      return -1;
    } else if (!robots->next) {
      robots->next = (robots_wizard *) calloct(1, sizeof(robots_wizard));
      if (robots->next) {
        robots->next->next = NULL;
        strcpybuff(robots->next->adr, adr);
        strcpybuff(robots->next->token, data);
#if DEBUG_ROBOTS
        printf("robots.txt: new set %s to %s\n", adr, data);
#endif
      }
#if DEBUG_ROBOTS
      else
        printf("malloc error!!\n");
#endif
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
}

// -- robots --
