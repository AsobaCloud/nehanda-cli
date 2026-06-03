#Requires -Version 5.1
param([switch]$Portable)
$ErrorActionPreference = "Stop"

try {
    $repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
    $buildDir = Join-Path $repoRoot "build"
    $installRoot = Join-Path $env:LOCALAPPDATA "aimee"
    $binDir = Join-Path $installRoot "bin"

    # Build
    Write-Host "> Building aimee with CMake..." -ForegroundColor Green
    Push-Location $repoRoot
    try {
        & cmake -B $buildDir -S $repoRoot
        & cmake --build $buildDir --config Release
    }
    finally {
        Pop-Location
    }

    $releaseDir = Join-Path $buildDir "Release"

    # Detect running services and stop them (drain top-down: server first, then kb)
    $kbWasRunning = $false
    $serverWasRunning = $false

    if (-not $Portable) {
        $serverSvc = Get-Service aimee-server -ErrorAction SilentlyContinue
        if ($serverSvc -and $serverSvc.Status -eq "Running") {
            $serverWasRunning = $true
            Write-Host "> Stopping aimee-server service..." -ForegroundColor Yellow
            Stop-Service aimee-server
        }

        $kbSvc = Get-Service aimee-kb -ErrorAction SilentlyContinue
        if ($kbSvc -and $kbSvc.Status -eq "Running") {
            $kbWasRunning = $true
            Write-Host "> Stopping aimee-kb service..." -ForegroundColor Yellow
            Stop-Service aimee-kb
        }
    }

    # Find build outputs
    $cliCandidate = @(
        (Join-Path $releaseDir "aimee.exe"),
        (Join-Path $buildDir "aimee.exe")
    ) | Where-Object { Test-Path $_ } | Select-Object -First 1
    $serverCandidate = @(
        (Join-Path $releaseDir "aimee-server.exe"),
        (Join-Path $buildDir "aimee-server.exe")
    ) | Where-Object { Test-Path $_ } | Select-Object -First 1
    $kbCandidate = @(
        (Join-Path $releaseDir "aimee-kb.exe"),
        (Join-Path $buildDir "aimee-kb.exe")
    ) | Where-Object { Test-Path $_ } | Select-Object -First 1

    if (-not $cliCandidate) {
        throw "Built binary not found: aimee.exe"
    }
    if (-not $serverCandidate) {
        throw "Built binary not found: aimee-server.exe"
    }

    # Copy fresh binaries
    Write-Host "> Updating binaries in $binDir..." -ForegroundColor Green
    Copy-Item $cliCandidate (Join-Path $binDir "aimee.exe") -Force
    Copy-Item $serverCandidate (Join-Path $binDir "aimee-server.exe") -Force
    if ($kbCandidate) {
        Copy-Item $kbCandidate (Join-Path $binDir "aimee-kb.exe") -Force
    }

    # Re-render WinSW XMLs (overwrite)
    if (-not $Portable) {
        Write-Host "> Re-rendering WinSW service definitions..." -ForegroundColor Green
        foreach ($xmlName in "aimee-kb.xml", "aimee-server.xml") {
            $src = Join-Path $repoRoot "service\$xmlName"
            if (Test-Path $src) {
                $svcId = $xmlName -replace '\.xml$', ''
                $dst = Join-Path $binDir "$svcId-service.xml"
                Copy-Item $src $dst -Force
            }
        }
    }

    # Restart services in startup order (kb first, then server)
    if (-not $Portable) {
        if ($kbWasRunning) {
            Write-Host "> Starting aimee-kb service..." -ForegroundColor Green
            Start-Service aimee-kb
        }
        if ($serverWasRunning) {
            Write-Host "> Starting aimee-server service..." -ForegroundColor Green
            Start-Service aimee-server
        }
    }

    Write-Host ""
    $summary = "Update complete."
    if ($kbWasRunning -or $serverWasRunning) {
        $summary += " Services restarted."
    }
    elseif ($Portable) {
        $summary += " (portable mode; services untouched)"
    }
    Write-Host $summary -ForegroundColor Green
}
catch {
    Write-Host "Update failed: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
