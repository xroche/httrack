# ADR 0003: Read-only Yotsuba metadata adapter

- Status: Accepted
- Date: 2026-09-16
- Extends: ADR 0001 and ADR 0002

## Context

Phase 3 needs a production adapter for the public 4chan/Yotsuba API without
turning catalog synchronization into media acquisition. The API exposes board
metadata, active-thread inventories, optional archive inventories, complete
thread documents, and deterministic media URLs. Catalog previews are not full
threads and therefore cannot be used as the post source of truth.

The upstream contract is the official [4chan API documentation](https://github.com/4chan/4chan-API), including its documentation for [endpoints and domains](https://github.com/4chan/4chan-API/blob/master/pages/Endpoints_and_domains.md), [catalogs](https://github.com/4chan/4chan-API/blob/master/pages/Catalog.md), [archives](https://github.com/4chan/4chan-API/blob/master/pages/Archive.md), and [media layout](https://github.com/4chan/4chan-API/blob/master/pages/User_images_and_static_content.md).

## Decision

`htsyotsuba` is a read-only implementation of the Phase 2 adapter contract.
It accepts only `https://a.4cdn.org` as its metadata origin and rejects
credentials. It supports explicit board allow-lists and opt-in discovery from
`boards.json`. The adapter is independently developed and must not be
presented as an official 4chan client.

Synchronization uses this sequence for each selected board:

1. Refresh or conditionally validate `boards.json`.
2. Read the complete active-thread inventory from `threads.json`.
3. Read `archive.json` only when `boards.json` advertises archive support.
4. Compare each thread's remote version and lifecycle with its durable
   synchronized state.
5. Fetch every new or changed full `/<board>/thread/<op>.json` document.
6. Commit the full parsed thread, memberships, lifecycle, and cursor in one
   SQLite transaction.

Catalog-preview replies are never treated as complete threads. Every post and
attachment comes from a successfully parsed full-thread response. A thread is
a collection; posts use API array order; an attachment's `original` variant
precedes its `preview` variant. The media URLs are stored as metadata using the
documented `i.4cdn.org/<board>/<tim><ext>` and `<tim>s.jpg` layout. The adapter
transport remains origin-locked to `a.4cdn.org`, so catalog synchronization
cannot request either media variant.

The default policy is 30 requests per minute, concurrency one, and a 30-second
minimum refresh interval. Configuration is rejected if it exceeds the
documented maximum of 60 requests per minute, permits concurrent requests, or
sets a thread refresh interval below ten seconds. Conditional requests,
bounded bodies, retry/backoff, `Retry-After`, timeouts, redirects,
cancellation, and the identifying user agent remain enforced by the Phase 2
runtime.

## Reconciliation rule

Lifecycle changes require a complete, successfully parsed inventory. An item
seen in `threads.json` is `active`; an item seen only in `archive.json` is
`archived`. When an archive-capable board omits a previously known thread from
both complete inventories, or a non-archive board omits it from its complete
active inventory, the adapter records one missing observation. Two consecutive
complete observations (configurable upward, never below two) classify it as
`expired`.

A thread endpoint 404 or 410 records one missing observation but never erases
the collection, posts, attachments, or source metadata. A 429, 503, transport
failure, cancellation, malformed response, or incomplete scan does not count
as deletion evidence. The resource records the transient error while the last
known lifecycle remains intact. Media marked `filedeleted` is `deleted`;
archived media is `archived`; first-missing media is
`temporarily_unavailable`; confirmed-expired media is `expired`.

No unofficial archive is queried and no deleted content is recovered.

## Testing and operations

Committed fixtures are synthetic and contain no copied user media or
substantive live posts. The deterministic fake transport verifies initial and
incremental synchronization, conditional 304 handling, 503 retries,
per-origin pacing, malformed rollback, 404 behavior, cancellation, restart,
ordering, archive reconciliation, and zero media-host requests.

`tests/yotsuba-live-smoke.py` is not part of the default test suite. It runs
only when `HTTRACK_YOTSUBA_LIVE=1` and an operator explicitly supplies
`HTTRACK_YOTSUBA_LIVE_BOARD`. It applies the production rate limit and reads
only JSON API endpoints.

## Consequences

The catalog retains canonical public thread/post URLs and media references,
but does not download, mirror, or publicly rehost their bodies. Archived media
may expire before a later acquisition phase can fetch it. That limitation is
intentional: preservation eligibility is metadata for future policy decisions,
not authorization for Phase 3 to acquire binaries.
