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

/* Fuzz the ZIP cache reader (htscache.c): cache_init()'s central-directory
   index walk, the local-extra-field header block each entry carries
   (hts_cache_read_fields), the offset-addressed record read behind
   cache_read()/cache_read_ro()/cache_read_including_broken(), and the
   cache_repair() rescan of a damaged cache.

   The container crosses the ZIP CRC gate by construction: each input holds
   [name][headers][body] triples that are zipped with the library's own
   writer, the headers going into the local extra field exactly where
   cache_add() writes them. Mode 1 instead treats the bytes as a raw cache,
   so the damaged-cache repair path is what runs. */
#include "fuzz.h"

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "htscache.h"
#include "htscore.h"
#include "htsglobal.h"
#include "htsstrings.h"
#include "coucal.h"
#include "minizip/zip.h"
#include "minizip/unzip.h"

/* An entry past these bounds says nothing a smaller one cannot. */
#define FUZZ_CACHE_MAXENTRIES 8
#define FUZZ_CACHE_NAME_MAX 512
#define FUZZ_CACHE_HEADER_MAX 2048
#define FUZZ_CACHE_BODY_MAX 16384
#define FUZZ_CACHE_RAW_MAX (256 * 1024)

static char cache_dir[256];
static char cache_zip[sizeof(cache_dir) + sizeof("/hts-cache/new.zip")];
static char cache_old[sizeof(cache_dir) + sizeof("/hts-cache/old.zip")];
static char cache_salvage[sizeof(cache_dir) + sizeof("/hts-cache/repair.zip")];
static httrackp *cache_opt = NULL;
static cache_back cache;

static void fuzz_cache_cleanup(void) {
  char sub[sizeof(cache_dir) + sizeof("/hts-cache")];

  (void) unlink(cache_zip);
  (void) unlink(cache_old);
  (void) unlink(cache_salvage);
  snprintf(sub, sizeof(sub), "%s/hts-cache", cache_dir);
  (void) rmdir(sub);
  (void) rmdir(cache_dir);
}

static unsigned int rd16(const uint8_t *p) {
  return (unsigned int) p[0] | ((unsigned int) p[1] << 8);
}

static unsigned long rd32(const uint8_t *p) {
  return (unsigned long) p[0] | ((unsigned long) p[1] << 8) |
         ((unsigned long) p[2] << 16) | ((unsigned long) p[3] << 24);
}

/* One parsed [name][headers][body] triple. */
typedef struct {
  char name[FUZZ_CACHE_NAME_MAX]; /* as a cache entry is named */
  char adr[FUZZ_CACHE_NAME_MAX];  /* key the index stores it under */
  char fil[FUZZ_CACHE_NAME_MAX];
  char headers[FUZZ_CACHE_HEADER_MAX + 1];
  char body[FUZZ_CACHE_BODY_MAX];
  size_t header_len;
  size_t body_len;
} fuzz_cache_entry;

/* Split the entry name the way the index load does: an optional http://
   comes off, the rest splits at the first slash into (adr, fil). */
static void fuzz_cache_split(const char *name, char *adr, size_t adr_size,
                             char *fil, size_t fil_size) {
  const char *p = name;
  const char *slash;

  if (strfield(name, "http://"))
    p += 7;
  slash = strchr(p, '/');
  if (slash == NULL || slash == p) {
    snprintf(adr, adr_size, "%s", p);
    snprintf(fil, fil_size, "/%s", slash == NULL ? "" : slash + 1);
  } else {
    size_t n = (size_t) (slash - p);

    if (n >= adr_size)
      n = adr_size - 1;
    memcpy(adr, p, n);
    adr[n] = '\0';
    snprintf(fil, fil_size, "%s", slash);
  }
}

/* Copy LEN bytes of SRC into DST (capacity DST_SIZE), dropping NUL bytes and
   control characters a real entry name never carries. */
