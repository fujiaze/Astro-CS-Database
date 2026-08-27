$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$cfg = Join-Path $root 'lib/orchestrator/configs/stage1_gc_panel1_Red.json'
Write-Output '=== stage1_gc_panel1_Red.json content ===';
Get-Content -LiteralPath $cfg -TotalCount 40
Write-Output '=== run/configs ===';
Get-ChildItem -LiteralPath (Join-Path $root 'run/configs') -Name -ErrorAction SilentlyContinue | Select-Object -First 15
