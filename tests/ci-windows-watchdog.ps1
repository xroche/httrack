# Off-box telemetry for the Windows suite step (#795): it spawns and kills
# nothing, so a wedge cannot disarm it, and reports as a commit status, the only
# channel that outlives a dead runner. Every status carries the same state.
param(
    [string]$ProgressLog = '',
    # 15s, not 30: a lost runner dies inside a single status period (#1228).
    [int]$IntervalSeconds = 15,
    [int]$PollSeconds = 5,
    # Cannot outlive the step, whatever the caller forgets to kill.
    [int]$MaxSeconds = 2700,
    # Seam for the suite's own test, which points it at a sink it can count.
    [string]$ApiBase = 'https://api.github.com',
    [switch]$NoPost,
    [switch]$SelfTest
)

$ErrorActionPreference = 'Stop'
# Windows PowerShell renders a progress bar per web call otherwise.
$ProgressPreference = 'SilentlyContinue'

# --- decisions, kept pure so -SelfTest can drive them by return value ---------

function Get-WatchdogAction {
    param([int]$Now, [int]$Posted, [int]$Interval)
    if (($Now - $Posted) -ge $Interval) { return 'post' }
    return 'wait'
}

# Posts to skip after a rejected one: a fork PR's token is read-only for the run.
function Get-NextBackoff {
    param([int]$Current)
    if ($Current -lt 1) { return 1 }
    return [Math]::Min($Current * 2, 32)
}

# The (skip, backoff) pair after an attempted post, $Ok being whether it landed:
# a rejection that later resolves has to leave nothing behind.
function Get-NextThrottle {
    param([bool]$Ok, [int]$Backoff)
    if ($Ok) { return @(0, 0) }
    $b = Get-NextBackoff $Backoff
    return @($b, $b)
}

# GitHub truncates a description at 140 chars, so the counters take the cut, not
# the fields a wedge is read for. $Static below zero is unknown.
function Format-WatchdogStatus {
    param([int]$Elapsed, [int]$Static, [string]$InFlight, [string]$Counters)
    $q = '?'
    if ($Static -ge 0) { $q = [string]$Static }
    $t = ($InFlight -replace '\s+', ' ').Trim()
    if ($t.Length -gt 30) { $t = $t.Substring(0, 30) }
    $s = 't={0}s q={1}s {2} | {3}' -f $Elapsed, $q, $t, $Counters
    if ($s.Length -gt 140) { $s = $s.Substring(0, 140) }
    return $s
}

# The worse of the running peak and how much longer the last iteration took than
# the poll it asked for. Peak, not last: a status covers several iterations.
function Get-MaxLag {
    param([int]$Peak, [double]$Elapsed, [double]$Since, [int]$Poll)
    $lag = [int]($Elapsed - $Since - $Poll * 1000)
    if ($lag -gt $Peak) { return $lag }
    return $Peak
}

# The TCP counters are cumulative since boot, so only the change over a status
# period says what the suite did; $Prev is $null on the first sample.
function Get-TcpDelta {
    param($Prev, $Cur)
    if ($null -eq $Prev) { return 'n=? f=?' }
    return 'n={0} f={1}' -f ($Cur[0] - $Prev[0]), ($Cur[1] - $Prev[1])
}

# --- probes ------------------------------------------------------------------

$script:LastTcp = $null

