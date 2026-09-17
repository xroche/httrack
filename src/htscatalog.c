#include "htscatalog.h"

#include <sqlite3.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HTS_CATALOG_KIND_MAX 64U
#define HTS_CATALOG_LABEL_MAX 4096U
#define HTS_CATALOG_HASH_MAX 256U
#define HTS_CATALOG_MIME_MAX 255U

struct hts_catalog {
  sqlite3 *db;
  char error[512];
  int transaction_active;
};

static const char migration_1[] =
  "CREATE TABLE schema_migrations ("
  " version INTEGER PRIMARY KEY,"
  " applied_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now'))"
  ");"
  "CREATE TABLE sources ("
  " id INTEGER PRIMARY KEY,"
  " adapter_kind TEXT NOT NULL CHECK(length(adapter_kind) BETWEEN 1 AND 64),"
  " canonical_base_url TEXT NOT NULL CHECK(length(canonical_base_url) BETWEEN 1 AND 8192),"
  " display_name TEXT CHECK(display_name IS NULL OR length(display_name) <= 4096),"
  " enabled INTEGER NOT NULL DEFAULT 1 CHECK(enabled IN (0,1)),"
  " policy_json TEXT CHECK(policy_json IS NULL OR length(policy_json) <= 65536),"
  " credential_ref TEXT CHECK(credential_ref IS NULL OR length(credential_ref) <= 1024),"
  " metadata_json TEXT CHECK(metadata_json IS NULL OR length(metadata_json) <= 65536),"
  " created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')) ,"
  " updated_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')) ,"
  " UNIQUE(adapter_kind, canonical_base_url)"
  ");"
  "CREATE TABLE boards ("
  " id INTEGER PRIMARY KEY,"
  " source_id INTEGER NOT NULL,"
  " remote_id TEXT NOT NULL CHECK(length(remote_id) BETWEEN 1 AND 1024),"
  " name TEXT NOT NULL CHECK(length(name) BETWEEN 1 AND 4096),"
  " display_name TEXT CHECK(display_name IS NULL OR length(display_name) <= 4096),"
  " capabilities_json TEXT CHECK(capabilities_json IS NULL OR length(capabilities_json) <= 65536),"
  " last_seen_at TEXT,"
  " metadata_json TEXT CHECK(metadata_json IS NULL OR length(metadata_json) <= 65536),"
  " UNIQUE(source_id, remote_id),"
  " UNIQUE(id, source_id),"
  " FOREIGN KEY(source_id) REFERENCES sources(id) ON DELETE CASCADE"
  ");"
  "CREATE TABLE collections ("
  " id INTEGER PRIMARY KEY,"
  " source_id INTEGER NOT NULL,"
  " board_id INTEGER,"
  " kind TEXT NOT NULL CHECK(kind IN ('thread','pool','query')) ,"
  " remote_id TEXT NOT NULL CHECK(length(remote_id) BETWEEN 1 AND 1024),"
  " title TEXT CHECK(title IS NULL OR length(title) <= 4096),"
  " description TEXT CHECK(description IS NULL OR length(description) <= 1048576),"
  " lifecycle_state TEXT NOT NULL CHECK(length(lifecycle_state) BETWEEN 1 AND 64),"
  " created_at TEXT, updated_at TEXT, last_seen_at TEXT,"
  " metadata_json TEXT CHECK(metadata_json IS NULL OR length(metadata_json) <= 65536),"
  " UNIQUE(source_id, kind, remote_id),"
  " UNIQUE(id, source_id),"
  " FOREIGN KEY(source_id) REFERENCES sources(id) ON DELETE CASCADE,"
  " FOREIGN KEY(board_id, source_id) REFERENCES boards(id, source_id)"
  ");"
  "CREATE TABLE posts ("
  " id INTEGER PRIMARY KEY,"
  " source_id INTEGER NOT NULL,"
  " collection_id INTEGER, parent_post_id INTEGER,"
  " remote_id TEXT NOT NULL CHECK(length(remote_id) BETWEEN 1 AND 1024),"
  " subject TEXT CHECK(subject IS NULL OR length(subject) <= 4096),"
  " body_text TEXT CHECK(body_text IS NULL OR length(body_text) <= 1048576),"
  " rating TEXT CHECK(rating IS NULL OR length(rating) <= 64),"
  " created_at TEXT, updated_at TEXT, last_seen_at TEXT,"
  " deleted INTEGER NOT NULL DEFAULT 0 CHECK(deleted IN (0,1)),"
  " restricted INTEGER NOT NULL DEFAULT 0 CHECK(restricted IN (0,1)),"
  " raw_metadata_version INTEGER NOT NULL DEFAULT 1 CHECK(raw_metadata_version >= 0),"
  " metadata_json TEXT CHECK(metadata_json IS NULL OR length(metadata_json) <= 65536),"
  " UNIQUE(source_id, remote_id),"
  " UNIQUE(id, source_id),"
  " FOREIGN KEY(source_id) REFERENCES sources(id) ON DELETE CASCADE,"
  " FOREIGN KEY(collection_id, source_id) REFERENCES collections(id, source_id),"
  " FOREIGN KEY(parent_post_id, source_id) REFERENCES posts(id, source_id)"
  ");"
  "CREATE TABLE media ("
  " id INTEGER PRIMARY KEY,"
  " source_id INTEGER NOT NULL, post_id INTEGER NOT NULL,"
  " remote_id TEXT NOT NULL CHECK(length(remote_id) BETWEEN 1 AND 1024),"
  " variant_kind TEXT NOT NULL CHECK(variant_kind IN ('original','sample','preview')) ,"
  " remote_url TEXT NOT NULL CHECK(length(remote_url) BETWEEN 1 AND 8192),"
  " original_filename TEXT CHECK(original_filename IS NULL OR length(original_filename) <= 1024),"
  " extension TEXT CHECK(extension IS NULL OR length(extension) <= 32),"
  " mime_type TEXT CHECK(mime_type IS NULL OR length(mime_type) <= 255),"
  " size_bytes INTEGER CHECK(size_bytes IS NULL OR size_bytes >= 0),"
  " width INTEGER CHECK(width IS NULL OR width >= 0),"
  " height INTEGER CHECK(height IS NULL OR height >= 0),"
  " remote_hash TEXT CHECK(remote_hash IS NULL OR length(remote_hash) <= 256),"
  " hash_algorithm TEXT CHECK(hash_algorithm IS NULL OR length(hash_algorithm) <= 64),"
  " availability_state TEXT NOT NULL CHECK(length(availability_state) BETWEEN 1 AND 64),"
  " metadata_json TEXT CHECK(metadata_json IS NULL OR length(metadata_json) <= 65536),"
  " UNIQUE(source_id, remote_id, variant_kind),"
  " UNIQUE(id, source_id),"
  " FOREIGN KEY(source_id) REFERENCES sources(id) ON DELETE CASCADE,"
  " FOREIGN KEY(post_id, source_id) REFERENCES posts(id, source_id) ON DELETE CASCADE"
  ");"
  "CREATE TABLE collection_media ("
  " source_id INTEGER NOT NULL, collection_id INTEGER NOT NULL, media_id INTEGER NOT NULL,"
  " position INTEGER NOT NULL CHECK(position >= 0),"
  " added_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')) ,"
  " PRIMARY KEY(collection_id, media_id),"
  " UNIQUE(collection_id, position),"
  " FOREIGN KEY(collection_id, source_id) REFERENCES collections(id, source_id) ON DELETE CASCADE,"
  " FOREIGN KEY(media_id, source_id) REFERENCES media(id, source_id) ON DELETE CASCADE"
  ");"
  "CREATE TABLE sync_cursors ("
  " id INTEGER PRIMARY KEY, source_id INTEGER NOT NULL,"
  " resource_kind TEXT NOT NULL CHECK(length(resource_kind) BETWEEN 1 AND 64),"
  " resource_id TEXT NOT NULL CHECK(length(resource_id) BETWEEN 1 AND 1024),"
  " cursor_value TEXT CHECK(cursor_value IS NULL OR length(cursor_value) <= 8192),"
  " etag TEXT CHECK(etag IS NULL OR length(etag) <= 4096),"
  " last_modified TEXT CHECK(last_modified IS NULL OR length(last_modified) <= 4096),"
  " last_attempt_at TEXT, last_success_at TEXT,"
  " error_state TEXT CHECK(error_state IS NULL OR length(error_state) <= 4096),"
  " metadata_json TEXT CHECK(metadata_json IS NULL OR length(metadata_json) <= 65536),"
  " UNIQUE(source_id, resource_kind, resource_id),"
  " FOREIGN KEY(source_id) REFERENCES sources(id) ON DELETE CASCADE"
  ");"
  "CREATE TABLE selections ("
  " id INTEGER PRIMARY KEY, source_id INTEGER NOT NULL,"
  " name TEXT NOT NULL CHECK(length(name) BETWEEN 1 AND 4096),"
  " kind TEXT NOT NULL CHECK(length(kind) BETWEEN 1 AND 64),"
  " definition_json TEXT NOT NULL CHECK(length(definition_json) <= 65536),"
  " enabled INTEGER NOT NULL DEFAULT 1 CHECK(enabled IN (0,1)),"
  " created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')) ,"
  " updated_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')) ,"
  " UNIQUE(source_id, name),"
  " UNIQUE(id, source_id),"
  " FOREIGN KEY(source_id) REFERENCES sources(id) ON DELETE CASCADE"
  ");"
  "CREATE TABLE download_jobs ("
  " id INTEGER PRIMARY KEY, source_id INTEGER NOT NULL,"
  " selection_id INTEGER, media_id INTEGER,"
  " idempotency_key TEXT NOT NULL CHECK(length(idempotency_key) BETWEEN 1 AND 1024),"
  " status TEXT NOT NULL CHECK(status IN ('pending','running','complete','failed','cancelled')) ,"
  " target_path TEXT CHECK(target_path IS NULL OR length(target_path) <= 8192),"
  " error_state TEXT CHECK(error_state IS NULL OR length(error_state) <= 4096),"
  " created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')) ,"
  " updated_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')) ,"
  " UNIQUE(source_id, idempotency_key),"
  " FOREIGN KEY(source_id) REFERENCES sources(id) ON DELETE CASCADE,"
  " FOREIGN KEY(selection_id, source_id) REFERENCES selections(id, source_id),"
  " FOREIGN KEY(media_id, source_id) REFERENCES media(id, source_id)"
  ");"
  "CREATE INDEX boards_source_idx ON boards(source_id);"
  "CREATE INDEX collections_board_idx ON collections(board_id);"
  "CREATE INDEX posts_collection_idx ON posts(collection_id);"
  "CREATE INDEX posts_parent_idx ON posts(parent_post_id);"
  "CREATE INDEX media_post_idx ON media(post_id);"
  "CREATE INDEX jobs_status_idx ON download_jobs(status);"
  "INSERT INTO schema_migrations(version) VALUES(1);"
  "PRAGMA user_version=1;";

