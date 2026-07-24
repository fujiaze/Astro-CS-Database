[CmdletBinding()]
param(
    [switch]$CopyPrompt
)

$ErrorActionPreference = 'Stop'
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$engineeringDir = Split-Path -Parent $scriptDir
$promptPath = Join-Path $scriptDir 'AUTONOMOUS_MASTER_AGENT_PROMPT.md'
$currentWorkPath = Join-Path $engineeringDir 'control\CURRENT_WORK.md'

if (-not (Test-Path -LiteralPath $promptPath)) {
    throw "Missing prompt file: $promptPath"
}
if (-not (Test-Path -LiteralPath $currentWorkPath)) {
    throw "Missing current work file: $currentWorkPath"
}

$prompt = @"
$(Get-Content -LiteralPath $promptPath -Raw -Encoding UTF8)

--- CURRENT WORK ---
$(Get-Content -LiteralPath $currentWorkPath -Raw -Encoding UTF8)
"@

if ($CopyPrompt) {
    if (Get-Command Set-Clipboard -ErrorAction SilentlyContinue) {
        Set-Clipboard -Value $prompt
        Write-Host 'AstroCS autonomous Agent prompt copied to clipboard.'
    } else {
        Write-Warning 'Set-Clipboard is unavailable; prompt will only be printed.'
    }
}

$prompt
