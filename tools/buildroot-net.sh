#!/bin/bash
#
# Runs a command on the network an rpm buildroot has: loopback alone, with a
# default route and no resolver. Fedora's mock builds that, and a UDP connect
# there succeeds while naming no source, which #1463 read as an address.
# Debian's sbuild leaves out the default route, so it lands on the skip path.
#
# Usage: tools/buildroot-net.sh <command> [args...]      (re-execs under sudo)

set -euo pipefail

fail() {
    echo "buildroot-net: $*" >&2
    exit 1
}

# Second entry, inside both namespaces: mount away the resolver and drop back to
# the invoking user, since the suite must not run as root. On an env var, not an
# argument: as a flag, a root caller typing it by hand unmounts the real one.
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

# Non-vacuity: a namespace that names a real address, or that refuses the
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
BUILDROOT_NET_INNER="user=${SUDO_USER:-}" ip netns exec "$ns" \
    unshare --mount --propagation private bash "$0" "$@" || rc=$?
exit "$rc"
