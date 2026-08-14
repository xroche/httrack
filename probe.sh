set -eu
. "$(dirname "$0")/crawllib.sh"
root=$(nativepath "${testdir}/server-root")
tmpdir=$AUDIT_OUT
local_server_start --root "$root"
rc=0
run_with_timeout 60 "$HTB" -O "${tmpdir}/crawl" -W --robots=0 --retries=0 \
    --timeout=10 "http://127.0.0.1:${SRV_PORT}/${AUDIT_PAGE:-wizardeof/index.html}" \
    </dev/null >"${tmpdir}/crawl.log" 2>&1 || rc=$?
echo "RC=$rc PORT=$SRV_PORT" > "${tmpdir}/rc.txt"
