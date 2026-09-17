#define _XOPEN_SOURCE 700

#include "htscatalog.h"
#include "htsyotsuba.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BODY_CAPACITY (1024U * 1024U)
#define ITEM_CAPACITY 64U

#define CHECK(value) do { \
  if (!(value)) { \
    (void) fprintf(stderr, "check failed at %s:%d: %s\n", \
                   __FILE__, __LINE__, #value); \
    return 1; \
  } \
} while (0)

typedef struct fake_plan {
  const char *url;
  const char *if_none_match;
  hts_metadata_transport_status transport_status;
  int status_code;
  const char *fixture;
  const char *etag;
  uint64_t retry_after_ms;
} fake_plan;

typedef struct fake_transport_context {
  const fake_plan *plans;
  size_t plan_count;
  size_t plan_index;
  unsigned char body[BODY_CAPACITY];
  uint64_t last_request_ms;
  int have_request_time;
  int failed;
  int media_request;
  struct fake_clock_context *clock;
} fake_transport_context;

typedef struct fake_clock_context {
  uint64_t now_ms;
  uint64_t total_sleep_ms;
  unsigned int sleep_count;
  int cancel_on_sleep;
  int cancelled;
} fake_clock_context;

typedef struct entry_list {
  int64_t ids[ITEM_CAPACITY];
  int64_t positions[ITEM_CAPACITY];
  size_t count;
  int deleted_file_seen;
} entry_list;

typedef struct resource_capture {
  char state[65];
  char synchronized_state[65];
  char remote_version[65];
  char synchronized_version[65];
  char error_state[65];
  unsigned int missing_count;
  int seen;
} resource_capture;

typedef struct report_capture {
  unsigned int retries;
  unsigned int errors;
  int safe;
} report_capture;

static int fixture_read(const char *name, unsigned char *buffer,
                        size_t capacity, size_t *size) {
  const char *root;
  char path[1024];
  FILE *file;
  int length;
  root = getenv("HTTRACK_YOTSUBA_FIXTURE_ROOT");
  if (root == NULL) root = "tests/fixtures/yotsuba";
  length = snprintf(path, sizeof(path), "%s/%s", root, name);
  if (length < 0 || (size_t) length >= sizeof(path)) return 0;
  file = fopen(path, "rb");
  if (file == NULL) return 0;
  *size = fread(buffer, 1U, capacity, file);
  if (ferror(file) || !feof(file)) {
    (void) fclose(file);
    return 0;
  }
  (void) fclose(file);
  return 1;
}

static hts_metadata_transport_status fake_perform(
    void *opaque, const hts_metadata_http_request *request,
    hts_metadata_http_response *response) {
  fake_transport_context *context;
  const fake_plan *plan;
  size_t size;
  context = (fake_transport_context *) opaque;
  if (strstr(request->url, "i.4cdn.org") != NULL) {
    context->media_request = 1;
    context->failed = 1;
    return HTS_METADATA_TRANSPORT_PERMANENT_ERROR;
  }
  if (context->plan_index >= context->plan_count) {
    context->failed = 1;
    return HTS_METADATA_TRANSPORT_PERMANENT_ERROR;
  }
  plan = &context->plans[context->plan_index++];
  if (strcmp(request->url, plan->url) != 0 ||
      request->credential_ref != NULL || request->user_agent == NULL ||
      strcmp(request->user_agent, HTS_YOTSUBA_DEFAULT_USER_AGENT) != 0 ||
      request->origin_concurrency_limit != 1U ||
      request->redirect_limit != 3U ||
      ((request->if_none_match == NULL) != (plan->if_none_match == NULL)) ||
      (request->if_none_match != NULL &&
       strcmp(request->if_none_match, plan->if_none_match) != 0)) {
    context->failed = 1;
    return HTS_METADATA_TRANSPORT_PERMANENT_ERROR;
  }
  if (context->have_request_time &&
      context->clock->now_ms - context->last_request_ms < 1000U) {
    context->failed = 1;
    return HTS_METADATA_TRANSPORT_PERMANENT_ERROR;
  }
  context->last_request_ms = context->clock->now_ms;
  context->have_request_time = 1;
  response->status_code = plan->status_code;
  response->etag = plan->etag;
  response->retry_after_ms = plan->retry_after_ms;
  if (plan->fixture != NULL) {
    if (!fixture_read(plan->fixture, context->body,
                      sizeof(context->body), &size)) {
      context->failed = 1;
      return HTS_METADATA_TRANSPORT_PERMANENT_ERROR;
    }
    response->body = context->body;
    response->body_size = size;
  }
  return plan->transport_status;
}

