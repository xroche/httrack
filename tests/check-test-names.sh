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

# One number names one test. The exceptions are listed by name rather than by
# magnitude, because the tree carried no such boundary. The sub-100 duplicates
# it held were unrelated tests that happened to pick one number, which is the
# accident this rejects, so 02_update-cache and 13_crawl_proxy_https moved to
# 394 and 395 rather than being blessed. Each family below was read out of the
# history, not assumed:
#   01_engine-           the self-test bucket AGENTS.md documents, ~70 members
#   01_zlib-             "group zlib-dependent self-tests under 01_zlib-*" (#460)
#   11_crawl-            five crawl tests the original author added over 2013-2014
#   53_local-proxytrack- #701 took #572's prefix twelve days later, not a free number
#   62_lang-             one test plus the two count files it pins
#   74_local-warc-       one author, two WARC PRs half an hour apart one night,
#                        so the shared prefix was a choice and not a collision
# What this catches is a duplicate that reaches master and stays. It cannot
# catch the case that prompted it, two open PRs each picking 389, because the
# ruleset leaves strict_required_status_checks_policy off: a green lint on one
# PR says nothing about the sibling that lands before it. Hand-checking the
# open PRs at handout time is still the only thing that closes that.
families=(01_engine- 01_zlib- 11_crawl- 53_local-proxytrack- 62_lang- 74_local-warc-)

# Every extension, not just .test: the count files 62_lang-integrity.test pins
# carry a number too, so a glob of *.test alone cannot see one squatting.
# An in-tree build puts automake's own NNN_name.log and NNN_name.trs in this
# same directory, and those are output rather than a name anyone chose, so
# reading them made every test a duplicate of itself. Only the output is
# skipped: a .log with no .test of its own name is a file claiming that number,
# which is the thing being rejected here.
shopt -s nullglob
numbered=""
for f in "$testdir"/[0-9]*_*; do
    [ -f "$f" ] || continue
    case $f in *.log | *.trs) [ ! -e "${f%.*}.test" ] || continue ;; esac
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
