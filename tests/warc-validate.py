#!/usr/bin/env python3
# Structural + semantic WARC/1.1 validator, Python stdlib only (no warcio):
# walks the concatenated gzip members with zlib (gzip.decompress would fuse them
# and lose per-record boundaries) and checks each record against the spec.
#
# Options:
#   --expect-revisit          at least one revisit record must be present
#   --expect-body-hex SUB=HEX a response whose WARC-Target-URI contains SUB must
#                             have an entity body byte-equal to bytes.fromhex(HEX),
#                             no Content-Encoding/Transfer-Encoding header, and a
#                             WARC-Payload-Digest matching sha1(body) when present
#   --no-response-for SUB     the asset containing SUB must be a revisit: no
#                             response may target it, and a revisit must
#   --verbatim                compressed asset: --expect-body-hex instead keeps
#                             Content-Encoding, checks the HTTP Content-Length is
#                             the stored (compressed) length, asserts the stored
#                             body inflates to HEX (the served plaintext), and
#                             requires the payload digest when the file emits any
import base64
import hashlib
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


def opt_values(argv, name):
    out = []
    for i, a in enumerate(argv):
        if a == name and i + 1 < len(argv):
            out.append(argv[i + 1])
    return out


def check_body(rec, http_hdr, body, sub, want):
    if b"Content-Encoding" in http_hdr or b"Transfer-Encoding" in http_hdr:
        sys.exit("record for %s kept a content/transfer-encoding header" % sub)
    if body != want:
        sys.exit(
            "body mismatch for %s: got %d bytes, expected %d"
            % (sub, len(body), len(want))
        )
    pd = field(rec[: rec.find(b"\r\n\r\n")], b"WARC-Payload-Digest")
    if pd is not None and pd.startswith(b"sha1:"):
        want_b32 = base64.b32encode(hashlib.sha1(want).digest()).decode("ascii")
        if pd[5:].decode("ascii") != want_b32:
            sys.exit("WARC-Payload-Digest mismatch for %s" % sub)


def check_body_verbatim(rec, http_hdr, body, sub, want, digests_emitted):
    """Verbatim: the stored body is the coded octets, Content-Encoding is kept,
    the HTTP Content-Length equals the stored (compressed) length, inflating the
    body yields the served plaintext, and the payload digest is over the coded
    body. The differential: inflate(stored) == the body the server compressed."""
    if b"Content-Encoding" not in http_hdr:
        sys.exit("verbatim record for %s dropped Content-Encoding" % sub)
    if b"Transfer-Encoding" in http_hdr:
        sys.exit("verbatim record for %s kept Transfer-Encoding" % sub)
    hcl = field(http_hdr, b"Content-Length")
    if hcl is None or int(hcl) != len(body):
        sys.exit("verbatim record for %s: HTTP Content-Length != stored body" % sub)
    try:
        decoded = zlib.decompress(body, zlib.MAX_WBITS | 16)
    except Exception as exc:
        sys.exit("verbatim record for %s: body did not inflate: %s" % (sub, exc))
    if decoded != want:
        sys.exit(
            "verbatim decoded mismatch for %s: got %d bytes, expected %d"
            % (sub, len(decoded), len(want))
        )
    pd = field(rec[: rec.find(b"\r\n\r\n")], b"WARC-Payload-Digest")
    # Catch a regression that drops the digest on the verbatim path, but only
    # when this file emits digests at all (an OpenSSL build; none otherwise).
    if digests_emitted and pd is None:
        sys.exit("verbatim record for %s: missing WARC-Payload-Digest" % sub)
    if pd is not None and pd.startswith(b"sha1:"):
        want_b32 = base64.b32encode(hashlib.sha1(body).digest()).decode("ascii")
        if pd[5:].decode("ascii") != want_b32:
            sys.exit("WARC-Payload-Digest (compressed) mismatch for %s" % sub)


def main():
    argv = sys.argv[1:]
    expect_revisit = "--expect-revisit" in argv
    verbatim = "--verbatim" in argv
    body_specs = [s.split("=", 1) for s in opt_values(argv, "--expect-body-hex")]
    no_resp = opt_values(argv, "--no-response-for")
    path = [a for a in argv if not a.startswith("--") and "=" not in a][0]
    data = open(path, "rb").read()

    # digests are emitted only on an OpenSSL build; detect it once so --verbatim
    # can require the payload digest exactly when the file carries any.
    digests_emitted = any(
        b"WARC-Payload-Digest" in r[: r.find(b"\r\n\r\n")] for r in records(data)
    )

    total = revisits = responses = infos = 0
    body_hits = {sub: False for sub, _ in body_specs}
    revisit_hits = {sub: False for sub in no_resp}
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
        uri = field(header, b"WARC-Target-URI") or b""
        if wtype == b"warcinfo":
            infos += 1
        elif wtype == b"response":
            responses += 1
            for sub in no_resp:
                if sub.encode() in uri:
                    sys.exit("unexpected full response for %s (want revisit)" % sub)
            block = rec[hdr_end : hdr_end + block_len]
            bsep = block.find(b"\r\n\r\n")
            http_hdr, body = block[:bsep], block[bsep + 4 :]
            for sub, hexval in body_specs:
                if sub.encode() in uri:
                    want = bytes.fromhex(hexval)
                    if verbatim:
                        check_body_verbatim(
                            rec, http_hdr, body, sub, want, digests_emitted
                        )
                    else:
                        check_body(rec, http_hdr, body, sub, want)
                    body_hits[sub] = True
        elif wtype == b"revisit":
            revisits += 1
            for sub in no_resp:
                if sub.encode() in uri:
                    revisit_hits[sub] = True

    if total < 1:
        sys.exit("no records found")
    if infos != 1:
        sys.exit("expected exactly one warcinfo record, got %d" % infos)
    if expect_revisit and revisits < 1:
        sys.exit("expected at least one revisit record, found none")
    for sub, hit in body_hits.items():
        if not hit:
            sys.exit("no response record found for --expect-body-hex %s" % sub)
    for sub, hit in revisit_hits.items():
        if not hit:
            sys.exit("no revisit record found for unchanged asset %s" % sub)
    print(
        "warc-validate: %d records OK (%d response, %d revisit)"
        % (total, responses, revisits)
    )


if __name__ == "__main__":
    main()
