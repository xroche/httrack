#!/bin/bash
#
# Count the call sites where strcpybuff() / strcatbuff() / strncatbuff()
# silently degrade to an unchecked strcpy() / strcat() / strncat().
#
# That happens whenever the destination expression is a pointer rather than a
# char[] array, because the macros size the destination with sizeof(). The
# call site still reads as if it were bounded, which is what makes this worth
# tracking rather than eyeballing. See src/htssafe.h.
#
# The fix for a given site is to pass the real capacity explicitly:
#   strcpybuff(d, s)      ->  strlcpybuff(d, s, <capacity>)
#   strcatbuff(d, s)      ->  strlcatbuff(d, s, <capacity>)
#   strncatbuff(d, s, n)  ->  strlncatbuff(d, s, <capacity>, n)
#
# Usage:  tools/audit-unchecked-buffers.sh [--list]
# Run from the top of a configured build tree. Requires gcc: the probe uses
# __attribute__((warning)), and relies on -O2 folding the compile-time branch
# so that the checked (array) case stays silent.

set -u

# Ratchet. Lower this as sites are fixed; never raise it.
BASELINE=156

cd "$(dirname "$0")/.." || exit 1

if test ! -f src/Makefile ; then
	echo "error: run ./configure first (src/Makefile not found)" >&2
	exit 1
fi

log=$(mktemp) || exit 1
trap 'rm -f "$log"' EXIT

# -w would suppress the probe's warnings too, so quiet the noisy-but-known
# warnings individually instead.
quiet="-Wno-unused-parameter -Wno-sign-compare -Wno-unused-variable"

for f in src/*.c src/proxy/*.c ; do
	test -f "$f" || continue
	gcc -c -o /dev/null \
		-Isrc -I. -Isrc/proxy -Isrc/coucal -DHAVE_CONFIG_H \
		-O2 -DHTS_AUDIT_UNCHECKED_BUFFERS $quiet \
		"$f" 2>>"$log"
done

sites=$(grep -B4 'destination is a pointer' "$log" \
	| grep -oE '^[a-zA-Z0-9_/.-]+\.c:[0-9]+:[0-9]+:' \
	| sort -u)
count=$(test -n "$sites" && echo "$sites" | wc -l || echo 0)

if test "${1:-}" == "--list" ; then
	echo "$sites"
	echo "--"
	echo "$sites" | cut -d: -f1 | sort | uniq -c | sort -rn
fi

echo "unchecked buffer call sites: $count (baseline $BASELINE)" >&2

if test "$count" -gt "$BASELINE" ; then
	echo "ERROR: $((count - BASELINE)) new unchecked buffer call site(s)." >&2
	echo "Pass the destination capacity explicitly; see the header of this script." >&2
	exit 1
elif test "$count" -lt "$BASELINE" ; then
	echo "Good: $((BASELINE - count)) fewer than the baseline." >&2
	echo "Lower BASELINE in $0 to $count to lock that in." >&2
fi

exit 0
