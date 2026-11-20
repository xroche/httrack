#!/bin/bash
# Drive every built harness against its seed corpus.
#   run-fuzzers.sh <build-fuzz-dir> check       deterministic replay (CI smoke)
#   run-fuzzers.sh <build-fuzz-dir> [seconds]   timed mutation run (discovery)
# Replay is crash/leak-only and never mutates; the per-unit -timeout guards
# both modes against pathological slowdowns (e.g. pre-#501 strjoker).
set -euo pipefail

srcdir=$(cd "$(dirname "$0")" && pwd)
bld=${1:?usage: run-fuzzers.sh <build-fuzz-dir> [check|seconds]}
mode=${2:-20}

# libFuzzer raises its length limit from the largest seed only after long
# stretches with no new coverage, so a slow target stays short of what its
# transforms need (past 2048 bytes for fuzz-htsparse). -len_control=0 takes the
# limit at once but spends the short-input budget, so only the two targets
# measured as needing it get the flags.
fuzz_len_flags() {
    case "$1" in
    fuzz-htsparse | fuzz-cachendx)
        echo "-max_len=16384 -len_control=0"
        ;;
    esac
}

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
    # word-split on purpose: empty for a target that keeps libFuzzer's defaults
    # shellcheck disable=SC2046
    if ! "$f" -max_total_time="$mode" -timeout=25 -rss_limit_mb=2048 \
        $(fuzz_len_flags "$name") \
        -artifact_prefix="$work/" -print_final_stats=1 "${args[@]}"; then
        echo "*** $name FAILED; artifacts:" >&2
        ls -l "$work" >&2
        status=1
    else
        rm -rf "$work"
    fi
done
exit $status
