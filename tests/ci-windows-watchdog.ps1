# Off-box telemetry for the Windows suite step (#795). It spawns nothing, because
# the wedge is a box where forks stopped working, and it reports as a commit
# status, the only channel that outlives a dead runner. It never kills.
param(
    [string]$ProgressLog = '',
    [int]$IntervalSeconds = 30,
    [int]$PollSeconds = 5,
    # Cannot outlive the step, whatever the caller forgets to kill.
    [int]$MaxSeconds = 2700,
    # One-shot mode: post this state and exit, so the driver reports its own end.
    [string]$Post = '',
    [string]$Message = '',
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

# GitHub truncates a description at 140 chars, so the counters take the cut, not
# the fields a wedge is read for. $Static below zero is unknown.
function Format-WatchdogStatus {
    param([int]$Elapsed, [int]$Static, [string]$InFlight, [string]$Counters)
    $q = '?'
    if ($Static -ge 0) { $q = [string]$Static }
    $t = ($InFlight -replace '\s+', ' ').Trim()
    if ($t.Length -gt 46) { $t = $t.Substring(0, 46) }
    $s = 't={0}s q={1}s {2} | {3}' -f $Elapsed, $q, $t, $Counters
    if ($s.Length -gt 140) { $s = $s.Substring(0, 140) }
    return $s
}

# --- probes ------------------------------------------------------------------

# One try/catch per counter: a probe that fails costs its own field, not the loop.
function Get-WatchdogCounters {
    $f = New-Object System.Collections.ArrayList
    try {
        $os = Get-CimInstance -ClassName Win32_OperatingSystem -OperationTimeoutSec 5
        [void]$f.Add('m={0}' -f [int]($os.FreePhysicalMemory / 1KB))
        [void]$f.Add('v={0}' -f [int]($os.FreeVirtualMemory / 1KB))
    } catch { [void]$f.Add('m=? v=?') }
    try {
        $ps = @(Get-Process)
        [void]$f.Add('p={0}' -f $ps.Count)
        [void]$f.Add('h={0}' -f (($ps | Measure-Object -Property Handles -Sum).Sum))
    } catch { [void]$f.Add('p=? h=?') }
    try {
        $drive = New-Object System.IO.DriveInfo($env:SystemDrive + '\')
        [void]$f.Add('d={0}' -f [int]($drive.AvailableFreeSpace / 1MB))
    } catch { [void]$f.Add('d=?') }
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

# -NoPost is the test's hard stop: no request, whatever the environment holds.
function Send-WatchdogStatus {
    param([string]$State, [string]$Description)
    if ($NoPost -or $SelfTest) { return $false }
    if (-not $script:Token -or -not $script:Repo -or -not $script:Sha) { return $false }
    $body = @{ state = $State; context = $script:Context; description = $Description }
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

    $long = '43_local-update-truncate-with-a-very-long-name-indeed.test'
    $line = Format-WatchdogStatus 812 41 $long 'm=9012 v=14022 p=118 h=41230 d=13210'
    Assert-That ($line.Length -le 140) ('status description is {0} characters' -f $line.Length)
    Assert-That ($line -like 't=812s q=41s 43_local-update-truncate*') ('status leads with the wrong fields: {0}' -f $line)
    Assert-That ($line -like '*d=13210') 'the counters did not survive a long test name'
    # -match, not -like: '?' is a wildcard there, so q=0s would satisfy it too.
    Assert-That ((Format-WatchdogStatus 8 -1 'x' 'y') -match '^t=8s q=\?s x \| y$') 'an unknown staticness reads as a number'
    $clip = Format-WatchdogStatus 1 2 ('x' * 80) 'c'
    Assert-That ($clip -match '^t=1s q=2s x{46} \| c$') ('the in-flight name was not clipped to 46: {0}' -f $clip)
    $wide = Format-WatchdogStatus 1 2 ('x' * 300) ('y' * 300)
    Assert-That ($wide.Length -le 140) ('an oversized status was not clipped: {0}' -f $wide.Length)

    $gone = Get-ProgressTail -Path ('no-such-progress-log-{0}.tmp' -f $PID)
    Assert-That (-not $gone.Ok) 'an unreadable log reads as one that was read'
    $f = New-Object System.IO.FileInfo([System.IO.Path]::GetTempFileName())
    try {
        [System.IO.File]::WriteAllText($f.FullName, "first`nRUN 42_probe.test at 7s`n")
        $tail = Get-ProgressTail -Path $f.FullName
        Assert-That ($tail.Ok) 'a readable log reads as unreadable'
        Assert-That ($tail.Line -eq 'RUN 42_probe.test at 7s') ('the tail is not the last line: {0}' -f $tail.Line)
    } finally { [System.IO.File]::Delete($f.FullName) }

    Assert-That (-not (Send-WatchdogStatus 'success' 'self-test')) 'the self-test can reach the API'

    if ($bad.Count -gt 0) {
        foreach ($b in $bad) { Write-Host ('self-test FAIL: {0}' -f $b) }
        exit 1
    }
    Write-Host 'watchdog self-test OK'
    exit 0
}

if ($SelfTest) { Invoke-WatchdogSelfTest }

# --- one-shot: the driver's own verdict ---------------------------------------

# Windows PowerShell still defaults below TLS 1.2, which api.github.com refuses.
try { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12 } catch { }

if ($Post) {
    # One transient 5xx here is the difference between a resolved status and a
    # finished suite left reading `pending`.
    for ($try = 1; $try -le 3; $try++) {
        if (Send-WatchdogStatus $Post $Message) { break }
        if ($NoPost -or -not $script:Token) { break }
        Start-Sleep -Seconds (2 * $try)
    }
    Write-WatchdogLog ('final status {0}: {1}' -f $Post, $Message)
    exit 0
}

# --- main loop ----------------------------------------------------------------

$sw = [System.Diagnostics.Stopwatch]::StartNew()
$lastSig = ''
$movedAt = 0
# Negative, so the first tick posts: an early status is itself a datum.
$postedAt = -$IntervalSeconds
$backoff = 0
$skip = 0

# Guarded like the rest; the launcher waits for this exact line.
try { Write-Host 'watchdog ready' } catch { }
Write-WatchdogLog ('watching {0} every {1}s' -f $ProgressLog, $IntervalSeconds)

while ($sw.Elapsed.TotalSeconds -lt $MaxSeconds) {
    # Measured, never accumulated: starvation is what makes a sleep overshoot.
    $now = [int]$sw.Elapsed.TotalSeconds
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
            $desc = Format-WatchdogStatus $now $static $tail.Line (Get-WatchdogCounters)
            # Logged whatever the backoff decides: it throttles the API, not the
            # artifact, which is all a run whose token cannot post will leave.
            Write-WatchdogLog $desc
            if ($skip -gt 0) {
                $skip--
            } elseif (Send-WatchdogStatus 'pending' $desc) {
                $backoff = 0
            } else {
                $backoff = Get-NextBackoff $backoff
                $skip = $backoff
            }
        }
    } catch {
        Write-WatchdogLog ('tick failed: {0}' -f $_.Exception.Message)
    }
    Start-Sleep -Seconds $PollSeconds
}

Write-WatchdogLog ('stopping after {0}s' -f [int]$sw.Elapsed.TotalSeconds)
