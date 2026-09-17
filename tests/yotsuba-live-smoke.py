#!/usr/bin/env python3
"""Opt-in metadata-only smoke test for the official 4chan API."""

import json
import os
import re
import sys
import time
import urllib.error
import urllib.request


API_ORIGIN = "https://a.4cdn.org"
USER_AGENT = "HTTrackClone-Catalog/3 (+https://www.httrack.com/)"
MINIMUM_INTERVAL_SECONDS = 1.1
MAXIMUM_BODY_BYTES = 16 * 1024 * 1024


def fetch_json(path: str) -> object:
    if not path.startswith("/") or "//" in path:
        raise ValueError("invalid API path")
    request = urllib.request.Request(
        API_ORIGIN + path,
        headers={"User-Agent": USER_AGENT, "Accept": "application/json"},
    )
    with urllib.request.urlopen(request, timeout=30) as response:
        body = response.read(MAXIMUM_BODY_BYTES + 1)
        if len(body) > MAXIMUM_BODY_BYTES:
            raise RuntimeError("response exceeds metadata body limit")
        return json.loads(body)


def main() -> int:
    if os.environ.get("HTTRACK_YOTSUBA_LIVE") != "1":
        print("Yotsuba live smoke test disabled; set HTTRACK_YOTSUBA_LIVE=1")
        return 0
    board = os.environ.get("HTTRACK_YOTSUBA_LIVE_BOARD", "")
    if re.fullmatch(r"[a-z0-9]+", board) is None:
        print("HTTRACK_YOTSUBA_LIVE_BOARD must explicitly name a board", file=sys.stderr)
        return 2

    boards = fetch_json("/boards.json")
    available = {
        item.get("board") for item in boards.get("boards", [])
        if isinstance(item, dict)
    }
    if board not in available:
        raise RuntimeError("configured board is not present in boards.json")

    time.sleep(MINIMUM_INTERVAL_SECONDS)
    pages = fetch_json(f"/{board}/threads.json")
    thread_ids = [
        thread.get("no")
        for page in pages if isinstance(page, dict)
        for thread in page.get("threads", []) if isinstance(thread, dict)
    ]
    if not thread_ids:
        print("Configured board currently has no active threads")
        return 0

    time.sleep(MINIMUM_INTERVAL_SECONDS)
    thread = fetch_json(f"/{board}/thread/{thread_ids[0]}.json")
    posts = thread.get("posts", []) if isinstance(thread, dict) else []
    if not posts or posts[0].get("no") != thread_ids[0]:
        raise RuntimeError("full thread response did not contain its OP")
    print(f"Metadata-only smoke test passed for /{board}/ ({len(posts)} posts)")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, urllib.error.URLError, ValueError, json.JSONDecodeError,
            RuntimeError) as error:
        print(f"Yotsuba live smoke test failed: {error}", file=sys.stderr)
        raise SystemExit(1)
