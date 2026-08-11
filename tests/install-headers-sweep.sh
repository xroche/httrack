#!/bin/bash
#
# Compile every installed header on its own and in each ordered pair, in both
# HTS_INTERNAL_BYTECODE states, one compiler run per language: on the Windows
# runner a spawn per unit costs more than the compile. 269 drives this over what
# automake installed; the MSVC job has no automake and stages the same list out
# of DevIncludes_DATA instead (#1153). Units are named after the headers they
# include, so the compiler's own diagnostic says which pair broke.

set -euo pipefail

# shellcheck source=tests/testlib.sh
. "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/testlib.sh"

usage() {
    echo "usage: ${0##*/} {--srcdir DIR [--builddir DIR] | --headers-dir DIR}" \
        "[--backend cl|cc] [--cc CMD] [--cxx CMD] [--self-test] [-- CPPFLAGS...]" >&2
    exit 1
}

srcdir=""
builddir=""
hdrdir=""
backend=cc
cc_cmd=""
cxx_cmd=""
cxx_set=0
selftest=0
extra=()
while [ $# -gt 0 ]; do
    case $1 in
    --self-test)
        selftest=1
        shift
        ;;
    --srcdir)
        srcdir=${2-}
        shift 2 || usage
        ;;
    --builddir)
        builddir=${2-}
        shift 2 || usage
        ;;
    --headers-dir)
        hdrdir=${2-}
        shift 2 || usage
        ;;
    --backend)
        backend=${2-}
        shift 2 || usage
        ;;
    --cc)
        cc_cmd=${2-}
        shift 2 || usage
        ;;
    --cxx)
        cxx_cmd=${2-}
        cxx_set=1
        shift 2 || usage
        ;;
    --)
        shift
        extra=("$@")
        break
        ;;
    *) usage ;;
    esac
done

# One flag vocabulary per compiler driver. Both read @file, so the batch never
# reaches the command line.
case $backend in
cl)
    : "${cc_cmd:=cl}"
    [ "$cxx_set" = 1 ] || cxx_cmd=$cc_cmd
    # -W3 matches the vcxproj and no -WX: the property under test is that a
    # consumer's unit compiles at all. -MP for the runner's cores.
    common=(-nologo -c -W3 -MP)
    c_lang=(-TC)
    cxx_lang=(-TP)
    ;;
cc)
    : "${cc_cmd:=${CC:-cc}}"
    [ "$cxx_set" = 1 ] || cxx_cmd=${CXX:-c++}
    common=(-fsyntax-only)
    c_lang=(-x c)
    cxx_lang=(-x c++)
    ;;
*) usage ;;
esac
read -r -a cc_argv <<<"$cc_cmd"
read -r -a cxx_argv <<<"$cxx_cmd"
langs=(c)
if [ "${#cxx_argv[@]}" -gt 0 ]; then
    langs+=(cxx)
else
    echo "note: no C++ compiler given, C leg only" >&2
fi

tmp=$(mktemp -d) || exit 1
cleanup_push rm -rf "$tmp"
mkdir -p "$tmp/include/httrack" "$tmp/tu" "$tmp/obj"

headers=()
if [ -n "$hdrdir" ]; then
    [ -z "$srcdir" ] || usage
    for h in "$hdrdir"/*.h; do
        cp "$h" "$tmp/include/httrack/"
        headers+=("${h##*/}")
    done
else
    [ -n "$srcdir" ] || usage
    srcdir=$(cd "$srcdir" && pwd) || fail "no such source tree"
    [ -f "$srcdir/src/Makefile.am" ] || fail "$srcdir is not a source tree"
    [ -z "$builddir" ] || builddir=$(cd "$builddir" && pwd) || fail "no such build tree"
    declared=()
    while read -r h; do declared+=("$h"); done < <(abs_top_srcdir=$srcdir declared_headers)
    [ "${#declared[@]}" -ge 10 ] ||
        fail "read only ${#declared[@]} headers out of DevIncludes_DATA, the parse is wrong"
    # An entry outside src/ is a build product; MSVC generates no config.h, which
    # htsglobal.h includes on its POSIX branch only.
    absent=()
    for h in "${declared[@]}"; do
        from=$srcdir/src/$h
        case $h in
        ../*) [ -z "$builddir" ] || from=$builddir/${h#../} ;;
        esac
        if [ -f "$from" ]; then
            cp "$from" "$tmp/include/httrack/${h##*/}"
            headers+=("${h##*/}")
        elif [ "${h#../}" != "$h" ]; then
            absent+=("$h")
        else
            fail "$h is in DevIncludes_DATA but not in $srcdir/src"
        fi
    done
    [ "${#absent[@]}" -eq 0 ] ||
        echo "note: not built here, so out of the sweep: ${absent[*]}" >&2
fi