static const char migration_2[] =
  "ALTER TABLE sync_cursors ADD COLUMN cursor_version INTEGER NOT NULL DEFAULT 1 "
  "CHECK(cursor_version >= 1);"
  "ALTER TABLE sync_cursors ADD COLUMN adapter_version TEXT NOT NULL DEFAULT 'legacy' "
  "CHECK(length(adapter_version) BETWEEN 1 AND 128);"
  "ALTER TABLE sync_cursors ADD COLUMN last_attempt_ms INTEGER;"
  "ALTER TABLE sync_cursors ADD COLUMN last_success_ms INTEGER;"
  "INSERT INTO schema_migrations(version) VALUES(2);"
  "PRAGMA user_version=2;";

static const char migration_3[] =
  "ALTER TABLE posts ADD COLUMN position INTEGER NOT NULL DEFAULT 0 "
  "CHECK(position >= 0);"
  "CREATE TABLE resource_states ("
  " id INTEGER PRIMARY KEY, source_id INTEGER NOT NULL,"
  " resource_kind TEXT NOT NULL CHECK(length(resource_kind) BETWEEN 1 AND 64),"
  " resource_id TEXT NOT NULL CHECK(length(resource_id) BETWEEN 1 AND 1024),"
  " parent_remote_id TEXT CHECK(parent_remote_id IS NULL OR length(parent_remote_id) <= 1024),"
  " remote_version TEXT CHECK(remote_version IS NULL OR length(remote_version) <= 1024),"
  " synchronized_version TEXT CHECK(synchronized_version IS NULL OR length(synchronized_version) <= 1024),"
  " lifecycle_state TEXT NOT NULL CHECK(length(lifecycle_state) BETWEEN 1 AND 64),"
  " synchronized_state TEXT CHECK(synchronized_state IS NULL OR length(synchronized_state) <= 64),"
  " missing_count INTEGER NOT NULL DEFAULT 0 CHECK(missing_count >= 0),"
  " last_seen_ms INTEGER, last_checked_ms INTEGER,"
  " error_state TEXT CHECK(error_state IS NULL OR length(error_state) <= 4096),"
  " metadata_json TEXT CHECK(metadata_json IS NULL OR length(metadata_json) <= 65536),"
  " UNIQUE(source_id,resource_kind,resource_id),"
  " FOREIGN KEY(source_id) REFERENCES sources(id) ON DELETE CASCADE"
  ");"
  "CREATE INDEX resource_states_parent_idx ON "
  "resource_states(source_id,resource_kind,parent_remote_id);"
  "INSERT INTO schema_migrations(version) VALUES(3);"
  "PRAGMA user_version=3;";

static const char migration_4[] =
  "ALTER TABLE sources ADD COLUMN deleted_at TEXT;"
  "CREATE TABLE selection_collections ("
  " selection_id INTEGER NOT NULL, source_id INTEGER NOT NULL,"
  " collection_id INTEGER NOT NULL, position INTEGER NOT NULL CHECK(position >= 0),"
  " frozen_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')) ,"
  " PRIMARY KEY(selection_id,collection_id),"
  " UNIQUE(selection_id,position),"
  " FOREIGN KEY(selection_id,source_id) REFERENCES selections(id,source_id) ON DELETE CASCADE,"
  " FOREIGN KEY(collection_id,source_id) REFERENCES collections(id,source_id)"
  ");"
  "CREATE TABLE selection_posts ("
  " selection_id INTEGER NOT NULL, source_id INTEGER NOT NULL,"
  " post_id INTEGER NOT NULL, position INTEGER NOT NULL CHECK(position >= 0),"
  " frozen_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')) ,"
  " PRIMARY KEY(selection_id,post_id),"
  " UNIQUE(selection_id,position),"
  " FOREIGN KEY(selection_id,source_id) REFERENCES selections(id,source_id) ON DELETE CASCADE,"
  " FOREIGN KEY(post_id,source_id) REFERENCES posts(id,source_id)"
  ");"
  "CREATE TABLE selection_media ("
  " selection_id INTEGER NOT NULL, source_id INTEGER NOT NULL,"
  " media_id INTEGER NOT NULL, position INTEGER NOT NULL CHECK(position >= 0),"
  " frozen_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')) ,"
  " PRIMARY KEY(selection_id,media_id),"
  " UNIQUE(selection_id,position),"
  " FOREIGN KEY(selection_id,source_id) REFERENCES selections(id,source_id) ON DELETE CASCADE,"
  " FOREIGN KEY(media_id,source_id) REFERENCES media(id,source_id)"
  ");"
  "CREATE INDEX selection_collections_collection_idx ON selection_collections(collection_id);"
  "CREATE INDEX selection_posts_post_idx ON selection_posts(post_id);"
  "CREATE INDEX selection_media_media_idx ON selection_media(media_id);"
  "INSERT INTO schema_migrations(version) VALUES(4);"
  "PRAGMA user_version=4;";

static void set_error(hts_catalog *catalog, const char *format, ...) {
  va_list args;
  if (catalog == NULL) {
    return;
  }
  va_start(args, format);
  (void) vsnprintf(catalog->error, sizeof(catalog->error), format, args);
  va_end(args);
}

static int prepare(hts_catalog *catalog, const char *sql, sqlite3_stmt **stmt) {
  int rc;
  rc = sqlite3_prepare_v2(catalog->db, sql, -1, stmt, NULL);
  if (rc != SQLITE_OK) {
    set_error(catalog, "%s", sqlite3_errmsg(catalog->db));
    return HTS_CATALOG_ERROR;
  }
  return HTS_CATALOG_OK;
}

static int execute_script(hts_catalog *catalog, const char *sql) {
  const char *tail;
  sqlite3_stmt *stmt;
  int rc;
  tail = sql;
  while (tail != NULL && *tail != '\0') {
    stmt = NULL;
    rc = sqlite3_prepare_v2(catalog->db, tail, -1, &stmt, &tail);
    if (rc != SQLITE_OK) {
      set_error(catalog, "%s", sqlite3_errmsg(catalog->db));
      return HTS_CATALOG_ERROR;
    }
    if (stmt == NULL) {
      continue;
    }
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
      set_error(catalog, "%s", sqlite3_errmsg(catalog->db));
      (void) sqlite3_finalize(stmt);
      return HTS_CATALOG_ERROR;
    }
    while (rc == SQLITE_ROW) {
      rc = sqlite3_step(stmt);
    }
    if (rc != SQLITE_DONE) {
      set_error(catalog, "%s", sqlite3_errmsg(catalog->db));
      (void) sqlite3_finalize(stmt);
      return HTS_CATALOG_ERROR;
    }
    (void) sqlite3_finalize(stmt);
  }
  return HTS_CATALOG_OK;
}

static int get_user_version(hts_catalog *catalog, int *version) {
  sqlite3_stmt *stmt;
  int rc;
  if (prepare(catalog, "PRAGMA user_version", &stmt) != HTS_CATALOG_OK) {
    return HTS_CATALOG_ERROR;
  }
  rc = sqlite3_step(stmt);
  if (rc != SQLITE_ROW) {
    set_error(catalog, "%s", sqlite3_errmsg(catalog->db));
    (void) sqlite3_finalize(stmt);
    return HTS_CATALOG_ERROR;
  }
  *version = sqlite3_column_int(stmt, 0);
  (void) sqlite3_finalize(stmt);
  return HTS_CATALOG_OK;
}

static int migrate(hts_catalog *catalog) {
  int version;
  int rc;
  if (get_user_version(catalog, &version) != HTS_CATALOG_OK) {
    return HTS_CATALOG_ERROR;
  }
  if (version > HTS_CATALOG_SCHEMA_VERSION) {
    set_error(catalog, "catalog schema %d is newer than supported schema %d",
              version, HTS_CATALOG_SCHEMA_VERSION);
    return HTS_CATALOG_UNSUPPORTED_SCHEMA;
  }
  if (version == HTS_CATALOG_SCHEMA_VERSION) {
    return HTS_CATALOG_OK;
  }
  rc = execute_script(catalog, "BEGIN IMMEDIATE");
  if (rc == HTS_CATALOG_OK && version < 1) {
    rc = execute_script(catalog, migration_1);
  }
  if (rc == HTS_CATALOG_OK && version < 2) {
    rc = execute_script(catalog, migration_2);
  }
  if (rc == HTS_CATALOG_OK && version < 3) {
    rc = execute_script(catalog, migration_3);
  }
  if (rc == HTS_CATALOG_OK && version < 4) {
    rc = execute_script(catalog, migration_4);
  }
  if (rc == HTS_CATALOG_OK) {
    rc = execute_script(catalog, "COMMIT");
  } else {
    (void) execute_script(catalog, "ROLLBACK");
  }
  return rc;
}

static int valid_text(const char *value, size_t maximum, int required) {
  size_t length;
  if (value == NULL) {
    return !required;
  }
  length = strlen(value);
  return length <= maximum && (!required || length != 0U);
}

