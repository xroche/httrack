#define _XOPEN_SOURCE 700

#include "htsmetadata.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define FIXTURE_ITEMS 16U
#define FIXTURE_STORAGE 8192U
#define FIXTURE_BODY 16384U

#define CHECK(expression) do { \
  if (!(expression)) { \
    fprintf(stderr, "check failed at %s:%d: %s\n", \
            __FILE__, __LINE__, #expression); \
    return 1; \
  } \
} while (0)

typedef struct json_reader {
  const char *current;
  const char *end;
} json_reader;

typedef struct fixture_adapter_context {
  hts_metadata_board_item boards[FIXTURE_ITEMS];
  hts_metadata_collection_item collections[FIXTURE_ITEMS];
  hts_metadata_post_item posts[FIXTURE_ITEMS];
  hts_metadata_media_item media[FIXTURE_ITEMS];
  hts_metadata_membership_item memberships[FIXTURE_ITEMS];
  size_t board_count;
  size_t collection_count;
  size_t post_count;
  size_t media_count;
  size_t membership_count;
  char storage[FIXTURE_STORAGE];
  size_t storage_used;
  char next_cursor[HTS_CATALOG_URL_MAX + 1U];
} fixture_adapter_context;

typedef struct fake_plan {
  const char *url;
  const char *if_none_match;
  hts_metadata_transport_status transport_status;
  int status_code;
  const char *fixture_name;
  const char *etag;
  uint64_t retry_after_ms;
} fake_plan;

typedef struct fake_transport_context {
  const fake_plan *plans;
  size_t plan_count;
  size_t plan_index;
  char body[FIXTURE_BODY];
  int failed;
} fake_transport_context;

typedef struct fake_clock_context {
  uint64_t now_ms;
  uint64_t total_sleep_ms;
  unsigned int sleep_count;
  int cancel_on_sleep;
  int cancelled;
} fake_clock_context;

typedef struct report_context {
  unsigned int events;
  unsigned int retries;
  unsigned int committed;
  int safe;
} report_context;

typedef struct list_context {
  size_t count;
  int64_t positions[8];
} list_context;

static void skip_space(json_reader *reader) {
  while (reader->current < reader->end &&
         (*reader->current == ' ' || *reader->current == '\n' ||
          *reader->current == '\r' || *reader->current == '\t'))
    reader->current++;
}

static int take(json_reader *reader, char expected) {
  skip_space(reader);
  if (reader->current >= reader->end || *reader->current != expected)
    return 0;
  reader->current++;
  return 1;
}

static char *parse_string(fixture_adapter_context *context,
                          json_reader *reader) {
  size_t start;
  skip_space(reader);
  if (reader->current >= reader->end || *reader->current != '"')
    return NULL;
  reader->current++;
  start = context->storage_used;
  while (reader->current < reader->end && *reader->current != '"') {
    unsigned char value;
    value = (unsigned char) *reader->current++;
    if (value < 0x20U || value == '\\' ||
        context->storage_used + 1U >= sizeof(context->storage))
      return NULL;
    context->storage[context->storage_used++] = (char) value;
  }
  if (reader->current >= reader->end || *reader->current != '"')
    return NULL;
  reader->current++;
  context->storage[context->storage_used++] = '\0';
  return &context->storage[start];
}

static int parse_integer(json_reader *reader, int64_t *value) {
  int64_t result;
  int found;
  skip_space(reader);
  result = 0;
  found = 0;
  while (reader->current < reader->end &&
         *reader->current >= '0' && *reader->current <= '9') {
    found = 1;
    result = result * 10 + (int64_t) (*reader->current - '0');
    reader->current++;
  }
  if (!found) return 0;
  *value = result;
  return 1;
}

static char *next_string(fixture_adapter_context *context,
                         json_reader *reader) {
  if (!take(reader, ',')) return NULL;
  return parse_string(context, reader);
}

static int parse_board(fixture_adapter_context *context,
                       json_reader *reader) {
  hts_metadata_board_item *item;
  if (context->board_count >= FIXTURE_ITEMS) return 0;
  item = &context->boards[context->board_count++];
  (void) memset(item, 0, sizeof(*item));
  item->record.remote_id = next_string(context, reader);
  item->record.name = next_string(context, reader);
  item->record.display_name = next_string(context, reader);
  item->record.capabilities_json =
      "{\"threads\":true,\"incremental\":true}";
  item->record.metadata_json = "{}";
  return item->record.remote_id != NULL && item->record.name != NULL &&
         item->record.display_name != NULL;
}

