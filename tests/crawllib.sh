#!/bin/bash
#
# local-server.py launch and crawl helpers, shared by local-crawl.sh and by the
# tests that drive the server themselves. Sourced, not run.

# shellcheck source=tests/testlib.sh
. "$(dirname "${BASH_SOURCE[0]}")/testlib.sh"

# Engine time cap, and the watchdog above it: the cap fires first on a healthy
# crawl, so only a genuine wedge trips the watchdog.
CRAWL_MAX_TIME=120
# A function, not a value: 72_watchdog-crawl and 258 set CRAWL_DEADLINE after
# this file is sourced.
crawl_deadline() { printf '%s\n' "${CRAWL_DEADLINE:-180}"; }

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
# "PORT n" line. Sets SRV_PORT, SRV_PID, SRV_LOG and BASEURL, registers the
# reaping cleanup and appends to SRV_PIDS. A second call overwrites all four and
# truncates the default log, so a caller wanting two servers saves each set and
# gives each its own --log. Options, ahead of any server argument; -- ends them:
#   --root DIR    tree to serve (default: the shared server-root fixture)
#   --log FILE    where the announcement lands (default: $tmpdir/server.log)
#   --env V=VAL   an environment variable for the server, repeatable
#   --tls         serve HTTPS with the test certificate; BASEURL says https
# shellcheck disable=SC2120 # most callers want the fixture and no options
local_server_start() {
    local root="${testdir}/server-root" log='' envs=() tls=() scheme=http
    while test $# -gt 0; do
        case $1 in
        --root)
            root=$2
            shift 2
            ;;
        --tls)
            scheme=https
            tls=(--tls --cert "$(nativepath "${testdir}/server.crt")")
            tls+=(--key "$(nativepath "${testdir}/server.key")")
            shift
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

    # local-server.py runs native under wsl2: the shim never converts a path
    # inside a value, and find_python's .exe fallback goes round it entirely.
    local wslenv=() names='' e
    if test "$(suite_backend)" = wsl2 && test ${#envs[@]} -gt 0; then
        for e in "${!envs[@]}"; do
            names="${names:+$names:}${envs[$e]%%=*}"
            envs[e]="${envs[$e]%%=*}=$(nativepath "${envs[$e]#*=}")"
        done
        wslenv=("WSLENV=${WSLENV:+$WSLENV:}$names")
    fi

    # Stdin off the terminal: run_with_timeout toggles job control, and a
    # background job that touches the tty is stopped with SIGTTIN.
    env ${wslenv[@]+"${wslenv[@]}"} ${envs[@]+"${envs[@]}"} "$SRV_PYTHON" \
        "$(nativepath "${testdir}/local-server.py")" \
        --root "$(nativepath "$root")" ${tls[@]+"${tls[@]}"} "$@" \
        >"$SRV_LOG" 2>&1 </dev/null &
    SRV_PID=$!
    SRV_PIDS+=("$SRV_PID")
    cleanup_push local_server_reap "$((${#SRV_PIDS[@]} - 1))"

    SRV_PORT=$(discover_server_port "$SRV_LOG" "$SRV_PID") ||
        fail "local-server did not come up: $(<"$SRV_LOG")"
    # shellcheck disable=SC2034 # set here for the caller, not used here
    BASEURL="${scheme}://127.0.0.1:${SRV_PORT}"
}

# Run httrack against the local server, returning its exit status. Carries the
# backstops most of these tests lacked: a --max-time cap (skipped when the caller
# sets its own) and a watchdog above it, so a wedge outliving the engine limit
# reds the test instead of the 45-minute CI timeout.
# Options, ahead of any httrack argument; -- ends them:
#   --log FILE     crawl output (default: discarded)
#   --stdin FILE   what the engine reads on stdin, for a test answering an
#                  interactive prompt; the word "closed" hands it no descriptor
#                  at all. Redirecting the local_crawl call instead does not
#                  reach the engine (#1258).
local_crawl() {
    local deadline log=/dev/null stdin=() arg rc=0
    deadline=$(crawl_deadline)
    while test $# -gt 0; do
        case $1 in
        --log)
            log=$2
            shift 2
            ;;
        --stdin)
            stdin=(--stdin "$2")
            shift 2
            ;;
        --)
            shift
            break
            ;;
        *) break ;;
        esac
    done

    local args=("--max-time=$CRAWL_MAX_TIME")
    for arg in "$@"; do
        case $arg in --max-time | --max-time=*) args=() ;; esac
    done
    args+=("$@")

    run_with_timeout ${stdin[@]+"${stdin[@]}"} "$deadline" httrack "${args[@]}" \
        >"$log" 2>&1 || rc=$?
    test "$rc" -ne 124 || fail "crawl watchdog fired after ${deadline}s"
    return "$rc"
}

# What a run reported doing to the files the previous mirror had, out of
# hts-changes.json: assert_purged true|false OUTDIR.
assert_purged() {
    local want="$1" json="${2}/hts-changes.json"
    command grep -q "\"purged\": ${want}" "$json" ||
        fail "expected \"purged\": ${want} in $(<"$json")"
}

# assert_alive FILE MARKER: still mirrored, and still the body we mirrored.
assert_alive() {
    test -s "$1" || fail "$1 is gone"
    command grep -q "$2" "$1" || fail "$1 no longer carries ${2}"
}

# assert_logged OUTDIR PATTERN WHAT: the premise a shape rests on, in the log.
assert_logged() {
    local log="${1}/hts-log.txt"
    command grep -qE "$2" "$log" || fail "${3}: $(<"$log")"
}

# assert_gave_up OUTDIR URLPATH: the link really did exhaust its retries on a
# transport failure, not on an answered error.
assert_gave_up() {
    assert_logged "$1" "Error:.*\\(-[0-9]+\\).*at link .*${2}" \
        "no transport failure gave up on ${2}"
}
