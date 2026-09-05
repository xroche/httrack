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

# Count the MSYS fork-emulation failures in the console log $1, and the workers the
# suite lost with them (#1273). A worker can die without bash saying a word, so those
# counts at zero are not by themselves a clean leg (#1352).
ci_report_fork_failures() {
    local log=$1 died retried gaveup lost
    test -r "$log" || {
        echo "no console log at $log: MSYS fork failures not counted"
        return 0
    }
    # Split on CR first: the console carries stdout and stderr merged, so two
    # messages can share a physical line and a line count would see one. bash
    # retries on EAGAIN alone, so a give-up can carry any strerror; each `next`
    # keeps one message out of a second class. The LOST verdicts sit at column 0,
    # where the suite's indented failure tails cannot forge one.
    read -r died retried gaveup lost <<<"$(tr '\r' '\n' <"$log" | awk '
        /^LOST / { lost++; next }
        /fork: child -1|child_info_fork::abort|sync_with_child:/ { died++; next }
        /reserve memory for parent stack/ { died++; next }
        /fork: retry:/ { retried++; next }
        /: fork: / { gaveup++ }
        END { print died + 0, retried + 0, gaveup + 0, lost + 0 }')"
    test -z "${GITHUB_STEP_SUMMARY:-}" ||
        printf 'MSYS forks: %s died, %s retried, %s given up; %s worker(s) lost\n' \
            "$died" "$retried" "$gaveup" "$lost" >>"$GITHUB_STEP_SUMMARY"
    if test $((died + retried + gaveup)) -eq 0; then
        # Never bare: "no MSYS fork failure" alone is what called the #1352 leg clean.
        if test "$lost" -eq 0; then
            echo "no MSYS fork failure and no lost worker in $log"
        else
            echo "no MSYS fork failure in $log, but $lost worker(s) left no status"
        fi
    else
        ci_annotate warning "MSYS fork failures" "$(
            printf '%s child(ren) died at DLL init, %s retry wait(s), %s fork(s) given up\n' \
                "$died" "$retried" "$gaveup"
            # -a: one NUL in the log would otherwise cost every sample line.
            tr '\r' '\n' <"$log" | grep -am 3 -e 'fork: child -1' -e 'sync_with_child:' \
                -e 'child_info_fork::abort' -e 'reserve memory' -e ': fork: ' || true
        )"
    fi
    # error, not warning: a lost worker is re-run where a fork storm is investigated.
    test "$lost" -eq 0 || ci_annotate error "workers lost with no status" "$(
        printf '%s worker(s) died before reporting: re-run this leg, and see #1228\n' "$lost"
        tr '\r' '\n' <"$log" | grep -am 3 '^LOST ' || true
    )"
}

# Classify test $1's outcome from what its worker left in results dir $2, into
# ci_outcome (pass, skip, fail, lost) and ci_rc. "lost" is a worker that died before
# writing any status: an infrastructure symptom, not a test result. Assigned, not
# printed: a fork per test costs tens of milliseconds under MSYS.
ci_rc='' ci_outcome=''
ci_read_outcome() {
    ci_rc=''
    # An empty .rc is a worker that died mid-write, never a status of 0.
    test ! -r "$2/$1.rc" || read -r ci_rc <"$2/$1.rc" || true
    case "$ci_rc" in
    0) ci_outcome=pass ;;
    77) ci_outcome=skip ;;
    '') ci_outcome=lost ;;
    *) ci_outcome=fail ;;
    esac
}

# How far test $1's vanished worker got, into ci_reason, read off the log it opened:
# wait -n reaps workers the pool can no longer name, so the wait status is gone.
ci_reason=''
ci_lost_reason() {
    if test ! -e "$1.log"; then
        ci_reason='no log at all: it died before opening one'
    elif test ! -s "$1.log"; then
        ci_reason='0-byte log: it died before its test wrote anything'
    else
        ci_reason='log written: it died after its test had run'
    fi
}

