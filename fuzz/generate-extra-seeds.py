#!/usr/bin/env python3
"""Generate extra structured seed corpora for the libFuzzer harnesses.

The hand-picked seeds in fuzz/corpus/<target>/ are a starting point; this
script constructs structurally-valid inputs from the project's own tables and
grammars so mutation reaches past the magic/length checks into deep parsing:

- entities: every named entity in htsentities.h, plus the numeric, unicode,
  unterminated and oversized shapes the decoder must handle;
- htsparse/singlefile: HTML shapes the tiny hand corpus lacks (frames, forms,
  tables, conditional comments, SVG/MathML, CDATA, meta refresh, data: URLs,
  long attributes, tag soup);
- url/unescape/idna: URL shapes across schemes, authorities, encodings;
- header: response header blocks (status line variants, folding, cookies);
- charset/codepage/meta: charset declaration shapes;
- filters: wildcard filter lines.

Run from the repository root:  python3 fuzz/generate-extra-seeds.py
Outputs land next to the hand-picked seeds in fuzz/corpus/<target>/.
"""

import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
CORPUS = os.path.join(HERE, "corpus")


def write(target, name, text):
    d = os.path.join(CORPUS, target)
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, name), "wb") as fh:
        fh.write(text.encode("utf-8") if isinstance(text, str) else text)


def entity_names():
    """The named entities, from htsentities.h's hash-table comments."""
    names = set()
    with open(os.path.join(HERE, "..", "src", "htsentities.h"),
              encoding="utf-8") as fh:
        for line in fh:
            m = re.search(r"/\* ([A-Za-z][A-Za-z0-9]*) \*/", line)
            if m:
                names.add(m.group(1))
    return sorted(names)


def gen_entities():
    names = entity_names()
    write("entities", "entities-all-named.txt",
          "".join("&%s;" % n for n in names))
    write("entities", "entities-numeric.txt",
          "&#65;&#x41;&#0;&#9;&#1114111;&#1114112;&#-1;&#;&#x;&#xD800;")
    write("entities", "entities-shapes.txt",
          "&amp &amp;amp;&#x41t;&notarealentity;&;&;;;"
          "&" + "a" * 64 + ";&Aacute;&#X41;&Aacute")
    write("entities", "entities-in-context.html",
          "<p title=\"&quot;a&quot;\">&lt;tag&gt; &nbsp;&copy;2026"
          " caf&eacute; &family;&#38;&BAD;</p>")


