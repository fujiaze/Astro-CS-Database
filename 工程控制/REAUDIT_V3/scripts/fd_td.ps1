$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
Write-Output '=== testdata tree (top 3 levels) ===';
Get-ChildItem -LiteralPath (Join-Path $root 'testdata') -Directory | Select-Object -ExpandProperty Name
$t4 = Join-Path $root 'testdata/Galaxy_Center_T4'
if (Test-Path -LiteralPath $t4) {
  Get-ChildItem -LiteralPath $t4 -Directory | Select-Object -ExpandProperty Name
  foreach ($sub in @('masters','calibrated','lights')) {
    $p = Join-Path $t4 $sub
    if (Test-Path -LiteralPath $p) { Write-Output ($sub + ': ' + (Get-ChildItem -LiteralPath $p -File -ErrorAction SilentlyContinue).Count + ' files') }
  }
  if (Test-Path -LiteralPath (Join-Path $t4 'masters')) { Get-ChildItem -LiteralPath (Join-Path $t4 'masters') -Name | Select-Object -First 10 }
}
