# Spike helper: the Windows half of the WSL2 kill prototype (#1228). Every mode
# prints machine-readable KEY=value lines so the bash side can parse them.
param(
    [Parameter(Mandatory = $true)][ValidateSet('find', 'findslow', 'snapshot', 'alive', 'kill', 'victim')]
    [string]$Mode,
    [string]$Marker = '',
    [string]$OutDir = '',
    [string]$ChildTag = '',
    [int]$Children = 0,
    [string]$Pids = ''
)

$ErrorActionPreference = 'Continue'

function Get-PidList([string]$s) {
    if ([string]::IsNullOrWhiteSpace($s)) { return @() }
    return @($s -split ',' | Where-Object { $_ -ne '' } | ForEach-Object { [int]$_ })
}

switch ($Mode) {

    # One WQL query, filtered by the provider. $PID is this very process, which
    # carries the marker on its own command line and must never be a candidate.
    'find' {
        $sw = [Diagnostics.Stopwatch]::StartNew()
        $procs = @(Get-CimInstance Win32_Process -Filter "CommandLine LIKE '%$Marker%'")
        $ms = $sw.Elapsed.TotalMilliseconds
        Write-Host ("QUERY_MS={0:F1}" -f $ms)
        foreach ($p in $procs) {
            if ($p.ProcessId -eq $PID) { Write-Host "SELF=$($p.ProcessId)"; continue }
            Write-Host "HIT=$($p.ProcessId) PPID=$($p.ParentProcessId) NAME=$($p.Name)"
        }
    }

    # Unfiltered enumeration plus a client-side match, for cost comparison.
    'findslow' {
        $sw = [Diagnostics.Stopwatch]::StartNew()
        $all = @(Get-CimInstance Win32_Process)
        $ms1 = $sw.Elapsed.TotalMilliseconds
        $procs = @($all | Where-Object { $_.CommandLine -and $_.CommandLine.Contains($Marker) })
        $ms = $sw.Elapsed.TotalMilliseconds
        Write-Host ("ENUM_MS={0:F1} TOTAL_MS={1:F1} COUNT={2}" -f $ms1, $ms, $all.Count)
        foreach ($p in $procs) {
            if ($p.ProcessId -eq $PID) { continue }
            Write-Host "HIT=$($p.ProcessId) PPID=$($p.ParentProcessId) NAME=$($p.Name)"
        }
    }

    # Marker match and the descendants of what it matched, out of ONE snapshot:
    # a child spawned by the exe need not carry the marker itself.
    'snapshot' {
        $sw = [Diagnostics.Stopwatch]::StartNew()
        $all = @(Get-CimInstance Win32_Process)
        $ms = $sw.Elapsed.TotalMilliseconds
        Write-Host ("SNAPSHOT_MS={0:F1} COUNT={1}" -f $ms, $all.Count)
        $roots = @($all | Where-Object { $_.ProcessId -ne $PID -and $_.CommandLine -and $_.CommandLine.Contains($Marker) })
        foreach ($r in $roots) { Write-Host "ROOT=$($r.ProcessId) NAME=$($r.Name)" }
        $byParent = @{}
        foreach ($p in $all) {
            $k = [string]$p.ParentProcessId
            if (-not $byParent.ContainsKey($k)) { $byParent[$k] = New-Object System.Collections.ArrayList }
            [void]$byParent[$k].Add($p)
        }
        $queue = New-Object System.Collections.Queue
        foreach ($r in $roots) { $queue.Enqueue($r) }
        $seen = @{}
        while ($queue.Count -gt 0) {
            $cur = $queue.Dequeue()
            $k = [string]$cur.ProcessId
            if ($seen.ContainsKey($k)) { continue }
            $seen[$k] = $true
            if ($byParent.ContainsKey($k)) {
                foreach ($c in $byParent[$k]) {
                    # A recycled pid can point at a parent younger than itself.
                    if ($c.CreationDate -and $cur.CreationDate -and $c.CreationDate -lt $cur.CreationDate) { continue }
                    Write-Host "DESC=$($c.ProcessId) PPID=$($c.ParentProcessId) NAME=$($c.Name)"
                    $queue.Enqueue($c)
                }
            }
        }
    }

    'alive' {
        foreach ($p in (Get-PidList $Pids)) {
            $q = @(Get-CimInstance Win32_Process -Filter "ProcessId = $p")
            if ($q.Count -gt 0) { Write-Host "ALIVE=$p NAME=$($q[0].Name)" } else { Write-Host "DEAD=$p" }
        }
        if ($Marker -ne '') {
            $left = @(Get-CimInstance Win32_Process -Filter "CommandLine LIKE '%$Marker%'" |
                Where-Object { $_.ProcessId -ne $PID })
            Write-Host "MARKER_LEFT=$($left.Count)"
            foreach ($p in $left) { Write-Host "LEFTOVER=$($p.ProcessId) NAME=$($p.Name)" }
        }
    }

    'kill' {
        foreach ($p in (Get-PidList $Pids)) {
            $sw = [Diagnostics.Stopwatch]::StartNew()
            & taskkill.exe /PID $p /T /F 2>&1 | ForEach-Object { Write-Host "taskkill: $_" }
            Write-Host ("KILL_EXIT={0} PID={1} MS={2:F1}" -f $LASTEXITCODE, $p, $sw.Elapsed.TotalMilliseconds)
        }
    }

    # The victim: a Windows exe that writes steadily and can spawn Windows
    # children whose own command lines never carry the marker.
    'victim' {
        New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
        $log = Join-Path $OutDir 'log.txt'
        for ($i = 0; $i -lt $Children; $i++) {
            $arg = "-NoProfile -Command `"`$t='$ChildTag'; while (`$true) { Start-Sleep -Seconds 1 }`""
            Start-Process -FilePath 'powershell.exe' -ArgumentList $arg -WindowStyle Hidden | Out-Null
        }
        $n = 0
        while ($true) {
            Add-Content -Path $log -Value ("line {0}" -f $n)
            $n++
            Start-Sleep -Milliseconds 50
        }
    }
}
