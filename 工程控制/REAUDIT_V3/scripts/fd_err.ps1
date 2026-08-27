$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$err = Join-Path $root 'run/temp/p2_v7/gc/audit_stage1_panel1_Red.err.log'
$out = Join-Path $root 'run/temp/p2_v7/gc/audit_stage1_panel1_Red.out.log'
Write-Output '=== full stderr (count=' + (Get-Content -LiteralPath $err).Count + ') ===';
Get-Content -LiteralPath $err -Encoding Default
Write-Output '=== stdout count=' + (Get-Content -LiteralPath $out -ErrorAction SilentlyContinue).Count + ' ===';
