#!/usr/bin/env python3
"""Fail on new ctype calls without an (unsigned char) cast, unsafe function-like
macros, header functions that can fail without HTS_CHECK_RESULT, and a span's
.p handed to a function that reads up to a NUL or printed with %s.
Known cases live in tools/api-lint/, and --update rewrites those baselines.
Known limits: a failure delegated through two calls, free names inside _Generic,
and a function defined only in a later branch of an unbalanced #if group go
unreported. A span is seen only as a local, a parameter or a URL view field, never through a
pointer copy, another struct or a macro, and only the NUL_READERS functions and
printf-style %s count, so a span handed to any other function goes unreported.
"""

import argparse
import collections
import os
import re
import sys

RULES = ("ctype", "macro", "check-result", "span")
HEADER = "# Known %s violations. Regenerate: tools/check-api-lint.py --update\n"

# Vendored trees follow their upstream's conventions.
VENDORED = ("src/coucal", "src/minizip")

# Well under the tree's count, so an empty listing fails instead of passing.
MIN_SOURCES = 100

CTYPE = (
    "isalnum isalpha isascii isblank iscntrl isdigit isgraph islower isprint "
    "ispunct isspace isupper isxdigit tolower toupper"
).split()
CTYPE_CALL = re.compile(r"(?<![\w.>])(" + "|".join(CTYPE) + r")\s*\(")
UCHAR_CAST = re.compile(r"^\(\s*(const\s+)?unsigned\s+char\s*\)")
# Exempt: a plain name declared int in the same function, since it may hold EOF.
QUALIFIERS = r"(?:(?:const|volatile|static|register|signed|unsigned|short|long)\s+)*"
DECL = (
    r"(?<![\w.>])(" + QUALIFIERS + r"(?:(?:struct|union|enum)\s+)?([A-Za-z_]\w*))"
    r"\s*((?:[\w*\s]+(?:=[^,;()]*(?:\([^()]*\))?)?,\s*)*)(\**)\s*%s\s*[=;,)\[]"
)

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
    "__builtin_constant_p",
    "HTS_SIZEOF_SRC_",  # htssafe.h reads its operand through sizeof only
)
# Library types a cast may name without a typedef in the tree.
LIBC_TYPES = {"va_list", "FILE", "DIR", "jmp_buf", "fd_set"}
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
    directive = re.match(r"[ \t]*#", text) is not None
    while i < n:
        c = text[i]
        if text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            gap = blank(text, i, j)
            if directive:
                # The comment does not end the directive, so keep it continued.
                rows = gap.split("\n")
                gap = "\\\n".join([r[:-1] for r in rows[:-1]] + rows[-1:])
            out.append(gap)
            i = j
        elif text.startswith("//", i):
            j = i
            while j < n and text[j] != "\n":
                j += 2 if text[j] == "\\" and j + 1 < n and text[j + 1] == "\n" else 1
            out.append(blank(text, i, j))
            i = j
        elif c in "\"'":
            # A literal never crosses a newline, so a stray quote eats one line.
            j = i + 1
            while j < n and text[j] not in (c, "\n"):
                j += 2 if text[j] == "\\" and text[j + 1 : j + 2] != "\n" else 1
            j = min(j, n)
            closed = j < n and text[j] == c
            inner = blank(text, i + 1, j)
            if not closed and j > i + 1 and text[j - 1] == "\\":
                inner = inner[:-1] + "\\"
            out.append(c + inner + (c if closed else ""))
            i = j + 1 if closed else j
        else:
            if c == "\n":
                continued = text[i - 1 : i] == "\\"
                directive = (directive and continued) or bool(
                    re.match(r"[ \t]*#", text[i + 1 : i + 80])
                )
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


def in_scope(text, decl, at):
    """Is a declaration at decl still in scope at position at?"""
    opener = enclosing_paren(text, decl)
    if opener is not None:
        before = text[:opener].rstrip()
        if before.endswith(")"):
            return False
        if re.search(r"\bfor$", before):
            close = matching(text, opener)
            rest = re.match(r"\s*\{", text[close:])
            end = (
                matching(text, close + rest.end() - 1, "{}")
                if rest
                else text.find(";", close)
            )
            return end < 0 or at <= end
    depth = 0
    for c in text[decl:at]:
        depth += c == "{"
        depth -= c == "}"
        if depth < 0:
            return False
    return True


