# Packaging HTTrack

For distribution and ports maintainers. [CONTRIBUTING.md](CONTRIBUTING.md) covers
sending us changes; this covers shipping a build of ours.

## Take the release tarball, not the tag archive

`https://github.com/xroche/httrack/releases/download/X.Y.Z/httrack-X.Y.Z.tar.gz`,
with a detached signature and a checksum beside it.

That is a `make dist` tarball. It carries `configure`, the generated
`Makefile.in`s, the man pages and the coucal sources, so it builds with nothing
but a compiler, zlib and OpenSSL.

GitHub's `archive/refs/tags/X.Y.Z.tar.gz` carries none of them. coucal is a
submodule, so the directory comes out empty, and `configure` and the man pages
are build products we do not track. Several recipes use the tag archive today and
pay for it with an `autoreconf`, a second tarball and a coucal commit pin
somebody has to move at every bump.

## configure

- `--with-zlib=DIR` points at a non-standard zlib prefix. zlib itself is
  required, not optional: the cache and the WARC output are zip containers.
- `--with-brotli[=DIR]`, `--with-zstd[=DIR]` select the `br` and `zstd` content
  codings, `--enable-backtrace` selects symbolic crash traces on Linux, and
  `--with-iconv[=DIR]` converts charsets with `iconv()`. All four default to
  `auto`. See
  [the optional dependencies are automagic](#the-optional-dependencies-are-automagic).
- `--disable-auto-features` turns those four defaults into `no`. Pass it.
- `--enable-https=yes|no|auto` selects OpenSSL. On by default.
- `--enable-ipv6=yes|no|auto` selects IPv6, and probes for `getaddrinfo` by
  default. It picks which arm of `SOCaddr` the installed headers declare, so pin
  it rather than letting the build machine answer.
- `--enable-online-unit-tests` is off by default, and `make check` reaches the
  network only when it is on. Nothing to pass.
- `--disable-origin-rpath` exists, but configure already declines a
  binary-relative rpath when `libdir` is a system one. A distribution build gets
  none, so there is no libtool line to sed out.
- `--enable-fuzzers` builds the fuzz harnesses under clang. Not for a package.
- `--htmldir` defaults to `$(docdir)/html` rather than autoconf's `$(docdir)`.
  The pages link `../license.txt`, `../greetings.txt` and `../history.txt`, which
  install into `$(docdir)`, so a layout that puts the pages anywhere else strands
  those three.

## Everything under $(docdir) is safe to strip

`$(docdir)` holds documentation and nothing else. `rpm --excludedocs`, Portage's
`nodoc`, FreeBSD's `DOCS=off` and an automatic `-doc` split all work with no path
override, no `%doc` exception and no symlink fixup on your side.

The WebHTTrack interface htsserver serves is runtime, so it installs under
`$(datadir)/httrack/html/server`. Beside it, `$(datadir)/httrack/html/doc` is a
relative symlink to `$(docdir)`, which is how the panes' Help links reach the
manual. Stripping the documentation leaves that link dangling, htsserver answers
404, and the panes hide the links they would have carried. Ship the symlink with
the interface or with the documentation, whichever your split prefers.

Older releases installed the interface under `$(docdir)` and made
`$(datadir)/httrack/html` the symlink, which is what several recipes carried a
path override or a `%files` exception for. Drop yours.

## The optional dependencies are automagic

`--with-brotli` and `--with-zstd` default to `auto`, so the `br` and `zstd`
content codings follow whatever happens to be installed on the builder. A recipe
that does not pin them records no choice and links what it finds. Both flags have
carried that default since 3.49-13 (#556). Two more defaults work the same way.
`--enable-backtrace` selects the symbolic stack trace httrack prints on a crash,
and picks up libexecinfo where the C library has no `backtrace()`.
`--with-iconv` converts charsets with `iconv()`, taking a separate GNU libiconv
where the C library has none, and `--without-iconv` falls back to the codepage
tables built into the binary. Those tables are single-byte and convert to UTF-8
only, so a build without `iconv()` reads the ISO 8859 and `windows-125x` family
and nothing else. It cannot read a multi-byte charset such as `gb2312`,
`shift_jis` or `euc-jp`, and it cannot write a mirrored URL back out in the
page's own charset. Keep `--with-iconv` unless you want the smaller,
dependency-free conversion on purpose.

Pin it, either way:

```
./configure --with-brotli    --with-zstd
./configure --without-brotli --without-zstd
```

The `--with-` forms are hard requirements, so configure fails when the library is
missing instead of dropping the coding. Add the development packages to your
build dependencies beside them. Either way configure reports the answer as
`checking whether to enable the brotli content coding... yes (auto)`, where
`(auto)` means the build environment decided and `(requested)` means you did.

Naming four flags only works while four is the number, and a release that adds a
fifth optional dependency would slip past a recipe pinned this way. So there is
one switch that moves every `auto` default at once, the autotools spelling of
meson's `auto_features=disabled`:

```
./configure --disable-auto-features \
    --with-brotli --with-zstd --enable-backtrace --with-iconv
```

Everything you pass beside it still wins, and fails loudly when its library is
absent. The one exception is `--enable-backtrace` away from Linux, which reports
`no (not implemented on <host>)` and carries on, so a recipe can pass it for
every architecture it builds. Every optional feature you leave out is off,
whatever the builder has installed, so the line above names all four this build
takes. zlib and OpenSSL are not among them, because both are required and both
already fail the build when they are missing. A feature the switch turned off
reports as `no (auto-features off)`, which is how you tell it apart from a probe
that looked and found nothing. Our own `debian/rules` passes exactly that line.

The switch moves features, not build tools, so one thing is still yours to pin.
`pkg-config` decides nothing about the binary and everything about the installed
`libhttrack.pc`: with it, a codec reaches a consumer as `Requires.private:
libbrotlidec libzstd`, and without it as `Libs.private: -lbrotlidec -lzstd`.
The second form drops the `-L` an out-of-the-way prefix needs and orders nothing
for a static link, so build-depend on `pkg-config` (`pkgconf` on Debian, where
the old name is transitional) rather than letting the chroot decide. Ours does.

## Do not re-encode the tree

Everything is UTF-8 except `src/htsconcat.c`, which has ISO-8859-1 bytes in its
comments, and the binary vectors under `fuzz/corpus/`. An `iconv -f ISO8859-1`
pass over `greetings.txt`, `history.txt` or `html/contact.html` double-encodes
them. That is how the mojibake in issue #1463 reached Fedora users.

## The test suite in a buildroot

`make check -j"$(nproc)"`. Each crawl test binds its own ephemeral-port server,
so parallelism never contends and a serial run costs minutes for nothing.

The suite is offline but needs python3, which several buildroots lack. Without it
most tests skip and the suite still exits 0, so read the `# TOTAL:` and `# PASS:`
counts rather than the exit status. There are over 400 tests; a run reporting a
couple of hundred is one that mostly skipped.

A buildroot's network is not a builder's. Fedora's mock gives loopback and a
default route with no resolver, where a UDP `connect()` succeeds but returns no
source address. Debian's sbuild leaves out the default route, so the same
connect fails outright. `tools/buildroot-net.sh <command>` reproduces the mock
shape, and our CI runs the whole suite under it. Failures land in
`tests/test-suite.log` and in a per-test `tests/NNN_*.log`.

## When you need a patch

Send it upstream, even when you have already applied it locally. A patch carried
in a recipe is a bug report we never received: we cannot fix a bug we never hear
about, and it breaks at your next version bump, in your build.

Issues go to <https://github.com/xroche/httrack/issues>, security reports to
[SECURITY.md](SECURITY.md). We build master through Fedora's spec on a schedule
and check other recipes' assumptions against our tree, so we sometimes catch a
break first, but only for recipes we know about. Tell us yours exists.
