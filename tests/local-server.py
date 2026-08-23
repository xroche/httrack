#!/usr/bin/env python3
"""Self-contained local web server for httrack's crawl tests.

Serves static fixtures from a docroot plus a handful of dynamic endpoints
(cookies, ...) so httrack can be exercised over loopback, deterministically and
offline, instead of crawling the live ut.httrack.com.

Binds to an ephemeral port (port 0) and prints the chosen port to stdout as
"PORT <n>\n" so a launcher can discover it. Pass --tls to wrap the socket with
the shipped self-signed test cert; httrack does not verify certs, so no CA
trust plumbing is needed.

stdlib only (http.server + ssl) -- no new build or runtime dependency.
"""

import argparse
import base64
import gzip
import hashlib
import os
import re
import socket
import socketserver
import struct
import sys
import time
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import quote, unquote, urlsplit

# Cookie chain replicated from the old ut/cookies/*.php fixtures.
COOKIE_PATH = "/cookies/"
COOKIES = {
    "cat": "dog",
    "cake": "is a lie!",
    "badger": "mushroom, with 'ants'",
}

PAGE = """<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"
\t"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" lang="en">
<head>
\t<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
\t<title>Sample test</title>
</head>
<body>
{body}
</body>
</html>
"""


# --- /big/ seeded pseudo-site (36_local-bigcrawl) ---------------------------
# Deterministic ~360-file tree; bodies derive from sha256(BIG_SEED, name) so
# every run serves identical content and the test pins exact counts.
BIG_SEED = "bigcrawl-lite-1"
BIG_PAGES = 96
BIG_FANOUT = 4
# Fixed validator: a matching If-Modified-Since gets 304, so the update pass
# revalidates instead of re-downloading.
BIG_LASTMOD = "Mon, 01 Jan 2024 00:00:00 GMT"

BIG_CTYPES = {
    "html": "text/html",
    "css": "text/css",
    "js": "application/x-javascript",
    "png": "image/png",
    "gif": "image/gif",
    "jpg": "image/jpeg",
    "webp": "image/webp",
    "pdf": "application/pdf",
    "woff2": "font/woff2",
    "mp4": "video/mp4",
    "webm": "video/webm",
    "mp3": "audio/mpeg",
    "vtt": "text/vtt",
    "xml": "text/xml",
    "svg": "image/svg+xml",
    "jar": "application/java-archive",
    "bin": "application/octet-stream",
}

# Honest magic bytes per claimed type so the #478 sniff never contests.
BIG_MAGIC = {
    "png": b"\x89PNG\r\n\x1a\n",
    "gif": b"GIF89a",
    "jpg": b"\xff\xd8\xff\xe0",
    "webp": b"RIFF\x10\x27\x00\x00WEBPVP8 ",
    "pdf": b"%PDF-1.4\n",
    "woff2": b"wOF2",
    "mp4": b"\x00\x00\x00\x18ftypmp42",
    "webm": b"\x1a\x45\xdf\xa3",
    "mp3": b"ID3\x04\x00\x00\x00\x00\x00\x00",
    "jar": b"PK\x03\x04",
}


def big_blob(name, size):
    out = b""
    n = 0
    while len(out) < size:
        out += hashlib.sha256(f"{BIG_SEED}/{name}/{n}".encode()).digest()
        n += 1
    return out[:size]


def big_asset(name):
    ext = name.rsplit(".", 1)[-1]
    size = 200 + int(hashlib.sha256(name.encode()).hexdigest(), 16) % 3800
    raw = big_blob(name, size)
    if ext in ("css", "js", "txt"):
        return b"/* " + raw.hex().encode() + b" */"
    return BIG_MAGIC.get(ext, b"") + raw


def big_html(title, inner):
    page = (
        "<!DOCTYPE html><html><head><title>%s</title></head><body>\n%s\n</body></html>"
        % (
            title,
            inner,
        )
    )
    return page.encode()


def _hexfill(name):
    return big_blob(name, 160).hex()


HOME = '<a href="/big/index.html">home</a>'

BIG_TEXT_ASSETS = {
    "site.css": (
        "body { background: url(bg.png); } /* %s */" % _hexfill("site.css"),
        "text/css",
    ),
    "print.css": ("p { margin: 0; } /* %s */" % _hexfill("print.css"), "text/css"),
    "blk.css": (
        '@import "blk2.css";\n'
        '@font-face { font-family: big; src: local("Nope Sans"), '
        'url(font.woff2) format("woff2"); }\n'
        "/* %s */" % _hexfill("blk.css"),
        "text/css",
    ),
    # Absolute url() must come back relative after the rewrite (test greps it);
    # the \/ escapes collapse to an already-linked URL if taken literally.
    "blk2.css": (
        "body { background: url(/big/a/blk2-bg.png); }\n"
        "i { background: url(/big\\/a\\/bg.png); }\n"
        "/* %s */" % _hexfill("blk2.css"),
        "text/css",
    ),
    # .open() grabs its first arg only (a method there is rejected, #218), so
    # the window.open single-URL form is the token-detected shape.
    "app.js": (
        'var im = new Image(); im.src = "/big/a/js-img.png";\n'
        'function pop() { window.open("/big/a/js-data.bin"); }\n'
        "// %s\n" % _hexfill("app.js"),
        "application/x-javascript",
    ),
    "heavy.js": (
        'var h = new Image(); h.src = "/big/a/js1.png";\n'
        'function nav() { location.href = "/big/p/1.html"; }\n'
        'function pop() { window.open("/big/a/js2.bin"); }\n'
        "// %s\n" % _hexfill("heavy.js"),
        "application/x-javascript",
    ),
    # text/javascript is scanned like the legacy spelling, so this one lands.
    "modern.js": (
        'var d = new Image(); d.src = "/big/a/js3.png";\n',
        "text/javascript",
    ),
    "subs.vtt": ("WEBVTT\n\n00:00.000 --> 00:01.000\nbig\n", "text/vtt"),
    "logo.svg": (
        '<svg xmlns="http://www.w3.org/2000/svg" width="4" height="4">'
        '<image href="ref.png" width="4" height="4"/></svg>',
        "image/svg+xml",
    ),
}


def _fam_feeds(port):
    return (
        '<link rel="alternate" type="application/rss+xml" href="/big/f12/rss.xml">'
        '<a href="/big/f12/atom.xml">atom</a>'
        '<a href="/big/f12/sitemap.xml">sitemap</a>'
    )


def _fam_plain(port):
    return (
        '<a href="../f1/one.html">one</a>'
        '<a href="./two.html">two</a>'
        '<a href="../../big/f1/tri.html">tri</a>'
        '<a href="/big/f1/abs.html">abs</a>'
        '<a href="/big/f1/list.html">list</a>'
        '<a href="/big/f1/list.html?page=2">p2</a>'
        '<a href="/big/f1/list.html?page=3&amp;sort=asc">p3</a>'
        '<a href="/big/f1/dir">dir</a>'
        '<a href="">self</a><a href="#">frag</a>'
        '<a href="mailto:big@example.com">mail</a>'
        '<a href="tel:+15551234">tel</a>'
        '<a href="data:text/plain;base64,aGk=">data</a>'
    )


def _fam_srcset(port):
    return (
        '<img src="/big/a/f2-base.png">'
        '<img srcset="/big/a/f2-1x.png 1x, /big/a/f2-2x.png 2x"'
        ' src="/big/a/f2-base.png">'
        '<img data-srcset="/big/a/f2-1x.png 1x, /big/a/f2-2x.png 2x"'
        ' src="/big/a/f2-base.png" loading="lazy">'
        '<picture><source type="image/webp" srcset="/big/a/f2-alt.webp">'
        '<img src="/big/a/f2-base.png"></picture>'
    )


def _fam_media(port):
    return (
        '<video src="/big/a/clip.mp4" poster="/big/a/poster.jpg">'
        '<source src="/big/a/clip.webm" type="video/webm">'
        '<track src="/big/a/subs.vtt" kind="subtitles" srclang="en">'
        "</video>"
        '<audio><source src="/big/a/tune.mp3" type="audio/mpeg"></audio>'
    )


def _fam_css(port):
    # image-set with descriptors is a proven-safe decoy (engine-surface §6).
    return (
        '<link rel="stylesheet" href="/big/a/print.css" media="print">'
        '<div style="background:url(/big/a/attr-bg.png)">styled</div>'
        '<style>@import "/big/a/blk.css"; h1 { background: url(/big/a/blk-bg.gif); }'
        ' h2 { background-image: image-set("/big/x/is1.png" 1x, "/big/x/is2.png" 2x); }'
        "</style>"
    )


def _fam_js(port):
    # The concatenated string is rejected by the scanner (no single literal).
    return (
        '<script src="/big/a/heavy.js"></script>'
        '<script src="/big/a/modern.js"></script>'
        "<script>document.write('<a href=\"/big/f5/dw.html\">dw</a>');\n"
        'var nope = "xx-" + "/big/x/concat.html";</script>'
    )


def _fam_meta(port):
    # Extensionless decoy targets stay unfetchable even if the aggressive
    # parser fires (no known extension, no scheme: rejected in every state).
    return (
        '<meta http-equiv="refresh" content="2;URL=/big/f6/refreshed.html">'
        '<a href="/big/f6/based.html">based</a>'
        '<meta property="og:image" content="/big/x/og">'
        '<meta name="twitter:image" content="/big/x/tw">'
        '<script type="application/ld+json">'
        '{"@type": "Thing", "image": "/big/x/jsonld.png"}</script>'
    )


def _fam_legacy(port):
    # Comma-valued applet archive is rejected whole by the engine (decoy).
    return (
        '<a href="/big/f7/frames.html">frames</a>'
        '<img src="/big/a/map.gif" usemap="#m">'
        '<map name="m">'
        '<area shape="rect" coords="0,0,9,9" href="/big/f7/area.html"></map>'
        '<embed src="/big/a/e.pdf" type="application/pdf" width="9" height="9">'
        '<object data="/big/a/o.pdf" type="application/pdf"></object>'
        '<applet archive="/big/x/aj.jar,/big/x/bj.jar" width="1" height="1"></applet>'
    )


def _fam_svg(port):
    return (
        '<svg width="9" height="9">'
        '<image href="/big/a/svg-in.png" width="4" height="4"/>'
        '<use xlink:href="#icon"/></svg>'
        '<img src="/big/a/logo.svg">'
    )


def _fam_i18n(port):
    return (
        '<a href="/big/f9/caf%C3%A9.html">cafe</a>'
        '<a href="/big/f9/latin1.html">latin1</a>'
        '<a href="/big/f9/metaonly.html">meta</a>'
        '<a href="/big/f9/bom.html">bom</a>'
    )


def _fam_http(port):
    return (
        '<a href="/big/r/hop1">chain</a>'
        '<a href="/big/r/get42">get42</a>'
        '<a href="/big/d/01">d01</a>'
        '<a href="/big/d/02">d02</a>'
        '<a href="/big/f10/empty.html">empty</a>'
        '<a href="/big/d/dl">dl</a>'
    )


def _fam_forms(port):
    # GET form action is rewritten but never fetched; formaction/ping are
    # outside the attribute tables (decoys).
    return (
        '<form action="/big/x/form-target.html" method="get">'
        '<input type="text" name="q">'
        '<input type="image" src="/big/a/btn.png" alt="go"></form>'
        '<a href="/big/f11/page.html">bare</a>'
        '<a href="/big/f11/page.html?utm_source=news&amp;utm_medium=mail">utm</a>'
        '<a href="/big/f11/sess.html?PHPSESSID=deadbeef123">sess</a>'
        '<button formaction="/big/x/formact">go</button>'
        '<a href="/big/f11/page.html" ping="/big/x/ping">ping</a>'
    )


BIG_FAMILIES = [
    _fam_feeds,
    _fam_plain,
    _fam_srcset,
    _fam_media,
    _fam_css,
    _fam_js,
    _fam_meta,
    _fam_legacy,
    _fam_svg,
    _fam_i18n,
    _fam_http,
    _fam_forms,
]


def big_link(m, style):
    return ["%d.html" % m, "../p/%d.html" % m, "/big/p/%d.html" % m][style]


