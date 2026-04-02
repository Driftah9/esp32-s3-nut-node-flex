# idf-build.ps1 - Claude-callable ESP-IDF build/flash/monitor wrapper
#
# Removes MinGW MSYSTEM env var before IDF init to bypass MinGW rejection.
# All output goes to docs\ logs for Claude to read immediately after.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File .\idf-build.ps1 -Target build
#   powershell -ExecutionPolicy Bypass -File .\idf-build.ps1 -Target flash
#   powershell -ExecutionPolicy Bypass -File .\idf-build.ps1 -Target monitor
#   powershell -ExecutionPolicy Bypass -File .\idf-build.ps1 -Target monitor -Duration 90
#
# Log files:
#   docs\build.log    <- build output
#   docs\flash.log    <- flash output
#   docs\monitor.log  <- serial capture (Duration seconds then stops)

param(
    [string]$Target   = "build",
    [string]$Port     = "COM3",
    [int]$Duration    = 60        # seconds to capture for monitor target
)

$ProjectRoot = $PSScriptRoot
$ProjectSrc  = "$ProjectRoot\src\current"
$DocsDir     = "$ProjectRoot\docs"
$IdfId       = "esp-idf-ab7213b7273352b64422b1f400ff27a0"
$InitScript  = "C:\Espressif\Initialize-Idf.ps1"
$Timestamp   = Get-Date -Format "yyyy-MM-dd HH:mm:ss"

# ---- Remove MinGW env vars that block IDF init ----
Remove-Item Env:MSYSTEM      -ErrorAction SilentlyContinue
Remove-Item Env:MSYS         -ErrorAction SilentlyContinue
Remove-Item Env:MINGW_PREFIX -ErrorAction SilentlyContinue

# ---- Init IDF environment ----
Write-Host "[idf-build] Initializing ESP-IDF ($Target)..."
& $InitScript -IdfId $IdfId 2>&1 | Where-Object { $_ -match "Done!|Error|fail|not found" } | ForEach-Object { Write-Host "  $_" }

# ---- Move to source directory ----
Set-Location $ProjectSrc

# ===========================================================================
# BUILD
# ===========================================================================
if ($Target -eq "build") {
    $LogFile = "$DocsDir\build.log"
    Write-Host "[idf-build] Building..."

    $output   = & idf.py build 2>&1
    $exitcode = $LASTEXITCODE

    "=== build - $Timestamp ===" | Set-Content $LogFile -Encoding UTF8
    $output | Add-Content $LogFile -Encoding UTF8
    "=== Exit code: $exitcode ===" | Add-Content $LogFile -Encoding UTF8

    if ($exitcode -eq 0) {
        $sizeLines = $output | Select-String "Binary size|bytes free|Project build complete" | Select-Object -Last 3
        Write-Host "[idf-build] BUILD OK - docs\build.log"
        $sizeLines | ForEach-Object { Write-Host "  $_" }
    } else {
        $errLines = $output | Select-String "error:|Error:|undefined reference" | Select-Object -Last 10
        Write-Host "[idf-build] BUILD FAILED (exit $exitcode) - docs\build.log"
        $errLines | ForEach-Object { Write-Host "  $_" }
    }
    exit $exitcode
}

# ===========================================================================
# FLASH
# ===========================================================================
if ($Target -eq "flash" -or $Target -eq "flash-monitor") {
    # Kill any lingering python/idf_monitor processes that may hold the COM port
    Get-Process python* -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 500
}

if ($Target -eq "flash") {
    $LogFile = "$DocsDir\flash.log"
    Write-Host "[idf-build] Flashing on $Port..."

    $output   = & idf.py flash -p $Port 2>&1
    $exitcode = $LASTEXITCODE

    "=== flash $Port - $Timestamp ===" | Set-Content $LogFile -Encoding UTF8
    $output | Add-Content $LogFile -Encoding UTF8
    "=== Exit code: $exitcode ===" | Add-Content $LogFile -Encoding UTF8

    if ($exitcode -eq 0) {
        Write-Host "[idf-build] FLASH OK - docs\flash.log"
        $output | Select-String "Hash of data verified|Leaving|Compressed" | Select-Object -Last 3 | ForEach-Object { Write-Host "  $_" }
    } else {
        Write-Host "[idf-build] FLASH FAILED (exit $exitcode) - docs\flash.log"
        $output | Select-Object -Last 15 | ForEach-Object { Write-Host "  $_" }
    }
    exit $exitcode
}

