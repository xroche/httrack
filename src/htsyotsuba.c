#include "htsyotsuba.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define YOTSUBA_BOARD_MAX 32U
#define YOTSUBA_VERSION_MAX 64U
#define YOTSUBA_MAX_BOARDS 512U
#define YOTSUBA_MAX_THREADS 16384U
#define YOTSUBA_MAX_POSTS 10000U

typedef enum y_token_type {
  Y_TOKEN_OBJECT = 1,
  Y_TOKEN_ARRAY,
  Y_TOKEN_STRING,
  Y_TOKEN_PRIMITIVE
} y_token_type;

typedef struct y_token {
  y_token_type type;
  size_t start;
  size_t end;
  size_t size;
  int parent;
} y_token;

typedef struct y_document {
  const char *json;
  size_t length;
  y_token *tokens;
  size_t token_count;
} y_document;

typedef enum y_endpoint {
  Y_ENDPOINT_NONE = 0,
  Y_ENDPOINT_BOARDS,
  Y_ENDPOINT_THREADS,
  Y_ENDPOINT_ARCHIVE,
  Y_ENDPOINT_THREAD
} y_endpoint;

typedef struct y_known_resource {
  char resource_id[HTS_CATALOG_REMOTE_ID_MAX + 1U];
  char parent_remote_id[HTS_CATALOG_REMOTE_ID_MAX + 1U];
  char remote_version[YOTSUBA_VERSION_MAX + 1U];
  char synchronized_version[YOTSUBA_VERSION_MAX + 1U];
  char lifecycle_state[65];
  char synchronized_state[65];
  unsigned int missing_count;
  int64_t last_seen_ms;
} y_known_resource;

typedef struct y_known_post {
  char remote_id[HTS_CATALOG_REMOTE_ID_MAX + 1U];
  int seen;
} y_known_post;

typedef struct y_board_info {
  char board[YOTSUBA_BOARD_MAX + 1U];
  int archives;
} y_board_info;

typedef struct y_batch_storage {
  hts_metadata_board_item *boards;
  hts_metadata_collection_item *collections;
  hts_metadata_post_item *posts;
  hts_metadata_media_item *media;
  hts_metadata_membership_item *memberships;
  hts_metadata_resource_item *resources;
  hts_metadata_lifecycle_change *changes;
  char **owned;
  size_t owned_count;
  size_t owned_capacity;
} y_batch_storage;

struct hts_yotsuba_adapter {
  hts_catalog *catalog;
  hts_yotsuba_options options;
  char **configured_boards;
  hts_metadata_adapter contract;
  y_endpoint endpoint;
  char current_board[YOTSUBA_BOARD_MAX + 1U];
  char current_thread[32];
  char expected_version[YOTSUBA_VERSION_MAX + 1U];
  char expected_state[65];
  uint64_t scan_marker_ms;
  int archive_supported;
  y_known_resource *known_resources;
  size_t known_resource_count;
  y_known_post *known_posts;
  size_t known_post_count;
  y_batch_storage *batch;
  size_t last_post_count;
  size_t last_media_count;
};

typedef struct y_resource_copy_context {
  y_known_resource *items;
  size_t count;
  size_t capacity;
  int failed;
} y_resource_copy_context;

typedef struct y_post_copy_context {
  y_known_post *items;
  size_t count;
  size_t capacity;
  int failed;
} y_post_copy_context;

typedef struct y_board_copy_context {
  const hts_yotsuba_adapter *adapter;
  y_board_info *items;
  size_t count;
  size_t capacity;
  int failed;
} y_board_copy_context;

static int y_copy(char *output, size_t output_size, const char *input) {
  size_t length;
  if (output == NULL || output_size == 0U || input == NULL) return 0;
  length = strlen(input);
  if (length >= output_size) return 0;
  (void) memcpy(output, input, length + 1U);
  return 1;
}

static int y_board_valid(const char *board) {
  const unsigned char *scan;
  size_t length;
  if (board == NULL) return 0;
  length = strlen(board);
  if (length == 0U || length > YOTSUBA_BOARD_MAX) return 0;
  for (scan = (const unsigned char *) board; *scan != '\0'; scan++) {
    if (!((*scan >= 'a' && *scan <= 'z') ||
          (*scan >= '0' && *scan <= '9'))) return 0;
  }
  return 1;
}

static int y_configured(const hts_yotsuba_adapter *adapter,
                        const char *board) {
  size_t i;
  if (adapter->options.discover_boards) return 1;
  for (i = 0U; i < adapter->options.board_count; i++) {
    if (strcmp(adapter->configured_boards[i], board) == 0) return 1;
  }
  return 0;
}

static int y_token_add(y_document *document, size_t capacity,
                       y_token_type type, size_t start, int parent,
                       size_t *out_index) {
  y_token *token;
  if (document->token_count >= capacity) return 0;
  token = &document->tokens[document->token_count];
  token->type = type;
  token->start = start;
  token->end = 0U;
  token->size = 0U;
  token->parent = parent;
  if (parent >= 0) document->tokens[parent].size++;
  *out_index = document->token_count++;
  return 1;
}

static int y_hex(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  return -1;
}

static int y_parse_document(const unsigned char *body, size_t body_size,
                            y_document *document) {
  size_t capacity;
  size_t position;
  int parent;
  if (body == NULL || document == NULL || body_size == 0U ||
      body_size > (SIZE_MAX - 32U) / 2U) return 0;
  (void) memset(document, 0, sizeof(*document));
  capacity = body_size / 2U + 32U;
  document->tokens = (y_token *) calloc(capacity, sizeof(y_token));
  if (document->tokens == NULL) return 0;
  document->json = (const char *) body;
  document->length = body_size;
  parent = -1;
  position = 0U;
  while (position < body_size) {
    unsigned char value;
    size_t index;
    value = body[position];
    if (value == ' ' || value == '\t' || value == '\r' || value == '\n' ||
        value == ':' || value == ',') {
      position++;
      continue;
    }
    if (value == '{' || value == '[') {
      if (!y_token_add(document, capacity,
                       value == '{' ? Y_TOKEN_OBJECT : Y_TOKEN_ARRAY,
                       position, parent, &index)) goto malformed;
      parent = (int) index;
      position++;
      continue;
    }
    if (value == '}' || value == ']') {
      y_token_type expected;
      if (parent < 0) goto malformed;
      expected = value == '}' ? Y_TOKEN_OBJECT : Y_TOKEN_ARRAY;
      if (document->tokens[parent].type != expected) goto malformed;
      document->tokens[parent].end = position + 1U;
      parent = document->tokens[parent].parent;
      position++;
      continue;
    }
    if (value == '"') {
      size_t start;
      int escaped;
      start = ++position;
      escaped = 0;
      while (position < body_size) {
        value = body[position];
        if (!escaped && value == '"') break;
        if (value < 0x20U) goto malformed;
        if (!escaped && value == '\\') {
          escaped = 1;
          position++;
          continue;
        }
        if (escaped) {
          if (value == 'u') {
            size_t j;
            if (position + 4U >= body_size) goto malformed;
            for (j = 1U; j <= 4U; j++) {
              if (y_hex((char) body[position + j]) < 0) goto malformed;
            }
            position += 4U;
          } else if (strchr("\"\\/bfnrt", (int) value) == NULL) {
            goto malformed;
          }
          escaped = 0;
        }
        position++;
      }
      if (position >= body_size || escaped) goto malformed;
      if (!y_token_add(document, capacity, Y_TOKEN_STRING, start, parent,
                       &index)) goto malformed;
      document->tokens[index].end = position;
      position++;
      continue;
    }
    {
      size_t start;
      start = position;
      while (position < body_size &&
             strchr(" \t\r\n,:]}", (int) body[position]) == NULL) {
        if (body[position] < 0x20U) goto malformed;
        position++;
      }
      if (position == start ||
          !y_token_add(document, capacity, Y_TOKEN_PRIMITIVE, start, parent,
                       &index)) goto malformed;
      document->tokens[index].end = position;
    }
  }
  if (parent >= 0 || document->token_count == 0U) goto malformed;
  {
    size_t roots;
    size_t i;
    roots = 0U;
    for (i = 0U; i < document->token_count; i++) {
      if (document->tokens[i].parent < 0) roots++;
    }
    if (roots != 1U) goto malformed;
  }
  return 1;

malformed:
  free(document->tokens);
  (void) memset(document, 0, sizeof(*document));
  return 0;
}

static void y_document_release(y_document *document) {
  if (document == NULL) return;
  free(document->tokens);
  (void) memset(document, 0, sizeof(*document));
}

static int y_token_key_equal(const y_document *document, size_t token_index,
                             const char *key) {
  const y_token *token;
  size_t length;
  token = &document->tokens[token_index];
  length = strlen(key);
  return token->type == Y_TOKEN_STRING && token->end - token->start == length &&
         memcmp(document->json + token->start, key, length) == 0;
}

static size_t y_token_skip(const y_document *document, size_t token_index) {
  size_t end;
  size_t index;
  end = document->tokens[token_index].end;
  index = token_index + 1U;
  while (index < document->token_count &&
         document->tokens[index].start < end) index++;
  return index;
}

static int y_object_get(const y_document *document, size_t object_index,
                        const char *key, size_t *value_index) {
  size_t index;
  const y_token *object;
  object = &document->tokens[object_index];
  if (object->type != Y_TOKEN_OBJECT) return 0;
  index = object_index + 1U;
  while (index < document->token_count &&
         document->tokens[index].start < object->end) {
    size_t value;
    if (document->tokens[index].parent != (int) object_index ||
        document->tokens[index].type != Y_TOKEN_STRING ||
        index + 1U >= document->token_count) return 0;
    value = index + 1U;
    if (document->tokens[value].parent != (int) object_index) return 0;
    if (y_token_key_equal(document, index, key)) {
      *value_index = value;
      return 1;
    }
    index = y_token_skip(document, value);
  }
  return 0;
}

