#!/bin/bash
#
# Launcher for httrack crawl tests against the local Python test server.
#
# Starts tests/local-server.py on an ephemeral port, discovers the port from
# the server's stdout, then runs httrack against http(s)://127.0.0.1:$PORT and
# audits the mirror. The server is always killed and the tmpdir removed on exit.
#
# The token BASEURL in any httrack argument is replaced with the discovered
# http(s)://127.0.0.1:$PORT base, and BASEHOST with the bare 127.0.0.1:$PORT. --found/--directory paths are relative to the
# discovered host root (127.0.0.1_<port>/), since the random port leaks into
# the mirror directory name.
#
# Usage:
#   bash local-crawl.sh [--tls] [--root DIR] [--cookie NAME=VALUE ...] \
#       [--rerun-args 'ARGS'] \
#       --errors N --errors-content N --files N --found PATH ... --directory PATH ... \
#       --log-found REGEX ... --log-not-found REGEX ... \
#       --file-matches PATH REGEX ... --file-not-matches PATH REGEX ... \
#       --cache-found URLTAIL ... --cache-not-found URLTAIL ... \
#       --file-min-bytes PATH N --file-mode PATH OCTAL --max-mirror-bytes N \
#       httrack BASEURL/some/path [httrack-args...]
# --errors counts every "Error:" log line; --errors-content drops transient
# network failures (codes -2..-6) that flake on busy loopback under -c8.
# --log-found/--log-not-found grep (ERE) the crawl's hts-log.txt.
# --max/--min-mirror-bytes bound the mirrored content bytes (host root).
# --file-matches/--file-not-matches grep (ERE) a mirrored file (PATH under the
# host root), to assert rewritten link/content survived the crawl.
# --cache-found/--cache-not-found assert whether hts-cache/new.zip holds an
# entry whose URL ends with URLTAIL, e.g. /dir/page.html; being mirrored and
# being cached are separate outcomes (#840).
# --file-min-bytes asserts a mirrored file (PATH) is at least N bytes.
# --file-mode asserts its octal permissions (e.g. 644); POSIX hosts only.
# --rerun-args runs a second pass (same server and mirror dir) with the given
# extra httrack args appended, e.g. an --update run under a cap.
# --cookie writes a Netscape cookies.txt (scoped to the discovered host:port,
# which the ephemeral port forces into the cookie domain) and passes it to
# httrack via --cookies-file, to exercise preloaded cookies.
# --rerun-dead re-runs with the server stopped: the no-data rollback must
# restore the previous hts-cache generation byte-identical.
# --archive-kept-on-rerun: the second pass must leave the first pass's
# .warc[.gz]/.cdx/.wacz byte-identical, having no bodies to replace them (#759).
# --archive-replaced-on-rerun is its mirror: every one of them is poisoned
# between the passes and no poison may survive, since a repeat crawl can
# reproduce a .cdx exactly and byte comparison then reads it as stale (#1041).
# Both also require no *.tmp left behind, and take an optional
# --archive-min-files N guarding against a scenario that silently stopped
# producing the segments it means to check.
# --plant-file/--plant-dir drop a regular file (holding $plant_poison) or a
# directory at PATH under the host root between the passes, to hand the second
# pass leftovers a killed run would have left (#758).

set -u

