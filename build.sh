#!/bin/sh
#
# One-shot build from a git checkout: bootstrap (autoreconf) -> configure -> make.
# A convenience wrapper; the canonical steps are ./bootstrap && ./configure && make.
#
# Extra arguments are passed straight to ./configure, e.g.
#   ./build.sh --prefix="$HOME" --disable-https
#
# Build out of tree (recommended; required if the source tree is read-only or
# mounted noexec) by pointing BUILD_DIR at a scratch directory:
#   BUILD_DIR=/var/tmp/httrack-build ./build.sh
#
# On a noexec filesystem the executable bit does not take effect, so invoke the
# script explicitly with sh:  sh build.sh
#
# Requires autoconf/automake/libtool (for bootstrap); the release tarball ships
# ./configure, so from a tarball just run ./configure && make.

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