# End a wedged suite before its runner dies: a step that fails on its own terms
# keeps its log, a lost runner keeps nothing, annotations included (#795). Quiet
# for $1s, then names the test in flight from $3 every $2s; kills $5 once $3 has
# been static for $4s, or unconditionally $6s in (0: never). Staticness, never
# elapsed time: a healthy test in flight and a wedged one look identical by the
# clock, but every outcome writes a line, the per-test timeout included, so $4 past
# that timeout means it never fired. The cap covers what staticness cannot: each of
# those lines buys another $4s, so a tail of blown budgets walks the step onto the
# workflow timeout, which keeps neither the log nor the artifacts (#1126).
# Assigns into hb_time rather than printing: reading the clock forks nothing, which
# matters when the box has none to spare. Overridable for the unit test virtual clock.
hb_time=0
hb_now() { hb_time=$SECONDS; }

# Seconds the launch below waits for the marker. Overridable for the unit test.
ci_watchdog_wait=${ci_watchdog_wait:-30}

# True once the launched interpreter has announced itself.
ci_watchdog_spoke() { grep -q '^watchdog ready$' watchdog.log; }

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
    while test "$waited" -lt "$ci_watchdog_wait"; do
        ci_watchdog_spoke && return 0
        kill -0 "$ci_watchdog_pid" 2>/dev/null || break
        sleep 1
        waited=$((waited + 1))
    done
    # A child that spoke and exited mid-poll still launched (#1321).
    ci_watchdog_spoke && return 0
    kill_pid "$ci_watchdog_pid"
    ci_watchdog_pid=''
    return 1
}

# End the step, announcing $2 first: the kill runs no EXIT trap, so an unexplained
# death is all the log would otherwise hold.
ci_heartbeat_kill() {
    local main=$1 winpid winimage
    ci_annotate error "suite watchdog" "$2"
    # Ahead of the kill, which runs no EXIT trap: an orphan would outlive the
    # step and overwrite its last status with a frozen tail.
    test -z "${watchdog:-}" || kill_pid "$watchdog"
    # Read before the two kills below, which would leave the winpid naming
    # whoever Windows hands the number to next (#1228).
    win_capture "$main"
    winpid=$WIN_PID winimage=$WIN_IMAGE
    # Direct first: kill_tree may reap this watchdog before its own root (#953).
    kill_pid "$main"
    kill_tree "$main" "$winpid" "$winimage"
}

