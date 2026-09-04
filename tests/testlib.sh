#!/bin/bash
#
# Helpers shared by the crawl tests. Sourced, not run: it resolves $testdir and
# $top_srcdir, and leaves shell options to the caller, since the suite drivers
# source it too and errexit would end them on the first failing test.

# shellcheck disable=SC2034 # resolved here for the caller, not used here
testdir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# Relative, as each test spelled it before: 100 hands this path straight to
# python.exe, which cannot resolve an absolute MSYS one. make check exports its
# own value, so the default only serves a hand-run and the Windows suite.
: "${top_srcdir:=..}"

# Paths a failure should print, named by VARIABLE: a test that retargets its log
# mid-run keeps the dump pointed at the current one.
FAIL_DUMP_VARS=(${FAIL_DUMP_VARS[@]+"${FAIL_DUMP_VARS[@]}"})
fail_dump_var() { FAIL_DUMP_VARS+=("$@"); }

# Headed and indented, so a dumped log cannot be read as the harness's own
# output. Silent on a missing or empty file: the verdict still stands alone.
dump_file() { # dump_file FILE
    test -s "$1" || return 0
    echo "--- $1" >&2
    sed 's/^/  | /' <"$1" >&2
}

fail() {
    local i v
    echo "FAIL: $*" >&2
    # By index: ${!ARR[@]+...} would read each element as a variable name.
    for ((i = 0; i < ${#FAIL_DUMP_VARS[@]}; i++)); do
        v=${FAIL_DUMP_VARS[i]}
        dump_file "${!v:-}"
    done
    exit 1
}

# fail(), plus the logs the failure is about: a verdict with no evidence sends
# the reader back to a run that no longer exists.
fail_dump() { # fail_dump MSG FILE...
    local msg=$1
    shift
    echo "FAIL: ${msg}" >&2
    while test $# -gt 0; do
        dump_file "$1"
        shift
    done
    exit 1
}

# Cache uname -s once: every skip gate asks for it, and each call is a fork the
# MSYS runtime emulates (#1273). Fatal when it says nothing, never guessed: the
# guess read as not-Windows, so a test excluded there ran on a Windows runner.
: "${HTS_OS:=$(uname -s 2>/dev/null)}"
export HTS_OS
test -n "$HTS_OS" ||
    fail "uname -s said nothing, so no platform gate can be trusted; export HTS_OS to override"

# 77 is automake's "skipped", not a failure.
skip() {
    echo "$*; skipping" >&2
    exit 77
}

# Guard form, so a bare `target_is_windows && skip ...` cannot end an errexit
# test that is merely not on Windows.
skip_on_windows() { ! target_is_windows || skip "$*"; }

# Set by tools/emulated-suite.sh, which interprets every instruction it runs.
is_emulated() { [ -n "${EMULATED_ARCH:-}" ]; }
skip_on_emulated() { ! is_emulated || skip "emulated ${EMULATED_ARCH}: $*"; }

# Assertions, each printing what it wanted beside what arrived: a bare
# `|| exit 1` reds a test without naming the value that differed.
assert_eq() { # assert_eq WANT GOT [LABEL]
    test "$1" = "$2" || fail "${3:+$3: }expected [$1], got [$2]"
}

# Unanchored ERE: ^ and $ are the whole subject's ends, not a line's.
# Keep $1 unquoted here, or bash 3.2 matches it as a literal string.
assert_match() { # assert_match RE TEXT [LABEL]
    [[ $2 =~ $1 ]] || fail "${3:+$3: }no match for /$1/ in: $2"
}

assert_file() { test -f "$1" || fail "${2:+$2: }missing file: $1"; }

# Basenames of the files a mirror wrote, so a check reads what was written
# rather than a path this host may spell differently (macOS $TMPDIR, Windows
# ':' -> '_').
mirror_names() { # mirror_names DIR
    find "$1" -type f | sed 's|.*/||'
}
mirror_has_name() { # mirror_has_name DIR BASENAME
    grep -Fqx "$2" <<<"$(mirror_names "$1")"
}

# Every step skip_if_out_of_budget projects against ran. A skip is an exit, so a
# loop that quietly ran fewer paces against a count that is a lie.
assert_steps_ran() { # assert_steps_ran WANT GOT
    assert_eq "$1" "$2" "steps the budget is paced against"
}

# Run engine self-test NAME: stdout must equal WANT, status must be 0 (which a
# `test "$(...)" = ...` cannot see). expect_ok takes a command, for a real -O.
# Trailing newlines and CRs off a captured run. Windows stdout is text mode, so
# the engine ends every line CRLF; MSYS eats the pair and a Linux shell under
# wsl2 does not, which would make the two backends disagree on every comparison.
# Interior CRs are lines mode's business.
# Into STRIPPED, not echoed: macOS drives this under bash 3.2, which has no
# namerefs, and a command substitution is a fork (#795).
STRIPPED=
strip_trailing_eol() { # strip_trailing_eol TEXT
    STRIPPED=$1
    while :; do
        case $STRIPPED in
        *"$TESTLIB_NL") STRIPPED=${STRIPPED%"$TESTLIB_NL"} ;;
        *"$SELFTEST_CR") STRIPPED=${STRIPPED%"$SELFTEST_CR"} ;;
        *) break ;;
        esac
    done
}

assert_selftest() { # assert_selftest WANT NAME [ARGS...]
    local want=$1 name=$2 got rc=0
    shift 2
    got=$(httrack -O /dev/null "-#test=$name" "$@") || rc=$?
    strip_trailing_eol "$got"
    got=$STRIPPED
    test "$rc" -eq 0 || fail "-#test=$name $*: exited $rc, output: $got"
    test "$got" = "$want" || fail "-#test=$name $*: expected [$want], got [$got]"
}

# Batched form of the assert above: selftest_queue records a case, and
# selftest_run_queued runs a whole file's cases in ONE httrack (-#test=batch in
# htsselftest.c), which the Windows legs pay dearly for otherwise (#795).
# Same contract, except a mismatch surfaces at the flush, named. A case whose
# output later shell logic reads, or that exercises one of htsmain()'s argv
# rewrites, stays on assert_selftest.
SELFTEST_RS=$(printf '\036')
SELFTEST_CR=$(printf '\r')
SELFTEST_SCRIPT=${SELFTEST_SCRIPT:-}
# Where the file started, to catch a cd that leaves HTTRACK_PATH unresolvable.
TESTLIB_PWD=${TESTLIB_PWD:-$PWD}
# Self-preserving, as CLEANUP_ARGV below and for the same reason. ARGV holds
# <argc> <name> <args...> per case, which is the engine's own script format.
SELFTEST_ARGV=(${SELFTEST_ARGV[@]+"${SELFTEST_ARGV[@]}"})
SELFTEST_WANT=(${SELFTEST_WANT[@]+"${SELFTEST_WANT[@]}"})
SELFTEST_MODE=(${SELFTEST_MODE[@]+"${SELFTEST_MODE[@]}"})
SELFTEST_LABEL=(${SELFTEST_LABEL[@]+"${SELFTEST_LABEL[@]}"})

selftest_queue() { # selftest_queue WANT NAME [ARGS...]
    selftest_queue_mode exact "$@"
}