# One try/catch per counter: a probe that fails costs its own field, not the loop.
# In-process only. A CIM query is richer, but its connect to a wedged WMI service
# is unbounded, and would hang the one reporter still standing.
function Get-WatchdogCounters {
    param([int]$LagMs = 0, [int]$Failed = 0)
    $f = New-Object System.Collections.ArrayList
    $ps = @()
    try {
        $ps = @(Get-Process)
        [void]$f.Add('p={0}' -f $ps.Count)
        [void]$f.Add('h={0}' -f (($ps | Measure-Object -Property Handles -Sum).Sum))
    } catch { [void]$f.Add('p=? h=?') }
    try {
        $drive = New-Object System.IO.DriveInfo($env:SystemDrive + '\')
        [void]$f.Add('d={0}' -f [int]($drive.AvailableFreeSpace / 1GB))
    } catch { [void]$f.Add('d=?') }
    # The ramp detector: starvation is what makes a poll overshoot.
    [void]$f.Add('l={0}' -f $LagMs)
    # Box-stop against network-break: the status that lands after an outage says
    # how many it swallowed, and a box that stopped never lands one.
    [void]$f.Add('x={0}' -f $Failed)
    try {
        # One GetTcpStatisticsEx; GetActiveTcpConnections() would allocate per socket.
        $t = [System.Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetTcpIPv4Statistics()
        $cur = @($t.ConnectionsInitiated, ($t.FailedConnectionAttempts + $t.ResetConnections))
        [void]$f.Add((Get-TcpDelta $script:LastTcp $cur))
        $script:LastTcp = $cur
        [void]$f.Add('e={0}' -f $t.CurrentConnections)
    } catch { [void]$f.Add('n=? f=? e=?') }
    try {
        if ($ps.Count -lt 1) { throw 'no process list' }
        [void]$f.Add('m={0}' -f [int]((($ps | Measure-Object -Property WorkingSet64 -Sum).Sum) / 1MB))
        [void]$f.Add('c={0}' -f [int]((($ps | Measure-Object -Property PagedMemorySize64 -Sum).Sum) / 1MB))
        # Its own field: the agent is what stops reporting, and the box total hides it.
        $agent = @($ps | Where-Object { $_.Name -eq 'Runner.Worker' })
        $ws = 0
        if ($agent.Count -gt 0) { $ws = [int]((($agent | Measure-Object -Property WorkingSet64 -Sum).Sum) / 1MB) }
        [void]$f.Add('a={0}' -f $ws)
    } catch { [void]$f.Add('m=? c=? a=?') }
    return ($f -join ' ')
}

# Ok separates "nothing moved" from "could not read it", which would otherwise report
# a wedge for an unreadable file. Share flags: the driver appends as we read.
function Get-ProgressTail {
    param([string]$Path)
    $r = @{ Ok = $false; Signature = ''; Line = '' }
    if (-not $Path) { return $r }
    try {
        $share = [System.IO.FileShare]::ReadWrite -bor [System.IO.FileShare]::Delete
        $fs = New-Object System.IO.FileStream($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, $share)
        try {
            $sr = New-Object System.IO.StreamReader($fs)
            $text = $sr.ReadToEnd()
        } finally { $fs.Dispose() }
        $lines = @($text -split "`r?`n" | Where-Object { $_ -ne '' })
        if ($lines.Count -gt 0) { $r.Line = $lines[-1] }
        $r.Signature = '{0}|{1}' -f $text.Length, $r.Line
        $r.Ok = $true
    } catch { }
    return $r
}

function Write-WatchdogLog {
    param([string]$Message)
    # A full disk is a state the sick runner reaches, and a log line lost to it
    # must not take the loop reporting off-box with it.
    try { Write-Host ('[watchdog {0:HH:mm:ss}] {1}' -f (Get-Date), $Message) } catch { }
}

# --- reporting ---------------------------------------------------------------

$script:Repo = $env:WATCHDOG_REPO
$script:Sha = $env:WATCHDOG_SHA
$script:Token = $env:WATCHDOG_TOKEN
$script:TargetUrl = $env:WATCHDOG_URL
$script:Context = $env:WATCHDOG_CONTEXT
if (-not $script:Context) { $script:Context = 'windows-suite-watchdog' }

# One state always, so nothing downstream can read a verdict out of telemetry:
# GitHub records the job's conclusion already. -NoPost is the test's hard stop,
# whatever the environment holds.
function Send-WatchdogStatus {
    param([string]$Description)
    if ($NoPost -or $SelfTest) { return $false }
    if (-not $script:Token -or -not $script:Repo -or -not $script:Sha) { return $false }
    $body = @{ state = 'success'; context = $script:Context; description = $Description }
    if ($script:TargetUrl) { $body['target_url'] = $script:TargetUrl }
    try {
        Invoke-RestMethod -Method Post -TimeoutSec 20 `
            -Uri ('{0}/repos/{1}/statuses/{2}' -f $ApiBase.TrimEnd('/'), $script:Repo, $script:Sha) `
            -UserAgent 'httrack-windows-suite-watchdog' `
            -Headers @{
            Authorization = ('Bearer {0}' -f $script:Token)
            Accept        = 'application/vnd.github+json'
        } -ContentType 'application/json' -Body ($body | ConvertTo-Json -Compress) | Out-Null
        return $true
    } catch {
        Write-WatchdogLog ('status post failed: {0}' -f $_.Exception.Message)
        return $false
    }
}

# --- self-test ----------------------------------------------------------------

function Invoke-WatchdogSelfTest {
    $bad = New-Object System.Collections.ArrayList
    function Assert-That($cond, $what) { if (-not $cond) { [void]$bad.Add($what) } }

    Assert-That ((Get-WatchdogAction 30 0 30) -eq 'post') 'a post due exactly on the interval was skipped'
    Assert-That ((Get-WatchdogAction 29 0 30) -eq 'wait') 'posted ahead of the interval'
    Assert-That ((Get-WatchdogAction 5000 4990 30) -eq 'wait') 'posted off cadence'

    Assert-That ((Get-NextBackoff 0) -eq 1) 'the first rejection does not back off'
    Assert-That ((Get-NextBackoff 1) -eq 2) 'the backoff does not grow'
    Assert-That ((Get-NextBackoff 32) -eq 32) 'the backoff is not capped'

    $ok = Get-NextThrottle $true 8
    Assert-That ($ok[0] -eq 0 -and $ok[1] -eq 0) 'a landed post leaves the run throttled'
    $ko = Get-NextThrottle $false 0
    Assert-That ($ko[0] -eq 1 -and $ko[1] -eq 1) 'a first rejection skips nothing'
    $ko = Get-NextThrottle $false 4
    Assert-That ($ko[0] -eq 8 -and $ko[1] -eq 8) 'a repeat rejection does not widen the gap'

    $long = '43_local-update-truncate-with-a-very-long-name-indeed.test'
    # The widest real counter line, so a status that fits here fits on the runner.
    $line = Format-WatchdogStatus 2700 2700 $long 'p=201 h=54598 d=85 l=120 x=0 n=412 f=0 e=180 m=3100 c=4200 a=210'
    Assert-That ($line.Length -le 140) ('status description is {0} characters' -f $line.Length)
    Assert-That ($line -like 't=2700s q=2700s 43_local-update-truncate*') ('status leads with the wrong fields: {0}' -f $line)
    Assert-That ($line -like '*a=210') 'the counters did not survive a long test name'
    # -match, not -like: '?' is a wildcard there, so q=0s would satisfy it too.
    Assert-That ((Format-WatchdogStatus 8 -1 'x' 'y') -match '^t=8s q=\?s x \| y$') 'an unknown staticness reads as a number'
    $clip = Format-WatchdogStatus 1 2 ('x' * 80) 'c'
    Assert-That ($clip -match '^t=1s q=2s x{30} \| c$') ('the in-flight name was not clipped to 30: {0}' -f $clip)
    $wide = Format-WatchdogStatus 1 2 ('x' * 300) ('y' * 300)
    Assert-That ($wide.Length -le 140) ('an oversized status was not clipped: {0}' -f $wide.Length)
    # Cut from the tail: the head carries the fields a wedge is read for.
    Assert-That ($wide -like 't=1s q=2s x*') ('clipping dropped the leading fields: {0}' -f $wide)

    $gone = Get-ProgressTail -Path ('no-such-progress-log-{0}.tmp' -f $PID)
    Assert-That (-not $gone.Ok) 'an unreadable log reads as one that was read'
    $f = New-Object System.IO.FileInfo([System.IO.Path]::GetTempFileName())
    try {
        [System.IO.File]::WriteAllText($f.FullName, "first`nRUN 42_probe.test at 7s`n")
        $tail = Get-ProgressTail -Path $f.FullName
        Assert-That ($tail.Ok) 'a readable log reads as unreadable'
        Assert-That ($tail.Line -eq 'RUN 42_probe.test at 7s') ('the tail is not the last line: {0}' -f $tail.Line)
    } finally { [System.IO.File]::Delete($f.FullName) }

    # Space-separated key=value: the counters share the 140-char description with
    # the fields a wedge is read for, and '?' from a failed probe is a value.
    $c = Get-WatchdogCounters 120 3
    Assert-That ($c -match '^[a-z]+=\S+( [a-z]+=\S+)*$') ('the counters are not key=value pairs: {0}' -f $c)
    foreach ($k in 'p', 'h', 'd', 'l', 'x', 'n', 'f', 'e', 'm', 'c', 'a') {
        Assert-That ($c -match ('(^| ){0}=' -f $k)) ('the counters dropped {0}=: {1}' -f $k, $c)
    }
    Assert-That ($c -match '(^| )l=120( |$)') ('the loop lag is not what the caller measured: {0}' -f $c)
    Assert-That ($c -match '(^| )x=3( |$)') ('the failed-post count is not what the caller passed: {0}' -f $c)
    Assert-That ((Get-MaxLag 0 6200 1000 5) -eq 200) 'a poll that overshot by 200ms was not measured'
    Assert-That ((Get-MaxLag 500 6200 1000 5) -eq 500) 'a smaller lag replaced the peak'
    Assert-That ((Get-MaxLag 0 5900 1000 5) -eq 0) 'a poll that returned early reported a lag'
    Assert-That ((Get-TcpDelta $null @(70, 9)) -eq 'n=? f=?') 'a first sample with no predecessor reported a delta'
    Assert-That ((Get-TcpDelta @(64, 7) @(70, 9)) -eq 'n=6 f=2') 'a total was reported where the delta was asked for'
    # 140 less the 16 of t=/q= and the 33 a clipped test name and its separator take.
    Assert-That ($c.Length -le 91) ('the counters take {0} of the 140 characters' -f $c.Length)

    # Nothing else reads these: every other leg passes its own schedule.
    Assert-That ($IntervalSeconds -eq 15) ('the default status cadence is {0}s' -f $IntervalSeconds)
    Assert-That ($PollSeconds -eq 5) ('the default poll is {0}s' -f $PollSeconds)

    Assert-That (-not (Send-WatchdogStatus 'self-test')) 'the self-test can reach the API'

    if ($bad.Count -gt 0) {
        foreach ($b in $bad) { Write-Host ('self-test FAIL: {0}' -f $b) }
        exit 1
    }
    Write-Host 'watchdog self-test OK'
    exit 0
}

if ($SelfTest) { Invoke-WatchdogSelfTest }

# --- main loop ----------------------------------------------------------------

# Windows PowerShell still defaults below TLS 1.2, which api.github.com refuses.
try { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12 } catch { }

$sw = [System.Diagnostics.Stopwatch]::StartNew()
$lastSig = ''
$movedAt = 0
# Negative, so the first tick posts: an early status is itself a datum.
$postedAt = -$IntervalSeconds
$backoff = 0
$skip = 0
$lagMax = 0
$failed = 0
$tickAt = $sw.Elapsed.TotalMilliseconds

# Guarded like the rest; the launcher waits for this exact line.
try { Write-Host 'watchdog ready' } catch { }
Write-WatchdogLog ('watching {0} every {1}s' -f $ProgressLog, $IntervalSeconds)

while ($sw.Elapsed.TotalSeconds -lt $MaxSeconds) {
    # Measured, never accumulated: starvation is what makes a sleep overshoot.
    $ms = $sw.Elapsed.TotalMilliseconds
    $now = [int]($ms / 1000)
    # Measured at the top, so the lag covers the probes and the post as well as
    # the sleep: starvation stretches all three.
    $lagMax = Get-MaxLag $lagMax $ms $tickAt $PollSeconds
    $tickAt = $ms
    try {
        $tail = Get-ProgressTail -Path $ProgressLog
        if ($tail.Ok -and $tail.Signature -ne $lastSig) {
            $lastSig = $tail.Signature
            $movedAt = $now
        }
        if ((Get-WatchdogAction $now $postedAt $IntervalSeconds) -eq 'post') {
            $postedAt = $now
            $static = -1
            if ($tail.Ok) { $static = $now - $movedAt }
            $desc = Format-WatchdogStatus $now $static $tail.Line (Get-WatchdogCounters $lagMax $failed)
            # Logged whatever the backoff decides: it throttles the API, not the
            # artifact, which is all a run whose token cannot post will leave.
            Write-WatchdogLog $desc
            if ($skip -gt 0) {
                $skip--
            } else {
                $ok = Send-WatchdogStatus $desc
                # Cleared together, and only by a status that landed: a peak reached
                # while nothing was getting through is what the next one has to carry.
                if ($ok) { $failed = 0; $lagMax = 0 } else { $failed++ }
                $next = Get-NextThrottle $ok $backoff
                $skip = $next[0]
                $backoff = $next[1]
            }
        }
    } catch {
        Write-WatchdogLog ('tick failed: {0}' -f $_.Exception.Message)
    }
    Start-Sleep -Seconds $PollSeconds
}

Write-WatchdogLog ('stopping after {0}s' -f [int]$sw.Elapsed.TotalSeconds)
