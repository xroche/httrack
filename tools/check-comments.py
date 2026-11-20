#!/usr/bin/env python3
"""Reject machine-sounding phrasing in the comments a change adds.

Usage: check-comments.py [--base REV] [FILE...]
With --base, checks the comment lines added since REV; with files, checks all
their comments (used by the self-test).
"""
import argparse
import os
import re
import subprocess
import sys

# Phrases that gesture at a fact instead of stating it, with the fix.
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
SLOGAN = re.compile(r"^[A-Z][a-z]+(?:ly)?, not [a-z]+[:.]")

C_EXT = (".c", ".h", ".cpp", ".js", ".css")
HASH_EXT = (".sh", ".test", ".py", ".yml", ".yaml", ".am", ".ac", ".m4")


def comment_text(path, line, state):
    """Return the comment part of one source line, tracking /* */ in state."""
    if path.endswith(HASH_EXT):
        s = line.lstrip()
        if s.startswith("#") and not s.startswith("#!"):
            return s.lstrip("#").strip()
        return ""
    if not path.endswith(C_EXT):
        return ""
    out = []
    i = 0
    while i < len(line):
        if state["in"]:
            end = line.find("*/", i)
            out.append(line[i : end if end >= 0 else len(line)])
            if end < 0:
                break
            state["in"] = False
            i = end + 2
            continue
        start = line.find("/*", i)
        slash = line.find("//", i)
        if (
            slash >= 0
            and (start < 0 or slash < start)
            and "://" not in line[slash - 1 : slash + 2]
        ):
            out.append(line[slash + 2 :])
            break
        if start < 0:
            break
        state["in"] = True
        i = start + 2
    return " ".join(t.strip(" *") for t in out).strip()


def check(path, lineno, text, findings):
    low = text.lower()
    for phrase, fix in BANNED.items():
        if re.search(r"\b" + re.escape(phrase) + r"\b", low):
            findings.append(f"{path}:{lineno}: '{phrase}': {fix}")
    if SLOGAN.match(text):
        findings.append(f"{path}:{lineno}: slogan opener: state the fact plainly")


def scan(path, findings, only=None):
    """Check the comments of path, or only those on the line numbers given."""
    state = {"in": False}
    try:
        f = open(path, encoding="utf-8", errors="replace")
    except OSError:
        return
    with f:
        for lineno, line in enumerate(f, 1):
            text = comment_text(path, line, state)
            if text and (only is None or lineno in only):
                check(path, lineno, text, findings)


def added_lines(base):
    diff = subprocess.run(
        ["git", "diff", "-U0", "--no-color", base, "--"],
        capture_output=True,
        text=True,
        errors="replace",
        check=True,
    ).stdout
    path, lineno = None, 0
    for raw in diff.split("\n"):
        if raw.startswith("+++ "):
            path = raw[6:] if raw.startswith("+++ b/") else None
        elif raw.startswith("@@"):
            lineno = int(re.search(r"\+(\d+)", raw).group(1))
        elif raw.startswith("+") and path:
            yield path, lineno
            lineno += 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base")
    ap.add_argument("files", nargs="*")
    args = ap.parse_args()
    findings = []
    if args.base:
        top = subprocess.run(
            ["git", "rev-parse", "--show-toplevel"],
            capture_output=True,
            text=True,
            check=True,
        ).stdout.strip()
        os.chdir(top)
        added = {}
        for path, lineno in added_lines(args.base):
            added.setdefault(path, set()).add(lineno)
        for path, linenos in added.items():
            scan(path, findings, linenos)
    for path in args.files:
        scan(path, findings)
    for f in findings:
        print(f)
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
