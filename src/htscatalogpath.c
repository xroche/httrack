#include "htscatalogpath.h"
#include "htscatalog.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unicode/unorm2.h>
#include <unicode/uchar.h>
#include <unicode/ustring.h>
#include <unicode/utf8.h>

static uint64_t path_hash(const char *value) {
  uint64_t hash;
  const unsigned char *scan;
  hash = UINT64_C(1469598103934665603);
  for (scan = (const unsigned char *) value; *scan != '\0'; scan++) {
    hash ^= (uint64_t) *scan;
    hash *= UINT64_C(1099511628211);
  }
  return hash;
}

static char *normalize_nfkc(const char *input) {
  const UNormalizer2 *normalizer;
  UChar *utf16;
  UChar *normalized;
  char *utf8;
  int32_t utf16_length;
  int32_t normalized_length;
  int32_t utf8_length;
  UErrorCode error;
  if (input == NULL) input = "";
  error = U_ZERO_ERROR;
  u_strFromUTF8(NULL, 0, &utf16_length, input, -1, &error);
  if (error != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(error)) return NULL;
  error = U_ZERO_ERROR;
  utf16 = (UChar *) malloc(((size_t) utf16_length + 1U) * sizeof(UChar));
  if (utf16 == NULL) return NULL;
  u_strFromUTF8(utf16, utf16_length + 1, NULL, input, -1, &error);
  if (U_FAILURE(error)) {
    free(utf16);
    return NULL;
  }
  normalizer = unorm2_getNFKCInstance(&error);
  if (U_FAILURE(error)) {
    free(utf16);
    return NULL;
  }
  error = U_ZERO_ERROR;
  normalized_length = unorm2_normalize(normalizer, utf16, utf16_length,
                                        NULL, 0, &error);
  if (error != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(error)) {
    free(utf16);
    return NULL;
  }
  error = U_ZERO_ERROR;
  normalized = (UChar *) malloc(((size_t) normalized_length + 1U) *
                                sizeof(UChar));
  if (normalized == NULL) {
    free(utf16);
    return NULL;
  }
  (void) unorm2_normalize(normalizer, utf16, utf16_length, normalized,
                          normalized_length + 1, &error);
  free(utf16);
  if (U_FAILURE(error)) {
    free(normalized);
    return NULL;
  }
  error = U_ZERO_ERROR;
  u_strToUTF8(NULL, 0, &utf8_length, normalized, normalized_length, &error);
  if (error != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(error)) {
    free(normalized);
    return NULL;
  }
  error = U_ZERO_ERROR;
  utf8 = (char *) malloc((size_t) utf8_length + 1U);
  if (utf8 == NULL) {
    free(normalized);
    return NULL;
  }
  u_strToUTF8(utf8, utf8_length + 1, NULL, normalized,
              normalized_length, &error);
  free(normalized);
  if (U_FAILURE(error)) {
    free(utf8);
    return NULL;
  }
  return utf8;
}

static int forbidden_ascii(UChar32 codepoint) {
  return codepoint < 32 || codepoint == 127 || codepoint == '<' ||
         codepoint == '>' || codepoint == ':' || codepoint == '"' ||
         codepoint == '/' || codepoint == '\\' || codepoint == '|' ||
         codepoint == '?' || codepoint == '*';
}

static int reserved_windows_name(const char *value) {
  char base[16];
  size_t length;
  size_t i;
  const char *dot;
  dot = strchr(value, '.');
  length = dot != NULL ? (size_t) (dot - value) : strlen(value);
  if (length == 0U || length >= sizeof(base)) return 0;
  for (i = 0U; i < length; i++)
    base[i] = (char) toupper((unsigned char) value[i]);
  base[length] = '\0';
  if (strcmp(base, "CON") == 0 || strcmp(base, "PRN") == 0 ||
      strcmp(base, "AUX") == 0 || strcmp(base, "NUL") == 0) return 1;
  if (length == 4U &&
      ((memcmp(base, "COM", 3U) == 0) ||
       (memcmp(base, "LPT", 3U) == 0)) &&
      base[3] >= '1' && base[3] <= '9') return 1;
  return 0;
}

static size_t utf8_boundary(const char *value, size_t maximum) {
  size_t length;
  length = strlen(value);
  if (length <= maximum) return length;
  while (maximum != 0U &&
         (((unsigned char) value[maximum] & 0xc0U) == 0x80U)) maximum--;
  return maximum;
}

