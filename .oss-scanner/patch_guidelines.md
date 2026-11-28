# How we would like patches made

## Size and scope of a fix

- Fix the root cause, and keep the patch small. Do not refactor around the fix.
- If the same bug exists in other places that use the same pattern, fix them in the same patch and list them.

## Code style

- Read `AGENTS.md` at the root first. It lists the project's C rules.
- Write a bounds check as `untrusted < limit - controlled`, never as `controlled + untrusted < limit`, because the sum can wrap.
- Use the helpers in `src/htssafe.h`, `src/htsbuf.h` and `src/htsurlport.h` instead of raw `strcpy`, `strcat`, `sprintf` or `malloc`. For data from the network or a cache file, clip it with `strclipbuff` rather than abort.
- Write comments in English, one line each, and only where the reason is not clear from the code.
- Format only the lines you change, with `git clang-format` (clang-format 19). Do not reformat whole files.

## Tests

- Add a test that fails without the fix. Prefer an engine self-test (`httrack -#test=NAME`, in the `hts<module>_selftest.c` file of that module) driven by a new `tests/NN_*.test` script. Use a local crawl test only when a self-test cannot reach the bug.
- If the bug is in an area with a fuzz harness in `fuzz/`, add the reproducer to that harness's corpus.

## Commit message

- The subject line says what the change does, in plain words.
- Credit the finding in the body, for example `Discovered by Anthropic's OSS Scanner, as vulnerability ANT-2026-XXXX.`
