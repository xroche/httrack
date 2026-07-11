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
import gzip
import hashlib
import os
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
    # text/javascript is fetched but never scanned: the URL inside must stay
    # out of the mirror.
    "decoy.js": (
        'var d = new Image(); d.src = "/big/x/never-scanned.png";\n',
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
        '<script src="/big/a/decoy.js"></script>'
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

    def route_robots(self):
        body = b"User-agent: *\nDisallow:\n"
        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

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

    # --- /mini304/: tiny fully-cacheable site (an update gets only 304s) ---
    def route_mini304_index(self):
        self.big_send(
            b'<html><body>\n\t<a href="page.html">page</a>\n</body></html>\n',
            "text/html",
        )

    def route_mini304_page(self):
        self.big_send(b"<html><body>tiny cacheable page</body></html>\n", "text/html")

    # --- /errmask/: issue #176 — a page that 200'd on the first crawl but 403s
    # on the update fetch must keep its good copy, not be overwritten nor purged.
    ERRMASK_GOOD = b"KEEP" + b"." * 1020  # 1024 B distinctive non-HTML body
    ERRMASK_ERR = b"<html><body>error 403</body></html>\n"

    def route_errmask_index(self):
        self.send_html('\t<a href="keep.dat">keep</a>\n')

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

    ROUTES = {
        "/cookies/entrance.php": route_entrance,
        "/cookies/second.php": route_second,
        "/cookies/third.php": route_third,
        "/gated/index.php": route_gated_index,
        "/gated/secret.php": route_gated_secret,
        "/robots.txt": route_robots,
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
        "/resume/index.html": route_resume_index,
        "/resume/blob.txt": route_resume,
        "/resume304/index.html": route_resume304_index,
        "/resume304/blob.bin": route_resume304,
        "/overlap/index.html": route_overlap_index,
        "/overlap/flaky.bin": route_overlap,
        "/overlap/full.bin": route_overlap_full,
        "/size/index.html": route_size_index,
        "/size/oversize.bin": route_size_oversize,
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
        "/delayed/index.html": route_delayed_index,
        "/trickle/index.html": route_trickle_index,
        "/trickle/p0.bin": route_trickle_page,
        "/trickle/p1.bin": route_trickle_page,
        "/trickle/p2.bin": route_trickle_page,
        "/trickle/p3.bin": route_trickle_page,
        "/trickle/p4.bin": route_trickle_page,
        "/trickle/p5.bin": route_trickle_page,
        "/trickle/p6.bin": route_trickle_page,
        "/trickle/p7.bin": route_trickle_page,
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
        "/redir/index.html": route_redir_index,
        "/redir/go.php": route_redir_go,
        "/redir/target.html": route_redir_target,
        "/mini304/index.html": route_mini304_index,
        "/mini304/page.html": route_mini304_page,
        "/errmask/index.html": route_errmask_index,
        "/errmask/keep.dat": route_errmask_keep,
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
        if path.startswith("/charset/"):
            self.route_charset()
            return True
        # Match percent-encoded paths (accented #157 route) by their decoded form.
        handler = self.ROUTES.get(path) or self.ROUTES.get(unquote(path))
        if handler is not None:
            handler(self)
            return True
        return False

    def do_GET(self):
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

    httpd = ThreadingHTTPServer((args.bind, 0), factory)

    if args.tls:
        import ssl

        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ctx.load_cert_chain(certfile=args.cert, keyfile=args.key)
        httpd.socket = ctx.wrap_socket(httpd.socket, server_side=True)

    port = httpd.socket.getsockname()[1]
    # The launcher reads this line to discover the ephemeral port.
    print(f"PORT {port}", flush=True)

    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
