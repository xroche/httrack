#!/bin/bash
#
# Emit a CycloneDX 1.5 SBOM for this tree, and check THIRD-PARTY-NOTICES.md
# against what is actually vendored.
#
# There is no lockfile here to read a dependency list off. What ends up in a
# build is a mix of two things that have to be collected differently:
#
#   - Source committed into this repository (src/minizip, src/md5.c,
#     src/punycode.c, and the src/coucal submodule). These are pinned by the
#     commit being built, so they are listed below and cross-checked against
#     THIRD-PARTY-NOTICES.md.
#
#   - Libraries resolved at link time (zlib, OpenSSL, libiconv). These are a
#     property of the build host, not of this tree, so they are read back out
#     of the built shared object.
#
# That second part is why this reads the binary rather than pkg-config: on a
# host carrying OpenSSL 3.0.2 runtime with 1.1.1 development files,
# "pkg-config --modversion openssl" answers 1.1.1f while the binary links
# libcrypto.so.3. The SBOM has to say what was linked.
#
# Usage:
#   tools/generate-sbom.sh [--output FILE]   write the SBOM (default: stdout)
#   tools/generate-sbom.sh --check           only verify the notices file
#
# --check needs no build. Generating the full SBOM needs ./configure && make
# first; without a build the linked-library section is omitted and the script
# says so on stderr.

set -u

cd "$(dirname "$0")/.." || exit 1

OUTPUT=
CHECK_ONLY=0

while test $# -gt 0 ; do
	case "$1" in
	--output) shift ; OUTPUT="${1:-}" ;;
	--output=*) OUTPUT="${1#--output=}" ;;
	--check) CHECK_ONLY=1 ;;
	-h|--help) sed -n '2,30p' "$0" | sed 's/^# \{0,1\}//' ; exit 0 ;;
	*) echo "error: unknown argument '$1'" >&2 ; exit 2 ;;
	esac
	shift
done

NOTICES=THIRD-PARTY-NOTICES.md

# ---------------------------------------------------------------------------
# Vendored components. Adding third-party source to this tree means adding a
# row here and a row in THIRD-PARTY-NOTICES.md; --check enforces the pairing.
#
# Fields: path | name | version | license (SPDX id, or a short phrase where
# there is no SPDX id) | supplier
# ---------------------------------------------------------------------------
VENDORED="
src/minizip|minizip|1.1|Zlib|Gilles Vollant
src/md5.c|md5|1993|LicenseRef-public-domain|Colin Plumb
src/punycode.c|punycode|RFC3492|LicenseRef-RFC3492|Adam M. Costello
src/coucal|coucal|@COUCAL_COMMIT@|BSD-3-Clause|Xavier Roche
"

fail=0

# --- notices drift check ---------------------------------------------------
# A vendored path that nobody described is the failure mode worth catching:
# the SBOM would silently under-report it.
while IFS='|' read -r path name version license supplier ; do
	test -n "$path" || continue
	if test ! -e "$path" ; then
		echo "error: $path is listed as vendored but is not present" >&2
		echo "       (a submodule? try: git submodule update --init --recursive)" >&2
		fail=1
		continue
	fi
	# match on the stem, so a notices row may write "src/md5.[ch]" for a
	# single-file component rather than naming each extension
	stem="${path%.c}"
	if ! grep -qF "$stem" "$NOTICES" ; then
		echo "error: $path is vendored but not described in $NOTICES" >&2
		fail=1
	fi
done <<< "$VENDORED"

# The reverse direction: source directories under src/ that are neither
# httrack's own nor declared above.
for d in src/*/ ; do
	d="${d%/}"
	case "$d" in
	# declared above, or httrack's own source (src/proxy is proxytrack)
	src/minizip|src/coucal|src/proxy) continue ;;
	esac
	echo "warning: $d is not declared in this script; add it or ignore it here" >&2
done

if test "$fail" -ne 0 ; then
	exit 1
fi

if test "$CHECK_ONLY" -eq 1 ; then
	echo "$NOTICES covers every vendored path"
	exit 0
fi

# --- facts about this build ------------------------------------------------
VERSION=$(sed -n 's/^AC_INIT(\[httrack\], \[\([^]]*\)\].*/\1/p' configure.ac)
test -n "$VERSION" || VERSION=unknown
COMMIT=$(git rev-parse HEAD 2>/dev/null || echo unknown)
COUCAL_COMMIT=$(git -C src/coucal rev-parse HEAD 2>/dev/null || echo unknown)
TIMESTAMP=$(date -u +%Y-%m-%dT%H:%M:%SZ)

json_escape() {
	printf '%s' "$1" | sed -e 's/\\/\\\\/g' -e 's/"/\\"/g'
}

