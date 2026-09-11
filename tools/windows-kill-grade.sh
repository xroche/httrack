#!/bin/bash
#
# Tell a lost runner (#1228) from a real failure, for the jobs of one
# windows-build run. Reads the `actions/runs/<id>/jobs` payload on stdin and
# exits 0 when every failed job is a runner death, so the run is safe to re-run.
#
# Stricter than tools/windows-kill-census.sh on purpose: the census has to count
# an ambiguous job, this one must refuse to act on it.
set -euo pipefail

# The runner goes away mid-step, so the live step and the ones after it stay
# unfinished and none is marked failed. A real failure names the step it failed.
# A job with no steps at all never started one, which is the same wedge.
verdict=$(jq -r '
    [.jobs[]? | select(.conclusion == "failure")] as $failed
    | [$failed[] | select(
        ((.steps // []) | any(.conclusion == "failure")) | not)
      | select(
        ((.steps // []) | length) == 0
        or ((.steps // []) | any(.conclusion == null)))] as $killed
    | if ($failed | length) == 0 then
        "no\tno failed job"
      elif ($killed | length) == ($failed | length) then
        "yes\t" + (($failed | length) | tostring) + " failed job(s), none naming a step"
      else
        "no\t" + ([$failed[] | select((.steps // []) | any(.conclusion == "failure")) | .name]
                  | join(", ")) + " named a failed step"
      end')

echo "${verdict#*$'\t'}"
test "${verdict%%$'\t'*}" = yes
