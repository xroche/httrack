#!/bin/sh
# Surface sanitizer findings automake swallows in a green run: it keeps only the
# failing tests in test-suite.log, and most test scripts lack `set -e`.
# Both sources matter. A test that redirects its own stderr hides its report
# everywhere but the log_path file, and gcc's UBSan writes to stderr and ignores
# log_path, unlike clang's.
#
# Usage: ci-sanitizer-report.sh LOG_PATH_DIR [TEST_LOG_DIR...]; exits 1 on a hit.
set -eu

# Anchored on each runtime's banner: severity is not enough, since MSan reports
# findings as WARNING and ASan uses WARNING for 237_engine-arrays' deliberate
# over-large allocation. The engine's own "detected memory leaks" message is
# also not LeakSanitizer's.
pattern='ERROR: (Address|Leak|Memory|Thread|UndefinedBehavior)Sanitizer|WARNING: (Memory|Thread)Sanitizer|runtime error:|SUMMARY: .*Sanitizer|ERROR: libFuzzer|DEADLYSIGNAL'

log_dir=$1
shift
found=0

report() { # report LABEL FILE
    found=1
    echo "=== $1: $2"
    cat "$2"
}

# Print every log_path file, but fail only on one that matches: an unrecognised
# diagnostic stays visible, and a deliberate one does not turn the build red.
if [ -d "$log_dir" ]; then
    for f in "$log_dir"/*; do
        [ -f "$f" ] || continue
        if grep -qE "$pattern" "$f" 2>/dev/null; then
            report "sanitizer report" "$f"
        else
            echo "=== other sanitizer output (not a finding): $f"
            cat "$f"
        fi
    done
fi

# Only the harness logs: an in-tree build puts the .test sources here too, and
# one of them carries the pattern as literal text.
for dir in "$@"; do
    [ -d "$dir" ] || continue
    for f in "$dir"/*.log; do
        [ -f "$f" ] || continue
        grep -qE "$pattern" "$f" 2>/dev/null &&
            report "sanitizer output in a test log" "$f"
    done
done

[ "$found" -eq 0 ] || {
    echo "FAIL: a sanitizer reported above, which \`make check\` alone did not catch." >&2
    exit 1
}
echo "No sanitizer finding in $log_dir or the per-test logs."
