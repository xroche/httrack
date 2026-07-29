#!/usr/bin/env python3
"""Capture one PNG per WebHTTrack documentation screen.

Starts htsserver, drives its UI in headless Chromium the way a user would, and
shoots the welcome/project/URL panes, every option tab, and a mirror in progress
and finished. The crawl is real: a small site served by this script.
"""

import argparse
import os
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

# A plausible little site, so the progress screen shows readable names rather
# than the crawler-stress fixtures tests/local-server.py serves.
SITE = {
    "/": ["/about.html", "/products/index.html", "/news/index.html", "/contact.html"],
    "/about.html": ["/about/team.html", "/about/history.html"],
    "/about/team.html": [],
    "/about/history.html": [],
    "/contact.html": [],
    "/products/index.html": ["/products/%d.html" % n for n in range(1, 13)],
    "/news/index.html": ["/news/%d.html" % n for n in range(1, 13)],
}
for _n in range(1, 13):
    SITE["/products/%d.html" % _n] = ["/products/index.html"]
    SITE["/news/%d.html" % _n] = ["/news/index.html"]

PAGE = (
    "<!DOCTYPE html><html><head><title>Example — %s</title></head><body>"
    "<h1>%s</h1><p>%s</p>%s</body></html>"
)


class SiteHandler(BaseHTTPRequestHandler):
    delay = 0.25

    def do_GET(self):
        path = self.path.split("?")[0]
        links = SITE.get(path)
        if links is None:
            self.send_error(404)
            return
        # Pace the crawl so the progress screen carries live counters rather
        # than a set of zeros the engine blew through before the first refresh.
        time.sleep(self.delay)
        body = PAGE % (
            path,
            path,
            "Sample page for the HTTrack documentation screenshots. " * 12,
            "".join('<p><a href="%s">%s</a></p>' % (h, h) for h in links),
        )
        body = body.encode()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args):
        pass


def start_site():
    httpd = ThreadingHTTPServer(("127.0.0.1", 0), SiteHandler)
    threading.Thread(target=httpd.serve_forever, daemon=True).start()
    return httpd, "http://127.0.0.1:%d/" % httpd.socket.getsockname()[1]


def free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port


def start_htsserver(exe, root, home, lang, logpath):
    """Launch htsserver and return (process, base URL) once it announces itself."""
    env = dict(os.environ, HOME=home)
    # Announce through a file, as webhttrack does: htsserver block-buffers a pipe
    # and the URL line would not arrive. No --ppid either -- that arms a pinger
    # that exits the server when its parent goes, and the walk is its own parent.
    log = open(logpath, "w+")
    proc = subprocess.Popen(
        [
            exe,
            root + os.sep,
            "--port",
            str(free_port()),
            "path",
            os.path.join(home, "websites"),
            "lang",
            str(lang),
        ],
        stdout=log,
        stderr=subprocess.STDOUT,
        env=env,
    )
    deadline = time.time() + 30
    while time.time() < deadline:
        m = re.search(r"^URL=(\S+)", open(logpath).read(), re.M)
        if m:
            return proc, m.group(1)
        if proc.poll() is not None:
            break
        time.sleep(0.25)
    proc.kill()
    raise SystemExit("htsserver did not announce a URL:\n" + open(logpath).read())


class Walk:
    def __init__(self, page, outdir):
        self.page, self.outdir = page, outdir
        self.n = 0

    def shot(self, name, page=None):
        page = page or self.page
        path = os.path.join(self.outdir, "%02d_%s.png" % (self.n, name))
        # Shrink to the content: these pages are short, and a fixed viewport
        # would frame each one in a slab of empty background.
        size = page.viewport_size
        # Squash first: scrollHeight never reports less than the viewport, so a
        # short page measured at full height just gives the height back.
        page.set_viewport_size({"width": size["width"], "height": 200})
        height = page.evaluate("Math.ceil(document.documentElement.scrollHeight)")
        page.set_viewport_size({"width": size["width"], "height": max(height, 200)})
        page.screenshot(path=path, full_page=True)
        page.set_viewport_size(size)
        print("  %s" % os.path.basename(path))
        self.n += 1

    def next_pane(self):
        self.page.click("input[name=nextBtn]")
        self.page.wait_for_load_state()

    def popup(self, opener, size):
        """Click `opener` and return the window it opens, sized as the UI asks."""
        with self.page.expect_popup() as info:
            self.page.click(opener)
        popup = info.value
        popup.set_viewport_size({"width": size[0], "height": size[1]})
        popup.wait_for_load_state()
        return popup

    def popup_shot(self, name, opener, size):
        popup = self.popup(opener, size)
        self.shot(name, popup)
        popup.close()


