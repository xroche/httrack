#!/bin/bash
#
# TEMPORARY, not for merge: chases the ext2fs dirscanblock wedge. Runs inside
# the Hurd VM. Phase B churns directories while processes die from a fault with
# their cwd inside them, which is what the suite's crash tests do; phase A is
# the same churn with nothing dying. B runs first, so a wedge in it cannot be
# credited to having run longer.

set -u

say() { echo "[repro $(date +%T)] $*"; }

root=${1:-$HOME/repro}
secs=${2:-900}

churn() { # churn DIR TAG SECONDS
    local d=$1 tag=$2 end=$((SECONDS + $3)) i=0 n m
    mkdir -p "$d"
    while test "$SECONDS" -lt "$end"; do
        i=$((i + 1))
        m=$d/$tag-$i
        mkdir -p "$m" || return 0
        for n in $(seq 1 150); do : >"$m/f$n"; done
        ls "$m" >/dev/null
        for n in $(seq 1 150); do mv "$m/f$n" "$m/g$n" 2>/dev/null; done
        ls "$m" >/dev/null
        rm -rf "$m"
        test $((i % 10)) -ne 0 || say "$tag: $i rounds"
    done
    say "$tag: done, $i rounds"
}

# A process that dies from SIGSEGV with its cwd inside the churned directory.
# On the Hurd the crash server writes a core there whatever ulimit says, so the
# death itself is directory I/O.
crasher() { # crasher DIR SECONDS
    local d=$1 end=$((SECONDS + $2)) i=0
    mkdir -p "$d"
    while test "$SECONDS" -lt "$end"; do
        i=$((i + 1))
        (
            cd "$d" || exit 0
            ulimit -c 0 2>/dev/null
            exec bash -c 'kill -SEGV $$'
        ) >/dev/null 2>&1
        test $((i % 20)) -ne 0 || say "crasher: $i deaths"
    done
    say "crasher: done, $i deaths"
}

say "uname: $(uname -a)"
say "crash server: $(showtrans /servers/crash 2>&1)"
say "free: $(df -k "$HOME" | awk 'NR == 2 { print $4 }') KB"

rm -rf "$root"
mkdir -p "$root"

say "=== PHASE B: churn plus faulting processes, ${secs}s ==="
for t in b1 b2 b3; do churn "$root/B" "$t" "$secs" & done
crasher "$root/B" "$secs" &
wait
say "PHASE B survived"
say "cores left: $(find "$root/B" -name 'core*' 2>/dev/null | wc -l)"
say "free: $(df -k "$HOME" | awk 'NR == 2 { print $4 }') KB"

say "=== PHASE A: the same churn, nothing dying, ${secs}s ==="
for t in a1 a2 a3; do churn "$root/A" "$t" "$secs" & done
wait
say "PHASE A survived"
say "=== both phases survived ==="
