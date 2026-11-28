# Report guidelines

## Layout and length

- Keep each report under one page. Use these sections in this order: Summary, Attacker, Reproduction, Impact, Suggested fix.
- Report one root cause per report. When the same bug is reachable from several places, list the places in one report.
- Start the title with the source file or component, for example `htsparse: heap overflow when a tag ends at the buffer end`.

## What every report states

- Name the commit you scanned.
- State the attacker's position, in the terms of `threat_model.md`. For example, the attacker is a malicious web server, or a web page open in the user's browser while WebHTTrack runs.
- Give a reproducer that we can run on this tree. The best reproducer is a small file tree to serve, the `httrack` command line that fetches it, and the sanitizer output. A `-#test=NAME` self-test call or a fuzz harness input is also good.
- Say what the attacker gains, such as a write outside the mirror folder, code execution, or only a crash. Say so if you did not show more than a crash.

## Style

- Use plain English and short sentences. Many readers are not native speakers.
- Do not include long logs. Keep the first sanitizer frame inside `src/` and up to ten frames around it.
