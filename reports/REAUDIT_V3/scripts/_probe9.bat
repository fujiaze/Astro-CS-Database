@echo off
echo === A out dir NOW ===
dir /b "F:\Astro dev\Astro CS Normalization Database\run\temp\p2_v7\gc\audit_stage2_32f_A.mosaic.hips" 2>&1
echo === tasklist stage2 ===
tasklist /fi "imagename eq astrocs-stage2.exe" /fo csv /nh 2>&1
echo === python checks ===
where python 2>&1
where py 2>&1
python -c "print('py_ok')" 2>&1
echo PROBE_END
