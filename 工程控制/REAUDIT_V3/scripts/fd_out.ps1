$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$out = Join-Path $root 'run/temp/p2_v7/gc/panel1_Red.hips'
Write-Output ('OUTPUT_EXISTS=' + (Test-Path -LiteralPath $out))
if (Test-Path -LiteralPath $out) {
  Get-ChildItem -LiteralPath $out -Recurse -File | Select-Object -First 15 -ExpandProperty FullName
  Write-Output ('FILES=' + (Get-ChildItem -LiteralPath $out -Recurse -File).Count)
  if (Test-Path -LiteralPath (Join-Path $out 'manifest.json')) { Get-Content -LiteralPath (Join-Path $out 'manifest.json') }
  Write-Output '=== signal tile header: PHOTSCAL present? ===';
  $tile = Get-ChildItem -LiteralPath (Join-Path $out 'signal') -Recurse -Filter *.fits -ErrorAction SilentlyContinue | Select-Object -First 1
  if ($tile) {
    $b = [System.IO.File]::ReadAllBytes($tile.FullName)
    $h = [System.Text.Encoding]::ASCII.GetString($b[0..4000])
    $m = [regex]::Match($h, 'PHOTSCAL\s*=\s*([^/]+)')
    $m2 = [regex]::Match($h, 'PHOTZP\s*=\s*([^/]+)')
    Write-Output ('  ' + $tile.Name)
    Write-Output ('  PHOTSCAL=' + $m.Groups[1].Value.Trim())
    Write-Output ('  PHOTZP=' + $m2.Groups[1].Value.Trim())
  }
}
Write-Output '=== diagnostics dir ===';
$diag = Join-Path $root 'run/temp/p2_v7/gc/panel1_Red_diag'
if (Test-Path -LiteralPath $diag) { Get-ChildItem -LiteralPath $diag -Name | Select-Object -First 10 }