# --- linked libraries ------------------------------------------------------
# Resolve the soname to the real file, then ask that file its version: the
# soname alone gives a major ("libz.so.1") and the header may describe a
# different install than the one that got linked.
LIB=$(ls src/.libs/libhttrack.so 2>/dev/null \
	|| ls src/.libs/libhttrack.so.* 2>/dev/null | head -1)

linked_json=""
if test -n "$LIB" && command -v ldd >/dev/null 2>&1 ; then
	while read -r real ; do
		test -n "$real" || continue
		resolved=$(readlink -f "$real")
		base=$(basename "$resolved")
		ver=
		case "$base" in
		libcrypto*|libssl*)
			name=openssl
			license="Apache-2.0"
			ver=$(strings -a "$resolved" 2>/dev/null \
				| grep -oE '^OpenSSL [0-9]+\.[0-9]+\.[0-9]+[a-z]*' \
				| head -1 | cut -d' ' -f2)
			# 1.x predates the Apache-2.0 relicense
			case "$ver" in 0.*|1.*) license="OpenSSL" ;; esac
			;;
		libz.so*)
			name=zlib
			license="Zlib"
			ver=$(printf '%s' "$base" | sed -n 's/^libz\.so\.\(.*\)$/\1/p')
			case "$ver" in 1) ver= ;; esac
			;;
		libiconv*)
			name=libiconv
			license="LGPL-2.1-or-later"
			;;
		*) continue ;;
		esac
		test -n "$ver" || ver=unknown
		# one entry per library, not per soname (libssl + libcrypto)
		case "$linked_json" in *"\"name\": \"$name\""*) continue ;; esac
		linked_json="$linked_json
    {
      \"type\": \"library\",
      \"name\": \"$(json_escape "$name")\",
      \"version\": \"$(json_escape "$ver")\",
      \"scope\": \"required\",
      \"purl\": \"pkg:generic/$(json_escape "$name")@$(json_escape "$ver")\",
      \"licenses\": [ { \"license\": { \"id\": \"$(json_escape "$license")\" } } ],
      \"properties\": [
        { \"name\": \"httrack:resolvedFrom\", \"value\": \"$(json_escape "$resolved")\" }
      ]
    },"
	done < <(ldd "$LIB" 2>/dev/null \
		| grep -oE '=> /[^ ]+' | cut -d' ' -f2)
else
	echo "warning: no built library found; the SBOM will not list linked" >&2
	echo "         libraries. Run ./configure && make first." >&2
fi

# --- vendored components ---------------------------------------------------
vendored_json=""
while IFS='|' read -r path name version license supplier ; do
	test -n "$path" || continue
	version="${version/@COUCAL_COMMIT@/$COUCAL_COMMIT}"
	if test "$name" = coucal ; then
		purl="pkg:github/xroche/coucal@$version"
	else
		purl="pkg:generic/$name@$version"
	fi
	# SPDX ids go in "id"; anything else has to go in "name"
	case "$license" in
	LicenseRef-*) lic="{ \"name\": \"$(json_escape "$license")\" }" ;;
	*)            lic="{ \"id\": \"$(json_escape "$license")\" }" ;;
	esac
	vendored_json="$vendored_json
    {
      \"type\": \"library\",
      \"name\": \"$(json_escape "$name")\",
      \"version\": \"$(json_escape "$version")\",
      \"scope\": \"required\",
      \"purl\": \"$(json_escape "$purl")\",
      \"supplier\": { \"name\": \"$(json_escape "$supplier")\" },
      \"licenses\": [ { \"license\": $lic } ],
      \"properties\": [
        { \"name\": \"httrack:vendoredPath\", \"value\": \"$(json_escape "$path")\" }
      ]
    },"
done <<< "$VENDORED"

# --- emit ------------------------------------------------------------------
emit() {
	cat <<EOF
{
  "bomFormat": "CycloneDX",
  "specVersion": "1.5",
  "version": 1,
  "metadata": {
    "timestamp": "$TIMESTAMP",
    "tools": [ { "name": "generate-sbom.sh", "vendor": "httrack" } ],
    "component": {
      "type": "application",
      "name": "httrack",
      "version": "$(json_escape "$VERSION")",
      "licenses": [ { "license": { "id": "GPL-3.0-or-later" } } ],
      "properties": [
        { "name": "httrack:commit", "value": "$(json_escape "$COMMIT")" }
      ]
    }
  },
  "components": [$(printf '%s%s' "$vendored_json" "$linked_json" | sed '$ s/,$//')
  ]
}
EOF
}

if test -n "$OUTPUT" ; then
	emit > "$OUTPUT" || exit 1
	echo "wrote $OUTPUT"
else
	emit
fi