# shellcheck source=tests/crawllib.sh
. "${0%"${0##*/}"}./crawllib.sh"
root="${LOCAL_SERVER_ROOT:-${testdir}/server-root}"

tlsargs=()
verbose=
warc_validate=
wacz_validate=
html_subdir=
outdir_intl=
rerun=
rerun_args=
rerun_dead=
archive_kept=
archive_replaced=
archive_min_files=0
tmpdir=
crawlpid=
archive_poison="stale-archive-that-a-second-pass-must-replace"
plant_poison="stale-leftover-that-a-second-pass-must-clobber"

function warning {
    echo "** $*" >&2
    return 0
}
function die {
    warning "$*"
    exit 1
}
function debug {
    test -n "$verbose" && echo "$*" >&2
    return 0
}
function info { printf "[%s] ..\t" "$*" >&2; }
function result { echo "$*" >&2; }

# Run in reverse: the crawl dies first, then the server, then the tmpdir.
# Functions, not arguments: cleanup_push expands its arguments at push time.
function kill_crawl {
    test -n "$crawlpid" || return 0
    kill -9 "$crawlpid" 2>/dev/null
    crawlpid=
}
function purge_tmpdir {
    test -z "$nopurge" && test -n "$tmpdir" && test -d "$tmpdir" || return 0
    rm -rf "$tmpdir"
}

hostroot=
function find_hostroot {
    local cand
    for cand in "${mirrorroot}/127.0.0.1_${port}" "${mirrorroot}/127.0.0.1"; do
        if test -d "$cand"; then
            hostroot="$cand"
            return 0
        fi
    done
    die "could not find host root under $out"
}

# Does the cache hold an entry whose URL ends with $1? An unreadable index is a
# hard failure, else --cache-not-found would pass on a cache that never existed.
# Suffix match over the whole key, so a URL with a query string needs the query
# spelled out; a bare path would silently match nothing.
function cache_has {
    local rc
    "$python" -c '
import sys, zipfile
try:
    names = zipfile.ZipFile(sys.argv[1]).namelist()
except Exception:
    sys.exit(2)
sys.exit(0 if any(n.endswith(sys.argv[2]) for n in names) else 1)
' "${logroot}/hts-cache/new.zip" "$1"
    rc=$?
    test "$rc" -le 1 || die "cannot read cache index ${logroot}/hts-cache/new.zip"
    return "$rc"
}

function assert_equals {
    info "$1"
    if test ! "$2" == "$3"; then
        result "expected '$2', got '$3'"
        exit 1
    fi
    result "OK ($2)"
}

nopurge=
cleanup_push purge_tmpdir

# Line 2 echoes the engine's command line, so a path named after a searched word
# would answer for the crawl (#1220). The banner stays: audits match its URL.
# A missing log would make every --log-not-found pass on an empty file.
log_body() {
    test -s "${logroot}/hts-log.txt" || die "no crawl log at ${logroot}/hts-log.txt"
    awk 'NR == 2 && /^\(/ { next } { print }' "${logroot}/hts-log.txt"
}

# python3 is required; mirror check-network.sh's skip-with-77 convention. Found
# here, not in local_server_start, so a host without it skips before any setup.
python=$(find_python) || ! echo "python3 not found; skipping local crawl tests" >&2 || exit 77
SRV_PYTHON=$python

tmptopdir=${TMPDIR:-/tmp}
test -d "$tmptopdir" || mkdir -p "$tmptopdir" || die "no temporary directory; set TMPDIR"
tmpdir=$(mktemp -d "${tmptopdir}/httrack_local.XXXXXX") || die "could not create tmpdir"
logbody="${tmpdir}/hts-log-body.txt"

# --- parse leading control flags --------------------------------------------
declare -a audit=()
declare -a cookies=()
declare -a plants=()
pos=0
args=("$@")
nargs=$#
while test "$pos" -lt "$nargs"; do
    case "${args[$pos]}" in
    --debug) verbose=1 ;;
    --rerun) rerun=1 ;;           # run httrack a second time (update pass) before auditing
    --rerun-dead) rerun_dead=1 ;; # re-run with the server stopped (cache rollback)
    # the second pass must leave the first pass's archive files untouched
    --archive-kept-on-rerun) archive_kept=1 ;;
    --archive-replaced-on-rerun) archive_replaced=1 ;; # ...or rewrite all of them
    --archive-min-files)
        pos=$((pos + 1))
        archive_min_files="${args[$pos]}"
        ;;
    # validate the produced .warc.gz (see the validation block near the end)
    --warc-validate) warc_validate=1 ;;
    # validate the produced .wacz package (stdlib, plus py-wacz/pywb if present)
    --wacz-validate) wacz_validate=1 ;;
    --no-purge)
        nopurge=1
        audit+=("--no-purge")
        ;;
    --cache-under-logroot)
        audit+=("--cache-under-logroot")
        ;;
    --tls) tlsargs=(--tls) ;;
    --root)
        pos=$((pos + 1))
        root="${args[$pos]}"
        ;;
    --cookie)
        pos=$((pos + 1))
        cookies+=("${args[$pos]}")
        ;;
    --plant-file | --plant-dir)
        plants+=("${args[$pos]}" "${args[$((pos + 1))]}")
        pos=$((pos + 1))
        ;;
    --rerun-args)
        pos=$((pos + 1))
        rerun_args="${args[$pos]}"
        ;;
    --html-subdir)
        # Mirror into "$out/NAME" (path_html) but keep logs/cache in "$out"
        # (path_log): a non-ASCII NAME exercises the -O path encoding (#621)
        # without routing the harness-read hts-log.txt through a non-ASCII path.
        pos=$((pos + 1))
        html_subdir="${args[$pos]}"
        ;;
    --outdir-intl)
        # Single non-ASCII -O "$out/NAME": path_html AND path_log are NAME, so
        # the logs (and the harness reads of them) go through the non-ASCII path
        # (#630). Distinct from --html-subdir, which keeps path_log ASCII.
        pos=$((pos + 1))
        outdir_intl="${args[$pos]}"
        ;;
    --errors | --errors-content | --files)
        audit+=("${args[$pos]}" "${args[$((pos + 1))]}")
        pos=$((pos + 1))
        ;;
    --found | --not-found | --directory | --log-found | --log-not-found | --max-mirror-bytes | --min-mirror-bytes | --cache-found | --cache-not-found)
        audit+=("${args[$pos]}" "${args[$((pos + 1))]}")
        pos=$((pos + 1))
        ;;
    --file-matches | --file-not-matches | --file-min-bytes | --file-mode)
        audit+=("${args[$pos]}" "${args[$((pos + 1))]}" "${args[$((pos + 2))]}")
        pos=$((pos + 2))
        ;;
    httrack)
        pos=$((pos + 1))
        break
        ;;
    *) die "unrecognized option ${args[$pos]}" ;;
    esac
    pos=$((pos + 1))
