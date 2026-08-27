$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$t4 = Join-Path $root 'testdata/Galaxy_Center_T4'
Write-Output '=== dirs under T4 ===';
Get-ChildItem -LiteralPath $t4 -Directory -Recurse -ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName
Write-Output '=== calibration files dir (T4) ===';
$c4 = Join-Path $root 'testdata/T4 calibration files'
if (Test-Path -LiteralPath $c4) { Get-ChildItem -LiteralPath $c4 -Recurse -File -ErrorAction SilentlyContinue | Select-Object -First 20 -ExpandProperty FullName }
Write-Output '=== does any T4 light have PHOTSCAL? check first light header ===';
$lf = Get-ChildItem -LiteralPath (Join-Path $t4 'lights') -Recurse -Filter *.fts -ErrorAction SilentlyContinue | Select-Object -First 1
if ($lf) {
  $bytes = [System.IO.File]::ReadAllBytes($lf.FullName)
  $head = [System.Text.Encoding]::ASCII.GetString($bytes[0..3500])
  $has_phot = $head -match 'PHOTSCAL|PHOTZP|PHOTFLAM'
  Write-Output ('first light: ' + $lf.Name + ' HAS_PHOT_KEYWORDS=' + $has_phot)
}
