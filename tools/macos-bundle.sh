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
