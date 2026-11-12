#!/bin/sh
# Print why the suite failed, falling back to per-test logs when a step timeout
# left no test-suite.log.
#
# Usage: ci-test-log.sh [TEST_DIR], TEST_DIR defaulting to tests. Always exits 0.
set -eu

dir=${1:-tests}
if [ -f "$dir/test-suite.log" ]; then
    cat "$dir/test-suite.log"
    exit 0
fi
echo "no $dir/test-suite.log, so make check did not finish"
for trs in "$dir"/*.trs; do
    [ -f "$trs" ] || continue
    case $(sed -n 's/^:test-result: //p' "$trs" || true) in
    *FAIL* | *ERROR*)
        echo "=== ${trs%.trs}.log"
        cat "${trs%.trs}.log" 2>/dev/null || true
        ;;
    esac
done
# The driver writes a test's .trs only once the test ends.
for log in "$dir"/*.log; do
    [ -f "$log" ] || continue
    [ -f "${log%.log}.trs" ] && continue
    case ${log##*/} in
    [0-9]*_*.log)
        echo "=== still running: $log"
        tail -n 50 "$log" || true
        ;;
    esac
done
exit 0