done

# --- start the server --------------------------------------------------------
# local_server_start reaps the server and reports one that never announced its
# port. 72 and 105 skip on that wording, which discover_server_port writes.
local_server_start ${tlsargs[@]+"${tlsargs[@]}"} --root "$root"
port=$SRV_PORT
baseurl=$BASEURL
debug "server listening on $baseurl"

# --- substitute BASEURL/BASEHOST in the remaining (httrack) args -------------
declare -a hts=()
while test "$pos" -lt "$nargs"; do
    arg="${args[$pos]//BASEURL/$baseurl}"
    hts+=("${arg//BASEHOST/127.0.0.1:$port}")
    pos=$((pos + 1))
done

# --- materialize any --cookie entries into a cookies.txt ---------------------
if test "${#cookies[@]}" -gt 0; then
    jar="${tmpdir}/cookies.txt"
    : >"$jar"
    for spec in "${cookies[@]}"; do
        printf '127.0.0.1:%s\tTRUE\t/\tFALSE\t1999999999\t%s\t%s\n' \
            "$port" "${spec%%=*}" "${spec#*=}" >>"$jar"
    done
    hts+=(--cookies-file "$jar")
fi

# --- run httrack -------------------------------------------------------------
command -v httrack >/dev/null || die "could not find httrack"
ver=$(httrack -O /dev/null --version | sed -e 's/HTTrack version //')
test -n "$ver" || die "could not run httrack"

out="${tmpdir}/crawl"
mkdir "$out" || die "could not create $out"
# path_html holds the mirror + index; path_log holds hts-cache/hts-log.txt.
# Default: both are "$out". --html-subdir moves path_html to "$out/NAME" while
# path_log (logroot) stays "$out"; --outdir-intl moves both to "$out/NAME".
mirrorroot="$out"
logroot="$out"
odir="$out"
if test -n "$html_subdir"; then
    mirrorroot="${out}/${html_subdir}"
    odir="${mirrorroot},${out}"
elif test -n "$outdir_intl"; then
    mirrorroot="${out}/${outdir_intl}"
    logroot="$mirrorroot"
    odir="$mirrorroot"
