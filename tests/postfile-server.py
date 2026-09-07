#!/usr/bin/env python3
"""Raw-socket recorder for the >postfile:/>post: provenance test.

Serves a page whose links carry the undocumented post tokens, and appends every
request it receives verbatim to <log>.<n>, so a request that is a local file
rather than an HTTP header block is still readable. Prints "PORT <n>" once
listening. argv: <log> <path the page asks the engine to send>.
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
    '<a href="/post.html?&gt;post:sent=1">post</a></body></html>'
)


def handle(conn, logf, lock, seq, secret):
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
            seq[0] += 1
            logf.write(b"=== REQUEST ===\n" + data + b"\n")
            logf.flush()
            with open("%s.%d" % (sys.argv[1], seq[0]), "wb") as reqf:
                reqf.write(data)
        body = (ROOT % secret).encode() if data.startswith(b"GET / ") else b"leaf"
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
    secret = sys.argv[2]
    srv, port = bind_ephemeral()
    lock = threading.Lock()
    seq = [0]
    with open(sys.argv[1], "wb") as logf:
        sys.stdout.reconfigure(newline="\n")  # the launcher parses PORT
        print("PORT %d" % port, flush=True)
        while True:
            conn, _ = srv.accept()
            threading.Thread(
                target=handle, args=(conn, logf, lock, seq, secret), daemon=True
            ).start()


if __name__ == "__main__":
    main()