static uint64_t fake_now(void *opaque) {
  return ((fake_clock_context *) opaque)->now_ms;
}

static int fake_sleep(void *opaque, uint64_t delay_ms) {
  fake_clock_context *clock;
  clock = (fake_clock_context *) opaque;
  clock->now_ms += delay_ms;
  clock->total_sleep_ms += delay_ms;
  clock->sleep_count++;
  if (clock->cancel_on_sleep) clock->cancelled = 1;
  return clock->cancelled ? -1 : 0;
}

static int fake_cancelled(void *opaque) {
  return ((fake_clock_context *) opaque)->cancelled;
}

static void collect_report(void *opaque, const hts_metadata_event *event) {
  report_capture *capture;
  capture = (report_capture *) opaque;
  if (event->type == HTS_METADATA_EVENT_RETRY) capture->retries++;
  if (event->type == HTS_METADATA_EVENT_ERROR) capture->errors++;
  if (event->message == NULL ||
      strstr(event->message, "Authorization") != NULL ||
      strstr(event->message, "Synthetic fixture") != NULL ||
      strstr(event->message, "Cookie") != NULL)
    capture->safe = 0;
}

static int collect_entries(void *opaque, const hts_catalog_entry *entry) {
  entry_list *list;
  list = (entry_list *) opaque;
  if (list->count >= ITEM_CAPACITY) return 1;
  list->ids[list->count] = entry->id;
  list->positions[list->count] = entry->position;
  list->count++;
  if (entry->metadata_json != NULL &&
      strstr(entry->metadata_json, "\"file_deleted\":true") != NULL)
    list->deleted_file_seen = 1;
  return 0;
}

static int capture_resource(void *opaque,
                            const hts_catalog_resource_view *resource) {
  resource_capture *capture;
  capture = (resource_capture *) opaque;
  capture->seen = 1;
  (void) snprintf(capture->state, sizeof(capture->state), "%s",
                  resource->lifecycle_state);
  (void) snprintf(capture->synchronized_state,
                  sizeof(capture->synchronized_state), "%s",
                  resource->synchronized_state != NULL
                    ? resource->synchronized_state : "");
  (void) snprintf(capture->remote_version,
                  sizeof(capture->remote_version), "%s",
                  resource->remote_version != NULL
                    ? resource->remote_version : "");
  (void) snprintf(capture->synchronized_version,
                  sizeof(capture->synchronized_version), "%s",
                  resource->synchronized_version != NULL
                    ? resource->synchronized_version : "");
  (void) snprintf(capture->error_state, sizeof(capture->error_state), "%s",
                  resource->error_state != NULL ? resource->error_state : "");
  capture->missing_count = resource->missing_count;
  return 0;
}

static int make_temp_path(char *path, size_t size) {
  int fd;
  if (snprintf(path, size, "/tmp/httrack-yotsuba-XXXXXX") < 0) return 0;
  fd = mkstemp(path);
  if (fd < 0) return 0;
  (void) close(fd);
  (void) unlink(path);
  return 1;
}

static void set_plans(fake_transport_context *context,
                      const fake_plan *plans, size_t count) {
  context->plans = plans;
  context->plan_count = count;
  context->plan_index = 0U;
  context->failed = 0;
}

