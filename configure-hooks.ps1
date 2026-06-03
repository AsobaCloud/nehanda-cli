#Requires -Version 5.1
$ErrorActionPreference = "Stop"

$AimeeBin = Join-Path (Join-Path $env:LOCALAPPDATA "aimee") "bin\aimee.exe"
$script:Configured = 0

function Write-Info($Message) {
    Write-Host "> $Message" -ForegroundColor Green
}

function Write-WarnMsg($Message) {
    Write-Host "! $Message" -ForegroundColor Yellow
}

function Get-JsonObject([string]$Path) {
    if (Test-Path $Path) {
        $raw = Get-Content -Path $Path -Raw
        if ($raw.Trim()) {
            return $raw | ConvertFrom-Json -Depth 100
        }
    }
    return [pscustomobject]@{}
}

function Ensure-ObjectProperty($Object, [string]$Name, $DefaultValue) {
    if (-not ($Object.PSObject.Properties.Name -contains $Name)) {
        $Object | Add-Member -NotePropertyName $Name -NotePropertyValue $DefaultValue
    }
    return $Object.$Name
}

function Remove-AimeeHooks($Entries) {
    $result = @()
    foreach ($entry in @($Entries)) {
        $hasAimee = $false
        foreach ($hook in @($entry.hooks)) {
            if ($hook.command -and $hook.command -match 'aimee') {
                $hasAimee = $true
                break
            }
        }
        if (-not $hasAimee) {
            $result += $entry
        }
    }
    return $result
}

function Add-HookEntry($HooksObject, [string]$EventName, [string]$Matcher, [string]$Command) {
    $existing = @()
    if ($HooksObject.PSObject.Properties.Name -contains $EventName) {
        $existing = Remove-AimeeHooks $HooksObject.$EventName
    }

    $entry = [pscustomobject]@{
        matcher = $Matcher
        hooks   = @([pscustomobject]@{
            type    = 'command'
            command = $Command
        })
    }

    $HooksObject | Add-Member -NotePropertyName $EventName -NotePropertyValue (@($existing) + $entry) -Force
}

