#!/bin/bash
#
# local-server.py launch and crawl helpers, for the tests that drive the server
# themselves instead of going through local-crawl.sh. Sourced, not run.

# shellcheck source=tests/testlib.sh
. "$(dirname "${BASH_SOURCE[0]}")/testlib.sh"

# Start local-server.py in the background on an ephemeral port and wait for its
# "PORT n" line. Sets SRV_PORT, SRV_PID, SRV_LOG and BASEURL, and registers the
# reaping cleanup. Callable more than once; a caller wanting two servers saves
# each set before starting the next. Options, ahead of any server argument:
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

    test -n "${CRAWLLIB_PYTHON:-}" ||
        CRAWLLIB_PYTHON=$(find_python) || skip "python3 not found"
    test -n "$log" || log="${tmpdir:?no tmpdir and no --log}/server.log"
    SRV_LOG=$log
    : >"$SRV_LOG"

    env ${envs[@]+"${envs[@]}"} "$CRAWLLIB_PYTHON" \
        "$(nativepath "${testdir}/local-server.py")" \
        --root "$(nativepath "$root")" "$@" >"$SRV_LOG" 2>&1 </dev/null &
    SRV_PID=$!
    cleanup_push stop_server "$SRV_PID"

    SRV_PORT=$(discover_server_port "$SRV_LOG" "$SRV_PID") ||
        fail "local-server did not come up: $(cat "$SRV_LOG")"
    # shellcheck disable=SC2034 # set here for the caller, not used here
    BASEURL="http://127.0.0.1:${SRV_PORT}"
}

# Run httrack against the local server, returning its exit status. Carries the
# backstops local-crawl.sh has and most of these tests lacked: a --max-time cap
# (skipped when the caller sets its own) and a watchdog above it, so a wedge
# outliving the engine limit reds the test instead of the 45-minute CI timeout.
#   --deadline N   watchdog seconds (default: CRAWL_DEADLINE, else 180)
#   --log FILE     crawl output, appended (default: discarded)
local_crawl() {
    local deadline="${CRAWL_DEADLINE:-180}" log=/dev/null arg rc=0
    while test $# -gt 0; do
        case $1 in
        --deadline)
            deadline=$2
            shift 2
            ;;
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

    run_with_timeout "$deadline" httrack "${args[@]}" >>"$log" 2>&1 || rc=$?
    test "$rc" -ne 124 || fail "crawl watchdog fired after ${deadline}s"
    return "$rc"
}
