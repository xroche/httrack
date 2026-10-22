#!/usr/bin/env python3
"""Raw-socket probe for the header-injection and header-dedup tests.

Speaks HTTP off the socket so a split header line stays visible; serves an
origin-form site and an http-proxy absolute-URI one on the same port.

Appends "=== REQUEST ===\\n<bytes>\\n" per request to the log named on argv, and
writes each request verbatim to <log>.<n> as well, so a consumer grading line
endings never has to split the log with a tool that may translate them. Prints
"PORT <n>" once listening.
"""

import os
import socket
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from proxytestlib import bind_ephemeral  # noqa: E402

# &#13;&#10; outlives the parser's control-byte escape, so these land as raw CR/LF
POISON_PATH = "/x&#13;&#10;Foo:%20injected.html"
POISON_HOST = "evil&#13;&#10;Foo:%20injected.example"
FTP_POISON_HOST = "ftpevil&#13;&#10;Foo:%20x.example"


def page(body):
    return ("<html><body>%s</body></html>" % body).encode()


def root_page(port):
    return page(
        '<a href="http://127.0.0.1:%d%s">poison</a>'
        '<a href="http://127.0.0.1:%d/plain.html">plain</a>' % (port, POISON_PATH, port)
    )


# a password in every form the engine reads one (657)
PLANTED = (
    "http://luser:LINKPWA@127.0.0.1:%(p)d/l1.html",
    "//luser:LINKPWB@127.0.0.1:%(p)d/l2.html",
    "http://luser:LINKPWC1 LINKPWC2@127.0.0.1:%(p)d/l3.html",
    "http://luser:LINKPWD;q@127.0.0.1:%(p)d/l4.html",
    "http://luser:LINKPWE@LINKPWF@127.0.0.1:%(p)d/l5.html",
    "http://luser:LINKPWG%%40q@127.0.0.1:%(p)d/l6.html",
    "http://luser:LINKPWH@[::1]:1/l7.html",
    "http://luser:LINKPWI@127.0.0.1:%(p)d",
    "http://luser:LINKPWJ@127.0.0.1:1/l8.html",
    "mms://luser:SCHEMEPWM@127.0.0.1:%(p)d/m.asf",
    "gopher://luser:SCHEMEPWG@127.0.0.1:%(p)d/g",
    "  gopher://luser:SCHEMEPWL@127.0.0.1:%(p)d/g2",
    "Mms://luser:SCHEMEPWC@127.0.0.1:%(p)d/m2.asf",
    "HTTP://luser:LINKPWK@127.0.0.1:1/l9.html",
    "x-y.z+w://luser:RELPW@h.example/a",
    "/redir.html",
)
# the image loads while the engine waits on slow.html, so it is found ready
PLANTED_FIRST = (
    '<a href="/slow.html">s</a>'
    '<img src="http://luser:LINKPWO@127.0.0.1:%(p)d/i.gif">'
    "<style>body{background:url(http://luser:LINKPWU@127.0.0.1:%(p)d/bg.png)}</style>"
    '<img data-src="http://u:DATAPW@127.0.0.1:%(p)d/{w}.jpg" src="/i.gif">'
)


def planted_page(port):
    links = "".join('<a href="%s">l</a>' % (u % {"p": port}) for u in PLANTED)
    return page(PLANTED_FIRST % {"p": port} + links)


# the poisoned page must link onward, or its URL never becomes a Referer
POISON_PAGE = page('<a href="/deep.html">deep</a>')
# the ftp link takes the ftp-through-proxy emission site, which has no Host:
PROXY_ROOT = page(
    '<a href="ftp://' + FTP_POISON_HOST + '/f.txt">ftp</a>'
    '<a href="http://' + POISON_HOST + '/p.html">poison</a>'
    '<a href="http://plain.example/p.html">plain</a>'
)


