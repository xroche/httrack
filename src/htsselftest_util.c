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
/* File: htsselftest_util.c                                     */
/*       fixtures several self-test files share                 */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#include "htsselftest_int.h"

/* Decode an argument ("hex:FFD8.." or literal text) into buf. Raw non-UTF-8
   bytes cannot survive a Windows command line, so hostile inputs go as hex. */
size_t st_decode_body(const char *arg, char *buf, size_t size) {
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

/* The wiring hts_mirror() gives the naming path (htscore.c), and the teardown
   it owes: a self-test that leaks it cannot be run under LeakSanitizer. */
void st_mirror_wiring(httrackp *opt, struct_back **sback, hash_struct *hash,
                      hts_boolean backing) {
  *sback = backing ? back_new(opt, opt->maxsoc * 32 + 1024) : NULL;
  hash_init(opt, hash, opt->urlhack);
  hash->liens = (const lien_url *const *const *) &opt->liens;
  opt->hash = hash;
  hts_record_init(opt);
}

/* Everything a cache_back holds, freed. Called again mid-test where the cache
   is reopened read-only, since that drops the handles it already had. */
void st_cache_close(httrackp *opt, cache_back *cache) {
  if (cache->zipOutput != NULL) {
    zipClose(cache->zipOutput, NULL);
    cache->zipOutput = NULL;
  }
  if (cache->zipInput != NULL) {
    unzClose(cache->zipInput);
    cache->zipInput = NULL;
  }
  if (cache->lst != NULL) {
    fclose(cache->lst);
    cache->lst = opt->state.strc.lst = NULL;
  }
  if (cache->txt != NULL) {
    fclose(cache->txt);
    cache->txt = NULL;
  }
  if (cache->hashtable != NULL)
    coucal_delete(&cache->hashtable);
}

/* Torn down in XH_extuninit's order (htscore.c). */
void st_mirror_wiring_free(httrackp *opt, cache_back *cache,
                           struct_back **sback, hash_struct *hash) {
  hts_record_free(opt);
  back_free(sback);
  st_cache_close(opt, cache);
  hash_free(hash);
  opt->hash = NULL; /* it pointed at the caller's stack frame */
}

// mkdir path, tolerating one that already exists.
hts_boolean st_mkdir_at(const char *path, size_t n, const char *who) {
  if (MKDIR(path) != 0 && errno != EEXIST) {
    fprintf(stderr, "%s: mkdir failed at %u chars: %s\n", who, (unsigned) n,
            strerror(errno));
    return HTS_FALSE;
  }
  return HTS_TRUE;
}

// UTF-16 units a UTF-8 run costs, which is what Windows measures MAX_PATH in.
// A stray byte is charged short, and short only ever builds a longer path.
size_t st_utf16_units(const char *s, size_t n) {
  size_t units = 0;

  for (size_t i = 0; i < n; i++) {
    const unsigned char c = (unsigned char) s[i];

    if ((c & 0xC0) == 0x80) {
      continue; /* continuation byte: charged with its lead */
    }
    units += c >= 0xF0 ? 2 : 1; /* outside the BMP is a surrogate pair */
  }
  return units;
}

// Build a tree under dir whose deepest path clears MAX_PATH (260): an optional
// non-ASCII first segment, then ASCII ones (#133). 0 on failure; *baselen is
// where teardown must stop, and is written on every path.
size_t st_mkdeep(char *buf, size_t bufsize, const char *dir, const char *nseg,
                 const char *who, size_t *baselen) {
  // 40-char segments: each under the 255 per-component limit \\?\ can't lift.
  static const char seg[] = "/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  const size_t room = bufsize - (sizeof(seg) + ST_LEAF_ROOM);
  size_t n = (size_t) snprintf(buf, bufsize, "%s", dir);

  assertf(bufsize > sizeof(seg) + ST_LEAF_ROOM);
  if (baselen != NULL) {
    *baselen = 0;
  }
  if (n >= room) {
    goto too_long;
  }
  while (n > 0 && (buf[n - 1] == '/' || buf[n - 1] == '\\')) {
    buf[--n] = '\0';
  }
  if (baselen != NULL) {
    *baselen = n;
  }
  if (nseg != NULL) {
    const size_t nseglen = strlen(nseg);

    if (nseglen >= room - n) { /* room - n > 0 above */
      goto too_long;
    }
    memcpybuff(buf + n, nseg, nseglen + 1);
    n += nseglen;
    if (!st_mkdir_at(buf, n, who)) {
      return 0;
    }
  }
  // Loop on the limit itself, over the whole path: an arithmetic bound landed
  // exactly on 260 for some base lengths, a byte count on a non-ASCII base.
  while (st_utf16_units(buf, n) <= 260) {
    if (n >= room) {
      goto too_long;
    }
    memcpybuff(buf + n, seg, sizeof(seg));
    n += sizeof(seg) - 1;
    if (!st_mkdir_at(buf, n, who)) {
      return 0;
    }
  }
  assertf(st_utf16_units(buf, n) > 260); /* what every caller depends on */
  return n;

too_long:
  fprintf(stderr, "%s: base dir too long (%u chars)\n", who, (unsigned) n);
  return 0;
}
