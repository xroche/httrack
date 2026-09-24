#!/bin/bash
#
# One staged run of ci-windows-suite.sh over 121 stub tests, for the tests that
# drive the suite itself (337, 495). ci_stage_init reads the pinned skip set and
# builds the stub bindir under $tmp; stage writes one suite, run_suite drives it.
# Sourcing defines the helpers and stages nothing.
#
# $tmp and $testdir come from the test that sources this, and $ran, $rc and
# $arm_took go back to it.
# shellcheck disable=SC2154,SC2034

# Sets $bin, $share, $expected_skips, $nskip and $ran, and skips the caller where
# the staged suite cannot run at all.
ci_stage_init() { # ci_stage_init
    bin=$tmp/bin
    mkdir -p "$bin"
    printf '#!/bin/sh\nexit 0\n' >"$bin/httrack"
    # shellcheck disable=SC2016 # $2 is the stub's own expansion
    printf '#!/bin/sh\necho "$2"\n' >"$bin/cygpath"
    chmod +x "$bin/httrack" "$bin/cygpath"
    # Ahead of the driver's own command -v, which asks access(X_OK) and reads
    # false here.
    test -x "$bin/httrack" ||
        skip "${TMPDIR:-/tmp} is noexec, the stub bindir cannot be run"

    # A stub reports its own runs here, so an arm can tell a retry from a first run.
    share=$tmp/share
    mkdir -p "$share"
    export POOLSHARE=$share

    # The 29 the driver pins as msys's expected skips (the default backend here), so
    # a stub run can reach the gates past the skip-set compare instead of stopping
    # on it.
    expected_skips=$(sed -n '/^expected_skips_msys="/,/"$/p' "$testdir/ci-windows-suite.sh" |
        sed -e 's/^expected_skips_msys="//' -e 's/"$//')
    nskip=$(count_matching_lines . <<<"$expected_skips")
    test "$nskip" -ge 1 || fail "read no expected skips out of the driver"
    # 91 passing stubs clear its floor of 90, and the lost one passes in the
    # control leg.
    ran=$((92 + nskip))
}

stub() { # stub <path> <exit status> [shell line]
    # shellcheck disable=SC2016 # the stub's own expansions, not this shell's
    printf '#!/bin/sh\n%s\nexit %s\n' "${3:-:}" "$2" >"$1"
}

stage() { # stage <dir> <lost stub body>
    local d=$1 t i
    rm -rf "$d"
    mkdir -p "$d/rt"
    cp "$testdir/ci-windows-suite.sh" "$testdir/testlib.sh" "$testdir/proclib.sh" \
        "$testdir/test-timeout.sh" "$d/"
    chmod u+w "$d"/*.sh
    # Neutered, or it would kill a sibling's engine under "make check -j".
    printf 'reap_leftover_processes() { return 0; }\n' >>"$d/proclib.sh"
    # One per glob the driver enumerates, plus enough passes to clear its floor of 90.
    for t in 00_runnable 900_zlib-pass 901_watchdog-pass 902_crawllib-pass \
        903_crawl-harness-pass 904_crawl_proxy_https 905_crawl-log-salvage; do
        stub "$d/$t.test" 0
    done
    i=0
    while test "$i" -lt 84; do
        stub "$d/9$((100 + i))_engine-pass.test" 0
        i=$((i + 1))
    done
    # shellcheck disable=SC2086 # one stub per name, so the splitting is the point
    for t in $expected_skips; do stub "$d/$t" 77; done
    if test -n "${3:-}"; then
        # The killer on a pinned skip: the vanished worker cannot record its
        # skip, so the set gate fires before the lost gate is ever consulted.
        stub "$d/906_engine-lost.test" 0
        stub "$d/$3" 77 "$2"
    else
        stub "$d/906_engine-lost.test" 0 "$2"
    fi
}

# Drive the suite staged in $1 into $tmp/out, its status into $rc and what it cost into
# $arm_took. Each arm paces after its own assertions, or a skip throws away the run it
# has already paid for.
rc=0
arm_took=0
run_suite() { # run_suite <dir>
    local began=$SECONDS
    rc=0
    (
        cd "$1"
        RUNNER_TEMP="$1/rt" GITHUB_STEP_SUMMARY="$tmp/summary" HTTRACK_SUITE_JOBS=6 \
            bash ./ci-windows-suite.sh "$bin"
    ) >"$tmp/out" 2>&1 || rc=$?
    arm_took=$((SECONDS - began))
}
