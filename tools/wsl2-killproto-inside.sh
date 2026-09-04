#!/bin/bash
#
# Spike (#1228): under WSL2 a Windows exe launched over interop has no
# /proc/<pid>/winpid, so the harness cannot reap what it started. This proves
# whether one Win32_Process query keyed on the per-launch output directory can
# replace it. $1 is the repo directory as seen from inside the distro.
#
# No `set -e`: every numbered point must report even when an earlier one fails.
set -uo pipefail

repo=${1:?repo path inside the distro}
ps1_win=$(wslpath -w "$repo/tools/wsl2-killproto.ps1")
psh=$(command -v powershell.exe || echo /mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe)
fails=0
echo "powershell: $psh"
echo "ps1: $ps1_win"

note() { echo "== $*"; }
pass() { echo "RESULT $1: PASS ${2:-}"; }
fail() {
    echo "RESULT $1: FAIL ${2:-}"
    fails=$((fails + 1))
}

# Run the helper, echoing its output so the log keeps everything.
helper() {
    "$psh" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$ps1_win" "$@" 2>&1 |
        tr -d '\r' | tee /dev/stderr
}

launch_victim() {
    local outdir_win=$1 childtag=$2 children=$3
    "$psh" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$ps1_win" \
        -Mode victim -OutDir "$outdir_win" -ChildTag "$childtag" -Children "$children" \
        >/dev/null 2>&1 &
    echo $!
}

alive_check() {
    local marker=$1 pids=${2:-}
    if [ -n "$pids" ]; then
        helper -Mode alive -Marker "$marker" -Pids "$pids"
    else
        helper -Mode alive -Marker "$marker"
    fi
}

kill_pids() {
    [ -n "${1:-}" ] || return 0
    helper -Mode kill -Pids "$1"
}

wait_for_log() {
    local log=$1 i
    for i in $(seq 100); do
        [ -s "$log" ] && return 0
        sleep 0.2
    done
    return 1
}

########################################################################
note "point 0: killing the Linux relay leaves the Windows process alive"
m0="kpmark-$(date +%s)-$RANDOM-relay"
out0="$repo/$m0"
mkdir -p "$out0"
relay=$(launch_victim "$(wslpath -w "$out0")" "kptag-$m0" 0)
echo "relay linux pid=$relay"
if wait_for_log "$out0/log.txt"; then
    echo "victim is writing"
else
    echo "victim never wrote a log"
fi
if [ -e "/proc/$relay/winpid" ]; then
    echo "WINPID_PRESENT=$(cat "/proc/$relay/winpid")"
    fail 0 "/proc/<pid>/winpid exists, MSYS route still available"
else
    echo "WINPID_ABSENT (as expected under WSL2)"
fi
kill -9 "$relay" 2>/dev/null
wait "$relay" 2>/dev/null
sleep 2
relay_left=$(alive_check "$m0" | sed -n 's/^MARKER_LEFT=//p' | tail -1)
echo "after killing the relay, processes still carrying the marker: $relay_left"
if [ "${relay_left:-0}" -gt 0 ]; then
    pass 0 "the Windows process survives its relay, which is the bug"
else
    fail 0 "the Windows process died with the relay (unexpected on WSL2)"
fi
kill_pids "$(helper -Mode find -Marker "$m0" 2>/dev/null | sed -n 's/^HIT=\([0-9]*\).*/\1/p' | paste -sd, -)" >/dev/null 2>&1
sleep 1

########################################################################
note "points 1-5: find, read, kill the tree, verify"
mark="kpmark-$(date +%s)-$RANDOM-tree"
childtag="kpchild-$mark"
outdir="$repo/$mark"
mkdir -p "$outdir"
outdir_win=$(wslpath -w "$outdir")
relay=$(launch_victim "$outdir_win" "$childtag" 2)
echo "relay linux pid=$relay outdir=$outdir_win"
wait_for_log "$outdir/log.txt" || echo "warning: no log yet"
sleep 3

# --- point 5: what one query costs, paid per kill, from inside WSL2.
note "point 5: query cost"
for i in 1 2 3; do
    t0=$(date +%s%N)
    fout=$(helper -Mode find -Marker "$mark" 2>/dev/null)
    t1=$(date +%s%N)
    echo "WALL_MS_$i=$(((t1 - t0) / 1000000)) $(echo "$fout" | sed -n 's/^QUERY_MS=/inproc_ms=/p')"
