# HTTrack threat model

## What HTTrack does

HTTrack is a website copier. It crawls HTTP, HTTPS and FTP sites and writes a browsable copy to a local folder, the mirror. It rewrites the links in every page it saves. The tree builds four programs and one library:

- `httrack`, the command-line crawler.
- `webhttrack` and `htsserver`, a web GUI that runs a crawl for the local user.
- `proxytrack`, a proxy that serves pages out of an existing mirror's cache.
- `libhttrack`, an installed shared library with an exported API (`HTSEXT_API` in `src/httrack-library.h`). The GUI front ends for Windows and Android link it.

## The attacker

The main attacker is a remote server. HTTrack fetches pages from servers its user does not control, and a crawl follows links to more servers. Treat everything such a server sends as hostile:

- HTTP status lines, headers, chunked framing and bodies, including gzip, deflate, brotli and zstd encoded bodies.
- Redirects (`Location`), `Content-Type`, `Content-Disposition`, cookies and authentication challenges.
- HTML, CSS, JavaScript and other content that the parser scans for links (`src/htsparse.c`).
- `robots.txt`, sitemaps, FTP replies (`src/htsftp.c`), and SOCKS5 or HTTP CONNECT replies from a proxy (`src/htsproxy.c`).
- Host names and URLs from all of the above, through URL decoding, entity decoding, charset conversion, punycode and the Public Suffix List.

The attacker does not choose the command line. They control only what their server sends, and which links their pages contain.

A second attacker is a web page open in the user's browser while WebHTTrack runs. That page can send requests to the WebHTTrack server on the loopback address, and it can use DNS rebinding.

## What we want protected

These are the guarantees, from most to least important:

1. **No code execution from server data.** This covers memory corruption in any parser above. It also covers the external command option `-V`. The saved file name comes from the server, so `src/htsusercmd.c` must quote it. The shell must never run any part of that name.
2. **No write outside the mirror folder.** `url_savename()` in `src/htsname.c` builds each local file name from server data. A name with `..`, an absolute path, or a reserved Windows device name must stay inside the mirror. The same holds for ProxyTrack, which must not serve a file outside the cache it was given.
3. **No leak of the user's secrets to another host.** HTTP authentication and cookies must go only to the hosts and paths they belong to (`src/htsbauth.c`). `Referer` must not go from an https page to an http page.
4. **The WebHTTrack server serves only its own user.** It binds to 127.0.0.1. Each POST must carry the session id (`sid`). `host_is_self` and `origin_is_self` in `src/htsserver.c` reject foreign `Host` and `Origin` headers. An authenticated request can start a crawl with any options, `-V` included. So a bypass of these checks from a web page gives command execution.
5. **The crawl does not crash.** A crash from server data stops the user's mirror. HTTrack has helpers that abort on overflow (`strcpybuff` and others in `src/htssafe.h`), which are correct for our own data. Data from the wire, a header or a cache file must be clipped with `strclipbuff` instead. So a reachable abort on server data is a bug.

## What we trust

These inputs come from the user, not from the attacker. A bug that needs the attacker to control one of them is not a vulnerability:

- The command line, and the options of a library caller.
- `~/.httrackrc`, and the project folder: `doit.log`, `hts-cache/`, `cookies.txt`, `./.httrackrc` and `winprofile.ini`. `SECURITY.md` already tells users that these files can carry `-V` (run a command) and `-%W` (load a plugin).
- Plugins loaded with `-%W`, and the command given to `-V`. The command is trusted. Only the file name substituted into it is not.
- Symbolic links that already exist in the output folder.
- A WebHTTrack server bound to a non-loopback address with `--bind`, and the network a ProxyTrack listener is exposed to. ProxyTrack has no access control by design.

There is one exception: data that a server wrote into the cache, and that HTTrack reads back on the next run (`--update`, `--continue`). That data is server data. A bug triggered by cache content that a server could have caused is in scope.

## How we rate severity

- **Critical**: code execution, or a write outside the mirror folder, from a remote server with default options. A bypass of the WebHTTrack checks from a web page.
- **High**: an out-of-bounds write, a use-after-free or a double free reachable from a remote server, even with no exploit shown. Shell injection through the `-V` file name. A write outside the mirror folder that needs a non-default option. Credentials or cookies sent to another host.
- **Medium**: an out-of-bounds read reachable from a remote server. Small input that costs huge memory or CPU, past the limits that options such as `-M`, `-E` and `-m` set. A ProxyTrack read outside its cache.
- **Low**: a crash or abort from server data with no memory corruption, such as a NULL dereference or a safe-helper abort. A crash in a ProxyTrack request parser is also low.
- **Not a vulnerability**: a bug that needs trusted input from the list above. A server that sends endless or slow content, because HTTrack is a crawler and the user sets its limits.

## Components

- Most important: `src/htsparse.c`, `src/htslib.c`, `src/htsback.c`, `src/htsname.c`, `src/htscodec.c`, `src/htszlib.c`, `src/htscache.c`, `src/htsbauth.c`, `src/htsusercmd.c`, `src/htsserver.c`.
- In scope: the rest of `src/`, `src/proxy/`, `src/minizip/` (a modified copy of minizip) and `src/coucal/` (our own hash table, in a submodule).
- Out of scope: `tests/`, `tools/`, `fuzz/`, the `*_selftest.c` files, `html/` and `lang/`. Code under `_WIN32` is in scope, but this image cannot build it.

## How to exercise it

- `/src/build/asan` holds a build with ASan and UBSan. The programs are in `/src/build/asan/src/`. `make check` there runs the test suite, and its log is `check.log`.
- `tests/local-server.py` serves a folder over HTTP or HTTPS on a local port. `tests/crawllib.sh` shows how the tests start it and crawl it. This is the best way to show that a remote server can trigger a bug.
- `httrack -#test` lists the engine self-tests, and `httrack -#test=NAME` runs one. They call internal functions directly.
- `/src/build/fuzz/fuzz/` holds libFuzzer harnesses with ASan and UBSan. Their seed corpora are in `/src/fuzz/corpus/<name>/`, and `fuzz/run-fuzzers.sh` drives them.
- These parts have no fuzz harness: the WebHTTrack request handler, the ProxyTrack HTTP, ICP and WebDAV request parsers, and the `-V` file name quoting.
