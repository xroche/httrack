# AGENTS.md — working in the HTTrack tree

Policy and PR etiquette live in [CONTRIBUTING.md](CONTRIBUTING.md). This file is
the operational checklist: toolchain, invariants, and how to ship a change.

## Build & test
- Fresh clone first: `git submodule update --init src/coucal`
- `bash configure && make && make check`

## Hard invariants
- **Toolchain edit** (`configure.ac`, any `Makefile.am`, `m4/`) → run
  `autoreconf -fi` and commit the regenerated tracked files. The repo ships the
  generated `configure`/`Makefile.in` so users build without autotools; CI does
  **not** catch staleness.
- **Format only changed lines** with `git clang-format` (clang-format 19). Never
  reformat untouched code: the engine was formatted by an old tool and won't
  round-trip.
- **Byte-safe edits.** Files with raw high bytes are ISO-8859-1 (French
  comments). Edit them byte-wise (`perl -0pi`, `sed`), not through a tool that
  re-encodes to UTF-8 and corrupts them.

## Security (HTTrack parses hostile input off the network)
- Bounds-check every copy. Overflow-safe form: put the untrusted value alone,
  `untrusted < limit - controlled` — never `controlled + untrusted < limit`,
  which can wrap and pass.

## Code & prose
- Be terse. Comment the why, in English; translate French comments you touch.
- Strip AI tells from prose (em-dash overuse, rule-of-three, filler, vague
  attributions). Ref: Wikipedia "Signs of AI writing". Claude Code: `/humanizer`.
- Behavior change → add a test. Fast path: a hidden `httrack -#N` debug
  subcommand (`htscoremain.c`) driven by a `tests/NN_*.test`, over a slow crawl.

## Review your change adversarially (strongly suggested)
Before pushing, and when reviewing others, don't skim for bugs:
- **One invariant at a time.** Name a property the diff must preserve (bounds
  hold, cache/wire format unchanged, no use-after-free, ABI stable), then
  construct inputs that would break it. "General correctness" is not a charter.
- **Audit tests against the spec, not the code.** For each new test ask: "what
  buggy path would still pass this?" If you can build one, the test is
  confirmation-biased: assertions copied from observed output lock bugs in.
- **Risk areas need runtime probes.** Touching hostile-input parsing, struct
  layout/ABI, cache/wire format, or a security path? A static or unit check
  isn't enough; exercise the wrong behavior at runtime. Claude Code:
  `/review-recipe`.

## Commits
- **Sign-off is mandatory.** Every commit carries a `Signed-off-by` trailer:
  `git commit -s` (DCO, CI-enforced — unsigned commits are rejected).
- **Co-Authored-By is mandatory for AI-assisted commits.** Carry a
  `Co-Authored-By:` trailer naming the assistant. Attribute there, never in a
  PR-body footer.
- PRs land as a merge commit; every commit on the branch goes onto master, so
  keep each commit message clean and meaningful.

## PR descriptions
- Plain concise prose; lead with what changed and why. No What/Why/How template.
- Title names the problem, not the implementation.
- Don't restate the diff — give what it can't show: motivation, context,
  tradeoffs, risk.
- Length tracks the change: a typo is one sentence; a security fix earns a writeup.
- Verify claims against the code before you write them; flag drift, don't repeat it.
- Don't hard-wrap (GitHub reflows). No "Generated with Claude" footer. Run the
  prose through `/humanizer`.

## Toolchain
C · clang-format-19 · autoreconf · shfmt + shellcheck (shell) · black + flake8 (Python)
