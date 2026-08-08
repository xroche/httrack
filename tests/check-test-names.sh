#!/bin/bash
#
# configure globs [0-9]*_*.test into TESTS, so a file named anything else is
# silently never run (#1037); a directory or dangling symlink chokes the driver.

set -euo pipefail
export LC_ALL=C

testdir=$(cd "${1:-$(dirname "$0")}" && pwd)

status=0
bad() {
    echo "$*" >&2
    status=1
}

shopt -s nullglob
files=("$testdir"/*.test)
shopt -u nullglob

count=0
for f in "${files[@]}"; do
    name=${f##*/}
    if [ ! -f "$f" ]; then
        bad "not a regular file, so make check would choke on it: $name"
    elif [[ $name != [0-9]*_*.test ]]; then
        bad "outside the [0-9]*_*.test glob, so make check never runs it: $name"
    else
        count=$((count + 1))
    fi
done

# Floor: an empty tests/ would satisfy every check above.
[ "$count" -gt 0 ] || bad "no test matched [0-9]*_*.test under $testdir"

if [ "$status" -eq 0 ]; then
    echo "test names: $count files, each one picked up by the make check glob"
fi
exit "$status"
