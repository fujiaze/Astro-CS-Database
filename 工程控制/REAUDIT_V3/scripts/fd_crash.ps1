$ErrorActionPreference = 'Continue'
$root = 'F:/Astro dev/Astro CS Normalization Database'
$lj = Join-Path $root 'run/temp/p2_v7/gc/panel1_Red.log.jsonl'
Write-Output '=== log.jsonl exists? ===';
if (Test-Path -LiteralPath $lj) { Get-Content -LiteralPath $lj -Tail 15 } else { Write-Output 'no log.jsonl' }
Write-Output '=== recent Application event for orchestrator crash ===';
Get-WinEvent -FilterHashtable @{LogName='Application'; Id=1000; StartTime=(Get-Date).AddMinutes(-10)} -ErrorAction SilentlyContinue | Select-Object -First 2 | ForEach-Object { $_.Message.Substring(0, [Math]::Min(500, $_.Message.Length)) }
