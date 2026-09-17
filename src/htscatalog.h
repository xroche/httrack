/*
 * HTTrack imageboard catalog persistence API.
 *
 * This module is deliberately independent from the mirror cache.  Opening a
 * catalog is always explicit and never changes normal HTTrack crawl behavior.
 */

#ifndef HTSCATALOG_H
#define HTSCATALOG_H

#include <stddef.h>
#include <stdint.h>

#include "htsglobal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HTS_CATALOG_SCHEMA_VERSION 4
#define HTS_CATALOG_METADATA_MAX (64U * 1024U)
#define HTS_CATALOG_TEXT_MAX (1024U * 1024U)
#define HTS_CATALOG_URL_MAX 8192U
#define HTS_CATALOG_REMOTE_ID_MAX 1024U
#define HTS_CATALOG_FILENAME_MAX 1024U

typedef struct hts_catalog hts_catalog;

typedef enum hts_catalog_status {
  HTS_CATALOG_OK = 0,
  HTS_CATALOG_ERROR = -1,
  HTS_CATALOG_INVALID = -2,
  HTS_CATALOG_NOT_FOUND = -3,
  HTS_CATALOG_UNSUPPORTED_SCHEMA = -4
} hts_catalog_status;

typedef enum hts_catalog_entity {
  HTS_CATALOG_ENTITY_SOURCE = 1,
  HTS_CATALOG_ENTITY_BOARD,
  HTS_CATALOG_ENTITY_COLLECTION,
  HTS_CATALOG_ENTITY_POST,
  HTS_CATALOG_ENTITY_MEDIA,
  HTS_CATALOG_ENTITY_COLLECTION_MEDIA,
  HTS_CATALOG_ENTITY_SYNC_CURSOR,
  HTS_CATALOG_ENTITY_SELECTION,
  HTS_CATALOG_ENTITY_DOWNLOAD_JOB,
  HTS_CATALOG_ENTITY_RESOURCE_STATE
} hts_catalog_entity;

typedef struct hts_catalog_source {
  const char *adapter_kind;
  const char *canonical_base_url;
  const char *display_name;
  int enabled;
  const char *policy_json;
  /* An external env/file/keyring/config reference; never a credential value. */
  const char *credential_ref;
  const char *metadata_json;
} hts_catalog_source;

typedef struct hts_catalog_board {
  int64_t source_id;
  const char *remote_id;
  const char *name;
  const char *display_name;
  const char *capabilities_json;
  const char *last_seen_at;
  const char *metadata_json;
} hts_catalog_board;

typedef struct hts_catalog_collection {
  int64_t source_id;
  int64_t board_id; /* zero means no board */
  const char *kind; /* thread, pool, or query */
  const char *remote_id;
  const char *title;
  const char *description;
  const char *lifecycle_state;
  const char *created_at;
  const char *updated_at;
  const char *last_seen_at;
  const char *metadata_json;
} hts_catalog_collection;

typedef struct hts_catalog_post {
  int64_t source_id;
  int64_t collection_id; /* zero means no collection */
  int64_t parent_post_id; /* zero means no parent */
  const char *remote_id;
  const char *subject;
  const char *text;
  const char *rating;
  const char *created_at;
  const char *updated_at;
  const char *last_seen_at;
  int64_t position; /* negative means unknown */
  int deleted;
  int restricted;
  int raw_metadata_version;
  const char *metadata_json;
} hts_catalog_post;

typedef struct hts_catalog_media {
  int64_t source_id;
  int64_t post_id;
  const char *remote_id;
  const char *variant_kind; /* original, sample, or preview */
  const char *remote_url;
  const char *original_filename;
  const char *extension;
  const char *mime_type;
  int64_t size_bytes; /* negative means unknown */
  int width;          /* negative means unknown */
  int height;         /* negative means unknown */
  const char *remote_hash;
  const char *hash_algorithm;
  const char *availability_state;
  const char *metadata_json;
} hts_catalog_media;

typedef struct hts_catalog_sync_cursor {
  int64_t source_id;
  const char *resource_kind;
  const char *resource_id;
  const char *cursor_value;
  const char *etag;
  const char *last_modified;
  const char *last_attempt_at;
  const char *last_success_at;
  const char *error_state;
  const char *metadata_json;
  int cursor_version;
  const char *adapter_version;
  int64_t last_attempt_ms;
  int64_t last_success_ms;
} hts_catalog_sync_cursor;

typedef struct hts_catalog_sync_state {
  char cursor_value[HTS_CATALOG_URL_MAX + 1U];
  char etag[4097];
  char last_modified[4097];
  char adapter_version[129];
  int cursor_version;
  int64_t last_attempt_ms;
  int64_t last_success_ms;
} hts_catalog_sync_state;

typedef struct hts_catalog_resource_state {
  int64_t source_id;
  const char *resource_kind;
  const char *resource_id;
  const char *parent_remote_id;
  const char *remote_version;
  const char *synchronized_version;
  const char *lifecycle_state;
  const char *synchronized_state;
  unsigned int missing_count;
  int64_t last_seen_ms;    /* negative means unknown */
  int64_t last_checked_ms; /* negative means unknown */
  const char *error_state;
  const char *metadata_json;
} hts_catalog_resource_state;

/* Views point into a live prepared statement and are valid only in callback. */
typedef struct hts_catalog_resource_view {
  int64_t source_id;
  const char *resource_kind;
  const char *resource_id;
  const char *parent_remote_id;
  const char *remote_version;
  const char *synchronized_version;
  const char *lifecycle_state;
  const char *synchronized_state;
  unsigned int missing_count;
  int64_t last_seen_ms;
  int64_t last_checked_ms;
  const char *error_state;
  const char *metadata_json;
} hts_catalog_resource_view;

