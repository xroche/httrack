#!/usr/bin/env python3
"""Raw-socket recorder for the >postfile:/>post: provenance test.

Serves a page whose links carry the undocumented post tokens, and appends every
request it receives to <log> verbatim, behind a "=== REQUEST ===" line, so a
request that is a local file rather than an HTTP header block is still readable.
Prints "PORT <n>" once listening.
argv: <log> <path the page asks the engine to send>.
"""

import os
import socket
import sys
import threading

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from proxytestlib import bind_ephemeral  # noqa: E402

# &gt; survives the parser's unescaping as ">", which is what forms the token.
ROOT = (
    '<html><body><a href="/leak.html?&gt;postfile:%s">file</a>'
    '<a href="/post.html?&gt;post:sent=1">post</a>'
    '<a href="/redir.html">redirect</a></body></html>'
)


def reply_to(request, secret):
    # A redirect reaches the token with no link carrying it. The target is not
    # HTML on purpose: that is the arm which re-records the link, and the one
    # that used to hand it the grandparent's referer.
    if request.startswith(b"GET /redir.html"):
        target = ("/leak2.gif?&gt;postfile:%s" % secret).replace("&gt;", ">")
        return (
            b"HTTP/1.1 302 Found\r\nLocation: %s\r\n"
            b"Content-Length: 0\r\nConnection: close\r\n\r\n" % target.encode()
        )
    body = (ROOT % secret).encode() if request.startswith(b"GET / ") else b"leaf"
    return (
        b"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n"
        b"Content-Length: %d\r\nConnection: close\r\n\r\n" % len(body) + body
    )


def handle(conn, logf, lock, secret):
    # A postfile request is the file's bytes, so it need never end in CRLF CRLF;
    # the timeout is what closes the read in that case.
    conn.settimeout(2)
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
        try:
            conn.sendall(reply_to(data, secret))
        except OSError:
            pass
    try:
        conn.close()
    except OSError:
        pass


def main():
    secret = sys.argv[2]
    srv, port = bind_ephemeral()
    lock = threading.Lock()
    with open(sys.argv[1], "wb") as logf:
        sys.stdout.reconfigure(newline="\n")  # the launcher parses PORT
        print("PORT %d" % port, flush=True)
        while True:
            conn, _ = srv.accept()
            threading.Thread(
                target=handle, args=(conn, logf, lock, secret), daemon=True
            ).start()


if __name__ == "__main__":
    main()
