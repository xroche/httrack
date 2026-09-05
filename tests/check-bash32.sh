#!/bin/bash
#
# macOS runs the suite under /bin/bash, which is bash 3.2. A construct it does
# not know is a fatal expansion error there: the script aborts mid-test, and
# testlib.sh's EXIT trap (kept for #773) exits 0, so automake records a PASS
# (#1540). bash 3.2 zeroes $? before entering that trap, so the trap cannot
# recover the real status -- catching the constructs statically is the only
# defence left. "bash -n" catches none of them: they all parse, and fail when
# the expansion runs.
#
# Only constructs bash 5 accepts can hide this way. Anything bash 5 rejects
# reds every Linux leg on the first run.
#
# NOT covered, on purpose: "${a[@]}" on an empty array, which bash 3.2 treats
# as unbound under "set -u". Whether an array is empty at a given line is not
# statically decidable, and the tree holds a few hundred such expansions that
# are fine, so a pattern for it would be noise rather than a lint. Do not
# "complete" the list with it.

# Every pattern and sample below is literal bash text, never an expansion.
# shellcheck disable=SC2016

set -euo pipefail
export LC_ALL=C

testdir=$(cd "${1:-$(dirname "$0")}" && pwd)
self=$(basename "$0")

status=0
bad() {
    echo "$*" >&2
    status=1
}

# One row per class: message, extended regex, and a line that must match it.
labels=()
regexes=()
samples=()
add() {
    labels+=("$1")
    regexes+=("$2")
    samples+=("$3")
}

add 'negative array subscript, bash 4.3+' \
    '\$\{#?[A-Za-z_][A-Za-z0-9_]*\[[[:space:]]*-' \
    'last=${arr[-1]}'
add 'mapfile/readarray, bash 4.0+' \
    '(^|[;&|(]|[[:space:]])(mapfile|readarray)[[:space:]]' \
    'mapfile -t lines <input'
add 'associative array (declare -A), bash 4.0+' \
    '(^|[;&|(]|[[:space:]])(declare|local|typeset)[[:space:]]+-[A-Za-z]*A' \
    'declare -A seen'
add 'case conversion ${v^^} / ${v,,}, bash 4.0+' \
    '\$\{#?[A-Za-z_][A-Za-z0-9_]*(\[[^]]*\])?(\^|,)' \
    'up=${name^^}'
add 'nameref (declare -n), bash 4.3+' \
    '(^|[;&|(]|[[:space:]])(declare|local|typeset)[[:space:]]+-[A-Za-z]*n' \
    'local -n ref=$1'

# A lint nobody can see fail is worth nothing, so prove each pattern still
# matches its own known-bad line, and that a plain one matches none of them.
clean='printf "%s\n" "${files[@]}" | sort'
i=0
while [ "$i" -lt "${#labels[@]}" ]; do
    if ! command grep -qE "${regexes[$i]}" <<<"${samples[$i]}"; then
        bad "self-check: the pattern for ${labels[$i]} no longer matches its own sample"
    fi
    if command grep -qE "${regexes[$i]}" <<<"$clean"; then
        bad "self-check: the pattern for ${labels[$i]} matches a plain line"
    fi
    i=$((i + 1))
done

# Strip comments first, or a comment naming one of these trips the lint. Only a
# "#" opening a line or following whitespace, so "$#" and "${#a[@]}" survive.
strip='s/^[[:space:]]*#.*$//; s/[[:space:]]#.*$//'

count=0
shopt -s nullglob
for f in "$testdir"/*.test "$testdir"/*.sh; do
    name=${f##*/}
    # This file carries a known-bad line of every class as its own control.
    [ "$name" != "$self" ] || continue
    body=$(sed "$strip" "$f")
    count=$((count + 1))
    i=0
    while [ "$i" -lt "${#labels[@]}" ]; do
        hits=$(command grep -nE "${regexes[$i]}" <<<"$body" || true)
        if [ -n "$hits" ]; then
            while IFS= read -r hit; do
                bad "$name:${hit%%:*}: ${labels[$i]}: ${hit#*:}"
            done <<<"$hits"
        fi
        i=$((i + 1))
    done
done
shopt -u nullglob

# Floor: an empty tests/ would satisfy every check above.
[ "$count" -gt 0 ] || bad "no .test or .sh file found under $testdir"

if [ "$status" -eq 0 ]; then
    echo "bash 3.2: $count files, none using a construct macOS would abort on"
else
    echo "macOS runs these under bash 3.2, where the abort is laundered into a PASS." >&2
fi
exit "$status"
