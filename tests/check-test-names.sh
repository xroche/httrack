#!/bin/bash
#
# configure globs [0-9]*_*.test into TESTS, so a file named anything else is
# silently never run (#1037); a directory or dangling symlink chokes the driver.
# Only that exact spelling runs, so a parked 00_x.test.in or 00_x.test.disabled
# stays deliberately invisible to configure and to this check alike.

set -euo pipefail
export LC_ALL=C

testdir=$(cd "${1:-$(dirname "$0")}" && pwd)

status=0
bad() {
    echo "$*" >&2
    status=1
}

present=""
count=0
# nocaseglob so a 01_Engine.TEST, which configure would never pick up, is caught
# here rather than by nobody.
shopt -s nocaseglob nullglob
for f in "$testdir"/*.test; do
    name=${f##*/}
    if [ ! -f "$f" ]; then
        bad "not a regular file, so make check would choke on it: $name"
    elif [[ $name == *[[:space:]]* ]]; then
        # configure joins the basenames with spaces, so make sees two words.
        bad "whitespace in the name, which make would split in two: $name"
    elif [[ $name != [0-9]*_*.test ]]; then
        bad "outside the [0-9]*_*.test glob, so make check never runs it: $name"
    else
        present="${present}${name}"$'\n'
        count=$((count + 1))
    fi
done
shopt -u nocaseglob nullglob

# Floor: an empty tests/ would satisfy every check above.
[ "$count" -gt 0 ] || bad "no test matched [0-9]*_*.test under $testdir"

# The glob carries no oracle of its own: a test its author forgot to "git add",
# or one missing from a checkout, just makes the suite quietly smaller. Nothing
# tracked here means a tarball, where there is no oracle to apply.
tracked=$(git -C "$testdir" ls-files -- '[0-9]*_*.test' 2>/dev/null || true)
if [ -n "$tracked" ]; then
    untracked=$(comm -13 <(sort <<<"$tracked") <(printf '%s' "$present" | sort))
    [ -z "$untracked" ] || bad "untracked, so CI would run a smaller suite than you do: $untracked"
    gone=$(comm -23 <(sort <<<"$tracked") <(printf '%s' "$present" | sort))
    [ -z "$gone" ] || bad "tracked but absent, so the suite is short of them: $gone"
fi

if [ "$status" -eq 0 ]; then
    echo "test names: $count files, each one picked up by the make check glob"
fi
exit "$status"
