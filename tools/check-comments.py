#!/usr/bin/env python3
"""Reject machine-sounding phrasing in the comments a change adds.

Usage: check-comments.py [--base REV] [FILE...]
With --base, checks the comment lines added since REV; with files, checks all
their comments (used by the self-test).
"""
import argparse
import bisect
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
    out, i, n = {}, 0, len(text)
    newlines = [m.start() for m in re.finditer("\n", text)]

    def word_before(at):
        j = at
        while j > 0 and (text[j - 1].isalnum() or text[j - 1] == "_"):
            j -= 1
        return text[j:at]

    def add(start, end):
        line = bisect.bisect_left(newlines, start) + 1
        for k, part in enumerate(text[start:end].split("\n")):
            part = part.strip(" *\t\\")
            out[line + k] = (out.get(line + k, "") + " " + part).strip()

    while i < n:
        c = text[i]
        word = word_before(i) if c in "\"'`" else ""
        if c == '"' and word in ("R", "LR", "uR", "UR", "u8R"):
            m = re.match(r'"([^()\\\s]{0,16})\(', text[i:])
            if m:
                end = text.find(")" + m.group(1) + '"', i)
                i = n if end < 0 else end + len(m.group(1)) + 2
                continue
        if c == "'" and word[:1].isdigit():
            i += 1  # digit separator, as in 1'000
        elif c in "\"'`":
            stop = (c,) if c == "`" else (c, "\n")
            i += 1
            while i < n and text[i] not in stop:
                i += 2 if text[i] == "\\" else 1
            i += 1
        elif text.startswith("/*", i):
            end = text.find("*/", i + 2)
            end = n if end < 0 else end
            add(i + 2, end)
            i = end + 2
        elif line_comments and text.startswith("//", i):
            end = i
            while True:
                end = text.find("\n", end)
                if end < 0:
                    end = n
                    break
                if text[end - 1] != "\\":
                    break
                end += 1
            add(i + 2, end)
            i = end
        else:
            i += 1
    return out


def py_comments(text, path):
    out = {}
    try:
        for tok in tokenize.generate_tokens(io.StringIO(text).readline):
            if tok.type == tokenize.COMMENT:
                out[tok.start[0]] = tok.string.lstrip("#").strip()
    except (tokenize.TokenError, SyntaxError) as e:
        sys.exit(f"{path}: cannot tokenize: {e}")
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
    if path.endswith(C_EXT):
        scan = c_comments
    elif path.endswith(".css"):
        scan = lambda text: c_comments(text, line_comments=False)  # noqa: E731
    elif path.endswith(".py"):
        scan = lambda text: py_comments(text, path)  # noqa: E731
    elif path.endswith(HASH_EXT):
        scan = hash_comments
    else:
        return {}
    with open(path, encoding="utf-8", errors="replace") as f:
        return scan(f.read())


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
    status = git("diff", "--name-status", "-z", "-M", "--ignore-submodules", base)
    fields = iter(status.split("\0"))
    for kind in fields:
        if not kind:
            continue
        paths = [next(fields)]
        if kind[0] in "RC":
            paths.append(next(fields))
        path = paths[-1]
        if kind[0] == "D" or os.path.islink(path):
            continue
        hunks = git("diff", "--no-ext-diff", "-M", "-U0", base, "--", *paths)
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
