@echo off
echo === A out dir NOW ===
dir /b "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_v7\gc\audit_stage2_32f_A.mosaic.hips" 2>&1
echo === tasklist stage2 ===
tasklist /fi "imagename eq astrocs-stage2.exe" /fo csv /nh 2>&1
echo === A CPU ===
powershell -NoProfile -c "Get-Process astrocs-stage2 -ErrorAction SilentlyContinue | Select-Object Id,StartTime,CPU | Format-List | Out-String" 2>&1
echo PROBE_END
