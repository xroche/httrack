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

Please visit our Website: http://www.httrack.com
*/

/* Fuzz the robots.txt handler (htsrobots.c): robots_parse() reads the body a
   server hands back for /robots.txt and keeps the HTTrack group's rules;
   checkrobots() then matches every fetch path against them with the RFC 9309
   wildcard matcher ('*' any run, '$' anchors the end). The input's first byte
   picks the parse variants (keep_root_disallow), the rest is the body, and
   every body line doubles as a matched path, plus a probe host for the
   per-host list walk. The raw rule-store path (checkrobots_set) runs on the
   same bytes. The body is attacker-written on every mirror, so memory safety
   in the parser, the token store and the matcher is the point. */
#include "fuzz.h"

#include "htscore.h"
#include "htsrobots.h"

static unsigned long robots_sink;

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  robots_wizard robots;
  robots_wizard raw;
  char *body;
  char info[512];
  char sitemaps[512];
  const char *p;
  hts_boolean keep_root;

  if (size < 2)
    return 0;
  keep_root = (data[0] & 1) ? HTS_TRUE : HTS_FALSE;
  data++;
  size--;
  body = fuzz_strdup(data, size);

  memset(&robots, 0, sizeof(robots));
  info[0] = '\0';
  sitemaps[0] = '\0';
  robots_parse(NULL, &robots, "fuzz.example", body, size, info, sizeof(info),
               keep_root, sitemaps, sizeof(sitemaps));
  robots_sink += (unsigned long) strlen(info);
  robots_sink += (unsigned long) strlen(sitemaps);

  /* Every body line doubles as a fetch path for the wildcard matcher, and a
     probe host walks the per-host list. */
  p = body;
  while (p != NULL && *p != '\0') {
    char *nl = strchr(p, '\n');

    if (nl != NULL)
      *nl = '\0';
    if (*p != '\0') {
      robots_sink += (unsigned long) checkrobots(&robots, "fuzz.example", p);
      robots_sink += (unsigned long) checkrobots(&robots, "probe.example", p);
    }
    p = (nl != NULL) ? nl + 1 : NULL;
  }

  /* The raw store path: marker + pattern lines, exactly what robots_parse
     feeds checkrobots_set. */
  memset(&raw, 0, sizeof(raw));
  if (checkrobots_set(&raw, "raw.example", body) == -1) {
    robots_sink += (unsigned long) checkrobots(&raw, "raw.example", "/a/b");
    robots_sink += (unsigned long) checkrobots(&raw, "raw.example", "/");
  }
  checkrobots_free(&raw);
  checkrobots_free(&robots);
  freet(body);
  return 0;
}
