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
# Into TRANSLATED, not echoed: a command substitution strips trailing newlines,
# and losing bytes off an argument is what the cut this replaced used to do.
# Each comma-separated piece, because httrack's own -O takes `dir,cache` and
# both halves are paths.
TRANSLATED=
translate_arg() { # translate_arg ARG
    local rest=$1 piece drive
    TRANSLATED=
    while :; do
        piece=${rest%%,*}
        case $piece in
        /mnt/[A-Za-z]/*)
            # One character, so no newline can hide in the substitution.
            drive=$(printf '%s' "${piece:5:1}" | tr '[:lower:]' '[:upper:]')
            TRANSLATED=$TRANSLATED$drive:${piece:6}
            ;;
        *) TRANSLATED=$TRANSLATED$piece ;;
        esac
        case $rest in
        *,*)
            rest=${rest#*,}
            TRANSLATED=$TRANSLATED,
            ;;
        *) break ;;
        esac
    done
}

args=()
for a in "$@"; do
    case "$a" in
    # A drvfs absolute path, which no URL and no option ever looks like.
    /mnt/[A-Za-z]/*)
        translate_arg "$a"
        args+=("$TRANSLATED")
        ;;
    # A file:// URL built from one of those paths. MSYS produced file://D:/...
    # here, because its own TMPDIR was already a drive-letter path.
    file:///mnt/[A-Za-z]/*)
        translate_arg "${a#file://}"
        args+=("file://$TRANSLATED")
        ;;
    # An option carrying a path as its value. Only when the argument starts
    # with a dash, so a URL with /mnt/ after a ? or & is left alone. The glued
    # -O/mnt/d/x form is not translated, and no test uses it.
    -*=/mnt/[A-Za-z]/*)
        translate_arg "${a#*=}"
        args+=("${a%%=*}=$TRANSLATED")
        ;;
    *) args+=("$a") ;;
    esac
done

# A Windows process started from here sees only the variables WSLENV names,
# where under MSYS it inherited the whole environment. Tests rely on that: one
# sets HOME inline to pin ~ expansion. So name them all, minus the ones WSL owns
# or that mean nothing to a native exe.
bridged=
while IFS= read -r name; do
    # Already named by the caller: naming it again would drop the flags it
    # carries there, and GITHUB_STEP_SUMMARY crosses with /p for a reason.
    case ":${WSLENV:-}:" in *":$name:"* | *":$name/"*) continue ;; esac
    value=${!name}
    # /p for a value that is wholly a drvfs path, which argv translation above
    # would have caught had it arrived as an argument. TMPDIR is one. A value
    # holding a colon may be a list or a path with something after it, and the
    # flag for that is /l, so leave those to cross verbatim as before.
    flag=
    case $value in
    /mnt/[A-Za-z]/*) case $value in *:* | *[[:space:]]*) ;; *) flag=/p ;; esac ;;
    esac
    bridged="${bridged:+$bridged:}$name$flag"
done < <(compgen -e | grep -vxE 'PATH|WSLENV|_|SHLVL|PWD|OLDPWD|HOSTTYPE|IFS|LS_COLORS|TERM')
test -z "$bridged" || export WSLENV="${WSLENV:+$WSLENV:}$bridged"

# The +"..." form: bash 3.2 on macOS treats "${args[@]}" as unbound when the
# array is empty, and 257 calls this with no arguments at all.
exec "$(basename "$0").exe" ${args[@]+"${args[@]}"}
