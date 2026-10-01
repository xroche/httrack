#!/usr/bin/env python3
"""List statements that bash 3.2 (macOS /bin/bash) lets fail under set -e.

bash 3.2 exits only on a failing simple command. It runs on past a failing
`( ... )`, `[[ ]]` or `(( ))` statement, and past a pipeline ending in one.
`( ... ) || fail` is no fix, because set -e is off inside a list's left side.
These are safe: a status read by `subshell_ok $?`, a condition, and the last
statement of a function or script. It needs shfmt.
"""

import json
import re
import subprocess
import sys

# Statement kinds bash 3.2 lets fail, and the label each hit carries.
LOST = {"Subshell": "subshell", "TestClause": "[[ ]]", "ArithmCmd": "(( ))"}
# shfmt's syntax.BinCmdOperator values.
AND, OR, PIPE, PIPEALL = 10, 11, 12, 13
SET_ERREXIT = re.compile(r"([-+])([a-zA-Z]*e[a-zA-Z]*|o errexit)")


def text(part):
    if part.get("Type") == "ParamExp" and part.get("Short"):
        return "$" + part["Param"]["Value"]
    return part.get("Value", "?")


def words(cmd):
    return ["".join(map(text, w.get("Parts", []))) for w in cmd.get("Args") or []]


def is_call(stmt, *argv):
    cmd = (stmt or {}).get("Cmd") or {}
    return cmd.get("Type") == "CallExpr" and words(cmd)[: len(argv)] == list(argv)


def keeps(stmt):
    """Is stmt a bare `var=$?`?"""
    cmd = stmt["Cmd"]
    assigns = cmd.get("Assigns") or []
    return (
        cmd["Type"] == "CallExpr"
        and not cmd.get("Args")
        and len(assigns) == 1
        and [text(p) for p in (assigns[0].get("Value") or {}).get("Parts", [])]
        == ["$?"]
    )


def pipeline_elems(stmt):
    cmd = stmt["Cmd"]
    if cmd["Type"] == "BinaryCmd" and cmd["Op"] in (PIPE, PIPEALL):
        yield from pipeline_elems(cmd["X"])
        yield from pipeline_elems(cmd["Y"])
    else:
        yield stmt


class Scan:
    """Walk a shfmt AST."""

    # errexit means set -e is in force here.
    # checked means a failure here would end the script.
    # reaches_caller means this status is also the parent's, so a caller sees it.

    def __init__(self, path):
        self.path = path
        self.hits = []

    def hit(self, stmt, reason):
        self.hits.append(f"{self.path}:{stmt['Pos']['Line']}: {reason}")

    def stmts(self, lst, errexit, reaches_caller):
        lst = lst or []
        for i, s in enumerate(lst):
            last = i == len(lst) - 1
            read = not last and is_call(lst[i + 1], "subshell_ok", "$?")
            errexit = self.stmt(s, errexit, (reaches_caller and last) or read)

    def stmt(self, s, errexit, reaches_caller):
        """Return the set -e state after s. Only a set command changes it."""
        cmd = s.get("Cmd")
        if cmd is None:
            return errexit
        checked = errexit and not s.get("Negated") and not s.get("Background")
        kind = cmd["Type"]
        if kind in LOST and checked and not reaches_caller:
            self.hit(s, LOST[kind])
        if kind == "CallExpr":
            argv = words(cmd)
            if argv[:1] == ["set"]:
                for flag in (" ".join(argv[1:3]), argv[1] if len(argv) > 1 else ""):
                    m = SET_ERREXIT.fullmatch(flag)
                    if m:
                        return m.group(1) == "-"
        elif kind == "Subshell":
            # A background job's status still decides what wait reports.
            self.stmts(cmd["Stmts"], checked or bool(s.get("Background")), True)
        elif kind == "Block":
            self.stmts(cmd["Stmts"], checked, reaches_caller)
        elif kind == "IfClause":
            self.stmts(cmd.get("Cond"), False, False)
            self.stmts(cmd.get("Then"), checked, reaches_caller)
            if cmd.get("Else"):
                # shfmt leaves the type off an elif/else node.
                else_ = {"Cmd": dict(cmd["Else"], Type="IfClause")}
                self.stmt(else_, checked, reaches_caller)
        elif kind in ("WhileClause", "ForClause"):
            self.stmts(cmd.get("Cond"), False, False)
            # The loop runs on after its body's last statement.
            self.stmts(cmd.get("Do"), checked, False)
        elif kind == "CaseClause":
            for item in cmd.get("Items") or []:
                self.stmts(item.get("Stmts"), checked, reaches_caller)
        elif kind == "TimeClause" and cmd.get("Stmt"):
            self.stmt(cmd["Stmt"], checked, reaches_caller)
        elif kind == "FuncDecl":
            self.stmt(cmd["Body"], True, True)
        elif kind == "BinaryCmd" and cmd["Op"] in (AND, OR):
            left = cmd["X"]
            body = (left.get("Cmd") or {}).get("Stmts") or []
            # `|| rc=$?` and `|| true` keep or drop the status on purpose.
            kept = (
                is_call(cmd["Y"], "true") or is_call(cmd["Y"], ":") or keeps(cmd["Y"])
            )
            if left["Cmd"]["Type"] == "Subshell" and len(body) > 1 and not kept:
                self.hit(left, "set -e is off inside a subshell left of && or ||")
            self.stmt(left, False, False)
            self.stmt(cmd["Y"], checked, reaches_caller)
        elif kind == "BinaryCmd" and cmd["Op"] in (PIPE, PIPEALL):
            elems = list(pipeline_elems(s))
            last = elems[-1]["Cmd"]["Type"]
            if (
                checked
                and not reaches_caller
                and last not in ("CallExpr", "DeclClause")
            ):
                self.hit(s, f"pipeline ending in a {last}")
            for e in elems:
                self.stmt(e, checked, True)
        return errexit


def scan(path):
    """Return (hits, parsed) for one file. A file shfmt cannot parse is a hit."""
    with open(path, "rb") as f:
        src = f.read()
    try:
        out = subprocess.run(
            ["shfmt", "-ln", "bash", "--to-json"],
            input=src,
            capture_output=True,
            check=True,
        ).stdout
    except subprocess.CalledProcessError as e:
        return [f"{path}: shfmt could not parse it: {e.stderr.decode().strip()}"], 0
    sc = Scan(path)
    # A sourced library runs under its caller's set -e.
    errexit = not path.endswith(".test") or re.search(
        rb"^\s*set (-[a-z]*e|-o errexit)", src, re.M
    )
    sc.stmts(json.loads(out)["Stmts"], bool(errexit), True)
    return sc.hits, 1


def main():
    hits, parsed = [], 0
    for path in sys.argv[1:]:
        h, ok = scan(path)
        hits += h
        parsed += ok
    for h in hits:
        print(h)
    print(f"{parsed} files, {len(hits)} hits", file=sys.stderr)
    return 1 if hits else 0


if __name__ == "__main__":
    sys.exit(main())
