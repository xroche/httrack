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

OSS-Fuzz builds these harnesses with `oss-fuzz-build.sh`, so a new harness needs only a `Makefile.am` entry and a `corpus/<target>/` directory. The script links each harness with `$LIB_FUZZING_ENGINE`. When it is set, `configure` uses it in place of `-fsanitize=fuzzer`.
