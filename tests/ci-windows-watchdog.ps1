# Off-box watchdog for the Windows suite step (#795). The bash heartbeat beside it
# costs a fork per tick, and every recorded wedge is a box where forks stopped
# working, so nothing here spawns a process: .NET and CIM in-process only, and the
# telemetry leaves as a commit status, the one channel that outlives a dead runner.
param(
    [string]$ProgressLog = '',
    # Windows PID of the bash driver: what to kill, and whose exit ends this loop.
    [int]$DriverPid = 0,
    [int]$StuckSeconds = 900,
    [int]$IntervalSeconds = 30,
    [int]$PollSeconds = 5,
    [switch]$DryRun,
    [switch]$SelfTest
)

$ErrorActionPreference = 'Stop'
# Windows PowerShell renders a progress bar per web call otherwise.
$ProgressPreference = 'SilentlyContinue'

# --- decisions, kept pure so -SelfTest can drive them ------------------------

# Staticness, never elapsed time: a healthy long test and a wedged one look
# identical by the clock, but every outcome writes a progress line.
function Get-WatchdogAction {
    param([int]$Now, [int]$Moved, [int]$Posted, [int]$Stuck, [int]$Interval)
    if (($Now - $Moved) -ge $Stuck) { return 'kill' }
    if (($Now - $Posted) -ge $Interval) { return 'post' }
    return 'wait'
}

# Descendants of $Root, deepest first, from rows carrying ProcessId,
# ParentProcessId and CreationDate. Walks down only, so the runner's own
# processes are out of reach by construction -- killing Runner.Worker.exe
# reproduces this very failure. A candidate older than its parent is a recycled
# PID rather than a child, and $Self is dropped so the kill outlives the killer.
function Get-DescendantPids {
    param($Table, [int]$Root, [int]$Self = 0)
    $byParent = @{}
    $born = @{}
    foreach ($p in $Table) {
        $pp = [int]$p.ParentProcessId
        if (-not $byParent.ContainsKey($pp)) { $byParent[$pp] = New-Object System.Collections.ArrayList }
        [void]$byParent[$pp].Add($p)
        $born[[int]$p.ProcessId] = $p.CreationDate
    }
    $out = New-Object System.Collections.ArrayList
    $seen = @{ $Root = $true }
    $frontier = @([int]$Root)
    while ($frontier.Count -gt 0) {
        $next = New-Object System.Collections.ArrayList
        foreach ($parent in $frontier) {
            if (-not $byParent.ContainsKey($parent)) { continue }
            foreach ($child in $byParent[$parent]) {
                $cpid = [int]$child.ProcessId
                # 0 and 4 are Idle and System; $seen also closes a PPID cycle.
                if ($cpid -le 4 -or $seen.ContainsKey($cpid)) { continue }
                if ($null -ne $born[$parent] -and $null -ne $child.CreationDate -and $child.CreationDate -lt $born[$parent]) { continue }
                $seen[$cpid] = $true
                [void]$next.Add($cpid)
                [void]$out.Insert(0, $cpid)
            }
        }
        $frontier = $next
    }
    if ($Self -gt 0) { return @($out | Where-Object { $_ -ne $Self }) }
    return @($out)
}

# GitHub truncates a status description at 140 characters, so the fields a wedge
# is read for lead and the counters take the cut.
function Format-WatchdogStatus {
    param([int]$Elapsed, [int]$Static, [string]$InFlight, [string]$Counters)
    $t = ($InFlight -replace '\s+', ' ').Trim()
    if ($t.Length -gt 46) { $t = $t.Substring(0, 46) }
    $s = 't={0}s q={1}s {2} | {3}' -f $Elapsed, $Static, $t, $Counters
    if ($s.Length -gt 140) { $s = $s.Substring(0, 140) }
    return $s
}

# --- probes ------------------------------------------------------------------

# One try/catch per counter: a probe that fails must cost its own field, not the
# loop. Every value is in-process, none of them shells out.
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

# Tolerant share mode: the driver appends to this file while we read it.
function Get-ProgressTail {
    param([string]$Path)
    $r = @{ Signature = ''; Line = '' }
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
    } catch { }
    return $r
}

function Write-WatchdogLog {
    param([string]$Message)
    Write-Host ('[watchdog {0:HH:mm:ss}] {1}' -f (Get-Date), $Message)
}

# --- reporting ---------------------------------------------------------------

$script:Repo = $env:WATCHDOG_REPO
$script:Sha = $env:WATCHDOG_SHA
$script:Token = $env:WATCHDOG_TOKEN
$script:TargetUrl = $env:WATCHDOG_URL
$script:Context = $env:WATCHDOG_CONTEXT
if (-not $script:Context) { $script:Context = 'windows-suite-watchdog' }
$script:PostFailures = 0

