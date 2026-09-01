#!/bin/bash
# Smoke-test an installed webhttrack: launch it with a stub browser and assert
# htsserver comes up and serves the web UI. Args: the install prefix, and optionally
# the launcher to run instead of $prefix/bin/webhttrack (the macOS .app stub).
set -euo pipefail

prefix="${1:?usage: webhttrack-smoke.sh <install-prefix> [launcher]}"
wht="${2:-$prefix/bin/webhttrack}"
test -x "$wht" || {
    echo "no webhttrack at $wht" >&2
    exit 1
}

browserstub="$prefix/bin/x-www-browser"
work="$(mktemp -d)"
# webhttrack backgrounds htsserver, which outlives it; reap any stray one (scoped
# to this prefix) so a lingering server can never hold the CI step open.
trap 'set +e; pkill -f "$prefix/bin/htsserver" 2>/dev/null || true; rm -rf "$work" "$browserstub" "$prefix/bin/open"' EXIT
export HOME="$work/home"
mkdir -p "$HOME/websites"
marker="$work/marker"

# No installed symlink may be absolute, or the tree only works at the prefix it was
# built for (#885). BSD find has no -lname, so read each link back by hand.
abs=""
nlink=0
while IFS= read -r l; do
    nlink=$((nlink + 1))
    case "$(readlink "$l")" in
    /*) abs="$abs $l" ;;
    esac
done < <(find "$prefix" -type l)
test -z "$abs" || {
    echo "absolute symlink(s) in install tree:$abs" >&2
    exit 1
}
# find's status is lost through the pipe, so an empty scan would pass vacuously.
test "$nlink" -gt 0 || {
    echo "scanned no symlinks at all under $prefix" >&2
    exit 1
}

# Locate the data dir the way webhttrack does, by the file it keys on, rather than
# assuming $prefix/share: --datadir moves it, and the link has to follow the data.
langdef=$(find "$prefix" -name lang.def -type f | head -1)
test -n "$langdef" || {
    echo "no lang.def under $prefix" >&2
    exit 1
}
htmllink="$(dirname "$langdef")/html"
test -L "$htmllink" || {
    echo "no data symlink beside $langdef" >&2
    exit 1
}

# Relocation: resolve the link inside a copy at another path and require it to stay
# inside that copy. An absolute link resolves back to $prefix and fails here.
reloc="$work/reloc"
cp -R "$prefix" "$reloc"
relocreal=$(cd "$reloc" && pwd -P)
htmlreal=$(cd "$reloc${htmllink#"$prefix"}" 2>/dev/null && pwd -P) || {
    echo "relocated tree: the data link does not resolve" >&2
    exit 1
}
case "$htmlreal" in
"$relocreal"/*) ;;
*)
    echo "relocated tree: link escapes to $htmlreal" >&2
    exit 1
    ;;
esac
# Resolving is not enough: it must land on the served UI, not just any directory.
test -d "$htmlreal/server" || {
    echo "relocated tree: $htmlreal has no server/" >&2
    exit 1
}
echo "install tree is relocatable"

stubdir="$work/bin"
mkdir -p "$stubdir"

# On Darwin webhttrack falls back to "open -W", which launches a real GUI browser and
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

# Stub browser, named after the first entry of webhttrack's browser list. It goes in
# $prefix/bin because webhttrack searches its own SRCHPATH before $PATH, so a real
# /usr/bin/x-www-browser (Edge, on the GitHub Linux runners) would beat a PATH shadow.
# It fetches the server URL and records PASS only for the working UI: the brand
# string, the step-2 form action, and an option-page tooltip, which a
# truncated/degraded template page would lack. htsserver only lives until webhttrack
# exits, so the check has to happen here.
# -a: the UI is served ISO-8859-1, so grep must not treat it as binary.
cat >"$browserstub" <<EOF
#!/bin/bash
echo "stub browser invoked with: \$1" >&2
# Also fetch an option page and require a rendered title='' tooltip: proves the
# option template expands and the \${html:} filter escapes into the attribute.
# option9 additionally proves the WARC and change-report controls and
# option2 the --single-file pair render with their expanded labels, and
# option8 the sitemap ones. option2 also pins the absence of an
# unexpanded key, since the default locale here is French.
opturl="\${1%/}/server/option2.html"
warcurl="\${1%/}/server/option9.html"
smurl="\${1%/}/server/option8.html"
if body="\$(curl -fsSL --max-time 20 "\$1")" && grep -qai httrack <<<"\$body" && grep -qaF step2.html <<<"\$body" &&
    opt="\$(curl -fsSL --max-time 20 "\$opturl")" && grep -qaF "title='" <<<"\$opt" &&
    grep -qaF 'name="singlefile"' <<<"\$opt" && grep -qaF 'name="singlefilemax"' <<<"\$opt" &&
    ! grep -qaF '\${LANG_SINGLEFILE}' <<<"\$opt" &&
    warc="\$(curl -fsSL --max-time 20 "\$warcurl")" && grep -qaF 'name="warcfile"' <<<"\$warc" && grep -qaF WARC <<<"\$warc" &&
    grep -qaF 'name="changes"' <<<"\$warc" && grep -qaF hts-changes.json <<<"\$warc" &&
    sm="\$(curl -fsSL --max-time 20 "\$smurl")" && grep -qaF 'name="sitemapurl"' <<<"\$sm" && grep -qaF 'name="sitemap"' <<<"\$sm"; then
    echo PASS >"$marker"
else
    echo "FAIL: unexpected response from \$1" >"$marker"
fi
EOF
chmod +x "$browserstub"
# Deliberately NOT $prefix/bin: the launcher must find its payload from $0, and
# the browser stub is picked up through webhttrack's SRCHPATH, not $PATH.
export PATH="$stubdir:$PATH"

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

test "$(cat "$marker" 2>/dev/null || true)" = PASS || {
    echo "webhttrack smoke: FAIL" >&2
    exit 1
}
echo "webhttrack smoke: PASS"

# The browser helper must not outlive the session: Darwin's "open -W" returns only
# once the browser itself quits, and LaunchServices keeps the bundle, and its
# bouncing Dock icon, alive on that orphan until the user force-quits it. Darwin
# only, because everywhere else a real browser is found ahead of open in the list.
rm -f "$stubdir/uname"
test "$(uname -s)" = Darwin || {
    echo "not Darwin: skipping the browser-helper check"
    exit 0
}

# $prefix/bin heads webhttrack's own search path, so this shadows /usr/bin/open and
# no real browser is launched. It records the pid it blocks under, which is what the
# check below waits on.
rm -f "$browserstub"
openstub="$prefix/bin/open"
openargs="$work/open.args"
openpid="$work/open.pid"
cat >"$openstub" <<EOF
#!/bin/bash
echo "\$@" >"$openargs"
echo \$\$ >"$openpid"
exec sleep 600
EOF
chmod +x "$openstub"

echo "launching webhttrack against the open helper"
"$wht" </dev/null >"$work/webhttrack-helper.log" 2>&1 &
whpid2=$!
for i in $(seq 1 45); do
    test -s "$openpid" && {
        echo "helper started after ${i}s"
        break
    }
    kill -0 "$whpid2" 2>/dev/null || break
    sleep 1
done
test -s "$openpid" || {
    echo "webhttrack never ran the open helper; its log follows" >&2
    cat "$work/webhttrack-helper.log" >&2
    exit 1
}
# -W is what makes open wait, and therefore what makes reaping necessary.
case "$(cat "$openargs")" in
*-W*) ;;
*)
    echo "open helper got [$(cat "$openargs")], expected -W" >&2
    exit 1
    ;;
esac

# End the session the way closing the UI does, then give webhttrack its own teardown.
helper=$(cat "$openpid")
pkill -f "$prefix/bin/htsserver" || true
for _ in $(seq 1 30); do
    kill -0 "$whpid2" 2>/dev/null || break
    sleep 1
done
for _ in $(seq 1 10); do
    kill -0 "$helper" 2>/dev/null || break
    sleep 1
done
kill -0 "$helper" 2>/dev/null && {
    kill -9 "$helper" 2>/dev/null || true
    echo "the browser helper ($helper) outlived the session" >&2
    exit 1
}
echo "browser helper reaped with the session"
