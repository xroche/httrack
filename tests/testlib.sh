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

is_windows() {
    case "$(uname -s)" in
    MINGW* | MSYS* | CYGWIN*) return 0 ;;
    *) return 1 ;;
    esac
}

# Stop a backgrounded server and reap it; MSYS cannot signal a native python.exe,
# so only -9 lands. Every step is "|| true": callers run under set -e, and reaping
# a server we just signalled makes wait return 143.
stop_server() {
    test -n "${1:-}" || return 0
    kill "$1" 2>/dev/null || true
    if is_windows; then
        kill -9 "$1" 2>/dev/null || true
    fi
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
        rm -rf "$d" # so a later test's dump cannot re-report this one
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
            wait "$pid" 2>/dev/null || true
            return 124
        fi
        sleep 1
        waited=$((waited + 1))
    done
    wait "$pid"
}
