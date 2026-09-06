#!/bin/bash
#
# Process forensics for a wedged test: what is still running, and a stack for
# each engine process. Sourced after testlib.sh, not run.

# Engine and fixture-server processes, matched on the executable basename only.
# Matching the whole ps line instead would catch every unrelated command whose
# arguments merely mention a path containing "httrack". Derived from testlib.sh's
# ENGINE_EXES, never spelled out again (#1067).
ENGINE_EXE_RE="^(lt-)?(${ENGINE_EXES// /|})([.]exe)?\$"
# The same set against tasklist, whose first column is the image basename.
# Anchored, or "notepad-httrack-notes.exe" reads as a leaked engine.
ENGINE_IMAGE_RE="^(${ENGINE_EXES// /|})[.]exe"
FIXTURE_SERVER_RE='^(local-server|proxy-https-server|proxy-connect-server|socks5-server|tls-stall-server)[.]py$'

# Under qemu-user the kernel reports the binfmt interpreter as the command, and
# the program's own argv[0] lands one place right (Debian's hppa buildd
# emulates). qemu-img and friends take a disk image, not a program; these lists
# feed kill, so shifting past one would read the image path as the process.
QEMU_INTERP_RE='^qemu-[[:alnum:]_]+(-static)?$|^[[:alnum:]_]+-binfmt(-[[:upper:]]+)?$'
QEMU_IMAGE_TOOL_RE='^qemu-(img|nbd|io|ga|edid|keymap)(-static)?$'

# awk prologue for the matchers below, taking those two as -v qint and -v qimg.
# shellcheck disable=SC2016 # awk fields, not shell expansions
AWK_PROC_NAMES='
function basen(s) { sub(/.*[\/\\]/, "", s); return s }
function qemushift(  n) {
    n = basen($6)
    if (n ~ qimg) return 0
    return n ~ qint ? 1 : 0
}'

# The name of the program pid $1 is running, empty if it is gone. Fed to the
# prologue above as a one-row listing, so the shift is decided in one place.
proc_program_name() { # proc_program_name <pid>
    local a args=
    { while IFS= read -r -d '' a; do args="${args:+$args }$a"; done <"/proc/$1/cmdline"; } 2>/dev/null
    test -n "$args" || return 1
    awk -v qint="$QEMU_INTERP_RE" -v qimg="$QEMU_IMAGE_TOOL_RE" "$AWK_PROC_NAMES"'
        { print basen($(6 + qemushift())) }' <<<"1 1 1 0 S $args"
}

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
    if target_is_windows; then
        # No process groups here, so every mode gives the same host-wide list. No
        # slash switches: without MSYS_NO_PATHCONV a /fi would be rewritten to a
        # path. Plain output is Image Name + PID, which is all we need.
        test "$mode" != others || return 0
        tasklist 2>/dev/null | command grep -Ei "$ENGINE_IMAGE_RE|^python" || true
    else
        # Fields 6 and 7 are the command and its first argument (the interpreter
        # and its script, for the Python fixtures).
        ps_snapshot |
            awk -v pg="$pgid" -v mode="$mode" -v eng="$ENGINE_EXE_RE" -v srv="$FIXTURE_SERVER_RE" \
                -v qint="$QEMU_INTERP_RE" -v qimg="$QEMU_IMAGE_TOOL_RE" \
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
    if target_is_windows; then
        left=$(tasklist 2>/dev/null | command grep -Ei "$ENGINE_IMAGE_RE" || true)
    else
        left=$(list_stray_processes 0 named | awk 'NR > 1')
    fi
    test -n "$left" || return 0
    printf '::warning::%s left processes behind\n' "$label"
    printf '%s\n' "$left"
    if target_is_windows; then
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
        awk -v pg="$pgid" -v eng="$ENGINE_EXE_RE" \
            -v qint="$QEMU_INTERP_RE" -v qimg="$QEMU_IMAGE_TOOL_RE" "$AWK_PROC_NAMES"'
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
# frames resolve to names. Opt-in (HTTRACK_EXCLUSIVE_HOST), because with no
# process group to scope by it stacks every engine on the host: under a parallel
# run that freezes each sibling's for up to 60s, and a cdb killed by its own
# timeout can take its debuggee with it.
dump_windows_stacks() {
    local c p
    local cdb='' found=''
    if test -z "${HTTRACK_EXCLUSIVE_HOST:-}"; then
        printf 'no stacks: other tests are running on this host\n'
        return 0
    fi
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
    for p in $(tasklist 2>/dev/null | command grep -Ei "$ENGINE_IMAGE_RE" | awk '{print $2}'); do
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
    if target_is_windows; then
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