static int ascii_equal_nocase(const char *left, size_t left_length,
                              const char *right) {
  size_t i;
  if (strlen(right) != left_length) {
    return 0;
  }
  for (i = 0U; i < left_length; i++) {
    unsigned char a;
    unsigned char b;
    a = (unsigned char) left[i];
    b = (unsigned char) right[i];
    if (a >= 'A' && a <= 'Z') a = (unsigned char) (a - 'A' + 'a');
    if (b >= 'A' && b <= 'Z') b = (unsigned char) (b - 'A' + 'a');
    if (a != b) return 0;
  }
  return 1;
}

static int forbidden_secret_key(const char *key, size_t length) {
  static const char *names[] = {
    "api_key", "apikey", "authorization", "cookie", "password",
    "secret", "token", "access_token", "refresh_token"
  };
  size_t i;
  for (i = 0U; i < sizeof(names) / sizeof(names[0]); i++) {
    if (ascii_equal_nocase(key, length, names[i])) return 1;
  }
  return 0;
}

static int json_contains_secret_key(const char *value) {
  const char *cursor;
  cursor = value;
  while (cursor != NULL && *cursor != '\0') {
    const char *key;
    const char *end;
    const char *after;
    key = strchr(cursor, '"');
    if (key == NULL) break;
    key++;
    end = key;
    while (*end != '\0' && *end != '"') {
      if (*end == '\\' && end[1] != '\0') end++;
      end++;
    }
    if (*end == '\0') break;
    after = end + 1;
    while (*after == ' ' || *after == '\t' || *after == '\r' || *after == '\n')
      after++;
    if (*after == ':' && forbidden_secret_key(key, (size_t) (end - key)))
      return 1;
    cursor = end + 1;
  }
  return 0;
}

static int valid_json(const char *value, int required) {
  size_t length;
  const char *end;
  if (value == NULL) {
    return !required;
  }
  length = strlen(value);
  if (length > HTS_CATALOG_METADATA_MAX || length < 2U) {
    return 0;
  }
  while (*value == ' ' || *value == '\t' || *value == '\r' || *value == '\n') {
    value++;
  }
  end = value + strlen(value);
  while (end > value && (end[-1] == ' ' || end[-1] == '\t' ||
                         end[-1] == '\r' || end[-1] == '\n')) {
    end--;
  }
  return end > value && !json_contains_secret_key(value) &&
         ((*value == '{' && end[-1] == '}') ||
          (*value == '[' && end[-1] == ']'));
}

static int valid_url(const char *value) {
  const char *scheme;
  const char *authority_end;
  const char *cursor;
  if (!valid_text(value, HTS_CATALOG_URL_MAX, 1)) return 0;
  for (cursor = value; *cursor != '\0'; cursor++) {
    if ((unsigned char) *cursor < 32U || (unsigned char) *cursor == 127U)
      return 0;
  }
  scheme = strstr(value, "://");
  if (scheme != NULL) {
    authority_end = strchr(scheme + 3, '/');
    if (authority_end == NULL) authority_end = value + strlen(value);
    for (cursor = scheme + 3; cursor < authority_end; cursor++) {
      if (*cursor == '@') return 0;
    }
  }
  return 1;
}

static int valid_credential_ref(const char *value) {
  static const char *prefixes[] = {
    "env:", "file:", "keyring:", "config:"
  };
  size_t i;
  if (value == NULL || *value == '\0') {
    return 1;
  }
  if (!valid_text(value, HTS_CATALOG_REMOTE_ID_MAX, 1)) {
    return 0;
  }
  for (i = 0U; i < sizeof(prefixes) / sizeof(prefixes[0]); i++) {
    size_t length;
    length = strlen(prefixes[i]);
    if (strncmp(value, prefixes[i], length) == 0 && value[length] != '\0') {
      return 1;
    }
  }
  return 0;
}

static int bind_text(sqlite3_stmt *stmt, int index, const char *value) {
  if (value == NULL) {
    return sqlite3_bind_null(stmt, index);
  }
  return sqlite3_bind_text(stmt, index, value, -1, SQLITE_TRANSIENT);
}

static int finish_statement(hts_catalog *catalog, sqlite3_stmt *stmt) {
  int rc;
  rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    set_error(catalog, "%s", sqlite3_errmsg(catalog->db));
    (void) sqlite3_finalize(stmt);
    return HTS_CATALOG_ERROR;
  }
  (void) sqlite3_finalize(stmt);
  return HTS_CATALOG_OK;
}

static int select_id(hts_catalog *catalog, const char *sql,
                     int64_t value1, const char *value2,
                     const char *value3, const char *value4,
                     int64_t *out_id) {
  sqlite3_stmt *stmt;
  int rc;
  if (prepare(catalog, sql, &stmt) != HTS_CATALOG_OK) {
    return HTS_CATALOG_ERROR;
  }
  if (value1 != 0) {
    (void) sqlite3_bind_int64(stmt, 1, value1);
  } else {
    (void) bind_text(stmt, 1, value2);
    value2 = value3;
    value3 = value4;
  }
  if (value2 != NULL) {
    (void) bind_text(stmt, 2, value2);
  }
  if (value3 != NULL) {
    (void) bind_text(stmt, 3, value3);
  }
  rc = sqlite3_step(stmt);
  if (rc != SQLITE_ROW) {
    set_error(catalog, "%s", rc == SQLITE_DONE ? "upsert key was not found" :
              sqlite3_errmsg(catalog->db));
    (void) sqlite3_finalize(stmt);
    return HTS_CATALOG_ERROR;
  }
  if (out_id != NULL) {
    *out_id = (int64_t) sqlite3_column_int64(stmt, 0);
  }
  (void) sqlite3_finalize(stmt);
  return HTS_CATALOG_OK;
}

int hts_catalog_open(const char *path, hts_catalog **out_catalog) {
  hts_catalog *catalog;
  int rc;
  if (out_catalog == NULL || !valid_text(path, HTS_CATALOG_URL_MAX, 1)) {
    return HTS_CATALOG_INVALID;
  }
  *out_catalog = NULL;
  catalog = (hts_catalog *) calloc(1U, sizeof(*catalog));
  if (catalog == NULL) {
    return HTS_CATALOG_ERROR;
  }
  rc = sqlite3_open_v2(path, &catalog->db,
                       SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                       SQLITE_OPEN_FULLMUTEX, NULL);
  if (rc != SQLITE_OK) {
    set_error(catalog, "%s", sqlite3_errmsg(catalog->db));
    hts_catalog_close(catalog);
    return HTS_CATALOG_ERROR;
  }
  (void) sqlite3_busy_timeout(catalog->db, 5000);
  if (execute_script(catalog, "PRAGMA foreign_keys=ON; PRAGMA trusted_schema=OFF") !=
      HTS_CATALOG_OK) {
    hts_catalog_close(catalog);
    return HTS_CATALOG_ERROR;
  }
  rc = migrate(catalog);
  if (rc != HTS_CATALOG_OK) {
    hts_catalog_close(catalog);
    return rc;
  }
  *out_catalog = catalog;
  return HTS_CATALOG_OK;
}

void hts_catalog_close(hts_catalog *catalog) {
  if (catalog != NULL) {
    if (catalog->db != NULL) {
      (void) sqlite3_close(catalog->db);
    }
    free(catalog);
  }
}

const char *hts_catalog_last_error(const hts_catalog *catalog) {
  return catalog == NULL ? "no catalog" : catalog->error;
}

int hts_catalog_schema_version(hts_catalog *catalog, int *out_version) {
  if (catalog == NULL || out_version == NULL) {
    return HTS_CATALOG_INVALID;
  }
  return get_user_version(catalog, out_version);
}

int hts_catalog_begin(hts_catalog *catalog) {
  int rc;
  if (catalog == NULL || catalog->transaction_active) {
    return HTS_CATALOG_INVALID;
  }
  rc = execute_script(catalog, "BEGIN IMMEDIATE");
  if (rc == HTS_CATALOG_OK) {
    catalog->transaction_active = 1;
  }
  return rc;
}

int hts_catalog_commit(hts_catalog *catalog) {
  int rc;
  if (catalog == NULL || !catalog->transaction_active) {
    return HTS_CATALOG_INVALID;
  }
  rc = execute_script(catalog, "COMMIT");
  if (rc == HTS_CATALOG_OK) {
    catalog->transaction_active = 0;
  }
  return rc;
}

int hts_catalog_rollback(hts_catalog *catalog) {
  int rc;
  if (catalog == NULL || !catalog->transaction_active) {
    return HTS_CATALOG_INVALID;
  }
  rc = execute_script(catalog, "ROLLBACK");
  if (rc == HTS_CATALOG_OK) {
    catalog->transaction_active = 0;
  }
  return rc;
}

int hts_catalog_sanitize_filename(const char *input, char *output,
                                  size_t output_size) {
  size_t input_length;
  size_t i;
  size_t j;
  unsigned char byte;
  if (input == NULL || output == NULL || output_size < 2U) {
    return HTS_CATALOG_INVALID;
  }
  input_length = strlen(input);
  if (input_length == 0U || input_length > HTS_CATALOG_FILENAME_MAX) {
    return HTS_CATALOG_INVALID;
  }
  j = 0U;
  for (i = 0U; i < input_length && j + 1U < output_size; i++) {
    byte = (unsigned char) input[i];
    if (byte < 32U || byte == 127U || byte == '/' || byte == '\\' ||
        byte == ':' || byte == '*' || byte == '?' || byte == '"' ||
        byte == '<' || byte == '>' || byte == '|') {
      output[j++] = '_';
    } else {
      output[j++] = (char) byte;
    }
  }
  if (i != input_length) {
    return HTS_CATALOG_INVALID;
  }
  output[j] = '\0';
  if (strcmp(output, ".") == 0 || strcmp(output, "..") == 0) {
    return HTS_CATALOG_INVALID;
  }
  return HTS_CATALOG_OK;
}

