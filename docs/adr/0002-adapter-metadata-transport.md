# ADR 0002: Adapter contract and metadata transport

- Status: accepted
- Date: 2026-09-16
- Scope: Phase 2
- Extends: ADR 0001

## Context

Imageboard APIs differ in source configuration, discovery, pagination,
conditional request support, authentication, and lifecycle semantics. Shared
catalog code must not interpret an adapter's cursor or infer deletion from an
HTTP failure. Metadata synchronization also needs origin-specific policy and
deterministic offline tests without introducing a second networking stack.

HTTrack's existing `back_*` transport is suitable for a future production
backend but is coupled to crawl queues, cache entries, output files, and mirror
lifecycle state. Calling it directly from catalog persistence would violate the
isolation established by ADR 0001.

## Decision

`src/htsmetadata.{c,h}` defines an adapter vtable for source validation and
canonicalization, board discovery/validation, collection and post enumeration,
request construction, deterministic response parsing, normalized batches, and
capability advertisement. Cursor strings are passed unchanged between the
adapter and catalog. The catalog schema stores the cursor format version and
adapter version and rejects incompatible resumes.

The metadata runtime accepts an injected HTTP transport interface. It owns no
socket, DNS, proxy, TLS, cookie, or authentication implementation. A production
transport can therefore adapt HTTrack's existing connection machinery without
duplicating it or exposing catalog transactions to crawl/cache state. Phase 2
ships only a deterministic fake transport. Authentication crosses this boundary
only as an opaque credential reference for transport-side resolution; adapters
do not manufacture or log authorization headers. Response buffers remain owned
by the transport until its next request, avoiding an additional allocator or
network-lifecycle API.

Each canonical origin has independent scheduling state. Adapter/source policy
sets request rate, concurrency limit, minimum refresh interval, retry count,
exponential backoff and jitter, response bound, redirect limit, connect and
request timeouts, and User-Agent. Retry-After can lengthen the computed retry
delay. Conditional validators are sourced from the durable cursor record. The
blocking runtime serializes cursor-dependent pages and passes the per-origin
concurrency cap plus a cooperative cancellation token into the shared
transport boundary, where concurrent requests across synchronization jobs must
be queued.

HTTP and parser outcomes use explicit categories: success, not modified,
temporary unavailability, rate limiting, unauthorized, forbidden, gone/not
found, malformed data, permanent configuration error, and cancellation. HTTP
errors never create lifecycle changes. Adapters must emit lifecycle state only
from a successfully parsed response.

An entire response page is parsed into memory before persistence begins.
Boards, collections, posts, media metadata, membership positions, and the next
cursor are then upserted in one SQLite transaction. Any reference, validation,
or database error rolls back both records and cursor advancement.

Progress events contain only fixed messages, category, adapter identifier,
canonical origin, operation, attempt/page counters, record counts, and delay.
They never contain credentials, sensitive request headers, response bodies, or
post bodies.

## Consequences

Adapters remain testable against stored response fixtures without a database or
network. The runtime is metadata-only and cannot acquire binary media. Live
network smoke tests remain separate, opt-in, and disabled by default. A later
phase must implement and review the concrete HTTrack-backed transport wrapper
before any real imageboard adapter is enabled.