static int parse_collection(fixture_adapter_context *context,
                            json_reader *reader) {
  hts_metadata_collection_item *item;
  if (context->collection_count >= FIXTURE_ITEMS) return 0;
  item = &context->collections[context->collection_count++];
  (void) memset(item, 0, sizeof(*item));
  item->record.kind = next_string(context, reader);
  item->record.remote_id = next_string(context, reader);
  item->board_remote_id = next_string(context, reader);
  item->record.title = next_string(context, reader);
  item->record.lifecycle_state = next_string(context, reader);
  item->record.metadata_json = "{}";
  return item->record.kind != NULL && item->record.remote_id != NULL &&
         item->board_remote_id != NULL && item->record.title != NULL &&
         item->record.lifecycle_state != NULL;
}

static int parse_post(fixture_adapter_context *context,
                      json_reader *reader) {
  hts_metadata_post_item *item;
  if (context->post_count >= FIXTURE_ITEMS) return 0;
  item = &context->posts[context->post_count++];
  (void) memset(item, 0, sizeof(*item));
  item->record.remote_id = next_string(context, reader);
  item->collection_kind = next_string(context, reader);
  item->collection_remote_id = next_string(context, reader);
  item->record.text = next_string(context, reader);
  item->record.deleted = 0;
  item->record.restricted = 0;
  item->record.raw_metadata_version = 1;
  item->record.metadata_json = "{}";
  return item->record.remote_id != NULL &&
         item->collection_kind != NULL &&
         item->collection_remote_id != NULL &&
         item->record.text != NULL;
}

static int parse_media(fixture_adapter_context *context,
                       json_reader *reader) {
  hts_metadata_media_item *item;
  if (context->media_count >= FIXTURE_ITEMS) return 0;
  item = &context->media[context->media_count++];
  (void) memset(item, 0, sizeof(*item));
  item->record.remote_id = next_string(context, reader);
  item->post_remote_id = next_string(context, reader);
  item->record.variant_kind = next_string(context, reader);
  item->record.remote_url = next_string(context, reader);
  item->record.original_filename = next_string(context, reader);
  item->record.size_bytes = -1;
  item->record.width = -1;
  item->record.height = -1;
  item->record.availability_state = "available";
  item->record.metadata_json = "{}";
  return item->record.remote_id != NULL && item->post_remote_id != NULL &&
         item->record.variant_kind != NULL &&
         item->record.remote_url != NULL &&
         item->record.original_filename != NULL;
}

static int parse_membership(fixture_adapter_context *context,
                            json_reader *reader) {
  hts_metadata_membership_item *item;
  if (context->membership_count >= FIXTURE_ITEMS) return 0;
  item = &context->memberships[context->membership_count++];
  (void) memset(item, 0, sizeof(*item));
  item->collection_kind = next_string(context, reader);
  item->collection_remote_id = next_string(context, reader);
  item->media_remote_id = next_string(context, reader);
  item->media_variant_kind = next_string(context, reader);
  if (!take(reader, ',') || !parse_integer(reader, &item->position))
    return 0;
  return item->collection_kind != NULL &&
         item->collection_remote_id != NULL &&
         item->media_remote_id != NULL &&
         item->media_variant_kind != NULL;
}

static int parse_record(fixture_adapter_context *context,
                        json_reader *reader) {
  char *type;
  int ok;
  if (!take(reader, '[')) return 0;
  type = parse_string(context, reader);
  if (type == NULL) return 0;
  if (strcmp(type, "board") == 0)
    ok = parse_board(context, reader);
  else if (strcmp(type, "collection") == 0)
    ok = parse_collection(context, reader);
  else if (strcmp(type, "post") == 0)
    ok = parse_post(context, reader);
  else if (strcmp(type, "media") == 0)
    ok = parse_media(context, reader);
  else if (strcmp(type, "member") == 0)
    ok = parse_membership(context, reader);
  else
    return 0;
  return ok && take(reader, ']');
}

