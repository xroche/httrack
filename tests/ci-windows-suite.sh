#!/bin/bash
#
# Drives the offline test suite on the Windows runner (per-test budget, suite
# deadline, engine reaping, skip gate, wedge watchdog #795). $1 is the directory
# holding the built httrack.exe. Sourcing defines the helpers and drives nothing.

testdir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=tests/testlib.sh
. "$testdir/testlib.sh"
# shellcheck source=tests/proclib.sh
. "$testdir/proclib.sh"

# Emit one GitHub annotation at level $1. The runner keeps only the first 10 of
# each level per step and drops the rest silently, so the level is a budget: pick
# one the step does not spend elsewhere. Led by a newline, since a command is only
# read at a line head. sed/awk, not ${v//p/r}: bash 3.2 cannot parse $'..' inside one.
ci_annotate() {
    local level=$1 title=$2 msg
    msg=$(printf '%s' "$3" | tr -d '\r' | sed 's/%/%25/g' |
        awk 'NR > 1 { printf "%%0A" } { printf "%s", $0 }' || true)
    printf '\n::%s title=%s::%s\n' "$level" "$title" "$msg"
}

# End a wedged suite before its runner dies: a step that fails on its own terms
# keeps its log, a lost runner keeps nothing, annotations included (#795). Quiet
# for $1s, then names the test in flight from $3 every $2s; kills $5 once $3 has
# been static for $4s. Staticness, never elapsed time: a healthy test in flight and
# a wedged one look identical by the clock, but every outcome writes a line, the
# per-test timeout included, so $4 past that timeout means it never fired.
# Assigns into hb_time rather than printing: reading the clock forks nothing, which
# matters when the box has none to spare. Overridable for the unit test virtual clock.
hb_time=0
hb_now() { hb_time=$SECONDS; }

# Start the off-box telemetry over the progress log $1, setting ci_watchdog_pid;
# return 1 with no PowerShell available. Forks nothing and kills nothing, so it
# reports where the heartbeat below cannot (#795).
ci_start_native_watchdog() {
    local progress=$1 ps1 c exe='' waited=0
    ps1="$testdir/ci-windows-watchdog.ps1"
    test -r "$ps1" || return 1
    for c in pwsh powershell.exe; do
        if command -v "$c" >/dev/null 2>&1; then
            exe=$c
            break
        fi
    done
    test -n "$exe" || return 1
    # Never the step's stdout: a background holder of that pipe keeps the step
    # open past the suite (#949), and tests/*.log reaches the artifact anyway.
    : >watchdog.log
    WATCHDOG_TOKEN="${ci_watchdog_token:-}" \
        "$exe" -NoProfile -NonInteractive -ExecutionPolicy Bypass \
        -File "$(nativepath "$ps1")" \
        -ProgressLog "$(nativepath "$progress")" \
        >>watchdog.log 2>&1 &
    ci_watchdog_pid=$!
    # $! comes from the fork, not the exec: wait for it to actually speak.
    while test "$waited" -lt 30; do
        grep -q '^watchdog ready$' watchdog.log && return 0
        kill -0 "$ci_watchdog_pid" 2>/dev/null || break
        sleep 1
        waited=$((waited + 1))
    done
    kill_pid "$ci_watchdog_pid"
    ci_watchdog_pid=''
    return 1
}

