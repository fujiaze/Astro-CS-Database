$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$gc = Join-Path $root 'run/temp/p2_v7/gc'
Write-Output '=== gc dir HiPS products ===';
Get-ChildItem -LiteralPath $gc -Filter *.hips -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name
Write-Output '=== f01 manifest ===';
if (Test-Path -LiteralPath (Join-Path $gc 'gc_R_panel1_f01.hips/manifest.json')) { Get-Content -LiteralPath (Join-Path $gc 'gc_R_panel1_f01.hips/manifest.json') }