static int y_token_i64(const y_document *document, size_t token_index,
                       int64_t *output) {
  const y_token *token;
  size_t index;
  int negative;
  uint64_t value;
  token = &document->tokens[token_index];
  if (token->type != Y_TOKEN_PRIMITIVE || token->start >= token->end)
    return 0;
  index = token->start;
  negative = 0;
  if (document->json[index] == '-') {
    negative = 1;
    index++;
  }
  if (index >= token->end) return 0;
  value = 0U;
  while (index < token->end) {
    unsigned int digit;
    if (document->json[index] < '0' || document->json[index] > '9') return 0;
    digit = (unsigned int) (document->json[index] - '0');
    if (value > (UINT64_MAX - digit) / 10U) return 0;
    value = value * 10U + digit;
    index++;
  }
  if ((!negative && value > (uint64_t) INT64_MAX) ||
      (negative && value > (uint64_t) INT64_MAX + 1U)) return 0;
  if (negative) {
    *output = value == (uint64_t) INT64_MAX + 1U
                ? INT64_MIN : -(int64_t) value;
  } else {
    *output = (int64_t) value;
  }
  return 1;
}

static uint32_t y_unicode_unit(const char *input) {
  uint32_t value;
  size_t i;
  value = 0U;
  for (i = 0U; i < 4U; i++) {
    value = value * 16U + (uint32_t) y_hex(input[i]);
  }
  return value;
}

static size_t y_utf8(char *output, uint32_t codepoint) {
  if (codepoint <= 0x7fU) {
    output[0] = (char) codepoint;
    return 1U;
  }
  if (codepoint <= 0x7ffU) {
    output[0] = (char) (0xc0U | (codepoint >> 6));
    output[1] = (char) (0x80U | (codepoint & 0x3fU));
    return 2U;
  }
  if (codepoint <= 0xffffU) {
    output[0] = (char) (0xe0U | (codepoint >> 12));
    output[1] = (char) (0x80U | ((codepoint >> 6) & 0x3fU));
    output[2] = (char) (0x80U | (codepoint & 0x3fU));
    return 3U;
  }
  output[0] = (char) (0xf0U | (codepoint >> 18));
  output[1] = (char) (0x80U | ((codepoint >> 12) & 0x3fU));
  output[2] = (char) (0x80U | ((codepoint >> 6) & 0x3fU));
  output[3] = (char) (0x80U | (codepoint & 0x3fU));
  return 4U;
}

static int y_batch_own(y_batch_storage *storage, char *value) {
  char **next;
  size_t capacity;
  if (value == NULL) return 0;
  if (storage->owned_count == storage->owned_capacity) {
    capacity = storage->owned_capacity == 0U
                 ? 32U : storage->owned_capacity * 2U;
    if (capacity < storage->owned_capacity) return 0;
    next = (char **) realloc(storage->owned, capacity * sizeof(char *));
    if (next == NULL) return 0;
    storage->owned = next;
    storage->owned_capacity = capacity;
  }
  storage->owned[storage->owned_count++] = value;
  return 1;
}

static char *y_owned_text(y_batch_storage *storage, const char *value) {
  char *copy;
  size_t length;
  if (value == NULL) return NULL;
  length = strlen(value);
  copy = (char *) malloc(length + 1U);
  if (copy == NULL) return NULL;
  (void) memcpy(copy, value, length + 1U);
  if (!y_batch_own(storage, copy)) {
    free(copy);
    return NULL;
  }
  return copy;
}

static char *y_owned_concat(y_batch_storage *storage,
                            const char *first, const char *second,
                            const char *third, const char *fourth,
                            const char *fifth) {
  size_t length;
  char *value;
  length = strlen(first) + strlen(second) + strlen(third) +
           strlen(fourth) + strlen(fifth);
  if (length > HTS_CATALOG_URL_MAX) return NULL;
  value = (char *) malloc(length + 1U);
  if (value == NULL) return NULL;
  (void) memcpy(value, first, strlen(first));
  (void) memcpy(value + strlen(first), second, strlen(second));
  (void) memcpy(value + strlen(first) + strlen(second), third,
                strlen(third));
  (void) memcpy(value + strlen(first) + strlen(second) + strlen(third),
                fourth, strlen(fourth));
  (void) memcpy(value + strlen(first) + strlen(second) + strlen(third) +
                strlen(fourth), fifth, strlen(fifth));
  value[length] = '\0';
  if (!y_batch_own(storage, value)) {
    free(value);
    return NULL;
  }
  return value;
}

static char *y_owned_u64(y_batch_storage *storage, uint64_t value) {
  char buffer[32];
  (void) snprintf(buffer, sizeof(buffer), "%llu",
                  (unsigned long long) value);
  return y_owned_text(storage, buffer);
}

static char *y_token_string(y_batch_storage *storage,
                            const y_document *document,
                            size_t token_index) {
  const y_token *token;
  char *output;
  size_t input_index;
  size_t output_index;
  token = &document->tokens[token_index];
  if (token->type != Y_TOKEN_STRING) return NULL;
  output = (char *) malloc(token->end - token->start + 1U);
  if (output == NULL) return NULL;
  input_index = token->start;
  output_index = 0U;
  while (input_index < token->end) {
    unsigned char value;
    value = (unsigned char) document->json[input_index++];
    if (value != '\\') {
      output[output_index++] = (char) value;
      continue;
    }
    value = (unsigned char) document->json[input_index++];
    if (value == '"' || value == '\\' || value == '/') {
      output[output_index++] = (char) value;
    } else if (value == 'b') {
      output[output_index++] = '\b';
    } else if (value == 'f') {
      output[output_index++] = '\f';
    } else if (value == 'n') {
      output[output_index++] = '\n';
    } else if (value == 'r') {
      output[output_index++] = '\r';
    } else if (value == 't') {
      output[output_index++] = '\t';
    } else if (value == 'u') {
      uint32_t codepoint;
      uint32_t low;
      codepoint = y_unicode_unit(document->json + input_index);
      input_index += 4U;
      if (codepoint >= 0xd800U && codepoint <= 0xdbffU) {
        if (input_index + 6U > token->end ||
            document->json[input_index] != '\\' ||
            document->json[input_index + 1U] != 'u') {
          free(output);
          return NULL;
        }
        low = y_unicode_unit(document->json + input_index + 2U);
        if (low < 0xdc00U || low > 0xdfffU) {
          free(output);
          return NULL;
        }
        input_index += 6U;
        codepoint = 0x10000U + ((codepoint - 0xd800U) << 10) +
                    (low - 0xdc00U);
      } else if (codepoint >= 0xdc00U && codepoint <= 0xdfffU) {
        free(output);
        return NULL;
      }
      output_index += y_utf8(output + output_index, codepoint);
    } else {
      free(output);
      return NULL;
    }
  }
  output[output_index] = '\0';
  if (!y_batch_own(storage, output)) {
    free(output);
    return NULL;
  }
  return output;
}

static int y_optional_i64(const y_document *document, size_t object,
                          const char *key, int64_t *value, int64_t fallback) {
  size_t token;
  *value = fallback;
  if (!y_object_get(document, object, key, &token)) return 0;
  if (!y_token_i64(document, token, value)) {
    *value = fallback;
    return -1;
  }
  return 1;
}

static char *y_optional_string(y_batch_storage *storage,
                               const y_document *document,
                               size_t object, const char *key) {
  size_t token;
  if (!y_object_get(document, object, key, &token) ||
      document->tokens[token].type != Y_TOKEN_STRING) return NULL;
  return y_token_string(storage, document, token);
}

static void y_batch_free(y_batch_storage *storage) {
  size_t i;
  if (storage == NULL) return;
  for (i = 0U; i < storage->owned_count; i++) free(storage->owned[i]);
  free(storage->owned);
  free(storage->boards);
  free(storage->collections);
  free(storage->posts);
  free(storage->media);
  free(storage->memberships);
  free(storage->resources);
  free(storage->changes);
  free(storage);
}

static y_batch_storage *y_batch_new(hts_yotsuba_adapter *adapter) {
  y_batch_storage *storage;
  y_batch_free(adapter->batch);
  adapter->batch = NULL;
  storage = (y_batch_storage *) calloc(1U, sizeof(*storage));
  if (storage != NULL) adapter->batch = storage;
  return storage;
}

static int y_make_resource_id(char *output, size_t output_size,
                              const char *board, const char *thread) {
  int written;
  written = snprintf(output, output_size, "%s/%s", board, thread);
  return written >= 0 && (size_t) written < output_size;
}

static int y_parse_resource_id(const char *value, char *board,
                               size_t board_size, char *thread,
                               size_t thread_size) {
  const char *slash;
  size_t board_length;
  size_t thread_length;
  const unsigned char *scan;
  slash = value != NULL ? strchr(value, '/') : NULL;
  if (slash == NULL || strchr(slash + 1, '/') != NULL) return 0;
  board_length = (size_t) (slash - value);
  thread_length = strlen(slash + 1);
  if (board_length == 0U || board_length >= board_size ||
      thread_length == 0U || thread_length >= thread_size) return 0;
  (void) memcpy(board, value, board_length);
  board[board_length] = '\0';
  if (!y_board_valid(board)) return 0;
  for (scan = (const unsigned char *) slash + 1; *scan != '\0'; scan++) {
    if (*scan < '0' || *scan > '9') return 0;
  }
  (void) memcpy(thread, slash + 1, thread_length + 1U);
  return 1;
}

static int y_resource_copy(void *opaque,
                           const hts_catalog_resource_view *resource) {
  y_resource_copy_context *context;
  y_known_resource *next;
  size_t capacity;
  y_known_resource *item;
  context = (y_resource_copy_context *) opaque;
  if (context->count == context->capacity) {
    capacity = context->capacity == 0U ? 32U : context->capacity * 2U;
    next = (y_known_resource *) realloc(
        context->items, capacity * sizeof(y_known_resource));
    if (next == NULL) {
      context->failed = 1;
      return 1;
    }
    context->items = next;
    context->capacity = capacity;
  }
  item = &context->items[context->count++];
  (void) memset(item, 0, sizeof(*item));
  if (!y_copy(item->resource_id, sizeof(item->resource_id),
              resource->resource_id) ||
      (resource->parent_remote_id != NULL &&
       !y_copy(item->parent_remote_id, sizeof(item->parent_remote_id),
               resource->parent_remote_id)) ||
      (resource->remote_version != NULL &&
       !y_copy(item->remote_version, sizeof(item->remote_version),
               resource->remote_version)) ||
      (resource->synchronized_version != NULL &&
       !y_copy(item->synchronized_version,
               sizeof(item->synchronized_version),
               resource->synchronized_version)) ||
      !y_copy(item->lifecycle_state, sizeof(item->lifecycle_state),
              resource->lifecycle_state) ||
      (resource->synchronized_state != NULL &&
       !y_copy(item->synchronized_state,
               sizeof(item->synchronized_state),
               resource->synchronized_state))) {
    context->failed = 1;
    return 1;
  }
  item->missing_count = resource->missing_count;
  item->last_seen_ms = resource->last_seen_ms;
  return 0;
}

