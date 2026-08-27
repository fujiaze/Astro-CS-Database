@echo off
echo === newest logs ===
powershell -NoProfile -c "Get-ChildItem C:/Users/fujia -Filter *.log | Sort-Object LastWriteTime -Descending | Select-Object -First 15 | ForEach-Object { Write-Output ($_.Name + ' | ' + $_.LastWriteTime.ToString('MM-dd HH:mm:ss') + ' | ' + $_.Length) }" 2>&1
echo === A CPU ===
powershell -NoProfile -c "Get-Process astrocs-stage2 -ErrorAction SilentlyContinue | ForEach-Object { Write-Output ('CPU=' + $_.CPU + ' start=' + $_.StartTime.ToString('HH:mm:ss')) }" 2>&1
echo === gc recent dirs ===
powershell -NoProfile -c "Get-ChildItem 'F:/Astro dev/Astro CS Normalization Database/run/temp/p2_v7/gc' -Directory | Where-Object LastWriteTime -gt (Get-Date).AddMinutes(-100) | ForEach-Object { Write-Output ($_.Name + ' | ' + $_.LastWriteTime.ToString('HH:mm:ss')) }" 2>&1
echo PROBE_END