typedef int (*hts_catalog_resource_visit_fn)(
    void *user, const hts_catalog_resource_view *resource);

typedef struct hts_catalog_selection {
  int64_t source_id;
  const char *name;
  const char *kind;
  const char *definition_json;
  int enabled;
} hts_catalog_selection;

typedef struct hts_catalog_download_job {
  int64_t source_id;
  int64_t selection_id; /* zero means no selection */
  int64_t media_id;     /* zero means no media assigned yet */
  const char *idempotency_key;
  const char *status;
  const char *target_path;
  const char *error_state;
} hts_catalog_download_job;

/* Views point into a live prepared statement and are valid only in callback. */
typedef struct hts_catalog_entry {
  int64_t id;
  int64_t source_id;
  int64_t parent_id;
  int64_t position;
  const char *remote_id;
  const char *kind;
  const char *label;
  const char *metadata_json;
} hts_catalog_entry;

typedef int (*hts_catalog_visit_fn)(void *user, const hts_catalog_entry *entry);

HTSEXT_API int hts_catalog_open(const char *path, hts_catalog **out_catalog);
HTSEXT_API void hts_catalog_close(hts_catalog *catalog);
HTSEXT_API const char *hts_catalog_last_error(const hts_catalog *catalog);
HTSEXT_API int hts_catalog_schema_version(hts_catalog *catalog, int *out_version);

HTSEXT_API int hts_catalog_begin(hts_catalog *catalog);
HTSEXT_API int hts_catalog_commit(hts_catalog *catalog);
HTSEXT_API int hts_catalog_rollback(hts_catalog *catalog);

HTSEXT_API int hts_catalog_upsert_source(hts_catalog *catalog,
                              const hts_catalog_source *source,
                              int64_t *out_id);
HTSEXT_API int hts_catalog_upsert_board(hts_catalog *catalog,
                             const hts_catalog_board *board,
                             int64_t *out_id);
HTSEXT_API int hts_catalog_upsert_collection(hts_catalog *catalog,
                                  const hts_catalog_collection *collection,
                                  int64_t *out_id);
HTSEXT_API int hts_catalog_upsert_post(hts_catalog *catalog,
                            const hts_catalog_post *post,
                            int64_t *out_id);
HTSEXT_API int hts_catalog_upsert_media(hts_catalog *catalog,
                             const hts_catalog_media *media,
                             int64_t *out_id);
HTSEXT_API int hts_catalog_upsert_collection_media(hts_catalog *catalog,
                                         int64_t source_id,
                                         int64_t collection_id,
                                         int64_t media_id,
                                         int64_t position);
HTSEXT_API int hts_catalog_upsert_sync_cursor(hts_catalog *catalog,
                                   const hts_catalog_sync_cursor *cursor,
                                   int64_t *out_id);
HTSEXT_API int hts_catalog_upsert_selection(hts_catalog *catalog,
                                 const hts_catalog_selection *selection,
                                 int64_t *out_id);
HTSEXT_API int hts_catalog_upsert_download_job(hts_catalog *catalog,
                                    const hts_catalog_download_job *job,
                                    int64_t *out_id);

HTSEXT_API int hts_catalog_find_id(hts_catalog *catalog,
                         hts_catalog_entity entity, int64_t source_id,
                         const char *key1, const char *key2,
                         int64_t *out_id);
HTSEXT_API int hts_catalog_get_sync_cursor(hts_catalog *catalog,
                                int64_t source_id,
                                const char *resource_kind,
                                const char *resource_id,
                                hts_catalog_sync_state *out_state);
HTSEXT_API int hts_catalog_upsert_resource_state(
    hts_catalog *catalog, const hts_catalog_resource_state *resource,
    int64_t *out_id);
HTSEXT_API int hts_catalog_get_resource_state(
    hts_catalog *catalog, int64_t source_id, const char *resource_kind,
    const char *resource_id, hts_catalog_resource_visit_fn visit, void *user);
HTSEXT_API int hts_catalog_list_resource_states(
    hts_catalog *catalog, int64_t source_id, const char *resource_kind,
    const char *parent_remote_id, hts_catalog_resource_visit_fn visit,
    void *user);
HTSEXT_API int hts_catalog_set_collection_lifecycle(
    hts_catalog *catalog, int64_t source_id, const char *kind,
    const char *remote_id, const char *lifecycle_state,
    const char *media_availability_state);
HTSEXT_API int hts_catalog_mark_post_deleted(
    hts_catalog *catalog, int64_t source_id, const char *remote_id,
    const char *media_availability_state);
HTSEXT_API int hts_catalog_clear_collection_media(
    hts_catalog *catalog, int64_t source_id, const char *kind,
    const char *remote_id);

HTSEXT_API int hts_catalog_read(hts_catalog *catalog, hts_catalog_entity entity,
                     int64_t id, hts_catalog_visit_fn visit, void *user);
/* parent_id is source, collection, post, or selection according to entity. */
HTSEXT_API int hts_catalog_list(hts_catalog *catalog, hts_catalog_entity entity,
                     int64_t parent_id, hts_catalog_visit_fn visit,
                     void *user);

/* Produces a safe basename; separators/control bytes are replaced with '_'. */
HTSEXT_API int hts_catalog_sanitize_filename(const char *input, char *output,
                                  size_t output_size);

#ifdef __cplusplus
}
#endif

#endif