int hts_catalog_upsert_source(hts_catalog *catalog,
                              const hts_catalog_source *source,
                              int64_t *out_id) {
  static const char sql[] =
    "INSERT INTO sources(adapter_kind,canonical_base_url,display_name,enabled,"
    "policy_json,credential_ref,metadata_json) VALUES(?,?,?,?,?,?,?) "
    "ON CONFLICT(adapter_kind,canonical_base_url) DO UPDATE SET "
    "display_name=excluded.display_name,enabled=excluded.enabled,"
    "policy_json=excluded.policy_json,credential_ref=excluded.credential_ref,"
    "metadata_json=excluded.metadata_json,deleted_at=NULL,"
    "updated_at=strftime('%Y-%m-%dT%H:%M:%fZ','now')";
  sqlite3_stmt *stmt;
  if (catalog == NULL || source == NULL ||
      !valid_text(source->adapter_kind, HTS_CATALOG_KIND_MAX, 1) ||
      !valid_url(source->canonical_base_url) ||
      !valid_text(source->display_name, HTS_CATALOG_LABEL_MAX, 0) ||
      (source->enabled != 0 && source->enabled != 1) ||
      !valid_json(source->policy_json, 0) ||
      !valid_credential_ref(source->credential_ref) ||
      !valid_json(source->metadata_json, 0)) {
    return HTS_CATALOG_INVALID;
  }
  if (prepare(catalog, sql, &stmt) != HTS_CATALOG_OK) {
    return HTS_CATALOG_ERROR;
  }
  (void) bind_text(stmt, 1, source->adapter_kind);
  (void) bind_text(stmt, 2, source->canonical_base_url);
  (void) bind_text(stmt, 3, source->display_name);
  (void) sqlite3_bind_int(stmt, 4, source->enabled);
  (void) bind_text(stmt, 5, source->policy_json);
  (void) bind_text(stmt, 6, source->credential_ref);
  (void) bind_text(stmt, 7, source->metadata_json);
  if (finish_statement(catalog, stmt) != HTS_CATALOG_OK) {
    return HTS_CATALOG_ERROR;
  }
  return select_id(catalog,
                   "SELECT id FROM sources WHERE adapter_kind=? AND canonical_base_url=?",
                   0, source->adapter_kind, source->canonical_base_url, NULL,
                   out_id);
}

int hts_catalog_upsert_board(hts_catalog *catalog,
                             const hts_catalog_board *board,
                             int64_t *out_id) {
  static const char sql[] =
    "INSERT INTO boards(source_id,remote_id,name,display_name,capabilities_json,"
    "last_seen_at,metadata_json) VALUES(?,?,?,?,?,?,?) "
    "ON CONFLICT(source_id,remote_id) DO UPDATE SET name=excluded.name,"
    "display_name=excluded.display_name,capabilities_json=excluded.capabilities_json,"
    "last_seen_at=excluded.last_seen_at,metadata_json=excluded.metadata_json";
  sqlite3_stmt *stmt;
  if (catalog == NULL || board == NULL || board->source_id <= 0 ||
      !valid_text(board->remote_id, HTS_CATALOG_REMOTE_ID_MAX, 1) ||
      !valid_text(board->name, HTS_CATALOG_LABEL_MAX, 1) ||
      !valid_text(board->display_name, HTS_CATALOG_LABEL_MAX, 0) ||
      !valid_json(board->capabilities_json, 0) ||
      !valid_text(board->last_seen_at, 128U, 0) ||
      !valid_json(board->metadata_json, 0)) {
    return HTS_CATALOG_INVALID;
  }
  if (prepare(catalog, sql, &stmt) != HTS_CATALOG_OK) return HTS_CATALOG_ERROR;
  (void) sqlite3_bind_int64(stmt, 1, board->source_id);
  (void) bind_text(stmt, 2, board->remote_id);
  (void) bind_text(stmt, 3, board->name);
  (void) bind_text(stmt, 4, board->display_name);
  (void) bind_text(stmt, 5, board->capabilities_json);
  (void) bind_text(stmt, 6, board->last_seen_at);
  (void) bind_text(stmt, 7, board->metadata_json);
  if (finish_statement(catalog, stmt) != HTS_CATALOG_OK) return HTS_CATALOG_ERROR;
  return select_id(catalog, "SELECT id FROM boards WHERE source_id=? AND remote_id=?",
                   board->source_id, board->remote_id, NULL, NULL, out_id);
}

int hts_catalog_upsert_collection(hts_catalog *catalog,
                                  const hts_catalog_collection *collection,
                                  int64_t *out_id) {
  static const char sql[] =
    "INSERT INTO collections(source_id,board_id,kind,remote_id,title,description,"
    "lifecycle_state,created_at,updated_at,last_seen_at,metadata_json) "
    "VALUES(?,NULLIF(?,0),?,?,?,?,?,?,?,?,?) "
    "ON CONFLICT(source_id,kind,remote_id) DO UPDATE SET board_id=excluded.board_id,"
    "title=excluded.title,description=excluded.description,"
    "lifecycle_state=excluded.lifecycle_state,created_at=excluded.created_at,"
    "updated_at=excluded.updated_at,last_seen_at=excluded.last_seen_at,"
    "metadata_json=excluded.metadata_json";
  sqlite3_stmt *stmt;
  if (catalog == NULL || collection == NULL || collection->source_id <= 0 ||
      collection->board_id < 0 ||
      (!valid_text(collection->kind, HTS_CATALOG_KIND_MAX, 1) ||
       (strcmp(collection->kind, "thread") != 0 &&
        strcmp(collection->kind, "pool") != 0 &&
        strcmp(collection->kind, "query") != 0)) ||
      !valid_text(collection->remote_id, HTS_CATALOG_REMOTE_ID_MAX, 1) ||
      !valid_text(collection->title, HTS_CATALOG_LABEL_MAX, 0) ||
      !valid_text(collection->description, HTS_CATALOG_TEXT_MAX, 0) ||
      !valid_text(collection->lifecycle_state, HTS_CATALOG_KIND_MAX, 1) ||
      !valid_text(collection->created_at, 128U, 0) ||
      !valid_text(collection->updated_at, 128U, 0) ||
      !valid_text(collection->last_seen_at, 128U, 0) ||
      !valid_json(collection->metadata_json, 0)) return HTS_CATALOG_INVALID;
  if (prepare(catalog, sql, &stmt) != HTS_CATALOG_OK) return HTS_CATALOG_ERROR;
  (void) sqlite3_bind_int64(stmt, 1, collection->source_id);
  (void) sqlite3_bind_int64(stmt, 2, collection->board_id);
  (void) bind_text(stmt, 3, collection->kind);
  (void) bind_text(stmt, 4, collection->remote_id);
  (void) bind_text(stmt, 5, collection->title);
  (void) bind_text(stmt, 6, collection->description);
  (void) bind_text(stmt, 7, collection->lifecycle_state);
  (void) bind_text(stmt, 8, collection->created_at);
  (void) bind_text(stmt, 9, collection->updated_at);
  (void) bind_text(stmt, 10, collection->last_seen_at);
  (void) bind_text(stmt, 11, collection->metadata_json);
  if (finish_statement(catalog, stmt) != HTS_CATALOG_OK) return HTS_CATALOG_ERROR;
  return select_id(catalog,
                   "SELECT id FROM collections WHERE source_id=? AND kind=? AND remote_id=?",
                   collection->source_id, collection->kind,
                   collection->remote_id, NULL, out_id);
}

int hts_catalog_upsert_post(hts_catalog *catalog,
                            const hts_catalog_post *post,
                            int64_t *out_id) {
  static const char sql[] =
    "INSERT INTO posts(source_id,collection_id,parent_post_id,remote_id,subject,"
    "body_text,rating,created_at,updated_at,last_seen_at,position,deleted,restricted,"
    "raw_metadata_version,metadata_json) VALUES(?,NULLIF(?,0),NULLIF(?,0),?,?,?,?,?,?,?,NULLIF(?,-1),?,?,?,?) "
    "ON CONFLICT(source_id,remote_id) DO UPDATE SET collection_id=excluded.collection_id,"
    "parent_post_id=excluded.parent_post_id,subject=excluded.subject,body_text=excluded.body_text,"
    "rating=excluded.rating,created_at=excluded.created_at,updated_at=excluded.updated_at,"
    "last_seen_at=excluded.last_seen_at,position=excluded.position,"
    "deleted=excluded.deleted,restricted=excluded.restricted,"
    "raw_metadata_version=excluded.raw_metadata_version,metadata_json=excluded.metadata_json";
  sqlite3_stmt *stmt;
  if (catalog == NULL || post == NULL || post->source_id <= 0 ||
      post->collection_id < 0 || post->parent_post_id < 0 ||
      !valid_text(post->remote_id, HTS_CATALOG_REMOTE_ID_MAX, 1) ||
      !valid_text(post->subject, HTS_CATALOG_LABEL_MAX, 0) ||
      !valid_text(post->text, HTS_CATALOG_TEXT_MAX, 0) ||
      !valid_text(post->rating, HTS_CATALOG_KIND_MAX, 0) ||
      !valid_text(post->created_at, 128U, 0) ||
      !valid_text(post->updated_at, 128U, 0) ||
      !valid_text(post->last_seen_at, 128U, 0) ||
      post->position < -1 ||
      (post->deleted != 0 && post->deleted != 1) ||
      (post->restricted != 0 && post->restricted != 1) ||
      post->raw_metadata_version < 0 || !valid_json(post->metadata_json, 0))
    return HTS_CATALOG_INVALID;
  if (prepare(catalog, sql, &stmt) != HTS_CATALOG_OK) return HTS_CATALOG_ERROR;
  (void) sqlite3_bind_int64(stmt, 1, post->source_id);
  (void) sqlite3_bind_int64(stmt, 2, post->collection_id);
  (void) sqlite3_bind_int64(stmt, 3, post->parent_post_id);
  (void) bind_text(stmt, 4, post->remote_id);
  (void) bind_text(stmt, 5, post->subject);
  (void) bind_text(stmt, 6, post->text);
  (void) bind_text(stmt, 7, post->rating);
  (void) bind_text(stmt, 8, post->created_at);
  (void) bind_text(stmt, 9, post->updated_at);
  (void) bind_text(stmt, 10, post->last_seen_at);
  (void) sqlite3_bind_int64(stmt, 11, post->position);
  (void) sqlite3_bind_int(stmt, 12, post->deleted);
  (void) sqlite3_bind_int(stmt, 13, post->restricted);
  (void) sqlite3_bind_int(stmt, 14, post->raw_metadata_version);
  (void) bind_text(stmt, 15, post->metadata_json);
  if (finish_statement(catalog, stmt) != HTS_CATALOG_OK) return HTS_CATALOG_ERROR;
  return select_id(catalog, "SELECT id FROM posts WHERE source_id=? AND remote_id=?",
                   post->source_id, post->remote_id, NULL, NULL, out_id);
}

