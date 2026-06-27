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
#       --errors N --files N --found PATH ... --directory PATH ... \
#       --log-found REGEX ... --log-not-found REGEX ... \
#       httrack BASEURL/some/path [httrack-args...]
# --log-found/--log-not-found grep (ERE) the crawl's hts-log.txt.
# --cookie writes a Netscape cookies.txt (scoped to the discovered host:port,
# which the ephemeral port forces into the cookie domain) and passes it to
# httrack via --cookies-file, to exercise preloaded cookies.

set -u

testdir=$(cd "$(dirname "$0")" && pwd)
server="${testdir}/local-server.py"
root="${LOCAL_SERVER_ROOT:-${testdir}/server-root}"
cert="${testdir}/server.crt"
key="${testdir}/server.key"

tls=
verbose=
rerun=
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
    --rerun) rerun=1 ;; # run httrack a second time (update pass) before auditing
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
    --errors | --files)
        audit+=("${args[$pos]}" "${args[$((pos + 1))]}")
        pos=$((pos + 1))
        ;;
    --found | --not-found | --directory | --log-found | --log-not-found)
        audit+=("${args[$pos]}" "${args[$((pos + 1))]}")
        pos=$((pos + 1))
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

# --- audit -------------------------------------------------------------------
i=0
while test "$i" -lt "${#audit[@]}"; do
    case "${audit[$i]}" in
    --errors)
        i=$((i + 1))
        assert_equals "checking errors" "${audit[$i]}" \
            "$(grep -iEc "^[0-9:]*[[:space:]]Error:" "${out}/hts-log.txt")"
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
    esac
    i=$((i + 1))
done
