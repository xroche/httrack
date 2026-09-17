#define _XOPEN_SOURCE 700

#include "htscatalog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int add_post_and_media(hts_catalog *catalog, int64_t source_id,
                              int64_t collection_id, const char *post_remote,
                              int position, const char *subject,
                              const char *rating, const char *metadata,
                              const char *media_remote, const char *filename,
                              const char *availability, const char *hash,
                              int64_t size, int64_t *media_id) {
  hts_catalog_post post;
  hts_catalog_media media;
  int64_t post_id;
  (void) memset(&post, 0, sizeof(post));
  post.source_id = source_id;
  post.collection_id = collection_id;
  post.remote_id = post_remote;
  post.subject = subject;
  post.text = "Synthetic Phase 4 fixture metadata";
  post.rating = rating;
  post.created_at = "1700001000";
  post.updated_at = "1700001001";
  post.last_seen_at = "1700001001";
  post.position = position;
  post.raw_metadata_version = 1;
  post.metadata_json = metadata;
  if (hts_catalog_upsert_post(catalog, &post, &post_id) != HTS_CATALOG_OK)
    return 0;
  (void) memset(&media, 0, sizeof(media));
  media.source_id = source_id;
  media.post_id = post_id;
  media.remote_id = media_remote;
  media.variant_kind = "original";
  media.remote_url = "https://i.4cdn.org/safe/999.jpg";
  media.original_filename = filename;
  media.extension = ".jpg";
  media.mime_type = "image/jpeg";
  media.size_bytes = size;
  media.width = 640;
  media.height = 480;
  media.remote_hash = hash;
  media.hash_algorithm = "md5-base64";
  media.availability_state = availability;
  media.metadata_json = "{\"synthetic\":true}";
  return hts_catalog_upsert_media(catalog, &media, media_id) == HTS_CATALOG_OK;
}