ci_suite_heartbeat() {
    local quiet=$1 every=$2 progress=$3 stuck=$4 main=$5 hard=${6:-0}
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
        # Ahead of the quiet window, which only delays the staticness verdict: a
        # cap that inherited that delay could not be set below it.
        if test "$hard" -gt 0 && test $((now - begin)) -ge "$hard"; then
            ci_heartbeat_kill "$main" \
                "killing the step: ${hard}s cap reached, in flight: $last"
            return 0
        fi
        test $((now - begin)) -ge "$quiet" || continue
        # Every tick, not on the annotation cadence, which would let a late wedge
        # outlive the step's own timeout before being caught.
        if test $((now - moved)) -ge "$stuck"; then
            ci_heartbeat_kill "$main" \
                "killing the step: $((now - moved))s without progress, in flight: $last"
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

# Every gate here exits 77, so an all-skipped suite would report green having
# tested nothing: pin the skips, and floor the passes in case the glob empties.
# One name per line, so two branches each appending one don't collide on the
# same line; compared as a sorted set below, so glob discovery order can't
# cause a false mismatch either.
# footer-overflow and purge-longpath skip on Windows (need a path past MAX_PATH);
# webdav-default and proxytrack-quiet read proxytrack's console through a pty,
# which Windows Python does not build;
# badmtime needs a filesystem that stores an mtime past gmtime's range;
# single-file-gui and holdport drive htsserver, which this job does not build;
# update-304-leak and cmdline-leak need a LeakSanitizer build, which MSVC has no
# equivalent of;
# crash-symbolize and backtrace-empty need backtrace(), which Windows has no
# equivalent of;
# string-oom drives a helper binary that only the automake build produces;
# datadir-ospath copies the unwrapped binary the automake build leaves in .libs,
# and needs the loader variable libtool picked, neither of which this job has;
# link-control-bytes names its fixtures with the raw control bytes the requests
# decode back to, which NTFS refuses;
# memresume, repaircache and resume-recovery interrupt pass 1 with a signal
# MSYS cannot deliver to a native exe;
# ftp-deadhost-interrupt, ftp-sigterm, abort-purge, signal-receive and
# ftp-stop-window need that same signal (deadhost's --timeout half runs as 245,
# abort-purge's --max-time half as 268);
# close-once and threadattr-leak interpose through LD_PRELOAD, which MSYS has no
# equivalent for, and this job sets neither interposer's path;
# engine-install-paths reads the compiled-in POSIX install paths, which this job
# has no equivalent of;
# build-features compares the feature reporter against the automake config.h,
# which the MSVC build does not produce;
# engine-wizard-eof drives the wizard through a pty, and Windows builds Python
# with neither pty nor os.fork.
expected_skips_msys="01_engine-footer-overflow.test
253_local-ftp-close-once.test
113_engine-threadattr-leak.test
100_local-purge-longpath.test
158_local-link-control-bytes.test
114_local-update-304-leak.test
283_engine-cmdline-leak.test
120_local-proxytrack-webdav-default.test
143_engine-backtrace-empty.test
152_engine-string-oom.test
153_local-proxytrack-quiet.test
215_engine-datadir-ospath.test
243_local-ftp-deadhost-interrupt.test
255_local-ftp-sigterm.test
261_local-abort-purge.test
262_local-signal-receive.test
263_local-ftp-stop-window.test
235_local-resume-recovery.test
48_local-crange-memresume.test
71_local-crange-repaircache.test
80_engine-crash-symbolize.test
88_local-proxytrack-badmtime.test
241_local-single-file-gui.test
288_testlib-holdport.test
350_local-diskfull-abort.test
352_engine-filesave-diskfull.test
355_local-write-error-not-eof.test
377_engine-install-paths.test
398_engine-build-features.test
424_engine-wizard-eof.test"

# A copy of the msys list above: testlib.sh's suite_backend split (shell_is_msys
# vs target_is_windows) is designed so the same tests skip under either shell.
# A prediction, not a measurement yet — correct it from the first real wsl2 run.
expected_skips_wsl2=$expected_skips_msys

# Sets ci_skip_list to the pinned skip set for backend $1, failing loudly if
# there is none: an unknown backend must never fall back to an empty list,
# which would make any skip look expected.
ci_skip_list=''
ci_expected_skips_for_backend() {
    case "$1" in
    msys) ci_skip_list=$expected_skips_msys ;;
    wsl2) ci_skip_list=$expected_skips_wsl2 ;;
    *)
        echo "::error::no expected-skip list for backend '$1'"
        return 1
        ;;
    esac
}

# Only a direct run drives a suite. Asked of the shell, not derived from $0,
# which a caller can set to this very path (172_ci-windows-driver.test).
(return 0 2>/dev/null) && return 0

# Explicit now that this is a script; GitHub's "shell: bash" gave the step both.
set -euo pipefail
bin=${1:?usage: ci-windows-suite.sh <bindir>}
# The shell decides the default: this script drives the suite from MSYS today
# and from a WSL2 distro on the second leg, where the workflow sets it. Nothing
# can sniff wsl2, since that shell answers Linux like any other.
export HTTRACK_SUITE_BACKEND=${HTTRACK_SUITE_BACKEND:-msys}
export PATH="$bin:$PATH"

# WSL2 resolves a filename exactly, so every Windows program the tests name bare
# needs one, and a drvfs argument means nothing to a native exe. One shim per
# name does both; see tests/wsl2-exe-shim.sh. MSYS needs none of it, which is
# why the tests keep saying `httrack` and `taskkill` on both backends.
if test "$HTTRACK_SUITE_BACKEND" = wsl2; then
    shimdir=$PWD/.wsl2-shims
    rm -rf "$shimdir"
    mkdir -p "$shimdir"
    # Copied, not linked: the checkout's own mount may refuse to execute it.
    for e in $ENGINE_EXES taskkill tasklist ping; do
        cp "$testdir/wsl2-exe-shim.sh" "$shimdir/$e"
        chmod +x "$shimdir/$e"
    done
    export PATH="$shimdir:$PATH"
