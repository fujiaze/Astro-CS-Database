$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$log = Join-Path $root 'run/temp/p2_v7/gc/audit_stage1_panel1_Red.out.log'
$err = Join-Path $root 'run/temp/p2_v7/gc/audit_stage1_panel1_Red.err.log'
Start-Sleep -Seconds 45
Write-Output '=== stdout tail ===';
if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log -Tail 20 } else { Write-Output 'no stdout log' }
Write-Output '=== stderr tail ===';
if (Test-Path -LiteralPath $err) { Get-Content -LiteralPath $err -Tail 10 } else { Write-Output 'no stderr log' }
Write-Output '=== alive? ===';
$proc = Get-Process -Id 23208 -ErrorAction SilentlyContinue
if ($proc) { Write-Output ('ALIVE cpu=' + [math]::Round($proc.CPU,1) + 's mem=' + [math]::Round($proc.WorkingSet64/1MB,0) + 'MB') } else { Write-Output 'DEAD' }
