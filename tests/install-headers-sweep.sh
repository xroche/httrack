#!/bin/bash
#
# Compile every installed header on its own and in each ordered pair, in both
# HTS_INTERNAL_BYTECODE states. One compiler run per batch: a spawn per unit
# costs more than the compile on the Windows runner. 269 sweeps what automake
# installed; the MSVC job has no automake and stages the same DevIncludes_DATA
# list out of the source tree (#1153). Units carry the names of the headers they
# include, so the compiler's own diagnostic says which pair broke.

set -euo pipefail

# shellcheck source=tests/testlib.sh
. "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/testlib.sh"

usage() {
    echo "usage: ${0##*/} {--srcdir DIR [--builddir DIR] | --headers-dir DIR}" \
        "[--backend cl|cc] [--cc CMD] [--cxx CMD] [--budget SECONDS] [--self-test]" \
        "[-- CPPFLAGS...]" >&2
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
budget=
extra=()
while [ $# -gt 0 ]; do
    case $1 in
    --self-test)
        selftest=1
        shift
        ;;
    --budget)
        budget=${2-}
        shift 2 || usage
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
    # An entry outside src/ is a build product: it comes from the build tree or
    # not at all. MSVC runs no configure, so the htsfeatures.h that htsglobal.h
    # reads on its POSIX branch is absent there. Never $srcdir/src/../, which
    # would stage a stray file.
    absent=()
    for h in "${declared[@]}"; do
        case $h in
        ../*) from=${builddir:+$builddir/${h#../}} ;;
        *) from=$srcdir/src/$h ;;
        esac
        if [ -n "$from" ] && [ -f "$from" ]; then
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
# The count alone cannot see a Makefile.am reformat the awk mis-parses: a set that
# lost these and stayed above the floor would sweep, and pass, without them.
for h in htsglobal.h httrack-library.h htssafe.h htsbasenet.h htsopt.h; do
    [ -f "$tmp/include/httrack/$h" ] || fail "$h is not in the set to sweep"
done

modes=(-UHTS_INTERNAL_BYTECODE -DHTS_INTERNAL_BYTECODE)

gen() { # gen NAME HEADER...
    local f=$1
    shift
    printf '#include <httrack/%s>\n' "$@" >"$tmp/tu/$f.c"
}

units=()
for a in "${headers[@]}"; do
    gen "${a%.h}" "$a"
    units+=("../tu/${a%.h}.c")
    for b in "${headers[@]}"; do
        [ "$a" != "$b" ] || continue
        gen "${a%.h}--${b%.h}" "$a" "$b"
        units+=("../tu/${a%.h}--${b%.h}.c")
    done
done
# n first headers x (itself + n-1 seconds). A generator that lost a nesting
# level, or that collided two names, would still sweep something.
[ "${#units[@]}" -eq $((n * n)) ] || fail "generated ${#units[@]} units, want $((n * n))"
written=("$tmp"/tu/*.c)
[ "${#written[@]}" -eq "${#units[@]}" ] || fail "the unit names are not unique"

# Run from obj/ with relative operands. cl drops each .obj in the working
# directory, and MSYS rewrites no argument that is not a leading-slash path, so
# anything passed after -- must be absolute.
cd "$tmp/obj"

sweep_log=$tmp/obj/cc.log
compile() { # compile c|cxx MODE UNIT...
    local lang=$1 mode=$2 rc=0
    shift 2
    {
        printf '%s\n' "${common[@]}" -I../include "$mode"
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
# A header must react to the bytecode mode, or both halves run the same case
# twice and the unit count still reads right. TEST_CPPFLAGS carrying -D does it.
printf '#ifndef HTS_INTERNAL_BYTECODE\n#error not in bytecode mode\n#endif\n' \
    >"$tmp/include/httrack/sweep-bytecode.h"
gen good sweep-second.h sweep-first.h
gen bad sweep-first.h sweep-second.h
gen bytecode sweep-bytecode.h

for lang in "${langs[@]}"; do
    compile "$lang" "${modes[0]}" ../tu/ok.c ../tu/good.c || {
        cat "$sweep_log" >&2
        fail "the $backend $lang driver cannot compile <stdio.h> and the control pair"
    }
    ! compile "$lang" "${modes[0]}" ../tu/missing.c ||
        fail "the $backend $lang driver accepted a missing include, the sweep proves nothing"
    ! compile "$lang" "${modes[0]}" ../tu/ok.c ../tu/bad.c ../tu/good.c ||
        fail "the $backend $lang driver missed a one-order-only pair inside a batch"
    compile "$lang" "${modes[1]}" ../tu/bytecode.c ||
        fail "${modes[1]} does not reach a $lang header, so that half of the sweep is a duplicate"
    ! compile "$lang" "${modes[0]}" ../tu/bytecode.c ||
        fail "${modes[0]} leaves HTS_INTERNAL_BYTECODE defined for $lang, so both halves are one"
done
rm -f "$tmp/include/httrack/sweep-first.h" "$tmp/include/httrack/sweep-second.h" \
    "$tmp/include/httrack/sweep-bytecode.h" "$tmp/tu/ok.c" "$tmp/tu/missing.c" \
    "$tmp/tu/good.c" "$tmp/tu/bad.c" "$tmp/tu/bytecode.c"

# Staging, generation and the controls, without paying for the whole set again.
if [ "$selftest" = 1 ]; then
    echo "self-test ok: $n headers staged, ${#units[@]} units, ${langs[*]} controls fire"
    exit 0
fi

began=$SECONDS
bad=0
# Sliced only for a caller that gave a budget: one call per batch cannot be given up on,
# and an emulated compiler needs more time for it than the harness allows a test (#1146).
# Unsliced elsewhere, so the Windows job keeps paying one compiler spawn per batch.
if [ -n "$budget" ] && [ "$budget" -gt 0 ]; then
    export HTTRACK_TEST_TIMEOUT=$budget
    slices=8
else
    slices=1
fi
slice=$(((${#units[@]} + slices - 1) / slices))
# From the slice size, not from $slices: they differ whenever the units do not divide
# evenly, and a step count that outlives the loop leaves the pacer projecting forever.
per=$(((${#units[@]} + slice - 1) / slice))
left=$((${#langs[@]} * ${#modes[@]} * per))
swept=0
for lang in "${langs[@]}"; do
    for mode in "${modes[@]}"; do
        i=0
        while [ "$i" -lt "${#units[@]}" ]; do
            step=$SECONDS
            chunk=("${units[@]:i:slice}")
            swept=$((swept + ${#chunk[@]}))
            compile "$lang" "$mode" "${chunk[@]}" || {
                head -40 "$sweep_log" >&2
                echo "the headers do not compile as $lang standalone and pairwise ($mode)" >&2
                bad=1
            }
            i=$((i + slice))
            left=$((left - 1))
            # Only while nothing has failed: a skip past a real break would bury it.
            [ "$bad" -ne 0 ] || [ -z "$budget" ] ||
                skip_if_out_of_budget "$left" "$((SECONDS - step))"
        done
    done
done
# What reached the compiler, not what was generated: a slice loop that steps past a unit
# would otherwise report the full set and pass.
want=$((${#langs[@]} * ${#modes[@]} * ${#units[@]}))
[ "$swept" -eq "$want" ] || fail "compiled $swept units of $want, the slicing lost some"
echo "swept $n headers standalone and pairwise x ${#modes[@]} bytecode modes x ${langs[*]}" \
    "= $((${#modes[@]} * ${#langs[@]} * ${#units[@]})) units in $((SECONDS - began))s with $backend"
[ "$bad" -eq 0 ] || exit 1
exit 0
