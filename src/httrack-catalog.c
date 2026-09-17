#define _POSIX_C_SOURCE 200809L

#include "htscatalog.h"
#include "htscatalogpath.h"
#include "htsyotsuba.h"

#include <curl/curl.h>
#include <errno.h>
#include <signal.h>
#include <sqlite3.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define CLI_LIMIT_DEFAULT 50
#define CLI_LIMIT_MAX 500
#define CLI_SOURCE_BOARDS_MAX 512U

typedef struct cli_context {
  const char *catalog_path;
  int json;
  sqlite3 *db;
} cli_context;

typedef struct query_filter {
  int64_t source_id;
  const char *board;
  const char *kind;
  const char *remote_id;
  const char *query;
  const char *tag;
  const char *rating;
  const char *created_from;
  const char *created_to;
  const char *updated_from;
  const char *updated_to;
  const char *media_type;
  int minimum_width;
  int maximum_width;
  int minimum_height;
  int maximum_height;
  int64_t minimum_bytes;
  int64_t maximum_bytes;
  const char *lifecycle;
  const char *availability;
  int limit;
  int offset;
} query_filter;

typedef struct http_buffer {
  unsigned char *data;
  size_t size;
  size_t capacity;
  size_t maximum;
  int overflow;
} http_buffer;

typedef struct curl_transport_context {
  http_buffer body;
  char etag[4097];
  char last_modified[4097];
  uint64_t retry_after_ms;
  const hts_metadata_http_request *request;
} curl_transport_context;

typedef struct fixture_transport_context {
  const char *root;
  unsigned char *body;
  size_t capacity;
} fixture_transport_context;

typedef struct real_clock_context {
  uint64_t origin_ms;
} real_clock_context;

typedef struct virtual_clock_context {
  uint64_t now_ms;
} virtual_clock_context;

typedef struct source_config {
  int64_t id;
  char adapter[65];
  char base_url[HTS_CATALOG_URL_MAX + 1U];
  char display_name[4097];
  char policy_json[HTS_CATALOG_METADATA_MAX + 1U];
  char metadata_json[HTS_CATALOG_METADATA_MAX + 1U];
  int enabled;
} source_config;

typedef struct sync_report_context {
  int json;
} sync_report_context;

static volatile sig_atomic_t cli_cancelled = 0;

static void handle_signal(int signal_number) {
  (void) signal_number;
  cli_cancelled = 1;
}

static void json_string(FILE *stream, const char *value) {
  const unsigned char *scan;
  (void) fputc('"', stream);
  if (value != NULL) {
    for (scan = (const unsigned char *) value; *scan != '\0'; scan++) {
      switch (*scan) {
        case '"': (void) fputs("\\\"", stream); break;
        case '\\': (void) fputs("\\\\", stream); break;
        case '\b': (void) fputs("\\b", stream); break;
        case '\f': (void) fputs("\\f", stream); break;
        case '\n': (void) fputs("\\n", stream); break;
        case '\r': (void) fputs("\\r", stream); break;
        case '\t': (void) fputs("\\t", stream); break;
        default:
          if (*scan < 32U)
            (void) fprintf(stream, "\\u%04x", (unsigned int) *scan);
          else
            (void) fputc((int) *scan, stream);
          break;
      }
    }
  }
  (void) fputc('"', stream);
}

static void print_error(const cli_context *context, const char *message) {
  (void) context;
  (void) fprintf(stderr, "httrack-catalog: %s\n", message);
}

static int parse_i64(const char *value, int64_t *output) {
  char *end;
  long long parsed;
  if (value == NULL || value[0] == '\0') return 0;
  errno = 0;
  end = NULL;
  parsed = strtoll(value, &end, 10);
  if (errno != 0 || end == value || *end != '\0') return 0;
  *output = (int64_t) parsed;
  return 1;
}

static int parse_int(const char *value, int *output) {
  int64_t parsed;
  if (!parse_i64(value, &parsed) || parsed < 0 || parsed > 2147483647)
    return 0;
  *output = (int) parsed;
  return 1;
}

static int open_catalog(cli_context *context) {
  hts_catalog *catalog;
  int status;
  catalog = NULL;
  status = hts_catalog_open(context->catalog_path, &catalog);
  if (status != HTS_CATALOG_OK) {
    print_error(context, catalog != NULL ? hts_catalog_last_error(catalog)
                                         : "could not open catalog");
    hts_catalog_close(catalog);
    return 0;
  }
  hts_catalog_close(catalog);
  if (sqlite3_open_v2(context->catalog_path, &context->db,
                      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) !=
      SQLITE_OK) {
    print_error(context, "could not open catalog database");
    if (context->db != NULL) sqlite3_close(context->db);
    context->db = NULL;
    return 0;
  }
  (void) sqlite3_exec(context->db, "PRAGMA foreign_keys=ON", NULL, NULL, NULL);
  (void) sqlite3_busy_timeout(context->db, 5000);
  return 1;
}

static void close_catalog(cli_context *context) {
  if (context->db != NULL) (void) sqlite3_close(context->db);
  context->db = NULL;
}

static int execute(cli_context *context, const char *sql) {
  char *error;
  int status;
  error = NULL;
  status = sqlite3_exec(context->db, sql, NULL, NULL, &error);
  if (status != SQLITE_OK) {
    print_error(context, error != NULL ? error : "catalog update failed");
    sqlite3_free(error);
    return 0;
  }
  return 1;
}

static int prepare_sql(cli_context *context, const char *sql,
                       sqlite3_stmt **statement) {
  if (sqlite3_prepare_v2(context->db, sql, -1, statement, NULL) != SQLITE_OK) {
    print_error(context, sqlite3_errmsg(context->db));
    return 0;
  }
  return 1;
}

static const char *column_text(sqlite3_stmt *statement, int column) {
  const unsigned char *value;
  value = sqlite3_column_text(statement, column);
  return value != NULL ? (const char *) value : "";
}

static int command_init(cli_context *context) {
  int version;
  sqlite3_stmt *statement;
  if (!open_catalog(context)) return 2;
  if (!prepare_sql(context, "PRAGMA user_version", &statement)) {
    close_catalog(context);
    return 2;
  }
  version = sqlite3_step(statement) == SQLITE_ROW
              ? sqlite3_column_int(statement, 0) : -1;
  (void) sqlite3_finalize(statement);
  if (context->json) {
    (void) fputs("{\"catalog\":", stdout);
    json_string(stdout, context->catalog_path);
    (void) fprintf(stdout, ",\"metadata_only\":true,\"schema_version\":%d}\n",
                   version);
  } else {
    (void) printf("Catalog: %s\nSchema version: %d\nMode: metadata-only\n",
                  context->catalog_path, version);
    (void) puts("Warning: ephemeral originals may disappear before acquisition.");
  }
  close_catalog(context);
  return version == HTS_CATALOG_SCHEMA_VERSION ? 0 : 2;
}

static int board_valid(const char *board) {
  const unsigned char *scan;
  if (board == NULL || board[0] == '\0') return 0;
  for (scan = (const unsigned char *) board; *scan != '\0'; scan++) {
    if (!((*scan >= 'a' && *scan <= 'z') ||
          (*scan >= '0' && *scan <= '9'))) return 0;
  }
  return 1;
}

static int append_json_text(char *output, size_t capacity, size_t *used,
                            const char *value) {
  const unsigned char *scan;
  for (scan = (const unsigned char *) value; *scan != '\0'; scan++) {
    if (*scan == '"' || *scan == '\\') {
      if (*used + 2U >= capacity) return 0;
      output[(*used)++] = '\\';
      output[(*used)++] = (char) *scan;
    } else {
      if (*used + 1U >= capacity) return 0;
      output[(*used)++] = (char) *scan;
    }
  }
  output[*used] = '\0';
  return 1;
}

static int source_add(cli_context *context, int argc, char **argv) {
  const char *adapter;
  const char *url;
  const char *name;
  const char *credential_ref;
  const char *boards[CLI_SOURCE_BOARDS_MAX];
  size_t board_count;
  int discover;
  int enabled;
  int index;
  char metadata[HTS_CATALOG_METADATA_MAX + 1U];
  size_t used;
  hts_catalog_source source;
  hts_catalog *catalog;
  int64_t source_id;
  adapter = NULL;
  url = NULL;
  name = NULL;
  credential_ref = NULL;
  board_count = 0U;
  discover = 0;
  enabled = 1;
  for (index = 0; index < argc; index++) {
    if (strcmp(argv[index], "--adapter") == 0 && index + 1 < argc)
      adapter = argv[++index];
    else if (strcmp(argv[index], "--url") == 0 && index + 1 < argc)
      url = argv[++index];
    else if (strcmp(argv[index], "--name") == 0 && index + 1 < argc)
      name = argv[++index];
    else if (strcmp(argv[index], "--credential-ref") == 0 && index + 1 < argc)
      credential_ref = argv[++index];
    else if (strcmp(argv[index], "--board") == 0 && index + 1 < argc) {
      if (board_count >= CLI_SOURCE_BOARDS_MAX || !board_valid(argv[index + 1])) {
        print_error(context, "invalid or excessive board configuration");
        return 2;
      }
      boards[board_count++] = argv[++index];
    } else if (strcmp(argv[index], "--discover-boards") == 0)
      discover = 1;
    else if (strcmp(argv[index], "--disabled") == 0)
      enabled = 0;
    else {
      print_error(context, "invalid source add option");
      return 2;
    }
  }
  if (adapter == NULL) adapter = "yotsuba";
  if (url == NULL && strcmp(adapter, "yotsuba") == 0)
    url = HTS_YOTSUBA_API_ORIGIN;
  if (url == NULL || (board_count == 0U && !discover)) {
    print_error(context, "source add requires a URL and board(s) or discovery");
    return 2;
  }
  used = 0U;
  used += (size_t) snprintf(metadata + used, sizeof(metadata) - used,
                            "{\"metadata_only\":true,\"discover_boards\":%s,"
                            "\"boards\":[", discover ? "true" : "false");
  for (index = 0; index < (int) board_count; index++) {
    if (index != 0) metadata[used++] = ',';
    metadata[used++] = '"';
    if (!append_json_text(metadata, sizeof(metadata), &used, boards[index]))
      return 2;
    metadata[used++] = '"';
  }
  if (used + 3U >= sizeof(metadata)) return 2;
  metadata[used++] = ']';
  metadata[used++] = '}';
  metadata[used] = '\0';
  catalog = NULL;
  if (hts_catalog_open(context->catalog_path, &catalog) != HTS_CATALOG_OK) {
    print_error(context, "could not open catalog");
    hts_catalog_close(catalog);
    return 2;
  }
  (void) memset(&source, 0, sizeof(source));
  source.adapter_kind = adapter;
  source.canonical_base_url = url;
  source.display_name = name;
  source.enabled = enabled;
  source.policy_json = "{\"metadata_only\":true}";
  source.credential_ref = credential_ref;
  source.metadata_json = metadata;
  if (hts_catalog_upsert_source(catalog, &source, &source_id) !=
      HTS_CATALOG_OK) {
    print_error(context, hts_catalog_last_error(catalog));
    hts_catalog_close(catalog);
    return 2;
  }
  hts_catalog_close(catalog);
  if (context->json)
    (void) printf("{\"enabled\":%s,\"id\":%lld,\"metadata_only\":true}\n",
                  enabled ? "true" : "false", (long long) source_id);
  else
    (void) printf("Added metadata-only source %lld. Ephemeral originals may disappear.\n",
                  (long long) source_id);
  return 0;
}

static int source_list(cli_context *context, int argc, char **argv) {
  sqlite3_stmt *statement;
  int include_removed;
  int limit;
  int offset;
  int index;
  int first;
  include_removed = 0;
  limit = CLI_LIMIT_DEFAULT;
  offset = 0;
  for (index = 0; index < argc; index++) {
    if (strcmp(argv[index], "--include-removed") == 0)
      include_removed = 1;
    else if (strcmp(argv[index], "--limit") == 0 && index + 1 < argc) {
      if (!parse_int(argv[++index], &limit) || limit > CLI_LIMIT_MAX) return 2;
    } else if (strcmp(argv[index], "--offset") == 0 && index + 1 < argc) {
      if (!parse_int(argv[++index], &offset)) return 2;
    } else return 2;
  }
  if (!open_catalog(context)) return 2;
  if (!prepare_sql(context,
      "SELECT id,adapter_kind,canonical_base_url,COALESCE(display_name,''),"
      "enabled,deleted_at IS NOT NULL,credential_ref IS NOT NULL "
      "FROM sources WHERE (? OR deleted_at IS NULL) ORDER BY id LIMIT ? OFFSET ?",
      &statement)) {
    close_catalog(context);
    return 2;
  }
  (void) sqlite3_bind_int(statement, 1, include_removed);
  (void) sqlite3_bind_int(statement, 2, limit);
  (void) sqlite3_bind_int(statement, 3, offset);
  first = 1;
  if (context->json) (void) fputs("{\"sources\":[", stdout);
  while (sqlite3_step(statement) == SQLITE_ROW) {
    if (context->json) {
      if (!first) (void) fputc(',', stdout);
      (void) fprintf(stdout, "{\"adapter\":");
      json_string(stdout, column_text(statement, 1));
      (void) fputs(",\"canonical_base_url\":", stdout);
      json_string(stdout, column_text(statement, 2));
      (void) fprintf(stdout,
                     ",\"credential_configured\":%s,\"display_name\":",
                     sqlite3_column_int(statement, 6) ? "true" : "false");
      json_string(stdout, column_text(statement, 3));
      (void) fprintf(stdout,
                     ",\"enabled\":%s,\"id\":%lld,\"metadata_only\":true,"
                     "\"removed\":%s}",
                     sqlite3_column_int(statement, 4) ? "true" : "false",
                     (long long) sqlite3_column_int64(statement, 0),
                     sqlite3_column_int(statement, 5) ? "true" : "false");
    } else {
      (void) printf("%lld\t%s\t%s\t%s%s\n",
                    (long long) sqlite3_column_int64(statement, 0),
                    column_text(statement, 1), column_text(statement, 2),
                    sqlite3_column_int(statement, 4) ? "enabled" : "disabled",
                    sqlite3_column_int(statement, 5) ? ",removed" : "");
    }
    first = 0;
  }
  if (context->json)
    (void) fprintf(stdout, "],\"limit\":%d,\"metadata_only\":true,\"offset\":%d}\n",
                   limit, offset);
  (void) sqlite3_finalize(statement);
  close_catalog(context);
  return 0;
}