static int y_load_resources(hts_yotsuba_adapter *adapter,
                            const char *board) {
  y_resource_copy_context context;
  int status;
  (void) memset(&context, 0, sizeof(context));
  status = hts_catalog_list_resource_states(
      adapter->catalog, adapter->options.source_id, "yotsuba-thread",
      board, y_resource_copy, &context);
  if (status != HTS_CATALOG_OK || context.failed) {
    free(context.items);
    return 0;
  }
  free(adapter->known_resources);
  adapter->known_resources = context.items;
  adapter->known_resource_count = context.count;
  return 1;
}

static int y_post_copy(void *opaque, const hts_catalog_entry *entry) {
  y_post_copy_context *context;
  y_known_post *next;
  size_t capacity;
  context = (y_post_copy_context *) opaque;
  if (context->count == context->capacity) {
    capacity = context->capacity == 0U ? 32U : context->capacity * 2U;
    next = (y_known_post *) realloc(
        context->items, capacity * sizeof(y_known_post));
    if (next == NULL) {
      context->failed = 1;
      return 1;
    }
    context->items = next;
    context->capacity = capacity;
  }
  (void) memset(&context->items[context->count], 0,
                sizeof(context->items[context->count]));
  if (!y_copy(context->items[context->count].remote_id,
              sizeof(context->items[context->count].remote_id),
              entry->remote_id)) {
    context->failed = 1;
    return 1;
  }
  context->count++;
  return 0;
}

static int y_load_posts(hts_yotsuba_adapter *adapter,
                        const char *collection_remote_id) {
  y_post_copy_context context;
  int64_t collection_id;
  int status;
  free(adapter->known_posts);
  adapter->known_posts = NULL;
  adapter->known_post_count = 0U;
  status = hts_catalog_find_id(adapter->catalog,
                               HTS_CATALOG_ENTITY_COLLECTION,
                               adapter->options.source_id, "thread",
                               collection_remote_id, &collection_id);
  if (status == HTS_CATALOG_NOT_FOUND) return 1;
  if (status != HTS_CATALOG_OK) return 0;
  (void) memset(&context, 0, sizeof(context));
  status = hts_catalog_list(adapter->catalog, HTS_CATALOG_ENTITY_POST,
                            collection_id, y_post_copy, &context);
  if (status != HTS_CATALOG_OK || context.failed) {
    free(context.items);
    return 0;
  }
  adapter->known_posts = context.items;
  adapter->known_post_count = context.count;
  return 1;
}

static hts_metadata_category y_parse_boards(
    hts_yotsuba_adapter *adapter, const hts_metadata_http_response *response,
    hts_metadata_batch *batch);
static hts_metadata_category y_parse_threads(
    hts_yotsuba_adapter *adapter, const hts_metadata_http_response *response,
    hts_metadata_batch *batch);
static hts_metadata_category y_parse_archive(
    hts_yotsuba_adapter *adapter, const hts_metadata_http_response *response,
    hts_metadata_batch *batch);
static hts_metadata_category y_parse_thread(
    hts_yotsuba_adapter *adapter, const hts_metadata_http_response *response,
    hts_metadata_batch *batch);

static int y_validate_source(void *opaque,
                             const hts_metadata_source_input *input,
                             hts_metadata_source *output) {
  (void) opaque;
  if (input == NULL || output == NULL || input->base_url == NULL ||
      (strcmp(input->base_url, HTS_YOTSUBA_API_ORIGIN) != 0 &&
       strcmp(input->base_url, HTS_YOTSUBA_API_ORIGIN "/") != 0) ||
      (input->configured_board != NULL &&
       input->configured_board[0] != '\0' &&
       !y_board_valid(input->configured_board)) ||
      input->credential_ref != NULL) return -1;
  if (!y_copy(output->canonical_base_url,
              sizeof(output->canonical_base_url),
              HTS_YOTSUBA_API_ORIGIN) ||
      !y_copy(output->origin, sizeof(output->origin),
              HTS_YOTSUBA_API_ORIGIN)) return -1;
  if (input->configured_board != NULL &&
      !y_copy(output->configured_board, sizeof(output->configured_board),
              input->configured_board)) return -1;
  return 0;
}

static int y_endpoint_board(const char *resource_id, const char *suffix,
                            char *board, size_t board_size) {
  size_t resource_length;
  size_t suffix_length;
  size_t board_length;
  resource_length = strlen(resource_id);
  suffix_length = strlen(suffix);
  if (resource_length <= suffix_length ||
      strcmp(resource_id + resource_length - suffix_length, suffix) != 0)
    return 0;
  board_length = resource_length - suffix_length;
  if (board_length == 0U || board_length >= board_size) return 0;
  (void) memcpy(board, resource_id, board_length);
  board[board_length] = '\0';
  return y_board_valid(board);
}

static int y_thread_endpoint(const char *resource_id, char *board,
                             size_t board_size, char *thread,
                             size_t thread_size) {
  const char *marker;
  const unsigned char *scan;
  size_t board_length;
  size_t thread_length;
  marker = strstr(resource_id, "/thread/");
  if (marker == NULL || strstr(marker + 8, "/") != NULL) return 0;
  board_length = (size_t) (marker - resource_id);
  thread_length = strlen(marker + 8);
  if (board_length == 0U || board_length >= board_size ||
      thread_length == 0U || thread_length >= thread_size) return 0;
  (void) memcpy(board, resource_id, board_length);
  board[board_length] = '\0';
  if (!y_board_valid(board)) return 0;
  for (scan = (const unsigned char *) marker + 8; *scan != '\0'; scan++) {
    if (*scan < '0' || *scan > '9') return 0;
  }
  (void) memcpy(thread, marker + 8, thread_length + 1U);
  return 1;
}

static int y_prepare_request(void *opaque,
                             const hts_metadata_source *source,
                             hts_metadata_operation operation,
                             const char *resource_id,
                             const char *opaque_cursor,
                             hts_metadata_http_request *request) {
  hts_yotsuba_adapter *adapter;
  int written;
  (void) opaque_cursor;
  adapter = (hts_yotsuba_adapter *) opaque;
  adapter->endpoint = Y_ENDPOINT_NONE;
  adapter->current_board[0] = '\0';
  adapter->current_thread[0] = '\0';
  if (operation == HTS_METADATA_DISCOVER_BOARDS &&
      strcmp(resource_id, "boards") == 0) {
    adapter->endpoint = Y_ENDPOINT_BOARDS;
    written = snprintf(request->url, sizeof(request->url),
                       "%s/boards.json", source->canonical_base_url);
  } else if (operation == HTS_METADATA_ENUMERATE_COLLECTIONS &&
             y_endpoint_board(resource_id, "/threads",
                              adapter->current_board,
                              sizeof(adapter->current_board))) {
    adapter->endpoint = Y_ENDPOINT_THREADS;
    written = snprintf(request->url, sizeof(request->url), "%s/%s/threads.json",
                       source->canonical_base_url, adapter->current_board);
  } else if (operation == HTS_METADATA_ENUMERATE_COLLECTIONS &&
             y_endpoint_board(resource_id, "/archive",
                              adapter->current_board,
                              sizeof(adapter->current_board))) {
    adapter->endpoint = Y_ENDPOINT_ARCHIVE;
    written = snprintf(request->url, sizeof(request->url), "%s/%s/archive.json",
                       source->canonical_base_url, adapter->current_board);
  } else if (operation == HTS_METADATA_ENUMERATE_POSTS &&
             y_thread_endpoint(resource_id, adapter->current_board,
                               sizeof(adapter->current_board),
                               adapter->current_thread,
                               sizeof(adapter->current_thread))) {
    adapter->endpoint = Y_ENDPOINT_THREAD;
    written = snprintf(request->url, sizeof(request->url),
                       "%s/%s/thread/%s.json", source->canonical_base_url,
                       adapter->current_board, adapter->current_thread);
  } else {
    return -1;
  }
  return written < 0 || (size_t) written >= sizeof(request->url) ? -1 : 0;
}

static hts_metadata_category y_parse_response(
    void *opaque, hts_metadata_operation operation,
    const hts_metadata_http_response *response, hts_metadata_batch *batch) {
  hts_yotsuba_adapter *adapter;
  (void) operation;
  adapter = (hts_yotsuba_adapter *) opaque;
  if (adapter == NULL || response == NULL || batch == NULL)
    return HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
  switch (adapter->endpoint) {
    case Y_ENDPOINT_BOARDS:
      return y_parse_boards(adapter, response, batch);
    case Y_ENDPOINT_THREADS:
      return y_parse_threads(adapter, response, batch);
    case Y_ENDPOINT_ARCHIVE:
      return y_parse_archive(adapter, response, batch);
    case Y_ENDPOINT_THREAD:
      return y_parse_thread(adapter, response, batch);
    default:
      return HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
  }
}

static void y_release_batch(void *opaque, hts_metadata_batch *batch) {
  hts_yotsuba_adapter *adapter;
  (void) batch;
  adapter = (hts_yotsuba_adapter *) opaque;
  y_batch_free(adapter->batch);
  adapter->batch = NULL;
}

void hts_yotsuba_default_policy(hts_metadata_policy *policy) {
  if (policy == NULL) return;
  (void) memset(policy, 0, sizeof(*policy));
  policy->requests_per_minute = 30U;
  policy->maximum_concurrency = 1U;
  policy->minimum_refresh_interval_ms = 30000U;
  policy->base_backoff_ms = 2000U;
  policy->maximum_backoff_ms = 120000U;
  policy->maximum_attempts = 4U;
  policy->maximum_body_bytes = 16U * 1024U * 1024U;
  policy->redirect_limit = 3U;
  policy->connect_timeout_ms = 10000U;
  policy->request_timeout_ms = 30000U;
  policy->jitter_percent = 20U;
  policy->jitter_seed = 0x594f5453U;
  policy->user_agent = HTS_YOTSUBA_DEFAULT_USER_AGENT;
}

