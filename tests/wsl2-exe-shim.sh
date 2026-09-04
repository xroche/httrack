#!/bin/bash
#
# PATH shim for the wsl2 backend, symlinked once per engine under the name the
# tests use. Two things stand between a Linux shell and a native exe: WSL2's
# PATH matches a filename exactly, so a bare "httrack" finds nothing, and a
# drvfs path means nothing to the exe once it gets there. Both are fixed here
# rather than at ~73 call sites.
#
# exec, so the pid does not change: win_capture reads /proc/<pid>/cmdline to
# find the Windows process, and after the exec that cmdline is the translated
# one the Windows side really carries.
set -uo pipefail

args=()
for a in "$@"; do
    case "$a" in
    # A drvfs absolute path, which no URL and no option ever looks like.
    /mnt/[a-z]/*) args+=("$(wslpath -m "$a")") ;;
    *) args+=("$a") ;;
    esac
done

exec "$(basename "$0").exe" "${args[@]}"