# Fails soft: a fork PR gets a read-only token, and the kill is worth doing
# whether or not the telemetry can land.
function Send-WatchdogStatus {
    param([string]$State, [string]$Description)
    if (-not $script:Token -or -not $script:Repo -or -not $script:Sha) { return $false }
    $body = @{ state = $State; context = $script:Context; description = $Description }
    if ($script:TargetUrl) { $body['target_url'] = $script:TargetUrl }
    try {
        Invoke-RestMethod -Method Post -TimeoutSec 20 `
            -Uri ('https://api.github.com/repos/{0}/statuses/{1}' -f $script:Repo, $script:Sha) `
            -UserAgent 'httrack-windows-suite-watchdog' `
            -Headers @{
            Authorization = ('Bearer {0}' -f $script:Token)
            Accept        = 'application/vnd.github+json'
        } -ContentType 'application/json' -Body ($body | ConvertTo-Json -Compress) | Out-Null
        $script:PostFailures = 0
        return $true
    } catch {
        $script:PostFailures++
        if ($script:PostFailures -le 3) { Write-WatchdogLog ('status post failed: {0}' -f $_.Exception.Message) }
        return $false
    }
}

# --- the driver's process tree ------------------------------------------------

$script:DriverStart = $null

function Test-DriverAlive {
    if ($DriverPid -le 0) { return $true }
    try { $p = Get-Process -Id $DriverPid } catch { return $false }
    # Separately, so an unreadable StartTime does not read as a dead driver and
    # retire the watchdog for the rest of the step.
    try {
        if ($null -ne $script:DriverStart -and $p.StartTime -ne $script:DriverStart) { return $false }
    } catch { }
    return $true
}

function Stop-DriverTree {
    $kids = @()
    try {
        $table = Get-CimInstance -ClassName Win32_Process -OperationTimeoutSec 20 `
            -Property @('ProcessId', 'ParentProcessId', 'CreationDate')
        $kids = Get-DescendantPids -Table $table -Root $DriverPid -Self $PID
    } catch { Write-WatchdogLog ('process table unreadable: {0}' -f $_.Exception.Message) }
    Write-WatchdogLog ('killing {0} descendants then {1}' -f $kids.Count, $DriverPid)
    if ($DryRun) { Write-WatchdogLog ('dry run, would kill: {0}' -f ($kids -join ',')); return }
    foreach ($k in $kids) { try { Stop-Process -Id $k -Force } catch { } }
    # Idle and System; on a POSIX host pid 0 means the whole process group.
    if ($DriverPid -le 4) { return }
    try { Stop-Process -Id $DriverPid -Force } catch { }
}

# --- self-test ----------------------------------------------------------------

