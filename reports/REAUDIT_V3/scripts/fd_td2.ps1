$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$t4 = Join-Path $root 'testdata/Galaxy_Center_T4'
Get-ChildItem -LiteralPath $t4 -Recurse -File -ErrorAction SilentlyContinue | Select-Object -First 25 -ExpandProperty FullName
Write-Output ('TOTAL_T4_FILES=' + (Get-ChildItem -LiteralPath $t4 -Recurse -File -ErrorAction SilentlyContinue).Count)
