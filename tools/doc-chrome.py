#!/usr/bin/env python3
"""Generate the shared chrome (head, masthead, sidebar, footer) of the html/ pages.

The documentation is read from disk as often as over http, so it cannot use server
includes: the navigation is real markup in every page, written here and verified by
CI. Pages opt in by carrying the doc-chrome markers, mandatory once a page loads
doc.css; --convert adds them to a page still using the 2007 table layout.

    tools/doc-chrome.py                 rewrite the chrome regions in place
    tools/doc-chrome.py --check         fail if any page is out of sync
    tools/doc-chrome.py --convert FILE  migrate a legacy page, then rewrite it
"""

import argparse
import os
import re
import sys

HTML = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "html")

# The sidebar, and the labels this page set calls itself by. Editing this list
# updates every page on the next run.
NAV = [
    (
        "Start here",
        [
            ("guide.html", "Interface guide"),
            ("faq.html", "FAQ and troubleshooting"),
            ("abuse.html", "Using HTTrack responsibly"),
        ],
    ),
    (
        "Command line",
        [
            ("cmdguide.html", "Command-line guide"),
            ("httrack.man.html", "Option reference"),
            ("filters.html", "Filter syntax"),
        ],
    ),
    (
        "Developers",
        [
            ("dev.html", "Programming"),
            ("library.html", "libhttrack API"),
            ("plug.html", "Callbacks"),
            ("scripting.html", "Scripting"),
            ("cache.html", "Cache format"),
            ("changes.html", "Change report format"),
        ],
    ),
    (
        "More",
        [
            ("fcguide.html", "Users Guide (3.10)"),
            ("contact.html", "Contact and credits"),
            ("index.html", "All documentation"),
        ],
    ),
]

# Every legacy page carried the same title, "HTTrack Website Copier - Offline
# Browser", which tells a browser tab or a search result nothing.
TITLES = {
    "abuse.html": "Using HTTrack responsibly",
    "cache.html": "HTTrack cache format",
    "changes.html": "HTTrack change report format",
    "cmdguide.html": "HTTrack command-line guide",
    "contact.html": "Contact and credits",
    "dev.html": "Programming with HTTrack",
    "faq.html": "HTTrack FAQ and troubleshooting",
    "fcguide.html": "HTTrack Users Guide, by Fred Cohen (3.10)",
    "filters.html": "HTTrack filter syntax",
    "library.html": "The libhttrack library",
    "plug.html": "HTTrack callbacks and plugins",
    "plug_330.html": "HTTrack callbacks, 3.30 and earlier",
    "scripting.html": "Scripting HTTrack",
}

# A page-specific description, so a search result says what the page is. The old
# pages shared one blurb (with its word-join typos) and a keyword list that still
# advertised Windows 95 and AIX 4.0; keywords are dropped, nothing reads them.
DESCRIPTIONS = {
    "abuse.html": "How to mirror a site without hammering the server, and what a"
    " webmaster can do about crawlers that do.",
    "cache.html": "The format of the HTTrack cache in hts-cache, and how to read it.",
    "changes.html": "The change report HTTrack writes after an update, field by field.",
    "cmdguide.html": "A task-oriented guide to the httrack command line: scope,"
    " filters, limits, logins, proxies, updates and recipes.",
    "contact.html": "How to reach the HTTrack project, and who contributed to it.",
    "dev.html": "Using HTTrack from a script, from the callbacks, or through the"
    " libhttrack library.",
    "faq.html": "Frequently asked questions about HTTrack, and what to do when a"
    " site does not mirror the way you expected.",
    "fcguide.html": "Fred Cohen's HTTrack Users Guide, written for release 3.10.",
    "filters.html": "HTTrack filter syntax: wildcards, and the size and MIME-type"
    " forms of a scan rule.",
    "library.html": "Linking against the libhttrack library.",
    "plug.html": "Plugging your own C functions into the HTTrack engine.",
    "plug_330.html": "The HTTrack callback API as it was in release 3.30 and earlier.",
    "scripting.html": "Driving the httrack command-line program from shell and" " batch scripts.",
}

# Year left static: --check diffs the regenerated chrome against the shipped
# bytes, so a computed one would red CI every 1st of January.
FOOTER = [
    "<footer>",
    "\t<small>&copy; 1998-2026 Xavier Roche &amp; other contributors"
    " - Web Design: Leto Kauler.</small>",
    # Local copy: an absolute src breaks the image offline and makes a network request.
    '\t<a href="https://endsoftwarepatents.org/innovating-without-patents/"'
    ' target="_blank" rel="noopener"'
    ' title="This site is innovating without patents!">'
    '<img src="images/esp_chicklet.png" width="91" height="17"'
    ' alt="End Software Patents"></a>',
    "</footer>",
]

