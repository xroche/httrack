#!/bin/bash
#
# Differential for src/htsparse.c. The suite only crawls fixtures the parser
# already handles, so this is what sees the damage it cannot. --help says how to
# run it, and what follows is what the comparison keeps and why.
#
# Three signals are compared, each normalized only where two runs of one binary
# disagree:
#   - every mirrored file, byte for byte. The page footer is off (-%F '')
#     because its {date} is the only wall-clock value a mirrored page carries.
#   - hts-changes.json, written by --changes. It names each URL the parser
#     found, the file it went to and its size, so a link gained or lost shows
#     up even when no page body changed. Compared sorted, because the file
#     records completion order and adjacent fetches race.
#   - hts-log.txt, the only place a scanned-but-filtered candidate appears. The
#     corpus carries off-host <loc> entries that are never fetched and never
#     mirrored, so without this a change to the <loc> scanner would be invisible.
#
# Dropped: the hts-cache directory, because new.zip and new.lst store mtimes
# and doit.log carries its own timestamp. Stripped from the two kept text
# files: the per-line clock, the generator version, the build path, and the run
# duration and transfer rate.
#
# robots.txt comes from tests/local-server.py's own route rather than from the
# corpus, because httrack reads it from the host root only. That server also
# answers /sitemap.xml, which is why the corpus names its own
# corpus-sitemap.xml and the crawl points --sitemap-url at it.
#
# A SIGKILL that follows a SIGTERM leaves the workdir, both builds and the
# server behind, because the trap cannot run until the foreground make returns.

set -euo pipefail

progdir=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
srcdir=$(CDPATH='' cd -- "$progdir/.." && pwd)
server="$srcdir/tests/local-server.py"
corpus="$progdir/parser-corpus"
entry=/index.html

baseline=
keep=0
mode=release
jobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)

die() {
    echo "parser-diff: $*" >&2
    exit 2
}

usage() {
    cat <<'EOF'
Crawl tools/parser-corpus/ with a released httrack and with the working tree's,
then diff the two mirrors. Exit 0 when they agree, 1 when they differ, 2 on a
harness failure. The header comment of this script lists what the diff drops.

Everything is built and crawled under a temporary directory, which is removed
on exit unless --keep is given. A SIGKILL leaves it behind.

Options:
  --baseline REF  git ref to build the baseline from (default: newest X.Y.Z tag)
  --keep          keep the temporary directory, to read the two mirrors
  --self-check    the tree against itself, which must be clean, then against a
                  mutant that finds more links and one that finds fewer, each of
                  which must be reported
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
    --baseline)
        baseline=$2
        shift 2
        ;;
    --keep)
        keep=1
        shift
        ;;
    --self-check)
        mode=self-check
        shift
        ;;
    -h | --help)
        usage
        exit 0
        ;;
    *) die "unknown option $1 (try --help)" ;;
    esac
done

[ -r "$server" ] || die "no $server"
[ -d "$corpus" ] || die "no corpus directory $corpus"
command -v python3 >/dev/null || die "python3 is needed to serve the corpus"

# Two floors, because httrack exits 0 having fetched nothing and two empty
# mirrors agree, so a collapsed crawl reads exactly like a clean run. The first
# is the committed corpus's own file count, which only ever grows, so a corpus
# that lost files cannot shrink the second floor with it. The second is four
# fifths of what the corpus holds, which leaves room for the entries the corpus
# deliberately never fetches.
min_corpus=132
corpus_files=$(find "$corpus" -type f | wc -l)
[ "$corpus_files" -ge "$min_corpus" ] ||
    die "the corpus at $corpus holds $corpus_files files, under its own $min_corpus"
min_files=$((corpus_files * 4 / 5))

server_pid=
cleanup() {
    set +e
    [ -n "$server_pid" ] && kill "$server_pid" 2>/dev/null
    [ -n "$server_pid" ] && wait "$server_pid" 2>/dev/null
    if [ -n "${workdir:-}" ] && [ "$keep" = 0 ]; then
        rm -rf "$workdir"
    elif [ -n "${workdir:-}" ]; then
        echo "parser-diff: kept $workdir" >&2
    fi
}
trap 'set +e; cleanup' EXIT
trap 'exit 2' HUP INT TERM

