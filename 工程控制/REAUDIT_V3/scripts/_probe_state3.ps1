$ErrorActionPreference='Continue'
Write-Output '--- A output dir contents ---'
$a = 'F:/Astro dev/Astro CS Normalization Database/run/temp/p2_v7/gc/audit_stage2_32f_A.mosaic.hips'
if (Test-Path $a) {
  Get-ChildItem $a | ForEach-Object { Write-Output ($_.Name + ' | ' + $(if ($_.PSIsContainer) {'DIR'} else {$_.Length}) + ' | ' + $_.LastWriteTime.ToString('MM-dd HH:mm:ss')) }
  if (Test-Path ($a + '/diagnostics.json')) { Write-Output '--- A diagnostics ---'; Get-Content ($a + '/diagnostics.json') -Raw | Select-Object -First 1 }
} else { Write-Output 'A_OUT_MISSING' }
Write-Output '--- F: worktrees ---'
$wt = 'F:/Astro dev/Astro CS Normalization Database/run/temp/p2_ab_worktrees'
if (Test-Path $wt) { Get-ChildItem $wt | ForEach-Object { Write-Output ($_.Name + ' | ' + $_.LastWriteTime.ToString('MM-dd HH:mm:ss')) } } else { Write-Output 'NO_WT_F' }
Write-Output '--- A/B build exes (F:) ---'
foreach ($n in @('A','B')) { foreach ($p in @('build_ab','build_mingw','build')) { $e = $wt + '/' + $n + '/' + $p + '/astrocs-stage2.exe'; if (Test-Path $e) { Write-Output ($n + '/' + $p + ' EXISTS ' + (Get-Item $e).LastWriteTime.ToString('MM-dd HH:mm:ss')) } } }
Write-Output '--- running stage2 ---'
Get-Process astrocs-stage2 -ErrorAction SilentlyContinue | ForEach-Object { Write-Output ('RUNNING pid=' + $_.Id + ' start=' + $_.StartTime.ToString('HH:mm:ss') + ' cpu=' + $_.CPU) }
Write-Output 'PROBE_END'