done
t0=$(date +%s%N)
helper -Mode findslow -Marker "$mark" >/dev/null 2>&1
t1=$(date +%s%N)
echo "WALL_MS_findslow=$(((t1 - t0) / 1000000))"

# --- point 1: the marker resolves to a real Windows pid.
note "point 1: resolve the Windows pid from the marker"
find_out=$(helper -Mode find -Marker "$mark" 2>/dev/null)
root_pid=$(echo "$find_out" | sed -n 's/^HIT=\([0-9]*\).*/\1/p' | head -1)
hits=$(echo "$find_out" | grep -c '^HIT=')
echo "hits=$hits root_pid=${root_pid:-none}"
if [ -n "$root_pid" ] && [ "$hits" -eq 1 ]; then
    pass 1 "one hit, pid $root_pid"
elif [ -n "$root_pid" ]; then
    fail 1 "the marker matched $hits processes"
else
    fail 1 "the marker matched nothing"
fi

# --- point 3 (collect first): the tree the kill has to take with it.
note "point 3: descendants out of one snapshot"
snap=$(helper -Mode snapshot -Marker "$mark" 2>/dev/null)
desc_pids=$(echo "$snap" | sed -n 's/^DESC=\([0-9]*\).*/\1/p' | paste -sd, -)
echo "descendants=${desc_pids:-none}"

# --- point 4: read the output BEFORE signalling, and check it after.
note "point 4: output read before the kill stays readable"
cp "$outdir/log.txt" "/tmp/pre-$mark.txt"
pre_bytes=$(wc -c <"/tmp/pre-$mark.txt")
pre_last=$(tail -1 "/tmp/pre-$mark.txt")
echo "pre-kill bytes=$pre_bytes last=[$pre_last]"

# --- point 2 + 3: kill the resolved pid, tree included.
note "point 2: terminate from WSL2"
kill_pids "$root_pid" >/dev/null
sleep 2

post_bytes=$(wc -c <"$outdir/log.txt")
post_last=$(tail -1 "$outdir/log.txt")
echo "post-kill bytes=$post_bytes last=[$post_last]"
if [ "$post_bytes" -ge "$pre_bytes" ] && cmp -s -n "$pre_bytes" "/tmp/pre-$mark.txt" "$outdir/log.txt"; then
    pass 4 "the $pre_bytes bytes read before the kill are byte-identical after it"
else
    fail 4 "the output read before the kill does not match the file after it"
fi

# Re-query rather than trusting taskkill's exit status.
note "point 2/3 verification: re-query"
check=$(alive_check "$mark" "$root_pid${desc_pids:+,$desc_pids}" 2>/dev/null)
alive_n=$(echo "$check" | grep -c '^ALIVE=')
marker_left=$(echo "$check" | sed -n 's/^MARKER_LEFT=//p' | tail -1)
echo "still alive=$alive_n marker_left=$marker_left"
if [ "$alive_n" -eq 0 ] && [ "${marker_left:-1}" -eq 0 ]; then
    pass 2 "the resolved pid is really gone"
else
    fail 2 "$alive_n of the killed pids still answer"
fi

# The children never carried the marker, so this is an independent check.
child_left=$(alive_check "$childtag" 2>/dev/null | sed -n 's/^MARKER_LEFT=//p' | tail -1)
echo "children still carrying the child tag: $child_left"
n_desc=$(echo "$snap" | grep -c '^DESC=')
if [ "$n_desc" -ge 2 ] && [ "${child_left:-1}" -eq 0 ] && [ "$alive_n" -eq 0 ]; then
    pass 3 "$n_desc descendants found, none left"
elif [ "$n_desc" -lt 2 ]; then
    fail 3 "only $n_desc descendants were seen, the tree case was not exercised"
else
    fail 3 "$child_left children survived"
fi

# The Linux relay after its Windows process is gone.
if kill -0 "$relay" 2>/dev/null; then
    echo "relay $relay still exists after the Windows process died"
else
    echo "relay $relay exited with its Windows process"
fi
wait "$relay" 2>/dev/null

echo "KILLPROTO_FAILURES=$fails"
exit 0