static hts_metadata_category fixture_parse(
    void *opaque, hts_metadata_operation operation,
    const hts_metadata_http_response *response, hts_metadata_batch *batch) {
  fixture_adapter_context *context;
  json_reader reader;
  char *key;
  char *next;
  int64_t more;
  int first;
  (void) operation;
  context = (fixture_adapter_context *) opaque;
  (void) memset(context, 0, sizeof(*context));
  (void) memset(batch, 0, sizeof(*batch));
  if (response == NULL || response->body == NULL) return HTS_METADATA_MALFORMED;
  reader.current = (const char *) response->body;
  reader.end = reader.current + response->body_size;
  if (!take(&reader, '{')) return HTS_METADATA_MALFORMED;
  key = parse_string(context, &reader);
  if (key == NULL || strcmp(key, "next") != 0 || !take(&reader, ':'))
    return HTS_METADATA_MALFORMED;
  next = parse_string(context, &reader);
  if (next == NULL || strlen(next) > HTS_CATALOG_URL_MAX)
    return HTS_METADATA_MALFORMED;
  (void) strcpy(context->next_cursor, next);
  if (!take(&reader, ',')) return HTS_METADATA_MALFORMED;
  key = parse_string(context, &reader);
  if (key == NULL || strcmp(key, "more") != 0 || !take(&reader, ':') ||
      !parse_integer(&reader, &more) || (more != 0 && more != 1))
    return HTS_METADATA_MALFORMED;
  if (!take(&reader, ',')) return HTS_METADATA_MALFORMED;
  key = parse_string(context, &reader);
  if (key == NULL || strcmp(key, "records") != 0 ||
      !take(&reader, ':') || !take(&reader, '['))
    return HTS_METADATA_MALFORMED;
  first = 1;
  for (;;) {
    skip_space(&reader);
    if (reader.current < reader.end && *reader.current == ']') {
      reader.current++;
      break;
    }
    if (!first && !take(&reader, ',')) return HTS_METADATA_MALFORMED;
    if (!parse_record(context, &reader)) return HTS_METADATA_MALFORMED;
    first = 0;
  }
  if (!take(&reader, '}')) return HTS_METADATA_MALFORMED;
  skip_space(&reader);
  if (reader.current != reader.end) return HTS_METADATA_MALFORMED;
  batch->boards = context->boards;
  batch->board_count = context->board_count;
  batch->collections = context->collections;
  batch->collection_count = context->collection_count;
  batch->posts = context->posts;
  batch->post_count = context->post_count;
  batch->media = context->media;
  batch->media_count = context->media_count;
  batch->memberships = context->memberships;
  batch->membership_count = context->membership_count;
  batch->next_cursor = context->next_cursor;
  batch->has_more = (int) more;
  return HTS_METADATA_SUCCESS;
}

static int fixture_validate(void *opaque,
                            const hts_metadata_source_input *input,
                            hts_metadata_source *output) {
  const char *canonical;
  (void) opaque;
  if (input == NULL || input->base_url == NULL) return -1;
  if (strcmp(input->base_url, "https://fixture.invalid/api") != 0 &&
      strcmp(input->base_url, "https://fixture.invalid/api/") != 0)
    return -1;
  if (input->credential_ref != NULL &&
      strncmp(input->credential_ref, "env:", 4U) != 0)
    return -1;
  canonical = "https://fixture.invalid/api";
  (void) strcpy(output->canonical_base_url, canonical);
  (void) strcpy(output->origin, "https://fixture.invalid");
  if (input->configured_board != NULL)
    (void) strcpy(output->configured_board, input->configured_board);
  return 0;
}

static int fixture_prepare(void *opaque,
                           const hts_metadata_source *source,
                           hts_metadata_operation operation,
                           const char *resource_id,
                           const char *opaque_cursor,
                           hts_metadata_http_request *request) {
  int count;
  (void) opaque;
  (void) operation;
  count = snprintf(request->url, sizeof(request->url),
                   "%s/metadata/%s?cursor=%s",
                   source->canonical_base_url, resource_id,
                   opaque_cursor != NULL ? opaque_cursor : "");
  return count < 0 || (size_t) count >= sizeof(request->url) ? -1 : 0;
}

static void fixture_release(void *opaque, hts_metadata_batch *batch) {
  (void) opaque;
  (void) batch;
}