static int source_inspect(cli_context *context, const char *id_text) {
  sqlite3_stmt *statement;
  int64_t id;
  if (!parse_i64(id_text, &id) || id <= 0 || !open_catalog(context)) return 2;
  if (!prepare_sql(context,
      "SELECT id,adapter_kind,canonical_base_url,COALESCE(display_name,''),"
      "enabled,COALESCE(policy_json,''),COALESCE(metadata_json,''),"
      "deleted_at IS NOT NULL,credential_ref IS NOT NULL FROM sources WHERE id=?",
      &statement)) {
    close_catalog(context);
    return 2;
  }
  (void) sqlite3_bind_int64(statement, 1, id);
  if (sqlite3_step(statement) != SQLITE_ROW) {
    print_error(context, "source not found");
    (void) sqlite3_finalize(statement);
    close_catalog(context);
    return 1;
  }
  if (context->json) {
    (void) fputs("{\"adapter\":", stdout);
    json_string(stdout, column_text(statement, 1));
    (void) fputs(",\"canonical_base_url\":", stdout);
    json_string(stdout, column_text(statement, 2));
    (void) fprintf(stdout, ",\"credential_configured\":%s,\"display_name\":",
                   sqlite3_column_int(statement, 8) ? "true" : "false");
    json_string(stdout, column_text(statement, 3));
    (void) fprintf(stdout, ",\"enabled\":%s,\"id\":%lld,\"metadata\":",
                   sqlite3_column_int(statement, 4) ? "true" : "false",
                   (long long) id);
    (void) fputs(column_text(statement, 6), stdout);
    (void) fputs(",\"metadata_only\":true,\"policy\":", stdout);
    (void) fputs(column_text(statement, 5), stdout);
    (void) fprintf(stdout, ",\"removed\":%s}\n",
                   sqlite3_column_int(statement, 7) ? "true" : "false");
  } else {
    (void) printf("Source %lld\nAdapter: %s\nURL: %s\nName: %s\nState: %s%s\n"
                  "Mode: metadata-only\nCredential configured: %s\n",
                  (long long) id, column_text(statement, 1),
                  column_text(statement, 2), column_text(statement, 3),
                  sqlite3_column_int(statement, 4) ? "enabled" : "disabled",
                  sqlite3_column_int(statement, 7) ? ", removed" : "",
                  sqlite3_column_int(statement, 8) ? "yes" : "no");
    (void) puts("Warning: ephemeral originals may disappear before acquisition.");
  }
  (void) sqlite3_finalize(statement);
  close_catalog(context);
  return 0;
}

static int source_state(cli_context *context, const char *action,
                        const char *id_text, int confirmed) {
  sqlite3_stmt *statement;
  int64_t id;
  const char *sql;
  if (!parse_i64(id_text, &id) || id <= 0) return 2;
  if (strcmp(action, "remove") == 0 && !confirmed) {
    print_error(context, "source remove requires --yes; catalog metadata is soft-removed and files are untouched");
    return 2;
  }
  if (!open_catalog(context)) return 2;
  if (strcmp(action, "enable") == 0)
    sql = "UPDATE sources SET enabled=1,deleted_at=NULL,updated_at=strftime('%Y-%m-%dT%H:%M:%fZ','now') WHERE id=?";
  else if (strcmp(action, "disable") == 0)
    sql = "UPDATE sources SET enabled=0,updated_at=strftime('%Y-%m-%dT%H:%M:%fZ','now') WHERE id=? AND deleted_at IS NULL";
  else
    sql = "UPDATE sources SET enabled=0,deleted_at=strftime('%Y-%m-%dT%H:%M:%fZ','now'),updated_at=strftime('%Y-%m-%dT%H:%M:%fZ','now') WHERE id=? AND deleted_at IS NULL";
  if (!prepare_sql(context, sql, &statement)) {
    close_catalog(context);
    return 2;
  }
  (void) sqlite3_bind_int64(statement, 1, id);
  if (sqlite3_step(statement) != SQLITE_DONE || sqlite3_changes(context->db) == 0) {
    print_error(context, "source not found or already in requested state");
    (void) sqlite3_finalize(statement);
    close_catalog(context);
    return 1;
  }
  (void) sqlite3_finalize(statement);
  if (context->json)
    (void) printf("{\"action\":\"%s\",\"id\":%lld,\"downloaded_files_deleted\":false}\n",
                  action, (long long) id);
  else
    (void) printf("Source %lld %sd; downloaded files were not touched.\n",
                  (long long) id, action);
  close_catalog(context);
  return 0;
}

static size_t curl_write_body(char *data, size_t size, size_t count,
                              void *opaque) {
  curl_transport_context *context;
  size_t bytes;
  size_t needed;
  unsigned char *next;
  context = (curl_transport_context *) opaque;
  if (count != 0U && size > (size_t) -1 / count) return 0U;
  bytes = size * count;
  if (context->body.size > context->body.maximum - bytes) {
    context->body.overflow = 1;
    return 0U;
  }
  needed = context->body.size + bytes;
  if (needed > context->body.capacity) {
    size_t capacity;
    capacity = context->body.capacity == 0U ? 65536U : context->body.capacity;
    while (capacity < needed) {
      if (capacity > context->body.maximum / 2U) {
        capacity = context->body.maximum;
        break;
      }
      capacity *= 2U;
    }
    next = (unsigned char *) realloc(context->body.data, capacity);
    if (next == NULL) return 0U;
    context->body.data = next;
    context->body.capacity = capacity;
  }
  (void) memcpy(context->body.data + context->body.size, data, bytes);
  context->body.size += bytes;
  return bytes;
}

static void copy_header_value(char *output, size_t output_size,
                              const char *input, size_t length) {
  while (length != 0U && (*input == ' ' || *input == '\t')) {
    input++;
    length--;
  }
  while (length != 0U &&
         (input[length - 1U] == '\r' || input[length - 1U] == '\n' ||
          input[length - 1U] == ' ' || input[length - 1U] == '\t')) length--;
  if (length >= output_size) length = output_size - 1U;
  (void) memcpy(output, input, length);
  output[length] = '\0';
}

static int ascii_prefix(const char *value, size_t length, const char *prefix) {
  size_t prefix_length;
  size_t i;
  prefix_length = strlen(prefix);
  if (length < prefix_length) return 0;
  for (i = 0U; i < prefix_length; i++) {
    unsigned char left;
    unsigned char right;
    left = (unsigned char) value[i];
    right = (unsigned char) prefix[i];
    if (left >= 'A' && left <= 'Z') left = (unsigned char) (left + 32U);
    if (right >= 'A' && right <= 'Z') right = (unsigned char) (right + 32U);
    if (left != right) return 0;
  }
  return 1;
}

static size_t curl_read_header(char *data, size_t size, size_t count,
                               void *opaque) {
  curl_transport_context *context;
  size_t bytes;
  context = (curl_transport_context *) opaque;
  if (count != 0U && size > (size_t) -1 / count) return 0U;
  bytes = size * count;
  if (ascii_prefix(data, bytes, "etag:"))
    copy_header_value(context->etag, sizeof(context->etag), data + 5,
                      bytes - 5U);
  else if (ascii_prefix(data, bytes, "last-modified:"))
    copy_header_value(context->last_modified,
                      sizeof(context->last_modified), data + 14,
                      bytes - 14U);
  else if (ascii_prefix(data, bytes, "retry-after:")) {
    char value[64];
    int64_t seconds;
    copy_header_value(value, sizeof(value), data + 12, bytes - 12U);
    if (parse_i64(value, &seconds) && seconds >= 0 && seconds <= 86400)
      context->retry_after_ms = (uint64_t) seconds * 1000U;
  }
  return bytes;
}

static int curl_progress(void *opaque, curl_off_t download_total,
                         curl_off_t download_now, curl_off_t upload_total,
                         curl_off_t upload_now) {
  curl_transport_context *context;
  (void) download_total;
  (void) download_now;
  (void) upload_total;
  (void) upload_now;
  context = (curl_transport_context *) opaque;
  return cli_cancelled ||
         (context->request != NULL &&
          context->request->cancel.is_cancelled != NULL &&
          context->request->cancel.is_cancelled(
              context->request->cancel.context));
}

static hts_metadata_transport_status curl_perform(
    void *opaque, const hts_metadata_http_request *request,
    hts_metadata_http_response *response) {
  curl_transport_context *context;
  CURL *curl;
  CURLcode status;
  struct curl_slist *headers;
  char conditional_etag[4200];
  char conditional_modified[4200];
  long response_code;
  context = (curl_transport_context *) opaque;
  context->body.size = 0U;
  context->body.maximum = request->maximum_body_bytes;
  context->body.overflow = 0;
  context->etag[0] = '\0';
  context->last_modified[0] = '\0';
  context->retry_after_ms = 0U;
  context->request = request;
  curl = curl_easy_init();
  if (curl == NULL) return HTS_METADATA_TRANSPORT_PERMANENT_ERROR;
  headers = NULL;
  if (request->if_none_match != NULL) {
    (void) snprintf(conditional_etag, sizeof(conditional_etag),
                    "If-None-Match: %s", request->if_none_match);
    headers = curl_slist_append(headers, conditional_etag);
  }
  if (request->if_modified_since != NULL) {
    (void) snprintf(conditional_modified, sizeof(conditional_modified),
                    "If-Modified-Since: %s", request->if_modified_since);
    headers = curl_slist_append(headers, conditional_modified);
  }
  headers = curl_slist_append(headers, "Accept: application/json");
  (void) curl_easy_setopt(curl, CURLOPT_URL, request->url);
  (void) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  (void) curl_easy_setopt(curl, CURLOPT_USERAGENT, request->user_agent);
  (void) curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  (void) curl_easy_setopt(curl, CURLOPT_MAXREDIRS, (long) request->redirect_limit);
  (void) curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
  (void) curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "https");
  (void) curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS,
                          (long) request->connect_timeout_ms);
  (void) curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS,
                          (long) request->request_timeout_ms);
  (void) curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_body);
  (void) curl_easy_setopt(curl, CURLOPT_WRITEDATA, context);
  (void) curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curl_read_header);
  (void) curl_easy_setopt(curl, CURLOPT_HEADERDATA, context);
  (void) curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curl_progress);
  (void) curl_easy_setopt(curl, CURLOPT_XFERINFODATA, context);
  (void) curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
  (void) curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  status = curl_easy_perform(curl);
  response_code = 0L;
  (void) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  context->request = NULL;
  if (status != CURLE_OK) {
    if (status == CURLE_ABORTED_BY_CALLBACK || cli_cancelled)
      return HTS_METADATA_TRANSPORT_TEMPORARY_ERROR;
    if (context->body.overflow || status == CURLE_WRITE_ERROR)
      return HTS_METADATA_TRANSPORT_PERMANENT_ERROR;
    if (status == CURLE_OPERATION_TIMEDOUT || status == CURLE_COULDNT_CONNECT ||
        status == CURLE_COULDNT_RESOLVE_HOST || status == CURLE_RECV_ERROR ||
        status == CURLE_SEND_ERROR)
      return HTS_METADATA_TRANSPORT_TEMPORARY_ERROR;
    return HTS_METADATA_TRANSPORT_PERMANENT_ERROR;
  }
  response->status_code = (int) response_code;
  response->body = context->body.data;
  response->body_size = context->body.size;
  response->etag = context->etag[0] != '\0' ? context->etag : NULL;
  response->last_modified = context->last_modified[0] != '\0'
                              ? context->last_modified : NULL;
  response->retry_after_ms = context->retry_after_ms;
  return HTS_METADATA_TRANSPORT_OK;
}

