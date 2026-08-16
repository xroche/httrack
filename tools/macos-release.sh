#!/bin/sh
# Sign, notarize and pack macos-app.sh's bundle into a distributable DMG (#901). POSIX sh
# like its sibling. --identity - --skip-notarize runs everything but the calls to Apple.
set -eu

# shellcheck source=tools/macos-bundle.sh
. "$(dirname "$0")/macos-bundle.sh"

usage() {
    echo "usage: $0 --app DIR --identity ID [--out DIR] [--label NAME]" >&2
    echo "       [--notary-key FILE --notary-key-id ID --notary-issuer ID | --skip-notarize]" >&2
    exit 2
}

fail() {
    echo "release: $*" >&2
    exit 1
}

app=""
identity=""
out=""
label=""
notary_key=""
notary_key_id=""
notary_issuer=""
skip_notarize=0
while [ $# -gt 0 ]; do
    case "$1" in
    --app)
        app="${2:?}"
        shift 2
        ;;
    --identity)
        identity="${2:?}"
        shift 2
        ;;
    --out)
        out="${2:?}"
        shift 2
        ;;
    --label)
        label="${2:?}"
        shift 2
        ;;
    --notary-key)
        notary_key="${2:?}"
        shift 2
        ;;
    --notary-key-id)
        notary_key_id="${2:?}"
        shift 2
        ;;
    --notary-issuer)
        notary_issuer="${2:?}"
        shift 2
        ;;
    --skip-notarize)
        skip_notarize=1
        shift
        ;;
    *) usage ;;
    esac
done
test -n "$app" || usage
test -n "$identity" || usage
test -d "$app" || fail "no bundle at $app -- run make macos-app first"
app=$(cd "$app" && pwd -P)
out=${out:-$(dirname "$app")}
test -d "$out" || fail "no output directory at $out"
out=$(cd "$out" && pwd -P)
if [ "$skip_notarize" -eq 0 ]; then
    if [ -z "$notary_key" ] || [ -z "$notary_key_id" ] || [ -z "$notary_issuer" ]; then
        fail "notarization needs --notary-key, --notary-key-id and --notary-issuer"
    fi
    test -r "$notary_key" || fail "no App Store Connect key at $notary_key"
fi
for t in codesign xcrun hdiutil ditto file spctl lipo; do
    command -v "$t" >/dev/null 2>&1 ||
        fail "$t not found -- this needs the Xcode command line tools"
done

mach=$(mktemp)
nlog=$(mktemp)
trap 'rm -f "$mach" "$nlog"' EXIT

# Ad-hoc signatures cannot carry a timestamp, and Apple has nothing to countersign.
ts=""
[ "$identity" = "-" ] || ts="--timestamp"

sign() {
    # shellcheck disable=SC2086 # $ts is empty or exactly one flag
    codesign --force $ts -s "$identity" "$@"
}

# Each Mach-O by hand: --deep reaches them but never applies the hardened runtime.
machos_into "$app/Contents" "$mach"
test -s "$mach" || fail "no Mach-O file in the bundle, the signing below would prove nothing"

mainexe=$(main_executable "$app")

# Before --force, which would re-sign away the damage this is meant to catch.
while IFS= read -r bin; do
    if [ "$bin" != "$mainexe" ]; then
        codesign --verify --strict "$bin" || fail "$bin arrived with a broken signature"
    fi
done <"$mach"
codesign --verify --strict "$app" || fail "$app arrived with a broken signature"

# Inside out, and never the main executable on its own: signing the bundle is what
# signs it, and doing it early would seal resources the rest of this loop rewrites.
while IFS= read -r bin; do
    if [ "$bin" != "$mainexe" ]; then
        sign --options runtime "$bin"
    fi
done <"$mach"
sign --options runtime "$app"
codesign --verify --deep --strict --verbose=2 "$app"

notarize() {
    st=0
    xcrun notarytool submit "$1" --key "$notary_key" --key-id "$notary_key_id" \
        --issuer "$notary_issuer" --wait --timeout 2h >"$nlog" 2>&1 || st=$?
    cat "$nlog"
    # A rejected submission has exited 0 in the past, so read the verdict, not just $?.
    if [ "$st" -ne 0 ] || ! grep -q "status: Accepted" "$nlog"; then
        id=$(sed -n 's/^ *id: \([0-9a-fA-F][0-9a-fA-F-]*\)$/\1/p' "$nlog" | head -1)
        # The rejection reason lives only in this log.
        [ -z "$id" ] || xcrun notarytool log "$id" --key "$notary_key" \
            --key-id "$notary_key_id" --issuer "$notary_issuer" >&2 || true
        fail "notarization of $1 was not accepted"
    fi
}

if [ "$skip_notarize" -eq 0 ]; then
    zip="$out/$(basename "$app" .app).zip"
    ditto -c -k --keepParent "$app" "$zip"
    notarize "$zip"
    rm -f "$zip"
    # Stapled, so a copy dragged out of the DMG still launches on a machine that is offline.
    xcrun stapler staple "$app"
fi

version=$(plist_value "$app/Contents/Info.plist" CFBundleShortVersionString)
test -n "$version" || fail "no CFBundleShortVersionString in $app/Contents/Info.plist"

# --label names the download for a product channel (a 3.50 beta) whose number is not the
# engine's. It renames the DMG only: the bundle keeps the version its payload reports, so
# macos-app.sh's plist-vs-binary guard still holds.
case "$label" in
*[!A-Za-z0-9._-]*) fail "--label $label is not usable in a filename" ;;
esac
name=${label:-$version}

# The filename is what a user reads before downloading, so name the arch (#1083).
# An arch counts only if every Mach-O carries it: one thin dylib and the app is not universal.
arch=$(while IFS= read -r _bin; do lipo -archs "$_bin" | tr ' ' '\n'; done <"$mach" |
    grep -v '^$' | sort | uniq -c |
    awk -v n="$(wc -l <"$mach")" '$1 == n { print $2 }' | sort | paste -sd- -)
test -n "$arch" || fail "the bundle's Mach-O files share no architecture"
case "$arch" in
*-*) arch=universal ;;
esac

dmg="$out/HTTrack-$name-macos-$arch.dmg"
stage=$(mktemp -d)
trap 'rm -f "$mach" "$nlog"; rm -rf "$stage"' EXIT
# ditto, not cp: it carries a bundle's metadata across intact.
ditto "$app" "$stage/$(basename "$app")"
ln -s /Applications "$stage/Applications"
rm -f "$dmg"
hdiutil create -volname "HTTrack $name" -srcfolder "$stage" -fs HFS+ -format UDZO "$dmg"
sign "$dmg"

if [ "$skip_notarize" -eq 0 ]; then
    notarize "$dmg"
    xcrun stapler staple "$dmg"
    xcrun stapler validate "$app"
    xcrun stapler validate "$dmg"
    # A merely valid Developer ID signature also assesses, so match the source too.
    spctl --assess --type exec -vv "$app" >"$nlog" 2>&1 || {
        cat "$nlog" >&2
        fail "Gatekeeper rejected $app"
    }
    cat "$nlog"
    grep -q "source=Notarized Developer ID" "$nlog" ||
        fail "$app assessed, but not as a notarized Developer ID app"
    spctl --assess --type open --context context:primary-signature -vv "$dmg" >"$nlog" 2>&1 || {
        cat "$nlog" >&2
        fail "Gatekeeper rejected $dmg"
    }
    cat "$nlog"
fi

echo "built $dmg (version $version)"
