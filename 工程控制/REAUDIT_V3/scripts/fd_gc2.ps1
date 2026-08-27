$ErrorActionPreference = 'Continue'
$g = 'F:/Astro dev/Astro CS Normalization Database/run/temp/p2_v7/gc'
$hips = Get-ChildItem -LiteralPath $g -Filter *.hips -ErrorAction SilentlyContinue
Write-Output ('GC_HIPS_COUNT=' + $hips.Count)
$hips | Select-Object -ExpandProperty Name
