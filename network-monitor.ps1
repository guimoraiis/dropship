#Requires -Version 5.1
<#
.SYNOPSIS
    Network Monitor for Overwatch 2 route analysis
    Real-time pathping + netstat monitoring during gameplay

.DESCRIPTION
    Monitors active connections and network routes during Overwatch 2 gameplay.
    Logs route information (tracert/pathping) in JSON format for analysis.
    Correlates with dropship auto-block events for degradation validation.

.PARAMETER OutputPath
    Directory for log output. Default: %TEMP%\dropship\network-monitor

.PARAMETER CheckInterval
    Seconds between connection checks. Default: 5

.PARAMETER PathpingMaxHops
    Maximum hops for pathping. Default: 15

.PARAMETER TargetProcess
    Process name to monitor. Default: 'Overwatch'

.EXAMPLE
    .\network-monitor.ps1 -OutputPath "C:\Temp\ow2-logs" -CheckInterval 3

.NOTES
    Requires admin privileges for pathping functionality
    Logs both console output and JSON files for correlation
#>

param(
    [string]$OutputPath = "$env:TEMP\dropship\network-monitor",
    [int]$CheckInterval = 5,
    [int]$PathpingMaxHops = 15,
    [string]$TargetProcess = 'Overwatch'
)

$ErrorActionPreference = 'Continue'
$WarningPreference = 'SilentlyContinue'

# ============================================================================
# Configuration
# ============================================================================

$Config = @{
    OutputPath = $OutputPath
    CheckInterval = $CheckInterval
    PathpingMaxHops = $PathpingMaxHops
    TargetProcess = $TargetProcess
    LogFile = "$OutputPath\network-monitor.log"
    JsonLog = "$OutputPath\routes.json"
    StartTime = Get-Date
    IsRunning = $false
}

# ============================================================================
# Logging Functions
# ============================================================================

function Initialize-Directories {
    try {
        if (-not (Test-Path $Config.OutputPath)) {
            New-Item -ItemType Directory -Path $Config.OutputPath -Force | Out-Null
        }
        Write-Host "[$(Get-Date -Format 'HH:mm:ss')] ✓ Log directory: $($Config.OutputPath)"
    }
    catch {
        Write-Error "Failed to create output directory: $_"
        exit 1
    }
}

function Write-Log {
    param(
        [string]$Message,
        [ValidateSet('INFO', 'WARN', 'ERROR', 'SUCCESS')]
        [string]$Level = 'INFO'
    )

    $timestamp = Get-Date -Format 'yyyy-MM-dd HH:mm:ss.fff'
    $logEntry = "[$timestamp] [$Level] $Message"

    # Console output with color
    $color = @{
        'INFO' = 'Cyan'
        'WARN' = 'Yellow'
        'ERROR' = 'Red'
        'SUCCESS' = 'Green'
    }
    Write-Host $logEntry -ForegroundColor $color[$Level]

    # File log
    Add-Content -Path $Config.LogFile -Value $logEntry -ErrorAction SilentlyContinue
}

# ============================================================================
# Process Monitoring
# ============================================================================

function Test-TargetProcessRunning {
    $process = Get-Process -Name $Config.TargetProcess -ErrorAction SilentlyContinue
    return $null -ne $process
}

function Wait-ForTargetProcess {
    Write-Log "Waiting for $($Config.TargetProcess) to start..." 'INFO'

    while (-not (Test-TargetProcessRunning)) {
        Start-Sleep -Seconds 2
    }

    Write-Log "$($Config.TargetProcess) detected! Starting network monitoring." 'SUCCESS'
    $Config.IsRunning = $true
}

# ============================================================================
# Connection Detection
# ============================================================================

function Get-ActiveConnection {
    <#
    .SYNOPSIS
    Detects active game server connections using netstat
    #>
    try {
        $netstatOutput = netstat -ano | Select-String 'ESTABLISHED'

        # Filter for game-related connections (common Overwatch ports: 1119, 3074, 6113, 27015-27030, etc)
        $gameConnections = $netstatOutput | ForEach-Object {
            $parts = $_.Line -split '\s+' | Where-Object { $_ }
            if ($parts.Count -ge 5) {
                $remoteAddr = $parts[2] -split ':'
                [PSCustomObject]@{
                    Protocol = $parts[0]
                    LocalAddr = $parts[1]
                    RemoteAddr = $remoteAddr[0]
                    RemotePort = $remoteAddr[-1]
                    PID = $parts[-1]
                    Timestamp = Get-Date -Format 'yyyy-MM-dd HH:mm:ss.fff'
                }
            }
        }

        return $gameConnections | Sort-Object -Unique -Property RemoteAddr
    }
    catch {
        Write-Log "Error getting active connections: $_" 'ERROR'
        return $null
    }
}

