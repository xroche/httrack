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
/* File: htschanges.c subroutines:                              */
/*       --changes: what this crawl changed vs. the previous    */
/*       mirror (hts-changes.json)                              */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

#define HTS_INTERNAL_BYTECODE

#include "htschanges.h"

#include "htscache.h"
#include "htscharset.h"
#include "htscore.h"
#include "htslib.h"
#include "htsmd5.h"
#include "htssafe.h"
#include "htstools.h"
#include "coucal/coucal.h"
#include "md5.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DIGEST_SIZE 16

typedef struct hts_changes hts_changes;

/* One mirrored file. The mirror-relative path is the key, not the URL: a
   redirect and its target can share a save name, and counting that file twice
   would put it in two buckets. */
typedef struct {
  char *url;               /* absolute URL, "" for an engine-generated file */
  char *file;              /* mirror-relative path, as listed in new.lst */
  hts_boolean rewritten;   /* the crawl wrote over the local copy */
  hts_boolean not_updated; /* transfer signal: the server reported no change */
  hts_boolean existed;     /* the file was part of the previous mirror */
  hts_boolean on_disk;     /* a copy was on disk when the crawl first saw it */
  hts_boolean listed_prev; /* the previous mirror's index listed this file */
  hts_boolean has_prev;    /* prev_digest holds the previous payload's digest */
  hts_boolean has_new;     /* new_digest was taken from the payload, not disk */
  unsigned char prev_digest[DIGEST_SIZE];
  unsigned char new_digest[DIGEST_SIZE];
  LLint prev_size; /* -1 when unknown */
  LLint size;      /* size after the crawl, -1 when unknown */
  hts_change_bucket bucket;
} changes_entry;

struct hts_changes {
  coucal index; /* mirror-relative path -> entry slot + 1 */
  changes_entry *entries;
  size_t count;
  size_t capacity;
  hts_boolean old_index; /* the previous mirror's index was read */
  hts_boolean overflow;  /* an allocation failed; the report is partial */
};

/* ------------------------------------------------------------ */
/* JSON                                                          */
/* ------------------------------------------------------------ */

/* Length of the UTF-8 sequence led by c, or 0 if c cannot lead one. */
static size_t utf8_lead_length(unsigned char c) {
  if (c >= 0xc2 && c <= 0xdf)
    return 2;
  if (c >= 0xe0 && c <= 0xef)
    return 3;
  if (c >= 0xf0 && c <= 0xf4)
    return 4;
  return 0;
}

void hts_changes_json_string(String *out, const char *s) {
  const unsigned char *p = (const unsigned char *) s;

  StringAddchar(*out, '"');
  while (*p != '\0') {
    const unsigned char c = *p;

    if (c == '"' || c == '\\') {
      StringAddchar(*out, '\\');
      StringAddchar(*out, (char) c);
      p++;
    } else if (c < 0x20 || c == 0x7f) {
      char esc[8];

      snprintf(esc, sizeof(esc), "\\u%04x", (unsigned) c);
      StringCat(*out, esc);
      p++;
    } else if (c < 0x80) {
      StringAddchar(*out, (char) c);
      p++;
    } else {
      /* Non-ASCII: emit it verbatim only if it is a well-formed sequence, so
         a legacy-charset URL cannot produce unparseable JSON. */
      const size_t len = utf8_lead_length(c);

      if (len != 0 && strnlen((const char *) p, len) == len &&
          hts_isStringUTF8((const char *) p, len)) {
        StringMemcat(*out, (const char *) p, len);
        p += len;
      } else {
        StringCat(*out, "\\ufffd");
        p++;
      }
    }
  }
  StringAddchar(*out, '"');
}

/* ------------------------------------------------------------ */
/* Accumulator                                                   */
/* ------------------------------------------------------------ */

static hts_changes *changes_new(void) {
  hts_changes *changes = calloct(1, sizeof(*changes));

  if (changes == NULL)
    return NULL;
  changes->index = coucal_new(0);
  if (changes->index == NULL) {
    freet(changes);
    return NULL;
  }
  return changes;
}

