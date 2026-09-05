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
# shellcheck source=tests/proclib.sh
. "${testdir}/proclib.sh"

# 600s is what the Windows leg already bounded each test at, and it clears the
# 540s that local-crawl.sh's own watchdogs tolerate for a three-pass crawl
# (CRAWL_DEADLINE, 180s a pass) -- budget below that and a slow-but-legitimate
# run would be killed. The slowest healthy test measures 39s. A non-numeric or
# absurd value falls back; 0 disables the guard, for use under a debugger.
budget=$(budget_secs)

# The test script is the last argument; automake passes no others today.
for path in "$@"; do :; done
name=$(basename "$path")
# The runner sources this path, and a dot-command with no slash in it is a PATH lookup.
case $path in */*) ;; *) path=./$path ;; esac

# gcc's UBSan ignores log_path and reports on stderr, which most tests discard.
# Route the engine through the shims that keep a copy (tests/stderrwrap.c)
# when a capture directory is named; PATH is untouched otherwise, so no plain run
# pays for the extra layer.
if test -n "${HTTRACK_STDERR_CAPTURE_DIR:-}" && test -d "${HTTRACK_STDERR_WRAP_DIR:-}"; then
    PATH="$HTTRACK_STDERR_WRAP_DIR${PATH:+:}$PATH"
    HTTRACK_STDERR_CAPTURE_TAG=$name
    export PATH HTTRACK_STDERR_CAPTURE_TAG
fi

# A test whose work legitimately outlasts the wedge budget says so in its header
# (269 sweeps n^2 compiles and paces itself inside it). The name carries the rule the
# reader cannot see: it raises the budget, so no test can disarm the guard. Read with
# the shell to keep it off the per-test fork bill, and bounded, since bash's `test`
# errors rather than compares past intmax and would leave the guard unarmed.
if test "$budget" -gt 0 && test -r "$path"; then
    read_lines=0
    while test "$read_lines" -lt 40 && IFS= read -r line; do
        read_lines=$((read_lines + 1))
        case "$line" in
        '# TEST_TIMEOUT_AT_LEAST: '*)
            want=${line#'# TEST_TIMEOUT_AT_LEAST: '}
            case "$want" in
            '' | *[!0-9]* | ???????*) ;;
            *) test "$((10#$want))" -le "$budget" || budget=$((10#$want)) ;;
            esac
            break
            ;;
        esac
    done <"$path"
fi

# Exported so a test can pace itself against the same number (skip_if_out_of_budget)
# instead of being killed halfway.
export HTTRACK_TEST_TIMEOUT="$budget"
# No marker with the guard off: nothing survives the exec to read one.
test "$budget" -gt 0 || exec "$BASH" "$@"

# Give the test its own TMPDIR, so the hang dump can salvage exactly this test's
# crawl logs instead of racing (and deleting) a sibling's under "make check -j".
# Trailing slash dropped: macOS's TMPDIR carries one, and the "//" it would
# glue in is a path no test can compare against what the engine writes.
tmproot=${TMPDIR:-/tmp}
tmproot=${tmproot%/}
if mkdir -p "$tmproot/ht.$$" 2>/dev/null; then
    TMPDIR="$tmproot/ht.$$"
    export TMPDIR
    trap 'rm -rf "$TMPDIR" 2>/dev/null || true' EXIT
    TESTLIB_DONE_FILE="$TMPDIR/.testlib-done"
else
    TESTLIB_DONE_FILE="$tmproot/ht.done.$$"
    trap 'rm -f "$TESTLIB_DONE_FILE" 2>/dev/null || true' EXIT
fi
export TESTLIB_DONE_FILE
rm -f "$TESTLIB_DONE_FILE" 2>/dev/null || true

# bash before 4.2 patch 23 forgets the exit status when an EXIT trap runs after a
# fatal expansion error under errexit, so an aborted test reports 0 and automake
# records a PASS. macOS ships 3.2.57. So the test marks its own end, and a zero
# status with no mark is an abort. Sourcing it from `bash -c` keeps $0 the test's
# own path, which many tests derive $testdir from, writes nothing beside a
# read-only srcdir and leaves stdin alone. A subshell's exit is not the test's.
# Two shapes would lose the mark and fail a healthy test: an `exec` at top level,
# and removing the driver's TMPDIR before exiting. No test does either today.
# shellcheck disable=SC2016 # the child expands it, not us
TESTLIB_RUNNER='exit() {
    if test "$BASH_SUBSHELL" -eq 0; then : >"$TESTLIB_DONE_FILE" 2>/dev/null || true; fi
    builtin exit ${1+"$1"}
}
. "$0"
TESTLIB_RUNNER_ST=$?
: >"$TESTLIB_DONE_FILE" 2>/dev/null || true
builtin exit "$TESTLIB_RUNNER_ST"'

# Everything before the test path is an option for the shell that runs it.
runopts=()
while test "$#" -gt 1; do
    runopts+=("$1")
    shift
done

# Two questions, and they part company under the wsl2 backend: the shell there is
# Linux while the binary under test is still a native .exe.
msys_shell=
shell_is_msys && msys_shell=1
windows=
target_is_windows && windows=1

had_m=
case "$-" in *m*) had_m=1 ;; esac
test -n "$msys_shell" || set -m # own process group, so kill_tree can signal the group
"$BASH" ${runopts[@]+"${runopts[@]}"} -c "$TESTLIB_RUNNER" "$path" &
pid=$!
test -n "$had_m" || test -n "$msys_shell" || set +m
# Read while the test is certainly alive: by kill time /proc/<pid>/winpid is gone.
winpid=''
test -z "$windows" || winpid=$(win_pid "$pid")

# Poll, because bash cannot wait with a deadline and a watchdog subshell would
# have to signal across process groups, which MSYS cannot do. The interval is the
# latency this adds to every healthy test; the ceiling only matters where poll_wait
# has to fall back to a forked sleep, which under MSYS costs tens of milliseconds.
if test -n "$msys_shell"; then
    tick=1
else
    tick=0.1
fi

# Wall clock, not a tick count: under starvation an iteration costs far more than
# $tick, so a counted deadline never arrives. Strictly greater, because $SECONDS
# is floored: a reading of $budget can be a fraction under it, and firing early
# kills a healthy test.
start=$SECONDS
# A tick that never waits bounds nothing, and re-forking it each loop is a fork storm
# on a starved box (#1038). Counted, not tripped on the first tick.
nowait=0
nowait_limit=10
while kill -0 "$pid" 2>/dev/null; do
    if test "$((SECONDS - start))" -gt "$budget"; then
        # The dump below can run for minutes. Say so where a suite watchdog is
        # watching, or the silence reads as a wedge and the step dies mid-stack.
        # The elapsed time rides along because only the guard can measure it.
        test -z "${HTTRACK_PROGRESS_LOG:-}" ||
            echo "DUMP $name $((SECONDS - start))" >>"$HTTRACK_PROGRESS_LOG"
        dump_hang_diagnostics "$pid" "$name" "$budget"
        kill_tree "$pid" "$winpid"
        reap_bounded "$pid" || echo "hang: the tree outlived the kill; see the process list above"
        # After the kill, so a crawl log holds the backtrace its engine just wrote.
        dump_crawl_logs
        exit 124
    fi
    if poll_wait "$tick"; then
        nowait=0
        continue
    fi
    nowait=$((nowait + 1))
    test "$nowait" -ge "$nowait_limit" || continue
    # So the off-box watchdog can still see it: a dead runner leaves only its last
    # commit status behind (#795).
    test -z "${HTTRACK_PROGRESS_LOG:-}" || echo "NOFORK $name" >>"$HTTRACK_PROGRESS_LOG"
    echo "hang: the poll tick ran $nowait times without waiting; this box cannot start a process"
    kill_tree "$pid" "$winpid"
    # No reap_bounded here: it polls on the same broken tick, which is the spin
    # this branch exists to end.
    # The EXIT trap deletes TMPDIR, so a crawl log left undumped is a destroyed one.
    dump_crawl_logs
    exit 124
done
wait "$pid"
status=$?
if test "$status" -eq 0 && test ! -f "$TESTLIB_DONE_FILE"; then
    echo "abort: $name exited 0 without reaching its end; the shell above says why"
    status=1
fi
exit "$status"
