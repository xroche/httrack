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

# Below 100 the number is a family prefix shared on purpose (01_engine-, 53_,
# 74_), and from 100 up it identifies one test. Nothing rejected a duplicate
# before, so three landed in one day off branches that each read master right.
dup=$(printf '%s' "$present" |
    sed -n 's/^\([0-9]\{3,\}\)_.*/\1/p' | sort | uniq -d)
for n in $dup; do
    bad "number $n is taken by more than one test: $(
        printf '%s' "$present" | grep "^${n}_" | tr '\n' ' '
    )"
done

# The glob carries no oracle of its own: a test its author forgot to "git add",
# or one missing from a checkout, just makes the suite quietly smaller. Nothing
# tracked here means a tarball, where there is no oracle to apply.
# A git that cannot answer is not the same as an empty listing, so say which.
if ! command -v git >/dev/null 2>&1 || ! git -C "$testdir" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    tracked=""
    echo "note: no git checkout here that git can read, skipping the tracked-set check" >&2
else
    tracked=$(git -C "$testdir" ls-files -- '[0-9]*_*.test')
fi
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