ci_suite_heartbeat() {
    local quiet=$1 every=$2 progress=$3 stuck=$4 main=$5
    local tick=$2 begin now line said moved last=''
    # Measured, never accumulated: the starvation this watchdog exists to catch is
    # exactly what makes a sleep overshoot, and drift only ever delays the kill.
    test "$tick" -le 30 || tick=30
    hb_now
    begin=$hb_time said=$begin moved=$begin
    while :; do
        # Guarded: a tick that cannot exec returns 127, and under the caller's errexit a
        # bare failure would end the watchdog in silence (#1038).
        sleep "$tick" >/dev/null 2>&1 || : # holds no stdout: the caller's trap orphans it
        hb_now
        now=$hb_time
        # Guarded: under the caller's errexit a bare substitution assignment would
        # end the watchdog in silence, which reads as protection and is not.
        line=$(tail -n 1 "$progress" 2>/dev/null || true)
        test "$line" = "$last" || { last=$line moved=$now; }
        test $((now - begin)) -ge "$quiet" || continue
        # Every tick, not on the annotation cadence, which would let a late wedge
        # outlive the step's own timeout before being caught.
        if test $((now - moved)) -ge "$stuck"; then
            ci_annotate error "suite watchdog" "killing the step: $((now - moved))s without progress, in flight: $last"
            # Ahead of the kill, which runs no EXIT trap: an orphan would outlive the
            # step and overwrite its last status with a frozen tail.
            test -z "${watchdog:-}" || kill_pid "$watchdog"
            # Direct first: kill_tree may reap this watchdog before its own root (#953).
            kill_pid "$main"
            kill_tree "$main"
            return 0
        fi
        test $((now - said)) -ge "$every" || continue
        said=$now
        # notice, not warning: reap_leftover_processes spends the warning budget.
        ci_annotate notice "suite still running" "$(
            printf '%ss elapsed, %ss without progress, in flight: %s\n' \
                "$((now - begin))" "$((now - moved))" "$last"
            list_stray_processes 0 named | head -n 8
        )"
    done
}

# Only a direct run drives a suite. Asked of the shell, not derived from $0,
# which a caller can set to this very path (172_ci-windows-driver.test).
(return 0 2>/dev/null) && return 0

# Explicit now that this is a script; GitHub's "shell: bash" gave the step both.
set -euo pipefail
bin=${1:?usage: ci-windows-suite.sh <bindir>}
export PATH="$bin:$PATH"
command -v httrack >/dev/null || {
    echo "::error::no httrack.exe in $bin"
    exit 1
}

# Out of the environment before the first child forks below: an inherited token
# could post a commit status in the name of the run.
ci_watchdog_token=${WATCHDOG_TOKEN:-}
unset WATCHDOG_TOKEN

# httrack.exe is native, so MSYS rewrites any argument shaped like a
# POSIX path, and a URL path is shaped exactly like one: "/a/b.html"
# reached the engine as "C:/Program Files/Git/a/b.html". Switch that
# off, and hand the tests a TMPDIR that is already a Windows path.
export MSYS_NO_PATHCONV=1
export MSYS2_ARG_CONV_EXCL='*'
TMPDIR="$(cygpath -m "$RUNNER_TEMP")"
export TMPDIR

# Mirror what configure hands the suite. LC_ALL sets the codeset MSYS maps
# a UTF-8 mirror name onto UTF-16 with, which the intl crawls "test -f".
export HTTPS_SUPPORT=yes BROTLI_ENABLED=yes ZSTD_ENABLED=yes
export LC_ALL=C.UTF-8

# A wedged crawl must not eat the job's timeout budget. timeout(1)'s
# signals can't reap a native httrack.exe (MSYS signals don't reach it),
# so a hang orphaned processes that starved the runner; run_with_timeout
# TerminateProcess-es the whole tree. 600s is unchanged: it clears the
# 540s a three-pass crawl may legitimately take under local-crawl.sh's
# own watchdogs, against a slowest healthy test here of 39s.
per_test=600

# The whole suite must give up before the workflow's 45-minute step
# timeout. A cancelled step keeps neither its log nor the artifacts the
# later if:always() steps would upload, so an overrun that ends in a
# cancel tells us nothing; failing on our own terms keeps both. Healthy
# runs take 13 min. The check sits between tests, so the step can still
# reach 25 min plus one per-test budget, inside the 45.
suite_deadline=1500
started=$SECONDS

# Reaches the artifact whenever the step ends on its own terms, which the
# tail of its log may not. Absolute, since a .test may have chdir'd.
progress=suite-progress.log
: >"$progress"
export HTTRACK_PROGRESS_LOG="$PWD/$progress"

