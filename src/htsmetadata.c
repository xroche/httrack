#include "htsmetadata.h"

#include <stdlib.h>
#include <string.h>

#define HTS_METADATA_MAX_ORIGINS 32U
#define HTS_METADATA_MAX_BODY (16U * 1024U * 1024U)
#define HTS_METADATA_MAX_PAGES 10000U
#define HTS_METADATA_MAX_DELAY_MS (7U * 24U * 60U * 60U * 1000U)
#define HTS_METADATA_MAX_REQUESTS_PER_MINUTE 60000U
#define HTS_METADATA_MAX_CONCURRENCY 1024U

typedef struct hts_metadata_origin_state {
  char origin[HTS_METADATA_ORIGIN_MAX + 1U];
  uint64_t next_request_ms;
  unsigned int active_requests;
} hts_metadata_origin_state;

struct hts_metadata_runtime {
  hts_catalog *catalog;
  const hts_metadata_adapter *adapter;
  const hts_metadata_transport *transport;
  const hts_metadata_clock *clock;
  hts_metadata_report_fn report;
  void *report_context;
  hts_metadata_origin_state origins[HTS_METADATA_MAX_ORIGINS];
  size_t origin_count;
};

static int text_length_valid(const char *value, size_t maximum, int required) {
  size_t length;
  if (value == NULL) return !required;
  length = strlen(value);
  return length <= maximum && (!required || length != 0U);
}

static int copy_text(char *output, size_t output_size, const char *value) {
  size_t length;
  if (output == NULL || output_size == 0U) return 0;
  if (value == NULL) {
    output[0] = '\0';
    return 1;
  }
  length = strlen(value);
  if (length >= output_size) return 0;
  (void) memcpy(output, value, length + 1U);
  return 1;
}

static int safe_http_url(const char *url) {
  const char *authority;
  const char *end;
  const unsigned char *scan;
  if (url == NULL) return 0;
  if (strncmp(url, "https://", 8U) == 0)
    authority = url + 8;
  else if (strncmp(url, "http://", 7U) == 0)
    authority = url + 7;
  else
    return 0;
  if (*authority == '\0') return 0;
  end = strchr(authority, '/');
  if (end == NULL) end = url + strlen(url);
  if (end == authority || memchr(authority, '@', (size_t) (end - authority)) != NULL)
    return 0;
  for (scan = (const unsigned char *) url; *scan != '\0'; scan++) {
    if (*scan < 0x20U || *scan == 0x7fU) return 0;
  }
  return 1;
}

static int safe_http_origin(const char *origin) {
  const char *authority;
  if (!safe_http_url(origin)) return 0;
  authority = origin + (strncmp(origin, "https://", 8U) == 0 ? 8U : 7U);
  return strpbrk(authority, "/?#") == NULL;
}

static int url_matches_origin(const char *url, const char *origin) {
  size_t origin_length;
  if (url == NULL || origin == NULL) return 0;
  origin_length = strlen(origin);
  if (strncmp(url, origin, origin_length) != 0) return 0;
  return url[origin_length] == '\0' || url[origin_length] == '/' ||
         url[origin_length] == '?' || url[origin_length] == '#';
}

hts_metadata_category hts_metadata_classify_http_status(int status_code) {
  if (status_code >= 200 && status_code <= 299)
    return HTS_METADATA_SUCCESS;
  if (status_code == 304) return HTS_METADATA_NOT_MODIFIED;
  if (status_code == 401) return HTS_METADATA_UNAUTHORIZED;
  if (status_code == 403) return HTS_METADATA_FORBIDDEN;
  if (status_code == 404 || status_code == 410)
    return HTS_METADATA_GONE_OR_NOT_FOUND;
  if (status_code == 429) return HTS_METADATA_RATE_LIMITED;
  if (status_code == 408 || status_code == 425 ||
      (status_code >= 500 && status_code <= 599))
    return HTS_METADATA_TEMPORARILY_UNAVAILABLE;
  return HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
}

int hts_metadata_policy_validate(const hts_metadata_policy *policy) {
  const unsigned char *scan;
  if (policy == NULL || policy->requests_per_minute == 0U ||
      policy->requests_per_minute > HTS_METADATA_MAX_REQUESTS_PER_MINUTE ||
      policy->maximum_concurrency == 0U ||
      policy->maximum_concurrency > HTS_METADATA_MAX_CONCURRENCY ||
      policy->maximum_attempts == 0U ||
      policy->maximum_body_bytes == 0U ||
      policy->maximum_body_bytes > HTS_METADATA_MAX_BODY ||
      policy->redirect_limit > 20U ||
      policy->connect_timeout_ms == 0U ||
      policy->request_timeout_ms == 0U ||
      policy->connect_timeout_ms > HTS_METADATA_MAX_DELAY_MS ||
      policy->request_timeout_ms > HTS_METADATA_MAX_DELAY_MS ||
      policy->minimum_refresh_interval_ms > HTS_METADATA_MAX_DELAY_MS ||
      policy->base_backoff_ms > HTS_METADATA_MAX_DELAY_MS ||
      policy->maximum_backoff_ms > HTS_METADATA_MAX_DELAY_MS ||
      policy->base_backoff_ms > policy->maximum_backoff_ms ||
      policy->jitter_percent > 100U ||
      !text_length_valid(policy->user_agent, HTS_METADATA_USER_AGENT_MAX, 1))
    return 0;
  for (scan = (const unsigned char *) policy->user_agent;
       *scan != '\0'; scan++) {
    if (*scan < 0x20U || *scan == 0x7fU) return 0;
  }
  return 1;
}

int hts_metadata_adapter_validate_source(
    const hts_metadata_adapter *adapter,
    const hts_metadata_source_input *input,
    hts_metadata_source *output) {
  if (adapter == NULL || input == NULL || output == NULL ||
      adapter->validate_and_canonicalize == NULL ||
      !text_length_valid(adapter->identifier, 64U, 1) ||
      !text_length_valid(adapter->adapter_version,
                         HTS_METADATA_ADAPTER_VERSION_MAX, 1) ||
      adapter->cursor_version <= 0 ||
      !text_length_valid(input->base_url, HTS_CATALOG_URL_MAX, 1))
    return 0;
  (void) memset(output, 0, sizeof(*output));
  if (adapter->validate_and_canonicalize(adapter->context, input, output) != 0)
    return 0;
  if (!text_length_valid(output->canonical_base_url,
                         HTS_CATALOG_URL_MAX, 1) ||
      !text_length_valid(output->origin, HTS_METADATA_ORIGIN_MAX, 1) ||
      !text_length_valid(output->configured_board,
                         HTS_CATALOG_REMOTE_ID_MAX, 0) ||
      !safe_http_url(output->canonical_base_url) ||
      !safe_http_origin(output->origin) ||
      !url_matches_origin(output->canonical_base_url, output->origin))
    return 0;
  return 1;
}

static int cancelled(const hts_metadata_cancel *cancel) {
  return cancel != NULL && cancel->is_cancelled != NULL &&
         cancel->is_cancelled(cancel->context) != 0;
}

static void report_event(hts_metadata_runtime *runtime,
                         const hts_metadata_source *source,
                         const hts_metadata_sync_request *request,
                         hts_metadata_event_type type,
                         hts_metadata_category category,
                         unsigned int attempt, size_t page_number,
                         size_t records, uint64_t delay,
                         const char *message) {
  hts_metadata_event event;
  if (runtime->report == NULL) return;
  (void) memset(&event, 0, sizeof(event));
  event.type = type;
  event.category = category;
  event.adapter_identifier = runtime->adapter->identifier;
  event.origin = source->origin;
  event.operation = request->operation;
  event.attempt = attempt;
  event.page_number = page_number;
  event.normalized_record_count = records;
  event.delay_ms = delay;
  event.message = message;
  runtime->report(runtime->report_context, &event);
}

static hts_metadata_origin_state *origin_state(
    hts_metadata_runtime *runtime, const char *origin) {
  size_t i;
  for (i = 0U; i < runtime->origin_count; i++) {
    if (strcmp(runtime->origins[i].origin, origin) == 0)
      return &runtime->origins[i];
  }
  if (runtime->origin_count >= HTS_METADATA_MAX_ORIGINS) return NULL;
  i = runtime->origin_count++;
  (void) memset(&runtime->origins[i], 0, sizeof(runtime->origins[i]));
  if (!copy_text(runtime->origins[i].origin,
                 sizeof(runtime->origins[i].origin), origin))
    return NULL;
  return &runtime->origins[i];
}

