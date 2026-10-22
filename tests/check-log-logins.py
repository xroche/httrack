#!/usr/bin/env python3
"""List log calls that print an engine address without hiding its password.

A log call is hts_log_print(), hts_log_vprint(), hts_secret_fprintf(),
printf(), puts(), or fprintf(), fputs() or fprintfio() to a log file, stdout
or stderr. Its
arguments must pass every identifier containing adr, url, lien or link, and
every line or codebase, through adr_nologin() or url_nologin(). A log file
includes hts-ioinfo.txt. An allowed call is named "file: format" in ALLOW.
Code under "#if 0" or a debug switch such as "#if HDEBUG" is skipped. A call
through a macro wrapper, or an address held in an alias variable, is not
detected.
"""

import re
import sys

CALL = re.compile(
    r"\b(hts_log_print|hts_log_vprint|hts_secret_fprintf|printf|puts|fprintf|fputs"
    r"|fprintfio)\s*\("
)
STREAM_FIRST = ("fprintf", "fprintfio")
LOG_FILE = re.compile(r"(opt->log|log|fp_log|ioinfo|stdout|stderr)\s*$")
SAFE = re.compile(
    r"\b(adr_nologin|url_nologin|adr_nologin_r|url_nologin_r|jump_identification_const"
    r"|jump_identification|text_excerpt)\s*\("
)
NAME = r"\b(\w*(adr|url|lien|link)\w*|line|codebase)\b"
ADDRESS = re.compile(NAME)
# Names that hold a path, a local file, a count or a mode, never an address.
NOT_ADDRESS = re.compile(
    r"\w*fil|\w*_sav\w*|lien_n|lien_tot|maxlink|\w*forbidden_url|urlmode"
    r"|catch_url_strerror|shown_url"
)
NULL_TEST = re.compile(NAME + r"\s*(\(\s*\))?\s*[!=]=\s*NULL")
SIZEOF = re.compile(r"\bsizeof\s*\(\s*\w+\s*\)")
DEBUG_IF = re.compile(r"\s*#\s*if(def)?\s+(0|\w*DEBUG\w*(\s*==\s*\d+)?)\s*(/[/*].*)?$")
# Calls whose address argument is not printed as text, with the reason.
ALLOW = {
    "htsback.c: Illegal chunk (%s) for %s%s": "chunk_adr is a chunk size",
    "htsback.c: illegal chunk CRLF (%s) for %s%s": "chunk_adr is a chunk size",
    "htscore.c: dump 1024 bytes (address %p): ": "a pointer",
    "htscoremain.c: X-Host: %s\\r\\n": "the -#C cache dump lists the entry",
    "htscoremain.c: X-URL: %s\\r\\nX-Cache-Entry-Unreadable: yes\\r\\n\\r\\n": "-#C",
    "htscoremain.c: X-URL: %s\\r\\n": "-#C",
    "htscoremain.c: %s\\r\\n": "r.adr is the page body",
    "htsback.c: >>> ": "r.adr is the response header block",
    "htscore.c: index redirect omitted: first link too long (%s)": "a local file",
    "htshelp.c: \\tProxy's address: \\t%s\\n\\tProxy's port: \\t%d\\n": "a local proxy",
    'htshelp.c: \\nThe URL is: \\"%s\\"\\n': "--catchurl hands the URL back",
    'htshelp.c: You can capture it through: httrack \\"%s\\"\\n': "--catchurl",
    "htsweb.c: URL=http://%s:%d/\\n": "the server's own address",
    "htsbauth.c: Bundle %s, line %d: %s": "filter->line is a line number",
    "htsbundle.c: Bundle %s, line %d: unknown key%s%.*s": "a lowercase key alone",
}


def drop_debug(src):
    """Blanks the lines a debug switch compiles out, keeping line numbers."""
    out, stack = [], []
    for line in src.split("\n"):
        if re.match(r"\s*#\s*if", line):
            stack.append(bool(DEBUG_IF.match(line)) or (stack and stack[-1]))
        elif re.match(r"\s*#\s*(else|elif)\b", line) and stack:
            stack[-1] = len(stack) > 1 and stack[-2]
        elif re.match(r"\s*#\s*endif\b", line) and stack:
            stack.pop()
            line = ""
        out.append("" if stack and stack[-1] else line)
    return "\n".join(out)


def drop_comments(src):
    """Blanks comments, keeping line numbers and string literals."""
    out, i, quote = [], 0, None
    while i < len(src):
        c = src[i]
        if quote:
            if c == "\\":
                out.append(src[i : i + 2])
                i += 2
                continue
            if c == quote:
                quote = None
        elif c in "\"'":
            quote = c
        elif src.startswith("/*", i):
            end = src.find("*/", i + 2)
            end = len(src) if end < 0 else end + 2
            out.append(re.sub(r"[^\n]", " ", src[i:end]))
            i = end
            continue
        elif src.startswith("//", i):
            end = src.find("\n", i)
            i = len(src) if end < 0 else end
            continue
        out.append(c)
        i += 1
    return "".join(out)


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


def stream(call, name):
    """Returns the FILE argument of an fprintf(), fprintfio() or fputs() call."""
    args = strip_literals(call[call.index("(") + 1 : -1])
    depth, parts, cur = 0, [], ""
    for c in args:
        if c == "," and depth == 0:
            parts.append(cur)
            cur = ""
            continue
        depth += {"(": 1, "[": 1, ")": -1, "]": -1}.get(c, 0)
        cur += c
    parts.append(cur)
    return (parts[0] if name in STREAM_FIRST else parts[-1]).strip()


def first_format(call):
    m = re.search(r'"((\\.|[^"\\])*)"', call)
    return m.group(1) if m else ""


def main(paths):
    hits = 0
    used, names = set(), set()
    for path in paths:
        with open(path, encoding="latin-1") as f:
            src = drop_debug(drop_comments(f.read()))
        name = path.rsplit("/", 1)[-1]
        names.add(name)
        for m in CALL.finditer(src):
            call = src[m.start() : call_end(src, m.end())]
            if m.group(1) in STREAM_FIRST + ("fputs",) and not LOG_FILE.search(
                stream(call, m.group(1))
            ):
                continue
            args = strip_safe(strip_literals(call[call.index("(") + 1 :]))
            args = SIZEOF.sub("", NULL_TEST.sub("", args))
            if all(NOT_ADDRESS.fullmatch(a.group(0)) for a in ADDRESS.finditer(args)):
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
