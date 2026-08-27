$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$out = Join-Path $root 'run/temp/p2_v7/gc/audit_s1b_out.log'
$err = Join-Path $root 'run/temp/p2_v7/gc/audit_s1b_err.log'
Start-Sleep -Seconds 50
Write-Output '=== stdout tail ===';
if (Test-Path -LiteralPath $out) { Get-Content -LiteralPath $out -Tail 15 } else { Write-Output 'no stdout' }
Write-Output '=== stderr tail ===';
if (Test-Path -LiteralPath $err) { Get-Content -LiteralPath $err -Tail 15 -Encoding Default } else { Write-Output 'no stderr' }
Write-Output '=== alive? ===';
$proc = Get-Process -Id 4164 -ErrorAction SilentlyContinue
if ($proc) { Write-Output ('ALIVE cpu=' + [math]::Round($proc.CPU,1) + 's mem=' + [math]::Round($proc.WorkingSet64/1MB,0) + 'MB') } else { Write-Output 'DEAD' }
