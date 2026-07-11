#!/bin/bash
#
# Launcher for httrack crawl tests against the local Python test server.
#
# Starts tests/local-server.py on an ephemeral port, discovers the port from
# the server's stdout, then runs httrack against http(s)://127.0.0.1:$PORT and
# audits the mirror. The server is always killed and the tmpdir removed on exit.
#
# The token BASEURL in any httrack argument is replaced with the discovered
# http(s)://127.0.0.1:$PORT base. --found/--directory paths are relative to the
# discovered host root (127.0.0.1_<port>/), since the random port leaks into
# the mirror directory name.
#
# Usage:
#   bash local-crawl.sh [--tls] [--root DIR] [--cookie NAME=VALUE ...] \
#       [--rerun-args 'ARGS'] \
#       --errors N --errors-content N --files N --found PATH ... --directory PATH ... \
#       --log-found REGEX ... --log-not-found REGEX ... \
#       --file-matches PATH REGEX ... --file-not-matches PATH REGEX ... \
#       --file-min-bytes PATH N --max-mirror-bytes N \
#       httrack BASEURL/some/path [httrack-args...]
# --errors counts every "Error:" log line; --errors-content drops transient
# network failures (codes -2..-6) that flake on busy loopback under -c8.
# --log-found/--log-not-found grep (ERE) the crawl's hts-log.txt.
# --max/--min-mirror-bytes bound the mirrored content bytes (host root).
# --file-matches/--file-not-matches grep (ERE) a mirrored file (PATH under the
# host root), to assert rewritten link/content survived the crawl.
# --file-min-bytes asserts a mirrored file (PATH) is at least N bytes.
# --rerun-args runs a second pass (same server and mirror dir) with the given
# extra httrack args appended, e.g. an --update run under a cap.
# --cookie writes a Netscape cookies.txt (scoped to the discovered host:port,
# which the ephemeral port forces into the cookie domain) and passes it to
# httrack via --cookies-file, to exercise preloaded cookies.
# --rerun-dead re-runs with the server stopped: the no-data rollback must
# restore the previous hts-cache generation byte-identical.

set -u

testdir=$(cd "$(dirname "$0")" && pwd)
server="${testdir}/local-server.py"
root="${LOCAL_SERVER_ROOT:-${testdir}/server-root}"
cert="${testdir}/server.crt"
key="${testdir}/server.key"

tls=
verbose=
rerun=
rerun_args=
rerun_dead=
tmpdir=
serverpid=
crawlpid=

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

function cleanup {
    if test -n "$crawlpid"; then
        kill -9 "$crawlpid" 2>/dev/null
        crawlpid=
    fi
    if test -n "$serverpid"; then
        kill "$serverpid" 2>/dev/null
        # Reap it so the port is released before we rm the tmpdir/log.
        wait "$serverpid" 2>/dev/null
        serverpid=
    fi
    if test -n "$tmpdir" && test -d "$tmpdir"; then
        test -n "$nopurge" || rm -rf "$tmpdir"
    fi
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
trap cleanup EXIT HUP INT QUIT PIPE TERM

# python3 is required; mirror check-network.sh's skip-with-77 convention.
command -v python3 >/dev/null || ! echo "python3 not found; skipping local crawl tests" || exit 77

tmptopdir=${TMPDIR:-/tmp}
test -d "$tmptopdir" || mkdir -p "$tmptopdir" || die "no temporary directory; set TMPDIR"
tmpdir=$(mktemp -d "${tmptopdir}/httrack_local.XXXXXX") || die "could not create tmpdir"

# --- parse leading control flags --------------------------------------------
declare -a audit=()
declare -a cookies=()
scheme=http
pos=0
args=("$@")
nargs=$#
while test "$pos" -lt "$nargs"; do
    case "${args[$pos]}" in
    --debug) verbose=1 ;;
    --rerun) rerun=1 ;;           # run httrack a second time (update pass) before auditing
    --rerun-dead) rerun_dead=1 ;; # re-run with the server stopped (cache rollback)
    --no-purge)
        nopurge=1
        audit+=("--no-purge")
        ;;
    --tls)
        tls=1
        scheme=https
        ;;
    --root)
        pos=$((pos + 1))
        root="${args[$pos]}"
        ;;
    --cookie)
        pos=$((pos + 1))
        cookies+=("${args[$pos]}")
        ;;
    --rerun-args)
        pos=$((pos + 1))
        rerun_args="${args[$pos]}"
        ;;
    --errors | --errors-content | --files)
        audit+=("${args[$pos]}" "${args[$((pos + 1))]}")
        pos=$((pos + 1))
        ;;
    --found | --not-found | --directory | --log-found | --log-not-found | --max-mirror-bytes | --min-mirror-bytes)
        audit+=("${args[$pos]}" "${args[$((pos + 1))]}")
        pos=$((pos + 1))
        ;;
    --file-matches | --file-not-matches | --file-min-bytes)
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
test -r "$server" || die "cannot read $server"
serverlog="${tmpdir}/server.log"
serverargs=(--root "$root")
if test -n "$tls"; then
    serverargs+=(--tls --cert "$cert" --key "$key")
