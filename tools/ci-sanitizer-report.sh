#!/bin/sh
# Surface sanitizer findings automake swallows in a green run: it keeps only the
# failing tests in test-suite.log, and most test scripts lack `set -e`.
# Each source covers what the others cannot: the log_path file holds the report a
# test redirecting its own stderr hides, and the stderr copies hold gcc's UBSan
# findings, which land there because it ignores log_path, unlike clang's.
#
# Usage: ci-sanitizer-report.sh [-s STDERR_DIR] LOG_PATH_DIR [TEST_LOG_DIR...];
# exits 1 on a hit.
set -eu

# Anchored on each runtime's banner: severity is not enough, since MSan reports
# findings as WARNING and ASan uses WARNING for 237_engine-arrays' deliberate
# over-large allocation. The engine's own "detected memory leaks" message is
# also not LeakSanitizer's.
pattern='ERROR: (Address|Leak|Memory|Thread|UndefinedBehavior)Sanitizer|WARNING: (Memory|Thread)Sanitizer|runtime error:|SUMMARY: .*Sanitizer|ERROR: libFuzzer|DEADLYSIGNAL'

# A directory the caller named but that is not there means the wiring moved, not
# that the run was clean; scanning nothing must never read as finding nothing.
need_dir() { # need_dir WHAT PATH
    [ -d "$2" ] || {
        echo "$0: $1: not a directory: $2" >&2
        exit 2
    }
}

stderr_dir=
while [ $# -gt 0 ]; do
    case $1 in
    -s)
        [ $# -ge 2 ] || {
            echo "usage: $0 [-s STDERR_DIR] LOG_PATH_DIR [TEST_LOG_DIR...]" >&2
            exit 2
        }
        stderr_dir=$2
        need_dir "-s" "$stderr_dir"
        shift 2
        ;;
    *) break ;;
    esac
done

log_dir=$1
need_dir "LOG_PATH_DIR" "$log_dir"
shift
found=0

report() { # report LABEL FILE
    found=1
    echo "=== $1: $2"
    cat "$2"
}

scan() { # scan LABEL FILE -- report FILE only if it carries a finding
    [ -f "$2" ] || return 0
    grep -qE "$pattern" "$2" 2>/dev/null && report "$1" "$2"
    return 0
}

# Print every log_path file, but fail only on one that matches: an unrecognised
# diagnostic stays visible, and a deliberate one does not turn the build red.
for f in "$log_dir"/*; do
    [ -f "$f" ] || continue
    if grep -qE "$pattern" "$f" 2>/dev/null; then
        report "sanitizer report" "$f"
    else
        echo "=== other sanitizer output (not a finding): $f"
        cat "$f"
    fi
done

# Copies of the engine's stderr, kept by tests/stderrwrap.c. Never
# printed whole: these also hold the engine's ordinary chatter.
if [ -n "$stderr_dir" ]; then
    for f in "$stderr_dir"/*.log; do
        [ -s "$f" ] || {
            rm -f "$f"
            continue
        }
        scan "engine stderr" "$f"
    done
fi

# Only the harness logs: an in-tree build puts the .test sources here too, and
# one of them carries the pattern as literal text.
for dir in "$@"; do
    [ -d "$dir" ] || continue
    for f in "$dir"/*.log; do
        scan "sanitizer output in a test log" "$f"
    done
done

[ "$found" -eq 0 ] || {
    echo "FAIL: a sanitizer reported above, which \`make check\` alone did not catch." >&2
    exit 1
}
echo "No sanitizer finding in $log_dir${stderr_dir:+, $stderr_dir} or the per-test logs."