static int read_file(const char *path, unsigned char **buffer,
                     size_t *capacity, size_t *size, size_t maximum) {
  FILE *file;
  long length;
  unsigned char *next;
  file = fopen(path, "rb");
  if (file == NULL) return 0;
  if (fseek(file, 0L, SEEK_END) != 0 || (length = ftell(file)) < 0 ||
      (uint64_t) length > (uint64_t) maximum ||
      fseek(file, 0L, SEEK_SET) != 0) {
    (void) fclose(file);
    return 0;
  }
  if ((size_t) length > *capacity) {
    next = (unsigned char *) realloc(*buffer, (size_t) length);
    if (next == NULL) {
      (void) fclose(file);
      return 0;
    }
    *buffer = next;
    *capacity = (size_t) length;
  }
  *size = fread(*buffer, 1U, (size_t) length, file);
  if (*size != (size_t) length || ferror(file)) {
    (void) fclose(file);
    return 0;
  }
  (void) fclose(file);
  return 1;
}

static const char *fixture_name_for_url(const char *url, char *storage,
                                        size_t storage_size) {
  const char *path;
  const char *thread;
  const char *slash;
  int written;
  path = strstr(url, "a.4cdn.org/");
  if (path == NULL) return NULL;
  path += 11;
  if (strcmp(path, "boards.json") == 0) return "boards.json";
  if ((thread = strstr(path, "/thread/")) != NULL) {
    thread += 8;
    slash = strstr(thread, ".json");
    if (slash == NULL || slash[5] != '\0') return NULL;
    written = snprintf(storage, storage_size, "thread-%.*s.json",
                       (int) (slash - thread), thread);
    return written >= 0 && (size_t) written < storage_size ? storage : NULL;
  }
  if (strstr(path, "/threads.json") != NULL) return "threads-initial.json";
  if (strstr(path, "/archive.json") != NULL) return "archive-empty.json";
  return NULL;
}

static hts_metadata_transport_status fixture_perform(
    void *opaque, const hts_metadata_http_request *request,
    hts_metadata_http_response *response) {
  fixture_transport_context *context;
  char name[128];
  char path[2048];
  const char *fixture;
  size_t size;
  int written;
  context = (fixture_transport_context *) opaque;
  fixture = fixture_name_for_url(request->url, name, sizeof(name));
  if (fixture == NULL) return HTS_METADATA_TRANSPORT_PERMANENT_ERROR;
  written = snprintf(path, sizeof(path), "%s/%s", context->root, fixture);
  if (written < 0 || (size_t) written >= sizeof(path))
    return HTS_METADATA_TRANSPORT_PERMANENT_ERROR;
  if (!read_file(path, &context->body, &context->capacity, &size,
                 request->maximum_body_bytes)) {
    response->status_code = 404;
    return HTS_METADATA_TRANSPORT_OK;
  }
  response->status_code = 200;
  response->body = context->body;
  response->body_size = size;
  return HTS_METADATA_TRANSPORT_OK;
}

static uint64_t monotonic_ms(void *opaque) {
  struct timespec now;
  real_clock_context *context;
  uint64_t value;
  context = (real_clock_context *) opaque;
#ifdef _WIN32
  value = (uint64_t) GetTickCount64();
#else
  if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
    value = (uint64_t) time(NULL) * 1000U;
  else
    value = (uint64_t) now.tv_sec * 1000U + (uint64_t) now.tv_nsec / 1000000U;
#endif
  if (context->origin_ms == 0U) context->origin_ms = value;
  return value;
}

static int real_sleep(void *opaque, uint64_t delay_ms) {
#ifndef _WIN32
  struct timespec requested;
  struct timespec remaining;
#endif
  (void) opaque;
#ifdef _WIN32
  Sleep((DWORD) delay_ms);
#else
  requested.tv_sec = (time_t) (delay_ms / 1000U);
  requested.tv_nsec = (long) ((delay_ms % 1000U) * 1000000U);
  while (nanosleep(&requested, &remaining) != 0) {
    if (errno != EINTR || cli_cancelled) return -1;
    requested = remaining;
  }
#endif
  return cli_cancelled ? -1 : 0;
}

static uint64_t virtual_now(void *opaque) {
  return ((virtual_clock_context *) opaque)->now_ms;
}

static int virtual_sleep(void *opaque, uint64_t delay_ms) {
  ((virtual_clock_context *) opaque)->now_ms += delay_ms;
  return cli_cancelled ? -1 : 0;
}

static int sync_cancelled(void *opaque) {
  (void) opaque;
  return cli_cancelled != 0;
}

static void sync_report(void *opaque, const hts_metadata_event *event) {
  sync_report_context *context;
  context = (sync_report_context *) opaque;
  if (event->type == HTS_METADATA_EVENT_RETRY ||
      event->type == HTS_METADATA_EVENT_ERROR) {
    (void) fprintf(stderr, "sync: %s (category=%d, attempt=%u)\n",
                   event->message != NULL ? event->message : "event",
                   (int) event->category, event->attempt);
  } else if (!context->json && event->type == HTS_METADATA_EVENT_PAGE_COMMITTED) {
    (void) fprintf(stderr, "sync: committed %lu normalized records\n",
                   (unsigned long) event->normalized_record_count);
  }
}

static int load_source(cli_context *context, int64_t id,
                       source_config *source) {
  sqlite3_stmt *statement;
  if (!prepare_sql(context,
      "SELECT id,adapter_kind,canonical_base_url,COALESCE(display_name,''),"
      "enabled,COALESCE(policy_json,''),COALESCE(metadata_json,'') "
      "FROM sources WHERE id=? AND deleted_at IS NULL", &statement)) return 0;
  (void) sqlite3_bind_int64(statement, 1, id);
  if (sqlite3_step(statement) != SQLITE_ROW) {
    (void) sqlite3_finalize(statement);
    return 0;
  }
  (void) memset(source, 0, sizeof(*source));
  source->id = sqlite3_column_int64(statement, 0);
  (void) snprintf(source->adapter, sizeof(source->adapter), "%s",
                  column_text(statement, 1));
  (void) snprintf(source->base_url, sizeof(source->base_url), "%s",
                  column_text(statement, 2));
  (void) snprintf(source->display_name, sizeof(source->display_name), "%s",
                  column_text(statement, 3));
  source->enabled = sqlite3_column_int(statement, 4);
  (void) snprintf(source->policy_json, sizeof(source->policy_json), "%s",
                  column_text(statement, 5));
  (void) snprintf(source->metadata_json, sizeof(source->metadata_json), "%s",
                  column_text(statement, 6));
  (void) sqlite3_finalize(statement);
  return 1;
}

static int parse_source_boards(const char *json, char ***output,
                               size_t *output_count, int *discover) {
  const char *scan;
  const char *end;
  char **boards;
  size_t count;
  size_t capacity;
  *output = NULL;
  *output_count = 0U;
  *discover = strstr(json, "\"discover_boards\":true") != NULL;
  scan = strstr(json, "\"boards\":[");
  if (scan == NULL) return 0;
  scan += 10;
  boards = NULL;
  count = 0U;
  capacity = 0U;
  while (*scan != '\0' && *scan != ']') {
    size_t length;
    char *board;
    char **next;
    while (*scan == ' ' || *scan == '\t' || *scan == '\r' ||
           *scan == '\n' || *scan == ',') scan++;
    if (*scan == ']') break;
    if (*scan != '"') goto failure;
    scan++;
    end = strchr(scan, '"');
    if (end == NULL) goto failure;
    length = (size_t) (end - scan);
    board = (char *) malloc(length + 1U);
    if (board == NULL) goto failure;
    (void) memcpy(board, scan, length);
    board[length] = '\0';
    if (!board_valid(board)) {
      free(board);
      goto failure;
    }
    if (count == capacity) {
      capacity = capacity == 0U ? 8U : capacity * 2U;
      next = (char **) realloc(boards, capacity * sizeof(char *));
      if (next == NULL) {
        free(board);
        goto failure;
      }
      boards = next;
    }
    boards[count++] = board;
    scan = end + 1;
  }
  *output = boards;
  *output_count = count;
  return 1;
failure:
  while (count != 0U) free(boards[--count]);
  free(boards);
  return 0;
}

static void free_boards(char **boards, size_t count) {
  while (count != 0U) free(boards[--count]);
  free(boards);
}

static int sync_status(cli_context *context, int64_t source_id) {
  sqlite3_stmt *statement;
  if (!open_catalog(context)) return 2;
  if (!prepare_sql(context,
      "SELECT COUNT(*),SUM(CASE WHEN last_success_ms IS NOT NULL THEN 1 ELSE 0 END),"
      "SUM(CASE WHEN error_state IS NOT NULL THEN 1 ELSE 0 END),"
      "MAX(last_attempt_ms),MAX(last_success_ms) FROM sync_cursors WHERE source_id=?",
      &statement)) {
    close_catalog(context);
    return 2;
  }
  (void) sqlite3_bind_int64(statement, 1, source_id);
  if (sqlite3_step(statement) != SQLITE_ROW) {
    (void) sqlite3_finalize(statement);
    close_catalog(context);
    return 2;
  }
  if (context->json)
    (void) printf("{\"cursor_count\":%lld,\"error_count\":%lld,"
                  "\"last_attempt_ms\":%lld,\"last_success_ms\":%lld,"
                  "\"source_id\":%lld,\"successful_cursor_count\":%lld}\n",
                  (long long) sqlite3_column_int64(statement, 0),
                  (long long) sqlite3_column_int64(statement, 2),
                  (long long) sqlite3_column_int64(statement, 3),
                  (long long) sqlite3_column_int64(statement, 4),
                  (long long) source_id,
                  (long long) sqlite3_column_int64(statement, 1));
  else
    (void) printf("Source %lld: %lld cursors, %lld successful, %lld errors\n",
                  (long long) source_id,
                  (long long) sqlite3_column_int64(statement, 0),
                  (long long) sqlite3_column_int64(statement, 1),
                  (long long) sqlite3_column_int64(statement, 2));
  (void) sqlite3_finalize(statement);
  close_catalog(context);
  return 0;
}

static int sync_one_source(cli_context *context, const source_config *source,
                           const char *board_override,
                           const char *fixture_root, int dry_run,
                           int emit_dry_run,
                           hts_yotsuba_sync_result *sync_result) {
  hts_catalog *catalog;
  hts_yotsuba_options options;
  hts_yotsuba_adapter *adapter;
  hts_metadata_transport transport;
  hts_metadata_clock clock;
  curl_transport_context curl_context;
  fixture_transport_context fixture_context;
  real_clock_context real_context;
  virtual_clock_context virtual_context;
  sync_report_context report_context;
  char **boards;
  size_t board_count;
  int discover;
  const char *override_boards[1];
  hts_metadata_category category;
  size_t index;
  if (strcmp(source->adapter, "yotsuba") != 0) {
    print_error(context, "source adapter is not supported by this build");
    return 2;
  }
  if (!parse_source_boards(source->metadata_json, &boards, &board_count,
                           &discover)) {
    print_error(context, "source board configuration is malformed");
    return 2;
  }
  if (board_override != NULL) {
    if (!board_valid(board_override)) {
      free_boards(boards, board_count);
      print_error(context, "invalid board scope");
      return 2;
    }
    override_boards[0] = board_override;
  }
  if (dry_run) {
    if (!emit_dry_run) {
      free_boards(boards, board_count);
      return 0;
    }
    if (context->json) {
      (void) fprintf(stdout,
          "{\"adapter\":\"yotsuba\",\"board_count\":%lu,"
          "\"discover_boards\":%s,\"dry_run\":true,\"metadata_only\":true,"
          "\"source_id\":%lld}\n",
          (unsigned long) (board_override != NULL ? 1U : board_count),
          board_override == NULL && discover ? "true" : "false",
          (long long) source->id);
    } else {
      (void) printf("Would sync metadata-only source %lld (%s): ",
                    (long long) source->id, source->adapter);
      if (board_override != NULL)
        (void) printf("board /%s/\n", board_override);
      else if (discover)
        (void) puts("discover boards, then sync all accessible full threads");
      else {
        for (index = 0U; index < board_count; index++)
          (void) printf("/%s/%s", boards[index],
                        index + 1U == board_count ? "\n" : ", ");
      }
      (void) puts("No media bodies would be requested.");
    }
    free_boards(boards, board_count);
    return 0;
  }
  catalog = NULL;
  if (hts_catalog_open(context->catalog_path, &catalog) != HTS_CATALOG_OK) {
    free_boards(boards, board_count);
    print_error(context, "could not open catalog for synchronization");
    hts_catalog_close(catalog);
    return 2;
  }
  (void) memset(&options, 0, sizeof(options));
  options.source_id = source->id;
  options.boards = board_override != NULL ? override_boards
                                           : (const char *const *) boards;
  options.board_count = board_override != NULL ? 1U : board_count;
  options.discover_boards = board_override == NULL ? discover : 0;
  options.missing_confirmations = 2U;
  options.cancel.is_cancelled = sync_cancelled;
  hts_yotsuba_default_policy(&options.policy);
  adapter = NULL;
  if (!hts_yotsuba_adapter_create(catalog, &options, &adapter)) {
    free_boards(boards, board_count);
    hts_catalog_close(catalog);
    print_error(context, "source policy or board configuration is invalid");
    return 2;
  }
  (void) memset(&transport, 0, sizeof(transport));
  (void) memset(&clock, 0, sizeof(clock));
  (void) memset(&curl_context, 0, sizeof(curl_context));
  (void) memset(&fixture_context, 0, sizeof(fixture_context));
  (void) memset(&real_context, 0, sizeof(real_context));
  (void) memset(&virtual_context, 0, sizeof(virtual_context));
  if (fixture_root != NULL) {
    fixture_context.root = fixture_root;
    transport.context = &fixture_context;
    transport.perform = fixture_perform;
    virtual_context.now_ms = 100000U;
    clock.context = &virtual_context;
    clock.now_ms = virtual_now;
    clock.sleep_ms = virtual_sleep;
  } else {
    transport.context = &curl_context;
    transport.perform = curl_perform;
    clock.context = &real_context;
    clock.now_ms = monotonic_ms;
    clock.sleep_ms = real_sleep;
  }
  report_context.json = context->json;
  category = hts_yotsuba_sync(adapter, &transport, &clock, sync_report,
                              &report_context, sync_result);
  free(curl_context.body.data);
  free(fixture_context.body);
  hts_yotsuba_adapter_destroy(adapter);
  hts_catalog_close(catalog);
  free_boards(boards, board_count);
  if (category != HTS_METADATA_SUCCESS) {
    if (category == HTS_METADATA_CANCELLED)
      print_error(context, "synchronization cancelled");
    else
      (void) fprintf(stderr, "httrack-catalog: synchronization failed (category=%d)\n",
                     (int) category);
    return category == HTS_METADATA_CANCELLED ? 130 : 2;
  }
  return 0;
}

