#!/usr/bin/env bash
#
# Build the httrack Debian packages from a clean, committed source export.
#
# It exports HEAD (plus the coucal submodule) into a scratch directory, refreshes
# the build system and man page, builds the upstream tarball, overlays debian/,
# and runs debuild (which builds, runs lintian, and signs). Nothing is built in
# the working tree, and no hard-coded paths are used.
#
# Output (in --outdir, default <repo>/dist):
#   httrack_<ver>.orig.tar.gz            upstream tarball (Debian orig name)
#   httrack_<ver>-*.dsc / .debian.tar.*  source package
#   *.deb                                binary packages
#   *.changes / *.buildinfo              build metadata
#   httrack_<ver>.orig.tar.gz.{asc,md5,sha1}   release artifacts (unless disabled)
#
# Usage:
#   tools/mkdeb.sh [options]
#
# Options:
#   -k, --key KEYID            GPG key for signing (default: $DEBSIGN_KEYID)
#   -o, --outdir DIR           output directory (default: <repo>/dist)
#       --orig FILE            reuse this upstream orig tarball instead of
#                              regenerating it (required for a Debian revision
#                              >= 2, whose orig is frozen in the archive)
#   -s, --source-only          build only the source package
#   -u, --unsigned             do not sign anything (implies no release sigs)
#       --no-release-artifacts skip the orig tarball .asc/.md5/.sha1
#       --sbuild               additionally build the .dsc in a clean sbuild
#                              chroot as a from-scratch verification gate
#   -h, --help                 show this help
#
# --sbuild reproduces the buildd environment: it builds the source package in a
# minimal chroot holding only the declared Build-Depends, so an FTBFS or a
# missing dependency fails here instead of on the archive's buildds (which, with
# a source-only upload, are otherwise the first clean build). It needs an sbuild
# chroot for the changelog's distribution; create one once with the companion
# tools/mk-sbuild-chroot.sh (rootless unshare backend).
#
# The Debian revision in debian/changelog decides the orig: revision 1 builds a
# fresh upstream tarball; revision >= 2 must reuse the orig frozen at revision 1
# (the .dsc references it by checksum), so pass it with --orig.
#
# SOURCE_DATE_EPOCH is honored for reproducible output.

set -euo pipefail