static int read_fixture(const char *name, char *buffer, size_t buffer_size,
                        size_t *output_size) {
  const char *root;
  char path[1024];
  FILE *file;
  size_t used;
  root = getenv("HTTRACK_FIXTURE_ROOT");
  if (root == NULL) root = "tests/fixtures/catalog";
  {
    int written = snprintf(path, sizeof(path), "%s/%s", root, name);
    if (written < 0 || (size_t) written >= sizeof(path)) return 0;
  }
  file = fopen(path, "rb");
  if (file == NULL) return 0;
  used = fread(buffer, 1U, buffer_size, file);
  if (ferror(file) || !feof(file)) {
    (void) fclose(file);
    return 0;
  }
  (void) fclose(file);
  *output_size = used;
  return 1;
}

static hts_metadata_transport_status fake_perform(
    void *opaque, const hts_metadata_http_request *request,
    hts_metadata_http_response *response) {
  fake_transport_context *context;
  const fake_plan *plan;
  size_t body_size;
  context = (fake_transport_context *) opaque;
  if (context->plan_index >= context->plan_count) {
    context->failed = 1;
    return HTS_METADATA_TRANSPORT_PERMANENT_ERROR;
  }
  plan = &context->plans[context->plan_index++];
  if (strcmp(request->url, plan->url) != 0 ||
      request->credential_ref == NULL ||
      strcmp(request->credential_ref, "env:FIXTURE_TOKEN") != 0 ||
      request->user_agent == NULL ||
      strcmp(request->user_agent, "HTTrack-Imageboard-Catalog-Test/2") != 0 ||
      request->maximum_body_bytes != FIXTURE_BODY ||
      request->redirect_limit != 3U ||
      request->connect_timeout_ms != 1000U ||
      request->request_timeout_ms != 5000U ||
      request->origin_concurrency_limit != 2U ||
      request->cancel.is_cancelled == NULL ||
      ((request->if_none_match == NULL) !=
       (plan->if_none_match == NULL)) ||
      (request->if_none_match != NULL &&
       strcmp(request->if_none_match, plan->if_none_match) != 0)) {
    context->failed = 1;
    return HTS_METADATA_TRANSPORT_PERMANENT_ERROR;
  }
  response->status_code = plan->status_code;
  response->etag = plan->etag;
  response->retry_after_ms = plan->retry_after_ms;
  if (plan->fixture_name != NULL) {
    if (!read_fixture(plan->fixture_name, context->body,
                      sizeof(context->body), &body_size)) {
      context->failed = 1;
      return HTS_METADATA_TRANSPORT_PERMANENT_ERROR;
    }
    response->body = (const unsigned char *) context->body;
    response->body_size = body_size;
  }
  return plan->transport_status;
}

static uint64_t fake_now(void *opaque) {
  return ((fake_clock_context *) opaque)->now_ms;
}

static int fake_sleep(void *opaque, uint64_t delay_ms) {
  fake_clock_context *context;
  context = (fake_clock_context *) opaque;
  context->now_ms += delay_ms;
  context->total_sleep_ms += delay_ms;
  context->sleep_count++;
  if (context->cancel_on_sleep) {
    context->cancelled = 1;
    return 1;
  }
  return 0;
}

static int fake_cancelled(void *opaque) {
  return ((fake_clock_context *) opaque)->cancelled;
}

static void collect_report(void *opaque, const hts_metadata_event *event) {
  report_context *context;
  context = (report_context *) opaque;
  context->events++;
  if (event->type == HTS_METADATA_EVENT_RETRY) context->retries++;
  if (event->type == HTS_METADATA_EVENT_PAGE_COMMITTED) context->committed++;
  if (event->message == NULL ||
      strstr(event->message, "FIXTURE_TOKEN") != NULL ||
      strstr(event->message, "fixture post") != NULL ||
      strstr(event->message, "Authorization") != NULL)
    context->safe = 0;
}

static int collect_list(void *opaque, const hts_catalog_entry *entry) {
  list_context *context;
  context = (list_context *) opaque;
  if (context->count < sizeof(context->positions) /
                         sizeof(context->positions[0]))
    context->positions[context->count] = entry->position;
  context->count++;
  return 0;
}

static int count_entity(hts_catalog *catalog, hts_catalog_entity entity,
                        int64_t parent_id, size_t *count) {
  list_context context;
  (void) memset(&context, 0, sizeof(context));
  if (hts_catalog_list(catalog, entity, parent_id,
                       collect_list, &context) != HTS_CATALOG_OK)
    return 0;
  *count = context.count;
  return 1;
}

