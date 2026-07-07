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
