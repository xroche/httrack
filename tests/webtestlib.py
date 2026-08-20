#!/usr/bin/env python3
"""Session-aware htsserver client for the WebHTTrack tests.

urllib, not httpclient.py: these tests drive the GUI like a browser, over
several requests that share one session. They assert on the rendered HTML,
not on the status-line and header bytes urllib parses away.
"""
import re
import sys
import urllib.parse
import urllib.request

DEFAULT_TIMEOUT = 20

# The server renders its session id into every form as 32 hex digits.
SID_RE = re.compile(r'name="sid" value="([0-9a-f]+)"')


class Session:
    """One browser session against the htsserver at URL.

    Paths are page names under /server/, the only place the GUI lives.
    """

    def __init__(self, url, timeout=DEFAULT_TIMEOUT):
        self.url = url.rstrip("/")
        self.timeout = timeout
        self._sid = None

    def _open(self, page, data=None):
        req = urllib.request.Request(
            self.url + "/server/" + page, data=data, method="POST" if data else "GET"
        )
        # The GUI is served ISO-8859-1, so decode, do not assume UTF-8.
        return (
            urllib.request.urlopen(req, timeout=self.timeout).read().decode("latin-1")
        )

    def get(self, page):
        return self._open(page)

    @property
    def sid(self):
        if self._sid is None:
            m = SID_RE.search(self.get("index.html"))
            if m is None:
                sys.exit("no session id in server/index.html")
            self._sid = m.group(1)
        return self._sid

    def post(self, page, fields):
        """POST urlencoded FIELDS to /server/PAGE, with the session id, and
        return the reply.

        Only the fields given are sent. A field the page owns but the POST
        omits keeps its previous value in the session. Several tests rely on
        that, so post() never replays or clears a field.
        """
        fields = [("sid", self.sid)] + list(fields)
        body = "&".join("%s=%s" % (k, urllib.parse.quote(v)) for k, v in fields)
        return self.post_raw(page, body)

    def post_raw(self, page, body):
        """POST BODY to /server/PAGE verbatim: no encoding, no session id.

        For a test sending bytes post() would encode away, or an absent or
        deliberately wrong session id.
        """
        return self._open(page, data=body.encode("latin-1"))


def textarea(page, name):
    """The content of textarea NAME in the rendered PAGE, as a browser posts it.

    Exits when absent: every caller asserts on the content.
    """
    # [ >] anchors the name: unanchored, "command" also matches "commandline".
    m = re.search(r'<textarea name="%s"[ >].*?>(.*?)</textarea>' % name, page, re.S)
    if m is None:
        sys.exit("no %s textarea in the rendered page" % name)
    # One newline right after the tag is markup, not content. Only one: a second
    # is a blank first line, which a test may be asserting on.
    return re.sub(r"\A\r?\n", "", m.group(1))


def checked(page, name):
    """Whether checkbox NAME is checked in the rendered PAGE."""
    return re.search(r'name="%s"[ \t]*checked' % name, page) is not None


def field(page, name):
    """The value attribute of input NAME in the rendered PAGE, or None.

    Returns rather than exits, unlike textarea(): callers test for absence.
    """
    m = re.search(r'name="%s"[^>]*\bvalue="([^"]*)"' % name, page)
    return m.group(1) if m else None