int hts_catalog_upsert_media(hts_catalog *catalog,
                             const hts_catalog_media *media,
                             int64_t *out_id) {
  static const char sql[] =
    "INSERT INTO media(source_id,post_id,remote_id,variant_kind,remote_url,"
    "original_filename,extension,mime_type,size_bytes,width,height,remote_hash,"
    "hash_algorithm,availability_state,metadata_json) "
    "VALUES(?,?,?,?,?,?,?,?,NULLIF(?,-1),NULLIF(?,-1),NULLIF(?,-1),?,?,?,?) "
    "ON CONFLICT(source_id,remote_id,variant_kind) DO UPDATE SET post_id=excluded.post_id,"
    "remote_url=excluded.remote_url,original_filename=excluded.original_filename,"
    "extension=excluded.extension,mime_type=excluded.mime_type,size_bytes=excluded.size_bytes,"
    "width=excluded.width,height=excluded.height,remote_hash=excluded.remote_hash,"
    "hash_algorithm=excluded.hash_algorithm,availability_state=excluded.availability_state,"
    "metadata_json=excluded.metadata_json";
  sqlite3_stmt *stmt;
  char safe_filename[HTS_CATALOG_FILENAME_MAX + 1U];
  const char *filename;
  if (catalog == NULL || media == NULL || media->source_id <= 0 || media->post_id <= 0 ||
      !valid_text(media->remote_id, HTS_CATALOG_REMOTE_ID_MAX, 1) ||
      !valid_text(media->variant_kind, HTS_CATALOG_KIND_MAX, 1) ||
      (strcmp(media->variant_kind, "original") != 0 &&
       strcmp(media->variant_kind, "sample") != 0 &&
       strcmp(media->variant_kind, "preview") != 0) ||
      !valid_url(media->remote_url) ||
      !valid_text(media->extension, 32U, 0) ||
      !valid_text(media->mime_type, HTS_CATALOG_MIME_MAX, 0) ||
      media->size_bytes < -1 || media->width < -1 || media->height < -1 ||
      !valid_text(media->remote_hash, HTS_CATALOG_HASH_MAX, 0) ||
      !valid_text(media->hash_algorithm, HTS_CATALOG_KIND_MAX, 0) ||
      !valid_text(media->availability_state, HTS_CATALOG_KIND_MAX, 1) ||
      !valid_json(media->metadata_json, 0)) return HTS_CATALOG_INVALID;
  filename = NULL;
  if (media->original_filename != NULL) {
    if (hts_catalog_sanitize_filename(media->original_filename, safe_filename,
                                      sizeof(safe_filename)) != HTS_CATALOG_OK)
      return HTS_CATALOG_INVALID;
    filename = safe_filename;
  }
  if (prepare(catalog, sql, &stmt) != HTS_CATALOG_OK) return HTS_CATALOG_ERROR;
  (void) sqlite3_bind_int64(stmt, 1, media->source_id);
  (void) sqlite3_bind_int64(stmt, 2, media->post_id);
  (void) bind_text(stmt, 3, media->remote_id);
  (void) bind_text(stmt, 4, media->variant_kind);
  (void) bind_text(stmt, 5, media->remote_url);
  (void) bind_text(stmt, 6, filename);
  (void) bind_text(stmt, 7, media->extension);
  (void) bind_text(stmt, 8, media->mime_type);
  (void) sqlite3_bind_int64(stmt, 9, media->size_bytes);
  (void) sqlite3_bind_int(stmt, 10, media->width);
  (void) sqlite3_bind_int(stmt, 11, media->height);
  (void) bind_text(stmt, 12, media->remote_hash);
  (void) bind_text(stmt, 13, media->hash_algorithm);
  (void) bind_text(stmt, 14, media->availability_state);
  (void) bind_text(stmt, 15, media->metadata_json);
  if (finish_statement(catalog, stmt) != HTS_CATALOG_OK) return HTS_CATALOG_ERROR;
  return select_id(catalog,
                   "SELECT id FROM media WHERE source_id=? AND remote_id=? AND variant_kind=?",
                   media->source_id, media->remote_id, media->variant_kind, NULL,
                   out_id);
}

int hts_catalog_upsert_collection_media(hts_catalog *catalog,
                                         int64_t source_id,
                                         int64_t collection_id,
                                         int64_t media_id,
                                         int64_t position) {
  static const char sql[] =
    "INSERT INTO collection_media(source_id,collection_id,media_id,position) VALUES(?,?,?,?) "
    "ON CONFLICT(collection_id,media_id) DO UPDATE SET position=excluded.position";
  sqlite3_stmt *stmt;
  if (catalog == NULL || source_id <= 0 || collection_id <= 0 || media_id <= 0 ||
      position < 0) return HTS_CATALOG_INVALID;
  if (prepare(catalog, sql, &stmt) != HTS_CATALOG_OK) return HTS_CATALOG_ERROR;
  (void) sqlite3_bind_int64(stmt, 1, source_id);
  (void) sqlite3_bind_int64(stmt, 2, collection_id);
  (void) sqlite3_bind_int64(stmt, 3, media_id);
  (void) sqlite3_bind_int64(stmt, 4, position);
  return finish_statement(catalog, stmt);
}

int hts_catalog_upsert_sync_cursor(hts_catalog *catalog,
                                   const hts_catalog_sync_cursor *cursor,
                                   int64_t *out_id) {
  static const char sql[] =
    "INSERT INTO sync_cursors(source_id,resource_kind,resource_id,cursor_value,etag,"
    "last_modified,last_attempt_at,last_success_at,error_state,metadata_json,"
    "cursor_version,adapter_version,last_attempt_ms,last_success_ms) "
    "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?) ON CONFLICT(source_id,resource_kind,resource_id) "
    "DO UPDATE SET cursor_value=excluded.cursor_value,etag=excluded.etag,"
    "last_modified=excluded.last_modified,last_attempt_at=excluded.last_attempt_at,"
    "last_success_at=excluded.last_success_at,error_state=excluded.error_state,"
    "metadata_json=excluded.metadata_json,cursor_version=excluded.cursor_version,"
    "adapter_version=excluded.adapter_version,last_attempt_ms=excluded.last_attempt_ms,"
    "last_success_ms=excluded.last_success_ms";
  sqlite3_stmt *stmt;
  if (catalog == NULL || cursor == NULL || cursor->source_id <= 0 ||
      !valid_text(cursor->resource_kind, HTS_CATALOG_KIND_MAX, 1) ||
      !valid_text(cursor->resource_id, HTS_CATALOG_REMOTE_ID_MAX, 1) ||
      !valid_text(cursor->cursor_value, HTS_CATALOG_URL_MAX, 0) ||
      !valid_text(cursor->etag, HTS_CATALOG_LABEL_MAX, 0) ||
      !valid_text(cursor->last_modified, HTS_CATALOG_LABEL_MAX, 0) ||
      !valid_text(cursor->last_attempt_at, 128U, 0) ||
      !valid_text(cursor->last_success_at, 128U, 0) ||
      !valid_text(cursor->error_state, HTS_CATALOG_LABEL_MAX, 0) ||
      (cursor->cursor_version < 0) ||
      !valid_text(cursor->adapter_version, 128U, 0) ||
      !valid_json(cursor->metadata_json, 0)) return HTS_CATALOG_INVALID;
  if (prepare(catalog, sql, &stmt) != HTS_CATALOG_OK) return HTS_CATALOG_ERROR;
  (void) sqlite3_bind_int64(stmt, 1, cursor->source_id);
  (void) bind_text(stmt, 2, cursor->resource_kind);
  (void) bind_text(stmt, 3, cursor->resource_id);
  (void) bind_text(stmt, 4, cursor->cursor_value);
  (void) bind_text(stmt, 5, cursor->etag);
  (void) bind_text(stmt, 6, cursor->last_modified);
  (void) bind_text(stmt, 7, cursor->last_attempt_at);
  (void) bind_text(stmt, 8, cursor->last_success_at);
  (void) bind_text(stmt, 9, cursor->error_state);
  (void) bind_text(stmt, 10, cursor->metadata_json);
  (void) sqlite3_bind_int(stmt, 11,
                          cursor->cursor_version > 0 ? cursor->cursor_version : 1);
  (void) bind_text(stmt, 12, cursor->adapter_version != NULL
                             ? cursor->adapter_version : "legacy");
  if (cursor->last_attempt_ms >= 0)
    (void) sqlite3_bind_int64(stmt, 13, cursor->last_attempt_ms);
  else
    (void) sqlite3_bind_null(stmt, 13);
  if (cursor->last_success_ms >= 0)
    (void) sqlite3_bind_int64(stmt, 14, cursor->last_success_ms);
  else
    (void) sqlite3_bind_null(stmt, 14);
  if (finish_statement(catalog, stmt) != HTS_CATALOG_OK) return HTS_CATALOG_ERROR;
  return select_id(catalog,
                   "SELECT id FROM sync_cursors WHERE source_id=? AND resource_kind=? AND resource_id=?",
                   cursor->source_id, cursor->resource_kind, cursor->resource_id,
                   NULL, out_id);
}