static int seed(const char *path, int64_t source_id) {
  hts_catalog *catalog;
  int64_t board_id;
  int64_t collection_id;
  int64_t media_a;
  int64_t media_b;
  int64_t media_c;
  hts_catalog_collection collection;
  hts_catalog_download_job job;
  int index;
  char remote_id[64];
  char title[128];
  char post_remote[64];
  char media_remote[64];
  char long_filename[400];
  catalog = NULL;
  if (hts_catalog_open(path, &catalog) != HTS_CATALOG_OK) return 0;
  if (hts_catalog_find_id(catalog, HTS_CATALOG_ENTITY_BOARD, source_id,
                          "safe", NULL, &board_id) != HTS_CATALOG_OK) {
    hts_catalog_close(catalog); return 0;
  }
  if (hts_catalog_begin(catalog) != HTS_CATALOG_OK) {
    hts_catalog_close(catalog); return 0;
  }
  for (index = 0; index < 125; index++) {
    (void) snprintf(remote_id, sizeof(remote_id), "bulk/%03d", index);
    (void) snprintf(title, sizeof(title), "Bulk synthetic %03d", index);
    (void) memset(&collection, 0, sizeof(collection));
    collection.source_id = source_id;
    collection.board_id = board_id;
    collection.kind = "thread";
    collection.remote_id = remote_id;
    collection.title = title;
    collection.lifecycle_state = "active";
    collection.created_at = "1700001000";
    collection.updated_at = "1700001001";
    collection.last_seen_at = "1700001001";
    collection.metadata_json = "{\"tags\":[\"bulk\",\"alpha\"]}";
    if (hts_catalog_upsert_collection(catalog, &collection,
                                      &collection_id) != HTS_CATALOG_OK)
      goto failure;
    (void) snprintf(post_remote, sizeof(post_remote), "bulk-post/%03d", index);
    (void) snprintf(media_remote, sizeof(media_remote), "bulk-media/%03d", index);
    if (!add_post_and_media(catalog, source_id, collection_id, post_remote, 0,
                            title, index % 2 == 0 ? "safe" : "questionable",
                            "{\"tags\":[\"alpha\"]}", media_remote,
                            "bulk.jpg", "available", NULL, 1000 + index,
                            &media_a)) goto failure;
  }
  (void) memset(&collection, 0, sizeof(collection));
  collection.source_id = source_id;
  collection.board_id = board_id;
  collection.kind = "thread";
  collection.remote_id = "hostile/900";
  collection.title = "Cafe\314\201 / .. CON";
  collection.lifecycle_state = "active";
  collection.created_at = "1700001000";
  collection.updated_at = "1700001001";
  collection.last_seen_at = "1700001001";
  collection.metadata_json = "{\"tags\":[\"hostile\",\"alpha\"]}";
  if (hts_catalog_upsert_collection(catalog, &collection, &collection_id) !=
      HTS_CATALOG_OK) goto failure;
  (void) memset(long_filename, 'x', sizeof(long_filename) - 1U);
  (void) memcpy(long_filename, "../", 3U);
  (void) memcpy(long_filename + sizeof(long_filename) - 5U, ".JPG", 5U);
  if (!add_post_and_media(catalog, source_id, collection_id, "hostile/901", 0,
                          "Hostile filename", "safe",
                          "{\"tags\":[\"hostile\"]}", "hostile-media/a",
                          "../CON?.JPG", "available",
                          "SYNTHETIC-DUPLICATE==", 2048, &media_a) ||
      !add_post_and_media(catalog, source_id, collection_id, "hostile/901", 0,
                          "Hostile filename", "safe",
                          "{\"tags\":[\"hostile\"]}", "hostile-media/b",
                          "../CON?.JPG", "expired",
                          "SYNTHETIC-DUPLICATE==", 2048, &media_b) ||
      !add_post_and_media(catalog, source_id, collection_id, "hostile/901", 0,
                          "Hostile filename", "safe",
                          "{\"tags\":[\"hostile\"]}", "hostile-media/c",
                          long_filename, "available", NULL, 2048, &media_c))
    goto failure;
  (void) memset(&job, 0, sizeof(job));
  job.source_id = source_id;
  job.media_id = media_a;
  job.idempotency_key = "phase4-local-fixture";
  job.status = "complete";
  job.target_path = "fixture/already-local.jpg";
  if (hts_catalog_upsert_download_job(catalog, &job, NULL) != HTS_CATALOG_OK)
    goto failure;
  if (hts_catalog_commit(catalog) != HTS_CATALOG_OK) goto failure_close;
  hts_catalog_close(catalog);
  return 1;
failure:
  (void) hts_catalog_rollback(catalog);
failure_close:
  hts_catalog_close(catalog);
  return 0;
}

static int mutate(const char *path, int64_t source_id) {
  hts_catalog *catalog;
  int64_t collection_id;
  int64_t media_id;
  catalog = NULL;
  if (hts_catalog_open(path, &catalog) != HTS_CATALOG_OK) return 0;
  if (hts_catalog_find_id(catalog, HTS_CATALOG_ENTITY_COLLECTION, source_id,
                          "thread", "hostile/900", &collection_id) !=
      HTS_CATALOG_OK) {
    hts_catalog_close(catalog); return 0;
  }
  if (!add_post_and_media(catalog, source_id, collection_id, "hostile/902", 1,
                          "Late remote post", "safe", "{\"tags\":[\"late\"]}",
                          "hostile-media/late", "late.jpg", "available", NULL,
                          4096, &media_id)) {
    hts_catalog_close(catalog); return 0;
  }
  hts_catalog_close(catalog); return 1;
}

int main(int argc, char **argv) {
  int64_t source_id;
  char *end;
  if (argc != 4) return 2;
  source_id = (int64_t) strtoll(argv[3], &end, 10);
  if (*end != '\0' || source_id <= 0) return 2;
  if (strcmp(argv[1], "seed") == 0) return seed(argv[2], source_id) ? 0 : 1;
  if (strcmp(argv[1], "mutate") == 0) return mutate(argv[2], source_id) ? 0 : 1;
  return 2;
}