fi

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
if test "$HTTRACK_SUITE_BACKEND" = wsl2; then
    # The Linux view, not the native one: a WSL2 shell cannot mktemp into a
    # drive-letter path. Tests hand the engine drvfs paths and the shim above
    # translates them, which is why RUNNER_TEMP has to stay on a Windows volume.
    TMPDIR="$(drvfs_path -u "$RUNNER_TEMP")"
else
    export MSYS_NO_PATHCONV=1
    export MSYS2_ARG_CONV_EXCL='*'
    TMPDIR="$(cygpath -m "$RUNNER_TEMP")"
fi
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
# timeout: a cancelled step keeps neither its log nor the artifacts the
# later if:always() steps would upload, one failing on its own terms keeps
# both. Left at the serial budget though the pool costs 4 min, because it
# bounds a wedge and not a healthy run: fitted to the pool it reds a merely
# slow run and every jobs=1 one (23 min), and the traced rerun below gets
# only what is left of it, so a tight value drops the trace on exactly the
# runs worth diagnosing.
suite_deadline=1500
started=$SECONDS

# Reaches the artifact whenever the step ends on its own terms, which the
# tail of its log may not. Absolute, since a .test may have chdir'd.
progress=suite-progress.log
: >"$progress"
export HTTRACK_PROGRESS_LOG="$PWD/$progress"

# A dying runner takes both, and every annotation with them (measured, see
# #795), so the watchdog's job is to end the step first: one that fails on
# its own terms keeps its log. Also left at the serial budget: under the pool
# any live worker keeps the log moving, so staticness now catches a stalled
# suite and nothing narrower -- one wedged test is bounded by $per_test and
# by $hard_deadline below.
stuck=900
# Orthogonal to that heartbeat rather than a spare of it: the heartbeat needs
# 960s of quiet and 900s of static log, and every #795 death measured so far
# lands inside the first 750s of the step.
watchdog='' ci_watchdog_pid=''
# Wall-clock backstop, because staticness alone bounds nothing: the deadline above
# is read before each dispatch, and past it the tests in flight may still spend
# $per_test and then a traced rerun, each writing a progress line that re-arms
# $stuck. 2400s clears that 1500+600 worst case with room for the hang dump, and
# leaves 300s of the step's 45 minutes to fail on our own terms and upload (#1126).
hard_deadline=2400
ci_start_native_watchdog "$PWD/$progress" && watchdog=$ci_watchdog_pid
test -n "$watchdog" || echo "no off-box watchdog: no usable PowerShell"
ci_suite_heartbeat 960 360 "$progress" "$stuck" $$ "$hard_deadline" &
heartbeat=$!
trap 'set +e; kill "$heartbeat" 2>/dev/null; test -z "$watchdog" || kill_pid "$watchdog"' EXIT

pass=0 fail=0 skip=0 lost=0 failed="" skipped="" vanished="" deadline=0
# Tests in flight at once, min(2*nproc, 16), unless the caller pins it
# (windows-build.yml does): each binds its own ephemeral-port server, and they
# mostly sleep.
cores=$(nproc 2>/dev/null || echo "${NUMBER_OF_PROCESSORS:-2}")
case "$cores" in '' | 0 | *[!0-9]*) cores=2 ;; esac
jobs=$((cores * 2))
test "$jobs" -le 16 || jobs=16
# So a test can pin the width; a nonsense value is named rather than obeyed.
case "${HTTRACK_SUITE_JOBS:-}" in
'') ;;
0 | *[!0-9]*) echo "::warning::ignoring HTTRACK_SUITE_JOBS=$HTTRACK_SUITE_JOBS" ;;
*) jobs=$HTTRACK_SUITE_JOBS ;;
esac
# wait -n is bash 4.3, and macOS drives this script under 3.2
# (172_ci-windows-driver.test): with no way to wait for a free slot, run serially.
pool=
if test "${BASH_VERSINFO[0]}" -gt 4 ||
    { test "${BASH_VERSINFO[0]}" -eq 4 && test "${BASH_VERSINFO[1]}" -ge 3; }; then
    pool=1
fi
test -n "$pool" || jobs=1
# kill_tree's last-resort sweep (testlib.sh) kills every engine and every python
# on the host, so it is sound only while one test is in flight.
if test "$jobs" -gt 1; then
    unset HTTRACK_EXCLUSIVE_HOST
