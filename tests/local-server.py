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

    ROUTES = {
        "/cookies/entrance.php": route_entrance,
        "/cookies/second.php": route_second,
        "/cookies/third.php": route_third,
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
    }

    # --- dispatch ----------------------------------------------------------

    def dispatch(self):
        self._set_cookies = []
        path = urlsplit(self.path).path
        handler = self.ROUTES.get(path)
        if handler is not None:
            handler(self)
            return True
        return False

    def do_GET(self):
        if not self.dispatch():
            super().do_GET()

    def do_HEAD(self):
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
