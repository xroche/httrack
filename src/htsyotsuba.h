/* Read-only Yotsuba API metadata adapter. */

#ifndef HTSYOTSUBA_H
#define HTSYOTSUBA_H

#include "htsmetadata.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HTS_YOTSUBA_API_ORIGIN "https://a.4cdn.org"
#define HTS_YOTSUBA_ADAPTER_VERSION "yotsuba-1"
#define HTS_YOTSUBA_CURSOR_VERSION 1
#define HTS_YOTSUBA_DEFAULT_USER_AGENT \
  "HTTrackClone-Catalog/3 (+https://www.httrack.com/)"

typedef struct hts_yotsuba_adapter hts_yotsuba_adapter;

typedef struct hts_yotsuba_options {
  int64_t source_id;
  const char *const *boards;
  size_t board_count;
  int discover_boards;
  unsigned int missing_confirmations;
  hts_metadata_policy policy;
  hts_metadata_cancel cancel;
} hts_yotsuba_options;

typedef struct hts_yotsuba_sync_result {
  hts_metadata_category category;
  size_t boards_processed;
  size_t threads_discovered;
  size_t threads_fetched;
  size_t threads_not_modified;
  size_t threads_missing;
  size_t posts_committed;
  size_t media_variants_committed;
  unsigned int requests_made;
} hts_yotsuba_sync_result;

HTSEXT_API void hts_yotsuba_default_policy(hts_metadata_policy *policy);
HTSEXT_API int hts_yotsuba_adapter_create(
    hts_catalog *catalog, const hts_yotsuba_options *options,
    hts_yotsuba_adapter **out_adapter);
HTSEXT_API void hts_yotsuba_adapter_destroy(hts_yotsuba_adapter *adapter);
HTSEXT_API const hts_metadata_adapter *hts_yotsuba_adapter_contract(
    hts_yotsuba_adapter *adapter);
HTSEXT_API hts_metadata_category hts_yotsuba_sync(
    hts_yotsuba_adapter *adapter, const hts_metadata_transport *transport,
    const hts_metadata_clock *clock, hts_metadata_report_fn report,
    void *report_context, hts_yotsuba_sync_result *result);

#ifdef __cplusplus
}
#endif

#endif
