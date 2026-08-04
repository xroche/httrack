#!/usr/bin/env python3
"""Raw-socket probe for 159_local-header-injection.

Speaks HTTP off the socket rather than through http.server so the request is
logged byte-for-byte: a split header line is what this test is about, and any
parsing framework would have already normalised it away. Serves an origin-form
site and, on the same port, an http-proxy absolute-URI one, so a single server
covers both the Referer and the Host/request-line vectors.

Appends "=== REQUEST ===\\n<bytes>\\n" per request to the log named on argv, and
prints "PORT <n>" once listening.
"""

import os
import socket
import sys
import threading

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from proxytestlib import bind_ephemeral  # noqa: E402

# &#13;&#10; survives the parser's control-byte escape (hts_unescapeEntities runs
# after it), so these land as raw CR/LF in the engine's link buffers.
POISON_PATH = "/x&#13;&#10;Foo:%20injected.html"
POISON_HOST = "evil&#13;&#10;Foo:%20injected.example"


def page(body):
    return ("<html><body>%s</body></html>" % body).encode()


def root_page(port):
    return page(
        '<a href="http://127.0.0.1:%d%s">poison</a>'
        '<a href="http://127.0.0.1:%d/plain.html">plain</a>' % (port, POISON_PATH, port)
    )


# the poisoned page must link onward, or its URL never becomes a Referer
POISON_PAGE = page('<a href="/deep.html">deep</a>')
PROXY_ROOT = page(
    '<a href="http://%s/p.html">poison</a>'
    '<a href="http://plain.example/p.html">plain</a>' % POISON_HOST
)


def body_for(request, port):
    """Pick a body from the request line alone; a split one just gets a leaf."""
    line = request.split(b"\r\n", 1)[0]
    if b"start.example" in line:
        return PROXY_ROOT
    if line.startswith(b"GET / "):
        return root_page(port)
    if b"/x" in line:
        return POISON_PAGE
    # the proxied pages link onward too, so a poisoned *host* becomes a referer
    if b"/p.html" in line:
        return page('<a href="/deep2.html">deep</a>')
    return page("leaf")


def handle(conn, port, logf, lock):
    conn.settimeout(10)
    data = b""
    try:
        while b"\r\n\r\n" not in data:
            chunk = conn.recv(4096)
            if not chunk:
                break
            data += chunk
    except (OSError, socket.timeout):
        pass
    if data:
        with lock:
            logf.write(b"=== REQUEST ===\n" + data + b"\n")
            logf.flush()
        body = body_for(data, port)
        try:
            conn.sendall(
                b"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n"
                b"Content-Length: %d\r\nConnection: close\r\n\r\n" % len(body) + body
            )
        except OSError:
            pass
    try:
        conn.close()
    except OSError:
        pass


def main():
    srv, port = bind_ephemeral()
    lock = threading.Lock()
    with open(sys.argv[1], "wb") as logf:
        print("PORT %d" % port, flush=True)
        while True:
            conn, _ = srv.accept()
            threading.Thread(
                target=handle, args=(conn, port, logf, lock), daemon=True
            ).start()


if __name__ == "__main__":
    main()
