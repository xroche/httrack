#!/usr/bin/env python3
"""Local SOCKS5 proxy (RFC 1928/1929) + TLS and plain HTTP origins for #563.

Logs the auth method, any user/pass, and the CONNECT ATYP + literal domain, so
the test proves httrack sent a hostname (remote DNS) and negotiated auth.
Modes (argv[3]): ok | refuse (REP=5) | truncate (partial reply, must not hang).
Usage: socks5-server.py <cert.pem> <logdir> [mode]
Prints "TLS <port>", "HTTP <port>", "SOCKS <port>", "ready" on stdout.
"""
import http.server
import os
import socket
import socketserver
import ssl
import struct
import sys
import threading

# The one name the proxy answers for; a .invalid TLD never resolves (RFC 6761),
# so a locally-resolving client could not reach us -- success proves remote DNS.
REMOTE_HOST = b"socks-origin.invalid"
ORIGIN_BODY = b"<html><body>ORIGIN-PAGE-563</body></html>"
SOCKS_LOG = "socks.log"
ORIGIN_LOG = "origin-headers.log"


def make_origin(logdir):
    class Origin(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            with open(os.path.join(logdir, ORIGIN_LOG), "a") as handle:
                handle.write(
                    "REQUEST %s %s %s\n"
                    % (self.command, self.path, self.request_version)
                )
                for key in self.headers.keys():
                    handle.write(key + ": " + self.headers[key] + "\n")
            self.send_response(200)
            self.send_header("Content-Type", "text/html")
            self.send_header("Content-Length", str(len(ORIGIN_BODY)))
            self.end_headers()
            self.wfile.write(ORIGIN_BODY)

        def log_message(self, *args):
            pass

    return Origin


def start_origin(logdir, certfile=None):
    httpd = socketserver.TCPServer(("127.0.0.1", 0), make_origin(logdir))
    if certfile is not None:
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ctx.load_cert_chain(certfile)
        httpd.socket = ctx.wrap_socket(httpd.socket, server_side=True)
    port = httpd.socket.getsockname()[1]
    threading.Thread(target=httpd.serve_forever, daemon=True).start()
    return port


def recvn(conn, n):
    buf = b""
    while len(buf) < n:
        chunk = conn.recv(n - len(buf))
        if not chunk:
            raise OSError("short read")
        buf += chunk
    return buf


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


def log(logdir, line):
    with open(os.path.join(logdir, SOCKS_LOG), "a") as handle:
        handle.write(line + "\n")


def negotiate_auth(conn, logdir):
    """RFC 1928 greeting + RFC 1929 sub-negotiation. Returns True to proceed."""
    ver, nmethods = recvn(conn, 2)
    if ver != 0x05:
        return False
    methods = recvn(conn, nmethods)
    if 0x02 in methods:  # prefer user/pass so the auth test exercises RFC 1929
        conn.sendall(b"\x05\x02")
        (subver,) = recvn(conn, 1)
        (ulen,) = recvn(conn, 1)
        uname = recvn(conn, ulen)
        (plen,) = recvn(conn, 1)
        passwd = recvn(conn, plen)
        log(logdir, "METHOD userpass")
        log(logdir, "AUTH %s %s" % (uname.decode(), passwd.decode()))
        conn.sendall(b"\x01\x00")  # RFC 1929 success
    elif 0x00 in methods:
        conn.sendall(b"\x05\x00")
        log(logdir, "METHOD noauth")
    else:
        conn.sendall(b"\x05\xff")  # no acceptable method
        return False
    return True


def read_request(conn, logdir):
    """RFC 1928 CONNECT. Logs CMD, ATYP, domain/addr, port. Returns dest port."""
    ver, cmd, _rsv, atyp = recvn(conn, 4)
    log(logdir, "CMD %d" % cmd)
    if atyp == 0x01:
        addr = socket.inet_ntoa(recvn(conn, 4))
        log(logdir, "ATYP ipv4 %s" % addr)
    elif atyp == 0x03:
        (dlen,) = recvn(conn, 1)
        domain = recvn(conn, dlen)
        log(logdir, "ATYP domain %s" % domain.decode())
    elif atyp == 0x04:
        recvn(conn, 16)
        log(logdir, "ATYP ipv6")
        domain = b""
    else:
        return None, None
    (port,) = struct.unpack(">H", recvn(conn, 2))
    log(logdir, "PORT %d" % port)
    name = domain if atyp == 0x03 else b""
    return name, port


def reply(conn, rep):
    conn.sendall(bytes([0x05, rep, 0x00, 0x01]) + b"\x00\x00\x00\x00" + b"\x00\x00")


def handle_socks(conn, logdir, mode):
    try:
        if not negotiate_auth(conn, logdir):
            conn.close()
            return
        if mode == "truncate":
            # one byte of the 10-byte reply, then hold the socket: the client
            # must bound this handshake read and give up, not hang.
            conn.sendall(b"\x05")
            threading.Event().wait()
            return
        name, port = read_request(conn, logdir)
        if mode == "refuse" or name != REMOTE_HOST or port is None:
            reply(conn, 0x05)  # connection refused
            conn.close()
            return
        try:
            upstream = socket.create_connection(("127.0.0.1", port))
        except OSError:
            reply(conn, 0x05)
            conn.close()
            return
        reply(conn, 0x00)  # succeeded
        threading.Thread(target=pipe, args=(conn, upstream), daemon=True).start()
        pipe(upstream, conn)
    except OSError:
        try:
            conn.close()
        except OSError:
            pass


def start_socks(logdir, mode):
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", 0))
    srv.listen(16)
    port = srv.getsockname()[1]

    def serve():
        while True:
            conn, _ = srv.accept()
            threading.Thread(
                target=handle_socks, args=(conn, logdir, mode), daemon=True
            ).start()

    threading.Thread(target=serve, daemon=True).start()
    return port


def main():
    certfile, logdir = sys.argv[1], sys.argv[2]
    mode = sys.argv[3] if len(sys.argv) > 3 else "ok"
    for name in (SOCKS_LOG, ORIGIN_LOG):
        open(os.path.join(logdir, name), "w").close()
    tls_port = start_origin(logdir, certfile)
    http_port = start_origin(logdir, None)
    socks_port = start_socks(logdir, mode)
    print("TLS %d" % tls_port, flush=True)
    print("HTTP %d" % http_port, flush=True)
    print("SOCKS %d" % socks_port, flush=True)
    print("ready", flush=True)
    threading.Event().wait()


if __name__ == "__main__":
    main()
