#!/usr/bin/env bash
#
# Build, sign, and publish a source-tarball GitHub Release for HTTrack.
#
# The website (httrack.com) links its "Linux/OSX/BSD/Unix sources" download
# straight at the GitHub Release asset produced here, so the release must carry
# the real `make dist` tarball -- which bundles the generated ./configure, the
# regenerated man page, and the coucal submodule. GitHub's auto-generated tag
# archive has NONE of those and fails to build, so it must not be used.
#
# The tarball is built from a clean checkout of the *tag* (a throwaway git
# worktree), never from your working tree, then GPG-signed and checksummed.
#
# Output (in --outdir, default <repo>/dist):
#   httrack-<ver>.tar.gz          release tarball (make dist)
#   httrack-<ver>.tar.gz.asc      detached GPG signature (httrack.com PGP key)
#   httrack-<ver>.tar.gz.sha256   SHA-256 checksum (sha256sum -c format)
#
# Usage:
#   tools/gh-release.sh [options] [TAG]
#
# TAG defaults to the version in configure.ac (e.g. 3.49.10). The tarball
# version is read from the tag's own configure.ac, so TAG and that tree agree
# by construction.
#
# Options:
#   -k, --key KEYID     GPG key to sign with (default: $DEBSIGN_KEYID, else the
#                       gpg default key)
#   -o, --outdir DIR    output directory (default: <repo>/dist)
#   -n, --no-publish    build + sign + checksum only; do not touch GitHub
#   -h, --help          show this help
#
# Requires: autoconf/automake/libtool and a C toolchain (bootstrap + man-page
# regen), gpg, and -- unless --no-publish -- the GitHub CLI `gh`, authenticated
# with push rights to the origin repository.

set -euo pipefail

