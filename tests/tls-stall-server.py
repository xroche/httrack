#!/usr/bin/env python3
"""Peers that accept a connection and never speak TLS (#607).

Modes: "direct" stalls the handshake straight away; "proxy <secs>" answers a
CONNECT after <secs> before stalling, so the handshake starts on a clock the
connect has already eaten into. Prints "PORT <n>" once listening.
Usage: tls-stall-server.py [direct | proxy <secs>]
"""
import os
import sys
import threading
import time

# python3 -P (PYTHONSAFEPATH) drops the script's own directory from sys.path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from proxytestlib import bind_ephemeral  # noqa: E402

held = []  # keep every socket open: a close would fail the handshake outright


def stall(conn, delay):
    if delay is not None:
        rfile = conn.makefile("rb")
        while rfile.readline() not in (b"\r\n", b"\n", b""):
            pass
        time.sleep(delay)
        conn.sendall(b"HTTP/1.0 200 Connection established\r\n\r\n")
    held.append(conn)


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "direct"
    delay = float(sys.argv[2]) if mode == "proxy" else None
    srv, port = bind_ephemeral()
    sys.stdout.reconfigure(newline="\n")  # Windows would emit \r\n
    print("PORT %d" % port, flush=True)
    while True:
        conn, _ = srv.accept()
        threading.Thread(target=stall, args=(conn, delay), daemon=True).start()


main()
