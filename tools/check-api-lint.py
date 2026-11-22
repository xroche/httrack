#!/usr/bin/env python3
"""Ratchet three API rules over src/ against the baselines in tools/api-lint/.

ctype: a ctype call takes an (unsigned char) argument.
macro: a function-like macro evaluates each argument once, uses only its
       arguments, and wraps statements in do { } while (0).
check-result: a header function that can return failure carries HTS_CHECK_RESULT.
              It reads return statements, so it misses a failure that only a
              tail call returns.

A violation missing from its baseline fails, and so does a baseline entry that no
longer occurs, so each list only shrinks. --update rewrites the baselines.
"""

import argparse
import collections
import glob
import os
import re
import sys

RULES = ("ctype", "macro", "check-result")
HEADER = "# Known %s violations. Regenerate: tools/check-api-lint.py --update\n"

# Vendored trees follow their upstream's conventions.
SOURCES = ["src/*.c", "src/*.h", "src/proxy/*.c", "src/proxy/*.h"]

# Well under the tree's count, so an empty listing fails instead of passing.
MIN_SOURCES = 100

CTYPE = (
    "isalnum isalpha isascii isblank iscntrl isdigit isgraph islower isprint "
    "ispunct isspace isupper isxdigit tolower toupper"
).split()
CTYPE_CALL = re.compile(r"(?<![\w.>])(" + "|".join(CTYPE) + r")\s*\(")
UCHAR_CAST = re.compile(r"^\(\s*(const\s+)?unsigned\s+char\s*\)")

KEYWORDS = set(
    """auto break case char const continue default do double else enum extern
    float for goto if inline int long register restrict return short signed
    sizeof static struct switch typedef union unsigned void volatile while
    _Bool _Alignof _Static_assert bool true false NULL defined typeof
    __typeof__ __inline __inline__ __attribute__ __extension__ __restrict
    __declspec""".split()
)
TYPE_WORDS = set(
    "char short int long float double void signed unsigned _Bool bool".split()
)
# Their operand is never evaluated.
UNEVALUATED = (
    "sizeof",
    "__typeof__",
    "typeof",
    "_Alignof",
    "__builtin_types_compatible_p",
)
# Process-wide names every C file may use.
LIBC_GLOBALS = {"errno", "stderr", "stdout", "stdin"}
STATEMENT_START = re.compile(
    r"^(\{|if\b|for\b|while\b|switch\b|return\b|goto\b|break\b)"
)
DO_WHILE0 = re.compile(r"^do\s*\{.*\}\s*while\s*\(\s*0\s*\)$", re.S)
DEFINE = re.compile(r"^[ \t]*#[ \t]*define[ \t]+(\w+)\(([^)]*)\)(.*)$")
IDENT = re.compile(r"(?<!\w)[A-Za-z_]\w*")

# A return that signals failure, by return class.
FAILURE = {
    "int": re.compile(r"\breturn\s*\(?\s*-\s*\d"),
    "hts_boolean": re.compile(r"\breturn\s*\(?\s*(HTS_FALSE|0|false)\s*\)?\s*;"),
    "pointer": re.compile(r"\breturn\s*\(?\s*(NULL|0)\s*\)?\s*;"),
}


def blank(text, start, end):
    """Replace text[start:end] by spaces, keeping newlines so lines still count."""
    return re.sub(r"[^\n]", " ", text[start:end])


def strip_comments(text):
    """Blank comments and the contents of literals, keeping their quotes."""
    out = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(blank(text, i, j))
            i = j
        elif text.startswith("//", i):
            j = i
            while j < n and text[j] != "\n":
                j += 2 if text[j] == "\\" and j + 1 < n and text[j + 1] == "\n" else 1
            out.append(blank(text, i, j))
            i = j
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            j = min(j, n - 1)
            out.append(c + blank(text, i + 1, j) + c)
            i = j + 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


def strip_preprocessor(text):
    """Blank every directive line and its continuations."""
    lines = text.split("\n")
    in_directive = False
    for k, line in enumerate(lines):
        if in_directive or line.lstrip().startswith("#"):
            in_directive = line.endswith("\\")
            lines[k] = " " * len(line)
    return "\n".join(lines)


def matching(text, open_at, pair="()"):
    """Index just past the bracket closing the one at open_at, or len(text)."""
    depth = 0
    for k in range(open_at, len(text)):
        if text[k] == pair[0]:
            depth += 1
        elif text[k] == pair[1]:
            depth -= 1
            if depth == 0:
                return k + 1
    return len(text)


def line_of(text, pos):
    return text.count("\n", 0, pos) + 1


def squash(s):
    return " ".join(s.split())