static int catalog_counts(hts_catalog *catalog, int64_t source_id,
                          size_t *collections, size_t *posts,
                          size_t *media) {
  entry_list collection_list;
  size_t i;
  (void) memset(&collection_list, 0, sizeof(collection_list));
  if (hts_catalog_list(catalog, HTS_CATALOG_ENTITY_COLLECTION, source_id,
                       collect_entries, &collection_list) != HTS_CATALOG_OK)
    return 0;
  *collections = collection_list.count;
  *posts = 0U;
  *media = 0U;
  for (i = 0U; i < collection_list.count; i++) {
    entry_list post_list;
    size_t j;
    (void) memset(&post_list, 0, sizeof(post_list));
    if (hts_catalog_list(catalog, HTS_CATALOG_ENTITY_POST,
                         collection_list.ids[i], collect_entries,
                         &post_list) != HTS_CATALOG_OK) return 0;
    *posts += post_list.count;
    for (j = 0U; j < post_list.count; j++) {
      entry_list media_list;
      (void) memset(&media_list, 0, sizeof(media_list));
      if (hts_catalog_list(catalog, HTS_CATALOG_ENTITY_MEDIA,
                           post_list.ids[j], collect_entries,
                           &media_list) != HTS_CATALOG_OK) return 0;
      *media += media_list.count;
    }
  }
  return 1;
}

static int run_discovery_test(void) {
  static const fake_plan plans[] = {
    {"https://a.4cdn.org/boards.json", NULL, HTS_METADATA_TRANSPORT_OK,
     200, "boards-discovery.json", "discover-boards", 0U},
    {"https://a.4cdn.org/plain/threads.json", NULL,
     HTS_METADATA_TRANSPORT_OK, 200, "threads-empty.json",
     "discover-threads", 0U}
  };
  char path[128];
  hts_catalog *catalog;
  hts_catalog_source source;
  int64_t source_id;
  hts_yotsuba_options options;
  hts_yotsuba_adapter *adapter;
  fake_clock_context clock_context;
  hts_metadata_clock clock;
  fake_transport_context transport_context;
  hts_metadata_transport transport;
  hts_yotsuba_sync_result result;
  entry_list board_list;
  CHECK(make_temp_path(path, sizeof(path)));
  catalog = NULL;
  CHECK(hts_catalog_open(path, &catalog) == HTS_CATALOG_OK);
  (void) memset(&source, 0, sizeof(source));
  source.adapter_kind = "yotsuba";
  source.canonical_base_url = HTS_YOTSUBA_API_ORIGIN;
  source.display_name = "Yotsuba discovery fixture";
  source.enabled = 1;
  source.metadata_json = "{\"metadata_only\":true}";
  CHECK(hts_catalog_upsert_source(catalog, &source, &source_id) ==
        HTS_CATALOG_OK);
  (void) memset(&options, 0, sizeof(options));
  options.source_id = source_id;
  options.discover_boards = 1;
  hts_yotsuba_default_policy(&options.policy);
  options.policy.jitter_percent = 0U;
  CHECK(hts_yotsuba_adapter_create(catalog, &options, &adapter));
  (void) memset(&clock_context, 0, sizeof(clock_context));
  clock_context.now_ms = 1000U;
  clock.context = &clock_context;
  clock.now_ms = fake_now;
  clock.sleep_ms = fake_sleep;
  (void) memset(&transport_context, 0, sizeof(transport_context));
  transport_context.clock = &clock_context;
  transport.context = &transport_context;
  transport.perform = fake_perform;
  set_plans(&transport_context, plans, sizeof(plans) / sizeof(plans[0]));
  CHECK(hts_yotsuba_sync(adapter, &transport, &clock, NULL, NULL, &result) ==
        HTS_METADATA_SUCCESS);
  CHECK(!transport_context.failed && !transport_context.media_request);
  CHECK(transport_context.plan_index == transport_context.plan_count);
  CHECK(result.boards_processed == 1U && result.threads_discovered == 0U);
  (void) memset(&board_list, 0, sizeof(board_list));
  CHECK(hts_catalog_list(catalog, HTS_CATALOG_ENTITY_BOARD, source_id,
                         collect_entries, &board_list) == HTS_CATALOG_OK);
  CHECK(board_list.count == 1U);
  hts_yotsuba_adapter_destroy(adapter);
  hts_catalog_close(catalog);
  (void) unlink(path);
  return 0;
}

