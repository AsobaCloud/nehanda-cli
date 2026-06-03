#Requires -Version 5.1
param([switch]$Portable)
$ErrorActionPreference = "Stop"

try {
    Write-Host "> Checking prerequisites..." -ForegroundColor Green

    $git = Get-Command git -ErrorAction SilentlyContinue
    if (-not $git) {
        throw "Git is not installed or not available on PATH."
    }

    $cl = Get-Command cl.exe -ErrorAction SilentlyContinue
    $gcc = Get-Command gcc -ErrorAction SilentlyContinue
    if (-not $cl -and -not $gcc) {
        throw "No supported C compiler found. Install Visual Studio Build Tools (cl.exe) or MinGW gcc."
    }

    $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    if (-not $cmake) {
        throw "cmake is not installed or not available on PATH."
    }

    $repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
    $buildDir = Join-Path $repoRoot "build"
    $installRoot = Join-Path $env:LOCALAPPDATA "aimee"
    $binDir = Join-Path $installRoot "bin"

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
    $cliCandidate = @(
        (Join-Path $releaseDir "aimee.exe"),
        (Join-Path $buildDir "aimee.exe")
    ) | Where-Object { Test-Path $_ } | Select-Object -First 1
    $serverCandidate = @(
        (Join-Path $releaseDir "aimee-server.exe"),
        (Join-Path $buildDir "aimee-server.exe")
    ) | Where-Object { Test-Path $_ } | Select-Object -First 1

    if (-not $cliCandidate) {
        throw "Built binary not found: aimee.exe"
    }
    if (-not $serverCandidate) {
        throw "Built binary not found: aimee-server.exe"
    }

    Write-Host "> Installing binaries to $binDir..." -ForegroundColor Green
    New-Item -ItemType Directory -Force -Path $binDir | Out-Null
    Copy-Item $cliCandidate (Join-Path $binDir "aimee.exe") -Force
    Copy-Item $serverCandidate (Join-Path $binDir "aimee-server.exe") -Force

    $skillsSrc = Join-Path $repoRoot "skills"
    if (Test-Path $skillsSrc) {
        $skillsDst = Join-Path $installRoot "skills"
        New-Item -ItemType Directory -Force -Path $skillsDst | Out-Null
        Get-ChildItem -Path $skillsSrc -Directory | ForEach-Object {
            $dst = Join-Path $skillsDst $_.Name
            if (-not (Test-Path $dst)) {
                Copy-Item $_.FullName $dst -Recurse
                Write-Host "  Installed bundled skill: $($_.Name)" -ForegroundColor Green
            }
        }
    }

    # Windows Service installation via WinSW (best-effort; skip with -Portable)
    $servicesRegistered = $false
    if (-not $Portable) {
        Write-Host "> Registering Windows services (WinSW)..." -ForegroundColor Green

        $winswUrl = "https://github.com/winsw/winsw/releases/download/v3.0.0-alpha.11/WinSW-x64.exe"
        $serviceDefs = @(
            @{ id = "aimee-kb";  xml = "aimee-kb.xml" },
            @{ id = "aimee-server"; xml = "aimee-server.xml" }
        )

        foreach ($svc in $serviceDefs) {
            $svcId = $svc.id
            $xmlSrc = Join-Path $repoRoot "service\$($svc.xml)"
            $xmlDst = Join-Path $binDir "$svcId-service.xml"
            $exeDst = Join-Path $binDir "$svcId-service.exe"
            $winsw  = $exeDst

            # Copy WinSW XML config
            Copy-Item $xmlSrc $xmlDst -Force

            # Download WinSW.exe if absent
            if (-not (Test-Path $exeDst)) {
                try {
                    Invoke-WebRequest -Uri $winswUrl -OutFile $exeDst -UseBasicParsing
                }
                catch {
                    Write-Warning "Failed to download WinSW for $svcId ($winswUrl). Skipping service registration. Re-run with -Portable for portable install."
                    continue
                }
            }

            # Install the service (refresh if already installed)
            try {
                & $winsw install
            }
            catch {
                try {
                    & $winsw refresh
                }
                catch {
                    Write-Warning "Failed to install/refresh service $svcId: $($_.Exception.Message)"
                    continue
                }
            }

            # Start the service
            try {
                & $winsw start
            }
            catch {
                Write-Warning "Failed to start service $svcId: $($_.Exception.Message)"
            }

            $servicesRegistered = $true
        }

        if ($servicesRegistered) {
            Write-Host "  Services registered successfully." -ForegroundColor Green
        }
    }

    Write-Host "> Updating user PATH..." -ForegroundColor Green
    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
    $pathEntries = @()
    if ($userPath) {
        $pathEntries = $userPath -split ';' | Where-Object { $_ -and $_.Trim() -ne '' }
    }
    $binDirNormalized = [System.IO.Path]::GetFullPath($binDir)
    $alreadyPresent = $false
    foreach ($entry in $pathEntries) {
        try {
            if ([System.StringComparer]::OrdinalIgnoreCase.Equals([System.IO.Path]::GetFullPath($entry), $binDirNormalized)) {
                $alreadyPresent = $true
                break
            }
        }
        catch {
            if ([System.StringComparer]::OrdinalIgnoreCase.Equals($entry, $binDirNormalized)) {
                $alreadyPresent = $true
                break
            }
        }
    }
    if (-not $alreadyPresent) {
        $newPath = if ($userPath -and $userPath.Trim()) { "$userPath;$binDir" } else { $binDir }
        [Environment]::SetEnvironmentVariable("Path", $newPath, "User")
        $env:Path = "$env:Path;$binDir"
        Write-Host "  Added $binDir to your user PATH." -ForegroundColor Yellow
    }
    else {
        Write-Host "  User PATH already includes $binDir." -ForegroundColor Yellow
    }

    Write-Host "> Initializing aimee..." -ForegroundColor Green
    & (Join-Path $binDir "aimee.exe") init

    Write-Host ""
    Write-Host "Install complete." -ForegroundColor Green
    Write-Host "Next steps:" -ForegroundColor Green
    Write-Host "  1. Open a new PowerShell or Command Prompt so PATH changes take effect everywhere."
    Write-Host "  2. Run .\configure-hooks.ps1 to configure Claude, Codex, Gemini, or Copilot hooks on Windows."
    if ($Portable) {
        Write-Host "  3. For Windows service setup, see service\aimee-server.xml (WinSW) or use the schtasks example in that file."
    }
    elseif ($servicesRegistered) {
        Write-Host "  3. aimee-server and aimee-kb are registered as Windows services (WinSW). Inspect with: Get-Service aimee-server,aimee-kb"
    }
    else {
        Write-Host "  3. For Windows service setup, see service\aimee-server.xml (WinSW) or use the schtasks example in that file."
    }
}
catch {
    Write-Host "Install failed: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
