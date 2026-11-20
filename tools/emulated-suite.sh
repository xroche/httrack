#!/bin/bash
#
# Build and run the suite under qemu-user, which nothing else here does (#1148).
# Driven by .github/workflows/emulated-suite.yml, from inside the container.

set -euo pipefail

fail() {
    echo "emulated-suite: $*" >&2
    exit 1
}

want=${EMULATED_ARCH:?EMULATED_ARCH must name the architecture under test}
test "$(dpkg --print-architecture)" = "$want" ||
    fail "container is $(dpkg --print-architecture), not $want"

# binfmt_misc can put the interpreter in argv[0] (#1146). Read it off a CHILD,
# because qemu answers /proc/self/cmdline from the guest argv.
sleep 60 &
probe=$!
argv0=
for _ in $(seq 20); do
    read -r -d '' argv0 <"/proc/$probe/cmdline" || argv0=
    case "${argv0##*/}" in sleep | qemu-* | *-binfmt*) break ;; esac
    sleep 0.2
done
kill "$probe" 2>/dev/null || true
case "${argv0##*/}" in
qemu-* | *-binfmt*) ;;
*) fail "not emulated: a child's cmdline leads with '${argv0:-<empty>}'" ;;
esac
echo "emulated by $argv0"

apt-get update
# No python3, as on the buildds: the crawl tests skip there too, so what this
# leg covers is the engine self-tests and the install plumbing.
apt-get install -y --no-install-recommends \
    build-essential autoconf automake libtool autoconf-archive \
    zlib1g-dev libssl-dev libbrotli-dev libzstd-dev ca-certificates

bash ./bootstrap
mkdir -p /bld
cd /bld
bash "${GITHUB_WORKSPACE:-/src}/configure"
make -j"$(nproc)"
# The buildd's own invocation, so a failure here is the one it would report.
rc=0
make check -j"$(nproc)" || rc=$?
# Always print the log, so a paced-out skip shows its reason.
cat tests/test-suite.log || true
test "$rc" -eq 0 || exit "$rc"

# make check passes an all-SKIP run, so floor the PASS count.
passed=$(sed -n 's/^# PASS: *//p' tests/test-suite.log | head -1)
test "${passed:-0}" -ge "${EMULATED_PASS_FLOOR:-100}" ||
    fail "only ${passed:-0} tests passed, want ${EMULATED_PASS_FLOOR:-100} or more"
echo "emulated suite: $passed passed"
