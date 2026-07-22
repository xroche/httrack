#!/usr/bin/env python3
# Structural WARC/1.1 validator, Python stdlib only (no warcio): walks the
# concatenated gzip members with zlib (gzip.decompress would fuse them and lose
# per-record boundaries) and checks each record against the spec.
import sys
import zlib


def records(data):
    """Yield each record's decompressed bytes (one gzip member per record)."""
    if data[:2] != b"\x1f\x8b":  # uncompressed .warc: split on the record magic
        parts = data.split(b"WARC/1.")
        for p in parts[1:]:
            yield b"WARC/1." + p
        return
    while data:
        d = zlib.decompressobj(zlib.MAX_WBITS | 16)
        block = d.decompress(data) + d.flush()
        yield block
        data = d.unused_data


def field(header, name):
    for line in header.split(b"\r\n"):
        if line.lower().startswith(name.lower() + b":"):
            return line.split(b":", 1)[1].strip()
    return None


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    expect_revisit = "--expect-revisit" in sys.argv[1:]
    path = args[0]
    data = open(path, "rb").read()
    total = revisits = responses = infos = 0
    for rec in records(data):
        total += 1
        if not rec.startswith(b"WARC/1."):
            sys.exit("record %d: bad magic %r" % (total, rec[:16]))
        sep = rec.find(b"\r\n\r\n")
        if sep < 0:
            sys.exit("record %d: no header terminator" % total)
        hdr_end = sep + 4
        header = rec[:sep]
        cl = field(header, b"Content-Length")
        if cl is None:
            sys.exit("record %d: no Content-Length" % total)
        block_len = int(cl)
        if hdr_end + block_len + 4 != len(rec):
            sys.exit(
                "record %d: Content-Length %d != block length %d"
                % (total, block_len, len(rec) - hdr_end - 4)
            )
        if rec[hdr_end + block_len :] != b"\r\n\r\n":
            sys.exit("record %d: missing \\r\\n\\r\\n trailer" % total)
        wtype = field(header, b"WARC-Type")
        if wtype == b"warcinfo":
            infos += 1
        elif wtype == b"response":
            responses += 1
        elif wtype == b"revisit":
            revisits += 1
    if total < 1:
        sys.exit("no records found")
    if infos != 1:
        sys.exit("expected exactly one warcinfo record, got %d" % infos)
    if expect_revisit and revisits < 1:
        sys.exit("expected at least one revisit record, found none")
    print(
        "warc-validate: %d records OK (%d response, %d revisit)"
        % (total, responses, revisits)
    )


if __name__ == "__main__":
    main()
