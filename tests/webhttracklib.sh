#!/bin/bash
#
# htsserver launch, request and reaping helpers. Sourced, not run.

HTS_TESTDIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=tests/testlib.sh
. "${HTS_TESTDIR}/testlib.sh"

HTS_DISTDIR=$(cd "${top_srcdir:-${HTS_TESTDIR}/..}" && pwd)
HTS_BG_PIDS=()
HTS_SRV_PIDS=()
HTS_REAPED_PIDS=()
HTS_TMP_LOGS=()
HTS_TMP_DIRS=()

HTS_CR=$(printf '\r')

# htsserver, plus a python3 the Debian buildd chroot may not have: that one skips.
htsserver_require() {
    command -v htsserver >/dev/null || fail "no htsserver in PATH"
    HTS_PYTHON=$(find_python) || skip "python3 not found"
}

# shellcheck disable=SC2120 # one port is the common case
htsserver_freeport() {
    local python=${HTS_PYTHON}
    freeport "$@"
}

# First URL= and PID= of the announcement, or empty. Windows announces no pid.
htsserver_announced() {
    local line
    HTS_URL=
    HTS_PID=
    test -r "${HTS_LOG}" || return 0
    while read -r line; do
        line=${line%"$HTS_CR"}
        case ${line} in
        URL=*) test -n "${HTS_URL}" || HTS_URL=${line#URL=} ;;
        PID=*) test -n "${HTS_PID}" || HTS_PID=${line#PID=} ;;
        esac
    done <"${HTS_LOG}"
}

# The install reaches the documentation through $(datadir)/httrack/html/doc,
# which the source tree has no counterpart for, so the panes' Help links dangle
# when the source root is served. $HTS_DISTROOT is the source tree with that one
# link added, built once per test. It is a superset of the shipped layout rather
# than a model of it: test 396 serves a staged install for the real shape.
htsserver_distroot() {
    test -z "${HTS_DISTROOT:-}" || return 0
    local root entry
    root=$(mktemp -d "${TMPDIR:-/tmp}/htsdist.XXXXXX") || fail "no tmpdir"
    HTS_TMP_DIRS+=("${root}")
    mkdir -p "${root}/html" || fail "no ${root}/html"
    for entry in "${HTS_DISTDIR}"/*; do
        test "${entry##*/}" = html || ln -s "${entry}" "${root}/${entry##*/}"
    done
    for entry in "${HTS_DISTDIR}"/html/*; do
        ln -s "${entry}" "${root}/html/${entry##*/}"
    done
    ln -s "${HTS_DISTDIR}" "${root}/html/doc"
    HTS_DISTROOT=${root}
}

# The server process; reads htsserver_start's locals and its background stdout.
htsserver_exec() {
    # htsserver keeps SIGTERM ignored across its exec, so only -9 reaps it.
    trap '' TERM TTOU XFSZ
    test -z "${wlimit}" || ulimit -f "${wlimit}"
    test -z "${home}" || export HOME="${home}"
    exec htsserver "${root%/}/" --port "${HTS_PORT}" "$@" 2>&1
}

# Start htsserver in the background on a free port and wait for its
# announcement. Sets HTS_URL, HTS_PORT, HTS_LOG, HTS_BGPID and HTS_PID (the
# server's own pid, which Windows does not announce). Options, ahead of any
# htsserver argument:
#   --root DIR       tree to serve (default: $HTS_DISTROOT)
#   --home DIR       $HOME for the server, so no ~/.httrack.ini leaks in
#   --log FILE       where the announcement lands (default: a temp file)
#   --port N         a port already picked, to keep the fork out of a window the
#                    caller is timing; owned by the caller, so a lost bind fails
#                    rather than being redrawn
#   --write-limit N  ulimit -f N; the log rides a pipe, which the cap spares
# shellcheck disable=SC2120 # most callers need no htsserver argument
htsserver_start() {
    local root='' home='' log='' wlimit='' port=''
    while test $# -gt 0; do
        case $1 in
        --root)
            root=$2
            shift 2
            ;;
        --home)
            home=$2
            shift 2
            ;;
        --log)
            log=$2
            shift 2
            ;;
        --write-limit)
            wlimit=$2
            shift 2
            ;;
        --port)
            port=$2
            shift 2
            ;;
        --)
            shift
            break
            ;;
        *) break ;;
        esac
    done
    if test -z "${root}"; then
        htsserver_distroot
        root=${HTS_DISTROOT}
    fi

    # freeport hands back a port it has already released, so a neighbour can take
    # it before the server binds: redraw and retry, as start_proxytrack does.
    local ownlog='' try start
    test -n "${log}" || ownlog=1
    for try in 1 2 3; do
        HTS_PORT=${port}
        test -n "${HTS_PORT}" ||
            HTS_PORT=$(htsserver_freeport) || fail "no free loopback port"
        # One log per attempt: a killed attempt's writer can still be draining.
        if test -n "${ownlog}"; then
            log=$(mktemp)
            HTS_TMP_LOGS+=("${log}")
        fi
        HTS_LOG=${log}
        : >"${HTS_LOG}"
        if test -n "${wlimit}"; then
            # Process substitution, not a pipeline: $! must be the server. In a
            # pipeline it is the reader, so a start that never announces leaves the
            # server unkilled and parks the reaping wait on the whole job.
            htsserver_exec "$@" > >(cat >"${HTS_LOG}") &
        else
            htsserver_exec "$@" >"${HTS_LOG}" &
        fi
        HTS_BGPID=$!
        HTS_BG_PIDS+=("${HTS_BGPID}")

        # A sed and a sleep per tick is a process per tick (#795), and the deadline
        # self-extends rather than expiring on a loaded parallel run.
        start=$SECONDS
        while :; do
            htsserver_announced
            test -z "${HTS_URL}" || break
            kill -0 "${HTS_BGPID}" 2>/dev/null || break
            test "$((SECONDS - start))" -lt 60 || break
            poll_wait 0.1
        done
        test -z "${HTS_URL}" || break
        # Only a lost bind is worth redrawing; anything else is a regression.
        command grep -qE "${BIND_LOST}" "${HTS_LOG}" ||
            fail "htsserver did not come up: $(<"${HTS_LOG}")"
        test -z "${port}" ||
            fail "htsserver could not bind port ${port}: $(<"${HTS_LOG}")"
        stop_server "${HTS_BGPID}"
        # Loud, so an intermittent bind regression cannot hide behind the retry.
        echo "htsserver did not get port ${HTS_PORT}, retrying" >&2
    done
    test -n "${HTS_URL}" || fail "htsserver bound none of 3 ports: $(<"${HTS_LOG}")"
    test -z "${HTS_PID}" || HTS_SRV_PIDS+=("${HTS_PID}")
}

