#!/bin/bash
#
# Build and run the suite under qemu-user, the way a Debian buildd does. Runs
# inside a foreign-architecture container, from the checkout; the workflow that
# drives it is .github/workflows/emulated-suite.yml.

set -euo pipefail

fail() {
    echo "emulated-suite: $*" >&2
    exit 1
}

want=${EMULATED_ARCH:?EMULATED_ARCH must name the architecture under test}
test "$(dpkg --print-architecture)" = "$want" ||
    fail "container is $(dpkg --print-architecture), not $want"

# The property this leg exists for: binfmt_misc gives argv[0] to the interpreter
# and the program lands one place right, which is what hid the engine from the
# harness on hppa (#1146). A runner that stops emulating would keep the leg green
# while covering nothing, so read it back rather than trusting --platform.
read -r -d '' argv0 </proc/self/cmdline || true
case "${argv0##*/}" in
qemu-* | *-binfmt*) ;;
*) fail "not emulated: /proc/self/cmdline leads with '${argv0:-<empty>}'" ;;
esac
echo "emulated by ${argv0}"

apt-get update
# No python3, as on the buildds: the tests needing it skip there too, and this
# leg is for the ones that run.
apt-get install -y --no-install-recommends \
    build-essential autoconf automake libtool autoconf-archive \
    zlib1g-dev libssl-dev libbrotli-dev libzstd-dev ca-certificates

bash ./bootstrap
mkdir -p /bld
cd /bld
bash "${GITHUB_WORKSPACE:-/src}/configure"
make -j"$(nproc)"
# The buildd's own invocation, so a failure here is the one it would report.
make check -j"$(nproc)"
