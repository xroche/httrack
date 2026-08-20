#!/bin/bash
#
# ARC 1.0 fixtures for the proxytrack tests. Sourced, not run.

# Every fixture dates its records the same: the tests assert on rewriting, not
# on the clock.
ARC_DATE=20250101000000

# The version block an ARC file opens with. LEN is the declared length of that
# block, 9 stopping at the "2 0" line; 154 passes 10 to swallow the blank line
# the writer used to include.
# shellcheck disable=SC2120 # most callers take the default
arc_filedesc() { # arc_filedesc [LEN]
    printf 'filedesc://t.arc 0.0.0.0 %s text/plain 200 - - 0 t.arc %d\n' "$ARC_DATE" "${1:-9}"
    printf '2 0 test\n'
    printf '\n\n'
}

# One record: the header line, then HDRFILE and BODYFILE verbatim. The declared
# length counts BYTES, so a body outside ASCII still declares its true length.
arc_record() { # arc_record URL MIME STATUS HDRFILE BODYFILE
    printf '%s 0.0.0.0 %s %s %s - - 0 t.arc %d\n' "$1" "$ARC_DATE" "$2" "$3" \
        "$(($(wc -c <"$4") + $(wc -c <"$5")))"
    cat "$4" "$5"
}
