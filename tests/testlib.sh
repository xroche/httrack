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

# Longest surviving run of char $2 in file $1, or 0: the length a field was
# clipped to, read back out of a binary artifact.
runlen() {
    grep -ao "$2\\+" "$1" | awk '{ print length($0) }' | sort -rn | head -n1 || true
}

is_windows() {
    case "$(uname -s)" in
    MINGW* | MSYS* | CYGWIN*) return 0 ;;
    *) return 1 ;;
    esac
}

# On Windows MSYS can't signal a native python.exe, so kill_tree ends the whole
# tree (a bare kill -9 leaves children). "|| true" throughout: callers run under
# set -e and the reap makes wait return 143.
stop_server() {
    test -n "${1:-}" || return 0
    kill "$1" 2>/dev/null || true
    if is_windows; then kill_tree "$1"; fi
    wait "$1" 2>/dev/null || true
    return 0
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

# Kill a backgrounded job and its whole descendant tree. POSIX: the caller must
# have put the job in its own process group (run_with_timeout does) so we signal
# the group; a bare kill would orphan the grandchildren. Windows: the tree is
# native processes MSYS can't signal, so taskkill /T ends it by Windows PID.
# Single-slash switches: the workflow sets MSYS_NO_PATHCONV/MSYS2_ARG_CONV_EXCL,
# so args pass verbatim and a //T would reach taskkill unfolded and be rejected.
kill_tree() {
    local pid=$1
    if is_windows; then
        local winpid=
        test -r "/proc/$pid/winpid" && winpid=$(cat "/proc/$pid/winpid" 2>/dev/null)
        if test -n "$winpid"; then
            taskkill /F /T /PID "$winpid" >/dev/null 2>&1 || true
        else
            # The offline suite runs serially, so no wanted process races this.
            taskkill /F /IM httrack.exe >/dev/null 2>&1 || true
            taskkill /F /IM python.exe >/dev/null 2>&1 || true
        fi
    fi
    kill -9 -"$pid" 2>/dev/null || kill -9 "$pid" 2>/dev/null || true
}

# Engine and fixture-server processes, matched on the executable basename only.
# Matching the whole ps line instead would catch every unrelated command whose
# arguments merely mention a path containing "httrack".
ENGINE_EXE_RE='^(lt-)?(httrack|proxytrack|htsserver|webhttrack)([.]exe)?$'
FIXTURE_SERVER_RE='^(local-server|proxy-https-server|proxy-connect-server|socks5-server|tls-stall-server)[.]py$'

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
        tasklist 2>/dev/null | grep -Ei 'httrack|proxytrack|htsserver|python' || true
    else
        # `args` and `state` are POSIX ps keywords, so this holds on macOS too.
        # Fields 6 and 7 are the command and its first argument (the interpreter
        # and its script, for the Python fixtures).
        ps -A -o pid,ppid,pgid,etime,state,args 2>/dev/null |
            awk -v pg="$pgid" -v mode="$mode" -v eng="$ENGINE_EXE_RE" -v srv="$FIXTURE_SERVER_RE" '
                NR == 1 { print; next }
                { ingroup = (pg > 0 && $3 == pg)
                  c = $6; sub(/.*[\/\\]/, "", c)
                  s = $7; sub(/.*[\/\\]/, "", s)
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
        left=$(tasklist 2>/dev/null | grep -Ei 'httrack|proxytrack|htsserver' || true)
    else
        left=$(list_stray_processes 0 named | awk 'NR > 1')
    fi
    test -n "$left" || return 0
    printf '::warning::%s left processes behind\n' "$label"
    printf '%s\n' "$left"
    if is_windows; then
        taskkill /F /IM httrack.exe >/dev/null 2>&1 || true
        taskkill /F /IM proxytrack.exe >/dev/null 2>&1 || true
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
    ps -A -o pid,pgid,args 2>/dev/null |
        awk -v pg="$pgid" -v eng="$ENGINE_EXE_RE" \
            'NR > 1 && $2 == pg { c = $3; sub(/.*[\/\\]/, "", c); if (c ~ eng) print $1 }'
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
    for p in $(tasklist 2>/dev/null | grep -Ei '^(httrack|proxytrack)[.]exe' | awk '{print $2}'); do
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

# Collect a killed job, giving up after REAP_GRACE seconds. kill_tree can fail to
# reap a native Windows descendant -- the very case these watchdogs exist for --
# and a bare `wait` then blocks the watchdog itself forever, so the timeout it was
# about to report is never printed and the whole suite wedges silently.
REAP_GRACE=${REAP_GRACE:-10}
reap_bounded() {
    local pid=$1 waited=0
    while kill -0 "$pid" 2>/dev/null; do
        test "$waited" -lt "$REAP_GRACE" || return 1
        sleep 1
        waited=$((waited + 1))
    done
    wait "$pid" 2>/dev/null || true
    return 0
}

# Run "$@" under a wall-clock deadline of $1 seconds; return its exit status, or
# 124 if it overran and was killed. timeout(1) is unusable here: it's absent on
# macOS and its signals can't reap httrack.exe on Windows. We poll and kill_tree.
run_with_timeout() {
    local secs=$1
    shift
    local had_m=
    case "$-" in *m*) had_m=1 ;; esac
    is_windows || set -m # own process group, so kill_tree can signal the group
    "$@" &
    local pid=$!
    test -n "$had_m" || is_windows || set +m
    local waited=0
    while kill -0 "$pid" 2>/dev/null; do
        if test "$waited" -ge "$secs"; then
            kill_tree "$pid"
            reap_bounded "$pid" || true
            return 124
        fi
        sleep 1
        waited=$((waited + 1))
    done
    wait "$pid"
}

# Bound an already-backgrounded crawl (pid $1) at $2s, reaping it and returning 124
# on overrun: a wedge past --max-time would else block wait() forever and hang the CI step.
wait_bounded() {
    local pid=$1 secs=$2 waited=0
    while kill -0 "$pid" 2>/dev/null; do
        if test "$waited" -ge "$secs"; then
            kill_tree "$pid"
            reap_bounded "$pid" || true
            return 124
        fi
        sleep 1
        waited=$((waited + 1))
    done
    wait "$pid"
}
