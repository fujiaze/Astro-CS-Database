@echo off
echo === C signal structure ===
dir /b "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_v7\gc\audit_stage2_32f.mosaic.hips\signal" 2>&1
echo === C signal Norder0 ===
dir /b "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_v7\gc\audit_stage2_32f.mosaic.hips\signal\Norder0" 2>&1
echo === A status ===
dir /b "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_v7\gc\audit_stage2_32f_A.mosaic.hips" 2>&1
tasklist /fi "imagename eq astrocs-stage2.exe" /fo csv /nh 2>&1
echo PROBE_END
