#!/usr/bin/env python3
"""Grade the request bytes header-injection-server.py captured.

Usage: header-injection-check.py <logfile> direct|proxy [port]
"""

import sys

MARKER = b"foo:"
POISON_HOST = b"evil%0d%0afoo:%20injected.example"
FTP_POISON_HOST = b"ftpevil%0d%0afoo:%20x.example"


def requests(path):
    blob = open(path, "rb").read()
    return [r for r in blob.split(b"=== REQUEST ===\n") if r.strip()]


def lines(req):
    """Split the way a tolerant server would, so a bare LF also ends a line."""
    return [line.rstrip(b"\r") for line in req.rstrip(b"\n").split(b"\n")]


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
    # the authority is lowercased upstream, hence "foo:" and not "Foo:"
    poison = find(reqs, b"/p.html")
    poison = [r for r in poison if b"evil" in lines(r)[0]]
    if not poison:
        fail("the poisoned host was never requested through the proxy")
    want_line = b"GET http://" + POISON_HOST + b"/p.html "
    want_host = b"Host: " + POISON_HOST
    for req in poison:
        if not lines(req)[0].startswith(want_line):
            fail("proxy request line not escaped, wanted %r" % want_line, req)
        if want_host not in lines(req):
            fail("Host not escaped, wanted %r" % want_host, req)

    # the poisoned host must also reach a Referer, or that escape goes ungraded
    deep = find(reqs, b"http://" + POISON_HOST + b"/deep2.html")
    if len(deep) != 1:
        fail("expected one poisoned-host deep2.html request, got %d" % len(deep))
    want = b"Referer: http://" + POISON_HOST + b"/p.html"
    if want not in lines(deep[0]):
        fail("poisoned referer host not escaped, wanted %r" % want, deep[0])

    # ftp through an http proxy is built by its own emission site
    ftp = find(reqs, b"ftp://")
    if len(ftp) != 1:
        fail("expected one ftp-through-proxy request, got %d" % len(ftp))
    want = b"GET ftp://" + FTP_POISON_HOST + b"/f.txt "
    if not lines(ftp[0])[0].startswith(want):
        fail("ftp request line not escaped, wanted %r" % want, ftp[0])

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