# Identical on every page, sidebar or not, so it is its own region: pages that
# build their own navigation still get their masthead checked (#1115).
MASTHEAD = [
    '<a class="skip" href="#main">Skip to content</a>',
    "",
    '<header class="masthead">',
    '\t<img src="images/wordmark.svg" width="400" height="36"'
    ' alt="HTTrack Website Copier">',
    '\t<div class="tagline">Free software offline browser</div>',
    "</header>",
]

MARK = {
    part: (f"<!-- doc-chrome:{part} -->", f"<!-- /doc-chrome:{part} -->")
    for part in ("head", "masthead", "top", "bottom", "footer")
}


def region(text, part, body):
    """Replace a marked region, keeping the markers."""
    open_, close = MARK[part]
    pattern = re.compile(re.escape(open_) + ".*?" + re.escape(close), re.S)
    if not pattern.search(text):
        raise SystemExit(f"missing {open_} marker")
    return pattern.sub(open_ + "\n" + body.rstrip("\n") + "\n" + close, text, count=1)


def slug(heading):
    plain = re.sub(r"<[^>]+>", "", heading)
    plain = re.sub(r"&[a-z]+;|&#\d+;", " ", plain)
    return re.sub(r"[^a-z0-9]+", "-", plain.lower()).strip("-")[:40] or None


def sections(content, level=2):
    """Section list for the sidebar, with ids added to headings that lack one.

    Legacy pages built their sections out of <h3> under a single centred title, so
    fall back a level when there are no <h2> to list.
    """
    out = []
    seen = set()

    def visit(match):
        attrs, inner = match.group(1), match.group(2)
        found = re.search(r'id="([^"]+)"', attrs)
        ident = found.group(1) if found else slug(inner)
        if not ident or ident in seen:
            return match.group(0)
        seen.add(ident)
        label = re.sub(r"\s+", " ", re.sub(r"<[^>]+>", "", inner)).strip(" :\t\n")
        out.append((ident, label))
        if found:
            return match.group(0)
        return f'<h{level} id="{ident}"{attrs}>{inner}</h{level}>'

    content = re.sub(rf"<h{level}([^>]*)>(.*?)</h{level}>", visit, content, flags=re.S)
    # One heading is a page title, not a section; a dozen is a table of contents.
    if 2 <= len(out) <= 12:
        return content, out
    if level == 2:
        return sections(content, 3)
    return content, []


def chrome(page, content):
    head = [
        '<meta charset="utf-8">',
        '<meta name="viewport" content="width=device-width, initial-scale=1">',
    ]
    if page in DESCRIPTIONS:
        head.append(f'<meta name="description" content="{DESCRIPTIONS[page]}">')
    head += [
        '<link rel="stylesheet" href="doc.css">',
        '<script src="doc.js" defer></script>',
    ]

    nav = ['<nav class="toc" aria-label="Documentation">']
    content, here = sections(content)
    if here:
        nav.append("\t<h2>On this page</h2>")
        nav.append("\t<ul>")
        nav += [f'\t\t<li><a href="#{i}">{label}</a></li>' for i, label in here]
        nav.append("\t</ul>")
    for group, entries in NAV:
        nav.append(f"\t<h2>{group}</h2>")
        nav.append("\t<ul>")
        for target, label in entries:
            current = ' aria-current="page" class="here"' if target == page else ""
            nav.append(f'\t\t<li><a href="{target}"{current}>{label}</a></li>')
        nav.append("\t</ul>")
    nav.append("</nav>")

    # Marked inside top too, so every page exposes httrack.com's injection point.
    top = [
        MARK["masthead"][0],
        *MASTHEAD,
        MARK["masthead"][1],
        "",
        '<div class="wrap">',
        "",
        "\n".join(nav),
        "",
        '<main id="main">',
    ]

    bottom = [
        "</main>",
        "</div>",
        "",
        '<dialog id="zoom" aria-label="Enlarged image"><img src="" alt=""></dialog>',
    ]

    return content, "\n".join(head), "\n".join(top), "\n".join(bottom)


LEGACY = re.compile(r"<!--=*\s*End prologue\s*=*-->(.*?)<!--=*\s*Start epilogue\s*=*-->", re.S)


