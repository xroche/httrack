/*
 * Imageboard metadata adapter and synchronization runtime.
 *
 * Adapters normalize metadata only.  They do not download media bodies and
 * shared code never interprets adapter cursor values.
 */

#ifndef HTSMETADATA_H
#define HTSMETADATA_H

#include <stddef.h>
#include <stdint.h>

#include "htscatalog.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HTS_METADATA_ADAPTER_VERSION_MAX 128U
#define HTS_METADATA_ORIGIN_MAX 8192U
#define HTS_METADATA_USER_AGENT_MAX 255U

#define HTS_METADATA_CAP_THREADS              (1UL << 0)
#define HTS_METADATA_CAP_POOLS                (1UL << 1)
#define HTS_METADATA_CAP_SAVED_QUERIES        (1UL << 2)
#define HTS_METADATA_CAP_INCREMENTAL_CHANGES  (1UL << 3)
#define HTS_METADATA_CAP_CONDITIONAL_REQUESTS (1UL << 4)
#define HTS_METADATA_CAP_AUTHENTICATION       (1UL << 5)
#define HTS_METADATA_CAP_PRESERVATION_ELIGIBLE (1UL << 6)

typedef enum hts_metadata_operation {
  HTS_METADATA_DISCOVER_BOARDS = 1,
  HTS_METADATA_VALIDATE_BOARD,
  HTS_METADATA_ENUMERATE_COLLECTIONS,
  HTS_METADATA_ENUMERATE_POSTS
} hts_metadata_operation;

typedef enum hts_metadata_category {
  HTS_METADATA_SUCCESS = 0,
  HTS_METADATA_NOT_MODIFIED,
  HTS_METADATA_TEMPORARILY_UNAVAILABLE,
  HTS_METADATA_RATE_LIMITED,
  HTS_METADATA_UNAUTHORIZED,
  HTS_METADATA_FORBIDDEN,
  HTS_METADATA_GONE_OR_NOT_FOUND,
  HTS_METADATA_MALFORMED,
  HTS_METADATA_PERMANENT_CONFIGURATION_ERROR,
  HTS_METADATA_CANCELLED
} hts_metadata_category;

typedef enum hts_metadata_transport_status {
  HTS_METADATA_TRANSPORT_OK = 0,
  HTS_METADATA_TRANSPORT_TEMPORARY_ERROR,
  HTS_METADATA_TRANSPORT_PERMANENT_ERROR
} hts_metadata_transport_status;

typedef struct hts_metadata_source_input {
  const char *base_url;
  const char *configured_board;
  const char *credential_ref;
} hts_metadata_source_input;

typedef struct hts_metadata_source {
  char canonical_base_url[HTS_CATALOG_URL_MAX + 1U];
  char origin[HTS_METADATA_ORIGIN_MAX + 1U];
  char configured_board[HTS_CATALOG_REMOTE_ID_MAX + 1U];
} hts_metadata_source;

typedef struct hts_metadata_cancel {
  void *context;
  int (*is_cancelled)(void *context);
} hts_metadata_cancel;

typedef struct hts_metadata_http_request {
  char url[HTS_CATALOG_URL_MAX + 1U];
  const char *credential_ref;
  const char *if_none_match;
  const char *if_modified_since;
  const char *user_agent;
  size_t maximum_body_bytes;
  unsigned int redirect_limit;
  uint64_t connect_timeout_ms;
  uint64_t request_timeout_ms;
  unsigned int origin_concurrency_limit;
  hts_metadata_cancel cancel;
} hts_metadata_http_request;

typedef struct hts_metadata_http_response {
  int status_code;
  const unsigned char *body;
  size_t body_size;
  const char *etag;
  const char *last_modified;
  uint64_t retry_after_ms;
} hts_metadata_http_response;

typedef struct hts_metadata_board_item {
  hts_catalog_board record;
} hts_metadata_board_item;

typedef struct hts_metadata_collection_item {
  hts_catalog_collection record;
  const char *board_remote_id;
} hts_metadata_collection_item;

typedef struct hts_metadata_post_item {
  hts_catalog_post record;
  const char *collection_kind;
  const char *collection_remote_id;
  const char *parent_remote_id;
} hts_metadata_post_item;

typedef struct hts_metadata_media_item {
  hts_catalog_media record;
  const char *post_remote_id;
} hts_metadata_media_item;

typedef struct hts_metadata_membership_item {
  const char *collection_kind;
  const char *collection_remote_id;
  const char *media_remote_id;
  const char *media_variant_kind;
  int64_t position;
} hts_metadata_membership_item;

typedef struct hts_metadata_resource_item {
  hts_catalog_resource_state record;
} hts_metadata_resource_item;

typedef enum hts_metadata_lifecycle_target {
  HTS_METADATA_LIFECYCLE_COLLECTION = 1,
  HTS_METADATA_LIFECYCLE_POST
} hts_metadata_lifecycle_target;

typedef struct hts_metadata_lifecycle_change {
  hts_metadata_lifecycle_target target;
  const char *kind;
  const char *remote_id;
  const char *state;
  const char *media_availability_state;
} hts_metadata_lifecycle_change;