static int make_temp_path(char *path, size_t path_size) {
  int fd;
  if (snprintf(path, path_size, "/tmp/httrack-metadata-XXXXXX") < 0)
    return 0;
  fd = mkstemp(path);
  if (fd < 0) return 0;
  (void) close(fd);
  (void) unlink(path);
  return 1;
}

static void set_plans(fake_transport_context *transport,
                      const fake_plan *plans, size_t count) {
  transport->plans = plans;
  transport->plan_count = count;
  transport->plan_index = 0U;
  transport->failed = 0;
}

static hts_metadata_policy default_policy(void) {
  hts_metadata_policy policy;
  (void) memset(&policy, 0, sizeof(policy));
  policy.requests_per_minute = 6000U;
  policy.maximum_concurrency = 2U;
  policy.base_backoff_ms = 100U;
  policy.maximum_backoff_ms = 2000U;
  policy.maximum_attempts = 3U;
  policy.maximum_body_bytes = FIXTURE_BODY;
  policy.redirect_limit = 3U;
  policy.connect_timeout_ms = 1000U;
  policy.request_timeout_ms = 5000U;
  policy.jitter_seed = 7U;
  policy.user_agent = "HTTrack-Imageboard-Catalog-Test/2";
  return policy;
}

static int direct_parser_tests(hts_metadata_adapter *adapter) {
  fake_transport_context fixture;
  hts_metadata_http_response response;
  hts_metadata_batch batch;
  size_t size;
  (void) memset(&fixture, 0, sizeof(fixture));
  (void) memset(&response, 0, sizeof(response));
  CHECK(read_fixture("incremental.json", fixture.body,
                     sizeof(fixture.body), &size));
  response.status_code = 200;
  response.body = (const unsigned char *) fixture.body;
  response.body_size = size;
  CHECK(adapter->parse_response(adapter->context,
                                HTS_METADATA_ENUMERATE_POSTS,
                                &response, &batch) == HTS_METADATA_SUCCESS);
  CHECK(batch.collection_count == 1U);
  CHECK(strcmp(batch.collections[0].record.lifecycle_state,
               "archived") == 0);
  CHECK(strcmp(batch.next_cursor, "inc-2") == 0);
  CHECK(read_fixture("malformed.json", fixture.body,
                     sizeof(fixture.body), &size));
  response.body_size = size;
  CHECK(adapter->parse_response(adapter->context,
                                HTS_METADATA_ENUMERATE_POSTS,
                                &response, &batch) == HTS_METADATA_MALFORMED);
  return 0;
}

