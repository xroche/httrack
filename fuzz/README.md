# Fuzzing httrack

libFuzzer harnesses for the pure hostile-input parsers (charset/UTF-8/IDNA codecs, entity and percent decoders, wildcard filters, URL splitter). Off by default; needs clang.

```sh
./bootstrap
mkdir /var/tmp/bld-fuzz && cd /var/tmp/bld-fuzz
CC=clang CFLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all -g -O1" \
  LDFLAGS="-fsanitize=address,undefined" \
  bash /path/to/httrack/configure --enable-fuzzers --disable-shared
make
bash /path/to/httrack/fuzz/run-fuzzers.sh fuzz 60   # 60s per target
```

Run one target by hand: `fuzz/fuzz-url -max_total_time=300 corpusdir fuzz/corpus/url`. Seed corpora live in `corpus/<target>/`; a crash reproducer is replayed with `fuzz/fuzz-url crash-file`.

`fuzz-codepage` links its own copy of `htscharset.c` under `DISABLE_ICONV`. Where iconv is available httrack uses it and never compiles `hts_codepageToUTF8()` or the `htscodepages.h` tables, which is exactly the decoder Windows and the iconv-less distros ship, so this harness pins the tables in regardless of what the build chose. On a build without iconv it and `fuzz-charset` cover the same code.

`fuzz-cachendx` drives proxytrack's legacy `.ndx`/`.dat` reader, the only one of that format left: #1551 moved `-#C` onto the ZIP index. Its input carries both files, the first two bytes giving the big-endian length of the `.ndx` half.

`fuzz-arc` is the odd one out: it drives proxytrack's `.arc` reader the way `--convert` does, through a temp file rather than a buffer, and it compiles `src/proxy/store.c` into the harness because proxytrack does not link libhttrack. Both readers and the writer print to stderr on malformed input, so pass `-close_fd_mask=2` for anything longer than a corpus replay.

`fuzz-codec` drives `hts_codec_unpack_max()` through temp files. Its first input byte picks the codec: gzip/deflate, br, zstd, or one that is never decoded. The same byte sets the `-m` cap and whether the sniffer's prefix decoder also runs. Gzip and deflate output is checked against zlib itself. br and zstd decode only in a build that has them, so the corpus replays either way.

`fuzz-response` drives `back_wait()` over a socketpair, so it covers the header block, chunked framing and the content decoders without a network. Its first byte is option bits (see `fuzz-response.c`), its second picks the file name and the size of a partial file to resume, and its third cuts the response into pieces that arrive one read at a time. Disk options use a temp dir under `$TMPDIR`.

`fuzz-redirect` drives `hts_mirror_check_moved()`, which handles a redirect's `Location` and a 412 or 416 answer to a resume. Its first byte picks the status and the page, turns on MIME HTML, and can plant a partial file and a resume reference. Its second byte sets the URL hack and the error-page options, and can move the page to another host and port. The first line is the `Location`, and the bytes after it fill the planted files. Files go in a temp dir under `$TMPDIR`.

`fuzz-socks5` replays a hostile proxy reply into the SOCKS5 client handshake. Its input is the origin address, a newline, the proxy URL, a newline, then the bytes the proxy sends back.

`fuzz-ftp` feeds its input to `get_ftp_line()` over a socketpair and runs each reply line through the PASV, EPSV, SIZE and MDTM parsers. The same bytes, read as a URL, go through the user, host and command splitters. Its first byte sets the reply buffer size: odd is the 1024 bytes every caller passes, even is a short buffer.

`fuzz-accept` asks `hts_acceptlink()` whether to follow a link found on `/dir/page.html`, a page the start URL `/` links to. Its first byte sets the travel scope, the wizard mode and whether the crawl may go up a directory. It can also put that page at the last depth level. The NUL-separated fields after it are the link's host and path, a MIME type, a wizard reply, then up to six filter rules. The filter rules apply only when the wizard bit is set.

`fuzz-alias` splits a command line with `hts_split_args()`, maps each argument through the option aliases, and checks that each argument, once quoted, splits back to itself. Its first byte holds the split flags: 6 is how `doit.log` is read back, 3 is the WebHTTrack form. Bytes after a NUL become a config file for `optinclude_file()`, the path that reads a project's `httrackrc`.

`fuzz-sniff` drives the MIME magic-byte check in `htssniff.c`. Its input is the MIME type the server claims, a NUL, then the body. The dict holds the MIME names the rules know, so a new rule needs a dict entry but no harness change.

OSS-Fuzz builds these harnesses with `oss-fuzz-build.sh`, so a new harness needs only a `Makefile.am` entry and a `corpus/<target>/` directory. The script links each harness with `$LIB_FUZZING_ENGINE`. When it is set, `configure` uses it in place of `-fsanitize=fuzzer`.