static void fuzz_cache_name(char *dst, size_t dst_size, const uint8_t *src,
                            size_t len) {
  size_t i, used = 0;

  for (i = 0; i < len && used + 1 < dst_size; i++) {
    if (src[i] >= 0x20 && src[i] != 0x7f)
      dst[used++] = (char) src[i];
  }
  dst[used] = '\0';
}

/* Parse the [count][name][headers][body] triples. */
static int fuzz_cache_parse(const uint8_t *data, size_t size,
                            fuzz_cache_entry *entries) {
  int n, count = 0;

  if (size < 2)
    return 0;
  n = data[1] % (FUZZ_CACHE_MAXENTRIES + 1);
  data += 2, size -= 2;
  while (n-- > 0) {
    unsigned int name_len, header_len;
    unsigned long body_len;
    fuzz_cache_entry *e;

    if (size < 8)
      break;
    name_len = rd16(data);
    header_len = rd16(data + 2);
    body_len = rd32(data + 4);
    data += 8, size -= 8;
    if (name_len > FUZZ_CACHE_NAME_MAX - 16 ||
        header_len > FUZZ_CACHE_HEADER_MAX || body_len > FUZZ_CACHE_BODY_MAX)
      break;
    if (size < (size_t) name_len + header_len + body_len)
      break;
    if (count >= FUZZ_CACHE_MAXENTRIES)
      break;
    e = &entries[count];
    memset(e, 0, sizeof(*e));
    fuzz_cache_name(e->name, sizeof(e->name), data, name_len);
    if (e->name[0] == '\0')
      break;
    memcpy(e->headers, data + name_len, header_len);
    e->headers[header_len] = '\0';
    e->header_len = header_len;
    memcpy(e->body, data + name_len + header_len, body_len);
    e->body_len = body_len;
    fuzz_cache_split(e->name, e->adr, sizeof(e->adr), e->fil, sizeof(e->fil));
    count++;
    data += (size_t) name_len + header_len + body_len;
    size -= (size_t) name_len + header_len + body_len;
  }
  return count;
}

/* Zip the parsed entries into path the way cache_add() writes them: headers
   in the local extra field, body following when X-In-Cache says so. */
static void fuzz_cache_write_zip(const char *path,
                                 const fuzz_cache_entry *entries, int count) {
  zipFile zf = zipOpen(path, 0);
  int i;

  if (zf == NULL)
    return;
  for (i = 0; i < count; i++) {
    zip_fileinfo fi;
    const fuzz_cache_entry *const e = &entries[i];
    const int dataincache = strfield(e->headers, "X-In-Cache: 1") ? 1 : 0;

    memset(&fi, 0, sizeof(fi));
    if (zipOpenNewFileInZip(zf, e->name, &fi, e->headers,
                            (uInt) e->header_len, NULL, 0, NULL, Z_DEFLATED,
                            Z_DEFAULT_COMPRESSION) != Z_OK)
      break;
    if (dataincache && e->body_len > 0)
      (void) zipWriteInFileInZip(zf, e->body, (uInt) e->body_len);
    (void) zipCloseFileInZip(zf);
  }
  (void) zipClose(zf, NULL);
}

/* Read every entry back through the public readers, freeing what they
   return. */