int hts_catalog_upsert_selection(hts_catalog *catalog,
                                 const hts_catalog_selection *selection,
                                 int64_t *out_id) {
  static const char sql[] =
    "INSERT INTO selections(source_id,name,kind,definition_json,enabled) VALUES(?,?,?,?,?) "
    "ON CONFLICT(source_id,name) DO UPDATE SET kind=excluded.kind,"
    "definition_json=excluded.definition_json,enabled=excluded.enabled,"
    "updated_at=strftime('%Y-%m-%dT%H:%M:%fZ','now')";
  sqlite3_stmt *stmt;
  if (catalog == NULL || selection == NULL || selection->source_id <= 0 ||
      !valid_text(selection->name, HTS_CATALOG_LABEL_MAX, 1) ||
      !valid_text(selection->kind, HTS_CATALOG_KIND_MAX, 1) ||
      !valid_json(selection->definition_json, 1) ||
      (selection->enabled != 0 && selection->enabled != 1)) return HTS_CATALOG_INVALID;
  if (prepare(catalog, sql, &stmt) != HTS_CATALOG_OK) return HTS_CATALOG_ERROR;
  (void) sqlite3_bind_int64(stmt, 1, selection->source_id);
  (void) bind_text(stmt, 2, selection->name);
  (void) bind_text(stmt, 3, selection->kind);
  (void) bind_text(stmt, 4, selection->definition_json);
  (void) sqlite3_bind_int(stmt, 5, selection->enabled);
  if (finish_statement(catalog, stmt) != HTS_CATALOG_OK) return HTS_CATALOG_ERROR;
  return select_id(catalog, "SELECT id FROM selections WHERE source_id=? AND name=?",
                   selection->source_id, selection->name, NULL, NULL, out_id);
}

int hts_catalog_upsert_download_job(hts_catalog *catalog,
                                    const hts_catalog_download_job *job,
                                    int64_t *out_id) {
  static const char sql[] =
    "INSERT INTO download_jobs(source_id,selection_id,media_id,idempotency_key,status,"
    "target_path,error_state) VALUES(?,NULLIF(?,0),NULLIF(?,0),?,?,?,?) "
    "ON CONFLICT(source_id,idempotency_key) DO UPDATE SET selection_id=excluded.selection_id,"
    "media_id=excluded.media_id,status=excluded.status,target_path=excluded.target_path,"
    "error_state=excluded.error_state,updated_at=strftime('%Y-%m-%dT%H:%M:%fZ','now')";
  sqlite3_stmt *stmt;
  if (catalog == NULL || job == NULL || job->source_id <= 0 ||
      job->selection_id < 0 || job->media_id < 0 ||
      !valid_text(job->idempotency_key, HTS_CATALOG_REMOTE_ID_MAX, 1) ||
      !valid_text(job->status, HTS_CATALOG_KIND_MAX, 1) ||
      !valid_text(job->target_path, HTS_CATALOG_URL_MAX, 0) ||
      !valid_text(job->error_state, HTS_CATALOG_LABEL_MAX, 0)) return HTS_CATALOG_INVALID;
  if (prepare(catalog, sql, &stmt) != HTS_CATALOG_OK) return HTS_CATALOG_ERROR;
  (void) sqlite3_bind_int64(stmt, 1, job->source_id);
  (void) sqlite3_bind_int64(stmt, 2, job->selection_id);
  (void) sqlite3_bind_int64(stmt, 3, job->media_id);
  (void) bind_text(stmt, 4, job->idempotency_key);
  (void) bind_text(stmt, 5, job->status);
  (void) bind_text(stmt, 6, job->target_path);
  (void) bind_text(stmt, 7, job->error_state);
  if (finish_statement(catalog, stmt) != HTS_CATALOG_OK) return HTS_CATALOG_ERROR;
  return select_id(catalog,
                   "SELECT id FROM download_jobs WHERE source_id=? AND idempotency_key=?",
                   job->source_id, job->idempotency_key, NULL, NULL, out_id);
}

int hts_catalog_find_id(hts_catalog *catalog, hts_catalog_entity entity,
                        int64_t source_id, const char *key1,
                        const char *key2, int64_t *out_id) {
  const char *sql;
  sqlite3_stmt *stmt;
  int rc;
  if (catalog == NULL || source_id <= 0 || out_id == NULL ||
      !valid_text(key1, HTS_CATALOG_REMOTE_ID_MAX, 1) ||
      !valid_text(key2, HTS_CATALOG_REMOTE_ID_MAX, 0))
    return HTS_CATALOG_INVALID;
  switch (entity) {
    case HTS_CATALOG_ENTITY_BOARD:
      sql = "SELECT id FROM boards WHERE source_id=? AND remote_id=?";
      break;
    case HTS_CATALOG_ENTITY_COLLECTION:
      if (key2 == NULL) return HTS_CATALOG_INVALID;
      sql = "SELECT id FROM collections WHERE source_id=? AND kind=? AND remote_id=?";
      break;
    case HTS_CATALOG_ENTITY_POST:
      sql = "SELECT id FROM posts WHERE source_id=? AND remote_id=?";
      break;
    case HTS_CATALOG_ENTITY_MEDIA:
      if (key2 == NULL) return HTS_CATALOG_INVALID;
      sql = "SELECT id FROM media WHERE source_id=? AND remote_id=? AND variant_kind=?";
      break;
    default:
      return HTS_CATALOG_INVALID;
  }
  if (prepare(catalog, sql, &stmt) != HTS_CATALOG_OK)
    return HTS_CATALOG_ERROR;
  (void) sqlite3_bind_int64(stmt, 1, source_id);
  (void) bind_text(stmt, 2, key1);
  if (key2 != NULL) (void) bind_text(stmt, 3, key2);
  rc = sqlite3_step(stmt);
  if (rc == SQLITE_ROW) {
    *out_id = (int64_t) sqlite3_column_int64(stmt, 0);
    (void) sqlite3_finalize(stmt);
    return HTS_CATALOG_OK;
  }
  if (rc != SQLITE_DONE) set_error(catalog, "%s", sqlite3_errmsg(catalog->db));
  (void) sqlite3_finalize(stmt);
  return rc == SQLITE_DONE ? HTS_CATALOG_NOT_FOUND : HTS_CATALOG_ERROR;
}

static int copy_column_text(sqlite3_stmt *stmt, int column,
                            char *output, size_t output_size) {
  const unsigned char *value;
  int length;
  value = sqlite3_column_text(stmt, column);
  if (value == NULL) {
    output[0] = '\0';
    return HTS_CATALOG_OK;
  }
  length = sqlite3_column_bytes(stmt, column);
  if (length < 0 || (size_t) length >= output_size)
    return HTS_CATALOG_ERROR;
  (void) memcpy(output, value, (size_t) length);
  output[length] = '\0';
  return HTS_CATALOG_OK;
}

int hts_catalog_get_sync_cursor(hts_catalog *catalog, int64_t source_id,
                                const char *resource_kind,
                                const char *resource_id,
                                hts_catalog_sync_state *out_state) {
  static const char sql[] =
    "SELECT cursor_value,etag,last_modified,cursor_version,adapter_version,"
    "last_attempt_ms,last_success_ms FROM sync_cursors "
    "WHERE source_id=? AND resource_kind=? AND resource_id=?";
  sqlite3_stmt *stmt;
  int rc;
  if (catalog == NULL || source_id <= 0 || out_state == NULL ||
      !valid_text(resource_kind, HTS_CATALOG_KIND_MAX, 1) ||
      !valid_text(resource_id, HTS_CATALOG_REMOTE_ID_MAX, 1))
    return HTS_CATALOG_INVALID;
  (void) memset(out_state, 0, sizeof(*out_state));
  out_state->last_attempt_ms = -1;
  out_state->last_success_ms = -1;
  if (prepare(catalog, sql, &stmt) != HTS_CATALOG_OK)
    return HTS_CATALOG_ERROR;
  (void) sqlite3_bind_int64(stmt, 1, source_id);
  (void) bind_text(stmt, 2, resource_kind);
  (void) bind_text(stmt, 3, resource_id);
  rc = sqlite3_step(stmt);
  if (rc == SQLITE_DONE) {
    (void) sqlite3_finalize(stmt);
    return HTS_CATALOG_NOT_FOUND;
  }
  if (rc != SQLITE_ROW ||
      copy_column_text(stmt, 0, out_state->cursor_value,
                       sizeof(out_state->cursor_value)) != HTS_CATALOG_OK ||
      copy_column_text(stmt, 1, out_state->etag,
                       sizeof(out_state->etag)) != HTS_CATALOG_OK ||
      copy_column_text(stmt, 2, out_state->last_modified,
                       sizeof(out_state->last_modified)) != HTS_CATALOG_OK ||
      copy_column_text(stmt, 4, out_state->adapter_version,
                       sizeof(out_state->adapter_version)) != HTS_CATALOG_OK) {
    set_error(catalog, "invalid stored sync cursor");
    (void) sqlite3_finalize(stmt);
    return HTS_CATALOG_ERROR;
  }
  out_state->cursor_version = sqlite3_column_int(stmt, 3);
  if (sqlite3_column_type(stmt, 5) != SQLITE_NULL)
    out_state->last_attempt_ms = (int64_t) sqlite3_column_int64(stmt, 5);
  if (sqlite3_column_type(stmt, 6) != SQLITE_NULL)
    out_state->last_success_ms = (int64_t) sqlite3_column_int64(stmt, 6);
  (void) sqlite3_finalize(stmt);
  return HTS_CATALOG_OK;
}