# A dying runner takes both, and every annotation with them (measured, see
# #795), so the watchdog's job is to end the step first: one that fails on
# its own terms keeps its log. Quiet past 16 min, clear of the 13 a healthy
# run measures here, and a kill 900s after the last progress line clears
# the longest legitimate gap, one $per_test.
stuck=900
# Orthogonal to that heartbeat rather than a spare of it: the heartbeat needs
# 960s of quiet and 900s of static log, and every #795 death measured so far
# lands inside the first 750s of the step.
watchdog='' ci_watchdog_pid=''
ci_start_native_watchdog "$PWD/$progress" && watchdog=$ci_watchdog_pid
test -n "$watchdog" || echo "no off-box watchdog: no usable PowerShell"
ci_suite_heartbeat 960 360 "$progress" "$stuck" $$ &
heartbeat=$!
trap 'set +e; kill "$heartbeat" 2>/dev/null; test -z "$watchdog" || kill_pid "$watchdog"' EXIT

pass=0 fail=0 skip=0 failed="" skipped="" deadline=0
# label:pattern, globbed rather than enumerated so a new NNN_engine-*.test or
# NNN_local-*.test is picked up instead of silently getting zero coverage. Every
# entry carries a metacharacter, or nullglob cannot empty it and the gate below
# has nothing to catch.
# testlib and crawllib cover what most tests here rest on.
categories=(runnable:'00_runnable*.test' engine:'*_engine-*.test' zlib:'*_zlib-*.test'
    local:'*_local-*.test' watchdog:'*_watchdog*.test'
    testlib:'*_testlib-*.test' crawllib:'*_crawllib*.test'
    proxy-https:'*_crawl_proxy_https.test' log-salvage:'*_crawl-log-salvage.test')
tests=()
shopt -s nullglob
for c in "${categories[@]}"; do
    # shellcheck disable=SC2206 # expanding the pattern is the point
    matched=(${c#*:})
    # Named, and before anything runs: left unexpanded the pattern reaches
    # test-timeout.sh literally and is counted as a test failing 127 (#952).
    test -n "${matched[0]:-}" || {
        echo "::error::test category ${c%%:*} matched no tests (${c#*:})"
        exit 1
    }
    tests+=("${matched[@]}")
done
shopt -u nullglob

# The one test deliberately failing a crawl, which wedged this leg twice at its
# step timeout (#1126). Its name is what keeps it out of the globs above, so the
# name is asserted: renaming it into one of them must red here, not on the runner.
wedged=260_crawl-harness-fails-loudly.test
test -e "$wedged" || {
    echo "::error::$wedged is gone; a rename must stay clear of the globs above (#1126)"
    exit 1
}

for t in "${tests[@]}"; do
    elapsed=$((SECONDS - started))
    if [ "$elapsed" -ge "$suite_deadline" ]; then
        echo "::error::suite deadline: ${elapsed}s elapsed, stopping before $t"
        echo "DEADLINE before $t after ${elapsed}s" >>"$progress"
        # Per-test start times, so the slow ones are named rather than guessed.
        sed 's/^/      /' "$progress"
        deadline=1
        break
    fi
    echo "RUN $t at ${elapsed}s" >>"$progress"
    rc=0
    # Same guard "make check" uses on POSIX, so a wedge is diagnosed the
    # same way on every platform. It dumps before it kills, which a bare
    # run_with_timeout cannot: by the time that returns, the tree whose
    # stack we wanted is already gone.
    HTTRACK_TEST_TIMEOUT=$per_test bash ./test-timeout.sh "$t" >"$t.log" 2>&1 || rc=$?
    case "$rc" in
    0)
        pass=$((pass + 1))
        echo "PASS $t"
        ;;
    77)
        skip=$((skip + 1)) skipped="$skipped $t"
        echo "SKIP $t"
        ;;
    124)
        fail=$((fail + 1)) failed="$failed $t"
        # test-timeout.sh has already written the process list, the stacks
        # and the killed crawl's own logs into $t.log.
        echo "FAIL $t (timed out, tree killed)"
        tail -n 25 "$t.log" | sed 's/^/      /'
        ;;
    *)
        fail=$((fail + 1)) failed="$failed $t"
        echo "FAIL $t (exit $rc)"
        # These assert with `test "$(...)" == "..." || exit 1`, which
        # says nothing at all on failure. Re-run traced, still bounded.
        # Noted first, or a slow trace reads as a wedge to the watchdog,
        # which would then kill the step in the middle of writing it.
        echo "RERUN $t" >>"$progress"
        run_with_timeout "$per_test" bash -x "$t" >>"$t.log" 2>&1 || true
        tail -n 25 "$t.log" | sed 's/^/      /'
        ;;
    esac
    echo "$rc $t" >>"$progress"
    # An orphaned native httrack.exe spins and starves the runner, which
    # is how this job dies with "lost communication" rather than a plain
    # timeout. Clear them between tests and name whoever leaked them.
    reap_leftover_processes "$t" | tee -a "$progress"
