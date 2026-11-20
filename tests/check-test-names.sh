#!/bin/bash
#
# configure globs [0-9]*_*.test into TESTS, so a file named anything else is
# silently never run (#1037); a directory or dangling symlink chokes the driver.
# Only that exact spelling runs, so a parked 00_x.test.in or 00_x.test.disabled
# stays deliberately invisible to configure and to this check alike.

set -euo pipefail
export LC_ALL=C

testdir=$(cd "${1:-$(dirname "$0")}" && pwd)

status=0
bad() {
    echo "$*" >&2
    status=1
}

present=""
count=0
# nocaseglob so a 01_Engine.TEST, which configure would never pick up, is caught
# here rather than by nobody.
shopt -s nocaseglob nullglob
for f in "$testdir"/*.test; do
    name=${f##*/}
    if [ ! -f "$f" ]; then
        bad "not a regular file, so make check would choke on it: $name"
    elif [[ $name == *[[:space:]]* ]]; then
        # configure joins the basenames with spaces, so make sees two words.
        bad "whitespace in the name, which make would split in two: $name"
    elif [[ $name != [0-9]*_*.test ]]; then
        bad "outside the [0-9]*_*.test glob, so make check never runs it: $name"
    else
        present="${present}${name}"$'\n'
        count=$((count + 1))
    fi
done
shopt -u nocaseglob nullglob

# Floor: an empty tests/ would satisfy every check above.
[ "$count" -gt 0 ] || bad "no test matched [0-9]*_*.test under $testdir"

# One number names one test. Exceptions are listed in `families`, each a
# deliberate shared prefix, for example 01_zlib- (#460).
# Two open PRs can still share a number, which this catches only on master.
families=(01_engine- 01_zlib- 11_crawl- 53_local-proxytrack- 62_lang- 74_local-warc-)

# Any file claims its number, unless it is build output beside a same-stem test.
shopt -s nullglob
numbered=""
for f in "$testdir"/[0-9]*_*; do
    [ -f "$f" ] || continue
    case $f in *.test) ;; *) [ ! -e "${f%.*}.test" ] || continue ;; esac
    name=${f##*/}
    digits=${name%%_*}
    case $digits in *[!0-9]*) continue ;; esac
    exempt=no
    for fam in "${families[@]}"; do
        case $name in "$fam"*)
            exempt=yes
            break
            ;;
        esac
    done
    # 10# so 0389_a and 389_b are one number rather than two strings.
    numbered="${numbered}$((10#$digits)) $exempt $name"$'\n'
done
shopt -u nullglob

for n in $(printf '%s' "$numbered" | cut -d' ' -f1 | sort -n | uniq -d); do
    group=$(printf '%s' "$numbered" | awk -v n="$n" '$1 == n')
    # A family may share its number; anything else in there is squatting on it.
    if [ -n "$(awk '$2 != "yes"' <<<"$group")" ]; then
        bad "number $n is claimed by more than one test: $(
            awk '{printf "%s ", $3}' <<<"$group"
        )"
    fi
done

# The glob carries no oracle of its own: a test its author forgot to "git add",
# or one missing from a checkout, just makes the suite quietly smaller. Nothing
# tracked here means a tarball, where there is no oracle to apply.
# A git that cannot answer is not the same as an empty listing, so say which.
if ! command -v git >/dev/null 2>&1 || ! git -C "$testdir" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    tracked=""
    echo "note: no git checkout here that git can read, skipping the tracked-set check" >&2
else
    tracked=$(git -C "$testdir" ls-files -- '[0-9]*_*.test')
fi
if [ -n "$tracked" ]; then
    untracked=$(comm -13 <(sort <<<"$tracked") <(printf '%s' "$present" | sort))
    [ -z "$untracked" ] || bad "untracked, so CI would run a smaller suite than you do: $untracked"
    gone=$(comm -23 <(sort <<<"$tracked") <(printf '%s' "$present" | sort))
    [ -z "$gone" ] || bad "tracked but absent, so the suite is short of them: $gone"
fi

if [ "$status" -eq 0 ]; then
    echo "test names: $count files, each one picked up by the make check glob"
fi
exit "$status"
