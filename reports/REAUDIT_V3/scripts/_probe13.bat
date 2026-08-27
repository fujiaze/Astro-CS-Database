@echo off
echo === now ===
powershell -NoProfile -c "Write-Output (Get-Date -Format 'HH:mm:ss')" 2>&1
echo === A proc ===
powershell -NoProfile -c "Get-Process astrocs-stage2 -ErrorAction SilentlyContinue | ForEach-Object { Write-Output ('CPU=' + $_.CPU + ' start=' + $_.StartTime.ToString('HH:mm:ss') + ' WS=' + $_.WS) }" 2>&1
echo === A out dir ===
dir /b "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_v7\gc\audit_stage2_32f_A.mosaic.hips" 2>&1
echo === A mosaic temp files any ===
powershell -NoProfile -c "(Get-ChildItem 'F:/Astro dev/Astro CS Normalization Database/run/temp/p2_v7/gc/audit_stage2_32f_A.mosaic.hips' -Recurse -File | Measure-Object).Count" 2>&1
echo PROBE_END