function Invoke-WatchdogSelfTest {
    $bad = New-Object System.Collections.ArrayList
    function Assert-That($cond, $what) { if (-not $cond) { [void]$bad.Add($what) } }

    Assert-That ((Get-WatchdogAction 900 0 0 900 30) -eq 'kill') 'staticness exactly at the threshold does not kill'
    Assert-That ((Get-WatchdogAction 5000 0 0 900 30) -eq 'kill') 'a long-static log does not kill'
    Assert-That ((Get-WatchdogAction 899 0 0 900 30) -eq 'post') 'a post due one second short of the threshold was skipped'
    Assert-That ((Get-WatchdogAction 899 0 890 900 30) -eq 'wait') 'posted off cadence'
    Assert-That ((Get-WatchdogAction 5000 4200 5000 900 30) -eq 'wait') 'a suite still reporting was not left alone'

    # 100 is the driver, 50 its parent, 400 this watchdog, 500 a recycled PID
    # claiming 100 as parent while predating it, 800/900 a PPID cycle back to 100.
    $t0 = Get-Date '2026-01-01T00:00:00Z'
    $table = @(
        [pscustomobject]@{ ProcessId = 50; ParentProcessId = 40; CreationDate = $t0 },
        [pscustomobject]@{ ProcessId = 100; ParentProcessId = 50; CreationDate = $t0.AddSeconds(10) },
        [pscustomobject]@{ ProcessId = 200; ParentProcessId = 100; CreationDate = $t0.AddSeconds(20) },
        [pscustomobject]@{ ProcessId = 300; ParentProcessId = 200; CreationDate = $t0.AddSeconds(30) },
        [pscustomobject]@{ ProcessId = 400; ParentProcessId = 100; CreationDate = $t0.AddSeconds(25) },
        [pscustomobject]@{ ProcessId = 500; ParentProcessId = 100; CreationDate = $t0.AddSeconds(1) },
        [pscustomobject]@{ ProcessId = 800; ParentProcessId = 100; CreationDate = $t0.AddSeconds(40) },
        [pscustomobject]@{ ProcessId = 900; ParentProcessId = 800; CreationDate = $t0.AddSeconds(50) },
        [pscustomobject]@{ ProcessId = 100; ParentProcessId = 900; CreationDate = $t0.AddSeconds(10) }
    )
    $kids = @(Get-DescendantPids -Table $table -Root 100 -Self 400)
    Assert-That ($kids -contains 200) 'a direct child was not selected'
    Assert-That ($kids -contains 300) 'a grandchild was not selected'
    Assert-That (-not ($kids -contains 400)) 'the watchdog would kill itself before its target'
    Assert-That (-not ($kids -contains 500)) 'a process predating its claimed parent was selected'
    Assert-That (-not ($kids -contains 50)) 'the walk reached the target parent'
    Assert-That (-not ($kids -contains 40)) 'the walk reached a grandparent'
    Assert-That (-not ($kids -contains 100)) 'the target came back as its own descendant'
    $i200 = [array]::IndexOf($kids, 200)
    $i300 = [array]::IndexOf($kids, 300)
    Assert-That ($i300 -ge 0 -and $i200 -ge 0 -and $i300 -lt $i200) 'the tree is not killed deepest first'
    Assert-That ((@(Get-DescendantPids -Table $table -Root 300 -Self 400)).Count -eq 0) 'a leaf reported descendants'

    $long = '43_local-update-truncate-with-a-very-long-name-indeed.test'
    $line = Format-WatchdogStatus 812 41 $long 'm=9012 v=14022 p=118 h=41230 d=13210'
    Assert-That ($line.Length -le 140) ('status description is {0} characters' -f $line.Length)
    Assert-That ($line -like 't=812s q=41s 43_local-update-truncate*') ('status leads with the wrong fields: {0}' -f $line)
    Assert-That ($line -like '*d=13210') 'the counters did not survive a long test name'
    $wide = Format-WatchdogStatus 1 2 ('x' * 300) ('y' * 300)
    Assert-That ($wide.Length -le 140) ('an oversized status was not clipped: {0}' -f $wide.Length)

    $tail = Get-ProgressTail -Path ('no-such-progress-log-{0}.tmp' -f $PID)
    Assert-That ($tail.Line -eq '') 'a missing progress log was not tolerated'

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
try { $script:DriverStart = (Get-Process -Id $DriverPid).StartTime } catch { }

$sw = [System.Diagnostics.Stopwatch]::StartNew()
$lastSig = ''
$movedAt = 0
# Negative, so the first tick posts: a status that lands early is itself the
# datum that the step started.
$postedAt = -$IntervalSeconds
$killed = $false

Write-WatchdogLog ('watching {0} for pid {1}, kill after {2}s static' -f $ProgressLog, $DriverPid, $StuckSeconds)

while ($true) {
    # Measured off a stopwatch, never accumulated from what the sleeps asked for:
    # starvation is exactly what makes a sleep overshoot.
    $now = [int]$sw.Elapsed.TotalSeconds
    try {
        if (-not (Test-DriverAlive)) { break }
        $tail = Get-ProgressTail -Path $ProgressLog
        if ($tail.Signature -ne $lastSig) { $lastSig = $tail.Signature; $movedAt = $now }
        $action = Get-WatchdogAction $now $movedAt $postedAt $StuckSeconds $IntervalSeconds
        if ($action -eq 'kill') {
            $desc = Format-WatchdogStatus $now ($now - $movedAt) $tail.Line (Get-WatchdogCounters)
            # No prefix: the state says it, and 140 characters is the whole budget.
            [void](Send-WatchdogStatus 'failure' $desc)
            Write-WatchdogLog ('killing the step: ' + $desc)
            Stop-DriverTree
            $killed = $true
            break
        }
        if ($action -eq 'post') {
            $postedAt = $now
            $desc = Format-WatchdogStatus $now ($now - $movedAt) $tail.Line (Get-WatchdogCounters)
            [void](Send-WatchdogStatus 'pending' $desc)
            Write-WatchdogLog $desc
        }
    } catch {
        Write-WatchdogLog ('tick failed: {0}' -f $_.Exception.Message)
    }
    Start-Sleep -Seconds $PollSeconds
}

if (-not $killed) {
    # Resolves the pending status, which would otherwise sit on the commit for good.
    [void](Send-WatchdogStatus 'success' ('suite step ended after {0}s' -f [int]$sw.Elapsed.TotalSeconds))
    Write-WatchdogLog 'driver gone, exiting'
}
