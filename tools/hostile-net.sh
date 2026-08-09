#!/bin/bash
#
# Runs a command on a network that REFUSES TEST-NET-1 instead of dropping it,
# which is what the powerpc and ppc64 Debian buildds do. The suite's dead-host
# fixtures must skip there, never fail (#1108).
#
# Usage: tools/hostile-net.sh <command> [args...]        (re-execs under sudo)

set -euo pipefail

fail() {
    echo "hostile-net: $*" >&2
    exit 1
}

test $# -ge 1 || fail "usage: $0 <command> [args...]"

if [ "$(id -u)" -ne 0 ]; then
    exec sudo -E bash "$0" "$@"
fi

# Back to the invoking user for the payload: the suite must not run as root.
run_as=${SUDO_USER:-}
ns="hostilenet$$"
peer="${ns}p"

trap 'set +e; ip netns del "$ns" 2>/dev/null; ip netns del "$peer" 2>/dev/null' EXIT
trap 'exit 1' HUP INT TERM

ip netns add "$ns"
ip netns add "$peer"
ip link add veth-h netns "$ns" type veth peer name veth-p netns "$peer"
ip -n "$ns" link set lo up
ip -n "$ns" addr add 10.53.0.1/30 dev veth-h
ip -n "$ns" link set veth-h up
ip -n "$peer" link set lo up
ip -n "$peer" addr add 10.53.0.2/30 dev veth-p
ip -n "$peer" link set veth-p up
# The peer owns TEST-NET-1 and listens on nothing, so every SYN draws a RST.
ip -n "$peer" addr add 192.0.2.1/32 dev lo
# A local veth answers before connect() has returned; 30ms is one real hop.
ip netns exec "$ns" tc qdisc add dev veth-h root netem delay 30ms
ip -n "$ns" route add 192.0.2.0/24 via 10.53.0.2

# Non-vacuity: a namespace that drops, or one where the peer answers, makes
# every fixture below skip for the wrong reason and proves nothing.
probe=0
ip netns exec "$ns" timeout 5 bash -c 'exec 3<>/dev/tcp/192.0.2.1/80' 2>/dev/null ||
    probe=$?
case "$probe" in
0) fail "192.0.2.1 accepted a connection" ;;
124) fail "192.0.2.1 is dropped, not refused; the run would prove nothing" ;;
esac

rc=0
if [ -n "$run_as" ]; then
    ip netns exec "$ns" sudo -E -u "$run_as" "$@" || rc=$?
else
    ip netns exec "$ns" "$@" || rc=$?
fi
exit "$rc"