done
echo "ran=$((pass + fail + skip)) pass=$pass fail=$fail skip=$skip" |
    tee -a "$GITHUB_STEP_SUMMARY"

# Every gate here exits 77, so an all-skipped suite would report green having
# tested nothing: pin the skips, and floor the passes in case the glob empties.
# One name per line, so two branches each appending one don't collide on the
# same line; compared as a sorted set below, so glob discovery order can't
# cause a false mismatch either.
# footer-overflow and purge-longpath skip on Windows (need a path past MAX_PATH);
# webdav-default and proxytrack-quiet read proxytrack's console through a pty,
# which Windows Python does not build;
# badmtime needs a filesystem that stores an mtime past gmtime's range;
# single-file-gui drives htsserver, which this job does not build;
# update-304-leak needs a LeakSanitizer build, which MSVC has no equivalent of;
# crash-symbolize and backtrace-empty need backtrace(), which Windows has no
# equivalent of;
# string-oom drives a helper binary that only the automake build produces;
# datadir-ospath copies the unwrapped binary the automake build leaves in .libs,
# and needs the loader variable libtool picked, neither of which this job has;
# link-control-bytes names its fixtures with the raw control bytes the requests
# decode back to, which NTFS refuses;
# memresume, repaircache and resume-recovery interrupt pass 1 with a signal
# MSYS cannot deliver to a native exe;
# ftp-deadhost-interrupt and ftp-sigterm need that same signal (deadhost's
# --timeout half runs, as 245);
# close-once interposes close() through LD_PRELOAD, which MSYS has no equivalent for.
expected_skips="01_engine-footer-overflow.test
253_local-ftp-close-once.test
100_local-purge-longpath.test
158_local-link-control-bytes.test
114_local-update-304-leak.test
120_local-proxytrack-webdav-default.test
143_engine-backtrace-empty.test
152_engine-string-oom.test
153_local-proxytrack-quiet.test
215_engine-datadir-ospath.test
243_local-ftp-deadhost-interrupt.test
255_local-ftp-sigterm.test
235_local-resume-recovery.test
48_local-crange-memresume.test
71_local-crange-repaircache.test
80_engine-crash-symbolize.test
88_local-proxytrack-badmtime.test
241_local-single-file-gui.test"
# First, or the deadline reads as an unexplained shortfall in the gates below.
[ "$deadline" -eq 0 ] || {
    echo "::error::suite did not finish within ${suite_deadline}s"
    exit 1
}
[ "$pass" -ge 90 ] || {
    echo "::error::only $pass tests passed ($skip skipped)"
    exit 1
}
# Word-split on whitespace (space-joined $skipped, newline-joined
# expected_skips both work) and sort, so the compare is a set, not a string.
# shellcheck disable=SC2086 # the splitting is what makes it a set
got=$(printf '%s\n' $skipped | sort)
# shellcheck disable=SC2086
want=$(printf '%s\n' $expected_skips | sort)
if [ "$got" != "$want" ]; then
    echo "::error::skip set changed from expected; - missing, + newly skipped"
    diff -u <(echo "$want") <(echo "$got") | tail -n +3 | sed 's/^/      /'
    exit 1
fi
[ "$fail" -eq 0 ] || {
    echo "::error::failing:$failed"
    exit 1
}