static void changes_free(hts_changes **pchanges) {
  hts_changes *changes = *pchanges;

  if (changes == NULL)
    return;
  if (changes->entries != NULL) {
    size_t i;

    for (i = 0; i < changes->count; i++) {
      freet(changes->entries[i].url);
      freet(changes->entries[i].file);
    }
    freet(changes->entries);
  }
  if (changes->index != NULL)
    coucal_delete(&changes->index);
  freet(changes);
  *pchanges = NULL;
}

/* Slot of `file`, or -1 when it has not been recorded. */
static intptr_t changes_find(const hts_changes *changes, const char *file) {
  intptr_t slot = 0;

  return coucal_read(changes->index, file, &slot) ? slot - 1 : -1;
}

/* Slot of `file`, appending a fresh entry when it is new; -1 if allocation
   failed, which flags the report partial. */
static intptr_t changes_slot(hts_changes *changes, const char *file,
                             hts_boolean *is_new) {
  intptr_t slot = 0;

  *is_new = HTS_FALSE;
  if (coucal_read(changes->index, file, &slot))
    return slot - 1;

  if (changes->count == changes->capacity) {
    const size_t capacity = changes->capacity != 0 ? changes->capacity * 2 : 64;
    changes_entry *const entries =
        realloct(changes->entries, capacity * sizeof(*entries));

    if (entries == NULL) {
      changes->overflow = HTS_TRUE;
      return -1;
    }
    changes->entries = entries;
    changes->capacity = capacity;
  }
  slot = (intptr_t) changes->count;
  memset(&changes->entries[slot], 0, sizeof(changes->entries[slot]));
  changes->entries[slot].file = strdupt(file);
  changes->entries[slot].prev_size = -1;
  changes->entries[slot].size = -1;
  if (changes->entries[slot].file == NULL) {
    changes->overflow = HTS_TRUE;
    return -1;
  }
  changes->count++;
  coucal_write(changes->index, file, slot + 1);
  *is_new = HTS_TRUE;
  return slot;
}

/* MD5 of the file at `path`; change detection over our own mirror, not a
   security boundary. HTS_FALSE if it cannot be read. */
static hts_boolean digest_file(const char *path,
                               unsigned char digest[DIGEST_SIZE]) {
  const int endian = 1;
  struct MD5Context ctx;
  char BIGSTK buffer[32768];
  FILE *fp = FOPEN(path, "rb");
  size_t nread;

  if (fp == NULL)
    return HTS_FALSE;
  MD5Init(&ctx, *((const char *) &endian));
  while ((nread = fread(buffer, 1, sizeof(buffer), fp)) > 0)
    MD5Update(&ctx, (const unsigned char *) buffer, (unsigned int) nread);
  if (ferror(fp) != 0) {
    fclose(fp);
    return HTS_FALSE;
  }
  fclose(fp);
  MD5Final(digest, &ctx);
  return HTS_TRUE;
}

static void digest_mem(const char *buffer, size_t len,
                       unsigned char digest[DIGEST_SIZE]) {
  domd5mem(buffer, len, (char *) digest, 0);
}

hts_change_bucket hts_changes_classify(hts_boolean rewritten,
                                       hts_boolean existed,
                                       hts_boolean not_updated,
                                       hts_boolean have_digests,
                                       hts_boolean digests_equal) {
  if (!rewritten)
    return HTS_CHANGE_UNCHANGED; /* the crawl left the copy alone */
  if (!existed)
    return HTS_CHANGE_NEW;
  if (have_digests)
    return digests_equal ? HTS_CHANGE_UNCHANGED : HTS_CHANGE_CHANGED;
  /* No digest to compare: a server with no validators answers 200 with the
     same bytes, so this signal alone over-reports. */
  return not_updated ? HTS_CHANGE_UNCHANGED : HTS_CHANGE_CHANGED;
}

/* ------------------------------------------------------------ */
/* Engine hooks                                                  */
/* ------------------------------------------------------------ */

