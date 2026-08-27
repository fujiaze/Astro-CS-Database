@echo off
echo === A output dir NOW ===
dir /b "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_v7\gc\audit_stage2_32f_A.mosaic.hips" 2>&1
echo === A diagnostics exists ===
dir "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_v7\gc\audit_stage2_32f_A.mosaic.hips\diagnostics.json" 2>&1 | findstr /i json
echo === stage2 running? ===
tasklist /fi "imagename eq astrocs-stage2.exe" /fo csv /nh 2>&1
echo === C mosaic full ===
dir /b "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_v7\gc\audit_stage2_32f.mosaic.hips" 2>&1
echo === C A-mosaic sizes quick ===
powershell -NoProfile -c "(Get-ChildItem 'F:/Astro dev/Astro CS Normalization Database/run/temp/p2_v7/gc/audit_stage2_32f.mosaic.hips' -Recurse -File | Measure-Object Length -Sum).Sum" 2>&1
echo PROBE_END
