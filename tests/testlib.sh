#!/bin/bash
#
# Helpers shared by the crawl tests. Sourced, not run.

# Python 3 interpreter, or empty. Windows installs it as python.exe only, and a
# bare "python" may be 2.x or the Store stub, so probe rather than trust the name.
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

# Native form of a path, for arguments handed to a non-MSYS binary: httrack.exe
# and python.exe cannot resolve the /d/a/... paths Git Bash hands out.
nativepath() {
    if command -v cygpath >/dev/null 2>&1; then
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

# Stop a backgrounded test server and reap it, so the port is released and the
# tmpdir can be removed. MSYS cannot deliver a signal to a native python.exe, so
# only -9 (TerminateProcess) lands there; without it the wait below never returns.
# Every step is "|| true": the caller runs under set -e, and reaping a server we
# just signalled makes wait return 143.
stop_server() {
    test -n "${1:-}" || return 0
    kill "$1" 2>/dev/null || true
    if is_windows; then
        kill -9 "$1" 2>/dev/null || true
    fi
    wait "$1" 2>/dev/null || true
    return 0
}