def enclosing_paren(text, pos):
    """Index of the innermost ( still open at pos, or None."""
    depth = 0
    for k in range(pos - 1, -1, -1):
        if text[k] == ")":
            depth += 1
        elif text[k] == "(":
            if depth == 0:
                return k
            depth -= 1
        elif text[k] in "{};":
            return None
    return None


def int_variable(code, functions, at, arg, types):
    """Is the nearest declaration of arg in scope at position at a plain int?"""
    if not re.fullmatch(r"[A-Za-z_]\w*", arg):
        return False
    for start, end in functions:
        if start <= at < end:
            text = code[start:at]
            decls = [
                m
                for m in re.finditer(DECL % re.escape(arg), text)
                if is_type(m.group(2), types)
            ]
            for m in sorted(decls, key=lambda m: m.end(), reverse=True):
                if in_scope(text, m.start(), len(text)):
                    words = m.group(1).split()
                    plain = set(words[:-1]) <= {"const", "volatile", "register"}
                    return words[-1] == "int" and plain and not m.group(4)
            return False
    return False


def check_ctype(path, code, types):
    """Yield (line, key, message) for each ctype call lacking the cast."""
    functions = []
    for start, head, body in top_level(code):
        if body is not None:
            functions.append((start, start + len(head) + len(body)))
    for m in CTYPE_CALL.finditer(code):
        open_at = m.end() - 1
        arg = code[open_at + 1 : matching(code, open_at) - 1].strip()
        if UCHAR_CAST.match(arg) or arg.startswith("'") or re.fullmatch(r"-?\d+", arg):
            continue
        if int_variable(code, functions, m.start(), arg, types):
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
    return (
        word in TYPE_WORDS or word in types or word in LIBC_TYPES or word.endswith("_t")
    )


def locals_of(body, types):
    """Names the body declares itself, as in do { size_t n = ...; } while (0)."""
    found = set()
    for m in re.finditer(
        r"([A-Za-z_]\w*)[\s*]+(?:const\s+)?([A-Za-z_]\w*)\s*[=;,\[)]", body
    ):
        if is_type(m.group(1), types):
            found.add(m.group(2))
    return found


def split_top(text, sep):
    """Split text at each sep outside any bracket."""
    parts, depth, last = [], 0, 0
    for k, c in enumerate(text):
        depth += c in "([{"
        depth -= c in ")]}"
        if c == sep and depth == 0:
            parts.append(text[last:k])
            last = k + 1
    return parts + [text[last:]]


def generic_uses(text, named, types, declared):
    """Parameter uses of a _Generic( ... ) body: only one association runs."""
    uses = collections.Counter()
    for assoc in split_top(text, ",")[1:]:
        expr = ":".join(split_top(assoc, ":")[1:]) or assoc
        uses |= idents(expr, named, types, declared)[0]
    return uses


def idents(body, named, types, declared):
    """(evaluated parameter uses, free names) of a macro body."""
    unevaluated = list(spans(body, UNEVALUATED))
    attributes = list(spans(body, ("__attribute__", "__declspec")))
    generics = []
    for a, b in spans(body, ("_Generic",)):
        if not any(x <= a < y for x, y in generics):
            generics.append((a, b))
    uses = collections.Counter()
    free = set()
    for a, b in generics:
        uses.update(generic_uses(body[a + 1 : b - 1], named, types, declared))
    for m in IDENT.finditer(body):
        ident, at = m.group(0), m.start()
        if any(a <= at < b for a, b in generics):
            continue
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
    return uses, free


def macro_problems(name, params, body, types):
    """List the rule-6 problems of one macro, each a stable phrase."""
    problems = []
    named = [p for p in params if p != "..."]
    uses, free = idents(body, named, types, locals_of(body, types))
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


def first_branches(text):
    """strip_preprocessor, also blanking the later branches of an #if group whose
    branches leave braces or parentheses open, so brackets match on one branch."""
    raw = text.split("\n")
    lines = strip_preprocessor(text).split("\n")

    def delta(a, b):
        seg = "\n".join(lines[a:b])
        return seg.count("{") - seg.count("}"), seg.count("(") - seg.count(")")

    stack = []
    for k, line in enumerate(raw):
        m = re.match(r"[ \t]*#[ \t]*(if|ifdef|ifndef|elif|else|endif)\b", line)
        if not m:
            continue
        if m.group(1).startswith("if"):
            stack.append([k])
        elif stack and m.group(1) != "endif":
            stack[-1].append(k)
        elif stack:
            marks = stack.pop() + [k]
            branches = list(zip(marks, marks[1:]))
            if any(delta(a, b) != (0, 0) for a, b in branches):
                for a, b in branches[1:]:
                    lines[a:b] = [" " * len(x) for x in lines[a:b]]
    return "\n".join(lines)


