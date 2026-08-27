$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$lights = Join-Path $root 'testdata/Galaxy_Center_T4/lights'
$red = Get-ChildItem -LiteralPath $lights -Recurse -Filter *-Red.fts -ErrorAction SilentlyContinue
Write-Output ('T4_RED_FRAMES=' + $red.Count)
$red | Select-Object -ExpandProperty FullName | Select-Object -First 5
Write-Output '...'
$red | Select-Object -Last 3 -ExpandProperty FullName