/* The live accumulator, created on first use; NULL when --changes is off. */
static hts_changes *changes_get(httrackp *opt) {
  if (!opt->changes)
    return NULL;
  if (opt->changes_state == NULL)
    opt->changes_state = changes_new();
  return (hts_changes *) opt->changes_state;
}

void hts_changes_notify(httrackp *opt, const char *adr, const char *fil,
                        const char *save, hts_boolean rewritten,
                        hts_boolean not_updated) {
  hts_changes *const changes = changes_get(opt);
  char BIGSTK file[HTS_URLMAXSIZE * 2];
  hts_boolean is_new;
  intptr_t slot;
  changes_entry *entry;

  /* Engine-generated scaffolding (the top index) carries no URL and is not a
     mirrored resource. */
  if (changes == NULL || save == NULL || !strnotempty(save) || adr == NULL ||
      fil == NULL || (!strnotempty(adr) && !strnotempty(fil)))
    return;

  hts_savename_listed(&opt->state.strc, save, file, sizeof(file));
  slot = changes_slot(changes, file, &is_new);
  if (slot < 0)
    return;
  entry = &changes->entries[slot];

  if (!is_new) {
    /* Retry, or a second call site for the same file: the pre-run state was
       already sampled and the copy on disk may no longer be it. */
    entry->rewritten = entry->rewritten || rewritten;
    return;
  }

  {
    char BIGSTK url[HTS_URLMAXSIZE * 2];

    url[0] = '\0';
    if (!link_has_authority(adr))
      strlcatbuff(url, "http://", sizeof(url));
    strlcatbuff(url, adr, sizeof(url));
    strlcatbuff(url, fil, sizeof(url));
    entry->url = strdupt(url);
    if (entry->url == NULL)
      changes->overflow = HTS_TRUE;
  }

  entry->rewritten = rewritten;
  entry->not_updated = not_updated;
  entry->prev_size = fsize_utf8(save);
  entry->on_disk = entry->prev_size >= 0;
  /* Hash the previous copy only when it is about to be overwritten: this is
     the last moment those bytes exist. */
  if (entry->on_disk && rewritten)
    entry->has_prev = digest_file(save, entry->prev_digest);
}

void hts_changes_html(httrackp *opt, cache_back *cache, const htsblk *r,
                      const char *adr, const char *fil, const char *save) {
  hts_changes *const changes = changes_get(opt);
  char BIGSTK file[HTS_URLMAXSIZE * 2];
  char BIGSTK location[HTS_URLMAXSIZE * 2];
  changes_entry *entry;
  intptr_t slot;
  htsblk prev;

  if (changes == NULL || r->adr == NULL || r->size < 0 || save == NULL ||
      !strnotempty(save))
    return;
  hts_savename_listed(&opt->state.strc, save, file, sizeof(file));
  /* file_notify() records the entry; this call only refines it. */
  slot = changes_find(changes, file);
  if (slot < 0)
    return;
  entry = &changes->entries[slot];

  /* On disk this is the payload plus rewritten links and a footer dated by the
     crawl, so it differs every run; compare payloads, the previous one being
     the body the cache kept. */
  entry->has_new = HTS_TRUE;
  digest_mem(r->adr, (size_t) r->size, entry->new_digest);
  entry->has_prev = HTS_FALSE;
  prev = cache_read_ro(opt, cache, adr, fil, "", location);
  if (HTTP_IS_OK(prev.statuscode) && prev.adr != NULL && prev.size >= 0) {
    digest_mem(prev.adr, (size_t) prev.size, entry->prev_digest);
    entry->has_prev = HTS_TRUE;
  }
  freet(prev.adr);
}

void hts_changes_gone(httrackp *opt, const char *file) {
  hts_changes *const changes = changes_get(opt);
  hts_boolean is_new;
  intptr_t slot;

  if (changes == NULL || file == NULL || !strnotempty(file))
    return;
  slot = changes_slot(changes, file, &is_new);
  if (slot < 0 || !is_new)
    return;
  changes->entries[slot].url = strdupt("");
  changes->entries[slot].bucket = HTS_CHANGE_GONE;
  changes->entries[slot].listed_prev = HTS_TRUE;
}

