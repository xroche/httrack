#!/usr/bin/env python3
"""Raw-socket probe for the header-injection and header-dedup tests.

Speaks HTTP off the socket so a split header line stays visible; serves an
origin-form site and an http-proxy absolute-URI one on the same port.

Appends "=== REQUEST ===\\n<bytes>\\n" per request to the log named on argv, and
writes each request verbatim to <log>.<n> as well, so a consumer grading line
endings never has to split the log with a tool that may translate them. Prints
"PORT <n>" once listening.
"""

import os
import socket
import sys
import threading

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from proxytestlib import bind_ephemeral  # noqa: E402

# &#13;&#10; outlives the parser's control-byte escape, so these land as raw CR/LF
POISON_PATH = "/x&#13;&#10;Foo:%20injected.html"
POISON_HOST = "evil&#13;&#10;Foo:%20injected.example"
FTP_POISON_HOST = "ftpevil&#13;&#10;Foo:%20x.example"


def page(body):
    return ("<html><body>%s</body></html>" % body).encode()


def root_page(port):
    return page(
        '<a href="http://127.0.0.1:%d%s">poison</a>'
        '<a href="http://127.0.0.1:%d/plain.html">plain</a>' % (port, POISON_PATH, port)
    )


# the poisoned page must link onward, or its URL never becomes a Referer
POISON_PAGE = page('<a href="/deep.html">deep</a>')
# the ftp link takes the ftp-through-proxy emission site, which has no Host:
PROXY_ROOT = page(
    '<a href="ftp://' + FTP_POISON_HOST + '/f.txt">ftp</a>'
    '<a href="http://' + POISON_HOST + '/p.html">poison</a>'
    '<a href="http://plain.example/p.html">plain</a>'
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
    # one hop, so the second request of a crawl carries a Referer (328)
    if b"/hop.html" in line:
        return page('<a href="/plain.html">plain</a>')
    # the proxied pages link onward too, so a poisoned *host* becomes a referer
    if b"/p.html" in line:
        return page('<a href="/deep2.html">deep</a>')
    return page("leaf")


def handle(conn, port, logf, lock, seq):
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
            seq[0] += 1
            logf.write(b"=== REQUEST ===\n" + data + b"\n")
            logf.flush()
            with open("%s.%d" % (sys.argv[1], seq[0]), "wb") as reqf:
                reqf.write(data)
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
    seq = [0]
    with open(sys.argv[1], "wb") as logf:
        sys.stdout.reconfigure(newline="\n")  # the launcher parses PORT, CRLF breaks it
        print("PORT %d" % port, flush=True)
        while True:
            conn, _ = srv.accept()
            threading.Thread(
                target=handle, args=(conn, port, logf, lock, seq), daemon=True
            ).start()


if __name__ == "__main__":
    main()
