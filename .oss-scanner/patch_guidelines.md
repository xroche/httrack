# Patch guidelines

## Size and scope of a fix

- Fix the root cause, and keep the patch small. Do not refactor around the fix.
- If the same bug exists in other places that use the same pattern, fix them in the same patch and list them.

## Code style and tests

- Follow `AGENTS.md` at the root. It gives the bounds-check form, the safe string and buffer helpers, the comment style, the formatter and how to write a test.
- Add a test that fails without the fix. Use a local crawl test only when an engine self-test cannot reach the bug.
- If the bug is in an area with a fuzz harness in `fuzz/`, add the reproducer to that harness's corpus.

## Commit message

- The subject line says what the change does, in plain words.
- Credit the finding in the body, for example `Discovered by Anthropic's OSS Scanner, as vulnerability ANT-2026-XXXX.`