int hts_yotsuba_adapter_create(
    hts_catalog *catalog, const hts_yotsuba_options *options,
    hts_yotsuba_adapter **out_adapter) {
  hts_yotsuba_adapter *adapter;
  size_t i;
  hts_metadata_policy defaults;
  if (catalog == NULL || options == NULL || out_adapter == NULL ||
      options->source_id <= 0 ||
      (!options->discover_boards && options->board_count == 0U) ||
      options->board_count > YOTSUBA_MAX_BOARDS ||
      (options->board_count != 0U && options->boards == NULL)) return 0;
  *out_adapter = NULL;
  adapter = (hts_yotsuba_adapter *) calloc(1U, sizeof(*adapter));
  if (adapter == NULL) return 0;
  adapter->catalog = catalog;
  adapter->options = *options;
  if (adapter->options.missing_confirmations == 0U)
    adapter->options.missing_confirmations = 2U;
  if (adapter->options.missing_confirmations < 2U ||
      adapter->options.missing_confirmations > 100U) goto failure;
  if (adapter->options.policy.requests_per_minute == 0U) {
    hts_yotsuba_default_policy(&defaults);
    adapter->options.policy = defaults;
  }
  if (!hts_metadata_policy_validate(&adapter->options.policy) ||
      adapter->options.policy.requests_per_minute > 60U ||
      adapter->options.policy.maximum_concurrency != 1U ||
      adapter->options.policy.minimum_refresh_interval_ms < 10000U)
    goto failure;
  if (options->board_count != 0U) {
    adapter->configured_boards =
        (char **) calloc(options->board_count, sizeof(char *));
    if (adapter->configured_boards == NULL) goto failure;
  }
  for (i = 0U; i < options->board_count; i++) {
    size_t length;
    if (!y_board_valid(options->boards[i])) goto failure;
    length = strlen(options->boards[i]);
    adapter->configured_boards[i] = (char *) malloc(length + 1U);
    if (adapter->configured_boards[i] == NULL) goto failure;
    (void) memcpy(adapter->configured_boards[i], options->boards[i],
                  length + 1U);
  }
  adapter->contract.identifier = "yotsuba";
  adapter->contract.adapter_version = HTS_YOTSUBA_ADAPTER_VERSION;
  adapter->contract.cursor_version = HTS_YOTSUBA_CURSOR_VERSION;
  adapter->contract.capabilities =
      HTS_METADATA_CAP_THREADS | HTS_METADATA_CAP_INCREMENTAL_CHANGES |
      HTS_METADATA_CAP_CONDITIONAL_REQUESTS |
      HTS_METADATA_CAP_PRESERVATION_ELIGIBLE;
  adapter->contract.context = adapter;
  adapter->contract.validate_and_canonicalize = y_validate_source;
  adapter->contract.prepare_request = y_prepare_request;
  adapter->contract.parse_response = y_parse_response;
  adapter->contract.release_batch = y_release_batch;
  *out_adapter = adapter;
  return 1;

failure:
  hts_yotsuba_adapter_destroy(adapter);
  return 0;
}

void hts_yotsuba_adapter_destroy(hts_yotsuba_adapter *adapter) {
  size_t i;
  if (adapter == NULL) return;
  y_batch_free(adapter->batch);
  for (i = 0U; i < adapter->options.board_count; i++)
    free(adapter->configured_boards != NULL
           ? adapter->configured_boards[i] : NULL);
  free(adapter->configured_boards);
  free(adapter->known_resources);
  free(adapter->known_posts);
  free(adapter);
}

const hts_metadata_adapter *hts_yotsuba_adapter_contract(
    hts_yotsuba_adapter *adapter) {
  return adapter != NULL ? &adapter->contract : NULL;
}

static int y_array_next(const y_document *document, size_t array_index,
                        size_t *item_index) {
  size_t index;
  const y_token *array;
  array = &document->tokens[array_index];
  if (array->type != Y_TOKEN_ARRAY) return 0;
  index = *item_index;
  if (index <= array_index) index = array_index + 1U;
  while (index < document->token_count &&
         document->tokens[index].start < array->end &&
         document->tokens[index].parent != (int) array_index) {
    index++;
  }
  if (index >= document->token_count ||
      document->tokens[index].start >= array->end) return 0;
  *item_index = index;
  return 1;
}

static int y_batch_arrays(y_batch_storage *storage, size_t boards,
                          size_t collections, size_t posts, size_t media,
                          size_t memberships, size_t resources,
                          size_t changes) {
  if (boards != 0U) {
    storage->boards = (hts_metadata_board_item *)
        calloc(boards, sizeof(*storage->boards));
    if (storage->boards == NULL) return 0;
  }
  if (collections != 0U) {
    storage->collections = (hts_metadata_collection_item *)
        calloc(collections, sizeof(*storage->collections));
    if (storage->collections == NULL) return 0;
  }
  if (posts != 0U) {
    storage->posts = (hts_metadata_post_item *)
        calloc(posts, sizeof(*storage->posts));
    if (storage->posts == NULL) return 0;
  }
  if (media != 0U) {
    storage->media = (hts_metadata_media_item *)
        calloc(media, sizeof(*storage->media));
    if (storage->media == NULL) return 0;
  }
  if (memberships != 0U) {
    storage->memberships = (hts_metadata_membership_item *)
        calloc(memberships, sizeof(*storage->memberships));
    if (storage->memberships == NULL) return 0;
  }
  if (resources != 0U) {
    storage->resources = (hts_metadata_resource_item *)
        calloc(resources, sizeof(*storage->resources));
    if (storage->resources == NULL) return 0;
  }
  if (changes != 0U) {
    storage->changes = (hts_metadata_lifecycle_change *)
        calloc(changes, sizeof(*storage->changes));
    if (storage->changes == NULL) return 0;
  }
  return 1;
}

static hts_metadata_category y_parse_boards(
    hts_yotsuba_adapter *adapter, const hts_metadata_http_response *response,
    hts_metadata_batch *batch) {
  y_document document;
  y_batch_storage *storage;
  size_t boards_token;
  size_t item;
  size_t output_count;
  unsigned char *found;
  hts_metadata_category result;
  size_t i;
  (void) memset(batch, 0, sizeof(*batch));
  if (!y_parse_document(response->body, response->body_size, &document))
    return HTS_METADATA_MALFORMED;
  storage = y_batch_new(adapter);
  if (storage == NULL) {
    y_document_release(&document);
    return HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
  }
  found = adapter->options.board_count != 0U
            ? (unsigned char *) calloc(adapter->options.board_count, 1U) : NULL;
  result = HTS_METADATA_MALFORMED;
  if ((adapter->options.board_count != 0U && found == NULL) ||
      document.tokens[0].type != Y_TOKEN_OBJECT ||
      !y_object_get(&document, 0U, "boards", &boards_token) ||
      document.tokens[boards_token].type != Y_TOKEN_ARRAY ||
      document.tokens[boards_token].size > YOTSUBA_MAX_BOARDS ||
      !y_batch_arrays(storage, document.tokens[boards_token].size,
                      0U, 0U, 0U, 0U, 0U, 0U)) goto done;
  item = boards_token + 1U;
  output_count = 0U;
  while (y_array_next(&document, boards_token, &item)) {
    size_t board_token;
    size_t title_token;
    char *board_name;
    char *title;
    int64_t work_safe;
    int64_t archives;
    char capabilities[128];
    char metadata[256];
    char seen[32];
    hts_catalog_board *record;
    if (document.tokens[item].type != Y_TOKEN_OBJECT ||
        !y_object_get(&document, item, "board", &board_token) ||
        !y_object_get(&document, item, "title", &title_token)) goto done;
    board_name = y_token_string(storage, &document, board_token);
    title = y_token_string(storage, &document, title_token);
    if (board_name == NULL || title == NULL || !y_board_valid(board_name))
      goto done;
    for (i = 0U; i < adapter->options.board_count; i++) {
      if (strcmp(adapter->configured_boards[i], board_name) == 0) found[i] = 1U;
    }
    if (!y_configured(adapter, board_name)) {
      item = y_token_skip(&document, item);
      continue;
    }
    (void) y_optional_i64(&document, item, "ws_board", &work_safe, 0);
    (void) y_optional_i64(&document, item, "is_archived", &archives, 0);
    (void) snprintf(capabilities, sizeof(capabilities),
                    "{\"threads\":true,\"archives\":%s}",
                    archives == 1 ? "true" : "false");
    (void) snprintf(metadata, sizeof(metadata),
                    "{\"source\":\"4chan\",\"canonical_url\":\"%s\","
                    "\"work_safe\":%s,\"archives\":%s}",
                    HTS_YOTSUBA_API_ORIGIN,
                    work_safe == 1 ? "true" : "false",
                    archives == 1 ? "true" : "false");
    (void) snprintf(seen, sizeof(seen), "%llu",
                    (unsigned long long) adapter->scan_marker_ms);
    record = &storage->boards[output_count].record;
    record->remote_id = board_name;
    record->name = board_name;
    record->display_name = title;
    record->capabilities_json = y_owned_text(storage, capabilities);
    record->last_seen_at = y_owned_text(storage, seen);
    record->metadata_json = y_owned_text(storage, metadata);
    if (record->capabilities_json == NULL || record->last_seen_at == NULL ||
        record->metadata_json == NULL) {
      result = HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
      goto done;
    }
    output_count++;
    item = y_token_skip(&document, item);
  }
  for (i = 0U; i < adapter->options.board_count; i++) {
    if (found[i] == 0U) {
      result = HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
      goto done;
    }
  }
  batch->boards = storage->boards;
  batch->board_count = output_count;
  batch->next_cursor = y_owned_u64(storage, adapter->scan_marker_ms);
  if (batch->next_cursor == NULL) {
    result = HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
    goto done;
  }
  result = HTS_METADATA_SUCCESS;

done:
  free(found);
  y_document_release(&document);
  return result;
}

static int y_board_copy(void *opaque, const hts_catalog_entry *entry) {
  y_board_copy_context *context;
  y_board_info *next;
  y_board_info *item;
  size_t capacity;
  context = (y_board_copy_context *) opaque;
  if (entry == NULL || entry->remote_id == NULL ||
      !y_configured(context->adapter, entry->remote_id)) return 0;
  if (context->count == context->capacity) {
    capacity = context->capacity == 0U ? 16U : context->capacity * 2U;
    next = (y_board_info *) realloc(context->items,
                                     capacity * sizeof(y_board_info));
    if (next == NULL) {
      context->failed = 1;
      return 1;
    }
    context->items = next;
    context->capacity = capacity;
  }
  item = &context->items[context->count++];
  (void) memset(item, 0, sizeof(*item));
  if (!y_copy(item->board, sizeof(item->board), entry->remote_id)) {
    context->failed = 1;
    return 1;
  }
  item->archives = entry->metadata_json != NULL &&
                   strstr(entry->metadata_json, "\"archives\":true") != NULL;
  return 0;
}

