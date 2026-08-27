$ErrorActionPreference = 'Continue'
$p = 'F:/Astro dev/Astro CS Normalization Database/run/temp/p2_v7/gc/audit_stage2_f01f11.log'
if (Test-Path -LiteralPath $p) {
  $c = Get-Content -LiteralPath $p -Encoding Default
  Write-Output ('LINES=' + $c.Count)
  $c | Select-Object -Last 14
} else { Write-Output 'no stage2 log yet' }