# Reap every server started so far and remember them for
# htsserver_assert_reaped. Safe from a cleanup trap, and safe to call twice.
htsserver_stop() {
    local pid
    HTS_REAPED_PIDS=(${HTS_BG_PIDS[@]+"${HTS_BG_PIDS[@]}"}
        ${HTS_SRV_PIDS[@]+"${HTS_SRV_PIDS[@]}"})
    # Grouped: bash announces a killed job at any command boundary, so the
    # notice escapes a redirect on the wait alone once there are two servers.
    {
        for pid in ${HTS_SRV_PIDS[@]+"${HTS_SRV_PIDS[@]}"} \
            ${HTS_BG_PIDS[@]+"${HTS_BG_PIDS[@]}"}; do
            kill -9 "${pid}" 2>/dev/null || true
        done
        for pid in ${HTS_BG_PIDS[@]+"${HTS_BG_PIDS[@]}"}; do
            reap_bounded "${pid}" || true
        done
    } 2>/dev/null
    HTS_SRV_PIDS=()
    HTS_BG_PIDS=()
}

# Teardown: reap, then drop the logs the library allocated. Keep it out of
# htsserver_stop, which a test may call mid-run before reading the log back.
htsserver_cleanup() {
    htsserver_stop
    rm -f ${HTS_TMP_LOGS[@]+"${HTS_TMP_LOGS[@]}"}
    HTS_TMP_LOGS=()
    # Symlinks only, so this never reaches the source tree they point into.
    rm -rf ${HTS_TMP_DIRS[@]+"${HTS_TMP_DIRS[@]}"}
    HTS_TMP_DIRS=()
    HTS_DISTROOT=
}

# Teardown for a test that owns a work dir: htsserver holds ${HOME} under it, so
# the reap has to precede the removal. Takes the dir rather than reading a
# caller variable, since cleanup_push records its arguments at push time.
htsserver_cleanup_dir() { # htsserver_cleanup_dir DIR
    htsserver_cleanup
    rm -rf "$1"
}

# Reap helper processes the test backgrounded, named by VARIABLE: their pids are
# assigned after the teardown frame is pushed.
htsserver_reap_vars() { # htsserver_reap_vars VAR...
    local v
    for v in "$@"; do
        test -z "${!v:-}" || kill -9 "${!v}" 2>/dev/null || true
    done
    # Absorbs bash's async "Killed" notice, which reads as a failure otherwise.
    for v in "$@"; do
        test -z "${!v:-}" || wait "${!v}" 2>/dev/null || true
    done
}

# A leaked htsserver wedges the parallel harness behind a green log. Bounded
# rather than instantaneous: under the sanitizer shim (tests/stderrwrap.c) the
# server is the shim's child, so killing both leaves a zombie its reaper has yet
# to collect, and a zombie holds neither the port nor the payload.
htsserver_assert_reaped() {
    local pid start
    test "${#HTS_REAPED_PIDS[@]}" -gt 0 || fail "nothing was reaped to assert on"
    for pid in ${HTS_REAPED_PIDS[@]+"${HTS_REAPED_PIDS[@]}"}; do
        start=${SECONDS}
        while kill -0 "${pid}" 2>/dev/null; do
            test "$((SECONDS - start))" -le "${REAP_GRACE}" ||
                fail "htsserver ${pid} survived"
            poll_wait 0.1
        done
    done
}

htsserver_alive() { test -n "${HTS_PID:-}" && kill -0 "${HTS_PID}" 2>/dev/null; }

# Raw HTTP against $HTS_PORT; tests/httpclient.py documents the options.
htsserver_client() {
    "${HTS_PYTHON}" "${HTS_TESTDIR}/httpclient.py" --port "${HTS_PORT}" "$@"
}

# Python with tests/ importable: "python -" puts no script directory on
# sys.path, so a heredoc cannot import webtestlib without this.
htsserver_python() {
    PYTHONPATH="${HTS_TESTDIR}${PYTHONPATH:+:$PYTHONPATH}" "${HTS_PYTHON}" "$@"
}

# The reply to a GET of $1, status line and headers included.
htsserver_get() { htsserver_client --path "$1"; }

# The reply to a POST of the urlencoded body $1, to / unless $2 names a page.
htsserver_post() { htsserver_client --path "${2:-/}" --post "$1"; }

# The session id the server renders into every form, as a browser picks it up.
# Callers assert its length: an empty one makes every later check vacuous.
htsserver_sid() {
    firstline "$(htsserver_get /server/index.html |
        sed -n 's/.*name="sid" value="\([0-9a-f]*\)".*/\1/p')"
}
