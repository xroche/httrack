#!/usr/bin/env python3
"""Raw HTTP/1.0 client for the htsserver tests.

Speaks the socket directly: the tests assert on status-line and header bytes
urllib parses away, and need a reply even from a headerless fragment.
"""
import argparse
import socket
import sys
import urllib.parse


def host_header(args):
    """The Host line, or none at all when --host is empty: an HTTP/1.0 client
    may send none, and the server has to answer that request too."""
    return "Host: %s\r\n" % args.host if args.host else ""


def build_request(args):
    if args.field or args.post is not None:
        if args.field:
            # urlencode, not quote(): a browser sends "+" for a space, and the
            # server decodes that on its own branch.
            body = urllib.parse.urlencode([tuple(f.split("=", 1)) for f in args.field])
        else:
            body = args.post
        return (
            "POST %s HTTP/1.0\r\n%s"
            "Content-type: application/x-www-form-urlencoded\r\n"
            "Content-length: %d\r\n\r\n%s"
            % (args.path or "/", host_header(args), len(body), body)
        )
    return "GET %s HTTP/1.0\r\n%s\r\n" % (
        args.path or "/server/index.html",
        host_header(args),
    )


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--port", type=int, required=True)
    p.add_argument("--path")
    p.add_argument("--post", help="raw urlencoded body; POSTs it")
    p.add_argument(
        "--field",
        action="append",
        metavar="K=V",
        help="POST field, form-encoded here; repeatable",
    )
    p.add_argument(
        "--host",
        default="127.0.0.1",
        help="Host header value; empty sends none",
    )
    p.add_argument("--timeout", type=float, default=30)
    p.add_argument(
        "--headers-only",
        action="store_true",
        help="stop at the end of the header block; the server need not close",
    )
    p.add_argument("--head-file", help="write the header block here, not to stdout")
    p.add_argument("--body-file", help="write the body here, not to stdout")
    args = p.parse_args()

    s = socket.create_connection(("127.0.0.1", args.port), 10)
    s.settimeout(args.timeout)
    s.sendall(build_request(args).encode())
    out = b""
    try:
        while True:
            if args.headers_only and b"\r\n\r\n" in out:
                break
            chunk = s.recv(65536)
            if not chunk:
                break
            out += chunk
    except socket.timeout:
        # Exit rather than hang: a wedged server is the failure under test.
        sys.stderr.write("timed out after %d bytes\n" % len(out))
        sys.exit(9)
    s.close()

    if args.headers_only:
        out = out.split(b"\r\n\r\n")[0]
    if args.head_file or args.body_file:
        head, _, body = out.partition(b"\r\n\r\n")
        if args.head_file:
            open(args.head_file, "wb").write(head)
        if args.body_file:
            open(args.body_file, "wb").write(body)
        return
    # Bytes through: the GUI has been UTF-8 since #1407, and decoding as
    # latin-1 re-encoded every non-ASCII reply on the way out.
    sys.stdout.buffer.write(out)


main()