else
    export HTTRACK_EXCLUSIVE_HOST=1
fi

# A worker cannot increment a counter of ours, so every outcome is written here
# and the tally read back once the pool has drained.
results=suite-results
rm -rf "$results"
mkdir -p "$results"

# Run test $1 to completion: its verdict on stdout, a failure's 25-line tail into
# $results/$1.tail, and its status last into $results/$1.rc, which says it got there.
run_one_test() (
    local t=$1 rc=0 left ttmp
    # Its own TMPDIR, since dump_crawl_logs (testlib.sh) globs the whole of it.
    # Windows-shaped like the export above: the tests hand it to a native exe.
    ttmp="$TMPDIR/suite.$t"
    rm -rf "$ttmp" 2>/dev/null || true
    mkdir -p "$ttmp"
    # Same guard "make check" uses on POSIX, so a wedge is diagnosed the
    # same way on every platform. It dumps before it kills, which a bare
    # run_with_timeout cannot: by the time that returns, the tree whose
    # stack we wanted is already gone.
    HTTRACK_TEST_TIMEOUT=$per_test TMPDIR="$ttmp" \
        bash ./test-timeout.sh "$t" >"$t.log" 2>&1 || rc=$?
    case "$rc" in
    0) echo "PASS $t" ;;
    77) echo "SKIP $t" ;;
    124)
        # test-timeout.sh has already written the process list, the stacks
        # and the killed crawl's own logs into $t.log.
        echo "FAIL $t (timed out, tree killed)"
        tail -n 25 "$t.log" | sed 's/^/      /' >"$results/$t.tail"
        ;;
    *)
        echo "FAIL $t (exit $rc)"
        # Captured before the trace appends below, or an intermittent failure
        # reports the tail of the re-run that passed.
        tail -n 25 "$t.log" | sed 's/^/      /' >"$results/$t.tail"
        # These assert with `test "$(...)" == "..." || exit 1`, which
        # says nothing at all on failure. Re-run traced, still bounded.
        # Charged to the suite deadline rather than given a budget of its
        # own: a failure just under that deadline would else spend
        # $per_test twice and land on the workflow's own timeout (#1126).
        left=$((suite_deadline - (SECONDS - started)))
        test "$left" -le "$per_test" || left=$per_test
        if [ "$left" -gt 0 ]; then
            # Noted first, or a slow trace reads as a wedge to the watchdog,
            # which would then kill the step in the middle of writing it.
            echo "RERUN $t" >>"$progress"
            # Handed the same TMPDIR: the trace runs outside test-timeout.sh,
            # which is what gave the first run one of its own.
            TMPDIR="$ttmp" run_with_timeout "$left" bash -x "$t" >>"$t.log" 2>&1 || true
            echo "      --- traced re-run ---" >>"$results/$t.tail"
            tail -n 25 "$t.log" | sed 's/^/      /' >>"$results/$t.tail"
        else
            echo "no trace: past the ${suite_deadline}s suite deadline" |
                tee -a "$t.log" | sed 's/^/      /' >>"$results/$t.tail"
        fi
        ;;
    esac
    echo "$rc $t" >>"$progress"
    # Never fatal: Windows may still hold a file the killed tree left open.
    rm -rf "$ttmp" 2>/dev/null || true
    echo "$rc" >"$results/$t.rc"
)

# Workers still running, into $workers. Counted, not taken from wait -n, which
# returns for any job (the heartbeat included) and cannot be told which to watch.
workers=0
count_workers() {
    local p
    workers=0
    for p in ${pids[@]+"${pids[@]}"}; do
        if kill -0 "$p" 2>/dev/null; then workers=$((workers + 1)); fi
    done
}

