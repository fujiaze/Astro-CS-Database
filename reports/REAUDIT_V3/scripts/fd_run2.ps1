$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
Write-Output '=== run/ top-level dirs ===';
Get-ChildItem -LiteralPath (Join-Path $root 'run') -Directory -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name
Write-Output '=== stage1 configs for T4? ===';
Get-ChildItem -LiteralPath $root -Recurse -Filter *stage1*.json -ErrorAction SilentlyContinue | Select-Object -First 10 -ExpandProperty FullName
Write-Output '=== existing calibrated / mosaic outputs under run/phase2 ===';
Get-ChildItem -LiteralPath (Join-Path $root 'run/phase2') -ErrorAction SilentlyContinue | Select-Object -First 15 -ExpandProperty Name
