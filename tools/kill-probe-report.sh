#!/bin/bash
#
# Kill-rate census per arm (#1228), with windows-build as the control, because a
# window whose control shows no kill measures nothing.
set -euo pipefail

repo=xroche/httrack
since=
# GitHub drops a job's step records after about a week, and the wedge signature
# lives in them. Past that age a kill is recognised by its runtime instead: the
# runner lease reaps a lost job at ~45 min, where a real failure ends in ~15.
reaped_minutes=40

usage() {
    echo "usage: $0 [--repo OWNER/NAME] [--since YYYY-MM-DD]" >&2
    exit 2
}

while [ $# -gt 0 ]; do
    case $1 in
    --repo)
        repo=${2:?}
        shift 2
        ;;
    --since)
        since=${2:?}
        shift 2
        ;;
    *) usage ;;
    esac
done
[ -n "$since" ] || since=$(python3 -c \
    'import datetime; print(datetime.date.today() - datetime.timedelta(days=7))')

# filter=all, or a rerun hides the killed attempt and the census reads ~40% low
# (#1228). A killed job keeps no log and no artifact, so what survives is the job
# record: an unfinished step, or no steps at all once they age out, or a job the
# run left behind with no conclusion. $2 is the event filter, which keeps the
# probe's smoke-dose PR jobs out of its scheduled full-dose census.
census() {
    local wf=$1 event=${2:-} runs id filter=() total
    test -n "$event" && filter=(-f "event=$event")
    total=$(gh api -X GET "repos/$repo/actions/workflows/$wf/runs" \
        -f "created=>=$since" -f per_page=1 "${filter[@]}" -q .total_count 2>/dev/null) || {
        echo "no runs of $wf in $repo" >&2
        return 0
    }
    # The run listing caps at 1000 and serves newest first, so a wider window
    # drops its own oldest end without saying so.
    if [ "$total" -gt 1000 ]; then
        echo "$wf: $total runs since $since, only the newest 1000 are readable" >&2
    fi
    runs=$(gh api --paginate -X GET "repos/$repo/actions/workflows/$wf/runs" \
        -f "created=>=$since" -f per_page=100 "${filter[@]}" \
        -q '.workflow_runs[] | select(.status == "completed") | .id')
    for id in $runs; do
        gh api "repos/$repo/actions/runs/$id/jobs?filter=all&per_page=100" -q \
            '.jobs[] | [.name, (.conclusion // "none"),
             ((.steps // []) | map(select(.conclusion == null)) | length),
             ((.steps // []) | length),
             (if .completed_at and .started_at
              then (((.completed_at | fromdate) - (.started_at | fromdate)) / 60 | floor)
              else -1 end)] | @tsv'
    done
}

# Columns: job name, conclusion, unfinished steps, step count, runtime in
# minutes. A cancelled job is censored exposure and counts in neither column.
# Wilson, not the normal approximation: at a handful of kills in a few dozen jobs
# the latter puts the bound below zero and reads as certainty.
report() {
    awk -F'\t' -v reaped="$reaped_minutes" '
        $2 == "cancelled" { censored[$1]++; next }
        {
            jobs[$1]++
            if ($2 == "success") next
            if ($3 > 0) { kills[$1]++; next }
            # No steps left to read, so fall back to the runtime. A job the run
            # abandoned has no runtime at all, and that is the same wedge.
            if ($4 == 0 && ($5 < 0 || $5 >= reaped)) { kills[$1]++; aged[$1]++ }
        }
        END {
            printf "%-34s %6s %6s %8s %17s %10s %8s\n",
                "job", "jobs", "kills", "rate", "95% CI", "cancelled", "by age"
            for (k in jobs) {
                n = jobs[k]; x = kills[k] + 0; p = x / n; z = 1.96
                d = 1 + z * z / n
                c = (p + z * z / (2 * n)) / d
                w = z * sqrt(p * (1 - p) / n + z * z / (4 * n * n)) / d
                lo = c - w; hi = c + w
                if (lo < 0) lo = 0
                printf "%-34s %6d %6d %7.1f%% %8.1f%% - %5.1f%% %9d %8d\n",
                    k, n, x, 100 * p, 100 * lo, 100 * hi, censored[k] + 0, aged[k] + 0
            }
        }' | (read -r header && echo "$header" && sort)
}

echo "window: $since .. today, repo $repo"
echo
{
    census windows-kill-probe.yml schedule
    census windows-build.yml
} | report
