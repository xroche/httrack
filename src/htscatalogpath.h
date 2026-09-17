#ifndef HTSCATALOGPATH_H
#define HTSCATALOGPATH_H

#include "htsglobal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HTS_CATALOG_PREVIEW_PATH_MAX 1024U

HTSEXT_API int hts_catalog_safe_component(
    const char *input, size_t maximum_bytes, char *output,
    size_t output_size);

HTSEXT_API int hts_catalog_preview_path(
    const char *source, const char *board, const char *collection_id,
    const char *collection_title, unsigned long ordinal,
    const char *post_id, const char *original_filename,
    const char *extension, const char *collision_key,
    const char *collision_suffix, char *output, size_t output_size);

#ifdef __cplusplus
}
#endif

#endif