def top_level(code):
    """Yield (start, head, body) per top-level declaration: body is None for a ;.
    Brackets match on the first branch of an unbalanced #if, but head and body
    hold every branch."""
    extern = r'\bextern\s*"[^"\n]*"\s*\{'
    flat = re.sub(extern, lambda m: " " * len(m.group(0)), strip_preprocessor(code))
    first = re.sub(extern, lambda m: " " * len(m.group(0)), first_branches(code))
    start = 0
    k = 0
    while k < len(first):
        c = first[k]
        if c == "(":
            k = matching(first, k)
            continue
        if c == ";":
            yield start, flat[start:k], None
            start = k = k + 1
            continue
        if c == "{":
            end = matching(first, k, "{}")
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


DELEGATE = re.compile(r"\breturn\s+([A-Za-z_]\w*)\s*\(")


def check_result(files):
    """Yield (path, line, key, message) for header functions that can fail unmarked."""
    bodies = collections.defaultdict(list)
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
                bodies[name].append(body)
            if path.endswith(".h"):
                decls.append((path, start, head, name, ret, body))

    def literal(cls, found):
        return any(
            FAILURE[cls].search(b) or (cls == "int" and yes_no(b)) for b in found
        )

    # Functions whose failure shows in a literal return, or that are marked.
    failing = set(marked)
    for path, start, head, name, ret, body in decls:
        cls = return_class(ret)
        found = [body] if body is not None else bodies.get(name, [])
        if cls and literal(cls, found):
            failing.add(name)
    for path, start, head, name, ret, body in decls:
        cls = return_class(ret)
        found = [body] if body is not None else bodies.get(name, [])
        if cls is None or name in marked:
            continue
        # One level of `return g(...);` where g can fail.
        calls = (g for b in found for g in DELEGATE.findall(b))
        if name in failing or any(g in failing for g in calls):
            line = line_of(files[path], start + len(head) - len(head.lstrip()))
            msg = "%s returns failure (%s) without HTS_CHECK_RESULT" % (name, cls)
            yield path, line, name, msg


# Functions that read their argument up to a NUL.
NUL_READERS = frozenset(
    """
    strlen strnlen strcmp strncmp strcasecmp strncasecmp strchr strrchr strstr
    strspn strcspn strpbrk strcpy strncpy strcat strncat strdup strtok strtol
    strtoul strtoll strtoull strtod atoi atol atoll atof sscanf fputs puts fopen
    getaddrinfo hts_strcasecmp hts_strncasecmp hts_strdup hts_strtod_c
    hts_stringLengthUTF8 strdupt strcpybuff strcatbuff strlcpybuff strlcatbuff
    strclipbuff strncatbuff strlncatbuff strfield strfield2 streql strstrcase
    strnotempty strjoker str_begins strrchr_limit hts_authority_end hts_span_of
    hts_parse_url_port hts_header_param strjokerfind strtoimax strtoumax
    strendwith_ structcheck strlist_add
    """.split()
)
VIEW_FIELDS = r"(?:scheme|userinfo|host|port|path|query|fragment)"
SPAN_DECL = re.compile(
    r"[;{}(,]\s*(?:(?:const|static|register)\s+)*hts_span\b(?!\s*\()(?=([^;{}()]*))"
)
CAST = re.compile(r"^\(\s*(?:const\s+)?(?:unsigned\s+)?char\s*\*\s*\)\s*")
CALL = re.compile(r"(?<![\w.>])([A-Za-z_]\w*)\s*\(")
CONVERSION = re.compile(
    r"%[-+ #0]*(\*|\d+)?(\.(\*|\d*))?(hh|h|ll|l|L|z|j|t|I64)?([a-zA-Z%])"
)


def span_names(text):
    """Names declared as hts_span in a function head or body."""
    names = set()
    for m in SPAN_DECL.finditer(text):
        # A later parameter starts with its own type, which is never a name.
        for item in split_top(m.group(1), ","):
            name = re.match(r"\s*(?:const\s+)?\**\s*(?:const\s+)?([A-Za-z_]\w*)", item)
            if name:
                names.add(name.group(1))
    return names