def slug(text):
    return re.sub(r"[^a-z0-9]+", "_", text.strip().lower()).strip("_")


def run(page, url, site_url, outdir, popup_size):
    w = Walk(page, outdir)

    page.goto(url)
    page.wait_for_selector("select[name=lang]")
    w.shot("startup")

    w.next_pane()
    page.fill("input[name=projname]", "example.com")
    page.fill("input[name=projcateg]", "Documentation")
    w.shot("project_name")

    w.next_pane()
    page.fill("textarea[name=urls]", site_url)
    w.shot("project_setup")

    w.popup_shot("add_url", "input[onclick*='doOpenWindow']", popup_size)

    # The tabs are enumerated off the tab bar, so a new option page joins the
    # set on its own.
    opts = w.popup("input[onclick*='option1.html']", popup_size)
    tabs = [
        (a.get_attribute("href"), a.inner_text())
        for a in opts.query_selector_all("td.tabCtrl a")
    ]
    for href, caption in tabs:
        opts.click("td.tabCtrl a[href='%s']" % href)
        opts.wait_for_load_state()
        w.shot("options_" + slug(caption), opts)
    opts.click("input[type=submit][onclick*='closeme']")
    page.wait_for_load_state()

    w.next_pane()
    page.wait_for_selector("input[name=command_do]")
    w.shot("ready_to_start")

    w.next_pane()
    # Both the progress and the finished screen are served as refresh.html, so
    # tell them apart by the form each carries, not by the URL. Wait for live
    # counters as well: the first render arrives with the stats table empty.
    page.wait_for_selector("form[action='step4.html']")
    deadline = time.time() + 120
    while time.time() < deadline:
        # "Links scanned n/m" is the one counter that reads the same in every
        # locale. Hold out for a mid-crawl m: the first refresh lands with the
        # table still at zero and the transfer slots empty.
        m = re.search(r"(\d+)/(\d+)", page.inner_text("body"))
        if m and int(m.group(2)) >= len(SITE) // 2:
            break
        time.sleep(0.5)
    else:
        raise SystemExit("the mirror never reported progress")
    w.shot("mirror_progress")

    page.wait_for_selector("form[action='exit.html']", timeout=300000)
    w.shot("mirror_finished")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--htsserver",
        default=shutil.which("htsserver"),
        help="htsserver binary (default: from PATH)",
    )
    ap.add_argument(
        "--root",
        required=True,
        help="dist root holding lang.def, lang.indexes, lang/ and html/",
    )
    ap.add_argument("--out", default="shots")
    ap.add_argument(
        "--lang", type=int, default=1, help="lang.indexes number (1 = English)"
    )
    ap.add_argument("--home", help="$HOME to run under; default is a throwaway one")
    ap.add_argument("--width", type=int, default=1024)
    ap.add_argument("--height", type=int, default=768)
    ap.add_argument("--scale", type=float, default=2.0, help="device pixel ratio")
    args = ap.parse_args()

    if not args.htsserver:
        raise SystemExit("no htsserver: pass --htsserver")
    for needed in ("lang.def", "lang.indexes", "lang", "html"):
        if not os.path.exists(os.path.join(args.root, needed)):
            raise SystemExit("--root %s has no %s" % (args.root, needed))
    os.makedirs(args.out, exist_ok=True)

    from playwright.sync_api import sync_playwright

    site, site_url = start_site()
    # A pristine HOME by default: an existing mirror would finish from cache in
    # no time and turn the progress shot into a second copy of the finished
    # screen, and the project list would show whatever the host had lying
    # around. It does put a /tmp path on the screens that show the base path.
    home = args.home or tempfile.mkdtemp(prefix="webhttrack-shots-")
    os.makedirs(home, exist_ok=True)
    fd, logpath = tempfile.mkstemp(prefix="htsserver-", suffix=".log")
    os.close(fd)
    server, url = start_htsserver(args.htsserver, args.root, home, args.lang, logpath)
    try:
        with sync_playwright() as p:
            browser = p.chromium.launch()
            context = browser.new_context(
                viewport={"width": args.width, "height": args.height},
                device_scale_factor=args.scale,
            )
            # 640x480 is what the UI itself asks for in its window.open calls.
            run(context.new_page(), url, site_url, args.out, (640, 480))
            browser.close()
    finally:
        server.kill()
        site.shutdown()
        os.unlink(logpath)
        if not args.home:
            shutil.rmtree(home, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
