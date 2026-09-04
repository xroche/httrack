# Sockets arm of the Windows runner-kill probe (#1228): loopback connections
# with no MSYS process anywhere, so the dose differs from tools/kill-probe-arm.sh
# in kind and not in size.
$ErrorActionPreference = "Stop"

$budget = [int]$env:PROBE_EVENTS
$seconds = [int]$env:PROBE_SECONDS
if ($budget -le 0 -or $seconds -le 0) { throw "PROBE_EVENTS and PROBE_SECONDS must be set" }

# Started here, not in an earlier step: a background process on a Windows runner
# does not survive the step that started it.
$root = "$env:RUNNER_TEMP\probe-root"
New-Item -ItemType Directory -Force -Path $root | Out-Null
Set-Content -Path "$root\index.html" -Value "<html>probe</html>"
$log = "$env:RUNNER_TEMP\probe-server.log"
$server = Start-Process -FilePath python -PassThru `
    -ArgumentList @("tests/local-server.py", "--root", $root) `
    -RedirectStandardOutput $log -RedirectStandardError "$env:RUNNER_TEMP\probe-server.err"
$port = $null
foreach ($i in 1..60) {
    if (Test-Path $log) {
        $m = Select-String -Path $log -Pattern '^PORT (\d+)' | Select-Object -First 1
        if ($m) { $port = [int]$m.Matches[0].Groups[1].Value; break }
    }
    if ($server.HasExited) { break }
    Start-Sleep -Milliseconds 500
}
if (-not $port) { throw "the loopback server never announced a port" }

# Paced, unlike the bash arms: nothing here throttles the loop, and a flat-out
# run would exhaust the ephemeral range and measure its own port starvation.
$rate = 40
$req = [Text.Encoding]::ASCII.GetBytes("GET / HTTP/1.0`r`nHost: 127.0.0.1`r`n`r`n")
$began = Get-Date
$deadline = $began.AddSeconds($seconds)
$conns = 0
$errors = 0
try {
    while ($conns -lt $budget -and (Get-Date) -lt $deadline) {
        $c = $null
        try {
            # Bounded on every leg: an unbounded read parks the step until the job
            # times out, which the census would then read as a lost runner.
            $c = [Net.Sockets.TcpClient]::new()
            if (-not $c.ConnectAsync("127.0.0.1", $port).Wait(5000)) { throw "connect timed out" }
            $s = $c.GetStream()
            $s.WriteTimeout = 5000
            $s.ReadTimeout = 5000
            $s.Write($req, 0, $req.Length)
            $s.ReadByte() | Out-Null
        } catch {
            $errors++
        } finally {
            if ($c) { $c.Dispose() }
        }
        $conns++
        $ahead = $conns / $rate - ((Get-Date) - $began).TotalSeconds
        if ($ahead -gt 0) { Start-Sleep -Milliseconds ([int]($ahead * 1000)) }
    }
} finally {
    if (-not $server.HasExited) { Stop-Process -Id $server.Id -Force -ErrorAction SilentlyContinue }
}

$elapsed = [int]((Get-Date) - $began).TotalSeconds
"arm=sockets procs=0 conns=$conns errors=$errors attempts=$conns elapsed=${elapsed}s"
# Against attempts, never against the budget: a failure that is slow spends few
# events, so a budget-relative tolerance passes an arm where everything failed.
if ($conns -eq 0 -or $errors -gt $conns / 20) {
    throw "$errors of $conns attempts failed: dose not delivered"
}
