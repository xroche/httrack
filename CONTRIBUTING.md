# Contributing to HTTrack

HTTrack is small and old. Keep changes easy to review and safe to merge.

## Pull requests

- One change per PR. Small diffs merge fast.
- PRs are squash-merged: the title and description become the commit message, so
  explain *why*.
- Add or update tests for engine changes (`tests/`), and keep CI green.

## Style

- C, matching nearby code. **Format only the lines you change** (`git
  clang-format` against the repo `.clang-format`). Never reformat untouched code.
- Comment the *why*, in English.
- HTTrack parses hostile input off the network. Check bounds, avoid unchecked
  copies, and never let an attacker-controlled length drive arithmetic unchecked.

## Sign your work

Every commit needs a `Signed-off-by` line, the
[DCO](https://developercertificate.org/): `git commit -s`. CI rejects unsigned
commits; fix a branch with `git rebase --signoff master`.

## AI assistants

Welcome, and nothing to disclose. Two rules:

- **Own every line** as if you wrote it. Can't explain it in review? Not ready.
- **Don't push your work onto reviewers.** A raw generated patch a maintainer has
  to vet from scratch will be closed.

The sign-off covers AI-assisted code too.

## Bugs

Open an issue with the version, OS, command used, and expected vs actual result.
For security issues see [SECURITY.md](SECURITY.md), not a public issue.
