#!/bin/bash
#
# configure globs fuzz/corpus/*/* into FUZZ_CORPUS_LIST, the only thing that
# ships the vectors in a tarball. The glob carries no oracle, so a file nobody
# git-added, one sitting a level deeper, or one whose name make would split
# leaves a smaller corpus with no complaint (three were missing that way).

set -euo pipefail
export LC_ALL=C

corpusdir=$(cd "${1:-$(dirname "$0")/corpus}" && pwd)

status=0
bad() {
    echo "$*" >&2
    status=1
}

present=""
count=0
shopt -s nullglob dotglob
for f in "$corpusdir"/*/*; do
    name=${f#"$corpusdir"/}
    if [ ! -f "$f" ]; then
        # configure skips it, so a directory here means vectors a level deeper
        bad "not a regular file, so the vectors under it never ship: $name"
    elif [[ $name == *[[:space:]]* ]]; then
        # configure joins the paths with spaces, so make sees two words.
        bad "whitespace in the name, which make would split in two: $name"
    else
        present="${present}${name}"$'\n'
        count=$((count + 1))
    fi
done
# a vector parked at the top level matches no glob and ships nowhere
for f in "$corpusdir"/*; do
    test -f "$f" || continue
    bad "directly under corpus/, outside the corpus/*/* glob: ${f#"$corpusdir"/}"
done
shopt -u nullglob dotglob

# Floor: an empty corpus/ would satisfy every check above.
[ "$count" -gt 0 ] || bad "no fuzz vector matched corpus/*/* under $corpusdir"

# Nothing tracked means a tarball, where there is no oracle to apply. A git
# that cannot answer is not the same as an empty listing, so say which.
if ! command -v git >/dev/null 2>&1 || ! git -C "$corpusdir" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    tracked=""
    echo "note: no git checkout here that git can read, skipping the tracked-set check" >&2
else
    tracked=$(git -C "$corpusdir" ls-files -- '*/*')
fi
if [ -n "$tracked" ]; then
    untracked=$(comm -13 <(sort <<<"$tracked") <(printf '%s' "$present" | sort))
    [ -z "$untracked" ] || bad "untracked, so the tarball would ship without them: $untracked"
    gone=$(comm -23 <(sort <<<"$tracked") <(printf '%s' "$present" | sort))
    [ -z "$gone" ] || bad "tracked but absent, so the corpus is short of them: $gone"
fi

if [ "$status" -eq 0 ]; then
    echo "$count fuzz corpus files, all tracked and reachable by the glob"
fi
exit "$status"
