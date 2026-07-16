#!/usr/bin/env python3
"""Local CONNECT-only proxy + plain-HTTP origin for the issue #564 test.

Models a tor HTTPTunnelPort: an HTTP proxy that honours CONNECT and rejects
every other method with 501 (the classic absolute-URI form a normal HTTP proxy
accepts). Fronting it is a plain-HTTP origin. The proxy logs each request line
(and any Proxy-Authorization); the origin logs its request line and headers.
That lets the test assert a plain-http crawl tunneled through CONNECT, arrived
origin-form (not "GET http://host/..."), and never leaked proxy credentials.

Proxy modes (argv[2], default "ok"):
  ok    - honour CONNECT and tunnel to the origin
  flood - answer 200 then stream headers forever with no blank line, to exercise
          the client's bound on the proxy response (must not hang the crawl)

Usage: proxy-connect-server.py <logdir> [mode]
Prints "ORIGIN <port>", "PROXY <port>", then "ready" (one per line) on stdout.
"""
import sys

import proxytestlib

ORIGIN_BODY = b"<html><body>ORIGIN-PAGE-564</body></html>"


def main():
    logdir = sys.argv[1]
    mode = sys.argv[2] if len(sys.argv) > 2 else "ok"
    proxytestlib.serve(logdir, ORIGIN_BODY, 80, mode)


if __name__ == "__main__":
    main()