static int y_load_boards(hts_yotsuba_adapter *adapter,
                         y_board_info **boards, size_t *board_count) {
  y_board_copy_context context;
  int status;
  (void) memset(&context, 0, sizeof(context));
  context.adapter = adapter;
  status = hts_catalog_list(adapter->catalog, HTS_CATALOG_ENTITY_BOARD,
                            adapter->options.source_id, y_board_copy, &context);
  if (status != HTS_CATALOG_OK || context.failed) {
    free(context.items);
    return 0;
  }
  *boards = context.items;
  *board_count = context.count;
  return 1;
}

static int y_persist_thread_state(hts_yotsuba_adapter *adapter,
                                  const y_known_resource *known,
                                  const char *lifecycle_state,
                                  const char *synchronized_version,
                                  const char *synchronized_state,
                                  unsigned int missing_count,
                                  int64_t last_checked_ms,
                                  const char *error_state,
                                  const char *media_state) {
  hts_catalog_resource_state resource;
  int status;
  (void) memset(&resource, 0, sizeof(resource));
  resource.source_id = adapter->options.source_id;
  resource.resource_kind = "yotsuba-thread";
  resource.resource_id = known->resource_id;
  resource.parent_remote_id = known->parent_remote_id;
  resource.remote_version = known->remote_version[0] != '\0'
                              ? known->remote_version : NULL;
  resource.synchronized_version = synchronized_version;
  resource.lifecycle_state = lifecycle_state;
  resource.synchronized_state = synchronized_state;
  resource.missing_count = missing_count;
  resource.last_seen_ms = known->last_seen_ms;
  resource.last_checked_ms = last_checked_ms;
  resource.error_state = error_state;
  if (hts_catalog_begin(adapter->catalog) != HTS_CATALOG_OK) return 0;
  status = hts_catalog_upsert_resource_state(adapter->catalog, &resource, NULL);
  if (status == HTS_CATALOG_OK && media_state != NULL) {
    status = hts_catalog_set_collection_lifecycle(
        adapter->catalog, adapter->options.source_id, "thread",
        known->resource_id, lifecycle_state, media_state);
    if (status == HTS_CATALOG_NOT_FOUND) status = HTS_CATALOG_OK;
  }
  if (status == HTS_CATALOG_OK &&
      hts_catalog_commit(adapter->catalog) == HTS_CATALOG_OK) return 1;
  (void) hts_catalog_rollback(adapter->catalog);
  return 0;
}

static hts_metadata_category y_run_request(
    hts_yotsuba_adapter *adapter, hts_metadata_runtime *runtime,
    hts_metadata_operation operation, const char *resource_kind,
    const char *resource_id, const char *board,
    hts_metadata_sync_result *sync_result) {
  hts_metadata_sync_request request;
  (void) memset(&request, 0, sizeof(request));
  request.source_id = adapter->options.source_id;
  request.operation = operation;
  request.resource_kind = resource_kind;
  request.resource_id = resource_id;
  request.source.base_url = HTS_YOTSUBA_API_ORIGIN;
  request.source.configured_board = board;
  request.policy = adapter->options.policy;
  request.cancel = adapter->options.cancel;
  return hts_metadata_runtime_sync(runtime, &request, sync_result);
}

static int y_thread_needs_fetch(const y_known_resource *resource) {
  if (strcmp(resource->lifecycle_state, "active") != 0 &&
      strcmp(resource->lifecycle_state, "archived") != 0) return 0;
  return resource->synchronized_version[0] == '\0' ||
         strcmp(resource->remote_version,
                resource->synchronized_version) != 0 ||
         resource->synchronized_state[0] == '\0' ||
         strcmp(resource->lifecycle_state,
                resource->synchronized_state) != 0;
}

static hts_metadata_category y_handle_thread_result(
    hts_yotsuba_adapter *adapter, const y_known_resource *known,
    hts_metadata_category category, uint64_t now,
    hts_yotsuba_sync_result *result) {
  const char *state;
  const char *media_state;
  unsigned int missing_count;
  if (category == HTS_METADATA_SUCCESS) return category;
  if (category == HTS_METADATA_NOT_MODIFIED) {
    media_state = strcmp(known->lifecycle_state, "archived") == 0
                    ? "archived" : "available";
    if (!y_persist_thread_state(adapter, known, known->lifecycle_state,
                                known->remote_version,
                                known->lifecycle_state, 0U,
                                (int64_t) now, NULL, media_state))
      return HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
    result->threads_not_modified++;
    return HTS_METADATA_SUCCESS;
  }
  if (category == HTS_METADATA_GONE_OR_NOT_FOUND) {
    missing_count = known->missing_count + 1U;
    state = missing_count >= adapter->options.missing_confirmations
              ? "expired" : "missing";
    media_state = strcmp(state, "expired") == 0
                    ? "expired" : "temporarily_unavailable";
    if (!y_persist_thread_state(adapter, known, state,
                                known->synchronized_version[0] != '\0'
                                  ? known->synchronized_version : NULL,
                                known->synchronized_state[0] != '\0'
                                  ? known->synchronized_state : NULL,
                                missing_count, (int64_t) now,
                                "gone_or_not_found", media_state))
      return HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
    result->threads_missing++;
    return HTS_METADATA_SUCCESS;
  }
  if (category == HTS_METADATA_TEMPORARILY_UNAVAILABLE ||
      category == HTS_METADATA_RATE_LIMITED) {
    if (!y_persist_thread_state(adapter, known, known->lifecycle_state,
                                known->synchronized_version[0] != '\0'
                                  ? known->synchronized_version : NULL,
                                known->synchronized_state[0] != '\0'
                                  ? known->synchronized_state : NULL,
                                known->missing_count, (int64_t) now,
                                category == HTS_METADATA_RATE_LIMITED
                                  ? "rate_limited"
                                  : "transiently_unavailable", NULL))
      return HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
  }
  return category;
}

hts_metadata_category hts_yotsuba_sync(
    hts_yotsuba_adapter *adapter, const hts_metadata_transport *transport,
    const hts_metadata_clock *clock, hts_metadata_report_fn report,
    void *report_context, hts_yotsuba_sync_result *result) {
  hts_metadata_runtime *runtime;
  hts_metadata_sync_result sync_result;
  hts_metadata_category category;
  y_board_info *boards;
  size_t board_count;
  size_t board_index;
  if (result != NULL) (void) memset(result, 0, sizeof(*result));
  if (adapter == NULL || transport == NULL || clock == NULL ||
      clock->now_ms == NULL || clock->sleep_ms == NULL || result == NULL)
    return HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
  result->category = HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
  if (!hts_metadata_runtime_create(adapter->catalog, &adapter->contract,
                                   transport, clock, report, report_context,
                                   &runtime)) return result->category;
  boards = NULL;
  board_count = 0U;
  adapter->scan_marker_ms = clock->now_ms(clock->context);
  category = y_run_request(adapter, runtime, HTS_METADATA_DISCOVER_BOARDS,
                           "yotsuba-boards", "boards", NULL, &sync_result);
  result->requests_made += sync_result.requests_made;
  if (category != HTS_METADATA_SUCCESS &&
      category != HTS_METADATA_NOT_MODIFIED) goto complete;
  if (!y_load_boards(adapter, &boards, &board_count) || board_count == 0U) {
    category = HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
    goto complete;
  }
  result->boards_processed = board_count;
  for (board_index = 0U; board_index < board_count; board_index++) {
    char resource_id[HTS_CATALOG_REMOTE_ID_MAX + 1U];
    size_t resource_index;
    int length;
    adapter->scan_marker_ms = clock->now_ms(clock->context);
    adapter->archive_supported = boards[board_index].archives;
    if (!y_load_resources(adapter, boards[board_index].board)) {
      category = HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
      goto complete;
    }
    length = snprintf(resource_id, sizeof(resource_id), "%s/threads",
                      boards[board_index].board);
    if (length < 0 || (size_t) length >= sizeof(resource_id)) {
      category = HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
      goto complete;
    }
    category = y_run_request(adapter, runtime,
                             HTS_METADATA_ENUMERATE_COLLECTIONS,
                             "yotsuba-thread-list", resource_id,
                             boards[board_index].board, &sync_result);
    result->requests_made += sync_result.requests_made;
    if (category != HTS_METADATA_SUCCESS &&
        category != HTS_METADATA_NOT_MODIFIED) goto complete;
    if (!y_load_resources(adapter, boards[board_index].board)) {
      category = HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
      goto complete;
    }
    if (boards[board_index].archives) {
      length = snprintf(resource_id, sizeof(resource_id), "%s/archive",
                        boards[board_index].board);
      if (length < 0 || (size_t) length >= sizeof(resource_id)) {
        category = HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
        goto complete;
      }
      category = y_run_request(adapter, runtime,
                               HTS_METADATA_ENUMERATE_COLLECTIONS,
                               "yotsuba-archive", resource_id,
                               boards[board_index].board, &sync_result);
      result->requests_made += sync_result.requests_made;
      if (category != HTS_METADATA_SUCCESS &&
          category != HTS_METADATA_NOT_MODIFIED) goto complete;
      if (!y_load_resources(adapter, boards[board_index].board)) {
        category = HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
        goto complete;
      }
    }
    for (resource_index = 0U;
         resource_index < adapter->known_resource_count; resource_index++) {
      y_known_resource known;
      char thread_board[YOTSUBA_BOARD_MAX + 1U];
      char thread[32];
      char thread_endpoint[HTS_CATALOG_REMOTE_ID_MAX + 1U];
      uint64_t now;
      known = adapter->known_resources[resource_index];
      if (strcmp(known.lifecycle_state, "missing") == 0 ||
          strcmp(known.lifecycle_state, "expired") == 0)
        result->threads_missing++;
      else
        result->threads_discovered++;
      if (!y_thread_needs_fetch(&known)) continue;
      if (!y_parse_resource_id(known.resource_id, thread_board,
                               sizeof(thread_board), thread,
                               sizeof(thread)) ||
          strcmp(thread_board, boards[board_index].board) != 0 ||
          !y_copy(adapter->expected_version,
                  sizeof(adapter->expected_version), known.remote_version) ||
          !y_copy(adapter->expected_state,
                  sizeof(adapter->expected_state), known.lifecycle_state) ||
          !y_load_posts(adapter, known.resource_id)) {
        category = HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
        goto complete;
      }
      length = snprintf(thread_endpoint, sizeof(thread_endpoint),
                        "%s/thread/%s", thread_board, thread);
      if (length < 0 || (size_t) length >= sizeof(thread_endpoint)) {
        category = HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
        goto complete;
      }
      category = y_run_request(adapter, runtime,
                               HTS_METADATA_ENUMERATE_POSTS,
                               "yotsuba-thread", thread_endpoint,
                               boards[board_index].board, &sync_result);
      result->requests_made += sync_result.requests_made;
      now = clock->now_ms(clock->context);
      if (category == HTS_METADATA_SUCCESS && !sync_result.skipped_refresh) {
        result->threads_fetched++;
        result->posts_committed += adapter->last_post_count;
        result->media_variants_committed += adapter->last_media_count;
      } else {
        category = y_handle_thread_result(adapter, &known, category, now,
                                          result);
      }
      if (category != HTS_METADATA_SUCCESS) goto complete;
    }
  }
  category = HTS_METADATA_SUCCESS;

complete:
  free(boards);
  hts_metadata_runtime_destroy(runtime);
  result->category = category;
  return category;
}

