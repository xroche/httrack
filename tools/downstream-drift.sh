#!/bin/bash
#
# Downstream recipes encode assumptions about our tree: a patch's context, a
# path a sed rewrites, a string an ebuild greps for before editing it. Nothing
# tells us when one stops holding -- the packager finds out at their next
# version bump, and we hear about it as a bug. This fetches the live recipes we
# cannot build and reports which of their assumptions our tree no longer meets.
#
# Usage: tools/downstream-drift.sh        (needs ./bootstrap first, and network)

# The recipe patterns matched below are literal third-party text, not shell.
# shellcheck disable=SC2016

set -euo pipefail

top=$(cd "$(dirname "$0")/.." && pwd)
work=$(mktemp -d)
trap 'set +e; rm -rf "$work"' EXIT
trap 'exit 1' HUP INT TERM

# The findings this tree already had when the job was written, so that only new
# drift reds a run. Each is being reported to its packager; delete the id once
# they have acted, and the next run says so if they have not.
acknowledged=(
    gentoo-zlib-tripwire # m4/check_zlib.m4 rewritten around $zlib_want
    gentoo-minizip-patch # ioapi.h defines OF() already
    freebsd-datadir-sed
    freebsd-coucal-cc-sed
    freebsd-shebangfix # src/webhttrack generated since #899
    openbsd-bash-files # same
    openbsd-patch-configure_ac
    openbsd-patch-src_htslib_c
    openbsd-patch-src_minizip_ioapi_h
    openbsd-patch-src_webhttrack
    termux-html-Makefile.in.patch
    termux-htsglobal.h.patch # the install paths follow --prefix since #1469
    termux-src-Makefile.in.patch
    termux-src-htsbacktrace.c.patch
    termux-src-proxy-proxytrack.h.patch # the sys/timeb.h include went away with #1467
    termux-store.c.patch
)

checks=0
known_hits=0

drift=0
die() {
    echo "downstream-drift: $*" >&2
    exit 1
}
ok() {
    checks=$((checks + 1))
    printf 'ok       %s\n' "$*"
}
flag() {
    local id=$1 msg=$2 known
    checks=$((checks + 1))
    for known in ${acknowledged[@]+"${acknowledged[@]}"}; do
        if [ "$known" = "$id" ]; then
            printf 'known    %s: %s\n' "$id" "$msg"
            known_hits=$((known_hits + 1))
            return
        fi
    done
    printf 'DRIFT    %s: %s\n' "$id" "$msg"
    drift=1
}

# A fetch that half-fails must abort: a check run over an empty recipe reports
# clean and proves nothing.
fetch() {
    curl -fsSL --retry 3 --retry-delay 5 -o "$2" "$1" || die "cannot fetch $1"
    [ -s "$2" ] || die "$1 came back empty"
}

# The recipe must still carry the assumption, then our tree must still meet it.
# Order matters: a premise that is gone makes the tree test vacuous, so it is a
# finding of its own rather than a pass.
expect() {
    local id=$1 recipe=$2 pattern=$3 msg=$4
    shift 4
    if ! command grep -qF -- "$pattern" "$recipe"; then
        flag "$id" "the recipe no longer contains '$pattern'; prune this check"
        return
    fi
    if "$@"; then
        ok "$id"
    else
        flag "$id" "$msg"
    fi
}

# A patch that still applies can still be wrong: with fuzz, or hundreds of lines
# from where it was written, it lands somewhere its author never saw. OpenBSD's
# SSL patch matches a generic four-line block 307 lines past its target, and
# would silently edit the wrong AC_CHECK_LIB. Legitimate drift here is single
# digits (-10, -5, 2), so 50 separates the two populations with room to spare.
# Recipes disagree on strip level, so take the first that applies.
carried_patch() {
    local id=$1 url=$2 p out off
    fetch "$url" "$work/patch"
    for p in 0 1 2; do
        out=$(patch -d "$top" "-p$p" --dry-run --force <"$work/patch" 2>&1) || continue
        if command grep -q 'with fuzz' <<<"$out"; then
            flag "$id" "applies only with fuzz, so its context has moved: $(tr '\n' ' ' <<<"$out")"
            return
        fi
        off=$(sed -n 's/.*offset \(-\{0,1\}[0-9]*\) lines.*/\1/p' <<<"$out" |
            tr -d - | sort -n | tail -1)
        if [ "${off:-0}" -gt 50 ]; then
            flag "$id" "applies ${off} lines from where it was written, so it may be landing in the wrong place"
            return
        fi
        ok "$id"
        return
    done
    flag "$id" "no longer applies at any strip level; it landed upstream, or its context moved"
}

# FreeBSD rewrites this glob, OpenBSD's BASH_FILES and FreeBSD's SHEBANG_FILES
# name a file, and Gentoo's DOCS names four.
glob_contains() {
    local pattern=$1
    shift
    command grep -qF -- "$pattern" "$@"
}
coucal_names_gcc() {
    grep -v '^[[:space:]]*#' "$top/src/coucal/Makefile" | grep -qw gcc
}

# The Termux patches target generated files, so a checkout that skipped
# ./bootstrap would report them as drift for our reason, not theirs.
if [ ! -f "$top/src/Makefile.in" ] || [ ! -f "$top/html/Makefile.in" ]; then
    die "run ./bootstrap first: the generated files Termux patches are missing"
