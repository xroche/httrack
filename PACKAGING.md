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
- `--with-brotli[=DIR]`, `--with-zstd[=DIR]`: the `br` and `zstd` content
  codings, used when the library is found, dropped by `--without-`.
- `--enable-https=yes|no|auto` selects OpenSSL. On by default.
- `--enable-online-unit-tests` is off by default, and `make check` reaches the
  network only when it is on. Nothing to pass.
- `--disable-origin-rpath` exists, but configure already declines a
  binary-relative rpath when `libdir` is a system one. A distribution build gets
  none, so there is no libtool line to sed out.
- `--enable-fuzzers` builds the fuzz harnesses under clang. Not for a package.

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
default route with no resolver, where a UDP connect succeeds and names no source
address. Debian's sbuild leaves out the default route, so the same connect fails
outright. `tools/buildroot-net.sh <command>` reproduces the mock shape, and our
CI runs the whole suite under it. Failures land in `tests/test-suite.log` and in
a per-test `tests/NNN_*.log`.

## When you need a patch

Send it upstream, even when you have already applied it locally. A patch carried
in a recipe is a bug report we never received: we cannot fix or keep working what
we cannot see, and it breaks at your next version bump, in your build.

Issues go to <https://github.com/xroche/httrack/issues>, security reports to
[SECURITY.md](SECURITY.md). We build master through Fedora's spec on a schedule
and check other recipes' assumptions against our tree, so we sometimes catch a
break first, but only for recipes we know about. Tell us yours exists.
