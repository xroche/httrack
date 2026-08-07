#!/bin/bash
#
# tests-list.mk and tests/*.test must stay a bijection (#1037): a stale
# registration kills make check silently, an unregistered file just never runs.

set -euo pipefail
export LC_ALL=C

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

# The sed below assumes one file per "TESTS +=" line; a continuation or a second
# name on a line would slip past unseen.
strays=$(grep -nv -e '^$' -e '^#' -e '^TESTS =$' -e '^TESTS += [^ ]*\.test$' "$list" || true)
[ -z "$strays" ] || bad "malformed tests-list.mk lines, want one 'TESTS += NN_name.test' each:
$strays"

sed -n 's/^TESTS += //p' "$list" | sort >"$tmp/all"
# A directory or a dangling symlink named NN_x.test is not a test file, and
# make check would choke on it; count it as absent.
for f in "$testdir"/*.test; do
    if [ -f "$f" ]; then printf '%s\n' "${f##*/}"; fi
done | sort >"$tmp/present"

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