fi
# Localhost is fast; disable the rate/bandwidth safety limits but keep the
# engine cap so a hang cannot wedge the suite.
declare -a moreargs=(--quiet "--max-time=$CRAWL_MAX_TIME" --timeout=30 --disable-security-limits --robots=0)
log="${tmpdir}/log"

# One pass: $1 its log, $2 its name in the diagnostics, the rest extra httrack
# arguments. Backgrounded here, not through local_crawl, whose own process group
# would hide the engine from the suite watchdog's stack dump (105).
function run_pass {
    local passlog=$1 label=$2 deadline rc=0
    shift 2
    deadline=$(crawl_deadline)
    httrack -O "$odir" --user-agent="httrack $ver local ($(uname -mrs))" \
        "${moreargs[@]}" "${hts[@]}" "$@" >"$passlog" 2>&1 &
    crawlpid=$!
    wait_bounded "$crawlpid" "$deadline" || rc=$?
    crawlpid=
    test "$rc" -ne 124 || warning "${label} watchdog fired after ${deadline}s"
    return "$rc"
}

# A pass that has to succeed: same arguments, but a failure ends the run with
# the engine's log, the only record of why it stopped.
function require_pass {
    local passlog=$1 label=$2 rc=0
    run_pass "$@" || rc=$?
    test "$rc" -eq 0 && return 0
    result "$label exited $rc"
    cat "$passlog" >&2
    exit 1
}

cleanup_push kill_crawl
info "running httrack ${hts[*]}"
# httrack exits 0 even on hard connect/DNS errors, so this is a backstop only;
# the real guard is the audit below (--errors 0 plus the host-root existence check).
require_pass "$log" crawl
result "OK"
grep -iE "^[0-9:]*[[:space:]]Error:" "${logroot}/hts-log.txt" >&2

# Snapshot the first-pass WARC before an update pass overwrites it: the fresh
# crawl carries the full response bodies, the update pass only revisits.
if test -n "$warc_validate"; then
    w1=$(find "$mirrorroot" -maxdepth 2 -name '*.warc.gz' 2>/dev/null | sort | tail -n1)
    test -z "$w1" || cp "$w1" "${tmpdir}/warc-pass1.gz"
fi

# Snapshot the archive files the second pass must keep (or must replace).
declare -a archive_files=()
if test -n "${archive_kept}${archive_replaced}"; then
    while read -r f; do
        test -n "$f" || continue
        if test -n "$archive_kept"; then
            cp "$f" "${tmpdir}/kept-${#archive_files[@]}" || die "could not snapshot $f"
        fi
        archive_files+=("$f")
    done < <(find "$mirrorroot" -maxdepth 2 \
        \( -name '*.warc.gz' -o -name '*.warc' -o -name '*.cdx' -o -name '*.wacz' \) \
        2>/dev/null | sort)
    test "${#archive_files[@]}" -gt 0 ||
        die "the first pass produced no archive to compare against"
    test "${#archive_files[@]}" -ge "$archive_min_files" ||
        die "only ${#archive_files[@]} archive file(s), wanted $archive_min_files: ${archive_files[*]}"
fi

# Poison what the second pass must replace: it moves a fresh file over each one,
# so no poison may survive (#726, #1041).
declare -a poisoned_files=()
if test -z "$archive_kept" && test -n "${rerun}${rerun_args}"; then
    if test -n "$archive_replaced"; then
        poisoned_files=("${archive_files[@]}")
    elif test -n "$wacz_validate"; then
        wacz_first=$(find "$mirrorroot" -maxdepth 2 -name '*.wacz' 2>/dev/null | sort | tail -n1)
        test -z "$wacz_first" || poisoned_files=("$wacz_first")
    fi
    i=0
    while test "$i" -lt "${#poisoned_files[@]}"; do
        echo "$archive_poison" >"${poisoned_files[$i]}" ||
            die "could not poison ${poisoned_files[$i]}"
        i=$((i + 1))
    done
fi
test -z "$archive_replaced" || test "${#poisoned_files[@]}" -gt 0 ||
    die "--archive-replaced-on-rerun poisoned nothing, so it would assert nothing"

