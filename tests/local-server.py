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

    def send_raw(self, body, content_type):
        """Send a raw body with an explicit Content-Type, or none at all when
        content_type is None (to observe httrack's typeless-file naming)."""
        self.send_response(200)
        if content_type is not None:
            self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    # Fake-binary blobs for the image/pdf/typeless cases.
    FAKE_PNG = b"\x89PNG\r\n\x1a\n" + b"\x00" * 64
    FAKE_PDF = b"%PDF-1.4\n" + b"\x00" * 64

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
    # warns "bogus state (broken size)" and skips the cache unless -%B.
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
        "/redir/index.html": route_redir_index,
        "/redir/go.php": route_redir_go,
        "/redir/target.html": route_redir_target,
    }

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
