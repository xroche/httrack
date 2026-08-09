#!/bin/bash
#
# Helpers shared by the crawl tests. Sourced, not run.

# Python 3 interpreter, or empty: Windows only installs python.exe, and a bare
# "python" may be 2.x or the Store stub.
find_python() {
    local py
    for py in "${PYTHON:-}" python3 python; do
        test -n "$py" || continue
        "$py" -c 'import sys; sys.exit(sys.version_info[0] != 3)' 2>/dev/null || continue
        printf '%s\n' "$py"
        return 0
    done
    return 1
}

# Native form of a path: a non-MSYS binary cannot resolve Git Bash's /d/a/... ones.
nativepath() {
    if is_windows && command -v cygpath >/dev/null 2>&1; then
        cygpath -m "$1"
    else
        printf '%s\n' "$1"
    fi
}

# The shipped cert and key joined into $1/both.pem, the single path
# load_cert_chain() takes, key first as it documents.
make_tls_pem() {
    local dir=$1 src
    src=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
    cat "$src/server.key" "$src/server.crt" >"$dir/both.pem" || {
        echo "FAIL: cannot read the test cert fixture under $src" >&2
        exit 1
    }
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

# Run src/ install target $1 into DESTDIR $2, logging to $3. The configured prefix
# survives DESTDIR, so libtool will not relink the build the rest of the suite runs
# against; MAKEFLAGS and MAKELEVEL cleared, no jobserver to hunt.
stage_install_target() {
    local target=$1 dest=$2 log=$3
    env -u MAKEFLAGS -u MAKELEVEL "${MAKE:-make}" -C "${abs_top_builddir:?}/src" \
        "${target}" DESTDIR="${dest}" >"${log}" 2>&1 && return 0
    cat "${log}" >&2
    return 1
}

# The binaries and the library alone, for a caller reading what was linked.
stage_install_exec() {
    stage_install_target install-exec "$1" "$2"
}

IS_WINDOWS=
is_windows() {
    if test -z "$IS_WINDOWS"; then
        case "$(uname -s)" in
        MINGW* | MSYS* | CYGWIN*) IS_WINDOWS=yes ;;
        *) IS_WINDOWS=no ;;
        esac
    fi
    test "$IS_WINDOWS" = yes
}

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
    is_windows && return 0
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
    kill "$1" 2>/dev/null || true
    if is_windows; then kill_tree "$1"; fi
    reap_bounded "$1" || true
    return 0
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

# The Windows PID behind an MSYS pid, empty when unknown.
win_pid() {
    if test -r "/proc/$1/winpid"; then
        cat "/proc/$1/winpid" 2>/dev/null || true
    fi
}