# mktemp owns the name, so the removal above reaches only what this run made.
workdir=$(mktemp -d "${TMPDIR:-/tmp}/parser-diff.XXXXXX") || die "mktemp failed"

# Echo the newest release tag, ordered by version rather than by creation date.
newest_tag() {
    git -C "$srcdir" tag -l |
        sed -n 's/^\([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\)$/\1/p' |
        sort -t. -k1,1n -k2,2n -k3,3n |
        tail -1
}

# Copy the tree at $1 into $2, .git aside. Never build in the real checkout,
# because bootstrap writes some sixty gitignored files there and config.status
# can re-run under another session's live build directory.
copy_tree() {
    mkdir -p "$2"
    (cd "$1" && tar --exclude=.git -cf - .) | (cd "$2" && tar -xf -)
}

# Export the tree at $2 into $3, coucal included, without touching $1's refs.
export_ref() {
    local src=$1 ref=$2 dst=$3 sha mod
    git clone --quiet --shared --no-checkout "$src" "$dst" ||
        die "cannot clone $src"
    git -C "$dst" checkout --quiet "$ref" || die "no such ref: $ref"
    sha=$(git -C "$dst" rev-parse "HEAD:src/coucal" 2>/dev/null) || sha=
    mod="$(git -C "$src" rev-parse --git-common-dir)/modules/src/coucal"
    mkdir -p "$dst/src/coucal"
    if [ -n "$sha" ] && [ -d "$mod" ] &&
        git --git-dir="$mod" archive "$sha" 2>/dev/null | tar -x -C "$dst/src/coucal"; then
        :
    else
        git -C "$dst" submodule update --init src/coucal ||
            die "cannot populate src/coucal at $ref"
    fi
    # The clone shares $src's object store, but the build reads files only.
    rm -rf "$dst/.git"
}

# Build httrack from $1 into $2 and echo the binary's path.
build_tree() {
    local src=$1 bld=$2 log
    log="$bld.log"
    mkdir -p "$bld"
    # One && chain, because bash 3.2 drops errexit inside a subshell guarded by ||.
    (
        cd "$src" && bash ./bootstrap &&
            cd "$bld" && bash "$src/configure" --disable-dependency-tracking &&
            make -j"$jobs"
    ) >"$log" 2>&1 || {
        tail -30 "$log" >&2
        die "build of $src failed, see $log"
    }
    echo "$bld/src/httrack"
}

# Sets server_pid and server_port. Not a command substitution, because the pid
# has to land in the caller's shell for cleanup to reach it.
start_server() {
    local root=$1 tries=0
    server_port=
    python3 "$server" --root "$root" >"$workdir/server.log" 2>&1 &
    server_pid=$!
    while [ -z "$server_port" ] && [ "$tries" -lt 200 ]; do
        server_port=$(sed -n 's/^PORT \([0-9][0-9]*\)$/\1/p' "$workdir/server.log" | head -1)
        kill -0 "$server_pid" 2>/dev/null || break
        [ -n "$server_port" ] || sleep 0.1
        tries=$((tries + 1))
    done
    [ -n "$server_port" ] || {
        cat "$workdir/server.log" >&2
        die "the test server never announced a port"
    }
}

# SITEBASE and SITEHOSTREL only resolve once the ephemeral port is known, which
# is why the corpus is served from a copy. Reads base_url and base_host.
expand_corpus() {
    local root=$1 list f
    list="$workdir/corpus-files"
    find "$root" -type f \
        \( -name '*.html' -o -name '*.xml' -o -name '*.css' -o -name '*.js' \
        -o -name '*.txt' -o -name '*.svg' -o -name '*.vtt' \) >"$list"
    # Read from a file, not a pipe, because a sed failure in a pipeline's subshell
    # would leave a half-written page behind and still look like a success.
    while IFS= read -r f; do
        sed -e "s|SITEBASE|$base_url|g" -e "s|SITEHOSTREL|//$base_host|g" \
            "$f" >"$f.pd"
        mv "$f.pd" "$f"
    done <"$list"
}