static int run_metadata_tests(void) {
  static const fake_plan initial_plans[] = {
    {
      "https://fixture.invalid/api/metadata/main?cursor=", NULL,
      HTS_METADATA_TRANSPORT_OK, 200, "page1.json", "etag-1", 0U
    },
    {
      "https://fixture.invalid/api/metadata/main?cursor=page-2", "etag-1",
      HTS_METADATA_TRANSPORT_OK, 503, NULL, NULL, 0U
    },
    {
      "https://fixture.invalid/api/metadata/main?cursor=page-2", "etag-1",
      HTS_METADATA_TRANSPORT_OK, 200, "page2.json", "etag-2", 0U
    }
  };
  static const fake_plan not_modified_plan[] = {
    {
      "https://fixture.invalid/api/metadata/main?cursor=inc-1", "etag-2",
      HTS_METADATA_TRANSPORT_OK, 304, NULL, "etag-2", 0U
    }
  };
  static const fake_plan rerun_plans[] = {
    {
      "https://fixture.invalid/api/metadata/rerun?cursor=", NULL,
      HTS_METADATA_TRANSPORT_OK, 429, NULL, NULL, 500U
    },
    {
      "https://fixture.invalid/api/metadata/rerun?cursor=", NULL,
      HTS_METADATA_TRANSPORT_OK, 200, "page1.json", "etag-r1", 0U
    },
    {
      "https://fixture.invalid/api/metadata/rerun?cursor=page-2", "etag-r1",
      HTS_METADATA_TRANSPORT_OK, 200, "page2.json", "etag-r2", 0U
    }
  };
  static const fake_plan incremental_plan[] = {
    {
      "https://fixture.invalid/api/metadata/main?cursor=inc-1", "etag-2",
      HTS_METADATA_TRANSPORT_OK, 200, "incremental.json", "etag-3", 0U
    }
  };
  static const fake_plan malformed_plan[] = {
    {
      "https://fixture.invalid/api/metadata/malformed?cursor=", NULL,
      HTS_METADATA_TRANSPORT_OK, 200, "malformed.json", NULL, 0U
    }
  };
  static const fake_plan rollback_plan[] = {
    {
      "https://fixture.invalid/api/metadata/rollback?cursor=", NULL,
      HTS_METADATA_TRANSPORT_OK, 200, "rollback.json", NULL, 0U
    }
  };
  static const fake_plan cancel_plan[] = {
    {
      "https://fixture.invalid/api/metadata/cancel?cursor=", NULL,
      HTS_METADATA_TRANSPORT_OK, 503, NULL, NULL, 0U
    }
  };
  char path[128];
  hts_catalog *catalog;
  hts_catalog_source catalog_source;
  hts_catalog_sync_cursor incompatible;
  hts_catalog_sync_state state;
  fixture_adapter_context fixture_adapter;
  hts_metadata_adapter adapter;
  fake_transport_context fake_transport;
  hts_metadata_transport transport;
  fake_clock_context clock_context;
  hts_metadata_clock clock;
  report_context reports;
  hts_metadata_runtime *runtime;
  hts_metadata_sync_request request;
  hts_metadata_sync_result result;
  int64_t source_id;
  int64_t ignored_id;
  int64_t collection_id;
  size_t count;
  size_t before;
  list_context ordering;

  CHECK(hts_metadata_classify_http_status(200) == HTS_METADATA_SUCCESS);
  CHECK(hts_metadata_classify_http_status(304) == HTS_METADATA_NOT_MODIFIED);
  CHECK(hts_metadata_classify_http_status(401) == HTS_METADATA_UNAUTHORIZED);
  CHECK(hts_metadata_classify_http_status(403) == HTS_METADATA_FORBIDDEN);
  CHECK(hts_metadata_classify_http_status(404) ==
        HTS_METADATA_GONE_OR_NOT_FOUND);
  CHECK(hts_metadata_classify_http_status(410) ==
        HTS_METADATA_GONE_OR_NOT_FOUND);
  CHECK(hts_metadata_classify_http_status(429) ==
        HTS_METADATA_RATE_LIMITED);
  CHECK(hts_metadata_classify_http_status(503) ==
        HTS_METADATA_TEMPORARILY_UNAVAILABLE);
  CHECK(hts_metadata_classify_http_status(418) ==
        HTS_METADATA_PERMANENT_CONFIGURATION_ERROR);

  (void) memset(&fixture_adapter, 0, sizeof(fixture_adapter));
  (void) memset(&adapter, 0, sizeof(adapter));
  adapter.identifier = "fixture";
  adapter.adapter_version = "fixture-2";
  adapter.cursor_version = 7;
  adapter.capabilities =
      HTS_METADATA_CAP_THREADS | HTS_METADATA_CAP_POOLS |
      HTS_METADATA_CAP_SAVED_QUERIES |
      HTS_METADATA_CAP_INCREMENTAL_CHANGES |
      HTS_METADATA_CAP_CONDITIONAL_REQUESTS |
      HTS_METADATA_CAP_AUTHENTICATION |
      HTS_METADATA_CAP_PRESERVATION_ELIGIBLE;
  adapter.context = &fixture_adapter;
  adapter.validate_and_canonicalize = fixture_validate;
  adapter.prepare_request = fixture_prepare;
  adapter.parse_response = fixture_parse;
  adapter.release_batch = fixture_release;
  CHECK(direct_parser_tests(&adapter) == 0);

  CHECK(make_temp_path(path, sizeof(path)));
  catalog = NULL;
  CHECK(hts_catalog_open(path, &catalog) == HTS_CATALOG_OK);
  (void) memset(&catalog_source, 0, sizeof(catalog_source));
  catalog_source.adapter_kind = "fixture";
  catalog_source.canonical_base_url = "https://fixture.invalid/api";
  catalog_source.display_name = "Fixture";
  catalog_source.enabled = 1;
  catalog_source.policy_json = "{\"requests_per_minute\":6000}";
  catalog_source.credential_ref = "env:FIXTURE_TOKEN";
  catalog_source.metadata_json = "{}";
  CHECK(hts_catalog_upsert_source(catalog, &catalog_source,
                                  &source_id) == HTS_CATALOG_OK);

  (void) memset(&fake_transport, 0, sizeof(fake_transport));
  transport.context = &fake_transport;
  transport.perform = fake_perform;
  (void) memset(&clock_context, 0, sizeof(clock_context));
  clock_context.now_ms = 10000U;
  clock.context = &clock_context;
  clock.now_ms = fake_now;
  clock.sleep_ms = fake_sleep;
  (void) memset(&reports, 0, sizeof(reports));
  reports.safe = 1;
  runtime = NULL;
  CHECK(hts_metadata_runtime_create(catalog, &adapter, &transport, &clock,
                                    collect_report, &reports,
                                    &runtime));

  (void) memset(&request, 0, sizeof(request));
  request.source_id = source_id;
  request.operation = HTS_METADATA_ENUMERATE_POSTS;
  request.resource_kind = "posts";
  request.resource_id = "main";
  request.source.base_url = "https://fixture.invalid/api/";
  request.source.configured_board = "b1";
  request.source.credential_ref = "env:FIXTURE_TOKEN";
  request.policy = default_policy();
  request.cancel.context = &clock_context;
  request.cancel.is_cancelled = fake_cancelled;
  CHECK(hts_metadata_policy_validate(&request.policy));

  set_plans(&fake_transport, initial_plans,
            sizeof(initial_plans) / sizeof(initial_plans[0]));
  CHECK(hts_metadata_runtime_sync(runtime, &request,
                                  &result) == HTS_METADATA_SUCCESS);
  CHECK(result.pages_committed == 2U);
  CHECK(result.records_committed == 9U);
  CHECK(result.requests_made == 3U);
  CHECK(fake_transport.plan_index == fake_transport.plan_count);
  CHECK(!fake_transport.failed);
  CHECK(clock_context.sleep_count >= 2U);
  CHECK(reports.retries >= 1U && reports.committed == 2U && reports.safe);
  CHECK(hts_catalog_get_sync_cursor(catalog, source_id, "posts", "main",
                                    &state) == HTS_CATALOG_OK);
  CHECK(strcmp(state.cursor_value, "inc-1") == 0);
  CHECK(strcmp(state.adapter_version, "fixture-2") == 0);
  CHECK(state.cursor_version == 7);
  CHECK(count_entity(catalog, HTS_CATALOG_ENTITY_BOARD,
                     source_id, &count) && count == 1U);
  CHECK(count_entity(catalog, HTS_CATALOG_ENTITY_COLLECTION,
                     source_id, &count) && count == 2U);
  CHECK(hts_catalog_find_id(catalog, HTS_CATALOG_ENTITY_COLLECTION,
                            source_id, "thread", "t1",
                            &collection_id) == HTS_CATALOG_OK);
  CHECK(count_entity(catalog, HTS_CATALOG_ENTITY_POST,
                     collection_id, &count) && count == 1U);

  hts_metadata_runtime_destroy(runtime);
  hts_catalog_close(catalog);
  catalog = NULL;
  CHECK(hts_catalog_open(path, &catalog) == HTS_CATALOG_OK);
  CHECK(hts_metadata_runtime_create(catalog, &adapter, &transport, &clock,
                                    collect_report, &reports,
                                    &runtime));

  set_plans(&fake_transport, not_modified_plan, 1U);
  CHECK(hts_metadata_runtime_sync(runtime, &request,
                                  &result) == HTS_METADATA_NOT_MODIFIED);
  CHECK(result.not_modified);
  CHECK(!fake_transport.failed && fake_transport.plan_index == 1U);

  request.policy.minimum_refresh_interval_ms = 1000U;
  set_plans(&fake_transport, NULL, 0U);
  CHECK(hts_metadata_runtime_sync(runtime, &request,
                                  &result) == HTS_METADATA_SUCCESS);
  CHECK(result.skipped_refresh && fake_transport.plan_index == 0U);
  request.policy.minimum_refresh_interval_ms = 0U;

  request.resource_id = "rerun";
  set_plans(&fake_transport, rerun_plans,
            sizeof(rerun_plans) / sizeof(rerun_plans[0]));
  CHECK(hts_metadata_runtime_sync(runtime, &request,
                                  &result) == HTS_METADATA_SUCCESS);
  CHECK(!fake_transport.failed);
  CHECK(count_entity(catalog, HTS_CATALOG_ENTITY_BOARD,
                     source_id, &count) && count == 1U);
  CHECK(count_entity(catalog, HTS_CATALOG_ENTITY_COLLECTION,
                     source_id, &count) && count == 2U);

  request.resource_id = "main";
  set_plans(&fake_transport, incremental_plan, 1U);
  CHECK(hts_metadata_runtime_sync(runtime, &request,
                                  &result) == HTS_METADATA_SUCCESS);
  CHECK(!fake_transport.failed && result.pages_committed == 1U);
  CHECK(hts_catalog_get_sync_cursor(catalog, source_id, "posts", "main",
                                    &state) == HTS_CATALOG_OK);
  CHECK(strcmp(state.cursor_value, "inc-2") == 0);
  CHECK(count_entity(catalog, HTS_CATALOG_ENTITY_POST,
                     collection_id, &count) && count == 2U);
  (void) memset(&ordering, 0, sizeof(ordering));
  CHECK(hts_catalog_list(catalog, HTS_CATALOG_ENTITY_COLLECTION_MEDIA,
                         collection_id, collect_list,
                         &ordering) == HTS_CATALOG_OK);
  CHECK(ordering.count == 2U);
  CHECK(ordering.positions[0] == 0 && ordering.positions[1] == 1);

  CHECK(count_entity(catalog, HTS_CATALOG_ENTITY_BOARD,
                     source_id, &before));
  request.resource_id = "malformed";
  set_plans(&fake_transport, malformed_plan, 1U);
  CHECK(hts_metadata_runtime_sync(runtime, &request,
                                  &result) == HTS_METADATA_MALFORMED);
  CHECK(count_entity(catalog, HTS_CATALOG_ENTITY_BOARD,
                     source_id, &count) && count == before);
  CHECK(hts_catalog_get_sync_cursor(catalog, source_id, "posts", "malformed",
                                    &state) == HTS_CATALOG_NOT_FOUND);

  request.resource_id = "rollback";
  set_plans(&fake_transport, rollback_plan, 1U);
  CHECK(hts_metadata_runtime_sync(runtime, &request,
                                  &result) == HTS_METADATA_MALFORMED);
  CHECK(count_entity(catalog, HTS_CATALOG_ENTITY_BOARD,
                     source_id, &count) && count == before);
  CHECK(hts_catalog_get_sync_cursor(catalog, source_id, "posts", "rollback",
                                    &state) == HTS_CATALOG_NOT_FOUND);

  request.resource_id = "cancel";
  set_plans(&fake_transport, cancel_plan, 1U);
  clock_context.cancel_on_sleep = 1;
  clock_context.cancelled = 0;
  CHECK(hts_metadata_runtime_sync(runtime, &request,
                                  &result) == HTS_METADATA_CANCELLED);
  CHECK(clock_context.cancelled);
  clock_context.cancel_on_sleep = 0;
  clock_context.cancelled = 0;

  (void) memset(&incompatible, 0, sizeof(incompatible));
  incompatible.source_id = source_id;
  incompatible.resource_kind = "posts";
  incompatible.resource_id = "incompatible";
  incompatible.cursor_value = "opaque";
  incompatible.cursor_version = 99;
  incompatible.adapter_version = "fixture-old";
  incompatible.last_attempt_ms = -1;
  incompatible.last_success_ms = -1;
  CHECK(hts_catalog_upsert_sync_cursor(catalog, &incompatible,
                                       &ignored_id) == HTS_CATALOG_OK);
  request.resource_id = "incompatible";
  set_plans(&fake_transport, NULL, 0U);
  CHECK(hts_metadata_runtime_sync(
            runtime, &request, &result) ==
        HTS_METADATA_PERMANENT_CONFIGURATION_ERROR);
  CHECK(fake_transport.plan_index == 0U);
  CHECK(reports.safe);

  hts_metadata_runtime_destroy(runtime);
  hts_catalog_close(catalog);
  (void) unlink(path);
  return 0;
}

int main(void) {
  return run_metadata_tests();
}
