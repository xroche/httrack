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
whpid=""

# Kill the htsserver the launcher $whpid spawned. It carries "--ppid <launcher>", and
# matching that rather than an installed path is what reaches the .app, whose argv[0]
# runs through Contents/MacOS/.. instead.
kill_server() {
    test -n "$whpid" || return 1
    pkill -f "ppid $whpid "
}
# htsserver outlives the launcher and the stub browsers below block, so nothing this
# script started may be left holding the CI step open.
teardown() {
    kill_server 2>/dev/null
    kill "$whpid" 2>/dev/null
    for f in "$work"/*.pid; do
        kill -9 "$(cat "$f" 2>/dev/null)" 2>/dev/null
    done
    rm -rf "$work" "$browserstub" "$prefix/bin/open"
}
# Inline "set +e", the shape 103_teardown-status pins: a failing teardown under
# errexit would otherwise become the verdict (#773).
trap 'set +e; teardown' EXIT
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
# The served UI is runtime, so it is real files beside the catalogs. A symlink
# here is the older layout, where a package stripping $(docdir) took the UI too.
htmldir="$(dirname "$langdef")/html"
test ! -L "$htmldir" || {
    echo "the served root $htmldir is a symlink to $(readlink "$htmldir")" >&2
    exit 1
}
test -f "$htmldir/server/index.html" || {
    echo "no served UI at $htmldir/server" >&2
    exit 1
}
# The manual is documentation, so the UI reaches it through this link. It may
# dangle where a package strips the docs, but it may not be missing.
doclink="$htmldir/doc"
test -L "$doclink" || {
    echo "no documentation link at $doclink" >&2
    exit 1
}

# Relocation: resolve the link inside a copy at another path and require it to stay
# inside that copy. An absolute link resolves back to $prefix and fails here.
reloc="$work/reloc"
cp -R "$prefix" "$reloc"
relocreal=$(cd "$reloc" && pwd -P)
docreal=$(cd "$reloc${doclink#"$prefix"}" 2>/dev/null && pwd -P) || {
    echo "relocated tree: the documentation link does not resolve" >&2
    exit 1
}
case "$docreal" in
"$relocreal"/*) ;;
*)
    echo "relocated tree: link escapes to $docreal" >&2
    exit 1
    ;;
esac
# Resolving is not enough: it must land on the manual the panes link, and the UI
# must have travelled as real files rather than through the link.
test -f "$docreal/html/guide.html" || {
    echo "relocated tree: $docreal holds no html/guide.html" >&2
    exit 1
}
test -f "$reloc${htmldir#"$prefix"}/server/index.html" || {
    echo "relocated tree: the served UI did not travel" >&2
    exit 1
}
echo "install tree is relocatable"

stubdir="$work/bin"
mkdir -p "$stubdir"

# On Darwin webhttrack falls back to "open -W", which launches a real GUI browser and
# blocks headless. Shadow uname so it takes the generic path and picks the stub browser
# below. The rest, htsserver included, still runs for real.
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
if kill_server 2>/dev/null; then
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

# The browser helper must not outlive the session: Darwin's open -W blocks until the
# browser itself quits, and an unreaped one keeps the bundle registered with
# LaunchServices, so its Dock icon stays up until the user force-quits it. Darwin only,
# because everywhere else a real browser is found ahead of open in webhttrack's list.
rm -f "$stubdir/uname"
test "$(uname -s)" = Darwin || {
    echo "not Darwin: skipping the browser-helper check"
    exit 0
}

# A browser stub that blocks the way open -W does, recording its argv one word per line
# in $2 and the pid it blocks under in $3. exec, so that pid is the one still alive.
mkhelper() {
    cat >"$1" <<EOF
#!/bin/bash
printf '%s\n' "\$@" >"$2"
echo \$\$ >"$3"
exec sleep 600
EOF
    chmod +x "$1"
}

# Run webhttrack and wait for the stub recording into $1 to block. Sets whpid and helper.
launch_and_wait() {
    local pidfile=$1 log=$2 i
    rm -f "$pidfile"
    "$wht" </dev/null >"$log" 2>&1 &
    whpid=$!
    for i in $(seq 1 45); do
        test -s "$pidfile" && {
            echo "the browser stub blocked after ${i}s"
            break
        }
        kill -0 "$whpid" 2>/dev/null || {
            echo "webhttrack exited on its own after ${i}s"
            break
        }
        sleep 1
    done
    test -s "$pidfile" || {
        echo "no browser stub ever blocked; webhttrack's log follows" >&2
        cat "$log" >&2
        exit 1
    }
    helper=$(cat "$pidfile")
}

# Poll for $1 to go away, up to $2 seconds. Signals are asynchronous, so the helper can
# outlive the launcher that reaped it by a moment.
wait_gone() {
    local pid=$1 n=$2
    while test "$n" -gt 0; do
        kill -0 "$pid" 2>/dev/null || return 0
        n=$((n - 1))
        sleep 1
    done
    return 1
}

# End the session the way closing the last UI window does. A no-match means the server
# had already gone and the reap was never put under test.
end_session() {
    kill_server || {
        echo "no htsserver to end the session with; the reap went untested" >&2
        exit 1
    }
    wait_gone "$whpid" 30 || {
        echo "webhttrack did not exit when the session ended" >&2
        exit 1
    }
}

# $prefix/bin heads webhttrack's search path, so this shadows /usr/bin/open. The PATH is
# the one launchd hands an app opened from Finder, which is why the shipped bundle finds
# no browser and reaches open at all.
rm -f "$browserstub"
openstub="$prefix/bin/open"
openargs="$work/open.args"
openpid="$work/open.pid"
mkhelper "$openstub" "$openargs" "$openpid"
export PATH=/usr/bin:/bin:/usr/sbin:/sbin

echo "case 1: the session ends, the helper goes with it"
launch_and_wait "$openpid" "$work/helper1.log"
# A whole line, because a URL carrying the letters would satisfy a substring match.
grep -qxF -- -W "$openargs" || {
    echo "open got [$(tr '\n' ' ' <"$openargs")], expected -W" >&2
    exit 1
}
kill -0 "$helper" 2>/dev/null || {
    echo "the helper was reaped before the session ended" >&2
    exit 1
}
end_session
wait_gone "$helper" 10 || {
    echo "the browser helper ($helper) outlived the session" >&2
    exit 1
}
echo "browser helper reaped with the session"

# A user quitting the Dock icon arrives here, not on the path above.
echo "case 2: a signal ends the session, the helper still goes"
launch_and_wait "$openpid" "$work/helper2.log"
kill "$whpid"
wait_gone "$whpid" 30 || {
    echo "webhttrack did not exit on SIGTERM" >&2
    exit 1
}
wait_gone "$helper" 10 || {
    echo "the browser helper ($helper) survived the signal path" >&2
    exit 1
}
echo "browser helper reaped on the signal path"

# x-www-browser heads webhttrack's browser list, so it is chosen over open here. Reaping
# is for the helper we asked to wait, never for a browser the user is reading in.
echo "case 3: a real browser is left alone"
browserargs="$work/browser.args"
browserpid="$work/browser.pid"
mkhelper "$browserstub" "$browserargs" "$browserpid"
launch_and_wait "$browserpid" "$work/helper3.log"
browser=$helper
grep -qxF -- -W "$browserargs" && {
    echo "the browser got -W, which belongs to open alone" >&2
    exit 1
}
end_session
kill -0 "$browser" 2>/dev/null || {
    echo "the user's browser ($browser) was reaped with the session" >&2
    exit 1
}
echo "a real browser is left running"
