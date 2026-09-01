#!/usr/bin/env python3
"""Fail when a source uses a standard macro without including the header defining it.

htscodec.c reached INT_MAX through <openssl/evp.h>, which macOS with --disable-https
never includes (#1481).
"""

import os
import re
import subprocess
import sys

# Each header lists only the macros it alone defines.
MACROS_BY_HEADER = {
    "limits.h": """CHAR_BIT SCHAR_MIN SCHAR_MAX UCHAR_MAX CHAR_MIN CHAR_MAX MB_LEN_MAX
        SHRT_MIN SHRT_MAX USHRT_MAX INT_MIN INT_MAX UINT_MAX LONG_MIN LONG_MAX
        ULONG_MAX LLONG_MIN LLONG_MAX ULLONG_MAX PATH_MAX NAME_MAX SSIZE_MAX""",
    "stdint.h": """SIZE_MAX INTPTR_MAX INTPTR_MIN UINTPTR_MAX INT8_MAX INT8_MIN
        INT16_MAX INT16_MIN INT32_MAX INT32_MIN INT64_MAX INT64_MIN UINT8_MAX
        UINT16_MAX UINT32_MAX UINT64_MAX""",
}

# One pass over all four, because each can hold the others. The // inside "http://"
# opens no comment, and the lone quote in '"' opens no string. Getting that wrong
# blanked 6% of this tree and hid every macro in it.
COMMENT_OR_LITERAL = re.compile(
    r"/\*.*?\*/" r"|//(?:\\\n|[^\n])*" r"|'(?:\\.|[^'\\])*'" r'|"(?:\\.|[^"\\])*"',
    re.S,
)
LITERAL = re.compile(r"'(?:\\.|[^'\\])*'" r'|"(?:\\.|[^"\\])*"', re.S)

# The name a #define, #undef or #ifdef introduces is not a use, which is what the usual
# "#ifndef SIZE_MAX / #define SIZE_MAX ..." portability shim looks like.
DECLARED = re.compile(r"^[ \t]*#[ \t]*(?:define|undef|ifdef|ifndef)[ \t]+\w+", re.M)

CONDITIONAL = re.compile(r"^\s*#\s*(if|ifdef|ifndef|endif)\b")
IFNDEF = re.compile(r"\s*#\s*ifndef\s+(\w+)")
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')

SOURCES = ["*.c", "*.h", "*.cpp", "*.cxx", "*.m", "*.c.in", "*.h.in"]

# Well under the tree's own count, so it catches an empty listing without tracking every
# file added.
MIN_SOURCES = 50


def keep_literal(match):
    """Drop a comment, keep a string or char literal."""
    return match.group(0) if match.group(0)[0] in "\"'" else ""


def guards_itself(lines):
    """Does the file open with the usual #ifndef NAME / #define NAME pair?

    That pair wraps every header in the tree, so counting it as a condition would
    reject the includes of all of them.
    """
    for index, line in enumerate(lines):
        if not CONDITIONAL.match(line):
            continue
        name = IFNDEF.match(line)
        if not name:
            return False
        for follow in lines[index + 1 :]:
            if not follow.strip():
                continue
            return re.match(r"\s*#\s*define\s+%s\b" % name.group(1), follow) is not None
        return False
    return False


def unconditional_includes(code):
    """Which headers does this file include outside every #if block?

    A macro used unconditionally is not covered by a header included only under
    #ifdef _WIN32, which is the other half of #1481.
    """
    lines = code.splitlines()
    top = 1 if guards_itself(lines) else 0
    depth = 0
    found = set()
    for line in lines:
        conditional = CONDITIONAL.match(line)
        if conditional:
            depth += -1 if conditional.group(1) == "endif" else 1
        elif depth == top:
            include = INCLUDE.match(line)
            if include:
                found.add(include.group(1))
    return found


def main():
    try:
        root = subprocess.run(
            ["git", "rev-parse", "--show-toplevel"],
            capture_output=True,
            text=True,
            check=True,
        ).stdout.strip()
        # git ls-files answers relative to the cwd, so from anywhere but the top it
        # would skip whole directories and still report success.
        os.chdir(root)
        listed = subprocess.run(
            ["git", "ls-files"] + SOURCES, capture_output=True, text=True, check=True
        ).stdout
    except (OSError, subprocess.CalledProcessError):
        print(
            "no git checkout here, so the tracked file list is unreadable",
            file=sys.stderr,
        )
        return 1

    sources = listed.split()
    if len(sources) < MIN_SOURCES:
        print(
            f"listed {len(sources)} sources, want {MIN_SOURCES} or more",
            file=sys.stderr,
        )
        return 1

    findings = []
    for path in sources:
        with open(path, encoding="latin-1") as handle:
            text = handle.read()
        # Literals survive this pass, or #include "limits.h" would read as absent.
        uncommented = COMMENT_OR_LITERAL.sub(keep_literal, text)
        included = unconditional_includes(uncommented)
        code = DECLARED.sub("", LITERAL.sub("", uncommented))
        for header, macros in MACROS_BY_HEADER.items():
            if header in included:
                continue
            missing = sorted(
                m for m in macros.split() if re.search(r"\b%s\b" % m, code)
            )
            if missing:
                findings.append(f"{path}: {', '.join(missing)} without <{header}>")

    for line in findings:
        print(line, file=sys.stderr)
    if findings:
        print(f"{len(findings)} file(s) rely on a transitive include", file=sys.stderr)
        return 1
    print(f"{len(sources)} sources include the headers whose macros they use")
    return 0


if __name__ == "__main__":
    sys.exit(main())