static const char *y_mime_type(const char *extension) {
  if (strcmp(extension, ".jpg") == 0 || strcmp(extension, ".jpeg") == 0)
    return "image/jpeg";
  if (strcmp(extension, ".png") == 0) return "image/png";
  if (strcmp(extension, ".gif") == 0) return "image/gif";
  if (strcmp(extension, ".webm") == 0) return "video/webm";
  if (strcmp(extension, ".pdf") == 0) return "application/pdf";
  if (strcmp(extension, ".swf") == 0)
    return "application/x-shockwave-flash";
  return "application/octet-stream";
}

static int y_extension_valid(const char *extension) {
  const unsigned char *scan;
  size_t length;
  if (extension == NULL || extension[0] != '.') return 0;
  length = strlen(extension);
  if (length < 2U || length > 16U) return 0;
  for (scan = (const unsigned char *) extension + 1; *scan != '\0'; scan++) {
    if (!isalnum(*scan)) return 0;
  }
  return 1;
}

static int y_dimension(int64_t value) {
  return value < 0 || value > INT_MAX ? -1 : (int) value;
}

static int y_mark_known_post(hts_yotsuba_adapter *adapter,
                             const char *remote_id) {
  size_t i;
  for (i = 0U; i < adapter->known_post_count; i++) {
    if (strcmp(adapter->known_posts[i].remote_id, remote_id) == 0) {
      adapter->known_posts[i].seen = 1;
      return 1;
    }
  }
  return 0;
}