int hts_catalog_upsert_resource_state(
    hts_catalog *catalog, const hts_catalog_resource_state *resource,
    int64_t *out_id) {
  static const char sql[] =
    "INSERT INTO resource_states(source_id,resource_kind,resource_id,"
    "parent_remote_id,remote_version,synchronized_version,lifecycle_state,"
    "synchronized_state,missing_count,last_seen_ms,last_checked_ms,error_state,"
    "metadata_json) VALUES(?,?,?,?,?,?,?,?,?,NULLIF(?,-1),NULLIF(?,-1),?,?) "
    "ON CONFLICT(source_id,resource_kind,resource_id) DO UPDATE SET "
    "parent_remote_id=COALESCE(excluded.parent_remote_id,parent_remote_id),"
    "remote_version=COALESCE(excluded.remote_version,remote_version),"
    "synchronized_version=COALESCE(excluded.synchronized_version,synchronized_version),"
    "lifecycle_state=excluded.lifecycle_state,"
    "synchronized_state=COALESCE(excluded.synchronized_state,synchronized_state),"
    "missing_count=excluded.missing_count,"
    "last_seen_ms=COALESCE(excluded.last_seen_ms,last_seen_ms),"
    "last_checked_ms=COALESCE(excluded.last_checked_ms,last_checked_ms),"
    "error_state=excluded.error_state,"
    "metadata_json=COALESCE(excluded.metadata_json,metadata_json)";
  sqlite3_stmt *stmt;
  if (catalog == NULL || resource == NULL || resource->source_id <= 0 ||
      !valid_text(resource->resource_kind, HTS_CATALOG_KIND_MAX, 1) ||
      !valid_text(resource->resource_id, HTS_CATALOG_REMOTE_ID_MAX, 1) ||
      !valid_text(resource->parent_remote_id, HTS_CATALOG_REMOTE_ID_MAX, 0) ||
      !valid_text(resource->remote_version, HTS_CATALOG_REMOTE_ID_MAX, 0) ||
      !valid_text(resource->synchronized_version,
                  HTS_CATALOG_REMOTE_ID_MAX, 0) ||
      !valid_text(resource->lifecycle_state, HTS_CATALOG_KIND_MAX, 1) ||
      !valid_text(resource->synchronized_state, HTS_CATALOG_KIND_MAX, 0) ||
      resource->last_seen_ms < -1 || resource->last_checked_ms < -1 ||
      !valid_text(resource->error_state, 4096U, 0) ||
      !valid_json(resource->metadata_json, 0))
    return HTS_CATALOG_INVALID;
  if (prepare(catalog, sql, &stmt) != HTS_CATALOG_OK)
    return HTS_CATALOG_ERROR;
  (void) sqlite3_bind_int64(stmt, 1, resource->source_id);
  (void) bind_text(stmt, 2, resource->resource_kind);
  (void) bind_text(stmt, 3, resource->resource_id);
  (void) bind_text(stmt, 4, resource->parent_remote_id);
  (void) bind_text(stmt, 5, resource->remote_version);
  (void) bind_text(stmt, 6, resource->synchronized_version);
  (void) bind_text(stmt, 7, resource->lifecycle_state);
  (void) bind_text(stmt, 8, resource->synchronized_state);
  (void) sqlite3_bind_int64(stmt, 9, (sqlite3_int64) resource->missing_count);
  (void) sqlite3_bind_int64(stmt, 10, resource->last_seen_ms);
  (void) sqlite3_bind_int64(stmt, 11, resource->last_checked_ms);
  (void) bind_text(stmt, 12, resource->error_state);
  (void) bind_text(stmt, 13, resource->metadata_json);
  if (finish_statement(catalog, stmt) != HTS_CATALOG_OK)
    return HTS_CATALOG_ERROR;
  return select_id(catalog,
                   "SELECT id FROM resource_states WHERE source_id=? AND resource_kind=? AND resource_id=?",
                   resource->source_id, resource->resource_kind,
                   resource->resource_id, NULL, out_id);
}

static int visit_resource_rows(hts_catalog *catalog, sqlite3_stmt *stmt,
                               hts_catalog_resource_visit_fn visit,
                               void *user, int single) {
  int rc;
  int found;
  found = 0;
  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    hts_catalog_resource_view resource;
    found = 1;
    resource.source_id = (int64_t) sqlite3_column_int64(stmt, 0);
    resource.resource_kind = (const char *) sqlite3_column_text(stmt, 1);
    resource.resource_id = (const char *) sqlite3_column_text(stmt, 2);
    resource.parent_remote_id = (const char *) sqlite3_column_text(stmt, 3);
    resource.remote_version = (const char *) sqlite3_column_text(stmt, 4);
    resource.synchronized_version = (const char *) sqlite3_column_text(stmt, 5);
    resource.lifecycle_state = (const char *) sqlite3_column_text(stmt, 6);
    resource.synchronized_state = (const char *) sqlite3_column_text(stmt, 7);
    resource.missing_count = (unsigned int) sqlite3_column_int64(stmt, 8);
    resource.last_seen_ms = sqlite3_column_type(stmt, 9) == SQLITE_NULL
                              ? -1 : (int64_t) sqlite3_column_int64(stmt, 9);
    resource.last_checked_ms = sqlite3_column_type(stmt, 10) == SQLITE_NULL
                                 ? -1 : (int64_t) sqlite3_column_int64(stmt, 10);
    resource.error_state = (const char *) sqlite3_column_text(stmt, 11);
    resource.metadata_json = (const char *) sqlite3_column_text(stmt, 12);
    if (visit(user, &resource) != 0 || single) break;
  }
  if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
    set_error(catalog, "%s", sqlite3_errmsg(catalog->db));
    (void) sqlite3_finalize(stmt);
    return HTS_CATALOG_ERROR;
  }
  (void) sqlite3_finalize(stmt);
  return found || !single ? HTS_CATALOG_OK : HTS_CATALOG_NOT_FOUND;
}

int hts_catalog_get_resource_state(
    hts_catalog *catalog, int64_t source_id, const char *resource_kind,
    const char *resource_id, hts_catalog_resource_visit_fn visit, void *user) {
  static const char sql[] =
    "SELECT source_id,resource_kind,resource_id,parent_remote_id,remote_version,"
    "synchronized_version,lifecycle_state,synchronized_state,missing_count,"
    "last_seen_ms,last_checked_ms,error_state,metadata_json FROM resource_states "
    "WHERE source_id=? AND resource_kind=? AND resource_id=?";
  sqlite3_stmt *stmt;
  if (catalog == NULL || source_id <= 0 || visit == NULL ||
      !valid_text(resource_kind, HTS_CATALOG_KIND_MAX, 1) ||
      !valid_text(resource_id, HTS_CATALOG_REMOTE_ID_MAX, 1))
    return HTS_CATALOG_INVALID;
  if (prepare(catalog, sql, &stmt) != HTS_CATALOG_OK)
    return HTS_CATALOG_ERROR;
  (void) sqlite3_bind_int64(stmt, 1, source_id);
  (void) bind_text(stmt, 2, resource_kind);
  (void) bind_text(stmt, 3, resource_id);
  return visit_resource_rows(catalog, stmt, visit, user, 1);
}

int hts_catalog_list_resource_states(
    hts_catalog *catalog, int64_t source_id, const char *resource_kind,
    const char *parent_remote_id, hts_catalog_resource_visit_fn visit,
    void *user) {
  static const char sql[] =
    "SELECT source_id,resource_kind,resource_id,parent_remote_id,remote_version,"
    "synchronized_version,lifecycle_state,synchronized_state,missing_count,"
    "last_seen_ms,last_checked_ms,error_state,metadata_json FROM resource_states "
    "WHERE source_id=? AND resource_kind=? AND (? IS NULL OR parent_remote_id=?) "
    "ORDER BY resource_id";
  sqlite3_stmt *stmt;
  if (catalog == NULL || source_id <= 0 || visit == NULL ||
      !valid_text(resource_kind, HTS_CATALOG_KIND_MAX, 1) ||
      !valid_text(parent_remote_id, HTS_CATALOG_REMOTE_ID_MAX, 0))
    return HTS_CATALOG_INVALID;
  if (prepare(catalog, sql, &stmt) != HTS_CATALOG_OK)
    return HTS_CATALOG_ERROR;
  (void) sqlite3_bind_int64(stmt, 1, source_id);
  (void) bind_text(stmt, 2, resource_kind);
  (void) bind_text(stmt, 3, parent_remote_id);
  (void) bind_text(stmt, 4, parent_remote_id);
  return visit_resource_rows(catalog, stmt, visit, user, 0);
}

int hts_catalog_set_collection_lifecycle(
    hts_catalog *catalog, int64_t source_id, const char *kind,
    const char *remote_id, const char *lifecycle_state,
    const char *media_availability_state) {
  static const char collection_sql[] =
    "UPDATE collections SET lifecycle_state=? WHERE source_id=? AND kind=? AND remote_id=?";
  static const char media_sql[] =
    "UPDATE media SET availability_state=? WHERE source_id=? AND post_id IN ("
    "SELECT p.id FROM posts p JOIN collections c ON c.id=p.collection_id "
    "WHERE c.source_id=? AND c.kind=? AND c.remote_id=?)";
  sqlite3_stmt *stmt;
  int changed;
  if (catalog == NULL || source_id <= 0 ||
      !valid_text(kind, HTS_CATALOG_KIND_MAX, 1) ||
      !valid_text(remote_id, HTS_CATALOG_REMOTE_ID_MAX, 1) ||
      !valid_text(lifecycle_state, HTS_CATALOG_KIND_MAX, 1) ||
      !valid_text(media_availability_state, HTS_CATALOG_KIND_MAX, 0))
    return HTS_CATALOG_INVALID;
  if (prepare(catalog, collection_sql, &stmt) != HTS_CATALOG_OK)
    return HTS_CATALOG_ERROR;
  (void) bind_text(stmt, 1, lifecycle_state);
  (void) sqlite3_bind_int64(stmt, 2, source_id);
  (void) bind_text(stmt, 3, kind);
  (void) bind_text(stmt, 4, remote_id);
  if (finish_statement(catalog, stmt) != HTS_CATALOG_OK)
    return HTS_CATALOG_ERROR;
  changed = sqlite3_changes(catalog->db);
  if (changed == 0) return HTS_CATALOG_NOT_FOUND;
  if (media_availability_state == NULL) return HTS_CATALOG_OK;
  if (prepare(catalog, media_sql, &stmt) != HTS_CATALOG_OK)
    return HTS_CATALOG_ERROR;
  (void) bind_text(stmt, 1, media_availability_state);
  (void) sqlite3_bind_int64(stmt, 2, source_id);
  (void) sqlite3_bind_int64(stmt, 3, source_id);
  (void) bind_text(stmt, 4, kind);
  (void) bind_text(stmt, 5, remote_id);
  return finish_statement(catalog, stmt);
}