static void fuzz_cache_read_back(const fuzz_cache_entry *entries, int count,
                                 int broken_at, int ro_at) {
  int i;

  for (i = 0; i < count; i++) {
    char location[HTS_LOCATION_SIZE];
    htsblk r;

    memset(location, 0, sizeof(location));
    r = cache_read(cache_opt, &cache, entries[i].adr, entries[i].fil, NULL,
                   location);
    freet(r.adr);
    if (i == broken_at) {
      char save[HTS_URLMAXSIZE * 2];

      memset(save, 0, sizeof(save));
      r = cache_read_including_broken(cache_opt, &cache, entries[i].adr,
                                      entries[i].fil, save, location);
      freet(r.adr);
    }
    if (i == ro_at) {
      r = cache_read_ro(cache_opt, &cache, entries[i].adr, entries[i].fil,
                        NULL, location);
      freet(r.adr);
    }
  }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static int inited = 0;
  const int raw = size > 0 ? (data[0] & 1) : 0;
  char sub[sizeof(cache_dir) + sizeof("/hts-cache")];
  fuzz_cache_entry entries[FUZZ_CACHE_MAXENTRIES];
  int count = 0;
  FILE *fp;

  if (size == 0)
    return 0;
  if (!inited) {
    hts_init();
    if (!fuzz_tmpdir(cache_dir, sizeof(cache_dir), "fuzz-cache",
                     fuzz_cache_cleanup))
      return 0;
    snprintf(cache_zip, sizeof(cache_zip), "%s/hts-cache/new.zip", cache_dir);
    snprintf(cache_old, sizeof(cache_old), "%s/hts-cache/old.zip", cache_dir);
    snprintf(cache_salvage, sizeof(cache_salvage),
             "%s/hts-cache/repair.zip", cache_dir);
    inited = 1;
  }
  (void) unlink(cache_zip);
  (void) unlink(cache_old);
  (void) unlink(cache_salvage);
  snprintf(sub, sizeof(sub), "%s/hts-cache", cache_dir);
  (void) mkdir(sub, 0755);

  data++, size--;
  if (raw) {
    /* The bytes are the cache itself: the damaged-cache paths run. */
    if (size > FUZZ_CACHE_RAW_MAX)
      size = FUZZ_CACHE_RAW_MAX;
    fp = fopen(cache_zip, "wb");
    if (fp == NULL)
      return 0;
    fwrite(data, 1, size, fp);
    fclose(fp);
  } else {
    count = fuzz_cache_parse(data, size, entries);
    fuzz_cache_write_zip(cache_zip, entries, count);
  }

  /* A damaged copy for cache_repair(): chop the tail off a constructed
     cache, keep a raw one whole (it is damage enough). */
  {
    LLint zsize = fsize_utf8(cache_zip);

    if (zsize > 0) {
      char *bytes = readfile2(cache_zip, &zsize);

      if (bytes != NULL) {
        size_t cut = raw
                         ? (size_t) zsize
                         : (zsize > 32 ? (size_t) zsize - 32 : (size_t) zsize);

        fp = fopen(cache_old, "wb");
        if (fp != NULL) {
          fwrite(bytes, 1, cut, fp);
          fclose(fp);
        }
        freet(bytes);
      }
    }
  }

  cache_opt = hts_create_opt();
  cache_opt->log = cache_opt->errlog = NULL;
  cache_opt->cache = 1;
  StringCopy(cache_opt->path_log, cache_dir);

  memset(&cache, 0, sizeof(cache));
  cache.type = 1;
  cache.hashtable = coucal_new(0);
  cache.ro = 1;
  cache_init(&cache, cache_opt);

  /* The index load ran inside cache_init(); read the entries back. */
  if (!raw) {
    fuzz_cache_read_back(entries, count, count / 2, count > 1 ? 1 : 0);
  } else {
    char location[HTS_LOCATION_SIZE];
    htsblk r;

    memset(location, 0, sizeof(location));
    r = cache_read(cache_opt, &cache, "fuzz.example", "/probe", NULL, location);
    freet(r.adr);
  }

  /* The recovery rescan of a damaged cache. */
  {
    unsigned long repaired = 0;
    unsigned long repairedBytes = 0;

    (void) cache_repair(cache_opt, cache_old, &repaired, &repairedBytes);
  }

  if (cache.zipInput != NULL) {
    unzClose(cache.zipInput);
    cache.zipInput = NULL;
  }
  if (cache.hashtable != NULL)
    coucal_delete(&cache.hashtable);
  if (cache.cached_tests != NULL)
    coucal_delete(&cache.cached_tests);
  if (cache.kept != NULL)
    coucal_delete(&cache.kept);
  hts_free_opt(cache_opt);
  cache_opt = NULL;
  return 0;
}
