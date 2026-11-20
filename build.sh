#!/bin/sh
#
# One-shot build from a git checkout: ./bootstrap && ./configure && make.
#
# Extra arguments go to ./configure, e.g.
#   ./build.sh --prefix="$HOME" --disable-https
#
# Set BUILD_DIR to build out of tree, which a read-only or noexec tree needs:
#   BUILD_DIR=/var/tmp/httrack-build ./build.sh
#
# On a noexec filesystem, run the script with sh:  sh build.sh
#
# bootstrap needs autoconf, automake and libtool. A release tarball ships
# ./configure, so there just run ./configure && make.

set -e

# shellcheck disable=SC1007  # "CDPATH= cd" is a deliberate empty-CDPATH prefix.
srcdir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

# 1. Regenerate the build system in the source tree (autotools requires it).
sh "$srcdir/bootstrap"

# 2. Configure and build, out of tree when BUILD_DIR is set.
builddir=${BUILD_DIR:-$srcdir}
mkdir -p "$builddir"
cd "$builddir"
# Invoke configure via the shell, not execve: it is a portable sh script, and a
# noexec source tree (or a cleared executable bit) would make ./configure fail.
"${CONFIG_SHELL:-/bin/sh}" "$srcdir/configure" "$@"
make