static hts_metadata_category y_parse_thread(
    hts_yotsuba_adapter *adapter, const hts_metadata_http_response *response,
    hts_metadata_batch *batch) {
  y_document document;
  y_batch_storage *storage;
  size_t posts_token;
  size_t item;
  size_t post_count;
  size_t media_count;
  size_t membership_count;
  size_t change_count;
  size_t post_position;
  size_t i;
  char collection_id[HTS_CATALOG_REMOTE_ID_MAX + 1U];
  char *collection_remote;
  const char *thread_state;
  int64_t op_archived;
  int64_t op_closed;
  int64_t op_sticky;
  char *op_subject;
  char *op_comment;
  char *op_created;
  char collection_metadata[640];
  hts_metadata_category result;
  (void) memset(batch, 0, sizeof(*batch));
  adapter->last_post_count = 0U;
  adapter->last_media_count = 0U;
  if (!y_parse_document(response->body, response->body_size, &document))
    return HTS_METADATA_MALFORMED;
  storage = y_batch_new(adapter);
  if (storage == NULL) {
    y_document_release(&document);
    return HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
  }
  result = HTS_METADATA_MALFORMED;
  if (document.tokens[0].type != Y_TOKEN_OBJECT ||
      !y_object_get(&document, 0U, "posts", &posts_token) ||
      document.tokens[posts_token].type != Y_TOKEN_ARRAY ||
      document.tokens[posts_token].size == 0U ||
      document.tokens[posts_token].size > YOTSUBA_MAX_POSTS ||
      !y_make_resource_id(collection_id, sizeof(collection_id),
                          adapter->current_board,
                          adapter->current_thread) ||
      !y_batch_arrays(storage, 0U, 1U,
                      document.tokens[posts_token].size,
                      document.tokens[posts_token].size * 2U,
                      document.tokens[posts_token].size * 2U,
                      1U, adapter->known_post_count)) goto done;
  collection_remote = y_owned_text(storage, collection_id);
  if (collection_remote == NULL) {
    result = HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
    goto done;
  }
  for (i = 0U; i < adapter->known_post_count; i++)
    adapter->known_posts[i].seen = 0;
  post_count = 0U;
  media_count = 0U;
  membership_count = 0U;
  change_count = 0U;
  post_position = 0U;
  op_archived = 0;
  op_closed = 0;
  op_sticky = 0;
  op_subject = NULL;
  op_comment = NULL;
  op_created = NULL;
  item = posts_token + 1U;
  while (y_array_next(&document, posts_token, &item)) {
    size_t no_token;
    size_t resto_token;
    size_t time_token;
    int64_t number;
    int64_t resto;
    int64_t created;
    char number_text[32];
    char parent_text[32];
    char remote_id[HTS_CATALOG_REMOTE_ID_MAX + 1U];
    char parent_id[HTS_CATALOG_REMOTE_ID_MAX + 1U];
    char *subject;
    char *comment;
    char *created_text;
    int64_t file_deleted;
    int64_t spoiler;
    int64_t tim;
    int64_t size_bytes;
    int64_t width;
    int64_t height;
    int64_t thumb_width;
    int64_t thumb_height;
    char *extension;
    char *filename;
    char *md5;
    char post_metadata[640];
    hts_metadata_post_item *post_item;
    if (document.tokens[item].type != Y_TOKEN_OBJECT ||
        !y_object_get(&document, item, "no", &no_token) ||
        !y_object_get(&document, item, "resto", &resto_token) ||
        !y_object_get(&document, item, "time", &time_token) ||
        !y_token_i64(&document, no_token, &number) || number <= 0 ||
        !y_token_i64(&document, resto_token, &resto) || resto < 0 ||
        !y_token_i64(&document, time_token, &created) || created <= 0)
      goto done;
    if (post_position == 0U && resto != 0) goto done;
    (void) snprintf(number_text, sizeof(number_text), "%lld",
                    (long long) number);
    if (post_position == 0U &&
        strcmp(number_text, adapter->current_thread) != 0) goto done;
    if (!y_make_resource_id(remote_id, sizeof(remote_id),
                            adapter->current_board, number_text)) goto done;
    parent_id[0] = '\0';
    if (resto != 0) {
      (void) snprintf(parent_text, sizeof(parent_text), "%lld",
                      (long long) resto);
      if (!y_make_resource_id(parent_id, sizeof(parent_id),
                              adapter->current_board, parent_text)) goto done;
    }
    subject = y_optional_string(storage, &document, item, "sub");
    comment = y_optional_string(storage, &document, item, "com");
    created_text = y_owned_u64(storage, (uint64_t) created);
    if (created_text == NULL) {
      result = HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
      goto done;
    }
    (void) y_optional_i64(&document, item, "filedeleted",
                          &file_deleted, 0);
    (void) y_optional_i64(&document, item, "spoiler", &spoiler, 0);
    (void) snprintf(post_metadata, sizeof(post_metadata),
                    "{\"source\":\"4chan\",\"resto\":%lld,"
                    "\"canonical_source_url\":"
                    "\"https://boards.4chan.org/%s/thread/%s#p%s\","
                    "\"file_deleted\":%s,\"spoiler\":%s}",
                    (long long) resto, adapter->current_board,
                    adapter->current_thread, number_text,
                    file_deleted == 1 ? "true" : "false",
                    spoiler == 1 ? "true" : "false");
    post_item = &storage->posts[post_count++];
    post_item->record.remote_id = y_owned_text(storage, remote_id);
    post_item->record.subject = subject;
    post_item->record.text = comment;
    post_item->record.created_at = created_text;
    post_item->record.last_seen_at =
        y_owned_u64(storage, adapter->scan_marker_ms);
    post_item->record.position = (int64_t) post_position;
    post_item->record.deleted = 0;
    post_item->record.restricted = 0;
    post_item->record.raw_metadata_version = 1;
    post_item->record.metadata_json = y_owned_text(storage, post_metadata);
    post_item->collection_kind = y_owned_text(storage, "thread");
    post_item->collection_remote_id = collection_remote;
    post_item->parent_remote_id = resto != 0
                                    ? y_owned_text(storage, parent_id) : NULL;
    if (post_item->record.remote_id == NULL ||
        post_item->record.last_seen_at == NULL ||
        post_item->record.metadata_json == NULL ||
        post_item->collection_kind == NULL ||
        (resto != 0 && post_item->parent_remote_id == NULL)) {
      result = HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
      goto done;
    }
    (void) y_mark_known_post(adapter, remote_id);
    if (post_position == 0U) {
      (void) y_optional_i64(&document, item, "archived", &op_archived, 0);
      (void) y_optional_i64(&document, item, "closed", &op_closed, 0);
      (void) y_optional_i64(&document, item, "sticky", &op_sticky, 0);
      op_subject = subject;
      op_comment = comment;
      op_created = created_text;
    }
    tim = -1;
    extension = NULL;
    if (y_optional_i64(&document, item, "tim", &tim, -1) == 1)
      extension = y_optional_string(storage, &document, item, "ext");
    if (tim > 0 && y_extension_valid(extension)) {
      char tim_text[32];
      char media_id[HTS_CATALOG_REMOTE_ID_MAX + 1U];
      char metadata[128];
      char *original_url;
      char *thumbnail_url;
      char *original_filename;
      const char *availability;
      hts_metadata_media_item *original;
      hts_metadata_media_item *thumbnail;
      hts_metadata_membership_item *member;
      filename = y_optional_string(storage, &document, item, "filename");
      md5 = y_optional_string(storage, &document, item, "md5");
      (void) y_optional_i64(&document, item, "fsize", &size_bytes, -1);
      (void) y_optional_i64(&document, item, "w", &width, -1);
      (void) y_optional_i64(&document, item, "h", &height, -1);
      (void) y_optional_i64(&document, item, "tn_w", &thumb_width, -1);
      (void) y_optional_i64(&document, item, "tn_h", &thumb_height, -1);
      (void) snprintf(tim_text, sizeof(tim_text), "%lld", (long long) tim);
      {
        int media_length;
        media_length = snprintf(media_id, sizeof(media_id), "%s/%s/%s",
                                adapter->current_board, number_text,
                                tim_text);
        if (media_length < 0 || (size_t) media_length >= sizeof(media_id))
          goto done;
      }
      original_url = y_owned_concat(storage, "https://i.4cdn.org/",
          adapter->current_board, "/", tim_text, extension);
      thumbnail_url = y_owned_concat(storage, "https://i.4cdn.org/",
          adapter->current_board, "/", tim_text, "s.jpg");
      original_filename = filename != NULL
                            ? y_owned_concat(storage, filename, extension,
                                             "", "", "") : NULL;
      availability = file_deleted == 1 ? "deleted" :
                     (strcmp(adapter->expected_state, "archived") == 0 ||
                      op_archived == 1) ? "archived" : "available";
      (void) snprintf(metadata, sizeof(metadata),
                      "{\"spoiler\":%s,\"file_deleted\":%s}",
                      spoiler == 1 ? "true" : "false",
                      file_deleted == 1 ? "true" : "false");
      if (original_url == NULL || thumbnail_url == NULL ||
          (filename != NULL && original_filename == NULL)) {
        result = HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
        goto done;
      }
      original = &storage->media[media_count++];
      original->record.remote_id = y_owned_text(storage, media_id);
      original->record.variant_kind = y_owned_text(storage, "original");
      original->record.remote_url = original_url;
      original->record.original_filename = original_filename;
      original->record.extension = extension;
      original->record.mime_type = y_owned_text(storage,
                                                 y_mime_type(extension));
      original->record.size_bytes = size_bytes;
      original->record.width = y_dimension(width);
      original->record.height = y_dimension(height);
      original->record.remote_hash = md5;
      original->record.hash_algorithm = md5 != NULL
                                          ? y_owned_text(storage,
                                                         "md5-base64") : NULL;
      original->record.availability_state =
          y_owned_text(storage, availability);
      original->record.metadata_json = y_owned_text(storage, metadata);
      original->post_remote_id = post_item->record.remote_id;
      thumbnail = &storage->media[media_count++];
      thumbnail->record.remote_id = y_owned_text(storage, media_id);
      thumbnail->record.variant_kind = y_owned_text(storage, "preview");
      thumbnail->record.remote_url = thumbnail_url;
      thumbnail->record.original_filename = original_filename;
      thumbnail->record.extension = y_owned_text(storage, ".jpg");
      thumbnail->record.mime_type = y_owned_text(storage, "image/jpeg");
      thumbnail->record.size_bytes = -1;
      thumbnail->record.width = y_dimension(thumb_width);
      thumbnail->record.height = y_dimension(thumb_height);
      thumbnail->record.availability_state =
          y_owned_text(storage, availability);
      thumbnail->record.metadata_json = y_owned_text(storage, metadata);
      thumbnail->post_remote_id = post_item->record.remote_id;
      if (original->record.remote_id == NULL ||
          original->record.variant_kind == NULL ||
          original->record.mime_type == NULL ||
          original->record.availability_state == NULL ||
          original->record.metadata_json == NULL ||
          thumbnail->record.remote_id == NULL ||
          thumbnail->record.variant_kind == NULL ||
          thumbnail->record.extension == NULL ||
          thumbnail->record.mime_type == NULL ||
          thumbnail->record.availability_state == NULL ||
          thumbnail->record.metadata_json == NULL ||
          (md5 != NULL && original->record.hash_algorithm == NULL)) {
        result = HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
        goto done;
      }
      member = &storage->memberships[membership_count++];
      member->collection_kind = post_item->collection_kind;
      member->collection_remote_id = collection_remote;
      member->media_remote_id = original->record.remote_id;
      member->media_variant_kind = original->record.variant_kind;
      member->position = (int64_t) membership_count - 1;
      member = &storage->memberships[membership_count++];
      member->collection_kind = post_item->collection_kind;
      member->collection_remote_id = collection_remote;
      member->media_remote_id = thumbnail->record.remote_id;
      member->media_variant_kind = thumbnail->record.variant_kind;
      member->position = (int64_t) membership_count - 1;
    }
    post_position++;
    item = y_token_skip(&document, item);
  }
  thread_state = op_archived == 1 ||
                 strcmp(adapter->expected_state, "archived") == 0
                   ? "archived" : "active";
  (void) snprintf(collection_metadata, sizeof(collection_metadata),
                  "{\"source\":\"4chan\",\"board\":\"%s\","
                  "\"thread_id\":%s,\"canonical_source_url\":"
                  "\"https://boards.4chan.org/%s/thread/%s\","
                  "\"closed\":%s,\"sticky\":%s,"
                  "\"archived\":%s,\"last_modified\":%s}",
                  adapter->current_board, adapter->current_thread,
                  adapter->current_board, adapter->current_thread,
                  op_closed == 1 ? "true" : "false",
                  op_sticky == 1 ? "true" : "false",
                  strcmp(thread_state, "archived") == 0 ? "true" : "false",
                  adapter->expected_version[0] != '\0'
                    ? adapter->expected_version : "null");
  storage->collections[0].record.kind = y_owned_text(storage, "thread");
  storage->collections[0].record.remote_id = collection_remote;
  storage->collections[0].record.title = op_subject;
  storage->collections[0].record.description = op_comment;
  storage->collections[0].record.lifecycle_state =
      y_owned_text(storage, thread_state);
  storage->collections[0].record.created_at = op_created;
  storage->collections[0].record.updated_at =
      adapter->expected_version[0] != '\0'
        ? y_owned_text(storage, adapter->expected_version) : NULL;
  storage->collections[0].record.last_seen_at =
      y_owned_u64(storage, adapter->scan_marker_ms);
  storage->collections[0].record.metadata_json =
      y_owned_text(storage, collection_metadata);
  storage->collections[0].board_remote_id =
      y_owned_text(storage, adapter->current_board);
  if (storage->collections[0].record.kind == NULL ||
      storage->collections[0].record.lifecycle_state == NULL ||
      storage->collections[0].record.last_seen_at == NULL ||
      storage->collections[0].record.metadata_json == NULL ||
      storage->collections[0].board_remote_id == NULL) {
    result = HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
    goto done;
  }
  storage->resources[0].record.resource_kind =
      y_owned_text(storage, "yotsuba-thread");
  storage->resources[0].record.resource_id = collection_remote;
  storage->resources[0].record.parent_remote_id =
      storage->collections[0].board_remote_id;
  storage->resources[0].record.remote_version =
      adapter->expected_version[0] != '\0'
        ? y_owned_text(storage, adapter->expected_version) : NULL;
  storage->resources[0].record.synchronized_version =
      adapter->expected_version[0] != '\0'
        ? y_owned_text(storage, adapter->expected_version) : NULL;
  storage->resources[0].record.lifecycle_state =
      storage->collections[0].record.lifecycle_state;
  storage->resources[0].record.synchronized_state =
      y_owned_text(storage, thread_state);
  storage->resources[0].record.missing_count = 0U;
  storage->resources[0].record.last_seen_ms =
      (int64_t) adapter->scan_marker_ms;
  storage->resources[0].record.last_checked_ms =
      (int64_t) adapter->scan_marker_ms;
  if (storage->resources[0].record.resource_kind == NULL ||
      storage->resources[0].record.synchronized_state == NULL ||
      (adapter->expected_version[0] != '\0' &&
       (storage->resources[0].record.remote_version == NULL ||
        storage->resources[0].record.synchronized_version == NULL))) {
    result = HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
    goto done;
  }
  for (i = 0U; i < adapter->known_post_count; i++) {
    hts_metadata_lifecycle_change *change;
    if (adapter->known_posts[i].seen) continue;
    change = &storage->changes[change_count++];
    change->target = HTS_METADATA_LIFECYCLE_POST;
    change->remote_id = y_owned_text(storage,
                                     adapter->known_posts[i].remote_id);
    change->state = y_owned_text(storage, "deleted");
    change->media_availability_state = y_owned_text(storage, "deleted");
    if (change->remote_id == NULL || change->state == NULL ||
        change->media_availability_state == NULL) {
      result = HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
      goto done;
    }
  }
  batch->collections = storage->collections;
  batch->collection_count = 1U;
  batch->posts = storage->posts;
  batch->post_count = post_count;
  batch->media = storage->media;
  batch->media_count = media_count;
  batch->memberships = storage->memberships;
  batch->membership_count = membership_count;
  batch->resources = storage->resources;
  batch->resource_count = 1U;
  batch->lifecycle_changes = storage->changes;
  batch->lifecycle_change_count = change_count;
  batch->replace_membership_collection_kind =
      storage->collections[0].record.kind;
  batch->replace_membership_collection_remote_id = collection_remote;
  batch->next_cursor = adapter->expected_version[0] != '\0'
                         ? y_owned_text(storage, adapter->expected_version)
                         : y_owned_text(storage, "complete");
  if (batch->next_cursor == NULL) {
    result = HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
    goto done;
  }
  adapter->last_post_count = post_count;
  adapter->last_media_count = media_count;
  result = HTS_METADATA_SUCCESS;

done:
  y_document_release(&document);
  return result;
}

static int y_resource_in_batch(const y_batch_storage *storage,
                               size_t count, const char *resource_id) {
  size_t i;
  for (i = 0U; i < count; i++) {
    const char *current;
    current = storage->resources[i].record.resource_id;
    if (current != NULL && strcmp(current, resource_id) == 0) return 1;
  }
  return 0;
}

