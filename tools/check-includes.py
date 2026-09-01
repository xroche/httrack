#!/usr/bin/env python3
"""Fail when a source uses a standard macro without including the header defining it.

htscodec.c reached INT_MAX through <openssl/evp.h>, so every build CI runs had the
header and macOS with --disable-https did not (#1481).
"""

import re
import subprocess
import sys

# One entry per header, listing only macros that header alone defines.
HEADERS = {
    "limits.h": """CHAR_BIT SCHAR_MIN SCHAR_MAX UCHAR_MAX CHAR_MIN CHAR_MAX MB_LEN_MAX
        SHRT_MIN SHRT_MAX USHRT_MAX INT_MIN INT_MAX UINT_MAX LONG_MIN LONG_MAX
        ULONG_MAX LLONG_MIN LLONG_MAX ULLONG_MAX PATH_MAX NAME_MAX SSIZE_MAX""",
    "stdint.h": """SIZE_MAX INTPTR_MAX INTPTR_MIN UINTPTR_MAX INT8_MAX INT8_MIN
        INT16_MAX INT16_MIN INT32_MAX INT32_MIN INT64_MAX INT64_MIN UINT8_MAX
        UINT16_MAX UINT32_MAX UINT64_MAX""",
}

# A macro named in a comment or a string is not a use.
NOISE = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"', re.S)

# The floor is well under the tree's own count, so it catches an empty listing
# rather than tracking every file added.
MIN_SOURCES = 50


def main():
    try:
        out = subprocess.run(
            ["git", "ls-files", "*.c", "*.h"],
            capture_output=True,
            text=True,
            check=True,
        )
    except (OSError, subprocess.CalledProcessError):
        print(
            "this needs a git checkout: it reads the tracked file list", file=sys.stderr
        )
        return 1
    sources = [f for f in out.stdout.split() if f]
    if len(sources) < MIN_SOURCES:
        print(
            f"listed {len(sources)} sources, want at least {MIN_SOURCES}",
            file=sys.stderr,
        )
        return 1

    bad = []
    for path in sources:
        with open(path, encoding="latin-1") as fp:
            text = fp.read()
        code = NOISE.sub("", text)
        for header, macros in HEADERS.items():
            if re.search(r"^\s*#\s*include\s*<%s>" % re.escape(header), text, re.M):
                continue
            used = sorted({m for m in macros.split() if re.search(r"\b%s\b" % m, code)})
            if used:
                bad.append(f"{path}: {', '.join(used)} without <{header}>")

    for line in bad:
        print(line, file=sys.stderr)
    if bad:
        print(f"{len(bad)} file(s) rely on a transitive include", file=sys.stderr)
        return 1
    print(f"{len(sources)} sources include the headers whose macros they use")
    return 0


if __name__ == "__main__":
    sys.exit(main())