static int run_tests(void) {
  static const fake_plan initial[] = {
    {"https://a.4cdn.org/boards.json", NULL, HTS_METADATA_TRANSPORT_OK,
     200, "boards.json", "b1", 0U},
    {"https://a.4cdn.org/safe/threads.json", NULL, HTS_METADATA_TRANSPORT_OK,
     200, "threads-initial.json", "t1", 0U},
    {"https://a.4cdn.org/safe/archive.json", NULL, HTS_METADATA_TRANSPORT_OK,
     200, "archive-empty.json", "a1", 0U},
    {"https://a.4cdn.org/safe/thread/100.json", NULL,
     HTS_METADATA_TRANSPORT_OK, 200, "thread-100.json", "x100", 0U},
    {"https://a.4cdn.org/safe/thread/200.json", NULL,
     HTS_METADATA_TRANSPORT_OK, 200, "thread-200.json", "x200", 0U}
  };
  static const fake_plan incremental[] = {
    {"https://a.4cdn.org/boards.json", "b1", HTS_METADATA_TRANSPORT_OK,
     304, NULL, "b1", 0U},
    {"https://a.4cdn.org/safe/threads.json", "t1",
     HTS_METADATA_TRANSPORT_OK, 200, "threads-incremental.json", "t2", 0U},
    {"https://a.4cdn.org/safe/archive.json", "a1",
     HTS_METADATA_TRANSPORT_OK, 200, "archive-100.json", "a2", 0U},
    {"https://a.4cdn.org/safe/thread/100.json", "x100",
     HTS_METADATA_TRANSPORT_OK, 304, NULL, "x100", 0U},
    {"https://a.4cdn.org/safe/thread/200.json", "x200",
     HTS_METADATA_TRANSPORT_OK, 304, NULL, "x200", 0U},
    {"https://a.4cdn.org/safe/thread/300.json", NULL,
     HTS_METADATA_TRANSPORT_OK, 503, NULL, NULL, 4000U},
    {"https://a.4cdn.org/safe/thread/300.json", NULL,
     HTS_METADATA_TRANSPORT_OK, 200, "thread-300.json", "x300", 0U}
  };
  static const fake_plan missing_once[] = {
    {"https://a.4cdn.org/boards.json", "b1", HTS_METADATA_TRANSPORT_OK,
     304, NULL, "b1", 0U},
    {"https://a.4cdn.org/safe/threads.json", "t2",
     HTS_METADATA_TRANSPORT_OK, 200, "threads-steady.json", "t3", 0U},
    {"https://a.4cdn.org/safe/archive.json", "a2",
     HTS_METADATA_TRANSPORT_OK, 200, "archive-empty.json", "a3", 0U}
  };
  static const fake_plan missing_twice[] = {
    {"https://a.4cdn.org/boards.json", "b1", HTS_METADATA_TRANSPORT_OK,
     304, NULL, "b1", 0U},
    {"https://a.4cdn.org/safe/threads.json", "t3",
     HTS_METADATA_TRANSPORT_OK, 200, "threads-steady.json", "t4", 0U},
    {"https://a.4cdn.org/safe/archive.json", "a3",
     HTS_METADATA_TRANSPORT_OK, 200, "archive-empty.json", "a4", 0U}
  };
  static const fake_plan thread_404[] = {
    {"https://a.4cdn.org/boards.json", "b1", HTS_METADATA_TRANSPORT_OK,
     304, NULL, "b1", 0U},
    {"https://a.4cdn.org/safe/threads.json", "t4",
     HTS_METADATA_TRANSPORT_OK, 200, "threads-404.json", "t5", 0U},
    {"https://a.4cdn.org/safe/archive.json", "a4",
     HTS_METADATA_TRANSPORT_OK, 200, "archive-empty.json", "a5", 0U},
    {"https://a.4cdn.org/safe/thread/300.json", "x300",
     HTS_METADATA_TRANSPORT_OK, 404, NULL, NULL, 0U}
  };
  static const fake_plan rerun[] = {
    {"https://a.4cdn.org/boards.json", "b1", HTS_METADATA_TRANSPORT_OK,
     304, NULL, "b1", 0U},
    {"https://a.4cdn.org/safe/threads.json", "t5",
     HTS_METADATA_TRANSPORT_OK, 304, NULL, "t5", 0U},
    {"https://a.4cdn.org/safe/archive.json", "a5",
     HTS_METADATA_TRANSPORT_OK, 304, NULL, "a5", 0U}
  };
  static const fake_plan malformed[] = {
    {"https://a.4cdn.org/boards.json", "b1", HTS_METADATA_TRANSPORT_OK,
     200, "malformed.json", "broken", 0U}
  };
  static const fake_plan cancelled[] = {
    {"https://a.4cdn.org/boards.json", "b1", HTS_METADATA_TRANSPORT_OK,
     503, NULL, NULL, 0U}
  };
  const char *boards[] = {"safe"};
  char path[128];
  hts_catalog *catalog;
  hts_catalog_source source;
  int64_t source_id;
  hts_yotsuba_options options;
  hts_yotsuba_adapter *adapter;
  const hts_metadata_adapter *contract;
  hts_metadata_source_input source_input;
  hts_metadata_source normalized;
  fake_clock_context clock_context;
  hts_metadata_clock clock;
  fake_transport_context transport_context;
  hts_metadata_transport transport;
  report_capture reports;
  hts_yotsuba_sync_result result;
  size_t collections;
  size_t posts;
  size_t media;
  int64_t collection_id;
  entry_list ordered;
  resource_capture resource;
  hts_metadata_category category;

  CHECK(run_discovery_test() == 0);

  CHECK(make_temp_path(path, sizeof(path)));
  catalog = NULL;
  CHECK(hts_catalog_open(path, &catalog) == HTS_CATALOG_OK);
  (void) memset(&source, 0, sizeof(source));
  source.adapter_kind = "yotsuba";
  source.canonical_base_url = HTS_YOTSUBA_API_ORIGIN;
  source.display_name = "4chan read-only metadata";
  source.enabled = 1;
  source.policy_json = "{\"metadata_only\":true}";
  source.metadata_json =
      "{\"source\":\"4chan\",\"official_adapter\":false}";
  CHECK(hts_catalog_upsert_source(catalog, &source, &source_id) ==
        HTS_CATALOG_OK);
  (void) memset(&options, 0, sizeof(options));
  options.source_id = source_id;
  options.boards = boards;
  options.board_count = 1U;
  options.missing_confirmations = 2U;
  hts_yotsuba_default_policy(&options.policy);
  options.policy.jitter_percent = 0U;
  CHECK(hts_yotsuba_adapter_create(catalog, &options, &adapter));
  contract = hts_yotsuba_adapter_contract(adapter);
  CHECK(contract != NULL);
  CHECK((contract->capabilities & HTS_METADATA_CAP_THREADS) != 0U);
  CHECK((contract->capabilities & HTS_METADATA_CAP_CONDITIONAL_REQUESTS) != 0U);
  CHECK((contract->capabilities & HTS_METADATA_CAP_AUTHENTICATION) == 0U);
  (void) memset(&source_input, 0, sizeof(source_input));
  source_input.base_url = HTS_YOTSUBA_API_ORIGIN "/";
  source_input.configured_board = "safe";
  CHECK(hts_metadata_adapter_validate_source(contract, &source_input,
                                              &normalized));
  CHECK(strcmp(normalized.canonical_base_url, HTS_YOTSUBA_API_ORIGIN) == 0);
  source_input.credential_ref = "env:must-not-be-used";
  CHECK(!hts_metadata_adapter_validate_source(contract, &source_input,
                                               &normalized));
  source_input.credential_ref = NULL;

  (void) memset(&clock_context, 0, sizeof(clock_context));
  clock_context.now_ms = 100000U;
  clock.context = &clock_context;
  clock.now_ms = fake_now;
  clock.sleep_ms = fake_sleep;
  (void) memset(&transport_context, 0, sizeof(transport_context));
  transport_context.clock = &clock_context;
  transport.context = &transport_context;
  transport.perform = fake_perform;
  (void) memset(&reports, 0, sizeof(reports));
  reports.safe = 1;

  set_plans(&transport_context, initial, sizeof(initial) / sizeof(initial[0]));
  category = hts_yotsuba_sync(adapter, &transport, &clock, collect_report,
                              &reports, &result);
  if (category != HTS_METADATA_SUCCESS)
    (void) fprintf(stderr, "initial category=%d plan=%lu failed=%d\n",
                   (int) category, (unsigned long) transport_context.plan_index,
                   transport_context.failed);
  CHECK(category == HTS_METADATA_SUCCESS);
  CHECK(!transport_context.failed && !transport_context.media_request);
  CHECK(transport_context.plan_index == transport_context.plan_count);
  CHECK(result.threads_discovered == 2U && result.threads_fetched == 2U);
  CHECK(result.posts_committed == 5U && result.media_variants_committed == 6U);
  CHECK(catalog_counts(catalog, source_id, &collections, &posts, &media));
  CHECK(collections == 2U && posts == 5U && media == 6U);
  CHECK(clock_context.total_sleep_ms >= 8000U);

  CHECK(hts_catalog_find_id(catalog, HTS_CATALOG_ENTITY_COLLECTION,
                            source_id, "thread", "safe/100",
                            &collection_id) == HTS_CATALOG_OK);
  (void) memset(&ordered, 0, sizeof(ordered));
  CHECK(hts_catalog_list(catalog, HTS_CATALOG_ENTITY_POST, collection_id,
                         collect_entries, &ordered) == HTS_CATALOG_OK);
  CHECK(ordered.count == 3U && ordered.positions[0] == 0 &&
        ordered.positions[1] == 1 && ordered.positions[2] == 2);
  CHECK(ordered.deleted_file_seen);
  (void) memset(&ordered, 0, sizeof(ordered));
  CHECK(hts_catalog_list(catalog, HTS_CATALOG_ENTITY_COLLECTION_MEDIA,
                         collection_id, collect_entries,
                         &ordered) == HTS_CATALOG_OK);
  CHECK(ordered.count == 4U && ordered.positions[0] == 0 &&
        ordered.positions[1] == 1 && ordered.positions[2] == 2 &&
        ordered.positions[3] == 3);

  hts_yotsuba_adapter_destroy(adapter);
  clock_context.now_ms += 60000U;
  transport_context.have_request_time = 0;
  CHECK(hts_yotsuba_adapter_create(catalog, &options, &adapter));
  set_plans(&transport_context, incremental,
            sizeof(incremental) / sizeof(incremental[0]));
  CHECK(hts_yotsuba_sync(adapter, &transport, &clock, collect_report, &reports,
                         &result) == HTS_METADATA_SUCCESS);
  CHECK(!transport_context.failed && !transport_context.media_request);
  CHECK(result.threads_fetched == 1U && result.threads_not_modified == 2U);
  CHECK(result.posts_committed == 1U && result.media_variants_committed == 2U);
  CHECK(reports.retries >= 1U && reports.safe);
  CHECK(catalog_counts(catalog, source_id, &collections, &posts, &media));
  CHECK(collections == 3U && posts == 6U && media == 8U);

  clock_context.now_ms += 60000U;
  transport_context.have_request_time = 0;
  set_plans(&transport_context, missing_once,
            sizeof(missing_once) / sizeof(missing_once[0]));
  CHECK(hts_yotsuba_sync(adapter, &transport, &clock, collect_report, &reports,
                         &result) == HTS_METADATA_SUCCESS);
  (void) memset(&resource, 0, sizeof(resource));
  CHECK(hts_catalog_get_resource_state(catalog, source_id, "yotsuba-thread",
                                       "safe/100", capture_resource,
                                       &resource) == HTS_CATALOG_OK);
  CHECK(resource.seen && strcmp(resource.state, "missing") == 0 &&
        resource.missing_count == 1U);

  clock_context.now_ms += 60000U;
  transport_context.have_request_time = 0;
  set_plans(&transport_context, missing_twice,
            sizeof(missing_twice) / sizeof(missing_twice[0]));
  CHECK(hts_yotsuba_sync(adapter, &transport, &clock, collect_report, &reports,
                         &result) == HTS_METADATA_SUCCESS);
  (void) memset(&resource, 0, sizeof(resource));
  CHECK(hts_catalog_get_resource_state(catalog, source_id, "yotsuba-thread",
                                       "safe/100", capture_resource,
                                       &resource) == HTS_CATALOG_OK);
  CHECK(strcmp(resource.state, "expired") == 0 &&
        resource.missing_count == 2U);
  CHECK(catalog_counts(catalog, source_id, &collections, &posts, &media));
  CHECK(collections == 3U && posts == 6U && media == 8U);

  clock_context.now_ms += 60000U;
  transport_context.have_request_time = 0;
  set_plans(&transport_context, thread_404,
            sizeof(thread_404) / sizeof(thread_404[0]));
  CHECK(hts_yotsuba_sync(adapter, &transport, &clock, collect_report, &reports,
                         &result) == HTS_METADATA_SUCCESS);
  (void) memset(&resource, 0, sizeof(resource));
  CHECK(hts_catalog_get_resource_state(catalog, source_id, "yotsuba-thread",
                                       "safe/300", capture_resource,
                                       &resource) == HTS_CATALOG_OK);
  CHECK(strcmp(resource.state, "missing") == 0 &&
        strcmp(resource.error_state, "gone_or_not_found") == 0 &&
        resource.missing_count == 1U);

  clock_context.now_ms += 60000U;
  transport_context.have_request_time = 0;
  set_plans(&transport_context, rerun, sizeof(rerun) / sizeof(rerun[0]));
  CHECK(hts_yotsuba_sync(adapter, &transport, &clock, collect_report, &reports,
                         &result) == HTS_METADATA_SUCCESS);
  CHECK(catalog_counts(catalog, source_id, &collections, &posts, &media));
  CHECK(collections == 3U && posts == 6U && media == 8U);

  clock_context.now_ms += 60000U;
  transport_context.have_request_time = 0;
  set_plans(&transport_context, malformed,
            sizeof(malformed) / sizeof(malformed[0]));
  CHECK(hts_yotsuba_sync(adapter, &transport, &clock, collect_report, &reports,
                         &result) == HTS_METADATA_MALFORMED);
  CHECK(catalog_counts(catalog, source_id, &collections, &posts, &media));
  CHECK(collections == 3U && posts == 6U && media == 8U);

  hts_yotsuba_adapter_destroy(adapter);
  clock_context.now_ms += 60000U;
  clock_context.cancel_on_sleep = 1;
  clock_context.cancelled = 0;
  options.cancel.context = &clock_context;
  options.cancel.is_cancelled = fake_cancelled;
  CHECK(hts_yotsuba_adapter_create(catalog, &options, &adapter));
  transport_context.have_request_time = 0;
  set_plans(&transport_context, cancelled,
            sizeof(cancelled) / sizeof(cancelled[0]));
  CHECK(hts_yotsuba_sync(adapter, &transport, &clock, collect_report, &reports,
                         &result) == HTS_METADATA_CANCELLED);
  CHECK(transport_context.plan_index == 1U);
  CHECK(!transport_context.media_request && reports.safe);

  hts_yotsuba_adapter_destroy(adapter);
  hts_catalog_close(catalog);
  (void) unlink(path);
  return 0;
}

int main(void) {
  return run_tests();
}