# --- plant leftovers the second pass has to deal with ------------------------
if test "${#plants[@]}" -gt 0; then
    find_hostroot
    i=0
    while test "$i" -lt "${#plants[@]}"; do
        path="${hostroot}/${plants[$((i + 1))]}"
        info "planting ${plants[$i]} ${plants[$((i + 1))]}"
        if test "${plants[$i]}" = "--plant-dir"; then
            mkdir -p "$path" || die "could not create $path"
        else
            mkdir -p "$(dirname "$path")" || die "could not create ${path%/*}"
            echo "$plant_poison" >"$path" || die "could not write $path"
        fi
        result "OK"
        i=$((i + 2))
    done
fi

# --- optional second pass: re-mirror into the same dir (cache/update path) ----
if test -n "$rerun"; then
    info "re-running httrack (update pass)"
    require_pass "${log}.2" "update pass"
    result "OK (update)"
    # The update summary reports "files updated"; a fresh crawl never does. Assert
    # it so a regression that bypasses the cache (re-crawls fresh) can't pass.
    info "checking update used the cache"
    if grep -aqE "mirror complete in .*files updated" "${logroot}/hts-log.txt"; then
        result "OK"
    else
        result "update pass did not report cache activity"
        exit 1
    fi
fi

# --- optional second pass with extra args (e.g. an --update run under a cap) ---
# Same server and mirror dir as the first pass, so the second pass sees the
# cache the first pass wrote. Used to exercise re-fetch/update behaviour.
if test -n "$rerun_args"; then
    read -ra extra <<<"$rerun_args"
    info "re-running httrack with ${rerun_args}"
    require_pass "${log}.2" "second pass" "${extra[@]}"
    result "OK (second pass)"
fi

# --- optional: did the second pass keep the whole archive byte-identical? ----
if test -n "$archive_kept" && test "${#archive_files[@]}" -gt 0; then
    i=0
    for f in "${archive_files[@]}"; do
        info "checking the second pass kept $(basename "$f")"
        cmp -s "${tmpdir}/kept-${i}" "$f" ||
            die "$(basename "$f") was rewritten: the previous archive was destroyed"
        result "OK"
        i=$((i + 1))
    done
fi
# ...and did it replace what was poisoned above?
i=0
while test "$i" -lt "${#poisoned_files[@]}"; do
    f="${poisoned_files[$i]}"
    info "checking the second pass replaced $(basename "$f")"
    test -s "$f" || die "$(basename "$f") is gone, not replaced"
    grep -qa "$archive_poison" "$f" && die "$(basename "$f") still holds the poison"
    result "OK"
    i=$((i + 1))
done
if test "${#archive_files[@]}" -gt 0; then
    # A leftover in-progress file is as bad: the next pass would silently eat it.
    info "checking no in-progress archive was left behind"
    leftover=$(find "$mirrorroot" -maxdepth 2 \( -name '*.warc.gz.tmp' -o -name '*.warc.tmp' \) 2>/dev/null | head -n1)
    test -z "$leftover" || die "left behind $leftover"
    result "OK"
fi

# --- optional dead pass: server stopped, the cache must survive the rollback --
if test -n "$rerun_dead"; then
    zip="${out}/hts-cache/new.zip"
    test -s "$zip" || die "no cache was written by the first pass"
    cp "$zip" "${tmpdir}/cache-before.zip"
    cp "${logroot}/hts-log.txt" "${tmpdir}/log-before.txt"
    # Disarms its teardown too, so the reaping cannot signal a recycled pid.
    local_server_stop "$SRV_PID"
    info "re-running httrack against the stopped server"
    # Status ignored: the assertions below are about the rollback, not the pass.
    run_pass "${log}.dead" "dead pass"
    result "OK (dead pass ran)"
    # The dead pass must have gone through the no-data rollback, not bailed out
    # before the mirror loop (which would leave the cache trivially untouched).
    info "checking the dead pass hit the rollback"
    if grep -aq "No data seems to have been transferred" "${logroot}/hts-log.txt"; then
        result "OK"
    else
        result "rollback notice not found in hts-log.txt"
        exit 1
    fi
    info "checking the previous cache generation was restored"
    if cmp -s "$zip" "${tmpdir}/cache-before.zip" &&
        test ! -e "${out}/hts-cache/old.zip"; then
        result "OK"
    else
        result "new.zip differs from the pre-outage cache (or old.zip left behind)"
        exit 1
    fi
    # Audits below describe the healthy crawl, not the dead pass.
    cp "${tmpdir}/log-before.txt" "${logroot}/hts-log.txt"
