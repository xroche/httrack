/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) 1998-2017 Xavier Roche and other contributors

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

Important notes:

- We hereby ask people using this source NOT to use it in purpose of grabbing
emails addresses, or collecting any other private information on persons.
This would disgrace our work, and spoil the many hours we spent on it.

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

/* Rules are stored one per line, each prefixed by its kind:
     "A<pattern>\n"  Allow
     "D<pattern>\n"  Disallow
   '<pattern>' is a robots.txt path pattern: '*' matches any sequence of
   characters, and a trailing '$' anchors the match to the end of the path.
   See RFC 9309 section 2.2. */
#define ROBOTS_RULE_ALLOW    'A'
#define ROBOTS_RULE_DISALLOW 'D'

/* Match 'pat' (pat_len octets, '*' being a wildcard) against 'str'.
   When 'anchored' the pattern must consume the whole of 'str' ; otherwise
   matching any prefix of 'str' is enough, which is the implicit trailing
   '*' every unanchored robots.txt pattern carries.
   Greedy matcher that backtracks to the last star, so a pathological
   pattern costs O(pattern * path) rather than blowing up. */
static int robots_glob(const char *pat, size_t pat_len, const char *str,
                       int anchored) {
  const char *const pat_end = pat + pat_len;
  const char *star = NULL;
  const char *str_at_star = NULL;
  const char *p = pat;
  const char *s = str;

  for(;;) {
    if (p == pat_end) {
      if (!anchored || *s == '\0') {
        return 1;
      }
    } else if (*p == '*') {
      /* remember where to resume if the rest fails to match */
      star = ++p;
      str_at_star = s;
      continue;
    } else if (*s != '\0' && *p == *s) {
      p++;
      s++;
      continue;
    }
    /* no match here: give the last star one more character to swallow */
    if (star != NULL && *str_at_star != '\0') {
      p = star;
      s = ++str_at_star;
      continue;
    }
    return 0;
  }
}

/* Does the robots.txt path pattern 'pattern' (pattern_len octets) apply to
   'path'? A trailing '$' anchors the match to the end of the path. */
static int robots_pattern_match(const char *pattern, size_t pattern_len,
                                const char *path) {
  if (pattern_len > 0 && pattern[pattern_len - 1] == '$') {
    return robots_glob(pattern, pattern_len - 1, path, 1);
  }
  return robots_glob(pattern, pattern_len, path, 0);
}

/* Is 'fil' forbidden by the rules in 'rules'?
   Per RFC 9309 section 2.2.2 the most specific rule wins, specificity being
   the octet length of the pattern, and Allow wins a tie. A path no rule
   matches is allowed. */
static int robots_rules_forbid(const char *rules, const char *fil) {
  const char *line = rules;
  size_t best_len = 0;
  int best_is_allow = 0;
  int found = 0;

  while(line != NULL && *line != '\0') {
    const char *const eol = strchr(line, '\n');
    const size_t line_len = eol != NULL
      ? (size_t) (eol - line) : strlen(line);

    if (line_len > 1) {
      const char kind = line[0];
      const char *const pattern = line + 1;
      const size_t pattern_len = line_len - 1;

      if ((kind == ROBOTS_RULE_ALLOW || kind == ROBOTS_RULE_DISALLOW)
          && robots_pattern_match(pattern, pattern_len, fil)) {
        /* longer pattern wins ; on a tie, Allow wins */
        if (!found || pattern_len > best_len
            || (pattern_len == best_len && kind == ROBOTS_RULE_ALLOW)) {
          best_len = pattern_len;
          best_is_allow = (kind == ROBOTS_RULE_ALLOW);
          found = 1;
        }
      }
    }
    line = eol != NULL ? eol + 1 : NULL;
  }

  return (found && !best_is_allow) ? -1 : 0;
}

// -- robots --

// fil="" : vérifier si règle déja enregistrée
int checkrobots(robots_wizard * robots, const char *adr, const char *fil) {
  while(robots) {
    if (strfield2(robots->adr, adr)) {
      if (fil[0]) {
        if (robots->rules != NULL && robots->rules[0] != '\0') {
          const int forbidden = robots_rules_forbid(robots->rules, fil);

          if (forbidden != 0) {
            return forbidden;
          }
        }
      } else {
        return -1;
      }
    }
    robots = robots->next;
  }
  return 0;
}

int checkrobots_set(robots_wizard * robots, const char *adr, const char *data) {
  if (((int) strlen(adr)) >= (int) sizeof(robots->adr) - 2)
    return 0;
  while(robots) {
    if (strfield2(robots->adr, adr)) {  // entrée existe
      char *const copy = strdupt(data);

      if (copy == NULL) {
        return 0;
      }
      freet(robots->rules);
      robots->rules = copy;
#if DEBUG_ROBOTS
      printf("robots.txt: set %s to %s\n", adr, data);
#endif
      return 1;
      return -1;
    } else if (!robots->next) {
      robots->next = (robots_wizard *) calloct(1, sizeof(robots_wizard));
      if (robots->next) {
        robots->next->next = NULL;
        strcpybuff(robots->next->adr, adr);
        robots->next->rules = strdupt(data);
        if (robots->next->rules == NULL) {
          freet(robots->next);
          robots->next = NULL;
          return 0;
        }
#if DEBUG_ROBOTS
        printf("robots.txt: new set %s to %s\n", adr, data);
#endif
        return 1;
      } else {
#if DEBUG_ROBOTS
        printf("malloc error!!\n");
#endif
        return 0;
      }
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
  /* note: the head node is owned by the caller (it lives on the stack in
     httpmirror()), but its rules are ours to release. */
  freet(robots->rules);
  robots->rules = NULL;
}
