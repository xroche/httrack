#!/bin/bash
#
# Runs a command on the network an rpm buildroot has: loopback alone, with a
# default route and no resolver. Fedora's mock builds that, and a UDP connect
# there succeeds while naming no source (#1463).
#
# Usage: tools/buildroot-net.sh <command> [args...]      (re-execs under sudo)

set -euo pipefail

fail() {
    echo "buildroot-net: $*" >&2
    exit 1
}

# The second entry hides the resolver and drops root. It is an env var, so a
# root caller cannot unmount the real resolver with a typed flag.
if [ -n "${BUILDROOT_NET_INNER:-}" ]; then
    mount --bind /dev/null /etc/resolv.conf
    run_as=${BUILDROOT_NET_INNER#user=}
    unset BUILDROOT_NET_INNER # the payload gets a clean environment
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

# The leg covers a connect that succeeds without naming a real address.
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
BUILDROOT_NET_INNER="user=${SUDO_USER:-}" ip netns exec "$ns" \
    unshare --mount --propagation private bash "$0" "$@" || rc=$?
exit "$rc"
