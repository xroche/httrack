#!/bin/bash
#
# Refuse a WSL2 kernel the Windows suite cannot pass on. Kernel 5.10.16.3, which
# the cached wsl_update_x64.msi installs, fails every Windows-process launch
# carrying more than about 1.2 KB of argv or environment. 6.18 passes.
#
# Usage: wsl-kernel-check.sh <uname -r of the distro> <true if wsl --update worked>
set -euo pipefail

release=${1:-}
updated=${2:-}

# No distro is the suite step's to report, and it names it better.
if test -z "$release"; then
    echo "no kernel version to check, so the suite step decides"
    exit 0
fi
major=${release%%.*}
case "$major" in
'' | *[!0-9]*)
    echo "::warning::cannot read the WSL2 kernel version '$release'"
    exit 0
    ;;
esac
if test "$major" -ge 6; then
    echo "WSL2 kernel $release"
    exit 0
fi
if test "$updated" = true; then
    why="wsl --update worked but left"
else
    why="wsl --update failed, and the cached package installed"
fi
echo "::error::$why WSL2 kernel $release, which cannot run the suite: re-run this leg"
exit 1
