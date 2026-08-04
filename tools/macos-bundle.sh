# shellcheck shell=sh
# Sourced by macos-app.sh and macos-release.sh. One answer to "which files in this
# bundle are code" and "what does its Info.plist say", so the assembler and the signer
# can never disagree about either.

# Every Mach-O under $1, one per line, into $2.
machos_into() {
    _mb_list=$(mktemp)
    find "$1" -type f >"$_mb_list"
    : >"$2"
    while IFS= read -r _mb_f; do
        # -b, and its own status: `file | grep` matches the path too, and silently drops
        # a Mach-O that file could not read.
        _mb_desc=$(file -b "$_mb_f")
        case "$_mb_desc" in *Mach-O*) printf '%s\n' "$_mb_f" >>"$2" ;; esac
    done <"$_mb_list"
    rm -f "$_mb_list"
}

# The string value of key $2 in the plist $1, empty if absent.
plist_value() {
    awk -v k="$2" '$0 ~ "<key>" k "</key>" {
        getline; gsub(/^[^>]*>|<[^<]*$/, ""); print; exit }' "$1"
}

# Rewrite the string value of key $2 in the plist $1 to $3. A missing key is silent,
# so callers check the result rather than trust it.
plist_set() {
    _mb_tmp=$(mktemp)
    awk -v k="$2" -v v="$3" '
        { if (_p) { sub(/>[^<]*</, ">" v "<"); _p = 0 } print }
        $0 ~ "<key>" k "</key>" { _p = 1 }' "$1" >"$_mb_tmp"
    cat "$_mb_tmp" >"$1"
    rm -f "$_mb_tmp"
}

# The oldest macOS every Mach-O listed in the file $1 can load on.
macho_floor() {
    _mb_floor=
    while IFS= read -r _mb_bin; do
        for _mb_v in $(otool -l "$_mb_bin" | awk '
            $1 == "cmd" { c = $2 }
            (c == "LC_BUILD_VERSION" && $1 == "minos") ||
            (c == "LC_VERSION_MIN_MACOSX" && $1 == "version") { print $2 }'); do
            _mb_floor=$(printf '%s\n%s\n' "$_mb_floor" "$_mb_v" |
                sort -t. -k1,1n -k2,2n -k3,3n | tail -1)
        done
    done <"$1"
    printf '%s\n' "$_mb_floor"
}

# The bundle's main executable. codesign resolves it to the enclosing bundle, so it
# is signed and verified with the bundle, never on its own.
main_executable() {
    printf '%s/Contents/MacOS/%s\n' "$1" \
        "$(plist_value "$1/Contents/Info.plist" CFBundleExecutable)"
}
