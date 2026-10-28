#!/usr/bin/env python3
"""Generate lang.def from lang-symbols.tsv.

The TSV is the source of truth, and front ends read its columns rather than
counting lang.def's lines. lang.def is kept, byte for byte, for its existing
readers. Generation only ever runs this way round.
"""
import argparse
import re
import sys

COLUMNS = ["kind", "symbol", "text"]
# Blocks in file order. A language row is written text first, the others
# symbol first.
KINDS = ["language", "meta", "string"]
SYMBOL = re.compile(r"^[A-Z][A-Za-z0-9_]*$")


def parse(path):
    """Rows of the TSV, or exit naming the first row that is malformed."""
    rows, seen, block = [], set(), 0
    # Bytes, not text: a stray non-ASCII byte must name its line rather than
    # arrive as a decode traceback.
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
            # A real tab inside a text adds a column, so it lands here: write
            # it as the two characters \t.
            f = line.split("\t")
            if len(f) != len(COLUMNS):
                sys.exit("%s:%d: %d columns, want %d" % (path, n, len(f), len(COLUMNS)))
            row = dict(zip(COLUMNS, f))
            if row["kind"] not in KINDS:
                sys.exit("%s:%d: bad kind %r" % (path, n, row["kind"]))
            if KINDS.index(row["kind"]) < block:
                sys.exit(
                    "%s:%d: a %s row after the %s block"
                    % (path, n, row["kind"], KINDS[block])
                )
            block = KINDS.index(row["kind"])
            if not SYMBOL.match(row["symbol"]):
                sys.exit("%s:%d: bad symbol %r" % (path, n, row["symbol"]))
            if row["symbol"] in seen:
                sys.exit("%s:%d: duplicate symbol %s" % (path, n, row["symbol"]))
            seen.add(row["symbol"])
            if not row["text"] or row["text"] != row["text"].strip():
                sys.exit(
                    "%s:%d: %s has an empty or padded text" % (path, n, row["symbol"])
                )
            if row["kind"] == "meta" and row["text"] != row["symbol"]:
                sys.exit("%s:%d: a meta row's text must repeat its symbol" % (path, n))
            rows.append(row)
    for k in KINDS:
        if not any(r["kind"] == k for r in rows):
            sys.exit("%s: no %s rows" % (path, k))
    return rows


def emit(rows, out):
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
