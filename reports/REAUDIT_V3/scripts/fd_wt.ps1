$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$wt = Join-Path $root 'run/worktrees'
Write-Output '=== run/worktrees ===';
if (Test-Path -LiteralPath $wt) { Get-ChildItem -LiteralPath $wt -Name } else { Write-Output 'no worktrees dir' }
Write-Output '=== any anchor A/B checkouts anywhere? (git worktree list) ===';
Set-Location -LiteralPath $root
git worktree list 2>$null
Write-Output '=== anchor A/B staged2 builds present? ===';
Get-ChildItem -LiteralPath $root -Recurse -Filter astrocs-stage2.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName
