#!/bin/bash
#
# Differential for src/htsparse.c: crawl a corpus with a released httrack and
# with the working tree's, then diff the mirrors. The suite only crawls fixtures
# the parser already handles, so this is what sees the damage it cannot.
#
# Run --self-check first: it proves the diff can see a planted parser change.
# --help lists the options. Two signals are compared, both built by
# tools/parser-diff-normalize.sh: every mirrored file byte for byte, and the
# fetch manifest (hts-cache/new.txt), which names each URL the parser found and
# the file it was saved to, so a link gained or lost shows up even when no page
# body changed.
#
# Excluded from the diff, because each varies between two runs of one binary:
#   - hts-cache/new.zip and new.lst: the zip envelope stores mtimes
#   - hts-cache/doit.log: carries its own generation timestamp
#   - hts-log.txt: timestamps and durations
#   - the mirrored-page footer, switched off with -%F '' on both legs: its {date}
#     is wall-clock. The version it prints is HTTRACK_AFF_VERSION, "3.x", which
#     does not move between releases, so only the date forced this.
#
# robots.txt comes from tests/local-server.py's own route, not from the corpus:
# httrack reads it from the host root only, so a corpus copy would be dead. That
# server also answers /sitemap.xml, which is why the corpus names its own
# corpus-sitemap.xml and the crawl points --sitemap-url at it.

set -euo pipefail

progdir=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
srcdir=$(CDPATH='' cd -- "$progdir/.." && pwd)
server="$srcdir/tests/local-server.py"
normalize="$progdir/parser-diff-normalize.sh"

baseline=
baseline_bin=
tree="$srcdir"
tree_bin=
corpus="$progdir/parser-corpus"
entry=/index.html
workdir=
keep=0
jobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)
extra_args=
mode=release

# The mutant the self-check plants: srcset stops being split into candidates,
# so `srcset="a.gif 1x, b.gif 2x"` is taken as one bogus URL.
mutant_file=src/htsparse.c
mutant_match='srcset_p[ ]*=[ ]*1;'
mutant_repl='srcset_p = 0;'

die() {
    echo "parser-diff: $*" >&2
    exit 2
}

usage() {
    cat <<'EOF'
Crawl a corpus with a released httrack and with the working tree's, then diff
the two mirrors. Exit 0 when they agree, 1 when they differ, 2 on a harness
failure. The header comment of this script lists what the diff excludes.

Options:
  --baseline REF        git ref to build the baseline from (default: newest X.Y.Z tag)
  --baseline-bin PATH   use an already-built httrack as the baseline
  --tree DIR            source tree under test (default: this script's tree)
  --tree-bin PATH       use an already-built httrack as the tree under test
  --corpus DIR          docroot to crawl (default: tools/parser-corpus)
  --entry PATH          entry path on the server (default: /index.html)
  --httrack-args 'ARGS' extra arguments appended to both crawls
  --workdir DIR         where to build and crawl (default: a temporary directory)
  --keep                keep the workdir
  --jobs N              make -j width
  --self-noise          run the tree's binary as both legs; must report no change
  --self-check          --self-noise, then the tree against a mutant of itself;
                        passes only when the first is clean and the second is not
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
    --baseline)
        baseline=$2
        shift 2
        ;;
    --baseline-bin)
        baseline_bin=$2
        shift 2
        ;;
    --tree)
        tree=$2
        shift 2
        ;;
    --tree-bin)
        tree_bin=$2
        shift 2
        ;;
    --corpus)
        corpus=$2
        shift 2
        ;;
    --entry)
        entry=$2
        shift 2
        ;;
    --httrack-args)
        extra_args=$2
        shift 2
        ;;
    --workdir)
        workdir=$2
        shift 2
        ;;
    --keep)
        keep=1
        shift
        ;;
    --jobs)
        jobs=$2
        shift 2
        ;;
    --self-noise)
        mode=self-noise
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
[ -r "$normalize" ] || die "no $normalize"
[ -d "$corpus" ] || die "no corpus directory $corpus"
command -v python3 >/dev/null || die "python3 is needed to serve the corpus"

server_pid=
server_port=
cleanup() {
    set +e
    [ -n "$server_pid" ] && kill "$server_pid" 2>/dev/null
    [ -n "$server_pid" ] && wait "$server_pid" 2>/dev/null
    if [ -n "$workdir" ] && [ "$keep" = 0 ]; then
        rm -rf "$workdir"
    elif [ -n "$workdir" ]; then
        echo "parser-diff: kept $workdir" >&2
    fi
}
trap 'set +e; cleanup' EXIT
trap 'exit 2' HUP INT TERM

if [ -z "$workdir" ]; then
    workdir=$(mktemp -d "${TMPDIR:-/tmp}/parser-diff.XXXXXX") || die "mktemp failed"
else
    mkdir -p "$workdir"
    workdir=$(CDPATH='' cd -- "$workdir" && pwd)
fi

# Newest release tag, by version order rather than by creation date.
newest_tag() {
    git -C "$tree" tag -l |
        sed -n 's/^\([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\)$/\1/p' |
        sort -t. -k1,1n -k2,2n -k3,3n |
        tail -1
}

# Build httrack from $1 into $2 and echo the binary's path.
build_tree() {
    local src=$1 bld=$2 log
    log="$bld.log"
    mkdir -p "$bld"
    # One && chain: bash 3.2 drops errexit inside a subshell guarded by ||.
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
    # The clone shares $src's object store; the build reads files only.
    rm -rf "$dst/.git"
}