# selftest_queue for output spanning several lines: Windows stdout is text mode,
# so every interior newline arrives as CRLF and a byte-exact compare rejects it.
# Only the CR before a newline goes, or a test asserting the engine drops an
# interior CR would pass whatever the engine did.
selftest_queue_lines() { # selftest_queue_lines WANT NAME [ARGS...]
    selftest_queue_mode lines "$@"
}

# selftest_queue asserting one end of the output rather than all of it, for a
# name whose other end the caller has no reason to spell out. Both WANTs are
# literal and carry their own boundary ("/name", not "name").
selftest_queue_tail() { # selftest_queue_tail WANT NAME [ARGS...]
    selftest_queue_mode tail "$@"
}

selftest_queue_head() { # selftest_queue_head WANT NAME [ARGS...]
    selftest_queue_mode head "$@"
}

# Queue NAME once per base-dir length across a full segment period: the tree the
# long-path self-tests build starts at the base's own length, so a length-blind
# stop bound clears MAX_PATH for some bases and lands exactly on it for others
# (#1409). What detects that is st_mkdeep's own assertf, which aborts the engine
# and so the whole batch; WANT only pins that each base length got that far.
selftest_queue_base_lengths() { # selftest_queue_base_lengths MODE WANT NAME DIR
    local mode=$1 want=$2 name=$3 dir=$4 pad i
    # 41 is st_mkdeep's ASCII segment, so this covers every residue.
    local width=41 before=${#SELFTEST_WANT[@]} accent
    # The base is non-ASCII too, or the length axis is all the sweep exercises:
    # a byte count overstates the UTF-16 units Windows measures MAX_PATH in.
    accent=$(printf '\303\251%.0s' $(seq 1 15)) # 15 x U+00E9: 30 bytes, 15 units
    for ((i = 1; i <= width; i++)); do
        pad=$accent$(repeat_chars "$i" p)
        mkdir "$dir/$pad" || fail "cannot create $dir/$pad"
        selftest_queue_mode "$mode" "$want" "$name" "$dir/$pad"
    done
    # A sweep that queued nothing would be a silent pass.
    test "$((${#SELFTEST_WANT[@]} - before))" -eq "$width" ||
        fail "queued $((${#SELFTEST_WANT[@]} - before)) of $width cases"
}

# Queue a -#test=filtersize case (nothing to do with -#test=fsize): a negative
# SIZE means the size is not known yet, as at scan time.
fsize() { # fsize WANT SIZE STRING [FILTER...]
    local want=$1
    shift
    selftest_queue "$want" filtersize "$@"
}

selftest_queue_mode() { # selftest_queue_mode exact|lines|tail|head WANT NAME [ARGS...]
    local mode=$1 want=$2 name=$3
    shift 3
    case $mode in
    exact | lines) ;;
    # An empty want is a prefix and a suffix of everything, so it would assert
    # nothing at all.
    head | tail) test -n "$want" || fail "a $mode want must not be empty" ;;
    *) fail "unknown selftest mode $mode" ;;
    esac
    SELFTEST_WANT+=("$want")
    SELFTEST_MODE+=("$mode")
    SELFTEST_LABEL+=("-#test=$name $*")
    SELFTEST_ARGV+=("$#" "$name" ${1+"$@"})
    test "${#SELFTEST_WANT[@]}" -ne 1 || {
        SELFTEST_SCRIPT=${TMPDIR:-/tmp}/httrack-selftest-batch.$$
        cleanup_push rm -f "$SELFTEST_SCRIPT"
    }
}