# Crawl the running server into $workdir/mirror-$2 with the binary $1. Reads
# base_url, entry and sitemap_args, all set below.
crawl() {
    local bin=$1 leg=$2 dir
    dir="$workdir/mirror-$leg"
    rm -rf "$dir"
    mkdir -p "$dir"
    # shellcheck disable=SC2086 # sitemap_args is a deliberate split
    (cd "$dir" && "$bin" "$base_url$entry" -O . -q -%F '' --changes \
        $sitemap_args) \
        >"$workdir/crawl-$leg.out" 2>&1 || {
        tail -20 "$workdir/crawl-$leg.out" >&2
        die "the $leg crawl failed"
    }
}

# Flatten hts-changes.json into one line per entry, sorted, because two runs of
# one binary swap adjacent entries: the file records completion order, and
# adjacent fetches race. The date and generator keys go, because they carry the
# clock and the version.
changes_py='
import json, sys
d = json.load(open(sys.argv[1]))
for k in ("schema", "first_crawl", "partial", "purged"):
    print("header", k, d.get(k))
print("header counts", json.dumps(d.get("counts", {}), sort_keys=True))
for state in ("new", "changed", "unchanged", "gone"):
    rows = d.get(state) or []
    for e in sorted(rows, key=lambda e: (e.get("url", ""), e.get("file", ""))):
        print("entry", state, e.get("url"), e.get("file"), e.get("size"))
'

# Snapshot the mirror at $1 into $2, dropping what two runs of one binary
# disagree on. The header comment says what goes and why.
normalize() {
    local src=$1 dst=$2
    rm -rf "$dst"
    mkdir -p "$dst/files"
    (cd "$src" && tar -cf - .) | (cd "$dst/files" && tar -xf -)
    rm -rf "$dst/files/hts-cache"
    : >"$dst/CHANGES"
    if [ -f "$dst/files/hts-changes.json" ]; then
        python3 -c "$changes_py" "$dst/files/hts-changes.json" >"$dst/CHANGES" ||
            die "cannot read $dst/files/hts-changes.json"
        rm -f "$dst/files/hts-changes.json"
    fi
    : >"$dst/LOG"
    if [ -f "$dst/files/hts-log.txt" ]; then
        # Lines 1 and 2 are the launch stamp and the command line, which carry
        # the version and the build path. "N links scanned, N files written"
        # sits on the duration line, so the timing goes and the counts stay.
        sed -e '1,2d' \
            -e 's/^[0-9][0-9]:[0-9][0-9]:[0-9][0-9]	//' \
            -e 's|^HTTrack Website Copier/[^ ]* mirror complete in [0-9]* seconds*|mirror complete|' \
            -e 's| \[[0-9]* bytes received at [0-9]* bytes/sec\]||' \
            "$dst/files/hts-log.txt" >"$dst/LOG"
        rm -f "$dst/files/hts-log.txt"
    fi
}

# Compare the two mirrors. Returns 1 when they differ.
compare() {
    local na="$workdir/norm-$1" nb="$workdir/norm-$2" leg files fetched rc=0
    normalize "$workdir/mirror-$1" "$na"
    normalize "$workdir/mirror-$2" "$nb"
    for leg in "$1" "$2"; do
        files=$(find "$workdir/norm-$leg/files" -type f | wc -l)
        fetched=$(grep -c '^entry ' "$workdir/norm-$leg/CHANGES" || true)
        [ "$files" -ge "$min_files" ] ||
            die "the $leg mirror holds $files files, under the corpus floor of $min_files"
        [ "$fetched" -ge "$min_files" ] ||
            die "the $leg crawl fetched $fetched URLs, under the corpus floor of $min_files"
        echo "$leg: $files files, $fetched fetched"
    done
    diff -ru "$na" "$nb" >"$workdir/diff-$1-$2.txt" 2>&1 || rc=1
    if [ "$rc" = 0 ]; then
        echo "no difference: $1 and $2 mirror the corpus identically"
    else
        echo "DIFFERENCE between $1 and $2:"
        cat "$workdir/diff-$1-$2.txt"
    fi
    return "$rc"
}

docroot="$workdir/docroot"
mkdir -p "$docroot"
(cd "$corpus" && tar -cf - .) | (cd "$docroot" && tar -xf -)