static uint64_t request_interval(const hts_metadata_policy *policy) {
  return (60000U + (uint64_t) policy->requests_per_minute - 1U) /
         (uint64_t) policy->requests_per_minute;
}

static uint64_t retry_delay(const hts_metadata_policy *policy,
                            unsigned int attempt, uint64_t retry_after_ms) {
  uint64_t delay;
  uint64_t spread;
  uint32_t value;
  unsigned int shift;
  delay = policy->base_backoff_ms;
  shift = attempt > 1U ? attempt - 1U : 0U;
  while (shift-- != 0U && delay < policy->maximum_backoff_ms) {
    if (delay > policy->maximum_backoff_ms / 2U) {
      delay = policy->maximum_backoff_ms;
      break;
    }
    delay *= 2U;
  }
  if (delay > policy->maximum_backoff_ms)
    delay = policy->maximum_backoff_ms;
  value = policy->jitter_seed ^ (uint32_t) (attempt * 2654435761U);
  value ^= value << 13;
  value ^= value >> 17;
  value ^= value << 5;
  spread = (delay * (uint64_t) policy->jitter_percent) / 100U;
  if (spread != 0U) {
    delay += (uint64_t) value % (spread + 1U);
    if (delay > policy->maximum_backoff_ms)
      delay = policy->maximum_backoff_ms;
  }
  if (retry_after_ms > delay) delay = retry_after_ms;
  return delay;
}

static hts_metadata_category wait_for_delay(
    hts_metadata_runtime *runtime, const hts_metadata_cancel *cancel,
    uint64_t delay_ms) {
  if (cancelled(cancel)) return HTS_METADATA_CANCELLED;
  if (delay_ms != 0U &&
      runtime->clock->sleep_ms(runtime->clock->context, delay_ms) != 0)
    return HTS_METADATA_CANCELLED;
  return cancelled(cancel) ? HTS_METADATA_CANCELLED : HTS_METADATA_SUCCESS;
}

static int resolve_id(hts_metadata_runtime *runtime,
                      hts_catalog_entity entity, int64_t source_id,
                      const char *key1, const char *key2,
                      int64_t *out_id) {
  return hts_catalog_find_id(runtime->catalog, entity, source_id,
                             key1, key2, out_id) == HTS_CATALOG_OK;
}

