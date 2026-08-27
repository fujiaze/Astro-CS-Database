$ErrorActionPreference = 'Stop'
$root = 'F:/Astro dev/Astro CS Normalization Database'
foreach ($d in @('GaiaDR3','GaiaDR3SP','BASS DR3','run','launch','testdata')) {
  $p = Join-Path $root $d
  if (Test-Path -LiteralPath $p) {
    $n = (Get-ChildItem -LiteralPath $p -Recurse -File -ErrorAction SilentlyContinue | Measure-Object).Count
    $sz = (Get-ChildItem -LiteralPath $p -Recurse -File -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum
    Write-Output ($d + ': files=' + $n + ' sizeGB=' + [math]::Round($sz/1GB,2))
  } else { Write-Output ($d + ': MISSING') }
}
