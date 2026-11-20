#!/usr/bin/env python3
"""Reject machine-sounding phrasing in the comments a change adds.

Usage: check-comments.py [--base REV] [FILE...]
With --base, checks the comment lines added since REV; with files, checks all
their comments (used by the self-test).
"""
import argparse
import io
import os
import re
import subprocess
import sys
import tokenize

# Phrases that gesture at a fact instead of stating it, with the fix.
# Kept in step with BANNED in the maintainer's check-prose.py.
BANNED = {
    "load-bearing": "say what breaks without it",
    "by construction": "say why it holds",
    "the whole point": "state the point",
    "the point is": "state the point",
    "is the tell": "say what it shows",
    "in practice": "drop it",
    "worth noting": "drop it",
    "it should be noted": "drop it",
    "earns its place": "say why it is there",
    "belt and braces": "say what each check catches",
    "not assumed": "state the fact",
    "rather than assumed": "state the fact",
}

# A slogan opening a comment, such as "Verbed, not verbed:".
SLOGAN = re.compile(r"^[A-Z][a-z]+, not [a-z]+[:.]")

C_EXT = (".c", ".h", ".cpp", ".js")
HASH_EXT = (".sh", ".test", ".yml", ".yaml", ".am", ".ac", ".m4")


def c_comments(text, line_comments=True):
    """Map each line number to its comment text, skipping string literals."""
    out, line, i, n = {}, 1, 0, len(text)

    def add(at, s):
        out[at] = (out.get(at, "") + " " + s.strip(" *\t")).strip()

    while i < n:
        c = text[i]
        if c == "\n":
            line += 1
            i += 1
        elif c in "\"'":
            i += 1
            while i < n and text[i] not in (c, "\n"):
                i += 2 if text[i] == "\\" else 1
            if i < n and text[i] == c:
                i += 1
        elif text.startswith("/*", i):
            end = text.find("*/", i + 2)
            end = n if end < 0 else end
            for k, part in enumerate(text[i + 2 : end].split("\n")):
                add(line + k, part)
            line += text.count("\n", i, end)
            i = end + 2
        elif line_comments and text.startswith("//", i):
            end = text.find("\n", i)
            end = n if end < 0 else end
            add(line, text[i + 2 : end])
            i = end
        else:
            i += 1
    return out


def py_comments(text):
    out = {}
    try:
        for tok in tokenize.generate_tokens(io.StringIO(text).readline):
            if tok.type == tokenize.COMMENT:
                out[tok.start[0]] = tok.string.lstrip("#").strip()
    except (tokenize.TokenError, SyntaxError):
        pass
    return out


def hash_comments(text):
    """Whole-line # comments only, because a trailing # needs a shell parser."""
    out = {}
    for lineno, line in enumerate(text.split("\n"), 1):
        s = line.lstrip()
        if s.startswith("#") and not s.startswith("#!"):
            out[lineno] = s.lstrip("#").strip()
    return out


def comments(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        text = f.read()
    if path.endswith(C_EXT):
        return c_comments(text)
    if path.endswith(".css"):
        return c_comments(text, line_comments=False)
    if path.endswith(".py"):
        return py_comments(text)
    if path.endswith(HASH_EXT):
        return hash_comments(text)
    return {}


def check(path, only, findings):
    for lineno, text in sorted(comments(path).items()):
        if only is not None and lineno not in only:
            continue
        low = text.lower()
        for phrase, fix in BANNED.items():
            if re.search(r"\b" + re.escape(phrase) + r"\b", low):
                findings.append(f"{path}:{lineno}: '{phrase}': {fix}")
        if SLOGAN.match(text):
            findings.append(f"{path}:{lineno}: slogan opener: state the fact plainly")


def git(*args):
    return subprocess.run(
        ["git", *args],
        capture_output=True,
        text=True,
        errors="replace",
        check=True,
    ).stdout


def added_lines(base):
    """Yield each file changed since base with the set of its added lines."""
    names = git("diff", "--name-only", "-z", "--diff-filter=AMRC", base, "--")
    for path in filter(None, names.split("\0")):
        hunks = git("diff", "--no-ext-diff", "-U0", base, "--", path)
        added = set()
        for m in re.finditer(r"^@@ -\S+ \+(\d+)(?:,(\d+))? @@", hunks, re.M):
            start, count = int(m.group(1)), int(m.group(2) or 1)
            added.update(range(start, start + count))
        if added:
            yield path, added


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base")
    ap.add_argument("files", nargs="*")
    args = ap.parse_args()
    findings = []
    if args.base:
        os.chdir(git("rev-parse", "--show-toplevel").strip())
        for path, added in added_lines(args.base):
            check(path, added, findings)
    for path in args.files:
        check(path, None, findings)
    for f in findings:
        print(f)
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
