#!/bin/bash
#
# htsserver launch, request and reaping helpers. Sourced, not run.

HTS_TESTDIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=tests/testlib.sh
. "${HTS_TESTDIR}/testlib.sh"

HTS_DISTDIR=$(cd "${top_srcdir:-${HTS_TESTDIR}/..}" && pwd)
HTS_BG_PIDS=()
HTS_SRV_PIDS=()
HTS_REAPED_PIDS=()
HTS_TMP_LOGS=()

fail() {
    echo "FAIL: $*" >&2
    exit 1
}

skip() {
    echo "$*; skipping" >&2
    exit 77
}

# First line of $1. A "| head -1" would close the pipe early and, under
# pipefail, SIGPIPE the producer into a spurious failure.
firstline() { printf '%s\n' "${1%%$'\n'*}"; }

# What every test here needs. The Debian buildd chroot has no python3, so that
# one skips rather than reds.
htsserver_require() {
    command -v htsserver >/dev/null || fail "no htsserver in PATH"
    HTS_PYTHON=$(find_python) || skip "python3 not found"
}

# A free loopback port; the socket is closed before htsserver rebinds it.
htsserver_freeport() {
    "${HTS_PYTHON}" -c 'import socket
s = socket.socket()
s.bind(("127.0.0.1", 0))
print(s.getsockname()[1])
s.close()'
}

# The server process; reads htsserver_start's locals and its background stdout.
htsserver_exec() {
    # htsserver keeps SIGTERM ignored across its exec, so only -9 reaps it.
    trap '' TERM TTOU XFSZ
    test -z "${wlimit}" || ulimit -f "${wlimit}"
    test -z "${home}" || export HOME="${home}"
    exec htsserver "${root%/}/" --port "${HTS_PORT}" "$@" 2>&1
}

# Start htsserver in the background on a free port and wait for its
# announcement. Sets HTS_URL, HTS_PORT, HTS_LOG, HTS_BGPID and HTS_PID (the
# server's own pid, which Windows does not announce). Options, ahead of any
# htsserver argument:
#   --root DIR       tree to serve (default: the dist root)
#   --home DIR       $HOME for the server, so no ~/.httrack.ini leaks in
#   --log FILE       where the announcement lands (default: a temp file)
#   --write-limit N  ulimit -f N; the announcement then rides a pipe, exempt
# shellcheck disable=SC2120 # most callers need no htsserver argument
htsserver_start() {
    local root=${HTS_DISTDIR} home='' log='' wlimit=''
    while test $# -gt 0; do
        case $1 in
        --root)
            root=$2
            shift 2
            ;;
        --home)
            home=$2
            shift 2
            ;;
        --log)
            log=$2
            shift 2
            ;;
        --write-limit)
            wlimit=$2
            shift 2
            ;;
        --)
            shift
            break
            ;;
        *) break ;;
        esac
    done

    HTS_PORT=$(htsserver_freeport) || fail "no free loopback port"
    if test -z "${log}"; then
        log=$(mktemp)
        HTS_TMP_LOGS+=("${log}")
    fi
    HTS_LOG=${log}
    : >"${HTS_LOG}"
    if test -n "${wlimit}"; then
        htsserver_exec "$@" | cat >"${HTS_LOG}" &
    else
        htsserver_exec "$@" >"${HTS_LOG}" &
    fi
    HTS_BGPID=$!
    HTS_BG_PIDS+=("${HTS_BGPID}")

    HTS_URL=
    for _ in $(seq 1 40); do
        HTS_URL=$(sed -n 's/^URL=//p' "${HTS_LOG}" 2>/dev/null | tr -d '\r') &&
            test -n "${HTS_URL}" && break
        kill -0 "${HTS_BGPID}" 2>/dev/null || break
        sleep 0.25
    done
    test -n "${HTS_URL}" || fail "htsserver did not come up: $(cat "${HTS_LOG}")"
    HTS_URL=$(firstline "${HTS_URL}")
    HTS_PID=$(firstline "$(sed -n 's/^PID=//p' "${HTS_LOG}" | tr -d '\r')")
    test -z "${HTS_PID}" || HTS_SRV_PIDS+=("${HTS_PID}")
}

# Reap every server started so far and remember them for
# htsserver_assert_reaped. Safe from a cleanup trap, and safe to call twice.
htsserver_stop() {
    local pid
    HTS_REAPED_PIDS=(${HTS_BG_PIDS[@]+"${HTS_BG_PIDS[@]}"})
    # Grouped: bash announces a killed job at any command boundary, so the
    # notice escapes a redirect on the wait alone once there are two servers.
    {
        for pid in ${HTS_SRV_PIDS[@]+"${HTS_SRV_PIDS[@]}"} \
            ${HTS_BG_PIDS[@]+"${HTS_BG_PIDS[@]}"}; do
            kill -9 "${pid}" 2>/dev/null || true
        done
        for pid in ${HTS_BG_PIDS[@]+"${HTS_BG_PIDS[@]}"}; do
            wait "${pid}" || true
        done
    } 2>/dev/null
    HTS_SRV_PIDS=()
    HTS_BG_PIDS=()
}

# Teardown: reap, then drop the logs the library allocated. Keep it out of
# htsserver_stop, which a test may call mid-run before reading the log back.
htsserver_cleanup() {
    htsserver_stop
    rm -f ${HTS_TMP_LOGS[@]+"${HTS_TMP_LOGS[@]}"}
    HTS_TMP_LOGS=()
}

# A leaked htsserver wedges the parallel harness behind a green log.
htsserver_assert_reaped() {
    local pid
    for pid in ${HTS_REAPED_PIDS[@]+"${HTS_REAPED_PIDS[@]}"}; do
        ! kill -0 "${pid}" 2>/dev/null || fail "htsserver ${pid} survived"
    done
}

htsserver_alive() { test -n "${HTS_PID:-}" && kill -0 "${HTS_PID}" 2>/dev/null; }

# Raw HTTP against $HTS_PORT; tests/httpclient.py documents the options.
htsserver_client() {
    "${HTS_PYTHON}" "${HTS_TESTDIR}/httpclient.py" --port "${HTS_PORT}" "$@"
}

# The reply to a GET of $1, status line and headers included.
htsserver_get() { htsserver_client --path "$1"; }

# The reply to a POST of the urlencoded body $1, to / unless $2 names a page.
htsserver_post() { htsserver_client --path "${2:-/}" --post "$1"; }

# The session id the server renders into every form, as a browser picks it up.
# Callers assert its length: an empty one makes every later check vacuous.
htsserver_sid() {
    firstline "$(htsserver_get /server/index.html |
        sed -n 's/.*name="sid" value="\([0-9a-f]*\)".*/\1/p')"
}