fi
debug "starting python3 $server ${serverargs[*]}"
python3 "$server" "${serverargs[@]}" >"$serverlog" 2>&1 &
serverpid=$!

# Wait for the "PORT <n>" line (server prints it once bound).
port=
for _ in $(seq 1 50); do
    if test -s "$serverlog"; then
        line=$(head -n1 "$serverlog")
        if test "${line%% *}" == "PORT"; then
            port="${line#PORT }"
            break
        fi
    fi
    kill -0 "$serverpid" 2>/dev/null || die "server exited early: $(cat "$serverlog")"
    sleep 0.1
done
test -n "$port" || die "could not discover server port: $(cat "$serverlog")"
debug "server listening on ${scheme}://127.0.0.1:${port}"

baseurl="${scheme}://127.0.0.1:${port}"

# --- substitute BASEURL in the remaining (httrack) args ----------------------
declare -a hts=()
while test "$pos" -lt "$nargs"; do
    hts+=("${args[$pos]//BASEURL/$baseurl}")
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
which httrack >/dev/null || die "could not find httrack"
ver=$(httrack -O /dev/null --version | sed -e 's/HTTrack version //')
test -n "$ver" || die "could not run httrack"

out="${tmpdir}/crawl"
mkdir "$out" || die "could not create $out"
# Localhost is fast; disable the rate/bandwidth safety limits but keep a
# max-time backstop so a hang cannot wedge the suite.
declare -a moreargs=(--quiet --max-time=120 --timeout=30 --disable-security-limits --robots=0)
log="${tmpdir}/log"
info "running httrack ${hts[*]}"
httrack -O "$out" --user-agent="httrack $ver local ($(uname -omrs))" "${moreargs[@]}" "${hts[@]}" >"$log" 2>&1 &
crawlpid=$!
wait "$crawlpid"
crawlres=$?
crawlpid=
# httrack exits 0 even on hard connect/DNS errors, so this is a backstop only;
# the real guard is the audit below (--errors 0 plus the host-root existence check).
test "$crawlres" -eq 0 || ! result "httrack exited $crawlres" || {
    cat "$log" >&2
    exit 1
}
result "OK"
grep -iE "^[0-9:]*[[:space:]]Error:" "${out}/hts-log.txt" >&2