fi

# --- discover the single host root (127.0.0.1_<port> or 127.0.0.1) -----------
find_hostroot
debug "host root: $hostroot"

# --- optional WARC validation (stdlib validator, no warcio) ------------------
# WARC_VALIDATE_BODY="URLSUB=HEX" byte-checks a fresh-crawl response body;
# WARC_VALIDATE_NORESP="URLSUB..." asserts those assets are revisits post-update;
# WARC_VALIDATE_NORECORD="URLSUB..." asserts those assets have no record at all;
# WARC_VALIDATE_IP="URLSUB=IP..." asserts the exact WARC-IP-Address on the record;
# WARC_VALIDATE_PROFILE="URLSUB=SUBSTR..." asserts a revisit's WARC-Profile;
# WARC_VALIDATE_NO_REVISIT=1 skips the "at least one revisit" requirement (a
# no-OpenSSL leg where the only unchanged assets end up with no record at all);
# WARC_VALIDATE_EXCHANGE=1 asserts each revisit carries its 304 request/response.
if test -n "$warc_validate"; then
    validator=$(nativepath "${testdir}/warc-validate.py")
    warc=$(find "$mirrorroot" -maxdepth 2 \( -name '*.warc.gz' -o -name '*.warc' \) 2>/dev/null | sort | tail -n1)
    test -n "$warc" || die "no WARC file produced under $mirrorroot"

    # Fresh-crawl file (snapshot if an update pass overwrote it): full responses.
    fresh="${tmpdir}/warc-pass1.gz"
    test -f "$fresh" || fresh="$warc"
    declare -a bodyargs=()
    # WARC_VALIDATE_BODY holds one or more whitespace-separated SUB=HEX specs.
    for spec in ${WARC_VALIDATE_BODY:-}; do
        bodyargs+=(--expect-body-hex "$spec")
    done
    # compressed asset: assert the stored (verbatim) body inflates to the served
    # body and keeps Content-Encoding, instead of expecting a decoded body.
    test -n "${WARC_VALIDATE_VERBATIM:-}" && bodyargs+=(--verbatim)
    info "validating fresh WARC (response bodies)"
    # macOS bash 3.2 calls an empty array unbound under set -u, and a caller
    # asking only for the revisit checks leaves this one empty.
    "$python" "$validator" "$(nativepath "$fresh")" \
        ${bodyargs[@]+"${bodyargs[@]}"} >&2 ||
        die "fresh WARC validation failed"
    result "OK"

    # After an update pass the unchanged assets must be revisits, in an archive
    # of the pass's own (WARC_VALIDATE_UPDATE): a revisit-only pass never
    # replaces the archive holding the bodies it would strand (#759).
    if test -n "${WARC_VALIDATE_UPDATE:-}"; then
        upd=$(find "$mirrorroot" -maxdepth 2 -name "$WARC_VALIDATE_UPDATE" 2>/dev/null | head -n1)
        test -n "$upd" || die "no $WARC_VALIDATE_UPDATE produced under $mirrorroot"
        declare -a revargs=()
        test -z "${WARC_VALIDATE_NO_REVISIT:-}" && revargs+=(--expect-revisit)
        for sub in ${WARC_VALIDATE_NORESP:-}; do
            revargs+=(--no-response-for "$sub")
        done
        for sub in ${WARC_VALIDATE_NORECORD:-}; do
            revargs+=(--no-record-for "$sub")
        done
        for spec in ${WARC_VALIDATE_IP:-}; do
            revargs+=(--expect-ip "$spec")
        done
        for spec in ${WARC_VALIDATE_PROFILE:-}; do
            revargs+=(--expect-revisit-profile "$spec")
        done
        test -n "${WARC_VALIDATE_EXCHANGE:-}" && revargs+=(--revisit-exchange)
        info "validating update WARC (revisits)"
        "$python" "$validator" "$(nativepath "$upd")" "${revargs[@]}" >&2 ||
            die "update WARC validation failed"
        result "OK"
    fi

    if command -v warcio >/dev/null 2>&1; then
        info "warcio check (optional)"
        if warcio check -v "$warc" >&2; then result "OK"; else die "warcio check failed"; fi
    fi
