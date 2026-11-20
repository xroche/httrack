#!/bin/sh
#
# Decide whether HTTraQt upstream has moved away from the revision the canary
# job pins; httraqt-upstream.yml does the telling.
#
# Prints pinned=, upstream= and status= (unchanged, moved or gone), also to
# $GITHUB_OUTPUT. A broken watch exits non-zero.

set -eu

WORKFLOW=".github/workflows/httraqt.yml"
UPSTREAM="https://git.code.sf.net/p/httraqt/code"
ATTEMPTS=5

fail() {
    echo "$0: $*" >&2
    exit 1
}

usage() {
    echo "usage: $0 [--workflow FILE] [--upstream URL] [--attempts N]" >&2
    exit 2
}

while [ $# -gt 0 ]; do
    case "$1" in
    --workflow)
        WORKFLOW="${2?missing value}"
        shift 2
        ;;
    --upstream)
        UPSTREAM="${2?missing value}"
        shift 2
        ;;
    --attempts)
        ATTEMPTS="${2?missing value}"
        shift 2
        ;;
    -h | --help) usage ;;
    *) usage ;;
    esac
done

case "$ATTEMPTS" in
"" | 0 | *[!0-9]*) usage ;;
esac

# The pin is what the canary builds. The anchored match rejects a quoted or
# suffixed value.
pinned=$(sed -n 's/^[[:space:]]*HTTRAQT_REF:[[:space:]]*\([0-9a-f]\{40\}\)\([[:space:]].*\)\{0,1\}$/\1/p' "$WORKFLOW")
[ "${#pinned}" -eq 40 ] || fail "no single bare 40-hex HTTRAQT_REF in $WORKFLOW"

# A private SourceForge repo would otherwise block on a credential prompt.
export GIT_TERMINAL_PROMPT=0

attempt=1
while :; do
    if refs=$(timeout 60 git ls-remote "$UPSTREAM" 2>&1); then
        break
    fi
    echo "ls-remote failed (attempt $attempt/$ATTEMPTS): $refs" >&2
    [ "$attempt" -lt "$ATTEMPTS" ] || fail "$UPSTREAM did not answer"
    sleep $((attempt * 15))
    attempt=$((attempt + 1))
done

# An answer with no ref at all comes from a half-restored host.
[ -n "$refs" ] || fail "$UPSTREAM answered, but the repository is empty"

upstream=$(printf '%s\n' "$refs" | sed -n 's/^\([0-9a-f]\{40\}\)[[:space:]]*refs\/heads\/master$/\1/p')
if [ -z "$upstream" ]; then
    upstream=none
    status=gone
elif [ "$upstream" = "$pinned" ]; then
    status=unchanged
else
    status=moved
fi

for line in "pinned=$pinned" "upstream=$upstream" "status=$status"; do
    echo "$line"
    [ -z "${GITHUB_OUTPUT:-}" ] || echo "$line" >>"$GITHUB_OUTPUT"
done
