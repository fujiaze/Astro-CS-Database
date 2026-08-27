$ErrorActionPreference = 'Stop'
$base = 'F:\Astro dev\Astro CS Normalization Database'
$testbase = Join-Path $base 'testdata'
$outDir = Join-Path $env:TEMP 't4_audit'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$list = Join-Path $outDir 't4_pack_list.txt'
$tar = Join-Path $outDir 't4_red_plus_masters.tar'
$lines = New-Object System.Collections.Generic.List[string]
foreach ($panel in @('panel1','panel2','panel3')) {
  $dir = Join-Path (Join-Path $testbase 'Galaxy_Center_T4\lights') $panel
  $red = Get-ChildItem -LiteralPath $dir -File | Where-Object { $_.Name -like '*-Red.fts' } | Sort-Object Name
  foreach ($f in $red) {
    $rel = 'testdata/Galaxy_Center_T4/lights/' + $panel + '/' + $f.Name
    $lines.Add($rel)
  }
}
$calib = Join-Path $testbase 'T4 calibration files'
$masters = @('masterBias_BIN-1_4500x3600.xisf','masterDark_BIN-1_4500x3600_EXPOSURE-180.00s.xisf','masterFlat_BIN-1_4500x3600_FILTER-Red_mono.xisf')
foreach ($m in $masters) {
  if (-not (Test-Path -LiteralPath (Join-Path $calib $m))) { throw ('missing master: ' + $m) }
  $lines.Add(('testdata/T4 calibration files/' + $m))
}
[System.IO.File]::WriteAllLines($list, $lines)
if (Test-Path -LiteralPath $tar) { Remove-Item -LiteralPath $tar }
$tarExe = Join-Path $env:SystemRoot 'System32\tar.exe'
& $tarExe '-C' $base '-cf' $tar '-T' $list
if ($LASTEXITCODE -ne 0) { throw ('tar failed exit ' + $LASTEXITCODE) }
[Console]::Out.WriteLine('LIST_PATH=' + $list)
[Console]::Out.WriteLine('LIST_COUNT=' + $lines.Count)
[Console]::Out.WriteLine('TAR_PATH=' + $tar)
[Console]::Out.WriteLine('TAR_SIZE=' + (Get-Item -LiteralPath $tar).Length)