# ===========================================================================
# FLASH-MONITOR  (flash, then immediately capture serial output)
# ===========================================================================
if ($Target -eq "flash-monitor") {
    $FlashLog   = "$DocsDir\flash.log"
    $MonitorLog = "$DocsDir\monitor.log"

    Write-Host "[idf-build] Flashing on $Port..."
    $flashOut  = & idf.py flash -p $Port 2>&1
    $flashExit = $LASTEXITCODE

    "=== flash $Port - $Timestamp ===" | Set-Content $FlashLog -Encoding UTF8
    $flashOut | Add-Content $FlashLog -Encoding UTF8
    "=== Exit code: $flashExit ===" | Add-Content $FlashLog -Encoding UTF8

    if ($flashExit -ne 0) {
        Write-Host "[idf-build] FLASH FAILED (exit $flashExit) - docs\flash.log"
        $flashOut | Select-Object -Last 10 | ForEach-Object { Write-Host "  $_" }
        exit $flashExit
    }
    Write-Host "[idf-build] FLASH OK - starting monitor for ${Duration}s..."

    # Device just reset from flash - start monitor immediately with --no-reset
    $elf = Get-ChildItem "$ProjectSrc\build\*.elf" -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $elf) { Write-Host "[idf-build] ERROR: No .elf found"; exit 1 }

    "=== monitor $Port - ${Duration}s capture (post-flash) - $Timestamp ===" | Set-Content $MonitorLog -Encoding UTF8

    $pinfo                         = New-Object System.Diagnostics.ProcessStartInfo
    $pinfo.FileName                = "cmd.exe"
    $pinfo.Arguments               = "/c idf.py monitor -p $Port --no-reset"
    $pinfo.WorkingDirectory        = $ProjectSrc
    $pinfo.RedirectStandardOutput  = $true
    $pinfo.RedirectStandardError   = $true
    $pinfo.UseShellExecute         = $false
    $pinfo.CreateNoWindow          = $true

    $proc = New-Object System.Diagnostics.Process
    $proc.StartInfo = $pinfo

    $mlog = [System.Collections.Generic.List[string]]::new()
    Register-ObjectEvent -InputObject $proc -EventName OutputDataReceived `
        -MessageData $mlog `
        -Action { if ($null -ne $EventArgs.Data) { $Event.MessageData.Add($EventArgs.Data) } } | Out-Null
    Register-ObjectEvent -InputObject $proc -EventName ErrorDataReceived `
        -MessageData $mlog `
        -Action { if ($null -ne $EventArgs.Data) { $Event.MessageData.Add($EventArgs.Data) } } | Out-Null

    $proc.Start() | Out-Null
    $proc.BeginOutputReadLine()
    $proc.BeginErrorReadLine()

    $deadline = (Get-Date).AddSeconds($Duration)
    while (-not $proc.HasExited -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 200 }
    if (-not $proc.HasExited) { $proc.Kill(); Write-Host "[idf-build] Monitor stopped after ${Duration}s" }
    Start-Sleep -Milliseconds 500
    Get-EventSubscriber | Unregister-Event -Force -ErrorAction SilentlyContinue

    $mlog | Where-Object { $_ -ne $null } | Add-Content $MonitorLog -Encoding UTF8
    "=== End of capture ===" | Add-Content $MonitorLog -Encoding UTF8

    $lc = (Get-Content $MonitorLog | Measure-Object -Line).Lines
    Write-Host "[idf-build] Monitor log: docs\monitor.log ($lc lines)"
    exit 0
}

# ===========================================================================
# MONITOR
# ===========================================================================
if ($Target -eq "monitor") {
    $LogFile = "$DocsDir\monitor.log"
    Write-Host "[idf-build] Starting monitor on $Port for ${Duration}s..."

    $elf = Get-ChildItem "$ProjectSrc\build\*.elf" -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $elf) {
        Write-Host "[idf-build] ERROR: No .elf file found in build\ - run build first"
        exit 1
    }

    "=== monitor $Port - ${Duration}s capture - $Timestamp ===" | Set-Content $LogFile -Encoding UTF8

    $pinfo                         = New-Object System.Diagnostics.ProcessStartInfo
    $pinfo.FileName                = "cmd.exe"
    $pinfo.Arguments               = "/c idf.py monitor -p $Port --no-reset"
    $pinfo.WorkingDirectory        = $ProjectSrc
    $pinfo.RedirectStandardOutput  = $true
    $pinfo.RedirectStandardError   = $true
    $pinfo.UseShellExecute         = $false
    $pinfo.CreateNoWindow          = $true

    $proc = New-Object System.Diagnostics.Process
    $proc.StartInfo = $pinfo

    $stdout = [System.Collections.Generic.List[string]]::new()

    Register-ObjectEvent -InputObject $proc -EventName OutputDataReceived `
        -MessageData $stdout `
        -Action { if ($null -ne $EventArgs.Data) { $Event.MessageData.Add($EventArgs.Data) } } | Out-Null
    Register-ObjectEvent -InputObject $proc -EventName ErrorDataReceived `
        -MessageData $stdout `
        -Action { if ($null -ne $EventArgs.Data) { $Event.MessageData.Add($EventArgs.Data) } } | Out-Null

    $proc.Start()        | Out-Null
    $proc.BeginOutputReadLine()
    $proc.BeginErrorReadLine()

    $deadline = (Get-Date).AddSeconds($Duration)
    while (-not $proc.HasExited -and (Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 200
    }
    if (-not $proc.HasExited) {
        $proc.Kill()
        Write-Host "[idf-build] Monitor stopped after ${Duration}s"
    } else {
        Write-Host "[idf-build] Monitor exited early (exit $($proc.ExitCode))"
    }
    Start-Sleep -Milliseconds 500

    Get-EventSubscriber | Unregister-Event -Force -ErrorAction SilentlyContinue

    $stdout | Where-Object { $_ -ne $null } | Add-Content $LogFile -Encoding UTF8
    "=== End of capture ===" | Add-Content $LogFile -Encoding UTF8

    $lineCount = (Get-Content $LogFile | Measure-Object -Line).Lines
    Write-Host "[idf-build] Monitor log: docs\monitor.log ($lineCount lines)"
    exit 0
}

Write-Host "[idf-build] Unknown target '$Target'. Use: build / flash / monitor"
exit 1