# --- optional second pass: re-mirror into the same dir (cache/update path) ----
if test -n "$rerun"; then
    info "re-running httrack (update pass)"
    httrack -O "$out" --user-agent="httrack $ver local ($(uname -omrs))" \
        "${moreargs[@]}" "${hts[@]}" >"${log}.2" 2>&1 &
    crawlpid=$!
    wait "$crawlpid"
    crawlres=$?
    crawlpid=
    test "$crawlres" -eq 0 || ! result "update pass exited $crawlres" || {
        cat "${log}.2" >&2
        exit 1
    }
    result "OK (update)"
    # The update summary reports "files updated"; a fresh crawl never does. Assert
    # it so a regression that bypasses the cache (re-crawls fresh) can't pass.
    info "checking update used the cache"
    if grep -aqE "mirror complete in .*files updated" "${out}/hts-log.txt"; then
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
    httrack -O "$out" --user-agent="httrack $ver local ($(uname -omrs))" \
        "${moreargs[@]}" "${hts[@]}" "${extra[@]}" >"${log}.2" 2>&1 &
    crawlpid=$!
    wait "$crawlpid"
    crawlres=$?
    crawlpid=
    test "$crawlres" -eq 0 || ! result "second pass exited $crawlres" || {
        cat "${log}.2" >&2
        exit 1
    }
    result "OK (second pass)"
fi

# --- optional dead pass: server stopped, the cache must survive the rollback --
if test -n "$rerun_dead"; then
    zip="${out}/hts-cache/new.zip"
    test -s "$zip" || die "no cache was written by the first pass"
    cp "$zip" "${tmpdir}/cache-before.zip"
    cp "${out}/hts-log.txt" "${tmpdir}/log-before.txt"
    kill "$serverpid" 2>/dev/null
    wait "$serverpid" 2>/dev/null
    serverpid=
    info "re-running httrack against the stopped server"
    httrack -O "$out" --user-agent="httrack $ver local ($(uname -omrs))" \
        "${moreargs[@]}" "${hts[@]}" >"${log}.dead" 2>&1 &
    crawlpid=$!
    wait "$crawlpid" || true
    crawlpid=
    result "OK (dead pass ran)"
    # The dead pass must have gone through the no-data rollback, not bailed out
    # before the mirror loop (which would leave the cache trivially untouched).
    info "checking the dead pass hit the rollback"
    if grep -aq "No data seems to have been transferred" "${out}/hts-log.txt"; then
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
    cp "${tmpdir}/log-before.txt" "${out}/hts-log.txt"
fi

# --- discover the single host root (127.0.0.1_<port> or 127.0.0.1) -----------
hostroot=
for cand in "${out}/127.0.0.1_${port}" "${out}/127.0.0.1"; do
    if test -d "$cand"; then
        hostroot="$cand"
        break
    fi
done
test -n "$hostroot" || die "could not find host root under $out"
debug "host root: $hostroot"

# No crawl, even a cancelled one, may leave .delayed temporaries (#107, #483).
info "checking for leftover .delayed files"
leftovers=$(find "$out" -name '*.delayed' 2>/dev/null | head -5)
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
            "$(grep -iEc "^[0-9:]*[[:space:]]Error:" "${out}/hts-log.txt")"
        ;;
    --errors-content)
        i=$((i + 1))
        total=$(grep -icE "^[0-9:]*[[:space:]]Error:" "${out}/hts-log.txt")
        # transient network failures (statuscode -2..-6) flake on busy loopback;
        # the code parens are followed by " at link" or " after N retries at link"
        transient=$(grep -cE '\(-[2-6]\) (at link|after )' "${out}/hts-log.txt" || true)
        assert_equals "checking content errors" "${audit[$i]}" "$((total - transient))"
        ;;
    --files)
        i=$((i + 1))
        nFiles=$(grep -E "^HTTrack Website Copier/[^ ]* mirror complete in " "${out}/hts-log.txt" |
            sed -e 's/.*[[:space:]]\([^ ]*\)[[:space:]]files written.*/\1/g')
        assert_equals "checking files" "${audit[$i]}" "$nFiles"
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
        if grep -aqE "${audit[$i]}" "${out}/hts-log.txt"; then result "OK"; else
            result "not in log"
            exit 1
        fi
        ;;
    --log-not-found)
        i=$((i + 1))
        info "checking log lacks ${audit[$i]}"
        if grep -aqE "${audit[$i]}" "${out}/hts-log.txt"; then
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
    esac
    i=$((i + 1))
done
