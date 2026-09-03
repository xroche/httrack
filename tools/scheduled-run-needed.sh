#!/bin/sh
#
# Decide whether a scheduled run has any work to do, for a job whose answer is a
# pure function of this tree. A job that reads a downstream recipe, a rolling
# container image or a live URL must NOT call this: a week in which we changed
# nothing is exactly the week the thing it watches moves.
#
# Prints needed= and age_days= on stdout, and to $GITHUB_OUTPUT when set.
# Anything but a schedule answers needed=true, so the button always works. So
# does an unreadable commit date, because a guard that suppresses the work it
# guards costs more than the runner minutes it saves.

set -eu

# One week, matching the cron of every current caller. A job on a slower cron
# passes its own --max-age-days.
MAX_AGE_DAYS=7
EVENT="${GITHUB_EVENT_NAME:-}"

usage() {
    echo "usage: $0 [--max-age-days N] [--event NAME]" >&2
    exit 2
}

while [ $# -gt 0 ]; do
    case "$1" in
    --max-age-days)
        MAX_AGE_DAYS="${2?missing value}"
        shift 2
        ;;
    --event)
        EVENT="${2?missing value}"
        shift 2
        ;;
    -h | --help) usage ;;
    *) usage ;;
    esac
done

case "$MAX_AGE_DAYS" in
"" | *[!0-9]*) usage ;;
esac

# The run log scrolls and the summary does not, so say it in both.
say() {
    echo "$1"
    [ -z "${GITHUB_STEP_SUMMARY:-}" ] || echo "$1" >>"${GITHUB_STEP_SUMMARY}"
}

answer() {
    for line in "needed=$1" "age_days=$2"; do
        echo "$line"
        [ -z "${GITHUB_OUTPUT:-}" ] || echo "$line" >>"${GITHUB_OUTPUT}"
    done
    exit 0
}

# Forcing a run is the first thing anyone does when investigating this job.
if [ "$EVENT" != schedule ]; then
    say "running: ${EVENT:-<no event>} is not a scheduled run, so the age of the tree decides nothing"
    answer true unknown
fi

# The committer date is when the commit landed on this branch, which for a
# squash merge is when master moved. A shallow checkout carries it, so this
# needs no API call, no token, and works the same on a fork.
head_epoch=$(git log -1 --format=%ct HEAD 2>/dev/null || true)
now=$(date +%s 2>/dev/null || true)
case "${head_epoch:-x}${now:-x}" in
*[!0-9]*)
    say "running: cannot read the head commit date here, so the guard stands aside"
    answer true unknown
    ;;
esac

age_days=$(((now - head_epoch) / 86400))
# A commit dated ahead of the runner clock is skew or a rewritten date.
[ "$age_days" -ge 0 ] || age_days=0
head_id=$(git log -1 --format=%h HEAD 2>/dev/null || echo HEAD)

if [ "$age_days" -lt "$MAX_AGE_DAYS" ]; then
    say "running: the tree moved ${age_days}d ago at ${head_id}, within the ${MAX_AGE_DAYS}d window"
    answer true "$age_days"
fi

msg="skipped: the tree has not moved in ${age_days}d (head ${head_id}, window ${MAX_AGE_DAYS}d)."
msg="$msg This job reads nothing outside the repository, so the push that made ${head_id} already produced this run's answer."
echo "::notice::$msg"
say "$msg"
answer false "$age_days"