n=${#headers[@]}
[ "$n" -ge 10 ] || fail "only $n headers to sweep, the list cannot be right"

# One unit per (bytecode mode, first header, optional second header). The define
# rides in the unit, so the whole batch shares one command line.
gen() { # gen NAME BYTECODE HEADER...
    local f=$1 def=$2
    shift 2
    {
        [ "$def" = 0 ] || printf '#define HTS_INTERNAL_BYTECODE 1\n'
        printf '#include <httrack/%s>\n' "$@"
    } >"$tmp/tu/$f.c"
}

units=()
for def in 0 1; do
    sfx=""
    [ "$def" = 0 ] || sfx="__bc"
    for a in "${headers[@]}"; do
        gen "${a%.h}$sfx" "$def" "$a"
        units+=("../tu/${a%.h}$sfx.c")
        for b in "${headers[@]}"; do
            [ "$a" != "$b" ] || continue
            gen "${a%.h}--${b%.h}$sfx" "$def" "$a" "$b"
            units+=("../tu/${a%.h}--${b%.h}$sfx.c")
        done
    done
done
# 2 modes x n first headers x (itself + n-1 seconds). A generator that lost a
# nesting level, or that collided two names, would still sweep something.
[ "${#units[@]}" -eq $((2 * n * n)) ] ||
    fail "generated ${#units[@]} units, want $((2 * n * n))"
written=("$tmp"/tu/*.c)
[ "${#written[@]}" -eq "${#units[@]}" ] || fail "the unit names are not unique"

# Run from obj/ with relative operands: cl drops each .obj in the working
# directory, and MSYS rewrites no argument that is not a leading-slash path.
cd "$tmp/obj"

sweep_log=$tmp/obj/cc.log
compile() { # compile c|cxx UNIT...
    local lang=$1 rc=0
    shift
    {
        printf '%s\n' "${common[@]}" -I../include
        [ "${#extra[@]}" -eq 0 ] || printf '%s\n' "${extra[@]}"
        if [ "$lang" = c ]; then printf '%s\n' "${c_lang[@]}"; else printf '%s\n' "${cxx_lang[@]}"; fi
        printf '%s\n' "$@"
    } >batch.rsp
    if [ "$lang" = c ]; then
        "${cc_argv[@]}" @batch.rsp >"$sweep_log" 2>&1 || rc=$?
    else
        "${cxx_argv[@]}" @batch.rsp >"$sweep_log" 2>&1 || rc=$?
    fi
    return "$rc"
}

# Nothing below means anything if the driver cannot find its own runtime headers,
# or cannot report a bad unit sitting in the middle of a batch.
printf '#include <stdio.h>\n' >"$tmp/tu/ok.c"
printf '#include "no-such-header-1153.h"\n' >"$tmp/tu/missing.c"
# Synthetic, because asserting that a real header still breaks would forbid ever
# hardening it.
printf '#define HTS_SWEEP_TAKEN 1\n' >"$tmp/include/httrack/sweep-first.h"
printf '#ifndef HTS_SWEEP_TAKEN\ntypedef int sweep_t;\n#endif\nsweep_t sweep_f(void);\n' \
    >"$tmp/include/httrack/sweep-second.h"
gen good 0 sweep-second.h sweep-first.h
gen bad 0 sweep-first.h sweep-second.h

for lang in "${langs[@]}"; do
    compile "$lang" ../tu/ok.c ../tu/good.c || {
        cat "$sweep_log" >&2
        fail "the $backend $lang driver cannot compile <stdio.h> and the control pair"
    }
    ! compile "$lang" ../tu/missing.c 2>/dev/null ||
        fail "the $backend $lang driver accepted a missing include, the sweep proves nothing"
    ! compile "$lang" ../tu/ok.c ../tu/bad.c ../tu/good.c 2>/dev/null ||
        fail "the $backend $lang driver missed a one-order-only pair inside a batch"
done
rm -f "$tmp/include/httrack/sweep-first.h" "$tmp/include/httrack/sweep-second.h" \
    "$tmp/tu/ok.c" "$tmp/tu/missing.c" "$tmp/tu/good.c" "$tmp/tu/bad.c"

# Staging, generation and the controls, without paying for the whole set again.
if [ "$selftest" = 1 ]; then
    echo "self-test ok: $n headers staged, ${#units[@]} units, ${langs[*]} controls fire"
    exit 0
fi

began=$SECONDS
bad=0
for lang in "${langs[@]}"; do
    compile "$lang" "${units[@]}" || {
        head -40 "$sweep_log" >&2
        echo "the installed headers do not compile as $lang standalone and pairwise" >&2
        bad=1
    }
done
echo "swept $n headers standalone and pairwise x 2 bytecode modes x ${langs[*]}" \
    "= $((${#langs[@]} * ${#units[@]})) units in $((SECONDS - began))s with $backend"
[ "$bad" -eq 0 ] || exit 1
exit 0
