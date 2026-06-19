#!/usr/bin/env python3
"""Local CONNECT proxy + self-signed HTTPS origin for the issue #85 test.

Starts a TLS origin server and an HTTP proxy that honours CONNECT, on ephemeral
ports. Every request line the proxy receives (and any Proxy-Authorization) is
appended to the log file, so the test can assert that an https crawl really
tunneled through the proxy instead of bypassing it.

Usage: proxy-https-server.py <cert.pem> <logfile>
Prints "ORIGIN <port>", "PROXY <port>", then "ready" (one per line) on stdout.
"""
import http.server
import socket
import socketserver
import ssl
import sys
import threading

ORIGIN_BODY = b"<html><body>ORIGIN-PAGE-85</body></html>"


class Origin(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/html")
        self.send_header("Content-Length", str(len(ORIGIN_BODY)))
        self.end_headers()
        self.wfile.write(ORIGIN_BODY)

    def log_message(self, *args):
        pass


def start_origin(certfile):
    httpd = socketserver.TCPServer(("127.0.0.1", 0), Origin)
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(certfile)
    httpd.socket = ctx.wrap_socket(httpd.socket, server_side=True)
    port = httpd.socket.getsockname()[1]
    threading.Thread(target=httpd.serve_forever, daemon=True).start()
    return port


def pipe(src, dst):
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


def handle_client(conn, logfile):
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
    with open(logfile, "a") as handle:
        handle.write(request_line + "\n")
        if auth is not None:
            handle.write("AUTH " + auth + "\n")
    parts = request_line.split()
    if len(parts) >= 2 and parts[0] == "CONNECT":
        host, _, port = parts[1].partition(":")
        try:
            upstream = socket.create_connection((host, int(port or 443)))
        except OSError:
            conn.sendall(b"HTTP/1.0 502 Bad Gateway\r\n\r\n")
            conn.close()
            return
        conn.sendall(b"HTTP/1.0 200 Connection established\r\n\r\n")
        threading.Thread(target=pipe, args=(conn, upstream), daemon=True).start()
        pipe(upstream, conn)
    else:
        conn.sendall(b"HTTP/1.0 501 Not Implemented\r\n\r\n")
        conn.close()


def start_proxy(logfile):
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", 0))
    srv.listen(16)
    port = srv.getsockname()[1]

    def serve():
        while True:
            conn, _ = srv.accept()
            threading.Thread(
                target=handle_client, args=(conn, logfile), daemon=True
            ).start()

    threading.Thread(target=serve, daemon=True).start()
    return port


def main():
    certfile, logfile = sys.argv[1], sys.argv[2]
    open(logfile, "w").close()
    origin_port = start_origin(certfile)
    proxy_port = start_proxy(logfile)
    print("ORIGIN %d" % origin_port, flush=True)
    print("PROXY %d" % proxy_port, flush=True)
    print("ready", flush=True)
    threading.Event().wait()


if __name__ == "__main__":
    main()
