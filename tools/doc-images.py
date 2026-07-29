#!/usr/bin/env python3
"""Prepare the GUI-guide screenshots from the three capture sets in httrack-works.

Maps each set's capture order onto shared names (the sets number their option
tabs differently), downscales, and writes html/img/guide/<platform>/<name>.
"""

import os
import sys

from PIL import Image

# Shared name -> capture stem, per set. A None means that screen does not exist
# on that platform.
SCREENS = {
    # wizard walkthrough
    "start": ("01_startup", "00_startup", "01_startup"),
    "project-name": ("02_project_name", "01_project_name", "02_project_name"),
    "project-setup": ("03_project_setup", "02_project_setup", "03_project_setup"),
    "ready": ("15_ready_to_start", "15_ready_to_start", None),
    "progress": ("16_mirror_progress", "16_mirror_progress", "16_mirror_progress"),
    "finished": ("17_mirror_finished", "17_mirror_finished", "17_mirror_finished"),
    # platform-specific extras
    "language": ("00_language_preference", None, None),
    "add-url": (None, "03_add_url", None),
    "permission": (None, None, "00a_first_run_permission"),
    "legacy-import": (None, None, "00_legacy_import_dialog"),
    "options-menu": (None, None, "04_options_menu"),
    # the eleven option tabs
    "opt-scan-rules": ("05_options_scan_rules", "11_options_scan_rules", "05_options_scan_rules"),
    "opt-limits": ("06_options_limits", "08_options_limits", "06_options_limits"),
    "opt-flow-control": ("07_options_flow_control", "07_options_flow_control", "07_options_flow_control"),
    "opt-links": ("08_options_links", "04_options_links", "08_options_links"),
    "opt-build": ("09_options_build", "05_options_build", "09_options_build"),
    "opt-browser-id": ("12_options_browser_id", "10_options_browser_id", "10_options_browser_id"),
    "opt-spider": ("10_options_spider", "12_options_spider", "11_options_spider"),
    "opt-proxy": ("04_options_proxy", "14_options_proxy", "12_options_proxy_address"),
    "opt-log-index-cache": ("13_options_log_index_cache", "13_options_log_index_cache", "13_options_log_index_cache"),
    "opt-mime-types": ("11_options_mime_types", "09_options_mime_types", "14_options_type_mime_associations"),
    "opt-experts-only": ("14_options_experts_only", "06_options_experts_only", "15_options_experts_only"),
}

# (output dir, capture set, max width) — the Windows shots are already small
# enough to pass through untouched.
PLATFORMS = (
    ("win", "windows-screenshots", 1040),
    ("web", "webhttrack-screenshots", 1024),
    ("droid", "android-screenshots", 600),
)


def convert(src, dst, max_width, fmt):
    im = Image.open(src)
    if im.width > max_width:
        height = round(im.height * max_width / im.width)
        im = im.resize((max_width, height), Image.LANCZOS)
    if fmt == "webp":
        im.convert("RGB").save(dst, "WEBP", quality=88, method=6)
    else:
        im.convert("RGB").quantize(colors=256, method=Image.MAXCOVERAGE, dither=Image.Dither.NONE).save(dst, "PNG", optimize=True)
    return im.size, os.path.getsize(dst)


def main():
    fmt = sys.argv[1] if len(sys.argv) > 1 else "webp"
    works = os.path.expanduser("~/git/httrack-works")
    out_root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "html", "img")

    total = 0
    for index, (platform, capture_set, max_width) in enumerate(PLATFORMS):
        os.makedirs(out_root, exist_ok=True)
        for name, stems in SCREENS.items():
            stem = stems[index]
            if stem is None:
                continue
            src = os.path.join(works, capture_set, "shots", stem + ".png")
            if not os.path.exists(src):
                sys.exit("missing capture: " + src)
            dst = os.path.join(out_root, "guide-%s-%s.%s" % (platform, name, fmt))
            size, written = convert(src, dst, max_width, fmt)
            total += written
            print("%-6s %-22s %4dx%-5d %6dK" % (platform, name, size[0], size[1], written // 1024))
    print("TOTAL %d K in %s" % (total // 1024, fmt))


if __name__ == "__main__":
    main()
