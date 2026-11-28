# Image for Anthropic's OSS Scanner (see threat_model.md next to this file).
# Two builds of /src, both with ASan and UBSan:
#   /src/build/asan  httrack, webhttrack, proxytrack, libhttrack (gcc); runs make check
#   /src/build/fuzz  the libFuzzer harnesses in fuzz/ (clang)
FROM docker.io/library/debian:trixie

ENV DEBIAN_FRONTEND=noninteractive \
    ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:symbolize=1 \
    UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
    ASAN_SYMBOLIZER_PATH=/usr/lib/llvm-19/bin/llvm-symbolizer

RUN apt-get update && apt-get install -y --no-install-recommends \
      ca-certificates build-essential git \
      clang-19 llvm-19 libclang-rt-19-dev libfuzzer-19-dev \
      autoconf automake libtool autoconf-archive pkg-config \
      zlib1g-dev libssl-dev libbrotli-dev libzstd-dev \
      python3 openssl gdb strace procps \
    && rm -rf /var/lib/apt/lists/*

COPY . /src
WORKDIR /src

# The build context may come without the coucal submodule.
RUN if [ ! -f src/coucal/coucal.c ]; then \
      git submodule update --init src/coucal \
      || { rm -rf src/coucal && git clone https://github.com/xroche/coucal.git src/coucal; }; \
    fi \
    && test -f src/coucal/coucal.c

RUN ./bootstrap

# gcc, because clang leaves its sanitizer runtime out of libhttrack.so.
RUN mkdir -p build/asan && cd build/asan \
    && /src/configure CC=gcc \
      CFLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all -g -O1 -fno-omit-frame-pointer" \
      LDFLAGS="-fsanitize=address,undefined" \
    && make -j"$(nproc)" \
    && ./src/httrack --version

# unsigned-integer-overflow is how a bounds check that adds before it compares fails.
RUN mkdir -p build/fuzz && cd build/fuzz \
    && /src/configure CC=clang-19 --enable-fuzzers --disable-shared \
      CFLAGS="-fsanitize=address,undefined,unsigned-integer-overflow -fsanitize-ignorelist=/src/fuzz/ubsan-ignorelist.txt -fno-sanitize-recover=all -g -O1 -fno-omit-frame-pointer" \
      LDFLAGS="-fsanitize=address,undefined,unsigned-integer-overflow" \
    && make -j"$(nproc)" \
    && ls fuzz/fuzz-*

# Smoke tests only: a red test must not fail the image, so the scanner still gets it.
RUN cd build/asan \
    && (timeout 40m make check -j16 >check.log 2>&1 \
        || echo "WARNING: make check failed or timed out, see /src/build/asan/check.log and tests/*.log") \
    && (grep -E '^(FAIL|ERROR):|^# ' check.log || true) \
    && for t in $(sed -n 's/^\(FAIL\|ERROR\): //p' check.log); do tail -n 30 "tests/${t%.test}.log"; done
RUN (timeout 15m bash fuzz/run-fuzzers.sh build/fuzz/fuzz check >build/fuzz/replay.log 2>&1 \
        || echo "WARNING: corpus replay failed, see /src/build/fuzz/replay.log") \
    && tail -n 5 build/fuzz/replay.log