static int persist_batch(hts_metadata_runtime *runtime,
                         const hts_metadata_sync_request *request,
                         const hts_metadata_batch *batch,
                         const hts_metadata_http_response *response,
                         const hts_catalog_sync_state *previous,
                         uint64_t now_ms, size_t *record_count) {
  hts_catalog_sync_cursor cursor;
  size_t i;
  int ok;
  int64_t ignored_id;
  ok = hts_catalog_begin(runtime->catalog) == HTS_CATALOG_OK;
  *record_count = 0U;
  for (i = 0U; ok && i < batch->board_count; i++) {
    hts_catalog_board record;
    record = batch->boards[i].record;
    record.source_id = request->source_id;
    ok = hts_catalog_upsert_board(runtime->catalog, &record,
                                  &ignored_id) == HTS_CATALOG_OK;
    if (ok) (*record_count)++;
  }
  for (i = 0U; ok && i < batch->collection_count; i++) {
    hts_catalog_collection record;
    record = batch->collections[i].record;
    record.source_id = request->source_id;
    record.board_id = 0;
    if (text_length_valid(batch->collections[i].board_remote_id,
                          HTS_CATALOG_REMOTE_ID_MAX, 0) &&
        batch->collections[i].board_remote_id != NULL &&
        batch->collections[i].board_remote_id[0] != '\0') {
      ok = resolve_id(runtime, HTS_CATALOG_ENTITY_BOARD,
                      request->source_id,
                      batch->collections[i].board_remote_id,
                      NULL, &record.board_id);
    }
    if (ok)
      ok = hts_catalog_upsert_collection(runtime->catalog, &record,
                                         &ignored_id) == HTS_CATALOG_OK;
    if (ok) (*record_count)++;
  }
  for (i = 0U; ok && i < batch->post_count; i++) {
    hts_catalog_post record;
    record = batch->posts[i].record;
    record.source_id = request->source_id;
    record.collection_id = 0;
    record.parent_post_id = 0;
    if (batch->posts[i].collection_remote_id != NULL &&
        batch->posts[i].collection_remote_id[0] != '\0') {
      ok = resolve_id(runtime, HTS_CATALOG_ENTITY_COLLECTION,
                      request->source_id,
                      batch->posts[i].collection_kind,
                      batch->posts[i].collection_remote_id,
                      &record.collection_id);
    }
    if (ok && batch->posts[i].parent_remote_id != NULL &&
        batch->posts[i].parent_remote_id[0] != '\0') {
      ok = resolve_id(runtime, HTS_CATALOG_ENTITY_POST,
                      request->source_id, batch->posts[i].parent_remote_id,
                      NULL, &record.parent_post_id);
    }
    if (ok)
      ok = hts_catalog_upsert_post(runtime->catalog, &record,
                                   &ignored_id) == HTS_CATALOG_OK;
    if (ok) (*record_count)++;
  }
  for (i = 0U; ok && i < batch->media_count; i++) {
    hts_catalog_media record;
    record = batch->media[i].record;
    record.source_id = request->source_id;
    record.post_id = 0;
    ok = resolve_id(runtime, HTS_CATALOG_ENTITY_POST,
                    request->source_id, batch->media[i].post_remote_id,
                    NULL, &record.post_id);
    if (ok)
      ok = hts_catalog_upsert_media(runtime->catalog, &record,
                                    &ignored_id) == HTS_CATALOG_OK;
    if (ok) (*record_count)++;
  }
  if (ok && batch->replace_membership_collection_remote_id != NULL) {
    ok = hts_catalog_clear_collection_media(
             runtime->catalog, request->source_id,
             batch->replace_membership_collection_kind,
             batch->replace_membership_collection_remote_id) ==
         HTS_CATALOG_OK;
  }
  for (i = 0U; ok && i < batch->membership_count; i++) {
    int64_t collection_id;
    int64_t media_id;
    ok = resolve_id(runtime, HTS_CATALOG_ENTITY_COLLECTION,
                    request->source_id,
                    batch->memberships[i].collection_kind,
                    batch->memberships[i].collection_remote_id,
                    &collection_id);
    if (ok)
      ok = resolve_id(runtime, HTS_CATALOG_ENTITY_MEDIA,
                      request->source_id,
                      batch->memberships[i].media_remote_id,
                      batch->memberships[i].media_variant_kind,
                      &media_id);
    if (ok)
      ok = hts_catalog_upsert_collection_media(
               runtime->catalog, request->source_id, collection_id, media_id,
               batch->memberships[i].position) == HTS_CATALOG_OK;
    if (ok) (*record_count)++;
  }
  for (i = 0U; ok && i < batch->resource_count; i++) {
    hts_catalog_resource_state record;
    record = batch->resources[i].record;
    record.source_id = request->source_id;
    ok = hts_catalog_upsert_resource_state(runtime->catalog, &record,
                                           &ignored_id) == HTS_CATALOG_OK;
    if (ok) (*record_count)++;
  }
  for (i = 0U; ok && i < batch->lifecycle_change_count; i++) {
    int status;
    const hts_metadata_lifecycle_change *change;
    change = &batch->lifecycle_changes[i];
    if (change->target == HTS_METADATA_LIFECYCLE_COLLECTION) {
      status = hts_catalog_set_collection_lifecycle(
          runtime->catalog, request->source_id, change->kind,
          change->remote_id, change->state,
          change->media_availability_state);
    } else if (change->target == HTS_METADATA_LIFECYCLE_POST &&
               change->state != NULL && strcmp(change->state, "deleted") == 0) {
      status = hts_catalog_mark_post_deleted(
          runtime->catalog, request->source_id, change->remote_id,
          change->media_availability_state);
    } else {
      status = HTS_CATALOG_INVALID;
    }
    ok = status == HTS_CATALOG_OK || status == HTS_CATALOG_NOT_FOUND;
    if (ok && status == HTS_CATALOG_OK) (*record_count)++;
  }
  if (ok) {
    (void) memset(&cursor, 0, sizeof(cursor));
    cursor.source_id = request->source_id;
    cursor.resource_kind = request->resource_kind;
    cursor.resource_id = request->resource_id;
    cursor.cursor_value = batch->next_cursor != NULL
                            ? batch->next_cursor : previous->cursor_value;
    cursor.etag = response->etag != NULL ? response->etag : previous->etag;
    cursor.last_modified = response->last_modified != NULL
                             ? response->last_modified
                             : previous->last_modified;
    cursor.cursor_version = runtime->adapter->cursor_version;
    cursor.adapter_version = runtime->adapter->adapter_version;
    cursor.last_attempt_ms = (int64_t) now_ms;
    cursor.last_success_ms = (int64_t) now_ms;
    ok = hts_catalog_upsert_sync_cursor(runtime->catalog, &cursor,
                                        &ignored_id) == HTS_CATALOG_OK;
  }
  if (ok) {
    if (hts_catalog_commit(runtime->catalog) != HTS_CATALOG_OK) ok = 0;
  } else {
    (void) hts_catalog_rollback(runtime->catalog);
  }
  return ok;
}