# Signal one process, never its descendants: a caller inside the target's own
# tree cannot rely on kill_tree, whose taskkill is then a grandchild of it (#953).
kill_pid() {
    local pid=$1
    if is_windows; then
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
kill_tree() {
    local pid=$1
    if is_windows; then
        local winpid
        winpid=$(win_pid "$pid")
        if test -n "$winpid"; then
            taskkill /F /T /PID "$winpid" >/dev/null 2>&1 || true
        else
            # The offline suite runs serially, so no wanted process races this.
            taskkill_engines
            taskkill /F /IM python.exe >/dev/null 2>&1 || true
        fi
        return 0
    fi
    # No caller puts $pid in its own group on Windows (set -m is skipped there),
    # so -"$pid" here would target whatever real group $pid's number collides
    # with -- possibly the harness's own -- and taskkill above already reaped it.
    kill -9 -"$pid" 2>/dev/null || kill -9 "$pid" 2>/dev/null || true
}

# Engine and fixture-server processes, matched on the executable basename only.
# Matching the whole ps line instead would catch every unrelated command whose
# arguments merely mention a path containing "httrack". ENGINE_EXES feeds the
# matcher and every Windows taskkill below, which used to drift apart (#1067).
ENGINE_EXES='httrack proxytrack htsserver webhttrack'
ENGINE_EXE_RE="^(lt-)?(${ENGINE_EXES// /|})([.]exe)?\$"
# The same set against tasklist, whose first column is the image basename.
# Anchored, or "notepad-httrack-notes.exe" reads as a leaked engine.
ENGINE_IMAGE_RE="^(${ENGINE_EXES// /|})[.]exe"
FIXTURE_SERVER_RE='^(local-server|proxy-https-server|proxy-connect-server|socks5-server|tls-stall-server)[.]py$'

# taskkill every engine image on the host. Windows only, and by name, so the
# caller must be sure no wanted process shares one.
taskkill_engines() {
    local e
    for e in $ENGINE_EXES; do
        taskkill /F /IM "$e.exe" >/dev/null 2>&1 || true
    done
}

# awk prologue for the matchers below: under qemu-user the kernel reports the
# binfmt interpreter as the command, so the name we match on lands one column
# right (Debian's hppa buildd emulates).
# shellcheck disable=SC2016 # awk fields, not shell expansions
AWK_PROC_NAMES='
function basen(s) { sub(/.*[\/\\]/, "", s); return s }
function qemushift(  n) {
    n = basen($6)
    # qemu-img and friends take an image, not a program, and this list is fed to
    # kill: shifting past them would read a disk path as the process name.
    if (n ~ /^qemu-(img|nbd|io|ga|edid|keymap)$/) return 0
    return n ~ /^qemu-[[:alnum:]_]+(-static)?$|^[[:alnum:]_]+-binfmt(-[[:upper:]]+)?$/ ? 1 : 0
}'

# Every process as "PID PPID PGID ELAPSED S COMMAND", header first. A Fedora
# build root has no procps, and an empty list reads as "nothing running" (#1021).
ps_snapshot() {
    local snap
    # POSIX keywords, so the ps route holds on macOS too; a ps that lists nothing
    # (hidepid, a locked-down container) is no better than an absent one.
    if snap=$(ps -A -o pid,ppid,pgid,etime,state,args 2>/dev/null |
        awk 'NR > 1 { rows++ } { print } END { exit rows ? 0 : 1 }'); then
        printf '%s\n' "$snap"
        return 0
    fi
    proc_snapshot && return 0
    # Every consumer drops line 1, so the notice rides in the header's place.
    printf 'no process list: this host has neither ps nor a readable /proc\n'
    return 1
}

# The same six columns out of /proc, in bash alone; elapsed as plain seconds.
proc_snapshot() {
    local d stat rest arg args comm hz uptime
    local -a f
    local ws # spelled out of line: bash 3.2 fails to parse $'..' inside ${v//p/r}
    ws=$' \t\n\v\f\r'
    test -r /proc/self/stat || return 1
    hz=$(getconf CLK_TCK 2>/dev/null) || hz=
    # A zero or non-numeric tick would abort the shell in the division below.
    case "$hz" in '' | 0 | *[!0-9]*) hz=100 ;; esac
    read -r uptime _ </proc/uptime 2>/dev/null || return 1
    printf 'PID PPID PGID ELAPSED S COMMAND\n'
    for d in /proc/[0-9]*; do
        # Braced: a failed open reports to the caller's stderr, not the redirect.
        { read -r stat <"$d/stat"; } 2>/dev/null || continue
        # comm may hold spaces and parens; every field past it is numeric.
        rest=${stat##*') '}
        comm=${stat#*(}
        comm=${comm%)*}
        read -ra f <<<"$rest" || continue
        test "${#f[@]}" -ge 20 || continue # short read: the process is going away
        args=
        # Whitespace inside an argv would split the row or shift the columns the
        # consumers match on, which ps avoids by mapping those bytes away.
        { while IFS= read -r -d '' arg; do
            args="${args:+$args }${arg//["$ws"]/ }"
        done <"$d/cmdline"; } 2>/dev/null || true
        printf '%s %s %s %s %s %s\n' "${d#/proc/}" "${f[1]}" "${f[2]}" \
            "$(((${uptime%%.*} * hz - f[19]) / hz))" \
            "${f[0]}" "${args:-[$comm]}"
    done
}

# List processes a hung test may have left running, one per line. $1 is the test's
# process group; $2 selects "group" (that group's members, whatever their name),
# "others" (engine and fixture processes outside it, which under "make check -j"
# belong to healthy siblings) or "named" (every engine and fixture process on the
# host). Read-only: it never signals anything.
list_stray_processes() {
    local pgid=${1:-0} mode=${2:-group}
    if is_windows; then
        # No process groups here, so every mode gives the same host-wide list. No
        # slash switches: without MSYS_NO_PATHCONV a /fi would be rewritten to a
        # path. Plain output is Image Name + PID, which is all we need.
        test "$mode" != others || return 0
        tasklist 2>/dev/null | grep -Ei "$ENGINE_IMAGE_RE|^python" || true
    else
        # Fields 6 and 7 are the command and its first argument (the interpreter
        # and its script, for the Python fixtures).
        ps_snapshot |
            awk -v pg="$pgid" -v mode="$mode" -v eng="$ENGINE_EXE_RE" -v srv="$FIXTURE_SERVER_RE" \
                "$AWK_PROC_NAMES"'
                NR == 1 { print; next }
                { ingroup = (pg > 0 && $3 == pg)
                  q = qemushift()
                  c = basen($(6 + q))
                  s = basen($(7 + q))
                  named = (c ~ eng || s ~ srv)
                  if (mode == "group" ? ingroup : \
                      mode == "named" ? named : (named && !ingroup)) print }' || true
    fi
}

# Kill engine processes a finished test left behind, and print what was found so
# the leak is attributed to the test that just ran: an orphaned httrack.exe spins
# and starves the runner, which is how the Windows job dies of "lost
# communication" rather than a clean timeout. SERIAL RUNNERS ONLY -- it matches by
# name host-wide, so under a parallel "make check" it would kill a healthy
# sibling's engine. Only the engine images: a runner may run python.exe of its
# own, and tasklist alone cannot tell that one from a leaked fixture server.
reap_leftover_processes() {
    local label=${1:-} left
    if is_windows; then
        left=$(tasklist 2>/dev/null | grep -Ei "$ENGINE_IMAGE_RE" || true)
    else
        left=$(list_stray_processes 0 named | awk 'NR > 1')
    fi
    test -n "$left" || return 0
    printf '::warning::%s left processes behind\n' "$label"
    printf '%s\n' "$left"
    if is_windows; then
        taskkill_engines
    else
        printf '%s\n' "$left" | awk '{ print $1 }' |
            while read -r p; do kill -9 "$p" 2>/dev/null || true; done
    fi
    return 0
}

# Pids of engine processes in process group $1. Scoped to the group because the
# caller signals them, and under "make check -j" a global match would abort a
# healthy sibling test's engine. Matches the executable basename only, so a
# harness script whose *path* contains "httrack" is not mistaken for the engine.
list_engine_pids() {
    local pgid=${1:-0}
    test "$pgid" -gt 0 2>/dev/null || return 0
    ps_snapshot |
        awk -v pg="$pgid" -v eng="$ENGINE_EXE_RE" "$AWK_PROC_NAMES"'
            NR > 1 && $3 == pg { if (basen($(6 + qemushift())) ~ eng) print $1 }'
}

# Ask the wedged test's engine processes for a stack. What is obtainable differs
# per platform, and each branch says which one it took: a dump that silently
# produces nothing reads as coverage when it is not.
#
#   Linux  httrack's own SIGABRT handler (sig_fatal in httrack.c) symbolizes via
#          addr2line and writes to the process's OWN stderr, so the trace lands in
#          whatever log the test gave it -- the caller must salvage those logs.
#          It walks only the signalled thread, and it aborts the process.
#   macOS  htsbacktrace.c gates that handler on __linux, so SIGABRT would yield
#          "No stack trace available on this OS". sample(1) is OS-provided, needs
#          no root, covers every thread and leaves the process running.
#
# gdb -p is not an option on either: it is EPERM from a sibling under yama
# ptrace_scope=1, which is how a harness watchdog necessarily invokes it.
request_engine_backtraces() {
    local p os
    local sent='' abrt=''
    os=$(uname -s 2>/dev/null || echo unknown)
    for p in $(list_engine_pids "$1"); do
        sent=1
        test ! -r "/proc/$p/wchan" ||
            printf 'pid %s blocked in: %s\n' "$p" "$(cat "/proc/$p/wchan")"
        case "$os" in
        Linux)
            kill -ABRT "$p" 2>/dev/null && abrt=1
            ;;
        Darwin)
            if test -x /usr/bin/sample; then
                # Drop the trailing image map: ~40 lines of load addresses that
                # say nothing about the hang.
                /usr/bin/sample "$p" 2 -mayDie -file /dev/stdout 2>&1 |
                    sed '/^Binary Images:/,$d' ||
                    printf 'pid %s: sample(1) failed\n' "$p"
            else
                printf 'pid %s: no stack, /usr/bin/sample is absent\n' "$p"
            fi
            ;;
        *)
            printf 'pid %s: no stack mechanism known for %s\n' "$p" "$os"
            ;;
        esac
    done
    test -n "$sent" || printf 'no engine process left to ask (see the list above)\n'
    test -z "$abrt" || sleep 3 # let the handlers symbolize and print
}