typedef struct hts_metadata_batch {
  const hts_metadata_board_item *boards;
  size_t board_count;
  const hts_metadata_collection_item *collections;
  size_t collection_count;
  const hts_metadata_post_item *posts;
  size_t post_count;
  const hts_metadata_media_item *media;
  size_t media_count;
  const hts_metadata_membership_item *memberships;
  size_t membership_count;
  const hts_metadata_resource_item *resources;
  size_t resource_count;
  const hts_metadata_lifecycle_change *lifecycle_changes;
  size_t lifecycle_change_count;
  const char *replace_membership_collection_kind;
  const char *replace_membership_collection_remote_id;
  const char *next_cursor;
  int has_more;
} hts_metadata_batch;

typedef struct hts_metadata_adapter {
  const char *identifier;
  const char *adapter_version;
  int cursor_version;
  unsigned long capabilities;
  void *context;
  int (*validate_and_canonicalize)(void *context,
                                   const hts_metadata_source_input *input,
                                   hts_metadata_source *output);
  int (*prepare_request)(void *context,
                         const hts_metadata_source *source,
                         hts_metadata_operation operation,
                         const char *resource_id,
                         const char *opaque_cursor,
                         hts_metadata_http_request *request);
  hts_metadata_category (*parse_response)(
                         void *context,
                         hts_metadata_operation operation,
                         const hts_metadata_http_response *response,
                         hts_metadata_batch *batch);
  void (*release_batch)(void *context, hts_metadata_batch *batch);
} hts_metadata_adapter;

typedef struct hts_metadata_transport {
  void *context;
  /* Response storage remains transport-owned until the next perform call. */
  hts_metadata_transport_status (*perform)(
      void *context, const hts_metadata_http_request *request,
      hts_metadata_http_response *response);
} hts_metadata_transport;

typedef struct hts_metadata_clock {
  void *context;
  uint64_t (*now_ms)(void *context);
  int (*sleep_ms)(void *context, uint64_t delay_ms);
} hts_metadata_clock;

typedef struct hts_metadata_policy {
  unsigned int requests_per_minute;
  unsigned int maximum_concurrency;
  uint64_t minimum_refresh_interval_ms;
  uint64_t base_backoff_ms;
  uint64_t maximum_backoff_ms;
  unsigned int maximum_attempts;
  size_t maximum_body_bytes;
  unsigned int redirect_limit;
  uint64_t connect_timeout_ms;
  uint64_t request_timeout_ms;
  unsigned int jitter_percent;
  uint32_t jitter_seed;
  const char *user_agent;
} hts_metadata_policy;

typedef enum hts_metadata_event_type {
  HTS_METADATA_EVENT_REQUEST = 1,
  HTS_METADATA_EVENT_RETRY,
  HTS_METADATA_EVENT_PAGE_COMMITTED,
  HTS_METADATA_EVENT_NOT_MODIFIED,
  HTS_METADATA_EVENT_SKIPPED_REFRESH,
  HTS_METADATA_EVENT_ERROR,
  HTS_METADATA_EVENT_COMPLETE
} hts_metadata_event_type;

typedef struct hts_metadata_event {
  hts_metadata_event_type type;
  hts_metadata_category category;
  const char *adapter_identifier;
  const char *origin;
  hts_metadata_operation operation;
  unsigned int attempt;
  size_t page_number;
  size_t normalized_record_count;
  uint64_t delay_ms;
  const char *message;
} hts_metadata_event;

typedef void (*hts_metadata_report_fn)(void *context,
                                       const hts_metadata_event *event);

typedef struct hts_metadata_runtime hts_metadata_runtime;

typedef struct hts_metadata_sync_request {
  int64_t source_id;
  hts_metadata_operation operation;
  const char *resource_kind;
  const char *resource_id;
  hts_metadata_source_input source;
  hts_metadata_policy policy;
  hts_metadata_cancel cancel;
} hts_metadata_sync_request;

typedef struct hts_metadata_sync_result {
  hts_metadata_category category;
  size_t pages_committed;
  size_t records_committed;
  unsigned int requests_made;
  int not_modified;
  int skipped_refresh;
} hts_metadata_sync_result;

HTSEXT_API hts_metadata_category hts_metadata_classify_http_status(
    int status_code);
HTSEXT_API int hts_metadata_policy_validate(
    const hts_metadata_policy *policy);
HTSEXT_API int hts_metadata_adapter_validate_source(
    const hts_metadata_adapter *adapter,
    const hts_metadata_source_input *input,
    hts_metadata_source *output);

HTSEXT_API int hts_metadata_runtime_create(
    hts_catalog *catalog, const hts_metadata_adapter *adapter,
    const hts_metadata_transport *transport, const hts_metadata_clock *clock,
    hts_metadata_report_fn report, void *report_context,
    hts_metadata_runtime **out_runtime);
HTSEXT_API void hts_metadata_runtime_destroy(hts_metadata_runtime *runtime);
HTSEXT_API hts_metadata_category hts_metadata_runtime_sync(
    hts_metadata_runtime *runtime, const hts_metadata_sync_request *request,
    hts_metadata_sync_result *result);

#ifdef __cplusplus
}
#endif

#endif