fi

# --- optional WACZ validation (--wacz) --------------------------------------
if test -n "$wacz_validate"; then
    wacz=$(find "$mirrorroot" -maxdepth 2 -name '*.wacz' 2>/dev/null | sort | tail -n1)
    if test -z "$wacz"; then
        # No package: only acceptable when the build lacks OpenSSL (SHA-256).
        if grep -aqi "WACZ requires an OpenSSL" "${logroot}/hts-log.txt"; then
            info "no .wacz produced (build without OpenSSL); skipping"
            exit 77
        fi
        die "no .wacz file produced under $mirrorroot"
    fi
    validator=$(nativepath "${testdir}/wacz-validate.py")
    info "validating WACZ package"
    "$python" "$validator" "$(nativepath "$wacz")" >&2 || die "WACZ validation failed"
    result "OK"
fi

# No crawl, even a cancelled one, may leave engine temporaries: .delayed (#107,
# #483), the .z/.u content-coding temps (#557), or the ~hts-tmp directory those
# and the re-fetch backup live in (#774). Only a test that planted something in
# ~hts-tmp itself owns what is left there, so only that case skips the scan.
info "checking for leftover engine temporaries"
scan_tmpdir=1
i=0
while test "$i" -lt "${#plants[@]}"; do
    case "${plants[$((i + 1))]}" in
    */~hts-tmp/*) scan_tmpdir=0 ;;
    esac
    i=$((i + 2))
done
if test "$scan_tmpdir" -eq 1; then
    leftovers=$(find "$out" \( -name '*.delayed' -o -name '*.z' -o -name '*.u' \
        -o -name '~hts-tmp' \) 2>/dev/null | head -5)
else
    leftovers=$(find "$out" \( -name '*.delayed' -o -name '*.z' -o -name '*.u' \) \
        2>/dev/null | head -5)
fi
if test -z "$leftovers"; then result "OK"; else
    result "leftover: $leftovers"
    exit 1
fi

# --- audit -------------------------------------------------------------------
i=0
while test "$i" -lt "${#audit[@]}"; do
    case "${audit[$i]}" in
    --errors)
        i=$((i + 1))
        assert_equals "checking errors" "${audit[$i]}" \
            "$(count_log_errors "${logroot}/hts-log.txt")"
        ;;
    --errors-content)
        i=$((i + 1))
        total=$(count_log_errors "${logroot}/hts-log.txt")
        # transient network failures (statuscode -2..-6) flake on busy loopback;
        # the code parens are followed by " at link" or " after N retries at link"
        transient=$(count_matching_lines_regexp '\(-[2-6]\) (at link|after )' "${logroot}/hts-log.txt")
        assert_equals "checking content errors" "${audit[$i]}" "$((total - transient))"
        ;;
    --files)
        i=$((i + 1))
        nFiles=$(grep -E "^HTTrack Website Copier/[^ ]* mirror complete in " "${logroot}/hts-log.txt" |
            sed -e 's/.*[[:space:]]\([^ ]*\)[[:space:]]files written.*/\1/g')
        assert_equals "checking files" "${audit[$i]}" "$nFiles"
        ;;
    --cache-under-logroot)
        # The cache must sit under path_log (logroot), not an ANSI-mangled twin
        # of a non-ASCII -O dir (#630 cache half). Bites only on the Windows leg.
        info "checking cache under logroot"
        if test -e "${logroot}/hts-cache/new.zip"; then result "OK"; else
            result "cache not under logroot (mangled twin?)"
            exit 1
        fi
        ;;
    --cache-found)
        i=$((i + 1))
        info "checking cache holds ${audit[$i]}"
        if cache_has "${audit[$i]}"; then result "OK"; else
            result "not cached"
            exit 1
        fi
        ;;
    --cache-not-found)
        i=$((i + 1))
        info "checking cache lacks ${audit[$i]}"
        if cache_has "${audit[$i]}"; then
            result "cached"
            exit 1
        else result "OK"; fi
        ;;
    --found)
        i=$((i + 1))
        info "checking for ${audit[$i]}"
        if test -f "${hostroot}/${audit[$i]}"; then result "OK"; else
            result "not found"
            exit 1
        fi
        ;;
    --not-found)
        i=$((i + 1))
        info "checking absence of ${audit[$i]}"
        if test ! -f "${hostroot}/${audit[$i]}"; then result "OK"; else
            result "present"
            exit 1
        fi
        ;;
    --directory)
        i=$((i + 1))
        info "checking for dir ${audit[$i]}"
        if test -d "${hostroot}/${audit[$i]}"; then result "OK"; else
            result "not found"
            exit 1
        fi
        ;;
    --log-found)
        i=$((i + 1))
        info "checking log matches ${audit[$i]}"
        log_body >"$logbody"
        if grep -aqE "${audit[$i]}" "$logbody"; then result "OK"; else
            result "not in log"
            exit 1
        fi
        ;;
    --log-not-found)
        i=$((i + 1))
        info "checking log lacks ${audit[$i]}"
        log_body >"$logbody"
        if grep -aqE "${audit[$i]}" "$logbody"; then
            result "present in log"
            exit 1
        else result "OK"; fi
        ;;
    --max-mirror-bytes)
        i=$((i + 1))
        sz=$(find "$hostroot" -type f -exec cat {} + | wc -c | tr -d '[:space:]')
        info "checking mirror size ${sz} <= ${audit[$i]} bytes"
        if test "$sz" -le "${audit[$i]}"; then result "OK"; else
            result "mirror too big"
            exit 1
        fi
        ;;
    --min-mirror-bytes)
        i=$((i + 1))
        sz=$(find "$hostroot" -type f -exec cat {} + | wc -c | tr -d '[:space:]')
        info "checking mirror size ${sz} >= ${audit[$i]} bytes"
        if test "$sz" -ge "${audit[$i]}"; then result "OK"; else
            result "mirror too small"
            exit 1
        fi
        ;;
    --file-matches)
        path="${audit[$((i + 1))]}"
        i=$((i + 2))
        info "checking ${path} matches ${audit[$i]}"
        if grep -aqE "${audit[$i]}" "${hostroot}/${path}"; then result "OK"; else
            result "no match"
            exit 1
        fi
        ;;
    --file-not-matches)
        path="${audit[$((i + 1))]}"
        i=$((i + 2))
        info "checking ${path} lacks ${audit[$i]}"
        if grep -aqE "${audit[$i]}" "${hostroot}/${path}"; then
            result "matched"
            exit 1
        else result "OK"; fi
        ;;
    --file-min-bytes)
        path="${audit[$((i + 1))]}"
        i=$((i + 2))
        sz=$(wc -c <"${hostroot}/${path}" 2>/dev/null | tr -d '[:space:]')
        info "checking ${path} size ${sz:-0} >= ${audit[$i]} bytes"
        if test -n "$sz" && test "$sz" -ge "${audit[$i]}"; then result "OK"; else
            result "file too small (or missing)"
            exit 1
        fi
        ;;
    --file-mode)
        path="${audit[$((i + 1))]}"
        i=$((i + 2))
        if is_windows; then
            # No POSIX modes, and the engine only chmods #ifndef _WIN32.
            info "checking ${path} mode"
            result "SKIP (no POSIX modes)"
        else
            mode=$(stat -c '%a' "${hostroot}/${path}" 2>/dev/null ||
                stat -f '%Lp' "${hostroot}/${path}" 2>/dev/null)
            info "checking ${path} mode ${mode:-none} is ${audit[$i]}"
            if test "$mode" = "${audit[$i]}"; then result "OK"; else
                result "wrong mode"
                exit 1
            fi
        fi
        ;;
    esac
    i=$((i + 1))
done