# Stack of every native engine process, through cdb. Windows has neither half of
# the POSIX route: htsbacktrace.c is gated on __linux, and MSYS signals never
# reach a native httrack.exe. cdb ships with the SDK on the runner image but that
# is incidental, so probe for it and say so when it is missing. The MSVC build
# writes PDBs beside the binaries, which the test step already puts on PATH, so
# frames resolve to names. Serial runners only: it stacks every engine process it
# finds, having no process group to scope by.
dump_windows_stacks() {
    local c p
    local cdb='' found=''
    for c in "$(command -v cdb 2>/dev/null)" \
        "/c/Program Files (x86)/Windows Kits/10/Debuggers/x64/cdb.exe" \
        "/c/Program Files (x86)/Windows Kits/10/Debuggers/x86/cdb.exe"; do
        test -n "$c" || continue
        test -x "$c" || continue
        cdb=$c
        break
    done
    if test -z "$cdb"; then
        printf 'no stack: cdb.exe is not in the SDK Debuggers directories or on PATH\n'
        return 0
    fi
    for p in $(tasklist 2>/dev/null | grep -Ei "$ENGINE_IMAGE_RE" | awk '{print $2}'); do
        found=1
        printf -- '--- cdb stack of pid %s ---\n' "$p"
        # Bounded, so a debugger that wedges cannot become the new hang. "qd"
        # detaches and leaves the process for kill_tree.
        run_with_timeout 60 "$cdb" -p "$p" -c '~*kv; qd' 2>&1 ||
            printf 'cdb failed or timed out on pid %s\n' "$p"
    done
    test -n "$found" || printf 'no engine process left to ask (see the list above)\n'
}