static int y_add_missing(hts_yotsuba_adapter *adapter,
                         y_batch_storage *storage, size_t *resource_count,
                         size_t *change_count,
                         const y_known_resource *known) {
  hts_catalog_resource_state *resource;
  hts_metadata_lifecycle_change *change;
  unsigned int missing;
  const char *state;
  missing = strcmp(known->lifecycle_state, "missing") == 0
              ? known->missing_count + 1U : 1U;
  if (strcmp(known->lifecycle_state, "expired") == 0) {
    state = "expired";
    missing = known->missing_count;
  } else {
    state = missing >= adapter->options.missing_confirmations
              ? "expired" : "missing";
  }
  resource = &storage->resources[(*resource_count)++].record;
  resource->resource_kind = y_owned_text(storage, "yotsuba-thread");
  resource->resource_id = y_owned_text(storage, known->resource_id);
  resource->parent_remote_id = y_owned_text(storage, adapter->current_board);
  resource->remote_version = NULL;
  resource->synchronized_version = NULL;
  resource->lifecycle_state = y_owned_text(storage, state);
  resource->synchronized_state = NULL;
  resource->missing_count = missing;
  resource->last_seen_ms = -1;
  resource->last_checked_ms = (int64_t) adapter->scan_marker_ms;
  if (resource->resource_kind == NULL || resource->resource_id == NULL ||
      resource->parent_remote_id == NULL || resource->lifecycle_state == NULL)
    return 0;
  change = &storage->changes[(*change_count)++];
  change->target = HTS_METADATA_LIFECYCLE_COLLECTION;
  change->kind = y_owned_text(storage, "thread");
  change->remote_id = y_owned_text(storage, known->resource_id);
  change->state = y_owned_text(storage, state);
  change->media_availability_state =
      strcmp(state, "expired") == 0
        ? y_owned_text(storage, "expired") : NULL;
  return change->kind != NULL && change->remote_id != NULL &&
         change->state != NULL &&
         (strcmp(state, "expired") != 0 ||
          change->media_availability_state != NULL);
}

static hts_metadata_category y_parse_threads(
    hts_yotsuba_adapter *adapter, const hts_metadata_http_response *response,
    hts_metadata_batch *batch) {
  y_document document;
  y_batch_storage *storage;
  size_t page_item;
  size_t total;
  size_t resource_count;
  size_t change_count;
  size_t i;
  hts_metadata_category result;
  (void) memset(batch, 0, sizeof(*batch));
  if (!y_parse_document(response->body, response->body_size, &document))
    return HTS_METADATA_MALFORMED;
  storage = y_batch_new(adapter);
  if (storage == NULL) {
    y_document_release(&document);
    return HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
  }
  result = HTS_METADATA_MALFORMED;
  if (document.tokens[0].type != Y_TOKEN_ARRAY) goto done;
  total = 0U;
  page_item = 1U;
  while (y_array_next(&document, 0U, &page_item)) {
    size_t threads_token;
    if (document.tokens[page_item].type != Y_TOKEN_OBJECT ||
        !y_object_get(&document, page_item, "threads", &threads_token) ||
        document.tokens[threads_token].type != Y_TOKEN_ARRAY ||
        total > YOTSUBA_MAX_THREADS - document.tokens[threads_token].size)
      goto done;
    total += document.tokens[threads_token].size;
    page_item = y_token_skip(&document, page_item);
  }
  if (!y_batch_arrays(storage, 0U, 0U, 0U, 0U, 0U,
                      total + adapter->known_resource_count,
                      adapter->known_resource_count)) {
    result = HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
    goto done;
  }
  resource_count = 0U;
  change_count = 0U;
  page_item = 1U;
  while (y_array_next(&document, 0U, &page_item)) {
    size_t threads_token;
    size_t thread_item;
    (void) y_object_get(&document, page_item, "threads", &threads_token);
    thread_item = threads_token + 1U;
    while (y_array_next(&document, threads_token, &thread_item)) {
      size_t no_token;
      size_t modified_token;
      int64_t number;
      int64_t modified;
      char thread[32];
      char resource_id[HTS_CATALOG_REMOTE_ID_MAX + 1U];
      hts_catalog_resource_state *resource;
      if (document.tokens[thread_item].type != Y_TOKEN_OBJECT ||
          !y_object_get(&document, thread_item, "no", &no_token) ||
          !y_object_get(&document, thread_item, "last_modified",
                        &modified_token) ||
          !y_token_i64(&document, no_token, &number) || number <= 0 ||
          !y_token_i64(&document, modified_token, &modified) || modified <= 0)
        goto done;
      (void) snprintf(thread, sizeof(thread), "%lld", (long long) number);
      if (!y_make_resource_id(resource_id, sizeof(resource_id),
                              adapter->current_board, thread)) goto done;
      resource = &storage->resources[resource_count++].record;
      resource->resource_kind = y_owned_text(storage, "yotsuba-thread");
      resource->resource_id = y_owned_text(storage, resource_id);
      resource->parent_remote_id =
          y_owned_text(storage, adapter->current_board);
      resource->remote_version =
          y_owned_u64(storage, (uint64_t) modified);
      resource->lifecycle_state = y_owned_text(storage, "active");
      resource->missing_count = 0U;
      resource->last_seen_ms = (int64_t) adapter->scan_marker_ms;
      resource->last_checked_ms = (int64_t) adapter->scan_marker_ms;
      if (resource->resource_kind == NULL || resource->resource_id == NULL ||
          resource->parent_remote_id == NULL ||
          resource->remote_version == NULL ||
          resource->lifecycle_state == NULL) {
        result = HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
        goto done;
      }
      thread_item = y_token_skip(&document, thread_item);
    }
    page_item = y_token_skip(&document, page_item);
  }
  if (!adapter->archive_supported) {
    for (i = 0U; i < adapter->known_resource_count; i++) {
      if (!y_resource_in_batch(storage, resource_count,
                               adapter->known_resources[i].resource_id) &&
          !y_add_missing(adapter, storage, &resource_count, &change_count,
                         &adapter->known_resources[i])) {
        result = HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
        goto done;
      }
    }
  }
  batch->resources = storage->resources;
  batch->resource_count = resource_count;
  batch->lifecycle_changes = storage->changes;
  batch->lifecycle_change_count = change_count;
  batch->next_cursor = y_owned_u64(storage, adapter->scan_marker_ms);
  if (batch->next_cursor == NULL) {
    result = HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
    goto done;
  }
  result = HTS_METADATA_SUCCESS;

done:
  y_document_release(&document);
  return result;
}

static hts_metadata_category y_parse_archive(
    hts_yotsuba_adapter *adapter, const hts_metadata_http_response *response,
    hts_metadata_batch *batch) {
  y_document document;
  y_batch_storage *storage;
  size_t item;
  size_t resource_count;
  size_t change_count;
  size_t i;
  hts_metadata_category result;
  (void) memset(batch, 0, sizeof(*batch));
  if (!y_parse_document(response->body, response->body_size, &document))
    return HTS_METADATA_MALFORMED;
  storage = y_batch_new(adapter);
  if (storage == NULL) {
    y_document_release(&document);
    return HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
  }
  result = HTS_METADATA_MALFORMED;
  if (document.tokens[0].type != Y_TOKEN_ARRAY ||
      document.tokens[0].size > YOTSUBA_MAX_THREADS ||
      !y_batch_arrays(storage, 0U, 0U, 0U, 0U, 0U,
                      document.tokens[0].size + adapter->known_resource_count,
                      document.tokens[0].size +
                        adapter->known_resource_count)) goto done;
  resource_count = 0U;
  change_count = 0U;
  item = 1U;
  while (y_array_next(&document, 0U, &item)) {
    int64_t number;
    char thread[32];
    char resource_id[HTS_CATALOG_REMOTE_ID_MAX + 1U];
    hts_catalog_resource_state *resource;
    hts_metadata_lifecycle_change *change;
    if (!y_token_i64(&document, item, &number) || number <= 0) goto done;
    (void) snprintf(thread, sizeof(thread), "%lld", (long long) number);
    if (!y_make_resource_id(resource_id, sizeof(resource_id),
                            adapter->current_board, thread)) goto done;
    resource = &storage->resources[resource_count++].record;
    resource->resource_kind = y_owned_text(storage, "yotsuba-thread");
    resource->resource_id = y_owned_text(storage, resource_id);
    resource->parent_remote_id = y_owned_text(storage, adapter->current_board);
    resource->remote_version = y_owned_text(storage, "archived");
    resource->lifecycle_state = y_owned_text(storage, "archived");
    resource->missing_count = 0U;
    resource->last_seen_ms = (int64_t) adapter->scan_marker_ms;
    resource->last_checked_ms = (int64_t) adapter->scan_marker_ms;
    change = &storage->changes[change_count++];
    change->target = HTS_METADATA_LIFECYCLE_COLLECTION;
    change->kind = y_owned_text(storage, "thread");
    change->remote_id = y_owned_text(storage, resource_id);
    change->state = y_owned_text(storage, "archived");
    change->media_availability_state = y_owned_text(storage, "archived");
    if (resource->resource_kind == NULL || resource->resource_id == NULL ||
        resource->parent_remote_id == NULL ||
        resource->remote_version == NULL ||
        resource->lifecycle_state == NULL || change->kind == NULL ||
        change->remote_id == NULL || change->state == NULL ||
        change->media_availability_state == NULL) {
      result = HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
      goto done;
    }
    item = y_token_skip(&document, item);
  }
  for (i = 0U; i < adapter->known_resource_count; i++) {
    const y_known_resource *known;
    known = &adapter->known_resources[i];
    if ((known->last_seen_ms == (int64_t) adapter->scan_marker_ms &&
         strcmp(known->lifecycle_state, "active") == 0) ||
        y_resource_in_batch(storage, resource_count, known->resource_id))
      continue;
    if (!y_add_missing(adapter, storage, &resource_count, &change_count,
                       known)) {
      result = HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
      goto done;
    }
  }
  batch->resources = storage->resources;
  batch->resource_count = resource_count;
  batch->lifecycle_changes = storage->changes;
  batch->lifecycle_change_count = change_count;
  batch->next_cursor = y_owned_u64(storage, adapter->scan_marker_ms);
  if (batch->next_cursor == NULL) {
    result = HTS_METADATA_PERMANENT_CONFIGURATION_ERROR;
    goto done;
  }
  result = HTS_METADATA_SUCCESS;

done:
  y_document_release(&document);
  return result;
}