function Get-ServerInfo {
    <#
    .SYNOPSIS
    Attempts to identify server region from IP using known ranges
    #>
    param([string]$IP)

    # Dropship known server ranges
    $serverRanges = @{
        'GBR1' = @('34.39.128.0/17', '34.95.128.0/17')
        'GSG1' = @('34.1.128.0/20', '34.1.192.0/20', '34.2.16.0/20')
        'GTK1' = @('34.84.0.0/16', '34.85.0.0/17')
        'GUE4' = @('8.228.64.0/18', '8.234.128.0/17', '34.145.128.0/17')
        'ORD1' = @('64.224.0.0/21', '24.105.40.0/21')
        'LAS1' = @('64.224.24.0/23')
    }

    foreach ($server in $serverRanges.GetEnumerator()) {
        foreach ($cidr in $server.Value) {
            # Simplified CIDR check (not perfect but good enough for logs)
            if ($IP -match ('^' + ($cidr -split '/')[0] -replace '\.\d+$', '\.'))) {
                return $server.Key
            }
        }
    }

    return 'UNKNOWN'
}

# ============================================================================
# Pathping Analysis
# ============================================================================

function Invoke-PathpingAnalysis {
    <#
    .SYNOPSIS
    Runs pathping and returns structured hop data
    #>
    param([string]$TargetIP)

    if ([string]::IsNullOrEmpty($TargetIP)) {
        return $null
    }

    try {
        Write-Log "Pathping to $TargetIP..." 'INFO'

        # Pathping with timeout (period 125ms, count 4)
        $pathpingResult = pathping -h $Config.PathpingMaxHops -q 4 -w 125 $TargetIP 2>$null

        $hops = @()
        $capturingHops = $false

        foreach ($line in $pathpingResult) {
            # Start capturing after header
            if ($line -match 'Hop.*RTT.*Lost') {
                $capturingHops = $true
                continue
            }

            if ($capturingHops -and $line -match '^\s*\d+') {
                $parts = $line -split '\s+' | Where-Object { $_ }
                if ($parts.Count -ge 4) {
                    $hop = [PSCustomObject]@{
                        HopNum = [int]$parts[0]
                        IP = $parts[1] -replace '[(\[\])]', ''
                        MinMs = 0
                        AvgMs = 0
                        MaxMs = 0
                        LossPct = 0
                        Timestamp = Get-Date -Format 'yyyy-MM-dd HH:mm:ss.fff'
                    }

                    # Try to parse latency values
                    if ($parts.Count -ge 5 -and $parts[2] -match '^\d+') {
                        $hop.MinMs = [int]$parts[2]
                        $hop.AvgMs = [int]$parts[3]
                        $hop.MaxMs = [int]$parts[4]
                    }

                    # Extract loss percentage
                    if ($line -match '(\d+)%') {
                        $hop.LossPct = [int]$Matches[1]
                    }

                    $hops += $hop
                }
            }
        }

        return $hops
    }
    catch {
        Write-Log "Error during pathping: $_" 'WARN'
        return $null
    }
}

function Find-DegradationHop {
    <#
    .SYNOPSIS
    Identifies which hop is causing degradation
    #>
    param([array]$Hops)

    if ($null -eq $Hops -or $Hops.Count -eq 0) {
        return $null
    }

    # Find first hop with loss > 10% or latency > 200ms
    $degradedHop = $Hops | Where-Object {
        $_.LossPct -gt 10 -or $_.AvgMs -gt 200
    } | Select-Object -First 1

    return $degradedHop
}

# ============================================================================
# Data Logging
# ============================================================================

