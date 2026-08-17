#!/usr/bin/env python3
"""Generate the winprofile.ini key table header from winprofile-keys.tsv.

The TSV is the source of truth; every front end reads it or a product of it.
Parsing the header back would put us where three hand-kept copies already put
us, so generation only ever runs this way round.
"""
import argparse
import re
import sys

COLUMNS = [
    "key",
    "owners",
    "scope",
    "kind",
    "flag_on",
    "flag_off",
    "composed_with",
    "default_state",
    "default_value",
    "empty_means",
    "legacy_of",
]
KIND = re.compile(r"^(string|number|checkbox|bitmask|list:\d+:\d+)$")
SCOPES = {"setting", "project", "write_only", "read_only"}
STATES = {"agreed", "none", "derived", "unresolved", ""}
EMPTY = {"absent", "literal", "unresolved", ""}


def parse(path):
    """Rows of the TSV, or exit naming the first row that is malformed."""
    rows, seen = [], set()
    # Bytes, not text: a stray non-ASCII byte must name its line rather than
    # arrive as a decode traceback.
    with open(path, "rb") as fp:
        for n, raw in enumerate(fp, 1):
            bad = [b for b in raw.rstrip(b"\n") if not 0x20 <= b <= 0x7E and b != 0x09]
            if bad:
                sys.exit(
                    "%s:%d: byte 0x%02x is outside printable ASCII" % (path, n, bad[0])
                )
            line = raw.decode("ascii").rstrip("\n")
            if not line or line.startswith("#"):
                continue
            # Never split(-1)-less: a trailing empty cell must stay a cell.
            f = line.split("\t")
            if len(f) != len(COLUMNS):
                sys.exit("%s:%d: %d columns, want %d" % (path, n, len(f), len(COLUMNS)))
            row = dict(zip(COLUMNS, f))
            if not row["key"]:
                sys.exit("%s:%d: empty key" % (path, n))
            stripped = [c for c in COLUMNS if row[c] != row[c].strip()]
            if stripped:
                sys.exit("%s:%d: %s is padded with whitespace" % (path, n, stripped[0]))
            if not KIND.match(row["kind"]):
                sys.exit("%s:%d: bad kind %r" % (path, n, row["kind"]))
            if row["key"] in seen:
                sys.exit("%s:%d: duplicate key %s" % (path, n, row["key"]))
            if row["scope"] not in SCOPES:
                sys.exit("%s:%d: bad scope %r" % (path, n, row["scope"]))
            if row["default_state"] not in STATES:
                sys.exit(
                    "%s:%d: bad default_state %r" % (path, n, row["default_state"])
                )
            if row["empty_means"] not in EMPTY:
                sys.exit("%s:%d: bad empty_means %r" % (path, n, row["empty_means"]))
            seen.add(row["key"])
            rows.append(row)
    if not rows:
        sys.exit("%s: no rows" % path)
    # A pointer at a key that is not here is a row nobody can follow.
    for r in rows:
        for col in ("composed_with", "legacy_of"):
            for ref in filter(None, r[col].split(",")):
                if ref not in seen:
                    sys.exit(
                        "%s: %s.%s names %s, which has no row"
                        % (path, r["key"], col, ref)
                    )
    return rows


def cstr(s):
    return '"%s"' % s.replace("\\", "\\\\").replace('"', '\\"')


def emit(rows, tsv, out):
    w = out.write
    w("/* Generated from %s by tools/gen-winprofile-keys.py.\n" % tsv)
    w(" * Do not edit. */\n")
    w("#ifndef WINPROFILE_KEYS_H\n#define WINPROFILE_KEYS_H\n\n")
    w("typedef struct {\n")
    for c in COLUMNS:
        w("  const char *%s;\n" % c)
    w("} winprofile_key_t;\n\n")
    # A count the consumer can assert, taken from the file rather than kept by
    # hand: a table that shrinks silently is the failure this whole file exists
    # to prevent.
    w("#define WINPROFILE_KEY_COUNT %d\n\n" % len(rows))
    # One row per line beats whatever a formatter would do with 11 fields, and
    # a reflowed table is unreadable against the TSV it mirrors.
    w("/* clang-format off */\n")
    w("static const winprofile_key_t winprofile_keys[WINPROFILE_KEY_COUNT] = {\n")
    for r in rows:
        w("  {%s},\n" % ", ".join(cstr(r[c]) for c in COLUMNS))
    w("};\n/* clang-format on */\n\n#endif\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("tsv")
    ap.add_argument("header")
    a = ap.parse_args()
    rows = parse(a.tsv)
    with open(a.header, "w", encoding="ascii") as fp:
        emit(rows, a.tsv.split("/")[-1], fp)
    print("%s: %d keys" % (a.header, len(rows)))


if __name__ == "__main__":
    main()
