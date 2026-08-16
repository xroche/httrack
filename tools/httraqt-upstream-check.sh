#!/bin/sh
#
# Decide whether HTTraQt upstream has moved away from the revision the canary
# job pins; httraqt-upstream.yml does the telling.
#
# Prints pinned=, upstream= and status= on stdout, and to $GITHUB_OUTPUT when
# set. status is unchanged, moved, or gone (the host answers but has no master
# branch). An unreadable pin and a host that will not answer both exit non-zero:
# neither is news about HTTraQt, and both mean the watch has stopped working.

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

# The pin is the state: nothing else can disagree with what the canary builds.
# Anchored, so a quoted or suffixed value is a hard error rather than a silent
# near-miss, and two matching lines overshoot the length check.
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

# An answer carrying no ref at all is a half-restored host, not a rename.
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
