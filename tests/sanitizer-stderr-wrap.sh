#!/bin/sh
#
# Body of the PATH shims tests/Makefile.am generates: run PROGRAM with a copy of
# its stderr kept in $HTTRACK_STDERR_CAPTURE_DIR, where tools/ci-sanitizer-report.sh
# finds it. gcc's UBSan ignores log_path and reports on stderr, which most tests
# send to /dev/null or into a tmpdir they delete.
#
# Usage: sanitizer-stderr-wrap.sh PROGRAM [ARG...]. PROGRAM is exec'd whether the
# capture is on or off, so the pid, the process group and the exit status the
# tests read stay its own.
set -u

prog=$1
shift

dir=${HTTRACK_STDERR_CAPTURE_DIR:-}

# The setup below forks, and a test's interposer belongs in the engine alone:
# 113's rewrites its report file from every process it loads into.
preload_set=${LD_PRELOAD+y}
preload=${LD_PRELOAD-}
unset LD_PRELOAD

# Setting the capture up must never cost a test its run.
if test -n "$dir" && mkdir -p "$dir" 2>/dev/null; then
    # The test name, so a report names its test; anything else is not a filename.
    tag=${HTTRACK_STDERR_CAPTURE_TAG:-engine}
    case $tag in '' | *[!A-Za-z0-9._-]*) tag=engine ;; esac

    fifo=$dir/.fifo.$$
    rm -f "$fifo"
    if mkfifo "$fifo" 2>/dev/null; then
        # tee, so the test still gets every byte on the stderr it chose,
        # /dev/null included; cat drains the fifo if tee dies, or the engine
        # takes a SIGPIPE from its own stderr. An exec'd sh, never a `{ }`
        # subshell, which would keep whatever the fork already had loaded.
        sh -c 'tee -a "$1" >&2 || cat >/dev/null' wrap "$dir/$tag.$$.log" <"$fifo" &
        exec 2>"$fifo" # blocks until the reader opens, which is what makes the unlink safe
        rm -f "$fifo"
    fi
fi

test -z "$preload_set" || export LD_PRELOAD="$preload"
exec "$prog" "$@"
