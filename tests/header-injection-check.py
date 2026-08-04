#!/usr/bin/env python3
"""Grade the request bytes header-injection-server.py captured.

Usage: header-injection-check.py <logfile> direct|proxy [port]

Every check is on the literal bytes: httrack's own logs cannot see a request
line that got split in two on the wire.
"""

import sys

MARKER = b"foo:"


def requests(path):
    blob = open(path, "rb").read()
    return [r for r in blob.split(b"=== REQUEST ===\n") if r.strip()]


def lines(req):
    return req.rstrip(b"\n").split(b"\r\n")


def fail(msg, req=None):
    print("FAIL: %s" % msg, file=sys.stderr)
    if req is not None:
        print(repr(req), file=sys.stderr)
    sys.exit(1)


def no_injected_header(reqs):
    """Nothing the site controls may start a line of its own."""
    for req in reqs:
        for line in lines(req)[1:]:
            if line.lower().startswith(MARKER):
                fail("attacker header line reached the wire", req)


def find(reqs, needle):
    return [r for r in reqs if needle in lines(r)[0]]


def check_direct(reqs, port):
    host = b"127.0.0.1:%d" % port

    deep = find(reqs, b"GET /deep.html ")
    if len(deep) != 1:
        fail("expected one /deep.html request, got %d" % len(deep))
    want = b"Referer: http://" + host + b"/x%0d%0aFoo:%20injected.html"
    if want not in lines(deep[0]):
        fail("poisoned referer not sent percent-encoded, wanted %r" % want, deep[0])

    # positive control: an ordinary referer is still emitted, and unmangled
    plain = find(reqs, b"GET /plain.html ")
    if len(plain) != 1:
        fail("expected one /plain.html request, got %d" % len(plain))
    want = b"Referer: http://" + host + b"/"
    if want not in lines(plain[0]):
        fail("ordinary referer missing or malformed, wanted %r" % want, plain[0])


def check_proxy(reqs):
    poison = find(reqs, b"/p.html")
    poison = [r for r in poison if b"evil" in lines(r)[0]]
    if not poison:
        fail("the poisoned host was never requested through the proxy")
    # the authority is lowercased upstream, hence "foo:" and not "Foo:"
    want_line = b"GET http://evil%0d%0afoo:%20injected.example/p.html HTTP/1.1"
    want_host = b"Host: evil%0d%0afoo:%20injected.example"
    for req in poison:
        if lines(req)[0] != want_line:
            fail("proxy request line not escaped, wanted %r" % want_line, req)
        if want_host not in lines(req):
            fail("Host not escaped, wanted %r" % want_host, req)

    # positive control: an ordinary absolute-URI request is unaffected
    plain = find(reqs, b"GET http://plain.example/p.html ")
    if len(plain) != 1:
        fail("expected one plain.example/p.html request, got %d" % len(plain))
    if b"Host: plain.example" not in lines(plain[0]):
        fail("ordinary Host missing or malformed", plain[0])


def main():
    path, mode = sys.argv[1], sys.argv[2]
    reqs = requests(path)
    if not reqs:
        fail("no request was captured at all")
    no_injected_header(reqs)
    if mode == "direct":
        check_direct(reqs, int(sys.argv[3]))
    else:
        check_proxy(reqs)
    print("OK: %s, %d requests, none carrying an injected header" % (mode, len(reqs)))


if __name__ == "__main__":
    main()
