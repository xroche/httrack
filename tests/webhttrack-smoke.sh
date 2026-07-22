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

stubdir="$work/bin"
mkdir -p "$stubdir"

# On Darwin webhttrack hardcodes "open -W", which launches a real GUI browser and
# blocks headless. Shadow uname so it takes the generic path and picks the stub
# browser below; htsserver and webhttrack's path resolution still run for real.
cat >"$stubdir/uname" <<'EOF'
#!/bin/bash
[ "${1:-}" = "-s" ] && {
    echo Linux
    exit 0
}
exec /usr/bin/uname "$@"
EOF
chmod +x "$stubdir/uname"

# Stub browser: webhttrack tries its browser-name list in order and runs the
# first it finds, so shadow the first entry, "x-www-browser". It fetches the
# server URL and records PASS only for the working UI: the brand string, the
# step-2 form action, and an option-page tooltip, which a truncated/degraded
# template page would lack. htsserver only lives until webhttrack exits, so the
# check has to happen here.
# -a: the UI is served ISO-8859-1, so grep must not treat it as binary.
cat >"$stubdir/x-www-browser" <<EOF
#!/bin/bash
echo "stub browser invoked with: \$1" >&2
# Also fetch an option page and require a rendered title='' tooltip: proves the
# option template expands and the \${html:} filter escapes into the attribute.
# option9 additionally proves the WARC control renders with its expanded label.
opturl="\${1%/}/server/option2.html"
warcurl="\${1%/}/server/option9.html"
if body="\$(curl -fsSL --max-time 20 "\$1")" && printf '%s' "\$body" | grep -qai httrack && printf '%s' "\$body" | grep -qaF step2.html &&
    opt="\$(curl -fsSL --max-time 20 "\$opturl")" && printf '%s' "\$opt" | grep -qaF "title='" &&
    warc="\$(curl -fsSL --max-time 20 "\$warcurl")" && printf '%s' "\$warc" | grep -qaF 'name="warcfile"' && printf '%s' "\$warc" | grep -qaF WARC; then
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

# Bounded poll for the marker (macOS has no timeout(1)); teardown below kills
# webhttrack and reaps htsserver, so the run is bounded without a watchdog.
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

# Reap webhttrack and the htsserver it spawned. Confirm death with a bounded poll
# (not a blocking wait, which could hang on macOS); SIGKILL if it ignores TERM.
echo "tearing down"
kill "$whpid" 2>/dev/null || true
if pkill -f "$prefix/bin/htsserver" 2>/dev/null; then
    echo "reaped a lingering htsserver"
else
    echo "no lingering htsserver"
fi
for _ in $(seq 1 10); do
    kill -0 "$whpid" 2>/dev/null || break
    sleep 1
done
kill -9 "$whpid" 2>/dev/null || true

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