def check_ctype(path, code):
    """Yield (line, key, message) for each ctype call lacking the cast."""
    for m in CTYPE_CALL.finditer(code):
        open_at = m.end() - 1
        arg = code[open_at + 1 : matching(code, open_at) - 1].strip()
        if UCHAR_CAST.match(arg) or arg.startswith("'") or re.fullmatch(r"-?\d+", arg):
            continue
        call = squash(m.group(1) + "(" + arg + ")")
        yield line_of(code, m.start()), call, call + " needs an (unsigned char) cast"


def macros(code):
    """Yield (line, name, params, body) for each function-like #define."""
    lines = code.split("\n")
    k = 0
    while k < len(lines):
        first = k
        text = lines[k]
        while text.endswith("\\") and k + 1 < len(lines):
            k += 1
            text = text[:-1] + " " + lines[k]
        k += 1
        m = DEFINE.match(text)
        if m:
            params = [p.strip() for p in m.group(2).split(",") if p.strip()]
            yield first + 1, m.group(1), params, m.group(3).strip()


def spans(body, words):
    """Spans of body inside the parentheses following one of words."""
    for m in re.finditer(r"\b(" + "|".join(words) + r")\s*\(", body):
        yield m.end() - 1, matching(body, m.end() - 1)


def is_type(word, types):
    return word in TYPE_WORDS or word in types or word.endswith("_t")


def locals_of(body, types):
    """Names the body declares itself, as in do { size_t n = ...; } while (0)."""
    found = set()
    for m in re.finditer(
        r"([A-Za-z_]\w*)[\s*]+(?:const\s+)?([A-Za-z_]\w*)\s*[=;,\[)]", body
    ):
        if is_type(m.group(1), types):
            found.add(m.group(2))
    return found


def macro_problems(name, params, body, types):
    """List the rule-6 problems of one macro, each a stable phrase."""
    problems = []
    named = [p for p in params if p != "..."]
    unevaluated = list(spans(body, UNEVALUATED))
    attributes = list(spans(body, ("__attribute__", "__declspec")))
    declared = locals_of(body, types)
    uses = collections.Counter()
    free = set()
    for m in IDENT.finditer(body):
        ident, at = m.group(0), m.start()
        before = body[:at].rstrip()
        after = body[m.end() :].lstrip()
        if before.endswith("#") or after.startswith("##"):
            continue
        if ident in named or ident == "__VA_ARGS__":
            if not any(a <= at < b for a, b in unevaluated):
                uses[ident] += 1
            continue
        if (
            before.endswith((".", "->"))
            or after.startswith("(")
            or re.search(r"\b(struct|union|enum)$", before)
            or ident in KEYWORDS
            or ident in declared
            or ident in LIBC_GLOBALS
            or is_type(ident, types)
            or any(a <= at < b for a, b in attributes)
            or ident.startswith("__")
            or ident.endswith("_t")
            or re.fullmatch(r"[A-Z0-9_]+", ident)
        ):
            continue
        free.add(ident)
    upper = re.fullmatch(r"[A-Z0-9_]+", name) is not None
    if not upper and "__builtin_choose_expr" not in body:
        for p in sorted(p for p in uses if uses[p] > 1):
            problems.append("evaluates %s twice" % p)
    for ident in sorted(free):
        problems.append("uses free name %s" % ident)
    statements = STATEMENT_START.match(body) or depth0_semicolon(body)
    if statements and not DO_WHILE0.match(body):
        problems.append("holds statements outside do { } while (0)")
    return problems


def depth0_semicolon(body):
    """Is there a ; outside any parenthesis, the mark of a statement?"""
    depth = 0
    for c in body:
        depth += c == "("
        depth -= c == ")"
        if c == ";" and depth == 0:
            return True
    return False


def collect_types(codes):
    """Typedef names across the tree, which a macro body may name freely."""
    types = set()
    for code in codes:
        flat = strip_preprocessor(code)
        for m in re.finditer(r"\btypedef\b([^;{]*(\{)?)", flat):
            if m.group(2):
                end = matching(flat, m.start(2), "{}")
                decl = flat[end : flat.find(";", end)]
            else:
                decl = m.group(1)
            fn = re.search(r"\(\s*\*\s*(\w+)\s*\)", decl)
            names = (
                [fn.group(1)] if fn else re.findall(r"(\w+)\s*(?:\[[^]]*\])?\s*$", decl)
            )
            types.update(names)
    return types


def top_level(code):
    """Yield (start, head, body) per top-level declaration: body is None for a ;."""
    flat = strip_preprocessor(code)
    flat = re.sub(r'\bextern\s*"\s*C\s*"\s*\{', lambda m: " " * len(m.group(0)), flat)
    start = 0
    k = 0
    while k < len(flat):
        c = flat[k]
        if c == "(":
            k = matching(flat, k)
            continue
        if c == ";":
            yield start, flat[start:k], None
            start = k = k + 1
            continue
        if c == "{":
            end = matching(flat, k, "{}")
            head = flat[start:k]
            if re.search(r"\)\s*$", head):
                yield start, head, flat[k:end]
                start = k = end
            else:
                k = end
            continue
        if c == "}":
            start = k + 1
        k += 1


