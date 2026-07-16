#!/usr/bin/env python3
"""Accept TCP connections on an ephemeral port and never speak TLS (#607).

Prints "PORT <n>" once listening. Connections are held open and silent, so a
client's TLS handshake stalls until its own timeout reaps it.
"""
import socket

srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("127.0.0.1", 0))
srv.listen(16)
print("PORT %d" % srv.getsockname()[1], flush=True)

held = []
while True:
    conn, _ = srv.accept()
    held.append(conn)  # keep it open: a closed socket would fail the handshake