void hts_changes_previous(httrackp *opt, const char *file) {
  hts_changes *const changes = changes_get(opt);
  intptr_t slot;

  if (changes == NULL || file == NULL)
    return;
  changes->old_index = HTS_TRUE;
  slot = changes_find(changes, file);
  if (slot >= 0)
    changes->entries[slot].listed_prev = HTS_TRUE;
}

/* ------------------------------------------------------------ */
/* Report                                                        */
/* ------------------------------------------------------------ */

/* Assign each entry its final bucket from the bytes now on disk. */
static void changes_resolve(hts_changes *changes, httrackp *opt) {
  char catbuff[CATBUFF_SIZE];
  size_t i;

  for (i = 0; i < changes->count; i++) {
    changes_entry *const entry = &changes->entries[i];
    const char *path;
    unsigned char digest[DIGEST_SIZE];
    hts_boolean have_digests = HTS_FALSE;
    hts_boolean digests_equal = HTS_FALSE;

    if (entry->bucket == HTS_CHANGE_GONE)
      continue;

    /* The previous mirror's index is the authority on what was there before:
       a partial left by this crawl's own failed attempt is on disk but was
       never part of the previous mirror. Without an index, fall back to what
       the first notify saw on disk. */
    entry->existed = changes->old_index ? entry->listed_prev : entry->on_disk;
    path = fconcat(catbuff, sizeof(catbuff), StringBuff(opt->path_html),
                   entry->file);
    entry->size = fsize_utf8(path);
    if (entry->rewritten && entry->existed) {
      if (entry->size >= 0 && entry->prev_size >= 0 &&
          entry->size != entry->prev_size) {
        have_digests = HTS_TRUE; /* different lengths: no need to hash */
        digests_equal = HTS_FALSE;
      } else if (entry->has_prev &&
                 (entry->has_new
                      ? (memcpy(digest, entry->new_digest, DIGEST_SIZE), 1)
                      : digest_file(path, digest))) {
        have_digests = HTS_TRUE;
        digests_equal = memcmp(digest, entry->prev_digest, DIGEST_SIZE) == 0
                            ? HTS_TRUE
                            : HTS_FALSE;
      }
    }
    entry->bucket =
        hts_changes_classify(entry->rewritten, entry->existed,
                             entry->not_updated, have_digests, digests_equal);
  }
}

static const char *const bucket_names[HTS_CHANGE_BUCKETS] = {
    "new", "changed", "unchanged", "gone"};