def function_head(head):
    """(name, return text) of a function declaration head, else None."""
    if re.search(r"\b(typedef)\b|=", head):
        return None
    for m in re.finditer(r"([A-Za-z_]\w*)\s*\(", head):
        name = m.group(1)
        if re.fullmatch(r"[A-Z0-9_]+", name) or name in KEYWORDS:
            continue
        ret = re.sub(r"\b(__attribute__|[A-Z][A-Z0-9_]*)\s*\(", "", head[: m.start()])
        if not ret.strip() or "(" in ret:
            return None
        return name, head[: m.start()]
    return None


def return_class(ret):
    ret = re.sub(r"\b[A-Z][A-Z0-9_]*(\s*\([^)]*\))?", " ", ret)
    if "*" in ret:
        return "pointer"
    words = ret.split()
    if words and words[-1] == "hts_boolean":
        return "hts_boolean"
    if words and words[-1] == "int" and "unsigned" not in words:
        return "int"
    return None


def yes_no(body):
    """Does an int function return 0 and otherwise only 1 or a boolean expression?"""
    values = {
        squash(v).strip("() ") for v in re.findall(r"\breturn\b\s*([^;]*);", body)
    }
    boolean = re.compile(r"^!|==|!=|<|>|&&|\|\||^1$|^0$|^strnotempty\b")
    return "0" in values and len(values) > 1 and all(boolean.search(v) for v in values)


def check_result(files):
    """Yield (path, line, key, message) for header functions that can fail unmarked."""
    bodies = {}
    marked = set()
    decls = []
    for path, code in files.items():
        for start, head, body in top_level(code):
            fh = function_head(head)
            if not fh:
                continue
            name, ret = fh
            if "HTS_CHECK_RESULT" in head:
                marked.add(name)
            if body is not None and "static" not in ret.split():
                bodies.setdefault(name, body)
            if path.endswith(".h"):
                decls.append((path, start, head, name, ret, body))
    for path, start, head, name, ret, body in decls:
        cls = return_class(ret)
        body = body if body is not None else bodies.get(name)
        if cls is None or body is None or name in marked:
            continue
        if FAILURE[cls].search(body) or (cls == "int" and yes_no(body)):
            line = line_of(files[path], start + len(head) - len(head.lstrip()))
            msg = "%s returns failure (%s) without HTS_CHECK_RESULT" % (name, cls)
            yield path, line, name, msg


def scan(root):
    """Map each rule to a list of (path, line, key, message)."""
    paths = sorted({p for pat in SOURCES for p in glob.glob(os.path.join(root, pat))})
    if len(paths) < MIN_SOURCES:
        sys.exit("check-api-lint: only %d sources under %s" % (len(paths), root))
    files = {}
    for p in paths:
        rel = os.path.relpath(p, root).replace(os.sep, "/")
        with open(p, encoding="utf-8", errors="surrogateescape") as f:
            files[rel] = strip_comments(f.read())
    found = {rule: [] for rule in RULES}
    for path, code in files.items():
        for line, key, msg in check_ctype(path, code):
            found["ctype"].append((path, line, key, msg))
    types = collect_types(files.values())
    for path, code in files.items():
        for line, name, params, body in macros(code):
            for problem in macro_problems(name, params, body, types):
                key = "%s: %s" % (name, problem)
                found["macro"].append((path, line, key, "macro " + key))
    found["check-result"] = list(check_result(files))
    return found


def baseline_path(bdir, rule):
    return os.path.join(bdir, rule + ".txt")


def read_baseline(path):
    with open(path, encoding="utf-8") as f:
        return collections.Counter(
            line.rstrip("\n") for line in f if line.strip() and not line.startswith("#")
        )


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--root", default=os.path.dirname(here), help="tree to scan")
    ap.add_argument("--baseline-dir", default=os.path.join(here, "api-lint"))
    ap.add_argument("--update", action="store_true", help="rewrite the baselines")
    args = ap.parse_args()

    found = scan(args.root)
    status = 0
    for rule in RULES:
        current = collections.Counter(
            "%s: %s" % (p, key) for p, _, key, _ in found[rule]
        )
        bpath = baseline_path(args.baseline_dir, rule)
        if args.update:
            with open(bpath, "w", encoding="utf-8") as f:
                f.write(HEADER % rule)
                f.writelines(e + "\n" for e in sorted(current.elements()))
            continue
        known = read_baseline(bpath)
        new = current - known
        for path, line, key, msg in found[rule]:
            entry = "%s: %s" % (path, key)
            if new[entry] > 0:
                print("%s:%d: %s: new violation: %s" % (path, line, rule, msg))
                status = 1
        for entry in sorted((known - current).elements()):
            print("%s: %s: stale baseline entry, delete it: %s" % (bpath, rule, entry))
            status = 1
    return status


if __name__ == "__main__":
    sys.exit(main())
