#!/bin/bash
#
# Tell a lost runner (#1228) from a real failure, for the jobs of one
# windows-build run. Reads the `actions/runs/<id>/jobs` payload on stdin and
# exits 0 when every failed job is a runner death, so the run is safe to re-run.
#
# This is stricter than tools/windows-kill-census.sh, which has to count an
# ambiguous job where this one must refuse to act on it.
set -euo pipefail

# The runner goes away mid-step, so the live step and the ones after it stay
# unfinished and none is marked failed. A real failure names the step it failed.
# A job with no steps at all never started one, which is the same wedge.
#
# "failure" is not the only way a job ends badly, so $bad collects every job that
# did not succeed and is not still running. A run holding one of the others, such
# as timed_out, is a human's to read even when a sibling leg was killed.
verdict=$(jq -r '
    def ended_badly: .conclusion != null and .conclusion != "success"
        and .conclusion != "skipped" and .conclusion != "cancelled";
    def names_a_step: (.steps // [])
        | any(.conclusion == "failure" or .conclusion == "timed_out");
    [.jobs[]? | select(ended_badly)] as $bad
    | [$bad[] | select(.conclusion == "failure")] as $failed
    | [$failed[] | select(names_a_step | not)
      | select(
        ((.steps // []) | length) == 0
        or ((.steps // []) | any(.conclusion == null)))] as $killed
    | if ($bad | length) == 0 then
        "no\tno failed job"
      elif ($bad | length) != ($failed | length) then
        "no\t" + ([$bad[] | select(.conclusion != "failure")
                   | .name + " ended " + .conclusion] | join(", "))
      elif ($killed | length) == ($failed | length) then
        "yes\t" + (($failed | length) | tostring) + " failed job(s), none naming a step"
      else
        "no\t" + ([$failed[] | select((names_a_step) or
                     (((.steps // []) | length) > 0
                      and ((.steps // []) | any(.conclusion == null) | not)))
                   | .name] | join(", ")) + " is not the wedge signature"
      end')

echo "${verdict#*$'\t'}"
test "${verdict%%$'\t'*}" = yes