int hts_catalog_mark_post_deleted(
    hts_catalog *catalog, int64_t source_id, const char *remote_id,
    const char *media_availability_state) {
  static const char post_sql[] =
    "UPDATE posts SET deleted=1 WHERE source_id=? AND remote_id=?";
  static const char media_sql[] =
    "UPDATE media SET availability_state=? WHERE source_id=? AND post_id IN ("
    "SELECT id FROM posts WHERE source_id=? AND remote_id=?)";
  sqlite3_stmt *stmt;
  int changed;
  if (catalog == NULL || source_id <= 0 ||
      !valid_text(remote_id, HTS_CATALOG_REMOTE_ID_MAX, 1) ||
      !valid_text(media_availability_state, HTS_CATALOG_KIND_MAX, 0))
    return HTS_CATALOG_INVALID;
  if (prepare(catalog, post_sql, &stmt) != HTS_CATALOG_OK)
    return HTS_CATALOG_ERROR;
  (void) sqlite3_bind_int64(stmt, 1, source_id);
  (void) bind_text(stmt, 2, remote_id);
  if (finish_statement(catalog, stmt) != HTS_CATALOG_OK)
    return HTS_CATALOG_ERROR;
  changed = sqlite3_changes(catalog->db);
  if (changed == 0) return HTS_CATALOG_NOT_FOUND;
  if (media_availability_state == NULL) return HTS_CATALOG_OK;
  if (prepare(catalog, media_sql, &stmt) != HTS_CATALOG_OK)
    return HTS_CATALOG_ERROR;
  (void) bind_text(stmt, 1, media_availability_state);
  (void) sqlite3_bind_int64(stmt, 2, source_id);
  (void) sqlite3_bind_int64(stmt, 3, source_id);
  (void) bind_text(stmt, 4, remote_id);
  return finish_statement(catalog, stmt);
}

int hts_catalog_clear_collection_media(
    hts_catalog *catalog, int64_t source_id, const char *kind,
    const char *remote_id) {
  static const char sql[] =
    "DELETE FROM collection_media WHERE source_id=? AND collection_id IN ("
    "SELECT id FROM collections WHERE source_id=? AND kind=? AND remote_id=?)";
  sqlite3_stmt *stmt;
  if (catalog == NULL || source_id <= 0 ||
      !valid_text(kind, HTS_CATALOG_KIND_MAX, 1) ||
      !valid_text(remote_id, HTS_CATALOG_REMOTE_ID_MAX, 1))
    return HTS_CATALOG_INVALID;
  if (prepare(catalog, sql, &stmt) != HTS_CATALOG_OK)
    return HTS_CATALOG_ERROR;
  (void) sqlite3_bind_int64(stmt, 1, source_id);
  (void) sqlite3_bind_int64(stmt, 2, source_id);
  (void) bind_text(stmt, 3, kind);
  (void) bind_text(stmt, 4, remote_id);
  return finish_statement(catalog, stmt);
}

static const char *read_sql(hts_catalog_entity entity) {
  switch (entity) {
    case HTS_CATALOG_ENTITY_SOURCE:
      return "SELECT id,0,0,0,canonical_base_url,adapter_kind,display_name,metadata_json FROM sources WHERE id=?";
    case HTS_CATALOG_ENTITY_BOARD:
      return "SELECT id,source_id,source_id,0,remote_id,NULL,name,metadata_json FROM boards WHERE id=?";
    case HTS_CATALOG_ENTITY_COLLECTION:
      return "SELECT id,source_id,COALESCE(board_id,0),0,remote_id,kind,title,metadata_json FROM collections WHERE id=?";
    case HTS_CATALOG_ENTITY_POST:
      return "SELECT id,source_id,COALESCE(collection_id,0),position,remote_id,rating,COALESCE(subject,body_text),metadata_json FROM posts WHERE id=?";
    case HTS_CATALOG_ENTITY_MEDIA:
      return "SELECT id,source_id,post_id,0,remote_id,variant_kind,original_filename,metadata_json FROM media WHERE id=?";
    case HTS_CATALOG_ENTITY_SYNC_CURSOR:
      return "SELECT id,source_id,source_id,0,resource_id,resource_kind,error_state,metadata_json FROM sync_cursors WHERE id=?";
    case HTS_CATALOG_ENTITY_SELECTION:
      return "SELECT id,source_id,source_id,0,name,kind,name,definition_json FROM selections WHERE id=?";
    case HTS_CATALOG_ENTITY_DOWNLOAD_JOB:
      return "SELECT id,source_id,COALESCE(selection_id,0),0,idempotency_key,status,target_path,error_state FROM download_jobs WHERE id=?";
    default:
      return NULL;
  }
}

static const char *list_sql(hts_catalog_entity entity) {
  switch (entity) {
    case HTS_CATALOG_ENTITY_SOURCE:
      return "SELECT id,0,0,0,canonical_base_url,adapter_kind,display_name,metadata_json FROM sources ORDER BY id";
    case HTS_CATALOG_ENTITY_BOARD:
      return "SELECT id,source_id,source_id,0,remote_id,NULL,name,metadata_json FROM boards WHERE source_id=? ORDER BY remote_id";
    case HTS_CATALOG_ENTITY_COLLECTION:
      return "SELECT id,source_id,COALESCE(board_id,0),0,remote_id,kind,title,metadata_json FROM collections WHERE source_id=? ORDER BY kind,remote_id";
    case HTS_CATALOG_ENTITY_POST:
      return "SELECT id,source_id,COALESCE(collection_id,0),position,remote_id,rating,COALESCE(subject,body_text),metadata_json FROM posts WHERE collection_id=? ORDER BY position,id";
    case HTS_CATALOG_ENTITY_MEDIA:
      return "SELECT id,source_id,post_id,0,remote_id,variant_kind,original_filename,metadata_json FROM media WHERE post_id=? ORDER BY id";
    case HTS_CATALOG_ENTITY_COLLECTION_MEDIA:
      return "SELECT m.id,cm.source_id,cm.collection_id,cm.position,m.remote_id,m.variant_kind,m.original_filename,m.metadata_json FROM collection_media cm JOIN media m ON m.id=cm.media_id WHERE cm.collection_id=? ORDER BY cm.position,m.id";
    case HTS_CATALOG_ENTITY_SYNC_CURSOR:
      return "SELECT id,source_id,source_id,0,resource_id,resource_kind,error_state,metadata_json FROM sync_cursors WHERE source_id=? ORDER BY resource_kind,resource_id";
    case HTS_CATALOG_ENTITY_SELECTION:
      return "SELECT id,source_id,source_id,0,name,kind,name,definition_json FROM selections WHERE source_id=? ORDER BY name";
    case HTS_CATALOG_ENTITY_DOWNLOAD_JOB:
      return "SELECT id,source_id,COALESCE(selection_id,0),0,idempotency_key,status,target_path,error_state FROM download_jobs WHERE (?=0 OR selection_id=?) ORDER BY id";
    default:
      return NULL;
  }
}

static int visit_rows(hts_catalog *catalog, sqlite3_stmt *stmt,
                      hts_catalog_visit_fn visit, void *user, int single) {
  int rc;
  int found;
  found = 0;
  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    hts_catalog_entry entry;
    found = 1;
    entry.id = (int64_t) sqlite3_column_int64(stmt, 0);
    entry.source_id = (int64_t) sqlite3_column_int64(stmt, 1);
    entry.parent_id = (int64_t) sqlite3_column_int64(stmt, 2);
    entry.position = (int64_t) sqlite3_column_int64(stmt, 3);
    entry.remote_id = (const char *) sqlite3_column_text(stmt, 4);
    entry.kind = (const char *) sqlite3_column_text(stmt, 5);
    entry.label = (const char *) sqlite3_column_text(stmt, 6);
    entry.metadata_json = (const char *) sqlite3_column_text(stmt, 7);
    if (visit(user, &entry) != 0 || single) {
      break;
    }
  }
  if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
    set_error(catalog, "%s", sqlite3_errmsg(catalog->db));
    (void) sqlite3_finalize(stmt);
    return HTS_CATALOG_ERROR;
  }
  (void) sqlite3_finalize(stmt);
  return found || !single ? HTS_CATALOG_OK : HTS_CATALOG_NOT_FOUND;
}

int hts_catalog_read(hts_catalog *catalog, hts_catalog_entity entity,
                     int64_t id, hts_catalog_visit_fn visit, void *user) {
  const char *sql;
  sqlite3_stmt *stmt;
  if (catalog == NULL || id <= 0 || visit == NULL) return HTS_CATALOG_INVALID;
  sql = read_sql(entity);
  if (sql == NULL) return HTS_CATALOG_INVALID;
  if (prepare(catalog, sql, &stmt) != HTS_CATALOG_OK) return HTS_CATALOG_ERROR;
  (void) sqlite3_bind_int64(stmt, 1, id);
  return visit_rows(catalog, stmt, visit, user, 1);
}

int hts_catalog_list(hts_catalog *catalog, hts_catalog_entity entity,
                     int64_t parent_id, hts_catalog_visit_fn visit,
                     void *user) {
  const char *sql;
  sqlite3_stmt *stmt;
  if (catalog == NULL || parent_id < 0 || visit == NULL) return HTS_CATALOG_INVALID;
  sql = list_sql(entity);
  if (sql == NULL) return HTS_CATALOG_INVALID;
  if (prepare(catalog, sql, &stmt) != HTS_CATALOG_OK) return HTS_CATALOG_ERROR;
  if (entity != HTS_CATALOG_ENTITY_SOURCE) {
    (void) sqlite3_bind_int64(stmt, 1, parent_id);
    if (entity == HTS_CATALOG_ENTITY_DOWNLOAD_JOB)
      (void) sqlite3_bind_int64(stmt, 2, parent_id);
  }
  return visit_rows(catalog, stmt, visit, user, 0);
}