def gen_htsparse():
    html = [
        ("frames.html",
         "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Frameset//EN\">"
         "<frameset cols=\"50%,50%\"><frame src=\"a.html\" name=\"l\">"
         "<frame src=\"../b.html\" name=\"r\"><noframes><a href=\"n.html\">"
         "n</a></noframes></frameset>"),
        ("forms.html",
         "<form action=\"/submit\" method=\"post\"><input type=\"text\" "
         "name=\"q\" value=\"a&amp;b\"><select><option value=\"1\">o"
         "</option></select><textarea>x</textarea>"
         "<button formaction=\"/other\">go</button></form>"),
        ("tables.html",
         "<table background=\"bg.png\"><caption>c</caption><colgroup>"
         "<col span=\"2\" style=\"url(x.css)\"><tr><td colspan=\"2\">"
         "<img src=\"t.png\" srcset=\"t@2x.png 2x\"></td></tr></table>"),
        ("conditional.html",
         "<!--[if IE 9]><a href=\"ie9.html\">ie</a><![endif]-->"
         "<!--<![CDATA[ <a href=\"cdata.html\">x</a> ]]>-->"),
        ("svg-mathml.html",
         "<svg><a xlink:href=\"svg.html\"><use xlink:href=\"u.svg#i\">"
         "</use></a><image href=\"i.svg\"/></svg>"
         "<math href=\"m.html\"><mi>x</mi></math>"),
        ("base-refresh.html",
         "<base href=\"https://cdn.example.com/dir/\"><meta http-equiv="
         "\"refresh\" content=\"5; url=/next.html\"><meta http-equiv="
         "\"Refresh\" content=\"0;URL='rel.html'\">"
         "<meta charset=\"windows-1252\">"),
        ("data-urls.html",
         "<img src=\"data:image/png;base64,iVBORw0KGgo=\">"
         "<a href=\"javascript:void(0)\">j</a>"
         "<a href=\"//protocol.relative/x.html\">p</a>"
         "<link rel=\"stylesheet\" href=\"data:text/css,body{}\">"),
        ("long-attrs.html",
         "<a href=\"x.html#" + "f" * 2000 + "\" title=\"" + "t" * 3000 +
         "\">long</a><img src=\"" + "p" * 1500 + ".png\">"),
        ("object-applet.html",
         "<object data=\"o.swf\" codebase=\"cb/\"><param name=\"m\" "
         "value=\"v\"><embed src=\"e.svg\"><applet code=\"A.class\" "
         "archive=\"a.jar\"></applet></object>"),
        ("soup.html",
         "<b><i><u><a href=\"s.html<p><div x=1 y='2' z=\"3\" w=4 5 <ul>"
         "<li>one<li>two</ul><p/><br/ ><!-- unterminated"),
        ("style-script.html",
         "<style>@import \"i.css\"; @import url(s.css); body{background:"
         "url(bg.png)}</style><script src=\"s.js\"></script>"
         "<script>var s=\"</script><a href=\\\"in.html\\\">\";</script>"
         "<script type=\"text/template\"><a href=\"t.html\">t</a></script>"),
        ("areas.html",
         "<img src=\"m.png\" usemap=\"#m\"><map name=\"m\">"
         "<area href=\"a1.html\" shape=\"rect\" coords=\"0,0,1,1\">"
         "<area href=\"../a2.html\" shape=\"circle\" coords=\"1,1,1\">"
         "</map>"),
    ]
    for name, text in html:
        write("htsparse", name, text)
    write("singlefile", "doctype-frames.html", html[0][1])
    write("singlefile", "svg-links.html", html[4][1])
    write("meta", "meta-refresh.txt",
          "<meta http-equiv=\"refresh\" content=\"0;url=r.html\">"
          "<meta http-equiv=refresh content='1;URL=\\'r2.html\\''>"
          "<meta charset=utf-8><meta http-equiv=\"Content-Type\" "
          "content=\"text/html; charset=Shift_JIS\">")


def gen_url():
    urls = [
        "http://example.com/index.html",
        "https://example.com:8443/a/b/c.html?q=1&r=2#frag",
        "ftp://ftp.example.com:2121/pub/files/file.txt",
        "file:///localhost/home/user/x.gif",
        "http://user:pass@example.com/secret/",
        "http://[2001:db8::1]:8080/v6/",
        "http://[::ffff:192.0.2.1]/v4mapped.html",
        "//cdn.example.com/lib.js",
        "/absolute/path.html",
        "relative/paths/here.html",
        "../up/and/over.html",
        "./current/./dot/../dotdot.html",
        "a/../../b/../../../c.html",
        "http://example.com/a//b///c////d.html",
        "http://example.com/percent%20enc%2Foded.html",
        "http://example.com/percent-%2-%zz-bad.html",
        "http://xn--caf-dma.example.com/caf%C3%A9.html",
        "http://caf\xc3\xa9.example.com/r\xc3\xa9sum\xc3\xa9.html",
        "http://example.com/./" + "d/" * 200 + "deep.html",
        "http://example.com/x.html?" + "a=1&" * 100 + "z=9",
        "HtTp://Mixed.Case.EXAMPLE.CoM/Case.html",
        "http://example.com/back\\slash.html",
        "http://trailing.example.com/",
        "mailto:someone@example.com",
        "javascript:void(0)",
        "data:text/plain,hello",
        "http://example.com:0/port0.html",
        "http://:8080/nohost.html",
        "http://example.com:x/badport.html",
    ]
    write("url", "urls.txt", "\n".join(urls))
    write("unescape", "unescapes.txt",
          "\n".join(["%41%42%43", "%zz%2%2G", "%", "%2", "a+b",
                     "%C3%A9", "%FF%FE", "100%25", "%00%01", "plain",
                     "&amp;copy", "%2F%2E%2E%2F"]))
    write("idna", "idna.txt",
          "\n".join(["caf\xc3\xa9.example", "xn--caf-dma.example",
                     "\xe4\xb8\xad\xe6\x96\x87.example",
                     "xn--fiqs8s.example", "a.b.c.example",
                     ".trailing.dot.example.", "UPPER.CASE.EXAMPLE",
                     "xn--", "x.example", "-lead.example",
                     "do..ouble.example"]))


