#!/bin/bash
# Drive every built harness against its seed corpus.
#   run-fuzzers.sh <build-fuzz-dir> check       deterministic replay (CI smoke)
#   run-fuzzers.sh <build-fuzz-dir> [seconds]   timed mutation run (discovery)
# Replay is crash/leak-only and never mutates, so it can't hit strjoker's
# catastrophic backtracking; the timed mode can, hence the per-unit -timeout.
set -euo pipefail

srcdir=$(cd "$(dirname "$0")" && pwd)
bld=${1:?usage: run-fuzzers.sh <build-fuzz-dir> [check|seconds]}
mode=${2:-20}

status=0
for f in "$bld"/fuzz-*; do
    if [ ! -f "$f" ] || [ ! -r "$f" ]; then continue; fi
    case "$f" in *.o | *.c | *.dSYM) continue ;; esac
    name=$(basename "$f")
    corpus="$srcdir/corpus/${name#fuzz-}"
    if [ "$mode" = "check" ]; then
        echo "=== $name (replay) ==="
        [ -d "$corpus" ] || continue
        if ! "$f" -runs=0 -timeout=25 -rss_limit_mb=2048 "$corpus"; then
            echo "*** $name FAILED on its corpus" >&2
            status=1
        fi
        continue
    fi
    work=$(mktemp -d)
    args=("$work")
    [ -d "$corpus" ] && args+=("$corpus")
    echo "=== $name (${mode}s) ==="
    if ! "$f" -max_total_time="$mode" -timeout=25 -rss_limit_mb=2048 \
        -artifact_prefix="$work/" -print_final_stats=1 "${args[@]}"; then
        echo "*** $name FAILED; artifacts:" >&2
        ls -l "$work" >&2
        status=1
    else
        rm -rf "$work"
    fi
done
exit $status
