$ErrorActionPreference='Continue'
Write-Output '--- host ---'
hostname; whoami; Get-Date -Format s
Write-Output '--- stage2_32f.log (C run) ---'
if (Test-Path C:/Users/fujia/stage2_32f.log) { Get-Content C:/Users/fujia/stage2_32f.log -Tail 10 } else { Write-Output 'MISSING' }
Write-Output '--- gc audit_stage2_32f outputs ---'
if (Test-Path 'F:/Astro dev/Astro CS Normalization Database/run/temp/p2_v7/gc') {
  Get-ChildItem 'F:/Astro dev/Astro CS Normalization Database/run/temp/p2_v7/gc' -Filter 'audit_stage2_32f*' | ForEach-Object { ("" + $_.Name + " | " + $_.LastWriteTime.ToString('yyyy-MM-dd HH:mm:ss') + " | " + ($(if ($_.PSIsContainer) {'DIR'} else {$_.Length}))) }
} else { Write-Output 'NO_GC_DIR' }
Write-Output '--- fujia logs ---'
if (Test-Path C:/Users/fujia) { Get-ChildItem C:/Users/fujia -Filter '*.log' | Sort-Object LastWriteTime -Descending | Select-Object -First 12 | ForEach-Object { ("" + $_.Name + " | " + $_.LastWriteTime.ToString('MM-dd HH:mm:ss') + " | " + $_.Length) } }
Write-Output '--- worktrees ---'
if (Test-Path C:/Users/fujia/run/temp/p2_ab_worktrees) { Get-ChildItem C:/Users/fujia/run/temp/p2_ab_worktrees | ForEach-Object { ("" + $_.Name + " DIR=" + $_.LastWriteTime.ToString('MM-dd HH:mm:ss')) } } else { Write-Output 'NO_WORKTREE_DIR' }