readonly PROGNAME=${0##*/}

# Scratch dir, global so the EXIT trap can see it.
scratch=""

die() {
    printf '%s: error: %s\n' "$PROGNAME" "$*" >&2
    exit 1
}

info() {
    printf '==> %s\n' "$*" >&2
}

usage() {
    sed -n '2,/^set -euo/{/^set -euo/!p}' "$0" | sed 's/^# \{0,1\}//'
}

need() {
    local tool
    for tool in "$@"; do
        command -v "$tool" >/dev/null 2>&1 || die "required tool not found: $tool"
    done
}

main() {
    local key=${DEBSIGN_KEYID:-}
    local outdir=""
    local orig_in=""
    local source_only=0
    local unsigned=0
    local release_artifacts=1
    local sbuild=0

    while [[ $# -gt 0 ]]; do
        case $1 in
        -k | --key)
            [[ $# -ge 2 ]] || die "missing argument for $1"
            key=$2
            shift 2
            ;;
        -o | --outdir)
            [[ $# -ge 2 ]] || die "missing argument for $1"
            outdir=$2
            shift 2
            ;;
        --orig)
            [[ $# -ge 2 ]] || die "missing argument for $1"
            orig_in=$2
            shift 2
            ;;
        -s | --source-only)
            source_only=1
            shift
            ;;
        -u | --unsigned)
            unsigned=1
            shift
            ;;
        --no-release-artifacts)
            release_artifacts=0
            shift
            ;;
        --sbuild)
            sbuild=1
            shift
            ;;
        -h | --help)
            usage
            exit 0
            ;;
        *)
            die "unknown option: $1 (try --help)"
            ;;
        esac
    done

    need git autoreconf debuild dcmd dpkg-parsechangelog
    [[ $sbuild -eq 1 ]] && need sbuild
    if [[ $unsigned -eq 0 ]]; then
        need gpg
        [[ -n $key ]] || die "no signing key (pass --key or set DEBSIGN_KEYID, or use --unsigned)"
    fi

    local repo
    repo=$(git rev-parse --show-toplevel) || die "not inside a git repository"
    : "${outdir:=$repo/dist}"
    mkdir -p "$outdir"
    outdir=$(cd "$outdir" && pwd)

    if [[ -n $orig_in ]]; then
        [[ -r $orig_in ]] || die "--orig file not readable: $orig_in"
        orig_in=$(cd "$(dirname "$orig_in")" && pwd)/$(basename "$orig_in")
    fi

    scratch=$(mktemp -d "${TMPDIR:-/tmp}/httrack-mkdeb.XXXXXX")
    trap 'rm -rf -- "$scratch"' EXIT

    # Pristine export of committed HEAD plus the coucal submodule.
    info "exporting committed sources"
    local export_dir=$scratch/src
    mkdir -p "$export_dir"
    git -C "$repo" archive --format=tar HEAD | tar -x -C "$export_dir"
    git -C "$repo/src/coucal" archive --format=tar --prefix=src/coucal/ HEAD |
        tar -x -C "$export_dir"

    # Upstream version and Debian revision drive the orig: revision 1 builds a
    # fresh tarball, revision >= 2 reuses the one frozen at -1 (the .dsc pins it
    # by checksum, so a regenerated orig with new mtimes would be rejected).
    local fullver ver rev
    fullver=$(cd "$export_dir" && dpkg-parsechangelog -S Version)
    ver=${fullver%-*}
    rev=${fullver##*-}
    local orig=httrack_${ver}.orig.tar.gz
    info "version $ver (Debian revision $rev)"

    # A signed build is upload-bound, so a revision >= 2 must reuse the frozen
    # orig (--orig); an unsigned build is a throwaway (CI, local) and may
    # regenerate it, since it can never reach the archive.
    if [[ -z $orig_in && $rev != 1 && $unsigned -eq 0 ]]; then
        die "Debian revision $rev needs --orig FILE (the orig is frozen from revision 1)"
    fi

    if [[ -n $orig_in ]]; then
        info "reusing upstream tarball $orig_in"
        cp -- "$orig_in" "$scratch/$orig"
    else
        # Refresh build system and man page, then build the tarball. We build
        # here only because regen-man needs the compiled binaries; the test
        # suite is not run in this pass. debuild (below) runs the full suite
        # once, online tests enabled, so a check here would just repeat it.
        info "regenerating build system and man page"
        (
            cd "$export_dir"
            autoreconf -fi
            ./configure --quiet
            make -s -j"$(nproc)"
            make -s -C man regen-man
            # Build the tarball from a clean tree so no object files leak in.
            make -s clean
            make -s dist
        )
        local -a tarballs
        shopt -s nullglob
        tarballs=("$export_dir"/httrack-*.tar.gz)
        shopt -u nullglob
        [[ ${#tarballs[@]} -ge 1 ]] || die "make dist produced no tarball"
        local tarball=${tarballs[0]##*/}
        [[ $tarball == "httrack-$ver.tar.gz" ]] ||
            die "changelog version $ver disagrees with built tarball $tarball (configure.ac mismatch?)"
        cp -- "$export_dir/$tarball" "$scratch/$orig"
    fi

    # 3.0 (quilt): orig tarball is upstream-only; debian/ is overlaid on top.
    (
        cd "$scratch"
        tar -xf "$orig"
        [[ -d httrack-$ver ]] || die "orig tarball does not unpack to httrack-$ver/"
        cp -a "$export_dir/debian" "httrack-$ver/debian"
    )

    # Build and sign. debuild runs lintian too but does NOT propagate its exit
    # status, so a broken package would pass unnoticed; disable it here and run
    # lintian ourselves below as the real gate.
    local -a debuild_opts=(--no-lintian)
    local -a build_opts=()
    # -d: a source build runs no debhelper, so don't require Build-Depends
    # locally (the buildds and the --sbuild gate enforce them).
    [[ $source_only -eq 1 ]] && build_opts+=(-S -d)
    if [[ $unsigned -eq 1 ]]; then
        build_opts+=(-us -uc)
    else
        build_opts+=("-k$key")
    fi
    info "building packages with debuild"
    (
        cd "$scratch/httrack-$ver"
        # debuild options (--no-lintian) must precede the dpkg-buildpackage ones
        debuild "${debuild_opts[@]}" "${build_opts[@]}"
    )

    # Collect every file the .changes references (orig, dsc, debs, ddebs, buildinfo).
    info "collecting artifacts into $outdir"
    local -a changes
    shopt -s nullglob
    changes=("$scratch"/*.changes)
    shopt -u nullglob
    [[ ${#changes[@]} -ge 1 ]] || die "debuild produced no .changes file"

    # The real lintian gate (debuild only reports, it does not fail on tags).
    # --profile debian: CI runners are Ubuntu, whose vendor data would wrongly
    # reject the Debian "unstable" distribution. Suppressed tags are stale-local-
    # lintian skew, not package defects: newer-standards-version, and
    # recommended-field (old lintian still wants the Priority field the sid
    # lintian in CI accepts dropping). set -e turns any error/warning tag into
    # a failure.
    info "running lintian gate (--fail-on=error,warning)"
    lintian --profile debian -I -i --fail-on=error,warning \
        --suppress-tags newer-standards-version,recommended-field \
        "${changes[@]}"

    dcmd cp -- "${changes[@]}" "$outdir/"

    # Clean-room build gate: rebuild the source package in a minimal chroot that
    # holds only the declared Build-Depends, the same way the buildds will. An
    # undeclared dependency or any FTBFS aborts the release here instead of
    # surfacing after a source-only upload. Logs and clean-built debs land in
    # $outdir/sbuild for inspection.
    if [[ $sbuild -eq 1 ]]; then
        local -a dscs
        shopt -s nullglob
        dscs=("$scratch"/*.dsc)
        shopt -u nullglob
        [[ ${#dscs[@]} -ge 1 ]] || die "no .dsc to sbuild"

        local dist
        dist=$(cd "$scratch/httrack-$ver" && dpkg-parsechangelog -S Distribution)
        [[ $dist == UNRELEASED ]] && dist=unstable

        info "clean-room build with sbuild (dist $dist)"
        local sbdir=$outdir/sbuild
        rm -rf -- "$sbdir"
        mkdir -p "$sbdir"
        (cd "$sbdir" && sbuild --dist="$dist" -- "${dscs[0]}")
        info "sbuild clean-room build passed; logs in $sbdir"
    fi

    # Release artifacts for the upstream tarball (detached sig + checksums).
    # A Debian revision >= 2 .changes omits the orig (it is already in the
    # archive), so dcmd above won't have copied it; place it from the build tree
    # so the website artifacts are produced regardless of the revision.
    if [[ $release_artifacts -eq 1 && $unsigned -eq 0 ]]; then
        info "signing upstream tarball"
        cp -- "$scratch/$orig" "$outdir/$orig"
        (
            cd "$outdir"
            gpg --armor --detach-sign --yes -u "$key" -- "$orig"
            md5sum -- "$orig" >"$orig.md5"
            sha1sum -- "$orig" >"$orig.sha1"
        )
    fi

    info "done. artifacts in $outdir:"
    ls -1 "$outdir" >&2
}

main "$@"