function Write-RouteSnapshot {
    <#
    .SYNOPSIS
    Records snapshot of current route state to JSON
    #>
    param(
        [PSCustomObject]$Connection,
        [array]$PathpingHops,
        [PSCustomObject]$DegradedHop
    )

    $snapshot = [PSCustomObject]@{
        timestamp = Get-Date -Format 'yyyy-MM-dd HH:mm:ss.fff'
        uptime_seconds = ([DateTime]::Now - $Config.StartTime).TotalSeconds
        active_connection = @{
            remote_ip = $Connection.RemoteAddr
            remote_port = $Connection.RemotePort
            server_region = Get-ServerInfo $Connection.RemoteAddr
            local_addr = $Connection.LocalAddr
        }
        pathping = @{
            target_ip = $Connection.RemoteAddr
            max_hops = $Config.PathpingMaxHops
            hops = $PathpingHops
        }
        degradation = @{
            detected = $null -ne $DegradedHop
            degraded_hop = $DegradedHop
            recommendation = if ($null -ne $DegradedHop) {
                "Degradation at hop $($DegradedHop.HopNum) ($($DegradedHop.IP)): loss=$($DegradedHop.LossPct)%, latency=$($DegradedHop.AvgMs)ms"
            } else {
                "Route is healthy"
            }
        }
    }

    # Append to JSON log
    try {
        $json = $snapshot | ConvertTo-Json -Depth 10
        Add-Content -Path $Config.JsonLog -Value $json
        Add-Content -Path $Config.JsonLog -Value "---"

        if ($snapshot.degradation.detected) {
            Write-Log "⚠️  $($snapshot.degradation.recommendation)" 'WARN'
        } else {
            Write-Log "✓ Route healthy to $($snapshot.active_connection.server_region)" 'SUCCESS'
        }
    }
    catch {
        Write-Log "Error writing route snapshot: $_" 'ERROR'
    }
}

function Correlate-WithAutoBlockLog {
    <#
    .SYNOPSIS
    Compares network monitor log with dropship auto-block log
    #>
    $autoBlockLog = "$env:TEMP\dropship\autoblock.log"

    if (-not (Test-Path $autoBlockLog)) {
        Write-Log "Auto-block log not found. Run dropship with auto-block enabled." 'WARN'
        return
    }

    try {
        $autoBlockEvents = Get-Content $autoBlockLog | Where-Object { $_ -match '^\d{4}-\d{2}-\d{2}' }

        if ($autoBlockEvents.Count -gt 0) {
            Write-Log "Found $($autoBlockEvents.Count) auto-block events. Correlating..." 'INFO'

            # Create correlation report
            $correlationFile = "$($Config.OutputPath)\correlation-report.txt"
            $autoBlockEvents | Out-File -FilePath $correlationFile -Append

            Write-Log "Correlation report: $correlationFile" 'SUCCESS'
        }
    }
    catch {
        Write-Log "Error correlating logs: $_" 'WARN'
    }
}

# ============================================================================
# Main Loop
# ============================================================================

function Start-NetworkMonitor {
    Write-Host @"

╔════════════════════════════════════════════════════════════════╗
║          OW2 Network Monitor (Dropship Companion)             ║
║                Route Analysis & Pathping Monitor              ║
╚════════════════════════════════════════════════════════════════╝

"@

    Initialize-Directories
    Write-Log "Network Monitor initialized" 'SUCCESS'
    Write-Log "Output: $($Config.OutputPath)" 'INFO'
    Write-Log "Check interval: $($Config.CheckInterval)s" 'INFO'

    Wait-ForTargetProcess

    $lastIP = $null

    while ($true) {
        try {
            # Check if process still running
            if (-not (Test-TargetProcessRunning)) {
                Write-Log "$($Config.TargetProcess) closed. Stopping monitor." 'INFO'
                Correlate-WithAutoBlockLog
                break
            }

            # Get active connection
            $connection = Get-ActiveConnection | Select-Object -First 1

            if ($null -ne $connection) {
                # Only run pathping if IP changed (expensive operation)
                if ($connection.RemoteAddr -ne $lastIP) {
                    Write-Log "New connection detected: $($connection.RemoteAddr)" 'SUCCESS'
                    $lastIP = $connection.RemoteAddr

                    $hops = Invoke-PathpingAnalysis $connection.RemoteAddr
                    $degradedHop = Find-DegradationHop $hops

                    Write-RouteSnapshot $connection $hops $degradedHop
                }
            }

            Start-Sleep -Seconds $Config.CheckInterval
        }
        catch {
            Write-Log "Error in main loop: $_" 'ERROR'
            Start-Sleep -Seconds 5
        }
    }
}

# ============================================================================
# Entry Point
# ============================================================================

if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Warning "⚠️  This script works best with administrator privileges"
    Write-Host "Continuing anyway, but pathping may be limited..."
}

Start-NetworkMonitor