static int command_sync(cli_context *context, int argc, char **argv) {
  int64_t source_id;
  int all;
  int dry_run;
  int status;
  const char *board;
  const char *fixture_root;
  int index;
  source_config source;
  hts_yotsuba_sync_result result;
  sqlite3_stmt *statement;
  int return_code;
  source_id = 0;
  all = 0;
  dry_run = 0;
  status = 0;
  board = NULL;
  fixture_root = NULL;
  for (index = 0; index < argc; index++) {
    if (strcmp(argv[index], "--source") == 0 && index + 1 < argc) {
      if (!parse_i64(argv[++index], &source_id) || source_id <= 0) return 2;
    } else if (strcmp(argv[index], "--board") == 0 && index + 1 < argc)
      board = argv[++index];
    else if (strcmp(argv[index], "--all") == 0)
      all = 1;
    else if (strcmp(argv[index], "--dry-run") == 0)
      dry_run = 1;
    else if (strcmp(argv[index], "--status") == 0)
      status = 1;
    else if (strcmp(argv[index], "--fixture-dir") == 0 && index + 1 < argc)
      fixture_root = argv[++index];
    else {
      print_error(context, "invalid sync option");
      return 2;
    }
  }
  if (status) {
    if (source_id <= 0 || all || board != NULL || dry_run) {
      print_error(context, "sync --status requires exactly one --source");
      return 2;
    }
    return sync_status(context, source_id);
  }
  if ((source_id > 0) == all || (board != NULL && source_id <= 0)) {
    print_error(context, "sync requires exactly one of --source or --all");
    return 2;
  }
  cli_cancelled = 0;
  (void) signal(SIGINT, handle_signal);
#ifdef SIGTERM
  (void) signal(SIGTERM, handle_signal);
#endif
  if (source_id > 0) {
    if (!open_catalog(context)) return 2;
    if (!load_source(context, source_id, &source)) {
      close_catalog(context);
      print_error(context, "source not found");
      return 1;
    }
    close_catalog(context);
    (void) memset(&result, 0, sizeof(result));
    return_code = sync_one_source(context, &source, board, fixture_root,
                                  dry_run, 1, &result);
    if (return_code == 0 && !dry_run) {
      if (context->json)
        (void) printf("{\"boards_processed\":%lu,\"media_bodies_requested\":0,"
                      "\"media_variants_committed\":%lu,\"metadata_only\":true,"
                      "\"posts_committed\":%lu,\"requests_made\":%u,"
                      "\"source_id\":%lld,\"threads_discovered\":%lu,"
                      "\"threads_fetched\":%lu,\"threads_not_modified\":%lu}\n",
                      (unsigned long) result.boards_processed,
                      (unsigned long) result.media_variants_committed,
                      (unsigned long) result.posts_committed,
                      result.requests_made, (long long) source_id,
                      (unsigned long) result.threads_discovered,
                      (unsigned long) result.threads_fetched,
                      (unsigned long) result.threads_not_modified);
      else
        (void) printf("Synced source %lld: %lu boards, %lu threads fetched, "
                      "%lu posts, 0 media bodies.\n",
                      (long long) source_id,
                      (unsigned long) result.boards_processed,
                      (unsigned long) result.threads_fetched,
                      (unsigned long) result.posts_committed);
    }
    return return_code;
  }
  {
    int64_t *source_ids;
    size_t source_count;
    size_t source_capacity;
    size_t source_index;
    source_ids = NULL;
    source_count = 0U;
    source_capacity = 0U;
    if (!open_catalog(context)) return 2;
    if (!prepare_sql(context,
        "SELECT id FROM sources WHERE enabled=1 AND deleted_at IS NULL ORDER BY id",
        &statement)) {
      close_catalog(context);
      return 2;
    }
    while (sqlite3_step(statement) == SQLITE_ROW) {
      if (source_count == source_capacity) {
        int64_t *next;
        source_capacity = source_capacity == 0U ? 8U : source_capacity * 2U;
        next = (int64_t *) realloc(source_ids,
                                   source_capacity * sizeof(int64_t));
        if (next == NULL) {
          free(source_ids); sqlite3_finalize(statement); close_catalog(context);
          return 2;
        }
        source_ids = next;
      }
      source_ids[source_count++] = sqlite3_column_int64(statement, 0);
    }
    sqlite3_finalize(statement);
    close_catalog(context);
    return_code = 0;
    if (context->json) {
      if (dry_run)
        (void) fputs("{\"dry_run\":true,\"metadata_only\":true,"
                     "\"source_ids\":[", stdout);
      else
        (void) fputs("{\"results\":[", stdout);
    }
    index = 0;
    for (source_index = 0U; source_index < source_count; source_index++) {
      if (!open_catalog(context) ||
          !load_source(context, source_ids[source_index], &source)) {
        close_catalog(context); return_code = 2; break;
      }
      close_catalog(context);
      (void) memset(&result, 0, sizeof(result));
      return_code = sync_one_source(context, &source, NULL, fixture_root,
                                    dry_run, !(context->json && dry_run),
                                    &result);
      if (context->json && dry_run) {
        if (index++ != 0) (void) fputc(',', stdout);
        (void) fprintf(stdout, "%lld", (long long) source_ids[source_index]);
      } else if (context->json) {
        if (index++ != 0) (void) fputc(',', stdout);
        (void) printf("{\"media_bodies_requested\":0,\"source_id\":%lld,"
                      "\"threads_fetched\":%lu}",
                      (long long) source_ids[source_index],
                      (unsigned long) result.threads_fetched);
      }
      if (return_code != 0 || cli_cancelled) break;
    }
    free(source_ids);
  }
  if (context->json) {
    if (dry_run)
      (void) fputs("]}\n", stdout);
    else
      (void) fputs("],\"metadata_only\":true}\n", stdout);
  }
  return return_code;
}

static void filter_defaults(query_filter *filter) {
  (void) memset(filter, 0, sizeof(*filter));
  filter->minimum_width = -1;
  filter->maximum_width = -1;
  filter->minimum_height = -1;
  filter->maximum_height = -1;
  filter->minimum_bytes = -1;
  filter->maximum_bytes = -1;
  filter->limit = CLI_LIMIT_DEFAULT;
}

static int parse_filters(cli_context *context, int argc, char **argv,
                         query_filter *filter) {
  int index;
  filter_defaults(filter);
  for (index = 0; index < argc; index++) {
    const char *option;
    const char *value;
    option = argv[index];
    value = index + 1 < argc ? argv[index + 1] : NULL;
    if (strcmp(option, "--source") == 0 && value != NULL) {
      if (!parse_i64(value, &filter->source_id) || filter->source_id <= 0)
        goto invalid;
      index++;
    } else if (strcmp(option, "--board") == 0 && value != NULL) {
      filter->board = value; index++;
    } else if (strcmp(option, "--kind") == 0 && value != NULL) {
      if (strcmp(value, "thread") != 0 && strcmp(value, "pool") != 0 &&
          strcmp(value, "query") != 0) goto invalid;
      filter->kind = value; index++;
    } else if (strcmp(option, "--remote-id") == 0 && value != NULL) {
      filter->remote_id = value; index++;
    } else if (strcmp(option, "--query") == 0 && value != NULL) {
      filter->query = value; index++;
    } else if (strcmp(option, "--tag") == 0 && value != NULL) {
      filter->tag = value; index++;
    } else if (strcmp(option, "--rating") == 0 && value != NULL) {
      filter->rating = value; index++;
    } else if (strcmp(option, "--created-from") == 0 && value != NULL) {
      filter->created_from = value; index++;
    } else if (strcmp(option, "--created-to") == 0 && value != NULL) {
      filter->created_to = value; index++;
    } else if (strcmp(option, "--updated-from") == 0 && value != NULL) {
      filter->updated_from = value; index++;
    } else if (strcmp(option, "--updated-to") == 0 && value != NULL) {
      filter->updated_to = value; index++;
    } else if (strcmp(option, "--media-type") == 0 && value != NULL) {
      filter->media_type = value; index++;
    } else if (strcmp(option, "--min-width") == 0 && value != NULL) {
      if (!parse_int(value, &filter->minimum_width)) goto invalid;
      index++;
    } else if (strcmp(option, "--max-width") == 0 && value != NULL) {
      if (!parse_int(value, &filter->maximum_width)) goto invalid;
      index++;
    } else if (strcmp(option, "--min-height") == 0 && value != NULL) {
      if (!parse_int(value, &filter->minimum_height)) goto invalid;
      index++;
    } else if (strcmp(option, "--max-height") == 0 && value != NULL) {
      if (!parse_int(value, &filter->maximum_height)) goto invalid;
      index++;
    } else if (strcmp(option, "--min-bytes") == 0 && value != NULL) {
      if (!parse_i64(value, &filter->minimum_bytes) || filter->minimum_bytes < 0)
        goto invalid;
      index++;
    } else if (strcmp(option, "--max-bytes") == 0 && value != NULL) {
      if (!parse_i64(value, &filter->maximum_bytes) || filter->maximum_bytes < 0)
        goto invalid;
      index++;
    } else if (strcmp(option, "--lifecycle") == 0 && value != NULL) {
      filter->lifecycle = value; index++;
    } else if (strcmp(option, "--availability") == 0 && value != NULL) {
      filter->availability = value; index++;
    } else if (strcmp(option, "--limit") == 0 && value != NULL) {
      if (!parse_int(value, &filter->limit) || filter->limit < 1 ||
          filter->limit > CLI_LIMIT_MAX) goto invalid;
      index++;
    } else if (strcmp(option, "--offset") == 0 && value != NULL) {
      if (!parse_int(value, &filter->offset)) goto invalid;
      index++;
    } else goto invalid;
  }
  if (filter->maximum_width >= 0 && filter->minimum_width >= 0 &&
      filter->maximum_width < filter->minimum_width) goto invalid;
  if (filter->maximum_height >= 0 && filter->minimum_height >= 0 &&
      filter->maximum_height < filter->minimum_height) goto invalid;
  if (filter->maximum_bytes >= 0 && filter->minimum_bytes >= 0 &&
      filter->maximum_bytes < filter->minimum_bytes) goto invalid;
  return 1;
invalid:
  print_error(context, "invalid search filter");
  return 0;
}

static int sql_add(char *sql, size_t capacity, const char *text) {
  size_t used;
  size_t length;
  used = strlen(sql);
  length = strlen(text);
  if (used + length + 1U > capacity) return 0;
  (void) memcpy(sql + used, text, length + 1U);
  return 1;
}

