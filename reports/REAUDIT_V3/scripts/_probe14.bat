@echo off
echo === A finalize ===
dir /b "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_v7\gc\audit_stage2_32f_A.mosaic.hips" 2>&1
tasklist /fi "imagename eq astrocs-stage2.exe" /fo csv /nh 2>&1
echo === C signal tile list (orders with counts) ===
powershell -NoProfile -c "$d='F:/Astro dev/Astro CS Normalization Database/run/temp/p2_v7/gc/audit_stage2_32f.mosaic.hips/signal'; Get-ChildItem $d -Recurse -Filter *.fits | Measure-Object | Select-Object -ExpandProperty Count; Get-ChildItem $d -Directory | ForEach-Object { $n=(Get-ChildItem $_.FullName -Recurse -Filter *.fits | Measure-Object).Count; Write-Output ($_.Name + ' ' + $n) }" 2>&1
echo PROBE_END
