$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$log = Join-Path $root 'run/temp/p2_v7/gc/audit_stage1_panel1_Red.out.log'
$err = Join-Path $root 'run/temp/p2_v7/gc/audit_stage1_panel1_Red.err.log'
Start-Sleep -Seconds 20
Write-Output '=== stdout tail ===';
if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log -Tail 15 } else { Write-Output 'no stdout log yet' }
Write-Output '=== stderr tail ===';
if (Test-Path -LiteralPath $err) { Get-Content -LiteralPath $err -Tail 10 } else { Write-Output 'no stderr log yet' }
Write-Output '=== process alive? ===';
$proc = Get-Process -Id 8016 -ErrorAction SilentlyContinue
if ($proc) { Write-Output ('ALIVE cpu=' + $proc.CPU) } else { Write-Output 'DEAD' }
