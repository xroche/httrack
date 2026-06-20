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

    need git autoreconf debuild dcmd
    [[ $sbuild -eq 1 ]] && need sbuild dpkg-parsechangelog
    if [[ $unsigned -eq 0 ]]; then
        need gpg
        [[ -n $key ]] || die "no signing key (pass --key or set DEBSIGN_KEYID, or use --unsigned)"
    fi

    local repo
    repo=$(git rev-parse --show-toplevel) || die "not inside a git repository"
    : "${outdir:=$repo/dist}"
    mkdir -p "$outdir"
    outdir=$(cd "$outdir" && pwd)

    scratch=$(mktemp -d "${TMPDIR:-/tmp}/httrack-mkdeb.XXXXXX")
    trap 'rm -rf -- "$scratch"' EXIT

    # Pristine export of committed HEAD plus the coucal submodule.
    info "exporting committed sources"
    local export_dir=$scratch/src
    mkdir -p "$export_dir"
    git -C "$repo" archive --format=tar HEAD | tar -x -C "$export_dir"
    git -C "$repo/src/coucal" archive --format=tar --prefix=src/coucal/ HEAD |
        tar -x -C "$export_dir"

    # Refresh build system and man page, then build the tarball. We build here
    # only because regen-man needs the compiled binaries; the test suite is not
    # run in this pass. debuild (below) runs the full suite once, with the online
    # tests enabled, so a check here would just be a slower, offline-only repeat.
    info "regenerating build system and man page"
    (
        cd "$export_dir"
        autoreconf -fi
        ./configure --quiet
        make -s -j"$(nproc)"
        make -s -C man regen-man
        # Build the tarball from a clean tree so no object files leak into it.
        make -s clean
        make -s dist
    )

    local tarball ver
    local -a tarballs
    shopt -s nullglob
    tarballs=("$export_dir"/httrack-*.tar.gz)
    shopt -u nullglob
    [[ ${#tarballs[@]} -ge 1 ]] || die "make dist produced no tarball"
    tarball=${tarballs[0]##*/}
    ver=${tarball#httrack-}
    ver=${ver%.tar.gz}
    info "version $ver"

    # 3.0 (quilt): orig tarball is upstream-only; debian/ is overlaid on top.
    local orig=httrack_${ver}.orig.tar.gz
    cp -- "$export_dir/$tarball" "$scratch/$orig"
    (
        cd "$scratch"
        tar -xf "$orig"
        cp -a "$export_dir/debian" "httrack-$ver/debian"
    )

    # Build (debuild also runs lintian and signs). --fail-on aborts on a lintian
    # error or warning, so neither a release nor CI produces an unclean package.
    local -a debuild_opts=(--lintian-opts -I -i "--fail-on=error,warning")
    local -a build_opts=()
    [[ $source_only -eq 1 ]] && build_opts+=(-S)
    if [[ $unsigned -eq 1 ]]; then
        build_opts+=(-us -uc)
    else
        build_opts+=("-k$key")
    fi
    info "building packages with debuild"
    (
        cd "$scratch/httrack-$ver"
        debuild "${build_opts[@]}" "${debuild_opts[@]}"
    )

    # Collect every file the .changes references (orig, dsc, debs, ddebs, buildinfo).
    info "collecting artifacts into $outdir"
    local -a changes
    shopt -s nullglob
    changes=("$scratch"/*.changes)
    shopt -u nullglob
    [[ ${#changes[@]} -ge 1 ]] || die "debuild produced no .changes file"
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
