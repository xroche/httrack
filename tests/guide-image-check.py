#!/usr/bin/env python3
"""Compare guide.html's hardcoded <img> geometry against the files it ships.

Usage: guide-image-check.py <html-dir>
"""

import os
import re
import sys

from PIL import Image

# Floor: a parse that collapses to a handful matched the markup, not the tags.
MIN_IMAGES = 40


def check(html_dir):
    """Return (number of img/ tags, list of defect messages)."""
    with open(os.path.join(html_dir, "guide.html"), encoding="utf-8") as fp:
        text = fp.read()

    refs = []
    bad = []
    for tag in re.findall(r"<img\b[^>]*>", text):
        attrs = dict(re.findall(r'\b([a-z]+)="([^"]*)"', tag))
        src = attrs.get("src", "")
        if not src.startswith("img/"):
            continue
        refs.append(src)
        path = os.path.join(html_dir, src)
        if not os.path.isfile(path):
            bad.append("%s: referenced by guide.html, not shipped" % src)
            continue
        if "width" not in attrs or "height" not in attrs:
            bad.append("%s: <img> carries no width/height" % src)
            continue
        with Image.open(path) as im:
            width, height = im.size
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