start_server "$docroot"
base_host="127.0.0.1:$server_port"
base_url="http://$base_host"
expand_corpus "$docroot"
sitemap_args=
if [ -f "$docroot/corpus-sitemap.xml" ]; then
    sitemap_args="--sitemap-url $base_url/corpus-sitemap.xml"
fi
echo "parser-diff: corpus $corpus served at $base_url$entry"

# Before the builds, because two legs at one commit are one binary and print
# the same "no difference" a real pass prints.
if [ "$mode" = release ]; then
    [ -n "$baseline" ] || baseline=$(newest_tag)
    [ -n "$baseline" ] || die "no X.Y.Z tag found, pass --baseline"
    git -C "$srcdir" rev-parse --verify --quiet "$baseline^{commit}" >/dev/null ||
        die "no such ref: $baseline"
    if git -C "$srcdir" diff --quiet "$baseline" HEAD -- src &&
        git -C "$srcdir" diff --quiet HEAD -- src; then
        die "src/ is identical at $baseline and in the tree, so both legs are one binary"
    fi
fi

echo "parser-diff: building the tree under test"
copy_tree "$srcdir" "$workdir/src-tree"
tree_httrack=$(build_tree "$workdir/src-tree" "$workdir/bld-tree")
[ -r "$tree_httrack" ] || die "no tree binary at $tree_httrack"

case "$mode" in
release)
    echo "parser-diff: building the baseline at $baseline"
    export_ref "$srcdir" "$baseline" "$workdir/src-baseline"
    base_httrack=$(build_tree "$workdir/src-baseline" "$workdir/bld-baseline")
    [ -r "$base_httrack" ] || die "no baseline binary at $base_httrack"
    crawl "$base_httrack" baseline
    crawl "$tree_httrack" tree
    compare baseline tree
    ;;
self-check)
    # Each mutant edits one hts_detect row, the table deciding whether an
    # attribute is read as a link. No test under tests/ names "cite" or
    # "data-srcset", so make check stays green on both and only the corpus sees
    # them. The corpus spells the data-srcset value with a "1x" descriptor, so
    # the dirty-attribute fallback cannot take it for a path and rescue it.
    mutant_file=src/htslib.c

    crawl "$tree_httrack" treeA
    crawl "$tree_httrack" treeB
    noise_rc=0
    compare treeA treeB || noise_rc=1

    rcs=
    for direction in widen narrow; do
        echo
        echo "parser-diff: planting the $direction mutant in a copy of the tree"
        mutant="$workdir/src-$direction"
        copy_tree "$srcdir" "$mutant"
        [ -f "$mutant/$mutant_file" ] || die "no $mutant_file in $srcdir"
        # The newline is escaped, because BSD sed takes no \n in a replacement.
        case "$direction" in
        widen)
            match='  "background",'
            repl='  "background",\
  "cite",'
            ;;
        narrow)
            match='  "data-srcset",'
            repl=''
            ;;
        esac
        hits=$(grep -c "^$match" "$mutant/$mutant_file" || true)
        [ "$hits" = 1 ] ||
            die "the $direction pattern matches $hits times in $mutant_file, not once"
        sed "s|^$match.*\$|$repl|" "$mutant/$mutant_file" >"$mutant/$mutant_file.pd"
        mv "$mutant/$mutant_file.pd" "$mutant/$mutant_file"
        mutant_httrack=$(build_tree "$mutant" "$workdir/bld-$direction")
        crawl "$mutant_httrack" "$direction"
        rc=0
        compare treeA "$direction" || rc=1
        rcs="$rcs $direction=$rc"
    done

    echo
    echo "self-check: the tree against itself ... $([ "$noise_rc" = 0 ] && echo clean || echo NOISY)"
    for pair in $rcs; do
        echo "self-check: the ${pair%=*} mutant     ... $([ "${pair#*=}" = 1 ] && echo reported || echo MISSED)"
    done
    [ "$noise_rc" = 0 ] || die "the tree does not compare clean against itself"
    case "$rcs" in
    *=0*) die "a mutant went unreported" ;;
    esac
    echo "self-check: PASS"
    ;;
esac
