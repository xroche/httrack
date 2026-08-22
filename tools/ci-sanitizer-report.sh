#!/bin/sh
# Surface sanitizer findings the test harness would otherwise swallow: automake
# keeps only failing tests in test-suite.log, so an abort inside a script that
# lacks `set -e` leaves no trace in a green run. Both sources are needed, since
# the log_path files carry a report whose stderr a test redirected away, and the
# per-test logs carry gcc's UBSan, which writes to stderr and ignores log_path.
#
# Usage: ci-sanitizer-report.sh LOG_PATH_DIR [TEST_LOG_DIR...]; exits 1 on a hit.
set -eu

# Anchored on each runtime's banner: severity alone is not the signal. MSan
# reports findings as WARNING, while ASan uses WARNING for the over-large
# allocation 237_engine-arrays has it refuse on purpose; and the engine's own
# "detected memory leaks" message is not LeakSanitizer's.
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
# diagnostic stays visible without turning a deliberate one into a red build.
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

for dir in "$@"; do
    [ -d "$dir" ] || continue
    for f in $(grep -rlE "$pattern" "$dir" 2>/dev/null || true); do
        report "sanitizer output in a test log" "$f"
    done
done

[ "$found" -eq 0 ] || {
    echo "FAIL: a sanitizer reported above; a passing test can still hide one." >&2
    exit 1
}
echo "No sanitizer finding in $log_dir or the per-test logs."