static int add_common_filters(char *sql, size_t capacity,
                              const query_filter *filter, int posts) {
  if (filter->source_id > 0 && !sql_add(sql, capacity, " AND s.id=?")) return 0;
  if (filter->board != NULL && !sql_add(sql, capacity, " AND b.remote_id=?")) return 0;
  if (filter->kind != NULL && !sql_add(sql, capacity, " AND c.kind=?")) return 0;
  if (filter->remote_id != NULL &&
      !sql_add(sql, capacity, posts ? " AND p.remote_id=?" : " AND c.remote_id=?"))
    return 0;
  if (filter->query != NULL &&
      !sql_add(sql, capacity, posts
        ? " AND (p.subject LIKE '%'||?||'%' OR p.body_text LIKE '%'||?||'%')"
        : " AND (c.title LIKE '%'||?||'%' OR c.description LIKE '%'||?||'%' OR EXISTS (SELECT 1 FROM posts qp WHERE qp.collection_id=c.id AND (qp.subject LIKE '%'||?||'%' OR qp.body_text LIKE '%'||?||'%')))"))
    return 0;
  if (filter->tag != NULL &&
      !sql_add(sql, capacity, posts
        ? " AND (p.metadata_json LIKE '%'||?||'%' OR c.metadata_json LIKE '%'||?||'%')"
        : " AND (c.metadata_json LIKE '%'||?||'%' OR EXISTS (SELECT 1 FROM posts tp WHERE tp.collection_id=c.id AND tp.metadata_json LIKE '%'||?||'%'))"))
    return 0;
  if (filter->rating != NULL &&
      !sql_add(sql, capacity, posts
        ? " AND p.rating=?"
        : " AND EXISTS (SELECT 1 FROM posts rp WHERE rp.collection_id=c.id AND rp.rating=?)"))
    return 0;
  if (filter->created_from != NULL &&
      !sql_add(sql, capacity, posts ? " AND p.created_at>=?" : " AND c.created_at>=?")) return 0;
  if (filter->created_to != NULL &&
      !sql_add(sql, capacity, posts ? " AND p.created_at<=?" : " AND c.created_at<=?")) return 0;
  if (filter->updated_from != NULL &&
      !sql_add(sql, capacity, posts ? " AND p.updated_at>=?" : " AND c.updated_at>=?")) return 0;
  if (filter->updated_to != NULL &&
      !sql_add(sql, capacity, posts ? " AND p.updated_at<=?" : " AND c.updated_at<=?")) return 0;
  if (filter->lifecycle != NULL &&
      !sql_add(sql, capacity, " AND c.lifecycle_state=?")) return 0;
  if (filter->media_type != NULL &&
      !sql_add(sql, capacity, posts
        ? " AND EXISTS (SELECT 1 FROM media mm WHERE mm.post_id=p.id AND mm.mime_type LIKE ?)"
        : " AND EXISTS (SELECT 1 FROM posts mp JOIN media mm ON mm.post_id=mp.id WHERE mp.collection_id=c.id AND mm.mime_type LIKE ?)")) return 0;
  if (filter->minimum_width >= 0 &&
      !sql_add(sql, capacity, posts
        ? " AND EXISTS (SELECT 1 FROM media mw WHERE mw.post_id=p.id AND mw.width>=?)"
        : " AND EXISTS (SELECT 1 FROM posts wp JOIN media mw ON mw.post_id=wp.id WHERE wp.collection_id=c.id AND mw.width>=?)")) return 0;
  if (filter->maximum_width >= 0 &&
      !sql_add(sql, capacity, posts
        ? " AND EXISTS (SELECT 1 FROM media mw WHERE mw.post_id=p.id AND mw.width<=?)"
        : " AND EXISTS (SELECT 1 FROM posts wp JOIN media mw ON mw.post_id=wp.id WHERE wp.collection_id=c.id AND mw.width<=?)")) return 0;
  if (filter->minimum_height >= 0 &&
      !sql_add(sql, capacity, posts
        ? " AND EXISTS (SELECT 1 FROM media mh WHERE mh.post_id=p.id AND mh.height>=?)"
        : " AND EXISTS (SELECT 1 FROM posts hp JOIN media mh ON mh.post_id=hp.id WHERE hp.collection_id=c.id AND mh.height>=?)")) return 0;
  if (filter->maximum_height >= 0 &&
      !sql_add(sql, capacity, posts
        ? " AND EXISTS (SELECT 1 FROM media mh WHERE mh.post_id=p.id AND mh.height<=?)"
        : " AND EXISTS (SELECT 1 FROM posts hp JOIN media mh ON mh.post_id=hp.id WHERE hp.collection_id=c.id AND mh.height<=?)")) return 0;
  if (filter->minimum_bytes >= 0 &&
      !sql_add(sql, capacity, posts
        ? " AND EXISTS (SELECT 1 FROM media mb WHERE mb.post_id=p.id AND mb.size_bytes>=?)"
        : " AND EXISTS (SELECT 1 FROM posts bp JOIN media mb ON mb.post_id=bp.id WHERE bp.collection_id=c.id AND mb.size_bytes>=?)")) return 0;
  if (filter->maximum_bytes >= 0 &&
      !sql_add(sql, capacity, posts
        ? " AND EXISTS (SELECT 1 FROM media mb WHERE mb.post_id=p.id AND mb.size_bytes<=?)"
        : " AND EXISTS (SELECT 1 FROM posts bp JOIN media mb ON mb.post_id=bp.id WHERE bp.collection_id=c.id AND mb.size_bytes<=?)")) return 0;
  if (filter->availability != NULL &&
      !sql_add(sql, capacity, posts
        ? " AND EXISTS (SELECT 1 FROM media ma WHERE ma.post_id=p.id AND ma.availability_state=?)"
        : " AND EXISTS (SELECT 1 FROM posts ap JOIN media ma ON ma.post_id=ap.id WHERE ap.collection_id=c.id AND ma.availability_state=?)")) return 0;
  return 1;
}

