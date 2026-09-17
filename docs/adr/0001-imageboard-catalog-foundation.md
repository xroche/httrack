# ADR 0001: Imageboard catalog foundation

- Status: accepted
- Date: 2026-09-16
- Scope: Phase 1 only

## Context

HTTrack's mirror engine and binary cache are URL- and HTTP-response-oriented.
An imageboard index instead needs queryable semantic entities (sources, boards,
collections, posts, and media) plus synchronization and later download state.
Putting those records in `cache_add`, the URL queue, or save-name callbacks
would couple remote API semantics to the existing mirror format and risk
changing established crawl behavior.

## Decision

The catalog is an isolated `src/htscatalog.{c,h}` repository module.  Callers
must explicitly open a catalog; no existing CLI path or mirror lifecycle hook
opens one.  Later source adapters will remain separate from persistence and use
the typed upsert/transaction/read/list API.  Catalog synchronization will store
metadata only; media body fetching belongs to a later, separately enabled
preservation/download phase.

SQLite is used through the system `sqlite3` development package.  Configure
discovers it with the project's Autoconf conventions only when requested.  The
default is off so the legacy build remains unchanged.
`--enable-imageboard-catalog=yes` enables the module and turns missing SQLite
headers or libraries into a clear configure error; `auto` is available for
packagers that prefer dependency-based detection.  No SQLite amalgamation is
vendored.

Schema changes are forward-only, numbered migrations executed inside one
transaction, with both `PRAGMA user_version` and `schema_migrations` recording
the applied version.  Opening a newer schema fails rather than attempting a
downgrade.  Natural uniqueness keys are source-scoped and upserts are prepared
statements, making repeated pages idempotent.  Adapter-specific data may be
retained only in bounded JSON object/array fields; normalized columns remain the
primary model.  Credential fields accept references to external configuration
(`env:`, `file:`, `keyring:`, or `config:`), never secret values.  JSON fields
also reject common credential-bearing keys before persistence.

## Consequences

The existing cache format, URL naming, crawl filters, callbacks, and CLI remain
unchanged.  Builds that enable the catalog add a dynamic dependency on the
system SQLite library.  API consumers must batch a page with
`hts_catalog_begin`/`commit` and roll back the whole page on any validation or
database error.  The Phase 1 test utility initializes and inspects only local
database files and performs no network or media operations.
