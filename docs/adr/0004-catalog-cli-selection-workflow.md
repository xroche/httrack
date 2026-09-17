# ADR 0004: Catalog CLI and frozen selections

## Status

Accepted for Phase 5.

## Context

The legacy `httrack` entry point delegates its arguments directly to the historic HTTrack option parser. Adding an unrelated subcommand grammar there would risk changing long-standing option behavior. The catalog also needs stable JSON output, destructive-operation confirmation, metadata-only synchronization, and immutable download intent without acquiring media bodies.

Selections must remain meaningful after a remote collection changes. Output paths must be predictable on Windows, macOS, and Unix filesystems and safe when remote titles or filenames are hostile.

## Decision

Phase 5 installs a companion `httrack-catalog` executable. The legacy `httrack` command and parser are unchanged.

The companion opens or initializes the same SQLite catalog and reports its schema version. Schema version 4 adds soft source removal and three frozen membership tables: selected collections, selected posts, and selected media variants. Creating a selection copies ordered identifiers into these tables in one transaction. A selection does not change during later source synchronization. `selection refresh` is the explicit operation that replaces its post and media membership while retaining its selected collections.

Source removal is a recoverable soft delete and requires `--yes`. It neither removes catalog history nor touches downloaded paths. Enabling a removed source restores it.

Human output is the default. `--json` emits one stable JSON value to stdout; progress, warnings, cancellation, and errors go to stderr. Credential references may be accepted for configuration, but neither their names nor secret values are emitted by inspection or error output.

Synchronization remains metadata-only. The Yotsuba adapter is reached through the Phase 2 metadata transport and never requests `i.4cdn.org`. The CLI's production wrapper uses libcurl only to implement that transport boundary; it does not add another networking stack. Offline integration tests use the deterministic fixture transport. Live access remains opt-in through explicit source configuration.

Path previews use ICU NFKC normalization followed by a conservative cross-platform sanitizer. Separators, traversal components, control characters, Windows-reserved names, trailing dots/spaces, component length, and total path length are handled before deterministic collision suffixes are assigned. The layout is:

`source/board/collection-id — sanitized-slug/0001-post-id-sanitized-original-name.ext`

The preview is a manifest-in-waiting only. It does not create directories or download attachment bodies.

## Consequences

- Existing HTTrack command-line behavior is isolated from catalog evolution.
- Frozen selections are auditable and cannot silently expand after a sync.
- Soft removal keeps historical metadata and any downloaded files recoverable.
- ICU and libcurl join SQLite as requirements when the optional imageboard catalog is enabled.
- Future download phases can consume the ordered selection tables and the exact path policy without redefining user intent.