static int persist_not_modified(hts_metadata_runtime *runtime,
                                const hts_metadata_sync_request *request,
                                const hts_metadata_http_response *response,
                                const hts_catalog_sync_state *previous,
                                uint64_t now_ms) {
  hts_catalog_sync_cursor cursor;
  int64_t ignored_id;
  int ok;
  (void) memset(&cursor, 0, sizeof(cursor));
  cursor.source_id = request->source_id;
  cursor.resource_kind = request->resource_kind;
  cursor.resource_id = request->resource_id;
  cursor.cursor_value = previous->cursor_value;
  cursor.etag = response->etag != NULL ? response->etag : previous->etag;
  cursor.last_modified = response->last_modified != NULL
                           ? response->last_modified
                           : previous->last_modified;
  cursor.cursor_version = runtime->adapter->cursor_version;
  cursor.adapter_version = runtime->adapter->adapter_version;
  cursor.last_attempt_ms = (int64_t) now_ms;
  cursor.last_success_ms = (int64_t) now_ms;
  ok = hts_catalog_begin(runtime->catalog) == HTS_CATALOG_OK;
  if (ok)
    ok = hts_catalog_upsert_sync_cursor(runtime->catalog, &cursor,
                                        &ignored_id) == HTS_CATALOG_OK;
  if (ok)
    ok = hts_catalog_commit(runtime->catalog) == HTS_CATALOG_OK;
  else
    (void) hts_catalog_rollback(runtime->catalog);
  return ok;
}

int hts_metadata_runtime_create(
    hts_catalog *catalog, const hts_metadata_adapter *adapter,
    const hts_metadata_transport *transport, const hts_metadata_clock *clock,
    hts_metadata_report_fn report, void *report_context,
    hts_metadata_runtime **out_runtime) {
  hts_metadata_runtime *runtime;
  if (catalog == NULL || adapter == NULL || transport == NULL ||
      clock == NULL || out_runtime == NULL ||
      adapter->prepare_request == NULL || adapter->parse_response == NULL ||
      transport->perform == NULL || clock->now_ms == NULL ||
      clock->sleep_ms == NULL ||
      !text_length_valid(adapter->identifier, 64U, 1) ||
      !text_length_valid(adapter->adapter_version,
                         HTS_METADATA_ADAPTER_VERSION_MAX, 1) ||
      adapter->cursor_version <= 0)
    return 0;
  runtime = (hts_metadata_runtime *) calloc(1U, sizeof(*runtime));
  if (runtime == NULL) return 0;
  runtime->catalog = catalog;
  runtime->adapter = adapter;
  runtime->transport = transport;
  runtime->clock = clock;
  runtime->report = report;
  runtime->report_context = report_context;
  *out_runtime = runtime;
  return 1;
}

void hts_metadata_runtime_destroy(hts_metadata_runtime *runtime) {
  free(runtime);
}

static hts_metadata_category terminal_error(
    hts_metadata_runtime *runtime, const hts_metadata_source *source,
    const hts_metadata_sync_request *request, hts_metadata_sync_result *result,
    hts_metadata_category category, unsigned int attempt, size_t page,
    const char *message) {
  result->category = category;
  report_event(runtime, source, request, HTS_METADATA_EVENT_ERROR, category,
               attempt, page, 0U, 0U, message);
  return category;
}