fi

# Whoever reads a failed run has none of this context, so state it up front.
echo "Gentoo, FreeBSD, OpenBSD and Termux each patch this tree to build it. Their"
echo "live recipes, re-checked below: ok = still fits, known = a mismatch already"
echo "reported to its packager, DRIFT = a new one."
echo

### Gentoo -- www-client/httrack, read through the GitHub mirror because
### gitweb.gentoo.org has no stable raw URL for a version-numbered ebuild.
gentoo_raw=https://raw.githubusercontent.com/gentoo-mirror/gentoo/master/www-client/httrack
ebuild=$(curl -fsSL https://api.github.com/repos/gentoo-mirror/gentoo/contents/www-client/httrack |
    sed -n 's/.*"name": "\(httrack-[0-9.]*\.ebuild\)".*/\1/p' | head -1)
test -n "$ebuild" || die "no ebuild found in the Gentoo mirror listing"
fetch "$gentoo_raw/$ebuild" "$work/gentoo.ebuild"

expect gentoo-zlib-tripwire "$work/gentoo.ebuild" '{ZLIB_HOME}/lib' \
    "src_prepare greps m4/check_zlib.m4 for it and dies without it, so every non-lib libdir (amd64) fails to build" \
    command grep -qF '{ZLIB_HOME}/lib' "$top/m4/check_zlib.m4"
expect gentoo-docs "$work/gentoo.ebuild" 'DOCS=( AUTHORS README greetings.txt history.txt )' \
    "one of the four DOCS files is gone; einstalldocs dies" \
    test -f "$top/AUTHORS" -a -f "$top/README" -a -f "$top/greetings.txt" -a -f "$top/history.txt"
carried_patch gentoo-minizip-patch "$gentoo_raw/files/httrack-3.48.13-minizip.patch"

### FreeBSD -- www/httrack
fetch https://raw.githubusercontent.com/freebsd/freebsd-ports/main/www/httrack/Makefile \
    "$work/freebsd.mk"

expect freebsd-datadir-sed "$work/freebsd.mk" 's|/usr/share|${PREFIX}/share|' \
    "the REINPLACE over html/server/div/WebHTTrack* rewrites nothing" \
    glob_contains /usr/share "$top"/html/server/div/WebHTTrack*
expect freebsd-coucal-cc-sed "$work/freebsd.mk" 's|gcc|${CC}|' \
    "src/coucal/Makefile names gcc only in a comment; it honours CC already" \
    coucal_names_gcc
expect freebsd-shebangfix "$work/freebsd.mk" 'SHEBANG_FILES=	src/webhttrack' \
    "src/webhttrack is generated from webhttrack.in since #899, so it does not exist at post-extract time" \
    test -f "$top/src/webhttrack"

### OpenBSD -- www/httrack
obsd_raw=https://raw.githubusercontent.com/openbsd/ports/master/www/httrack
fetch "$obsd_raw/Makefile" "$work/openbsd.mk"

expect openbsd-online-tests "$work/openbsd.mk" '--enable-online-unit-tests=no' \
    "configure no longer offers --enable-online-unit-tests" \
    command grep -qF online-unit-tests "$top/configure.ac"
expect openbsd-bash-files "$work/openbsd.mk" '${WRKSRC}/src/webhttrack' \
    "src/webhttrack is generated from webhttrack.in since #899, and the pre-configure perl dies on a missing file" \
    test -f "$top/src/webhttrack"
for p in patch-configure_ac patch-src_htslib_c patch-src_md5_h \
    patch-src_minizip_ioapi_h patch-src_webhttrack; do
    carried_patch "openbsd-$p" "$obsd_raw/patches/$p"
done

### Termux -- packages/httrack
termux_raw=https://raw.githubusercontent.com/termux/termux-packages/master/packages/httrack
fetch "$termux_raw/build.sh" "$work/termux.sh"

expect termux-werror-sed "$work/termux.sh" 's/-Werror/-Wno-error/g' \
    "configure.ac no longer passes -Werror, so the sed is dead" \
    command grep -qF -- -Werror "$top/configure.ac"
expect termux-with-zlib "$work/termux.sh" --with-zlib \
    "configure no longer offers --with-zlib" \
    command grep -qF -- --with-zlib "$top/m4/check_zlib.m4"
for p in html-Makefile.in.patch htsglobal.h.patch src-Makefile.in.patch \
    src-htsbacktrace.c.patch src-proxy-proxytrack.h.patch store.c.patch; do
    carried_patch "termux-$p" "$termux_raw/$p"
done

echo
# A check that stops running reports nothing, which reads exactly like a clean
# tree. Pin the count against the table above.
test "$checks" -ge 21 || die "only $checks checks ran; the table lost some"

if [ "$drift" -ne 0 ]; then
    echo "A DRIFT line above is a packaging recipe that no longer fits this tree,"
    echo "so the distribution breaks at its next version bump. The recipe is theirs"
    echo "and so is the fix. Tell the packager, then add the id to \$acknowledged"
    echo "in this script so a later run reds only for what is new."
    exit 1
fi
echo "$checks checks, no new drift; $known_hits are findings already known"
