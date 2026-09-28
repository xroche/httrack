#!/bin/bash
#
# TEMPORARY, not for merge: second pass at the ext2fs dirscanblock wedge. Each
# phase kills a process that is itself creating and removing directory entries,
# which the first pass did not do. Phase K uses SIGKILL, which no crash server
# sees, and phase S a fault, which goes through /hurd/crash. Whichever wedges
# says whether the crash server is part of it or only the abrupt death is.

set -u

say() { echo "[repro2 $(date +%T)] $*"; }

root=${1:-$HOME/repro2}
secs=${2:-600}

# Grows and shrinks the directory itself, so its blocks keep changing under the
# scanner, until something kills it.
io_child() { # io_child DIR
    exec bash -c 'cd "$1" || exit 0
        i=0
        while :; do
            i=$((i + 1))
            for n in $(seq 1 60); do : >"n$$-$i-$n"; done
            ls >/dev/null 2>&1
            for n in $(seq 1 60); do mv "n$$-$i-$n" "m$$-$i-$n" 2>/dev/null; done
            rm -f "n$$-$i-"* "m$$-$i-"*
        done' bash "$1"
}

# Kills such a child mid-operation, over and over.
killer() { # killer DIR SIGNAL SECONDS
    local d=$1 sig=$2 end=$((SECONDS + $3)) i=0 child
    mkdir -p "$d"
    while test "$SECONDS" -lt "$end"; do
        i=$((i + 1))
        io_child "$d" &
        child=$!
        sleep 0.2
        kill "-$sig" "$child" 2>/dev/null
        wait "$child" 2>/dev/null
        test $((i % 25)) -ne 0 || say "$sig: $i children killed"
    done
    say "$sig: done, $i children killed"
}

# Looks names up in the phase directory while its entries come and go, which
# is what calls dirscanblock over a directory whose blocks are changing.
scanner() { # scanner DIR SECONDS
    local d=$1 end=$((SECONDS + $2)) i=0
    while test "$SECONDS" -lt "$end"; do
        i=$((i + 1))
        ls "$d" >/dev/null 2>&1
        test -e "$d/k-nothing-$i" || true
    done
    say "scanner: done, $i passes"
}

say "uname: $(uname -a)"
say "crash server: $(showtrans /servers/crash 2>&1)"

rm -rf "$root"
mkdir -p "$root"

for phase in S:SEGV K:KILL T:TERM; do
    sig=${phase#*:}
    d=$root/${phase%:*}
    mkdir -p "$d"
    say "=== PHASE ${phase%:*}: children killed with SIG$sig mid directory I/O, ${secs}s ==="
    scanner "$d" "$secs" &
    killer "$d" "$sig" "$secs" &
    killer "$d" "$sig" "$secs"
    wait
    say "PHASE ${phase%:*} survived, $(find "$d" -maxdepth 1 | wc -l) entries left"
done
say "=== all phases survived ==="
