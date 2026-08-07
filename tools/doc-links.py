#!/usr/bin/env python3
"""Check that every relative link and #fragment inside html/ resolves.

Catches the two ways this doc set breaks: a page is renamed or retired and the
links to it are missed, and a heading id moves out from under a deep link.

    tools/doc-links.py [html-dir]
"""

import os
import re
import sys
from urllib.parse import unquote

# A regex sweep with a typo in it would find nothing and report success, so the
# run also has to see at least this much of the tree.
MIN_PAGES = 20
MIN_LINKS = 300

HREF = re.compile(rb'(?:href|src)\s*=\s*"([^"]+)"', re.I)
ANCHOR = re.compile(rb'(?:\bid|\bname)\s*=\s*"([^"]+)"', re.I)
REFRESH = re.compile(rb'content\s*=\s*"[^"]*url=([^"]+)"', re.I)
EXTERNAL = re.compile(r"^(https?:|mailto:|ftp:|javascript:|data:|//)", re.I)


def anchors(path):
    with open(path, "rb") as fp:
        return {a.decode("utf-8", "replace") for a in ANCHOR.findall(fp.read())}


def main(root):
    pages = sorted(f for f in os.listdir(root) if f.endswith(".html"))
    known = {}
    links = bad = 0

    for page in pages:
        with open(os.path.join(root, page), "rb") as fp:
            body = fp.read()
        for raw in HREF.findall(body) + REFRESH.findall(body):
            url = unquote(raw.decode("utf-8", "replace")).strip()
            # Site-absolute paths mean nothing to a page read from disk, and the
            # FAQ quotes a few as examples.
            if url.startswith("/") or EXTERNAL.match(url):
                continue
            if url.startswith("#"):
                target, frag = page, url[1:]
            else:
                target, _, frag = url.partition("#")
            if not target:
                continue
            links += 1
            path = os.path.normpath(os.path.join(root, target))
            if not os.path.exists(path):
                print(f"{page}: {target} does not exist")
                bad += 1
                continue
            if frag and target.endswith(".html"):
                # The guide's deep links carry a platform: "#win/opt-limits".
                frag = frag.split("/")[-1]
                if path not in known:
                    known[path] = anchors(path)
                if frag and frag not in known[path]:
                    print(f"{page}: {target} has no anchor '{frag}'")
                    bad += 1

    print(f"{len(pages)} pages, {links} internal links, {bad} broken")
    if len(pages) < MIN_PAGES or links < MIN_LINKS:
        print(
            f"::error::only {len(pages)} pages and {links} links seen:"
            " the sweep is not looking at the documentation"
        )
        return 1
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "html"))