static int bind_common_filters(sqlite3_stmt *statement,
                               const query_filter *filter, int posts) {
  int parameter;
  char media_type[260];
  parameter = 1;
  if (filter->source_id > 0) sqlite3_bind_int64(statement, parameter++, filter->source_id);
  if (filter->board != NULL) sqlite3_bind_text(statement, parameter++, filter->board, -1, SQLITE_TRANSIENT);
  if (filter->kind != NULL) sqlite3_bind_text(statement, parameter++, filter->kind, -1, SQLITE_TRANSIENT);
  if (filter->remote_id != NULL) sqlite3_bind_text(statement, parameter++, filter->remote_id, -1, SQLITE_TRANSIENT);
  if (filter->query != NULL) {
    sqlite3_bind_text(statement, parameter++, filter->query, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, parameter++, filter->query, -1, SQLITE_TRANSIENT);
    if (!posts) {
      sqlite3_bind_text(statement, parameter++, filter->query, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(statement, parameter++, filter->query, -1, SQLITE_TRANSIENT);
    }
  }
  if (filter->tag != NULL) {
    sqlite3_bind_text(statement, parameter++, filter->tag, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, parameter++, filter->tag, -1, SQLITE_TRANSIENT);
  }
  if (filter->rating != NULL) sqlite3_bind_text(statement, parameter++, filter->rating, -1, SQLITE_TRANSIENT);
  if (filter->created_from != NULL) sqlite3_bind_text(statement, parameter++, filter->created_from, -1, SQLITE_TRANSIENT);
  if (filter->created_to != NULL) sqlite3_bind_text(statement, parameter++, filter->created_to, -1, SQLITE_TRANSIENT);
  if (filter->updated_from != NULL) sqlite3_bind_text(statement, parameter++, filter->updated_from, -1, SQLITE_TRANSIENT);
  if (filter->updated_to != NULL) sqlite3_bind_text(statement, parameter++, filter->updated_to, -1, SQLITE_TRANSIENT);
  if (filter->lifecycle != NULL) sqlite3_bind_text(statement, parameter++, filter->lifecycle, -1, SQLITE_TRANSIENT);
  if (filter->media_type != NULL) {
    size_t length;
    (void) snprintf(media_type, sizeof(media_type), "%s", filter->media_type);
    length = strlen(media_type);
    if (length != 0U && media_type[length - 1U] == '*') media_type[length - 1U] = '%';
    sqlite3_bind_text(statement, parameter++, media_type, -1, SQLITE_TRANSIENT);
  }
  if (filter->minimum_width >= 0) sqlite3_bind_int(statement, parameter++, filter->minimum_width);
  if (filter->maximum_width >= 0) sqlite3_bind_int(statement, parameter++, filter->maximum_width);
  if (filter->minimum_height >= 0) sqlite3_bind_int(statement, parameter++, filter->minimum_height);
  if (filter->maximum_height >= 0) sqlite3_bind_int(statement, parameter++, filter->maximum_height);
  if (filter->minimum_bytes >= 0) sqlite3_bind_int64(statement, parameter++, filter->minimum_bytes);
  if (filter->maximum_bytes >= 0) sqlite3_bind_int64(statement, parameter++, filter->maximum_bytes);
  if (filter->availability != NULL) sqlite3_bind_text(statement, parameter++, filter->availability, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(statement, parameter++, filter->limit);
  sqlite3_bind_int(statement, parameter++, filter->offset);
  return parameter;
}

static int collection_query_statement(cli_context *context,
                                      const query_filter *filter,
                                      sqlite3_stmt **statement,
                                      int ids_only) {
  char sql[16384];
  if (ids_only)
    (void) strcpy(sql, "SELECT DISTINCT c.id FROM collections c JOIN sources s ON s.id=c.source_id LEFT JOIN boards b ON b.id=c.board_id WHERE s.deleted_at IS NULL");
  else
    (void) strcpy(sql, "SELECT DISTINCT c.id,s.id,s.adapter_kind,COALESCE(b.remote_id,''),c.kind,c.remote_id,COALESCE(c.title,''),c.lifecycle_state,COALESCE(c.created_at,''),COALESCE(c.updated_at,''),(SELECT COUNT(*) FROM posts cp WHERE cp.collection_id=c.id),(SELECT COUNT(*) FROM posts cp JOIN media cm ON cm.post_id=cp.id WHERE cp.collection_id=c.id) FROM collections c JOIN sources s ON s.id=c.source_id LEFT JOIN boards b ON b.id=c.board_id WHERE s.deleted_at IS NULL");
  if (!add_common_filters(sql, sizeof(sql), filter, 0) ||
      !sql_add(sql, sizeof(sql), " ORDER BY c.id LIMIT ? OFFSET ?") ||
      !prepare_sql(context, sql, statement)) return 0;
  (void) bind_common_filters(*statement, filter, 0);
  return 1;
}

static int command_collection_search(cli_context *context, int argc,
                                     char **argv) {
  query_filter filter;
  sqlite3_stmt *statement;
  int first;
  if (!parse_filters(context, argc, argv, &filter) || !open_catalog(context))
    return 2;
  if (!collection_query_statement(context, &filter, &statement, 0)) {
    close_catalog(context);
    return 2;
  }
  first = 1;
  if (context->json) (void) fputs("{\"collections\":[", stdout);
  while (sqlite3_step(statement) == SQLITE_ROW) {
    if (context->json) {
      if (!first) (void) fputc(',', stdout);
      (void) fputs("{\"board\":", stdout); json_string(stdout, column_text(statement, 3));
      (void) fprintf(stdout, ",\"id\":%lld,\"kind\":", (long long) sqlite3_column_int64(statement, 0));
      json_string(stdout, column_text(statement, 4));
      (void) fputs(",\"lifecycle\":", stdout); json_string(stdout, column_text(statement, 7));
      (void) fprintf(stdout, ",\"media_variants\":%lld,\"post_count\":%lld,\"remote_id\":",
                     (long long) sqlite3_column_int64(statement, 11),
                     (long long) sqlite3_column_int64(statement, 10));
      json_string(stdout, column_text(statement, 5));
      (void) fprintf(stdout, ",\"source_id\":%lld,\"title\":",
                     (long long) sqlite3_column_int64(statement, 1));
      json_string(stdout, column_text(statement, 6)); (void) fputc('}', stdout);
    } else {
      (void) printf("%lld\t%lld\t%s\t%s\t%s\t%s\n",
                    (long long) sqlite3_column_int64(statement, 0),
                    (long long) sqlite3_column_int64(statement, 1),
                    column_text(statement, 3), column_text(statement, 4),
                    column_text(statement, 5), column_text(statement, 6));
    }
    first = 0;
  }
  if (context->json)
    (void) fprintf(stdout, "],\"limit\":%d,\"offset\":%d}\n", filter.limit, filter.offset);
  (void) sqlite3_finalize(statement);
  close_catalog(context);
  return 0;
}

static int command_post_search(cli_context *context, int argc, char **argv) {
  query_filter filter;
  sqlite3_stmt *statement;
  char sql[16384];
  int first;
  if (!parse_filters(context, argc, argv, &filter) || !open_catalog(context)) return 2;
  (void) strcpy(sql, "SELECT p.id,p.source_id,COALESCE(b.remote_id,''),c.id,c.remote_id,p.remote_id,COALESCE(p.subject,''),COALESCE(p.body_text,''),COALESCE(p.rating,''),COALESCE(p.created_at,''),p.position,p.deleted FROM posts p JOIN collections c ON c.id=p.collection_id JOIN sources s ON s.id=p.source_id LEFT JOIN boards b ON b.id=c.board_id WHERE s.deleted_at IS NULL");
  if (!add_common_filters(sql, sizeof(sql), &filter, 1) ||
      !sql_add(sql, sizeof(sql), " ORDER BY p.id LIMIT ? OFFSET ?") ||
      !prepare_sql(context, sql, &statement)) {
    close_catalog(context); return 2;
  }
  (void) bind_common_filters(statement, &filter, 1);
  first = 1;
  if (context->json) (void) fputs("{\"posts\":[", stdout);
  while (sqlite3_step(statement) == SQLITE_ROW) {
    if (context->json) {
      if (!first) (void) fputc(',', stdout);
      (void) fputs("{\"board\":", stdout); json_string(stdout, column_text(statement, 2));
      (void) fprintf(stdout, ",\"collection_id\":%lld,\"created_at\":",
                     (long long) sqlite3_column_int64(statement, 3));
      json_string(stdout, column_text(statement, 9));
      (void) fprintf(stdout, ",\"deleted\":%s,\"id\":%lld,\"position\":%lld,\"rating\":",
                     sqlite3_column_int(statement, 11) ? "true" : "false",
                     (long long) sqlite3_column_int64(statement, 0),
                     (long long) sqlite3_column_int64(statement, 10));
      json_string(stdout, column_text(statement, 8));
      (void) fputs(",\"remote_id\":", stdout); json_string(stdout, column_text(statement, 5));
      (void) fprintf(stdout, ",\"source_id\":%lld,\"subject\":",
                     (long long) sqlite3_column_int64(statement, 1));
      json_string(stdout, column_text(statement, 6));
      (void) fputs(",\"text\":", stdout); json_string(stdout, column_text(statement, 7));
      (void) fputc('}', stdout);
    } else {
      (void) printf("%lld\t%s\t%s\t%s\n",
                    (long long) sqlite3_column_int64(statement, 0),
                    column_text(statement, 2), column_text(statement, 5),
                    column_text(statement, 6));
    }
    first = 0;
  }
  if (context->json)
    (void) fprintf(stdout, "],\"limit\":%d,\"offset\":%d}\n", filter.limit, filter.offset);
  (void) sqlite3_finalize(statement);
  close_catalog(context);
  return 0;
}

static int command_board_list(cli_context *context, int argc, char **argv) {
  int64_t source_id;
  int limit;
  int offset;
  int index;
  int first;
  sqlite3_stmt *statement;
  source_id = 0;
  limit = CLI_LIMIT_DEFAULT;
  offset = 0;
  for (index = 0; index < argc; index++) {
    if (strcmp(argv[index], "--source") == 0 && index + 1 < argc) {
      if (!parse_i64(argv[++index], &source_id) || source_id <= 0) return 2;
    } else if (strcmp(argv[index], "--limit") == 0 && index + 1 < argc) {
      if (!parse_int(argv[++index], &limit) || limit < 1 ||
          limit > CLI_LIMIT_MAX) return 2;
    } else if (strcmp(argv[index], "--offset") == 0 && index + 1 < argc) {
      if (!parse_int(argv[++index], &offset)) return 2;
    } else return 2;
  }
  if (!open_catalog(context)) return 2;
  if (!prepare_sql(context,
      "SELECT b.id,b.source_id,b.remote_id,b.name,COALESCE(b.display_name,''),"
      "COALESCE(b.last_seen_at,''),(SELECT COUNT(*) FROM collections c WHERE c.board_id=b.id) "
      "FROM boards b JOIN sources s ON s.id=b.source_id WHERE s.deleted_at IS NULL "
      "AND (?=0 OR b.source_id=?) ORDER BY b.id LIMIT ? OFFSET ?", &statement)) {
    close_catalog(context); return 2;
  }
  sqlite3_bind_int64(statement, 1, source_id);
  sqlite3_bind_int64(statement, 2, source_id);
  sqlite3_bind_int(statement, 3, limit);
  sqlite3_bind_int(statement, 4, offset);
  first = 1;
  if (context->json) (void) fputs("{\"boards\":[", stdout);
  while (sqlite3_step(statement) == SQLITE_ROW) {
    if (context->json) {
      if (!first) (void) fputc(',', stdout);
      (void) fprintf(stdout, "{\"collection_count\":%lld,\"display_name\":",
                     (long long) sqlite3_column_int64(statement, 6));
      json_string(stdout, column_text(statement, 4));
      (void) fprintf(stdout, ",\"id\":%lld,\"last_seen_at\":",
                     (long long) sqlite3_column_int64(statement, 0));
      json_string(stdout, column_text(statement, 5));
      (void) fputs(",\"remote_id\":", stdout); json_string(stdout, column_text(statement, 2));
      (void) fprintf(stdout, ",\"source_id\":%lld}",
                     (long long) sqlite3_column_int64(statement, 1));
    } else
      (void) printf("%lld\t%lld\t%s\t%s\t%lld collections\n",
                    (long long) sqlite3_column_int64(statement, 0),
                    (long long) sqlite3_column_int64(statement, 1),
                    column_text(statement, 2), column_text(statement, 4),
                    (long long) sqlite3_column_int64(statement, 6));
    first = 0;
  }
  if (context->json)
    (void) fprintf(stdout, "],\"limit\":%d,\"offset\":%d}\n", limit, offset);
  sqlite3_finalize(statement);
  close_catalog(context);
  return 0;
}

static int command_board_inspect(cli_context *context, const char *id_text) {
  int64_t id;
  sqlite3_stmt *statement;
  if (!parse_i64(id_text, &id) || id <= 0 || !open_catalog(context)) return 2;
  if (!prepare_sql(context,
      "SELECT b.id,b.source_id,b.remote_id,b.name,COALESCE(b.display_name,''),"
      "COALESCE(b.capabilities_json,'{}'),COALESCE(b.last_seen_at,''),"
      "COALESCE(b.metadata_json,'{}'),(SELECT COUNT(*) FROM collections c WHERE c.board_id=b.id) "
      "FROM boards b WHERE b.id=?", &statement)) {
    close_catalog(context); return 2;
  }
  sqlite3_bind_int64(statement, 1, id);
  if (sqlite3_step(statement) != SQLITE_ROW) {
    print_error(context, "board not found"); sqlite3_finalize(statement);
    close_catalog(context); return 1;
  }
  if (context->json) {
    (void) fputs("{\"capabilities\":", stdout); (void) fputs(column_text(statement, 5), stdout);
    (void) fprintf(stdout, ",\"collection_count\":%lld,\"display_name\":",
                   (long long) sqlite3_column_int64(statement, 8));
    json_string(stdout, column_text(statement, 4));
    (void) fprintf(stdout, ",\"id\":%lld,\"last_seen_at\":", (long long) id);
    json_string(stdout, column_text(statement, 6));
    (void) fputs(",\"metadata\":", stdout); (void) fputs(column_text(statement, 7), stdout);
    (void) fputs(",\"remote_id\":", stdout); json_string(stdout, column_text(statement, 2));
    (void) fprintf(stdout, ",\"source_id\":%lld}\n",
                   (long long) sqlite3_column_int64(statement, 1));
  } else
    (void) printf("Board %lld\nSource: %lld\nRemote ID: %s\nName: %s\nCollections: %lld\n",
                  (long long) id, (long long) sqlite3_column_int64(statement, 1),
                  column_text(statement, 2), column_text(statement, 4),
                  (long long) sqlite3_column_int64(statement, 8));
  sqlite3_finalize(statement); close_catalog(context); return 0;
}

static int command_collection_inspect(cli_context *context,
                                      const char *id_text) {
  int64_t id;
  sqlite3_stmt *statement;
  if (!parse_i64(id_text, &id) || id <= 0 || !open_catalog(context)) return 2;
  if (!prepare_sql(context,
      "SELECT c.id,c.source_id,COALESCE(b.remote_id,''),c.kind,c.remote_id,"
      "COALESCE(c.title,''),COALESCE(c.description,''),c.lifecycle_state,"
      "COALESCE(c.created_at,''),COALESCE(c.updated_at,''),COALESCE(c.metadata_json,'{}'),"
      "(SELECT COUNT(*) FROM posts p WHERE p.collection_id=c.id),"
      "(SELECT COUNT(*) FROM posts p JOIN media m ON m.post_id=p.id WHERE p.collection_id=c.id) "
      "FROM collections c LEFT JOIN boards b ON b.id=c.board_id WHERE c.id=?", &statement)) {
    close_catalog(context); return 2;
  }
  sqlite3_bind_int64(statement, 1, id);
  if (sqlite3_step(statement) != SQLITE_ROW) {
    print_error(context, "collection not found"); sqlite3_finalize(statement);
    close_catalog(context); return 1;
  }
  if (context->json) {
    (void) fputs("{\"board\":", stdout); json_string(stdout, column_text(statement, 2));
    (void) fprintf(stdout, ",\"id\":%lld,\"kind\":", (long long) id);
    json_string(stdout, column_text(statement, 3));
    (void) fputs(",\"lifecycle\":", stdout); json_string(stdout, column_text(statement, 7));
    (void) fprintf(stdout, ",\"media_variants\":%lld,\"metadata\":",
                   (long long) sqlite3_column_int64(statement, 12));
    (void) fputs(column_text(statement, 10), stdout);
    (void) fprintf(stdout, ",\"post_count\":%lld,\"remote_id\":",
                   (long long) sqlite3_column_int64(statement, 11));
    json_string(stdout, column_text(statement, 4));
    (void) fprintf(stdout, ",\"source_id\":%lld,\"title\":",
                   (long long) sqlite3_column_int64(statement, 1));
    json_string(stdout, column_text(statement, 5)); (void) fputs("}\n", stdout);
  } else
    (void) printf("Collection %lld\nSource: %lld\nBoard: %s\nKind: %s\nRemote ID: %s\n"
                  "Title: %s\nLifecycle: %s\nPosts: %lld\nMedia variants: %lld\n",
                  (long long) id, (long long) sqlite3_column_int64(statement, 1),
                  column_text(statement, 2), column_text(statement, 3),
                  column_text(statement, 4), column_text(statement, 5),
                  column_text(statement, 7),
                  (long long) sqlite3_column_int64(statement, 11),
                  (long long) sqlite3_column_int64(statement, 12));
  sqlite3_finalize(statement); close_catalog(context); return 0;
}

static int freeze_selection_members(cli_context *context, int64_t selection_id,
                                    int64_t source_id) {
  sqlite3_stmt *statement;
  const char *post_sql;
  const char *media_sql;
  post_sql =
    "INSERT INTO selection_posts(selection_id,source_id,post_id,position,frozen_at) "
    "SELECT ?,?,p.id,ROW_NUMBER() OVER (ORDER BY sc.position,p.position,p.id)-1,"
    "strftime('%Y-%m-%dT%H:%M:%fZ','now') FROM selection_collections sc "
    "JOIN posts p ON p.collection_id=sc.collection_id "
    "WHERE sc.selection_id=? ORDER BY sc.position,p.position,p.id";
  media_sql =
    "INSERT INTO selection_media(selection_id,source_id,media_id,position,frozen_at) "
    "SELECT ?,?,m.id,ROW_NUMBER() OVER (ORDER BY sc.position,p.position,"
    "CASE m.variant_kind WHEN 'original' THEN 0 WHEN 'sample' THEN 1 ELSE 2 END,m.id)-1,"
    "strftime('%Y-%m-%dT%H:%M:%fZ','now') FROM selection_collections sc "
    "JOIN posts p ON p.collection_id=sc.collection_id JOIN media m ON m.post_id=p.id "
    "WHERE sc.selection_id=? ORDER BY sc.position,p.position,m.id";
  if (!prepare_sql(context, post_sql, &statement)) return 0;
  sqlite3_bind_int64(statement, 1, selection_id);
  sqlite3_bind_int64(statement, 2, source_id);
  sqlite3_bind_int64(statement, 3, selection_id);
  if (sqlite3_step(statement) != SQLITE_DONE) {
    print_error(context, sqlite3_errmsg(context->db)); sqlite3_finalize(statement); return 0;
  }
  sqlite3_finalize(statement);
  if (!prepare_sql(context, media_sql, &statement)) return 0;
  sqlite3_bind_int64(statement, 1, selection_id);
  sqlite3_bind_int64(statement, 2, source_id);
  sqlite3_bind_int64(statement, 3, selection_id);
  if (sqlite3_step(statement) != SQLITE_DONE) {
    print_error(context, sqlite3_errmsg(context->db)); sqlite3_finalize(statement); return 0;
  }
  sqlite3_finalize(statement);
  return 1;
}

static int insert_selection_collection(cli_context *context,
                                       int64_t selection_id,
                                       int64_t source_id,
                                       int64_t collection_id,
                                       int position) {
  sqlite3_stmt *statement;
  if (!prepare_sql(context,
      "INSERT INTO selection_collections(selection_id,source_id,collection_id,position) "
      "SELECT ?,?,?,? WHERE EXISTS (SELECT 1 FROM collections WHERE id=? AND source_id=?)",
      &statement)) return 0;
  sqlite3_bind_int64(statement, 1, selection_id);
  sqlite3_bind_int64(statement, 2, source_id);
  sqlite3_bind_int64(statement, 3, collection_id);
  sqlite3_bind_int(statement, 4, position);
  sqlite3_bind_int64(statement, 5, collection_id);
  sqlite3_bind_int64(statement, 6, source_id);
  if (sqlite3_step(statement) != SQLITE_DONE || sqlite3_changes(context->db) != 1) {
    print_error(context, "collection is missing, duplicated, or belongs to another source");
    sqlite3_finalize(statement); return 0;
  }
  sqlite3_finalize(statement);
  return 1;
}

static int selection_create(cli_context *context, int argc, char **argv) {
  const char *name;
  int64_t source_id;
  int64_t *collection_ids;
  size_t collection_count;
  size_t collection_capacity;
  int from_search;
  char **filter_args;
  int filter_count;
  int index;
  query_filter filter;
  sqlite3_stmt *statement;
  int64_t selection_id;
  int position;
  if (argc < 1) return 2;
  name = argv[0];
  source_id = 0;
  collection_ids = NULL;
  collection_count = 0U;
  collection_capacity = 0U;
  from_search = 0;
  filter_args = (char **) calloc((size_t) argc, sizeof(char *));
  if (filter_args == NULL) return 2;
  filter_count = 0;
  for (index = 1; index < argc; index++) {
    if (strcmp(argv[index], "--source") == 0 && index + 1 < argc) {
      if (!parse_i64(argv[++index], &source_id) || source_id <= 0) goto invalid;
    } else if (strcmp(argv[index], "--collection") == 0 && index + 1 < argc) {
      int64_t id;
      int64_t *next;
      if (!parse_i64(argv[++index], &id) || id <= 0) goto invalid;
      if (collection_count == collection_capacity) {
        collection_capacity = collection_capacity == 0U ? 8U : collection_capacity * 2U;
        next = (int64_t *) realloc(collection_ids,
                                   collection_capacity * sizeof(int64_t));
        if (next == NULL) goto invalid;
        collection_ids = next;
      }
      collection_ids[collection_count++] = id;
    } else if (strcmp(argv[index], "--from-search") == 0) {
      from_search = 1;
    } else {
      filter_args[filter_count++] = argv[index];
      if (index + 1 < argc && strncmp(argv[index], "--", 2U) == 0 &&
          strcmp(argv[index], "--from-search") != 0) {
        filter_args[filter_count++] = argv[++index];
      }
    }
  }
  if (source_id <= 0 || (collection_count == 0U) == !from_search ||
      (collection_count != 0U && from_search)) goto invalid;
  filter_defaults(&filter);
  if (from_search) {
    if (!parse_filters(context, filter_count, filter_args, &filter)) goto failure;
    filter.source_id = source_id;
    if (filter.limit == CLI_LIMIT_DEFAULT) filter.limit = CLI_LIMIT_MAX;
  } else if (filter_count != 0) goto invalid;
  if (!open_catalog(context)) goto failure;
  if (!execute(context, "BEGIN IMMEDIATE")) goto database_failure;
  if (!prepare_sql(context,
      "INSERT INTO selections(source_id,name,kind,definition_json,enabled) "
      "VALUES(?,?,'frozen',?,1)", &statement)) goto rollback;
  sqlite3_bind_int64(statement, 1, source_id);
  sqlite3_bind_text(statement, 2, name, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement, 3,
                    from_search
                      ? "{\"immutable\":true,\"origin\":\"search\"}"
                      : "{\"immutable\":true,\"origin\":\"explicit\"}",
                    -1, SQLITE_STATIC);
  if (sqlite3_step(statement) != SQLITE_DONE) {
    print_error(context, "selection name already exists or is invalid");
    sqlite3_finalize(statement); goto rollback;
  }
  sqlite3_finalize(statement);
  selection_id = sqlite3_last_insert_rowid(context->db);
  position = 0;
  if (from_search) {
    if (!collection_query_statement(context, &filter, &statement, 1)) goto rollback;
    while (sqlite3_step(statement) == SQLITE_ROW) {
      if (!insert_selection_collection(context, selection_id, source_id,
                                       sqlite3_column_int64(statement, 0),
                                       position++)) {
        sqlite3_finalize(statement); goto rollback;
      }
    }
    sqlite3_finalize(statement);
  } else {
    size_t item;
    for (item = 0U; item < collection_count; item++) {
      if (!insert_selection_collection(context, selection_id, source_id,
                                       collection_ids[item], position++))
        goto rollback;
    }
  }
  if (position == 0) {
    print_error(context, "selection search produced no collections");
    goto rollback;
  }
  if (!freeze_selection_members(context, selection_id, source_id) ||
      !execute(context, "COMMIT")) goto rollback;
  if (context->json)
    (void) printf("{\"collection_count\":%d,\"id\":%lld,\"immutable\":true,\"name\":",
                  position, (long long) selection_id), json_string(stdout, name),
                  (void) fputs("}\n", stdout);
  else
    (void) printf("Created immutable selection '%s' with %d collections.\n",
                  name, position);
  close_catalog(context);
  free(filter_args); free(collection_ids); return 0;
rollback:
  (void) execute(context, "ROLLBACK");
database_failure:
  close_catalog(context);
failure:
  free(filter_args); free(collection_ids); return 2;
invalid:
  print_error(context, "selection create requires NAME, --source, and either --collection IDs or --from-search filters");
  goto failure;
}

static int find_selection(cli_context *context, int64_t source_id,
                          const char *name, int64_t *selection_id) {
  sqlite3_stmt *statement;
  if (!prepare_sql(context,
      "SELECT id FROM selections WHERE source_id=? AND name=?", &statement)) return 0;
  sqlite3_bind_int64(statement, 1, source_id);
  sqlite3_bind_text(statement, 2, name, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(statement) != SQLITE_ROW) {
    sqlite3_finalize(statement); return 0;
  }
  *selection_id = sqlite3_column_int64(statement, 0);
  sqlite3_finalize(statement); return 1;
}

static int selection_refresh(cli_context *context, int argc, char **argv) {
  const char *name;
  int64_t source_id;
  int64_t selection_id;
  int index;
  sqlite3_stmt *statement;
  if (argc < 1) return 2;
  name = argv[0]; source_id = 0;
  for (index = 1; index < argc; index++) {
    if (strcmp(argv[index], "--source") == 0 && index + 1 < argc) {
      if (!parse_i64(argv[++index], &source_id) || source_id <= 0) return 2;
    } else return 2;
  }
  if (source_id <= 0 || !open_catalog(context) ||
      !find_selection(context, source_id, name, &selection_id)) {
    print_error(context, "selection not found"); close_catalog(context); return 1;
  }
  if (!execute(context, "BEGIN IMMEDIATE")) goto refresh_failure;
  if (!prepare_sql(context,
      "DELETE FROM selection_media WHERE selection_id=?", &statement)) goto refresh_rollback;
  sqlite3_bind_int64(statement, 1, selection_id);
  if (sqlite3_step(statement) != SQLITE_DONE) { sqlite3_finalize(statement); goto refresh_rollback; }
  sqlite3_finalize(statement);
  if (!prepare_sql(context,
      "DELETE FROM selection_posts WHERE selection_id=?", &statement)) goto refresh_rollback;
  sqlite3_bind_int64(statement, 1, selection_id);
  if (sqlite3_step(statement) != SQLITE_DONE) { sqlite3_finalize(statement); goto refresh_rollback; }
  sqlite3_finalize(statement);
  if (!freeze_selection_members(context, selection_id, source_id))
    goto refresh_rollback;
  if (!prepare_sql(context,
      "UPDATE selections SET updated_at=strftime('%Y-%m-%dT%H:%M:%fZ','now') WHERE id=?",
      &statement)) goto refresh_rollback;
  sqlite3_bind_int64(statement, 1, selection_id);
  if (sqlite3_step(statement) != SQLITE_DONE) {
    sqlite3_finalize(statement); goto refresh_rollback;
  }
  sqlite3_finalize(statement);
  if (!execute(context, "COMMIT")) goto refresh_rollback;
  if (context->json)
    (void) printf("{\"id\":%lld,\"name\":", (long long) selection_id),
    json_string(stdout, name), (void) fputs(",\"refreshed\":true}\n", stdout);
  else (void) printf("Explicitly refreshed frozen membership for '%s'.\n", name);
  close_catalog(context); return 0;
refresh_rollback:
  (void) execute(context, "ROLLBACK");
refresh_failure:
  close_catalog(context); return 2;
}

static int selection_stats(cli_context *context, int argc, char **argv) {
  const char *name;
  int64_t source_id;
  int64_t selection_id;
  int index;
  sqlite3_stmt *statement;
  if (argc < 1) return 2;
  name = argv[0]; source_id = 0;
  for (index = 1; index < argc; index++) {
    if (strcmp(argv[index], "--source") == 0 && index + 1 < argc) {
      if (!parse_i64(argv[++index], &source_id) || source_id <= 0) return 2;
    } else return 2;
  }
  if (source_id <= 0 || !open_catalog(context) ||
      !find_selection(context, source_id, name, &selection_id)) {
    print_error(context, "selection not found"); close_catalog(context); return 1;
  }
  if (!prepare_sql(context,
      "SELECT (SELECT COUNT(*) FROM selection_collections WHERE selection_id=?),"
      "(SELECT COUNT(*) FROM selection_posts WHERE selection_id=?),"
      "(SELECT COUNT(*) FROM selection_media WHERE selection_id=?),"
      "(SELECT COALESCE(SUM(m.size_bytes),0) FROM selection_media sm JOIN media m ON m.id=sm.media_id WHERE sm.selection_id=? AND m.variant_kind='original'),"
      "(SELECT COUNT(*) FROM selection_media sm JOIN media m ON m.id=sm.media_id WHERE sm.selection_id=? AND m.variant_kind='original' AND m.availability_state NOT IN ('available','archived')) ,"
      "(SELECT COUNT(DISTINCT sm.media_id) FROM selection_media sm JOIN download_jobs d ON d.media_id=sm.media_id AND d.status='complete' WHERE sm.selection_id=?),"
      "(SELECT COUNT(*) FROM (SELECT m.remote_hash FROM selection_media sm JOIN media m ON m.id=sm.media_id WHERE sm.selection_id=? AND m.variant_kind='original' AND m.remote_hash IS NOT NULL GROUP BY m.hash_algorithm,m.remote_hash HAVING COUNT(*)>1))",
      &statement)) { close_catalog(context); return 2; }
  for (index = 1; index <= 7; index++) sqlite3_bind_int64(statement, index, selection_id);
  if (sqlite3_step(statement) != SQLITE_ROW) { sqlite3_finalize(statement); close_catalog(context); return 2; }
  if (context->json) {
    (void) printf("{\"already_local_items\":%lld,\"collections\":%lld,"
                  "\"duplicate_content_candidates\":%lld,\"expected_original_bytes\":%lld,"
                  "\"immutable\":true,\"media_variants\":%lld,\"name\":",
                  (long long) sqlite3_column_int64(statement, 5),
                  (long long) sqlite3_column_int64(statement, 0),
                  (long long) sqlite3_column_int64(statement, 6),
                  (long long) sqlite3_column_int64(statement, 3),
                  (long long) sqlite3_column_int64(statement, 2));
    json_string(stdout, name);
    (void) printf(",\"posts\":%lld,\"selection_id\":%lld,\"unavailable_items\":%lld}\n",
                  (long long) sqlite3_column_int64(statement, 1),
                  (long long) selection_id,
                  (long long) sqlite3_column_int64(statement, 4));
  } else {
    (void) printf("Selection: %s (immutable)\nCollections: %lld\nPosts: %lld\n"
                  "Media variants: %lld\nExpected original bytes: %lld\nUnavailable originals: %lld\n"
                  "Already local: %lld\nDuplicate content candidates: %lld\n",
                  name, (long long) sqlite3_column_int64(statement, 0),
                  (long long) sqlite3_column_int64(statement, 1),
                  (long long) sqlite3_column_int64(statement, 2),
                  (long long) sqlite3_column_int64(statement, 3),
                  (long long) sqlite3_column_int64(statement, 4),
                  (long long) sqlite3_column_int64(statement, 5),
                  (long long) sqlite3_column_int64(statement, 6));
  }
  sqlite3_finalize(statement); close_catalog(context); return 0;
}

static int selection_list(cli_context *context, int argc, char **argv) {
  int64_t source_id;
  int limit;
  int offset;
  int index;
  int first;
  sqlite3_stmt *statement;
  source_id = 0; limit = CLI_LIMIT_DEFAULT; offset = 0;
  for (index = 0; index < argc; index++) {
    if (strcmp(argv[index], "--source") == 0 && index + 1 < argc) {
      if (!parse_i64(argv[++index], &source_id) || source_id <= 0) return 2;
    } else if (strcmp(argv[index], "--limit") == 0 && index + 1 < argc) {
      if (!parse_int(argv[++index], &limit) || limit < 1 || limit > CLI_LIMIT_MAX) return 2;
    } else if (strcmp(argv[index], "--offset") == 0 && index + 1 < argc) {
      if (!parse_int(argv[++index], &offset)) return 2;
    } else return 2;
  }
  if (!open_catalog(context)) return 2;
  if (!prepare_sql(context,
      "SELECT sl.id,sl.source_id,sl.name,sl.kind,sl.enabled,sl.created_at,sl.updated_at,"
      "(SELECT COUNT(*) FROM selection_collections sc WHERE sc.selection_id=sl.id) "
      "FROM selections sl JOIN sources s ON s.id=sl.source_id WHERE s.deleted_at IS NULL "
      "AND (?=0 OR sl.source_id=?) ORDER BY sl.id LIMIT ? OFFSET ?", &statement)) {
    close_catalog(context); return 2;
  }
  sqlite3_bind_int64(statement, 1, source_id); sqlite3_bind_int64(statement, 2, source_id);
  sqlite3_bind_int(statement, 3, limit); sqlite3_bind_int(statement, 4, offset);
  first = 1;
  if (context->json) (void) fputs("{\"selections\":[", stdout);
  while (sqlite3_step(statement) == SQLITE_ROW) {
    if (context->json) {
      if (!first) (void) fputc(',', stdout);
      (void) fprintf(stdout, "{\"collection_count\":%lld,\"enabled\":%s,\"id\":%lld,\"immutable\":true,\"name\":",
                     (long long) sqlite3_column_int64(statement, 7),
                     sqlite3_column_int(statement, 4) ? "true" : "false",
                     (long long) sqlite3_column_int64(statement, 0));
      json_string(stdout, column_text(statement, 2));
      (void) fprintf(stdout, ",\"source_id\":%lld}",
                     (long long) sqlite3_column_int64(statement, 1));
    } else
      (void) printf("%lld\t%lld\t%s\t%lld collections\timmutable\n",
                    (long long) sqlite3_column_int64(statement, 0),
                    (long long) sqlite3_column_int64(statement, 1),
                    column_text(statement, 2),
                    (long long) sqlite3_column_int64(statement, 7));
    first = 0;
  }
  if (context->json)
    (void) fprintf(stdout, "],\"limit\":%d,\"offset\":%d}\n", limit, offset);
  sqlite3_finalize(statement); close_catalog(context); return 0;
}

static int path_seen(char **paths, size_t count, const char *path) {
  size_t index;
  for (index = 0U; index < count; index++)
    if (strcmp(paths[index], path) == 0) return 1;
  return 0;
}

static int selection_preview(cli_context *context, int argc, char **argv) {
  const char *name;
  int64_t source_id;
  int64_t selection_id;
  int index;
  int first;
  sqlite3_stmt *statement;
  char **paths;
  char **availability_values;
  int64_t *media_ids;
  size_t path_count;
  size_t path_capacity;
  unsigned long ordinal;
  int step_result;
  if (argc < 1) return 2;
  name = argv[0]; source_id = 0;
  for (index = 1; index < argc; index++) {
    if (strcmp(argv[index], "--source") == 0 && index + 1 < argc) {
      if (!parse_i64(argv[++index], &source_id) || source_id <= 0) return 2;
    } else return 2;
  }
  if (source_id <= 0 || !open_catalog(context) ||
      !find_selection(context, source_id, name, &selection_id)) {
    print_error(context, "selection not found"); close_catalog(context); return 1;
  }
  if (!prepare_sql(context,
      "SELECT s.adapter_kind||'-'||s.id,COALESCE(b.remote_id,'unboarded'),c.remote_id,"
      "COALESCE(c.title,'untitled'),p.remote_id,COALESCE(m.original_filename,'unnamed'),"
      "COALESCE(m.extension,''),m.remote_id,m.id,m.availability_state "
      "FROM selection_media sm JOIN media m ON m.id=sm.media_id "
      "JOIN posts p ON p.id=m.post_id JOIN collections c ON c.id=p.collection_id "
      "JOIN sources s ON s.id=m.source_id LEFT JOIN boards b ON b.id=c.board_id "
      "WHERE sm.selection_id=? AND m.variant_kind='original' ORDER BY sm.position,m.id",
      &statement)) { close_catalog(context); return 2; }
  sqlite3_bind_int64(statement, 1, selection_id);
  paths = NULL; availability_values = NULL; media_ids = NULL;
  path_count = 0U; path_capacity = 0U; ordinal = 1U; first = 1;
  while ((step_result = sqlite3_step(statement)) == SQLITE_ROW) {
    char path[HTS_CATALOG_PREVIEW_PATH_MAX + 1U];
    char suffix[32];
    char *copy;
    char *availability_copy;
    char **next;
    int64_t *next_ids;
    size_t next_capacity;
    suffix[0] = '\0';
    if (!hts_catalog_preview_path(
          column_text(statement, 0), column_text(statement, 1),
          column_text(statement, 2), column_text(statement, 3), ordinal,
          column_text(statement, 4), column_text(statement, 5),
          column_text(statement, 6), column_text(statement, 7), NULL,
          path, sizeof(path))) {
      print_error(context, "could not create a safe preview path");
      goto preview_failure;
    }
    if (path_seen(paths, path_count, path)) {
      (void) snprintf(suffix, sizeof(suffix), "~m%lld",
                      (long long) sqlite3_column_int64(statement, 8));
      if (!hts_catalog_preview_path(
            column_text(statement, 0), column_text(statement, 1),
            column_text(statement, 2), column_text(statement, 3), ordinal,
            column_text(statement, 4), column_text(statement, 5),
            column_text(statement, 6), column_text(statement, 7), suffix,
            path, sizeof(path)) || path_seen(paths, path_count, path)) {
        print_error(context, "could not resolve a deterministic path collision");
        goto preview_failure;
      }
    }
    copy = strdup(path);
    if (copy == NULL) goto preview_failure;
    availability_copy = strdup(column_text(statement, 9));
    if (availability_copy == NULL) { free(copy); goto preview_failure; }
    if (path_count == path_capacity) {
      next_capacity = path_capacity == 0U ? 16U : path_capacity * 2U;
      next = (char **) realloc(paths, next_capacity * sizeof(char *));
      if (next == NULL) {
        free(availability_copy); free(copy); goto preview_failure;
      }
      paths = next;
      next = (char **) realloc(availability_values,
                               next_capacity * sizeof(char *));
      if (next == NULL) {
        free(availability_copy); free(copy); goto preview_failure;
      }
      availability_values = next;
      next_ids = (int64_t *) realloc(media_ids,
                                     next_capacity * sizeof(int64_t));
      if (next_ids == NULL) {
        free(availability_copy); free(copy); goto preview_failure;
      }
      media_ids = next_ids;
      path_capacity = next_capacity;
    }
    paths[path_count] = copy;
    availability_values[path_count] = availability_copy;
    media_ids[path_count] = sqlite3_column_int64(statement, 8);
    path_count++;
    ordinal++;
  }
  if (step_result != SQLITE_DONE) {
    print_error(context, "could not read selection preview");
    goto preview_failure;
  }
  if (context->json) (void) fputs("{\"metadata_only\":true,\"paths\":[", stdout);
  for (ordinal = 0U; ordinal < path_count; ordinal++) {
    if (context->json) {
      if (!first) (void) fputc(',', stdout);
      (void) fputs("{\"availability\":", stdout);
      json_string(stdout, availability_values[ordinal]);
      (void) fprintf(stdout, ",\"media_id\":%lld,\"path\":",
                     (long long) media_ids[ordinal]);
      json_string(stdout, paths[ordinal]); (void) fputc('}', stdout);
    } else (void) puts(paths[ordinal]);
    first = 0;
  }
  if (context->json) {
    (void) fputs("],\"policy\":", stdout);
    json_string(stdout, "source/board/collection-id — sanitized-slug/0001-post-id-sanitized-original-name.ext");
    (void) fputs("}\n", stdout);
  }
  sqlite3_finalize(statement);
  while (path_count != 0U) {
    path_count--;
    free(paths[path_count]);
    free(availability_values[path_count]);
  }
  free(media_ids); free(availability_values); free(paths);
  close_catalog(context); return 0;
preview_failure:
  sqlite3_finalize(statement);
  while (path_count != 0U) {
    path_count--;
    free(paths[path_count]);
    free(availability_values[path_count]);
  }
  free(media_ids); free(availability_values); free(paths);
  close_catalog(context); return 2;
}

static void print_usage(FILE *stream) {
  (void) fputs(
    "Usage: httrack-catalog [--catalog PATH] [--json] COMMAND ...\n"
    "Commands:\n"
    "  init | schema\n"
    "  source add|list|inspect|enable|disable|remove (remove requires --yes)\n"
    "  sync --source ID [--board BOARD] [--dry-run|--status] [--fixture-dir DIR]\n"
    "  sync --all [--dry-run] [--fixture-dir DIR]\n"
    "  board list|inspect [--source ID] [--limit N --offset N]\n"
    "  collection list|inspect [FILTERS] [--limit N --offset N]\n"
    "  search collections|posts [FILTERS] [--limit N --offset N]\n"
    "  selection create|refresh|list|stats|preview\n"
    "Selection creation:\n"
    "  selection create NAME --source ID --collection ID [--collection ID ...]\n"
    "  selection create NAME --source ID --from-search [FILTERS]\n"
    "Search filters:\n"
    "  --source ID --board BOARD --kind KIND --remote-id ID --query TEXT --tag TAG\n"
    "  --rating RATING --created-from VALUE --created-to VALUE\n"
    "  --updated-from VALUE --updated-to VALUE --media-type TYPE\n"
    "  --min-width N --max-width N --min-height N --max-height N\n"
    "  --min-bytes N --max-bytes N --lifecycle STATE --availability STATE\n",
    stream);
}

int main(int argc, char **argv) {
  cli_context context;
  int index;
  const char *command;
  int remaining;
  char **arguments;
  (void) memset(&context, 0, sizeof(context));
  context.catalog_path = "httrack-catalog.sqlite3";
  index = 1;
  while (index < argc && strncmp(argv[index], "--", 2U) == 0) {
    if (strcmp(argv[index], "--catalog") == 0 && index + 1 < argc) {
      context.catalog_path = argv[index + 1]; index += 2;
    } else if (strcmp(argv[index], "--json") == 0) {
      context.json = 1; index++;
    } else if (strcmp(argv[index], "--help") == 0) {
      print_usage(stdout); return 0;
    } else break;
  }
  if (index >= argc) { print_usage(stderr); return 2; }
  command = argv[index++]; remaining = argc - index; arguments = argv + index;
  if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
    print_error(&context, "could not initialize HTTPS transport"); return 2;
  }
  if (strcmp(command, "init") == 0 || strcmp(command, "schema") == 0) {
    index = remaining == 0 ? command_init(&context) : 2;
  } else if (strcmp(command, "source") == 0 && remaining >= 1) {
    const char *action = arguments[0];
    if (strcmp(action, "add") == 0)
      index = source_add(&context, remaining - 1, arguments + 1);
    else if (strcmp(action, "list") == 0)
      index = source_list(&context, remaining - 1, arguments + 1);
    else if (strcmp(action, "inspect") == 0 && remaining == 2)
      index = source_inspect(&context, arguments[1]);
    else if ((strcmp(action, "enable") == 0 || strcmp(action, "disable") == 0) && remaining == 2)
      index = source_state(&context, action, arguments[1], 0);
    else if (strcmp(action, "remove") == 0 && (remaining == 2 || remaining == 3))
      index = source_state(&context, action, arguments[1],
                           remaining == 3 && strcmp(arguments[2], "--yes") == 0);
    else index = 2;
  } else if (strcmp(command, "sync") == 0) {
    index = command_sync(&context, remaining, arguments);
  } else if (strcmp(command, "board") == 0 && remaining >= 1) {
    if (strcmp(arguments[0], "list") == 0)
      index = command_board_list(&context, remaining - 1, arguments + 1);
    else if (strcmp(arguments[0], "inspect") == 0 && remaining == 2)
      index = command_board_inspect(&context, arguments[1]);
    else index = 2;
  } else if (strcmp(command, "collection") == 0 && remaining >= 1) {
    if (strcmp(arguments[0], "list") == 0)
      index = command_collection_search(&context, remaining - 1, arguments + 1);
    else if (strcmp(arguments[0], "inspect") == 0 && remaining == 2)
      index = command_collection_inspect(&context, arguments[1]);
    else index = 2;
  } else if (strcmp(command, "search") == 0 && remaining >= 1) {
    if (strcmp(arguments[0], "collections") == 0)
      index = command_collection_search(&context, remaining - 1, arguments + 1);
    else if (strcmp(arguments[0], "posts") == 0)
      index = command_post_search(&context, remaining - 1, arguments + 1);
    else index = 2;
  } else if (strcmp(command, "selection") == 0 && remaining >= 1) {
    if (strcmp(arguments[0], "create") == 0)
      index = selection_create(&context, remaining - 1, arguments + 1);
    else if (strcmp(arguments[0], "refresh") == 0)
      index = selection_refresh(&context, remaining - 1, arguments + 1);
    else if (strcmp(arguments[0], "list") == 0)
      index = selection_list(&context, remaining - 1, arguments + 1);
    else if (strcmp(arguments[0], "stats") == 0)
      index = selection_stats(&context, remaining - 1, arguments + 1);
    else if (strcmp(arguments[0], "preview") == 0)
      index = selection_preview(&context, remaining - 1, arguments + 1);
    else index = 2;
  } else {
    index = 2;
  }
  if (index == 2) print_usage(stderr);
  curl_global_cleanup();
  return index;
}