# Run every queued case and assert each. The script goes through a file, not a
# pipe: printf is a builtin, so this costs no fork of its own. Args travel on
# stdin because the engine parses argv before it reaches the self-test dispatch,
# and would take a case's '-*' for one of its own filters.
# The queued arguments, NUL-separated, for -#test=batch to read back. This file
# stands in for the command line, so a drvfs path in it needs the translation
# the shim gives a real argument: the engine reads the file itself and the shim
# never sees it. Left alone under the other backends, where the paths the tests
# build are already native.
selftest_write_argv() {
    local a
    if test "$(suite_backend)" != wsl2; then
        printf '%s\0' "${SELFTEST_ARGV[@]}"
        return 0
    fi
    for a in "${SELFTEST_ARGV[@]}"; do
        case $a in
        /mnt/[A-Za-z]/*) printf '%s\0' "$(drvfs_path -m "$a")" ;;
        file:///mnt/[A-Za-z]/*) printf 'file://%s\0' "$(drvfs_path -m "${a#file://}")" ;;
        *) printf '%s\0' "$a" ;;
        esac
    done
}

selftest_run_queued() {
    local n=${#SELFTEST_WANT[@]} out rest got want rc=0 i
    test "$n" -gt 0 || return 0
    selftest_write_argv >"$SELFTEST_SCRIPT" ||
        fail "cannot write $SELFTEST_SCRIPT"
    # By path, not by name: make check's ../src is relative, so resolving it from
    # anywhere else finds an installed httrack and grades the wrong binary.
    test -n "$HTTRACK_PATH" || test "$PWD" = "$TESTLIB_PWD" ||
        fail "this test cd'd out of $TESTLIB_PWD: assign HTTRACK_PATH=\$(httrack_path) before it moves"
    test -n "$HTTRACK_PATH" || httrack_path >/dev/null
    out=$("$HTTRACK_PATH" -O /dev/null -#test=batch <"$SELFTEST_SCRIPT") || rc=$?
    rm -f "$SELFTEST_SCRIPT"
    test "$rc" -eq 0 || fail "-#test=batch: exited $rc over $n cases, output: $out"
    rest=$out
    for ((i = 0; i < n; i++)); do
        got=${rest%%"$SELFTEST_RS"*}
        rest=${rest#*"$SELFTEST_RS"}
        rc=${rest%%"$SELFTEST_RS"*}
        rest=${rest#*"$SELFTEST_RS"}
        case $rc in '' | *[!0-9]*)
            fail "-#test=batch: framing lost at case $((i + 1)) of $n, output: $out"
            ;;
        esac
        # The whole trailing run, which is what a command substitution drops
        # around a lone case. The CR goes with it: Windows stdout is text mode,
        # so every case ends CRLF there and MSYS eats the pair. Stripping the
        # run rather than one terminator keeps the two platforms saying the
        # same thing. Interior CRs are lines mode's business.
        strip_trailing_eol "$got"
        got=$STRIPPED
        test "${SELFTEST_MODE[i]}" != lines ||
            got=${got//"$SELFTEST_CR$TESTLIB_NL"/"$TESTLIB_NL"}
        want=${SELFTEST_WANT[i]}
        test "$rc" -eq 0 || fail "${SELFTEST_LABEL[i]}: exited $rc, output: $got"
        # Case patterns, where the quoted want stays literal even when the name
        # it pins carries a '*' or a '['.
        case ${SELFTEST_MODE[i]} in
        tail)
            case $got in
            *"$want") ;;
            *) fail "${SELFTEST_LABEL[i]}: expected an output ending [$want], got [$got]" ;;
            esac
            ;;
        head)
            case $got in
            "$want"*) ;;
            *) fail "${SELFTEST_LABEL[i]}: expected an output starting [$want], got [$got]" ;;
            esac
            ;;
        *) test "$got" = "$want" || fail "${SELFTEST_LABEL[i]}: expected [$want], got [$got]" ;;
        esac
    done
    # The count must close exactly: a case the engine ran and nobody asserted,
    # or a self-test that printed the framing byte itself, both land here.
    test -z "$rest" || fail "-#test=batch: output past the $n queued cases: $out"
    SELFTEST_ARGV=()
    SELFTEST_WANT=()
    SELFTEST_MODE=()
    SELFTEST_LABEL=()
}

# Absolute path to httrack, since make check's relative ../src breaks once a test
# cd's away. assert_selftest runs the bare name, so a test that cd's calls this.
# Memoized in HTTRACK_PATH, which selftest_run_queued runs too: a test that cd's
# and queues must assign it (HTTRACK_PATH=$(httrack_path)) before it moves, the
# memo a bare $(httrack_path) writes dying with its subshell.
HTTRACK_PATH=${HTTRACK_PATH:-}
httrack_path() {
    local p dir
    if test -z "$HTTRACK_PATH"; then
        p=$(command -v httrack) || fail "no httrack in PATH"
        # Assigned, so a failed cd is named here instead of a bare /httrack.
        dir=$(cd "$(dirname "$p")" && pwd) || fail "cannot reach $(dirname "$p")"
        HTTRACK_PATH=$dir/$(basename "$p")
    fi
    printf '%s\n' "$HTTRACK_PATH"
}

# A literal, not $'..': Apple's bash 3.2 loses quote state on that inside a
# parameter expansion and the whole file dies of "unexpected EOF".
TESTLIB_NL='
'

# First line of $1. A "| head -1" would close the pipe early and, under pipefail,
# SIGPIPE the producer into a spurious failure.
firstline() { printf '%s\n' "${1%%"$TESTLIB_NL"*}"; }

# LIFO teardown: cleanup_push CMD ARG... registers a command and its arguments,
# expanded now, run in reverse on the way out; the first call installs both traps,
# the signal half of which most tests never wrote (#773). A flat argv, not a shell
# snippet, so no eval -- wrap a redirection or a late value in a function.
# Self-preserving, since 172 sources a driver that sources this file a second time.
CLEANUP_ARGV=(${CLEANUP_ARGV[@]+"${CLEANUP_ARGV[@]}"})
CLEANUP_FRAMES=(${CLEANUP_FRAMES[@]+"${CLEANUP_FRAMES[@]}"})
cleanup_push() {
    CLEANUP_FRAMES+=("${#CLEANUP_ARGV[@]}")
    CLEANUP_ARGV+=("$@")
    test "${#CLEANUP_FRAMES[@]}" -eq 1 || return 0
    trap 'set +e; run_cleanups' EXIT
    # No PIPE: bash cannot trap a signal it inherited as ignored, which is how the
    # runners hand SIGPIPE down, and a real one drains via the EXIT trap (#1136).
    trap 'set +e; run_cleanups; exit 1' HUP INT QUIT TERM
}

# Drains the stack, so the EXIT trap after a signal is a no-op. Both slices carry
# the `[@]+` guard: on bash 3.2 an empty-array expansion under `set -u` is fatal.
run_cleanups() {
    local i start
    for ((i = ${#CLEANUP_FRAMES[@]} - 1; i >= 0; i--)); do
        start=${CLEANUP_FRAMES[i]}
        ${CLEANUP_ARGV[@]+"${CLEANUP_ARGV[@]:start}"} || true
        CLEANUP_ARGV=(${CLEANUP_ARGV[@]+"${CLEANUP_ARGV[@]:0:start}"})
    done
    CLEANUP_FRAMES=()
}

# Python 3 interpreter, or empty: Windows only installs python.exe, and a bare
# "python" may be 2.x or the Store stub.
find_python() {
    local py names='python3 python'
    # Windows-side under wsl2, which is what keeps the fixture servers and
    # httrack.exe on one side of the boundary: no socket crosses it, and the
    # paths handed to them stay the native ones nativepath already produces.
    # A Linux python3 sitting in the distro would take the same arguments and
    # fail to open every one of them.
    test "$(suite_backend)" != wsl2 || names='python3.exe python.exe'
    # shellcheck disable=SC2086 # the split is what makes it a candidate list
    for py in "${PYTHON:-}" $names; do
        test -n "$py" || continue
        "$py" -c 'import sys; sys.exit(sys.version_info[0] != 3)' 2>/dev/null || continue
        printf '%s\n' "$py"
        return 0
    done
    return 1
}

# curl, or empty. Windows-side under wsl2 for the same reason python is: the
# distro's 127.0.0.1 is its own loopback, and the engine listens on Windows's.
find_curl() {
    local c=curl
    test "$(suite_backend)" != wsl2 || c=curl.exe
    command -v "$c" >/dev/null 2>&1 || return 1
    printf '%s\n' "$c"
}

# WSL2's own drvfs mapping, done here rather than with wslpath, which a bare
# imported rootfs does not ship: `wsl -- wslpath` answers ERROR_PATH_NOT_FOUND.
# Only drvfs paths ever cross this boundary, since the suite keeps its files on
# a Windows volume, so the two-way mapping is the whole of it.
drvfs_path() { # drvfs_path -m|-u PATH
    local p=$2 drive rest
    case "$1" in
    -u)
        # C:/foo, C:\foo -> /mnt/c/foo. Already POSIX: leave it be.
        case "$p" in
        [A-Za-z]:[/\\]*)
            drive=$(printf '%s' "${p%%:*}" | tr '[:upper:]' '[:lower:]')
            rest=${p#?:}
            printf '/mnt/%s%s\n' "$drive" "$(printf '%s' "$rest" | tr '\134' '/')"
            ;;
        *) printf '%s\n' "$p" ;;
        esac
        ;;
    -m)
        # /mnt/c/foo -> C:/foo. Anything else has no drive to name.
        case "$p" in
        /mnt/[A-Za-z]/*)
            drive=$(printf '%s' "$p" | cut -c6 | tr '[:lower:]' '[:upper:]')
            printf '%s:%s\n' "$drive" "$(printf '%s' "$p" | cut -c7-)"
            ;;
        *) printf '%s\n' "$p" ;;
        esac
        ;;
    *) fail "drvfs_path: unknown direction $1" ;;
    esac
}

# cygpath under MSYS, the mapping above under WSL2. A missing cygpath falls
# through to the path unchanged, which is how this has always behaved.
path_convert() { # path_convert -m|-u PATH
    case "$(suite_backend)" in
    wsl2)
        drvfs_path "$1" "$2"
        return 0
        ;;
    msys) ;;
    *)
        printf '%s\n' "$2"
        return 0
        ;;
    esac
    if command -v cygpath >/dev/null 2>&1; then
        cygpath "$1" "$2"
    else
        printf '%s\n' "$2"
    fi
}

# Native form of a path: a non-MSYS binary cannot resolve Git Bash's /d/a/... ones.
nativepath() { path_convert -m "$1"; }

# POSIX form of a path. Anything MSYS splits on a colon needs it, a PATH entry
# below the drive-letter TMPDIR above all.
posixpath() { path_convert -u "$1"; }

# Key before cert in $1/both.pem, the single path load_cert_chain() takes.
make_tls_pem() {
    local dir=$1 src
    src=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
    cat "$src/server.key" "$src/server.crt" >"$dir/both.pem" || {
        echo "FAIL: could not write $dir/both.pem from the fixture in $src" >&2
        exit 1
    }
}

# The over-long strings the bounds tests feed. printf and tr, because
# ${v// /c} is quadratic on the bash 3.2 macOS runners.
repeat_chars() { # repeat_chars COUNT [CHAR]
    # A negative width left-justifies instead of erroring, so a computed
    # COUNT that goes negative would pad silently.
    case $1 in '' | *[!0-9]*) fail "repeat_chars: COUNT is not a number: $1" ;; esac
    printf '%*s' "$1" '' | tr ' ' "${2:-a}"
}

# grep -c counts matching LINES, not matches: two hits on one line count once.
# It also returns 0, not a failing status, when nothing matches. -a throughout,
# inert for GNU grep here but not for one that skips a file it reads as binary.
count_matching_lines() { # count_matching_lines PATTERN [FILE]
    grep -ac "$@" || true
}

count_matching_lines_nocase() { # count_matching_lines_nocase PATTERN [FILE]
    grep -aci "$@" || true
}

count_matching_lines_regexp() { # count_matching_lines_regexp ERE [FILE]
    grep -acE "$@" || true
}

# The engine's error lines in a crawl log. One spelling of the pattern, which
# six call sites used to carry.
count_log_errors() { # count_log_errors LOGFILE
    grep -aciE '^[0-9:]*[[:space:]]Error:' "$1" || true
}

# A passing step, on stdout: the failures go to stderr through fail().
ok() { echo "OK: $*"; }

# Size of file $1 in bytes. wc pads its count on the BSDs.
size_of() { wc -c <"$1" | tr -d '[:space:]'; }

# Lines in file $1, padding stripped for the same reason.
lines_of() { wc -l <"$1" | tr -d '[:space:]'; }

# `which` is not POSIX and lies under MSYS; use `command -v`.
require_httrack() {
    command -v httrack >/dev/null || fail "could not find httrack"
}

# have_feature NAME: is this binary's optional feature NAME on? Asks the binary,
# because --disable-auto-features turns off features the platform still has.
HTS_FEATURES=
have_feature() {
    if test -z "${HTS_FEATURES}"; then
        HTS_FEATURES=$(httrack -#test=features 2>/dev/null) ||
            fail "httrack -#test=features failed"
        test -n "${HTS_FEATURES}" || fail "httrack -#test=features said nothing"
    fi
    grep -q "^$1 1\$" <<<"${HTS_FEATURES}"
}

# Longest surviving run of char $2 in file $1, or 0: the length a field was
# clipped to, read back out of a binary artifact.
runlen() {
    grep -ao "$2\\+" "$1" | awk '{ print length($0) }' | sort -rn | head -n1 || true
}

# Run an engine self-test and require its "<label>: OK" line. No pipe into grep:
# SIGPIPE would mask a failing exit status.
expect_ok() {
    local label="$1" out
    shift
    out=$("$@" 2>&1) || {
        echo "FAIL: ${label} exited non-zero: ${out}"
        exit 1
    }
    case "$out" in
    *"${label}: OK"*) ;;
    *)
        echo "FAIL: ${out}"
        exit 1
        ;;
    esac
}

# A staged install writes to its own DESTDIR but runs make in the ONE build tree
# the suite shares, and a top-level "make install" relinks each libtest plugin in
# place: two at once and one moves away the .so the other is mid-move on, leaving
# the tree short a file make will not rebuild. The lock is one file naming the pid
# that holds it; macOS has no flock(1).
: "${INSTALL_LOCK:=${abs_top_builddir:-.}/.install-lock}"
# Seconds one holder may keep the lock before a waiter calls the run broken. Per
# holder, not per wait: a queue of legitimate installs is not a fault.
: "${INSTALL_LOCK_TIMEOUT:=300}"
# Seconds a lock must go on naming nobody alive before a waiter reports it stale.
# A holder that exits normally takes its lock with it well inside this, so the
# wait is what tells a released lock from an abandoned one.
: "${INSTALL_LOCK_STALE:=10}"

# $1's pid into $INSTALL_LOCK_PID, empty when the file is absent or still being
# written: a $(cat) here would fork on every poll tick (#795). The 2> comes first,
# or the failing input redirection still reports a missing file.
install_lock_read_pid() { # install_lock_read_pid LOCKFILE
    INSTALL_LOCK_PID=
    read -r INSTALL_LOCK_PID 2>/dev/null <"$1" || true
}

# Create the lock naming $1 as its owner, or fail because it exists. noclobber is
# O_EXCL, so the file and the identity of who holds it arrive in one step: a lock
# claimed first and stamped afterwards can be stamped by the wrong process.
install_lock_stamp() { # install_lock_stamp PID
    local had_c='' rc=0
    case $- in *C*) had_c=1 ;; esac
    set -C
    printf '%s\n' "$1" 2>/dev/null >"${INSTALL_LOCK}" || rc=1
    test -n "${had_c}" || set +C
    return "${rc}"
}

install_lock_release() {
    install_lock_read_pid "${INSTALL_LOCK}"
    # cleanup_push replays this at exit and must not drop a lock some other test
    # has since taken.
    test "${INSTALL_LOCK_PID}" = "$$" || return 0
    rm -f "${INSTALL_LOCK}"
}

# Non-zero, with a reason, rather than any waiter deciding whose lock it is to
# delete: only the process named inside one removes it, and that process is alive
# for as long as it runs. An abandoned lock therefore survives, and is reported to
# be cleared by hand -- reclaiming it automatically was three rounds of putting
# two installs in the tree at once, because "remove the thing I judged" is not
# something rm and mv can express: both name a path, not the file that was read.
install_lock_acquire() {
    local held_by=- since=$SECONDS
    until install_lock_stamp "$$"; do
        install_lock_read_pid "${INSTALL_LOCK}"
        if test "${INSTALL_LOCK_PID}" != "${held_by}"; then
            held_by=${INSTALL_LOCK_PID}
            since=$SECONDS
        fi
        if test "$((SECONDS - since))" -ge "${INSTALL_LOCK_STALE}" &&
            { test -z "${held_by}" || ! kill -0 "${held_by}" 2>/dev/null; }; then
            echo "install lock: ${INSTALL_LOCK} names ${held_by:-nobody}, which is gone;" \
                "a killed holder left it behind, so remove it to run staged installs again" >&2
            return 1
        fi
        if test "$((SECONDS - since))" -ge "${INSTALL_LOCK_TIMEOUT}"; then
            echo "install lock: ${INSTALL_LOCK} held by ${held_by:-nobody}" \
                "for ${INSTALL_LOCK_TIMEOUT}s" >&2
            return 1
        fi
        poll_wait 1
    done
    cleanup_push install_lock_release
}

# Run one command against the shared build tree, alone.
run_with_install_lock() { # run_with_install_lock CMD ARG...
    local rc=0
    install_lock_acquire || return 1
    "$@" || rc=$?
    install_lock_release
    return "${rc}"
}

# Run install target $1 into DESTDIR $2, logging to $3, from build directory $4
# below the top (src/ by default). The configured prefix survives DESTDIR, so the
# staged tree is the real layout; MAKEFLAGS and MAKELEVEL cleared, no jobserver to
# hunt.
# Trailing VAR=value arguments reach make, for a caller staging a layout its own
# build was not configured for.
stage_install_target() {
    local target=$1 dest=$2 log=$3 dir=${4:-src}
    shift $(($# < 4 ? $# : 4))
    run_with_install_lock env -u MAKEFLAGS -u MAKELEVEL "${MAKE:-make}" \
        -C "${abs_top_builddir:?}/${dir}" \
        "${target}" DESTDIR="${dest}" "$@" >"${log}" 2>&1 && return 0
    dump_file "${log}"
    return 1
}

# The binaries and the library alone, for a caller reading what was linked.
stage_install_exec() {
    stage_install_target install-exec "$1" "$2"
}

# The headers DevIncludes_DATA declares, kept relative to src/ as it writes them,
# so "../config.h" still names the build product it is.
declared_headers() {
    awk '/^DevIncludes_DATA[[:space:]]*=/ { inlist = 1 }
        inlist {
            last = ($0 !~ /\\$/)
            sub(/^[^=]*=/, "")
            gsub(/\\/, " ")
            for (i = 1; i <= NF; i++)
                if ($i ~ /\.h$/) print $i
            if (last) exit
        }' "${abs_top_srcdir:?}/src/Makefile.am"
}

# How many, so a caller can tell a shrunken install from a complete one and
# refuse to sweep a set that lost members.
declared_header_count() {
    declared_headers | awk 'END { print NR + 0 }'
}

# The same condition htsbacktrace.c compiles USES_SYMBOLIZER on. A build without
# <spawn.h> (bionic before API 28) prints frames as module+offset and stops.
build_names_frames() {
    local conf="${abs_top_builddir:?}/config.h"
    test -r "${conf}" || fail "no config.h at ${conf}"
    grep -q '^#define HAVE_BACKTRACE ' "${conf}" &&
        grep -q '^#define HAVE_SPAWN_H ' "${conf}"
}

# Which harness drives the tests: `msys` for an MSYS/Git Bash shell, `wsl2` for
# a Linux shell reaching the same native .exe over interop, `posix` otherwise.
# Only msys is sniffable, because a wsl2 shell answers `Linux` exactly like a
# native one, so the driver sets the variable and an unset one means posix.
suite_backend() {
    test -z "${HTTRACK_SUITE_BACKEND:-}" || {
        printf '%s\n' "$HTTRACK_SUITE_BACKEND"
        return 0
    }
    # IS_WINDOWS is the override this file has always honoured, and before the
    # split it answered both questions at once, so it names a backend and not
    # just the binary's platform. Resolved on every call rather than cached,
    # because a test sets it after sourcing us (249_windows-reap-images.test).
    case "${IS_WINDOWS:-}" in
    yes) printf 'msys\n' ;;
    no) printf 'posix\n' ;;
    *)
        case "$HTS_OS" in
        MINGW* | MSYS* | CYGWIN*) printf 'msys\n' ;;
        *) printf 'posix\n' ;;
        esac
        ;;
    esac
}

# Is the program under test a native Windows executable? This is what almost
# every caller of the old is_windows() was asking, and it stays true under the
# wsl2 backend even though the shell there is Linux.
target_is_windows() {
    if test -z "${IS_WINDOWS:-}"; then
        case "$(suite_backend)" in
        msys | wsl2) IS_WINDOWS=yes ;;
        *) IS_WINDOWS=no ;;
        esac
        export IS_WINDOWS
    fi
    test "$IS_WINDOWS" = yes
}

# Is this shell MSYS/Git Bash? Ask only about the shell's own quirks, its broken
# job control above all. A question about the binary wants target_is_windows.
shell_is_msys() { test "$(suite_backend)" = msys; }

# The binary under test is a native Linux one. The shell being Linux is not
# enough, because the wsl2 backend drives a Windows exe from a Linux shell.
target_is_linux() { test "$HTS_OS" = Linux && ! target_is_windows; }

# Open the timer fd poll_wait reads from: a fifo held open read-write, so there is
# always a writer and a read blocks to its own timeout instead of seeing EOF. fd 9
# is the harness's from here on; no test may hold it open across a poll.
POLL_STATE=
poll_open() {
    local f
    POLL_STATE=forked
    test -z "${HTTRACK_POLL_SLEEP:-}" || return 0
    # Not under MSYS: its fifos are emulated, and the leg whose stability #795 is
    # about is no place to discover how its select() behaves. That tick is a whole
    # second anyway, so there is little to win.
    shell_is_msys && return 0
    # Unique: $$ is the same in every subshell, and the loser of a race on one name
    # opens a path that is gone.
    f=$(mktemp -u "${TMPDIR:-/tmp}/.httrack-poll.XXXXXX" 2>/dev/null) || return 0
    mkfifo "$f" 2>/dev/null || return 0
    # Braced: "exec 9<>x 2>/dev/null" alone would send this shell's stderr to
    # /dev/null for good, exec applying its redirections to the shell itself.
    # Still a fifo, or bash's <> created a regular file on a vanished path, which
    # reads EOF at once and spins every deadline loop.
    if { exec 9<>"$f"; } 2>/dev/null && test -p "$f"; then
        POLL_STATE=fifo
    else
        exec 9<&-
    fi
    rm -f "$f" 2>/dev/null
    return 0
}

# One poll tick of at most $1 seconds, forking nothing where bash can time out a
# read on that fd (a `sleep` per tick was 22% of every process the suite created,
# and costs milliseconds apiece under MSYS, #795). $1 is a ceiling: the fd tick
# returns every 0.1s whatever it is asked, and callers re-check their own deadline.
# HTTRACK_POLL_SLEEP=1 forces the forked tick back, re-read per tick so a test can
# starve the poll mid-run.
poll_wait() {
    local secs=$1 rc=0
    test -n "$POLL_STATE" || poll_open
    if test "$POLL_STATE" = fifo && test -z "${HTTRACK_POLL_SLEEP:-}"; then
        # bash 4.0 is where -t took a fraction; below it, hand a caller asking for
        # one the sleep it asked for rather than rounding its tick up to a second.
        if test "${BASH_VERSINFO[0]}" -ge 4; then
            secs=0.1
        else
            case "$secs" in *.*)
                sleep "$secs"
                return 0
                ;;
            esac
        fi
        read -t "$secs" -r -u 9 _ || rc=$?
        test "$rc" -le 128 || return 0 # >128 is the timeout we asked for
        # bash 4.0 is also where a timeout started reporting >128; 3.2 says 1, which is
        # what a closed fd says everywhere. Ask the fd itself rather than the status,
        # or macOS retires its timer on the first tick and forks for the whole run.
        # `true`, not `:`: a redirection error on a POSIX special builtin exits the
        # shell, which POSIXLY_CORRECT in the environment is enough to turn on.
        if { true >&9; } 2>/dev/null; then return 0; fi
        POLL_STATE=forked # not a timer any more, stop trusting it
        exec 9<&-
    fi
    sleep "$1"
}

# On Windows MSYS can't signal a native python.exe, so kill_tree ends the whole
# tree (a bare kill -9 leaves children). Bounded because this runs from an EXIT
# trap, where a survivor would turn a passing test into a harness timeout.
stop_server() {
    test -n "${1:-}" || return 0
    local winpid winimage
    # Before the signal: a winpid read after it can already name a stranger.
    win_capture "$1"
    winpid=$WIN_PID winimage=$WIN_IMAGE
    kill "$1" 2>/dev/null || true
    if target_is_windows; then kill_tree "$1" "$winpid" "$winimage"; fi
    reap_bounded "$1" || true
    return 0
}

# Free loopback ports, one per protocol in $@ (tcp/udp, default a single tcp),
# space separated and all distinct, each closed before it returns: the number is
# only a hint, so a caller must retry its bind (start_proxytrack) or take
# hold_port instead when the port has to still be free later (#1218).
# Probed in the protocol the caller binds, since free in tcp says nothing about
# udp and Windows excludes port ranges per protocol (#1178).
freeport() { # freeport [PROTO...]
    local out
    # Captured, not piped into tr, which would swallow a failed probe's status.
    out=$("${python:?freeport needs the caller python}" -c 'import socket, sys
kinds = {"tcp": socket.SOCK_STREAM, "udp": socket.SOCK_DGRAM}
held, ports = [], []
for proto in sys.argv[1:] or ["tcp"]:
    if proto not in kinds:
        sys.exit("freeport: unknown protocol " + proto)
    for _ in range(10):
        s = socket.socket(socket.AF_INET, kinds[proto])
        s.bind(("127.0.0.1", 0))
        held.append(s)
        port = s.getsockname()[1]
        # tcp and udp draw from separate spaces, so the same number can come
        # back twice: hold that one too and draw again.
        if port not in ports:
            break
    else:
        sys.exit("freeport: no distinct " + proto + " port")
    ports.append(port)
print(" ".join(str(p) for p in ports))
for s in held:
    s.close()' "$@") || return 1
    # tr: Windows python's print leaves a CR that $() does not strip.
    printf '%s\n' "$out" | tr -d '\r'
}

# Holds a free loopback port instead of sampling it: the socket stays bound and
# unlistened, so the number is nobody else's while a connect to it is refused as
# on a dead port. Sets HELD_PORT and HELD_PID, and registers the release; call
# stop_server "$HELD_PID" to free the port sooner.
# shellcheck disable=SC2120 # tcp is the common case
hold_port() { # hold_port [PROTO]
    local proto=${1:-tcp} log
    # In a subshell the release is registered where it dies, pinning the port 900s.
    test "${BASHPID:-$$}" = "$$" ||
        fail "hold_port ran in a subshell: call it in the current shell, not as \$(hold_port), and read HELD_PORT"
    log=$(mktemp "${TMPDIR:-/tmp}/httrack_hold.XXXXXX") || fail "hold_port: no temp log"
    # Stdin off the tty: a background job that touches it is stopped by SIGTTIN.
    "${python:?hold_port needs the caller python}" -c 'import socket, sys, time
kinds = {"tcp": socket.SOCK_STREAM, "udp": socket.SOCK_DGRAM}
if sys.argv[1] not in kinds:
    sys.exit("hold_port: unknown protocol " + sys.argv[1])
s = socket.socket(socket.AF_INET, kinds[sys.argv[1]])
s.bind(("127.0.0.1", 0))
sys.stdout.reconfigure(newline="\n")  # no CR for discover_server_port to read
print("PORT %d" % s.getsockname()[1], flush=True)
time.sleep(900)  # bounded, so a SIGKILLed test cannot pin the port for the run
' "$proto" >"$log" 2>&1 </dev/null &
    HELD_PID=$!
    cleanup_push rm -f "$log"
    cleanup_push stop_server "$HELD_PID"
    HELD_PORT=$(discover_server_port "$log" "$HELD_PID") ||
        fail "the $proto port holder did not come up: $(<"$log")"
}

PT_LISTENING="HTTP Proxy installed on"
# Both proxytrack and htsserver announce a lost bind with this, out of the engine.
BIND_LOST="Unable to (initialize a temporary server|create the server)"

# Returns 0 once $1 (its log) carries the listen banner, 1 if the bind lost its
# port; a proxytrack that announces neither ends the test here.
proxytrack_bound() { # proxytrack_bound LOG PID
    local log=$1 pid=$2 waited=0
    until grep -qE "$PT_LISTENING|$BIND_LOST" "$log"; do
        # An exit flushes the log, so re-read it rather than calling this dead.
        kill -0 "$pid" 2>/dev/null || break
        test "$waited" -lt 50 || fail "proxytrack never announced its listen port: $(<"$log")"
        sleep 0.1
        waited=$((waited + 1))
    done
    grep -q "$PT_LISTENING" "$log" && return 0
    grep -qE "$BIND_LOST" "$log" || fail "proxytrack exited before listening: $(<"$log")"
    return 1
}

# proxytrack binds the ports itself and can find one stolen in between, so retry
# rather than red the suite. LAUNCH reads $proxyport/$icpport, writes $ptlog and
# leaves the pid in $ptpid; on return $ptlog is the surviving attempt's log.
start_proxytrack() { # start_proxytrack LOGBASE LAUNCH
    local base=$1 launch=$2 try
    for try in 1 2 3; do
        read -r proxyport icpport <<<"$(freeport tcp udp)"
        # One log per attempt: a killed one's pty drainer creates its .done
        # marker by path, and would answer for the attempt below.
        ptlog="$base.$try"
        : >"$ptlog"
        ptpid=
        "$launch"
        test -n "$ptpid" || fail "$launch left no proxytrack pid in \$ptpid"
        proxytrack_bound "$ptlog" "$ptpid" && return 0
        stop_server "$ptpid"
        ptpid=
        # Loud, so an intermittent bind regression cannot hide behind the retry.
        echo "proxytrack did not get port $proxyport, retrying" >&2
    done
    fail "proxytrack bound none of 3 port pairs: $(<"$ptlog")"
}

# Echo the port local-server.py announces on $1 (its log), $2 being its pid, or
# return 2 if the server died and 1 on the deadline: only the deadline is a race a
# caller may skip on. Matches anywhere, since a warning merged via 2>&1 can precede
# the line. A full minute of wall clock: a cold Python start under a parallel
# `make check -jN` lags well past a second, 5s had macos-15 missing it on 15 tests,
# and the count-of-ticks loop this replaced self-extended under load instead.
discover_server_port() {
    local log=$1 pid=$2 line start=$SECONDS
    while :; do
        if test -r "$log"; then
            # Read in the shell: a grep per tick is a process per tick (#795).
            while read -r line; do
                case "$line" in 'PORT '*)
                    printf '%s\n' "${line#PORT }"
                    return 0
                    ;;
                esac
            done <"$log"
        fi
        kill -0 "$pid" 2>/dev/null || {
            echo "server exited early: $(cat "$log" 2>/dev/null)" >&2
            return 2
        }
        test "$((SECONDS - start))" -lt 60 || break
        poll_wait 0.1
    done
    echo "could not discover server port: $(cat "$log" 2>/dev/null)" >&2
    return 1
}

# Start ftp-server.py: sets FTP_PORT, FTP_PID and FTP_LOG. --root resolves for
# the host, other arguments pass through, stdin off the terminal as in
# local_server_start.
ftp_server_start() { # ftp_server_start [--root DIR] [SERVER-ARGS...]
    local root='' args=()
    while test $# -gt 0; do
        case $1 in
        --root)
            root=$2
            shift 2
            ;;
        *)
            args+=("$1")
            shift
            ;;
        esac
    done
    test -n "${FTP_PYTHON:-}" || FTP_PYTHON=$(find_python) || skip "python3 not found"
    # Numbered: a second server here would truncate the first's log and the port
    # it reported. No --log option, the name colliding with ftp-server.py's own.
    FTP_N=$((${FTP_N:-0} + 1))
    FTP_LOG="${tmpdir:?no tmpdir set before ftp_server_start}/ftp-server.${FTP_N}.out"
    : >"$FTP_LOG"
    "$FTP_PYTHON" "$(nativepath "${testdir}/ftp-server.py")" \
        ${root:+--root "$(nativepath "$root")"} ${args[@]+"${args[@]}"} \
        >"$FTP_LOG" 2>&1 </dev/null &
    FTP_PID=$!
    cleanup_push stop_server "$FTP_PID"
    FTP_PORT=$(discover_server_port "$FTP_LOG" "$FTP_PID") ||
        fail "ftp-server did not come up: $(<"$FTP_LOG")"
}

# Dump and clear the crawl logs a hard-killed test leaves in TMPDIR (its cleanup
# trap never ran): hts-log.txt alone records "More than N seconds passed.. giving
# up", so a wedge past --max-time is undiagnosable without it (#605).
dump_crawl_logs() {
    local d f
    for d in "${TMPDIR:-/tmp}"/httrack_local.*; do
        test -d "$d" || continue
        for f in "$d/crawl/hts-log.txt" "$d/log" "$d/log.2"; do
            test -f "$f" || continue
            # Leading newline: the killed test's last line has no terminator.
            printf '\n--- %s (last 200 lines)\n' "$f"
            tail -n 200 "$f"
        done
        # so a later test's dump cannot re-report this one; never fatal, the
        # caller is already handling a failure and Windows may still hold a file
        rm -rf "$d" || true
    done
}

# The substring identifying ONE launch among every Windows process on the host.
# WSL2 has no /proc/<pid>/winpid, so all that is left is what the relay was
# started with. Skip argv[0], the same exe for every concurrent test, and skip
# the flags: local-crawl.sh passes a --user-agent every concurrent crawl shares,
# and it is LONGER than the output directory. What is left is per-launch,
# because the output directory and the URL's port are both per-test. Empty when
# nothing is distinctive enough, which the callers treat as "unknown" (#1228).
win_marker() { # win_marker <path to a NUL-separated cmdline>
    test -r "$1" || return 0
    tr '\0' '\n' <"$1" 2>/dev/null |
        awk 'NR == 1 { next }
             /^-/ { next }
             length($0) >= 8 && length($0) > length(best) { best = $0 }
             END { if (best != "") print best }'
}

# The Windows PID of the process carrying $1 on its command line, empty unless
# exactly one matches: naming a stranger is as good as naming nobody (#1228).
# The marker travels in the environment, not in the argument list, so this query
# cannot match itself the way the same marker on its own command line would.
win_pid_by_marker() { # win_pid_by_marker <marker>
    test -n "$1" || return 0
    # shellcheck disable=SC2016 # the $ below are PowerShell's, not the shell's
    HTS_WIN_MARKER=$1 WSLENV=HTS_WIN_MARKER powershell.exe -NoProfile \
        -NonInteractive -Command '
        $m = $env:HTS_WIN_MARKER
        $p = @(Get-CimInstance Win32_Process | Where-Object {
            $_.ProcessId -ne $PID -and $_.CommandLine -and
            $_.CommandLine.Contains($m) })
        if ($p.Count -eq 1) { $p[0].ProcessId }' 2>/dev/null |
        tr -cd '0-9'
}

# The Windows PID behind an MSYS pid, empty when unknown.
win_pid() {
    if test "$(suite_backend)" = wsl2; then
        win_pid_by_marker "$(win_marker "/proc/$1/cmdline")"
        return 0
    fi
    if test -r "/proc/$1/winpid"; then
        cat "/proc/$1/winpid" 2>/dev/null || true
    fi
}

# WIN_PID and WIN_IMAGE for MSYS pid $1, read while it is alive: /proc keeps the
# entry once the process is gone and Windows reissues the number at once, so a
# later read can name a stranger (#1228). Not for a job just backgrounded: until
# its exec lands, tens of milliseconds later, both still name the forking shell.
# Assigned rather than echoed, a command substitution being a fork (#795).
win_capture() { # win_capture <pid>
    WIN_PID='' WIN_IMAGE=''
    target_is_windows || return 0
    if test "$(suite_backend)" = wsl2; then
        # The relay's own cmdline, which is the Windows process's: read while it
        # is alive for the same reason winpid is, since a dead relay has none.
        local marker
        marker=$(win_marker "/proc/$1/cmdline")
        WIN_PID=$(win_pid_by_marker "$marker")
        { read -r -d '' WIN_IMAGE <"/proc/$1/cmdline"; } 2>/dev/null || true
        WIN_IMAGE=${WIN_IMAGE##*[\\/]}
        return 0
    fi
    # Unguarded reads: a missing file leaves the empty value set above, and read
    # reports EOF on an unterminated line having already assigned it.
    { read -r WIN_PID <"/proc/$1/winpid"; } 2>/dev/null || true
    { read -r WIN_IMAGE <"/proc/$1/winexename"; } 2>/dev/null || true
    WIN_IMAGE=${WIN_IMAGE##*[\\/]}
    return 0
}

# Whether Windows PID $1 runs image $2. Both columns at once, since either alone
# answers for a recycled PID, and case-folded as the proclib.sh matchers are.
win_pid_runs() { # win_pid_runs <winpid> <image>
    tasklist 2>/dev/null |
        awk -v p="$1" -v i="$2" 'tolower($1) == tolower(i) && $2 == p { f = 1 } END { exit !f }'
}

# Signal one process, never its descendants: a caller inside the target's own
# tree cannot rely on kill_tree, whose taskkill is then a grandchild of it (#953).
kill_pid() {
    local pid=$1
    if target_is_windows; then
        local winpid
        winpid=$(win_pid "$pid")
        if test -n "$winpid"; then
            taskkill /F /PID "$winpid" >/dev/null 2>&1 || true
        fi
        return 0
    fi
    kill -9 "$pid" 2>/dev/null || true
}

# Kill a backgrounded job and its whole descendant tree. POSIX: the caller must
# have put the job in its own process group (run_with_timeout does) so we signal
# the group; a bare kill would orphan the grandchildren. Windows: the tree is
# native processes MSYS can't signal, so taskkill /T ends it by Windows PID.
# Single-slash switches: the workflow sets MSYS_NO_PATHCONV/MSYS2_ARG_CONV_EXCL,
# so args pass verbatim and a //T would reach taskkill unfolded and be rejected.
# $2 is that Windows PID when the caller read it while the job was certainly
# alive: /proc/<pid>/winpid is already gone for a job that has just died, and
# without it the only route left is the host-wide sweep below. $3 is the image it
# ran then: a number that no longer runs it was reissued while we were not
# looking, and naming a stranger is as good as naming nobody (#1228).
kill_tree() {
    local pid=$1 winpid=${2:-} image=${3:-}
    if target_is_windows; then
        test -n "$winpid" || winpid=$(win_pid "$pid")
        if test -n "$winpid" && test -n "$image" && ! win_pid_runs "$winpid" "$image"; then
            printf '::warning::pid %s no longer runs %s, not killing it\n' "$winpid" "$image"
            winpid=
        fi
        if test -n "$winpid"; then
            taskkill /F /T /PID "$winpid" >/dev/null 2>&1 || true
        # Last resort, so it is opt-in: it kills every engine and every python on
        # the host, siblings of a parallel run included (HTTRACK_EXCLUSIVE_HOST).
        elif test -n "${HTTRACK_EXCLUSIVE_HOST:-}"; then
            taskkill_engines
            taskkill /F /IM python.exe >/dev/null 2>&1 || true
        fi
        # Not a fallback under wsl2 but the other half of the job: the shell
        # there is Linux, so $pid is often a process that never had a Windows
        # counterpart for taskkill to find, a backgrounded test bash above all.
        if test "$(suite_backend)" = wsl2; then
            kill -9 -"$pid" 2>/dev/null || kill -9 "$pid" 2>/dev/null || true
        fi
        return 0
    fi
    # Under msys no caller puts $pid in its own group (set -m is skipped there),
    # so -"$pid" would target whatever real group that number collides with,
    # possibly the harness's own, and the taskkill above already reaped it.
    kill -9 -"$pid" 2>/dev/null || kill -9 "$pid" 2>/dev/null || true
}

# The engine executables, one list so the kills here and the matchers proclib.sh
# derives from it cannot drift apart again (#1067).
ENGINE_EXES='httrack proxytrack htsserver webhttrack'

# taskkill every engine image on the host. Windows only, and by name, so the
# caller must be sure no wanted process shares one.
taskkill_engines() {
    local e
    for e in $ENGINE_EXES; do
        taskkill /F /IM "$e.exe" >/dev/null 2>&1 || true
    done
}

# Install a `sleep` in dir $1 costing $2 times what it asks, simulating the CPU
# starvation that stretches a poll iteration. Callers must subshell it (it edits
# PATH); sub-second requests round up to 1s, so a 0.1s poll stretches too.
starve_sleep() {
    local dir=$1 factor=$2 real
    real=$(command -v sleep) || return 1
    mkdir -p "$dir" || return 1
    cat >"$dir/sleep" <<EOF
#!/bin/sh
n=\${1%%.*}
test "\$n" -gt 0 2>/dev/null || n=1
exec "$real" "\$((n * $factor))"
EOF
    chmod +x "$dir/sleep" || return 1
    PATH="$dir:$PATH"
    export PATH
    # Proved, not assumed: MSYS hands out a drive-letter TMPDIR and a PATH entry
    # carrying a colon is read as two, so an unreachable shim starves nothing.
    test "$(command -v sleep)" = "$dir/sleep" || return 1
}

# Skip when the next of $1 remaining steps, at 1.5x the $2 seconds the last one
# took, no longer fits the budget meant to catch a wedge (hppa spends ~150s on one
# configure run and would else FTBFS). One step ahead rather than all of them:
# projecting a single sample over the whole tail skips a run that fits whenever
# one step is slower than its neighbours. It asks an ordering of the callers
# instead, expensive steps first, so no step left can outrun the reserve the one
# before it set (#1146).
# The budget test-timeout.sh enforces, in seconds, 0 being the guard off. The one
# parser: a value bash arithmetic or test would choke on falls back to the default,
# and a leading zero would otherwise read as octal in one place and decimal in the next.
budget_secs() {
    local budget=${HTTRACK_TEST_TIMEOUT:-600}
    case "$budget" in '' | *[!0-9]* | ???????*) budget=600 ;; esac
    echo "$((10#$budget))"
}

skip_if_out_of_budget() { # skip_if_out_of_budget <steps left> <seconds the last took>
    local budget need=$(($2 + $2 / 2))

    budget=$(budget_secs)
    test "$1" -gt 0 && test "$budget" -gt 0 || return 0
    test "$((SECONDS + need))" -ge "$budget" || return 0
    echo "$1 steps left, the last took ${2}s and the budget is ${budget}s; skipping" >&2
    exit 77
}

# Seconds left of the budget, for a child pacing itself against it (269 hands it to
# the sweep). Never below 1 unless the guard is off, when it stays 0.
budget_left() {
    local budget left
    budget=$(budget_secs)
    test "$budget" -gt 0 || {
        echo 0
        return 0
    }
    left=$((budget - SECONDS))
    test "$left" -ge 1 || left=1
    echo "$left"
}

# Collect a killed job, giving up after REAP_GRACE seconds. kill_tree can fail to
# reap a native Windows descendant -- the very case these watchdogs exist for --
# and a bare `wait` then blocks the watchdog itself forever, so the timeout it was
# about to report is never printed and the whole suite wedges silently.
REAP_GRACE=${REAP_GRACE:-10}
reap_bounded() {
    local pid=$1 start=$SECONDS
    while kill -0 "$pid" 2>/dev/null; do
        test "$((SECONDS - start))" -le "$REAP_GRACE" || return 1
        poll_wait 1
    done
    wait "$pid" 2>/dev/null || true
    return 0
}

# Run "$@" under a wall-clock deadline of $1 seconds; return its exit status, or
# 124 if it overran and was killed. timeout(1) is unusable here: it's absent on
# macOS and its signals can't reap httrack.exe on Windows. We poll and kill_tree.
# All three deadlines below compare strictly: $SECONDS is floored, so a reading of
# the budget can be a fraction under it, and firing early kills healthy work.
# One option, ahead of the deadline:
#   --stdin FILE   read FILE, or nothing at all with the word "closed". A
#                  redirect on the caller does not reach here: where job control
#                  is off, bash gives a background job /dev/null (#1258). The
#                  redirect stays on the job, so the target is still our direct
#                  child and kill_tree keeps signalling it rather than a wrapper.
run_with_timeout() {
    local stdin=''
    if test "${1:-}" = --stdin; then
        stdin=$2
        shift 2
    fi
    local secs=$1
    shift
    local had_m=
    case "$-" in *m*) had_m=1 ;; esac
    shell_is_msys || set -m # own process group, so kill_tree can signal the group
    case $stdin in
    '') "$@" & ;;
    closed) "$@" <&- & ;;
    *) "$@" <"$stdin" & ;;
    esac
    local pid=$!
    test -n "$had_m" || shell_is_msys || set +m
    # Read while the job is certainly alive: by kill time /proc/<pid>/winpid is gone.
    local winpid=''
    ! target_is_windows || winpid=$(win_pid "$pid")
    local start=$SECONDS
    while kill -0 "$pid" 2>/dev/null; do
        if test "$((SECONDS - start))" -gt "$secs"; then
            kill_tree "$pid" "$winpid"
            reap_bounded "$pid" || true
            return 124
        fi
        poll_wait 1
    done
    wait "$pid"
}

# Bound an already-backgrounded crawl (pid $1) at $2s, reaping it and returning 124
# on overrun: a wedge past --max-time would else block wait() forever and hang the CI step.
wait_bounded() {
    local pid=$1 secs=$2 start=$SECONDS
    while kill -0 "$pid" 2>/dev/null; do
        if test "$((SECONDS - start))" -gt "$secs"; then
            kill_tree "$pid"
            reap_bounded "$pid" || true
            return 124
        fi
        poll_wait 1
    done
    wait "$pid"
}
