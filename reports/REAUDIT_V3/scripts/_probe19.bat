@echo off
echo === A sig count ===
powershell -NoProfile -c "(Get-ChildItem 'F:/Astro dev/Astro CS Normalization Database/run/temp/p2_v7/gc/audit_stage2_32f_A.mosaic.hips/signal' -Recurse -Filter *.fits | Measure-Object).Count" 2>&1
tasklist /fi "imagename eq astrocs-stage2.exe" /fo csv /nh 2>&1
echo === py check ===
python -c "import sys,struct,math;print('py ok', sys.version[:6])" 2>&1
echo PROBE_END
