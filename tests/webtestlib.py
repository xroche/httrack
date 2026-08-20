#!/usr/bin/env python3
"""Session-aware htsserver client for the WebHTTrack tests.

urllib, not httpclient.py: these tests drive the GUI as a browser does, over
several requests sharing one session, and assert on the rendered HTML rather
than on status-line or header bytes.
"""
import re
import sys
import urllib.parse
import urllib.request

DEFAULT_TIMEOUT = 20

# The server renders its session id into every form as 32 hex digits.
SID_RE = re.compile(r'name="sid" value="([0-9a-f]+)"')


class Session:
    """One browser session against the htsserver at URL."""

    def __init__(self, url, timeout=DEFAULT_TIMEOUT):
        self.url = url.rstrip("/")
        self.timeout = timeout
        self._sid = None

    def get_bytes(self, path):
        if not path.startswith("/"):
            path = "/server/" + path
        return urllib.request.urlopen(self.url + path, timeout=self.timeout).read()

    def get(self, path):
        # The GUI is served ISO-8859-1, so decode, do not assume UTF-8.
        return self.get_bytes(path).decode("latin-1")

    @property
    def sid(self):
        if self._sid is None:
            m = SID_RE.search(self.get("/server/index.html"))
            if m is None:
                sys.exit("no session id in server/index.html")
            self._sid = m.group(1)
        return self._sid

    def post(self, page, fields, with_sid=True):
        """POST urlencoded FIELDS to /server/PAGE and return the reply.

        Only the fields given are sent. A field the page owns but the caller
        omits keeps whatever the previous POST left in the session, which
        several tests depend on, so this never replays or clears one.
        """
        if with_sid:
            fields = [("sid", self.sid)] + list(fields)
        body = "&".join("%s=%s" % (k, urllib.parse.quote(v)) for k, v in fields)
        if not page.startswith("/"):
            page = "/server/" + page
        req = urllib.request.Request(
            self.url + page, data=body.encode("latin-1"), method="POST"
        )
        return (
            urllib.request.urlopen(req, timeout=self.timeout).read().decode("latin-1")
        )


def textarea(page, name):
    """The content of textarea NAME in the rendered PAGE, as a browser posts it."""
    m = re.search(r'<textarea name="%s".*?>(.*?)</textarea>' % name, page, re.S)
    if m is None:
        sys.exit("no %s textarea in the rendered page" % name)
    # The newline right after the tag is markup, not content.
    return m.group(1).lstrip("\r\n")


def checked(page, name):
    """Whether checkbox NAME is checked in the rendered PAGE."""
    return re.search(r'name="%s"[ \t]*checked' % name, page) is not None


def field(page, name):
    """The value attribute of input NAME in the rendered PAGE, or None."""
    m = re.search(r'name="%s"[^>]*\bvalue="([^"]*)"' % name, page)
    return m.group(1) if m else None