def gen_header():
    write("header", "headers-basic.txt",
          "HTTP/1.1 200 OK\r\n"
          "Content-Type: text/html; charset=utf-8\r\n"
          "Content-Length: 10\r\n"
          "Location: /redirected.html\r\n"
          "Set-Cookie: a=1; Path=/; Secure; HttpOnly\r\n"
          "Content-Disposition: attachment; filename=\"f.zip\"\r\n"
          "\r\n")
    write("header", "headers-odd.txt",
          "HTTP/1.0 302 Moved Temporarily\n"
          "location:relative.html\n"
          "Location: \n"
          "Content-Type:\n"
          "Set-Cookie: no-path=1\n"
          "Set-Cookie: =empty-name\n"
          "Set-Cookie: v=2; Domain=.example.com; Max-Age=-1; Expires=Wed, "
          "21 Oct 2026 07:28:00 GMT\n"
          "Refresh: 0;url=r.html\n"
          "Link: <l.css>; rel=stylesheet\n"
          "Content-Range: bytes 0-9/100\n"
          "Transfer-Encoding: chunked\n"
          "X-Long: " + "x" * 8000 + "\n\r\n")
    write("header", "headers-statuslines.txt",
          "HTTP/1.1 100 Continue\r\nHTTP/1.1 204\r\nHTTP/9.9 999 X\r\n"
          "garbage status line\r\nHTTP/1.1 200 OK (comment) x\r\n")


def gen_charset():
    write("charset", "charsets.html",
          "<meta charset=\"utf-8\"><meta http-equiv=\"Content-Type\" "
          "content=\"text/html; charset=iso-8859-1\">"
          "<meta http-equiv=charset content=\"x-mac-roman\">"
          "<meta charset=\"unknown-charset\"><meta charset=\"\">"
          "<meta charset=\"UCS-4\"><meta charset=\"CsIcO932\">")
    write("codepage", "codepages.txt",
          "utf-8\niso-8859-1\nwindows-1252\nkoi8-r\nbig5\nshift_jis\n"
          "iso-2022-jp\nutf-16le\ngb18030\nunknown-cp\ncp1252\n"
          "\xff\xfeA\x00B\x00")
    write("charset", "charset-bom.txt",
          "\xef\xbb\xbf<html><head><meta charset=\"utf-8\"></head></html>")


def gen_filters():
    write("filters", "filters.txt",
          "\n".join(["+*.example.com/*", "-*/private/*",
                     "+*.html", "-*.css", "*[path]/*", "+*/*[file]",
                     "localhost", "127.0.0.1", "+http://*/x.html",
                     "-*cgi-bin*", "+*.example.com:8080/*"]))


def gen_sitemap():
    write("sitemap", "sitemap.xml",
          "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
          "<urlset xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\">"
          "<url><loc>http://example.com/a.html</loc><lastmod>2026-01-01"
          "</lastmod></url><url><loc>http://example.com/b.html</loc>"
          "</url></urlset>")
    write("sitemap", "sitemap-broken.xml",
          "<urlset><url><loc>http://example.com/x.html</url>"
          "</urlset><unclosed>")


def main():
    gen_entities()
    gen_htsparse()
    gen_url()
    gen_header()
    gen_charset()
    gen_filters()
    gen_sitemap()
    n = 0
    for d in sorted(os.listdir(CORPUS)):
        p = os.path.join(CORPUS, d)
        if os.path.isdir(p):
            n += len(os.listdir(p))
    print("corpus files now: %d" % n)


if __name__ == "__main__":
    main()
