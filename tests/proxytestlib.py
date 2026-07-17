#!/usr/bin/env python3
"""Shared helpers for the local proxy test servers.

A CONNECT proxy in front of an origin, as used by proxy-https-server.py (TLS
origin, #85) and proxy-connect-server.py (plain origin, #564). socks5-server.py
reuses the relay only; its origin is specialised for keep-alive reuse.
Importable because Python puts the running script's directory on sys.path.
"""
import http.server
import os
import socket
import socketserver
import ssl
import sys
import threading

PROXY_LOG = "proxy.log"
ORIGIN_LOG = "origin-headers.log"


def bind_ephemeral():
    """Listening socket on a free loopback port, and that port."""
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", 0))
    srv.listen(16)
    return srv, srv.getsockname()[1]


def pipe(src, dst):
    """Relay bytes one way until EOF, then tear both ends down."""
    try:
        while True:
            data = src.recv(65536)
            if not data:
                break
            dst.sendall(data)
    except OSError:
        pass
    finally:
        for sock in (src, dst):
            try:
                sock.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass


def make_origin(logdir, body):
    class Origin(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            # the request line proves origin-form vs absolute-URI (#564)
            with open(os.path.join(logdir, ORIGIN_LOG), "a") as handle:
                handle.write(self.requestline + "\n")
                for key in self.headers.keys():
                    handle.write(key + "\n")
            self.send_response(200)
            self.send_header("Content-Type", "text/html")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, *args):
            pass

    return Origin


def start_origin(logdir, body, certfile=None):
    """Serve body on an ephemeral port, over TLS when certfile is given."""
    httpd = socketserver.TCPServer(("127.0.0.1", 0), make_origin(logdir, body))
    if certfile is not None:
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ctx.load_cert_chain(certfile)
        httpd.socket = ctx.wrap_socket(httpd.socket, server_side=True)
    port = httpd.socket.getsockname()[1]
    threading.Thread(target=httpd.serve_forever, daemon=True).start()
    return port


def handle_client(conn, logdir, mode, default_port):
    rfile = conn.makefile("rb")
    request_line = rfile.readline().decode("latin-1").strip()
    auth = None
    while True:
        line = rfile.readline().decode("latin-1")
        if line in ("\r\n", "\n", ""):
            break
        key, _, value = line.partition(":")
        if key.strip().lower() == "proxy-authorization":
            auth = value.strip()
    with open(os.path.join(logdir, PROXY_LOG), "a") as handle:
        handle.write(request_line + "\n")
        if auth is not None:
            handle.write("AUTH " + auth + "\n")
    parts = request_line.split()
    # CONNECT-only: reject the classic absolute-URI form a normal proxy accepts
    if not (len(parts) >= 2 and parts[0] == "CONNECT"):
        conn.sendall(b"HTTP/1.0 501 Not Implemented\r\n\r\n")
        conn.close()
        return
    if mode == "flood":
        # 200, then endless headers with no blank line: the client must not hang
        try:
            conn.sendall(b"HTTP/1.0 200 Connection established\r\n")
            while True:
                conn.sendall(b"X-Pad: 0123456789\r\n")
        except OSError:
            pass
        conn.close()
        return
    host, _, port = parts[1].partition(":")
    try:
        # default_port only backstops a portless CONNECT; httrack sends host:port
        upstream = socket.create_connection((host, int(port or default_port)))
    except OSError:
        conn.sendall(b"HTTP/1.0 502 Bad Gateway\r\n\r\n")
        conn.close()
        return
    conn.sendall(b"HTTP/1.0 200 Connection established\r\n\r\n")
    threading.Thread(target=pipe, args=(conn, upstream), daemon=True).start()
    pipe(upstream, conn)


def start_proxy(logdir, mode, default_port):
    srv, port = bind_ephemeral()

    def accept_loop():
        while True:
            conn, _ = srv.accept()
            threading.Thread(
                target=handle_client,
                args=(conn, logdir, mode, default_port),
                daemon=True,
            ).start()

    threading.Thread(target=accept_loop, daemon=True).start()
    return port


def serve(logdir, origin_body, default_port, mode="ok", certfile=None):
    """Start the origin+proxy pair, announce both ports, then block forever."""
    for name in (PROXY_LOG, ORIGIN_LOG):
        open(os.path.join(logdir, name), "w").close()
    origin_port = start_origin(logdir, origin_body, certfile)
    proxy_port = start_proxy(logdir, mode, default_port)
    # Keep the port lines the caller parses LF: Windows would emit \r\n.
    sys.stdout.reconfigure(newline="\n")
    print("ORIGIN %d" % origin_port, flush=True)
    print("PROXY %d" % proxy_port, flush=True)
    print("ready", flush=True)
    threading.Event().wait()