void hts_changes_report(httrackp *opt, String *out) {
  hts_changes *const changes = (hts_changes *) opt->changes_state;
  size_t counts[HTS_CHANGE_BUCKETS];
  char date[32];
  char scratch[64];
  int bucket;
  size_t i;

  StringClear(*out);
  if (changes == NULL)
    return;
  changes_resolve(changes, opt);

  memset(counts, 0, sizeof(counts));
  for (i = 0; i < changes->count; i++)
    counts[changes->entries[i].bucket]++;

  hts_now_iso8601(date);
  StringCat(*out, "{\n  \"schema\": ");
  snprintf(scratch, sizeof(scratch), "%d", HTS_CHANGES_SCHEMA);
  StringCat(*out, scratch);
  StringCat(*out, ",\n  \"generator\": ");
  hts_changes_json_string(out, "HTTrack Website Copier/" HTTRACK_VERSION);
  StringCat(*out, ",\n  \"date\": ");
  hts_changes_json_string(out, date);
  StringCat(*out, ",\n  \"first_crawl\": ");
  StringCat(*out, changes->old_index ? "false" : "true");
  StringCat(*out, ",\n  \"partial\": ");
  StringCat(*out, changes->overflow ? "true" : "false");
  StringCat(*out, ",\n  \"purged\": ");
  StringCat(*out, opt->delete_old ? "true" : "false");

  StringCat(*out, ",\n  \"counts\": {");
  for (bucket = 0; bucket < HTS_CHANGE_BUCKETS; bucket++) {
    StringCat(*out, bucket != 0 ? ", " : " ");
    hts_changes_json_string(out, bucket_names[bucket]);
    snprintf(scratch, sizeof(scratch), ": %d", (int) counts[bucket]);
    StringCat(*out, scratch);
  }
  StringCat(*out, " }");

  for (bucket = 0; bucket < HTS_CHANGE_BUCKETS; bucket++) {
    hts_boolean first = HTS_TRUE;

    StringCat(*out, ",\n  ");
    hts_changes_json_string(out, bucket_names[bucket]);
    StringCat(*out, ": [");
    for (i = 0; i < changes->count; i++) {
      const changes_entry *const entry = &changes->entries[i];

      if (entry->bucket != bucket)
        continue;
      StringCat(*out, first ? "\n    { \"url\": " : ",\n    { \"url\": ");
      first = HTS_FALSE;
      hts_changes_json_string(out, entry->url != NULL ? entry->url : "");
      StringCat(*out, ", \"file\": ");
      hts_changes_json_string(out, entry->file);
      if (entry->size >= 0) {
        snprintf(scratch, sizeof(scratch), ", \"size\": " LLintP,
                 (LLint) entry->size);
        StringCat(*out, scratch);
      }
      if (bucket == HTS_CHANGE_CHANGED && entry->prev_size >= 0) {
        snprintf(scratch, sizeof(scratch), ", \"previous_size\": " LLintP,
                 (LLint) entry->prev_size);
        StringCat(*out, scratch);
      }
      StringCat(*out, " }");
    }
    StringCat(*out, first ? "]" : "\n  ]");
  }
  StringCat(*out, "\n}\n");
}

void hts_changes_close_opt(httrackp *opt) {
  hts_changes *changes = (hts_changes *) opt->changes_state;
  char catbuff[CATBUFF_SIZE];
  const char *path;
  String report = STRING_EMPTY;
  size_t counts[HTS_CHANGE_BUCKETS];
  size_t i;
  FILE *fp;

  if (changes == NULL)
    return;

  hts_changes_report(opt, &report);
  memset(counts, 0, sizeof(counts));
  for (i = 0; i < changes->count; i++)
    counts[changes->entries[i].bucket]++;

  path = fconcat(catbuff, sizeof(catbuff), StringBuff(opt->path_log),
                 HTS_CHANGES_FILE);
  fp = FOPEN(path, "wb");
  if (fp != NULL) {
    const size_t len = StringLength(report);

    if (len != 0 && fwrite(StringBuff(report), 1, len, fp) != len)
      hts_log_print(opt, LOG_ERROR | LOG_ERRNO,
                    "Unable to write the change report %s", path);
    fclose(fp);
  } else {
    hts_log_print(opt, LOG_ERROR | LOG_ERRNO,
                  "Unable to create the change report %s", path);
  }

  if (!changes->old_index) {
    hts_log_print(opt, LOG_NOTICE,
                  "Change report: first crawl, %d files mirrored, nothing to "
                  "compare against (%s)",
                  (int) changes->count, HTS_CHANGES_FILE);
  } else {
    hts_log_print(opt, LOG_NOTICE,
                  "Change report: %d new, %d changed, %d unchanged, %d gone "
                  "(%s)",
                  (int) counts[HTS_CHANGE_NEW],
                  (int) counts[HTS_CHANGE_CHANGED],
                  (int) counts[HTS_CHANGE_UNCHANGED],
                  (int) counts[HTS_CHANGE_GONE], HTS_CHANGES_FILE);
  }

  StringFree(report);
  changes_free(&changes);
  opt->changes_state = NULL;
}

void hts_changes_free_opt(httrackp *opt) {
  hts_changes *changes = (hts_changes *) opt->changes_state;

  changes_free(&changes);
  opt->changes_state = NULL;
}
