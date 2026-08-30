#!/bin/bash
#
# Runs a command on the network an rpm or deb buildroot has: loopback alone, but
# carrying a default route, and no resolver. mock installs exactly that (its
# condUnshareNet adds "default via 127.0.0.1"), which makes a UDP connect to any
# address succeed and name no source, and #1463 read that 0.0.0.0 as an address.
#
# Usage: tools/buildroot-net.sh <command> [args...]      (re-execs under sudo)

set -euo pipefail

fail() {
    echo "buildroot-net: $*" >&2
    exit 1
}

# Second entry, inside both namespaces: mount away the resolver and drop back to
# the invoking user, since the suite must not run as root.
if [ "${1:-}" = "--in-namespace" ]; then
    mount --bind /dev/null /etc/resolv.conf
    run_as=$2
    shift 2
    test -n "$run_as" || exec "$@"
    exec sudo -E -u "$run_as" "$@"
fi

test $# -ge 1 || fail "usage: $0 <command> [args...]"

if [ "$(id -u)" -ne 0 ]; then
    exec sudo -E bash "$0" "$@"
fi

ns="buildrootnet$$"

trap 'set +e; ip netns del "$ns" 2>/dev/null' EXIT
trap 'exit 1' HUP INT TERM

ip netns add "$ns"
ip -n "$ns" link set lo up
ip -n "$ns" route add default via 127.0.0.1

# Non-vacuity: a namespace answering with a real address, or refusing the
# connect outright, is not the regime this leg exists to cover.
src=$(ip netns exec "$ns" python3 -c 'import socket
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
try:
    s.connect(("192.0.2.1", 9))
    print(s.getsockname()[0])
except OSError as e:
    print(type(e).__name__)
finally:
    s.close()')
test "$src" = "0.0.0.0" ||
    fail "the namespace answers $src, not the wildcard a buildroot names"

rc=0
ip netns exec "$ns" unshare --mount --propagation private \
    bash "$0" --in-namespace "${SUDO_USER:-}" "$@" || rc=$?
exit "$rc"