def span_ref(arg, spans):
    """Is arg a span's .p, maybe cast or offset, or a URL view field's?"""
    names = "|".join(map(re.escape, sorted(spans))) or "(?!)"
    pat = (
        r"^(?:(?:%s)\s*(?:\.|->)|[\w\]\)\s.>-]*?(?:\.|->)\s*%s\s*\.)"
        r"\s*p\b(?:\s*[-+].*)?$" % (names, VIEW_FIELDS)
    )
    return re.match(pat, CAST.sub("", arg.strip()), re.S) is not None


def format_args(raw, inner_start, args):
    """Indexes of the arguments a %s with no precision prints, or []."""
    offset = 0
    for k, a in enumerate(args):
        lit = raw[inner_start + offset : inner_start + offset + len(a)].strip()
        offset += len(a) + 1
        if not lit.startswith('"'):
            continue
        text = "".join(re.findall(r'"((?:[^"\\]|\\.)*)"', lit))
        found, at = [], k + 1
        for c in CONVERSION.finditer(text):
            if c.group(5) == "%":
                continue
            at += (c.group(1) == "*") + (c.group(3) == "*")
            if c.group(5) == "s" and c.group(2) is None:
                found.append(at)
            at += 1
        return found
    return []


def bounded_strnlen(name, args):
    """Is this strnlen(x.p, x.len), which stops at the span's end?"""
    if name != "strnlen" or len(args) != 2:
        return False
    m = re.match(r"^(.*?)\s*(\.|->)\s*p$", args[0].strip(), re.S)
    bare = re.sub(r"\s+", "", args[1])
    return m is not None and bare == re.sub(r"\s+", "", m.group(1)) + m.group(2) + "len"


def check_span(path, raw, code):
    """Yield (line, key, message) for a span's .p read as a C string."""
    for start, head, body in top_level(code):
        if body is None:
            continue
        text = head + body
        spans = span_names(text)
        field = re.search(r"(?:\.|->)\s*%s\s*\.\s*p\b" % VIEW_FIELDS, text)
        if not spans and not field:
            continue
        for m in CALL.finditer(text):
            open_at = m.end() - 1
            close = matching(text, open_at)
            args = split_top(text[open_at + 1 : close - 1], ",")
            at = start + open_at + 1
            name = m.group(1)
            hits = []
            if name in NUL_READERS and not bounded_strnlen(name, args):
                hits = [(a, "reads a span up to a NUL") for a in args]
            for k in format_args(raw, at, args):
                if k < len(args):
                    hits.append((args[k], "prints a span with %s, use %.*s"))
            for arg, why in hits:
                if span_ref(arg, spans):
                    call = squash("%s(%s)" % (name, arg.strip()))
                    yield line_of(code, start + m.start()), call, "%s %s" % (call, why)


def scan(root):
    """Map each rule to a list of (path, line, key, message)."""
    paths = []
    for top, dirs, names in os.walk(os.path.join(root, "src")):
        rel = os.path.relpath(top, root).replace(os.sep, "/")
        dirs[:] = sorted(d for d in dirs if rel + "/" + d not in VENDORED)
        paths += [os.path.join(top, f) for f in names if f.endswith((".c", ".h"))]
    paths.sort()
    if len(paths) < MIN_SOURCES:
        sys.exit("check-api-lint: only %d sources under %s" % (len(paths), root))
    files, raws = {}, {}
    for p in paths:
        rel = os.path.relpath(p, root).replace(os.sep, "/")
        with open(p, encoding="utf-8", errors="surrogateescape") as f:
            raws[rel] = f.read()
        files[rel] = strip_comments(raws[rel])
    found = {rule: [] for rule in RULES}
    types = collect_types(files.values())
    for path, code in files.items():
        for line, key, msg in check_ctype(path, code, types):
            found["ctype"].append((path, line, key, msg))
    for path, code in files.items():
        for line, name, params, body in macros(code):
            for problem in macro_problems(name, params, body, types):
                key = "%s: %s" % (name, problem)
                found["macro"].append((path, line, key, "macro " + key))
    found["check-result"] = list(check_result(files))
    for path, code in files.items():
        for line, key, msg in check_span(path, raws[path], code):
            found["span"].append((path, line, key, msg))
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
                new[entry] -= 1
                print("%s:%d: %s: new violation: %s" % (path, line, rule, msg))
                status = 1
        for entry in sorted((known - current).elements()):
            print("%s: %s: stale baseline entry, delete it: %s" % (bpath, rule, entry))
            status = 1
    return status


if __name__ == "__main__":
    sys.exit(main())
