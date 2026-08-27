$ErrorActionPreference = 'Stop'
$base = 'F:\Astro dev\Astro CS Normalization Database\testdata'
$lights = Join-Path $base 'Galaxy_Center_T4\lights'
$outDir = Join-Path $env:TEMP 't4_audit'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$csv = Join-Path $outDir 't4_manifest_remote.csv'
$lines = New-Object System.Collections.Generic.List[string]
$lines.Add('dataset,panel,filter,index,filename,full_path,size_bytes,sha256')
foreach ($panel in @('panel1','panel2','panel3')) {
  $dir = Join-Path $lights $panel
  $red = Get-ChildItem -LiteralPath $dir -File | Where-Object { $_.Name -like '*-Red.fts' } | Sort-Object Name
  $i = 0
  foreach ($f in $red) {
    $i++
    $h = Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256
    $lines.Add(('Galaxy_Center_T4,' + $panel + ',Red,' + $i + ',' + $f.Name + ',' + $f.FullName + ',' + $f.Length + ',' + $h.Hash))
  }
}
$masters = @{
  'master_bias'  = Join-Path $base 'T4 calibration files\masterBias_BIN-1_4500x3600.xisf'
  'master_dark'  = Join-Path $base 'T4 calibration files\masterDark_BIN-1_4500x3600_EXPOSURE-180.00s.xisf'
  'master_flat'  = Join-Path $base 'T4 calibration files\masterFlat_BIN-1_4500x3600_FILTER-Red_mono.xisf'
}
foreach ($k in @('master_bias','master_dark','master_flat')) {
  $p = $masters[$k]
  if (Test-Path -LiteralPath $p) {
    $f = Get-Item -LiteralPath $p
    $h = Get-FileHash -LiteralPath $p -Algorithm SHA256
    $lines.Add(('Galaxy_Center_T4,masters,' + $k + ',0,' + $f.Name + ',' + $f.FullName + ',' + $f.Length + ',' + $h.Hash))
  } else {
    $lines.Add(('Galaxy_Center_T4,masters,' + $k + ',0,MISSING,' + $p + ',-1,'))
  }
}
[System.IO.File]::WriteAllLines($csv, $lines)
[Console]::Out.WriteLine('CSV_PATH=' + $csv)
[Console]::Out.WriteLine('ROW_COUNT=' + ($lines.Count - 1))
