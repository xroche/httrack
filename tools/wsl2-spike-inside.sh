#!/bin/bash
#
# Runs inside the spike's WSL2 distro (#1228): what each layer costs, and which
# direction of loopback survives the boundary. $1 is a Windows-side server port.
set -uo pipefail

port=${1:-}

began=$SECONDS
for _ in $(seq 500); do /bin/true; done
echo "wsl2-fork: 500 spawns in $((SECONDS - began))s"

began=$SECONDS
for _ in $(seq 100); do /mnt/c/Windows/System32/cmd.exe /c exit >/dev/null 2>&1; done
echo "wsl2-interop: 100 windows spawns in $((SECONDS - began))s"

io=/mnt/c/probe-io
mkdir -p "$io"
began=$SECONDS
for i in $(seq 300); do echo x >"$io/f$i"; done
sync
echo "wsl2-9p-write: 300 files in $((SECONDS - began))s"
began=$SECONDS
cat "$io"/f* >/dev/null
echo "wsl2-9p-read: 300 files in $((SECONDS - began))s"
rm -rf "$io"

began=$SECONDS
for _ in $(seq 300); do : >/tmp/probe-f; done
echo "wsl2-ext4-write: 300 files in $((SECONDS - began))s"
rm -f /tmp/probe-f

if [ -n "$port" ]; then
    code=$(curl -s -o /dev/null -w '%{http_code}' --max-time 5 "http://127.0.0.1:$port/" || echo none)
    echo "wsl2-to-windows-localhost: $code"
    host=$(ip route show default | awk '{print $3}' | head -1)
    code=$(curl -s -o /dev/null -w '%{http_code}' --max-time 5 "http://$host:$port/" || echo none)
    echo "wsl2-to-windows-hostip($host): $code"
fi
