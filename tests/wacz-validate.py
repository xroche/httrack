#!/usr/bin/env python3
# Validate a WACZ package with the stdlib only (no py-wacz needed): every entry
# is ZIP STORE, the fixed layout is present, each datapackage resource hash and
# size recomputes, the digest chains datapackage.json, and pages.jsonl carries
# the json-pages-1.0 header. If py-wacz (`wacz validate`) is importable it runs
# too; both gates must pass. Exit 0 = valid, nonzero = invalid.
import sys
import json
import zipfile
import hashlib


def fail(msg):
    print("wacz-validate: FAIL: %s" % msg, file=sys.stderr)
    sys.exit(1)


def main():
    if len(sys.argv) < 2:
        fail("usage: wacz-validate.py FILE.wacz")
    path = sys.argv[1]
    z = zipfile.ZipFile(path)
    names = z.namelist()

    for info in z.infolist():
        if info.compress_type != zipfile.ZIP_STORED:
            fail("%s is not STORE mode (%d)" % (info.filename, info.compress_type))

    need_arc = any(
        n.startswith("archive/") and n.endswith((".warc.gz", ".warc")) for n in names
    )
    for req, ok in (
        ("archive/*.warc.gz", need_arc),
        ("indexes/index.cdx", "indexes/index.cdx" in names),
        ("pages/pages.jsonl", "pages/pages.jsonl" in names),
        ("datapackage.json", "datapackage.json" in names),
        ("datapackage-digest.json", "datapackage-digest.json" in names),
    ):
        if not ok:
            fail("missing %s (entries: %s)" % (req, names))

    lines = [ln for ln in z.read("pages/pages.jsonl").split(b"\n") if ln.strip()]
    if not lines or json.loads(lines[0]).get("format") != "json-pages-1.0":
        fail("pages.jsonl header is not json-pages-1.0: %r" % lines[:1])
    body = [json.loads(ln) for ln in lines[1:]]
    if not body:
        fail("pages.jsonl has no page rows after the header")
    for row in body:
        if "url" not in row or "ts" not in row:
            fail("pages.jsonl row missing url/ts: %r" % row)

    dp = json.loads(z.read("datapackage.json"))
    if dp.get("profile") != "data-package":
        fail("profile != data-package: %r" % dp.get("profile"))
    if dp.get("wacz_version") != "1.1.1":
        fail("wacz_version != 1.1.1: %r" % dp.get("wacz_version"))
    resources = dp.get("resources", [])
    if not resources:
        fail("datapackage has no resources")
    for r in resources:
        data = z.read(r["path"])
        h = "sha256:" + hashlib.sha256(data).hexdigest()
        if h != r["hash"]:
            fail("%s hash %s != %s" % (r["path"], h, r["hash"]))
        if len(data) != r["bytes"]:
            fail("%s bytes %d != %d" % (r["path"], len(data), r["bytes"]))

    dig = json.loads(z.read("datapackage-digest.json"))
    want = "sha256:" + hashlib.sha256(z.read("datapackage.json")).hexdigest()
    if dig.get("path") != "datapackage.json" or dig.get("hash") != want:
        fail("digest chain broken: %r" % dig)

    print(
        "wacz-validate: OK (%d entries, %d resources, stdlib)"
        % (len(names), len(resources))
    )

    # Optional stricter gate when py-wacz is present.
    try:
        import wacz  # noqa: F401
    except Exception:
        return
    try:
        from wacz.main import main as wacz_main  # type: ignore
    except Exception:
        return
    print("wacz-validate: running py-wacz validate")
    rc = wacz_main(["validate", "-f", path])
    if rc not in (0, None):
        fail("py-wacz validate returned %r" % rc)
    print("wacz-validate: py-wacz OK")


if __name__ == "__main__":
    main()
