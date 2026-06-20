#!/usr/bin/env bash
#
# Bootstrap an sbuild chroot for the clean-room build gate (mkdeb.sh --sbuild).
#
# Uses the rootless unshare backend: no root, no schroot daemon. It builds a
# minimal buildd chroot tarball into ~/.cache/sbuild/<dist>-<arch>.tar.zst, where
# sbuild --dist=<dist> finds it automatically in unshare mode.
#
# Usage:
#   tools/mk-sbuild-chroot.sh [options]
#
# Options:
#   -d, --dist DIST       suite to bootstrap (default: unstable)
#   -a, --arch ARCH       architecture (default: dpkg --print-architecture)
#   -m, --mirror URL      apt mirror (default: http://deb.debian.org/debian)
#       --components LIST  comma-separated components (default: main)
#   -f, --force           rebuild even if the tarball already exists
#       --write-sbuildrc  add "$chroot_mode = 'unshare';" to ~/.sbuildrc if absent
#   -h, --help            show this help
#
# One-time setup; refresh later with sbuild-update or by rerunning with --force.
# Requires mmdebstrap and the uidmap tools (newuidmap) for the unshare backend.

set -euo pipefail

readonly PROGNAME=${0##*/}

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
    local dist=unstable
    local arch=""
    local mirror=http://deb.debian.org/debian
    local components=main
    local force=0
    local write_sbuildrc=0

    while [[ $# -gt 0 ]]; do
        case $1 in
        -d | --dist)
            [[ $# -ge 2 ]] || die "missing argument for $1"
            dist=$2
            shift 2
            ;;
        -a | --arch)
            [[ $# -ge 2 ]] || die "missing argument for $1"
            arch=$2
            shift 2
            ;;
        -m | --mirror)
            [[ $# -ge 2 ]] || die "missing argument for $1"
            mirror=$2
            shift 2
            ;;
        --components)
            [[ $# -ge 2 ]] || die "missing argument for $1"
            components=$2
            shift 2
            ;;
        -f | --force)
            force=1
            shift
            ;;
        --write-sbuildrc)
            write_sbuildrc=1
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

    need mmdebstrap dpkg
    # Unshare needs the setuid uid/gid mappers; mmdebstrap fails cryptically without.
    command -v newuidmap >/dev/null 2>&1 ||
        die "newuidmap not found; install the uidmap package for the unshare backend"

    # Unshare maps a whole UID range, not just the caller's: the base install
    # creates system users, and without an /etc/subuid+subgid range the install
    # crashes (dpkg SIGSEGV) instead of erroring cleanly. Root uses mode=root and
    # needs no range.
    if [[ $(id -u) -ne 0 ]]; then
        local me
        me=$(id -un)
        if ! grep -qs "^$me:" /etc/subuid || ! grep -qs "^$me:" /etc/subgid; then
            # Suggest a range starting past every allocation in either file.
            local start
            start=$(awk -F: '{e = $2 + $3; if (e > m) m = e} END {print (m ? m : 100000)}' \
                /etc/subuid /etc/subgid 2>/dev/null)
            die "no /etc/subuid+subgid range for $me; the unshare backend needs one:
  sudo usermod --add-subuids $start-$((start + 65535)) --add-subgids $start-$((start + 65535)) $me"
        fi
    fi

    : "${arch:=$(dpkg --print-architecture)}"
    local cache=$HOME/.cache/sbuild
    local tarball=$cache/${dist}-${arch}.tar.zst

    if [[ -e $tarball && $force -eq 0 ]]; then
        info "chroot already exists: $tarball (use --force to rebuild)"
    else
        info "bootstrapping $dist/$arch chroot into $tarball"
        mkdir -p "$cache"
        mmdebstrap --variant=buildd --arch="$arch" --components="$components" \
            "$dist" "$tarball" "$mirror"
        info "chroot ready: $tarball"
    fi

    local rc=$HOME/.sbuildrc
    local mode_line="\$chroot_mode = 'unshare';"
    # shellcheck disable=SC2016  # $chroot_mode is literal regex text, not a shell var.
    if grep -qsE '^[[:space:]]*\$chroot_mode[[:space:]]*=.*unshare' "$rc"; then
        : # already configured (active, non-commented line)
    elif [[ $write_sbuildrc -eq 1 ]]; then
        info "enabling the unshare backend in $rc"
        printf '%s\n' "$mode_line" >>"$rc"
    else
        cat >&2 <<EOF
==> To use this chroot without passing --chroot-mode each time, add to $rc:
      $mode_line
    (or rerun with --write-sbuildrc). Then verify with:
      sbuild --dist=$dist path/to/package.dsc
    and build the release gate with:
      tools/mkdeb.sh --source-only --sbuild
EOF
    fi
}

main "$@"
