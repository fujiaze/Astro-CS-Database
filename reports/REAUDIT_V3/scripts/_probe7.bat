@echo off
echo === A CPU now ===
powershell -NoProfile -c "Get-Process astrocs-stage2 | Select-Object Id,StartTime,CPU,WS | Format-List | Out-String" 2>&1
echo === gc dirs modified after 18:00 today ===
powershell -NoProfile -c "Get-ChildItem 'F:/Astro dev/Astro CS Normalization Database/run/temp/p2_v7/gc' -Directory | Where-Object LastWriteTime -gt (Get-Date).AddHours(-2) | ForEach-Object { Write-Output ($_.Name + ' | ' + $_.LastWriteTime.ToString('MM-dd HH:mm:ss')) }" 2>&1
echo PROBE_END