def body_for(request, port):
    """Pick a body from the request line alone; a split one just gets a leaf."""
    line = request.split(b"\r\n", 1)[0]
    if b"start.example" in line:
        return PROXY_ROOT
    if line.startswith(b"GET / "):
        return root_page(port)
    if b"/planted.html" in line:
        return planted_page(port)
    if b"/x" in line:
        return POISON_PAGE
    # one hop, so the second request of a crawl carries a Referer (328)
    if b"/hop.html" in line:
        return page('<a href="/plain.html">plain</a>')
    # the proxied pages link onward too, so a poisoned *host* becomes a referer
    if b"/p.html" in line:
        return page('<a href="/deep2.html">deep</a>')
    # the passwords of the start URL and of a link, and a -%X token, come back
    # in a query, where the log's backstop hides only the token
    if b"/token.html" in line:
        return page(
            '<a href="http://luser:LINKTOKQ2@127.0.0.1:%d/leaf.html">l</a>'
            '<a href="/leaf.html?k=TOKENPWQ1">t</a>'
            '<a href="/leaf.html?l=LINKTOKQ2">u</a>'
            '<a href="/leaf.html?h=HDRTOKQ3">h</a>' % port
        )
    # sets a cookie and links on, so the next request sends it back (657)
    if b"/setcookie.html" in line:
        return page('<a href="/plain.html">plain</a>')
    return page("leaf")


def extra_headers(request, port):
    line = request.split(b"\r\n", 1)[0]
    if b"/redir.html" in line:
        return b"Location: http://ruser:LINKPWR@127.0.0.1:%d/hop.html\r\n" % port
    if b"/setcookie.html" in line:
        # a blank before the colon and a folded line still belong to the field
        return (
            b"Set-Cookie: srvsess=SETCOOKIESECRET; Path=/\r\n"
            b"set-cookie\t: spaced=SPACEDSECRET;\r\n\tfolded=FOLDEDSECRET\r\n"
            b"X-Visible: shown\r\n"
            b"X-Cookie-Id: VISIBLE2\r\n"
            b"Authorization-Info: VISIBLE4\r\n"
        )
    return b""


def handle(conn, port, logf, lock, seq):
    conn.settimeout(10)
    data = b""
    try:
        while b"\r\n\r\n" not in data:
            chunk = conn.recv(4096)
            if not chunk:
                break
            data += chunk
    except (OSError, socket.timeout):
        pass
    if data:
        with lock:
            seq[0] += 1
            logf.write(b"=== REQUEST ===\n" + data + b"\n")
            logf.flush()
            with open("%s.%d" % (sys.argv[1], seq[0]), "wb") as reqf:
                reqf.write(data)
        if b"/slow.html" in data.split(b"\r\n", 1)[0]:
            time.sleep(2)
        body = body_for(data, port)
        extra = extra_headers(data, port)
        status = b"302 Found" if extra.startswith(b"Location:") else b"200 OK"
        # a real image, which the engine finishes in the background (657)
        image = b".gif " in data.split(b"\r\n", 1)[0]
        if image:
            body = b"GIF89a"
        try:
            conn.sendall(
                b"HTTP/1.1 "
                + status
                + b"\r\nContent-Type: "
                + (b"image/gif" if image else b"text/html")
                + b"\r\n"
                + extra
                + b"Content-Length: %d\r\nConnection: close\r\n\r\n" % len(body)
                + body
            )
        except OSError:
            pass
    try:
        conn.close()
    except OSError:
        pass


def main():
    srv, port = bind_ephemeral()
    lock = threading.Lock()
    seq = [0]
    with open(sys.argv[1], "wb") as logf:
        sys.stdout.reconfigure(newline="\n")  # the launcher parses PORT, CRLF breaks it
        print("PORT %d" % port, flush=True)
        while True:
            conn, _ = srv.accept()
            threading.Thread(
                target=handle, args=(conn, port, logf, lock, seq), daemon=True
            ).start()


if __name__ == "__main__":
    main()
