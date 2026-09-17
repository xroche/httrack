#include "htscatalog.h"

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(condition) do { \
  if (!(condition)) { \
    fprintf(stderr, "catalog-test:%d: check failed: %s\n", __LINE__, #condition); \
    return 1; \
  } \
} while (0)

typedef struct visit_state {
  int count;
  int64_t ids[8];
  int64_t positions[8];
  char labels[8][128];
  char filenames[8][128];
} visit_state;

static int collect_entry(void *user, const hts_catalog_entry *entry) {
  visit_state *state;
  int index;
  state = (visit_state *) user;
  index = state->count;
  if (index < 8) {
    state->ids[index] = entry->id;
    state->positions[index] = entry->position;
    if (entry->label != NULL) {
      (void) snprintf(state->labels[index], sizeof(state->labels[index]),
                      "%s", entry->label);
    }
    if (entry->remote_id != NULL) {
      (void) snprintf(state->filenames[index], sizeof(state->filenames[index]),
                      "%s", entry->remote_id);
    }
  }
  state->count++;
  return 0;
}

static int make_temp_path(char *path, size_t path_size) {
  int fd;
  if (snprintf(path, path_size, "/tmp/httrack-catalog-XXXXXX") < 0) return -1;
  fd = mkstemp(path);
  if (fd < 0) return -1;
  (void) close(fd);
  (void) unlink(path);
  return 0;
}

static int create_future_database(const char *path) {
  sqlite3 *db;
  char *error;
  int rc;
  db = NULL;
  error = NULL;
  rc = sqlite3_open(path, &db);
  if (rc == SQLITE_OK)
    rc = sqlite3_exec(db, "PRAGMA user_version=999", NULL, NULL, &error);
  if (error != NULL) sqlite3_free(error);
  if (db != NULL) (void) sqlite3_close(db);
  return rc == SQLITE_OK ? 0 : -1;
}

static int create_v1_database(const char *path) {
  static const char sql[] =
    "CREATE TABLE schema_migrations(version INTEGER PRIMARY KEY,"
    "applied_at TEXT NOT NULL DEFAULT 'fixture');"
    "INSERT INTO schema_migrations(version) VALUES(1);"
    "CREATE TABLE sources(id INTEGER PRIMARY KEY);"
    "CREATE TABLE sync_cursors("
    "id INTEGER PRIMARY KEY,source_id INTEGER NOT NULL,"
    "resource_kind TEXT NOT NULL,resource_id TEXT NOT NULL,"
    "cursor_value TEXT,etag TEXT,last_modified TEXT,"
    "last_attempt_at TEXT,last_success_at TEXT,error_state TEXT,"
    "metadata_json TEXT,UNIQUE(source_id,resource_kind,resource_id));"
    "CREATE TABLE posts(id INTEGER PRIMARY KEY);"
    "PRAGMA user_version=1;";
  sqlite3 *db;
  char *error;
  int rc;
  db = NULL;
  error = NULL;
  rc = sqlite3_open(path, &db);
  if (rc == SQLITE_OK)
    rc = sqlite3_exec(db, sql, NULL, NULL, &error);
  if (error != NULL) sqlite3_free(error);
  if (db != NULL) (void) sqlite3_close(db);
  return rc == SQLITE_OK ? 0 : -1;
}

static int run_catalog_tests(void) {
  char path[128];
  char future_path[128];
  char v1_path[128];
  char sanitized[128];
  char *oversized;
  hts_catalog *catalog;
  hts_catalog *future;
  hts_catalog_source source;
  hts_catalog_board board;
  hts_catalog_collection collection;
  hts_catalog_post post;
  hts_catalog_media media;
  hts_catalog_sync_cursor cursor;
  hts_catalog_selection selection;
  hts_catalog_download_job job;
  visit_state visited;
  int64_t source_id;
  int64_t repeated_source_id;
  int64_t board_id;
  int64_t collection_id;
  int64_t post_id;
  int64_t media1_id;
  int64_t media2_id;
  int64_t selection_id;
  int64_t rollback_id;
  int version;

  CHECK(make_temp_path(path, sizeof(path)) == 0);
  catalog = NULL;
  CHECK(hts_catalog_open(path, &catalog) == HTS_CATALOG_OK);
  CHECK(hts_catalog_schema_version(catalog, &version) == HTS_CATALOG_OK);
  CHECK(version == HTS_CATALOG_SCHEMA_VERSION);

  memset(&source, 0, sizeof(source));
  source.adapter_kind = "fixture";
  source.canonical_base_url = "https://example.invalid/api";
  source.display_name = "画像板";
  source.enabled = 1;
  source.policy_json = "{\"respect_rate_limits\":true}";
  source.credential_ref = "env:HTTRACK_TEST_TOKEN";
  source.metadata_json = "{\"unknown\":\"雪\"}";
  CHECK(hts_catalog_begin(catalog) == HTS_CATALOG_OK);
  CHECK(hts_catalog_upsert_source(catalog, &source, &source_id) == HTS_CATALOG_OK);
  CHECK(hts_catalog_commit(catalog) == HTS_CATALOG_OK);
  source.display_name = "画像板・更新";
  CHECK(hts_catalog_upsert_source(catalog, &source, &repeated_source_id) == HTS_CATALOG_OK);
  CHECK(source_id == repeated_source_id);
  memset(&visited, 0, sizeof(visited));
  CHECK(hts_catalog_read(catalog, HTS_CATALOG_ENTITY_SOURCE, source_id,
                         collect_entry, &visited) == HTS_CATALOG_OK);
  CHECK(visited.count == 1);
  CHECK(strcmp(visited.labels[0], "画像板・更新") == 0);

  source.credential_ref = "plaintext-secret";
  CHECK(hts_catalog_upsert_source(catalog, &source, NULL) == HTS_CATALOG_INVALID);
  source.credential_ref = "env:HTTRACK_TEST_TOKEN";
  source.metadata_json = "not-json";
  CHECK(hts_catalog_upsert_source(catalog, &source, NULL) == HTS_CATALOG_INVALID);
  source.metadata_json = "{\"api_key\":\"must-not-persist\"}";
  CHECK(hts_catalog_upsert_source(catalog, &source, NULL) == HTS_CATALOG_INVALID);
  source.metadata_json = "{\"unknown\":\"雪\"}";

  oversized = (char *) malloc(HTS_CATALOG_METADATA_MAX + 3U);
  CHECK(oversized != NULL);
  oversized[0] = '{';
  memset(oversized + 1, 'x', HTS_CATALOG_METADATA_MAX);
  oversized[HTS_CATALOG_METADATA_MAX + 1U] = '}';
  oversized[HTS_CATALOG_METADATA_MAX + 2U] = '\0';
  source.metadata_json = oversized;
  CHECK(hts_catalog_upsert_source(catalog, &source, NULL) == HTS_CATALOG_INVALID);
  free(oversized);
  source.metadata_json = "{\"unknown\":\"雪\"}";

  memset(&board, 0, sizeof(board));
  board.source_id = source_id + 9999;
  board.remote_id = "b";
  board.name = "board";
  CHECK(hts_catalog_upsert_board(catalog, &board, NULL) == HTS_CATALOG_ERROR);
  board.source_id = source_id;
  board.display_name = NULL;
  board.capabilities_json = "{\"threads\":true}";
  CHECK(hts_catalog_upsert_board(catalog, &board, &board_id) == HTS_CATALOG_OK);

  memset(&collection, 0, sizeof(collection));
  collection.source_id = source_id;
  collection.board_id = board_id;
  collection.kind = "thread";
  collection.remote_id = "42";
  collection.title = "Unicode ✓ thread";
  collection.lifecycle_state = "active";
  collection.metadata_json = "{}";
  CHECK(hts_catalog_upsert_collection(catalog, &collection, &collection_id) == HTS_CATALOG_OK);

  memset(&post, 0, sizeof(post));
  post.source_id = source_id;
  post.collection_id = collection_id;
  post.remote_id = "100";
  post.text = "first post — nullable subject and timestamps";
  post.deleted = 0;
  post.restricted = 0;
  post.raw_metadata_version = 2;
  post.metadata_json = "{\"adapter_field\":17}";
  CHECK(hts_catalog_upsert_post(catalog, &post, &post_id) == HTS_CATALOG_OK);

  memset(&media, 0, sizeof(media));
  media.source_id = source_id;
  media.post_id = post_id;
  media.remote_id = "asset-1";
  media.variant_kind = "original";
  media.remote_url = "https://cdn.example.invalid/a.jpg";
  media.original_filename = "../unsafe/name.jpg";
  media.extension = "jpg";
  media.mime_type = "image/jpeg";
  media.size_bytes = -1;
  media.width = -1;
  media.height = -1;
  media.availability_state = "available";
  media.metadata_json = "{}";
  CHECK(hts_catalog_upsert_media(catalog, &media, &media1_id) == HTS_CATALOG_OK);
  media.remote_id = "asset-2";
  media.remote_url = "https://cdn.example.invalid/b.jpg";
  media.original_filename = "second.jpg";
  CHECK(hts_catalog_upsert_media(catalog, &media, &media2_id) == HTS_CATALOG_OK);
  CHECK(hts_catalog_upsert_collection_media(catalog, source_id, collection_id,
                                            media1_id, 10) == HTS_CATALOG_OK);
  CHECK(hts_catalog_upsert_collection_media(catalog, source_id, collection_id,
                                            media2_id, 1) == HTS_CATALOG_OK);
  memset(&visited, 0, sizeof(visited));
  CHECK(hts_catalog_list(catalog, HTS_CATALOG_ENTITY_COLLECTION_MEDIA,
                         collection_id, collect_entry, &visited) == HTS_CATALOG_OK);
  CHECK(visited.count == 2);
  CHECK(visited.positions[0] == 1 && visited.positions[1] == 10);
  CHECK(strcmp(visited.filenames[0], "asset-2") == 0);
  CHECK(hts_catalog_sanitize_filename("../unsafe/name.jpg", sanitized,
                                      sizeof(sanitized)) == HTS_CATALOG_OK);
  CHECK(strchr(sanitized, '/') == NULL && strchr(sanitized, '\\') == NULL);

  memset(&cursor, 0, sizeof(cursor));
  cursor.source_id = source_id;
  cursor.resource_kind = "board";
  cursor.resource_id = "b";
  cursor.cursor_value = "next-page";
  cursor.etag = "W/\"etag\"";
  cursor.metadata_json = "{}";
  CHECK(hts_catalog_upsert_sync_cursor(catalog, &cursor, NULL) == HTS_CATALOG_OK);

  memset(&selection, 0, sizeof(selection));
  selection.source_id = source_id;
  selection.name = "all fixture images";
  selection.kind = "query";
  selection.definition_json = "{\"rating\":null}";
  selection.enabled = 1;
  CHECK(hts_catalog_upsert_selection(catalog, &selection, &selection_id) == HTS_CATALOG_OK);

  memset(&job, 0, sizeof(job));
  job.source_id = source_id;
  job.selection_id = selection_id;
  job.media_id = media1_id;
  job.idempotency_key = "fixture-job-1";
  job.status = "pending";
  CHECK(hts_catalog_upsert_download_job(catalog, &job, NULL) == HTS_CATALOG_OK);

  CHECK(hts_catalog_begin(catalog) == HTS_CATALOG_OK);
  source.adapter_kind = "rollback";
  source.canonical_base_url = "https://rollback.invalid";
  source.display_name = NULL;
  source.policy_json = NULL;
  source.credential_ref = NULL;
  source.metadata_json = NULL;
  CHECK(hts_catalog_upsert_source(catalog, &source, &rollback_id) == HTS_CATALOG_OK);
  board.source_id = rollback_id + 9999;
  CHECK(hts_catalog_upsert_board(catalog, &board, NULL) == HTS_CATALOG_ERROR);
  CHECK(hts_catalog_rollback(catalog) == HTS_CATALOG_OK);
  memset(&visited, 0, sizeof(visited));
  CHECK(hts_catalog_read(catalog, HTS_CATALOG_ENTITY_SOURCE, rollback_id,
                         collect_entry, &visited) == HTS_CATALOG_NOT_FOUND);

  hts_catalog_close(catalog);
  catalog = NULL;
  CHECK(hts_catalog_open(path, &catalog) == HTS_CATALOG_OK);
  memset(&visited, 0, sizeof(visited));
  CHECK(hts_catalog_read(catalog, HTS_CATALOG_ENTITY_SOURCE, source_id,
                         collect_entry, &visited) == HTS_CATALOG_OK);
  CHECK(strcmp(visited.labels[0], "画像板・更新") == 0);
  hts_catalog_close(catalog);
  (void) unlink(path);

  CHECK(make_temp_path(future_path, sizeof(future_path)) == 0);
  CHECK(create_future_database(future_path) == 0);
  future = NULL;
  CHECK(hts_catalog_open(future_path, &future) == HTS_CATALOG_UNSUPPORTED_SCHEMA);
  CHECK(future == NULL);
  (void) unlink(future_path);

  CHECK(make_temp_path(v1_path, sizeof(v1_path)) == 0);
  CHECK(create_v1_database(v1_path) == 0);
  catalog = NULL;
  CHECK(hts_catalog_open(v1_path, &catalog) == HTS_CATALOG_OK);
  CHECK(hts_catalog_schema_version(catalog, &version) == HTS_CATALOG_OK);
  CHECK(version == HTS_CATALOG_SCHEMA_VERSION);
  hts_catalog_close(catalog);
  (void) unlink(v1_path);
  return 0;
}

int main(int argc, char **argv) {
  hts_catalog *catalog;
  int version;
  if (argc == 3 && strcmp(argv[1], "--init") == 0) {
    catalog = NULL;
    if (hts_catalog_open(argv[2], &catalog) != HTS_CATALOG_OK) {
      fprintf(stderr, "unable to initialize catalog\n");
      return 1;
    }
    if (hts_catalog_schema_version(catalog, &version) != HTS_CATALOG_OK) {
      fprintf(stderr, "%s\n", hts_catalog_last_error(catalog));
      hts_catalog_close(catalog);
      return 1;
    }
    printf("catalog schema version: %d\n", version);
    hts_catalog_close(catalog);
    return 0;
  }
  if (argc != 1) {
    fprintf(stderr, "usage: %s [--init DATABASE]\n", argv[0]);
    return 2;
  }
  return run_catalog_tests();
}
