$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$out = Join-Path $root 'run/temp/p2_v7/gc/audit_s1_out.log'
$err = Join-Path $root 'run/temp/p2_v7/gc/audit_s1_err.log'
Start-Sleep -Seconds 40
Write-Output '=== stdout tail ===';
if (Test-Path -LiteralPath $out) { Get-Content -LiteralPath $out -Tail 12 } else { Write-Output 'no stdout' }
Write-Output '=== stderr tail ===';
if (Test-Path -LiteralPath $err) { Get-Content -LiteralPath $err -Tail 12 -Encoding Default } else { Write-Output 'no stderr' }
Write-Output '=== alive? ===';
$proc = Get-Process -Id 22000 -ErrorAction SilentlyContinue
if ($proc) { Write-Output ('ALIVE cpu=' + [math]::Round($proc.CPU,1) + 's mem=' + [math]::Round($proc.WorkingSet64/1MB,0) + 'MB') } else { Write-Output 'DEAD' }
