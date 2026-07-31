param(
    [Parameter(Mandatory = $true)]
    [string]$Repository,  # OWNER/REPOSITORY

    [string]$SourcePath = (Join-Path $PSScriptRoot "..\wiki"),
    [string]$WorkPath = (Join-Path $env:TEMP "AstroCS.wiki"),
    [string]$CommitMessage = "docs(wiki): freeze AstroCS Stage1 specification",
    [switch]$Push,
    [int]$GitTimeoutSeconds = 120
)

$ErrorActionPreference = "Stop"

function Invoke-Git {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments,
        [Parameter(Mandatory = $true)]
        [string]$WorkingDirectory
    )

    $stdout = [System.IO.Path]::GetTempFileName()
    $stderr = [System.IO.Path]::GetTempFileName()
    try {
        $process = Start-Process `
            -FilePath "git" `
            -ArgumentList $Arguments `
            -WorkingDirectory $WorkingDirectory `
            -RedirectStandardOutput $stdout `
            -RedirectStandardError $stderr `
            -PassThru

        if (-not $process.WaitForExit($GitTimeoutSeconds * 1000)) {
            try { $process.Kill($true) } catch {}
            throw "git command timed out after $GitTimeoutSeconds seconds: git $($Arguments -join ' ')"
        }

        $outText = Get-Content -Raw -ErrorAction SilentlyContinue $stdout
        $errText = Get-Content -Raw -ErrorAction SilentlyContinue $stderr
        if ($outText) { Write-Host $outText.TrimEnd() }
        if ($errText) { Write-Host $errText.TrimEnd() }

        if ($process.ExitCode -ne 0) {
            throw "git command failed with exit code $($process.ExitCode): git $($Arguments -join ' ')"
        }
    }
    finally {
        Remove-Item -Force -ErrorAction SilentlyContinue $stdout, $stderr
    }
}

if (-not (Test-Path $SourcePath)) {
    throw "Wiki source path does not exist: $SourcePath"
}

$remote = "https://github.com/$Repository.wiki.git"
$parent = Split-Path -Parent $WorkPath
New-Item -ItemType Directory -Force -Path $parent | Out-Null

if (-not (Test-Path (Join-Path $WorkPath ".git"))) {
    if (Test-Path $WorkPath) {
        Remove-Item -Recurse -Force $WorkPath
    }
    Invoke-Git -Arguments @("clone", $remote, $WorkPath) -WorkingDirectory $parent
}
else {
    Invoke-Git -Arguments @("pull", "--rebase") -WorkingDirectory $WorkPath
}

# Only copy Markdown pages managed by this package. Unknown existing pages remain untouched.
Get-ChildItem -Path $SourcePath -Filter "*.md" -File | ForEach-Object {
    Copy-Item -Force $_.FullName (Join-Path $WorkPath $_.Name)
}

Invoke-Git -Arguments @("status", "--short") -WorkingDirectory $WorkPath
Invoke-Git -Arguments @("add", "--", "*.md") -WorkingDirectory $WorkPath

$diffProcess = Start-Process -FilePath "git" `
    -ArgumentList @("diff", "--cached", "--quiet") `
    -WorkingDirectory $WorkPath `
    -PassThru
if (-not $diffProcess.WaitForExit($GitTimeoutSeconds * 1000)) {
    try { $diffProcess.Kill($true) } catch {}
    throw "git diff timed out."
}

if ($diffProcess.ExitCode -eq 0) {
    Write-Host "No wiki changes to commit."
    exit 0
}
elseif ($diffProcess.ExitCode -ne 1) {
    throw "git diff failed with exit code $($diffProcess.ExitCode)."
}

Invoke-Git -Arguments @("commit", "-m", $CommitMessage) -WorkingDirectory $WorkPath

if ($Push) {
    Invoke-Git -Arguments @("pull", "--rebase") -WorkingDirectory $WorkPath
    Invoke-Git -Arguments @("push") -WorkingDirectory $WorkPath
    Write-Host "Wiki pushed successfully."
}
else {
    Write-Host "Commit created locally. Re-run with -Push to push it."
    Write-Host "Work tree: $WorkPath"
}
