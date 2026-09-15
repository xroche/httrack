# Third-party notices

HTTrack itself is GPL-3.0-or-later (`COPYING`), with the OpenSSL linking
exception stated in `license.txt`.

This file covers everything else that ends up in a build: code vendored into
this tree, and libraries resolved at link time. There is no lockfile to read
it off — dependencies here are a mix of committed source and whatever the
build host provides — so it is maintained by hand and checked by
`tools/generate-sbom.sh`, which fails if a vendored directory is not
described here.

## Vendored source

Code committed into this repository, compiled into `libhttrack`.

| Component | Where | Upstream | License |
|---|---|---|---|
| MiniZip 1.1 | `src/minizip/{zip,unzip,ioapi,iowin32}.[ch]`, `crypt.h` | Gilles Vollant — <http://www.winimage.com/zLibDll/minizip.html> | Zlib |
| MiniZip Zip64 changes | same files | Even Rouault (2007–2008), Mathias Svensson (2009–2010) | Zlib |
| Info-ZIP decryption | `src/minizip/crypt.h`, part of `unzip.c` | Info-ZIP (1990–2000), woven in by Terry Thorsen 2003 | Info-ZIP |
| mztools | `src/minizip/mztools.[ch]` | Xavier Roche, 2004 | Zlib ("same as zlib") |
| MD5 | `src/md5.[ch]` | Colin Plumb, 1993 | Public domain |
| Punycode | `src/punycode.[ch]` | Adam M. Costello, RFC 3492 appendix B | RFC 3492 disclaimer and license |
| Coucal | `src/coucal/` (git submodule) | <https://github.com/xroche/coucal> | BSD-3-Clause |
| MurmurHash3 | `src/coucal/murmurhash3.h` | Austin Appleby | Public domain |

`src/coucal` is a submodule, so what a given build contains is the commit
recorded in the superproject, not whatever upstream currently is. The SBOM
records that commit.

The minizip and murmurhash3 copies are patched rather than pristine. Each
modified file keeps its pre-patch original and the diff beside it
(`zip.c.orig` / `zip.c.diff`, and so on for `ioapi.[ch]`, `mztools.c`,
`zip.h` and `murmurhash3.h`), so what was changed against upstream stays
readable. Treat the upstream version in the table as a baseline, not as an
exact match.

### Note on the Punycode notice

The vendored `punycode.[ch]` had the RFC 3492 "Disclaimer and license"
paragraph stripped from its header, while that license requires the notice be
retained. It has been restored in place. The text is quoted from RFC 3492
appendix B and can be checked against
<https://www.rfc-editor.org/rfc/rfc3492.txt>.

## Libraries linked at build time

Not vendored. Which of these a binary actually depends on, and at what
version, is a property of the host it was built on — `tools/generate-sbom.sh`
reads them back out of the built artifact rather than guessing from headers.

| Library | Required | License | Notes |
|---|---|---|---|
| zlib | yes | Zlib | |
| OpenSSL (`libssl`, `libcrypto`) | optional (`configure --disable-https`, or absent at configure time) | Apache-2.0 for 3.x; OpenSSL/SSLeay for 1.x | covered by the `license.txt` exception |
| libiconv | optional | LGPL-2.1-or-later | glibc provides `iconv` natively, in which case nothing extra is linked |
| pthreads, libdl, libnsl, libsocket | platform | system C library terms | |

Reading the version off the headers is not good enough to record here: on a
host with OpenSSL 3.0.2 installed alongside 1.1.1 development files,
`pkg-config --modversion openssl` answers `1.1.1f` while the binary links
`libcrypto.so.3`. The SBOM takes the linked object's answer.

## Regenerating

```
./configure && make
tools/generate-sbom.sh --output sbom.json
```

See that script for the output format (CycloneDX 1.5 JSON) and for what it
checks.
