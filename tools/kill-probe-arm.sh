#!/bin/bash
#
# One arm of the Windows runner-kill probe (#1228). Spends an event budget on
# MSYS process creation, on loopback connections, or on both, then reports what
# it spent. The kill rate per arm is the readout; this side only has to make the
# dose, and to make it countable.
set -euo pipefail

usage() {
    echo "usage: $0 forks|both <events> <seconds> [port]" >&2
    exit 2
}

arm=${1-}
events=${2-}
seconds=${3-}
port=${4-}
if [ -z "$arm" ] || [ -z "$events" ] || [ -z "$seconds" ]; then
    usage
fi

# type -P, not command -v: `true` is a builtin, and a builtin creates no process.
spawn=$(type -P true) || spawn=
client=$(type -P curl) || client=

began=$SECONDS
procs=0
conns=0
errors=0

case $arm in
forks)
    [ -n "$spawn" ] || {
        echo "no true(1) on PATH: the fork arm would measure nothing" >&2
        exit 1
    }
    while [ "$procs" -lt "$events" ] && [ $((SECONDS - began)) -lt "$seconds" ]; do
        "$spawn" || errors=$((errors + 1))
        procs=$((procs + 1))
    done
    ;;
both)
    [ -n "$client" ] || {
        echo "no curl(1) on PATH: the combined arm would measure nothing" >&2
        exit 1
    }
    [ -n "$port" ] || usage
    # One process and one connection per iteration, which is the shape the suite
    # makes when it spawns httrack.exe. No pacing: the MSYS spawn is the slow
    # part, and it holds the rate below what would exhaust the ephemeral range.
    while [ "$conns" -lt "$events" ] && [ $((SECONDS - began)) -lt "$seconds" ]; do
        "$client" -s -o /dev/null --max-time 10 "http://127.0.0.1:$port/" ||
            errors=$((errors + 1))
        procs=$((procs + 1))
        conns=$((conns + 1))
    done
    ;;
*)
    usage
    ;;
esac

elapsed=$((SECONDS - began))
echo "arm=$arm procs=$procs conns=$conns errors=$errors elapsed=${elapsed}s"

# An arm that mostly failed to spend its dose is not a null result about the
# runner, so say so rather than reporting a clean job.
[ "$errors" -le $((events / 20)) ] || {
    echo "$errors of $((procs > conns ? procs : conns)) events failed: dose not delivered" >&2
    exit 1
}
