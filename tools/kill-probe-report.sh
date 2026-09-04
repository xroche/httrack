#!/bin/bash
#
# Census for the Windows runner-kill probe (#1228): the kill rate per arm, with
# the live windows-build suite as the positive control. A run of the probe that
# shows no kill in the control arm measures nothing, so the control is printed
# beside the arms rather than remembered.
set -euo pipefail

repo=xroche/httrack
since=

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
# (#1228). A killed job keeps no log and no artifact, so the step records are
# the whole evidence: the runner dies mid-step, leaving that step's conclusion
# null where a test failure leaves "failure".
census() {
    local wf=$1 runs id
    # A 404 is the probe before its first run, not an error worth losing the
    # control row over.
    runs=$(gh api --paginate -X GET "repos/$repo/actions/workflows/$wf/runs" \
        -f "created=>=$since" -f per_page=100 \
        -q '.workflow_runs[] | select(.status == "completed") | .id') || {
        echo "no runs of $wf in $repo" >&2
        return 0
    }
    for id in $runs; do
        gh api "repos/$repo/actions/runs/$id/jobs?filter=all&per_page=100" -q \
            '.jobs[] | select(.status == "completed") | [.name, .conclusion,
             ((.steps // []) | map(select(.conclusion == null)) | length)] | @tsv'
    done
}

# Wilson, not the normal approximation: at a handful of kills in a few dozen
# jobs the latter puts the bound below zero and reads as certainty.
report() {
    awk -F'\t' '
        $2 == "cancelled" { censored[$1]++; next }
        { jobs[$1]++; if ($2 != "success" && $3 > 0) kills[$1]++ }
        END {
            printf "%-34s %6s %6s %8s %17s %10s\n",
                "job", "jobs", "kills", "rate", "95% CI", "cancelled"
            for (k in jobs) {
                n = jobs[k]; x = kills[k] + 0; p = x / n; z = 1.96
                d = 1 + z * z / n
                c = (p + z * z / (2 * n)) / d
                w = z * sqrt(p * (1 - p) / n + z * z / (4 * n * n)) / d
                lo = c - w; hi = c + w
                if (lo < 0) lo = 0
                printf "%-34s %6d %6d %7.1f%% %8.1f%% - %5.1f%% %9d\n",
                    k, n, x, 100 * p, 100 * lo, 100 * hi, censored[k] + 0
            }
        }' | (read -r header && echo "$header" && sort)
}

echo "window: $since .. today, repo $repo"
echo
{
    census windows-kill-probe.yml
    census windows-build.yml
} | report