def convert(path):
    """Turn a 2007-layout page into the marker skeleton, content untouched."""
    with open(path, encoding="utf-8") as fp:
        text = fp.read()
    if MARK["top"][0] in text:
        return text
    body = LEGACY.search(
        text.replace("==================== ", "").replace(" ====================", "")
    )
    if not body:
        raise SystemExit(f"{path}: no prologue/epilogue markers to convert")
    content = body.group(1).strip("\n")

    # contact.html defines its mailto obfuscator in the head; keep any such script.
    scripts = re.findall(r"<script\b.*?</script>", text.split("</head>")[0], re.S)

    page = os.path.basename(path)
    title = TITLES.get(page) or re.search(r"<title>(.*?)</title>", text, re.S).group(1)

    # The centered heading the legacy shell used as a page title is the real <h1>.
    content = re.sub(
        r'<h2 align="center">\s*(?:<em>)?(.*?)(?:</em>)?\s*</h2>',
        lambda m: f"<h1>{m.group(1).strip()}</h1>",
        content,
        count=1,
        flags=re.S,
    )
    # Presentational leftovers the stylesheet now covers.
    content = re.sub(r"</?font[^>]*>", "", content)
    content = content.replace('class="tblHeaderColor"', 'class="head"')
    content = re.sub(r'\s*class="tbl(Regular|NoBorder)"', "", content)

    return "\n".join(
        [
            "<!DOCTYPE html>",
            '<html lang="en">',
            "<head>",
            MARK["head"][0],
            MARK["head"][1],
            f"<title>{title}</title>",
            *scripts,
            "</head>",
            "<body>",
            "",
            MARK["top"][0],
            MARK["top"][1],
            "",
            content,
            "",
            MARK["bottom"][0],
            MARK["bottom"][1],
            "",
            MARK["footer"][0],
            MARK["footer"][1],
            "",
            "</body>",
            "</html>",
            "",
        ]
    )


def rewrite(path, text):
    page = os.path.basename(path)
    text = region(text, "footer", "\n".join(FOOTER))
    # A page owning its sidebar takes the masthead alone, and nothing else here
    # applies to it: no head, no wrap, no generated navigation.
    if MARK["top"][0] not in text:
        return region(text, "masthead", "\n".join(MASTHEAD))
    open_, close = MARK["top"]
    body = text.split(close, 1)[1].split(MARK["bottom"][0], 1)[0]
    content, head, top, bottom = chrome(page, body)
    text = text.replace(close + body + MARK["bottom"][0], close + content + MARK["bottom"][0], 1)
    text = region(text, "head", head)
    text = region(text, "top", top)
    return region(text, "bottom", bottom)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="report drift, change nothing")
    ap.add_argument("--convert", nargs="*", metavar="FILE", help="migrate legacy pages first")
    args = ap.parse_args()

    for name in args.convert or []:
        path = name if os.path.isabs(name) else os.path.join(HTML, os.path.basename(name))
        text = convert(path)
        with open(path, "w", encoding="utf-8") as fp:
            fp.write(text)

    stale = 0
    pages = sorted(f for f in os.listdir(HTML) if f.endswith(".html"))
    owned = 0
    for page in pages:
        path = os.path.join(HTML, page)
        with open(path, encoding="utf-8") as fp:
            before = fp.read()
        if MARK["top"][0] not in before and MARK["masthead"][0] not in before:
            # A doc.css page without markers would drop out of the check, not fail it.
            if 'href="doc.css"' in before:
                print(f"{page}: loads doc.css but carries no doc-chrome marker")
                stale += 1
            continue
        owned += 1
        # region() rewrites the first pair only, so a second one would ship unchecked.
        double = [
            o for o, c in MARK.values() if before.count(o) > 1 or before.count(c) > 1
        ]
        if double:
            print(f"{page}: duplicate {double[0]} marker")
            stale += 1
            continue
        try:
            after = rewrite(path, before)
        except SystemExit as exc:
            # Malformed markers, e.g. an unclosed region: name the page, keep going.
            print(f"{page}: {exc}")
            stale += 1
            continue
        # Check the bytes that ship, not the regenerated form, which always has it.
        if MARK["masthead"][0] not in (before if args.check else after):
            print(f"{page}: no {MARK['masthead'][0]} marker")
            stale += 1
        if after == before:
            continue
        if args.check:
            print(f"{page}: chrome is out of sync with tools/doc-chrome.py")
            stale += 1
        else:
            with open(path, "w", encoding="utf-8") as fp:
                fp.write(after)
            print(f"{page}: updated")
    if not owned:
        print("no page carries the doc-chrome markers", file=sys.stderr)
        return 1
    print(f"{owned} pages carry the chrome, {stale} out of sync")
    return 1 if stale else 0


if __name__ == "__main__":
    sys.exit(main())