readonly PROGNAME=${0##*/}

# Set in main(); the EXIT trap needs them at global scope.
repo=""
scratch=""

die() {
    printf '%s: error: %s\n' "$PROGNAME" "$*" >&2
    exit 1
}

info() {
    printf '==> %s\n' "$*" >&2
}

usage() {
    sed -n '2,/^$/ { /^#/ { s/^# \{0,1\}//; p } }' "$0"
}

cleanup() {
    [[ -n $scratch && -d $scratch ]] || return 0
    # Prefer a clean worktree deregistration; fall back to rm if it was never
    # (or only partially) registered.
    git -C "$repo" worktree remove --force "$scratch" 2>/dev/null || rm -rf "$scratch"
}

# Extract the upstream version from an AC_INIT([httrack], [X.Y.Z], ...) line.
version_from() {
    local file=$1
    [[ -f $file ]] || return 1
    sed -n 's/^AC_INIT(\[httrack\], \[\([0-9][0-9.]*\)\].*/\1/p' "$file" | head -n1
}

main() {
    local key=${DEBSIGN_KEYID:-}
    local outdir=""
    local publish=1
    local tag=""

    while [[ $# -gt 0 ]]; do
        case $1 in
        -k | --key)
            key=${2:?missing KEYID}
            shift 2
            ;;
        -o | --outdir)
            outdir=${2:?missing DIR}
            shift 2
            ;;
        -n | --no-publish)
            publish=0
            shift
            ;;
        -h | --help)
            usage
            exit 0
            ;;
        --)
            shift
            break
            ;;
        -*) die "unknown option: $1 (see --help)" ;;
        *) break ;;
        esac
    done
    [[ $# -le 1 ]] || die "too many arguments (see --help)"
    tag=${1:-}

    command -v git >/dev/null || die "git not found"
    repo=$(git rev-parse --show-toplevel) || die "not inside a git repository"

    # Default TAG = the working tree's own version.
    if [[ -z $tag ]]; then
        tag=$(version_from "$repo/configure.ac")
        [[ -n $tag ]] || die "cannot read version from configure.ac; pass TAG explicitly"
    fi
    git -C "$repo" rev-parse -q --verify "refs/tags/$tag" >/dev/null ||
        die "tag '$tag' not found -- create and push it first"

    : "${outdir:=$repo/dist}"

    command -v gpg >/dev/null || die "gpg not found"
    if [[ $publish -eq 1 ]]; then
        command -v gh >/dev/null || die "gh (GitHub CLI) not found; use --no-publish to skip upload"
        (cd "$repo" && gh auth status >/dev/null 2>&1) || die "gh is not authenticated (run: gh auth login)"
    fi

    scratch=$(mktemp -d) || die "mktemp failed"
    trap cleanup EXIT

    info "checking out $tag into a throwaway worktree"
    git -C "$repo" worktree add --quiet --detach "$scratch" "$tag"
    info "initialising submodules"
    git -C "$scratch" submodule update --init --recursive --quiet

    local ver
    ver=$(version_from "$scratch/configure.ac")
    [[ -n $ver ]] || die "cannot read version from the tag's configure.ac"

    # A release tarball needs the man page, which is regenerated from the built
    # binaries; build once, regen, then roll the tarball from a clean tree so no
    # object files leak in. (Same sequence tools/mkdeb.sh uses.)
    info "building release tarball for $ver"
    (
        cd "$scratch"
        sh ./bootstrap
        ./configure --quiet
        make -s -j"$(nproc)"
        make -s -C man regen-man
        make -s clean
        make -s dist
    )

    local tarball="httrack-$ver.tar.gz"
    [[ -f $scratch/$tarball ]] || die "make dist did not produce $tarball"

    mkdir -p "$outdir"
    cp -f "$scratch/$tarball" "$outdir/$tarball"

    info "signing (detached, armored) and checksumming"
    local -a signopt=()
    [[ -n $key ]] && signopt=(--local-user "$key")
    # Signing is interactive: pinentry needs a terminal, so export GPG_TTY and do
    # NOT pass --batch (it suppresses the passphrase prompt, failing with an
    # "Inappropriate ioctl for device" error). Run this from a real terminal, or
    # unlock the key in gpg-agent beforehand.
    GPG_TTY=$(tty 2>/dev/null || true)
    export GPG_TTY
    gpg --yes --armor "${signopt[@]}" \
        --detach-sign --output "$outdir/$tarball.asc" "$outdir/$tarball" ||
        die "gpg signing failed -- run from an interactive terminal so pinentry can prompt (or unlock the signing key in gpg-agent first)"
    (cd "$outdir" && sha256sum "$tarball" >"$tarball.sha256")

    info "artifacts ready in $outdir:"
    printf '    %s\n' "$tarball" "$tarball.asc" "$tarball.sha256" >&2

    if [[ $publish -eq 0 ]]; then
        info "no-publish: GitHub Release not touched"
        return 0
    fi

    publish_release "$tag" "$ver" "$outdir" "$tarball"
}

publish_release() {
    local tag=$1 ver=$2 outdir=$3 tarball=$4

    # gh reads owner/repo from the origin remote, so run it inside the repo.
    if (cd "$repo" && gh release view "$tag" >/dev/null 2>&1); then
        info "release $tag already exists; re-uploading assets (--clobber)"
    else
        info "creating release $tag"
        (
            cd "$repo"
            gh release create "$tag" \
                --title "HTTrack $ver" \
                --verify-tag \
                --notes "HTTrack $ver source release.

The signed \`make dist\` tarball below bundles ./configure, the man page, and
submodules -- build it with \`./configure && make && sudo make install\` (no
bootstrap needed). Verify with the detached \`.asc\` signature (httrack.com PGP
key) and the \`.sha256\` checksum."
        )
    fi

    (
        cd "$repo"
        gh release upload "$tag" \
            "$outdir/$tarball" "$outdir/$tarball.asc" "$outdir/$tarball.sha256" \
            --clobber
    )
    info "published: $(cd "$repo" && gh release view "$tag" --json url --jq .url)"
}

main "$@"