function Configure-JsonHooks {
    param(
        [string]$Name,
        [string]$ConfigPath,
        [string]$PreEvent,
        [string]$PostEvent,
        [string]$SessionEvent,
        [string]$PreMatcher,
        [string]$PostMatcher,
        [string]$McpPath = $ConfigPath
    )

    $settings = Get-JsonObject $ConfigPath
    $mcpSettings = if ($McpPath -ne $ConfigPath) { Get-JsonObject $McpPath } else { $settings }

    $hooks = Ensure-ObjectProperty $settings 'hooks' ([pscustomobject]@{})
    $mcpServers = Ensure-ObjectProperty $mcpSettings 'mcpServers' ([pscustomobject]@{})

    if ($SessionEvent -ne 'NONE') {
        Add-HookEntry $hooks $SessionEvent 'startup|resume|compact' "`"$AimeeBin`" session-start"
    }
    if ($PreEvent -ne 'NONE') {
        Add-HookEntry $hooks $PreEvent $PreMatcher "`"$AimeeBin`" hooks pre"
    }
    if ($PostEvent -ne 'NONE') {
        Add-HookEntry $hooks $PostEvent $PostMatcher "`"$AimeeBin`" hooks post"
    }

    $mcpServers | Add-Member -NotePropertyName 'aimee' -NotePropertyValue ([pscustomobject]@{
        command = $AimeeBin
        args    = @('mcp-serve')
    }) -Force

    $configDir = Split-Path -Parent $ConfigPath
    if ($configDir) {
        New-Item -ItemType Directory -Force -Path $configDir | Out-Null
    }
    $settings | ConvertTo-Json -Depth 100 | Set-Content -Path $ConfigPath -Encoding UTF8
    Write-Info "Configured $Name: $ConfigPath"

    if ($McpPath -ne $ConfigPath) {
        $mcpDir = Split-Path -Parent $McpPath
        if ($mcpDir) {
            New-Item -ItemType Directory -Force -Path $mcpDir | Out-Null
        }
        $mcpSettings | ConvertTo-Json -Depth 100 | Set-Content -Path $McpPath -Encoding UTF8
        Write-Info "Configured $Name MCP: $McpPath"
    }

    $script:Configured++
}

try {
    if (-not (Test-Path $AimeeBin)) {
        throw "aimee.exe not found at $AimeeBin. Run install.ps1 first."
    }

    $claudeRoots = @(
        (Join-Path $env:APPDATA 'Claude'),
        (Join-Path $env:LOCALAPPDATA 'Claude')
    ) | Where-Object { $_ -and (Test-Path $_) }

    if ($claudeRoots.Count -gt 0) {
        $claudeSettings = Join-Path $claudeRoots[0] 'settings.json'
        Configure-JsonHooks -Name 'Claude Code' `
            -ConfigPath $claudeSettings `
            -PreEvent 'PreToolUse' -PostEvent 'PostToolUse' -SessionEvent 'SessionStart' `
            -PreMatcher 'Edit|Write|MultiEdit|Bash|Read|Glob|Grep' -PostMatcher 'Edit|Write|MultiEdit'
    }
    else {
        Write-WarnMsg 'Claude config directory not found in APPDATA or LOCALAPPDATA.'
    }

    $claudeDesktop = @(
        (Join-Path $env:APPDATA 'Claude\claude_desktop_config.json'),
        (Join-Path $env:LOCALAPPDATA 'Claude\claude_desktop_config.json')
    ) | Where-Object { Test-Path (Split-Path -Parent $_) } | Select-Object -First 1
    if ($claudeDesktop) {
        Configure-JsonHooks -Name 'Claude Desktop' `
            -ConfigPath $claudeDesktop `
            -PreEvent 'NONE' -PostEvent 'NONE' -SessionEvent 'NONE' `
            -PreMatcher '' -PostMatcher ''
    }

    $geminiRoot = Join-Path $env:USERPROFILE '.gemini'
    if (Test-Path $geminiRoot) {
        Configure-JsonHooks -Name 'Gemini CLI' `
            -ConfigPath (Join-Path $geminiRoot 'settings.json') `
            -PreEvent 'BeforeTool' -PostEvent 'AfterTool' -SessionEvent 'SessionStart' `
            -PreMatcher 'write_file|replace|shell' -PostMatcher 'write_file|replace'
    }

    $codexRoot = Join-Path $env:USERPROFILE '.codex'
    if (Test-Path $codexRoot) {
        Configure-JsonHooks -Name 'Codex CLI' `
            -ConfigPath (Join-Path $codexRoot 'hooks.json') `
            -PreEvent 'PreToolUse' -PostEvent 'PostToolUse' -SessionEvent 'SessionStart' `
            -PreMatcher 'Bash' -PostMatcher 'Bash' `
            -McpPath (Join-Path $codexRoot 'mcp-config.json')
    }

    $copilotRoot = Join-Path $env:USERPROFILE '.copilot'
    if (Test-Path $copilotRoot) {
        Configure-JsonHooks -Name 'GitHub Copilot' `
            -ConfigPath (Join-Path $copilotRoot 'config.json') `
            -PreEvent 'PreToolUse' -PostEvent 'PostToolUse' -SessionEvent 'SessionStart' `
            -PreMatcher 'Bash|Edit|Write' -PostMatcher 'Edit|Write' `
            -McpPath (Join-Path $copilotRoot 'mcp-config.json')
    }

    if ($script:Configured -gt 0) {
        Write-Info "aimee hooks refreshed for $script:Configured tool(s)."
    }
    else {
        Write-WarnMsg 'No supported AI coding tool configs were detected.'
    }
}
catch {
    Write-Host "Hook configuration failed: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
