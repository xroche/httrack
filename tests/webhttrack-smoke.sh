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
# webhttrack backgrounds htsserver, which outlives it; reap any stray one (scoped
# to this prefix) so a lingering server can never hold the CI step open.
trap 'pkill -f "$prefix/bin/htsserver" 2>/dev/null || true; rm -rf "$work"' EXIT
export HOME="$work/home"
mkdir -p "$HOME/websites"
marker="$work/marker"

# Stub browser: webhttrack tries its browser-name list in order and runs the
# first it finds, so shadow the first entry, "x-www-browser". It gets the server
# URL, fetches it, and records the result (htsserver only lives until webhttrack
# exits, so the check has to happen here).
# -a: the UI is served ISO-8859-1, so grep must not treat it as binary.
stubdir="$work/bin"
mkdir -p "$stubdir"
cat >"$stubdir/x-www-browser" <<EOF
#!/bin/bash
echo "stub browser invoked with: \$1" >&2
if body="\$(curl -fsSL --max-time 20 "\$1")" && printf '%s' "\$body" | grep -qai httrack; then
    echo PASS >"$marker"
else
    echo "FAIL: unexpected response from \$1" >"$marker"
fi
EOF
chmod +x "$stubdir/x-www-browser"
export PATH="$stubdir:$prefix/bin:$PATH"

echo "launching webhttrack"
"$wht" </dev/null >"$work/webhttrack.log" 2>&1 &
whpid=$!

# Hard watchdog: macOS has no timeout(1), and webhttrack can block if htsserver
# never publishes a URL, so guarantee the tree dies regardless of the poll below.
(
    sleep 40
    kill "$whpid" 2>/dev/null || true
    pkill -f "$prefix/bin/htsserver" 2>/dev/null || true
) &
wdpid=$!

for i in $(seq 1 45); do
    test -f "$marker" && {
        echo "marker written after ${i}s"
        break
    }
    kill -0 "$whpid" 2>/dev/null || {
        echo "webhttrack exited on its own after ${i}s"
        break
    }
    sleep 1
done

# Reap webhttrack and the htsserver it spawned; stop the watchdog. Confirm death
# with a bounded poll instead of a blocking wait (which could hang on macOS).
echo "tearing down"
kill "$whpid" 2>/dev/null || true
if pkill -f "$prefix/bin/htsserver" 2>/dev/null; then
    echo "reaped a lingering htsserver"
else
    echo "no lingering htsserver"
fi
kill "$wdpid" 2>/dev/null || true
for _ in $(seq 1 10); do
    kill -0 "$whpid" 2>/dev/null || break
    sleep 1
done

echo "--- webhttrack.log ---"
cat "$work/webhttrack.log" 2>/dev/null || true
echo "--- end ---"
echo "marker=[$(cat "$marker" 2>/dev/null || echo NONE)]"

if test "$(cat "$marker" 2>/dev/null || true)" = PASS; then
    echo "webhttrack smoke: PASS"
else
    echo "webhttrack smoke: FAIL" >&2
    exit 1
fi
