#!/usr/bin/env python3
"""List log calls that print an engine address without hiding its password.

A log call is hts_log_print(), hts_log_vprint() or fprintf() to a log file.
Its arguments must pass every adr, url_adr or urladr() through adr_nologin()
or url_nologin(). An allowed call is named "file: format" in ALLOW.
"""

import re
import sys

CALL = re.compile(r"\b(hts_log_print|hts_log_vprint|fprintf)\s*\(")
LOG_FILE = re.compile(r"fprintf\s*\(\s*(opt->log|log|fp_log)\b")
SAFE = re.compile(
    r"\b(adr_nologin|url_nologin|jump_identification_const|jump_identification)"
    r"\s*\("
)
ADDRESS = re.compile(r"\b\w*adr\b")
NULL_TEST = re.compile(r"\b\w*adr\b\s*(\(\s*\))?\s*[!=]=\s*NULL")
# Calls whose address argument is not printed as text, with the reason.
ALLOW = {
    "htsback.c: Illegal chunk (%s) for %s%s": "chunk_adr is a chunk size",
    "htsback.c: illegal chunk CRLF (%s) for %s%s": "chunk_adr is a chunk size",
    "htscore.c: dump 1024 bytes (address %p): ": "a pointer",
    "htsparse.c: data attribute is no URL: %.*s": "eadr ends the value",
}


def call_end(s, i):
    """Returns the index after the ')' closing the call opened before i."""
    depth, quote = 1, None
    while depth and i < len(s):
        c = s[i]
        if quote:
            if c == "\\":
                i += 1
            elif c == quote:
                quote = None
        elif c in "\"'":
            quote = c
        elif c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
        i += 1
    return i


def strip_literals(s):
    return re.sub(r'"(\\.|[^"\\])*"' + r"|'(\\.|[^'\\])*'", '""', s)


def strip_safe(s):
    while True:
        m = SAFE.search(s)
        if m is None:
            return s
        s = s[: m.start()] + s[call_end(s, m.end()) :]


def first_format(call):
    m = re.search(r'"((\\.|[^"\\])*)"', call)
    return m.group(1) if m else ""


def main(paths):
    hits = 0
    used, names = set(), set()
    for path in paths:
        with open(path, encoding="latin-1") as f:
            src = f.read()
        name = path.rsplit("/", 1)[-1]
        names.add(name)
        for m in CALL.finditer(src):
            call = src[m.start() : call_end(src, m.end())]
            if m.group(1) == "fprintf" and not LOG_FILE.match(call):
                continue
            args = strip_safe(strip_literals(call[call.index("(") + 1 :]))
            args = NULL_TEST.sub("", args)
            if not ADDRESS.search(args):
                continue
            key = "%s: %s" % (name, first_format(call))
            if key in ALLOW:
                used.add(key)
                continue
            line = src.count("\n", 0, m.start()) + 1
            print("%s:%d: %s" % (path, line, " ".join(call.split())))
            hits += 1
    for key in sorted(ALLOW):
        if key.split(":")[0] in names and key not in used:
            print("stale ALLOW entry: %s" % key)
            hits += 1
    print("%d files, %d hits" % (len(paths), hits), file=sys.stderr)
    return 1 if hits else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
