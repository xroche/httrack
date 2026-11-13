#!/bin/bash
# OSS-Fuzz build: projects/httrack/build.sh runs this from the source root.
# fuzz/Makefile.am is the one list of harnesses and their extra sources.
set -euo pipefail

: "${CC:?} ${CXX:?} ${OUT:?} ${LIB_FUZZING_ENGINE:?}"

./bootstrap
# afl-cc rewrites -fcf-protection into CFI+LTO, emitting bitcode objects GNU
# binutils can't archive or link: drop the CET flag, use the llvm tools and lld.
ax_cv_check_cflags___fcf_protection=no \
    ./configure --enable-fuzzers --disable-shared --disable-https \
    AR=llvm-ar RANLIB=llvm-ranlib LDFLAGS="${LDFLAGS:-} -fuse-ld=lld"
make -j"$(nproc)" -C src libhttrack.la
# Link with the C++ driver, as OSS-Fuzz requires of every engine.
make -j"$(nproc)" -C fuzz CCLD="$CXX"

for c in fuzz/fuzz-*.c; do
    f=$(basename "$c" .c)
    cp "fuzz/$f" "$OUT/"
    zip -qj "$OUT/${f}_seed_corpus.zip" "fuzz/corpus/${f#fuzz-}"/*
done
cp fuzz/fuzz-*.dict fuzz/fuzz-*.options "$OUT/"
