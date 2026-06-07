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
#   -h, --help                 show this help
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

    # Refresh build system and man page, then build and validate the tarball.
    info "regenerating build system and man page"
    (
        cd "$export_dir"
        autoreconf -fi
        ./configure --quiet
        make -s -j"$(nproc)"
        make -s -C man regen-man
        info "running test suite"
        make -s check
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

    # Build (debuild also runs lintian and signs).
    local -a debuild_opts=(--lintian-opts -I -i)
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

    # Release artifacts for the upstream tarball (detached sig + checksums).
    if [[ $release_artifacts -eq 1 && $unsigned -eq 0 ]]; then
        info "signing upstream tarball"
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
