#!/usr/bin/env python3
"""Generate lang.def from lang-symbols.tsv, the table front ends read.

lang.def is kept, byte for byte, for its existing readers.
"""
import argparse
import re
import sys

COLUMNS = ["kind", "symbol", "text"]
# Blocks in file order.
KINDS = ["language", "meta", "string"]
SYMBOL = {
    "language": re.compile(r"^LANGUAGE_[0-9]+$"),
    "meta": re.compile(r"^LANGUAGE_[A-Z]+$"),
    "string": re.compile(r"^(LANG_|LISTDEF_)[A-Za-z0-9_]+$"),
}
SYMBOL_LIKE = re.compile(r"^(LANG_|LISTDEF_|LANGUAGE_)")
# htsserver.c skips a longer line.
MAXTEXT = 7999


def parse(path):
    """Return the TSV's rows, or exit naming the first malformed one."""
    rows, seen, kind_at = [], set(), 0
    # Read bytes so a non-ASCII byte names its line, not a traceback.
    with open(path, "rb") as fp:
        for n, raw in enumerate(fp, 1):
            if not raw.endswith(b"\n"):
                sys.exit("%s:%d: no final newline" % (path, n))
            bad = [b for b in raw[:-1] if not 0x20 <= b <= 0x7E and b != 0x09]
            if bad:
                sys.exit(
                    "%s:%d: byte 0x%02x is outside printable ASCII" % (path, n, bad[0])
                )
            line = raw[:-1].decode("ascii")
            if not line or line.startswith("#"):
                continue
            # A real tab inside a text adds a column.
            f = line.split("\t")
            if len(f) != len(COLUMNS):
                sys.exit("%s:%d: %d columns, want %d" % (path, n, len(f), len(COLUMNS)))
            row = dict(zip(COLUMNS, f))
            if row["kind"] not in KINDS:
                sys.exit("%s:%d: bad kind %r" % (path, n, row["kind"]))
            if KINDS.index(row["kind"]) < kind_at:
                sys.exit(
                    "%s:%d: a %s row after the %s block"
                    % (path, n, row["kind"], KINDS[kind_at])
                )
            kind_at = KINDS.index(row["kind"])
            if not SYMBOL[row["kind"]].match(row["symbol"]):
                sys.exit("%s:%d: bad symbol %r" % (path, n, row["symbol"]))
            if row["symbol"] in seen:
                sys.exit("%s:%d: duplicate symbol %s" % (path, n, row["symbol"]))
            seen.add(row["symbol"])
            if not row["text"] or row["text"] != row["text"].strip():
                sys.exit(
                    "%s:%d: %s has an empty or padded text" % (path, n, row["symbol"])
                )
            # The server reads a LANG_ or LISTDEF_ key as a symbol, and finds
            # languages by looking up LANGUAGE_1, LANGUAGE_2 and so on.
            if row["kind"] != "meta" and SYMBOL_LIKE.match(row["text"]):
                sys.exit("%s:%d: %s has a symbol for a text" % (path, n, row["symbol"]))
            # A trailing backslash joins the next line in lang.def's readers.
            if row["text"].endswith("\\") or max(map(len, f)) > MAXTEXT:
                sys.exit(
                    "%s:%d: %s has a text lang.def cannot hold"
                    % (path, n, row["symbol"])
                )
            if row["kind"] == "language" and row["symbol"] != "LANGUAGE_%d" % (
                len(rows) + 1
            ):
                sys.exit(
                    "%s:%d: languages must be numbered from 1 in order" % (path, n)
                )
            # A language's text names its catalog file, which the server opens.
            if row["kind"] == "language" and re.search(r"[/\\]|\.\.", row["text"]):
                sys.exit("%s:%d: a language text must be a plain file name" % (path, n))
            if row["kind"] == "meta" and row["text"] != row["symbol"]:
                sys.exit("%s:%d: a meta row's text must repeat its symbol" % (path, n))
            rows.append(row)
    for k in KINDS:
        if not any(r["kind"] == k for r in rows):
            sys.exit("%s: no %s rows" % (path, k))
    check_keys(path, rows)
    return rows


def check_keys(path, rows):
    """Exit if a text lands on another row's key.

    The server keys repeats of a text as text1, text2 and so on, taking the
    first free key, so the TSV order holds only while no key collides.
    """
    keys, count = set(), {}
    for r in rows:
        if r["kind"] != "string":
            continue
        n = count.get(r["text"], 0)
        count[r["text"]] = n + 1
        want = r["text"] if n == 0 else "%s%d" % (r["text"], n)
        if want in keys:
            sys.exit(
                "%s: %s's text %r collides with another row"
                % (path, r["symbol"], r["text"])
            )
        keys.add(want)


def emit(rows, out):
    """Write each row as two lines, text first for a language, else symbol first."""
    for r in rows:
        pair = (
            (r["text"], r["symbol"])
            if r["kind"] == "language"
            else (r["symbol"], r["text"])
        )
        out.write("%s\n%s\n" % pair)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("tsv")
    ap.add_argument("langdef")
    a = ap.parse_args()
    rows = parse(a.tsv)
    with open(a.langdef, "w", encoding="ascii", newline="\n") as fp:
        emit(rows, fp)
    print("%s: %d rows" % (a.langdef, len(rows)))


if __name__ == "__main__":
    main()
