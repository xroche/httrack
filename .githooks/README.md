# Git hooks

Versioned hooks for this repo. Enable them once per clone:

```sh
git config core.hooksPath .githooks
```

## pre-commit: auto-format changed C lines

Runs `git-clang-format` (clang-format 19, using the repo `.clang-format`) on the
**staged lines only** and re-stages the result, so every commit is
clang-format-clean and the CI `format` check passes. It never reformats the
whole tree, only the lines you changed.

- Disable for a single commit: `HTTRACK_NO_AUTOFORMAT=1 git commit ...`
- If clang-format 19 isn't installed, the hook skips silently (CI still
  enforces). Install it with your distro's `clang-format-19`, or from
  apt.llvm.org.
- If a file has *both* staged and unstaged changes, the hook does not
  auto-mutate it (that would commit the unstaged part); it instead reports
  whether its staged lines need formatting and asks you to stage/stash the rest.

### noexec working trees

Git executes the hook directly, so if your working tree is on a `noexec` mount
git cannot run `.githooks/pre-commit`. Point `core.hooksPath` at a copy on an
exec filesystem instead:

```sh
mkdir -p ~/.httrack-hooks && cp .githooks/pre-commit ~/.httrack-hooks/
chmod +x ~/.httrack-hooks/pre-commit
git config core.hooksPath ~/.httrack-hooks
```
</content>
