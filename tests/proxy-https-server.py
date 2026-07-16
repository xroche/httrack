#!/usr/bin/env python3
"""Local CONNECT proxy + self-signed HTTPS origin for the issue #85 test.

Starts a TLS origin server and an HTTP proxy that honours CONNECT, on ephemeral
ports. Every request line the proxy receives (and any Proxy-Authorization) is
appended to the proxy log; every header the origin receives over the tunnel is
appended to the origin log. That lets the test assert both that an https crawl
tunneled through the proxy and that proxy credentials never leaked to the origin.

Proxy modes (argv[3], default "ok"):
  ok    - honour CONNECT and tunnel to the origin
  flood - answer 200 then stream headers forever with no blank line, to exercise
          the client's bound on the proxy response (must not hang the crawl)

Usage: proxy-https-server.py <cert.pem> <logdir> [mode]
Prints "ORIGIN <port>", "PROXY <port>", then "ready" (one per line) on stdout.
"""
import sys

import proxytestlib

ORIGIN_BODY = b"<html><body>ORIGIN-PAGE-85</body></html>"


def main():
    certfile, logdir = sys.argv[1], sys.argv[2]
    mode = sys.argv[3] if len(sys.argv) > 3 else "ok"
    proxytestlib.serve(logdir, ORIGIN_BODY, 443, mode, certfile=certfile)


if __name__ == "__main__":
    main()
