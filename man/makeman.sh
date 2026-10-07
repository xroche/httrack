#!/bin/sh
#
# Regenerate a man page from the program's --help and its template.
#
# Usage:
#   man/makeman.sh [-p PAGE] [BINARY] > man/PAGE.1
#
# PAGE is httrack (the default), htsserver, proxytrack or webhttrack, and
# man/PAGE.tmpl holds its fixed prose. BINARY defaults to PAGE in $PATH. Set
# SOURCE_DATE_EPOCH for a reproducible page date.
#
# A template line holding only @SYNOPSIS@, @OPTIONS@, @EXAMPLES@, @LIMITS@ or
# @FOOTER@ becomes that block (the footer is man/footer.tmpl); @DATE@ and @YEAR@
# are replaced inline.
#
# httrack's OPTIONS section is derived from --help by indentation, which is what
# makes it robust (no more prose turning into bogus options, see Debian #1061053):
#   column 0 starting with "--"  -> long option    (.IP)
#   column 0 otherwise           -> section header  (.SS)
#   1-2 leading spaces           -> option          (.IP)
#   3+ leading spaces            -> continuation / sub-value (description text)
#
# The other programs print one option per line, two spaces in, then the option
# and its description separated by two or more spaces. Their other column-0
# lines are prose for the terminal and are skipped.

set -eu

page=httrack
if [ "${1:-}" = -p ]; then
    page=$2
    shift 2
fi
bin=${1:-$page}
script_dir=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
topdir=${TOPDIR:-$(CDPATH='' cd -- "$script_dir/.." && pwd)}
readme=${README:-$topdir/README}
tmpl=$script_dir/$page.tmpl
if [ ! -r "$tmpl" ]; then
    echo "makeman.sh: no template $tmpl" >&2
    exit 1
fi

# Reproducible date when SOURCE_DATE_EPOCH is set, otherwise today.
if [ -n "${SOURCE_DATE_EPOCH:-}" ]; then
    date_str=$(LC_ALL=C date -u -d "@${SOURCE_DATE_EPOCH}" '+%d %B %Y' 2>/dev/null ||
        LC_ALL=C date -u -r "${SOURCE_DATE_EPOCH}" '+%d %B %Y')
else
    date_str=$(LC_ALL=C date '+%d %B %Y')
