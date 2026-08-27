$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$runlog = Join-Path $root 'run/temp/p2_v7/gc/audit_s1_full.log'
Get-Content -LiteralPath $runlog -Encoding Default | Select-String -Pattern '完成 \(成功\)|PROCESS_EXIT|photometry|photscal|drizzle_engine|hips_verify|PHOTOMETRIC|platesolve|PLATESOLVE|calibrate|CALIBRATE|stage1' | Select-Object -Last 40 | ForEach-Object { $_.Line }