# Report what a wedged test left behind, into the log the harness keeps: the test
# that blew its budget, the processes still running, and a stack for each engine
# process. $1 is the timed-out job's pid (its process group leader on POSIX).
# Never deletes, so it is safe under a parallel "make check".
dump_hang_diagnostics() {
    local pid=$1 label=${2:-?} secs=${3:-?}
    printf '\n===== TIMEOUT: %s exceeded its %ss budget =====\n' "$label" "$secs"
    if is_windows; then
        printf -- '--- still running ---\n'
        list_stray_processes 0 group
        printf -- '--- stacks (via cdb) ---\n'
        dump_windows_stacks
    else
        printf -- '--- the test'\''s own process tree (group %s) ---\n' "$pid"
        list_stray_processes "$pid" group
        # Under "make check -j" these belong to healthy siblings, so they are
        # reported but never signalled; a leaked orphan also lands here.
        printf -- '--- other engine processes on this host ---\n'
        list_stray_processes "$pid" others
        printf -- '--- stacks (via the engine SIGABRT handler) ---\n'
        request_engine_backtraces "$pid"
    fi
    printf -- '===== end of diagnostics: %s =====\n' "$label"
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
# configure run and would else FTBFS). One step ahead rather than all of them: 196
# shares a config.cache, so its first step costs several times the rest and
# projecting it over them would skip a run that fits.
skip_if_out_of_budget() { # skip_if_out_of_budget <steps left> <seconds the last took>
    local budget=${HTTRACK_TEST_TIMEOUT:-600} need=$(($2 + $2 / 2))

    case "$budget" in '' | *[!0-9]*) budget=600 ;; esac
    test "$1" -gt 0 && test "$budget" -gt 0 || return 0
    test "$((SECONDS + need))" -ge "$budget" || return 0
    echo "$1 steps left, the last took ${2}s and the budget is ${budget}s; skipping" >&2
    exit 77
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
run_with_timeout() {
    local secs=$1
    shift
    local had_m=
    case "$-" in *m*) had_m=1 ;; esac
    is_windows || set -m # own process group, so kill_tree can signal the group
    "$@" &
    local pid=$!
    test -n "$had_m" || is_windows || set +m
    local start=$SECONDS
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
