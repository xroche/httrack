#!/bin/bash
#
# Bash arms of the Windows runner-kill probe (#1228): spend MSYS process
# creations, or one process and one loopback connection per iteration, and
# report what was spent. tools/kill-probe-arm.ps1 is the sockets arm.
set -euo pipefail

arm=${1:-}
events=${PROBE_EVENTS:-}
seconds=${PROBE_SECONDS:-}
if [ -z "$arm" ] || [ -z "$events" ] || [ -z "$seconds" ]; then
    echo "usage: PROBE_EVENTS=n PROBE_SECONDS=n $0 forks|both" >&2
    exit 2
fi

testdir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../tests" && pwd)
srvpid=
srvlog=

cleanup() {
    set +e
    test -n "$srvpid" && kill "$srvpid" 2>/dev/null
}
trap cleanup EXIT

# The server lives in this process, not in an earlier step: a background process
# on a Windows runner does not survive the step that started it.
start_server() {
    local root=$TMPDIR/probe-root port
    srvlog=$TMPDIR/probe-server.log
    mkdir -p "$root"
    echo '<html>probe</html>' >"$root/index.html"
    python "$testdir/local-server.py" --root "$root" >"$srvlog" 2>&1 &
    srvpid=$!
    for _ in $(seq 60); do
        port=$(sed -n 's/^PORT \([0-9][0-9]*\).*/\1/p' "$srvlog" | head -1)
        test -n "$port" && break
        kill -0 "$srvpid" 2>/dev/null || break
        sleep 0.5
    done
    test -n "$port" || {
        echo "the loopback server never announced a port: $(cat "$srvlog")" >&2
        exit 1
    }
    echo "$port"
}

# type -P, not command -v: `true` is a builtin, and a builtin creates no process.
spawn=$(type -P true) || spawn=
# System32 wins the image PATH, so this is the native curl. The connection is
# native Winsock either way; what the arm is spending is the MSYS spawn around it,
# which is the shape the suite makes when it launches httrack.exe.
client=$(type -P curl) || client=

began=$SECONDS
procs=0
conns=0
errors=0

case $arm in
forks)
    test -n "$spawn" || {
        echo "no true(1) on PATH: the fork arm would spend nothing" >&2
        exit 1
    }
    while [ "$procs" -lt "$events" ] && [ $((SECONDS - began)) -lt "$seconds" ]; do
        "$spawn" || errors=$((errors + 1))
        procs=$((procs + 1))
    done
    ;;
both)
    test -n "$client" || {
        echo "no curl(1) on PATH: the combined arm would spend nothing" >&2
        exit 1
    }
    port=$(start_server)
    # Unpaced, unlike the sockets arm: the MSYS spawn is the slow part and it
    # holds the rate well under what would exhaust the ephemeral range.
    while [ "$conns" -lt "$events" ] && [ $((SECONDS - began)) -lt "$seconds" ]; do
        "$client" -s -o /dev/null --max-time 10 "http://127.0.0.1:$port/" ||
            errors=$((errors + 1))
        procs=$((procs + 1))
        conns=$((conns + 1))
    done
    ;;
*)
    echo "unknown arm: $arm" >&2
    exit 2
    ;;
esac

elapsed=$((SECONDS - began))
attempts=$((procs > conns ? procs : conns))
echo "arm=$arm procs=$procs conns=$conns errors=$errors attempts=$attempts elapsed=${elapsed}s"

# Against attempts, never against the budget: a failure that is slow spends few
# events, so a budget-relative tolerance passes an arm where everything failed.
if [ "$attempts" -eq 0 ] || [ "$errors" -gt $((attempts / 20)) ]; then
    echo "$errors of $attempts attempts failed: dose not delivered" >&2
    exit 1
fi