# Crawl the running server into $workdir/mirror-$2 with the binary $1.
crawl() {
    local bin=$1 leg=$2 dir
    dir="$workdir/mirror-$leg"
    rm -rf "$dir"
    mkdir -p "$dir"
    # shellcheck disable=SC2086 # extra_args and sitemap_args are deliberate splits
    (cd "$dir" && "$bin" "$base_url$entry" -O . -q -%F '' \
        $sitemap_args $extra_args) \
        >"$workdir/crawl-$leg.out" 2>&1 || {
        tail -20 "$workdir/crawl-$leg.out" >&2
        die "the $leg crawl failed"
    }
}

# Sets server_pid and server_port. Not a command substitution: the pid has to
# land in the caller's shell for cleanup to reach it.
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
# is why the corpus is served from a copy.
expand_corpus() {
    local root=$1 list f
    list="$workdir/corpus-files"
    find "$root" -type f \
        \( -name '*.html' -o -name '*.xml' -o -name '*.css' -o -name '*.js' \
        -o -name '*.txt' -o -name '*.svg' -o -name '*.vtt' \) >"$list"
    # Read from a file, not a pipe: a sed failure in a pipeline's subshell would
    # leave a half-written page behind and still look like a success.
    while IFS= read -r f; do
        sed -e "s|SITEBASE|$base_url|g" -e "s|SITEHOSTREL|//$base_host|g" \
            "$f" >"$f.pd"
        mv "$f.pd" "$f"
    done <"$list"
}

# Compare the two mirrors. Returns 1 when they differ.
compare() {
    local a="$workdir/mirror-$1" b="$workdir/mirror-$2" na nb rc=0
    na="$workdir/norm-$1"
    nb="$workdir/norm-$2"
    bash "$normalize" "$a" "$na"
    bash "$normalize" "$b" "$nb"
    diff -ru "$na" "$nb" >"$workdir/diff-$1-$2.txt" 2>&1 || rc=1
    # Print the sizes: a crawl that fetched almost nothing agrees with itself too.
    echo "$1: $(find "$na/files" -type f | wc -l) files, $(wc -l <"$na/MANIFEST") fetched"
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
# A corpus naming no sitemap gets no --sitemap-url: the suite's server answers
# /sitemap.xml itself, so the corpus cannot use that name.
sitemap_args=
if [ -f "$docroot/corpus-sitemap.xml" ]; then
    sitemap_args="--sitemap-url $base_url/corpus-sitemap.xml"
fi
echo "parser-diff: corpus $corpus served at $base_url$entry"

if [ -n "$tree_bin" ]; then
    tree_httrack=$tree_bin
else
    echo "parser-diff: building the tree under test"
    tree_httrack=$(build_tree "$tree" "$workdir/bld-tree")
fi
[ -r "$tree_httrack" ] || die "no tree binary at $tree_httrack"

case "$mode" in
self-noise | self-check)
    crawl "$tree_httrack" treeA
    crawl "$tree_httrack" treeB
    noise_rc=0
    compare treeA treeB || noise_rc=1
    if [ "$mode" = self-noise ]; then
        exit "$noise_rc"
    fi
    ;;
esac

case "$mode" in
release)
    if [ -n "$baseline_bin" ]; then
        base_httrack=$baseline_bin
    else
        [ -n "$baseline" ] || baseline=$(newest_tag)
        [ -n "$baseline" ] || die "no X.Y.Z tag found, pass --baseline"
        echo "parser-diff: building the baseline at $baseline"
        export_ref "$tree" "$baseline" "$workdir/src-baseline"
        base_httrack=$(build_tree "$workdir/src-baseline" "$workdir/bld-baseline")
    fi
    [ -r "$base_httrack" ] || die "no baseline binary at $base_httrack"
    crawl "$base_httrack" baseline
    crawl "$tree_httrack" tree
    compare baseline tree
    ;;
self-check)
    echo "parser-diff: planting the mutant in a copy of the tree"
    mutant="$workdir/src-mutant"
    mkdir -p "$mutant"
    (cd "$tree" && tar --exclude=.git -cf - .) | (cd "$mutant" && tar -xf -)
    [ -f "$mutant/$mutant_file" ] || die "no $mutant_file in $tree"
    hits=$(grep -c "$mutant_match" "$mutant/$mutant_file" || true)
    [ "$hits" = 1 ] ||
        die "the mutant pattern $mutant_match matches $hits times in $mutant_file, not once"
    sed "s|$mutant_match|$mutant_repl|" "$mutant/$mutant_file" >"$mutant/$mutant_file.pd"
    mv "$mutant/$mutant_file.pd" "$mutant/$mutant_file"
    mutant_httrack=$(build_tree "$mutant" "$workdir/bld-mutant")
    crawl "$mutant_httrack" mutant
    mutant_rc=0
    compare treeA mutant || mutant_rc=1
    echo
    echo "self-check: unmutated tree against itself ... $([ "$noise_rc" = 0 ] && echo clean || echo NOISY)"
    echo "self-check: mutated tree against the tree   ... $([ "$mutant_rc" = 1 ] && echo reported || echo MISSED)"
    if [ "$noise_rc" != 0 ] || [ "$mutant_rc" != 1 ]; then
        die "the control did not fire both ways"
    fi
    echo "self-check: PASS"
    ;;
esac
