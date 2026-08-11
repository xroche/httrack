#!/usr/bin/env python3
"""Compare guide.html's hardcoded <img> geometry against the files it ships.

Usage: guide-image-check.py <html-dir>

Stdlib only: the suite cannot assume Pillow on a build host, and no CI leg
has it.
"""

import os
import re
import struct
import sys

# Floor: a parse that matched nothing would satisfy every check below.
MIN_IMAGES = 40

SIGNATURE = b"\x89PNG\r\n\x1a\n"
# Signature, then the first chunk's length, its IHDR type, and the two sizes.
HEADER = len(SIGNATURE) + 4 + 4 + 4 + 4

COMMENT = re.compile(r"<!--.*?-->", re.S)
IMG_TAG = re.compile(r"<img\b[^>]*>", re.I)
# The leading space keeps data-height="..." one key rather than a height.
ATTR = re.compile(r'\s([A-Za-z-]+)="([^"]*)"')


def png_size(path):
    """Width and height out of the IHDR. Raises ValueError on anything else."""
    with open(path, "rb") as fp:
        head = fp.read(HEADER)
    if head[: len(SIGNATURE)] != SIGNATURE:
        raise ValueError("not a PNG, so its size cannot be read")
    if len(head) < HEADER:
        raise ValueError("truncated inside the IHDR")
    if head[12:16] != b"IHDR":
        raise ValueError("no IHDR where a PNG keeps it")
    return struct.unpack(">II", head[16:HEADER])


def attributes(tag):
    """First value wins, as a browser does with a repeated attribute."""
    attrs = {}
    for name, value in ATTR.findall(tag):
        attrs.setdefault(name.lower(), value)
    return attrs


def check(html_dir):
    """Return (number of img/ tags, list of defect messages)."""
    with open(os.path.join(html_dir, "guide.html"), encoding="utf-8") as fp:
        # A commented-out screenshot is not a reference.
        text = COMMENT.sub("", fp.read())

    refs = []
    bad = []
    for tag in IMG_TAG.findall(text):
        attrs = attributes(tag)
        src = attrs.get("src")
        if src is None:
            bad.append("%s: no src this checker can read" % tag)
            continue
        if not src.startswith("img/"):
            # Every other src is chrome, shared with the rest of the doc set.
            if "img/" in src:
                bad.append("%s: an img/ path spelled past the check" % src)
            continue
        refs.append(src)
        path = os.path.join(html_dir, src)
        if not os.path.isfile(path):
            bad.append("%s: referenced by guide.html, not shipped" % src)
            continue
        if "width" not in attrs or "height" not in attrs:
            bad.append("%s: <img> carries no width/height" % src)
            continue
        try:
            width, height = png_size(path)
        except (OSError, ValueError) as exc:
            bad.append("%s: %s" % (src, exc))
            continue
        if (attrs["width"], attrs["height"]) != (str(width), str(height)):
            bad.append(
                "%s: guide.html says %sx%s, the file is %dx%d"
                % (src, attrs["width"], attrs["height"], width, height)
            )

    if len(refs) < MIN_IMAGES:
        bad.append(
            "only %d <img> tags reference img/, want at least %d"
            % (len(refs), MIN_IMAGES)
        )

    seen = set(refs)
    for name in sorted(os.listdir(os.path.join(html_dir, "img"))):
        # doc-images.py owns this prefix; the rest of img/ serves other pages.
        if name.startswith("guide-") and "img/" + name not in seen:
            bad.append("img/%s: shipped, but guide.html no longer shows it" % name)

    return len(refs), bad


def main():
    if len(sys.argv) != 2:
        sys.stderr.write("usage: %s <html-dir>\n" % sys.argv[0])
        return 2
    count, bad = check(sys.argv[1])
    for message in bad:
        sys.stderr.write("FAIL: %s\n" % message)
    if bad:
        return 1
    print("guide.html: %d image sizes match their files" % count)
    return 0


if __name__ == "__main__":
    sys.exit(main())
