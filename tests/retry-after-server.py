#!/usr/bin/env python3
"""Status codes with and without Retry-After, for 490_local-retry-after.

Each path answers one case and prints "HIT <path>" so the test can count the
requests the engine actually made. Prints "PORT <n>" once listening.
"""

import email.utils
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

# path -> (status, Retry-After value; None sends no header)
CASES = {
    "rate": (429, "1"),
    "rate-nodelay": (429, None),
    "busy": (503, "1"),
    "busy-nodelay": (503, None),
    "busy-date": (503, "DATE"),
    "busy-junk": (503, "soon"),
    "boom": (500, None),
    "huge": (429, "100000"),
    "ok": (200, None),
}

INDEX = (
    "<html><body>"
    + "".join('<a href="%s.html">%s</a>' % (n, n) for n in sorted(CASES))
    + "</body></html>"
).encode()

lock = threading.Lock()


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.0"

    def log_message(self, *args):
        pass

    def do_GET(self):
        path = self.path.split("?")[0]
        with lock:
            print("HIT %s %s" % (self.command, path), flush=True)
        if path in ("/", "/index.html"):
            return self.reply(200, INDEX)
        name = path.strip("/")
        if name.endswith(".html"):
            name = name[: -len(".html")]
        if name not in CASES:
            return self.reply(404, b"no")
        code, retry_after = CASES[name]
        if retry_after == "DATE":
            retry_after = email.utils.formatdate(time.time() + 1, usegmt=True)
        self.reply(code, b"<html><body>body</body></html>", retry_after)

    do_HEAD = do_GET

    def reply(self, code, body, retry_after=None):
        self.send_response(code)
        if retry_after is not None:
            self.send_header("Retry-After", retry_after)
        self.send_header("Content-Type", "text/html")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)


class Server(ThreadingHTTPServer):
    request_queue_size = 32  # BSD/macOS drop connections at Python's default 5


# The shell greps these lines anchored, and a native python writes CRLF.
sys.stdout.reconfigure(newline="\n")

srv = Server(("127.0.0.1", 0), Handler)
print("PORT %d" % srv.server_address[1], flush=True)
srv.serve_forever()
