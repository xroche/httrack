#!/bin/sh
#
# Compare HTTraQt upstream against the revision the canary job pins. Deciding
# only; httraqt-upstream.yml does the telling.
#
# Prints pinned=, upstream= and status= on stdout, and to $GITHUB_OUTPUT when
# set. status is unchanged, moved, gone (no master branch) or unreachable (the
# host did not answer, which says nothing about upstream). Non-zero exit means
# the pin itself could not be read, so the comparison never happened.

set -eu

WORKFLOW=".github/workflows/httraqt.yml"
UPSTREAM="https://git.code.sf.net/p/httraqt/code"
ATTEMPTS=3

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

# The pin is the state: no cache, no committed sidecar file, and no way for the
# two to disagree about what we last saw.
pinned=$(sed -n 's/^[[:space:]]*HTTRAQT_REF:[[:space:]]*\([0-9a-f][0-9a-f]*\).*/\1/p' "$WORKFLOW")
case "$pinned" in
????????????????????????????????????????) ;;
*)
    echo "$0: no single 40-hex HTTRAQT_REF in $WORKFLOW" >&2
    exit 1
    ;;
esac

# A private SourceForge repo would otherwise block on a credential prompt.
export GIT_TERMINAL_PROMPT=0

refs=
attempt=1
while [ "$attempt" -le "$ATTEMPTS" ]; do
    if refs=$(timeout 60 git ls-remote "$UPSTREAM" 'refs/heads/master' 2>&1); then
        break
    fi
    echo "ls-remote failed (attempt $attempt/$ATTEMPTS): $refs" >&2
    refs=
    [ "$attempt" -eq "$ATTEMPTS" ] || sleep $((attempt * 15))
    attempt=$((attempt + 1))
done

if [ "$attempt" -gt "$ATTEMPTS" ]; then
    upstream=unknown
    status=unreachable
else
    upstream=$(printf '%s\n' "$refs" | sed -n 's/^\([0-9a-f]\{40\}\)[[:space:]]*refs\/heads\/master$/\1/p')
    if [ -z "$upstream" ]; then
        upstream=none
        status=gone
    elif [ "$upstream" = "$pinned" ]; then
        status=unchanged
    else
        status=moved
    fi
fi

for line in "pinned=$pinned" "upstream=$upstream" "status=$status"; do
    echo "$line"
    [ -z "${GITHUB_OUTPUT:-}" ] || echo "$line" >>"$GITHUB_OUTPUT"
done