# label:pattern, globbed rather than enumerated so a new NNN_engine-*.test or
# NNN_local-*.test is picked up instead of silently getting zero coverage. Every
# entry carries a metacharacter, or nullglob cannot empty it and the gate below
# has nothing to catch.
# testlib and crawllib cover what most tests here rest on.
categories=(runnable:'00_runnable*.test' engine:'*_engine-*.test' zlib:'*_zlib-*.test'
    local:'*_local-*.test' watchdog:'*_watchdog*.test'
    testlib:'*_testlib-*.test' crawllib:'*_crawllib*.test'
    crawl-harness:'*_crawl-harness-*.test'
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

pids=() ran_tests=()
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
    ran_tests+=("$t")
    if test "$jobs" -eq 1; then
        # Never fatal: a worker killed by a signal is one failed test, not the
        # end of the suite, and only the gates below may stop it (errexit).
        run_one_test "$t" || true
        continue
    fi
    count_workers
    while test "$workers" -ge "$jobs"; do
        wait -n 2>/dev/null || true
        count_workers
    done
    run_one_test "$t" &
    pids+=("$!")
done
# Never a bare wait, which would also wait on the heartbeat and never return.
for p in ${pids[@]+"${pids[@]}"}; do
    wait "$p" 2>/dev/null || true
done
# In test order, once nothing is still writing: eight workers interleaving their
# failure tails is noise.
for t in ${ran_tests[@]+"${ran_tests[@]}"}; do
    ci_read_outcome "$t" "$results"
    case "$ci_outcome" in
    pass) pass=$((pass + 1)) ;;
    skip) skip=$((skip + 1)) skipped="$skipped $t" ;;
    fail) fail=$((fail + 1)) failed="$failed $t" ;;
    *)
        lost=$((lost + 1)) vanished="$vanished $t"
        # Its own verdict, not a FAIL: a test that crashed is not a worker killed
        # under it.
        ci_lost_reason "$t"
        echo "LOST $t (worker left no status; $ci_reason)"
        ;;
    esac
    test ! -s "$results/$t.tail" || cat "$results/$t.tail"
done
# An orphaned httrack.exe spins and starves the runner ("lost communication"). Once,
# at the end: matching by image name, an earlier reap cannot spare a live sibling.
reap_leftover_processes "the suite" | tee -a "$progress"
echo "ran=$((pass + fail + skip + lost)) pass=$pass fail=$fail skip=$skip lost=$lost" |
    tee -a "$GITHUB_STEP_SUMMARY"

# Every gate here exits 77, so an all-skipped suite would report green having
# tested nothing: pin the skips, and floor the passes in case the glob empties.
# The pinned lists (per-entry reasons above ci_expected_skips_for_backend) are
# compared as a sorted set below, so glob discovery order can't cause a false
# mismatch, and picked by backend, since a skip tied to the MSYS shell need not
# hold under wsl2, which drives the same .exe from a different shell.
ci_expected_skips_for_backend "$HTTRACK_SUITE_BACKEND" || exit 1
expected_skips=$ci_skip_list
# First, or the deadline reads as an unexplained shortfall in the gates below.
[ "$deadline" -eq 0 ] || {
    echo "::error::suite did not finish within ${suite_deadline}s"
    exit 1
}
# Ahead of the gates below, each of which a lost worker can trip on its own: it
# leaves the pass count short and the skip set holed.
test "$lost" -eq 0 ||
    echo "::error::worker(s) vanished with no status, re-run this leg:$vanished"
[ "$pass" -ge 90 ] || {
    echo "::error::only $pass tests passed ($skip skipped)"
    # Vanished workers lower the count without anything having failed.
    [ "$lost" -gt 0 ] || exit 1
}
# Word-split on whitespace (space-joined $skipped, newline-joined
# expected_skips both work) and sort, so the compare is a set, not a string.
# shellcheck disable=SC2086 # the splitting is what makes it a set
got=$(printf '%s\n' $skipped | sort)
# shellcheck disable=SC2086
want=$(printf '%s\n' $expected_skips | sort)
if [ "$got" != "$want" ]; then
    echo "::error::skip set changed from expected; - missing, + newly skipped"
    diff -u <(echo "$want") <(echo "$got") | tail -n +3 | sed 's/^/      /' || true
    # A worker that vanished cannot have recorded its skip, so the set differs
    # for a reason we already know; keep that a re-run, not a red.
    [ "$lost" -gt 0 ] || exit 1
fi
[ "$fail" -eq 0 ] || {
    echo "::error::failing:$failed"
    exit 1
}
# Last, and 3 rather than 1: nothing failed on its own terms, so this leg is one to
# repeat rather than a red to investigate (#1228).
[ "$lost" -eq 0 ] || exit 3