def big_page(n, port):
    style = n % 3
    home = ["../index.html", "/big/index.html", "../index.html"][style]
    parts = ['<a href="%s">home</a>' % home]
    if n > 0:
        parts.append('<a href="%s">up</a>' % big_link((n - 1) // BIG_FANOUT, style))
    for c in range(n * BIG_FANOUT + 1, n * BIG_FANOUT + BIG_FANOUT + 1):
        if c < BIG_PAGES:
            parts.append('<a href="%s">p%d</a>' % (big_link(c, style), c))
    parts.append('<link rel="stylesheet" href="/big/a/site.css">')
    parts.append('<script src="/big/a/app.js"></script>')
    exts = ["png", "gif", "jpg"]
    ia = "/big/a/i%da.%s" % (n, exts[n % 3])
    ib = "/big/a/i%db.%s" % (n, exts[(n + 1) % 3])
    # Rotate the second-image construct across deterministic table attributes.
    con = n % 4
    if con == 0:
        parts.append('<img src="%s"><img src="%s">' % (ia, ib))
    elif con == 1:
        parts.append(
            '<img src="%s"><table background="%s"><tr><td>t</td></tr></table>'
            % (ia, ib)
        )
    elif con == 2:
        parts.append('<img src="%s"><img src="%s" data-src="%s">' % (ia, ia, ib))
    else:
        parts.append(
            '<img src="%s" loading="lazy"><video poster="%s"></video>' % (ia, ib)
        )
    parts.append(BIG_FAMILIES[n % 12](port))
    return big_html("p%d" % n, "\n".join(parts))


def big_index(port):
    return big_html(
        "big index",
        '<link rel="stylesheet" href="/big/a/site.css">'
        '<script src="/big/a/app.js"></script>'
        '<a href="p/0.html">root</a>'
        '<img src="/big/a/d1/d2/d3/d4/d5/d6/d7/d8/deep.png">'
        '<a href="/big/f1/long.html?x=%s">long</a>'
        '<a href="/big/f1/gzok.html">gzok</a>'
        '<a href="/big/f1/gzid.html">gzid</a>'
        '<a href="//127.0.0.1:%d/big/f1/protorel.html">protorel</a>'
        '<a href="http://127.0.0.1:%d/big/f1/abshost.html">abshost</a>'
        '<a href="/big/e/404.html">e404</a>'
        '<a href="/big/e/410.html">e410</a>'
        '<a href="/big/e/500.html">e500</a>'
        '<a href="/big/e/gztrunc.html">gzt</a>'
        '<a href="?">query</a>' % ("a" * 900, port, port),
    )


BIG_REDIRECTS = {
    "/big/r/hop1": (301, "/big/r/hop2"),
    "/big/r/hop2": (302, "/big/f10/land.html"),
    "/big/r/get42": (301, "/big/a/doc.pdf"),
    "/big/f1/dir": (301, "/big/f1/dir/"),
}

BIG_SIMPLE_PAGES = {
    "/big/p/two.html": "dot-slash target",
    "/big/f1/one.html": "one",
    "/big/f1/tri.html": "tri",
    "/big/f1/abs.html": "abs",
    "/big/f1/dir/": "dir index",
    "/big/f1/long.html": "long",
    "/big/f1/gzok.html": "gzok",
    "/big/f1/gzid.html": "gzid",
    "/big/f1/protorel.html": "protorel",
    "/big/f1/abshost.html": "abshost",
    "/big/f5/dw.html": "dw target",
    "/big/f6/refreshed.html": "refreshed",
    "/big/f6/sub/leaf.html": "leaf",
    "/big/f7/fa.html": "frame a",
    "/big/f7/fb.html": "frame b",
    "/big/f7/fn.html": "noframes",
    "/big/f7/area.html": "area",
    "/big/f10/land.html": "landed",
    "/big/f11/page.html": "the page",
    "/big/f11/sess.html": "the sess page",
}

# Extensionless downloads: name resolution is wire-type driven (#478 contract).
BIG_DOWNLOADS = {
    "/big/d/01": ("pdf", None),
    "/big/d/02": ("png", None),
    "/big/d/dl": ("pdf", 'attachment; filename="named.pdf"'),
}


def _big_rss(port):
    # purl.org marker makes the feed parse; item URLs are already-linked pages.
    return (
        '<?xml version="1.0"?>\n'
        '<rss version="2.0" xmlns:content="http://purl.org/rss/1.0/modules/content/">\n'
        "<channel><title>big</title><link>http://127.0.0.1:%d/big/index.html</link>\n"
        "<item><title>i1</title><link>http://127.0.0.1:%d/big/p/1.html</link>\n"
        '<enclosure url="http://127.0.0.1:%d/big/p/2.html" type="text/html"/></item>\n'
        "</channel></rss>\n" % (port, port, port)
    ).encode()


def _big_atom(port):
    # No purl marker: emitted verbatim, its URL must never be fetched.
    return (
        '<?xml version="1.0"?>\n'
        '<feed xmlns="http://www.w3.org/2005/Atom"><title>big</title>\n'
        "<entry><title>e1</title>"
        '<link href="http://127.0.0.1:%d/big/x/atom-only.html"/>'
        "</entry></feed>\n" % port
    ).encode()


def _big_sitemap(port):
    return (
        '<?xml version="1.0"?>\n'
        '<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">\n'
        "<url><loc>http://127.0.0.1:%d/big/x/sitemap-only.html</loc></url>\n"
        "</urlset>\n" % port
    ).encode()


DEEPDIR = "/trickle/deep/a-long-directory-segment/another-long-segment"


class Handler(SimpleHTTPRequestHandler):
    # Quieter logging; the launcher captures httrack's own log anyway.
    def log_message(self, fmt, *args):
        if os.environ.get("LOCAL_SERVER_VERBOSE"):
            super().log_message(fmt, *args)

    # --- helpers -----------------------------------------------------------

    def request_cookies(self):
        """Parse the Cookie header into {name: decoded-value}.

        Mirrors PHP's $_COOKIE: values are url-decoded, matching the encoding
        applied when the cookie was set (see set_cookie)."""
        jar = {}
        raw = self.headers.get("Cookie", "")
        for pair in raw.split(";"):
            pair = pair.strip()
            if "=" in pair:
                name, value = pair.split("=", 1)
                jar[name.strip()] = unquote(value.strip())
        return jar

    def set_cookie(self, name, value):
        """Queue a Set-Cookie header, url-encoding the value like PHP's
        setcookie() so spaces/quotes/commas stay a single token that httrack
        can store and replay verbatim."""
        self._set_cookies.append(f"{name}={quote(value)}; Path={COOKIE_PATH}")

    def send_html(self, body, status=200, extra_status=None):
        encoded = PAGE.format(body=body).encode("utf-8")
        self.send_response(status, extra_status)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(encoded)))
        for cookie in self._set_cookies:
            self.send_header("Set-Cookie", cookie)
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(encoded)

    def fail_cookie(self, what):
        # The old PHPs answered 500 with the reason in the status line.
        self.send_html("", status=500, extra_status=f"The {what} is missing or invalid")

    # --- dynamic routes ----------------------------------------------------

    def route_entrance(self):
        self.set_cookie("cat", COOKIES["cat"])
        self.set_cookie("cake", COOKIES["cake"])
        self.send_html('\tThis is a <a href="second.php">link</a>')

    def route_second(self):
        jar = self.request_cookies()
        if jar.get("cat") != COOKIES["cat"]:
            return self.fail_cookie("cat")
        if jar.get("cake") != COOKIES["cake"]:
            return self.fail_cookie("cake")
        self.set_cookie("badger", COOKIES["badger"])
        self.send_html('\tThis is a <a href="third.php">link</a>')

    def route_third(self):
        jar = self.request_cookies()
        if jar.get("cat") != COOKIES["cat"]:
            return self.fail_cookie("cat")
        if jar.get("cake") != COOKIES["cake"]:
            return self.fail_cookie("cake")
        if jar.get("badger") != COOKIES["badger"]:
            return self.fail_cookie("badger")
        self.send_html("\tThis is a test.")

    # --cookies-file (#215): the secret page needs a cookie no page ever sets,
    # so it is reachable only when --cookies-file preloads it.
    GATE_COOKIE = ("session", "opensesame")

    def route_gated_index(self):
        self.send_html('\tThis is a <a href="secret.php">link</a>')

    def route_gated_secret(self):
        name, value = self.GATE_COOKIE
        if self.request_cookies().get(name) != value:
            return self.fail_cookie(name)
        self.send_html("\tThis is the secret.")

    # A User-Agent carrying NO_SITEMAP_UA gets a robots.txt with no Sitemap:
    # record, so a test can drive the /sitemap.xml fallback instead.
    NO_SITEMAP_UA = "nositemap"
    # ... and one that additionally Disallows the well-known location, so the
    # fallback has to be refused by the rules this very body carries.
    DENY_SITEMAP_UA = "denysitemap"
    # ... the same Disallow, but with the sitemap also declared: the
    # declaration is the site inviting the fetch and must win.
    DENY_DECLARED_UA = "denydeclared"
    # ... and one that points the sitemap at the site root, to check a subtree
    # crawl is not widened by where the site chooses to put its sitemap.
    SCOPE_SITEMAP_UA = "scopesitemap"
    # ... and one whose probe fails, with a body past the 1070 bytes that
    # issue #769's over-read needed.
    ERROR_ROBOTS_UA = "errorrobots"
    # ... and "robotsblobNNNN", which sizes the Disallow list so the engine's
    # stored rules come to exactly NNNN bytes, /secret/ last (#1286).
    BLOB_ROBOTS_RE = re.compile(r"robotsblob(\d+)")
    # ... and one whose last Disallow outruns the reader's line buffer, its own
    # tail spelling an Allow that re-opens what the line above forbids (#1294).
    TAIL_ROBOTS_UA = "tailrobots"
    # ... the same Allow written as a line of its own: the control saying the
    # rule is one the engine acts on, so a refusal is the tail going unread.
    TAIL_ONLINE_UA = "tailonline"
    TAIL_RESUME = 1023  # where the old reader resumed: HTS_ROBOTS_LINE_SIZE - 1

    def tail_robots_body(self, own_line):
        phantom = "Allow: /secret/"
        # /hidden/ is refused by both bodies, so a leg that mirrors it never
        # read this robots.txt at all
        head = "User-agent: *\nDisallow: /hidden/\nDisallow: /secret/\n"
        if own_line:
            return (head + phantom + "\n").encode()
        rule = "Disallow: "
        rule += "a" * (self.TAIL_RESUME - len(rule))
        return (head + rule + phantom + "\n").encode()

    def robots_blob_body(self, blobsize):
        # One "<marker><pattern>\n" line per stored rule, so a rule costs
        # len(pattern) + 2. tests/01_engine-robots.test pins that accounting.
        rules = []
        used = 0
        room = blobsize - (len("/secret/") + 2)
        while used + 12 <= room:
            rules.append("/pad%05d/" % len(rules))
            used += 12
        # A 1- or 2-byte filler cannot be written, and a 1-char one would be the
        # site-wide "/". Give back a pad rule until the remainder is spendable.
        while 0 < room - used < 5 and rules:
            rules.pop()
            used -= 12
        if room - used:
            rules.append("/" + "z" * (room - used - 3))
        rules.append("/secret/")
        return (
            "User-agent: *\n" + "".join("Disallow: %s\n" % r for r in rules)
        ).encode()

    def route_robots(self):
        # The Sitemap: record is group-independent; only --sitemap acts on it.
        ua = self.headers.get("User-Agent") or ""
        if self.ERROR_ROBOTS_UA in ua:
            body = b"# " + b"x" * 4000 + b"\n"
            self.send_response(404, "Not Found")
            self.send_header("Content-Type", "text/plain")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            if self.command != "HEAD":
                self.wfile.write(body)
            return
        blob = self.BLOB_ROBOTS_RE.search(ua)
        if blob:
            self.send_raw(self.robots_blob_body(int(blob.group(1))), "text/plain")
            return
        if self.TAIL_ROBOTS_UA in ua or self.TAIL_ONLINE_UA in ua:
            self.send_raw(
                self.tail_robots_body(self.TAIL_ONLINE_UA in ua), "text/plain"
            )
            return
        host = self.headers.get("Host")
        body = "User-agent: *\nDisallow:\n"
        if self.DENY_DECLARED_UA in ua:
            body = (
                "User-agent: *\nDisallow: /sitemap.xml\n"
                f"Sitemap: http://{host}/sitemap.xml\n"
            )
        elif self.DENY_SITEMAP_UA in ua:
            body = "User-agent: *\nDisallow: /sitemap.xml\n"
        elif self.SCOPE_SITEMAP_UA in ua:
            body += f"Sitemap: http://{host}/scopesitemap.xml\n"
        elif self.NO_SITEMAP_UA not in ua:
            body += f"Sitemap: http://{host}/sitemapdir/index.xml\n"
        body = body.encode()
        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    # --- sitemap ingestion (issue #712) ------------------------------------
    # start.html links to nothing, so orphan*.html are reachable only through
    # the sitemap. deep1.html proves the seeds keep a full depth budget; the
    # off-host page <loc> must be dropped by the travel scope, and the off-host
    # child sitemap by the ingester's same-host rule. The index is served both
    # from /sitemapdir/ (named by robots.txt) and from the well-known
    # /sitemap.xml (the fallback).

    def route_sitemap_index(self):
        host = self.headers.get("Host")
        self.send_raw(
            '<?xml version="1.0" encoding="UTF-8"?>\n'
            '<sitemapindex xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">'
            f"<sitemap><loc>http://{host}/sitemapdir/pages.xml.gz</loc></sitemap>"
            "<sitemap><loc>http://sitemap-offhost.invalid/s.xml</loc></sitemap>"
            "</sitemapindex>\n".encode(),
            "application/xml",
        )

    def route_sitemap_pages(self):
        host = self.headers.get("Host")
        xml = (
            '<?xml version="1.0" encoding="UTF-8"?>\n'
            '<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">'
            f"<url><loc>http://{host}/sitemapdir/orphan1.html</loc></url>"
            "<url><loc>http://sitemap-offhost.invalid/x.html</loc></url>"
            f"<url><loc>http://{host}/sitemapdir/orphan2.html</loc></url>"
            "</urlset>\n"
        ).encode()
        self.send_raw(gzip.compress(xml), "application/x-gzip")

    def route_sitemap_start(self):
        self.send_html("\tNothing links to the sitemap pages.")

    def route_sitemap_orphan1(self):
        self.send_html('\t<a href="deep1.html">deeper</a>')

    def route_sitemap_orphan2(self):
        self.send_html("\tSecond orphan.")

    def route_sitemap_deep1(self):
        self.send_html("\tOne level below an orphan.")

    # chainN is a sitemapindex at nesting level N, listing chain(N+1) and a
    # urlset capN.xml whose single page is capN.html. Levels up to
    # HTS_SITEMAP_MAX_LEVEL are followed, so capN.html appears for N below the
    # cap and stops appearing at it: the pair pins the boundary, which a cap
    # mutated either way would break.
    def route_sitemap_chain(self):
        host = self.headers.get("Host")
        level = int(self.path.rsplit("/", 1)[-1][len("chain") : -len(".xml")])
        self.send_raw(
            (
                '<?xml version="1.0" encoding="UTF-8"?>\n<sitemapindex>'
                f"<sitemap><loc>http://{host}/sitemapdir/chain{level + 1}.xml"
                "</loc></sitemap>"
                f"<sitemap><loc>http://{host}/sitemapdir/cap{level}.xml"
                "</loc></sitemap></sitemapindex>\n"
            ).encode(),
            "application/xml",
        )

    def route_sitemap_capset(self):
        host = self.headers.get("Host")
        level = self.path.rsplit("/", 1)[-1][len("cap") : -len(".xml")]
        self.send_raw(
            (
                '<?xml version="1.0" encoding="UTF-8"?>\n<urlset>'
                f"<url><loc>http://{host}/sitemapdir/cap{level}.html</loc></url>"
                "</urlset>\n"
            ).encode(),
            "application/xml",
        )

    def route_sitemap_cappage(self):
        self.send_html("\tReached through a nested sitemapindex.")

    def route_sitemap_gatedindex(self):
        host = self.headers.get("Host")
        self.send_raw(
            '<?xml version="1.0" encoding="UTF-8"?><sitemapindex>'
            f"<sitemap><loc>http://{host}/sitemapdir/filtered.xml</loc></sitemap>"
            "</sitemapindex>\n".encode(),
            "application/xml",
        )

    def route_sitemap_filtered(self):
        host = self.headers.get("Host")
        self.send_raw(
            '<?xml version="1.0" encoding="UTF-8"?><urlset>'
            f"<url><loc>http://{host}/sitemapdir/gated.html</loc></url>"
            "</urlset>\n".encode(),
            "application/xml",
        )

    def route_sitemap_gated(self):
        self.send_html("\tListed only by the filtered child sitemap.")

    # A moved sitemap: the marking has to follow the redirect.
    # A root sitemap naming a page below the crawl's start directory and one
    # above it. Only the first may be seeded when the crawl started at /deep/dir/.
    def route_sitemap_scope(self):
        host = self.headers.get("Host")
        self.send_raw(
            '<?xml version="1.0" encoding="UTF-8"?><urlset>'
            f"<url><loc>http://{host}/deep/dir/below.html</loc></url>"
            f"<url><loc>http://{host}/elsewhere/updir.html</loc></url>"
            "</urlset>\n".encode(),
            "application/xml",
        )

    def route_sitemap_deepstart(self):
        self.send_html("\tA start page in a subdirectory, linking nothing.")

    def route_sitemap_below(self):
        self.send_html("\tBelow the start directory.")

    def route_sitemap_updir(self):
        self.send_html("\tAbove the start directory.")

    def route_sitemap_moved(self):
        self.send_response(301)
        self.send_header("Location", "/sitemapdir/pages.xml.gz")
        self.send_header("Content-Length", "0")
        self.end_headers()

    # --- JavaScript content types (#302 increment) -------------------------
    # One spelling of the JS type per directory; links inside a script resolve
    # against the parent page, so every target lands directly under /jsmime/.
    JSMIME_TYPES = {
        "xjs": ("application/x-javascript", "js"),
        "txt": ("text/javascript", "js"),
        "app": ("application/javascript", "js"),
        "chs": ("text/javascript; charset=utf-8", "js"),
        "mod": ("text/javascript", "mjs"),
    }

    # Link-free script: scanning it must invent no link and rewrite no byte.
    # q.bin serves the same bytes under a type nothing scans, as the reference.
    JSMIME_QUIET = (
        b'"use strict";\n'
        b'var cfg = { name: "app/main", version: "1.2.3", sep: "/" };\n'
        b'function join(a, b) { return a + "/" + b; }\n'
        b"var re = /^[a-z]+\\.[a-z]+$/;\n"
        b'var tpl = "<div class=\\"x\\">no link here</div>";\n'
        b"console.log(join(cfg.name, tpl), re);\n"
    )

    @staticmethod
    def jsmime_script(key):
        return (
            b'var a = "%(k)s1.html";\n'
            b"var b = '%(k)s2.html';\n"
            b'window.location = "%(k)s3.html";\n' % {b"k": key.encode()}
        )

    def route_jsmime(self):
        path = urlsplit(self.path).path
        rest = path[len("/jsmime/") :]
        if rest in ("", "index.html"):
            body = ""
            for key, (_, ext) in self.JSMIME_TYPES.items():
                attr = ' type="module"' if key == "mod" else ""
                body += '\t<script%s src="%s/s.%s"></script>\n' % (attr, key, ext)
            body += '\t<script src="quiet/q.js"></script>\n'
            body += '\t<a href="quiet/q.bin">reference copy</a>\n'
            body += '\t<a href="json/d.json">data</a>\n'
            self.send_html(body)
            return
        if rest == "quiet/q.js":
            self.send_raw(self.JSMIME_QUIET, "text/javascript")
            return
        if rest == "quiet/q.bin":
            self.send_raw(self.JSMIME_QUIET, "application/octet-stream")
            return
        if rest == "json/d.json":
            # Not a script type: widening the JS set must not reach this one.
            self.send_raw(b'{ "u": "jsn1.html" }\n', "application/json")
            return
        key = rest.split("/")[0]
        if key in self.JSMIME_TYPES:
            ctype, ext = self.JSMIME_TYPES[key]
            if rest == "%s/s.%s" % (key, ext):
                self.send_raw(self.jsmime_script(key), ctype)
                return
        elif re.fullmatch(r"[a-z]{3}[123]\.html", rest):
            self.send_html("\tTarget %s reached from a script.\n" % rest)
            return
        self.send_error(404)

    # --- type/extension matrix (issue #267 family) -------------------------

    def send_raw(self, body, content_type, extra_headers=()):
        """Send a raw body with an explicit Content-Type, or none at all when
        content_type is None (to observe httrack's typeless-file naming)."""
        self.send_response(200)
        if content_type is not None:
            self.send_header("Content-Type", content_type)
        for name, value in extra_headers:
            self.send_header(name, value)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    # Fake-binary blobs for the image/pdf/typeless cases.
    FAKE_PNG = b"\x89PNG\r\n\x1a\n" + b"\x00" * 64
    FAKE_PDF = b"%PDF-1.4\n" + b"\x00" * 64
    FAKE_JPEG = b"\xff\xd8\xff\xe0" + b"\x00" * 64
    BIG_JPEG = b"\xff\xd8\xff\xe0" + bytes(range(256)) * 64  # > sniff window

    # path -> (body, content_type); None sends no header, "" sends an empty
    # Content-Type value (no usable type, must be treated like None).
    TYPE_MATRIX = {
        "/types/control.php": (b"<html><body>control</body></html>", "text/html"),
        "/types/photo.png": (FAKE_PNG, "image/png"),
        "/types/doc.pdf": (FAKE_PDF, "application/pdf"),
        "/types/notype.png": (FAKE_PNG, None),
        "/types/notype.pdf": (FAKE_PDF, None),
        "/types/emptyct.png": (FAKE_PNG, ""),
        "/types/lie.png": (FAKE_PNG, "text/html"),
        "/types/wrongtype.jpg": (FAKE_JPEG, "image/png"),
        "/types/bigtype.jpg": (BIG_JPEG, "image/png"),
        "/types/report.pdf": (b"<html><body>real page</body></html>", "text/html"),
        "/types/page.htm": (b"<html><body>htm page</body></html>", "text/html"),
        "/types/script.js": (b"var x = 1;\n", "application/javascript"),
        "/types/style.css": (b"body { color: red; }\n", "text/css"),
        "/types/data.json": (b'{"k": "v"}\n', "application/json"),
        "/types/gen.php": (FAKE_PNG, "image/png"),
    }

    def route_types_index(self):
        body = (
            '\t<a href="control.php">control</a>\n'
            '\t<img src="photo.png" />\n'
            '\t<a href="doc.pdf">doc</a>\n'
            '\t<img src="notype.png" />\n'
            '\t<a href="notype.pdf">notypepdf</a>\n'
            '\t<img src="emptyct.png" />\n'
            '\t<img src="lie.png" />\n'
            '\t<img src="wrongtype.jpg" />\n'
            '\t<img src="bigtype.jpg" />\n'
            '\t<img src="mutant.jpg" />\n'
            '\t<img src="packed.jpg" />\n'
            '\t<a href="report.pdf">report</a>\n'
            '\t<a href="page.htm">htm</a>\n'
            '\t<script src="script.js"></script>\n'
            '\t<link rel="stylesheet" href="style.css" />\n'
            '\t<a href="data.json">json</a>\n'
            '\t<img src="gen.php?id=5" />\n'
        )
        self.send_html(body)

    def route_types(self):
        path = urlsplit(self.path).path
        body, ctype = self.TYPE_MATRIX[path]
        self.send_raw(body, ctype)

    # content changes between crawls: run 1 sniffs JPEG, the update pass must
    # keep the run-1 name (recorded verdict) even though the body is now PNG
    MUTANT_SEEN = set()

    def route_types_mutant(self):
        path = urlsplit(self.path).path
        body = self.FAKE_PNG if path in self.MUTANT_SEEN else self.FAKE_JPEG
        if self.command != "HEAD":
            self.MUTANT_SEEN.add(path)
        self.send_raw(body, "image/png")

    # gzip on the wire: the sniff must see the decoded body, not the stream
    def route_types_packed(self):
        self.send_raw(
            gzip.compress(self.FAKE_JPEG),
            "image/png",
            extra_headers=[("Content-Encoding", "gzip")],
        )

    # A gzip-coded HTML page whose decoded body is known, for the verbatim-WARC
    # differential (stored compressed bytes must inflate to this).
    WARCGZ_BODY = b"<html><body>verbatim gzip page for WARC strategy A</body></html>\n"

    # A NON-html gzip-coded asset: HTTrack streams it straight to disk, so the
    # verbatim spool adoption runs on the is_write (direct-to-disk) branch of
    # back_finalize, not the in-memory branch route_warcgz_page exercises.
    WARCGZ_BIN_BODY = b"verbatim gzip octet-stream body for the WARC is_write path\n"

    def route_warcgz_index(self):
        self.send_html(
            '\t<a href="page.html">page</a>\n' '\t<a href="data.bin">data</a>\n'
        )

    def route_warcgz_page(self):
        self.send_raw(
            gzip.compress(self.WARCGZ_BODY),
            "text/html",
            extra_headers=[("Content-Encoding", "gzip")],
        )

    def route_warcgz_data(self):
        self.send_raw(
            gzip.compress(self.WARCGZ_BIN_BODY),
            "application/octet-stream",
            extra_headers=[("Content-Encoding", "gzip")],
        )

    # --- content codings ---------------------------------------------------
    # Canned br/zstd bodies (no brotli/zstd module in the stdlib): both decode
    # to CODEC_BODY. Regenerate with the brotli/zstd CLIs over that string.
    CODEC_BODY = (
        b"<html><head><title>codec</title></head>"
        b"<body><p>coded body</p></body></html>"
    )
    CODEC_BR = base64.b64decode(
        "G0sAAAQccqSBBfJlUvOccsDeitqC9CbHwENWiptQj5aExP0mBjjVgy2DF17olLzLo2T2Eg=="
    )
    CODEC_ZSTD = base64.b64decode(
        "KLUv/SBMFQIAFAM8aHRtbD48aGVhZD48dGl0bGU+Y29kZWM8LzwvYm9keT48"
        "cGQgPC9wPjwvL2h0bWw+BQA7p8QMDNQ1PgcWhjkG"
    )

    def route_codec_index(self):
        self.send_html(
            '\t<a href="br.html">br</a>\n'
            '\t<a href="zstd.html">zstd</a>\n'
            '\t<a href="junk.html">junk</a>\n'
            '\t<a href="bad.html">bad</a>\n'
            '\t<a href="bin.dat">bin</a>\n'
            '\t<a href="ae.html">ae</a>\n'
        )

    def route_codec_br(self):
        self.send_raw(
            self.CODEC_BR, "text/html", extra_headers=[("Content-Encoding", "br")]
        )

    def route_codec_zstd(self):
        self.send_raw(
            self.CODEC_ZSTD, "text/html", extra_headers=[("Content-Encoding", "zstd")]
        )

    # Junk token on a plain body: the page must survive (broken servers do this)
    def route_codec_junk(self):
        self.send_raw(
            b"<html><body><p>junk coding</p></body></html>",
            "text/html",
            extra_headers=[("Content-Encoding", "utf-8")],
        )

    # A real coding we have no decoder for: the fetch must fail rather than
    # save the coded bytes as the page.
    def route_codec_bad(self):
        self.send_raw(
            b"<html><body><p>never decoded</p></body></html>",
            "text/html",
            extra_headers=[("Content-Encoding", "compress")],
        )

    # Same, on a non-HTML body: this takes the direct-to-disk (is_write) branch,
    # a different discard path than the in-memory bad.html above.
    def route_codec_bin(self):
        self.send_raw(
            b"\x00\x01\x02 CODED-BINARY-MUST-NOT-LAND \xff\xfe" * 8,
            "application/octet-stream",
            extra_headers=[("Content-Encoding", "compress")],
        )

    # --- coded re-fetch that fails to decode (#557) -------------------------
    # Pass 1 mirrors each file from a valid gzip body; pass 2 (--update) serves
    # a body that cannot be decoded. The previously-mirrored copy must survive.
    # fresh.html is the control: its pass-2 body decodes, so it must be updated.
    # Per-path body-fetch counter shared by the update-refetch routes; paths are
    # distinct so one dict serves all of them.
    REFETCH_SEEN = {}

    def refetch_pass(self):
        """1 on the first body fetch of this path, N on the Nth. HEADs don't
        count, so a stray one can't shift which pass gets the special body."""
        if self.command == "HEAD":
            return 1
        seen = Handler.REFETCH_SEEN.get(self.path, 0) + 1
        Handler.REFETCH_SEEN[self.path] = seen
        return seen

    @staticmethod
    def gzipped(body):
        return gzip.compress(body)

    @staticmethod
    def bad_gzip(body):
        """A gzip stream whose deflate payload is mangled: inflate fails partway
        through, after some plausible output has already been produced."""
        raw = bytearray(gzip.compress(body))
        raw[20:40] = b"\xff" * 20
        return bytes(raw[:-4])

    def send_coded(self, body, content_type, coding="gzip"):
        self.send_raw(body, content_type, extra_headers=[("Content-Encoding", coding)])

    def route_upcodec_index(self):
        self.send_html(
            '\t<a href="mem.html">mem</a>\n'
            '\t<a href="disk.bin">disk</a>\n'
            '\t<a href="unsup.html">unsup</a>\n'
            '\t<a href="fresh.html">fresh</a>\n'
            '\t<a href="freshdisk.bin">freshdisk</a>\n'
        )

    MEM_V1 = b"<html><body><p>MIRRORED-MEM-V1</p></body></html>"
    DISK_V1 = b"MIRRORED-DISK-V1\n" + b"\x00\x01\x02\xff" * 8192
    UNSUP_V1 = b"<html><body><p>MIRRORED-UNSUP-V1</p></body></html>"

    def route_upcodec_mem(self):
        if self.refetch_pass() == 1:
            self.send_coded(self.gzipped(self.MEM_V1), "text/html")
        else:
            self.send_coded(self.bad_gzip(self.MEM_V1), "text/html")

    def route_upcodec_disk(self):
        if self.refetch_pass() == 1:
            self.send_coded(self.gzipped(self.DISK_V1), "application/octet-stream")
        else:
            self.send_coded(self.bad_gzip(self.DISK_V1), "application/octet-stream")

    # Pass 2 switches to a coding we have no decoder for.
    def route_upcodec_unsup(self):
        if self.refetch_pass() == 1:
            self.send_coded(self.gzipped(self.UNSUP_V1), "text/html")
        else:
            self.send_coded(self.UNSUP_V1, "text/html", coding="compress")

    def route_upcodec_fresh(self):
        pass1 = self.refetch_pass() == 1
        body = b"<html><body><p>FRESH-V%d</p></body></html>" % (1 if pass1 else 2)
        self.send_coded(self.gzipped(body), "text/html")

    # Same, direct-to-disk: the update pass decodes, so the temp is renamed over
    # an existing mirror file.
    def route_upcodec_freshdisk(self):
        pass1 = self.refetch_pass() == 1
        body = b"FRESHDISK-V%d\n" % (1 if pass1 else 2) + b"\x03\x02\x01\xfe" * 8192
        self.send_coded(self.gzipped(body), "application/octet-stream")

    # #562: pass 1 mirrors fully; pass 2 (--update) declares the full
    # Content-Length but delivers half then closes, so httrack refuses the partial.
    PAGE_V1 = b"<html><body><p>MIRRORED-PAGE-V1</p></body></html>"
    BIN_V1 = b"MIRRORED-BIN-V1\n" + b"\x00\x01\x02\xff" * 8192

    def route_uptrunc_index(self):
        self.send_html(
            '\t<a href="page.html">page</a>\n'
            '\t<a href="file.bin">file</a>\n'
            '\t<a href="stay.html">stay</a>\n'
        )

    def send_truncated(self, body, content_type):
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if self.command == "HEAD":
            return
        try:
            self.wfile.write(body[: len(body) // 2])  # short, then close
            self.wfile.flush()
        except OSError:
            pass

    def route_uptrunc_page(self):
        if self.refetch_pass() == 1:
            self.send_raw(self.PAGE_V1, "text/html")
        else:
            self.send_truncated(self.PAGE_V1, "text/html")

    def route_uptrunc_file(self):
        if self.refetch_pass() == 1:
            self.send_raw(self.BIN_V1, "application/octet-stream")
        else:
            self.send_truncated(self.BIN_V1, "application/octet-stream")

    # Control: fully served both passes, so a normal --update still lands.
    def route_uptrunc_stay(self):
        v = 1 if self.refetch_pass() == 1 else 2
        self.send_raw(b"<html><body><p>STAY-V%d</p></body></html>" % v, "text/html")

    # --- re-fetch cut mid-header, so nothing is stored (#746, #748) ---------
    KEEP_PAGE = b"<html><body><p>KEEP-PAGE-V1</p></body></html>"
    KEEP_BIN = b"KEEP-BIN-V1\n" + b"\x51\x52\x53\x54" * 512
    KEEP_ERR = b"KEEP-ERR-V1\n" + b"\x61\x62\x63\x64" * 512

    def send_cut_headers(self):
        """Hang up mid-header: the response never becomes parseable."""
        self.close_connection = True
        try:
            self.wfile.write(b"HTTP/1.0 200 OK\r\nContent-Ty")
            self.wfile.flush()
        except OSError:
            pass
        self.connection.close()

    def route_keep_index(self):
        self.refetch_pass()
        self.send_html(
            '\t<a href="page.html">page</a>\n'
            '\t<a href="data.bin">data</a>\n'
            '\t<a href="err.bin">err</a>\n'
            '\t<a href="stay.bin">stay</a>\n'
        )

    def route_keep_page(self):
        if self.refetch_pass() == 1:
            self.send_raw(self.KEEP_PAGE, "text/html")
        else:
            self.send_cut_headers()

    def route_keep_data(self):
        if self.refetch_pass() == 1:
            self.send_raw(self.KEEP_BIN, "application/octet-stream")
        else:
            self.send_cut_headers()

    # Control: an HTTP error on the same resource already keeps the copy, so
    # the cut-header routes above must end up indistinguishable from it.
    def route_keep_err(self):
        if self.refetch_pass() == 1:
            self.send_raw(self.KEEP_ERR, "application/octet-stream")
        else:
            self.send_response(500)
            self.send_header("Content-Type", "text/html")
            self.send_header("Content-Length", "0")
            self.end_headers()

    # Control: answers normally, with a new body on pass 2, so a fix that
    # stopped overwriting mirrored files altogether would be caught.
    def route_keep_stay(self):
        v = 1 if self.refetch_pass() == 1 else 2
        self.send_raw(
            b"KEEP-STAY-V%d\n" % v + b"\x71\x72\x73\x74" * 512,
            "application/octet-stream",
        )

    # Range-capable asset for 343: a resume at or past the end gets the 416 the
    # on-disk acceptance path reads as "already complete", shorter gets a 206.
    RANGED_BIN = b"RANGED-" + bytes((i * 11 + 5) % 256 for i in range(8192))

    def route_ranged_asset(self):
        body = self.RANGED_BIN
        m = re.match(r"bytes=(\d+)-", self.headers.get("Range", "") or "")
        start = int(m.group(1)) if m else 0
        if not m or start == 0:
            self.send_raw(body, "application/octet-stream")
        elif start >= len(body):
            self.send_response(416, "Requested Range Not Satisfiable")
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Range", "bytes */%d" % len(body))
            self.send_header("Content-Length", "0")
            self.end_headers()
        else:
            self.send_response(206, "Partial Content")
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header(
                "Content-Range", "bytes %d-%d/%d" % (start, len(body) - 1, len(body))
            )
            self.send_header("Content-Length", str(len(body) - start))
            self.end_headers()
            if self.command != "HEAD":
                self.wfile.write(body[start:])

    # --- a hub page that fails on the update, taking its children with it --
    # The children are only reachable through hub.html: if the engine drops
    # them from new.lst because the hub was never parsed, the purge unlinks
    # them. gone.html is the live control: it really does leave the site.
    HUBFAIL_CHILD = b"<html><body><p>HUBFAIL-CHILD-%d</p></body></html>"

    def route_hubfail_index(self):
        links = '\t<a href="hub.html">hub</a>\n\t<a href="leaf.html">leaf</a>\n'
        if self.refetch_pass() <= 2:  # dropped for the third crawl only
            links += '\t<a href="gone.html">gone</a>\n'
        self.send_html(links)

    # Cuts off both attempts of the caller's second crawl (the request and the
    # one retry --retries=1 buys), then answers again, so the third crawl can
    # show the held-back purge running.
    def route_hubfail_hub(self):
        if 2 <= self.refetch_pass() <= 3:
            self.send_cut_headers()
        else:
            self.send_html(
                '\t<a href="child1.html">c1</a>\n\t<a href="child2.html">c2</a>\n'
            )

    def route_hubfail_child(self):
        n = int(self.path.rstrip(".html")[-1])
        self.send_raw(self.HUBFAIL_CHILD % n, "text/html")

    # Control: linked from the index on both passes, so it is never at risk.
    def route_hubfail_leaf(self):
        self.send_raw(b"<html><body><p>HUBFAIL-LEAF</p></body></html>", "text/html")

    # Control: the index stops linking it on the third crawl, the one where
    # nothing fails, so that crawl must purge it.
    def route_hubfail_gone(self):
        self.send_raw(b"<html><body><p>HUBFAIL-GONE</p></body></html>", "text/html")

    # Echo what httrack advertised, so a crawl can assert the header.
    def route_codec_ae(self):
        self.send_raw(
            b"<html><body><p>AE=%s</p></body></html>"
            % self.headers.get("Accept-Encoding", "").encode(),
            "text/html",
        )

    # --- MIME-type exclusion abort (issue #58) -----------------------------
    # A -mime:application/pdf filter must abort the transfer once the header
    # arrives, not download the whole body and discard it.
    def route_mimex_index(self):
        self.send_html(
            '\t<a href="blob.pdf">pdf</a>\n' '\t<a href="real.html">real</a>\n'
        )

    # 1 MB body: the fix aborts after the header, so httrack's "bytes received"
    # stays tiny; without it the engine reads the body and the count jumps.
    MIMEX_BLOB = b"%PDF-1.4\n" + b"\x00" * (1024 * 1024)

    def route_mimex_blob(self):
        self.send_raw(self.MIMEX_BLOB, "application/pdf")

    def route_mimex_real(self):
        self.send_raw(b"<html><body>real</body></html>", "text/html")

    # --- special chars in URLs across an update (issue #157) ---------------
    # A dotless, accented basename served as text/html (MediaWiki style). The
    # name the first crawl picks (.html) must survive the update pass.
    INTL_NAME = "Instalação_CVS_no_Ubuntu"

    def route_intl_index(self):
        self.send_html('\t<a href="%s">accented</a>\n' % self.INTL_NAME)

    def route_intl_page(self):
        self.send_raw(b"<html><body>accented page</body></html>\n", "text/html")

    # Raw non-ASCII href matrix (#180): each variant declares the page charset
    # differently; the PDF exists only at its exact UTF-8 path, so a
    # mis-decoded link 404s.
    CHARSET_CJK = "统计大数据服务平台.pdf"
    # variant -> (index Content-Type, <head> bytes, href bytes, pdf name)
    CHARSET_VARIANTS = {
        "header": ("text/html; charset=utf-8", b"", CHARSET_CJK.encode(), CHARSET_CJK),
        "meta5": (
            "text/html",
            b'<meta charset="utf-8">',
            CHARSET_CJK.encode(),
            CHARSET_CJK,
        ),
        "metaeq": (
            "text/html",
            b'<meta http-equiv="Content-Type" content="text/html; charset=utf-8">',
            CHARSET_CJK.encode(),
            CHARSET_CJK,
        ),
        "none": ("text/html", b"", CHARSET_CJK.encode(), CHARSET_CJK),
        "latin1hdr": (
            "text/html; charset=iso-8859-1",
            b"",
            CHARSET_CJK.encode(),
            CHARSET_CJK,
        ),
        # genuine latin-1 href: the charset conversion must still apply
        "latin1real": (
            "text/html; charset=iso-8859-1",
            b"",
            "café.pdf".encode("latin-1"),
            "café.pdf",
        ),
        # latin-1 declared by META only: the meta parser is load-bearing
        "metalatin1": (
            "text/html",
            b'<meta charset="iso-8859-1">',
            "déjà.pdf".encode("latin-1"),
            "déjà.pdf",
        ),
        # latin-1 bytes that form an overlong UTF-8 shape: strict validation
        # must still convert them
        "latin1ovl": (
            "text/html; charset=iso-8859-1",
            b"",
            "À¡x.pdf".encode("latin-1"),
            "À¡x.pdf",
        ),
        # header wins over meta: latin-1 href only resolves if iso-8859-1 is kept
        "priority": (
            "text/html; charset=iso-8859-1",
            b'<meta charset="utf-8">',
            "nuée.pdf".encode("latin-1"),
            "nuée.pdf",
        ),
        "preenc": ("text/html", b"", quote(CHARSET_CJK).encode(), CHARSET_CJK),
        "bom": ("text/html", b"", CHARSET_CJK.encode(), CHARSET_CJK),
    }

    def route_charset(self):
        path = unquote(urlsplit(self.path).path)
        parts = path.split("/")
        if path == "/charset/index.html":
            self.send_html(
                "".join(
                    '\t<a href="%s/index.html">%s</a>\n' % (v, v)
                    for v in self.CHARSET_VARIANTS
                )
            )
            return
        if len(parts) == 4 and parts[2] in self.CHARSET_VARIANTS:
            ctype, head, href, pdf = self.CHARSET_VARIANTS[parts[2]]
            if parts[3] == "index.html":
                body = (
                    b"<html><head>"
                    + head
                    + b'</head><body><a href="'
                    + href
                    + b'">doc</a></body></html>'
                )
                if parts[2] == "bom":
                    body = b"\xef\xbb\xbf" + body
                self.send_raw(body, ctype)
                return
            if parts[3] == pdf:
                self.send_raw(self.FAKE_PDF, "application/pdf")
                return
        self.send_response(404)
        self.send_header("Content-Length", "0")
        self.end_headers()

    # Character references in a query string (#854). CHARREF_LOG collects the
    # request-target the origin saw, plus the parameter count it parses out of
    # it. Per page: a reference the declared charset represents (é, あ) and one
    # it does not (€).
    CHARREF_QUERY = "?a=&euro;&b=&#8364;&c=&eacute;&d=x"
    CHARREF_MB_QUERY = "?a=&#12354;&c=&#12316;&b=&euro;&d=x"
    CHARREF_PAGES = {
        "none": ("text/html", CHARREF_QUERY),
        "utf8": ("text/html; charset=utf-8", CHARREF_QUERY),
        "sjis": ("text/html; charset=shift_jis", CHARREF_MB_QUERY),
        "jis": ("text/html; charset=iso-2022-jp", CHARREF_MB_QUERY),
    }

    def record_charref(self):
        log = os.environ.get("CHARREF_LOG")
        if log:
            fields = [f for f in urlsplit(self.path).query.split("&") if f]
            with open(log, "a") as fp:
                fp.write("%s\t%d\n" % (self.path, len(fields)))

    def route_charref(self):
        name = urlsplit(self.path).path[len("/charref/") :]
        variant = name[: -len(".html")] if name.endswith(".html") else ""
        if variant == "index":
            body = "".join(
                '<a href="%s.html">%s</a>' % (v, v) for v in self.CHARREF_PAGES
            )
            self.send_raw(("<html><body>%s</body></html>" % body).encode(), "text/html")
        elif variant in self.CHARREF_PAGES:
            ctype, query = self.CHARREF_PAGES[variant]
            body = '<a href="q%s.html%s">q</a>' % (variant, query)
            self.send_raw(("<html><body>%s</body></html>" % body).encode(), ctype)
        elif variant[1:] in self.CHARREF_PAGES:
            self.send_raw(b"<html><body>q</body></html>", "text/html")
        else:
            self.send_response(404)
            self.send_header("Content-Length", "0")
            self.end_headers()

    # variant -> (Content-Type, title bytes, body bytes); the gb2312 and l1decl
    # titles are valid UTF-8 as bytes, so only the declared charset decodes them
    TITLEENC_PAGES = {
        "u8": (
            "text/html",
            "Café-u8".encode(),
            '<img src="s.svg#café"><a href="né.txt">leaf</a>'.encode(),
        ),
        # genuine latin-1: the guess must still be applied
        "l1": (
            "text/html",
            "Café-l1".encode("latin-1"),
            b'<a href="l1.txt">leaf</a>',
        ),
        "declared": ("text/html; charset=utf-8", "Café-declared".encode(), b"x"),
        "gb2312": ("text/html; charset=gb2312", "图片".encode("gb2312") + b"-gb", b"x"),
        "l1decl": (
            "text/html; charset=iso-8859-1",
            "Ã©".encode("latin-1") + b"-l1d",
            b"x",
        ),
    }

    def route_titleenc(self):
        name = unquote(urlsplit(self.path).path)[len("/titleenc/") :]
        variant = name[: -len(".html")] if name.endswith(".html") else None
        if variant in self.TITLEENC_PAGES:
            ctype, title, body = self.TITLEENC_PAGES[variant]
            self.send_raw(
                b"<html><head><title>"
                + title
                + b"</title></head><body>"
                + body
                + b"</body></html>",
                ctype,
            )
            return
        if name == "s.svg":
            self.send_raw(
                (
                    '<svg xmlns="http://www.w3.org/2000/svg">'
                    '<symbol id="café"><rect/></symbol></svg>'
                ).encode(),
                "image/svg+xml",
            )
            return
        if name in ("né.txt", "l1.txt"):
            self.send_raw(b"leaf\n", "text/plain")
            return
        self.send_response(404)
        self.send_header("Content-Length", "0")
        self.end_headers()

    # resume / 416 loop (#206): the first GET stalls after a prefix so the crawl
    # can be interrupted (partial + temp-ref); every later request is 416.
    RESUME_PREFIX = b"PARTIAL-" + b"x" * 4096  # flushed before the stall
    RESUME_LEN = len(RESUME_PREFIX) + 4096  # declared length never delivered
    _resume_started = False

    def route_resume_index(self):
        self.send_html('\t<a href="blob.txt">blob</a>')

    def route_resume(self):
        counter = os.environ.get("RESUME_COUNTER")
        if counter:
            with open(counter, "a") as fp:
                fp.write("x")
        # First GET: stall mid-body so the crawl can be interrupted with a partial.
        if not Handler._resume_started:
            Handler._resume_started = True
            self.send_response(200)
            self.send_header("Content-Type", "image/png")
            self.send_header("Content-Length", str(self.RESUME_LEN))
            self.send_header("Accept-Ranges", "bytes")
            self.end_headers()
            if self.command != "HEAD":
                self.wfile.write(self.RESUME_PREFIX)
                self.wfile.flush()
                try:
                    while True:
                        time.sleep(3600)
                except OSError:
                    pass
            return
        self.send_response(416, "Requested Range Not Satisfiable")
        self.send_header("Content-Type", "image/png")
        self.send_header("Content-Range", "bytes */%d" % self.RESUME_LEN)
        self.send_header("Content-Length", "0")
        self.end_headers()

    # Always-stall endpoint for 72_watchdog-crawl: never finishes, so the harness
    # watchdog must reap it.
    def route_watchdog_stall(self):
        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", "1000000")  # never delivered
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(b"STALL")
            self.wfile.flush()
            try:
                while True:
                    time.sleep(3600)
            except OSError:
                pass

    # C7: stall the first GET (partial + temp-ref), then answer the resume's
    # Range with a bogus 304; httrack must drop the partial and refetch.
    RESUME304_BODY = b"C7DATA--" + bytes((i * 7 + 3) % 256 for i in range(8192))
    _resume304_started = False

    def route_resume304_index(self):
        self.send_html('\t<a href="blob.bin">blob</a>')

    def route_resume304(self):
        counter = os.environ.get("RESUME304_COUNTER")
        if counter:
            with open(counter, "a") as fp:
                fp.write("x")
        rng = self.headers.get("Range")
        if not Handler._resume304_started:
            Handler._resume304_started = True
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(len(self.RESUME304_BODY)))
            self.send_header("Accept-Ranges", "bytes")
            self.send_header("Last-Modified", BIG_LASTMOD)
            self.send_header("ETag", '"c7"')
            self.end_headers()
            if self.command != "HEAD":
                self.wfile.write(self.RESUME304_BODY[:4096])
                self.wfile.flush()
                try:
                    while True:
                        time.sleep(3600)
                except OSError:
                    pass
            return
        if rng is not None:  # resume request: bogus out-of-protocol 304
            mark = os.environ.get("RESUME304_MARK")
            if mark:
                with open(mark, "a") as fp:
                    fp.write("z")
            self.send_response(304)
            self.send_header("ETag", '"c7"')
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        # Range-less refetch after the partial is dropped: whole file.
        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(len(self.RESUME304_BODY)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(self.RESUME304_BODY)

    # 206 resume must honor the server's Content-Range, not the offset we asked
    # for (#198): a server resuming a few bytes *before* the request must not
    # leave httrack duplicating the overlap onto the partial. flaky.bin
    # interrupts once then resumes OVERLAP_EARLY bytes early; full.bin serves
    # the identical bytes in one shot, so the test can compare the two.
    OVERLAP_BLOB = b"%PDF-1.4\n" + bytes((i * 37 + 11) % 256 for i in range(8000))
    OVERLAP_EARLY = 8
    OVERLAP_PREFIX_LEN = 4000  # flushed before the stall
    _overlap_started = False

    def route_overlap_index(self):
        self.send_html('\t<a href="flaky.bin">flaky</a>\n\t<a href="full.bin">full</a>')

    def route_overlap_full(self):
        self.send_raw(self.OVERLAP_BLOB, "application/octet-stream")

    def route_overlap(self):
        counter = os.environ.get("OVERLAP_COUNTER")
        if counter:
            with open(counter, "a") as fp:
                fp.write("x")
        blob = self.OVERLAP_BLOB
        rng = self.headers.get("Range")
        # First GET: stream a prefix then stall, so the crawl can be interrupted
        # mid-body (partial + temp-ref on disk).
        if rng is None and not Handler._overlap_started:
            Handler._overlap_started = True
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(len(blob)))
            self.send_header("Accept-Ranges", "bytes")
            self.end_headers()
            if self.command != "HEAD":
                self.wfile.write(blob[: self.OVERLAP_PREFIX_LEN])
                self.wfile.flush()
                try:
                    while True:
                        time.sleep(3600)
                except OSError:
                    pass
            return
        if rng is None:  # no resume request: serve the whole file
            return self.route_overlap_full()
        # Resume: honor the Range, but back up OVERLAP_EARLY bytes.
        start = (
            int(rng[len("bytes=") :].split("-")[0]) if rng.startswith("bytes=") else 0
        )
        start = max(0, start - self.OVERLAP_EARLY)
        # Signal that the resume Range -> 206 path actually fired, so the test
        # can prove it was exercised (not a silent full re-download).
        resumed = os.environ.get("OVERLAP_RESUMED")
        if resumed:
            with open(resumed, "a") as fp:
                fp.write("x")
        part = blob[start:]
        self.send_response(206, "Partial Content")
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(len(part)))
        self.send_header(
            "Content-Range", "bytes %d-%d/%d" % (start, len(blob) - 1, len(blob))
        )
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(part)

    # C2: a resume answered with a 206 whose Content-Range end is INT64_MAX would
    # sign-overflow the crange+1 range check (UBSan abort). Stall first (partial +
    # ref), then answer the resume Range with that hostile 206; httrack must reject
    # the range and refetch, never overflow.
    CRANGE206_BODY = b"CR206DAT" + bytes((i * 5 + 1) % 256 for i in range(6000))
    _crange206_started = False

    def route_crange206_index(self):
        self.send_html('\t<a href="blob.bin">blob</a>')

    def route_crange206(self):
        rng = self.headers.get("Range")
        if not Handler._crange206_started:
            Handler._crange206_started = True
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(len(self.CRANGE206_BODY)))
            self.send_header("Accept-Ranges", "bytes")
            self.end_headers()
            if self.command != "HEAD":
                self.wfile.write(self.CRANGE206_BODY[:3000])
                self.wfile.flush()
                try:
                    while True:
                        time.sleep(3600)
                except OSError:
                    pass
            return
        if rng is not None:  # resume: hostile 206, Content-Range end = INT64_MAX
            self.send_response(206, "Partial Content")
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(len(self.CRANGE206_BODY)))
            self.send_header("Content-Range", "bytes 0-9223372036854775807/1")
            self.end_headers()
            if self.command != "HEAD":
                self.wfile.write(self.CRANGE206_BODY)
            return
        # range-less refetch after the bad range is rejected: whole file.
        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(len(self.CRANGE206_BODY)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(self.CRANGE206_BODY)

    # C2 memory branch: a resume answered with a 206 that lies text/html (so the
    # resume buffers in memory) plus a matching INT64_MAX Content-Length would
    # overflow the buffer-size add. Stall first, then send that hostile 206.
    _crange206mem_started = False

    def route_crange206mem_index(self):
        self.send_html('\t<a href="blob.bin">blob</a>')

    def route_crange206mem(self):
        rng = self.headers.get("Range")
        if not Handler._crange206mem_started:
            Handler._crange206mem_started = True
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(len(self.CRANGE206_BODY)))
            self.send_header("Accept-Ranges", "bytes")
            self.end_headers()
            if self.command != "HEAD":
                self.wfile.write(self.CRANGE206_BODY[:3000])
                self.wfile.flush()
                try:
                    while True:
                        time.sleep(3600)
                except OSError:
                    pass
            return
        if rng is not None:  # resume: text/html + matching INT64_MAX Content-Length
            self.send_response(206, "Partial Content")
            self.send_header("Content-Type", "text/html")
            self.send_header("Content-Length", "9223372036854775807")
            self.send_header(
                "Content-Range", "bytes 0-9223372036854775806/9223372036854775807"
            )
            self.end_headers()
            return  # the overflow is computed before any body read
        # range-less refetch after the resume is rejected: whole file.
        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(len(self.CRANGE206_BODY)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(self.CRANGE206_BODY)

    # #581/#1052: an interrupted download leaves a partial plus a temp-ref, and
    # the resume is answered with a Content-Range starting past what is on disk.
    # The "loop" variant keeps answering unusably even without a Range, so only
    # the one-shot latch can end the crawl.
    RESUME206_BODY = b"RSM206DT" + bytes((i * 7 + 3) % 256 for i in range(6000))
    RESUME206_HAVE = 3000
    _resume206_started = set()

    def _resume206_stall(self):
        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(len(self.RESUME206_BODY)))
        self.send_header("Accept-Ranges", "bytes")
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(self.RESUME206_BODY[: self.RESUME206_HAVE])
            self.wfile.flush()
            try:
                while True:
                    time.sleep(3600)
            except OSError:
                pass

    def _resume206_unusable(self):
        start = self.RESUME206_HAVE + 2000  # past what is on disk
        body = self.RESUME206_BODY[start:]
        self.send_response(206, "Partial Content")
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(len(body)))
        self.send_header(
            "Content-Range",
            "bytes %d-%d/%d"
            % (start, len(self.RESUME206_BODY) - 1, len(self.RESUME206_BODY)),
        )
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    def _resume206_whole(self):
        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(len(self.RESUME206_BODY)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(self.RESUME206_BODY)

    def route_resume206_index(self):
        self.send_html('\t<a href="blob.bin">blob</a>')

    def route_resume206(self):
        if "plain" not in Handler._resume206_started:
            Handler._resume206_started.add("plain")
            return self._resume206_stall()
        if self.headers.get("Range") is not None:
            return self._resume206_unusable()
        return self._resume206_whole()

    def route_resume206loop_index(self):
        self.send_html('\t<a href="blob.bin">blob</a>')

    def route_resume206loop(self):
        if "loop" not in Handler._resume206_started:
            Handler._resume206_started.add("loop")
            return self._resume206_stall()
        return self._resume206_unusable()  # unusable forever, Range or not

    # An unusable 206 alternating with a redirect to a case-different alias of
    # the same path: the hop re-records the link, and a latch lost there buys
    # another free restart every time round (#1052).
    _alias206_count = 0

    def route_alias206_index(self):
        self.send_html('\t<a href="Blob.bin">blob</a>')

    def _alias206(self, other):
        Handler._alias206_count += 1
        if Handler._alias206_count % 2:
            return self._resume206_unusable()
        self.send_response(301, "Moved Permanently")
        self.send_header("Location", "/alias206/" + other)
        self.send_header("Content-Length", "0")
        self.end_headers()

    def route_alias206_upper(self):
        self._alias206("blob.bin")

    def route_alias206_lower(self):
        self._alias206("Blob.bin")

    # error pages / 0-byte files (#17): -o0 ("no error pages") must keep 4xx/5xx
    # bodies off disk; a genuine 0-byte 200 is a valid file and stays.
    def route_errpage_index(self):
        self.send_html(
            '\t<a href="good.html">good</a>\n'
            '\t<a href="missing.html">missing</a>\n'
            '\t<a href="empty.html">empty</a>\n'
        )

    def route_errpage_good(self):
        self.send_raw(b"<html><body>good page</body></html>\n", "text/html")

    def route_errpage_missing(self):
        self.send_html("\t404 error body", status=404, extra_status="Not Found")

    def route_errpage_empty(self):
        self.send_raw(b"", "text/html")

    # broken Content-Length (#32/#41): declared size != bytes sent. httrack
    # warns "incomplete transfer" and skips the cache unless -%B.
    def route_size_index(self):
        self.send_html('\t<a href="oversize.bin">over</a>\n')

    def route_size_oversize(self):
        body = b"A" * 100
        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(len(body) - 2))  # lie: too short
        self.send_header("Connection", "close")
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    def route_chunked_index(self):
        self.send_html('\t<a href="page.html">chunked</a>\n')

    def route_chunked_page(self):
        # Transfer-Encoding: chunked over many small chunks: drives the engine's
        # chunk automaton (htsback.c). The mirrored file must equal the joined
        # chunk bodies, so the 2GB in-RAM cap doesn't fire on ordinary traffic.
        blob = big_html("chunked", "<p>" + "chunk-body " * 300 + "</p>")
        self.protocol_version = "HTTP/1.1"
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Transfer-Encoding", "chunked")
        self.send_header("Connection", "close")
        self.end_headers()
        if self.command == "HEAD":
            return
        step = 64
        for off in range(0, len(blob), step):
            piece = blob[off : off + step]
            self.wfile.write(b"%X\r\n" % len(piece) + piece + b"\r\n")
        self.wfile.write(b"0\r\n\r\n")

    # #840: a chunked stream cut before its terminating zero-length chunk.
    CHUNKTRUNC_V1 = b"<html><body><p>CHUNKTRUNC-PAGE-V1</p></body></html>"
    CHUNKTRUNC_V2 = b"<html><body><p>CHUNKTRUNC-PAGE-V2</p></body></html>"

    CHUNKTRUNC_BIN_V1 = b"CHUNKTRUNC-BIN-V1\n" + b"\x07\x08\x09\xfe" * 8192
    CHUNKTRUNC_BIN_V2 = b"CHUNKTRUNC-BIN-V2\n" + b"\x17\x18\x19\xee" * 8192

    def route_chunktrunc_index(self):
        self.send_html(
            '\t<a href="page.html">page</a>\n'
            '\t<a href="always.html">always</a>\n'
            '\t<a href="stay.html">stay</a>\n'
            '\t<a href="file.bin">file</a>\n'
            '\t<a href="always.bin">alwaysbin</a>\n'
            '\t<a href="hostile.html">hostile</a>\n'
            '\t<a href="reset.bin">reset</a>\n'
        )

    def send_chunked(self, body, terminate, ctype="text/html; charset=utf-8"):
        self.protocol_version = "HTTP/1.1"
        self.send_response(200)
        self.send_header("Content-Type", ctype)
        self.send_header("Transfer-Encoding", "chunked")
        self.send_header("Connection", "close")
        self.end_headers()
        if self.command == "HEAD":
            return
        try:
            self.wfile.write(b"%X\r\n" % len(body) + body + b"\r\n")
            if terminate:
                self.wfile.write(b"0\r\n\r\n")
            self.wfile.flush()
        except OSError:
            pass
        self.close_connection = True

    def route_chunktrunc_page(self):
        if self.refetch_pass() == 1:
            self.send_chunked(self.CHUNKTRUNC_V1, True)
        else:
            self.send_chunked(self.CHUNKTRUNC_V2, False)

    # Same over the direct-to-disk path: the update pass delivers half the new
    # body and never terminates it.
    def route_chunktrunc_file(self):
        octet = "application/octet-stream"
        if self.refetch_pass() == 1:
            self.send_chunked(self.CHUNKTRUNC_BIN_V1, True, octet)
        else:
            half = self.CHUNKTRUNC_BIN_V2[: len(self.CHUNKTRUNC_BIN_V2) // 2]
            self.send_chunked(half, False, octet)

    # Truncated on every pass, so a first crawl has nothing good to fall back on.
    def route_chunktrunc_always(self):
        self.send_chunked(b"<html><body><p>CHUNKTRUNC-ALWAYS</p></body></html>", False)

    def route_chunktrunc_alwaysbin(self):
        self.send_chunked(
            b"CHUNKTRUNC-ALWAYSBIN\n" + b"\x27\x28\x29\xde" * 1000,
            False,
            "application/octet-stream",
        )

    # Control: terminated on both passes, so a normal --update still lands.
    def route_chunktrunc_stay(self):
        v = 1 if self.refetch_pass() == 1 else 2
        self.send_chunked(b"<html><body><p>CHUNKSTAY-V%d</p></body></html>" % v, True)

    # The update pass declares a chunk of 0x80000000, which sscanf("%x") lands in
    # an int as INT_MIN: it must not read as the terminating chunk, and the sum
    # it drives negative must not read as a complete body either.
    def route_chunktrunc_hostile(self):
        if self.refetch_pass() == 1:
            self.send_chunked(b"<html><body><p>CHUNKHOSTILE-V1</p></body></html>", True)
            return
        self.protocol_version = "HTTP/1.1"
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Transfer-Encoding", "chunked")
        self.send_header("Connection", "close")
        self.end_headers()
        if self.command == "HEAD":
            return
        try:
            self.wfile.write(b"80000000\r\n")
            self.wfile.flush()
        except OSError:
            pass
        self.close_connection = True

    # #855: a trailer section after the terminating zero-length chunk.
    def send_chunked_trailed(self, body, trailers, ctype="text/html; charset=utf-8"):
        self.protocol_version = "HTTP/1.1"
        self.send_response(200)
        self.send_header("Content-Type", ctype)
        self.send_header("Transfer-Encoding", "chunked")
        self.send_header("Connection", "close")
        self.end_headers()
        if self.command == "HEAD":
            return
        try:
            half = len(body) // 2
            for piece in (body[:half], body[half:]):
                self.wfile.write(b"%X\r\n" % len(piece) + piece + b"\r\n")
            self.wfile.write(
                b"0\r\n" + b"".join(t + b"\r\n" for t in trailers) + b"\r\n"
            )
            self.wfile.flush()
        except OSError:
            pass
        self.close_connection = True

    def route_chunktrail_index(self):
        self.send_html(
            '\t<a href="one.html">one</a>\n'
            '\t<a href="many.html">many</a>\n'
            '\t<a href="none.html">none</a>\n'
            '\t<a href="huge.html">huge</a>\n'
            '\t<a href="bogus.html">bogus</a>\n'
            '\t<a href="eof.html">eof</a>\n'
            '\t<a href="file.bin">file</a>\n'
        )

    def route_chunktrail_one(self):
        self.send_chunked_trailed(
            b"<html><body><p>CHUNKTRAIL-ONE</p></body></html>", [b"X-Foo: bar"]
        )

    # Spans several reads: longer than the reader's 256 bytes per call.
    def route_chunktrail_many(self):
        self.send_chunked_trailed(
            b"<html><body><p>CHUNKTRAIL-MANY</p></body></html>",
            [b"X-Field-%02d: %s" % (n, b"v" * 40) for n in range(20)],
        )

    # Control: same body, no trailer section.
    def route_chunktrail_none(self):
        self.send_chunked_trailed(
            b"<html><body><p>CHUNKTRAIL-NONE</p></body></html>", []
        )

    # Past HTS_LINE_BLOCK_SIZE: the discard stays bounded, so the transfer drops.
    def route_chunktrail_huge(self):
        self.send_chunked_trailed(
            b"<html><body><p>CHUNKTRAIL-HUGE</p></body></html>",
            [b"X-Bloat-%04d: %s" % (n, b"w" * 100) for n in range(160)],
        )

    # Only the terminating chunk opens the section, so junk in a data chunk's
    # own CRLF stays a framing error.
    def route_chunktrail_bogus(self):
        self.protocol_version = "HTTP/1.1"
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Transfer-Encoding", "chunked")
        self.send_header("Connection", "close")
        self.end_headers()
        if self.command == "HEAD":
            return
        try:
            body = b"<html><body><p>CHUNKTRAIL-BOGUS</p></body></html>"
            self.wfile.write(b"%X\r\n" % len(body) + body + b"X-Foo: bar\r\n")
            self.wfile.write(b"0\r\n\r\n")
            self.wfile.flush()
        except OSError:
            pass
        self.close_connection = True

    # Body complete, then EOF before the blank line closing the section. The
    # terminating chunk already arrived, so the payload stands.
    def route_chunktrail_eof(self):
        self.protocol_version = "HTTP/1.1"
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Transfer-Encoding", "chunked")
        self.send_header("Connection", "close")
        self.end_headers()
        if self.command == "HEAD":
            return
        try:
            body = b"<html><body><p>CHUNKTRAIL-EOF</p></body></html>"
            self.wfile.write(b"%X\r\n" % len(body) + body + b"\r\n")
            self.wfile.write(b"0\r\nX-Checksum: deadbeef\r\n")
            self.wfile.flush()
        except OSError:
            pass
        self.close_connection = True

    # The direct-to-disk path, where the body never sits in memory.
    def route_chunktrail_file(self):
        self.send_chunked_trailed(
            b"CHUNKTRAIL-BIN\n" + b"\x41\x42\x43\xfd" * 4096,
            [b"X-Checksum: 0badc0de", b"X-Done: 1"],
            "application/octet-stream",
        )

    # Aborts the chunked body with an RST, so the read fails rather than seeing a
    # clean EOF and the transfer is already in error before the framing check.
    def route_chunktrunc_reset(self):
        self.protocol_version = "HTTP/1.1"
        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Transfer-Encoding", "chunked")
        self.send_header("Connection", "close")
        self.end_headers()
        if self.command == "HEAD":
            return
        try:
            body = b"CHUNKRESET\n" + b"\x31\x32\x33\xcd" * 4096
            self.wfile.write(b"%X\r\n" % len(body) + body + b"\r\n")
            self.wfile.flush()
            time.sleep(0.5)  # let the client consume the chunk first
            self.connection.setsockopt(
                socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0)
            )
            self.connection.close()
        except OSError:
            pass
        self.close_connection = True

    # Content-Disposition naming: the attachment filename replaces the
    # URL-derived name; path components in it are stripped (RFC 2616).
    CDISPO_NAMES = {
        "/cdispo/fetch.php": "report.pdf",
        "/cdispo/evil.php": "../../evil.pdf",
    }

    def route_cdispo_index(self):
        self.send_html(
            '\t<a href="fetch.php">report</a>\n' '\t<a href="evil.php">evil</a>\n'
        )

    def route_cdispo(self):
        filename = self.CDISPO_NAMES[urlsplit(self.path).path]
        cdispo = 'attachment; filename="%s"' % filename
        self.send_raw(
            self.FAKE_PDF,
            "application/pdf",
            extra_headers=[("Content-Disposition", cdispo)],
        )

    # 302 whose Location carries a #fragment (#204): the fragment is a UA anchor
    # that must be dropped before the target is fetched. A leaked '#' reaches the
    # strict-server guard below and 400s.
    def route_redir_index(self):
        self.send_html('\t<a href="go.php">go</a>')

    def route_redir_go(self):
        self.send_response(302, "Found")
        self.send_header("Location", "target.html#section")
        self.send_header("Content-Length", "0")
        self.end_headers()

    def route_redir_target(self):
        self.send_raw(b"<html><body>redirect target</body></html>\n", "text/html")

    # A Location past the header-line read buffer (#1291): the engine only ever
    # sees a cut value, so it must drop the header rather than follow a target
    # this server never named.
    def route_longloc_index(self):
        self.send_html('\t<a href="go.php">go</a>')

    def route_longloc_go(self):
        # absolute: a relative target is capped at HTS_URLMAXSIZE by
        # ident_url_relatif long before the gate under test sees it
        base = "http://127.0.0.1:%d/longloc/" % self.server.server_address[1]
        self.send_response(302, "Found")
        self.send_header("Location", base + "target.html?q=" + "a" * 2400)
        self.send_header("Content-Length", "0")
        self.end_headers()

    def route_longloc_target(self):
        self.send_raw(b"<html><body>long-location target</body></html>\n", "text/html")

    # same shape with a Location the reader takes whole: the control proving
    # the fixture only refuses because the header outran the read
    def route_shortloc_index(self):
        self.send_html('\t<a href="go.php">go</a>')

    def route_shortloc_go(self):
        base = "http://127.0.0.1:%d/shortloc/" % self.server.server_address[1]
        self.send_response(302, "Found")
        self.send_header("Location", base + "target.html?q=" + "a" * 40)
        self.send_header("Content-Length", "0")
        self.end_headers()

    def route_shortloc_target(self):
        self.send_raw(b"<html><body>short-location target</body></html>\n", "text/html")

    # --- /mini304/: tiny fully-cacheable site (an update gets only 304s) ---
    def route_mini304_index(self):
        self.big_send(
            b'<html><body>\n\t<a href="page.html">page</a>\n</body></html>\n',
            "text/html",
        )

    def route_mini304_page(self):
        self.big_send(b"<html><body>tiny cacheable page</body></html>\n", "text/html")

    # --- /bakname/: #774 — a mirrored file named like a re-fetch backup ----
    # a.bin gets a new body every pass, so the update re-fetches it and takes a
    # backup. The sibling carries a validator, so it only revalidates and must
    # still be there afterwards. It is served under hts-tmp/, the directory the
    # backup lives in, so the crawl asks for the exact path the engine writes.
    BAKNAME_SIB = b"BAKNAME-SIBLING\n" + b"\x41\x42\x43\x44" * 512

    def route_bakname_index(self):
        self.send_html(
            '\t<a href="a.bin">a</a>\n' '\t<a href="hts-tmp/a.bin.bak">bak</a>\n'
        )

    def route_bakname_main(self):
        v = 1 if self.refetch_pass() == 1 else 2
        self.send_raw(
            b"BAKNAME-MAIN-V%d\n" % v + b"\x31\x32\x33\x34" * 512,
            "application/octet-stream",
        )

    def route_bakname_sibling(self):
        if self.headers.get("If-Modified-Since") or self.headers.get("If-None-Match"):
            self.send_response(304)
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Last-Modified", BIG_LASTMOD)
        self.send_header("Content-Length", str(len(self.BAKNAME_SIB)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(self.BAKNAME_SIB)

    # --- /tmpspace/: #842 — same collision, reached through the trailing-space
    # bypass of the reserved-segment escape. "hts-tmp%20" was escaped by nothing
    # and then had its space stripped, so the sibling landed on the backup's own
    # name on shipped defaults.
    def route_tmpspace_index(self):
        self.send_html(
            '\t<a href="a.bin">a</a>\n' '\t<a href="hts-tmp%20/a.bin.bak">bak</a>\n'
        )

    # --- /errmask/: issue #176 — a page that 200'd on the first crawl but 403s
    # on the update fetch must keep its good copy, not be overwritten nor purged.
    ERRMASK_GOOD = b"KEEP" + b"." * 1020  # 1024 B distinctive non-HTML body
    ERRMASK_ERR = b"<html><body>error 403</body></html>\n"

    def route_errmask_index(self):
        self.send_html(
            '\t<a href="keep.dat">keep</a>\n\t<a href="empty.dat">empty</a>\n'
        )

    def route_errmask_keep(self):
        # First crawl (no validator) gets the 1024 B body + Last-Modified; the
        # update sends a conditional and gets a 403 error page.
        if self.headers.get("If-Modified-Since") or self.headers.get("If-None-Match"):
            self.send_response(403, "Forbidden")
            self.send_header("Content-Type", "text/html")
            self.send_header("Content-Length", str(len(self.ERRMASK_ERR)))
            self.end_headers()
            if self.command != "HEAD":
                self.wfile.write(self.ERRMASK_ERR)
            return
        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Last-Modified", BIG_LASTMOD)
        self.send_header("Content-Length", str(len(self.ERRMASK_GOOD)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(self.ERRMASK_GOOD)

    def route_errmask_empty(self):
        # Same masking shape as keep.dat, but a genuinely zero-length body: an
        # engine-forced revisit must still digest and archive it, not treat
        # it as a missing-crypto case (#839).
        if self.headers.get("If-Modified-Since") or self.headers.get("If-None-Match"):
            self.send_response(403, "Forbidden")
            self.send_header("Content-Type", "text/html")
            self.send_header("Content-Length", str(len(self.ERRMASK_ERR)))
            self.end_headers()
            if self.command != "HEAD":
                self.wfile.write(self.ERRMASK_ERR)
            return
        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Last-Modified", BIG_LASTMOD)
        self.send_header("Content-Length", "0")
        self.end_headers()

    # --- delayed-type degenerate paths (issues #5/#107) --------------------
    def route_delayed_index(self):
        self.send_html(
            '\t<a href="noloc.php">noloc</a>\n'
            '\t<a href="selfloop.php">selfloop</a>\n'
            '\t<a href="chain1.php">chain</a>\n'
            '\t<a href="redir.php">redir</a>\n'
            '\t<a href="notype.bin">notype</a>\n'
            '\t<a href="empty.php">empty</a>\n'
        )

    def send_redirect(self, location):
        self.send_response(302, "Found")
        if location is not None:
            self.send_header("Location", location)
        self.send_header("Content-Length", "0")
        self.end_headers()

    def route_delayed_noloc(self):
        self.send_redirect(None)  # 302 without Location: name never resolves

    def route_delayed_selfloop(self):
        self.send_redirect("selfloop.php")

    def route_hostalias_moved(self):
        # 301 to another hostname of this site: what --host-alias is really for
        self.send_response(301, "Moved Permanently")
        self.send_header("Location", "http://alias.example.invalid/hostalias/c.html")
        self.send_header("Content-Length", "0")
        self.end_headers()

    def route_hostalias_noext(self):
        # no extension, so the type probe re-requests this URL: it must already
        # carry the canonical host by then
        self.send_raw(
            b"<html><body>type known only from the probe</body></html>",
            "text/html",
        )

    def route_hostalias_sitemap(self):
        # the sitemap seeder records a link without naming it first
        self.send_raw(
            '<?xml version="1.0" encoding="UTF-8"?>\n'
            '<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">'
            "<url><loc>http://alias.example.invalid/hostalias/e.html</loc></url>"
            "</urlset>\n".encode(),
            "application/xml",
        )

    def route_delayed_chain(self):
        # chain1..chain9: one more hop than the type-check redirect budget
        n = int(urlsplit(self.path).path.rsplit("chain", 1)[1].split(".")[0])
        if n < 9:
            self.send_redirect("chain%d.php" % (n + 1))
        else:
            self.send_raw(self.FAKE_PDF, "application/pdf")

    def route_delayed_redir(self):
        self.send_redirect("real.pdf")

    def route_delayed_realpdf(self):
        self.send_raw(self.FAKE_PDF, "application/pdf")

    def route_delayed_notype(self):
        self.send_raw(self.FAKE_PDF, None)

    def route_delayed_empty(self):
        self.send_raw(b"", "text/html")  # 200 + Content-Length: 0

    # --- /cookiewall/ (#15): a self-redirect that only sets a cookie is a
    # consent wall; httrack must replay the cookie and fetch the real page.
    WALL_MARK = b"REAL-CONTENT-BEHIND-COOKIE-WALL"

    def route_cookiewall_index(self):
        self.send_html('\t<a href="wall.php">wall</a>')

    def route_cookiewall_wall(self):
        self._cookiewall_reply("wall.php")

    # Known-extension twin: .html so the type is not delayed-resolved.
    def route_cookiewall2_index(self):
        self.send_html('\t<a href="wall.html">wall</a>')

    def route_cookiewall2_wall(self):
        self._cookiewall_reply("wall.html")

    def _cookiewall_reply(self, location):
        if self.request_cookies().get("gate") == "1":
            self.send_raw(
                b"<html><body>" + self.WALL_MARK + b"</body></html>\n", "text/html"
            )
        else:
            self._wall_redirect(location, "gate=1; Path=/")

    # No-cookie self-redirect: the jar never changes, so httrack must give up at
    # once rather than re-fetch (proves the cookie-wall retry stays gated on #15).
    def route_cookiewall3_index(self):
        self.send_html('\t<a href="wall.php">wall</a>')

    def route_cookiewall3_wall(self):
        self._wall_redirect("wall.php", None)

    # Ever-changing cookie: every hit sets a fresh value, so the jar keeps
    # changing; httrack must stop at the loops<7 cap, not spin forever.
    def route_cookiewall4_index(self):
        self.send_html('\t<a href="wall.php">wall</a>')

    def route_cookiewall4_wall(self):
        nonce = int(self.request_cookies().get("gate", "0")) + 1
        self._wall_redirect("wall.php", f"gate={nonce}; Path=/")

    def _wall_redirect(self, location, set_cookie):
        self.send_response(302, "Found")
        self.send_header("Location", location)
        if set_cookie is not None:
            self.send_header("Set-Cookie", set_cookie)
        self.send_header("Content-Length", "0")
        self.end_headers()

    # -E time-limit (#481): pages that trickle far longer than any -E budget,
    # so only an engine-side abort can end the crawl.
    TRICKLE_SECONDS = 60

    def send_bin_index(self):
        """Index page linking p0.bin..p7.bin (shared by trickle and bigfiles)."""
        self.send_html(
            "".join('\t<a href="p%d.bin">p%d</a>\n' % (i, i) for i in range(8))
        )

    def route_trickle_index(self):
        self.send_bin_index()

    # #1109: slow.html outlasts any small -E budget and is the only source of
    # the leaf links, which a stopped parser then drops.
    ABORTPURGE_SECONDS = 5

    def route_abortpurge_index(self):
        self.send_html('\t<a href="slow.html">slow</a>\n')

    def route_abortpurge_slow(self):
        time.sleep(self.ABORTPURGE_SECONDS)
        self.send_html(
            "".join('\t<a href="x%d.html">x%d</a>\n' % (i, i) for i in range(6))
        )

    def route_abortpurge_leaf(self):
        self.send_html("\t<p>leaf</p>\n")

    # #97: a path long enough that the CLI in-progress column truncates it at 80
    # columns but not at 200. Its own index: /trickle/ is asserted on elsewhere.
    def route_deeptrickle_index(self):
        self.send_html('\t<a href="p0.bin">p0</a>\n\t<a href="p1.bin">p1</a>\n')

    def route_trickle_page(self):
        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(2 * self.TRICKLE_SECONDS))
        self.end_headers()
        if self.command == "HEAD":
            return
        try:
            for _ in range(self.TRICKLE_SECONDS):
                self.wfile.write(b"xy")
                self.wfile.flush()
                time.sleep(1.0)
        except OSError:
            pass

    # #973: quoted, so the link parser keeps the < and > (only an unquoted > ends
    # a link) and the whole run reaches the progress panel.
    XSS_NAME = "p0<img src=x onerror=alert(1)>'quote.bin"
    # The panel splits the URL at the last slash, so only a directory reaches its
    # name column. Short, because the engine elides that column past 40 chars.
    XSS_DIR = "d0<i'>"

    def route_xssjob_index(self):
        self.send_html('\t<a href="%s/%s">job</a>\n' % (self.XSS_DIR, self.XSS_NAME))

    # #483: trickled .bin pages so the -E stop lands in the type waiter's
    # unlock-to-patch window with body bytes pending.
    def route_dcancel_index(self):
        self.send_bin_index()

    def route_dcancel_page(self):
        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", "4096")
        self.end_headers()
        if self.command == "HEAD":
            return
        try:
            for _ in range(32):
                self.wfile.write(b"z" * 128)
                self.wfile.flush()
                time.sleep(0.05)
        except OSError:
            pass

    # -M byte cap (#77): large fast files so a crawl overruns -M immediately.
    BIGFILE_BYTES = 640 * 1024

    def route_bigfiles_index(self):
        self.send_bin_index()

    def route_bigfile(self):
        self.send_raw(b"x" * self.BIGFILE_BYTES, "application/octet-stream")

    # -M under a slow server (#77): p0 is a fast 640KB file that alone overruns
    # -M; p1..p3 trickle for a minute. The cap must abort those in-flight
    # transfers, not wait them out.
    def route_bigtrickle_index(self):
        self.send_html(
            "".join('\t<a href="p%d.bin">p%d</a>\n' % (i, i) for i in range(4))
        )

    # -M hard-abort must not destroy an already-complete file (#77 follow-up).
    # "fast.bin" alone overruns -M and completes; "slow.bin" transfers fully on
    # its first fetch (initial mirror) but stalls on every later fetch (the
    # --update re-fetch), so the -M abort tears it down mid-body. An engine that
    # truncates the good local copy on the aborted re-fetch loses data.
    slow_seen = 0

    def route_bigtrunc_index(self):
        self.send_html('\t<a href="fast.bin">fast</a>\n\t<a href="slow.bin">slow</a>\n')

    def route_bigtrunc_slow(self):
        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(self.BIGFILE_BYTES))
        self.end_headers()
        if self.command == "HEAD":
            return
        # Count body fetches only, so a stray HEAD can't shift which pass stalls.
        Handler.slow_seen += 1
        first = Handler.slow_seen == 1
        try:
            if first:
                self.wfile.write(b"x" * self.BIGFILE_BYTES)
                self.wfile.flush()
            else:
                self.wfile.write(b"x" * 4096)
                self.wfile.flush()
                for _ in range(120):
                    self.wfile.write(b"x")
                    self.wfile.flush()
                    time.sleep(1.0)
        except OSError:
            pass

    # -M received-volume cap (#520): links to large 404 bodies. httrack receives
    # each (HTS_TOTAL_RECV climbs) but saves none, so saved stays far below -M.
    def route_maxrecv_index(self):
        self.send_html(
            "".join('\t<a href="r%d.bin">r%d</a>\n' % (i, i) for i in range(16))
        )

    def route_maxrecv_404(self):
        body = b"x" * self.BIGFILE_BYTES
        self.send_response(404, "Not Found")
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    # --changes (#714). Every route answers 200 with no validators, so the
    # transfer signal alone would call the whole site changed on pass 2; only a
    # payload comparison can tell stable.html and stable.bin from moved.*.
    def route_changes_index(self):
        links = "".join(
            '\t<a href="%s">%s</a>\n' % (name, name)
            for name in (
                "stable.html",
                "moved.html",
                "stable.bin",
                "moved.bin",
                "redir.html",
                "flaky.bin",
                "coded.bin",
                "codedstable.bin",
                "sized.html",
                "reset.bin",
            )
        )
        seen = self.refetch_pass()
        if seen == 1:
            links += '\t<a href="doomed.html">doomed</a>\n'
        else:
            links += '\t<a href="fresh.html">fresh</a>\n'
        # transient.html appears on pass 2 only, so pass 3 has a deletion of
        # its own to purge.
        if seen == 2:
            links += '\t<a href="transient.html">transient</a>\n'
        self.send_html(links)

    def route_changes_stable(self):
        self.refetch_pass()
        self.send_raw(b"<html><body><p>CHANGES-STABLE</p></body></html>", "text/html")

    def route_changes_moved(self):
        pass1 = self.refetch_pass() == 1
        self.send_raw(
            b"<html><body><p>CHANGES-MOVED-V%d</p></body></html>" % (1 if pass1 else 2),
            "text/html",
        )

    def route_changes_stable_bin(self):
        self.refetch_pass()
        self.send_raw(
            b"CHANGES-STABLE-BIN\n" + b"\x00\x01\x02\xff" * 512,
            "application/octet-stream",
        )

    def route_changes_moved_bin(self):
        pass1 = self.refetch_pass() == 1
        self.send_raw(
            b"CHANGES-MOVED-BIN-V%d\n" % (1 if pass1 else 2)
            + b"\x03\x02\x01\xfe" * 512,
            "application/octet-stream",
        )

    def route_changes_doomed(self):
        self.refetch_pass()
        self.send_raw(b"<html><body><p>CHANGES-DOOMED</p></body></html>", "text/html")

    def route_changes_fresh(self):
        self.refetch_pass()
        self.send_raw(b"<html><body><p>CHANGES-FRESH</p></body></html>", "text/html")

    def route_changes_redir(self):
        self.send_response(302)
        self.send_header("Location", "/changes/redirtarget.html")
        self.send_header("Content-Length", "0")
        self.end_headers()

    def route_changes_redirtarget(self):
        self.refetch_pass()
        self.send_raw(b"<html><body><p>CHANGES-REDIR</p></body></html>", "text/html")

    # Direct-to-disk, and the first body fetch of each pass dies mid-transfer:
    # the retry notifies the same file a second time, so the accumulator has to
    # keep the pre-run sample from the first notify.
    FLAKY = b"CHANGES-FLAKY\n" + b"\x07\x06\x05\x04" * 512

    # A page whose payload never changes, linking through a redirect whose
    # target is renamed between passes: the rewritten link makes the file on
    # disk change length while the payload is byte-identical. Only a payload
    # comparison can call it unchanged.
    SIZED_SHORT = "s.html"
    SIZED_LONG = "s" * 40 + ".html"

    def route_changes_sized(self):
        self.refetch_pass()
        self.send_raw(
            b'<html><body><a href="sizedredir.html">t</a></body></html>',
            "text/html",
        )

    def route_changes_sizedredir(self):
        target = self.SIZED_SHORT if self.refetch_pass() == 1 else self.SIZED_LONG
        self.send_response(302)
        self.send_header("Location", "/changes/" + target)
        self.send_header("Content-Length", "0")
        self.end_headers()

    def route_changes_sizedtarget(self):
        self.refetch_pass()
        self.send_raw(b"<html><body><p>SIZED-TARGET</p></body></html>", "text/html")

    # Content-Encoding on a direct-to-disk body: the decoded temp is renamed
    # over the mirror file, so the previous copy has to be sampled before the
    # rename. Same length on both passes, so only a digest can tell them apart.
    def route_changes_coded(self):
        pass1 = self.refetch_pass() == 1
        body = b"CHANGES-CODED-V%d\n" % (1 if pass1 else 2) + b"\x11\x22" * 1024
        self.send_coded(self.gzipped(body), "application/octet-stream")

    # Control: same coding, same bytes on both passes.
    def route_changes_coded_stable(self):
        self.refetch_pass()
        body = b"CHANGES-CODED-STABLE\n" + b"\x33\x44" * 1024
        self.send_coded(self.gzipped(body), "application/octet-stream")

    def route_changes_flaky(self):
        if self.refetch_pass() % 2 == 1:
            self.send_truncated(self.FLAKY, "application/octet-stream")
        else:
            self.send_raw(self.FLAKY, "application/octet-stream")

    def route_changes_transient(self):
        self.refetch_pass()
        self.send_raw(
            b"<html><body><p>CHANGES-TRANSIENT</p></body></html>", "text/html"
        )

    # Answers once, then hangs up before sending anything on every later
    # attempt, retries included: nothing is written, so the file never reaches
    # new.lst while its mirrored copy stays intact.
    RESET = b"CHANGES-RESET\n" + b"\x21\x22\x23\x24" * 512

    def route_changes_reset(self):
        if self.refetch_pass() == 1:
            self.send_raw(self.RESET, "application/octet-stream")
        else:
            self.close_connection = True
            self.connection.close()

    # A second, independent mirror crawled with the cache off: nothing but the
    # bytes on disk can tell stable2 from ticker2, whose body differs every
    # fetch at a constant length.
    def route_changes2_index(self):
        self.refetch_pass()
        self.send_html('\t<a href="stable2.bin">s</a>\n\t<a href="ticker2.bin">t</a>\n')

    def route_changes2_stable(self):
        self.refetch_pass()
        self.send_raw(
            b"CHANGES2-STABLE-00\n" + b"\x55\xaa" * 512, "application/octet-stream"
        )

    def route_changes2_ticker(self):
        self.send_raw(
            b"CHANGES2-TICKER-%02d\n" % (self.refetch_pass() % 100) + b"\x55\xaa" * 512,
            "application/octet-stream",
        )

    # A --single-file mark in the charset, the one channel a body sweep cannot
    # see (#1069). It names pixel.svg, a real asset, so an expansion would show.
    SINGLEFILE_MARK_CHARSET = "pixel.svg#!0011223344556677.-.9"

    def route_singlefile_mark(self):
        self.send_raw(
            b"<html><head></head><body><p>charset</p></body></html>",
            "text/html; charset=" + self.SINGLEFILE_MARK_CHARSET,
        )

    FOREIGN_CHAIN = 5

    def route_foreign(self):
        # text/html on every path whatever the extension, and a chain to follow.
        name = urlsplit(self.path).path.rsplit("/", 1)[-1]
        step = re.fullmatch(r"c(\d+)\.html", name)
        n = int(step.group(1)) if step else 0
        if name == "pic.png" or n >= self.FOREIGN_CHAIN:
            self.send_html("")
        else:
            self.send_html(
                '\t<a href="c%d.html">next</a>\n\t<img src="pic.png" />\n' % (n + 1)
            )

    # Honestly typed assets, each referencing a file only a parse can find.
    ASSETS = {
        "/asset/style.css": (b"body { background: url(bg.png); }\n", "text/css"),
        "/asset/script.js": (
            b'var i = new Image(); i.src = "pic2.png";\n',
            "application/x-javascript",
        ),
        "/asset/vector.svg": (
            b'<svg xmlns="http://www.w3.org/2000/svg">'
            b'<image href="inner.png" /></svg>\n',
            "image/svg+xml",
        ),
    }

    def route_asset(self):
        path = urlsplit(self.path).path
        if path in self.ASSETS:
            body, ctype = self.ASSETS[path]
            self.send_raw(body, ctype)
        else:
            self.send_raw(self.FAKE_PNG, "image/png")

    ROUTES = {
        "/sfmark.html": route_singlefile_mark,
        "/cookies/entrance.php": route_entrance,
        "/cookies/second.php": route_second,
        "/cookies/third.php": route_third,
        "/gated/index.php": route_gated_index,
        "/gated/secret.php": route_gated_secret,
        "/robots.txt": route_robots,
        "/sitemapdir/index.xml": route_sitemap_index,
        "/sitemap.xml": route_sitemap_index,
        "/sitemapdir/pages.xml.gz": route_sitemap_pages,
        "/sitemapdir/start.html": route_sitemap_start,
        "/sitemapdir/orphan1.html": route_sitemap_orphan1,
        "/sitemapdir/orphan2.html": route_sitemap_orphan2,
        "/sitemapdir/deep1.html": route_sitemap_deep1,
        "/sitemapdir/gatedindex.xml": route_sitemap_gatedindex,
        "/sitemapdir/filtered.xml": route_sitemap_filtered,
        "/sitemapdir/gated.html": route_sitemap_gated,
        "/sitemapdir/moved.xml": route_sitemap_moved,
        "/scopesitemap.xml": route_sitemap_scope,
        "/deep/dir/start.html": route_sitemap_deepstart,
        "/deep/dir/below.html": route_sitemap_below,
        "/elsewhere/updir.html": route_sitemap_updir,
        "/warcgz/index.html": route_warcgz_index,
        "/warcgz/page.html": route_warcgz_page,
        "/warcgz/data.bin": route_warcgz_data,
        "/codec/index.html": route_codec_index,
        "/codec/br.html": route_codec_br,
        "/codec/zstd.html": route_codec_zstd,
        "/codec/junk.html": route_codec_junk,
        "/codec/bad.html": route_codec_bad,
        "/codec/bin.dat": route_codec_bin,
        "/codec/ae.html": route_codec_ae,
        "/changes/index.html": route_changes_index,
        "/changes/stable.html": route_changes_stable,
        "/changes/moved.html": route_changes_moved,
        "/changes/stable.bin": route_changes_stable_bin,
        "/changes/moved.bin": route_changes_moved_bin,
        "/changes/doomed.html": route_changes_doomed,
        "/changes/fresh.html": route_changes_fresh,
        "/changes/redir.html": route_changes_redir,
        "/changes/redirtarget.html": route_changes_redirtarget,
        "/changes/flaky.bin": route_changes_flaky,
        "/changes/coded.bin": route_changes_coded,
        "/changes/sized.html": route_changes_sized,
        "/changes/sizedredir.html": route_changes_sizedredir,
        "/changes/" + SIZED_SHORT: route_changes_sizedtarget,
        "/changes/" + SIZED_LONG: route_changes_sizedtarget,
        "/changes/codedstable.bin": route_changes_coded_stable,
        "/changes/transient.html": route_changes_transient,
        "/changes/reset.bin": route_changes_reset,
        "/changes2/index.html": route_changes2_index,
        "/changes2/stable2.bin": route_changes2_stable,
        "/changes2/ticker2.bin": route_changes2_ticker,
        "/upcodec/index.html": route_upcodec_index,
        "/upcodec/mem.html": route_upcodec_mem,
        "/upcodec/disk.bin": route_upcodec_disk,
        "/upcodec/unsup.html": route_upcodec_unsup,
        "/upcodec/fresh.html": route_upcodec_fresh,
        "/upcodec/freshdisk.bin": route_upcodec_freshdisk,
        "/uptrunc/index.html": route_uptrunc_index,
        "/uptrunc/page.html": route_uptrunc_page,
        "/uptrunc/file.bin": route_uptrunc_file,
        "/uptrunc/stay.html": route_uptrunc_stay,
        "/keep/index.html": route_keep_index,
        "/keep/page.html": route_keep_page,
        "/keep/data.bin": route_keep_data,
        "/keep/err.bin": route_keep_err,
        "/keep/stay.bin": route_keep_stay,
        "/hubfail/index.html": route_hubfail_index,
        "/hubfail/hub.html": route_hubfail_hub,
        "/hubfail/child1.html": route_hubfail_child,
        "/hubfail/child2.html": route_hubfail_child,
        "/hubfail/leaf.html": route_hubfail_leaf,
        "/hubfail/gone.html": route_hubfail_gone,
        "/ranged/asset.bin": route_ranged_asset,
        "/types/index.html": route_types_index,
        "/types/control.php": route_types,
        "/types/photo.png": route_types,
        "/types/doc.pdf": route_types,
        "/types/notype.png": route_types,
        "/types/notype.pdf": route_types,
        "/types/emptyct.png": route_types,
        "/types/lie.png": route_types,
        "/types/wrongtype.jpg": route_types,
        "/types/bigtype.jpg": route_types,
        "/types/mutant.jpg": route_types_mutant,
        "/types/packed.jpg": route_types_packed,
        "/types/report.pdf": route_types,
        "/types/page.htm": route_types,
        "/types/script.js": route_types,
        "/types/style.css": route_types,
        "/types/data.json": route_types,
        "/types/gen.php": route_types,
        "/intl/index.html": route_intl_index,
        "/intl/" + INTL_NAME: route_intl_page,
        "/watchdog/stall": route_watchdog_stall,
        "/resume/index.html": route_resume_index,
        "/resume/blob.txt": route_resume,
        "/resume304/index.html": route_resume304_index,
        "/resume304/blob.bin": route_resume304,
        "/overlap/index.html": route_overlap_index,
        "/overlap/flaky.bin": route_overlap,
        "/overlap/full.bin": route_overlap_full,
        "/crange206/index.html": route_crange206_index,
        "/crange206/blob.bin": route_crange206,
        "/crange206mem/index.html": route_crange206mem_index,
        "/crange206mem/blob.bin": route_crange206mem,
        "/resume206/index.html": route_resume206_index,
        "/resume206/blob.bin": route_resume206,
        "/resume206loop/index.html": route_resume206loop_index,
        "/resume206loop/blob.bin": route_resume206loop,
        "/alias206/index.html": route_alias206_index,
        "/alias206/Blob.bin": route_alias206_upper,
        "/alias206/blob.bin": route_alias206_lower,
        "/size/index.html": route_size_index,
        "/size/oversize.bin": route_size_oversize,
        "/chunked/index.html": route_chunked_index,
        "/chunked/page.html": route_chunked_page,
        "/chunktrunc/index.html": route_chunktrunc_index,
        "/chunktrunc/page.html": route_chunktrunc_page,
        "/chunktrunc/always.html": route_chunktrunc_always,
        "/chunktrunc/file.bin": route_chunktrunc_file,
        "/chunktrunc/always.bin": route_chunktrunc_alwaysbin,
        "/chunktrunc/stay.html": route_chunktrunc_stay,
        "/chunktrunc/hostile.html": route_chunktrunc_hostile,
        "/chunktrunc/reset.bin": route_chunktrunc_reset,
        "/chunktrail/index.html": route_chunktrail_index,
        "/chunktrail/one.html": route_chunktrail_one,
        "/chunktrail/many.html": route_chunktrail_many,
        "/chunktrail/none.html": route_chunktrail_none,
        "/chunktrail/huge.html": route_chunktrail_huge,
        "/chunktrail/bogus.html": route_chunktrail_bogus,
        "/chunktrail/eof.html": route_chunktrail_eof,
        "/chunktrail/file.bin": route_chunktrail_file,
        "/errpage/index.html": route_errpage_index,
        "/errpage/good.html": route_errpage_good,
        "/errpage/missing.html": route_errpage_missing,
        "/errpage/empty.html": route_errpage_empty,
        "/mimex/index.html": route_mimex_index,
        "/mimex/blob.pdf": route_mimex_blob,
        "/mimex/real.html": route_mimex_real,
        "/cdispo/index.html": route_cdispo_index,
        "/cdispo/fetch.php": route_cdispo,
        "/cdispo/evil.php": route_cdispo,
        "/hostalias/moved.html": route_hostalias_moved,
        "/hostalias/noext": route_hostalias_noext,
        "/hostalias/sitemap.xml": route_hostalias_sitemap,
        "/delayed/index.html": route_delayed_index,
        "/abortpurge/index.html": route_abortpurge_index,
        "/abortpurge/slow.html": route_abortpurge_slow,
        "/abortpurge/x0.html": route_abortpurge_leaf,
        "/abortpurge/x1.html": route_abortpurge_leaf,
        "/abortpurge/x2.html": route_abortpurge_leaf,
        "/abortpurge/x3.html": route_abortpurge_leaf,
        "/abortpurge/x4.html": route_abortpurge_leaf,
        "/abortpurge/x5.html": route_abortpurge_leaf,
        "/trickle/index.html": route_trickle_index,
        "/xssjob/": route_xssjob_index,
        "/xssjob/index.html": route_xssjob_index,
        "/trickle/p0.bin": route_trickle_page,
        "/trickle/p1.bin": route_trickle_page,
        "/trickle/p2.bin": route_trickle_page,
        "/trickle/p3.bin": route_trickle_page,
        "/trickle/p4.bin": route_trickle_page,
        "/trickle/p5.bin": route_trickle_page,
        "/trickle/p6.bin": route_trickle_page,
        "/trickle/p7.bin": route_trickle_page,
        DEEPDIR + "/index.html": route_deeptrickle_index,
        DEEPDIR + "/p0.bin": route_trickle_page,
        DEEPDIR + "/p1.bin": route_trickle_page,
        "/dcancel/index.html": route_dcancel_index,
        "/dcancel/p0.bin": route_dcancel_page,
        "/dcancel/p1.bin": route_dcancel_page,
        "/dcancel/p2.bin": route_dcancel_page,
        "/dcancel/p3.bin": route_dcancel_page,
        "/dcancel/p4.bin": route_dcancel_page,
        "/dcancel/p5.bin": route_dcancel_page,
        "/dcancel/p6.bin": route_dcancel_page,
        "/dcancel/p7.bin": route_dcancel_page,
        "/bigfiles/index.html": route_bigfiles_index,
        "/bigfiles/p0.bin": route_bigfile,
        "/bigfiles/p1.bin": route_bigfile,
        "/bigfiles/p2.bin": route_bigfile,
        "/bigfiles/p3.bin": route_bigfile,
        "/bigfiles/p4.bin": route_bigfile,
        "/bigfiles/p5.bin": route_bigfile,
        "/bigfiles/p6.bin": route_bigfile,
        "/bigfiles/p7.bin": route_bigfile,
        "/bigtrickle/index.html": route_bigtrickle_index,
        "/bigtrickle/p0.bin": route_bigfile,
        "/bigtrickle/p1.bin": route_trickle_page,
        "/bigtrickle/p2.bin": route_trickle_page,
        "/bigtrickle/p3.bin": route_trickle_page,
        "/bigtrunc/index.html": route_bigtrunc_index,
        "/bigtrunc/fast.bin": route_bigfile,
        "/bigtrunc/slow.bin": route_bigtrunc_slow,
        "/delayed/noloc.php": route_delayed_noloc,
        "/delayed/selfloop.php": route_delayed_selfloop,
        "/delayed/redir.php": route_delayed_redir,
        "/delayed/real.pdf": route_delayed_realpdf,
        "/delayed/notype.bin": route_delayed_notype,
        "/delayed/empty.php": route_delayed_empty,
        "/delayed/chain1.php": route_delayed_chain,
        "/delayed/chain2.php": route_delayed_chain,
        "/delayed/chain3.php": route_delayed_chain,
        "/delayed/chain4.php": route_delayed_chain,
        "/delayed/chain5.php": route_delayed_chain,
        "/delayed/chain6.php": route_delayed_chain,
        "/delayed/chain7.php": route_delayed_chain,
        "/delayed/chain8.php": route_delayed_chain,
        "/delayed/chain9.php": route_delayed_chain,
        "/cookiewall/index.html": route_cookiewall_index,
        "/cookiewall/wall.php": route_cookiewall_wall,
        "/cookiewall2/index.html": route_cookiewall2_index,
        "/cookiewall2/wall.html": route_cookiewall2_wall,
        "/cookiewall3/index.html": route_cookiewall3_index,
        "/cookiewall3/wall.php": route_cookiewall3_wall,
        "/cookiewall4/index.html": route_cookiewall4_index,
        "/cookiewall4/wall.php": route_cookiewall4_wall,
        "/redir/index.html": route_redir_index,
        "/redir/go.php": route_redir_go,
        "/redir/target.html": route_redir_target,
        "/longloc/index.html": route_longloc_index,
        "/longloc/go.php": route_longloc_go,
        "/longloc/target.html": route_longloc_target,
        "/shortloc/index.html": route_shortloc_index,
        "/shortloc/go.php": route_shortloc_go,
        "/shortloc/target.html": route_shortloc_target,
        "/bakname/index.html": route_bakname_index,
        "/bakname/a.bin": route_bakname_main,
        "/bakname/hts-tmp/a.bin.bak": route_bakname_sibling,
        "/tmpspace/index.html": route_tmpspace_index,
        "/tmpspace/a.bin": route_bakname_main,
        "/tmpspace/hts-tmp /a.bin.bak": route_bakname_sibling,
        "/mini304/index.html": route_mini304_index,
        "/mini304/page.html": route_mini304_page,
        "/errmask/index.html": route_errmask_index,
        "/errmask/keep.dat": route_errmask_keep,
        "/errmask/empty.dat": route_errmask_empty,
        "/maxrecv/index.html": route_maxrecv_index,
        "/maxrecv/r0.bin": route_maxrecv_404,
        "/maxrecv/r1.bin": route_maxrecv_404,
        "/maxrecv/r2.bin": route_maxrecv_404,
        "/maxrecv/r3.bin": route_maxrecv_404,
        "/maxrecv/r4.bin": route_maxrecv_404,
        "/maxrecv/r5.bin": route_maxrecv_404,
        "/maxrecv/r6.bin": route_maxrecv_404,
        "/maxrecv/r7.bin": route_maxrecv_404,
        "/maxrecv/r8.bin": route_maxrecv_404,
        "/maxrecv/r9.bin": route_maxrecv_404,
        "/maxrecv/r10.bin": route_maxrecv_404,
        "/maxrecv/r11.bin": route_maxrecv_404,
        "/maxrecv/r12.bin": route_maxrecv_404,
        "/maxrecv/r13.bin": route_maxrecv_404,
        "/maxrecv/r14.bin": route_maxrecv_404,
        "/maxrecv/r15.bin": route_maxrecv_404,
    }

    # --- /big/ seeded pseudo-site ------------------------------------------

    def big_send(self, body, ctype, code=200, extra=()):
        if code == 200 and self.headers.get("If-Modified-Since") == BIG_LASTMOD:
            self.send_response(304)
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        self.send_response(code)
        if code == 200:
            self.send_header("Last-Modified", BIG_LASTMOD)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        for name, value in extra:
            self.send_header(name, value)
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    def big_error(self, code, reason):
        body = big_html("error", "<p>%d</p>%s" % (code, HOME))
        self.big_send(body, "text/html", code=code, extra=[("X-Reason", reason)])

    def route_big(self):
        split = urlsplit(self.path)
        path = unquote(split.path)
        port = self.server.server_address[1]
        if path in BIG_REDIRECTS:
            code, location = BIG_REDIRECTS[path]
            self.send_response(code)
            self.send_header("Location", location)
            self.send_header("Content-Length", "0")
            self.end_headers()
        elif path == "/big/index.html":
            self.big_send(big_index(port), "text/html")
        elif path in BIG_SIMPLE_PAGES:
            body = big_html(path, "<p>%s</p>%s" % (BIG_SIMPLE_PAGES[path], HOME))
            if path == "/big/f1/gzok.html":
                self.big_send(
                    gzip.compress(body, mtime=0),
                    "text/html",
                    extra=[("Content-Encoding", "gzip")],
                )
            elif path == "/big/f1/gzid.html":
                # Plain body mislabeled as gzip: identity fallback keeps it (#47)
                self.big_send(
                    body,
                    "text/html",
                    extra=[("Content-Encoding", "gzip")],
                )
            else:
                self.big_send(body, "text/html")
        elif path == "/big/f1/list.html":
            # Pagination: distinct content per query string.
            body = big_html("list", "<p>listing %s</p>%s" % (split.query or "1", HOME))
            self.big_send(body, "text/html")
        elif path == "/big/f6/based.html":
            self.big_send(
                big_html(
                    "based",
                    '<base href="http://127.0.0.1:%d/big/f6/sub/">'
                    '<a href="leaf.html">leaf</a>' % port,
                ),
                "text/html",
            )
        elif path == "/big/f7/frames.html":
            self.big_send(
                b'<html><frameset cols="50%,50%"><frame src="fa.html">'
                b'<frame src="fb.html"><noframes><body><a href="fn.html">fn</a>'
                b"</body></noframes></frameset></html>",
                "text/html",
            )
        elif path == "/big/f9/café.html":
            self.big_send(big_html("cafe", "<p>cafe</p>%s" % HOME), "text/html")
        elif path == "/big/f9/latin1.html":
            self.big_send(
                b"<html><body><p>caf\xe9 latin</p></body></html>",
                "text/html; charset=ISO-8859-1",
            )
        elif path == "/big/f9/metaonly.html":
            self.big_send(
                '<html><head><meta charset="utf-8"></head>'
                "<body><p>café meta</p></body></html>".encode(),
                "text/html",
            )
        elif path == "/big/f9/bom.html":
            self.big_send(
                b"\xef\xbb\xbf" + big_html("bom", "<p>bom</p>%s" % HOME), "text/html"
            )
        elif path == "/big/f10/empty.html":
            self.big_send(b"", "text/html")
        elif path == "/big/f12/rss.xml":
            self.big_send(_big_rss(port), "text/xml")
        elif path == "/big/f12/atom.xml":
            self.big_send(_big_atom(port), "application/xml")
        elif path == "/big/f12/sitemap.xml":
            self.big_send(_big_sitemap(port), "text/xml")
        elif path.startswith("/big/p/"):
            try:
                n = int(path[len("/big/p/") : -len(".html")])
            except ValueError:
                n = -1
            if 0 <= n < BIG_PAGES and path.endswith(".html"):
                self.big_send(big_page(n, port), "text/html")
            else:
                self.big_error(404, "no such page")
        elif path.startswith("/big/a/") or path.startswith("/big/x/"):
            name = path[len("/big/a/") :]
            if path.startswith("/big/a/") and name in BIG_TEXT_ASSETS:
                text, ctype = BIG_TEXT_ASSETS[name]
                self.big_send(text.encode(), ctype)
            elif name.endswith(".html"):
                # Decoy targets 200 so a parser leak becomes a mirror file.
                self.big_send(big_html(name, "<p>%s</p>" % name), "text/html")
            else:
                ext = name.rsplit(".", 1)[-1]
                ctype = BIG_CTYPES.get(ext, "application/octet-stream")
                self.big_send(big_asset(name), ctype)
        elif path in BIG_DOWNLOADS:
            ext, cdispo = BIG_DOWNLOADS[path]
            extra = [("Content-Disposition", cdispo)] if cdispo else []
            self.big_send(
                big_asset(path[len("/big/") :] + "." + ext),
                BIG_CTYPES[ext],
                extra=extra,
            )
        elif path == "/big/e/404.html":
            self.big_error(404, "Not Found")
        elif path == "/big/e/410.html":
            self.big_error(410, "Gone")
        elif path == "/big/e/500.html":
            self.big_error(500, "Server Error")
        elif path == "/big/e/gztrunc.html":
            # Half a gzip stream, honest Content-Length: decode fails, and the
            # missing Last-Modified keeps it the one uncacheable resource.
            full = gzip.compress(big_html("gz", "x" * 3000), mtime=0)
            body = full[: len(full) // 2]
            self.send_response(200)
            self.send_header("Content-Type", "text/html")
            self.send_header("Content-Encoding", "gzip")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            if self.command != "HEAD":
                self.wfile.write(body)
        else:
            self.big_error(404, "no such big path")

    # --- dispatch ----------------------------------------------------------

    def reject_fragment(self):
        # Strict server: a '#' in the request-target is the client failing to
        # drop a fragment (#204). RFC 3986 forbids it on the wire; answer 400.
        if "#" in self.path:
            self.send_response(400, "Bad Request")
            self.send_header("Content-Length", "0")
            self.end_headers()
            return True
        return False

    def dispatch(self):
        self._set_cookies = []
        path = urlsplit(self.path).path
        if path.startswith("/big/"):
            self.route_big()
            return True
        if path.startswith("/foreign/"):
            self.route_foreign()
            return True
        if path.startswith("/asset/"):
            self.route_asset()
            return True
        if path.startswith("/jsmime/"):
            self.route_jsmime()
            return True
        if path.startswith("/charset/"):
            self.route_charset()
            return True
        if path.startswith("/charref/"):
            self.route_charref()
            return True
        if path.startswith("/titleenc/"):
            self.route_titleenc()
            return True
        # Match percent-encoded paths (accented #157 route) by their decoded form.
        handler = self.ROUTES.get(path) or self.ROUTES.get(unquote(path))
        if handler is None:
            if re.fullmatch(r"/sitemapdir/chain\d+\.xml", path):
                handler = type(self).route_sitemap_chain
            elif re.fullmatch(r"/sitemapdir/cap\d+\.xml", path):
                handler = type(self).route_sitemap_capset
            elif re.fullmatch(r"/sitemapdir/cap\d+\.html", path):
                handler = type(self).route_sitemap_cappage
            elif path.startswith("/xssjob/"):
                # Whatever the engine made of the metacharacters, the job trickles.
                handler = type(self).route_trickle_page
        if handler is not None:
            handler(self)
            return True
        return False

    def do_GET(self):
        # Before reject_fragment(), so an unescaped '#' still shows up recorded.
        if self.path.startswith("/charref/"):
            self.record_charref()
        if self.reject_fragment():
            return
        if not self.dispatch():
            super().do_GET()

    def do_HEAD(self):
        if self.reject_fragment():
            return
        if not self.dispatch():
            super().do_HEAD()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True, help="docroot for static files")
    parser.add_argument("--bind", default="127.0.0.1", help="bind address")
    parser.add_argument("--tls", action="store_true", help="serve HTTPS")
    parser.add_argument("--cert", help="TLS certificate (PEM)")
    parser.add_argument("--key", help="TLS private key (PEM)")
    args = parser.parse_args()

    root = os.path.abspath(args.root)

    def factory(*a, **kw):
        return Handler(*a, directory=root, **kw)

    # macOS/BSD drop SYNs when the listen backlog overflows (Linux is lenient);
    # raise it from Python's default 5 so a busy -c8 crawl can't lose fetches.
    class BacklogHTTPServer(ThreadingHTTPServer):
        request_queue_size = 128

        # Skip the getfqdn() reverse lookup stock server_bind() does for the
        # unread server_name: it stalls 35s on macOS (#870).
        def server_bind(self):
            socketserver.TCPServer.server_bind(self)
            self.server_name, self.server_port = self.server_address[:2]

    httpd = BacklogHTTPServer((args.bind, 0), factory)

    if args.tls:
        import ssl

        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ctx.load_cert_chain(certfile=args.cert, keyfile=args.key)
        httpd.socket = ctx.wrap_socket(httpd.socket, server_side=True)

    port = httpd.socket.getsockname()[1]
    # Keep the port line the launcher parses LF: Windows would emit \r\n.
    sys.stdout.reconfigure(newline="\n")
    print(f"PORT {port}", flush=True)

    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
