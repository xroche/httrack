#!/bin/bash
#
# automake TEST_LOG_COMPILER: run one .test under a wall-clock budget. A wedged
# test otherwise hangs the whole job until CI cancels it, and a cancelled step
# keeps neither its log nor its artifacts -- so nobody ever learns which test
# hung. On expiry this names the test, dumps what is still running, and exits
# 124, which makes the step FAIL instead: logs survive a failure.
#
# Output goes to the test's own tests/NN_*.log (automake redirects us there) and
# from there into test-suite.log, which CI prints.

set -u

testdir=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=tests/testlib.sh
. "${testdir}/testlib.sh"

# 600s is what the Windows leg already bounded each test at, and it clears the
# 540s that local-crawl.sh's own watchdogs tolerate for a three-pass crawl
# (CRAWL_DEADLINE, 180s a pass) -- budget below that and a slow-but-legitimate
# run would be killed. The slowest healthy test measures 39s. A non-numeric or
# absurd value falls back; 0 disables the guard, for use under a debugger.
budget=${HTTRACK_TEST_TIMEOUT:-600}
case "$budget" in
'' | *[!0-9]*) budget=600 ;;
esac
test "$budget" -gt 0 || exec "$BASH" "$@"

# The test script is the last argument; automake passes no others today.
for name in "$@"; do :; done
name=$(basename "$name")

# Give the test its own TMPDIR, so the hang dump can salvage exactly this test's
# crawl logs instead of racing (and deleting) a sibling's under "make check -j".
tmproot=${TMPDIR:-/tmp}
if mkdir -p "$tmproot/ht.$$" 2>/dev/null; then
    TMPDIR="$tmproot/ht.$$"
    export TMPDIR
    trap 'rm -rf "$TMPDIR" 2>/dev/null || true' EXIT
fi

windows=
is_windows && windows=1

had_m=
case "$-" in *m*) had_m=1 ;; esac
test -n "$windows" || set -m # own process group, so kill_tree can signal the group
"$BASH" "$@" &
pid=$!
test -n "$had_m" || test -n "$windows" || set +m

# Poll, because bash cannot wait with a deadline and a watchdog subshell would
# have to signal across process groups, which MSYS cannot do. The interval is the
# latency this adds to every healthy test, so keep it small where fork is cheap;
# under MSYS fork costs tens of milliseconds, and a second is what the Windows
# suite already paid before this wrapper existed.
if test -n "$windows"; then
    tick=1 per_sec=1
else
    tick=0.1 per_sec=10
fi

ticks=0
while kill -0 "$pid" 2>/dev/null; do
    if test "$((ticks / per_sec))" -ge "$budget"; then
        dump_hang_diagnostics "$pid" "$name" "$budget"
        kill_tree "$pid"
        reap_bounded "$pid" || echo "hang: the tree outlived the kill; see the process list above"
        # After the kill, so a crawl log holds the backtrace its engine just wrote.
        dump_crawl_logs
        exit 124
    fi
    sleep "$tick"
    ticks=$((ticks + 1))
done
wait "$pid"
