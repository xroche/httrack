#!/bin/bash
# Run from the source root to build the OSS-Fuzz fuzzers.
# fuzz/Makefile.am is the one list of harnesses and their extra sources.
set -euo pipefail

: "${CC:?} ${CXX:?} ${OUT:?} ${LIB_FUZZING_ENGINE:?}"

./bootstrap
# afl-cc turns -fcf-protection into LTO bitcode that GNU binutils cannot link, so use llvm tools and lld.
ax_cv_check_cflags__Werror__fcf_protection=no \
    ./configure --enable-fuzzers --disable-shared --disable-https \
    AR=llvm-ar RANLIB=llvm-ranlib LDFLAGS="${LDFLAGS:-} -fuse-ld=lld"
make -j"$(nproc)" -C src libhttrack.la
# OSS-Fuzz requires linking with $CXX and $CXXFLAGS for every engine.
make -j"$(nproc)" -C fuzz CCLD="$CXX $CXXFLAGS"

for c in fuzz/fuzz-*.c; do
    f=$(basename "$c" .c)
    cp "fuzz/$f" "$OUT/"
    zip -qj "$OUT/${f}_seed_corpus.zip" "fuzz/corpus/${f#fuzz-}"/*
done
# OSS-Fuzz pairs these with a fuzzer by name, so a misnamed one is silently unused.
for x in fuzz/fuzz-*.dict fuzz/fuzz-*.options; do
    [ -f "${x%.*}.c" ] || {
        echo "$x names no harness" >&2
        exit 1
    }
    cp "$x" "$OUT/"
done
