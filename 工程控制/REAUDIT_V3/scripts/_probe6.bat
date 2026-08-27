@echo off
echo === A out dir NOW ===
dir /b "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_v7\gc\audit_stage2_32f_A.mosaic.hips" 2>&1
echo === tasklist ===
tasklist /fi "imagename eq astrocs-stage2.exe" /fo csv /nh 2>&1
echo PROBE_END