fi
year=${date_str##* }

# Shared by the awk programs below: roff-escape backslashes and hyphens, and
# guard body text whose leading "." or "'" roff would read as a request.
ROFF_AWK='
  function esc(s) { gsub(/\\/, "\\\\", s); gsub(/-/, "\\-", s); return s }
  function emit(s) { s = esc(s); if (substr(s, 1, 1) == "." || substr(s, 1, 1) == "\x27") s = "\\&" s; print s }'

# httrack: options classified by indentation, plus LIMITS from the README.
if [ "$page" = httrack ]; then
    help=$("$bin" --quiet --help 2>/dev/null)

    st=$(printf '%s\n' "$help" | grep -n 'General options' | head -1 | cut -d: -f1)
    en=$(printf '%s\n' "$help" | grep -nE '^example' | head -1 | cut -d: -f1)
    en2=$(printf '%s\n' "$help" | grep -nE '^HTTrack version' | tail -1 | cut -d: -f1)

    # SYNOPSIS: one "[ -x, --long ]" per option carrying a long name (skip "#" guru
    # options, as the original did).
    synopsis=$(printf '%s\n' "$help" | awk '
      $0 ~ /\(--/ && $0 !~ / #/ {
        short = $1
        if (match($0, /\(--[^ )]+/)) {
          lng = substr($0, RSTART + 3, RLENGTH - 3)
          gsub(/-/, "\\-", short); gsub(/-/, "\\-", lng)
          printf "[ \\fB\\-%s, \\-\\-%s\\fR ]\n", short, lng
        }
      }')

    # OPTIONS: indentation-driven classifier (see header comment).
    options=$(printf '%s\n' "$help" | sed -n "${st},$((en - 2))p" | awk "$ROFF_AWK"'
      /^[ \t]*$/ { next }
      {
        match($0, /^ */); ind = RLENGTH
        if (ind == 0 && substr($0, 1, 2) == "--") {        # long option
          opt = $1
          rest = $0; sub(/^[^ \t]+[ \t]+/, "", rest)
          printf ".IP %s\n", esc(opt)
          emit(rest)
        } else if (ind == 0) {                             # section header
          printf ".SS %s\n", esc($0)
        } else if (ind <= 2) {                             # option
          opt = $1
          gsub(/^\x27|\x27$/, "", opt)                     # drop quotes around tokens like %t
          rest = $0; sub(/^[ \t]+[^ \t]+[ \t]*/, "", rest)
          printf ".IP \\-%s\n", esc(opt)
          if (rest != "") emit(rest)
        } else {                                           # continuation / sub-value
          line = $0; sub(/^[ \t]+/, "", line)
          print ".br"
          emit(line)
        }
      }')

    # LIMITS: the "Engine limits" block from the README.
    limits=$(awk "$ROFF_AWK"'
      /^Engine limits/ { grab = 1; next }
      /^Advanced options/ { grab = 0 }
      grab {
        if ($0 ~ /^-/) { print ".SM"; print esc($0) }
        else if ($0 !~ /^[ \t]*$/) print esc($0)
      }' "$readme")
    example_lines=$(printf '%s\n' "$help" | sed -n "${en},$((en2 - 1))p")
else
    # The other programs: one "  option  description" row per line.
    case $page in
    # A script, run through bash because the tree may be mounted noexec.
    webhttrack) help=$(bash "$bin" --help) ;;
    *) help=$("$bin" --help) ;;
    esac
    # The programs print argv[0], which may be a build-tree path.
    help=$(printf '%s\n' "$help" | sed -E "s#^(usage|example): [^ ]*#\\1: $page#")

    synopsis=$(printf '%s\n' "$help" | awk "$ROFF_AWK"'
      /^usage: / { sub(/^usage: /, ""); if (n++) print ".br"; printf ".B %s\n", esc($0) }')

    options=$(printf '%s\n' "$help" | awk "$ROFF_AWK"'
      /^  [^ ]/ {
        line = substr($0, 3); desc = ""
        if (match(line, /   */)) { desc = substr(line, RSTART + RLENGTH); line = substr(line, 1, RSTART - 1) }
        printf ".TP\n.B %s\n", esc(line)
        if (desc != "") emit(desc)
      }')
    example_lines=$help
    limits=
fi

# EXAMPLES: "example: <cmd>" / "means: <text>" pairs.
examples=$(printf '%s\n' "$example_lines" | awk "$ROFF_AWK"'
  /^example:/ { sub(/^example:[ \t]*/, ""); s = esc($0); gsub(/"/, "\\(dq", s); printf ".TP\n.B %s\n", s; next }
  /^means:/   { sub(/^means:[ \t]*/, "");   if ($0 != "") print esc($0); next }
')

# Expand the placeholders of template $1 from the MM_* environment.
render() {
    awk '
      $0 == "@SYNOPSIS@" { print ENVIRON["MM_SYNOPSIS"]; next }
      $0 == "@OPTIONS@"  { print ENVIRON["MM_OPTIONS"]; next }
      $0 == "@EXAMPLES@" { print ENVIRON["MM_EXAMPLES"]; next }
      $0 == "@LIMITS@"   { print ENVIRON["MM_LIMITS"]; next }
      $0 == "@FOOTER@"   { print ENVIRON["MM_FOOTER"]; next }
      { gsub(/@DATE@/, ENVIRON["MM_DATE"]); gsub(/@YEAR@/, ENVIRON["MM_YEAR"]); print }' "$1"
}

export MM_DATE="$date_str" MM_YEAR="$year"
MM_FOOTER=$(render "$script_dir/footer.tmpl")
export MM_FOOTER MM_SYNOPSIS="$synopsis" MM_OPTIONS="$options" \
    MM_EXAMPLES="$examples" MM_LIMITS="$limits"
render "$tmpl"
