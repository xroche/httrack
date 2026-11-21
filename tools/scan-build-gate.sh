#!/bin/bash
# Run the clang static analyzer over a configured build tree, and fail on any
# finding, or when the analyzer could not have reported one.
# Usage: bash tools/scan-build-gate.sh SRCDIR REPORTDIR, from the build dir.
set -euo pipefail

srcdir=$(cd "$1" && pwd)
reportdir=$2
scan_build=${SCAN_BUILD:-scan-build-19}
jobs=${JOBS:-$(nproc)}
mkdir -p "$reportdir"

# coucal and minizip are vendored, so their findings go to their upstreams.
# Dead stores are style, and unix.Errno flags every libc call after a failed one.
flags=(--status-bugs
    --exclude "$srcdir/src/coucal" --exclude "$srcdir/src/minizip"
    -disable-checker deadcode.DeadStores -disable-checker unix.Errno)

# The verdict line scan-build prints last, or nothing.
verdict() {
    grep -E '^scan-build: ([0-9]+ bugs? found|No bugs found)' "$1" || true
}

grep -q '^CC = .*ccc-analyzer' src/Makefile ||
    {
        echo "::error::src/Makefile does not compile through the analyzer"
        exit 1
    }

# A planted bug under src/ proves the analyzer, the exclusions and the verdict
# parsing can still see a finding.
probe=$(mktemp "$srcdir/src/scan-build-probe-XXXXXX.c")
trap 'rm -f "$probe"' EXIT
printf 'int probe(int *p) {\n  if (p == 0)\n    return *p;\n  return 0;\n}\n' >"$probe"
status=0
# shellcheck disable=SC2016 # $CC is the analyzer wrapper scan-build exports
"$scan_build" -o "$reportdir/probe" "${flags[@]}" \
    sh -c '"$CC" -c "$1" -o /dev/null' sh "$probe" >"$reportdir/probe.log" 2>&1 ||
    status=$?
if test "$status" -eq 0 || test "$(verdict "$reportdir/probe.log")" != "scan-build: 1 bug found."; then
    cat "$reportdir/probe.log"
    echo "::error::the analyzer did not report the planted null dereference"
    exit 1
fi
rm -rf "$reportdir/probe"

status=0
"$scan_build" -o "$reportdir/scan" "${flags[@]}" make -j"$jobs" \
    >"$reportdir/scan.log" 2>&1 || status=$?
summary=$(verdict "$reportdir/scan.log")
grep -E 'warning: .*\[[a-zA-Z]+\.[a-zA-Z.]+\]$' "$reportdir/scan.log" |
    grep -v -e "^$srcdir/src/coucal/" -e "^$srcdir/src/minizip/" || true
echo "${summary:-scan-build printed no verdict}"
# A build that never reached the analyzer also ends without findings.
if test -z "$summary"; then
    tail -50 "$reportdir/scan.log"
    echo "::error::scan-build printed no verdict"
    exit 1
fi
if compgen -G "$reportdir/scan/*/failures" >/dev/null; then
    echo "::error::the analyzer crashed on some files, see the report's failures/"
    exit 1
fi
case "$summary" in
"scan-build: No bugs found." | "scan-build: 0 bugs found.") ;;
*)
    echo "::error::$summary"
    exit 1
    ;;
esac
if test "$status" -ne 0; then
    tail -50 "$reportdir/scan.log"
    echo "::error::scan-build exited with status $status"
    exit 1
fi