int hts_catalog_safe_component(const char *input, size_t maximum_bytes,
                               char *output, size_t output_size) {
  char *normalized;
  char *temporary;
  int32_t index;
  int32_t length;
  size_t used;
  int separator;
  uint64_t hash;
  if (output == NULL || output_size == 0U || maximum_bytes < 12U ||
      maximum_bytes >= output_size) return 0;
  normalized = normalize_nfkc(input);
  if (normalized == NULL) return 0;
  length = (int32_t) strlen(normalized);
  temporary = (char *) malloc((size_t) length * 4U + 4U);
  if (temporary == NULL) {
    free(normalized);
    return 0;
  }
  index = 0;
  used = 0U;
  separator = 0;
  while (index < length) {
    int32_t start;
    UChar32 codepoint;
    start = index;
    U8_NEXT(normalized, index, length, codepoint);
    if (codepoint < 0) {
      free(temporary);
      free(normalized);
      return 0;
    }
    if (forbidden_ascii(codepoint) || u_isUWhiteSpace(codepoint)) {
      separator = used != 0U;
      continue;
    }
    if (separator && used != 0U && temporary[used - 1U] != '-')
      temporary[used++] = '-';
    separator = 0;
    if (codepoint == '.') {
      temporary[used++] = '.';
    } else {
      size_t bytes;
      bytes = (size_t) (index - start);
      (void) memcpy(temporary + used, normalized + start, bytes);
      used += bytes;
    }
  }
  free(normalized);
  while (used != 0U && (temporary[used - 1U] == '.' ||
                         temporary[used - 1U] == '-' ||
                         temporary[used - 1U] == ' ')) used--;
  while (used != 0U && (temporary[0] == '.' || temporary[0] == ' ')) {
    (void) memmove(temporary, temporary + 1, used - 1U);
    used--;
  }
  temporary[used] = '\0';
  if (used == 0U || strcmp(temporary, ".") == 0 ||
      strcmp(temporary, "..") == 0) {
    (void) strcpy(temporary, "_");
    used = 1U;
  }
  if (reserved_windows_name(temporary)) {
    (void) memmove(temporary + 1, temporary, used + 1U);
    temporary[0] = '_';
    used++;
  }
  if (used > maximum_bytes) {
    char suffix[18];
    size_t suffix_length;
    size_t prefix;
    hash = path_hash(temporary);
    (void) snprintf(suffix, sizeof(suffix), "~%08lx",
                    (unsigned long) (hash & UINT64_C(0xffffffff)));
    suffix_length = strlen(suffix);
    prefix = utf8_boundary(temporary, maximum_bytes - suffix_length);
    (void) memcpy(temporary + prefix, suffix, suffix_length + 1U);
    used = prefix + suffix_length;
  }
  if (used + 1U > output_size) {
    free(temporary);
    return 0;
  }
  (void) memcpy(output, temporary, used + 1U);
  free(temporary);
  return 1;
}

int hts_catalog_preview_path(
    const char *source, const char *board, const char *collection_id,
    const char *collection_title, unsigned long ordinal,
    const char *post_id, const char *original_filename,
    const char *extension, const char *collision_key,
    const char *collision_suffix, char *output, size_t output_size) {
  char source_part[129];
  char board_part[129];
  char collection_part[129];
  char slug[129];
  char post_part[129];
  char filename[193];
  char *raw_filename;
  char extension_part[35];
  char directory[300];
  const char *suffix;
  size_t filename_length;
  size_t extension_length;
  size_t compare_index;
  int same_extension;
  int written;
  (void) collision_key;
  suffix = collision_suffix != NULL ? collision_suffix : "";
  if (original_filename == NULL) original_filename = "unnamed";
  filename_length = strlen(original_filename);
  raw_filename = (char *) malloc(filename_length + 1U);
  if (raw_filename == NULL) return 0;
  (void) memcpy(raw_filename, original_filename, filename_length + 1U);
  extension_length = extension != NULL ? strlen(extension) : 0U;
  same_extension = extension_length != 0U && filename_length > extension_length;
  if (same_extension) {
    for (compare_index = 0U; compare_index < extension_length; compare_index++) {
      unsigned char left;
      unsigned char right;
      left = (unsigned char) raw_filename[filename_length - extension_length + compare_index];
      right = (unsigned char) extension[compare_index];
      if (left >= 'A' && left <= 'Z') left = (unsigned char) (left + 32U);
      if (right >= 'A' && right <= 'Z') right = (unsigned char) (right + 32U);
      if (left != right) { same_extension = 0; break; }
    }
  }
  if (same_extension) raw_filename[filename_length - extension_length] = '\0';
  if (!hts_catalog_safe_component(source, 120U, source_part,
                                  sizeof(source_part)) ||
      !hts_catalog_safe_component(board, 120U, board_part,
                                  sizeof(board_part)) ||
      !hts_catalog_safe_component(collection_id, 120U, collection_part,
                                  sizeof(collection_part)) ||
      !hts_catalog_safe_component(collection_title, 120U, slug,
                                  sizeof(slug)) ||
      !hts_catalog_safe_component(post_id, 120U, post_part,
                                  sizeof(post_part)) ||
      !hts_catalog_safe_component(raw_filename, 170U, filename,
                                  sizeof(filename))) {
    free(raw_filename);
    return 0;
  }
  free(raw_filename);
  extension_part[0] = '\0';
  if (extension != NULL && extension[0] != '\0' &&
      !hts_catalog_safe_component(extension[0] == '.' ? extension + 1 : extension,
                                  24U, extension_part,
                                  sizeof(extension_part))) return 0;
  written = snprintf(directory, sizeof(directory), "%s — %s",
                     collection_part, slug);
  if (written < 0 || (size_t) written >= sizeof(directory)) return 0;
  written = snprintf(output, output_size,
                     "%s/%s/%s/%04lu-%s-%s%s%s%s",
                     source_part, board_part, directory, ordinal,
                     post_part, filename, suffix,
                     extension_part[0] != '\0' ? "." : "",
                     extension_part);
  return written >= 0 && (size_t) written < output_size &&
         (size_t) written <= HTS_CATALOG_PREVIEW_PATH_MAX;
}
