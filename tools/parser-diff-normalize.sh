#!/bin/bash
#
# Snapshot the mirror at $1 into $2, dropping what two runs of one binary differ
# on. tools/parser-diff.sh explains each exclusion; run this on its own to look
# at one mirror in the form the differential compares.
#
# Usage: bash tools/parser-diff-normalize.sh MIRRORDIR OUTDIR

set -euo pipefail

[ $# = 2 ] || {
    echo "usage: $0 MIRRORDIR OUTDIR" >&2
    exit 2
}
src=$1
dst=$2
[ -d "$src" ] || {
    echo "$0: no mirror at $src" >&2
    exit 2
}

rm -rf "$dst"
mkdir -p "$dst/files"
(cd "$src" && tar -cf - .) | (cd "$dst/files" && tar -xf -)
rm -rf "$dst/files/hts-cache" "$dst/files/hts-log.txt"

# The fetch manifest, minus the per-line fetch time and the server's Date, is
# the set of URLs the parser found and the file each was saved to. Always
# written, so a crawl with the cache off still compares against one.
: >"$dst/MANIFEST"
if [ -f "$src/hts-cache/new.txt" ]; then
    tab=$(printf '\t')
    sed -e "s/^[0-9][0-9]:[0-9][0-9]:[0-9][0-9]$tab//" \
        -e "s/${tab}date:[^$tab]*$tab/$tab/" "$src/hts-cache/new.txt" |
        LC_ALL=C sort >"$dst/MANIFEST"
fi
