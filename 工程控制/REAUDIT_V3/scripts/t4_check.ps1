$ErrorActionPreference = 'Continue'
Write-Host "=== DIRECT PATH CHECKS ==="
$paths = @(
  'F:\Astro dev\Astro CS Normalization Database\testdata\Galaxy_Center_T4',
  'F:\Astro dev\Astro CS Normalization Database',
  'F:\Astro dev'
)
foreach ($p in $paths) {
  Write-Host ("CHECK " + $p + " => " + (Test-Path -LiteralPath $p))
}
Write-Host "=== F ROOT LISTING ==="
Get-ChildItem -LiteralPath 'F:\' | ForEach-Object { Write-Host ("FITEM " + $_.Name) }
Write-Host "=== SEARCH Astro CS Normalization Database ==="
if (Test-Path -LiteralPath 'F:\Astro dev') {
  Get-ChildItem -LiteralPath 'F:\Astro dev' -Directory | ForEach-Object { Write-Host ("AD " + $_.Name) }
}
Write-Host "=== T4 LIGHTS COUNT (if present) ==="
$t4 = 'F:\Astro dev\Astro CS Normalization Database\testdata\Galaxy_Center_T4\lights'
if (Test-Path -LiteralPath $t4) {
  foreach ($p in @('panel1','panel2','panel3')) {
    $files = Get-ChildItem -LiteralPath (Join-Path $t4 $p) -File -ErrorAction SilentlyContinue
    Write-Host ("PANEL " + $p + " file_count=" + ($files | Measure-Object).Count)
    foreach ($f in $files) { Write-Host ("FILE " + $f.Name + " " + $f.Length) }
  }
} else {
  Write-Host "T4_LIGHTS_DIR_NOT_FOUND"
}
Write-Host "=== CALIB FILES ==="
$calib = 'F:\Astro dev\Astro CS Normalization Database\testdata\T4 calibration files'
if (Test-Path -LiteralPath $calib) {
  Get-ChildItem -LiteralPath $calib -File | ForEach-Object { Write-Host ("CALIB " + $_.Name + " " + $_.Length) }
} else {
  Write-Host "CALIB_DIR_NOT_FOUND"
}
