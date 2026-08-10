#!/bin/bash
#
# local-server.py launch and crawl helpers, for the tests that drive the server
# themselves instead of going through local-crawl.sh. Sourced, not run.

# shellcheck source=tests/testlib.sh
. "$(dirname "${BASH_SOURCE[0]}")/testlib.sh"

# Live servers, in start order; a stopped one leaves an empty slot. cleanup_push
# expands its arguments at push time, so a teardown holding the pid itself could
# not be disarmed, and 240 stops its server mid-test on purpose.
SRV_PIDS=()

# Teardown for the server in slot $1. Signalling a pid the system has since
# recycled would kill an unrelated process and then stall reap_bounded for its
# whole grace period, so a slot is read, not a pid.
local_server_reap() {
    local pid=${SRV_PIDS[$1]:-}
    test -n "$pid" || return 0
    SRV_PIDS[$1]=
    stop_server "$pid"
}

# Stop server $1 now and disarm its teardown, for a test whose next step needs
# the server gone.
local_server_stop() {
    local i
    for ((i = 0; i < ${#SRV_PIDS[@]}; i++)); do
        test "${SRV_PIDS[$i]:-}" != "$1" || SRV_PIDS[i]=
    done
    stop_server "$1"
}

# Start local-server.py in the background on an ephemeral port and wait for its
# "PORT n" line. Sets SRV_PORT, SRV_PID, SRV_LOG and BASEURL, and registers the
# reaping cleanup, and appends to SRV_PIDS. A second call overwrites all four
# and truncates the default
# log, so a caller wanting two servers saves each set and gives each its own --log. Options, ahead of any server argument; -- ends them:
#   --root DIR    tree to serve (default: the shared server-root fixture)
#   --log FILE    where the announcement lands (default: $tmpdir/server.log)
#   --env V=VAL   an environment variable for the server, repeatable
# shellcheck disable=SC2120 # most callers want the fixture and no options
local_server_start() {
    local root="${testdir}/server-root" log='' envs=()
    while test $# -gt 0; do
        case $1 in
        --root)
            root=$2
            shift 2
            ;;
        --log)
            log=$2
            shift 2
            ;;
        --env)
            envs+=("$2")
            shift 2
            ;;
        --)
            shift
            break
            ;;
        *) break ;;
        esac
    done

    test -n "${SRV_PYTHON:-}" ||
        SRV_PYTHON=$(find_python) || skip "python3 not found"
    test -n "$log" || log="${tmpdir:?no tmpdir and no --log}/server.log"
    SRV_LOG=$log
    : >"$SRV_LOG"

    # Stdin off the terminal: run_with_timeout toggles job control, and a
    # background job that touches the tty is stopped with SIGTTIN.
    env ${envs[@]+"${envs[@]}"} "$SRV_PYTHON" \
        "$(nativepath "${testdir}/local-server.py")" \
        --root "$(nativepath "$root")" "$@" >"$SRV_LOG" 2>&1 </dev/null &
    SRV_PID=$!
    SRV_PIDS+=("$SRV_PID")
    cleanup_push local_server_reap "$((${#SRV_PIDS[@]} - 1))"

    SRV_PORT=$(discover_server_port "$SRV_LOG" "$SRV_PID") ||
        fail "local-server did not come up: $(cat "$SRV_LOG")"
    # shellcheck disable=SC2034 # set here for the caller, not used here
    BASEURL="http://127.0.0.1:${SRV_PORT}"
}

# Run httrack against the local server, returning its exit status. Carries the
# backstops local-crawl.sh has and most of these tests lacked: a --max-time cap
# (skipped when the caller sets its own) and a watchdog above it, so a wedge
# outliving the engine limit reds the test instead of the 45-minute CI timeout.
# CRAWL_DEADLINE drives the watchdog, as it does for local-crawl.sh.
# One option, ahead of any httrack argument; -- ends it:
#   --log FILE     crawl output (default: discarded)
local_crawl() {
    local deadline="${CRAWL_DEADLINE:-180}" log=/dev/null arg rc=0
    while test $# -gt 0; do
        case $1 in
        --log)
            log=$2
            shift 2
            ;;
        --)
            shift
            break
            ;;
        *) break ;;
        esac
    done

    local args=(--max-time=120)
    for arg in "$@"; do
        case $arg in --max-time | --max-time=*) args=() ;; esac
    done
    args+=("$@")

    run_with_timeout "$deadline" httrack "${args[@]}" >"$log" 2>&1 || rc=$?
    test "$rc" -ne 124 || fail "crawl watchdog fired after ${deadline}s"
    return "$rc"
}
