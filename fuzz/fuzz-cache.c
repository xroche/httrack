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

/* Fuzz the ZIP cache reader: cache_init()'s index walk, every reader on each
   indexed entry, and cache_repair(). Byte 0 picks the mode. Odd: the rest is
   the cache file itself. Even: the rest is [u16 name len][u16 header len]
   [u32 body len][name][headers][body] records, zipped the way cache_add()
   does, so the input gets past the CRC check. */
#include "fuzz.h"

#include <sys/stat.h>
#include <unistd.h>

#include "htscache.h"
#include "htscore.h"
#include "htsglobal.h"
#include "coucal.h"
#include "minizip/zip.h"
#include "minizip/unzip.h"

#define FUZZ_CACHE_MAXENTRIES 8
#define FUZZ_CACHE_NAME_MAX 512
/* u16 name len, u16 header len, u32 body len */
#define FUZZ_CACHE_RECORD_HDR 8
/* past the reader's buffer, so a header block can be clipped */
#define FUZZ_CACHE_HEADER_MAX (CACHE_HEADERS_SIZE + 64)
#define FUZZ_CACHE_BODY_MAX 16384
#define FUZZ_CACHE_RAW_MAX (256 * 1024)

static char cache_dir[256];
static char cache_sub[sizeof(cache_dir) + sizeof("/hts-cache")];
static char cache_zip[sizeof(cache_dir) + sizeof("/hts-cache/new.zip")];
static char cache_salvage[sizeof(cache_dir) + sizeof("/hts-cache/repair.zip")];

static void fuzz_cache_cleanup(void) {
  (void) unlink(cache_zip);
  (void) unlink(cache_salvage);
  (void) rmdir(cache_sub);
  (void) rmdir(cache_dir);
}

static uint16_t rd16(const uint8_t *p) {
  return (uint16_t) (p[0] | (p[1] << 8));
}

static uint32_t rd32(const uint8_t *p) {
  return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) |
         ((uint32_t) p[3] << 24);
}

/* Zip the records into path, headers in the local extra field as cache_add()
   writes them. */
static void fuzz_cache_write_zip(const char *path, const uint8_t *data,
                                 size_t size) {
  zipFile zf = zipOpen(path, 0);
  int n;

  if (zf == NULL)
    return;
  for (n = 0; n < FUZZ_CACHE_MAXENTRIES && size >= FUZZ_CACHE_RECORD_HDR; n++) {
    const uint32_t name_len = rd16(data);
    const uint32_t header_len = rd16(data + 2);
    const uint32_t body_len = rd32(data + 4);
    char name[FUZZ_CACHE_NAME_MAX];
    size_t i, used = 0;
    zip_fileinfo fi;

    data += FUZZ_CACHE_RECORD_HDR, size -= FUZZ_CACHE_RECORD_HDR;
    if (name_len >= sizeof(name) || header_len > FUZZ_CACHE_HEADER_MAX ||
        body_len > FUZZ_CACHE_BODY_MAX ||
        size < (size_t) name_len + header_len + body_len)
      break;
    /* a real entry name carries no control character */
    for (i = 0; i < name_len; i++)
      if (data[i] >= 0x20 && data[i] != 0x7f)
        name[used++] = (char) data[i];
    name[used] = '\0';
    memset(&fi, 0, sizeof(fi));
    if (used == 0 ||
        zipOpenNewFileInZip(zf, name, &fi, data + name_len, header_len, NULL, 0,
                            NULL, Z_DEFLATED, Z_DEFAULT_COMPRESSION) != Z_OK)
      break;
    (void) zipWriteInFileInZip(zf, data + name_len + header_len, body_len);
    (void) zipCloseFileInZip(zf);
    data += (size_t) name_len + header_len + body_len;
    size -= (size_t) name_len + header_len + body_len;
  }
  (void) zipClose(zf, NULL);
}

/* Read an indexed entry through each reader: headers only, headers and body,
   and the variant that also takes kept and broken entries. */
static void fuzz_cache_read_entry(httrackp *opt, cache_back *cache,
                                  const char *key) {
  char location[HTS_LOCATION_SIZE];
  char save[HTS_URLMAXSIZE * 2];
  htsblk r;

  r = cache_read(opt, cache, key, "", NULL, HTS_BUF(location));
  freet(r.adr);
  r = cache_readex(opt, cache, key, "", "", HTS_BUF(location), hts_buf_none(),
                   1);
  freet(r.adr);
  save[0] = '\0';
  r = cache_read_including_broken(opt, cache, key, "", HTS_BUF(save),
                                  HTS_BUF(location));
  freet(r.adr);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static int inited = 0;
  httrackp *opt;
  cache_back cache;
  hts_boolean raw;

  if (size == 0)
    return 0;
  if (!inited) {
    hts_init();
    if (!fuzz_tmpdir(cache_dir, sizeof(cache_dir), "fuzz-cache",
                     fuzz_cache_cleanup))
      return 0;
    snprintf(cache_sub, sizeof(cache_sub), "%s/hts-cache", cache_dir);
    snprintf(cache_zip, sizeof(cache_zip), "%s/new.zip", cache_sub);
    snprintf(cache_salvage, sizeof(cache_salvage), "%s/repair.zip", cache_sub);
    (void) mkdir(cache_sub, 0755);
    inited = 1;
  }
  (void) unlink(cache_zip);
  (void) unlink(cache_salvage);

  raw = (data[0] & 1) != 0;
  data++, size--;
  if (raw) {
    FILE *const fp = fopen(cache_zip, "wb");
    const size_t len = size < FUZZ_CACHE_RAW_MAX ? size : FUZZ_CACHE_RAW_MAX;
    hts_boolean written;

    if (fp == NULL)
      return 0;
    written = fwrite(data, 1, len, fp) == len;
    if (fclose(fp) != 0 || !written)
      return 0;
  } else {
    fuzz_cache_write_zip(cache_zip, data, size);
  }

  opt = hts_create_opt();
  opt->log = opt->errlog = NULL;
  opt->cache = 1;
  /* X-Save names resolve under the temp dir, not the working directory */
  StringCopy(opt->path_html_utf8, cache_dir);
  StringCat(opt->path_html_utf8, "/");
  StringCopy(opt->path_html, StringBuff(opt->path_html_utf8));
  StringCopy(opt->path_log, StringBuff(opt->path_html_utf8));

  memset(&cache, 0, sizeof(cache));
  cache.type = 1;
  cache.ro = 1;
  cache.hashtable = coucal_new(0);
  cache_init(&cache, opt);

  if (cache.zipInput != NULL) {
    struct_coucal_enum e = coucal_enum_new(cache.hashtable);
    const coucal_item *item;
    int n = 0;

    /* a raw zip can index any number of entries, so cap the per-input work */
    while ((item = coucal_enum_next(&e)) != NULL && n++ < FUZZ_CACHE_MAXENTRIES)
      fuzz_cache_read_entry(opt, &cache, item->name);
    unzClose(cache.zipInput);
  }

  {
    unsigned long entries, bytes;

    (void) cache_repair(opt, cache_zip, &entries, &bytes);
  }

  coucal_delete(&cache.hashtable);
  if (cache.cached_tests != NULL)
    coucal_delete(&cache.cached_tests);
  if (cache.kept != NULL)
    coucal_delete(&cache.kept);
  hts_free_opt(opt);
  return 0;
}
