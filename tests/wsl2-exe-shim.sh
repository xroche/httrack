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

# The same drvfs mapping testlib.sh does, spelled again because this runs as its
# own process inside the distro, and because a bare rootfs ships no wslpath.
args=()
rest=
for a in "$@"; do
    case "$a" in
    # A drvfs absolute path, which no URL and no option ever looks like.
    /mnt/[A-Za-z]/*)
        drive=$(printf '%s' "$a" | cut -c6 | tr '[:lower:]' '[:upper:]')
        args+=("$drive:$(printf '%s' "$a" | cut -c7-)")
        ;;
    # A file:// URL built from one of those paths. MSYS produced file://D:/...
    # here, because its own TMPDIR was already a drive-letter path.
    file:///mnt/[A-Za-z]/*)
        rest=${a#file://}
        drive=$(printf '%s' "$rest" | cut -c6 | tr '[:lower:]' '[:upper:]')
        args+=("file://$drive:$(printf '%s' "$rest" | cut -c7-)")
        ;;
    *) args+=("$a") ;;
    esac
done

# A Windows process started from here sees only the variables WSLENV names,
# where under MSYS it inherited the whole environment. Tests rely on that: one
# sets HOME inline to pin ~ expansion. So name them all, minus the ones WSL owns
# or that mean nothing to a native exe.
bridged=$(compgen -e |
    grep -vxE 'PATH|WSLENV|_|SHLVL|PWD|OLDPWD|HOSTTYPE|IFS|LS_COLORS|TERM' |
    paste -sd: -)
test -z "$bridged" || export WSLENV="${WSLENV:+$WSLENV:}$bridged"

# The +"..." form: bash 3.2 on macOS treats "${args[@]}" as unbound when the
# array is empty, and 257 calls this with no arguments at all.
exec "$(basename "$0").exe" ${args[@]+"${args[@]}"}
