#!/usr/bin/env python3
"""Raw-socket server for 433: one malformed HTTP status line per path.

A conforming HTTP server cannot emit these, so the reply is written straight to
the socket. The path picks the case; anything else gets a well-formed 200.
Prints "PORT <n>" once listening.
"""

import os
import socket
import sys
import threading

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from proxytestlib import bind_ephemeral  # noqa: E402

BODY = b"<html><body>marker_body</body></html>"
HTML = b"Content-Type: text/html\r\n"
# past INT_MAX, and 2**32 + 200 so a wrapping sscanf("%d") lands on 200
OVERFLOW = b"4294967496"

CASES = {
    # well-formed, with a reason phrase no infostatuscode() table can produce
    "ok": b"HTTP/1.1 200 Fine Thanks\r\n" + HTML + b"\r\n" + BODY,
    # HTTP/ prefix, then something that is not a status code
    "nocode": b"HTTP/1.1 OK\r\n\r\n" + BODY,
    "nonnumeric": b"HTTP/1.1 XYZ Bad\r\n\r\n" + BODY,
    "signed": b"HTTP/1.1 +200 OK\r\n\r\n" + BODY,
    "twodigit": b"HTTP/1.1 20 OK\r\n\r\n" + BODY,
    "fourdigit": b"HTTP/1.1 2000 OK\r\n\r\n" + BODY,
    "overflow": b"HTTP/1.1 " + OVERFLOW + b" OK\r\n" + HTML + b"\r\n" + BODY,
    # nothing at all after the version token
    "versiononly": b"HTTP/1.1\r\n\r\n" + BODY,
    # no HTTP/ prefix
    "noprefix": b"200 OK\r\n\r\n" + BODY,
    # HTTP/ with no version: the version token ends at the space, so the code
    # is still where the parser looks for it
    "noversion": b"HTTP/ 200 OK\r\n" + HTML + b"\r\n" + BODY,
    # a lone CR does not end the line, so the phrase swallows a forged log line
    "lonecr": b"HTTP/1.1 404 Gone\r18:00:00\tError:\tINJECTED\r\n\r\n" + BODY,
    "lonelf": b"HTTP/1.1 200 Fine Thanks\n" + HTML + b"\n" + BODY,
    # no line terminator anywhere, then close
    "noterm": b"HTTP/1.1 200 Fine Thanks",
    # empty first line, and one of nothing but spaces
    "emptyline": b"\r\n\r\n" + BODY,
    "spaces": b"    \t \r\n\r\n" + BODY,
    # a body where the status line belongs
    "ltjunk": b"<html><body>marker_junk</body></html>\r\n\r\n" + BODY,
    # a status line well past the reader's 1024-byte line buffer
    "huge": b"HTTP/1.1 200 " + b"A" * 4000 + b"\r\n\r\n" + BODY,
}

DEFAULT = b"HTTP/1.1 200 OK\r\n" + HTML + b"\r\n" + BODY


def reply_for(request):
    line = request.split(b"\r\n", 1)[0].split(b" ")
    if len(line) < 2:
        return DEFAULT
    name = line[1].decode("latin-1").strip("/")
    if name.endswith(".html"):
        name = name[: -len(".html")]
    return CASES.get(name, DEFAULT)


def handle(conn):
    conn.settimeout(10)
    data = b""
    try:
        while b"\r\n\r\n" not in data:
            chunk = conn.recv(4096)
            if not chunk:
                break
            data += chunk
        if data:
            conn.sendall(reply_for(data))
    except (OSError, socket.timeout):
        pass
    finally:
        conn.close()


def main():
    sock, port = bind_ephemeral()
    print("PORT %d" % port, flush=True)
    while True:
        conn, _ = sock.accept()
        threading.Thread(target=handle, args=(conn,), daemon=True).start()


main()
