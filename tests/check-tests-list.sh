#!/bin/bash
#
# tests-list.mk and tests/*.test must stay a bijection: a registration whose file
# is gone makes "make check" exit 2 before any test runs, with no "# TOTAL:" line
# at all, and an unregistered file is silently never run (#1037).

set -euo pipefail

testdir=$(cd "${1:-$(dirname "$0")}" && pwd)
list="${testdir}/tests-list.mk"
[ -r "$list" ] || {
    echo "not readable: $list" >&2
    exit 2
}

status=0
bad() {
    echo "$*" >&2
    status=1
}

tmp=$(mktemp -d "${TMPDIR:-/tmp}/tests-list.XXXXXX")
trap 'set +e; rm -rf "$tmp"' EXIT

# The extraction below takes one file per "TESTS +=" line, so a continuation or a
# second name on a line would slip past the comparison unseen.
strays=$(grep -nv -e '^$' -e '^#' -e '^TESTS =$' -e '^TESTS += [^ ]*\.test$' "$list" || true)
[ -z "$strays" ] || bad "malformed tests-list.mk lines, want one 'TESTS += NN_name.test' each:
$strays"

sed -n 's/^TESTS += //p' "$list" | sort >"$tmp/all"
for f in "$testdir"/*.test; do printf '%s\n' "${f##*/}"; done | sort >"$tmp/present"

# Dedupe before comparing, or a repeated entry also reads as an absent file.
dup=$(uniq -d "$tmp/all")
[ -z "$dup" ] || bad "registered twice: $dup"
uniq "$tmp/all" >"$tmp/registered"

missing=$(comm -23 "$tmp/registered" "$tmp/present")
[ -z "$missing" ] || bad "registered but absent, so make check dies with no results: $missing"

extra=$(comm -13 "$tmp/registered" "$tmp/present")
[ -z "$extra" ] || bad "present but unregistered, so never run: $extra"

if [ "$status" -eq 0 ]; then
    echo "tests-list.mk: $(wc -l <"$tmp/registered") registrations, each matching a file"
fi
exit "$status"