hts_metadata_category hts_metadata_runtime_sync(
    hts_metadata_runtime *runtime, const hts_metadata_sync_request *request,
    hts_metadata_sync_result *result) {
  hts_metadata_source source;
  hts_catalog_sync_state cursor;
  hts_metadata_origin_state *origin;
  hts_metadata_category category;
  const char *opaque_cursor;
  size_t page;
  int cursor_status;
  if (result != NULL) (void) memset(result, 0, sizeof(*result));
  if (runtime == NULL || request == NULL || result == NULL ||
      request->source_id <= 0 ||
      !text_length_valid(request->resource_kind, 64U, 1) ||
      !text_length_valid(request->resource_id,
                         HTS_CATALOG_REMOTE_ID_MAX, 1) ||
      !hts_metadata_policy_validate(&request->policy) ||
      !hts_metadata_adapter_validate_source(runtime->adapter,
                                            &request->source, &source)) {
    if (result != NULL)
      result->category = HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
    return HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
  }
  origin = origin_state(runtime, source.origin);
  if (origin == NULL)
    return terminal_error(runtime, &source, request, result,
                          HTS_METADATA_PERMANENT_CONFIGURATION_ERROR,
                          0U, 0U, "origin capacity exceeded");
  cursor_status = hts_catalog_get_sync_cursor(
      runtime->catalog, request->source_id, request->resource_kind,
      request->resource_id, &cursor);
  if (cursor_status == HTS_CATALOG_NOT_FOUND) {
    (void) memset(&cursor, 0, sizeof(cursor));
    cursor.last_attempt_ms = -1;
    cursor.last_success_ms = -1;
  } else if (cursor_status != HTS_CATALOG_OK) {
    return terminal_error(runtime, &source, request, result,
                          HTS_METADATA_PERMANENT_CONFIGURATION_ERROR,
                          0U, 0U, "catalog cursor read failed");
  } else if (cursor.cursor_version != runtime->adapter->cursor_version ||
             strcmp(cursor.adapter_version,
                    runtime->adapter->adapter_version) != 0) {
    return terminal_error(runtime, &source, request, result,
                          HTS_METADATA_PERMANENT_CONFIGURATION_ERROR,
                          0U, 0U, "incompatible stored cursor version");
  }
  if (cursor.last_success_ms >= 0 &&
      request->policy.minimum_refresh_interval_ms != 0U) {
    uint64_t now;
    now = runtime->clock->now_ms(runtime->clock->context);
    if (now >= (uint64_t) cursor.last_success_ms &&
        now - (uint64_t) cursor.last_success_ms <
          request->policy.minimum_refresh_interval_ms) {
      result->category = HTS_METADATA_SUCCESS;
      result->skipped_refresh = 1;
      report_event(runtime, &source, request,
                   HTS_METADATA_EVENT_SKIPPED_REFRESH,
                   HTS_METADATA_SUCCESS, 0U, 0U, 0U, 0U,
                   "minimum refresh interval not elapsed");
      return HTS_METADATA_SUCCESS;
    }
  }
  opaque_cursor = cursor.cursor_value;
  page = 0U;
  for (;;) {
    unsigned int attempt;
    if (page >= HTS_METADATA_MAX_PAGES)
      return terminal_error(runtime, &source, request, result,
                            HTS_METADATA_MALFORMED, 0U, page,
                            "adapter page limit exceeded");
    for (attempt = 1U; attempt <= request->policy.maximum_attempts;
         attempt++) {
      hts_metadata_http_request http_request;
      hts_metadata_http_response response;
      hts_metadata_transport_status transport_status;
      uint64_t now;
      uint64_t delay;
      if (cancelled(&request->cancel))
        return terminal_error(runtime, &source, request, result,
                              HTS_METADATA_CANCELLED, attempt, page,
                              "synchronization cancelled");
      (void) memset(&http_request, 0, sizeof(http_request));
      if (runtime->adapter->prepare_request(
          runtime->adapter->context, &source, request->operation,
              request->resource_id, opaque_cursor, &http_request) != 0 ||
          !safe_http_url(http_request.url) ||
          !url_matches_origin(http_request.url, source.origin)) {
        return terminal_error(runtime, &source, request, result,
                              HTS_METADATA_PERMANENT_CONFIGURATION_ERROR,
                              attempt, page, "adapter produced invalid request");
      }
      http_request.credential_ref = request->source.credential_ref;
      http_request.if_none_match = cursor.etag[0] != '\0'
                                     ? cursor.etag : NULL;
      http_request.if_modified_since =
          cursor.last_modified[0] != '\0' ? cursor.last_modified : NULL;
      http_request.user_agent = request->policy.user_agent;
      http_request.maximum_body_bytes = request->policy.maximum_body_bytes;
      http_request.redirect_limit = request->policy.redirect_limit;
      http_request.connect_timeout_ms = request->policy.connect_timeout_ms;
      http_request.request_timeout_ms = request->policy.request_timeout_ms;
      http_request.origin_concurrency_limit =
          request->policy.maximum_concurrency;
      http_request.cancel = request->cancel;
      now = runtime->clock->now_ms(runtime->clock->context);
      if (now < origin->next_request_ms) {
        category = wait_for_delay(runtime, &request->cancel,
                                  origin->next_request_ms - now);
        if (category != HTS_METADATA_SUCCESS)
          return terminal_error(runtime, &source, request, result,
                                category, attempt, page,
                                "synchronization cancelled while rate limited");
      }
      if (origin->active_requests >= request->policy.maximum_concurrency) {
        delay = request_interval(&request->policy);
        category = wait_for_delay(runtime, &request->cancel, delay);
        if (category != HTS_METADATA_SUCCESS)
          return terminal_error(runtime, &source, request, result,
                                category, attempt, page,
                                "synchronization cancelled while queued");
      }
      report_event(runtime, &source, request, HTS_METADATA_EVENT_REQUEST,
                   HTS_METADATA_SUCCESS, attempt, page, 0U, 0U,
                   "metadata request started");
      (void) memset(&response, 0, sizeof(response));
      origin->active_requests++;
      transport_status = runtime->transport->perform(
          runtime->transport->context, &http_request, &response);
      origin->active_requests--;
      result->requests_made++;
      now = runtime->clock->now_ms(runtime->clock->context);
      origin->next_request_ms = now + request_interval(&request->policy);
      if (cancelled(&request->cancel))
        return terminal_error(runtime, &source, request, result,
                              HTS_METADATA_CANCELLED, attempt, page,
                              "synchronization cancelled after request");
      if (transport_status == HTS_METADATA_TRANSPORT_PERMANENT_ERROR) {
        category = HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
      } else if (transport_status == HTS_METADATA_TRANSPORT_TEMPORARY_ERROR) {
        category = HTS_METADATA_TEMPORARILY_UNAVAILABLE;
      } else {
        category = hts_metadata_classify_http_status(response.status_code);
      }
      if (category == HTS_METADATA_RATE_LIMITED ||
          category == HTS_METADATA_TEMPORARILY_UNAVAILABLE) {
        if (attempt == request->policy.maximum_attempts)
          return terminal_error(runtime, &source, request, result, category,
                                attempt, page, "metadata retry limit reached");
        delay = retry_delay(&request->policy, attempt,
                            response.retry_after_ms);
        report_event(runtime, &source, request, HTS_METADATA_EVENT_RETRY,
                     category, attempt, page, 0U, delay,
                     category == HTS_METADATA_RATE_LIMITED
                       ? "metadata origin rate limited"
                       : "metadata origin temporarily unavailable");
        category = wait_for_delay(runtime, &request->cancel, delay);
        if (category != HTS_METADATA_SUCCESS)
          return terminal_error(runtime, &source, request, result, category,
                                attempt, page,
                                "synchronization cancelled during retry");
        continue;
      }
      if (category == HTS_METADATA_NOT_MODIFIED) {
        if (!persist_not_modified(runtime, request, &response, &cursor, now))
          return terminal_error(runtime, &source, request, result,
                                HTS_METADATA_PERMANENT_CONFIGURATION_ERROR,
                                attempt, page, "catalog commit failed");
        result->category = category;
        result->not_modified = 1;
        report_event(runtime, &source, request,
                     HTS_METADATA_EVENT_NOT_MODIFIED, category,
                     attempt, page, 0U, 0U,
                     "metadata not modified");
        return category;
      }
      if (category != HTS_METADATA_SUCCESS)
        return terminal_error(runtime, &source, request, result, category,
                              attempt, page, "metadata request rejected");
      if (response.body_size > request->policy.maximum_body_bytes ||
          (response.body_size != 0U && response.body == NULL))
        return terminal_error(runtime, &source, request, result,
                              HTS_METADATA_MALFORMED, attempt, page,
                              "metadata response exceeded bounds");
      {
        hts_metadata_batch batch;
        size_t records;
        const char *next_cursor;
        int has_more;
        (void) memset(&batch, 0, sizeof(batch));
        category = runtime->adapter->parse_response(
            runtime->adapter->context, request->operation, &response, &batch);
        if (category != HTS_METADATA_SUCCESS) {
          if (runtime->adapter->release_batch != NULL)
            runtime->adapter->release_batch(runtime->adapter->context, &batch);
          return terminal_error(runtime, &source, request, result, category,
                                attempt, page, "metadata response parse failed");
        }
        if ((batch.board_count != 0U && batch.boards == NULL) ||
            (batch.collection_count != 0U && batch.collections == NULL) ||
            (batch.post_count != 0U && batch.posts == NULL) ||
            (batch.media_count != 0U && batch.media == NULL) ||
            (batch.membership_count != 0U && batch.memberships == NULL) ||
            (batch.resource_count != 0U && batch.resources == NULL) ||
            (batch.lifecycle_change_count != 0U &&
             batch.lifecycle_changes == NULL) ||
            ((batch.replace_membership_collection_kind == NULL) !=
             (batch.replace_membership_collection_remote_id == NULL)) ||
            (batch.has_more && (batch.next_cursor == NULL ||
                                batch.next_cursor[0] == '\0')) ||
            (batch.has_more && strcmp(opaque_cursor,
                                      batch.next_cursor) == 0)) {
          if (runtime->adapter->release_batch != NULL)
            runtime->adapter->release_batch(runtime->adapter->context, &batch);
          return terminal_error(runtime, &source, request, result,
                                HTS_METADATA_MALFORMED, attempt, page,
                                "adapter produced invalid batch");
        }
        next_cursor = batch.next_cursor;
        has_more = batch.has_more;
        if (!persist_batch(runtime, request, &batch, &response, &cursor,
                           now, &records)) {
          if (runtime->adapter->release_batch != NULL)
            runtime->adapter->release_batch(runtime->adapter->context, &batch);
          return terminal_error(runtime, &source, request, result,
                                HTS_METADATA_MALFORMED, attempt, page,
                                "catalog batch rolled back");
        }
        if (!copy_text(cursor.cursor_value, sizeof(cursor.cursor_value),
                       next_cursor != NULL ? next_cursor : opaque_cursor) ||
            !copy_text(cursor.etag, sizeof(cursor.etag),
                       response.etag != NULL ? response.etag : cursor.etag) ||
            !copy_text(cursor.last_modified, sizeof(cursor.last_modified),
                       response.last_modified != NULL
                         ? response.last_modified : cursor.last_modified) ||
            !copy_text(cursor.adapter_version,
                       sizeof(cursor.adapter_version),
                       runtime->adapter->adapter_version)) {
          if (runtime->adapter->release_batch != NULL)
            runtime->adapter->release_batch(runtime->adapter->context, &batch);
          return terminal_error(runtime, &source, request, result,
                                HTS_METADATA_MALFORMED, attempt, page,
                                "cursor update exceeded bounds");
        }
        cursor.cursor_version = runtime->adapter->cursor_version;
        cursor.last_attempt_ms = (int64_t) now;
        cursor.last_success_ms = (int64_t) now;
        opaque_cursor = cursor.cursor_value;
        result->pages_committed++;
        result->records_committed += records;
        report_event(runtime, &source, request,
                     HTS_METADATA_EVENT_PAGE_COMMITTED,
                     HTS_METADATA_SUCCESS, attempt, page, records, 0U,
                     "metadata page committed");
        if (runtime->adapter->release_batch != NULL)
          runtime->adapter->release_batch(runtime->adapter->context, &batch);
        if (!has_more) {
          result->category = HTS_METADATA_SUCCESS;
          report_event(runtime, &source, request,
                       HTS_METADATA_EVENT_COMPLETE,
                       HTS_METADATA_SUCCESS, attempt, page, records, 0U,
                       "metadata synchronization complete");
          return HTS_METADATA_SUCCESS;
        }
      }
      page++;
      break;
    }
  }
}
