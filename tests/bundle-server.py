#!/usr/bin/env python3
"""Several sites behind one proxy port, for 656_local-bundles and
788_local-travel-domain.

httrack reaches it as its -P proxy, so the absolute request URL names the
site. Every request prints one "HIT <host><path> ua=<...> auth=<...>
cookie=<...> x=<...>" line, "-" for a missing header, with the cookies in
name order. Prints "PORT <n>".
"""

import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit

CDN = "http://cdn.gamma.test"
PAGES = {
    "www.alpha.test/": '<a href="/page.html">p</a><img src="%s/a.gif">' % CDN,
    "www.alpha.test/page.html": "alpha page",
    "alpha.test/plain.html": "plain URL on the alpha site",
    "www.beta.test/": '<img src="%s/b.gif">' % CDN,
    "cdn.gamma.test/a.gif": "GIF89a",
    "cdn.gamma.test/b.gif": "GIF89a",
    "github.io/": '<a href="http://attacker.github.io/x.html">x</a>',
    "attacker.github.io/x.html": "another user's site",
    "evil.test/": "another site",
    "sub.alpha.test/home": "back on the alpha site",
    "example.com/": '<a href="http://www.example.com/w.html">w</a>'
    '<a href="http://sub.example.com/s.html">s</a>'
    '<a href="http://example.net/n.html">n</a>',
    "a.co.uk/": '<a href="http://www.a.co.uk/w.html">w</a>'
    '<a href="http://b.co.uk/b.html">b</a>',
    "192.0.2.1/": '<a href="http://192.0.2.1/same.html">same</a>'
    '<a href="http://192.0.2.2/other.html">other</a>',
}
# out of the bundle's site and back in
REDIRECTS = {
    "www.alpha.test/out": "http://evil.test/back",
    "evil.test/back": "http://sub.alpha.test/home",
}

lock = threading.Lock()


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.0"

    def log_message(self, *args):
        pass

    def do_GET(self):
        url = urlsplit(self.path)
        key = url.hostname + url.path
        with lock:
            print(
                "HIT %s ua=%s auth=%s cookie=%s x=%s"
                % (
                    key,
                    self.headers.get("User-Agent", "-"),
                    self.headers.get("Authorization", "-"),
                    "; ".join(sorted(self.headers.get("Cookie", "-").split("; "))),
                    self.headers.get("X-Global", "-"),
                ),
                flush=True,
            )
        if key in REDIRECTS:
            self.send_response(302)
            self.send_header("Location", REDIRECTS[key])
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        body = PAGES.get(key, "")
        ctype = "image/gif" if key.endswith(".gif") else "text/html"
        if ctype == "text/html":
            body = "<html><body>%s</body></html>" % body
        self.reply(200, body.encode(), ctype)

    def reply(self, code, body, ctype):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


class Server(ThreadingHTTPServer):
    request_queue_size = 32  # BSD/macOS drop connections at Python's default 5


# The shell greps these lines anchored, and a native python writes CRLF.
sys.stdout.reconfigure(newline="\n")

srv = Server(("127.0.0.1", 0), Handler)
print("PORT %d" % srv.server_address[1], flush=True)
srv.serve_forever()
