#!/bin/bash
# Smoke-test an installed webhttrack: launch it with a stub browser and assert
# htsserver comes up and serves the web UI. Arg: the install prefix.
set -euo pipefail

prefix="${1:?usage: webhttrack-smoke.sh <install-prefix>}"
wht="$prefix/bin/webhttrack"
test -x "$wht" || {
    echo "no webhttrack at $wht" >&2
    exit 1
}

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
export HOME="$work/home"
mkdir -p "$HOME/websites"
marker="$work/marker"

# Stub browser: webhttrack tries its browser-name list in order and runs the
# first it finds, so shadow the first entry, "x-www-browser". It gets the server
# URL, fetches it, and records the result (htsserver only lives until webhttrack
# exits, so the check has to happen here).
stubdir="$work/bin"
mkdir -p "$stubdir"
cat >"$stubdir/x-www-browser" <<EOF
#!/bin/bash
# -a: the UI is served ISO-8859-1, so grep must not treat it as binary.
if body="\$(curl -fsSL --max-time 20 "\$1")" && printf '%s' "\$body" | grep -qai httrack; then
    echo PASS >"$marker"
else
    echo "FAIL: unexpected response from \$1" >"$marker"
fi
EOF
chmod +x "$stubdir/x-www-browser"
export PATH="$stubdir:$prefix/bin:$PATH"

# webhttrack self-exits once the browser stub returns; poll for the marker and
# reap it, with a kill guard in case htsserver never publishes a URL.
"$wht" >"$work/webhttrack.log" 2>&1 &
whpid=$!
for _ in $(seq 1 60); do
    test -f "$marker" && break
    kill -0 "$whpid" 2>/dev/null || break
    sleep 1
done
kill "$whpid" 2>/dev/null || true
wait "$whpid" 2>/dev/null || true

if test "$(cat "$marker" 2>/dev/null || true)" = PASS; then
    echo "webhttrack smoke: PASS"
else
    echo "webhttrack smoke: FAIL" >&2
    echo "--- marker ---" >&2
    cat "$marker" >&2 2>/dev/null || echo "(no marker written)" >&2
    echo "--- webhttrack.log ---" >&2
    cat "$work/webhttrack.log" >&2 || true
    exit 1
fi
