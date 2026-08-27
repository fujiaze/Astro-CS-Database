$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$runlog = Join-Path $root 'run/temp/p2_v7/gc/audit_s1_full.log'
if (Test-Path -LiteralPath $runlog) {
  $c = Get-Content -LiteralPath $runlog -Encoding Default
  Write-Output ('LINES=' + $c.Count)
  $c | Select-Object -Last 12
} else { Write-Output 'no run log yet' }
