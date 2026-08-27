Get-ChildItem 'F:/Astro dev/Astro CS Normalization Database/run/temp/p2_v7/gc' -Directory | Where-Object Name -like 'audit_stage2_32f*' | ForEach-Object { Write-Output ($_.Name + ' | ' + $_.LastWriteTime.ToString('yyyy-MM-dd HH:mm:ss')) }
Write-Output '--- ALL gc dirs count ---'
(Get-ChildItem 'F:/Astro dev/Astro CS Normalization Database/run/temp/p2_v7/gc' -Directory).Count
Write-Output '--- fujia logs ---'
Get-ChildItem C:/Users/fujia -Filter '*.log' | Sort-Object LastWriteTime -Descending | Select-Object -First 12 | ForEach-Object { Write-Output ($_.Name + ' | ' + $_.LastWriteTime.ToString('MM-dd HH:mm:ss') + ' | ' + $_.Length) }
Write-Output '--- worktree A/B build dirs ---'
Get-ChildItem C:/Users/fujia/run/temp/p2_ab_worktrees -Directory | ForEach-Object { Write-Output ($_.Name + ' | ' + $_.LastWriteTime.ToString('MM-dd HH:mm:ss')) }
Write-Output '--- A/B exes ---'
foreach ($n in @('A','B')) { $e = 'C:/Users/fujia/run/temp/p2_ab_worktrees/' + $n + '/build_ab/astrocs-stage2.exe'; Write-Output ($n + ' ' + (Test-Path $e) + ' ' + $(if (Test-Path $e) { (Get-Item $e).LastWriteTime.ToString('MM-dd HH:mm:ss') } else { '-' })) }
Write-Output '--- A/B run logs ---'
Get-ChildItem C:/Users/fujia -Filter '*32f*' | ForEach-Object { Write-Output ($_.Name + ' | ' + $_.LastWriteTime.ToString('MM-dd HH:mm:ss') + ' | ' + $_.Length) }
