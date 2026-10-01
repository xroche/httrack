#!/usr/bin/env python3
"""List the statements whose failure bash 3.2 (macOS /bin/bash) lets set -e miss.

bash 3.2 only exits on a failing simple command, so a `( ... )` statement, or a
pipeline ending in a compound command such as `| while`, fails and execution goes
on. Exempt: a handled status (`|| fail`), a condition, and the last statement of a
function or script, whose status still reaches the caller. Needs shfmt.
"""

import json
import re
import subprocess
import sys

SIMPLE = {"CallExpr", "DeclClause", "TestClause", "ArithmCmd", "LetClause"}
AND, OR, PIPE, PIPEALL = 10, 11, 12, 13
SET_E = re.compile(r"-[a-zA-Z]*e[a-zA-Z]*")
SET_NO_E = re.compile(r"\+[a-zA-Z]*e[a-zA-Z]*")


class Scan:
    def __init__(self, path):
        self.path = path
        self.hits = []

    def hit(self, stmt, kind):
        self.hits.append(f"{self.path}:{stmt['Pos']['Line']}: {kind}")

    def stmts(self, lst, active, tail):
        lst = lst or []
        for i, s in enumerate(lst):
            active = self.stmt(s, active, tail and i == len(lst) - 1)

    def stmt(self, s, active, tail):
        """Walk one statement; return errexit for the next one, after set -e/+e."""
        cmd = s.get("Cmd")
        if cmd is None:
            return active
        on = active and not s.get("Negated")
        bg = bool(s.get("Background"))
        kind = cmd["Type"]
        if kind == "CallExpr":
            args = [
                "".join(p.get("Value", "?") for p in w.get("Parts", []))
                for w in cmd.get("Args") or []
            ]
            if args[:1] == ["set"]:
                for a in args[1:]:
                    if SET_E.fullmatch(a):
                        return True
                    if SET_NO_E.fullmatch(a):
                        return False
        elif kind == "Subshell":
            if on and not tail and not bg:
                self.hit(s, "subshell")
            # A background job's own status still decides what wait reports.
            self.stmts(cmd["Stmts"], on or bg, True)
        elif kind == "Block":
            self.stmts(cmd["Stmts"], on, tail)
        elif kind == "IfClause":
            self.stmts(cmd.get("Cond"), False, False)
            self.stmts(cmd.get("Then"), on, tail)
            if cmd.get("Else"):
                # shfmt leaves the type off an elif/else node.
                self.stmt({"Cmd": dict(cmd["Else"], Type="IfClause")}, on, tail)
        elif kind in ("WhileClause", "ForClause"):
            self.stmts(cmd.get("Cond"), False, False)
            self.stmts(cmd.get("Do"), on, tail)
        elif kind == "CaseClause":
            for item in cmd.get("Items") or []:
                self.stmts(item.get("Stmts"), on, tail)
        elif kind == "TimeClause" and cmd.get("Stmt"):
            self.stmt(cmd["Stmt"], on, tail)
        elif kind == "FuncDecl":
            self.stmt(cmd["Body"], True, True)
        elif kind == "BinaryCmd" and cmd["Op"] in (AND, OR):
            self.stmt(cmd["X"], False, False)
            self.stmt(cmd["Y"], on, tail)
        elif kind == "BinaryCmd" and cmd["Op"] in (PIPE, PIPEALL):
            elems = []

            def flatten(x):
                c = x["Cmd"]
                if c["Type"] == "BinaryCmd" and c["Op"] in (PIPE, PIPEALL):
                    flatten(c["X"])
                    flatten(c["Y"])
                else:
                    elems.append(x)

            flatten(s)
            last = elems[-1]["Cmd"]["Type"]
            if on and not tail and not bg and last not in SIMPLE:
                self.hit(s, f"pipeline ending in a {last}")
            for e in elems:
                self.stmt(e, on, True)
        return active


def scan(path):
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
        return [f"{path}: shfmt could not parse it: {e.stderr.decode().strip()}"]
    sc = Scan(path)
    # A sourced library runs under its caller's set -e.
    on = not path.endswith(".test") or re.search(rb"^\s*set -[a-z]*e", src, re.M)
    sc.stmts(json.loads(out)["Stmts"], bool(on), True)
    return sc.hits


def main():
    hits = [h for path in sys.argv[1:] for h in scan(path)]
    for h in hits:
        print(h)
    print(f"{len(sys.argv) - 1} files, {len(hits)} hits", file=sys.stderr)
    return 1 if hits else 0


if __name__ == "__main__":
    sys.exit(main())
